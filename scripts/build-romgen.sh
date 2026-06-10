#!/usr/bin/env bash
#
# build-romgen.sh — build the ROM tools (tools/romgen.c + tools/minxdump.c) into the
# toolchain bin/.
#
# romgen converts the sdld88 Intel-HEX output into a flat Pokémon Mini .min ROM, or
# (with a .minx output) the MINX debug container; minxdump validates and dumps that
# container. Both are plain C programs (no Python dependency in the shipped toolchain)
# and install next to sdas88/sdldz80 so the whole pipeline is self-contained.
#
#   scripts/build-romgen.sh
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"

mkdir -p "${SDCC}/bin"
CC="${CC:-cc}"
"$CC" -O2 -Wall -o "${SDCC}/bin/romgen"   "${REPO}/tools/romgen.c"
"$CC" -O2 -Wall -o "${SDCC}/bin/minxdump" "${REPO}/tools/minxdump.c"
echo ">> built ${SDCC}/bin/romgen + minxdump"
