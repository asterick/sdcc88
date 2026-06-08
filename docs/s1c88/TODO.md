# sdcc88 TODO — toward a usable toolchain

Status snapshot (2026-06-07): the four core tools (`sdcc -ms1c88`, `sdas88`, `sdldz80` +
banked branches, `romgen.py`) all **work** and are exercised end-to-end by the script
harnesses (`scripts/*.sh`, `tests/`). But they only work *through those scripts* — a user
typing `sdcc -ms1c88 game.c` today FAILS, because the driver integration, runtime, and
packaging aren't done. The codegen is functionally complete + verified (corpus 20/20,
emu-test 8/8, diff-test 4 modules); 5 reachable codegen bugs found+fixed via the test layers.

Legend: **S/M/L** = rough effort. Items are roughly dependency-ordered within each section.

---

## A. Critical path to a usable toolchain

A user should be able to: `sdcc -ms1c88 game.c` → assemble → link (banked) → `.min`.

1. **[S, blocking] Driver tool wiring.** `src/s1c88/main.c` still names the z80 tools:
   `_z80AsmCmd = "sdasz80"` must become **`sdas88`**; `_crt = "crt0.rel"` and `_libs = "z80"`
   must point at the s1c88 startup + lib. (`_z80LinkCmd = "sdldz80"` is already correct — our
   banked linker is sdldz80.) Without this the integrated assemble/link uses the wrong
   assembler and missing files.
2. **[S] Preprocessor wiring.** `sdcpp` is built and works when invoked directly, but the
   driver calls bare `sdcpp` via PATH → fails (so `sdcc foo.c` without `--c1mode` can't
   preprocess). Needs install/PATH or an absolute path.
3. **[M] A real crt0 / startup.** Only a *test* crt0 exists (`tests/emu/crt0.asm`). Need a
   production startup: Pokémon Mini ROM header, the real interrupt **vector table**, `_DATA`
   zero + `_INITIALIZER`→`_INITIALIZED` copy, stack + EP/XP/YP=0 setup, the `__sdcc_fptr`
   cell, `bcall _main` entry — assembled to `crt0.rel` where the driver finds it.
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
13. **[L, large lift] Linker branch relaxation (shrink `bjump`/`bcall`).** Today the compiler
    emits `bjump`/`bcall` as the always-long, bank-switching form (the linker picks the bank but
    not the *size*): worst-case `ld nb,#bank ; nop ; carl/jrl` (~6–7 bytes). When the resolved
    target is in the **same bank** and within relative range, the linker should shrink it to the
    smallest legal form — drop the `ld nb` bank-switch entirely, and pick `cars`/`jrs` (8-bit
    relative) over `carl`/`jrl` (16-bit) when it fits. This is a classic **relaxation** pass:
    iteratively shrink branches and recompute all addresses to a fixpoint (shrinking one branch
    moves later addresses, which can let *more* shrink), being careful that no branch that fit
    stops fitting. The ASxxxx/sdld model is fixed-size by default, so this means adding a
    relaxation phase over the `R_S1C88_BANK`/PC-relative relocs + address recomputation — large,
    but a big code-size/speed win. Cross-check against `branch-smoke.sh` (the displacement
    convention) and re-baseline the corpus afterward.

    Part of the same task: **conditional `bjump`/`bcall` via invert-and-skip trampolines.** The
    long forms (`carl`/`jrl`) and the linker's `bjump`/`bcall` only support the basic conditions
    `c/nc/z/nz`; the signed/flag conditions (`lt/ge/gt/le/v/nv/p/m/f0..nf3`) exist **only** as
    short relative (`jrs`/`cars`, ±127). So when such a conditional must reach a far / out-of-
    range / cross-bank target, the linker must synthesize a trampoline: invert the short
    condition to skip over an unconditional long branch/call —
    `jrs <inverted-cond>, .+<len> ; bjump/bcall target`. This generalizes the assembler's
    task-#10 lowering (local `jp <signed cc>` → `jrs <inv>,+4 ; jrl e`) to the linker's
    bank-switching path, chosen as part of the same size-selection/relaxation pass (a short
    conditional that fits stays a plain `jrs`; only out-of-range ones grow the trampoline).
14. **[S] `__mul*int2*long` widening differential coverage.** Skipped in the diff harness for
    lack of the support routines; add once #4 (real lib) exists.

---

## C. Known limitations — fix or formally document

15. **`UNIMPLEMENTED` traps.** Audit + document the pathological shapes that bail loudly (e.g.
    `--reserve-regs-iy` + >127-byte frame + multi-byte pointer read; shift/cast corners). Loud
    traps today, not silent miscompiles — but users should know the boundaries.
16. **CPOINTER (code-space `const` data pointers).** 3 bytes but deref'd near-only; fine while
    const data stays in the common bank — document the convention (or lift it).
17. **float / long long correctness** — unverified until #8 exists.
18. **[optional] Structured test runner** (TAP/parallel/per-assertion). Current bash + exit
    codes is fine but doesn't scale to many cases.

---

## Suggested sequencing
A1+A2 first (tiny, unblocks the real driver) → A3→A4→A5 (crt0/lib/headers — the substance of
"usable") → A6/A7 (package it). Run **B8 (float)** in parallel as the highest-value quality
item (biggest untested surface). See [HANDOFF.md](HANDOFF.md) for current state + per-session log.
