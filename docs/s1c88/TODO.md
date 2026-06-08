# sdcc88 TODO — toward a usable toolchain

Status snapshot (2026-06-07, session 26): **A1+A2+A3 done** — the integrated driver now
preprocesses (real sdcpp built), compiles, and assembles via `sdas88`, and a production
`crt0.s` (real PM `"PM"`/`"NINTENDO"` header + vector table + C runtime) is built and boots a C
`main()` end to end through a new mini-BIOS in the emulator runner. **The one remaining gap to
`sdcc -ms1c88 game.c` linking cleanly is the `s1c88` support library (#4).** The four core tools
(`sdcc -ms1c88`, `sdas88`, `sdldz80` + banked branches, `romgen.py`) all **work**. Codegen is
functionally complete + verified (corpus 20/20, emu-test 8/8, diff-test 4 modules); 5 reachable
codegen bugs found+fixed via the test layers.

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
4. **[M] Target C runtime library.** Today each test compiles `_divuint`/`_mullong`/… ad-hoc.
   Build SDCC's `device/lib` for s1c88 into an **`s1c88.lib`** (div/mul + the `__mul*int2*long`
   widening helpers, native byte-loop memcpy/memset/strcpy/…, float/longlong support) and
   package it so the driver auto-links it.
5. **[M] Device headers.** A Pokémon Mini hardware header (MMIO map at `0x20xx`, IRQ vector
   numbers, timer/IRQ/PRC register bits) so users don't hand-write magic addresses (cf. the
   `0x2018/0x2027/0x10` constants in `tests/emu/cases/08_isr.c`).
6. **[S/M] romgen integration.** Fold `scripts/romgen.py` into the build as the documented
   final `.ihx`→`.min` step (driver post-link hook or a Makefile rule), incl. the `--far` /
   bank-range declaration story.
7. **[M] Packaging + example + docs.** An install/SDK layout (tools + `s1c88.lib` + headers +
   `crt0.rel` together), a minimal example project with a Makefile, and a "how to build a ROM"
   doc. This is what turns "works in our scripts" into "someone else can use it."

---

## B. Quality & coverage (discussed / wanted)

8.  **[M] Differential module: float / softfloat.** Large, entirely *unexecuted* surface
    (arith, compares, int↔float casts) — high bug probability, like the integer modules were.
9.  **[S] Differential module: volatile / MMIO.** Verify volatile loads/stores aren't
    elided/reordered/duplicated (matters for the #5 hardware registers).
10. **[S/M] `__critical` + nested-IRQ execution coverage.** Emu cases for SC-masking critical
    sections and an ISR interrupting another (now possible with the kept timers/IRQ).
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
A1+A2 first (tiny, unblocks the real driver) → A3→A4→A5 (crt0/lib/headers — the substance of
"usable") → A6/A7 (package it). Run **B8 (float)** in parallel as the highest-value quality
item (biggest untested surface). See [HANDOFF.md](HANDOFF.md) for current state + per-session log.
