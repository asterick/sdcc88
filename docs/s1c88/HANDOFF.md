# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION** below. Everything needed to continue is here or linked from here.

_Last updated: 2026-05-30. Branch: `s1c88-retarget`. State: **GREEN** (compiler builds, links, runs).
Step 1 of the reshape is **DONE** (commit `b606833`); current action is **Step 2**._

---

## TL;DR state

- **What sdcc88 is:** SDCC 4.5.0 retargeted to the Epson S1C88 (Pokémon Mini). See `CLAUDE.md`.
- **`src/s1c88/` is a clone of SDCC's `z80` port** (re-based from an earlier stm8 clone). It builds,
  links, and `sdcc -ms1c88` compiles C to assembly — but **the emitted code is still z80-flavored**.
- **The job in progress:** retarget the codegen to the real S1C88 ISA. Design is fully decided and
  written in [`abi-decision.md`](abi-decision.md): **Faithful BA+HL** register model, **sdas** asm output,
  executed via an **always-green incremental** strategy.
- The build and the freshly-built `sdcc` both run **inside the sandbox** — iterate freely, no `! ...`.

## NEXT ACTION (do this)

1. Make sure you're on the branch and it's green:
   ```
   git checkout s1c88-retarget
   ./scripts/dev.sh          # overlay + build + codegen smoke test → prints "GREEN"
   ```
