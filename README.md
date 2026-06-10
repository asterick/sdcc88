# sdcc88

A retarget of **SDCC 4.5.0** (Small Device C Compiler) to the **Epson S1C88**, the 8-bit core of the
**Pokémon Mini**. sdcc88 reuses SDCC's C frontend and middle-end unchanged and adds one backend port in
`src/s1c88/`. It's an **overlay on upstream SDCC, built with SDCC's own autotools** — `build.sh` fetches
SDCC, drops in our port, registers it, and builds the whole SDK.

> **Status: complete and in maintenance.** The full SDK builds and a multi-bank Pokémon Mini ROM compiles,
> links, and runs end-to-end:
>
> ```bash
> sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx game.min
> ```
>
> Codegen is fully retargeted from its z80 origin to the real S1C88 ISA (the faithful BA+HL register model,
> IY index args, `__far` 3-byte banked pointers, native `DIV`/`MLT`, the MAXIMUM-mode 3-byte CB:PC call
> model). The differential test suite is clean — no known correctness bugs. The bundled libc covers
> `string.h`, `stdlib.h`, `ctype.h`, and `printf`/`sprintf` (`<stdio.h>`). The numbered backlog is complete;
> the port is in maintenance.

The port started as a clone of SDCC's **z80** backend (the z80 register model fits the S1C88 far better than
the earlier stm8 base) and is fully re-pointed to the S1C88 register set, ABI, and instruction encodings.

## Get it

**Prebuilt SDK (Linux x86-64):** grab `sdcc88-sdk-<version>-linux-x64.tar.gz` from the
[Releases page](https://github.com/asterick/sdcc88/releases) (tagged releases; CI also attaches the same
tarball to every green run as the `sdcc88-sdk-linux-x64` artifact). It's relocatable — unpack anywhere and
`bin/sdcc -ms1c88` works with no environment setup. Built by `scripts/package-sdk.sh`, which proves the
staged tree self-contained (compile + link + romgen + emulator run from a temp dir under `env -i`) before
tarring.

## Build

```bash
# one-time deps (Debian/Ubuntu/WSL)
sudo apt-get install -y build-essential flex bison m4 gawk libboost-dev zlib1g-dev

./build.sh                                    # fetch + overlay + patch + configure + build the whole SDK
build/sdcc-4.5.0/src/sdcc --version           # -> "SDCC : s1c88 ... 4.5.0"
```

`./build.sh` builds the complete SDK under `build/sdcc-4.5.0/`: the `sdcc -ms1c88` driver plus `bin/`
(`sdcpp`, `sdas88`, `sdldz80`, `romgen`), the runtime (`crt0.rel`, `s1c88.lib`), and the device headers.
`sdcc -ms1c88 foo.c` preprocesses, compiles, and links for real.

The fast inner loop for codegen work is `./scripts/dev.sh` (overlay `src/s1c88` + `make` the compiler +
codegen smoke). In that loop only the compiler is rebuilt, so feed already-preprocessed C via `--c1mode`:

```bash
printf 'int add1(int x){return x+1;}\n' | \
  build/sdcc-4.5.0/src/sdcc -ms1c88 --c1mode -o out.asm
```

## The toolchain

The binary handoff is SDCC's own `sdas`/`sdld` family, retargeted for the S1C88:

- **`sdas88`** — the assembler: full practical ISA, every form byte-verified against
  `docs/s1c88/instruction-set.md`, with same-module branch relaxation. Doubles as the **codegen validator**
  (`./scripts/validate-s1c88.sh <file.asm>` rejects any form the S1C88 can't encode).
- **`sdldz80`** — the linker: assemble→link plus banked **`bcall`/`bjump`** (the linker resolves and writes
  the target's code bank), with default-on cross-module branch relaxation.
- **`romgen`** (`tools/romgen.c`) — packs the linked banks into a flat `.min` ROM, or (with a `.minx`
  output / `--minx`) into the **MINX debug container**: ROM + binary symbol table + the `.map`/`.noi`/`.cdb`
  artifacts rolled into one ELF-like sectioned binary (`docs/s1c88/minx-format.md`).
- **`crt0.rel` + `s1c88.lib` + `<pm.h>`** — the production startup, support + libc library, and device
  header, installed into the driver's lib/include dirs.

## Try it

```bash
make -C examples/hello run        # builds a real ROM and runs it on the bundled emulator
```

## Tests

`./scripts/run-tests.sh` builds once and runs every suite (TAP): byte-identical codegen (`corpus-check`),
host-vs-emulator differential (`diff-test`), on-emulator execution (`emu-test`), plus the toolchain smokes.
`main` is protected — a PR can't merge unless this `ci` check is green.

## Layout

- `src/s1c88/` — the SDCC backend port (z80 clone, retargeted). See `src/s1c88/README.md`.
- `sdas/as88/` — the S1C88 assembler backend (overlaid into SDCC's `sdas` tree).
- `device/` — the production runtime: `lib/s1c88/crt0.s`, the repo-owned libc sources, `include/s1c88/pm.h`.
- `examples/hello/` — a copy-me Pokémon Mini ROM template.
- `build.sh` — fetch/overlay/build orchestrator; `scripts/` — dev, validate, and toolchain build/test scripts.
- `third_party/sdcc/*.patch` — register the port + the banked-branch changes to shared `sdas`/`sdld` sources.
- `docs/s1c88/` — how the compiler and processor work: distilled Epson references (architecture, ISA,
  addressing, memory model, toolchain) plus the design docs `abi-decision.md` (authoritative ABI),
  `building-roms.md` (end-user guide), `banked-branch.md`, and `sdas88-retarget.md`.

## License

SDCC is GPL-2.0-or-later; as an SDCC derivative, sdcc88 is GPL-compatible.
