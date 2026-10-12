/* 22_far_arg — the ADDRESS of a __far object passed as a call argument.
 *
 * `rd(tbl, i)` pushes the 3-byte far pointer immediate `&tbl` (lo, hi, page).
 * genIpush builds the high word with genMove_o(pair, aop, offset 1, 2 bytes),
 * which went through fetchLitPair — whose offset is an ADDRESS offset for
 * immediates — so the stack image became [lo, lo+1, hi] (`ld hl, #_tbl + 1`)
 * instead of [lo, hi, page]; every read through the argument hit garbage.
 * Far pointer VARIABLES (register / stack sources) were fine, which is all
 * 07_far / 13_farconst exercised. Fixed by slicing a 3-byte immediate's high
 * word as (#>(sym), #((sym) >> 16)).
 */
#include "emu.h"

__far const unsigned char tbl[8] = { 0xA5, 0x5A, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
__far const int wtbl[4] = { 1000, -2, 300, -4000 };
__far unsigned char fbuf[4];

static unsigned char rd(const unsigned char __far *p, unsigned char i) { return p[i]; }
static int rdw(const int __far *p, unsigned char i) { return p[i]; }
static unsigned int sum(const unsigned char __far *p, unsigned char n)
{
    unsigned int s = 0;
    while (n--) s += *p++;
    return s;
}
static void wr(unsigned char __far *p, unsigned char i, unsigned char v) { p[i] = v; }
static unsigned char rd3(unsigned char a, const unsigned char __far *p, unsigned char i) { return p[i] + a; }

int main(void)
{
    const unsigned char __far *fp = tbl;

    CHECK(rd(fp, 0) == 0xA5);                 /* far pointer VARIABLE (always worked) */
    CHECK(rd(tbl, 0) == 0xA5);                /* far object ADDRESS as the argument */
    CHECK(rd(tbl, 1) == 0x5A);
    CHECK(rd(tbl, 7) == 0x66);
    CHECK(rd(tbl + 2, 1) == 0x22);            /* address + constant offset */
    CHECK(rdw(wtbl, 0) == 1000);
    CHECK(rdw(wtbl, 3) == -4000);
    CHECK(sum(tbl, 8) == 0x264);
    CHECK(rd3(1, tbl, 2) == 0x12);            /* far address not in the first slot */

    wr(fbuf, 2, 0x77);
    CHECK(fbuf[2] == 0x77);
    CHECK(rd(fbuf, 2) == 0x77);

    emu_puts("far_arg ok\n");
    EMU_DONE();
}
