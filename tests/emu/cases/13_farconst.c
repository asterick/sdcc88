/* 13_farconst — far const data: a __far const table placed in a far bank,
 * read back via the EP-paged deref (the #9 far machinery). Confirms far const
 * works today (relevant to TODO #17). */
#include "emu.h"

__far const unsigned char tbl[4] = { 0x11, 0x22, 0x33, 0x44 };

int main(void)
{
    CHECK(tbl[0] == 0x11);
    CHECK(tbl[1] == 0x22);
    CHECK(tbl[2] == 0x33);
    CHECK(tbl[3] == 0x44);
    emu_puts("farconst ok\n");
    EMU_DONE();
}
