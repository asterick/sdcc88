/* structargs — struct-by-value arguments + struct copy/assignment, differential.
 *
 * The S1C88 ABI passes every struct/union argument ON THE STACK (scalars fill
 * A/L/H/B and BA/HL/IY first), so this exercises the aggregate push path, the
 * register-vs-stack split when structs and scalars are mixed, by-value copy-in
 * semantics, whole-struct assignment (the ldir copy), and struct-arg + struct-
 * return together (the hidden-return-pointer interaction that bit the return
 * side once, #18 — this is the symmetric arg half).
 *
 * SOUNDNESS: struct *layout/padding* is implementation-defined and DIFFERS
 * host-vs-target (gcc aligns; sdcc packs byte-wise), so we NEVER read raw struct
 * bytes / pun / emit sizeof — only NAMED members. A by-value pass or a whole-
 * struct assignment preserves every named member's value regardless of layout,
 * and each compiler reads its own members at its own offsets, so member reads
 * are layout-independent and sound. Every member value is truncated to its
 * emitted width.
 */
#include "harness.h"

struct s1  { u8  a; };                       /* 1 byte  */
struct s2  { u8  a, b; };                     /* 2 bytes */
struct s3  { u8  a, b, c; };                  /* 3 bytes (odd size on the stack) */
struct s4  { u16 a, b; };                     /* 4 bytes */
struct sm  { u8  a; u16 b; u8 c; };           /* mixed -> packed on target, padded on host */
struct s8  { u32 lo, hi; };                   /* 8 bytes -> struct-return is bigreturn */
struct nst { struct s2 p; u16 tag; u8 arr[3]; }; /* nested struct + array member */

static volatile u8  v8[8]  = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
static volatile u16 v16[4] = { 0x1234, 0xABCD, 0x8001, 0xFFFF };

/* --- by-value args, scalar results (read the members the callee was handed) --- */
static u16 sum1(struct s1 s) { return s.a; }
static u16 sum2(struct s2 s) { return (u16)s.a + s.b; }
static u16 sum3(struct s3 s) { return (u16)s.a + s.b + s.c; }
static u32 sum4(struct s4 s) { return (u32)s.a + s.b; }
static u32 summ(struct sm s) { return (u32)s.a + s.b + s.c; }
static u32 sum8(struct s8 s) { return s.lo ^ s.hi; }
static u16 sumn(struct nst s) { return (u16)s.p.a + s.p.b + s.tag + s.arr[0] + s.arr[1] + s.arr[2]; }

/* by-value semantics: the callee clobbers its copy; the caller's must be intact */
static u16 clobber(struct s4 s) { s.a = 0; s.b = 0; return s.a + s.b; }

/* struct arg mixed with scalar args: x->reg, s->stack, y->reg (split ABI) */
static u32 mixargs(u8 x, struct s4 s, u16 y) { return (u32)x + s.a + s.b + y; }

/* struct FIRST, then a scalar: the scalar takes a register (BA / A / HL) BEFORE
   the struct is pushed, so the struct push must preserve it (stash via IY).
   These are the patterns that exposed the dropped-register-arg miscompile. */
static u32 sfirst_int(struct s4 s, u16 y)            { return (u32)s.a + s.b + y; }  /* y -> BA */
static u16 sfirst_char(struct s2 s, u8 c)            { return (u16)s.a + s.b + c; }  /* c -> A  */
static u16 sfirst_ptr(struct s2 s, volatile u8 *p)   { return (u16)s.a + s.b + *p; } /* p -> HL */

/* two struct args, both on the stack, different odd sizes */
static u16 twostructs(struct s2 a, struct s3 b)
{
    return (u16)a.a + a.b + b.a + b.b + b.c;
}

/* struct arg AND struct return together (bigreturn hidden pointer + stack arg) */
static struct s8 scale8(struct s8 s)
{
    struct s8 r;
    r.lo = s.lo + 1u;
    r.hi = s.hi ^ 0xFFFFFFFFu;
    return r;
}

/* a small struct returned by value (fits in registers, not bigreturn) */
static struct s4 swap4(struct s4 s)
{
    struct s4 r;
    r.a = s.b;
    r.b = s.a;
    return r;
}

void diff_run(void)
{
    u8 i;

    for (i = 0; i < 4; i++) {
        struct s1 a1 = { v8[i] };
        struct s2 a2 = { v8[i], v8[i + 1] };
        struct s3 a3 = { v8[i], v8[i + 1], v8[i + 2] };
        struct s4 a4 = { v16[i], v16[(u8)(i + 1) & 3] };
        struct sm am = { v8[i], v16[i], v8[i + 1] };
        struct s8 a8;
        struct nst an;

        a8.lo = ((u32)v16[i] << 16) | v16[(u8)(i + 1) & 3];
        a8.hi = ((u32)v16[(u8)(i + 2) & 3] << 16) | v16[(u8)(i + 3) & 3];

        an.p.a = v8[i]; an.p.b = v8[i + 1];
        an.tag = v16[i];
        an.arr[0] = v8[i + 2]; an.arr[1] = v8[i + 3]; an.arr[2] = v8[i + 4];

        EMIT_U16("s1",  sum1(a1));
        EMIT_U16("s2",  sum2(a2));
        EMIT_U16("s3",  sum3(a3));
        EMIT_U32("s4",  sum4(a4));
        EMIT_U32("sm",  summ(am));
        EMIT_U32("s8",  sum8(a8));
        EMIT_U16("sn",  sumn(an));

        /* by-value: clobber returns 0, but a4 must be untouched afterwards */
        EMIT_U16("clob", clobber(a4));
        EMIT_U16("a4keepA", a4.a);
        EMIT_U16("a4keepB", a4.b);

        EMIT_U32("mix", mixargs(v8[i], a4, v16[i]));
        EMIT_U16("two", twostructs(a2, a3));

        /* struct-first-then-scalar: the trailing reg arg must survive the push */
        EMIT_U32("sfi", sfirst_int(a4, v16[i]));
        EMIT_U16("sfc", sfirst_char(a2, v8[i + 3]));
        EMIT_U16("sfp", sfirst_ptr(a2, &v8[i]));

        /* whole-struct assignment (copy), then read members from the copy */
        {
            struct s8 c8 = a8;          /* ldir-style copy */
            struct nst cn = an;
            EMIT_U32("cpLo", c8.lo);
            EMIT_U32("cpHi", c8.hi);
            EMIT_U16("cpTag", cn.tag);
            EMIT_U16("cpA",  cn.p.a);
            EMIT_U16("cpArr2", cn.arr[2]);
        }

        /* struct arg + struct return */
        {
            struct s8 r = scale8(a8);
            EMIT_U32("scLo", r.lo);
            EMIT_U32("scHi", r.hi);
            struct s4 w = swap4(a4);
            EMIT_U16("swA", w.a);
            EMIT_U16("swB", w.b);
        }
    }
}
