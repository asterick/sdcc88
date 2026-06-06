# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **▶ Resuming / "pick up where you left off"?** Go to **[`docs/s1c88/HANDOFF.md`](docs/s1c88/HANDOFF.md)** —
> it has the current state and the exact next action. TL;DR: `./scripts/dev.sh` (confirms the build is
> green + runs a codegen smoke test). The codegen retarget is **functionally complete for the
> verification corpus** (session 16: **#7a done — zero sub-port variant-macro refs remain port-wide**
> (~2,000 dead lines gone; every `ex (sp),hl`/`ldir` emit went with them), and the four remaining z80
> builtins (`__builtin_memset/strcpy/strncpy/strchr` — a reachable blind spot) retargeted to native
> byte loops; session 15: `genMult`→BA/B, `MLT`, block-scope `.globl`, #10 `jp <signed cc>`, **plus the
> critical branch-displacement off-by-one — see HANDOFF "THE BRANCH DISPLACEMENT CONVENTION"**):
> **17/17 corpus files assemble with 0 sdas88 errors**. Remaining work:
> far pointers (#9) — **#7 (register-model sweep) and #8 (IY argument passing) are CLOSED** (s19:
> int/near-ptr args overflow into IY, the PCALL HL-argument miscompile fixed via RET-dispatch,
> instruction sizing corrected; corpus 18/18). Validate each
> change with `./scripts/corpus-check.sh` (byte-identical +
> sdas88) — **always rebuild via the overlay (`dev.sh`/`corpus-check.sh`), never raw `make -C
> build/.../src` (it compiles a stale copy).** (All work is on **`main`**.)

## Project Overview

