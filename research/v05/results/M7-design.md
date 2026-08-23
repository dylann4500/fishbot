# M7 — online per-seat opponent model

Dylan Nguyen, FishLab Research Project
Repository `/Users/dylan/Documents/GitHub/fish optimization`, base commit `fe21e19` plus the lead
session's working v0.5 (M1 + M2 + M8 as built in `engine/src/v05.hpp`).

**Deliverables.** `engine/src/v05_oppmodel.hpp` (the mechanism, new and unprotected) and
`research/v05/patches/M7.patch` (the wiring into `v05.hpp` / `factory.hpp` / `main.cpp`, **not
applied** — those files belong to the lead session). Every number below was produced by an
out-of-tree harness compiled against the unmodified engine, or by the patch applied to a sandbox
copy of the tree. No protected header was modified in place.

**The target.** The project owner's own manoeuvre: hold two cards of a half-suit you were asked
for, deliberately do not ask back in it, and the bot is confused. `P3-deception.md` reproduces this
as `withholder:k=6` and measures the damage — v0.4 loses **0.122 of posterior mass** on the cards
the withholder actually holds in half-suits it has asked in, and is **under-confident by −0.090**
in the "asked once" evidence cell (truth 0.4367, model 0.3464).

---

## 0. Headline

1. **`priorPhi` is not a silence channel, and the reason is exact algebra, not a small effect.**
   The exponent rearranges so that `phi`'s contribution is a factor depending on the **seat alone**,
   which iterative proportional fitting erases at its fixed point. Verified to mean 1.9e-7 per
   marginal on 1,852 real belief states (§1). The pair (θ, φ) is a single effective certificate
   weight θ_eff = θ + φ, and v0.4's shipped (0.26380, 0.13280) is θ_eff = **0.39660**.
2. **The condition a replacement statistic must meet is weaker than "card-dependent within the
   half-suit".** It is: *the log-weight must not be a function of the seat alone*. A
   (seat, half-suit) statistic suffices — that is exactly what θ is, and θ moves the posterior by
   mean 4.9e-3 / max 0.41 (§2). This matters, because the card-level refinement I built turns out
   to bind on only **0.9%** of cells; had within-half-suit variation been genuinely required, M7
   would have had almost nothing to work with.
3. **The silence statistic has an exact zero on one side.** Ask legality requires the asker to hold
   another card of the half-suit, so `P(p asks in S | p holds no card of S) = 0` — confirmed
   empirically at 0.000 over 5 policies (§3). A `fast`/`slow` outcome is therefore the hard C5
   certificate the engine already applies, and `never` is its exact soft complement. Only `never`
   tilts the belief, so nothing is double-counted at the level of the observation.
4. **The reply channel identifies the owner's manoeuvre; nothing identifies the Feint.**
   `P(never | holds S)` is **0.503** for `withholder:k=6` against **0.008–0.027** for every other
   policy tested — a likelihood ratio near 30 per episode. The probe channel and two further
   candidate statistics (ask breadth, cards-held-at-ask) fail to separate `feint` at all (§3.2).
   The `feint` type therefore ships with prior mass **0.0**: present so M10 can refit it the moment
   a discriminating statistic exists, inert until then.
5. **Identification needs more than one deal.** A seat supplies only 1.7–4.9 *uncensored* episodes
   per deal. Within one deal the withholder's type posterior reaches 0.228 against a 0.16 prior;
   carried across deals at retention 0.9 it reaches **0.542**, against 0.14–0.24 for the innocent
   seats at the same table and 0.142 when seat 1 is honest (§4). Bayesian Policy Reuse is defined
   across episodes of a task and that is how it has to be used here.
6. **Nothing is claimed about strength.** 50-game sanity runs at two seeds disagree in *sign* on
   every cell (§5). No parameter was chosen from them. The one default that is not v0.4's is
   *derived* from an existing P3 measurement, not tuned.

---

## 1. The algebra, and what actually happens in the shipped code

### 1.1 The rearrangement

`Knowledge::priorWeight` (`engine/src/belief.hpp:100-108`) computes

```
w(c, p) = exp( theta * a[p][S(c)] - phi * (T[p] - a[p][S(c)]) )        clipped to ±2.6
        = exp( (theta + phi) * a[p][S(c)] )  ·  exp( -phi * T[p] )
          \_________ depends on (seat, half-suit) _________/  \__ seat only __/
```

