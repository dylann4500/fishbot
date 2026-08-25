#!/usr/bin/env bash
# FishBot v0.7 phase 4 -- the partner-regime table and cross-play.
#
# The phase-4 brief: "Check for brittle self-play conventions against the corpus's
# own baseline: v0.6 gains +2.25 in self-play and -0.8 to +1.4 under partner
# change.  For every survivor, run the partner-regime table (copies of itself /
# previous versions / archetype partners) and cross-play between independently-
# trained runs of the same architecture."
#
# DESIGN.  Ledger L6's table (paper/tables_v06/partners.tex, engine/experiments_v06.sh:125-131)
# is `match --a=$ARM --b=v05 --partners=$P`: team A is [ARM, P, P] and team B is
# three copies of v0.5.  It ran at 800 games a cell, half-width +-3.46, at which
# "not one of the four deltas is separated from any other".  L6's own cheapest
# experiment is to re-run at 18,000 games a cell.  This does that and adds v0.7.
#
# Cell size: 6,000 deals x 2 rotations x 2 banks = 24,000 games, pooled
# half-width 98/sqrt(24000) = 0.63 -- five and a half times L6's resolution.
#
#   usage: ./p4_partners_v07.sh [--part=table|cross|all] [--threads=N]
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh

FISH=${FISH:-./fish7}
OUT=${OUT:-../research/v07/results}
THREADS=${THREADS:-13}
DEALS=${DEALS:-6000}
PART=all
XPDIR=${XPDIR:-../research/v07/runs}
for a in "$@"; do case "$a" in
  --part=*) PART="${a#*=}" ;; --threads=*) THREADS="${a#*=}" ;;
  --deals=*) DEALS="${a#*=}" ;; --out=*) OUT="${a#*=}" ;; --fish=*) FISH="${a#*=}" ;;
esac; done

pcell () {  # $1 label  $2 armid  $3 armspec  $4 partnerid  $5 partnerspec  $6 deals $7 file $8 bspec
  local lab="$1" armid="$2" arm="$3" pid="$4" p="$5" D="$6" F="$7" B="$8" bank j
  for bank in "${BANKS[@]}"; do
    j=$("$FISH" match --a="$arm" --b="$B" --partners="$p" --seed="$bank" \
                      --games="$D" --rotations=2 --threads="$THREADS" --json 2>/dev/null | tail -1)
    if [ -z "$j" ]; then
      printf '{"label":"%s","arm":"%s","partner":"%s","bank":%s,"error":"no output"}\n' \
             "$lab" "$armid" "$pid" "$bank" >> "$F"
      echo "  !! $lab $armid x $pid bank $bank FAILED" >&2; continue
    fi
    LAB="$lab" ARM="$armid" ASPEC="$arm" PID="$pid" PSPEC="$p" BSPEC="$B" BANK="$bank" J="$j" \
    python3 -c '
import json,os
d=json.loads(os.environ["J"])
print(json.dumps({"label":os.environ["LAB"],"arm":os.environ["ARM"],
  "armSpec":os.environ["ASPEC"],"partner":os.environ["PID"],
  "partnerSpec":os.environ["PSPEC"],"b":os.environ["BSPEC"],
  "bank":int(os.environ["BANK"]),"deals":d["deals"],"games":d["games"],
  "winRateA":d["winRateA"],"edgePts":100*d["winRateA"]-50,
  "ci":[100*d["ci"][0]-50,100*d["ci"][1]-50],
  "declAccA":d["declAccA"],"askAccA":d["askAccA"],
  "eventsPerGame":d["eventsPerGame"],"gamesPerSec":d["gamesPerSec"],
  "hw98":d["power"]["halfWidth98Games"],"mirror":d["power"]["mirror"]}))' >> "$F"
    echo "  $lab  $armid x $pid  bank $bank" >&2
  done
}

# ---------------------------------------------------------- the partner table
if [ "$PART" = "table" ] || [ "$PART" = "all" ]; then
  F="$OUT/P4-partners.jsonl"; : > "$F"
  echo "########## PARTNER REGIME  (a=ARM, partners=P, b=v05; L6's design at 30x its power)" >&2
  # "" means partners = the arm itself, which is arena.hpp's own encoding of the
  # three-copies target configuration.  It is the row that must never be the headline.
  PARTNERS=("|self" "v06|v06" "v05|v05" "v04|v04" "v03|v03" "detective|detective" "withholder|withholder" "lockout|lockout")
  for entry in "${PARTNERS[@]}"; do
    P="${entry%%|*}"; PID="${entry#*|}"
    pcell partner v07cand "$V07CAND" "$PID" "$P" "$DEALS" "$F" v05
    pcell partner v06     "$V06"     "$PID" "$P" "$DEALS" "$F" v05
  done
  # L6's own two rows for v0.5, so the corpus's table is reproduced at power and
  # the new arm is not compared against a number measured at +-3.46.
  for entry in "|self" "v03|v03" "detective|detective" "withholder|withholder"; do
    P="${entry%%|*}"; PID="${entry#*|}"
    pcell partner v05 v05 "$PID" "$P" "$DEALS" "$F" v05
  done
  # And the same table against the deployed policy rather than v0.5, because a
  # partner-transfer claim that only holds against one opponent is not a claim.
  for entry in "|self" "v06|v06" "v03|v03" "detective|detective"; do
    P="${entry%%|*}"; PID="${entry#*|}"
    pcell partner_vs_v06 v07cand "$V07CAND" "$PID" "$P" "$DEALS" "$F" v06
    pcell partner_vs_v06 v06     "$V06"     "$PID" "$P" "$DEALS" "$F" v06
  done
fi

# ------------------------------------------------------------------ cross-play
if [ "$PART" = "cross" ] || [ "$PART" = "all" ]; then
  F="$OUT/P4-crossplay.jsonl"; : > "$F"
  echo "########## CROSS-PLAY between independently-trained runs of the architecture" >&2
  RUNS=()
  for f in "$XPDIR"/p4-xp*.spec; do
    [ -e "$f" ] || continue
    RUNS+=("$(basename "$f" .spec)|$(cat "$f")")
  done
  if [ "${#RUNS[@]}" -lt 2 ]; then
    echo "!! fewer than two fitted runs in $XPDIR (p4-xp*.spec) -- run p4_crossfit_v07.sh first" >&2
  else
    # Every ordered pair: seat 0 from run i, the other two team-A seats from run j.
    # i == j is the self-play row.  A convention private to one run shows up as the
    # off-diagonal collapsing relative to the diagonal.
    for ri in "${RUNS[@]}"; do
      IDI="${ri%%|*}"; SI="${ri#*|}"
      for rj in "${RUNS[@]}"; do
        IDJ="${rj%%|*}"; SJ="${rj#*|}"
        [ "$IDI" = "$IDJ" ] && P="" || P="$SJ"
        pcell cross "$IDI" "$SI" "$IDJ" "$P" "$DEALS" "$F" v05
      done
    done
    # Head to head, so "they are different policies" is measured and not assumed.
    n=${#RUNS[@]}
    for ((i=0;i<n;i++)); do for ((j=i+1;j<n;j++)); do
      IDI="${RUNS[i]%%|*}"; SI="${RUNS[i]#*|}"
      IDJ="${RUNS[j]%%|*}"; SJ="${RUNS[j]#*|}"
      pcell h2h "$IDI" "$SI" "$IDJ" "" "$DEALS" "$F" "$SJ"
    done; done
  fi
fi
echo "== partner battery complete ==" >&2
