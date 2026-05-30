# S1C88 Locator (`lc88`) and DELFEE Description Language

> **Source:** Epson *S5U1C88000C Manual I — C Compiler / Assembler / Linker*, Chapter 4 "LOCATOR" and
> Chapter 5 "DEscriptive Language For Embedded Environments (DELFEE)".
> **PDF page range:** 176–220 (printed manual pages 164–208).
> Machine-extracted plain text; figures were not captured and are reconstructed as tables, marked
> *(figure not captured)*. Garbled/ambiguous spots are flagged inline with **[sic]** or notes.

This is the reference for anyone writing S1C88 linker-description / scatter files. The **locator** (`lc88`)
is the final tool stage: it takes the relocatable `.out` produced by the linker (`lk88`) and assigns
absolute physical addresses, respecting the S1C88 memory model, producing an absolute object image plus
optional map and error files. **DELFEE** is the steering language (`.dsc`/`.cpu`/`.mem` files) that tells
the locator how to lay out sections onto physical memory.

---

# CHAPTER 4 — LOCATOR (`lc88`)

## 4.1 Overview

The task of the locator is to locate a `.out` file (made by `lk88`) to absolute addresses. In an embedded
environment an accurate description of available memory and control over the locator's behavior is crucial
— e.g. to port applications to processors with different memory configurations, or to tune section
placement to take advantage of fast memory chips.

To perform its task the locator needs a description of the S1C88 derivative being used. It uses a special
language for this description: **DELFEE** (DEscriptive Language For Embedded Environments). The description
is held in a **description file** (see Chapter 5).

The description file is an **optional** parameter in the locator invocation. Without a description-file name
on the command line, or without the `-d` option, the locator searches for the file `s1c88.dsc` in the
current directory or in directory `etc` in the S1C88 product tree.

**Locator data flow** *(figure 4.1.1 not captured — reconstructed)*:

| Input(s)                         | Tool   | Output(s)                                   |
|----------------------------------|--------|---------------------------------------------|
| linker object files `.out`       | `lc88` | absolute object file (IEEE-695 `.abs` or Motorola S `.sre`) |
| description file `.dsc`           |        | map file `.map`                             |
|                                  |        | error file `.elc` (with `-err`)             |

## 4.2 Invocation

```
lc88 [ option ]... [ file ]...
```

- Options may appear in any order. Options start with `-`.
- Options may be combined: `-eM` is equal to `-e -M`.
- Options that require a filename or string may be separated by a space or not: `-oname` is equal to `-o name`.
- `file` may be any file with a `.out` or `.dsc` extension.

### Options Summary

| Option        | Description                                                          |
|---------------|---------------------------------------------------------------------|
| `-M`          | Produce a locate map file (`.map`)                                   |
| `-S space`    | Generate a specific space (separate output file for one space)      |
| `-V`          | Display version header only                                         |
| `-d file`     | Read description-file information from `file`; `-` means stdin      |
| `-e`          | Clean up (remove all locate products) if the result is erroneous   |
| `-err`        | Redirect error messages to an error file (`.elc`)                  |
| `-f file`     | Read command-line information from `file`; `-` means stdin         |
| `-f format`   | Specify output format (`1`=IEEE-695 default, `2`=Motorola S)       |
| `-o filename` | Specify name of the output file                                     |
| `-p`          | Make a proposal for a software part on stdout                       |
| `-v`          | Verbose: print the name of each file as it is processed             |
| `-w n`        | Suppress messages above warning level `n` (0–9, default 8)         |

> Note: `-f` is overloaded — `-f file` (command file) vs. `-f format` (output format). A command file
> name passed to `-f` **may not** be a number in the range 0–3, because those numbers are reserved to
> specify an output format.

### 4.2.1 Detailed Description of Locator Options

**`-M`** — Produce a locate map (`.map`).

**`-S space`** — Generate a specific output file for the named `space` (a space name from a `.dsc` file)
instead of generating one output file containing all spaces.

**`-V`** — Display the version header of the locator. This option must be the **only** argument of `lc88`;
other options are ignored, and the locator exits after displaying the header.

**`-d file`** — Read description-file information from `file` instead of a `.dsc` file. `-` reads from
standard input.

**`-e`** — Remove all locate products (temporary files, resulting output file, map file) if an error
occurred.

**`-err`** — Redirect error messages to an error file with extension `.elc`.

**`-f file`** — Read command-line information from `file` (`-` = stdin; you must provide the EOF code to
close stdin). `file` may not be a number 0–3 (reserved for output format). Command files get around limits
on command-line size; they may be generated on the fly (e.g. by `make`). More than one `-f` option is
allowed. Command-file format rules:

1. Multiple arguments per line are allowed.
2. To include whitespace in an argument, surround it with single or double quotes.
3. Embedded quotes inside a quoted argument:
   - If only one quote type is embedded, surround the argument with the opposite quote type.
   - If both types appear, split the argument so each embedded quote is surrounded by the opposite type.

   Examples:
   ```
   "This has a single quote ' embedded"
   'This has a double quote " embedded'
   'This has a double quote " and a single quote '"' embedded"
   ```
4. Continuation lines end with a backslash + newline. In a quoted argument, continuation lines are
   appended without stripping whitespace on the next line; for non-quoted arguments, all whitespace on
   the next line is stripped.

   Examples:
   ```
   "This is a continuation \
   line"
   → "This is a continuation line"

   control(file1(mode,type),\
   file2(type))
   → control(file1(mode,type),file2(type))
   ```
5. Command files may be nested up to 25 levels.

**`-f format`** — Specify output format:

| Value | Format                       |
|-------|------------------------------|
| `1`   | IEEE Std. 695 (**default**)  |
| `2`   | Motorola S records           |

IEEE-695 (`-f1`) can be used directly by the debugger; Motorola S can be used to load a PROM programmer.

**`-o filename`** — Use `filename` as the locator output filename. If omitted, the default depends on the
output format:

| Format | Default output name |
|--------|---------------------|
| `1`    | `a.abs`             |
| `2`    | `a.sre`             |

**`-p`** — Make a proposal for a software part in a description file on standard output.

**`-v`** — Verbose: print the name of each file as it is processed.

**`-w n`** — Warning level 0–9 (inclusive). All warnings with a level above `n` are suppressed. The level
of a message is printed in the last column of the message. Default warning level is 8.

## 4.3 Getting Started

The locator invocation is normally done via the control program (`cc88`), which hides the locator phase
completely. To understand options and the description file, you can invoke the locator separately.

To locate the `calc` demo, you first need relocatable `calc.out`. Generate it (copy `examples\asm` to a
working directory, with the S1C88 `bin` directory on the search path):

```
cc88 -cl -M -Ms -nolib startup.asm _copytbl.asm watchdog.asm calc.asm -o calc.out
```

`-cl` tells the control program to stop after linking and suppress locating. Then locate the relocatable
file to absolute addresses:

```
lc88 -M calc.out -ds1c88316.dsc
```

`-M` makes a map file. Default output format is IEEE-695 (`-f1`); with no `-o` name, default output
`a.abs` is generated (for `-f1` → `a.abs`, for `-f2` → `a.sre`). After this invocation the locator
generated:

- `a.abs` — the IEEE-695 output file
- `a.map` — the locate map file

To give the output a specific name use `-ofile`:

```
lc88 -M calc.out -o calc.abs -ds1c88316.dsc
```

You may need to adjust the description file to change the locating algorithm. Without a `-d` argument the
locator uses `s1c88.dsc` from the `etc` subdirectory (in the product tree). With `-d` above you select
`s1c88316.dsc`. To avoid changing the original, make a copy of `s1c88316.dsc` into your working directory.
Everything after a comment (`//`) until end of line is ignored. For example, changing:

```delfee
amode code {
    section selection=x;
    section selection=r;
    copy;
    table;
}
```

into:

```delfee
amode code {
    section .text;
    section .ptext;
    copy;
    table;
}
```

forces the location order of `.text` and `.ptext` to be fixed. Relocate to see the effect; the modified
copy in the working directory is found before the original in `etc`. To compare map files, use another
output name:

```
lc88 -M calc.out -ocalc_o.abs -ds1c88316.dsc
```

To choose between changed/unchanged descriptions, rename the working-directory `s1c88316.dsc` to e.g.
`order.dsc` and invoke:

```
lc88 -M -d order calc.out -ocalc_o.abs
```

The space between `-d` and `order` is optional. Installing `order.dsc` in `etc` lets you use `-dorder`
from any working directory.

## 4.4 Calling the Locator via the Control Program

