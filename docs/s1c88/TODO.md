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

**One open — #11-longshift-iy (silent miscompile, narrow trigger).** The **32-bit variable LEFT shift**
(`u32 << n`) miscompiles when BOTH the value and the count are **memory** operands (global/array), e.g.
`out = arr[i] << gcount;`. Minimal repro:
```c
unsigned long arr[2], out;  unsigned char cc;
void f(unsigned char i){ out = arr[i] << cc; }   /* loop runs a garbage count -> wrong result */
```
Cause: the 4-byte value fills A/B/L/H, so `genLeftShift` (gen.c) puts the loop counter in **IY**
(`count_iy`). IY isn't byte-addressable, so the count must be zero-extended through a pair scratch
(`ld l,c; ld h,#0; ld iy,hl`). That works when the count is loaded first with HL free (the param case —
`u32 f(u32 a,u8 c){return a<<c;}` is correct). But a memory value is materialised into HL+A+B *first*, so
HL is busy and `genMove(ASMOP_IY,count)` degenerates to reusing IY's **stale high byte** (or a pure
no-op) — the counter is garbage. **Only this op**: right shifts, 16/64-bit shifts, division, and any
register/param-sourced count all work. **Fix is deep** (genLeftShift IY path / genMove-to-IY zero-extend
without a free pair) and risks the shift codegen that otherwise passes everything — deferred. **Workaround
in code:** route the shift through a helper so the count is a parameter (what `arith.c`/`longshift.c` do;
the call launders the memory count into a clean operand). `tests/diff/cases/longshift.c` exercises the
WORKING variable-shift paths exhaustively (every count 0..width-1 for 16/32/64-bit + rotates + division
edges) and notes the trap so the helpers aren't "simplified" back to inline.

Otherwise the differential suite is clean across integer, pointer, struct/union, function-pointer,
long-long, and **float** code. The prior open bug — **#8 float subtraction** (`10.0-4.0` → `0x40C00182`) —
is **FIXED**: a register/move-ordering bug in `genUminusFloat` (the `-a1` sign-flip inside `__fssub` loaded
the top byte into A before spilling the low word, dropping byte 0 on the HLBA layout). Fix: copy the low
bytes before the sign flip; regression in the subtraction / opposite-sign block of `tests/diff/cases/float.c`.
*(Note: a full cancellation `a-a` yields softfloat `-0.0` where hardware gives `+0.0` — a library zero-sign
convention, not a bug; harmless since `-0.0 == +0.0`.)*

---

## Correctness coverage — keep mining the differential suite (#11)

The highest-value ongoing work. Each new `tests/diff/cases/*.c` (+ a `tests/emu/cases/*.c` for ABI-shaped
behaviour) is run through `corpus-check` + `emu-test` + `diff-test`; the suite has caught several real
**silent** miscompiles that byte-identical assembly never could (struct-arg register-drop, the `cp ba,hl`
pointer-compare peephole gap, the long-long/struct return-ABI off-by-one). Covered: arith, bitfields, calls,
control, longlong, memory, ptrarith, switch, structargs, fnptr2, unions, float — plus the emu ABI cases.
**Still untested (pick any; new territory is also fair game):**

- **#11-libc** — `mem*`/`str*` differential (memcpy / memmove / memset / strcmp / strlen …) run through the lib.
- ✅ **#11-longshift — DONE** (`tests/diff/cases/longshift.c`): exhaustive variable-count shifts (every
  count 0..width-1 for 16/32/64-bit, vs arith.c's 6 samples) + rotates + long-division edges. Found one
  silent miscompile — the 32-bit variable-left-shift IY-counter bug above (**#11-longshift-iy**, deferred;
  the test routes shifts through helpers to cover the working paths).

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
- **✅ #14c — linker cross-module branch relaxation — DONE + default-on (PR #6, 2026-06-09).** Same-bank
  cross-module `bcall`/`bjump` slots drop their `ld nb` bank switch at link time (corpus **−150 B**); opt out
  `SDLD_NO_RELAX=1`. Stage (i) + split-slot reclaim shipped; (ii) iterate-to-fixpoint and (iii) carl→cars
  closed as N/A / not-worth-it (a same-bank disp always fits `carl`, so stage (i) already drops every in-range
  slot; carl→cars is ~1 B for the riskiest emit surgery). `size-check.sh`'s `#14c relax` section tracks the
  reclaim vs `scripts/corpus/relax.baseline`. Design + full history: `banked-branch.md` + git (`7949b4a`,
  `2d7d7c8`, `99375b6`; squashed into `b37bc0d`).

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
  - ⏸ **Remaining (deferred, byte-neutral, low value):** collapse the peephole `jp → jr → jrs` chain to emit
    `jrs` directly and drop the z80 `jr` mnemonic intermediary (`peeph.def`: the 162/163 `jp→jr` rules + the
    `s1c88-j1`/`j2` `jr→jrs` map, plus the 6 direct `jr nc/NZ/z` emits in `gen.c`). **Caveat:** this is NOT
    "let the assembler size it" — `sdas88`'s `jp cc` is the explicit *long* form (`jrl cc`, 3 B) and it does
    **not** relax `jp`→`jrs`; the *peephole* is the short-form sizing (in-range `jp`→`jrs`, 2 B). So the
    collapse must stay **byte-identical** (corpus-check guards it). Cosmetic z80-lineage removal; moderate
    peephole risk for zero bytes — do only as part of a deliberate peephole pass.

  (Done in always-green slices — each byte-identical, run-tests 50/50, smokes green.)

- **(F) MUST NOT touch — shared core / external contract.** `TARGET_Z80_LIKE`, `TARGET_IS_Z80`, `IS_Z80`,
  `ASM_TYPE_Z80ASM` (shared SDCC core — the port DEPENDS on being z80-like, see `CLAUDE.md`) and `sdldz80`
  (the ASxxxx linker-binary / build-script contract). The `@file … derived from the z80 port` provenance
  headers stay as factual lineage.
