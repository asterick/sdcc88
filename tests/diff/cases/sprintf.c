/* sprintf — differential coverage of the formatter (_print_format) vs host.
 *
 * sprintf writes into a caller buffer (no putchar), so it's directly comparable:
 * the SAME format + args produce the SAME text on host and target.  This covers
 * the shared printf/sprintf/vprintf core; printf only adds putchar on top.
 *
 * SOUNDNESS: keep %d/%i args within int16 range and %u/%x within u16 (host int is
 * 32-bit, target 16-bit); use %ld/%lu for 32-bit.  Emit the produced bytes and the
 * return value (number of chars written).  Only width/precision/flags whose result
 * is value-independent of pointer size are used.
 */
#include "harness.h"

#ifdef DIFF_HOST
#include <stdio.h>
#else
int sprintf (char *, const char *, ...);
#endif

/* emit the NUL-terminated result plus sprintf's return value */
static void emit_buf (const char *tag, const char *b, int rv)
{
    diff_puts (tag); diff_putc (':');
    while (*b) diff_hex2 ((u8) *b++);
    diff_putc ('/');
    diff_hex2 ((u8) rv);
    diff_nl ();
}

void diff_run (void)
{
    char b[40];
    int n;

    n = sprintf (b, "plain");                       emit_buf ("plain", b, n);
    n = sprintf (b, "d=%d u=%u", -1234, 1234u);     emit_buf ("dec", b, n);
    n = sprintf (b, "x=%x X=%X", 0xab, 0xCD);       emit_buf ("hex", b, n);
    n = sprintf (b, "c=%c s=%s", 'Q', "str");       emit_buf ("cs", b, n);
    n = sprintf (b, "pct=%% end");                  emit_buf ("pct", b, n);
    n = sprintf (b, "[%5d]", 42);                   emit_buf ("w", b, n);
    n = sprintf (b, "[%-5d]", 42);                  emit_buf ("wl", b, n);
    n = sprintf (b, "[%05d]", 42);                  emit_buf ("wz", b, n);
    n = sprintf (b, "[%+d]", 42);                   emit_buf ("sgn", b, n);
    n = sprintf (b, "ld=%ld lu=%lu", -100000L, 100000UL); emit_buf ("long", b, n);
    n = sprintf (b, "lx=%lx", 0xDEADBEEFUL);        emit_buf ("longx", b, n);
    n = sprintf (b, "mix %d/%s/%x!", 7, "ab", 0xf); emit_buf ("mix", b, n);
    n = sprintf (b, "%d %d %d %d", 1, 22, 333, 4444); emit_buf ("multi", b, n);
}
