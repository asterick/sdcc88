# sdcc88 — backlog: bugs, coverage, code-size, cleanup

The toolchain is **complete and in maintenance.** `sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx
game.min` builds a bootable Pokémon Mini ROM, and every gate is green: corpus 20/20 byte-identical, emu-test
16/16, diff-test 12, run-tests 50/50 (TAP), plus the driver / crt0 / rom / branch / insn-size smokes and the
`examples/hello` build.

This file is the **forward backlog** for the existing source base — bug-chasing, coverage, code-size, and
z80-lineage cleanup. Shipped work is not re-documented here; it lives in git history + commit messages, the
per-topic memories, and `abi-decision.md`. Resume state + load-bearing watch-outs are in
[`HANDOFF.md`](HANDOFF.md); the design/ABI is [`abi-decision.md`](abi-decision.md); the end-user guide is
[`building-roms.md`](building-roms.md).

Legend: **S/M/L** = rough effort.

---

## Known bugs

**One open — #11-longshift-iy (silent miscompile, narrow trigger).** The **32-bit variable LEFT shift**
(`u32 << n`) miscompiles when BOTH the value and the count are **memory** operands (global/array), e.g.
`out = arr[i] << gcount;`. Minimal repro:
```c
unsigned long arr[2], out;  unsigned char cc;
void f(unsigned char i){ out = arr[i] << cc; }   /* loop runs a garbage count -> wrong result */
```
Cause: the 4-byte value fills A/B/L/H, so `genLeftShift` (gen.c) puts the loop counter in **IY**
(`count_iy`). IY isn't byte-addressable, so the count must be zero-extended through a pair scratch
(`ld l,c; ld h,#0; ld iy,hl`). That works when the count is loaded first with HL free (the param case —
`u32 f(u32 a,u8 c){return a<<c;}` is correct). But a memory value is materialised into HL+A+B *first*, so
HL is busy and `genMove(ASMOP_IY,count)` degenerates to reusing IY's **stale high byte** (or a pure
no-op) — the counter is garbage. **Only this op**: right shifts, 16/64-bit shifts, division, and any
register/param-sourced count all work. **Fix is deep** (genLeftShift IY path / genMove-to-IY zero-extend
without a free pair) and risks the shift codegen that otherwise passes everything — deferred. **Workaround
in code:** route the shift through a helper so the count is a parameter (what `arith.c`/`longshift.c` do;
the call launders the memory count into a clean operand). `tests/diff/cases/longshift.c` exercises the
WORKING variable-shift paths exhaustively (every count 0..width-1 for 16/32/64-bit + rotates + division
edges) and notes the trap so the helpers aren't "simplified" back to inline.

Otherwise the differential suite is clean across integer, pointer, struct/union, function-pointer,
long-long, and **float** code. The prior open bug — **#8 float subtraction** (`10.0-4.0` → `0x40C00182`) —
is **FIXED**: a register/move-ordering bug in `genUminusFloat` (the `-a1` sign-flip inside `__fssub` loaded
the top byte into A before spilling the low word, dropping byte 0 on the HLBA layout). Fix: copy the low
bytes before the sign flip; regression in the subtraction / opposite-sign block of `tests/diff/cases/float.c`.
*(Note: a full cancellation `a-a` yields softfloat `-0.0` where hardware gives `+0.0` — a library zero-sign
convention, not a bug; harmless since `-0.0 == +0.0`.)*

---

## Correctness coverage — keep mining the differential suite (#11)

The highest-value ongoing work. Each new `tests/diff/cases/*.c` (+ a `tests/emu/cases/*.c` for ABI-shaped
behaviour) is run through `corpus-check` + `emu-test` + `diff-test`; the suite has caught several real
**silent** miscompiles that byte-identical assembly never could (struct-arg register-drop, the `cp ba,hl`
pointer-compare peephole gap, the long-long/struct return-ABI off-by-one). Covered: arith, bitfields, calls,
control, longlong, memory, ptrarith, switch, structargs, fnptr2, unions, float, libc — plus the emu ABI cases.
**Still untested (pick any; new territory is also fair game):**

