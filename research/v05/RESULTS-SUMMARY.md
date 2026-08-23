# v0.5 results summary — what the battery actually shows

Generated from `engine/experiments_v05.sh` artifacts, `research/v05/results/E1`–`E9`, at the
frozen configuration (`engine/freeze_config_v05.py`, vector in
`research/v05/runs/v05-fitted.txt`, round-trip assertion passes: `v05` vs
`v05:allparams=<vector>` returns exactly 50%).

## The honest headline

**v0.5 is not meaningfully stronger than v0.4 in win rate.** It is +1.11 points head-to-head,
the nine-opponent means are a wash (83.60% vs 83.57%), and v0.4 is marginally better on minimax
regret. What v0.5 delivers is the elimination of the failure mode, not a strength jump. The
paper must say this plainly and must not headline a win rate.

## Pathology (E2) — this is the result

| | v0.4 mirror | v0.5 mirror |
|---|---:|---:|
| provably dead asks | 39.04% | **0%** |
| dead runs | 2,610 (longest 286) | **0** |
| games with a dead run ≥ 6 | 34.33% | **0%** |
| exact repeat asks | 40.03% | **2.63%** |
| declarations wrong | 10.44% | **2.07%** |
| declarations at/after the horizon | 768, 58.59% wrong | **0** |
| forced-endgame declarations | 28, **100% wrong** | 2, **0% wrong** |
| events/game (median, p90, p99) | 143.6 (106, 312, 321) | **96.6 (96, 112, 125)** |
| ask hit rate | 34.25% | **55.47%** |
| games hitting the action cap | 0% | 0% |

E1: VERIFY PASS — 0 audit violations in 23,594,580 checks, 0 set-conservation failures,
0 action-limit games, determinism PASS.

## Forced endgame at volume (E8, 24,000 games per arm)

| | rate/game | correct |
|---|---:|---:|
| v0.4 | 0.0307 | **0.14%** |
| v0.5 | 0.0048 | **24.35%** |

v0.5 both enters the forced endgame 6.4× less often and, when it does, names a
capacity-feasible allocation that is right about a quarter of the time. The measured ceiling for
the best feasible allocation at those states (P2) is ~40.6%, so a gap remains.

**Caveat that must appear in the paper:** `fish pathology` pools both teams' forced
declarations, so the cross-match figure (29 declarations, 89.7% wrong) is dominated by v0.4's
side. Forced-endgame accuracy must be reported per declaring team, as E8 does. This is the same
error the v0.4 corrections register flags at C4.

## Per-style profile (E4, 300 deals x 6 rotations per cell, seed 515253)

| opponent | v0.5 | v0.4 | delta |
|---|---:|---:|---:|
| v0.4 | 51.11% | 50.00% | +1.11 |
| v0.3 | 72.33% | 73.33% | −1.00 |
| v0.2 | 81.28% | 83.06% | −1.78 |
| lockout | 79.56% | 77.94% | +1.61 |
| detective | 76.78% | 77.28% | −0.50 |
| diversifier | 93.78% | 92.89% | +0.89 |
| hunter | 97.72% | 97.67% | +0.06 |
| bluffer | 99.89% | 99.94% | −0.06 |
| random | 100.00% | 100.00% | 0.00 |
| **worst case** | **51.11%** | **50.00%** | |
| mean | 83.60% | 83.57% | |
| minimax regret over the style set | 1.78 (on v0.2) | 1.61 (on lockout) | |

The v0.4 study reported "\vfast's lowest win rate against any panel member is 75.07%". That
panel contained no opponent of its own strength. Once one is included the worst case for both
policies is ~50%, because the worst case for any policy in this family is a copy of itself.
Corrections register C10 covers this.

## Fitting (E-fit)

`research/v05/runs/fit-round1.jsonl`: 40 generations, CEM over the 34-coordinate vector,
population 24, elite 6, panel `v04, v03, lockout, detective, diversifier, hunter`, beta = 25.

- The mirror-strength opponent is in the panel, which it was not in any recorded v0.4 round.
- At beta = 25 over a panel spanning ~0.47 the soft minimum is a genuine minimum; v0.4's
  beta = 10 over a panel spanning ~0.06 gave a max/min gradient weight ratio of 1.9, i.e. a
  weighted mean. Corrections register C4.
- In-panel best generation: 57.0 / 75.2 / 80.5 / 79.0 / 94.8 / 98.5.
- The tuner's final common-seed re-evaluation preferred the distribution mean (0.5272) over the
  best single generation (0.4798) — the winner's-curse guard doing its job.
- **Held-out, after freezing: ~51% against v0.4.** In-panel 57% versus held-out 51% is the
  regression, and the held-out figure is the one to report.

## What must be stated as a limitation

1. The strength gain is small and within a couple of points of noise on some styles. v0.5's
   case rests on the pathology table, not the win rate.
2. The fit raised `priorTheta` from 0.264 to 0.445. The diagnosis established that
   over-weighting the policy prior is precisely the deception exposure (doubling costs ~6
   points). Robustness of the fitted value against the deceptive archetypes is **not yet
   measured** and is the first thing a v0.5.1 should check.
3. Mechanisms M3–M7 (net-information ask term, per-seat knowledge models, target-dimension
   selection, partner-aware stochastic selection, online opponent model) are specified and
   **not built**. Patches for M4/M5 and M7 exist under `research/v05/patches/` and are unmeasured.
4. No exploitability probe has been run against v0.5. The v0.4 study ran one; v0.5 must not
   claim comparable robustness until it does.
5. Forced-endgame accuracy is 24.35% against a measured feasible ceiling of ~40.6%.
