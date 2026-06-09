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
# The table below pins (asm line -> expected bytes).  Two contracts:
#  - WORST-CASE slot (target is the EXTERNAL symbol `_ext`, i.e. cross-area): this
#    is what the COMPILER sizes against — EVERY such expected value MUST equal what
#    `s1c88instructionSize` returns for that mnemonic.  The compiler cannot know a
#    target's area/distance at compile time, so it always sizes the worst case.
#  - RELAXED form (target is the LOCAL same-area label `_x`, TODO #14b): the
#    assembler drops `ld nb` and emits the minimal relative form (2/3 B).  These
#    lock the relaxation so a regression is caught; the compiler's worst-case size
#    is a safe UPPER BOUND over them.
# A case ending `:local` references `_x` (same-area); otherwise `_ext` (external).
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDAS="${REPO}/build/sdcc-4.5.0/bin/sdas88"
[ -x "$SDAS" ] || { echo "!! sdas88 not built — run scripts/build-sdas.sh as88" >&2; exit 2; }

OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

# "asm line@expected"  ('_ext' = external worst-case = compiler size; '_x' = same-area relaxed)
CASES=(
  # worst-case linker slots (cross-area) — must match peep.c s1c88instructionSize
  "bcall _ext@6"        # ld nb,#bank ; carl  (banked call, 6-byte slot)
  "bjump _ext@6"        # ld nb,#bank ; jrl   (banked jump, 6-byte slot)
  "bcall c, _ext@6"     # basic-cc banked call
  "bjump nz, _ext@6"    # basic-cc banked jump
  "bjump lt, _ext@9"    # short-only signed cc -> invert-and-skip (jrs inv,+7 ; ld nb ; jrl)
  "bcall ge, _ext@9"    # short-only signed cc banked call
  # relaxed same-area forms (TODO #14b) — drop ld nb, minimal relative form
  "bcall _x@2"          # same-area uncond call -> cars d8
  "bjump _x@2"          # same-area uncond jump -> jrs d8
  "bcall c, _x@2"       # same-area basic-cc call -> cars c, d8
  "bjump nz, _x@2"      # same-area basic-cc jump -> jrs nz, d8
  # fixed relative branches (size-invariant)
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
  # `_x` is defined locally (same area); `_ext` stays external (cross-area worst case).
  printf '\t.globl _x\n\t.globl _ext\n\t.area _T\n%s\n_x:\n' "$line" > "${OUT}/t.s"
  if ! "$SDAS" -o "${OUT}/t.rel" "${OUT}/t.s" 2>"${OUT}/e"; then
    echo "  FAIL  '$line' did not assemble:"; sed 's/^/        /' "${OUT}/e" | head -3; fail=1; continue
  fi
  # OUTPUT size = the assembler's location-counter advance over the instruction,
  # i.e. the address of the `_x:` label placed right after it.  We read that from
  # the symbol record `S _x Def<hex>` rather than counting T-line data bytes:
  # since #14c, a same-area relative branch carries an R_PCR R_BYT3 relocation
  # whose T-line FIELD is 3 bytes wide but collapses to ONE output byte at link
  # (the linker's byte-select), so the T-line byte count overstates the real,
  # ROM-resident size the compiler models.  The dot advance never lies.
  hex="$(grep -E '^S _x Def' "${OUT}/t.rel" | sed -E 's/.*Def0*([0-9A-Fa-f]*)$/\1/')"
  got="$(printf '%d' "0x${hex:-0}")"
  if [ "$got" = "$want" ]; then
    printf "  ok    %-16s %s bytes\n" "$line" "$got"
  else
    printf "  FAIL  %-16s assembler emits %s bytes, compiler assumes %s\n" "$line" "$got" "$want"
    fail=1
  fi
done

[ "$fail" -eq 0 ] && echo "== instruction-size contract GREEN ==" || echo "== instruction-size MISMATCH (sync peep.c s1c88instructionSize <-> the assembler) =="
exit "$fail"
