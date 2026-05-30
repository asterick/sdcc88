# S1C88 Core CPU — Outline & Architecture (ALU and Registers)

> **Source:** Epson *S1C88 Core CPU Manual* (MF658-05).
> **Printed sections covered:** Chapter 1 *OUTLINE* (§1.1–§1.4) and Chapter 2 *ARCHITECTURE* §2.1–§2.2 (§2.1 Address Space and CPU Model; §2.2 ALU and Registers, including §2.2.1 ALU through §2.2.6 Multiplication and division).
> **PDF page range:** 7–17 (printed page numbers 1–11), with §2.3 onward out of scope.
> **Note:** This content is distilled from machine-extracted plain text of the PDF. Figures/diagrams are lost in extraction; where a figure is referenced it is reconstructed as a table or description and marked *(figure not captured)*. Watch for occasional extraction artifacts noted inline.

---

# 1 OUTLINE

The S1C88 is the core CPU of the 8-bit single-chip microcomputer **S1C88 Family**, using EPSON's original architecture. It has a maximum **16M byte** address space, high speed and abundant instruction sets, a wide range of operating voltages, and low power consumption. It adopts a unified architecture and a peripheral-circuit interface for its **memory-mapped I/O** mode to flexibly meet future expansion of the S1C88 Family.

## 1.1 Features

| Feature | Value |
|---|---|
| Address space | Maximum 16M bytes |
| Instruction cycle | 1–15 cycles (1 cycle = 2 clocks) |
| Instruction set | 608 types |
| Data registers | 2 |
| Index registers | 3 (one is used as a data register) |
| Program counter | yes |
| Stack pointer | yes |
| System condition flag | yes |
| Customize condition flag | yes |
| Exception processing factors | Reset, zero division, and interrupt |
| Exception processing vectors | Maximum 128 vectors |
| Standby function | HALT / SLEEP |
| Peripheral circuit interface | Memory mapped I/O system |

## 1.2 Instruction Set Features

1. Adopts high-efficiency machine cycles plus high speed and abundant instruction sets.
2. Memory management can be done easily by **12 types of addressing modes**.
3. Has effective **16-bit operation functions** including address calculation.
4. Includes powerful **decimal operation functions** such as a decimal operation mode and pack/unpack instructions.
5. Supports realization of various types of special-service microcomputers through **customized flag instructions**.
6. Composed of an instruction system that enables **relocatable programming**, permitting easy development of software libraries.

## 1.3 Block Diagram

*(Fig. 1.3.1 — S1C88 block diagram — figure not captured.)* Reconstructed from the labels in the extracted text:

**Internal registers / datapath blocks:**

| Block | Notes |
|---|---|
| A, B | 8-bit data registers |
| L, H | 8-bit registers (also form HL) |
| IX, IY | 16-bit index registers |
| SP | Stack pointer |
| PC | Program counter |
| LR | (register present in datapath) |
| BR | Base register |
| EP, XP, YP | Expand page registers |
| CB, NB | Code bank / new code bank registers |
| SC | System condition flag |
| TEMP 0, TEMP 1, TEMP 2 | ALU temporary registers |
| ALU | Arithmetic & logic unit |
| Code Address Generator | Generates program addresses |
| Instruction Register / Instruction Decoder / Micro Instruction | Instruction fetch & decode |

**Surrounding CPU control blocks:** Power Supply (V<sub>DD</sub>, V<sub>SS</sub>), Timing Generator, Bus Controller, System Controller, Interrupt Controller, Status Controller, System Clock.

**Bus structure:** Data Bus D0–D7; Address Bus A00–A15 and A16–A23; Internal Data Bus (16-bit). Connects to ROM, RAM and peripheral circuits.

**External signals shown on the diagram:** CLK, PK, PL, WAIT, RD, WR, RDIV, SR, MODE, F0–F3, USLP, BREQ, BACK, SPP0–SPP7, NMI, IRQ1–IRQ3, IMASK, IACK, I0F, I1F, SYNC, STOP, DBS0, DBS1.

## 1.4 Input-Output Signals

### Table 1.4.1(a) Input/output signal list (1)

