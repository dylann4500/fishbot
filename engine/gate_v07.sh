#!/usr/bin/env bash
# FishBot v0.7 phase 4 -- THE COMMIT GATE, and it runs before any strength number.
#
# The corpus contains configurations that score higher while being unsound.  The
# two on record:
#   v06:rtie=1,m1=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1  +2.68 over v06
#      and 2.91% provably-dead asks, a 326-ask dead run, 0.33% of games killed by
#      the action limit, a 405-event tail  (ADVERSARIES.md section 4H)
#   M8-alone  56.60% against v0.4 with 44.83% dead asks, a 373-ask dead run and
#      14% of games killed by the action limit  (ledger C14)
# so soundness is checked first and a failing configuration is never scored.
#
# Every threshold below is set from measured configurations, not chosen a priori,
# and the justification is printed with the verdict.
#
#   usage: ./gate_v07.sh --spec=SPEC [--id=NAME] [--games=400] [--seed=31]
#                        [--threads=N] [--out=DIR] [--side=0|1] [--banks=a,b]
#
# Emits one JSON verdict line per configuration to $OUT/P4-gate.jsonl and a human
# digest to $OUT/P4-gate.txt.  Exit status 0 = PASS, 1 = FAIL.
set -uo pipefail

FISH=${FISH:-./fish7}
OUT=${OUT:-../research/v07/results}
SPEC=""; ID=""; GAMES=400; SEED=31; THREADS=${THREADS:-13}; SIDE=1
BANKS="7030001,7030002"
for a in "$@"; do
  case "$a" in
    --spec=*)    SPEC="${a#*=}" ;;
    --id=*)      ID="${a#*=}" ;;
    --games=*)   GAMES="${a#*=}" ;;
    --seed=*)    SEED="${a#*=}" ;;
    --threads=*) THREADS="${a#*=}" ;;
    --out=*)     OUT="${a#*=}" ;;
    --side=*)    SIDE="${a#*=}" ;;
    --banks=*)   BANKS="${a#*=}" ;;
    --fish=*)    FISH="${a#*=}" ;;
  esac
done
[ -z "$SPEC" ] && { echo "gate_v07.sh: --spec= is required" >&2; exit 2; }
[ -z "$ID" ] && ID="$SPEC"

TXT="$OUT/P4-gate.txt"; JSONL="$OUT/P4-gate.jsonl"
mkdir -p "$OUT"

# ---- rule 5 needs the mirror, because the tail is a self-play property -------
MIR=$("$FISH" pathology --a="$SPEC" --b="$SPEC" --games="$GAMES" --rotations=2 \
                        --seed="$SEED" --threads="$THREADS" 2>&1)
[ -z "$MIR" ] && { echo "gate_v07.sh: pathology produced no output for $ID" >&2; exit 2; }

# ---- the side-channel gate, on both banks -----------------------------------
SIDEJSON="[]"
if [ "$SIDE" = "1" ]; then
  rows=""
  IFS=, read -r -a BK <<< "$BANKS"
  for b in "${BK[@]}"; do
    j=$("$FISH" v7side --a="$SPEC" --b=v06 --games=400 --seed="$b" \
                       --threads=2 --json 2>/dev/null | tail -1)
    [ -n "$j" ] && rows="${rows:+$rows,}$j"
  done
  SIDEJSON="[${rows}]"
fi

ID="$ID" SPEC="$SPEC" MIR="$MIR" SIDEJSON="$SIDEJSON" GAMES="$GAMES" SEED="$SEED" JSONL="$JSONL" \
python3 - <<'PY' | tee -a "$TXT"
import json, os, re, sys

mir, sid = os.environ["MIR"], os.environ["SIDEJSON"]
def g(pat, cast=float, d=None):
    m = re.search(pat, mir)
    return cast(m.group(1)) if m else d

