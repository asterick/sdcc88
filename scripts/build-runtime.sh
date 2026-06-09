#!/usr/bin/env bash
#
# build-runtime.sh — build + install the s1c88 target runtime into the SDCC lib dir
# so the driver's integrated link (`sdcc -ms1c88 foo.c`) finds crt0 and the support lib.
#
# The driver searches <datadir>/lib/s1c88/ for crt0.rel (port->linker.crt) and the
# 's1c88' library (port->linker.libs). This script populates that directory:
#   - crt0.rel        the production startup (device/lib/s1c88/crt0.s)
#   - s1c88.lib       the support library: a classic ASxxxx text-index (one module
#                     name per line) + the matching <module>.rel objects alongside.
#                     sdldz80 reads this format directly (no sdar/binutils needed).
#
# The library members are SDCC's generic C support routines, compiled THROUGH OUR
# OWN PORT (self-hosting; same path emu-test uses). We include exactly the routines
# the codegen emits implicit bcalls to (16- and 32-bit div/mod/mul — char ops run on
# the native DIV/MLT and shifts/widening are inline, so they need no library), plus a
# small libc subset (mem/str) for user code. Float/longlong are out of scope here
# (TODO #8/#18 — unverified surface).
#
#   scripts/build-runtime.sh
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"
SDAS="${SDCC}/bin/sdas88"
DLIB="${SDCC}/device/lib"
DINC="${SDCC}/device/include"
LIBDIR="${SDCC}/share/sdcc/lib/s1c88"
INCDIR="${SDCC}/share/sdcc/include/s1c88"

[ -x "$SDAS" ]    || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }
[ -x "$SDCCBIN" ] || { echo "!! sdcc not built — run ./build.sh" >&2; exit 2; }
mkdir -p "$LIBDIR" "$INCDIR"

# --- device headers: install where the driver's -isystem .../include/s1c88 looks ---
echo ">> installing device headers -> ${INCDIR}/"
cp "${REPO}/device/include/s1c88/"*.h "$INCDIR/"

echo ">> assembling crt0 -> ${LIBDIR}/crt0.rel"
"$SDAS" -o "${LIBDIR}/crt0.rel" "${REPO}/device/lib/s1c88/crt0.s" \
  || { echo "!! crt0 assemble FAILED" >&2; exit 1; }

# --- support library members ---
# Compiler-emitted helpers (mandatory: link fails without them):
LIB_INT="_divsint _divuint _modsint _moduint _mulint"
LIB_LONG="_divslong _divulong _modslong _modulong _mullong"
# int*int->long widening multiplies (has_mulint2long, main.c) — repo-owned sources:
LIB_WIDEMUL="_muluint2ulong _mulsint2slong"
# libc subset for user code (the codegen inlines mem*/str* as builtins, but user
# code may call them as real functions):
LIB_LIBC="_memmove _memcmp _memchr _strcpy _strncpy _strcat _strncat _strcmp _strncmp _strchr _strrchr _strlen _strstr"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
echo ">> building s1c88.lib members (compiled through the s1c88 port)"
members=""
for r in $LIB_INT $LIB_LONG $LIB_WIDEMUL $LIB_LIBC; do
  # Prefer a repo-owned source (e.g. _strlen.c, which upstream SDCC 4.5.0 lacks)
  # over the fetched SDCC device/lib copy.
  src="${REPO}/device/lib/s1c88/${r}.c"
  [ -f "$src" ] || src="${DLIB}/${r}.c"
  [ -f "$src" ] || { echo "   skip ${r} (no ${r}.c)"; continue; }
  if ! cc -E -P -x c -I "$DINC" -D_SDCC_NO_ASM_LIB_FUNCS "$src" > "${TMP}/${r}.i" 2>"${TMP}/e" \
     || ! "$SDCCBIN" -ms1c88 --c1mode -o "${TMP}/${r}.asm" < "${TMP}/${r}.i" 2>"${TMP}/e" \
     || ! "$SDAS" -o "${LIBDIR}/${r}.rel" "${TMP}/${r}.asm" 2>"${TMP}/e"; then
    echo "!! support routine ${r} FAILED:"; sed 's/^/    /' "${TMP}/e" | head -8; exit 1
  fi
  members="${members}${r}"$'\n'
done

# --- default interrupt vectors --------------------------------------------
# crt0's header trampolines bjump to _irq_v1.._irq_v26.  Provide a do-nothing
# (RETE) default for each as a SEPARATE library module, so a program links even
# if it defines none, and overriding one vector (define `void irq_vN(void)
# __interrupt`) pulls only the remaining defaults.  Each unresolved vector
# decomposes to its OWN standalone RETE (no shared default handler).
# (Future: __interrupt(N) auto-wiring so the user doesn't number vectors by hand.)
echo ">> building default interrupt vectors (irq_v1..irq_v26)"
for n in $(seq 1 26); do
  printf '\t.module irq_v%s\n\t.globl _irq_v%s\n\t.area _CODE\n_irq_v%s::\n\trete\n' "$n" "$n" "$n" > "${TMP}/irq_v${n}.s"
  "$SDAS" -o "${LIBDIR}/irq_v${n}.rel" "${TMP}/irq_v${n}.s" 2>"${TMP}/e" \
    || { echo "!! irq_v${n} FAILED:"; sed 's/^/    /' "${TMP}/e" | head; exit 1; }
  members="${members}irq_v${n}"$'\n'
done

# classic ASxxxx text-index library: one module name per line (sdld appends .rel and
# searches the lib dir). Only modules that resolve an undefined symbol get pulled in.
printf '%s' "$members" > "${LIBDIR}/s1c88.lib"

echo ">> installed: ${LIBDIR}/{crt0.rel, s1c88.lib + $(echo "$members" | grep -c .) modules}"
