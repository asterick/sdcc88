/* 10_critical — EXECUTION test for __critical sections (SC interrupt-level
 * masking, s12). First execution coverage of __critical.
 *
 * A timer ISR bumps `lo_count` continuously. We count how many times it fires
 * across an UNMASKED spin vs the same spin inside __critical. If masking works
 * the masked spin sees ~no fires while the unmasked spin sees many.
 *
 * Note on the snapshot: the start count is read just BEFORE entering __critical
 * and the end count INSIDE it (after the masked spin). Reading the start INSIDE
 * (right after the masking instruction) is unreliable — the emulator accepts a
 * pending IRQ in the one-instruction window at the mask boundary. The generous
 * `<= 2` threshold also tolerates that boundary fire; what matters is the order-
 * of-magnitude gap between the masked and unmasked fire counts.
 *
 * (Nested interrupts — a higher-priority IRQ preempting a lower-priority ISR —
 * need two phase-aligned timers and are a follow-up; see docs TODO #10.)
 */
#include "emu.h"

#define MMIO8(a)  (*(volatile unsigned char *)(a))

volatile unsigned int lo_count = 0;

void tim0_isr(void) __interrupt
{
    lo_count++;
    MMIO8(0x2027) = 0x04;                /* ack TIM0 */
}

static void irq_enable(void)  { __asm and sc, #0x3f __endasm; }
static void irq_disable(void) { __asm or  sc, #0xc0 __endasm; }

static unsigned int spin_fires(void)            /* fires over an UNMASKED spin */
{
    volatile unsigned int k;
    unsigned int start = lo_count;
    for (k = 0; k < 4000; k++)
        ;
    return lo_count - start;
}

static unsigned int spin_fires_critical(void)   /* fires over the same spin, MASKED */
{
    volatile unsigned int k;
    unsigned int start = lo_count;       /* snapshot before the critical section */
    unsigned int after = start;
    __critical {
        for (k = 0; k < 4000; k++)
            ;
        after = lo_count;                /* read inside, after the masked spin */
    }
    return after - start;
}

int main(void)
{
    unsigned int unmasked, masked;
    unsigned long guard;

    *(volatile unsigned int *)0x0010 = (unsigned int)&tim0_isr;   /* vector 0x08 */
    MMIO8(0x2019) = 0x20;   /* OSC3 enable; timer0-lo source = OSC3 */
    MMIO8(0x2018) = 0x08;   /* timer0-lo clock enable, prescale 0   */
    MMIO8(0x2032) = 0xFF;   /* preset lo */
    MMIO8(0x2030) = 0x06;   /* lo flags = PRESET | RUNNING (8-bit) */
    MMIO8(0x2020) = 0x04;   /* IRQ_PRI1: group 2 (TIM1/0) priority = 1 */
    MMIO8(0x2023) = 0x04;   /* enable TIM0 */
    irq_enable();

    guard = 0;
    while (lo_count == 0 && ++guard < 300000UL)
        ;
    CHECK(lo_count != 0);                 /* ISR is firing */

    unmasked = spin_fires();              /* baseline: many fires */
    masked   = spin_fires_critical();     /* inside __critical: ~none */
    irq_disable();

    CHECK(unmasked >= 8);                 /* the ISR fires often over the spin    */
    CHECK(masked <= 2);                   /* __critical masked it (allow boundary) */

    emu_puts("critical ok\n");
    EMU_DONE();
}
