/* longlong — 64-bit (long long) integer arithmetic, differential vs host.
 *
 * The widest integer type, signed + unsigned: + - * / % & | ^ ~ unary- !
 * shifts and same-type compares, with carry/borrow-across-8-bytes edge values
 * (0, 1, -1, type min/max, the 32-bit boundary, alternating nibbles).
 * *, /, %, and the shifts route through the __*longlong support calls; +, -,
 * &, |, ^, ~ are inline 8-byte chains. See harness.h SOUNDNESS.
 */
#include "harness.h"

#define N 6

static volatile u64 u64v[N] = {
    0ull, 1ull, 0xFFFFFFFFFFFFFFFFull, 0x8000000000000000ull,
    0x0123456789ABCDEFull, 0x00000000FFFFFFFFull   /* straddles the 32-bit half */
};
static volatile i64 i64v[N] = {
    0ll, 1ll, -1ll, 0x7FFFFFFFFFFFFFFFll,
    (-0x7FFFFFFFFFFFFFFFll - 1) /* INT64_MIN */, 0x0123456789ABCDEFll
};
static volatile u8 shv[8] = { 0u, 1u, 7u, 31u, 32u, 33u, 48u, 63u };  /* incl. the 32-bit boundary */

/* one (a,b) pair, all binary + compare ops. DIVGUARD excludes /0 and the
   signed MIN/-1 overflow (same as arith.c). */
#define LLBINOPS(TY, EMIT, MIN)                        \
    EMIT(#TY "+", a + b);                              \
    EMIT(#TY "-", a - b);                              \
    EMIT(#TY "*", a * b);                              \
    if (b != 0 && !((a == (MIN)) && (b == (TY) - 1))) {\
        EMIT(#TY "/", a / b);                          \
        EMIT(#TY "%", a % b);                          \
    }                                                  \
    EMIT(#TY "&", a & b);                              \
    EMIT(#TY "|", a | b);                              \
    EMIT(#TY "^", a ^ b);                              \
    EMIT(#TY "<", a < b);                              \
    EMIT(#TY ">", a > b);                              \
    EMIT(#TY "<=", a <= b);                            \
    EMIT(#TY ">=", a >= b);                            \
    EMIT(#TY "==", a == b);                            \
    EMIT(#TY "!=", a != b)

static void bin_u64(u64 a, u64 b) { LLBINOPS(u64, EMIT_U64, 0ull); }
static void bin_i64(i64 a, i64 b) { LLBINOPS(i64, EMIT_I64, (-0x7FFFFFFFFFFFFFFFll - 1)); }

static void un_u64(u64 a) { EMIT_U64("u64~", ~a); EMIT_U64("u64neg", -a); EMIT_U8("u64!", !a); }
static void un_i64(i64 a) { EMIT_I64("i64~", ~a); EMIT_I64("i64neg", -a); EMIT_U8("i64!", !a); }

/* shifts: unsigned both directions; signed right only (signed left is UB) */
static void sh_64(u64 a, i64 s, u8 c)
{
    EMIT_U64("u64<<", a << c);
    EMIT_U64("u64>>", a >> c);
    EMIT_I64("i64>>", s >> c);   /* arithmetic shift */
}

/* 64-bit casts (a distinct codegen path): widen narrow->64 (sign/zero extend)
   and narrow 64->narrow (truncate). Reads the volatile tables by index so no
   mixed-width args ride the call boundary. */
static void llcasts(u8 i)
{
    i64 s = i64v[i];
    u64 a = u64v[i];
    EMIT_I64("i8>i64",  (i64)(i8)i64v[i]);   /* sign-extend low byte to 64 */
    EMIT_I64("i16>i64", (i64)(i16)i64v[i]);
    EMIT_I64("i32>i64", (i64)(i32)i64v[i]);
    EMIT_U64("u32>u64", (u64)(u32)u64v[i]);  /* zero-extend low 32 to 64 */
    EMIT_U32("u64>u32", (u32)a);             /* truncate */
    EMIT_U16("u64>u16", (u16)a);
    EMIT_U8 ("u64>u8",  (u8)a);
    EMIT_I32("i64>i32", (i32)s);
    EMIT_I16("i64>i16", (i16)s);
    EMIT_I8 ("i64>i8",  (i8)s);
}

/* 64-bit RETURN-value ABI (distinct from arg passing): the helper returns an
   i64/u64 the caller must marshal back. */
static i64 ll_smul(i64 x, i64 y) { return x * y; }
static u64 ll_ushr(u64 x, u8 c)  { return x >> c; }

void diff_run(void)
{
    u8 i, j, k;

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) {
            bin_u64(u64v[i], u64v[j]);
            bin_i64(i64v[i], i64v[j]);
        }

    for (i = 0; i < N; i++) {
        un_u64(u64v[i]);
        un_i64(i64v[i]);
    }

    for (i = 0; i < N; i++)
        for (k = 0; k < 8; k++)
            sh_64(u64v[i], i64v[i], shv[k]);

    for (i = 0; i < N; i++)
        llcasts(i);

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            EMIT_I64("ll_ret*", ll_smul(i64v[i], i64v[j]));   /* i64 return */
    for (i = 0; i < N; i++)
        for (k = 0; k < 8; k++)
            EMIT_U64("ll_ret>>", ll_ushr(u64v[i], shv[k]));   /* u64 return */
}
