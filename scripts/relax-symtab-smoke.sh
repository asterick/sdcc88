#!/usr/bin/env bash
#
# relax-symtab-smoke.sh — lock the #14e symbol-table fix.
#
# #14c relaxation drops a cross-module same-bank `bcall`/`bjump`'s `ld nb` (3
# bytes) at link time and reflows the ROM down, but it does NOT update the
# linker's s_addr/a_addr model — so the .map symbol table would print the stale,
# pre-relax (too-high) address for any symbol past a drop.  #14e fixes the .map
# writer to apply the same rlxDelta the emit path uses.
#
# This test links two modules with a relaxable cross-module bcall and a symbol
# AFTER it, both with relaxation (default) and without (SDLD_NO_RELAX=1), and
# asserts:
#   - the post-drop symbol's relaxed .map address is exactly 3 bytes LOWER than
#     its no-relax address (it tracks the dropped ld nb) — pre-#14e they were
#     identical (the bug);
#   - a symbol BEFORE the drop is unchanged either way.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
SDLD="${REPO}/build/sdcc-4.5.0/bin/sdldz80"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }
[ -x "$SDLD" ] || { echo "!! sdldz80 not built — run scripts/build-sdld.sh" >&2; exit 2; }

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

# Module A: a symbol before the relaxable bcall, the bcall, a symbol after it.
cat > "$tmp/a.asm" <<'EOF'
	.globl	far_fn
	.area	_CODE
before::
	nop
	bcall	far_fn          ; cross-module, same bank -> #14c drops its `ld nb`
after::
	ret
EOF
# Module B: the bcall target (same bank).
cat > "$tmp/b.asm" <<'EOF'
	.area	_CODE
far_fn::
	ret
EOF

"$SDAS" -o "$tmp/a.rel" "$tmp/a.asm" || { echo "FAIL: assemble a"; exit 1; }
"$SDAS" -o "$tmp/b.rel" "$tmp/b.asm" || { echo "FAIL: assemble b"; exit 1; }

# map_addr <mapfile> <symbol> -> hex address (lowercased, no leading zeros normalized)
map_addr() {
	awk -v s="$2" '$2 == s { print $1 }' "$1" | head -1 | sed 's/^0*//;s/^$/0/' | tr 'A-F' 'a-f'
}

link() { # $1=extra-env-prefix(empty or SDLD_NO_RELAX=1) $2=mapfile-out
	env $1 "$SDLD" -nmwxi -b _CODE=0x21D0 "$tmp/out.ihx" "$tmp/a.rel" "$tmp/b.rel" >/dev/null 2>&1 \
		|| { echo "FAIL: link ($1)"; exit 1; }
	cp "$tmp/out.map" "$2"
}

link ""               "$tmp/relax.map"
link "SDLD_NO_RELAX=1" "$tmp/norelax.map"

bf_r=$(map_addr "$tmp/relax.map"   before); af_r=$(map_addr "$tmp/relax.map"   after)
bf_n=$(map_addr "$tmp/norelax.map" before); af_n=$(map_addr "$tmp/norelax.map" after)

# `before` (pre-drop) must be identical in both layouts.
if [ "$bf_r" != "$bf_n" ]; then
	echo "FAIL: 'before' moved under relaxation ($bf_r vs $bf_n) — it is pre-drop"; exit 1
fi
# `after` (post-drop) relaxed address must be 3 bytes below the no-relax model.
want=$(printf '%x' $(( 0x$af_n - 3 )))
if [ "$af_r" != "$want" ]; then
	echo "FAIL: #14e stale .map — 'after' reads 0x$af_r relaxed (model 0x$af_n), expected 0x$want"
	echo "      (pre-#14e the relaxed .map printed the un-relaxed 0x$af_n)"; exit 1
fi

echo "relax-symtab ok: before=0x$bf_r (fixed), after=0x$af_r relaxed = 0x$af_n model - 3"
exit 0
