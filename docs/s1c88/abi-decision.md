# S1C88 backend: base-port & ABI decision

> **Status:** decided 2026-05-29. Fixes the *target ABI* and the *base port* for sdcc88's S1C88 backend.
> This doc is the design + the always-green retarget plan; **Step 2 is the live worklist.** For the current
> implementation state and next action see **[HANDOFF.md](HANDOFF.md)** — as of the latest slices the call
> ABI, frame, branches, compares, 16-bit ALU, `adjustStack`, and shifts are done; the C/D/E + DE/BC
> register-model cleanup remains. The codegen validator (`sdas88`, referenced below as "the assembler") is
> built and complete.
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

The port keeps SDCC's z80 register table/`*_IDX` ordinals (the Boost allocator in `ralloc2.cc` is
hard-keyed to them), and the allocator is constrained to the S1C88 byte set `A/B/L/H` with `PAIR_BA` added
as a first-class pair. The table below is the logical mapping the retarget realizes incrementally; the
`C/D/E` byte regs and `DE`/`BC` scratch pairs are the remaining cleanup (Step 2).

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

## Divergences from the target ABI (progress)

This section started as the "skeleton divergences" list; it now tracks which have been resolved as the
codegen retarget proceeds. (See [HANDOFF.md](HANDOFF.md) for the live per-slice state.)

1. **z80 register names & instruction selection** in emitted asm — **largely resolved.** Frame, branches,
   compares, 16-bit ALU, `adjustStack`, 8-bit L/H ALU operands, and shifts emit native S1C88. Remaining:
   the `C/D/E` byte regs + `DE`/`BC` scratch pairs (the register-model grind, Step 2 below).
2. **Pointer sizes:** still z80's 2-byte far/generic pointers, not the Epson 3-byte `_far` pointer
   (changing `gptr`/`farptr` size touches `gen.c` pointer codegen). **Deferred.**
3. **Argument/return registers** — **DONE for the byte-addressable set**: returns BA/HL:BA, args use the
   faithful Epson register-priority order (see *Argument ABI* below); IX/IY/page-reg args are the deferred
   Phase 2/3.
4. **`IX`/`IY`** retain z80's allocation behavior rather than the S1C88-correct "index-only, not
   GP-allocated" treatment. **Deferred** (folds into the register-model grind).
5. **Single-port cleanup** — **DONE**: the unregistered z80 variant PORT structs were pruned from `main.c`.
6. **Toolchain handoff** — **DONE**: not the Epson `as88`/`lk88`, but SDCC's own `sdas`/`sdld` retargeted
   to the S1C88 (`sdas88` + `sdldz80` + `romgen.py`); see the Toolchain section below.

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
- **Phase 2 — DONE (s19, with IX excluded by the frame prologue):** int = BA, HL, IY; near-ptr =
  HL, BA, IY; IYIX longs deferred.
