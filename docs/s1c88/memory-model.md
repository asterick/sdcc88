# S1C88 Memory Model — Program & Data Memory, Banking, Paging, Stack

> **Source:** S1C88 Core CPU Manual (MF658-05)
> **Printed manual sections covered:** Chapter 2 ARCHITECTURE — §2.3 Program Memory (§2.3.1–§2.3.4) and §2.4 Data Memory (§2.4.1–§2.4.4)
> **PDF page range covered:** pages 18–26 (with peeks at pp. 17 and 27 for spill).
> **Printed page numbers in this range:** 12–21 (manual prints "12"–"20" plus the start of "21"/Chapter 3).
>
> This file is the authoritative description of the S1C88's 24-bit / 16 MB address space, the bank-mapping (program memory) and paging (data memory) systems, the branch/banking interaction, and the stack model. Values transcribed verbatim from the machine-extracted text; reconstructed figures are marked *(figure not captured)*.

---

## Address-space overview

The S1C88 has a **16 M byte** (24-bit, addresses `000000H`–`FFFFFFH`) physical address space, but the CPU core itself is an 8-bit CPU with a **64 K byte (16-bit) logic space** (`0000H`–`FFFFH`). Two systems bridge the gap:

- **Program memory** uses a **bank-mapping** system (32 K-byte banks), addressed by **CB:PC**.
- **Data memory** uses a **paging** system (64 K-byte pages), addressed by the page registers **EP / XP / YP** prefixing index registers / immediate addresses, and by **SPP** for the stack.

The split between program and data memory:

| Region | Address range | Size | Notes |
|---|---|---|---|
| Program memory field | `000000H`–`7FFFFFH` | first 8 M bytes | "programming field" |
| Remainder usable as data memory | up to `FFFFFFH` | rest of the 16 M space | everything except the program field can be data memory |

**Model dependence.** MODEL0 and MODEL1 are limited to a maximum 64 K-byte address space (16-bit address bus), so the program field is limited to 64 K or less and no bank/page management is needed. MODEL2 and MODEL3 use the full bank/page model described here. Multiplication/division (and the "maximum mode" 24-bit return-address behavior) further distinguish the models — see notes inline.

---

## 2.3 Program Memory

### 2.3.1 Configuration of the program memory

The first **8 M bytes** (`000000H`–`7FFFFFH`) of the 16 M-byte address space are designed to be used as the programming field. For MODEL0/MODEL1 the address space (and thus program field) is limited to ≤ 64 K bytes.

The S1C88 uses a **bank mapping system** to manage program memory beyond the 64 K logic space of the 8-bit CPU. The maximum 8 M-byte program memory is divided into **32 K-byte banks**, numbered **bank 0 through bank 255** (256 banks).

Two banks are laid out on the 64 K-byte logic space such that they logically continue as logic address `0000H`–`FFFFH`. The program executes within that logic address space; addressing within the logic space is done by the **PC (program counter)**.

- **Common area** = logic `0000H`–`7FFFH` (lower 32 K). Fixed to **bank 0** (physical `000000H`–`007FFFH`). Because it is fixed, the logic address equals the physical address here.
  - `000000H`–`0000FFH` is allocated to the exception-processing (e.g. interrupt) vector table. (See "3.5.2 Exception processing factor and vectors.") Because the common area is fixed, there is no need to allocate exception vectors per bank, and general-purpose subroutines can also live in the common area.
- **Bank area** = logic `8000H`–`FFFFH` (upper 32 K). The bank laid out here is selected by the **CB (code bank) register**.
  - The bank in this area can be optionally selected by the program in MODEL2/MODEL3 (maximum mode).
  - For **MODEL0/MODEL1**, the bank area is **fixed to bank 1**.
  - For the **minimum modes** of MODEL2/MODEL3, it is **fixed to one optionally selected bank**.

#### Fig. 2.3.1.1 Configuration of program memory

*(figure — transcribed as tables)*

Logic space (64 K):

| Logic region | Logic address range | Size |
|---|---|---|
| Common area | `0000H`–`7FFFH` | 32 K byte |
| Bank area | `8000H`–`FFFFH` | 32 K byte |

Physical space (banks, 32 K each):