st = {
  "eventsMean":  g(r"events/game\s+([\d.eE+-]+)"),
  "eventsP99":   g(r"p99\s+(\d+)", int),
  "eventsMax":   g(r"max\s+(\d+)", int),
  "askHit":      g(r"hit rate ([\d.]+)%"),
  "deadAskPct":  g(r"DEAD asks\s+\d+\s+\(([\d.eE+-]+)%"),
  "deadAsks":    g(r"DEAD asks\s+(\d+)", int),
  "longestDead": g(r"longest (\d+)", int),
  "gamesRun6":   g(r"games w/ run>=6\s+(\d+)", int),
  "starvedPct":  g(r"starved turns\s+\d+\s+\(([\d.eE+-]+)%"),
  "declPct":     g(r"declarations\s+\d+\s+wrong \d+ \(([\d.eE+-]+)%"),
  "declsLate":   g(r"at/after ev>=220\s+(\d+)", int),
  "limitPct":    g(r"action-limit games\s+\d+ \(([\d.eE+-]+)%"),
  "limitGames":  g(r"action-limit games\s+(\d+)", int),
}

# The rules.  Each carries the measured configurations the threshold is set from.
RULES = [
 ("G1 dead asks",       st["deadAskPct"]  <= 0.10,
  "%.5f%% of asks <= 0.10%%" % st["deadAskPct"],
  "v06 0.0118%, K3 arms 0.0088-0.0203%; the rejected m1=0 stack 2.91%"),
 ("G2 longest dead run", st["longestDead"] <= 5,
  "%d <= 5" % st["longestDead"],
  "v06 1, K3 arms 1; the rejected stack 326.  6 is the engine's own run cut"),
 ("G3 games w/ run>=6",  st["gamesRun6"]   == 0,
  "%d == 0" % st["gamesRun6"], "zero for every configuration the corpus has shipped"),
 ("G4 action-limit",     st["limitGames"]  == 0,
  "%d == 0" % st["limitGames"],
  "v06 0; the rejected stack 0.33% of games"),
 ("G5 mirror tail",      st["eventsMax"]   < 220 and st["eventsP99"] <= 150,
  "max %d < 220 and p99 %d <= 150" % (st["eventsMax"], st["eventsP99"]),
  "220 is v0.5's pressure rung and a 15.18-point cliff; v06 max 131, K3 125-129, the rejected stack 405"),
 ("G6 late declarations", st["declsLate"]  == 0,
  "%d == 0" % st["declsLate"],
  "declarations taken at or after the 220-event rung are taken under the collapsed floor"),
]

side = json.loads(sid) if sid.strip() else []
if side:
    ok = all(r.get("verdict") == "CERTIFIED" for r in side)
    RULES.append(("G7 side-channel", ok,
      " / ".join("%s:%s" % (r.get("seed"), r.get("verdict")) for r in side),
      "THREAT-MODEL S3/S4/S5/S6 by fish7 v7side, both training banks"))

passed = all(ok for _, ok, _, _ in RULES)
verdict = "PASS" if passed else "FAIL"

print("=" * 78)
print("COMMIT GATE  %s  ::  %s" % (verdict, os.environ["ID"]))
print("  spec   %s" % os.environ["SPEC"])
print("  mirror %s games, seed %s" % (os.environ["GAMES"], os.environ["SEED"]))
for name, ok, got, why in RULES:
    print("  [%s] %-22s %-46s  (%s)" % ("ok" if ok else "XX", name, got, why))
print("  reported, not gated: events/game %.3f, ask hit %.3f%%, misdeclaration %.5f%%"
      % (st["eventsMean"], st["askHit"], st["declPct"]))

row = {"id": os.environ["ID"], "spec": os.environ["SPEC"], "verdict": verdict,
       "games": int(os.environ["GAMES"]), "seed": int(os.environ["SEED"]),
       "rules": {n: bool(ok) for n, ok, _, _ in RULES}, "stats": st,
       "side": [{"seed": r.get("seed"), "verdict": r.get("verdict")} for r in side]}
open(os.environ.get("JSONL", "/dev/null"), "a").write(json.dumps(row) + "\n")
sys.exit(0 if passed else 1)
PY
rc=${PIPESTATUS[0]}
exit "$rc"
