/* compound — read-modify-write codegen: compound assignment & inc/dec across
 * every width/signedness AND every addressing mode (differential vs host).
 *
 * The `arith` case only ever computes `a OP b` into a fresh result. This case
 * exercises the distinct codegen of an in-place READ-MODIFY-WRITE of a memory
 * lvalue — `lv OP= rhs`, `++lv`, `lv--` — where the lvalue is reached three
 * different ways, each a different S1C88 addressing mode:
 *   - a plain global         (direct)
 *   - an array element, idx   (indexed / computed effective address)
 *   - through a pointer       (indirect, `(hl)`)
 * Seeds and right-hand operands come from volatile tables so nothing is folded.
 *
 * Per-type work lives in small straight-line helpers called from the loop (as in
 * `arith`): it keeps each function small (SDCC's codegen is superlinear in basic-
 * block size — one giant inlined diff_run would be pathologically slow to
 * compile), keeps loop bodies under the jrs branch range, and isolates failures.
 *
 * SOUNDNESS: the lvalue has the declared width, so the RMW truncates to that
 * width identically on host and target (see harness.h). DIVGUARD excludes /0 and
 * the signed MIN/-1 overflow; shift counts are kept below the width.
 */
#include "harness.h"

#define N 5   /* (seed,rhs) pair-table length */

static volatile u8  su8 [N] = {1u, 0x7fu, 0x80u, 0xffu, 0x40u};
static volatile u8  ru8 [N] = {1u, 3u, 0x81u, 0xffu, 2u};
static volatile i8  si8 [N] = {1, -1, 127, -128, 0x33};
static volatile i8  ri8 [N] = {1, -1, 3, 50, -7};
static volatile u16 su16[N] = {1u, 0x7fffu, 0x8000u, 0xffffu, 0x1234u};
static volatile u16 ru16[N] = {1u, 3u, 0x8001u, 0xabcdu, 100u};
static volatile i16 si16[N] = {1, -1, 32767, -32768, 0x0123};
static volatile i16 ri16[N] = {1, -1, 300, -300, 7};
static volatile u32 su32[N] = {1ul, 0x7ffffffful, 0x80000000ul, 0xfffffffful, 0x12345678ul};
static volatile u32 ru32[N] = {1ul, 3ul, 0x80000001ul, 0xdeadbeeful, 100000ul};
static volatile i32 si32[N] = {1L, -1L, 2147483647L, (-2147483647L - 1), 0x01234567L};
static volatile i32 ri32[N] = {1L, -1L, 1000L, -1000L, 7L};

static volatile u8 shc[N] = {0u, 1u, 3u, 7u, 4u};   /* shift counts (< 8, safe for all widths) */
static volatile u8 idx    = 2;                       /* runtime array index */

/* the arith RMW ops, swept over one lvalue expression LV (reset to SEED each
   time so every op sees the same input). TY/MIN gate the signed-overflow case. */
#define RMW(LV, EMIT, TY, MIN, SEED, RHS)                                     \
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
    } while (0)

/* shift RMW + pre/post inc & dec, swept over LV (the value-returning forms). */
#define RMW2(LV, EMIT, SEED, CNT)                          \
    do {                                                   \
        LV = (SEED); EMIT("<<=", (LV <<= (CNT)));          \
        LV = (SEED); EMIT(">>=", (LV >>= (CNT)));          \
        LV = (SEED); EMIT("pre++",  (++LV));               \
        LV = (SEED); EMIT("pre--",  (--LV));               \
        LV = (SEED); EMIT("post++", (LV++));               \
        LV = (SEED); EMIT("post--", (LV--));               \
        LV = (SEED); (void)(LV++); EMIT("post++val", LV);  \
    } while (0)

/* per-type globals (direct) and arrays (indexed); a pointer gives indirect */
#define DECL(TY) static TY g_##TY; static TY a_##TY[4]
DECL(u8);  DECL(i8);  DECL(u16); DECL(i16); DECL(u32); DECL(i32);

/* one per-type helper: every op-class over every addressing mode. Small body. */
#define DEFHELPER(TY, EMIT, MIN)                                       \
    static void rmw_##TY(TY seed, TY rhs, u8 cnt)                      \
    {                                                                 \
        TY *p = &g_##TY;                                              \
        RMW(g_##TY,      EMIT, TY, MIN, seed, rhs);                   \
        RMW(a_##TY[idx], EMIT, TY, MIN, seed, rhs);                   \
        RMW(*p,          EMIT, TY, MIN, seed, rhs);                   \
        RMW2(g_##TY,      EMIT, seed, cnt);  /* shift/inc-dec: direct + */ \
        RMW2(*p,          EMIT, seed, cnt);  /* indirect (arith covers all 3) */ \
    }
DEFHELPER(u8,  EMIT_U8,  0u)
DEFHELPER(i8,  EMIT_I8,  -128)
DEFHELPER(u16, EMIT_U16, 0u)
DEFHELPER(i16, EMIT_I16, -32768)
DEFHELPER(u32, EMIT_U32, 0ul)
DEFHELPER(i32, EMIT_I32, (-2147483647L - 1))

void diff_run(void)
{
    u8 i;
    for (i = 0; i < N; i++) {
        rmw_u8 (su8[i],  ru8[i],  shc[i]);
        rmw_i8 (si8[i],  ri8[i],  shc[i]);
        rmw_u16(su16[i], ru16[i], shc[i]);
        rmw_i16(si16[i], ri16[i], shc[i]);
        rmw_u32(su32[i], ru32[i], shc[i]);
        rmw_i32(si32[i], ri32[i], shc[i]);
    }
}