It is recommended to call the locator via the control program `cc88`. The control program translates
certain options for the locator (e.g. `-srec` → `-f2`); other options (such as `-M`) are passed directly.
Typically the control program produces an `.abs` file directly from `.c`, `.src`, `.asm`, or `.obj` files:

```
cc88 -M -Ms -g -nolib startup.asm _copytbl.asm watchdog.asm calc.asm -o calc.abs s1c88316.dsc
```

builds an absolute demo file `calc.abs` ready to run under the debugger.

## 4.5 Locator Output

The locator produces an absolute file and, if requested, a map file and/or an error file.

- **Absolute output file** — Motorola S-record or IEEE-695 format, depending on `-f`. Default name
  `a.sre` or `a.abs` respectively.
- **Map file** (`-M`) — always the same basename as the output object file, extension `.map`. It shows
  the absolute position of each section. External symbols are listed with their absolute address, both
  sorted on address and sorted on symbol.
- **Error output file** (`-err`) — same name as the object output file, extension `.elc`. Errors that
  occur before `-err` is evaluated are printed on stderr.

## 4.6 Locator Messages

Four kinds of messages: **fatal**, **error**, **warning**, **verbose**.

| Kind    | When                                                                  | Exit code |
|---------|-----------------------------------------------------------------------|-----------|
| Fatal   | Locator cannot continue due to severity of the error                  | 2         |
| Error   | An error occurred, not fatal, but output is not usable                | 1         |
| Warning | Potential errors the locator can't judge; `.abs` is still usable      | 0         |
| (none)  | No messages reported                                                   | 0         |
| Verbose | Progress messages, only with `-v` (no effect on exit code)            | —         |

Each message has a built-in warning level; `-wx` suppresses messages with a level above `x`.

Message layout:

```
S1C88 locator vx.y rz             SN000000-127 (C)year Tasking Software BV
lc88 W112 (3) calc.out: Copy table not referenced, initial data is not copied
```

The first line is the locator banner (suppressed when invoked via the control program). The second line is
the warning; the number in parentheses after the warning number is the warning level.

## 4.7 Address Space

*(Figures 4.7.1 and 4.7.2 not captured — reconstructed from extracted labels.)*

**Fig. 4.7.1 — S1C88 physical address space mapping:** a `Space` named **`S1C88_space`** covering
`0x000000`–`0xffffff`, mapping through an `internal_bus` named **`S1C88_bus`** to the address bus.

**Fig. 4.7.2 — S1C88 virtual address space mapping:** the `Space` **`S1C88_space`** (`0x000000`–`0xffffff`)
contains several virtual addressing-mode regions:

| Region        | Address range (as extracted)              |
|---------------|--------------------------------------------|
| `code`        | up to `0x7fffff` / `0xffffff`              |
| `code_short`  | `0x0000`–`0x7fff`                          |
| `io`          | `0xf000`–`0xffff`                          |
| `data_short`  | `0x..00`–`0x..ff` *(page-relative)*        |
| `data_tiny`   | `0x..00`–`0x..ff` *(page-relative)*        |
| `data`        | `0x000000`–`0xffffff`                      |

> **[extraction note]** The exact start/end address per region in Fig. 4.7.2 is ambiguous in the extracted
> text (the page lists a flat list of hex bounds: `0xffffff 0x7fffff 0x0000 0x7fff 0xf000 0xffff 0x..ff
> 0x..00 0xffff`). The region→range pairing above is a best-effort reconstruction; consult the original PDF
> figure for precise bounds.

## 4.8 Copy Table

One of the process-initialization actions is to copy data from ROM to RAM and to initialize memory with
the CLEAR attribute. The locator generates a **copy table** per process, referenced by label `__lc_cp`.
One entry has the following layout (see `locate.h`, delivered with the C compiler):

```c
typedef struct cp_entry {
    char                 cp_actions;   /* 1 byte          */
    _huge unsigned char *cp_destin;    /* 3 byte address  */
    _huge unsigned char *cp_source;    /* 3 byte address  */
    unsigned long        cp_length;    /* 4 byte length   */
} cp_entry_t;
```

`cp_actions` is a bit-per-action field:

| Value / name        | Meaning                                                          |
|---------------------|-----------------------------------------------------------------|
| `0`                 | Reached end of the table                                        |
| `CP_COPY` (value 1) | Copy from `cp_source` to `cp_destin` over `cp_length` bytes     |
| `CP_BSS`  (value 2) | Clear memory from `cp_destin` over `cp_length` bytes            |

Table entries are generated as:
- one entry for each section with the **CLEAR** attribute,
- one entry for each section with the **INIT** attribute,
- one 'zero' entry to indicate end-of-table.

If there is nothing to do (no clears and no copies), the copy table has only one action entry, value zero.

At C level the copy table can be declared as `cpt_t _lc_cp;` and member access of entry `x` is
`_lc_cp[ x ].cp_actions;`. **If label `__lc_cp` is not used, the table is not generated.**

> **[extraction note]** Both `cpt_t` and `cp_entry_t`/`cpt_t[]` appear in the text; the declared C type at
> the access example is `cpt_t _lc_cp;`. The struct typedef is `cp_entry_t`. Treat `cpt_t` as the
> array/table type defined in `locate.h`.

## 4.9 Locator Labels

The locator assigns addresses to the following labels when they are referenced:

| Label              | Meaning                                                                           |
|--------------------|-----------------------------------------------------------------------------------|
| `__lc_cp`          | Start of copy table (generated only if this label is used)                        |
| `__lc_bs`          | Begin of stack space (keyword `stack`)                                             |
| `__lc_es`          | End of stack space; used to initialize the stack pointer                          |
| `__lc_b_name`      | Begin of section `name`                                                            |
| `__lc_e_name`      | End of section `name`                                                              |
| `__lc_u_name`      | User-defined label, defined in the description file via `label mylab;`            |
| `__lc_ub_name`     | Begin of user-defined reserved area (`label mybuffer length=100;`)                |
| `__lc_ue_name`     | End of user-defined reserved area                                                  |

### 4.9.1 Locator Labels Reference

Locator labels start with `__lc_`. They are ignored by the linker and resolved at locate time. Some are
real labels at the start/end of a section; others address locator-generated data (generated only if the
label is used). Because `__lc_` labels are treated specially in both linker and locator, **you can only use
them as references, not as definitions.**

> **Note (C level):** all locator labels start with one leading underscore — the compiler adds another
> underscore `_`. So assembly `__lc_b_text` ↔ C `_lc_b_text`.

#### `__lc_b_section`, `__lc_e_section`

```c
extern unsigned char _lc_b_section[ ];
extern unsigned char _lc_e_section[ ];
```

General labels for the start (`b`) and end (`e`) of section `section`. You can replace the dot before a
section name with an underscore `_` to access these from C. This introduces a possible name conflict: if
both `.text` and `_text` exist, the general label `__lc_b__text` is set to the start of `_text`; the
label for `.text` is then only usable at assembly level with its real name. Avoid section names with a
leading underscore. Example:

```c
printf( "Text size is 0x%x\n", _lc_e__text - _lc_b__text );
```

#### `__lc_bh`, `__lc_eh`  (heap)

```c
extern unsigned char _lc_bh[ ];
extern unsigned char _lc_eh[ ];
```

All `h` labels relate to the heap. Allocate a heap by defining it in a cluster description (DELFEE keyword
`heap`). `__lc_bh` is the begin of the heap; `__lc_eh` the end. Heap definition + `sbrk` example:

```delfee
block total_range {
    cluster ram {
        amode data {
            heap length = 200;
        }
    }
}
```

```c
extern unsigned char _lc_bh[ ];
extern unsigned char _lc_eh[ ];
static char *
sbrk( long length ) {
    if ( (lastmem + length) > _lc_eh ) {
        return (char *) -1;   /* overflow */
    }
    ...
}
```

#### `__lc_bs`, `__lc_es`  (stack)

```c
extern unsigned char _lc_bs[ ];
extern unsigned char _lc_es[ ];
```

All `s` labels relate to the stack (DELFEE keyword `stack`). `__lc_bs` is the begin of the stack, `__lc_es`
the end. Because `__lc_es` is at a higher address than `__lc_bs` and the **S1C88 stack grows toward lower
addresses**, the stack actually starts at `__lc_es` and ends at `__lc_bs`. Definition + initialization:

```delfee
block total_range {
    cluster ram {
        amode data {
            stack length = 100;
        }
    }
}
```

```asm
__START:
    LD SP,#__lc_es   ; set stack pointer to begin of stack space
```

#### `__lc_cp`  (copy table)

```c
extern char *_lc_cp;
```

