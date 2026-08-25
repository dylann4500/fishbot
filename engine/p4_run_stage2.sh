#!/usr/bin/env bash
# phase 4 stage 2: waits for stage 1 to drain, then the partner/cross-play half
# and the adversary re-search.  Chained so the machine never idles and so no two
# batteries ever contend -- throughput numbers in this corpus are only meaningful
# on a quiet machine.
set -uo pipefail
cd "$(dirname "$0")"
while pgrep -f "p4_run_stage1b.sh" >/dev/null 2>&1; do sleep 30; done
while pgrep -f "fish7 (match|tune)" >/dev/null 2>&1; do sleep 30; done
echo "###### STAGE 2a: three independently-trained runs of the architecture  $(date)"
./p4_crossfit_v07.sh --runs=3 --gens=6 --pop=12 --deals=150 --sigmarel=0.12 --threads=13
echo "###### STAGE 2b: the partner-regime table and cross-play  $(date)"
./p4_partners_v07.sh --part=all --threads=13 --deals=6000
echo "###### STAGE 2c: adversary re-search against the improved policy  $(date)"
./p4_adversary_v07.sh --part=all --threads=13 --deals=6000
echo "###### STAGE 2 COMPLETE  $(date)"
