# S1C88 reference set

Distilled, agent- and engineer-facing reference for the **Epson S1C88** core CPU and its **official
toolchain**. Built so this project (and future agents) have authoritative grounding without
re-deriving the ISA each session.

These files are distilled from the **Epson official PDFs**, transcribed from machine-extracted text.
Figures/diagrams in the source were lost in extraction and have been reconstructed as tables or
descriptions, marked `*(figure not captured)*`. Where the extraction was garbled, the affected spot is
flagged inline (often `[OCR?]`) rather than guessed. The sibling [`../../../skiploom`] assembler's
opcode table (`src/util/s1c88.csv`) was used as an independent cross-check, **not** as a source.

## Source documents

| Doc | File | Pages | What it is |
|-----|------|-------|------------|
| **Core CPU Manual** (MF658-05) | `docs/s1c88-core-cpu-manual.pdf` | 226 | The S1C88 core architecture and ISA |
| **Tool Package Manual I** (S5U1C88000C) | `../skiploom/docs/id000920.pdf` | 331 | Official C compiler, assembler, linker, locator, utilities |

(Raw per-page text extraction lives in the git-ignored `docs/_extract/`; regenerate with PyMuPDF — see
the extraction note at the bottom.)

## Core CPU (from the Core CPU Manual)

| File | Covers | Source §§ / PDF pp |
|------|--------|--------------------|
| [architecture.md](architecture.md) | Outline, ALU, full register file (A/B/BA, L/H/HL, IX/IY, SP, PC, CB/NB, BR, EP/XP/YP, SC), flags, complement/overflow, decimal/unpack, MLT/DIV | Ch 1–2.2 · pp 7–17 |
| [memory-model.md](memory-model.md) | 24-bit/16 MB space, program banking (CB:PC, 32 K banks), data paging (EP/XP/YP, 64 K pages), stack, memory-mapped I/O | §2.3–2.4 · pp 18–26 |
| [cpu-operation-interrupts.md](cpu-operation-interrupts.md) | Bus cycles/wait, processing statuses, reset + register init values, exception/interrupt vectors & sequence, HALT/SLEEP | Ch 3 · pp 27–40 |
| [addressing-modes.md](addressing-modes.md) | All 12 addressing modes, instruction format, the `CEH`/`CFH` prefix (escape) page scheme, operand encoding/endianness | §4.1–4.2 · pp 41–47 |
| [instruction-set.md](instruction-set.md) | Symbol legend, function classification, full instruction list (mnemonics, operands, bytes, cycles, flags), Appendix A opcode map (unprefixed + CE + CF pages), Appendix B by addressing mode | §4.3 + App A/B/C · pp 46–64, 201–223 |

> Per-instruction detailed operation pseudocode (Core CPU Manual §4.4, pp 65–200) is **not** transcribed
> here — that section of the PDF remains the authoritative source for single-instruction semantics.

## Toolchain (from Tool Package Manual I)

| File | Covers | Source / PDF pp |
|------|--------|-----------------|
| [c-compiler.md](c-compiler.md) | TASKING `c88` C cross-compiler: memory models, data types, **parameter passing & return registers**, register usage, sections, stack/heap, interrupts, intrinsics, inline asm, options, libraries | Ch 1 · pp 13–96 |
| [assembler.md](assembler.md) | `as88` assembler: lexical syntax, sections (DEFSECT/SECT + attributes), expressions & `@` functions, macros, all 33 directives, 8 controls, options | Ch 2 · pp 97–161 |
| [linker.md](linker.md) | `lk88` linker: invocation/options, library search, section merging, overlays, type checking, IEEE-695 output | Ch 3 · pp 162–175 |
| [locator-delfee.md](locator-delfee.md) | `lc88` locator: options, output (`.abs` IEEE-695 / `.sre` Motorola-S), locator labels; the **DELFEE** locator description language (memory/section placement) | Ch 4–5 · pp 176–220 |
| [utilities.md](utilities.md) | `ar88` librarian, `cc88` control program, `mk88` make, `pr88` object reader; **file formats**: IEEE-695/MUFOM (full binary opcode tables), Motorola S-records | Ch 6 + App · pp 221–331 |
| [toolchain.md](toolchain.md) | How the tools fit together: the build pipeline, file extensions, and intermediate/output formats | synthesis |

## sdcc88 backend decisions

- [abi-decision.md](abi-decision.md) — the base-port choice (clone of SDCC's **z80** port) and the
  target S1C88 C ABI / z80→s1c88 register mapping the backend retarget follows.

## ABI quick-reference (for the SDCC retarget)

From [c-compiler.md](c-compiler.md) — the official S1C88 C ABI, useful as a model for the sdcc88 backend:

- **Endianness:** little-endian. **Sizes:** `char` 1, `short`/`int` 2, `long` 4, `float`/`double` 4
  (single precision). `_near` pointer = 2 bytes (16-bit), `_far` pointer = 3 bytes (24-bit).
- **Argument registers:** `char`→`A`; `int`/`short`→`BA, HL, IX, IY`; `long`→pairs `HLBA, IYIX`;
  `_near` ptr→`IY, IX, HL, BA`; `_far` ptr→`IYP, IXP, HLP`. Overflow, structs/unions, and varargs go on
  the stack (pushed in reverse).
- **Return registers:** `char`→`A`; `int`/`short`→`BA`; `long`→`HLBA`; pointer→`HLP`; `float`→`HLBA`;
  struct/union→stack.
- **Convention:** caller-saves; stack grows down (≤ 64 K); interrupt functions save all clobbered
  registers and return via `RETE`.

> sdcc88 does **not** have to match the Epson ABI — but it's the reference design for an S1C88 C ABI.

## Regenerating the text extraction

The PDFs need [PyMuPDF](https://pymupdf.readthedocs.io/) (self-contained wheel, no system deps):

```bash
python3 -m pip install --user --break-system-packages pymupdf
python3 - <<'PY'
import fitz, os
for src, out in [("docs/s1c88-core-cpu-manual.pdf","docs/_extract/core-cpu"),
                 ("../skiploom/docs/id000920.pdf","docs/_extract/id000920")]:
    os.makedirs(out, exist_ok=True); d = fitz.open(src)
    if d.needs_pass: d.authenticate("")          # id000920 is empty-password RC4 encrypted
    for i in range(d.page_count):
        open(f"{out}/page-{i+1:03d}.txt","w").write(d[i].get_text())
PY
```
