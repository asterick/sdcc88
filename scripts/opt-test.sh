#!/usr/bin/env bash
#
# opt-test.sh — the OPTION-MATRIX gate: re-run the two execution-verified suites
# (emu = exit-code cases on the minimon core, diff = host-vs-emulator differential)
# under the sdcc option sets users actually pick. The default-options runs are
# covered by run-tests.sh on every platform; codegen is host-identical (the corpus
# proves it byte-for-byte), so this matrix runs on one host.
#
# Only the CASE code is compiled with the variant options — crt0 + s1c88.lib stay
# default-built, exactly like the shipped SDK a user links against. corpus-check is
# excluded by design: its baselines pin default-options output.
#
# --fomit-frame-pointer is NOT a mode: the port rejects it (main.c
# _finaliseOptions) — forcing omission onto functions the allocator's
# heuristic rejects reaches exstk paths that miscompile; the heuristic's own
# (default-on) omission is covered by every suite already. Lifting this means
# auditing the forced-exstk offset math in gen.c, then re-adding the mode.
#
# --no-peep is deliberately NOT a mode: the port's peephole carries the
# "instruction-legality fixups" section (peeph.def — `add hl,sp` etc. have no
# native S1C88 encoding and are rewritten there), so disabling it produces
# loud assembler errors by design, not a user configuration.
#
# Modes (label = TAP prefix):
#   size     --opt-code-size           the cost model's primary target
#   speed    --opt-code-speed          cycle-weighted allocator + codegen choices
#   alloc25  --max-allocs-per-node 25  a cheap allocator explores different
#                                      assignments (the swapped-HL bug class)
#   nomid    --nolospre --noinduction  middle-end optimizations off — different
#                                      iCode streams into the backend
#
#   scripts/opt-test.sh              # overlay+rebuild sdcc, run the full matrix
#   scripts/opt-test.sh size         # only modes matching 'size'
#   EMU_NO_BUILD=1 scripts/opt-test.sh   # skip the compiler rebuild
#   TAP=1 scripts/opt-test.sh        # emit one TAP stream (for CI)
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
FILTER="${1:-}"
TAP="${TAP:-}"

MODES=(
  "size|--opt-code-size"
  "speed|--opt-code-speed"
  "alloc25|--max-allocs-per-node 25"
  "nomid|--nolospre --noinduction"
)

note() { if [ -n "$TAP" ]; then echo "$@" >&2; else echo "$@"; fi; }

# build once (overlay + make, same as run-tests.sh); the per-mode suite runs
# then all use EMU_NO_BUILD=1
if [ -z "${EMU_NO_BUILD:-}" ]; then
  SDCC="${REPO}/build/sdcc-4.5.0"
  [ -f "${SDCC}/config.status" ] || { echo ">> not configured — run ./build.sh first" >&2; exit 2; }
  note ">> overlay + build sdcc (once)"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    echo "!! BUILD FAILED:" >&2; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20 >&2; exit 1
  fi
fi

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
total=0; failed=0; lines=0

for mode in "${MODES[@]}"; do
  label="${mode%%|*}"; flags="${mode#*|}"
  case "$label" in *"$FILTER"*) ;; *) continue ;; esac
  note ">> mode ${label}: ${flags}"
  for suite in emu diff; do
    body="${OUT}/${label}.${suite}"
    SDCC_OPTS="$flags" TAP=1 EMU_NO_BUILD=1 "${REPO}/scripts/${suite}-test.sh" \
      >"$body" 2>"${body}.log"
    # re-prefix the suite's TAP body under this mode
    while IFS= read -r ln; do
      case "$ln" in
        "ok - "*)     ln="ok - opt-${label}/${ln#ok - }" ;;
        "not ok - "*) ln="not ok - opt-${label}/${ln#not ok - }"; failed=$((failed+1)) ;;
        \#*) ;;
        *) continue ;;
      esac
      [ "${ln#not ok}" != "$ln" ] || [ "${ln#ok}" != "$ln" ] && total=$((total+1)) || true
      if [ -n "$TAP" ]; then echo "$ln"; else echo "  $ln"; fi
      lines=$((lines+1))
    done < "$body"
  done
done

if [ -n "$TAP" ]; then
  echo "1..${total}"
  echo "# opt-matrix: ${total} tests, $((total-failed)) passed, ${failed} failed"
else
  echo "== opt-matrix: ${total} tests, $((total-failed)) passed, ${failed} failed =="
fi
# keep the per-mode logs around for CI on failure
if [ -n "${TEST_LOG_DIR:-}" ]; then mkdir -p "$TEST_LOG_DIR"; cp "${OUT}"/* "$TEST_LOG_DIR"/ 2>/dev/null || true; fi
[ "$failed" -eq 0 ]
