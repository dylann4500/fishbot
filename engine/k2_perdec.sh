#!/bin/zsh
# K2 -- the per-decision movement of jalloc=1 against the v06 baseline already
# measured in K2-ceiling-urgon.jsonl on the same two banks.
cd "$(dirname "$0")"
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
for BANK in 7030001 7030002; do
  ./fish7 v7decide --a=v06:jalloc=1 --b=v06:jalloc=1 --capture=a --games=2500 \
      --rotations=2 --seed=$BANK --threads=2 --json >> "$R/K2-perdec-jalloc.jsonl"
  echo "perdec $BANK done"
done
echo K2_PERDEC_DONE
