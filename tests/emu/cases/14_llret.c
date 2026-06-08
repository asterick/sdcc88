/* 14_llret — 64-bit (long long) return-value ABI. Isolates the diff-suite
 * miscompile found mining #11. Uses inline ops (no support call) so the bare
 * emu link needs no long long runtime. */
#include "emu.h"

static long long sadd(long long x, long long y) { return x + y; }
static long long sconst(void) { return 0x1122334455667788LL; }

volatile long long a, b;

int main(void)
{
    long long r;

    r = sconst();
    CHECK(r == 0x1122334455667788LL);

    a = 0x00000000FFFFFFFFLL; b = 1;
    r = sadd(a, b);
    CHECK(r == 0x0000000100000000LL);   /* carry across the 32-bit half */

    a = -1; b = 1;
    CHECK(sadd(a, b) == 0);

    a = 0x7FFFFFFFFFFFFFFFLL; b = 1;
    CHECK(sadd(a, b) == (long long)0x8000000000000000LL);

    emu_puts("llret ok\n");
    EMU_DONE();
}
