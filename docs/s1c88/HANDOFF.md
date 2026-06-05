# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

_Last updated: 2026-06-05 (session 16: **#7a — the dead variant-branch sweep — is COMPLETE**: all 599
sub-port variant-macro refs (468 in gen.c + 131 in peep.c/ralloc2.cc/main.c/ralloc.c) constant-folded
or deleted; the `IS_Z80/IS_SM83/…` predicates, the `Z80_SUB_PORT` enum, the SM83 reg table, the
GB/RGBDS/ISAS/r2k dialect tables, the gbz80 add/sub helpers and `genMultTwoChar` are GONE (~2,000
lines). Collateral: **every `ex (sp),hl` and `ldir` emit site is now gone** (19 of #7b's 20 ex-sites
lived in dead variant branches); `push de` is down to 4 latent sites. **And a fresh corpus blind spot
found+fixed (the s14 lesson again): the four remaining z80 builtins** — `__builtin_memset/strcpy/
strncpy/strchr` are registered and REACHABLE and emitted `ldi/ldir`/DE/`jp PO/PE` (19 sdas88 errors) —
retargeted to native byte loops in the s5 memcpy style; `scripts/corpus/17_builtins.c` added.
**Corpus: 17/17 byte-identical, 0 sdas88 errors; rom/link/branch smoke GREEN.** See "Session 16". —
Earlier: session 15: **the last reachable z80-isms are gone** — `genMult`'s literal path
(`a112acf`) now uses BA/B (`add hl,ba` 16-bit loop; multiplicand in B for the `add/sub a,b` byte loop;
byte-granular `push a`/`push b` saves; native `SEP` for the in-genMult sign-extend path; peep.c sizes
`sep`). `4fe9ed8`: block-scope `extern void f(void); f();` now gets its `.globl` (was a REAL
assemble failure, not a validator false positive). `0913cf9`: **a fresh corpus blind spot found and
fixed** — variable `char*char` (genMultOneChar, the z80 shift-add loop with DE) was corpus-invisible;
now emits the native **`MLT`** (`HL←L*A`, CE D8; best case `mlt; ld a,l; ret`), and the corpus gained
`16_mult.c` covering the whole multiply cluster. `47eb41c`: **task #10 closed** (`jp <signed cc>` lowers
in sdas88 to the fixed `jrs <inv>,+4; jrl e` invert-and-skip) — **and, found while implementing it, a
CRITICAL toolchain-wide bug: every relative branch was ONE BYTE SHORT** (sdas88 inherited the z80
next-instruction disp base; the S1C88 base is one byte earlier — `PC←PC+rr+1`, confirmed vs both the
Epson §4.3.3 semantics and PokeMini's `JMPS: PC = PC + OFFSET - 1`). Fixed assembler-side everywhere
(local resolution + a +1 R_PCR addend bias so the stock z80 linker math lands on the S1C88 base — no
linker change); `scripts/branch-smoke.sh` locks the convention byte-for-byte. **Corpus: 16/16 files at
0 sdas88 errors — the first fully-GREEN corpus.** See "Session 15". —
Session 14: **built a byte-identical corpus harness** (`scripts/corpus/` +
`corpus-check.sh`) which **disproved "functionally complete"** — a broader corpus exposed REACHABLE z80-isms
the narrower prior corpus missed. Fixed 3 (`a6bf7e4` `or a,l/h` in `&&`/`||`; `20b9d72` bitfield
`set/res`+`rld/rrd`; `5f890f1` `ld (iy+d),#imm`), then `8596ef3`/`718263a` (ldir block copy, STL-address
BA). **Also: there is NO codegen
heisenbug** — a long false hunt was edited-vs-stale builds; always rebuild via the overlay
(`dev.sh`/`corpus-check.sh`), never raw `make -C build/.../src`. See "Session 14". —
Earlier state: the codegen retarget was believed **functionally complete**: assemble→link→banked-ROM GREEN,
s1c88 an independent port alongside z80. Remaining = cleanup + ABI completeness (tasks
#7 register-model dead-code/symbol sweep [started s13], #8 IX/IY args, #9 far pointers, #10 assembler
signed-branch) — see "NEXT ACTION". session 13: register-model dead-code sweep STARTED (`cee1680`, dead
RAB/TLCS90 epilogue removed) + 3-part scope recorded. session 12: **`__critical` retargeted** to SC
interrupt-level masking (`push sc; or sc,#0xc0` / `pop sc`, no ei/di) — ei/di/reti/ld a,i/jp PO now gone
from all reachable codegen. session 11: **struct/union return-by-value implemented** — `return *p` now
copies sizeof(return type) bytes to the caller's hidden buffer (HL=source, IY=`*(sp+off)`, byte loop);
0 errors across struct-return forms. See "Session 11". Session 10: **ISR prologue/epilogue retargeted** to
the S1C88 RETE model —
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
there is no `s1c88-retarget` branch anymore). State: **GREEN** — compiler builds/links/runs, the binary
toolchain (assembler + linker + banked ROM) is complete, and the codegen retarget is **functionally
complete**. The remaining work is cleanup + ABI completeness (the register-model symbol sweep + IX/IY args
+ far pointers + the assembler signed-branch gap)._

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