- ✅ **#11-libc — DONE** (`tests/diff/cases/libc.c`, 82 values): the s1c88.lib string/memory subset vs host
  libc — memmove (both overlap directions), memcmp/memchr, strcpy/strncpy (zero-pad + truncate), strcat/
  strncat, strcmp/strncmp, strchr/strrchr, strstr, strlen — plus memcpy/memset via `__builtin_*` (their only
  target form; no lib member). Sign-normalized `*cmp` and pointer-offset results keep the impl-defined
  corners sound. No miscompiles surfaced. Surfaced + fixed a real toolchain gap: **`strlen` had no target
  implementation** (upstream SDCC 4.5.0 ships no `_strlen.c`, and it's not a builtin) — added a repo-owned
  `device/lib/s1c88/_strlen.c` (build-runtime.sh now prefers repo libc sources over the fetched SDCC copies).
- ✅ **#11-longshift — DONE** (`tests/diff/cases/longshift.c`): exhaustive variable-count shifts (every
  count 0..width-1 for 16/32/64-bit, vs arith.c's 6 samples) + rotates + long-division edges. Found one
  silent miscompile — the 32-bit variable-left-shift IY-counter bug above (**#11-longshift-iy**, deferred;
  the test routes shifts through helpers to cover the working paths).

Workflow: add the case, run the three gates, fix what surfaces, add an emu/diff regression for any bug.

---

## Code size (#12, #14) — measurable via `scripts/size-check.sh` (corpus baseline, currently 8424 B)

- **[S] #12-flag-reuse byte-combine peephole.** The post-16-bit-add zero test
  `add hl,X ; ld a,h ; ld b,l ; or a,b ; jr Z` (the `(a+b)?` / function-result idiom) is now provably
  redundant — the flag model knows `add hl,X` sets Z — but capturing it needs a peephole that sees past the
  `push b`/`pop b` register-preservation noise. ~5 insns/site when it fires.
- **[S] #12-far-idiom.** Tighten the `__far` EP=0 deref sequences and (now that #14b relaxation landed) the
  `bcall`/`bjump` slot idioms.
- **✅ int×int→long widening — DONE (−72 B corpus, 8424→8352).** `has_mulint2long` is now ON in `main.c`,
  so a 16×16→32 product lowers to the dedicated `__mulsint2slong`/`__muluint2ulong` instead of widening both
  operands to 32 bits and calling `__mullong`. The routines are repo-owned C built on the native 8×8 `MLT`
  (`device/lib/s1c88/_mul{u,s}int2*long.c`); the unsigned one is signed-fixup reused by the signed one. Two
  gotchas handled: (1) the middle-end symbol is `___mul*int2*long` (3 underscores), which **no C function
  name can emit** — so the math is plain-named C and an in-module sdas `=` alias exports the mangled symbol;
  (2) with the flag on, a partial product assigned to a **32-bit** result would re-trigger the widening
  substitution and recurse — so the four 8×8 products stay in 16-bit (`unsigned int`) result context and the
  widen happens via adds/shifts. `13_swap` −72 B; only int×int→long users pull the routines (no other corpus
  program regressed). Validated: diff/arith 6132 values, emu 16/16, corpus re-blessed.
- **[S] #12 residual cleanup.** Prune the dead z80-mnemonic tokens (`rlca`/`scf`/`daa`/…) from multi-token
  `same()` lists in `peeph.def` (byte-identical). Refining `cost2`'s cycle numbers to exact S1C88 counts is
  **low value** for this target — the allocator cost is bytes-dominated (cycles discounted 64–512×; see the
  cost-model memory) — so do it only as part of a speed-focused pass.
- **✅ #14c — linker cross-module branch relaxation — DONE + default-on (PR #6, 2026-06-09).** Same-bank
  cross-module `bcall`/`bjump` slots drop their `ld nb` bank switch at link time (corpus **−150 B**); opt out
  `SDLD_NO_RELAX=1`. Stage (i) + split-slot reclaim shipped; (ii) iterate-to-fixpoint and (iii) carl→cars
  closed as N/A / not-worth-it (a same-bank disp always fits `carl`, so stage (i) already drops every in-range
  slot; carl→cars is ~1 B for the riskiest emit surgery). `size-check.sh`'s `#14c relax` section tracks the
  reclaim vs `scripts/corpus/relax.baseline`. Design + full history: `banked-branch.md` + git (`7949b4a`,
  `2d7d7c8`, `99375b6`; squashed into `b37bc0d`).

- **✅ #14d — vector-slot NOP reorder — DONE + default-on (2026-06-09).** An absolute-area banked branch
  (the hardware-pinned IRQ/reset vector table) can't be byte-dropped, so a same-/common-bank target left its
  `ld nb` NOP'd at the slot HEAD (`nop nop nop ; bjump`) — 3 dead NOPs the CPU ran before every dispatch.
  The linker now reorders the fixed slot to `bjump ; nop nop nop` so an unconditional jump skips them; **cycles
  only, no byte change** (neutral for `bcall`, never a pessimization). Gated `rlxVecReorderOn()`, opt out
  `SDLD_NO_VECREORDER=1`. `vec-reorder-smoke.sh` locks the placement + the +3 disp. Design: `banked-branch.md`
  §10 "#14d".

- **[M] #14e — fix stale symbol tables under #14c relaxation (debug-info bug, code is correct).** #14c
  reflows the emitted ROM down by `rlxDelta()` but never updates the linker's symbol/area model, so
  `.map`/`.sym`/`.noi`/`.lst` report **pre-relax** addresses — off by the cumulative reclaim for any symbol
  past a drop in its bank (e.g. a user `_irq_v1` reads `0x21EA` in the map but is emitted at `0x21E4`). The
  generated **code is correct** (vector `bjump` disps resolve to where handlers are actually emitted —
  verified by ROM-byte inspection); only the debug metadata lies, which reads as "jumps to strange locations"
  when disps are cross-referenced against the map. Fix: delta-adjust symbol addresses at symbol-table emit
  (`lkmain.c`/`lksym`/`lknoice` — apply the same `rlxDelta()`). Add a test linking a real trampoline-dispatched
  handler that asserts both the disp and the map address resolve to the handler's emitted location (the
  `0x2102` `bjump`-dispatch path currently has **zero** execution coverage — emu-test installs vectors
  directly in low RAM). Full write-up: `banked-branch.md` §10 "KNOWN ISSUE".