The copy table is generated per process; each entry is a copy or clearing action. Entries are
automatically generated for:
- all sections with attribute `b`, cleared at startup (clearing action),
- all sections with attribute `i`, copied from ROM to RAM at startup (copy action).

Layout in §4.8; type `cpt_t` is defined in `locate.h`.

#### `__lc_u_identifier`

```c
extern int _lc_u_identifier[ ];
```

User-defined via the DELFEE keyword `label`. Define it in the DELFEE file **without** the prefix `__lc_u_`.
Reference from assembly with prefix `__lc_u_`, from C with `_lc_u_` (one leading underscore). Example:

```delfee
block total_range {
    cluster ram {
        amode data {
            label bstart;
            section text;
            label bend;
        }
    }
}
```

```c
#include <stdio.h>
extern int _lc_u_bstart[];
extern int _lc_u_bend[];
int main() {
    printf( "Size of cluster ram is %d\n",
            (long)_lc_u_bend - (long)_lc_u_bstart );
}
```

#### `__lc_ub_identifier`, `__lc_ue_identifier`

```c
extern int _lc_ub_identifier[ ];
extern int _lc_ue_identifier[ ];
```

Defined via the DELFEE keyword `reserved label=`. They mark the begin/end of a reserved area. Define
`identifier` in the DELFEE file **without** the prefix `__lc_ub_`/`__lc_ue_`. Reference from assembly with
`__lc_ub_`/`__lc_ue_`, from C with `_lc_ub_`/`_lc_ue_`. Example:

```delfee
block total_range {
    cluster ram {
        attribute w;
        amode data {
            section selection=w;
            reserved label=xvwbuffer length=0x10;
            // Start of reserved area: label __lc_ub_xvwbuffer
            // End of reserved area:   label __lc_ue_xvwbuffer
        }
    }
}
```

```c
#include <stdio.h>
extern int _lc_ub_xvwbuffer[];
extern int _lc_ue_xvwbuffer[];
int main() {
    printf( "Size of reserved area xvwbuffer is %d\n",
            (long)_lc_ue_xvwbuffer - (long)_lc_ub_xvwbuffer );
}
```

---

# CHAPTER 5 — DELFEE (DEscriptive Language For Embedded Environments)

## 5.1 Introduction

In an embedded environment an accurate description of available memory and control over the locator's
behavior is crucial — to port applications to different memory configurations, or to tune section
placement for fast memory chips. The **DELFEE** language was designed for this purpose.

## 5.2 Getting Started

### 5.2.1 Introduction

DELFEE is the description language used in the **description file**. The following sections build up the
detail and examples.

### 5.2.2 Basic Structure

DELFEE describes where code/data sections should be placed on the actual memory chips — the interface
between the **virtual world** (software) and the **physical world** (hardware).

- **Virtual world:** code/data sections described by assembly language. Sections have names, attributes
  (writable, read-only, …), and either an address in the address space or an addressing mode describing
  the range in which they may be located.
- **Physical world:** the actual processor reading instructions from memory chips. DELFEE instructs the
  locator to place sections at correct addresses, accounting for memory type (rom/ram, fast/slow),
  availability, etc. The same application can be tuned for different hardware configurations.

The interface is described in **three parts**:

1. **software part** (`*.dsc`) — virtual world; describes ordering of data/code sections. Varies per
   application; **can be empty**.
2. **cpu part** (`*.cpu`) — the interface between virtual and physical worlds. Contains the
   application-independent virtual part (address translation of addressing modes to address space) and the
   configuration-independent physical part (on-chip memory, address busses). Independent of application and
   configuration; **must always be defined.**
3. **memory part** (`*.mem`) — physical world; describes external memory. Varies per configuration;
   **can be empty** (if there is no external memory).

Top-level DELFEE syntax:

```delfee
software {
    layout {
        // ordering of sections
    }
}

cpu {
    // mapping of addressing modes to address space
    // defining address space
    // mapping of address space to actual busses
    // defining on-chip memory
}

memory {
    // description of external memory
}
```

The cpu and memory parts can be placed in separate files and included:

```delfee
cpu  filename     // include cpu part defined in file filename
mem  filename     // include memory part defined in file filename
```

## 5.3 CPU Part

### 5.3.1 Introduction

The cpu part defines the address translations from the assembler (virtual) addresses all the way to the
chips (physical). DELFEE recognizes four main levels:

1. **addressing mode(s)** — subsets of an address space; define address ranges within an address space.
2. **address space(s)** — the total range of available addresses.
3. **bus(ses)**.
4. **(on-chip) memory chips**.

Translation flows: **addressing mode → space → bus → chip.** Addressing modes and busses can be **nested**;
the space and the chip cannot.

*(Fig. 5.3.1.1 "Address translation" not captured — reconstructed)*: addressing modes 1–4 `map` into the
**space**, which `map`s onto an **internal bus** and **external bus**; busses `mem`/`map` onto an
**internal chip** and **external chip**. Addressing modes & spaces are virtual; busses & chips are physical.

Worked (fictitious) cpu part:

```delfee
cpu {
    //
    // addressing mode definitions
    //
    amode near_code {
        attribute Y1;
        mau 8;
        map src=0 size=1k dst=0 amode = far_code;
    }
    amode far_code {
        attribute Y2;
        mau 8;
        map src=0 size=32k dst=0 space = address_space;
    }
    amode near_data {
        attribute Y3;
        mau 8;
        map src=0 size=1k dst=0 amode = far_data;
    }
    amode far_data {
        attribute Y4;
        mau 8;
        map src=0 size=32k dst=32k space = address_space;
    }
    //
    // space definitions
    //
    space address_space {
        mau 8;
        map src=0   size=32k dst=0   bus = address_bus label = rom;
        map src=32k size=32k dst=32k bus = address_bus label = ram;
    }
    //
    // bus definitions
    //
    bus address_bus {
        mau 8;
        mem addr=0   chips=rom_chip;
        map src=0x100 size=0x7f00  dst=0x100 bus = external_rom_bus;
        mem addr=32k chips=ram_chip;
        map src=0x8100 size=0x7f00 dst=0x100 bus = external_ram_bus;
    }
    //
    // internal memory definitions
    //
    chips rom_chip  attr=r mau=8 size=0x100;  // internal rom
    chips ram_chip  attr=w mau=8 size=0x100;  // internal ram
}
```

### 5.3.2 Address Translation: `map` and `mem`

Two ways to describe a memory translation between a *source level* and a *destination level*:

1. **`map`** — for translations between amodes, spaces, busses (not chips).
2. **`mem`** — translation between bus and chip; a simplified case of `map`.

Generalized `map` syntax *(Fig. 5.3.2.1 not captured)*:

```delfee
map src=number size=number dst=number destination_type=destination_name optional_specifiers;
```

| Field               | Meaning                                                                                     |
|---------------------|---------------------------------------------------------------------------------------------|
| `src`               | Start address of the source level (amode→space: source = amode, destination = space)        |
| `size`              | Length of the source level                                                                   |
| `dst`               | Start address at the destination level                                                       |
| `destination_type`  | Context-dependent: `amode` (in amode), `space` (in amode), `bus` (in space or bus)          |

`optional_specifiers` (context-dependent):

| Specifier         | Context     | Meaning                                                                          |
|-------------------|-------------|----------------------------------------------------------------------------------|
| `label = name ;`  | space only  | Reference name needed for the block definition in the software part (see §5.4.5) |
| `align = number ;`| —           | Every section will be aligned at the specified value                             |
| `page = number ;` | —           | Every section should be within a given page size                                 |

Both source and destination levels have an address range expressed in **MAUs** (Minimum Addressable Units
— the minimal amount of storage, in bits, accessed using an address). The mapping describes only range and
destination; the actual transformation also depends on the MAU. If a source MAU of 8 bits maps to a
destination MAU of 16 bits, the destination size (in address range) is half the source size. The end
address relation:

```
end_address of level2 = dst + ( size * mau of level1 / mau of level2 )
```

**`mem`** is a simplified `map`: the translation length is taken from the chip size, and the destination
address is always zero. Used to map a bus to a chip:

```delfee
mem addr=number chips=name;
```

| Field   | Meaning                                          |
|---------|--------------------------------------------------|
| `addr`  | Start address location of a chip                 |
| `chips` | Name of the chip located at address `number`     |

### 5.3.3 Address Spaces

The link between virtual and physical world is the address-space description and how it maps onto the
internal busses. An address space is the complete range of addresses the instruction set can access (some
instruction sets support multiple spaces, e.g. data space and code space).

