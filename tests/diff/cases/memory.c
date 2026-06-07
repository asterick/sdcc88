/* memory — pointers, arrays, structs, unions, bitfields (differential vs host).
 *
 * SOUNDNESS for aggregates (host and target differ in struct padding, pointer
 * width, and bitfield allocation-unit size): only ever emit LOGICAL values —
 * member reads, runtime-computed element indices/sums, pointer DIFFERENCES
 * (element counts, sizeof cancels) — never raw struct bytes, sizeof, or pointer
 * bit patterns. Union punning uses only the width-exact u8/u16/u32 typedefs and
 * relies on both ends being little-endian (host x86-64 + S1C88). Members use the
 * width-exact typedefs so logical values match; bitfield widths fit both ends.
 */
#include "harness.h"

static u16 garr[8] = {10, 20, 30, 40, 50, 60, 70, 80};
static const u8 g2d[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

struct point { u8 x; u16 y; i8 z; };
struct line  { struct point a; struct point b; };
static struct point pts[3] = {{1, 0x100, -1}, {2, 0x200, -2}, {3, 0x300, -3}};

union pun { u32 l; u16 w[2]; u8 b[4]; };
/* Bitfields must use EXPLICIT signedness: a plain `int` bitfield's signedness is
   implementation-defined, and i16 is `int` on the target (impl-defined) but
   `int16_t` (signed) on the host — those legitimately differ. `signed char`
   is unambiguously signed on both. */
struct bits { u8 a : 1; u8 b : 3; u8 c : 4; u16 wide : 12; signed char s : 4; };

static volatile u8 i0 = 0, i1 = 1, i2 = 2, i3 = 3;   /* runtime indices (no folding) */

void diff_run(void)
{
    u8 i;

    /* --- arrays --- */
    EMIT_U16("arr_idx", garr[i3]);
    { u16 s = 0; for (i = 0; i < 8; i++) s += garr[i]; EMIT_U16("arr_sum", s); }

    /* --- pointer deref / walk / writeback --- */
    {
        u16 *p = garr;
        p += 2;
        EMIT_U16("ptr_deref", *p);
        *p = 0x999;
        EMIT_U16("ptr_wb", garr[2]);
        garr[2] = 30;                      /* restore */
    }
    /* pointer difference = element count (sizeof cancels host/target) */
    { u16 *p = &garr[5], *q = &garr[1]; EMIT_I16("ptr_diff", (i16)(p - q)); }

    /* --- pointer to pointer --- */
    {
        u16 v = 0x55aa, *p = &v, **pp = &p;
        EMIT_U16("pp", **pp);
        **pp = 0x1234;
        EMIT_U16("pp_wb", v);
    }

    /* --- 2D array --- */
    EMIT_U8("2d", g2d[i1][i3]);            /* g2d[1][3] = 8 */
    { u16 s = 0; u8 r, c; for (r = 0; r < 3; r++) for (c = 0; c < 4; c++) s += g2d[r][c]; EMIT_U16("2d_sum", s); }

    /* --- structs --- */
    EMIT_U16("st_y", pts[i2].y);
    EMIT_I8("st_z", pts[i1].z);
    { struct point *pp = &pts[i2]; EMIT_U8("stp_x", pp->x); EMIT_U16("stp_y", pp->y); EMIT_I8("stp_z", pp->z); }
    { u16 s = 0; for (i = 0; i < 3; i++) s += pts[i].y; EMIT_U16("aos_sum", s); }

    /* nested struct + struct assignment (member-wise copy codegen) */
    {
        struct line L;
        L.a = pts[i0];
        L.b = pts[i1];
        EMIT_U16("nest_ay", L.a.y);
        EMIT_I8("nest_bz", L.b.z);
    }
    {
        struct point t = pts[i0];          /* whole-struct copy */
        t.x = 99;
        EMIT_U8("copy_x", t.x);
        EMIT_U8("orig_x", pts[i0].x);      /* source unchanged */
        EMIT_U16("copy_y", t.y);
    }

    /* --- unions (little-endian punning, width-exact) --- */
    {
        union pun u;
        u.l = 0x12345678UL;
        EMIT_U8("pun_b0", u.b[0]); EMIT_U8("pun_b3", u.b[3]);
        EMIT_U16("pun_w0", u.w[0]); EMIT_U16("pun_w1", u.w[1]);
    }
    {
        union pun u;
        u.b[0] = 0xAA; u.b[1] = 0xBB; u.b[2] = 0xCC; u.b[3] = 0xDD;
        EMIT_U32("pun_l", u.l);
    }

    /* --- bitfields (near): logical read-back of each field --- */
    {
        struct bits bf;
        bf.a = 1; bf.b = 5; bf.c = 9; bf.wide = 0xABC; bf.s = -3;
        EMIT_U8("bf_a", bf.a); EMIT_U8("bf_b", bf.b); EMIT_U8("bf_c", bf.c);
        EMIT_U16("bf_wide", bf.wide); EMIT_I16("bf_s", bf.s);
        bf.b = 0;                          /* adjacent field must be untouched */
        EMIT_U8("bf_a2", bf.a); EMIT_U8("bf_c2", bf.c);
    }
}
