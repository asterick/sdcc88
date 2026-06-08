# sdcc88 TODO — toward a usable toolchain

Status snapshot (2026-06-07, session 26): **THE CRITICAL PATH (A.#1–#7) IS DONE — the toolchain
is usable.** `scripts/setup-sdk.sh` builds everything in one command; `sdcc -ms1c88 game.c -o
game.ihx && romgen game.ihx game.min` produces a bootable Pokémon Mini ROM, with the production
`crt0` (real `"PM"`/`"NINTENDO"` header), the auto-linked `s1c88.lib`, the `<pm.h>` device header,
and a C `romgen` (no Python). `examples/hello/` is a copy-me project (`make` → `.min`, boots), and
`docs/s1c88/building-roms.md` is the how-to. All gates green (corpus 20/20, emu-test 8/8, diff-test
4, driver/crt0/rom/branch smokes, example). **Remaining = Section B (quality/coverage: float diff
module #8, volatile/MMIO #9, peephole tuning #12, branch relaxation #13/#14) and Section C.**

Legend: **S/M/L** = rough effort. Items are roughly dependency-ordered within each section.

---

## A. Critical path to a usable toolchain

A user should be able to: `sdcc -ms1c88 game.c` → assemble → link (banked) → `.min`.

1. **[S, blocking] Driver tool wiring. ✅ DONE (session 26).** `src/s1c88/main.c`:
   `_z80AsmCmd` `"sdasz80"`→**`"sdas88"`**, `_libs` `"z80"`→**`"s1c88"`** (`_z80LinkCmd
   = "sdldz80"` was already correct; `_crt = "crt0.rel"` is generic). Verified: the
   integrated driver invokes `sdas88 -plosgffw` and produces a valid XL3 `.rel`.
2. **[S] Preprocessor wiring. ✅ DONE (session 26).** The installed `bin/sdcpp` was a thin
   wrapper that exec'd whatever `cpp` lived in `support/cpp/gcc/` — but the real SDCC cpp was
   never built (`build.sh` does only `make -C src`). **`scripts/build-sdcpp.sh`** now builds
   `support/sdbinutils/libiberty` + `support/cpp` (the GCC-cpp fork), so the wrapper finds the
   real cpp. Verified end to end: `sdcc -ms1c88 foo.c` preprocesses (sdcpp) → compiles → `sdas88`
   → `.rel`, no `--c1mode` needed.
3. **[M] A real crt0 / startup. ✅ DONE (session 26).** `device/lib/s1c88/crt0.s`: the real
   Pokémon Mini cartridge header — **`"PM"` marker @ 0x2100**, the **6-byte IRQ vector slots**
   (`ld nb,#page ; jrl handler`; reset→`__start`, 26 maskable→`_irq_default` RETE), **`"NINTENDO"`
   watermark @ 0x21A4** (the optional 0x21BC tail is dropped, unchecked by hardware) — plus the C
   runtime: stack, **EP=XP=YP=0** (the EP=0 invariant), IRQ mask, gsinit
   (`_INITIALIZER`→`_INITIALIZED`; `_DATA` zero-init needs no loop — the BIOS clears RAM at boot,
   as does the runner), the **`__sdcc_fptr`** cell, `bcall _main`. `scripts/build-runtime.sh`
   assembles it to `crt0.rel` and installs it in the driver's lib dir (the "couldn't find crt0.rel"
   warning is gone — only the `s1c88` lib (#4) is still missing). **The emulator runner gained a
   minimal BIOS** (auto-detected via `"PM"`): it synthesizes the 0x0000-0x00FF vector table from the
   cart's slots and enters via the reset vector, exactly as hardware does. `scripts/crt0-smoke.sh`
   boots a C `main()` through it end to end (header bytes verified, gsinit ran, `main()`=42); the 8
   existing emu cases (bare test crt0) still pass.
4. **[M] Target C runtime library. ✅ DONE (session 26).** `scripts/build-runtime.sh` now
   builds **`s1c88.lib`** and installs it (+ `crt0.rel`) in the driver's lib dir. It's the classic
   ASxxxx **text-index** format (one module name per line + the matching `<module>.rel` alongside)
   — sdldz80 reads it directly, no `sdar`/binutils needed. Members are SDCC's generic support
   routines **compiled through our own port** (self-hosting): the 10 the codegen actually emits
   implicit bcalls to — `__{div,mod,mul}{sint,uint}` (16-bit) + `__{div,mod,mul}{slong,ulong}`
   (32-bit); char ops use native DIV/MLT and shifts/widening are inline, so they need no library —
   plus a libc `mem*/str*` subset for user code. (Float/longlong deferred to #8/#18.) The driver
   was finished to match: `code_loc`/`data_loc` defaulted to the PM map (`_CODE`=0x2100 with the
   header at its front, `_DATA`=0x1000), and crt0 declares `_HOME` so library code chains into ROM.
   The **stack is left as the BIOS sets it on reset** (crt0 doesn't touch SP; the emulator runner's
   mini-BIOS parks SP below the test mailbox). **`sdcc -ms1c88 game.c` now builds a bootable PM ROM
   end to end** — `scripts/driver-smoke.sh` compiles a div-using program through the integrated
   driver, romgens, and runs it (header OK, `main()`=42).
5. **[M] Device headers. ✅ DONE (session 26).** `device/include/s1c88/pm.h` — the full PM MMIO
   map in Epson/official-SDK register names (SYS_/SEC_/TMR1-3_/TMR256_/IRQ_/AUD_/PRC_/LCD_/KEY_/
   IO_), with bit-field macros, the IRQ priority/enable/active flags, and the hardware **vector
   numbers** (`VEC_*`). Adapted to SDCC C from the EPSON/TASKING `c88-pokemini` header; addresses
   cross-checked against minimon-core. `build-runtime.sh` installs it to the driver's
   `include/s1c88` path; `#include <pm.h>` compiles + boots (verified: `PRC_MODE` write lands at
   0x2080, key-bit macros correct). *(Field names still worth a spot-check against the owner's
   wiki, which is JS-rendered and couldn't be fetched.)*
6. **[S/M] romgen integration. ✅ DONE (session 26).** `scripts/romgen.py` rewritten in C as
   `tools/romgen.c` (**no Python dependency in the shipped toolchain**, per the owner);
   `scripts/build-romgen.sh` builds it into the toolchain `bin/`. Byte-identical to the old Python
   on both the banked and `--far` paths. All harness scripts (emu-test, diff-test, rom-smoke,
   crt0-smoke, driver-smoke) now invoke `bin/romgen`; `romgen.py` removed. (The documented
   `.ihx`→`.min` final step is wired into the example Makefile under #7.)
7. **[M] Packaging + example + docs. ✅ DONE (session 26).** `scripts/setup-sdk.sh` builds the
   whole toolchain from a clean checkout in one command (compiler → sdcpp → sdas88 → sdldz80 →
   romgen → runtime), leaving `build/sdcc-4.5.0/` as a usable SDK (`bin/` tools, `src/sdcc`,
   `share/sdcc/{lib,include}/s1c88/`). **`examples/hello/`** is a copy-me project — `hello.c`
   (`#include <pm.h>` + a div from the lib), a `Makefile` (`make` → `.min`, `make run` → emulator),
   and a README; it builds and boots (exit 42). **`docs/s1c88/building-roms.md`** is the end-user
   guide (setup, the two-step `sdcc`→`romgen` build, the header, the memory map, interrupts,
   verification). This turns "works in our scripts" into "someone else can use it."

---

## B. Quality & coverage (discussed / wanted)

> **Session 26 (cont.): #8, #9, #10 done.** Float diff module (found a float-subtract codegen
> bug — see #8), volatile/MMIO coverage, and `__critical` execution coverage all landed; emu-test
> is now 10/10 (added `09_volatile`, `10_critical`), diff-test 5/5 (added `float`). **The one open
> correctness item surfaced this session: the `_fsadd` different-sign miscompile (#8) — all float
> subtraction.** Nested-IRQ (part of #10) deferred.


8.  **[M] Differential module: float / softfloat. ✅ BUILT (session 26) — found a bug.**
    `tests/diff/cases/float.c` (bit-exact `EMIT_F32`, exactly-representable operands) +
    the diff harness now links the float support routines as an on-demand text-index lib.
    Same-sign add, multiply, divide, compares, and all int↔float casts are CORRECT (30 values
    match). **It found a codegen bug (NOT yet fixed): float SUBTRACTION (and opposite-sign
    addition) corrupts the low mantissa bits** — `10.0-4.0` → `0x40C00182` not `0x40C00000`.
    `__fssub` is `-((-a)+b)`, so it routes through the `_fsadd` *different-sign* path; the
    algorithm + all isolated 32-bit ops compile correctly (reductions passed), so it's a
    register-pressure / spill bug in the full `_fsadd` compile, not a library issue. Re-add the
    subtract cases to `float.c` (the regression test) once fixed. **Also disabled `has_mulint2long`**
    (main.c) so int×int→long uses `__mullong` — our port lacks the `__mul*int2*long` widening
    routines (asm-only; they conflict with a C definition), which `_fsmul` needs (#15).
    ⮕ **Open: fix the `_fsadd` different-sign miscompile** (high impact — all float subtraction).
9.  **[S] volatile / MMIO coverage. ✅ DONE (session 26).** `tests/emu/cases/09_volatile.c` —
    a timer ISR bumps a `volatile` counter that `main` spin-reads; a discriminator fails if the
    load is hoisted/cached (the classic "poll a status register" bug). Verified in the asm that the
    spin re-loads `(_ticks)` every iteration; plus a volatile RAM round-trip (store-not-elided +
    ordering). Passes — volatile loads are not hoisted.
10. **[S/M] `__critical` execution coverage. ✅ DONE (session 26); nested-IRQ deferred.**
    `tests/emu/cases/10_critical.c` — first execution test of `__critical`: counts timer-ISR fires
    over an unmasked spin vs the same spin inside `__critical`; masked sees ~none, unmasked many, so
    SC-level masking (`or sc,#0xc0`) works. ⚠ Found: the emulator accepts a pending IRQ in the
    one-instruction window at the mask boundary, so reading the counter *immediately* after the
    masking instruction is unreliable (the test snapshots before entering the section + a generous
    threshold). **Nested IRQ (a higher-priority IRQ preempting a lower-priority ISR) is a follow-up**
    — it needs two phase-aligned timers; the emulator preemption path exists (`priority() <
    next_priority`) but making a second timer underflow reliably during the first ISR was fiddly.
11. **[ongoing] Keep mining with the differential suite.** It's found 5 real codegen bugs;
    more modules (longs, bitfield-heavy, deep call chains) will find more.
12. **[L, open-ended] Peephole / cost tuning.** The standing "remaining" codegen item — size
    and speed (codegen is correct-first, not yet tuned).
13. **[M] Conditional `bjump`/`bcall` via invert-and-skip trampolines.** *(Prerequisite for #14.)*
    The long forms (`carl`/`jrl`) and the linker's `bjump`/`bcall` only support the basic
    conditions `c/nc/z/nz`; the signed/flag conditions (`lt/ge/gt/le/v/nv/p/m/f0..nf3`) exist
    **only** as short relative (`jrs`/`cars`, ±127). So a conditional that must reach a far /
    out-of-range / cross-bank target needs an **invert-and-skip trampoline**: invert the short
    condition to skip over an unconditional long branch/call —
    `jrs <inverted-cond>, .+<len> ; bjump/bcall target`. This generalizes the assembler's
    task-#10 lowering (local `jp <signed cc>` → `jrs <inv>,+4 ; jrl e`) to the linker's
    bank-switching path. Doing it first gives #14 a uniform "every conditional has a reachable
    long form" model to size against.
14. **[L, large lift] Linker branch relaxation (shrink `bjump`/`bcall`).** *(Depends on #13.)*
    Today the compiler emits `bjump`/`bcall` as the always-long, bank-switching form (the linker
    picks the bank but not the *size*): worst-case `ld nb,#bank ; nop ; carl/jrl` (~6–7 bytes).
    When the resolved target is in the **same bank** and within relative range, the linker should
    shrink it to the smallest legal form — drop the `ld nb` bank-switch entirely, pick `cars`/
    `jrs` (8-bit relative) over `carl`/`jrl` (16-bit) when it fits, and for conditionals choose a
    plain short `jrs <cc>` over the #13 trampoline when in range. This is a classic **relaxation**
    pass: iteratively shrink branches and recompute all addresses to a fixpoint (shrinking one
    branch moves later addresses, which can let *more* shrink), being careful that no branch that
    fit stops fitting. The ASxxxx/sdld model is fixed-size by default, so this means adding a
    relaxation phase over the `R_S1C88_BANK`/PC-relative relocs + address recomputation — large,
    but a big code-size/speed win. Cross-check against `branch-smoke.sh` (the displacement
    convention) and re-baseline the corpus afterward.
15. **[S] `__mul*int2*long` widening differential coverage.** Skipped in the diff harness for
    lack of the support routines; add once #4 (real lib) exists.

---

## C. Known limitations — fix or formally document

16. **`UNIMPLEMENTED` traps.** Audit + document the pathological shapes that bail loudly (e.g.
    `--reserve-regs-iy` + >127-byte frame + multi-byte pointer read; shift/cast corners). Loud
    traps today, not silent miscompiles — but users should know the boundaries.
17. **CPOINTER (code-space `const` data pointers).** 3 bytes but deref'd near-only; fine while
    const data stays in the common bank — document the convention (or lift it).
18. **float / long long correctness** — unverified until #8 exists.
19. **[optional] Structured test runner** (TAP/parallel/per-assertion). Current bash + exit
    codes is fine but doesn't scale to many cases.

---

## Suggested sequencing
~~Section A (the critical path) is **done** (session 26).~~ Next: **B8 (float diff module)** is the
highest-value quality item (biggest untested surface — the integer modules each found real bugs);
**B9 (volatile/MMIO)** now matters because real ROMs use the `<pm.h>` registers; then peephole/cost
tuning (B12) and the branch-relaxation lift (B13→B14). A nice usability follow-up: auto-wire
`__interrupt(n)` functions to the crt0 vector table (today ISRs are installed by hand). See
[HANDOFF.md](HANDOFF.md) for current state + per-session log, and
[building-roms.md](building-roms.md) for the end-user guide.
