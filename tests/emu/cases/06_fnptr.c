/* 06_fnptr — 3-byte banked function pointers (s22): table dispatch through the
 * __sdcc_fptr cell (provided by crt0) + native call (hhll). */
#include "emu.h"

static int op_add(int a, int b) { return a + b; }
static int op_sub(int a, int b) { return a - b; }
static int op_mix(int a, int b) { return (a & 0xFF) | (b << 8); }

typedef int (*binop)(int, int);

binop ops[3] = {op_add, op_sub, op_mix};
volatile unsigned char idx = 1;

int main(void)
{
    binop f = op_add;

    CHECK(f(40, 2) == 42);

    f = ops[idx];                       /* runtime-indexed load of a 3-byte pointer */
    CHECK(f(40, 2) == 38);

    CHECK(ops[2](0x1234, 0x56) == 0x5634);

    f = (idx != 0) ? op_sub : op_add;   /* conditional pointer select */
    CHECK(f(100, 1) == 99);

    EMU_DONE();
}
