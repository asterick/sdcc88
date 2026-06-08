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

**None.** The differential suite is clean across integer, pointer, struct/union, function-pointer, long-long,
and **float** code (add / subtract / multiply / divide / compares / int↔float casts, all exact-operand). The
last open correctness bug — **#8 float subtraction** (`10.0-4.0` → `0x40C00182`) — is **FIXED**: it was a
register/move-ordering bug in `genUminusFloat` (the `-a1` sign-flip inside `__fssub` loaded the top byte into
A before spilling the low word, dropping byte 0 on the HLBA layout). Fix: copy the low bytes before the sign
flip. Regression: the subtraction / opposite-sign block in `tests/diff/cases/float.c`. *(Note: a full
cancellation `a-a` yields softfloat `-0.0` where hardware gives `+0.0` — a library zero-sign convention, not
a bug; harmless since `-0.0 == +0.0`.)*

---

## Correctness coverage — keep mining the differential suite (#11)

The highest-value ongoing work. Each new `tests/diff/cases/*.c` (+ a `tests/emu/cases/*.c` for ABI-shaped
behaviour) is run through `corpus-check` + `emu-test` + `diff-test`; the suite has caught several real
**silent** miscompiles that byte-identical assembly never could (struct-arg register-drop, the `cp ba,hl`
pointer-compare peephole gap, the long-long/struct return-ABI off-by-one). Covered: arith, bitfields, calls,
control, longlong, memory, ptrarith, switch, structargs, fnptr2, unions, float — plus the emu ABI cases.
**Still untested (pick any; new territory is also fair game):**

- **#11-libc** — `mem*`/`str*` differential (memcpy / memmove / memset / strcmp / strlen …) run through the lib.
- **#11-longshift** — 32-bit shifts/rotates by a *variable* count + long-division edge values (beyond
  `arith.c`'s fixed-count shifts).

Workflow: add the case, run the three gates, fix what surfaces, add an emu/diff regression for any bug.

---

## Code size (#12, #14) — measurable via `scripts/size-check.sh` (corpus baseline, currently 8424 B)

- **[S] #12-flag-reuse byte-combine peephole.** The post-16-bit-add zero test
  `add hl,X ; ld a,h ; ld b,l ; or a,b ; jr Z` (the `(a+b)?` / function-result idiom) is now provably
  redundant — the flag model knows `add hl,X` sets Z — but capturing it needs a peephole that sees past the
  `push b`/`pop b` register-preservation noise. ~5 insns/site when it fires.
- **[S] #12-far-idiom.** Tighten the `__far` EP=0 deref sequences and (now that #14b relaxation landed) the
  `bcall`/`bjump` slot idioms.
- **[S] int×int→long widening.** `has_mulint2long` is off in `main.c`, so int×int→long widens to 32-bit and
  calls `__mullong`. Writing the hand `__muluint2ulong`/`__mulsint2slong` asm + enabling the flag gives a
  smaller/faster widening multiply (`arith.c`'s `widemul` cases will exercise it automatically). Code-size
  nicety, not correctness.
- **[S] #12 residual cleanup.** Prune the dead z80-mnemonic tokens (`rlca`/`scf`/`daa`/…) from multi-token
  `same()` lists in `peeph.def` (byte-identical). Refining `cost2`'s cycle numbers to exact S1C88 counts is
  **low value** for this target — the allocator cost is bytes-dominated (cycles discounted 64–512×; see the
  cost-model memory) — so do it only as part of a speed-focused pass.
- **[L, deferrable] #14c — linker cross-module branch relaxation.** sdld is fixed-size. Add a relaxation
  pass that, after area placement, iteratively shrinks in-range same-bank *cross-module* `bcall`/`bjump`
  slots and reflows subsequent addresses / symbols / relocs to a fixpoint, then re-emits. Staged: (i)
  single-pass conservative shrink; (ii) iterate to fixpoint; (iii) conditional-trampoline shrink. #14b
  already covers single-module / common-bank-heavy programs, so this is the harder remaining tail. Gate on
  `branch-smoke.sh` + emu/diff and re-baseline the corpus sizes. (Design: `banked-branch.md` "Relaxation
  plan".)
  - **✅ Stage 0 (opportunity report) DONE.** `linksrc/lkrloc3.c` tallies every cross-module
    `R_S1C88_BANK` slot at link time and `lkmain.c` prints the reclaimable-bytes summary; gated behind
    `SDLD_RELAX_REPORT=1`, read-only (corpus byte-identical). Run: `SDLD_RELAX_REPORT=1 make -C examples/hello`.
    First data: hello = **87 B reclaimable**, all `carl`-range ⇒ the simple stage-(i) shrink captures it all.
    Remaining = the reflow itself: **(i)** single-pass shrink + address/symbol/reloc reflow, **(ii)** iterate,
    **(iii)** trampoline/`cars` refinement. Stage 0 is the inventory those consume.

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
  - ⏸ **Remaining (deferred, low value):** the gas/z80asm asm-DIALECT trees (`mappings.i` `_z80asm`,
    `_s1c88_z80asm_z80`, `_s1c88_gas_z80` + their mapping tables, and the `--asm=gas`/`--asm=z80asm` branches
    in `main.c`). These are non-functional for the S1C88 (the toolchain is asxxxx/sdas-only), but removal is
    entangled with the multi-dialect `ASM_TYPE` machinery and those paths have **zero test coverage**, so a
    botched edit wouldn't be caught — poor risk/reward for cosmetic gain. Leave the dialect system intact
    (it's inert; the default is always asxxxx).
  - ⏸ **Remaining (deferred):** the peephole **flag-token model** (`pf`/`sf`/`hf`/`nf`/`vf`/`lf` — z80 flag
    names). This is NOT a clean rename: the S1C88 has Z/C/V/N (no S/P/H, and N is negative not add-subtract),
    so mapping the z80 flag set onto the S1C88 is a real flag-semantics task (it touches the same analysis as
    the #12-flag-reuse work), not cosmetic. Defer until/unless it's worth a careful pass.

  (Done in always-green slices — each byte-identical, run-tests 50/50, smokes green.)

- **(F) MUST NOT touch — shared core / external contract.** `TARGET_Z80_LIKE`, `TARGET_IS_Z80`, `IS_Z80`,
  `ASM_TYPE_Z80ASM` (shared SDCC core — the port DEPENDS on being z80-like, see `CLAUDE.md`) and `sdldz80`
  (the ASxxxx linker-binary / build-script contract). The `@file … derived from the z80 port` provenance
  headers stay as factual lineage.
