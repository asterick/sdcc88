# S1C88 Assembler (`as88`) Reference

> **Source:** Epson *S5U1C88000C Manual I — "C Compiler/Assembler/Linker"*, **Chapter 2 Assembler**.
> **Extracted PDF page range:** 97–161 (printed pages 85–149).
> Generated from machine-extracted plain text. Figures/tables in the PDF were lost in extraction and
> have been reconstructed; spots that could not be reconstructed are marked `*(figure not captured)*`.
> Garbled or ambiguous extraction is flagged inline with **[sic / extraction note]**.

This document targets two audiences:
1. A compiler backend that must **emit** assembly text that `as88` accepts.
2. An independent reimplementation of `as88` (assembler/linker for the Epson S1C88, the Pokémon Mini core).

The assembler emits **relocatable IEEE-695** object files (`.obj`) that are linked by `lk88`. Source files
may be C-compiler-generated (`c88`) or hand-written. Default source extensions tried: `.asm` then `.src`.

---

## 2.1 Description / Invocation

`as88` assembles assembly source into relocatable IEEE-695 objects. Phases: (1) preprocess, (2) legality
check of all instructions, (3) address calculation, (4) generation of object and (optionally) list file.
File inclusion and macro facilities are integrated into the assembler.

The control program `cc88` may invoke `as88` automatically (translating some of its options), but `as88`
can be run standalone.

### Invocation syntax

```
as88 [option]... source-file [map-file]
as88 -V
```

- `source-file` — the assembly source. If it has no extension, `.asm` is tried, then `.src`.
- `map-file` — optional; supplied (with `.map` extension) to generate an **absolute** list file using the
  locator's map. See 2.1.4.1.
- `-V` alone displays only the version header; all other options are ignored.
- Options are preceded by `-` (minus). **Options cannot be combined** after a single `-`.
- Default output is an object file with `.obj` extension (`-o` overrides). `-l` produces a `.lst` list file.
- Errors go to the terminal unless redirected with `-err`.

### 2.1.1 Options Summary

| Option | Description |
|--------|-------------|
| `-C file` | Include file before source |
| `-Dmacro[=def]` | Define preprocessor macro |
| `-L[flag...]` | Remove specified source lines from list file |
| `-M[s\|c\|d\|l]` | Specify memory model |
| `-V` | Display version header only |
| `-c` | Switch to case-insensitive mode (default: case sensitive) |
| `-e` | Remove object file on assembly errors |
| `-err` | Redirect error messages to error file (`.ers`) |
| `-f file` | Read options from file |
| `-i[l\|g]` | Default label style local or global |
| `-l` | Generate listing file |
| `-o filename` | Specify name of output file |
| `-t` | Display section summary (totals) |
| `-v` | Verbose mode. Print filenames and pass numbers while they progress |
| `-w[num]` | Suppress one or all warning messages |

### 2.1.2 Detailed Option Descriptions

**`-C file`** — Include `file` before assembling the source.
```
as88 -C S1C88.inc test.src
```

**`-c`** — Switch to case-insensitive mode. Default is case sensitive.
```
as88 -c test.src
```

**`-Dmacro[=def]`** — Define `macro` as with `DEFINE`. If `=def` is absent, `1` is assumed. Any number of
symbols can be defined.
```
as88 -DTWO=2 test.src
```

**`-e`** — Do not produce an object file when the assembler generates errors (so `make` always does the
proper productions).
```
as88 -e test.src
```

**`-err`** — Redirect error messages to a file with the **output** file's basename and extension `.ers`
(instead of `stderr`).
```
as88 -err test.src      ; -> test.ers
```

**`-f file`** — Read command-line arguments from `file` (use `-` for stdin). Works around command-line
length limits; more than one `-f` allowed. Command-file rules:
1. Multiple arguments allowed per line.
2. To include whitespace in an argument, surround it with single or double quotes.
3. Quote nesting: if embedded quotes are only one kind, wrap with the opposite kind; if both kinds
   appear, split the argument so each embedded quote is surrounded by the opposite type.
   ```
   "This has a single quote ' embedded"
   'This has a double quote " embedded'
   'This has a double quote " and a single quote '"' embedded"
   ```
4. Continuation lines end with backslash + newline. In a quoted argument the next line is appended
   *without* stripping whitespace; for non-quoted arguments leading whitespace on the next line is
   stripped.
   ```
   "This is a continuation \
   line"                          -> "This is a continuation line"
   control(file1(mode,type),\
   file2(type))                   -> control(file1(mode,type),file2(type))
   ```
5. Command files may be nested up to **25** levels.

**`-i[l|g]`** — Default handling of label identifiers. Default `-il` (data/code labels are LOCAL unless a
`GLOBAL` directive overrules). `-ig` makes them GLOBAL by default unless `LOCAL` overrules.
```
as88 -ig test.src
```

**`-L[flag...]`** — Specify which source lines are removed from the list file (only relevant with `-l`).
Default flag set: `-LcDEGlMnPQsWXy`. `-L` with no flags equals `-Lcdeglmnpqswxy` (remove all).
Lower-case = remove (on); upper-case = keep (off).

| Flag | Meaning |
|------|---------|
| `c` / `C` | (c=default remove) source lines containing assembler controls (the `OPTIMIZE` directive) / keep |
| `d` / `D` | remove section directives (`DEFSECT`, `SECT`) / (D=default) keep |
| `e` / `E` | remove symbol-definition directives (`EXTERN`, `GLOBAL`, `LOCAL`) / (E=default) keep |
| `g` / `G` | remove generic instruction expansion / (G=default) show it |
| `l` / `L` | (l=default) remove C preprocessor line info (`#line`) / keep |
| `m` / `M` | remove macro/dup directive lines (`MACRO`/`DUP`) / (M=default) keep |
| `n` / `N` | (n=default) remove empty source lines (newlines) / keep |
| `p` / `P` | remove conditional-assembly lines (`IF`/`ELSE`/`ENDIF`; only valid condition shown) / (P=default) keep |
| `q` / `Q` | remove assembler equate lines (`EQU`) / (Q=default) keep |
| `s` / `S` | (s=default) remove HLL symbolic debug lines (`SYMB`) / keep |
| `w` / `W` | remove wrapped part of source lines / (W=default) keep wrapped lines |
| `x` / `X` | remove `MACRO`/`DUP` expansions / (X=default) keep them |
| `y` / `Y` | (y=default) hide cycle counts / show cycle counts |

```
as88 -l -Lcw test.src
```

**`-l`** — Generate a listing file (`.lst`, basename of the output file). See also `-L`.

**`-Mmodel`** — Memory model (see also `$MODEL` control):

| Model | Meaning |
|-------|---------|
| `s` | small — max 64K code and data |
| `c` | compact code — max 64K code, 16M data |
| `d` | compact data — max 8M code, 64K data |
| `l` | large — max 8M code, 16M data |

Default: `-Ml`. `as88 -Ms test.src`

**`-o filename`** — Output filename (must be separated from `-o` by tab/space). Default: assembly
basename + `.obj`.
```
as88 test.src -o myfile.obj
```

**`-t`** — Produce totals: per-section memory address, size, cycle count, and name on stdout.
```
as88 -t test.src
Section summary:
 NR ADDR   SIZE CYCLE NAME
  1        0007     5 .text
  2 021234 000e     0 .data
  3        0001     0 .tiny
```

**`-V`** — Display the version header only and exit (must be the sole argument).
```
S1C88 assembler va.b rc       SN000000-015 (c) year TASKING, Inc.
```

**`-v`** — Verbose mode (prints filenames and pass progress).

**`-w[num]`** — `-w` suppresses all warnings; `-wnum` suppresses warning number `num` (repeatable).
```
as88 -w113 -w114 file.src
```

