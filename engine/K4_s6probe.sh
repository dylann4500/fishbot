#!/bin/zsh
# K4 side probe: the ONE irreproducible F-cheap decision on bank 7030002.
# The phase brief flags it as "almost certainly a determinization-order or
# accumulation artifact rather than a channel" and says finding out is worth ten
# minutes because it bears on every search number in the corpus.
# S6 only.  Two runs: the reported cell, and the same cell at ONE search node
# (--s3nodes only affects S3, so the second run is a straight repeat -- if the
# failure is a genuine function of (own hand, public stream, reset seed) it must
# recur at the same count; if it is a race or an accumulation artifact it need
# not).
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
OUT="$RES/K4-s6probe.txt"
FCHEAP="v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
: > "$OUT"
for RUN in 1 2; do
  echo "=== F-cheap S6, bank 7030002, run $RUN, threads=2 ===" >> "$OUT"
  ./fish7 v7side --a="$FCHEAP" --b=v06 --games=400 --seed=7030002 \
      --tests=s6 --threads=2 >> "$OUT" 2>&1
  echo "EXIT=$?" >> "$OUT"
done
echo "=== F-cheap S6, bank 7030002, run 3, threads=1 (serial: kills any race) ===" >> "$OUT"
./fish7 v7side --a="$FCHEAP" --b=v06 --games=400 --seed=7030002 \
    --tests=s6 --threads=1 >> "$OUT" 2>&1
echo "EXIT=$?" >> "$OUT"
echo "=== F-cheap S6, bank 7030001 control, threads=2 ===" >> "$OUT"
./fish7 v7side --a="$FCHEAP" --b=v06 --games=400 --seed=7030001 \
    --tests=s6 --threads=2 >> "$OUT" 2>&1
echo "EXIT=$?" >> "$OUT"
echo S6PROBEDONE
