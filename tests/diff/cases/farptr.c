/* farptr — __far (3-byte banked) pointer reads, writes, RMW, arithmetic, and
 * comparisons, differential vs host.
 *
 * emu 07_far is a single-shot execution check; this mines the far path
 * differentially across a value matrix. A far access stages the 24-bit page into
 * EP and the offset into HL and walks `(hl)` under the EP=0 invariant
 * (abi-decision.md "Task #9"); the read-modify-write forms (`fp[i] += v`,
 * `*fp <<= n`) are a far read, an ALU op, and a far store back — the densest far
 * codegen and the prime bug target here.
 *
 * SOUNDNESS: the harness #defines `__far` away on the host, so far objects are
 * plain little-endian memory there; values + layout match the located target
 * tables, so logical reads agree (harness.h). We only ever emit logical values —
 * far reads, write-then-read-backs, pointer DIFFERENCES (element counts, sizeof
 * cancels) and relative-order COMPARISONS — never a raw far pointer / page byte
 * (those legitimately differ: a host pointer vs the target's physical 0x10000+).
 * WRITABLE far scratch is always written before it is read, so its (uninit, ROM-
 * located) starting contents never matter. Runtime indices come from volatile
 * tables so nothing folds to a direct near access.
 */
#include "harness.h"

/* located const far ROM tables (reads) */
static const u8  __far fc[8] = {11u, 22u, 33u, 44u, 55u, 66u, 77u, 88u};
static const u16 __far fw[6] = {0x1111u, 0x2222u, 0x7fffu, 0x8000u, 0xffffu, 0x0123u};
static const u32 __far fl[4] = {1ul, 0x12345678ul, 0x80000000ul, 0xfffffffful};

/* writable far scratch (writes / RMW / read-back) */
static u8  __far wb[8];
static u16 __far ww[6];
static u32 __far wl[4];

static volatile u8 idx = 3;   /* runtime index (no folding) */

/* far RMW value sweep: edge seeds/operands through a far load->ALU->far store,
   read back each result. Exercises carry/borrow across the far store and the
   signed/width edges, not just one value per op. */
#define NS 5
static volatile u8  s8 [NS] = {1u, 0x7fu, 0x80u, 0xffu, 0x40u};
static volatile u8  r8 [NS] = {1u, 3u, 0x81u, 0xffu, 2u};
static volatile u16 s16[NS] = {1u, 0x7fffu, 0x8000u, 0xffffu, 0x1234u};
static volatile u16 r16[NS] = {1u, 3u, 0x8001u, 0xabcdu, 100u};
static volatile u32 s32[NS] = {1ul, 0x7ffffffful, 0x80000000ul, 0xfffffffful, 0x12345678ul};
static volatile u32 r32[NS] = {1ul, 3ul, 0x80000001ul, 0xdeadbeeful, 100000ul};

static void far_rmw_sweep(void)
{
    u8 i;
    u8  __far *p8  = wb;
    u16 __far *p16 = ww;
    u32 __far *p32 = wl;
    for (i = 0; i < NS; i++) {
        wb[idx] = s8[i];  p8[idx]  += r8[i];  EMIT_U8 ("s8+",  wb[idx]);
        wb[idx] = s8[i];  p8[idx]  ^= r8[i];  EMIT_U8 ("s8^",  wb[idx]);
        ww[2]   = s16[i]; p16[2]   += r16[i]; EMIT_U16("s16+", ww[2]);
        ww[2]   = s16[i]; p16[2]   |= r16[i]; EMIT_U16("s16|", ww[2]);
        wl[1]   = s32[i]; p32[1]   += r32[i]; EMIT_U32("s32+", wl[1]);
        wl[1]   = s32[i]; p32[1]   &= r32[i]; EMIT_U32("s32&", wl[1]);
    }
}

void diff_run(void)
{
    u8 i;

    /* --- far reads: deref, constant displacement, runtime index, walked sum --- */
    {
        const u8 __far *p = fc;
        EMIT_U8("fc0", *p);
        EMIT_U8("fc3", p[3]);
        EMIT_U8("fcr", fc[idx]);
        { u16 s = 0; for (i = 0; i < 8; i++) s += fc[i]; EMIT_U16("fcsum", s); }
    }
    { const u16 __far *p = fw; EMIT_U16("fw0", *p); EMIT_U16("fw1", p[1]); EMIT_U16("fwr", fw[idx % 6]); }
    { const u32 __far *p = fl; EMIT_U32("fl0", *p); EMIT_U32("fl1", p[1]); EMIT_U32("flr", fl[idx & 3]); }

    /* --- far pointer walk: ++ / += / -= then deref (24-bit linear arithmetic) --- */
    {
        const u8 __far *q = fc;
        q++;    EMIT_U8("w1", *q);   /* idx 1 */
        q += 4; EMIT_U8("w5", *q);   /* idx 5 */
        q -= 2; EMIT_U8("w3", *q);   /* idx 3 */
    }

    /* --- far pointer DIFFERENCE (element count; sizeof cancels host/target) --- */
    { const u16 __far *a = &fw[5], *b = &fw[1]; EMIT_I16("fwdiff", (i16)(a - b)); }
    { const u32 __far *a = &fl[3], *b = &fl[0]; EMIT_I16("fldiff", (i16)(a - b)); }

    /* --- far pointer COMPARISONS (relative-order bools) --- */
    {
        const u8 __far *a = &fc[2], *b = &fc[5];
        EMIT_U8("flt", a <  b); EMIT_U8("fgt", a >  b);
        EMIT_U8("feq", a == fc + 2); EMIT_U8("fne", a != b);
    }

    /* --- far WRITES + read-back --- */
    {
        u8 __far *p = wb;
        for (i = 0; i < 8; i++) p[i] = (u8)(fc[i] ^ 0x0f);   /* far store, walked */
        EMIT_U8("wb0", wb[0]); EMIT_U8("wbr", wb[idx]);
        { u16 s = 0; for (i = 0; i < 8; i++) s += wb[i]; EMIT_U16("wbsum", s); }
    }
    { u16 __far *p = ww; p[idx % 6] = 0xBEEFu; EMIT_U16("ww", ww[idx % 6]); }
    { u32 __far *p = wl; p[idx & 3] = 0xDEADBEEFul; EMIT_U32("wl", wl[idx & 3]); }

    /* --- far READ-MODIFY-WRITE (far load -> ALU -> far store) --- */
    {
        u8 __far *p = wb;
        wb[idx] = 0x10u; p[idx] += 5u;     EMIT_U8("r8+",  wb[idx]);
        wb[idx] = 0xF0u; p[idx] &= 0x3fu;  EMIT_U8("r8&",  wb[idx]);
        wb[idx] = 0x03u; p[idx] <<= 2;     EMIT_U8("r8<<", wb[idx]);
        wb[idx] = 0x7fu; p[idx]++;         EMIT_U8("r8++", wb[idx]);
    }
    {
        u16 __far *p = ww;
        ww[2] = 0x1000u; p[2] += 0x0234u;  EMIT_U16("r16+", ww[2]);
        ww[2] = 0x00ffu; p[2] ^= 0xff00u;  EMIT_U16("r16^", ww[2]);
    }
    {
        u32 __far *p = wl;
        wl[1] = 0x00000100ul; p[1] += 0x00019999ul; EMIT_U32("r32+", wl[1]);
        wl[1] = 0x0f0f0f0ful; p[1] |= 0xf0f0f0f0ul;  EMIT_U32("r32|", wl[1]);
    }

    far_rmw_sweep();
}
