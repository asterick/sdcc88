# S1C88 Addressing Modes & Instruction Format

**Source:** S1C88 Core CPU Manual (MF658-05)
**Chapter:** 4 Instruction Sets
**Sections covered:** §4.1 Addressing Mode, §4.2 Instruction Format (with §4.3 intro context)
**PDF page range:** 41–47 (manual printed pages 35–41)
**Extraction note:** Text is machine-extracted, one file per page, from
`docs/_extract/core-cpu/page-NNN.txt`. The original figures (Fig. 4.1.x) are register/memory
flow diagrams; their text fragments were interleaved by the extractor and have been
reconstructed below as worked examples and marked where the original was a figure.

---

## 4 Instruction Sets — overview

The S1C88 offers high machine-cycle efficiency along with ample, high-speed instruction
sets. It has **608 instructions (MODEL3)**, designed as an instruction system that permits
relocatable programming.

> **Model note:** The New Code Bank register **NB** and the expand page registers
> **EP / XP / YP** (and **IP** = XP+YP) exist only on **MODEL2/3**. On **MODEL0/1**,
> instructions that access these registers cannot be used, and the page/bank extensions
> described below do not apply. Register-direct use of `NB, EP, XP, YP, IP` is MODEL2/3 only.

---

## 4.1 Addressing Mode

The S1C88 has **12 types of addressing modes**. The address specifications corresponding to
the various statuses are done concisely and accurately. The explanations and examples below
are focused on the **source** side.

### Table 4.1.1 Types of addressing modes

| No. | Addressing mode |
|-----|-----------------|
| 1 | Immediate data addressing |
| 2 | Register direct addressing |
| 3 | Register indirect addressing |
| 4 | Register indirect addressing with displacement |
| 5 | Register indirect addressing with index register |
| 6 | 8-bit absolute addressing |
| 7 | 16-bit absolute addressing |
| 8 | 8-bit indirect addressing |
| 9 | 16-bit indirect addressing |
| 10 | Signed 8-bit PC relative addressing |
| 11 | Signed 16-bit PC relative addressing |
| 12 | Implied register addressing |

---

### 4.1 (1) Immediate data addressing

Immediate data addressing is the addressing mode used when immediate data is the operation
or transmission **source** data. It specifies the source operand as direct source data, with
8-bit immediate data and 16-bit immediate data following a `#`.

The following symbols indicate the immediate data in the instruction-set notation.

#### Table 4.1.2 Immediate data symbols

| Symbol | Use | Size | Range |
|--------|-----|------|-------|
| `#nn` | General purpose data | 8 bits | 0–255 |
| `#hh` | For BR setting | 8 bits | 0–255 |
| `#bb` | For NB setting | 8 bits | 0–255 |
| `#pp` | For page setting | 8 bits | 0–255 |
| `#mmnn` | General purpose data | 16 bits | 0–65535 |

**Effective operand:** the literal value itself (no memory access). For `#mmnn`, the operand
bytes are encoded little-endian — low byte `nn` first, high byte `mm` second (see §4.2).

*Fig. 4.1.1 Immediate data addressing — (figure not captured)*

Worked example:

```
LD A,#03H        ; A reg. <- 03H   (machine: B0,nn with nn=03)
                 ; A was 05H -> becomes 03H
```

---

### 4.1 (2) Register direct addressing

Register direct addressing specifies a **register** as the source or destination. The register
name is written as (or in place of) the operand.

**Register notations:**

- **8-bit:** `A, B, L, H, BR, SC, NB, EP, XP, YP`
- **16-bit:** `BA, HL, IX, IY, PC, SP, IP` (where **IP = YP and XP**)

> `NB, EP, XP, YP, IP` are usable on **MODEL2/3 only**.

- **As source:** the content of the specified register becomes the source data for the
  operation or transmission.
- **As destination:** data storage and calculation results are written into that register.

*Fig. 4.1.2 Register direct addressing — (figure not captured)*

Worked examples:

```
LD A,#03H        ; immediate into register-direct destination; A <- 03H
LD A,B           ; A <- B  (A=05H, B=03H -> A=03H)
```

---

### 4.1 (3) Register indirect addressing

Register indirect addressing accesses **data memory**, indirectly specifying the data-memory
address by an **index register**.

There are three index registers for address specification: **HL, IX, IY**. Their content is
the data-memory address that is accessed. In the instruction set the index-register names are
surrounded by brackets `[ ]`: **`[HL]`, `[IX]`, `[IY]`**.