## NEXT ACTION (do this)

1. Confirm green: `./scripts/dev.sh` → builds the compiler + smoke test → `GREEN`.
2. The codegen retarget is **functionally complete for the verification corpus** — every *reachable*
   z80-ism it exposes is gone: **16/16 corpus files assemble with 0 `sdas88` errors**, and the full
   assemble→link→banked-ROM pipeline is GREEN. (Session 14 proved "functionally complete" claims are
   only as strong as the corpus — keep extending `scripts/corpus/` when touching new codegen territory.)
   All the feature gaps are closed: compares / 8- and 16-bit
   ALU / shifts (sessions 1–4), the **`ldir` struct-copy cluster** (session 5), **`bcall`/`bjump` for
   inter-function calls + `.globl` for support routines** (6), the **active C/D/E+DE/BC z80-isms** (7),
   **independent port** linking alongside z80 et al. (8), **function-pointer calls** (9), **ISR
   prologue/epilogue** (10), **struct return-by-value** (11), **`__critical`** (12), the s14 corpus
   findings (`or a,l/h`, bitfield `set/res`/`rld/rrd`, `ld (iy+d),#imm`, `ldir` block copy, STL-address
   BA) (14), **`genMult` literal path + variable 8×8 `MLT` + block-scope extern `.globl` + the #10
   `jp <signed cc>` lowering** (15). See the per-session entries below.
3. **Remaining work = the open tasks** (cleanup + ABI completeness, not functional gaps):
   - **#7 — register-model dead-code sweep / symbol removal.** **Tier (a) — the dead variant branches —
     is DONE (session 16): zero variant-macro refs remain port-wide,** and it took 19 of the 20
     `ex (sp),hl` sites and every `ldir` emit with it. What's left:
     **(b)** the 4 latent `push de` sites (see "Session 16" — only the genIpush `d_free` byte-push
     fallback ~5769 is even theoretically reachable; needs a register-pressure repro), and
     **(c)** the structural symbol removal — PAIR_DE (182), PAIR_BC (74), ASMOP_DE (47), C/D/E_IDX
     (149), IYL/IYH_IDX (107) refs in gen.c, plus the `*_IDX` ordinals keyed into `ralloc2.cc`'s Boost
     allocator. Always-green, build + byte-identical after each step.
   - **#8 — ABI Phase 2: IX/IY argument passing** (int 3rd/4th + near-ptr 1st/2nd currently spill to stack).
   - **#9 — ABI Phase 3: 3-byte far pointers + `_near`/`_far` memory model** (large).
4. **Validator workflow** (how to check a slice): compile with `sdcc -ms1c88 --c1mode -o /tmp/x.asm`, then
   `scripts/validate-s1c88.sh /tmp/x.asm` (assembles with `sdas88`; any reject = a z80-ism to fix). For a
   refactor, also confirm **byte-identical** codegen across the corpus (the strongest safety check).
