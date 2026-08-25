#!/bin/zsh
# K2 -- the SCREEN for jalloc=1 (joint-posterior declaration allocation).
set -e
cd "$(dirname "$0")"
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"

for BANK in 7030001 7030002; do
  ./fish7 match --a=v06:jalloc=1 --b=v06 --seed=$BANK --games=12000 \
      --rotations=2 --threads=2 --json >> "$R/K2-screen-vs-v06.jsonl"
  echo "cell $BANK v06 done"
done
for BANK in 7030001 7030002; do
  ./fish7 match --a="v06:jalloc=1" --b="$FCHEAP" --seed=$BANK --games=4000 \
      --rotations=2 --threads=2 --json >> "$R/K2-screen-vs-fcheap.jsonl"
  echo "cell $BANK fcheap done"
done
echo K2_SCREEN_DONE