- **As source:** the content of the specified index register is the data-memory address; the
  content stored at that address is the source data.
- **As destination:** data storage / calculations are performed on the specified data memory.

**Page interaction (MODEL2/3):** the page section must also be specified, using the expand
page registers:

| Index register | Page register |
|----------------|---------------|
| `[HL]` | **EP** |
| `[IX]` | **XP** |
| `[IY]` | **YP** |

*Fig. 4.1.3 Register indirect addressing — (figure not captured)*

Worked example:

```
LD A,[HL]        ; machine: 45
                 ; HL = 3200H, EP = 00 (MODEL2/3 page) -> effective addr 00:3200H
                 ; [3200H] = 1AH -> A becomes 1AH (was 05H)
```

---

### 4.1 (4) Register indirect addressing with displacement

This addressing mode accesses data memory and specifies the data-memory address as the
**register content + displacement**. The displacement is **signed 8-bit data (-128…127)**,
written with the symbol **`dd`**.

The registers used are **IX, IY, SP**, noted as **`[IX+dd]`, `[IY+dd]`, `[SP+dd]`**.

- **As source:** address = (register content) + dd; the content at that address is the source
  data.
- **As destination:** data storage / calculations on the specified data memory.

**Page interaction (MODEL2/3):**

| Mode | Page register |
|------|---------------|
| `[IX+dd]` | **XP** |
| `[IY+dd]` | **YP** |
| `[SP+dd]` | The SP page register set for the peripheral circuit of each model (SP-specific page setting) |

*Fig. 4.1.4 Register indirect addressing with displacement — (figure not captured)*

Worked example:

```
LD A,[IX+2]      ; machine: CE,40,dd with dd=2
                 ; IX = 3200H, +2 -> 3202H ; XP = 00 (MODEL2/3 page)
                 ; [3202H] = 1AH -> A becomes 1AH (was 05H)
```

---

### 4.1 (5) Register indirect addressing with index register

Same as register-indirect-with-displacement, but the displacement is the content of the
**L register** instead of an 8-bit immediate. The L register content is treated as
**signed 8-bit data (-128…127)**.

Index registers **IX, IY** are used; the displacement register is fixed as **L**. Noted as
**`[IX+L]`, `[IY+L]`**.

**Page interaction (MODEL2/3):**

| Mode | Page register |
|------|---------------|
| `[IX+L]` | **XP** |
| `[IY+L]` | **YP** |

*Fig. 4.1.5 Register indirect addressing with index register — (figure not captured)*

Worked example:

```
LD A,[IY+L]      ; IY = 3200H, L = 02H -> 3202H ; YP = 00 (MODEL2/3 page)
                 ; [3202H] = 1AH -> A becomes 1AH (was 05H)
```

---

### 4.1 (6) 8-bit absolute addressing

8-bit absolute addressing accesses data memory and **directly specifies the lower 8 bits** of
the address with an 8-bit absolute address; the **upper 8 bits are taken from the BR register**.

The symbol **`ll`** is used for the 8-bit absolute address, noted as **`[BR:ll]`**.

- **As source:** address upper byte = BR, lower byte = `ll`; the content stored there is the
  source data.
- **As destination:** data storage / calculations on the specified data memory.

**Page interaction (MODEL2/3):** the page section is specified by the expand page register
**EP**.

*Fig. 4.1.6 8-bit absolute addressing — (figure not captured)*

Worked example:

```
LD A,[BR:2]      ; machine: 44,ll with ll=02
                 ; BR = 32H -> address 3202H ; EP = 00 (MODEL2/3 page)
                 ; [3202H] = 1AH -> A becomes 1AH (was 05H)
```

---

### 4.1 (7) 16-bit absolute addressing

16-bit absolute addressing accesses data memory and **directly specifies the full 16-bit
address**. The symbol **`hhll`** is used for the 16-bit absolute address (0–65535), noted as
**`[hhll]`**.

- **As source:** the specified 16-bit absolute address is the direct data-memory address; the
  content stored there is the source data.
- **As destination:** data storage / calculations on the specified data memory.

**Page interaction (MODEL2/3):** the page section is specified by the expand page register
**EP**.

*Fig. 4.1.7 16-bit absolute addressing — (figure not captured)*

Worked example:

