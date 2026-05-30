# S1C88 C Compiler (`c88`) — Reference

> **Source:** *S5U1C88000C Manual I — C Compiler/Assembler/Linker* (Integrated Tool Package for the
> S1C88 Family), **Chapter 1 "C Compiler"** (printed pages 1–84).
> **PDF page range distilled:** 13–96 of `id000920.pdf`.
> Vendor: SEIKO EPSON; compiler toolchain by TASKING, Inc. This is the **TASKING-based S1C88 C
> cross-compiler**, distinct from the SDCC retarget effort in this repo.
>
> This document is **distilled from machine-extracted plain text**, one file per PDF page
> (`docs/_extract/id000920/page-NNN.txt`). Figures and original table grids are lost in extraction;
> they have been **reconstructed as markdown tables** where the data was recoverable. Spots marked
> `*(figure not captured)*` or flagged inline are reconstructions or known extraction artifacts.
>
> **Why this matters for the SDCC retarget:** §1.2.5, §1.2.15, §1.3.2, §1.3.3, §1.3.4, §1.3.6 give the
> EPSON/TASKING ABI (parameter-passing registers, return registers, section names, stack frame layout,
> interrupt code generation). These are the highest-value sections for defining an S1C88 calling
> convention. Note the SDCC port is **not** required to match this ABI, but this is the de-facto
> vendor reference.

---

## Toolchain components and shorthand

| Tool | Shorthand | Role | Input → Output |
|------|-----------|------|----------------|
| C cross-compiler | `c88` | C → S1C88 assembly | `.c` → `.src` |
| Cross-assembler | `as88` | assembly → object | `.src`/`.asm` → `.obj` |
| Linker (incremental) | `lk88` | link objects + libs | `.obj`/`.a` → `.out` (+ `.lnl` map) |
| Locator | `lc88` | locate to absolute | `.out`/`.dsc` → `.abs` (IEEE-695) or `.sre` (S-record) (+ `.map`) |
| Control program | `cc88` | drives the whole chain | recognizes file extensions |
| Library maintainer | `ar88` | build `.a` libraries | |
| Program builder | `mk88` | make utility | reads `makefile` |
| Object reader | `pr88` | inspect objects | |

`c88` integrates preprocessor + C frontend + S1C88 backend into one program (no intermediate files
between phases). It is a one-pass compiler: a whole C function is read into an intermediate code tree,
then the backend runs (allowing global optimization). Output assembly uses the `as88` assembly
language and contains **no macros**.

File-extension dispatch by `cc88`:

| Extension | Treated as | Passed to |
|-----------|-----------|-----------|
| `.c` | C source | compiler |
| `.asm` | assembly (preprocessed first) | assembler |
| `.src` | compiled assembly (no preprocess) | assembler |
| `.a` | library | linker |
| `.obj` | object | linker |
| `.out` | linked object (only one allowed) | locator |
| `.dsc` | locator description file | locator (triggers locate phase) |
| anything else | object | linker |

Stop-phase controls: `-cs` (stop after compile, keep `.src`), `-c` (stop after assemble, `.obj`),
`-cl` (stop after link, `.out`). Default output format is IEEE-695; `-srec`/`-ieee` select it.
`-tmp` retains intermediate files (created in the current directory).

---

## 1.1 Overview

### 1.1.1 Introduction

`c88` accepts ANSI C and translates to S1C88 assembly for the S1C88 operating in **'native' mode**.
Language extensions improve code performance and expose S1C88 architectural features at the C level.
ANSI C compatible (`__STDC__` is defined as `0` — a non-conforming implementation). The compiler
processes one C function at a time; optimizations happen both during intermediate-code construction and
after a complete function is processed ("global optimizations").

### 1.1.2 General Implementation

#### 1.1.2.1 Compiler Phases

**Frontend:**
- **Preprocessor** — file inclusion + macro substitution (ANSI X3.159-1989).
- **Scanner** — preprocessor output → token stream.
- **Parser** — C grammar, syntactic/semantic analysis → intermediate representation.
- **Frontend optimization** — target-independent transforms on intermediate code.

**Backend:**
- **Backend optimization** — target-specific transforms, register allocation, addressing-mode selection.
- **Code generator** — intermediate code → internal instruction code (S1C88 instructions).
- **Peephole optimizer / pipeline scheduler** — pattern-matching peephole opts; reorders/combines
  instructions to minimize count; finally emits `as88` assembly text.

All phases are combined into one program; no intermediate files between phases; the backend starts only
after a complete C function is processed; single pass over the input file.

#### 1.1.2.2 Frontend Optimizations (target-independent)

Controlled by `-O` and `#pragma optimize`. Some opts (e.g. constant folding) are always done.

- **Constant folding** — constant-only expressions replaced by result.
- **Expression rearrangement** — rearranged to allow more folding, e.g. `1+(x-3)` → `x+(1-3)`.
- **Expression simplification** — remove `*0`, `*1`, `+0`, `-0`.
- **Logical expression optimization** — `&&`, `||`, `!` → conditional jumps.
- **Loop rotation** — for/while loop condition evaluated at top then bottom (speed, not size).
- **Switch optimization** — delete redundant case labels, or the switch itself.
- **Control flow optimization** — reverse jump conditions, move code to minimize jumps.
- **Jump chaining** — jump to a label immediately followed by an unconditional jump → jump to final dest.
- **Remove useless jumps** — drop unconditional jump to following label; conditional becomes condition eval.
- **Conditional jump reversal** — conditional jump over unconditional jump → one reversed conditional jump.
- **Cross jumping / branch tail merging** — merge identical code sequences in two paths.
- **Constant/copy propagation** — reference to a variable with known contents replaced by those contents.
- **Common subexpression elimination (CSE)** — repeated subexpressions saved and reused.
- **Dead code elimination** — unreachable code removed (warning emitted).
- **Loop optimization** — move invariant expressions out of loops; strength-reduce index expressions.
- **Loop unrolling** — replace short loops with copies.

#### 1.1.2.3 Backend Optimizations (target-dependent)

- **Allocation graph** — per-function graph of allocation units (variables, params, temporaries, CSEs);
  drives register allocation and code generation.
- **Peephole optimizations** — replace instruction sequences with faster/shorter equivalents.
- **Leaf function handling** — leaf functions (no further calls) get special stack-frame treatment.
- **Dead store elimination** — expressions whose result is never used are removed.
- **Tail recursion elimination** — recursion replaced by a branch to the start of the function.

### 1.1.3 Compiler Structure / Development Flow

*(figure 1.1.3.1 not captured — reconstructed below)*

```
 .c ──> [c88: C preprocessor & compiler] ──> .src (assembly)  (+ .lst list)
 .src ─> [as88: assembler] ─────────────> .obj (relocatable object)
 .obj + .a ─> [lk88: linker] ───────────> .out (+ .lnl link map)
 .out + .dsc ─> [lc88: locator] ─────────> .abs (IEEE-695)  or  .sre (S-record)  (+ .map)
 .abs ──> [debugger S5U1C88000H5]
Other tools: cc88 (control program), mk88 (program builder), ar88 (library maintainer), pr88 (object reader)
```

Compiler output options: default suffix `.src`; `-o` redirects; `-s` interleaves C source lines with
generated assembly; `-g` adds HLL debug directives (do not use `-g` when reading generated asm).

### 1.1.4 Environment Variables

| Variable | Description |
|----------|-------------|
| `AS88INC` | Alternative include path for the assembler. |
| `C88INC` | Alternative path for `#include` files for `c88`. |
| `C88LIB` | Path to search for library files used by `lk88`. |
| `CC88BIN` | When set, `cc88` prepends this directory to the names of the tools it invokes. |
| `CC88OPT` | Extra options/args added to every `cc88` invocation (processed before command-line args). |
| `PATH` | Search path for executables. |
| `TMPDIR` | Alternative directory for temporary files (used by `c88`, `cc88`, `as88`, `lk88`, `lc88`, `ar88`). |

#### 1.1.4.1 / 1.1.4.2 Using the control program / makefile (examples)

```sh
cc88 -g -M -Ml calc.c -o calc.abs          # compile+assemble+link+locate in one call
```
- `-g` symbolic debug info (always specify when debugging; pair with `-O0` to avoid warning W555).
- `-M` generate map files (`.lnl`, `.map`).
- `-Ml` large memory model.
- `-o` output filename.

