#!/usr/bin/env bash
#
# run-tests.sh — the unified test runner for the s1c88 port. Builds the compiler
# once, runs every suite IN PARALLEL, and emits a single TAP version 13 stream
# (one test point per case, with per-assertion diagnostics) plus a summary.
#
# Suites:
#   corpus  — byte-identical codegen + clean assembly (scripts/corpus-check.sh)
#   emu     — execution on the minimon core            (scripts/emu-test.sh)
#   diff    — host-vs-emulator differential            (scripts/diff-test.sh)
#   smoke   — toolchain invariants, one test each      (branch-smoke, insn-size-check)
#
# Each case-suite is invoked with TAP=1 (clean `ok`/`not ok` body on stdout) and
# EMU_NO_BUILD=1 (reuse the single up-front compiler build). Output is deterministic
# (suites run concurrently but are composed in a fixed order).
#
#   scripts/run-tests.sh             # build once, run everything, print TAP
#   scripts/run-tests.sh | tee t.tap # capture for CI / a TAP consumer (prove, tappy, …)
#   EMU_NO_BUILD=1 scripts/run-tests.sh   # skip the up-front compiler rebuild
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"
SDAS="${SDCC}/bin/sdas88"
SDLD="${SDCC}/bin/sdldz80"

log() { echo "$@" >&2; }

# --- preflight: the binary toolchain must exist (built once, reused by every suite) ---
[ -f "${SDCC}/config.status" ] || { log "!! not configured — run ./build.sh first"; exit 2; }
[ -x "$SDAS" ] || { log ">> building sdas88"; "${REPO}/scripts/build-sdas.sh" as88 >&2 || exit 2; }
[ -x "$SDLD" ] || { log ">> building sdldz80"; "${REPO}/scripts/build-sdld.sh" >&2 || exit 2; }
[ -x "${SDCC}/bin/romgen" ] || { log ">> building romgen"; "${REPO}/scripts/build-romgen.sh" >&2 || exit 2; }

# --- build the compiler ONCE (overlay + make); suites then run with EMU_NO_BUILD=1 ---
if [ -z "${EMU_NO_BUILD:-}" ]; then
  log ">> overlay + build sdcc (once)"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    log "!! BUILD FAILED:"; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20 >&2; exit 1
  fi
fi
[ -x "$SDCCBIN" ] || { log "!! no sdcc at ${SDCCBIN}"; exit 2; }
# Build the emulator runner once, serially, so the parallel suites see it up-to-date
# (concurrent `make` on the same target would race).
log ">> build emulator runner (once)"
make -s -C "${REPO}/tests/emu" >&2 || { log "!! runner build FAILED"; exit 1; }

TAPDIR="$(mktemp -d)"; trap 'rm -rf "$TAPDIR"' EXIT

# --- run the case-suites in parallel; each writes its TAP body to a file ----------
log ">> running suites in parallel: corpus, emu, diff, smoke"
TAP=1 EMU_NO_BUILD=1 "${REPO}/scripts/corpus-check.sh" check >"${TAPDIR}/corpus" 2>"${TAPDIR}/corpus.log" &
TAP=1 EMU_NO_BUILD=1 "${REPO}/scripts/emu-test.sh"        >"${TAPDIR}/emu"    2>"${TAPDIR}/emu.log"    &
TAP=1 EMU_NO_BUILD=1 "${REPO}/scripts/diff-test.sh"       >"${TAPDIR}/diff"   2>"${TAPDIR}/diff.log"   &

# --- smoke suite: one TAP point per toolchain-invariant script (run in this shell) ---
{
  for s in branch-smoke insn-size-check vec-reorder-smoke relax-symtab-smoke rom-smoke; do
    if "${REPO}/scripts/${s}.sh" >"${TAPDIR}/${s}.out" 2>&1; then
      echo "ok - smoke/${s}"
    else
      echo "not ok - smoke/${s} # exit $?"
      tail -8 "${TAPDIR}/${s}.out" | sed 's/^/#   /'
    fi
  done
} >"${TAPDIR}/smoke" 2>/dev/null &

wait   # all suites done

# --- compose one numbered TAP stream in a fixed suite order ----------------------
echo "TAP version 13"
total=0; failed=0
emit_suite() {  # $1 = label, $2 = tap body file
  echo "# Suite: $1"
  [ -s "$2" ] || { echo "# (no output — suite may have errored; see stderr log)"; failed=$((failed+1)); return; }
  local line
  while IFS= read -r line; do
    case "$line" in
      "ok - "*)     total=$((total+1)); echo "ok ${total} - ${line#ok - }" ;;
      "not ok - "*) total=$((total+1)); failed=$((failed+1)); echo "not ok ${total} - ${line#not ok - }" ;;
      "#"*)         echo "$line" ;;
      *)            ;;  # drop stray non-TAP lines
    esac
  done < "$2"
}
emit_suite corpus "${TAPDIR}/corpus"
emit_suite emu    "${TAPDIR}/emu"
emit_suite diff   "${TAPDIR}/diff"
emit_suite smoke  "${TAPDIR}/smoke"

echo "1..${total}"
echo "# ${total} tests, $((total-failed)) passed, ${failed} failed"
[ "$failed" -eq 0 ] || log ">> FAILURES — see the 'not ok' lines above (suite stderr in ${TAPDIR}/*.log were captured during the run)"
[ "$failed" -eq 0 ]
