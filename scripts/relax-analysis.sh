#!/usr/bin/env bash
#
# relax-analysis.sh — branch-relaxation OPPORTUNITY analysis (TODO #14a, zero-risk).
#
# Today every compiler-emitted call/tail-jump is a fixed 6-byte banked slot
# (`ld nb,#bank ; carl|jrl`, 9 B for a signed-conditional #13 trampoline). The
# linker only ever NOPs unused bytes — it never shrinks the slot (see
# docs/s1c88/banked-branch.md). Relaxation (#14b/#14c) would drop the `ld nb`
# for same-bank targets and use the short `cars`/`jrs` (2 B) or `carl`/`jrl`
# (3 B) form when in range. THIS script measures the win up front, with no
# codegen change, by inspecting fully-linked real programs.
#
# How it works (authoritative, no binary-scan heuristics):
#   s1c88.lib carries ZERO bcall/bjump slots, so the only slot-bearing modules
#   are crt0 + the user's C code. We compile each program, assemble with -l
#   (listing) and link with -u (RELOCATED listing -> .rst). Each .rst slot line
#   shows the linker's FINAL resolved bytes, which fully self-describe the slot:
#       FF FF FF F2 dd dd   same-bank, ld nb NOP'd     (carl/F2, jrl/F3)
#       CE C4 00 F2 dd dd   bank-0 target, ld nb kept  (a MISSED relaxation today)
#       CE C4 NN F2 dd dd   cross-bank (NN!=0), ld nb required
#   so range/form/bank come straight from the bytes; no .map cross-reference.
#
# crt0's 27 reset/IRQ vector slots sit at HARDWARE-FIXED addresses (0x2102 /
# 0x2108+6k) and must never move (crt0.s forbids it) -> reported as "fixed",
# excluded from the shrinkable count.
#
#   scripts/relax-analysis.sh [prog.c ...]   # default: examples/hello + scripts/relax/*.c
#   RELAX_NO_BUILD=1 scripts/relax-analysis.sh    # reuse the last compiler build
#
# Report-only; always exits 0.
set -uo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SDCC="${REPO}/build/sdcc-4.5.0"
SDCCBIN="${SDCC}/src/sdcc"
SDAS="${SDCC}/bin/sdas88"
SDLD="${SDCC}/bin/sdldz80"
LIBDIR="${SDCC}/share/sdcc/lib/s1c88"
INCDIR="${SDCC}/share/sdcc/include/s1c88"
CRT0="${REPO}/device/lib/s1c88/crt0.s"
OUT="$(mktemp -d)"; trap 'rm -rf "$OUT"' EXIT

[ -x "$SDAS" ] && [ -x "$SDLD" ] || { echo "!! build sdas88 + sdldz80 first (scripts/build-sdas.sh as88; scripts/build-sdld.sh)" >&2; exit 2; }
[ -f "${LIBDIR}/s1c88.lib" ] || { echo "!! runtime not installed — run scripts/build-runtime.sh (or ./build.sh)" >&2; exit 2; }
[ -f "${SDCC}/config.status" ] || { echo ">> not configured — run ./build.sh first" >&2; exit 1; }