| Bank | Physical address range | Size |
|---|---|---|
| Bank 0 | `000000H`–`007FFFH` | 32 K byte |
| Bank 1 | `008000H`–`00FFFFH` | 32 K byte |
| Bank 2 | `010000H`–`017FFFH` | 32 K byte |
| Bank 3 | `018000H`–`01FFFFH` | 32 K byte |
| … | `020000H` … | … |
| Bank 254 | `7EFFFFH` (range ends `7F0000H`–`7F7FFFH`*) | 32 K byte |
| Bank 255 | `7F8000H`–`7FFFFFH` | 32 K byte |

> Extraction note: the raw page-18 text lists the boundary markers `020000H`, `7EFFFFH`, `7F0000H`, `7F7FFFH`, `7F8000H`, `7FFFFFH` in a column without clean per-row pairing. By the uniform 32 K stride: bank N occupies physical `N×8000H` … `N×8000H + 7FFFH`. Thus bank 254 = `7F0000H`–`7F7FFFH` and bank 255 = `7F8000H`–`7FFFFFH`. (`7EFFFFH` is the last byte of bank 253.)

---

### 2.3.2 PC (Program counter) and CB (Code bank register)

The **PC** holds the program address to be executed. Its content is an address within the 64 K logic space; it addresses program memory that logically continues across the 32 K common area and 32 K bank area (which are *not* physically contiguous).

- The **common area** is fixed to **bank 0** of the physical address.
- For the **bank area**, one optional bank out of 256 can be selected (MODEL2/MODEL3).
- **CB (code bank)** is the register that indicates the bank address (**0–255**) allocated to the bank area.

#### Physical-address formation (Fig. 2.3.2.1, MODEL2/3)

The physical address output to the bus is built inside the CPU from PC and CB. Key rules:

- The **MSB of the PC (bit 15)** selects common vs. bank area: `0` = common area, `1` = bank area. **The MSB itself is NOT output to the address bus** — be aware of this during system development.
- The lower **15 bits of the PC (bits 0–14)** drive address bus lines **A00–A14**.
- For **common-area access** (`PC[15] = 0`, logic `0000H`–`7FFFH`): the page/bank byte output to **A15–A22** is **`00H`** (i.e. bank 0).
- For **bank-area access** (`PC[15] = 1`, logic `8000H`–`FFFFH`): the **8-bit content of CB** is output to **A15–A22**.
- **A23** is reserved exclusively for the data-memory area and **always outputs `0`** during program-memory access (max 8 M program memory ⇒ A23 = 0).

Resulting 24-bit physical address:

| Access type | Condition | A23 | A15–A22 (page/bank byte) | A00–A14 |
|---|---|---|---|---|
| Common area (`0000H`–`7FFFH`) | `PC[15] = 0` | `0` | `00H` | `PC[14:0]` |
| Bank area (`8000H`–`FFFFH`) | `PC[15] = 1` | `0` | `CB[7:0]` | `PC[14:0]` |

So the 24-bit code address is effectively:

```
common:   physical = 0x000000 | (PC & 0x7FFF)
bank:     physical = (CB << 15) | (PC & 0x7FFF)        ; with A23 forced 0
```

> The high address line for the bank byte begins at **A15** (not A16): each bank is 32 K (15-bit offset), so the bank number shifts left by 15 bits, not 16. This differs from the data-memory page model (16-bit offset, page byte on A16–A23) described in §2.4.

**MODEL0/MODEL1:** the address bus is 16 bits, so the PC content is output as-is (no CB / bank byte).

#### Value of PC loaded by `LD BA,PC` / `LD HL,PC`

`LD BA,PC` and `LD HL,PC` load the current PC into BA / HL respectively. When the processor fetches one of these load instructions it **increments the PC by two** to point to the next instruction, so the value loaded is the address of the *following* instruction, i.e.:

```
PC loaded = <address of the LD instruction> + 2
```

Example: if `LD BA,PC` is at address `100H`, then `102H` is loaded into BA.

---

### 2.3.3 Bank management

Program execution is basically limited to the bank allocated to the logic space. The bank changes **only when a branch instruction that specifies another bank is executed**.

> **Note:** The CB is **not** updated if the PC overflows during normal (non-branch) execution. On PC overflow, execution **re-starts from the beginning of the common area** (it does not advance to the next physical bank).

#### Bank setting at reset

At initial reset, **CB is initialized to `1`** → **bank 1** is allocated to the bank area. Since the common area is fixed to bank 0, the logic address equals the physical address at reset. This stays until a branch instruction actually performs a bank change.

