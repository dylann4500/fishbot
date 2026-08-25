#!/usr/bin/env bash
# FishBot v0.7 phase 3 -- the common candidate profile battery.
#
# Every arm is scored against the SAME opponent panel on the SAME two training
# banks with the SAME protocol, so that worst-case-over-panel and minimax regret
# are comparable ACROSS arms.  Minimax regret is only defined over a shared
# panel, which is why `v06` is an arm here and not only an opponent.
#
# Per the project's standing rule the aggregate is never the headline: this
# script emits every cell and the reader takes the worst one.
#
#   usage:  ./candidates_v07.sh [--fish=PATH] [--out=DIR] [--threads=N] [--arms=a,b] [--quick]
#
# Banks: 7030001 and 7030002 (phase-2 adversary evaluation banks; phase-3
# training).  NEVER 709xxxx -- sealed holdout, and runMatch refuses them.
#
# NOTE ON n: `--games` counts DEALS; the arena plays each at `--rotations`
# orientations, so a cell of D deals is 2D games.  Every n below is in DEALS and
# every half-width quoted is for the resulting GAME count.
#
# n is allocated per cell class, and the allocation is PRINTED WITH THE CELL,
# because the panel spans configurations whose throughput differs by two orders
# of magnitude and because power is only worth buying where the arms are near
# parity.  Minimax regret differences live in the near-parity cells; against an
# opponent every arm beats by thirty points, another 20,000 games buys nothing
# any reader would act on.
#
#   cheap arm  x near-parity opponent   6000 deals = 12000 games/bank  hw 0.89  (pooled 0.63)
#   search arm x near-parity opponent   2500 deals =  5000 games/bank  hw 1.39  (pooled 0.98)
#   any        x FAR opponent (>15 pts) 1500 deals =  3000 games/bank  hw 1.79  (pooled 1.26)
#   any        x F-cheap                2500 deals =  5000 games/bank  hw 1.39  (pooled 0.98)
#   any        x F-mid / P2-composite   1200 deals =  2400 games/bank  hw 2.00  (pooled 1.41)
set -uo pipefail

FISH=${FISH:-./fish7}
OUT=${OUT:-../research/v07/results}
THREADS=${THREADS:-13}
ARMS_SEL=${ARMS_SEL:-}
QUICK=0
for a in "$@"; do
  case "$a" in
    --fish=*)    FISH="${a#*=}" ;;
    --out=*)     OUT="${a#*=}" ;;
    --threads=*) THREADS="${a#*=}" ;;
    --arms=*)    ARMS_SEL="${a#*=}" ;;
    --quick)     QUICK=1 ;;
  esac
done

BANKS=(7030001 7030002)
FCHEAP='v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26'
FMID='v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26'
SEARCHKEYS='s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26'
K3KEYS='rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12'
P2COMP="v07:m2=0,r12=25,${SEARCHKEYS}"

# ---- the arms -------------------------------------------------------------
# id | tier ("full" = whole panel, "frontier" = frontier cells only) | spec
ARMS=(
"A0-v06|full|v06"
"K3-stack|full|v06:${K3KEYS}"
"K3-search|full|v06:${K3KEYS},${SEARCHKEYS}"
"K3-on-composite|full|v07:m2=0,r12=25,${K3KEYS},${SEARCHKEYS}"
"P2-composite|frontier|${P2COMP}"
"K1-fullgame|frontier|v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12"
)

# The archetype panel, split by how near parity the opponent is to the frontier.
# NEAR gets full n because that is where regret differences between arms live;
# FAR is every style the incumbent already beats by more than fifteen points
# (phase 2's unfitted screen: v02 -31.2, hunter -48.3, diversifier -44.9,
# bluffer -49.9, silent -33.5, withholder -29.4, random -50.0).
ARCH_NEAR=(v05 v04 v03 lockout detective feint)
ARCH_FAR=(v02 hunter diversifier bluffer silent withholder random)
ADVFILE=${ADVFILE:-../research/v07/banks/train/adversaries-train.tsv}

is_search () { case "$1" in *s1=1*|*v07i*) return 0 ;; *) return 1 ;; esac; }

