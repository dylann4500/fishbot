#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "$0")"
cp fish4 fish; cp fish4 fish2
echo "### selecting on the validation bank"
FISHBIN=./fish2 python3 select_final.py --trace ../research/v04/runs/tune-round5.jsonl --last 6 \
  --games 500 --seed 1357911 --rotations 2 --panel v03,lockout,detective,v02 \
  | tee ../research/v04/runs/selection.log
PARAMS=$(grep '^allparams=' ../research/v04/runs/selection.log | tail -1 | sed 's/^allparams=//')
[ -z "$PARAMS" ] && { echo "no params"; exit 1; }
echo "### freezing"
python3 freeze_config.py "$PARAMS"
clang++ -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -Wall -Wextra src/main.cpp -o fish -pthread || exit 1
cp fish fish2; cp fish fish4
echo "### round-trip check (allparams must reproduce the frozen defaults)"
./fish match --a="v04:allparams=$PARAMS" --b="v04:mgate=0.008" --games=120 --seed=5150 --json | python3 -c "
import sys,json;d=json.load(sys.stdin);print('  self-match win rate',round(d['winRateA'],4),'(0.5 == identical policies)')"
./fish verify --games=200 | tail -3
V04="v04:mgate=0.008" ./experiments.sh
V04="v04:mgate=0.008" ./exploitability.sh
python3 build_tables.py
mkdir -p ../output/pdf
( cd ../paper && python3 inline.py && \
  tectonic -X compile fishbot_v04.tex --outdir ../output/pdf && \
  tectonic -X compile fishbot_v04_standalone.tex --outdir ../output/pdf ) 2>&1 | tail -3
echo "### finalize4 done"