---

## Library coverage (#17) — expand the bundled libc

The toolchain ships only a string subset + compiler helpers historically. Expanding it
surfaced a **foundational header bug** (now fixed): SDCC's `<stdarg.h>`/`<sdcc-lib.h>`
gate `va_list`/`_REENTRANT` on `defined(__SDCC_z80) || …` and `s1c88` wasn't listed, so
they fell to the mcs51 `__data` default and **`<stdarg.h>`/`<stdio.h>` were broken for
all s1c88 user code**. `build.sh` now adds `__SDCC_s1c88` to the z80-family branch of
those two headers (NOT `string.h` — its z80 branch asserts `__preserves_regs(iyl,iyh)`,
which our C impls don't honor). `build-runtime.sh` prefers repo libc sources and passes
`-D__SDCC_s1c88`.

- **✅ Tier 1 — DONE** (`build-runtime.sh`; `tests/diff/cases/libc2.c`, 1813 values):
  `atoi`/`atol`, `abs`/`labs`, full `ctype` (`isalnum`…`isxdigit`, `tolower`/`toupper`),
  `strspn`/`strcspn`/`strpbrk`/`strtok`, `memccpy`, `__itoa`/`__ltoa`/`__uitoa`/`__ultoa`,
  `rand`/`srand` (rand not diffable; itoa/ltoa build+run, not diffed — non-standard).
- **✅ qsort / bsearch — DONE** (now in the lib; the callback ICE below is fixed).
  Validated on the emulator (sort + search through a `__reentrant` comparator) and via
  the `17_fnptr_arg` regression.
- **✅ #17-printf — DONE.** `printf`/`vprintf`/`sprintf`/`vsprintf`/`puts` are in the lib
  (formatter core `_print_format` from `printf_large`, built `USE_FLOATS=0` — no `%f`, so a
  `%d`/`%s`/`%x` printf doesn't pull the float routines). `putchar()` contract: the default
  (repo-owned `device/lib/s1c88/putchar.c`) stores to the minimon debug console
  (`DEBUG_OUT`, RAM `0x1FF8`); it's a standalone module, so a user `putchar()` (e.g. LCD via
  PRC) overrides it through ordinary archive selection. This also fixed a latent gap:
  the **standard headers weren't installed** — `build-runtime.sh` now copies SDCC's
  `device/include` (stdlib/stdio/string/ctype + the `asm/` trees) to the driver's include
  dir, so user code can `#include` them. Validated: `tests/diff/cases/sprintf.c` (13 values
  vs host), `examples/hello` now prints via `printf`, putchar-override confirmed.
- **[M] #17-setjmp — deferred.** Upstream `_setjmp.c` is mcs51-only (`#include <8051.h>`);
  an s1c88 `setjmp`/`longjmp` must be written as port asm (save/restore SP, return PC,
  callee-saved IX/IY). Deferred — rarely needed on this target; pick up on demand.
- **[L] #17-malloc — deferred by request.** Needs a heap area + `_sdcc_heap` wired into
  crt0/linker; a real design choice on a 4 KB-RAM device.

**✅ #17-callback-ICE — FIXED.** Diagnosis: passing a function NAME/address as an
argument (not the callback call itself) aborted codegen with `FATAL "Unbalanced stack"`.
A function's address is a 2-byte immediate (its PC); a function POINTER is a 3-byte
banked value (PC + bank).  `genIpush` pushed only the 2 PC bytes for a function
immediate, while the caller cleaned up 3 → `_G.stack.pushed` ended at −1.  (A fptr
*variable* already pushed 3 bytes; only the immediate dropped its bank byte.)  Fix
(`gen.c` `genIpush`): emit the full 3-byte banked fptr (PC + zero bank, matching the
`f = c` store).  Fast path when a pair + byte register are free; under full register
pressure, reserve the 3-byte slot and fill it through HL saved/restored via the stack
(single-byte SP-relative stores are illegal, so the bank is written by an overlapping
word store).  Unblocked qsort, bsearch, printf, sprintf, vprintf, and any higher-order
call. Corpus byte-identical (no existing code passes a function immediate as an arg);
regression `tests/emu/cases/17_fnptr_arg.c` (fast + reserve-fill paths).

---

## Codegen-boundary lift (#16) — future research pass

The ~66 `UNIMPLEMENTED` sites are **loud traps, never silent miscompiles** (a `cost(4000)` dry-run penalty
steers the allocator away; only a forced real-emit aborts). The boundary categories are cataloged in
`abi-decision.md` "Known codegen boundaries" — no-spare-pointer under `--reserve-regs-iy`, multi-byte-ALU
register pressure, a value spanning A+B that spills into L/H, a permutation cycle through A that isn't the
`A↔B` swap, a >255-byte struct return, the HL-restore-vs-return-in-HL conflict, and the struct-arg
two-parked-pairs push. **The lift:** construct a triggering C snippet per site, classify
reachable-vs-cost-avoided, fix the cheap reachable ones, delete the genuinely-impossible guards. The cost
steering makes triggers hard to hand-write — which is itself evidence they rarely fire. None is a
correctness risk today.

---

## Cleanup — z80-artifact scrub remainder (#20)

The port was cloned from SDCC's multi-variant z80 backend. The variant *predicate machinery*
(`IS_Z80`/`IS_RAB`/…) is already constant-folded away, and the bulk of the scrub is done — port-private
identifiers renamed to `s1c88*`, the variant comments reworded, and the `cost2` 7-variant timing signature
collapsed to `cost2(bytes, cycles)`. **Remaining:**

- **(D) Dead-variant toggles + machinery — mostly done; two remainders.**
  - ✅ `HAS_IYL_INST` hardcoded `0` (was tied to `--allow-undoc-inst`, a footgun that would switch on the
    eZ80 byte-addressable IX/IY instructions that don't exist on the S1C88). The branches now constant-fold
    away, matching the `IS_Z80`-constant pattern; `IYL_IDX`/`IYH_IDX` remain as ASMOP_IY's byte ordinals.
  - ✅ Removed the dead `nmosZ80` / `--nmos-z80` option (declared, never read) and the dead
    `#pragma portmode z80/z180` + `--portmode=` + `port_mode`/`port_back` fields (only set, never read).
  - ✅ Deleted the commented-out `z80-elf-ld`/`z80-elf-as` gas command-template blocks (already dead; the
    live commands are `sdldz80`/`sdas88`).
  - ✅ **Multi-dialect asm machinery removed — DONE (asxxxx-only collapse, −270 lines).** The TODO's
    "entangled with `ASM_TYPE`" worry didn't hold up: `_G.asmType` was only ever *assigned* ASXXXX (or ISAS/GAS
    inside the `--asm=z80asm`/`--asm=gas` branches), and RGBDS was never set at all — so removing those two
    `--asm` branches left **asxxxx as the only reachable dialect**, making the whole enum + every other branch
    dead by construction. Removed: the `_z80asm`/`_z80asm_z80`/`_gas_z80` mapping tables + `_s1c88_z80asm_z80`/
    `_s1c88_gas_z80` structs (`mappings.i`), and the `ASM_TYPE` enum, `_G.asmType`, the `--asm=` option, and the
    per-dialect switches in `_s1c88_init`/`do_pragma(P_BANK)`/`-bo`/`-ba` (`main.c`) — each collapsed to its
    one reachable asxxxx string. Core never referenced the `_s1c88_*` dialect symbols. Validated **corpus
    byte-identical 20/20** + run-tests 52/52 (the segment-naming branches aren't corpus-covered, but the kept
    string is verbatim the prior asxxxx-reachable value).
  - ⏸ **Remaining (deferred):** the peephole **flag-token model** (`pf`/`sf`/`hf`/`nf`/`vf`/`lf` — z80 flag
    names). This is NOT a clean rename: the S1C88 has Z/C/V/N (no S/P/H, and N is negative not add-subtract),
    so mapping the z80 flag set onto the S1C88 is a real flag-semantics task (it touches the same analysis as
    the #12-flag-reuse work), not cosmetic. Defer until/unless it's worth a careful pass.
  - ✅ **`jp → jr → jrs` chain collapsed — DONE (byte-identical).** The `jr` mnemonic was a pure z80-lineage
    intermediary the S1C88 doesn't have: rules 162/163 (`jp→jr`) emitted it, then `s1c88-j1`/`j2` mapped every
    `jr`→`jrs`. Now 162/163 emit `jrs` directly (same `labelInRange(%5)` guard, which gates the jrs 8-bit
    range), rule 164's dead-jump elimination matches `jrs`, j1/j2 are deleted, and the 6 direct `jr nc/NZ/z`
    emits in `gen.c` emit `jrs` (the `195-1/2` always-true-check guards retargeted `jr`→`jrs` to track them).
    Out-of-range `jp`→`jrl` (j3/j4) and `call`→`carl` (j5/j6) unchanged. One spacing subtlety: the old `j2`
    template normalized to `jrs cc, %5` (comma-space), so the direct emits use the same spacing to stay
    byte-identical. corpus 20/20 byte-identical + run-tests 52/52.

  (Done in always-green slices — each byte-identical, run-tests 50/50, smokes green.)

- **(F) MUST NOT touch — shared core / external contract.** `TARGET_Z80_LIKE`, `TARGET_IS_Z80`, `IS_Z80`,
  `ASM_TYPE_Z80ASM` (shared SDCC core — the port DEPENDS on being z80-like, see `CLAUDE.md`) and `sdldz80`
  (the ASxxxx linker-binary / build-script contract). The `@file … derived from the z80 port` provenance
  headers stay as factual lineage.
