#!/usr/bin/env bash
#
# build-sdas.sh — build a sdas assembler backend in the configured SDCC tree.
#
# sdcc88 targets SDCC's own sdas/sdld (see docs/s1c88/abi-decision.md "Toolchain & validator").
# build.sh only does `make -C src` (the compiler); the sdas assemblers are separate. This script
# builds a backend against the shared sdas/asxxsrc/ core, producing build/sdcc-4.5.0/bin/sdas<arch>.
#
#   scripts/build-sdas.sh            # default: as88 -> sdas88 (our S1C88 backend = the validator)
#   scripts/build-sdas.sh asz80      # stock z80 backend -> sdasz80 (proves the pipeline)
#
# Two cases:
#  - stock backend (asz80, …): configure already knows it; generate its Makefile via config.status.
#  - our overlaid backend (as88): sources live in sdcc88/sdas/as88/; overlay them into the build tree
#    and derive the Makefile from asz80's generated one (config.status doesn't know our port) — the
#    same trick build.sh uses to inject src/s1c88/Makefile.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
BACKEND="${1:-as88}"
DIR="${SDCC}/sdas/${BACKEND}"
OVERLAY="${REPO}/sdas/${BACKEND}"
BIN="sdas${BACKEND#as}"   # asz80->sdasz80, as88->sdas88

[ -f "${SDCC}/config.status" ] || { echo "!! tree not configured — run ./build.sh first" >&2; exit 1; }

# Banked-branch toolchain patch: gives the shared asxxsrc a TARGET_ID_S1C88 identity that routes
# relocations through the 24-bit/escape path (so bcall/bjump's R_S1C88_BANK survives), and adds the
# bank-rewrite case to linksrc/lkrloc3.c. Applied idempotently (marker = TARGET_ID_S1C88 in sdas.h).
if ! grep -q TARGET_ID_S1C88 "${SDCC}/sdas/asxxsrc/sdas.h" 2>/dev/null; then
  echo ">> applying s1c88_banked_branch.patch"
  ( cd "${SDCC}" && patch -p1 --forward < "${REPO}/third_party/sdcc/s1c88_banked_branch.patch" ) \
    || { echo "!! s1c88_banked_branch.patch failed to apply" >&2; exit 1; }
fi

cd "${SDCC}"
if [ -d "${OVERLAY}" ]; then
  echo ">> overlaying sdcc88/sdas/${BACKEND} -> build tree"
  mkdir -p "${DIR}"
  cp "${OVERLAY}"/* "${DIR}/"
  # config.status doesn't know our backend; derive its Makefile from asz80's generated one,
  # fixing the backend dir, the source filenames, and the output binary name.
  [ -f sdas/asz80/Makefile ] || ./config.status --file=sdas/asz80/Makefile:sdas/asz80/Makefile.in
  sed -e "s/asz80/${BACKEND}/g" \
      -e 's/z80pst\.c/s1c88pst.c/g; s/z80mch\.c/s1c88mch.c/g; s/z80adr\.c/s1c88adr.c/g' \
      -e "s/sdasz80/${BIN}/g" \
      sdas/asz80/Makefile > "${DIR}/Makefile"
else
  echo ">> stock backend — generating Makefile via config.status"
  [ -d "${DIR}" ] || { echo "!! no sdas backend: sdas/${BACKEND}" >&2; exit 1; }
  ./config.status --file="sdas/${BACKEND}/Makefile:sdas/${BACKEND}/Makefile.in"
fi

echo ">> make -C sdas/${BACKEND}"
make -C "sdas/${BACKEND}"
echo ">> built: $(ls -la "${SDCC}/bin/${BIN}" 2>/dev/null | awk '{print $NF" ("$5" bytes)"}')"
