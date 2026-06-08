/* 11_bankcc — EXECUTION test for a banked conditional branch with a SHORT-ONLY
 * signed condition (bjump <signed cc>, target), which sdas88 lowers to the
 * invert-and-skip form `jrs <inv cc>, +7 ; ld nb,#bank ; jrl target` (9 bytes).
 * The compiler never emits this (its banked branches are unconditional), so it
 * has no other coverage — verify the control flow is correct both ways via
 * hand-written inline asm.  `lt` = signed less-than after `cp a,#imm`.
 */
#include "emu.h"

volatile unsigned char r_true, r_false;

int main(void)
{
    /* lt TRUE: signed 0 < 5 -> the banked branch is taken */
    __asm
        ld   a, #0
        cp   a, #5
        bjump lt, 00001$
        ld   a, #0xBB          ; fall-through (should NOT run)
        jrs  00002$
      00001$:
        ld   a, #0xAA          ; branch target (taken)
      00002$:
        ld   (_r_true), a
    __endasm;

    /* lt FALSE: signed 5 < 0 is false -> the branch is skipped (fall through) */
    __asm
        ld   a, #5
        cp   a, #0
        bjump lt, 00003$
        ld   a, #0xBB          ; fall-through (should run)
        jrs  00004$
      00003$:
        ld   a, #0xAA          ; branch target (should NOT run)
      00004$:
        ld   (_r_false), a
    __endasm;

    CHECK(r_true  == 0xAA);    /* lt true  -> banked branch taken      */
    CHECK(r_false == 0xBB);    /* lt false -> invert-skip jumped over it */

    emu_puts("bankcc ok\n");
    EMU_DONE();
}
