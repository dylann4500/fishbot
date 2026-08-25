#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery C: EVALUATION of adversaries on fresh banks.
#
# Reads a TSV of  id <TAB> cluster <TAB> spec  and plays each against the named
# frontier points on the phase-2 training banks.  Fitting banks (704xxxx) never
# appear here; evaluation banks are the registered train banks (703xxxx).
#
# Sample sizes are chosen so that the half-width is small against the quantity
# being claimed rather than against the compute available.  98/sqrt(N):
#   Ffast   24,000 games/bank on THREE banks -> +/-0.37 pts pooled (~1 min a cell)
#   Fcheap  12,000 games/bank -> +/-0.89 pts   ( 96 games/s -> ~2 min a cell)
#   Fmid     6,000 games/bank -> +/-1.27 pts   (6.4 games/s -> ~16 min a cell)
#   Fsearch  2,000 games/bank -> +/-2.19 pts   (1.7 games/s -> ~20 min a cell)
# Phase 1 measured the detection floor at 12,000 games a bank and showed it is
# EVALUATION-power-limited below ~1.7 points, scaling as (games)^-1/2.  Buying
# the Ffast column to 48,000 games a bank is that trade taken deliberately: it
# is the difference between "no exploiter clears the floor" being a statement
# about the adversaries and being a statement about the sample size.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7}
THREADS=${THREADS:-14}
ART=${ART:-$OUT/P3-eval.jsonl}
SPECS=${SPECS:-../research/v07/results/p2-specs.tsv}
TARGETS=${TARGETS:-"Ffast"}
BANKS=${BANKS:-"7030001 7030002 7030003"}
PARTNERS=${PARTNERS:-""}
KTAG=${KTAG:-"k3"}
CORR=${CORR:-0}
mkdir -p "$OUT"

FFAST="v06"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
FMID="v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26"
FSEARCH="v06:s1=1,det=12,cand=4,kappa=2.5"

specOf () { case "$1" in
    Ffast) echo "$FFAST";; Fcheap) echo "$FCHEAP";; Fmid) echo "$FMID";; Fsearch) echo "$FSEARCH";; *) echo "$1";; esac; }
dealsOf () { case "$1" in
    Ffast) echo "${DEALS_FFAST:-12000}";; Fcheap) echo "${DEALS_FCHEAP:-6000}";;
    Fmid) echo "${DEALS_FMID:-3000}";; Fsearch) echo "${DEALS_FSEARCH:-1000}";; *) echo "${DEALS_OTHER:-6000}";; esac; }

for B in $BANKS; do $BIN seeds --require="$B" >/dev/null || { echo "bank $B unusable"; exit 3; }; done

grep -v '^#' "$SPECS" | grep -v '^[[:space:]]*$' | while IFS=$'\t' read -r ID CLUSTER SPEC; do
  [ -z "${ID:-}" ] && continue
  for T in $TARGETS; do
    TS=$(specOf "$T"); D=$(dealsOf "$T")
    for B in $BANKS; do
      EXTRA=""
      [ -n "$PARTNERS" ] && EXTRA="$EXTRA --partners=$PARTNERS"
      [ "$CORR" = "1" ] && EXTRA="$EXTRA --correlated"
      $BIN match --a="$SPEC" --b="$TS" --games=$D --rotations=2 --seed=$B \
           --threads=$THREADS $EXTRA --json 2>/dev/null \
        | sed "s|^{|{\"battery\":\"P3eval\",\"id\":\"$ID\",\"cluster\":\"$CLUSTER\",\"targetTag\":\"$T\",\"bank\":$B,\"k\":\"$KTAG\",\"corr\":$CORR,|" \
        >> "$ART"
      echo "  $ID vs $T bank $B done"
    done
  done
done
echo "done -> $ART"