5. **Per user direction (applied):** inter-function calls/tail-jumps emit **`bcall`/`bjump`** so the linker
   is the single place that picks branch form + bank switch (memory `codegen-prefer-bcall-bjump`).

### ~~KNOWN GAP — out-of-range local signed conditional branch (task #10)~~ DONE (session 15, `47eb41c`)
sdas88 now lowers `jp <signed cc>,e` to the fixed 6-byte invert-and-skip (`jrs <inv>,+4 ; jrl e`;
pairs lt/ge le/gt v/nv p/m fN/nfN) and `jp c/nc/z/nz,e` to `jrl cc`. peep.c sizes them 6/3.
**`bcall`/`bjump` were already unaffected** (C/NC/Z/NZ-only, always long, clean error on signed cc).

### ⚠ THE BRANCH DISPLACEMENT CONVENTION (fixed s15 — read before touching branch emission)
The S1C88 computes a taken relative branch as **PC ← PC(after full fetch) + disp − 1** (Epson §4.3.3
`JRS rr → PC←PC+rr+1`; PokeMini `JMPS: PC = PC + OFFSET - 1`). So an **8-bit rr is relative to the rr
byte's own address**, and a **16-bit qqrr is relative to (first disp byte + 1)** — both one byte EARLIER
than the z80 next-instruction base the ASxxxx code inherits. sdas88 was off by one on EVERY relative
branch until `47eb41c`; nothing caught it because no ROM is ever executed here and the smoke expectations
had been derived under the same wrong convention. The fix is assembler-side only: local resolution uses
the S1C88 base, and cross-area R_PCR relocs get a **+1 addend bias** so the stock z80-convention linker
(`sdldz80` — it cannot be target-gated) lands on the right base. `scripts/branch-smoke.sh` byte-locks
every form (jrs/cars/jrl/carl/djr/jp-lowering, forward + backward); run it whenever branch emission or
the linker patch changes.

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

## Session 16 (2026-06-05) — #7a dead variant-branch sweep COMPLETE; builtins retargeted

**Tier (a) of the register-model sweep is done — the port has ZERO sub-port variant-macro references —
and the sweep exposed + fixed another reachable builtin cluster.** Commits `b824303`→`dba51c2`:

- **The sweep (b824303, 44c68d2, 9301956, b42f2d6, f282e72):** all 599 refs to the compile-time-dead
  predicates (`IS_SM83/IS_RAB/IS_TLCS90/…` ≡ 0, `IS_Z80` ≡ 1) folded away. Method, reusable for #7c:
  (1) scripted whitespace/newline-aware boolean-term strips with the conjunct-tail shapes (`a && IS_X
  || b`, `a || IS_X && b`) excluded as unsafe; (2) a string/comment-aware **statement-level constant
  folder** (`/tmp`-only tooling, not in-repo) that brace-matches each `if/else if` whose condition
  mentions a macro, evaluates it tri-state, and deletes/unwraps/rewrites — run to fixpoint, every
  transform logged; dangling-`else` in else-if chains handled (a deleted `else if (F) B` tail must take
  its `else` with it). Pitfall to remember: Python `'' in str` is True — an end-of-condition guard bug
  made parenthesized macros at condition end look like unknown atoms until fixed. (3) manual leftovers
  (wasserts, bool initializers, multi-line conditions, `IS_RAB * 120` arithmetic). Deleted whole: the
  caller-less `_gbz80_emitAddSubLongLong`/`_gbz80_emitAddSubLong`/`genMultTwoChar`, the
  `s1c88_sm83_regs` table + `SM83_MAX_REGS`, the GB/RGBDS/ISAS/r2k dialect tables in mappings.i (main.c
  references only `asxxxx_z80`/`gas_z80`/`z80asm_z80`), the `Z80_SUB_PORT` enum + `Z80_OPTS.sub`, and
  the predicate `#define`s themselves. The two `AOP_DIR wassert (IS_SM83)` guards became `wassert (0)`
  traps (SM83-only direct space). gen.c 18,104 → ~16,100 lines. **Byte-identical at every step** (the
  corpus exercises the cost paths heavily, so an allocation-affecting mistake would show as DIFF).
