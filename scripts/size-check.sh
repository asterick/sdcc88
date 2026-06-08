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
CORPUS="${REPO}/scripts/corpus"
BASELINE="${CORPUS}/sizes.baseline"
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

declare -A cur; total=0
for f in "${CORPUS}"/*.c; do
  b="$(basename "$f" .c)"
  n="$(code_bytes "$f")"
  cur[$b]="$n"
  [ "$n" != ERR ] && total=$((total + n))
done

if [ "$MODE" = "snapshot" ]; then
  : > "$BASELINE"
  for b in $(printf '%s\n' "${!cur[@]}" | sort); do echo "$b ${cur[$b]}" >> "$BASELINE"; done
  echo "TOTAL ${total}" >> "$BASELINE"
  echo ">> size baseline saved to ${BASELINE} (total ${total} ROM bytes, $(ls "${CORPUS}"/*.c | wc -l) programs)"
  exit 0
fi

declare -A base; btotal=""
if [ -f "$BASELINE" ]; then
  while read -r k v; do [ "$k" = TOTAL ] && btotal="$v" || base[$k]="$v"; done < "$BASELINE"
fi

fmt_delta() { local d="$1"; if [ "$d" -gt 0 ]; then echo "+$d"; else echo "$d"; fi; }

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
exit 0
