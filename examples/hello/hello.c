/* hello.c — a minimal Pokémon Mini ROM built with sdcc88.
 *
 * Demonstrates the device header (<pm.h>) and the C runtime (the support library
 * is auto-linked, e.g. the division below). Build with the Makefile in this dir:
 *
 *     make            -> hello.min   (a flat Pokémon Mini ROM image)
 *     make run        -> build + run on the bundled emulator
 *
 * On real hardware main() would normally loop forever driving the game; here it
 * returns a value so the test harness/emulator sees a clean exit (the production
 * crt0 stores the return value and halts).
 */
#include <pm.h>

int main(void)
{
    /* --- configure the program-rendering chip (display) --- */
    PRC_RATE = RATE_24FPS;
    PRC_MODE = MAP_ENABLE | COPY_ENABLE | MAP_24X16;   /* 192x128 tile map */

    /* --- read the key pad (active low: a pressed key reads 0) --- */
    unsigned char keys = (unsigned char) ~KEY_PAD;

    /* --- a little arithmetic to exercise the runtime/support library --- */
    int score = 1000;
    int lives = 7;
    int avg   = score / lives;        /* __divsint from s1c88.lib: 142 */

    /* Return something deterministic so `make run` can check it (== 42). */
    return (avg - 100) + (keys & 0) ;  /* 142 - 100 = 42 */
}
