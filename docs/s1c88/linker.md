# S1C88 Linker (`lk88`) Reference

> **Source:** S5U1C88000C Manual I — *C Compiler/Assembler/Linker*, **Chapter 3 LINKER**.
> **PDF pages 162–175** (printed pages 150–163).
> Distilled from machine-extracted plain text. Figures lost in extraction are reconstructed as
> tables and marked *(figure not captured)*. Garbled/ambiguous spots are flagged inline.

This document describes `lk88`, the relocatable object **linker** for the Epson S1C88 toolchain. It
covers the linking model, invocation/options, library handling, output format, overlay sections,
high-level type checking, and the linker message model.

---

## 3.1 Overview

The linker executable for the S1C88 is **`lk88`**.

`lk88` combines relocatable object files (produced by the assembler) into one new relocatable
object file (preferred extension **`.out`**). Key properties of the linking model:

- **Incremental / repeatable.** The `.out` output is itself relocatable and may be fed back as
  input to a subsequent linker call. Linkage can therefore be **incremental**. Incremental linking
  must be selected explicitly (via `-r`).
- **Unresolved references.** Normally the linker complains about unresolved external references.
  Under incremental linking it is normal to leave unresolved references in the output; those
  diagnostics are suppressed (`-r`).
- **Object files and libraries.** The linker reads ordinary object files and **libraries** of object
  modules. A module in a library is included **only when it is referenced**.
- **Load module.** When the linkage process completes with **no unresolved references**, the
  generated object is called a **load module**.
- **Overlaying linker.** The compiler generates **overlayable sections** holding space for
  variables that are local (at C level) to a function. If two functions do not call each other,
  their local variables can occupy the same memory (be *overlaid*). The linker builds a **call
  graph** from function-call information and uses its structure to decide how sections can be
  overlaid, minimizing RAM use (see §3.5).
- **Incremental disables overlaying.** Because `-r` (incremental) turns off overlaying, the **final
  link phase must not be incremental** — even if an earlier incremental phase already resolved all
  externals.

After the linker, the **locator** assigns absolute addresses (see §3.4 — the linker output is still
relocatable).

### Fig. 3.1.1 — S1C88 Linker dataflow *(figure not captured; reconstructed)*

| Direction | File | Extension | Role |
|-----------|------|-----------|------|
| Input  | object files          | `.obj` | relocatable objects from the assembler |
| Input  | object library        | `.a`   | archive of object modules (built by `ar88`) |
| Input  | incremental object    | `.out` | a prior linker output, re-linked |
| Output | load module           | `.out` | relocatable combined object (default `a.out`) |
| Output | map file              | `.lnl` | link map + call graph (only with `-M`) |
| Output | call graph file       | `.cal` | separate compressed call graph (only with `-c`) |
| Output | error file            | `.elk` | redirected error messages (only with `-err`) |

---

## 3.2 Linker Invocation

```
lk88 [ option ]...  file ...
```

- Options may appear in **any order** and start with `-`. **Only `-lx` is position dependent.**
- Options may be **combined**: `-rM` ≡ `-r -M`.
- An option taking a filename/string may be written with or without a space: `-oname` ≡ `-o name`.
- `file` may be an object file (`.obj`), an object library (`.a`), or an incremental linker output
  (`.out`). **Files are linked in the order they appear on the command line.**

### Options summary

| Option | Argument | Description |
|--------|----------|-------------|
| `-C` | — | Link **case insensitive** (default: case sensitive) |
| `-L directory` | directory | Additional search path for system libraries |
| `-L` | — (no directory) | Skip the `C88LIB` system-library search |
| `-M` | — | Produce a link map (`.lnl`) |
| `-N` | — | Turn **off** overlaying |
| `-O name` | name | Specify basename of the resulting map file(s) |
| `-V` | — | Display version header only (must be the sole argument) |
| `-c` | — | Produce a separate call graph file (`.cal`) |
| `-e` | — | Clean up (remove link products) if the result is erroneous |
| `-err` | — | Redirect error messages to an error file (`.elk`) |
| `-f file` | file | Read command-line information from `file`; `-` means stdin |
| `-l x` | x | Search also in system library `libx.a` (**position dependent**) |
| `-o filename` | filename | Specify name of output file |
| `-r` | — | Suppress undefined-symbol diagnostics (incremental linking) |
| `-u symbol` | symbol | Enter `symbol` as undefined in the symbol table |
| `-v` or `-t` | — | Verbose: print the name of each file as it is processed |
| `-w n` | n (0–9) | Suppress messages above warning level `n` |

