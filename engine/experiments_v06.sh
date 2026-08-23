#!/usr/bin/env bash
# FishBot v0.6 experiment battery.  Every number in paper/numbers_v06.tex traces
# to one of these artifacts.  Run from engine/ after `make`.
#
# Step 0 is a GATE, not a measurement: the pathology KPIs and the identity
# controls must pass before any strength number is collected.  The v0.5 study's
# own ablation table contains two configurations that score six points higher
# than the shipped policy while carrying a 373-ask dead run and killing 14% of
# games at the action limit, so a battery ordered by win rate first will select
# the broken policy.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v06/results}
V6=${V6:-v06}
mkdir -p "$OUT"

echo "== E0 identity controls (must pass) =="
{
  echo "### v06 carrying v05's vector with every switch off must be v05 BIT FOR BIT"
  A=$(./fish pathology --a="$V6:legacy=1" --b="$V6:legacy=1" --games=60 --seed=31 | tail -n +2 | md5)
  B=$(./fish pathology --a=v05 --b=v05 --games=60 --seed=31 | tail -n +2 | md5)
  echo "v06-off md5 $A"; echo "v05     md5 $B"
  if [ "$A" = "$B" ]; then echo "IDENTITY PASS"; else echo "IDENTITY FAIL"; fi
  echo
  echo "### BlockDP table aliasing (v0.6 E2): QUERY mismatches must be 0"
  ./fish blockalias --a=v04 --games=25 --seed=31
  ./fish blockalias --a=v05 --games=25 --seed=31
} > "$OUT/E0-identity.txt" 2>&1
tail -6 "$OUT/E0-identity.txt"

echo "== E1 verify: rules, information safety, belief soundness =="
./fish verify --games=600                     > "$OUT/E1-verify.txt" 2>&1

echo "== E2 pathology: the KPIs that gate a v0.6 commit =="
{ echo "### v0.6 mirror";        ./fish pathology --a="$V6" --b="$V6" --games=300 --rotations=2 --seed=31
  echo; echo "### v0.5 mirror";  ./fish pathology --a=v05  --b=v05  --games=300 --rotations=2 --seed=31
  echo; echo "### v0.4 mirror";  ./fish pathology --a=v04  --b=v04  --games=300 --rotations=2 --seed=31
  echo; echo "### v0.6 vs v0.5"; ./fish pathology --a="$V6" --b=v05  --games=300 --rotations=2 --seed=90210
} > "$OUT/E2-pathology.txt" 2>&1

echo "== E3 head-to-head against v0.5 and v0.4, five held-out banks =="
: > "$OUT/E3-headtohead.jsonl"
for S in 90210 31337 515151 777001 424242; do
  for OPP in v05 v04; do
    ./fish match --a="$V6" --b="$OPP" --games=300 --rotations=6 --seed="$S" --json >> "$OUT/E3-headtohead.jsonl"
    echo >> "$OUT/E3-headtohead.jsonl"
  done
done

echo "== E4 per-opponent profile and worst case (the headline is the MINIMUM) =="
: > "$OUT/E4-perstyle.jsonl"
for OPP in v05 v04 v03 v02 lockout detective diversifier hunter bluffer random silent feint withholder; do
  for A in "$V6" v05 v04; do
    ./fish match --a="$A" --b="$OPP" --games=300 --rotations=6 --seed=515253 --json >> "$OUT/E4-perstyle.jsonl"
    echo >> "$OUT/E4-perstyle.jsonl"
  done
done

echo "== E5 paired mechanism ablations against a common panel =="
./fish ablate --ref="$V6" --panel=v05,v04,v03,lockout,detective,withholder \
    --games=400 --rotations=2 --seed=606060 \
    --variants="v05;$V6:legacy=1;$V6:xf=0;$V6:s1=1,det=16,cand=6,kappa=2.0,maxq=26" > "$OUT/E5-ablations.json" 2>&1

echo "== E6 calibration =="
./fish calibrate --a="$V6" --b=v05 --games=400 --seed=717171 > "$OUT/E6-calibration.txt" 2>&1

echo "== E7 rule dialects =="
: > "$OUT/E7-rules.jsonl"
for FLAG in "" "--no-out-of-turn" "--no-cardless-declare" "--legacy"; do
  echo "{\"dialect\":\"${FLAG:-default}\"}" >> "$OUT/E7-rules.jsonl"
  ./fish match --a="$V6" --b=v05 --games=250 --rotations=6 --seed=828282 $FLAG --json >> "$OUT/E7-rules.jsonl"
  echo >> "$OUT/E7-rules.jsonl"
done

echo "== E8 the two structural results: exchangeable ties, and belief as a predictor =="
{ echo "### tie structure and every tie-break rule, v0.6 mirror"
  ./fish v6probe --mode=ties --a="$V6" --b="$V6" --games=150 --seed=31
  echo; echo "### tie structure, v0.5 mirror (reference)"
  ./fish v6probe --mode=ties --a=v05 --b=v05 --games=150 --seed=31
  echo; echo "### tie structure against a NON-mirror opponent"
  ./fish v6probe --mode=ties --a=v05 --b=v03 --games=150 --seed=90210
} > "$OUT/E8-ties.txt" 2>&1
./fish v6probe --mode=belief --a=v05 --b=v05 --games=120 --seed=31 > "$OUT/E8-belief.txt" 2>&1

