# S1C88 — CPU Operation, Processing Status, and Interrupts

> **Source:** S1C88 Core CPU Manual (MF658-05)
> **Chapter:** 3 — CPU OPERATION AND PROCESSING STATUS
> **Sections covered:** 3.1 through 3.7.2 (3.1 Timing Generator and Bus Control; 3.2 Outline of Processing Statuses; 3.3 Reset Status; 3.4 Program Execution Status; 3.5 Exception Processing Status incl. 3.5.1–3.5.4; 3.6 Bus Authority Release Status; 3.7 Standby Status incl. 3.7.1 HALT / 3.7.2 SLEEP)
> **PDF page range:** 27–40 (printed page numbers 21–34)
> **Provenance note:** Transcribed from machine-extracted plain text (`docs/_extract/core-cpu/page-027.txt` … `page-040.txt`). All timing diagrams and sequence figures in the source are bitmap figures whose signal/label tokens were OCR'd as loose lists; they are reconstructed here as descriptions/tables and marked *(figure not captured)*. Hex addresses, vector numbers, and cycle counts are preserved verbatim.

---

## 3 CPU Operation and Processing Status (overview)

The CPU operates in synchronization with the system clock. CPU processing includes several status types: the status that sequentially executes programs, the standby status, and others. This chapter explains the various processing statuses (including interrupts) and the timing of the operations.

---

## 3.1 Timing Generator and Bus Control

Explains the clock and bus control on which CPU operation is based.

### 3.1.1 Bus cycle

The S1C88 timing generator generates a two-phase divided signal from the input clock **CLK** and factors CLK into **states**.

- One **state** = 1/2 cycle of CLK.
- One **bus cycle** (the instruction execution unit) = **four states** (T1, T2, T3, T4).
- The numeric values listed as "cycles" in the instruction set list indicate the **number of bus cycles**.

The two-phase divided clocks are designated **PK** and **PL**.

**Data bus status output (DBS0, DBS1):** In each bus cycle, the data bus status is output externally on the **DBS0** and **DBS1** signals as a 2-bit status. A peripheral circuit can easily perform tasks such as directional control of the bus driver using this signal.

**Table 3.1.1.1 — State of data bus**

| DBS1 | DBS0 | State |
|------|------|-------|
| 0 | 0 | High impedance |
| 0 | 1 | Interrupt vector address read |
| 1 | 0 | Memory write |
| 1 | 1 | Memory read |

The timing chart for each bus status follows.

#### High impedance (internal register access)

During an internal register access, the data bus goes into the high-impedance state. Both the read signal **RD** and the write signal **WR** are fixed to a **high** level, and the address bus outputs a **dummy address** during the bus cycle period.

*Fig. 3.1.1.2 — Bus cycle at the time of internal register access (figure not captured).* Signals shown: CLK, PK, PL, A00–A23 (= dummy address), WR, RD, D0–D7, DBS1, DBS0, across states T1–T4 (one bus cycle).

#### Interrupt vector address read

The interrupt vector address is read from the data bus **between the T2–T3 states**. During this read, a dedicated interrupt-vector-address read signal **RDIV** is output **instead of** the read signal RD (RD is not asserted). The address bus outputs a **dummy address** during the bus cycle period.

*Fig. 3.1.1.3 — Bus cycle at the time of the interrupt vector address read (figure not captured).* Signals: CLK, PK, PL, A00–A23 (= dummy address), RDIV, WR, RD, D0–D7 (= interrupt vector address), DBS1, DBS0, across T1–T4.

#### Memory write

On a memory write, written data is output to the data bus **between the T2–T4 states** and the write signal **WR** is asserted in the **T3 state**. The address bus outputs the **target address** during the bus cycle period.

*Fig. 3.1.1.4 — Bus cycle at the time of memory write (figure not captured).* Signals: CLK, PK, PL, A00–A23 (= address), WR, RD, D0–D7 (= write data), DBS1, DBS0, across T1–T4.

#### Memory read

On a memory read, the read signal **RD** is asserted **between the T2–T3 states** and the data on the data bus is read. The address bus outputs the **target address** during the bus cycle period.

*Fig. 3.1.1.5 — Bus cycle at the time of memory read (figure not captured).* Signals: CLK, PK, PL, A00–A23 (= address), WR, RD, D0–D7 (= read data), DBS1, DBS0, across T1–T4.

