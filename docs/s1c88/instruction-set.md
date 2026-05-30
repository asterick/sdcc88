# S1C88 Instruction Set Reference

**Source:** S1C88 Core CPU Manual (MF658-05), Seiko Epson. First issue March 1993; printed February 2001.

**Sections covered (transcribed from machine-extracted page text):**

| Region | Manual section | PDF pages | Extracted files |
|--------|----------------|-----------|-----------------|
| 1 | §4.3 Instruction Set List (§4.3.1 classification, §4.3.2 symbols, §4.3.3 list by function) | 46–64 | `page-046`…`page-064` |
| 2 | Appendix A Operation Code Map | 201–203 | `page-201`…`page-203` |
| 2 | Appendix B Instruction List by Addressing Mode | 204–209 | `page-204`…`page-209` |
| 2 | Appendix C Instruction Index | 210–217 | `page-210`…`page-223` |

> **Authoritative per-instruction semantics live in §4.4 (PDF pp. 65–200), which is NOT transcribed here.** §4.4 gives the full operation pseudocode, flag-derivation detail, and timing notes for every individual instruction form. The "Page" columns below (and Appendix C) point into that §4.4 region (manual page numbers). When this reference and §4.4 disagree, §4.4 wins.

> **Cross-reference:** the sibling project `../skiploom` carries an independent S1C88 opcode table (`src/util/s1c88.csv`) derived from the same manual; use it to cross-check opcode bytes.

## Notes on the extraction and its faithfulness

The instruction tables in §4.3.3 are printed as parallel columns (Mnemonic / operands / Machine Code / Operation / Cycle / Byte / flags / Page). The text extractor flattened each column into a contiguous run, so reassembly is by positional correspondence. The reconstruction below is reliable for **mnemonic, operands, machine code (opcode bytes), operation, cycle and byte counts**, which I verified against the Appendix A opcode map and Appendix B byte/cycle columns.

**Flag columns:** the per-page flag grid is 9 columns wide (`SC  I1 I0 U D N V C Z`) and was flattened column-major into long runs of `–`/`↕`/`#`/`0`. Splitting that back into exact per-row, per-flag cells is error-prone, so rather than risk silent misalignment I give **flag effects per instruction class** (which is unambiguous from the manual and consistent across each block), and call out the special cases the manual marks. Where you need the exact flag bit set by one specific form, consult §4.4.

**Flag-column legend (order as printed):** `SC | I1 I0 U D N V C Z`
- `SC` — the System Condition flag register as a whole (changes only on `LD SC,…` / `AND/OR/XOR SC,#nn` / `POP SC` / `RETS`-class ops that reload SC).
- `I1 I0` — interrupt flags; `U` unpack flag; `D` decimal flag; `N` negative; `V` overflow; `C` carry; `Z` zero.
- `↕` set/reset according to result · `–` unchanged · `0` reset to 0 · `#` instruction also honors decimal/unpack operation (the `#` mark in §4.3.2).

**Known garbled / ambiguous spots (see also inline notes):**
- **Auxiliary Operation block (PDF p.52 / file `page-058`)**: PACK/UPCK/SEP came out badly. Only one machine code (`CE,A8`) and three byte/cycle pairs were cleanly extractable, and the operation-diagram nibble figures are scrambled. Opcodes are recovered from Appendix A instead: **PACK = `DE`**, **UPCK = `DF`**, **SEP = `CE,A8`**. Byte/cycle from Appendix B (12/12): PACK 1 byte/2 cyc, UPCK 1 byte/2 cyc, SEP 2 byte/3 cyc.
- **`LD [BR:ll],#nn` opcode** prints as `DD,ll,nn` in §4.3.3 (file `page-049`) — note operand order ll then nn in the encoding.
- Appendix B has a few OCR slips in operands: `[IX],{HL]` should be `[IX],[HL]`; `LD BR,#nn` in the immediate column is `LD BR,#hh`; `[HL],nn` is `[HL],#nn`. Corrected in the transcription.
- A handful of model-gated forms exist only on certain core models — see the per-block MODEL notes.

---

# §4.3.1 Function Classification

Instructions are grouped by function (manual Table 4.3.1.1):

| Group | Mnemonics |
|-------|-----------|
| **8-bit arithmetic & logic** | ADD, ADC, SUB, SBC, AND, OR, XOR, CP, BIT, INC, DEC, MLT, DIV, CPL, NEG |
| **8-bit transfer** | LD, EX, SWAP |
| **Rotate / shift** | RL, RLC, RR, RRC, SLA, SLL, SRA, SRL |
| **Auxiliary operation** | PACK, UPCK, SEP |
| **16-bit arithmetic** | ADD, ADC, SUB, SBC, CP, INC, DEC |
| **16-bit transfer** | LD, EX |
| **Stack control** | PUSH, POP |
| **Branch** | JRS, JRL, JP, DJR, CARS, CARL, CALL, RET, RETE, RETS, INT |
| **System control** | NOP, HALT, SLP |

Operation meanings: Addition (ADD), Addition-with-carry (ADC), Subtraction (SUB), Subtraction-with-carry (SBC), Logical product (AND), Logical sum (OR), Exclusive-OR (XOR), Comparison (CP), Bit test (BIT), Increment (INC), Decrement (DEC), Multiplication (MLT), Division (DIV), 1's complement (CPL), 2's complement / negate (NEG), Load (LD), Byte exchange (EX), Nibble exchange (SWAP), rotate left/right with-or-without carry (RL/RLC/RR/RRC), arithmetic/logical shift left/right (SLA/SLL/SRA/SRL), Pack (PACK), Unpack (UPCK), Code extension (SEP), Push/Pop, relative short/long jump (JRS/JRL), indirect jump (JP), loop (DJR), relative short/long call (CARS/CARL), indirect call (CALL), Return (RET), exception-return (RETE), return-and-skip (RETS), software interrupt (INT), No-op (NOP), HALT, SLEEP (SLP).

---

# §4.3.2 Symbol Meanings (notation legend)

### Registers

| Symbol | Meaning |
|--------|---------|
| `A` | Data register A (8-bit) |
| `A(H)` / `A(L)` | Upper / lower 4 bits of A |
| `B` | Data register B (8-bit) |
| `BA` | BA pair register (16-bit; B = high, A = low) |
| `H`, `L` | Data registers H, L (8-bit) |
| `HL` | Index register HL (16-bit) |
| `IX`, `IY` | Index registers IX, IY (16-bit) |
| `IX(H)`/`IX(L)`, `IY(H)`/`IY(L)` | Upper / lower 8 bits of IX, IY |
| `SP` | Stack pointer (16-bit) |
| `BR` | Base register (8-bit; provides high byte of 8-bit-absolute `[BR:ll]` addressing) |
| `SC` | System Condition flag register |
| `CC` | Customize Condition flag |
| `PC` | Program counter; `PC(H)`/`PC(L)` upper/lower 8 bits |
| `NB` | New code-bank register (MODEL2/3 only) |
| `CB` | Code-bank register |
| `EP` | Expand page register (MODEL2/3 only) |
| `XP` | Expand page register for IX (MODEL2/3 only) |
| `YP` | Expand page register for IY (MODEL2/3 only) |
| `IP` | XP and YP register pair |

### Immediate / address operands

| Symbol | Meaning |
|--------|---------|
| `nn` | 8-bit immediate data (unsigned) |
| `hh` | absolute address, upper 8 bits (unsigned) |
| `ll` | absolute address, lower 8 bits (unsigned) |
| `pp` | page setting data (unsigned) |
| `bb` | bank setting data (unsigned) |
| `dd` | signed 8-bit displacement |
| `rr` | 8-bit relative address (signed) |
| `kk` | vector address setting data (unsigned) |
| `mmnn` | 16-bit immediate (unsigned; mm = high, nn = low) |
| `hhll` | 16-bit absolute address (unsigned) |
| `qqrr` | 16-bit relative address (signed; qq = high, rr = low) |

### Memory operands

| Symbol | Meaning |
|--------|---------|
| `[HL]` | memory at HL · `[HL](H)`/`[HL](L)` upper/lower 4 bits |
| `[IX]`, `[IY]` | memory at IX / IY |
| `[IX+dd]`, `[IY+dd]` | memory at IX/IY + signed displacement dd |
| `[IX+L]`, `[IY+L]` | memory at IX/IY + L register |
| `[BR:ll]` | memory at {BR, ll} (8-bit absolute, high byte = BR) |
| `[hhll]` | memory at 16-bit absolute address hhll |
| `[kk]` | interrupt/call vector at kk |
| `[SP]`, `[SP+dd]` | stack at SP / SP+dd |

### Flags & operators

`Z` zero, `C` carry, `V` overflow, `N` negative, `D` decimal, `U` unpack, `I0`/`I1` interrupt flags, `F0`–`F3` customize-condition flags.
Flag effect symbols: `↕` set/reset, `–` no change, `0` reset.
Operators: `+` add, `-` subtract, `*` multiply, `/` divide, `∧` AND, `∨` OR, `∀` XOR, `#` instruction permits decimal & unpack operation.

---

# §4.3.3 Instruction List by Function

All "Machine Code" values are hex opcode bytes in program order (operand bytes such as `nn`, `ll`, `hh`, `dd`, `mm` follow). Columns: **Cyc** = cycles (states), **Byt** = byte length. **Flags** summarized per block; **§4.4 pg** = page in §4.4.

## 8-bit Transfer Instructions (LD / EX / SWAP)

**Flags:** all 8-bit transfer forms leave `Z C V N D U I0 I1` unchanged. The two `LD SC,…` forms (`LD SC,A`, `LD SC,#nn`) reload the whole SC flag register (`SC` column = `↕`). All others `–` across the board.

> MODEL note: `NB`, `EP`, `XP`, `YP` accesses are MODEL2/3 only; on MODEL0/1 those `LD` forms are unavailable.

### LD into A / B / L / H (page 47–48 of manual)

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| LD A,A | 40 | A←A | 1 | 1 | 115 |
| LD A,B | 41 | A←B | 1 | 1 | 115 |
| LD A,L | 42 | A←L | 1 | 1 | 115 |
| LD A,H | 43 | A←H | 1 | 1 | 115 |
| LD A,BR | CE,C0 | A←BR | 2 | 2 | 115 |
| LD A,SC | CE,C1 | A←SC | 2 | 2 | 115 |
| LD A,#nn | B0,nn | A←nn | 2 | 2 | 122 |
| LD A,[BR:ll] | 44,ll | A←[BR:ll] | 3 | 2 | 125 |
| LD A,[hhll] | CE,D0,ll,hh | A←[hhll] | 5 | 4 | 127 |
| LD A,[HL] | 45 | A←[HL] | 2 | 1 | 127 |
| LD A,[IX] | 46 | A←[IX] | 2 | 1 | 129 |
| LD A,[IY] | 47 | A←[IY] | 2 | 1 | 130 |
| LD A,[IX+dd] | CE,40,dd | A←[IX+dd] | 4 | 3 | 132 |
| LD A,[IY+dd] | CE,41,dd | A←[IY+dd] | 4 | 3 | 133 |
| LD A,[IX+L] | CE,42 | A←[IX+L] | 4 | 2 | 135 |
| LD A,[IY+L] | CE,43 | A←[IY+L] | 4 | 2 | 136 |
| LD A,NB | CE,C8 | A←NB | 2 | 2 | 116 |
| LD A,EP | CE,C9 | A←EP | 2 | 2 | 116 |
| LD A,XP | CE,CA | A←XP | 2 | 2 | 116 |
| LD A,YP | CE,CB | A←YP | 2 | 2 | 116 |
| LD B,A | 48 | B←A | 1 | 1 | 115 |
| LD B,B | 49 | B←B | 1 | 1 | 115 |
| LD B,L | 4A | B←L | 1 | 1 | 115 |
| LD B,H | 4B | B←H | 1 | 1 | 115 |
| LD B,#nn | B1,nn | B←nn | 2 | 2 | 122 |
| LD B,[BR:ll] | 4C,ll | B←[BR:ll] | 3 | 2 | 125 |
| LD B,[hhll] | CE,D1,ll,hh | B←[hhll] | 5 | 4 | 127 |
| LD B,[HL] | 4D | B←[HL] | 2 | 1 | 127 |
| LD B,[IX] | 4E | B←[IX] | 2 | 1 | 129 |
| LD B,[IY] | 4F | B←[IY] | 2 | 1 | 130 |
| LD B,[IX+dd] | CE,48,dd | B←[IX+dd] | 4 | 3 | 132 |
| LD B,[IY+dd] | CE,49,dd | B←[IY+dd] | 4 | 3 | 133 |
| LD B,[IX+L] | CE,4A | B←[IX+L] | 4 | 2 | 135 |
| LD B,[IY+L] | CE,4B | B←[IY+L] | 4 | 2 | 136 |
| LD L,A | 50 | L←A | 1 | 1 | 115 |
| LD L,B | 51 | L←B | 1 | 1 | 115 |
| LD L,L | 52 | L←L | 1 | 1 | 115 |
| LD L,H | 53 | L←H | 1 | 1 | 115 |
| LD L,#nn | B2,nn | L←nn | 2 | 2 | 122 |
| LD L,[BR:ll] | 54,ll | L←[BR:ll] | 3 | 2 | 125 |
| LD L,[hhll] | CE,D2,ll,hh | L←[hhll] | 5 | 4 | 127 |
| LD L,[HL] | 55 | L←[HL] | 2 | 1 | 127 |
| LD L,[IX] | 56 | L←[IX] | 2 | 1 | 129 |
| LD L,[IY] | 57 | L←[IY] | 2 | 1 | 130 |
| LD L,[IX+dd] | CE,50,dd | L←[IX+dd] | 4 | 3 | 132 |
| LD L,[IY+dd] | CE,51,dd | L←[IY+dd] | 4 | 3 | 133 |
| LD L,[IX+L] | CE,52 | L←[IX+L] | 4 | 2 | 135 |
| LD L,[IY+L] | CE,53 | L←[IY+L] | 4 | 2 | 136 |
| LD H,A | 58 | H←A | 1 | 1 | 115 |
| LD H,B | 59 | H←B | 1 | 1 | 115 |
| LD H,L | 5A | H←L | 1 | 1 | 115 |
| LD H,H | 5B | H←H | 1 | 1 | 115 |
| LD H,#nn | B3,nn | H←nn | 2 | 2 | 122 |
| LD H,[BR:ll] | 5C,ll | H←[BR:ll] | 3 | 2 | 125 |
| LD H,[hhll] | CE,D3,ll,hh | H←[hhll] | 5 | 4 | 127 |
| LD H,[HL] | 5D | H←[HL] | 2 | 1 | 127 |
| LD H,[IX] | 5E | H←[IX] | 2 | 1 | 129 |
| LD H,[IY] | 5F | H←[IY] | 2 | 1 | 130 |
| LD H,[IX+dd] | CE,58,dd | H←[IX+dd] | 4 | 3 | 132 |
| LD H,[IY+dd] | CE,59,dd | H←[IY+dd] | 4 | 3 | 133 |
| LD H,[IX+L] | CE,5A | H←[IX+L] | 4 | 2 | 135 |
| LD H,[IY+L] | CE,5B | H←[IY+L] | 4 | 2 | 136 |

