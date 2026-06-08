#!/usr/bin/env bash
#
# build-runtime.sh — build + install the s1c88 target runtime into the SDCC lib dir
# so the driver's integrated link (`sdcc -ms1c88 foo.c`) finds crt0 and the support lib.
#
# The driver searches <datadir>/lib/s1c88/ for crt0.rel (port->linker.crt) and the
# 's1c88' library (port->linker.libs). This script populates that directory:
#   - crt0.rel        the production startup (device/lib/s1c88/crt0.s)
#   - s1c88.lib       [TODO A4] div/mul + widening + mem/str support routines
#
#   scripts/build-runtime.sh
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDAS="${SDCC}/bin/sdas88"
LIBDIR="${SDCC}/share/sdcc/lib/s1c88"

[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }
mkdir -p "$LIBDIR"

echo ">> assembling crt0 -> ${LIBDIR}/crt0.rel"
"$SDAS" -o "${LIBDIR}/crt0.rel" "${REPO}/device/lib/s1c88/crt0.s" \
  || { echo "!! crt0 assemble FAILED" >&2; exit 1; }

echo ">> installed: ${LIBDIR}/crt0.rel"
echo "   (s1c88.lib — div/mul/mem/str support — is TODO A4)"
