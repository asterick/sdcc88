#!/usr/bin/env bash
#
# rom-smoke.sh — end-to-end banked toolchain test: assemble (sdas88) -> link (sdldz80) -> romgen ->
# flat .min, and verify a multi-bank program's auto-banked bcall + physical placement.
# Also packs the same link into the MINX debug container (romgen's second output format,
# docs/s1c88/minx-format.md) and verifies its structure, CRCs, symtab, and that the ROM
# section is byte-identical to the flat .min.
#
# Bank layout (Pokémon Mini): code areas live at linker address (bank<<16)|logic.
#   _HOME    bank 0 (common)  -> 0x2100      (logic == physical)
#   _CODE_1  bank 1           -> 0x18000     ((1<<16)|0x8000)
#   _CODE_2  bank 2           -> 0x28000     ((2<<16)|0x8000)
# romgen maps those to physical: bank0 logic; bankN -> N*0x8000 + (logic & 0x7FFF).
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDAS="${SDCC}/bin/sdas88"; SDLD="${SDCC}/bin/sdldz80"
[ -x "$SDAS" ] && [ -x "$SDLD" ] || { echo "!! build sdas88 + sdldz80 first" >&2; exit 2; }
[ -x "${SDCC}/bin/romgen" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null

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
	.area _FAR
_ftbl::
	.db 0xA5
	.db 0x5A
	.area _HOME
_farrd::
	ld	hl, #_ftbl	; __far read: offset...
	ld	a, #((_ftbl) >> 16)	; ...page byte via the R_BYT3/R_HIB reloc
	ld	ep, a
	ld	a, (hl)
	ld	ep, #0x00
	ret
_ccfn::
	bjump	lt, _b2fn	; cross-bank CONDITIONAL: short-only signed cc to a
				; bank-2 target -> invert-and-skip + ld nb,#2 written
	ret
EOF
"$SDAS" -o "$tmp/r.rel" "$tmp/r.asm" >/dev/null 2>&1 || { echo "FAIL: assemble"; exit 1; }
# _FAR is __far data: located at its PHYSICAL address (bank 3 = phys 0x18000..),
# declared to romgen with --far so it bypasses the (bank<<16)|logic code mapping.
# -j/-m also emit r.noi/r.map, the sidecars the MINX leg below rolls up
"$SDLD" -nwxjmi -b _HOME=0x2100 -b _CODE_1=0x18000 -b _CODE_2=0x28000 -b _FAR=0x18800 "$tmp/r.ihx" "$tmp/r.rel" >/dev/null 2>&1 \
  || { echo "FAIL: link"; exit 1; }
"${SDCC}/bin/romgen" "$tmp/r.ihx" "$tmp/r.min" --far=0x18800-0x18fff || { echo "FAIL: romgen"; exit 1; }

# Verify with a hexdump of the three physical regions. Rejoin od's fields with
# awk: BSD od (macOS) pads the line with trailing whitespace, which broke the
# exact-match case patterns below under `tr -s ' ' | sed 's/^ //'`.
hx() { od -An -tx1 -j "$1" -N "$2" "$tmp/r.min" | awk '{for(i=1;i<=NF;i++)printf "%s%s",(c++?" ":""),$i}'; }
home="$(hx 0 8)"        # _start @ physical 0x2100 (file 0)
b1="$(hx $((0x8000-0x2100)) 3)"    # _b1fn  @ physical 0x08000
b2="$(hx $((0x10000-0x2100)) 3)"   # _b2fn  @ physical 0x10000
fdata="$(hx $((0x18800-0x2100)) 2)"   # _ftbl @ physical 0x18800 (the __far block)
farrd="$(hx 13 11)"   # _farrd follows _start (two 6-byte bcall slots + ret = 13 B of _HOME)
ccfn="$(hx 25 7)"     # _ccfn follows _farrd (12 B): the conditional cross-bank bjump slot
echo "  _start  (phys 0x02100): $home"
echo "  _b1fn   (phys 0x08000): $b1"
echo "  _b2fn   (phys 0x10000): $b2"
echo "  _ftbl   (phys 0x18800): $fdata"
echo "  _farrd  (phys 0x0210d): $farrd"
echo "  _ccfn   (phys 0x02119): $ccfn"
fail=0
case "$home" in "ce c4 01 f2"*) echo "  ok  bcall _b1fn -> ld nb,#1 ; carl (6-byte slot)";; *) echo "  FAIL bcall _b1fn bank/slot"; fail=1;; esac
case "$b1"   in "b0 11 f8")        echo "  ok  _b1fn placed in bank 1 (ld a,#0x11 ; ret)";;  *) echo "  FAIL _b1fn placement"; fail=1;; esac
case "$b2"   in "b0 22 f8")        echo "  ok  _b2fn placed in bank 2 (ld a,#0x22 ; ret)";;  *) echo "  FAIL _b2fn placement"; fail=1;; esac
case "$fdata" in "a5 5a")          echo "  ok  __far data at its physical address (0x18800)";; *) echo "  FAIL __far data placement"; fail=1;; esac
# ld hl,#0x8800 ; ld a,#0x01 (page = 0x18800>>16) ; ld ep,a ; ld a,(hl) ; ld ep,#0 ; ret
case "$farrd" in "c5 00 88 b0 01 ce cd 45 ce c5 00"*) echo "  ok  __far page byte reloc resolved (ld a,#0x01)";; *) echo "  FAIL __far page byte"; fail=1;; esac
# bjump lt,_b2fn -> jrs ge,+7 (ce e3 07) ; ld nb,#2 (ce c4 02) ; jrl (f3 ..): the
# invert-and-skip + cross-bank NB write composed (TODO #13 worst case).
case "$ccfn" in "ce e3 07 ce c4 02 f3") echo "  ok  bjump lt,_b2fn -> jrs ge,+7 ; ld nb,#2 ; jrl (cross-bank conditional)";; *) echo "  FAIL conditional cross-bank bjump"; fail=1;; esac
# --- MINX container leg: same link packed as a chunk-tree debug bundle --------------
# minxdump validates structure + CRC + table invariants; we additionally check the
# extracted ROM is byte-identical to the flat .min and the banked/--far symbol
# mappings landed in the SYM chunk.
[ -x "${SDCC}/bin/minxdump" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
"${SDCC}/bin/romgen" "$tmp/r.ihx" "$tmp/r.minx" --far=0x18800-0x18fff >/dev/null || { echo "FAIL: romgen --minx"; exit 1; }
if "${SDCC}/bin/minxdump" --rom="$tmp/r.x.min" "$tmp/r.minx" > "$tmp/dump" \
   && cmp -s "$tmp/r.x.min" "$tmp/r.min" \
   && grep -q "_start .* value 0x002100 phys 0x002100" "$tmp/dump" \
   && grep -q "_b1fn .* value 0x018000 phys 0x008000" "$tmp/dump" \
   && grep -q "_ftbl .* value 0x018800 phys 0x018800" "$tmp/dump"
then echo "  ok  MINX container (minxdump valid, ROM==min, banked+far symtab)"
else echo "  FAIL MINX container"; sed -n '1,5p' "$tmp/dump" 2>/dev/null; fail=1; fi

[ "$fail" -eq 0 ] && echo "== banked ROM pipeline GREEN ==" || echo "== ROM pipeline BROKEN =="
exit "$fail"
