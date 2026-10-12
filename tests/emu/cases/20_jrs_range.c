/* 20_jrs_range — the peephole's instruction-size model vs real S1C88 sizes.
 *
 * The jp->jrs peephole (labelInRange) sums s1c88instructionSize() over the
 * lines a branch spans. The model under-sized the CE/CF-prefixed forms the
 * codegen emits most — `ld a,(hhll)` (4 B, modelled 3), `push a` (2 B, modelled
 * 1), `ld ba,#imm` (3 B, modelled 2) — so a branch over 16 global byte copies
 * (real span 128 B, modelled 96 B) became a `jrs` whose displacement overflows.
 * The linker reports "Byte PCR relocation error" and the ROM loops or jumps
 * short.  Fixed by deriving the size table from the ISA; this case links only
 * if every branch the compiler shortens really is in range.
 */
#include "emu.h"

volatile unsigned char c = 1;
unsigned char g1, g2, g3, g4, g5, g6, g7, g8, g9, g10, g11, g12, g13, g14, g15, g16;
unsigned char h1 = 1, h2 = 2, h3 = 3, h4 = 4, h5 = 5, h6 = 6, h7 = 7, h8 = 8,
              h9 = 9, h10 = 10, h11 = 11, h12 = 12, h13 = 13, h14 = 14, h15 = 15, h16 = 16;

void copy16(void)
{
    if (c) {
        g1 = h1;   g2 = h2;   g3 = h3;   g4 = h4;
        g5 = h5;   g6 = h6;   g7 = h7;   g8 = h8;
        g9 = h9;   g10 = h10; g11 = h11; g12 = h12;
        g13 = h13; g14 = h14; g15 = h15; g16 = h16;
    }
    c = 0;
}

/* a backward branch over the same span: a loop whose body is ~128 B */
unsigned int loop16(unsigned char n)
{
    unsigned int acc = 0;
    while (n--) {
        acc += h1;  acc += h2;  acc += h3;  acc += h4;
        acc += h5;  acc += h6;  acc += h7;  acc += h8;
        acc += h9;  acc += h10; acc += h11; acc += h12;
        acc += h13; acc += h14; acc += h15; acc += h16;
    }
    return acc;
}

int main(void)
{
    copy16();
    CHECK(c == 0);
    CHECK(g1 == 1 && g8 == 8 && g16 == 16);
    CHECK(g1 + g2 + g3 + g4 + g5 + g6 + g7 + g8 + g9 + g10 + g11 + g12 + g13 + g14 + g15 + g16 == 136);
    CHECK(loop16(3) == 408);

    emu_puts("jrs_range ok\n");
    EMU_DONE();
}
