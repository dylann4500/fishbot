#!/bin/zsh
# K4 mechanical side-channel gate.  Every configuration reported, on the standard
# phase-3 cell (400 games, bank 7030001), replicated on 7030002 for the headline
# arm.  Exit 0 = CERTIFIED, 1 = NOT CERTIFIED.
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
TAG=${1:-s1}; BANK=${2:-7030001}
OUT="$RES/K4-sidechannel-$BANK.txt"
: > "$OUT"
for KPI in win selfdecl selfask selfalloc; do
  F="$W/K4-fit-$TAG-$KPI.txt"
  [ -s "$F" ] || continue
  echo "=== K4-fit-$TAG-$KPI  bank $BANK ===" >> "$OUT"
  ./fish7 v7side --a="v07:allparams=$(cat "$F")" --b=v06 --games=400 \
      --seed=$BANK --threads=2 >> "$OUT" 2>&1
  echo "EXIT=$?  ($KPI)" >> "$OUT"
  echo "" >> "$OUT"
  echo "$KPI exit=$?"
done
echo SIDEDONE
