/* volatile — the compiler must honour `volatile`: no elimination, no coalescing,
 * no hoisting out of loops, no reordering of volatile accesses. Differential vs
 * host.
 *
 * This needs an OBSERVABLE side effect on access, which a plain memory cell can't
 * give (reading it twice yields the same value whether the compiler read once or
 * twice). The emulator provides a VOLATILE-PROBE register at 0x2070: a READ
 * returns a counter then post-increments it, a WRITE seeds it. So N volatile
 * reads must return N consecutive values — if codegen drops, merges, hoists, or
 * reorders a volatile access the target does a different number of reads and the
 * emitted sequence diverges from the host model.
 *
 * SOUNDNESS: VREAD/VSEED are the SAME source API on both ends; the host models
 * the identical post-increment/seed sequence, so a faithful target matches it
 * exactly. Every VREAD/VSEED is its own statement (a sequence point), so there is
 * no unspecified-evaluation-order divergence; results truncate to width.
 */
#include "harness.h"

#ifdef DIFF_HOST
/* host reference model of the 0x2070 probe: read returns then post-increments */
static unsigned char _vp;
static inline u8   VREAD(void)     { return _vp++; }
static inline void VSEED(u8 v)     { _vp = v; }
#else
/* target: the real side-effecting MMIO register (volatile — codegen must emit
   every access, in order, exactly once) */
#define VPROBE (*(volatile unsigned char *)0x2070)
static inline u8   VREAD(void)     { return VPROBE; }
static inline void VSEED(u8 v)     { VPROBE = v; }
#endif

void diff_run(void)
{
    u8 a, b, c, d;

    /* 1. no coalescing: four adjacent reads are four DISTINCT values */
    VSEED(0);
    a = VREAD(); b = VREAD(); c = VREAD(); d = VREAD();
    EMIT_U8("seq_a", a); EMIT_U8("seq_b", b);
    EMIT_U8("seq_c", c); EMIT_U8("seq_d", d);          /* 0,1,2,3 */

    /* 2. no elimination: a read whose value is discarded must still happen
          (the counter advances, so the NEXT read sees it) */
    VSEED(10);
    (void) VREAD();                                     /* discarded -> counter 11 */
    EMIT_U8("after_discard", VREAD());                  /* 11, not 10 */

    /* 3. no hoisting out of a loop: the read must re-execute each iteration */
    VSEED(100);
    {
        u16 s = 0; u8 i;
        for (i = 0; i < 8; i++) s = (u16)(s + VREAD());
        EMIT_U16("loopsum", s);                         /* 100+101+...+107 = 828 */
    }

    /* 4. a read in a loop CONDITION re-reads every test */
    VSEED(0);
    {
        u8 cnt = 0;
        while (VREAD() < 5u) cnt++;                      /* reads 0,1,2,3,4,5 -> stop */
        EMIT_U8("while_cnt", cnt);                       /* 5 */
    }

    /* 5. ordering: writes and reads keep program order, neither hoisted past the
          other */
    VSEED(20); a = VREAD();
    VSEED(40); b = VREAD();
    EMIT_U8("ord_a", a); EMIT_U8("ord_b", b);           /* 20, 40 */

    /* 6. a volatile read folded into arithmetic must still be a real read
          (sequence the two reads explicitly — operand order is unspecified) */
    VSEED(50);
    a = VREAD(); b = VREAD();
    EMIT_U8("arith", (u8)(a * 3u + b));                 /* 50*3+51 = 201 */
}
