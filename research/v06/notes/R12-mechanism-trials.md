# R12 — Every v0.6 mechanism trial, with its verdict

Dylan Nguyen, FishLab Research Project. Engine at the v0.6 working tree (v0.5 base `bd812fe`).
All comparisons are **paired**: `fish ablate` plays every variant on the same deals against the same
panel and bootstraps the per-deal margin over the reference, resampling deals as clusters.
The sign convention is the corpus's: `delta = reference − variant`, so a **negative delta means the
variant is better**.

The single most important methodological fact recorded here: **four separate mechanisms looked
significant at 400–1,200 games per cell and evaporated at 3,000.** Every number below that is
quoted as a result was re-run at the larger budget on a second, disjoint seed bank.

---

## 0. The control that validates the instrument

`v06:s1=0` — the v0.6 agent with every switch off — returns `delta = +0.0000` with a **zero-width**
confidence interval against `v05`. `V06Agent` derives from `V05Agent` and defers to it when no
mechanism is enabled, so this is not a near-agreement but an identity, and it is checked by md5 of
the full `fish pathology` output as well
(`7d2865b9a6614ce59cd0516f84e83b76` for both `v05` and `v06`, 40 deals, seed 31).

Consequence: every ablation in this study is exact. There is no "v0.6 without mechanism X differs
from v0.5 for unrelated reasons" confound.

---

## 1. Mechanisms tried, and what they measured

| # | Mechanism | Best paired result | Verdict |
|---|---|---|---|
| A1 | **Exact-posterior tie resolution.** Re-score the tied candidates under the exact count law | **0.00% of ties are separated at all** | **REFUTED — see §2** |
| A1b | **Exact posterior as the ask marginal** (`belief=block` in effect) | argmax hit rate 48.30% against the Fast posterior's 51.04% on identical states | **REFUTED — see §3** |
| A2 | **Team-ownership discount** `wTeamHas` | −5: delta −0.0080 [−0.0373, +0.0207] (n=1,500/cell) | null |
| A3 | **Void creation** `wVoid` — reward asks that take the target's last card of a half-suit | 1.5: −0.0292 [−0.0537, −0.0037] at n=400/cell; **+0.0007 [−0.0213, +0.0227] and +0.0200 [−0.0017, +0.0413] at n=1,000/cell on two banks** | **null; the small-n result did not replicate** |
| A4 | **Last-live-opponent split** `wLastLive` | bit-identical to the wVoid-only arm (the condition fires too rarely to move anything) | inert |
| A5 | **The rationed deliberate miss**, gated on the linear score | identical output at margins 0.0, 0.5, 1.0, 2.0 — the gate never fires | **structurally inexpressible; see §4** |
| A5' | **Unrationed deliberate miss** (`v05:m1=0,m1p=1`) | 52.33% [48.33, 56.30] against v0.5 | **rejected on the KPI gate; see §4** |
| B1 | **Policy-prior retune** (`priorTheta`) | 0.30: 48.75%; 0.60: 45.58%; 0.80: 47.83%; 1.10: 42.92% (all n=1,200 vs v0.5) | **worse in play although better as a predictor; see §3** |
| D1 | **Declaration margin** (`declareMargin`, more negative = cash sooner) | −0.06: delta +0.0133 [+0.0013, +0.0247] — i.e. the *reference* is better | null-to-negative at the larger budget |
| S1 | **Determinized information-set search** | see §5 | **−27 points unguarded; ≈0 guarded** |

---

## 2. The exchangeable-tie result

`fish v6probe --mode=ties --a=v05 --b=v05 --games=100 --seed=31`, 4,728 ask decisions:

