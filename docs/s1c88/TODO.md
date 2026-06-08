# sdcc88 TODO — toward a usable toolchain

Status snapshot (2026-06-07): **THE CRITICAL PATH (Section A) IS DONE — the toolchain is usable.**
`scripts/setup-sdk.sh` builds everything in one command; `sdcc -ms1c88 game.c -o game.ihx && romgen
game.ihx game.min` produces a bootable Pokémon Mini ROM, with the production `crt0` (real `"PM"`/
`"NINTENDO"` header), the auto-linked `s1c88.lib`, the `<pm.h>` device header, and a C `romgen` (no
Python). `examples/hello/` is a copy-me project; `docs/s1c88/building-roms.md` is the how-to. All gates
green (corpus 20/20, emu-test 12/12, diff-test 5, driver/crt0/rom/branch smokes, example). **Remaining =
Section B (quality/coverage) and Section C (documented limitations).**

Legend: **S/M/L** = rough effort. Items are roughly dependency-ordered within each section.

---

## A. Critical path to a usable toolchain — ✅ DONE

`sdcc -ms1c88 game.c` → assemble → link (banked) → `.min` works end to end. Delivered:

1. **Driver tool wiring** — `main.c` `_z80AsmCmd`→`sdas88`, `_libs`→`s1c88`; integrated driver emits valid XL3 `.rel`.
2. **Preprocessor** — `scripts/build-sdcpp.sh` builds the real SDCC cpp (libiberty + `support/cpp`); `sdcc -ms1c88 foo.c` preprocesses for real (no `--c1mode`).
3. **Production crt0** — `device/lib/s1c88/crt0.s`: the real PM cartridge header (`"PM"` @ 0x2100, 27 × 6-byte vector slots, `"NINTENDO"` @ 0x21A4) + C runtime (EP=XP=YP=0, IRQ mask, gsinit, `__sdcc_fptr`, `bcall _main`). SP is left as the BIOS sets it. The emulator runner has a minimal BIOS (auto-detected via `"PM"`) that synthesizes the low vector table and enters via the reset vector.
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
11. **[ongoing] Keep mining with the differential suite.** It has found multiple real codegen bugs; more
    modules (longs, bitfield-heavy, deep call chains) will find more.
12. **[L, open-ended] Peephole / cost tuning.** The standing "remaining" codegen item — size and speed
    (codegen is correct-first, not yet tuned).
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
14. **[L, large lift] Linker branch relaxation (shrink `bjump`/`bcall`).** *(Prerequisite #13 is ✅ DONE.)*
    Today the compiler emits `bjump`/`bcall` as the always-long bank-switching form (the linker picks the
    bank, not the *size*): worst-case `ld nb,#bank ; carl/jrl` (~6 bytes). When the resolved target is in
    the **same bank** and within relative range, the linker should shrink it to the smallest legal form —
    drop the `ld nb` bank-switch, pick `cars`/`jrs` (8-bit) over `carl`/`jrl` (16-bit) when it fits, and
    for conditionals choose a plain short `jrs <cc>` over the #13 trampoline when in range. This is a
    classic **relaxation** pass: iteratively shrink branches and recompute addresses to a fixpoint
    (shrinking one branch moves later addresses, which can let *more* shrink), being careful that no branch
    that fit stops fitting. The ASxxxx/sdld model is fixed-size by default, so this means adding a
    relaxation phase over the `R_S1C88_BANK`/PC-relative relocs + address recomputation — large, but a big
    code-size/speed win. Cross-check against `branch-smoke.sh` and re-baseline the corpus afterward.
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

16. **`UNIMPLEMENTED` traps.** Audit + document the pathological shapes that bail loudly (e.g.
    `--reserve-regs-iy` + >127-byte frame + multi-byte pointer read; shift/cast corners). Loud traps
    today, not silent miscompiles — but users should know the boundaries.
17. **CPOINTER (code-space `const` data pointers).** 3 bytes but deref'd near-only; fine while const data
    stays in the common bank — document the convention (or lift it).
18. **float / long long correctness** — float subtraction is the open #8 bug; long long is unverified.
19. **Structured test runner — ✅ DONE.** `scripts/run-tests.sh` builds the compiler once, runs every
    suite (corpus / emu / diff / toolchain smokes) **in parallel**, and emits one **TAP version 13**
    stream — one test point per case, with per-assertion `#` diagnostics (emu `CHECK` failures), a `1..N`
    plan, and a summary; exits non-zero on any failure. The case-suites gained an opt-in `TAP=1` mode
    (clean `ok`/`not ok` body on stdout, build noise to stderr); their default human output is unchanged.
    38 points green.

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
