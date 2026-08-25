#!/bin/zsh
# K4 commit gate: the MIRROR pathology run for every configuration reported,
# which is the v0.7 deployment configuration.  Reference values for v06 in
# mirror at 800 games are in the phase brief.
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
TAG=${1:-s1}
OUT="$RES/K4-gate.txt"
: > "$OUT"
{
echo "### K4 commit gate -- MIRROR (self-play), 400 deals x 2 rotations, seed 31, 2 threads"
echo
echo "--- reference: v06 mirror ---"
./fish7 pathology --a=v06 --b=v06 --games=400 --rotations=2 --seed=31 --threads=2
for KPI in win selfdecl selfask selfalloc; do
  F="$W/K4-fit-$TAG-$KPI.txt"
  [ -s "$F" ] || continue
  echo
  echo "--- K4-fit-$TAG-$KPI (mirror) ---"
  ./fish7 pathology --a="v07:allparams=$(cat "$F")" --b="v07:allparams=$(cat "$F")" \
      --games=400 --rotations=2 --seed=31 --threads=2
done
} >> "$OUT" 2>&1
echo GATEDONE