```
contested (>=2 live asks)  4671  (98.79%)
EXACT TIES at the top      2599  (55.64% of contested)
  same card, diff target    116  ( 4.46%)
  same half-suit, diff card 2444 (94.04%)
mean top1-top2 gap 1.60027   mean spread 7.70144
EXACT posterior SEPARATES the tied candidates  0  (0.00% of ties)

realised hit rate of each tie-break rule (n = 2,599):
  array order (enumeration first)   43.75%
  Fast-posterior argmax in the tie   43.75%
  the SHIPPED policy's actual pick   44.13%
  EXACT-posterior argmax in the tie  43.75%
  hindsight best in the tie          70.68%
```

**The 55% tie channel is not a defect *for any marginal-based rule*.** The tied candidates are two
cards of one half-suit at one target that the posterior — the *exact* posterior, not merely the
Sinkhorn fit — assigns identical **marginal** probability. Every rule that ranks them by that
marginal therefore realises the same 43.75% hit rate to three significant figures, and enumeration
order is as good as exactness.

**The scope of that claim has to be stated precisely, because a later experiment in this study
breaches it.** Equal marginals do not imply exchangeability in the *joint* posterior: two cards can
be equally likely to sit with the target while being differently correlated with the rest of the
deal. The probe above compares marginals, so it establishes that no marginal-based tie-break can
help — not that nothing can. A method that samples whole deals sees the joint structure the
marginals discard, and §5 reports that the determinized search's entire measured advantage comes
from exactly this set. The 70.68% hindsight
figure and the +3.16 sets/game "ask-oracle" bound in `R0` price **clairvoyance**, not headroom.

Two corrections this forces on the recon's own framing:

* **Array order does not decide 55% of asks.** v0.5's top-K chain/threat pass re-scores the leaders
  and moves the pick at **65.28% of ties** (`probe/tie2.cpp`, 80 games, 4,110 ties), so array order
  decides 34.72% × 55.42% ≈ **19.2%** of contested decisions — and, per the table above, decides
  them exactly as well as anything else could.
* **The channel is the CARD dimension, not the target dimension.** 94.04% of ties are two cards of
  one half-suit at one target; only 4.46% are one card at two targets. The v0.5 study's headline
  emphasis on the target channel (`(void)target;`) is aimed at 4% of the tie mass.

---

## 3. The exact posterior is a worse predictor than the deployed approximation

Same probe, full live candidate set at the same 4,671 decisions:

```
mean |exact - fast| marginal    0.0289
fast/exact argmax disagreement  22.59%
hit rate, Fast argmax           51.04%
hit rate, EXACT argmax          48.30%
hit rate, hindsight best        99.19%
```

`fish v6probe --mode=belief --games=100 --seed=31` scores each posterior as a predictor by
log loss on the 118,616 unresolved cards of those states and by the realised hit rate of its own
argmax ask:

| posterior | mean NLL | argmax p | argmax hit |
|---|---:|---:|---:|
| exact (uniform deal prior) | 1.42300 | 0.4657 | 47.04% |
| sinkhorn, θ = 0, φ = 0 | 1.39437 | 0.4889 | 47.96% |
| **sinkhorn, shipped θ = 0.44458** | **1.38358** | 0.5044 | **50.12%** |
| sinkhorn, θ = 0.60 | 1.38207 | 0.5121 | 50.20% |
| sinkhorn, θ = 0.80 | 1.38225 | 0.5217 | 50.25% |
| sinkhorn, θ = 1.10 | 1.38498 | 0.5341 | 50.25% |
| sinkhorn, θ = 2.00 | 1.39405 | 0.5560 | 50.01% |

Three things follow.

1. **"Exact" is exact under a *uniform-deal* prior, and that prior is wrong.** The exact count law
   is the worst predictor in the table. The policy prior is worth **+2.16 points of argmax hit rate**
   (47.96 → 50.12) and **0.011 nats**.

   **Correction, from the adversarial re-read.** An earlier draft said the policy prior was *the only
   thing* separating the deployed posterior from the exact one. It is not: with both prior
   parameters deleted the approximation still wins (NLL 1.40210 against the exact law's 1.42932,
   argmax hit 45.68% against 44.55%, independently re-run). About a third of the gap is the prior
   and the rest is the Sinkhorn fit's own maximum-entropy smoothing, which hedges better than the
   exact count law does against a truth that neither of them conditions on. A residual defect in the
   block dynamic program cannot be excluded from this evidence alone: its brute-force validation
   covers the states small enough to enumerate, not the ones where the gap is largest.
