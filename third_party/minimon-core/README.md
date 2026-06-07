# minimon-core (vendored)

The Pokémon Mini emulator core from [minimon.js](https://github.com/asterick/minimon.js)
(Bryon Vandiver, ISC license — see `LICENSE`), vendored here as the ground-truth S1C88
machine for the sdcc88 codegen test harness (`tests/emu/`).

Vendored from minimon.js commit `c3c3c9336111e1e0482a1138e583c686c3b1fb36`:

- `src/` — the emulator core (`core/src/*.cc`): CPU, IRQ, timers, LCD, blitter, …
- `include/` — its headers (`core/include/*.h`), including the embedded free BIOS image
- `tools/` — `table.py` + `s1c88.csv`, which generate the opcode dispatch `table.h`
  at build time (done by `tests/emu/Makefile` into `build/emu/`)

The core is plain C++17 with no dependencies; the host glue it expects
(`get_machine`, `debug_print`, `flip_screen`, `audio_push`) is provided by
`tests/emu/runner.cc`. To refresh the vendored copy, re-copy those three
directories from a minimon.js checkout and update the commit hash above.
