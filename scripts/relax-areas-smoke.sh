#!/usr/bin/env bash
#
# relax-areas-smoke.sh — lock WHAT the linker's cross-module relaxation (#14c) is
# allowed to move, and that the vector-slot NOP reorder (#14d) never fires on a
# slot that straddles a T record.
#
# Relaxation drops the 3 `ld nb` bytes of a same-bank cross-module bcall/bjump
# and reflows the code stream behind it.  "Behind it" means the rest of that
# area plus the relocatable areas the linker laid out sequentially after it —
# NEVER an area the user pinned with `-b AREA=addr`, an area on the far side of
# a layout gap, or another bank.  Before the 2026-10 fix the delta was purely
# address-based (every byte above a drop in the same 64 KB block), so a `__far`
# table at `-b _FAR=0x18800` behind two droppable bcalls in `_CODE_1=0x18000`
# moved to 0x187FA and romgen placed it at the wrong physical address.
#
# The reorder moves an ABS vector slot's branch to its head when the `ld nb`
# would be NOP'd.  Its two relocations cooperate across a static flag; when the
# slot's tail (branch op + disp16) sits in the NEXT T record the flag leaked
# into that record and the rewrite landed on the record's address bytes.  Such
# a slot must now link byte-identically to the legacy (SDLD_NO_VECREORDER) form.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
SDLD="${REPO}/build/sdcc-4.5.0/bin/sdldz80"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }
[ -x "$SDLD" ] || { echo "!! sdldz80 not built — run scripts/build-sdld.sh" >&2; exit 2; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
fail=0
ok()   { printf "  ok    %s\n" "$1"; }
bad()  { printf "  FAIL  %s\n" "$1"; fail=1; }

# symbol address from a .map ("  018800  _ftbl  fb1" style rows)
symaddr() { gawk -v s="$2" '$2 == s { print toupper($1); exit }' "$1"; }
# area base from a .map ("_FAR   018800  000004 = ..." rows)
areabase() { gawk -v a="$2" '$1 == a { print toupper($2); exit }' "$1"; }

# ---------------------------------------------------------------------------
# 1. Two modules in bank 1 with two droppable same-bank bcalls, then a __far
#    table in an explicitly based area higher in the same bank, and a second
#    relocatable area placed SEQUENTIALLY (no -b) right behind the code.
#      - _FAR (pinned) must stay at 0x18800
#      - _TAIL (sequential) must move down by the 6 reclaimed bytes
#      - the code's `ld hl,#_ftbl` must load 0x8800 (what the far read uses)
# ---------------------------------------------------------------------------
cat > "$tmp/m1.asm" <<'EOF'
	.module m1
	.globl b1_other
	.globl _ftbl
	.globl tail_sym
	.area _CODE_1
b1_fn::
	bcall b1_other		; cross-module, same bank -> dropped
	bcall b1_other		; cross-module, same bank -> dropped
	ld hl,#_ftbl
	ld a,#((_ftbl) >> 16)
	ret
	.area _TAIL
tail_sym::
	.db 0x11
	.area _FAR
_ftbl::
	.db 0xA5, 0x5A, 0x11, 0x22
EOF
cat > "$tmp/m2.asm" <<'EOF'
	.module m2
	.area _CODE_1
b1_other::
	ld a,#0x77
	ret
EOF
"$SDAS" -o "$tmp/m1.rel" "$tmp/m1.asm" && "$SDAS" -o "$tmp/m2.rel" "$tmp/m2.asm" || { echo "FAIL: assemble"; exit 1; }
"$SDLD" -nwxi -m -b _CODE=0x21D0 -b _CODE_1=0x18000 -b _FAR=0x18800 "$tmp/r.ihx" "$tmp/m1.rel" "$tmp/m2.rel" >/dev/null 2>&1 \
	|| { echo "FAIL: link (relax)"; exit 1; }
( SDLD_NO_RELAX=1; export SDLD_NO_RELAX
  "$SDLD" -nwxi -m -b _CODE=0x21D0 -b _CODE_1=0x18000 -b _FAR=0x18800 "$tmp/n.ihx" "$tmp/m1.rel" "$tmp/m2.rel" >/dev/null 2>&1 ) \
	|| { echo "FAIL: link (no relax)"; exit 1; }

far_r="$(symaddr "$tmp/r.map" _ftbl)"; far_n="$(symaddr "$tmp/n.map" _ftbl)"
[ "$far_r" = "018800" ] && ok "pinned _FAR symbol stays at 0x18800 under relaxation" \
	|| bad "pinned _FAR symbol moved: relax=$far_r legacy=$far_n"
tail_r="$(symaddr "$tmp/r.map" tail_sym)"; tail_n="$(symaddr "$tmp/n.map" tail_sym)"
# the two drops reclaim 6 bytes; _TAIL follows _CODE_1 sequentially
if [ -n "$tail_r" ] && [ -n "$tail_n" ] && [ $((0x$tail_n - 0x$tail_r)) -eq 6 ]; then
	ok "sequential _TAIL moves down by the 6 reclaimed bytes ($tail_n -> $tail_r)"
else
	bad "sequential _TAIL did not reflow by 6: relax=$tail_r legacy=$tail_n"
fi
# the far table's bytes must be AT the pinned address in the image
if grep -qi '^:04880000A55A1122' "$tmp/r.ihx"; then
	ok "far table bytes emitted at 0x8800 (bank 1)"
else
	bad "far table bytes not at 0x8800: $(grep -i 'A55A1122' "$tmp/r.ihx")"
fi
# the code's `ld hl,#_ftbl` (C5 ll hh) must load 0x8800
code_rec="$(grep -i '^:..8000' "$tmp/r.ihx" | head -1 | tr 'a-f' 'A-F')"
case "$code_rec" in
	*C50088*) ok "ld hl,#_ftbl relocates to 0x8800 (not shifted by the drops)" ;;
	*)        bad "ld hl,#_ftbl mis-relocated in: $code_rec" ;;