| Type | Name | Signal name | I/O | Function |
|---|---|---|---|---|
| Power | Power | VDD | I | Inputs the + side power. |
| Power | Ground | VSS | I | Inputs the − side power (GND). |
| Clock | Clock input | CLK | I | Inputs the system clock from the peripheral circuit. |
| Clock | Clock output | PK | O | Outputs the two-phase divided signals generated from the CLK input (phases CLK / PK / PL). |
| Clock | Clock output | PL | O | (Second phase of the divided clock; see above.) |
| Address bus | Address bus | A00–A23 | O | A 24-bit address bus. |
| Data bus | Data bus | D0–D7 | I/O | An 8-bit bidirectional data bus. |
| Bus control signal | Wait | WAIT | I | Controls wait-state insertion for access-time extension during memory access. Valid with LOW level input. |
| Bus control signal | Read | RD | O | Memory (and peripheral) read signal; shifts to LOW during readout. |
| Bus control signal | Write | WR | O | Memory (and peripheral) write signal; shifts to LOW during writing. |
| Bus control signal | Read interrupt vector address | RDIV | O | Interrupt vector address read signal; shifts to LOW during readout of the vector address. |
| System control signal | Reset | SR | I | A LOW level input shifts the CPU into the reset status. |
| System control signal | Mode setting | MODE | I | Sets CPU operation mode via the peripheral circuit. LOW = Minimum mode; HIGH = Maximum mode. |
| System control signal | Customize condition flag | F0–F3 | I | Status signals input by a peripheral circuit; meaning differs per peripheral circuit. |
| System control signal | Micro sleep | USLP | O | Set HIGH 1 cycle prior to the CPU entering SLEEP from the SLP instruction. The peripheral circuit controls oscillation stop based on this signal. |
| System control signal | Bus authority request | BREQ | I | Bus authority request for peripheral DMA. LOW level causes the CPU to release the bus; address bus, data bus and read/write signals go to high impedance. |
| System control signal | Bus authority acknowledge | BACK | O | Response indicating bus authorization has been released; LOW when released. |
| System control signal | Stack pointer page | SPP0–SPP7 | I | Page address of the stack pointer specified by the peripheral circuit. When SP accesses memory this address is output to the page section (AD16–AD23) of the address bus. |

> The S1C88's USLP timing reference: "1 cycle". Refer to Chapter 3, *CPU OPERATION AND PROCESSING STATUSES*, for the timing of each signal and related information.

### Table 1.4.1(b) Input/output signal list (2)

| Type | Name | Signal name | I/O | Function |
|---|---|---|---|---|
| Interrupt signal | Non-maskable interrupt | NMI | I | Interrupt not maskable by software. Sensed at the falling edge. |
| Interrupt signal | Interrupt request 3 | IRQ3 | I | Software-maskable interrupt. Priority level 3; sensed at LOW level. |
| Interrupt signal | Interrupt request 2 | IRQ2 | I | Software-maskable interrupt. Priority level 2; sensed at LOW level. |
| Interrupt signal | Interrupt request 1 | IRQ1 | I | Software-maskable interrupt. Priority level 1; sensed at LOW level. |
| Interrupt signal | Interrupt mask | IMASK | I | Interrupt mask input by the peripheral circuit. When the SP page section (configured on the peripheral circuit) is accessed, LOW is input here and NMI, IRQ3, IRQ2, IRQ1 are masked. |
| Interrupt signal | Interrupt acknowledge | IACK | O | Response indicating an interrupt request has been received; LOW when received. The peripheral circuit holds the vector address on this signal. Also goes LOW when exception processing by reset and zero division is executed. |
| Status signal | Interrupt flag | I0F | O | Outputs the status of the interrupt flags (I0, I1) in the system condition flag (SC). |
| Status signal | Interrupt flag | I1F | O | (Second interrupt-flag status output; see I0F.) |
| Status signal | First operation code fetch signal | SYNC | O | Active when the CPU fetches the first operation code. HIGH during the bus cycle of the first op-code fetch. Interrupt is sampled at the rising edge of this signal. |
| Status signal | Stop signal | STOP | O | LOW when the CPU shifts into: CPU stop by HALT instruction; CPU stop by SLP instruction; or bus authorization released by LOW BREQ. |
| Status signal | Data bus status | DBS0 | O | 2-bit status indicating data bus state (see table below). |
| Status signal | Data bus status | DBS1 | O | (Second bit of the data bus status; see table below.) |

**Data bus status decode** (note: text labels the bits "DSB1/DSB0", an extraction typo for DBS1/DBS0):

| DBS1 | DBS0 | State |
|---|---|---|
| 0 | 0 | High impedance |
| 0 | 1 | Interrupt vector address read |
| 1 | 0 | Memory write |
| 1 | 1 | Memory read |

> **Note:** Input/output signals may differ from the above; e.g. a peripheral-circuit signal may be added by each device of the S1C88 Family.

---

# 2 ARCHITECTURE

