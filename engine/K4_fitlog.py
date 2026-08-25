#!/usr/bin/env python3
"""K4 -- read ledger L5 directly off the fit logs.

L5's claim is that a per-GAME objective is too noisy for a CEM to climb at a
practical budget, and that a per-DECISION one is not.  That is a claim about the
SEARCH, not about the final win rate, and it can be read straight out of the
generation log without playing another game:

  * `bestScore` is the elite candidate's paired margin over the generation's own
    mean.  It is exactly 0.0 when the mean vector WAS the best candidate -- i.e.
    when the objective could not separate 12 proposals at all.  The share of
    generations at exactly zero is the flatness of the objective.
  * `spread` is max-minus-min score across the population.  Divided by the
    objective's own noise scale it says how many noise widths of signal the CEM
    had to work with.

Both are reported per objective at MATCHED budget.
"""
import json, glob, os, sys, statistics as st

RES = "/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
TAG = sys.argv[1] if len(sys.argv) > 1 else "s1"

print(f"{'objective':<11} {'denom/gen':>10} {'gens':>5} {'zeroGens':>9} "
      f"{'meanBest':>11} {'meanSpread':>11} {'finalMu':>9} {'r12':>7}")
for kpi in ("win", "selfdecl", "selfask", "selfalloc"):
    p = f"{RES}/K4-fit-{TAG}-{kpi}.jsonl"
    if not os.path.exists(p):
        print(f"{kpi:<11} (missing)"); continue
    gens, fin, denom = [], None, None
    for l in open(p):
        d = json.loads(l)
        if "gen" in d:
            gens.append(d); denom = d.get("denom")
        elif d.get("final") == "mu":
            fin = d["score"]
    if not gens:
        print(f"{kpi:<11} (empty)"); continue
    z = sum(1 for g in gens if abs(g["bestScore"]) < 1e-12)
    wf = f"{RES}/K4-weights/K4-fit-{TAG}-{kpi}.txt"
    r12 = float(open(wf).read().strip().split("|")[-18:][12]) if os.path.exists(wf) else float("nan")
    print(f"{kpi:<11} {denom:>10} {len(gens):>5} {z:>4}/{len(gens):<4} "
          f"{st.mean(g['bestScore'] for g in gens):>+11.5f} "
          f"{st.mean(g['spread'] for g in gens):>11.5f} "
          f"{(fin if fin is not None else float('nan')):>9.4f} {r12:>7.2f}")
print()
print("denom/gen is the objective's DECISION count per generation-candidate cell;")
print("for `win` it is games*rotations, i.e. one Bernoulli per game.")
print("finalMu is the fit's own terminal score on the FITTING bank (7030004) and")
print("is in the objective's own units -- it is not a win rate except for `win`.")
