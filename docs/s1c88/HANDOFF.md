# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-06-07._

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
- **All gates green:** corpus 20/20 byte-identical (0 sdas88 errors), emu-test 13/13 (execution),
  diff-test 5 (host-vs-emulator), plus driver/crt0/rom/branch smokes and the `examples/hello` build.
- Everything builds + runs **inside the sandbox** — iterate freely, no `! ...`.

## NEXT ACTION (do this)

1. **Confirm green:** `./scripts/dev.sh` (builds the compiler + codegen smoke) then
   **`scripts/corpus-check.sh`** (byte-identical, 20/20), **`scripts/emu-test.sh`** (13/13 execution),
   and **`scripts/diff-test.sh`** (host-vs-emulator). corpus-check proves asm is *stable*; emu-test +
   diff-test prove it *computes the right values*. **Run all three for every codegen change**, and add
   an emu/diff case whenever you touch new codegen territory (each new module has found real bugs).
2. **Next priority — keep mining with the differential suite (#11).** It has found ~9 real reachable
   miscompiles that byte-identical assembly never could; the untested integer/pointer territory is where
   the next correctness bugs live: **long long (currently unverified)**, bitfield-heavy code, and deep
   call chains. Add a `tests/diff/cases/*.c` module, run corpus-check + emu-test + diff-test, fix what it
   surfaces. After that, the code-size/speed work — the branch-relaxation lift (#14; #13 prerequisite done) and
   peephole/cost tuning (#12) — plus the Section C limitation audit (#16, #17).
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
- **Runtime contract:** programs provide `__sdcc_fptr:: .ds 2` in near RAM (crt0 does; bare test startups
  must too). Far const data lives in area `_FAR` at PHYSICAL addresses (`romgen --far=start-end`).
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
  TAP version 13 stream (40 points) + summary, exits non-zero on any failure.** Use this as the one-shot
  gate; the individual suites below are still there for focused runs (and each takes `TAP=1`).
- `./scripts/dev.sh` — build compiler + codegen smoke test → `GREEN`.
- `./scripts/corpus-check.sh` — byte-identical codegen + 0-error assembly across `scripts/corpus/` (20/20).
- `./scripts/emu-test.sh` — RUN `tests/emu/cases/*.c` on the vendored minimon core (13/13). Execution truth.
- `./scripts/diff-test.sh` — compile the same C host-vs-emulator and diff the output (5 modules).
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