#### Fig. 2.3.3.1 Bank modification

*(figure — transcribed)* Before a branch, the program first writes the branch-destination bank number into **NB**. The branch then transfers NB → CB:

| Register | Before branch | At/after branch |
|---|---|---|
| NB | (set to) branch-destination bank | (holds destination bank) |
| CB | current bank | branch-destination bank |
| PC | `xxxxH` → `yyyyH` (current bank) | `zzzzH` (destination bank) |

---

### 2.3.4 Branch instruction

Branch instructions modify **PC** (and **CB** when a bank change is involved) to branch the program to an optional address.

#### Table 2.3.4.1 Types of branch instructions

| Type | Condition | Instruction(s) |
|---|---|---|
| PC relative jump | Conditional / Unconditional | JRS, JRL, DJR |
| Indirect jump | Unconditional | JP |
| PC relative call | Conditional / Unconditional | CARS, CARL |
| Indirect call | Unconditional | CALL |
| Return | Unconditional | RET, RETS, RETE |
| Software interrupt | Unconditional | INT |

When a **conditional** branch's condition is not met, it does not branch and instead executes the instruction following the branch.

#### PC relative jump (JRS, JRL, DJR)

Adds a relative address (displacement) given by the operand to the PC and branches there; permits relocatable programming. The displacement is a signed offset from the branch-point address to the destination:

| Instruction | Displacement width | Range |
|---|---|---|
| JRS | 8-bit two's-complement | −128 … 127 |
| JRL | 16-bit two's-complement | −32768 … 32767 |

The branch destination (PC + relative) is a **logic** address.

- **`JRS`**: 1 unconditional + **20 conditional** jump variants.
- **`JRL`**: 1 unconditional + **4 conditional** jump variants.
- **`DJR NZ,rr`**: subtracts 1 from the **B register**; if the result ≠ 0 it performs a `JRS` unconditional jump. Lets you write a simple repeat/wait loop with B as the counter.

  Example — 50-cycle wait routine:
  ```
  LD   B,#12      ; set B initial value (2 cycles)
  DJR  NZ,$       ; repeat until B = 0 (48 cycles)
  ```

#### Bank specification for branches (NB / new code bank)

**CB cannot be modified directly by the program.** Instead, the **NB (new code bank)** register is loaded with the destination bank number (0–255) **before** the branch:

```
LD  NB,A        ; destination bank in A register
LD  NB,#bb      ; destination bank as 8-bit immediate
```

When the branch is actually taken, **NB → CB** and the new bank is selected for the bank area. If a conditional branch is **not** taken, the reverse happens: **CB → NB** (so NB is reloaded with the current bank). Consequently, if you execute a branch *without* having set NB, the branch stays within the current logic space / current bank.

You can branch to another bank by setting NB first, **but the branch destination strictly cannot specify a physical address within the logic space** — destinations are logic addresses; the bank is selected via NB/CB.

`JP HL` unconditionally branches to the address held in HL. Because it turns an operation result directly into a branch target, it is useful for jump tables.

#### Fig. 2.3.4.1 PC relative jump operation *(JRL example)*

Example `JRL $+57H` crossing from bank 1 to bank 2 after `LD NB,#2`:

| Register | Before | After branch |
|---|---|---|
| NB | `01H` → `02H` (set by `LD NB,#2`) | `02H` |
| CB | `01H` | `02H` |
| PC | `A06DH` → `A070H` | `A0C7H` |

Arithmetic: `A070H + 57H = A0C7H` (logic). Physical: logic `A0C7H` in bank 2 = `0120C7H`. (Sample physical addresses in bank 1: `A06DH`→`00A06DH`, `A070H`→`00A070H`, `A073H`→`00A073H`.)

#### PC relative call (CARS, CARL)

Adds the operand's relative address to PC and **calls** the subroutine there.

| Instruction | Displacement width | Range |
|---|---|---|
| CARS | 8-bit two's-complement | −128 … 127 |
| CARL | 16-bit two's-complement | −32768 … 32767 |

> Extraction note: page-21 text says the 16-bit range is for "the `CARS` instruction" in one spot — that is an OCR/source typo; by the JRS/JRL pattern and the stated widths, the 16-bit −32768…32767 range belongs to **CARL**.

