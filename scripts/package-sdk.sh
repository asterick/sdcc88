#!/usr/bin/env bash
#
# package-sdk.sh — stage the built SDK as a relocatable tree and tar it up.
#
# Produces build/dist/sdcc88-sdk/ (the staged tree) and
# build/dist/sdcc88-sdk-<version>-<os>-<arch>.tar.gz for the HOST platform
# (linux-x64, darwin-arm64, ...). The layout is the standard
# SDCC install shape, which the driver resolves relative to its own binary
# (path(argv0)/../share/sdcc/{include,lib} for headers+runtime, path(argv0)
# first when spawning sdcpp/sdas88/sdldz80), so the unpacked tree works from
# any directory with no environment setup:
#
#   bin/        sdcc sdcpp sdas88 sdldz80 romgen minxdump   (real binaries, stripped —
#               the build tree's bin/sdcc + bin/sdcpp are configure-generated
#               wrappers with the absolute build path baked in, so we copy the
#               real driver from src/ and the real cpp from support/cpp/gcc/)
#   libexec/sdcc/<triple>/<gcc-ver>/cc1
#               sdcpp is a GCC driver: it execs a separate cc1 backend, located
#               by relocating its configured libexecsubdir against argv[0]
#               (verified: cc1 placed flat next to sdcpp is NOT found). The
#               triple + version are read from the built support/cpp tree.
#   share/sdcc/ include/ (libc + <pm.h>) and lib/s1c88/ (crt0.rel + s1c88.lib
#               + member .rel objects — the .lib is a text index, the members
#               must travel with it)
#   docs/       building-roms.md (the end-user guide) + minx-format.md (the debug container)
#   examples/   hello/ (sources only — a copy-me starter project)
#   LICENSE     SDCC's COPYING (GPL-2.0-or-later; the binaries are SDCC-derived)
#
# Before tarring, the staged tree is PROVEN relocatable: it is copied to a
# temp dir and a test program is compiled + linked + romgen'd there under a
# minimal environment (env -i). If the bundled emulator runner has been built
# (tests/emu), the ROM is also executed and its exit code checked.
#
#   scripts/package-sdk.sh             # version from `git describe`
#   SDK_VERSION=v1.0 scripts/package-sdk.sh
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"

# --- prereqs: the full SDK (lazy component builds are fast no-ops once built) ---
[ -x "${SDCC}/src/sdcc" ] || { echo "!! sdcc not built — run ./build.sh" >&2; exit 2; }
"${REPO}/scripts/build-sdcpp.sh"   >/dev/null
"${REPO}/scripts/build-sdas.sh" as88 >/dev/null
"${REPO}/scripts/build-sdld.sh"    >/dev/null
"${REPO}/scripts/build-romgen.sh"  >/dev/null
"${REPO}/scripts/build-runtime.sh" >/dev/null

VERSION="${SDK_VERSION:-$(git -C "$REPO" describe --tags --always --dirty 2>/dev/null || echo unknown)}"
case "$(uname -s)" in
  Darwin)              HOST_OS=darwin ;;
  Linux)               HOST_OS=linux ;;
  MINGW*|MSYS*|CYGWIN*) HOST_OS=windows ;;
  *)      HOST_OS="$(uname -s | tr '[:upper:]' '[:lower:]')" ;;
esac
case "$(uname -m)" in
  x86_64)        HOST_ARCH=x64 ;;
  arm64|aarch64) HOST_ARCH=arm64 ;;
  *)             HOST_ARCH="$(uname -m)" ;;
esac
PLATFORM="${HOST_OS}-${HOST_ARCH}"
DIST="${REPO}/build/dist"
STAGE="${DIST}/sdcc88-sdk"
# Windows binaries carry .exe, and the archive is a .zip (native to the platform).
EXE=""; ARCHIVE_EXT=tar.gz
if [ "$HOST_OS" = windows ]; then EXE=".exe"; ARCHIVE_EXT=zip; fi
TARBALL="${DIST}/sdcc88-sdk-${VERSION}-${PLATFORM}.${ARCHIVE_EXT}"

echo ">> staging ${STAGE}"
rm -rf "$STAGE"
mkdir -p "${STAGE}/bin" "${STAGE}/docs" "${STAGE}/examples/hello"

cp "${SDCC}/src/sdcc${EXE}"             "${STAGE}/bin/sdcc${EXE}"
cp "${SDCC}/support/cpp/gcc/cpp${EXE}"  "${STAGE}/bin/sdcpp${EXE}"
cp "${SDCC}/bin/sdas88${EXE}"           "${STAGE}/bin/sdas88${EXE}"
cp "${SDCC}/bin/sdldz80${EXE}"          "${STAGE}/bin/sdldz80${EXE}"
cp "${SDCC}/bin/romgen${EXE}"           "${STAGE}/bin/romgen${EXE}"
cp "${SDCC}/bin/minxdump${EXE}"         "${STAGE}/bin/minxdump${EXE}"

# sdcpp's cc1 backend, at the libexecsubdir shape the driver relocates to
TRIPLE="$(sed -n 's/^target_noncanonical:=//p' "${SDCC}/support/cpp/gcc/Makefile")"
CPPVER="$(cat "${SDCC}/support/cpp/gcc/BASE-VER")"
[ -n "$TRIPLE" ] && [ -n "$CPPVER" ] || { echo "!! can't derive sdcpp libexec dir" >&2; exit 1; }
LIBEXEC="${STAGE}/libexec/sdcc/${TRIPLE}/${CPPVER}"
mkdir -p "$LIBEXEC"
cp "${SDCC}/support/cpp/gcc/cc1${EXE}" "${LIBEXEC}/cc1${EXE}"

