# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-05-31 (native 16-bit sub/cp, adjustStack, non-ifx compare slices). Branch: **`main`** (all work is on main;
there is no `s1c88-retarget` branch anymore — CLAUDE.md's mention of it is stale). State: **GREEN** —
compiler builds/links/runs, and the
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
   via `peeph-z80.def`), **signed compare → native `jrs LT/GE`** (`genCmp`/`genIfxJump`; the
   `z80instructionSize` branch-sizing in `peep.c` came with it), **16-bit `sub ba,hl`** (genMinus/genSub,
   `fa05339`), and **16-bit `cp ba,hl` for ifx compares** (genCmp, `1dc9d94`). Note (verified vs sdas88):
   the byte-wise `sub a,l` / `sbc a,h` idiom is *illegal* on the S1C88 — the 8-bit ALU source must be A or B,
   never L/H — so the 16-bit ALU work is **required**, not just an optimization. The native 16-bit
   sub/sbc/cp on the two ALU pairs (BA, HL) all assemble; `rr l` / `rrc l` are also illegal (`rr`/`rrc` take
   only a, b, (br:ll), (hl)). New helper `aluPairId()` recognizes BA (the z80 getPairId helpers don't).
   **Remaining** (the validator list):
   - **Byte-wise 16-bit ALU — remaining sites.** Done: int `sub`, int `cp` (ifx `<`/`>`/…), int `==`/`!=`
     (`gencjne` native `cp hl,ba`, `661555f` — killed the illegal `sbc hl,bc`). Still byte-wise:
     - **8-bit char compare/sub** with right in L/H emits `sub a,l` — 8-bit ALU can't source L/H; move
       through B (or memory) first.
     - **operands the allocator leaves outside ALU pairs** (e.g. `*p-*q` puts a byte in H → `sbc a,h`) —
       the broader operand-placement grind.
     - Note: register-operand long (4-byte) `<`/`==` is handled now; the common stack-operand long compare
       was already legal (byte-wise on memory: `sub a,d(ix)`/`sbc a,d(ix)`).
   - ~~`push af` stack reservation~~ **DONE** (`a69a11f`): `adjustStack` now emits native S1C88 SP moves —
     reserve via flag-safe `push hl`/`push a` filler, free via `add sp,#n` (flags dead) or `pop hl`/`pop iy`
     (flags live). Killed `push af`/`pop af`/`pop bc`/`pop de`. NB: on the S1C88 even `inc/dec sp` set
     Z C V N (unlike z80), so flag-preserving frees must use `pop <pair>`, not `inc/dec sp`.
   - ~~non-ifx signed compare~~ **DONE** (`eb3adcc`): `return a<b` now does `cp ba,hl; ld a,#1; jrs LT;
     xor a,a` (native), `>=`/`<=` add `xor a,#1`, unsigned uses `ld a,#0; rl a`. Killed `jp PO`/`rlca`;
     `outBitC` now emits `rl a` not `rla`.
   - ~~8-bit char compare~~ **DONE** (`6c49c73`): `sub a,l` (operand in L/H — the 8-bit ALU can't source
     L/H) now routes through B via new helper `emit3_8alu` (`ld b,l; sub a,b`, push/pop b if B live).
     Covers signed/unsigned char, ifx/non-ifx, `<`/`>`/`==`. **Remaining compare z80-isms**: signed
     **literal** compare (`if(a<100)` → the `xor a,#0x80`/`sub`/`rla`/`ccf`/`rra` sign-map path, which the
     register-only native `cp` block skips); the rare AOP_CRY (bit) signed result still keeps the z80 fixup.
   - `rlca`/`rla`/`rrca`/`rra` → `rlc a`/`rl a`/… (**flag-subtle** — z80 acc-rotates are carry-only,
     S1C88's set Z/S too; check each use site). `outBitC`'s use is done; others remain.
   - 16-bit shifts (`rr l` — *illegal*, see above; `sla -2(ix)`); `inc d(ix)`.
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
(`a13bc77`); genPlus 16-bit add prefers BA → `add hl,ba` (`aee2ed0`); genMinus/genSub native `sub ba,hl`
(`fa05339`); genCmp native `cp ba,hl` for ifx 16-bit compares (`1dc9d94`). The **call ABI is done**
(returns BA/HL:BA, args faithful Epson order). Next pieces: chained 16-bit `sbc` for the **long
sub/compare** (kills the illegal `sbc hl,bc`), the **8-bit `sub a,l`** mop-up (move L/H through B), then
the broader operand-placement work so the allocator keeps 16-bit operands in BA/HL more often.

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

Recent (codegen): `6c49c73` genCmp/gencjne route L/H operand through B (killed 8-bit `sub a,l`) ·
`eb3adcc` genCmp non-ifx signed/bool native (killed `jp PO`/`rlca`) · `a69a11f`
adjustStack native SP moves (killed `push af`) · `661555f` gencjne native `cp hl,ba` (`==`/`!=`) ·
`1dc9d94` genCmp native `cp ba,hl` (ifx 16-bit compares) · `fa05339` genMinus native `sub ba,hl`.
Toolchain: `33948cb` romgen + ROM test · `dced778` auto-bank works · `8da1910` linker built ·
the `sdas88: …` series (full ISA, byte-verified) · `f95e7fb`/`85af71a`/`384472a` codegen slices (frame,
branches, signed-compare). Earlier: the call-ABI + allocator reshape (`b606833` Step 1, `aee2ed0` etc.).