- **Phase 3 — CLOSED as a documented divergence (decided 2026-06-06, won't do):** char `YP,XP`
  args, far-ptr `IYP/IXP/HLP` args, `IYIX` longs. Rationale: caller+callee already agree via the
  stack (no correctness gap and no Epson-object interop exists to match); the allocator has no
  page-register model; and receiving a far pointer's page in EP would collide with the
  load-bearing **EP=0 invariant** on every function entry — Epson-faithful register far-ptr args
  are actively hazardous in this codegen model, not merely unimplemented. Far-ptr *returns* remain
  Epson-faithful (HLA = Epson HLP).

The ABI therefore permanently diverges from Epson for args that would need IX/YP/XP/page registers
(they're stacked). This is safe — the convention is sdcc88-internal (caller+callee both read
`aopArg`); there is no Epson-object interop.

## Task #9: 3-byte far pointers + the `_near`/`_far` memory model (design decided 2026-06-05)

The S1C88 data-memory paging is **linear**: a data access is physically `page×65536 + offset16`
(EP prefixes `[HL]` and absolute `[hhll]`; XP/YP prefix `[IX]`/`[IY]` — memory-model.md §2.4.2).
That makes the far-pointer representation trivial:

- **A far pointer is a 24-bit linear physical data address**, stored little-endian in 3 bytes:
  `[0]=addr&0xFF, [1]=(addr>>8)&0xFF, [2]=addr>>16` — byte 2 **is** the EP page value, bytes 0–1
  are the 16-bit offset. SDCCglue's stock 3-byte emission `name, (name>>8), (name>>16)` is therefore
  natively correct, and far-pointer arithmetic is plain 24-bit integer arithmetic (pages are
  64K-aligned and contiguous).
- **C surface:** `__far` (port keyword "far" → S_XDATA → the `xdata` memmap → pointer class
  FPOINTER, `fptr_size = 3`). `__near` = explicit S_DATA. Unqualified pointers stay GPOINTER,
  **2 bytes, untagged, == near** (no runtime-tagged generic pointers). far→near/generic casts
  truncate to the offset; near→far widens with page 0 — correct, because the near space *is*
  physical `0x000000–0x00FFFF` (page 0).

**The EP=0 invariant.** All near codegen (absolute `[hhll]`, indirect `[HL]`) implicitly assumes
EP=0 (the small-model convention). Therefore every far access sequence must leave EP zero:
`ld a,<page>; ld ep,a; …access via [hl]…; ld ep,#0` — the immediate restore form (`CE C5 00`)
does not touch A, so it can run after a read lands its result in A. XP/YP are never touched
(far accesses route through HL+EP only). ISRs must preserve the invariant against interrupting
mid-sequence: the ISR prologue saves EP and zeroes it (`ld a,ep; push a; ld ep,#0`), the epilogue
restores (`pop a; ld ep,a`) — EP is not part of the hardware exception save (only CB:PC and SC are).

**Codegen idioms:**
- far read: `<page>→A; ld ep,a; <offset>→HL; ld a,[hl]` (+ `inc hl` walk for multi-byte), `ld ep,#0`.
- far write: page→EP as above, value through A/B, `ld [hl],a`, `ld ep,#0`.
- genCast near→far: copy 2 bytes + `#0x00` page; far→near: copy bytes 0–1.
- 3-byte ALU (p++, p+n, ==, <): the generic byte-wise paths; 24-bit linear add/compare is correct.

**Far objects are const ROM data.** On the Pokémon Mini all RAM (4K) is near; the only far *objects*
are ROM tables. `__far` objects are emitted as a **romable static segment** (area `_FAR`,
`emitStaticSeg` semantics — initializer bytes inline, never GSINIT runtime stores). Non-const
`__far` RAM objects are unsupported (documented; there is no far RAM on this target). Writes
*through* far pointers remain legal codegen (the target may map far RAM on other S1C88 chips).

**ABI:**
- far-ptr argument passing: **stack** (documented divergence from Epson `IYP/IXP/HLP` — our
  allocator has no page-register model; caller+callee agree via `aopArg`, no interop concern).
- far-ptr return: **offset in HL, page in A** — i.e. bytes (L, H, A) — faithful to Epson `HLP`.

**Shared-glue patch points** (these live in upstream files, so they extend
`third_party/sdcc/register_s1c88_port.patch`; all are gated so other ports in the same driver are
byte-identical):
1. `SDCCglue.c glue()`: the xdata oBuf is only written for mcs51-like/mos6502 — gate in S1C88.
2. `SDCCglue.c emitMaps()`: route s1c88's xdata through `emitStaticSeg` (ROM const data), not
   `emitRegularMap` (which emits `.ds` + GSINIT runtime-init stores — wrong twice for far ROM).
3. `SDCCglue.c printIvalPtr()` symbol path: `size==FARPTRSIZE` must emit 3 real bytes when
   FARPTRSIZE==3 (`use_dw_for_init` short-circuits it to `.dw`, and `printPointerType` hardcodes 2);
   a 2-byte pointer must NOT fall into the `GPTRSIZE` branch's tag-byte emission (it emitted
   `lo,hi,#0x40` = 3 bytes into a `.ds 2` slot once FARPTRSIZE stopped masking it).
4. `printIvalPtr()` literal `case 3`: an FPOINTER literal's third byte must be `aopLiteral(val,2)`
   (the real page), not `pointerTypeToGPByte` (the mcs51 generic-pointer *tag* — page 0 for us).

**Link story (deferred within #9):** the page byte of a link-time symbol is `(sym >> 16)`; that
needs far areas located at physical 24-bit addresses and byte-3 extraction relocs through the
ASxxxx 16-bit-address pipeline — likely an `R_S1C88_BANK`-style reloc on the third byte, exactly
like the `bcall` bank slot. ~~Far *code* pointers (banked indirect calls) are out of #9's scope.~~
**DONE (2026-06-06):** function pointers are 3-byte banked code pointers — see "The call model:
MAXIMUM mode". **Far bit-fields are DONE too** (genFarUnpackBits/genFarPackBits: raw bytes via
HL+EP, mask/shift/sign at EP=0, the genPackBits and/or-mask merge against `(hl)` under EP;
non-literal values stage at EP=0 before the pointer claims HLA, multi-byte ones carried across on
the stack — `pop ba` is SP-paged, near-safe under EP).