### 2.1.3 Environment Variables

- **`AS88INC`** — Search directories for include files (semicolon-separated). For `"..."` includes:
  search the including file's directory, then the current directory, then `AS88INC`, then `..\include`
  relative to the assembler binary. For `<...>` includes: the including-file directory and current
  directory are **not** searched (only `AS88INC` and the relative path).
- **`TMPDIR`** — Directory for temporary files (removed on normal termination). Defaults to the current
  working directory.

### 2.1.4 List File

Generated only with `-l`; named after the output basename with `.lst`. When `-l` is set, errors/warnings
are also emitted just below the offending source line.

**2.1.4.1 Absolute List File Generation.** After locating the whole application, an absolute list file can be
produced by re-assembling the source **with the locator map file** (`.map`) as the optional `map-file`
argument. Absolute list files contain absolute addresses (vs. relocatable in a standard list). The previously
generated object file is **not** overwritten. You must pass the **same options** used to build the object.
```
as88 -Ms test.src                 ; original
as88 -Ms -l test.src test.map     ; produces absolute test.lst
```

**2.1.4.2 Page Header.** Four lines: (1) assembler name/version/serial/copyright, (2) `TITLE`/`STITLE` +
page number, (3) source filename (first page) or empty, (4) source-listing column header.

**2.1.4.3 Source Listing columns** — header line: `ADDR   CODE      CYCLES LINE SOURCELINE`
- **ADDR** — 6-digit hex memory address; offset from start of a relocatable section, or absolute address
  for an absolute section. Shown only on lines that generate code.
- **CODE** — generated object code in hex. Relocatable parts are flagged with the letter `r`. Lines that
  allocate space (`DS`) show the text `RESERVED`.
- **CYCLES** — only with `-LY`. First value = instruction cycle count, second = cumulative count.
- **LINE** — decimal source line number (still increments when listing suppressed by `$LIST OFF`).
- **SOURCELINE** — copy of source text (tabs expanded; long lines wrap). Errors/warnings follow the line.

Example listing fragment (note `r` markers and `RESERVED`):
```
ADDR   CODE      LINE SOURCELINE
000000              1         defsect ".text", code
000000              2         sect    ".text"
000000 CEC6rr       4         ld      xp,#@dpag(data_label)
000003 CEC4rr       7         ld      nb,#@cpag(label)
000006 F101         8         jr      label
021234             13         defsect ".data", data at 21234h
                   14 data_label:
021234             16         ds      49
   |  RESERVED
021264
```

### 2.1.5 Debug Information

If the C compiler placed debug information in the source, `as88` passes it through to the object file (for C
source symbolic debugging). `as88` does **not** generate new debug information.

### 2.1.6 Instruction Set

`as88` accepts all S1C88 instruction mnemonics (see the *S1C88 Core CPU Manual* for the full table of
mnemonics, operands, opcode format, and states).

**Precaution — the `RETS` instruction:** `rets` returns to *return address + 2*. Sequences like a `carl`
followed by a 3-byte `carl _function` can therefore have the `rets` jump into the middle of the 3-byte
instruction. The assembler cannot detect this conflict.
```
carl  _label
carl  _function   ; 3-byte instruction
...
_label:
...
rets              ; in effect jumps into the middle of 'carl _function' (return addr + 2)
```

---

## 2.2 Software Concept

### 2.2.1 / 2.2.2 Modules

A program is divided into **modules**, one per file, each assembled separately. `INCLUDE` brings in common
definitions/macros; `mk88` manages dependencies for incremental re-assembly.

**Symbols in a module** can be:
- **Local** — not accessible from other modules. Created by the `LOCAL` directive, or by `SET`/`EQU`.
- **Global** — accessible from other modules. Either labels (global by default depending on `-i`/`$IDENT`)
  or symbols explicitly made global with `GLOBAL`.
- **External** — defined outside the module, declared with `EXTERN`.

### 2.2.3 Sections

Sections are relocatable blocks of code/data, **defined** once with `DEFSECT` and **activated**
(and re-activated = *continuation*) with `SECT`. The linker checks that section attributes match across
modules (error otherwise) and concatenates all like-named sections (all `.text` become one located chunk).
The locator assigns absolute addresses. A section is the smallest relocatable unit. The `@` character is not
allowed in *regular* section names — the assembler/linker reserve it for overlayable section naming.

`DEFSECT` general form:
```
DEFSECT sect_name, sect_type [, attrib]... [AT address]
```

**Section type** (`sect_type`): `CODE | DATA` — which memory the section lives in.

**Section attributes** (`attrib`):

| Attribute | Meaning |
|-----------|---------|
| `SHORT` | CODE: within first 32K of code memory. DATA: within first 64K of data memory. (Locator warns if it cannot be placed there.) |
| `TINY` | within one 256-byte page, in the first 64K of data memory (DATA) |
| `FIT 100H` | section must fit within one 256-byte page |
| `FIT 8000H` | section must fit within one 32K-byte page |
| `FIT 10000H` | section must fit within one 64K-byte page |
| `CLEAR` | clear (zero) section during program startup (DATA only) |
| `NOCLEAR` | section is not cleared at startup (default for all DATA sections) |
| `INIT` | DATA section holds initialization data copied from ROM to RAM at startup |
| `OVERLAY` | section must have an overlay name; only DATA sections are overlayable (implies `NOCLEAR`) |
| `ROMDATA` | section contains data (not executable code); allowed on DATA and CODE |
| `MAX` | DATA only: linker sizes the merged section to the maximum size across modules |
| `JOIN` | group sections together (use with `FIT`; same page size for all members) |

Per attribute *group* (see 2.6.9), at most one attribute may be specified. The startup code must clear
`CLEAR` DATA sections (data without initializers). `NOCLEAR` is the default and excludes a section from
that init.

**Overlayable sections** (DATA only): names embed `@` as `poolname@functionname`:
```
DEFSECT "OVLN@nfunc", DATA, OVERLAY, SHORT
            ^pool        ^function (after the @)
```
The linker overlays sections with the same **pool name**. To decide which DATA sections can be overlaid,
the linker builds a **call graph**; data of functions that call each other cannot be overlaid. The compiler
emits `CALLS` pseudo-instructions describing the graph:
```
CALLS 'caller_name', 'callee_name' [, 'callee_name']...
```
Example (function `main` with overlay data calling `nfunc`):
```
DEFSECT "OVLN@nfunc", DATA, OVERLAY, SHORT
DEFSECT "OVLN@main",  DATA, OVERLAY, SHORT
CALLS 'main', 'nfunc'
```

**Absolute sections** — declared with `AT address`. The locator places the section at that address.
`AT` may **not** be combined with `OVERLAY` (error). Absolute sections may only be *continued* in the
defining module; if the same absolute section is defined in another module the locator will try to place
both at the same address (error). For multi-module absolute placement, define the section relocatable and
set its start address in the locator description (`.dsc`) file.

**Grouped sections** — use `JOIN` together with `FIT` (which sets the page size; must match across the
group). Sections are grouped by the **extension after `@`** in the section name:
```
DEFSECT ".data1@group", DATA, JOIN, FIT 10000H
SECT    ".data1@group"
DEFSECT ".data2@group", DATA, JOIN, FIT 10000H
SECT    ".data2@group"
          ^section name  ^joined group name (after @)
```

**Activation** — all code/data-generating instructions and pseudos must be inside an active section. The
assembler warns if code/data appears with no section defined and activated.
```
DEFSECT ".STRING", CODE, ROMDATA
SECT    ".STRING"
_l001:  ASCII "hello world"
```

