#!/usr/bin/env bash
# Freeze the fitted configuration and run the full evaluation battery.
set -uo pipefail
cd "$(dirname "$0")"
TRACE=${TRACE:-../research/v04/runs/tune-round3.jsonl}
LAST=${LAST:-6}
VAL_GAMES=${VAL_GAMES:-500}

echo "### selecting on the validation bank"
FISHBIN=./fish2 python3 select_final.py --trace "$TRACE" --last $LAST --games $VAL_GAMES \
  --seed 1357911 --rotations 2 --panel v03,lockout,detective,v02 | tee ../research/v04/runs/selection.log
PARAMS=$(grep '^allparams=' ../research/v04/runs/selection.log | tail -1 | sed 's/^allparams=//')
if [ -z "$PARAMS" ]; then echo "no params selected"; exit 1; fi

echo "### freezing into V04Config"
python3 freeze_config.py "$PARAMS"
clang++ -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -Wall -Wextra src/main.cpp -o fish -pthread || exit 1
cp fish fish2

echo "### sanity check"
./fish verify --games=200 | tail -3

echo "### experiment battery"
V04="v04:mgate=0.008" ./experiments.sh

echo "### exploitability probe"
V04="v04:mgate=0.008" ./exploitability.sh

echo "### tables"
python3 build_tables.py
echo "### finalize done"
