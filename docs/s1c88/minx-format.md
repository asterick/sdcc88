# The MINX container format (`.minx`)

`romgen`'s second output format: a single binary that carries everything a
**debugging emulator** needs — the ROM content as sparse segments, symbols,
memory areas, a source-line table, function extents, a full type graph,
variable locations, and the source files themselves — in one file. It is
designed for a consumer that has **no filesystem access** and **parses no
structured text**: `romgen` does all the text parsing on the host (the
linker's `.noi`/`.map`, sdcc's `--debug` `.cdb`) and promotes the results to
sorted binary tables; the only text inside is *display-only* (source code,
names) and is reached through binary indices.

```bash
sdcc -ms1c88 --debug game.c -o game.ihx
romgen game.ihx game.min     # flat ROM, what emulators/flash carts consume
romgen game.ihx game.minx    # MINX: segments + symbols + lines + funcs + types + vars + sources
```

Container mode is selected by a `.minx` output extension or `--minx`.
`--far=start-end` works exactly as in `.min` mode and also informs every
physical-address field in the debug tables.

Inputs are **auto-discovered** next to the input by swapping the `.ihx`
extension: `game.noi` (symbols; the sdcc driver always passes `-j`), `game.map`
(areas; `-m` always passed), and `game.cdb` (line/function/type/variable
records; present when compiled with `--debug`). Missing inputs just mean the
corresponding chunks are absent. Explicit paths override discovery and then
*must* exist. Source files are located by their `.cdb`-recorded names — tried
as-is, next to the `.ihx`, then under each `--srcdir=` — and embedded verbatim
(`--no-src` to skip).

```
romgen in.ihx out.minx [--minx] [--far=start-end]...
       [--map=file] [--noi=file] [--cdb=file]
       [--srcdir=dir]... [--no-src] [--embed=name=file]...
```

Producer: `tools/romgen.c`. **Reference reader / validator: `tools/minxdump.c`**
(`minxdump file.minx` validates + dumps; `--rom=out.min` reconstructs the flat
ROM byte-identically). Tests: `scripts/rom-smoke.sh` (banked/far mappings) and
`scripts/minx-smoke.sh` (C-level debug info end-to-end). `examples/hello`
builds a `.minx` by default.

## Design

- **Chunk tree** (IFF/RIFF-style): the file is a sequence of chunks; a chunk is
  a 4-byte ASCII id + u32 payload size + payload. *Container* chunks hold a
  sequence of child chunks — nesting and repetition are first-class, which is
  the extensibility story: new chunk types and new children can be added
  anywhere, and readers skip what they don't know by size.
- **Tree for structure, flat tables for lookups.** Anything a debugger touches
  per-step (PC→line, PC→function, address→symbol) is a contiguous array of
  fixed-stride records **sorted by address** — one binary search, no tree walk,
  no allocation. The tree carries the repeating/nested payloads (ROM segments,
  per-file source text).
- **Sparse ROM.** The ROM is stored as the byte runs the program actually
  defines (`SEG` records), not a flat `0xFF`-padded image — file size is
  proportional to content (a mostly-empty multi-bank layout costs nothing),
  and the emulator knows exactly which cart addresses are *defined*, so reads
  from never-written ROM can be flagged as bugs. The flat `.min` remains
  derivable: fill `0xFF`, copy each segment (that is what `minxdump --rom=`
  does, byte-identically).
- **Little-endian throughout**, matching the S1C88 and the rest of the toolchain.
- **Deterministic** — no timestamps; same inputs → byte-identical output.
- **Self-validating** — the header carries the total file size and a CRC-32
  (polynomial `0xEDB88320`, i.e. zlib `crc32()`) of everything after the header.
- **One-buffer consumable** — load the file, walk in place; `minxdump.c` shows
  the complete pattern in plain C.

## File layout

### Header (16 bytes)

| Offset | Size | Field | Value / meaning |
|---|---|---|---|
| 0 | 4 | magic | ASCII `"MINX"` |
| 4 | 2 | version | currently **1** |
| 6 | 2 | reserved | 0 |
| 8 | 4 | file size | total bytes, header included |
| 12 | 4 | crc32 | CRC-32 of every byte after this 16-byte header |

### Chunks

```
chunk := { char id[4];  u32 size; }  payload[size]  pad-to-4
```

- `size` counts the payload only. The pad (zero bytes to the next 4-byte
  boundary) is **not** in `size` but **is** part of the enclosing container/file
  — iterate children with `next = align4(payload_end)`.
