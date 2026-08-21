#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "$0")"
./fish verify --games=200 | tail -3
V04="v04:mgate=0.008" ./experiments.sh
V04="v04:mgate=0.008" ./exploitability.sh
python3 build_tables.py
mkdir -p ../output/pdf
( cd ../paper && python3 inline.py && \
  tectonic -X compile fishbot_v04.tex --outdir ../output/pdf && \
  tectonic -X compile fishbot_v04_standalone.tex --outdir ../output/pdf ) 2>&1 | tail -3
echo "### finalize3 done"
