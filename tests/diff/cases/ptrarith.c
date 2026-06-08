/* ptrarith — pointer arithmetic & array indexing, differential vs host.
 *
 * Pointers ARE addresses, which differ host vs target, so we never emit a raw
 * pointer — only address-INDEPENDENT results: values read through a computed
 * pointer (a[i], *(p+n), p[-1], m[i][j], recs[i].f), pointer DIFFERENCES
 * (element counts), and pointer COMPARISONS (relative-order bools). That fully
 * exercises stride scaling and divide-by-element-size while staying sound.
 * Indices come from a volatile table so the arithmetic is real codegen, not
 * constant-folded.
 */
#include "harness.h"

#define N 6

static volatile u8  a8 [8] = { 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17 };
static volatile u16 a16[8] = { 0x1000,0x1100,0x1200,0x1300,0x1400,0x1500,0x1600,0x1700 };
static volatile u32 a32[8] = { 0x01000000ul,0x02000000ul,0x03000000ul,0x04000000ul,
                               0x05000000ul,0x06000000ul,0x07000000ul,0x08000000ul };
static volatile u16 m16[3][4] = { {1,2,3,4}, {5,6,7,8}, {9,10,11,12} };

struct odd3 { u8 x, y, z; };                 /* size 3 -> pointer diff divides by 3 (not a shift) */
static volatile struct odd3 recs[8] = {      /* 8 elems: every idx[] value (max 7) is in bounds */
    {0,1,2},{3,4,5},{6,7,8},{9,10,11},{12,13,14},{15,16,17},{18,19,20},{21,22,23}
};

__far const u16 fa[8] = { 0xF000,0xF100,0xF200,0xF300,0xF400,0xF500,0xF600,0xF700 };

static volatile u8 idx[N] = { 0u, 1u, 3u, 7u, 4u, 2u };

void diff_run(void)
{
    u8 i, j;

    /* indexed reads across element widths */
    for (i = 0; i < N; i++) {
        u8 k = idx[i];
        EMIT_U8 ("a8[k]",  a8[k]);
        EMIT_U16("a16[k]", a16[k]);
        EMIT_U32("a32[k]", a32[k]);
        EMIT_U16("*(a16+k)", *(a16 + k));
        if (k >= 1) {
            volatile u16 *p = a16 + k;       /* pointer + decrement index */
            EMIT_U16("p[-1]", p[-1]);
        }
    }

    /* pointer DIFFERENCES (element counts) and COMPARISONS (bools) */
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            u8 ki = idx[i], kj = idx[j];
            EMIT_I16("d8",  (i16)(&a8[kj]  - &a8[ki]));     /* /1 */
            EMIT_I16("d16", (i16)(&a16[kj] - &a16[ki]));    /* /2 (shift) */
            EMIT_I16("d32", (i16)(&a32[kj] - &a32[ki]));    /* /4 (shift) */
            EMIT_I16("drec",(i16)(&recs[kj] - &recs[ki]));  /* /3 (real divide) */
            EMIT_U8 ("eq",  &a32[ki] == &a32[kj]);
            /* NOTE: a RELATIONAL compare of two freshly address-of'd elements
               with runtime indices — `&a16[ki] < &a16[kj]` — is a known SILENT
               miscompile (genCmp native `cp pair,pair` drops the 2nd operand's
               address computation; see tests/emu/cases/16_ptrcmp.c + TODO #11).
               Pointer subtraction, equality, and the common `p < end` loop are
               all correct; only this inline double-address-of relational form is
               broken. Excluded here until fixed. */
        }

    /* multi-dim: value + flattened offset (= i*4 + j) */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++) {
            EMIT_U16("m[i][j]", m16[i][j]);
            EMIT_I16("m_off",   (i16)(&m16[i][j] - &m16[0][0]));
        }

    /* struct-array stride: field reads + a field-pointer walk */
    for (i = 0; i < N; i++) {
        u8 k = idx[i];
        EMIT_U8("recs.x", recs[k].x);
        EMIT_U8("recs.y", recs[k].y);
        EMIT_U8("recs.z", recs[k].z);
    }

    /* pointer walk with post-increment (sum of a slice) */
    {
        volatile u8 *p = a8;
        u16 sum = 0;
        for (i = 0; i < 5; i++)
            sum = (u16)(sum + *p++);
        EMIT_U16("walk_sum", sum);
    }

    /* __far pointer arithmetic: indexed far reads + far pointer difference */
    for (i = 0; i < N; i++) {
        u8 ki = idx[i], kj = idx[(u8)((i + 1) % N)];
        EMIT_U16("fa[k]",  fa[ki]);
        EMIT_I16("fdiff",  (i16)(&fa[kj] - &fa[ki]));
    }
}