Destination is a logic address; another bank is reached by pre-setting NB (again, no physical-address-within-logic-space targeting).

- **`CARS`**: 1 unconditional + **20 conditional** call variants.
- **`CARL`**: 1 unconditional + **4 conditional** call variants.

On a subroutine call, the **PC value** (top address of the instruction following the call) is **pushed** onto the stack as return information. In the **maximum mode of MODEL2/3**, the **CB value is also pushed** (in addition to PC), so the return restores the calling bank.

#### Indirect jump (JP)

`JP [kk]` indirectly specifies the destination. It loads memory `00kk` (kk = `00H`–`FFH`, **page fixed at 0**) into the low 8 bits of PC and memory `00kk+1` into the high 8 bits of PC, then branches unconditionally. The `00kk` location is set up as the **vector field** for exception processing and software interrupts.

#### Indirect call (CALL)

`CALL [hhll]` indirectly specifies the subroutine address. It loads memory `hhll` (hhll = `0000H`–`FFFFH`, **page specified by the EP register**) into the low 8 bits of PC and `hhll+1` into the high 8 bits, then calls unconditionally. On the call it pushes the **PC** (top address of the next instruction) and, in **MODEL2/3 maximum mode**, the **CB** as return information.

#### Fig. 2.3.4.2 PC relative call operation *(CARL example)*

`CARL $+57H` in bank 1, after `LD NB,#2`, calling into bank 2:

| Register | Before | After call |
|---|---|---|
| NB | `01H` → `02H` | `02H` |
| CB | `01H` | `02H` |
| PC | `A06DH` → `A070H` | `A0C7H` |

Return-info pushed onto the stack (max mode), shown low-to-high as pushed: PC low `73H`, PC high `A0H`, CB `01H`. (`A073H` = return address = address following the call; `A070H + 57H = A0C7H` is the call target; physical bank-2 target `0120C7H`, subroutine runs to `A110H`/`012110H` then `RET`.)

#### Return instructions (RET, RETS, RETE)

A return instruction returns to the routine that called the subroutine. It **pops** the PC (top address of the instruction following the call) back into PC. In **MODEL2/3 maximum mode**, the **CB is also popped**, returning to the calling bank.

- **Minimum mode of MODEL2/3 (and MODEL0/1):** only the PC is popped (CB not on stack). **If the bank is changed during/after the call, a correct return is impossible** even with a return instruction. (Minimum mode pushes only PC, so > 64 K program memory cannot be used.)
- **`RET`**: returns to the top address of the instruction following the call, using the return info as-is.
- **`RETS`**: returns by **adding 2** to the saved PC, skipping the 1-byte instruction that follows the call.
- **`RETE`**: the return instruction dedicated to **software-interrupt and exception-processing routines**. Unlike `RET`, the return information includes the **SC (system condition flag)**. (See "3.5 Exceptional Processing Status".)

#### Fig. 2.3.4.3 Return from subroutine

*(figure — transcribed)* Illustrates `RET` vs `RETS` skip behavior:

```
Main routine                       Subroutine
  ...                                CARS rr        -> call
  (logic 1000H) ...                  ...
  (logic 1002H) JRS  rr              ADD  A,B
  (logic 1004H) LD   B,H             JRS  NC,$+3
                                     RET    -> returns to 1002H
                                     RETS   -> returns to 1004H
```

`RET` returns to `1002H`; `RETS` returns to `1004H` (skips the 1-byte instruction at `1002H`).

#### Software interrupt (INT)

`INT [kk]` specifies the vector at address `00kk` (kk = `00H`–`FFH`, **page fixed at 0**) and executes its interrupt routine. It is a kind of indirect call, but the **SC (system condition flag) is also pushed** before branching. Therefore routines entered via `INT` **must return with `RETE`**. (See "3.5 Exceptional Processing Status".)

#### Value of PC for relative branch instructions

For the signed 8-bit relatives **JRS, CARS, DJR** (rr = −128…127):

```
<Branch address> = <Address of branch instruction> + rr + (n − 1)
```
where `n` = length (in bytes) of the branch instruction. Example: `JRS LE,rr` at `100H` → branch address `= 102H + rr`.

For the signed 16-bit relatives **JRL, CARL** (qqrr = −32768…32767):

```
<Branch address> = <Address of branch instruction> + qqrr + 2
```
Example: `JRL C,qqrr` at `100H` → branch address `= 102H + qqrr`.