The S1C88 has a maximum 16M byte address space for large-scale applications. This chapter explains the address space, memory control, and register configuration.

## 2.1 Address Space and CPU Model

Four CPU models (**MODEL0**–**MODEL3**) are defined according to the size of the address space and whether a multiplication/division instruction is present, allowing selection by microcomputer service and application scope.

For **MODEL2** and **MODEL3** either the **minimum mode** (programming field max 64K bytes) or the **maximum mode** (max 8M bytes) can be selected via the **MODE** terminal of the CPU.

Program memory is managed by dividing into a **bank for each 32K bytes**; data memory into **one page for each 64K bytes**. (See §2.3 Program Memory and §2.4 Data Memory.)

> **Note:** Memory configuration varies for the respective devices of the S1C88 Family; refer to the manual for each device.

### Table 2.1.1 CPU model

| CPU model | Address space | Multiplication/division instruction |
|---|---|---|
| MODEL0 | 64K bytes | Not available |
| MODEL1 | 64K bytes | Available |
| MODEL2 | 16M bytes | Not available |
| MODEL3 | 16M bytes | Available |

### Table 2.1.2 Setting of the operation mode (MODEL2/3)

| MODE | Operation mode | Programming area |
|---|---|---|
| 0 | Minimum mode | Maximum 64K bytes |
| 1 | Maximum mode | Maximum 8M bytes |

### Fig. 2.1.1 Memory map *(figure not captured)*

Reconstructed from the extracted address labels. The map distinguishes three layouts:

**MODEL0/1 (64K byte space)** — a single 64K-byte Page 0, split into two 32K-byte banks:

| Address range | Region |
|---|---|
| 000000H–007FFFH | ROM — Bank 0 (32K bytes) |
| 008000H–00FFFFH | RAM, peripheral I/O — Bank 1 (32K bytes) |

**MODEL2/3 (minimum mode)** — pages of 64K bytes each (Page 0 … Page 255):

| Address range | Region |
|---|---|
| 000000H–00FFFFH | Page 0 (64K bytes) — ROM / RAM, peripheral I/O |
| 010000H–01FFFFH | Page 1 (64K bytes) |
| … | … |
| (Page 127, Page 128, Page 129 …) | … |
| FF0000H–FFFFFFH | Page 255 (64K bytes) |

**MODEL2/3 (maximum mode)** — 32K-byte banks (Bank 0 … Bank 255), the program field being the first 8M bytes:

| Address range | Region |
|---|---|
| 000000H–007FFFH | Bank 0 (32K bytes) — ROM |
| 008000H–00FFFFH | Bank 1 (32K bytes) |
| 010000H–017FFFH | Bank 2 (32K bytes) |
| 018000H–01FFFFH | Bank 3 (32K bytes) |
| 020000H … 7EFFFFH | (banks 4 … 253) |
| 7F0000H–7F7FFFH | Bank 254 (32K bytes) |
| 7F8000H–7FFFFFH | Bank 255 (32K bytes) |
| 800000H–80FFFFH | Page 0 (64K bytes) — RAM, peripheral I/O |
| 810000H–81FFFFH | Page 1 (64K bytes) |
| 820000H … FEFFFFH | (pages 2 … 254) |
| FF0000H–FFFFFFH | Page 255 (64K bytes) |

> "Bank X is one optional bank from bank 1 to bank 255." The logic-space view pairs the common Bank 0 with a single selected Bank X (each 32K bytes) to form the 64K-byte logic space.

## 2.2 ALU and Registers

### 2.2.1 ALU

The ALU (arithmetic and logic unit) operates between 8-bit and 16-bit data stored in the two temporary registers **TEMP 0** and **TEMP 1**. After the operation the result is stored in the 16-bit temporary register **TEMP 2**, then either written to register/memory or used as address data, according to the instruction.

The **Z** (zero), **C** (carry), **V** (overflow) and **N** (negative) flags are set/reset according to the operation result (see §2.2.3 Flags).

#### Table 2.2.1.1 ALU operation functions

("16-bit operation = G" marks functions with a 16-bit form.)

| Arithmetic function | Instruction(s) | 16-bit operation |
|---|---|---|
| Addition | ADD, ADC | G |
| Subtraction | SUB, SBC | G |
| Logical product | AND | |
| Logical sum | OR | |
| Exclusive OR | XOR | |
| Comparison | CP | G |
| Bit test | BIT | |
| Increment/decrement | INC, DEC | G |
| Multiplication | MLT | |
| Division | DIV | |
| Complement | CPL, NEG | |
| Rotate | RL, RLC, RR, RRC | |
| Shift | SLA, SLL, SRA, SRL | |
| Pack/unpack | PACK, UPCK | |
| Code extension | SEP | |

