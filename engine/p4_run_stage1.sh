#!/usr/bin/env bash
# phase 4 stage 1: gate every configuration, then the replication, then the lattice.
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh
echo "###### STAGE 1a: the commit gate, before any strength number  $(date)"
for e in "v07cand|$V07CAND" "P2-composite|$P2COMP" "F-cheap|$FCHEAP" "K3-search|$K3SEARCH" "NEGCTL-m1off|$GATEFAIL"; do
  id="${e%%|*}"; sp="${e#*|}"
  ./gate_v07.sh --spec="$sp" --id="$id" --games=400 --threads=7 || echo "  ^^ $id FAILED THE GATE"
done
echo "###### STAGE 1b: replication  $(date)"
./p4_strength_v07.sh --part=repl --threads=7
echo "###### STAGE 1c: attribution lattice  $(date)"
./p4_strength_v07.sh --part=lattice --threads=7
echo "###### STAGE 1 COMPLETE  $(date)"
