/* 03_muldiv — native MLT multiply and the native DIV clusters (s21/s22):
 * unsigned 8÷8, the 16÷8 chain, and signed 8÷8 with the sep-mask fixup. */
#include "emu.h"

volatile unsigned char u8a = 250;
volatile unsigned char u8b = 7;
volatile signed char s8a = -100;
volatile signed char s8b = 7;
volatile signed char s8c = -7;
volatile unsigned int u16a = 50000;
volatile unsigned int i16 = 1234;

int main(void)
{
    /* 8x8 multiply -> MLT */
    CHECK((unsigned char)(u8b * 9) == 63);
    CHECK(u8a * u8b == 1750);           /* full 16-bit product */

    /* 16-bit multiply, literal path */
    CHECK(i16 * 10 == 12340);
    CHECK(i16 * 3 == 3702);

    /* unsigned 8÷8 */
    CHECK(u8a / u8b == 35);
    CHECK(u8a % u8b == 5);

    /* unsigned 16÷8 chain */
    CHECK(u16a / u8b == 7142);
    CHECK(u16a % u8b == 6);
    CHECK(i16 / 10 == 123);
    CHECK(i16 % 10 == 4);

    /* signed 8÷8 (C truncation toward zero) */
    CHECK(s8a / s8b == -14);
    CHECK(s8a % s8b == -2);
    CHECK(s8a / s8c == 14);
    CHECK((signed char)(100) / s8c == -14);

    EMU_DONE();
}
