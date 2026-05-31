# sdas88 — retargeting the assembler backend (z80 clone → real S1C88)

> **Status:** `sdas/as88/` builds `bin/sdas88` and encodes **real S1C88** for the **register/immediate
> subset (v0, byte-verified vs App. A — commit `16a9bd4`)**: `ld8`/`ld16` (reg-reg, reg-#imm, rr-rr),
> 8-bit + 16-bit ALU, `inc`/`dec`, `push`/`pop`, `ex`, `ret`/`nop`. **v1 (commit `dad7745`) adds the
> memory-indirect operands** — `(hl)`/`(ix)`/`(iy)`, `d(ix)`/`d(iy)`, `(hhll)`/`(label)` absolute (byte-
> verified). Run on real codegen (args.c, 70 insns) it assembles every valid instruction and flags only
> genuine codegen z80-isms (`sub a,l`, `push af`, `pop de`, `add ix,sp`, `jp (hl)`/`jp label`). **Next
> (v2): the branch instructions** (`jp hl`/`call`/`jrs`/`jrl`/`cars`/`carl` with PC-relative) + rotates,
> then wire into `check-s1c88.sh`. Build with
> `scripts/build-sdas.sh as88`. Authority: [`instruction-set.md`](instruction-set.md) (App. A opcode map
> + the `CE`/`CF` prefix pages) and [`addressing-modes.md`](addressing-modes.md).
>
> **Two framework gotchas (cost real time):** (1) the `mne[]` table's **last entry must have
> `m_flag = S_EOL`** (octal 040) — the loader (`assym.c`) hashes entries until it sees it; without it the
> loop runs off the array → segfault. (2) `minit()` **must** do `hilo = 0` (little-endian) +
> `exprmasks(4)` (expression/address masks) or the output machinery segfaults at startup.

## The ASxxxx framework (Alan Baldwin's), per file

A backend = 4 files over the shared `sdas/asxxsrc/` core (`asmain`/`aslex`/`asexpr`/`asout`…):

| File | Role |
|------|------|
| `s1c88.h` | `#define`s: register numbers, addressing-mode constants (`S_*`), instruction-type constants (`S_LD`…), and the `adsym`/extern table decls. |
| `s1c88adr.c` | operand parser. `addr(esp)` decodes one operand → a *mode* (`S_IMMED`, `S_R8`, `S_R16`, `S_INDB`=`(r8)`, `S_INDR+reg`=`(r16)`, `offset(IX/IY)`, `S_INDM`=`(label)`, `S_USER`=label/expr). `admode(table)` matches a register from a table; the `R8[]`/`R16[]`/`CND[]` tables map register strings → numbers. |
| `s1c88pst.c` | the `mne[]` table: each row `{NULL, "MNEMONIC", S_TYPE, m_flag, base_opcode}`. Pseudo-ops (`.area`,`.module`,…) are shared/standard — **keep verbatim**. The machine mnemonics drive `machine()`. |
| `s1c88mch.c` | `machine(mp)` — the encoder. `op = mp->m_valu` (base opcode), `rf = mp->m_type`; a `switch (rf)` per instruction class emits bytes via `outab()`, reading operands via `addr()`/`admode()`. Plus `minit()` (sets `cpu`/`dsft`), `genop()`, `mchpcr()` (PC-relative). |

`outab(b)` emits one byte; `outrb(&e,flags)` emits a relocatable byte; `outaw`/`outrw` words. The
`mchtyp` switch at the top of `machine()` gates z80 variants — **collapse to a single S1C88 type.**

## What the codegen emits (the syntax sdas88 must accept)

sdcc88 emits **sdas dialect** (unchanged from the z80 base): `.module`/`.area`, lowercase mnemonics,
`(hl)` / `(ix+d)` indirect, `#imm`, `_label`. So the *parser* (`addr()`, delimiters `(` `)`) stays; only
the register tables and the encodings change. Current emitted set (grows as Step 2/3 land): `ld` (reg-reg,
reg-#imm, reg-(hl)/(ix+d), (hl)/(nn)-reg, 16-bit `ld rr,#imm`/`ld rr,(nn)`), `add/adc/sub/sbc/and/or/xor/cp`
(8-bit + 16-bit `add hl,ba` etc.), `inc/dec` (8/16), `push/pop`, `ret`/`call`/`jp`/`jr`(+cc), `ex ba,hl`,
`cp`, `swap`, and the S1C88 branch forms as Step 3 lands (`jrs`/`jrl`/`cars`/`carl`, `rete`).

## Register / addressing tables → S1C88 (`s1c88.h` + `s1c88adr.c`)

- **Byte regs `R8[]`** → `a, b, l, h` only (drop `c,d,e`). S1C88 byte-reg field encodings (from App. A
  `LD A,r` = 40–43: A,B,L,H): **A=0? B? L? H?** — read the `LD r,r'` block (instruction-set.md L134-195:
  `ld a,a/b/l/h`=40/41/42/43, so the *source* field is A=0,B=1,L=2,H=3; the *dest* row stride is 8:
  A-row 40, B-row 48, L-row 50, H-row 58). Encode r8 = {A:0,B:1,L:2,H:3} and dest = base + r8*? — derive
  from the 40–5F block.
- **16-bit regs `R16[]`** → `ba, hl, ix, iy, sp` (drop `bc,de`). The CF-page 16-bit ops use a 2-bit/3-bit
  pair field (App. A.3 `CF` page: `ADD BA,BA/HL/IX/IY`=CF,00..03; `LD BA,BA..IY`=CF,E0..E3; etc.).
- **Drop** `R8X` (I/R), `R16X` (AF/AF'), `R8U1`/`R8U2` (ixh/iyl undoc — S1C88 IX/IY aren't byte-addressable).
- **`CND[]`** → S1C88 conditions: `C, NC, Z, NZ` (+ the extended `LT/LE/GT/GE/V/NV/P/M/F0–F3` for the full
  set; the codegen currently emits only C/NC/Z/NZ via `jr cc`/`ret cc`/`jp cc`).
- **`s1c88.h` `#define`s:** renumber the registers to the S1C88 field encodings; replace the z80
  instruction-type enum (`S_LD`…`S_SBC`, the HD64180/eZ80/ZXN blocks) with the S1C88 classes (below).

## Instruction classes (the `switch (rf)` cases) and encoding

S1C88 opcodes come in three "pages" (instruction-set.md App. A): **unprefixed**, **`CE`-prefixed**
(extended 8-bit / control-reg / `[IX+dd]` etc.), **`CF`-prefixed** (16-bit arith/transfer + stack-rr).
Map classes to `machine()` cases, each emitting `[CE|CF,] opcode [, operands]`:

- `S_LD8` — 8-bit `ld`: reg-reg (40–5F), reg-#imm (B0–B3), `[hl]`/`[ix]`/`[iy]`/`[ix+dd]`/`[hhll]`/`[br:ll]`
  forms (45/46/47, CE-page 40-43, etc.), and the store directions.
- `S_LD16` — 16-bit `ld` (CF,E0–FE for rr-rr; C4–C7 `ld rr,#imm`; B8–BF `[hhll]`; CF,C0–DF `[rr]`).
- `S_ALU8` — `add/adc/sub/sbc/and/or/xor/cp` 8-bit (the A-column blocks + immediate forms).
- `S_ALU16` — 16-bit `add/adc/sub/sbc/cp` (CF,00–3F: BA/HL/IX/IY × op).
- `S_INC`/`S_DEC` — 8-bit (unprefixed) and 16-bit (90–93 inc rr / 98–9B dec rr).
- `S_PUSH`/`S_POP` — A0–A3 / A8–AB (rr); CF,B0–B7 / CF,B4–B7 (single byte regs).
- `S_RET` (ret/rete/rets), `S_CALL`, `S_JP` (indirect), `S_JR`/`S_JRL`/`S_CARS`/`S_CARL` (relative, PC-rel
  via `mchpcr`), `S_EX` (C8–CB ex ba,hl/ix/iy/sp; CC ex a,b; CD ex a,[hl]), `S_SWAP`, `S_MLT`/`S_DIV`,
  `S_PACK`/`S_UPCK`, `S_INH` (nop/halt/etc.).

Each case: validate operand modes, then `outab(op | field)` (+ prefix byte, + displacement/immediate
bytes via `outrb`/`outrw`). Use `sdasz80` as an **encoding oracle for the mnemonics S1C88 shares with z80
by *opcode* (rare — most differ)**; the authority is App. A.

## Implementation order (coupled — the 4 files change together; build with `build-sdas.sh as88`)

1. **Foundation:** `s1c88.h` (register field encodings + the S1C88 `S_*` class enum) and `s1c88adr.c`
   (R8/R16/CND tables, prune the drops). `minit()` in `s1c88mch.c`: set `cpu="Epson S1C88"`, collapse the
   `mchtyp` gate.
2. **Minimal encoder:** rewrite `machine()` for the subset the codegen emits today (`ld8/ld16`, `alu8/16`,
   `inc/dec`, `push/pop`, `ret/jp/jr`, `ex ba,hl`). Trim the `mne[]` table to those + the pseudo-ops.
   Build; assemble a hand-written S1C88 `.asm` and check bytes against App. A (e.g. `ld a,#5`→`B0 05`,
   `inc hl`→`91`, `ret`→? , `add hl,ba`→`CF 20`).
3. **Wire validation:** point `scripts/check-s1c88.sh` (or a new `--assemble`) at `sdas88` to assemble the
   codegen smoke output; iterate until it's clean.
4. **Grow:** add the remaining classes (branches `jrs/jrl/cars/carl`, `mlt/div`, `pack/upck`, `rete`,
   control regs, far/`[br:ll]`) as Step 3 emission lands.

## Verification

A round-trip oracle: hand-assemble a few instructions per App. A, feed them to `sdas88`, and diff the
`.rel` `T` (text) records against the expected bytes. Once the emitted-codegen subset assembles cleanly,
`sdas88` becomes the real validator the codegen retarget has been missing.
