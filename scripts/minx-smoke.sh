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
struct pair { int a; unsigned char b; };

static unsigned char counter;
struct pair g_pair = {7, 9};

static unsigned char bump(struct pair *p)
{
    counter += p->b;
    return counter;
}

int main(void)
{
    volatile int arr[2];
    unsigned char i;
    arr[0] = g_pair.a;
    for (i = 0; i < 3; i++)
        bump(&g_pair);
    return counter + (unsigned char)arr[0];
}
EOF

cd "$tmp"
"$SDCCBIN" -ms1c88 --debug game.c -o game.ihx >/dev/null 2>&1 || { echo "FAIL: compile+link"; exit 1; }
"${SDCC}/bin/romgen" game.ihx game.min  >/dev/null || { echo "FAIL: romgen .min"; exit 1; }
"${SDCC}/bin/romgen" game.ihx game.minx >/dev/null || { echo "FAIL: romgen .minx"; exit 1; }

"${SDCC}/bin/minxdump" --rom=game.x.min game.minx > dump || { echo "FAIL: minxdump validation"; cat dump; exit 1; }

fail=0
FAILED=""
ck() { # ck <label> <command...>
  local label="$1"; shift
  if "$@" >/dev/null 2>&1; then echo "  ok  $label"; else echo "  FAIL $label"; FAILED="${FAILED}[$label] "; fail=1; fi
}
ck "ROM reconstructed identical to flat .min"      cmp -s game.x.min game.min
ck "ROM is sparse SEG runs"                        grep -qE "^  SEG 0x0021" dump
ck "LINE table present and populated"              grep -qE "^LINE +[1-9]" dump
ck "line records map game.c"                       grep -q "game.c:" dump
ck "FUNC extents for main"                         grep -qE "^  main .*0x[0-9a-f]+\.\.0x[0-9a-f]+ +game.c" dump
ck "FUNC bump is static with extents"              grep -qE "^  bump .*game.c.*static" dump
ck "STRU layout for struct pair"                   grep -qE "struct pair +size 3, 2 members" dump
ck "struct member b at offset 2"                   grep -qE "^    \+2 +b +unsigned char" dump
ck "TYPE graph renders struct pair \*"             grep -qE "struct pair \*" dump
ck "VAR g_pair static address"                     grep -qE "^  g_pair .*struct pair .*@0x" dump
ck "VAR arr is IX-relative stack"                  grep -qE "^  arr .*int\[2\] .*ix[+-]" dump
ck "VAR counter is file-static"                    grep -qE "^  counter .*<file-static>.*@0x" dump
ck "FUNC main establishes an IX frame"             grep -qE "^  main .*frame-ptr" dump
ck "IO register map names the SFR space"           grep -qE "^  PRC_MODE .*0x002080 size 1" dump
ck "rom-crc32 build identity in NOTE"              grep -qE "rom-crc32 = 0x[0-9a-f]{8}" dump
ck "source text embedded"                          grep -q "game.c .*embedded" dump
ck "symbols include _main"                         grep -q "_main " dump
ck "container self-reports valid"                  grep -qx "ok" dump

# the embedded TEXT is the verbatim source (the canary string is inside the payload)
ck "embedded source is verbatim"                   grep -q "SOURCE_EMBED_CANARY" game.minx

# failed labels go LAST: run-tests' TAP diag keeps only the tail of this output
[ "$fail" -eq 0 ] && echo "== MINX debug container GREEN ==" || { sed -n '1,40p' dump; echo "== MINX debug container BROKEN =="; echo "-- failed: ${FAILED}"; }
exit "$fail"
