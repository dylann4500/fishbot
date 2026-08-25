#!/bin/sh
# K5 SCREEN, part 2 -- the decisive cell.
#
# The tie-only learned re-ranker scores +1.58 over v06.  But `v06:rtie=1`, which
# resolves the SAME tie group by a hash of the public event stream and contains
# no learned content whatever, is already worth +1.14 [+0.52, +1.77] over v06
# (ADVERSARIES.md section H, 24,000 games, phase 2).  So the question the whole
# candidate turns on is whether the LEARNED content adds anything over a free
# random tie-break.  Head-to-head is the only clean way to ask.
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
K5="/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/f63a50cf-0555-4d3c-99d1-238332bf1a3d/scratchpad/K5run.sh"

for bank in 7030001 7030002; do
  "$K5" match --a=@SPECTIE --b="v06:rtie=1" --seed=$bank --games=12000 --rotations=2 --threads=2 --json \
      > "$R/K5tie-vs-rtie-$bank.jsonl" 2>&1
  echo "done tie-vs-rtie $bank"
done
# replicate the unrestricted arm's negative on the second bank
"$K5" match --a=@SPEC --b=v06 --seed=7030002 --games=12000 --rotations=2 --threads=2 --json \
    > "$R/K5-vs-v06-7030002.jsonl" 2>&1
echo "done full-v06 7030002"
echo ALLDONE2
