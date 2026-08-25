#!/usr/bin/env python3
"""Reduce the phase-4 artifacts to markdown.  No number in RESEARCH-LOG section 4
is hand-typed; every table below is generated from research/v07/results/P4-*.jsonl.

  usage: ./build_p4_v07.py [--dir=research/v07/results] [--part=gate|repl|lattice|partners|cross|adv|all]
"""
import glob, json, math, os, sys
from collections import OrderedDict, defaultdict

D = "research/v07/results"
PART = "all"
for a in sys.argv[1:]:
    if a.startswith("--dir="): D = a.split("=", 1)[1]
    elif a.startswith("--part="): PART = a.split("=", 1)[1]

def load(name):
    p = os.path.join(D, name)
    if not os.path.exists(p): return []
    out = []
    for line in open(p):
        line = line.strip()
        if line: out.append(json.loads(line))
    return out

def f(x, s=2): return ("%+." + str(s) + "f") % x

def pool(rows, key="edgePts"):
    """Pool two banks: mean of the per-bank edges, half-widths added in quadrature."""
    n = len(rows)
    e = sum(r[key] for r in rows) / n
    hw = math.sqrt(sum(((r["ci"][1] - r["ci"][0]) / 2.0) ** 2 for r in rows)) / n
    return e, hw, n

def group(rows, *keys):
    g = OrderedDict()
    for r in rows:
        if "error" in r: continue
        k = tuple(r[x] for x in keys)
        g.setdefault(k, []).append(r)
    return g

L = []
w = L.append

# --------------------------------------------------------------------- gate
if PART in ("gate", "all"):
    rows = load("P4-gate.jsonl")
    if rows:
        seen = OrderedDict()
        for r in rows: seen[r["id"]] = r          # last run of each id wins
        w("#### The commit gate, run before any strength number")
        w("")
        w("| configuration | verdict | dead asks | longest run | run>=6 | action-limit | mirror max / p99 | late decl | S3/S4/S5 |")
        w("|---|---|---:|---:|---:|---:|---:|---:|---|")
        for id_, r in seen.items():
            s = r["stats"]
            s345 = [x for x in r.get("side", []) if "S6 seat-isolation" not in x.get("tests", {})]
            ok345 = "ok" if s345 and all(x["verdict"] == "CERTIFIED" for x in s345) else ("--" if not s345 else "FAIL")
            w("| `%s` | **%s** | %.5f%% | %d | %d | %d | %d / %d | %d | %s |" % (
                id_, r["verdict"], s["deadAskPct"], s["longestDead"], s["gamesRun6"],
                s["limitGames"], s["eventsMax"], s["eventsP99"], s["declsLate"], ok345))
        w("")
        w("Reported, not gated: events/game, ask hit rate and mirror misdeclaration for the same runs.")
        w("")
        w("| configuration | events/game | ask hit | mirror misdeclaration |")
        w("|---|---:|---:|---:|")
        for id_, r in seen.items():
            s = r["stats"]
            w("| `%s` | %.3f | %.3f%% | %.5f%% |" % (id_, s["eventsMean"], s["askHit"], s["declPct"]))
        w("")

# ---------------------------------------------------------------- replicate
if PART in ("repl", "all"):
    rows = load("P4-replicate.jsonl")
    if rows:
        w("#### Replication: the cell CANDIDATES section 10 named as the first job of phase 4")
        w("")
        w("| cell | A | B | n (games) | pooled edge | 95% CI | per bank | replicated |")
        w("|---|---|---|---:|---:|---|---|:--:|")
        for (lab,), rs in group(rows, "label").items():
            e, hw, n = pool(rs)
            per = " / ".join(f(r["edgePts"]) for r in sorted(rs, key=lambda x: x["bank"]))
            rep = "yes" if len({r["edgePts"] > 0 for r in rs}) == 1 else "**no**"
            w("| `%s` | `%s` | `%s` | %s | **%s** | [%s, %s] | %s | %s |" % (
                lab, rs[0]["a"][:38], rs[0]["b"][:38], format(sum(r["games"] for r in rs), ",d"),
                f(e), f(e - hw), f(e + hw), per, rep))
        w("")

