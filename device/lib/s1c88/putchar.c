/*-------------------------------------------------------------------------
   putchar.c - default character output for the s1c88 (Pokemon Mini)

   printf()/vprintf() output one character at a time through putchar().  The
   Pokemon Mini has no text console, so the default sends each byte to the
   minimon emulator's debug-console mailbox (DEBUG_OUT in <pm.h>, RAM 0x1FF8);
   storing there prints the character on the host.

   This is a STANDALONE library module: a program that defines its own putchar()
   (for example, drawing glyphs to the LCD through the PRC) overrides this one —
   the linker resolves the reference from the user's object and never pulls this
   default in.  (Not a weak symbol: the s1c88 toolchain has no weak symbols; the
   override works through ordinary archive member selection.)

   Addressed by literal because the runtime library is compiled without the
   target's <pm.h> on the include path; the address mirrors <pm.h>'s DEBUG_OUT.
-------------------------------------------------------------------------*/

int
putchar (int c)
{
    *(volatile unsigned char *) 0x1FF8 = (unsigned char) c;
    return c;
}
