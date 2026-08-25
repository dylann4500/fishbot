#!/usr/bin/env bash
# phase 4 stage 3: the common profile for the FROZEN configuration and for the
# phase-2 composite, on the SAME panel phase 3 scored A0-v06 and K3-stack on.
# Worst case and minimax regret are only defined over a shared panel, and this is
# what makes the phase-4 arm comparable with the phase-3 ones.
set -uo pipefail
cd "$(dirname "$0")"
while pgrep -f "p4_run_stage2.sh" >/dev/null 2>&1; do sleep 30; done
while pgrep -f "fish7 (match|tune|v7side)" >/dev/null 2>&1; do sleep 30; done
echo "###### STAGE 3: the common profile, FROZEN and P2-composite  $(date)"
./candidates_v07.sh --threads=13 --arms=FROZEN,P2-composite
echo "###### STAGE 3 COMPLETE  $(date)"
