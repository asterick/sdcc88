#!/usr/bin/env bash
#
# build-sdas.sh — build a sdas assembler backend in the configured SDCC tree.
#
# skip-c targets SDCC's own sdas/sdld (see docs/s1c88/abi-decision.md "Toolchain & validator").
# The compiler build (build.sh) only does `make -C src`; the sdas assemblers are separate. This
# script generates a backend's Makefile (config.status) and builds it against the shared asxxsrc
# core, producing build/sdcc-4.5.0/bin/sdas<arch>.
#
#   scripts/build-sdas.sh            # default: asz80 -> sdasz80 (proves the pipeline)
#   scripts/build-sdas.sh as88       # the S1C88 backend -> sdas88 (once sdas/as88/ exists)
#
# For a backend configure doesn't know about (e.g. our new as88), this generates its Makefile from
# Makefile.in by substitution from a sibling backend's Makefile — analogous to how build.sh injects
# src/s1c88/Makefile.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
BACKEND="${1:-asz80}"
DIR="${SDCC}/sdas/${BACKEND}"

[ -f "${SDCC}/config.status" ] || { echo "!! tree not configured — run ./build.sh first" >&2; exit 1; }
[ -d "${DIR}" ] || { echo "!! no sdas backend dir: sdas/${BACKEND}" >&2; exit 1; }

cd "${SDCC}"
if [ ! -f "sdas/${BACKEND}/Makefile" ]; then
  echo ">> generating sdas/${BACKEND}/Makefile"
  if grep -q "sdas/${BACKEND}/Makefile" config.status; then
    ./config.status --file="sdas/${BACKEND}/Makefile:sdas/${BACKEND}/Makefile.in"
  else
    # configure doesn't know this backend (new port): derive the Makefile from asz80's, fixing paths.
    echo "   (backend not in config.status — deriving from asz80)"
    sed "s|asz80|${BACKEND}|g" "sdas/asz80/Makefile" > "sdas/${BACKEND}/Makefile"
  fi
fi

echo ">> make -C sdas/${BACKEND}"
make -C "sdas/${BACKEND}"
echo ">> built: $(ls -la "${SDCC}/bin/sdas${BACKEND#as}" 2>/dev/null | awk '{print $NF" ("$5" bytes)"}')"
