/* 18_structarg2 — struct-by-value argument followed by TWO register args
 * (the FIXED #16 genPointerPush "both pairs parked" boundary).
 *
 * THE FIXED LIMITATION: pushing a struct by value needs HL as the walk pointer
 * and A/B as the byte/word vehicle.  When the struct is the FIRST argument, the
 * later scalar args are SENT into the register pairs first, so by the time the
 * struct is pushed BOTH HL and BA can hold a parked sent value.  The old code
 * could stash only ONE parked pair (into the dead IY) and hit `UNIMPLEMENTED`
 * (a FATAL compiler-internal error) on calls like f(struct, char, int) or
 * f(struct, int, int) — the one UNIMPLEMENTED site reachable in real emit.
 *
 * THE FIX: stash the first parked pair in IY and the second in the near-RAM
 * __sdcc_fptr scratch cell (free during the push — it is only loaded at an
 * indirect call's dispatch, after BA has been restored).  Both register args
 * are preserved across the struct push and arrive intact.
 *
 * The single-parked forms f(struct, int) / f(u8, struct, int) already worked;
 * this case locks the two-parked forms.
 */
#include "emu.h"

typedef unsigned char u8;

struct s3 { int a, b, c; };

static int g_a, g_b, g_c;
static u8  g_ch;
static int g_iw;

/* struct (by value) + char + int  ->  parks A (char) and HL (int) */
static void f_ci (struct s3 v, u8 ch, int iw)
{
    g_a = v.a; g_b = v.b; g_c = v.c;
    g_ch = ch; g_iw = iw;
}

/* struct (by value) + int + int  ->  parks BA and HL (the comment's example) */
static void f_ii (struct s3 v, int p, int q)
{
    g_a = v.a; g_b = v.b; g_c = v.c;
    g_iw = p; g_ch = (u8) q;
}

int main (void)
{
    struct s3 x = { 0x1111, 0x2222, 0x3333 };

    f_ci (x, 0x5A, 0x1234);
    CHECK (g_a == 0x1111 && g_b == 0x2222 && g_c == 0x3333);
    CHECK (g_ch == 0x5A);
    CHECK (g_iw == 0x1234);

    g_a = g_b = g_c = g_iw = 0; g_ch = 0;
    f_ii (x, 0x4321, 0x00BE);
    CHECK (g_a == 0x1111 && g_b == 0x2222 && g_c == 0x3333);
    CHECK (g_iw == 0x4321);
    CHECK (g_ch == 0xBE);

    emu_puts ("structarg2 ok\n");
    EMU_DONE ();
}