`Belief::sinkhornDisj` (`belief.hpp:478-529`) fits `w` to row sums 1 and column sums `q_p` by IPF.
For a kernel with **total support**, IPF's limit is the unique matrix `diag(u) K diag(v)` carrying
those margins. Replacing `K` by `K·diag(g)` replaces `diag(v)` by `diag(v/g)` and leaves the matrix
itself identical. A seat-only factor is erased exactly.

So `phi` is not a second channel. `(theta, phi)` is one number:

```
theta_eff = theta + phi
```

### 1.2 Verification

An out-of-tree replica of `sinkhornDisj` with a pluggable initialiser, run on the belief states of
20 real v0.4 mirror games (seed 4242, every 5th event, seats 0/2/4): **1,852 states, 275,046
(card, seat) cells.**

| comparison | max \|Δ marginal\| | mean |
|---|---|---|
| replica vs `Belief::sinkhornDisj`, same weights | **0.000e+00** | **0.000e+00** |
| clip **off**, converged (outer 4, inner 200): (θ,φ) vs (θ+φ,0) | 4.26e-04 | **1.92e-07** |
| clip **off**, shipped iterations (outer 4, inner 8) | 8.60e-01 | 1.39e-03 |
| clip **on**, converged | 4.44e-01 | 2.56e-03 |
| clip **on**, shipped iterations | 4.44e-01 | 2.56e-03 |

The algebraic claim holds to machine precision once the two implementation artefacts are removed:

* **the ±2.6 clip is not separable.** `max(e^{-2.6}, A(c,p)g(p))` does not factor. Census over the
  same states: 62,934 (seat, half-suit) cells, the clip binds **high on 2.32%** (1,463 cells) and **low on
  12.12%** (7,630). The low binding is what creates φ's only genuine card-dependence — a saturation
  artefact of a numerical guard, not a modelled signal.
* **`sinkInner = 8` stops short of the fixed point.** Raising it to 200 collapses the clip-off
  residual from 1.39e-03 to 1.92e-07.

The theory carries one exception, and it turns out to be too rare to see. IPF absorbs a column
scaling at its limit only when the kernel has **total support** — every mask-legal cell lying on
some capacity-feasible allocation. Splitting the same states on that property, checked against the
exact DP (`DealDP`), with pure IPF (`outer = 1`, no clip) at two iteration counts:

| | states | cells | inner = 200: max / mean | inner = 5000: max / mean |
|---|---|---|---|---|
| total support | 1,845 | 274,866 | 1.13e-03 / 9.12e-07 | 4.65e-05 / 5.01e-08 |
| not total support | 5 | 156 | 8.42e-06 / 2.27e-07 | 1.36e-08 / 3.67e-10 |

**This split demonstrates nothing, and the honest reading is that it cannot.** Only 5 of 1,850
states lack total support, and only 29 of 213,017 mask-legal cells (0.01%) are capacity-infeasible,
so the exception has 156 cells to show itself in against 274,866. Both groups' residuals fall by
roughly the same factor when the iteration count is raised twenty-five-fold, which is what
incomplete IPF convergence looks like in both — not a persistent floor in one and machine precision
in the other. The load-bearing measurement is the clip-off converged row of the table above
(mean 1.9e-07); the total-support refinement is recorded here only so that nobody spends an
afternoon re-deriving it from a sample this thin. Establishing it needs states chosen for the
property, not sampled from ordinary play.

### 1.3 What this does to the record

`DESIGN.md` defect **K** says the second term "is card-independent and Sinkhorn's capacity
normalisation removes it". That is right, and §1.2 upgrades it from an assertion to a measurement,
with the two artefacts named. Two consequences that should go into the paper:

**(a) P3's ablation table is a one-dimensional sweep of a single parameter.** Re-labelled by θ_eff,
`P3-deception.md` §4's paired ablation reads:

| variant | θ_eff | paired loss vs shipped v0.4 |
|---|---|---|
| shipped `v04` | 0.3966 | — (reference) |
| `pphi=0` | 0.2638 | −0.30 [−3.05, 2.40] |
| `ptheta=0` | 0.1328 | +0.95 [−1.75, 3.70] |
| `ptheta=0,pphi=0` | 0.0000 | +4.40 [1.60, 7.15] |
| `2×` both | 0.7932 | +5.25 [2.45, 8.05] |