`-v0` shows tool invocations without running; `-v` shows and runs. Example expansion:

```
+ c88  -e -g -Ml -o /tmp/cc24611b.src calc.c
+ as88 -e -g -o calc.obj /tmp/cc24611b.src
+ lk88 -e -M calc.obj -lcl -lrt -lfp -ocalc.out -Ocalc
+ lc88 -e -M -ocalc.abs calc.out
```
`-e` = remove output after errors; `-lcl`/`-lrt`/`-lfp` link the C library, run-time library, floating
point library respectively; `-O<name>` (linker) = basename of map file.

With `mk88`: `mk88` builds via `makefile`; `mk88 -n` shows commands without running; `mk88 clean` removes
generated files.

---

## 1.2 Language Implementation

### 1.2.1 Language Extensions (summary)

| Extension | Purpose |
|-----------|---------|
| `_sfrbyte`, `_sfrword` | Declare Special Function Registers; no memory allocated. |
| `_at(addr)` | Place a variable at an absolute address. |
| storage types `_near`, `_far`, `_rom` | Memory-model-independent addressing of variables in several address ranges. |
| memory-specific pointers | Pointers that point to a specific target memory (`_near` / `_far`); efficient per type. |
| `_common` | Function contents placed in the lower 32K (shared code bank). |
| assembly functions | See §1.2.15. |
| `_interrupt(vector)` | Declare interrupt service routines via the C interrupt vector. |
| intrinsic functions | Pre-declared built-ins emitting inline asm (avoid call overhead). |

### 1.2.2 Accessing Memory

The S1C88 has **different banking mechanisms for CODE and DATA access**; the compiler handles this. Most
of an application is plain C (no extensions); only small parts use extensions (I/O via SFRs, speed, code
density, non-default memory, interrupts).

#### 1.2.2.1 Storage Types

Static objects may carry an explicit storage specifier. **Defaults:** static variables are allocated in
`_far` for the **large** and **compact code** models, and in `_near` for the **small** and **compact
data** models. Functions default to ROM (specifier may be omitted). Function return values cannot be
assigned a storage area.

| Storage Type | Description |
|--------------|-------------|
| `_near` | Lowest 64K addresses of data memory (fast access; faster pointers). |
| `_far` | Anywhere in data memory, but within one 64K page. |
| `_rom` | Located in ROM. |

```c
int  _near Var_in_near;             /* fast int in low 64K of _near memory */
int  _near * _far Ptr_in_far_to_near; /* pointer in _far, points to int in _near */
char _rom  string[] = "S1C88";      /* string in ROM */
int  _near myvar _at(0x100);        /* fixed memory address */
```

`_near` allows faster access code for frequently used variables. Allocations with the same storage
specifier are collected into **sections**; `_near` sections locate within the first 64K. Section
placement can be steered manually.

**Storage type → assembler section type/attribute** (used by `as88`):

| C Storage Type | S1C88 Section Type / Attribute |
|----------------|--------------------------------|
| `_near` | `DATA, SHORT` |
| `_far` | `DATA, FIT 10000H` |
| `_rom` | `CODE, ROMDATA, FIT 10000H` |
| `const _near` | `CODE, ROMDATA` |
| `const _far` | `CODE, ROMDATA, FIT 10000H` |

Example: `char _far example; example = 2;` generates:
```asm
LD  ep,#@dpag(_example)
LD  a,#2
LD  [@doff(_example)],a
```
(`@dpag` / `@doff` extract the page and offset of a `_far` symbol.) Pointer storage vs. target:
```c
int _near *p;   // pointer (16-bit) to int in _near
int _far  *g;   // pointer (24-bit) to int in _far
g = p;          // compiler issues a warning
```

#### 1.2.2.2 Memory Models

Four models, selected with `-M`. Default **small**. **All S1C88 programs are compiled reentrant**;
static-model functions must be specified in the source. The assembler uses the **same** model
assumptions, so objects built for different models cannot be linked together (linker detects mixing).
Separate C and run-time libraries ship per model.

| Model | Flag | Program Size | Data Size | Code-gen behavior |
|-------|------|--------------|-----------|-------------------|
| small (default) | `s` | ≤ 64K | ≤ 64K | No `LD NB`; expand-page registers assumed zero |
| compact code | `c` | ≤ 64K | > 64K | No `LD NB`; expand-page registers loaded when needed |
| compact data | `d` | > 64K | ≤ 64K | `LD NB` inserted; expand-page registers assumed zero |
| large | `l` | > 64K | > 64K | `LD NB` inserted; expand-page registers loaded when needed |

**CPU mode mapping:**

| Model | CPU Mode |
|-------|----------|
| small | Single chip (MCU), 64K (MPU) |
| compact code | 512K Min |
| compact data | 512K Max |
| large | 512K Max |

**Return-address size pushed on CALL** (critical for stack-frame and startup code):
- **small** and **compact code**: **2-byte** return addresses pushed.
- **compact data** and **large**: **3-byte** return addresses pushed.

The startup module may need adapting per situation. Models differ in PAGE-register handling and
return-address size → **mixing models in one application is rejected by the linker**.

In **all** models, C function parameters and automatics are passed via the stack [as a fallback when
registers run out — see §1.2.5]. The linker uses a whole-application call graph to **overlay** data
areas of functions that never run simultaneously (impossible for functions called through pointers).

**`_MODEL` predefined preprocessor symbol** — the model character:

| Model | `_MODEL` value |
|-------|----------------|
| small | `'s'` |
| compact code | `'c'` |
| compact data | `'d'` |
| large | `'l'` |

```c
#if _MODEL == 's'   /* small model */
#endif
```
> **Extraction note:** the `-D`/predefined-symbols description on page 50 (§1.4.2.1) garbles `_MODEL`
> as expanding to `'t'`/`'s'`/`'m'`/`'l'` ("tiny/small/medium/large") — that is **stale boilerplate**.
> The authoritative S1C88 values are `s`/`c`/`d`/`l` as given here in §1.2.2.2.

#### 1.2.2.3 The `_at()` Attribute

Place variables at absolute addresses without writing asm:
```c
_far unsigned char Display _at(0x2000);
```
Generates an absolute section reserving space at `0x2000`. Restrictions:
- Only **global** variables (not parameters or automatics).
- `extern`: variable not allocated by the compiler (mismatched addresses across modules go undetected).
- `static`: no public symbol generated.
- Absolute variables **cannot be initialized**, except those in ROM.
- Functions cannot be declared absolute.
- Overlapping absolute variables → assembler/linker error (the compiler does not check).
- Declaring the same absolute variable in two modules → link conflict (unless one is `extern`).

### 1.2.3 Data Types

**Endianness: Little Endian** — most significant part stored at the higher memory address. `char`/`short`
treated as 8-bit/16-bit `int`. `char`, `_sfrbyte`, `_sfrword`, `short`, `int`, `long` are integral.

| Data Type | Size (bytes) | Range |
|-----------|--------------|-------|
| `signed char` | 1 | -128 to +127 |
| `unsigned char` | 1 | 0 to 255U |
| `_sfrbyte` | 1 | 0 to 255U |
| `signed short` | 2 | -32768 to +32767 |
| `unsigned short` | 2 | 0 to 65535U |
| `signed int` | 2 | -32768 to +32767 |
| `unsigned int` | 2 | 0 to 65535U |
| `_sfrword` | 2 | 0 to 65535U |
| `signed long` | 4 | -2147483648 to +2147483647 |
| `unsigned long` | 4 | 0 to 4294967295UL |
| `enum` | 2 | 0 to 65535U |
| `_near` pointer | 2 | 0 to 65535U |
| `_far` pointer | 3 | 0 to 16M |

> **Pointer sizes (ABI-critical):** `_near` pointer = **2 bytes (16-bit)**; `_far` pointer = **3 bytes
> (24-bit)**. Floating point: `float` and `double` are both **single-precision (4 bytes)**; range
> ±1.176E-38 to ±3.402E+38 (see §1.6.1). *(extraction artifact: page 90 prints "4-bit data" — should
> read 4-byte/32-bit data.)*

`c88` uses 8-bit (character) arithmetic when the result equals the integer-arithmetic result, for
density.

#### 1.2.3.1 ANSI C Type Conversions

