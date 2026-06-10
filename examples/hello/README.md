# hello — minimal sdcc88 Pokémon Mini ROM

A copy-me template for a Pokémon Mini ROM built with sdcc88. It uses the device
header (`<pm.h>`) and the C runtime (the support library is auto-linked).

## Build

```bash
make            # -> hello.min  (a flat Pokémon Mini ROM)
make run        # build + run on the bundled emulator
make clean
```

`make run` prints:

```
Hello, Pokemon Mini!
1000 / 7 = 142
exit=0
```

`make` runs two steps:

```
sdcc -ms1c88 --opt-code-size hello.c -o hello.ihx   # compile + link (crt0 + s1c88.lib)
romgen hello.ihx hello.min                          # Intel-HEX -> flat .min ROM
```

`hello.min` boots on PokeMini and real hardware (byte 0 = physical `0x2100`, the
`"PM"` cartridge header).

## Using a different SDK location

The Makefile defaults `SDK` to the in-tree build (`../../build/sdcc-4.5.0`). Point it
elsewhere for an installed SDK:

```bash
make SDK=/opt/sdcc88
```

## What the program does

`hello.c` greets the world with `printf` and prints a number whose `%u` conversion
uses the runtime divide/modulo from `s1c88.lib`, then returns 0. `printf` goes
through the library's default `putchar`, which stores each byte to the minimon
debug console (`DEBUG_OUT`, RAM `0x1FF8`) so `make run` shows the text. Real
hardware has no text console — define your own `putchar` (drawing to the LCD via
the PRC) to override the default. A real game would loop forever in `main()`
instead of returning.

**Interrupts:** as a worked example the program defines a single handler for the A
button — `void key_a(void) __interrupt(VEC_KEYA)`, which the compiler auto-wires to
that cart vector slot; `main()` arms it (priority + `IRQ_ENA3` unmask + lowering the
CPU interrupt-priority level). The other 25 cart vectors fall through to the
runtime's do-nothing `rete` defaults (a program that handles none still links). The
`VEC_*` slot constants are in `<pm.h>`; see `docs/s1c88/building-roms.md` §5.

See [`docs/s1c88/building-roms.md`](../../docs/s1c88/building-roms.md) for the full
guide and [`<pm.h>`](../../device/include/s1c88/pm.h) for the register map.
