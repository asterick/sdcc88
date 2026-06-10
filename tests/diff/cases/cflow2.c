/* cflow2 — irregular control flow, differential vs host.
 *
 * control.c covers ordinary if/switch/loops/goto-loop; this stresses the
 * PATHOLOGICAL shapes that exercise the control-flow-graph + branch codegen
 * (and branch relaxation, the #14 area) the hardest:
 *   - Duff's device: a switch whose cases land INSIDE a do-while body
 *     (interleaved switch/loop — irreducible-looking flow);
 *   - goto out of doubly-nested loops (labelled-break emulation);
 *   - a goto-driven state machine (jumps among state labels);
 *   - switch nested in a loop nested in a switch, with fallthrough.
 *
 * SOUNDNESS: control flow is width-independent and every echo is a deterministic
 * value/trace truncated to its emitted width (harness.h) — sound by construction.
 */
#include "harness.h"

/* --- Duff's device: copy `count` bytes, x8-unrolled, switch into the loop --- */
static void duff_copy(u8 *dst, const u8 *src, u16 count)
{
    u16 n;
    if (count == 0) return;
    n = (u16)((count + 7u) / 8u);
    switch (count % 8u) {
    case 0: do { *dst++ = *src++;   /* fallthrough */
    case 7:      *dst++ = *src++;   /* fallthrough */
    case 6:      *dst++ = *src++;   /* fallthrough */
    case 5:      *dst++ = *src++;   /* fallthrough */
    case 4:      *dst++ = *src++;   /* fallthrough */
    case 3:      *dst++ = *src++;   /* fallthrough */
    case 2:      *dst++ = *src++;   /* fallthrough */
    case 1:      *dst++ = *src++;
            } while (--n > 0);
    }
}

/* --- goto out of nested loops: first cell == target -> row*16+col, else 0xFFFF --- */
static u16 find2d(const u8 *m, u8 rows, u8 cols, u8 target)
{
    u8 r, c;
    for (r = 0; r < rows; r++)
        for (c = 0; c < cols; c++)
            if (m[(u16)r * cols + c] == target)
                goto found;
    return 0xFFFFu;
found:
    return (u16)((u16)r * 16u + c);
}

/* --- goto state machine: count "ab" occurrences in a byte string --- */
static u16 count_ab(const u8 *s)
{
    u16 hits = 0;
s0:                                 /* start / saw-non-a */
    if (*s == 0) return hits;
    if (*s == 'a') { s++; goto s1; }
    s++; goto s0;
s1:                                 /* saw 'a' */
    if (*s == 0) return hits;
    if (*s == 'b') { hits++; s++; goto s0; }
    if (*s == 'a') { s++; goto s1; }   /* "aa..": stay armed */
    s++; goto s0;
}

/* --- switch in loop in switch, with fallthrough --- */
static u16 nested_sw(u8 mode, u8 n)
{
    u16 acc = 0;
    u8 i;
    switch (mode) {
    case 0:
        for (i = 0; i < n; i++)
            switch (i & 3u) {
            case 0: acc += 1; break;
            case 1: acc += 2;   /* fallthrough */
            case 2: acc += 4; break;
            default: acc += 8; break;
            }
        break;
    case 1: acc = 0xAAAA; break;          /* fallthrough */
    case 2: acc = (u16)(acc + n); break;
    default: acc = 0xFFFF; break;
    }
    return acc;
}

static const u8 src[20] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20
};
static const u8 grid[3][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12}};
static const u8 str1[] = "xabyaabbabz";   /* "ab" at 1, ...; aab -> one, bb -> one, ab -> one */

static volatile u8 counts[6] = {0u, 1u, 3u, 8u, 17u, 20u};
static volatile u8 targs[5]  = {1u, 6u, 12u, 99u, 9u};

void diff_run(void)
{
    u8 dst[20];
    u8 i, k;

    /* Duff copy at every length, sum the copied prefix back out */
    for (k = 0; k < 6; k++) {
        u8 cnt = counts[k];
        u16 s = 0;
        for (i = 0; i < 20; i++) dst[i] = 0;
        duff_copy(dst, src, cnt);
        for (i = 0; i < cnt; i++) s += dst[i];
        EMIT_U16("duff", s);
        EMIT_U8("duff_tailclear", (u8)(cnt < 20 ? dst[cnt] : 0));   /* nothing past cnt */
    }

    /* goto-out-of-nested-loops search */
    for (k = 0; k < 5; k++)
        EMIT_U16("find", find2d(&grid[0][0], 3, 4, targs[k]));

    /* goto state machine */
    EMIT_U16("count_ab", count_ab(str1));

    /* nested switch/loop/switch */
    for (k = 0; k < 4; k++)
        for (i = 0; i < 6; i++)
            EMIT_U16("nsw", nested_sw(k, counts[i]));
}
