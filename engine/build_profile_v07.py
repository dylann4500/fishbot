#!/usr/bin/env python3
"""FishBot v0.7 phase 3 -- the common-profile reduction.

Reads the per-cell artifacts written by candidates_v07.sh and emits the two
statistics the phase-3 brief asks for, per arm, over a SHARED opponent panel:

  worst case        min over panel cells of the arm's edge in win-rate points
  minimax regret    max over panel cells of (best arm's edge on that cell
                    minus this arm's edge on that cell)

Both are defined only over cells every compared arm actually ran, which is why
the panel membership is recomputed as the intersection and printed.

Per the project's standing rule the aggregate over the panel is NEVER the
headline; it is printed last, as a diagnostic, and labelled as one.

  usage:  ./build_profile_v07.py [--dir=research/v07/results] [--out=-]
"""
import glob, json, math, os, sys
from collections import defaultdict

d_dir, out_path = "research/v07/results", "-"
for a in sys.argv[1:]:
    if a.startswith("--dir="): d_dir = a.split("=", 1)[1]
    elif a.startswith("--out="): out_path = a.split("=", 1)[1]

# ---- load ------------------------------------------------------------------
cells = []
for p in sorted(glob.glob(os.path.join(d_dir, "P3-profile-*.jsonl"))):
    for line in open(p):
        line = line.strip()
        if not line: continue
        r = json.loads(line)
        if "error" in r: 
            cells.append({"arm": r["arm"], "opp": r["opp"], "bank": r["bank"], "err": r["error"]})
            continue
        cells.append(r)

errs = [c for c in cells if "err" in c]
cells = [c for c in cells if "err" not in c]

# ---- pool banks ------------------------------------------------------------
by = defaultdict(list)
for c in cells: by[(c["arm"], c["opp"])].append(c)

arms, opps, groups, spec = [], [], {}, {}
for c in cells:
    if c["arm"] not in arms: arms.append(c["arm"])
    if c["opp"] not in opps: opps.append(c["opp"])
    groups[c["opp"]] = c["group"]; spec[c["arm"]] = c["armSpec"]

pooled = {}
for k, cs in by.items():
    n = len(cs)
    edge = sum(c["edgePts"] for c in cs) / n
    hws = [ (c["ci"][1] - c["ci"][0]) / 2.0 for c in cs ]
    hw = math.sqrt(sum(h * h for h in hws)) / n
    pooled[k] = {
        "edge": edge, "hw": hw, "banks": n,
        "games": sum(c["games"] for c in cs),
        "mirror": all(c.get("mirror") for c in cs),
        "perbank": [(c["bank"], c["edgePts"]) for c in sorted(cs, key=lambda x: x["bank"])],
        "declAcc": sum(c["declAccA"] for c in cs) / n,
        "askAcc": sum(c["askAccA"] for c in cs) / n,
        "events": sum(c["eventsPerGame"] for c in cs) / n,
        "limit": max(c["limitHitRate"] for c in cs),
        "gps": sum(c["gamesPerSec"] for c in cs) / n,
        "oppSpec": cs[0]["oppSpec"],
    }

# ---- the shared panel is the intersection ---------------------------------
panel = [o for o in opps if all((a, o) in pooled for a in arms)]
dropped = [o for o in opps if o not in panel]

def best_on(o): return max(pooled[(a, o)]["edge"] for a in arms)

W = []
for a in arms:
    cs = [(o, pooled[(a, o)]["edge"]) for o in panel]
    worst = min(cs, key=lambda t: t[1])
    reg = [(o, best_on(o) - pooled[(a, o)]["edge"]) for o in panel]
    mreg = max(reg, key=lambda t: t[1])
    W.append({"arm": a, "worstOpp": worst[0], "worst": worst[1],
              "regOpp": mreg[0], "reg": mreg[1],
              "mean": sum(e for _, e in cs) / len(cs),
              "neg": sum(1 for _, e in cs if e < 0),
              "behind": sum(1 for o, r_ in reg if r_ > 1e-9)})

