/* 08_isr — EXECUTION test for interrupt handlers (the __interrupt prologue/
 * epilogue + RETE, s10/s20). Drives a real timer interrupt on the (kept) IRQ +
 * timers peripherals: program timer 0 to underflow, install its vector, lower
 * the CPU interrupt-priority so the IRQ is accepted, then run a fixed
 * computation while the ISR fires asynchronously. If the prologue/epilogue
 * save/restore is correct, the foreground sum is unperturbed; the ISR's own
 * counter proves it actually ran.
 *
 * The interrupt vector table lives in low memory (vector N at 2*N); the pruned
 * core makes that region writable, so we install &tim0_isr at 2*IRQ_TIM0 = 0x10.
 * IRQ_TIM0 is group 2; we give the group priority 1 and drop the CPU level to 0.
 */
#include "emu.h"

#define MMIO8(a) (*(volatile unsigned char *)(a))

volatile unsigned int  isr_count = 0;
volatile unsigned char isr_marker = 0;

void tim0_isr(void) __interrupt
{
    isr_count++;
    isr_marker = (unsigned char)(isr_count * 7u + 1u);  /* ISR-side work (clobbers regs) */
    MMIO8(0x2027) = 0x04;                                /* acknowledge TIM0 (clear active) */
}

static void irq_enable(void)  { __asm and sc, #0x3f __endasm; }   /* CPU level -> 0 */
static void irq_disable(void) { __asm or  sc, #0xc0 __endasm; }   /* CPU level -> 3 (mask) */

int main(void)
{
    unsigned int acc = 0;
    unsigned int i;

    /* install the ISR vector at 2 * IRQ_TIM0 (0x08) = 0x10 */
    *(volatile unsigned int *)0x0010 = (unsigned int)&tim0_isr;

    /* program timer 0 (lo, 8-bit) as a fast OSC3 down-counter */
    MMIO8(0x2019) = 0x20;   /* OSC3 enable; timer0 lo clock source = OSC3 */
    MMIO8(0x2018) = 0x08;   /* timer0 lo clock enable, prescale ratio 0 */
    MMIO8(0x2032) = 0xFF;   /* preset lo */
    MMIO8(0x2030) = 0x06;   /* lo flags = PRESET | RUNNING (8-bit mode) */

    /* enable the TIM0 interrupt: group 2 priority = 1, then unmask the source */
    MMIO8(0x2020) = 0x04;   /* priority group 2 = 1 */
    MMIO8(0x2023) = 0x04;   /* enable IRQ 0x08 (TIM0) */

    irq_enable();

    /* fixed foreground work: sum 0..255 = 32640. The ISR fires throughout; a
       correct prologue/epilogue leaves this result untouched. */
    for (i = 0; i < 256; i++)
        acc += i;

    irq_disable();

    CHECK(acc == 32640);                                       /* registers preserved across IRQs */
    CHECK(isr_count >= 1);                                     /* the ISR actually fired */
    CHECK(isr_marker == (unsigned char)(isr_count * 7u + 1u)); /* ISR body ran correctly */

    emu_puts("isr ran\n");
    EMU_DONE();
}
