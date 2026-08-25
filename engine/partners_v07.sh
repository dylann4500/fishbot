#!/usr/bin/env bash
# FishBot v0.7 -- phase 2: ATTACK PARTNER COORDINATION, and settle ledger L7.
#
# L7 ("Independent multi-agent search corrupts partners' beliefs", risk, gates
# L2) names its cheapest decisive experiment: "Run the search on ONE seat of the
# team only and compare, paired, to all three."  It had never been run because it
# was not expressible: `runMatch` built all three B seats from one spec string.
# `--partnersb` (added this phase) makes a mixed target team constructible.
#
# The literature this tests: Lerer, Hu, Foerster & Brown (SPARTA, AAAI 2020)
# measure two agents each searching while assuming the partner plays the
# blueprint, and report 22.99 -> 14.41 -- a 37% relative collapse.  Three agents
# is strictly worse on that axis: each deviation corrupts two partners.  If the
# collapse is present in Fish, one searching seat should be worth MORE THAN A
# THIRD of three searching seats; if Fish's public-action structure protects it,
# three should scale roughly linearly and L7 closes as a negative -- which the
# ledger itself calls "a genuinely interesting negative".
#
# The rows are stated as the SEARCH ARM's edge over the deployed policy, so a
# larger number is a stronger target team.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-6}
DEALS=${DEALS:-6000}
BANKS=${BANKS:-"7030001 7030003"}
ART=${ART:-$OUT/P12-partners.jsonl}
mkdir -p "$OUT"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
FMID="v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26"
for B in $BANKS; do $BIN seeds --require="$B" >/dev/null || exit 3; done

cell () { # cell <tag> <A> <B> [extra...]
  local T="$1" A="$2" BB="$3"; shift 3
  for B in $BANKS; do
    $BIN match --a="$A" --b="$BB" --games=$DEALS --rotations=2 --seed=$B --threads=$THREADS "$@" --json 2>/dev/null \
      | sed "s|^{|{\"battery\":\"P12partners\",\"tag\":\"$T\",\"bank\":$B,\"flags\":\"$*\",|" >> "$ART"
  done
  echo "  done $T"; }

echo "== P12 L7: how many of the target's seats have to search =="
cell search3-cheap "$FCHEAP" "v06"
cell search1-cheap "$FCHEAP" "v06" --partners=v06
cell search3-mid   "$FMID"   "v06"
cell search1-mid   "$FMID"   "v06" --partners=v06
echo "== P12b the same question for the phase-2 arms: does the mechanism need three seats? =="
cell contest3 "v07:r12=25" "v06"
cell contest1 "v07:r12=25" "v06" --partners=v06
cell noURG3   "v06:pool=-1,oppfloor=-1,force=1000000,askfloor=-1" "v06"
cell noURG1   "v06:pool=-1,oppfloor=-1,force=1000000,askfloor=-1" "v06" --partners=v06
echo "done -> $ART"