Single-peaked in θ_eff, with the optimum bracketing the shipped value: the loss falls monotonically
from +4.40 at θ_eff = 0 to −0.30 at θ_eff = 0.2638, then rises through the reference to +5.25 at
0.7932. P3's own conclusion that "the
whole cliff is in `theta`" is then a scale artefact: its `theta` column sweeps θ_eff over
{0.133, 0.199, 0.265, 0.397, 0.660, 1.133}, an 8.5-fold range, while its `phi` column sweeps
{0.264, 0.297, 0.330, 0.397, 0.529, 0.764}, a 2.9-fold range centred higher. **Falsifiable
prediction: plotted against θ_eff the two columns must collapse onto one curve.** It costs one
command and would retire two paragraphs of P3 §5:

```
./fish match --a=v04:ptheta=0.3966,pphi=0 --b=withholder:k=6 --games=300 --seed=20260823 --json
# and the same for ptheta in {0.1328, 0.1987, 0.2647, 0.6604, 1.1328}, pphi=0
```

**(b) `--ptheta` and `--pphi` are redundant in the fitting vector.** `factory.hpp` exposes both to
the CEM as independent parameters (`allparams` slots K+9 and K+10). They span a one-dimensional
manifold, so the optimiser is searching a degenerate direction. The running refit is not wrong
because of it — the objective is flat along that direction, not distorted — but one slot is wasted
and the fitted pair is not identifiable. M10 should fit θ_eff alone.

---

## 2. Why M7's statistic is not normalised away

The condition is exactly: **the log-weight must not be a function of the seat alone.** Anything
carrying a half-suit index survives, because the IPF fixed point only absorbs full-row and
full-column scalings.

Control measurement, same 1,852 states, converged settings: a (seat, half-suit) tilt of the shape
`exp(-0.35 · missCount[p][S])` — deliberately chosen as an arbitrary (seat, half-suit) statistic —
moves marginals by **mean 4.85e-3, max 0.41** relative to the untilted fit. That is two to three
orders of magnitude above the erased φ residual and comparable in size to what θ itself does.

M7's silence term is indexed `(seat, half-suit)` through `cell[p][S].llr` and additionally by card
through the **opportunity weight**: a card of S whose possible owners are all p's own teammates or
dead seats is one p would never ask for, so p's silence in S says nothing about it, and the tilt is
withheld.

**Honest measurement of the card-level part: it binds rarely.** Over 12,656 (seat, half-suit) cells
with at least two unresolved candidate cards, the opportunity weight differs across the cards of the
half-suit in **114 (0.9%)**. The card-level refinement is a correctness guard, not the load-bearing
part; the (seat, half-suit) index is what makes the statistic survive.

This corrects the design constraint I was given. "It must be card-dependent within the half-suit or
it will be normalised away exactly like `priorPhi`" is stronger than the mathematics requires, and
0.9% is what a design built to satisfy it literally would have had to work with.

---

## 3. The statistics, measured

Two episode kinds, both opened by a **public act**, so a seat that never acts generates no evidence:

* **reply** — opened when seat *p* is the **target** of an ask in half-suit *S*. Outcome: did *p*
  ask in *S* within its next W1 = 2 own turns (*fast*), within W2 = 6 (*slow*), or not at all
  (*never*)? *never* is the owner's manoeuvre.
* **probe** — opened when *p* **asks** in *S*. Same three outcomes for asking in *S* again.

An "own turn" is an Ask or a Pass with *p* as actor; declarations are not turns.

### 3.1 The exact zero, and the census

`fish`-linked harness, 30 games per policy, all six seats running the marked policy, seed 20260822,
conditioned on **ground truth** "does this seat hold a card of S". Reply n = 159–690, probe
n = 1,781–3,963 per policy.

| policy | reply fast / slow / never | probe fast / slow / never |
|---|---|---|
| v0.5 (M1+M2+M8) | .946 / .046 / **.008** | .946 / .034 / .021 |
| v0.4 | .941 / .042 / **.017** | .978 / .016 / .006 |
| v0.3 | .890 / .084 / **.027** | .931 / .040 / .030 |
| `silent:tol=0.10` | .915 / .073 / **.012** | .932 / .054 / .014 |
| `feint` | .912 / .068 / **.020** | .910 / .068 / .022 |
| **`withholder:k=6`** | .333 / .164 / **.503** | .974 / .015 / .012 |

And the other half of the table, the one the rules make exact:

> for every policy, `fast = 0.000` and `slow = 0.000` when the seat holds **no** card of S.

That is ask legality (`fish.hpp:183-184`), not an approximation. It is why a *fast* or *slow* outcome is
the C5 certificate the belief engine already applies as a hard constraint, and why only *never*
outcomes are allowed to tilt the belief.