run_cell () {   # $1 armid  $2 armspec  $3 group  $4 oppid  $5 oppspec  $6 games
  local armid="$1" arm="$2" group="$3" oppid="$4" opp="$5" games="$6" bank j
  [ "$QUICK" = "1" ] && games=$(( games / 8 + 100 ))
  for bank in "${BANKS[@]}"; do
    j=$("$FISH" match --a="$arm" --b="$opp" --seed="$bank" --games="$games" \
                      --rotations=2 --threads="$THREADS" --json 2>/dev/null | tail -1)
    if [ -z "$j" ]; then
      printf '{"arm":"%s","group":"%s","opp":"%s","bank":%s,"error":"no output"}\n' \
             "$armid" "$group" "$oppid" "$bank" >> "$JSONL"
      echo "  !! $armid vs $oppid bank $bank FAILED" >&2
      continue
    fi
    ARMID="$armid" ARMSPEC="$arm" GROUP="$group" OPPID="$oppid" OPPSPEC="$opp" BANK="$bank" J="$j" \
    python3 -c '
import json,os
d=json.loads(os.environ["J"])
row={"arm":os.environ["ARMID"],"armSpec":os.environ["ARMSPEC"],"group":os.environ["GROUP"],
     "opp":os.environ["OPPID"],"oppSpec":os.environ["OPPSPEC"],"bank":int(os.environ["BANK"]),
     "games":d["games"],"deals":d["deals"],"winRateA":d["winRateA"],
     "edgePts":100*d["winRateA"]-50,"ci":[100*d["ci"][0]-50,100*d["ci"][1]-50],
     "meanSetsA":d["meanSetsA"],"meanSetsB":d["meanSetsB"],
     "declAccA":d["declAccA"],"declAccB":d["declAccB"],
     "askAccA":d["askAccA"],"askAccB":d["askAccB"],
     "declPerGameA":d["declPerGameA"],"forcedPerGameA":d["forcedPerGameA"],
     "eventsPerGame":d["eventsPerGame"],"limitHitRate":d["limitHitRate"],
     "gamesPerSec":d["gamesPerSec"],"threads":d["threads"],
     "hw98":d["power"]["halfWidth98Games"],"mirror":d["power"]["mirror"]}
print(json.dumps(row))' >> "$JSONL"
    echo "  $armid vs $oppid bank $bank  ($games games)" >&2
  done
}

for entry in "${ARMS[@]}"; do
  ARMID="${entry%%|*}"; rest="${entry#*|}"; TIER="${rest%%|*}"; ARMSPEC="${rest#*|}"
  if [ -n "$ARMS_SEL" ]; then case ",$ARMS_SEL," in *",$ARMID,"*) ;; *) continue ;; esac; fi
  JSONL="$OUT/P3-profile-${ARMID}.jsonl"
  : > "$JSONL"
  echo "########## $ARMID  ($TIER)  ::  $ARMSPEC" >&2
  if is_search "$ARMSPEC"; then NNEAR=2500; else NNEAR=6000; fi
  NFAR=1500

  # --- the frontier.  "Both configurations" of the brief plus the strongest
  #     measured point plus phase 2's composite, which is the bar.
  run_cell "$ARMID" "$ARMSPEC" frontier F-fast       "v06"     "$NNEAR"
  run_cell "$ARMID" "$ARMSPEC" frontier F-cheap      "$FCHEAP" 2500
  run_cell "$ARMID" "$ARMSPEC" frontier F-mid        "$FMID"   1200
  run_cell "$ARMID" "$ARMSPEC" frontier P2-composite "$P2COMP" 1200
  [ "$TIER" = "frontier" ] && { echo "  (frontier tier -- panel skipped)" >&2; continue; }

  for a in "${ARCH_NEAR[@]}"; do run_cell "$ARMID" "$ARMSPEC" archetype "$a" "$a" "$NNEAR"; done
  for a in "${ARCH_FAR[@]}";  do run_cell "$ARMID" "$ARMSPEC" archetype "$a" "$a" "$NFAR";  done

  if [ -f "$ADVFILE" ]; then
    while IFS=$'\t' read -r id cluster spec; do
      case "$id" in \#*|"") continue ;; esac
      n="$NNEAR"; is_search "$spec" && n=1500
      run_cell "$ARMID" "$ARMSPEC" adversary "$id" "$spec" "$n"
    done < "$ADVFILE"
  else
    echo "!! $ADVFILE not found -- adversary group skipped" >&2
  fi
done
echo "== battery complete ==" >&2