echo "== E12 the search ladder: the optimizer's curse, and what a guard recovers =="
: > "$OUT/E12-search.jsonl"
# Row 1 is the control that isolates the statistic from the machinery: the same
# search code with the blueprint forced to decide.  Rows 2-6 are the deviation
# threshold swept from an unguarded argmax upward.
for CFG in "s1=1,det=8,cand=6,blend=1000000"            "s1=1,det=8,cand=6,kappa=0"            "s1=1,det=12,cand=4,kappa=0"            "s1=1,det=12,cand=4,kappa=1"            "s1=1,det=12,cand=4,kappa=2.5"            "s1=1,det=12,cand=4,kappa=4"            "s1=1,det=16,cand=6,kappa=2,maxq=26"            "s1=1,det=16,cand=6,kappa=2,maxq=26,deadsearch=2"; do
  echo "{\"search\":\"$CFG\"}" >> "$OUT/E12-search.jsonl"
  ./fish match --a="$V6:legacy=1,$CFG" --b=v05 --games=120 --rotations=6 --seed=90210 --json >> "$OUT/E12-search.jsonl"
  echo >> "$OUT/E12-search.jsonl"
done

echo "== E15 the deliberate miss: what M1 deletes, and why unbanning it fails =="
{ echo "### unbanned dead asks with the ownership incentive gated (v05:m1=0,m1p=1) -- PATHOLOGY"
  ./fish pathology --a="v05:m1=0,m1p=1" --b="v05:m1=0,m1p=1" --games=200 --rotations=2 --seed=31
  echo; echo "### the same configuration's win rate against v0.5 -- HIGHER, and rejected on the KPIs"
  ./fish match --a="v05:m1=0,m1p=1" --b=v05 --games=200 --rotations=6 --seed=90210
  echo; echo "### v0.5 mirror, for reference"
  ./fish pathology --a=v05 --b=v05 --games=200 --rotations=2 --seed=31
} > "$OUT/E15-deliberate-miss.txt" 2>&1

echo "== E14 search deviation rate: the falsifier a healthy search must pass =="
{ for K in 0 1 2.5 4 6; do
    echo "### kappa=$K"
    ./fish v6probe --mode=search --a="$V6:legacy=1,s1=1,det=12,cand=4,kappa=$K" --b=v05 --games=20 --seed=90210
    echo
  done } > "$OUT/E14-searchdev.txt" 2>&1

echo "== E13 rollout-blueprint fidelity: the cost/fidelity dial the corpus had not swept =="
: > "$OUT/E13-rollout.jsonl"
for R in "v05:belief=indep,topk=0" "v05:topk=0,souter=1,sinner=1" "v05:topk=0,souter=1,sinner=3"          "v05:topk=0,souter=2,sinner=4" "v05:topk=0"; do
  echo "{\"rollout\":\"$R\"}" >> "$OUT/E13-rollout.jsonl"
  ./fish match --a="$R" --b=v05 --games=200 --rotations=6 --seed=90210 --json >> "$OUT/E13-rollout.jsonl"
  echo >> "$OUT/E13-rollout.jsonl"
  printf "{\"rolloutBench\":\"%s\",\"gps\":" "$R" >> "$OUT/E13-rollout.jsonl"
  ./fish bench --a="$R" --b="$R" --games=100 --threads=1 | sed 's/ games\/s.*//' | tr -d '\n' >> "$OUT/E13-rollout.jsonl"
  echo "}" >> "$OUT/E13-rollout.jsonl"
done

echo "== E11 partner regimes (owner decision D2): never headline the self-play row =="
: > "$OUT/E11-partners.jsonl"
for P in "" v03 detective withholder; do
  for A in "$V6" v05; do
    echo "{\"partners\":\"${P:-self}\",\"seat\":\"$A\"}" >> "$OUT/E11-partners.jsonl"
    ./fish match --a="$A" --b=v05 --partners="$P" --games=400 --rotations=2 --seed=313131 --json >> "$OUT/E11-partners.jsonl"
    echo >> "$OUT/E11-partners.jsonl"
  done
done

echo "== E9 throughput =="
{ for S in v04 v05 "$V6" "v05:topk=0" "v05:belief=indep,topk=0"; do
    printf "%-34s " "$S"; ./fish bench --a="$S" --b="$S" --games=200
  done } > "$OUT/E9-throughput.txt" 2>&1

echo "== E10 declaration pre-gate audit (dead for v0.5 until this study) =="
./fish gateaudit --a="$V6:gateaudit=1" --games=400 --rotations=6 --seed=90210 > "$OUT/E10-gateaudit.txt" 2>&1 || true

echo "done. artifacts in $OUT"
