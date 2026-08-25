#!/usr/bin/env bash
# FishBot v0.7 phase 4 -- replication of the one cell phase 3 named, and the
# attribution lattice.
#
# CANDIDATES.md section 10, fact 1: "K3's four keys on top of phase 2's composite beat
# that composite by +1.42 [+0.18, +2.68] ON ONE BANK, WITH NO MIRROR GATE on the
# combined configuration.  Replicating that is the first job."
#
# And the phase-4 brief: "v0.7 must not headline a gain it cannot attribute --
# budget the games."  So the second half is a leave-one-out and add-one-in
# lattice around the candidate, every arm against a COMMON reference opponent,
# on both banks, at a size that resolves the pieces rather than only the whole.
#
#   usage: ./p4_strength_v07.sh [--part=repl|lattice|all] [--threads=N] [--out=DIR]
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh

FISH=${FISH:-./fish7}
OUT=${OUT:-../research/v07/results}
THREADS=${THREADS:-13}
PART=all
for a in "$@"; do case "$a" in
  --part=*) PART="${a#*=}" ;; --threads=*) THREADS="${a#*=}" ;;
  --out=*) OUT="${a#*=}" ;; --fish=*) FISH="${a#*=}" ;;
esac; done

cell () {   # $1 label  $2 A  $3 B  $4 deals  $5 jsonl
  local lab="$1" A="$2" B="$3" D="$4" F="$5" bank j
  for bank in "${BANKS[@]}"; do
    j=$("$FISH" match --a="$A" --b="$B" --seed="$bank" --games="$D" \
                      --rotations=2 --threads="$THREADS" --json 2>/dev/null | tail -1)
    if [ -z "$j" ]; then
      printf '{"label":"%s","bank":%s,"error":"no output"}\n' "$lab" "$bank" >> "$F"
      echo "  !! $lab bank $bank FAILED" >&2; continue
    fi
    LAB="$lab" A="$A" B="$B" BANK="$bank" J="$j" python3 -c '
import json,os
d=json.loads(os.environ["J"])
print(json.dumps({"label":os.environ["LAB"],"a":os.environ["A"],"b":os.environ["B"],
  "bank":int(os.environ["BANK"]),"deals":d["deals"],"games":d["games"],
  "winRateA":d["winRateA"],"edgePts":100*d["winRateA"]-50,
  "ci":[100*d["ci"][0]-50,100*d["ci"][1]-50],
  "declAccA":d["declAccA"],"declAccB":d["declAccB"],
  "askAccA":d["askAccA"],"askAccB":d["askAccB"],
  "eventsPerGame":d["eventsPerGame"],"limitHitRate":d["limitHitRate"],
  "gamesPerSec":d["gamesPerSec"],"hw98":d["power"]["halfWidth98Games"]}))' >> "$F"
    echo "  $lab bank $bank  ($D deals x2)" >&2
  done
}

# ---------------------------------------------------------------- replication
if [ "$PART" = "repl" ] || [ "$PART" = "all" ]; then
  F="$OUT/P4-replicate.jsonl"; : > "$F"
  echo "########## REPLICATION: the candidate against the phase-2 composite" >&2
  # 12,000 deals x 2 rotations x 2 banks = 48,000 games; pooled half-width 0.63.
  # Phase 3 ran 3,000 deals on one bank (half-width 1.27) and got +1.42.
  cell "cand_vs_p2comp"  "$V07CAND" "$P2COMP" 12000 "$F"
  # The same claim from the other end: the candidate against the deployed policy,
  # and against the cheap search, so the three numbers can be read together.
  cell "cand_vs_v06"     "$V07CAND" "$V06"    12000 "$F"
  cell "cand_vs_fcheap"  "$V07CAND" "$FCHEAP" 12000 "$F"
  cell "p2comp_vs_v06"   "$P2COMP"  "$V06"    12000 "$F"
fi

# ----------------------------------------------------------------- attribution
if [ "$PART" = "lattice" ] || [ "$PART" = "all" ]; then
  F="$OUT/P4-lattice.jsonl"; : > "$F"
  echo "########## ATTRIBUTION: leave-one-out from the candidate, and add-one-in from v06" >&2
  # Reference opponent is `v06` throughout, because the headline claim is over the
  # deployed policy and a common reference is what makes the pieces additive-checkable.
  # 12,000 deals x 2 x 2 banks = 48,000 games a cell, pooled half-width 0.63 --
  # which is the size at which a 1.5-point piece is resolved and a 0.5-point piece is not.

  # add-one-in: each mechanism alone on top of v06
  cell "add_none"     "$V06"                                   "$V06" 12000 "$F"
  cell "add_search"   "v06:${SEARCH}"                          "$V06" 12000 "$F"
  cell "add_rtie"     "v06:${RTIE}"                            "$V06" 12000 "$F"
  cell "add_urgoff"   "v06:${URGOFF}"                          "$V06" 12000 "$F"
  cell "add_stall"    "v06:${STALL}"                           "$V06" 12000 "$F"
  cell "add_r12"      "v07:r12=25"                             "$V06" 12000 "$F"
  cell "add_m2"       "v07:m2=0"                               "$V06" 12000 "$F"

  # leave-one-out: the candidate minus each mechanism
  cell "full"         "$V07CAND"                                                     "$V06" 12000 "$F"
  cell "no_search"    "v07:m2=0,r12=25,${K3KEYS}"                                    "$V06" 12000 "$F"
  cell "no_rtie"      "v07:m2=0,r12=25,${URGOFF},${STALL},${SEARCH}"                 "$V06" 12000 "$F"
  cell "no_urgoff"    "v07:m2=0,r12=25,${RTIE},${STALL},${SEARCH}"                   "$V06" 12000 "$F"
  cell "no_stall"     "v07:m2=0,r12=25,${RTIE},${URGOFF},${SEARCH}"                  "$V06" 12000 "$F"
  cell "no_r12"       "v07:m2=0,${K3KEYS},${SEARCH}"                                 "$V06" 12000 "$F"
  cell "no_m2"        "v07:r12=25,${K3KEYS},${SEARCH}"                               "$V06" 12000 "$F"
fi
echo "== strength battery complete ==" >&2