> Note: the summary table renders `-V` as "Display version header only" and `-v`/`-t` as the
> verbose option — they are distinct.

### 3.2.1 Detailed description of linker options

**`-C`** — Link case insensitive. Default is case sensitive linking.

**`-L [directory]`** — Add `directory` to the list of directories searched for **system libraries**.
Directories given with `-L` are searched **before** the standard directories named by the
environment variable `C88LIB`. If you specify `-L` **without** a directory, `C88LIB` is **not**
searched for system libraries. `-L` may be used more than once; the search path is built in the
order the directories appear on the command line.
*Note:* directory names that include `"O"` (capital letter) cannot be specified with `-L`.
*(extraction note: this restriction reads oddly but is reproduced as written.)*

**`-M`** — Produce a link map (`.lnl`).

**`-N`** — Turn off overlaying. Useful for debugging.

**`-O name`** — Use `name` as the default basename for the resulting map files.

**`-V`** — Display the version header of the linker. This option must be the **only** argument of
`lk88`; other options are ignored, and the linker exits after printing the header.

**`-c`** — Generate a separate call graph file (`.cal`).

**`-e`** — Remove all link products (temporary files, the output file, and the map file) **in case
an error occurred**.

**`-err`** — Redirect error messages to a file with the same basename as the output file and the
extension `.elk`. The default filename is `a.elk`.

**`-f file`** — Read command-line information from `file`. If `file` is `-`, the information is read
from standard input (you must provide the EOF code to close stdin). Command files work around
limits on command-line length and can be generated on the fly (e.g. by `make`). More than one `-f`
is allowed. Format rules:

1. Multiple arguments may appear on the same line.
2. To include whitespace in an argument, surround it with single or double quotes.
3. Embedded quotes inside a quoted argument:
   a. If the embedded quotes are only single **or** only double, surround the argument with the
      *opposite* quote.
   b. If both types appear, split the argument so each embedded quote is surrounded by the
      opposite quote type. Examples:
      ```
      "This has a single quote ' embedded"
      'This has a double quote " embedded'
      'This has a double quote " and a single quote '"' embedded"
      ```
4. **Continuation lines** end with a backslash + newline. In a *quoted* argument the next line is
   appended without stripping whitespace; for *non-quoted* arguments, leading whitespace on the
   next line is stripped. Examples:
   ```
   "This is a continuation \
   line"                              -> "This is a continuation line"

   control(file1(mode,type),\
   file2(type))                       -> control(file1(mode,type),file2(type))
   ```
5. Command files may be **nested up to 25 levels**.

**`-l x`** — Search also in system library `libx.a` (`x` is a string). The linker first searches in
directories given with `-Ldirectory`, then in the standard directories named by `C88LIB` (unless
`-L` was used with no directory). **Position dependent** — see §3.3.2.

**`-o filename`** — Use `filename` as the output filename. If omitted, the default is `a.out`.

**`-r`** — No report is made for unresolved symbols. Use with incremental linking. (Also disables
overlaying.)

**`-u symbol`** — Enter `symbol` as undefined in the symbol table. Useful for forcing extraction
from a library.

**`-v` or `-t`** — Verbose; print the name of each file as it is processed.

**`-w n`** — Set a warning level between **0 and 9** inclusive. All warnings with a level **above**
`n` are suppressed. A message's level is printed in its last column. **Default warning level is 8.**

---

## 3.3 Libraries

Two kinds of libraries exist:

