/* 02_arith — 8/16-bit ALU, shifts, compares (the cp ba,hl / jrs LT/GE cluster). */
#include "emu.h"

volatile int a16 = 1234;
volatile int b16 = -567;
volatile unsigned int u16 = 0xBEEF;
volatile unsigned char a8 = 200;
volatile unsigned char b8 = 57;
volatile signed char s8 = -100;

int main(void)
{
    /* 16-bit add/sub/neg */
    CHECK(a16 + b16 == 667);
    CHECK(a16 - b16 == 1801);
    CHECK(-b16 == 567);

    /* logic ops (8-bit routed through B, 16-bit pairwise) */
    CHECK((u16 & 0x0FF0) == 0x0EE0);
    CHECK((u16 | 0x1010) == 0xBEFF);
    CHECK((u16 ^ 0xFFFF) == 0x4110);
    CHECK((unsigned char)(a8 & b8) == 8);
    CHECK((unsigned char)(a8 | b8) == 249);
    CHECK((unsigned char)(a8 ^ b8) == 241);
    CHECK((unsigned char)~b8 == 198);

    /* shifts (A/B routed; S1C88 only shifts A/B/[HL]) */
    CHECK((a16 << 3) == 9872);
    CHECK((a16 >> 2) == 308);
    CHECK((b16 >> 1) == -284);          /* arithmetic right shift */
    CHECK((u16 >> 4) == 0x0BEE);
    CHECK((unsigned char)(b8 << 2) == 228);

    /* compares: signed and unsigned, reg and literal */
    CHECK(b16 < a16);
    CHECK(a16 > 1233);
    CHECK(b16 < 0);
    CHECK(u16 > 0x8000u);               /* unsigned compare must not go signed */
    CHECK(a8 > b8);
    CHECK(s8 < 1);
    CHECK((signed char)(s8 - 30) > 0);  /* 8-bit signed wraparound: -130 -> 126 */
    CHECK(a16 == 1234 && b16 != 0);

    /* increments through memory */
    a8++;
    b16--;
    CHECK(a8 == 201);
    CHECK(b16 == -568);

    EMU_DONE();
}