```delfee
space name {
    mau  number;
    map  src=number size=number dst=number bus=bus_name label=name;
    //   :
    // more maps
}
```

| Field   | Meaning                                                                                        |
|---------|------------------------------------------------------------------------------------------------|
| `space` | Name by which the space can be referenced                                                      |
| `mau`   | Minimum Addressable Unit (bits)                                                                 |
| `map`   | Maps a range of addresses (`src`,`size`) to a bus (`bus_name`) at offset `dst`. A space can only map onto a bus. The bus MAU may differ, changing the bus-side length. |

Usually an address in the space corresponds to the same address on the bus (`src` == `dst`). Example:

```delfee
space address_space {
    mau 8;
    map src=0   size=32k dst=0   bus=address_bus label=rom;
    map src=32k size=32k dst=32k bus=address_bus label=ram;
}
```

The amode definitions use this name (`address_space`) as their `space=` destination. Labels `rom` and
`ram` are used by block definitions in the software part (§5.4.5).

### 5.3.4 Addressing Modes

Addressing modes define address ranges in the address space (e.g. bit-addressable memory, code-only
regions, zero pages). They are defined by the instruction set.

```delfee
amode name {
    mau  number;
    attr Ynumber;
    map  src=number size=number dst=number amode|space=name;
}
```

| Field    | Meaning                                                                                                  |
|----------|---------------------------------------------------------------------------------------------------------|
| `amode`  | Name to reference the addressing mode. In the object file the addressing mode of a section is encoded with a `Ynumber`; the **name** only has meaning inside the description file, not to the sections. |
| `mau`    | Minimum Addressable Unit (bits)                                                                          |
| `attr Y` | The addressing-mode number. Every code/data section has a number specifying which addressing mode it belongs to; **this number must never be changed** or section interpretation gets mixed up. |
| `map`    | Maps the addressing mode to another addressing mode (`amode`) or to an address space (`space`)          |

Example (`near_data` → `far_data` → `address_space`):

```delfee
amode near_data {
    attribute Y3;
    mau 8;
    map src=0 size=1k dst=0 amode=far_data;
}
amode far_data {
    attribute Y4;
    mau 8;
    map src=0 size=32k dst=32k space=address_space;
}
```

*(Fig. 5.3.4.1 not captured)*: `near_data` (`0x0000`–`0x03ff`) maps into `far_data` (`0x0000`–`0x7fff`),
which maps into `Space address_space` (`0x0000`–`0xffff`).

### 5.3.5 Busses

`bus` describes the bus configuration — the address translation from address space to chip.

```delfee
bus name {
    mau number;
    map src=number size=number dst=number bus=name;   // mapping to another bus
    mem addr=number chips=name;                        // mapping to a memory chip
}
```

| Field | Meaning                                       |
|-------|-----------------------------------------------|
| `bus` | Name by which the bus can be referenced       |
| `mau` | Minimum Addressable Unit (bits)               |
| `map` | Mapping to another bus                        |
| `mem` | Mapping to a memory chip                       |

Example:

```delfee
bus address_bus {
    mau 8;
    mem addr=0   chips=rom_chip;
    map src=0x100 size=0x7f00  dst=0x100 bus=external_rom_bus;
    mem addr=32k chips=ram_chip;
    map src=0x8100 size=0x7f00 dst=0x100 bus=external_ram_bus;
}
```

*(Fig. 5.3.5.1 not captured)*: `Bus address_bus` (`0x0000`–`0xffff`) contains `rom_chip` at `0x0000`,
`external_rom_bus` mapped at `0x0100`–`0x7fff`, `ram_chip` at `0x8000` (32k), and `external_ram_bus` at
`0x8100`–`0xffff`. The internal `rom_chip` is at bus address 0; `ram_chip` at 32k. The first map translates
`0x100`–`0x7fff` of `address_bus` onto `external_rom_bus` starting at `0x100`; the second translates
`0x8100`–`0xffff` onto `external_ram_bus` starting at `0x100` (RAM, so both destination addresses are the
same).

> **[extraction note]** The text says the first mapping translates "0x100-0x7ff" — given
> `src=0x100 size=0x7f00` the actual range is `0x100`–`0x7fff`. Treat `0x7ff` as an extraction typo.

### 5.3.6 Chips

`chips` describes a memory chip.

```delfee
chips name attr=letter_code mau=number size=number;
```

| Field   | Meaning                                                  |
|---------|---------------------------------------------------------|
| `chips` | Name by which the chip can be referenced                |
| `attr`  | Attribute letter code (see below)                       |
| `mau`   | Minimum Addressable Unit (bits)                         |
| `size`  | Size of the chip (address range `0`–`size`)             |

Chip attribute `letter_code`:

| Code | Meaning                                  |
|------|------------------------------------------|
| `r`  | Read-only memory                          |
| `w`  | Writable memory                           |
| `s`  | Special memory (it must **not** be located) |

Example:

```delfee
chips rom_chip  attr=r mau=8 size=0x100;  // internal rom (256 bytes, read-only)
chips ram_chip  attr=w mau=8 size=0x100;  // internal ram (256 bytes, writable)
```

### 5.3.7 External Memory

It is possible (but **not advisory**) to map an address space directly to external memory chips in the cpu
part — DELFEE doesn't actually distinguish on-chip vs. external memory. For maintenance/flexibility, keep
the internal (static) memory part separate from the external (variable) memory part (§5.5). In the cpu part
you only define a mapping to an **external bus**, defined later in the memory part:

```delfee
bus address_bus {
    mau 8;
    mem addr=0   chips=rom_chip;
    map src=0x100 size=0x7f00  dst=0x100 bus=external_rom_bus;
    mem addr=32k chips=ram_chip;
    map src=0x8100 size=0x7f00 dst=0x100 bus=external_ram_bus;
}
```

## 5.4 Software Part

### 5.4.1 Introduction

Two main parts: **`load_mod`** and **layout description**.

```delfee
software {
    load_mod start = start_label;
    layout {
        // ordering of sections
    }
}
```

### 5.4.2 Load Module

`load_mod` defines the program start label — the start of the code, to which the reset vector should
point. The locator generates a warning if this label is not referenced.

```delfee
load_mod start = start_label;
```

### 5.4.3 Layout Description

The layout definition can be omitted — then the locator generates one based on the amode definitions in
the cpu part. But that does not let you control the order in which sections (like stack and heap) are
located. If you define the layout part, the locator uses it. Example:

```delfee
layout {
    space address_space {
        block rom {
            cluster first_code_clstr {
                attribute i;
                amode near_code;
                amode far_code;
            }
            cluster code_clstr {
                attribute r;
                amode near_code {
                    section selection=x;
                    section selection=r;
                }
                amode far_code {
                    table;
                    section selection=x;
                    section selection=r;
                    copy;             // locate rom copies here
                }
            }
        }
        block ram {
            cluster data_clstr {
                attribute w;
                amode near_data {
                    section selection=w;
                }
                amode far_data {
                    section selection=w;
                    heap;
                    stack;
                }
            }
        }
    }
}
```

```delfee
layout {
    // space definitions
}
```

**Levels inside the layout definition:**

| Level     | Constraints                                                                                    |
|-----------|------------------------------------------------------------------------------------------------|
| `space`   | Only inside `layout`. As many `space` levels as there are space definitions in the cpu part.    |
| `block`   | Only inside `space`. As many `block` levels as there are mappings defined in the space definition in the cpu part. |
| `cluster` | Only inside `block`. Multiple clusters per block; group (code/data) sections. The locator locates each cluster in the specified order. |
| `amode`   | Only inside `cluster`. Corresponds to an amode definition in the cpu part. Within an amode you specify the order in which sections are located. |

Roughly: `space`/`block` correspond to **address ranges**; `cluster`/`amode` correspond to **(groups of)
sections**.

### 5.4.4 Space Definition

§5.3.3 defined the address translation of a space in the cpu part. For every cpu-part space you must
provide a description in the layout. The layout `space` level can only contain `block` levels; its name
must match a cpu-part space definition.

```delfee
space name {
    // block definitions
}
```

Example:

```delfee
space address_space {
    block rom {
        ....
    }
    block ram {
        ...
    }
}
```

### 5.4.5 Block Definition

A `block` sets boundaries based on chip sizes — it references a physical area of memory; selected sections
are only allowed within its range. The physical range of a block is defined in the cpu part by a **labeled
mapping**:

```delfee
space address_space {
    mau 8;
    map src=0   size=32k dst=0   bus=address_bus label=rom;  // --> block name: rom
    map src=32k size=32k dst=32k bus=address_bus label=ram;  // --> block name: ram
}
```

