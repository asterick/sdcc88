/* 24_stray_halt — the runner must NOT report a stray `halt` as a clean exit.
 *
 * NEGATIVE case: `emu-expect-rc: 124`.
 *
 * The production crt0 ends a run by `int (0x48)` into the test BIOS's shutdown
 * routine, which halts with main's return value in BA. The runner used to treat
 * ANY halt as "main returned" and read BA — so a miscompile that wandered into a
 * `halt` opcode (0xCE 0xAE) with BA == 0 passed, and the "124 stray-halt" code
 * the harness documented did not exist. The runner now requires the halt PC to
 * be inside the BIOS shutdown routine (BIOS_SHUTDOWN, from bios.map) and
 * returns 124 otherwise. This case halts in user code with BA = 0 and must be
 * reported as a stray halt — never as a pass.
 */
#include "emu.h"

volatile unsigned char sink;

int main(void)
{
    sink = 1;
    __asm
        ld   ba, #0x0000       ; "exit code 0" in the return register
        halt                   ; ... but NOT via the shutdown vector
    __endasm;
    sink = 2;                  /* never reached (the runner stops at the halt) */
    return 7;                  /* would be a FAIL if we ever got here */
}
