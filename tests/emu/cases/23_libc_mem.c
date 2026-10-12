/* 23_libc_mem — memcpy, memset, memmove, strcpy and strlen as REAL library calls
 * with runtime lengths, through the SDK's own <string.h>.
 *
 * The installed <string.h> maps the mem/str functions to __builtin_* only for the z80-family
 * targets; for s1c88 it takes the generic branch (`#define memcpy __memcpy`,
 * plain extern memset), so these are calls into s1c88.lib. memcpy.c (__memcpy)
 * and _memset.c were NOT library members: a program using them linked with only
 * an "Undefined Global" warning and the bcalls targeted address 0 (2026-10
 * review). The driver does fail the link, but the test suites only failed on
 * undefined globals in their own grep — and no case called memcpy with a
 * runtime n. This one does.
 */
#include <string.h>
#include "emu.h"

unsigned char src[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
unsigned char dst[16];
char sbuf[24];
volatile unsigned char n8 = 10;      /* runtime lengths: no literal to inline */
volatile unsigned int  n16 = 300;
unsigned char big[320];

int main(void)
{
    unsigned char i, ok;

    memset(dst, 0xEE, 16);
    CHECK(dst[0] == 0xEE && dst[15] == 0xEE);
    CHECK(memcpy(dst, src, n8) == dst);
    ok = 1;
    for (i = 0; i < 10; i++) if (dst[i] != i + 1) ok = 0;
    CHECK(ok && dst[10] == 0xEE);               /* copied n8 bytes, not more */

    CHECK(memset(dst, 0x5A, n8) == dst);
    ok = 1;
    for (i = 0; i < 10; i++) if (dst[i] != 0x5A) ok = 0;
    CHECK(ok && dst[10] == 0xEE);

    /* 16-bit lengths (> 255) */
    memset(big, 0, sizeof big);
    memset(big, 0x11, n16);
    CHECK(big[0] == 0x11 && big[299] == 0x11 && big[300] == 0);
    memcpy(big + 16, big, n16);
    CHECK(big[16] == 0x11 && big[315] == 0x11 && big[316] == 0);

    /* overlapping move, both directions */
    for (i = 0; i < 16; i++) dst[i] = i;
    memmove(dst + 4, dst, n8);                  /* forward overlap */
    CHECK(dst[4] == 0 && dst[13] == 9 && dst[14] == 14);
    memmove(dst, dst + 4, n8);                  /* backward overlap */
    CHECK(dst[0] == 0 && dst[9] == 9);

    /* strings */
    strcpy(sbuf, "s1c88");
    CHECK(strlen(sbuf) == 5);
    strcat(sbuf, "-mini");
    CHECK(strcmp(sbuf, "s1c88-mini") == 0);
    CHECK(memcmp(src, dst, 1) != 0);

    emu_puts("libc_mem ok\n");
    EMU_DONE();
}