The block name must match a `label` in a space's `map` definition. A block must be inside a space and can
only contain `cluster` levels.

```delfee
block name {
    // cluster definitions
}
```

Example:

```delfee
block rom {
    cluster first_code_clstr {
        ...
    }
    cluster code_clstr {
        ...
    }
}
```

### 5.4.6 Selecting Sections

To define locate order you need a handle on a section or group. DELFEE recognizes these section
characteristics:

- **name of the section** — unique to a specific section.
- **attribute(s) of a section** — set by assembler/compiler (Table 5.4.6.1). Selecting an attribute selects
  a group. Attributes can be grouped into an **attribute string**, e.g. `by1w`.
- **addressing mode** — every section has one (as defined in the cpu part).

**Table 5.4.6.1 — Section attributes**

| attr    | Meaning           | Description                                                            |
|---------|-------------------|------------------------------------------------------------------------|
| `W`     | Writable          | Must be located in RAM                                                  |
| `R`     | Read only         | Can be located in ROM                                                   |
| `X`     | Execute only      | Can be located in ROM                                                   |
| `Z`     | Zero page         | Must be located in the zero page                                        |
| `Ynum`  | Addressing mode   | Must be located in addressing mode `num`                               |
| `A`     | Absolute          | Already located by the assembler                                        |
| `B`     | Blank             | Section must be initialized to `0` (cleared)                           |
| `F`     | Not filled        | Section is not filled or cleared (scratch)                              |
| `I`     | Initialize        | Section must be initialized in ROM                                      |
| `N`     | Now               | Section is located before normal sections (without `N` or `P`)         |
| `P`     | Postponed         | Section is located after normal sections (without `N` or `P`)          |

**Section-selection syntax:**

```delfee
// 1. select a group on section attribute
section selection = attr;

// 2. select a section by name
section name;

// 3. select a special section
heap;          // locate heap here
stack;         // locate stack here
table;         // locate copy table here
copy;          // locate all initial data here
copy name;     // locate initial data of the named section here

// 4. create a section
reserved label = name length = number;
```

**Excluding by attribute:** place a `-` (minus) in front of an attribute. So
`section selection=attr1-attr2` selects sections with `attr1` and **without** `attr2`.

### 5.4.7 Cluster Definition

Clusters group sections; the locator handles clusters in the order specified, giving grouped sections a
higher locate priority. Three main ways a section is assigned to a cluster (full rules in §5.4.10):

1. attribute
2. `section selection=`
3. amode definition

Example (an extra `first_code_clstr` gives `i` sections higher priority than `code_clstr`):

```delfee
layout {
    space address_space {
        block rom {
            cluster first_code_clstr {
                attribute i;
                amode near_code;
                amode far_code;
            }
            cluster code_clstr {
                attribute r;
                amode near_code {
                    section selection=x;
                    section selection=r;
                }
                amode far_code {
                    table;
                    section selection=x;
                    section selection=r;
                    copy;          // locate rom copies here
                }
            }
        }
    }
}
```

```delfee
cluster name {
    // section selections
}
```

Within a cluster, the sections with the **least freedom** (fewest possible addresses) are located first.

### 5.4.8 Amode Definition

Within a cluster, an amode identifies a group of sections (even though the cpu part assigned an address
range to it). Order of locating is determined by the order of specification.

```delfee
amode name {
    section selection = attr;
    :
}
```

Example — locate writable sections first, then heap, then stack:

```delfee
section selection = w;   // 'w' means writable sections
heap;
stack;
```

### 5.4.9 Manipulating Sections in Amodes

An amode definition can contain these keywords:

| Keyword     | Description                                                                  |
|-------------|------------------------------------------------------------------------------|
| `section`   | Selects a section, or group of sections                                      |
| `selection` | Specifies attributes for grouping sections                                   |
| `attribute` | Assigns attributes (passed to the cluster)                                   |
| `copy`      | Selects a ROM copy of a section by name, or all ROM copies in general        |
| `fixed`     | Forces a section to be located around a fixed address                        |
| `gap`       | Creates a gap in the address range where sections will not be located        |
| `reserved`  | Reserves a memory area, referenceable using locator labels                   |
| `heap`      | Defines the place and attributes of the heap                                 |
| `stack`     | Defines the place and attributes of the stack                                |
| `table`     | Defines the place and attributes of the copy table                           |
| `assert`    | A user-defined assertion                                                      |
| `length`    | Specifies the length of stack, heap, physical block, or reserved space       |

All keywords are described in §5.6.

### 5.4.10 Section Placing Algorithm

To find where a section is placed, DELFEE uses this algorithm:

1. Try to find a selection by **section name**.
2. If not found, search for a `section selection=` within a **matching amode block**.
3. If not found, search for a `section selection=` **not** within an amode block.
4. If not found, search for a cluster with a correct `amode= ..,..,.. ;` and correct attributes.
5. If not found, search for a cluster with correct attributes.
6. If not found, **relax attribute checking** and start over.

**Relax attributes** using these rules (in order):

1. If `stack`, `heap`, or `reserved` — switch the indication off and try again.
2. If attribute `f` (not filled) — switch `f` off and try again.
3. If attribute `b` (clear) — switch `b` off and try again.
4. If attribute `i` (initialize) — switch `i` off and try again.
5. If attribute `x` (executable code) — switch `x` off and `r` (read-only) on and try again (try to place
   executable sections in read-only memory).
6. If attribute `r` (read-only) — switch `r` off and `w` (writable) on and try again (try to place
   read-only sections in writable memory).

## 5.5 Memory Part

### 5.5.1 Introduction

The memory part defines the **variable** part of the memory configuration; placing it in a separate file
lets you switch easily between configurations. Mapping syntax is the same as in the cpu part. Given the cpu
part's two references to external busses:

```delfee
bus address_bus {
    mau 8;
    mem addr=0   chips=rom_chip;
    map src=0x100 size=0x7f00  dst=0x100 bus=external_rom_bus;
    mem addr=32k chips=ram_chip;
    map src=0x8100 size=0x7f00 dst=0x100 bus=external_ram_bus;
}
```

the memory part defines those external busses (and their chips):

```delfee
memory {
    bus external_rom_bus {
        mau 8;
        mem addr=0 chips=xrom;
    }
    chips xrom attr=r mau=8 size=0x8000;

    bus external_ram_bus {
        mau 8;
        mem addr=0 chips=xram;
    }
    chips xram attr=w mau=8 size=0x8000;
}
```

## 5.6 DELFEE Keyword Reference

Alphabetical reference. Some keywords can be abbreviated to a minimum of four characters (see §5.6.1). The
"(part)" annotation indicates where each form is valid.

### `.addr`  *(Software part)*

The predefined label `.addr` contains the **current address**.

```delfee
block ram {
    cluster data_clstr {
        attribute w;
        amode near_data {
            section selection=w;
            assert ( .addr < 256, "page overflow");
            // if the condition is false, the locator
            // generates an error with the text as message
        }
        ...
    }
}
```

### `address`  *(all parts)*

```delfee
address = address          // (full)
addr    = address          // (abbreviated)
```

Specify an absolute address in memory.

```delfee
// Cpu or memory part:
bus address_bus {
    mau 8;
    mem addr=0   chips=rom_chip;
    mem addr=32k chips=ram_chip;
}
// Software part:
block rom {
    cluster code_clstr {
        attribute r;
        amode near_code {
            section selection=x;
            section selection=r;
            section .string address = 0x0100;
        }
    }
}
```

> Note: in the amode example above the locate order is fixed — sections selected by `x`/`r` are forced
> before `.string`. If a fixed order is not desired, put the absolute-address spec in a separate amode:
> ```delfee
> amode near_code { section .string address = 0x0100; }
> amode near_code { section selection=x; section selection=r; }
> ```

### `amode`

```delfee
// Cpu or memory part:
amode identifier[, identifier]... { amod_description }   // (def)
amode = identifier                                       // (ref)
// Software part:
amode identifier[, identifier]... ;
amode identifier[, identifier]... { section_blocks }
```

In the cpu/memory part, `amode` maps an addressing mode (or register bank) onto a particular address space
(definition); `amode=` maps a specific addressing mode onto a previously defined one (reference). The only
keywords allowed in an `amod_description` (cpu part) are `attribute`, `map`, and `mau`. `attribute Ynum`
uniquely identifies the addressing mode. In the software part, `amode` is used in a cluster definition to
change the locate order of sections (see §5.4.10).