**The reply channel is the mechanism.** `P(never | holds S)` = 0.503 for the withholder against
0.008–0.027 for everything else: a likelihood ratio of roughly 30 per episode, on exactly the
manoeuvre the owner reported.

### 3.2 Negative result: the Feint is not identifiable from the public record

The probe channel was designed to catch "ask once in a half-suit you hold one card of, then abandon
it". It does not, and the reason is structural: the Feint keeps that one card, so it remains legally
able to ask in the half-suit and does. `P(never | holds S)` on the probe channel is 0.022 for
`feint` against 0.021 for v0.5 — no separation.

Two further candidates were measured (40 games per policy, seed 20260822) and also fail:

| policy | ask breadth (distinct half-suits / ask) | *ground truth*: cards held in S at the ask | *ground truth*: asks holding exactly 1 |
|---|---|---|---|
| v0.5 | 0.3281 | 2.336 | 27.2% |
| v0.4 | 0.2810 | 2.237 | 33.2% |
| v0.3 | 0.2962 | 2.572 | 22.9% |
| `silent:tol=0.10` | 0.2898 | 2.001 | 38.7% |
| **`feint`** | **0.2972** | **2.260** | **34.1%** |
| `withholder:k=6` | 0.2663 | 2.065 | 33.6% |

The Feint is not separable from v0.4 even on the *ground-truth* statistic that defines it (34.1% vs
33.2%), because its deviation rate is only 15.2% of its asks (P3 §1) and it is drowned in the base
rate. This is consistent with P3's own finding that the Feint's damage runs through the hard C5
certificate, which no reweighting recovers.

**Consequence in the shipped code.** `feint` is in the type library with prior mass **0.0** and a
θ multiplier of 0.67, inert. Shipping it with mass would apply a constant across-the-board discount
to every seat's certificate weight and present it as per-seat modelling, which the evidence does not
support. The `renorm` floor deliberately does **not** revive a zero-prior type.

