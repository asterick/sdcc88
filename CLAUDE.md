# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **▶ Resuming / "pick up where you left off"?** Go to **[`docs/s1c88/HANDOFF.md`](docs/s1c88/HANDOFF.md)** —
> it has the current state and the exact next action. TL;DR: `git checkout s1c88-retarget && ./scripts/dev.sh`
> (confirms the build is green + runs a codegen smoke test), then continue the codegen retarget per
> `docs/s1c88/abi-decision.md`.

## Project Overview

**sdcc88** retargets [SDCC](https://sdcc.sourceforge.net/) (the Small Device C Compiler) **4.5.0** to the
**Epson S1C88** — the 8-bit core of the **Pokémon Mini**. It inherits SDCC's C frontend and
machine-independent middle-end unchanged and adds one backend code-generator **port** in `src/s1c88/`.

sdcc88 is an **overlay on upstream SDCC, built with SDCC's own autotools build** (`configure` + `make`) —
*not* a reimplemented build system. `build.sh` fetches SDCC, drops our port into its tree, registers it,
and builds the compiler.

> **Status:** the compiler **builds, links, and runs** as a stock SDCC `sdcc` driver with `-ms1c88`
> selectable. **`src/s1c88` is a clone of SDCC's `z80` port** (re-based from an earlier stm8 clone — the
> z80 register model fits the S1C88 far better). **Codegen is still z80-flavored**; retargeting it to the
> S1C88 ISA is in progress. Work happens on the **`s1c88-retarget`** branch. The design, ABI, and the
> step-by-step plan live in **`docs/s1c88/abi-decision.md`** — read it before touching `src/s1c88/`.

## Current work: the codegen retarget (read `docs/s1c88/abi-decision.md`)

Decided design:
- **Register model = Faithful BA+HL:** byte GPRs `A,B,L,H`; pairs `BA`(B:A) and `HL`(H:L); index `IX,IY`
  (not byte-addressable). Drop the z80 `C,D,E,DE` — S1C88 has no DE, and `A` doubles as BA's low byte.
- **Asm output = keep SDCC sdas style** (reuse the sdas/sdld assembler+linker family).
- **Execution = always-green incremental:** keep every register symbol *defined* so the build never
  breaks; constrain the allocator to A/B/L/H, then rewrite the `DE`/`BC` scratch uses in `gen.c`
  function-by-function, **building + smoke-testing after each batch**, deleting symbols only once unused.
  (A from-scratch big-bang reshape was tried and reset — it breaks ~1144 `gen.c` sites at once with no way
  to verify; the dead WIP is in reflog at `417bed5` as a reference for the end-state register defs.)

The remaining grind is the `gen.c` scratch-asmop machinery (`asmop_bc/de`, the combined long asmops
`DEHL/HLDE/HLBC/DEBC`, `_pairs[]`, the `[IYH_IDX+1]` parm-mask arrays). See `abi-decision.md` for the map.

## Build

One-time dependencies (Debian/Ubuntu/WSL): `sudo apt-get install -y build-essential flex bison m4 gawk
libboost-dev zlib1g-dev`.

```bash
./build.sh           # fetch (cached) + overlay + patch + configure + make
./build.sh --fresh   # wipe build/ and rebuild from scratch (needed when the patch changes)
./scripts/dev.sh      # overlay current src/s1c88 + make + run the smoke test (fast inner-loop)
```

Result: `build/sdcc-4.5.0/src/sdcc` — a normal SDCC compiler driver that knows `-ms1c88`.

**Smoke-testing codegen** — `build.sh` builds the **compiler**, not SDCC's bundled `sdcpp` preprocessor,
so `sdcc foo.c` can't preprocess. Feed already-preprocessed C via `--c1mode` (reads cpp'd C on stdin,
emits asm):

```bash
build/sdcc-4.5.0/src/sdcc --version                                    # -> "SDCC : s1c88 ... 4.5.0"
printf 'int add1(int x){return x+1;}\n' | \
  build/sdcc-4.5.0/src/sdcc -ms1c88 --c1mode -o out.asm                # compile to (currently z80-ish) asm
```

