/* fnptr2 — function pointers with varied signatures, differential vs host.
 *
 * 06_fnptr covers basic int(int,int) dispatch and calls.c covers the DIRECT
 * call ABI; this exercises the INDIRECT call (PCALL through the 3-byte banked
 * pointer + __sdcc_fptr cell) with the richer signatures the direct path tests:
 * wide (u32) returns, struct returns (bigreturn hidden pointer THROUGH a fnptr),
 * stack-overflow arg lists, mixed-width args, a struct passed BY VALUE through a
 * fnptr (the genPointerPush register-stash path, via PCALL this time), pointers
 * held in arrays/structs and runtime-selected, and a fnptr-returning-fnptr.
 *
 * A `volatile` selector forces a real runtime dispatch (PCALL), so nothing folds
 * to a direct call. SOUNDNESS: same as calls.c — deterministic value echoes,
 * every result truncated to its emitted width; struct returns read named members
 * only (layout-independent, see structargs.c).
 */
#include "harness.h"

struct pt { u16 x, y; };
struct big { u32 lo, hi; };

/* --- wide return through a fnptr --- */
static u32 mul_u32(u32 a, u32 b) { return a * b; }
static u32 xor_u32(u32 a, u32 b) { return a ^ b; }
typedef u32 (*binop32)(u32, u32);

/* --- struct return through a fnptr (bigreturn hidden pointer + PCALL) --- */
static struct big mk_big(u32 a, u32 b) { struct big r; r.lo = a + b; r.hi = a ^ b; return r; }
static struct big rs_big(u32 a, u32 b) { struct big r; r.lo = a - b; r.hi = a | b; return r; }
typedef struct big (*bigfn)(u32, u32);

/* --- small struct return through a fnptr (fits registers, not bigreturn) --- */
static struct pt swap_pt(struct pt p) { struct pt r; r.x = p.y; r.y = p.x; return r; }
typedef struct pt (*ptfn)(struct pt);

/* --- many args through a fnptr (register set + stack overflow) --- */
static u16 sum7(u16 a, u16 b, u16 c, u16 d, u16 e, u16 f, u16 g)
{
    return (u16)(a + (u16)(b * 2) + (u16)(c * 3) + (u16)(d * 4) + (u16)(e * 5) + (u16)(f * 6) + (u16)(g * 7));
}
typedef u16 (*fn7)(u16, u16, u16, u16, u16, u16, u16);

/* --- mixed-width args through a fnptr --- */
static u32 mix(u8 a, u16 b, u32 c, u8 d) { return (u32)a + b + c + d; }
typedef u32 (*fnmix)(u8, u16, u32, u8);

/* --- struct BY VALUE through a fnptr (genPointerPush via PCALL) --- */
static u32 take_pt(struct pt p, u16 y) { return (u32)p.x + p.y + y; }
typedef u32 (*fntp)(struct pt, u16);

/* --- fnptr returning a fnptr --- */
static binop32 pick(u8 which) { return which ? xor_u32 : mul_u32; }
typedef binop32 (*fnret)(u8);

/* runtime selectors / tables (volatile => real PCALL) */
static volatile u8 sel = 1;
static binop32 tbl32[2] = { mul_u32, xor_u32 };
static bigfn   tblbig[2] = { mk_big, rs_big };
struct dispatch { ptfn sw; fn7 s7; };
static struct dispatch disp = { swap_pt, sum7 };

static volatile u32 a32[4] = { 0ul, 3ul, 0x12345678ul, 0xfffffffful };
static volatile u16 a16[4] = { 0u, 7u, 0x1234u, 0xffffu };
static volatile u8  a8[4]  = { 0u, 1u, 0x55u, 0xffu };

void diff_run(void)
{
    u8 i;

    for (i = 0; i < 4; i++) {
        u8 s = (u8)(i & 1);

        /* wide return via table-loaded fnptr */
        binop32 f32 = tbl32[s];
        EMIT_U32("op32", f32(a32[i], a32[(u8)(i + 1) & 3]));

        /* struct return (bigreturn) via fnptr */
        bigfn bf = tblbig[s];
        struct big rb = bf(a32[i], a32[(u8)(i + 2) & 3]);
        EMIT_U32("bgLo", rb.lo);
        EMIT_U32("bgHi", rb.hi);

        /* small struct return via fnptr */
        ptfn pf = disp.sw;
        struct pt p = { a16[i], a16[(u8)(i + 1) & 3] };
        struct pt rp = pf(p);
        EMIT_U16("swX", rp.x);
        EMIT_U16("swY", rp.y);

        /* many args (stack overflow) via fnptr */
        fn7 f7 = disp.s7;
        EMIT_U16("s7", f7(a16[i], a16[(u8)(3 - i)], a16[i], 1, 2, 3, 4));

        /* mixed-width args via fnptr */
        fnmix fm = (sel ? mix : (fnmix)0);
        EMIT_U32("mix", fm(a8[i], a16[i], a32[i], a8[(u8)(3 - i)]));

        /* struct by value through a fnptr (the register-stash push via PCALL) */
        fntp ft = (sel ? take_pt : (fntp)0);
        EMIT_U32("tp", ft(p, a16[i]));

        /* fnptr returning fnptr, then called */
        fnret fr = pick;
        binop32 chosen = fr(s);
        EMIT_U32("ret2", chosen(a32[i], 0x0F0F0F0FuL));
    }
}
