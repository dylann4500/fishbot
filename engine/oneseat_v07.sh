#!/usr/bin/env bash
# FishBot v0.7 -- phase 2: the ONE-SEAT DEVIATION COLUMN (threat-model T2).
#
# T2: "The headline adversary controls all three opposing seats.  A ONE-SEAT
# deviation column is reported alongside it, ALWAYS, on the same deals."  The
# reason is not aesthetic: best-responding with a single perfect-recall seat, the
# other five fixed, is an LP over that seat's sequence form and is polynomial,
# while three decentralised seats is APX-hard; and bridge's two published
# partnership-AI evaluations rank agents differently on the two metrics by about
# three times the half-width.  The column is the tractable anchor.
#
# `--partners=v06` seats ONE copy of the adversary and two copies of the target's
# policy on the adversary's team.  A mechanism that needs three coordinated seats
# should lose most of its edge here; one that is a per-seat improvement should
# keep about a third of it, since one seat in three is deviating.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7b}
THREADS=${THREADS:-8}
DEALS=${DEALS:-12000}
BANKS=${BANKS:-"7030001 7030003"}
ART=${ART:-$OUT/P11-oneseat.jsonl}
SPECS=${SPECS:-$OUT/p9-arms.tsv}
mkdir -p "$OUT"
for B in $BANKS; do $BIN seeds --require="$B" >/dev/null || exit 3; done
while IFS=$'\t' read -r ID CL SPEC; do
  case "$ID" in ''|\#*) continue;; esac
  for B in $BANKS; do
    $BIN match --a="$SPEC" --b=v06 --partners=v06 --games=$DEALS --rotations=2 --seed=$B \
         --threads=$THREADS --json 2>/dev/null \
      | sed "s|^{|{\"battery\":\"P11oneseat\",\"id\":\"$ID\",\"cluster\":\"$CL\",\"k\":\"k1\",\"bank\":$B,|" >> "$ART"
  done
  echo "  done $ID (k=1)"
done < "$SPECS"
# The one-switch defects too: a defect that only shows up with all three seats
# carrying it is a different object from one that shows up in a single seat.
for S in "v06:rtie=1" "v06:m1=0" "v06:m2=0" "v06:pool=-1,oppfloor=-1,force=1000000,askfloor=-1"; do
  for B in $BANKS; do
    $BIN match --a="$S" --b=v06 --partners=v06 --games=$DEALS --rotations=2 --seed=$B \
         --threads=$THREADS --json 2>/dev/null \
      | sed "s|^{|{\"battery\":\"P11oneseat\",\"id\":\"SW-$S\",\"cluster\":\"one-switch\",\"k\":\"k1\",\"bank\":$B,|" >> "$ART"
  done
  echo "  done $S (k=1)"
done
echo "done -> $ART"
