#!/usr/bin/env bash
#
# build.sh — build skip-c: SDCC 4.5.0 retargeted to the Epson S1C88 (Pokémon Mini).
#
# Strategy: skip-c is an OVERLAY on upstream SDCC, built with SDCC's own autotools build system
# (configure + make) rather than a reimplemented build. This script fetches SDCC, drops our
# src/s1c88 port into its tree, registers the port, and builds the `sdcc` compiler driver.
#
# Requirements (install once, e.g. on Debian/Ubuntu/WSL):
#   sudo apt-get install -y build-essential flex bison m4 gawk libboost-dev zlib1g-dev
#
# Usage:
#   ./build.sh           # incremental: fetch (cached), overlay, configure (once), make
#   ./build.sh --fresh   # wipe build/ and start clean
#
set -euo pipefail

SDCC_VER=4.5.0
SDCC_SHA256=d5030437fb436bb1d93a8dbdbfb46baaa60613318f4fb3f5871d72815d1eed80
TARBALL_URL="https://master.dl.sourceforge.net/project/sdcc/sdcc/${SDCC_VER}/sdcc-src-${SDCC_VER}.tar.bz2?viasf=1"

REPO="$(cd "$(dirname "$0")" && pwd)"
BUILD="${REPO}/build"
SDCC="${BUILD}/sdcc-${SDCC_VER}"
TARBALL="${BUILD}/sdcc-src-${SDCC_VER}.tar.bz2"

[ "${1:-}" = "--fresh" ] && { echo ">> --fresh: removing ${SDCC}"; rm -rf "${SDCC}"; }
mkdir -p "${BUILD}"

# 1. Fetch (cached) and verify integrity.
if [ ! -f "${TARBALL}" ]; then
  echo ">> downloading SDCC ${SDCC_VER}"
  curl -fSL -A "Mozilla/5.0" "${TARBALL_URL}" -o "${TARBALL}"
fi
echo "${SDCC_SHA256}  ${TARBALL}" | sha256sum -c -

# 2. Extract with Python (no dependency on a bzip2 binary).
if [ ! -d "${SDCC}" ]; then
  echo ">> extracting"
  python3 - "${TARBALL}" "${BUILD}" <<'PY'
import sys, tarfile
with tarfile.open(sys.argv[1], 'r:bz2') as t:
    try: t.extractall(sys.argv[2], filter='data')   # py3.12+
    except TypeError: t.extractall(sys.argv[2])
PY
fi

# 3. Overlay our port sources into SDCC's tree.
echo ">> overlaying src/s1c88 port"
mkdir -p "${SDCC}/src/s1c88"
cp "${REPO}"/src/s1c88/*.c  "${REPO}"/src/s1c88/*.h  "${REPO}"/src/s1c88/*.cc \
   "${REPO}"/src/s1c88/*.i  "${REPO}"/src/s1c88/peeph*.def \
   "${REPO}"/src/s1c88/Makefile.in  "${SDCC}/src/s1c88/"

# 4. Register the port in SDCC's core (port.h + SDCCmain.c). Idempotent: skip if already applied.
#    Use patch(1), NOT `git apply`: build/ is nested inside skip-c's own git repo, so `git apply`
#    resolves the patch paths against the OUTER repo and silently no-ops (exits 0 without touching the
#    files), which then fails the build later with a confusing "TARGET_ID_S1C88 undeclared" error.
if ! grep -q 'TARGET_ID_S1C88' "${SDCC}/src/port.h"; then
  echo ">> applying register_s1c88_port.patch"
  ( cd "${SDCC}" && patch -p1 --forward < "${REPO}/third_party/sdcc/register_s1c88_port.patch" )
fi
# Fail loudly if registration didn't land — otherwise the build dies later with a cryptic compile error.
grep -q 'TARGET_ID_S1C88' "${SDCC}/src/port.h" \
  || { echo "ERROR: port registration patch did not apply (src/port.h missing TARGET_ID_S1C88)" >&2; exit 1; }

# 5. Configure: build only the compiler + s1c88, plus stm8 (kept as the donor for a generated per-port
#    Makefile, and to validate the Boost-based allocator). Skip the other ports and peripheral tools.
cd "${SDCC}"
if [ ! -f config.status ]; then
  echo ">> configure"
  # Disable ALL stock ports — skip-c is an s1c88-only compiler, and our port is a clone of stm8, so
  # building stm8 too would cause duplicate-symbol link errors. s1c88 is injected below.
  ./configure \
    --disable-mcs51-port --disable-z80-port --disable-z180-port --disable-z80n-port \
    --disable-ez80_z80-port --disable-r2k-port --disable-r2ka-port --disable-r3ka-port \
    --disable-r800-port --disable-sm83-port --disable-tlcs90-port --disable-ds390-port \
    --disable-ds400-port --disable-pic14-port --disable-pic16-port --disable-hc08-port \
    --disable-s08-port --disable-stm8-port --disable-pdk13-port --disable-pdk14-port \
    --disable-pdk15-port --disable-mos6502-port --disable-mos65c02-port --disable-f8-port \
    --disable-device-lib --disable-ucsim --disable-sdcdb --disable-sdbinutils \
    --disable-non-free --disable-packihx
fi

# 6. Inject s1c88 into the configured build (configure doesn't know our port): add it to the port
#    list, and generate its Makefile from Makefile.in via config.status (no donor port needed).
grep -qx s1c88 ports.all   2>/dev/null || echo s1c88 >> ports.all
grep -qx s1c88 ports.build 2>/dev/null || echo s1c88 >> ports.build
./config.status --file=src/s1c88/Makefile:src/s1c88/Makefile.in

# 7. Build the sdcc compiler driver (with the s1c88 port). We build only src/ — not SDCC's bundled
#    sdcpp preprocessor, which is a heavyweight GCC tree (and needs sdbinutils). End-to-end C
#    compilation additionally needs a preprocessor; see CLAUDE.md.
echo ">> make"
make -C src

echo
echo ">> built: ${SDCC}/src/sdcc"
"${SDCC}/src/sdcc" --version 2>&1 | head -3 || true
echo ">> port available as: ${SDCC}/src/sdcc -ms1c88 ..."
