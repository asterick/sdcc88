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
15. **[S] `__mul*int2*long` widening differential coverage.** Skipped in the diff harness for lack of the
    support routines; add once those exist (also unblocks `_fsmul` in #8).

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
