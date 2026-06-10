# The MINX container format (`.minx`)

`romgen`'s second output format: a single binary that carries everything a
**debugging emulator** needs — the bootable ROM image, symbols, memory areas, a
source-line table, function extents, and the source files themselves — in one
file. It is designed for a consumer that has **no filesystem access** and
**parses no structured text**: `romgen` does all the text parsing on the host
(the linker's `.noi`/`.map`, sdcc's `--debug` `.cdb`) and promotes the results
to sorted binary tables; the only text inside is *display-only* (source code,
names) and is reached through binary indices.

```bash
sdcc -ms1c88 --debug game.c -o game.ihx
romgen game.ihx game.min     # flat ROM, what emulators/flash carts consume
romgen game.ihx game.minx    # MINX: ROM + symbols + lines + functions + sources
```

Container mode is selected by a `.minx` output extension or `--minx`.
`--far=start-end` works exactly as in `.min` mode and also informs every
physical-address field in the debug tables.

Inputs are **auto-discovered** next to the input by swapping the `.ihx`
extension: `game.noi` (symbols; the sdcc driver always passes `-j`), `game.map`
(areas; `-m` always passed), and `game.cdb` (line/function records; present when
compiled with `--debug`). Missing inputs just mean the corresponding chunks are
absent. Explicit paths override discovery and then *must* exist. Source files
are located by their `.cdb`-recorded names — tried as-is, next to the `.ihx`,
then under each `--srcdir=` — and embedded verbatim (`--no-src` to skip).

```
romgen in.ihx out.minx [--minx] [--far=start-end]...
       [--map=file] [--noi=file] [--cdb=file]
       [--srcdir=dir]... [--no-src] [--embed=name=file]...
```

Producer: `tools/romgen.c`. **Reference reader / validator: `tools/minxdump.c`**
(`minxdump file.minx` validates + dumps; `--rom=out.min` extracts the flat ROM).
Tests: `scripts/rom-smoke.sh` (banked/far mappings) and `scripts/minx-smoke.sh`
(C-level debug info end-to-end).

## Design

- **Chunk tree** (IFF/RIFF-style): the file is a sequence of chunks; a chunk is
  a 4-byte ASCII id + u32 payload size + payload. *Container* chunks hold a
  sequence of child chunks — nesting and repetition are first-class, which is
  the extensibility story: new chunk types and new children can be added
  anywhere, and readers skip what they don't know by size.
- **Tree for structure, flat tables for lookups.** Anything a debugger touches
  per-step (PC→line, PC→function, address→symbol) is a contiguous array of
  fixed 16-byte records **sorted by address** — one binary search, no tree walk,
  no allocation. The tree carries the repeating/nested payloads (per-file source
  text, future per-scope variables).
- **Little-endian throughout**, matching the S1C88 and the rest of the toolchain.
- **Deterministic** — no timestamps; same inputs → byte-identical output.
- **Self-validating** — the header carries the total file size and a CRC-32
  (polynomial `0xEDB88320`, i.e. zlib `crc32()`) of everything after the header.
