/* longshift — VARIABLE-count shifts over the FULL count range + rotates +
 * long-division edge values (differential vs host).
 *
 * Complements arith.c (samples 6 shift counts) and longlong.c (samples 8):
 * those would miss a runtime shift-helper bug that only shows at a specific
 * count, because the helpers decompose a shift into whole-byte moves + a
 * residual bit-shift — so counts on the byte/bit boundaries (7, 9, 15, 17, 23,
 * 25, …) exercise distinct paths. Here every count 0..width-1 is tested.
 *
 * The value and count are passed through a helper FUNCTION (count = a parameter)
 * here, which exercises the register/stack-operand shift path at every count.
 * The complementary INLINE form (`mem_u32 << mem_count`, the IY-counter path) was
 * the #11-longshift-iy miscompile — now fixed — and is covered directly by
 * `longshift_iy.c`.
 *
 * SOUNDNESS (see harness.h): the shift count must be < the PROMOTED operand width
 * (int is 16-bit on the S1C88 → 8/16-bit shift by 0..15, 32-bit by 0..31, 64-bit
 * by 0..63; >= width is UB). Signed left shift of a negative is UB → only
 * unsigned operands are left-shifted; signed right is arithmetic and tested.
 * Division excludes /0 and the signed MIN/-1 overflow (DIVGUARD).
 */
#include "harness.h"

static volatile u16 u16v[] = { 0x0000u, 0x0001u, 0xFFFFu, 0x8000u, 0x1234u, 0xAAAAu, 0x00FFu, 0xFF00u };
static volatile i16 i16v[] = { 0, 1, -1, 32767, -32768, 0x1234, -256, 128 };
static volatile u32 u32v[] = { 0u, 1u, 0xFFFFFFFFu, 0x80000000u, 0x12345678u, 0xAAAAAAAAu, 0x0000FFFFu, 0xFFFF0000u, 0xDEADBEEFu };
static volatile i32 i32v[] = { 0, 1, -1, 2147483647L, (-2147483647L - 1), 0x12345678L, -65536L, 0x40000000L };
static volatile u64 u64v[] = { 0ull, 1ull, ~0ull, 0x8000000000000000ull, 0x0123456789ABCDEFull, 0x00000000FFFFFFFFull };
static volatile i64 i64v[] = { 0ll, 1ll, -1ll, 0x7FFFFFFFFFFFFFFFll, (-0x7FFFFFFFFFFFFFFFll - 1), 0x0123456789ABCDEFll };

#define N(a) (u8)(sizeof(a) / sizeof((a)[0]))

/* count is a PARAMETER (see header note) — variable shift, clean operand. */
static void sh16 (u16 a, i16 s, u8 c)
{
    EMIT_U16 ("u16<<", (u16)(a << c));
    EMIT_U16 ("u16>>", (u16)(a >> c));
    EMIT_I16 ("i16>>", (i16)(s >> c));          /* arithmetic */
}

static void sh32 (u32 a, i32 s, u8 c)
{
    EMIT_U32 ("u32<<", a << c);
    EMIT_U32 ("u32>>", a >> c);
    EMIT_I32 ("i32>>", s >> c);                 /* arithmetic */
    if (c)                                      /* rotl by 0 would be x>>32 (UB) */
        EMIT_U32 ("rotl32", (a << c) | (a >> (32 - c)));
}

static void sh64 (u64 a, i64 s, u8 c)
{
    EMIT_U64 ("u64<<", a << c);
    EMIT_U64 ("u64>>", a >> c);
    EMIT_I64 ("i64>>", s >> c);                 /* arithmetic */
}

void diff_run (void)
{
    u8 i, c;

    for (i = 0; i < N(u16v); i++)               /* 16-bit: every count 0..15 */
        for (c = 0; c < 16; c++)
            sh16 (u16v[i], i16v[i], c);

    for (i = 0; i < N(u32v); i++)               /* 32-bit: every count 0..31 (+ rotl) */
        for (c = 0; c < 32; c++)
            sh32 (u32v[i], i32v[i], c);

    for (i = 0; i < N(u64v); i++)               /* 64-bit: every count 0..63 */
        for (c = 0; c < 64; c++)
            sh64 (u64v[i], i64v[i], c);

    /* ---- long division / modulo, edge dividends x edge divisors ---- */
    {
        static volatile u32 du[] = { 0u, 1u, 2u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x12345678u, 1000000u };
        static volatile i32 ds[] = { 1, -1, 2, -2, 2147483647L, (-2147483647L - 1), 3, -1000000L };
        u8 a, b;

        for (a = 0; a < N(du); a++)
            for (b = 0; b < N(du); b++)
                if (du[b] != 0u) {
                    EMIT_U32 ("u32/", du[a] / du[b]);
                    EMIT_U32 ("u32%", du[a] % du[b]);
                }

        for (a = 0; a < N(ds); a++)
            for (b = 0; b < N(ds); b++)
                if (ds[b] != 0 && !(ds[a] == (i32)0x80000000L && ds[b] == -1)) {
                    EMIT_I32 ("i32/", ds[a] / ds[b]);
                    EMIT_I32 ("i32%", ds[a] % ds[b]);
                }
    }
}