**More section examples:**
```
DEFSECT ".CONST", CODE AT 1000H      ; absolute section at 1000H
SECT    ".CONST"
DEFSECT ".text", CODE                ; relocatable CODE section
SECT    ".text"
DEFSECT ".fardata", DATA, CLEAR      ; relocatable DATA, zeroed at startup
SECT    ".fardata"
DEFSECT ".ovlf@f", DATA, OVERLAY     ; overlayable DATA for function "f"
SECT    ".ovlf@f"
```

---

## 2.3 Assembly Language (lexical / source format)

### 2.3.1 Input Specification

- One statement per line. A statement may be followed by a comment introduced by `;` and ended by EOL.
- **Line continuation:** a `\` as the **last** character on a line continues the statement on the next line;
  concatenated lines are processed as one. Max source statement length (first line + continuations) is
  **512 characters**.
- **Case:** upper/lower case are equivalent for **mnemonics and directives**, but **distinct** for labels,
  symbols, directive arguments, and literal strings. (Controllable via `-c` / `$CASE`.)

Statement grammar:
```
[label:] [instruction | directive | macro_call] [;comment]
```
- **label** — an identifier; need not start at column 1 but **must be followed by a colon `:`**.
  An *identifier* is letters, digits, and/or underscores `_`; the first character may not be a digit.
  Size limited only by available memory.
  ```
  LAB1:   ; This is a label
  ```
- **instruction** — any valid S1C88 mnemonic + operands.
  ```
  RET            ; no operand
  PUSH A         ; one operand
  ADD  BA,HL     ; two operands
  ```
- **directive** — see 2.6.
- **macro_call** — a call to a previously defined macro (2.5).
- A statement may be empty.

### 2.3.2 Assembler-Significant Characters

| Char | Meaning |
|------|---------|
| `;` | Comment delimiter |
| `\` | Line continuation, **or** macro dummy-argument concatenation operator |
| `?` | Macro value-substitution operator (decimal value of symbol) |
| `%` | Macro hex value-substitution operator |
| `^` | Macro local-label operator (also binary XOR operator) |
| `"` | Macro string delimiter, **or** quoted-string DEFINE-expansion character |
| `@` | Function delimiter (all built-in functions start with `@`) |
| `*` | Location-counter substitution (current runtime location counter) |
| `[]` | Location (memory-reference) addressing-mode operator |
| `#` | Immediate addressing-mode operator |

Details:

- **`;` comment** — anything after `;` (not inside a literal string) is a comment; reproduced in the listing,
  preserved inside macro definitions; may occupy a whole line or trail a statement.
- **`\` continuation / concatenation** — as the last char on a line, continues the statement. Inside macros,
  `\` concatenates a dummy argument with adjacent alphanumeric characters (place `\` before and/or after
  the argument name; no intervening blanks). See 2.5.5.1.
- **`?symbol`** — in macro definitions, replaced by an ASCII string giving the **decimal** value of `symbol`
  (must be integer). Combinable with `\`. See 2.5.5.2.
- **`%symbol`** — replaced by an ASCII string giving the **hexadecimal** value of `symbol` (integer).
  Combinable with `\`. Note `%` is also the binary-constant indicator; inside macros, escape a binary
  constant with `\` or parentheses. See 2.5.5.3.
- **`^`** — unary operator in a macro expansion that mangles an associated local label into a unique label:
  removes the leading underscore and appends `__M_Lxxxxxx` (unique sequence). No effect outside macro
  expansion. Also the binary XOR operator. See 2.5.5.5.
- **`"..."`** — in macro definitions the macro processor turns `"` into the single-quote `'` string delimiter
  while still scanning for dummy arguments (lets arguments become literal strings). DEFINE symbols are
  expanded inside `"..."` strings but **not** inside `'...'` strings. See 2.5.5.4.
- **`@`** — function delimiter. `SVAL EQU @ABS(VAL)`.
- **`*`** — current integer value of the runtime location counter.
  ```
  DEFSECT ".CODE", CODE AT 100H
  SECT    ".CODE"
  XBASE   EQU *+20H        ; XBASE = 120H
  ```
- **`[]`** — memory/location addressing. `LD A,[_Value]`
- **`#`** — immediate addressing. `LD A,#CNST`

### 2.3.3 Registers (reserved — cannot be symbol names)

`A B BA H L HL BR IX IY NB SC EP PC XP SP YP` (upper or lower case).

### 2.3.4 Other Special Names (reserved — cannot be symbol names)

Condition codes / flags used in the instruction set (upper or lower case):
`C P T M LT LE GT GE V NV NC NT F0 F1 F2 F3 NF0 NF1 NF2 NF3`.

---

## 2.4 Operands and Expressions

### 2.4.1 Operands

An operand follows the opcode. Instructions take zero, one, or two operands. Operand types:

| Type | Description |
|------|-------------|
| `expr` | any valid expression (2.4.2) |
| `reg` | any valid register (2.3.3) |
| `symbol` | a symbolic name from an equate (a symbol can be an expression) |
| `address` | a combination of `expr`, `reg`, and `symbol` |

Fully assembly-time-evaluable expressions are **absolute**; otherwise **relocatable**.

### 2.4.1.1 Addressing Modes (Epson AS88 syntax)

| Mode | Syntax | Notes |
|------|--------|-------|
| Register Direct | `mnemonic register` | register holds the operand |
| Register Indirect | `mnemonic [RR]` / `mnemonic [RR + off]` / `mnemonic [RR + L]` | register holds the operand address |
| Immediate | `mnemonic #number` | one byte or one word, encoded in the instruction; `#` prefix |
| Absolute | `mnemonic [direct_address]` | instruction contains the 8- or 16-bit operand address |
| PC Relative | `mnemonic offset` | instruction contains 8- or 16-bit offset relative to PC |
| Implied | `mnemonic` | registers implied by the instruction |

### 2.4.2 Expressions

An expression denotes an address (in a memory space) or a number. **Absolute** expressions resolve at
assembly time; **relocatable** expressions resolve at link/locate time and are emitted in the object file.
If any operand is relocatable, the whole expression is relocatable. Relocatable expressions may only
contain **integral** functions (a non-IEEE relocatable expression is an error). Evaluation is **64-bit
two's complement**.

Expression syntax:
```
- number
- expression_string
- symbol
- expression binary_operator expression
- unary_operator expression
- (expression)
- function
```
Parentheses control evaluation order: `(3+4)*5` = 35; `3+(4*5)` = 23.

### 2.4.2.1 Number

If no postfix, the number is in the default `RADIX` (default decimal = 10).

| Form | Radix | Rule | Examples |
|------|-------|------|----------|
| `bin_numB` | 2 | digits `0`–`1`, suffix `B`/`b` | `1001B`, `01100100b` |
| `dec_num` (`dec_numD`) | 10 | digits `0`–`9`, optional `D`/`d` | `12`, `5978D` |
| `oct_numO` / `oct_numQ` | 8 | digits `0`–`7`, suffix `O`/`o`/`Q`/`q` | `11O`, `447o`, `30146q` |
| `hex_numH` | 16 | digits `0`–`9`,`a`–`f`/`A`–`F`, suffix `H`/`h`; **first char must be a decimal digit** (prefix `0` if needed) | `45H`, `0FFD4h`, `9abcH` |

The radix suffix can be omitted if the input radix is changed with the `RADIX` directive.

### 2.4.2.2 Expression String

A string of arbitrary length evaluating to a number: value = first 4 characters, **padded with 0 on the
left**. `string` = ASCII characters in single (`'`) or double (`"`) quotes (start and close quote must match);
to embed the enclosing quote, double it.
```
'A'+1      ; 1-char ASCII string, result 42H
"9C"+1     ; 2-char ASCII string, result 3944H
```