- **`c66d76c`:** genAssign's force-disabled z80 ldir block-copy path (s14's `l_better = false`) deleted.
- **#7b collateral:** 19 of the 20 `ex (sp),hl` sites and most `push de`/`ldi(r)` sites lived in the
  dead variant branches — **gen.c now has ZERO `ex (sp),hl` and `ldir` emit sites**. `push de` remains
  at exactly 4 latent sites: genIpush smallc C/E_IDX (~5544, needs `--smallc` + phantom regs), genIpush
  byte-push `aopInReg D_IDX` (~5726, phantom), genIpush byte-push `else if (d_free)` fallback (~5769 —
  **the only maybe-reachable one**: fires only if A, B, L(H?) are all unfree for a byte push; needs a
  repro), and genFunction callee-saves `deInUse` (~6528, requires C/D/E allocation ⇒ never).
- **`dba51c2` builtins retargeted (the s14 lesson, third time):** `__builtin_memset/strcpy/strncpy/
  strchr` are registered in `_z80_builtins` and REACHABLE; they emitted `ldi/ldir`, DE loads and
  `jp PO/PE` parity loops (19 sdas88 errors on a 5-shape test). All four now emit native byte loops in
  the s5 style (HL=src/scan, IY=dst, B or borrowed-IX counter): memset straight-line ≤4 / B-loop ≤255 /
  IX-loop above with `ld (hl),#imm|a|b` fill; strcpy with `or a,a` NUL re-test AFTER the 16-bit incs
  (they clobber Z on the S1C88!) and the original dst pushed (IY) for the returned value; strncpy with
  a remaining-store counter + zero-pad tail (A already holds the NUL); strchr with `cp a,#imm`/`cp a,b`
  BEFORE `inc hl` so the found-pointer is exact, NULL via `ld hl,#0`. Byte-granular `push b` saves when
  the counter/comparand clobbers a live B. The z80 `setupForMemcpy` (DE-based regMove) is deleted; the
  clobber-safe HL/IY load is the shared `setupHLSrcIYDst()` (memcpy deduped, byte-identical).
  Hand-traced: ms_keep (live-B fill), sc_ret (saved-IY result via BA), sn_wide (300-count pad), sr_var
  (`cp a,b`). **`scripts/corpus/17_builtins.c`** locks all shapes; baseline re-snapshotted (17 files).

Remaining: **#7b** is now just the 4 `push de` sites (above) + any other allocator-avoided z80-isms a
IY-pressure repro can surface; **#7c** the structural symbol removal (PAIR_DE 182, PAIR_BC 74,
ASMOP_DE 47, C/D/E_IDX 149, IYL/IYH_IDX 107 refs in gen.c after the sweep); **#8** IX/IY args; **#9**
far pointers.

## Session 14 (2026-06-02) — verification corpus + reachable z80-isms it exposed

**Built a byte-identical corpus harness and used it to fix reachable codegen bugs that the prior narrower
corpus missed — "functionally complete" was overstated.**

