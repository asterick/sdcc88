/* longshift_iy — the FIXED #11-longshift-iy miscompile: a 32-bit variable LEFT
 * shift where BOTH the value and the count are MEMORY operands.
 *
 * `out = arr[idx] << cnt` (value from a runtime-indexed global array, count from a
 * global) fills all four byte GPRs with the value, so genLeftShift puts the loop
 * counter in IY. The count had to be zero-extended into IYH, but with every byte
 * GPR and HL busy the IY-half store left IYH stale — the loop ran a garbage count.
 * Now IYH is cleared explicitly. This case drives that exact inline path (NOT the
 * helper-laundered form longshift.c uses) across every count 0..31 and several
 * value patterns; on the host these are plain u32 shifts, bit-exact.
 */
#include "harness.h"

static u32 vals[6] = { 1u, 0x0Fu, 0x12345678u, 0x80000001u, 0xFFFFFFFFu, 0xDEADBEEFu };
static volatile u8 idx;        /* runtime index  -> value is a memory operand */
static volatile u8 cnt;        /* runtime count  -> count is a memory operand */

void diff_run(void)
{
    u8 v, n;

    for (v = 0; v < 6; v++)
        for (n = 0; n < 32; n++)
        {
            idx = v;
            cnt = n;
            EMIT_U32("u32<<mem", vals[idx] << cnt);   /* the inline IY-counter path */
        }

    /* count == 0 and count == width-1 edges, value re-read from memory each time */
    idx = 2; cnt = 0;  EMIT_U32("sh0",  vals[idx] << cnt);
    idx = 2; cnt = 31; EMIT_U32("sh31", vals[idx] << cnt);
}
