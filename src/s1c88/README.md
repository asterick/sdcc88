# S1C88 backend port

The SDCC backend port for the Epson S1C88 — the only compiler code sdcc88 writes. Everything else is
inherited from upstream SDCC (see `../../CLAUDE.md`). Built by `../../build.sh`, which overlays this
directory into SDCC's tree and builds it with SDCC's `port.mk`.

## Provenance

Cloned from **SDCC 4.5.0's `src/z80` port** (the z80 register model — carry flag, byte-addressable pairs,
non-byte-addressable `IX`/`IY` index registers with `(ix+d)` addressing — is the closest fit to the
S1C88). Registered with SDCC's core via `../../third_party/sdcc/register_s1c88_port.patch`
(`TARGET_ID_S1C88`, `TARGET_IS_S1C88`, S1C88 added to `TARGET_Z80_LIKE`, `extern PORT s1c88_port`, and the
`_ports[]` entry). `.target` is `s1c88`, so the driver selects this port with **`-ms1c88`**.

The base-port choice and the target ABI / z80→s1c88 register mapping are documented in
`../../docs/s1c88/abi-decision.md`. The S1C88 architecture/ISA/toolchain references are in
`../../docs/s1c88/`.

> **Status (skeleton milestone):** the port **compiles, links, and is selectable** as `-ms1c88`, but its
> **code generation is still z80** (register names, instruction selection, calling convention, assembler
> hooks). Retargeting the codegen to the S1C88 ISA is the next milestone.
>
> For a low-risk first build the clone is kept close to upstream z80: all 10 z80 `PORT` structs remain in
> `main.c` (only `s1c88_port` is registered) and the z80 variant peephole `.def` files are shipped. A
> follow-up cleanup will strip to a single port.

| File | Role |
|------|------|
| `main.c` | The `PORT s1c88_port` struct — target sizes/alignment, options, assembler/linker command lines, codegen hooks. (Also retains the other z80 variant PORT structs, unregistered.) |
| `s1c88.h` | Port-private common header (was z80.h): the sub-port enum, `z80_opts`, and the `IS_*` variant macros. `s1c88_port` runs the plain-z80 path (`sub == SUB_Z80`). |
| `gen.c` / `gen.h` | iCode → assembly. The bulk of the retargeting effort. |
| `ralloc.c` / `ralloc.h` | Register allocation (`z80_regs[]`/`*_IDX`, kept verbatim — `ralloc2.cc` is hard-keyed to them). |
| `ralloc2.cc` | C++ instantiation of SDCC's generic tree-decomposition allocator (`SDCCralloc.hpp`, needs Boost). |
| `support.c` / `support.h` | Port support helpers. |
| `mappings.i` | ASM-dialect mapping tables (`_asxxxx_z80`, …), `#include`d by `main.c`. |
| `peep.c` / `peep.h` | Port-specific peephole helpers. |
| `peeph.def`, `peeph-*.def` | Peephole rules. `port.mk` builds each `*.def` into `*.rul` via `gawk -f ../SDCCpeeph.awk`; `main.c` `#include`s `peeph.rul` + `peeph-z80.rul`. |
| `Makefile.in` | Per-port build stub (just `include ../port.mk`); `build.sh` instantiates it via `config.status`. |

## Retargeting checklist (replace z80 codegen with S1C88)

Drive every change from `../../docs/s1c88/` (distilled Epson manuals) — especially
[`instruction-set.md`](../../docs/s1c88/instruction-set.md), [`addressing-modes.md`](../../docs/s1c88/addressing-modes.md),
and [`abi-decision.md`](../../docs/s1c88/abi-decision.md).

1. **`ralloc.c` / `ralloc.h` / `ralloc2.cc`** — reshape the z80 register file (`a,c,b,e,d,l,h,iyl,iyh` +
   pairs) into the real S1C88 set (`A`, `B`/`BA`, `L`/`H`/`HL`, `IX`, `IY`, carry, `SP`) and update the
   `*_IDX`↔`REG_*`↔`num_regs` coupling and allocator hooks.
2. **`main.c` `s1c88_port`** — Epson pointer/int widths (3-byte `_far`), generic-pointer tags, segment
   names, calling-convention/return registers (see `abi-decision.md`), assembler/linker command lines.
3. **`gen.c`** — replace z80 instruction selection/mnemonics with S1C88 (`LD`, `ADD`/`ADC`, `[HL]`,
   `[IX+dd]`, `JRS/JRL/CARS/CARL`, `RETE`, …; `CE`/`CF` prefix pages).
4. **`peeph*.def`** — replace z80 peephole patterns with S1C88 ones (strip the unused variant defs).
5. **Single-port cleanup** — strip the unregistered z80 variant PORT structs from `main.c`.
