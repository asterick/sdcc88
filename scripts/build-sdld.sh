#!/usr/bin/env bash
#
# build-sdld.sh — build the sdld (ASlink) linker in the configured SDCC tree.
#
# sdcc88 targets SDCC's own sdas/sdld. sdas88 emits the standard ASxxxx z80-dialect .rel object
# format, so the matching linker is the z80 variant: ASlink is ONE binary (build/.../bin/sdld) whose
# target is selected at runtime from argv[0] (sdld_init() in linksrc/sdld.c scans the name for
# "z80"/"gb"/"8051"/...). The variants (sdldz80, sdldgb, ...) are plain copies of sdld; sdldz80
# selects TARGET_ID_Z80 — z80-like relocation/area behavior, which is exactly what our objects need.
#
# So our linker IS sdldz80 today. The distinct s1c88-branded `sdld88` (a new TARGET_ID_S1C88, z80-like
# plus the banked-branch rewrite relocation) lands with the Phase-2 work in docs/s1c88/banked-branch.md,
# which patches sdld.c regardless.
#
#   scripts/build-sdld.sh           # build bin/sdld + bin/sdldz80
#
# Verify the assemble->link pipeline with scripts/link-smoke.sh.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"

[ -f "${SDCC}/config.status" ] || { echo "!! tree not configured — run ./build.sh first" >&2; exit 1; }

echo ">> make -C sdas/linksrc sdcc-ldz80"
make -C "${SDCC}/sdas/linksrc" sdcc-ldz80 > /tmp/sdld88-build.log 2>&1 || {
  echo "!! linker build FAILED — last errors:"; grep -iE "error:|Error [0-9]+" /tmp/sdld88-build.log | head -20
  echo "   (full log: /tmp/sdld88-build.log)"; exit 1; }

LD="${SDCC}/bin/sdldz80"
[ -x "${LD}" ] || { echo "!! expected ${LD} not produced" >&2; exit 1; }
echo ">> built: ${LD} ($(stat -c%s "${LD}") bytes)  — invoke as the z80-target ASlink"
