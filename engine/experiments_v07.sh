#!/usr/bin/env bash
# FishBot v0.7 -- phase 1 instrument battery.
#
# ORDERING IS THE POINT.  Step 0 is a GATE, not a measurement.  The v0.5 study's
# own ablation table contains configurations that score six points higher than
# the shipped policy while carrying a 373-ask dead run and killing 14% of games
# at the action limit, so a battery ordered by win rate first selects the broken
# policy (experiments_v06.sh header; ledger C14).  v0.7 adds two gates the v0.6
# battery could not have: the reserved-seed registry, and the identity controls
# for the new classes -- `v07` with its twelve responder coordinates at zero, and
# `v07i` with inversion off, must both be v0.6 BIT FOR BIT.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7}
mkdir -p "$OUT"

echo "== G0 reserved-seed registry (must pass) =="
{ $BIN seeds; } > "$OUT/G0-seeds.txt" 2>&1
tail -3 "$OUT/G0-seeds.txt"

echo "== G1 identity controls (must pass) =="
{
  echo "### v06 carrying v05's vector with every switch off must be v05 BIT FOR BIT"
  A=$($BIN pathology --a="v06:legacy=1" --b="v06:legacy=1" --games=60 --seed=31 | tail -n +2 | md5)
  B=$($BIN pathology --a=v05 --b=v05 --games=60 --seed=31 | tail -n +2 | md5)
  echo "v06-off md5 $A"; echo "v05     md5 $B"
  [ "$A" = "$B" ] && echo "V05/V06 IDENTITY PASS" || echo "V05/V06 IDENTITY FAIL"
  echo
  echo "### v07 with its twelve responder coordinates at zero must be v06 BIT FOR BIT"
  C=$($BIN pathology --a=v07 --b=v07 --games=60 --seed=31 | tail -n +2 | md5)
  D=$($BIN pathology --a=v06 --b=v06 --games=60 --seed=31 | tail -n +2 | md5)
  echo "v07-zero md5 $C"; echo "v06      md5 $D"
  [ "$C" = "$D" ] && echo "V07 IDENTITY PASS" || echo "V07 IDENTITY FAIL"
  echo
  echo "### v07i with inversion off must be v06 BIT FOR BIT"
  E=$($BIN pathology --a="v07i:inv=0" --b="v07i:inv=0" --games=60 --seed=31 | tail -n +2 | md5)
  echo "v07i-off md5 $E"; echo "v06      md5 $D"
  [ "$E" = "$D" ] && echo "V07I IDENTITY PASS" || echo "V07I IDENTITY FAIL"
  echo
  echo "### the leaf-evaluator refactor must reproduce v0.6's leafValue exactly"
  echo "### (checked against the pre-refactor binary in RESEARCH-LOG.md; here the"
  echo "###  truncated search must be reproducible run to run)"
  # The timing fields are wall-clock and differ run to run by construction, so
  # they are stripped before the digest.  The first version of this gate hashed
  # the raw JSON and reported FAIL on a perfectly deterministic engine.
  strip () { python3 -c "
import json,sys
d=json.load(sys.stdin)
for k in ('seconds','gamesPerSec','threads','power'): d.pop(k,None)
print(json.dumps(d,sort_keys=True))"; }
  F=$($BIN match --a="v06:s1=1,det=12,cand=4,kappa=2.5,depth=24" --b=v06 --games=12 --seed=90210 --json | strip | md5)
  G=$($BIN match --a="v06:s1=1,det=12,cand=4,kappa=2.5,depth=24" --b=v06 --games=12 --seed=90210 --json | strip | md5)
  [ "$F" = "$G" ] && echo "TRUNCATION DETERMINISM PASS" || echo "TRUNCATION DETERMINISM FAIL"
  echo
  echo "### sharding must partition a bank exactly"
  H=$($BIN match --a=v06 --b=v05 --games=120 --seed=7010001 --json | python3 -c "import json,sys;d=json.load(sys.stdin);print(d['winRateA'],d['deals'])")
  S0=$($BIN match --a=v06 --b=v05 --games=120 --seed=7010001 --shard=0/3 --json | python3 -c "import json,sys;d=json.load(sys.stdin);print(int(round(d['winRateA']*d['deals']*2)),d['deals'])")
  S1=$($BIN match --a=v06 --b=v05 --games=120 --seed=7010001 --shard=1/3 --json | python3 -c "import json,sys;d=json.load(sys.stdin);print(int(round(d['winRateA']*d['deals']*2)),d['deals'])")
  S2=$($BIN match --a=v06 --b=v05 --games=120 --seed=7010001 --shard=2/3 --json | python3 -c "import json,sys;d=json.load(sys.stdin);print(int(round(d['winRateA']*d['deals']*2)),d['deals'])")
  echo "whole=$H  shards: $S0 | $S1 | $S2"
  python3 - "$H" "$S0" "$S1" "$S2" <<'PY'
import sys
# winRateA is printed to six significant figures, so reconstructing the whole
# run's win COUNT from it needs rounding, not an exact float comparison: the
# first version of this gate tested |128 - 127.99992| < 1e-6 and reported FAIL
# on an exact partition.
w, dl = sys.argv[1].split(); w = float(w); dl = int(dl)
whole = round(w * dl * 2)
tot = deals = 0
for a in sys.argv[2:]:
    k, d = a.split(); tot += int(k); deals += int(d)
ok = (deals == dl) and (tot == whole)
print(f"  whole: {whole} wins over {dl} deals;  shards: {tot} wins over {deals} deals")
print("SHARD PARTITION PASS" if ok else "SHARD PARTITION FAIL")
PY
} > "$OUT/G1-identity.txt" 2>&1
grep -E "PASS|FAIL" "$OUT/G1-identity.txt"

echo "== G2 pathology KPIs for every class that will be measured =="
{ for S in v06 v07 "v07i:idet=48"; do
    echo "### $S mirror"; $BIN pathology --a="$S" --b="$S" --games=200 --rotations=2 --seed=31; echo
  done } > "$OUT/G2-pathology.txt" 2>&1
grep -E "dead|limit" "$OUT/G2-pathology.txt" | head -12

echo "== T1 throughput, replacing E9 (both bases, never mixed) =="
echo "  (run separately by /tmp/through.sh on a quiet machine; see T1-throughput.jsonl)"

echo "== D1 per-decision channel =="
: > "$OUT/D1-decisions.jsonl"
for S in 7011001 7011002; do
  for A in v06 v05; do
    $BIN v7decide --a="$A" --b=v06 --games=400 --rotations=2 --seed=$S --json >> "$OUT/D1-decisions.jsonl"
  done
done
$BIN v7decide --a=v06 --b=v06 --games=400 --rotations=2 --seed=7011001 \
  --dump="$OUT/D1-records-v06.csv" > "$OUT/D1-decisions.txt" 2>&1
tail -12 "$OUT/D1-decisions.txt"

echo "== W1 transcript inversion: bits, and whether bits become a better posterior =="
: > "$OUT/W1-inversion.jsonl"
for S in 7012001 7012002; do
  for T in v06 v05; do
    $BIN v7bits --a=v06 --b="$T" --games=120 --det=64 --seed=$S --gain=1.0 --focus=2 --json >> "$OUT/W1-inversion.jsonl"
  done
done
$BIN v7bits --a=v06 --b=v06 --games=120 --det=64 --seed=7012001 --gain=1.0 --focus=2 > "$OUT/W1-inversion.txt" 2>&1
cat "$OUT/W1-inversion.txt"

echo "done."