Standard integral promotion and usual arithmetic conversions apply (X3.159-1989). Promotion also applies
to function pointers and old-style function parameters of integral type — **use prototypes**. Surprising
results when `unsigned char` is promoted to `int`; e.g. `~a` and `a+1` are evaluated in `int`, so
`b == ~a` and `c != a+1` compare against the 16-bit results. Use explicit casts, e.g.
`(unsigned char)~a`, `(unsigned char)(a+1)`. Multiplications follow the same rules:
`int*int → int` (not `long`); cast an operand to `long` to get a `long` result.

#### 1.2.3.2 Character Arithmetic

8-bit arithmetic used when equivalent to integer arithmetic. Saves data space and improves density. Force
with a char cast. Examples:
```c
c = a + b;             /* char arithmetic */
i = a + b;             /* int  arithmetic */
i = (char)(a + b);     /* char arithmetic */
c = (a + b) / d;       /* int  arithmetic */
c = ((char)(a + b))/d; /* char arithmetic */
if ( a > b )           /* char arithmetic */
if ( (a + b) > c )     /* int  arithmetic */
```

#### 1.2.3.3 Special Function Registers

`_sfrbyte` is treated as `volatile unsigned char`; `_sfrword` as `volatile unsigned int`. Declared with
`_at()`; no storage allocated (registers live in the SFR area):
```c
_sfrbyte name _at( address );
_sfrword name _at( address );
_sfrbyte SPP _at(0xFF01);
```
`'sfrbyte'`/`'sfrword'` (without leading underscore) are not reserved. No symbolic debug info is
generated for SFRs (the debugger already knows them). Accesses are treated as `volatile` (never
optimized away).

### 1.2.4 Function Parameters

`c88` supports ANSI prototyping; with a prototype, `char` params are passed as `char` (no promotion to
`int`) → higher density/speed and less RAM. With a prototype, `func(char number, long value)` passes the
`char` as a byte and promotes the `int` arg to `long`. Without a prototype (K&R style / variadic like
`printf`), both `char`-type params are promoted to `int`.

### 1.2.5 Parameter Passing  *(ABI-critical)*

- **By default, parameters are passed via registers.** If not enough registers are available, the
  remaining parameters are passed on the **stack**.
- **All parameters of a variable-argument function are always passed on the stack**, pushed in **reverse
  order**, so `stdarg.h` macros work. (Including `format` in `printf(char *format, ...)`.)

See §1.2.15 and §1.3.2 for the exact register-priority scheme.

### 1.2.6 Automatic Variables

- **Non-reentrant functions:** automatics are NOT on a stack; they are in a **static area** (no
  recursion). Still overlayable with automatics of other functions; not the same as `static` locals
  (an automatic may be overlaid and lose its value; a `static` local retains its value and is never
  overlaid).
- **Reentrant functions:** automatics live on the stack (come and go with the function).
- In static functions, an automatic can be forced to a specific memory via a storage-type specifier;
  still overlayable.

### 1.2.7 Register Variables

The compiler allocates automatics and parameters into registers whenever possible, so the `register`
keyword is **ignored**. Strategy is **"saved by caller"** (caller-saves): a function needing register
contents across a call saves and restores them — only registers actually used after the call are saved.
`register` is unnecessary for density/speed. `register` cannot be used for arrays/structures.

### 1.2.8 Initialized Variables

Non-automatic initialized variables use the same space in both ROM and RAM: initializers stored in ROM,
copied to RAM at startup (transparent). Exception: variables in ROM via `_rom`. Examples (small model):
```c
int        i = 100;     /* 2 bytes ROM + 2 bytes RAM */
_rom int   j = 3;       /* 2 bytes ROM */
char      *p = "TEXT";  /* 2 bytes (p) RAM + 5 bytes ("TEXT") ROM */
_rom char  a[] = "HELP";/* 5 bytes ROM */
_near char c = 'a';     /* 1 byte ROM + 1 byte _near RAM */
```

### 1.2.9 `volatile`

Disables optimizations that keep a value in a register / skip memory writes. **Volatile variables are
located in a segment with the `NOCLEAR` attribute set.** Example:
```c
const volatile _near int real_time_clock _at(0x1234);
```

### 1.2.10 Strings

String literals (not used to initialize an array) have static storage and are **always allocated in
ROM**. Initialized arrays remain in RAM:
```c
char ramhelp[] = "help";  /* 5 bytes RAM + 5 bytes ROM */
char * _rom message[] = {"hello","alarm","exit"}; /* array + strings all ROM */
```
ANSI string concatenation of adjacent primary-expression strings; max length: ANSI limit 509,
**actual compiler limit 1500 characters**. Identical string literals within the same module are
**overlaid**. Modifying a string literal in place (e.g. `"st ing"[2] = 'r';`) is **not accepted** by
`c88`.

### 1.2.11 Pointers

A pointer has both a **logical type** (target) and a **storage type** (where the pointer lives). The
specifier left of `*` sets the **target memory**; the specifier right of `*` sets the **storage memory**:
```c
_far char *_near p;   /* p lives in _near, points to _far */
char _far *_near p;   /* identical declaration */
```
If unspecified, the **target memory default** follows the model:

| Model | Target Memory Default |
|-------|-----------------------|
| `s` | `_near` |
| `c` | `_far` |
| `d` | `_near` |
| `l` | `_far` |

In pointer arithmetic `c88` checks both the type and the **target memory** of the pointers (must match);
assigning a `_far` pointer to a `_near` pointer is invalid without a cast.

### 1.2.12 Function Pointers

Reentrant functions use the stack for parameters and automatics. In the reentrant model all functions are
implicitly reentrant, so function pointers may only point to **reentrant** functions; parameters are
passed via the stack. A function pointer may point to any reentrant function in the application.

### 1.2.13 Inline C Functions

`_inline` inlines the function body instead of calling. The inline function must be defined (in the same
file) before use; commonly defined in a header so each file that "calls" it includes the definition. An
unused `_inline` function produces no code. No specific debug info; the debugger treats an inline
function as one HLL source line. `#pragma asm`/`endasm` are allowed inside inline functions (→ inline
assembly functions). Example:
```c
_inline int add( int a, int b ) { return( a + b ); }
```

### 1.2.14 Inline Assembly

| Pragma | Effect |
|--------|--------|
| `#pragma asm` | Insert assembly text following the pragma; **flushes** the peephole code buffer. |
| `#pragma asm_noflush` | Like `asm`, but the peephole optimizer does **not** flush the code buffer (assumes register contents remain valid). |
| `#pragma endasm` | Switch back to C. |

The compiler does not interpret inline asm text — it passes lines directly to the output. Flushing
prevents reordering of pending peephole-buffer instructions past the inline asm. Modules with inline asm
are not portable. For portable assembly access, prefer intrinsic functions.

### 1.2.15 Calling Assembly Functions  *(ABI-critical — asm interface)*

The S1C88 C compiler uses **fixed registers for passing arguments**. When calling asm functions from C,
follow this scheme; **all arguments must be passed via the usable registers only** (no error/warning is
issued even if some arguments cannot be allocated to registers). To verify allocation, compile the
calling C function and inspect the generated asm.

**Parameter allocation scheme — registers in descending priority by type:**

| Type | Priority (Higher → Lower) |
|------|---------------------------|
| `char` | `A`, `L`, `YP`, `XP`, `H`, `B` |
| `int` | `BA`, `HL`, `IX`, `IY` |
| `long` | `HLBA`, `IYIX` |
| near pointer | `IY`, `IX`, `HL`, `BA` |
| far pointer | `IYP`, `IXP`, `HLP` |

> Far-pointer register pairs: `IYP = IY + YP`, `IXP = IX + XP`, `HLP = HL + A` (see §1.3.2).

**Worked example:**
```c
int sub_asm(int ia, char ca, int ib, char cb, int _near *pic);
```
Allocation (note `A` — highest `char` priority — is consumed by `int ia` via `BA`, so `char ca` falls to
`L`):
```
BA = int  ia
L  = char ca
IX = int  ib
YP = char cb
IY = int  *pic   (near pointer)
```
Generated call (no options):
```asm
LD  iy,#05h
LD  [sp],iy      ; [sp] <- ic
LD  iy,sp        ; iy <- &ic
LD  ba,#01h      ; ba <- ia
LD  l,#033h      ; l  <- ca
LD  ix,#02h      ; ix <- ib
LD  yp,#034h     ; yp <- cb
CARL _sub_asm
INC ba           ; id <- ba (return value)
```
The `int` return value lands in `BA` (highest-priority `int` return register). The asm body is supplied
via `#pragma asm` … `_sub_asm: … ret … #pragma endasm`. Note the **leading underscore** convention:
C symbol `sub_asm` → asm label `_sub_asm`.

