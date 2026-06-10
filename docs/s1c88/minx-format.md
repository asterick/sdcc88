# The MINX container format (`.minx`)

`romgen`'s second output format: a single sectioned binary — deliberately ELF-like in
shape — that rolls **everything a build produces** into one file: the bootable ROM
image, a parsed binary symbol table, and the link-time debug artifacts (`.map`,
`.noi`, `.cdb`) embedded verbatim. A flat `.min` is exactly what the console's data
bus sees and nothing else; the `.minx` is the same ROM **plus** the metadata an
emulator, debugger, symbol server, or archive needs, in a single distributable
artifact.

```bash
sdcc -ms1c88 --debug game.c -o game.ihx
romgen game.ihx game.min     # flat ROM, what emulators/flash carts consume
romgen game.ihx game.minx    # MINX container: ROM + symtab + map/noi/cdb
```

The container mode is selected by an output path ending in `.minx`, or explicitly
with `--minx` (any output name). `--far=start-end` works exactly as in `.min` mode
and also informs the symbol table's physical-address mapping.

Sidecar artifacts are **auto-discovered** next to the input by swapping the `.ihx`
extension: `game.map` (linker map, from `sdldz80 -m` — the sdcc driver always passes
it), `game.noi` (NoICE symbols, from `-j` — also always passed), and `game.cdb`
(SDCC debug records, present when compiled with `--debug`). Missing sidecars are
skipped silently; their sections are simply absent. Explicit paths override
discovery and then *must* exist:

```
romgen game.ihx game.minx [--far=start-end]...
       [--map=file] [--noi=file] [--cdb=file] [--embed=name=file]...
```

`--embed=name=file` (repeatable, up to 32) adds an arbitrary file as a *user
section* — asset manifests, build IDs, source archives, anything.

Producer: `tools/romgen.c`. Round-trip test: `scripts/rom-smoke.sh` (the MINX leg).

## Design notes

- **Little-endian throughout** — matching the S1C88 itself and the byte order the
  rest of the toolchain writes. All multi-byte fields are unsigned little-endian.
- **Deterministic** — no timestamps or host paths beyond the `source=` name passed
  on the command line. Rebuilding the same `.ihx` with the same flags produces a
  byte-identical `.minx`.
- **Self-validating** — every section carries a CRC-32 of its payload.
- **Extraction-friendly** — the flat `.min` is recoverable with a single
  offset/size slice (the `ROM` section payload is byte-identical to `.min` output),
  and the text artifacts are stored verbatim, so no information is lost by bundling.
- **Extensible** — readers must ignore section types they don't know, the header
  and section-entry sizes are self-described (and may grow in later versions), and
  reserved fields are written as zero.

## File layout

```
+--------------------------+  offset 0
| header (32 bytes)        |
+--------------------------+  offset 32
| section table            |  section_count × 32-byte entries
+--------------------------+
| section payloads         |  in table order, each 4-byte aligned
+--------------------------+
```

### Header (32 bytes)

| Offset | Size | Field | Value / meaning |
|---|---|---|---|
| 0 | 4 | `magic` | ASCII `"MINX"` (`4D 49 4E 58`) |
| 4 | 2 | `version` | format version, currently **1** |
| 6 | 2 | `header_size` | size of this header, **32** |
| 8 | 4 | `section_count` | number of section-table entries |
| 12 | 4 | `table_offset` | file offset of the section table (= `header_size`) |
| 16 | 4 | `entry_size` | size of one table entry, **32** |
| 20 | 4 | `strtab_index` | index (into the table) of the `STRTAB` section |
| 24 | 4 | `file_size` | total file size in bytes (integrity check) |
| 28 | 4 | `flags` | reserved, **0** |

Readers should locate the table via `table_offset`/`entry_size` rather than assuming
the constants, so a later version can grow either structure compatibly.

### Section-table entry (32 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `type` | section type (below) |
| 4 | 4 | `name` | offset of a NUL-terminated name in the `STRTAB` section |
| 8 | 4 | `offset` | file offset of the payload (4-byte aligned) |
| 12 | 4 | `size` | payload size in bytes (alignment padding excluded) |
| 16 | 4 | `addr` | load address: `0x2100` for `ROM`, `0` otherwise |
| 20 | 4 | `crc32` | CRC-32 (IEEE 802.3, polynomial `0xEDB88320`, i.e. zlib `crc32()`) of the payload |
| 24 | 8 | — | reserved, zero |

### Section types

