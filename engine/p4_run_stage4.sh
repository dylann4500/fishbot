#!/usr/bin/env bash
# phase 4 stage 4: re-run the whole gate table under the FINAL rule, so the table
# is one measurement rather than a mixture.  The first pass ran S6 at 13 threads
# under a single G7; section 4.3 shows that S6 above one thread is a lottery, and
# the gate now runs it at one thread under the split G7a/G7b rule the
# preregistration commits.
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh
while pgrep -f "p4_run_stage3.sh" >/dev/null 2>&1; do sleep 30; done
while pgrep -f "fish7 (match|tune|v7side)" >/dev/null 2>&1; do sleep 30; done
echo "###### STAGE 4: the gate table, re-run under the final rule  $(date)"
: > ../research/v07/results/P4-gate.jsonl
: > ../research/v07/results/P4-gate.txt
for e in "v06|$V06" "K3-stack|$K3STACK" "F-cheap|$FCHEAP" "K3-search|$K3SEARCH" \
         "P2-composite|$P2COMP" "FROZEN-v07|$V07CAND" "NEGCTL-m1off|$GATEFAIL"; do
  id="${e%%|*}"; sp="${e#*|}"
  ./gate_v07.sh --spec="$sp" --id="$id" --games=400 --threads=13 --s6games=400 \
    || echo "  ^^ $id FAILED THE GATE"
done
echo "###### STAGE 4 COMPLETE  $(date)"
