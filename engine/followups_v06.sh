#!/usr/bin/env bash
# Follow-ups forced by the adversarial re-read (research/v06/notes/V1-*).
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v06/results}
mkdir -p "$OUT"

echo "== F0 the search result the ladder resolved: does it replicate, and does it add to the refit? =="
: > "$OUT/F0-search-confirm.jsonl"
for CFG in "v06:legacy=1,s1=1,det=12,cand=4,kappa=2.5" "v06:legacy=1,s1=1,det=12,cand=4,kappa=4"; do
  for S in 31337 515151; do
    echo "{\"cfg\":\"$CFG\",\"opp\":\"v05\",\"bank\":$S}" >> "$OUT/F0-search-confirm.jsonl"
    ./fish match --a="$CFG" --b=v05 --games=120 --rotations=6 --seed="$S" --json >> "$OUT/F0-search-confirm.jsonl"
    echo >> "$OUT/F0-search-confirm.jsonl"
  done
done
# does the search add ON TOP of the refit?  root = v0.6's vector, rollout = v0.6.
for CFG in "v06:s1=1,det=12,cand=4,kappa=2.5,roll=v06" "v06:s1=1,det=12,cand=4,kappa=2.5"; do
  for S in 90210 31337; do
    echo "{\"cfg\":\"$CFG\",\"opp\":\"v06\",\"bank\":$S}" >> "$OUT/F0-search-confirm.jsonl"
    ./fish match --a="$CFG" --b=v06 --games=120 --rotations=6 --seed="$S" --json >> "$OUT/F0-search-confirm.jsonl"
    echo >> "$OUT/F0-search-confirm.jsonl"
  done
done

echo "== F1 the clean 2x2: extra ask terms x v0.5's chain/threat pass =="
# `xf=0` is a CONFOUNDED ablation: a non-zero extra weight is what sets
# extraFeats, and extraFeats is what selects the v0.6 scoring path, so xf=0 also
# restores the chain pass.  Zeroing the three weights while leaving extraFeats on
# gives the fourth cell.
./fish ablate --ref=v06 --panel=v05,v04,v03,lockout,detective,withholder \
  --games=400 --rotations=2 --seed=606060 \
  --variants="v06:wvoid=0,wteam=0,wlast=0;v06:chain2=1;v06:xf=0" > "$OUT/F1-chain2x2.json" 2>&1

echo "== F2 belief with the policy prior deleted OUTRIGHT (theta and phi) =="
{ echo "### phi = 0 sweep (the theta=0,phi=0 row is the policy-agnostic approximation)"
  ./fish v6probe --mode=belief --a=v05 --b=v05 --games=120 --seed=31 --theta=0,0.44458 --phi=0
  echo; echo "### phi = 0.12198 sweep, for comparison"
  ./fish v6probe --mode=belief --a=v05 --b=v05 --games=120 --seed=31 --theta=0,0.44458 --phi=0.12198
} > "$OUT/F2-belief-noprior.txt" 2>&1

echo "== F3 exact-engine validation, to bound the belief claim =="
./fish selftest --games=40 > "$OUT/F3-selftest.txt" 2>&1 || true
./fish oracle --games=120 > "$OUT/F3-oracle.txt" 2>&1 || true

echo "== F4 per-style at two further banks (the worst-case claim rests on more than one) =="
: > "$OUT/F4-perstyle-banks.jsonl"
for S in 90210 424242; do
  for OPP in v05 v04 v03 lockout detective silent feint withholder; do
    for A in v06 v05; do
      echo "{\"bank\":$S}" >> "$OUT/F4-perstyle-banks.jsonl"
      ./fish match --a="$A" --b="$OPP" --games=250 --rotations=6 --seed="$S" --json >> "$OUT/F4-perstyle-banks.jsonl"
      echo >> "$OUT/F4-perstyle-banks.jsonl"
    done
  done
done

echo "== F5 exploitability: best-response probe, positive-controlled on v0.4 =="
OUT="$OUT" BASE=v05 TARGETS="v04 v05" GENS=10 GAMES=150 POP=14 EVAL=400 ./exploitability_v06.sh \
  > "$OUT/F5-exploitability.log" 2>&1 || true

echo "done"
