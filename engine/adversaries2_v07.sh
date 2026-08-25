#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery A2: the DECEPTION SPACE and the unexercised
# scripted machinery.
#
# The phase brief: "the corpus itself says the deception archetypes are three
# stylised hand-built manoeuvres and the wider deception space is unmeasured."
# That is exactly right, and it is narrower than it sounds: `silent`, `feint` and
# `withholder` are one V04Agent with one behavioural difference -- the set of
# half-suits it is willing to ask in (probe_deception.hpp:3-8) -- and each of
# them carries two parameters (`tol`, `k`) that NO artifact in the corpus ever
# sets.  Every published deception cell is at the struct defaults.  So the
# "wider deception space" is not only unexplored between archetypes, it is
# unexplored WITHIN each of the three.
#
# Two further pieces of machinery are compiled into the engine and switched on
# nowhere: `psychTells` on the scripted baselines (factory.hpp, `tells`), and the
# `roppo=` opponent model at operating points other than the one phase 1 used.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
RUNS=${RUNS:-../research/v07/runs}
BIN=${BIN:-./fish7}
THREADS=${THREADS:-14}
DEALS=${DEALS:-6000}
BANK=${BANK:-7030001}
TARGET=${TARGET:-v06}
TAG=${TAG:-Ffast}
ART=${ART:-$OUT/P1b-screen.jsonl}
mkdir -p "$OUT"
$BIN seeds --require=$BANK >/dev/null || { echo "seed registry check FAILED"; exit 3; }

run () { local CL="$1"; shift; local SPEC="$1"; shift
  $BIN match --a="$SPEC" --b="$TARGET" --games=$DEALS --rotations=2 --seed=$BANK \
       --threads=$THREADS --json 2>/dev/null \
    | sed "s|^{|{\"battery\":\"P1bscreen\",\"cluster\":\"$CL\",\"targetTag\":\"$TAG\",\"bank\":$BANK,|" >> "$ART"
  echo "  done $CL  $SPEC"; }

echo "== P1b deception space and unexercised machinery vs $TARGET =="

# ---- the deception space, swept in both of its parameters -----------------
for T in 0.02 0.05 0.10 0.20 0.40; do
  run deception "feint:tol=$T"
  run deception "silent:tol=$T"
  run deception "withholder:tol=$T"
done
for K in 1 3 6 12; do
  run deception "feint:k=$K"
  run deception "withholder:k=$K"
done
run deception "feint:tol=0.10,k=6"
run deception "feint:tol=0.20,k=3"
run deception "withholder:tol=0.10,k=6"
# The deception archetypes' own reference: the unrestricted v0.4 they are built
# from.  If every restriction scores BELOW it, deception as implemented is a
# cost, not a weapon, and the taxonomy has to say so.
run deception "v04"

# ---- psychTells: compiled in, enabled in no artifact anywhere -------------
for B in bluffer detective lockout hunter diversifier; do
  run tells "$B:tells=1"
done

# ---- the in-class imitation of the feint's channel ------------------------
# feint restricts which half-suits it asks in; the linear class can express a
# graded version of the same thing through the two coordinates that price what
# an ask reveals about the asker's own holding (f[3] and f[19]).
for W in "w3=-2.0" "w3=-4.0" "w19=-6.0" "w19=-12.0" "w3=-2.0,w19=-6.0" "w3=1.9,w19=-12.0"; do
  run inclass-feint "v06:$W"
done

# ---- the search-based responder at operating points phase 1 never used ----
C1=$(cat "$RUNS/w-C1-none.txt" 2>/dev/null || echo "")
if [ -n "$C1" ]; then
  run c3 "v06:allparams=$C1"
  run c3 "v06:allparams=$C1,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26,roppo=v06"
  run c3 "v06:allparams=$C1,s1=1,det=16,cand=6,kappa=2.0,maxq=26,roppo=v06"
  run c3 "v06:allparams=$C1,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
fi
run c3 "v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26,roppo=v06"
run c3 "v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26,roppo=v06"
run c3 "v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26,deadsearch=2"

# ---- the white-box class at settings other than the struct defaults -------
for S in "idet=96" "idet=24" "igain=2.0" "igain=0.5" "ifocus=1" "ifocus=0" "ikappa=1.0" "istep=2.5" "imaxq=26"; do
  run invert "v07i:$(echo $S),imodel=v06"
done
run invert "v07i:idet=96,igain=2.0,imodel=v06"
run invert "v07i:idet=48,imodel=v06,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"

# ---- URGENCY INDUCTION ----------------------------------------------------
# The target's declaration path takes a decision under `urgent` when ANY of four
# clauses fires (v05.hpp:939-948), and in the v0.6 mirror it fires on 50.5% of
# voluntary declarations, at 95.74% accuracy against 99.10% for the ones taken
# with time in hand.  Two of the four clauses are things an opponent moves:
#   oppCards <= oppCardFloor(2.999)   -- the opposing team's own total hand count
#   pub.nEvents >= forceDeclareEvents(220)  -- the length of the game
# and a third, bestAskProbability < askFloor(0.2657), is a starved posterior.
# The clause shares in the mirror are 15.3% / 0.0% / 47.0%.  These arms pull the
# first two levers directly and with no fitting at all: declare earlier and you
# empty your own hands sooner; lengthen the game and you cross a threshold the
# target has never reached in a normal deal.
for V in -0.05 -0.10 -0.20 -0.40; do run urgency "v06:vmargin=$V"; done
for D in 0.60 0.70 0.95; do run urgency "v06:decl=$D"; done
run urgency "v06:vmargin=-0.10,decl=0.60"
run urgency "v06:vmargin=-0.20,decl=0.60"
run urgency "v06:dead=1,deadbudget=8"
run urgency "v06:dead=1,deadbudget=20"
run urgency "v06:vmargin=-0.10,dead=1,deadbudget=8"
run urgency "v06:pool=20"
run urgency "v06:pool=45"

# ---- C6, the scripted-adaptive class ---------------------------------------
# The manoeuvre and both of its polarities (v07_adapt.hpp).  `mode=0` is the
# identity control and must sit at zero.
run c6 "v07c:mode=0"
for M in 1 2 3; do run c6 "v07c:mode=$M"; done
run c6 "v07c:mode=1,holdmax=30"
run c6 "v07c:mode=1,holdmax=120"
run c6 "v07c:mode=2,aggr=-2.0"
run c6 "v07c:mode=2,lockp=0.80,ambig=1"
run c6 "v07c:mode=2,lockp=0.99,ambig=3"
run c6 "v07c:mode=3,holdmax=30"

echo "done -> $ART"
