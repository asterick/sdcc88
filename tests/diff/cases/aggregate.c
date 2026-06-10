/* aggregate — read-modify-write of struct/array-of-struct MEMBERS, plus chained
 * and embedded assignment (differential vs host).
 *
 * `memory` reads members and copies whole structs; `compound` does RMW on scalar
 * globals/arrays/pointers. The untested combination is RMW of an aggregate
 * MEMBER, where the lvalue is a base+offset (or indexed-base+offset) effective
 * address: `s.m += v`, `arr[i].m++`, `p->m |= k`, nested `L.a.m <<= n`. Every
 * member sits at a NON-zero offset (a leading pad field) so the offset
 * arithmetic is actually exercised, not folded to base+0.
 *
 * Also: chained assignment `a = b = c` (assignment is right-assoc and yields the
 * stored value) and assignment used as a subexpression.
 *
 * Per-type helpers keep each function small (SDCC codegen is superlinear in
 * basic-block size — see TODO #11). SOUNDNESS (harness.h): members use the
 * width-exact typedefs and only logical member values are emitted (never raw
 * struct bytes / sizeof / padding); RMW truncates to the member width; DIVGUARD
 * excludes /0 and signed MIN/-1.
 */
#include "harness.h"

#define N 5

static volatile u8  su8 [N] = {1u, 0x7fu, 0x80u, 0xffu, 0x40u};
static volatile u8  ru8 [N] = {1u, 3u, 0x81u, 0xffu, 2u};
static volatile u16 su16[N] = {1u, 0x7fffu, 0x8000u, 0xffffu, 0x1234u};
static volatile u16 ru16[N] = {1u, 3u, 0x8001u, 0xabcdu, 100u};
static volatile u32 su32[N] = {1ul, 0x7ffffffful, 0x80000000ul, 0xfffffffful, 0x12345678ul};
static volatile u32 ru32[N] = {1ul, 3ul, 0x80000001ul, 0xdeadbeeful, 100000ul};
static volatile i16 si16[N] = {1, -1, 32767, -32768, 0x0123};
static volatile i16 ri16[N] = {1, -1, 300, -300, 7};

static volatile u8 shc[N] = {0u, 1u, 3u, 7u, 4u};
static volatile u8 idx    = 2;

/* the arith RMW + a couple shifts + inc/dec, swept over one member lvalue LV. */
#define RMW(LV, EMIT, TY, MIN, SEED, RHS, CNT)                                \
    do {                                                                      \
        LV = (SEED); EMIT("+=",  (LV += (RHS)));                             \
        LV = (SEED); EMIT("-=",  (LV -= (RHS)));                             \
        LV = (SEED); EMIT("*=",  (LV *= (RHS)));                             \
        if ((RHS) != 0 && !((TY)(SEED) == (MIN) && (RHS) == (TY) - 1)) {     \
            LV = (SEED); EMIT("/=", (LV /= (RHS)));                          \
            LV = (SEED); EMIT("%=", (LV %= (RHS)));                          \
        }                                                                     \
        LV = (SEED); EMIT("&=",  (LV &= (RHS)));                             \
        LV = (SEED); EMIT("|=",  (LV |= (RHS)));                             \
        LV = (SEED); EMIT("^=",  (LV ^= (RHS)));                             \
        LV = (SEED); EMIT("<<=", (LV <<= (CNT)));                            \
        LV = (SEED); EMIT(">>=", (LV >>= (CNT)));                            \
        LV = (SEED); EMIT("++",  (++LV));                                    \
        LV = (SEED); EMIT("--",  (LV--));                                    \
    } while (0)

/* a leading pad puts the live member `m` at a non-zero offset; a tail member
   guards against an over-wide store clobbering the next field. */
#define DECLAGG(TY)                                                  \
    static struct S_##TY { u8 pad; TY m; i8 tail; } gs_##TY;         \
    static struct S_##TY ga_##TY[4];                                 \
    static struct Nn_##TY { struct S_##TY a, b; } gn_##TY

DECLAGG(u8); DECLAGG(u16); DECLAGG(u32); DECLAGG(i16);

#define DEFAGG(TY, EMIT, MIN)                                        \
    static void agg_##TY(TY seed, TY rhs, u8 cnt)                    \
    {                                                               \
        struct S_##TY *p = &gs_##TY;                                \
        RMW(gs_##TY.m,      EMIT, TY, MIN, seed, rhs, cnt);         \
        RMW(ga_##TY[idx].m, EMIT, TY, MIN, seed, rhs, cnt);         \
        RMW(p->m,           EMIT, TY, MIN, seed, rhs, cnt);         \
        RMW(gn_##TY.b.m,    EMIT, TY, MIN, seed, rhs, cnt);         \
        /* the tail field must never be disturbed by a member RMW */ \
        gs_##TY.tail = -7; gs_##TY.m = seed; gs_##TY.m += rhs;      \
        EMIT_I8("tail", gs_##TY.tail);                              \
    }

DEFAGG(u8,  EMIT_U8,  0u)
DEFAGG(u16, EMIT_U16, 0u)
DEFAGG(u32, EMIT_U32, 0ul)
DEFAGG(i16, EMIT_I16, -32768)

/* chained / embedded assignment: right-assoc, yields the stored value */
static u16 ca, cb, cc;
static void chained(u16 v)
{
    ca = cb = cc = v;                          /* triple chain */
    EMIT_U16("ch_a", ca); EMIT_U16("ch_b", cb); EMIT_U16("ch_c", cc);

    EMIT_U16("asn_val", (cb = (u16)(v + 1)));  /* assignment as a value */

    ga_u16[idx].m = gs_u16.m = v;              /* chained member assignment */
    EMIT_U16("ch_m1", ga_u16[idx].m); EMIT_U16("ch_m2", gs_u16.m);

    if ((cc = v) != 0)                          /* assignment in a condition */
        EMIT_U16("asn_cond", cc);
}

void diff_run(void)
{
    u8 i;
    for (i = 0; i < N; i++) {
        agg_u8 (su8[i],  ru8[i],  shc[i]);
        agg_u16(su16[i], ru16[i], shc[i]);
        agg_u32(su32[i], ru32[i], shc[i]);
        agg_i16(si16[i], ri16[i], shc[i]);
        chained(su16[i]);
    }
}
