#!/usr/bin/env bash
#
# corpus-check.sh — byte-identical codegen regression harness for the s1c88 port.
#
# Overlays the current src/s1c88, rebuilds sdcc, compiles every scripts/corpus/*.c with
# `-ms1c88 --c1mode`, normalizes the filename-derived `.module` line + the `;(null)` debug
# comments, and:
#   - `snapshot`  saves the normalized asm as the baseline (scripts/corpus/.baseline/).
#   - `check`     (default) diffs current output vs the baseline and, for refactors that MUST be
#                 behavior-neutral, reports BYTE-IDENTICAL / DIFF per file. Also runs sdas88 so a
#                 change that stays byte-identical is still proven to assemble cleanly.
#
# Workflow for the register-model grind: `snapshot` once on a known-green tree, make an edit,
# then `check` — a clean refactor is byte-identical everywhere AND 0 sdas88 errors.
#
#   scripts/corpus-check.sh snapshot   # save baseline from current sources
#   scripts/corpus-check.sh            # rebuild + diff vs baseline + validate
#   scripts/corpus-check.sh check
#   TAP=1 scripts/corpus-check.sh      # emit TAP body lines (for scripts/run-tests.sh)
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"
CORPUS="${REPO}/scripts/corpus"
BASE="${CORPUS}/.baseline"
OUT="$(mktemp -d)"
MODE="${1:-check}"
TAP="${TAP:-}"
note() { if [ -n "$TAP" ]; then echo "$@" >&2; else echo "$@"; fi; }

# --- overlay + build (same as dev.sh) ---
if [ ! -f "${SDCC}/config.status" ]; then echo ">> not configured — run ./build.sh first" >&2; exit 1; fi
if [ -n "${EMU_NO_BUILD:-}" ]; then
  note ">> reuse existing build (EMU_NO_BUILD)"
else
  note ">> overlay + build"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    echo "!! BUILD FAILED:" >&2; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20 >&2; exit 1
  fi
  note ">> build OK"
fi

# Normalize: drop the filename-derived `.module` line, the `;(null)` c1mode debug
# comments, and the SDCC version banner — it embeds the build host OS
# ("(Linux)" / "(Mac OS X)"), which would make every baseline host-specific.
# Strip \r too: on Windows the mingw CRT writes the .asm in text mode (CRLF) —
# line-ending noise, not codegen.
normalize() { grep -vE '^\s*\.module|^;\(null\)|^; Version 4\.5\.0' "$1" | tr -d '\r'; }

compile() { # $1 = src .c ; $2 = dest normalized asm
  # --c1mode has no preprocessor: strip comments/directives with the host cpp first (-P = no line markers).
  cc -std=gnu17 -E -P -x c "$1" > "${OUT}/pp.c" 2>/dev/null || { echo "!! cpp FAILED: $1"; return 1; }
  "${SDCCBIN}" -ms1c88 --c1mode -o "${OUT}/raw.asm" < "${OUT}/pp.c" 2>"${OUT}/err" || {
    echo "!! compile FAILED: $1"; cat "${OUT}/err"; return 1; }
  if grep -qiE 'Internal Error|backtrace|FATAL' "${OUT}/err"; then
    echo "!! compiler crash on $1:"; cat "${OUT}/err"; return 1; fi
  normalize "${OUT}/raw.asm" > "$2"
}

if [ "$MODE" = "snapshot" ]; then
  rm -rf "$BASE"; mkdir -p "$BASE"
  for f in "${CORPUS}"/*.c; do
    b="$(basename "$f" .c)"
    compile "$f" "${BASE}/${b}.asm" || exit 1
  done
  echo ">> baseline snapshot saved to ${BASE} ($(ls "${BASE}" | wc -l) files)"
  exit 0
fi

# --- check mode ---
[ -d "$BASE" ] || { echo "!! no baseline — run: scripts/corpus-check.sh snapshot" >&2; exit 1; }
identical=0; diffd=0; aserr=0; fails=0
for f in "${CORPUS}"/*.c; do
  b="$(basename "$f" .c)"
  cur="${OUT}/${b}.asm"
  if ! compile "$f" "$cur" > "${OUT}/cout" 2>&1; then
    fails=$((fails+1))
    if [ -n "$TAP" ]; then echo "not ok - corpus/$b # COMPILE-FAIL"; sed 's/^/#   /' "${OUT}/cout" | head -10
    else cat "${OUT}/cout"; fi
    continue
  fi
  # byte-identical vs baseline?
  if [ -f "${BASE}/${b}.asm" ] && diff -q "${BASE}/${b}.asm" "$cur" >/dev/null; then
    identical=$((identical+1)); tag="IDENTICAL"; isdiff=0
  else
    diffd=$((diffd+1)); tag="DIFF     "; isdiff=1
  fi
  # sdas88 validation (errors only)
  ne="$("${REPO}/scripts/validate-s1c88.sh" "$cur" 2>/dev/null | sed -nE 's/.*assembler errors  : ([0-9]+).*/\1/p')"
  ne="${ne:-?}"
  [ "$ne" != "0" ] && [ "$ne" != "?" ] && aserr=$((aserr+1))
  if [ -n "$TAP" ]; then
    # a corpus case is ok iff byte-identical to the baseline AND assembles 0 errors
    if [ "$isdiff" -eq 0 ] && [ "$ne" = "0" ]; then
      echo "ok - corpus/$b"
    else
      echo "not ok - corpus/$b # ${tag// /} sdas88_err=$ne"
      [ "$isdiff" -ne 0 ] && echo "#   codegen differs from baseline (diff ${BASE}/${b}.asm ${OUT}/${b}.asm)"
    fi
  else
    printf "  %-14s %s  sdas88_err=%s\n" "$b" "$tag" "$ne"
  fi
done
if [ -z "$TAP" ]; then
  echo "-------------------------------------------"
  echo ">> byte-identical: ${identical}  diff: ${diffd}  files-with-asm-errors: ${aserr}  compile-fails: ${fails}"
  if [ "$diffd" -eq 0 ] && [ "$aserr" -eq 0 ] && [ "$fails" -eq 0 ]; then
    echo ">> GREEN (byte-identical + assembles clean)"
  else
    echo ">> review the DIFF / error files above (baseline in ${BASE}, current in ${OUT})"
    echo "   diff e.g.:  diff ${BASE}/<name>.asm ${OUT}/<name>.asm"
  fi
fi
# exit nonzero if anything is off (so run-tests / CI sees the failure)
[ "$diffd" -eq 0 ] && [ "$aserr" -eq 0 ] && [ "$fails" -eq 0 ]