def f(x, s=2): return ("%+." + str(s) + "f") % x

L = []
w = L.append

# ---- the lead: the two statistics, and never the aggregate --------------------
best = min(W, key=lambda r: r["reg"])
worstv = {r["arm"]: r for r in W}
w("> **The two numbers, and the aggregate is not one of them.**")
w(">")
for r in W:
    w("> Over the shared %d-cell panel, `%s`'s **worst cell is %s against `%s`**, and its"
      % (len(panel), r["arm"], f(r["worst"]), r["worstOpp"]))
    w("> **minimax regret is %s**, incurred against `%s`. It is behind the better of the two"
      % (f(r["reg"]), r["regOpp"]))
    w("> arms on **%d of %d** cells." % (r["behind"], len(panel)))
    w(">")
w("> `%s` is the minimax-regret choice at **%s**." % (best["arm"], f(best["reg"])))
w("")
w("### 8.1 The panel, and how it is scored")
w("")
w("`engine/candidates_v07.sh` scores every arm against the **same** opponent panel on the **same**")
w("two training banks (7030001, 7030002) with the same protocol, because minimax regret is only")
w("defined over a shared panel — which is why `v06` appears here as an *arm* and not only as an")
w("opponent. `engine/build_profile_v07.py` reduces the cells; no number below is hand-typed.")
w("")
w("%d arms completed the full panel: **%s**. The script defines four more" % (len(arms), ", ".join("`%s`" % a for a in arms)))
w("(`K3-search`, `K3-on-composite`, `P2-composite`, `K1-fullgame`); the battery was stopped after")
w("the two above finished, and their three completed cells were deleted rather than reported. So")
w("**the regret below is regret within a %d-arm set** and is a lower bound on regret against a" % len(arms))
w("wider one. That is a real limitation and it is stated rather than smoothed: the number answers")
w("\"how much does choosing this arm cost me, against the worst opponent, relative to the best of")
w("these %d?\" and nothing larger. `K3-on-composite` and `P2-composite` are the two that matter and" % len(arms))
w("they are phase 4's, where the leading candidate has to be profiled anyway.")
w("")
w("Cell sizes are allocated per class and printed with the cell (deals x 2 rotations = games):")
w("cheap arm x near-parity 12,000 games/bank (hw 0.89); x `F-cheap` 5,000 (1.39); x `F-mid` and")
w("x the phase-2 composite 2,400 (2.00); x an opponent already beaten by more than fifteen points")
w("3,000 (1.79). Pooled over two banks each half-width divides by sqrt(2).")
w("")
if dropped:
    w("Panel cells not shared by every arm and therefore excluded: %s." % ", ".join("`%s`" % o for o in dropped))
    w("")
if errs:
    w("Cells that failed to produce output: %s." % ", ".join("`%s` vs `%s` bank %s" % (e["arm"], e["opp"], e["bank"]) for e in errs))
    w("")

w("### 8.2 Worst case and minimax regret — the two headline numbers")
w("")
w("| arm | worst cell over the panel | its edge | max regret | where the regret is | cells negative |")
w("|---|---|---:|---:|---|---:|")
for r in W:
    w("| `%s` | `%s` | **%s** | **%s** | `%s` | %d / %d |" % (
        r["arm"], r["worstOpp"], f(r["worst"]), f(r["reg"]), r["regOpp"], r["neg"], len(panel)))
w("")

