#!/usr/bin/env bash
#
# build.sh — build the complete sdcc88 SDK: SDCC 4.5.0 retargeted to the Epson S1C88 (Pokémon Mini).
#
# Strategy: sdcc88 is an OVERLAY on upstream SDCC, built with SDCC's own autotools build system
# (configure + make) rather than a reimplemented build. This script fetches SDCC, drops our
# src/s1c88 port into its tree, registers the port, and builds the whole toolchain end to end:
# the `sdcc` compiler driver, the sdcpp preprocessor, the sdas88/sdldz80/romgen binary tools, and
# the target runtime (crt0 + s1c88.lib + device headers). After it, `build/sdcc-4.5.0/` is a usable
# SDK: `sdcc -ms1c88 game.c -o game.ihx && romgen game.ihx game.min` builds a bootable ROM.
#
# (The per-component build-*.sh scripts under scripts/ still exist — the test/smoke scripts call
# them lazily — but you no longer need to run them by hand; build.sh does the whole SDK.)
#
# Requirements (install once, e.g. on Debian/Ubuntu/WSL):
#   sudo apt-get install -y build-essential flex bison m4 gawk libboost-dev zlib1g-dev
#
# Usage:
#   ./build.sh           # incremental: fetch (cached), overlay, configure (once), build the SDK
#   ./build.sh --fresh   # wipe build/ and rebuild the whole SDK from scratch
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
#    Use patch(1), NOT `git apply`: build/ is nested inside sdcc88's own git repo, so `git apply`
#    resolves the patch paths against the OUTER repo and silently no-ops (exits 0 without touching the
#    files), which then fails the build later with a confusing "TARGET_ID_S1C88 undeclared" error.
if ! grep -q 'TARGET_ID_S1C88' "${SDCC}/src/port.h"; then
  echo ">> applying register_s1c88_port.patch"
  ( cd "${SDCC}" && patch -p1 --forward < "${REPO}/third_party/sdcc/register_s1c88_port.patch" )
fi
# Fail loudly if registration didn't land — otherwise the build dies later with a cryptic compile error.
grep -q 'TARGET_ID_S1C88' "${SDCC}/src/port.h" \
  || { echo "ERROR: port registration patch did not apply (src/port.h missing TARGET_ID_S1C88)" >&2; exit 1; }

# 4b. Teach the two SDCC device headers whose z80-family branch is BOTH needed and
#     correct for s1c88. <stdarg.h> gates va_list on `defined(__SDCC_z80) || ...`;
#     s1c88 isn't listed, so it falls to the mcs51 default (the __data qualifier)
#     and <stdarg.h>/<stdio.h> (printf, varargs) break. <sdcc-lib.h> likewise gates
#     _REENTRANT and the asm/<port>/features.h include. s1c88's stack/varargs model
#     matches the z80 family, so join that branch (idempotent).
#     NB: deliberately NOT string.h — its z80 branch decorates mem*/str* with
#     __preserves_regs(iyl,iyh), a guarantee our C implementations don't make.
echo ">> teaching stdarg.h / sdcc-lib.h that s1c88 is z80-like"
for h in stdarg.h sdcc-lib.h; do
  f="${SDCC}/device/include/${h}"
  if [ -f "$f" ] && ! grep -q '__SDCC_s1c88' "$f"; then
    sed -i 's/defined(__SDCC_z80)/defined(__SDCC_s1c88) || defined(__SDCC_z80)/g' "$f"
  fi
done

# 5. Configure: build the compiler with ALL stock ports enabled, alongside s1c88. The s1c88 port used to
#    require --disable-ing every other port because, as a clone of the z80 port, it kept the z80 port's
#    global symbol names and collided at link time. Those 44 globals were renamed to unique s1c88_* names
#    (see git history), so s1c88 is now a fully independent variant that links cleanly next to z80 and the
#    rest. We only disable the *peripheral* tooling (device libs, ucsim, sdcdb, sdbinutils) — heavyweight
#    extras unrelated to the s1c88 codegen. s1c88 itself is injected below (configure doesn't know it).
cd "${SDCC}"
if [ ! -f config.status ]; then
  echo ">> configure"
  ./configure \
    --disable-device-lib --disable-ucsim --disable-sdcdb --disable-sdbinutils \
    --disable-non-free --disable-packihx
fi

# 6. Inject s1c88 into the configured build (configure doesn't know our port): add it to the port
#    list, and generate its Makefile from Makefile.in via config.status (no donor port needed).
grep -qx s1c88 ports.all   2>/dev/null || echo s1c88 >> ports.all
grep -qx s1c88 ports.build 2>/dev/null || echo s1c88 >> ports.build
./config.status --file=src/s1c88/Makefile:src/s1c88/Makefile.in

# 7. Build the sdcc compiler driver (with the s1c88 port). This builds src/ only — the codegen.
echo ">> make (sdcc compiler driver)"
make -C src

# 8. Build the rest of the SDK: the preprocessor + binary toolchain + runtime, so that the result
#    is a complete, usable Pokémon Mini SDK (not just the codegen). Each step is its own idempotent
#    component script — they are also invoked lazily by the test/smoke scripts — and is a fast no-op
#    once built, so re-running build.sh after the first full build only re-makes the compiler above.
#    The heavy one is sdcpp (a GCC-cpp fork); --fresh rebuilds it. Order matters: the runtime is
#    compiled through both the sdcc driver and sdas88, so it must come last.
echo ">> sdcpp (preprocessor)";        "${REPO}/scripts/build-sdcpp.sh"
echo ">> sdas88 (assembler)";          "${REPO}/scripts/build-sdas.sh" as88
echo ">> sdldz80 (linker)";            "${REPO}/scripts/build-sdld.sh"
echo ">> romgen (.ihx -> .min)";       "${REPO}/scripts/build-romgen.sh"
echo ">> runtime (crt0 + s1c88.lib + headers)"; "${REPO}/scripts/build-runtime.sh"

echo
echo ">> SDK ready under ${SDCC}/"
"${SDCC}/src/sdcc" --version 2>&1 | head -3 || true
echo ">>   compiler : ${SDCC}/src/sdcc -ms1c88 ..."
echo ">>   bin/      : sdcpp, sdas88, sdldz80, romgen (+ sdcc-family tools)"
echo ">>   try       : make -C examples/hello run"
