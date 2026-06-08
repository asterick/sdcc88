# sdcc88 TODO — toward a usable toolchain

Status snapshot (2026-06-08): **THE CRITICAL PATH (Section A) IS DONE — the toolchain is usable.**
`scripts/setup-sdk.sh` builds everything in one command; `sdcc -ms1c88 game.c -o game.ihx && romgen
game.ihx game.min` produces a bootable Pokémon Mini ROM, with the production `crt0` (real `"PM"`/
`"NINTENDO"` header), the auto-linked `s1c88.lib`, the `<pm.h>` device header, and a C `romgen` (no
Python). `examples/hello/` is a copy-me project; `docs/s1c88/building-roms.md` is the how-to. All gates
green (corpus 20/20, emu-test 16/16, diff-test 11, driver/crt0/rom/branch smokes, example). **Remaining =
Section B (quality/coverage) and Section C (documented limitations).**

Legend: **S/M/L** = rough effort. Items are roughly dependency-ordered within each section.

---

## A. Critical path to a usable toolchain — ✅ DONE

`sdcc -ms1c88 game.c` → assemble → link (banked) → `.min` works end to end. Delivered:

1. **Driver tool wiring** — `main.c` `_z80AsmCmd`→`sdas88`, `_libs`→`s1c88`; integrated driver emits valid XL3 `.rel`.
2. **Preprocessor** — `scripts/build-sdcpp.sh` builds the real SDCC cpp (libiberty + `support/cpp`); `sdcc -ms1c88 foo.c` preprocesses for real (no `--c1mode`).
3. **Production crt0** — `device/lib/s1c88/crt0.s`: the real PM cartridge header (`"PM"` @ 0x2100, 27 × 6-byte vector slots, `"NINTENDO"` @ 0x21A4) + C runtime (EP=0, gsinit, `__sdcc_fptr`, `bcall _main`). It trusts the **BIOS reset-state contract** (all CPU regs 0 except BA=0xFFFF / NB=CB=0x01; SP parked; MMIO left in BIOS reset config — interrupts already masked), so it re-inits NO MMIO and only pins EP=0 defensively. When `main` returns it hands off via the **software-interrupt shutdown vector `int (0x48)`** (the BIOS installs it) — no test/exit code in the runtime. The emulator runner loads an embedded **test BIOS** (`tests/emu/bios.s`) that establishes that reset state in real S1C88 code, installs the 0x0048 shutdown routine, and enters the cart; the runner reads the exit code off `BA` on halt. There is ONE crt0 (the old `tests/emu/crt0.asm` is retired; emu/diff cases boot as real PM carts).
4. **Target C runtime library** — `s1c88.lib` (ASxxxx text-index; the 10 codegen-emitted div/mod/mul routines + a mem*/str* libc subset, compiled through our own port) + the 26 per-vector default `rete` modules. Built/installed by `scripts/build-runtime.sh`.
5. **Device headers** — `device/include/s1c88/pm.h`: full PM MMIO map in Epson/SDK names + `VEC_*` vector numbers, cross-checked vs minimon-core.
6. **romgen integration** — `tools/romgen.c` (C, no Python; `romgen.py` removed), byte-identical on the banked + `--far` paths; built by `scripts/build-romgen.sh`.
7. **Packaging + example + docs** — `scripts/setup-sdk.sh` (one-command SDK), `examples/hello/` (copy-me, `make` → `.min`, boots), `docs/s1c88/building-roms.md` (end-user guide).

---

## B. Quality & coverage

