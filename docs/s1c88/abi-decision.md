# S1C88 backend: base-port & ABI decision

> **Status:** decided 2026-05-29. Fixes the *target ABI* and the *base port* for sdcc88's S1C88 backend.
> Codegen conformance to this ABI lands in later milestones — the current build is a **z80-flavored
> skeleton** (see *Skeleton divergences* below).
> **Sources:** SDCC 4.5.0 `src/z80`; the Epson C ABI in [c-compiler.md](c-compiler.md); the S1C88
> register/ISA model in [architecture.md](architecture.md) and [instruction-set.md](instruction-set.md).

## Decision

sdcc88's `src/s1c88/` backend is a **standalone clone of SDCC 4.5.0's `z80` port** (was a clone of the
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

This is the ABI sdcc88 *aims* to emit. It is a reference design, not a hard mandate — divergences are
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

## Codegen milestone — decided design

**Register model: Faithful BA+HL** (decided). Reshape the z80 register file to the true S1C88 set:

| Class | Registers | SDCC `*_IDX` (planned) | Notes |
|-------|-----------|------------------------|-------|
| byte GPRs (`num_regs`=4) | `A`, `B`, `L`, `H` | `A_IDX=0, B_IDX, L_IDX, H_IDX` | tree-decomposition allocator handles these 4 |
| condition | carry | `CND_IDX` | bool-in-carry, as z80 |
| GP pairs | `BA`(B:A), `HL`(H:L) | `BA_IDX, HL_IDX` | low-byte-first, adjacent: BA=(A,B), HL=(L,H) |
| index pairs | `IX`, `IY` | `IX_IDX, IY_IDX` | 16-bit, **not** byte-addressable; `(ix+d)`/`(iy+d)` |

Key differences from the z80 base to work through in `ralloc.c`/`ralloc.h`/`ralloc2.cc`/`gen.c`:
- **Drop `C`, `D`, `E` and the `DE` pair** — S1C88 has no DE-equivalent; the z80 codegen uses BC/DE as
  scratch pairs, so those paths must be rewritten to use BA/HL/stack.
- **`A` is the low byte of `BA`** (unlike z80, where A is a standalone accumulator outside all GP pairs).
  The allocator must know that using `BA` clobbers `A` and vice-versa — this is the core `ralloc2.cc`
  rework. BA keeps the low-first/adjacent layout the z80 allocator already assumes (A=0, B=1).
- **Drop `IYL`/`IYH`** — S1C88 `IX`/`IY` are not byte-addressable.

**Asm output: keep SDCC sdas style** (decided). Emit sdas-dialect assembly (`.area`/`.module`, lowercase
mnemonics, `(hl)`/`(ix+d)` addressing) and target the **sdas/sdld** assembler+linker family for the
binary handoff — least divergence from the z80 base, self-contained. S1C88 mnemonics carry over from z80
where they match (`ld`, `add`, `adc`, `sub`, `sbc`, `and`, `or`, `xor`, `inc`, `dec`, `push`, `pop`,
`ret`, `call`, `jp`); S1C88-specific selection (`ba` ops, `jrs`/`jrl`/`cars`/`carl`, `rete`, `[br:ll]`,
`mlt`/`div`, `pack`/`upck`, `ex`, `swap`) replaces the z80-only forms.

**Execution strategy: always-green incremental** (decided after a big-bang attempt). Removing the z80
registers from the model up-front breaks ~1144 `gen.c` sites at once and leaves the compiler unbuildable
for a long, unverifiable stretch — high risk for silent miscompilation with no test suite. Instead:

1. Keep every register symbol (`C/D/E/IYL/IYH_IDX`, `BC/DE_IDX`, `PAIR_BC/PAIR_DE`, the `asmop_*` scratch)
   **defined** so the build never breaks.
2. Constrain the *allocator* to the S1C88 byte regs (A, B, L, H) first.
3. Rewrite the `DE`/`BC` *scratch* uses in `gen.c` function-by-function — building and smoke-testing
   (`sdcc -ms1c88 --c1mode`) after each — mapping onto `BA`/`HL`/`IX`/`IY`/stack (z80 long combos
   `DEHL`/`HLDE` → S1C88 `HL:BA` per the ABI; drop `asmop_iyl`/`iyh` since IX/IY aren't byte-addressable).
4. Delete each register symbol only once it has no remaining uses.

The linchpin is the scratch-asmop machinery near the top of `gen.c` (`asmop_bc/de`, `asmop_dehl/hlde/
hlbc/debc`, `_pairs[]`, the `[IYH_IDX+1]` parm-mask arrays). Note: reordering the `*_IDX` enum so
A,B,L,H are the first four (needed for `num_regs==4`) makes the z80 `BC` pair non-adjacent (B=1, C=4) —
verify `ralloc2.cc`'s pair-adjacency logic tolerates that for the (never-allocated) scratch pairs.

Still deferred within the codegen milestone:
- Epson segment/section names and the generic-pointer tag scheme.
- 3-byte far-pointer code generation and the `_near`/`_far` memory-model story.
- Peephole rules retargeting (replace the z80 `peeph*.def`).
- Single-port cleanup (strip the 9 unregistered z80 PORT structs; entangled with `_parseOptions`).

## Step 2: concrete codegen mapping (z80 pairs → S1C88) — ISA-grounded

> Status: design recorded 2026-05-30 after auditing `instruction-set.md`. Step 1 (allocator constrained
> to A,B,L,H, commit `b606833`) is done; this section is the executable plan for the `gen.c` reshape.

### What the S1C88 actually gives us (instruction-set.md §"16-bit Transfer/Arithmetic")
- **Four 16-bit registers — fully orthogonal for *transfer*:** `BA, HL, IX, IY` are mutually loadable
  (`LD dst,src` for every pair, 2 B / 2 cyc), and every one loads `#imm`, `[hhll]`, `[HL]`, `[IX]`,
  `[IY]`, `[SP+dd]` and stores back. 16-bit moves are therefore cheap and uniform.
- **Two 16-bit *ALU* pairs:** only `BA` and `HL` have the full `ADD/ADC/SUB/SBC/CP` cross-product (with
  each other, with IX/IY, and `#imm`), and only they are **byte-addressable** (A/B, L/H). This mirrors the
  z80 exactly, where `HL` and `DE` are the two 16-bit ALU pairs.
- **`IX`/`IY` are index-only:** 16-bit, **not byte-addressable** (there is no IXL/IXH/IYL/IYH), and they
  have *limited* arithmetic — `ADD/SUB IX,{BA,HL,#imm}`, `CP IX,#imm`, `INC/DEC` — but **no `ADC`/`SBC`**,
  so they can't carry-chain. Good for pointers/frame, not for multi-precision math.
- **`EX` always pivots through BA:** `EX BA,HL` / `EX BA,IX` / `EX BA,IY` / `EX BA,SP` (+ 8-bit `EX A,B`,
  `EX A,[HL]`). There is **no `EX HL,IX`**, etc. z80's workhorse `ex de,hl` → `ex ba,hl`.
- **`A` is *both* the accumulator and BA's low byte; `B` is BA's high byte.**

### The mapping

| z80 | → S1C88 | byte-addressable | notes |
|-----|---------|------------------|-------|
| `HL` | `HL` | yes (H,L) | DIRECT — primary pointer + ALU pair |
| `DE` | `BA` | yes (B,A) | the 2nd ALU pair. `ex de,hl`→`ex ba,hl`; `add/adc/sbc hl,de`→`…,ba` |
| `BC` | `IX`/`IY`/stack | **no** | 3rd "pair" → index reg or spill; byte-level B/C uses eliminated |
| `A`,`B`,`H`,`L` | `A`,`B`,`H`,`L` | — | DIRECT (the Step-1 allocatable byte set) |
| `C`,`D`,`E` | **(eliminated)** | — | no S1C88 byte home — restructure to A/B/L/H/16-bit/stack |
| `IYL`/`IYH` | **(dropped)** | — | IX/IY are not byte-addressable |
| `IX` (frame ptr) | `IX` | no | keep as frame pointer |

### Why it is NOT a textual rename — the two hazards
1. **A/BA overlap.** `DE→BA` relocates z80 `E`→`A`, `D`→`B`. Any z80 site holding *independent* live
   values in `A` (accumulator) and `E`/`D` (DE bytes) collapses them — e.g.
   `ld a,#5; ld e,#3; add a,e` would become `ld a,#5; ld a,#3; add a,a` (miscompile). The relocation is
   valid only where `A` is dead across the DE lifetime; `isPairDead(PAIR_BA)` must now test **both A and B**.
2. **No home for C/D/E bytes.** S1C88 has 4 byte regs (A,B,L,H); z80 uses 7. C/D/E cannot be renamed
   (D→B / E→A collide with z80's own live A/B) — their *byte* uses must be restructured away.

### Tactic — retarget the central pair abstraction, then mop up direct byte sites
The 335 `PAIR_DE` / 154 `PAIR_BC` references mostly flow through a few helpers; fix the abstraction once
and most call sites follow. Keep every old symbol *defined* throughout (always-green):
1. **`_pairs[]` + `PAIR_ID`**: make the 2nd ALU pair emit `"ba"` with l_idx=`A_IDX`, h_idx=`B_IDX`
   (introduce `PAIR_BA`; keep `PAIR_DE` as a transition alias). 3rd pair → index (`PAIR_IX`/`PAIR_IY`).
2. **byte-selection ternaries** (`pairId==PAIR_DE ? ASMOP_E : ASMOP_C`, dense in `fetchPairLong`,
   `getPairId`, `setupPairFromSP`, the push/pop paths): re-point to BA's bytes (`ASMOP_A`/`ASMOP_B`);
   teach `isPairDead`/`isPairInUse` that BA = {A,B}.
3. **EX + 16-bit moves**: `ex de,hl`→`ex ba,hl`; lean on the orthogonal `LD`s for the rest.
4. **ABI `aopRet`/`aopArg`** — **DONE for the byte-addressable registers** (commits `d89db99`,
   `498ad12`, `482f23b`). Returns: char→`A`, int→`BA`, long/float→`HL:BA` (faithful). Arguments use the
   **faithful Epson register-priority *consumption* model** — see "Argument ABI" below.
5. **direct byte C/D/E sites** (`countreg` picks, `isRegIdxPair`, the genByte ALU loops): eliminate
   per-site → A/B/L/H/IX/IY/stack. Build + run the meter (below) after each batch; delete a symbol only
   once it has zero uses.

### Argument ABI — faithful Epson order (decided 2026-05-31)

Decided to follow the Epson scheme faithfully (c-compiler.md §1.2.15/§1.3.2), **not** a minimal z80-slot
swap. Arguments are assigned by **descending priority per type**, sharing the byte-register file (placing
a value in `BA` consumes `A` and `B`; in `HL` consumes `L` and `H`):

| Type | Priority (high → low) |
|------|-----------------------|
| `char` | `A`, `L`, `YP`, `XP`, `H`, `B` |
| `int`/`short` | `BA`, `HL`, `IX`, `IY` |
| `long`/`float` | `HLBA`, `IYIX` |
| near ptr | `IY`, `IX`, `HL`, `BA` |
| far ptr | `IYP`, `IXP`, `HLP` (`IYP=IY+YP`, `IXP=IX+XP`, `HLP=HL+A`) |

`aopArg` became a stateless **consumption allocator** (`aopArgRegS1C88` in gen.c): it replays args 1..i,
tracking which byte regs (A,B,L,H) are consumed, and returns the first free register from arg i's priority
list. The A/BA overlap is handled by construction (once `A` is taken, no char picks it; once `L` is taken,
`HL` is unavailable). Both caller (`genSend`) and callee (`genReceive`) are data-driven off `aopArg`.

**Executed always-green in phases:**
- **Phase 1 — DONE (`482f23b`):** the byte-addressable registers only — char `A,L,H,B`; int `BA,HL`;
  near-ptr `HL,BA`; long `HLBA`. Anything the Epson ABI would place in `IX/IY/YP/XP` (and any overflow)
  goes on the **stack** for now. Verified: `f2(int,int)`→BA,HL; `fc(char,int)`→A,HL; `f3(int,int,int)`→3rd
  on stack; caller/callee agree.
- **Phase 2 — next:** add `IX`,`IY` (int 3rd/4th, near-ptr 1st/2nd), `IYIX` (long) + the S1C88
  index-register move codegen (`ASMOP_IX`, `ld ix,hl`, non-byte-addressable handling).
- **Phase 3 — later (with the deferred far-pointer/page-register work):** char `YP,XP`; far-ptr
  `IYP/IXP/HLP` and far-ptr return `HLP`.

Until phases 2-3 land the ABI diverges from Epson for args that need IX/IY/YP/XP (they're stacked / use a
lower-priority byte reg). This is safe — the convention is sdcc88-internal (caller+callee both read
`aopArg`); there is no Epson-object interop yet.

### Verification meter (interim, until the assembler validates output)
`scripts/check-s1c88.sh` scans emitted asm for z80-only residue (`\bde\b`, `\bbc\b`, `ex de,hl`, `iyl`,
`iyh`, z80-only mnemonics) and prints a count — a cheap progress/regression signal for Steps 2–3. It
catches wrong *register names*, not wrong *encodings/flags/sizes* — the assembler validator (below) does.

## Toolchain & validator: target sdas / sdld (decided 2026-05-31)

**Decision:** sdcc88's binary handoff is **SDCC's own `sdas`/`sdld`** (its in-tree fork of the ASxxxx
cross-assembler suite — the toolchain almost every SDCC port uses, and the dialect sdcc88 already emits).
**We retarget them for the S1C88** rather than bridging to an external assembler. `../skiploom` (AS88) is
*not* the toolchain; its opcode table (`src/util/s1c88.csv`) stays only an independent ISA cross-check
(per `CLAUDE.md`).

Concretely:
- **Assembler — add an `sdas/as88/` backend** modeled on `build/sdcc-4.5.0/sdas/asz80/` (`z80.h`,
  `z80adr.c` addressing modes, `z80mch.c` the opcode/encoding table, `z80pst.c` mnemonics). This holds the
  real S1C88 encodings (incl. the `CE`/`CF` 2nd-page prefix scheme; see `instruction-set.md` App. A and
  the skiploom CSV as cross-check). Built incrementally — start with the instruction subset the codegen
  currently emits and grow it alongside the codegen.
- **Linker — `sdld`** is largely architecture-independent (the `.rel` relocatable format is generic);
  retarget is mostly registering the S1C88 target + any arch-specific bits.
- **The `as88` IS the validator:** assemble the emitted `.asm`/`.src` as-is (sdas syntax — no bridge),
  wired into `scripts/dev.sh`/`check-s1c88.sh` so every build proves the output is legal S1C88 and catches
  encoding/size/flag mistakes the meter can't.

Note: the in-compiler `z80instructionSize()` (peep.c) + peephole `.def` rules carry their own S1C88
instruction knowledge and must be taught the S1C88 forms separately — independent of which assembler
consumes the output.

**Feasibility — PROVEN (2026-05-31).** The sdas build works in our sandbox: `scripts/build-sdas.sh asz80`
generates the backend Makefile (`config.status --file=`) and builds `bin/sdasz80` against the shared
`sdas/asxxsrc/` core; it assembles `.asm` → `.rel` with correct encodings (`ld a,#5; inc hl; ret` →
`3E 05 23 C9`). So the whole validator/toolchain path is viable — no blockers. A backend is **~3000 lines**
(`z80.h` 275, `z80adr.c` 296 addressing, `z80pst.c` 496 mnemonic table, `z80mch.c` 2157 encoder).

**Building `as88` (next):** create `sdas/as88/` (`s1c88.h`, `s1c88adr.c`, `s1c88pst.c`, `s1c88mch.c`,
`Makefile.in`) modeled on `asz80`, with S1C88 encodings from `instruction-set.md` (App. A opcode map +
the `CE`/`CF` 2nd-page prefixes; skiploom CSV cross-check). Build incrementally — cover the instruction
subset the codegen emits first (ld/add/adc/sub/sbc/inc/dec/push/pop/ret/call/jp/jr/ex/cp + the 16-bit
ops), wire into `check-s1c88.sh` to assemble the smoke output, then grow it as codegen emits more.
`build-sdas.sh` already handles building a config-unknown backend (derives its Makefile from asz80).
