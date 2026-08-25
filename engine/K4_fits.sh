#!/bin/zsh
# K4 -- the claim under test (ledger L5).  Four fits at MATCHED BUDGET from the
# same starting vector (v0.6 + 18 zeros = v0.6 bit for bit) on the same common
# random numbers, differing ONLY in the objective the CEM climbs:
#   win       the incumbent per-GAME objective            (1 Bernoulli / game)
#   selfdecl  the arm's own declaration accuracy          (4.48 decisions / game)
#   selfask   the arm's own ask hit rate                  (~85 decisions / game)
#   selfalloc 1 - the arm's own allocation-error share    (L1's error class)
# Budget: 6 gens x pop 12 x 150 deals x 2 rotations = 21,600 games per fit,
# phase 1's small rung.  CEM settings copied verbatim from phase 2's
# exploiters_v07.sh so the comparison is at the corpus's own hyperparameters.
# Fitting bank is 7030004, disjoint from evaluation banks 7030001/7030002.
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
mkdir -p "$W"
GENS=${GENS:-6}; POP=${POP:-12}; DEALS=${DEALS:-150}
SEED=${SEED:-7030004}
PANEL=${PANEL:-v06}
TAG=${TAG:-s1}
for KPI in win selfdecl selfask selfalloc; do
  ID="K4-fit-$TAG-$KPI"
  echo "=== $ID  panel=$PANEL gens=$GENS pop=$POP deals=$DEALS seed=$SEED ==="
  T0=$(date +%s)
  OUT=$(./fish7 tune --panel="$PANEL" --base=v07 --full --fromv6 --kpi="$KPI" \
        --games=$DEALS --pop=$POP --elite=$(( POP / 3 + 1 )) --gens=$GENS \
        --beta=1 --sigmarel=0.08 --paired --obj=min --seed=$SEED --threads=2 \
        --out="$RES/$ID.jsonl" | tail -1 | sed 's/^weights=//')
  T1=$(date +%s)
  echo "$OUT" > "$W/$ID.txt"
  echo "  done in $((T1-T0))s, $(echo -n "$OUT" | wc -c) bytes of weights"
done
echo ALLDONE