- **User library** — your own archive of object modules, specified as an ordinary filename. The
  linker does **not** apply any search path to it. The file must have extension `.a`.
  ```
  lk88 start.obj -fobj.lnk mylib.a
  lk88 start.obj -fobj.lnk libs\mylib.a       # library in a subdirectory
  ```
- **System library** — specified with the `-l` option. `-lcs` selects the system library `libcs.a`.

### 3.3.1 Library search path

System library files are located by the following algorithm:

1. The directories given with `-Ldirectory` options, **left-to-right**. Example:
   ```
   lk88 -L..\lib -L\usr\local\lib start.obj -fobj.lnk -lcs
   ```
2. If `-L` was **not** used without a directory, and the environment variable **`C88LIB`** exists,
   use its contents as directory specifier(s). `C88LIB` may list more than one directory, separated
   by a directory separator. *(The list of "valid directory separators" appears as an uncaptured
   figure/list in the source — separators include `;` as shown in the example below.)*
   ```
   set C88LIB=..\lib;\usr\local\lib
   lk88 start.obj -fobj.lnk -lcs
   ```
3. The `lib` directory **relative to the installation directory of `lk88`**. The linker determines at
   run time which directory the binary was executed from. Example: with `lk88.exe` installed in
   `C:\C88\BIN`, the directory searched is `C:\C88\LIB`.
4. If still not found, the **processor- and model-specific subdirectory** of that `lib` directory,
   e.g. `C:\C88\LIB\S1C88s`.

Model-specific library subdirectories:

| Directory | Application built in |
|-----------|----------------------|
| `S1C88s`  | small model |
| `S1C88d`  | compact data model |
| `S1C88c`  | compact code model |
| `S1C88l`  | large model |

(For memory models see Chapter 1 "C Compiler" and §2.7.3.5 "MODEL".) A directory given via
`-Ldirectory` or `C88LIB` may or may not end with a directory separator — `lk88` inserts it if
omitted.

### 3.3.2 Linking with libraries

When linking from libraries, **only the objects you need are extracted**. Consequences:

- `lk88 mylib.a` links nothing and produces no output file — there are no unresolved symbols when
  the linker scans `mylib.a`.
- Force a symbol undefined with `-u` to drive extraction:
  ```
  lk88 -u main mylib.a        # space between -u and main is optional
  ```
  `main` is searched in the library; if found, the containing object is extracted. If that module
  introduces new unresolved symbols, the linker scans `mylib.a` again, repeating until no new
  unresolved symbols remain.

**Position matters.** Given
```
lk88 -lcs myobj.obj mylib.a
```
the linker first searches `libcs.a` with no unresolved symbols (nothing extracted), then links the
user object and library — leaving all C-library symbols unresolved. The **correct** invocation is:
```
lk88 myobj.obj mylib.a -lcs
```
All symbols still unresolved after `myobj.obj` and `mylib.a` are then searched for in `libcs.a`.
The link order for objects, user libraries, and system libraries is the order they appear on the
command line. Plain objects are always linked; library modules only when needed.

### 3.3.3 Library member search algorithm

A library built with **`ar88`** carries an **index** at its beginning. To keep the index small, only
the **defined** symbols of each member are recorded there. The linker scans this index for
unresolved externals:

1. When a recorded symbol matches an unresolved external, the corresponding object is **extracted**
   and processed.
2. After processing, the remaining index is searched.
3. If a complete pass introduced new unresolved externals, the library is **scanned again**.

Use `-v` to follow the linker's library actions.

---

## 3.4 Linker Output

The linker produces an **IEEE-695 object output file** and, if requested, a map file and/or a call
graph file.

- The output object is **still relocatable**; it is the task of the **locator** to assign absolute
  addresses to the sections.
- The linker **combines sections with the same name** into one larger output section.

**Map file (`.lnl`)** — produced with `-M`. Its basename matches the output file; extension `.lnl`
(default `a.lnl`). The map is organized **per linked object**; each object is divided into
**sections**, and each section into **symbols**. It shows the **relative position of each linked
object from the start of the section** (addresses are offsets relative to the section start in the
output file). An `E` after an address marks the label as **external**.

