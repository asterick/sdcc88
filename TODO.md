# sdcc88 — backlog

Forward backlog for the existing source base. **The toolchain is complete and in maintenance:**
`sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx game.min` builds a bootable Pokémon Mini ROM, and every
gate is green — corpus **20/20** byte-identical, emu-test **18/18**, diff-test **25**, run-tests **67/67**,
plus the driver / crt0 / rom / branch / insn-size / vec-reorder / relax-symtab smokes and the
`examples/hello` build. Corpus ROM size baseline = **8352 B** (`scripts/size-check.sh`).

Completed work is **not** re-listed here — it lives in git history + commit messages, the per-topic Claude
memories, and `docs/s1c88/abi-decision.md`. The design/ABI is [`docs/s1c88/abi-decision.md`](docs/s1c88/abi-decision.md);
the end-user guide is [`docs/s1c88/building-roms.md`](docs/s1c88/building-roms.md).

Legend: **S/M/L** = rough effort.

---

## Known bugs

**None open.** The differential suite (25 cases) is clean across integer arith / casts / mixed-width
conversion trees, pointer + `__far` pointer, struct/union + aggregate-member read-modify-write,
function-pointer, recursion, irregular control flow, bit-manipulation idioms, long-long, float, the libc
subset, and `volatile`.

---

## Open work

### #12 — code size (yardstick `scripts/size-check.sh`, baseline 8352 B)

- **[S] #12-flag-reuse / #12-far-idiom peepholes.** Both were investigated and found **inert on the current
  corpus** (the idioms don't occur), so they give no measurable, validatable win today. Revisit only if a
  future corpus case exercises them.
- **[S] #12 residual cleanup.** Prune dead z80-mnemonic tokens (`rlca`/`scf`/`daa`/…) from multi-token
  `same()` lists in `peeph.def`. Byte-identical — hygiene, **no size win**.

That is the entire forward backlog. Everything else is done (git history) or out of scope (below).

---

## Test infrastructure — #11 differential mining (concluded: suite is comprehensive)

The differential harness (`scripts/diff-test.sh`, host-vs-emulator) is considered **comprehensive** — every
distinct codegen dimension now has a case, and the mining caught one real silent miscompile along the way
(the `genLeftShift` variable-shift counter clobber). #11 is closed as an active task; the harness and these
notes remain for any future additions. To add a case: drop a `tests/diff/cases/*.c` defining `diff_run()`,
run the three gates (`corpus-check` + `emu-test` + `diff-test`), fix what surfaces, add a regression.

Two authoring constraints (learned the hard way):
- **Small helper functions, not one giant `diff_run`.** SDCC's codegen is **superlinear in basic-block
  size** — a ~6000-line function effectively hangs the compiler (measured 1.4s→6.5s→42s→∞ as one function
  grew). Put per-iteration work in small straight-line helpers (as `arith` does).
- **Mind the near common-bank budget** (0x2100–0x7FFF): a too-large case overflows it
  (`romgen: common-bank overflow`) — split it or trim coverage.

`volatile` is testable via the emulator's side-effecting **probe register at 0x2070** (`machine.cc`): a READ
returns a counter then post-increments, a WRITE seeds it — so N volatile reads yield N consecutive values.
See `tests/diff/cases/volatile.c` (sensitivity-checked: it diverges if the `volatile` qualifier is dropped).

---

## Out of scope (won't do)

- **`setjmp`/`longjmp`** and **`malloc`/`free`** — not pursued. A Pokémon Mini ROM has no realistic use for
  either (no non-local-jump-heavy C, and dynamic allocation is impractical on 4 KB of RAM). Both need real
  port work (setjmp = hand asm; malloc = a heap area + `_sdcc_heap` wiring) for no benefit, so they stay
  unimplemented by design.
