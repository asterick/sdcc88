# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-06-08._

> **▶ The forward-looking work list is [`TODO.md`](TODO.md).** The critical path to a *usable* toolchain
> (TODO Section A) is **DONE** — `sdcc -ms1c88 game.c && romgen game.ihx game.min` builds a bootable
> Pokémon Mini ROM. What remains is quality/coverage (Section B) and documented limitations (Section C).

---

## Current state

- **What sdcc88 is:** SDCC 4.5.0 retargeted to the Epson S1C88 (Pokémon Mini). `src/s1c88/` started as a
  clone of SDCC's `z80` port and has been retargeted to the real S1C88 ISA, always-green incremental.
  See `CLAUDE.md` for the overlay build and `abi-decision.md` for the authoritative design + ABI.
- **The toolchain is complete and usable:**
  - **`sdcc -ms1c88`** — the compiler driver (preprocesses via `sdcpp`, no `--c1mode` needed).
  - **`sdas88`** — the assembler: full practical ISA, every form byte-verified vs `instruction-set.md`
    App. A. Doubles as the codegen validator (`scripts/validate-s1c88.sh`).
  - **`sdldz80`** — the linker, with **banked `bcall`/`bjump`** (linker resolves the code bank).
  - **`romgen`** (`tools/romgen.c`, no Python) — `.ihx`/`.rel` → flat `.min`.
  - **`crt0.rel` + `s1c88.lib` + `<pm.h>`** installed in the driver's lib/include dirs by
    `scripts/build-runtime.sh`. `scripts/setup-sdk.sh` builds the whole SDK in one command.