# --- overlay + build the compiler (same as corpus-check / size-check / dev.sh) ---
if [ -z "${RELAX_NO_BUILD:-}" ]; then
  echo ">> overlay + build sdcc"
  cp "${REPO}"/src/s1c88/*.c "${REPO}"/src/s1c88/*.h "${REPO}"/src/s1c88/*.cc \
     "${REPO}"/src/s1c88/*.i "${REPO}"/src/s1c88/peeph*.def \
     "${REPO}"/src/s1c88/Makefile.in "${SDCC}/src/s1c88/"
  if ! make -C "${SDCC}/src" > /tmp/sdcc88-build.log 2>&1; then
    echo "!! BUILD FAILED:" >&2; grep -iE "error:|Error [0-9]+" /tmp/sdcc88-build.log | head -20 >&2; exit 1
  fi
fi

# Programs to analyze: args, else examples/hello + scripts/relax/*.c
PROGS=("$@")
if [ "${#PROGS[@]}" -eq 0 ]; then
  PROGS=("${REPO}/examples/hello/hello.c")
  for f in "${REPO}"/scripts/relax/*.c; do [ -e "$f" ] && PROGS+=("$f"); done
fi

# Assemble crt0 once (with listing) — shared across all programs.
"$SDAS" -l -o "${OUT}/crt0.rel" "$CRT0" >/dev/null 2>&1 \
  || { echo "!! crt0 failed to assemble" >&2; exit 1; }

# --- the per-.rst slot analyzer -------------------------------------------------
# Reads relocated-listing lines, classifies each bcall/bjump slot from its FINAL
# bytes, and prints one TSV record per slot: module cat disp cur min saved target
analyze_rst() {  # $1 = module label, $2 = .rst path
gawk -v MOD="$1" '
  function h(x){ return strtonum("0x" x) }                 # hex byte -> int
  function s8(d){ return (d>=128) ? d-256 : d }            # signed int8 test helper
  # collect the per-line object bytes (tokens before the leading TAB)
  {
    line=$0
    ti=index(line, "\t")
    if (ti==0) next
    head=substr(line, 1, ti-1)                             # addr + bytes + linenum
    src =substr(line, ti+1)                                # the source text
    if (src !~ /(bcall|bjump)[ \t]/) next
    # target symbol
    if (match(src, /(bcall|bjump)[ \t]+[A-Za-z_.$][A-Za-z0-9_.$]*/)) {
      tok=substr(src, RSTART, RLENGTH); sub(/^(bcall|bjump)[ \t]+/, "", tok); tgt=tok
    } else { tgt="?" }
    n=split(head, t, /[ \t]+/)
    # t[1]=addr(6hex) ... t[n]=listing linenum ; t[2..n-1]=object bytes
    nb_b=0; delete B
    for (i=2;i<n;i++){ if (t[i] ~ /^[0-9A-Fa-f][0-9A-Fa-f]$/){ nb_b++; B[nb_b]=h(t[i]) } }
    if (nb_b < 4) next                                     # not a full slot line

    # --- classify from the resolved bytes ---
    b0=B[1]; b1=B[2]; b2=B[3]; b3=B[4]
    cond=0; tramp=0
    if (b0==0xFF) { bank="nop"; opi=4 }                    # ld nb NOPed (same bank)
    else if (b0==0xCE && b1==0xC4) { bank=(b2==0)?"zero":"bank"; opi=4 } # ld nb present
    else if (b0==0xCE && b1>=0xE0 && b1<=0xEF) { tramp=1; cond=1; bank="tramp"; opi=0 }
    else next
    op=B[opi]                                              # branch opcode

    # displacement: long branch carries disp16 at op+1,op+2 (little-endian)
    if (!tramp) {
      lo=B[opi+1]; hi=B[opi+2]; disp=lo+hi*256; if (disp>=32768) disp-=65536
      # branch form / call-vs-jump from op
      if (op==0xF2||op==0xF3) cond=0
      else if ((op>=0xE8&&op<=0xEB)||(op>=0xEC&&op<=0xEF)) cond=1
      cur=6
      fits8 = (disp>=-128 && disp<=127)
      samebank = (bank=="nop" || bank=="zero")
      if (samebank) min = fits8 ? 2 : 3                    # drop ld nb (3 B)
      else          min = 3 + (fits8 ? 2 : 3)             # keep ld nb
    } else {
      cur=9; min=9; disp=0; samebank=0                     # signed-cond trampoline (rare)
    }
    saved = cur - min
    cat = (tgt ~ /^(__start|_irq_v[0-9]+)$/) ? "fixed" : \
          (tramp ? "tramp" : (samebank ? (bank=="zero"?"same0":"samenop") : "cross"))
    printf "%s\t%s\t%d\t%d\t%d\t%d\t%s\n", MOD, cat, disp, cur, min, saved, tgt
  }
' "$2"
}

