/* recursion — recursive and mutually-recursive calls, differential vs host.
 *
 * calls.c has one simple factorial; this stresses the call ABI under
 * self-reference, where the prime bug target is REGISTER/LOCAL PRESERVATION
 * ACROSS A RECURSIVE CALL:
 *   - tree (double) recursion: fib(n-1)'s result must survive the fib(n-2) call;
 *   - a frame with several live locals that must be intact after the callee
 *     (which freely clobbers caller registers) returns;
 *   - mutual recursion (is_even/is_odd cycle);
 *   - nested double recursion (Ackermann, bounded);
 *   - tail-style recursion with a modulo (gcd) and a pointer+accumulator (rsum).
 *
 * SOUNDNESS: deterministic value echoes, every result truncated to its emitted
 * width (harness.h); recursion depth is kept small so the target stack is fine.
 */
#include "harness.h"

/* tree (double) recursion: the fib(n-1) result must be preserved across the
   fib(n-2) call — the classic "value live across a clobbering call" test */
static u16 fib(u8 n) { return n < 2 ? n : (u16)(fib((u8)(n - 1)) + fib((u8)(n - 2))); }

/* mutual recursion */
static u8 is_odd(u8 n);
static u8 is_even(u8 n) { return n == 0 ? 1u : is_odd((u8)(n - 1)); }
static u8 is_odd(u8 n)  { return n == 0 ? 0u : is_even((u8)(n - 1)); }

/* Ackermann — nested double recursion (inner ack result feeds the outer call) */
static u16 ack(u8 m, u8 n)
{
    if (m == 0) return (u16)(n + 1);
    if (n == 0) return ack((u8)(m - 1), 1u);
    return ack((u8)(m - 1), (u8)ack(m, (u8)(n - 1)));
}

/* a frame with MULTIPLE live locals straddling the recursive call: a and b are
   computed before the call and combined after, so the callee must not corrupt
   them (they live in callee-saved regs or a spill slot) */
static u32 mix_rec(u8 n)
{
    u32 a, b, r;
    if (n == 0) return 0ul;
    a = (u32)n * 3u;
    b = (u32)n ^ 0xa5u;
    r = mix_rec((u8)(n - 1));      /* clobbers caller's scratch */
    return r + a - b;              /* a, b must be exactly as computed above */
}

/* gcd (Euclid) — tail-style recursion with a modulo */
static u16 gcd(u16 a, u16 b) { return b == 0 ? a : gcd(b, (u16)(a % b)); }

/* recursive array sum via pointer + accumulator (pointer-arg recursion) */
static u16 rsum(const u16 *p, u8 n, u16 acc) { return n == 0 ? acc : rsum(p + 1, (u8)(n - 1), (u16)(acc + *p)); }

static volatile u8  nn[8] = {0u, 1u, 2u, 5u, 8u, 10u, 12u, 14u};
static volatile u16 gp[5] = {7u, 36u, 0x1234u, 48u, 1000u};
static u16 ga[6] = {10u, 20u, 30u, 40u, 50u, 60u};

void diff_run(void)
{
    u8 i;

    for (i = 0; i < 8; i++) {
        u8 v = nn[i];
        EMIT_U16("fib",  fib((u8)(v & 15u)));
        EMIT_U8 ("even", is_even(v));
        EMIT_U8 ("odd",  is_odd(v));
        EMIT_U32("mixr", mix_rec((u8)(v & 7u)));
    }

    /* Ackermann — bounded SHALLOW on purpose. ack(3,2) and deeper recurse far
       enough to exhaust the runner's small RAM stack; the overflow corrupts data
       silently (no guard page) rather than crashing, so keep to ack(2,*)/ack(3,1)
       which fit. (Codegen is correct — this is a stack-depth resource limit, the
       same one a real PM ROM must respect.) */
    EMIT_U16("ack20", ack(2u, 0u));
    EMIT_U16("ack23", ack(2u, 3u));
    EMIT_U16("ack31", ack(3u, 1u));

    /* gcd over a few pairs */
    for (i = 0; i < 5; i++) {
        u16 a = gp[i];
        EMIT_U16("gcd", gcd(a, 36u));
    }

    /* recursive pointer+accumulator sum */
    EMIT_U16("rsum", rsum(ga, 6u, 0u));
}
