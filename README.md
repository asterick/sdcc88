# sdcc88

A retarget of **SDCC 4.5.0** (Small Device C Compiler) to the **Epson S1C88**, the 8-bit core of the
**Pokémon Mini**. sdcc88 reuses SDCC's C frontend and middle-end unchanged and adds one backend port in
`src/s1c88/`. It's an **overlay on upstream SDCC, built with SDCC's own autotools** — `build.sh` fetches
SDCC, drops in our port, registers it, and builds.

> **Status: the compiler builds, links, and runs, and the binary toolchain is complete.**
> `sdcc -ms1c88` is a working SDCC driver with our port. The **assembler, linker, and ROM packer are
> done**, and a multi-bank Pokémon Mini ROM builds end-to-end. **Code generation is being retargeted**
> from its z80 origin to the real S1C88 ISA — the frame, branches, compares, 16-bit ALU, and shifts are
> done; the remaining work is the C/D/E + DE/BC register-model cleanup. See [`CLAUDE.md`](CLAUDE.md) and
> [`docs/s1c88/HANDOFF.md`](docs/s1c88/HANDOFF.md).

The port is a clone of SDCC's **z80** backend (the z80 register model fits the S1C88 far better than the
earlier stm8 base), re-pointed to the S1C88 register set, ABI, and instruction encodings.

## Build

```bash
# one-time deps (Debian/Ubuntu/WSL)
sudo apt-get install -y build-essential flex bison m4 gawk libboost-dev zlib1g-dev

./build.sh                                    # fetch + overlay + patch + configure + make
build/sdcc-4.5.0/src/sdcc --version           # -> "SDCC : s1c88 ... 4.5.0"
```

`build.sh` builds the **compiler**, not SDCC's `sdcpp` preprocessor, so `sdcc foo.c` can't preprocess.
Feed already-preprocessed C via `--c1mode` (reads cpp'd C on stdin, emits asm):

```bash
printf 'int add1(int x){return x+1;}\n' | \
  build/sdcc-4.5.0/src/sdcc -ms1c88 --c1mode -o out.asm
```

The fast inner loop is `./scripts/dev.sh` (overlay `src/s1c88` + `make` + a codegen smoke test).

## The toolchain (complete)

The binary handoff is SDCC's own `sdas`/`sdld` family, retargeted for the S1C88:

- **`sdas88`** — the assembler (`sdas/as88/`): full practical ISA, every form byte-verified against
  `docs/s1c88/instruction-set.md`. Built with `./scripts/build-sdas.sh as88`. It doubles as the **codegen
  validator** — `./scripts/validate-s1c88.sh <file.asm>` assembles emitted asm and reports any form the
  S1C88 can't encode (i.e. the remaining z80-isms).
- **`sdldz80`** — the linker (`./scripts/build-sdld.sh`): assemble→link, plus banked **`bcall`/`bjump`**
  pseudo-ops whose code bank the linker resolves and writes (`ld nb,#bank`).
- **`scripts/romgen.py`** — packs the linked banks into a flat `.min` ROM.

End-to-end checks: `./scripts/link-smoke.sh` (assemble→link) and `./scripts/rom-smoke.sh`
(assemble→link→romgen, a multi-bank ROM).

## Layout

- `src/s1c88/` — the SDCC backend port (z80 clone, retargeted). See `src/s1c88/README.md`.
- `sdas/as88/` — the S1C88 assembler backend (overlaid into SDCC's `sdas` tree).
- `build.sh` — fetch/overlay/build orchestrator; `scripts/` — dev, validate, and toolchain build/smoke scripts.
- `third_party/sdcc/register_s1c88_port.patch` — registers the port in SDCC's core.
- `third_party/sdcc/s1c88_banked_branch.patch` — the banked-branch changes to shared `sdas`/`sdld` sources.
- `docs/s1c88/` — distilled Epson references (architecture, ISA, addressing, memory model, ABI, toolchain)
  plus the design docs: `abi-decision.md`, `sdas88-retarget.md`, `banked-branch.md`, and `HANDOFF.md`
  (the resume entry point).

## License

SDCC is GPL-2.0-or-later; as an SDCC derivative, sdcc88 is GPL-compatible.