```delfee
// Cpu or memory part:
cpu {
    amode near_data {
        attribute Y3;
        mau 8;
        map src=0 size=1k dst=0 amode = far_data;   // reference
    }
    amode far_data {                                 // definition
        attribute Y4;
        mau 8;
        map src=0 size=32k dst=32k space = address_space;
    }
}
// Software part:
block ram {
    cluster data_clstr {
        attribute w;
        amode near_data {
            section selection=w;
        }
        amode far_data {
            section selection=w;
            heap;
            stack;
        }
    }
}
```

### `assert`  *(Software part)*

```delfee
assert ( condition , text ) ;
asse   ( condition , text ) ;     // (abbreviated)
```

Test a condition of a virtual address. Generate an error with `text` if the assertion fails. `condition`
is one of:

```
expr1 >  expr2
expr1 <  expr2
expr1 == expr2
expr1 != expr2
```

`expr1`/`expr2` can be any expression or label. The predefined label `.addr` holds the current address.

```delfee
block ram {
    cluster data_clstr {
        attribute w;
        amode near_data {
            section selection=w;
            assert ( .addr < 256, "page overflow");
        }
        ...
    }
}
```

### `attribute`  *(Software part)*

```delfee
attribute attribute_string ;
attr      attribute_string ;     // (abbreviated)
attribute = attribute_string
attr      = attribute_string     // (abbreviated)
```

Assign attributes to sections, clusters, or memory blocks (see also `selection`). For **sections** these
are supplementary to the standard section attributes; standard attributes such as zero page (`Y1`), blank
(`B`), executable (`X`) are set by the compiler/assembler. With an action attribute after a section
(`attr=`), you can set section attributes or disable them with `-` (minus).

Attribute meanings:

| Attr     | Applies to              | Meaning                                                              |
|----------|-------------------------|---------------------------------------------------------------------|
| `num`    | section only            | Align the section at `2^num` MAUs                                    |
| `Ynum`   | amode and sections only | Identify addressing mode; sections with this attribute go in this cluster |
| `r`      | memory and clusters     | Read-only cluster or read-only memory                               |
| `w`      | memory and clusters     | Writable cluster or writable memory                                 |
| `s`      | memory only             | Special memory; must **not** be located                             |
| `x`      | clusters/sections only  | Cluster/section is executable                                       |
| `g`      | clusters/sections only  | Cluster/section is global (known in a multi-module environment)     |
| `b`      | clusters/sections only  | Cluster/section should be cleared before locating                   |
| `i`      | sections only           | Cluster/section should be copied from ROM to RAM                    |
| `f`      | clusters/sections only  | Cluster/section should not be filled and not cleared (scratch)      |

**Default attributes if the `attribute` keyword is omitted:**

| Target    | Default                                                                    |
|-----------|----------------------------------------------------------------------------|
| sections  | The attributes generated by the assembler/compiler                         |
| clusters  | Attributes from the underlying memory: `r` for rom, `w` for ram            |
| memory    | If none defined, default is writable (`w`)                                  |

```delfee
// Software part:
layout {
    space address_space {
        block rom {
            cluster first_code_clstr {
                attribute i;                  // set cluster attribute
                amode near_code;
                amode far_code;
            }
        }
        block ram {
            cluster ram {
                amode near_data {             // default cluster attr is 'w' (RAM)
                    section selection=w;
                    section selection=b attr=-b;
                    // sections with attr b located here, then 'b' switched off
                }
            }
        }
    }
}
// Cpu part:
amode near_data {
    attribute Y3;                              // identify code with Y3
    mau 8;
    map src=0 size=1k dst=0 amode = far_data;
}
chips rom_chip  attr=r mau=8 size=0x100;       // memory attributes
chips ram_chip  attr=w mau=8 size=0x100;
```

### `block`  *(Software part)*

```delfee
block identifier { block_description }
```

Defines the contents of a physical area of memory. Make a block description for each chip you use. Each
block has a symbolic name as previously defined by `chips`. Two or more chips can be combined in one block
as long as their total address range is **linear, without gaps**. The `identifier` indicates the block
starts at the specified chip, regardless of how many chips are combined.

```delfee
layout {
    space address_space {
        block ram {                  // memory block starting at chip ram_chip
            cluster ram {
                ...
            }
        }
    }
}
```

### `bus`  *(Cpu or memory part)*

```delfee
bus identifier[, identifier]... { bus_description }   // (def)
bus = identifier                                      // (ref)
```

Defines the physical memory addresses for the chips on the cpu (definition); `bus=` maps a specific address
range onto a previously defined bus (reference). The only keywords allowed in a bus description are `mem`,
`map`, `mau`.

```delfee
cpu {
    space address_space {
        mau 8;
        map src=0   size=32k dst=0   bus = address_bus label = rom;
        map src=32k size=32k dst=32k bus = address_bus label = ram;   // ref
    }
    bus address_bus {                                                  // definition
        mau 8;
        mem addr=0   chips=rom_chip;
        map src=0x100 size=0x7f00  dst=0x100 bus = external_rom_bus;
        mem addr=32k chips=ram_chip;
        map src=0x8100 size=0x7f00 dst=0x100 bus = external_ram_bus;
    }
}
```

### `chips`  *(Cpu or memory part)*

```delfee
chips identifier[, identifier]... chips_description                    // (def)
chips = identifier[| identifier]... [, identifier[| identifier]...]... // (ref)
```

Describes the chips on the cpu or target board (definition). For each chip its `size` and `mau` are
specified; `attr` sets read-only (`r`), writable (`w`), or special (`s`); default is `w`. Use `chips=`
after `mem` to specify where a chip is located (reference). Create **chip pairs** by separating chips with
a vertical bar `|`.

```delfee
cpu {
    bus address_bus {
        mau 8;
        mem addr=0 chips=rom_chip;            // ref
    }
    chips rom_chip  attr=r mau=8 size=0x100;  // def
    chips ram_chip  attr=w mau=8 size=0x100;
}
```

### `cluster`  *(Software part)*

```delfee
cluster cluster_name { cluster_description }
cluster cluster_name[, cluster_name]... ;
```

In the layout part, define the cluster name and cluster location order. Cluster attributes (see
`attribute`) can be specified in the first form; if none specified, the default `r` or `w` is set
automatically. In a cluster description you can determine the locate order of sections within the named
cluster and also specify stack/heap size, extra process memory, define labels for the process, etc.

```delfee
space address_space {
    block rom {
        cluster first_code_clstr {
            attribute i;             // default 'r' overruled to 'i'; sections with 'i' located here
            amode near_code;
            amode far_code;
        }
    }
    block ram {
        cluster data_clstr {
            attribute w;             // can be omitted; default 'w' because RAM
            amode near_data {
                section selection=w;
            }
        }
    }
}
```

### `copy`  *(Software part)*

```delfee
copy section_name [ attr = attribute ] ;
copy selection = attribute [ attr = attribute ] ;
copy ;
```

The ROM copy of data sections with attribute `i` is copied from ROM to RAM at startup. `copy` defines where
these ROM copies are placed. Specify a section by name, or select sections by attribute; with no argument
the locator places **all** ROM copies at the specified location. `attr=` changes the section attributes. If
you don't specify `copy` at all, the locator finds a suitable place for ROM copies. (See `attribute`,
`selection`.)

```delfee
space address_space {
    block rom {
        cluster code_clstr {
            attribute r;                 // cluster attribute
            amode far_code {
                table;
                section selection=x;
                section selection=r;
                copy;                    // all ROM copies are located here
            }
        }
    }
}
```

### `cpu`  *(Cpu part)*

```delfee
cpu { cpu_description }
cpu filename
```

Appears together with `software` and `memory` at the highest level. The actual cpu description is between
`{ }`. Normally you don't need to change the cpu part (delivered with the product, describing the
derivative completely). The second form is the **include** syntax: the locator opens `filename` and reads
the cpu description from it; the included file must start with `cpu` again. `filename` can contain a full
path/drive letter, or parts can come from an environment variable. The file is searched first in the
current directory, then in `etc` relative to the installation directory.

```delfee
software { ... }
cpu target.cpu        // cpu part in separate file
memory target.mem
```

### `dst`  *(Cpu or memory part)*

```delfee
dst = address
```

Destination address, part of `map` in an amode/space/bus description. `address` can be decimal,
hexadecimal, or octal; the Delfee suffix `k` = kilo (2^10) and `M` = mega (2^20) are allowed. Unit of
measure depends on the destination memory space's MAU.

```delfee
cpu {
    amode near_code {
        attribute Y1;
        mau 8;                                  // 8-bit addressable
        map src=0 size=1k dst=0 amode=far_code;
    }
}
```

### `fixed`  *(Software part)*

