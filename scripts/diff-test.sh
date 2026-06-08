#!/usr/bin/env bash
#
# diff-test.sh — DIFFERENTIAL codegen tests for the s1c88 port.
#
# Each tests/diff/cases/*.c defines diff_run(), which emits "tag:hex\n" lines for
# a battery of computations. The SAME source is compiled two ways and the output
# streams are compared:
#   reference : host cc (native)            -> golden stream
#   candidate : sdcc88 -> sdas88 -> sdld -> romgen -> minimon emulator
# Any divergence is a miscompile (or an unsound test — see tests/diff/harness.h).
#
#   scripts/diff-test.sh              # overlay+rebuild sdcc, then run all cases
#   scripts/diff-test.sh arith       # only cases matching 'arith'
#   EMU_NO_BUILD=1 scripts/diff-test.sh   # skip the compiler rebuild
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
[ -x "${SDCC}/bin/romgen" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
SDCCBIN="${SDCC}/src/sdcc"
SDAS="${SDCC}/bin/sdas88"
SDLD="${SDCC}/bin/sdldz80"
EMU="${REPO}/tests/emu"
DIFF="${REPO}/tests/diff"
RUNNER="${REPO}/build/emu/runner"
FILTER="${1:-}"

[ -x "$SDAS" ] && [ -x "$SDLD" ] || { echo "!! build sdas88 + sdldz80 first" >&2; exit 2; }

# --- overlay + rebuild the compiler (same as dev.sh / emu-test.sh) ---
if [ -z "${EMU_NO_BUILD:-}" ]; then
  [ -f "${SDCC}/config.status" ] || { echo ">> not configured — run ./build.sh first"; exit 2; }
  echo ">> overlay + build sdcc"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    echo "!! BUILD FAILED:"; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20; exit 1
  fi
fi
[ -x "$SDCCBIN" ] || { echo "!! no sdcc at ${SDCCBIN}" >&2; exit 2; }

echo ">> build emulator runner"
make -s -C "$EMU" || { echo "!! runner build FAILED"; exit 1; }

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

# --- crt0 + runtime support library, via OUR port (self-hosting). Built into a
#     text-index library (rt.lib) so each case pulls only the modules it references
#     (and their transitive deps) — float pulls ~25 routines, int cases pull none. ---
"$SDAS" -o "${OUT}/crt0.rel" "${EMU}/crt0.asm" || { echo "!! crt0 assemble FAILED"; exit 1; }
RT_INT="_mulint _mullong _divuint _divsint _moduint _modsint _divulong _divslong _modulong _modslong"
RT_FLOAT="_fsadd _fssub _fsmul _fsdiv _fseq _fslt _fscmp \
          _fsget1arg _fsget2args _fsnormalize _fsreturnval _fsrshift _fsswapargs \
          _fs2schar _fs2sint _fs2slong _fs2uchar _fs2uint _fs2ulong \
          _schar2fs _sint2fs _slong2fs _uchar2fs _uint2fs _ulong2fs"
: > "${OUT}/rt.lib"
for r in $RT_INT $RT_FLOAT; do
  if ! cc -E -P -x c -I "${SDCC}/device/include" -D_SDCC_NO_ASM_LIB_FUNCS \
        "${SDCC}/device/lib/${r}.c" > "${OUT}/${r}.i" 2>"${OUT}/err" \
     || ! "$SDCCBIN" -ms1c88 --c1mode -o "${OUT}/${r}.asm" < "${OUT}/${r}.i" 2>"${OUT}/err" \
     || ! "$SDAS" -o "${OUT}/${r}.rel" "${OUT}/${r}.asm" > "${OUT}/err" 2>&1; then
    echo "!! runtime ${r} build FAILED:"; sed 's/^/    /' "${OUT}/err" | head -10; exit 1
  fi
  echo "$r" >> "${OUT}/rt.lib"
done

pass=0; fail=0
for src in "${DIFF}"/cases/*.c; do
  b="$(basename "$src" .c)"
  case "$b" in *"$FILTER"*) ;; *) continue ;; esac
  printf "  %-14s " "$b"

  # one TU = the case + a main() that calls diff_run()
  printf '#include "%s"\nint main(void){diff_run();return 0;}\n' "$src" > "${OUT}/wrap.c"

  # --- reference: host build + run -> golden ---
  if ! cc -O2 -w -DDIFF_HOST -I "$DIFF" "${OUT}/wrap.c" -o "${OUT}/host" 2>"${OUT}/err"; then
    echo "HOST-COMPILE-FAIL"; sed 's/^/    /' "${OUT}/err" | head -10; fail=$((fail+1)); continue
  fi
  "${OUT}/host" > "${OUT}/golden" 2>/dev/null

  # --- candidate: sdcc88 -> sdas88 -> link -> romgen -> emulator ---
  if ! cc -E -P -I "$DIFF" "${OUT}/wrap.c" > "${OUT}/${b}.i" 2>"${OUT}/err"; then
    echo "CPP-FAIL"; sed 's/^/    /' "${OUT}/err"; fail=$((fail+1)); continue
  fi
  if ! "$SDCCBIN" -ms1c88 --c1mode -o "${OUT}/${b}.asm" < "${OUT}/${b}.i" 2>"${OUT}/err" \
     || grep -qiE 'Internal Error|backtrace|FATAL' "${OUT}/err"; then
    echo "COMPILE-FAIL"; sed 's/^/    /' "${OUT}/err" | head -10; fail=$((fail+1)); continue
  fi
  if ! "$SDAS" -o "${OUT}/${b}.rel" "${OUT}/${b}.asm" > "${OUT}/err" 2>&1; then
    echo "ASSEMBLE-FAIL"; sed 's/^/    /' "${OUT}/err" | head -10; fail=$((fail+1)); continue
  fi
  if ! "$SDLD" -nwxi -b _CODE=0x4000 -b _HOME=0x2100 -b _DATA=0x1000 \
        "${OUT}/${b}.ihx" "${OUT}/crt0.rel" "${OUT}/${b}.rel" -k "${OUT}" -l rt > "${OUT}/err" 2>&1 \
     || grep -q "Undefined Global" "${OUT}/err"; then
    echo "LINK-FAIL"; grep -E "Undefined Global|ASlink" "${OUT}/err" | sort -u | sed 's/^/    /'
    fail=$((fail+1)); continue
  fi
  if ! "${SDCC}/bin/romgen" "${OUT}/${b}.ihx" "${OUT}/${b}.min" > "${OUT}/err" 2>&1; then
    echo "ROMGEN-FAIL"; sed 's/^/    /' "${OUT}/err"; fail=$((fail+1)); continue
  fi
  "$RUNNER" "${OUT}/${b}.min" > "${OUT}/guest" 2>"${OUT}/rerr"; rc=$?

  # --- compare ---
  if [ "$rc" -ne 0 ]; then
    echo "RUN-FAIL (rc=$rc)"; sed 's/^/    /' "${OUT}/rerr" | head -5; fail=$((fail+1)); continue
  fi
  if cmp -s "${OUT}/golden" "${OUT}/guest"; then
    n="$(wc -l < "${OUT}/golden")"
    echo "PASS ($n values match)"; pass=$((pass+1))
  else
    d="$(diff "${OUT}/golden" "${OUT}/guest" | grep -cE '^[<>]')"
    echo "DIFF ($d lines: < host  > guest)"
    diff "${OUT}/golden" "${OUT}/guest" | grep -E '^[<>]' | head -20 | sed 's/^/    /'
    fail=$((fail+1))
  fi
done

echo "== diff-test: ${pass} passed, ${fail} failed =="
[ "$fail" -eq 0 ]