**Call graph in the map file** — the generated call graph lists which function calls exist and the
**stack usage** of the graph. Notation:

- The value **in front of** a function name = stack usage *before entering* that function (local
  usage at the call site).
- The value **behind** a function name (in parentheses) = the *total* stack usage of that function
  *including its calls*.
- The value **below** a function = the function's own maximum stack usage.
- Numbers are sizes in **bytes** (for the S1C88).

The call graph also emits two special annotations:

- **Recursive call** detection:
  ```
  Call graph 1:
   function
    |
    +-- function1  !! RECURSIVE !!
  ```
- **Static function referenced by multiple call graphs.** Such a function is treated as a separate
  graph and is **not overlaid** with the graphs that reference it:
  ```
  Call graph 1:
   root1
    |
    +-- shared  !! NOT OVERLAYED !! (referenced by different call graphs)
  Call graph 2:
   root2
    |
    +-- shared  !! NOT OVERLAYED !! (referenced by different call graphs)
    |
    +-- sub2
  ```

**Separate call graph file (`.cal`)** — `-c` forces a separate file containing a **compressed** call
graph.

**Incremental linking** — for incremental linking, use `-r`: unresolved-symbol diagnostics are
suppressed and overlaying is not performed (see §3.5), so the output can be re-used as input. A call
graph is **always** generated.

### Sample map file (`.lnl`) — annotated

The call graph section (stack figures in parentheses):

```
Call graph(s)
=============
Call graph 1:
 _start ( 14 )
  |
  +-( 4 )- _exit ( 2 )
  |    |
  |    +-( 2 )
  |
  +-( 2 )- main ( 12 )
  |    |
  |    +-( 2 )- puts ( 10 )
  |    |    |
  |    |    +-( 2 )- fputc ( 8 )
  |    |    |    |
  |    |    |    +-( 2 )- _flsbuf ( 6 )
  |    |    |    |    |
  |    |    |    |    +-( 2 )- _iowrite ( 2 )
  |    |    |    |    |    |
  |    |    |    |    |    +-( 2 )
  |    |    |    |    |
  |    |    |    |    +-( 2 )- _write ( 4 )
  |    |    |    |    |    |
  |    |    |    |    |    +-( 2 )- _iowrite ( 2 )
  |    |    |    |    |    |    |
  |    |    |    |    |    |    +-( 2 )
  ...
Maximum stack usage: 14
```

Pool offsets (overlay pool layout — offset/size per overlaid function):

```
Pool offsets
============
Pool #1: zp_ovln  (Total of  39 bytes)
Pool: zp_ovln
            off   siz
puts()        0    6
fputc()       6    7
_flsbuf()    13   12
_write()     25   10
_iowrite()   35    4
```
*(The columns "off"/"siz" were extracted vertically; reconstructed as a table above.)*

Per-object section/symbol listing:

```
Object: cstart.obj
==================
Section:abs_65534 ( Start = 0x0 )
Section:.text ( Start = 0x0 )
0x0000001c E __exit
0x00000000 E __START

Object: hello.obj
=================
Section:.text ( Start = 0x1f )
0x0000001f E _main
Section:.string ( Start = 0x0 )

Object: _puts.obj
=================
Section:.text ( Start = 0x28 )
0x00000028 E _puts

Object: _fputc.obj
==================
Section:.text ( Start = 0x78 )
0x00000078 E _fputc

Object: _iob.obj
================
Section:.near_data ( Start = 0x0 )
0x00000000 E __iob
Section:.near_bss ( Start = 0x0 )
0x00000000 E __ungetc

Object: _flsbuf.obj
===================
Section:.text ( Start = 0x0102 )
0x00000102 E __flsbuf

Object: _iowrite.obj
====================
Section:.text ( Start = 0x0314 )
0x00000314 E __iowrite

Object: _write.obj
==================
Section:.text ( Start = 0x0318 )
0x00000318 E __write
```

Interpretation: addresses are **offsets relative to the start of the section in the output file**.
For example, section `.text` of `hello.obj` starts at offset `0x1f` from the output `.text` section,
and `main` likewise starts at `0x1f` from the start of the resulting `.text` section. The trailing
`E` indicates an external label.

