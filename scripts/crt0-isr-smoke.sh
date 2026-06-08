#!/usr/bin/env bash
#
# crt0-isr-smoke.sh — prove the READ-ONLY-ROM interrupt model: a handler is
# installed by DEFINING the per-vector symbol the crt0 trampoline bjumps to, NOT
# by writing the low-memory vector table (which is BIOS ROM on real hardware).
#
# Uses the production crt0 (device/lib/s1c88/crt0.s): its header trampoline for
# cart vector N (`bjump _irq_vN`) dispatches the IRQ to the handler.  This program
# uses __interrupt(n) AUTO-WIRING: `void f(void) __interrupt(VEC_TIM1_LO_UF)` makes
# the compiler define _irq_v6 at f's entry (VEC_TIM1_LO_UF = cart slot 6, the BIOS
# forward of hardware IRQ $08 = Timer1-lower), overriding the library default; the
# other 25 vectors keep their lib defaults.  The runner's mini-BIOS synthesizes the
# 0x00-0xFF vector table from the cart trampolines using the real BIOS permutation,
# so dispatch runs exactly as on hardware.  main() returns 42 iff the ISR fired.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"; SDAS="${SDCC}/bin/sdas88"; SDLD="${SDCC}/bin/sdldz80"
RUNNER="${REPO}/build/emu/runner"; LIBDIR="${SDCC}/share/sdcc/lib/s1c88"
export PATH="${SDCC}/bin:${PATH}"
[ -x "${SDCC}/bin/romgen" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
for t in "$SDCCBIN" "$SDAS" "$SDLD"; do [ -x "$t" ] || { echo "!! missing $t" >&2; exit 2; }; done
[ -f "${LIBDIR}/crt0.rel" ] || { echo "!! run scripts/build-runtime.sh first" >&2; exit 2; }
make -s -C "${REPO}/tests/emu" >/dev/null

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT
cat > "${OUT}/isr.c" <<'EOF'
#include <pm.h>
#define MMIO8(a) (*(volatile unsigned char *)(a))
volatile unsigned int ticks = 0;

/* __interrupt(VEC_TIM1_LO_UF) -> the compiler defines _irq_v6, the symbol the
   crt0 trampoline at cart slot 6 (0x2126) bjumps to.  No `*(unsigned int*)0x10
   = ...` write — the low-memory vector table is read-only ROM on hardware. */
void tim_isr(void) __interrupt(VEC_TIM1_LO_UF)
{
    ticks++;
    MMIO8(0x2027) = 0x04;            /* acknowledge hardware IRQ $08 (Timer1-lower) */
}

int main(void)
{
    unsigned long guard = 0;
    MMIO8(0x2019) = 0x20;            /* OSC3 enable; timer0-lo source = OSC3 */
    MMIO8(0x2018) = 0x08;            /* timer0-lo clock enable */
    MMIO8(0x2032) = 0xFF;            /* preset */
    MMIO8(0x2030) = 0x06;            /* PRESET | RUNNING */
    MMIO8(0x2020) = 0x04;            /* IRQ_PRI1: group 2 priority 1 */
    MMIO8(0x2023) = 0x04;            /* enable TIM0 */
    __asm  and sc, #0x3f  __endasm; /* drop the CPU interrupt level */
    while (ticks == 0 && ++guard < 300000UL)
        ;
    return (ticks != 0) ? 42 : 7;   /* 42 = ISR dispatched through the trampoline */
}
EOF
# full driver: preprocess (<pm.h>) -> compile -> assemble -> link (production crt0
# + s1c88.lib, including the irq_v* defaults; irq_v8 overridden by this program).
"$SDCCBIN" -ms1c88 "${OUT}/isr.c" -o "${OUT}/isr.ihx" > "${OUT}/err" 2>&1 || {
  echo "!! driver build FAILED"; sed 's/^/    /' "${OUT}/err" | head; exit 1; }
"${SDCC}/bin/romgen" "${OUT}/isr.ihx" "${OUT}/isr.min" >/dev/null 2>&1 || { echo "!! romgen FAILED"; exit 1; }

set +e; "$RUNNER" "${OUT}/isr.min" >/dev/null 2>"${OUT}/rerr"; rc=$?; set -e
if [ "$rc" -eq 42 ]; then
  echo ">> crt0-isr-smoke: PASS — TIM0 ISR dispatched via the crt0 trampoline (no low-memory write)"
else
  echo "!! crt0-isr-smoke: FAIL (exit=$rc, expected 42)"; cat "${OUT}/rerr"; exit 1
fi