- Top level: the region from offset 16 to end-of-file is a chunk sequence.
- **Container** chunks (`ROM `, `SRCS`, `FILE`) hold a chunk sequence as their
  payload. Container-ness is defined per id (like RIFF's `LIST`), not flagged.
- Chunk order is not guaranteed (today's writer emits `ROM ` `AREA` `SYM `
  `TYPE` `STRU`* `LINE` `FUNC` `VAR ` `SRCS` `USR `* `NOTE` `STR `); select by
  id. Unknown ids must be skipped. A reader should locate `STR ` (names),
  `TYPE` and `FUNC` (cross-referenced indices) up front.

### Chunk catalog

| Id | Kind | Payload |
|---|---|---|
| `ROM ` | container | repeating `SEG ` children, ascending, non-overlapping |
| `SEG ` | leaf | `u32 phys` + the defined ROM bytes starting at that physical address |
| `STR ` | leaf | concatenated NUL-terminated strings; offset 0 is always `""`; last byte is NUL |
| `SYM ` | leaf | symbol records [16 B], sorted by `value` |
| `AREA` | leaf | linker area records [16 B] |
| `TYPE` | leaf | type-graph records [16 B]; referenced by index |
| `STRU` | leaf, repeating | one per struct/union: header + member records; the Nth `STRU` chunk in file order is struct **ordinal** N |
| `LINE` | leaf | **the global line table**: records [16 B] sorted by physical address |
| `FUNC` | leaf | function records [32 B] sorted by entry |
| `VAR ` | leaf | variable records [32 B] sorted by (scope function, name) |
| `IO  ` | leaf | hardware register map records [16 B], sorted by address |
| `SRCS` | container | one `FILE` child per source file, in **file-id order** |
| `FILE` | container | children: `NAME` (leaf: the path as recorded by the compiler, raw bytes, no NUL) and optionally `TEXT` (leaf: the full source text, verbatim, display-only) |
| `USR ` | leaf | `u32 name` + raw bytes — one per `--embed=name=file` |
| `NOTE` | leaf | metadata pairs `{u32 key, u32 val}` (both `STR ` offsets) |

## The address model

Tables carry **two address spaces** where relevant:

- `value` — the 24-bit **linker address**: banked code at `(bank<<16)|logic`,
  `__far` data and RAM at their natural addresses.
- `phys` — the **physical cartridge address** (what the data bus sees), per the
  same mapping `romgen` applies to the ROM bytes, `--far` included:

```
in a --far range          : phys = value
bank 0 (value < 0x10000)  : phys = value                       (common bank)
bank N (value ≥ 0x10000)  : phys = N*0x8000 + (value & 0x7FFF)
```

Flat-ROM file offset = `phys − 0x2100`. `SEG`/`LINE`/`FUNC` store physical
addresses (they describe ROM bytes); `SYM` stores both; `VAR` static locations
store the **bus address** (physical for ROM-mapped const data; RAM addresses
`0x1000-0x1FFF` unchanged).

## Record layouts (fields u32, little-endian)

**`SYM `** [16 B] — `{ name, value, phys, flags }`, sorted by `value` (ties by
name offset). From the linker's NoICE `DEF`/`DEFS` records, so asm-level names
(`_main`, `s__CODE`, library internals) with underscore prefixes.
`flags` bit 0 (`SYM_ROM`): `phys` valid — maps into cart ROM (else `phys` =
`0xFFFFFFFF`). `flags` bit 1 (`SYM_LOCAL`): scoped symbol (NoICE `DEFS`).

**`AREA`** [16 B] — `{ name, base, size, flags }` in `.map` order. `base` is a
linker address. `flags`: bit 0 ABS, bit 1 OVR. Best-effort from the map's area
table — gives a debugger the memory-region names (`_CODE`, `_DATA`, `_FAR`, …).

**`TYPE`** [16 B] — `{ kind_flags, size, target, extra }`, deduplicated;
everything that mentions a type does so by index into this table. `size` is
the storage size in bytes. `kind_flags` low byte:

| kind | meaning | `target` | `extra` |
|---|---|---|---|
| 0 | void | — | — |
| 1 | char | — | — |
| 2 | short | — | — |
| 3 | int | — | — |
| 4 | long | — | — |
| 5 | float | — | — |
| 6 | sbit | — | — |
| 7 | bitfield | — | bit offset \| (bit count << 16) |
| 8 | struct/union | — | `STRU` ordinal (`0xFFFFFFFF` if unresolved) |
| 9 | array | element type | element count |
| 10 | function | return type | — |
| 11 | pointer | pointee type | the cdb space letter (`'X'` = `__far`, 3 bytes; near pointers 2 bytes; function/code pointers 3) |

`kind_flags` bit 8: unsigned. Unused `target`/`extra` are `0xFFFFFFFF`/`0`.

**`STRU`** — payload `{ u32 name, u32 size, u32 member_count }` followed by
member records [16 B] `{ name, byte_offset, type, reserved }` in declaration
order. Unions are structs whose members share offsets; bitfield members carry
their bit offset/width in their *type* (kind 7). Struct **ordinal** = position
among `STRU` chunks in file order; `TYPE` kind-8 records reference it.

**`LINE`** [16 B] — `{ phys, file, line, scope }`, sorted by (`phys`, `line`).
`file` is an ordinal into the `SRCS` children (`0xFFFFFFFF` = none). `scope` =
lexical block (low 16 bits) | scope level (bits 16-23) | sub-level (bits
24-31), matching the `VAR` level/block fields so a debugger can prefer
variables of the innermost live block. Built from the `.cdb`'s
linker-resolved `L:C$file$line$level$block:addr` records. Several records may
share one `phys` (e.g. a loop head); for breakpoints scan for (`file`,`line`)
and take its first `phys`.

**The line-coverage rule (normative).** A line record covers the half-open
range from its `phys` to the next record's `phys`, **clamped to the enclosing
`FUNC` extent `[entry, end]`** — and a PC that lies inside no `FUNC` extent is
on **no** source line. The clamp matters: C functions interleave in the
address space with library/runtime code that has no line records, so a naive
"last record ≤ PC" would attribute runtime addresses to the previous
function's final line. PC→line is therefore: binary-search `FUNC` (last
`entry ≤ PC`, require `PC ≤ end`), then binary-search `LINE` (last record with
`phys ≤ PC`) and require the found record to lie inside the same extent.
*Step into* under a source-line-only model is "run until the PC is covered" —
code with no coverage (crt0, `s1c88.lib`, `__asm` blocks) is stepped over by
construction; `SYM` still names those regions for status display.