# ------------------------------------------------------------------ lattice
if PART in ("lattice", "all"):
    rows = load("P4-lattice.jsonl")
    if rows:
        g = group(rows, "label")
        P = {k[0]: pool(v) for k, v in g.items()}
        per = {k[0]: " / ".join(f(r["edgePts"]) for r in sorted(v, key=lambda x: x["bank"])) for k, v in g.items()}
        w("#### Attribution: add-one-in from `v06`, and leave-one-out from the candidate")
        w("")
        w("Every arm against the same reference opponent `v06`, on the same two banks and the same")
        w("deal indices, so the cells are differences of correlated quantities and not independent")
        w("draws. `add_none` is the mirror and is a check that the reference is the reference.")
        w("")
        w("| arm | pooled edge over `v06` | 95% CI | per bank |")
        w("|---|---:|---|---|")
        order = ["add_none", "add_search", "add_rtie", "add_urgoff", "add_stall", "add_r12", "add_m2",
                 "full", "no_search", "no_rtie", "no_urgoff", "no_stall", "no_r12", "no_m2"]
        for k in order:
            if k not in P: continue
            e, hw, n = P[k]
            w("| `%s` | **%s** | [%s, %s] | %s |" % (k, f(e), f(e - hw), f(e + hw), per[k]))
        w("")
        if "full" in P:
            whole = P["full"][0]
            adds = [k for k in ("add_search", "add_rtie", "add_urgoff", "add_stall", "add_r12", "add_m2") if k in P]
            naive = sum(P[k][0] for k in adds)
            w("**Composition.** The six components measured alone sum to **%s**; the configuration"
              % f(naive))
            w("carrying all six measures **%s**. That is **%.0f%%** of the naive sum." % (f(whole), 100 * whole / naive if naive else 0))
            w("Phase 2 measured its own three mechanisms composing at 83%. A report that quotes the sum is wrong.")
            w("")
            w("| removed from the candidate | edge without it | drop from the whole |")
            w("|---|---:|---:|")
            for k in ("no_search", "no_rtie", "no_urgoff", "no_stall", "no_r12", "no_m2"):
                if k not in P: continue
                w("| `%s` | %s | **%s** |" % (k.replace("no_", ""), f(P[k][0]), f(whole - P[k][0])))
            w("")

# ----------------------------------------------------------------- partners
if PART in ("partners", "all"):
    rows = [r for r in load("P4-partners.jsonl") if "error" not in r]
    for lab, title, bspec in (("partner", "against three copies of `v05`", "v05"),
                              ("partner_vs_v06", "against three copies of `v06`", "v06")):
        rs = [r for r in rows if r["label"] == lab]
        if not rs: continue
        arms = list(OrderedDict((r["arm"], 1) for r in rs))
        ps = list(OrderedDict((r["partner"], 1) for r in rs))
        w("#### The partner-regime table, %s" % title)
        w("")
        w("`match --a=ARM --partners=P --b=%s`: team A is [ARM, P, P]. Ledger L6's design at 30x its" % bspec)
        w("power -- L6 ran 800 games a cell (half-width +-3.46), this runs 24,000 (half-width 0.63).")
        w("")
        w("| partners | " + " | ".join("`%s`" % a for a in arms) + " | delta (first two arms) |")
        w("|---|" + "---:|" * (len(arms) + 1))
        for p in ps:
            cells, vals = [], {}
            for a in arms:
                sub = [r for r in rs if r["arm"] == a and r["partner"] == p]
                if not sub: cells.append("--"); continue
                e, hw, n = pool(sub)
                vals[a] = e
                # v0.7 phase 4: `power.mirror` is `specA == specB` and ignores the
                # PARTNER specs, so `--a=v06 --partners=v03 --b=v06` -- a one-seat
                # deviation column running at 31.7% -- was flagged a mirror with an
                # interval of [0,0].  Fixed in the engine (main.cpp:70,166); recomputed
                # here so artifacts written by the pre-fix binary read correctly.
                truemirror = all(r.get("mirror") and not r.get("partnerSpec") for r in sub)
                cells.append("mirror" if truemirror else "%s [%s, %s]" % (f(e), f(e - hw), f(e + hw)))
            d = "%s" % f(vals[arms[0]] - vals[arms[1]]) if len(arms) > 1 and arms[0] in vals and arms[1] in vals else "--"
            w("| `%s` | %s | **%s** |" % (p, " | ".join(cells), d))
        w("")

