/*-------------------------------------------------------------------------
   _mulsint2slong.c - signed 16x16 -> 32 widening multiply (s1c88)

   Used by the middle end for `int * int -> long` when
   port->support.has_mulint2long is enabled (main.c).

   The 32-bit two's-complement product of two sign-extended 16-bit operands
   equals the UNSIGNED 16x16 product of their bit patterns, minus — for each
   negative operand — the OTHER operand's 16-bit value placed in the high word
   (all mod 2^32):

       sext(a) == (uint)a - (a<0 ? 2^16 : 0)             (mod 2^32)
       sext(a)*sext(b) == U - a<0?( (uint)b << 16 ) - b<0?( (uint)a << 16 )

   where U = the unsigned 16x16 product and the a<0*b<0 * 2^32 cross term
   vanishes mod 2^32. So reuse the unsigned routine and apply the fixups.

   SYMBOL: see _muluint2ulong.c — the mangled middle-end name `___mulsint2slong`
   is aliased to the plain-named routine in this same module.
-------------------------------------------------------------------------*/

#include <sdcc-lib.h>

extern unsigned long _uwmul16 (unsigned int a, unsigned int b) __SDCC_NONBANKED;

long
_swmul16 (int a, int b) __SDCC_NONBANKED
{
    unsigned long u = _uwmul16 ((unsigned int)a, (unsigned int)b);

    if (a < 0)
        u -= (unsigned long)(unsigned int)b << 16;
    if (b < 0)
        u -= (unsigned long)(unsigned int)a << 16;

    return (long)u;
}

/* alias the middle end's mangled symbol to _swmul16 (same module, same addr) */
void
_swmul16_alias (void) __naked
{
    __asm
        .globl ___mulsint2slong
        ___mulsint2slong = __swmul16
    __endasm;
}
