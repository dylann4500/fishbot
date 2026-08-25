#!/bin/sh
# K5 SCREEN, retuned for a machine carrying six agents (load ~70 on 15 cores).
# Paired duplicate blocks, both evaluation banks.  Training bank was 7030004,
# disjoint from these.  --games is DEALS; played games are twice that.
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
K5="/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/f63a50cf-0555-4d3c-99d1-238332bf1a3d/scratchpad/K5run.sh"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"

# 1. the two deployment shapes against F-fast.  12,000 deals = 24,000 games, +-0.63.
for bank in 7030001 7030002; do
  "$K5" match --a=@SPEC --b=v06 --seed=$bank --games=12000 --rotations=2 --threads=2 --json \
      > "$R/K5-vs-v06-$bank.jsonl" 2>&1
  echo "done full-v06 $bank"
done
for bank in 7030001 7030002; do
  "$K5" match --a=@SPECTIE --b=v06 --seed=$bank --games=12000 --rotations=2 --threads=2 --json \
      > "$R/K5tie-vs-v06-$bank.jsonl" 2>&1
  echo "done tie-v06 $bank"
done
# 2. against F-cheap.  Search-speed, so a smaller cell: 4,000 deals = 8,000 games, +-1.10.
for bank in 7030001 7030002; do
  "$K5" match --a=@SPEC --b="$FCHEAP" --seed=$bank --games=4000 --rotations=2 --threads=2 --json \
      > "$R/K5-vs-fcheap-$bank.jsonl" 2>&1
  echo "done fcheap $bank"
done
echo ALLDONE
