#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery E: CHANNEL CEILINGS.
#
# An adversary that raises some quantity in the target is only interesting if
# raising that quantity is worth something.  This battery measures the worth
# directly, by handing the adversary omnipotence over the mechanism instead of
# asking it to earn it: the TARGET is modified so the mechanism is permanently
# on or permanently off, and the unmodified incumbent plays it.  The resulting
# edge is the CEILING on what any adversary attacking that mechanism could ever
# collect, and it is measured before any adversary is fitted, in exactly the way
# phase 1 measured dTrue before fitting any responder.
#
# The mechanism under test is the declaration URGENCY predicate (v05.hpp:939-948).
# It fires when ANY of four clauses holds, and in the v0.6 mirror it fires on
# 50.5% of voluntary declarations, at 95.74% accuracy against 99.10% for the
# declarations taken with time in hand.  The four clauses are separated here
# because they are not equally attackable:
#   pool=45      unresolvedCount <= patiencePool          always true
#   oppfloor=54  oppCards <= oppCardFloor                 always true  <- the adversary's own hand count
#   force=1      pub.nEvents >= forceDeclareEvents        always true  <- the length of the game
#   askfloor=1.1 bestAskProbability < askFloor            always true  <- a starved posterior
# and the fifth arm is the OTHER side of the same question, which is the control
# that decides whether "urgency" is a defect at all: a target for which no clause
# can ever fire.  If never-urgent LOSES, urgency is a correct mechanism and an
# adversary that induces it is collecting a bounded, possibly negative, prize.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-14}
DEALS=${DEALS:-6000}
BANK=${BANK:-7030002}
ART=${ART:-$OUT/P7-ceilings.jsonl}
mkdir -p "$OUT"
$BIN seeds --require=$BANK >/dev/null || { echo "bank $BANK unusable"; exit 3; }

cell () { local TAG="$1" B="$2"
  $BIN match --a=v06 --b="$B" --games=$DEALS --rotations=2 --seed=$BANK --threads=$THREADS --json 2>/dev/null \
    | sed "s|^{|{\"battery\":\"P7ceiling\",\"tag\":\"$TAG\",\"bank\":$BANK,|" >> "$ART"
  echo "  done $TAG  $B"; }

echo "== P7 channel ceilings, bank $BANK, $DEALS deals x 2 =="
cell control            "v06"
cell urg-always-pool    "v06:pool=45"
cell urg-always-oppcard "v06:oppfloor=54"
cell urg-always-events  "v06:force=1"
cell urg-always-askfl   "v06:askfloor=1.1"
cell urg-always-all     "v06:pool=45,oppfloor=54,force=1,askfloor=1.1"
cell urg-never          "v06:pool=-1,oppfloor=-1,force=1000000,askfloor=-1"
# The M2 defect: v05.hpp:851 raises pTeam to pAlloc, so the team-ownership gate
# is passed on the strength of a quantity computed by a different approximation.
cell m2-off             "v06:m2=0"
cell m2-off-never-urg   "v06:m2=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1"
# The declaration channel's own ceiling, for scale: a target that cannot declare
# at all, and one whose declarations are always correct is not constructible, so
# this is the lower anchor only.
cell no-declare         "v06:declare=0"
echo "done -> $ART"
