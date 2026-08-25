#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery D: the detection floor RE-MEASURED at 4x the
# evaluation power.
#
# Phase 1 measured the floor at 12,000 evaluation games a bank and found
# C1 = C5 = 1.68 points, with nothing detecting the +0.86 rung.  It also
# established WHY: below ~1.7 points the binding constraint is evaluation power,
# not search power -- at `decl 0.08` the pooled excesses were positive (+0.69 C1,
# +0.98 C5) and the excess estimator's +/-1.2-point width simply could not
# exclude zero -- so the floor buys down as (evaluation games)^-1/2
# (INSTRUMENT.md 3.4, RESEARCH-LOG.md 1.16).
#
# That is an ASSERTION about a scaling law, and phase 2 cannot rest a null on it.
# If phase 2's honest answer is "nothing beats the frontier beyond the detection
# floor", the floor has to be a measured number at phase 2's own sample size, not
# a phase-1 number extrapolated.  So: the same responders, the same rungs, the
# same two banks, 24,000 deals x 2 = 48,000 games a cell instead of 12,000.
# Each bank's half-width falls from +/-0.89 to +/-0.45 points.
#
# The banks are EXTENDED, not replaced: a deal's seed is a function of its index
# alone (arena.hpp), so the first 6,000 deals of each bank are bit-identical to
# phase 1's and the additional 18,000 are new.  The comparison is therefore
# nested, and a floor that does not buy down is a refutation of the scaling law
# rather than a difference of banks.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
RUNS=${RUNS:-../research/v07/runs}
BIN=${BIN:-./fish7}
THREADS=${THREADS:-14}
DEALS=${DEALS:-24000}
BANKS=${BANKS:-"7021001 7021002"}
GTSEED=${GTSEED:-7022001}
GTDEALS=${GTDEALS:-12000}
ART=${ART:-$OUT/P4-floor.jsonl}
CLASSES=${CLASSES:-"C1"}
RUNGS=${RUNGS:-"none decl,hstr=0.05 decl,hstr=0.08 decl,hstr=0.11 decl,hstr=0.15 prior,hstr=0.6 leak,hstr=1.5 hit,hstr=1.0"}
mkdir -p "$OUT"
for B in $BANKS $GTSEED; do $BIN seeds --require="$B" >/dev/null || { echo "bank $B unusable"; exit 3; }; done

for H in $RUNGS; do
  if [ "$H" = "none" ]; then TARGET="v06"; else TARGET="v06:hcap=$H"; fi
  echo "=== rung $H ==="
  # dTrue re-measured at 24,000 games as well, so the ground truth and the
  # responder columns are resolved to the same order.
  $BIN match --a=v06 --b="$TARGET" --games=$GTDEALS --rotations=2 --seed=$GTSEED \
       --threads=$THREADS --json 2>/dev/null \
    | sed "s|^{|{\"battery\":\"P4floor\",\"row\":\"dTrue\",\"tag\":\"$H\",\"bank\":$GTSEED,|" >> "$ART"
  for C in $CLASSES; do
    W=$(cat "$RUNS/w-$C-$H.txt" 2>/dev/null || echo "")
    case "$C" in
      C1) [ -z "$W" ] && { echo "  $C: no phase-1 vector for $H"; continue; }; RESP="v06:allparams=$W" ;;
      C2) [ -z "$W" ] && { echo "  $C: no phase-1 vector for $H"; continue; }; RESP="v07:allparams=$W" ;;
      C5) if [ "$H" = "none" ]; then RM="v06"; else RM="v06:$(echo "hcap=$H" | tr ',' '+')"; fi
          RESP="v07i:idet=48,imodel=$RM" ;;
    esac
    for B in $BANKS; do
      $BIN match --a="$RESP" --b="$TARGET" --games=$DEALS --rotations=2 --seed=$B \
           --threads=$THREADS --json 2>/dev/null \
        | sed "s|^{|{\"battery\":\"P4floor\",\"row\":\"$C\",\"tag\":\"$H\",\"bank\":$B,|" >> "$ART"
      echo "  $C $H bank $B done"
    done
  done
done
echo "done -> $ART"
