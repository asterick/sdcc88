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

**Covered so far:** integer arith/casts/shifts (`arith`), bitfields, calls, control flow, float, fnptr,
libc, long long, long shifts, pointer arith, sprintf, struct args, switch, unions — and now
**read-modify-write** (`compound`: compound-assign `+= … >>=` and `++`/`--` across every width/signedness ×
direct/indexed/indirect addressing modes).

> **Test-authoring rule (learned writing `compound`):** keep per-iteration work in **small straight-line
> helper functions** called from the loop (as `arith` does), never a single giant macro-expanded
> `diff_run`. SDCC's codegen is **superlinear in function/basic-block size** — a ~4000-line function takes
> minutes and a ~6000-line one effectively hangs the compiler (measured: 1.4s→6.5s→42s→∞ as one function
> grew). This is an inherent SDCC characteristic, not an s1c88 bug; real code rarely hits it, but a
> macro-heavy test can. Also watch the **near common-bank budget** (0x2100–0x7FFF): a too-large case
> overflows it (`romgen: common-bank overflow`) — split it or trim coverage.

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

## ✅ #16 — codegen-boundary lift (research pass) — DONE

The 66 `UNIMPLEMENTED` sites are **loud traps, never silent miscompiles** (a `cost(4000)` dry-run penalty
steers the allocator away). An instrumented census (the macro made to log dry-vs-real instead of aborting)
compiled the full test battery under all `{default, --reserve-regs-iy, --opt-code-speed}` combos plus
hand-built triggers per category. **Result: 65/66 are cost-avoided** (fire only in the dry run, never in
real emit); the **one** reachable site — `genPointerPush` aborting on `f(struct, char, int)` — is now
**fixed** (second parked pair stashes into the `__sdcc_fptr` scratch cell; emu `18_structarg2`). The other
65 are left as live traps: a not-triggered guard isn't a proven-impossible guard, so deleting them would be
unsound. See `docs/s1c88/abi-decision.md` "Known codegen boundaries". None was a correctness risk.

---

## ✅ #20 — z80-artifact scrub — DONE

The full scrub is complete: dead multi-dialect asm machinery removed, the `jp→jr→jrs` chain collapsed, and
the peephole **flag-token model** corrected to the real S1C88 flag set. The z80 model carried six flags
(`zf`/`cf`/`sf`/`pf`/`nf`/`hf`); the S1C88 has only **four** — Z/C/V/N. The z80 `nf` (add-subtract) and `hf`
(half-carry) don't exist here and were only ever read by `daa` (which the port never emits), so they were
dead — removed from `peep.c` + `peeph.def`. The remaining tokens are renamed to S1C88 names: `pf`→`vf` (V
overflow), `sf`→`nf` (N negative). **Corpus byte-identical** — proving the change is behavior-preserving (it
removes genuinely-dead bookkeeping and renames consistently), not a codegen change.

**MUST NOT touch (by design):** `TARGET_Z80_LIKE` / `TARGET_IS_Z80` / `IS_Z80` (shared SDCC core — the port
depends on being z80-like) and `sdldz80` (the ASxxxx linker-binary / build-script contract).

---

## Out of scope (won't do)

- **`setjmp`/`longjmp`** and **`malloc`/`free`** — not pursued. A Pokémon Mini ROM has no realistic use for
  either (no non-local-jump-heavy C, and dynamic allocation is impractical on 4 KB of RAM). Both need real
  port work (setjmp = hand asm; malloc = a heap area + `_sdcc_heap` wiring) for no benefit, so they stay
  unimplemented by design.
