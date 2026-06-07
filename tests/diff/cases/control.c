/* control — control flow (differential vs host).
 *
 * if/else-if chains, switch (dense jump-table + gaps + fallthrough + default),
 * for/while/do-while, break/continue, nested loops, short-circuit && / || with
 * side effects (verifies evaluation order + skipping), ternary, and goto. Each
 * construct emits a deterministic trace (branch markers + computed values); the
 * host and the emulator must produce identical streams, so any mis-dispatched
 * branch, wrong loop count, or broken short-circuit shows up as a diff.
 *
 * Control flow is width-independent in structure and all compares here are
 * same-typed, so this is sound by construction (see harness.h SOUNDNESS).
 */
#include "harness.h"

#define NQ 8
static volatile u8 inq[NQ] = {0, 1, 2, 3, 4, 5, 7, 255};

/* if / else-if / else chain */
static void t_ifelse(u8 x)
{
    if (x < 2)          EMIT_U8("if<2", x);
    else if (x < 5)     EMIT_U8("if<5", x);
    else if (x == 255)  EMIT_U8("if255", x);
    else                EMIT_U8("ifelse", x);
}

/* switch: dense body (jump table likely), a gap (4 -> default for some inputs),
   stacked labels (1,2), and a fallthrough (3 -> 5) */
static void t_switch(u8 x)
{
    switch (x) {
    case 0: EMIT_U8("sw0", 0); break;
    case 1:
    case 2: EMIT_U8("sw12", x); break;
    case 3: EMIT_U8("sw3", 3);          /* fallthrough */
    case 5: EMIT_U8("sw35", x); break;
    default: EMIT_U8("swd", x); break;
    }
}

/* for loop with continue (skip 4) + break (stop at 7) */
static void t_for(u8 n)
{
    u16 sum = 0;
    u8 i;
    for (i = 0; i < n; i++) {
        if (i == 4) continue;
        if (i == 7) break;
        sum += i;
        EMIT_U8("it", i);
    }
    EMIT_U16("forsum", sum);
}

/* while + do-while (do runs at least once even when n == 0) */
static void t_while(u8 n)
{
    u8 i = 0;
    u16 s = 0;
    while (i < n) { s += i; i++; }
    EMIT_U16("while", s);

    i = 0;
    u16 d = 0;
    do { d += i; i++; } while (i < n);
    EMIT_U16("dowhile", d);
}

/* nested loops -> a*b iterations */
static void t_nested(u8 a, u8 b)
{
    u16 c = 0;
    u8 i, j;
    for (i = 0; i < a; i++)
        for (j = 0; j < b; j++)
            c++;
    EMIT_U16("nested", c);
}

/* short-circuit && / || with side effects: emits each operand actually
   evaluated, in order, then the boolean result */
static u8 sfx(u8 id, u8 ret) { EMIT_U8("sfx", id); return ret; }

static void t_shortcircuit(u8 p, u8 q)
{
    EMIT_U8("and", sfx(1, p) && sfx(2, q));
    EMIT_U8("or",  sfx(3, p) || sfx(4, q));
}

/* ternary */
static void t_ternary(u8 x)
{
    EMIT_U8("tern", (x & 1) ? (u8)(x + 10) : (u8)(x + 20));
}

/* goto-built loop (forward + backward jumps) */
static void t_goto(u8 n)
{
    u8 i = 0;
    u16 s = 0;
loop:
    if (i >= n) goto done;
    s += i;
    i++;
    goto loop;
done:
    EMIT_U16("goto", s);
}

void diff_run(void)
{
    u8 a, b;

    for (a = 0; a < NQ; a++) {
        t_ifelse(inq[a]);
        t_switch(inq[a]);
        t_for(inq[a]);
        t_while(inq[a]);
        t_ternary(inq[a]);
        t_goto(inq[a]);
    }

    for (a = 0; a < NQ; a++)
        for (b = 0; b < NQ; b++) {
            t_nested(inq[a], inq[b]);
            t_shortcircuit(inq[a] & 1, inq[b] & 1);
        }
}