### LD of control regs / stores (page 48–50)

| Mnemonic | Machine Code | Operation | Cyc | Byt | Flags | §4.4 pg |
|----------|--------------|-----------|----:|----:|------|--------:|
| LD BR,A | CE,C2 | BR←A | 2 | 2 | – | 116 |
| LD BR,#hh | B4,hh | BR←hh | 2 | 2 | – | 122 |
| LD SC,A | CE,C3 | SC←A | 3 | 2 | SC=↕ | 116 |
| LD SC,#nn | 9F,nn | SC←nn | 3 | 2 | SC=↕ | 122 |
| LD [BR:ll],A | 78,ll | [BR:ll]←A | 3 | 2 | – | 117 |
| LD [BR:ll],B | 79,ll | [BR:ll]←B | 3 | 2 | – | 117 |
| LD [BR:ll],L | 7A,ll | [BR:ll]←L | 3 | 2 | – | 117 |
| LD [BR:ll],H | 7B,ll | [BR:ll]←H | 3 | 2 | – | 117 |
| LD [BR:ll],#nn | DD,ll,nn | [BR:ll]←nn | 4 | 3 | – | 124 |
| LD [BR:ll],[HL] | 7D,ll | [BR:ll]←[HL] | 4 | 2 | – | 128 |
| LD [BR:ll],[IX] | 7E,ll | [BR:ll]←[IX] | 4 | 2 | – | 129 |
| LD [BR:ll],[IY] | 7F,ll | [BR:ll]←[IY] | 4 | 2 | – | 131 |
| LD [hhll],A | CE,D4,ll,hh | [hhll]←A | 5 | 4 | – | 118 |
| LD [hhll],B | CE,D5,ll,hh | [hhll]←B | 5 | 4 | – | 118 |
| LD [hhll],L | CE,D6,ll,hh | [hhll]←L | 5 | 4 | – | 118 |
| LD [hhll],H | CE,D7,ll,hh | [hhll]←H | 5 | 4 | – | 118 |
| LD [HL],A | 68 | [HL]←A | 2 | 1 | – | 118 |
| LD [HL],B | 69 | [HL]←B | 2 | 1 | – | 118 |
| LD [HL],L | 6A | [HL]←L | 2 | 1 | – | 118 |
| LD [HL],H | 6B | [HL]←H | 2 | 1 | – | 118 |
| LD [HL],#nn | B5,nn | [HL]←nn | 3 | 2 | – | 124 |
| LD [HL],[BR:ll] | 6C,ll | [HL]←[BR:ll] | 4 | 2 | – | 126 |
| LD [HL],[HL] | 6D | [HL]←[HL] | 3 | 1 | – | 128 |
| LD [HL],[IX] | 6E | [HL]←[IX] | 3 | 1 | – | 129 |
| LD [HL],[IY] | 6F | [HL]←[IY] | 3 | 1 | – | 131 |
| LD [HL],[IX+dd] | CE,60,dd | [HL]←[IX+dd] | 5 | 3 | – | 132 |
| LD [HL],[IY+dd] | CE,61,dd | [HL]←[IY+dd] | 5 | 3 | – | 134 |
| LD [HL],[IX+L] | CE,62 | [HL]←[IX+L] | 5 | 2 | – | 135 |
| LD [HL],[IY+L] | CE,63 | [HL]←[IY+L] | 5 | 2 | – | 137 |
| LD [IX],A | 60 | [IX]←A | 2 | 1 | – | 119 |
| LD [IX],B | 61 | [IX]←B | 2 | 1 | – | 119 |
| LD [IX],L | 62 | [IX]←L | 2 | 1 | – | 119 |
| LD [IX],H | 63 | [IX]←H | 2 | 1 | – | 119 |
| LD [IX],#nn | B6,nn | [IX]←nn | 3 | 2 | – | 125 |
| LD [IX],[BR:ll] | 64,ll | [IX]←[BR:ll] | 4 | 2 | – | 126 |
| LD [IX],[HL] | 65 | [IX]←[HL] | 3 | 1 | – | 128 |
| LD [IX],[IX] | 66 | [IX]←[IX] | 3 | 1 | – | 130 |
| LD [IX],[IY] | 67 | [IX]←[IY] | 3 | 1 | – | 131 |
| LD [IX],[IX+dd] | CE,68,dd | [IX]←[IX+dd] | 5 | 3 | – | 133 |
| LD [IX],[IY+dd] | CE,69,dd | [IX]←[IY+dd] | 5 | 3 | – | 134 |
| LD [IX],[IX+L] | CE,6A | [IX]←[IX+L] | 5 | 2 | – | 136 |
| LD [IX],[IY+L] | CE,6B | [IX]←[IY+L] | 5 | 2 | – | 137 |
| LD [IY],A | 70 | [IY]←A | 2 | 1 | – | 119 |
| LD [IY],B | 71 | [IY]←B | 2 | 1 | – | 119 |
| LD [IY],L | 72 | [IY]←L | 2 | 1 | – | 119 |
| LD [IY],H | 73 | [IY]←H | 2 | 1 | – | 119 |
| LD [IY],#nn | B7,nn | [IY]←nn | 3 | 2 | – | 125 |
| LD [IY],[BR:ll] | 74,ll | [IY]←[BR:ll] | 4 | 2 | – | 126 |
| LD [IY],[HL] | 75 | [IY]←[HL] | 3 | 1 | – | 128 |
| LD [IY],[IX] | 76 | [IY]←[IX] | 3 | 1 | – | 130 |
| LD [IY],[IY] | 77 | [IY]←[IY] | 3 | 1 | – | 131 |
| LD [IY],[IX+dd] | CE,78,dd | [IY]←[IX+dd] | 5 | 3 | – | 133 |
| LD [IY],[IY+dd] | CE,79,dd | [IY]←[IY+dd] | 5 | 3 | – | 134 |
| LD [IY],[IX+L] | CE,7A | [IY]←[IX+L] | 5 | 2 | – | 136 |
| LD [IY],[IY+L] | CE,7B | [IY]←[IY+L] | 5 | 2 | – | 137 |
| LD [IX+dd],A | CE,44,dd | [IX+dd]←A | 4 | 3 | – | 120 |
| LD [IX+dd],B | CE,4C,dd | [IX+dd]←B | 4 | 3 | – | 120 |
| LD [IX+dd],L | CE,54,dd | [IX+dd]←L | 4 | 3 | – | 120 |
| LD [IX+dd],H | CE,5C,dd | [IX+dd]←H | 4 | 3 | – | 120 |
| LD [IY+dd],A | CE,45,dd | [IY+dd]←A | 4 | 3 | – | 120 |
| LD [IY+dd],B | CE,4D,dd | [IY+dd]←B | 4 | 3 | – | 120 |
| LD [IY+dd],L | CE,55,dd | [IY+dd]←L | 4 | 3 | – | 120 |
| LD [IY+dd],H | CE,5D,dd | [IY+dd]←H | 4 | 3 | – | 120 |
| LD [IX+L],A | CE,46 | [IX+L]←A | 4 | 2 | – | 121 |
| LD [IX+L],B | CE,4E | [IX+L]←B | 4 | 2 | – | 121 |
| LD [IX+L],L | CE,56 | [IX+L]←L | 4 | 2 | – | 121 |
| LD [IX+L],H | CE,5E | [IX+L]←H | 4 | 2 | – | 121 |
| LD [IY+L],A | CE,47 | [IY+L]←A | 4 | 2 | – | 121 |
| LD [IY+L],B | CE,4F | [IY+L]←B | 4 | 2 | – | 121 |
| LD [IY+L],L | CE,57 | [IY+L]←L | 4 | 2 | – | 121 |
| LD [IY+L],H | CE,5F | [IY+L]←H | 4 | 2 | – | 121 |
| LD NB,A | CE,CC | NB←A | 2 | 2 | – | 117 (MODEL2/3) |
| LD NB,#bb | CE,C4,bb | NB←bb | 3 | 3 | – | 123 (MODEL2/3) |
| LD EP,A | CE,CD | EP←A | 2 | 2 | – | 117 (MODEL2/3) |
| LD EP,#pp | CE,C5,pp | EP←pp | 3 | 3 | – | 123 (MODEL2/3) |
| LD XP,A | CE,CE | XP←A | 2 | 2 | – | 117 (MODEL2/3) |
| LD XP,#pp | CE,C6,pp | XP←pp | 3 | 3 | – | 123 (MODEL2/3) |
| LD YP,A | CE,CF | YP←A | 2 | 2 | – | 117 (MODEL2/3) |
| LD YP,#pp | CE,C7,pp | YP←pp | 3 | 3 | – | 124 (MODEL2/3) |

### EX / SWAP (8-bit)

| Mnemonic | Machine Code | Operation | Cyc | Byt | Flags | §4.4 pg |
|----------|--------------|-----------|----:|----:|------|--------:|
| EX A,B | CC | A↔B | 3 | 1 | – | 105 |
| EX A,[HL] | CD | A↔[HL] | 4 | 1 | – | 105 |
| SWAP A | F6 | A(H)↔A(L) | 2 | 1 | – | 188 |
| SWAP [HL] | F7 | [HL](H)↔[HL](L) | 3 | 1 | – | 188 |

## 16-bit Transfer Instructions (LD / EX)

**Flags:** all 16-bit transfers leave every flag unchanged (`–`).

### LD into BA / HL / IX / IY (page 51)

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| LD BA,BA | CF,E0 | BA←BA | 2 | 2 | 138 |
| LD BA,HL | CF,E1 | BA←HL | 2 | 2 | 138 |
| LD BA,IX | CF,E2 | BA←IX | 2 | 2 | 138 |
| LD BA,IY | CF,E3 | BA←IY | 2 | 2 | 138 |
| LD BA,SP | CF,F8 | BA←SP | 2 | 2 | 138 |
| LD BA,PC | CF,F9 | BA←PC+2 | 2 | 2 | 138 |
| LD BA,#mmnn | C4,nn,mm | BA←mmnn | 3 | 3 | 143 |
| LD BA,[hhll] | B8,ll,hh | A←[hhll], B←[hhll+1] | 5 | 3 | 144 |
| LD BA,[HL] | CF,C0 | A←[HL], B←[HL+1] | 5 | 2 | 145 |
| LD BA,[IX] | CF,D0 | A←[IX], B←[IX+1] | 5 | 2 | 146 |
| LD BA,[IY] | CF,D8 | A←[IY], B←[IY+1] | 5 | 2 | 146 |
| LD BA,[SP+dd] | CF,70,dd | A←[SP+dd], B←[SP+dd+1] | 6 | 3 | 147 |
| LD HL,BA | CF,E4 | HL←BA | 2 | 2 | 138 |
| LD HL,HL | CF,E5 | HL←HL | 2 | 2 | 138 |
| LD HL,IX | CF,E6 | HL←IX | 2 | 2 | 138 |
| LD HL,IY | CF,E7 | HL←IY | 2 | 2 | 138 |
| LD HL,SP | CF,F4 | HL←SP | 2 | 2 | 139 |
| LD HL,PC | CF,F5 | HL←PC+2 | 2 | 2 | 139 |
| LD HL,#mmnn | C5,nn,mm | HL←mmnn | 3 | 3 | 143 |
| LD HL,[hhll] | B9,ll,hh | L←[hhll], H←[hhll+1] | 5 | 3 | 144 |
| LD HL,[HL] | CF,C1 | L←[HL], H←[HL+1] | 5 | 2 | 145 |
| LD HL,[IX] | CF,D1 | L←[IX], H←[IX+1] | 5 | 2 | 146 |
| LD HL,[IY] | CF,D9 | L←[IY], H←[IY+1] | 5 | 2 | 146 |
| LD HL,[SP+dd] | CF,71,dd | L←[SP+dd], H←[SP+dd+1] | 6 | 3 | 147 |
| LD IX,BA | CF,E8 | IX←BA | 2 | 2 | 138 |
| LD IX,HL | CF,E9 | IX←HL | 2 | 2 | 138 |
| LD IX,IX | CF,EA | IX←IX | 2 | 2 | 138 |
| LD IX,IY | CF,EB | IX←IY | 2 | 2 | 138 |
| LD IX,SP | CF,FA | IX←SP | 2 | 2 | 139 |
| LD IX,#mmnn | C6,nn,mm | IX←mmnn | 3 | 3 | 143 |
| LD IX,[hhll] | BA,ll,hh | IX(L)←[hhll], IX(H)←[hhll+1] | 5 | 3 | 144 |
| LD IX,[HL] | CF,C2 | IX(L)←[HL], IX(H)←[HL+1] | 5 | 2 | 145 |
| LD IX,[IX] | CF,D2 | IX(L)←[IX], IX(H)←[IX+1] | 5 | 2 | 146 |
| LD IX,[IY] | CF,DA | IX(L)←[IY], IX(H)←[IY+1] | 5 | 2 | 146 |
| LD IX,[SP+dd] | CF,72,dd | IX(L)←[SP+dd], IX(H)←[SP+dd+1] | 6 | 3 | 147 |
| LD IY,BA | CF,EC | IY←BA | 2 | 2 | 138 |
| LD IY,HL | CF,ED | IY←HL | 2 | 2 | 138 |
| LD IY,IX | CF,EE | IY←IX | 2 | 2 | 138 |
| LD IY,IY | CF,EF | IY←IY | 2 | 2 | 138 |
| LD IY,SP | CF,FE | IY←SP | 2 | 2 | 139 |
| LD IY,#mmnn | C7,nn,mm | IY←mmnn | 3 | 3 | 143 |
| LD IY,[hhll] | BB,ll,hh | IY(L)←[hhll], IY(H)←[hhll+1] | 5 | 3 | 144 |
| LD IY,[HL] | CF,C3 | IY(L)←[HL], IY(H)←[HL+1] | 5 | 2 | 145 |
| LD IY,[IX] | CF,D3 | IY(L)←[IX], IY(H)←[IX+1] | 5 | 2 | 146 |
| LD IY,[IY] | CF,DB | IY(L)←[IY], IY(H)←[IY+1] | 5 | 2 | 146 |
| LD IY,[SP+dd] | CF,73,dd | IY(L)←[SP+dd], IY(H)←[SP+dd+1] | 6 | 3 | 147 |

