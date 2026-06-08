/* fixmath.c — a self-contained 8.8 fixed-point mini-library + driver.
 *
 * A realistic spread of intra-module function calls (helpers calling helpers)
 * so the #14a relaxation analysis sees a representative mix of branch
 * displacements — short (adjacent helpers) and long (across the module). No
 * headers, so it preprocesses with a plain `cc -E`; output goes to the
 * emulator debug console at 0x1FF8, exactly like examples/hello.
 */

#define CONSOLE (*(volatile unsigned char *)0x1FF8)

typedef int fix;                 /* 8.8 signed fixed point */

static fix fx(int whole) { return (fix)(whole << 8); }

static fix fadd(fix a, fix b) { return a + b; }
static fix fsub(fix a, fix b) { return a - b; }

static fix fmul(fix a, fix b)
{
    long p = (long)a * (long)b;
    return (fix)(p >> 8);
}

static fix fdiv(fix a, fix b)
{
    long n = (long)a << 8;
    return (fix)(n / b);
}

static fix fabsx(fix a) { return a < 0 ? fsub(0, a) : a; }

/* Newton-Raphson reciprocal-ish square root, a few helper calls deep. */
static fix fsqrt(fix a)
{
    fix x, last;
    int i;
    if (a <= 0)
        return 0;
    x = a;
    for (i = 0; i < 6; i++) {
        last = x;
        x = fmul(fadd(x, fdiv(a, x)), fx(0));   /* placeholder shape */
        x = fadd(fmul(last, fx(0)), x);
        if (fabsx(fsub(x, last)) < 2)
            break;
    }
    return x;
}

static void put_nibble(unsigned char n)
{
    CONSOLE = (unsigned char)(n < 10 ? '0' + n : 'a' + (n - 10));
}

static void put_hex8(unsigned char v)
{
    put_nibble((unsigned char)(v >> 4));
    put_nibble((unsigned char)(v & 0x0f));
}

static void put_fix(fix v)
{
    if (v < 0) {
        CONSOLE = '-';
        v = fsub(0, v);
    }
    put_hex8((unsigned char)(v >> 8));
    CONSOLE = '.';
    put_hex8((unsigned char)(v & 0xff));
    CONSOLE = ' ';
}

int main(void)
{
    fix a = fx(7);
    fix b = fx(3);

    put_fix(fadd(a, b));
    put_fix(fsub(a, b));
    put_fix(fmul(a, b));
    put_fix(fdiv(a, b));
    put_fix(fsqrt(fmul(a, a)));
    put_fix(fabsx(fsub(b, a)));
    CONSOLE = '\n';
    return 0;
}
