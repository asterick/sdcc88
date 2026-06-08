/* 09_volatile — EXECUTION test for `volatile` semantics (matters for the <pm.h>
 * hardware registers): a volatile load must NOT be hoisted/cached out of a loop,
 * a volatile store must NOT be elided, and volatile accesses keep program order.
 *
 * The decisive test is load-hoisting (the classic "poll a status register" bug):
 * a timer ISR bumps `ticks`; main spin-reads it. If the compiler hoists the load,
 * it spins on a cached 0 until the safety bound and the `spins < BOUND` check
 * fails. If volatile works, the loop re-reads each iteration, sees the ISR's update,
 * and exits early. (Same timer/IRQ setup as 08_isr.)
 */
#include "emu.h"

#define MMIO8(a)  (*(volatile unsigned char *)(a))
#define BOUND     300000UL

volatile unsigned int  ticks = 0;
volatile unsigned char vbyte = 0;     /* a plain volatile RAM cell */

void tick_isr(void) __interrupt
{
    ticks++;
    MMIO8(0x2027) = 0x04;             /* acknowledge TIM0 */
}

static void irq_enable(void)  { __asm and sc, #0x3f __endasm; }
static void irq_disable(void) { __asm or  sc, #0xc0 __endasm; }

int main(void)
{
    unsigned long spins;
    unsigned int  seen;
    unsigned char i, acc;

    /* --- store/order/round-trip: a volatile RAM cell, written then read back.
       Each store must actually happen and each load must re-read it; if stores
       were coalesced or loads cached, the running sum diverges. --- */
    acc = 0;
    for (i = 1; i <= 8; i++) {
        vbyte = i;          /* volatile store */
        acc += vbyte;       /* volatile load — must see THIS store, not a cached one */
        vbyte = vbyte ^ 0xFF; /* RMW through volatile */
        acc += vbyte;       /* must re-read the modified value */
    }
    /* sum over i=1..8 of (i + (i^0xFF)) = sum of 0xFF = 8*255 = 2040; low byte 0xF8 */
    CHECK(acc == (unsigned char)2040);

    /* --- load-hoisting: install the timer ISR and spin on `ticks` --- */
    *(volatile unsigned int *)0x0010 = (unsigned int)&tick_isr;   /* vector 0x08 = TIM0 */
    MMIO8(0x2019) = 0x20;   /* OSC3 enable; timer0-lo source = OSC3   */
    MMIO8(0x2018) = 0x08;   /* timer0-lo clock enable, prescale 0     */
    MMIO8(0x2032) = 0xFF;   /* preset lo                              */
    MMIO8(0x2030) = 0x06;   /* lo flags = PRESET | RUNNING (8-bit)    */
    MMIO8(0x2020) = 0x04;   /* IRQ group 2 priority = 1               */
    MMIO8(0x2023) = 0x04;   /* enable TIM0 (IRQ 0x08)                 */
    irq_enable();

    spins = 0;
    while (ticks == 0) {            /* MUST re-read `ticks` every iteration */
        if (++spins >= BOUND)
            break;                 /* safety: if the load were hoisted we'd spin forever */
    }
    irq_disable();

    seen = ticks;                  /* a final genuine read */
    CHECK(seen != 0);              /* the ISR fired and main observed it          */
    CHECK(spins >= 1);             /* the loop ran at least once (timer not instant)*/
    CHECK(spins < BOUND);          /* DISCRIMINATOR: exited via the volatile read,
                                      not the safety bound -> the load was re-read  */

    emu_puts("volatile ok\n");
    EMU_DONE();
}