### 1.2.16 Intrinsic Functions

Built-ins interpreted by the code generator → efficient inline asm; no parameter-passing/context-saving
overhead. Names start with `_` (implementation-defined per ANSI). Advantages over `#pragma asm`: host
simulation/stubs possible, C-level variable access, optimized code (except `_nop()`). Prototypes are in
`c88.h`.

| Function | Prototype | Description |
|----------|-----------|-------------|
| `_bcd()` | `void _bcd();` | Set `D` flag during expression evaluation (binary-decimal add/sub/negate). K&R arg list (any type). |
| `_halt()` | `void _halt(void);` | Emit `HALT`. |
| `_int()` | `void _int(ICE vector);` | Emit `INT` software interrupt; `vector` must be an Integral Constant Expression (interrupt vector address). |
| `_jrsf()` | `char _jrsf(ICE number);` | `JRS Fn,_lab` — for `if()` tests; `number` = const 0..3; codegen picks `Fn`/`NFn`. Returns the result. |
| `_nop()` | `void _nop(void);` | Emit `NOP` (not optimized away). |
| `_pack()` | `char _pack(int operand);` | `PACK` — pack int into char. Returns char. |
| `_rlc()` | `char _rlc(char operand);` | `RLC` — rotate byte left (affects result only, not operand). Returns result. |
| `_rrc()` | `char _rrc(char operand);` | `RRC` — rotate byte right (result only). Returns result. |
| `_slp()` | `void _slp(void);` | Emit `SLP`. |
| `_swap()` | `char _swap(char operand);` | `SWAP` — swap high/low nibbles. Returns result. |
| `_ubcd()` | `void _ubcd();` | Set `U` and `D` flags during evaluation (lower nibble only + BCD). K&R arg list. |
| `_unpack()` | `void _unpack();` | Set `U` flag during evaluation (lower nibble only). K&R arg list. |
| `_upck()` | `int _upck(char operand);` | `UPCK` — unpack char into int. Returns int. |

Examples:
```c
_halt();           /* -> HALT */
_nop();            /* -> NOP  */
_slp();            /* -> SLP  */
c = _rlc(c);       /* rotate left  */
c = _rrc(c);       /* rotate right */
if ( _jrsf(2) ) {} /* -> JRS NF2, _L0001 ... _L0001: */
```

### 1.2.17 Interrupts

`_interrupt(vector)` — special type qualifier, only on function declarations. Interrupt functions
**cannot return anything** and must have a `void` argument list. The qualifier takes the **interrupt
vector address** of a **two-byte** interrupt vector area. Some interrupts (e.g. hardware reset) are
reserved/handled by the compiler (run-time library). The compiler generates an interrupt service frame.

```c
void _interrupt(0x30) transmit(void) { c = 1; }
```
generates:
```asm
DEFSECT ".abs_48", CODE AT 48
SECT    ".abs_48"
DW      _transmit
DEFSECT ".short_code", CODE, SHORT
SECT    ".short_code"
_transmit:
LD      A,#1
LD      [_c],A
RETE
```
(The vector value `0x30` = 48 decimal; the vector entry is a `DW` to the handler. The handler ends with
`RETE`.) See §1.3.6 for register saving.

### 1.2.18 Structure Tags

A tag specifies layout only. A memory type used as a **storage** specifier on a member is **ignored**
(silently); a memory type used to specify a pointer **target** memory is valid:
```c
struct S {
  _near int  i;   /* storage specifier: IGNORED (no warning) */
  _far  char *p;  /* target-memory specifier: correct */
};
```
A tag may declare objects in different memories (in the same scope).

### 1.2.19 Typedef

`typedef` follows normal scope rules (may be re-declared in inner blocks, not at parameter level). Memory
specifiers are allowed; at least one type specifier required.
```c
typedef _near int NEARINT; /* storage type _near */
typedef int _far *PTR;     /* logical type _far, default storage */
```

### 1.2.20 Language Extensions (non-ANSI)

- **Character arithmetic** (§1.2.3.2).
- **Uninitialized constant definitions** — `const char i[1];` emits `DS 1` (no implicit zero init).
- **Keyword extensions** — `_near`, `_far`, `_sfrbyte`, etc.
- **Max significant characters** — 500 in an identifier (vs ANSI min 31); excess truncated silently.
- **C++ style comments** (`//`).
- **`__STDC__`** defined as `0`.
- **Promoting old-style function parameters** — do not promote old-style params when prototype checking.
- **Using `unsigned char` for 0x80–0xff** — unsuffixed octal/hex constant type chosen from:
  `char, unsigned char, int, unsigned int, long, unsigned long`.
- **lvalue cast** — allow cast of an lvalue with incomplete type `void`, and lvalue casts that do not
  change type/memory: `void *p; ((int*)p)++;` allowed; `int i; (char)i=2;` not allowed.
- **Constant-string → non-const pointer** — assignment not checked (`char *p; p = "hello";` no warning).

### 1.2.21 Portable C Code

Header `c88.h` checks `_C88` (c88 only) and redefines storage-type specifiers when not c88, plus provides
adapted prototypes of all built-ins, so source compiles on a host C compiler for simulation. Write the
built-ins in C (same behavior) and link for host simulation.

### 1.2.22 How to Program Smart

1. Always use prototypes (so `char` stays `char`, not promoted to `int`).
2. In large model, declare frequently used (static) variables `_near` (use `register` to stay portable).
3. Prefer `unsigned` (unsigned comparisons take less code).
4. Use the smallest data type possible (char for small loops).

---

## 1.3 Run-time Environment

### 1.3.1 Startup Code

Linking with the library auto-links `cstart.obj` (one per model and execution mode), present in every C
library. Source is `cstart.c` in `lib/src`. Edit a copy to match needs; it has macro-preprocessor symbols
for tuning. Build with e.g. `cc88 -Ms -c cstart.c`.

- An absolute code section sets up the **reset vector** and the C environment. The reset vector contains
  a jump to `_START` (global label, referred to by the compiler; default application start address — see
  the `start` keyword in the locator DELFEE language). Code space for **unused interrupt vectors** is
  reserved in the locator description file (prevents `lc88` from using it; may be used for small user code
  sections).
- The **stack** is defined in the `.dsc` file with keyword `stack` → section `stack` (§1.3.4).
- The **heap** is defined with keyword `heap` → section `heap` (§1.3.5).
- Startup copies initializers of initialized C variables from ROM to RAM (each memory type has a unique
  ROM and RAM section name) using run-time library helpers. Compile `cstart.c` with `-DNOCOPY` to skip
  this (see DELFEE `table` keyword).
- The application is entered at global label `_main` (for C `main()`). On return (unlikely in embedded),
  the program ends with `SLP` at label `__exit` (useful breakpoint; also reached via `exit()`).
- **Watchdog/NMI** handling is done by default in the startup code (the Watchdog cannot be disabled);
  recompile `cstart.c` if the application handles NMI/Watchdog itself.

| Macro | Effect |
|-------|--------|
| `NOCOPY` | Do **not** produce code to clear BSS sections and initialize DATA sections. |

### 1.3.2 Register Usage  *(ABI-critical)*

The compiler uses a **flexible** register allocation scheme — any C change may alter register usage. The
**fixed parameter-passing scheme**:

- Arguments are passed via registers **`A`, `B`, `L`, `H`, `YP`, `XP`, `BA`, `HL`, `IX`, `IY`**.
  - **`char` arguments** via byte registers **`A`, `L`, `YP`, `XP`, `H`, `B`**.
  - **`int` arguments** via word registers **`BA`, `HL`, `IX`, `IY`**.
  - **`long` arguments** via 32-bit register pairs **`HLBA`** and **`IYIX`**.
  - **near pointers** via **`IY`, `IX`, `HL`, `BA`**.
  - **far pointers** via register pairs **`IYP`, `IXP`, `HLP`**, where `IYP = IY + YP`, `IXP = IX + XP`,
    `HLP = HL + A`.
- **Structures and unions** are passed via the **stack**.
- When too many arguments to fit registers, the rest go on the **stack**.

