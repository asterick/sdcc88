# S1C88 toolchain — pipeline & file formats

> **Source:** synthesized from Epson *S5U1C88000C Manual I — C Compiler/Assembler/Linker* (see the
> per-tool files in this directory for the detailed, page-cited distillations). This file is the
> "how it fits together" overview.

The Epson S1C88 Integrated Tool Package is a classic compile → assemble → link → **locate** pipeline.
The distinguishing stage is the **locator** (`lc88`): the linker produces a *relocatable* IEEE-695
object, and a separate locator assigns absolute addresses (respecting the S1C88 bank/page model) under
the control of a **DELFEE** description file. Modern versions replace `lc88` with the *Advanced Locator*
`alc88`, which folds in branch optimization and needs no hand-written description file.

## Stages

| Stage | Tool | Role | Input → Output |
|-------|------|------|----------------|
| Compile | `c88` | C → S1C88 assembly | `.c` → `.src` |
| Assemble | `as88` | assembly → relocatable object | `.src` / `.asm` → `.obj` |
| Archive *(opt.)* | `ar88` | bundle objects into a library | `.obj…` → `.a` (IEEE-695 archive) |
| Link | `lk88` | combine objects/libs → one relocatable object | `.obj` / `.a` → `.out` |
| Locate | `lc88` (or `alc88`) | assign absolute addresses → final image | `.out` + DELFEE → `.abs` / `.sre` |

`cc88` is the **control program** (driver) that runs the whole chain, dispatching each input by its
file suffix and invoking the stages in order — analogous to `gcc` driving `cpp`/`cc1`/`as`/`ld`.

```
  foo.c ──c88──▶ foo.src ──as88──▶ foo.obj ─┐
                                            ├─lk88──▶ a.out ──lc88──▶ a.abs (IEEE-695, default)
  libc.a (ar88 of .obj) ─────────────────── ┘              + DELFEE        a.sre (Motorola-S, -srec)
```

## File extensions

| Ext | Contents | Produced by | Consumed by |
|-----|----------|-------------|-------------|
| `.c` | C source | — | `c88` |
| `.asm` | assembly source (preprocessed first) | hand / `c88` | `as88` |
| `.src` | compiled assembly (assembled directly, no preprocess) | `c88` | `as88` |
| `.obj` | relocatable object (IEEE-695/MUFOM) | `as88` | `lk88`, `ar88` |
| `.a` | object library/archive (IEEE-695) | `ar88` | `lk88` |
| `.out` | linked relocatable object (default `a.out`) | `lk88` | `lc88` |
| `.abs` | absolute image, **IEEE-695** (locator default, `-ieee`) | `lc88` | debugger / mask tools |
| `.sre` | absolute image, **Motorola S-record** (`-srec`) | `lc88` | ROM writers |
| `.lnl` | linker map/list | `lk88` | human |
| `.map` / `.elc` | locator map / list | `lc88` | human |

> No Intel-HEX in this toolchain — final images are IEEE-695 (`.abs`) or Motorola S-records (`.sre`).

## Control-program stop phases (`cc88`)

| Flag | Stops after | Keeps |
|------|-------------|-------|
| `-cs` | compile (`.c`) / preprocess (`.asm`) | `.src` |
| `-c` | assemble | `.obj` |
| `-cl` | link | `.out` |
| *(none)* | locate | `.abs` (or `.sre` with `-srec`) |

## File formats (interop)

- **IEEE-695 / MUFOM** — the object and absolute format used throughout (`.obj`, `.out`, `.a`, `.abs`).
  A command/record language with RPN expressions; full binary opcode tables are transcribed in
  [utilities.md](utilities.md) (Appendix H). This is what any tool interoperating with the Epson
  objects must read/write.
- **Motorola S-records** — `S0`/`S2`/`S8` with 3-byte (24-bit) addresses for the S1C88's address space;
  layout + checksum in [utilities.md](utilities.md) (Appendix I).

## Relationship to this project (skip-c)

- **Decided (2026-05-31): skip-c targets SDCC's own `sdas`/`sdld`** (its in-tree ASxxxx fork), not the
  Epson `as88`/`lk88`/`lc88` chain. skip-c already emits sdas-dialect assembly; we **add an S1C88 backend
  to `sdas`** — `build/sdcc-4.5.0/sdas/as88/` (binary `sdas88`), modeled on `sdas/asz80/`. That `sdas88`
  doubles as the **codegen validator** (assemble emitted `.asm` as-is). See
  [`abi-decision.md`](abi-decision.md) → "Toolchain & validator". (Note: our `sdas88` ≠ Epson `as88`; the
  `sdas` prefix disambiguates.)
- The **Epson chain** (`as88`/`lk88`/`lc88`, IEEE-695/S-record) and these docs remain the **authoritative
  ISA/ABI reference**, but are no longer the build target.
- **skiploom** (`../../../skiploom`) — a JS reimplementation of the Epson AS88 assembler/linker — stays an
  **independent ISA cross-check** only (its opcode table `src/util/s1c88.csv`), not the toolchain.
