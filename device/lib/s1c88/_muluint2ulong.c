/*-------------------------------------------------------------------------
   _muluint2ulong.c - unsigned 16x16 -> 32 widening multiply (s1c88)

   Used by the middle end for `unsigned int * unsigned int -> unsigned long`
   when port->support.has_mulint2long is enabled (main.c), instead of widening
   both operands to 32 bits and calling the full 32x32 __mullong.

   Built from four 8x8 -> 16 partial products: `(unsigned char)x * (unsigned
   char)y` is recognised by the port as a native 8x8 multiply (the MLT
   instruction, HL <- L*A), so this routine pulls no 32x32 multiply itself.
   The product of two 16-bit values always fits in 32 bits, so the sum below
   never overflows.

   IMPORTANT: keep each partial product in a 16-bit (unsigned int) result, NOT a
   32-bit one. With has_mulint2long enabled, a multiply whose RESULT is 32-bit is
   itself rewritten by the middle end into a ___muluint2ulong call — so assigning
   a partial product straight to `unsigned long` would make this routine call
   itself. A 16-bit result context keeps it a plain native 8x8 multiply. The
   widening into 32 bits then happens with adds/shifts only (no multiply).

   SYMBOL: the middle end calls the mangled name `___muluint2ulong`, which no C
   function name can emit (SDCC never produces a 3-underscore asm symbol from a
   C identifier). So the routine is defined under a plain name and the mangled
   symbol is aliased to it in this same module (sdas `=` assignment).
-------------------------------------------------------------------------*/

#include <sdcc-lib.h>

unsigned long
_uwmul16 (unsigned int a, unsigned int b) __SDCC_NONBANKED
{
    unsigned char ah = a >> 8, bh = b >> 8;

    /* four 8x8 -> 16 native multiplies (16-bit result keeps them native; see above) */
    unsigned int ll = (unsigned char)a * (unsigned char)b;    /* bytes 0..1 */
    unsigned int lh = (unsigned char)a * bh;                  /* bytes 1..2 */
    unsigned int hl = ah * (unsigned char)b;                  /* bytes 1..2 */
    unsigned int hh = ah * bh;                                /* bytes 2..3 */

    /* widen + assemble with adds/shifts only (no multiply) */
    unsigned long mid = (unsigned long)lh + hl;               /* up to 17 bits */
    return (unsigned long)ll + (mid << 8) + ((unsigned long)hh << 16);
}

/* alias the middle end's mangled symbol to _uwmul16 (same module, same addr) */
void
_uwmul16_alias (void) __naked
{
    __asm
        .globl ___muluint2ulong
        ___muluint2ulong = __uwmul16
    __endasm;
}
