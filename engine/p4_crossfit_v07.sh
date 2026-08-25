#!/usr/bin/env bash
# FishBot v0.7 phase 4 -- independently-trained runs of the SAME architecture.
#
# The phase-4 brief requires "cross-play between independently-trained runs of the
# same architecture".  The shipped v0.7 candidate is a CONFIGURATION on top of
# v0.6's frozen vector, so no two independently-trained runs of it exist; they
# have to be produced.  This produces them.
#
# INDEPENDENT means three things at once, and all three are varied here:
#   * a different CEM trajectory                (tune --seed drives Rng rng(sp.seed))
#   * a DISJOINT fitting deal bank              (the same --seed drives genSeed)
#   * a different starting basin for one run    (--fromv6 vs the v0.5 defaults)
# The architecture -- every structural key of the candidate -- is held fixed, and
# only the 55-coordinate vector is fitted.  That is what makes the runs runs of
# the SAME architecture rather than different architectures.
#
# The fit reproduces v0.6's own recipe (v06.hpp:196 V6FIT_PROVENANCE):
#   obj=minimaxregret, paired, panel = v05 + v03 + withholder + feint.
# sigmarel is deliberately WIDER than v0.6's 0.04.  A cross-play test is only
# informative if the runs actually separate in parameter space, so the separation
# is bought on purpose and then MEASURED and reported alongside the result.
#
#   usage: ./p4_crossfit_v07.sh [--runs=3] [--gens=6] [--pop=12] [--deals=150]
set -uo pipefail
cd "$(dirname "$0")"
. ./p4_specs.sh

FISH=${FISH:-./fish7}
RUNS=${RUNS:-../research/v07/runs}
OUT=${OUT:-../research/v07/results}
THREADS=${THREADS:-13}
NRUNS=3; GENS=6; POP=12; DEALS=150; SIGREL=0.12
for a in "$@"; do case "$a" in
  --runs=*) NRUNS="${a#*=}" ;; --gens=*) GENS="${a#*=}" ;; --pop=*) POP="${a#*=}" ;;
  --deals=*) DEALS="${a#*=}" ;; --sigmarel=*) SIGREL="${a#*=}" ;;
  --threads=*) THREADS="${a#*=}" ;; --fish=*) FISH="${a#*=}" ;;
esac; done
mkdir -p "$RUNS" "$OUT"
ART="$OUT/P4-crossfit.jsonl"

# The architecture, as a --base.  weightSpec continues an option list with a
# comma when the base already carries a colon (tuner.hpp:158), so this is safe.
ARCH="$V07CAND"
# Panel: v0.6's own, with each member's commas rewritten to '+' and members
# joined by ';' -- the escape main.cpp:220-226 provides and the defect
# RESEARCH-LOG 1.7 records what happens without.
PANEL="v05;v03;withholder;feint"

FITSEEDS=(7060001 7060002 7060003 7060004 7060005 7060006)
for ((i=0;i<NRUNS;i++)); do
  ID=$(printf "xp%d" $((i+1)))
  SEED=${FITSEEDS[$i]}
  SPECF="$RUNS/p4-$ID.spec"
  if [ -s "$SPECF" ]; then echo "skip $ID (already fitted)"; continue; fi
  # Run 3 starts from the v0.5 defaults instead of the incumbent: a genuinely
  # different basin, which is one more shared bias removed.
  SEEDFLAG="--fromv6"; BASIN="v06-incumbent"
  [ "$ID" = "xp3" ] && { SEEDFLAG=""; BASIN="v05-defaults"; }
  echo "=== crossfit $ID  seed=$SEED basin=$BASIN budget=${GENS}x${POP}x${DEALS} sigmarel=$SIGREL ===" >&2
  T0=$(date +%s)
  W=$("$FISH" tune --panel="$PANEL" --base="$ARCH" --full $SEEDFLAG --kpi=win \
        --games=$DEALS --pop=$POP --elite=$(( POP / 3 + 1 )) --gens=$GENS \
        --beta=10 --sigmarel=$SIGREL --paired --obj=minimaxregret \
        --seed=$SEED --threads="$THREADS" --out="$RUNS/p4-$ID.jsonl" \
        | tail -1 | sed 's/^weights=//')
  T1=$(date +%s)
  [ -z "$W" ] && { echo "  $ID FAILED" >&2; continue; }
  echo "$W" > "$RUNS/p4-$ID.txt"
  case "$ARCH" in *:*) SEP="," ;; *) SEP=":" ;; esac
  echo "${ARCH}${SEP}allparams=${W}" > "$SPECF"
  printf '{"battery":"P4crossfit","id":"%s","arch":"%s","basin":"%s","fitSeed":%s,"gens":%s,"pop":%s,"fitDeals":%s,"fitGames":%s,"sigmaRel":%s,"panel":"%s","seconds":%s}\n' \
    "$ID" "$ARCH" "$BASIN" "$SEED" "$GENS" "$POP" "$DEALS" "$(( GENS * POP * DEALS * 2 * 4 ))" \
    "$SIGREL" "$PANEL" "$(( T1 - T0 ))" >> "$ART"
  echo "  $ID fitted in $(( T1 - T0 ))s" >&2
done

# How far apart did the runs actually land?  Printed with the cross-play table,
# because an off-diagonal that does not collapse is only evidence of robustness
# if the runs are genuinely different policies.
python3 - "$RUNS" >> "$ART" <<'PY'
import glob, json, os, sys, math
d = sys.argv[1]
V = {}
for f in sorted(glob.glob(os.path.join(d, "p4-xp*.txt"))):
    V[os.path.basename(f)[3:-4]] = [float(x) for x in open(f).read().strip().split("|")]
ids = sorted(V)
for i in range(len(ids)):
    for j in range(i + 1, len(ids)):
        a, b = V[ids[i]], V[ids[j]]
        n = min(len(a), len(b))
        l2 = math.sqrt(sum((a[k] - b[k]) ** 2 for k in range(n)))
        li = max(abs(a[k] - b[k]) for k in range(n))
        print(json.dumps({"battery": "P4crossfit", "kind": "distance",
                          "a": ids[i], "b": ids[j], "n": n, "l2": l2, "linf": li}))
PY
echo "== crossfit complete -> $ART ==" >&2
