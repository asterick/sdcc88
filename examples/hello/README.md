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

`hello.c` prints a greeting and a number whose decimal conversion uses the runtime
divide/modulo from `s1c88.lib`, then returns 0. Real hardware has no text console
(you'd draw to the LCD via the PRC); the program writes to the emulator's debug
console at `0x1FF8` so `make run` shows output. A real game would loop forever in
`main()` instead of returning.

**Interrupts:** this program defines no handlers, and it still links — the crt0's
per-vector trampolines `bjump _irq_v<N>`, and the runtime library defaults each
vector to a redirect to the shared `_irq_default` handler (a do-nothing `rete`).
To handle one vector, define `void irq_v<N>(void) __interrupt` (the `VEC_*`
numbers are in `<pm.h>`); it overrides just that vector. To catch every otherwise-
unhandled vector in one place, define `void irq_default(void) __interrupt`. See
`docs/s1c88/building-roms.md` §5.

See [`docs/s1c88/building-roms.md`](../../docs/s1c88/building-roms.md) for the full
guide and [`<pm.h>`](../../device/include/s1c88/pm.h) for the register map.
