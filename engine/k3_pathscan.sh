#!/bin/zsh
set -u
E="/Users/dylan/Documents/GitHub/fish optimization/.claude/worktrees/wf_88eccd0d-4c3-4/engine"
OUT="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results/K3-stalldist-baseline.txt"
cd "$E"
: > "$OUT"
for spec in \
  "v06:stall=999" \
  "v06:rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=999" \
  "v06:rtie=1,m1=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=999" \
  "v06:m1=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=999"
do
  echo "### MIRROR $spec" >> "$OUT"
  ./fish7 pathology --a="$spec" --b="$spec" --games=400 --rotations=2 --seed=31 --threads=2 >> "$OUT" 2>&1
  echo "" >> "$OUT"
done
echo DONE
