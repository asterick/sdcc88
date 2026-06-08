# hello — minimal sdcc88 Pokémon Mini ROM

A copy-me template for a Pokémon Mini ROM built with sdcc88. It uses the device
header (`<pm.h>`) and the C runtime (the support library is auto-linked).

## Build

```bash
make            # -> hello.min  (a flat Pokémon Mini ROM)
make run        # build + run on the bundled emulator (prints exit=42)
make clean
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

`hello.c` configures the display (PRC), reads the key pad, and does a little
arithmetic (the division pulls `__divsint` from `s1c88.lib`). It returns `42` so
`make run` can verify the toolchain end to end. A real game would loop forever in
`main()` instead of returning.

See [`docs/s1c88/building-roms.md`](../../docs/s1c88/building-roms.md) for the full
guide and [`<pm.h>`](../../device/include/s1c88/pm.h) for the register map.