### 3.1.2 Wait state

The S1C88 can extend the bus cycle by inserting a **wait state** to precisely access a low-speed device on the bus line. The WAIT-insertion function for access-time extension is controlled by the input signal of the **WAIT** terminal.

**Wait insertion rules:**

- The **WAIT** signal is sampled at the **CLK rising edge of the T3 state**.
- If WAIT is **low** at that time, wait states **Tw1** and **Tw2** are inserted between the T3 and T4 states, extending the access time.
- If WAIT is **high**, no wait state is inserted.
- Wait states Tw1/Tw2 are inserted **continuously while WAIT remains low**. Sampling to release wait insertion is done at the **CLK rising edge of the Tw2 state**; when WAIT returns high, no further wait states are inserted and the T4 state begins.
- The wait state is inserted **only** when accessing devices on the memory space; it is **not** inserted when accessing an internal register.

Wait-insertion timing charts (interrupt vector address read, memory write, memory read):

- *Fig. 3.1.2.1 — Wait insert of the interrupt vector address read cycle (figure not captured).* Sequence of states: T1, T2, T3, **Tw1, Tw2**, T4. WAIT shown L … L … H (returns high at Tw2 sampling). Signals include RDIV (vector read), A00–A23 = dummy address, D0–D7 = interrupt vector address.
- *Fig. 3.1.2.2 — Wait insert of the memory write cycle (figure not captured).* States T1, T2, T3, **Tw1, Tw2**, T4. A00–A23 = address, D0–D7 = write data, WAIT L…L…H.
- *Fig. 3.1.2.3 — Wait insert of the memory read cycle (figure not captured).* States T1, T2, T3, **Tw1, Tw2**, T4. A00–A23 = address, D0–D7 = read data, WAIT L…L…H.

---

## 3.2 Outline of Processing Statuses

S1C88 operation is classified by processing content into **five** processing statuses.

**Table 3.2.1 — Classification of the processing statuses**

| Processing status | Outline |
|-------------------|---------|
| **Reset status** | Status where the CPU is reset and stopped. |
| **Program execution status** | Status where the CPU successively executes programs. |
| **Exception processing status** | Transitive status where exception processing (fetching of a vector address; PC and SC evacuation; setting of a branch address for the PC) is activated by an exception processing factor such as a reset or interrupt. |
| **Bus authority release status** | Status where an external bus is released by a bus authority request signal from outside. |
| **Standby status — HALT** | Status where the CPU is stopped to reduce power consumption. |
| **Standby status — SLEEP** | Status where the CPU **and peripheral circuit** are stopped to reduce power consumption. |

### Status transition diagram (Fig. 3.2.1) *(figure not captured)*