---

## 2.4 Data Memory

### 2.4.1 Data memory configuration

Everything within the S1C88 address space (max 16 M bytes) **except** the field used as program memory can be used as data memory. RAM, display memory, and I/O memory (peripheral-control registers) are laid out in the data-memory field.

Data memory is managed in **64 K-byte pages**.

- **MODEL0/1:** 64 K address space → no page management needed (single page).
- **MODEL2/3:** configured with up to **255 pages** (maximum).

#### Fig. 2.4.1.1 Data memory configuration

*(figure — transcribed)*

MODEL0/1 (single page):

| Page | Logic range |
|---|---|
| Page 0 (64 K byte) | `0000H`–`FFFFH` |

MODEL2/3 (paged, 64 K each):

| Page | Physical address range | Size |
|---|---|---|
| Page 0 | `000000H`–`00FFFFH` | 64 K byte |
| Page 1 | `010000H`–`01FFFFH` | 64 K byte |
| Page 2 | `020000H`–`02FFFFH` | 64 K byte |
| … | … | … |
| Page 254 | `FE0000H`–`FEFFFFH` | 64 K byte |
| Page 255 | `FF0000H`–`FFFFFFH` | 64 K byte |

> Page N occupies physical `N×10000H` … `N×10000H + FFFFH`. Note pages are 64 K (16-bit offset) — distinct from the 32 K program banks.

---

### 2.4.2 Page registers EP, XP, YP

The data-memory physical space is logically delimited into 64 K-byte pages: the **upper 8 bits** of the 24-bit physical address are the **page section**, the **lower 16 bits** are the **logical address**. Within-page addressing is done mainly by index register and immediate data per the addressing mode.

Three page registers — **EP, XP, YP** — supply the page section in **MODEL2/MODEL3**, chosen by addressing mode.

#### Fig. 2.4.2.1 Data memory addressing — page register per addressing mode

*(figure — transcribed)* The page register prefixes the 16-bit logical address to form the 24-bit physical address. Mapping of addressing mode → page register:

| Addressing mode | Address specification (low 16 bits → A0–A15) | Page register (→ A16–A23) |
|---|---|---|
| Register indirect | IX | **XP** |
| Register indirect | IY | **YP** |
| Register indirect | HL | **EP** |
| Register indirect | BR (base register) | **EP** |
| 16-bit absolute | `<immediate data>` (16-bit) | **EP** |
| 8-bit absolute | `<immediate data>` (8-bit) | **EP** |

So the physical data address is:

```
IX-based:  phys = (XP << 16) | <16-bit logical addr>
IY-based:  phys = (YP << 16) | <16-bit logical addr>
HL/BR/absolute: phys = (EP << 16) | <16-bit logical addr>
```

(See "4.1 Addressing Mode".) The page byte drives **A16–A23**; the logical address drives **A0–A15**.

> Extraction note: page-25 lists page-register values "0 … 255" and bit-width markers (7/15) confirming each page register is 8 bits (page 0–255) and the logical address is 16 bits.

---

### 2.4.3 Stack

The stack is LIFO memory allocated to the **RAM field** of data memory. The CPU uses it to save register information on subroutine calls and exception processing (interrupts), and the program may also use it to save registers at any point. Terminology: storing = **push**, removing = **pop**.

#### Stack pointer SP — semantics

- Data is **pushed from the uppermost address downward** (stack **grows down**).
- The **SP (stack pointer)** holds the current stack address.
- **Push:** SP is **pre-decremented** by 1 per byte (`SP = SP − 1`, then store).
- **Pop:** SP is **post-incremented** by 1 per byte (load, then `SP = SP + 1`).

Because push pre-decrements and pop post-increments, after a push SP points at the **last-pushed (occupied)** byte.

#### Fig. 2.4.3.1 Operation of stack

*(figure — transcribed)* Starting with `SP = 0000H`, pushes go downward from the top of the page (`FFFFH`):

| Step | Action | SP after | Memory state |
|---|---|---|---|
| 1 | Initial status | `0000H` | (empty) |
| 2 | Push "Data 1" (`SP = SP − 1`) | `FFFFH` | `[FFFFH] = Data 1` |
| 3 | Push "Data 2" (`SP = SP − 1`) | `FFFEH` | `[FFFFH]=Data 1`, `[FFFEH]=Data 2` |
| 4 | Pop data (`SP = SP + 1`) | `FFFFH` | Data 2 removed; `[FFFFH]=Data 1` |

