/* arith — integer arithmetic & edge cases (differential vs host).
 *
 * Covers all integer widths/signedness x {+ - * / % & | ^ ~ unary- ! shifts
 * same-type compares}, plus widening/narrowing casts. Operands are volatile
 * tables (no constant folding -> real codegen);
 * edge values (0, 1, -1, type min/max, sign boundaries, alternating bits) are
 * included on purpose. See harness.h SOUNDNESS for the truncation/compare rules.
 *
 * Per-iteration work lives in small straight-line helpers called from the loops:
 * it keeps loop bodies well under the jrs branch range, isolates failures, and
 * exercises the call ABI (8/16/32-bit args) as a bonus.
 */
#include "harness.h"

#define N    8        /* operand-table length */
#define NSH  6        /* shift-count-table length */

static volatile u8  u8v[N]  = {0u, 1u, 2u, 0x7fu, 0x80u, 0xffu, 0x55u, 10u};
static volatile i8  i8v[N]  = {0, 1, -1, 127, -128, 0x33, -50, 7};
static volatile u16 u16v[N] = {0u, 1u, 0x7fffu, 0x8000u, 0xffffu, 0x1234u, 0xabcdu, 100u};
static volatile i16 i16v[N] = {0, 1, -1, 32767, -32768, 0x0100, -0x0100, 1000};
static volatile u32 u32v[N] = {0ul, 1ul, 0xfffffffful, 0x80000000ul, 0x12345678ul, 0xdeadbeeful, 100000ul, 7ul};
static volatile i32 i32v[N] = {0, 1, -1, 2147483647L, (-2147483647L - 1), 0x12345678L, -100000L, 7L};

static volatile u8 shv[NSH]   = {0u, 1u, 4u, 7u, 8u, 15u};   /* 8/16-bit shift counts */
static volatile u8 shv32[NSH] = {0u, 1u, 8u, 16u, 24u, 31u}; /* 32-bit shift counts */

/* one (a,b) pair, all binary + (for the second arg) compare ops. DIVGUARD
   excludes /0 and the signed MIN/-1 overflow. */