```delfee
fixed address = address ;
fixed addr    = address ;     // (abbreviated)
```

Define a fixed point in the memory map. The locator allocates the section/cluster **preceding** the fixed
definition and the section/cluster **following** it as close as possible to the fixed point.

```delfee
block ram {
    cluster near_data_clstr {
        amode near_data {
            section selection=w;
            fixed addr = 0x2000;
        }
    }
    cluster far_data_clstr;
}
```

`far_data_clstr` is located with its upper bound at `0x2000`, and `near_data_clstr` starts at this address.
The same can apply to sections.

### `gap`  *(Software part)*

```delfee
gap;
gap length = value ;
```

Reserve a gap with a **dynamic** size — the locator tries to make the gap as big as possible. Use in a
block description (gap between clusters) or in a cluster description (gap between sections). Can be combined
with `fixed`. The second form specifies a **fixed** length and can only occur in a block description.

```delfee
space address_space {
    block ram {
        cluster data_clstr {
            attr w;
            amode near_data;
        }                 // low side mapping
        gap;              // balloon
        cluster stck;     // high side mapping
    }
}
```

### `heap`  *(Software part)*

```delfee
heap heap_description ;
heap ;
```

Like `table` and `stack`, `heap` is a special section — not from the `.out` file, but generated at locate
time. Use `length` within the heap description to control the size. Use `heap` to include dynamic memory
for a process; it can only be used if `malloc()` has been implemented. Two locator labels mark begin/end:
`__lc_bh` (begin) and `__lc_eh` (end). **A heap is only allocated when its section labels are used in the
program.** The heap description can be a length spec and/or an attribute spec.

```delfee
layout {
    space address_space {
        block ram {
            cluster data_clstr {
                amode far_data {
                    section selection=w;
                    heap length=100;          // heap of 100 MAUs
                }
            }
        }
    }
}
```

### `label`

```delfee
label identifier ;        // (Software part) — stand-alone
label = identifier ;      // (All parts) — as part of another keyword
```

The first form (stand-alone) specifies a virtual address by label `__lc_u_identifier`. (At C level all
locator labels start with one underscore — the compiler adds another.) The second form is used as part of
another keyword: as part of `reserved` it assigns a label to an address range (start `__lc_ub_identifier`,
end `__lc_ue_identifier`); as part of `map` it assigns a name to a block of memory in a space definition.

```delfee
// Software part:
block ram {
    cluster data_clstr {
        attribute w;
        amode far_data {
            section selection=w;
            heap;
            stack;
            reserved label=xvwbuffer length=0x10;
            // Start: __lc_ub_xvwbuffer ; End: __lc_ue_xvwbuffer
        }
    }
}
// Cpu part:
space address_space {
    mau 8;
    map src=0   size=32k dst=0   bus = address_bus label=rom;
    map src=32k size=32k dst=32k bus = address_bus label=ram;
}
```

### `layout`  *(Software part)*

```delfee
layout { layout_description }
layout filename
```

Describes the layout of sections in memory. Groups sections into clusters; you define name, number, and
order of clusters, and how clusters are allocated into physical RAM/ROM blocks. The space and block names
used must exist in the memory part or cpu part. Cluster definitions can contain fixed addresses and gap
definitions.

```delfee
software {
    layout {
        space address_space {
            block rom {
                cluster first_code_clstr {
                    attribute i;
                    amode near_code;
                }
                ....
            }
        }
    }
}
```

### `length`  *(Cpu, memory and software part)*

```delfee
length = length
leng   = length          // (abbreviated)
```

Defines the length in MAUs of a memory area. Numeric value in hex (`0x...`), octal (`0...`), or decimal;
suffix `k` (kilo) or `M` (mega) allowed. Used for reserved memory, stack/heap/gap length (see `reserved`,
`stack`, `heap`, `gap`).

```delfee
space address_space {
    block ram {
        cluster data_clstr {
            amode far_data {
                stack leng = 2k;
            }
        }
    }
}
```

### `load_mod`  *(Software part)*

```delfee
load_mod identifier start = label;
load_mod start = label;
```

Introduces a load-module description. The optional `identifier` represents a load-module name (with or
without `.out`). The load module itself must be supplied to the locator as an invocation parameter; if the
identifier is omitted, the load module is taken from the command line.

```delfee
software {
    load_mod start = __START;
}
// or
software {
    load_mod hello start = __USER_start;
}
```

### `map`  *(Cpu or memory part)*

```delfee
map map_description
```

Map a memory part (source address + size) to a destination address of an amode, space, or bus. Unit of
measure depends on the MAU of the memory space.

```delfee
cpu {
    amode far_data {
        attribute Y4;
        mau 8;
        map src=0 size=32k dst=32k space=address_space;
    }
    space address_space {
        mau 8;
        map src=0   size=32k dst=0   bus = address_bus label=rom;
        map src=32k size=32k dst=32k bus = address_bus label=ram;
    }
    bus address_bus {
        mau 8;
        mem addr=0   chips=rom_chip;
        map src=0x100 size=0x7f00  dst=0x100 bus=external_rom_bus;
        mem addr=32k chips=ram_chip;
        map src=0x8100 size=0x7f00 dst=0x100 bus=external_ram_bus;
    }
}
```

### `mau`  *(Cpu or memory part)*

```delfee
mau number ;
mau = number
```

Specify the minimum addressable unit in **bits**. The first form is used in an amode/space/bus description;
the second specifies the MAU of a chip. `mau` affects the unit of measure for other keywords. Default is 8
(byte addressable).

```delfee
cpu {
    amode near_code {
        attribute Y1;
        mau 8;                                  // byte addressable
        map src=0 size=1k dst=0 amode=far_code; // src@0, size 1k byte units, dst@0
    }
}
```

### `mem`  *(Cpu or memory part)*

```delfee
mem mem_description ;
```

Define the start address of a chip in memory. The only keywords allowed in a mem description are `address`
and `chips`.

```delfee
cpu {
    bus internal_bus {
        mau 8;
        mem addr=0    chips=rom_chip;    // rom_chip at address 0
        mem addr=32k  chips=ram_chip;    // ram_chip at address 0x8000
    }
    chips rom_chip  attr=r mau=8 size=0x100;
    chips ram_chip  attr=w mau=8 size=0x100;
}
```

### `memory`  *(Memory part)*

```delfee
memory { memory_description }
memory filename
```

Together with `software` and `cpu`, introduces a main part. Describe the memory part between `{ }`. Use it
to describe additional memory or addresses of peripherals not integrated on the cpu. Second form = include
syntax (file must start with `memory` again; search order: current dir, then `etc`).

```delfee
software { ... }
cpu target.cpu
memory target.mem        // mem part in separate file
```

### `regsfr`  *(Cpu or memory part)*

```delfee
regsfr filename
```

Specify a register file generated by the register manager for use by the debugger.

```delfee
cpu {
    regsfr regfile.dat
    /* Use file regfile.dat generated by register manager */
}
```

### `reserved`  *(Software part)*

```delfee
reserved reserved_description ;
reserved ;
```

Reserve a fixed amount of memory, or as much as possible. If no `length` is specified, the size depends on
the memory-space size, or is limited by a following fixed-point definition. Only `address`, `attribute`,
`label`, and `length` are allowed in the reserved description. Use `reserved` in an amode description.

```delfee
space address_space {
    block rom {
        cluster code_clstr {
            amode near_code {
                // system reserved (exception vector)
                reserved length=0x2 addr=0x24;
            }
        }
    }
}
```

### `section`  *(Software part)*

```delfee
section identifier [addr = address] [attr = attribute] ;
section selection = attribute [addr = address] [attr = attribute] ;
```

Specify the location order within a cluster (see `layout`). `identifier` is the section name. `addr=` makes
a section absolute. `attr=` assigns new attributes or disables attributes. (See `address`, `attribute`,
`selection`.)

```delfee
space address_space {
    block ram {
        cluster data_clstr {
            amode near_data {
                section .data attr=w;          // locate .data here, set attr 'w'
                section selection=b attr=-b;
            }
        }
    }
}
```

### `selection`

```delfee
selection = attribute
```

Use after `section` or `copy` to select all sections with the specified attribute(s). If more attributes
are specified, only sections with **all** attributes are selected. A `-` before an attribute selects
sections **not** having that attribute. (See `attribute`, `copy`, `section`.)

```delfee
space address_space {
    block ram {
        cluster data_clstr {
            amode near_data {
                // select sections with w on and not i
                // (writable sections not copied from ROM)
                section selection=-iw;
            }
        }
    }
}
```

### `size`  *(Cpu or memory part)*

