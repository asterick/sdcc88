/* farimmd — a __far object's ADDRESS as a 3-byte immediate VALUE: passed as an
 * argument, stored into a far pointer, compared, and widened.
 *
 * The high word of a 3-byte symbolic immediate (bytes 1..2 = sym>>8, sym>>16)
 * was sliced through fetchLitPair, whose offset is an ADDRESS offset, so the
 * pushed/stored image was [lo, lo+1, hi]. Differential vs host (where __far is
 * plain memory; the values read back are what matter).
 */
#include "harness.h"

__far const u8 ftab[6] = { 3, 1, 4, 1, 5, 9 };
__far const i16 fw[3] = { -300, 7, 12345 };
__far u8 fout[4];

static u8  rd8(const u8 __far *p, u8 i)        { return p[i]; }
static i16 rd16(const i16 __far *p, u8 i)      { return p[i]; }
static u16 acc(const u8 __far *p, u8 n)        { u16 s = 0; while (n--) s += *p++; return s; }
static void wr8(u8 __far *p, u8 i, u8 v)       { p[i] = v; }
static u8  rd8b(u8 bias, const u8 __far *p, u8 i) { return (u8)(p[i] + bias); }

volatile const u8 __far *gfp;

void diff_run(void)
{
    gfp = ftab;                                   /* store the immediate into a far pointer */
    EMIT_U8("rd8_0",   rd8(ftab, 0));
    EMIT_U8("rd8_5",   rd8(ftab, 5));
    EMIT_U8("rd8_off", rd8(ftab + 2, 1));
    EMIT_I16("rd16_0", rd16(fw, 0));
    EMIT_I16("rd16_2", rd16(fw, 2));
    EMIT_U16("acc",    acc(ftab, 6));
    EMIT_U8("rd8b",    rd8b(10, ftab, 4));
    EMIT_U8("via_gfp", rd8(gfp, 2));
    wr8(fout, 1, 0x5A);
    EMIT_U8("fout1",   fout[1]);
    EMIT_U8("eq",      (u8)(gfp == ftab));
    EMIT_U8("ne",      (u8)(gfp == (const u8 __far *)fout));
}