**`FUNC`** [32 B] — `{ name, entry, end, file, rettype, frame, flags, rsv }`,
sorted by `entry`; addresses physical. From `.cdb` `F:` records plus the
linker's `L:`/`L:X` bindings (the exit label sits at the function's final
return instruction, so code lies in `[entry, end]`). `name` is the **C-level**
name (no underscore prefix). `rettype` is a `TYPE` index. `frame` is the stack
frame size in bytes. `flags`: bit 0 file-static, bit 1 interrupt handler,
bit 2 **frame established** (romgen verified the entry bytes are the prologue
`push ix ; ld ix,sp` = `A2 CF FA`); bits 8-15 interrupt number; bits 16-23
register bank.

**Backtraces (the frame walk).** The Pokémon Mini runs the S1C88 in MAXIMUM
mode: every call pushes a 3-byte return frame (`CB`, then `PCH`, then `PCL`;
the stack grows down). A function whose `FUNC` record has flag bit 2 set has,
after its prologue, this layout relative to IX:

```
[IX+0..IX+1]  caller's IX (little-endian u16)
[IX+2]        return PCL      } the return address: PC = PCH:PCL,
[IX+3]        return PCH      } bank = CB — linker-style address
[IX+4]        return CB       } (CB<<16)|PC when PC ≥ 0x8000, else PC
[IX+5...]     caller-pushed arguments
```

Unwind: map the return address to physical (the standard mapping above), look
it up in `FUNC`/`LINE` for the caller's frame row, then continue with
`IX ← u16[IX]`. Stop when the return address leaves every `FUNC` extent (the
crt0/BIOS boundary) or the flag-bit-2 chain breaks — frameless leaf functions
(no flag) can appear only as the innermost frame, where the debugger still
knows the PC directly.

**`VAR `** [32 B] — `{ name, scope, type, levelblock, loc_kind, loc, flags,
rsv }`, sorted by (`scope`, `name` offset) with globals (`scope` =
`0xFFFFFFFF`) last — so a function's variables are one contiguous,
binary-searchable slice. `scope` is the `FUNC` table index of the owning
function. `type` is a `TYPE` index. `levelblock` = scope level (low 8) |
sub-level (bits 8-15) | lexical block (bits 16-31), for disambiguating
same-named variables in sibling blocks against `LINE.scope`. `flags`: bit 0
file-static; bit 1 **artificial** (a compiler spill temporary, `sloc<N>` —
full fidelity is preserved but UIs should hide these by default); bits 8-15
the raw cdb address-space letter. Location:

