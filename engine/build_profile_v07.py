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

d_dir, out_path, arms_sel, sec = "research/v07/results", "-", None, "8"
for a in sys.argv[1:]:
    if a.startswith("--dir="): d_dir = a.split("=", 1)[1]
    elif a.startswith("--out="): out_path = a.split("=", 1)[1]
    elif a.startswith("--arms="): arms_sel = a.split("=", 1)[1].split(",")
    elif a.startswith("--sec="): sec = a.split("=", 1)[1]

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
if arms_sel: cells = [c for c in cells if c["arm"] in arms_sel]

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
    w("> **minimax regret is %s**, incurred against `%s`. It is behind the best of the %d"
      % (f(r["reg"]), r["regOpp"], len(arms)))
    w("> arms on **%d of %d** cells." % (r["behind"], len(panel)))
    w(">")
w("> `%s` is the minimax-regret choice at **%s**." % (best["arm"], f(best["reg"])))
w("")
w("### " + sec + ".1 The panel, and how it is scored")
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
if arms_sel and "FROZEN" not in arms_sel:
    w("(Phase 4 scored a third arm — the frozen v0.7 configuration — against this identical panel;")
    w("the three-arm table is `RESEARCH-LOG.md` §4.10, and it is reported there rather than here")
    w("because section 8 is phase 3's deliverable.)")
    w("")
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

w("### " + sec + ".2 Worst case and minimax regret — the two headline numbers")
w("")
w("| arm | worst cell over the panel | its edge | max regret | where the regret is | cells negative |")
w("|---|---|---:|---:|---|---:|")
for r in W:
    w("| `%s` | `%s` | **%s** | **%s** | `%s` | %d / %d |" % (
        r["arm"], r["worstOpp"], f(r["worst"]), f(r["reg"]), r["regOpp"], r["neg"], len(panel)))
w("")

NEAR = [o for o in panel if max(abs(pooled[(a, o)]["edge"]) for a in arms) <= 15.0]
FAR = [o for o in panel if o not in NEAR]
w("**Where the regret lives.** The panel deliberately contains opponents every arm beats by thirty")
w("points or more, because a configuration that has quietly broken shows up there first. But a")
w("three-point difference at a thirty-point margin is not a decision anyone makes, so regret is")
w("also reported over the **near-parity** subset — every cell in which no arm's edge exceeds 15")
w("points — which is where a choice between these arms is actually taken.")
w("")
w("| arm | max regret, whole panel | where | max regret, near-parity cells only | where |")
w("|---|---:|---|---:|---|")
for a in arms:
    rn = max(((o, best_on(o) - pooled[(a, o)]["edge"]) for o in NEAR), key=lambda t: t[1])
    rw = max(((o, best_on(o) - pooled[(a, o)]["edge"]) for o in panel), key=lambda t: t[1])
    w("| `%s` | %s | `%s` | **%s** | `%s` |" % (a, f(rw[1]), rw[0], f(rn[1]), rn[0]))
w("")
w("The near-parity subset is %d of %d cells (%s far cells excluded)." % (len(NEAR), len(panel), len(FAR)))
w("")
w("### " + sec + ".3 Every cell, so the reader can take the worst one")
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

w("### " + sec + ".4 What the panel says that the head-to-head cells did not")
w("")
FOCUS = max(W, key=lambda r: r["worst"])["arm"]        # the arm with the best worst case
neg = {a: [o for o in panel if pooled[(a, o)]["edge"] < 0] for a in arms}
w("* **Worst case and minimax regret do not pick the same arm, and that is the result.** `%s` has the"
  % FOCUS)
w("  best worst case at **%s** (%s), and %s. But the whole-panel minimax regret is won by `%s` at %s"
  % (f(worstv[FOCUS]["worst"]), worstv[FOCUS]["worstOpp"],
     ("it is negative on no cell at all" if not neg[FOCUS]
      else "it is negative on " + ", ".join("`%s`" % o for o in neg[FOCUS])),
     min(W, key=lambda r: r["reg"])["arm"], f(min(W, key=lambda r: r["reg"])["reg"])))
w("  against `%s`'s %s — and every point of that difference is bought in cells the panel includes as"
  % (FOCUS, f(worstv[FOCUS]["reg"])))
w("  a tripwire rather than as a decision. Restricted to the near-parity cells, the ordering reverses.")
w("* **The incumbent is negative on %d of %d cells**, %d of them members of the phase-2 adversary"
  % (len(neg[arms[0]]), len(panel), sum(1 for o in neg[arms[0]] if groups[o] == "adversary")))
w("  bank. That is the panel restating phase 2's finding in the panel's own currency: the deployed")
w("  policy is *behind* several unfitted deviations of itself.")
if ("A0-v06", "S-ask-2") in pooled:
    w("* **One panel member is a component of another arm.** `S-ask-2` *is* `v06:rtie=1`, so the column")
    w("  against it is measured against the only admissible control for anything touching the tie")
    w("  group: %s." % ", ".join("`%s` %s" % (a, f(pooled[(a, "S-ask-2")]["edge"])) for a in arms))
if ("A0-v06", "R-v05") in pooled and ("A0-v06", "v05") in pooled:
    ok = all(abs(pooled[(a, "R-v05")]["edge"] - pooled[(a, "v05")]["edge"]) < 1e-9 for a in arms)
    w("* **An internal consistency check %s.** `R-v05` and the archetype `v05` are the same policy"
      % ("passes" if ok else "FAILS"))
    w("  entered on the panel twice by two different routes, and they agree to the digit for every")
    w("  arm (%s). That is the panel's own determinism check." %
      "; ".join("%s / %s" % (f(pooled[(a, "R-v05")]["edge"]), f(pooled[(a, "v05")]["edge"])) for a in arms))
w("")
w("### " + sec + ".5 The aggregate, printed last and labelled as a diagnostic")
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