(Priority order within each class is the descending order given in §1.2.15.)

**Return-value registers:**

| Return Type | Register | Description |
|-------------|----------|-------------|
| `char` | `A` | accumulator |
| `short` / `int` | `BA` | |
| `long` | `HLBA` | `HL` = high word, `BA` = low word |
| pointer | `HLP` | `HL + A` |

- **Structures and unions are returned on the stack.**
- **Calling convention:** caller-saves ("saved by caller", §1.2.7).
- **Floating point** (`float`/`double`): argument/return via `HLBA` (`HL` ← high word, `BA` ← low word)
  (§1.6.1).

### 1.3.3 Section Usage  *(ABI-critical)*

The compiler emits a `DEFSECT` for each used section. Section names:

| Section Name | Contents |
|--------------|----------|
| `.text` | code — models **s** and **c** |
| `.text_function` | code — models **d** and **l** |
| `.comm` | code with `_common` qualifier; `_interrupt` code |
| `.nbss` | cleared `_near` data |
| `.fbss` | cleared `_far` data |
| `.nbssnc` | non-cleared `_near` data |
| `.fbssnc` | non-cleared `_far` data |
| `.ndata` | initialized `_near` data |
| `.fdata` | initialized `_far` data |
| `.nrdata` | const `_near` data |
| `.frdata` | const `_far` data |

> **Extraction note:** the source lays out the section/comment columns ambiguously around `.comm`. As
> extracted, the `.comm` row pairs "code with `_common` qualifier" and "`_interrupt` code"; the per-row
> mapping above reflects the most consistent reading. Treat the `.text`/`.text_function` model split and
> the bss/data/rdata naming as authoritative.

Other section names seen in examples: `.short_code` / `.code48` (interrupt handler code, `CODE, SHORT`),
`.abs_NN` / `.codeNN` (absolute vector section, `CODE AT NN`), `.bss` (`DATA, SHORT, CLEAR`).

### 1.3.4 Stack  *(ABI-critical)*

The S1C88 has a system stack of **max 64K bytes**, used for function calls, interrupts, function
parameters and automatics. **Static functions** use overlayable sections instead. The stack grows
**downwards** (high → low memory).

**Reentrant-function system stack layout** *(figure 1.3.4.1 reconstructed; top = high memory):*

```
 high memory   ──────────────────────────
               parameter n
               ...
               parameter 1
               return address
               saved registers
               ─────────────────────────  <- fp ($fp)
               local 1
               ...
               local n
               temporary storage
               ─────────────────────────  <- sp ($sp)
 low memory    (stack grows down)
   framesize = fp..sp region ;  stacksize = total stack ;  stack pointer adjust ("sdjust")
```
*(Field name "sdjust" in the extracted figure is a typo for "adjust".)*

The stack is defined in the `.dsc` file with `stack` → section `stack`. Size via `length=size`; if
unspecified, the locator allocates the remaining RAM (as the startup code does). Locator-defined labels
`__lc_bs` (begin) and `__lc_es` (end) give the stack bounds — the locator allocates a stack section
**only if** the application references one of these symbols. Ensure enough downward-growing space.

For **non-reentrant** functions, (non-register) automatics and parameters live in a **static area** and
use **no stack space**.

### 1.3.5 Heap

Needed only for `malloc()`, `calloc()`, `free()`, `realloc()`. Allocated by the locator only if a memory
function is used. Special section `heap` (placeable anywhere via the `.dsc` file); size via `length=size`;
if unspecified but referenced, the locator allocates the rest of memory. Locator labels `__lc_bh` (begin)
and `__lc_eh` (end) are used by `sbrk()` (called by `malloc()`). The heap segment is only allocated when
its locator labels are used.

```
amode data
{
    section selection=w;
    heap length=1000;  // heap (only when used)
}
```
For **small** or **compact data** models, the heap declaration must be moved from `amode data` into
`amode data_short`, else the locator reports errors.

### 1.3.6 Interrupt Functions  *(ABI-critical)*

A function declared `_interrupt(n)` differs from a normal function:
1. **All registers that might be corrupted during execution are saved on entry and restored on exit**
   (a normal function saves only registers it directly uses).
2. Terminates with **`RETE`** instead of `RET`.

Example (`_interrupt(0x30) void handler(void)`):
```asm
GLOBAL  _handler
DEFSECT ".code48", CODE AT 030H
SECT    ".code48"
DW      _handler            ; vector entry at 0x30
DEFSECT ".comm", CODE, SHORT
SECT    ".comm"
_handler:
PUSH    ale                 ; save registers
LD      iy,#01h
LD      [_flag],iy          ; flag = 1
POP     ale                 ; restore
RETE
DEFSECT ".bss", DATA, SHORT, CLEAR
SECT    ".bss"
GLOBAL  _flag
_flag:  DS 2
EXTERN  (DATA) __lc_es
END
```

---

## 1.4 Compiler Use

### 1.4.1 Control Program (`cc88`)

```
cc88   [ [option] ... [control] ... [file] ... ] ...
```
Options preceded by `-`; some are handled by `cc88`, the rest passed to the relevant tool. (See file
extension table at top for dispatch.) Only one `.out` file per invocation. `cc88` produces unique
intermediate filenames and removes them afterward (unless `-tmp`). When compiler and assembler run
back-to-back, `cc88` prevents preprocessing of the compiler-generated asm; otherwise asm inputs are
preprocessed first.

**Control Program Options:**

| Option | Description |
|--------|-------------|
| `-Mc` | Compact code memory model |
| `-Md` | Compact data memory model |
| `-Ml` | Large memory model |
| `-Ms` | Small memory model |
| `-Ta arg` | Pass argument directly to the assembler |
| `-Tc arg` | Pass argument directly to the C compiler |
| `-Tlk arg` | Pass argument directly to the linker |
| `-Tlc arg` | Pass argument directly to the locator |
| `-V` | Display version header only |
| `-al` | Generate absolute list file (per module) |
| `-c` | Do not link: stop at `.obj` |
| `-cl` | Do not locate: stop at `.out` |
| `-cs` | Do not assemble: compile C files to `.src` and stop |
| `-f file` | Read arguments from file (`-` = stdin) |
| `-ieee` | Set locator output format to IEEE-695 (default) |
| `-nolib` | Do not link with the standard libraries |
| `-o file` | Specify the output file |
| `-srec` | Set locator output format to Motorola S-records |
| `-tmp` | Keep intermediate files (created in current directory) |
| `-v` | Show command invocations |
| `-v0` | Show command invocations, but do not start them |

`-o` routing: passed to locator by default; to linker with `-cl`; to assembler with `-c` (single source
only); to compiler with `-cs`. `-f` command-file rules: multiple args per line; quote to embed
whitespace; use opposite quote type for embedded quotes; backslash-newline continuation lines; nesting up
to **25 levels**. `cc88` environment variables: `TMPDIR`, `CC88OPT`, `CC88BIN` (see §1.1.4).

### 1.4.2 Compiler (`c88`)

```
c88   [ [option] ... [file] ... ] ...
```
Files processed left-to-right; on conflicting options the **first (leftmost)** wins. Each `.c` → `.src`.
`-o` is consumed once per source file (multiple `-o` for multiple sources). Option args must start
immediately after the option **except** `-o` (needs a space/tab before the filename) and `-I`.

**Compiler Options:**

| Option | Description |
|--------|-------------|
| `-Dmacro[=def]` | Define preprocessor macro (`=1` if `def` omitted) |
| `-H file` | Include file before starting compilation |
| `-Idirectory` | Look in directory for include files |
| `-M{s\|c\|d\|l}` | Select memory model: small / compact code / compact data / large |
| `-O{0\|1}` | Control optimization |
| `-V` | Display version header only |
| `-e` | Remove output file if compiler errors occur |
| `-err` | Send diagnostics to error list file (`source.err`) |
| `-f file` | Read options from file |
| `-g` | Enable symbolic debug information |
| `-o file` | Specify name of output file |
| `-s` | Merge C source code with assembly output |
| `-w[num]` | Suppress one (`-wnum`) or all (`-w`) warning messages |

(Floating-point options `-F` / `-Fc` — see §1.6.2. Control-program-only float option `-fptrap` selects
the trapping FP library — see §1.6.4.)

#### 1.4.2.1 Detailed option notes

