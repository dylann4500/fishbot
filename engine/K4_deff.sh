#!/bin/zsh
# K4 cheap decisive probe: the DESIGN EFFECT of a per-decision estimator.
# Ledger L5 assumes a decision is worth a unit of effective sample.  Decisions
# inside one deal are not independent, so the true effective sample is
# nDecisions / DEFF, and DEFF has never been measured in this corpus.
# 24 independent 100-deal x 2-rotation blocks per arm; the block-to-block SD of
# each per-decision rate is compared against its binomial baseline.
cd "$(dirname "$0")"
OUT="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results/K4-designeffect.jsonl"
: > "$OUT"
for ARM in "v06" "v07:r12=25"; do
  for i in $(seq 1 24); do
    S=$((74000000 + i))
    R=$(./fish7 match --a="$ARM" --b=v06 --seed=$S --games=100 --rotations=2 --threads=2 --json)
    echo "{\"arm\":\"$ARM\",\"block\":$i,\"seed\":$S,\"r\":$R}" >> "$OUT"
  done
done
echo DONE
