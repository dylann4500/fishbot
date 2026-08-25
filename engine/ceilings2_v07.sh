#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery E2: the PRESSURE CLIFF, mapped.
#
# Battery E separated two mechanisms that share one threshold and are not the
# same thing.  `pub.nEvents >= cfg.forceDeclareEvents` (220) appears BOTH as the
# third clause of the urgency predicate (v05.hpp) AND as the trigger of
# `pressure()` (v05.hpp), and the two do very different damage:
#
#   urgency  replaces an expected-value comparison with a threshold, and on a
#            LOCKED half-suit that threshold is pAlloc >= 0.5.  Measured ceiling:
#            +0.38 points [-0.06, +0.82] -- a target that is PERMANENTLY urgent
#            loses less than half a point, and its declaration accuracy falls
#            only 0.9777 -> 0.9768.  Below every detection floor.  Dead.
#   pressure at rung 1 relaxes `teamFloor` from minTeamProb = 0.849 to 0.25,
#            bypasses the capacity gate, and cashes any half-suit at
#            pAlloc >= 0.5.  Measured ceiling: +15.18 points [+14.41, +15.94],
#            with the target's declaration accuracy collapsing 0.9777 -> 0.8115.
#
# So v0.6 carries a fifteen-point cliff, and it sits behind an event counter.
# The question that decides whether it is a vulnerability or a curiosity is how
# long a game an adversary can produce: the baseline is ~95 events and the
# longest any adversary in this phase produced is ~105, against the 220 the
# cliff needs.  This battery maps the cliff as a function of where the threshold
# sits, so that "how much lengthening would be needed" has a number rather than
# a shrug.  `force=N` makes the target behave as it would in a game that had
# already reached N events -- it is a ceiling probe on the TARGET, not an
# adversary.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-6}
DEALS=${DEALS:-4000}
BANK=${BANK:-7030002}
ART=${ART:-$OUT/P7b-pressure.jsonl}
mkdir -p "$OUT"
$BIN seeds --require=$BANK >/dev/null || exit 3
cell () { $BIN match --a=v06 --b="$2" --games=$DEALS --rotations=2 --seed=$BANK --threads=$THREADS --json 2>/dev/null \
  | sed "s|^{|{\"battery\":\"P7bpressure\",\"tag\":\"$1\",\"bank\":$BANK,|" >> "$ART"; echo "  done $1"; }
echo "== P7b the pressure cliff =="
cell force-220-shipped "v06"
for N in 1 40 60 80 100 110 120 140 160 180 200; do cell "force-$N" "v06:force=$N"; done
# Rung 2 as well: forceStage2 cashes the best candidate whatever it is, and its
# threshold is 7/5 x forceDeclareEvents.
cell stage2-at-40  "v06:stage2=1,force=40"
cell stage2-at-100 "v06:stage2=1,force=100"
echo "done -> $ART"