### 2.2.2 Register configuration

*(Fig. 2.2.2.1 — Register configuration — figure not captured.)* Reconstructed below.

#### Register list

| Register | Width | Bit indices shown | Role |
|---|---|---|---|
| A | 8-bit | 7..0 | Data register |
| B | 8-bit | 7..0 | Data register (upper 8 bits of BA pair) |
| BA | 16-bit | (B:A) | 16-bit data register pair (B = upper, A = lower) |
| L | 8-bit | 7..0 | Data register / displacement; lower half of HL |
| H | 8-bit | 7..0 | Data register; upper half of HL |
| HL | 16-bit | 15..0 | Index (data) register |
| IX | 16-bit | 15..0 | Index register |
| IY | 16-bit | 15..0 | Index register |
| PC | 16-bit | 15..0 | Program counter |
| SP | 16-bit | 15..0 | Stack pointer |
| BR | 8-bit | 7..0 | Base register |
| SC | 8-bit | 7..0 | System condition flag |
| CC | 4-bit | F3 F2 F1 F0 | Customize condition flag |

Standard section above is **common for MODEL0–MODEL3**.

**Expansion section (MODEL2, MODEL3 only)** — all 8-bit, bits 7..0:

| Register | Width | Role |
|---|---|---|
| NB | 8-bit | New code bank register |
| CB | 8-bit | Code bank register |
| EP | 8-bit | Expand page register |
| XP | 8-bit | Expand page register for IX |
| YP | 8-bit | Expand page register for IY |

#### A and B registers

Respective **8-bit data registers**; perform data transfer/operation with other registers and/or data memory, transfer of immediate data, and operations. Used independently for 8-bit transfer/operations, and used as a **BA pair** (B = upper 8 bits) for 16-bit transfer/operations.

#### HL register

A **16-bit index register** used for indirect addressing of data memory (specifies the address within the page). Performs 16-bit data transfer/operations with other registers/memories. Can also be split into **8-bit H and L** data registers; in that case **L can serve as a displacement** for indirect addressing by IX and IY. (See §2.4 Data Memory; §4.1 Addressing Mode.)

#### IX and IY registers

Respective **16-bit index registers** used for indirect addressing of data memory (specifies the address within the page). Perform 16-bit data transfer/operations with other registers/memories. (See §2.4 Data Memory; §4.1 Addressing Mode.)

#### PC (Program Counter)

A **16-bit** counter register that addresses program memory and indicates the next address to be executed. (See §2.3 Program Memory.)

#### SP (Stack Pointer)

A **16-bit** counter register that indicates the stack address (address within the stack page). Performs 16-bit data transfer/operations with other registers/memories. (See §2.4.3 Stack.)

#### BR (Base Register)

An **8-bit** index register used for upper-8-bit address specification within the page at the time of 8-bit absolute addressing (the lower 8 bits are specified with immediate data). (See §4.1 Addressing Mode.)

#### SC (System Condition Flag)

An **8-bit** flag configured with Z, C, V and N flags (operation result), D and U flags (operation mode), and I0 and I1 flags (interrupt priority level). (See §2.2.3 Flags.) *(The manual text reads "Z, C, V and Z flags" — a typo for Z, C, V and N.)*

#### CC (Customize Condition Flag)

A **4-bit** flag indicating various statuses selected by the peripheral circuit. Set/reset by the peripheral circuit and used as a branch-instruction condition. (See §2.2.3 Flags.)

#### NB (New Code Bank Register)

An **8-bit** register that specifies the program memory bank. Set for MODEL2 and MODEL3. (See §2.3 Program Memory.)

#### CB (Code Bank Register)

An **8-bit** register that indicates the currently selected program-memory bank. When data is set into NB, it is loaded into CB and a new bank is selected. Set for MODEL2 and MODEL3. (See §2.3 Program Memory.)

#### EP, XP and YP (Expand Page Registers)

**8-bit** registers that specify the data-memory page. **EP** is used for indirect addressing by HL or absolute addressing by immediate data. **XP** and **YP** are used for indirect addressing by IX and IY, respectively. Set for MODEL2 and MODEL3. (See §2.4.2 Page registers EP, XP, YP; §4.1 Addressing Mode.)

### 2.2.3 Flags

The S1C88 has the **system condition flag (SC)** indicating operation-result status within the CPU, and the **customize condition flag (CC)** indicating peripheral-circuit status.

