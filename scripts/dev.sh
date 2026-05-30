#!/usr/bin/env bash
#
# dev.sh — fast inner loop for the s1c88 port: overlay the current src/s1c88 sources into the
# already-configured SDCC build tree, rebuild the compiler, and run a codegen smoke test.
#
# Use this while iterating on the backend. It only recompiles the port + relinks sdcc (seconds),
# unlike ./build.sh (which re-runs the overlay/patch/configure machinery). If the tree isn't
# configured yet, it falls back to ./build.sh.
#
#   ./scripts/dev.sh                 # build + default smoke test
#   ./scripts/dev.sh path/to/pre-cpp.c   # build + compile a specific pre-preprocessed C file
#
# Note: the build has no preprocessor (sdcpp isn't built), so input must be already-preprocessed C;
# we feed it through --c1mode (cpp'd C on stdin -> asm).

set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"

if [ ! -f "${SDCC}/config.status" ]; then
  echo ">> build tree not configured — running full ./build.sh"
  exec "${REPO}/build.sh"
fi

echo ">> overlaying src/s1c88 sources"
cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
   "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
   "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"

echo ">> make -C src"
if ! make -C "${SDCC}/src" > /tmp/skipc-build.log 2>&1; then
  echo "!! BUILD FAILED — last errors:"
  grep -iE "error:|Error [0-9]+" /tmp/skipc-build.log | head -20
  echo "   (full log: /tmp/skipc-build.log)"
  exit 1
fi
warns=$(grep -c "warning:" /tmp/skipc-build.log || true)
echo ">> build OK (${warns} warnings) — $(${SDCCBIN} --version 2>&1 | head -1)"

SRC="${1:-}"
echo ">> codegen smoke test"
if [ -n "${SRC}" ]; then
  "${SDCCBIN}" -ms1c88 --c1mode -o /tmp/skipc-smoke.asm < "${SRC}" 2>/tmp/skipc-smoke.err
else
  printf 'int add1(int x){return x+1;}\nchar g; void store(char c){g=c;}\n' \
    | "${SDCCBIN}" -ms1c88 --c1mode -o /tmp/skipc-smoke.asm 2>/tmp/skipc-smoke.err
fi
rc=$?
if [ ${rc} -ne 0 ]; then
  echo "!! codegen failed (exit ${rc}):"; cat /tmp/skipc-smoke.err; exit 1
fi
echo ">> codegen OK -> /tmp/skipc-smoke.asm"
awk '/^_[a-z]/{f=1} f{print} /\tret/{if(f)print "---"; f=0}' /tmp/skipc-smoke.asm | head -40
echo ">> GREEN"
