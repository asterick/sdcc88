# Banked `CALL` / `JUMP` — assembler + linker design (Phase 2)

> **Status: AUTO-BANK WORKS.** `bcall`/`bjump` are implemented end-to-end. Each emits the slot
> `ld nb,#bb ; nop ; carl|jrl [cc,] target`, the displacement links across areas/banks via the standard
> `R_PCR` (the 16-bit write masks the bank difference → logic-relative disp), **and the linker now
> resolves and writes the destination bank automatically.** Verified: with code areas placed at
> `(bank<<16)|logic`, `bcall` to a bank-1 target links to `CE C4 01 FF F2 <disp>` (ld nb,#1 ; nop ; carl),
> while a same-bank / bank-0 target links to `FF FF FF FF F3 <disp>` (the whole `ld nb` NOP'd, no switch).
>
> **How it's built** (shared-source changes persisted in `third_party/sdcc/s1c88_banked_branch.patch`,
> applied idempotently by `build-sdas.sh`/`build-sdld.sh`):
> - A **`TARGET_ID_S1C88`** identity in `sdas.c`/`sdas.h` (binary "sdas88" matches the "88" table entry)
>   routes word relocations through the **escape path** in `asout.c` — needed because the z80 16-bit reloc
>   path *truncates* escape modes (the bank mode `0x800` → `0x00`).
> - `bcall`/`bjump` emit the bank as a **2-byte `[bank][pad]` field via `outrw`** (which emits exactly 2
>   bytes and advances the location counter by 2 — `outrb` would over-emit `a_bytes`=4 bytes and desync
>   the counter), carrying **`R_S1C88_BANK` (0x800)**.
> - `lkrloc3.c` gets a dispatch case: write `symval >> 16` (the target's bank) into `[bank]` and `0xFF`
>   (nop) into `[pad]`, or NOP the whole `ld nb`+pad when the bank is 0 or the current area's. **Key
>   gotcha:** *do not clear* `rtflg[]` for these bytes — `rtflg[i]` is lkout's "emit this byte" flag, so
>   clearing it drops the bytes from the output.
>
> **Remaining:** the bank `.lk` (areas at `(bank<<16)|logic`) and romgen (bank slices → flat `.min`).
> Current cost: a `nop` pad per call (the 2-byte field) and always-long form — a later optimization can
> reclaim those via the linker also choosing `cars`/`jrs`.
>
> This is the concrete plan for two **pseudo-instructions** that pick the short/long branch form *and*
> insert the code-bank switch automatically:
>
> ```
> CALL  target [, cc]      ; -> [ld nb,#bank] cars|carl [cc,] target
> JUMP  target [, cc]      ; -> [ld nb,#bank] jrs |jrl [cc,] target
> ```
>
> `cc` ∈ { (none), `C`, `NC`, `Z`, `NZ` } — the conditions that have **both** a short and a long form.
> (The CE-page *signed* conditions `LT`/`GE`/… are **short-only**; they're handled by `genCmp`'s
> `jrs LT/GE` path, not by these pseudo-ops. See "Constraints".)

---

## 1. The S1C88 code-bank model (recap — full detail in `memory-model.md`)

- Program memory = **32 KB banks**, addressed by **`CB:PC`**. `physical = (CB << 15) | (PC & 0x7FFF)`.
- **Common area** = logic `0x0000–0x7FFF`: hardware-forced to **bank 0**, always mapped.
- **Bank window** = logic `0x8000–0xFFFF`: whichever bank `CB` selects. **Bank N = physical `N×0x8000`.**
- You can't write `CB` directly. Load **`NB`** (new bank) first; a *taken* branch does `NB→CB`. A *not-taken*
  conditional branch does `CB→NB` (NB reverts) — so pre-loading NB before a conditional branch is safe.

**The bank of any symbol is purely mechanical from its physical address `P`:**

```
bank(P)  = (P < 0x8000) ? 0 : (P >> 15)
logic(P) = (P < 0x8000) ? P : (0x8000 | (P & 0x7FFF))
```

**NB-load rule** (the user's spec, restated): emit `ld nb,#bank(target)` **unless**
`bank(target) == 0` (common) **or** `bank(target) == bank(this instruction)`.

---

## 2. Why this is link-time (the constraint that shapes the design)

Each `CALL`/`JUMP` needs two facts about the target: its **logic displacement** (short vs long) and its
**bank** (NB or not). For the only interesting case — a call to *another function* (another `.area`) —
**both are known only at link time.** And:

- **No sdld target relaxes.** Exhaustive sweep of `linksrc/` for `relax`/`shrink`/`span`/`widen` → nothing.
  Every `R_*` relocation rewrites *within* a fixed byte region; the linker never removes bytes or reflows
  addresses.
- **The one assembler-side relaxer (STM8 `ls_mode`/`fuzz`/bit-table) forces the long form for any
  relocatable operand** (`if (e->e_base.e_ap) return long`). So even it can't shrink a cross-area ref.

**Therefore:** the assembler reserves the **worst-case fixed-size slot** and tags it with a relocation;
the **linker rewrites the slot** (form + NB) once it knows the address and bank. Size is never reclaimed
(unused bytes become `nop`). This is the **`R_J11` model** — the 8051 backend already has the linker fuse a
resolved address into instruction *opcode* bytes (`lkrloc3.c`), and `R_C24`/`adb_24_hi()` already extracts a
high/bank byte. We're adding one more such mode.

---

## 3. The reserved slot

Worst case = `ld nb,#bb` (3 B: `CE C4 bb`) + a long branch (3 B) = **6 bytes**.

| Mnemonic | Encoding | Bytes |
|---|---|---|
| `ld nb, #bb` | `CE C4 bb` | 3 |
| `carl target` / `carl cc,target` | `F2 lo hi` / `E8+cc lo hi` | 3 |
| `jrl  target` / `jrl  cc,target` | `F3 lo hi` / `EC+cc lo hi` | 3 |
| `cars target` / `cars cc,target` | `F0 d8` / `E0+cc d8` | 2 |
| `jrs  target` / `jrs  cc,target` | `F1 d8` / `E4+cc d8` | 2 |
| `nop` (padding) | `FF` | 1 |

**What the linker writes into the 6-byte slot** (nops are *trailing* — for an unconditional branch they're
dead; for a conditional one they execute harmlessly on the not-taken fall-through):

| Case | Sequence | Used / pad |
|---|---|---|
| same bank, near | `cars/jrs [cc,]d8` + `nop×4` | 2 + 4 |
| same bank, far  | `carl/jrl [cc,]d16` + `nop×3` | 3 + 3 |
| cross bank, near | `ld nb,#bb` + `cars/jrs [cc,]d8` + `nop` | 3+2 + 1 |
| cross bank, far | `ld nb,#bb` + `carl/jrl [cc,]d16` | 3+3 |
| common (bank 0) target | as "same bank" (no NB) | — |

The branch displacement is computed by the linker from the branch's **final offset inside the slot**
(which it controls), so the NB-present/absent shift is accounted for automatically.

---

## 4. The relocation

Mirror `R_J11`: the assembler emits a **template** in the slot bytes (so the linker recovers flavor + cc
without extra relocation fields), plus one `R` record.

**Template the assembler emits** (the worst-case long form, address/bank zeroed):

```
offset 0:  CE C4 00            ; ld nb,#0           (placeholder bank)
offset 3:  <op> 00 00          ; carl|jrl [cc,] 0   (long branch, placeholder disp16)
```
where `<op>` ∈ { `F2` carl, `E8+cc` carl cc, `F3` jrl, `EC+cc` jrl cc }. The linker reads byte 3 to recover
**CALL vs JUMP** and the **condition**.

**`R` record:** points at slot offset 0, mode = a new **`R_S1C88_BRANCH`** (carved from the
`R_ESCAPE_MASK = 0xf0` namespace so it doesn't collide with the byte/word modes), symbol = `target`,
PC-relative base = the slot.

---

## 5. Linker rewrite (`lkrloc3.c`, new `case`)

```text
on R_S1C88_BRANCH at slot S referencing symbol T:
    Ttgt   = resolved address of T              # physical (locator-assigned)
    tbank  = bank(Ttgt)                          # = Ttgt < 0x8000 ? 0 : Ttgt>>15
    cbank  = this area's a_bank                  # bank of the code being relocated
    flavor, cc = decode(slot[3])                 # carl/jrl + condition
    needNB = (tbank != 0) && (tbank != cbank)

    p = S
    if needNB:  emit CE C4 tbank at p;  p += 3
    Lbranch = logic(Ttgt)                         # target's logic address (0x8000|... or P)
    Lhere   = logic address of p                  # branch's own logic address
    disp    = Lbranch - (Lhere + lenOfChosenForm)
    if disp fits int8 and cc is basic/uncond:
         emit short form (cars/jrs/cc) at p;  p += 2
    else:
         emit long  form (carl/jrl/cc) at p;  p += 3
    fill [p .. S+6) with FF (nop)
```

Everything on the right is available during relocation: `T`'s resolved address, the symbol→area→`a_bank`
chain (ASlink already tracks it), and the current area's `a_bank`. **The conditional-NB ("skip when bank 0
or current bank") is just the `needNB` line — no separate phase.**

---

## 6. Assembler emission (`sdas88`)

1. `s1c88pst.c`: two new mnemonics `{ "call", S_PCALL, … }`, `{ "jump", S_PJUMP, … }` (distinct from the
   real `call`/`carl`/`jrl`). *(Pick final spellings — `CALL`/`JUMP` collide with nothing in our set, but
   `call` already exists as the absolute-indirect `call (hhll)`; use `CALL`/`JUMP` upper-or-distinct.)*
2. `s1c88adr.c`: operand path for `target [, cc]` (reuse `admode(CND)` for the optional condition).
3. `s1c88mch.c`: a `machine()` case that emits the 6-byte template (§4) and the `R_S1C88_BRANCH` relocation
   via the relocation-emit path (model on how `outrw(…, R_…)` attaches a mode today).
4. **Optional refinement (later):** if the target is same-area and already defined (backward ref) *or* an
   absolute constant, resolve at assembly time and emit the minimal form directly — skipping the slot and
   saving the worst-case bytes. Not needed for correctness; the linker handles those uniformly.

---

## 7. Bank config (`.lk`) + area→bank tagging

The linker needs each code area's `a_bank`. Two pieces:

1. **Enable area/bank tagging in `sdas88`** — the `.bank` directive is currently commented out in
   `s1c88pst.c`; turn it on so `.area`s emit `A name … bank N`. (Or adopt the GBZ80 name-encoding
   convention `_CODE_N` → bank `N`, which ASlink already decodes via `atoi(a_id+6)`.)
2. **A bank-layout `.lk`** for the target. Pokémon Mini bank table (physical, cart file byte 0 ↔ `0x2100`):

   | Bank | Physical | Logic | Holds |
   |---|---|---|---|
   | 0 (common) | `0x002100–0x007FFF` | `0x2100–0x7FFF` | header, IRQ vectors, always-resident code (~24 KiB) |
   | 1 | `0x008000–0x00FFFF` | `0x8000–0xFFFF` | banked code, 32 KiB |
   | … | `N×0x8000 …` | `0x8000–0xFFFF` | … |
   | 63 | `0x1F8000–0x1FFFFF` | `0x8000–0xFFFF` | last bank of a 2 MiB cart |

   ```
   -mwxu
   ; RAM (common; physical == logic)
   -b _DATA   = 0x1300
   ; common-area ROM (bank 0; logic == physical)
   -b _HEADER = 0x2100        ; "MN" + IRQ vector table
   -b _HOME   = 0x2200        ; always-resident code
   -b _CODE   = 0x....        ; main common-area code (bank 0)
   ; banked ROM (window 0x8000; one bank each)
   B  BANK1 base=0x008000 size=0x8000 map=0x8000 fsfx=.b01
   B  BANK2 base=0x010000 size=0x8000 map=0x8000 fsfx=.b02
   -b _CODE_1 = 0x8000        ; a_bank = 1
   -b _CODE_2 = 0x8000        ; a_bank = 2
   crt0.rel  main.rel  ...
   -e
   ```

   ASlink *parses* `B base/size/map` but its actual output path is target-specific (GB uses the
   name-encoding instead). Which mechanism we ride on — `B`-window overlay vs `_CODE_N` name-encoding — is a
   detail to pin during implementation; the **bank table is firm either way**. A small post-link romgen drops
   each bank slice at physical `N×0x8000` in the flat `.min` (`sdobjcopy` is already in `bin/`).

---

## 8. Constraints / decisions to confirm

- **Conditions:** `CALL`/`JUMP` support unconditional + the basic flags `C/NC/Z/NZ` (both short & long exist).
  The **signed** conditions (`LT/GE/…`) are CE-page **short-only** — a *far* signed-conditional branch would
  need an invert-and-skip the linker can't synthesize in a fixed slot, so they're out of scope here (and
  already covered by `genCmp`'s direct `jrs LT/GE`).
- **Worst-case size is permanent** (nop padding) — acceptable because these pseudo-ops are for
  *potentially-far/cross-bank* transfers (function calls, tail-jumps); local control flow keeps using plain
  `jrs`/`jrl` chosen by the compiler peephole.
- **`map`/`base` vs name-encoding** — pick the ASlink banking mechanism (see §7).
- **Return bank: RESOLVED (2026-06-06)** — the Pokémon Mini runs **maximum mode** (PokeMini pushes
  `PC.B.I` on every CALL and RET pops 3 bytes; min mode pins the bank window, unusable for a 2MB
  banked ROM). `CALL`/`CARL` push the 3-byte `PCL PCH CB` frame and `RET` restores the caller's
  bank automatically — `CALL` alone is sufficient, exactly as this design assumed. The compiler's
  call model was retargeted to match (`call_overhead` 5, caller cleanup, the `__sdcc_fptr`
  indirect-call cell) — see `abi-decision.md` "The call model: MAXIMUM mode".

---

## 9. Order of work

| # | Phase | Where | Notes |
|---|---|---|---|
| 0 | Build the linker | ✅ `scripts/build-sdld.sh` + `link-smoke.sh` | base `sdldz80` builds & links sdas88 objects (both reloc kinds). Remaining: brand an `sdld88` (distinct `TARGET_ID_S1C88`, z80-like) to hang the new reloc off. |
| 1 | Pseudo-op + same-area path | `s1c88pst/adr/mch.c` | parse `CALL/JUMP target[,cc]`; resolve same-area/absolute at assembly time |
| 2 | `R_S1C88_BRANCH` reloc | `asxxxx.h` (shared) + `sdas88` emit | escape-mode mode; template bytes (§4) |
| 3 | Linker rewrite | `lkrloc3.c` (in `sdld88`) | the §5 algorithm; model on the `R_J11` block |
| 4 | Area↔bank tagging | enable `.bank` in `s1c88pst.c`; bank `.lk` | populate `a_bank` (§7) |
| 5 | romgen + e2e test | `sdobjcopy` + a 2-bank test | verify slot rewrite, NB present/absent, short/long |

The novel work is steps 2–3 (the rewrite relocation); steps 0/4/5 are plumbing on existing ASlink banking.

---

## 10. Relaxation plan (TODO #14) — shrinking the worst-case slot

The Phase-2 design above deliberately reserves the **worst-case** 6-byte slot
(`ld nb,#bank ; carl/jrl`; 9 B for a signed-conditional via the #13 trampoline) and
never reclaims it — the linker only fills unused bytes with `nop`. Relaxation makes
the slot shrink to the smallest legal form when the target is **same-bank and in
range**. Most calls are intra-common-bank, so the payoff is large. Two independent
mechanisms, deliberately split so the easy, high-value half ships without the hard
half:

### The forms, smallest-first

| Case | Sequence | Bytes |
|---|---|---|
| same bank, `jrs`/`cars` range (±127), uncond/basic-cc | `cars`/`jrs [cc,] d8` | 2 |
| same bank, `jrl`/`carl` range (±32 K), uncond/basic-cc | `carl`/`jrl [cc,] d16` | 3 |
| same bank, signed-cc, in `jrs` range | `jrs <cc>, d8` (CE-page) | 3 |
| same bank, signed-cc, out of `jrs` range | `jrs <inv>,+4 ; jrl d16` (#13-style, no `ld nb`) | 6 |
| cross bank | `ld nb,#bank ; <branch>` | +3 |

### #14b — assembler-side, SAME-MODULE (the practical win, no linker change) — ✅ DONE

**Implemented** in `s1c88mch.c` `S_PCALL`/`S_PJUMP`: when `e1.e_flag==0 && e1.e_base.e_ap
== dot.s_area` (target in the current area ⇒ same bank), drop the `ld nb` and emit the
minimal relative form — `cars`/`jrs d8` (2 B) if the displacement fits ±127, else
`carl`/`jrl d16` (3 B). Short/long is chosen via a per-target `setbit`/`getbit` bit table
(ported from asstm8/asf8; reset in `minit()`): pass 0 sizes long (upper bound), pass 1
records the decision with the `fuzz` forward-ref correction, pass 2 replays it — so the
pass-1/pass-2 layouts match and the pass-2 displacement is exact (a pass-2 range check
loudly catches a wrong short choice). Unconditional + basic-cc relax; signed-cc same-area
(scc≥0) and all cross-area/external targets fall through to the fixed 6/9-byte slot,
unchanged. The compiler still sizes the worst case (6/9) in `s1c88instructionSize` — it
can't know a target's area at compile time, and over-sizing only makes its own jr-range
calc conservative (safe). Measured: relax-analysis opportunity collapsed 45→2 user slots;
in real multi-function objects intra-module `bcall` widely lowers to 2-byte `cars`.

The original design notes follow.

A call to another function in the **same `_CODE` area** (the common intra-module case)
has a displacement the assembler can compute each pass. ASxxxx already runs a
**multi-pass `fuzz`** loop (`asmain.c` — `a_fuzz` tracks pass-to-pass address drift, so
forward refs converge); a relaxing backend emits a size that may change across passes,
and the loop re-runs until stable. So `machine()`'s `S_PCALL`/`S_PJUMP` path, when
`expr.e_base.e_ap == dot.s_area` and the symbol is in-area, can emit the minimal form
directly (no `ld nb` — same area ⇒ same bank; `cars`/`jrs` vs `carl`/`jrl` by the
fuzz-current displacement). Cross-area/external targets fall through to the existing
fixed 6/9-byte linker-resolved slot, unchanged.

**Feasibility gate — ✅ ANSWERED by #14a (`scripts/relax-analysis.sh`).** sdas does NOT
iterate to a `fuzz==0` fixpoint — it runs a FIXED 3-pass sequencer
(`asxxsrc/asmain.c` `for(pass=0;pass<3)`). But it does not need to: the STM8 and F8
backends already relax short/long *inside* this same 3-pass loop, via a per-target
`setbit`/`getbit` bit table + the shared `fuzz` correction. It converges because the
scheme is **monotonic** — pass 0 sizes everything LONG (upper bound), pass 1 shrinks
only what fits and records the choice, pass 2 replays it; shrinking only pulls targets
closer, so a branch that fit can never stop fitting. So #14b adds NO `asmain.c` change.
**The one catch:** STM8/F8 `ls_mode` forces long for *any* relocatable operand
(`e_base.e_ap != 0`); #14b must instead relax the **same-area** relocatable case, where
the displacement is `e_addr − dot.s_addr` (known each pass). Port the ~30-line bit table
from `asstm8`/`asf8` into `s1c88mch.c`. Watch the branch-displacement convention
(§ HANDOFF: one byte earlier than z80) at *each* form.

**Opportunity measured (#14a):** on real fully-linked programs (examples/hello +
`scripts/relax/{fixmath,sprite}.c`), **user-code call sites shrink ~53% — 143 B saved of
270 across 45 slots, every one same-bank** (intra-common-bank — exactly this #14b path);
crt0's 27 reset/IRQ vector slots are hardware-fixed and excluded. The analysis also
flagged that the linker currently NOPs the `ld nb` for only *some* bank-0 targets (20 of
the sampled slots still carry a live `ld nb,#0` — an existing inconsistency that
relaxation makes moot). Re-run anytime: `scripts/relax-analysis.sh` (report-only).

### #14c — linker-side, CROSS-MODULE (the hard reflow, deferrable)

A cross-area/cross-module call's displacement is known only at link time, and sdld is
**fixed-size**: addresses are assigned per area in `lkarea` before relocation, and the
`.rel` `T` records + symbol addresses bake in fixed offsets. True shrinking needs a new
pass: model each area's bytes + relocs + symbol positions, iteratively shrink in-range
same-bank slots, recompute positions to a fixpoint (only ever shrinking ⇒ it converges;
guard that no branch which fit stops fitting), then re-emit. Stage it: (i) single-pass
conservative shrink; (ii) iterate; (iii) conditional-trampoline shrink. This is the
"large" part and can wait — #14b already covers single-module and common-bank-heavy
programs.

### Validation (every step)

`branch-smoke.sh` byte-locks every form; emu-test + diff-test prove correctness;
re-baseline the corpus afterward (sizes change). Add a size delta to the corpus report
so each shrink is visible and provably monotone.
