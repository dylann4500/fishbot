#!/bin/zsh
# K4 SCREEN, F-cheap end.  Only the `win` arm is measured here: it is the only
# one of the four that is not negative against F-fast, and F-cheap sits +1.89
# above F-fast, so the other three cannot reach it.  6,000 games a bank (+/-1.27).
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
OUT="$RES/K4-screen-s1-Fcheap.jsonl"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
: > "$OUT"
for KPI in win; do
  SPEC="v07:allparams=$(cat "$W/K4-fit-s1-$KPI.txt")"
  for BANK in 7030001 7030002; do
    echo "--- $KPI vs Fcheap bank $BANK ---"
    R=$(./fish7 match --a="$SPEC" --b="$FCHEAP" --seed=$BANK --games=3000 \
          --rotations=2 --threads=2 --json)
    echo "{\"tag\":\"s1\",\"kpi\":\"$KPI\",\"opp\":\"Fcheap\",\"oppSpec\":\"$FCHEAP\",\"bank\":$BANK,\"r\":$R}" >> "$OUT"
    echo "$R" | python3 -c "import sys,json; d=json.load(sys.stdin); print('   edge %+.2f  ci [%+.2f, %+.2f]  n=%d  %.1f g/s' % (100*d['winRateA']-50, 100*d['ci'][0]-50, 100*d['ci'][1]-50, d['games'], d['gamesPerSec']))"
  done
done
echo FCHEAPDONE
