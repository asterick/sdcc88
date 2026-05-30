# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION** below. Everything needed to continue is here or linked from here.

_Last updated: 2026-05-30. Branch: `s1c88-retarget`. State: **GREEN** (compiler builds, links, runs).
Step 1 of the reshape is **DONE** (commit `b606833`); current action is **Step 2**._

---

## TL;DR state

- **What skip-c is:** SDCC 4.5.0 retargeted to the Epson S1C88 (Pokémon Mini). See `CLAUDE.md`.
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
   - **Step 2 (task #18):** rewrite the `DE`/`BC` scratch uses in `gen.c` **function-by-function**,
     running `./scripts/dev.sh` after each batch. Map z80 `BC`→`BA`(B:A); the combined long asmops
     `DEHL`/`HLDE`/`HLBC`/`DEBC` → S1C88 `HL:BA` (long return per the ABI); drop `asmop_iyl`/`iyh`
     (IX/IY aren't byte-addressable). Delete each register symbol only once it has **no** remaining uses.
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
mechanical `BC→BA`/`DE→HL` sed: z80 code uses HL and DE together, so each site needs real analysis →
`BA`/`HL`/`IX`/`IY`/stack.

> A from-scratch big-bang reshape (remove all symbols at once) was tried and **reset** — it leaves the
> build unverifiable-red for the whole grind. The dead WIP is in the reflog at **`417bed5`** and is a
> useful reference for the *end-state* register definitions (`ralloc.h`/`ralloc.c`/`main.c` with the
> S1C88-only register set). `git show 417bed5 -- src/s1c88/ralloc.h` to see the target.

## Verify

- `./scripts/dev.sh` → builds, runs `sdcc -ms1c88 --c1mode`, prints the emitted functions and `GREEN`.
- A verification harness (assemble the output with `../skiploom` to prove it's valid S1C88) is **deferred
  by the user** — stand it up before/while doing Step 2 for real confidence (task is implicit; it's the
  biggest de-risker).

## Map of everything

- `CLAUDE.md` — project overview, build, the overlay mechanics + gotchas, conventions.
- `docs/s1c88/abi-decision.md` — the codegen design + ABI + always-green strategy (authoritative).
- `docs/s1c88/` — distilled Epson manuals (architecture, ISA, addressing, toolchain).
- `src/s1c88/README.md` — per-file roles + retargeting checklist.
- `scripts/dev.sh` — fast build + smoke-test loop.
- Auto-loaded memory `skip-c-bringup-status` — same state summary, loads every session.
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