### 2.4.2.3 Symbol

A symbol is an identifier representing a value defined (or to be defined) by a label declaration or an
equate. Predefined symbols available at invocation:
- **`_AS88`** — string with the assembler name (`"as88"`).
- **`_MODEL`** — integer holding the ASCII value of the selected MODEL (lower case).

```
CON1 EQU 3H
LD A,[CON1+20H]   ; load A with contents of address 23H
```

### 2.4.2.4 Expression Type

Result type is either **integer** (number) or **address**. Notes:
1. A label is type `address`; an equate symbol takes the type of its expression.
2. An untyped symbol may be address or number depending on context.
3. Binary logical/relational operators (`||`, `&&`, `==`, `!=`, `<`, `<=`, `>`, `>=`) accept any operand
   combination; result is always integer `0` or `1`.
4. Shift and bitwise operators (`<<`, `>>`, `|`, `&`, `^`) accept **integral operands only**.

**Table 2.4.2.4.1 — unary operator result types** (`*` = illegal):

| Operand type | `~` | `!` | `-` | `+` |
|--------------|-----|-----|-----|-----|
| integer | integer | integer | integer | integer |
| addr | `*` | `*` | `*` | integer |

**Table 2.4.2.4.2 — binary numerical operator result types** (`*` = illegal):

| Operands | `+` | `-` | `*` | `/` | `%` |
|----------|-----|-----|-----|-----|-----|
| integer, integer | integer | integer | integer | integer | integer |
| addr, integer | addr | addr | `*` | `*` | `*` |
| integer, addr | addr | `*` | `*` | `*` | `*` |
| addr, addr | `*` | integer | `*` | `*` | `*` |

> **[extraction note]** The PDF lays these operator/result tables out as grids; the row/column pairing
> above is reconstructed from the linear text and the surrounding prose. A string operand is converted to
> an integral number before use.

**Table 2.4.2.4.3 — function operand/result types:**

| Function | Operands | Result |
|----------|----------|--------|
| `@ABS()` | integer | integer |
| `@ARG()` | symbol | integer |
| `@AS88()` | – | integer **[sic: see note]** |
| `@CADDR()` | integer, addr | addr |
| `@CAT()` | string, string | string |
| `@CNT()` | – | integer |
| `@COFF()` | addr | integer |
| `@CPAG()` | addr | addr |
| `@DADDR()` | integer, addr | addr |
| `@DEF()` | symbol | integer |
| `@DOFF()` | addr | integer |
| `@DPAG()` | addr | addr |
| `@HIGH()` | addr | integer |
| `@LEN()` | string | integer |
| `@LOW()` | addr | integer |
| `@LST()` | – | integer |
| `@MAC()` | symbol | integer |
| `@MAX()` | integer, integer, ... | integer |
| `@MIN()` | integer, integer, ... | integer |
| `@MODEL()` | – | integer |
| `@MXP()` | – | integer |
| `@POS()` | string, string [, integer] | integer |
| `@SCP()` | string, string | integer |
| `@SGN()` | integer | integer |
| `@SUB()` | string, integer, integer | string |

> **[extraction note]** In the table `@AS88()` shows `Operands: –, Result: integer`, but the prose
> (2.4.4.6) says `@AS88()` returns the assembler executable **name** (a string, e.g. `'as88'`). Treat the
> prose as authoritative: `@AS88()` -> string. (The table's `CPAG`/`DPAG` results are `addr`; prose calls
> them the page number — relocatable when the input is relocatable.)

### 2.4.3 Operators

**Table 2.4.3.1 — precedence (highest first):**

| Operators | Type |
|-----------|------|
| `+`, `-`, `~`, `!` | unary |
| `*`, `/`, `%` | binary |
| `+`, `-` | binary |
| `<<`, `>>` | binary |
| `<`, `<=`, `>`, `>=` | binary |
| `==`, `!=` | binary |
| `&` | binary |
| `^` | binary |
| `\|` | binary |
| `&&` | binary |
| `\|\|` | binary |

Same-precedence binary operators evaluate left-to-right; unary operators right-to-left.
So `-4+3*2` = `(-4)+(3*2)`.

- **2.4.3.1 Addition/Subtraction** — `operand + operand`, `operand - operand`.
- **2.4.3.2 Sign** — unary `+operand` (no change), `-operand` (subtract from zero). `5+-3` = 2.
- **2.4.3.3 Multiplication/Division/Modulo** — `*` multiply, `/` integer divide (remainder discarded),
  `%` integer modulo (quotient discarded). Right operand of `/` and `%` may not be zero. `23%4` = 3.
- **2.4.3.4 Shift** — `operand << count`, `operand >> count` (count = absolute integer).
- **2.4.3.5 Relational** — `==`, `!=`, `<`, `<=`, `>`, `>=`; return integer 1 (true) / 0 (false).
- **2.4.3.6 Bitwise** — `&` AND, `|` OR, `^` XOR, `~` one's complement (NOT). `~0AH` = `0FFF5H` (over the
  64-bit width the high bits are also set). Integer operands only.
- **2.4.3.7 Logical** — `&&` AND, `||` OR, `!` NOT; return integer 1/0. `!(4<3)` = 1.

### 2.4.4 Built-in Functions

Syntax: `@function_name(argument[,argument]...)`. No spaces between name and `(`, or around the commas.
Five categories: mathematical, string, macro, assembler-mode, address-handling.

| Category | Functions |
|----------|-----------|
| Mathematical | `ABS`, `MAX`, `MIN`, `SGN` |
| String | `CAT`, `LEN`, `POS`, `SCP`, `SUB` |
| Macro | `ARG`, `CNT`, `MAC`, `MXP` |
| Assembler mode | `AS88`, `DEF`, `LST`, `MODEL` |
| Address handling | `CADDR`, `COFF`, `CPAG`, `DADDR`, `DOFF`, `DPAG`, `HIGH`, `LOW` |

**Detailed descriptions:**

- **`@ABS(expression)`** — absolute value (integer). `LD A,#@ABS(VAL)`
- **`@ARG(symbol | expression)`** — 1 if the macro argument is present, else 0. A `symbol` form must be
  single-quoted and name a dummy argument; an `expression` form is the argument's ordinal position.
  Warns if used outside a macro expansion. `IF @ARG('TWIDDLE')`
- **`@AS88()`** — name of the assembler executable (`'as88'` for the S1C88 family). `ANAME: DB @AS88()`
- **`@CADDR(code-page, code-offset)`** — code address from 32K bank (`code-page`) + 32K offset
  (`code-offset`). Relocatable if `code-offset` is relocatable. `@CADDR(3,8004h)` = `18004h`.
- **`@CAT(str1, str2)`** — concatenate two (quoted) strings. `@CAT('S1C','88')` = `'S1C88'`.
- **`@CNT()`** — count of current macro-expansion arguments (warns outside a macro).
- **`@COFF(address)`** — code page offset (32K offset) of `address`. Bit 16 (MSB) of the result is 0 when in
  the first 32K, else 1. `@COFF(07FFFH)` = `07FFFH`, `@COFF(0CFFFH)` = `0CFFFH`.
- **`@CPAG(address)`** — code page (32K bank). `@CPAG(07FFFH)` = `0`, `@CPAG(014FFFH)` = `2`.
- **`@DADDR(data-page, data-offset)`** — data address from 64K bank + 64K offset.
  `@DADDR(3,1234h)` = `31234h`.
- **`@DEF(symbol)`** — 1 if `symbol` is defined, else 0. Quoted -> looked up as a DEFINE symbol; unquoted ->
  ordinary label (not a MACRO name). `IF @DEF('ANGLE')`
