#!/usr/bin/env bash
#
# emu-test.sh — EXECUTION tests for the s1c88 codegen, run on the vendored minimon
# emulator core (third_party/minimon-core, the Pokémon Mini ground truth).
#
# Where corpus-check.sh proves the asm is stable + assembles, this proves it RUNS:
# each tests/emu/cases/*.c is compiled (host cpp -> sdcc -ms1c88 --c1mode), assembled
# (sdas88), linked with tests/emu/crt0.asm (sdldz80, _HOME=0x2100 _DATA=0x1000),
# romgen'd to a flat .min, and executed in the emulator. The guest's main() return
# value (0 = pass; CHECK() failures print + count) becomes the verdict.
#
#   scripts/emu-test.sh             # overlay+rebuild sdcc, then run all cases
#   scripts/emu-test.sh 04          # run only cases matching '04'
#   EMU_NO_BUILD=1 scripts/emu-test.sh   # skip the compiler rebuild (reuse last build)
#   TAP=1 scripts/emu-test.sh       # emit TAP body lines (for scripts/run-tests.sh)
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
[ -x "${SDCC}/bin/romgen" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
SDCCBIN="${SDCC}/src/sdcc"
SDAS="${SDCC}/bin/sdas88"
SDLD="${SDCC}/bin/sdldz80"
EMU="${REPO}/tests/emu"
RUNNER="${REPO}/build/emu/runner"
FILTER="${1:-}"
TAP="${TAP:-}"

# In TAP mode, progress/noise goes to stderr so stdout is a clean TAP stream.
note() { if [ -n "$TAP" ]; then echo "$@" >&2; else echo "$@"; fi; }
# Per-case reporting. report_pass <name>; report_fail <name> <verdict> [<diagfile>]
report_pass() {
  if [ -n "$TAP" ]; then echo "ok - emu/$1"; else echo "PASS"; fi
  pass=$((pass+1))
}
report_fail() {
  if [ -n "$TAP" ]; then
    echo "not ok - emu/$1 # $2"
    [ -n "${3:-}" ] && [ -s "$3" ] && sed 's/^/#   /' "$3"
  else
    echo "$2"
    [ -n "${3:-}" ] && [ -s "$3" ] && sed 's/^/    /' "$3"
  fi
  fail=$((fail+1))
}

[ -x "$SDAS" ] && [ -x "$SDLD" ] || { echo "!! build sdas88 + sdldz80 first (scripts/build-sdas.sh / build-sdld.sh)" >&2; exit 2; }

# --- overlay + rebuild the compiler (same as dev.sh / corpus-check.sh) ---
if [ -z "${EMU_NO_BUILD:-}" ]; then
  [ -f "${SDCC}/config.status" ] || { echo ">> not configured — run ./build.sh first" >&2; exit 2; }
  note ">> overlay + build sdcc"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    echo "!! BUILD FAILED:" >&2; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20 >&2; exit 1
  fi
fi
[ -x "$SDCCBIN" ] || { echo "!! no sdcc at ${SDCCBIN}" >&2; exit 2; }

# --- build the emulator runner ---
note ">> build emulator runner"
make -s -C "$EMU" >&2 || { echo "!! runner build FAILED" >&2; exit 1; }

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

# --- assemble crt0 once ---
"$SDAS" -o "${OUT}/crt0.rel" "${EMU}/crt0.asm" >&2 || { echo "!! crt0 assemble FAILED" >&2; exit 1; }

# --- runtime library: SDCC's generic C support routines, compiled by OUR port ---
# (self-hosting: the runtime exercises the codegen too). Linked into every case;
# sdld links whole objects, but bank-0 ROM is ample for these.
RT_SRCS="_divuint _divsint _moduint _modsint _mulint"
RT_RELS=""
for r in $RT_SRCS; do
  if ! cc -E -P -x c -I "${SDCC}/device/include" -D_SDCC_NO_ASM_LIB_FUNCS \
        "${SDCC}/device/lib/${r}.c" > "${OUT}/${r}.i" 2>"${OUT}/err" \
     || ! "$SDCCBIN" -ms1c88 --c1mode -o "${OUT}/${r}.asm" < "${OUT}/${r}.i" 2>"${OUT}/err" \
     || ! "$SDAS" -o "${OUT}/${r}.rel" "${OUT}/${r}.asm" > "${OUT}/err" 2>&1; then
    echo "!! runtime ${r} build FAILED:" >&2; sed 's/^/    /' "${OUT}/err" | head -10 >&2; exit 1
  fi
  RT_RELS="${RT_RELS} ${OUT}/${r}.rel"
done

pass=0; fail=0
for src in "${EMU}"/cases/*.c; do
  b="$(basename "$src" .c)"
  case "$b" in *"$FILTER"*) ;; *) continue ;; esac
  [ -z "$TAP" ] && printf "  %-14s " "$b"

  # host cpp (no sdcpp in this build) -> sdcc -> sdas88
  if ! cc -E -P -x c -I "$EMU" "$src" > "${OUT}/${b}.i" 2>"${OUT}/err"; then
    report_fail "$b" "CPP-FAIL" "${OUT}/err"; continue
  fi
  if ! "$SDCCBIN" -ms1c88 --c1mode -o "${OUT}/${b}.asm" < "${OUT}/${b}.i" 2>"${OUT}/err" \
     || grep -qiE 'Internal Error|backtrace|FATAL' "${OUT}/err"; then
    head -10 "${OUT}/err" > "${OUT}/diag"; report_fail "$b" "COMPILE-FAIL" "${OUT}/diag"; continue
  fi
  if ! "$SDAS" -o "${OUT}/${b}.rel" "${OUT}/${b}.asm" > "${OUT}/err" 2>&1; then
    head -10 "${OUT}/err" > "${OUT}/diag"; report_fail "$b" "ASSEMBLE-FAIL" "${OUT}/diag"; continue
  fi

  # __far data lane (task #9): a case using __far gets a _FAR area located at its
  # PHYSICAL address (page 1 = 0x10000, disjoint from the bank-0 code) and romgen
  # treats that range as physical, so the codegen's (sym>>16) page byte is the
  # address the data bus sees. Only far cases pay it (others link byte-identically).
  FAR_LINK=""; FAR_ROMGEN=""
  if grep -q '__far' "$src"; then
    FAR_LINK="-b _FAR=0x10000"; FAR_ROMGEN="--far=0x10000-0x1ffff"
  fi

  # link (crt0 first: fixes the entry at 0x2100 and the area order) -> .min
  # _CODE needs an explicit base: sdas88 implicitly opens _CODE as area 0 of every
  # module, so it is first in link order and would land at 0. _HOME at 0x2100 is
  # followed by the chained _GSINIT/_GSFINAL/_INITIALIZER (must stay under 0x4000);
  # _INITIALIZED chains after _DATA in RAM.
  if ! "$SDLD" -nwxi -b _CODE=0x4000 -b _HOME=0x2100 -b _DATA=0x1000 ${FAR_LINK} \
        "${OUT}/${b}.ihx" "${OUT}/crt0.rel" "${OUT}/${b}.rel" ${RT_RELS} > "${OUT}/err" 2>&1 \
     || grep -q "Undefined Global" "${OUT}/err"; then
    grep -E "Undefined Global|ASlink" "${OUT}/err" | sort -u > "${OUT}/diag"
    report_fail "$b" "LINK-FAIL" "${OUT}/diag"; continue
  fi
  if ! "${SDCC}/bin/romgen" "${OUT}/${b}.ihx" "${OUT}/${b}.min" ${FAR_ROMGEN} > "${OUT}/err" 2>&1; then
    report_fail "$b" "ROMGEN-FAIL" "${OUT}/err"; continue
  fi

  # run; guest exit code 0 = pass (124 stray-halt / 125 crash / 126 timeout)
  guest_out="$("$RUNNER" "${OUT}/${b}.min" 2>"${OUT}/err")"; rc=$?
  if [ "$rc" -eq 0 ]; then
    report_pass "$b"
  else
    case "$rc" in
      124) verdict="STRAY-HALT" ;;
      125) verdict="CPU-CRASH" ;;
      126) verdict="TIMEOUT" ;;
      *)   verdict="FAIL (exit $rc)" ;;
    esac
    # guest_out carries the per-assertion CHECK failures ("FAIL: <expr>")
    { [ -n "$guest_out" ] && printf '%s\n' "$guest_out"; head -5 "${OUT}/err"; } > "${OUT}/diag"
    report_fail "$b" "$verdict" "${OUT}/diag"
  fi
done

if [ -z "$TAP" ]; then echo "== emu-test: ${pass} passed, ${fail} failed =="; fi
[ "$fail" -eq 0 ]
