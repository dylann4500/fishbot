#!/usr/bin/env bash
# phase 4 stage 1: gate every configuration, then the replication, then the lattice.
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh
echo "###### STAGE 1a: the commit gate, before any strength number  $(date)"
# Rule 1 is ENFORCED here, not merely asserted in the log.  The first run of this
# phase printed "^^ FAILED THE GATE" and carried straight on into the strength
# battery, which is exactly the failure the rule exists to prevent; the battery
# was killed by hand and only restarted after the gate question was settled.
# NEGCTL-m1off is the negative control and is REQUIRED to fail, so it is excluded
# from the abort and its verdict is asserted the other way round.
FAILED=""
for e in "v07cand|$V07CAND" "P2-composite|$P2COMP" "F-cheap|$FCHEAP" "K3-search|$K3SEARCH"; do
  id="${e%%|*}"; sp="${e#*|}"
  ./gate_v07.sh --spec="$sp" --id="$id" --games=400 --threads=7 || FAILED="$FAILED $id"
done
if ./gate_v07.sh --spec="$GATEFAIL" --id=NEGCTL-m1off --games=400 --threads=7; then
  echo "!! THE NEGATIVE CONTROL PASSED THE GATE.  The gate is broken; nothing below is a measurement." >&2
  exit 3
fi
if [ -n "$FAILED" ]; then
  echo "!! GATE FAILURES:$FAILED -- STOPPING.  A configuration that has not passed the gate is not" >&2
  echo "!! scored, and not scored-with-a-caveat.  Settle the gate question, then re-run." >&2
  exit 2
fi
echo "###### STAGE 1b: replication  $(date)"
./p4_strength_v07.sh --part=repl --threads=7
echo "###### STAGE 1c: attribution lattice  $(date)"
./p4_strength_v07.sh --part=lattice --threads=7
echo "###### STAGE 1 COMPLETE  $(date)"