- **`@DOFF(address)`** — data page offset (64K offset). `@DOFF(014FFFH)` = `04FFFH`.
- **`@DPAG(address)`** — data page (64K bank). `@DPAG(014FFFH)` = `1`.
- **`@HIGH(address)`** — 256-byte page **number**. `@HIGH(07FFFH)` = `07FH`.
- **`@LEN(string)`** — length of string. `@LEN('string')` = 6.
- **`@LOW(address)`** — 256-byte page **offset**. `@LOW(07FFFH)` = `0FFH`.
- **`@LST()`** — value of the LIST control flag. `LIST ON` increments it, `LIST OFF` decrements it.
- **`@MAC(symbol)`** — 1 if `symbol` is a macro name, else 0.
- **`@MAX(expr1[,exprN]...)`** — greatest argument. `@MAX(1,5,-3)` = 5.
- **`@MIN(expr1[,exprN]...)`** — least argument. `@MIN(1,5,-3)` = -3.
- **`@MODEL()`** — ASCII char value of the selected model (lower case). With `-Ms`, `@MODEL()` = `73h` (`'s'`).
- **`@MXP()`** — 1 if a macro is currently being expanded, else 0.
- **`@POS(str1, str2[, start])`** — position of `str2` in `str1`, starting at `start` (default beginning).
  `start` must be a positive integer ≤ `len(str1)`. `@POS('S1C88','88')` = 3.
- **`@SCP(str1, str2)`** — 1 if the two strings are equal, else 0.
- **`@SGN(expression)`** — -1 / 0 / 1 for negative / zero / positive (relative or absolute).
- **`@SUB(str, expr1, expr2)`** — substring of `str` starting at `expr1`, length `expr2` (error if either
  exceeds `len(str)`). `@SUB('S1C88',3,2)` = `'88'`.

---

## 2.5 Macro Operations

The macro preprocessor is part of the assembler. Macros provide shorthand for repeated instruction
patterns with variable fields and conditional assembly. A macro name becomes the mnemonic for calling it
(redefining an existing directive/opcode name issues a warning). Macro calls produce in-line code. Nested
macro **calls** are processed at expansion time; nested macro **definitions** are not processed until the
enclosing macro is expanded. A macro must be defined before it is called.

### 2.5.3 Macro Definition

Three parts: **header** (`MACRO` directive + name + dummy-argument list), **body** (skeleton statements),
**terminator** (`ENDM`).
```
macro_name   MACRO   [dummy argument list]   [comment]
   ...body...
   ENDM
```
Dummy-argument list: `[dumarg[,dumarg]...]` (comma-separated; each obeys global-symbol naming rules).
Local labels using the `^` operator are made unique per call (a unique postfix is appended), so a macro may
reuse local labels regardless of expansion count. Plain labels are normal labels — they cannot occur more
than once unless used with `SET`.

Example (note `^again` becomes unique, `_RESULT` does not):
```
N_R_MUL MACRO NMUL,AVEC,BVEC,OFFSET,RESULT   ;header
        LD   B,#NMUL                          ;body
        LD   IX,#AVEC
        LD   IY,#BVEC
^again: LD   L,[IX+OFFSET]
        LD   A,[IY+OFFSET]
        MLT
        ADD  A,[RESULT]
        LD   [RESULT],A
        INC  IX
        INC  IY
        DJR  NZ,^again
        ENDM                                  ;terminator

N_R_MUL 10H,_obj1,_obj2,10H,_RESULT
; expands to (again -> again__M_L000001, _RESULT unchanged):
        LD   B,#10H
        ...
again__M_L000001:
        LD   L,[IX+10H]
        ...
        DJR  NZ,again__M_L000001
```

### 2.5.4 Macro Calls

```
[label:]  macro_name  [arguments]  [comment]
arguments := [arg[,arg]...]
```
The optional `label` equals the location counter at the start of expansion. Calling arguments map
one-to-one (left-to-right) to dummy arguments. Arguments are comma-separated character sequences;
quotes are not required, but an argument with an embedded comma or space must be single-quoted.
**Null arguments** (no characters substituted) may be specified four ways: adjacent commas; trailing comma
omitting the rest; a null string; or simply omitting arguments. Supplying more arguments than the
definition has issues a warning.

### 2.5.5 Dummy Argument Operators

- **2.5.5.1 `\` (concatenation)** — concatenate a dummy argument with adjacent characters; place `\`
  before and/or after the argument name, no intervening blanks.
  ```
  SWAP_MEM MACRO REG1,REG2
           LD A,[I\REG1]
           LD B,[I\REG2]
           LD [I\REG1],B
           LD [I\REG2],A
           ENDM
  ; SWAP_MEM X,Y  ->  LD A,[IX] / LD B,[IY] / LD [IX],B / LD [IY],A
  ```
- **2.5.5.2 `?` (return decimal value)** — `?symbol` becomes the decimal value of `symbol` as a string;
  usable with `\`. (E.g. `[_lab\?REG1]` with `REG1=AREG`, `AREG SET 1` -> `[_lab1]`.)
- **2.5.5.3 `%` (return hex value)** — `%symbol` becomes the hex value of `symbol` as a string; usable with
  `\`. Because `%` is also the binary-constant prefix, escape a binary constant inside macros with `\` or
  parentheses.
  ```
  GEN_LAB MACRO LAB,VAL,STMT
  LAB\%VAL: STMT
            ENDM
  ; NUM SET 10 / GEN_LAB HEX,NUM,'NOP'  ->  HEXA: NOP
  ```
- **2.5.5.4 `"` (string operator)** — `"..."` becomes `'...'` while still scanning for dummy arguments, so
  arguments become literal strings. DEFINE expansion occurs in `"..."` (before macro substitution) but not
  in `'...'`.
  ```
  STR_MAC MACRO STRING
          ASCII "STRING"
          ENDM
  ; STR_MAC ABCD  ->  ASCII 'ABCD'
  ```
- **2.5.5.5 `^` (local-label)** — `^identifier` name-mangles the label so it is used literally (uniquely) in
  the expansion. Useful for passing a label name as an argument used as a local label inside the macro.

### 2.5.6 DUP / DUPA / DUPC / DUPF

Specialized "unnamed macro" forms — a simultaneous definition and call. Source lines between the `DUP*`
directive and `ENDM` follow macro rules (including, for `DUPA`/`DUPC`/`DUPF`, the dummy-operator
characters). Detailed syntax in 2.6.11–2.6.14.

### 2.5.7 Conditional Assembly

`IF` conditionally includes a block:
```
IF expression
   ...
[ELSE]            ; optional
   ...
ENDIF
```
Statements between `IF` and `ENDIF` are assembled only if `expression` is non-zero (true). With `ELSE`,
the `IF..ELSE` block assembles when true and the `ELSE..ENDIF` block when false. `IF` may nest to any
level; `ELSE`/`ENDIF` bind to the nearest previous `IF`. `expression` must be an absolute integer known
on pass one (no forward references). May also be used inside macro definitions for argument checking.

---

## 2.6 Assembler Directives

Directives (pseudo-instructions) control assembly rather than emitting machine code. Case-insensitive.
Functional groups: **Debugging** (`CALLS`, `SYMB`); **Assembly control** (`ALIGN`, `COMMENT`, `DEFINE`,
`DEFSECT`, `END`, `FAIL`, `INCLUDE`, `MSG`, `RADIX`, `SECT`, `UNDEF`, `WARN`); **Symbol definition**
(`EQU`, `EXTERN`, `GLOBAL`, `LOCAL`, `NAME`, `SET`); **Data definition / storage** (`ASCII`, `ASCIZ`, `DB`,
`DS`, `DW`); **Macros & conditional assembly** (`DUP`, `DUPA`, `DUPC`, `DUPF`, `ENDIF`, `ENDM`, `EXITM`,
`IF`, `MACRO`, `PMACRO`).

