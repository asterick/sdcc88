#!/usr/bin/env bash
#
# branch-smoke.sh — lock down the S1C88 relative-branch displacement convention.
#
# The S1C88 computes a taken relative branch as PC <- PC(after full fetch) + disp - 1
# (Epson §4.3.3 semantics; PokeMini MinxCPU JMPS).  That makes an 8-bit disp relative
# to the ADDRESS OF THE DISP BYTE ITSELF and a 16-bit disp relative to (first disp
# byte + 1) — one byte EARLIER than the z80 next-instruction convention the ASxxxx
# base uses.  sdas88 was originally off by one here (every branch landed 1 byte
# short); this test assembles every relative-branch form at fixed addresses and
# byte-compares the emitted displacements against hand-computed hardware-correct
# values, so the convention can never silently regress.
#
# Layout (area _CODE, org 0; b = 0x00, f = 0x1C) and the hand-computed disps
# are spelled out next to the EXPECT string below.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/br.asm" <<'EOF'
	.area _CODE
b:	nop
	jrs	f
	jrs	C, b
	jrs	LT, f
	djr	nz, b
	cars	f
	carl	b
	jrl	f
	jp	GE, f
	jp	NZ, b
	nop
f:	ret
EOF

"$SDAS" -l -o "$tmp/br.rel" "$tmp/br.asm" || { echo "FAIL: assemble"; exit 1; }

# Concatenated code bytes from the listing.
got="$(grep -E '^ +[0-9A-F]{8} ' "$tmp/br.lst" | cut -c14-37 | tr -d ' \n' | tr 'a-f' 'A-F')"

# Hand-computed displacements (8-bit rr = target - rr_addr; 16-bit qqrr =
# target - first_disp_addr - 1):
#   00: FF        b: nop
#   01: F1 1A        jrs f       rr@02   = 0x1C-0x02   = 0x1A
#   03: E4 FC        jrs C,b     rr@04   = 0x00-0x04   = 0xFC
#   05: CE E0 15     jrs LT,f    rr@07   = 0x1C-0x07   = 0x15
#   08: F5 F7        djr nz,b    rr@09   = 0x00-0x09   = 0xF7
#   0A: F0 11        cars f      rr@0B   = 0x1C-0x0B   = 0x11
#   0C: F2 F2 FF     carl b      qqrr@0D = 0x00-0x0D-1 = 0xFFF2
#   0F: F3 0B 00     jrl f       qqrr@10 = 0x1C-0x10-1 = 0x000B
#   12: CE E0 04     jp GE,f  -> jrs LT,+4 (skip target = 0x14+4 = 0x18 = next)
#   15: F3 05 00              -> jrl f    qqrr@16 = 0x1C-0x16-1 = 0x0005
#   18: EF E6 FF     jp NZ,b  -> jrl NZ   qqrr@19 = 0x00-0x19-1 = 0xFFE6
#   1B: FF           nop
#   1C: F8        f: ret
expect="FFF11AE4FCCEE015F5F7F011F2F2FFF30B00CEE004F30500EFE6FFFFF8"

if [ "$got" = "$expect" ]; then
	echo "== branch displacement convention GREEN =="
	exit 0
else
	echo "FAIL: branch encodings drifted"
	echo "  expect: $expect"
	echo "  got:    $got"
	exit 1
fi
