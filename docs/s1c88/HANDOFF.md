# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-05-31 (peephole audit — see "Peephole audit" below; plus native 16-bit sub/cp, adjustStack, non-ifx compare slices). Branch: **`main`** (all work is on main;
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
   - ~~**Byte-wise 16-bit ALU**~~ **DONE**. int `sub`/`cp`/`==`/`!=` native (`661555f` etc.); the byte
     fallback's L/H operands (`*p-*q` → `sbc a,h`, char `sub a,l`) now route through B via `emit3_8alu`
     (`6c49c73` compares, `a7e7235` genPlus/genSub).
   - ~~**Shifts/rotates** (`rr l`, `rr h`, `sla -N(ix)`, `rl -N(ix)`)~~ **DONE** (`ad12cb5`): S1C88
     shifts only target A/B/[HL]/[BR:ll]; new `emit3_shift` routes L/H/[ix+d] operands through A/B (carry
     chain preserved via flag-neutral `ld`). Also **removed peephole 21** (it re-folded the routed code
     back into illegal `sla m(ix)`). Constant int/long/char `<<`/`>>` (signed+unsigned) all 0 errors.
   - ~~`push af` stack reservation~~ **DONE** (`a69a11f`): `adjustStack` now emits native S1C88 SP moves —
     reserve via flag-safe `push hl`/`push a` filler, free via `add sp,#n` (flags dead) or `pop hl`/`pop iy`
     (flags live). Killed `push af`/`pop af`/`pop bc`/`pop de`. NB: on the S1C88 even `inc/dec sp` set
     Z C V N (unlike z80), so flag-preserving frees must use `pop <pair>`, not `inc/dec sp`.
   - ~~non-ifx signed compare~~ **DONE** (`eb3adcc`): `return a<b` now does `cp ba,hl; ld a,#1; jrs LT;
     xor a,a` (native), `>=`/`<=` add `xor a,#1`, unsigned uses `ld a,#0; rl a`. Killed `jp PO`/`rlca`;
     `outBitC` now emits `rl a` not `rla`.
   - ~~8-bit char compare~~ **DONE** (`6c49c73`): `sub a,l` (operand in L/H — the 8-bit ALU can't source
     L/H) now routes through B via new helper `emit3_8alu` (`ld b,l; sub a,b`, push/pop b if B live).
     Covers signed/unsigned char, ifx/non-ifx, `<`/`>`/`==`.
   - ~~signed/literal compare~~ **DONE** (`cd83036`): `if(a<100)` now emits native `cp {ba,hl},#imm` +
     `jrs LT/GE/NC`, replacing the illegal `xor #0x80`/`rla`/`ccf`/`rra` sign-map. **The whole compare
     test corpus (reg/eq/lc/ce/nb/ch/sc/li) is now 0 sdas88 errors.** Only the rare AOP_CRY (bit) signed
     result still keeps the z80 fixup. Compares are essentially retargeted; next clusters are the
     non-compare byte ALU and shifts below.
   - ~~8-bit AND/OR/XOR with L/H operand~~ **DONE** (`1e860c8`): `and a,l`/`or a,h`/`xor a,l` route through
     B via `emit3_8alu`, applied to genAnd/genOr/genEor. **`emit3_8alu` now always push/pop B** — these ops
     have no native 16-bit form so they hit the both-in-pairs case (`a&b`, a=BA b=HL) where B holds the
     accumulator's high byte mid-op (isRegDead reports B dead *after* the iCode, which was a latent
     clobber). push/pop B is correct everywhere; the genSub/genPlus/compare uses inherit the fix.
   - **NEXT — the C/D/E + DE/BC register-model grind** (the dominant residue; ~319 `PAIR_DE` refs total).
     **Partly done:** `getFreePairId`/`getDeadPairId` now return **BA** not BC/DE (`d17a167`) — fixed the
     scratch-pair picks (`sbc hl,bc`→`sbc hl,ba`, `add hl,bc`, the `ex (sp),hl` shuffle). **Remaining is
     several *distinct* idioms, each per-site (NOT one mechanical fix):**
     - **Callee-cleanup epilogue return-address shuffle** (`gen.c` ~7393–7460): the `bc_free||de_free`
       branch + the hard `else` path use `_push/_pop(PAIR_BC/DE)`, `ld e,(hl)`/`ld d,(hl)`, and
       **`ex (sp),hl`** (no S1C88 stack-exchange) to hop the return address over the param area. Retarget to
       BA/IX/IY scratch + `ld {hl,ba},[sp+dd]` (the S1C88 *has* SP-relative loads/stores — use them instead
       of the pop/push dance). Compare with the *other* epilogue path that already works (`pop hl; pop iy×N;
       jp hl`, seen in `_callee`).
     - **Stack-peek** (`pop de; pop hl; push hl; push de` to reload a 2nd stack word into HL) — replace with
       a direct `ld hl,[sp+dd]`.
     - **Variable-shift `C` loop counter** (`ld c,l`/`inc c`/`dec c`): when left=BA and result=HL use all 4
       byte regs there's no free byte for the counter — needs a 5th slot (stack temp, or `dec iy`/IX
       counter, or the native `DJR` after the value moves to HL). A real restructure of genLeftShift/
       genRightShift's `countreg` selection (it still lists `C_IDX`/`D_IDX`).
     - **saveRegs/restoreRegs around calls** + the remaining direct `asmop_de`/`DEHL`/`HLDE`/`HLBC`/`DEBC`
       combos and `_push(PAIR_DE)` scratch sites. Per `abi-decision.md` Step 2.
     - **⚠ DEBUGGING GOTCHA (cost me a whole session):** `emitDebug()` only prints with `--verbose`. Marker
       traces compiled *without* `--verbose` silently emit nothing → false "this code path isn't hit"
       conclusions. **Use `fprintf(stderr, …)` for instrumentation, or pass `--verbose`.** The `_vemit2`
       fprintf trick (check the rendered `p` buffer) reliably catches any emitted line.
     - **`inc/dec -N(ix)`** (indexed-mem INC/DEC illegal): peeph 116/117 fold removed (`725ca44`). The
       remaining direct `inc -N(ix)` (e.g. `sum_arr` counter) — my "not emit3_o" conclusion was from a
       *suppressed* emitDebug, so it's **probably emit3_o after all**; the reverted `emit3_incdec` helper
       (route stack INC/DEC through A: `ld a,d(ix); inc a; ld d(ix),a`, Z-safe via the trailing ld) was
       likely the right fix — **re-apply it** and verify with `--verbose`/fprintf. (Reflog has the helper.)
     - **`set 7,l`** (bit ops only on A/B/[HL]); **`neg`** form. Lower freq.
   - **Stack-peek `pop de;pop hl;push hl;push de` (idx/mul):** CONFIRMED via `_vemit2` fprintf trace it goes
     through `_push(pair)`/`_pop(pair)` with a *computed* `pair == PAIR_DE` (NOT the literal `_pop(PAIR_DE)`
     sites, NOT getFreePairId — already BA'd). The caller selects PAIR_DE some other way; locate it with
     `fprintf` of `pairId` + `__builtin_return_address(0)` in `_push`, or per-candidate-callsite markers.
     It peeks the 2nd stack word into HL — **retarget to `ld hl, d(sp)`** (the S1C88 SP-relative load;
     assembles). deref2's peek is already gone (getFreePairId→BA fixed it).
   - ~~Accumulator rotates~~ **DONE** (`644576e`): all 41 `emit3(A_RLA/RLCA/RRA/RRCA…)` sites + the live
     raw `emit2("rla")` substituted to the operand form `rl a`/`rlc a`/`rr a`/`rrc a` (also fixes the
     cost). Verified no site reads Z/S/V/P after a rotate (all carry-centric), so the operand forms' extra
     Z/S effects are unused. The peeph.def rotate rules match on the old output, so they just go dead (a
     couple micro-opts lost, no illegal output).
   - ~~`push af`/`pop af`~~ **DONE** (`4038be3`): `_push/_pop(PAIR_AF)` now emit `push a; push sc` /
     `pop sc; pop a` (no AF reg; SC = flag register; preserves A *and* flags). Covers the reachable
     callers. A few raw `emit2("push af")` sites remain in dead/rare paths (adjustStack's unreachable z80
     tail, the `#if 0` block) — none hit by fresh codegen; mop up if they ever surface.
   - Misc: `ex (sp),hl` (no S1C88 stack-exchange — arg/spill shuffle); `set 7,l`/bit set-res-test on L/H
     (S1C88 bit ops only on A/B/[HL]); `inc -N(ix)` (indexed-memory INC illegal); `neg` form. Lower freq.
   - **KNOWN GAP (verified 2026-05-31, deferred): out-of-range local signed conditional branch.** The
     CE-page signed conditions (`LT/LE/GT/GE/V/NV/P/M`, F-flags) are **short-only** — there is *no*
     `jrl LT`/`carl LT`. The peephole shortens `jp LT → jrs LT` only `if labelInRange`; out of ±127 B it
     leaves `jp LT`, which sdas88 hard-rejects (`jp takes hl or a (kk) vector`). So a function with a signed
     compare whose branch target is >127 B away **fails to assemble**. Correct lowering = invert-and-skip
     (`jrs GE,$skip ; jrl target ; $skip:`); best done in the assembler (it knows the true range), peephole
     keeps preferring in-range `jrs LT`. **`bcall`/`bjump` are NOT affected** — they only accept C/NC/Z/NZ
     (both forms exist), always emit the long form, and *reject* signed conditions (error, not miscompile;
     verified). Also TODO there: a clear "signed cond unsupported" diagnostic for `bcall LT`.
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

## Peephole audit (2026-05-31) — DONE

Audited **all** peephole rules for S1C88 validity, then **collapsed everything into one file**:
`src/s1c88/peeph.def` is now the single peephole definition (the z80 `peeph-z80.def` — the residual
branch -> `jrs`/`jrl`/`carl` mapping — was merged into its end as the "S1C88 control-transfer mapping"
section, kept LAST so it runs after the `jp->ret`/`jp->jr` passes; `main.c` now `#include`s only
`peeph.rul`; symbol renamed `_z80_defaultRules`->`_s1c88_defaultRules`). The 5 other `peeph-*.def`
variant files (ez80_z80/r2k/sm83/tlcs90/z80n) were never `#include`d — dead clutter — and were
**deleted**. We are a standalone S1C88 port, not a z80 variant, so there is one rules file.
Method: parsed every `replace…by…if` rule, ground-truthed each ambiguous form against `sdas88`
(e.g. `ret cc`, `add hl,sp`, `ld mem,#imm`, the S1C88 `BIT`/shift operand classes — all illegal),
then verified empirically by compiling a broad C corpus **with vs. without** peepholes and proving
the with-peepholes illegal-instruction set is a strict **subset** of the gen.c (`--no-peep`) baseline.
Invariant now holds: **peepholes introduce zero new illegal instruction forms.** Findings:

- **Dropped 64 dead/illegal rules from `peeph.def`** (z80 idioms with no legal S1C88 analog): the
  `ex de,hl`/DE-BC pair shuffles, `add hl,sp` frame-access folds (S1C88 uses `[SP+dd]`), `ex (sp),hl`
  epilogue folds, z80 `bit n,reg` (S1C88 `BIT` is a different AND-test), `rlca`/`rrca`, the `ld c/e,#imm`
  conditional-set variants, the 32-bit-compare folds (194-1/2), `isPort()`-dead rules (162a, 167/djnz),
  and **peephole 161 `jp cc→ret cc`** — which was *live* and miscompiling (S1C88 has no conditional
  return; it emitted illegal `ret Z`/`ret C`).
- **Dropped `peeph-z80.def` rule 178** (`xor a,a; ld d(ix),a → ld d(ix),#0`) — immediate-to-indexed
  (and -absolute) memory store is illegal on S1C88 (only `ld (hl),#nn` exists); it was live.
- **Hardened 7 fold rules** (99, 118, 154x, 176b…) with `notSame(%N 'l' 'h' 'c' 'd' 'e')`: their z80
  `canAssign('b' %N)` guard was a stale proxy for "`or a,%N` legal" — true on z80, **false** on S1C88
  where `ld b,l` is legal but `or a,l` is not. The guard blocks promoting a `ld`-source into an illegal
  8-bit-ALU source while keeping the fold for legal (memory/imm/a/b) operands.
- **Guarded 52a/52c** (`push %1; pop %2 → ld split`) with `…'de' 'bc'`: `canSplitReg` still splits the
  (always-green) `de`/`bc` into nonexistent `d/e/c` bytes, turning gen.c's illegal `push hl; pop de`
  into illegal `ld e,l; ld d,h`. Now restricted to the byte-addressable pairs (ba/hl).
- **Kept** the intentional `jp→jr→jrs` branch-shortening pipeline (160/162/163/164 + peeph-z80 j1–j10):
  the scanner's `jr`/`jp` flags there are false alarms (peeph-z80 converts `jr→jrs`, `jp→jrl`).

The remaining `--no-peep`-baseline illegal forms (`push/pop de`, `inc -N(ix)`, `ex (sp),hl`, `djnz`,
`cpl`, `cp a,l`, `neg`, `add hl,de`, `bit 7,b`, out-of-range `jp GE`) are all **gen.c** residue (the
register-model grind / the documented out-of-range-signed-branch gap), **not** peepholes.

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

Recent (codegen): `725ca44` drop peeph 116/117 (illegal `inc/dec m(ix)` fold) · `d17a167`
getFreePairId/getDeadPairId → BA (killed `sbc hl,bc`/`ex (sp),hl` scratch
picks) · `4038be3` _push/_pop(PAIR_AF) → push a;push sc (killed `push af`) · `644576e` acc-rotates
→ operand form `rl a`… (killed `rla`/`rlca`) · `1e860c8` genAnd/genOr/genEor L/H operand via B
(emit3_8alu always-safe) · `ad12cb5` shifts route L/H/[ix+d] through A/B + drop peephole 21 (killed `rr l`/
`sla -N(ix)`) · `a7e7235` genPlus/genSub L/H byte-ALU via B · `6c49c73` genCmp/gencjne route L/H operand
through B (killed 8-bit `sub a,l`) · `eb3adcc` genCmp non-ifx signed/bool native (killed `jp PO`/`rlca`) ·
`a69a11f`
adjustStack native SP moves (killed `push af`) · `661555f` gencjne native `cp hl,ba` (`==`/`!=`) ·
`1dc9d94` genCmp native `cp ba,hl` (ifx 16-bit compares) · `fa05339` genMinus native `sub ba,hl`.
Toolchain: `33948cb` romgen + ROM test · `dced778` auto-bank works · `8da1910` linker built ·
the `sdas88: …` series (full ISA, byte-verified) · `f95e7fb`/`85af71a`/`384472a` codegen slices (frame,
branches, signed-compare). Earlier: the call-ABI + allocator reshape (`b606833` Step 1, `aee2ed0` etc.).
