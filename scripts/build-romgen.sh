#!/usr/bin/env bash
#
# build-romgen.sh — build the romgen tool (tools/romgen.c) into the toolchain bin/.
#
# romgen converts the sdld88 Intel-HEX output into a flat Pokémon Mini .min ROM. It is
# a plain C program (no Python dependency in the shipped toolchain) and installs next
# to sdas88/sdldz80 so the whole pipeline is self-contained.
#
#   scripts/build-romgen.sh
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
BIN="${SDCC}/bin/romgen"

mkdir -p "${SDCC}/bin"
CC="${CC:-cc}"
"$CC" -O2 -Wall -o "$BIN" "${REPO}/tools/romgen.c"
echo ">> built ${BIN}"