(Setting SP's initial value to `0000H` makes the first push wrap to `FFFFH`, i.e. push sequentially toward lower addresses from the page's final address `FFFFH`.)

#### Stack page (SPP) — physical placement

The stack's page within physical memory is fixed by the **SPP0–SPP7 (stack pointer page)** signal, input to the core CPU from the peripheral circuit as a page address. When the stack is accessed, the **SPP content is output as-is to the page section (A16–A23)** of the address; the within-page address comes from **SP**.

```
stack physical address = (SPP[7:0] << 16) | SP[15:0]
```

> **Note:** SP is **undefined at initial reset** — you must initialize it (`LD SP,**`) before using the stack.

#### Subroutine call and stack

On a call, before branching, the CPU pushes as return information: the **top address of the instruction following the call**, plus the **CB** (in MODEL2/3 **maximum** mode). A return instruction pops this back into PC and CB. Nesting (subroutine calling another subroutine) is possible to any depth that fits in the stack's page memory.

#### Fig. 2.4.3.2 Stack consumption at subroutine call

*(figure — transcribed)* Maximum mode (MODEL2/3) — call consumes **3 bytes** (`SP = SP − 3`). Push order / layout (high address first; PC stored low-byte then high-byte, then CB on top of stack figure):

| Stack slot (relative) | Content |
|---|---|
| higher address | CB |
| | PC(H) |
| lower address ← SP | PC(L) |

- **Maximum mode (MODEL2/3):** 3 bytes consumed (CB + PC).
- **Minimum mode (MODEL2/3) and MODEL0/1:** 2 bytes consumed (PC only; **no CB**).

#### Exception processing and stack

On exception processing (e.g. interrupt), return information is pushed like a call, but the **SC** is also included (in addition to PC and CB-in-max-mode).

#### Fig. 2.4.3.3 Stack consumption on exception processing

*(figure — transcribed)* Maximum mode (MODEL2/3) — **4 bytes** consumed (`SP = SP − 4`):

| Stack slot (relative) | Content |
|---|---|
| higher address | CB |
| | PC(H) |
| | PC(L) |
| lower address ← SP | SC |

- **Maximum mode (MODEL2/3):** 4 bytes (PC + CB + SC).
- **Minimum mode (MODEL2/3) and MODEL0/1:** 3 bytes (PC + SC; **no CB**).

#### Other stack operations (PUSH / POP)

Return info is pushed automatically, but **general-purpose registers are not**. To preserve registers across a subroutine/exception routine, push at entry and pop at exit.

Registers that `PUSH` / `POP` can handle:

```
A, B, L, H, BR, SC, EP*, IP (XP and YP)*, BA, HL, IX, IY
```
`*` = does not exist in MODEL0/1 (EP, IP/XP/YP are MODEL2/3 only).

Block push/pop instructions:

- **`PUSH ALL` / `POP ALL`** — MODEL0/1: pushes/pops all the above registers **except SC** with one instruction.
- **`PUSH ALE` / `POP ALE`** — MODEL2/3: same, but also pushes/pops the expanded registers **EP** and **IP (XP and YP)**.

Equivalent sequences (push order top-to-bottom; pop is the mirror):

`PUSH ALL` ≡:
```
PUSH BA
PUSH HL
PUSH IX
PUSH IY
PUSH BR
```
`POP ALL` ≡:
```
POP BR
POP IY
POP IX
POP IY      ; (as listed in source — see note)
POP BA
```

`PUSH ALE` ≡:
```
PUSH BA
PUSH HL
PUSH IX
PUSH IY
PUSH BR
PUSH EP
PUSH IP
```
`POP ALE` ≡:
```
POP IP
POP EP
POP BR
POP IY
POP IX
POP HL
POP BA
```

> Extraction note: the page-26 two-column `PUSH ALL`/`POP ALL` listing is partly garbled (the `POP ALL` column shows `POP IY` twice and is missing an `POP HL`/`POP IX` pairing). The reliable invariant is that **POP reverses PUSH order**, so `POP ALL` should mirror `PUSH ALL` as `POP BR, POP IY, POP IX, POP HL, POP BA`. Verify exact `ALL` register set/order against the instruction-set chapter before relying on it. `ALL` = MODEL0/1; `ALE` = MODEL2/3 (adds EP and IP/XP/YP).

