/* 16_ptrcmp — pointer comparison codegen (from #11-ptrarith mining).
 *
 * Guards the canonical `p < end` loop bound, equality of computed addresses,
 * relational compares where at least one side is a plain pointer variable, AND
 * (the regression for the FIXED #11-ptrcmp bug) a relational compare of TWO
 * freshly address-of'd elements with runtime indices, `&arr[ii] < &arr[jj]`.
 *
 * THE FIXED BUG: that double-address-of relational form lowers to the native
 * `cp ba, hl` pair compare. The peephole read-analysis (`s1c88MightRead`) had no
 * case for `ba` as the FIRST operand of `cp`, so it fell through to `argCont`,
 * which stops at the comma and never saw the `hl` second operand — so reads of
 * HL by `cp ba, hl` were invisible, and the peephole deleted the `add hl, ba`
 * (and the index shift) that built the right-hand address. Fix: peep.c teaches
 * the cp/or read-analysis the `cp ba, rr` form.
 */
#include "emu.h"
volatile unsigned int arr[8];

static unsigned int count_to(unsigned char n)        /* the common loop bound */
{
    volatile unsigned int *p = arr, *end = arr + n;
    unsigned int c = 0;
    while (p < end) { c++; p++; }
    return c;
}

/* the previously-miscompiled form: relational compare of two computed addresses */
static unsigned char lt(unsigned char ii, unsigned char jj) { return &arr[ii] <  &arr[jj]; }
static unsigned char ge(unsigned char ii, unsigned char jj) { return &arr[ii] >= &arr[jj]; }

int main(void)
{
    volatile unsigned char k = 3;
    volatile unsigned int *p = arr, *q = arr + k;
    volatile unsigned char i1 = 1, i3 = 3, i5 = 5;

    CHECK(count_to(0) == 0);
    CHECK(count_to(5) == 5);
    CHECK(count_to(8) == 8);

    CHECK((p <  q) == 1);     /* var ptr vs computed ptr */
    CHECK((q <  p) == 0);
    CHECK((p >= q) == 0);
    CHECK((p <= p) == 1);     /* equal */
    CHECK((p == arr) == 1);   /* equality of computed addresses */
    CHECK((q == arr) == 0);

    CHECK(lt(i1, i3) == 1);   /* &arr[1] <  &arr[3]  (was the silent miscompile) */
    CHECK(lt(i5, i3) == 0);   /* &arr[5] <  &arr[3] */
    CHECK(lt(i3, i3) == 0);   /* equal: not less */
    CHECK(ge(i5, i3) == 1);   /* &arr[5] >= &arr[3] */
    CHECK(ge(i1, i3) == 0);
    CHECK(ge(i3, i3) == 1);   /* equal: >= holds */

    emu_puts("ptrcmp ok\n");
    EMU_DONE();
}
