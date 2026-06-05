#!/usr/bin/env bash
#
# link-smoke.sh — prove the sdcc88 assemble->link pipeline (sdas88 + sdldz80) end to end.
#
# Assembles a tiny two-area program with sdas88 and links it with sdldz80, then checks that the
# linker resolved both relocation kinds the toolchain (and the banked-branch feature) rely on:
#   * cross-area ABSOLUTE 16-bit  (`ld ba,#_data`)         -> R_WORD,  filled with _data's address
#   * cross-area PC-RELATIVE call  (`carl _far`)           -> R_PCR,   filled with the signed disp
# Exits 0 iff the link succeeds and both resolved bytes match the expected layout.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
SDLD="${REPO}/build/sdcc-4.5.0/bin/sdldz80"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }
[ -x "$SDLD" ] || { echo "!! sdldz80 not built — run scripts/build-sdld.sh"      >&2; exit 2; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/sm.asm" <<'EOF'
	.module linksmoke
	.area _CODE
_main::
	ld	ba, #_data	; cross-area absolute -> R_WORD
	carl	_far		; cross-area relative -> R_PCR
	ret
	.area _CODE2
_far::
	ret
	.area _DATA
_data::
	.dw	0xBEEF
EOF

# _CODE@0x2100, _CODE2@0x2300, _DATA@0x1000 :
#   ld ba,#_data  -> C4 00 10                       (_data = 0x1000)
#   carl _far     -> F2 (disp field at 0x2104; the S1C88 16-bit branch base is
#                    field+1 — PC←PC+qqrr+2 from the instruction head, one less
#                    than the z80 next-instruction base (Epson §4.3.3 / PokeMini
#                    JMPS): 0x2300-(0x2104+1)=0x01FB) -> F2 FB 01
"$SDAS" -o "$tmp/sm.rel" "$tmp/sm.asm" >/dev/null 2>&1 || { echo "FAIL: assemble"; exit 1; }
err="$("$SDLD" -nwxi -b _CODE=0x2100 -b _CODE2=0x2300 -b _DATA=0x1000 "$tmp/sm.ihx" "$tmp/sm.rel" 2>&1)"
[ $? -eq 0 ] || { echo "FAIL: link: $err"; exit 1; }

code="$(grep -E '^:..2100' "$tmp/sm.ihx" | tr -d '\r')"
echo "  _CODE@2100 record: $code"
fail=0
case "$code" in
  *C40010*) echo "  ok  ld ba,#_data resolved to 0x1000 (C4 00 10)";;
  *)        echo "  FAIL ld ba,#_data not resolved (expected C4 00 10)"; fail=1;;
esac
case "$code" in
  *F2FB01*) echo "  ok  carl _far resolved to disp 0x01FB (F2 FB 01)";;
  *)        echo "  FAIL carl _far disp wrong (expected F2 FB 01)"; fail=1;;
esac
[ "$fail" -eq 0 ] && echo "== assemble->link pipeline GREEN ==" || echo "== pipeline BROKEN =="
exit "$fail"
