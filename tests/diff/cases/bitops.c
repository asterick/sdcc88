/* bitops — bit-manipulation idioms, differential vs host.
 *
 * arith.c covers straight shifts; this covers the COMPOSED idioms real embedded
 * / Pokemon-Mini code uses, which lower to shift+mask+branch sequences:
 *   - rotate left/right (variable and width-edge counts), 8/16/32-bit
 *   - byte swap (16/32), nibble swap, 8-bit bit reversal
 *   - popcount, count-leading / count-trailing zeros, parity
 *   - N-bit sign extension via shift pair
 *   - isolate / clear lowest set bit (x&-x, x&(x-1)), saturating add/sub
 *
 * SOUNDNESS: every EMIT truncates to its declared width; the composed ops are
 * the truncation-invariant ones (<<,>>,&,|,^,+,-) or stay within the width, and
 * all compares are same-typed (harness.h). Shift counts stay < width.
 */
#include "harness.h"

#define N 6
static volatile u8  v8 [N] = {0x00u, 0x01u, 0x80u, 0xFFu, 0x5Au, 0xC3u};
static volatile u16 v16[N] = {0x0000u, 0x0001u, 0x8000u, 0xFFFFu, 0x1234u, 0xACE1u};
static volatile u32 v32[N] = {0ul, 1ul, 0x80000000ul, 0xFFFFFFFFul, 0x12345678ul, 0xDEADBEEFul};
static volatile u8 c8 [N] = {0u, 1u, 3u, 7u, 4u, 6u};   /* rotate/shift counts < 8  */
static volatile u8 c16[N] = {0u, 1u, 7u, 15u, 8u, 11u}; /* < 16 */
static volatile u8 c32[N] = {0u, 1u, 15u, 31u, 16u, 23u}; /* < 32 */

/* --- rotates (n kept in [0,width-1]; the >> half shifts the promoted int) --- */
static u8  rotl8 (u8  x, u8 n) { return (u8) ((x << n) | (x >> ((8u  - n) & 7u))); }
static u8  rotr8 (u8  x, u8 n) { return (u8) ((x >> n) | (x << ((8u  - n) & 7u))); }
static u16 rotl16(u16 x, u8 n) { return (u16)((x << n) | (x >> ((16u - n) & 15u))); }
static u16 rotr16(u16 x, u8 n) { return (u16)((x >> n) | (x << ((16u - n) & 15u))); }
static u32 rotl32(u32 x, u8 n) { return (n == 0) ? x : (u32)((x << n) | (x >> (32u - n))); }
static u32 rotr32(u32 x, u8 n) { return (n == 0) ? x : (u32)((x >> n) | (x << (32u - n))); }

/* --- swaps / reversal --- */
static u16 bswap16(u16 x) { return (u16)((x >> 8) | (x << 8)); }
static u32 bswap32(u32 x) { return (u32)((x >> 24) | ((x & 0x00FF0000ul) >> 8) | ((x & 0x0000FF00ul) << 8) | (x << 24)); }
static u8  nibswap(u8 x)  { return (u8)((x >> 4) | (x << 4)); }
static u8  revbits8(u8 x)
{
    x = (u8)(((x & 0xF0u) >> 4) | ((x & 0x0Fu) << 4));
    x = (u8)(((x & 0xCCu) >> 2) | ((x & 0x33u) << 2));
    x = (u8)(((x & 0xAAu) >> 1) | ((x & 0x55u) << 1));
    return x;
}

/* --- population / zero counts / parity (loop forms; width-bounded) --- */
static u8 popc32(u32 x) { u8 n = 0; while (x) { n = (u8)(n + (u8)(x & 1u)); x >>= 1; } return n; }
static u8 clz16(u16 x)  { u8 n = 0; if (!x) return 16u; while (!(x & 0x8000u)) { n++; x = (u16)(x << 1); } return n; }
static u8 ctz16(u16 x)  { u8 n = 0; if (!x) return 16u; while (!(x & 1u)) { n++; x = (u16)(x >> 1); } return n; }
static u8 parity8(u8 x) { x ^= (u8)(x >> 4); x ^= (u8)(x >> 2); x ^= (u8)(x >> 1); return (u8)(x & 1u); }

/* --- N-bit sign extension via a shift pair (arith >> on i16 is sound) --- */
static i16 sext(u16 v, u8 bits) { u8 s = (u8)(16u - bits); return (i16)((i16)(v << s) >> s); }

/* --- lowest-set-bit tricks + saturating add/sub --- */
static u16 lsb_isolate(u16 x) { return (u16)(x & (u16)(0u - x)); }
static u16 lsb_clear  (u16 x) { return (u16)(x & (u16)(x - 1u)); }
static u8  sat_add8(u8 a, u8 b) { u8 r = (u8)(a + b); return (r < a) ? 0xFFu : r; }
static u8  sat_sub8(u8 a, u8 b) { return (a < b) ? 0u : (u8)(a - b); }

void diff_run(void)
{
    u8 i;

    for (i = 0; i < N; i++) {
        EMIT_U8 ("rotl8",  rotl8 (v8[i],  c8[i]));
        EMIT_U8 ("rotr8",  rotr8 (v8[i],  c8[i]));
        EMIT_U16("rotl16", rotl16(v16[i], c16[i]));
        EMIT_U16("rotr16", rotr16(v16[i], c16[i]));
        EMIT_U32("rotl32", rotl32(v32[i], c32[i]));
        EMIT_U32("rotr32", rotr32(v32[i], c32[i]));

        EMIT_U16("bswap16", bswap16(v16[i]));
        EMIT_U32("bswap32", bswap32(v32[i]));
        EMIT_U8 ("nibswap", nibswap(v8[i]));
        EMIT_U8 ("revbits", revbits8(v8[i]));

        EMIT_U8("popc32", popc32(v32[i]));
        EMIT_U8("clz16",  clz16(v16[i]));
        EMIT_U8("ctz16",  ctz16(v16[i]));
        EMIT_U8("parity", parity8(v8[i]));

        EMIT_I16("sext5",  sext(v16[i], 5u));
        EMIT_I16("sext12", sext(v16[i], 12u));

        EMIT_U16("lsb_iso", lsb_isolate(v16[i]));
        EMIT_U16("lsb_clr", lsb_clear(v16[i]));
        EMIT_U8 ("satadd",  sat_add8(v8[i], c8[i]));
        EMIT_U8 ("satsub",  sat_sub8(v8[i], c8[i]));
    }
}