### Computing total stack usage (worked example)

From this call-graph fragment:
```
+----- _write ( 4 )
     |
     +-( 2 )- _iowrite ( 2 )
     |    |
     |    +-( 2 )
     |
     +-( 2 )
```
The indentation shows `_write` calls `_iowrite`. The total stack usage of `_write` (behind its
name) is `( 4 )`. To determine total stack usage, take the **maximum** of:

1. **local usage before calling a function** (the first value) **+ total usage of that function**
   (the last value) — e.g. `+-( 2 )- _iowrite ( 2 )` → 2 + 2 = 4; and
2. **the usage of the function itself** — e.g. `+-( 2 )`.

---

## 3.5 Overlay Sections

To use memory more effectively in the **static memory model**, the compiler generates special
sections with the **overlay attribute** that the linker must overlay. **Each C function has its own
section** for local variables, temporaries, etc. The linker builds a **call graph** to find a valid
overlay of the sections of functions that do not call each other.

Example:
```c
#include <stdio.h>
void foo( int );

void main(void)
{
    int j;
    printf( "hello\n" );
    j = 2;
    foo(j);
}

void foo( int j )
{
    int i;
    i = j;
}
```
The linker detects that `foo` does not call `printf` and `printf` does not call `foo`. The compiler
generates an overlayable data section for `foo`'s local `i`, and `printf` gets its own overlayable
data section. The linker places **both overlay sections at the same memory area**, so target memory
is used more effectively.

---

## 3.6 Type Checking

### 3.6.1 Introduction

By default the compiler and assembler generate **high-level type information**. Unless you disable it
(`-g0`), each object carries type info for high-level types. The linker compares this information
and warns on conflicts. Four conflict classes:

| # | Warning | Meaning | Warning level | Suppress with |
|---|---------|---------|---------------|---------------|
| 1 | **W109** | Type not completely specified (e.g. unspecified array depth, or missing arguments in a prototype). Not reported unless `-w9`. | 9 | (default `-w8` hides it) |
| 2 | **W110** | Compatible types, different definitions (e.g. linking a `short` with an `int` — both 16-bit on S1C88, so it works but is non-portable; also differently-named structs/types). | 8 | `-w7` or less |
| 3 | **W111** | Signed/unsigned conflict (e.g. `signed int` linked with `unsigned int`). Often harmless, but the unsigned version holds a larger integer. | 6 | `-w5` or less |
| 4 | **W112** | All other type conflicts (probably more serious): return-type conflict, length conflict between built-ins (`short`/`long`), or a completely different type. | 4 | `-w3` or less |

### 3.6.2 Recursive type checking

The linker compares types **recursively**. Given object A:
```c
struct s1 { struct s2 *s2_ptr; };
struct s2 { int  count; } sample;
struct s1 foo = { &sample };
```
linked with object B where only `struct s2` differs:
```c
struct s1 { struct s2 *s2_ptr; };
struct s2 { short count; };
extern struct s1 foo;
```
**W112 (type conflict)** is generated. Although `struct s1` is identical in both, this is a real
conflict: e.g. `foo.s2_ptr->count++` produces different code in the two objects.

If a symbol has several conflicts, the linker reports only the **one with the lowest warning level**
(the most serious one).

### 3.6.3 Type checking between functions

- **K&R-style functions:** argument types and counts cannot be checked. An unspecified return type
  defaults to `int`. Prototypes are only needed when a function has a non-integer return type. At
  the default warning level the linker reports no conflict; at `-w9` it reports a "not completely
  specified" type (it cannot check the arguments). Return-type conflicts are real type conflicts at
  warning level 4.
- **ANSI style (recommended):** the linker checks the types and the number of all parameters.
  Prototypes are only needed for functions referenced before they are defined within one source, but
  it is good practice to keep a prototype header for all functions. If you do, type checking for
  functions is done by the **compiler** — though if you do not recompile all sources after changing
  the prototype file, the **linker** will report the resulting type conflict.
