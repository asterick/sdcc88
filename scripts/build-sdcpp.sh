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

HOST_WINDOWS=
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) HOST_WINDOWS=1 ;; esac

LIBIBERTY="${SDCC}/support/sdbinutils/libiberty"
CPP="${SDCC}/support/cpp/gcc/cpp"

# 1. libiberty — its own autotools project; the top-level configure --disable-sdbinutils skips it,
#    so configure + build it directly (cpp/gcc's Makefile hard-references ../../sdbinutils/libiberty).
if [ ! -f "${LIBIBERTY}/libiberty.a" ]; then
  echo ">> building libiberty"
  if [ ! -f "${LIBIBERTY}/Makefile" ]; then
    if [ -n "$HOST_WINDOWS" ]; then
      # match build.sh's gnu17 (C23-default gcc breaks old C in libiberty too)
      ( cd "${LIBIBERTY}" && CFLAGS='-g -O2 -std=gnu17' ./configure )
    else
      ( cd "${LIBIBERTY}" && ./configure )
    fi
  fi
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

# 3. Windows only: the driver spawns sdcpp via _popen(), which cannot exec a shell
#    script — replace the configure-generated bin/sdcpp wrapper with the real cpp.exe
#    and stage its cc1 backend at the libexecsubdir shape the driver relocates against
#    argv[0] (cc1 flat next to cpp is NOT found; same layout package-sdk.sh ships).
if [ -n "$HOST_WINDOWS" ]; then
  # Run every time (cheap) so a CACHED build tree gets corrected too — the
  # guard used to be "bin/sdcpp.exe exists", which left stale cc1 staging in
  # place on cache hits.
  if [ ! -x "${SDCC}/bin/sdcpp.exe" ]; then
    echo ">> windows: bin/sdcpp wrapper -> real cpp.exe"
    rm -f "${SDCC}/bin/sdcpp"
    cp "${SDCC}/support/cpp/gcc/cpp.exe" "${SDCC}/bin/sdcpp.exe"
  fi
  # The driver itself reports the (relocated) dir it will search for cc1 —
  # the first "programs" entry of -print-search-dirs. Deriving it from the
  # Makefile's target_noncanonical broke on CLANGARM64, where that alias
  # differs from the embedded DEFAULT_TARGET_MACHINE. ';'-separated on
  # Windows (paths carry "D:").
  progdir="$("${SDCC}/bin/sdcpp.exe" -print-search-dirs | sed -n 's/^programs: =//p' | cut -d';' -f1)"
  [ -n "$progdir" ] || { echo "!! sdcpp -print-search-dirs gave no programs dir" >&2; exit 1; }
  if [ ! -x "${progdir}/cc1.exe" ]; then
    echo ">> windows: staging cc1 -> ${progdir}"
    mkdir -p "$progdir"
    cp "${SDCC}/support/cpp/gcc/cc1.exe" "${progdir}/cc1.exe"
  fi
fi

echo
echo ">> sdcpp ready: ${CPP}"
echo "   integrated pipeline now works:  bin/sdcc -ms1c88 foo.c   (no --c1mode needed)"
