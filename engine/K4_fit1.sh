#!/bin/zsh
# One fit of the K4 matched-budget battery.  $1 = kpi, $2 = tag, $3 = seed.
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"; mkdir -p "$W"
KPI=${1:-selfalloc}; TAG=${2:-s1}; SEED=${3:-7030004}
GENS=${GENS:-6}; POP=${POP:-12}; DEALS=${DEALS:-150}; PANEL=${PANEL:-v06}
ID="K4-fit-$TAG-$KPI"
echo "=== $ID panel=$PANEL gens=$GENS pop=$POP deals=$DEALS seed=$SEED ==="
T0=$(date +%s)
OUT=$(./fish7 tune --panel="$PANEL" --base=v07 --full --fromv6 --kpi="$KPI" \
      --games=$DEALS --pop=$POP --elite=$(( POP / 3 + 1 )) --gens=$GENS \
      --beta=1 --sigmarel=0.08 --paired --obj=min --seed=$SEED --threads=2 \
      --out="$RES/$ID.jsonl" | tail -1 | sed 's/^weights=//')
T1=$(date +%s)
echo "$OUT" > "$W/$ID.txt"
echo "  done in $((T1-T0))s"
echo FITDONE
