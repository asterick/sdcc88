/* emu.h — guest-side test helpers for the emulator harness (tests/emu).
 *
 * Cases are preprocessed with the HOST cpp (cc -E -P, same as corpus-check.sh)
 * before being fed to sdcc --c1mode, so #include/#define work normally.
 *
 * Protocol (mirrors runner.cc + crt0.asm): a store to 0x1FF8 prints one char on
 * the host (the runner polls between every instruction, so back-to-back stores
 * are never lost); main()'s return value becomes the host process exit code,
 * 0 = pass. CHECK() prints the failing expression and counts it.
 */
#ifndef EMU_H
#define EMU_H

#define EMU_CHAR_OUT (*(volatile unsigned char *)0x1FF8)

static void emu_putc(char c) { EMU_CHAR_OUT = (unsigned char)c; }

static void emu_puts(const char *s)
{
    while (*s)
        emu_putc(*s++);
}

static void emu_puthex16(unsigned int v)
{
    static const char digits[] = "0123456789abcdef";
    emu_putc(digits[(v >> 12) & 0xF]);
    emu_putc(digits[(v >> 8) & 0xF]);
    emu_putc(digits[(v >> 4) & 0xF]);
    emu_putc(digits[v & 0xF]);
}

static int emu_failures;

#define CHECK(cond)                                \
    do {                                           \
        if (!(cond)) {                             \
            emu_failures++;                        \
            emu_puts("FAIL: " #cond "\n");         \
        }                                          \
    } while (0)

#define EMU_DONE() return emu_failures

#endif
