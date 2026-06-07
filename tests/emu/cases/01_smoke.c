/* 01_smoke — crt0 + protocol sanity: data init, char-out, clean exit. */
#include "emu.h"

int rom_init = 0x1234;  /* exercises the _INITIALIZER -> _INITIALIZED copy */
int bss_zero;

int main(void)
{
    emu_puts("smoke\n");
    CHECK(rom_init == 0x1234);
    CHECK(bss_zero == 0);
    EMU_DONE();
}
