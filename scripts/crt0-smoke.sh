#!/usr/bin/env bash
#
# crt0-smoke.sh — prove the PRODUCTION crt0 (device/lib/s1c88/crt0.s) boots a C
# program end to end through the real Pokémon Mini cartridge header.
#
# Unlike tests/emu/crt0.asm (the bare test startup with code at 0x2100), this links
# the production crt0: "PM" header @ 0x2100, 6-byte IRQ vector slots, "NINTENDO" @
# 0x21A4, reset-vector entry. The emulator runner's mini-BIOS (auto-detected via the
# "PM" marker) synthesizes the low vector table and enters via the reset slot, exactly
# as real hardware does. A global initializer exercises gsinit (_INITIALIZER copy);
# main()'s return value is the verdict.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
[ -x "${SDCC}/bin/romgen" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
SDCCBIN="${SDCC}/src/sdcc"; SDAS="${SDCC}/bin/sdas88"; SDLD="${SDCC}/bin/sdldz80"
EMU="${REPO}/tests/emu"; RUNNER="${REPO}/build/emu/runner"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

for t in "$SDCCBIN" "$SDAS" "$SDLD"; do [ -x "$t" ] || { echo "!! missing $t — run build.sh + build-sdas/sdld" >&2; exit 2; }; done

# build the runner (mini-BIOS lives here)
make -s -C "$EMU" >/dev/null || { echo "!! runner build FAILED"; exit 1; }

# assemble the production crt0
"$SDAS" -o "${OUT}/crt0.rel" "${REPO}/device/lib/s1c88/crt0.s" || { echo "!! crt0 assemble FAILED"; exit 1; }

# a tiny C program: global initializer (exercises gsinit copy) + a far-ish return value
cat > "${OUT}/t.c" <<'EOF'
int g = 35;                 /* initialized global -> _INITIALIZER -> _INITIALIZED   */
int z;                      /* zero-init global  -> must read 0 (BIOS/runner clears RAM) */
int seven(void){ return 7; }
int main(void){ return g + seven() + z; }   /* expect 42 (z==0) */
EOF
cc -E -P -x c "${OUT}/t.c" > "${OUT}/t.i"
"$SDCCBIN" -ms1c88 --c1mode -o "${OUT}/t.asm" < "${OUT}/t.i" || { echo "!! compile FAILED"; exit 1; }
"$SDAS" -o "${OUT}/t.rel" "${OUT}/t.asm" || { echo "!! assemble FAILED"; exit 1; }

# link: header is a fixed ABS area @ 0x2100; _CODE goes after it @ 0x21D0; RAM @ 0x1000.
# (SP is set by the BIOS / the runner's mini-BIOS on reset; crt0 doesn't touch it.)
LIBDIR="${SDCC}/share/sdcc/lib/s1c88"
"$SDLD" -nwxi -b _CODE=0x21D0 -b _DATA=0x1000 -k "$LIBDIR" -l s1c88 \
    "${OUT}/t.ihx" "${OUT}/crt0.rel" "${OUT}/t.rel" > "${OUT}/err" 2>&1 || {
  echo "!! LINK FAILED"; grep -E "Undefined|ASlink" "${OUT}/err" | sort -u | sed 's/^/    /'; exit 1; }

"${SDCC}/bin/romgen" "${OUT}/t.ihx" "${OUT}/t.min" > "${OUT}/err" 2>&1 || {
  echo "!! romgen FAILED"; cat "${OUT}/err"; exit 1; }

# verify the header bytes landed correctly in the .min (file byte 0 == phys 0x2100)
magic="$(dd if="${OUT}/t.min" bs=1 count=2 2>/dev/null)"
nin="$(dd if="${OUT}/t.min" bs=1 skip=$((0x21A4-0x2100)) count=8 2>/dev/null)"
[ "$magic" = "PM" ] || { echo "!! magic at 0x2100 is '$magic', expected 'PM'"; exit 1; }
[ "$nin" = "NINTENDO" ] || { echo "!! watermark at 0x21A4 is '$nin', expected 'NINTENDO'"; exit 1; }
echo ">> header OK: 'PM' @ 0x2100, 'NINTENDO' @ 0x21A4"

# run through the mini-BIOS
set +e
"$RUNNER" --verbose "${OUT}/t.min" >"${OUT}/run.out" 2>"${OUT}/run.err"; rc=$?
set -e
grep -q "PM cart:" "${OUT}/run.err" || { echo "!! runner did NOT take the PM boot path"; cat "${OUT}/run.err"; exit 1; }
if [ "$rc" -eq 42 ]; then
  echo ">> crt0-smoke: PASS (booted via PM reset vector, gsinit ran, main()=42)"
else
  echo "!! crt0-smoke: FAIL (exit=$rc, expected 42)"; cat "${OUT}/run.err"; exit 1
fi
