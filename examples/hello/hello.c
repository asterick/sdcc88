/* hello.c — a minimal Pokémon Mini ROM built with sdcc88.
 *
 *     make            -> hello.min   (a flat Pokémon Mini ROM image)
 *     make run        -> build + run on the bundled emulator
 *
 * It greets the world with printf and prints a number computed with the support
 * library (the %u conversion uses the runtime divide/modulo from s1c88.lib), and
 * wires up a single key-input interrupt (the A button) as a worked example.
 *
 * Output channel: printf() goes through the library's default putchar(), which
 * stores each byte at the minimon debug console (DEBUG_OUT, RAM 0x1FF8) so
 * `make run` shows the text.  Real Pokémon Mini hardware has no text console —
 * define your own putchar() (drawing to the LCD via the PRC; see the PRC
 * registers, OAM and TILEMAP in <pm.h>) to override the default for a cartridge.
 */
#include <pm.h>
#include <stdio.h>

/* A-button interrupt handler (cart vector slot 22, see <pm.h> VEC_KEYA).
 *
 * `__interrupt(VEC_KEYA)` auto-defines the _irq_v22 label the crt0 header
 * trampoline bjumps to, and the compiler emits the RETE — no hand-wiring.  The
 * handler just acknowledges the event: writing the IRQ3_KEYA bit to IRQ_ACT3
 * (0x2029) clears it so it doesn't immediately refire.  (The other 25 cart
 * vectors fall through to the runtime's do-nothing RETE defaults.) */
void key_a (void) __interrupt (VEC_KEYA)
{
    IRQ_ACT3 = IRQ3_KEYA;
}

int main (void)
{
    unsigned int n = 1000;

    /* Enable the A-button interrupt (the BIOS leaves key IRQs masked at reset):
     *   1. give the key IRQ group a nonzero priority level (PRI2_KEY),
     *   2. unmask just the A source in IRQ_ENA3,
     *   3. lower the CPU's SC interrupt-priority level (I1:I0, bits 7:6) from 3
     *      (all maskable IRQs blocked) to 0, so any source above level 0 is
     *      accepted.  SC is a CPU register, not MMIO, so step 3 is one inline
     *      instruction — the mirror of the `or sc,#0xc0` the compiler emits for
     *      __critical.  After this, pressing A vectors into key_a() above. */
    IRQ_PRI2 |= PRI2_KEY (2);
    IRQ_ENA3  = IRQ3_KEYA;
    __asm
        and sc, #0x3f
    __endasm;

    printf ("Hello, Pokemon Mini!\n");
    printf ("%u / 7 = %u\n", n, n / 7u);     /* 1000 / 7 = 142 */

    return 0;                                /* the crt0 halts; exit code 0 = ok */
}
