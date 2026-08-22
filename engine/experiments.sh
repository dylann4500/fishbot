#!/usr/bin/env bash
# FishBot v0.4 experiment battery.  Every command writes a JSON or text artifact
# under research/v04/results/, and engine/build_manifest.py records their
# checksums.  Seed banks are disjoint by design: fitting used base seeds
# 20260821 (round 1), 770077 (round 2, superseded), 313131 (round 3) and 888111
# (round 4); round 5's base seed was not captured in a committed script.  The
# shipping configuration was selected on validation bank 1357911.  Every seed
# below is disjoint from all of those, and the configuration was frozen before
# any of them ran.
set -uo pipefail
cd "$(dirname "$0")"
OUT=../research/v04/results
BIN=${BIN:-./fish}
V04=${V04:-v04}
THREADS=${THREADS:-0}
mkdir -p "$OUT"

say() { printf '\n=== %s ===\n' "$1"; }

# --- E1 engine and information-safety verification -------------------------
say "E1 verify"
$BIN verify --games=600 --threads=$THREADS > "$OUT/E1-verify.txt" 2>&1
$BIN verify --games=300 --legacy --threads=$THREADS > "$OUT/E1-verify-legacy.txt" 2>&1
cat "$OUT/E1-verify.txt"

# --- E2 belief-engine validation -------------------------------------------
say "E2 belief validation"
$BIN selftest --games=40 > "$OUT/E2-belief-selftest.txt" 2>&1
cat "$OUT/E2-belief-selftest.txt"

# --- E3 held-out head-to-head ----------------------------------------------
say "E3 head-to-head (held-out bank, 6-rotation duplicate)"
: > "$OUT/E3-headtohead.jsonl"
for opp in v03 lockout detective v02 diversifier hunter bluffer random; do
  echo "  vs $opp"
  $BIN match --a="$V04" --b=$opp --games=700 --rotations=6 --seed=90210 --json --threads=$THREADS \
    >> "$OUT/E3-headtohead.jsonl"
  echo >> "$OUT/E3-headtohead.jsonl"
done

# --- E4 round-robin matrix --------------------------------------------------
say "E4 population matrix"
$BIN matrix --policies="$V04",v03,v02,lockout,detective,diversifier,hunter,bluffer,random \
  --games=200 --rotations=6 --seed=515151 --threads=$THREADS > "$OUT/E4-matrix.json" 2>&1

# --- E5 paired ablations ----------------------------------------------------
say "E5 ablations"
$BIN ablate --ref="$V04" --games=500 --rotations=2 --seed=606060 --threads=$THREADS \
  --panel=v03,lockout,detective,v02 \
  --variants="$V04,belief=block;$V04,belief=fast,ptheta=0,pphi=0;$V04,belief=exact;$V04,belief=sinkhorn;$V04,belief=indep;$V04,ptheta=0,pphi=0;$V04,value=0;$V04,vdecl=0;$V04,patient=0;$V04,topk=0;$V04,gmap=1;$V04,w0=0;$V04,w5=0;$V04,w8=0;$V04,w18=0;$V04,w9=0,w19=0;$V04,decl=0.80;$V04,decl=0.99" \
  > "$OUT/E5-ablations.json" 2>&1

# --- E6 calibration ---------------------------------------------------------
say "E6 calibration"
$BIN calibrate --a="$V04" --b=v03 --games=600 --seed=717171 > "$OUT/E6-calibration-v04.txt" 2>&1
$BIN calibrate --a=v03 --b="$V04" --games=600 --seed=717171 > "$OUT/E6-calibration-v03.txt" 2>&1
cat "$OUT/E6-calibration-v04.txt" | head -3

# --- E7 rule-variant robustness ---------------------------------------------
say "E7 rule variants"
: > "$OUT/E7-rules.jsonl"
for opp in v03 lockout detective; do
  $BIN match --a="$V04" --b=$opp --games=400 --rotations=6 --seed=828282 --legacy --json --threads=$THREADS >> "$OUT/E7-rules.jsonl"; echo >> "$OUT/E7-rules.jsonl"
  $BIN match --a="$V04" --b=$opp --games=400 --rotations=6 --seed=838383 --sets=8 --json --threads=$THREADS >> "$OUT/E7-rules.jsonl"; echo >> "$OUT/E7-rules.jsonl"
  $BIN match --a="$V04" --b=$opp --games=400 --rotations=6 --seed=848484 --no-out-of-turn --json --threads=$THREADS >> "$OUT/E7-rules.jsonl"; echo >> "$OUT/E7-rules.jsonl"
done

# --- E8 v0.3 port validation ------------------------------------------------
say "E8 v0.3 port validation under legacy rules"
: > "$OUT/E8-v03-port.jsonl"
for opp in lockout detective v02 diversifier hunter bluffer random; do
  $BIN match --a=v03 --b=$opp --games=1000 --rotations=2 --seed=20260820 --legacy --json --threads=$THREADS >> "$OUT/E8-v03-port.jsonl"; echo >> "$OUT/E8-v03-port.jsonl"
