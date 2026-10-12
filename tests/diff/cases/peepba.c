/* peepba — 16-bit ALU ops whose SECOND operand is computed into HL right
 * before a `sub/add/adc/sbc ba, hl`.
 *
 * The peephole's read model (s1c88MightRead) parsed the first operand of the
 * 8-bit/16-bit ALU forms (`a,`/`hl,`/`sp,`) and then scanned the rest — but had
 * no `ba,` arm, so `sub ba, hl` was scanned as "ba" only, HL was deemed unread,
 * and the load that produced it was deleted as dead: `x - g` compiled to a bare
 * `sub ba, hl ; ret`.  (The same comma-stops-the-scan defect was fixed earlier
 * for `cp ba, X`; this locks the ALU forms.)  Found by the 2026-10 review and
 * its random-expression fuzz.  Differential vs host.
 */
#include "harness.h"

volatile i16 g5 = -2, g0 = -4096, gpos = 300;
volatile u16 gu = 0x1234;
volatile i32 g32 = -70000L, g32b = 123456L;
i16 arr[4] = { 7, -3, 1000, -1000 };

static u16 u_sub_glob(u16 x)         { return x - gu; }
static i16 s_sub_glob(i16 x)         { return x - g5; }
static i16 s_sub_deref(i16 x, i16 *p){ return x - *p; }
static u16 u_add_glob(u16 x)         { return x + gu; }
static i16 s_sub_expr(i16 x)         { return x - (i16)(g5 * 3); }
static u16 u_sub_shift(i16 p0)       { return (u16)gpos - (u16)((u16)p0 << 6); }
static i16 logic_sub(void)           { return (i16)((u16)(g5 & g0) - (u16)(g0 | g5)) ^ 1; }
static i32 l_sub_glob(i32 x)         { return x - g32; }
static i32 l_add_glob(i32 x)         { return x + g32b; }
static u32 ul_sub_deref(u32 x, i32 *p){ return x - (u32)*p; }

void diff_run(void)
{
    i16 loc = 7;
    EMIT_U16("u_sub_glob",  u_sub_glob(12));
    EMIT_U16("u_sub_glob2", u_sub_glob(0));
    EMIT_I16("s_sub_glob",  s_sub_glob(12));
    EMIT_I16("s_sub_deref", s_sub_deref(10, &loc));
    EMIT_I16("s_sub_arr1",  s_sub_deref(10, &arr[1]));
    EMIT_I16("s_sub_arr3",  s_sub_deref(-5, &arr[3]));
    EMIT_U16("u_add_glob",  u_add_glob(0xF000));
    EMIT_I16("s_sub_expr",  s_sub_expr(100));
    EMIT_U16("u_sub_shift", u_sub_shift(-1));
    EMIT_U16("u_sub_shift2",u_sub_shift(5));
    EMIT_I16("logic_sub",   logic_sub());
    EMIT_I32("l_sub_glob",  l_sub_glob(5));
    EMIT_I32("l_add_glob",  l_add_glob(-5));
    EMIT_U32("ul_sub_deref",ul_sub_deref(0x10000UL, (i32 *)&g32));
}
