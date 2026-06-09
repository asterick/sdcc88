# Building Pokémon Mini ROMs with sdcc88

This is the end-user guide: how to turn C source into a flashable Pokémon Mini `.min`
ROM with the sdcc88 toolchain. For the compiler internals see
[`abi-decision.md`](abi-decision.md); for current status see [`HANDOFF.md`](HANDOFF.md).

## 1. Build the SDK

One command builds the whole toolchain from a clean checkout:

```bash
./build.sh
```

It builds, in order: the `sdcc` driver, the `sdcpp` preprocessor, the `sdas88`
assembler, the `sdldz80` linker, the `romgen` ROM tool, and the runtime
(`crt0.rel` + `s1c88.lib` + device headers). Each step is idempotent, so re-running
`./build.sh` only re-makes what changed (`--fresh` rebuilds everything). Dependencies
(Debian/Ubuntu/WSL): `build-essential flex bison m4 gawk libboost-dev zlib1g-dev`.

The result, under `build/sdcc-4.5.0/`, is the SDK:

```
bin/    sdcpp  sdas88  sdldz80  romgen   (+ sdar/sdnm/...)
src/    sdcc                              (the -ms1c88 driver)
share/sdcc/lib/s1c88/      crt0.rel  s1c88.lib
share/sdcc/include/s1c88/  pm.h
```

Put `bin/` and `src/` on your `PATH` (the example Makefile does this for you).

## 2. Build a ROM

Two steps: compile/link to Intel-HEX, then pack to a flat `.min`:

```bash
sdcc -ms1c88 game.c -o game.ihx     # preprocess -> compile -> assemble -> link
romgen game.ihx game.min            # Intel-HEX -> flat Pokémon Mini ROM
```

`sdcc -ms1c88` automatically links the production `crt0` (the `"PM"` cartridge
header + interrupt vector table + C startup) and `s1c88.lib` (the div/mul/mem/str
support routines). The `.min` is byte 0 = physical `0x2100`; it boots on PokeMini
and real hardware.

The simplest way is the example project, which wraps both steps in a Makefile:

```bash
make -C examples/hello          # -> examples/hello/hello.min
make -C examples/hello run      # build + run on the bundled emulator
```

Copy `examples/hello/{Makefile,hello.c}` as your project template.

## 3. The device header

`#include <pm.h>` gives the full hardware register map in Epson/SDK-style names —
no magic addresses. Examples:

```c
#include <pm.h>

PRC_MODE = MAP_ENABLE | COPY_ENABLE | MAP_24X16;  /* enable 192x128 tile map */
PRC_RATE = RATE_24FPS;
unsigned char pressed = (unsigned char) ~KEY_PAD; /* keys are active-low      */
IRQ_ENA1 |= IRQ1_TIM1_LO_UF;                       /* enable a timer interrupt */
```

It covers `SYS_*`, `SEC_*` (RTC), `TMR1_*`/`TMR2_*`/`TMR3_*`/`TMR256_*` (timers),
`IRQ_*` (controller + the `IRQ?_*` flags), `KEY_*`, `IO_*`, `AUD_*`, `PRC_*`
(graphics), `LCD_*`, the `OAM`/`TILEMAP` video RAM, and the `VEC_*` interrupt
vector numbers.

## 4. Memory map & link options

`sdcc -ms1c88` lays the program out for the Pokémon Mini common bank by default:

| Area | Address | Holds |
|---|---|---|
| `_CODE` | `0x2100` | cartridge header (front of crt0) + code + library |
| `_HOME`, `_GSINIT`, `_INITIALIZER` | (chained) | home code, init runners, ROM copy of initialized data |
| `_DATA`, `_INITIALIZED` | `0x1000` | RAM variables (RAM is `0x1000-0x1FFF`) |

Useful options:

- `--opt-code-size` — favor size over speed.
- `--code-loc` / `--data-loc` — override the `_CODE` / `_DATA` base (rarely needed).