| `loc_kind` | meaning | `loc` |
|---|---|---|
| 0 | none (optimized out / unknown) | 0 |
| 1 | static | the bus address (see address model) |
| 2 | stack | signed offset (two's complement) **relative to IX**, the frame pointer |
| 3 | registers | `STR ` offset of the register list, e.g. `"a"`, `"b,a"`, `"hl"` |

**The frame model:** sdcc88 functions with stack frames execute `push ix ;
ld ix,sp` in the prologue; all `loc_kind 2` offsets are relative to that IX
value (parameters spilled by the callee and locals are both negative). A
debugger halted inside the function body computes `IX + (int32_t)loc`.

**`IO  `** [16 B] — `{ name, addr, size, flags }`, sorted by address: the
hardware register map, so watch windows can name the SFR space (`PRC_MODE`,
`IRQ_ENA1`, `KEY_PAD`, …) without any device knowledge. Parsed at pack time
from the toolchain's `<pm.h>` (found relative to the `romgen` binary;
`--io=file` overrides, `--no-io` omits). Aliases are real: a 16-bit register
and its `_L`/`_H` byte halves each get a record at the same address.

**`NOTE`** — `{ key, val }` string pairs. Current keys (tolerate unknown keys,
any order, repeats): `generator`, `source` (the input path as given),
`cart-base`, `rom-bytes` (flat extent), `rom-crc32` (**CRC-32 of the
SEG-reconstructed flat ROM — i.e. of the matching `.min` file**; an emulator
handed a `.min` and a `.minx` separately can verify they are the same build),
`io-regs`, `banks`, `segments`, `symbols`, `lines`, `functions`, `variables`,
`types`, and one `far` per `--far` range.

## What a debugger does with it

| Operation | Lookup |
|---|---|
| load ROM | memset `0xFF`, copy each `SEG` (and flag reads of undefined ROM) |
| source display | `SRCS` → `FILE[i]` → `TEXT` (split on `\n` at load) |
| PC → source line | binary-search `FUNC` then `LINE` (the line-coverage rule above) |
| PC → function | binary-search `FUNC` by `entry` (last record with `entry ≤ PC ≤ end`) |
| breakpoint at file:line | scan `LINE` for (`file`,`line`), first `phys` |
| step-over / step-out | `FUNC` extents |
| backtrace | the frame walk above (`FUNC` flag bit 2 + the IX frame layout) |
| locals at PC | PC → `FUNC` index → the `VAR` slice with that `scope` (hide bit-1 artificials) |
| read a variable | `loc_kind`: memory at `loc` / `IX+loc` / named registers; format via `TYPE`/`STRU` |
| watch a struct member | `VAR.type` → kind 8 → `STRU` ordinal → member offset/type |
| address ↔ symbol | binary-search `SYM` |
| name an SFR access | binary-search `IO` |
| memory-region names | `AREA` |
| match a loose `.min` | compare its CRC-32 to the `rom-crc32` NOTE key |

Everything above is index arithmetic over one loaded buffer.

## Fidelity notes / limitations

- Variable records cover exactly what sdcc's `.cdb` emits: parameters are not
  distinguished from locals, there are no live ranges (a register variable's
  listed registers are where it *predominantly* lives), and compiler spill
  temporaries appear as `sloc0…` names. `const` arrays in code space get no
  `S:` record from sdcc, so they appear in `SYM` but not `VAR`.
- Local-scope resolution matches variables to functions by function name; two
  *static* functions with the same name in different modules would collide
  (their variables merge under one `FUNC` entry).
- `AREA` is a best-effort parse of the human-oriented `.map` area table.

## Extensibility / reserved

Readers that skip unknown chunks are forward-compatible. The debugging model
is deliberately **C-source-line granularity**: an asm-level line table (the
`.cdb` `L:A$…` records + embedded listings) was considered and dropped — code
without line coverage is stepped over by the coverage rule, which keeps the
container free of listing text. It can return as a new chunk id if the model
ever changes. Versioning policy: additive changes (new chunk ids, new children
inside containers, new `NOTE` keys, new flag bits) do **not** bump the header
version; layout changes to existing records do.

## Why this shape (and not ELF/DWARF)?

ELF could carry the ROM and symbols but everything that matters here — the
dual linker/physical address model, the line/function/variable tables, embedded
sources — would land in private vendor sections needing custom readers anyway,
plus a psABI the S1C88 doesn't have. DWARF expressions are a bytecode
interpreter a 4 KiB-RAM-era console emulator shouldn't need; this target's
location vocabulary is exactly three cases (static address, IX-relative,
registers). A chunk tree + sorted fixed records keeps the *familiar* parts of
that shape (typed sections, string table, binary-searchable tables) at a
complexity an afternoon of C can fully implement — `minxdump.c` is the
existence proof.