- **`-D`** — like `#define`. ANSI predefined (non-removable): `__FILE__`, `__LINE__`, `__TIME__`,
  `__DATE__`, `__STDC__` (= `0`). c88-specific: `_C88` (compiler ID, expands to version),
  `_MODEL` (model character — authoritative values `s`/`c`/`d`/`l`; see §1.2.2.2 and the extraction note
  there about the stale "tiny/small/medium/large" boilerplate on this page).
  Example: `c88 -DNORAM -DPI=3.1416 test.c`.
- **`-e`** — remove output file on error (for `make`).
- **`-err`** — write errors to `source.err` instead of stderr.
- **`-f file`** — command file (same quoting/continuation/25-level-nesting rules as `cc88 -f`).
- **`-g`** — symbolic debug directives; high optimization reduces debug comfort.
- **`-H file`** — same as `#include "file"` at line 1.
- **`-I directory`** — add to include search (left-to-right). `""` includes search: file's directory,
  then `-I` dirs, then `C88INC`, then `../include` relative to the compiler binary. `<>` includes skip
  the file's own directory. See §1.4.3.
- **`-M model`** — default `-Ms`. See §1.2.2.2.
- **`-O flag`** — default `-O1`. `-O0` = switchable optimizations off; `-O1` = smallest code (see table
  below).
- **`-o file`** — default `module.src`; first `-o` ↔ first file, etc. `c88 a.c b.c -o x.src -o y.src` →
  `x.src` for `a.c`, `y.src` for `b.c`.
- **`-s`** (pragma `source`) — interleave C source as comments with asm.
- **`-V`** — version banner.
- **`-w[num]`** — `-w` suppress all warnings; `-wnum` suppress warning *num* (e.g. `-w135`).

**`-O` optimization breakdown:**

| Optimization | `-O1` (default) | `-O0` |
|--------------|-----------------|-------|
| Alias checking | **relaxed** (keeps remembered register contents across indirect writes — only safe if no aliasing) | **strict** (erase all user-var register contents on indirect write) |
| Clear non-init static/public vars | always done (independent of option) | always done |
| Common subexpression elimination (CSE) | enabled | disabled (also disables relax-alias, expr-propagation, loop-invariant motion) |
| Constant/copy propagation | enabled | disabled |
| Expression propagation | enabled | disabled |
| Control-flow / order rearranging (jump chaining, cond-jump reversal) | enabled | disabled |
| Peephole optimization | enabled | disabled |
| Move invariant code outside loop | enabled | disabled |
| Fast loops (increases code size) | disabled regardless | disabled regardless |
| Small code size | enabled (fewer instructions, possibly more cycles) | disabled |
| Loop unrolling | disabled regardless | disabled regardless |
| Subscript strength reduction | enabled | disabled |

### 1.4.3 Include Files

`#include` search algorithm:
1. For `""` (and not absolute): directory of the file containing the `#include` (skipped for `<>`).
2. `-I` directories, left-to-right.
3. `C88INC` (may list multiple dirs separated by `;`, `,`, or space).
4. The `include` subdirectory one level above the directory containing the `c88` binary (e.g. binary in
   `C:\C88\BIN` → `C:\C88\INCLUDE`).

A directory may omit the trailing separator (c88 inserts it).

### 1.4.4 Pragmas

Per ANSI 3.8.6. Priority (highest wins): **pragmas > keywords > command-line options**. Supported:

| Pragma | Effect |
|--------|--------|
| `asm` | Insert following non-preprocessor lines as asm; flush peephole code buffer (see §1.2.14). |
| `asm_noflush` | Like `asm` but do not flush the code buffer (assumes register contents valid). |
| `endasm` | Switch back to C. |
| `source` | Same as `-s`: mix C source with asm. |
| `nosource` | Default: disable C-source-in-asm. |

(`#pragma optimize` referenced in §1.1.2.2 for per-region optimization level control.)

### 1.4.5 Compiler Limits

ANSI minimums vs. c88 actual (in parentheses). `D` = dynamic (limited by host free memory); `P` = limited
by internal parser stack (size 200, actual may be lower).

- Nesting of compound/iteration/selection statements: ANSI 15 (`P > 15`)
- Nesting of conditional inclusion: ANSI 8 (50)
- Pointer/array/function declarators modifying a type: ANSI 12 (15)
- Parenthesized declarators within a full declarator: ANSI 31 (`P > 31`)
- Parenthesized expressions within a full expression: ANSI 32 (`P > 32`)
- Significant chars in external identifier: ANSI 31 (31 full-ANSI mode / **500** non-ANSI mode)
- External identifiers in one translation unit: ANSI 511 (D)
- Block-scope identifiers in one block: ANSI 127 (D)
- Macro identifiers simultaneously defined: ANSI 1024 (D)
- Parameters in one function declaration: ANSI 31 (D)
- Arguments in one function call: ANSI 31 (D)
- Parameters in one macro definition: ANSI 31 (D)
- Arguments in one macro call: ANSI 31 (D)
- Characters in a logical source line: ANSI 509 (**1500**)
- Characters in a string literal (after concatenation): ANSI 509 (**1500**)
- Nesting of `#include` files: ANSI 8 (50)
- Case labels in a switch (excluding nested): ANSI 257 (D)
- Members in a single struct/union: ANSI 127 (D)
- Enumeration constants in a single enum: ANSI 127 (D)
- Levels of nested struct/union in a single struct-declaration-list: ANSI 15 (D)

### 1.4.6 Compiler Messages

Three classes: user errors, warnings, internal compiler errors. Some errors carry extra 'I'-marked info
(no number shown). 'F'-marked = fatal (compilation aborts, output may be incomplete). **Error/warning
number ranges:** frontend uses **0–499**; backend (code generator) uses **500+**. For a code-generator
error, the displayed C source line is the **last line of the function** (codegen runs at function end),
but c88 shows the causing line number before the message; the error number is also emitted into the asm
output (so the assembler rejects a corrupt build — see `-e`). Warnings do not produce erroneous output;
controlled by `-w[num]`. Internal errors print `S number: internal error - please report` (report to
Seiko Epson with a minimal reproducing C program).

### 1.4.7 Return Values (exit status)

| Status | Meaning |
|--------|---------|
| 0 | Compilation successful, no errors |
| 1 | User errors, but terminated normally |
| 2 | Fatal or system error, premature ending |
| 3 | Stopped due to user abort |

---

## 1.5 Libraries

C libraries (object per memory model) + headers with prototypes; sources (C or asm) also shipped. Some
complex operations (e.g. 32-bit signed divide) are run-time library functions, not inline.
> Vendor note: "Use the library functions at the user's own risk after sufficient evaluation; operation
> is not guaranteed."

### 1.5.1 Header Files

| Header | Contents (selected) |
|--------|---------------------|
| `<assert.h>` | `assert` |
| `<c88.h>` | c88 definitions/prototypes (for host prototyping) — no C functions |
| `<ctype.h>` | `isalnum, isalpha, isascii, iscntrl, isdigit, isgraph, islower, isprint, ispunct, isspace, isupper, isxdigit, toascii, _tolower, tolower, _toupper, toupper` |
| `<errno.h>` | Error numbers — no C functions |
| `<limits.h>` | Limits/sizes of integral types — no C functions |
| `<locale.h>` | `localeconv, setlocale` (skeletons) |
| `<setjmp.h>` | `longjmp, setjmp` |
| `<signal.h>` | `raise, signal` (skeletons) |
| `<stdarg.h>` | `va_arg, va_end, va_start` |
| `<stddef.h>` | `offsetof`, special type definitions |
| `<stdio.h>` | `clearerr, fclose, _fclose, feof, ferror, fflush, fgetc, fgetpos, fgets, fopen, _fopen, fprintf, fputc, fputs, fread, freopen, fscanf, fseek, fsetpos, ftell, fwrite, getc, getchar, gets, _ioread, _iowrite, _lseek, perror, printf, putc, putchar, puts, _read, remove, rename, rewind, scanf, setbuf, setvbuf, sprintf, sscanf, tmpfile, tmpnam, ungetc, vfprintf, vprintf, vsprintf, _write` |
| `<stdlib.h>` | `abort, abs, atexit, atof, atoi, atol, bsearch, calloc, div, exit, free, getenv, labs, ldiv, malloc, mblen, mbstowcs, mbtowc, qsort, rand, realloc, srand, strtod, strtol, strtoul, system, wcstombs, wctomb` |
| `<string.h>` | `memchr, memcmp, memcpy, memmove, memset, strcat, strchr, strcmp, strcol(l), strcpy, strcspn, strerror, strlen, strncat, strncmp, strncpy, strpbrk, strrchr, strspn, strstr, strtok, strxfrm` |
| `<time.h>` | `asctime, clock, ctime, difftime, gmtime, localtime, mktime, strftime, time` (skeletons) |
| `<float.h>` | Floating-point constants + FP trap-handling API (§1.6) |
| `<math.h>` | `acos, asin, atan, atan2, ceil, cos, cosh, exp, fabs, floor, fmod, frexp, ldexp, log, log10, modf, pow, sin, sinh, sqrt, tan, tanh` |

