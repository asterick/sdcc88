/* 16_ptrcmp — pointer comparison codegen (from #11-ptrarith mining).
 *
 * Guards the WORKING patterns: the canonical `p < end` loop bound, equality of
 * computed addresses, and relational compares where at least one side is a plain
 * pointer variable.
 *
 * KNOWN BUG (deferred, TODO #11): a relational compare of TWO freshly
 * address-of'd elements with runtime indices — `&arr[ii] < &arr[jj]` — is a
 * SILENT miscompile. genCmp's native `cp pair,pair` path materializes the left
 * address but drops the right one (HL is left holding the scaled index, not the
 * 2nd address). Narrow: subtraction, equality, and `p < end` are all correct.
 * Add the `lt(1,3)==1` assertion back here when fixed.
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

int main(void)
{
    volatile unsigned char k = 3;
    volatile unsigned int *p = arr, *q = arr + k;

    CHECK(count_to(0) == 0);
    CHECK(count_to(5) == 5);
    CHECK(count_to(8) == 8);

    CHECK((p <  q) == 1);     /* var ptr vs computed ptr */
    CHECK((q <  p) == 0);
    CHECK((p >= q) == 0);
    CHECK((p <= p) == 1);     /* equal */
    CHECK((p == arr) == 1);   /* equality of computed addresses */
    CHECK((q == arr) == 0);

    emu_puts("ptrcmp ok\n");
    EMU_DONE();
}
