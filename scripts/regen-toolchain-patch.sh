#!/usr/bin/env bash
#
# regen-toolchain-patch.sh — regenerate third_party/sdcc/s1c88_banked_branch.patch
# from the (edited) files in build/sdcc-4.5.0 against the PRISTINE upstream tarball.
#
# The banked-branch toolchain changes live as edits to upstream's sdas/asxxsrc +
# sdas/linksrc sources inside the build tree; the patch is what build-sdas.sh /
# build-sdld.sh apply onto a fresh extraction (CI, `build.sh --fresh`). Never
# hand-edit hunk headers — edit the files under build/, rebuild, test, then run
# this to re-derive the patch.  The pristine side is extracted with Python's
# tarfile (no bzip2 dependency) into a scratch dir, so build/ is never touched.
#
#   scripts/regen-toolchain-patch.sh            # rewrite the patch in place
#   scripts/regen-toolchain-patch.sh --check    # exit 1 if the patch is stale
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
TARBALL="${REPO}/build/sdcc-src-4.5.0.tar.bz2"
PATCH="${REPO}/third_party/sdcc/s1c88_banked_branch.patch"
FILES="sdas/asxxsrc/asout.c sdas/asxxsrc/sdas.c sdas/asxxsrc/sdas.h
       sdas/linksrc/lklibr.c sdas/linksrc/lklist.c sdas/linksrc/lkmain.c sdas/linksrc/lkrloc3.c"

[ -f "$TARBALL" ] || { echo "!! no tarball at $TARBALL — run ./build.sh first" >&2; exit 2; }
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/a" "$tmp/b"
python3 - "$TARBALL" "$tmp/a" $FILES <<'PY'
import sys, tarfile
tar, out, files = sys.argv[1], sys.argv[2], sys.argv[3:]
with tarfile.open(tar) as t:
    members = {m.name.split('/', 1)[1]: m for m in t.getmembers() if '/' in m.name}
    for f in files:
        m = members[f]
        m.name = f
        t.extract(m, out, filter='data')
PY
for f in $FILES; do mkdir -p "$tmp/b/$(dirname "$f")"; cp "${SDCC}/$f" "$tmp/b/$f"; done
# git diff --no-index yields `diff --git a/<f> b/<f>` + `--- a/` / `+++ b/` headers (patch -p1 compatible)
( cd "$tmp" && git -c core.autocrlf=false diff --no-index --src-prefix=a/ --dst-prefix=b/ a b ) \
  | sed -E 's#^diff --git a/a/(.*) b/b/(.*)$#diff --git a/\1 b/\2#; s#^--- a/a/#--- a/#; s#^\+\+\+ b/b/#+++ b/#' > "$tmp/new.patch" || true
# git diff --no-index prefixes paths with the scratch dir names a/ and b/ — the sed above
# collapses "a/a/<f>" to "a/<f>" and "b/b/<f>" to "b/<f>" in the three header lines.
if [ "${1:-}" = "--check" ]; then
  if cmp -s "$tmp/new.patch" "$PATCH"; then echo ">> patch is current"; exit 0
  else echo "!! patch is STALE vs build/ sources:"; diff "$PATCH" "$tmp/new.patch" | head -20; exit 1; fi
fi
cp "$tmp/new.patch" "$PATCH"
echo ">> rewrote $PATCH ($(grep -c '^diff --git' "$PATCH") files, $(wc -l < "$PATCH" | tr -d ' ') lines)"