```
LD A,[3202H]     ; machine: CE,D0,ll,hh  (ll=02, hh=32 -> little-endian operand order)
                 ; EP = 00 (MODEL2/3 page)
                 ; [3202H] = 1AH -> A becomes 1AH (was 05H)
```

---

### 4.1 (8) 8-bit indirect addressing

8-bit indirect addressing uses the content of the **vector field (000000H–0000FFH)** as the
**branch destination address** for a branch instruction; it specifies the vector address with
an 8-bit absolute address.

It branches by loading the content of the specified memory address into the **lower 8 bits of
PC**, and the content of the **following address into the upper 8 bits of PC**.

The symbol **`kk`** is used for the 8-bit absolute (vector) address (0–255), noted as
**`[kk]`**. Two instructions use this mode: **`JP [kk]`** and **`INT [kk]`**.

**Bank interaction (MODEL2/3):** the branch destination bank can be selected by setting the
**NB** register.

*Fig. 4.1.8 8-bit indirect addressing — (figure not captured)*

Worked example:

```
JP [80H]         ; vector area at 00:0080H..00:0081H holds 00H,20H (little-endian)
                 ; -> PC <- 2000H  (CB/NB bank setting in MODEL2/3)
```

---

### 4.1 (9) 16-bit indirect addressing

16-bit indirect addressing is the mode of the **`CALL [hhll]`** instruction; it indirectly
specifies the branch destination address by a **16-bit absolute address (0–65535)**. It
branches the content of the specified data-memory address into the **lower 8 bits of PC** and
the content of the following address into the **upper 8 bits of PC**.

**Page interaction (MODEL2/3):** the page section is specified by **EP**; the branch
destination bank can also be selected by setting **NB**.

*Fig. 4.1.9 16-bit indirect addressing — (figure not captured)*

Worked example:

```
CALL [3280H]     ; [3280H..3281H] = 00H,40H (little-endian) -> branch target 4000H
                 ; EP = 00 page, CB/NB bank setting in MODEL2/3
```

---

### 4.1 (10) Signed 8-bit PC relative addressing

Used by branch instructions. A **signed 8-bit PC-relative value (-128…127)**, specified by an
operand, is added to the PC at that time and the instruction branches to that address.

The PC value used at that time is:

- **2-byte instruction:** PC = instruction top address + 1
- **3-byte instruction:** PC = instruction top address + 2

The symbol **`rr`** is used for the signed 8-bit PC-relative address (-128…127).

**Bank interaction (MODEL2/3):** the branch destination bank can also be selected by setting
**NB**.

*Fig. 4.1.10 Signed 8-bit PC relative addressing — (figure not captured)*

Worked example:

```
JRS $+38         ; 2-byte instruction: PC = top + 1
                 ; e.g. top = A102H -> PC base A103H ; + (38-1)=25H -> A128H
                 ; (CB/NB bank setting in MODEL2/3)
```

---

### 4.1 (11) Signed 16-bit PC relative addressing

Used by branch instructions. A **signed 16-bit PC-relative value (-32768…32767)**, specified
by an operand, is added to the PC at that time and the instruction branches to that address.

The PC value at that time is **instruction top address + 2**.

The symbol **`qqrr`** is used for the signed 16-bit PC-relative address (-32768…32767).

**Bank interaction (MODEL2/3):** the branch destination bank can also be selected by setting
**NB**.

*Fig. 4.1.11 Signed 16-bit PC relative addressing — (figure not captured)*

Worked example:

```
JRL $-3000H      ; 3-byte instruction: PC = top + 2
                 ; e.g. top = A102H -> PC base A104H ; + (-3000H - 2)=CFFEH -> 7102H
                 ; (CB/NB bank setting in MODEL2/3)
```

---

### 4.1 (12) Implied register addressing

Implied register addressing has **no operand**; it becomes register-direct addressing where
the register is implicitly specified by the instruction. Five instructions use this mode:

**`MLT`, `DIV`, `SEP`, `PACK`, `UPCK`**

---

## 4.2 Instruction Format

One S1C88 instruction is configured by a **1- to 4-byte code**.

### Op-code

The S1C88 instruction set has **608 instructions (MODEL3)** and cannot express them all in a
single-byte op-code. Therefore the codes **`CEH` and `CFH`** are made into **expansion
(escape/prefix) codes**: such a byte is used as the **first op-code**, and the following byte
becomes the **second op-code**, expanding the instruction space.

