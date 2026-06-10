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

> **Status: complete and in maintenance.** Codegen is fully retargeted to the S1C88 ISA: the faithful
> BA+HL register model (the z80 `C/D/E`/`DE`/`BC` scratch machinery is gone), the call ABI (BA / HL:BA
> returns, faithful Epson arg order), IY index-register arguments, `__far` 3-byte banked pointers (the EP=0
> invariant), native `DIV`/`MLT`, the MAXIMUM-mode 3-byte CB:PC call model, the branch cluster
> (`jrs`/`jrl`/`carl`), compares (`cp ba,hl`/`#imm` + `jrs LT/GE`), the 16-bit/8-bit ALU, shifts/rotates,
> and `__critical`. The differential suite is clean. The forward backlog (code-size peepholes, the
> `UNIMPLEMENTED`-boundary lift, the flag-token cleanup) is in the repo-root [`TODO.md`](../../TODO.md).
>
> The other 9 z80 variant `PORT` structs have been pruned; this is a single-variant port that runs the
> plain-z80 codegen path (`IS_Z80` hardcoded 1). The globals that used to collide with the z80 port were
> renamed to unique `s1c88_*` names, so it links cleanly alongside z80 and the rest in one driver. Some
> internal type/enum names (`PAIR_*`, `SUB_Z80`) keep their z80 spelling — port-internal and harmless.

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

## Working on the port

Drive any change from `../../docs/s1c88/` (distilled Epson manuals) — especially
[`instruction-set.md`](../../docs/s1c88/instruction-set.md), [`addressing-modes.md`](../../docs/s1c88/addressing-modes.md),
and the authoritative [`abi-decision.md`](../../docs/s1c88/abi-decision.md) (register model, argument ABI,
the EP=0 `__far` invariant, native `DIV`, the MAXIMUM-mode call model). The forward backlog is the
repo-root [`TODO.md`](../../TODO.md).

**Always rebuild via the overlay** — `../../scripts/dev.sh` (or `corpus-check.sh`) re-overlays this
directory and rebuilds; never run raw `make -C build/.../src` (it compiles a stale copy). After any change,
run the three codegen gates: `corpus-check.sh` (asm is byte-stable), `emu-test.sh` + `diff-test.sh` (it
computes the right values) — and add a `tests/{emu,diff}/cases/*.c` whenever you touch new territory.
