/* harness.h — differential codegen test harness for the sdcc88 S1C88 port.
 *
 * THE IDEA: one source (tests/diff/cases/*.c, each defining diff_run()) is
 * compiled TWICE — once natively by the host compiler (the reference) and once
 * by sdcc88 and run on the vendored minimon emulator (the candidate). Each test
 * emits its results as "tag:hexvalue\n" text; scripts/diff-test.sh diffs the two
 * streams. Any divergence is a miscompile (or an unsound test — see below).
 *
 * WHY HEX TEXT: the emulator char-out mailbox (0x1FF8) cannot transmit 0x00 (the
 * runner polls `if (c)`), so raw binary would drop zero bytes. Hex + newline is
 * zero-free, deterministic, and the diff is human-readable (tag pinpoints which
 * computation diverged).
 *
 * SOUNDNESS (load-bearing — host int is 32-bit, S1C88 int is 16-bit):
 *   - Width-exact typedefs on both sides (stdint on host, native on target), so
 *     u16/i16/u32/i32 name the SAME bit widths everywhere.
 *   - Every EMIT_* TRUNCATES its argument to the declared width. Truncation of
 *     +,-,*,/,%,&,|,^,~,<<,>> to a fixed width is identical on host and target
 *     regardless of the intermediate promotion width, so truncated emits are
 *     sound. (Signed / and % are sound too when operands fit the width and the
 *     MIN/-1 overflow is excluded.)
 *   - To test WIDE semantics (e.g. a real 32-bit multiply), widen the OPERANDS
 *     explicitly: EMIT_U32("..", (u32)a * (u32)b). Emitting a wide result of a
 *     narrow-promoted op WITHOUT widening operands is UNSOUND (host computes it
 *     in 32-bit int, target in 16-bit) — don't.
 *   - Do NOT emit cross-signedness or mixed-rank comparisons (i16 vs u16, etc.):
 *     the host promotes both to 32-bit int (signed compare) while the target,
 *     where u16 has int rank, does an UNSIGNED compare. That divergence is C
 *     semantics, not a codegen bug. Compare same-typed operands only.
 */
#ifndef DIFF_HARNESS_H
#define DIFF_HARNESS_H

#ifdef DIFF_HOST
#include <stdio.h>
#include <stdint.h>
typedef uint8_t  u8;  typedef int8_t  i8;
typedef uint16_t u16; typedef int16_t i16;
typedef uint32_t u32; typedef int32_t i32;
typedef float    f32;     /* IEEE 754 binary32 — same layout as the target's float */
static void diff_putc(char c) { putchar((unsigned char)c); }
#else
/* sdcc88: char=8, int=16, long=32 — width-exact to the host typedefs above */
typedef unsigned char u8;  typedef signed char i8;
typedef unsigned int  u16; typedef int          i16;
typedef unsigned long u32; typedef long         i32;
typedef float         f32;
#define DIFF_CHAR_OUT (*(volatile unsigned char *)0x1FF8)
static void diff_putc(char c) { DIFF_CHAR_OUT = (unsigned char)c; }
#endif

static void diff_puts(const char *s) { while (*s) diff_putc(*s++); }

/* Primitive emitters take a SINGLE argument each — never a char alongside a
   stacked long. (An earlier diff_emit(tag, u32 v, u8 nbytes) tripped a backend
   bug where the char `nbytes` was dropped/clobbered by the stacked long `v`'s
   staging under register pressure; see docs HANDOFF "Session 24". Keeping each
   helper to one scalar/pointer arg sidesteps that ABI hazard so the harness can
   test the rest of the codegen.) Bytes are extracted in the EMIT_* macros. */
static void diff_tag(const char *tag) { diff_puts(tag); diff_putc(':'); }

static void diff_hex2(u8 b)
{
    static const char hexd[] = "0123456789abcdef";
    diff_putc(hexd[(b >> 4) & 0xF]);
    diff_putc(hexd[b & 0xF]);
}

static void diff_nl(void) { diff_putc('\n'); }

/* Per-width emitters: each takes (tag, value) only — a pointer + a long, never a
   char alongside the long, so the byte extraction (which would otherwise pass a
   char arg) stays inside these functions and off the call boundary. The EMIT_*
   macros are then a single call each (compact — large test functions stay within
   the jrs branch range). */
static void diff_e1(const char *tag, u32 v) { diff_tag(tag); diff_hex2((u8)v); diff_nl(); }
static void diff_e2(const char *tag, u32 v) { diff_tag(tag); diff_hex2((u8)(v >> 8)); diff_hex2((u8)v); diff_nl(); }
static void diff_e4(const char *tag, u32 v)
{
    diff_tag(tag);
    diff_hex2((u8)(v >> 24)); diff_hex2((u8)(v >> 16)); diff_hex2((u8)(v >> 8)); diff_hex2((u8)v);
    diff_nl();
}

/* tag:hh[hh[hhhh]]\n  big-endian hex; each macro truncates to its width (the
   two's-complement bit pattern), identical on host and target — see SOUNDNESS. */
#define EMIT_U8(tag, x)  diff_e1((tag), (u32)(u8)(x))
#define EMIT_I8(tag, x)  diff_e1((tag), (u32)(u8)(i8)(x))
#define EMIT_U16(tag, x) diff_e2((tag), (u32)(u16)(x))
#define EMIT_I16(tag, x) diff_e2((tag), (u32)(u16)(i16)(x))
#define EMIT_U32(tag, x) diff_e4((tag), (u32)(x))
#define EMIT_I32(tag, x) diff_e4((tag), (u32)(i32)(x))

/* EMIT_F32 emits the 32-bit IEEE-754 bit pattern of a float, so the comparison is
   BIT-EXACT. Both sides store binary32 little-endian, so reinterpreting the float
   as a u32 yields the same value; diff_e4 then prints it big-endian.
   SOUNDNESS (float): only emit operations whose operands AND result are EXACTLY
   representable in binary32 (e.g. small integers, halves, quarters, exact quotients
   like 10.0/4.0). SDCC's softfloat is not bit-IEEE for results that need rounding,
   so a rounding-dependent result would diverge as a *library* difference, not a
   codegen bug. Exact-value ops still fully exercise the float ABI / support-call
   wiring / byte order — which is what we're testing. */
static void diff_ef(const char *tag, f32 v)
{
    union { f32 f; u32 u; } pun;
    pun.f = v;
    diff_e4(tag, pun.u);
}
#define EMIT_F32(tag, x) diff_ef((tag), (f32)(x))

void diff_run(void);

#endif /* DIFF_HARNESS_H */
