# ▶ HANDOFF — pick up here

**This is the single resume entry point.** If the prompt is *"let's pick up where you left off,"* do the
steps under **NEXT ACTION**. Everything needed to continue is here or linked from here.

> **▶ The forward-looking work list is [`TODO.md`](TODO.md)** — the critical path to a *usable*
> toolchain (driver wiring, real crt0, target lib, device headers, packaging) plus the
> quality/coverage backlog (float diff module, `__critical`/nested-IRQ, peephole tuning).
> Suggested next step: TODO **#4 (the `s1c88` support library)** — A1+A2+A3 are done (session 26),
> so the integrated `sdcc -ms1c88 game.c` now preprocesses → compiles → assembles → finds crt0;
> the only remaining link gap is the missing `s1c88` lib (div/mul/mem/str support routines).

_Last updated: 2026-06-07 (session 26: **the usable-toolchain critical path begins — TODO
A1+A2+A3 done**. (A1) Driver wiring: `src/s1c88/main.c` `_z80AsmCmd`→`sdas88`, `_libs`→`s1c88`;
the integrated driver now invokes `sdas88` and emits a valid XL3 `.rel`. (A2) Preprocessor: the
installed `bin/sdcpp` was a wrapper around an unbuilt cpp — **`scripts/build-sdcpp.sh`** builds
`support/sdbinutils/libiberty` + `support/cpp` (the GCC-cpp fork), so `sdcc -ms1c88 foo.c`
preprocesses for real (no `--c1mode`). (A3) **Production crt0** (`device/lib/s1c88/crt0.s`): the
real Pokémon Mini cartridge header — **`"PM"` @ 0x2100**, 27 × **6-byte vector slots**
(`ld nb,#page ; jrl`; reset→`__start`, 26 maskable→`_irq_default`), **`"NINTENDO"` @ 0x21A4**
(0x21BC tail dropped, unchecked) — plus stack/EP=XP=YP=0/IRQ-mask/gsinit/`__sdcc_fptr`/`bcall
_main`. `scripts/build-runtime.sh` installs `crt0.rel` in the driver's lib dir. **The emulator
runner gained a minimal BIOS** (auto-detected via `"PM"`): synthesizes the 0x00-0xFF vector table
from the cart slots and enters via the reset vector, as hardware does. `scripts/crt0-smoke.sh`
boots a C `main()` end to end (header bytes verified, gsinit ran, `main()`=42); emu-test still 8/8,
corpus 20/20. The header magic is **`"PM"`** per the project owner (a booting homebrew uses `"MN"`;
followed the owner's direction). Remaining critical path: **#4 `s1c88` lib**, then #5 headers, #6
romgen integration, #7 packaging. — Earlier, session 25: **emulator pruned to a minimal CPU+memory+IRQ+timers
harness** (LCD/blitter/audio/RTC/TIM256/input/GPIO/EEPROM removed; control = 3 dumb bytes; RAM
+ cartridge unified into one writable `memory[]` so **far writes work**; BIOS bypassed). **New
control-flow differential module** (`control.c`, 467 values) found **bug #4 — genCast unsigned
widening clobbered a live A (FIXED, `07d17ba`)** and **bug #5 — a `char` arg alongside a stacked
`long` arg dropped under register pressure (FIXED, `4ac3b14`: genIpush now excludes
already-sent arg registers from push scratch; regression in `calls.c`)**. Differential modules:
arith 5876 + control 467 + calls 72 + memory 32 (aggregates: arrays/structs/unions/near
bitfields — its draft caught a test-soundness issue, not a bug: plain `int` bitfield signedness
is impl-defined; use explicit `signed`). See "Session 25". Earlier — session 24: **two new test layers + three reachable codegen bugs
they found**. (1) `tests/emu/cases/07_far.c` + a `--far` lane in `emu-test.sh` give far
pointers their first EXECUTION coverage (far ROM reads via EP paging; emu-test 7/7). (2) A
host-vs-emulator **DIFFERENTIAL harness** — `scripts/diff-test.sh` + `tests/diff/` — compiles the
same C twice (host `cc` reference vs sdcc88→emulator) and diffs the output; `cases/arith.c` matches
**5876 integer-arithmetic values**. Its first run found three bugs INVISIBLE to the 20-file
byte-identical corpus (which stayed 20/20 after the fixes): **`srl l`/`sra l` illegal-operand** in
genrshOne (z80 shifts any reg; S1C88 only a/b/(hl) — result-in-L shift), **SP-relative
`ld …,dd(sp)` mis-sized 1 not 3** in `s1c88instructionSize` → `labelInRange` wrongly shortened
`jp cc`→`jrs` across a stacked-arg call → assembler "Branching Range Exceeded", and
**`u16 >> var` infinite loop** — genRightShift fell back to A as the loop counter (z80-safe) but
the S1C88 body routes L/H bytes through A as scratch, clobbering it. **Run `scripts/diff-test.sh`
with corpus-check + emu-test for codegen changes.** See "Session 24". Earlier: session 23:
**EXECUTION testing exists — the minimon emulator is now
the harness** (`scripts/emu-test.sh`, vendored core in `third_party/minimon-core`, 6/6 cases
PASS), and its FIRST RUN caught **four runtime miscompiles + one linker bug** that 20/20
corpus-clean assembly never could: the genSub PAIRPTR result-in-L guard (off-by-one), the genCopy
A-in-cycle swap silently DROPPED (→ native `ex a,b` — a latent z80-upstream bug our BA-first arg
ABI hits constantly), the struct-return hidden-buffer push with both BA+HL as args (ICE → IY), the
four genRet hidden-pointer reads still assuming 2-byte z80 return frames (max mode = 3-byte
CB:PC), the peep.c `bit` operand-order notUsed() miss (deleted the A reload before `bit a,#0x01`,
breaking C-compiled `__divuint` for dividends ≥0x8000), and sdld's bank-0 bcall slot patch
corrupting record addresses across T-line splits. **Run `scripts/emu-test.sh` for every codegen
change from now on.** See "Session 23". Earlier: session 22: **the polish list is CLOSED — and a port-wide latent bug
found and fixed**. (1) **signed 8÷8 division** joins the native DIV (branchless sep-mask fixup,
not under --opt-code-size); (2) the **phantom-register names are GONE** (PAIR_BC/PAIR_DE/C/D/E_IDX
deleted, ~670 lines incl. dead genArrayInit; 3 byte-identical phases); (3) **far bit-fields
implemented** (HL+EP read-modify-write; mask/sign work at EP=0); (4) **THE BIG ONE — the call
model was minimum-mode (2-byte z80 frames) but the PM is MAXIMUM mode (3-byte CB:PC frames,
PokeMini-verified)**: call_overhead 5, caller cleanup (only RET can pop a 3-byte frame), PCALL via
the `__sdcc_fptr` cell + native `call (hhll)`, the s19 RET-dispatch/manufactured-return schemes
deleted (they built frames RET would misinterpret on silicon); (5) **banked function pointers** —
3-byte (lo,hi,bank) code pointers end-to-end (the bank byte links via XL3; `ld nb` + the CB←NB
latch dispatches; verified `00 80 02` in a linked initializer with _CODE=0x028000); (6) Phase-3
Epson register args **closed as documented divergence** (EP=0 invariant makes them hazardous).
See "Session 22"; design in abi-decision.md "The call model: MAXIMUM mode". **Corpus 20/20, all
smokes GREEN.** Remaining = peephole/cost tuning only. Earlier: session 21: **native DIV** — `genDiv`/`genMod` retargeted from support
calls to the native `DIV` (`CE D9`, unsigned `HL÷A` → L quotient, H remainder): unsigned 8÷8 is a
single DIV, unsigned 16÷8 (literal divisors in practice — C promotion widens variable u8 divisors)
is the two-DIV schoolbook chain with provably-fitting partial quotients; `_hasNativeMulFor` now
claims `'/'`/`'%'` for exactly those shapes, `HLinst_ok` treats them like `'*'`, byte-granular A/B
saves, signed/16-bit-divisor/32-bit stay support calls. Design in abi-decision.md "Native DIV";
corpus file `20_div.c`; **corpus 20/20 byte-identical 0 errors, all smokes GREEN, 7 option-mode
combos clean.** Commits `1da6979` + `9cf0371`. See "Session 21". Earlier: session 20: **task #9 —
3-byte `__far` pointers — DONE end-to-end** in 5
slices: port config + shared-glue emission (far objects = const ROM, 3-byte initializers), the
HL+EP deref idiom (genFarPointerGet/Set, the EP=0 invariant, EP-toggle walks for absolutes — incl.
a peephole-fold hazard found the hard way), 24-bit arithmetic/compares/casts (three
"pointers-are-2-bytes" middle-end miscompiles fixed: optimizeOpWidth narrowing, geniCodeSubtract
missing widening, genAddrOf dropping the page) + the **HLA return ABI** (offset HL, page A — Epson
HLP; ASMOP_HLA), ISR EP hygiene (save+zero EP, restore before rete), and the **link story**:
`exprmasks(3)` (XL4's byte-reloc collapse corrupted streams; XL3 = byte-perfect `(sym>>16)` page
relocs via the stock R_BYT3/R_HIB path), far areas located at PHYSICAL addresses, `romgen.py
--far=start-end`, rom-smoke carries a far ROM block. **Corpus 19/19 byte-identical 0 errors; all
smokes GREEN.** Remaining: optional cosmetics only — see "Session 20". Earlier: session 19: **#8 IY
argument passing DONE** (int BA,HL,IY; nptr HL,BA,IY;
IX excluded by the frame prologue) + a **pre-existing PCALL HL-argument miscompile fixed via
RET-dispatch**; instruction sizing corrected (jrl→jrs improvements); reserve-regs-iy hardened (the
old census flag was a no-op misspelling). See "Session 19". Earlier:
session 18: **task #7 CLOSED** — #7c finale done: the phantom asmops are
deleted, zero phantom-register emissions remain, two more corpus blind spots (struct-by-value args,
jump tables) found+fixed, native byte pushes throughout; corpus 18/18 + all option modes GREEN. See
"Session 18". Earlier: session 17: **#7b COMPLETE + #7c nearly done** — every option mode
(`--fomit-frame-pointer`/`--opt-code-size`/`--reserve-iy` combos) now produces 0 sdas88 errors (two
were broken); the EXSTK/omit machinery, genIpush, call-saves, IY-half access, commitPair, makeFreePairId
all retargeted to BA/IY idioms; `__sfr` = plain memory; 108 constant liveness predicates folded; the 26
surviving phantom-asmop sites are census-proven guard-dead. Remaining: the #7c symbol-deletion finale —
see "Session 17". Earlier: session 16: **#7a — the dead variant-branch sweep — is COMPLETE**: all 599
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

1. Confirm green: `./scripts/dev.sh` → builds the compiler + smoke test → `GREEN`, then
   **`scripts/emu-test.sh` → 6/6 PASS** (the EXECUTION harness: compiles, links, romgens and RUNS
   each `tests/emu/cases/*.c` on the vendored minimon emulator core — real S1C88 semantics incl.
   max-mode 3-byte frames). corpus-check proves asm is *stable*; emu-test proves it *computes the
   right values*. Add an emu case whenever you touch new codegen territory.
2. The codegen retarget is **functionally complete for the verification corpus** — every *reachable*
   z80-ism it exposes is gone: **20/20 corpus files assemble with 0 `sdas88` errors** (incl.
   `19_far.c`, the #9 far-pointer cluster, and `20_div.c`, the native-DIV cluster), and the full
   assemble→link→banked-ROM pipeline — now including **far ROM data** — is GREEN. (Session 14 proved "functionally complete" claims are
   only as strong as the corpus — keep extending `scripts/corpus/` when touching new codegen territory.)
   All the feature gaps are closed: compares / 8- and 16-bit
   ALU / shifts (sessions 1–4), the **`ldir` struct-copy cluster** (session 5), **`bcall`/`bjump` for
   inter-function calls + `.globl` for support routines** (6), the **active C/D/E+DE/BC z80-isms** (7),
   **independent port** linking alongside z80 et al. (8), **function-pointer calls** (9), **ISR
   prologue/epilogue** (10), **struct return-by-value** (11), **`__critical`** (12), the s14 corpus
   findings (`or a,l/h`, bitfield `set/res`/`rld/rrd`, `ld (iy+d),#imm`, `ldir` block copy, STL-address
   BA) (14), **`genMult` literal path + variable 8×8 `MLT` + block-scope extern `.globl` + the #10
   `jp <signed cc>` lowering** (15). See the per-session entries below.
3. **ALL numbered tasks are CLOSED** (#7 s18, #8 s19, #9 s20) **and the polish list is CLOSED too**
   (session 22): native DIV incl. signed 8÷8 (s21+s22), the phantom-register names deleted, far
   bit-fields implemented, banked function pointers end-to-end, Phase-3 Epson register args closed
   as documented divergence — **plus the session-22 max-mode call-model correction** (a port-wide
   latent bug: 2-byte z80 frames vs the PM's 3-byte CB:PC maximum-mode frames — read
   abi-decision.md "The call model: MAXIMUM mode" before touching anything call-related).
   **There are no open correctness tasks.** What remains is open-ended peephole/cost tuning, and
   one recorded hazard to watch: pointers to code-space DATA (CPOINTER) are 3 bytes but deref
   near-only — fine while const data stays in the common bank (the current convention).
   **Runtime contract:** programs provide `__sdcc_fptr:: .ds 2` in near RAM (like the
   `__div`/`__mul` support routines).
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

## Session 25 (2026-06-07) — emulator pruned to a minimal harness; control-flow diff module; a 4th bug fixed + a 5th found

**Two threads: pruning the vendored emulator core, and growing the differential suite (which found two more bugs).**

**Emulator prune** (commits `8236cfa`, `68eea6f`): the codegen tests need a CPU + memory + a
few peripherals, not a full Pokémon Mini. Removed **LCD, blitter, audio, RTC, TIM256, input,
GPIO, EEPROM**; kept CPU (`machine`/`operations`), **IRQ**, the programmable **timers** (for
exercising interrupts later), and **control** (neutered to three side-effect-free bytes — no
LCD/cart enable gating). **RAM + cartridge merged into one writable `memory[0x200000]`** (RAM
0x1000–0x1FFF, cartridge/far from 0x2100) so **far writes stick and read back** — `07_far`
gained char+int write-then-readback coverage at a scratch far address. `cpu_clock` keeps the
OSC1 (32 kHz) derivation for the timers (now computed before `Timers::clock` so OSC1-sourced
timers actually advance); the CPU only ever executed off OSC3 (the microcontroller's
run-on-OSC1 feature was never modelled — nothing to remove). **The BIOS will not boot with the
peripherals gone** — the runner forces PC to 0x2100; flagged in `cpu_reset`, the runner, and
the core README. 15 modules → 6.

**Control-flow differential module** (`1fea6fa`): `tests/diff/cases/control.c` — if/else-if,
switch (jump-table + gap + fallthrough + default), for/while/do-while, break/continue, nested
loops, short-circuit `&&`/`||` with side effects, ternary, goto. 467 values match. It found:

- **#4 (FIXED, `07d17ba`): genCast unsigned widening clobbered a live A.** `if (x<N) … (unsigned
  long)x …`: the u8→u32 cast zero-fills its stack temp with `xor a,a; ld (mem),a`, clobbering A
  while it still holds `x` (live for the compare) → the compare reads 0 and always takes the
  first branch. The signed widening path already guarded with `_push(PAIR_AF)`; the unsigned
  path didn't (it relied on genMove_o honoring `a_dead` for ZERO→memory, which it doesn't).
  Added the guard. z80 dodges it (builds the widened value in registers DEHL); the S1C88's
  smaller file forces the stack-temp + A-scratch path. 4 cast-heavy corpus files re-baselined,
  reviewed benign.
- **#5 (FIXED, `4ac3b14`): a `char` arg passed alongside a stacked `long` arg was
  dropped/clobbered under register pressure.** `f(ptr, long, char)`: arg setup runs in reverse
  — SEND(char→A), IPUSH(long v), SEND(ptr→HL), CALL — and the S1C88 stages the stacked long
  through BA, clobbering A; the char came through as v's low byte (the peephole then removed the
  dead `ld a,#n`). Root cause: a *literal/immediate* SEND creates no live range, so
  `isRegDead(A)` reported A free at the IPUSH and genIpush used BA. (An iTemp arg has a live
  range and was correctly excluded — only literal/char args broke.) z80 dodges it (pushes via
  DE/BC). Fix: genIpush now scans the preceding SENDs for this call (as genSend already does)
  and excludes their arg registers from the push-scratch candidates → it stages via a free pair
  (HL/IY). Regression: `tests/diff/cases/calls.c` (the `f(ptr,long,char)` shape under nested-loop
  pressure + broader ABI coverage). The harness's per-width `diff_e1/e2/e4` emitters (added to
  dodge #5 before it was fixed) are kept — they're a clean, compact design regardless.

**ISR EXECUTION case** (`49a5f52`): `tests/emu/cases/08_isr.c` drives a real timer-0 interrupt
end-to-end on the kept IRQ + timers — program the timer, install `&tim0_isr` at vector
`2*IRQ_TIM0 = 0x10`, set the IRQ group priority + enable, drop the CPU level (`and sc,#0x3f`
inline asm) so the IRQ is accepted, then run a fixed foreground sum while the ISR fires;
verifies the sum is unperturbed (the `__interrupt` prologue/epilogue + RETE saved/restored
regs), the ISR ran, and its body computed correctly. Negative control (omit the SC unmask)
confirms it depends on real delivery. To install the vector the table (low memory, vector N at
`2*N`) had to be writable, so the **embedded BIOS was removed** — `cpu_read`/`cpu_write` route
everything outside the `0x2000..0x20FF` register window through the unified `memory[]` (the BIOS
couldn't boot post-prune and the runner forces PC=0x2100 anyway). emu-test now **8/8**.

(Earlier this session, before the prune: the far emu case and the differential harness +
arith module — see Session 24.) diff-test arith 5876 + control 467 + calls 72 + memory 32;
corpus 20/20.

## Session 24 (2026-06-07) — far execution coverage + a differential harness; three bugs found

**Two complementary test layers were added, and the differential one found three reachable codegen
bugs the 20-file byte-identical corpus never exercised** (corpus stayed 20/20 after the fixes;
emu-test 7/7; branch/rom smokes GREEN). Commits `edb9612` (07_far), `d67424a` (the three fixes),
`6563614` (the differential harness).

**1. Far-pointer EXECUTION coverage (`edb9612`).** `tests/emu/cases/07_far.c` + a per-case `--far`
lane in `emu-test.sh`: a case using `__far` links `_FAR` at physical `0x10000` (page 1, disjoint
from bank-0 code) and romgens that range with `--far`, so the codegen's `(sym>>16)` page byte is
the address the data bus sees. The emulator resolves `[hl]` as `(ep<<16)|hl` into the cartridge, so
far ROM **reads** are faithful — but `cpu_write_cart` is a no-op and the PM has no far RAM, so the
case verifies reads / pointer arithmetic / HLA returns / far bit-fields ONLY (never
write-then-readback). The nonzero-page assertion is load-bearing: with the page dropped, `*cp` would
read BIOS at offset 0 instead of the table bytes. Verified `ctbl` lands at physical 0x10000 and reads
back through EP=1. (Obvious next: an ISR emu case — needs an IRQ-injection runner option.)

**2. Host-vs-emulator DIFFERENTIAL harness (`6563614`).** `scripts/diff-test.sh` + `tests/diff/`:
each `cases/*.c` defines `diff_run()` emitting `tag:hex\n` per computation; the SAME source is
compiled by host `cc` (reference) and by sdcc88→minimon, and the streams are `diff`'d — no
hand-maintained expected values. **Soundness** (host int=32-bit, S1C88 int=16-bit): width-exact
typedefs (stdint host / native target); every `EMIT_*` truncates to its width (agrees host/target
for `+ - * / % & | ^ ~ << >>`); operands widened explicitly for wide semantics; **same-typed
compares only** (mixed rank/signedness diverge by C promotion, not codegen); `volatile` operands (no
folding); hex text (the char-out mailbox can't carry 0x00). `cases/arith.c` covers all integer
widths/signedness × the operator set + casts with edge operands, per-iteration work in small helpers
(keeps loop bodies under the jrs range, exercises the call ABI) — **5876 values match**. The
16×16→32 widening multiply is noted out of scope (needs the compiler-internal `__mul*int2*long`
support library, which ships only as per-target hand asm with a fixed support-call ABI).

**3. The three bugs (`d67424a`)** — all reachable from ordinary C, all corpus-invisible:
- **`srl l`/`sra l` illegal operand** (genrshOne): the "shift in the destination register" arm is a
  z80-ism (z80 shifts any reg; the S1C88 shifts only `a`/`b`/`(hl)`). A right-shift result landing
  in L/H (e.g. it feeds an array index via HL) while the operand stays live emitted `srl l`. Fixed:
  the in-place arm fires only for A/B; L/H + memory shift in A (unsigned via the cheaper rlc-based
  `AccRsh`, as `shiftR1Left2Result`).
- **SP-relative `ld …,dd(sp)` mis-sized** (`s1c88instructionSize`): the 16-bit forms
  `ld {hl,ba,ix,iy},dd(sp)` / `ld dd(sp),{hl,ba}` (all 3 bytes, `CF 7x dd`) fell through to the
  catch-all `return(1)` (and `ld ix/iy,dd(sp)` to 4). Under-counting `labelInRange()` made
  `jp cc`→`jrs` shorten across a stacked-arg call, producing an out-of-range short branch the
  assembler rejects ("Branching Range Exceeded"). Fixed: detect `(sp)` operand → 3. ⚠ Same class as
  the s15/s23 lessons — *retargeted sizing/operand facts vs inherited z80 tables*.
- **`u16 >> var` infinite loop** (genRightShift count-register selection): the z80 base fell back
  to **A** as the loop counter, safe there because z80 shifts any register in place. On the S1C88
  the multi-byte shift body routes the value's L/H (and memory) bytes through A as scratch
  (`emit3_shift`), so A-as-counter is clobbered mid-body → the loop never terminates. Fixed: the
  count is now a dead byte reg disjoint from the value, **never A** (B preferred for `djr nz`, then
  H/L); over-constrained allocations are steered away via `UNIMPLEMENTED`'s 4000 dry-run cost (a good
  combo — value in BA — always leaves H/L free).

**Remaining: open-ended peephole/cost tuning, plus growing the suites** (next differential modules:
control flow, calls/ABI, memory/aggregates; next emu case: ISR via IRQ injection). No open
correctness tasks.

## Session 23 (2026-06-06) — the emulator test harness: execution is now the ground truth

**The minimon.js Pokémon Mini emulator core is vendored (`third_party/minimon-core`, ISC, from
minimon.js `c3c3c93`; the generated opcode `table.h` is checked in pinned) and wrapped as an
EXECUTION harness: `scripts/emu-test.sh` compiles each `tests/emu/cases/*.c` (host cpp →
`--c1mode`), assembles, links with `tests/emu/crt0.asm`, romgens to a flat `.min` and RUNS it on
the emulator — main()'s return value is the verdict (0 = pass).** Commits `b90e160`/`1a31d8d`
(vendor+pin), `fdae401` (linker fix), `6ca6e65` (codegen fixes), `0facbf9` (harness). 6/6 PASS.

The harness mechanics (all in `tests/emu/`):
- **runner.cc** — native host build of the core (plain g++, no deps; `make -C tests/emu` →
  `build/emu/runner`). Loads the `.min` at `cartridge[0x2100]`, bypasses the BIOS (cart enabled,
  PC forced to 0x2100), steps instruction-by-instruction. Mailbox at the top of RAM: 0x1FF8
  char-out (host polls every step → `emu_puts` streams), 0x1FFA/B exit code (BA), 0x1FFC
  done-magic before `halt`. Stray-halt/crash/timeout get distinct exit codes.
- **crt0.asm** — linked FIRST: entry at 0x2100 and the area-order contract (_HOME _CODE _GSINIT
  _GSFINAL _INITIALIZER | _DATA _INITIALIZED). EP=XP=YP=0 (the EP=0 invariant), SP=0x1FF0, zeroes
  _DATA, copies _INITIALIZER, owns the `__sdcc_fptr` cell, `bcall _main`, stores BA+magic, halts.
  **Gotcha: `_CODE` needs an explicit `-b _CODE=0x4000`** — sdas88 implicitly opens _CODE as area
  0 of every module, so it's first in link order and would land at address 0.
- **runtime** — SDCC's generic `_divuint/_divsint/_moduint/_modsint/_mulint` compiled BY OUR PORT
  per run and linked into every case: self-hosting, the runtime exercises the codegen too.

**The first run caught 5 real bugs that 20/20 corpus-clean assembly never could** (all fixed):
1. **sdld bank-0 bcall slot corruption** (`fdae401`): the R_S1C88_BANK bank-0/same-bank path NOPs
   `CE C4` at `rtval[rtp-2..rtp-1]` — but the slot can split across T lines, leaving CE C4 in the
   PREVIOUS record; rtp-2 then hits this record's ADDRESS bytes → the tail of _HOME emitted at
   linker address 0xFFxxxx (an 8MB .min). Guard `rtp-2 >= rtofst`; fallback `ld nb,#tbank` is
   always correct (RET/RETE restore NB=CB=popped — minimon-verified). rom-smoke missed it: its
   bcalls are all cross-bank. **The repo patch `s1c88_banked_branch.patch` was regenerated.**
2. **genSub PAIRPTR guard off-by-one** (emu 02): checked whether the NEXT result byte lands in
   L/H instead of the bytes ALREADY written — result-low-in-L corrupted the (hl) pointer for the
   high byte (`ld l,a; inc hl; sbc a,(hl)`). setupPair AOP_PAIRPTR adjusts the cached pointer
   blindly, so the guard is the only defense; it now checks written bytes (dry-run cost steers).
3. **genCopy A-in-cycle permutation** (emu 04): the accumulator-cache for register permutations
   is unsound when A is in the cycle (`ld a,b` destroys old A before `ld b,a` reads it) — latent
   in the z80 ORIGINAL, but our BA-first argument ABI makes the {B,A}-temp→BA-arg swap routine,
   and genCopy emitted NOTHING (bytes crossed at the call). Fixed with the native **`ex a,b`**;
   other cycles through A are UNIMPLEMENTED-flagged. Bonus: shorter code (a push-a/push-sc dance
   in 20_div collapsed into one `ex a,b`).
4. **struct-return call frame** (emu 04, two bugs): the hidden-buffer push wassert'd when both BA
   and HL carry args (no scratch pair) — IY is now preferred for direct calls too, not only PCALL
   (IY is caller-clobbered, s19); and the four genRet hidden-pointer reads still assumed a 2-byte
   z80 return frame — max mode is 3-byte CB:PC, the callee read the pointer one byte short
   (`(_G.omitFramePtr ? 0 : 3)` now, was `: 2`).
5. **peep.c `bit` operand order** (emu 03): notUsed() scanned after the comma (z80 `bit #n,reg`),
   but S1C88 is `bit reg,#mask` — peephole 7 deleted the A reload before `bit a,#0x01`, breaking
   C-compiled `__divuint` for any dividend with bit 15 set (50000/7 wrong, 1234/10 fine).
   ⚠ Same class as the s15 displacement bug: **retargeted operand ORder vs inherited z80
   scanners** — if another retargeted mnemonic misbehaves under peepholes, check peep.c first.

Cases: 01 smoke/data-init, 02 ALU/shifts/compares, 03 native MLT/DIV + the divuint chain,
04 calls/recursion/struct-return/IY args (real 3-byte frames!), 05 pointers/arrays/bitfields/
block copies, 06 banked function pointers through `__sdcc_fptr`. corpus-check after the fixes:
14/20 byte-identical, 6 reviewed diffs (the intended fixes), 0 sdas88 errors — baseline
re-snapshot. All smokes GREEN. **Far-pointer and ISR emu cases are the obvious next additions**
(need a `--far` link lane in emu-test.sh and an IRQ-poking runner option respectively).

## Session 22 (2026-06-06) — the polish list closed; the MAX-MODE call-model bug found and fixed

**Every remaining polish item is done, and chasing the banked-function-pointer design surfaced a
port-wide latent correctness bug that got fixed on the spot.** Commits `0450dfe` (signed div),
`2ebe74e` (phantom names), `e45e57e` (far bit-fields), `89efb3b` (max mode), `29c4af8` (banked
fptrs). The essentials per item:

- **Signed 8÷8 division** (`0450dfe`): hasNativeMulFor claims signed char ÷ signed char (and the
  hook claims positive literal divisors, which never fire — the middle end widens `sc/10` to int).
  Branchless: `|x| = (x^m) - m` with m = SEP's sign mask; unsigned DIV on magnitudes; the result
  mask (q: `m(divd)^m(divr)`, r: `m(divd)`) re-applied the same way; wide results sep-extend.
  NOT claimed under `--opt-code-size` (~26 B inline vs a 6 B bcall) — the gate is verified.
- **Phantom names deleted** (`2ebe74e`, the s18 finale, 3 byte-identical phases): PAIR_BC/PAIR_DE
  out of the PAIR_ID enum and `_pairs[]`; C/D/E_IDX out of ralloc.h/`s1c88_regs[]` (IYL/IYH
  renumber to 4/5); ralloc2.cc REG_C/D/E folded under the 0..3-only invariant (DEinst_ok is
  `return true`); the de_dead/bc_free plumbing dropped from genMove/genMove_o/genCopy/adjustStack/
  restoreRegs (~140 call sites); dead genArrayInit + RLE machinery deleted (ARRAYINIT never
  generated). En route: genMove_o's stack-pop trick and genPointerSet's lit-word store no longer
  pick phantom pairs (the latter now saves a live HL); the reserve-regs-iy no-third-pointer case
  is a loud UNIMPLEMENTED.
- **Far bit-fields** (`e45e57e`): genFarUnpackBits/genFarPackBits — raw bytes through HL+EP,
  every mask/shift/sign op at EP=0, stores/loads of the VALUE at EP=0 too (multi-byte values ride
  the stack across the pointer staging; `pop ba` is SP-paged = near-safe under EP). Any alignment,
  signedness, result/value aop; blen>16 keeps a loud trap. `19_far.c` gained the fb_* cluster.
- **⚠ THE MAX-MODE CALL MODEL** (`89efb3b` — read abi-decision.md "The call model: MAXIMUM mode"):
  the S1C88 max mode pushes CB with every call; RET pops 3 bytes. The PM IS max mode
  (PokeMini: CALLS pushes PC.B.I+PCH+PCL; min mode can't bank-switch a 2MB ROM). The linker's
  bcall/bjump always assumed it; codegen assumed z80 2-byte frames — every stacked arg off by one
  on silicon, and the s9/s19 PCALL schemes built frames RET would misinterpret. Nothing caught it
  because no ROM executes in-repo (the s15 lesson again). Fix: call_overhead 4→5; CALLER cleanup
  by default (only RET can consume the frame; __z88dk_callee keeps one universal 3-byte frame-hop
  epilogue); PCALL = stage offset into the `__sdcc_fptr` near-RAM cell + native `call (hhll)`;
  tails keep `jp hl`. 14/20 corpus files re-baselined (+1 offsets, epilogues, dispatches —
  hand-verified incl. varargs `&n+1` at ix+7).
- **Banked function pointers** (`29c4af8`): funcptr_size = 3 — (lo, hi, bank); code symbols link
  as (bank<<16)|logic so `&f`'s `(sym>>16)` byte IS the bank (the #9 XL3 relocs, zero linker
  work); printIvalFuncPtr's 3-byte arm gated for s1c88 (patch regenerated from pristine).
  Dispatch: `ld nb, <bank>` then `call (__sdcc_fptr)` / `jp hl` — the CB←NB latch switches, the
  max-mode frame restores. **NB-window discipline: ≤1 instruction between ld nb and the branch**
  (the Minx interrupt blackout covers exactly the linker's own `ld nb; nop; carl` window).
  Peepholes 135/136 (call→jp tails) banned from `call (__sdcc_fptr)` — caught emitting a relative
  jump to the cell. **End-to-end:** linked with _CODE=0x028000, the fptr initializer bytes read
  `00 80 02` (offset + linker-resolved bank).
- **Phase-3 Epson register args: closed, won't do** (abi-decision.md updated): stack passing is
  correct on both sides, no Epson interop exists, and far-ptr args in EP-paired registers would
  collide with the EP=0 invariant at every entry.

**Remaining: open-ended peephole/cost tuning only.** Watch item: CPOINTER (code-space DATA)
pointers are 3 bytes (since #9) but deref near-only — fine while const data stays in the common
bank. Runtime contract: `__sdcc_fptr:: .ds 2` in near RAM, and indirect calls are not reentrant
against an ISR that itself makes an indirect call mid-dispatch.

## Session 21 (2026-06-06) — native DIV: unsigned 8÷8 and 16÷8 division/modulus

**The "native DIV" polish item is DONE.** `genDiv`/`genMod` no longer wassert — unsigned divisions
with an 8-bit divisor run on the hardware `DIV` (`CE D9`, unsigned `HL ÷ A` → L quotient,
H remainder, 2 B/13 cyc, MODEL1/3 — present on the Pokémon Mini core). Commits `1da6979` (8÷8) +
`9cf0371` (16÷8), each corpus-gated. Design + the exact claim/codegen contract in
**abi-decision.md "Native DIV"**. The essentials:

- **The claim** (`_hasNativeMulFor`, main.c — SDCCopt consults it for `'/'`/`'%'` too; what it
  declines keeps the `__div*`/`__mod*` support calls, so the claim must exactly match codegen):
  unsigned ≤16-bit dividend ÷ (unsigned char | literal 1..255). Signed, 16-bit divisors, 32-bit:
  not claimed. A zero divisor now raises the **hardware zero-division exception** (C UB; was
  support-call garbage). **C promotion caveat:** `u16 / u8var` widens the divisor to uint, so the
  variable-divisor 16÷8 only claims post-narrowing — in practice 16-bit claims are *literal*
  divisors (`x/10`, `x%10`: binary-to-decimal dropped from a `__divuint` bcall each to ~10 bytes
  inline).
- **8÷8** (`genDivMod`, gen.c): `dividend→L, 0→H, divisor→A; div`; quotient L / remainder H;
  staging is clobber-ordered (divisor-first when its home is L/H; a stack bounce `push a/l/h …
  pop l` for the circular A↔L and requiresHL-divisor shapes); live non-operand A saved
  byte-granular (the genMultOneChar scheme).
- **16÷8**: the schoolbook base-256 chain — `ld b,l; ld l,h; ld h,#0; div` (qhi, running
  remainder r), `[push l;] ld l,b; div [; pop h]` (qlo; HL = full quotient after the pop). Both
  partial quotients provably fit (r < divisor ≤ 255 ⇒ `(r:lo)/d < 256`) — V never set; DIV
  preserves A between steps; the qhi push/pop is skipped for `'%'`/1-byte quotients; a live B
  (the chain's lo-byte home) gets a byte-granular save with `_G.stack.pushed` keeping SP-relative
  operand math right across it (verified: live char in B across `x/10`).
- **Allocator/peephole:** `HLinst_ok` (ralloc2.cc) treats `'/'`/`'%'` like `'*'` (DIV clobbers HL;
  live non-operand values stay out — safe to add unconditionally: unclaimed divisions are already
  CALL iCodes at allocation time); both ops moved to the exact-cost dry-run list. peep.c: `div`
  sizes 2, reads no flags, surely writes Z/N/C/V (the mlt pattern).
- **Verification:** `scripts/corpus/20_div.c` (single/literal/memory/pointer/condition/chained/
  divmod-pair/live-A+B/16÷8 incl. dec3 + the unclaimed support-call shapes) — corpus **20/20
  byte-identical 0 sdas88 errors**; rom/link/branch smokes GREEN; 3 div stress files × 7
  option-mode combos 0 errors; q10/r10/keepb/dec3/divmod_uu/bigframe-EXSTK hand-traced (no
  execution test — the corpus checks legality, not runtime semantics).

**Remaining polish after this:** Epson-faithful Phase-3 register args, far bit-fields, far
function pointers / banked indirect calls, the inert PAIR_BC/DE names, peephole/cost tuning —
and optionally a signed-division claim (negate-fixup around DIV) if profile data ever wants it.

## Session 20 (2026-06-05) — task #9 (3-byte __far pointers) DONE end-to-end

**The last open task is CLOSED.** `__far` works from C source to a flashed-ROM byte image. Commits
`3a6ed47..90f1b8f` (5 always-green slices, each corpus-gated). Design recorded in
**abi-decision.md "Task #9"** — read it before touching far codegen. The essentials:

- **Model:** a far pointer = a **24-bit linear physical data address** in 3 little-endian bytes
  (byte 2 IS the EP page — S1C88 data paging is linear, so the stock SDCC `(sym>>16)` emission is
  natively correct). `__far` → S_XDATA → FPOINTER, fptr_size=3; unqualified/gptr stays 2-byte near,
  untagged. Far objects are **const ROM data** (area `_FAR`, emitted as a romable static segment —
  the PM has no far RAM); a far object never straddles a 64K page (Epson `_far` semantics).
- **The EP=0 invariant:** all near codegen assumes EP=0. Far accesses stage the pointer into
  **HLA** (a new 3-byte reg asmop: offset→HL, page→A), `ld ep,a`, walk `(hl)`, and ALWAYS restore
  `ld ep,#0` (A untouched). While EP≠0: (ix+d)/(iy+d)/[sp+dd] stay near (own page regs) — but
  absolutes are repaged, AND the peephole can fold an iy-literal store into an absolute INSIDE the
  window (a real miscompile found in testing) — so absolute results/values use a page-in-B
  **EP-toggle** walk with DIRECT absolute emission (correct even after folding). Pathological
  shapes (reg results >2B, EXSTK operands, staging collisions) are UNIMPLEMENTED traps whose
  dry-run cost steers the allocator away.
- **ABI:** far args = stack (Phase-3 Epson IYP/IXP/HLP = optional cosmetic); far return = **HLA**
  (Epson HLP) — caller chaining is free (`bcall f; ld ep,a; ld a,(hl)`). ISRs save+zero EP after
  the GP saves and restore before `rete` (EP is NOT in the hardware save set).
- **Middle-end fixes (all in `register_s1c88_port.patch`, gated inert for other ports):** glue
  emitted far objects nowhere (xdata written only for mcs51-like) and runtime-GSINIT'd const far
  data; printIvalPtr/printIvalCharPtr truncated 3-byte initializers (`use_dw_for_init`) and emitted
  GP *tag* bytes for 2-byte near pointers + literal far pages; genconstprop clamped every pointer's
  value range to GPTRSIZE=2 → "known-zero" page bytes constant-folded away (`xor a,a` for the page);
  optimizeOpWidth narrowed a pointer-`+`'s signed addend to uint16 whenever NEARPTRSIZE==2 (p+=n
  broke for negative n); geniCodeSubtract lacked geniCodeAdd's #3807 widening (p-=n lost the sign
  byte). gen.c: genAddrOf was 2-byte-only (page dropped); aopGet IMMD byte 2 was `!zero` (now
  `!bankimmeds` = `#((sym) >> 16)`); peep.c sizes the CE-page special-register loads.
- **The link story:** `(sym>>16)` resolves through the STOCK reloc path (asexpr R_HIB → outrxb's
  s1c88 gate → R_BYTE|R_BYT3|R_HIB → lkrloc3 adb_24_hi, ds390-proven) — but only after
  **`exprmasks(4)`→`exprmasks(3)`** in s1c88mch.c (under XL4 the linker's byte-slot collapse left a
  stray byte that CORRUPTED the instruction stream; XL3 output is byte-identical to hand-assembled
  reference). Far areas are located at their **physical** address (data reads are EP-linear, not
  CB-banked — the `(bank<<16)|logic` code convention cannot serve them); `romgen.py
  --far=start-end` declares those ranges (indistinguishable by value); keep far-data banks disjoint
  from code banks. branch-smoke's listing parser updated for XL3's 6-digit addresses (encodings
  unchanged).
- **Verification:** `scripts/corpus/19_far.c` (objects/initializers/deref/displacement/indexed
  tables/EP-toggle walks/literals/casts/24-bit arith/compares/returns/ISR) — corpus 19/19
  byte-identical 0 sdas88 errors; clean × 4 option modes; rom-smoke now builds a banked ROM **with
  a far ROM block** and byte-verifies both the physical placement and the linked page byte.

**Remaining (all optional, none functional):** Epson-faithful far-ptr register args (IYP/IXP/HLP)
+ YP/XP char args + IYIX longs (Phase 3 cosmetics); far bit-fields (loud UNIMPLEMENTED); far
function pointers / banked indirect calls (out of #9's scope — needs a CB-switch dispatch story);
the inert PAIR_BC/PAIR_DE name removal (s18). **There are no open correctness tasks.**

## Session 19 (2026-06-05) — peep sizing fixed; #8 (IY argument passing) DONE + a PCALL miscompile fixed

- **`e943e38` instruction sizing:** `s1c88instructionSize` learned the real S1C88 16-bit ALU sizes
  (reg,reg = 2; add/sub/cp #imm = 3; adc/sbc #imm = 4; `add sp,#imm` = 4) and `bcall`/`bjump` = 6 —
  the stale z80 blocks said `add hl,x` = 1 and everything else fell to assume-999, forcing long
  branches around every call. 4 corpus files improved (pure `jrl`→`jrs`, one `jp GE`→`jrs GE`).
- **`fe6c202` #8 ABI Phase 2:** int args = **BA, HL, IY**; near-ptr = **HL, BA, IY** — IY is the
  *overflow* register (divergence: Epson is IY-first for pointers, but our allocator can't hold
  operands in IY, so that taxes every pointer call). **IX permanently excluded** (the prologue
  `push ix; ld ix,sp` claims it before an argument could be read); IYIX longs deferred (no IX byte
  ordinals); `--reserve-regs-iy` removes IY from the ABI. genSend/genReceive guard the IY transport
  (iy_dead=false once loaded / while pending); genMovePairPair is now one orthogonal `ld dst, src`.
- **A REACHABLE PRE-EXISTING MISCOMPILE found & fixed** (since s9): `jp hl` PCALL dispatch cannot
  coexist with an HL argument — the fptr load destroyed it and the peephole then deleted the "dead"
  arg load (`fp2 (1, 2)` silently dropped the 2!). Now **RET-dispatch** when the callee takes an HL
  arg: `add sp,#-slots; push hl; ld hl,(fptr); ld 2(sp),hl; [ld hl,#ret; ld 4(sp),hl;] pop hl; ret`
  — the ret pops the target, HL arrives intact; tagged `;pcall` so the peephole liveness scan
  treats exactly these rets as calls. The jp-hl fast path remains for HL-argless callees, with a
  parm-aware return-address pair (IY → BA → the stack-slot fallback).
- **⚠ the earlier option census tested a MISSPELLED no-op flag** (`--reserve-iy`; the real option is
  `--reserve-regs-iy`). The corrected census exposed two real reserve-mode gaps, both fixed:
  genAssign's 4-byte `ld a,(de)` walk (deleted — genMove copies via absolute addressing) and the
  callee-cleanup return-address hop (generalized to any poststackadjust via BA staging). KNOWN
  LIMIT: reserve-regs-iy + >127-byte frame + multi-byte pointer reads = loud UNIMPLEMENTED (no
  third pointer exists).
- Corpus re-baselined twice (sizing: 4 files; ABI: 9 files — callers drop arg pushes, callees gain
  IY receive-spills; hand-verified incl. callfp2/3 dispatch and 12_arrays' 3-arg callee).
  **18/18 0 errors; rom/link/branch GREEN; 0 silent issues across 7 option combos × 29 files.**
- **Remaining: #9 far pointers** (the last open task) + optional cosmetics (inert PAIR_BC/DE names,
  IYIX longs, Epson YP/XP chars).

## Session 18 (2026-06-05) — #7c COMPLETE: zero phantom-register emissions; task #7 CLOSED

**The register-model sweep is DONE.** The phantom z80 registers (C/D/E bytes, BC/DE pairs, the
combined DEHL/HLDE/HLBC/DEBC long asmops) no longer exist as *emittable things*: the asmop structs,
their ASMOP_* handles and init code are deleted; every site that could name them is retargeted or
gone. Commits `7d309f2`..`456b259`:

- **Two more REACHABLE blind spots found and fixed** (the s14 lesson, counts 4 and 5):
  **struct-by-value argument pushes** (`genIpushValueAtAddress` emitted `ld c,(hl)/push bc/push af/
  inc sp`; now a BA pair walk + native 1-byte `push a`, live HL stashed in a dead IY) and
  **jump tables** (`genJumpTab` picked phantom DE/BC for the table-offset add; now `add hl,ba` ×2 +
  `ld a,(hl); inc hl; ld h,(hl); ld l,a; jp hl`). **`scripts/corpus/18_jumptab.c`** added; corpus
  is now 18 files.
- **Native byte pushes everywhere** (`push a/b/l/h`, CF B0–B3): the entire z80 `push pair; inc sp`
  odd-byte machinery is gone — incl. the raw `push af` arms (NOT encodable: only the byte forms and
  `push sc` exist). The all-byte-regs-live fallback stages via a reserved pair slot +
  `ld 2 (sp), hl` + `inc sp`.
- **Active latent bugs killed en route:** genLeftShift's `countreg = C` fallback (an always-true arm
  after constant-folding — a `--reserve-iy` miscompile-in-waiting); genCmp's `ccf` sign-flip arm
  (ccf doesn't exist); genPlus's uninitialized-B `add hl, bc` byte-literal trick (now A over
  `add hl, ba` with a dead-A gate); genMove_o's 16-bit-load-into-BC arm; genCall's bigreturn
  IY-path stack imbalance; commitPair's missing PAIR_BA arm (getDeadPairId returns BA — the old
  switch wassert(0)'d); makeFreePairId returned phantom BC whenever B was dead.
- **genEndFunction's callee-cleanup "hard way"** return-address hop stages through a saved BA
  (poststackadjust==1) and is a loud UNIMPLEMENTED otherwise; **genRet's register-bigreturn** writes
  the hidden buffer through IY (`ld iy,(hl)` + `ld (iy),a`); **gencjneshort's** PAIRPTR fallback
  compares through B inside a saved BA; **restoreRegs/cheapMove/regMove/genCopy/genSwap/genOr/
  shiftL2/genPointerSet** all lost their C/D/E/DE/BC arms (retargeted to BA/HL/IY idioms or deleted
  where operand-impossible); **aopRet/aopArg map the legacy z80 conventions** (sdcccall(0), smallc,
  z88dk_fastcall) **to the native S1C88 registers**.
- **All C/D/E register reads constant-folded globally** — sound now that no asmop contains those
  ordinals: `regs[C/D/E_IDX]` ≡ −1, `aopInReg (…, C/D/E/DE/BC_IDX)` ≡ false, rMask bits ≡ 0.
- **What deliberately REMAINS (inert, documented):** the `PAIR_BC`/`PAIR_DE` enum members and
  `C/D/E_IDX` ordinals as *names only* — the `_pairs[]` "bc"/"de" rows, the ralloc reg-table rows,
  `ralloc2.cc`'s `REG_C/D/E` defines, and graceful phantom arms in `isPairDead`/`isPairInUse`
  (variable pair-ids still flow through them; PAIR_DE ≡ dead, PAIR_BC ≡ B). Deleting them is a pure
  renumber of internal ordinals (the allocator only assigns 0..3) with zero behavioral effect —
  optional cosmetics, not debt. **IYL/IYH_IDX stay by design** (they are ASMOP_IY's byte-wise
  representation, load-bearing for the IY-pair support).
- ⚠ Process note: a sloppy text-anchor splice deleted 3,400 lines mid-session (`UNIMPLEMENTED;`
  matched an earlier site). Recovered by replaying the (deterministic) edit scripts from the last
  checkpoint — **commit checkpoints after every verified slice.**

**Verification: corpus 18/18 byte-identical 0 sdas88 errors; rom/link/branch smoke GREEN; 0 issues
across 7 option-mode combos × 29 test files** (struct args/returns, jump tables, long compares,
forced omit-frame-pointer, reserve-iy, opt-code-size/speed).

**Remaining open tasks: #8 (IX/IY argument passing), #9 (3-byte far pointers).** Also noted:
`s1c88instructionSize()` can't size `add sp,#imm` / `bcall`/`bjump` (assumes 999 → pessimizes
branch shortening around calls) — a small peep.c improvement.

## Session 17 (2026-06-05) — #7b COMPLETE; #7c was mostly done (the symbol deletion finale remained)

**#7b is CLOSED and #7c is down to guard-dead residue + the symbol deletion proper.** Commits
`c3ef689`..`ff8941a`:

- **The method that drove it: canary-instrumented reachability census.** Wrap every suspect emit
  statement in `{ fprintf (stderr, "CANARY %d %s", __LINE__, dry?"dry":"REAL"); stmt }`, compile the
  corpus + stress files under 8 option combos, classify. First census: 54 sites — only
  fetchPairLong's `ex de,hl` STL trick fired REAL (under `--fomit-frame-pointer`), the cheapMove
  IY-half paths fired dry-only, everything else silent. Final census: the 26 surviving phantom-asmop
  sites get ZERO hits, not even dry.
- **`--fomit-frame-pointer` was a broken option mode** (and `omit_frame_ptr()` is a HEURISTIC that can
  fire in default mode!): the EXSTK machinery still used DE. Fixed: genPlus EXSTK extrapair → BA;
  fetchPairLong STL → `ex ba, hl` pivot; **setupToPreserveCarry**'s three-distinct-carry-destroying-
  operands case (z80: right→HL, result→DE-ptr, left via cached IY) **collapses left into result**
  (carry-free pre-copy, then the chain runs in place — left/result share the IY pointer). Fixing that
  exposed an upstream latent bug: **aopGet's AOP_PAIRPTR-via-IX/IY both moved the pair AND printed the
  displacement** (double-applied offset) — IX/IY PAIRPTR access is now displacement-form off a fixed
  base, never moving the pair (aopGet + aopPut symmetric; HL keeps the move-protocol).
- **`--opt-code-size` was broken too**: `!enters` mapped to `call ___sdcc_enter_ix` — no such runtime
  helper; now expands inline (mappings.i ×3 dialects).
- **genIpush word push**: free-pair candidates HL → BA → IY (BA halves work in the literal-caching
  loop via PAIR_BA; IY loads any source incl. literals via genMove_o). The phantom bc/de picks — which
  pushed GARBAGE (`push bc` never loaded) for literal high words under forced omit — are gone. The
  all-pairs-live fallback replaces `ex (sp), hl` with `push hl; push hl; ld 2 (sp), hl; pop hl`.
- **genCall saves: B gets a byte-granular 1-byte slot** (`push b`/`pop b`, `_G.stack.pushed += 1` keeps
  all SP-relative math right); restoreRegs' bc arm pops the byte (`inc sp` when the result occupies B),
  the DE arms are gone everywhere (genCall, restoreRegs, genFunction callee-saves).
- **genCast sign-extend**: the illegal `sbc hl, hl` fast path deleted (byte-wise `sbc a, a` serves it).
- **cheapMove IY-half access** (the dry-considered family): the z80 `push iy / ex (sp), hl / ex de,hl`
  machinery replaced by two idioms — the **fully self-restoring BA pivot** `ex ba, iy; ld A/B↔L/H;
  ex ba, iy` (restores BA, the other IY half, everything — usable when the partner is L/H) and **HL
  staging** `push hl; ld hl, iy; …; [ld iy, hl;] pop hl` otherwise (memory partners keep IY free for
  their own addressing; IY re-loaded from the HL copy in case they re-pointed it).
- **makeFreePairId → BA/HL** (it returned phantom PAIR_BC whenever B was dead — latent `ld c,…`);
  **commitPair got its missing PAIR_BA arm** (getDeadPairId returns BA → the old switch wassert(0)'d),
  BC/DE arms deleted; setupPairFromSP wassert(HL||IY).
- **`__sfr` is plain memory now**: AOP_SFR (z80 in/out codegen) is never created — S1C88 hardware regs
  are memory-mapped, so `p = 1` emits `ld a,#1; ld (#_p),a` (was illegal `out (_p),a`). Every AOP_SFR
  consumer arm deleted (~120 lines incl. the genCmp ASMOP_C cluster).
- **The constant z80 liveness predicates folded** (108 sites): `isPairDead(PAIR_DE)` ≡ true,
  `isRegDead(C/D/E/DE_IDX)` ≡ true, `isPairDead(PAIR_BC)` → `isRegDead(B_IDX)`, rMask C/D/E bits ≡ 0 —
  sound because the allocator never assigns the phantom bytes (num_regs == 4) and post-#7b no codegen
  keeps values there. + 34 dead if-arms deleted by the statement folder with operand-scoped
  `aopInReg(op, n, C/D/E_IDX)` ≡ false atoms (genPlus's byte-in-C/E add helpers among them).
- 07_calls re-baselined once (cost-shift; hand-verified instruction by instruction).

**Remaining #7c (the finale, well-scoped):** ~26 emit sites referencing ASMOP_C/D/E/DE/BC remain, ALL
guard-dead per the census (zero dry hits) — they sit behind operand-shape guards that can't occur.
Deleting them + the symbols themselves: `PAIR_DE/PAIR_BC` enum members (~70/35 refs, mostly in those
dead branches + `_pairs[]`), `asmop_c/d/e/de/bc` + combined `dehl/hlde/hlbc/debc` defs, the
`genMove/genMove_o` `de_dead` parameter (pure plumbing now — every caller passes `true`), C/D/E_IDX
from ralloc.h's enum + the `s1c88_regs[]` table + the `[IYH_IDX+1]` parm-mask arrays + ralloc2.cc's
`REG_C/D/E` defines (renumbering IYL/IYH is safe — the allocator only assigns 0..3). **Decision
recorded: IYL/IYH_IDX stay** — they are the byte-wise representation of ASMOP_IY (load-bearing for
the IY-pair support), unlike the truly phantom C/D/E.

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