- **16-bit arithmetic / transfer instructions and stack-control instructions** are expanded
  using prefix code **`CFH`**.
- **All other** expanded instructions use prefix code **`CEH`**.

The **addressing mode** for each instruction is specified by the **lower 3 bits of the first
op-code (or of the second op-code)**. Instructions for **register direct addressing**,
**register indirect addressing**, and **register indirect addressing with index register** are
composed of **op-codes alone** (no operand bytes).

### Operands

**1-byte operand** instructions (the value is the 8-bit data as-is):

- 8-bit immediate data addressing
- register indirect addressing with displacement
- 8-bit absolute addressing (when the source is specified by a register)
- 8-bit indirect addressing
- signed 8-bit PC relative addressing

**2-byte operand** instructions:

- 16-bit immediate data addressing
- 8-bit absolute addressing (when the source is specified by immediate data)
- 16-bit absolute addressing
- 16-bit indirect addressing
- signed 16-bit PC relative addressing

For 2-byte operands, the **lower 8 bits** of the 16-bit value become the **first operand** and
the **upper 8 bits** become the **second operand** (little-endian operand ordering).

> **Special case — 8-bit absolute addressing with immediate source:** the **address
> specification (`ll`) becomes the first operand** and the **immediate data becomes the second
> operand**.

### Fig. 4.2.1 Instruction format — reconstructed

The instruction byte layout, by total length, including the `CEH/CFH` prefix where present.
A prefixed instruction uses one of `CEH`/`CFH` as its leading op-code byte.

| Length | Byte 1 | Byte 2 | Byte 3 | Byte 4 |
|--------|--------|--------|--------|--------|
| 1 | Op-code | — | — | — |
| 2 | Op-code | Op-code *(prefixed: CEH/CFH then 2nd op-code)* **or** Operand | — | — |
| 3 | Op-code | Operand | Operand | — |
| 4 | Op-code (prefix CEH/CFH) | Op-code (2nd) | Operand | Operand |

*Original figure shows six column variants illustrating the placement of `(CEH/CFH)` prefix,
op-code(s), and operand byte(s); reconstructed above. (figure not fully captured)*

Layout rules summarized:

- A non-prefixed 1-op-code-only instruction = 1 byte.
- A prefixed (`CEH`/`CFH`) op-code-only instruction = 2 bytes (prefix + 2nd op-code).
- A non-prefixed instruction with a 1-byte operand = 2 bytes; with a 2-byte operand = 3 bytes.
- A prefixed instruction with operand bytes = 3 or 4 bytes (e.g. `CE,40,dd` = 3 bytes;
  `CE,D0,ll,hh` = 4 bytes).

---

## §4.3 context (boundary — included for reference)

§4.2 ends on PDF page 45 (printed page 39). §4.3 *Instruction Set List* begins on PDF page 46.
The following two tables from the start of §4.3 fix the symbol vocabulary that the addressing
modes above rely on (notably the immediate/address operand symbols and page/bank registers).

### Table 4.3.2.1 Symbol meanings (relevant subsets)

**Immediate data / operand symbols (encoding-relevant):**

| Symbol | Meaning |
|--------|---------|
| `nn` | 8-bit immediate data (unsigned) |
| `hh` | Absolute address, upper 8 bits (unsigned) |
| `ll` | Absolute address, lower 8 bits (unsigned) |
| `pp` | Page setting data (unsigned) |
| `bb` | Bank setting data (unsigned) |
| `dd` | Signed 8-bit displacement |
| `rr` | 8-bit relative address setting data (signed) |
| `kk` | Vector address setting data (unsigned) |
| `mmnn` | 16-bit immediate data (unsigned) |
| `hhll` | 16-bit absolute address setting data (unsigned) |
| `qqrr` | 16-bit relative address setting data (signed) |

**Page / bank registers referenced by the addressing modes:**

| Symbol | Meaning |
|--------|---------|
| `NB` | New code bank register NB (branch destination bank, MODEL2/3) |
| `CB` | Code bank register CB (current code bank) |
| `EP` | Expand page register EP (used by `[HL]`, `[BR:ll]`, `[hhll]`, `[kk]` via vector, `CALL [hhll]`) |
| `XP` | Expand page register XP for IX (`[IX]`, `[IX+dd]`, `[IX+L]`) |
| `YP` | Expand page register YP for IY (`[IY]`, `[IY+dd]`, `[IY+L]`) |
| `IP` | XP and YP register (paired) |
| `BR` | Base register BR (upper 8 bits for `[BR:ll]`) |

