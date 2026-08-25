#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery G: MECHANISM CHARACTERISATION.
#
# The phase brief: "Every exploiter that clears its responder class's detection
# floor gets characterised, not just recorded: what mechanism it attacks, what
# v0.6 does wrong against it, whether it is distinct or a known weakness found
# again."  A win rate cannot answer any of those three.  This battery answers
# them by pointing the per-decision channel at the TARGET arm while the exploiter
# plays it, and reading what changed relative to the mirror.
#
# The decisive targeting fact that makes this cheap: `V06Agent` overrides only
# `reset`, `resetWithKnowledge`, `observe` and `chooseAsk` (engine/src/v06.hpp).
# It overrides NO part of the declaration path -- `proposeDeclaration`,
# `declareNow`, `evaluateSet`, `feasibleAllocation`, `willingForced` and
# `bestGuess` are v0.5's, inherited unchanged -- so every configuration of the
# frontier, from the 364 games/s deployed policy to the 1.7 games/s search, runs
# byte-for-byte the same declaration rule.  An attack on that rule is an attack
# on the whole frontier at once, and this battery measures it on both ends to
# check that the invariance holds in play as well as in the source.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-14}
DEALS=${DEALS:-4000}
DEALS_SLOW=${DEALS_SLOW:-600}
BANK=${BANK:-7050001}
ART=${ART:-$OUT/P5-mech.jsonl}
SPECS=${SPECS:-$OUT/p5-arms.tsv}
mkdir -p "$OUT"
$BIN seeds --require=$BANK >/dev/null || { echo "bank $BANK unusable"; exit 3; }

FFAST="v06"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"

cell () { # cell <adversary> <target> <deals> <tag>
  $BIN v7decide --a="$1" --b="$2" --games=$3 --rotations=2 --seed=$BANK --capture=b \
       --threads=$THREADS --json 2>/dev/null \
    | sed "s|^{|{\"battery\":\"P5mech\",\"tag\":\"$4\",\"bank\":$BANK,|" >> "$ART"
  echo "  done $4 :: ${1:0:40} vs ${2:0:26}"; }

echo "== P5 mechanism characterisation, target arm captured, bank $BANK =="
# The control the whole battery is read against.
cell "$FFAST" "$FFAST" $DEALS mirror-Ffast
cell "$FFAST" "$FCHEAP" $DEALS_SLOW mirror-Fcheap
while IFS=$'\t' read -r ID CL SPEC; do
  case "$ID" in ''|\#*) continue;; esac
  cell "$SPEC" "$FFAST"  $DEALS      "$ID-vs-Ffast"
  cell "$SPEC" "$FCHEAP" $DEALS_SLOW "$ID-vs-Fcheap"
done < "$SPECS"
echo "done -> $ART"
