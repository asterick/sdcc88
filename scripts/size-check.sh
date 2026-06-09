#!/usr/bin/env bash
#
# size-check.sh — code-size measurement harness for the s1c88 port (TODO #12-sizeharness).
#
# Compiles + assembles every scripts/corpus/*.c and reports each program's ROM/code
# size (the sum of every .rel area EXCEPT RAM: _DATA / _INITIALIZED / _DABS), per
# program and total, with a DELTA vs a committed baseline (scripts/corpus/sizes.baseline).
# Makes peephole/cost wins (TODO #12) and relaxation wins (#14) visible and monotone.
#
# Report-only — it always exits 0. Tuning experiments may regress transiently, so this
# never gates; bless a new baseline once a change is intentional and reviewed.
#
#   scripts/size-check.sh            # report current sizes + delta vs baseline
#   scripts/size-check.sh snapshot   # save current sizes AS the baseline
#   SIZE_NO_BUILD=1 scripts/size-check.sh   # reuse the last compiler build (skip overlay+make)
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"
SDAS="${SDCC}/bin/sdas88"
SDLD="${SDCC}/bin/sdldz80"
LIBDIR="${SDCC}/share/sdcc/lib/s1c88"
CORPUS="${REPO}/scripts/corpus"
BASELINE="${CORPUS}/sizes.baseline"
RELAX_BASELINE="${CORPUS}/relax.baseline"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
MODE="${1:-check}"

[ -x "$SDAS" ] || { echo "!! build sdas88 first (scripts/build-sdas.sh as88)" >&2; exit 2; }
[ -f "${SDCC}/config.status" ] || { echo ">> not configured — run ./build.sh first" >&2; exit 1; }

# --- overlay + build the compiler (same as corpus-check / dev.sh) ---
if [ -z "${SIZE_NO_BUILD:-}" ]; then
  echo ">> overlay + build sdcc"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    echo "!! BUILD FAILED:" >&2; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20 >&2; exit 1
  fi
fi

# code_bytes <src.c> -> ROM byte count (decimal), or "ERR"
code_bytes() {
  cc -E -P -x c "$1" > "${OUT}/pp.c" 2>/dev/null || { echo ERR; return; }
  "${SDCCBIN}" -ms1c88 --c1mode -o "${OUT}/a.asm" < "${OUT}/pp.c" 2>/dev/null || { echo ERR; return; }
  "${SDAS}" -o "${OUT}/a.rel" "${OUT}/a.asm" >/dev/null 2>&1 || { echo ERR; return; }
  local tot=0 name h
  # .rel area records: `A <name> size <hex> flags <hex> addr <hex>`; sum the ROM areas.
  while read -r name h; do
    case "$name" in _DATA|_INITIALIZED|_DABS) continue ;; esac
    tot=$((tot + 16#$h))
  done < <(awk '/^A /{ print $2, $4 }' "${OUT}/a.rel")
  echo "$tot"
}

# _rom <rel-files> <norelax(0|1)> -> linked ROM byte count, or ERR.  Links the
# program standalone against s1c88.lib in the common bank (no crt0/main — we only
# measure code size), with #14c relaxation on (default) or forced off.  A common-
# bank `_STUB` area absorbs the corpus's deliberately-external test symbols.
_rom() {
  local e=""; [ "$2" = 1 ] && e="SDLD_NO_RELAX=1"
  env $e "${SDLD}" -nwxi -b _CODE=0x2100 -b _STUB=0x7000 -b _DATA=0x1000 \
      "${OUT}/l.ihx" $1 -k "${LIBDIR}" -l s1c88 >/dev/null 2>&1 || { echo ERR; return; }
  awk '/^:/{ ll=strtonum("0x" substr($0,2,2)); t=substr($0,8,2);
            if (t=="00") s+=ll } END{ print s+0 }' "${OUT}/l.ihx"
}

