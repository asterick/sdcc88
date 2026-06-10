/* mixedwidth — integer promotion / conversion inside EXPRESSION TREES and
 * CHAINS, differential vs host.
 *
 * arith.c does single leaf casts; this nests conversions inside arithmetic and
 * chains them, where the bug target is sign-extend / zero-extend / narrow at each
 * tree node (not just a leaf) and the intermediate-width tracking between them.
 *
 * SOUNDNESS (load-bearing — host int is 32-bit, target int is 16-bit): every
 * operand is cast to an EXPLICIT width (i32/u32/i16/u16/i64/u64) so the
 * computation happens in the SAME width on both ends — we never rely on bare-int
 * promotion for a result that matters (that is the one legitimate host/target
 * difference). Every EMIT truncates to its declared width, and where a narrow op
 * (/, %, >>) is used the operands fit the width on both ends. char-width sums and
 * products truncated back to char are truncation-invariant, hence sound.
 */
#include "harness.h"

#define N 6
static volatile i8  i8v [N] = {0, 1, -1, 127, -128, 0x33};
static volatile u8  u8v [N] = {0u, 1u, 0x80u, 0xFFu, 0x7Fu, 0xC3u};
static volatile i16 i16v[N] = {0, 1, -1, 32767, -32768, 0x0123};
static volatile u16 u16v[N] = {0u, 1u, 0x8000u, 0xFFFFu, 0x1234u, 0xACE1u};
static volatile i32 i32v[N] = {0L, 1L, -1L, 2147483647L, (-2147483647L - 1), 0x01234567L};
static volatile u32 u32v[N] = {0ul, 1ul, 0x80000000ul, 0xFFFFFFFFul, 0x12345678ul, 0xDEADBEEFul};
static volatile u8  shv[N]  = {0u, 1u, 3u, 7u, 8u, 4u};

/* widen-then-combine: sign/zero extension feeding an arithmetic tree (all in an
   explicit wide type, so host & target agree) */
static void widen(i8 a, u8 b, i16 c, u16 d, i32 e)
{
    EMIT_I32("se_i8i16",  (i32)a + (i32)c);              /* sign-extend both */
    EMIT_U32("ze_u8u16",  (u32)b + (u32)d);              /* zero-extend both */
    EMIT_I32("tree_i32",  (i32)a * (i32)c - (i32)e);     /* 3-node tree, all i32 */
    EMIT_I64("tree_i64",  (i64)a * (i64)c + (i64)e);     /* widen to 64 mid-tree */
    EMIT_I32("mix_su",    (i32)a * (i32)b + (i32)c);     /* signed*unsigned, both -> i32 */
    EMIT_U32("shl_or",    ((u32)b << 8) | (u32)d);       /* zero-extend, shift, combine */
    EMIT_I32("nest",      (i32)((i16)((i32)c * (i32)c)) + (i32)a); /* mul, narrow to i16, re-widen */
}

/* narrowing chains: lossy multi-step casts */
static void narrow(i32 x, u32 y)
{
    EMIT_U8 ("n_32_8",   (u8)x);
    EMIT_I8 ("n_32_i8",  (i8)x);
    EMIT_U16("n_32_16",  (u16)y);
    EMIT_U8 ("chain_a",  (u8)(i16)(i32)x);               /* 32 -> 16 -> 8 */
    EMIT_I16("chain_b",  (i16)(i8)(i32)x);               /* 32 -> i8 narrow -> i16 sign-extend */
    EMIT_I32("chain_c",  (i32)(i8)(u8)(y));              /* low byte, signed reinterpret, widen */
    EMIT_U32("chain_d",  (u32)(u16)(i16)(i32)x);         /* down to 16 then zero-extend back to 32 */
}

/* conversion mid-tree under a shift (shift count variable) */
static void midtree(i16 a, i16 b, u8 sh)
{
    EMIT_I16("mt_narrow", (i16)((i32)a * (i32)b));       /* 32-bit product, keep low 16 */
    EMIT_I32("mt_sx",     (i32)(i16)((i32)a * (i32)b));  /* ...then sign-extend back to 32 */
    EMIT_U8 ("mt_u8",     (u8)((u16)a + (u16)b));        /* 16-bit add, low byte */
    EMIT_U32("mt_shift",  (u32)((u16)a) << (sh & 15u));  /* zero-extend, then a wide shift */
    EMIT_I32("mt_asr",    ((i32)a << 8) >> (sh & 7u));   /* widen, shift up, arithmetic down */
}

void diff_run(void)
{
    u8 i;
    for (i = 0; i < N; i++) {
        widen (i8v[i], u8v[i], i16v[i], u16v[i], i32v[i]);
        narrow(i32v[i], u32v[i]);
        midtree(i16v[i], i16v[(i + 1u) % N], shv[i]);
    }
}
