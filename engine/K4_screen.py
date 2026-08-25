#!/usr/bin/env python3
"""K4 -- the claim under test, tabulated.

For each matched-budget fit: the per-decision PROXY it climbed, measured on the
EVALUATION banks, next to the win-rate OUTCOME on the same games.  The v0.5 ->
v0.6 transition lost 2.3 points of ask hit rate while gaining win rate; the
question is whether a fitted per-decision objective inherits that pathology.
The v06 reference row is the design-effect probe (48 blocks, seeds 74000001+).
"""
import json, glob, sys, statistics as st

RES = "/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
TAG = sys.argv[1] if len(sys.argv) > 1 else "s1"

# v06's own per-decision rates as arm A against v06, from K4-designeffect.jsonl
ref = [json.loads(l)["r"] for l in open(f"{RES}/K4-designeffect.jsonl")
       if json.loads(l)["arm"] == "v06"]
REF = dict(ask=st.mean(r["askAccA"] for r in ref),
           decl=st.mean(r["declAccA"] for r in ref),
           alloc=st.mean(r["allocErrRateA"] for r in ref))
print(f"reference v06-as-A vs v06 (48 blocks x 200 games): askAcc {100*REF['ask']:.3f}%  "
      f"declAcc {100*REF['decl']:.3f}%  allocErrShare {100*REF['alloc']:.3f}%")
print()

for opp in ("Ffast", "Fcheap"):
    rows = []
    try:
        rows = [json.loads(l) for l in open(f"{RES}/K4-screen-{TAG}-{opp}.jsonl")]
    except FileNotFoundError:
        continue
    print(f"=== SCREEN vs {opp} ===")
    print(f"{'objective':<11} {'bank':<9} {'edge':>7} {'ci':>18} {'n':>7} "
          f"{'dAskAcc':>9} {'dDeclAcc':>9} {'dAllocErr':>10}")
    by = {}
    for x in rows:
        r = x["r"]
        e = 100 * r["winRateA"] - 50
        ci = (100 * r["ci"][0] - 50, 100 * r["ci"][1] - 50)
        by.setdefault(x["kpi"], []).append(e)
        print(f"{x['kpi']:<11} {x['bank']:<9} {e:>+7.2f} "
              f"{'[%+.2f, %+.2f]' % ci:>18} {r['games']:>7} "
              f"{100*(r['askAccA']-REF['ask']):>+9.2f} "
              f"{100*(r['declAccA']-REF['decl']):>+9.2f} "
              f"{100*(r['allocErrRateA']-REF['alloc']):>+10.2f}")
    print()
    print(f"{'objective':<11} {'pooled edge':>12} {'n':>8}  replicated-in-sign  clears-1.53-floor")
    for k, v in by.items():
        p = sum(v) / len(v)
        rep = all(x > 0 for x in v) or all(x < 0 for x in v)
        print(f"{k:<11} {p:>+12.2f} {24000:>8}  {str(rep):<19} {str(abs(p) >= 1.53):<5}")
    print()