- **One-buffer consumable** — load the file, walk in place; `minxdump.c` shows
  the complete pattern in ~100 lines of plain C.

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
- **Container** chunks (`SRCS`, `FILE`) hold a chunk sequence as their payload.
  Container-ness is defined per id (like RIFF's `LIST`), not flagged.
- Chunk order is not guaranteed (today's writer emits `ROM` `AREA` `SYM` `LINE`
  `FUNC` `SRCS` `USR`* `NOTE` `STR`); select by id. Unknown ids must be skipped.
  A reader should locate `STR ` first — every `name` field below is an offset
  into it.

### Chunk catalog

| Id | Kind | Payload |
|---|---|---|
| `ROM ` | leaf | `u32 load_addr` (= `0x2100`) + the flat ROM image, **byte-identical to the `.min` output** (gaps `0xFF`) |
| `STR ` | leaf | concatenated NUL-terminated strings; offset 0 is always `""`; last byte is NUL |
| `SYM ` | leaf | symbol records[16 B], sorted by `value` |
| `AREA` | leaf | linker area records[16 B] |
| `LINE` | leaf | **the global line table**: records[16 B] sorted by physical address |
| `FUNC` | leaf | function-extent records[16 B] sorted by entry |
| `SRCS` | container | one `FILE` child per source file, in **file-id order** |
| `FILE` | container | children: `NAME` (leaf: the path as recorded by the compiler, raw bytes, no NUL) and optionally `TEXT` (leaf: the full source text, verbatim, display-only) |
| `USR ` | leaf | `u32 name` + raw bytes — one per `--embed=name=file` |
| `NOTE` | leaf | metadata pairs `{u32 key, u32 val}` (both `STR ` offsets) |

All record arrays use fixed 16-byte records, so `count = size / 16` and the
arrays are binary-searchable in place.

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

ROM file offset = `phys − 0x2100`. `LINE`/`FUNC` store physical addresses only
(they describe ROM bytes); `SYM` stores both.

## Record layouts (all fields u32, little-endian)

**`SYM `** — `{ name, value, phys, flags }`, sorted by `value` (ties by name
offset). From the linker's NoICE `DEF`/`DEFS` records, so asm-level names
(`_main`, `s__CODE`, library internals) with underscore prefixes.
`flags` bit 0 (`SYM_ROM`): `phys` is valid — the symbol maps into cart ROM
(else `phys` = `0xFFFFFFFF`: RAM at `0x1000-0x1FFF`, absolutes, …).
`flags` bit 1 (`SYM_LOCAL`): scoped symbol (NoICE `DEFS`).

**`AREA`** — `{ name, base, size, flags }` in `.map` order. `base` is a linker
address. `flags`: bit 0 ABS, bit 1 OVR. Best-effort from the map's area table —
gives a debugger the memory-region names (`_CODE`, `_DATA`, `_FAR`, …).

**`LINE`** — `{ phys, file, line, reserved }`, sorted by (`phys`, `line`).
`file` is an ordinal into the `SRCS` children (`0xFFFFFFFF` = none). Built from
the `.cdb`'s linker-resolved `L:C$file$line$…:addr` records. Several records may
share one `phys` (e.g. a loop head); for PC→line take the **last** record with
`phys ≤ PC`; for breakpoints scan for (`file`,`line`) and take its first `phys`.

**`FUNC`** — `{ name, entry, end, file }`, sorted by `entry`; addresses
physical. From `.cdb` `F:G$` (is-a-function) + `L:G$` (entry) + `L:XG$` (exit
label — SDCC emits it at the function's final return instruction, so code lies
in `[entry, end]`). `name` is the **C-level** name (no underscore prefix —
`main`, not `_main`). `file` = the file of the first line record inside the
extent, `0xFFFFFFFF` if none.

**`NOTE`** — `{ key, val }` string pairs. Current keys (tolerate unknown keys,
any order, repeats): `generator`, `source` (the input path as given),
`cart-base`, `rom-bytes`, `banks`, `symbols`, `lines`, `functions`, and one
`far` per `--far` range. Informational only.

## What a debugger does with it

| Operation | Lookup |
|---|---|
| source display | `SRCS` → `FILE[i]` → `TEXT` (split on `\n` at load) |
| PC → source line | binary-search `LINE` by `phys` (last record ≤ PC) |
| PC → function | binary-search `FUNC` by `entry` (last record with `entry ≤ PC ≤ end`) |
| breakpoint at file:line | scan `LINE` for (`file`,`line`), first `phys` |
| step-over / step-out | `FUNC` extents |
| address ↔ symbol | binary-search `SYM` |
| memory-region names | `AREA` |

Everything above is index arithmetic over one loaded buffer.

## Extensibility / reserved

Planned (not yet emitted) — readers that skip unknown chunks are already
compatible:

- **`TYPE` + per-scope variable chunks** — the `.cdb`'s `S:`/`T:` records
  (variable locations: register/stack/static; struct layouts) promoted to
  binary, enabling watch-window inspection. This is the next fidelity tier.
- **Asm-level line table** — the `.cdb` `L:A$module$listingline:addr` records,
  for source-stepping hand-written assembly.

Versioning policy: additive changes (new chunk ids, new children inside
containers, new `NOTE` keys, new flag bits) do **not** bump the header version;
layout changes to existing records do.

## Why this shape (and not ELF/DWARF)?

ELF could carry the ROM and symbols but everything that matters here — the
dual linker/physical address model, the line/function tables, embedded sources
— would land in private vendor sections needing custom readers anyway, plus a
psABI the S1C88 doesn't have. DWARF line programs are a bytecode interpreter a
4 KiB-RAM-era console emulator shouldn't need. A chunk tree + sorted fixed
records keeps the *familiar* parts of that shape (typed sections, string table,
binary-searchable tables) at a complexity an afternoon of C can fully implement
— `minxdump.c` is the existence proof.
