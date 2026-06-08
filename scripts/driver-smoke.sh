#!/usr/bin/env bash
#
# driver-smoke.sh — prove the INTEGRATED driver works the way a user invokes it:
#   sdcc -ms1c88 game.c -o game.ihx   (sdcpp -> sdcc -> sdas88 -> sdldz80 + crt0 + s1c88.lib)
# then romgen -> .min, and RUN it on the emulator. This is the end-to-end "usable
# toolchain" guarantee (TODO A1-A4): preprocessing, the production crt0, the support
# library auto-link, the PM memory map, and --stack-loc all exercised in one shot.
#
# Prereqs: ./build.sh, scripts/build-sdcpp.sh, scripts/build-sdas.sh as88,
#          scripts/build-sdld.sh, scripts/build-runtime.sh (crt0.rel + s1c88.lib).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
[ -x "${SDCC}/bin/romgen" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
SDCCBIN="${SDCC}/src/sdcc"
RUNNER="${REPO}/build/emu/runner"
export PATH="${SDCC}/bin:${PATH}"          # sdcpp, sdas88, sdldz80 on PATH for the driver

[ -x "$SDCCBIN" ] || { echo "!! no sdcc — run ./build.sh" >&2; exit 2; }
[ -f "${SDCC}/share/sdcc/lib/s1c88/s1c88.lib" ] || { echo "!! no s1c88.lib — run scripts/build-runtime.sh" >&2; exit 2; }
make -s -C "${REPO}/tests/emu" >/dev/null

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cat > "${OUT}/game.c" <<'EOF'
/* exercises the support library (div) + an initialized global (gsinit copy) */
volatile int  a  = 1000, b = 7;     /* a/b = 142 via __divsint  */
volatile long la = 84,  lb = 2;     /* la/lb = 42 via __divslong */
int g = 100;                        /* initialized global        */
int main(void){
  int  q = a / b;                   /* 142 */
  long m = la / lb;                 /* 42  */
  return (int)m + (q - 142) + (g - 100);   /* == 42 */
}
EOF

run_case() {  # $1 = extra sdcc args, $2 = label
  local args="$1" label="$2"
  "$SDCCBIN" -ms1c88 $args "${OUT}/game.c" -o "${OUT}/game.ihx" 2>"${OUT}/err" \
    || { echo "!! [$label] driver build FAILED"; sed 's/^/    /' "${OUT}/err" | head; exit 1; }
  "${SDCC}/bin/romgen" "${OUT}/game.ihx" "${OUT}/game.min" >"${OUT}/err" 2>&1 \
    || { echo "!! [$label] romgen FAILED"; cat "${OUT}/err"; exit 1; }
  # header sanity: "PM" @ 0x2100, "NINTENDO" @ 0x21A4
  [ "$(dd if="${OUT}/game.min" bs=1 count=2 2>/dev/null)" = "PM" ] \
    || { echo "!! [$label] missing 'PM' marker @ 0x2100"; exit 1; }
  [ "$(dd if="${OUT}/game.min" bs=1 skip=$((0x21A4-0x2100)) count=8 2>/dev/null)" = "NINTENDO" ] \
    || { echo "!! [$label] missing 'NINTENDO' @ 0x21A4"; exit 1; }
  set +e; "$RUNNER" "${OUT}/game.min" >/dev/null 2>&1; local rc=$?; set -e
  [ "$rc" -eq 42 ] || { echo "!! [$label] ran but returned $rc (expected 42)"; exit 1; }
  echo ">> [$label] PASS — sdcc -ms1c88 ${args:-(defaults)}: built, header OK, booted, main()=42"
}

run_case ""                "default"
run_case "--opt-code-size" "opt-size"
echo ">> driver-smoke: PASS"
