/* unions — union type-punning & overlapping-member access, differential vs host.
 *
 * Writing one member and reading an overlapping one reinterprets the bytes, so
 * this is endianness-exact: both host and target are little-endian with the same
 * widths (harness.h typedefs), so b[0] is the LSB on both. Covers width punning
 * (u32 <-> u16[2] <-> u8[4]), sign punning, struct-in-union / union-in-struct,
 * and a union passed/returned by value (the aggregate ABI).
 */
#include "harness.h"

#define N 6

union w32 { u32 u; i32 i; u16 h[2]; u8 b[4]; };
union w16 { u16 u; i16 i; u8 b[2]; };

struct s2 { u8 lo; u8 hi; };
union mix { u16 word; struct s2 parts; u8 raw[2]; };

/* an 8-byte union returned by value -> the bigreturn hidden-pointer ABI */
union w64 { u32 lo32[2]; u8 b[8]; };

static volatile u32 v32[N] = { 0u, 0xFFFFFFFFu, 0x12345678u, 0x80000000u, 0x0000FF00u, 0xDEADBEEFu };
static volatile u16 v16[N] = { 0u, 0xFFFFu, 0x1234u, 0x8000u, 0x00FFu, 0xABCDu };
static volatile u8  v8[4]  = { 0x11u, 0x22u, 0x33u, 0x44u };

/* write a 32-bit value, read every overlapping view (proves the LE byte layout) */
static void pun32(u32 x)
{
    union w32 w;
    w.u = x;
    EMIT_I32("w.i",  w.i);
    EMIT_U16("w.h0", w.h[0]);
    EMIT_U16("w.h1", w.h[1]);
    EMIT_U8 ("w.b0", w.b[0]);
    EMIT_U8 ("w.b1", w.b[1]);
    EMIT_U8 ("w.b2", w.b[2]);
    EMIT_U8 ("w.b3", w.b[3]);
    EMIT_I8 ("w.sb3", (i8)w.b[3]);   /* top byte read signed */
}

/* assemble a 32-bit value from bytes / halves through the union */
static void asm32(u16 a, u16 b)
{
    union w32 w;
    w.b[0] = v8[0]; w.b[1] = v8[1]; w.b[2] = v8[2]; w.b[3] = v8[3];
    EMIT_U32("bytes>u", w.u);
    w.h[0] = a; w.h[1] = b;
    EMIT_U32("halves>u", w.u);
}

static void pun16(u16 x)
{
    union w16 s;
    s.u = x;
    EMIT_I16("s.i",  s.i);
    EMIT_U8 ("s.b0", s.b[0]);
    EMIT_U8 ("s.b1", s.b[1]);
}

/* struct-in-union both directions */
static void mixed(u16 x)
{
    union mix m;
    m.word = x;
    EMIT_U8 ("m.lo",   m.parts.lo);
    EMIT_U8 ("m.hi",   m.parts.hi);
    EMIT_U8 ("m.raw0", m.raw[0]);
    m.parts.lo = v8[0]; m.parts.hi = v8[1];
    EMIT_U16("parts>word", m.word);
}

/* union by value: argument + return (8-byte union -> bigreturn) */
static union w64 mk64(u32 lo, u32 hi) { union w64 r; r.lo32[0] = lo; r.lo32[1] = hi; return r; }
static u32 lo_of(union w64 v) { return v.lo32[0]; }

void diff_run(void)
{
    u8 i, j;

    for (i = 0; i < N; i++) {
        pun32(v32[i]);
        pun16(v16[i]);
        mixed(v16[i]);
    }
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            asm32(v16[i], v16[j]);

    for (i = 0; i < N; i++) {
        union w64 w = mk64(v32[i], v32[(u8)(i + 1) % N]);
        EMIT_U32("w64.lo", w.lo32[0]);
        EMIT_U32("w64.hi", w.lo32[1]);
        EMIT_U8 ("w64.b3", w.b[3]);
        EMIT_U8 ("w64.b7", w.b[7]);
        EMIT_U32("w64.argret", lo_of(w));
    }
}