### Directive Summary (33 directives)

| Directive | Syntax | Purpose | Label allowed? |
|-----------|--------|---------|----------------|
| `ALIGN` | `ALIGN expression` | Align location counter to `2^k` | yes (gets aligned value) |
| `ASCII` | `[label:] ASCII string[,string]...` | ASCII bytes, no NUL | yes |
| `ASCIZ` | `[label:] ASCIZ string[,string]...` | ASCII bytes, NUL-terminated each | yes |
| `CALLS` | `CALLS 'caller','callee'[,nr]...[,'callee'[,nr]...]...` | Call-graph + stack info for overlay | no |
| `COMMENT` | `COMMENT delim ... delim` | Multi-line comment block | no |
| `DB` | `[label:] DB arg[,arg]...` | Define constant byte(s) | yes |
| `DEFINE` | `DEFINE symbol string` | Substitution string | no |
| `DEFSECT` | `DEFSECT section,type[,attr]...[AT address]` | Define section + attributes | no |
| `DS` | `[label:] DS expression` | Reserve storage (bytes) | yes |
| `DUP` | `[label:] DUP expression ... ENDM` | Duplicate lines N times | yes |
| `DUPA` | `[label:] DUPA dummy,arg[,arg]... ... ENDM` | Duplicate per argument | yes |
| `DUPC` | `[label:] DUPC dummy,string ... ENDM` | Duplicate per character | yes |
| `DUPF` | `[label:] DUPF dummy,[start],end[,inc] ... ENDM` | Duplicate as a loop | yes |
| `DW` | `[label:] DW arg[,arg]...` | Define constant word(s) | yes |
| `END` | `END` | End of source program | no |
| `ENDIF` | `ENDIF` | End conditional assembly | no |
| `ENDM` | `ENDM` | End macro / DUP definition | no |
| `EQU` | `name EQU expression` | Equate symbol (fixed) | (name required) |
| `EXITM` | `EXITM` | Exit macro expansion | no |
| `EXTERN` | `EXTERN [(attrib[,attrib]...)] symbol[,symbol]...` | External symbol declaration | no |
| `FAIL` | `FAIL [{str\|exp}[,{str\|exp}]...]` | Programmer error message (stops) | no |
| `GLOBAL` | `GLOBAL symbol[,symbol]...` | Global symbol declaration | no |
| `IF` | `IF expression ... [ELSE] ... ENDIF` | Conditional assembly | no |
| `INCLUDE` | `INCLUDE string \| <string>` | Include secondary file | no |
| `LOCAL` | `LOCAL symbol[,symbol]...` | Local symbol declaration | no |
| `MACRO` | `name MACRO [args] ... ENDM` | Macro definition | (name required) |
| `MSG` | `MSG [{str\|exp}[,{str\|exp}]...]` | Programmer message (no count change) | no |
| `NAME` | `NAME "str"` | Object-file identification | no |
| `PMACRO` | `PMACRO symbol[,symbol]...` | Purge macro definition | no |
| `RADIX` | `RADIX expression` | Change input radix (2/8/10/16) | no |
| `SECT` | `SECT "str" [, RESET]` | Activate section | no |
| `SET` | `name SET expression` | Set symbol (redefinable) | (name required) |
| `SYMB` | `SYMB string,expression[,abs_expr][,abs_expr]` | HLL symbolic debug (internal) | — |
| `UNDEF` | `UNDEF symbol` | Undefine DEFINE symbol | no |
| `WARN` | `WARN [{str\|exp}[,{str\|exp}]...]` | Programmer warning | no |

> The Chapter 2 detail subsections cover 33 directives (`ALIGN` … `WARN`); `CALLS` and `SYMB` are the two
> debugging directives, the rest are listed above.

### 2.6.2 ALIGN Directive
```
ALIGN expression
```
Align the location counter; `expression` must be a value of `2^k` and `> 0`. Default alignment is 1 byte.
If not `2^k`, a warning is issued and alignment rounds up to the next `2^k`. Alignment happens once at the
directive. A section's start is auto-aligned to the largest alignment value occurring in it. For relocatable
sections a gap is generated from the current relative location counter; for absolute sections the location is
unchanged and a gap is generated from the current absolute address.
```
ALIGN 4         ; align at 4 bytes
lab1: ALIGN 6   ; not a 2^k value -> warning; lab1 aligned on 8 bytes
```

### 2.6.3 ASCII Directive
```
[label:] ASCII string[, string]...
```
Allocate/initialize a byte array per string argument; **no NUL** appended (identical to `DB` with a string).
```
HELLO: ASCII "Hello world"    ; same as DB "Hello world"
```

### 2.6.4 ASCIZ Directive
```
[label:] ASCIZ string[, string]...
```
Like `ASCII` but appends a NUL byte to the end of **each** array.
```
HELLO: ASCIZ "Hello world"    ; same as DB "Hello world",0
```

