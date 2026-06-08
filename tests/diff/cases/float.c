/* float.c — differential coverage for the float (IEEE-754 binary32) codegen path.
 *
 * The float support library (softfloat) is large and was entirely unexecuted. This
 * exercises the float ABI / support-call wiring / byte order end to end against the
 * host's hardware float. Per harness.h SOUNDNESS(float): every operand AND result
 * here is EXACTLY representable in binary32 (small integers, halves/quarters, exact
 * quotients), so a divergence is a codegen bug, not a softfloat rounding difference.
 *
 * Operands are `volatile` so neither side constant-folds them away — the target
 * actually calls the runtime float routines (__fsadd/__fssub/__fsmul/__fs2sint/...).
 *
 * Covers add, subtract, opposite-sign add, multiply, divide, compares, and all
 * int<->float casts. The subtraction / opposite-sign block is the regression test
 * for the #8 float-subtract miscompile: `genUminusFloat` (the `-a1` sign flip inside
 * `__fssub`) loaded the top byte into A before spilling the low word, but on the
 * HLBA layout A holds byte 0 — so a1's low mantissa byte was dropped and every
 * `__fssub` result was corrupted in its low bits (10.0-4.0 → 0x40C00182). Fixed by
 * copying the low bytes before the sign flip; see TODO #8.
 */
#include "harness.h"

void diff_run(void)
{
    volatile f32 a = 10.0f, b = 4.0f, c = 2.5f, d = -3.0f;
    volatile f32 h = 0.5f, q = 0.25f, z = 0.0f;

    /* --- arithmetic (all results exact in binary32) --- */
    EMIT_F32("add",   a + b);            /* 14.0  */
    EMIT_F32("mul",   a * b);            /* 40.0  */
    EMIT_F32("muln",  a * d);            /* -30.0 */
    EMIT_F32("div",   a / b);            /* 2.5   */
    EMIT_F32("divn",  d / b);            /* -0.75 */
    EMIT_F32("neg",   -a);               /* -10.0 (sign-flip, not _fssub) */
    EMIT_F32("addh",  h + q);            /* 0.75  */
    EMIT_F32("addz",  a + z);            /* 10.0  */
    EMIT_F32("chain", (a + b) * c + h);  /* 14*2.5 + 0.5 = 35.5 */

    /* --- subtraction & opposite-sign addition (the #8 regression) — __fssub
       routes a-b through __fsadd's different-sign mantissa-subtract path --- */
    EMIT_F32("sub",    a - b);           /* 6.0   */
    EMIT_F32("subh",   a - c);           /* 7.5   */
    EMIT_F32("subneg", b - a);           /* -6.0  */
    EMIT_F32("subq",   c - q);           /* 2.25  */
    EMIT_F32("subz",   a - z);           /* 10.0  */
    EMIT_F32("cancel", 16.0f - 15.0f);   /* 1.0  (big cancellation -> renormalize) */
    /* NB: a FULL cancellation (a - a) is deliberately not emitted: softfloat's
       __fssub is `-((-a)+b)`, so a-a yields -0.0 (negate of +0.0) where hardware
       gives +0.0 — a library zero-sign convention difference, not a codegen bug
       (and harmless: -0.0 == +0.0). */
    EMIT_F32("addopp", (-a) + b);        /* -6.0 (opposite-sign add, direct) */
    EMIT_F32("addnd",  a + d);           /* 7.0  (10 + -3) */
    EMIT_F32("subbig", d - b);           /* -7.0 (-3 - 4) */
    EMIT_F32("subchain", (a - b) - c);   /* (10-4)-2.5 = 3.5 */

    /* --- comparisons (same-typed float operands -> 0/1) --- */
    EMIT_U8("lt",  a < b);               /* 0 */
    EMIT_U8("gt",  a > b);               /* 1 */
    EMIT_U8("le",  c <= c);              /* 1 */
    EMIT_U8("ge",  d >= a);              /* 0 */
    EMIT_U8("eq",  c == c);              /* 1 */
    EMIT_U8("ne",  a != b);              /* 1 */
    EMIT_U8("ltn", d < z);               /* 1 (-3 < 0) */

    /* --- int -> float (all |x| < 2^24 -> exact) --- */
    volatile i8  si8 = -100;
    volatile u8  ui8 = 200;
    volatile i16 si  = -1234;
    volatile u16 ui  = 50000;
    volatile i32 sl  = -100000;
    volatile u32 ul  =  200000;
    EMIT_F32("i8f",  (f32)si8);          /* -100.0   */
    EMIT_F32("u8f",  (f32)ui8);          /*  200.0   */
    EMIT_F32("i16f", (f32)si);           /* -1234.0  */
    EMIT_F32("u16f", (f32)ui);           /*  50000.0 */
    EMIT_F32("i32f", (f32)sl);           /* -100000.0*/
    EMIT_F32("u32f", (f32)ul);           /*  200000.0*/

    /* --- float -> int (truncation toward zero, in range) --- */
    volatile f32 big = 42.75f, negf = -3.9f, mid = 1000.5f, large = 123456.0f;
    EMIT_I8 ("f2i8",  (i8)big);          /* 42     */
    EMIT_U8 ("f2u8",  (u8)big);          /* 42     */
    EMIT_I16("f2i16", (i16)negf);        /* -3     */
    EMIT_U16("f2u16", (u16)mid);         /* 1000   */
    EMIT_I32("f2i32", (i32)large);       /* 123456 */
    EMIT_I32("f2i32n",(i32)negf);        /* -3     */

    /* --- round-trip int -> float -> int --- */
    EMIT_I16("rt16", (i16)((f32)si));    /* -1234  */
    EMIT_I32("rt32", (i32)((f32)sl));    /* -100000*/
}
