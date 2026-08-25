#!/bin/zsh
# K4 evaluation.  Every fitted vector is scored paired, on BOTH evaluation banks,
# against the frontier's two cheap ends.  Fitting bank was 7030004 and is
# disjoint from these.
#   $1 = tag (s1), $2 = games per bank, $3 = opponent spec, $4 = opponent label
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
TAG=${1:-s1}; G=${2:-12000}; OPP=${3:-v06}; OLAB=${4:-Ffast}
OUT="$RES/K4-screen-$TAG-$OLAB.jsonl"
: > "$OUT"
for KPI in win selfdecl selfask selfalloc; do
  F="$W/K4-fit-$TAG-$KPI.txt"
  [ -s "$F" ] || { echo "MISSING $F"; continue; }
  SPEC="v07:allparams=$(cat "$F")"
  for BANK in 7030001 7030002; do
    echo "--- $KPI vs $OLAB bank $BANK ---"
    R=$(./fish7 match --a="$SPEC" --b="$OPP" --seed=$BANK --games=$((G/2)) \
          --rotations=2 --threads=2 --json)
    echo "{\"tag\":\"$TAG\",\"kpi\":\"$KPI\",\"opp\":\"$OLAB\",\"oppSpec\":\"$OPP\",\"bank\":$BANK,\"r\":$R}" >> "$OUT"
    echo "$R" | python3 -c "import sys,json; d=json.load(sys.stdin); print('   edge %+.2f  ci [%+.2f, %+.2f]  n=%d' % (100*d['winRateA']-50, 100*d['ci'][0]-50, 100*d['ci'][1]-50, d['games']))"
  done
done
echo EVALDONE