# relax_reclaim <src.c> -> "<reclaimed-bytes>"  (#14c link-time relaxation win:
# linked-ROM delta between the default relaxed link and SDLD_NO_RELAX).  ERR on
# any failure.  Pre-link `.rel` size (code_bytes above) can't see this — the drop
# is a LINK step — so it is reported as its own column.  Corpus programs reference
# deliberately-external symbols (cross-area call targets, ISR handlers, float
# intrinsics); we synthesise a same-bank `_STUB` defining each so the program
# links AND the cross-module calls stay relaxable — the calls we want to measure.
relax_reclaim() {
  cc -E -P -x c "$1" > "${OUT}/pp.c" 2>/dev/null || { echo ERR; return; }
  "${SDCCBIN}" -ms1c88 --c1mode -o "${OUT}/a.asm" < "${OUT}/pp.c" 2>/dev/null || { echo ERR; return; }
  "${SDAS}" -o "${OUT}/a.rel" "${OUT}/a.asm" >/dev/null 2>&1 || { echo ERR; return; }
  local undef
  undef="$(SDLD_NO_RELAX=1 "${SDLD}" -nwxi -b _CODE=0x2100 -b _DATA=0x1000 \
            "${OUT}/probe.ihx" "${OUT}/a.rel" -k "${LIBDIR}" -l s1c88 2>&1 \
          | sed -nE "s/.*Undefined Global '([^']+)'.*/\1/p" | sort -u)"
  # Always emit a (possibly empty) _STUB area so `-b _STUB` always binds.
  { echo "	.area _STUB"; for s in $undef; do printf '\t.globl %s\n%s:\t.ds 2\n' "$s" "$s"; done; } > "${OUT}/stub.s"
  "${SDAS}" -o "${OUT}/stub.rel" "${OUT}/stub.s" >/dev/null 2>&1 || { echo ERR; return; }
  local rels="${OUT}/a.rel ${OUT}/stub.rel"
  local on off; on="$(_rom "$rels" 0)"; off="$(_rom "$rels" 1)"
  { [ "$on" = ERR ] || [ "$off" = ERR ]; } && { echo ERR; return; }
  echo "$((off - on))"
}

declare -A cur rec; total=0; rtotal=0
for f in "${CORPUS}"/*.c; do
  b="$(basename "$f" .c)"
  n="$(code_bytes "$f")"
  cur[$b]="$n"
  [ "$n" != ERR ] && total=$((total + n))
  r="$(relax_reclaim "$f")"
  rec[$b]="$r"
  [ "$r" != ERR ] && rtotal=$((rtotal + r))
done

if [ "$MODE" = "snapshot" ]; then
  : > "$BASELINE"; : > "$RELAX_BASELINE"
  for b in $(printf '%s\n' "${!cur[@]}" | sort); do
    echo "$b ${cur[$b]}"  >> "$BASELINE"
    echo "$b ${rec[$b]}"  >> "$RELAX_BASELINE"
  done
  echo "TOTAL ${total}"  >> "$BASELINE"
  echo "TOTAL ${rtotal}" >> "$RELAX_BASELINE"
  echo ">> size baseline   saved to ${BASELINE} (total ${total} ROM bytes, $(ls "${CORPUS}"/*.c | wc -l) programs)"
  echo ">> relax baseline  saved to ${RELAX_BASELINE} (total ${rtotal} bytes reclaimed by #14c)"
  exit 0
fi

declare -A base rbase; btotal=""; rbtotal=""
if [ -f "$BASELINE" ]; then
  while read -r k v; do [ "$k" = TOTAL ] && btotal="$v" || base[$k]="$v"; done < "$BASELINE"
fi
if [ -f "$RELAX_BASELINE" ]; then
  while read -r k v; do [ "$k" = TOTAL ] && rbtotal="$v" || rbase[$k]="$v"; done < "$RELAX_BASELINE"
fi

fmt_delta() { local d="$1"; if [ "$d" -gt 0 ]; then echo "+$d"; else echo "$d"; fi; }

# --- codegen size (pre-link .rel ROM areas; the #12 peephole/cost yardstick) ---
printf "%-18s %8s %9s\n" "program" "bytes" "delta"
printf -- "-------------------------------------------\n"
for b in $(printf '%s\n' "${!cur[@]}" | sort); do
  n="${cur[$b]}"; ob="${base[$b]:-}"
  if [ -n "$ob" ] && [ "$n" != ERR ]; then ds="$(fmt_delta $((n - ob)))"; else ds="-"; fi
  printf "%-18s %8s %9s\n" "$b" "$n" "$ds"
done
printf -- "-------------------------------------------\n"
[ -n "$btotal" ] && tds="$(fmt_delta $((total - btotal)))" || tds="-"
printf "%-18s %8s %9s\n" "TOTAL" "$total" "$tds"
[ -z "$btotal" ] && echo ">> no baseline yet — run: scripts/size-check.sh snapshot"

# --- #14c link-time relaxation reclaim (linked-ROM bytes dropped vs SDLD_NO_RELAX) ---
echo
printf "%-18s %8s %9s\n" "#14c relax" "reclaim" "delta"
printf -- "-------------------------------------------\n"
for b in $(printf '%s\n' "${!rec[@]}" | sort); do
  r="${rec[$b]}"; orb="${rbase[$b]:-}"
  if [ -n "$orb" ] && [ "$r" != ERR ]; then ds="$(fmt_delta $((r - orb)))"; else ds="-"; fi
  printf "%-18s %8s %9s\n" "$b" "$r" "$ds"
done
printf -- "-------------------------------------------\n"
[ -n "$rbtotal" ] && rtds="$(fmt_delta $((rtotal - rbtotal)))" || rtds="-"
printf "%-18s %8s %9s\n" "TOTAL" "$rtotal" "$rtds"
exit 0
