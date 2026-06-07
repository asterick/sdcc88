/* 04_calls — the MAXIMUM-mode call model against real 3-byte CB:PC frames:
 * multi-arg calls (IY argument passing, #8), recursion (frame discipline),
 * pointer args, and struct return-by-value (s11 hidden buffer). */
#include "emu.h"

static int add3(int a, int b, int c) { return a + b + c; }

static unsigned char sum8(unsigned char a, unsigned char b,
                          unsigned char c, unsigned char d)
{
    return (unsigned char)(a + b + c + d);
}

static unsigned int fib(unsigned char n)
{
    if (n < 2)
        return n;
    return fib((unsigned char)(n - 1)) + fib((unsigned char)(n - 2));
}

static void store(int *p, int v) { *p = v; }

struct pair {
    int x;
    int y;
};

static struct pair make_pair(int x, int y)
{
    struct pair p;
    p.x = x;
    p.y = y;
    return p;
}

volatile int sink;

int main(void)
{
    int local = 0;
    struct pair p;

    CHECK(add3(1000, -2000, 3500) == 2500);
    CHECK(sum8(1, 2, 3, 250) == 0);     /* 256 wraps to 0 */
    CHECK(fib(10) == 55);               /* deep recursion: 3-byte return frames */

    store(&local, 4242);
    CHECK(local == 4242);

    p = make_pair(-5, 600);
    CHECK(p.x == -5);
    CHECK(p.y == 600);

    sink = add3(p.x, p.y, local);
    CHECK(sink == 4837);

    EMU_DONE();
}
