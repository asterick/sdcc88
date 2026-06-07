/* calls — function-call ABI (differential vs host).
 *
 * Argument passing (register + stack overflow, mixed widths), return values of
 * each width, recursion, and — as a regression for the bug the control module
 * surfaced — a `char` argument passed alongside a stacked `long` argument under
 * register pressure (the long's BA-staged push must not clobber the char arg).
 * The callee echoes what it received, so any mis-passed argument diverges.
 */
#include "harness.h"

/* --- regression: f(ptr, long, char). The char `n` must survive the staging of
   the stacked long `v`. Trigger the high-pressure path with a value computed in
   a nested loop (mirrors what first exposed the bug). --- */
static void take_lc(const char *tag, u32 v, u8 n)
{
    diff_tag(tag);
    diff_hex2((u8)(v >> 8)); diff_hex2((u8)v);   /* low 16 bits of v */
    diff_hex2(n);                                /* the char arg under test */
    diff_nl();
}

static void pressure_lc(u8 a, u8 b)
{
    u16 c = 0;
    u8 i, j;
    for (i = 0; i < a; i++)
        for (j = 0; j < b; j++)
            c++;
    take_lc("lc", (u32)c, 0x2a);                 /* n = 0x2a must arrive intact */
}

/* --- many args: register set + stack overflow --- */
static u16 sum7(u16 a, u16 b, u16 c, u16 d, u16 e, u16 f, u16 g)
{
    return (u16)(a + (u16)(b * 2) + (u16)(c * 3) + (u16)(d * 4) + (u16)(e * 5) + (u16)(f * 6) + (u16)(g * 7));
}

/* --- mixed-width args --- */
static u32 mix(u8 a, u16 b, u32 c, u8 d)
{
    return (u32)a + (u32)b + c + (u32)d;
}

/* --- returns of each width --- */
static u8  ret_u8 (u8 x)  { return (u8)(x ^ 0x5a); }
static u16 ret_u16(u16 x) { return (u16)(x + 0x1234); }
static u32 ret_u32(u32 x) { return x ^ 0xdeadbeefUL; }

/* --- recursion --- */
static u16 fact(u8 n) { return (n < 2) ? 1u : (u16)(n * fact((u8)(n - 1))); }

static volatile u8  q8[6]  = {0, 1, 2, 5, 10, 200};
static volatile u16 q16[6] = {0, 1, 100, 0x1234, 0x8000, 0xffff};
static volatile u32 q32[4] = {0ul, 1ul, 0x12345678ul, 0xfffffffful};

void diff_run(void)
{
    u8 i, j;

    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            pressure_lc((u8)(q8[i] & 7), (u8)(q8[j] & 7));

    for (i = 0; i < 6; i++) {
        EMIT_U16("sum7", sum7(q16[i], q16[5 - i], q16[i], 1, 2, 3, 4));
        EMIT_U32("mix",  mix(q8[i], q16[i], q32[i & 3], q8[5 - i]));
        EMIT_U8 ("ru8",  ret_u8(q8[i]));
        EMIT_U16("ru16", ret_u16(q16[i]));
        EMIT_U32("ru32", ret_u32(q32[i & 3]));
        EMIT_U16("fact", fact((u8)(q8[i] & 7)));
    }
}