8.  **[M] Float / softfloat — ⏸ DEPRIORITIZED (float is low-value for this target).** Known bug, parked.
    `tests/diff/cases/float.c` (bit-exact, exactly-representable operands) covers same-sign add, multiply,
    divide, compares, and all int↔float casts — all CORRECT. **The open bug: float SUBTRACTION (and
    opposite-sign addition) corrupts the low mantissa bits** — `10.0-4.0` → `0x40C00182` not `0x40C00000`.
    `__fssub` is `-((-a)+b)`, routing through the `_fsadd` *different-sign* path; the algorithm + all
    isolated 32-bit ops compile correctly (reductions passed), so it's a register-pressure / spill bug in
    the full `_fsadd` compile, not a library issue. Float arithmetic is rare on the Pokémon Mini, so this
    is parked behind the integer/pointer correctness and code-size work; if revisited, re-add the subtract
    cases to `float.c` (the regression test). (Note: `has_mulint2long` is disabled in main.c so int×int→long
    uses `__mullong` — the port lacks the asm-only `__mul*int2*long` widening routines, which `_fsmul`
    needs; see #15.)
9.  **[S] volatile / MMIO coverage — ✅ DONE.** `tests/emu/cases/09_volatile.c` — volatile loads are not
    hoisted (spin re-loads every iteration; volatile RAM round-trip ordering OK).
10. **[S/M] `__critical` execution coverage — ✅ DONE; nested-IRQ — ✅ DONE.** `tests/emu/cases/10_critical.c`
    proves SC-level masking (`or sc,#0xc0`) works. **Nested IRQ** (a higher-priority IRQ preempting a
    lower-priority ISR) is now covered by `tests/emu/cases/12_nested_irq.c`: instead of two fiddly
    phase-aligned timers it uses **keypad edge IRQs** — K1x (priority group 5) and K0x (group 6) get
    different priorities, and the host drives each press on demand via the input mailbox
    (`emu_set_keys`/`update_inputs`), so the low-priority handler can fire a higher-priority one *while it
    runs* and the event log proves the {low-entry, high-entry, high-exit, low-exit} nesting order. (This
    required restoring the vendored core's **input module**, pruned earlier; see its README. Note the PM
    sticky-IRQ model: a keypad IRQ's `active` latch stays set until the ISR acks it via 0x2028/0x2029, or
    it refires — the test acks both.)
11. **[ongoing] Keep mining with the differential suite.** It has found multiple real codegen bugs (latest:
    the long-long/struct return-ABI off-by-one, #18); more modules will find more. Stays open. **Pointable
    targets** — each is one new `tests/diff/cases/*.c` (+ `tests/emu/cases/*.c` for ABI-shaped ones); add,
    run corpus-check + emu-test + diff-test, fix what it surfaces:
    - **#11-bitfields** — ✅ done (`tests/diff/cases/bitfields.c`, 264 values): unsigned/signed fields of
      assorted widths, a field straddling a byte boundary, RMW that must preserve neighbours + a plain
      member, compound-assign/`++`/`^=` on a field, and single-bit adjacency masking — all CORRECT, no
      codegen bug. **Surfaced a soundness trap (not a bug):** the signedness of a *bare* `int x : N`
      bit-field is implementation-defined (C11 6.7.2/5) — **sdcc treats it as UNSIGNED, gcc as signed** — so
      a bare-int signed field diverges host-vs-target as IDB, not a miscompile. The test declares every
      field with explicit `signed`/`unsigned` (both compilers honour that identically); recorded in the
      case header + harness convention. Signed-field sign-extension codegen (`bit`/`rlc`/`sbc`) verified
      correct for explicit `signed` fields.
    - **#11-structargs** — ✅ done (`tests/diff/cases/structargs.c`, 96 values): struct-by-value args of
      sizes 1–8 + nested/array structs, by-value copy-in semantics, whole-struct assignment (ldir copy),
      struct-arg + struct-return together, and structs mixed with scalar args in every position. **Found +
      FIXED a real SILENT miscompile:** when a struct-by-value arg precedes a register arg, e.g.
      `f(struct, int)` or `f(u8, struct, int)`, the trailing register arg was **dropped entirely** — the
      struct push (`genPointerPush`) walks the struct through HL and pushes via A/B, clobbering the
      already-`send`-ed register arg, and a literal SEND "creates no live range" so `isRegDead` reported the
      register free and it was never stashed. Fix: `genPointerPush` now scans preceding SENDs (as `genIpush`
      already did) and stashes whichever pair (HL or BA) holds a sent arg into the dead IY across the push,
      restoring it before the call. The rare two-parked-pairs form `f(struct, char, int)` (needs to stash
      BOTH HL and BA, only one IY slot) now traps loudly (UNIMPLEMENTED) instead of miscompiling — cataloged
      as a boundary in `abi-decision.md`. Corpus stayed byte-identical (the path isn't in the corpus);
      diff/emu caught it.
    - **#11-ptrarith** — ✅ done (`tests/diff/cases/ptrarith.c`, 264 values): indexing across widths,
      `p[-1]`, pointer differences incl. a `/3` struct stride, equality, multi-dim, struct fields, pointer
      walk, `__far` index + diff — all CORRECT. **Surfaced one bug, deferred:**
      - **#11-ptrcmp-bug** *(open, SILENT miscompile, narrow)* — a relational compare of TWO freshly
        address-of'd elements with **runtime** indices, e.g. `&a[i] < &a[j]`, is wrong: genCmp's native
        `cp pair,pair` path materializes the left address but drops the right one (HL is left holding the
        scaled index, not the 2nd address — a register-allocation/liveness bug, NOT caught by the
        `UNIMPLEMENTED` cost steering). **Narrow:** pointer subtraction, equality, and the common `p < end`
        loop are all correct; only this inline double-address-of relational form breaks. Repro +
        working-pattern guard: `tests/emu/cases/16_ptrcmp.c`. Fix is in genCmp's two-computed-pair operand
        materialization; add the excluded `lt`/`ge` cases back to `ptrarith.c` + the `lt(1,3)` assertion to
        `16_ptrcmp.c` once fixed.
    - **#11-switch** — ✅ done (`tests/diff/cases/switch.c`, 620 values): dense-from-zero (real jump table —
      `jp hl` + `.dw` table, swept across in-range/boundary/out-of-range→default), offset-dense, sparse
      (if-chain), signed selector with negative cases, wide 16-bit selector, fall-through / grouped labels,
      and a no-default switch (out-of-range leaves the value untouched) — all CORRECT, no codegen bug. The
      genJumpTab path (HL = &table + 2×selector, `jp hl`) executed correctly on the emulator across the full
      0..130 sweep. (Smaller dense blocks fall below the middle-end's table-density threshold and lower to
      if-chains; both paths are now covered.)
    - **#11-fnptr2** — function pointers with varied signatures (wide/struct returns, many-arg calls) —
      extends `06_fnptr`/`08_isr` into the call-ABI corners.
    - **#11-unions** — unions / type-punning / overlapping member access (endianness-exact).
    - **#11-libc** — `mem*`/`str*` differential (memcpy/memmove/memset/strcmp/strlen…) run through the lib.
    - **#11-longshift** — 32-bit shifts/rotates by a *variable* count + long division edge values (beyond
      `arith.c`'s fixed-count shifts).
12. **[L, open-ended, ongoing] Peephole / cost tuning.** Codegen is correct-first, not yet size/speed-tuned.
    Stays open. **Pointable targets:**
    - **#12-sizeharness** — ✅ DONE. `scripts/size-check.sh`: compiles+assembles every `scripts/corpus/*.c`
      and reports each program's ROM size (sum of non-RAM `.rel` areas) + a per-program/total **delta vs a
      committed baseline** (`scripts/corpus/sizes.baseline`, 8460 B / 20 programs). Report-only (never
      gates); `snapshot` re-blesses. Makes every peephole/cost (#12) and relaxation (#14) win visible and
      monotone — and since the baseline is tracked, size changes show up in git diffs. *Use it: run before
      a change, make the change, run again, read the delta column.*
    - **#12-peep-audit** — audit `peeph.def` for rules inherited from z80 that are wrong or suboptimal on the
      S1C88 (z80-cost-based shortenings, dead-variant guards, mnemonic assumptions). Remove/retarget.
    - **#12-redundant-moves** — eliminate redundant `ld`/pair-move/load-after-store sequences the current
      rules miss (e.g. `ld a,X ; ld X,a`, reload of a just-stored value, dead pair shuffles).
    - **#12-flag-reuse** — drop redundant compare-to-zero / `or a,a` when a preceding op already set Z/N
      (the S1C88 sets Z/C/V/N broadly — more reuse than the z80 model assumes).
    - **#12-cost-accuracy** — replace the inherited z80 cycle numbers in `cost2(...)` with real S1C88 counts
      so the allocator's cost-driven decisions match the target (overlaps the #20-A `cost2` collapse).
    - **#12-far-idiom** — tighten the `__far` EP=0 deref sequences and the `bcall`/`bjump` slots (ties into
      #14 once relaxation lands).
13. **[M] Conditional `bjump`/`bcall` via invert-and-skip trampolines — ✅ DONE** (commit `1bbe90c`).
    The long forms (`carl`/`jrl`) and the linker's `bjump`/`bcall` only have the basic conditions
    `c/nc/z/nz`; the signed/flag conditions (`lt/ge/gt/le/v/nv/p/m/f0..nf3`) exist **only** as short
    relative (`jrs`/`cars`, ±127). So `sdas88` lowers a signed-conditional `bjump`/`bcall` to an
    **invert-and-skip trampoline**: `jrs <inverted-cc>, +7 ; ld nb,#<bank> ; carl|jrl target` (9 bytes) —
    the inverted short condition hops over the unconditional 6-byte banked branch when the original
    condition is false. The inversion table (`invcce[]`) covers all 16 CNDE conditions; the inner banked
    branch reuses the same `R_S1C88_BANK`+`R_PCR` link resolution as the unconditional form, so the linker
    still writes/elides `ld nb` and the disp. Coverage: `insn-size-check.sh` (9-byte size), `11_bankcc.c`
    (execution, taken + skipped), and `rom-smoke.sh` (the cross-bank conditional worst case —
    `bjump lt,_b2fn` → `jrs ge,+7 ; ld nb,#2 ; jrl`). Now #14 has a uniform "every conditional has a
    reachable long form".
14. **[L] Branch relaxation — shrink `bjump`/`bcall`.** *(Prerequisite #13 ✅.)* Today `bjump`/`bcall` are
    the always-long bank-switching form (`ld nb,#bank ; carl/jrl`, 6 B; 9 B for a signed conditional). The
    linker picks the *bank*, not the *size* — same-bank slots are NOP-padded, never shrunk. The win:
    same-bank, in-range calls drop the `ld nb` and use `cars`/`jrs` (2 B) or `carl`/`jrl` (3 B), and signed
    conditionals use a plain short `jrs <cc>` instead of the #13 trampoline. Most calls are intra-common-bank,
    so this is a large code-size win. **Broken into digestible, value-first steps** (full design +
    grounding in `banked-branch.md` "Relaxation plan"):

    - **#14a [S] — opportunity analysis (zero-risk). — ✅ DONE.** `scripts/relax-analysis.sh` compiles each
      fully-linkable program, assembles with `-l` and links with `-u`, and reads every `bcall`/`bjump`
      slot's FINAL resolved bytes from the relocated listing (`.rst`) — authoritative, no binary-scan
      heuristics (and complete, since `s1c88.lib` carries **zero** slots; only crt0 + user code do). For
      each slot it recovers same-bank-vs-cross-bank, the branch form, and the displacement, then computes
      the minimal legal form and bytes saved. **Result (examples/hello + `scripts/relax/{fixmath,sprite}.c`):
      user-code call sites shrink ~53% — 270 → 127 B, 143 B saved across 45 slots, every one same-bank
      (intra-common-bank, exactly the #14b target); crt0's 27 reset/IRQ vector slots are hardware-fixed and
      correctly excluded.** Two findings fell out: (1) **all sampled programs are single common-bank**, so
      the cross-bank win (branch-form only, `ld nb` stays — smaller, #14c territory) isn't exercised here;
      (2) **20 bank-0 slots still carry `ld nb,#0`** — the linker NOPs the `ld nb` for *some* bank-0 targets
      but not others (an existing inconsistency; relaxation removes the field entirely). **Feasibility gate
      (the #14b precondition):** sdas runs a FIXED **3-pass** sequencer (`asxxsrc/asmain.c`
      `for(pass=0;pass<3)`), not iterate-to-fixpoint. The STM8 + F8 backends prove this 3-pass +
      per-target `setbit`/`getbit` bit-table + `fuzz` scheme **converges** for monotonic (start-long,
      shrink-only) relaxation — no `asmain.c` change needed. **Catch for #14b:** STM8/F8 `ls_mode` bails on
      ALL relocatable operands (`e_base.e_ap != 0` → forced long); same-module relaxation must extend it to
      the **same-area** relocatable case (displacement `= e_addr − dot.s_addr`, known each pass) and add the
      ~30-line bit table to `s1c88mch.c`. Run-it: `scripts/relax-analysis.sh [prog.c …]` (report-only).
    - **#14b [M] — assembler same-module relaxation (the big practical win, NO linker reflow).** Most calls
      are intra-module (same `_CODE` area). When the target is in the current area, the displacement is known
      each pass, so the assembler can emit the minimal form directly, letting ASxxxx's existing `fuzz`
      multi-pass loop converge the sizes. Cross-area/external targets keep the fixed 6/9-byte linker slot
      (unchanged). Sub-steps: (i) unconditional same-area (`carl`/`jrl`, drop `ld nb`); (ii) `cars`/`jrs`
      short form when in ±range; (iii) basic-cc; (iv) signed-cc (plain short `jrs <cc>` vs the #13
      trampoline). Mind the branch-displacement convention (one byte earlier than z80) at each form.
    - **#14c [L] — linker cross-module relaxation (the hard reflow, deferrable).** For cross-area/cross-module
      calls, add the relaxation pass sdld lacks: after area placement, iteratively shrink in-range same-bank
      slots and reflow subsequent addresses/symbols/relocs to a fixpoint, then re-emit. Itself staged: (i)
      single-pass conservative shrink; (ii) iterate to fixpoint; (iii) conditional-trampoline shrink. Can be
      deferred if #14b captures enough — many programs are single-module or common-bank-heavy.

    Gate every step on `branch-smoke.sh` (byte-lock the forms), emu/diff (correctness), and re-baseline the
    corpus; add a size-regression check so shrinks are visible and monotone.
15. **[S] `__mul*int2*long` widening differential coverage — ✅ DONE.** `tests/diff/cases/arith.c` now has a
    `widemul` helper covering the 16×16→32 widening multiply (`(u32)u16 * (u32)u16`, signed, and the
    single-cast/promote forms) — host-vs-emulator clean. The earlier "intentionally not tested" note was
    stale: with `has_mulint2long` off (main.c) the middle end does NOT emit a `__mul*int2*long` widening
    call — it widens to 32-bit and calls `__mullong` (verified), which the harness already links. So the
    widen-then-32×32 codegen is now covered. **Deferred optimization (low priority):** writing the
    hand-written `__muluint2ulong`/`__mulsint2slong` asm + enabling `has_mulint2long` would give a smaller/
    faster int×int→long (and was the `_fsmul` unblock for #8) — but float is parked, so this is a code-size
    nicety, not correctness; the widemul cases will exercise that call path automatically if it's enabled.
20. **[L] Z80-artifact scrub — B+C ✅ DONE; A/D/F deferred.** The port was cloned from SDCC's `z80` and
    carried z80/eZ80/Rabbit/SM83/Z80N/R800/TLCS90 mnemonics, symbols, comments, and variable names.
    **Done (scope categories B + C):** all port-private identifiers renamed to `s1c88*` (peep.c's 9
    flag/jump helpers, `genZ80iCode`/`dryZ80Code`/`z80_init_reg_asmop`, main.c's PORT wiring
    `_z80_init`/`_z80AsmCmd`/`_z80LinkCmd`/`_z80_options`/`_z80_builtins`/`_z80_genAssemblerStart`/
    `_libs_z80`, `Z80_OPTS`→`S1C88_OPTS`, `Z80_FLOAT`→`S1C88_FLOAT`, the include guards,
    `Z80_MAX_REGS`); and **every rephraseable comment** across peep.c/gen.c/main.c/headers/ralloc/support
    reworded to describe only the S1C88 (z80 + other-variant trivia removed). Done in always-green slices;
    39/39 throughout, corpus byte-identical.
    **Still deferred:** **(A)** the `cost2(...)` 7-variant timing params (gen.c) — collapse to
    `cost2(bytes, cycles)` across 491 call sites; **(D)** dead toggles + machinery — the `nmosZ80` /
    `--nmos-z80` option, `z80n_de` and its folded branch, the `#pragma portmode z80/z180` handling, and the
    asm-dialect tables (`mappings.i` `_z80asm`/`_gas_z80`, main.c's `{z80*}` link-command-template variables
    + the `z80-elf-ld/as` gas-path tool names) — these are coordinated/maybe-dead and want per-unit
    verification; **(D, sub-unit)** the peephole **flag-token model** (`pf`/`sf`/`hf`/`nf`/`vf`/`lf` — z80
    flag names) and its documenting comments (peep.c) — rename as one unit; **(F, MUST NOT touch)**
    `TARGET_Z80_LIKE`/`TARGET_IS_Z80`/`ASM_TYPE_Z80ASM` (shared SDCC core — the port depends on being
    Z80-like, see `CLAUDE.md`) and `sdldz80` (the ASxxxx linker-binary/build-script contract). Provenance
    `@file ... derived from the z80 port` header lines are kept as factual lineage.

---

## C. Known limitations — fix or formally document

16. **`UNIMPLEMENTED` traps — ✅ DOCUMENTED; lift is a future target.** The ~66 `UNIMPLEMENTED` sites are
    **loud traps, never silent miscompiles** (`wassertl(regalloc_dry_run,…) + cost(4000)` steers the
    allocator away; only a forced real-emit aborts). The boundary categories are now cataloged in
    `abi-decision.md` ("Known codegen boundaries"): no-spare-pointer under `--reserve-regs-iy`, register
    pressure in multi-byte ALU (genEor/genPlus/genAnd/…), a value spanning A+B that spills into L/H, a
    permutation cycle through A that isn't the `A<->B` swap, giant (>255-byte) struct return, and the
    HL-restore-vs-return-in-HL conflict. **Future lift:** construct a triggering C snippet per site,
    classify reachable-vs-cost-avoided, fix the cheap reachable ones, delete the impossible guards — a real
    research pass (the cost steering makes triggers hard to hand-write, which is itself evidence they
    rarely fire).
17. **CPOINTER (code-space `const` data pointers) — ✅ DONE (documented + guarded).** Investigation found
    the original premise wrong: plain `const` pointers are **2-byte near** (not 3-byte), and the
    `aop->code` flag is vestigial. Plain `const` data lives in the common bank (near deref, correct because
    it's physical `< 0x8000`); **far const data already works via `__far const`** (3-byte EP-paged deref —
    verified at runtime by `tests/emu/cases/13_farconst.c`). Documented the convention in `abi-decision.md`
    + `building-roms.md` and fixed the inaccurate HANDOFF note. The silent-miscompile hazard (near-pointed
    const overflowing the common bank) is now a **loud `romgen` error** on any non-banked content past
    logic `0x7FFF`. (The literal "lift" — page-aware plain const pointers — was rejected: it would regress
    every const pointer to 3-byte/slower to duplicate what `__far const` already does.)
18. **float / long long correctness** — float subtraction is the open #8 bug (deprioritized). **long long
    is now VERIFIED** (`tests/diff/cases/longlong.c`): binops/shifts/casts/return-ABI host-vs-emulator clean.
    Mining it found + fixed a real miscompile — struct/long-long **return-by-value** dropped the 3-byte
    max-mode return frame in the hidden-pointer offset for leaf (frame-ptr-omitted) functions (genRet); the
    byte-identical corpus never caught it (baseline encoded the bug), only execution did. Regression tests:
    `14_llret`, `15_structret`.
19. **Structured test runner — ✅ DONE.** `scripts/run-tests.sh` builds the compiler once, runs every
    suite (corpus / emu / diff / toolchain smokes) **in parallel**, and emits one **TAP version 13**
    stream — one test point per case, with per-assertion `#` diagnostics (emu `CHECK` failures), a `1..N`
    plan, and a summary; exits non-zero on any failure. The case-suites gained an opt-in `TAP=1` mode
    (clean `ok`/`not ok` body on stdout, build noise to stderr); their default human output is unchanged.
    49 points green.

---

## Suggested sequencing

Section A (the critical path) is **done**. Float (#8) is **deprioritized** — low-value for this target,
parked. Next, in priority order: **keep mining with the differential suite (#11)** — it's the highest-value
correctness work, and **long long is still unverified** (start there, then bitfield-heavy code and deep
call chains). Then the code-size/speed lift — branch relaxation (#14; its #13 prerequisite is done) and peephole/cost tuning (#12) —
plus the Section C limitation audit (#16, #17).
(`__interrupt(n)` auto-wiring is **done** — `void f(void) __interrupt(VEC_*)` emits `_irq_v<N>` at the
handler's entry, where N is the cart vector slot; the runner BIOS and `<pm.h>` VEC_* use the real PM
forwarding permutation, https://www.pokemon-mini.net/documentation/bios/.) See [HANDOFF.md](HANDOFF.md)
for current state and [building-roms.md](building-roms.md)
for the end-user guide.

---

## Z80-artifact scrub — scope (#20)

The port was cloned from SDCC's multi-variant `z80` backend. The variant *predicate
machinery* (`IS_Z80`/`IS_RAB`/…) is already constant-folded away (s1c88.h), so what
remains is **names, dead per-variant data, and comments** — not live wrong-variant
branches. Inventory (scan of `src/s1c88/` + `sdas/as88/`), in rough effort order:

**A. The `cost2` 7-variant timing model — the bulk (gen.c).** `cost2()` is declared
`cost2(bytes, z80_states, z180_states, r2k_clocks, sm83_cycles, tlcs90_states,
ez80_z80_cycles, r800_cycles)` but the body uses only `bytes` + `z80_states`; the
other **6 columns are dead** yet passed at **491 call sites**. Collapse to
`cost2(bytes, cycles)`, strip the 6 dead args everywhere (scriptable), rename
`z80_states`→`cycles`. High-volume but mechanical. *(Whether the kept numbers are
S1C88-accurate vs z80 is a separate concern — #12 cost tuning.)*

**B. Port-private identifiers to rename `*z80*`→`*s1c88*` — medium, low risk (build
catches misses).**
- `main.c` PORT wiring: `_z80_options`, `_z80_init`, `_z80_genAssemblerStart`,
  `_z80_builtins`, `_z80LinkCmd`, `_z80AsmCmd`, the asm-dialect/lib config
  (`_s1c88_z80asm_z80`, `_s1c88_asxxxx_z80`, `_s1c88_gas_z80`, `_libs_z80`).
- `gen.c`: `genZ80iCode`, `dryZ80Code` (`genS1C88Code` is already done).
- `peep.c`: `z80MightReadFlag[Condition]`, `z80SurelyWrites[Flag]`, `z80SurelyReturns`,
  `z80MightBeParmInCallFromCurrentFunction`, `z80UncondJump`, `z80CondJump`,
  `z80MightRead` (~11 functions).
- `s1c88.h`: the `Z80_OPTS` struct (→ `S1C88_OPTS`).

**C. Comments / variant notes — high count (~78 in gen.c alone), lowest risk.**
"the Rabbit has…", "SM83 does…", "eZ80 can…", "gbz80 flag handling…" notes that don't
apply to a single-variant port — trim/delete. File headers too: `s1c88.h`
(`z80/z80.h Common definitions for the z80-related ports`), `gen.c` (`code generator
for Z80 and related`), and `Derived from z80mch.c` in the assembler.

**D. Dead-variant functional toggles — verify, then remove (needs care).**
- `HAS_IYL_INST` + `IYL_IDX`/`IYH_IDX`: eZ80 byte-addressable index registers; the
  S1C88 IX/IY are NOT byte-addressable (s1c88.h flags this as the pending #7c removal).
  Removing collapses several `gen.c` branches.
- `nmosZ80` / `OPTION_NMOS_Z80` / `allow_undoc_inst`: the z80 undocumented-instruction
  toggle; confirm it gates nothing meaningful on the S1C88, then drop.

**E. Emitted z80 branch mnemonics `jp`/`jr`/`call` — large, coupled to #14 (DEFER).**
The codegen emits z80 `jp`/`jr`/`call`; the peephole control-transfer rules translate
them to S1C88 `jrs`/`jrl`/`cars`/`carl`. Emitting S1C88 directly requires solving
branch-form selection — that **is** #14 (linker relaxation). So #20 covers only the
comment/name hygiene around these; the mnemonic emission itself retires with #14.
(`ld`/`ex` are native S1C88 and stay; spot-check `scf`/`ccf`.)

**F. MUST NOT touch — shared core / external contract.**
`TARGET_Z80_LIKE`, `TARGET_IS_Z80`, `IS_Z80`, `ASM_TYPE_Z80ASM` (shared SDCC core; the
port depends on being Z80-like — see CLAUDE.md). `sdldz80` is the ASxxxx z80-family
linker binary the build scripts invoke; rebranding to `sdld88` is a build-system change,
out of scope for a code scrub.

**Sequencing:** B + C are the cheap, high-clarity wins — do first in always-green slices
(`run-tests.sh` after each; corpus is byte-identical and catches behavior drift). A is
the mechanical bulk. D needs per-toggle verification. E defers to #14. F is off-limits.