Direct stack-field access without PUSH/POP (useful for passing arguments in stack frames / structured programming) is supported via SP-relative operations:

```
ADD, SUB, CP, INC, DEC, LD
```

> **Note:** The stack is in general-purpose RAM — be careful not to overlap the data field and the stack field.

---

### 2.4.4 Memory-mapped I/O

The S1C88 Family builds the S1C88 core plus various peripheral circuits (I/O ports, etc.). It uses a **memory-mapped I/O** system: peripheral control/data registers are laid out in the **data-memory field**. This "I/O memory" is distinguished from general-purpose RAM by name only — it shares the page control and access methods of data memory, so peripherals are controlled with **normal memory-access instructions**.

#### Fig. 2.4.4.1 Peripheral circuit and memory-mapped I/O

*(figure — transcribed)* I/O memory exposes per-peripheral registers; bits map to peripheral control/status/data:

| I/O memory register | Bits | Maps to |
|---|---|---|
| Data register | D7–D0 | Peripheral input/output data |
| Control/status register | (bits) | Control bits / status bits of the peripheral |

**Display memory:** models with a built-in LCD driver use part of data memory as display memory for segment data. Each display-memory bit corresponds **1:1 with a segment**; setting/clearing the bit turns the segment on/off. Segments are controlled with normal memory-access instructions.

> **Note (write-only regions):** Depending on the model, part of the I/O memory and/or display memory may be **write-only**. Such sections cannot be bit-controlled by read/modify/write (e.g. arithmetic/logic instructions that read-then-write). To bit-control them, keep a **shadow buffer in R/W memory**, modify the buffer, then write the buffer into the primary (write-only) memory.

Refer to the individual S1C88 Family device manuals for peripheral / I/O-memory / display-memory specifics.

---

## Quick reference — address arithmetic cheat sheet

| Address kind | Formula | Bank/page size | Offset width | Page byte on |
|---|---|---|---|---|
| Program, common area | `0x000000 \| (PC & 0x7FFF)` | bank 0 fixed | 15-bit | A15–A22 = `00H` |
| Program, bank area | `(CB << 15) \| (PC & 0x7FFF)`, A23=0 | 32 K bank | 15-bit | A15–A22 = `CB` |
| Data via HL / BR / absolute | `(EP << 16) \| addr16` | 64 K page | 16-bit | A16–A23 = `EP` |
| Data via IX | `(XP << 16) \| addr16` | 64 K page | 16-bit | A16–A23 = `XP` |
| Data via IY | `(YP << 16) \| addr16` | 64 K page | 16-bit | A16–A23 = `YP` |
| Stack | `(SPP << 16) \| SP` | 64 K page | 16-bit | A16–A23 = `SPP` |

Key invariants:
- PC bit 15 (common/bank select) is **never** put on the address bus.
- Program banks are 32 K (shift 15); data pages and stack are 64 K (shift 16).
- A23 = 0 for all program-memory access.
- Reset: **CB = 1** (bank 1 in bank area), bank 0 fixed in common area; **SP undefined** (must be initialized).
- Stack grows **down**; push = pre-decrement, pop = post-increment; SP points at the last-pushed byte.
- Return info byte order on stack (low→high address): for a call max mode = PC(L), PC(H), CB; for an exception max mode = SC, PC(L), PC(H), CB.

---

### Extraction artifacts / caveats summary

1. **Bank 254/255 boundary pairing** on page 18 is ambiguous in the raw column; resolved by the uniform 32 K stride (bank 254 = `7F0000H`–`7F7FFFH`, bank 255 = `7F8000H`–`7FFFFFH`).
2. **CARL displacement** text mislabels the 16-bit range as "CARS" — corrected to CARL by pattern.
3. **`POP ALL` sequence** on page 26 is garbled (duplicate `POP IY`); the correct form mirrors `PUSH ALL`. Confirm against the instruction-set chapter.
4. Figures 2.3.1.1, 2.3.2.1, 2.3.3.1, 2.3.4.1–.3, 2.4.1.1, 2.4.2.1, 2.4.3.1–.3, 2.4.4.1 were image figures (not captured); all reconstructed above from surrounding text and column data.
