# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-06-02 (session 10: **ISR prologue/epilogue retargeted** to the S1C88 RETE model —
`push ba/hl/iy` save, `rete` return, no `ei`/`pusha`/`reti`; 0 errors across ISR shapes. New gap logged:
non-ISR `__critical` (di/ei → SC-bit masking). See "Session 10". Session 9: **indirect / function-pointer
calls retargeted** — `f(x)` via ptr /
`v->op()` / `tbl[i]()` now use `jp hl` (tail) or manufactured-return + `jp hl` (call), no `jp (iy)`/DE-BC/
`___sdcc_call_*`; 0 errors. New gap logged: ISR prologue/epilogue (`reti`→`rete`, save-all, 2 latent
`jp (iy)`). See "Session 9". Session 8: **s1c88 is now a fully independent port** — renamed the 44 globals
that collided with the z80 port, so build.sh no longer `--disable`s the other ports; a full all-ports build
links `-ms1c88`/`-mz80`/… into one driver with 0 collisions, s1c88 codegen byte-identical. See "Session 8".
Session 7: **register-model cleanup — all *active* C/D/E + DE/BC z80-isms
cleared** (bitfield store→B, callee-cleanup epilogue→IY, genSwap free-reg→A/B); a broad `--c1mode` sweep is
0 errors + 0 residue. NEW gap found: **indirect/function-pointer calls** aren't retargeted (`jrl (iy)`,
BC pointer load, `___sdcc_call_*` — a separate workstream). Remaining DE/BC residue is deeply latent
(~40 unreachable sites → the dead-symbol sweep). See "Session 7" below. Session 6: **codegen emits
`bcall`/`bjump` for inter-function calls/tail-jumps**
(linker picks form + bank switch), and **`.globl` is now emitted for compiler support routines** — so a
`--c1mode` corpus assembles with **0 sdas88 errors everywhere** (the last `jrl __mulint` false-positive is
gone). End-to-end compile→assemble→link verified. See "Session 6" below. Session 5: **`ldir` is fully
eliminated from codegen** — genBuiltInMemcpy (incl. variable/runtime count via a borrowed-IX 16-bit counter
+ `cp ix,#0` zero guard) / genPointerSet / genPointerGet / genRet all emit the native S1C88 byte loop
(HL=source, IY=dest); the feared latent garbage-`aop_stk` bug did NOT recur. Session 4:
stack-word peeks + IY/BA arg-push, variable-shift IY counter, `ex(sp),hl`→`ld dd(sp)`, signed-literal
compares→native `jrs` (drop `ccf`), immediate→indexed-store fix. Session 3: offsetPair native-add,
`add hl/iy/ix,sp` elimination, struct-return SIGSEGV fix; ldir attempt reverted — see "Session 3").
Branch: **`main`** (all work is on main;
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

## Session 10 (2026-06-02) — ISR prologue/epilogue retargeted (RETE model)

**`4e825c5`:** `__interrupt` functions emitted z80-isms (`!ei` at entry, the save/restore-all
`!pusha`/`!popa` = `push af/bc/de/hl/iy`, and `reti`/`retn`) — all illegal on the S1C88 (`ei`/`di`/`reti`/
`retn` don't exist). Per the Epson model (cpu-operation-interrupts.md §3.5) the exception sequence
auto-evacuates **CB:PC and SC** (result flags + the I0/I1 interrupt-priority mask) and **RETE restores
them**, so:
- **Prologue:** save only the GP regs the handler may clobber — `push ba; push hl; push iy` (IX via the
  frame setup). No SC/AF save (RETE handles SC), no `ei` (same-or-lower priority masked until RETE).
- **Epilogue:** `pop iy; pop hl; pop ba`, then `rete`.
- **Return:** always `rete` (NMI/critical/normal — it restores the mask).

Verified 0 sdas88 errors for a plain ISR, an ISR that calls a function, and an IY-heavy ISR; save set is
balanced. Full 13-input corpus + rom-smoke + link-smoke GREEN. The two latent epilogue `jp (iy)` sites are
now provably unreachable (one gated on `IS_RAB||IS_TLCS90` ≡ 0, the other needs an ISR with stack params —
impossible) → they DCE out, to be deleted with the dead-branch sweep.

**NEW GAP (task #12): non-ISR `__critical`** still emits the z80 idiom (`ld a,i; di; push af` … `pop af;
jp PO; ei`). S1C88 fix = the SC priority bits (I1=bit7, I0=bit6): `push sc; or sc,#0xC0` (mask all maskable)
/ `pop sc` (restore) — for both the function-level paths and the block-level `genCritical`/
`___sdcc_critical_enter` helpers. Separate workstream.

## Session 9 (2026-06-02) — indirect / function-pointer calls retargeted

**`bc03fff`:** calls through a function pointer (`f(x)` via ptr, `v->op(a,b)`, `tbl[i]()`) were illegal —
they emitted `jp (iy)` (peephole'd to bogus `jrl (iy)`), a BC pointer load, and `call ___sdcc_call_hl`/
`___sdcc_call_iy` helpers that don't exist for the S1C88. The only register-indirect transfer the S1C88 has
is **`jp hl`** (verified: `jp iy`/`call hl`/`jrl (iy)`/`carl (iy)` all illegal). Replaced the whole
HL/IY/BC/DE/UNIMPLEMENTED branch chain in genCall's `PCALL` case with one legal HL path: move the pointer
to HL, then **tail-jump** = `jp hl`; **call** = manufacture a return address in a free pair (IY normally, BA
under `--reserve-iy`), `push` it, `jp hl` — the callee's RET returns to the label right after. No runtime
helper, no DE/BC. Near function pointers are same-bank so plain `jp hl` is correct (far/banked ptrs = the
deferred 3-byte ABI). Verified 0 sdas88 errors across tail/non-tail/struct-member/table-indexed forms; full
12-input corpus + rom-smoke + link-smoke GREEN.

**NEW GAP found — ISR prologue/epilogue (task #11).** `__interrupt` functions emit `reti` (S1C88 = `rete`),
the save/restore-all `push bc/de/af`…`pop …` over the z80 register set, and the two remaining latent
`jp (iy)` epilogue sites (genEndFunction merged-adjust ~7262 + ISR ~7375). 28 sdas88 errors on a 2-ISR
test. Separate workstream; not reachable from ordinary direct/indirect-call codegen.

## Session 8 (2026-06-02) — s1c88 is now a fully independent port (links alongside z80 et al.)

**sdcc88 no longer has to `--disable` every other port to build.** It was a verbatim z80 clone that kept the
z80 port's global symbol names, so the two couldn't coexist in one `sdcc` binary (duplicate symbols) — hence
build.sh disabled all 24 stock ports. Fixed:
- **`c8757ba` rename the 44 colliding globals** → unique `s1c88_*` names. The set was found *exactly* by
  intersecting the two ports' object symbol tables (`nm src/z80/*.o` ∩ `nm src/s1c88/*.o`). Covers the
  `z80_*`/`z80X` family, `genZ80Code`→`genS1C88Code`, `dryZ80iCode`, `Z80RegFix`, `regsZ80`, the
  peephole predicates, `convertFloat`/`findAssignToSym`/`regWithIdx`/`regsUsedIniCode`/`sm83_regs`/
  `should_omit_frame_ptr`, the C++ `move_parms`, and the asm-dialect tables `_asxxxx_z80`/`_gas_z80`/….
  Special cases: `rUmaskForOp` (internal) vs `z80_rUmaskForOp` (wrapper) got distinct names; the `isFree`
  collision is the DEFSETFUNC *function* only (→`s1c88_isFree`) — the identically-named `reg_info` bit-field
  is untouched. None of the 44 are referenced from SDCC core (core reaches the port via the PORT struct +
  peephole hooks), so the rename is fully contained to `src/s1c88`.
- **`4ece413` build.sh drops all `--disable-*-port`** (keeps only the peripheral disables). 

Verified: a full all-ports build (`mcs51 z80 ds390 pic14 pic16 hc08 stm8 pdk mos6502 f8 s1c88`) links with
**0 duplicate-symbol errors** into one driver; `-ms1c88`/`-mz80`/`-mstm8` all work in that single binary;
the s1c88 objects share **zero** globals with z80; and `-ms1c88` codegen is **byte-identical** to the old
standalone build across the 11-input corpus. dev.sh + rom-smoke + sdas88 validation GREEN throughout.

## Session 7 (2026-06-02) — register-model cleanup: active C/D/E + DE/BC z80-isms cleared

**Found by a broad `--c1mode` sweep (bitfields, callee-cleanup epilogues, swaps, long ALU, varargs,
struct copy, pointer chains): three paths still emitted z80 scratch regs that don't exist on the S1C88,
each reachable under realistic code. All fixed (`a369049`), 0 sdas88 errors + 0 textual de/bc/iy?/ix?
residue across the whole sweep; rom-smoke + link-smoke GREEN.**
- **genPackBits** (bitfield store, both merge sites): `getFreePairId` INVALID → PAIR_BC → `ld c,a; … or a,c`
  (C nonexistent, `or a,c` illegal). Now stashes the shifted value in **B** (the only legal non-A `or`
  source), `push b`/`pop b` when B live. `p->bf=v` → `ld b,a; ld a,(hl); and a,#mask; or a,b; ld (hl),a`.
- **genEndFunction callee-cleanup epilogue**: the return-address hop took `bc_free || de_free` →
  `pop de; …; push de`. `de_free` is *always* true on the S1C88 (D/E never hold a return value), so it
  always fired with a phantom pair. Replaced with the **`iy_free`** branch (`pop iy; add sp,#n; push iy`) —
  IY is the only free pair there (HL holds the return value, IX was just restored as the frame pointer).
- **genSwap** (2-byte same-reg swap): the free-scratch scan ran A→B→C→D→E and, since C/D/E are
  phantom-"dead", always settled on `ASMOP_E` (`ld e,…`). Restricted to **A/B**.

**NEW GAP IDENTIFIED — indirect / function-pointer calls (a *separate* workstream, deferred).** `f(x)` via a
function pointer, `v->op(a,b)`, `table[i]()` are NOT retargeted: they emit `jrl (iy)` (S1C88 jumps via
register only with `jp hl`; there is no `jp iy`/`jrl (iy)`), load the pointer through BC (`ld c,(hl);
ld b,(hl)`), and `carl ___sdcc_call_hl`/`___sdcc_call_iy` trampolines that lack `.globl` (and need S1C88
definitions). Fixing it = the `jp hl` register-indirect idiom + a real-pair pointer load + the
`___sdcc_call_*` helper/`.globl` story. (Direct calls — the common case — are fine via Session 6's
bcall/bjump.)

**Remaining register-model residue is now deeply latent** (~40 literal `push de`/`pop de`/`ex (sp),hl`/
`ld {d,e,c},…` emit sites in gen.c) but **unreachable under realistic direct-call codegen** — they sit
behind IS_SM83 / banked / z88dk-fastcall / `--reserve-iy` / specific top-of-stack layouts the allocator
avoids (verified: the broad sweep + in-place swaps + long ALU hit none). Eliminating them is the
dead-symbol sweep (abi-decision.md Step 2 "delete each symbol once unused") — a separate, larger refactor
touching ralloc/the `*_IDX` enum/`_pairs[]`.

**Dead-code sweep (`3f1b6c3`):** removed the now-unused `isLastUse()` (its last caller went in the
genPackBits rewrite), and **made the sub-port predicates compile-time constants** in `s1c88.h` — sdcc88 is
single-variant (`z80_opts.sub = SUB_Z80` set once, never changed), so `IS_Z80≡1` and SM83/Z180/Rabbit/
TLCS90/eZ80/Z80N/R800 ≡ 0. This lets the compiler **dead-code-eliminate the ~494 unreachable z80-variant
branches** (which carry most of the latent DE/BC/ldi/ex idioms) instead of compiling them in. Proven
behavior-neutral: **byte-identical codegen across the full 11-input corpus** (modulo the filename-derived
`.module` line). NEXT level of this: physically delete the now-`if(0)` branch *source*, which removes the
latent DE/BC textual references and unblocks deleting the C/D/E/DE/BC symbols themselves.

## Session 6 (2026-06-02) — bcall/bjump for inter-function calls + `.globl` support routines

**Codegen now emits the banked pseudo-ops for direct inter-function transfers, and the last validator
false-positive is gone — a `--c1mode` corpus is now 0 sdas88 errors *everywhere*.** Commit `da38e3f`:
- **`genCall` named-symbol path → `bcall _f` (call) / `bjump _f` (tail-jump).** The linker becomes the
  single place that picks short/long form + inserts/omits the `ld nb` bank switch (per
  `banked-branch.md` + memory [[codegen-prefer-bcall-bjump]]). Indirect/register (`call (iy)`, `___sdcc_
  call_hl`) and literal-address calls are left on the existing path. Cost bumped to the 6-byte worst-case
  slot.
- **Two peephole rules** mirror the z80 tail-call opts 135/136 for the pseudo-ops: `bcall f; ret`→`bjump f`
  (+ the `pop ix` variant), guarded by `symmParmStack`.
- **`.globl` for compiler support routines** (`__mulint`/`__divsint`/`__mullong`/…): they are created by
  `funcOfType()` with `cdef=1` and never enter SDCC's publics/externs sets (`convertToFcall` only marks
  them extern for `TARGET_PIC_LIKE`), so the assembler rejected the undefined reference (verified genuinely
  required: stock `sdasz80` errors identically). `genCall` now `addSetIfnotP(&publics, csym)` for a `cdef`
  target so `printPublics` emits the `.globl`. **This kills the long-standing `jrl __mulint` validator
  flag.**
- **Verified end-to-end:** compiled C with `bcall _f` assembles AND links — the linker resolves the slot to
  `[ld nb omitted] carl <disp>` for a bank-0 target (decoded from a linked `.ihx`). Tail calls become
  `bjump`. corpus/stk/wide/calls/support-routine corpora all 0 errors; rom-smoke + link-smoke GREEN.

**Next:** the residual DE/BC register-model cleanup (saveRegs/restoreRegs around calls, remaining `push de`
scratch sites, the `asmop_de`/long-combo machinery) per `abi-decision.md` Step 2; full struct return-by-
value (the copy half works now via genRet's byte loop); and the documented out-of-range signed `jp LT`
assembler gap.

## Session 5 (2026-06-01) — the `ldir` struct-copy cluster, eliminated

**Net result: `ldir` is gone from every fixed-size copy path.** The S1C88 has no `ldir` and no `DE`, so the
z80 block-copy idiom (`ld e,l; ld d,h; ld hl,..; ld bc,#n; ldir` and its `ex de,hl` variants) was illegal.
All five emit sites now use a native forward byte loop — `ld a,(hl); ld 0(iy),a; inc hl; inc iy; <dec/djr>;
<branch>` with **HL = source, IY = dest** — committed as five always-green slices on `main`:
- **`114d44f` genBuiltInMemcpy** (struct copy/assign): IY ← dest, HL ← source (clobber-safe ordering via
  genMove dead-set; IY non-byte-addressable so it never disturbs L/H), `ld b,#count`, `djr nz`. A/B saved
  via BA/AF when live, with correct `_G.stack.pushed` accounting so the SP-relative address math stays right.
- **`57e4ecd` genPointerSet** (`p->longmember = v`, stack-source scalar store): same loop; source via
  `!ldahlsp` (peephole → `ld hl,sp; add hl,#off`), dest pointer → IY.
- **`5176132` genPointerGet** (read-pointer-into-stack): source pointer → HL (rightval via `offsetPair`;
  IMMD folds into the literal), dest stack address → IY (`setupPairFromSP` handles IY). Dropped the dead
  EZ80 `lea`/ix sub-branch.
- **`e9551f2` genRet** (struct return-by-value / bigreturn): read the caller's hidden buffer pointer in one
  16-bit `ld iy,(hl)`, source in HL, byte loop. `gget` (`struct S t=*p; return t;`) byte-loops both copies.
- **`148f9cf` genBuiltInMemcpy count > 255**: borrow the frame pointer **IX** as a 16-bit counter
  (`push ix; ld ix,#n; … dec ix; jp NZ` → peephole `jrs NZ`; `pop ix`). IX isn't referenced inside the
  loop, and the address loads run before the push, so (ix+d)/SP offsets are unaffected.
- **`f651906` genBuiltInMemcpy variable (runtime) count**: load `n` → IX first, `cp ix,#0; jrs Z,end` zero
  guard (so count==0 copies nothing instead of wrapping to 65536), `dec ix; jrs NZ` loop. Removed the dead
  z80 fallback (setupForMemcpy-into-DE, the n==1/2/≤4 `ldi` cases, the ldir/Rabbit paths) + unused
  saved_BC/DE/HL. **This was the last ldir — codegen now emits none.** The feared 3-operand clobber didn't
  materialise: count→IX loads while still in its source, and the pair loads use A/SP-relative, never IX.

**The feared "latent garbage-`aop_stk` bug" did NOT recur.** Verified clean (0 sdas88 errors) across
register-pointer, stack-source, stack-dest, global, and IMMD operands — `local=*p`, `*p=local`, `g=local`,
stacked long args, `__builtin_memcpy(d,s,16)` and `(…,300)`. The earlier revert was caused by the original
attempt's cost model driving the *middle-end* to route tiny `*d=*s` through memcpy into a bad stack-address
path; this change only fires for genuine memcpy iCodes, so unrelated codegen is untouched (copy_small/
copy_word are byte-identical to baseline). **valgrind reconfirmed unusable here:** the WSL2 ld.so is
stripped, and although a downloaded `libc6-dbg` has the matching build-id, valgrind's mandatory early
`strlen`-in-ld.so redirect isn't satisfied by `--extra-debuginfo-path` (and there's no root to install it).

**`ldir` is now gone from EVERY codegen path** — the variable-count `__builtin_memcpy(d,s,n)` was finished
in `f651906`: load runtime `n` → IX first (while still in its source location), then the source/dest pair
loads (register moves / `ld pair,dd(sp)` — they use A as scratch but never touch IX, and 16-bit pair loads
don't need IX as a frame pointer, so the count survives and the feared 3-operand clobber doesn't bite),
then `cp ix,#0; jrs Z,end` zero guard + `dec ix; jrs NZ` loop. The dead z80 fallback (setupForMemcpy-into-
DE, the n==1/2/≤4 `ldi` cases, the ldir/Rabbit paths) was removed. Verified 0 sdas88 errors for register-,
stack-, and call-result-count memcpy. rom-smoke GREEN throughout. **The only validator flag anywhere is now
the unrelated `jrl __mulint` support-routine `.globl` false-positive** (item 2 above).

## Session 4 (2026-06-01) — register-model grind: peeks, shifts, compares, indexed stores

**Net result: every *tractable* register-model z80-ism is cleared.** The compare/shift/arg-passing/stack
peek+store/frame-addressing clusters all validate **0 errors**; `corpus2` is down to **3 errors, all of
which are the `jrl/carl __mul*/__div*` library-symbol false-positives** (see below). The ONLY remaining
real z80-ism anywhere is the **struct-copy `ldir` cluster** (blocked — see end of this section).
rom-smoke GREEN throughout; no regressions across the corpora.

**Part A — stack-word peeks + arg marshalling (corpus 38 → 10):**
- **`151ee36` native `ld pair,dd(sp)` for stack-word peeks:** the S1C88 has
  `LD {ba,hl,ix,iy},[SP+dd]` (dd a *signed byte*) — 16-bit pair loads support `[SP+dd]` but **NOT
  `[IX+dd]`** (verified vs ISA: pair loads only take `[hhll]/[HL]/[IX]/[IY]/[SP+dd]`, no IX-displacement
  form). Added a native SP-relative load branch to BOTH peek sites — `fetchPairLong` (the `offset==2`/`==0`
  pop/push special cases) and `genCopyStack`'s result-load loop — replacing the z80
  `pop de; pop hl; push hl; push de` (and `pop hl; push hl`) dance. Killed the long-return epilogue peeks
  (`ladd`/`lsub` now end `ld hl, 2 (sp)`) and the member-addr peek. Guarded to pairs BA/HL/IX/IY and
  `|dd| <= 127`; the existing pop/push remains the out-of-range fallback.
- **`3fad33a` push stack/frame args through IY, not phantom DE/BC:** `genIpush`'s load-then-push path
  picked `ASMOP_DE`/`BC` because `de_free`/`bc_free` report **true** (the z80 D/E/B/C bytes are phantom
  always-dead regs), emitting illegal `ld e,N(ix); ld d,N(ix); push de`. When BA+HL are busy, prefer the
  real free index pair **IY**: `ld iy,dd(sp); push iy` (IY has both the SP-relative load and `push`). Gated
  to non-literal sources (the literal-caching loop writes pair byte halves, which IY lacks). Cleared
  `lmul`'s long-arg marshalling. *Subtlety verified correct:* the two words of a stacked long both emit
  `ld iy,10(sp)` — the frame offset (6→4) drops by 2 exactly as SP drops by 2 after the first `push iy`,
  so both resolve to SP+10.
- **`d37814e` `push ba` for a BA-resident low word:** `getPairId_o` deliberately doesn't recognize BA
  (A low, B high), so genIpush's direct-push branch missed a word already in B:A and built it in phantom
  BC (`ld c,a; push bc`). Use **`aluPairId`** (which *does* recognize BA) to take the direct `push ba`.

**Part B — shifts, compares, indexed stores (corpus 10 → 3; new wide corpus also clean):**
- **`b3dcc3f` variable-shift counter → 16-bit IY** (was the documented "hard" `ld c,l` case): for
  `int<<int`/`long<<int` all four byte GPRs hold value/count/result, so there is no 5th byte for the
  counter (the z80 had C). `genLeftShift` now uses **IY** when no byte reg is free — `inc iy / jr / loop /
  dec iy; jr nz` (16-bit `dec iy` sets Z V N). Allocator-agnostic; the count loads straight into IY
  (`ld iy,dd(sp)` for a stacked count, `push hl; pop iy` for a register one) and the value-move's `iy_dead`
  is cleared. `genRightShift` already fell back to legal `A_IDX`, so untouched. Verified char/int/long,
  signed+unsigned, left+right: 0 errors; 32-bit shift is a correct `add a,a; rl b; adc hl,hl` chain.
- **`3e21b03` `ex (sp),hl` → `ld dd(sp),{ba,hl}`** (`cpy`'s `*d=*s`): `genCopy`'s register→stack store used
  the z80 stack-exchange (S1C88 `EX BA,SP` swaps with the SP *register*, not memory). The S1C88 has direct
  SP-relative pair stores `ld dd(sp),BA`/`ld dd(sp),HL` (74/75); use them (any in-range offset, both pairs
  via `aluPairId`, no HL-dead requirement). Last `ex (sp),hl` in the compare/pointer corpus gone.
- **`ab3503c` signed *literal* compares → native `jrs LT/GE`** (drops illegal `ccf`): a signed compare vs a
  literal whose operand was on the stack, or was a `long`, fell to the z80 `xor#0x80/rl a/ccf/rr a` sign-
  flip — and `ccf`/`scf`/`rcf` don't exist on the S1C88. genCmp now (a) loads a 16-bit stack operand into a
  dead HL for native `cp hl,#imm`, and (b) for the byte-wise/long case emits a plain `sub/sbc a,#imm` chain
  and branches on the native S^V (`signed_native` at `fix:`). Only the rare AOP_CRY result keeps the old
  map. Verified across char/int/long × ifx/bool × both senses × reg/stack/literal: 0 errors, branch senses
  correct.
- **`5f3a3ee` no immediate→indexed store** (`ld d(ix),#imm` is illegal; only `ld (hl),#nn`): two sites —
  `aopPut`'s AOP_STK store routed constants through A (was direct), and **`z80canAssign`** (the `canAssign`
  peephole hook, `peep.c`) stopped reporting an immediate as assignable to ix/iy memory, which is what let
  **peepholes 9/9a** fold `ld a,#0; ld d(ix),a` back into the illegal `ld d(ix),#0` (the generic root cause
  the session-2 removal of explicit rule 178 missed). Both `--no-peep` and peephole modes now emit 0 such
  stores. (Diagnosing this: `--no-peep` made it vanish → peephole; `port->peep.canAssign` is the hook.)

**Remaining (everything else is clean) — one *non-register-model* item (the `.globl` false-positive):**
- ~~**Struct-copy `ldir` cluster**~~ **DONE (session 5)** — see "Session 5" below. ldir is eliminated from
  **all** copies: struct copy/assign + `__builtin_memcpy` of any count (genBuiltInMemcpy — B counter ≤255,
  borrowed-IX 16-bit counter >255, and IX + `cp ix,#0` zero guard for runtime counts), scalar store-
  through-pointer (genPointerSet), read-into-stack (genPointerGet), and struct return-by-value (genRet).
  All use the native byte loop
  `ld a,(hl); ld 0(iy),a; inc hl; inc iy; djr nz` (HL=source, IY=dest). **The "latent garbage-`aop_stk`
  bug" did NOT surface** with this approach — the byte loop computes correct SP-relative offsets across
  register/stack/global/IMMD operands (verified `local=*p`, `*p=local`, `g=local`, stacked args). The
  earlier revert was specific to the original attempt's cost model driving the middle-end to route tiny
  copies oddly; the current change only fires for memcpy iCodes (copy_small/copy_word byte-identical to
  baseline). valgrind was reconfirmed unusable in this sandbox (stripped ld.so; downloaded libc6-dbg
  build-id matches but `--extra-debuginfo-path` doesn't satisfy the mandatory ld.so `strlen` redirect; no
  root). **ldir is now gone from every codegen path** — the variable/runtime-count case was finished in
  `f651906` (see "Session 5"); the feared 3-operand clobber didn't bite (count→IX loads first, the pair
  loads use A/SP-relative and never touch IX).
- **`jrl __mulint`/`carl __mullong`/`jrl __divsint`** — NOT a gen.c z80-ism; **validator false-positives.**
  sdas88 errors `<u> undefined symbol` (verified: *reference* `sdasz80` errors identically — `.globl` is
  genuinely required, not an as88 bug). Our codegen emits `.globl` for called **C** externs (`_ext` is
  declared) but NOT for compiler support routines: `SDCCopt.c convertToFcall` only marks them extern for
  `TARGET_PIC_LIKE`, so `__mulint` never enters SDCC's `externs` set (`--emit-externs` doesn't help). This
  is upstream-middle-end glue, *outside* the `src/s1c88` overlay, and tangential to the register grind. It
  intersects **user directive #4** (emit `bcall`/`bjump` for inter-function calls): switching gen.c ~6961
  `jp/call` → `bjump/bcall` is the right next step there, but a call to an *external* still needs the
  `.globl`, and the whole thing wants full assemble→link (rom-smoke) validation — a separate workstream.

## Session 3 (2026-06-01) — frame-addressing + struct-return; ldir attempt reverted

**Landed (committed, green):**
- **`9958749` offsetPair → native `add {hl,ix,iy},#imm`:** adding a constant offset to a pointer pair
  (struct/array member addressing) used a z80 `ld de/bc,#off; add hl,de/bc` scratch idiom; HL/IX/IY all
  have a native immediate-add, so no scratch/push-pop. Kills `ld de,#off; add hl,de` for member addressing.
- **`c1c2d88` eliminate `add {hl,ix,iy},sp`** (peephole): the S1C88 16-bit add takes BA/HL/IX/IY/#imm,
  not SP. But `ld {hl,ix,iy},sp` and `add {hl,ix,iy},#imm` ARE legal, so a peephole rewrites the whole
  class `ld pair,#off; add pair,sp` → `ld pair,sp; add pair,#off` (off+SP == SP+off). One rule covers
  ~18 raw gen.c sites + the `!ldahlsp` macro + setupPairFromSP. **`add hl,sp` fully gone from the corpus.**
  A follow-up rule drops the degenerate `add pair,#0`. (Done as a peephole like jp→jrs, not 18 edits.)
- **`a48f26d` struct/union return-by-value no longer SIGSEGVs:** `return *q` (a struct) hit
  `genMove(aopRet(structtype)==NULL, …)` → null deref. Root cause: a bigreturn uses the hidden-buffer ABI
  (no return register), and the frontend lowers the RETURN to a pointer-sized operand that slips past
  genRet's `size<=4 && !IS_STRUCT` guard. Now: genRet emits `UNIMPLEMENTED` when `aopRet()` is NULL, and
  genMove guards `if(!result) return;`. Struct return-by-value reports "Unimplemented" (exit 1), no crash.
  **Full struct-return is still unimplemented** (needs the bigreturn copy-to-buffer + the ldir work below).

**Attempted + REVERTED — the `ldir` struct-copy retarget:**
- The S1C88 has no `ldir`/`ldi`/DE/BC. A copy loop `ld a,(hl); ld (iy),a; inc hl; inc iy; djr nz` works
  (verified building blocks; dest computed as `ld iy,sp; add iy,#off`, valid for AOP_STK *and* EXSTK —
  no IX/omitFramePtr dependence). **Struct *assignments* compiled correctly and validated 0 errors**
  (`*a=*b`, `gp=*q`, `struct l=*q`, `__builtin_memcpy(d,s,16)`).
- **Key map:** struct assignment lowers to **`genBuiltInMemcpy`** (NOT genPointerGet as first assumed).
  There are **6** `ldir` emit sites: ~7695 (genRet struct-return), ~14932 (genPointerGet),
  ~15606 (genPointerSet), ~16250, **~17240 (genBuiltInMemcpy — the one struct copies use)**, ~17463.
- **Why reverted:** the new fast path perturbed the regalloc *cost* model, shifting allocation in
  unrelated code and exposing a **latent garbage-SP-offset bug** (below) — `cpy`'s 1-byte `*d=*s`
  regressed from `ld (hl),b` to a 1-byte `ldir` with `add hl,#-523687714`. Real miscompile → reverted.
- **To land safely:** make the fast-path `cost2()` values allocation-neutral, and fix the garbage-offset
  bug first. The byte-loop approach itself is sound.

**⚠ KNOWN ISSUE — latent garbage SP-offset (uninitialized `aop_stk` read):**
- Symptom: a small (1–2 byte) copy emits `ld hl,#<garbage>; add hl,sp` (now peephole'd to `ld hl,sp;
  add hl,#<garbage>`), garbage e.g. `-523687714`. The value **changes between runs → uninitialized
  memory**. `gcc -Wmaybe-uninitialized -O2` does NOT flag it ⇒ it's a *union* field: almost certainly
  `aop->aopu.aop_stk` read to compute an SP offset for an aop whose type is NOT AOP_STK/EXSTK/STL (so
  `aop_stk` is uninitialised union memory). ~30 `aop_stk` reads in the copy paths; one lacks a type guard.
- **Latent** — committed codegen doesn't hit it; it surfaced only under the ldir change's allocation shift,
  and could not be re-triggered deterministically afterward. **Repro recipe for the fix:** re-trigger via
  a copy-cost perturbation (the ldir fast path, or a high-spill small-copy), run sdcc under **valgrind** —
  it flags the uninitialised read at the exact `aop_stk` site — then add the missing AOP_STK/EXSTK guard.

## gen.c grind progress (2026-05-31, session 2) — 6 slices landed

Cleared the small/self-contained grind residue, always-green, each validated with
`sdas88` (and crash-checked — see the gotcha below). Commits on `main`:
- **`cpl a`/`neg a`** — S1C88 CPL/NEG need an explicit operand; the bare z80 form
  is rejected. (`b0771cf`)
- **indexed/abs INC/DEC → through A** (`emit3_incdec`): S1C88 INC/DEC target only
  A/B/L/H/[HL]/[BR:ll], never [IX+d]/abs. Routes via `ld a,mem; inc/dec a; ld
  mem,a`; PUSH/POP/LD are flag-neutral so Z survives for the multi-byte
  carry-skip idiom (`inc low; jr NZ; inc high`). (`00a5fac`)
- **`cp a,l`/`sub a,l` → through B** (route genCmp/genSub byte ops via
  `emit3_8alu`). (`6e083b8`)
- **`djnz` → `djr nz`** (all 6 sites; same B-counter semantics). (`54c9dcd`)
- **`bit n,reg` → `bit reg,#mask`** (`emitBitTest`): S1C88 BIT is a logical
  AND-with-mask (`bit {a,b,[hl]},#nn`). A/B direct, L/H/mem routed. (`3beafa6`)
- **RES/SET eliminated** (no such instruction): `res 7,a`→`and a,#0x7f`; genAnd/
  genOr single-bit opts → `and/or a,#mask` (A-only; B/mem fall through — AND/OR's
  destination is only A). Also dropped peeph 61/75/76 (latent z80 res/set/bit
  folds the literal-mnemonic audit scanner missed — they use a *placeholder*
  operator `same(%N 'bit' 'res' 'set')`). (`b01eb03`)

**⚠ METHODOLOGY GOTCHA:** `sdcc ... 2>/dev/null` HIDES compiler crashes (FATAL
internal errors / asserts) — a stale `.asm` from a previous run then looks fine.
Always compile with stderr visible and grep for `Internal Error|backtrace|FATAL`
*before* trusting the emitted asm. (`aopGet` asserts `!regalloc_dry_run`; helpers
that call it must guard the call — this bit emitBitTest.)

**Remaining residue (all the DE/BC register-model grind + known gaps):**
`push de`/`pop de` (stack-peek `push hl;pop de;pop hl;push hl;push de`), `ex
(sp),hl` (epilogue/arg shuffle), `add hl,sp` (frame-address; use IX or `[SP+dd]`),
`ld de,#imm`/`add hl,de`/`ex de,hl` (DE scratch for 16-bit addr arithmetic →
BA/IX), `ldir` (struct copy → loop), and the two hardcoded `bit 7,e`/`bit 7,d`
sites in the signed-compare hard path. Plus the documented **out-of-range `jp GE`**
(assembler-level, deferred). This is the central `_pairs[]`/`PAIR_DE` machinery —
do it as one focused effort (see "Step 2" in abi-decision.md), not rushed.

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
- **Follow-up (flag semantics):** the reorder rules **96a/b/c** (move `inc/dec hl` across a neighbouring
  instruction) were safe on the z80 only because its 16-bit `inc/dec` are flag-transparent. On the S1C88
  16-bit `inc/dec` set `Z/V/N` (only `C` preserved), so the reorder changes the live flags — added
  `notUsed('f')` so they only fire when flags are dead. **96d** (push/pop) is flag-neutral, left as-is.
  The hazard was *latent* (the existing `operandsNotRelated(…,hl)` guards already make these nearly-dead
  on our register model — guarded-vs-disabled output is identical on the corpus). A scanner gap to note
  for any future re-audit: rules can hide illegal/flag-unsafe ops behind a *placeholder operator*
  (`same(%N 'bit' 'res' 'set' 'rl' …)`) — match those, not just literal mnemonics (this is how 61/75/76
  slipped the first pass).

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

Recent (codegen): `5f3a3ee` no immediate→indexed store (aopPut through-A + z80canAssign drops the
`ld d(ix),#imm` peephole fold) · `ab3503c` signed-literal compares native `jrs LT/GE` (drop illegal `ccf`)
· `3e21b03` reg→stack store `ld dd(sp),{ba,hl}` not `ex (sp),hl` · `b3dcc3f` variable-shift counter → 16-bit
IY (kill phantom `ld c`) · `d37814e` genIpush `push ba` for BA low word (via aluPairId) · `3fad33a` push
stack/frame args through IY not phantom DE/BC (`ld iy,dd(sp); push iy`) · `151ee36` native
`ld pair,dd(sp)` stack-word peeks (kill pop/push-peek in fetchPairLong + genCopyStack) · `725ca44` drop
peeph 116/117 (illegal `inc/dec m(ix)` fold) · `d17a167`
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
