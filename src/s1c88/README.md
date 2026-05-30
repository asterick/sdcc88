# S1C88 backend port

The SDCC backend port for the Epson S1C88 — the only compiler code skip-c writes. Everything else is
inherited from upstream SDCC (see `../../CLAUDE.md`). Built by `../../build.sh`, which overlays this
directory into SDCC's tree and builds it with SDCC's `port.mk`.

## Provenance

Cloned from **SDCC 4.5.0's `src/stm8` port** with a blanket identifier rename (`stm8`→`s1c88`,
`STM8`→`S1C88`), and registered with SDCC's core via `../../third_party/sdcc/register_s1c88_port.patch`
(`TARGET_ID_S1C88`, `TARGET_IS_S1C88`, `extern PORT s1c88_port`, and the `_ports[]` entry). `.target` is
`s1c88`, so the driver selects this port with **`-ms1c88`**.

> The port **compiles, links, and is selectable**, but its **code generation is still STM8** (instruction
> selection, register set, calling convention, assembler hooks). Retargeting it to the S1C88 is the work.

| File | Role |
|------|------|
| `main.c` | The `PORT s1c88_port` struct — target sizes/alignment, options, assembler/linker command lines, codegen hooks. |
| `gen.c` / `gen.h` | iCode → assembly. The bulk of the retargeting effort. |
| `ralloc.c` / `ralloc.h` | Register allocation (defines `s1c88_regs[]`). |
| `ralloc2.cc` | C++ instantiation of SDCC's generic tree-decomposition allocator (`SDCCralloc.hpp`, needs Boost). |
| `peep.c` / `peep.h` | Port-specific peephole helpers. |
| `peeph.def` | Peephole rules. SDCC's `port.mk` builds these into `peeph.rul` via `gawk -f ../SDCCpeeph.awk`; `main.c` does `#include "peeph.rul"`. |
| `Makefile.in` | Per-port build stub (just `include ../port.mk`); `build.sh` instantiates it via `config.status`. |

## Retargeting checklist (replace STM8 with S1C88)

Drive every change from the encoding sources next door — `../../../skiploom/src/util/s1c88.csv`
(opcode/operand table) and `../../../skiploom/docs/id000920.pdf` (S1C88 manual); see `../../docs/s1c88.md`.

1. **`ralloc.c` `s1c88_regs[]` + `gen.h`** — replace the STM8 register file (currently `a, xl, xh, yl,
   yh, c, x, y, sp`) and the `*_IDX` enum with the real S1C88 register set.
2. **`main.c` `s1c88_port`** — fix pointer/int widths, alignment, the model options, default memory
   locations, keywords, and the assembler/linker command lines (currently STM8/`sdas`-style; the
   asm→binary target is an open decision — see `../../CLAUDE.md`).
3. **`gen.c`** — rewrite instruction selection to emit S1C88 (AS88 syntax if targeting skiploom).
4. **`peeph.def`** — replace STM8 peephole patterns with S1C88 ones.
5. **`ralloc2.cc`** — adapt the allocator cost/legality hooks to the S1C88 register classes.
