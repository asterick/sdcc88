# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **▶ Resuming / "pick up where you left off"?** Go to **[`docs/s1c88/HANDOFF.md`](docs/s1c88/HANDOFF.md)** —
> it has the current state and the exact next action. TL;DR: `./scripts/dev.sh` confirms the build is
> green; `scripts/corpus-check.sh` + `scripts/emu-test.sh` + `scripts/diff-test.sh` are the gates.
> **Always rebuild via the overlay (`dev.sh`/`corpus-check.sh`), never raw `make -C build/.../src`**
> (it compiles a stale copy). The forward work list is **[`docs/s1c88/TODO.md`](docs/s1c88/TODO.md)**.
> All work is on **`main`**.

## Project Overview

**sdcc88** retargets [SDCC](https://sdcc.sourceforge.net/) (the Small Device C Compiler) **4.5.0** to the
**Epson S1C88** — the 8-bit core of the **Pokémon Mini**. It inherits SDCC's C frontend and
machine-independent middle-end unchanged and adds one backend code-generator **port** in `src/s1c88/`.

sdcc88 is an **overlay on upstream SDCC, built with SDCC's own autotools build** (`configure` + `make`) —
*not* a reimplemented build system. `build.sh` fetches SDCC, drops our port into its tree, registers it,
and builds the compiler.

> **Status:** the full toolchain is **complete and usable** — `sdcc -ms1c88 game.c -o game.ihx &&
> romgen game.ihx game.min` builds a bootable Pokémon Mini ROM. `src/s1c88` started as a clone of
> SDCC's `z80` port (the z80 register model fits the S1C88 well) and the codegen has been retargeted to
> the real S1C88 ISA, always-green incremental. The binary toolchain — `sdas88` (full ISA, byte-verified,
> also the codegen validator), `sdldz80` (assemble→link + **banked `bcall`/`bjump`**, linker-resolved bank
> switching), `romgen` (C, → flat `.min`) — plus the production `crt0`, `s1c88.lib`, and `<pm.h>` device
> header are all in place. The codegen retarget is **functionally complete** (corpus 20/20 byte-identical,
> emu-test 16/16, diff-test 12; all numbered ABI tasks closed; native `DIV`/`MLT`; 3-byte banked function
> pointers; S1C88 **MAXIMUM-mode** call model). Known bugs are **deprioritized/deferred** — the `_fsadd`
> different-sign float-subtract miscompile (TODO #8; float is low-value here) and a narrow pointer-compare
> miscompile (`&a[i] < &a[j]` with runtime indices — TODO #11-ptrcmp-bug); what remains is integer/pointer
> quality/coverage (#11), code-size/peephole tuning (#12, with `size-check.sh` as the yardstick), branch
> relaxation (#14, broken into #14a/b/c), and the z80-artifact scrub remainder (#20 A/D/F).
> Design/ABI: **`docs/s1c88/abi-decision.md`**; current state + next action: **`docs/s1c88/HANDOFF.md`**;
> end-user guide: **`docs/s1c88/building-roms.md`**; the toolchain: `docs/s1c88/{sdas88-retarget,banked-branch}.md`.

## The codegen design (read `docs/s1c88/abi-decision.md`)

- **Register model = Faithful BA+HL:** byte GPRs `A,B,L,H`; pairs `BA`(B:A) and `HL`(H:L); index `IX,IY`
  (not byte-addressable). The z80 `C,D,E,DE` are dropped — S1C88 has no DE, and `A` doubles as BA's low byte.
- **Asm output = SDCC sdas style** (reuse the sdas/sdld assembler+linker family).
- **The retarget was done always-green incremental** — constrain the allocator, rewrite `gen.c`'s `DE`/`BC`
  scratch uses function-by-function, build + smoke-test after each batch. (It is now complete.)
- **Load-bearing invariants** (read the named abi-decision.md sections before touching these): the **EP=0
  invariant** for `__far` deref (HL+EP idiom; "Task #9"), the **MAXIMUM-mode call model** (3-byte CB:PC
  frames, caller cleanup, PCALL via `__sdcc_fptr`; "The call model"), native `DIV` shapes ("Native DIV"),
  and the **branch displacement convention** (one byte earlier than z80; HANDOFF.md). Runtime contract:
  programs provide `__sdcc_fptr:: .ds 2` in near RAM (the production crt0 does).

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
so `sdcc foo.c` can't preprocess in the dev inner-loop. Feed already-preprocessed C via `--c1mode` (reads
cpp'd C on stdin, emits asm). (For the full SDK — real `sdcpp`, `sdas88`, `sdldz80`, `romgen`, runtime —
run `scripts/setup-sdk.sh` once; then `sdcc -ms1c88 foo.c` preprocesses + links for real.)

```bash
build/sdcc-4.5.0/src/sdcc --version                                    # -> "SDCC : s1c88 ... 4.5.0"
printf 'int add1(int x){return x+1;}\n' | \
  build/sdcc-4.5.0/src/sdcc -ms1c88 --c1mode -o out.asm                # compile to S1C88 asm
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
4. **Configure** with *all stock ports enabled* alongside s1c88, plus device-lib/ucsim/sdcdb/etc. off.
   (Historically every port had to be `--disable`d because s1c88, a z80 clone, kept the z80 port's global
   symbol names and collided at link time; those 44 globals were renamed to unique `s1c88_*` names, so the
   port is now a fully independent variant that links cleanly next to z80 and the rest — `-ms1c88`,
   `-mz80`, … coexist in one driver.)
5. **Inject** the port: append `s1c88` to `ports.build`/`ports.all` and generate `src/s1c88/Makefile`
   from `Makefile.in` via `./config.status --file=...`.
6. **`make -C src`** builds the `sdcc` driver. `port.mk` turns every `*.def` into a `*.rul` via
   `gawk -f ../SDCCpeeph.awk`; `main.c` `#include`s the single `peeph.rul` (the port has one peephole
   file, `peeph.def` — the z80 `peeph-z80.def` was merged into it and the dead variant `peeph-*.def`
   removed, since sdcc88 is a standalone S1C88 port, not a z80 variant).

## The S1C88 port (`src/s1c88/`)

Cloned from SDCC 4.5.0's `src/z80` (file `z80.h`→`s1c88.h`; the registered `z80_port`→`s1c88_port` with
`.target="s1c88"`). The other 9 z80 variant PORT structs were pruned; it's a single-variant port that runs
the plain-z80 codegen path (`IS_Z80` is hardcoded `1`, every other variant `0` — see `s1c88.h`). Every
global symbol that used to collide with the z80 port (`z80_regs`→`s1c88_regs`, `genZ80Code`→`genS1C88Code`,
`z80_opts`→`s1c88_opts`, the peephole predicates, the asm-dialect tables, …) was **renamed to a unique
`s1c88_*` name**, so the port links cleanly alongside z80 and all other ports in one driver. The remaining
shared *type/enum* names (`Z80_OPTS`, `SUB_Z80`, the `PAIR_*` ordinals) carry no link symbol, so they're
harmless; some internal identifiers still read "z80" but are port-private.

Per-file roles: `main.c` (the `PORT s1c88_port` struct + options), `gen.c`/`gen.h` (iCode → asm, the
bulk), `ralloc.c`/`ralloc.h` + `ralloc2.cc` (register allocation; `ralloc2.cc` instantiates SDCC's generic
tree-decomposition allocator in `SDCCralloc.hpp`, which needs Boost), `s1c88.h` (sub-port enum + `IS_*`
macros), `mappings.i` (asm-dialect tables), `peep.c`/`peeph*.def` (peephole). See `src/s1c88/README.md`.

## Reference material

**`docs/s1c88/`** is the authoritative, distilled reference (from the Epson official PDFs): architecture,
memory model, addressing modes, instruction set, the C ABI, and the assembler/linker/locator toolchain.
Start at `docs/s1c88/README.md`; the backend decisions are in `docs/s1c88/abi-decision.md`.

## Conventions

- **All work is on `main`** (the old `s1c88-retarget` branch is gone). Commit green checkpoints; clearly
  label any intentionally-red WIP. Validate each codegen slice with `./scripts/validate-s1c88.sh` before
  committing — the build working in-sandbox means you can (and should) verify every change.
- Convert the design/strategy in `docs/s1c88/abi-decision.md` into action — keep it current as decisions
  evolve, and keep `docs/s1c88/HANDOFF.md` accurate (it's the fastest way to resume).
