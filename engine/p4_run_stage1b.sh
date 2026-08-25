#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "$0")"
echo "###### STAGE 1b: replication  $(date)"
./p4_strength_v07.sh --part=repl --threads=13
echo "###### STAGE 1c: attribution lattice  $(date)"
./p4_strength_v07.sh --part=lattice --threads=13
echo "###### STAGE 1 STRENGTH COMPLETE  $(date)"