**Named limitation, with the experiment.** M7 cannot detect the cheapest deception in the panel —
`feint` costs its user nothing (75.4% vs v0.3, against v0.4's 75.0%). Resolving it needs a statistic
that reads *outcomes* rather than *actions*: the natural candidate is the seat's hand-count
trajectory within a half-suit it asked in, i.e. whether its certificate is ever cashed. That is a
one-afternoon measurement with the same harness and it is the first thing to try before M10.

---

## 4. Online behaviour of the model

Per-seat posterior over the type library, updated by Bayesian Policy Reuse (Rosman, Hawasly &
Ramamoorthy, MLJ 104:99–127, 2016). The latent "does *p* hold a card of S" is marginalised with a
cheap capacity-proportional estimate `q`, so

```
P(fast | tau) = q · fastRate(tau)     P(slow | tau) = q · slowRate(tau)
P(never | tau) = 1 - q · (fastRate + slowRate)
```

and every tilt is scaled by Johanson & Bowling's data-biased factor `n / (n + n0)`, `n0 = dataBias`.
Episodes are censored when `q` is outside [0.03, 0.97] (nothing latent to learn) or when the seat
has no possible ask left in the half-suit.

**Measured, 40 deals, seat 1 marked, seats 0/2/3/4/5 = v0.5, observer = seat 0, `dataBias = 2`:**

| marked policy | retention | withholder posterior at seat 1 | at the four innocent seats | uncensored episodes / deal |
|---|---|---|---|---|
| `withholder:k=6` | 0.0 (per-deal) | 0.228 | 0.147–0.185 | 4.9 (innocent 2.1–2.4) |
| `withholder:k=6` | 0.7 | 0.397 | — | 4.9 |
| `withholder:k=6` | 0.9 | **0.542** | 0.143–0.237 | 4.9 |
| v0.5 (honest control) | 0.9 | 0.142 | 0.114–0.211 | 2.5 |
| `feint` | 0.9 | 0.197 | 0.130–0.181 | 4.8 |

Population prior for the withholder type is 0.16. Readings:

* **Within one deal the model barely moves** — 0.228 against a 0.16 prior. That is exactly the
  identifiability budget P3 §6.2 predicted, now measured in the units that matter: 2–5 uncensored
  episodes per seat per deal, spread over 9 half-suits.
* **Carrying the posterior across deals is what makes it work**, and BPR is defined that way. At
  retention 0.9 the manoeuvre reaches 0.542 while honest seats sit at 0.11–0.24 and an honest seat 1
  sits at 0.142 — below its prior. Specificity is good; the false-positive drift to 0.237 on one
  innocent seat is the noise floor.
* **The Feint sits at its prior**, as §3.2 requires.
* **Effect sizes are bounded.** θ_eff for the identified withholder rises 0.3966 → 0.4437 (+11.9%);
  innocent seats move +1.4–3.9% across both retention-0.9 tables. The clip at 1.5× keeps the worst
  case strictly inside the region
  P3 §5 measured as safe (2× costs 5.25 points [2.45, 8.05] paired).
* **The silence tilt is rare and locally strong**: non-zero on 1.95% of (card, seat) cells against
  the withholder and 1.02% against an honest table, mean |z| over all cells 0.008, max at the 0.90
  cap. (Measured at tilt strength 1.0; the shipped default scales these by 0.30.)

**Seat-keying caveat, enforced in the patch.** The posterior is indexed by seat, so it may only be
carried while the agent keeps the same seat at the same table. `V05Agent::reset` full-resets on a
seat change. The arena's duplicate-block rotation flips seats every rotation, so `m7carry` cannot
engage under the default `--rotations=2`; the retention frontier can only be measured with
`--rotations=1` or at a fixed table. This is a real constraint on how M7 must be evaluated, and it
was found by measurement — `m7carry=0.9` and `m7carry=0` returned bit-identical match statistics
before the guard was understood.

---

## 5. What is **not** established

**M7's strength is unmeasured, and the sanity runs cannot measure it.** 25 deals × 2 rotations =
50 games per cell, cluster CIs roughly ±13 points, arms not paired.

| | seed 515151 | | seed 20260901 | |
|---|---|---|---|---|
| arm | vs `withholder:k=6` | vs `v04` | vs `withholder:k=6` | vs `v04` |
| `v05:m7=0` | 60% | 38% | 54% | 32% |
| `v05` (ask weight 0.30) | 72% | 42% | 60% | 50% |
| `v05:m7ask=1.0` | **44%** | 30% | **60%** | 40% |
| `v05:m7sil=0` (θ channel only) | 66% | 32% | 60% | 44% |
| `v05:m7th=0` (silence only) | 56% | 40% | 60% | 40% |

The `m7ask=1.0` row is the point: 44% at one seed and 60% at the other, against the same opponent.
**The two seeds disagree in sign.** An earlier draft of this note nearly reported the 44% as
evidence that the silence channel was harmful; it is noise. Nothing here is a result, and no
parameter was chosen from this table.

**The one default that is not v0.4's is derived, not tuned.** `m7AskWeight = 0.30` comes from
P3 §6.2(ii): the responsiveness rates in §3.1 are *raw* conditional rates, while the belief they
tilt has already applied C1–C5, which explains much of the same variation. P3 measured the size of
that gap for θ — v0.4's fitted 0.264 against an empirical raw coefficient of 0.907 for honest play,
a residual fraction of 0.29 — and the default carries that measured fraction across. **It is a
placeholder.** M10 must fit it on the residual after C1–C5, and P3 §2's truth-vs-marginal
calibration table, bucketed by silence-episode count instead of by ask count, is exactly the right
instrument.

**The remaining open items**, in the order they should be closed:

1. Fit `m7AskWeight` and `dataBias` on the residual (above), and publish the
   exploitation/robustness frontier over `dataBias ∈ {∞, 8, 4, 2, 1, 0.5, 0}` as the paper asked.
2. Measure the retention frontier `m7carry ∈ {0, 0.5, 0.7, 0.9, 1.0}` under `--rotations=1`.
3. Refit the type profiles against a *mixed* table; §3.1's rates come from mirror self-play, where
   every seat runs the marked policy.
4. **Validate against a best-responder, not against these archetypes.** P3 §6.3.7's requirement
   applies with more force to M7 than to v0.4, because an online per-seat model is a data-poisoning
   surface that only an adaptive opponent can probe. The minimum acceptable validation is an
   opponent allowed to condition its asks on a simulated copy of the model it is trying to move.
   None of the archetypes here observe v0.5's beliefs at all.
5. Look for a statistic that identifies the Feint (§3.2), before concluding it cannot be done.

---

## 6. Safety properties, each keyed to a measured constraint

| property | mechanism | evidence it answers |
|---|---|---|
| tilt only, never a constraint | M7 reweights deals satisfying C1–C5; it can never exclude one | P3 §6.3.1 — no archetype raised "confidently wrong" above 4.5%, because a deceiver can withhold evidence but cannot manufacture a contradiction |
| bounded tilt | \|z\| ≤ `tiltCap` 0.90, inside `belief.hpp`'s ±2.6 | P3 §5's cliff |
| bounded certificate weight | per-seat multiplier clipped to [1/1.5, 1.5] | 2× θ_eff costs 5.25 pts [2.45, 8.05]; 4× loses the match outright |
| no corner solutions | every admitted type keeps ≥ `typeFloor` = 0.02 mass | P3 §6.3.4 |
| declarations not unlocked | `m7DeclWeight = 0`, **and a second posterior** `belDecl` for the declaration path | P3 §6.3.5. Gating the final `pAlloc` is not enough: `feasibleAllocation` and `bestGuess` *choose* the allocation from `bel.marg`, so a tilted marginal steers which allocation is proposed even when its confidence is scored untilted. Found by measurement, then fixed |
| the prior is not deleted | M7 shrinks toward v0.4's prior, never toward none | P3 §4 — the policy-agnostic posterior is 4.40 points [1.60, 7.15] **worse** |
| silence cannot mislead a silent seat | episodes are opened only by a public act; no act ⇒ n = 0 ⇒ tilt 0 | the brief's requirement, satisfied structurally rather than by tuning |
| self-correcting against the manoeuvre | the tilt is `log(pNever)` under the seat's *current* mixture; as that mixture moves toward `withholder` (pNever 0.503) the tilt decays toward 0 | §3.1: `log(.017) = −4.07` for an honest type against `log(.503) = −0.69` for the withholder — the channel switches itself off, without ever asserting the opposite |
| the copy has not drifted | `fish m7check` requires `fitTilted(model off) == Belief::sinkhornDisj` **exactly** | 1,054 + 698 + 840 states, max difference `0` at three configurations |

`belief.hpp` is frozen so that v0.4 stays byte-identical as the reference opponent, so `fitTilted`
is a structural copy of `sinkhornDisj` with a generalised initialiser. `m7check` is the guard that
makes the duplication safe; it should be added to the KPI set M10 gates commits on.

---

## Appendix — files and commands

New, unprotected:

* `engine/src/v05_oppmodel.hpp` — the mechanism. Namespace `fish::m7`.
* `research/v05/patches/M7.patch` — the wiring. **Not applied.** `git apply --check` passes at the
  commit this was written against.

Ablation switches added by the patch (all through `factory.hpp`, no existing line altered):

```
m7=0        master switch; m7=0 reproduces the unpatched v0.5 exactly (verified, two seeds)
m7ask=W     tilt strength on the ask path            (default 0.30, derived — see §5)
m7decl=W    tilt strength on the declaration path    (default 0.00)
m7bias=N0   Johanson-Bowling data-bias               (default 2.0) -- the robustness dial
m7cap=Z     |z_M7| clip                              (default 0.90)
m7tcap=M    per-seat certificate multiplier bound    (default 1.50)
m7carry=C   type-posterior retention between deals   (default 0.70; needs --rotations=1)
m7sil=0     ablate the silence channel alone
m7th=0      ablate the per-seat certificate weight alone
```

Divergence guard:

```
./fish m7check --games=30 --seed=20260822 --a=v05:m7=0
./fish m7check --games=20 --seed=777      --a=v05
./fish m7check --games=20 --seed=777      --a=v04
```

The §1–§4 measurements were produced by five out-of-tree harnesses compiled against the unmodified
engine. They and their raw output are committed:

```
research/v05/runs/M7/m7_algebra_replica.cpp        §1.2 table 1 -- pluggable-initialiser replica
research/v05/runs/M7/m7_algebra_totalsupport.cpp   §1.2 table 2 -- DealDP total-support split
research/v05/runs/M7/m7_episode_census.cpp         §2, §3.1    -- divergence guard + episode census
research/v05/runs/M7/m7_clip_census.cpp            §1.2        -- how often the ±2.6 clip binds
research/v05/runs/M7/m7_feint_statistics.cpp       §3.2        -- breadth / cards-held-at-ask
research/v05/runs/M7/m7_online_model.cpp           §4          -- online-model driver
research/v05/runs/M7/M7-raw.txt                    all raw output, including the §5 sanity grid

clang++ -std=c++20 -O2 -I engine/src research/v05/runs/M7/<file>.cpp -o /tmp/h -pthread
```

They should be promoted to `engine/src/probe_m7.hpp` alongside a `fish m7profile` command when M10
refits the type table.