## Native DIV (decided + implemented 2026-06-06, sessions: commits `1da6979`, `9cf0371`)

The S1C88 `DIV` (`CE D9`, 2 B / 13 cyc, MODEL1/3 — present on the Pokémon Mini core) computes the
**unsigned** `HL ÷ A` → quotient `L`, remainder `H`. `V` is set (and HL kept) when the quotient
exceeds 8 bits; `A = 0` raises the **hardware zero-division exception** (C UB — a program dividing
by zero vectors through exception processing instead of returning garbage like the old support
call; documented, acceptable).

**What's claimed natively** (`_hasNativeMulFor`, which SDCCopt consults for `'/'`/`'%'` too —
unclaimed shapes keep the `__div*`/`__mod*` support calls, so claims must exactly match codegen):
- **unsigned 8 ÷ 8** (both `unsigned char`, or a literal divisor 1..255): one `DIV` — quotient and
  remainder always fit, `V` never set. `ld l,<dividend>; ld a,<divisor>; ld h,#0; div`.
- **unsigned 16 ÷ 8** (unsigned ≤16-bit dividend, same divisor classes): the **two-DIV schoolbook
  base-256 chain** — `ld b,l; ld l,h; ld h,#0; div` (L=qhi, H=r), `[push l;] ld l,b; div [; pop h]`
  (L=qlo, H=remainder; after `pop h`, HL = the full quotient). Both partial quotients provably fit
  (the running remainder is < the divisor ≤ 255); `DIV` preserves A (the divisor) between steps.
  The qhi `push/pop` is skipped for `'%'` and 1-byte-quotient results. **C promotion caveat:**
  `u16 / u8var` promotes the divisor to `unsigned int`, so the *variable*-divisor 16÷8 only claims
  when the middle end narrows it back — in practice the 16-bit claims are **literal** divisors
  (`x/10`, `x%10` — the binary-to-decimal workhorse, previously a `__divuint` call each).
- **signed 8 ÷ 8** (both `signed char`; NOT under `--opt-code-size` — the inline cluster is
  ~26 bytes vs a 6-byte bcall): branchless C-truncation via the sep sign-mask identity
  `|x| = (x^m) - m` (SEP: B ← sign of A), unsigned DIV on the magnitudes, the result's mask
  (`m(dividend)^m(divisor)` for `/`, `m(dividend)` for `%`) re-applied the same way. Wide results
  sep-extend into BA. B is the mask home (byte-granular save when live).
- **Not claimed:** 16-bit divisors, 32-bit anything; signed literal divisors never fire in
  practice (the middle end widens `sc/10` to int before consulting the hook).

**Codegen contract** (`genDivMod` in gen.c): staging is clobber-ordered (divisor-first when its
home is L/H or already A; dividend-first + a stack bounce — `push a/l/h … pop l`, or `push hl …
pop hl` for the 16-bit path — when the divisor read `requiresHL`); a live non-operand A gets the
byte-granular `push a` save (the genMultOneChar scheme), and the 16-bit chain saves a live B the
same way (`_G.stack.pushed` keeps SP-relative operand math right across the saves). `HLinst_ok`
(ralloc2.cc) treats `'/'`/`'%'` like `'*'` — DIV clobbers HL, so the allocator keeps live
non-operand values out of HL across it; both ops run the exact-cost dry-run path. peep.c: `div`
sizes 2, reads no flags, surely writes Z/N/C/V. Corpus: `scripts/corpus/20_div.c`.

`genDiv`/`genMod` (the old "handled through support calls" wasserts) both route to `genDivMod`.

## The call model: MAXIMUM mode (decided + fixed 2026-06-06 — load-bearing, read before touching calls)

**The Pokémon Mini runs the S1C88 in MAXIMUM mode: every call pushes a 3-byte `PCL PCH CB` return
frame and `RET` pops all three, restoring the caller's bank** (Epson instruction-set p.58,
memory-model §225–251; PokeMini pushes `PC.B.I`+`PCH`+`PCL` on every CALL and pops 3 on RET — min
mode pins the bank window, which a 2MB banked ROM cannot use). The linker's `bcall`/`bjump` design
always depended on this (no compiler-side bank restore); codegen however had inherited the z80
2-byte frame and was wrong everywhere until commit `89efb3b`. The model:

