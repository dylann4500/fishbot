#!/usr/bin/env python3
"""K2 -- ledger L13's arithmetic, re-derived from this session's own capture.

The ledger's conversion (SUBOPTIMALITY-LEDGER.md 0.2): eliminating v0.6's
misdeclarations entirely is 0.141 sets of differential = ~2.1 win-rate points,
so ONE set of differential = 2.1/0.141 = 14.89 win-rate points.  L13 closes the
forced-endgame accuracy gap 0.286 -> 0.466 and prices it at that rate.
"""
import json, sys, collections

PTS_PER_SET = 2.1 / 0.141
CEIL = 0.466

arms = collections.defaultdict(lambda: {"n": 0.0, "hit": 0.0, "games": 0, "decl": 0.0})
for path in sys.argv[1:]:
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        d = json.loads(line)
        m = d["metrics"]
        f = m.get("forcedDeclAccuracy", {})
        key = "urgency ON (v06 as shipped)" if "pool=-1" not in d["a"] else "urgency OFF"
        a = arms[key]
        a["n"] += f.get("n", 0)
        a["hit"] += f.get("n", 0) * f.get("rate", 0)
        a["games"] += d["deals"] * d["rotations"]
        a["decl"] += m["declAccuracy"]["n"]

for key, a in arms.items():
    rate = a["n"] / a["games"]
    acc = a["hit"] / max(1, a["n"])
    gap = max(0.0, CEIL - acc)
    sets = rate * gap * 2
    print("%s" % key)
    print("   forced declarations %.0f in %d team-games  ->  %.5f per game "
          "(%.4f%% of %.0f declarations)"
          % (a["n"], a["games"], rate, 100 * a["n"] / a["decl"], a["decl"]))
    print("   forced-endgame accuracy %.4f;  gap to the %.3f feasible ceiling %.4f" % (acc, CEIL, gap))
    print("   closing the WHOLE gap: %.5f x %.4f x 2 = %.6f sets = %.4f win-rate points"
          % (rate, gap, sets, sets * PTS_PER_SET))
    print()
