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
SPEC=""; ID=""; GAMES=400; SEED=31; THREADS=${THREADS:-13}; SIDE=1; SIDEGAMES=${SIDEGAMES:-400}; S6GAMES=${S6GAMES:-400}
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
    --sidegames=*) SIDEGAMES="${a#*=}" ;;
    --s6games=*)   S6GAMES="${a#*=}" ;;
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
# S6 runs in a CLEAN PROCESS.  Phase 3 (CANDIDATES section 2, C14) bisected the
# `F-cheap` S6 anomaly to `v7side` leaking state between its own four passes:
# running S3/S4/S5 in the same process changes which games S6 then sees, and the
# audited decision COUNT itself takes four different values with execution
# context (264,037 / 264,051 / 264,061 / 264,075).  Section 10 names fixing this
# as the first thing phase 4 inherits.  The fix costs one extra process.
SIDEJSON="[]"
if [ "$SIDE" = "1" ]; then
  rows=""; n=0
  IFS=, read -r -a BK <<< "$BANKS"
  for b in "${BK[@]}"; do
    j1=$("$FISH" v7side --a="$SPEC" --b=v06 --games="$SIDEGAMES" --seed="$b" \
                        --tests=s3,s4,s5 --threads="$THREADS" --json 2>/dev/null | tail -1)
    # S6 AT ONE THREAD.  Above one thread the test is a lottery: the same command
    # on the same cell returns 1, 2, 3 and 4 mismatches on successive runs and the
    # DENOMINATOR moves too (270,593 / 270,608 / 270,628).  At one thread it is
    # deterministic and reproduces phase 3's own one-thread figure for F-cheap
    # exactly (1/264,061).  A gate that is a lottery is not a gate.
    # ...AND WITH FRESH AGENTS PER DEAL.  Phase 4 traced the S6 residual to
    # per-agent state that survives `reset()`: agents are built once per thread and
    # reused across deals (arena.hpp, v07_side.hpp), so a searching configuration
    # carries residue from the previous deal that the reconstruction's fresh agent
    # does not have.  Rebuilding per deal takes every cell that showed a mismatch
    # to EXACTLY zero -- and takes F-cheap's denominator to 264,075, which is the
    # number phase 3 recorded for the one run it believed was clean.  With the
    # confound removed, S6 is a seat-isolation test again and zero tolerance is the
    # right rule.
    j2=$("$FISH" v7side --a="$SPEC" --b=v06 --games="$S6GAMES" --seed="$b" \
                        --tests=s6 --threads=1 --freshagents --json 2>/dev/null | tail -1)
    [ -n "$j1" ] && rows="${rows:+$rows,}$j1"
    [ -n "$j2" ] && rows="${rows:+$rows,}$j2"
    n=$(( n + 2 ))
    [ -z "$j1" ] && echo "gate_v07.sh: v7side S3/S4/S5 produced no output for $ID bank $b" >&2
    [ -z "$j2" ] && echo "gate_v07.sh: v7side S6 produced no output for $ID bank $b" >&2
  done
  # A MISSING side-channel run must not be a silent PASS.  Without this the empty
  # capture is swallowed by `2>/dev/null`, SIDEJSON stays "[]", the G7a/G7b rules
  # are never appended, and `all(...)` over G1-G6 reports PASS -- which would turn
  # the binary's own deliberate exit-6 on a mixSeed round-trip failure into a
  # hollow certification.
  got=$(printf '%s' "$rows" | awk -F'},{' '{print NF}')
  if [ "$rows" = "" ] || [ "$got" -ne "$n" ]; then
    echo "gate_v07.sh: expected $n v7side records for $ID, got ${got:-0} -- REFUSING to score." >&2
    exit 2
  fi
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
  "220 is v0.5's pressure rung and a 15.18-point cliff; v06 max 131, K3 125-129, the rejected stack 405. The p99 clause is a second tripwire for a configuration whose BULK lengthens rather than its tail: v06 120, K3 122-125, the frozen configuration 130, the rejected stack 132, so 150 is ~14% above the worst yet measured and nothing has failed it"),
 ("G6 late declarations", st["declsLate"]  == 0,
  "%d == 0" % st["declsLate"],
  "declarations taken at or after the 220-event rung are taken under the collapsed floor"),
]

side = json.loads(sid) if sid.strip() else []
s6res = {"mismatch": 0, "nodes": 0, "nonAsk": 0}
if side:
    a345 = [r for r in side if "S6 seat-isolation" not in r.get("tests", {})]
    a6   = [r for r in side if "S6 seat-isolation" in r.get("tests", {})]
    if a345:
        # `verdict` stays CERTIFIED when a test SKIPS (v07_side.hpp leaves the
        # roll-up untouched if s3MateQ / s4Deals / s5Nodes came out zero), so
        # "zero tolerance" would be asserted over a test that never ran.  Require
        # each enabled test to report PASS on its own line, and require all three.
        def ok345(r):
            t = r.get("tests", {})
            names = [k for k in t]
            return len(names) == 3 and all(t[k].get("status") == "PASS" for k in names)
        RULES.append(("G7a S3/S4/S5", all(ok345(r) for r in a345),
          " ".join("%s:%s" % (r["seed"], "ok" if ok345(r) else "FAIL/SKIP") for r in a345),
          "zero tolerance, per TEST and not per roll-up: a skipped test is not a pass"))
    if a6:
        for r in a6:
            s6res["mismatch"] += r["s6"]["mismatch"]; s6res["nodes"] += r["s6"]["nodes"]
            s6res["nonAsk"] += sum(r["s6"]["misByKind"][1:])
        rate = 1e6 * s6res["mismatch"] / max(1, s6res["nodes"])
        # G7b.  ZERO TOLERANCE, as THREAT-MODEL section 6.4 specifies it, now that the
        # confound is removed: one thread so the audit is deterministic, and fresh
        # agents per deal so the seat's own cross-deal residue is not counted as a
        # seat-isolation failure.  Under those two conditions every configuration in
        # this corpus -- blueprint and searching alike -- measures exactly zero.
        okb = (s6res["mismatch"] == 0)
        RULES.append(("G7b S6 seat-isolation", okb,
          "%d/%d = %.2f per million" % (s6res["mismatch"], s6res["nodes"], rate),
          "zero tolerance; one thread and fresh agents per deal, so the audit is deterministic "
          "and the cross-deal residue of section 4.3 is not miscounted as a side channel"))

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
       "side": side, "s6": (s6res if side else None), "sideRan": bool(side)}
open(os.environ.get("JSONL", "/dev/null"), "a").write(json.dumps(row) + "\n")
sys.exit(0 if passed else 1)
PY
rc=${PIPESTATUS[0]}
exit "$rc"
