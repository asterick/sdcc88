# Banked `CALL` / `JUMP` — assembler + linker design (Phase 2)

> **Status:** design only — the *feature* is unimplemented. **Phase 0 (the linker) is now partly done:**
> `scripts/build-sdld.sh` builds the ASlink linker and `scripts/link-smoke.sh` proves `sdas88 + sdldz80`
> link a 2-area program with both relocation kinds resolving (cross-area absolute + PC-relative). So the
> base toolchain exists; what's left for this feature is the **s1c88-branded linker** (a distinct
> `TARGET_ID_S1C88`) and the **rewrite relocation** below (§4–5).
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
- **Return bank:** on `MODEL2/3 maximum mode`, `CALL` also pushes `CB`, so `RET` restores the caller's bank
  automatically (`memory-model.md` §2.x). Confirm the target model; if not maximum mode, the callee must
  restore the caller's bank itself. **Affects whether `CALL` alone is sufficient or needs a return-bank
  convention.**

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
