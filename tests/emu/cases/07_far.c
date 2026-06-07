/* 07_far — EXECUTION test for 3-byte __far pointers (task #9, abi-decision.md).
 *
 * __far objects are const ROM tables located by the linker at their PHYSICAL
 * 24-bit address (area _FAR, romgen --far); a far access stages the page into EP
 * and the offset into HL and walks (hl) under the EP=0 invariant. The emulator
 * resolves [hl] as (ep<<16)|hl into the unified memory array, so far reads of
 * the located tables AND far WRITES to scratch far addresses both work (the core
 * now combines RAM + cartridge into one writable space).
 *
 * crt0 places _FAR at physical 0x10000 (page 1) via -b _FAR / --far, so every
 * pointer here carries a nonzero page byte — the test would pass trivially if
 * the page were silently dropped (it was, in five middle-end miscompiles #9
 * fixed), so the nonzero-page assertion is load-bearing.
 */
#include "emu.h"

const char __far ctbl[8] = {11, 22, 33, 44, 55, 66, 77, 88};
const int __far itbl[4] = {0x1111, 0x2222, 0x3333, -5};

struct fflags { unsigned char b0 : 1; unsigned char mid : 3; signed char s : 4; };
struct fwide  { unsigned int w12 : 12; signed int s10 : 10; unsigned int full : 16; };
const struct fflags __far gff = {1, 5, -3};
const struct fwide  __far gfw = {0xABC, -200, 0x1234};

char __far *cp = (char __far *)ctbl;     /* 3-byte initializer, page reloc (sym>>16) */
int  __far *iptr = (int __far *)itbl;

volatile unsigned char idx = 3;          /* defeat constant folding of the index */

/* HLA far return (offset in HL, page in A) */
char __far *ret_tbl(void) { return (char __far *)ctbl; }

int main(void)
{
    /* page byte really is present (would be 0 if the page were dropped) */
    CHECK(((unsigned long)cp >> 16) != 0);

    /* basic char deref + constant displacement (add hl,#imm in the EP window) */
    CHECK(*cp == 11);
    CHECK(cp[3] == 44);
    CHECK(cp[7] == 88);

    /* runtime-indexed far table read: 24-bit symbolic address arithmetic */
    CHECK(ctbl[idx] == 44);

    /* int far deref: 2-byte read stays within the page (cpu_read16 keeps 0xFF0000) */
    CHECK(*iptr == 0x1111);
    CHECK(iptr[1] == 0x2222);
    CHECK(iptr[3] == -5);

    /* far pointer arithmetic then deref (24-bit linear add/sub) */
    {
        char __far *q = cp;
        q++;            CHECK(*q == 22);   /* index 1 */
        q += 4;         CHECK(*q == 66);   /* index 5 */
        q -= 2;         CHECK(*q == 44);   /* index 3 */
    }

    /* 3-byte compares */
    CHECK(cp == (char __far *)ctbl);
    CHECK(cp != 0);
    CHECK((char __far *)ctbl + 2 > cp);
    CHECK(ret_tbl() != 0);

    /* HLA far return, then deref the returned far pointer */
    CHECK(*ret_tbl() == 11);

    /* far <-> long: the page byte must survive the round trip */
    {
        unsigned long L = (unsigned long)cp;
        CHECK((L >> 16) != 0);
        char __far *r = (char __far *)L;
        CHECK(*r == 11);
    }

    /* far WRITES then read-back (unified writable memory): a scratch far address
       above the located _FAR tables, page 1 (0x12000 >> 16 == 1) */
    {
        char __far *w = (char __far *)0x12000UL;
        *w = 0x5a;
        CHECK(*w == 0x5a);
        w[1] = (char)0xa5;
        CHECK((unsigned char)w[1] == 0xa5);
        w[3] = 0x7e;                 /* constant displacement store */
        CHECK(w[3] == 0x7e);
        CHECK((unsigned char)w[1] == 0xa5);   /* earlier write survived */
    }
    {
        int __far *wi = (int __far *)0x12100UL;
        *wi = 0x1234;
        CHECK(*wi == 0x1234);        /* 2-byte far write/read within the page */
    }

    /* far bit-fields read from a const ROM struct (HL+EP fetch, mask at EP=0) */
    CHECK(gff.b0 == 1);
    CHECK(gff.mid == 5);
    CHECK(gff.s == -3);
    CHECK(gfw.w12 == 0xABC);
    CHECK(gfw.s10 == -200);
    CHECK(gfw.full == 0x1234);

    EMU_DONE();
}
