/* 12_nested_irq — EXECUTION test for NESTED interrupts: a higher-priority IRQ
 * preempting a lower-priority ISR that is already running (the follow-up left
 * open by 10_critical / docs TODO #10).
 *
 * Keypad edge IRQs make this deterministic where two phase-aligned timers were
 * fiddly: K1x keys live in priority group 5, K0x keys in group 6, so we can give
 * them DIFFERENT priorities and drive each edge from the host on demand via the
 * input mailbox (emu_set_keys -> the runner's update_inputs between instructions).
 *
 * Setup: group 5 (K1x) = priority 1 (LOW), group 6 (K0x) = priority 2 (HIGH).
 *   - main presses K10  -> the LOW handler fires.
 *   - the LOW handler, while running, presses K00 -> the HIGH handler must
 *     PREEMPT it (nested), run to completion, then control returns to the LOW
 *     handler, which finishes last.
 *
 * The event sequence proves the nesting order: low-entry, high-entry, high-exit,
 * low-exit  ==  {1, 2, 4, 3}.
 */
#include "emu.h"

#define MMIO8(a)  (*(volatile unsigned char *)(a))

/* vector table slots (low RAM, address = 2 * hardware IRQ number) */
#define VEC_K10   0x0028        /* IRQ 0x14 (K10), priority group 5 */
#define VEC_K00   0x0038        /* IRQ 0x1C (K00), priority group 6 */

/* all keys released = 0x3FF (active low); clear a bit to "press" that key */
#define KEYS_IDLE   0x3FF
#define PRESS_K10   (KEYS_IDLE & ~0x100)   /* bit 8 */
#define PRESS_K00   (PRESS_K10 & ~0x001)   /* bit 0, K10 still held */

volatile unsigned char seq[8];
volatile unsigned char seq_n = 0;
volatile unsigned char high_ran = 0;

#define LOG(e)  (seq[seq_n++ & 7] = (e))

static void irq_enable(void)  { __asm and sc, #0x3f __endasm; }
static void irq_disable(void) { __asm or  sc, #0xc0 __endasm; }

void k00_isr(void) __interrupt          /* HIGH priority (group 6) */
{
    LOG(2);                              /* high entry */
    high_ran = 1;
    LOG(4);                              /* high exit */
    MMIO8(0x2029) = 0x01;               /* ack K00 */
}

void k10_isr(void) __interrupt          /* LOW priority (group 5) */
{
    unsigned int guard = 0;
    LOG(1);                              /* low entry */
    emu_set_keys(PRESS_K00);            /* press K00 -> should preempt us */
    while (!high_ran && ++guard < 50000U)
        ;                                /* give the nested handler its window */
    LOG(3);                              /* low exit (after the nested handler ran) */
    MMIO8(0x2028) = 0x01;               /* ack K10 */
}

int main(void)
{
    unsigned long guard;

    *(volatile unsigned int *)VEC_K10 = (unsigned int)&k10_isr;
    *(volatile unsigned int *)VEC_K00 = (unsigned int)&k00_isr;

    MMIO8(0x2050) = 0x01;   /* K00 edge direction = falling (trigger on press) */
    MMIO8(0x2051) = 0x01;   /* K10 edge direction = falling (trigger on press) */
    MMIO8(0x2021) = 0x18;   /* group5(K1x)=prio 1 (low), group6(K0x)=prio 2 (high) */
    MMIO8(0x2024) = 0x01;   /* enable K10 */
    MMIO8(0x2025) = 0x01;   /* enable K00 */
    irq_enable();

    emu_set_keys(PRESS_K10);            /* fire the LOW-priority IRQ */

    guard = 0;
    while (seq_n < 4 && ++guard < 300000UL)
        ;
    irq_disable();

    CHECK(high_ran);                     /* the high handler ran at all */
    CHECK(seq_n == 4);                   /* exactly entry/exit of both handlers */
    CHECK(seq[0] == 1);                  /* low entered first */
    CHECK(seq[1] == 2);                  /* high preempted it (nested) ... */
    CHECK(seq[2] == 4);                  /* ... and finished ... */
    CHECK(seq[3] == 3);                  /* ... before the low handler resumed/exited */

    emu_puts("nested irq ok\n");
    EMU_DONE();
}