- **`865cef1` corpus harness** — `scripts/corpus/01..15_*.c` (15 diverse pre-cpp'd inputs) + `scripts/
  corpus-check.sh {snapshot|check}`: overlays+builds, compiles the corpus, normalizes the filename-derived
  `.module` line, reports per-file **IDENTICAL/DIFF vs baseline** + **sdas88 errors**. See memory
  [[sdcc88-corpus-harness]]. It immediately surfaced **real reachable** instructions sdas88 rejects (a
  program using these features would not assemble) — the previous "0 errors" held only for the narrower
  corpus tested before.
- **`a6bf7e4` `_toBoolean` `or a,l/h` → through B.** `a && b` / `a || b` collapse a 16-bit value with
  `ld a,h; or a,l`, but the 8-bit OR source can't be L/H. Routed via `emit3_8alu` (the B-routing session 1
  added to genAnd/Or/Eor; _toBoolean bypassed it). 02_logic 2→0 errors.
- **`20b9d72` genPackBits drop `set/res` + `rld/rrd`.** `f->bit1 = 1` emitted `set N,(hl)` (no set/res on
  S1C88); a nibble-aligned `f->nib = v` emitted `rld/rrd` (no BCD nibble-rotate). Both z80 fast paths gated
  on `IS_Z80`(≡1) so live; deleted → the general and/or-mask merge (already legal). 06_bitfields 3→0.
- **`5f890f1` aopPut AOP_IY/EXSTK route immediate through A.** `g=1; f();` (const store to a global that
  lands in IY because HL is busy before a call) emitted `ld 0(iy),#imm` — illegal. Session 4 fixed the
  identical **AOP_STK** case (`|| isConstantString(s)`) but missed **AOP_IY/AOP_EXSTK**. 09_isr `ld
  0(iy),#0x01`→`ld a,#1; ld 0(iy),a`. (Ripple: a 14_mixed global const-store moves `ld hl,#g; ld (hl),#n`
  → `ld a,#n; ld (#g),a` — byte-identical *encoding*, verified `CE D4 ..` == `ld (g),a`; 0 new errors.)

**⚠ METHODOLOGY (cost me hours; now a memory [[sdcc88-build-via-overlay-only]]):** rebuild the port ONLY
via the overlay (`dev.sh`/`corpus-check.sh`, which `cp` repo `src/s1c88` → build tree before `make`).
Raw `make -C build/.../src` compiles the build tree's **stale own copy**, not your edits. Mixing the two
produced inconsistent output that masqueraded as **non-deterministic codegen / an uninitialized-memory
heisenbug**. There is NO such heisenbug — codegen is deterministic per binary (`MALLOC_PERTURB_`, env-pad,
8× reruns all stable); the "flicker" was edited-vs-stale builds. Verify an edit landed:
`strings build/.../sdcc | grep <token>`.

- **`8596ef3` genAssign drop z80 `ldir`/`ldi` block-copy.** `long g1=g2` / `swap_l` emitted `ld de,#g; ld
  bc,#4; ldir` + `ex de,hl` (no ldir/ldi/DE on S1C88). The path was gated only by a cost model (`l_better`)
  — forced off so mem→mem copies fall through to genMove's native byte/pair copy. 13_swap 25→0. (The dead
  z80 cost-model+ldir block stays for the task-#2 sweep.) Large mem→mem genAssign is now straight-line, but
  rare (large structs go through genBuiltInMemcpy's loop, s5).
- **`718263a` genPlus STL-address+operand uses BA not DE.** `&arr[i] = framebase + index` moved the addend
  into DE → `add hl,de` (illegal). Now prefer BA (S1C88 2nd ALU pair), save/restore when not dead; HL setup
  from the AOP_STL address (`ld hl,#off; add hl,sp`) never touches A/B so BA survives → `add hl,ba`.
  14_mixed/15_stack 6→0. **Hand-traced** every &arr[i]/&arr[j] (the corpus checks legality+non-regression,
  NOT runtime semantics — there is no execution test, so the A/BA grind must be hand-verified). 14_mixed's
  inner loop restructured by cost perturbation but every address is verified correct (one harmless redundant
  `add hl,#0`).

~~**STILL OPEN — one reachable z80-ism left = `genMultLit` (multiply-by-constant)**~~ **DONE (session 15,
`a112acf`)** — see "Session 15". The `bcall _ext` undefined-symbol flag turned out to be a REAL bug
(block-scope externs missing `.globl`), fixed in `4fe9ed8`; a fresh blind spot (variable `char*char`)
fixed via native `MLT` in `0913cf9` + corpus file `16_mult.c`. **Corpus now: 15/16 files 0 errors; only
12_arrays (the deferred #10 `jp GE`) remains.**

## Session 15 (2026-06-05) — genMult literal path retargeted to BA/B; block-scope extern .globl

**The last reachable z80-ism is gone — every corpus file except 12_arrays (#10, assembler-level) now
assembles with 0 sdas88 errors.**

- **`a112acf` genMult literal path (genMultLit) → BA/B.** `int b = a*3` emitted `ld c,l; ld b,h;
  add hl,hl; add hl,bc` (pair defaulted to phantom-dead PAIR_BC/PAIR_DE). Retargeted all four sub-paths:
  - **16-bit loop:** addend in **BA** (the only 2nd ALU pair), `add hl,ba`; HL set up by `ld l,a; ld h,b`
    after `genMove(ASMOP_BA, left)` (genMove, not fetchPair — it handles operand/pair overlap).
  - **byte loop in A** (`!add_in_hl`): multiplicand in **B**, CSD chain `add a,a` + `add/sub a,b` —
    A is both the accumulator and BA's low byte, so it can't also hold the addend.
  - **byte loop in L** (`add_in_hl`, HL dead): addend byte is **A**; **B is untouched** (its garbage high
    half only feeds H, which a byte result never reads) — so a live B needs no save here.
  - **signed-byte operand:** native **`SEP`** (sign-extend A into B, `CE A8` byte-verified) replaces the
    z80 `rlc/sbc/ld` shuffle. (In practice the middle-end widens via CAST first — which emits its own
    legal `rlc a; sbc a,a` — so this path is defensive; peep.c's `s1c88instructionSize` was taught `sep`.)
  - **Saves are byte-granular** (`push a`/`push b` + `_G.stack.pushed += 1`), decided per byte from
    `isRegDead`, and **restored after the result genMove**: a pair `push ba` could restore over a result
    byte landing in A. `spillPair(PAIR_BA)` after the loops (A/B no longer match the literal cache).
  - **Hand-traced** (no execution test): `mi3/mi10/mi100/mc7` CSD chains; `mc3` (live B → `push b/pop b`
    around the byte loop); `m_keep_a` (allocator parks a live char in B; the L-path leaves B untouched);
    `m_keep_a16` (`push b` around an int multiply); `deep` (7c=(2c+c)·2+c, 9d=8d+d). 09_isr 5→1 errors;
    the only corpus diff is the multiply site (`ld c,l`→`ld a,l`, `add hl,bc`→`add hl,ba`).
- **`4fe9ed8` block-scope extern calls get `.globl`.** `void g(void){ extern void f(void); f(); }` never
  emitted `.globl _f` — SDCCglue's publics walk only adds used **level-0** functions; inner scope tables
  aren't walked — so sdas88 rejected the `bcall` as undefined. This was a REAL assemble failure
  (stock `sdasz80` rejects identically), not the "link-resolved false positive" it was logged as.
  Extended the s6 cdef hook in genCall: register `level > 0` called symbols in `publics` too (level>0
  can't duplicate the glue's level-0-only entries; file-scope extern verified still exactly one `.globl`).
  09_isr 1→0 errors.
- **`0913cf9` genMultOneChar → native `MLT` (a fresh corpus blind spot).** The corpus had **no variable
  8×8 multiply**, so `char a*b`'s z80 shift-add loop (`ld e,l; ld d,l; add hl,de; djr nz` — DE doesn't
  exist) survived "functionally complete" — exactly the s14 lesson. Replaced with the native **`MLT`**
  (`HL ← L*A`, `CE D8`, 2 B/12 cyc, MODEL1/3 — the Pokémon Mini core has it): operands → A and L
  (commutative swap prefers in-place regs and keeps HL-reading operand loads in the L slot, after A),
  byte-granular `push a` when A is live non-operand; **MLT preserves B**, so a live B needs no save
  (verified: an allocator-parked char in B survives). Best case `char a*b` = `mlt; ld a,l; ret`. The
  z80 loop + the dead SM83/Z180/Z80N/RAB/R800 variant blocks (~220 lines) deleted. peep.c: `mlt` size
  rule ungated; the use/def predicates audited for bare `mlt` (conservative fallbacks; SurelyWritesFlag
  matches Minx MUL flags Z,N set / C,V cleared). **`scripts/corpus/16_mult.c` added** — the whole
  multiply cluster (literal CSD int/byte, live-A/live-B saves, widened chars, global stores, variable
  8×8 MLT, `__mulint`/`__mullong` support calls). (genDiv/genMod stay support-call-only, as on z80;
  native `DIV` (`L←HL/A, H←rem`, CE D9) is a future optimization, not a gap.)

- **`47eb41c` task #10 closed + a CRITICAL toolchain-wide off-by-one found and fixed.** Implementing the
  #10 lowering surfaced it: **every relative branch sdas88/sdldz80 emitted was one byte short.** The
  S1C88 branch base is `PC ← PC(after full fetch) + disp − 1` (Epson §4.3.3; confirmed independently
  against PokeMini's `JMPS`/`CALLS` macros: `PC.W.L = PC.W.L + OFFSET - 1`) — an 8-bit rr is relative to
  the rr byte's own address, a 16-bit qqrr to (first disp byte + 1) — one byte earlier than the inherited
  z80 next-instruction base. The App-A byte verification covered opcodes (not disp arithmetic), the smoke
  expectations were hand-derived under the same wrong convention, and no ROM is ever executed in-repo, so
  nothing caught it. Fix (assembler-side only, `sdas/as88/s1c88mch.c`): local-resolution formulas for
  jrs/cars/jrl/carl/djr use the S1C88 base; every cross-area `R_PCR` emission **biases the addend by +1**
  so the stock z80-convention linker math lands on the S1C88 base (the linker runs as `sdldz80`, so it
  can't be target-gated — and now needs no change at all, incl. the bcall/bjump slot, whose
  `R_S1C88_BANK` reloc is emitted before the bias so the bank byte stays true). link-smoke expectation
  corrected 0x01FA→0x01FB; **new `scripts/branch-smoke.sh`** byte-locks every form fwd+back.
  And **#10 itself**: sdas88 lowers `jp <signed cc>,e` → fixed 6-byte `jrs <inv>,+4 ; jrl e`
  (pairs lt/ge le/gt v/nv p/m fN/nfN; all 10 forms byte-verified), `jp c/nc/z/nz,e` → `jrl cc`;
  peep.c sizes jp hl/(kk)/basic-cc/signed-cc = 1/2/3/6. 12_arrays 3→0 errors → **corpus 16/16 GREEN**.

Remaining: **#7** the register-model dead-code/symbol sweep (cleanup, not functional), **#8** IX/IY
args, **#9** far pointers.
**Keep extending the corpus** when touching codegen territory it doesn't cover — two sessions in a row
proved "complete" claims are only as strong as the corpus. (And note the off-by-one lesson: **the corpus
validates legality, not encodings** — `branch-smoke.sh` exists because displacement arithmetic is
invisible to both the corpus and the App-A opcode verification.)

## Session 13 (2026-06-02) — register-model dead-code sweep (task #7) STARTED

**`cee1680`:** deleted the dead RAB/TLCS90-only "merged stack adjustment" epilogue in genEndFunction (gated
on `IS_RAB||IS_TLCS90` ≡ 0; it carried the illegal `jp (iy)`). Byte-identical across the 15-input corpus.

**Scope assessment for the rest of #7 (the register-model finish):** it splits into three parts of very
different risk —
- **(a) SAFE / byte-identical:** ~48 provably-dead pure-variant-macro branches (`if (IS_SM83)` / `IS_RAB`
  / `IS_TLCS90` / …, all compile-time 0). Deletable with byte-identical verification, but mostly
  *cosmetic* (the compiler already DCEs them) — value only where a branch carries an illegal-on-S1C88
  emit (like the one above).
- **(b) RISKY / hard-to-test:** the live-but-allocator-avoided z80-isms — `ex (sp),hl` (21 sites in
  cheapMove/genCopy/genIpush/genSwap/genAssign, used for **IY-byte access**; the S1C88 idiom is
  `ex ba,iy`), residual `push de`, etc. These fire under IY allocation, which the corpus avoids, so they
  need per-site repros — blind edits risk a silent miscompile (cf. the ldir revert). `ex ba,iy`/`ex ba,ix`
  verified legal.
- **(c) STRUCTURAL:** delete the C/D/E/DE/BC symbols themselves — `PAIR_DE` (301 refs), `ASMOP_DE` (72),
  `C/D/E_IDX` (~100 each), `IYL/IYH_IDX` (~98), the combined long asmops, and the `*_IDX` ordinals
  hard-keyed into the Boost allocator in `ralloc2.cc`. This is the big-bang that was reset at reflog
  `417bed5`; do it incrementally, always-green, build + byte-identical after each step.

The compiler is correct for all *reachable* code today, so #7 is cleanup + latent-risk reduction, not a
functional gap — best done as a careful dedicated effort rather than rushed.

## Session 12 (2026-06-02) — __critical sections retargeted (SC interrupt-level masking)

**`e962677`:** `__critical` functions/blocks emitted the z80 idiom (`ld a,i; di; push af` … `pop af; jp PO;
ei`) — all illegal on the S1C88 (no `ei`/`di`/`ld a,i`; `jp PO` is z80 parity). The S1C88 masks interrupts
via the SC interrupt-priority bits (I1=bit7, I0=bit6): **level 3 (`or sc,#0xc0`) masks all maskable
IRQ1-3**.
- **Function-level** (genFunction/genEndFunction `IFFUNC_ISCRITICAL`): prologue `push sc; or sc,#0xc0`
  (save SC = level+flags, raise to 3), epilogue `pop sc`. `push sc` is 1 byte (`param_offset += 1`); `pop
  sc` doesn't touch A/HL so the old return-value/flag shuffle is gone.
- **Block-level** (`genCritical`/`genEndCritical`): the no-result form (what SDCC emits for `__critical {}`)
  is `push sc; or sc,#0xc0` / `pop sc`. The result-capturing form threads the prior SC through the itemp
  (`ld a,sc`→result; `ld sc,a`←right) — implemented for completeness (not emitted by normal usage here).

push/pop sc naturally handle nesting + the prior level (restore the exact saved SC), so no IFF dance.
Verified 0 sdas88 errors for function-level, block statement, block-with-locals, and control-flow/loop/call
inside the section; full 15-input corpus + rom-smoke + link-smoke GREEN. (`ei`/`di`/`reti`/`ld a,i`/
`jp PO` are now gone from all reachable codegen.)

## Session 11 (2026-06-02) — struct/union return-by-value implemented

**`1ddb2c3`:** `return *p` from a struct-returning function hit a FATAL `Unimplemented` in genRet — a
bigreturn has `aopRet()==NULL`, and the frontend lowers the return to a *pointer-sized* operand (the address
of the source struct), so it landed in the `size<=4 && !IS_STRUCT` branch with no destination. (The
`return localstruct` form — the struct *value* on the stack — was already handled by session 5's byte-loop
at the AOP_STK path.) Implemented the copy to the caller's hidden return buffer:
- **HL = source pointer** — loaded first (it's usually the near-ptr arg in HL, so read before HL is reused).
- **IY = hidden buffer pointer = `*(sp+off)`** via `push hl; ld hl,sp; add hl,#off; ld iy,(hl); pop hl` —
  HL is the only pair that can deref `(hl)`; saving the source over the read + computing `off` after the
  push makes it correct for any frame offset. (Note: `ld iy,(iy)` would be cleaner but the dialect formats
  it as the illegal `ld iy,0(iy)` — IY has no register-indirect-with-displacement pair load.)
- byte loop `ld a,(hl); ld 0(iy),a; inc hl; inc iy; djr nz`. Struct > 255 B stays a clean Unimplemented.

Verified end-to-end (callee copies to the hidden buffer = this fix; caller allocates it + passes the
pointer = already worked, they agree): `return *p`/global/local/deref-modify-return, 2-byte and 10-byte
structs all 0 sdas88 errors; full 14-input corpus + rom-smoke GREEN.

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
