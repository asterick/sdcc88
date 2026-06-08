#!/usr/bin/env bash
#
# insn-size-check.sh — verify the ASSEMBLER emits exactly the byte size the
# COMPILER assumes for each instruction.
#
# The compiler decides branch ranges (labelInRange -> jrs vs jrl, etc.) using
# src/s1c88/peep.c `s1c88instructionSize`.  If the assembler's actual encoding is
# a different size, the layout the compiler reasoned about is wrong: a branch it
# thought was in range isn't (assembler "Branching Range Exceeded"), or it picks a
# short form that silently overflows.  This drift is invisible to corpus-check
# (which compares .asm TEXT) — it bit us once: bjump/bcall were sized 6 by the
# compiler but the assembler emitted 7 (an extra pad byte), so the assembler and
# linker disagreed on the slot format.
#
# The table below pins (asm line -> expected bytes).  EVERY expected value MUST
# equal what `s1c88instructionSize` returns for that mnemonic — keep them in sync.
# A relocatable operand is used where the form differs by target distance, so the
# assembler emits its fixed worst-case slot (what the compiler sizes against).
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

# "asm line@expected"  (peep.c s1c88instructionSize must agree)
CASES=(
  "bcall _x@6"          # ld nb,#bank ; carl  (banked call, 6-byte slot)
  "bjump _x@6"          # ld nb,#bank ; jrl   (banked jump, 6-byte slot)
  "bcall c, _x@6"       # basic-cc banked call
  "bjump nz, _x@6"      # basic-cc banked jump
  "bjump lt, _x@9"      # short-only signed cc -> invert-and-skip (jrs inv,+7 ; ld nb ; jrl)
  "bcall ge, _x@9"      # short-only signed cc banked call
  "jrl _x@3"            # 16-bit relative jump
  "jrs _x@2"            # 8-bit relative jump
  "carl _x@3"
  "cars _x@2"
  "jp lt, _x@6"         # short-only signed cc -> invert-and-skip (jrs inv,+4 ; jrl)
  "ld nb, #0@3"         # CE C4 bb
)

fail=0
for c in "${CASES[@]}"; do
  line="${c%@*}"; want="${c##*@}"
  printf '\t.globl _x\n\t.area _T\n%s\n_x:\n' "$line" > "${OUT}/t.s"
  if ! "$SDAS" -o "${OUT}/t.rel" "${OUT}/t.s" 2>"${OUT}/e"; then
    echo "  FAIL  '$line' did not assemble:"; sed 's/^/        /' "${OUT}/e" | head -3; fail=1; continue
  fi
  # first T line: "T a0 a1 a2 <data...>"  (XL3 = 3 address bytes); data = NF-1-3
  got="$(awk '/^T/{print NF-4; exit}' "${OUT}/t.rel")"
  if [ "$got" = "$want" ]; then
    printf "  ok    %-16s %s bytes\n" "$line" "$got"
  else
    printf "  FAIL  %-16s assembler emits %s bytes, compiler assumes %s\n" "$line" "$got" "$want"
    fail=1
  fi
done

[ "$fail" -eq 0 ] && echo "== instruction-size contract GREEN ==" || echo "== instruction-size MISMATCH (sync peep.c s1c88instructionSize <-> the assembler) =="
exit "$fail"
