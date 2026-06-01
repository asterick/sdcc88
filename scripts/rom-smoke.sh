#!/usr/bin/env bash
#
# rom-smoke.sh — end-to-end banked toolchain test: assemble (sdas88) -> link (sdldz80) -> romgen ->
# flat .min, and verify a multi-bank program's auto-banked bcall + physical placement.
#
# Bank layout (Pokémon Mini): code areas live at linker address (bank<<16)|logic.
#   _HOME    bank 0 (common)  -> 0x2100      (logic == physical)
#   _CODE_1  bank 1           -> 0x18000     ((1<<16)|0x8000)
#   _CODE_2  bank 2           -> 0x28000     ((2<<16)|0x8000)
# romgen maps those to physical: bank0 logic; bankN -> N*0x8000 + (logic & 0x7FFF).
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"; SDLD="${REPO}/build/sdcc-4.5.0/bin/sdldz80"
[ -x "$SDAS" ] && [ -x "$SDLD" ] || { echo "!! build sdas88 + sdldz80 first" >&2; exit 2; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/r.asm" <<'EOF'
	.module rom
	.area _HOME
_start::
	bcall	_b1fn		; -> bank 1
	bcall	_b2fn		; -> bank 2
	ret
	.area _CODE_1
_b1fn::
	ld	a, #0x11
	ret
	.area _CODE_2
_b2fn::
	ld	a, #0x22
	ret
EOF
"$SDAS" -o "$tmp/r.rel" "$tmp/r.asm" >/dev/null 2>&1 || { echo "FAIL: assemble"; exit 1; }
"$SDLD" -nwxi -b _HOME=0x2100 -b _CODE_1=0x18000 -b _CODE_2=0x28000 "$tmp/r.ihx" "$tmp/r.rel" >/dev/null 2>&1 \
  || { echo "FAIL: link"; exit 1; }
python3 "${REPO}/scripts/romgen.py" "$tmp/r.ihx" "$tmp/r.min" || { echo "FAIL: romgen"; exit 1; }

# Verify with a hexdump of the three physical regions.
hx() { od -An -tx1 -j "$1" -N "$2" "$tmp/r.min" | tr -s ' ' | sed 's/^ //'; }
home="$(hx 0 8)"        # _start @ physical 0x2100 (file 0)
b1="$(hx $((0x8000-0x2100)) 3)"    # _b1fn  @ physical 0x08000
b2="$(hx $((0x10000-0x2100)) 3)"   # _b2fn  @ physical 0x10000
echo "  _start  (phys 0x02100): $home"
echo "  _b1fn   (phys 0x08000): $b1"
echo "  _b2fn   (phys 0x10000): $b2"
fail=0
case "$home" in "ce c4 01 ff f2"*) echo "  ok  bcall _b1fn -> ld nb,#1 ; nop ; carl";; *) echo "  FAIL bcall _b1fn bank/slot"; fail=1;; esac
case "$b1"   in "b0 11 f8")        echo "  ok  _b1fn placed in bank 1 (ld a,#0x11 ; ret)";;  *) echo "  FAIL _b1fn placement"; fail=1;; esac
case "$b2"   in "b0 22 f8")        echo "  ok  _b2fn placed in bank 2 (ld a,#0x22 ; ret)";;  *) echo "  FAIL _b2fn placement"; fail=1;; esac
[ "$fail" -eq 0 ] && echo "== banked ROM pipeline GREEN ==" || echo "== ROM pipeline BROKEN =="
exit "$fail"