- **`call_overhead` = 5** (3-byte frame + 2-byte saved IX): stacked args start at `ix+5` /
  `sp+3-at-entry`.
- **The CALLER cleans up stack parameters** (`isFuncCalleeStackCleanup` returns false by default):
  only RET can consume a 3-byte frame — no `pop hl … jp hl` epilogue trick can restore CB.
  Explicit `__z88dk_callee` still works via one universal epilogue that moves the 3-byte frame up
  over the parameter area byte-by-byte (CB first; A/HL saved when they carry return bytes).
- **Indirect calls** (`PCALL`): the target's 16-bit offset goes into the **`__sdcc_fptr` scratch
  cell** (2 bytes of near RAM — a runtime-provided symbol like the `__div`/`__mul` support
  routines), the bank into NB, then the native indirect **`call (hhll)`** (FB) — the hardware
  builds and restores the full frame. Tail positions use `ld nb, a` + `jp hl` (reuses the caller's
  own frame). **NB-window discipline: at most ONE instruction between `ld nb` and the consuming
  branch** (the linker's own `ld nb ; nop ; carl` shape; the Minx blacks out interrupt acceptance
  for exactly that shadow — PokeMini `Shift_U`). Not reentrant against an ISR that itself makes an
  indirect call between the cell store and the call.
- **Function pointers are 3 bytes: (lo, hi, bank)** (`funcptr_size = 3`). Code symbols link as
  `(bank<<16)|logic`, so `&f`'s third byte — the stock `(sym >> 16)` emission, via the #9 XL3
  byte relocs — IS the bank; `printIvalFuncPtr` emits all three bytes (patch-gated alongside
  stm8). Casting a 2-byte data pointer widens with bank 0 (the common bank). End-to-end verified:
  linking with `_CODE=0x028000` puts `00 80 02` in an fptr initializer.
- **ISRs were already correct**: the interrupt sequence pushes `CB → PCH → PCL → SC` and RETE pops
  them all (and NB←CB), so the s10 RETE model needed no change.

### Verification meters
The **primary validator is `sdas88`**: `scripts/validate-s1c88.sh <file.asm>` assembles emitted codegen and
freq-ranks any form the S1C88 can't encode — catching wrong encodings/flags/sizes/register-classes, i.e.
the actual remaining z80-isms. (The older textual meter `scripts/check-s1c88.sh` — grepping for `\bde\b`,
`\bbc\b`, `iyl`, etc. — is now a secondary signal; it catches wrong register *names* but not encodings.)

## Toolchain & validator: target sdas / sdld (decided 2026-05-31)

**Decision:** sdcc88's binary handoff is **SDCC's own `sdas`/`sdld`** (its in-tree fork of the ASxxxx
cross-assembler suite — the toolchain almost every SDCC port uses, and the dialect sdcc88 already emits).
**We retarget them for the S1C88** rather than bridging to an external assembler.

Concretely:
- **Assembler — add an `sdas/as88/` backend** modeled on `build/sdcc-4.5.0/sdas/asz80/` (`z80.h`,
  `z80adr.c` addressing modes, `z80mch.c` the opcode/encoding table, `z80pst.c` mnemonics). This holds the
  real S1C88 encodings (incl. the `CE`/`CF` 2nd-page prefix scheme; see `instruction-set.md` App. A).
  Built incrementally — start with the instruction subset the codegen currently emits and grow it
  alongside the codegen.
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

**Building `as88` — DONE.** `sdas/as88/` (`s1c88.h`, `s1c88adr.c`, `s1c88pst.c`, `s1c88mch.c`,
`Makefile.in`, modeled on `asz80`, with S1C88 encodings from `instruction-set.md` App. A + the `CE`/`CF`
2nd-page prefixes) is built by `scripts/build-sdas.sh as88` → `bin/sdas88`, covers the full practical ISA
byte-verified, and is wired into `scripts/validate-s1c88.sh` as the codegen validator. The banked-branch
extensions (`bcall`/`bjump`, the `R_S1C88_BANK` relocation) live in `s1c88_banked_branch.patch`; the
linker (`scripts/build-sdld.sh` → `sdldz80`) + `romgen.py` complete the assemble→link→`.min` pipeline. See
[sdas88-retarget.md](sdas88-retarget.md) and [banked-branch.md](banked-branch.md).