**`const` data and banks.** Plain `const` data (strings, tables) lives in the common
bank and is read through ordinary 2-byte near pointers — so **all plain `const` data
must fit in the common bank** (`0x2100-0x7FFF`). If code + const outgrows it, `romgen`
stops with a *common-bank overflow* error rather than miscompiling. To put a large const
table in a far bank, declare it `__far const` (it lands in the `_FAR` area and is read
via the paged far-pointer path). Banked *code* is automatic — the linker splits oversized
code across banks via `bcall`/`bjump`.

The **stack pointer is set by the BIOS** on reset (the S1C88 leaves SP undefined at
power-on); the crt0 deliberately does not touch it, so the stack lives wherever the
Pokémon Mini BIOS parks it.

For **banked** (>32 KiB) ROMs and `__far` ROM data, code/data are placed at
`(bank<<16)|logic` and `romgen --far=start-end` declares physical far ranges; see
[`banked-branch.md`](banked-branch.md) and the `__far` notes in `abi-decision.md`.

## 5. Interrupts

The cartridge ROM is read-only, so you do **not** install handlers by writing the
low-memory vector table. Instead the crt0 header has a `bjump` trampoline per cart
vector slot (`bjump _irq_v<N>` at `0x2102 + 6*N`); the BIOS forwards a hardware IRQ
to its cart slot, and the trampoline jumps to the handler symbol `_irq_v<N>`.

**To handle a vector, just declare `__interrupt(VEC_*)`** — the compiler emits
`_irq_v<N>` at the function's entry automatically, overriding the library's
do-nothing default for that one slot:

```c
#include <pm.h>
/* VEC_TIM1_LO_UF == cart slot 6 -> the compiler defines _irq_v6 here */
void on_timer1(void) __interrupt(VEC_TIM1_LO_UF) {
    ...                          /* your work */
    IRQ_ACT1 = IRQ1_TIM1_LO_UF;  /* acknowledge the source */
}
int main(void) {
    TMR1_CTRL = ...;             /* program the timer        */
    IRQ_PRI1  = PRI1_TIM1(1);    /* group priority           */
    IRQ_ENA1  = IRQ1_TIM1_LO_UF; /* enable the source        */
    __asm and sc, #0x3f __endasm;/* drop the CPU level so IRQs are accepted */
    for(;;) { }
}
```

The `VEC_*` constants in `<pm.h>` are the **cart vector slots** (0..26) — *not* the
raw hardware IRQ numbers; the PM BIOS forwards a permuted subset of the 32 hardware
IRQs to those 27 slots (see `<pm.h>` for the per-slot hardware-IRQ comments, sourced
from <https://www.pokemon-mini.net/documentation/bios/>). Handlers can live in any
bank (the trampoline `bjump` resolves it). Vectors you don't declare keep the library
default — each unresolved slot decomposes to its own standalone do-nothing `rete`.
(You may still hand-name a function `irq_v<N>` instead of using `__interrupt(N)`.)
`scripts/crt0-isr-smoke.sh` exercises this end to end.

## 6. Verifying

The repo's harnesses run compiled code on the vendored emulator core:

- `scripts/driver-smoke.sh` — the full `sdcc -ms1c88 game.c` → `.min` → boot path.
- `scripts/emu-test.sh` — the execution test cases.
- `scripts/corpus-check.sh` — byte-identical codegen + clean assembly.

## 7. Limitations

- **`Unimplemented` build error.** The codegen has a few register-pressure corners (mostly under
  `--reserve-regs-iy`) where it stops with `Unimplemented` rather than emit wrong code. It is a **loud
  trap, never a silent miscompile** — if a build *succeeds*, the code is correct. Workarounds: drop
  `--reserve-regs-iy`, or simplify the offending expression (split a wide bitwise/arith op, or reduce the
  number of simultaneously-live pointers). The boundary categories are cataloged in
  `docs/s1c88/abi-decision.md` ("Known codegen boundaries").
- **`common-bank overflow` from `romgen`.** Plain `const` data outgrew the common bank — see §4
  (use `__far const` for large tables).

Single-precision `float` works (add / subtract / multiply / divide / compares / int↔float casts), but is
software-emulated and slow — fine for occasional math, not for hot loops on the Pokémon Mini.