### 1.5.2 C Libraries — naming

The `lib` directory has per-processor subdirectories. C library name syntax (Table 1.5.2.1):

| Compiler Model | Library to link |
|----------------|-----------------|
| Small (default) | `libcs.a` (`-lcs`) |
| Compact code | `libcc.a` (`-lcc`) |
| Compact data | `libcd.a` (`-lcd`) |
| Large | `libcl.a` (`-lcl`) |

`lk88 -l<x>` looks for `lib<x>.a` in the system `lib` directory (the control program supplies these).

#### 1.5.2.1 Implementation status legend

`Y` = fully implemented; `I` = implemented but needs a user low-level routine; `R` = needs recompilation;
`L` = delivered as a skeleton.

Notable status points (from pages 63–65):
- `ctype.h`: most routines as both macro and function; `_tolower`/`_toupper` only macros; `isascii`/
  `toascii` non-ANSI.
- `locale.h` (`localeconv`, `setlocale`): `L` — "No OS present".
- `stdio.h`: `tmpfile`/`tmpnam` `L`; most file ops are `I` and need low-level `_read`/`_write`/`_ioread`/
  `_iowrite`/`_lseek`/`_fopen`/`_fclose`. `tmpnam` is a random name generator (should use a process ID).
- `stdlib.h`: `atoi` `R` (needs recompilation of `exit()`); `getenv` `L` ("No OS present"); `system` `L`;
  multibyte/wide-char functions (`mblen`, `mbstowcs`, `mbtowc`, `wcstombs`, `wctomb`) `L` ("wide chars
  not supported").
- `string.h`: all `Y`.
- `time.h`: most `L` ("real time clock not supported").
- `abort`/`exit` call `_exit()` in `cstart`.

#### 1.5.2.2 C Library Interface (selected; low-level / customizable I/O)

| Function | Prototype | Notes |
|----------|-----------|-------|
| `_fclose` | `int _fclose(FILE *file)` | Low-level file close (used by `fclose`). |
| `_fopen` | `int _fopen(const char *file, FILE *iop)` | Low-level file open (used by `fopen`/`freopen`). |
| `_ioread` | `int _ioread(FILE *fp)` | Low-level input; **empty stub — customize** (used by all input functions). |
| `_iowrite` | `int _iowrite(int c, FILE *fp)` | Low-level output; **empty stub — customize** (used by all output functions). |
| `_lseek` | `long _lseek(FILE *iop, long offset, int origin)` | Low-level positioning. |
| `_read` | `size_t _read(FILE *fin, char *base, size_t size)` | Low-level block input; **customize** (else uses `_ioread`). |
| `_write` | `size_t _write(FILE *iop, char *base, size_t size)` | Low-level block output; **customize** (else uses `_iowrite`). |
| `_tolower` / `_toupper` | `int _tolower(int c)` / `int _toupper(int c)` | Non-ANSI, no range check. |

> The remaining standard library functions (`printf`, `scanf` family, `malloc`/`free`/`calloc`/`realloc`,
> `str*`, `mem*`, `qsort`, `bsearch`, `atoi`/`atol`/`atof`, `div`/`ldiv`, `setjmp`/`longjmp`,
> `signal`/`raise`, `<time.h>`, etc.) are standard ANSI C. Pages 67–87 give their conventional ANSI
> descriptions; they are not re-transcribed here as they carry no S1C88-specific ABI detail.
> Embedded specifics worth noting:
> - `getc` is `#define`d as `getchar` (FILE I/O not supported).
> - `clock()` returns `1`; `tmpnam` uses a random generator; `<time.h>` mostly skeletons (no RTC).
> - `malloc`/`calloc`/`realloc`/`free` require a heap (§1.3.5); in small/compact-data models the heap
>   must be moved to `amode data_short` in the `.dsc` or locating fails. By default **no heap** is
>   allocated; using an allocator with no heap defined → locator error.
> - `abort` and `exit` call `_exit()` in the startup module.

#### 1.5.2.3 `printf`/`scanf` formatting

All `printf`-family call `_doprint()`; all `scanf`-family call `_doscan()`. Three formatter sizes:

| Formatter | Capability |
|-----------|-----------|
| LARGE | Full formatter, no restrictions (default in shipped libraries). |
| MEDIUM | No floating-point printing. |
| SMALL | As MEDIUM, but the precision specifier `.` cannot be used. |

Select a smaller formatter by linking a separate object, e.g. (small model, MEDIUM):
```sh
cc88 -Ms hello.obj c:\c88\lib\libcs\_doprntm.obj
```
Object naming: `_doprnt{s|m|l}.obj` and `_doscan{s|m|l}.obj` (`s`=SMALL, `m`=MEDIUM, `l`=LARGE), in
`lib\libc{s|c|d|l}` per model.

**`printf` conversion characters:**

| Char | Printed as |
|------|-----------|
| `d`, `i` | `int`, signed decimal |
| `o` | `int`, unsigned octal |
| `x`, `X` | `int`, unsigned hex (lower/upper) |
| `u` | `int`, unsigned decimal |
| `c` | `int`, single character (→ unsigned char) |
| `s` | `char *`, until NULL or precision reached |
| `f`, `e`, `E`, `g`, `G` | `double` |
| `n` | `int *`, count of chars written so far stored into the arg; no output. Pointer to int in default memory. |
| `p` | pointer (hexadecimal **24-bit** value) |
| `%` | literal `%` |

`printf` flags: `-` left-adjust; `+` always show sign (higher precedence than space); space → sign for
negatives, space for positives; `0` zero-pad numeric fields; `#` alternate form. Then optional minimum
field width (or `*` from next `int` arg), `.` precision (or `*`), and length modifier `h` (short),
`l` (long), `L` (long double).

**`scanf` conversion characters:**

| Char | Scanned as |
|------|-----------|
| `d` | `int`, signed decimal |
| `i` | `int`, octal (leading `0`) / hex (`0x`/`0X`) / decimal |
| `o` | `int`, unsigned octal |
| `u` | `int`, unsigned decimal |
| `x` | `int`, unsigned hex (lower/upper) |
| `c` | single char (→ unsigned char) |
| `s` | `char *`, non-whitespace string + NULL terminator |
| `f`, `e`, `E`, `g`, `G` | `float` |
| `n` | `int *`, count written so far; no scanning |
| `p` | pointer; hexadecimal 24-bit value |
| `[...]` | match input chars from the set (`[]...]` includes `]`) |
| `[^...]` | match input chars not in the set (`[^]...]` includes `]`) |
| `%` | literal `%`, no assignment |

### 1.5.3 Run-time Library

Some generated code calls run-time library functions (too large to inline), e.g. 32-bit division. A
run-time library function name always has **two leading underscores**. Because c88 emits assembly, it
prepends one `_` to public C names (to distinguish them from S1C88 registers); so a C function with a
leading underscore becomes an asm label with **two** leading underscores — potentially colliding with a
run-time library name. (ANSI: leading-underscore public names are non-portable / implementation-defined.)
The run-time library is linked via `-lrt`.

---

## 1.6 Floating Point Arithmetic

FP support is a separate set of libraries, linked **after the C library and before the run-time
library**. Libraries are reentrant, using only temporary program-stack memory. Implemented to **IEEE-754**
(1985). c88 supports **single precision only** via `float` and `double`. A non-trapping library is also
included per model for speed.

### 1.6.1 Data Size and Register Usage  *(ABI)*

- `float` and `double` are **4-byte (32-bit)** values *(extraction prints "4-bit" — a typo for 4 bytes)*.
  Range ±1.176E-38 to ±3.402E+38.
- The compiler uses the **`HLBA`** register (`HL` ← high word, `BA` ← low word) for `float`/`double`
  arguments and return values when allocated to a register.

### 1.6.2 Compiler Options `-F` / `-Fc`

| Option | Effect |
|--------|--------|
| `-F` | Force single precision only — `double`/`long double` treated as `float`; suppress `float`→`double` default argument promotion. Must link the single-precision C library. |
| `-Fc` | Treat all floating-point constants as single-precision `float` (unless explicit `l` suffix). Normally ANSI treats `3.0` as `double`, `3.0f` as `float`. |

### 1.6.3 Special Floating Point Values (IEEE-754)

| Special Value | Sign | Exponent | Mantissa |
|---------------|------|----------|----------|
| +0.0 (Positive Zero) | 0 | all zeros | all zeros |
| -0.0 (Negative Zero) | 1 | all zeros | all zeros |
| +INF (Positive Infinite) | 0 | all ones | all zeros |
| -INF (Negative Infinite) | 1 | all zeros* | all zeros |
| NaN (Not a Number) | 0 | all ones | all ones |

> *Extraction artifact: the -INF row shows exponent "all zeros" in the extracted text, which is
> inconsistent with IEEE-754 (−INF has exponent all-ones, sign 1). Treat the -INF exponent as **all
> ones** per IEEE-754; flagged because the source text as extracted is wrong/garbled here.

### 1.6.4 Trapping Floating Point Exceptions

Two FP run-time libraries per model: **with** trap handling (`libfp{m}t.a`) and **without**
(`libfp{m}.a`), where `{m}` ∈ `s/c/d/l`. The control-program option `-fptrap` links the trapping library;
otherwise the non-trapping (faster, but undefined result on out-of-range) library is used.

- **IEEE-754 trap handler:** install via `_fp_install_trap_handler`; select exception types with
  `_fp_set_exception_mask`.
- **SIGFPE signal handler:** install via ANSI `signal`; requires a basic IEEE-754 handler that calls
  `raise(SIGFPE)`. No context/nature info passed to the signal handler.

```c
static void pass_fp_exception_to_signal(_fp_exception_info_t *info) {
    info;            /* suppress unused warning */
    raise(SIGFPE);   /* continue with unaltered result */
}
```

### 1.6.5 Floating Point Trap Handling API (`<float.h>`)

```c
int  _fp_get_exception_mask(void);
void _fp_set_exception_mask(int);
int  _fp_get_exception_status(void);
void _fp_set_exception_status(int);
void _fp_install_trap_handler(void (*)(_fp_exception_info_t *));
```
Exception flag bits: `EFINVOP`, `EFDIVZ`, `EFOVFL`, `EFUNFL`, `EFINEXCT`; `EFALL` = OR of all.

`_fp_exception_info_t` members:
- `exception` — one of `EFINVOP`, `EFDIVZ`, `EFOVFL`, `EFUNFL`, `EFINEXCT`.
- `operation` — one of `_OP_ADDITION`, `_OP_SUBTRACTION`, `_OP_COMPARISON`, `_OP_EQUALITY`,
  `_OP_LESS_THAN`, `_OP_LARGER_THAN`, `_OP_MULTIPLICATION`, `_OP_DIVISION`, `_OP_CONVERSION`.
- `source_format`, `destination_format` — one of `_TYPE_SIGNED_CHARACTER`, `_TYPE_UNSIGNED_CHARACTER`,
  `_TYPE_SIGNED_SHORT_INTEGER`, `_TYPE_UNSIGNED_SHORT_INTEGER`, `_TYPE_SIGNED_INTEGER`,
  `_TYPE_UNSIGNED_INTEGER`, `_TYPE_SIGNED_LONG_INTEGER`, `_TYPE_UNSIGNED_LONG_INTEGER`, `_TYPE_FLOAT`,
  `_TYPE_DOUBLE`.
- `operand1` (left of binary / right of unary), `operand2` (right of binary), `result` — all of type
  `_fp_value_union_t`:
```c
typedef union _fp_value_union_t {
    char c; unsigned char uc; short s; unsigned short us;
    int i; unsigned int ui; long l; unsigned long ul; float f;
#if ! _SINGLE_FP
    double d;
#endif
} _fp_value_union_t;
```

**Exception type flag codes (Table 1.6.5.1):**

| Error Description | Exception Flag | Default Result with Trapping |
|-------------------|----------------|------------------------------|
| Invalid Operation | `EFINVOP` | NaN |
| Division by zero | `EFDIVZ` | +INF or -INF |
| Overflow | `EFOVFL` | +INF or -INF |
| Underflow | `EFUNFL` | zero |
| Inexact | `EFINEXCT` (text also prints `EFINEXT`) | undefined |

(`INF` = largest absolute FP number, `-INF < every finite number < +INF`; `NaN` = symbolic non-number.)
Specify `EFALL` to cover all exception types.

### 1.6.6 Floating Point Libraries

Link **after** the C library and **before** the run-time library. Arithmetic routines (`sin`, `cos`, …)
are in the **C library**, not these; only basic FP operations are here.

| Compiler Model | No trapping | Trapping |
|----------------|-------------|----------|
| Small (default) | `libfps.a` (default) | `libfpst.a` |
| Compact code | `libfpc.a` | `libfpct.a` |
| Compact data | `libfpd.a` | `libfpdt.a` |
| Large | `libfpl.a` | `libfplt.a` |

FP headers: `<float.h>` (constants + trap API), `<math.h>` (`acos, asin, atan, atan2, ceil, cos, cosh,
exp, fabs, floor, fmod, frexp, ldexp, log, log10, modf, pow, sin, sinh, sqrt, tan, tanh`), `<time.h>`
(`difftime`, skeleton). §1.6.6.1 lists conventional ANSI `<math.h>` routine descriptions (pages 94–96),
not S1C88-specific.

---

## ABI Quick-Reference (for the SDCC retarget)

| Topic | Value |
|-------|-------|
| Endianness | **Little Endian** (MSB at higher address) |
| `char` / `short` / `int` / `long` | 1 / 2 / 2 / 4 bytes; `enum` = 2 |
| `_near` pointer | **2 bytes (16-bit)** |
| `_far` pointer | **3 bytes (24-bit)** |
| `float` / `double` | both **4 bytes**, single precision |
| Memory models | small(s)/compact-code(c)/compact-data(d)/large(l); selected by `-M`; default small |
| Return-addr size | 2 bytes (small, compact-code) / 3 bytes (compact-data, large) |
| Default storage | `_near` (small, compact-data) / `_far` (large, compact-code) |
| Reentrancy | all programs reentrant by default; static functions opt-in |
| Param passing | **registers first**, overflow + structs/unions + varargs → stack (reverse order) |
| `char` arg regs | `A, L, YP, XP, H, B` (descending priority) |
| `int` arg regs | `BA, HL, IX, IY` |
| `long` arg regs | pairs `HLBA, IYIX` |
| near-pointer arg regs | `IY, IX, HL, BA` |
| far-pointer arg regs | pairs `IYP, IXP, HLP` (`IYP=IY+YP`, `IXP=IX+XP`, `HLP=HL+A`) |
| Return: char | `A` |
| Return: int/short | `BA` |
| Return: long | `HLBA` (`HL`=high, `BA`=low) |
| Return: pointer | `HLP` (`HL+A`) |
| Return: float/double | `HLBA` |
| Return: struct/union | on the stack |
| Call convention | **caller-saves** ("saved by caller") |
| Frame/stack pointers | `$fp` (frame), `$sp` (stack); stack grows **down**, max 64K |
| Interrupt fn | declared `_interrupt(vector)`; void/void; saves all clobbered regs; ends with **`RETE`**; 2-byte vector entry via `DW` in an absolute `CODE AT n` section |
| Symbol decoration | C name → asm `_name`; run-time lib → `__name` (two underscores) |
| Stack labels | `__lc_bs` / `__lc_es` (begin/end) |
| Heap labels | `__lc_bh` / `__lc_eh` (begin/end) |
| Entry / exit | `_START` (reset), `_main`, `__exit` (ends in `SLP`) |

**Section names:** `.text`/`.text_function` (code), `.comm` (`_common`/interrupt code), `.nbss`/`.fbss`
(cleared near/far bss), `.nbssnc`/`.fbssnc` (non-cleared near/far), `.ndata`/`.fdata` (init near/far),
`.nrdata`/`.frdata` (const near/far).
