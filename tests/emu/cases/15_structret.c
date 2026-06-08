/* 15_structret — struct return-by-value ABI (bigreturn hidden pointer). Guards
 * the genRet offset fix (same path as the long long return, #11). A leaf and a
 * non-leaf returner, plus a multi-field struct. */
#include "emu.h"

struct pt { unsigned int x; unsigned int y; unsigned char z; };

static struct pt make(unsigned int a, unsigned int b) {  /* leaf bigreturn */
    struct pt p;
    p.x = a; p.y = b; p.z = (unsigned char)(a + b);
    return p;
}
static void sink(void) { }
static struct pt make2(unsigned int a) {  /* non-leaf (forces frame ptr) */
    struct pt p;
    sink();
    p.x = a; p.y = a ^ 0xFFFFu; p.z = 0x5A;
    return p;
}

int main(void)
{
    struct pt p = make(0x1234, 0xABCD);
    CHECK(p.x == 0x1234);
    CHECK(p.y == 0xABCD);
    CHECK(p.z == (unsigned char)(0x1234 + 0xABCD));

    struct pt q = make2(0x0F0F);
    CHECK(q.x == 0x0F0F);
    CHECK(q.y == 0xF0F0);
    CHECK(q.z == 0x5A);

    emu_puts("structret ok\n");
    EMU_DONE();
}
