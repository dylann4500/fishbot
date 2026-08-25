#!/usr/bin/env bash
# phase 4 stage 5: re-run the cross-play half against the ACTUAL frozen
# architecture.  The first pass (research/v07/runs/pre-fix/, P4-*-prefix.jsonl)
# was fitted before the factory.hpp ordering fix, so its three runs silently
# carried the urgency escalation ON -- see RESEARCH-LOG 4.8.  With an explicit key
# now beating allparams=, the emitted specs mean what they say.
set -uo pipefail
cd "$(dirname "$0")"
echo "###### STAGE 5: cross-play, re-fitted against the frozen architecture  $(date)"
./p4_crossfit_v07.sh --runs=3 --gens=6 --pop=12 --deals=150 --sigmarel=0.12 --threads=13
echo "###### STAGE 5b: the cross-play matrix  $(date)"
./p4_partners_v07.sh --part=cross --threads=13 --deals=6000
echo "###### STAGE 5 COMPLETE  $(date)"
