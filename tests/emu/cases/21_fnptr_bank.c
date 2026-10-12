/* 21_fnptr_bank — runtime-formed function pointers to code OUTSIDE bank 0.
 *
 * `#pragma codeseg BANK1` puts this whole module in area _BANK1; emu-test.sh
 * links it at 0x18000 (bank 1), so every function here has a non-zero bank
 * byte. A function's address used as a VALUE (`p = f`, `return f`, `f` pushed
 * as an fptr argument) was a 2-byte immediate in the port, so the third byte of
 * the pointer was zero-filled: `p = target; p()` did `ld nb, #0 ; jp hl` and
 * dispatched into bank 0 (the emulator reported "CPU CRASHED nb=01"). Only the
 * static-initializer path (printIvalFuncPtr: `.byte _f, _f>>8, _f>>16`) was
 * right. Fixed by giving function-symbol aops FUNCPTRSIZE (3) and slicing the
 * high word of a 3-byte symbolic immediate as (sym>>8, sym>>16).
 *
 * Every other emu case links in bank 0, where a zero bank byte is correct by
 * accident — this is the only case where the bank byte is observable.
 */
#pragma codeseg BANK1
#include "emu.h"

typedef int (*binop)(int, int);
typedef void (*thunk)(void);

volatile unsigned char hit;

static int add(int a, int b) { return a + b; }
static int sub(int a, int b) { return a - b; }
void mark(void) { hit = 0xA5; }

thunk pstatic = mark;                  /* static initializer: already correct */
thunk pglobal;
binop ops[2] = { add, sub };

static int apply(binop f, int a, int b) { return f(a, b); }
static thunk pick(unsigned char which) { return which ? mark : (thunk)0; }

int main(void)
{
    thunk p;
    binop q;

    hit = 0; pstatic(); CHECK(hit == 0xA5);          /* baseline: static init */

    hit = 0; p = mark; p();                           /* runtime formation (local) */
    CHECK(hit == 0xA5);

    hit = 0; pglobal = mark; pglobal();               /* runtime formation (global) */
    CHECK(hit == 0xA5);

    hit = 0; p = pick(1); CHECK(p == mark); p();      /* `return f` */
    CHECK(hit == 0xA5);

    CHECK(apply(add, 20, 22) == 42);                  /* `f` pushed as an fptr argument */
    CHECK(apply(sub, 20, 22) == -2);

    q = ops[1]; CHECK(q(7, 5) == 2);                  /* array of static-init pointers */
    q = add;    CHECK(q(7, 5) == 12);

    CHECK(p == mark && p != (thunk)0);                /* 3-byte compare incl. bank */

    emu_puts("fnptr_bank ok\n");
    EMU_DONE();
}
