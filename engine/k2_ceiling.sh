#!/bin/zsh
# K2 -- ledger L1's replay: the measured ceiling on declaration allocation.
# Pure replay: no policy changes, no new agent plays a single game.
set -e
cd "$(dirname "$0")"
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
mkdir -p "$R"
URGOFF="pool=-1,oppfloor=-1,force=1000000,askfloor=-1"

for BANK in 7030001 7030002; do
  ./fish7 v7decide --a=v06 --b=v06 --capture=a --games=4000 --rotations=2 \
      --seed=$BANK --threads=2 --json >> "$R/K2-ceiling-urgon.jsonl"
done
for BANK in 7030001 7030002; do
  ./fish7 v7decide --a=v06:$URGOFF --b=v06:$URGOFF --capture=a --games=3000 --rotations=2 \
      --seed=$BANK --threads=2 --json >> "$R/K2-ceiling-urgoff.jsonl"
done
./fish7 v7decide --a=v06 --b=v06 --capture=a --games=2000 --rotations=2 \
    --seed=7030001 --threads=2 --dump="$R/K2-records-v06-7030001.csv" --json \
    >> "$R/K2-ceiling-urgon.jsonl"
echo K2_CEILING_DONE
