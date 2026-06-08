#!/usr/bin/env bash
#
# build-sdcpp.sh — build SDCC's real preprocessor (sdcpp) in the configured tree.
#
# build.sh builds only the compiler driver (`make -C src`); it deliberately skips SDCC's bundled
# sdcpp, a heavyweight GCC-cpp fork that also needs libiberty from sdbinutils (both --disable'd at
# configure time). Without it the installed `bin/sdcpp` is a thin wrapper that execs whatever `cpp`
# lives in support/cpp/gcc/ — so `sdcc -ms1c88 foo.c` (no --c1mode) FAILS to preprocess until the
# real cpp is built here. This script builds the two missing pieces, idempotently:
#
#   1. support/sdbinutils/libiberty  -> libiberty.a   (cpp/gcc links against it)
#   2. support/cpp                   -> support/cpp/gcc/cpp  (the real sdcpp backend)
#
# After this, build/.../bin/sdcpp (the wrapper) finds the real cpp on its PATH and the integrated
# driver pipeline (sdcpp -> sdcc -> sdas88 -> sdldz80) works end to end.
#
#   scripts/build-sdcpp.sh         # build if missing (fast no-op once built)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"

[ -f "${SDCC}/config.status" ] || { echo "!! tree not configured — run ./build.sh first" >&2; exit 1; }

LIBIBERTY="${SDCC}/support/sdbinutils/libiberty"
CPP="${SDCC}/support/cpp/gcc/cpp"

# 1. libiberty — its own autotools project; the top-level configure --disable-sdbinutils skips it,
#    so configure + build it directly (cpp/gcc's Makefile hard-references ../../sdbinutils/libiberty).
if [ ! -f "${LIBIBERTY}/libiberty.a" ]; then
  echo ">> building libiberty"
  [ -f "${LIBIBERTY}/Makefile" ] || ( cd "${LIBIBERTY}" && ./configure )
  make -C "${LIBIBERTY}"
else
  echo ">> libiberty.a present"
fi

# 2. support/cpp — configured by the top-level configure (its Makefile exists); just build it. Yields
#    support/cpp/gcc/cpp, which the generated bin/sdcpp wrapper puts on PATH and execs as `cpp`.
if [ ! -x "${CPP}" ]; then
  echo ">> building support/cpp (this is heavy — a GCC cpp fork)"
  make -C "${SDCC}/support/cpp"
else
  echo ">> sdcpp backend (support/cpp/gcc/cpp) present"
fi

echo
echo ">> sdcpp ready: ${CPP}"
echo "   integrated pipeline now works:  bin/sdcc -ms1c88 foo.c   (no --c1mode needed)"
