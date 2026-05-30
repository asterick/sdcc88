# S1C88 Toolchain — Utilities & File Formats Reference

> **Source:** *S5U1C88000C Manual I — "C Compiler/Assembler/Linker"*, **Chapter 6 UTILITIES** + **Appendices A–I** and the back-of-book Quick Reference card.
> **PDF page range distilled:** 221–331 (printed pages 209–293, plus appendices and quick-reference foldout).
> **Extraction note:** Reconstructed from machine-OCR plain text (`docs/_extract/id000920/page-NNN.txt`). All figures were lost in extraction; figures have been reconstructed as tables and are marked *(figure not captured)* where the original was a diagram. Several OCR oddities are flagged inline with **[OCR?]**.

This document targets engineers and AI agents building S1C88 tooling. The most interop-critical content is in the appendices: **Appendix H (IEEE-695 / MUFOM object format)** and **Appendix I (Motorola S-records)**.

---

## Table of Contents

- Chapter 6 Utilities
  - [6.1 Overview](#61-overview)
  - [6.2 ar88 — archiver / library maintainer](#62-ar88--archiver--library-maintainer)
  - [6.3 cc88 — control program](#63-cc88--control-program)
  - [6.4 mk88 — make utility](#64-mk88--make-utility)
  - [6.5 pr88 — IEEE object reader](#65-pr88--ieee-object-reader)
- Appendices
  - [Appendix A–F — Error message catalogs (summarized)](#appendix-af--error-message-catalogs-summarized)
  - [Appendix G — DELFEE locator description language](#appendix-g--delfee-locator-description-language)
  - [Appendix H — IEEE-695 (MUFOM) object format](#appendix-h--ieee-695-mufom-object-format)
  - [Appendix I — Motorola S-records](#appendix-i--motorola-s-records)
- [Toolchain file extensions & dataflow](#toolchain-file-extensions--dataflow)

---

## 6.1 Overview

Four utilities are supplied with the Cross-Assembler for the S1C processor family (in addition to the core
compiler `c88`, assembler `as88`, linker `lk88`, locator `lc88`):

| Utility | Role |
|---------|------|
| `ar88`  | IEEE archiver / librarian — create and maintain object libraries. |
| `cc88`  | Control program — drives the whole tool chain from one command line. |
| `mk88`  | Make utility — maintain, update, reconstruct groups of programs. |
| `pr88`  | IEEE object reader — view contents of files produced by the tool chain. **Not a disassembler.** |

---

## 6.2 ar88 — archiver / library maintainer

**Name:** `ar88` — IEEE archiver and library maintainer.

Combines separate object modules into a library file. The linker optionally pulls modules from a library
when a module resolves an external symbol referenced by an already-read module. `ar88` can build
libraries and replace/extract/remove modules from an existing library.

### Synopsis

```
ar88 key_option [option]... library [object_file]...
ar88 -V
ar88 -?
```

- `key_option` — main action (delete/replace/etc.). May appear in any order, at any place.
- `option`     — optional sub-options.
- `library`    — the library file (`.a`).
- `object_file`— object module (`.obj`) to add/extract/replace/remove.

Options may be given with or without a leading `-`, in any order, and may be combined (e.g. `-xv`).
`-V` and `-?` must be the **only** option on the command line.

### Key options (exactly one required)

| Key | Action |
|-----|--------|
| `-d` | Delete the named object modules from the library. |
| `-m` | Move the named modules to the end of the library, or to a position given by a positioning option. |
| `-p` | Print named modules to **stdout** (binary — normally redirected: `ar88 -p lib.a object.obj > t.obj`). |
| `-r` | Replace named modules if present; otherwise add them. New modules are appended. With no names, replaces only modules for which a same-named file exists in the current directory. |
| `-t` | Print a table of contents. With no names, lists all modules; with names, only those. |
| `-x` | Extract named modules (with no names, extract all). Does **not** alter the library. |

### Other options

| Option | Description |
|--------|-------------|
| `-?` | Display option explanation on stdout. |
| `-V` | Display version info on stderr. |
| `-a posname` | Append/move new modules **after** existing module `posname` (only with `m` or `r`). |
| `-b posname` | Insert/move new modules **before** existing module `posname` (only with `m` or `r`). |
| `-c` | Create the library without notification if it does not exist. |
| `-f file` | Read options from `file` (`-` = stdin; close stdin with EOF, usually Ctrl-Z). |
| `-o` | Reset last-modified date to the date recorded in the library (only with `x`). |
| `-s` | Print a list of symbols (must combine with `-t`). |
| `-s1` | Print a symbol list, each symbol preceded by `library:object_file:` (must combine with `-t`). |
| `-u` | Replace only modules whose last-modified date is later than the library file (only with `r`). |
| `-v` | Verbose; module-by-module description (only with `d`, `m`, `r`, or `x`). |
| `-wn` | Set warning level `n`. |

### Examples

```
# Create library clib.a from two modules:
ar88 cr clib.a startup.obj calc.obj

# Extract all modules from clib.a:
ar88 x clib.a

# List symbols from clib.a:
ar88 ts clib.a
#   startup.obj
#     symbols:
#           _start
#           _copytable
#   calc.obj
#     symbols:
#          _entry

# List symbols in library:object:symbol form:
ar88 ts1 clib.a
#   clib.a:startup.obj:_start
#   clib.a:startup.obj:_copytable
#   clib.a:calc.obj:_entry

# Delete a module:
ar88 d clib.a calc.obj
```

---

## 6.3 cc88 — control program

**Name:** `cc88` — control program for the S1C tool chain. Invokes compiler/assembler/linker/locator
from a single command line. Source files and options may appear in any order.

### Synopsis

```
cc88 [ [option]... [control]... [file]... ]...
cc88 -V
cc88 -?
```

### Argument types (recognized by file suffix)

| Suffix | Treated as | Passed to |
|--------|-----------|-----------|
| `-…`   | Option (some consumed by `cc88`, rest forwarded) | — |
| `.c`   | C source | compiler |
| `.asm` | Assembly source (preprocessed first) | assembler |
| `.src` | Compiled assembly source (no preprocess) | assembler (directly) |
| `.a`   | Library | linker |
| `.obj` | Object file | linker |
| `.out` | Linked object | locator (only one `.out` allowed) |
| `.dsc` | Locator command (description) file | locator — its presence adds a locate phase; absence stops after link |

Default flow: compile + assemble all sources → object files → link → locate → absolute output. Options
can suppress assembler/linker/locator stages. Intermediate files are auto-removed unless `-tmp` is given.

### Options

| Option | Description |
|--------|-------------|
| `-?` | Short option explanation on stdout. |
| `-M{s\|c\|d\|l}` | Memory model: small (`s`), compact data (`d`), compact code (`c`), large (`l`). |
| `-V` | Display version header and terminate. |
| `-Ta arg` / `-Tc arg` / `-Tlk arg` / `-Tlc arg` | Pass `arg` directly to assembler / C compiler / linker / locator. Arg may be appended or follow as a separate token. |
| `-al` | Generate an absolute list file for each module. |
| `-cs` | Stop after compilation (`.c`) / preprocessing (`.asm`); keep resulting `.src` files. |
| `-c`  | Stop after assembler; output `.obj` files. |
| `-cl` | Stop after link stage; output linker object `.out`. |
| `-f file` | Read command-line args from `file` (`-` = stdin). See *command-file rules* below. |
| `-ieee` | Locator output as IEEE-695 (`.abs`) — **default**. |
| `-srec` | Locator output as Motorola S-record (`.sre`). |
| `-nolib` | Do not supply the standard C / run-time libraries to the linker. |
| `-o file` | Output filename. Forwarded to locator normally; to linker with `-cl`; to assembler with `-c` (single source only); to compiler with `-cs`. |
| `-tmp` | Create intermediate files in the current directory; do not auto-remove them. |
| `-v`  | Echo each tool invocation on stdout, preceded by `+`. |
| `-v0` | Like `-v`, but only displays invocations — programs are **not** started. |

### Command-file (`-f`) format rules

1. Multiple arguments per line allowed.
2. Surround an argument with single or double quotes to embed whitespace.
3. Embedded quotes: use the opposite quote type; if both quote types appear, split so each embedded
   quote is wrapped by the opposite type. Examples:
   `"This has a single quote ' embedded"` / `'This has a double quote " embedded'` /
   `'This has a double quote " and a single quote '"' embedded"`.
4. Continuation lines end with a backslash + newline. In a quoted argument, the next line is appended
   without stripping leading whitespace; for non-quoted arguments, leading whitespace is stripped.
5. Command files may be nested up to **25** levels.

### Environment variables used by cc88

| Variable | Effect |
|----------|--------|
| `TMPDIR`   | Directory for temporary files (default: current directory). |
| `CC88OPT`  | Extra options/args, processed **before** the command line. |
| `CC88BIN`  | Directory prepended to the names of the invoked tools. |

---

## 6.4 mk88 — make utility

**Name:** `mk88` — maintain, update, and reconstruct groups of programs. Reads a makefile of
dependencies and decides which commands to run to bring files up to date (executing them, or printing
them). With no target on the command line, builds the first target in the first makefile.

### Synopsis

```
mk88 [option]... [target]... [macro=value]...
mk88 -V
mk88 -?
```

### Options

| Option | Description |
|--------|-------------|
| `-?` | Show invocation syntax. |
| `-D` | Display the makefile text as read in. |
| `-DD` | Display makefile text **and** `mk88.mk`. |
| `-G dirname` | `cd` to `dirname` before reading the makefile (build in another directory). |
| `-S` | Undo `-k`; stop on first non-zero exit status. |
| `-V` | Display version info on stderr. |
| `-W target` | Pretend `target` was modified "right now" (What-If). |
| `-d` | Show why each target is rebuilt (newer dependencies). |
| `-dd` | Show dependency checks in more detail (older deps too). |
| `-e` | Let environment variables override makefile macros (normally makefile macros win; command-line macros always win). |
| `-f file` | Use `file` instead of `makefile` (`-` = stdin). Multiple `-f` concatenate left-to-right. |
| `-i` | Ignore command error codes (== `.IGNORE:`). |
| `-k` | On non-zero status, abandon current target but continue independent branches. |
| `-n` | Dry run: print commands (incl. `@`-lines) but don't execute. (A `mk88` invocation line is still executed.) |
| `-q` | Question mode: exit status reflects whether target is up to date. |
| `-r` | Do not read the default `mk88.mk`. |
| `-s` | Silent: don't echo command lines (== `.SILENT:`). |
| `-t` | Touch targets up to date instead of running rules. |
| `-w` | Redirect warnings/errors to stdout (default: stderr). |
| `macro=value` | Fixed macro definition for this invocation; overrides makefile + environment; inherited by sub-makes as an environment variable. |

### Makefiles

- First makefile read is **`mk88.mk`**, searched (in order) in: current dir, `$HOME`, then the `etc`
  directory relative to `mk88`'s install dir (e.g. `mk88` in `\C88\BIN` → searches `\C88\ETC`).
- Default makefile name is `makefile` in the current directory; alternates via `-f`.
- Content: comments (`#…`, unless inside a quoted string), macro definitions, `include` lines, target
  lines. Line continuation via trailing `\`. `include` and `export` lines are left-justified keywords;
  `export` pushes a macro into the environment of executed commands.

### Conditional processing

```
ifdef macroname        # or: ifndef macroname
  if-lines
else
  else-lines
endif
```
Nesting up to **6** levels deep.

### Macros

Form: `WORD = text…`. Reference via `$(WORD)` or `${WORD}`; single-char names allow optional parens.
Expansion is recursive and occurs at **use** time, except a direct self-reference in the definition
(`DRINK = $(DRINK) or beer`) is expanded at definition point.

#### Special macros

| Macro | Meaning |
|-------|---------|
| `MAKE` | Normally `mk88`. Lines invoking it temporarily override `-n`. |
| `MAKEFLAGS` | Set of options given to `mk88`; if an env var, processed before command-line options. `-f`/`-d` not recorded. |
| `PRODDIR` | Install dir of `mk88` minus the last path component (root of the S1C package). E.g. `mk88` in `/c88/bin` → `PRODDIR = /c88`. |
| `SHELLCMD` | Default list of SHELL-local commands; if a rule invokes one, a SHELL is spawned. |
| `TMP_CCPROG` | Name of the control program (e.g. `cc88`). |
| `TMP_CCOPT` | Option telling the control program to read a command file (e.g. `-f`). If both `TMP_*` set and the arg list exceeds 127 chars, `mk88` writes a temp command file. |
| `$$` | Translates to a single `$`. |

#### Dynamic (abbreviation) macros

| Macro | Meaning |
|-------|---------|
| `$*` | Basename of the current target. |
| `$<` | Name of the current dependency file. |
| `$@` | Name of the current target. |
| `$?` | Dependents younger than the target. |
| `$!` | All dependents. |

Suffix `F` → filename component (e.g. `${@F}`); `$*`, `$<`, `$@` may take `D` → directory component.

### Functions

Built-in functions look like macros but have embedded spaces: `match`, `separate`, `protect`, `exist`,
`nexist`. Arguments may themselves be macros/functions.

| Function | Behavior |
|----------|----------|
| `$(match .obj prog.obj sub.obj mylib.a)` | Yields args matching the suffix → `prog.obj sub.obj`. |
| `$(separate "\n" prog.obj sub.obj)` | Joins args using the first arg as separator. In double quotes, `\n`, `\t`, `\ooo` (octal) are interpreted. |
| `$(protect …)` | Adds one level of quoting; wraps in `"…"` and escapes `"`/`\` if whitespace/quotes/backslash present. |
| `$(exist file then-arg)` | Expands to second arg if `file` exists. |
| `$(nexist file then-arg)` | Opposite of `exist`. |

### Targets, special targets, rules

Target line: `target ... : [dependency ...] [; rule]` followed by indented rule lines. Targets repeated on
multiple lines accumulate dependencies. MS-DOS drive-letter colons (`c:foo.obj : a:foo.c`) are handled.

Special targets: `.DEFAULT:`, `.DONE:`, `.IGNORE:`, `.INIT:`, `.SILENT:`, `.SUFFIXES:`, `.PRECIOUS:`.

Rule-line prefixes:

| Prefix | Effect |
|--------|--------|
| `@` | Don't echo the command (unless `-n`). |
| `-` | Ignore the command's exit code. |
| `+` | Force execution via a shell / `COMMAND.COM`. |

Inline temp files: `<<WORD` … `WORD` writes the enclosed lines to a temp file and substitutes its name.
Example:
```
lk88 -o $@ -f <<EOF
$(separate "\n" $(match .obj $!))
$(separate "\n" $(match .a $!))
$(LKFLAGS)
EOF
```

### Implicit rules

Tied to `.SUFFIXES:`. Each suffix-pair defines how to build one file from another sharing a basename.

### Example makefiles

Explicit:
```
LIB = -ls
prog.out: prog.obj sub.obj
	lk88 prog.obj sub.obj $(LIB) -o prog.out
prog.obj: prog.c inc.h
	c88 prog.c
	as88 prog.src
sub.obj: sub.c inc.h
	c88 sub.c
	as88 sub.src
```

Using implicit rules from `mk88.mk`:
```
LDFLAGS = -ls
prog.out: prog.obj sub.obj
prog.obj: prog.c inc.h
sub.obj: sub.c inc.h
```

**Files:** `makefile` (dependencies/rules), `mk88.mk` (default rules).
**Diagnostics:** exit status 1 on error, else 0.

---

## 6.5 pr88 — IEEE object reader

**Name:** `pr88` — IEEE object reader. Displays the contents of a relocatable object file or an absolute
file produced by the S1C tool chain. **Not a disassembler.**

### Synopsis

```
pr88 [option]... file
pr88 -V
pr88 -?
```

If no display "part" is specified, the default is **`-hscegd0i0`** (all parts; debug and image parts shown
as a table of contents).

### Input control option

| Option | Description |
|--------|-------------|
| `-f file` | Read command-line info from `file` (`-` = stdin). Same command-file rules as `cc88` (quoting, continuation, nesting up to 25 levels). More than one `-f` allowed. |

### Output control options

| Option | Description |
|--------|-------------|
| `-H` or `-?` | Option explanation on stdout. |
| `-V` | Version info on stderr. |
| `-Wn` | Output width = `n` columns. Default 128, minimum 78. |
| `-ln` | Level control (see *Object Layers* below). |
| `-ofile` | Output file (default stdout). |
| `-v` | Print selected parts verbose. |
| `-vn` | Print level `n` verbose (shorthand for `-v -ln`). |
| `-wn` | Suppress messages above warning level `n`. |

### Display options (object parts)

| Option | Displays |
|--------|----------|
| `-c` | Call graphs. |
| `-d` | All debug info except global types. |
| `-d0` | Table of contents for the debug part. |
| `-dn` | Debug info from file number `n`. |
| `-e` (`-e0`) | Variables with external scope. |
| `-e1` | External-scope variables, each preceded by the object-file name. |
| `-g` | Global types. |
| `-h` | General file info (header + environment + AD/extension parts as a whole). |
| `-i` | All section images. |
| `-i0` | Table of contents for the image part. |
| `-in` | Image of section `n`. |
| `-s` | Section info. |

### Preparing the demo files (§6.5.1)

The chapter's examples use `calc.obj`, `calc.out`, `calc.abs`, built with:
```
cc88 -Ms -nolib startup.asm _copytbl.asm calc.asm -o calc.abs s1c88316.dsc -tmp
```

### -h, general file info (§6.5.2.1)

```
pr88 -h calc.out
File name    = calc.out:
Format       = Relocatable
Produced by  = S1C object linker
Date         = jan 23, 1997 16:35:40h
```
With `-hv`, also: Obj version, Processor (`S1Cs`), Address size (24 bits), Byte order (LSB at lowest
address), Host, and a **part offset/length table**:

| Part | File offset | Length |
|------|-------------|--------|
| Header part | 0x00000000 | 0x00000055 |
| AD Extension part | 0x00000055 | 0x00000033 |
| Environment part | 0x00000088 | 0x0000002b |
| Section part | 0x000000b3 | 0x0000009b |
| External part | 0x0000014e | 0x00000098 |
| Debug/type part | 0x000001e6 | 0x000002b8 |
| Data part | 0x0000049e | 0x000002b8 |
| Module end | 0x00000756 | — |

### -s, section info (§6.5.2.2)

```
pr88 -s calc.out
Section           Size
.startup_vector   0x000002
.startup          0x000063
.watchdog_vector  0x000002
.watchdog         0x000001
.text             0x00002d
.data             0x000003
.zdata            0x000001
```
For an **absolute** file the sections are combined into clusters, so `pr88 -s calc.abs` shows cluster sizes
(e.g. `rom 0x0000b9`, `ram 0x00f800`). With `-sv` the verbose form adds columns:
`Section | Size | Address | Align | PageSize | Mau | Attributes`. (Address `-` = still relocatable. Section
alignment is always 1 for S1C. MAU = minimum addressable unit, in bits.)

#### Section attributes

**Allocation attributes** (used by the locator):

| Attribute | Meaning |
|-----------|---------|
| `Write` | Must be located in RAM. |
| `ReadOnly` | May be located in ROM. |
| `Execute` | May be located in ROM. |
| `Space num` | Must be located in addressing mode `num`. |
| `Abs` | Already located by the assembler. |
| `Cleared` | Section must be initialized to `0`. |
| `Initialized` | Section must be copied from ram to rom. **[OCR? — text reads "copied from ram to rom"; semantics are init-data copied rom→ram]** |
| `Scratch` | Section is not filled or cleared. |

**Overlap attributes** (used by the linker):

| Attribute | Meaning |
|-----------|---------|
| `MaxSize` | Use largest length encountered. |
| `Unique` | Only one section with this name allowed. |
| `Cumulate` | Concatenate same-named sections into one bigger section. |
| `Overlay` | Sections named `name@func` combined into one section `name` per call-graph `func` rules. |
| `Separate` | Sections are not linked. |

### -c, call graphs (§6.5.2.3)

Used by the linker's overlay algorithm; removed from the object once linked/overlaid. A call graph is a
function followed by indented (2-space) lists of called functions/graphs; graphs may reference each other
(recursion = a graph calling itself). Verbose form draws an ASCII tree. To produce a resolved call graph:
```
lk88 -o call.out -Mcr calc.obj
# -M → .lnl file (verbose call graph); -c → .cal file (compact); -r → incremental link
```

### -e, external part (§6.5.2.4)

```
pr88 -e calc.out
Variable     S  Address/Size
__start_cpt  I  .startup + 0x00
__START      I  .startup + 0x00
__exit       I  .startup + 0x20
__copytable  I  .startup + 0x22
_main        I  .text + 0x20
__lc_es      X  -
__lc_cp      X  -
```
Status column **S**: `I` = defined (internal) symbol, `X` = referenced but not yet defined. Relocatable
addresses shown as `section + offset`; undefined symbols show `-`. `-e1` prefixes the object-file name.
Verbose (`-ev`) adds `Type`, `Attrib` (e.g. `0x0020` = assembler-generated), `MAU` (bits), `Amod`
(addressing mode).

### -g, global type information (§6.5.2.5)

Used by the linker for inter-module type checking (suppress with `-gn` at compile time). Output columns:
`Tp#` (hex type index; numbering starts at 0x101, < 0x100 reserved for basic types), `Mnem`, `Name`,
`Entry` (type parameters; referenced types prefixed `T`).

#### Basic types

| Index | Type | Meaning |
|-------|------|---------|
| 1 | void | — |
| 2 | char | 8 bits signed |
| 3 | unsigned char | 8 bits unsigned |
| 4 | short | 16 bits signed |
| 5 | unsigned short | 16 bits unsigned |
| 6 | long | 32 bits signed |
| 7 | unsigned long | 32 bits unsigned |
| 10 | float | 32-bit floating point |
| 11 | double | 64-bit floating point |
| 16 | int | 16 bits signed |
| 17 | unsigned int | 16 bits unsigned |

#### Type mnemonics

| Mnem | Description | Parameters |
|------|-------------|------------|
| `G` | generalized structure | size, [member, Tindex, offset, size]… (sizes/offsets in bits) |
| `N` | enumerated type | [name, value]… |
| `n` | pointer qualifier | Tindex, memspace |
| `O` | small pointer | Tindex |
| `P` | large pointer | Tindex |
| `Q` | type qualifier | q-bits, Tindex |
| `S` | structure | size, [member, Tindex, offset]… |
| `T` | typedef | Tindex |
| `t` | compiler generated type | Tindex |
| `U` | union | size, [member, Tindex, offset]… |
| `X` | function | x-bits, Tindex, 0, nbr-arg, [Tindex]… (first Tindex = return type, rest = parameter types) |
| `Z` | array | Tindex, upper-bound |
| `g` | bit type | sign, nbr-of-bits |

Notes: value `-1` (`0xffffffff`) means *unknown* (e.g. variadic function, unbounded array). `T0` compares
equal to any type; `N0` compares equal to any name. `-g` has no verbose equivalent.

### -d, debug information (§6.5.2.6)

`-d0` → table of contents of files; `-dn` → one file; `-d` → all. Without `-v`: local variables + procedure
info only. With `-v`: also local types, line numbers, stack-update info, more procedure info.

Local symbol status letters: `N` = local symbol; `G`/`S` mark procedures (printed to define their relative
position). Procedure block columns: `Name | S | Additional information`, where `G` = external (global)
function, `S` = static (local) function. Each function carries 5 parameters:

| Param | Meaning |
|-------|---------|
| #1 | Frame type (not used). |
| #2 | Frame size — distance from SP before the call to just after local variables. |
| #3 | Type of the function. |
| #4 | Start address (`section + offset` in a relocatable object). |
| #5 | Last function address. |

Line-number info maps `Address | Line`. Stack info gives the SP delta at each executable address
(measured from just after the function's locals; pushing one byte increments the delta by one). The
per-module debug block ends with a per-function block listing local variables.

### -i, section images (§6.5.2.7)

`-i0` → list of available section images; `-in` → image of section `n`. Hex dump, one byte per address.
Special byte markers:

| Marker | Meaning |
|--------|---------|
| `rr` | Byte not yet relocated (value undeterminable). |
| `ss` | Scratch memory — may or may not be initialized by start-up code; info not available to the reader. |
| (no image) | Sections cleared during startup → "No image allowed, cleared during startup". |

`-iNv` prefixes each line with the section offset / absolute address and appends an ASCII gutter.

### Viewing an object at lower level (§6.5.3)

The object file is layered like the OSI model:

| Level | View |
|-------|------|
| `-l0` (level 0) | Raw bytes — the binary-encoded MUFOM commands. |
| `-l1` (level 1) | Decoded MUFOM commands (e.g. `ST`, `AS`, `SA`). |
| `-l2` (level 2, **default**) | MUFOM environment — type/section tables built, values assigned, attributes set; predefined IEEE-695 meanings applied. |

(Higher levels — a compiler↔debugger protocol for target/language-specific info — are not supported by
the reader.) Levels may be mixed: `-l01` (== `-l10` / `-l0 -l1`). When mixing level 0/1 the switch is per
MUFOM command; mixing level 1/2 the switch is per object part. The general `-v` makes all selected
levels verbose; `-vn` selects **and** makes only level `n` verbose (so `-l0 -v1` = non-verbose level 0 +
verbose level 1).

At level 1, MUFOM variables `Ln` and `Sn` are the **address** and **size** of section `n` (at level 2 these
are resolved and not shown explicitly).

---

## Appendix A–F — Error message catalogs (summarized)

> **Summarized:** The manual lists every diagnostic verbatim. Below the structure and representative
> samples are kept; the full numbered catalogs are not reproduced in their entirety.

Message line format (assembler example):
```
[E|F|W] error_number: filename line number : error_message
as88 E214: \tmp\tst.src line 17 : illegal addressing mode
```

Severity letters used across tools:

| Letter | Meaning |
|--------|---------|
| `I` | information |
| `W` | warning |
| `E` | error |
| `F` | fatal error |
| `S` | internal compiler error (compiler only) |
| `V` | verbose message (linker/locator) |

### Appendix A — C compiler error messages (PDF p.249–264)

Numbered roughly **F1 … E560**, split into **Frontend** (≈1–327) and **Backend** (≈501–560). Covers
preprocessor, declaration/type, prototype, and S1C-specific diagnostics. Representative samples:

- `E 4: expected number more '#endif'` — unmatched `#if`/`#ifdef`/`#ifndef`.
- `F 11: file stack overflow` — `#include` nesting depth (50) exceeded.
- `E 117: "name" undefined` — identifier used before declaration.
- `E 327: too many arguments to pass in registers for _asmfunc 'name'` — register-passing limit reached.
- `E 511: interrupt function must have void result and void parameter list` — `_interrupt(n)` constraints.
- `W 512: 'number' illegal interrupt number (0, or 3 to 251) - ignored`.
- `E 513: calling an interrupt routine, use '_swi()'`.
- `E 528–532: _at() …` — absolute-address placement constraints (numeric address, in range, globals only, uninitialized only, no effect on extern).
- `E 560: Float/Double: not yet implemented`.

### Appendix B — Assembler (as88) error messages (PDF p.265–273)

Warnings **W101–W172**, Errors **E200–E298**, Fatal **F401–F416**. Samples:

- `W 105: section activation expected, use name directive` — use `SECT`.
- `E 214: illegal addressing mode`.
- `E 218: unknown mnemonic: "name"` (often a label missing its `:`).
- `E 219: this is not a hardware instruction (use $OPTIMIZE OFF "H")`.
- `E 239: byte constant out of range` / `E 240: word constant out of range` — `DB`/`DW` range.
- `E 276: immediate value must be between value and value` — use `&` or `#>`.
- `E 297: jump address must be a code address`.
- `F 412: macro calls nested too deep` (limit 1000).

### Appendix C — Linker (lk88) error messages (PDF p.274–277)

Warnings **W100–W119**, Errors **E200–E222**, Fatal **F400–F415**, Verbose **V000–V008**. Samples:

- `E 201: Bad magic number` — bad library file.
- `E 205: Symbol 'name' already defined in <name>`.
- `E 208: Found unresolved external(s):` (an error unless `-r`).
- `E 220: page size (0x…) overflow for section <name>` — section too big for its page.
- `V 008: Embedded environment name read, relaxed addressing mode check enabled`.

### Appendix D — Locator (lc88) error messages (PDF p.278–283)

Warnings **W100–W142**, Errors **E200–E266**, Fatal **F400–F420**, Verbose **V000–V007**. Samples:

- `E 200: Absolute address 0x… occupied`.
- `E 208: Cannot find a cluster for section name` (often a `.dsc` error).
- `E 220: Symbol 'symbol' already defined`.
- `E 261: User assert: message` — assertion from the `.dsc` layout part.
- `F 407: No description file found`.
- `F 417: Overlaying not yet done` — link without `-r` first.

### Appendix E — Archiver (ar88) error messages (PDF p.284–285)

Warnings **W100–W109**, Errors **E200–E207**, Fatal **F300–F318**. Samples:

- `W 104: Option -a or -b only allowed with key option 'r' or 'm'. Ignored!`.
- `F 310: filename not in archive format`.
- `F 311/F312: …more than one key {rxdmpt}… / no … key … specified`.
- `F 315: IEEE violation for object module name at address` (with `z` option).
- `F 316: corrupted object module name` — not conforming to IEEE object spec.

### Appendix F — Embedded Environment error messages (PDF p.286–287)

These are part of the **linker and/or locator** message streams (the numbers below are *not* shown in the
final message). Errors **E1–E21**, Warning **W100**. Concerned with the Delfee/embedded-environment
mapping (`amode`/`space`/`bus`/`chip` definitions). Samples:

- `E 6: Page size must be a power of 2`.
- `E 7: Mau size must be a power of 2`.
- `E 17: Cannot find bus/space 'name' in definition for space 'name'`.
- `E 19: Cannot find chip 'name' in definition for bus 'name'`.
- `W 100: Cannot find mapping 'name' in segment definition for space 'name'`.

---

## Appendix G — DELFEE locator description language

Appendix G (PDF p.288–291) gives the **grammar** of DELFEE, the language of the locator description
(`.dsc`) file. The file is divided into partitions: **cpu**, **memory**, and **software**.

### Top-level structure

```
description         ::= partition+
partition           ::= memory_partition | cpu_partition | software_partition
cpu_partition       ::= cpu { static_specs_list } | cpu { } | cpu file_name
memory_partition    ::= memory { static_specs_list } | memory { } | memory file_name
software_partition  ::= software { layout_blocks } | software { } | software file_name
```

### CPU / Memory partition specs

`static_specs` is one of: `amod_specs`, `spce_specs` (space), `bus_specs`, `chips_specs`.

```
amod_specs   ::= amode ident_list { amod_list }
spce_specs   ::= space ident_list { spce_list }
bus_specs    ::= bus   ident_list { bus_list }
chips_specs  ::= chips ident_list chips_list ;

amod_def     ::= mau_spec | attribute_spec | map_spec
spce_def     ::= mau_spec | map_spec
bus_def      ::= mau_spec | mem_spec | map_spec
chips_def    ::= mau_equ_spec | attribute_equ_spec | size_spec

mau_spec            ::= mau NUMBER ;
mau_equ_spec        ::= mau = NUMBER
attribute_spec      ::= attribute STRING ; | attribute NUMBER ; | attr STRING ; | attr NUMBER ;
attribute_equ_spec  ::= attribute = STRING | attribute = NUMBER | attr = STRING | attr = NUMBER
map_spec            ::= map map_list ;
mem_spec            ::= mem mem_list ;
```

`map_def` fields: `src = NUMBER`, `size = NUMBER`, `dst = NUMBER`, `align = NUMBER`,
`page = NUMBER`, `amode = identifier`, `space = identifier`, `bus = identifier`.
`mem_def` fields: `address|addr = NUMBER`, `chips = low_chip_list` (chips combined with `|` and `,`).

### Software partition

```
software_specs ::= start | process
start          ::= start = identifier ;
process        ::= process = pids               # pids := NUMBER [, NUMBER]...
layout         ::= layout { space_blocks } | layout { } | layout file_name
space_block    ::= space identifier { block_blocks }
block_block    ::= block identifier { cluster_blocks }
cluster_spec   ::= cluster identifier { amod_blocks } | cluster ident_list ;
amode_block    ::= amode ident_list { section_blocks } | amode ident_list ;
```

Physical-placement specs inside a cluster/amode block:
`gap [length];`, `fixed address;`, `pool [length];`, `skip;`, `label identifier;`.

Virtual section specs (`section_block`): `section selection [modifiers];`, `copy [selection] [attribute];`,
`fixed address;`, `gap;`, `reserved [options];`, `stack [options];`, `heap [options];`, `table [attribute];`,
`others;`, `label identifier;`, `label = identifier`, `assert ( bool_expression , STRING );`, plus
`attribute_spec`. A `modifier` is an `attribute` or an `address`.

`bool_expression` uses operators `<`, `>`, `==`, `!=` over `+`/`-` terms, parentheses, identifiers, NUMBERs.

### Lexical notes

- **NUMBER** — series of (hex) digits with optional suffixes `k`/`M`/`G` (kilo/mega/giga); hex/octal/
  decimal with the usual prefix; may be preceded by `-`.
- **STRING** — a series of characters that is *not* a number (e.g. `089` is a STRING — not a valid octal
  number); alphanumeric plus `_`, `.`, `-`, and directory separators `\`, `/`, `:`.
- **Environment variables** — any token may embed them: `$A/proto.dsc` → `foo/proto.dsc`; multi-char
  names need braces: `window = $(MODE);`.
- **Comments** — three styles: C `/* … */`; `#` in the first column (allows C-preprocessing; `#line`/`#file`
  directives are ignored by the locator); C++ `// …` to end of line.

---

## Appendix H — IEEE-695 (MUFOM) object format

Appendix H (PDF p.292–304) is the most important interop spec. The toolchain's object format is
**IEEE-695 / MUFOM** ("Microprocessor Universal Format for Object Modules"), a *command language*
rather than a record-oriented format. The standard does **not** prescribe symbolic-debug encoding, so
debug info here is vendor/CPU-specific.

### H.2 Command-language concept

An object file is a *sequence of commands* that steers five MUFOM processes:

1. **Creation** — assembler/compiler emits commands.
2. **Linkage** — resolves externals by renaming `X` → `I` variables and assigning `R` variables.
3. **Relocation** — gives each section an absolute address by assigning its `L` variable.
4. **Expression evaluation** — evaluates loader expressions.
5. **Loader** — loads the absolute image.

E.g. the `LR` command instructs the linker to *load* a number of MAUs (absolute bytes or an expression
to be evaluated). The locator combines processes 3 and 4.

### H.3 Notational conventions

| Notation | Meaning |
|----------|---------|
| `\|` | select one of the items between `\|` |
| `" "` | literal characters |
| `[ ]+` | optional item, repeats one or more times |
| `[ ]?` | optional item, repeats zero or one time |
| `[ ]*` | optional item, repeats zero or more times |
| `::=` | "is defined as" |

### H.4 Expressions and variables

Variable names start with a non-hex letter (`G`–`Z`) plus an optional hex number; the first letter gives
the class:

| Var | Class / meaning |
|-----|-----------------|
| `G` | Program start address. Defaults to address of low-level symbol `_start` if unassigned. |
| `I` | Global (internal) symbol in a module; created by `NI`. Available to other modules for linkage. |
| `L` | Section start address (absolute sections only). `L` + section index; section defined by `ST`. |
| `N` | Internal/local symbol name; used for local symbols, type building, inter-module type checking. Created by `NN`. |
| `P` | Program pointer per section — current target address. `P` + section index. Created on first assignment. |
| `R` | Relocation reference for a section; all addresses in the section are relative to it. Linking assigns a new `R`. Default (unassigned) value = 0. |
| `S` | Section size in MAUs; one per section. `S` + section index. Created on first assignment. |
| `W` | Work variable — holds intermediate values, no extra meaning. |
| `X` | External reference; **cannot be assigned a value**. |

Expressions are in **reverse Polish notation** (operator follows operands).

```
digit            ::= "0".."9"
hex_letter       ::= "A".."F"
hex_digit        ::= digit | hex_letter
hex_number       ::= [ hex_digit ]+
nonhex_letter    ::= "G".."Z"
identifier       ::= letter [ alpha_num ]*
char_string_length ::= hex_digit hex_digit        # 2-hex-digit length
char_string      ::= char_string_length [ character ]*
```

#### Functions / operators

| Class | Members |
|-------|---------|
| Boolean (no operand) | `@F` (false), `@T` (true). |
| Monadic (`operand , monop`) | `@ABS`, `@NEG`, `@NOT` (boolean negation / one's complement), `@ISDEF` (true if all vars defined). |
| Dyadic (`op1 , op2 , dyadop`) | `@AND`, `@MAX`, `@MIN`, `@MOD`, `@OR`, `@XOR`, `+`, `-`, `/`, `*`, `<`, `>`, `=`, `#` (unequal). |
| `@INS` (4-operand) | `op1,op2,op3,op4,@INS` — insert bitstring op2 into op1 from bit op3 to op4. |
| `@EXT` (3-operand) | `op1,op2,op3,@EXT` — extract bitstring from op1, bits op2..op3. |
| Conditional | `value,condition,err_num,@ERR` and `condition,@IF,expr,@ELSE,expr,@END`. |

Notes: division rounds toward zero; result undefined if divisor is 0. `@MOD` undefined if either operand
negative or divisor zero.

### H.5 MUFOM commands

#### H.5.1 Module-level

| Command | Syntax | Notes |
|---------|--------|-------|
| `MB` (module begin) | `"MB" machine_identifier [ "," module_name ]? "."` | First command. Example: `MB S1C88.` |
| `ME` (module end) | `"ME."` | Last command. |
| `DT` (date/time) | `"DT" [digit]* "."` | Format `YYYYMMDDHHMMSS`. Example: `DT19930120120432.` |
| `AD` (address descr.) | `"AD" bits_per_MAU [ "," MAU_per_address [ "," order ]? ]?` | `order` = `L` (little-endian) / `M` (big-endian). Example: `AD8,3,L.` = 3-byte-addressable 8-bit little-endian. |

#### H.5.2 Comment / checksum

- `CO` (comment): `"CO" [comment_level]? "," comment_text "."` — comment levels 0–6 reserved for
  standard-layer revision numbers; contents otherwise implementation-defined.
- `CS` (checksum): starts/checks the checksum calculation.

#### H.5.3 Sections

A *section* is the smallest separately controllable unit of code/data. Each has a unique number introduced
at its first `SB`. A section ends at the next `SB` with a different number and resumes at an `SB` with a
previously-introduced number.

| Command | Syntax |
|---------|--------|
| `SB` (section begin) | `"SB" hex_number "."` |
| `ST` (section type) | `"ST" section_number [ "," section_type ]* [ "," section_name ]? "."` |
| `SA` (section align) | `"SA" section_number "," [ MAU_boundary ]? [ "," page_size ]? "."` |

`SA` forces alignment on `MAU_boundary` MAUs; with `page_size`, the relocator checks the section does
not cross a page boundary.

##### ST section-type letters

| Letter | Class | Meaning |
|--------|-------|---------|
| `A` | access | absolute — absolute address assigned to the corresponding L-variable |
| `R` | access | read only — no write access |
| `W` | access | writable — read and write |
| `X` | access | executable code |
| `Z` | access | zero / short-addressable page — map into it if target has one |
| `Ynum` | access | must be located in addressing mode `num` |
| `B` | access | blank — must be initialized to `0` (cleared) |
| `F` | access | not filled — not filled or cleared (scratch) |
| `I` | access | initialize — must be initialized in ROM |
| `E` | overlap | equal — error if sections in two modules differ in length |
| `M` | overlap | max — use largest value as section size |
| `U` | overlap | unique — the section name must be unique |
| `C` | overlap | cumulative — concatenate same-named sections (preserve partial-section alignment) |
| `O` | overlap | overlay — combine `name@func` sections into one `name` per call-graph rules |
| `S` | overlap | separate — multiple same-named sections may relocate at unrelated addresses |
| `N` | when | now — located before normal (non-`N`/`P`) sections |
| `P` | when | postpone — located after normal sections |

#### H.5.4 Symbolic-name declaration & type definition

| Command | Syntax | Purpose |
|---------|--------|---------|
| `NI` | `"N" I_variable "," char_string "."` | Define an internal symbol (visible outside the module). Must precede any reference to the `I` var; names/numbers unique. |
| `NX` | `"N" X_variable "," char_string "."` | Declare an undefined external; resolved by an `NI` in another module. Must precede all uses of the `X` var. |
| `NN` | `"N" N_variable "," char_string "."` | Define a local name (module-scoped) for a local symbol or a type-definition name. |
| `AT` | `"AT" variable "," type_table_entry [ "," lex_level [ "," hex_number ]* ]? "."` | Attach debug info (e.g. symbol type number) to an `I`/`N`/`X` variable. `type_table_entry` references a `TY` type (forward refs allowed). |
| `TY` | `"TY" type_table_entry [ "," parameter ]+ "."` | Define a new type-table entry. `parameter ::= hex_number \| N_variable \| "T"type_table_entry`. |

Type comparison (for linkers without level-3 semantics): two types are equal iff equal parameter count,
equal numeric values, same-named `N` variables, and referenced type entries compare equal. `N0` and `T0`
compare equal to anything.

#### H.5.5 Value assignment

| Command | Syntax |
|---------|--------|
| `AS` (assign) | `"AS" MUFOM_variable "," expression "."` |

#### H.5.6 Loading commands

| Command | Syntax | Notes |
|---------|--------|-------|
| `LD` (load) | `"LD" [hex_digit]+ "."` | Absolute data; loaded **most-significant part first**, contiguously per the section's `P` variable. |
| `IR` (init reloc base) | `"IR" relocation_letter "," relocation_base [ "," number_of_bits ]? "."` | Associates a relocation base with a relocation letter. `number_of_bits` ≤ bits-per-address (MAU_per_address × bits_per_MAU); defaults to bits-per-address. Ex: `IRV,X20,16.` |
| `LR` (load w/ reloc) | `"LR" [load_item]+ "."` | `load_item ::= relocation_letter offset "," \| load_constant \| "(" expression [ "," number_of_MAUs ]? ")"`. Ex: `LR002000400060.`, `LRT80,0020.`, `LR(R2,100,+,4).` |
| `RE` (replicate) | `"RE" expression "."` | Number of times the immediately-following `LR` is replicated. Ex: `RE04.` then `LR(R2,200,+,4).` loads 16 MAUs. |

#### H.5.7 Linkage commands

| Command | Syntax | Notes |
|---------|--------|-------|
| `RI` (retain internal) | `"R" I_variable [ "," level_number ]? "."` | Keep an `NI` symbol's info in the output. |
| `WX` (weak external) | `"W" X_variable [ "," default_value ]? "."` | If the external stays unresolved, assign the default value. |
| `LI` (library search list) | `"LI" char_string [ "," char_string ]* "."` | Default library search list. |
| `LX` (library external) | `"L" X_variable [ "," char_string ]+ "."` | Library to search for a named unresolved variable. |

### H.6 Binary encoding (MUFOM functions)

The first byte of each MUFOM element classifies what follows. Numbers > 127 are length-prefixed
(`0x82` = a 2-byte integer follows; `0xE4` = the `LR` command code).

#### First-byte overview

| Range | Meaning |
|-------|---------|
| `0x00–0x7F` | Start of a regular string, or a one-byte number 0–127. |
| `0x80` | Code for an omitted optional number field. |
| `0x81–0x88` | Numbers outside the range 0–127. |
| `0x89–0x8F` | Unused. |
| `0x90–0xA0` | User-defined function codes. |
| `0xA0–0xBF` | MUFOM function codes (identifiers). |
| `0xC0` | Unused. |
| `0xC1–0xDA` | MUFOM letters. |
| `0xDB–0xDF` | Unused. |
| `0xE0–0xF9` | MUFOM commands. |
| `0xFA–0xFF` | Unused. |

#### Letter codes (`0xC1`–`0xDA`)

| Letter | Code | Letter | Code | Letter | Code |
|--------|------|--------|------|--------|------|
| A | 0xC1 | J | 0xCA | S | 0xD3 |
| B | 0xC2 | K | 0xCB | T | 0xD4 **[OCR: "OxD4"]** |
| C | 0xC3 | L | 0xCC | U | 0xD5 |
| D | 0xC4 | M | 0xCD | V | 0xD6 |
| E | 0xC5 | N | 0xCE | W | 0xD7 |
| F | 0xC6 | O | 0xCF | X | 0xD8 |
| G | 0xC7 | P | 0xD0 | Y | 0xD9 |
| H | 0xC8 | Q | 0xD1 | Z | 0xDA |
| I | 0xC9 | R | 0xD2 | | |

#### Function codes (`0xA0`–`0xB9`)

The function-code column lists `@F`, `@T`, `@ABS`, `@NEG`, `@NOT`, `+`, `-`, `/`, `*`, `@MAX`, `@MIN`,
`@MOD`, `<`, `>`, `=`, `!=`/`<>`, `@AND`, `@OR`, `@XOR`, `@EXT`, `@INS`, `@ERR`, `@IF`, `@ELSE`,
`@END`, `@ISDEF` assigned to codes `0xA0`–`0xB9`.

> **[OCR caveat]** In the source extraction, the function-name list (`@F … @ISDEF`) and the code list
> (`0xA0 … 0xB9`) are printed as two separate parallel columns and their precise 1-to-1 pairing is not
> unambiguous from the text alone. The first ~5 align as `@F=0xA0, @T=0xA1, @ABS=0xA2, @NEG=0xA3,
> @NOT=0xA4`, and the ranges (functions `0xA0–0xBF`) are reliable; verify exact mid-list pairings against
> the IEEE-695 standard / `../skiploom` before encoding.

#### Command codes (`0xE0`–`0xF9`)

| Code | Cmd | Description |
|------|-----|-------------|
| 0xE0 | `MB` | Module begin |
| 0xE1 | `ME` | Module end |
| 0xE2 | `AS` | Assign |
| 0xE3 | `IR` | Initialize relocation base |
| 0xE4 | `LR` | Load with relocation |
| 0xE5 | `SB` | Section begin |
| 0xE6 | `ST` | Section type |
| 0xE7 | `SA` | Section alignment |
| 0xE8 | `NI` | Internal name |
| 0xE9 | `NX` | External name |
| 0xEA | `CO` | Comment |
| 0xEB | `DT` | Date and time |
| 0xEC | `AD` | Address description |
| 0xED | `LD` | Load |
| 0xEE | `CS` | Checksum followed by sum value |
| 0xEF | `CS` | Checksum (reset sum to 0) |
| 0xF0 | `NN` | Name |
| 0xF1 | `AT` | Attribute |
| 0xF2 | `TY` | Type |
| 0xF3 | `RI` | Retain internal symbol |
| 0xF4 | `WX` | Weak external |
| 0xF5 | `LI` | Library search list |
| 0xF6 | `LX` | Library external |
| 0xF7 | `RE` | Replicate |
| 0xF8 | `SC` | Scope definition |
| 0xF9 | `LN` | Line number |
| 0xFA–0xFF | — | Undefined |

> Note: `SC` (0xF8, scope definition) and `LN` (0xF9, line number) appear in the binary code table but are
> not described in the §H.5 command prose — likely level-3 (language-layer) commands.

---

## Appendix I — Motorola S-records

Appendix I (PDF p.305) documents the locator's S-record output (`-srec` / `.sre`). The locator generates
three record types: **S0**, **S2**, **S8**. For the S1C88, addresses are **3 bytes** (matching `AD8,3,L`).

### Record layouts

| Type | Layout | Purpose |
|------|--------|---------|
| `S0` | `'S' '0' <length_byte> <2 bytes 0> <comment> <checksum_byte>` | Header/comment record (no execution-relevant info). |
| `S2` | `'S' '2' <length_byte> <address(3 bytes)> <code bytes> <checksum_byte>` | Program code/data. Output buffer = 32 code bytes per record. |
| `S8` | `'S' '8' <length_byte> <address(3 bytes)> <checksum_byte>` | End record — carries the program start address. |

`length_byte` = number of bytes in the record **after** the record-type and length byte (i.e. address +
data + checksum).

### Checksum algorithm (same for S0/S2/S8)

Sum the binary values of every byte from `length_byte` up to (but excluding) the checksum, take the
one's complement, and keep the least-significant byte. Equivalently, the sum of all bytes after the record
type (including the checksum) is `0xFF`.

### Examples

```
# S0 header generated by the locator:
#   length_byte = 0x10, comment = "E0C88 locator", checksum = 0x88
#      E 0 C 8 8   l o c a t o r
S0100000534D433838206C6F6361746F7288

# S2 data record (3-byte address FF0020):
S213FF002000232222754E00754F04AF4FAE4E22BF
#   ^^      = length (0x13)
#     ^^^^^^ = address (FF0020)
#           …code…                            ^^ = checksum (BF)

# S8 end record with start address FF0003:
S804FF0003F9
#   ^^      = length (0x04)
#     ^^^^^^ = address (FF0003)
#           ^^ = checksum (F9)
```

---

## Toolchain file extensions & dataflow

Reconstructed from the back-of-book *C Program Development Flowchart* (PDF p.308) *(figure not
captured — rendered as the table below)*:

| Extension | File | Produced/consumed by |
|-----------|------|----------------------|
| `.c`   | C source | input to `c88` (with C preprocessor) |
| `.src` | assembly source | output of `c88`, input to `as88` |
| `.asm` | hand-written assembly | preprocessed → `as88` |
| `.lst` | assembler list file | `as88` |
| `.obj` | relocatable object module | `as88` → linker `lk88` |
| `.a`   | relocatable object library | `ar88` → linker |
| `.out` | linker object | `lk88` → locator `lc88` |
| `.lnl` | link map file | `lk88` |
| `.map` | locate map file | `lc88` |
| `.dsc` | locator description (DELFEE) file | input to `lc88` |
| `.abs` | absolute load module (IEEE-695) | `lc88` (default) → debugger / object reader |
| `.sre` | Motorola S-record object file | `lc88` with `-srec` |
| `.cal` | compact call graph | `lk88 -c` |
| `.err` / `.lst` / `.map` | diagnostics / lists | various |

Tool roles: `c88` (compiler), `as88` (assembler), `lk88` (incremental linker), `lc88` (locator), `ar88`
(librarian), `cc88` (control program), `mk88` (program builder), `pr88` (object reader); the debugger is
S5U1C88000H5.

---

### Provenance / gaps

- **Captured utilities:** `ar88`, `cc88`, `mk88`, `pr88` (Chapter 6, full options + examples).
- **Captured appendices:** A (C compiler msgs — summarized), B (assembler msgs — summarized),
  C (linker msgs — summarized), D (locator msgs — summarized), E (archiver msgs — summarized),
  F (embedded-environment msgs — summarized), G (DELFEE grammar — full), H (IEEE-695/MUFOM —
  full incl. binary code tables), I (Motorola S-records — full).
- **Key file formats documented:** IEEE-695/MUFOM object format (command language, variables,
  expressions, section-type letters, full command/letter/function **binary code tables**) and Motorola
  S0/S2/S8 records (3-byte addresses, checksum algorithm).
- **Not in this range:** Intel-HEX is **not** documented (the toolchain emits IEEE-695 `.abs` or Motorola
  S-records only). The compiler/assembler/linker/locator option references and the C library catalog
  appear only as a condensed **Quick Reference card** (PDF p.307–329) that restates Chapters 1–5 — not
  re-transcribed here as it duplicates earlier chapters.
- **Pages 306, 330–331:** blank page / EPSON sales-office list / colophon (manual first issued Oct 2001,
  printed Mar 2008) — no technical content.
- **OCR flags:** MUFOM function-code↔name pairing within `0xA0–0xB9` is ambiguous in the extraction
  (see note in §H.6); letter-code `T` printed as `OxD4` (read 0xD4); the `Initialized` section-attribute
  gloss in §6.5.2.2 reads "copied from ram to rom" which is likely an inversion of the intended init-data
  rom→ram copy.
