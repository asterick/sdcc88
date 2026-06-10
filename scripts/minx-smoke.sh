#!/usr/bin/env bash
#
# minx-smoke.sh — end-to-end test of the MINX debug container's C-level debug info:
# compile a C program with --debug through the real driver, pack the .minx, and verify
# with minxdump that the promoted-to-binary debug tables came through: the global
# line table (sorted by address), function extents, the embedded source text, and a
# ROM payload byte-identical to the flat .min. This is the "a debugger with no
# filesystem and no text parsing can step through source" guarantee.
#
#   scripts/minx-smoke.sh
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"
[ -x "$SDCCBIN" ] || { echo "!! build the compiler first (./build.sh)" >&2; exit 2; }
[ -x "${SDCC}/bin/sdcpp" ] || { echo "!! sdcpp missing (./build.sh builds it)" >&2; exit 2; }
[ -x "${SDCC}/bin/romgen" ] && [ -x "${SDCC}/bin/minxdump" ] || "${REPO}/scripts/build-romgen.sh" >/dev/null
export PATH="${SDCC}/bin:${PATH}"   # the driver finds sdcpp/sdas88/sdldz80 via PATH

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/game.c" <<'EOF'
/* minx-smoke marker: SOURCE_EMBED_CANARY */
static unsigned char counter;

unsigned char bump(unsigned char n)
{
    counter += n;
    return counter;
}

int main(void)
{
    unsigned char i;
    for (i = 0; i < 3; i++)
        bump(i);
    return counter;
}
EOF

cd "$tmp"
"$SDCCBIN" -ms1c88 --debug game.c -o game.ihx >/dev/null 2>&1 || { echo "FAIL: compile+link"; exit 1; }
"${SDCC}/bin/romgen" game.ihx game.min  >/dev/null || { echo "FAIL: romgen .min"; exit 1; }
"${SDCC}/bin/romgen" game.ihx game.minx >/dev/null || { echo "FAIL: romgen .minx"; exit 1; }

"${SDCC}/bin/minxdump" --rom=game.x.min game.minx > dump || { echo "FAIL: minxdump validation"; cat dump; exit 1; }

fail=0
ck() { # ck <label> <command...>
  local label="$1"; shift
  if "$@" >/dev/null 2>&1; then echo "  ok  $label"; else echo "  FAIL $label"; fail=1; fi
}
ck "ROM payload identical to flat .min"            cmp -s game.x.min game.min
ck "LINE table present and populated"              grep -qE "^LINE +[1-9]" dump
ck "line records map game.c"                       grep -q "game.c:" dump
ck "FUNC extents for main"                         grep -qE "^  main .*0x[0-9a-f]+\.\.0x[0-9a-f]+ +game.c" dump
ck "FUNC extents for bump"                         grep -qE "^  bump .*game.c" dump
ck "source text embedded"                          grep -q "game.c .*embedded" dump
ck "symbols include _main"                         grep -q "_main " dump
ck "container self-reports valid"                  grep -qx "ok" dump

# the embedded TEXT is the verbatim source (the canary string is inside the payload)
ck "embedded source is verbatim"                   grep -q "SOURCE_EMBED_CANARY" game.minx

[ "$fail" -eq 0 ] && echo "== MINX debug container GREEN ==" || { echo "== MINX debug container BROKEN =="; sed -n '1,40p' dump; }
exit "$fail"