**sdcc88** retargets [SDCC](https://sdcc.sourceforge.net/) (the Small Device C Compiler) **4.5.0** to the
**Epson S1C88** — the 8-bit core of the **Pokémon Mini**. It inherits SDCC's C frontend and
machine-independent middle-end unchanged and adds one backend code-generator **port** in `src/s1c88/`.

sdcc88 is an **overlay on upstream SDCC, built with SDCC's own autotools build** (`configure` + `make`) —
*not* a reimplemented build system. `build.sh` fetches SDCC, drops our port into its tree, registers it,
and builds the compiler.

> **Status:** the compiler **builds, links, and runs** as a stock SDCC `sdcc` driver with `-ms1c88`
> selectable. **`src/s1c88` is a clone of SDCC's `z80` port** (re-based from an earlier stm8 clone — the
> z80 register model fits the S1C88 far better). The **binary toolchain is COMPLETE**: `sdas88` (full ISA,
> byte-verified — also the codegen validator), `sdldz80` (assemble→link + **banked `bcall`/`bjump`** with
> linker-resolved bank switching), and `romgen.py` (→ flat `.min`); a multi-bank ROM builds end-to-end.
> **Codegen is being retargeted** from its z80 origin to the S1C88 ISA, always-green incremental. **Done:**
> frame setup, branches (`jrs`/`jrl`/`carl`), the full compare cluster (signed/unsigned/literal `<`/`>`/
> `==` via native `cp ba,hl`/`cp …,#imm` + `jrs LT/GE`), the 16-bit ALU (`sub ba,hl`), `adjustStack`
> (native SP moves), 8-bit L/H ALU operands incl. AND/OR/XOR (routed through B), shifts (routed through
> A/B — the S1C88 only shifts A/B/[HL]), accumulator rotates (`rla`→`rl a` etc.), `push af`→`push a;push
> sc`, the scratch-pair selectors (`getFreePairId`→BA), `cpl a`/`neg a` operand forms, indexed/abs
> `inc/dec` routed through A (`emit3_incdec`), `cp a,l`/`sub a,l` routed through B, `djnz`→`djr nz`,
> `bit n,reg`→`bit reg,#mask` (`emitBitTest`), **RES/SET elimination** (no such instruction — bits via
> `and/or a,#mask`), constant pointer/member offsets via native `add {hl,ix,iy},#imm` (`offsetPair`),
> **`add hl/iy/ix,sp` fully eliminated** (peephole `ld pair,sp; add pair,#off`), and struct return-by-value
> no longer SIGSEGVs (clean "Unimplemented"). The **peephole rules were audited for S1C88 validity and
> collapsed into one file** (`peeph.def`; the z80 `peeph-z80.def` + 5 dead variant files removed) — see
> `docs/s1c88/HANDOFF.md` "Peephole audit". **`ldir` eliminated from the memcpy/pointer/struct-return
> paths** (session 5): genBuiltInMemcpy (incl. runtime-count via a borrowed-IX counter + `cp ix,#0` guard) /
> genPointerSet / genPointerGet / genRet emit the native byte loop (HL=source, IY=dest) — see HANDOFF
> "Session 5". (Session 14 found `ldir` still emitted for a long global↔global copy — fixed in `8596ef3`.)
> **Inter-function calls emit `bcall`/`bjump`** (linker picks form + bank switch) + **`.globl` for support
> routines** (s6); **independent port** linking alongside z80 et al. (s8); **function-pointer calls** via
> `jp hl`/manufactured-return (s9); **ISR prologue/epilogue** (RETE model, `push ba/hl/iy` save — s10);
> **struct return-by-value** (copy to the caller's hidden buffer — s11); **`__critical`** via SC
> interrupt-level masking (`push sc; or sc,#0xc0` / `pop sc` — s12). **The codegen is functionally
> complete for the verification corpus and the assemble→link→banked-ROM pipeline is GREEN:** the
> session-14 corpus (`scripts/corpus-check.sh`) exposed reachable z80-isms the earlier narrower corpus
> missed — all now fixed (s14: `or a,l/h` in `&&`/`||`, bitfield `set/res`+`rld/rrd`, `ld (iy+d),#imm`,
> `ldir` block copy, STL-address BA; s15: the `genMult` literal path → BA/B + block-scope extern
> `.globl`, genMultOneChar → native `MLT`, the #10 `jp <signed cc>` invert-and-skip lowering in sdas88,
> and the **critical relative-branch off-by-one fix** — every branch was one byte short vs the real
> S1C88 base; `scripts/branch-smoke.sh` now byte-locks the convention); s16: **#7a done — zero variant-
> macro refs port-wide** (~2,000 dead lines, every `ex (sp),hl`/`ldir` emit gone) + the reachable
> builtin cluster (`__builtin_memset/strcpy/strncpy/strchr`) retargeted to native byte loops with
> corpus file `17_builtins.c`. **17/17 corpus files assemble
> with 0 sdas88 errors.** **Remaining work:** 3-byte far
> pointers (#9) — tasks #7 and #8 are CLOSED (s18/s19; IX is permanently excluded from the argument
> ABI by the frame prologue; the inert PAIR_BC/PAIR_DE and C/D/E_IDX names are documented optional
> cosmetics, not debt).
> All work is on **`main`**.
> The design, ABI, and plan live in **`docs/s1c88/abi-decision.md`**; current state + next action in
> **`docs/s1c88/HANDOFF.md`**; the toolchain in `docs/s1c88/{sdas88-retarget,banked-branch}.md`.

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

**Progress:** the retarget is functionally complete for the verification corpus (assemble→link→ROM
GREEN; 17/17 corpus files at 0 sdas88 errors — sessions 14/15/16 closed every reachable z80-ism the
corpus exposes, most recently the builtin memset/strcpy/strncpy/strchr cluster in s16). **#7a (the
dead variant-branch sweep) is COMPLETE** — zero sub-port variant-macro refs remain anywhere in the
port, and every `ex (sp),hl`/`ldir` emit site went with them. See `HANDOFF.md` for per-session commits.
**The remaining grind (#7b/#7c)** is eliminating the z80 `C/D/E` byte regs + `DE`/`BC` scratch — the 4
latent `push de` sites (#7b, see HANDOFF "Session 16"), then the symbols themselves (#7c): the
scratch-asmop machinery (`asmop_bc/de`, the combined long asmops `DEHL/HLDE/HLBC/DEBC`, `_pairs[]`,
the `[IYH_IDX+1]` parm-mask arrays) and the `PAIR_DE`/`*_IDX` ordinals keyed into `ralloc2.cc`'s Boost
allocator. This is *cleanup/latent-risk reduction* — the binary is already correct — so do it carefully
(always-green, byte-identical after each step), not as a big-bang. See `abi-decision.md` Step 2 +
`HANDOFF.md` "Sessions 13/16" for scope/risk.

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
  evolve. Keep the auto-loaded memory (`sdcc88-bringup-status`) accurate; it's the fastest way to resume.