2. **The residual belief error is policy-model error, not approximation error.** This settles the
   corpus's oldest open question (`R0` Q1) in the negative: a matched-budget refit under
   `belief=block` is not worth running, because the object it would fit is a *less* informative
   posterior.
3. **The predictive optimum is not the playing optimum.** θ ≈ 0.6–0.8 predicts best, yet raising θ
   from the fitted 0.44458 costs 4.4 points of win rate at 0.60 and 7.1 at 1.10 (n = 1,200 each).
   The ask weights were fitted at θ = 0.44458 and do not transfer. Any θ change must be **jointly**
   refitted, which is what the v0.6 fit does.

---

## 4. The deliberate miss: expressible only under search

M1 removes every ask the actor can prove will miss. `R9` shows that this deletes the move class
every multi-step human tactic is built from — blackballing, choosing which opponent receives the
turn, the costly safe-ask signal — and that such an ask exists at 79.2% of v0.5's decisions and at
two or more *distinct* chosen opponents at 50.1%.

**Unbanning it is measured and rejected.** `v05:m1=0,m1p=1` — dead asks allowed, ownership features
gated by hit probability so the p = 0 incentive that caused v0.4's cycle cannot fire — scores
**52.33% [48.33, 56.30]** against v0.5 and fails the pathology gate outright:

```
DEAD asks          27.14% of asks        dead runs longest 364
games w/ run >= 6  11.25%                action-limit games 7.5%
forced endgame     8 declarations, 75% wrong
```

**Rationing it does not help either, because the linear score cannot price it.** With the four
ownership features zeroed on a dead candidate, a deliberate miss scores below the best live ask at
every decision: the admission margin was swept at 0.0, 0.5, 1.0 and 2.0 and the *output was
bit-identical at every setting*, because `f[0]`'s 11.64·p is zero on a dead ask by definition and
every remaining term of the score is a penalty.

This is the cleanest instance in the study of a move class that **exists only under search**: its
value is the value of the position the donated turn produces, which no static feature of the ask
computes. It is therefore offered to the search as a candidate (`deadsearch=N`, one per distinct
target, with a provably-free anti-repeat guard — a dead (card, target) pair stays dead unless the
card publicly moves to that target, which is observable, so forbidding the unmoved repeat forbids
nothing that could have become live; this is exactly the distinction the blanket repetition guard
missed, and why that guard cost 6.13 points).

---

## 5. Search: the optimizer's curse is worth 27 points

Full trace in `R11`. The result in one line: **an unguarded argmax over D rollout means is 27 points
worse than the blueprint it searches from**, and a paired lower-confidence-bound rule that deviates
only when the improvement over the blueprint's own choice clears κ standard errors recovers all 27
monotonically in κ.

| configuration | win rate vs `v05` |
|---|---:|
| plain argmax, D = 8, 6 candidates, cheap rollout | 9.44% [7.52, 11.80] n=720 |
| plain argmax, D = 8, 4 candidates, near-blueprint rollout | 22.78% n=180 |
| **control**: `blend=1e6` forces the blueprint's pick | 50.83% n=240 |
| LCB, κ = 0 | 26.67% n=150 |
| LCB, κ = 1 | 37.33% n=150 |
| LCB, κ = 2.5 | 55.33% n=150; 52.08% n=240 |
| LCB, κ = 4 | 47.92% n=240 |
| endgame-restricted (`maxq=26`), κ = 2.0 | 48.13% [43.69, 52.59] n=480 |
| endgame-restricted + deliberate miss (`deadsearch=2`) | 50.42% [45.96, 54.87] n=480 |

