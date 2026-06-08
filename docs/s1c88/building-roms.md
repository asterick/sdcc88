# Building Pokémon Mini ROMs with sdcc88

This is the end-user guide: how to turn C source into a flashable Pokémon Mini `.min`
ROM with the sdcc88 toolchain. For the compiler internals see
[`abi-decision.md`](abi-decision.md); for current status see [`HANDOFF.md`](HANDOFF.md).

## 1. Build the SDK

One command builds the whole toolchain from a clean checkout:

```bash
./scripts/setup-sdk.sh
```

It runs, in order: the `sdcc` driver (`build.sh`), the `sdcpp` preprocessor, the
`sdas88` assembler, the `sdldz80` linker, the `romgen` ROM tool, and the runtime
(`crt0.rel` + `s1c88.lib` + device headers). Each step is idempotent. Dependencies
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

The **stack pointer is set by the BIOS** on reset (the S1C88 leaves SP undefined at
power-on); the crt0 deliberately does not touch it, so the stack lives wherever the
Pokémon Mini BIOS parks it.

For **banked** (>32 KiB) ROMs and `__far` ROM data, code/data are placed at
`(bank<<16)|logic` and `romgen --far=start-end` declares physical far ranges; see
[`banked-branch.md`](banked-branch.md) and the `__far` notes in `abi-decision.md`.

## 5. Interrupts

The crt0 builds the cartridge IRQ vector table; by default every maskable vector
points at a do-nothing handler (`_irq_default`, which `RETE`s). To run code on an
interrupt today, enable it (`IRQ_ENA*`/`IRQ_PRI*` via `<pm.h>`), drop the CPU
interrupt level, and point the relevant 6-byte slot at your handler. (A higher-level
`__interrupt`-to-vector auto-wiring is a planned convenience — see TODO.) The
`VEC_*` constants in `<pm.h>` name the vector numbers.

## 6. Verifying

The repo's harnesses run compiled code on the vendored emulator core:

- `scripts/driver-smoke.sh` — the full `sdcc -ms1c88 game.c` → `.min` → boot path.
- `scripts/emu-test.sh` — the execution test cases.
- `scripts/corpus-check.sh` — byte-identical codegen + clean assembly.
