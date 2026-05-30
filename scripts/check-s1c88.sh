#!/usr/bin/env bash
#
# check-s1c88.sh — verification meter for the S1C88 codegen retarget.
#
# Scans emitted assembly for z80-only register/idiom residue that Steps 2-3 of the
# retarget must eliminate (the z80 DE/BC pairs, the non-existent IX/IY byte halves,
# and the z80 `ex de,hl` idiom), and prints a per-token count plus the functions
# that still carry residue.
#
# This is a PROGRESS / REGRESSION signal, not a correctness proof — a real S1C88
# assembler is the eventual validator (see docs/s1c88/abi-decision.md, "Step 2").
# As gen.c is retargeted function-by-function, TOTAL should trend to 0.
#
#   scripts/check-s1c88.sh [file.asm]        # default: /tmp/skipc-smoke.asm
#   scripts/check-s1c88.sh --gate [file.asm] # exit non-zero if any residue remains
#
set -uo pipefail
GATE=0
[ "${1:-}" = "--gate" ] && { GATE=1; shift; }
ASM="${1:-/tmp/skipc-smoke.asm}"
[ -f "$ASM" ] || { echo "!! no asm file: $ASM" >&2; exit 2; }

# label -> extended regex (emitted asm is lowercase; match instruction operands only)
labels=(  "de pair"  "bc pair"  "iyl/iyh"    "ixl/ixh"    "ex de,hl"                         "byte d"  "byte e" )
regexes=( '\bde\b'   '\bbc\b'   '\biy[lh]\b' '\bix[lh]\b' 'ex[[:space:]]+de,[[:space:]]*hl'  '\bd\b'   '\be\b'  )

# instruction lines = tab/space-indented starting with a lowercase mnemonic; strip comments
INSN="$(grep -E '^[[:space:]]+[a-z]' "$ASM" 2>/dev/null | sed 's/;.*//')"

echo "== S1C88 codegen meter: $ASM =="
total=0
for i in "${!labels[@]}"; do
  n=$(printf '%s\n' "$INSN" | grep -oE "${regexes[$i]}" | wc -l | tr -d ' ')
  printf "  %-10s %5d\n" "${labels[$i]}" "$n"
  # byte d / byte e are noisy; exclude from the gating TOTAL (still shown above)
  case "${labels[$i]}" in "byte d"|"byte e") : ;; *) total=$((total + n));; esac
done
echo "  ---------- -----"
printf "  %-10s %5d   (de/bc/iy?/ix?/ex — the Step-2 residue)\n" "TOTAL" "$total"

echo "== functions with pair/half-reg residue =="
awk '
  /^_[A-Za-z]/ { fn=$0; sub(/:.*/,"",fn) }
  /^[[:space:]]+[a-z]/ {
    l=$0; sub(/;.*/,"",l)            # awk has no \b; use explicit non-letter boundaries
    if (l ~ /(^|[^a-z])(de|bc|iyl|iyh|ixl|ixh)([^a-z]|$)/) hit[fn]++
  }
  END { n=0; for (f in hit) { printf "  %-24s %d\n", f, hit[f]; n++ }
        if (n==0) print "  (none)" }
' "$ASM" | sort

[ "$GATE" = 1 ] && [ "$total" -gt 0 ] && exit 1
exit 0
