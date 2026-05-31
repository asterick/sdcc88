#!/usr/bin/env bash
#
# validate-s1c88.sh — byte-validate emitted codegen by assembling it with sdas88.
#
# sdas88 (sdas/as88, built by scripts/build-sdas.sh as88) is the real S1C88 assembler. Feeding it the
# codegen's emitted asm proves the output is legal S1C88 — and the instructions it REJECTS are exactly
# the z80-isms still in gen.c (the codegen-retarget to-do list). Complements check-s1c88.sh, which is a
# cheap textual residue meter; this one is the ground truth (catches wrong encodings/flags/forms too).
#
#   scripts/validate-s1c88.sh [file.asm]   # default: /tmp/sdcc88-smoke.asm
#
# Exits 0 iff the file assembles with no errors.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
ASM="${1:-/tmp/sdcc88-smoke.asm}"

[ -x "$SDAS" ] || { echo "!! sdas88 not built — run: scripts/build-sdas.sh as88" >&2; exit 2; }
[ -f "$ASM" ]  || { echo "!! no asm file: $ASM" >&2; exit 2; }

# Strip the --c1mode debug comment lines (e.g. ';(null):1: ERROR:') that aren't valid asm.
clean="$(mktemp)"; out="$(mktemp).rel"
grep -vE '^;\(null\)' "$ASM" > "$clean"

ni="$(grep -cE '^[[:space:]]+[a-z]' "$clean")"
# (sdas88 wants a real -o target; /dev/null makes it try to create /dev/null.asm.)
errout="$("$SDAS" -o "$out" "$clean" 2>&1 || true)"
ne="$(printf '%s\n' "$errout" | grep -c 'Error' || true)"

echo "== sdas88 validation: $ASM =="
echo "  instruction lines : $ni"
echo "  assembler errors  : $ne"
if [ "$ne" -gt 0 ]; then
	echo "== flagged instructions (codegen z80-isms / forms sdas88 doesn't encode yet) =="
	printf '%s\n' "$errout" \
	  | sed -nE 's/^[^:]*:([0-9]+): Error.*/\1/p' | sort -un \
	  | while read -r ln; do sed -n "${ln}p" "$clean"; done \
	  | sed 's/^[[:space:]]*//; s/[[:space:]]\+/ /g' \
	  | sort | uniq -c | sort -rn | sed 's/^/  /'
fi
rm -f "$clean" "$out"
[ "$ne" -eq 0 ]
