/* bitfields — packed-struct bit-field read/write, differential vs host.
 *
 * SOUNDNESS (load-bearing): C bit-field *placement* within a storage unit is
 * implementation-defined, so the host (gcc, LSB-first) and the target (sdcc)
 * may lay bits out differently. This test therefore tests only LAYOUT-INDEPENDENT
 * semantics — the VALUE that a field read/write produces:
 *   - reading an N-bit unsigned field truncates to N bits;
 *   - reading an N-bit signed field sign-extends from bit N-1;
 *   - writing a field stores only its low N bits and DOES NOT disturb neighbours
 *     (the read-modify-write mask is the prime bug target);
 *   - compound assignment / ++ on a field is a correct masked RMW.
 * All of those are standardized by C regardless of where the bits sit, so a
 * write-then-read-back round trip is sound. We DELIBERATELY never read the raw
 * storage (no union punning of a bit-field struct), which is the only thing that
 * would expose the differing layout.
 *
 * `int` bit-fields are capped at 16 bits (the target's int width) so the legal
 * field widths are width-exact on both sides.
 *
 * SIGNEDNESS (also load-bearing): the signedness of a PLAIN `int x : N` bit-field
 * is IMPLEMENTATION-DEFINED (C11 6.7.2/5) — the host (gcc) treats it as signed,
 * but sdcc treats it as UNSIGNED. So every field below is declared with EXPLICIT
 * `signed`/`unsigned` (never bare `int`/`i16`), which both compilers honour
 * identically; a bare-int field would diverge as IDB, not as a codegen bug.
 */
#include "harness.h"

#define N 8

/* unsigned fields of assorted widths; mid straddles the byte-0/byte-1 boundary
   on a tightly-packed LSB-first target, exercising multi-byte field codegen. */
struct uf {
    u16 lo  : 6;
    u16 mid : 5;
    u16 hi  : 5;
};

/* signed fields — read must sign-extend from the field's top bit. EXPLICIT
   `signed` (a bare-int field's signedness is implementation-defined; see header). */
struct sf {
    signed int a : 3;   /* range -4..3   */
    signed int b : 7;   /* range -64..63 */
    signed int c : 6;   /* range -32..31 */
};

/* byte-storage fields adjacent to a normal member, to prove the RMW of one
   field leaves both the neighbour field and the plain member intact. */
struct mixed {
    u8 flag  : 1;
    u8 small : 3;
    u8 nibble: 4;
    u8 plain;        /* ordinary member, must never be clobbered by field writes */
};

/* one-bit booleans packed together — adjacency masking stress */
struct bits8 {
    u8 b0:1; u8 b1:1; u8 b2:1; u8 b3:1; u8 b4:1; u8 b5:1; u8 b6:1; u8 b7:1;
};

static volatile u16 vin[N] = { 0u, 1u, 2u, 0x7Fu, 0xFFu, 0x1234u, 0xAAAAu, 0xFFFFu };

/* write each field independently, read every field back: truncation + that the
   neighbours are unchanged from their last write. */
static void uf_roundtrip(u16 x)
{
    struct uf s;
    s.lo = 0; s.mid = 0; s.hi = 0;
    s.lo = x;
    EMIT_U8("uf.lo",  s.lo);
    EMIT_U8("uf.mid0", s.mid);   /* neighbour still 0 */
    EMIT_U8("uf.hi0",  s.hi);
    s.mid = x;
    EMIT_U8("uf.mid", s.mid);
    EMIT_U8("uf.lo_keep", s.lo);  /* lo must survive writing mid */
    s.hi = x;
    EMIT_U8("uf.hi", s.hi);
    EMIT_U8("uf.lo_keep2", s.lo);
    EMIT_U8("uf.mid_keep", s.mid);
}

static void sf_roundtrip(u16 x)
{
    struct sf s;
    s.a = (i16)x;
    s.b = (i16)x;
    s.c = (i16)x;
    EMIT_I16("sf.a", s.a);   /* sign-extended */
    EMIT_I16("sf.b", s.b);
    EMIT_I16("sf.c", s.c);
    /* arithmetic on a signed field promotes to int then truncates back */
    EMIT_I16("sf.a+b", (i16)(s.a + s.b));
    EMIT_I16("sf.c*2", (i16)(s.c * 2));
}

static void mixed_rmw(u16 x)
{
    struct mixed m;
    m.flag = 0; m.small = 0; m.nibble = 0; m.plain = 0xC3;
    m.flag   = x;            /* truncates to 1 bit */
    m.small  = x;            /* 3 bits */
    m.nibble = x;            /* 4 bits */
    EMIT_U8("mx.flag",  m.flag);
    EMIT_U8("mx.small", m.small);
    EMIT_U8("mx.nibble", m.nibble);
    EMIT_U8("mx.plain", m.plain);   /* the field writes must not touch this */

    /* compound / increment RMW with overflow wraparound inside the field */
    m.small += (u8)x;
    EMIT_U8("mx.small+=", m.small);
    m.nibble++;
    EMIT_U8("mx.nibble++", m.nibble);
    m.flag ^= 1;
    EMIT_U8("mx.flag^", m.flag);
    EMIT_U8("mx.plain2", m.plain);  /* still intact after the RMWs */
}

/* set bit i from x's bit i; read each back — single-bit adjacency masking */
static void bits8_walk(u16 x)
{
    struct bits8 s;
    s.b0 = (x >> 0); s.b1 = (x >> 1); s.b2 = (x >> 2); s.b3 = (x >> 3);
    s.b4 = (x >> 4); s.b5 = (x >> 5); s.b6 = (x >> 6); s.b7 = (x >> 7);
    EMIT_U8("b.0", s.b0); EMIT_U8("b.1", s.b1); EMIT_U8("b.2", s.b2); EMIT_U8("b.3", s.b3);
    EMIT_U8("b.4", s.b4); EMIT_U8("b.5", s.b5); EMIT_U8("b.6", s.b6); EMIT_U8("b.7", s.b7);
    /* clear the even bits, leave odd — RMW that must not disturb the odd bits */
    s.b0 = 0; s.b2 = 0; s.b4 = 0; s.b6 = 0;
    EMIT_U8("b.1k", s.b1); EMIT_U8("b.3k", s.b3); EMIT_U8("b.5k", s.b5); EMIT_U8("b.7k", s.b7);
}

void diff_run(void)
{
    u8 i;
    for (i = 0; i < N; i++) {
        uf_roundtrip(vin[i]);
        sf_roundtrip(vin[i]);
        mixed_rmw(vin[i]);
        bits8_walk(vin[i]);
    }
}