Transitions (reconstructed from the figure's labels):

- **Reset status** ↔ entered whenever **SR = 0** (low); exits when **SR = 1**.
- **Reset status → Exception processing status** on `SR = 1` (the reset exception sequence).
- **Program execution status → Exception processing status** on *exception processing factor generation*; returns to **Program execution status** on *exception processing completion*.
- **Program execution status → Standby (HALT)** via the **HALT instruction**; **HALT → Exception processing status** on *interrupt factor generation*.
- **Program execution status → Standby (SLEEP)** via the **SLP instruction**; **SLEEP → Exception processing status** on *interrupt factor generation*.
- **Program execution status → Bus authority release status** on **BREQ = 1** (request asserted); returns on **BREQ = 0** (request released).
- **HALT → Bus authority release status** on **BREQ = 1**; returns to HALT on **BREQ = 0**.
- A reset (**SR = 1** at its terminal going low→entering reset, etc.) can be entered from any status.

---

## 3.3 Reset Status

The reset status is the status where the S1C88 is reset and stops. The S1C88 is reset by inputting a **low** level into the **SR** terminal. Because resetting is done **out of synchronization with CLK**, the CPU shifts from **any** processing status to reset status immediately. Part of the internal registers are initialized by reset.

**During reset (SR low):** the address bus, data bus, and read/write signals become **high impedance**. However, because the address bus and read/write signals are **pulled up within the CPU**, a high level is output.

**Reset release sequence:**

1. Reset is released when the **SR** terminal becomes **high**.
2. The first bus cycle starts at the point where the **falling edge of CLK has been input twice**.
3. In this first bus cycle, a **dummy address** is output to the address bus, and the interrupt acknowledge **IACK** becomes enabled by the following bus cycle.
4. This starts the **reset exception processing**: it loads the start address stored in the vector table into the **PC** (which is in undefined status).
5. Simultaneously, it loads the initial value **01H** of the **NB** (new code bank register) into the **CB** (code bank register). As a result, **bank 1 (008000H–00FFFFH)** is selected for the bank area after reset.

After an initial reset, the program is executed from the start address stored in **000000H–000001H** of memory.

### Table 3.3.1 — Initial set value of the internal registers

| Register name | Symbol | Bit length | Initial value |
|---------------|--------|------------|---------------|
| Data register A | A | 8 | Undefined |
| Data register B | B | 8 | Undefined |
| Index (data) register L | L | 8 | Undefined |
| Index (data) register H | H | 8 | Undefined |
| Index register IX | IX | 16 | Undefined |
| Index register IY | IY | 16 | Undefined |
| Program counter | PC | 16 | Undefined \* |
| Stack pointer | SP | 16 | Undefined |
| Base register | BR | 8 | Undefined |
| Zero flag | Z | 1 | 0 |
| Carry flag | C | 1 | 0 |
| Overflow flag | V | 1 | 0 |
| Negative flag | N | 1 | 0 |
| Decimal flag | D | 1 | 0 |
| Unpack flag | U | 1 | 0 |
| Interrupt flag 0 | I0 | 1 | 1 |
| Interrupt flag 1 | I1 | 1 | 1 |
| New code bank register | NB | 8 | 01H |
| Code bank register | CB | 8 | Undefined \* |
| Expand page register | EP | 8 | 00H |
| Expand page register for IX | XP | 8 | 00H |
| Expand page register for IY | YP | 8 | 00H |

**\* Notes on the table:**

- **PC / CB:** The value stored in the top of bank 0 (**000000H–000001H**) is loaded into the **PC** by the reset exception processing. At the same time, the initial value **01H** of the **NB** is loaded into the **CB**.
- Registers **NB, CB, EP, XP, YP** exist (are set) for **MODEL2/3** and do **not** exist in **MODEL0/1**.
- Use the program to initialize, if necessary, any registers not initialized by reset.

### Fig. 3.3.1 — Reset status and sequence following reset release *(figure not captured)*

Reconstructed signal trace (left = reset status, right = sequence following reset release). Signals: CLK, SR, PK, PL, A00–A23, D0–D7, WR, RD, SYNC, IACK.

- During reset status: A00–A23 / D0–D7 carry "ANY / DUMMY" placeholders; SR low.
- After release, address bus emits: **DUMMY** (bus cycle 1, 2) → **000000**, **000001** (the reset vector, "VEC.") → then "ANY".
- Data bus reads **VECL**, **VECH** (the vector low/high bytes) then proceeds to **INST** (first instruction).
- SYNC marks the op-code fetch; IACK enables on the bus cycle following the first dummy cycle.

---

## 3.4 Program Execution Status

The program execution status is the status where the S1C88 successively executes programs.

**Instruction fetch overlap (pipelining):** The fetch of the **first op-code** of an instruction is done **overlapping the last cycle of the immediately prior instruction**. Consequently:

- The execution cycle for one instruction **begins** either from the fetch cycle for the **second op-code**, the read cycle for the **first operand**, or the **first execution cycle** (varies by instruction), and **terminates** with the fetch cycle for the **first op-code of the following instruction**.
- A **1-cycle instruction** consists only of the fetch cycle of the first op-code of the following instruction.
- There are also cases where it shifts to the fetch cycle of the first op-code **without** interposing an execute cycle after an operand read cycle.

**SYNC signal:** During the fetch cycle of the first op-code, **SYNC is high**. (This high SYNC edge is the sampling point for exception processing — see §3.5.4.)

### Fig. 3.4.1 — Example of instruction execution cycle *(figure not captured)*

Worked example program list:

```
001000   44 6E        LD   A,[BR:6EH]
001002   CE 10 34     SUB  A,[IX+34H]
001005   50           LD   L,A
001006   69           LD   [HL],B
```

Register and memory conditions for the example:

```
B = 7FH      H  = 81H     BR = 83H     IX = 8000H
EP = 00H     XP = 00H
M(008034H) = 27H         M(00836EH) = 9BH
```

Reconstructed bus activity (address bus / data bus / SYNC), in order:
- `001000` fetch `44` (1st op-code of `LD A,[BR:6EH]`, SYNC high) → `001001` reads `6E` operand → `00836E` reads `9B` (the `[BR:6EH]` memory operand; BR:6EH = 83:6E = 00836E).
- `001002` fetch `CE` (`SUB A,[IX+34H]`, SYNC high) → `001003` reads `10`, `001004` reads `34` operands → `008034` reads `27` (IX+34H = 8000+34 = 8034).
- `001005` fetch `50` (`LD L,A`, SYNC high).
- `001006` fetch `69` (`LD [HL],B`, SYNC high) → write `7F` to `008174` (HL = H:L = 81:74 after `L,A`; A had been computed). 
- `001007` fetch of the next 1st op-code (`ANY`).

(The address `008174` and the interleaving of fetch/operand/execute cycles illustrate the op-code-fetch overlap described above.)

---

## 3.5 Exception Processing Status

Exception processing status is a **transition** status where the S1C88 suspends normal program execution and changes the processing flow due to an exception processing factor such as an interrupt.

**Sequence overview:**

- Exception processing **begins with the termination of the instruction cycle** being executed when an exception factor occurred.
- It **evacuates the return information** (for reopening the suspended routine) onto the **stack**, then loads the **start address** of the exception processing routine (user routine) from the vector address corresponding to the factor into the **PC**, and **branches** to that routine.
- **Exception:** for **reset** exception processing, the return information is **not** evacuated.
- The transitive status up to the branch is the **exception processing status**; after the branch it returns to **normal program execution status**.

**Return convention:** User exception routines take the **subroutine format**; however, because the **SC** (system condition flag) is pushed onto the stack, the return instruction **must be `RETE`**. The `RETE` instruction resumes execution of the routine suspended by the exception processing.

> Note: `RETS` is the ordinary subroutine return; exception/interrupt routines invariably use **`RETE`** because the SC was pushed (and is restored by `RETE`, which also restores the interrupt mask — see §3.5.3).

### Fig. 3.5.1 — Exception processing flow *(figure not captured)*

Flow (top to bottom):

1. **Exception processing factor generation**
2. **Evacuation of CB** (code bank register) to stack — **MODEL2/3 (maximum mode) only**
3. **Evacuation of PC** (program counter) to stack
4. **Evacuation of SC** (system condition flag) to stack
5. **Reading of exception processing vector and loading into PC**
6. *(branch)* — followed by **Execution of exception processing routine** … then the **`RETE` instruction** → **Return to point of generation of the exception processing factor**

For **reset processing factor generation**, the flow skips the CB/PC/SC evacuation steps and goes directly to vector read / PC load.

### 3.5.1 Exception processing types and priority

A priority order is set for exception processing factors. When multiple factors are generated simultaneously, the highest-priority exception is executed first.

When a **new** exception factor is generated during an exception processing status, the new exception processing is executed **following the termination of the exception processing at that time (prior to execution of the exception processing routine)**.

> Example: If an **NMI** is generated during **IRQ3** exception processing execution, the NMI is sampled at the **final stage** of the IRQ3 exception processing, and the **NMI** user routine runs **ahead of** the IRQ3 user routine. The IRQ3 user routine runs after the NMI routine terminates.

For this reason, an interrupt is set up so that an interrupt of **lower priority** than the in-progress interrupt is **masked**.

Because exception processing by an **INT instruction** is started by the program, **no priority** is assigned.

**Table 3.5.1.1 — Types of exception processing and priorities**

| Type | Exception processing start timing | Priority |
|------|-----------------------------------|----------|
| **Reset** | Initial fetch cycle following change of SR terminal from low to high. | High |
| **Zero division** | Immediately following a DIV instruction when a DIV (division) was executed with divisor zero. | ↑ |
| **NMI** *(non-maskable interrupt)* | When an instruction or exception processing is terminated during execution, at the point where a **falling edge** has been input into the **NMI** terminal. | ↓ |
| **IRQ3** *(interrupt request 3)* | When an instruction or exception processing is terminated during execution, at the point where a **low level** has been input into the **IRQ3** terminal. | ↓ |
| **IRQ2** *(interrupt request 2)* | When an instruction or exception processing is terminated during execution, at the point where a **low level** has been input into the **IRQ2** terminal. | ↓ |
| **IRQ1** *(interrupt request 1)* | When an instruction or exception processing is terminated during execution, at the point where a **low level** has been input into the **IRQ1** terminal. | Low |
| **INT instruction** *(software interrupt)* | Execution of the INT instruction. | None |

### 3.5.2 Exception processing factor and vectors

The start address of an exception processing routine is set as the **vector** at the vector address corresponding to each factor. This vector is loaded into the **PC** following exception processing and branched to.

- Vectors are fixed as **2-byte address information** indicating a **logic address**, **regardless of the CPU model**.
- The **bank** for the exception processing routine **cannot be specified**, even in MODEL2/3 maximum mode. Therefore the start address of the routine **must be within the common area (000000H–007FFFH)** in order to branch from multiple banks to a common exception routine.
- **IRQ1–IRQ3** vector addresses are set by a **peripheral circuit**.
- For an **INT instruction**, the instruction **operand becomes the vector address** as-is.
- Including the other exception factors, up to a maximum of **128 vectors** are reserved.

**Table 3.5.2.1 — Correspondence of vector addresses with exception processing factors**

| Exception processing factor | Vector address | Vector address generation source |
|-----------------------------|----------------|----------------------------------|
| Reset | 000000H–000001H | Within CPU |
| Zero division | 000002H–000003H | Within CPU |
| NMI | 000004H–000005H | Within CPU |
| IRQ1–IRQ3 | 000006H–0000FFH | Peripheral circuit |
| INT instruction | 000000H–0000FFH | Instruction operand |

### 3.5.3 Interrupts

There are four interrupt types — **NMI, IRQ3, IRQ2, IRQ1** — each set by an interrupt priority level.

**Table 3.5.3.1 — Interrupt levels**

| Priority | Interrupt priority level | Interrupt factor |
|----------|--------------------------|------------------|
| High | 4 | NMI |
| ↑ | 3 | IRQ3 |
| ↓ | 2 | IRQ2 |
| Low | 1 | IRQ1 |

**Interrupt masking with I0 / I1:** An interrupt can be masked by the interrupt flags **I0** and **I1**. When the interrupt priority level is set in the 2 bits I0/I1 by the program, only interrupts **above** that level are accepted. The **NMI (level 4)** is **always accepted regardless of I0/I1**.

**Table 3.5.3.2 — Interrupt mask settings**

| I1 | I0 | Acceptable interrupts |
|----|----|------------------------|
| 1 | 1 | NMI |
| 1 | 0 | NMI, IRQ3 |
| 0 | 1 | NMI, IRQ3, IRQ2 |
| 0 | 0 | NMI, IRQ3, IRQ2, IRQ1 |

**Mask update on acceptance:** When exception processing is executed by an interrupt factor, **I0 and I1 are set to the same level as the accepted interrupt**, masking interrupts of the **same level or lower**. Because this mask is set **after** the SC (system condition flag) has been pushed to the stack, the SC returns to its original state when the routine terminates with a **`RETE`** instruction, so the interrupt mask **also returns to the original priority level**. To enable multiple interrupts at the same level or lower from within the routine, **re-set the priority level inside that routine**.

**Table 3.5.3.3 — I0 and I1 following interrupt acceptance**

| Accepted interrupt factor | I1 | I0 |
|---------------------------|----|----|
| NMI | 1 | 1 |
| IRQ3 | 1 | 1 |
| IRQ2 | 1 | 0 |
| IRQ1 | 0 | 1 |

> *(Extraction note: the source title reads "Table 3.5.3.2 Interrupt mask settings" but its content is the I0/I1 acceptance table; numbering of 3.5.3.2 vs 3.5.3.3 appears swapped in the OCR. The two tables above are reproduced under their content-correct headings.)*

**Atomicity around NB / SC writes:** Interrupts are **disabled while an instruction that modifies NB or SC is being executed**. The exception processing of an interrupt generated during that period is started **after the following instruction has been executed**.

### 3.5.4 Exception processing sequence

**Sampling:** Exception processing sampling is done at the **rising edge of SYNC** (i.e., at the start of the **first op-code fetch cycle** of the instruction). When a factor is present, the CPU asserts the interrupt acknowledge signal **IACK** and begins exception processing.

**IRQ1–IRQ3 vector delivery:** The peripheral circuit that generated the interrupt receives the **IACK** signal and then holds (drives) the vector address.

Common push order for non-reset exceptions (from the sequence figures): the stack receives, at decreasing SP, **CB** (MODEL2/3 max mode only) → **PC(H)** → **PC(L)** → **SC**. The vector (VEC(L), VEC(H)) is then read into PC. The opcode/byte tokens seen on the data bus during these pushes (`B9`, `CF`, `FC`, etc.) are the internal microcode/PUSH bytes captured by the trace.

#### Fig. 3.5.4.1 — Exception processing sequence: **reset** *(figure not captured)*

- Signals: CLK, PK, PL, A00–A23, D0–D7, DBS1, DBS0, RDIV, WR, RD, SYNC, IACK, I1/I0.
- **No return information is pushed** (reset is the exception that skips evacuation).
- A **dummy cycle** occurs; then the vector is read: address `000000` → `000001` (= "VEC."), data `VEC(L)`, `VEC(H)` loaded into PC.
- The first executed instruction shown is `LD SP,#mmnn` (data `nn`, `mm`) — i.e., the reset handler initializing SP.
- Exception-factor sampling points are marked at the SYNC rising edges. I1/I0 end at value `3` (binary 11 — both interrupt flags set, per Table 3.3.1).

#### Fig. 3.5.4.2 — Exception processing sequence: **zero division** *(figure not captured)*

- Triggered immediately following the `DIV` instruction with divisor 0.
- Pushes (`PUSH`): at SP-1 → **PC(H)**; SP-2 → **PC(L)**; SP-3 → **SC**; (SP-4 used) — and in **MODEL2/3 maximum mode, CB is pushed into the stack** as well.
- Vector read at `000002` → `000003` (= "VEC.", VEC.+1): data `VEC(L)`, `VEC(H)` → PC.
- I1/I0: **No change** (zero-division is not an interrupt; the mask is not altered).
- Data-bus microcode byte seen: `B9`.

#### Fig. 3.5.4.3 — Exception processing sequence: **NMI** *(figure not captured)*

- Triggered by a falling edge on the **NMI** terminal; suspends after the in-progress instruction (example next instruction `ADC A,[HL]`).
- Pushes: SP-1 → **PC(H)**; SP-2 → **PC(L)**; SP-3 → **SC**; (SP-4); **MODEL2/3 max mode: CB pushed** too.
- Vector read at `000004` → `000005` (VEC., VEC.+1) → PC.
- I1/I0 set to `3` (= 11) on acceptance (NMI), per Table 3.5.3.3.
- Microcode byte: `B9`.

#### Fig. 3.5.4.4 — Exception processing sequence: **IRQ1–IRQ3** *(figure not captured)*

- Triggered by a low level on the IRQx terminal; suspends after the in-progress instruction (example `ADC A,[HL]`).
- Pushes: SP-1 → **PC(H)**; SP-2 → **PC(L)**; SP-3 → **SC**; (SP-4); **MODEL2/3 max mode: CB pushed**.
- **Vector address is supplied by the peripheral circuit:** address shown as `0000xx` → `0000xx+1` where `xx` is the peripheral-furnished vector index; data `VEC(L)`, `VEC(H)` (the "VECA" token = vector-address acceptance) → PC.
- I1/I0 updated per accepted level (the figure annotates the I1/I0 values for IRQ3 / IRQ2 / IRQ1 cases): IRQ3 → 11; IRQ2 → 10; IRQ1 → 01 (matching Table 3.5.3.3, where the (xx) values map 0/1/2 to the respective IRQ).
- Microcode byte: `B9`.

#### Fig. 3.5.4.5 — Exception processing sequence: **INT instruction** *(figure not captured)*

- Started by program execution of the `INT` instruction (example next instruction `ADC A,[HL]`).
- The address pushed for the **return PC is `PC+1`** (the instruction following the INT operand), reflecting that INT is software-initiated. Pushes: SP-1 → **PC(H)**; SP-2 → **PC(L)**; SP-3 → **SC**; (SP-4); **MODEL2/3 max mode: CB pushed**.
- The **operand `kk` of the INT instruction is the vector index:** vector read at `0000kk` → `0000kk+1`, data `VEC(L)`, `VEC(H)` → PC.
- I1/I0: **No change** (software interrupt does not alter the mask).
- Microcode/operand bytes seen: `kk`, `FC`, `CF`.

> **Emulator/compiler summary of the push & restore semantics:**
> - **Reset:** nothing pushed; CB ← NB(01H); PC ← [000000H]. Bank 1 selected.
> - **Zero division / NMI / IRQx / INT:** push order (high→low addr) is **[CB (MODEL2/3 max only)], PC(H), PC(L), SC**, i.e. SC ends up on top of stack. PC ← vector; CB unchanged on entry except via the push (CB itself is not reloaded by a non-reset exception — the bank for the handler is the common-area logic address).
> - **Return PC pushed:** = address of the instruction to resume — for INT this is `PC+1` (past the INT); for hardware interrupts/zero-division it is the next instruction boundary.
> - **Mask:** I0/I1 ← accepted level for NMI/IRQ (Table 3.5.3.3); **unchanged** for zero-division and INT.
> - **`RETE`** pops **SC, PC(L), PC(H), [CB]** (reverse order), restoring the interrupt mask along with SC. Use `RETE` (not `RETS`) for every exception/interrupt handler because SC was pushed.

---

## 3.6 Bus Authority Release Status

The S1C88 can release the bus for an external bus-authority request (e.g., **DMA** transmission). This is the **bus authority release status**.

In this status, the **address bus (A00–A23)**, **data bus (D0–D7)**, and **read/write signals (RD/WR)** become **high impedance**, and the **bus master** (the external device that issued the request) can directly access another device (e.g., memory) on the bus.

### Bus release from program execution status (Fig. 3.6.1)

1. The would-be bus master drives the **BREQ** terminal **low**, requesting a bus authority release.
2. The CPU samples BREQ **twice per bus cycle**: at the **falling edge of CLK of the T2 state** (or **Tw1** state when WAIT is inserted) and at the **falling edge of CLK of the T4 state**.
3. If BREQ was low following the T2 sampling and is still low at the T4 sampling, the CPU **suspends the instruction** in execution in that bus cycle, drives **BACK low**, and shifts to the bus authority release status.
4. The external bus master receives **BACK** and starts bus control. It must **keep BREQ low** until it is finished using the bus.
5. After shifting to the release status, the CPU inserts **Tz1** and **Tz2** states and samples **BREQ at the falling edge of CLK of the Tz2 state**. Tz1/Tz2 are inserted **continuously** until a **high** level is detected.
6. When high is detected, the CPU returns **BACK high** at the **rising edge of CLK of the Tz2 state**; immediately after that Tz2 ends, it returns to the normal bus cycle, resuming the suspended processing.

**Granularity:** The bus authority release status can be inserted at the **break-point of the bus cycle**, in contrast to the **exception processing status**, which is inserted at the **break-point of each instruction execution cycle**.

**Restriction during IACK:** During exception processing that outputs the **IACK** signal, a bus release request is **not accepted as long as IACK is low**.

*Fig. 3.6.1 — Bus authority release sequence from program execution status (figure not captured).* Example instruction `LD [HL],[IX]`. State sequence: … T1 T2 T3 (Tw1 Tw2) T4 → **Tz1 Tz2 Tz1 Tz2 …** → T1 T2 T3 … . Signals CLK/PK/PL, A00–A23, D0–D7, WR, RD, DBS1/0, WAIT, BREQ (L), BACK (driven L during release, returns H). DBS shows codes 3/2/3 then 0 during high-Z.

### Bus release from HALT status (Fig. 3.6.2)

The bus authority release status can also be entered from the **HALT** standby status (see §3.7.1). The **difference** from program execution status: in HALT, the bus-release-request signal is **sampled at the falling edge of CLK of the Th2 state**.

*Fig. 3.6.2 — Bus authority release sequence from the HALT status (figure not captured).* State sequence: program execution (T1–T4) → **HALT (Th1 Th2 …)** → on BREQ low at Th2 sampling, **Tz1 Tz2 …** (bus released, BACK low) → back to **HALT (Th1 Th2 …)** when BREQ high. PC held throughout. DBS = 0 / high-Z during release; BREQ L, BACK H→L→H.

---

## 3.7 Standby Status

The S1C88 can stop CPU operation to greatly reduce power consumption — useful when there is no CPU processing to execute while an application program is present. Two types: **HALT** and **SLEEP**.

### 3.7.1 HALT status

- Entered with the **`HALT` instruction**. **Only the CPU stops** (peripheral circuits, including the oscillation circuit, keep operating).
- Exits to exception processing via the optional interrupts **NMI, IRQ1–IRQ3**. When restarted by an interrupt, a **`RETE`** instruction at the end of the exception routine resumes program execution **from the instruction following the `HALT` instruction**.
- Because peripheral circuits (e.g., the oscillation circuit) keep running, **no external restart circuit is needed** and restarting is **instantaneous**.
- The contents of CPU registers at the moment `HALT` was executed are **held** during HALT.
- In HALT, the **Th1 and Th2** states are inserted **continuously**. **Interrupt sampling is done at the falling edge of CLK of the Th2 state**; generation of an interrupt factor causes an **immediate shift to exception processing**.

*Fig. 3.7.1.1 — Sequence of shifting to the HALT status and restarting (figure not captured).* State trace: program execution (… T3 T4 / T1–T4) → **HALT: Th1 Th2 Th1 Th2 …** → on IRQ3 low (sampled at Th2 falling edge) → exception processing (T1 T2 T3 T4 …). Signals: CLK/PK/PL, A00–A23 (PC held), D0–D7, WR, RD, DBS1/0 (= 0 then 3 during exception), IRQ3, IACK. Data byte `AE` shown (the `HALT` instruction opcode region).

### 3.7.2 SLEEP status

- Entered with the **`SLP` instruction**. **Both the CPU and the peripheral circuits within the MCU stop** (including the oscillation circuit).
- Exits to exception processing via a **reset** or an interrupt (**NMI, IRQ1–IRQ3**) from outside the MCU. When restarted by an interrupt, a **`RETE`** at the end of the exception routine resumes program execution **from the instruction following the `SLP` instruction**.
- **Power consumption is much lower than HALT** because the oscillation circuit is also stopped. However, a **safety (oscillation-stabilization) period** is needed on restart, so SLEEP is best for **extended standby** where instantaneous restart is unnecessary.
- CPU register contents at the time `SLP` was executed are **held** while the rated voltage is applied.

**Restart timing (oscillation stabilization):**

1. An external interrupt in SLEEP starts the peripheral circuit, and the oscillation circuit begins to oscillate.
2. When oscillation starts, the **CLK input to the CPU is masked by the peripheral circuit**; CPU CLK input begins only **after a stable waiting time** (several 10 msec to several sec) elapses.
3. The CPU **samples the interrupt at the falling edge of the first input CLK** and starts exception processing.

*Fig. 3.7.2.1 — Sequence of the shift to the SLEEP status and restarting (figure not captured).* Trace: program execution (… T3 T4 / T1–T4) → **SLEEP** (OSC stopped; CLK halted; `SLP` opcode region byte `AF`) → external IRQ3 low → **oscillation stable waiting time (Tstp)** during which OSC restarts and CLK to CPU is masked → exception processing (T1 T2 T3 T4 …). Signals: OSC, CLK, PK/PL, A00–A23 (PC held), D0–D7, WR, RD, DBS1/0 (0 → 3), IRQ3, IACK.

#### HALT vs. SLEEP — quick comparison

| Aspect | HALT | SLEEP |
|--------|------|-------|
| Entry instruction | `HALT` | `SLP` |
| What stops | CPU only | CPU **and** peripheral circuits (incl. oscillator) |
| Oscillation circuit | Keeps running | Stopped |
| Wake sources | NMI, IRQ1–IRQ3 | Reset, NMI, IRQ1–IRQ3 (external to MCU) |
| Interrupt sampling | Falling edge of CLK in **Th2** state | Falling edge of **first input CLK** after oscillation stabilizes |
| Restart latency | Instantaneous | After oscillation stabilization wait (several 10 ms – several s) |
| Restart point (interrupt) | Instruction after `HALT` (via `RETE`) | Instruction after `SLP` (via `RETE`) |
| Power consumption | Reduced | Greatly reduced (lower than HALT) |
| Register contents | Held | Held (under rated voltage) |
| Can shift to bus-authority-release? | Yes (sampled at Th2) | — (not described) |
