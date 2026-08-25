#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery H: the INFORMATION-DENIAL dose response.
#
# Phase-2 reconnaissance measured a single, unfitted coordinate of the extended
# responder class -- `oppCertDonate`, the twelfth of the eighteen v0.7 responder
# terms -- at +1.2 to +3.6 points against `v06` on scratch banks, with a clean
# inverted-U dose response.  Those banks are not in the reserved-seed registry
# and the spread across them was large, so nothing about that measurement is
# admissible until it is repeated here: registered evaluation banks, three of
# them, at 24,000 games each, with the full KPI profile of both arms.
#
# The coordinate prices the negative certificate a MISS publishes.  When the
# adversary asks seat q of the target team for card c and misses, the whole table
# learns "q does not hold c" -- and the only seats for whom that is news are q's
# two TEAMMATES, who are trying to work out which of them holds it.  `enumerateAsks`
# forbids asking a teammate (fish.hpp:188-190), so a team can never manufacture
# such a certificate about itself: every one of them is a gift from an opponent.
# 88.1% of v0.6's wrong declarations are exactly that error -- the team held all
# six cards and named the wrong teammate.  No feature anywhere in the v0.4-v0.6
# lineage prices this: f[9], f[16] and f[19] all price what the ACTOR reveals
# about its OWN hand.
#
# The three arms that separate denial from strength, all in MatchStats already:
#   lockHoldB     events the target holds a provably-locked half-suit before cashing
#   declPerGameB  declarations the target gets to make at all
#   declAccA      the ADVERSARY's own declaration accuracy -- which must FALL if
#                 the mechanism is denial, because a denial adversary is a worse
#                 player that makes the target worse faster
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-14}
DEALS=${DEALS:-12000}
BANKS=${BANKS:-"7030001 7030002 7030003"}
ART=${ART:-$OUT/P8-denial.jsonl}
mkdir -p "$OUT"
for B in $BANKS; do $BIN seeds --require="$B" >/dev/null || { echo "bank $B unusable"; exit 3; }; done

FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
FMID="v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26"

cell () { # cell <tag> <adversary> <target> <deals>
  for B in $BANKS; do
    $BIN match --a="$2" --b="$3" --games=$4 --rotations=2 --seed=$B --threads=$THREADS --json 2>/dev/null \
      | sed "s|^{|{\"battery\":\"P8denial\",\"tag\":\"$1\",\"bank\":$B,|" >> "$ART"
  done
  echo "  done $1"; }

echo "== P8 denial dose response vs v06 =="
cell control        "v06"           "v06" $DEALS
for R in 5 10 15 20 25 30 40; do cell "r12=$R" "v07:r12=$R" "v06" $DEALS; done
# The other three denial coordinates, at the dose the first one peaks at, so the
# group is separated rather than attributed to whichever member was tried first.
cell "r13=25"  "v07:r13=25"  "v06" $DEALS
cell "r14=25"  "v07:r14=25"  "v06" $DEALS
cell "r15=25"  "v07:r15=25"  "v06" $DEALS
cell "r16=25"  "v07:r16=25"  "v06" $DEALS
cell "r17=25"  "v07:r17=25"  "v06" $DEALS
cell "denial-all" "v07:r12=20,r13=10,r14=5,r15=5" "v06" $DEALS
# M2: the target's self-inflicted allocation defect, which the adversary can
# simply not carry.
cell "m2off"      "v06:m2=0"            "v06" $DEALS
cell "m2off-r12"  "v07:m2=0,r12=25"     "v06" $DEALS
# The composite the reconnaissance measured, and its ablations, so the three
# mechanisms are separated rather than pooled.
cell "search"        "$FCHEAP"                                     "v06" $DEALS
cell "search-r12"    "v07:r12=25,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26" "v06" $DEALS
cell "search-m2"     "v06:m2=0,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"   "v06" $DEALS
cell "composite"     "v07:m2=0,r12=25,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26" "v06" $DEALS
echo "== P8 transfer: does denial survive the frontier's own search? =="
cell "r12-vs-Fcheap"      "v07:r12=25"       "$FCHEAP" 6000
cell "composite-vs-Fcheap" "v07:m2=0,r12=25,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26" "$FCHEAP" 6000
echo "done -> $ART"