#### System condition flag (SC)

*(Fig. 2.2.3.1 — System condition flag — figure not captured.)* The SC is an 8-bit register. Bit layout (MSB → LSB):

| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
|---|---|---|---|---|---|---|---|
| I1 | I0 | U | D | N | V | C | Z |

| Flag | Name | Meaning |
|---|---|---|
| Z | zero | Set per operation result |
| C | carry | Set per operation result / rotate-shift |
| V | overflow | Set per complementary-range overflow |
| N | negative | Set per sign of operation result |
| D | decimal | Selects decimal operation mode |
| U | unpack | Selects unpack operation mode |
| I0 | interrupt 0 | Interrupt priority level (low bit) |
| I1 | interrupt 1 | Interrupt priority level (high bit) |

The Z, C, V and N flags are set/reset according to operation results; the I0 and I1 flags are set/reset by interrupt. These flags can also be operated by the following instructions:

```
AND  SC,#nn    ; Resets the optional flag
OR   SC,#nn    ; Sets the optional flag
XOR  SC,#nn    ; Inverts the optional flag
LD   SC,#nn    ; Flag write
LD   SC,A      ; Flag write
POP  SC        ; Flag return
RETE           ; Flag evacuation
```

The Z, C, N and V flags are used for condition judgments at conditional jump/call execution for **JRS** and/or **CARS** instructions. (See §4.4 Detailed Explanation of Instructions.)

Per-flag definitions:

1. **Z (zero) flag** — Set to `1` when the arithmetic-instruction result is `0`; `0` otherwise.

2. **C (carry) flag** — Set to `1` when a carry (from the MSB) is generated by an addition, or when a borrow (to the MSB) is generated by a subtraction/comparison; `0` otherwise. The C flag does **not** vary with the 1-increment/decrement instructions (**INC**, **DEC**). It also varies with rotate/shift instructions, and is **reset to `0`** when multiply/divide (**MLT**, **DIV**) are executed. *(Extraction note: the manual text says "borrow ... by the execution of an addition instruction/comparison instruction" — read as subtraction/comparison.)*

