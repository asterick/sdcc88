# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**skip-c** retargets [SDCC](https://sdcc.sourceforge.net/) (the Small Device C Compiler) **4.5.0** to the
**Epson S1C88** — the 8-bit core of the **Pokémon Mini**. It inherits SDCC's C frontend and
machine-independent middle-end unchanged and adds one backend code-generator **port** in `src/s1c88/`.

skip-c is an **overlay on upstream SDCC, built with SDCC's own autotools build** (`configure` + `make`) —
*not* a reimplemented build system. `build.sh` fetches SDCC, drops our port into its tree, registers it,
and builds the compiler.

The sibling project [`../skiploom`](../skiploom) is the existing S1C88 assembler/linker (JavaScript,
AS88-compatible); its instruction table and CPU manual are the encoding reference here.

> **Status: the compiler builds, links, and runs** as a stock SDCC `sdcc` driver with the `-ms1c88` port
> selectable. **The code generation is still STM8** — `src/s1c88` is a renamed clone of SDCC's `stm8`
> port, so it emits STM8, not S1C88. Retargeting the backend, and the assembly→binary handoff, are the
> remaining work (see *Open Decisions*).

## Build

One-time dependencies (Debian/Ubuntu/WSL):

```bash
sudo apt-get install -y build-essential flex bison m4 gawk libboost-dev zlib1g-dev
```

Then:

```bash
./build.sh           # fetch (cached) + overlay + patch + configure + make
./build.sh --fresh   # wipe build/ and rebuild from scratch
```

Result: `build/sdcc-4.5.0/src/sdcc` — a normal SDCC compiler driver that knows `-ms1c88`:

```bash
build/sdcc-4.5.0/src/sdcc --version          # -> "SDCC : s1c88 ... 4.5.0"
build/sdcc-4.5.0/src/sdcc -ms1c88 -S foo.c   # compile C to S1C88 (currently STM8) assembly
```

> `build.sh` builds the **compiler** (`make -C src`), not SDCC's bundled `sdcpp` preprocessor (a
> heavyweight GCC-derived tree). So a full `sdcc foo.c` run isn't wired end-to-end yet — that needs a
> preprocessor plus the assembly→binary handoff (an open decision). Also, the Claude Code sandbox can't
> exec freshly built helper binaries, so any end-to-end runs should be done in a normal shell (`! ...`).

## How `build.sh` works (the overlay)

1. **Fetch** SDCC 4.5.0 source from SourceForge (cached in `build/`, sha256-verified). SourceForge's
   canonical `/download` URL serves a consent page to non-browsers, so a direct mirror host is used.
   Extraction is done with Python (`tarfile`) to avoid depending on a `bzip2` binary.
2. **Overlay** `src/s1c88/*` into `build/sdcc-4.5.0/src/s1c88/`.
3. **Register** the port in SDCC's core via `third_party/sdcc/register_s1c88_port.patch` (adds
   `TARGET_ID_S1C88`, `TARGET_IS_S1C88`, `extern PORT s1c88_port` to `src/port.h`, and the `&s1c88_port`
   entry to `src/SDCCmain.c`'s `_ports[]`).
4. **Configure** with *all stock ports disabled* (`--disable-*-port`) plus device-lib/ucsim/sdcdb/etc.
   off — skip-c is s1c88-only. Disabling stm8 is required: our port is its clone and would otherwise
   collide on non-`stm8`-named globals (`swap_to_a`, `adjustRegW`, …) at link time.
5. **Inject** the port into the configured build, since `configure` doesn't know it: append `s1c88` to
   `ports.build`/`ports.all`, and generate `src/s1c88/Makefile` from `Makefile.in` via
   `./config.status --file=...` (no donor port needed).
6. **`make -C src`** builds the `sdcc` compiler driver. The port's `peeph.rul` is generated from
   `peeph.def` by SDCC's `port.mk` rule (`gawk -f ../SDCCpeeph.awk`).

## The S1C88 port (`src/s1c88/`)

Cloned from SDCC 4.5.0's `src/stm8` with a blanket identifier rename (`stm8`→`s1c88`, `STM8`→`S1C88`).
`.target` is `s1c88`, so `-ms1c88` selects it. See `src/s1c88/README.md` for the per-file roles and the
**retargeting checklist** (replace the STM8 register set, instruction selection, peephole rules, and
calling convention with S1C88 ones). Drive that work from the encoding sources in `../skiploom`
(`src/util/s1c88.csv`, `docs/id000920.pdf`); see `docs/s1c88.md`.

SDCC porting model: each backend defines one global `struct PORT` (here `s1c88_port` in `main.c`); the
per-port files are `main.c` (the PORT struct + options), `gen.c`/`gen.h` (iCode → asm — the bulk),
`ralloc.c`/`ralloc.h` + `ralloc2.cc` (register allocation; `ralloc2.cc` instantiates SDCC's generic
tree-decomposition allocator in `SDCCralloc.hpp`, which needs Boost), and `peeph.def` (peephole rules).

## Open Decisions (not settled)

- **Assembler/linker handoff (asm → binary):** deferred. The `sdcc` driver currently emits assembly only.
  Candidates: SDCC's own `sdas`/`sdld`, the skiploom toolchain, or direct emission.
- **Retargeting the backend:** the cloned codegen is STM8; it must be rewritten for the S1C88 ISA.
- **Keeping s1c88 a clone vs. a from-scratch port:** for now it's a clone to get a working baseline.

## Relationship to skiploom (`../skiploom`)

skiploom is a JS S1C88 assembler/linker (AS88 syntax). It matters here as the **encoding reference**
(`src/util/s1c88.csv` opcode table, `docs/id000920.pdf` manual) and as a possible **assembly consumer** if
skip-c emits AS88 text.
