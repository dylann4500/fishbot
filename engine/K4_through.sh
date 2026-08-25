#!/bin/zsh
# K4 throughput.  ONE short dedicated run, at a stated thread count.  Every other
# K4 measurement in this session ran at --threads=2 under the phase's CPU
# discipline and must not be quoted as a throughput headline.
cd "$(dirname "$0")"
RES="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
W="$RES/K4-weights"
T=${1:-8}
OUT="$RES/K4-throughput.jsonl"
: > "$OUT"
for ARM in v06 K4-fit-s1-win K4-fit-s1-selfdecl; do
  case "$ARM" in
    v06) SPEC="v06" ;;
    *)   SPEC="v07:allparams=$(cat "$W/$ARM.txt")" ;;
  esac
  R=$(./fish7 match --a="$SPEC" --b=v06 --seed=7030004 --games=1000 --rotations=2 --threads=$T --json)
  echo "{\"arm\":\"$ARM\",\"threads\":$T,\"r\":$R}" >> "$OUT"
  echo "$R" | python3 -c "import sys,json;d=json.load(sys.stdin);print('$ARM: %.1f games/s at %d threads (n=%d)'%(d['gamesPerSec'],d['threads'],d['games']))"
done
echo THROUGHDONE