3. **V (overflow) flag** — Set to `1` when the result exceeds the complementary (two's-complement) representation range; reset to `0` when within range. Ranges: 8 bits = −128…127, 16 bits = −32768…32767. The V flag does **not** change with logic instructions (**AND**, **OR**, **XOR** — except when the destination is SC) nor with **INC**/**DEC**. **MLT** resets it to `0`. **DIV** sets it to `1` when the quotient exceeds the 8-bit data range. V indicates overflow of a *complementary* operation, vs. C which indicates over/underflow of an *absolute-value* operation. When performing a complementary operation likely to overflow, check V and correct the result if it is `1`. (See §2.2.4.)

4. **N (negative) flag** — Set to `1` when the result is minus (MSB = `1`), reset to `0` when plus (MSB = `0`). Does **not** change with **INC**/**DEC**.

5. **D (decimal) flag** — Sets the CPU to perform a **decimal operation** (decimal-corrected result) on 8-bit addition/subtraction. `1` = decimal operation; `0` = hexadecimal operation. (See §2.2.5.)

6. **U (unpack) flag** — Sets the CPU to perform an **unpack operation** (treats the upper 4 bits as `0`) on 8-bit addition/subtraction. `1` = unpack operation; `0` = normal 8-bit operation. (See §2.2.5.)

7. **I0 and I1 (interrupt) flags** — Set the interrupt priority level. The CPU accepts interrupts set higher than the level set by these two bits. When an interrupt occurs, they are automatically set to a new value that masks that level and below. (See §3.5.3 Interrupts.)

> Flags that change due to an instruction are marked "↕" in the instruction-set lists. The D and U flags carry a "#", indicating instructions that permit decimal and unpack operations.

#### Customize condition flag (CC)

*(Fig. 2.2.3.2 — Customize condition flag — figure not captured.)* The CC register is made up of 4-bit flags:

| Bit 3 | Bit 2 | Bit 1 | Bit 0 |
|---|---|---|---|
| F3 | F2 | F1 | F0 |

Each CC flag (F0–F3) varies according to the signals input to the corresponding F0–F3 terminals of the S1C88 from the peripheral circuit (peripheral-circuit status). The program can branch according to the peripheral-circuit status reflected in each flag — enabling special-purpose microcomputers. CC is used for condition judgment at conditional jump/call execution of **JRS** and/or **CARS** instructions. (See §4.4.)

### 2.2.4 Complementary operation and overflow

The S1C88 uses **complementary (two's-complement)** representation for minus data. Two complement types exist — **1's complement** and **2's complement**; "complement" alone normally means 2's complement, which the S1C88 uses for minus numbers.

Representable ranges:

| Representation | 8-bit range | 16-bit range |
|---|---|---|
| 2's complement | −128 … 127 | −32768 … 32767 |
| 1's complement | −127 … 127 | −32767 … 32767 |

When a complement representation is used, the MSB of a minus number is `1`; that MSB is reflected in the **N (negative)** flag. Conversion instructions provided:

| Instruction | Function |
|---|---|
| CPL | Convert 8-bit data to 1's complement |
| NEG | Convert 8-bit data to 2's complement |
| SEP | Sign-extend an 8-bit complement to 16 bits |

**Example: NEG and SEP instructions**

| Instruction | B reg. | A reg. | N flag |
|---|---|---|---|
| `LD A,#127` | 0000 0000 | 0111 1111 | 0 |
| `NEG A` | 0000 0000 | 1000 0001 | 1 |
| `SEP` | 1111 1111 | 1000 0001 | 1 |

**Complement formulas and value tables**

2's complement (8-bit): `−N = 2⁸ − N = 256 − N`
1's complement (8-bit): `−N = 2⁸ − 1 − N = 255 − N`

| Value | 2's complement (8-bit) | 1's complement (8-bit) |
|---|---|---|
| 127 | 0111 1111 | 0111 1111 |
| 126 | 0111 1110 | 0111 1110 |
| … | … | … |
| 2 | 0000 0010 | 0000 0010 |
| 1 | 0000 0001 | 0000 0001 |
| 0 | 0000 0000 | 0000 0000 |
| −1 | 1111 1111 (= 1 0000 0000 − 0000 0001) | 1111 1110 (= 1111 1111 − 0000 0001) |
| −2 | 1111 1110 (= 1 0000 0000 − 0000 0010) | 1111 1101 (= 1111 1111 − 0000 0010) |
| … | … | … |
| −127 | 1000 0001 (= 1 0000 0000 − 0111 1111) | — |
| −126 | — | 1000 0001 (= 1111 1111 − 0111 1110) |
| −128 | 1000 0000 (= 1 0000 0000 − 1000 0000) | — |
| −127 | — | 1000 0000 (= 1111 1111 − 0111 1111) |

2's complement (16-bit): `−N = 2¹⁶ − N = 65536 − N`
1's complement (16-bit): `−N = 2¹⁶ − 1 − N = 65535 − N`

| Value | 2's complement (16-bit) | 1's complement (16-bit) |
|---|---|---|
| 32767 | 0111 1111 1111 1111 | 0111 1111 1111 1111 |
| 32766 | 0111 1111 1111 1110 | 0111 1111 1111 1110 |
| … | … | … |
| 2 | 0000 0000 0000 0010 | 0000 0000 0000 0010 |
| 1 | 0000 0000 0000 0001 | 0000 0000 0000 0001 |
| 0 | 0000 0000 0000 0000 | 0000 0000 0000 0000 |
| −1 | 1111 1111 1111 1111 | 1111 1111 1111 1110 |
| −2 | 1111 1111 1111 1110 | 1111 1111 1111 1101 |
| … | … | … |
| −32767 | 1000 0000 0000 0001 | — |
| −32766 | — | 1000 0000 0000 0001 |
| −32768 | 1000 0000 0000 0000 | — |
| −32767 | — | 1000 0000 0000 0000 |

*(Extraction note: the source repeated some leading digits, e.g. "327677"/"327666" and "1111 11111111 1111" — corrected above to 32767/32766 and 1111 1111 1111 1111.)*

**Complement expression and the V (overflow) flag**

For absolute-value operations (e.g. address calculation), correct results lie in 0–255 (8-bit) or 0–65535 (16-bit). When an overflow/underflow misses that range, the **C** flag is set to `1`.

When operands are complements, the correct-result range is −128…127 (8-bit) or −32768…32767 (16-bit); correctness cannot be judged by C alone. The **V** flag is therefore set to `1` when the complement representation range is exceeded.

Because the ALU does not differentiate absolute vs. complementary operations, C and V are set/reset purely by whether the result is within the respective range. Consequently **V may also be set to `1` during absolute-value operations** — in that case V has no meaning and must not be checked by the program. Only a complementary operation can judge overflow via V; otherwise judge by whether the application's data is signed.

**Example: 8-bit operations and the V / C flags**

Addition (`ADD A,B`):

| A reg. | B reg. | Result (A reg.) | V flag | C flag |
|---|---|---|---|---|
| 0101 1010 | 1010 0101 | 1111 1111 | 0 | 0 |
| 0101 1011 | 1010 0101 | 0000 0000 | 0 | 1 |
| 0101 1011 | 0010 0101 | 1000 0000 | 1 | 0 |

Subtraction (`SUB A,B`):

| A reg. | B reg. | Result (A reg.) | V flag | C flag |
|---|---|---|---|---|
| 0101 1010 | 0101 1010 | 0000 0000 | 0 | 0 |
| 0101 1010 | 0101 1011 | 1111 1111 | 0 | 1 |
| 0101 1010 | 1101 1010 | 1000 0000 | 1 | 1 |

### 2.2.5 Decimal operation and unpack operation

For the following 8-bit arithmetic instructions, the S1C88 can perform **decimal** operations (in addition to normal hexadecimal), **unpack** operations, and combinations thereof, controlled by the **D (decimal)** and **U (unpack)** flags.

**Arithmetic instructions permitting decimal and unpack operations:** `ADD, ADC, SUB, SBC, NEG` (all 8-bit). A "#" on the D/U flag columns of the instruction-set list marks these.

#### Decimal operation

With **D = 1**, the listed instructions produce a result in **BCD** (binary-coded decimal). Set D (e.g. `OR SC,#00010000B`) and put operands in BCD before executing the instruction. If operands are not BCD, the result may be incorrect.

**SC flags after a decimal operation:**

| Flag | Condition |
|---|---|
| N | Always reset (0) |
| V | Always reset (0) |
| C | Set (1) on carry from / borrow to the 2-digit decimal value; else reset (0) |
| Z | Set (1) when result = 0; reset (0) when result ≠ 0 |

**Examples** (operand/result values are decimal/BCD):

| Instruction | A reg. (set) | B reg. (set) | A reg. (result) | N | V | C | Z |
|---|---|---|---|---|---|---|---|
| `ADD A,B` | 55 | 28 | 83 | 0 | 0 | 0 | 0 |
| `ADD A,B` | 74 | 98 | 72 | 0 | 0 | 1 | 0 |
| `SUB A,B` | 55 | 55 | 00 | 0 | 0 | 0 | 1 |
| `SUB A,B` | 55 | 28 | 27 | 0 | 0 | 0 | 0 |
| `SUB A,B` | 74 | 98 | 76 | 0 | 0 | 1 | 0 |

#### Unpack operation

With **U = 1**, the listed 8-bit arithmetic instructions operate in **unpack** format: the upper 4 bits of operands are disregarded (treated as `0`) and only the lower 4 bits are operated on. After execution, only the lower-4-bit result is output, with `0` in the upper 4 bits. Because each unpack value stores one digit per memory address, digit matching of operands is easy (reduces to memory-address pointing).

**`<Example of ADD instruction>`** *(figure reconstructed from text)* — bit fields MSB / 24 / 23 / LSB:

```
       MSB  24  23  LSB
 Augend:      Undefined | (lower digit)     (register or memory)
+Addend:      Undefined | (lower digit)     (register or memory)
 Result:           0    | (sum, lower 4 b)  (register or memory)
```

**SC flags after an unpack operation** (affects only the lower 4 bits; "2³ bit" = bit 3):

| Flag | Condition |
|---|---|
| N | Set (1) when bit 3 is `1`; reset (0) when bit 3 is `0` |
| V | Set (1) when the result exceeds the 4-bit complementary range (−8…7); reset (0) when within range |
| C | Set (1) on carry from / borrow to bit 3; else reset (0) |
| Z | Set (1) when the lower 4 bits = 0; reset (0) when ≠ 0 |

**Example: `ADD A,B`** (unpack):

| A reg. (set) | B reg. (set) | A reg. (result) | N | V | C | Z |
|---|---|---|---|---|---|---|
| 20H | D0H | 00H | 0 | 0 | 0 | 1 |
| 2EH | 53H | 01H | 0 | 0 | 1 | 0 |
| C7H | 52H | 09H | 1 | 1 | 0 | 0 |

#### Auxiliary pack/unpack instructions

**PACK** and **UPCK** mutually convert unpack format and pack format (normal 8-bit data format).

**PACK** — converts the unpack-format data of the **BA** register into pack format and stores it in **A**:

```
 BA reg:  B=*m  A=*n   ->   A = m n    (* = ignored upper nibble)
```

| BA reg. (set) | A reg. (result) | N V C Z |
|---|---|---|
| 38C4H | 84H | Unchanged |

**UPCK** — converts the 8-bit data of **A** into unpack format and stores it in **BA**:

```
 A reg:  m n   ->   BA reg: B=0 m, A=0 n
```

| A reg. (set) | BA reg. (result) | N V C Z |
|---|---|---|
| 84H | 0804H | Unchanged |

### 2.2.6 Multiplication and division

**MODEL1** and **MODEL3** have multiply/divide; **MODEL0** and **MODEL2** do not (the MLT/DIV instructions cannot be used there).

#### Multiplication (MLT)

Executing **MLT** performs **L register × A register**, storing the product in the **HL** register.

**SC flags after MLT:**

| Flag | Condition |
|---|---|
| N | Set (1) when MSB of HL (product) is `1`; reset (0) when `0` |
| V | Always reset (0) |
| C | Always reset (0) |
| Z | Set (1) when HL (product) = 0000H; reset (0) otherwise |

> MLT treats the set values as **unsigned** 8-bit data and performs an unsigned operation; therefore the N flag does **not** indicate a sign. Even multiplying two negative numbers (e.g. C8H × A5H) may not leave N reset.

**Examples** (result: HL = product):

| L reg. | A reg. | HL reg. | N | V | C | Z |
|---|---|---|---|---|---|---|
| 00H | 64H | 0000H | 0 | 0 | 0 | 1 |
| 64H | 58H | 2260H | 0 | 0 | 0 | 0 |
| C8H | 58H | 44C0H | 0 | 0 | 0 | 0 |
| A5H | 93H | 5EBFH | 0 | 0 | 0 | 0 |
| C8H | A5H | 80E8H | 1 | 0 | 0 | 0 |

#### Division (DIV)

Executing **DIV** performs **HL register ÷ A register**: the **quotient** is stored in the **L** register and the **remainder** in the **H** register. If the quotient exceeds 8 bits, the **V** (overflow) flag is set and HL retains the preceding dividend. Executing DIV with **A = 0** generates a **zero-division exception**.

**SC flags after DIV:**

| Flag | Condition |
|---|---|
| N | Set (1) when MSB of L (quotient) is `1`; reset (0) when `0` |
| V | Set (1) when the quotient is not restricted to 8 bits or less; reset (0) when restricted |
| C | Always reset (0) |
| Z | Set (1) when L (quotient) = 00H; reset (0) otherwise |

**SC operating examples** (`nz` = nonzero 8-bit or 16-bit data):

| HL reg. | A reg. | N | V | C | Z | Comment |
|---|---|---|---|---|---|---|
| nz | nz | ↕ | ↕ | 0 | ↕ | |
| 0000H | nz | 0 | 0 | 0 | 1 | |
| nz | 00H | 1 | 1 | 0 | 0 | Zero division exception processing has occurred |
| 0000H | 00H | 1 | 1 | 0 | 0 | Zero division exception processing has occurred |

**DIV execution examples** (result: L = quotient, H = remainder):

| HL reg. | A reg. | L reg. | H reg. | A reg. | N | V | C | Z |
|---|---|---|---|---|---|---|---|---|
| 1A16H | 64H | 42H | 4EH | 64H | 0 | 0 | 0 | 0 |
| 332CH | 64H | 83H | 00H | 64H | 1 | 0 | 0 | 0 |
| 0000H | 58H | 00H | 00H | 58H | 0 | 0 | 0 | 1 |
| 0301H | 02H | 01H | 03H | 02H | 1 | 1 | 0 | 0 |

In the last case (`0301H ÷ 02H`) the quotient exceeds 8 bits, so HL is held and no result is output. Handle this by splitting the dividend into upper and lower 8 bits:

```
; <An execution example of 0301H ÷ 02H>
        LD   HL,#0003H   ; Dividend = upper 8 bits
        LD   A,#02H      ; Divisor
        DIV               ; L = quotient, H = remainder
        LD   [hhll],L    ; Stores the quotient (upper 8 bits) into memory
        LD   L,#01H      ; Dividend = H register + upper 8 bits
        DIV               ;
```

Step results:

| HL reg. | A reg. | L reg. | H reg. | A reg. | N | V | C | Z |
|---|---|---|---|---|---|---|---|---|
| 0003H | 02H | 01H | 01H | 02H | 0 | 0 | 0 | 0 |
| 0101H | 02H | 80H | 01H | 02H | 1 | 0 | 0 | 0 |

Final: Quotient = `0180H`, Remainder = `01H`.

---

*End of covered range (§2.2.6). §2.3 Program Memory and later sections are out of scope for this file.*
