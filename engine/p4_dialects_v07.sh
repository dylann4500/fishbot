#!/usr/bin/env bash
# FishBot v0.7 phase 4 -- the rule-dialect table.
#
# Ledger P-8: paper/tables_v06/dialects.tex has four rows at n = 1,500 (+-2.53),
# and `--legacy` changes FOUR things at once -- out-of-turn declaration off,
# cardless declaration off, maxAsks 400 -> 360, and the forced-endgame
# willingness ladder from eight rungs to two -- so it perturbs the ladder and the
# cap and attributes nothing.  Genuinely unswept by any row: the arbitration
# order (`--arb`), the deck size (`--sets`), and the action cap on its own.
#
# This table keeps the four v0.6 rows at 16x their power, UNBUNDLES `--legacy`,
# and adds the three axes never swept.  Both arms play the same dialect, so the
# cell is "does the v0.7 edge over v0.6 survive this reading of the rules".
#
#   usage: ./p4_dialects_v07.sh [--deals=6000] [--threads=N]
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh

FISH=${FISH:-./fish7}
OUT=${OUT:-../research/v07/results}
THREADS=${THREADS:-13}
DEALS=${DEALS:-6000}
for a in "$@"; do case "$a" in
  --deals=*) DEALS="${a#*=}" ;; --threads=*) THREADS="${a#*=}" ;;
  --out=*) OUT="${a#*=}" ;; --fish=*) FISH="${a#*=}" ;;
esac; done
F="$OUT/P4-dialects.jsonl"; : > "$F"

# id | flags.  The willingness ladder has no CLI flag of its own -- it is
# reachable only through `--legacy`, which is exactly P-8's complaint.  It is
# therefore not given a row rather than faked: the three isolable components of
# `--legacy` are rows 2, 3 and 4, and the residual (legacy minus those three) is
# the ladder, reported as a residual.
DIALECTS=(
  "default|"
  "no-out-of-turn|--no-out-of-turn"
  "no-cardless-declare|--no-cardless-declare"
  "maxasks-360|--maxasks=360"
  "arb-high|--arb=high"
  "arb-turn|--arb=turn"
  "sets-8|--sets=8"
  "legacy|--legacy"
)

for entry in "${DIALECTS[@]}"; do
  ID="${entry%%|*}"; FLAGS="${entry#*|}"
  for bank in "${BANKS[@]}"; do
    j=$("$FISH" match --a="$V07CAND" --b="$V06" --seed="$bank" --games="$DEALS" \
                      --rotations=2 --threads="$THREADS" $FLAGS --json 2>/dev/null | tail -1)
    if [ -z "$j" ]; then
      printf '{"dialect":"%s","bank":%s,"error":"no output"}\n' "$ID" "$bank" >> "$F"
      echo "  !! dialect $ID bank $bank FAILED" >&2; continue
    fi
    ID="$ID" FLAGS="$FLAGS" BANK="$bank" J="$j" python3 -c '
import json,os
d=json.loads(os.environ["J"])
print(json.dumps({"battery":"P4dialect","dialect":os.environ["ID"],
  "flags":os.environ["FLAGS"],"bank":int(os.environ["BANK"]),
  "a":d["a"],"b":d["b"],"deals":d["deals"],"games":d["games"],
  "edgePts":100*d["winRateA"]-50,"ci":[100*d["ci"][0]-50,100*d["ci"][1]-50],
  "declAccA":d["declAccA"],"declAccB":d["declAccB"],
  "askAccA":d["askAccA"],"askAccB":d["askAccB"],
  "eventsPerGame":d["eventsPerGame"],"limitHitRate":d["limitHitRate"],
  "hw98":d["power"]["halfWidth98Games"]}))' >> "$F"
    echo "  dialect $ID bank $bank" >&2
  done
done
echo "== dialect table complete -> $F ==" >&2