### LD into SP / stores / EX (page 52)

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| LD SP,BA | CF,F0 | SP←BA | 2 | 2 | 140 |
| LD SP,[hhll] | CF,78,ll,hh | SP(L)←[hhll], SP(H)←[hhll+1] | 6 | 4 | 145 |
| LD SP,HL | CF,F1 | SP←HL | 2 | 2 | 140 |
| LD SP,IX | CF,F2 | SP←IX | 2 | 2 | 140 |
| LD SP,IY | CF,F3 | SP←IY | 2 | 2 | 140 |
| LD SP,#mmnn | CF,6E,nn,mm | SP←mmnn | 4 | 4 | 144 |
| LD [hhll],BA | BC,ll,hh | [hhll]←A, [hhll+1]←B | 5 | 3 | 140 |
| LD [hhll],HL | BD,ll,hh | [hhll]←L, [hhll+1]←H | 5 | 3 | 140 |
| LD [hhll],IX | BE,ll,hh | [hhll]←IX(L), [hhll+1]←IX(H) | 5 | 3 | 140 |
| LD [hhll],IY | BF,ll,hh | [hhll]←IY(L), [hhll+1]←IY(H) | 5 | 3 | 140 |
| LD [hhll],SP | CF,7C,ll,hh | [hhll]←SP(L), [hhll+1]←SP(H) | 6 | 4 | 141 |
| LD [HL],BA | CF,C4 | [HL]←A, [HL+1]←B | 5 | 2 | 141 |
| LD [HL],HL | CF,C5 | [HL]←L, [HL+1]←H | 5 | 2 | 141 |
| LD [HL],IX | CF,C6 | [HL]←IX(L), [HL+1]←IX(H) | 5 | 2 | 141 |
| LD [HL],IY | CF,C7 | [HL]←IY(L), [HL+1]←IY(H) | 5 | 2 | 141 |
| LD [IX],BA | CF,D4 | [IX]←A, [IX+1]←B | 5 | 2 | 142 |
| LD [IX],HL | CF,D5 | [IX]←L, [IX+1]←H | 5 | 2 | 142 |
| LD [IX],IX | CF,D6 | [IX]←IX(L), [IX+1]←IX(H) | 5 | 2 | 142 |
| LD [IX],IY | CF,D7 | [IX]←IY(L), [IX+1]←IY(H) | 5 | 2 | 142 |
| LD [IY],BA | CF,DC | [IY]←A, [IY+1]←B | 5 | 2 | 142 |
| LD [IY],HL | CF,DD | [IY]←L, [IY+1]←H | 5 | 2 | 142 |
| LD [IY],IX | CF,DE | [IY]←IX(L), [IY+1]←IX(H) | 5 | 2 | 142 |
| LD [IY],IY | CF,DF | [IY]←IY(L), [IY+1]←IY(H) | 5 | 2 | 142 |
| LD [SP+dd],BA | CF,74,dd | [SP+dd]←A, [SP+dd+1]←B | 6 | 3 | 143 |
| LD [SP+dd],HL | CF,75,dd | [SP+dd]←L, [SP+dd+1]←H | 6 | 3 | 143 |
| LD [SP+dd],IX | CF,76,dd | [SP+dd]←IX(L), [SP+dd+1]←IX(H) | 6 | 3 | 143 |
| LD [SP+dd],IY | CF,77,dd | [SP+dd]←IY(L), [SP+dd+1]←IY(H) | 6 | 3 | 143 |
| EX BA,HL | C8 | BA↔HL | 3 | 1 | 105 |
| EX BA,IX | C9 | BA↔IX | 3 | 1 | 105 |
| EX BA,IY | CA | BA↔IY | 3 | 1 | 105 |
| EX BA,SP | CB | BA↔SP | 3 | 1 | 105 |

## 8-bit Arithmetic and Logic Operation Instructions