# --- run the pipeline for each program, collect all slot records ----------------
RECS="${OUT}/recs.tsv"; : > "$RECS"
declare -a LABELS
for src in "${PROGS[@]}"; do
  [ -f "$src" ] || { echo "  (skip missing $src)"; continue; }
  lbl="$(basename "$src" .c)"
  LABELS+=("$lbl")
  cc -E -P -I"$INCDIR" -x c "$src" > "${OUT}/${lbl}.pp.c" 2>/dev/null \
    || { echo "  $lbl: preprocess ERR"; continue; }
  "${SDCCBIN}" -ms1c88 --opt-code-size --c1mode -o "${OUT}/${lbl}.asm" < "${OUT}/${lbl}.pp.c" 2>/dev/null \
    || { echo "  $lbl: compile ERR"; continue; }
  cp "${OUT}/crt0.rel" "${OUT}/${lbl}.crt0.rel"
  # assemble both with listings into a per-program subdir so .rst names don't clash
  d="${OUT}/${lbl}.d"; mkdir -p "$d"
  "$SDAS" -l -o "${d}/crt0.rel" "$CRT0" >/dev/null 2>&1
  "$SDAS" -l -o "${d}/m.rel" "${OUT}/${lbl}.asm" >/dev/null 2>&1 \
    || { echo "  $lbl: assemble ERR"; continue; }
  ( cd "$d" && "$SDLD" -mjwxu -i out.ihx -b _CODE=0x21d0 -b _DATA=0x1000 \
      -k "$LIBDIR" -l s1c88 crt0.rel m.rel -e ) >/dev/null 2>&1 \
    || { echo "  $lbl: link ERR"; continue; }
  [ -f "${d}/m.rst" ] && analyze_rst "$lbl" "${d}/m.rst" >> "$RECS"
  # crt0 is shared infrastructure — analyze it once (its 27 vector slots are
  # identical every link; only the gsinit/main bcalls vary slightly by program).
  if [ -z "${CRT0_DONE:-}" ] && [ -f "${d}/crt0.rst" ]; then
    analyze_rst "crt0" "${d}/crt0.rst" >> "$RECS"; CRT0_DONE=1
  fi
done

# --- report ---------------------------------------------------------------------
echo
echo "=============================================================================="
echo " Branch-relaxation opportunity (TODO #14a)  —  per fully-linked program"
echo "=============================================================================="
printf "%-16s %5s %5s %6s %6s %6s %7s %7s\n" \
  "program" "slots" "fixed" "same↓2" "same↓3" "cross" "curB" "saveB"
printf -- "------------------------------------------------------------------------------\n"