At every κ that is not actively harmful the search converges to the blueprint. **Search is shipped
off**, and the 27-point result is reported as the finding, because it is a property of determinized
evaluation in this game and not of this implementation: one determinization's return is the final
half-suit differential, sd ≈ 2.5, while genuine differences between the leading candidates are an
order of magnitude smaller.

---

## 6. What the null results imply, and the one thing that is not null

### 6.1 The plateau

Five mechanisms in §1 looked significant at 400–1,600 games per cell and returned zero at 3,000:

| mechanism | small-n paired delta | large-n paired delta, two disjoint banks |
|---|---|---|
| `wVoid = 1.5` | −0.0292 [−0.0537, −0.0037] (n=400) | +0.0007 [−0.0213, +0.0227] and +0.0200 [−0.0017, +0.0413] (n=1,000) |
| `wVoid = 3` | −0.0313 [−0.0593, −0.0033] (n=500) | as above |
| `w12 = 0` (delete the perseveration bonus) | −0.0200 [−0.0356, −0.0044] (n=800) | −0.0012 [−0.0137, +0.0112] and +0.0035 [−0.0092, +0.0158] (n=1,000) |
| `declareMargin = −0.06` | +1.75 points unpaired (n=1,200) | +0.0133 [+0.0013, +0.0247] — i.e. the reference is better |
| `w14 = 0` (delete the negative value-of-information term) | −0.0053 [−0.0244, +0.0141] (n=800) | not separated |

**Single-coordinate and single-feature perturbations of v0.5's policy class are worth zero.** This is
the study's central empirical finding about the plateau, and it retrospectively explains the v0.4 →
v0.5 transition, which also produced nothing in win rate. It is also a standing warning about the
corpus's own evidence standard: at the 400–1,000-games-per-cell budget used by most published
ablation rows in this project, an effect of the size these mechanisms appeared to have is
indistinguishable from a seed draw.

### 6.2 The one thing that is not null: a joint refit under the repaired optimiser

`fitA`: base `v05`, panel {v05, v03, lockout, withholder}, **paired** per-deal margin over the
incumbent, **minimax-regret** objective, **per-coordinate** sigma at 5% of each coordinate's range,
30 generations, population 20, elite 5, 300 games per cell, fitting seed **20260823**
(disjoint from every evaluation bank). Artifact `research/v06/runs/fitA.jsonl`.

Evaluated paired against `v05` over a seven-opponent panel at **1,000 games per cell** on two
held-out banks:

| bank | pooled delta (variant better = negative) | v05 | v04 | v03 | lockout | detective | **withholder** | feint |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 515253 reference | — | .500 | .518 | .733 | .754 | .751 | **.688** | .542 |
| 515253 fitA | **−0.0174 [−0.0319, −0.0027]** | .484 | .511 | .713 | .781 | .779 | **.783** | .557 |
| 90210 reference | — | .500 | .510 | .750 | .789 | .766 | **.698** | .522 |
| 90210 fitA | −0.0067 [−0.0209, +0.0079] | .515 | .490 | .747 | .755 | .778 | **.775** | .522 |

The pooled result resolves on one bank and not the other. **The per-cell result replicates in sign
and size on both: +9.5 and +7.7 points against the withholder**, the deception archetype that is the
project owner's own manoeuvre and the opponent the v0.5 study identified as the live exposure. Head
to head against v0.5 the refit is a wash (.484 / .515). Nothing else in this study produced a
replicated effect of that size.

Read together with §1: **the plateau is in the mechanisms, not in the parameters.** v0.5's own fit
was measured as sampling noise (OLS slope +0.00049/generation, t = 1.60, `R4`), so its vector is
essentially v0.4's, and v0.4's was produced by a CEM carrying the NFEAT-aliasing defect. A working
optimiser moves it.
