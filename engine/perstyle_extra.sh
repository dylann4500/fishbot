#!/usr/bin/env bash
# E4b: the per-style profile at two FURTHER held-out banks.  The single-bank E4
# table carries about +/-2.3 points per cell, which is wide enough that no
# individual cell is separated from zero; three banks is what the worst-case
# claim rests on.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v06/results}
V6=${V6:-v06}
mkdir -p "$OUT"
: > "$OUT/E4b-perstyle-banks.jsonl"
for S in 90210 424242; do
  for OPP in v05 v04 v03 v02 lockout detective diversifier hunter bluffer random silent feint withholder; do
    for A in "$V6" v05 v04; do
      echo "{\"bank\":$S}" >> "$OUT/E4b-perstyle-banks.jsonl"
      ./fish match --a="$A" --b="$OPP" --games=300 --rotations=6 --seed="$S" --threads="${THREADS:-0}" --json >> "$OUT/E4b-perstyle-banks.jsonl"
      echo >> "$OUT/E4b-perstyle-banks.jsonl"
    done
  done
done
echo "done"