strip "${STAGE}/bin/"* "${LIBEXEC}/cc1${EXE}" 2>/dev/null \
  || echo "   (strip unavailable — shipping unstripped)"
# macOS: stripping invalidates the linker's ad-hoc code signature; arm64 refuses
# to exec unsigned binaries, so re-sign ad-hoc (the relocation smoke below would
# catch a miss, but only with a cryptic "Killed: 9").
if [ "$HOST_OS" = darwin ]; then
  codesign -f -s - "${STAGE}/bin/"* "${LIBEXEC}/cc1"
fi

cp -r "${SDCC}/share" "${STAGE}/share"

cp "${REPO}/docs/s1c88/building-roms.md" "${STAGE}/docs/"
cp "${REPO}/docs/s1c88/minx-format.md"   "${STAGE}/docs/"
cp "${SDCC}/COPYING" "${STAGE}/LICENSE"
cp "${REPO}/examples/hello/hello.c" "${REPO}/examples/hello/Makefile" \
   "${REPO}/examples/hello/README.md" "${STAGE}/examples/hello/"

cat > "${STAGE}/README.md" <<EOF
# sdcc88 SDK ${VERSION} (${PLATFORM})

A C toolchain for the **Pokémon Mini** (Epson S1C88) — SDCC 4.5.0 retargeted to
the S1C88 core. Built from https://github.com/asterick/sdcc88.

The tree is relocatable — unpack it anywhere, no environment setup needed:

\`\`\`bash
bin/sdcc -ms1c88 game.c -o game.ihx   # preprocess + compile + link (crt0 + s1c88.lib)
bin/romgen game.ihx game.min          # pack into a flat .min ROM
bin/romgen game.ihx game.minx         # ...or the MINX debug container (ROM+symbols+lines+sources)
\`\`\`

- \`docs/building-roms.md\` — the end-user guide (memory map, \`__far\` banking,
  interrupts, \`<pm.h>\`).
- \`examples/hello\` — a copy-me starter project (\`make SDK=/path/to/this/dir\`).

Binaries are ${PLATFORM}, dynamically linked against the system C/C++ runtime.
SDCC is GPL-2.0-or-later — see \`LICENSE\`.
EOF

# --- prove the staged tree is relocatable before shipping it ---
echo ">> relocation smoke (temp-dir copy, minimal env)"
# Windows: sdcc spawns sdcpp via _popen() = cmd.exe, which needs COMSPEC +
# SystemRoot even under a minimal environment.
WINENV=()
if [ "$HOST_OS" = windows ]; then
  WINENV=("COMSPEC=${COMSPEC:-}" "SYSTEMROOT=${SYSTEMROOT:-${SystemRoot:-}}")
fi
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cp -r "$STAGE" "${TMP}/sdk"
cat > "${TMP}/prog.c" <<'EOF'
/* exercises sdcpp (#include), the support lib (div), and an initialized global */
#include <string.h>
volatile int a = 1000, b = 7;
char buf[8];
int main(void){
  strcpy(buf, "*");
  return (a / b) - 100 - (int)strlen(buf);   /* 142 - 100 - 1 == 41 */
}
EOF
env -i PATH=/usr/bin:/bin HOME="$TMP" ${WINENV[@]+"${WINENV[@]}"} \
  "${TMP}/sdk/bin/sdcc${EXE}" -ms1c88 "${TMP}/prog.c" -o "${TMP}/prog.ihx" \
  || { echo "!! relocated sdcc FAILED — package is not self-contained" >&2; exit 1; }
env -i PATH=/usr/bin:/bin ${WINENV[@]+"${WINENV[@]}"} \
  "${TMP}/sdk/bin/romgen${EXE}" "${TMP}/prog.ihx" "${TMP}/prog.min" >/dev/null \
  || { echo "!! relocated romgen FAILED" >&2; exit 1; }
[ -s "${TMP}/prog.min" ] || { echo "!! empty .min from relocated toolchain" >&2; exit 1; }
# the shipped starter project must build out of the box from inside the package
env -i PATH=/usr/bin:/bin HOME="$TMP" ${WINENV[@]+"${WINENV[@]}"} \
  make -s -C "${TMP}/sdk/examples/hello" hello.min \
  || { echo "!! packaged examples/hello FAILED to build" >&2; exit 1; }
RUNNER="${REPO}/build/emu/runner"
if [ -x "$RUNNER" ]; then
  rc=0; "$RUNNER" "${TMP}/prog.min" >/dev/null || rc=$?
  [ "$rc" -eq 41 ] || { echo "!! relocated ROM ran wrong: exit ${rc} != 41" >&2; exit 1; }
  echo "   relocated build runs on the emulator (exit 41) — OK"
else
  echo "   (emulator runner not built — compile/link/romgen verified, execution skipped)"
fi

echo ">> archiving ${TARBALL}"
rm -f "${DIST}"/sdcc88-sdk-*-"${PLATFORM}.${ARCHIVE_EXT}"
if [ "$ARCHIVE_EXT" = zip ]; then
  ( cd "$DIST" && zip -qr "$TARBALL" sdcc88-sdk )
else
  tar -czf "$TARBALL" -C "$DIST" sdcc88-sdk
fi
ls -lh "$TARBALL" | awk '{print "   " $5 "  " $9}'
echo ">> package-sdk OK"
