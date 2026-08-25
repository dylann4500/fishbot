#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery F: HARNESS probes.
#
# The phase brief: "An exploiter that works only through a harness defect is a
# finding about the harness -- file it as such."  This battery is the test that
# separates the two.  Every probe here asks whether a measured edge is a property
# of the GAME or of the SIMULATOR, and each one is designed so that a null is
# informative.
#
#   H1  arm swap.  Every exploitability number in the corpus is measured with the
#       adversary as the A arm.  runMatch builds A[0..2] before B[0..2]
#       (arena.hpp:116-119) and plays both orientations, so the design should be
#       symmetric -- but "should be" is not a measurement.  The same pair is
#       played with the arms exchanged on the same bank; the two edges must be
#       exact negatives.
#   H2  the action cap.  game.hpp:497 breaks the game at rules.maxAsks and
#       game.hpp:394 adjudicates every unresolved half-suit by PHYSICAL MAJORITY,
#       ties to the holder of the lowest card.  The physical game has no such
#       rule.  If an adversary's edge moves when maxAsks moves, the edge is
#       partly an artifact of the cap.
#   H3  rule dialects.  P-8 in the ledger records that the corpus's dialect table
#       has one bundled row and never sweeps the arbitration order.  An
#       exploitability number that depends on the dialect is dialect-specific and
#       must say so.
#   H4  the A2 correlation device.  arena.hpp draws correlationSignal() per game
#       and hands it to no policy through any argument.  A policy that does not
#       read it must be bit-identical with the device on and off; if it is not,
#       the device is a side channel rather than an adversary grant.
#   H5  the stalling family.  Does driving games to the cap actually win, and does
#       the win ride on adjudicated half-suits rather than on declared ones?
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7}
THREADS=${THREADS:-14}
BANK=${BANK:-7051001}
DEALS=${DEALS:-6000}
ART=${ART:-$OUT/P6-harness.jsonl}
mkdir -p "$OUT"
$BIN seeds --require=$BANK >/dev/null || { echo "bank $BANK unusable"; exit 3; }

emit () { # emit <probe> <a> <b> [extra flags...]
  local P="$1" A="$2" B="$3"; shift 3
  $BIN match --a="$A" --b="$B" --games=$DEALS --rotations=2 --seed=$BANK --threads=$THREADS "$@" --json 2>/dev/null \
    | sed "s|^{|{\"probe\":\"$P\",\"bank\":$BANK,\"flags\":\"$*\",|" >> "$ART"
  echo "  $P  $A  vs  $B  $*"
}

ADV=${ADV:-"v06:vmargin=-0.02"}

echo "== H1 arm swap: the same pair with the arms exchanged =="
emit H1-forward "$ADV" "v06"
emit H1-reverse "v06" "$ADV"

echo "== H2 the action cap: does the edge move with maxAsks? =="
for M in 200 300 400 800 2000; do
  emit "H2-maxasks$M" "$ADV" "v06" --maxasks=$M
done
echo "== H2b the same sweep for the stalling family =="
for M in 200 400 2000; do
  emit "H2b-dead-maxasks$M" "v06:dead=1" "v06" --maxasks=$M
  emit "H2b-nodecl-maxasks$M" "v06:declare=0" "v06" --maxasks=$M
done

echo "== H3 rule dialects =="
emit H3-default    "$ADV" "v06"
emit H3-legacy     "$ADV" "v06" --legacy
emit H3-nooot      "$ADV" "v06" --no-out-of-turn
emit H3-nocardless "$ADV" "v06" --no-cardless-declare
emit H3-arbhigh    "$ADV" "v06" --arb=high
emit H3-arbturn    "$ADV" "v06" --arb=turn

echo "== H4 the A2 correlation device must be invisible to a policy that does not read it =="
A=$($BIN match --a=v06 --b=v06 --games=200 --seed=$BANK --threads=1 --json 2>/dev/null \
    | python3 -c "import json,sys;d=json.load(sys.stdin);[d.pop(k,None) for k in ('seconds','gamesPerSec','threads','power')];print(json.dumps(d,sort_keys=True))" | md5)
B=$($BIN match --a=v06 --b=v06 --games=200 --seed=$BANK --threads=1 --correlated --json 2>/dev/null \
    | python3 -c "import json,sys;d=json.load(sys.stdin);[d.pop(k,None) for k in ('seconds','gamesPerSec','threads','power')];print(json.dumps(d,sort_keys=True))" | md5)
echo "{\"probe\":\"H4-corr-invisibility\",\"md5_off\":\"$A\",\"md5_on\":\"$B\",\"pass\":$([ "$A" = "$B" ] && echo true || echo false)}" >> "$ART"
[ "$A" = "$B" ] && echo "  A2 DEVICE INVISIBLE: PASS" || echo "  A2 DEVICE INVISIBLE: FAIL"

echo "== H5 the stalling family, at the shipped cap =="
for S in "v06:declare=0" "v06:dead=1" "v06:dead=1,deadbudget=8" "v06:pool=45" "v06:force=400" "v06:norepeat=1,dead=1"; do
  emit H5-stall "$S" "v06"
done
echo "done -> $ART"
