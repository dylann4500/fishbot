#!/bin/zsh
# K4 replication of the core matched-budget pair at a SECOND fitting seed.
# A 6-generation CEM random-walks, so a single trajectory is an anecdote.  Seed
# 7011001 is a phase-1 diagnostic training bank, disjoint from the evaluation
# banks 7030001/7030002 and from the s1 fitting bank 7030004.
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
for KPI in win selfdecl; do
  ./K4_fit1.sh "$KPI" s2 7011001
done
OUT="$RES/K4-screen-s2-Ffast.jsonl"
: > "$OUT"
for KPI in win selfdecl; do
  SPEC="v07:allparams=$(cat "$W/K4-fit-s2-$KPI.txt")"
  for BANK in 7030001 7030002; do
    echo "--- s2 $KPI vs Ffast bank $BANK ---"
    R=$(./fish7 match --a="$SPEC" --b=v06 --seed=$BANK --games=3000 --rotations=2 --threads=2 --json)
    echo "{\"tag\":\"s2\",\"kpi\":\"$KPI\",\"opp\":\"Ffast\",\"oppSpec\":\"v06\",\"bank\":$BANK,\"r\":$R}" >> "$OUT"
    echo "$R" | python3 -c "import sys,json;d=json.load(sys.stdin);print('   edge %+.2f  ci [%+.2f, %+.2f]  n=%d'%(100*d['winRateA']-50,100*d['ci'][0]-50,100*d['ci'][1]-50,d['games']))"
  done
done
echo REPDONE
