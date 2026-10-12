/* 19_critical_sp — SP-relative locals INSIDE a __critical block.
 *
 * genCritical saves the interrupt level with `push sc` (1 byte) for the whole
 * block. The codegen's stack-depth counter (_G.stack.pushed) did not account
 * for that byte, so every `N (sp)` access inside the block — spilled locals,
 * EXSTK pair setup, stack args for a call — landed one byte low. crit5 below
 * returned 0x0201 instead of 3 (the store of `s` overlapped `r`'s high byte).
 *
 * Found by the 2026-10 quality review; locks the fix (the counter is bumped
 * for the duration of the block).
 */
#include "emu.h"

int crit5(int a, int b)
{
    volatile int r, s;
    __critical { r = a; s = b; }
    return r + s;
}

/* a volatile array local indexed inside the block (several SP-relative stores) */
int crit_arr(int a)
{
    volatile int v[4];
    unsigned char i;
    __critical {
        for (i = 0; i < 4; i++)
            v[i] = a + i;
    }
    return v[0] + v[1] + v[2] + v[3];
}

static int sum4(int a, int b, int c, int d) { return a + b + c + d; }

/* a call with stacked arguments inside the block (push/pop around the call
   must stay balanced with the extra saved byte) */
int crit_call(int a, int b)
{
    volatile int r;
    __critical { r = sum4(a, b, a + 1, b + 1); }
    return r;
}

int main(void)
{
    CHECK(crit5(1, 2) == 3);
    CHECK(crit5(1000, -7) == 993);
    CHECK(crit_arr(10) == 46);           /* 10+11+12+13 */
    CHECK(crit_call(3, 4) == 16);        /* 3+4+4+5 */

    emu_puts("critical_sp ok\n");
    EMU_DONE();
}
