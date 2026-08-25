#!/bin/zsh
cd "$(dirname "$0")"
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
for BANK in 7030001 7030002; do
  ./fish7 v7side --a=v06:jalloc=1 --b=v06 --games=400 --seed=$BANK --threads=2 \
      --out="$R/K2-side-jalloc.jsonl" >> "$R/K2-side-jalloc.txt" 2>&1
  echo "side $BANK exit=$?"
done
echo K2_SIDE_DONE