- You can add ANSI prototypes to K&R C to gain full function type checking: create a header with all
  prototypes and include it in each source, or have the compiler include it via `-H`:
  ```
  cc88 -c -Hproto.h *.c
  ```

### 3.6.4 Missing types

C permits pointers to unspecified objects, which the linker cannot fully check. Example:
```c
struct s1 { struct s2 *s2_ptr; };
struct s1 foo;
```
`struct s2` is unspecified. Since the linker cannot verify `struct s2` is identical across sources, a
level-9 warning is issued:
```
lk88 W102 (9) <name>: Incomplete type specification, type index = T101
```
If another source knows `struct s2` and uses `foo`, a second level-9 conflict is reported (incomplete
type vs. complete type):
```
lk88 W109 (9) <f1>: Type not completely specified for symbol <foo> in <f2>
```
The first warning reports that the linker cannot check the type (allowed in C); it is given once per
object per incomplete type. The second reports the type difference. **Both appear only with `-w9`.**

---

## 3.7 Linker Messages

There are **four kinds** of messages: **fatal**, **error**, **warning**, and **verbose**.

| Kind | Trigger | Output usable? | Exit code |
|------|---------|----------------|-----------|
| **Fatal** | Linker cannot perform its task due to severity | no | **2** |
| **Error** | Non-fatal error occurred, but output unusable | no | **1** |
| **Warning** | Potential error the linker cannot judge | yes (`.out` usable) | **0** |
| (no messages) | — | yes | **0** |
| **Verbose** | Only with `-v`; reports link progress | — | — |

Each message has a built-in **warning level**; `-wx` suppresses messages with level above `x`.

### Message layout

```
S1C88 object linker vx.y rz        SN000000-000 (C)year Tasking Software BV
lk88 W112 a.obj: Type conflict for symbol <f> in b.obj                       (4)
```
- Line 1: the linker **banner**.
- Line 2: the message — here a type conflict in `a.obj`, conflicting with the definition of `f` in
  `b.obj`. The trailing `(4)` is the **warning level**.

### Message groups (representative, per the manual)

The manual lists message **groups** rather than a full numbered catalog. Summarized:

1. **Fatal** (always level 0): write error; out of memory; illegal input object.
2. **Error** (always level 0): unresolved symbols (without incremental linking); cannot open input
   file; illegal recursive use of a non-reentrant function.
3. **Warning** (levels 1–9): type conflict between two symbols (e.g. W109–W112, see §3.6); illegal
   option (ignored); no system-library search path while a system library was requested.
4. **Verbose** (level not relevant; only with `-v`): extracting files from a library; current
   file/library name; "pass one"/"pass two"; rescanning a library for new unresolved symbols;
   cleaning up temp files; warning level.

> *Summary note:* the source presents these as bulleted group examples, not an exhaustive numbered
> message catalog; specific numbered messages referenced elsewhere in Chapter 3 are W102, W109,
> W110, W111, W112 (type-checking, §3.6). This section is summarized as such rather than copied
> message-by-message.

---

## Quick reference / cheat sheet

```
# Typical link of a C program (objects, user lib, then system C library)
lk88 cstart.obj hello.obj mylib.a -lcs -M -o hello.out

# Produce a map file (.lnl) and a separate compressed call graph (.cal)
lk88 -M -c objs.obj -o app.out

# Incremental link (overlaying off, unresolved-symbol diagnostics off)
lk88 -r part1.obj part2.obj -o partial.out
# ...final phase must NOT use -r so overlaying is applied.

# Force extraction of main() from a library
lk88 -u main mylib.a -o out.out

# Suppress signed/unsigned and below (level 6 W111 -> hidden at -w5)
lk88 -w5 *.obj -lcs
```

| Default | Value |
|---------|-------|
| Output file | `a.out` |
| Map file | `a.lnl` (with `-M`) |
| Error file | `a.elk` (with `-err`) |
| Warning level | 8 |
| Case sensitivity | sensitive |
| Output object format | IEEE-695 (relocatable; consumed next by the **locator**) |
```
