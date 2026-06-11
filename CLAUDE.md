# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **▶ Resuming / "pick up where you left off"?** This file is the entry point. TL;DR: `./scripts/dev.sh`
> confirms the build is green; `scripts/corpus-check.sh` + `scripts/emu-test.sh` + `scripts/diff-test.sh`
> are the gates (or `scripts/run-tests.sh` for all at once). **Always rebuild via the overlay
> (`dev.sh`/`corpus-check.sh`), never raw `make -C build/.../src`** (it compiles a stale copy). The numbered backlog is
> **complete** — any new work is tracked via PRs. Work via self-contained PRs (meaningful names);
> **`main` is protected** — direct push blocked, the `ci` **build & test** check must be green to merge
> (reviews not required).

## Project Overview

**sdcc88** retargets [SDCC](https://sdcc.sourceforge.net/) (the Small Device C Compiler) **4.5.0** to the
**Epson S1C88** — the 8-bit core of the **Pokémon Mini**. It inherits SDCC's C frontend and
machine-independent middle-end unchanged and adds one backend code-generator **port** in `src/s1c88/`.

sdcc88 is an **overlay on upstream SDCC, built with SDCC's own autotools build** (`configure` + `make`) —
*not* a reimplemented build system. `build.sh` fetches SDCC, drops our port into its tree, registers it,
and builds the compiler.

> **Status: complete and in maintenance.** `sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx game.min`
> builds a bootable Pokémon Mini ROM. `src/s1c88` started as a clone of SDCC's `z80` port (the z80 register
> model fits the S1C88 well) and is fully retargeted to the real S1C88 ISA. The binary toolchain — `sdas88`
> (full ISA, byte-verified, also the codegen validator, with same-module branch relaxation), `sdldz80`
> (assemble→link + **banked `bcall`/`bjump`**, linker-resolved bank switching), `romgen` (C, → flat `.min`,
> or the **MINX** `.minx` debug container — sparse ROM segments + the full source-line debug tables, spec
> `docs/s1c88/minx-format.md`, reader `minxdump`; **complete, merged via PR #37**)
> — plus the production `crt0`, `s1c88.lib`, and `<pm.h>` device header are all in place. Codegen is
> functionally complete (all numbered ABI tasks closed; native `DIV`/`MLT`; 3-byte banked function pointers;
> S1C88 **MAXIMUM-mode** call model; int×int→long widening multiply), every gate green (corpus 20/20
> byte-identical, emu-test 18/18, diff-test 25, run-tests 69/69), and the differential-mining suite is
> **clean with no known correctness bugs** (integer / pointer / far-pointer / struct / union / fnptr /
> long-long / float / recursion / control-flow / bit-ops / mixed-width / `volatile` / libc all verified).
> The bundled libc covers `string.h`, `stdlib.h`, `ctype.h`, and
> `printf`/`sprintf` (`<stdio.h>`, default `putchar` → `DEBUG_OUT`).
>
> Linker cross-module relaxation (#14c) is **done and default-on** — every link reclaims same-bank
> cross-module `ld nb` bytes (opt out `SDLD_NO_RELAX=1`; `size-check.sh`'s `#14c relax` section). The
> numbered backlog is **complete** — #14e (stale-symtab), #16 (`UNIMPLEMENTED`-boundary lift), #20
> (z80-artifact scrub) and #12 (code size: dead-token cleanup done; the two size peepholes are inert on the
> corpus, parked) all closed, and #11 differential mining concluded (the suite is comprehensive). The
> backlog is **complete** — `setjmp`/`malloc` are out of scope by design (no realistic use on the device).
> Design/ABI: **`docs/s1c88/abi-decision.md`**; end-user guide:
> **`docs/s1c88/building-roms.md`**; the toolchain: `docs/s1c88/{sdas88-retarget,banked-branch}.md`.

## The codegen design (read `docs/s1c88/abi-decision.md`)

- **Register model = Faithful BA+HL:** byte GPRs `A,B,L,H`; pairs `BA`(B:A) and `HL`(H:L); index `IX,IY`
  (not byte-addressable). The z80 `C,D,E,DE` are dropped — S1C88 has no DE, and `A` doubles as BA's low byte.
- **Asm output = SDCC sdas style** (reuse the sdas/sdld assembler+linker family).
- **The retarget was done always-green incremental** — constrain the allocator, rewrite `gen.c`'s `DE`/`BC`
  scratch uses function-by-function, build + smoke-test after each batch. (It is now complete.)
- **Load-bearing invariants** (read the named abi-decision.md sections before touching these): the **EP=0
  invariant** for `__far` deref (HL+EP idiom; "Task #9"), the **MAXIMUM-mode call model** (3-byte CB:PC
  frames, caller cleanup, PCALL via `__sdcc_fptr`; "The call model"), native `DIV` shapes ("Native DIV"),
  and the **branch displacement convention** (one byte earlier than z80; `docs/s1c88/sdas88-retarget.md`).
  Runtime contract: programs provide `__sdcc_fptr:: .ds 2` in near RAM (the production crt0 does).

## Build

One-time dependencies (Debian/Ubuntu/WSL): `sudo apt-get install -y build-essential flex bison m4 gawk
libboost-dev zlib1g-dev`.

```bash
./build.sh           # fetch (cached) + overlay + patch + configure + build the whole SDK
./build.sh --fresh   # wipe build/ and rebuild the whole SDK from scratch (needed when the patch changes)
./scripts/dev.sh      # overlay current src/s1c88 + make + run the smoke test (fast inner-loop)
```

Result: a complete SDK under `build/sdcc-4.5.0/` — `src/sdcc` (the `-ms1c88` driver) plus `bin/`
(`sdcpp`, `sdas88`, `sdldz80`, `romgen`), the runtime, and device headers. `sdcc -ms1c88 foo.c`
preprocesses + links for real.

`scripts/package-sdk.sh` stages that SDK as a **relocatable tarball** (`build/dist/`; bin/ + the sdcpp
`libexec/.../cc1` backend + share/sdcc + docs + the hello starter) and proves it self-contained from a
temp dir under `env -i` before tarring. CI uploads it per platform (`sdcc88-sdk-<os>-<arch>`
artifacts — linux x64/arm64/x86(-m32), darwin arm64/x64, windows x64/arm64) on every green run; pushing a `v*` tag publishes all platforms as
one GitHub Release (`.github/workflows/release.yml`, gated on the same test suite per platform). The
shell scripts + Makefiles stay portable to macOS (BSD userland: no `sha256sum`/`stat -c`/bare `sed -i`;
awk `strtonum` needs explicit `gawk`).

**Smoke-testing codegen** — the fast inner-loop is `./scripts/dev.sh`, which re-makes **only the
compiler** (not the bundled `sdcpp` preprocessor), so in that loop `sdcc foo.c` can't preprocess: feed
already-preprocessed C via `--c1mode` (reads cpp'd C on stdin, emits asm). (`./build.sh` itself builds
the full SDK, `sdcpp` included.)

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
7. **Build the rest of the SDK** by invoking the idempotent component scripts in order: `build-sdcpp.sh`
   (the bundled GCC-cpp preprocessor — the heavy one), `build-sdas.sh as88` (`sdas88`), `build-sdld.sh`
   (`sdldz80`), `build-romgen.sh` (`romgen`), `build-runtime.sh` (`crt0.rel` + `s1c88.lib` + headers,
   compiled through the just-built driver + sdas88, so it runs last). These same scripts are also called
   lazily by the test/smoke scripts; each is a fast no-op once built, so a warm `build.sh` only re-makes
   the compiler in step 6.

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

- **Work goes through self-contained PRs with meaningful names** — one PR per scope; if two changes share a
  scope, combine them into one PR that details the total change. **Branch names are
  `<github-username>/<meaningful-name>`** — prefixed with the username of the person opening the PR (e.g.
  `asterick/branch-relaxation`). **`main` is protected:** direct pushes are blocked (work via a branch +
  PR), and a PR cannot merge unless the `ci` check (the **build & test** job: `build.sh` +
  `scripts/run-tests.sh`) is **green**. Code reviews are not required. Keep each PR green on its own —
  validate with `./scripts/run-tests.sh` (or `./scripts/validate-s1c88.sh` for a focused codegen slice)
  before opening it; clearly label any intentionally-red WIP.
- **Keep the docs current as part of each PR — they must never lag behind a merged branch.** Whenever a
  branch has an outstanding PR, bring the affected docs up to date *in that PR*: the root **`README.md`**
  (the user-facing status — refresh it whenever capability/status changes) and `docs/s1c88/abi-decision.md`
  when an ABI/design decision evolves. `docs/s1c88/` is for *how the compiler and processor work*. Completed
  work and any remaining task/state are recorded in git history + commit messages + the Claude memories, not
  in a tracked backlog file (the numbered backlog is complete; there is no `TODO.md`).