**Memory-notation symbols (addressing-mode operands):**

| Notation | Meaning |
|----------|---------|
| `[HL]` | Memory specified by HL register |
| `[IX]` | Memory specified by IX register |
| `[IX+dd]` | Memory specified by IX register + dd |
| `[IX+L]` | Memory specified by IX register + L register |
| `[IY]` | Memory specified by IY register |
| `[IY+dd]` | Memory specified by IY register + dd |
| `[IY+L]` | Memory specified by IY register + L register |
| `[BR:ll]` | Memory specified by BR register and `ll` |
| `[hhll]` | Memory specified by `hhll` |
| `[kk]` | Vector specified by `kk` |
| `[SP]` | Stack specified by SP |
| `[SP+dd]` | Stack specified by SP + dd |

---

## Encoding cross-reference (derived from §4.3 8-bit transfer table excerpt)

The following machine-code examples from the §4.3.3 `LD` table confirm the encoding rules of
§4.2 (prefix bytes, operand ordering, byte counts). Useful for assembler/emulator/codegen work.

| Mnemonic | Machine code (bytes) | Bytes | Notes |
|----------|----------------------|-------|-------|
| `LD A,A` | `40` | 1 | register-direct, op-code only |
| `LD A,B` | `41` | 1 | register-direct, op-code only |
| `LD A,L` | `42` | 1 | register-direct |
| `LD A,H` | `43` | 1 | register-direct |
| `LD A,BR` | `CE,C0` | 2 | CEH-prefixed, op-code only |
| `LD A,SC` | `CE,C1` | 2 | CEH-prefixed |
| `LD A,#nn` | `B0,nn` | 2 | 8-bit immediate; 1 operand byte |
| `LD A,[BR:ll]` | `44,ll` | 2 | 8-bit absolute; `ll` is the single operand |
| `LD A,[hhll]` | `CE,D0,ll,hh` | 4 | CEH prefix + 16-bit absolute; operand low (`ll`) then high (`hh`) |
| `LD A,[HL]` | `45` | 1 | register-indirect, op-code only |
| `LD A,[IX]` | `46` | 1 | register-indirect |
| `LD A,[IY]` | `47` | 1 | register-indirect |
| `LD A,[IX+dd]` | `CE,40,dd` | 3 | CEH prefix + 1 displacement operand byte |
| `LD A,[IY+dd]` | `CE,41,dd` | 3 | CEH prefix + 1 displacement operand byte |
| `LD A,[IX+L]` | `CE,42` | 2 | CEH-prefixed, op-code only (L is implicit) |
| `LD A,[IY+L]` | `CE,43` | 2 | CEH-prefixed, op-code only |
| `LD A,NB` | `CE,C8` | 2 | MODEL2/3 only |
| `LD A,EP` | `CE,C9` | 2 | MODEL2/3 only |
| `LD A,XP` | `CE,CA` | 2 | MODEL2/3 only |
| `LD A,YP` | `CE,CB` | 2 | MODEL2/3 only |
| `LD B,#nn` | `B1,nn` | 2 | |
| `LD B,[BR:ll]` | `B1`/`4C,ll` | 2 | (`LD B,[BR:ll]` = `4C,ll`) |
| `LD B,[hhll]` | `CE,D1,ll,hh` | 4 | |
| `LD L,[hhll]` | `CE,D2,ll,hh` | 4 | |
| `LD L,[IX+dd]` | `CE,50,dd` | 3 | |
| `LD L,[IY+dd]` | `CE,51,dd` | 3 | |
| `LD L,[IX+L]` | `CE,52` | 2 | |
| `LD L,[IY+L]` | `CE,53` | 2 | |

> Confirms: (1) `CEH` is the prefix for these (non-16-bit-arith) extended forms; (2) for
> 16-bit absolute operands the **low byte (`ll`) is emitted before the high byte (`hh`)**
> (little-endian); (3) register-direct / register-indirect / index-register modes are op-code
> only; (4) `[IX+L]`/`[IY+L]` carry no operand byte (L is implicit) whereas `[IX+dd]`/`[IY+dd]`
> carry one signed displacement byte.

---

*End of §4.1 / §4.2 reference (PDF pages 41–47). Detailed per-instruction encodings continue in
§4.3.3 "Instruction list by functions" beginning PDF page 47.*
