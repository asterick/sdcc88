# sdcc88

A retarget of **SDCC 4.5.0** (Small Device C Compiler) to the **Epson S1C88**, the 8-bit core of the
**Pokémon Mini**. sdcc88 reuses SDCC's C frontend and middle-end and adds a backend port in `src/s1c88/`.
It's an **overlay on upstream SDCC, built with SDCC's own autotools** — `build.sh` fetches SDCC, drops in
our port, registers it, and builds.

> **Status: the compiler builds and runs.** `sdcc -ms1c88` is a working SDCC driver with our port. The
> code generation is still STM8 (the port is a renamed clone of SDCC's `stm8` port) and must be retargeted
> to the S1C88; the assembly→binary handoff is also still open. See [`CLAUDE.md`](CLAUDE.md).

## Build

```bash
# one-time deps (Debian/Ubuntu/WSL)
sudo apt-get install -y build-essential flex bison m4 gawk libboost-dev zlib1g-dev

./build.sh                                    # fetch + overlay + patch + configure + make
build/sdcc-4.5.0/src/sdcc --version           # -> "SDCC : s1c88 ... 4.5.0"
build/sdcc-4.5.0/src/sdcc -ms1c88 -S foo.c    # compile C to S1C88 (currently STM8) assembly
```

`build.sh` builds the compiler, not SDCC's `sdcpp` preprocessor, so a full `sdcc foo.c` isn't wired
end-to-end yet (see CLAUDE.md).

## Layout

- `src/s1c88/` — the SDCC backend port (cloned from `stm8`, renamed). See `src/s1c88/README.md`.
- `build.sh` — fetch/overlay/build orchestrator.
- `third_party/sdcc/register_s1c88_port.patch` — registers the port in SDCC's core.
- `docs/s1c88.md` — S1C88 ISA references.

## Related

- [`../skiploom`](../skiploom) — the S1C88 assembler/linker (AS88-compatible); source of the opcode table
  (`src/util/s1c88.csv`) and CPU manual (`docs/id000920.pdf`) used as the encoding reference here.

## License

SDCC is GPL-2.0-or-later; as an SDCC derivative, sdcc88 is GPL-compatible.
