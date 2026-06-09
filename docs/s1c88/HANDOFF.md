# ▶ HANDOFF — pick up here

**Single resume entry point.** sdcc88 is a **complete, working toolchain in maintenance**; this is the
fastest way to resume. If the prompt is *"pick up where you left off,"* do the steps under **NEXT ACTION**.
The forward backlog is [`TODO.md`](TODO.md); the design/ABI is [`abi-decision.md`](abi-decision.md); the
end-user guide is [`building-roms.md`](building-roms.md).

_Last updated: 2026-06-08._

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
    `scripts/build-runtime.sh`. `./build.sh` builds the whole SDK in one command.
- **Codegen is functionally complete** — every reachable z80-ism gone; the ABI tasks (register model, IY
  args, `__far` pointers) closed; native `DIV`/`MLT`; 3-byte banked function pointers; the S1C88
  **MAXIMUM-mode** call model (3-byte CB:PC frames — `abi-decision.md` "The call model"). The assembler now
  does same-module branch relaxation (#14b): intra-area `bcall`/`bjump` shrink to 2–3 bytes.
- **All gates green:** corpus 20/20 byte-identical, emu-test 16/16, diff-test 12, run-tests 50/50 (TAP),
  plus driver/crt0/rom/branch/insn-size smokes and the `examples/hello` build. Corpus ROM = 8429 B.
- Everything builds + runs **inside the sandbox** — iterate freely, no `! ...`.

## NEXT ACTION (do this)

1. **Confirm green** — `./scripts/run-tests.sh` (builds once, runs every suite in parallel, TAP, 50/50). For
   a focused codegen change, the inner loop is `./scripts/dev.sh` then `corpus-check.sh` + `emu-test.sh` +
   `diff-test.sh`: corpus-check proves the asm is *stable*; emu/diff prove it *computes the right values*.
   **Run all three for every codegen change**, and add an emu/diff case whenever you touch new territory —
   each new differential module has found real silent miscompiles.
2. **Pick from the backlog — [`TODO.md`](TODO.md).** The toolchain is done and the #11 differential-mining
   menu is exhausted (no open mining bugs). What's left is forward improvement on the existing source base:
   keep mining new C constructs (`#11-libc`, `#11-longshift`, anything untested), code-size peephole work
   (`#12-flag-reuse` byte-combine peephole, `#12-far-idiom`), the deferred linker cross-module relaxation
   (`#14c`), the `UNIMPLEMENTED`-boundary lift (`#16`), and the z80-lineage cleanup remainder (`#20 D`).
   There are **no known correctness bugs** — the differential suite (including float) is clean.

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
- `./scripts/relax-analysis.sh` — branch-relaxation slot report (report-only): reads each fully-linked
  program's resolved `bcall`/`bjump` slots from the relocated listing. With #14b landed, same-module slots
  already relax to 2–3 B; use it to spot remaining cross-module (#14c) opportunity.
- `./scripts/emu-test.sh` — RUN `tests/emu/cases/*.c` on the vendored minimon core (16/16). Execution truth.
- `./scripts/diff-test.sh` — compile the same C host-vs-emulator and diff the output (12 modules).
- `./scripts/validate-s1c88.sh <file.asm>` — assemble emitted codegen with `sdas88`; any reject = a z80-ism.
- `./scripts/branch-smoke.sh` — byte-lock the branch displacement convention (above).
- `./build.sh` — build the whole toolchain from a clean checkout (compiler → sdcpp → sdas88 →
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
`examples/hello`).

**Workflow (since the toolchain went to maintenance):** work goes through **self-contained PRs with
meaningful names** (one per scope; combine same-scope changes into one PR). **Branch names are
`<github-username>/<meaningful-name>`** (e.g. `asterick/branch-relaxation`). **`main` is protected** —
direct pushes are blocked and a PR can't merge unless the `ci` **build & test** check is green; code reviews
are not required. So: `git checkout -b <user>/<name>` → commit green slices → push → open PR → CI green → merge.
