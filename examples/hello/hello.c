/* hello.c — a minimal Pokémon Mini ROM built with sdcc88.
 *
 *     make            -> hello.min   (a flat Pokémon Mini ROM image)
 *     make run        -> build + run on the bundled emulator
 *
 * It greets the world and prints a number computed with the support library
 * (the decimal conversion uses the runtime divide/modulo from s1c88.lib).
 *
 * Output channel: real Pokémon Mini hardware has no text console — you draw to
 * the LCD through the PRC (see the PRC registers, OAM and TILEMAP in <pm.h>). The development
 * emulator instead exposes a debug console at the top of RAM: storing a byte at
 * 0x1FF8 prints it on the host.  This example uses that so `make run` shows
 * something; swap in PRC drawing for a real cartridge.
 */
#include <pm.h>

#define CONSOLE  (*(volatile unsigned char *)0x1FF8)   /* emulator debug console */

static void cputs(const char *s)
{
    while (*s)
        CONSOLE = (unsigned char) *s++;
}

static void cputu(unsigned int v)          /* print v in decimal */
{
    char buf[5];
    unsigned char i = 0;
    if (v == 0) { CONSOLE = '0'; return; }
    while (v) {                            /* %/ are the s1c88.lib divide/modulo */
        buf[i++] = (char) ('0' + v % 10u);
        v /= 10u;
    }
    while (i)
        CONSOLE = (unsigned char) buf[--i];
}

int main(void)
{
    unsigned int n = 1000;

    cputs("Hello, Pokemon Mini!\n");
    cputs("1000 / 7 = ");
    cputu(n / 7u);                          /* 142 */
    CONSOLE = '\n';

    return 0;                               /* the crt0 halts; exit code 0 = ok */
}