done

# --- E9 timing ---------------------------------------------------------------
say "E9 throughput"
{
  printf 'v0.4 self-play      '; $BIN bench --a="$V04" --b="$V04" --games=200 --threads=$THREADS
  printf 'v0.4 exact posterior '; $BIN bench --a="$V04,belief=block" --b="$V04,belief=block" --games=60 --threads=$THREADS
  printf 'v0.3 self-play      '; $BIN bench --a=v03 --b=v03 --games=400 --threads=$THREADS
} > "$OUT/E9-throughput.txt" 2>&1
cat "$OUT/E9-throughput.txt"

# --- E12 does belief exactness matter to an UNFITTED policy? --------------
# The shipped weights were fitted against the fast posterior, so comparing the
# two beliefs under those weights is not symmetric.  Repeat the comparison with
# a deliberately unfitted policy -- pure posterior-greedy asking, no value
# lookahead, no two-ply refinement -- so neither belief has been fitted to.
say "E12 exactness under an unfitted policy"
GREEDY="weights=12|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0,value=0,topk=0,ptheta=0,pphi=0"
{
  printf 'greedy + fast  posterior vs v03: '
  $BIN match --a="v04:$GREEDY,belief=fast"  --b=v03 --games=400 --rotations=6 --seed=959595 --json --threads=$THREADS
  printf '\ngreedy + block posterior vs v03: '
  $BIN match --a="v04:$GREEDY,belief=block" --b=v03 --games=400 --rotations=6 --seed=959595 --json --threads=$THREADS
  printf '\n'
} > "$OUT/E12-exactness.txt" 2>&1
cat "$OUT/E12-exactness.txt" | cut -c1-140

# --- E13 termination incidence across the population ----------------------
say "E13 termination"
: > "$OUT/E13-termination.jsonl"
for opp in "$V04" v03 lockout detective v02 diversifier hunter bluffer random; do
  $BIN match --a="$V04" --b="$opp" --games=300 --rotations=6 --seed=464646 --json --threads=$THREADS >> "$OUT/E13-termination.jsonl"
  echo >> "$OUT/E13-termination.jsonl"
done
python3 - <<'PYEOF' >> "$OUT/E13-termination.jsonl"
import json
rows=[json.loads(l) for l in open("../research/v04/results/E13-termination.jsonl") if l.strip()]
print("# limit-hit rate by opponent:", {r["b"].split(":")[0]: round(r["limitHitRate"],5) for r in rows})
PYEOF
tail -1 "$OUT/E13-termination.jsonl"

# --- E14 value-function fit ------------------------------------------------
say "E14 value function"
$BIN fitvalue --a="$V04" --b="$V04" --games=250 --seed=31415 > "$OUT/E14-valuefit.txt" 2> "$OUT/E14-valuefit-stats.txt"
cat "$OUT/E14-valuefit-stats.txt"

# --- E15 brute-force oracle for the exact reference engine -----------------
# Exhaustive enumeration of the posterior on small reachable states, compared
# against the block engine's Z, marginals, team-ownership and full allocation
# probabilities, plus the sampler's frequencies.  Coverage (states skipped as
# too large) is reported alongside the result.
say "E15 brute-force oracle"
$BIN oracle --games=150 --maxdeals=200000 --samples=3000 > "$OUT/E15-oracle.txt" 2>&1
cat "$OUT/E15-oracle.txt"

# --- E16 declaration pre-gate false-negative audit -------------------------
# The two cheap gates that screen declaration candidates are not proved to be
# upper bounds on the full evaluation.  Re-run the full evaluation on every
# half-suit they reject, over the primary evaluation bank, and count how often
# the full rule would have declared one.
say "E16 declaration pre-gate audit"
$BIN gateaudit --games=700 --rotations=6 --seed=90210 \
  --panel=v03,lockout,detective,v02,diversifier,hunter,bluffer,random \
  > "$OUT/E16-gateaudit.txt" 2>&1
cat "$OUT/E16-gateaudit.txt"

# --- E17 simultaneous-declaration arbitration sensitivity ------------------
# Arbitration between simultaneous voluntary declarations is a modelling choice,
# not a rule.  Replay the primary bank under three orders.
say "E17 declaration arbitration"
: > "$OUT/E17-arbitration.jsonl"
for arb in low high turn; do
  for opp in v03 lockout detective; do
    $BIN match --a="$V04" --b=$opp --games=700 --rotations=6 --seed=90210 --arb=$arb --json --threads=$THREADS \
      | sed "s/^{/{\"arb\":\"$arb\",/" >> "$OUT/E17-arbitration.jsonl"
    echo >> "$OUT/E17-arbitration.jsonl"
  done
done

echo "done"