| Type | Name | Payload |
|---|---|---|
| `1` | `ROM` | the flat ROM image — **byte-identical to the `.min` output**; `addr = 0x2100` (byte 0 of the payload is physical `0x2100`, the cart header; unused gaps are `0xFF`) |
| `2` | `SYMTAB` | binary symbol records, 16 bytes each (below); present when a `.noi` was found |
| `3` | `STRTAB` | concatenated NUL-terminated strings; offset 0 is always the empty string `""` |
| `4` | `NOTE` | build metadata, `key=value` text lines (below) |
| `5` | `MAP` | the linker `.map`, verbatim (areas, per-module placement, full symbol listing) |
| `6` | `NOI` | the linker NoICE `.noi`, verbatim (`DEF`/`DEFS`/`FILE`/`FUNC`/`LINE` records — this is where source-line info lives) |
| `7` | `CDB` | the SDCC `.cdb`, verbatim (C-level symbol/type/scope debug records; see the SDCC manual's CDB chapter) |
| `0x100`+ | *(user)* | `--embed=name=file` payloads, verbatim; types are assigned `0x100, 0x101, …` in command-line order, names come from the `--embed` argument |

Section order in the file is not guaranteed — readers must select by `type` (and/or
`name`), not by position. The current writer emits `ROM`, `SYMTAB`, `MAP`, `NOI`,
`CDB`, user sections, `NOTE`, `STRTAB`, skipping absent ones. Unknown types must be
ignored (skip via `offset`/`size`).

## The symbol table (`SYMTAB`)

Parsed from the linker's NoICE output (`DEF` = global, `DEFS` = scoped/local) into
fixed 16-byte records, **sorted by `value` ascending** (binary-search friendly; ties
broken by name offset):

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `name` | `STRTAB` offset of the NUL-terminated symbol name |
| 4 | 4 | `value` | the 24-bit **linker address** — for banked code this is `(bank<<16)\|logic`, for `__far` data the physical address, for RAM the RAM address |
| 8 | 4 | `phys` | the **physical cartridge address** (what the data bus sees), per the same mapping `romgen` applies to the ROM bytes, `--far` ranges included; `0xFFFFFFFF` when the symbol does not map into cart ROM |
| 12 | 4 | `flags` | bit 0 (`SYM_ROM`): `phys` is valid — the symbol maps into cart ROM at `phys` (ROM file offset = `phys - 0x2100`). bit 1 (`SYM_LOCAL`): scoped symbol (a NoICE `DEFS` record). Other bits reserved, zero. |

The `value`→`phys` mapping is the address model documented in
[`banked-branch.md`](banked-branch.md) and `abi-decision.md`:

```
in a --far range          : phys = value                      (physical convention)
bank 0 (value < 0x10000)  : phys = value                      (common bank)
bank N (value ≥ 0x10000)  : phys = N*0x8000 + (value & 0x7FFF)
```

RAM symbols (`_DATA` at `0x1000-0x1FFF`), absolute constants, and anything else
below the cart base keep their linker `value` but get `phys = 0xFFFFFFFF`,
`SYM_ROM` clear.

Symbol count = `size / 16`. C-level type/scope information for these symbols is in
the `CDB` section; source-line records are in the `NOI` section.

## The note section (`NOTE`)

UTF-8 text, one `key=value` per `\n`-terminated line. Current keys (readers must
tolerate unknown keys and any order):

```
format=minx1                  format name + version
generator=romgen (sdcc88)     producing tool
source=game.ihx               the input path as given on the command line
cart-base=0x2100              physical address of ROM payload byte 0
rom-bytes=2918                ROM section payload size
banks=1                       32 KiB banks touched by the image
symbols=167                   SYMTAB record count (0 if no .noi was found)
far=0x18800-0x18fff           one line per --far range, in CLI order (absent if none)
```

## Reading it — reference decoder

```python
import struct, zlib

def read_minx(path):
    d = open(path, 'rb').read()
    magic, ver, hsz, nsec, toff, esz, stridx, fsz, _ = \
        struct.unpack_from('<4sHHIIIIII', d, 0)
    assert magic == b'MINX' and ver == 1 and fsz == len(d)
    secs = [struct.unpack_from('<8I', d, toff + i * esz)[:6] for i in range(nsec)]
    sb, ss = secs[stridx][2], secs[stridx][3]
    name = lambda o: d[sb+o : d.index(b'\0', sb+o)].decode()
    out = {}
    for typ, nm, off, size, addr, crc in secs:
        payload = d[off:off+size]
        assert zlib.crc32(payload) == crc, 'corrupt section ' + name(nm)
        out[name(nm)] = (typ, addr, payload)
    return out

secs = read_minx('game.minx')
open('game.min', 'wb').write(secs['rom'][2])          # extract the flat ROM
for i in range(0, len(secs['symtab'][2]), 16):        # walk the symbols
    nm, value, phys, flags = struct.unpack_from('<4I', secs['symtab'][2], i)
```

(The names emitted by the current writer are `rom`, `symtab`, `strtab`, `note`,
`map`, `noi`, `cdb`, and the `--embed` names — but keying on `type` is the robust
way.)

## Why not actual ELF?

ELF could express the ROM (a `PT_LOAD` segment) and the symbols (`.symtab`), but
not the parts that matter most here without inventing private conventions anyway:
the S1C88 has no ELF machine number or psABI, the dual linker-vs-physical address
model would need custom section flags, and `.map`/`.cdb`/`.noi` would all land in
opaque vendor sections. A 32-byte header + table that any language can decode in a
dozen lines was the better trade — the ELF *shape* (header / section table / string
table / per-section type+name+addr) is kept so the format feels familiar.
