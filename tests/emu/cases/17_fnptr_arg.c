/* 17_fnptr_arg — passing a function by value as an argument (the FIXED
 * #17-callback-ICE).
 *
 * THE FIXED BUG: a function's address is a 2-byte immediate (its PC), but a
 * function POINTER is a 3-byte banked value (PC + bank).  genIpush pushed only
 * the 2-byte PC for a function NAME/address argument, while the caller cleaned
 * up 3 bytes — so _G.stack.pushed ended at -1 and genEndFunction aborted with
 * a FATAL "Unbalanced stack".  (A function-pointer VARIABLE already pushed 3
 * bytes correctly; only the immediate dropped its bank byte.)  This blocked any
 * call passing a function name/address — qsort/bsearch comparators, printf's
 * output callback, plain higher-order calls.
 *
 * THE FIX: genIpush emits the full 3-byte banked fptr (PC + zero bank, matching
 * the `f = c` store).  A free pair + byte register take the fast path; under
 * full register pressure it reserves the 3-byte slot and fills it through HL,
 * saved/restored via the stack.  This case exercises BOTH: apply1() passes the
 * fptr as the only stacked arg (fast path); pick() passes it after four other
 * args (register pressure -> reserve-fill).
 */
#include "emu.h"

typedef unsigned char u8;

static u8 dbl (u8 x) { return (u8) (x * 2); }
static u8 inc (u8 x) { return (u8) (x + 1); }

/* fptr is the only stacked argument -> fast path */
static u8 apply1 (u8 (*f) (u8), u8 x) { return f (x); }

/* fptr passed AFTER four args -> the no-free-register reserve-fill path */
static u8 pick (u8 a, u8 b, u8 c, u8 d, u8 (*f) (u8), u8 x)
{
    return (u8) (a + b + c + d + f (x));
}

/* select a function pointer at runtime, pass it on, and call through it */
static u8 (*choose (u8 sel)) (u8) { return sel ? dbl : inc; }

int main (void)
{
    volatile u8 five = 5, seven = 7;

    /* fast path: function name / address as the sole stacked arg */
    CHECK (apply1 (dbl, five) == 10);
    CHECK (apply1 (inc, five) == 6);
    CHECK (apply1 (&dbl, seven) == 14);

    /* register-pressure path: fptr argument behind four others (1+2+3+4 + f(x)) */
    CHECK (pick (1, 2, 3, 4, dbl, seven) == 24);   /* 10 + 14 */
    CHECK (pick (1, 2, 3, 4, inc, five) == 16);    /* 10 + 6  */

    /* a runtime-selected fptr passed on (variable, not immediate) still works */
    CHECK (apply1 (choose (1), seven) == 14);
    CHECK (apply1 (choose (0), seven) == 8);

    emu_puts ("fnptr_arg ok\n");
    EMU_DONE ();
}
