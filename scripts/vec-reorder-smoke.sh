#!/usr/bin/env bash
#
# vec-reorder-smoke.sh — lock the banked-branch vector-slot NOP reorder.
#
# An ABSOLUTE-area `bjump`/`bcall` to a same-/common-bank target can't be
# byte-dropped (the IRQ/reset vector table is hardware-pinned), so the linker
# fills its `ld nb` with NOPs.  sdcc88 places those NOPs at the slot TAIL —
# `<branch> ; nop nop nop` — instead of the head, so an unconditional dispatch
# never executes them (banked-branch.md §10).  The displacement of the relocated
# branch grows by exactly the 3 bytes it shifted earlier.  This test links a
# fixed ABS `bjump` both ways (default reorder + SDLD_NO_VECREORDER legacy) and
# locks both the byte placement and the +3 displacement relationship, so the
# reorder (and its disp math) can never silently regress.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
SDLD="${REPO}/build/sdcc-4.5.0/bin/sdldz80"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }
[ -x "$SDLD" ] || { echo "!! sdldz80 not built — run scripts/build-sdld.sh" >&2; exit 2; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

# One ABS bjump slot at a fixed org, common-bank target in another area.
cat > "$tmp/v.asm" <<'EOF'
	.area _V (ABS)
	.org	0x0200
v:	bjump	tgt
	.area _CODE
tgt:	ret
EOF
"$SDAS" -o "$tmp/v.rel" "$tmp/v.asm" || { echo "FAIL: assemble"; exit 1; }

# Extract the 6 slot bytes (the only bytes in ABS area _V at 0x0200) from the
# linked .ihx — concatenate type-00 record data in file order, then take the
# record whose load address is 0x0200.
slot() {  # $1 = extra linker env already exported
	"$SDLD" -nwxi -b _CODE=0x0100 "$tmp/v.ihx" "$tmp/v.rel" >/dev/null 2>&1 \
		|| { echo "FAIL: link"; exit 1; }
	gawk '
	  /^:/ {
	    ll = strtonum("0x" substr($0,2,2));
	    addr = strtonum("0x" substr($0,4,4));
	    t = substr($0,8,2);
	    if (t != "00") next;
	    if (addr == 0x0200) { print toupper(substr($0, 10, ll*2)); }
	  }' "$tmp/v.ihx"
}

reordered="$(slot)"
legacy="$(SDLD_NO_VECREORDER=1; export SDLD_NO_VECREORDER; slot)"

fail=0
# Reordered: <op> <lo> <hi> FF FF FF  (op = F3 jrl unconditional)
if [ "${reordered:0:2}" = "F3" ] && [ "${reordered:6:6}" = "FFFFFF" ]; then
	:
else
	echo "FAIL: reordered slot not <branch> ; nop nop nop  (got $reordered)"; fail=1
fi
# Legacy: FF FF FF <op> <lo> <hi>
if [ "${legacy:0:6}" = "FFFFFF" ] && [ "${legacy:6:2}" = "F3" ]; then
	:
else
	echo "FAIL: legacy slot not nop nop nop ; <branch>  (got $legacy)"; fail=1
fi
# Displacement grows by exactly 3 (the branch shifted 3 bytes earlier).
rd=$(( 0x${reordered:4:2}${reordered:2:2} ))     # reordered disp (hi:lo @ bytes 1,2)
ld=$(( 0x${legacy:10:2}${legacy:8:2} ))          # legacy   disp (hi:lo @ bytes 4,5)
if [ $(( (rd - ld) & 0xFFFF )) -ne 3 ]; then
	echo "FAIL: disp delta != 3 (reordered=$rd legacy=$ld)"; fail=1
fi

if [ "$fail" = 0 ]; then
	echo "== vector-slot NOP reorder GREEN  (reordered=$reordered legacy=$legacy) =="
	exit 0
fi
exit 1
