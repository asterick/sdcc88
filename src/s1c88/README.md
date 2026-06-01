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

> **Status:** the port compiles, links, runs as `-ms1c88`, and its **code generation is being retargeted**
> to the S1C88 ISA, always-green incremental. **Done:** the call ABI (returns BA/HL:BA, faithful Epson arg
> order), frame setup, branches (`jrs`/`jrl`/`carl`), the full compare cluster (native `cp ba,hl` /
> `cp …,#imm` + `jrs LT/GE`), the 16-bit ALU (`sub ba,hl`), `adjustStack` (native SP moves), 8-bit L/H ALU
> operands incl. AND/OR/XOR (via B), shifts (via A/B), accumulator rotates (`rl a`…), `push af`→`push a;
> push sc`, and the scratch-pair selectors (`getFreePairId`→BA). **Remaining:** the z80 `C/D/E` byte regs +
> `DE`/`BC` scratch pairs (the central register-model grind: the stack-peek/epilogue idioms → `ld hl,
> d(sp)`, the variable-shift `C` counter, `inc -N(ix)`) — see `../../docs/s1c88/HANDOFF.md` for the live
> worklist + per-slice commits + the `emitDebug`/`--verbose` debugging gotcha.
>
> The other 9 z80 variant `PORT` structs have been pruned; this is a single-variant port running the plain
> z80 codegen path (`z80_opts.sub == SUB_Z80`). Many internal identifiers keep their z80 names
> (`z80_regs`, `z80_opts`, `IS_*`/`PAIR_*`) — port-internal and harmless (the real z80 port is disabled).

| File | Role |
|------|------|
| `main.c` | The `PORT s1c88_port` struct — target sizes/alignment, options, assembler/linker command lines, codegen hooks. |
| `s1c88.h` | Port-private common header (was z80.h): the sub-port enum, `z80_opts`, and the `IS_*` variant macros. `s1c88_port` runs the plain-z80 path (`sub == SUB_Z80`). |
| `gen.c` / `gen.h` | iCode → assembly. The bulk of the retargeting effort. |
| `ralloc.c` / `ralloc.h` | Register allocation (`z80_regs[]`/`*_IDX`, kept verbatim — `ralloc2.cc` is hard-keyed to them). |
| `ralloc2.cc` | C++ instantiation of SDCC's generic tree-decomposition allocator (`SDCCralloc.hpp`, needs Boost). |
| `support.c` / `support.h` | Port support helpers. |
| `mappings.i` | ASM-dialect mapping tables (`_asxxxx_z80`, …), `#include`d by `main.c`. |
| `peep.c` / `peep.h` | Port-specific peephole helpers. |
| `peeph.def` | Peephole rules — the single, complete S1C88 definition file (the z80 `peeph-z80.def` was merged in; dead variant files removed). `port.mk` builds it into `peeph.rul` via `gawk -f ../SDCCpeeph.awk`; `main.c` `#include`s `peeph.rul`. |
| `Makefile.in` | Per-port build stub (just `include ../port.mk`); `build.sh` instantiates it via `config.status`. |

## Retargeting checklist (replace z80 codegen with S1C88)

Drive every change from `../../docs/s1c88/` (distilled Epson manuals) — especially
[`instruction-set.md`](../../docs/s1c88/instruction-set.md), [`addressing-modes.md`](../../docs/s1c88/addressing-modes.md),
and [`abi-decision.md`](../../docs/s1c88/abi-decision.md). The live worklist is `../../docs/s1c88/HANDOFF.md`.

1. **`ralloc.c` / `ralloc.h` / `ralloc2.cc`** — reshape the register file toward the S1C88 set
   (`A,B,L,H` + `BA`/`HL`, `IX`, `IY`). *In progress:* the allocator is constrained to `A/B/L/H` and
   `PAIR_BA` is a first-class pair; eliminating the z80 `C/D/E` regs end-to-end is the remaining grind.
2. **`main.c` `s1c88_port`** — Epson widths, generic-pointer tags, segment names, calling-convention/return
   registers, assembler/linker command lines. *Done:* the call ABI (BA/HL:BA returns, faithful arg order).
3. **`gen.c`** — S1C88 instruction selection/mnemonics. *Done:* frame, branches (`jrs/jrl/carl`), compares
   (`cp ba,hl`/`#imm` + `jrs LT/GE`), 16-bit ALU (`sub ba,hl`), `adjustStack`, 8-bit L/H ALU + shifts
   (routed through A/B). *Remaining:* the `DE`/`BC` scratch-asmop machinery and the variable-shift `C`
   counter.
4. **`peeph*.def`** — S1C88 peephole patterns. *Done:* `jp→jrl`/`jr→jrs`/`call→carl`, native `jrs LT/GE`,
   and dropping z80 rules that emit illegal S1C88 forms (e.g. the indexed-memory shift fold).
5. **Single-port cleanup** — *Done:* the unregistered z80 variant PORT structs are stripped from `main.c`.
