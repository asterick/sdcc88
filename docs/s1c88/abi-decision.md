# S1C88 backend: base-port & ABI decision

> **Status:** decided 2026-05-29. Fixes the *target ABI* and the *base port* for skip-c's S1C88 backend.
> Codegen conformance to this ABI lands in later milestones — the current build is a **z80-flavored
> skeleton** (see *Skeleton divergences* below).
> **Sources:** SDCC 4.5.0 `src/z80`; the Epson C ABI in [c-compiler.md](c-compiler.md); the S1C88
> register/ISA model in [architecture.md](architecture.md) and [instruction-set.md](instruction-set.md).

## Decision

skip-c's `src/s1c88/` backend is a **standalone clone of SDCC 4.5.0's `z80` port** (was a clone of the
`stm8` port). Rationale: the z80 register/allocator model is the closest match to the S1C88 —

- a **carry flag** as a condition register,
- **byte-addressable 16-bit register pairs** (`BC/DE/HL` ↔ S1C88 `BA`/`HL`),
- **non-byte-addressable index registers `IX`/`IY` with `(ix+d)`/`(iy+d)` displacement addressing** —
  exactly the S1C88 `IX`/`IY` model, which STM8's *byte-addressable* `X`/`Y` index registers cannot
  represent.

The estimated retargeting effort is ~40% lower than continuing from the STM8 clone, and the z80 codegen
idioms (`ld`, `add`/`adc`, `push`/`pop` of pairs, `jp`/`jr`/`call`/`ret`, `inc`/`dec`, indexed
addressing) map directly onto the S1C88 ISA.

The build scaffolding is unchanged: `build.sh` overlays `src/s1c88/` and applies
`third_party/sdcc/register_s1c88_port.patch`. Because the configure step already disables **all** z80
variants, the clone cannot collide with z80 globals. The patch was extended to add **`TARGET_IS_S1C88`
to `TARGET_Z80_LIKE`** so SDCC's core (SDCCopt/SDCCicode/SDCCsymt/SDCCglue/SDCClrange/SDCCast/SDCCpeeph)
treats the port as z80-like — the z80 codegen depends on this.

## z80 → S1C88 register mapping (intended)

The skeleton keeps SDCC's z80 register table/`*_IDX` ordinals **verbatim** (the Boost allocator in
`ralloc2.cc` is hard-keyed to them). The table below is the *intended* logical mapping that the codegen
milestone will realize; until then the emitted asm uses z80 register names.

| SDCC IDX (kept) | z80 reg | → S1C88 reg | Role |
|-----------------|---------|-------------|------|
| `A_IDX` | `a` | **`A`** | 8-bit accumulator |
| `B_IDX` / `C_IDX` | `b` / `c` | **`B`** + accumulator → **`BA`** pair | 2nd 8-bit / 16-bit ALU pair `BA = B:A` |
| `D_IDX` / `E_IDX` | `d` / `e` | working bytes (no direct S1C88 `DE`) | scratch; spilled / synthesized |
| `L_IDX` / `H_IDX` | `l` / `h` | **`L` / `H`** → **`HL`** | byte-addressable pair; primary pointer (`[HL]`) |
| `IYL_IDX`/`IYH_IDX`, `IY_IDX` | `iy` | **`IY`** | 16-bit index, `[IY+d]`; not byte-addressable |
| (z80 `IX`, codegen-reserved) | `ix` | **`IX`** | 16-bit index, `[IX+d]`; frame pointer candidate |
| `CND_IDX` | carry | **carry (in `SC`)** | condition flag |
| — | `sp` | **`SP`** | stack pointer |

S1C88 has fewer general 16-bit pairs than z80 (`BA`,`HL` vs `BC`,`DE`,`HL`); `D`/`E` have no direct
S1C88 home and will be handled as scratch/stack in the codegen milestone. S1C88 `IX`/`IY` (non-byte-
addressable) align with z80's `IX`/`IY` index-register treatment.

## Target C ABI (from the Epson reference, [c-compiler.md](c-compiler.md))

This is the ABI skip-c *aims* to emit. It is a reference design, not a hard mandate — divergences are
allowed where they simplify the backend, as long as they're documented.

- **Endianness:** little-endian.
- **Type sizes:** `char` 1, `short`/`int` 2, `long` 4, `long long` 8, `float`/`double` 4 (single
  precision). `_near` pointer **2 B** (16-bit), `_far` pointer **3 B** (24-bit, includes page/bank).
- **Argument passing:** in registers first, overflow + struct/union + varargs on the stack (pushed in
  reverse). `char`→`A`; `int`/`short`→`BA, HL, IX, IY`; `long`→register pairs (`HLBA, IYIX`); `_near`
  ptr→`IY, IX, HL, BA`; `_far` ptr→`IYP, IXP, HLP`.
- **Return values:** `char`→`A`; `int`/`short`→`BA`; `long`→`HLBA` (HL high : BA low); pointer→`HLP`;
  `float`→`HLBA`; struct/union→stack.
- **Caller/callee-saved:** caller-saves (Epson reference). The z80 clone's `sdcccall` convention and its
  `calleeSavesBC` option differ — the codegen milestone reconciles which we adopt.
- **Interrupts:** interrupt functions save clobbered registers and return via **`RETE`**; 2-byte vector
  entries.

## Skeleton divergences (deferred to later milestones)

The current build is a **building skeleton** — `./build.sh` succeeds, `sdcc -ms1c88` is selectable, and
`sdcc -ms1c88 -S` runs — but its **code generation is still z80**. Known divergences from the target ABI,
each deferred:

1. **z80 register names & instruction selection** in emitted asm — the whole point of the next milestone.
2. **Pointer sizes:** skeleton keeps z80's 2-byte far/generic pointers, not the Epson 3-byte `_far`
   pointer (changing `gptr`/`farptr` size touches `gen.c` pointer codegen).
3. **Argument/return registers** follow z80's `sdcccall` assignment, not the Epson order above.
4. **`IX`/`IY`** retain z80's allocation behavior in the skeleton (`OPTRALLOC_IY` as in z80) rather than
   the S1C88-correct "index-only, not GP-allocated" treatment.
5. **All 10 z80 PORT structs are retained** in `main.c` (only `s1c88_port` is registered) and the z80
   variant peephole `.def` files are shipped — a follow-up cleanup will strip to a single port.
6. **Segment/section names, assembler/linker command strings** are placeholder z80 values; the real Epson
   `as88`/`lk88`/`lc88` handoff (see [toolchain.md](toolchain.md)) is a separate open decision.

## Open questions for the codegen milestone

- The true `s1c88_regs[]`/`*_IDX` reshape and the corresponding `ralloc2.cc` rework.
- Epson segment/section names ([assembler.md](assembler.md)) and the generic-pointer tag scheme.
- 3-byte far-pointer code generation and the memory-model (`_near`/`_far`) story.
- S1C88 instruction selection, mnemonics, and peephole rules (replace the z80 ones).
- Real assembler/linker handoff (Epson `as88` text, or skiploom — see [toolchain.md](toolchain.md)).
