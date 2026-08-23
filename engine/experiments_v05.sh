#!/usr/bin/env bash
# FishBot v0.5 experiment battery.  Every number in paper/numbers_v05.tex traces
# to one of these artifacts.  Run from engine/ after `make`.
set -euo pipefail
cd "$(dirname "$0")"
OUT=../research/v05/results
mkdir -p "$OUT"

echo "== E1 verify: rules, information safety, belief soundness =="
./fish verify --games=600                     > "$OUT/E1-verify.txt"

echo "== E2 pathology: the KPIs that gate a v0.5 commit =="
{ echo "### v0.5 mirror"; ./fish pathology --a=v05 --b=v05 --games=300 --rotations=2 --seed=31
  echo; echo "### v0.4 mirror (reference)"; ./fish pathology --a=v04 --b=v04 --games=300 --rotations=2 --seed=31
  echo; echo "### v0.5 vs v0.4"; ./fish pathology --a=v05 --b=v04 --games=300 --rotations=2 --seed=90210
} > "$OUT/E2-pathology.txt"

echo "== E3 head-to-head, held-out seeds =="
: > "$OUT/E3-headtohead.jsonl"
for S in 90210 31337 515151 777001 424242; do
  ./fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed="$S" --json >> "$OUT/E3-headtohead.jsonl"
  echo >> "$OUT/E3-headtohead.jsonl"
done

echo "== E4 per-opponent profile and worst case (the headline is the MINIMUM) =="
: > "$OUT/E4-perstyle.jsonl"
for OPP in v04 v03 v02 lockout detective diversifier hunter bluffer random; do
  ./fish match --a=v05 --b="$OPP" --games=300 --rotations=6 --seed=515253 --json >> "$OUT/E4-perstyle.jsonl"
  echo >> "$OUT/E4-perstyle.jsonl"
  ./fish match --a=v04 --b="$OPP" --games=300 --rotations=6 --seed=515253 --json >> "$OUT/E4-perstyle.jsonl"
  echo >> "$OUT/E4-perstyle.jsonl"
done

echo "== E5 mechanism ablations (single mechanism, factorial core) =="
: > "$OUT/E5-ablations.jsonl"
for SPEC in "v05:m1=0,m2=0,stage2=1" "v05:m1=1,m2=0,stage2=1" "v05:m1=0,m2=1,stage2=1" \
            "v05:m1=0,m2=0,stage2=0" "v05:m1=1,m2=1,stage2=1" "v05:m1=1,m2=0,stage2=0" \
            "v05:m1=0,m2=1,stage2=0" "v05" "v05:m1p=1" "v05:norepeat=1"; do
  echo "{\"spec\":\"$SPEC\"}" >> "$OUT/E5-ablations.jsonl"
  ./fish match --a="$SPEC" --b=v04 --games=250 --rotations=6 --seed=606060 --json >> "$OUT/E5-ablations.jsonl"
  echo >> "$OUT/E5-ablations.jsonl"
done

echo "== E6 calibration =="
./fish calibrate --a=v05 --b=v04 --games=400 --seed=717171 > "$OUT/E6-calibration-v05.txt" 2>&1 || true

echo "== E7 rule dialects =="
: > "$OUT/E7-rules.jsonl"
for FLAG in "" "--no-out-of-turn" "--no-cardless-declare" "--legacy"; do
  echo "{\"dialect\":\"${FLAG:-default}\"}" >> "$OUT/E7-rules.jsonl"
  ./fish match --a=v05 --b=v04 --games=250 --rotations=6 --seed=828282 $FLAG --json >> "$OUT/E7-rules.jsonl"
  echo >> "$OUT/E7-rules.jsonl"
done

echo "== E8 forced-endgame accuracy at volume (a rare event: ~0.007/game) =="
{ echo "### v0.5"; ./fish match --a=v05 --b=v04 --games=4000 --rotations=6 --seed=909090 | grep -E 'forced decls|declarations'
  echo "### v0.4"; ./fish match --a=v04 --b=v05 --games=4000 --rotations=6 --seed=909090 | grep -E 'forced decls|declarations'
} > "$OUT/E8-forced-endgame.txt"

echo "== E9 throughput =="
./fish bench --a=v05 --b=v05 --games=300 > "$OUT/E9-throughput.txt"

echo "done. artifacts in $OUT"
