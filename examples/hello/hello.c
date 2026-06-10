/* hello.c — a minimal Pokémon Mini ROM built with sdcc88.
 *
 *     make            -> hello.min   (a flat Pokémon Mini ROM image)
 *     make run        -> build + run on the bundled emulator
 *
 * It greets the world and prints a number computed with the support library
 * (the %u conversion uses the runtime divide/modulo from s1c88.lib).
 *
 * Output channel: printf() goes through the library's default putchar(), which
 * stores each byte at the minimon debug console (DEBUG_OUT, RAM 0x1FF8) so
 * `make run` shows the text.  Real Pokémon Mini hardware has no text console —
 * define your own putchar() (drawing to the LCD via the PRC; see the PRC
 * registers, OAM and TILEMAP in <pm.h>) to override the default for a cartridge.
 */
#include <pm.h>
#include <stdio.h>

/* Key-input interrupt handlers (cart vector slots 15..22, see <pm.h> VEC_KEY*).
 *
 * All 8 key IRQs share the IRQ_ACT3 acknowledge register (0x2029): writing a 1
 * to the matching IRQ3_KEY* bit clears that pending event so it doesn't refire.
 * Each handler does only that — acknowledge and return (the compiler emits the
 * RETE; __interrupt(N) also auto-defines the _irq_v<N> label the crt0 header
 * trampoline bjumps to, so no hand-wiring is needed).
 *
 * These only keep a delivered key IRQ from re-firing; the sources are armed
 * (priority + enable + CPU unmask) in main() below. */
void key_power(void) __interrupt(VEC_KEYPOWER) { IRQ_ACT3 = IRQ3_KEYPOWER; }
void key_right(void) __interrupt(VEC_KEYRIGHT) { IRQ_ACT3 = IRQ3_KEYRIGHT; }
void key_left (void) __interrupt(VEC_KEYLEFT)  { IRQ_ACT3 = IRQ3_KEYLEFT;  }
void key_down (void) __interrupt(VEC_KEYDOWN)  { IRQ_ACT3 = IRQ3_KEYDOWN;  }
void key_up   (void) __interrupt(VEC_KEYUP)    { IRQ_ACT3 = IRQ3_KEYUP;    }
void key_c    (void) __interrupt(VEC_KEYC)     { IRQ_ACT3 = IRQ3_KEYC;     }
void key_b    (void) __interrupt(VEC_KEYB)     { IRQ_ACT3 = IRQ3_KEYB;     }
void key_a    (void) __interrupt(VEC_KEYA)     { IRQ_ACT3 = IRQ3_KEYA;     }

int main(void)
{
    unsigned int n = 1000;

    /* Enable the key interrupts (the BIOS leaves them masked at reset):
     *   1. give the key IRQ group a nonzero priority level (PRI2_KEY),
     *   2. unmask all 8 key sources in IRQ_ENA3,
     *   3. lower the CPU's SC interrupt-priority level (I1:I0, bits 7:6) from 3
     *      (all maskable IRQs blocked) to 0, so any source above level 0 is
     *      accepted. SC is a CPU register, not MMIO, so step 3 is one inline
     *      instruction — the mirror of the `or sc,#0xc0` the compiler emits for
     *      __critical. After this, pressing a key vectors into the handlers above. */
    IRQ_PRI2 |= PRI2_KEY(2);
    IRQ_ENA3  = 0xFF;
    __asm
        and sc, #0x3f
    __endasm;

    printf("Hello, Pokemon Mini!\n");
    printf("%u / 7 = %u\n", n, n / 7u);     /* 1000 / 7 = 142 */

    return 0;                               /* the crt0 halts; exit code 0 = ok */
}