# aggregate per module (crt0 collapsed per program suffix ":crt0")
awk -F'\t' '
  { mod=$1; cat=$2; cur=$4; min=$5; sv=$6
    isrt = (mod=="crt0")                                    # runtime vs user code
    slots[mod]++
    curB[mod]+=cur
    if (cat=="fixed"){ fixed[mod]++; agg(isrt,"fixed",cur,0,cat); next }
    saveB[mod]+=sv
    if (cat=="same0"||cat=="samenop"){
      if (min==2) s2[mod]++; else s3[mod]++
      agg(isrt,(min==2?"s2":"s3"),cur,sv,cat)
    } else if (cat=="cross"){ cross[mod]++; agg(isrt,"cross",cur,sv,cat) }
    else if (cat=="tramp"){ tramp[mod]++; agg(isrt,"tramp",cur,sv,cat) }
  }
  function agg(rt,kind,cur,sv,cat,   g){          # accumulate user(0)/crt0(1)/grand(2)
    for (g=(rt?1:0); ; g=2){
      G_slots[g]++; G_cur[g]+=cur; G_save[g]+=sv
      if (kind=="fixed") G_fixed[g]++
      else if (kind=="s2") G_s2[g]++
      else if (kind=="s3") G_s3[g]++
      else if (kind=="cross") G_cross[g]++
      else if (kind=="tramp") G_tramp[g]++
      if (cat=="same0") G_miss[g]++
      if (g==2) break
    }
  }
  function row(m,lbl){ printf "%-16s %5d %5d %6d %6d %6d %7d %7d\n",
      lbl, slots[m]+0, fixed[m]+0, s2[m]+0, s3[m]+0, cross[m]+0, curB[m]+0, saveB[m]+0 }
  END{
    nm=0; for (m in slots) if (m!="crt0") order[nm++]=m
    for (i=0;i<nm;i++) for (j=i+1;j<nm;j++) if (order[j]<order[i]){ t=order[i];order[i]=order[j];order[j]=t }
    for (i=0;i<nm;i++) row(order[i],order[i])      # user programs first
    printf "------------------------------------------------------------------------------\n"
    printf "%-16s %5d %5d %6d %6d %6d %7d %7d\n", "USER subtotal",
      G_slots[0]+0,G_fixed[0]+0,G_s2[0]+0,G_s3[0]+0,G_cross[0]+0,G_cur[0]+0,G_save[0]+0
    if ("crt0" in slots){ printf "\n"; row("crt0","crt0  (runtime)") }   # crt0 (shared, once)
    printf "==============================================================================\n"
    printf "%-16s %5d %5d %6d %6d %6d %7d %7d\n", "GRAND TOTAL",
      G_slots[2]+0,G_fixed[2]+0,G_s2[2]+0,G_s3[2]+0,G_cross[2]+0,G_cur[2]+0,G_save[2]+0

    up = (G_cur[0]>0) ? (100.0*G_save[0]/G_cur[0]) : 0
    printf "\n  USER-CODE opportunity (the #14b target — all calls intra-common-bank):\n"
    printf "    shrinkable slots          : %d of %d (none fixed)\n", G_slots[0]-G_fixed[0], G_slots[0]
    printf "    -> 2-byte cars/jrs        : %d   -> 3-byte carl/jrl : %d\n", G_s2[0]+0, G_s3[0]+0
    printf "    call bytes today / saved  : %d / %d  (%.1f%% smaller)\n", G_cur[0]+0, G_save[0]+0, up
    printf "\n  crt0 runtime: %d slots, %d are HARDWARE-FIXED vector table (never shrink);\n", G_slots[1]+0, G_fixed[1]+0
    printf "               only %d relaxable (gsinit/main), saving %d B.\n", G_slots[1]-G_fixed[1], G_save[1]+0
    if (G_cross[2]==0)
      printf "\n  (No cross-bank slots in the sample: every program is single common-bank,\n   the dominant real case. Cross-bank has a smaller win — branch-form only, ld\n   nb stays — and is #14c territory; it needs a multi-bank program to show.)\n"
    if (G_miss[2]>0)
      printf "\n  NOTE: %d bank-0 slot(s) still carry `ld nb,#0` (linker did NOT NOP it —\n        an existing inconsistency; relaxation removes the field entirely).\n", G_miss[2]
  }
' "$RECS"

cat <<'EOF'

  Method: numbers come from each program's RELOCATED listing (.rst) — the
  linker's FINAL resolved slot bytes, so same-bank vs cross-bank is exact.
  Dropping `ld nb` (3 B) applies to every same-bank slot, so the headline
  byte-savings is firm; only the 2-vs-3-byte split (does the disp fit int8?)
  is approximate, judged on the current long-form disp ±a few bytes of reflow.
  A borderline disp could flip 2<->3, moving the total by at most ~1 B/slot.

------------------------------------------------------------------------------
 Feasibility gate (#14b precondition): does the sdas multi-pass loop converge?
------------------------------------------------------------------------------
 - sdas runs a FIXED 3-pass sequencer (asxxsrc/asmain.c: `for(pass=0;pass<3)`),
   NOT an iterate-to-fixpoint loop. The shared `fuzz` / per-area `a_fuzz`
   pass-to-pass drift trackers are present.
 - PRECEDENT: the STM8 and F8 backends relax (short/long) inside this same
   3-pass loop via a per-target `setbit`/`getbit` bit table + the `fuzz`
   correction. It CONVERGES because the scheme is monotonic: pass 0 sizes
   everything LONG (upper bound), pass 1 shrinks only what fits and records the
   choice, pass 2 replays it. Shrinking only pulls targets closer, so an
   in-range branch can never fall out of range -> safe, no extra passes needed.
 - CAVEAT for #14b: STM8/F8 `ls_mode` bails on ALL relocatable operands
   (`e_base.e_ap != 0` -> forced long). Same-module relaxation must instead
   relax the SAME-AREA relocatable case, where the displacement is known each
   pass as `e_addr - dot.s_addr`. The 3-pass+fuzz machinery supports this; the
   ~30-line bit table is per-target (copy from asstm8/asf8 into s1c88mch.c).
 VERDICT: feasible within the existing 3-pass loop — no asmain.c change; the
 work is a same-area-aware `ls_mode` + bit table in the s1c88 assembler backend.
EOF
exit 0