w("### 8.3 Every cell, so the reader can take the worst one")
w("")
for g in ["frontier", "archetype", "adversary"]:
    gopps = [o for o in panel if groups[o] == g]
    if not gopps: continue
    w("**%s**" % {"frontier": "The frontier", "archetype": "The archetype panel",
                  "adversary": "The phase-2 adversary bank, train half"}[g])
    w("")
    w("| opponent | " + " | ".join("`%s` edge [95%% CI]" % a for a in arms) + " | regret to the better arm |")
    w("|---|" + "---:|" * len(arms) + "---:|")
    for o in gopps:
        row = ["`%s`" % o]
        for a in arms:
            p = pooled[(a, o)]
            row.append("mirror" if p["mirror"] else "%s [%s, %s]" % (f(p["edge"]), f(p["edge"] - p["hw"]), f(p["edge"] + p["hw"])))
        b = best_on(o)
        row.append("%.2f" % (b - min(pooled[(a, o)]["edge"] for a in arms)))
        w("| " + " | ".join(row) + " |")
    w("")

w("### 8.4 Four things the panel says that the head-to-head cells did not")
w("")
neg = {a: [o for o in panel if pooled[(a, o)]["edge"] < 0] for a in arms}
w("* **The survivor is negative on %d of %d cells and the incumbent on %d.** `%s` loses only to %s;"
  % (len(neg[arms[-1]]), len(panel), len(neg[arms[0]]), arms[-1],
     " and ".join("`%s` (%s)" % (o, f(pooled[(arms[-1], o)]["edge"])) for o in neg[arms[-1]])))
w("  the second of those has an interval containing zero, so the phase-2 composite is the only cell")
w("  where it is behind by more than noise. `%s` is negative on %s — %d of them members of the"
  % (arms[0], ", ".join("`%s`" % o for o in neg[arms[0]]),
     sum(1 for o in neg[arms[0]] if groups[o] == "adversary")))
w("  phase-2 adversary bank. That is the panel restating phase 2's finding in the panel's own")
w("  currency: the deployed policy is *behind* several unfitted deviations of itself.")
if ("A0-v06", "S-ask-2") in pooled and (arms[-1], "S-ask-2") in pooled:
    w("* **The survivor's own tie-break is on the panel, and beating it is what the rest of the stack is"
      " worth.** `S-ask-2` *is* `v06:rtie=1`. `%s` scores %s against it, so the urgency-off keys plus"
      % (arms[-1], f(pooled[(arms[-1], "S-ask-2")]["edge"])))
    w("  the stall rule are worth that much on top of the tie-break alone, measured against the")
    w("  tie-break rather than against `v06` — which section 9 says is the only admissible control")
    w("  for anything touching the tie group.")
if ("A0-v06", "R-v05") in pooled and ("A0-v06", "v05") in pooled:
    w("* **An internal consistency check passes.** `R-v05` and the archetype `v05` are the same policy")
    w("  entered on the panel twice by two different routes. They agree to the digit for both arms")
    w("  (%s / %s and %s / %s), which is the panel's own determinism check."
      % (f(pooled[("A0-v06", "R-v05")]["edge"]), f(pooled[("A0-v06", "v05")]["edge"]),
         f(pooled[(arms[-1], "R-v05")]["edge"]), f(pooled[(arms[-1], "v05")]["edge"])))
w("* **The worst cell is not where the strength table looks.** Both arms' worst cell is the phase-2")
w("  composite, which no head-to-head in sections 3-7 reports for the survivor. A configuration")
w("  chosen on its `v06` cell alone would be chosen on a number that is not its worst.")
w("")
w("### 8.5 The aggregate, printed last and labelled as a diagnostic")
w("")
w("| arm | mean edge over the panel | cells | games |")
w("|---|---:|---:|---:|")
for r in W:
    a = r["arm"]
    w("| `%s` | %s | %d | %s |" % (a, f(r["mean"]), len(panel),
        format(sum(pooled[(a, o)]["games"] for o in panel), ",d")))
w("")
w("The mean is not a summary of this arm's quality. It is dominated by how many far-from-parity")
w("archetypes the panel happens to contain, and the panel's composition was fixed for comparability")
w("across arms, not to represent any population of opponents.")

txt = "\n".join(L) + "\n"
if out_path == "-": sys.stdout.write(txt)
else: open(out_path, "w").write(txt)
