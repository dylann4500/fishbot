# R11 — Determinized information-set search: first build, and what it measured

Written while the recon workflow was running; the numbers here are independent of R0–R10 and were
produced by `engine/src/v06.hpp` + `engine/src/v06_rollout.hpp` (this session's work in progress).
Machine load was 15–30 throughout, so every *timing* figure is a lower bound; win rates are unaffected.

## 1. What was built

`V06Agent : V05Agent`. With every v0.6 switch off it defers to the base class, and this is checked,
not asserted: at the time this note was written `./fish pathology --a=v06:s1=0 --b=v06:s1=0
--games=60 --seed=31` was **byte-identical** to the same command on `v05` (md5 `47c3e2bb…` on both).

**Superseded.** Once v0.6 was frozen with its own fitted vector, `v06:s1=0` is no longer v0.5 --- it
is v0.6's parameters with the search off. The identity control is now `v06:legacy=1`, which restores
v0.5's vector, and it is the form used by `experiments_v06.sh` E0 and by the paired ablation table.

The search is **not** the perfect-information Monte Carlo the v0.4 study eliminated and R0's
do-not-rebuild list re-eliminates. The deal is determinized — Proposition 1 makes the initial deal
the whole hidden state — but the six players who then play the continuation are seated at their own
information sets: the public deduction state refined by the hand the determinization gives them.
They therefore miss asks and misdeclare in the proportions the real game does, which is exactly what
a double-dummy rollout cannot do.

The reconstruction was checked against ground truth (`probe/reconcheck.cpp`, 40 games, 3,216 states,
103,644 card checks): the reconstructed information set is a **strict under-approximation** —
mask mismatches 0.334%, **wider in 346 of 346 cases and narrower in 0** — so it never credits a seat
with knowledge it does not have. Owner mismatches 0.212%.

## 2. Cost, measured end to end

| stage | cost |
|---|---:|
| `DealDP::build` on the actor's knowledge | 92.8 µs, once per decision |
| one exact posterior deal sample | 0.54 µs |
| six-seat information-set reconstruction | 8.7 µs |
| one rollout to the end of the game, `v05:belief=indep,value=0,topk=0` | 422 µs / 129 events |

Determinization is free; only the rollout costs anything.

## 3. The rollout blueprint is the binding constraint

| rollout policy | vs `v05`, 600–900 games, 6 rot, seed 90210 | µs/event |
|---|---:|---:|
| `v05:belief=indep,topk=0` | **11.78%** [9.83, 14.05] | 9.07 |
| `v05:topk=0,souter=1,sinner=1` | 47.33% [43.37, 51.33] | ~80 |
| `v05:topk=0,souter=1,sinner=3` | 45.33% [41.39, 49.33] | ~130 |
| `v05:topk=0,souter=2,sinner=4` | **50.00%** [46.01, 53.99] | ~260 |
| `v05:topk=0` (the blueprint itself) | ~50% | 348 |

The 62×-cheaper policy R10 measured is 38 points weaker, and a search whose continuation model is
38 points weak searches a different game. **Reducing the Sinkhorn iteration count is the cheap axis
that the corpus had not tried**: `souter=2,sinner=4` is statistically indistinguishable from the
blueprint at 1.3× the price of `souter=1,sinner=1` and 0.75× the price of the blueprint.

## 4. The result that matters: an unguarded search argmax is 27 points WORSE

`./fish match --a=v06:… --b=v05`, six rotations, seed 90210.

| configuration | win rate |
|---|---:|
| `det=8,cand=6`, rollout `belief=indep,topk=0`, plain argmax | **9.44%** [7.52, 11.80] n=720 |
| `det=8,cand=4`, rollout `souter=1,sinner=1`, plain argmax | 22.78% [17.26, 29.44] n=180 |
| `det=1,cand=4,blend=1e6` (plumbing control: forces the blueprint pick) | 50.83% [44.55, 57.10] n=240 |
| `det=12,cand=4,kappa=0` | 26.67% [20.24, 34.26] n=150 |
| `det=12,cand=4,kappa=1` | 37.33% [30.00, 45.30] n=150 |
| `det=12,cand=4,kappa=2.5` | 55.33% [47.34, 63.06] n=150 |
| `det=12,cand=4,kappa=2.5` | 52.08% [45.78, 58.32] n=240 |
| `det=12,cand=4,kappa=4` | 47.92% [41.68, 54.22] n=240 |
| `det=12,cand=4,kappa=6` | 47.50% [41.27, 53.81] n=240 |
| `det=16,tieonly=1,kappaTie=0` | 47.92% [41.68, 54.22] n=240 |
| `det=32,tieonly=1,kappaTie=0` | 52.08% [45.78, 58.32] n=240 |

**Two findings.**

**(a) The optimizer's curse dominates.** One determinization's value is the final half-suit
differential, sd ≈ 2.5; genuine differences between the leading candidates are an order of magnitude
smaller. An unguarded argmax over D noisy means therefore selects on residual noise and lands on a
candidate the blueprint had ranked below the top. The `blend=1e6` control isolates this: the same
code that scores 9.4% with the search deciding scores 50.8% when the blueprint decides, so the
machinery is correct and the *statistic* was wrong. Replacing the argmax with a paired
lower-confidence-bound rule — deviate only when the improvement over the blueprint's own choice, on
the same determinizations, clears κ standard errors — recovers the whole 27 points monotonically in
κ and takes the point estimate above 50% at κ = 2.5.

**(b) Searching the tie group alone does not obviously pay.** R0's loss channel #1 is the 55% of
decisions where the blueprint's score is bit-identical across two or more candidates. Waiving the
deviation penalty there (`tieonly=1, kappaTie=0`) gives 47.9% at D=16 and 52.1% at D=32 — no
resolved effect at n=240 either way. This is the predicted consequence of the ties being
*belief-symmetric*: if two cards are exchangeable under the posterior, the determinizations are
symmetric too and the rollout mean carries no signal, only variance. Only the fraction of ties that
the **exact** posterior separates (R6: 10.9 points of the 55) can be broken this way, and at D = 16–32
the noise on the other 80% swamps it.

## 5. What this implies for the v0.6 design

1. **The exact tie-break must be done with the exact posterior directly, not with rollouts.**
   `DealDP`/`BlockDP` resolve exchangeability in ~11 µs; a rollout ensemble that resolves the same
   thing costs ~10 ms and adds variance. Rollouts are the wrong instrument for loss channel #1.
2. **Any search must carry a paired significance guard.** This is not tuning: it is a 27-point
   correction, and it is the single largest effect this session measured.
3. **Search is a research bet, not the load-bearing mechanism**, and must be reported as such —
   including if it lands at zero.
4. **The rollout blueprint's Sinkhorn budget is the cost/fidelity dial**, and `souter=2,sinner=4`
   is the point where fidelity stops costing anything measurable.