- **The codegen retarget is functionally complete** — every reachable z80-ism is gone, all numbered
  ABI tasks (#7 register model, #8 IY args, #9 `__far` pointers) are CLOSED, division/modulus runs on
  the native `DIV`, multiply on native `MLT`, function pointers are 3-byte banked code pointers, and the
  call model is **S1C88 MAXIMUM mode** (3-byte CB:PC frames — `abi-decision.md` "The call model").
- **All gates green:** corpus 20/20 byte-identical (0 sdas88 errors), emu-test 16/16 (execution),
  diff-test 12 (host-vs-emulator), plus driver/crt0/rom/branch smokes and the `examples/hello` build.
- Everything builds + runs **inside the sandbox** — iterate freely, no `! ...`.

## NEXT ACTION (do this)

1. **Confirm green:** `./scripts/dev.sh` (builds the compiler + codegen smoke) then
   **`scripts/corpus-check.sh`** (byte-identical, 20/20), **`scripts/emu-test.sh`** (16/16 execution),
   and **`scripts/diff-test.sh`** (host-vs-emulator). corpus-check proves asm is *stable*; emu-test +
   diff-test prove it *computes the right values*. **Run all three for every codegen change**, and add
   an emu/diff case whenever you touch new codegen territory (each new module has found real bugs).
2. **Open work — see [`TODO.md`](TODO.md) for the pointable-target menu.** Mining (#11) keeps paying out —
   prior rounds found+fixed the long-long/struct **return-ABI off-by-one**; long long, unions, and pointer
   arithmetic are verified; **`#11-bitfields` is now verified too** (264 values; found only an implementation-
   defined-signedness *test* trap — bare `int:N` is unsigned in sdcc, signed in gcc — not a codegen bug;
   declare bit-fields with explicit `signed`/`unsigned`). **`#11-switch` is now verified too** (620 values;
   jump-table + if-chain lowering, dense/sparse/offset/wide/signed/fall-through/no-default — no codegen bug).
   **`#11-structargs` found + FIXED a real silent miscompile** (96 values): a register arg following a
   struct-by-value arg, e.g. `f(struct, int)`, was dropped — `genPointerPush` clobbered the already-sent
   register; fix stashes the parked HL/BA pair via IY (the rare two-parked-pairs `f(struct,char,int)` now
   traps loudly, cataloged in abi-decision.md). **`#11-fnptr2` is now verified too** (36 values; the
   INDIRECT PCALL path with wide/struct/bigreturn results, stack-overflow + mixed-width + struct-by-value
   args, fnptr-returning-fnptr — confirmed real PCALLs via `__sdcc_fptr`, no codegen bug). **`#11-ptrcmp-bug`
   is now FIXED** — the `&a[i] < &a[j]` silent miscompile was a peephole read-analysis gap (`s1c88MightRead`
   had no `cp ba, X` case, so `argCont` never saw the `hl` operand and the peephole deleted the right-address
   build); regression in `16_ptrcmp.c` + `ptrarith.c`. **The #11 pointable-target menu is now exhausted with
   no open mining bugs.** Code size is now measurable
   (`scripts/size-check.sh`, #12-sizeharness done). **#14a + #14b (assembler same-module branch
   relaxation) are now DONE** — same-area `bcall`/`bjump` drop the `ld nb` and emit a 2-byte `cars`/`jrs`
   (or 3-byte `carl`/`jrl`) instead of the 6-byte linker slot, chosen via a `setbit`/`getbit` bit table in
   `s1c88mch.c` (no `asmain.c` change). Relax-analysis opportunity collapsed 45→2 user slots; corpus ROM
   8460→8452; intra-module calls widely lower to 2-byte `cars`. The remaining lift is **#14c** (linker
   cross-module relaxation — the hard reflow, deferrable) and the **#12 peephole/cost targets**.
   **#12-peep-audit is now done** — dropped 4 dead z80 rules and enabled BA as a peephole scratch pair
   (`isRegPair`/`canSplitReg` + the `unusedReg` lists), resurrecting 3 rules that never fired and saving
   −20 B on the corpus (e.g. `ld a,#x ; ld b,#0` → `ld ba,#x`). Next: `#12-flag-reuse`, `#12-redundant-moves`.
   Section C (#16 traps, #17 const-data) is done (documented + guarded). The z80-artifact scrub is B+C done,
   A/D/F deferred (#20).
3. **Deprioritized — float is low-value for this target.** The one known correctness bug is the `_fsadd`
   different-sign miscompile (all float subtraction): `10.0-4.0` → `0x40C00182` not `0x40C00000`. It's a
   register-pressure / spill bug in the full `_fsadd` compile (algorithm + isolated 32-bit ops are
   correct), not a library issue. Parked unless float becomes relevant; if revisited, re-add the subtract
   cases to `tests/diff/cases/float.c`. See TODO #8.

### Watch-outs (load-bearing)

- **Build via the overlay only** — `dev.sh`/`corpus-check.sh` rebuild the port; **never** raw
  `make -C build/.../src` (it compiles a stale copy and produces a confusing edited-vs-stale "heisenbug").
- **Rebuild the runtime after any crt0/lib/linker change** — `scripts/build-runtime.sh` (the `.rel` bank
  field must match the current linker; a stale `crt0.rel`/`s1c88.lib` silently breaks the integrated link).
- **Const-data placement (TODO #17, done):** plain `const` data lives in the common bank and is reached
  via **2-byte near** pointers (correct — the common bank is physical `< 0x8000`). For const data in a
  **far** bank use **`__far const`** (3-byte, EP-paged deref — the #9 machinery; verified by
  `tests/emu/cases/13_farconst.c`). `romgen` now **hard-errors on common-bank overflow** (any non-banked
  content past logic `0x7FFF`), so an oversized near-pointed const can't silently miscompile. (The old
  "3-byte CPOINTER deref'd near" note was inaccurate — plain const pointers are 2-byte near.)
- **Runtime contract:** programs provide `__sdcc_fptr:: .ds 2` in near RAM (the production crt0 does).
  Far const data lives in area `_FAR` at PHYSICAL addresses (`romgen --far=start-end`).
- **One crt0 + a test BIOS (no test code in the runtime).** There is a single startup,
  `device/lib/s1c88/crt0.s`. It trusts the **BIOS reset-state contract** (all CPU regs 0 except BA=0xFFFF /
  NB=CB=0x01; SP parked; MMIO left in BIOS reset config — IRQs already masked), so it re-inits no MMIO and
  only pins EP=0. On `main` return it hands off via `int (0x48)`, the BIOS shutdown vector. The emulator
  runner embeds a tiny **test BIOS** (`tests/emu/bios.s`) that sets that reset state in real S1C88 code,
  installs the 0x0048 shutdown routine, and enters the cart; the runner reads the exit code off `BA` on
  halt (no exit mailbox). **emu-test + diff-test boot every case as a real PM cart through this crt0+BIOS**
  (the old `tests/emu/crt0.asm` shim is retired). If you edit `bios.s`, the runner rebuild regenerates the
  embedded `build/emu/bios_rom.h` (needs sdas88/sdldz80).
- **Emulator-core header changes need a full rebuild.** Every translation unit in
  `third_party/minimon-core` compiles against `machine.h`'s shared `Machine::State` layout. The
  `tests/emu/Makefile` now lists the core headers as a prerequisite, so editing one rebuilds *all*
  objects — but if you ever build by hand, **`make -C tests/emu clean` after touching a core header**.
  A stale object keeps the old struct layout and reads/writes fields at the wrong offset (this silently
  broke `HALT` once when a new struct field shifted `cpu.status` — the symptom was a clean-looking ROM
  spinning past its `halt`).

### ⚠ THE BRANCH DISPLACEMENT CONVENTION (read before touching branch emission)
The S1C88 computes a taken relative branch as **PC ← PC(after full fetch) + disp − 1** (Epson §4.3.3
`JRS rr → PC←PC+rr+1`; PokeMini `JMPS: PC = PC + OFFSET - 1`). So an **8-bit rr is relative to the rr
byte's own address**, and a **16-bit qqrr is relative to (first disp byte + 1)** — both one byte EARLIER
than the z80 next-instruction base the ASxxxx code inherits. sdas88 was off by one on EVERY relative
branch until `47eb41c`; nothing caught it because no ROM is executed at assemble time. The fix is
assembler-side only: local resolution uses the S1C88 base, and cross-area R_PCR relocs get a **+1 addend
bias** so the stock z80-convention linker (`sdldz80` — it cannot be target-gated) lands on the right base.
`scripts/branch-smoke.sh` byte-locks every form (jrs/cars/jrl/carl/djr/jp-lowering, fwd + back); run it
whenever branch emission or the linker patch changes.

## Verify / the tools

- **`./scripts/run-tests.sh` — the unified runner: builds once, runs every suite in parallel, emits one
  TAP version 13 stream (50 points) + summary, exits non-zero on any failure.** Use this as the one-shot
  gate; the individual suites below are still there for focused runs (and each takes `TAP=1`).
- `./scripts/dev.sh` — build compiler + codegen smoke test → `GREEN`.
- `./scripts/corpus-check.sh` — byte-identical codegen + 0-error assembly across `scripts/corpus/` (20/20).
- `./scripts/size-check.sh` — corpus ROM-size measurement + delta vs `scripts/corpus/sizes.baseline`
  (report-only; `snapshot` to re-bless). The yardstick for #12 (peephole/cost) and #14 (relaxation) wins.
- `./scripts/relax-analysis.sh` — branch-relaxation opportunity analysis (#14a, report-only): reads each
  fully-linked program's resolved `bcall`/`bjump` slots from the relocated listing and reports the bytes
  #14b/#14c would reclaim. Measured ~53% smaller user-code calls; confirms the 3-pass `fuzz` loop converges.
- `./scripts/emu-test.sh` — RUN `tests/emu/cases/*.c` on the vendored minimon core (16/16). Execution truth.
- `./scripts/diff-test.sh` — compile the same C host-vs-emulator and diff the output (12 modules).
- `./scripts/validate-s1c88.sh <file.asm>` — assemble emitted codegen with `sdas88`; any reject = a z80-ism.
- `./scripts/branch-smoke.sh` — byte-lock the branch displacement convention (above).
- `./scripts/setup-sdk.sh` — build the whole toolchain from a clean checkout (compiler → sdcpp → sdas88 →
  sdldz80 → romgen → runtime). `crt0-smoke.sh` / `driver-smoke.sh` / `rom-smoke.sh` — end-to-end ROM checks.
- `./scripts/build-sdas.sh as88` → `bin/sdas88`; `./scripts/build-sdld.sh` → `bin/sdldz80`; both auto-apply
  `third_party/sdcc/s1c88_banked_branch.patch` (banked-branch changes to shared asxxsrc/linksrc).

## Map of everything

- `CLAUDE.md` — project overview, build, overlay mechanics + gotchas, conventions.
- `docs/s1c88/abi-decision.md` — codegen design + ABI + always-green strategy (**authoritative**: register
  model, argument ABI, Task #9 far pointers, Native DIV, the MAXIMUM-mode call model).
- `docs/s1c88/building-roms.md` — the end-user guide (setup, the `sdcc`→`romgen` build, header, memory
  map, interrupts, verification).
- `docs/s1c88/banked-branch.md` — the banked `bcall`/`bjump` design + impl. `sdas88-retarget.md` — the
  assembler retarget. (Both: status complete, kept as reference.)
- `docs/s1c88/` — distilled Epson manuals (architecture, ISA, addressing, memory model, toolchain).
- `src/s1c88/` — the compiler port; `sdas/as88/` — the assembler backend (overlay).

## How we got here (history)

The blow-by-blow is in git (`git log --oneline`) and the commit-message detail; the design rationale is in
`abi-decision.md`. Milestone order, for orientation: the binary toolchain first (sdas88 byte-verified →
sdldz80 → banked `bcall`/`bjump` → romgen → multi-bank ROM); then the codegen retarget from z80 to S1C88
in always-green slices (frame/branches/compares/ALU/shifts → the `ldir` struct-copy cluster → `bcall`/
`bjump` calls → independent port alongside z80 → function-pointer calls → ISR prologue/epilogue → struct
return-by-value → `__critical`); a byte-identical **corpus harness** that disproved early "complete" claims
by exposing reachable z80-isms; then the numbered ABI tasks (#7 register-model sweep, #8 IY args, #9
`__far` pointers), native `DIV`/`MLT`, the MAXIMUM-mode call-model correction; then **execution testing**
(the minimon emulator harness + the host-vs-emulator differential suite), which has found ~9 runtime
miscompiles that byte-identical assembly never could; and finally the **usable-toolchain critical path**
(driver wiring, real `sdcpp`, production crt0 + PM header, `s1c88.lib`, `<pm.h>`, C romgen, SDK packaging +
`examples/hello`). All work is on **`main`**, every checkpoint green.
