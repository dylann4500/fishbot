#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery I: DOES IT BEAT THE FRONTIER?
#
# The incumbent is a FRONTIER, not a point (SUBOPTIMALITY-LEDGER.md 0.1), and
# phase 0 fixed the rule that a v0.7 claim must dominate it rather than beat its
# cheap end.  The same rule applies to an adversary: an exploiter that beats the
# 364 games/s deployed policy and loses to the 6.4 games/s search has not
# exploited the frontier, it has exploited the operating point the frontier
# occupies when it is cheap.  So the severity that leads is the WORST cell over
# the frontier, and this battery measures the cells that decide it.
#
# The targeting fact that makes the question live: `V06Agent` overrides only
# `reset`, `resetWithKnowledge`, `observe` and `chooseAsk`.  It overrides NO part
# of the declaration path -- `proposeDeclaration`, `declareNow`, `evaluateSet`,
# `feasibleAllocation`, `willingForced`, `bestGuess` are all v0.5's, inherited
# unchanged -- so every configuration of the frontier runs byte-for-byte the same
# declaration rule.  An attack that works through the declaration path should
# transfer across the whole frontier; an attack that works through ask quality
# should not.  That is a falsifiable prediction and this battery tests it.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-14}
BANKS=${BANKS:-"7030001 7030002"}
ART=${ART:-$OUT/P9-frontier.jsonl}
SPECS=${SPECS:-$OUT/p9-arms.tsv}
D_FMID=${D_FMID:-1500}
D_FSEARCH=${D_FSEARCH:-600}
D_FCHEAP=${D_FCHEAP:-6000}
mkdir -p "$OUT"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
FMID="v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26"
FSEARCH="v06:s1=1,det=12,cand=4,kappa=2.5"
for B in $BANKS; do $BIN seeds --require="$B" >/dev/null || exit 3; done

cell () { # cell <id> <cluster> <spec> <targetTag> <target> <deals> <banks>
  for B in $7; do
    $BIN match --a="$3" --b="$5" --games=$6 --rotations=2 --seed=$B --threads=$THREADS --json 2>/dev/null \
      | sed "s|^{|{\"battery\":\"P9frontier\",\"id\":\"$1\",\"cluster\":\"$2\",\"targetTag\":\"$4\",\"bank\":$B,|" >> "$ART"
  done
  echo "  done $1 vs $4"; }

while IFS=$'\t' read -r ID CL SPEC; do
  case "$ID" in ''|\#*) continue;; esac
  cell "$ID" "$CL" "$SPEC" Fcheap  "$FCHEAP"  $D_FCHEAP  "$BANKS"
  cell "$ID" "$CL" "$SPEC" Fmid    "$FMID"    $D_FMID    "$BANKS"
done < "$SPECS"
# F-search is affordable for one bank and the two strongest arms only; every row
# prints its own n so the resolution travels with the number.
head -2 "$SPECS" | while IFS=$'\t' read -r ID CL SPEC; do
  case "$ID" in ''|\#*) continue;; esac
  cell "$ID" "$CL" "$SPEC" Fsearch "$FSEARCH" $D_FSEARCH "$(echo $BANKS | cut -d' ' -f1)"
done
echo "done -> $ART"