### 2.6.5 CALLS Directive
```
CALLS 'caller', 'callee'[, nr ]... [, 'callee'[, nr ]... ]...
```
Create a flow-graph reference between a caller and its callees so the linker can build the overlay call
graph. After each callee name, a stack usage count `nr` may be given per stack (in bytes, including the
current function's RET address). The S1C88 tool currently uses only the system stack. An **empty** callee
name defines the stack usage of the function itself. Result appears in the linker map file (`-M`).
```
DEFSECT "OVLN@nfunc", DATA, OVERLAY, SHORT
DEFSECT "OVLN@main",  DATA, OVERLAY, SHORT
CALLS 'main', 'nfunc', 5
```

### 2.6.6 COMMENT Directive
```
COMMENT delimiter
   :
delimiter
```
Define one or more comment lines. The first non-blank character after `COMMENT` is the delimiter; the two
delimiters bound the comment text (the line with the second delimiter is the last comment line). No label
allowed. **Not permitted inside `IF/ELSE/ENDIF` or `MACRO/DUP` definitions.**
```
COMMENT + This is a one line comment +
COMMENT *  This is a multiple line comment. Any number of lines
           can be placed between the two delimiters.
        *
```

### 2.6.7 DB Directive
```
[label:] DB arg[, arg]...
```
Define constant byte(s). Each `arg` may be a numeric constant, single/multi-char string, symbol, or
expression; stored in successive locations. Null args (adjacent commas) store zero. Error if a value exceeds
one byte. `label` = location counter at directive start. Single-char strings store the ASCII byte (`'R'`=`52H`);
multi-char strings store each char (`'ABCD'` = `41H,42H,43H,44H`).
```
TABLE: DB 14,253,62H,'ABCD'
CHARS: DB 'A','B','C','D'
```

### 2.6.8 DEFINE Directive
```
DEFINE symbol string
```
Define a substitution string. All following source lines have `symbol` replaced by `string`. `symbol` follows
label naming rules. Redefining an existing DEFINE symbol warns. DEFINE translations apply to macro
definitions when encountered and again at expansion. No label allowed.
```
DEFINE ARRAYSIZ '10 * SAMPLSIZ'
DS ARRAYSIZ          ; -> DS 10 * SAMPLSIZ
```

### 2.6.9 DEFSECT Directive
```
DEFSECT section, type [, attr]... [AT address]
```
Define a section name and its attributes; activate later with `SECT`.
- **type:** `DATA | CODE`
- **attr groups** (at most one attribute per group):
  - Group 1: `SHORT | TINY`
  - Group 2: `FIT 100H | FIT 8000H | FIT 10000H`
  - Group 3: `OVERLAY | ROMDATA | NOCLEAR | CLEAR | INIT | MAX`
  - Group 4: `JOIN`

Semantics (see 2.2.3 for full detail): `CLEAR` zeroes DATA sections at startup; `NOCLEAR` (default for
DATA) excludes them; `INIT` marks ROM→RAM init data; `OVERLAY` (DATA only) makes the section
overlayable; `ROMDATA` (DATA or CODE) marks non-executable ROM data; `MAX` (DATA only) sizes the
merged section to the max across modules; `SHORT` = first 32K (CODE) / first 64K (DATA); `TINY` = one
256-byte page in the first 64K of data; `FIT n` = must not cross the given boundary (also caps size);
`JOIN` groups sections in one page (always with `FIT`). See also `SECT`.
```
DEFSECT ".text", DATA     ; declare section .text
SECT    ".text"           ; switch to section .text
```
> **[extraction note]** 2.6.9's example reads `DEFSECT ".text", DATA`, while the listing example in 2.1.4.3
> uses `defsect ".text", code`. Section type is per the programmer's intent — `.text` is conventionally CODE;
> the manual's two examples simply differ.

### 2.6.10 DS Directive
```
[label:] DS expression
```
Define storage — reserve `expression` bytes (uninitialized); advances the location counter. `expression`
must be an absolute integer `> 0` with no forward references to address labels. `label` = location counter
at start.
```
S_BUF: DS 12     ; Sample buffer
```

### 2.6.11 DUP Directive
```
[label:] DUP expression
   :
ENDM
```
Duplicate the enclosed source lines `expression` times. If `expression` ≤ 0 the lines are omitted.
`expression` must be an absolute integer with no forward references to address labels. May nest to any
level. `label` = location counter at start.
```
COUNT SET 3
DUP   COUNT       ; SRA BY COUNT
SRA   A
ENDM
; -> three  SRA A
```

### 2.6.12 DUPA Directive
```
[label:] DUPA dummy, arg[, arg]...
   :
ENDM
```
Repeat the block once per `arg`, substituting `dummy` with each argument. A null argument removes
`dummy`. Arguments with embedded blanks/significant chars must be single-quoted. `label` = location
counter at start.
```
DUPA VALUE,12,32,34
DB   VALUE
ENDM
; -> DB 12 / DB 32 / DB 34
```

### 2.6.13 DUPC Directive
```
[label:] DUPC dummy, string
   :
ENDM
```
Repeat the block once per **character** of `string`, substituting `dummy` with each character. Null string
skips the block. `label` = location counter at start.
```
DUPC VALUE,'123'
DB   VALUE
ENDM
; -> DB 1 / DB 2 / DB 3
```

### 2.6.14 DUPF Directive
```
[label:] DUPF dummy, [start], end[, increment]
   :
ENDM
```
Loop the block; generally `(end - start) + 1` times when `increment` is 1. `start` defaults to ... (the text
implies a default, matching `increment`'s default of 1 when omitted), `increment` defaults to 1. `dummy`
holds the loop index and may be used in the body. `label` = location counter at start.
```
DUPF NUM,0,3
LD   [NUM],A
ENDM
; -> LD [0],A / LD [1],A / LD [2],A / LD [3],A
```

### 2.6.15 DW Directive
```
[label:] DW arg[, arg]...
```
Define constant word(s). Each `arg` may be a numeric constant, single/double-char string, symbol, or
expression; stored in successive locations. Null args store zero. Error if a value exceeds one word.
**Words are little-endian: low 8 bits at the lowest address.** Single-char strings store the ASCII value in
the **low** byte **[sic: text says "lower seven bits"]**. Two-char strings store the first char as the **high**
byte, second as the **low** byte (`'AB'` = `4142H`). Strings of more than two characters are not allowed.
```
TABLE: DW 14,1635,2662H,'AB'
; equivalent to
TABLE: DB 14,0,1635%256,6,62H,26H,'B','A'
```
> **[extraction note]** The "lower seven bits" wording for single-char strings appears to be an extraction
> artifact; for two-char strings the example confirms full 8-bit bytes (`'AB' = 4142H`).

### 2.6.16 END Directive
```
END
```
Optional logical end of the source program. **Cannot** be used in a macro expansion. No label allowed.

### 2.6.17 ENDIF Directive
```
ENDIF
```
End the current level of conditional assembly. Binds to the most recent `IF`. May nest to any level.
No label allowed.
```
IF DEB
   MSG 'Debug Version'
ENDIF
```

### 2.6.18 ENDM Directive
```
ENDM
```
Terminate every `MACRO`, `DUP`, `DUPA`, `DUPC` (and `DUPF`). No label allowed.

### 2.6.19 EQU Directive
```
name EQU expression
```
Equate `name` to `expression` (relative or absolute; forward references allowed). The symbol **cannot be
redefined** elsewhere. An EQU symbol can be made global.
```
A_D_PORT EQU 4000H
```

### 2.6.20 EXITM Directive
```
EXITM
```
Immediately terminate a macro expansion (useful with `IF` for error conditions). No label allowed.
```
CALC MACRO XVAL,YVAL
     IF XVAL<0
        MSG 'Macro parameter value out of range'
        EXITM
     ENDIF
     :
     ENDM
```

### 2.6.21 EXTERN Directive
```
EXTERN [(attrib[, attrib]...)] symbol[, symbol]...
```
Declare symbols referenced but not defined in this module (defined outside any module, or `GLOBAL` in
another module). Optional `attrib`:
- `CODE` — symbol is in ROM
- `DATA` — symbol is in RAM
- `SHORT` — within first page (CODE: first 32K; DATA: first 64K)
- `TINY` — one 256-byte page of the first 64K page of DATA

If a referenced symbol is neither `EXTERN`-declared nor locally defined, a warning is issued and an EXTERN
symbol is inserted. No label allowed.
```
EXTERN AA,CC,DD
EXTERN (CODE,SHORT) EE
```

### 2.6.22 FAIL Directive
```
FAIL [{str | exp} [, {str | exp}]...]
```
Emit a programmer error message and increment the error count; **assembly stops immediately** after
printing. Typically used with conditional assembly. No label allowed.
```
FAIL 'Parameter out of range'
```

### 2.6.23 GLOBAL Directive
```
GLOBAL symbol[, symbol]...
```
Declare that the listed symbols are defined in this section/module and should be accessible from all
modules (via `EXTERN`). Error if a listed symbol is not defined in the module. Only **program labels and
EQU labels** can be made global. No label allowed.
```
GLOBAL LOOPA
```

### 2.6.24 IF Directive
```
IF expression
   :
[ELSE]
   :
ENDIF
```
Conditional assembly (see 2.5.7). `expression` must be an absolute integer known on pass one (no forward
references); true = non-zero. `IF` may nest to any level. No label allowed.
```
IF XVAL<0
   MSG 'Please select larger value for XVAL'
ENDIF
```

### 2.6.25 INCLUDE Directive
```
INCLUDE string | <string>
```
Insert a secondary source file. With `string`: search the current directory (or the directory in `string`)
first, then `AS88INC`, then `..\include` relative to the assembler binary. With `<string>`: the current
directory is **not** searched (only `AS88INC` and the relative path). No label allowed.
```
INCLUDE 'storage\mem.asm'
INCLUDE <data.asm>     ; do not look in current directory
```

### 2.6.26 LOCAL Directive
```
LOCAL symbol[, symbol]...
```
Declare the listed symbols explicitly local to this module/section (labels are global by default). Error if a
listed symbol is not defined in the module. No label allowed.
```
LOCAL LOOPA
```

### 2.6.27 MACRO Directive
```
name MACRO [dummy argument list]
   :
macro definition statements
   :
ENDM
```
Define a macro (see 2.5.3). Dummy-argument list: `[dumarg[, dumarg]...]`. Macro definitions may nest, but a
nested macro is not defined until the primary macro is expanded.
```
SWAP_MEM MACRO REG1,REG2
         LD A,[I\REG1]
         LD B,[I\REG2]
         LD [I\REG1],B
         LD [I\REG2],A
         ENDM
```

### 2.6.28 MSG Directive
```
MSG [{str | exp}[, {str | exp}]...]
```
Emit an informational message; **does not** change error/warning counts; assembly proceeds normally.
No label allowed.
```
MSG 'Generating tables'
```

### 2.6.29 NAME Directive
```
NAME "str"
```
Identify the produced object file (linker/locator map files, debugger "module" name). If omitted, the
module's source name is used.
```
NAME "strcat"
```

### 2.6.30 PMACRO Directive
```
PMACRO symbol[, symbol]...
```
Purge the named macro definitions from the macro table (reclaim space). No label allowed.
```
PMACRO MAC1,MAC2
```

### 2.6.31 RADIX Directive
```
RADIX expression
```
Change the input base for constants to `expression` (must evaluate to 2, 8, 10, or 16). Default radix 10.
The base-10 suffix is `D`. The constant used to set the radix must be expressed in the **then-current** base.
No label allowed.
```
_RAD10: DB 10        ; hex A
RADIX 2
_RAD2:  DB 10        ; hex 2
RADIX 16D
_RAD16: DB 10        ; hex 10
RADIX 3              ; bad radix expression
```

### 2.6.32 SECT Directive
```
SECT "str" [, RESET]
```
Activate the section named `str` (must already be defined by `DEFSECT`; subsequent activations use `SECT`
only). The `RESET` attribute resets counting storage allocation in DATA sections that have the `MAX`
attribute.
```
DEFSECT ".text", DATA
SECT    ".text"
```

### 2.6.33 SET Directive
```
name SET expression
```
Assign `expression` to `name`; **redefinable** (only by another `SET`). Forward references allowed. Useful
for temporary/reusable counters in macros. **SET symbols cannot be made global.**
```
COUNT SET 0
```

### 2.6.34 SYMB Directive
```
SYMB string, expression[, abs_expr] [, abs_expr]
```
Pass HLL symbolic debug information through the assembler to the debugger. `expression` is any
expression; `abs_expr` must yield an absolute value. **Internal to the tool chain — not meant for
hand-coded assembly; documented for completeness only.**

### 2.6.35 UNDEF Directive
```
UNDEF symbol
```
Release the substitution string associated with a DEFINE `symbol`; `symbol` is no longer a valid DEFINE
substitution. No label allowed.
```
UNDEF DEBUG
```

### 2.6.36 WARN Directive
```
WARN [{str | exp}[, {str | exp}]...]
```
Emit a programmer warning and increment the warning count; assembly proceeds normally. No label
allowed.
```
WARN 'parameter too large'
```

---

## 2.7 Assembler Controls

Controls alter the assembler's default behavior and are written on **control lines** — lines starting with a
dollar sign `$`. One control per source line; a control line may contain comments. Case-insensitive.

- **Primary controls** affect overall behavior and stay in effect for the whole assembly. They may only
  appear at the **beginning** of the source, before assembly starts. Specifying a primary control more than
  once warns and uses the last definition (so command-line options can override).
- **General controls** control the assembler during assembly and may appear anywhere. When a general
  control is given on the invocation line, the corresponding in-source general controls are ignored.

### 2.7.2 Overview — Assembler Controls

| Control | Type | Default | Description |
|---------|------|---------|-------------|
| `$CASE ON` | pri | ON | All user names case sensitive |
| `$CASE OFF` | pri | | User names not case sensitive |
| `$IDENT LOCAL` | pri | LOCAL | Default local labels |
| `$IDENT GLOBAL` | pri | | Default global labels |
| `$LIST ON` | gen | ON | Resume listing |
| `$LIST OFF` | gen | | Stop listing |
| `$LIST "flags"` | pri | `cDEGlMnPQsWXy` | Define what to include/exclude in the list file |
| `$MODEL [S\|C\|D\|L]` | pri | L | Select memory model (objects of different models cannot be linked) |
| `$STITLE "title"` | gen | (spaces) | Set list page-header title for following pages |
| `$TITLE "title"` | pri | (spaces) | Set list page-header title for the first page |
| `$WARNING OFF` | pri | (all enabled) | Suppress all warnings |
| `$WARNING OFF num` | pri | | Suppress one warning |

> **[extraction note]** The PDF's control table interleaves Type/Default columns; the mapping above is
> reconstructed (`$CASE`/`$IDENT GLOBAL`/`$LIST "flags"`/`$MODEL`/`$STITLE`/`$TITLE` are primary except
> `$LIST ON`/`$LIST OFF`/`$STITLE` which are general — `$STITLE` is documented as **general** class in
> 2.7.3.6, so the overview "gen" entry there is authoritative).

### 2.7.3 Control Descriptions

**2.7.3.1 CASE** — `$CASE ON` / `$CASE OFF`. Related option `-c`. Class: primary. Default `$CASE ON`.
Selects case sensitivity; in insensitive mode input is mapped to uppercase (literal strings excluded).
```
$case off    ; assembler in case insensitive mode
```

**2.7.3.2 IDENT** — `$IDENT LOCAL` / `$IDENT GLOBAL`. Related option `-i[l|g]`. Class: primary.
Default `$IDENT LOCAL`. Sets default treatment of **code/data** labels (local vs. global). SET identifiers
are always local. Overridable per label with `LOCAL`/`GLOBAL`.
```
$ident global   ; assembly labels global by default
```

**2.7.3.3 LIST ON/OFF** — `$LIST ON` / `$LIST OFF`. Related option `-l`. Class: general. Default
`$LIST ON`. Switch listing generation on/off starting at the next line. Actual file generation still requires
`-l`.
```
$list off
   :
$list on
```

**2.7.3.4 LIST** — `$LIST "flags"`. Related option `-L[flag...]`. Class: primary. Default
`$LIST "cDEGlMnPQsWXy"`. Same flags as the `-L` option (see 2.1.2).
```
$list "cw"   ; remove control lines and wrapped lines from list file
```

**2.7.3.5 MODEL** — `$MODEL [S|C|D|L]`. Related option `-Mmodel`. Class: primary. Default `$MODEL L`.

| Model | Description |
|-------|-------------|
| `S` | small — max 64K code and data |
| `C` | compact code — max 64K code, 16M data |
| `D` | compact data — max 8M code, 64K data |
| `L` | large — max 8M code, 16M data |

In small model you should never change CB/NB, and EP/XP/YP must be fixed. Objects assembled for different
models cannot be linked. (The C compiler sets this; an assembler programmer can still pick the "wrong"
model.)
```
$model s
```

**2.7.3.6 STITLE** — `$STITLE "title"`. Related option `-l`. Class: general. Default `$STITLE ""`.
Subtitle printed at the top of all succeeding pages until the next `$STITLE`. Not printed in the listing.
No-argument form blanks the subtitle. Truncated if it doesn't fit. See also TITLE.
```
$stitle "Demo title"
```

**2.7.3.7 TITLE** — `$TITLE "title"`. Related option `-l`. Class: primary. Default: spaces. Title for the
**first** page heading of the list file. Truncated if it doesn't fit. See also STITLE.
```
$title "NEWTITLE"
```

**2.7.3.8 WARNING** — `$WARNING OFF` / `$WARNING OFF num`. Related option `-w[num]`. Class: primary.
Default: all warnings enabled. `$WARNING OFF` suppresses all (same as `-w`); `$WARNING OFF num`
suppresses one warning by number (same as `-wnum`).
```
$warning off       ; switch all warnings off
```