#define BINOPS(TY, EMIT, MIN)                              \
    EMIT(#TY "+", a + b);                                  \
    EMIT(#TY "-", a - b);                                  \
    EMIT(#TY "*", a * b);                                  \
    if (b != 0 && !((a == (MIN)) && (b == (TY) - 1))) {   \
        EMIT(#TY "/", a / b);                              \
        EMIT(#TY "%", a % b);                              \
    }                                                      \
    EMIT(#TY "&", a & b);                                  \
    EMIT(#TY "|", a | b);                                  \
    EMIT(#TY "^", a ^ b);                                  \
    EMIT(#TY "<", a < b);                                  \
    EMIT(#TY ">", a > b);                                  \
    EMIT(#TY "<=", a <= b);                                \
    EMIT(#TY ">=", a >= b);                                \
    EMIT(#TY "==", a == b);                                \
    EMIT(#TY "!=", a != b)

static void bin_u8 (u8  a, u8  b) { BINOPS(u8,  EMIT_U8,  0u); }
static void bin_i8 (i8  a, i8  b) { BINOPS(i8,  EMIT_I8,  -128); }
static void bin_u16(u16 a, u16 b) { BINOPS(u16, EMIT_U16, 0u); }
static void bin_i16(i16 a, i16 b) { BINOPS(i16, EMIT_I16, -32768); }
static void bin_u32(u32 a, u32 b) { BINOPS(u32, EMIT_U32, 0ul); }
static void bin_i32(i32 a, i32 b) { BINOPS(i32, EMIT_I32, (-2147483647L - 1)); }

#define UNOPS(TY, EMIT)        \
    EMIT(#TY "~", ~a);         \
    EMIT(#TY "neg", -a);       \
    EMIT(#TY "!", !a)

static void un_u8 (u8  a) { UNOPS(u8,  EMIT_U8); }
static void un_i8 (i8  a) { UNOPS(i8,  EMIT_I8); }
static void un_u16(u16 a) { UNOPS(u16, EMIT_U16); }
static void un_i16(i16 a) { UNOPS(i16, EMIT_I16); }
static void un_u32(u32 a) { UNOPS(u32, EMIT_U32); }
static void un_i32(i32 a) { UNOPS(i32, EMIT_I32); }

/* shifts: unsigned both directions; signed right only (signed left is UB) */
static void sh_8_16(u8 a8, u16 a16, i16 s16, u8 c)
{
    EMIT_U8 ("u8<<",  a8  << c);
    EMIT_U8 ("u8>>",  a8  >> c);
    EMIT_U16("u16<<", a16 << c);
    EMIT_U16("u16>>", a16 >> c);
    EMIT_I16("i16>>", s16 >> c);   /* arithmetic shift */
}

static void sh_32(u32 a, i32 s, u8 c)
{
    EMIT_U32("u32<<", a << c);
    EMIT_U32("u32>>", a >> c);
    EMIT_I32("i32>>", s >> c);     /* arithmetic shift */
}

static void casts(u8 a8, i8 s8, u16 a16, i16 s16, u32 a32)
{
    EMIT_U16("u8>u16",  (u16)a8);
    EMIT_I16("i8>i16",  (i16)s8);    /* sign-extend */
    EMIT_U32("u8>u32",  (u32)a8);
    EMIT_I32("i8>i32",  (i32)s8);
    EMIT_I32("i16>i32", (i32)s16);   /* sign-extend */
    EMIT_U32("u16>u32", (u32)a16);
    EMIT_U8 ("u16>u8",  (u8)a16);    /* truncate */
    EMIT_U8 ("u32>u8",  (u8)a32);
    EMIT_U16("u32>u16", (u16)a32);
    EMIT_I8 ("i16>i8",  (i8)s16);    /* truncate */
}

/* 16x16 -> 32 widening multiply: explicitly widen the 16-bit operands to 32-bit
 * and multiply. Sound for the differential check because both operands are
 * widened, so the product always fits in 32 bits (host and target agree).
 *
 * On this port `has_mulint2long` is ON (main.c), so the middle end lowers a
 * 16-bit*16-bit->32 product to the dedicated __mulsint2slong / __muluint2ulong
 * widening routines (device/lib/s1c88/_mul{u,s}int2*long.c) instead of widening
 * both operands to 32 bits and calling __mullong. These cases exercise that path
 * across the value matrix below (incl. negatives and MIN/MAX edges). */
static void widemul(u16 a, u16 b, i16 s, i16 t)
{
    EMIT_U32("u16*u16>u32", (u32)a * (u32)b);   /* both operands cast */
    EMIT_I32("i16*i16>i32", (i32)s * (i32)t);   /* both cast, sign-extend */
    EMIT_U32("u16*Wu16",    a * (u32)b);        /* one cast; the other promotes */
    EMIT_I32("i16*Wi16",    s * (i32)t);
}

void diff_run(void)
{
    u8 i, j, k;

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            bin_u8 (u8v[i],  u8v[j]);
            bin_i8 (i8v[i],  i8v[j]);
            bin_u16(u16v[i], u16v[j]);
            bin_i16(i16v[i], i16v[j]);
            bin_u32(u32v[i], u32v[j]);
            bin_i32(i32v[i], i32v[j]);
            widemul(u16v[i], u16v[j], i16v[i], i16v[j]);
        }

    for (i = 0; i < N; i++) {
        un_u8 (u8v[i]);  un_i8 (i8v[i]);
        un_u16(u16v[i]); un_i16(i16v[i]);
        un_u32(u32v[i]); un_i32(i32v[i]);
    }

    for (i = 0; i < N; i++)
        for (k = 0; k < NSH; k++) {
            sh_8_16(u8v[i], u16v[i], i16v[i], shv[k]);
            sh_32(u32v[i], i32v[i], shv32[k]);
        }

    for (i = 0; i < N; i++)
        casts(u8v[i], i8v[i], u16v[i], i16v[i], u32v[i]);
}