2. Read [`abi-decision.md`](abi-decision.md) (the "Codegen milestone — decided design" + "Execution
   strategy: always-green incremental" sections). Then execute the always-green reshape:
   - **Step 1 (task #17): ✅ DONE — commit `b606833`.** Constrained the allocator to the S1C88 byte regs
     **A, B, L, H** while keeping every register symbol *defined* so the build stays green: reordered the
     `*_IDX` enum (`A,B,L,H` = 0-3), reordered `z80_regs[]` preserving position==ordinal, set
     `PORT.num_regs = 4`, updated the `REG_*` `#define`s, reordered `gen.c`'s `asmopregs[]`, fixed the
     `regWithIdx`/`freeAllRegs` loops (start at `A_IDX`, was `C_IDX`), and dropped the `IY_RESERVED`
     `num_regs-=2`. The BC-non-adjacency caveat was a non-issue (BC/DE are never allocated under
     `num_regs=4`). Smoke tests confirm A,B,L,H-only allocation. **Known limitation:** BA-pair formation
     is still disabled (the cost fn forbids A in multi-byte vars), so ints use HL only — enable BA once
     gen.c can emit BA-pair code (part of Step 2).
   - **Step 2 (task #18) — IN PROGRESS:** retarget gen.c off z80 DE/BC onto S1C88 BA/HL/IX/IY per
     `abi-decision.md` → "Step 2". **DONE — the call ABI** (commits `d89db99`, `498ad12`, `482f23b`):
     - **Returns:** `aopRet` int/short→`BA` (new `asmop_ba`=A:B), long/float→`HL:BA` (new `asmop_hlba`
       ={A,B,L,H}); char→`A` was already right.
     - **Arguments — faithful Epson order** (`abi-decision.md` → "Argument ABI"): `aopArg` is now a
       register-priority **consumption** allocator (`aopArgRegS1C88`), phase 1 = byte-addressable regs
       (char `A,L,H,B`; int `BA,HL`; near-ptr `HL,BA`; long `HLBA`); IX/IY/YP/XP slots + overflow → stack.
       Verified `f2/fc/fi/f3/pp/fl` + a caller; the A/BA overlap is handled by construction.
     - `z80IsReturned`/`z80IsRegArg`, `genSend`/`genReceive` are all data-driven off aopRet/aopArg, so
       they followed automatically.
   - **Central DE→BA scratch retarget — IN PROGRESS** (the "make emitted code actually S1C88" work).
     Additive strategy: add `PAIR_BA` as a first-class pair, then flip scratch *selection* to prefer it,
     gated by `isPairDead(PAIR_BA)` (= A,B dead → no A/BA overlap). Meter catches missed byte ternaries.
     - **slice 4a DONE (`a13bc77`):** `PAIR_BA` in the enum + `_pairs[]` + isPairInUse/isPairDead +
       genMovePairPair; `push ba`/`pop ba` valid. Additive no-op.
     - **slice 4b-i DONE (`aee2ed0`):** genPlus 16-bit add prefers BA → emits `add hl, ba` (3 sites);
       `fc`/`fi` shed the z80 DE copy. Meter t3 9→6, no regressions.
     - **Next:** genMinus `sbc hl,ba` (gen.c ~8953/10677 — **A-overlap trap:** force `a_dead=false` when
       pair==BA, since the intervening left-load would else clobber the operand in A); then wire
       `fetchPairLong` for PAIR_BA (`ex de,hl`→`ex ba,hl` SP tricks, opt gates) to unlock the fetchPair
       selections + the getFreePairId/getDeadPairId flip (~13 callers). Then mop up direct byte-C/D/E
       sites + the epilogue `pop de`; drop `asmop_iyl/iyh`. Revert point `a13bc77`.
   - **Also pending:** arg ABI phase 2 (IX/IY index-register passing — `ASMOP_IX`, `ld ix,hl`, non-byte
     handling). Run `scripts/check-s1c88.sh` after each batch (realistic input now ~6, was 18 at Step 1;
     pure call-ABI tests at 0). **HIGH-RISK area** (silent miscompile w/o assembler) — the **sdas88**
     validator (task #4) is the real de-risker for this stretch.
   - **Step 3 (task #19):** finish emission — S1C88 register names + any S1C88-specific mnemonics that
     differ from z80 (sdas style). Verify the smoke-test output looks like valid S1C88.
3. Commit **green** checkpoints as you go; clearly label any intentionally-red WIP.

## The gen.c worklist (the grind in Step 2)

The linchpin is the scratch-asmop machinery near the top of `src/s1c88/gen.c`:
- `asmop_c/d/e/iyh/iyl`, `asmop_bc/de`, and the long combos `asmop_dehl/hlde/hlbc/debc`
  (`ASMOP_DE`, `ASMOP_BC`, `ASMOP_DEHL`, …) — used as 16/32-bit scratch throughout.
- the `_pairs[]` table (`PAIR_BC`/`PAIR_DE`) and the parm-mask arrays sized `[IYH_IDX+1]`.

Approx. site counts to clear (from the reset big-bang spike): `PAIR_DE` 334, `PAIR_BC` 153,
`E_IDX` 146, `D_IDX` 141, `IYL/IYH_IDX` 219, `C_IDX` 86, `DE/BC_IDX` 65 → ~1144 total. **Not** a
mechanical sed — the concrete ISA-grounded mapping (`DE→BA`, `BC→IX/IY/stack`, the C/D/E bytes
*eliminated* not renamed, IYL/IYH dropped) and the two hazards (A/BA overlap; no byte home for C/D/E) are
written up in **`abi-decision.md` → "Step 2: concrete codegen mapping"** — read that first. The recommended
tactic is to retarget the central pair abstraction (`_pairs[]`, `getPairId`, `fetchPairLong`, push/pop,
`aopRet`/`aopArg`) once, let call sites follow, then mop up the direct byte-C/D/E sites, building +
running `check-s1c88.sh` after each batch.

> A from-scratch big-bang reshape (remove all symbols at once) was tried and **reset** — it leaves the
> build unverifiable-red for the whole grind. The dead WIP is in the reflog at **`417bed5`** and is a
> useful reference for the *end-state* register definitions (`ralloc.h`/`ralloc.c`/`main.c` with the
> S1C88-only register set). `git show 417bed5 -- src/s1c88/ralloc.h` to see the target.

## Verify

- `./scripts/dev.sh` → builds, runs `sdcc -ms1c88 --c1mode`, prints the emitted functions and `GREEN`.
- `./scripts/check-s1c88.sh [file.asm]` → **the codegen meter**: counts z80-only residue (de/bc pairs,
  iy?/ix? half-regs, `ex de,hl`) and names the functions still carrying it. Run it after each Step-2 batch;
  `TOTAL` should trend to 0. Baseline on a realistic 8-function input: **18** (de 14, bc 1, ex de,hl 3).
- **`./scripts/build-sdas.sh as88` → `bin/sdas88`** — the real validator/toolchain (decided: target
  SDCC's sdas/sdld; see `abi-decision.md` "Toolchain & validator"). The sdas build is proven in-sandbox;
  `sdas/as88/` is a buildable clone of `sdas/asz80`. **It still encodes z80** — retargeting the encoder
  (`s1c88pst.c`/`s1c88adr.c`/`s1c88mch.c` → real S1C88 ISA: App.A opcode map + CE/CF prefixes) is the next
  validator work, after which it wires into `check-s1c88.sh` to assemble emitted codegen as-is. Until
  then, `check-s1c88.sh` (the z80-residue meter) is the interim signal. (`bin/sdasz80` is an encoding
  oracle for shared mnemonics while building `s1c88mch.c`.)

## Map of everything

- `CLAUDE.md` — project overview, build, the overlay mechanics + gotchas, conventions.
- `docs/s1c88/abi-decision.md` — the codegen design + ABI + always-green strategy (authoritative).
- `docs/s1c88/` — distilled Epson manuals (architecture, ISA, addressing, toolchain).
- `src/s1c88/README.md` — per-file roles + retargeting checklist.
- `scripts/dev.sh` — fast build + smoke-test loop.
- Auto-loaded memory `sdcc88-bringup-status` — same state summary, loads every session.
- Tasks #17–19 (if the task list is still present) — the milestone breakdown.

## Commit history (branch `s1c88-retarget`, all green unless noted)

```
9bbf1d3  resumable: refresh CLAUDE.md + scripts/dev.sh
1ae5422  record always-green strategy
dbf3125  remove dead z80 variant symbols (clean single-port, 0 warnings)
0c95b6f  prune z80 variant PORT structs
faa64ad  record codegen design (Faithful BA+HL, sdas)
6fbc472  re-base onto z80 clone (skeleton: builds/links/runs)
2ba8d59  distilled Epson docs (docs/s1c88/)
e5b90d9  (main) original stm8-clone baseline
```