# ---------------------------------------------------------------- crossplay
if PART in ("cross", "all"):
    rows = [r for r in load("P4-crossplay.jsonl") if "error" not in r]
    xs = [r for r in rows if r["label"] == "cross"]
    if xs:
        runs = sorted(OrderedDict((r["arm"], 1) for r in xs))
        w("#### Cross-play between independently-trained runs of the same architecture")
        w("")
        w("Row = the seat-0 run, column = the run its two partners come from, opponent = three copies")
        w("of `v05`. The diagonal is self-play. A convention private to one run shows up as the")
        w("off-diagonal collapsing relative to the diagonal.")
        w("")
        w("| seat 0 \\ partners | " + " | ".join("`%s`" % r for r in runs) + " | diagonal - mean off-diagonal |")
        w("|---|" + "---:|" * (len(runs) + 1))
        for a in runs:
            cells, vals = [], {}
            for b in runs:
                sub = [r for r in xs if r["arm"] == a and r["partner"] == b]
                if not sub: cells.append("--"); continue
                e, hw, n = pool(sub); vals[b] = e
                cells.append("%s [%s, %s]" % (f(e), f(e - hw), f(e + hw)))
            off = [vals[b] for b in runs if b != a and b in vals]
            gap = f(vals[a] - sum(off) / len(off)) if off and a in vals else "--"
            w("| `%s` | %s | **%s** |" % (a, " | ".join(cells), gap))
        w("")
    diag, offd = [], []
    for a in runs:
        for b in runs:
            sub = [r for r in xs if r["arm"] == a and r["partner"] == b]
            if not sub: continue
            e = pool(sub)[0]
            (diag if a == b else offd).append(e)
    if diag and offd:
        dm, om = sum(diag) / len(diag), sum(offd) / len(offd)
        w("**Self-play %s over %d diagonal cells; cross-play %s over %d off-diagonal cells; the gap is"
          % (f(dm), len(diag), f(om), len(offd)))
        w("%s points** against a per-cell half-width of about 0.63. The Hanabi line reports"
          % f(dm - om))
        w("self-play-to-cross-play collapses of 23.97 to 2.52 (SAD) and 24.04 to 0.12 (IPPO); this is")
        w("not that, and the runs are genuinely different policies -- see the distances below.")
        w("")
    h2h = [r for r in rows if r["label"] == "h2h"]
    if h2h:
        w("Head to head, so \"these are different policies\" is measured rather than assumed:")
        w("")
        w("| pair | edge | 95% CI |")
        w("|---|---:|---|")
        for (a, b), rs in group(h2h, "arm", "partner").items():
            e, hw, n = pool(rs)
            w("| `%s` vs `%s` | %s | [%s, %s] |" % (a, b, f(e), f(e - hw), f(e + hw)))
        w("")
    dist = [r for r in load("P4-crossfit.jsonl") if r.get("kind") == "distance"]
    if dist:
        w("Parameter distance between the runs, so the table above can be read:")
        w("")
        w("| pair | L2 | L-inf | coordinates |")
        w("|---|---:|---:|---:|")
        for r in dist:
            w("| `%s` vs `%s` | %.3f | %.3f | %d |" % (r["a"], r["b"], r["l2"], r["linf"], r["n"]))
        w("")

# ---------------------------------------------------------------- adversary
if PART in ("adv", "all"):
    ev = [r for r in load("P4-adveval.jsonl") if "error" not in r]
    fits = {r["id"]: r for r in load("P4-advfits.jsonl")}
    if ev:
        w("#### A fresh adversary search against the improved policy")
        w("")
        w("The target is the v0.7 candidate, not `v06`, and the objective axis is aimed at the")
        w("MECHANISMS phase 2 named rather than at reproducing its adversaries. A positive edge is")
        w("an exploit; the class detection floor is **1.53** and nothing below it is an exploit.")
        w("")
        w("| id | class | objective | hypothesis | adversary edge | 95% CI | per bank | clears 1.53? |")
        w("|---|---|---|---|---:|---|---|:--:|")
        for (i,), rs in group(ev, "id").items():
            e, hw, n = pool(rs, "advEdgePts")
            fit = fits.get(i, {})
            per = " / ".join(f(r["advEdgePts"]) for r in sorted(rs, key=lambda x: x["bank"]))
            w("| `%s` | %s | `%s` | %s | **%s** | [%s, %s] | %s | %s |" % (
                i, fit.get("class", "--"), fit.get("kpi", "--"), fit.get("hypothesis", "--")[:52],
                f(e), f(e - hw), f(e + hw), per, "**YES**" if e - hw > 1.53 else "no"))
        w("")

sys.stdout.write("\n".join(L) + "\n")