```delfee
size = size
```

Define the size in MAUs of a memory area. Numeric: hex (`0x...`), octal (`0...`), decimal; suffix `k`/`M`
allowed. Use to specify the size of a memory part mapped on another, or a chip size (see `map`, `chips`).

```delfee
cpu {
    amode near_code {
        attribute Y1;
        map src=0 size=1k dst=0 amode=far_code;
    }
    space address_space {
        mau 8;
        map src=0   size=32k dst=0   bus=address_bus label=rom;
        map src=32k size=32k dst=32k bus=address_bus label=ram;
    }
    chips rom_chip attr=r mau=8 size=0x100;     // size of chips
    chips ram_chip attr=w mau=8 size=0x100;
}
```

### `software`  *(Software part)*

```delfee
software { software_description }
software filename
```

Appears at the highest level. The actual software description is between `{ }`. Second form = include
syntax (file's first keyword must be `software` again; search order: current dir, then `etc`).

```delfee
software $(MY_OWN_DESCRIPTION)
cpu target.cpu
memory target.mem
```

where the environment variable `MY_OWN_DESCRIPTION` names a file like:

```delfee
software {
    load_mod start = __START;
    layout {
        .
    }
}
```

### `space`

```delfee
space identifier { space_description }                       // (Software part)
space identifier[, identifier]... { space_description }      // (Cpu or memory part)
space = identifier
```

In the cpu/memory part, `space` describes a physical memory address space (only `mau` and `map` allowed in
its description). In the software part, `space` describes one or more memory blocks; each space has a
symbolic name previously defined by `space` in the cpu/memory part.

```delfee
// Cpu part:
cpu {
    amode far_data {
        attribute Y4;
        mau 8;
        map src=0 size=32k dst=32k space=address_space;
    }
    space address_space {
        mau 8;
        map src=0   size=32k dst=0   bus=address_bus label=rom;
        map src=32k size=32k dst=32k bus=address_bus label=ram;
    }
}
// Software part:
layout {
    space address_space {
        block rom {                  // memory block starting at chip rom_chip
            cluster code_clstr {
                ....
            }
        }
    }
}
```

### `src`  *(Cpu or memory part)*

```delfee
src = address
```

Source address, part of `map` in an amode/space/bus description. `address` can be decimal, hex, octal;
suffix `k`/`M` allowed. Specified in the addressing mode's local MAU size (default 8 bits).

```delfee
cpu {
    amode near_code {
        attribute Y1;
        mau 8;                                  // 8-bit addressable
        map src=0 size=1k dst=0 amode=far_code;
    }
}
```

### `stack`  *(Software part)*

```delfee
stack stack_description ;
stack ;
```

A special section, allocated at locate time. The locator only allocates a stack if one is needed. Two
locator labels: begin `__lc_bs`, end `__lc_es`. **If the stack grows downwards, begin must be the highest
address** — keep length positive and set the stack pointer to `end_of_stack`, so:

```
end_of_stack = begin_of_stack + length
```

is always true. Only `attribute` and `length` are allowed in the stack description. With `stack` and no
description, the locator makes the stack as big as possible. If you don't specify `stack` at all, the
locator also tries to make it as big as possible but **at least 100 MAUs**.

```delfee
space address_space {
    block ram {
        cluster data_clstr {
            amode far_data {
                section selection=w;
                stack leng=150;           // stack of 150 MAUs
            }
        }
    }
}
```

### `start`  *(Software part)*

```delfee
start = label ;
```

Define a start label for a process. Use `start` only within a load-module description.

```delfee
software {
    load_mod start = system_start;
    layout {
        .
    }
}
```

### `table`  *(Software part)*

```delfee
table attr = attribute ;
table ;
```

A special kind of section: the locator generates a **copy table**. Normally placed in read-only memory; use
`table` to steer its location. Only `attribute` is allowed (the length is calculated at locate time).
`table` can occur in a cluster description.

```delfee
space address_space {
    block rom {
        cluster code_clstr {
            attribute r;                  // cluster attribute
            amode far_code {
                table;                    // locate copy table here
                section selection=x;
                section selection=r;
                copy;                     // all ROM copies are located here
            }
        }
    }
}
```

### 5.6.1 Abbreviation of DELFEE Keywords

These keywords can be abbreviated to unique 4-character words:

**Table 5.6.1.1 — Abbreviation of DELFEE keywords**

| Keyword     | Abbreviation |
|-------------|--------------|
| `address`   | `addr`       |
| `assert`    | `asse`       |
| `attribute` | `attr`       |
| `length`    | `leng`       |

### 5.6.2 DELFEE Keywords Summary

**Table 5.6.2.1 — Overview of DELFEE keywords**

| Keyword     | Description                                                                 |
|-------------|-----------------------------------------------------------------------------|
| `address`   | Specify absolute memory address                                             |
| `amode`     | Specify the addressing modes                                                |
| `assert`    | Error if assertion failed                                                   |
| `attribute` | Assign attributes to clusters, sections, stack or heap                      |
| `block`     | Define physical memory area                                                 |
| `bus`       | Specify address bus                                                         |
| `chips`     | Specify cpu chips                                                           |
| `cluster`   | Specify the order and placement of clusters                                 |
| `copy`      | Define placement of ROM-copies of data sections                             |
| `cpu`       | Define cpu part                                                             |
| `dst`       | Destination address                                                         |
| `fixed`     | Define fixed point in memory map                                            |
| `gap`       | Reserve dynamic memory gap                                                  |
| `heap`      | Define heap                                                                 |
| `label`     | Define virtual address label                                               |
| `layout`    | Start of the layout description                                             |
| `length`    | Length of stack, heap, physical block or reserved space                    |
| `load_mod`  | Define load module (process)                                               |
| `map`       | Map a source address on a destination address                              |
| `mau`       | Define minimum addressable unit (in bits)                                   |
| `mem`       | Define physical start address of a chip                                     |
| `memory`    | Define memory part                                                         |
| `regsfr`    | Specify register file for use by debugger                                   |
| `reserved`  | Reserve memory                                                             |
| `section`   | Define how a section must be located                                        |
| `selection` | Specify attributes for grouping sections into clusters                      |
| `size`      | Size of address space or memory                                            |
| `software`  | Define the software part                                                    |
| `space`     | Define an addressing space or specify memory blocks                         |
| `src`       | Source address                                                             |
| `stack`     | Define a stack section                                                      |
| `start`     | Give an alternative start label                                            |
| `table`     | Define a table section                                                      |

---

## Quick-reference notes for tooling authors

- **Pipeline:** `lk88` (linker, `.out`) → `lc88` (locator, `.abs`/`.sre` + `.map` + `.elc`). The locator is
  driven by a DELFEE `.dsc` (which may `cpu`-/`mem`-include separate `.cpu`/`.mem` files).
- **Three DELFEE parts:** `software{}` (ordering, can be empty) + `cpu{}` (address translation, mandatory)
  + `memory{}` (external memory, can be empty).
- **Translation chain:** addressing mode → space → bus → chip. `map` connects amode/space/bus; `mem`
  connects bus → chip (length from chip size, dst always 0).
- **Layout nesting:** `layout { space { block { cluster { amode { section … } } } } }`. `space`/`block` are
  address ranges; `cluster`/`amode` are section groups. Block names match a `label=` on a space's `map`.
- **Section attributes** (`W R X Z Ynum A B F I N P`) drive selection (`section selection=…`, `-` to
  exclude) and the placing/relaxation algorithm (§5.4.10).
- **Special sections** generated at locate time: `stack`, `heap`, `table` (copy table). Locator labels:
  `__lc_bs/__lc_es` (stack, grows down — init SP to `__lc_es`), `__lc_bh/__lc_eh` (heap),
  `__lc_cp` (copy table — only generated if referenced), `__lc_b_/__lc_e_<sec>`, `__lc_u_<id>`,
  `__lc_ub_/__lc_ue_<id>` (reserved). C versions drop one leading underscore.
- **Copy table** layout (`locate.h`): `cp_entry_t { char cp_actions; addr cp_destin; addr cp_source;
  unsigned long cp_length; }`; actions `0`=end, `CP_COPY`=1, `CP_BSS`=2.

> **Extraction caveats summary:** Fig. 4.7.2 virtual-space address bounds, Fig. 5.3.5.1 bus-mapping bounds,
> and the `0x7ff`/`0x7fff` typo in §5.3.5 are reconstructed/flagged above — verify against the original PDF
> for exact numbers. Figures 4.1.1, 5.3.1.1, 5.3.2.1, 5.3.4.1 were reconstructed as tables/prose.