esac

# ---------------------------------------------------------------------------
# 2. An explicitly based second code area in bank 0 must not move.
# ---------------------------------------------------------------------------
cat > "$tmp/c2a.asm" <<'EOF'
	.area _CODE
	.globl o
f::
	bcall o
	bcall o
	ret
	.area _CODE2
g::
	ret
EOF
printf '\t.area _CODE\no::\n\tret\n' > "$tmp/c2b.asm"
"$SDAS" -o "$tmp/c2a.rel" "$tmp/c2a.asm" && "$SDAS" -o "$tmp/c2b.rel" "$tmp/c2b.asm" || { echo "FAIL: assemble"; exit 1; }
"$SDLD" -nwxi -m -b _CODE=0x21D0 -b _CODE2=0x2300 "$tmp/c2.ihx" "$tmp/c2a.rel" "$tmp/c2b.rel" >/dev/null 2>&1 || { echo "FAIL: link"; exit 1; }
g_addr="$(symaddr "$tmp/c2.map" g)"
[ "$g_addr" = "002300" ] && ok "explicit -b _CODE2=0x2300 honoured under relaxation" \
	|| bad "explicit -b _CODE2 base moved to $g_addr"

# ---------------------------------------------------------------------------
# 3. Vector slots at every offset around the T-record boundary: a slot whose
#    6 bytes are NOT all in one record must link byte-identically to the legacy
#    (no-reorder) layout; a slot wholly inside a record is reordered.
# ---------------------------------------------------------------------------
reordered=0; fallback=0
for n in 5 6 7 8 9 10 11 12 13; do
	printf '\t.globl t1\n\t.area _VEC (ABS)\n\t.org 0x2102\n\t.rept %d\n\tnop\n\t.endm\n\tbjump t1\n\tnop\n\tnop\n\t.area _CODE\nt1:: ret\n' "$n" > "$tmp/v$n.asm"
	"$SDAS" -o "$tmp/v$n.rel" "$tmp/v$n.asm" || { bad "assemble v$n"; continue; }
	"$SDLD" -nwxi -b _CODE=0x2200 "$tmp/v$n.ihx" "$tmp/v$n.rel" >/dev/null 2>&1 || { bad "link v$n"; continue; }
	( SDLD_NO_VECREORDER=1; export SDLD_NO_VECREORDER
	  "$SDLD" -nwxi -b _CODE=0x2200 "$tmp/v${n}n.ihx" "$tmp/v$n.rel" >/dev/null 2>&1 ) || { bad "link v${n} legacy"; continue; }
	# every record must be a sane data record: the slot's jrl must target t1 (0x2200)
	# in BOTH layouts — decode the bytes at the slot and check the displacement.
	bytes="$(gawk '/^:/ { ll=strtonum("0x" substr($0,2,2)); if (substr($0,8,2)=="00" && strtonum("0x" substr($0,4,4))==0x2102) print toupper(substr($0,10,ll*2)) }' "$tmp/v$n.ihx")"
	slot="${bytes:$((n*2)):12}"
	if [ "${slot:0:2}" = "F3" ]; then          # reordered: F3 lo hi FF FF FF, jrl op at 0x2102+n
		lo="${slot:2:2}"; hi="${slot:4:2}"; op=$((0x2102 + n))
	elif [ "${slot:6:2}" = "F3" ]; then        # legacy: FF FF FF F3 lo hi, jrl op at 0x2102+n+3
		lo="${slot:8:2}"; hi="${slot:10:2}"; op=$((0x2102 + n + 3))
	else
		bad "slot v$n unrecognised: $slot (record: $bytes)"; continue
	fi
	disp=$((0x$hi * 256 + 0x$lo)); [ $disp -ge 32768 ] && disp=$((disp - 65536))
	tgt=$((op + 1 + 1 + disp))                 # jrl: PC <- (disp addr) + 1 + disp16; disp addr = op+1
	[ $tgt -eq $((0x2200)) ] || bad "slot v$n branch resolves to $(printf 0x%04X $tgt), expected 0x2200 ($slot)"
	if cmp -s "$tmp/v$n.ihx" "$tmp/v${n}n.ihx"; then fallback=$((fallback+1)); else reordered=$((reordered+1)); fi
done
[ "$reordered" -gt 0 ] && [ "$fallback" -gt 0 ] \
	&& ok "vector slots: $reordered reordered (whole slot in one record), $fallback legacy fallbacks (straddling), all branch to t1" \
	|| bad "vector slots: reordered=$reordered fallback=$fallback — expected both kinds across the record boundary"

[ "$fail" -eq 0 ] && echo "== relax-areas smoke GREEN ==" || echo "== relax-areas smoke FAILED =="
exit "$fail"
