#!/usr/bin/env bash
# FishBot v0.7 phase 4 -- re-run adversary search against the IMPROVED policy.
#
# The phase-4 brief: "Iterate the surviving candidates against the mechanisms the
# exploiters revealed, not against the exploiters: after each fix, re-run adversary
# search against the improved policy and check whether the weakness closed or
# merely moved."
#
# So the target here is the v0.7 candidate, not v0.6, and the objective axis is
# aimed at the mechanisms phase 2 named rather than at the adversaries it built:
#   A3 the urgency-declaration complex   -> --kpi=declerr
#   A4 the 220-event cliff               -> --kpi=events   (the cliff is gone; is the stall rung reachable?)
#   L13 forced-endgame induction         -> --kpi=forced   (K2 raised the target's own incidence six-fold by deleting urgency)
#   L10 ask suppression                  -> --kpi=asksupp
# plus the two class controls and two independence axes (basin, step size).
#
# Fitting banks are disjoint from every evaluation bank and from each other, and
# nothing here evaluates anything: evaluation is the second phase of this script,
# on banks the fit never saw, because the win rate reached during fitting is a
# maximum over a population on shared seeds and is upward biased.
#
#   usage: ./p4_adversary_v07.sh [--part=fit|eval|all] [--target=SPEC] [ROW_ID ...]
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh

FISH=${FISH:-./fish7}
OUT=${OUT:-../research/v07/results}
RUNSD=${RUNSD:-../research/v07/runs}
THREADS=${THREADS:-13}
TARGET="$V07CAND"; TARGETID=v07cand
PART=all; DEALS=${DEALS:-6000}
WANT=""
for a in "$@"; do case "$a" in
  --part=*)   PART="${a#*=}" ;;
  --target=*) TARGET="${a#*=}" ;;
  --targetid=*) TARGETID="${a#*=}" ;;
  --threads=*) THREADS="${a#*=}" ;;
  --deals=*)  DEALS="${a#*=}" ;;
  --fish=*)   FISH="${a#*=}" ;;
  --*)        ;;
  *)          WANT="$WANT $a" ;;
esac; done
mkdir -p "$OUT" "$RUNSD"
FITART="$OUT/P4-advfits.jsonl"
EVALART="$OUT/P4-adveval.jsonl"

PANEL=$(echo "$TARGET" | tr ',' '+')

# ------------------------------------------------------------------- fitting
if [ "$PART" = "fit" ] || [ "$PART" = "all" ]; then
  grep -v '^#' p4_rows.tsv | grep -v '^[[:space:]]*$' |
  while IFS=$'\t' read -r ID CLASS BASE KPI SEED GENS POP D EXTRA HYP; do
    [ -z "${ID:-}" ] && continue
    if [ -n "$WANT" ] && ! echo " $WANT " | grep -q " $ID "; then continue; fi
    [ -s "$RUNSD/p4-$ID.spec" ] && { echo "skip $ID (already fitted)" >&2; continue; }
    [ "${EXTRA:-}" = "-" ] && EXTRA=""
    SEEDFLAG="--fromv6"
    case "${EXTRA:-}" in *NOFROMV6*) SEEDFLAG=""; EXTRA=$(echo "$EXTRA" | sed 's/NOFROMV6//') ;; esac
    echo "=== $ID  $CLASS base=$BASE kpi=$KPI seed=$SEED budget=${GENS}x${POP}x${D} ${EXTRA:-} ===" >&2
    T0=$(date +%s)
    W=$("$FISH" tune --panel="$PANEL" --base="$BASE" --full $SEEDFLAG --kpi="$KPI" \
          --games=$D --pop=$POP --elite=$(( POP / 3 + 1 )) --gens=$GENS \
          --beta=1 --sigmarel=0.08 --paired --obj=min --seed=$SEED --threads="$THREADS" \
          ${EXTRA:-} --out="$RUNSD/p4-$ID.jsonl" | tail -1 | sed 's/^weights=//')
    T1=$(date +%s)
    [ -z "$W" ] && { echo "  $ID FAILED" >&2; continue; }
    echo "$W" > "$RUNSD/p4-$ID.txt"
    case "$BASE" in *:*) SEP="," ;; *) SEP=":" ;; esac
    echo "${BASE}${SEP}allparams=${W}" > "$RUNSD/p4-$ID.spec"
    printf '{"battery":"P4advfit","id":"%s","class":"%s","base":"%s","target":"%s","targetSpec":"%s","kpi":"%s","fitSeed":%s,"gens":%s,"pop":%s,"fitDeals":%s,"fitGames":%s,"extra":"%s","hypothesis":"%s","seconds":%s}\n' \
      "$ID" "$CLASS" "$BASE" "$TARGETID" "$TARGET" "$KPI" "$SEED" "$GENS" "$POP" "$D" \
      "$(( GENS * POP * D * 2 ))" "${EXTRA:-}" "$HYP" "$(( T1 - T0 ))" >> "$FITART"
    echo "  $ID fitted in $(( T1 - T0 ))s" >&2
  done
fi

# ---------------------------------------------------------------- evaluation
if [ "$PART" = "eval" ] || [ "$PART" = "all" ]; then
  for f in "$RUNSD"/p4-Y*.spec; do
    [ -e "$f" ] || continue
    ID=$(basename "$f" .spec); ID="${ID#p4-}"
    if [ -n "$WANT" ] && ! echo " $WANT " | grep -q " $ID "; then continue; fi
    A=$(cat "$f")
    for bank in "${BANKS[@]}"; do
      # The adversary is arm A; the target is arm B.  The reported edge is the
      # ADVERSARY's, so a positive number is an exploit and a negative one is not.
      j=$("$FISH" match --a="$A" --b="$TARGET" --seed="$bank" --games="$DEALS" \
                        --rotations=2 --threads="$THREADS" --json 2>/dev/null | tail -1)
      [ -z "$j" ] && { echo "  !! $ID bank $bank FAILED" >&2; continue; }
      ID="$ID" A="$A" TGT="$TARGET" TID="$TARGETID" BANK="$bank" J="$j" python3 -c '
import json,os
d=json.loads(os.environ["J"])
print(json.dumps({"battery":"P4adveval","id":os.environ["ID"],
  "adversary":os.environ["A"],"target":os.environ["TID"],"targetSpec":os.environ["TGT"],
  "bank":int(os.environ["BANK"]),"deals":d["deals"],"games":d["games"],
  "advEdgePts":100*d["winRateA"]-50,"ci":[100*d["ci"][0]-50,100*d["ci"][1]-50],
  "targetDeclAcc":d["declAccB"],"targetAskAcc":d["askAccB"],
  "targetForcedPerGame":d.get("forcedPerGameA"),
  "eventsPerGame":d["eventsPerGame"],"limitHitRate":d["limitHitRate"],
  "hw98":d["power"]["halfWidth98Games"]}))' >> "$EVALART"
      echo "  eval $ID bank $bank" >&2
    done
  done
fi
echo "== adversary battery complete ==" >&2
