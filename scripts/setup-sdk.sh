#!/usr/bin/env bash
#
# setup-sdk.sh — build the complete sdcc88 toolchain from a clean checkout, in order.
#
# After this, `build/sdcc-4.5.0/` is a usable Pokémon Mini SDK:
#   bin/   : sdcpp, sdas88, sdldz80, romgen, sdcc-family tools
#   src/   : sdcc (the -ms1c88 driver)
#   share/sdcc/lib/s1c88/     : crt0.rel + s1c88.lib
#   share/sdcc/include/s1c88/ : pm.h (device header)
#
# Put bin/ and src/ on PATH and `sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx game.min`
# builds a bootable ROM. See examples/hello and docs/s1c88/building-roms.md.
#
#   scripts/setup-sdk.sh          # build everything (steps are individually idempotent)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"

echo "== 1/6  sdcc compiler driver (build.sh) =="
./build.sh
echo "== 2/6  sdcpp preprocessor =="
./scripts/build-sdcpp.sh
echo "== 3/6  sdas88 assembler =="
./scripts/build-sdas.sh as88
echo "== 4/6  sdldz80 linker =="
./scripts/build-sdld.sh
echo "== 5/6  romgen (.ihx -> .min) =="
./scripts/build-romgen.sh
echo "== 6/6  runtime: crt0.rel + s1c88.lib + device headers =="
./scripts/build-runtime.sh

echo
echo ">> SDK ready under build/sdcc-4.5.0/"
echo ">> try:  make -C examples/hello run"