**Flags by sub-class** (the `↕` columns):
- **ADD / ADC / SUB / SBC** (8-bit): set `Z C V N` per result; honor decimal/unpack (`#` in SC column). `D U I0 I1` unchanged.
- **AND / OR / XOR** (8-bit): set `Z N` per result; `C V` unchanged (logic ops). `SC,#nn` forms additionally reload SC.
- **CP** (compare = subtract, discard result): set `Z C V N`. `CP BR,#hh` likewise.
- **BIT** (test = AND, discard result): sets `Z` (and `N`); `C V` unchanged.
- **INC / DEC** (8-bit): set `Z V N`; **C is unchanged**. (Note: the table shows INC/DEC do not touch C.)
- **CPL**: sets `N` (and `Z`); per manual `V`/`C` rows show the operation marks—see §4.4 p.101.
- **NEG**: sets `Z C V N` (2's complement of operand).

> MODEL note: **MLT and DIV are MODEL1/3 only**; unavailable on MODEL0/2.

### ADD (8-bit) — page 47

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| ADD A,A | 00 | A←A+A | 2 | 1 | 67 |
| ADD A,B | 01 | A←A+B | 2 | 1 | 67 |
| ADD A,#nn | 02,nn | A←A+nn | 2 | 2 | 67 |
| ADD A,[BR:ll] | 04,ll | A←A+[BR:ll] | 3 | 2 | 67 |
| ADD A,[hhll] | 05,ll,hh | A←A+[hhll] | 4 | 3 | 68 |
| ADD A,[HL] | 03 | A←A+[HL] | 2 | 1 | 68 |
| ADD A,[IX] | 06 | A←A+[IX] | 2 | 1 | 68 |
| ADD A,[IY] | 07 | A←A+[IY] | 2 | 1 | 68 |
| ADD A,[IX+dd] | CE,00,dd | A←A+[IX+dd] | 4 | 3 | 69 |
| ADD A,[IY+dd] | CE,01,dd | A←A+[IY+dd] | 4 | 3 | 69 |
| ADD A,[IX+L] | CE,02 | A←A+[IX+L] | 4 | 2 | 69 |
| ADD A,[IY+L] | CE,03 | A←A+[IY+L] | 4 | 2 | 69 |
| ADD [HL],A | CE,04 | [HL]←[HL]+A | 4 | 2 | 70 |
| ADD [HL],#nn | CE,05,nn | [HL]←[HL]+nn | 5 | 3 | 70 |
| ADD [HL],[IX] | CE,06 | [HL]←[HL]+[IX] | 5 | 2 | 71 |
| ADD [HL],[IY] | CE,07 | [HL]←[HL]+[IY] | 5 | 2 | 71 |

### ADC (8-bit) — page 47

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| ADC A,A | 08 | A←A+A+C | 2 | 1 | 60 |
| ADC A,B | 09 | A←A+B+C | 2 | 1 | 60 |
| ADC A,#nn | 0A,nn | A←A+nn+C | 2 | 2 | 60 |
| ADC A,[BR:ll] | 0C,ll | A←A+[BR:ll]+C | 3 | 2 | 60 |
| ADC A,[hhll] | 0D,ll,hh | A←A+[hhll]+C | 4 | 3 | 61 |
| ADC A,[HL] | 0B | A←A+[HL]+C | 2 | 1 | 61 |
| ADC A,[IX] | 0E | A←A+[IX]+C | 2 | 1 | 62 |
| ADC A,[IY] | 0F | A←A+[IY]+C | 2 | 1 | 62 |
| ADC A,[IX+dd] | CE,08,dd | A←A+[IX+dd]+C | 4 | 3 | 62 |
| ADC A,[IY+dd] | CE,09,dd | A←A+[IY+dd]+C | 4 | 3 | 62 |
| ADC A,[IX+L] | CE,0A | A←A+[IX+L]+C | 4 | 2 | 63 |
| ADC A,[IY+L] | CE,0B | A←A+[IY+L]+C | 4 | 2 | 63 |
| ADC [HL],A | CE,0C | [HL]←[HL]+A+C | 4 | 2 | 63 |
| ADC [HL],#nn | CE,0D,nn | [HL]←[HL]+nn+C | 5 | 3 | 64 |
| ADC [HL],[IX] | CE,0E | [HL]←[HL]+[IX]+C | 5 | 2 | 64 |
| ADC [HL],[IY] | CE,0F | [HL]←[HL]+[IY]+C | 5 | 2 | 64 |

### SUB (8-bit) — page 47

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| SUB A,A | 10 | A←A-A | 2 | 1 | 180 |
| SUB A,B | 11 | A←A-B | 2 | 1 | 180 |
| SUB A,#nn | 12,nn | A←A-nn | 2 | 2 | 180 |
| SUB A,[BR:ll] | 14,ll | A←A-[BR:ll] | 3 | 2 | 180 |
| SUB A,[hhll] | 15,ll,hh | A←A-[hhll] | 4 | 3 | 181 |
| SUB A,[HL] | 13 | A←A-[HL] | 2 | 1 | 181 |
| SUB A,[IX] | 16 | A←A-[IX] | 2 | 1 | 181 |
| SUB A,[IY] | 17 | A←A-[IY] | 2 | 1 | 181 |
| SUB A,[IX+dd] | CE,10,dd | A←A-[IX+dd] | 4 | 3 | 182 |
| SUB A,[IY+dd] | CE,11,dd | A←A-[IY+dd] | 4 | 3 | 182 |
| SUB A,[IX+L] | CE,12 | A←A-[IX+L] | 4 | 2 | 182 |
| SUB A,[IY+L] | CE,13 | A←A-[IY+L] | 4 | 2 | 182 |
| SUB [HL],A | CE,14 | [HL]←[HL]-A | 4 | 2 | 183 |
| SUB [HL],#nn | CE,15,nn | [HL]←[HL]-nn | 5 | 3 | 183 |
| SUB [HL],[IX] | CE,16 | [HL]←[HL]-[IX] | 5 | 2 | 184 |
| SUB [HL],[IY] | CE,17 | [HL]←[HL]-[IY] | 5 | 2 | 184 |

### SBC (8-bit) — page 48

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| SBC A,A | 18 | A←A-A-C | 2 | 1 | 167 |
| SBC A,B | 19 | A←A-B-C | 2 | 1 | 167 |
| SBC A,#nn | 1A,nn | A←A-nn-C | 2 | 2 | 167 |
| SBC A,[BR:ll] | 1C,ll | A←A-[BR:ll]-C | 3 | 2 | 167 |
| SBC A,[hhll] | 1D,ll,hh | A←A-[hhll]-C | 4 | 3 | 168 |
| SBC A,[HL] | 1B | A←A-[HL]-C | 2 | 1 | 168 |
| SBC A,[IX] | 1E | A←A-[IX]-C | 2 | 1 | 168 |
| SBC A,[IY] | 1F | A←A-[IY]-C | 2 | 1 | 168 |
| SBC A,[IX+dd] | CE,18,dd | A←A-[IX+dd]-C | 4 | 3 | 169 |
| SBC A,[IY+dd] | CE,19,dd | A←A-[IY+dd]-C | 4 | 3 | 169 |
| SBC A,[IX+L] | CE,1A | A←A-[IX+L]-C | 4 | 2 | 169 |
| SBC A,[IY+L] | CE,1B | A←A-[IY+L]-C | 4 | 2 | 169 |
| SBC [HL],A | CE,1C | [HL]←[HL]-A-C | 4 | 2 | 170 |
| SBC [HL],#nn | CE,1D,nn | [HL]←[HL]-nn-C | 5 | 3 | 170 |
| SBC [HL],[IX] | CE,1E | [HL]←[HL]-[IX]-C | 5 | 2 | 171 |
| SBC [HL],[IY] | CE,1F | [HL]←[HL]-[IY]-C | 5 | 2 | 171 |

### AND (8-bit) — page 48

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| AND A,A | 20 | A←A∧A | 2 | 1 | 75 |
| AND A,B | 21 | A←A∧B | 2 | 1 | 75 |
| AND A,#nn | 22,nn | A←A∧nn | 2 | 2 | 75 |
| AND A,[BR:ll] | 24,ll | A←A∧[BR:ll] | 3 | 2 | 75 |
| AND A,[hhll] | 25,ll,hh | A←A∧[hhll] | 4 | 3 | 76 |
| AND A,[HL] | 23 | A←A∧[HL] | 2 | 1 | 76 |
| AND A,[IX] | 26 | A←A∧[IX] | 2 | 1 | 76 |
| AND A,[IY] | 27 | A←A∧[IY] | 2 | 1 | 76 |
| AND A,[IX+dd] | CE,20,dd | A←A∧[IX+dd] | 4 | 3 | 77 |
| AND A,[IY+dd] | CE,21,dd | A←A∧[IY+dd] | 4 | 3 | 77 |
| AND A,[IX+L] | CE,22 | A←A∧[IX+L] | 4 | 2 | 77 |
| AND A,[IY+L] | CE,23 | A←A∧[IY+L] | 4 | 2 | 77 |
| AND B,#nn | CE,B0,nn | B←B∧nn | 3 | 3 | 78 |
| AND L,#nn | CE,B1,nn | L←L∧nn | 3 | 3 | 78 |
| AND H,#nn | CE,B2,nn | H←H∧nn | 3 | 3 | 78 |
| AND SC,#nn | 9C,nn | SC←SC∧nn | 3 | 2 | 79 |
| AND [BR:ll],#nn | D8,ll,nn | [BR:ll]←[BR:ll]∧nn | 5 | 3 | 79 |
| AND [HL],A | CE,24 | [HL]←[HL]∧A | 4 | 2 | 79 |
| AND [HL],#nn | CE,25,nn | [HL]←[HL]∧nn | 5 | 3 | 80 |
| AND [HL],[IX] | CE,26 | [HL]←[HL]∧[IX] | 5 | 2 | 80 |
| AND [HL],[IY] | CE,27 | [HL]←[HL]∧[IY] | 5 | 2 | 80 |

### OR (8-bit) — page 48–49

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| OR A,A | 28 | A←A∨A | 2 | 1 | 149 |
| OR A,B | 29 | A←A∨B | 2 | 1 | 149 |
| OR A,#nn | 2A,nn | A←A∨nn | 2 | 2 | 149 |
| OR A,[BR:ll] | 2C,ll | A←A∨[BR:ll] | 3 | 2 | 150 |
| OR A,[hhll] | 2D,ll,hh | A←A∨[hhll] | 4 | 3 | 150 |
| OR A,[HL] | 2B | A←A∨[HL] | 2 | 1 | 150 |
| OR A,[IX] | 2E | A←A∨[IX] | 2 | 1 | 151 |
| OR A,[IY] | 2F | A←A∨[IY] | 2 | 1 | 151 |
| OR A,[IX+dd] | CE,28,dd | A←A∨[IX+dd] | 4 | 3 | 151 |
| OR A,[IY+dd] | CE,29,dd | A←A∨[IY+dd] | 4 | 3 | 151 |
| OR A,[IX+L] | CE,2A | A←A∨[IX+L] | 4 | 2 | 152 |
| OR A,[IY+L] | CE,2B | A←A∨[IY+L] | 4 | 2 | 152 |
| OR B,#nn | CE,B4,nn | B←B∨nn | 3 | 3 | 152 |
| OR L,#nn | CE,B5,nn | L←L∨nn | 3 | 3 | 152 |
| OR H,#nn | CE,B6,nn | H←H∨nn | 3 | 3 | 153 |
| OR SC,#nn | 9D,nn | SC←SC∨nn | 3 | 2 | 153 |
| OR [BR:ll],#nn | D9,ll,nn | [BR:ll]←[BR:ll]∨nn | 5 | 3 | 153 |
| OR [HL],A | CE,2C | [HL]←[HL]∨A | 4 | 2 | 154 |
| OR [HL],#nn | CE,2D,nn | [HL]←[HL]∨nn | 5 | 3 | 154 |
| OR [HL],[IX] | CE,2E | [HL]←[HL]∨[IX] | 5 | 2 | 154 |
| OR [HL],[IY] | CE,2F | [HL]←[HL]∨[IY] | 5 | 2 | 154 |

### XOR (8-bit) — page 49

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| XOR A,A | 38 | A←A∀A | 2 | 1 | 189 |
| XOR A,B | 39 | A←A∀B | 2 | 1 | 189 |
| XOR A,#nn | 3A,nn | A←A∀nn | 2 | 2 | 189 |
| XOR A,[BR:ll] | 3C,ll | A←A∀[BR:ll] | 3 | 2 | 189 |
| XOR A,[hhll] | 3D,ll,hh | A←A∀[hhll] | 4 | 3 | 190 |
| XOR A,[HL] | 3B | A←A∀[HL] | 2 | 1 | 190 |
| XOR A,[IX] | 3E | A←A∀[IX] | 2 | 1 | 190 |
| XOR A,[IY] | 3F | A←A∀[IY] | 2 | 1 | 190 |
| XOR A,[IX+dd] | CE,38,dd | A←A∀[IX+dd] | 4 | 3 | 191 |
| XOR A,[IY+dd] | CE,39,dd | A←A∀[IY+dd] | 4 | 3 | 191 |
| XOR A,[IX+L] | CE,3A | A←A∀[IX+L] | 4 | 2 | 191 |
| XOR A,[IY+L] | CE,3B | A←A∀[IY+L] | 4 | 2 | 191 |
| XOR B,#nn | CE,B8,nn | B←B∀nn | 3 | 3 | 192 |
| XOR L,#nn | CE,B9,nn | L←L∀nn | 3 | 3 | 192 |
| XOR H,#nn | CE,BA,nn | H←H∀nn | 3 | 3 | 192 |
| XOR SC,#nn | 9E,nn | SC←SC∀nn | 3 | 2 | 193 |
| XOR [BR:ll],#nn | DA,ll,nn | [BR:ll]←[BR:ll]∀nn | 5 | 3 | 193 |
| XOR [HL],A | CE,3C | [HL]←[HL]∀A | 4 | 2 | 193 |
| XOR [HL],#nn | CE,3D,nn | [HL]←[HL]∀nn | 5 | 3 | 194 |
| XOR [HL],[IX] | CE,3E | [HL]←[HL]∀[IX] | 5 | 2 | 194 |
| XOR [HL],[IY] | CE,3F | [HL]←[HL]∀[IY] | 5 | 2 | 194 |

### CP (8-bit compare) — page 49–50

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| CP A,A | 30 | A-A | 2 | 1 | 90 |
| CP A,B | 31 | A-B | 2 | 1 | 90 |
| CP A,#nn | 32,nn | A-nn | 2 | 2 | 90 |
| CP A,[BR:ll] | 34,ll | A-[BR:ll] | 3 | 2 | 90 |
| CP A,[hhll] | 35,ll,hh | A-[hhll] | 4 | 3 | 91 |
| CP A,[HL] | 33 | A-[HL] | 2 | 1 | 91 |
| CP A,[IX] | 36 | A-[IX] | 2 | 1 | 92 |
| CP A,[IY] | 37 | A-[IY] | 2 | 1 | 92 |
| CP A,[IX+dd] | CE,30,dd | A-[IX+dd] | 4 | 3 | 92 |
| CP A,[IY+dd] | CE,31,dd | A-[IY+dd] | 4 | 3 | 92 |
| CP A,[IX+L] | CE,32 | A-[IX+L] | 4 | 2 | 93 |
| CP A,[IY+L] | CE,33 | A-[IY+L] | 4 | 2 | 93 |
| CP B,#nn | CE,BC,nn | B-nn | 3 | 3 | 93 |
| CP L,#nn | CE,BD,nn | L-nn | 3 | 3 | 94 |
| CP H,#nn | CE,BE,nn | H-nn | 3 | 3 | 94 |
| CP BR,#hh | CE,BF,hh | BR-hh | 3 | 3 | 94 |
| CP [BR:ll],#nn | DB,ll,nn | [BR:ll]-nn | 5 | 3 | 95 |
| CP [HL],A | CE,34 | [HL]-A | 3 | 2 | 95 |
| CP [HL],#nn | CE,35,nn | [HL]-nn | 4 | 3 | 96 |
| CP [HL],[IX] | CE,36 | [HL]-[IX] | 4 | 2 | 96 |
| CP [HL],[IY] | CE,37 | [HL]-[IY] | 4 | 2 | 96 |

### BIT / INC / DEC / CPL / NEG / MLT / DIV (8-bit) — page 50

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| BIT A,B | 94 | A∧B | 2 | 1 | 81 |
| BIT A,#nn | 96,nn | A∧nn | 2 | 2 | 81 |
| BIT B,#nn | 97,nn | B∧nn | 2 | 2 | 81 |
| BIT [BR:ll],#nn | DC,ll,nn | [BR:ll]∧nn | 4 | 3 | 82 |
| BIT [HL],#nn | 95,nn | [HL]∧nn | 3 | 2 | 82 |
| INC A | 80 | A←A+1 | 2 | 1 | 106 |
| INC B | 81 | B←B+1 | 2 | 1 | 106 |
| INC L | 82 | L←L+1 | 2 | 1 | 106 |
| INC H | 83 | H←H+1 | 2 | 1 | 106 |
| INC BR | 84 | BR←BR+1 | 2 | 1 | 106 |
| INC [BR:ll] | 85,ll | [BR:ll]←[BR:ll]+1 | 4 | 2 | 107 |
| INC [HL] | 86 | [HL]←[HL]+1 | 3 | 1 | 107 |
| DEC A | 88 | A←A-1 | 2 | 1 | 102 |
| DEC B | 89 | B←B-1 | 2 | 1 | 102 |
| DEC L | 8A | L←L-1 | 2 | 1 | 102 |
| DEC H | 8B | H←H-1 | 2 | 1 | 102 |
| DEC BR | 8C | BR←BR-1 | 2 | 1 | 102 |
| DEC [BR:ll] | 8D,ll | [BR:ll]←[BR:ll]-1 | 4 | 2 | 102 |
| DEC [HL] | 8E | [HL]←[HL]-1 | 3 | 1 | 103 |
| CPL A | CE,A0 | A←A (1's complement) | 3 | 2 | 101 |
| CPL B | CE,A1 | B←B (1's complement) | 3 | 2 | 101 |
| CPL [BR:ll] | CE,A2,ll | [BR:ll]←~[BR:ll] | 5 | 3 | 101 |
| CPL [HL] | CE,A3 | [HL]←~[HL] | 4 | 2 | 101 |
| NEG A | CE,A4 | A←0-A | 3 | 2 | 148 |
| NEG B | CE,A5 | B←0-B | 3 | 2 | 148 |
| NEG [BR:ll] | CE,A6,ll | [BR:ll]←0-[BR:ll] | 5 | 3 | 148 |
| NEG [HL] | CE,A7 | [HL]←0-[HL] | 4 | 2 | 148 |
| MLT | CE,D8 | HL←L*A | 12 | 2 | 147 (MODEL1/3) |
| DIV | CE,D9 | L←HL/A, H←Remainder | 13 | 2 | 104 (MODEL1/3) |

> Note (CPL): operand byte for `CPL [BR:ll]` is `ll`; the manual's `Operation` shows the destination unchanged-notation (`A←A`), meaning 1's-complement in place. See §4.4 p.101.

## 16-bit Arithmetic Operation Instructions

**Flags:** ADD/ADC/SUB/SBC/CP (16-bit) set `Z C V N` per result. INC/DEC (16-bit) set `Z V N`, leave `C` unchanged. `D U I0 I1` unchanged throughout.

### ADD (16-bit) — page 51

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| ADD BA,BA | CF,00 | BA←BA+BA | 4 | 2 | 71 |
| ADD BA,HL | CF,01 | BA←BA+HL | 4 | 2 | 71 |
| ADD BA,IX | CF,02 | BA←BA+IX | 4 | 2 | 71 |
| ADD BA,IY | CF,03 | BA←BA+IY | 4 | 2 | 71 |
| ADD BA,#mmnn | C0,nn,mm | BA←BA+mmnn | 3 | 3 | 72 |
| ADD HL,BA | CF,20 | HL←HL+BA | 4 | 2 | 72 |
| ADD HL,HL | CF,21 | HL←HL+HL | 4 | 2 | 72 |
| ADD HL,IX | CF,22 | HL←HL+IX | 4 | 2 | 72 |
| ADD HL,IY | CF,23 | HL←HL+IY | 4 | 2 | 72 |
| ADD HL,#mmnn | C1,nn,mm | HL←HL+mmnn | 3 | 3 | 72 |
| ADD IX,BA | CF,40 | IX←IX+BA | 4 | 2 | 73 |
| ADD IX,HL | CF,41 | IX←IX+HL | 4 | 2 | 73 |
| ADD IX,#mmnn | C2,nn,mm | IX←IX+mmnn | 3 | 3 | 73 |
| ADD IY,BA | CF,42 | IY←IY+BA | 4 | 2 | 73 |
| ADD IY,HL | CF,43 | IY←IY+HL | 4 | 2 | 73 |
| ADD IY,#mmnn | C3,nn,mm | IY←IY+mmnn | 3 | 3 | 74 |
| ADD SP,BA | CF,44 | SP←SP+BA | 4 | 2 | 74 |
| ADD SP,HL | CF,45 | SP←SP+HL | 4 | 2 | 74 |
| ADD SP,#mmnn | CF,68,nn,mm | SP←SP+mmnn | 4 | 4 | 74 |

### ADC (16-bit) — page 51

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| ADC BA,BA | CF,04 | BA←BA+BA+C | 4 | 2 | 65 |
| ADC BA,HL | CF,05 | BA←BA+HL+C | 4 | 2 | 65 |
| ADC BA,IX | CF,06 | BA←BA+IX+C | 4 | 2 | 65 |
| ADC BA,IY | CF,07 | BA←BA+IY+C | 4 | 2 | 65 |
| ADC BA,#mmnn | CF,60,nn,mm | BA←BA+mmnn+C | 4 | 4 | 65 |
| ADC HL,BA | CF,24 | HL←HL+BA+C | 4 | 2 | 66 |
| ADC HL,HL | CF,25 | HL←HL+HL+C | 4 | 2 | 66 |
| ADC HL,IX | CF,26 | HL←HL+IX+C | 4 | 2 | 66 |
| ADC HL,IY | CF,27 | HL←HL+IY+C | 4 | 2 | 66 |
| ADC HL,#mmnn | CF,61,nn,mm | HL←HL+mmnn+C | 4 | 4 | 66 |

### SUB (16-bit) — page 51

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| SUB BA,BA | CF,08 | BA←BA-BA | 4 | 2 | 184 |
| SUB BA,HL | CF,09 | BA←BA-HL | 4 | 2 | 184 |
| SUB BA,IX | CF,0A | BA←BA-IX | 4 | 2 | 184 |
| SUB BA,IY | CF,0B | BA←BA-IY | 4 | 2 | 184 |
| SUB BA,#mmnn | D0,nn,mm | BA←BA-mmnn | 3 | 3 | 185 |
| SUB HL,BA | CF,28 | HL←HL-BA | 4 | 2 | 185 |
| SUB HL,HL | CF,29 | HL←HL-HL | 4 | 2 | 185 |
| SUB HL,IX | CF,2A | HL←HL-IX | 4 | 2 | 185 |
| SUB HL,IY | CF,2B | HL←HL-IY | 4 | 2 | 185 |
| SUB HL,#mmnn | D1,nn,mm | HL←HL-mmnn | 3 | 3 | 185 |
| SUB IX,BA | CF,48 | IX←IX-BA | 4 | 2 | 186 |
| SUB IX,HL | CF,49 | IX←IX-HL | 4 | 2 | 186 |
| SUB IX,#mmnn | D2,nn,mm | IX←IX-mmnn | 3 | 3 | 186 |
| SUB IY,BA | CF,4A | IY←IY-BA | 4 | 2 | 186 |
| SUB IY,HL | CF,4B | IY←IY-HL | 4 | 2 | 186 |
| SUB IY,#mmnn | D3,nn,mm | IY←IY-mmnn | 3 | 3 | 187 |
| SUB SP,BA | CF,4C | SP←SP-BA | 4 | 2 | 187 |
| SUB SP,HL | CF,4D | SP←SP-HL | 4 | 2 | 187 |
| SUB SP,#mmnn | CF,6A,nn,mm | SP←SP-mmnn | 4 | 4 | 187 |

### SBC / CP / INC / DEC (16-bit) — page 52

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| SBC BA,BA | CF,0C | BA←BA-BA-C | 4 | 2 | 171 |
| SBC BA,HL | CF,0D | BA←BA-HL-C | 4 | 2 | 171 |
| SBC BA,IX | CF,0E | BA←BA-IX-C | 4 | 2 | 171 |
| SBC BA,IY | CF,0F | BA←BA-IY-C | 4 | 2 | 171 |
| SBC BA,#mmnn | CF,62,nn,mm | BA←BA-mmnn-C | 4 | 4 | 172 |
| SBC HL,BA | CF,2C | HL←HL-BA-C | 4 | 2 | 172 |
| SBC HL,HL | CF,2D | HL←HL-HL-C | 4 | 2 | 172 |
| SBC HL,IX | CF,2E | HL←HL-IX-C | 4 | 2 | 172 |
| SBC HL,IY | CF,2F | HL←HL-IY-C | 4 | 2 | 172 |
| SBC HL,#mmnn | CF,63,nn,mm | HL←HL-mmnn-C | 4 | 4 | 172 |
| CP BA,BA | CF,18 | BA-BA | 4 | 2 | 97 |
| CP BA,HL | CF,19 | BA-HL | 4 | 2 | 97 |
| CP BA,IX | CF,1A | BA-IX | 4 | 2 | 97 |
| CP BA,IY | CF,1B | BA-IY | 4 | 2 | 97 |
| CP BA,#mmnn | D4,nn,mm | BA-mmnn | 3 | 3 | 97 |
| CP HL,BA | CF,38 | HL-BA | 4 | 2 | 98 |
| CP HL,HL | CF,39 | HL-HL | 4 | 2 | 98 |
| CP HL,IX | CF,3A | HL-IX | 4 | 2 | 98 |
| CP HL,IY | CF,3B | HL-IY | 4 | 2 | 98 |
| CP HL,#mmnn | D5,nn,mm | HL-mmnn | 3 | 3 | 98 |
| CP IX,#mmnn | D6,nn,mm | IX-mmnn | 3 | 3 | 99 |
| CP IY,#mmnn | D7,nn,mm | IY-mmnn | 3 | 3 | 99 |
| CP SP,BA | CF,5C | SP-BA | 4 | 2 | 100 |
| CP SP,HL | CF,5D | SP-HL | 4 | 2 | 100 |
| CP SP,#mmnn | CF,6C,nn,mm | SP-mmnn | 4 | 4 | 100 |
| INC BA | 90 | BA←BA+1 | 2 | 1 | 107 |
| INC HL | 91 | HL←HL+1 | 2 | 1 | 107 |
| INC IX | 92 | IX←IX+1 | 2 | 1 | 107 |
| INC IY | 93 | IY←IY+1 | 2 | 1 | 107 |
| INC SP | 87 | SP←SP+1 | 2 | 1 | 108 |
| DEC BA | 98 | BA←BA-1 | 2 | 1 | 103 |
| DEC HL | 99 | HL←HL-1 | 2 | 1 | 103 |
| DEC IX | 9A | IX←IX-1 | 2 | 1 | 103 |
| DEC IY | 9B | IY←IY-1 | 2 | 1 | 103 |
| DEC SP | 8F | SP←SP-1 | 2 | 1 | 103 |

## Auxiliary Operation Instructions (PACK / UPCK / SEP)

**Flags:** PACK sets `U`(unpack)-related state; UPCK sets `U`; SEP sets `N`. (The §4.3.3 extraction of this block was garbled — see "Known garbled spots" above. Opcodes/byte/cycle recovered from Appendix A + Appendix B.)

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| PACK | DE | pack A: A(H)/A(L) BCD-style pack (see §4.4) | 2 | 1 | 155 |
| UPCK | DF | unpack A into nibbles (see §4.4) | 2 | 1 | 188 |
| SEP | CE,A8 | sign-extend / code extension of A into B (see §4.4) | 3 | 2 | 173 |

> The garbled operation diagrams on PDF p.52 suggest: PACK combines two unpacked nibbles `0m 0n → mn`; UPCK splits a byte `mn → 0m 0n`; SEP sign-extends A's MSB across B (`B ← A bit7 ? 0xFF : 0x00`). Treat these as approximate; **confirm against §4.4 pp.155/188/173.**

## Rotate / Shift Instructions

**Flags:** RL/RLC/RR/RRC set `C` (rotated-out bit) and `Z`; `N` per result. SLA/SLL/SRA/SRL set `C` and `Z N`; SRL/SLL force the vacated bit / `N` to 0 (logical). `V` unchanged. Operand byte `ll` present only for the `[BR:ll]` forms.

| Mnemonic | Machine Code | Cyc | Byt | §4.4 pg |
|----------|--------------|----:|----:|--------:|
| RL A | CE,90 | 3 | 2 | 162 |
| RL B | CE,91 | 3 | 2 | 162 |
| RL [BR:ll] | CE,92,ll | 5 | 3 | 163 |
| RL [HL] | CE,93 | 4 | 2 | 163 |
| RLC A | CE,94 | 3 | 2 | 163 |
| RLC B | CE,95 | 3 | 2 | 163 |
| RLC [BR:ll] | CE,96,ll | 5 | 3 | 164 |
| RLC [HL] | CE,97 | 4 | 2 | 164 |
| RR A | CE,98 | 3 | 2 | 164 |
| RR B | CE,99 | 3 | 2 | 164 |
| RR [BR:ll] | CE,9A,ll | 5 | 3 | 165 |
| RR [HL] | CE,9B | 4 | 2 | 165 |
| RRC A | CE,9C | 3 | 2 | 166 |
| RRC B | CE,9D | 3 | 2 | 166 |
| RRC [BR:ll] | CE,9E,ll | 5 | 3 | 166 |
| RRC [HL] | CE,9F | 4 | 2 | 166 |
| SLA A | CE,80 | 3 | 2 | 173 |
| SLA B | CE,81 | 3 | 2 | 173 |
| SLA [BR:ll] | CE,82,ll | 5 | 3 | 174 |
| SLA [HL] | CE,83 | 4 | 2 | 174 |
| SLL A | CE,84 | 3 | 2 | 175 |
| SLL B | CE,85 | 3 | 2 | 175 |
| SLL [BR:ll] | CE,86,ll | 5 | 3 | 175 |
| SLL [HL] | CE,87 | 4 | 2 | 176 |
| SRA A | CE,88 | 3 | 2 | 177 |
| SRA B | CE,89 | 3 | 2 | 177 |
| SRA [BR:ll] | CE,8A,ll | 5 | 3 | 177 |
| SRA [HL] | CE,8B | 4 | 2 | 178 |
| SRL A | CE,8C | 3 | 2 | 178 |
| SRL B | CE,8D | 3 | 2 | 178 |
| SRL [BR:ll] | CE,8E,ll | 5 | 3 | 179 |
| SRL [HL] | CE,8F | 4 | 2 | 179 |

> Bit-flow (from the per-op diagrams): RL/RR rotate through carry C; RLC/RRC rotate within the 8 bits (carry copied but not in the loop — per manual diagrams `C ← bit out`); SLA/SLL shift left inserting 0 at bit0; SRA shifts right preserving bit7 (arithmetic), SRL shifts right inserting 0 at bit7. Verify exact carry coupling in §4.4.

## Stack Control Instructions (PUSH / POP)

**Flags:** PUSH leaves all flags unchanged. POP leaves flags unchanged **except** `POP SC` (and the `ALL`/`ALE` group, which reload SC) reload the SC flag register. `EP`/`IP`(=XP,YP) forms are MODEL2/3 only.

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| PUSH A | CF,B0 | [SP-1]←A, SP←SP-1 | 3 | 2 | 158 |
| PUSH B | CF,B1 | [SP-1]←B, SP←SP-1 | 3 | 2 | 158 |
| PUSH L | CF,B2 | [SP-1]←L, SP←SP-1 | 3 | 2 | 158 |
| PUSH H | CF,B3 | [SP-1]←H, SP←SP-1 | 3 | 2 | 158 |
| PUSH BR | A4 | [SP-1]←BR, SP←SP-1 | 3 | 1 | 159 |
| PUSH SC | A7 | [SP-1]←SC, SP←SP-1 | 3 | 1 | 160 |
| PUSH BA | A0 | [SP-1]←B, [SP-2]←A, SP←SP-2 | 4 | 1 | 158 |
| PUSH HL | A1 | [SP-1]←H, [SP-2]←L, SP←SP-2 | 4 | 1 | 158 |
| PUSH IX | A2 | [SP-1]←IX(H), [SP-2]←IX(L), SP←SP-2 | 4 | 1 | 158 |
| PUSH IY | A3 | [SP-1]←IY(H), [SP-2]←IY(L), SP←SP-2 | 4 | 1 | 158 |
| PUSH EP | A5 | [SP-1]←EP, SP←SP-1 | 3 | 1 | 159 (MODEL2/3) |
| PUSH IP | A6 | [SP-1]←XP, [SP-2]←YP, SP←SP-2 | 4 | 1 | 159 (MODEL2/3) |
| PUSH ALL | CF,B8 | PUSH BA, HL, IX, IY, BR | 12 | 2 | 160 |
| PUSH ALE | CF,B9 | PUSH BA, HL, IX, IY, BR, EP, IP | 15 | 2 | 160 |
| POP A | CF,B4 | A←[SP], SP←SP+1 | 3 | 2 | 155 |
| POP B | CF,B5 | B←[SP], SP←SP+1 | 3 | 2 | 155 |
| POP L | CF,B6 | L←[SP], SP←SP+1 | 3 | 2 | 155 |
| POP H | CF,B7 | H←[SP], SP←SP+1 | 3 | 2 | 155 |
| POP BR | AC | BR←[SP], SP←SP+1 | 2 | 1 | 156 |
| POP SC | AF | SC←[SP], SP←SP+1 | 2 | 1 | 156 |
| POP BA | A8 | A←[SP], B←[SP+1], SP←SP+2 | 3 | 1 | 155 |
| POP HL | A9 | L←[SP], H←[SP+1], SP←SP+2 | 3 | 1 | 155 |
| POP IX | AA | IX(L)←[SP], IX(H)←[SP+1], SP←SP+2 | 3 | 1 | 155 |
| POP IY | AB | IY(L)←[SP], IY(H)←[SP+1], SP←SP+2 | 3 | 1 | 155 |
| POP EP | AD | EP←[SP], SP←SP+1 | 2 | 1 | 156 (MODEL2/3) |
| POP IP | AE | YP←[SP], XP←[SP+1], SP←SP+2 | 3 | 1 | 156 (MODEL2/3) |
| POP ALL | CF,BC | POP BR, IY, IX, HL, BA | 11 | 2 | 157 |
| POP ALE | CF,BD | POP IP, EP, BR, IY, IX, HL, BA | 14 | 2 | 157 |

## Branch Instructions

Branch timing varies by core model and (for calls) minimum/maximum mode and whether the branch is taken. Cycle counts below: where two/three numbers appear they are MODEL0/1 / MODEL2/3 (min) / MODEL2/3 (max), per the manual. Conditions reference: `LT = [N∀V]=1`, `LE = Z∨[N∀V]=1`, `GT = Z∨[N∀V]=0`, `GE = [N∀V]=0`, `P = N=0`, `M = N=1`.

**Flags:** branch/jump/call leave flags unchanged, **except** `DJR NZ,rr` which decrements B and sets the SC `↕` (its B-test), and `RETS` which reloads SC (`SC=↕`). RET/RETE also restore context; RETE pops SC.

### Relative short/long jump & loop — page 55

| Mnemonic | Machine Code | Condition | Byt | Cyc (taken) | §4.4 pg |
|----------|--------------|-----------|----:|------------|--------:|
| JRS rr | F1,rr | unconditional | 2 | 2 (M0/1) / 2 (M2/3) | 112 |
| JRS C,rr | E4,rr | C=1 | 2 | 2 / 2 (taken) | 113 |
| JRS NC,rr | E5,rr | C=0 | 2 | 2 / 2 | 113 |
| JRS Z,rr | E6,rr | Z=1 | 2 | 2 / 2 | 113 |
| JRS NZ,rr | E7,rr | Z=0 | 2 | 2 / 2 | 113 |
| JRS LT,rr | CE,E0,rr | [N∀V]=1 | 3 | 3 / 3 | 113 |
| JRS LE,rr | CE,E1,rr | Z∨[N∀V]=1 | 3 | 3 / 3 | 113 |
| JRS GT,rr | CE,E2,rr | Z∨[N∀V]=0 | 3 | 3 / 3 | 113 |
| JRS GE,rr | CE,E3,rr | [N∀V]=0 | 3 | 3 / 3 | 113 |
| JRS V,rr | CE,E4,rr | V=1 | 3 | 3 / 3 | 113 |
| JRS NV,rr | CE,E5,rr | V=0 | 3 | 3 / 3 | 113 |
| JRS P,rr | CE,E6,rr | N=0 | 3 | 3 / 3 | 113 |
| JRS M,rr | CE,E7,rr | N=1 | 3 | 3 / 3 | 113 |
| JRS F0,rr | CE,E8,rr | F0=1 | 3 | 3 / 3 | 113 |
| JRS F1,rr | CE,E9,rr | F1=1 | 3 | 3 / 3 | 113 |
| JRS F2,rr | CE,EA,rr | F2=1 | 3 | 3 / 3 | 113 |
| JRS F3,rr | CE,EB,rr | F3=1 | 3 | 3 / 3 | 113 |
| JRS NF0,rr | CE,EC,rr | F0=0 | 3 | 3 / 3 | 113 |
| JRS NF1,rr | CE,ED,rr | F1=0 | 3 | 3 / 3 | 113 |
| JRS NF2,rr | CE,EE,rr | F2=0 | 3 | 3 / 3 | 113 |
| JRS NF3,rr | CE,EF,rr | F3=0 | 3 | 3 / 3 | 113 |
| JRL qqrr | F3,rr,qq | unconditional | 3 | 3 / 3 | 110 |
| JRL C,qqrr | EC,rr,qq | C=1 | 3 | 3 / 3 | 111 |
<!-- opcode assignments verified against skiploom src/util/s1c88.csv -->
| JRL NC,qqrr | ED,rr,qq | C=0 | 3 | 3 / 3 | 111 |
| JRL Z,qqrr | EE,rr,qq | Z=1 | 3 | 3 / 3 | 111 |
| JRL NZ,qqrr | EF,rr,qq | Z=0 | 3 | 3 / 3 | 111 |
| DJR NZ,rr | F5,rr | B≠0 after B←B-1 | 2 | 4 / 4 | 104 |

> **Opcode assignments (verified against skiploom `src/util/s1c88.csv`):** the §4.3.3 extracted opcode column for these short jumps was shifted by one row; the correct unprefixed assignments are `E0–E3 = CARS C/NC/Z/NZ,rr`, `E4–E7 = JRS C/NC/Z/NZ,rr`, `E8–EB = CARL C/NC/Z/NZ,qqrr`, `EC–EF = JRL C/NC/Z/NZ,qqrr`, and the unconditional forms are `F0 = CARS rr`, `F1 = JRS rr`, `F2 = CARL qqrr`, `F3 = JRL qqrr`. Tables here use these corrected values.
> Semantics: `JRS rr → PC←PC+rr+1`; conditional taken → `PC←PC+rr+1` else `PC←PC+2`. `JRL → PC←PC+qqrr+2` (else +3). On MODEL2/3 each also does `CB←NB` on taken / `NB←CB` on fall-through. `DJR NZ,rr → B←B-1; if B≠0 then PC←PC+rr+1 else PC←PC+2`.

### Indirect jump / relative short call — page 56

| Mnemonic | Machine Code | Condition | Byt | §4.4 pg |
|----------|--------------|-----------|----:|--------:|
| JP HL | F4 | unconditional → PC←HL | 1 | 109 |
| JP [kk] | FD,kk | unconditional → PC(L)←[00kk], PC(H)←[00kk+1] | 2 | 109 |
| CARS rr | F0,rr | unconditional | 2 | 86 |
| CARS C,rr | E0,rr | C=1 | 2 | 87 |
| CARS NC,rr | E1,rr | C=0 | 2 | 87 |
| CARS Z,rr | E2,rr | Z=1 | 2 | 87 |
| CARS NZ,rr | E3,rr | Z=0 | 2 | 87 |
| CARS LT,rr | CE,F0,rr | [N∀V]=1 | 3 | 88 |
| CARS LE,rr | CE,F1,rr | Z∨[N∀V]=1 | 3 | 88 |
| CARS GT,rr | CE,F2,rr | Z∨[N∀V]=0 | 3 | 88 |
| CARS GE,rr | CE,F3,rr | [N∀V]=0 | 3 | 88 |
| CARS V,rr | CE,F4,rr | V=1 | 3 | 88 |
| CARS NV,rr | CE,F5,rr | V=0 | 3 | 88 |
| CARS P,rr | CE,F6,rr | N=0 | 3 | 88 |
| CARS M,rr | CE,F7,rr | N=1 | 3 | 88 |
| CARS F0,rr | CE,F8,rr | F0=1 | 3 | 88 |
| CARS F1,rr | CE,F9,rr | F1=1 | 3 | 88 |
| CARS F2,rr | CE,FA,rr | F2=1 | 3 | 88 |
| CARS F3,rr | CE,FB,rr | F3=1 | 3 | 88 |
| CARS NF0,rr | CE,FC,rr | F0=0 | 3 | 88 |
| CARS NF1,rr | CE,FD,rr | F1=0 | 3 | 88 |
| CARS NF2,rr | CE,FE,rr | F2=0 | 3 | 88 |
| CARS NF3,rr | CE,FF,rr | F3=0 | 3 | 88 |

> CARS pushes return PC then `PC←PC+rr+1`. On MODEL2/3 max mode it additionally pushes CB (3-byte stack frame) and sets `CB←NB`. Opcodes verified via skiploom CSV: `CARS C/NC/Z/NZ = E0/E1/E2/E3`, unconditional `CARS rr = F0`.

### Relative long call / indirect call — page 57

| Mnemonic | Machine Code | Condition | Byt | §4.4 pg |
|----------|--------------|-----------|----:|--------:|
| CARL qqrr | F2,rr,qq | unconditional | 3 | 84 |
| CARL C,qqrr | E8,rr,qq | C=1 | 3 | 85 |
| CARL NC,qqrr | E9,rr,qq | C=0 | 3 | 85 |
| CARL Z,qqrr | EA,rr,qq | Z=1 | 3 | 85 |
| CARL NZ,qqrr | EB,rr,qq | Z=0 | 3 | 85 |
| CALL [hhll] | FB,ll,hh | unconditional | 3 | 83 |

> CARL pushes return PC then `PC←PC+qqrr+2`. CALL pushes return PC then `PC(L)←[hhll], PC(H)←[hhll+1]`. MODEL2/3 max mode pushes CB and sets CB←NB.

### Software interrupt & returns — page 58

| Mnemonic | Machine Code | Operation | Byt | §4.4 pg |
|----------|--------------|-----------|----:|--------:|
| INT [kk] | FC,kk | push PC + SC; PC(L)←[00kk], PC(H)←[00kk+1] (max mode also pushes CB) | 2 | 108 |
| RET | F8 | PC(L)←[SP], PC(H)←[SP+1], SP←SP+2 (max: also CB←[SP+2], NB←CB, SP←SP+3) | 1 | 161 |
| RETE | F9 | SC←[SP], PC(L)←[SP+1], PC(H)←[SP+2], SP←SP+3 (max: also CB←[SP+3], NB←CB, SP←SP+4) | 1 | 161 |
| RETS | FA | PC(L)←[SP], PC(H)←[SP+1], SP←SP+2, PC←PC+2 (max: also CB←[SP+2], NB←CB, SP←SP+3) | 1 | 162 |

> **Flags:** RETE and RETS reload SC from the stack (`SC=↕`). RET does not. Cycle counts: INT 7/8; RET 3/4; RETE 4/5; RETS 5/6 (MODEL0/1 or M2/3-min / M2/3-max).

## System Control Instructions — page 58

**Flags:** none affected.

| Mnemonic | Machine Code | Operation | Cyc | Byt | §4.4 pg |
|----------|--------------|-----------|----:|----:|--------:|
| NOP | FF | No operation | 2 | 1 | 149 |
| HALT | CE,AE | enter HALT mode | 3 | 2 | 106 |
| SLP | CE,AF | enter SLEEP mode | 3 | 2 | 176 |

---

# Appendix A — Operation Code Map (PDF pp. 195–197)

The map is a 16×16 grid: high nibble across (columns 0–F), low nibble down (rows 0–F), so each cell's opcode = `(column<<4) | row`. Three pages: the **unprefixed** page (first opcode byte), the **`CE`-prefixed** page (second byte when first = `CE`), and the **`CF`-prefixed** page (second byte when first = `CF`). Below, each page is given as sequential opcode tables (`00`…`FF`) reconstructed from the grid (read column-major, which matches sequential opcode order — verified against §4.3.3).

## A.1 Unprefixed page (1st operation code)

| Op | Instr | Op | Instr | Op | Instr | Op | Instr |
|----|-------|----|-------|----|-------|----|-------|
| 00 | ADD A,A | 01 | ADD A,B | 02 | ADD A,#nn | 03 | ADD A,[HL] |
| 04 | ADD A,[BR:ll] | 05 | ADD A,[hhll] | 06 | ADD A,[IX] | 07 | ADD A,[IY] |
| 08 | ADC A,A | 09 | ADC A,B | 0A | ADC A,#nn | 0B | ADC A,[HL] |
| 0C | ADC A,[BR:ll] | 0D | ADC A,[hhll] | 0E | ADC A,[IX] | 0F | ADC A,[IY] |
| 10 | SUB A,A | 11 | SUB A,B | 12 | SUB A,#nn | 13 | SUB A,[HL] |
| 14 | SUB A,[BR:ll] | 15 | SUB A,[hhll] | 16 | SUB A,[IX] | 17 | SUB A,[IY] |
| 18 | SBC A,A | 19 | SBC A,B | 1A | SBC A,#nn | 1B | SBC A,[HL] |
| 1C | SBC A,[BR:ll] | 1D | SBC A,[hhll] | 1E | SBC A,[IX] | 1F | SBC A,[IY] |
| 20 | AND A,A | 21 | AND A,B | 22 | AND A,#nn | 23 | AND A,[HL] |
| 24 | AND A,[BR:ll] | 25 | AND A,[hhll] | 26 | AND A,[IX] | 27 | AND A,[IY] |
| 28 | OR A,A | 29 | OR A,B | 2A | OR A,#nn | 2B | OR A,[HL] |
| 2C | OR A,[BR:ll] | 2D | OR A,[hhll] | 2E | OR A,[IX] | 2F | OR A,[IY] |
| 30 | CP A,A | 31 | CP A,B | 32 | CP A,#nn | 33 | CP A,[HL] |
| 34 | CP A,[BR:ll] | 35 | CP A,[hhll] | 36 | CP A,[IX] | 37 | CP A,[IY] |
| 38 | XOR A,A | 39 | XOR A,B | 3A | XOR A,#nn | 3B | XOR A,[HL] |
| 3C | XOR A,[BR:ll] | 3D | XOR A,[hhll] | 3E | XOR A,[IX] | 3F | XOR A,[IY] |
| 40 | LD A,A | 41 | LD A,B | 42 | LD A,L | 43 | LD A,H |
| 44 | LD A,[BR:ll] | 45 | LD A,[HL] | 46 | LD A,[IX] | 47 | LD A,[IY] |
| 48 | LD B,A | 49 | LD B,B | 4A | LD B,L | 4B | LD B,H |
| 4C | LD B,[BR:ll] | 4D | LD B,[HL] | 4E | LD B,[IX] | 4F | LD B,[IY] |
| 50 | LD L,A | 51 | LD L,B | 52 | LD L,L | 53 | LD L,H |
| 54 | LD L,[BR:ll] | 55 | LD L,[HL] | 56 | LD L,[IX] | 57 | LD L,[IY] |
| 58 | LD H,A | 59 | LD H,B | 5A | LD H,L | 5B | LD H,H |
| 5C | LD H,[BR:ll] | 5D | LD H,[HL] | 5E | LD H,[IX] | 5F | LD H,[IY] |
| 60 | LD [IX],A | 61 | LD [IX],B | 62 | LD [IX],L | 63 | LD [IX],H |
| 64 | LD [IX],[BR:ll] | 65 | LD [IX],[HL] | 66 | LD [IX],[IX] | 67 | LD [IX],[IY] |
| 68 | LD [HL],A | 69 | LD [HL],B | 6A | LD [HL],L | 6B | LD [HL],H |
| 6C | LD [HL],[BR:ll] | 6D | LD [HL],[HL] | 6E | LD [HL],[IX] | 6F | LD [HL],[IY] |
| 70 | LD [IY],A | 71 | LD [IY],B | 72 | LD [IY],L | 73 | LD [IY],H |
| 74 | LD [IY],[BR:ll] | 75 | LD [IY],[HL] | 76 | LD [IY],[IX] | 77 | LD [IY],[IY] |
| 78 | LD [BR:ll],A | 79 | LD [BR:ll],B | 7A | LD [BR:ll],L | 7B | LD [BR:ll],H |
| 7C | *Undefined* | 7D | LD [BR:ll],[HL] | 7E | LD [BR:ll],[IX] | 7F | LD [BR:ll],[IY] |
| 80 | INC A | 81 | INC B | 82 | INC L | 83 | INC H |
| 84 | INC BR | 85 | INC [BR:ll] | 86 | INC [HL] | 87 | INC SP |
| 88 | DEC A | 89 | DEC B | 8A | DEC L | 8B | DEC H |
| 8C | DEC BR | 8D | DEC [BR:ll] | 8E | DEC [HL] | 8F | DEC SP |
| 90 | INC BA | 91 | INC HL | 92 | INC IX | 93 | INC IY |
| 94 | BIT A,B | 95 | BIT [HL],#nn | 96 | BIT A,#nn | 97 | BIT B,#nn |
| 98 | DEC BA | 99 | DEC HL | 9A | DEC IX | 9B | DEC IY |
| 9C | AND SC,#nn | 9D | OR SC,#nn | 9E | XOR SC,#nn | 9F | LD SC,#nn |
| A0 | PUSH BA | A1 | PUSH HL | A2 | PUSH IX | A3 | PUSH IY |
| A4 | PUSH BR | A5 | PUSH EP | A6 | PUSH IP | A7 | PUSH SC |
| A8 | POP BA | A9 | POP HL | AA | POP IX | AB | POP IY |
| AC | POP BR | AD | POP EP | AE | POP IP | AF | POP SC |
| B0 | LD A,#nn | B1 | LD B,#nn | B2 | LD L,#nn | B3 | LD H,#nn |
| B4 | LD BR,#hh | B5 | LD [HL],#nn | B6 | LD [IX],#nn | B7 | LD [IY],#nn |
| B8 | LD BA,[hhll] | B9 | LD HL,[hhll] | BA | LD IX,[hhll] | BB | LD IY,[hhll] |
| BC | LD [hhll],BA | BD | LD [hhll],HL | BE | LD [hhll],IX | BF | LD [hhll],IY |
| C0 | ADD BA,#mmnn | C1 | ADD HL,#mmnn | C2 | ADD IX,#mmnn | C3 | ADD IY,#mmnn |
| C4 | LD BA,#mmnn | C5 | LD HL,#mmnn | C6 | LD IX,#mmnn | C7 | LD IY,#mmnn |
| C8 | EX BA,HL | C9 | EX BA,IX | CA | EX BA,IY | CB | EX BA,SP |
| CC | EX A,B | CD | EX A,[HL] | CE | *(prefix → CE page)* | CF | *(prefix → CF page)* |
| D0 | SUB BA,#mmnn | D1 | SUB HL,#mmnn | D2 | SUB IX,#mmnn | D3 | SUB IY,#mmnn |
| D4 | CP BA,#mmnn | D5 | CP HL,#mmnn | D6 | CP IX,#mmnn | D7 | CP IY,#mmnn |
| D8 | AND [BR:ll],#nn | D9 | OR [BR:ll],#nn | DA | XOR [BR:ll],#nn | DB | CP [BR:ll],#nn |
| DC | BIT [BR:ll],#nn | DD | LD [BR:ll],#nn | DE | PACK | DF | UPCK |
| E0 | CARS C,rr | E1 | CARS NC,rr | E2 | CARS Z,rr | E3 | CARS NZ,rr |
| E4 | JRS C,rr | E5 | JRS NC,rr | E6 | JRS Z,rr | E7 | JRS NZ,rr |
| E8 | CARL C,qqrr | E9 | CARL NC,qqrr | EA | CARL Z,qqrr | EB | CARL NZ,qqrr |
| EC | JRL C,qqrr | ED | JRL NC,qqrr | EE | JRL Z,qqrr | EF | JRL NZ,qqrr |
| F0 | CARS rr | F1 | JRS rr | F2 | CARL qqrr | F3 | JRL qqrr |
| F4 | JP HL | F5 | DJR NZ,rr | F6 | SWAP A | F7 | SWAP [HL] |
| F8 | RET | F9 | RETE | FA | RETS | FB | CALL [hhll] |
| FC | INT [kk] | FD | JP [kk] | FE | *Undefined* | FF | NOP |

> **Reconstruction note (E0–F3):** the E/F columns hold the CARS/JRS/CARL/JRL conditional & unconditional forms. The assignments above (`E0–E3` CARS C/NC/Z/NZ, `E4–E7` JRS C/NC/Z/NZ, `E8–EB` CARL, `EC–EF` JRL, `F0–F3` unconditional CARS/JRS/CARL/JRL) were **cross-verified against skiploom's `src/util/s1c88.csv`**, which corrects a one-row shift present in the flattened §4.3.3 extraction. All other unprefixed cells were unambiguous.

## A.2 `CE`-prefixed page (2nd byte, 1st byte = `CE`)

These are the `CE,xx` two-byte (plus operands) instructions. Opcode shown is the 2nd byte.

| 2nd | Instr | 2nd | Instr | 2nd | Instr | 2nd | Instr |
|----|-------|----|-------|----|-------|----|-------|
| 00 | ADD A,[IX+dd] | 01 | ADD A,[IY+dd] | 02 | ADD A,[IX+L] | 03 | ADD A,[IY+L] |
| 04 | ADD [HL],A | 05 | ADD [HL],#nn | 06 | ADD [HL],[IX] | 07 | ADD [HL],[IY] |
| 08 | ADC A,[IX+dd] | 09 | ADC A,[IY+dd] | 0A | ADC A,[IX+L] | 0B | ADC A,[IY+L] |
| 0C | ADC [HL],A | 0D | ADC [HL],#nn | 0E | ADC [HL],[IX] | 0F | ADC [HL],[IY] |
| 10 | SUB A,[IX+dd] | 11 | SUB A,[IY+dd] | 12 | SUB A,[IX+L] | 13 | SUB A,[IY+L] |
| 14 | SUB [HL],A | 15 | SUB [HL],#nn | 16 | SUB [HL],[IX] | 17 | SUB [HL],[IY] |
| 18 | SBC A,[IX+dd] | 19 | SBC A,[IY+dd] | 1A | SBC A,[IX+L] | 1B | SBC A,[IY+L] |
| 1C | SBC [HL],A | 1D | SBC [HL],#nn | 1E | SBC [HL],[IX] | 1F | SBC [HL],[IY] |
| 20 | AND A,[IX+dd] | 21 | AND A,[IY+dd] | 22 | AND A,[IX+L] | 23 | AND A,[IY+L] |
| 24 | AND [HL],A | 25 | AND [HL],#nn | 26 | AND [HL],[IX] | 27 | AND [HL],[IY] |
| 28 | OR A,[IX+dd] | 29 | OR A,[IY+dd] | 2A | OR A,[IX+L] | 2B | OR A,[IY+L] |
| 2C | OR [HL],A | 2D | OR [HL],#nn | 2E | OR [HL],[IX] | 2F | OR [HL],[IY] |
| 30 | CP A,[IX+dd] | 31 | CP A,[IY+dd] | 32 | CP A,[IX+L] | 33 | CP A,[IY+L] |
| 34 | CP [HL],A | 35 | CP [HL],#nn | 36 | CP [HL],[IX] | 37 | CP [HL],[IY] |
| 38 | XOR A,[IX+dd] | 39 | XOR A,[IY+dd] | 3A | XOR A,[IX+L] | 3B | XOR A,[IY+L] |
| 3C | XOR [HL],A | 3D | XOR [HL],#nn | 3E | XOR [HL],[IX] | 3F | XOR [HL],[IY] |
| 40 | LD A,[IX+dd] | 41 | LD A,[IY+dd] | 42 | LD A,[IX+L] | 43 | LD A,[IY+L] |
| 44 | LD [IX+dd],A | 45 | LD [IY+dd],A | 46 | LD [IX+L],A | 47 | LD [IY+L],A |
| 48 | LD B,[IX+dd] | 49 | LD B,[IY+dd] | 4A | LD B,[IX+L] | 4B | LD B,[IY+L] |
| 4C | LD [IX+dd],B | 4D | LD [IY+dd],B | 4E | LD [IX+L],B | 4F | LD [IY+L],B |
| 50 | LD L,[IX+dd] | 51 | LD L,[IY+dd] | 52 | LD L,[IX+L] | 53 | LD L,[IY+L] |
| 54 | LD [IX+dd],L | 55 | LD [IY+dd],L | 56 | LD [IX+L],L | 57 | LD [IY+L],L |
| 58 | LD H,[IX+dd] | 59 | LD H,[IY+dd] | 5A | LD H,[IX+L] | 5B | LD H,[IY+L] |
| 5C | LD [IX+dd],H | 5D | LD [IY+dd],H | 5E | LD [IX+L],H | 5F | LD [IY+L],H |
| 60 | LD [HL],[IX+dd] | 61 | LD [HL],[IY+dd] | 62 | LD [HL],[IX+L] | 63 | LD [HL],[IY+L] |
| 68 | LD [IX],[IX+dd] | 69 | LD [IX],[IY+dd] | 6A | LD [IX],[IX+L] | 6B | LD [IX],[IY+L] |
| 78 | LD [IY],[IX+dd] | 79 | LD [IY],[IY+dd] | 7A | LD [IY],[IX+L] | 7B | LD [IY],[IY+L] |
| 80 | SLA A | 81 | SLA B | 82 | SLA [BR:ll] | 83 | SLA [HL] |
| 84 | SLL A | 85 | SLL B | 86 | SLL [BR:ll] | 87 | SLL [HL] |
| 88 | SRA A | 89 | SRA B | 8A | SRA [BR:ll] | 8B | SRA [HL] |
| 8C | SRL A | 8D | SRL B | 8E | SRL [BR:ll] | 8F | SRL [HL] |
| 90 | RL A | 91 | RL B | 92 | RL [BR:ll] | 93 | RL [HL] |
| 94 | RLC A | 95 | RLC B | 96 | RLC [BR:ll] | 97 | RLC [HL] |
| 98 | RR A | 99 | RR B | 9A | RR [BR:ll] | 9B | RR [HL] |
| 9C | RRC A | 9D | RRC B | 9E | RRC [BR:ll] | 9F | RRC [HL] |
| A0 | CPL A | A1 | CPL B | A2 | CPL [BR:ll] | A3 | CPL [HL] |
| A4 | NEG A | A5 | NEG B | A6 | NEG [BR:ll] | A7 | NEG [HL] |
| A8 | SEP | AE | HALT | AF | SLP | — | — |
| B0 | AND B,#nn | B1 | AND L,#nn | B2 | AND H,#nn | B4 | OR B,#nn |
| B5 | OR L,#nn | B6 | OR H,#nn | B8 | XOR B,#nn | B9 | XOR L,#nn |
| BA | XOR H,#nn | BC | CP B,#nn | BD | CP L,#nn | BE | CP H,#nn |
| BF | CP BR,#hh | C0 | LD A,BR | C1 | LD A,SC | C2 | LD BR,A |
| C3 | LD SC,A | C4 | LD NB,#bb | C5 | LD EP,#pp | C6 | LD XP,#pp |
| C7 | LD YP,#pp | C8 | LD A,NB | C9 | LD A,EP | CA | LD A,XP |
| CB | LD A,YP | CC | LD NB,A | CD | LD EP,A | CE | LD XP,A |
| CF | LD YP,A | D0 | LD A,[hhll] | D1 | LD B,[hhll] | D2 | LD L,[hhll] |
| D3 | LD H,[hhll] | D4 | LD [hhll],A | D5 | LD [hhll],B | D6 | LD [hhll],L |
| D7 | LD [hhll],H | D8 | MLT | D9 | DIV | — | — |
| E0 | JRS LT,rr | E1 | JRS LE,rr | E2 | JRS GT,rr | E3 | JRS GE,rr |
| E4 | JRS V,rr | E5 | JRS NV,rr | E6 | JRS P,rr | E7 | JRS M,rr |
| E8 | JRS F0,rr | E9 | JRS F1,rr | EA | JRS F2,rr | EB | JRS F3,rr |
| EC | JRS NF0,rr | ED | JRS NF1,rr | EE | JRS NF2,rr | EF | JRS NF3,rr |
| F0 | CARS LT,rr | F1 | CARS LE,rr | F2 | CARS GT,rr | F3 | CARS GE,rr |
| F4 | CARS V,rr | F5 | CARS NV,rr | F6 | CARS P,rr | F7 | CARS M,rr |
| F8 | CARS F0,rr | F9 | CARS F1,rr | FA | CARS F2,rr | FB | CARS F3,rr |
| FC | CARS NF0,rr | FD | CARS NF1,rr | FE | CARS NF2,rr | FF | CARS NF3,rr |

> `CE` 2nd-byte ranges 64–67, 6C–6F, 70–77, 7C–7F, A9–AD, B3, B7, BB, and any cell not listed are **Undefined Code Area** per the map. The block 60–7B above only lists the defined `[HL]/[IX]/[IY]` ←index+disp/L LD forms; intervening codes are undefined.

## A.3 `CF`-prefixed page (2nd byte, 1st byte = `CF`)

| 2nd | Instr | 2nd | Instr | 2nd | Instr | 2nd | Instr |
|----|-------|----|-------|----|-------|----|-------|
| 00 | ADD BA,BA | 01 | ADD BA,HL | 02 | ADD BA,IX | 03 | ADD BA,IY |
| 04 | ADC BA,BA | 05 | ADC BA,HL | 06 | ADC BA,IX | 07 | ADC BA,IY |
| 08 | SUB BA,BA | 09 | SUB BA,HL | 0A | SUB BA,IX | 0B | SUB BA,IY |
| 0C | SBC BA,BA | 0D | SBC BA,HL | 0E | SBC BA,IX | 0F | SBC BA,IY |
| 18 | CP BA,BA | 19 | CP BA,HL | 1A | CP BA,IX | 1B | CP BA,IY |
| 20 | ADD HL,BA | 21 | ADD HL,HL | 22 | ADD HL,IX | 23 | ADD HL,IY |
| 24 | ADC HL,BA | 25 | ADC HL,HL | 26 | ADC HL,IX | 27 | ADC HL,IY |
| 28 | SUB HL,BA | 29 | SUB HL,HL | 2A | SUB HL,IX | 2B | SUB HL,IY |
| 2C | SBC HL,BA | 2D | SBC HL,HL | 2E | SBC HL,IX | 2F | SBC HL,IY |
| 38 | CP HL,BA | 39 | CP HL,HL | 3A | CP HL,IX | 3B | CP HL,IY |
| 40 | ADD IX,BA | 41 | ADD IX,HL | 42 | ADD IY,BA | 43 | ADD IY,HL |
| 44 | ADD SP,BA | 45 | ADD SP,HL | 48 | SUB IX,BA | 49 | SUB IX,HL |
| 4A | SUB IY,BA | 4B | SUB IY,HL | 4C | SUB SP,BA | 4D | SUB SP,HL |
| 5C | CP SP,BA | 5D | CP SP,HL | 60 | ADC BA,#mmnn | 61 | ADC HL,#mmnn |
| 62 | SBC BA,#mmnn | 63 | SBC HL,#mmnn | 68 | ADD SP,#mmnn | 6A | SUB SP,#mmnn |
| 6C | CP SP,#mmnn | 6E | LD SP,#mmnn | 70 | LD BA,[SP+dd] | 71 | LD HL,[SP+dd] |
| 72 | LD IX,[SP+dd] | 73 | LD IY,[SP+dd] | 74 | LD [SP+dd],BA | 75 | LD [SP+dd],HL |
| 76 | LD [SP+dd],IX | 77 | LD [SP+dd],IY | 78 | LD SP,[hhll] | 7C | LD [hhll],SP |
| B0 | PUSH A | B1 | PUSH B | B2 | PUSH L | B3 | PUSH H |
| B4 | POP A | B5 | POP B | B6 | POP L | B7 | POP H |
| B8 | PUSH ALL | B9 | PUSH ALE | BC | POP ALL | BD | POP ALE |
| C0 | LD BA,[HL] | C1 | LD HL,[HL] | C2 | LD IX,[HL] | C3 | LD IY,[HL] |
| C4 | LD [HL],BA | C5 | LD [HL],HL | C6 | LD [HL],IX | C7 | LD [HL],IY |
| D0 | LD BA,[IX] | D1 | LD HL,[IX] | D2 | LD IX,[IX] | D3 | LD IY,[IX] |
| D4 | LD [IX],BA | D5 | LD [IX],HL | D6 | LD [IX],IX | D7 | LD [IX],IY |
| D8 | LD BA,[IY] | D9 | LD HL,[IY] | DA | LD IX,[IY] | DB | LD IY,[IY] |
| DC | LD [IY],BA | DD | LD [IY],HL | DE | LD [IY],IX | DF | LD [IY],IY |
| E0 | LD BA,BA | E1 | LD BA,HL | E2 | LD BA,IX | E3 | LD BA,IY |
| E4 | LD HL,BA | E5 | LD HL,HL | E6 | LD HL,IX | E7 | LD HL,IY |
| E8 | LD IX,BA | E9 | LD IX,HL | EA | LD IX,IX | EB | LD IX,IY |
| EC | LD IY,BA | ED | LD IY,HL | EE | LD IY,IX | EF | LD IY,IY |
| F0 | LD SP,BA | F1 | LD SP,HL | F2 | LD SP,IX | F3 | LD SP,IY |
| F4 | LD HL,SP | F5 | LD HL,PC | F8 | LD BA,SP | F9 | LD BA,PC |
| FA | LD IX,SP | FE | LD IY,SP | — | — | — | — |

> All `CF` 2nd-byte cells not listed (e.g. 10–17, 30–37, 46–47, 4E–5B, 5E–5F, 64–67, 69, 6B, 6D, 6F, 79–7B, 7D–AF, BA–BB, BE–BF, F6–F7, FB–FD, FF) are **Undefined Code Area** per the map. `LD IX,SP`/`LD IY,SP` and `LD BA,PC` sit in otherwise-undefined neighborhoods (the map explicitly labels surrounding cells "Undefined Code Area").

---

# Appendix B — Instruction List by Addressing Mode (PDF pp. 198–209)

Appendix B re-tabulates every form grouped by **addressing mode of the operand**. It carries the same byte/cycle data as §4.3.3 (verified consistent). Modes used: Immediate, Register Direct, Register Indirect, Register Indirect + Displacement (`[IX+dd]`/`[IY+dd]`), Register Indirect + Index Register (`[IX+L]`/`[IY+L]`), 8-bit Absolute (`[BR:ll]`), 16-bit Absolute (`[hhll]`/16-bit indirect), Signed PC-relative (8-bit `rr`, 16-bit `qqrr`), Implied. Below is a compact per-mode index of which instruction forms appear (byte/cycle already given in §4.3.3 tables above; this section preserves the addressing-mode grouping for reference).

### 8-bit Arithmetic & Logic (B 1/3 – 2/12)
- **Immediate** `A,#nn`, `[HL],#nn`, `B,#nn`, `L,#nn`, `H,#nn`, `BR,#hh`, `SC,#nn`, `[BR:ll],#nn` (for ADD/ADC/SUB/SBC/AND/OR/XOR/CP/BIT). Bytes 2–3 reg / 3 mem; cycles 2 (A/reg), 5 (`[BR:ll]`,#nn), 3–5 (`[HL]`).
- **Register Direct** `A,A`, `A,B`, `[HL],A`; plus `A`/`B`/`L`/`H`/`BR`/`[HL]` for INC/DEC/CPL/NEG, `A,B` for BIT. 1 byte (A,A/A,B) … 2 bytes (`[HL],A`).
- **Register Indirect** `A,[HL]`, `A,[IX]`, `A,[IY]`, `[HL],[IX]`, `[HL],[IY]`; `[HL]` for INC/DEC/CPL/NEG.
- **+Displacement** `A,[IX+dd]`, `A,[IY+dd]` (3 byte / 4 cyc).
- **+Index** `A,[IX+L]`, `A,[IY+L]` (2 byte / 4 cyc).
- **8-bit Absolute** `A,[BR:ll]`, `[BR:ll]` (CPL/NEG) — 2–3 byte.
- **16-bit Absolute** `A,[hhll]` — 3 byte / 4 cyc.

### 8-bit Transfer (B 3/12 – 4/12)
- **Immediate** `A/B/L/H,#nn`, `BR,#hh`, `SC,#nn`, `[BR:ll]/[HL]/[IX]/[IY],#nn`, `NB,#bb`, `EP/XP/YP,#pp`.
- **Register Direct** all `r,r'` reg-pair LDs, `A,BR`/`A,SC`/`A,NB`/`A,EP`/`A,XP`/`A,YP`, `BR,A`/`SC,A`/`NB,A`/`EP,A`/`XP,A`/`YP,A`, plus stores `[BR:ll]/[hhll],r`. EX `A,B`; SWAP `A`.
- **Register Indirect** loads/stores via `[HL]`,`[IX]`,`[IY]` for `A/B/L/H` and `[BR:ll],[HL/IX/IY]`, `[HL/IX/IY],[HL/IX/IY]`. EX `A,[HL]`; SWAP `[HL]`.
- **+Displacement / +Index / 8-bit Abs / 16-bit Abs** as the corresponding `[IX+dd]`,`[IX+L]`,`[BR:ll]`,`[hhll]` forms.

### Rotate/Shift (B 5/12)
- **Register Direct** `A`,`B` (2 byte / 3 cyc); **Register Indirect** `[HL]` (2 byte / 4 cyc); **8-bit Absolute** `[BR:ll]` (3 byte / 5 cyc). For RL/RLC/RR/RRC/SLA/SLL/SRA/SRL.

### 16-bit Arithmetic (B 6/12 – 7/12)
- **Immediate** `BA/HL/IX/IY/SP,#mmnn` (3 byte / 3–4 cyc; SP forms 4 byte / 4 cyc).
- **Register Direct** `BA/HL,{BA,HL,IX,IY}`, `IX/IY/SP,{BA,HL}` (2 byte / 4 cyc); INC/DEC `BA/HL/IX/IY/SP` (1 byte / 2 cyc).

### 16-bit Transfer (B 8/12 – 9/12)
- **Immediate** `BA/HL/IX/IY/SP,#mmnn`.
- **Register Direct** all 16-bit `r,r'` pairs, `…,SP`, `…,PC`, `SP,{BA,HL,IX,IY}`, `[hhll],{BA,HL,IX,IY,SP}` (16-bit absolute store).
- **Register Indirect** `…,[HL]`/`[IX]`/`[IY]` and stores `[HL]/[IX]/[IY],…`. **+Displacement** `…,[SP+dd]`. **16-bit Absolute** `…,[hhll]`. EX `BA,{HL,IX,IY,SP}`.

### Branch (B 10/12 – 11/12)
- **8-bit Indirect** `JP/CARS/CARL/CALL/INT [kk]` and `CALL [hhll]` (16-bit indirect).
- **Signed 8-bit PC-relative** JRS/CARS `rr` and all condition variants (byte 2 unconditional/`C/NC/Z/NZ`; byte 3 the CE-prefixed conditions). DJR `NZ,rr`.
- **Signed 16-bit PC-relative** JRL/CARL `qqrr`, `C/NC/Z/NZ,qqrr`.
- **Register Direct** `JP HL`. **Others** RET/RETE/RETS.
- Cycle columns here are split **Min. / Max. / Skip** (Min = minimum mode, Max = maximum mode, Skip = not-branched fall-through).

### Stack / MLT-DIV / Auxiliary / System (B 12/12)
- **Stack Control** PUSH/POP `A/B/L/H/BR/SC/EP/IP/BA/HL/IX/IY` (Register Direct, 1–2 byte) and `ALL`/`ALE` (Implied, 2 byte; PUSH ALL 12 cyc / ALE 15; POP ALL 11 / ALE 14).
- **Multiplication/Division** (Implied): MLT 2 byte / 12 cyc; DIV 2 byte / 13 cyc.
- **Auxiliary** (Implied): PACK 1 byte / 2 cyc; UPCK 1 byte / 2 cyc; SEP 2 byte / 3 cyc.
- **System Control**: NOP 1 byte / 2 cyc; HALT 2 byte / 3 cyc; SLP 2 byte / 3 cyc.

> Appendix B OCR slips noted and corrected: `[IX],{HL]`→`[IX],[HL]`; `LD BR,#nn`→`LD BR,#hh`; `[HL],nn`→`[HL],#nn`; the heading "Sined"/"Implide" are misspellings of "Signed"/"Implied" in the source extraction.

---

# Appendix C — Instruction Index (PDF pp. 210–217)

Alphabetical index of every instruction form mapping to its detailed-description page in **§4.4** (manual page numbers). Grouped by mnemonic and bit-width. Reproduced compactly; the page numbers below are the §4.4 destinations (same values as the "§4.4 pg" columns above).

- **ADC** 8-bit: `A,A`/`A,B`/`A,#nn`/`A,[BR:ll]` 60; `A,[hhll]`/`A,[HL]` 61; `A,[IX]`/`A,[IX+dd]`/`A,[IY]`/`A,[IY+dd]` 62; `A,[IX+L]`/`A,[IY+L]` 63; `[HL],A` 63; `[HL],#nn`/`[HL],[IX]`/`[HL],[IY]` 64. **ADC** 16-bit: `BA,*` 65; `HL,*` 66.
- **ADD** 8-bit: `A,A`/`A,B`/`A,#nn`/`A,[BR:ll]` 67; `A,[hhll]`/`A,[HL]`/`A,[IX]`/`A,[IY]` 68; `A,[IX+dd]`/`A,[IX+L]`/`A,[IY+dd]`/`A,[IY+L]` 69; `[HL],A`/`[HL],#nn` 70; `[HL],[IX]`/`[HL],[IY]` 71. **ADD** 16-bit: `BA,*` 71–72; `HL,*` 72; `IX,*` 73; `IY,*` 73–74; `SP,*` 74.
- **AND**: `A,*` 75–77; `B,#nn`/`H,#nn`/`L,#nn` 78; `SC,#nn`/`[BR:ll],#nn`/`[HL],A` 79; `[HL],#nn`/`[HL],[IX]`/`[HL],[IY]` 80.
- **BIT**: `A,B`/`A,#nn`/`B,#nn` 81; `[BR:ll],#nn`/`[HL],#nn` 82.
- **CALL** `[hhll]` 83.
- **CARL**: `qqrr` 84; `C/NC/NZ/Z,qqrr` 85.
- **CARS**: `rr` 86; `C/NC/NZ/Z,rr` 87; all flag/cond variants (`F0..F3`,`NF0..NF3`,`GE`,`GT`,`LE`,`LT`,`M`,`NV`,`P`,`V`) 88.
- **CP** 8-bit: `A,*` 90–93; `B,#nn` 93; `BR,#hh`/`H,#nn`/`L,#nn` 94; `[BR:ll],#nn`/`[HL],A` 95; `[HL],#nn`/`[HL],[IX]`/`[HL],[IY]` 96. **CP** 16-bit: `BA,*` 97; `HL,*` 98; `IX,#mmnn`/`IY,#mmnn` 99; `SP,*` 100.
- **CPL** `A`/`B`/`[BR:ll]`/`[HL]` 101.
- **DEC** 8-bit: `A`/`B`/`BR`/`H`/`L`/`[BR:ll]` 102; `[HL]` 103. **DEC** 16-bit: `BA`/`HL`/`IX`/`IY`/`SP` 103.
- **DIV** 104. **DJR** `NZ,rr` 104.
- **EX** 8-bit: `A,B`/`A,[HL]` 105. **EX** 16-bit: `BA,HL`/`BA,IX`/`BA,IY`/`BA,SP` 105.
- **HALT** 106.
- **INC** 8-bit: `A`/`B`/`BR`/`H`/`L` 106; `[BR:ll]`/`[HL]` 107. **INC** 16-bit: `BA`/`HL`/`IX`/`IY` 107; `SP` 108.
- **INT** `[kk]` 108.
- **JP** `HL`/`[kk]` 109.
- **JRL**: `qqrr` 110; `C/NC/NZ/Z,qqrr` 111.
- **JRS**: `rr` 112; all `C/NC/NZ/Z` and flag/cond variants 113.
- **LD** 8-bit reg-reg & control 115–117; stores `[hhll],r`/`[HL],r` 118; `[IX],r`/`[IY],r` 119; `[IX+dd],r`/`[IY+dd],r` 120; `[IX+L],r`/`[IY+L],r` 121; `r,#nn` 122; `EP/XP/YP/NB,#…` 123–124; indirect-load/store immediates & `[BR:ll]`/`[HL]`/`[IX]`/`[IY]` and displacement/index forms 124–137. **LD** 16-bit: reg-reg/`,PC`/`,SP` 138–139; `SP,*`/`[hhll],*` 140–141; `[HL]/[IX]/[IY],*` 141–142; `[SP+dd],*`/`r,#mmnn` 143; `r,[hhll]` 144; `r,[HL]` 145; `r,[IX]`/`r,[IY]` 146; `r,[SP+dd]` 147.
- **MLT** 147.
- **NEG** `A`/`B`/`[BR:ll]`/`[HL]` 148.
- **NOP** 149.
- **OR** 8-bit: `A,*` 149–152; `B/H/L,#nn` 152–153; `SC,#nn`/`[BR:ll],#nn` 153; `[HL],*` 154.
- **PACK** 155. **POP** `A/B/L/H/BA/HL/IX/IY` 155; `BR/EP/IP/SC` 156; `ALL`/`ALE` 157. **PUSH** `A/B/L/H/BA/HL/IX/IY` 158; `BR/EP/IP` 159; `ALL`/`ALE`/`SC` 160.
- **RET** 161. **RETE** 161. **RETS** 162.
- **RL** `A`/`B` 162; `[BR:ll]`/`[HL]` 163. **RLC** `A`/`B` 163; `[BR:ll]`/`[HL]` 164. **RR** `A`/`B` 164; `[BR:ll]`/`[HL]` 165. **RRC** `A`/`B`/`[BR:ll]`/`[HL]` 166.
- **SBC** 8-bit: `A,*` 167–169; `[HL],A`/`[HL],#nn` 170; `[HL],[IX]`/`[HL],[IY]`/`BA,*` 171; `HL,*` 172. **SEP** 173.
- **SLA** `A`/`B` 173; `[BR:ll]`/`[HL]` 174. **SLL** `A`/`B`/`[BR:ll]` 175; `[HL]` 176. **SLP** 176.
- **SRA** `A`/`B`/`[BR:ll]` 177; `[HL]` 178. **SRL** `A`/`B` 178; `[BR:ll]`/`[HL]` 179.
- **SUB** 8-bit: `A,*` 180–182; `[HL],A`/`[HL],#nn` 183; `[HL],[IX]`/`[HL],[IY]`/`BA,*` 184; `HL,*` 185; `IX,*` 186; `IY,*`/`SP,*` 187. **SWAP** `A`/`[HL]` 188. **UPCK** 188.
- **XOR** 8-bit: `A,*` 189–191; `B/H/L,#nn` 192; `SC,#nn`/`[BR:ll],#nn`/`[HL],A` 193; `[HL],#nn`/`[HL],[IX]`/`[HL],[IY]` 194.

---

## Appendix C index headings note

The index labels rotate/shift entries somewhat loosely (e.g. RLC printed under "RL: Rotate to Left", RR under "RL: Rotate to Right with Carry") — these are heading typos in the source manual; the mnemonics themselves (RL/RLC/RR/RRC) are correct.
