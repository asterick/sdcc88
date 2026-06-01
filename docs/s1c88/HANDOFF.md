# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-05-31. Branch: **`main`** (all work is on main; there is no `s1c88-retarget` branch
anymore — CLAUDE.md's mention of it is stale). State: **GREEN** — compiler builds/links/runs, and the
**binary toolchain (assembler + linker + banked ROM) is complete**. The remaining work is the **codegen
retarget** (z80→S1C88 emission cleanup)._

---

## TL;DR state

- **What sdcc88 is:** SDCC 4.5.0 retargeted to the Epson S1C88 (Pokémon Mini). See `CLAUDE.md`.
- **`src/s1c88/` is a clone of SDCC's `z80` port.** `sdcc -ms1c88 --c1mode` compiles C → asm. The codegen
  is being retargeted from z80-flavored to real S1C88, **always-green incremental** (commit green slices).
- **The toolchain is DONE** (the big recent push):
  - **`sdas88`** — the S1C88 assembler: full practical ISA, every form byte-verified vs `instruction-set.md`
    App. A. Doubles as the **codegen validator** (`scripts/validate-s1c88.sh`).
  - **`sdldz80`** — the linker: assemble→link works; **banked `bcall`/`bjump`** auto-resolve the code bank
    (`ld nb,#bank` written/omitted by the linker). `scripts/romgen.py` → flat `.min`.
  - A multi-bank Pokémon Mini ROM builds end-to-end (`scripts/rom-smoke.sh`, GREEN).
- Everything builds + runs **inside the sandbox** — iterate freely, no `! ...`.

## NEXT ACTION (do this) — the codegen retarget

1. Confirm green:
   ```
   ./scripts/dev.sh            # overlay src/s1c88 + build compiler + smoke test → "GREEN"
   ```
2. Regenerate the validator's to-do list (the live z80-ism list):
   ```
   printf 'int cmp(int a,int b){if(a<b)return 1;return 0;}\n' | \
     build/sdcc-4.5.0/src/sdcc -ms1c88 --c1mode -o /tmp/x.asm
   scripts/validate-s1c88.sh /tmp/x.asm     # assembles emitted asm; rejects = the z80-isms to fix
   ```
3. Clear z80-isms in `src/s1c88/gen.c` one slice at a time, **byte-checking each with the validator**,
   committing green. **Done so far:** frame setup (`ld ix,sp`), branches (`jr→jrs`, `jp→jrl`, `call→carl`,
   via `peeph-z80.def`), and **signed compare → native `jrs LT/GE`** (`genCmp`/`genIfxJump`; the
   `z80instructionSize` branch-sizing in `peep.c` came with it). **Remaining** (the validator list):
   - **Byte-wise 16-bit ALU** (`sub a,l` / `add a,l` / `sbc hl,bc` …) — the central register-model work:
     get 16-bit operands into **BA/HL** so native `add hl,ba` / `sub hl,ba` / 16-bit `cp` replace the
     byte-wise idioms. Biggest cluster. See `abi-decision.md` → "Step 2: concrete codegen mapping" and the
     worklist below.
   - `push af` stack reservation → `sub sp,#imm`.
   - non-ifx signed compare (`return a<b`) still emits `jp PO` (the boolean-materialization path).
   - `rlca`/`rla`/`rrca`/`rra` → `rlc a`/`rl a`/… (**flag-subtle** — z80 acc-rotates are carry-only,
     S1C88's set Z/S too; check each use site).
   - 16-bit shifts (`rr l`, `sla -2(ix)`); `inc d(ix)`.
4. **Per user direction:** when retargeting branch emission, emit **`bcall`/`bjump`** for inter-function
   calls/tail-jumps (not raw `carl`/`jrl`) so the linker becomes the single place that optimizes branch
   form + bank switching. Local loop/if branches can stay plain `jrs`/`jrl`. (Memory:
   `codegen-prefer-bcall-bjump`.)

## The gen.c worklist (the central register-model grind)

The linchpin is the scratch-asmop machinery near the top of `src/s1c88/gen.c`: `asmop_c/d/e/iyh/iyl`,
`asmop_bc/de`, the long combos `asmop_dehl/hlde/hlbc/debc`, the `_pairs[]` table (`PAIR_BC`/`PAIR_DE`), and
the parm-mask arrays sized `[IYH_IDX+1]`. The ISA-grounded mapping (`DE→BA`, `BC→IX/IY/stack`, C/D/E bytes
*eliminated* not renamed, IYL/IYH dropped) + the two hazards (A/BA overlap; no byte home for C/D/E) are in
**`abi-decision.md` → "Step 2"** — read that first. Progress so far: PAIR_BA added as a first-class pair
(`a13bc77`); genPlus 16-bit add prefers BA → `add hl,ba` (`aee2ed0`). The **call ABI is done** (returns
BA/HL:BA, args faithful Epson order). Next pieces: genMinus `sbc hl,ba` (A-overlap trap: `a_dead=false`
when pair==BA), `fetchPairLong` PAIR_BA wiring, then mop up direct byte-C/D/E sites.

> A from-scratch big-bang reshape was tried and **reset** (unverifiable-red for the whole grind). The dead
> WIP is in reflog `417bed5` — useful only as a reference for the *end-state* register defs.

## Verify / the tools

- `./scripts/dev.sh` — build compiler + codegen smoke test → `GREEN`.
- `./scripts/validate-s1c88.sh <file.asm>` — **the real validator**: assembles emitted codegen with
  `sdas88`; exits 0 iff clean, else freq-ranks the rejected instructions = the z80-ism to-do list.
- `./scripts/check-s1c88.sh <file.asm>` — the cheap textual z80-residue meter (interim signal).
- `./scripts/build-sdas.sh as88` → `bin/sdas88` (assembler). `./scripts/build-sdld.sh` → `bin/sdldz80`
  (linker). Both auto-apply `third_party/sdcc/s1c88_banked_branch.patch` (the banked-branch toolchain
  changes to shared asxxsrc/linksrc).
- `./scripts/link-smoke.sh` — assemble→link pipeline check. `./scripts/rom-smoke.sh` — full banked
  assemble→link→`romgen.py`→`.min` check.

## Map of everything

- `CLAUDE.md` — project overview, build, overlay mechanics + gotchas, conventions.
- `docs/s1c88/abi-decision.md` — codegen design + ABI + always-green strategy (authoritative).
- `docs/s1c88/sdas88-retarget.md` — the assembler retarget (status: complete).
- `docs/s1c88/banked-branch.md` — the banked `bcall`/`bjump` design + impl (status: works end-to-end).
- `docs/s1c88/` — distilled Epson manuals (architecture, ISA, addressing, memory model, toolchain).
- `src/s1c88/` — the compiler port; `sdas/as88/` — the assembler backend (overlay).
- Auto-loaded memory `sdcc88-bringup-status` — same state, loads every session;
  `codegen-prefer-bcall-bjump` — the bcall/bjump emission guidance.

## Commit history (branch `main`, all green)

Recent (toolchain): `33948cb` romgen + ROM test · `dced778` auto-bank works · `8da1910` linker built ·
the `sdas88: …` series (full ISA, byte-verified) · `f95e7fb`/`85af71a`/`384472a` codegen slices (frame,
branches, signed-compare). Earlier: the call-ABI + allocator reshape (`b606833` Step 1, `aee2ed0` etc.).
