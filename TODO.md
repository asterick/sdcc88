# sdcc88 — backlog

Forward backlog for the existing source base. **The toolchain is complete and in maintenance:**
`sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx game.min` builds a bootable Pokémon Mini ROM, and every
gate is green — corpus 20/20 byte-identical, emu-test 17/17, diff-test 16, run-tests 56/56, plus the
driver / crt0 / rom / branch / insn-size / vec-reorder smokes and the `examples/hello` build. Corpus ROM
size baseline = **8352 B** (`scripts/size-check.sh`).

Completed work is **not** re-listed here — it lives in git history + commit messages, the per-topic Claude
memories, and `docs/s1c88/abi-decision.md`. The design/ABI is [`docs/s1c88/abi-decision.md`](docs/s1c88/abi-decision.md);
the end-user guide is [`docs/s1c88/building-roms.md`](docs/s1c88/building-roms.md).

Legend: **S/M/L** = rough effort.

---

## Known bugs

**None open.** The differential suite is clean across integer, pointer, struct/union, function-pointer,
long-long, float, the libc subset, and the previously-broken inline 32-bit variable shift.

- ✅ **#11-longshift-iy — FIXED.** The 32-bit variable LEFT shift (`u32 << n`) miscompiled when BOTH the
  value and the count were memory operands (e.g. `out = arr[i] << cc;`): `genLeftShift` puts the loop count
  in non-byte-addressable IY, but with all four byte GPRs holding the value and HL busy, `genMove` to IY
  left IYH stale (or spilled the count to the stack and never reached IY) — the loop ran a garbage count.
  Fix (`genLeftShift`): load the 8-bit count into `IY = 0x00:count` explicitly, saving/restoring the value
  bytes (A, HL) it stages through, instead of relying on `genMove`. Corpus byte-identical (no corpus code
  hits this path); regression `tests/diff/cases/longshift_iy.c` (194 values — every count 0..31 across six
  value patterns, the inline path the old `longshift.c` deliberately avoided).

---

## Correctness coverage (#11) — highest-value ongoing work

Keep mining the differential suite: each new `tests/diff/cases/*.c` (+ a `tests/emu/cases/*.c` for
ABI-shaped behaviour) is run through `corpus-check` + `emu-test` + `diff-test`. The suite has caught
several real **silent** miscompiles that byte-identical assembly never could. Any untested C construct is
fair game. Workflow: add the case, run the three gates, fix what surfaces, add a regression for any bug.

---

## Code size (#12) — yardstick `scripts/size-check.sh`

- **[S] #12-flag-reuse / #12-far-idiom peepholes.** Both were investigated and found **inert on the current
  corpus** (the idioms don't occur), so they give no measurable, validatable win today. Revisit only if a
  future corpus case exercises them.
- **[S] #12 residual cleanup.** Prune dead z80-mnemonic tokens (`rlca`/`scf`/`daa`/…) from multi-token
  `same()` lists in `peeph.def`. Byte-identical — hygiene, **no size win**.

---

## ✅ #14e — stale symbol tables under #14c relaxation — FIXED (debug-info only)

#14c reflowed the emitted ROM down by `rlxDelta()` but left the linker's `s_addr`/`a_addr` model pre-relax,
so the **`.map`** printed stale, too-high addresses for any symbol past a dropped `ld nb` (reads as "jumps
to strange locations" when disps are cross-referenced against the map). The generated code was always
correct. Fix (`s1c88_banked_branch.patch`): added `s1c88RelaxedAddr()` next to `rlxDelta()` in `lkrloc3.c`
and applied it to the `.map` area-base + symbol DISPLAY in `lklist.c` — never to the load-bearing `symval()`
(the relocation math needs the model address). **Scope correction vs the original note:** only the `.map`
was actually stale — `.noi` already tracks relaxation (it reads relocated values), and `.sym`/`.lst` carry
module-relative offsets (no final addresses). Verified: relaxed `_key_a` now reads `0x21E4` and the byte
there is its real ISR prologue; corpus byte-identical (link output unchanged). Regression
`scripts/relax-symtab-smoke.sh`.

---

## #16 — codegen-boundary lift (research pass)

The ~66 `UNIMPLEMENTED` sites are **loud traps, never silent miscompiles** (a `cost(4000)` dry-run penalty
steers the allocator away). Construct a triggering C snippet per site, classify reachable-vs-cost-avoided,
fix the cheap reachable ones, delete the genuinely-impossible guards. Categories cataloged in
`docs/s1c88/abi-decision.md` "Known codegen boundaries". None is a correctness risk today.

---

## #20 — z80-artifact scrub remainder

The bulk is done. Remaining: the peephole **flag-token model** (`pf`/`sf`/`hf`/`nf`/`vf`/`lf` — z80 flag
names). NOT a clean rename — the S1C88 has Z/C/V/N (no S/P/H; N is negative, not add-subtract), so it's a
real flag-semantics task that overlaps the #12-flag-reuse analysis. Defer until it's worth a careful pass.

**MUST NOT touch:** `TARGET_Z80_LIKE` / `TARGET_IS_Z80` / `IS_Z80` (shared SDCC core — the port depends on
being z80-like) and `sdldz80` (the ASxxxx linker-binary / build-script contract).

---

## Deferred — pick up on demand

- **#17-setjmp** — upstream `_setjmp.c` is mcs51-only (`#include <8051.h>`); an s1c88 `setjmp`/`longjmp`
  needs port asm (save/restore SP, return PC, callee-saved IX/IY). Rarely needed on this target.
- **#17-malloc** — needs a heap area + `_sdcc_heap` wired into crt0/linker; a real design choice on a 4 KB-RAM
  device.