> The build (compile + link) and running the freshly-built `sdcc` both work **inside the Claude Code
> sandbox** — you can iterate without `! ...`. Only `bzip2`/`sdcpp`-style not-built helpers are
> unavailable. `./scripts/dev.sh` does build + smoke-test in one shot.

## How `build.sh` works (the overlay)

1. **Fetch** SDCC 4.5.0 from SourceForge (cached in `build/`, sha256-verified; a direct mirror host is
   used because the `/download` URL serves a consent page). Extraction uses Python `tarfile` (no `bzip2`).
2. **Overlay** `src/s1c88/*` (`.c .h .cc .i`, `peeph*.def`, `Makefile.in`) into `build/.../src/s1c88/`.
3. **Register** the port via `third_party/sdcc/register_s1c88_port.patch` — adds `TARGET_ID_S1C88`,
   `TARGET_IS_S1C88`, **`S1C88` to `TARGET_Z80_LIKE`** (SDCC core gates real behavior on it; the z80
   codegen needs it), `extern PORT s1c88_port`, and the `_ports[]` entry. **Applied with `patch -p1`, NOT
   `git apply`** — `build/` is nested inside sdcc88's own git repo, so `git apply` resolves paths against
   the outer repo and **silently no-ops (exit 0)**, leaving the build to fail later with
   `TARGET_ID_S1C88 undeclared`. `build.sh` now hard-fails if the patch didn't land. Regenerate the patch
   with `git diff` against pristine extracted sources — never hand-edit hunk headers.
4. **Configure** with *all stock ports disabled* (`--disable-*-port`, incl. all z80 variants) plus
   device-lib/ucsim/sdcdb/etc. off. Disabling z80 is required: our port is its clone and would otherwise
   collide on non-`z80`-named globals at link time.
5. **Inject** the port: append `s1c88` to `ports.build`/`ports.all` and generate `src/s1c88/Makefile`
   from `Makefile.in` via `./config.status --file=...`.
6. **`make -C src`** builds the `sdcc` driver. `port.mk` turns every `*.def` into a `*.rul` via
   `gawk -f ../SDCCpeeph.awk`; `main.c` `#include`s `peeph.rul` + `peeph-z80.rul`.

## The S1C88 port (`src/s1c88/`)

Cloned from SDCC 4.5.0's `src/z80` (file `z80.h`→`s1c88.h`; the registered `z80_port`→`s1c88_port` with
`.target="s1c88"`). The other 9 z80 variant PORT structs were pruned; it's a single-variant port that runs
the plain-z80 codegen path (`z80_opts.sub == SUB_Z80`, so `IS_Z80` is true). Many internal identifiers
keep their z80 names (`z80_regs`, `genZ80Code`, `z80_opts`, the `IS_*`/`PAIR_*` machinery) — they're
port-internal and harmless (the real z80 port is disabled, so no collision).

Per-file roles: `main.c` (the `PORT s1c88_port` struct + options), `gen.c`/`gen.h` (iCode → asm, the
bulk), `ralloc.c`/`ralloc.h` + `ralloc2.cc` (register allocation; `ralloc2.cc` instantiates SDCC's generic
tree-decomposition allocator in `SDCCralloc.hpp`, which needs Boost), `s1c88.h` (sub-port enum + `IS_*`
macros), `mappings.i` (asm-dialect tables), `peep.c`/`peeph*.def` (peephole). See `src/s1c88/README.md`.

## Reference material

**`docs/s1c88/`** is the authoritative, distilled reference (from the Epson official PDFs): architecture,
memory model, addressing modes, instruction set, the C ABI, and the assembler/linker/locator toolchain.
Start at `docs/s1c88/README.md`; the backend decisions are in `docs/s1c88/abi-decision.md`.

## Conventions

- Work on the **`s1c88-retarget`** branch. Commit green checkpoints; clearly label any intentionally-red
  WIP. The build working in-sandbox means you can (and should) verify each change before committing.
- Convert the design/strategy in `docs/s1c88/abi-decision.md` into action — keep it current as decisions
  evolve. Keep the auto-loaded memory (`sdcc88-bringup-status`) accurate; it's the fastest way to resume.
