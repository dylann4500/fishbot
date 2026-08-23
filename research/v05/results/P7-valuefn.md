# P7 — The value function and the fitting pipeline

Scope: the 16-coefficient linear value function `V` (`V04Agent::value` /
`stateFeatures`, `engine/src/v04.hpp:370-431`), the ridge fit that is supposed to
produce its coefficients (`fish fitvalue`, `engine/src/main.cpp:480`), and the
cross-entropy fitting objective for the 34 policy parameters
(`engine/src/tuner.hpp`, `engine/freeze_config.py`, `engine/experiments.sh`).

New scratch code (nothing on any decision path):
`engine/src/probe_valuefn.hpp`, plus two appended commands in `main.cpp`
(`fish dumpvalue`, `fish shadow`).
Offline analysis scripts and raw outputs: `analyze2.py`, `analyze3.py`,
`ablate.jsonl`, `analyze2.out`, `analyze3` output, in the P7 scratch directory
(`/private/tmp/claude-501/.../scratchpad/p7`).

---

## Executive summary

1. The documented gap is real — the compiled `vw` is not the E14 vector — but the
   direction is the opposite of what the note implies. **The compiled vector is
   better than the fitted one, everywhere.** Swapping in `E14-valuefit.txt`
   costs 1.7–3.2 win-rate points against every opponent tested, including
   −2.9 pts in mirror play. Every cluster-bootstrap CI excludes zero.
   *Hypothesis "the shipped coefficients are a bug that costs strength" did NOT hold.*
2. The value function is, to measurement precision, **one feature**. Held-out
   (game-clustered) R² of the full 16-feature model is **0.2327**; of
   `bias + score differential` alone it is **0.2354** — *higher*. The other 14
   features contribute nothing and slightly hurt.
3. Two of the 16 features are **algebraically the same feature**
   (`expected control` ≡ `card differential`; corr = 0.999996, the residual is
   Sinkhorn error). Five more (`bias`, `my hand size`, `smallest friendly hand`,
   `our near-complete`, `their near-complete`) **cancel identically at every call
   site**, so their coefficients cannot change any decision:
   measured 0 / 31,788 declaration decisions change when all five are moved to ±9.
   This is a repeat of the v0.4 dead-feature bug in a new place.
4. Linear is *not* enough. A depth-3 gradient-boosted tree on exactly the same
   rows reaches held-out R² **0.388** vs **0.233** linear (+67 % relative).
   A quadratic expansion only reaches 0.250. The gain is in the interactions a
   tree finds, not in pairwise products.
5. The label is fine on the adjudication question — **0 %** of training rows come
   from games that hit the ask cap. The real label/state defect is elsewhere:
   rows are collected **only at ask decision points**, never at declaration
   points, yet `declareByValue` evaluates `V` at declaration points; and held-out
   R² is only **+0.168** in the ≥160-event tail — i.e. `V` is at its *least*
   informative exactly in the deadlocked positions v0.5 must fix.
6. Splitting the value function by use: in the mirror, deleting the ask-side
   expectimax entirely is worth **+0.006, CI [−0.016, +0.029]** (nothing), while
   reverting declarations to fixed thresholds costs **+0.025, CI [+0.004,
   +0.047]**. The part of `V` that earns its keep is the declaration rule; the
   ask-side use — the side the P0 deadlock lives on — does not.
7. **The mirror is absent from the fitting panel in every recorded round.**
   Panels were `v03, lockout, detective, v02` (+2 more in rounds 3–5); the
   per-opponent win rates recorded across all generations of rounds 3, 4 and 5
   never fall below 0.664, so no ~0.5 self-play opponent was ever present. The
   soft-min temperature is β = 10, which over a panel whose win rates span only
   ~0.06 gives a max/min gradient weight ratio of **1.9** — the objective is much
   closer to a weighted mean than to a minimum.

---

## 1. Shipped vs fitted value coefficients

### 1.1 The vectors differ

`V04Config::vw` (`engine/src/v04.hpp:111-127`) vs
`research/v04/results/E14-valuefit.txt`:

| j | feature | compiled `vw` | E14 fit | Δ |
|---|---|---|---|---|
| 0 | bias | +0.001242 | +0.007023 | +0.005781 |
| 1 | score differential | +0.888965 | +0.909779 | +0.020814 |
| 2 | expected control | +0.421266 | +0.115403 | **−0.305863** |
| 3 | sharpened control | −0.145791 | −0.249275 | −0.103484 |
| 4 | locked differential | +0.225573 | +0.011511 | **−0.214062** |
| 5 | side to move | +0.022896 | +0.026911 | +0.004015 |
| 6 | card differential | +0.422207 | +0.114251 | **−0.307956** |
| 7 | unresolved pool | +0.007678 | −0.079727 | −0.087405 |
| 8 | active half-suits | +0.005904 | +0.035886 | +0.029982 |
| 9 | turn × control | −0.000997 | −0.003278 | −0.002281 |
| 10 | my hand size | −0.006601 | −0.011850 | −0.005249 |
| 11 | smallest friendly hand | −0.007472 | +0.062496 | +0.069968 |
| 12 | our near-complete | −0.022484 | +0.005226 | +0.027710 |
| 13 | their near-complete | −0.025189 | −0.034832 | −0.009643 |
| 14 | contested mass | +0.080416 | +0.027557 | −0.052859 |
| 15 | turn × unresolved | −0.021409 | −0.019608 | +0.001801 |

0 of 16 coordinates agree. ‖compiled‖ = 1.108, ‖E14‖ = 0.965,
‖compiled − E14‖ = 0.512, cosine = 0.887.

Mechanism, confirmed at source: `engine/freeze_config.py:17-42` rewrites only
`double w[NFEAT] = { … }` (the 20 ask weights) and then calls `setval` on 14
named scalars. It never touches `vw`. The paper already discloses this
(`paper/sections/C-parameters.tex:190-240`); what was never measured is the cost.

### 1.2 Matched head-to-head, both vectors

`./fish ablate --ref=v04 --variants="v04:vweights=<E14>" --panel=<one opponent>
 --games=500 --rotations=6 --seed=7788991`, run once per opponent so each CI is
per-opponent. Paired bootstrap resamples **deals** (clusters of 6 rotations),
`arena.hpp:127-146`. Seed 7788991 is disjoint from every fitting bank
(20260821 / 770077 / 313131 / 888111 / 1357911) and from the E1–E17 banks.

| opponent | compiled `vw` | E14 `vw` | Δ (compiled − E14) | 95 % CI |
|---|---|---|---|---|
| **v04 (mirror)** | 0.5000 | 0.4710 | **+0.0290** | [+0.0143, +0.0433] |
| v03 | 0.7510 | 0.7307 | +0.0203 | [+0.0050, +0.0360] |
| lockout | 0.7830 | 0.7650 | +0.0180 | [+0.0013, +0.0343] |
| detective | 0.7470 | 0.7247 | +0.0223 | [+0.0077, +0.0370] |
| v02 | 0.8327 | 0.8140 | +0.0187 | [+0.0043, +0.0330] |
| diversifier | 0.9283 | 0.8960 | +0.0323 | [+0.0200, +0.0447] |
| hunter | 0.9723 | 0.9557 | +0.0167 | [+0.0090, +0.0243] |
| bluffer | 1.0000 | 0.9990 | +0.0010 | [+0.0000, +0.0023] |
| random | 1.0000 | 0.9967 | +0.0033 | [+0.0013, +0.0057] |

(500 deals × 6 rotations = 3,000 games per cell; the mirror row's reference arm
is v04 vs v04, exactly 0.5000 by symmetry.)

**Worst case across styles: the E14 vector is worse everywhere; it is never
better against any opponent, and the largest loss is in mirror play.**

Interpretation: the 34 policy parameters were fitted by CEM *with the compiled
`vw` in the loop* (`tuner.hpp:47-64` calls `weightSpec` on the 34-vector only,
so `vw` is whatever `V04Config` holds). `valueWeight = 6.043`,
`linearWeight = 0.767` and `declareMargin = −0.034` are therefore co-adapted to
the compiled vector's *scale*. The E14 vector has a much smaller
`w2 + w6` (0.2297 vs 0.8435), so substituting it silently shrinks the
expectimax term relative to the linear ask score. The gap is a real
reproducibility defect, but "fix it by pasting the fitted numbers in" is
measurably the wrong repair.

---

## 2. Audit of the value function itself

Data: `fish dumpvalue --a=v04 --b=v04 --games=350 --rotations=2 --seed=20260822`
→ 551,484 rows from 700 mirror games (held-out seed). Rows carry a game id, so
every held-out number below uses a **game-clustered** 5-fold split. Row-shuffled
CV inflates R² from 0.2327 to 0.2479.

### 2.1 How much outcome variance does it explain?

| model | held-out R² | rmse |
|---|---|---|
| in-sample ridge, same estimator as `fish fitvalue` | 0.2479 (in-sample) | 0.3369 |
| 16-feature linear, refit, game-clustered CV | **0.2327** | 0.3403 |
| compiled `vw` as shipped, no refit | 0.2299 | — |
| E14 `vw`, no refit | 0.2355 | — |
| bias only | 0.0000 | — |
| **bias + score differential only** | **0.2354** | — |
| bias + score differential + side to move | 0.2368 | — |

For reference, `E14-valuefit-stats.txt` reports `rows=405348 R2=0.2909
rmse=0.3047` — that is an **in-sample** R² (`main.cpp:527-535` computes SSE on
the same rows it fitted), and on a different bank; my in-sample figure on this
bank is 0.2479.

**The headline: a two-parameter model (`bias`, `score differential`) beats the
full 16-feature model out of sample.** Drop-one confirms it: removing `score
differential` collapses held-out R² from +0.2327 to **−0.0044** — all fifteen
remaining features together explain literally none of the outcome variance.
Every other drop-one loss is ≤ 0.0021, and four features (`active half-suits`,
`turn × control`, `our near-complete`, `their near-complete`) have a
*non-negative* drop-one loss.

```
drop score differential       R2=-0.00440  (loss -0.237121)
drop smallest friendly hand   R2=+0.23062  (loss -0.002101)
drop sharpened control        R2=+0.23219  (loss -0.000526)
drop side to move             R2=+0.23220  (loss -0.000518)
drop card differential        R2=+0.23251  (loss -0.000205)
drop expected control         R2=+0.23255  (loss -0.000173)
drop bias                     R2=+0.23260  (loss -0.000116)
drop turn x unresolved        R2=+0.23261  (loss -0.000104)
drop contested mass           R2=+0.23265  (loss -0.000073)
drop locked differential      R2=+0.23266  (loss -0.000063)
drop unresolved pool          R2=+0.23267  (loss -0.000049)
drop my hand size             R2=+0.23268  (loss -0.000038)
drop turn x control           R2=+0.23272  (loss +0.000000)
drop active half-suits        R2=+0.23274  (loss +0.000018)
drop their near-complete      R2=+0.23278  (loss +0.000060)
drop our near-complete        R2=+0.23285  (loss +0.000127)
```

This does **not** mean `V` is useless in the policy — the policy uses
*differences* of `V` under hypothetical perturbations, not its absolute level,
and `score differential` is constant across the candidate asks at a state, so
the terms the ask rule actually consumes are precisely the ones that carry no
outcome information in this regression. That is the finding: **the fitted signal
and the used signal are disjoint.**

### 2.2 Is linear enough? (offline only, nothing built into the engine)

Same rows, same game-clustered folds.

| model | held-out R² | Δ vs linear |
|---|---|---|
| 16-feature linear | 0.2327 | — |
| quadratic (all pairwise + squares, 136 cols), λ = 1e-3 | 0.2335 | +0.001 |
| quadratic, λ = 1e-4 | 0.2446 | +0.012 |
| quadratic, λ = 1e-5 | 0.2498 | +0.017 |
| **GBT, depth 3, 200 iters, lr 0.1** (250k-row subsample) | **0.3880** | **+0.155** |

A quadratic expansion is not worth the engineering: +0.017 R². A shallow
gradient-boosted tree is a different story — **+0.155 R², a 67 % relative
improvement**, from a model with 200 depth-3 trees over the same 15 features.
Whatever `V` needs is a non-additive, threshold-shaped function of the existing
features, which a 16-term linear form cannot represent no matter how it is
fitted. (Engineering caveat: `value()` is called on the inner loop for every
candidate ask, so a tree ensemble would have to be either cheap to evaluate or
distilled; that is a v0.5 design decision, not a measurement.)

### 2.3 Dead features — the v0.4 bug repeats

Three distinct failure modes, all present.

**(a) One feature is duplicated exactly.** `f[2]` and `f[6]`:

```
v04.hpp:382  f[2] = control / 9.0;                          // control = Σ_active (2 e_s − 1)
v04.hpp:386  f[6] = double(ourCards + dOur - theirCards - dTheir) / 54.0;
 (and the same pair at v04.hpp:411 / v04.hpp:415 inside stateFeatures)
```

`control = Σ_s (2 e_s − 1) = (1/3)·E[our cards] − active`, and every card of an
active half-suit is in someone's hand so `ourCards + theirCards = 6·active`,
giving `f[6] = (ourCards/3 − active)/9`. Under any posterior consistent with the
public hand counts, `E[our cards] = ourCards`, so **`f[2] ≡ f[6]`**. Measured on
551,484 rows: `corr = 0.9999959`, `mean |f2 − f6| = 7.9e-5`,
`max |f2 − f6| = 4.5e-3` — the residual is the Sinkhorn approximation failing to
enforce C4 exactly. Only `w2 + w6` is identified; the split between them is
arbitrary. (Compiled sum 0.8435, E14 sum 0.2297.)

**(b) Five coefficients cannot change any decision.** `cfg.vw` is read in exactly
one place (`v04.hpp:373`), and `value()` is called at exactly five sites:
`v04.hpp:454-455` (`vHit`, `vMiss` inside `askExpectedValue`) and
`v04.hpp:665-669` (`vRight`, `vWrong`, `vWait` inside `declareByValue`). At all
five, the hypothetical perturbation arguments (`dControl, dSharp, dLocked,
dContested, scoreDiff, turnSign, dOur, dTheir, dUnresolved, dActive`) never touch
`f[0]`, `f[10]`, `f[11]`, `f[12]`, `f[13]` — those are recomputed from
`agg`/`pub` and are identical across the branches being compared. And
`chooseAsk` (`v04.hpp:474-482`) is a pure argmax over candidates *at one state*,
where those five features are also identical across candidates. So
`vw[0], vw[10], vw[11], vw[12], vw[13]` are inert by construction.

Verified with a shadow-agent probe (`fish shadow`, 40 mirror games, seed 99001,
31,788 declaration decisions and 4,927 ask decisions, shadow policies fed the
identical public history):

```
v04:v0=9                            decl-decisions 0/31788 differ (0.0000%)
v04:v0=-9                           decl-decisions 0/31788 differ (0.0000%)
v04:v10=9                           decl-decisions 0/31788 differ (0.0000%)
v04:v11=9                           decl-decisions 0/31788 differ (0.0000%)
v04:v12=9                           decl-decisions 0/31788 differ (0.0000%)
v04:v13=9                           decl-decisions 0/31788 differ (0.0000%)
v04:v0=9,v10=9,v11=9,v12=9,v13=9    decl-decisions 0/31788 differ (0.0000%)
--- controls (coefficients that are NOT inert) ---
v04:v1=0.5                          decl-decisions 97/31788 differ (0.3051%)
v04:v2=0.5                          decl-decisions 9/31788 differ (0.0283%)
v04:v6=0.1                          decl-decisions 233/31788 differ (0.7330%)
v04:v8=0.5                          decl-decisions 104/31788 differ (0.3272%)
```

Moving five coefficients by a factor of ~1,000–7,000 changes **zero** of 31,788
declaration decisions, while a 0.5 change to `v1` changes 97. Ask decisions do
move (5.6 % for `v0=9`), but that is float tie-breaking, not preference: a
relative change of 1e-13 to the *same* coefficient already flips 0.41 % of ask
decisions, and a relative change of 1e-7 to `v2` flips 0.35 %.

```
v04:v0=0.001242            (identical)  ask 0/4927 differ (0.0000%)
v04:v0=0.0012420000001                  ask 20/4927 differ (0.4059%)
v04:v0=0.001243                         ask 14/4927 differ (0.2841%)
v04:v2=0.4212661                        ask 17/4927 differ (0.3450%)
```

Mirror positions are full of provably-dead asks with identical feature vectors,
so exact/near ties in the ask utility are common and any perturbation reshuffles
them. This also means whole-game win-rate ablations of individual `vw`
coefficients are uninterpretable at this sample size: the trajectory is chaotic.
A held-out `ablate` control confirms the noise floor —
`v04:v0=0.001243` (a 1e-6 absolute change to an inert coefficient) measured
Δ = −0.00396, CI [−0.00958, +0.00167] over 400 deals × 6 rotations.

**(c) Near-zero-variance features.** `locked differential` (`f[4]`) is nonzero in
only **1.33 %** of the 551,484 rows (sd 0.0128, the smallest of any feature) yet
carries the third-largest compiled coefficient, +0.2256. `turn × control`
(`f[9]`) has drop-one loss exactly 0.000000. Three features are ≥ 0.96
correlated with each other (`unresolved pool` / `active half-suits` /
`contested mass`), and `side to move` / `turn × unresolved` correlate at 0.911.

Summary of the 16: 1 genuinely load-bearing for the regression
(`score differential`), 1 exact duplicate pair (`expected control` ≡
`card differential`), 5 structurally inert (`bias`, `my hand size`,
`smallest friendly hand`, `our near-complete`, `their near-complete`), 1 near
degenerate (`locked differential`).

### 2.4 Is the label right?

Label (`engine/src/game.hpp:371-375`):

```cpp
double diff = (double(res.score[0]) - double(res.score[1])) / double(rules.deckSets);
for (int i = vsinkStart; i < int(vsink->y.size()); i++)
  vsink->y[i] = float(vsink->team[i] == 0 ? diff : -diff);
```

**The adjudication worry does not hold.** Of 551,484 rows, **0.000 %** come from
a game that hit the ask cap (`adjudicateRemaining`, `game.hpp:271-284`) — the
graduated declaration pressure terminates every mirror game through play, which
matches `P0-v04-pathology.md` (`action-limit games 0 (0%)`). 3.96 % of rows come
from games containing at least one forced-endgame declaration
(`forcedEndgame`, `game.hpp:234-268`), and held-out R² on that subset is
**+0.2509**, slightly *higher* than on the rest (+0.2322). The forced-endgame
fallback that awards a half-suit by physical majority (`game.hpp:259-267`) is
reached only when every agent refuses the willingness ladder, and does not
measurably distort the label.

Three label/state problems that *do* hold:

1. **The state distribution excludes the states `V` decides.** Rows are pushed
   only immediately before an ask, and only when the mover has cards
   (`game.hpp:310-323`). No row is ever collected at a declaration decision
   point, at a turn-pass, or during the forced endgame — yet `declareByValue`
   (`v04.hpp:653-671`) evaluates `V` at exactly those declaration points. `V` is
   fitted on one distribution and used on another.
2. **`V` is weakest where v0.5 needs it.** Held-out R² by decision-point index:

   | events into the game | n | held-out R² |
   |---|---|---|
   | 0–5 | 21,000 | −0.0022 |
   | 5–10 | 21,000 | +0.0136 |
   | 10–20 | 42,000 | +0.0611 |
   | 20–40 | 84,000 | +0.1541 |
   | 40–80 | 165,372 | +0.3694 |
   | 80–160 | 110,712 | +0.3172 |
   | **160+** | 107,400 | **+0.1680** |

   The deadlock tail (≥160 events, 19.5 % of rows) is the second-worst regime.
3. **The training set is skewed toward deadlocked games.** Rows per game:
   mean 788, median 576, p90 1,824, max 1,872. The longest 10 % of games supply
   **23.4 %** of all rows, so the fit is disproportionately shaped by exactly the
   pathological trajectories the value function fails to evaluate.

Two smaller notes. The label has no zero (9 half-suits ⇒ the differential is
odd), distributed ±0.111 (21.1 % each) … ±1.0 (0.67 % each). And using
`sign(final differential)` instead — the win/loss target — gives held-out
R² 0.1464, i.e. the continuous set differential is the better-behaved target of
the two; keep it.

### 2.5 Does `V` matter at all — in the mirror?

E5 (`research/v04/results/E5-ablations.json`) tested `value=0` and `vdecl=0`
against `v03, lockout, detective, v02` only, and found nothing:
`value=0` Δ = −0.00325, CI [−0.01875, +0.01225]; `vdecl=0` Δ = +0.00125,
CI [−0.01225, +0.01475]. Neither was ever run in the mirror. Run here:
`./fish ablate --ref=v04 --panel=v04 --games=250 --rotations=6 --seed=606061`
(1,500 games per arm, paired bootstrap over deals):

| variant | variant win rate vs v04 | Δ (ref − variant) | 95 % CI |
|---|---|---|---|
| `v04:value=0` — no expectimax over `V` at all | 0.4940 | +0.0060 | [−0.0160, +0.0287] |
| `v04:vdecl=0` — keep `V` in asks, revert declarations to fixed thresholds | 0.4747 | **+0.0253** | **[+0.0040, +0.0467]** |

Two readings, and they split the value function in half:

- **The ask-side use of `V` is worth nothing, in the mirror as well as against
  the panel.** Deleting the entire one-ply expectimax (`value=0` also removes it
  from declarations) is within noise of zero at 1,500 games.
- **The declaration-side use of `V` is worth about +2.5 points in the mirror**,
  and was worth nothing against the baseline panel. That is a
  mirror-only effect that the fitting objective could not have seen (§3.3).

Caveat: 250 deals is a small bank and the two rows are not mutually consistent at
face value (`value=0` removes strictly more than `vdecl=0` does yet measures
smaller); read them as "ask-side ≈ 0, declaration-side positive but imprecisely
measured", and re-run both at ≥1,000 deals before relying on the magnitude.

---

## 3. The fitting objective

Source: `engine/src/tuner.hpp` (whole file), `engine/src/main.cpp:174-221`
(`fish tune`), `engine/experiments.sh:49`, `engine/freeze_config.py`,
`research/v04/runs/`.

### 3.1 What is optimised

Cross-entropy method with a diagonal Gaussian over a 34-coordinate vector:
population 24, elite 6, 40 generations, σ₀ = 0.6, σ floor 0.03, smoothing 0.6
(`tuner.hpp:19-31`, `main.cpp:178-183`). Common random numbers within a
generation (`tuner.hpp:55`), fresh seed bank per generation (`tuner.hpp:70`).
The scored vector is `v04:allparams=…` (`tuner.hpp:34-44` → `factory.hpp:91-118`),
and `freeze_config.py` bakes the same 34 numbers back in. **`vw` is not in the
optimised vector and not in the freeze step** — see §1.

### 3.2 Soft-minimum temperature

```cpp
// tuner.hpp:61-63
acc += std::exp(-sp.beta * wr);
…
e.score = -std::log(acc) / sp.beta;
```

`beta = 10.0` (`tuner.hpp:23`, `main.cpp:180` default `"10"`). The gradient
weight on opponent *o* is `∝ exp(−β·wr_o)`. On the selected generation's own
profile (`research/v04/runs/selected.json`: 0.756 / 0.802 / 0.765 / 0.819) that
gives normalised weights **0.326 / 0.205 / 0.297 / 0.173** — a max/min ratio of
**1.9**. With a panel whose win rates span only ~0.06, β = 10 is a mild tilt, not
a minimum. The comment's claim that it "converges to `min_o`" is true in the
limit but is not what β = 10 is doing at this spread. To get a genuine worst-case
objective at this spread you need β on the order of 10²–10³.

### 3.3 Is the panel balanced, and is the mirror in it?

**The mirror is not in the panel. It never was.**

- `fish tune` default panel: `v03,lockout,detective,diversifier` (`main.cpp:176`).
- `experiments.sh:49` (E5 ablations) panel: `v03,lockout,detective,v02`.
- `research/v04/runs/selected.json`: `["v03","lockout","detective","v02"]`,
  validation seed 1357911, 500 deals.
- `research/v04/runs/README.md`: round 1 four-opponent panel, rounds 2–5
  six-opponent panels.
- Direct check on the traces: the minimum per-opponent win rate over **all**
  generations is 0.6636 (round 3), 0.6682 (round 4), 0.7273 (round 5). A mirror
  opponent would sit at ≈ 0.5 by construction, and no recorded generation has any
  opponent below 0.66. Round 5 generation 0 reads
  `winRates: [0.7409, 0.7955, 0.7636, 0.8409, 0.9, 0.9909]`.

The panel is also unbalanced in strength: every member is beaten by 72–99 %, so
the objective spends most of its resolution on opponents v0.4 already dominates.
The two weakest (`bluffer`, `random`) are held out of fitting, but `hunter` at
0.97 and `diversifier` at 0.93 remain in rounds 2–5 and contribute almost no
gradient.

The legacy TypeScript optimisers have the same shape and the same omission:
`scripts/optimize-fishbot.ts:28-34` weights `detective .35 / lockout .25 /
fishbot_v02 .20 / diversifier .12 / hunter .08`; `scripts/refine-fishbot.ts:7-11`
uses `detective .4 / lockout .35 / fishbot_v02 .25`. No self-play term in either.

**This is a direct explanation for the P0 pathology.** The deadlock is a
mirror/strong-opponent phenomenon (P0: 39.0 % provably-dead asks and a
286-ask dead run in the mirror, versus 2.8 % and 5 against v0.3). Nothing in the
34-parameter objective ever evaluated the policy against itself, so no
generation of CEM was ever penalised for it: a candidate that deadlocks in
self-play but converts against v0.3/lockout/detective/v02 scores exactly as well
as one that does not. The pathology was not traded away — it was never priced.

Note the value fit *was* run on self-play (`experiments.sh:116`,
`--a="$V04" --b="$V04"`), unlike the policy fit. But (i) that artifact is not the
vector that ships (§1), and (ii) its output is a regression target, not a policy
objective, so it cannot penalise a deadlock either.

---

## 4. Consequences for v0.5

Ranked by measured leverage.

1. **Put the mirror in the fitting panel.** It is a one-token change
   (`--panel=v04,v03,lockout,detective,v02`) and it is the only recorded reason
   the deadlock survived fitting. Any v0.5 ask-rule change must be scored against
   a self-play opponent or the same blindness recurs.
2. **Raise β, or switch to an explicit worst case.** At β = 10 and a 0.06 spread
   the "soft minimum" is a 1.9:1 weighted mean. If the mirror joins the panel its
   win rate will be ≈ 0.5 against a panel at 0.75–0.99, and β = 10 will then
   weight it ≈ 12× the best member — which is probably the intent, but it should
   be a decision, not an accident of the spread.
3. **Do not "fix" `vw` by pasting in `E14-valuefit.txt`** — measured −1.7 to
   −3.2 win-rate points, mirror included. If `vw` is to be refitted it must be
   refitted *jointly with* `valueWeight`, `linearWeight` and `declareMargin`, or
   at minimum rescaled so `w2 + w6` matches. Better: put the 16 value
   coefficients into the CEM vector so the freeze step covers them and the
   discrepancy cannot recur.
4. **Delete or repair the dead features before adding new ones.** Merge
   `expected control` with `card differential` (they are one feature); drop or
   re-express `bias`, `my hand size`, `smallest friendly hand`,
   `our near-complete`, `their near-complete` (structurally inert as used); and
   re-express `locked differential`, which fires in 1.3 % of states. A 16-term
   linear form with 7 wasted terms is a 9-term model.
5. **Collect value rows at declaration and pass decision points too.** `V` is
   consumed at states it has never been fitted on. This is a change to
   `game.hpp:310-323`, which is a protected header — flag it, do not make it here.
6. **Spend the value-function budget on declarations, not on asks.** §2.5: in the
   mirror, deleting the expectimax entirely is worth 0.006 ± 0.022, while
   reverting declarations to fixed thresholds costs 0.025 [+0.004, +0.047]. The
   ask-side use of `V` is the part that is not earning its keep — and the ask
   rule is exactly where the P0 deadlock lives.
7. **If `V` is to matter more, it needs a non-linear form.** Held-out R² 0.233 →
   0.388 from a depth-3 GBT on the identical rows. Worth doing only after (5) —
   fitting a better predictor on a state distribution that excludes the states it
   is used at will not help.

---

## 5. Reproduction

```bash
cd engine && make

# §1.2 — matched shipped vs fitted vw, per opponent
FIT=0.007023\|0.909779\|0.115403\|-0.249275\|0.011511\|0.026911\|0.114251\|-0.079727\|0.035886\|-0.003278\|-0.011850\|0.062496\|0.005226\|-0.034832\|0.027557\|-0.019608
for opp in v04 v03 lockout detective v02 diversifier hunter bluffer random; do
  ./fish ablate --ref=v04 --variants="v04:vweights=$FIT" --panel=$opp \
     --games=500 --rotations=6 --seed=7788991
done

# §2 — dump value rows with game ids (new command, probe_valuefn.hpp)
./fish dumpvalue --a=v04 --b=v04 --games=350 --rotations=2 --seed=20260822 --out=rows.csv
python3 analyze2.py ; python3 analyze3.py     # scratch scripts, numpy + scikit-learn

# §2.3 — shadow-agent decision-equality probe (new command)
./fish shadow --base=v04 --games=40 --seed=99001 \
  --variants="v04:v0=9;v04:v10=9;v04:v11=9;v04:v12=9;v04:v13=9;v04:v1=0.5;v04:v2=0.5;v04:v6=0.1"

# §2.5 — is the value function worth anything in the mirror?
./fish ablate --ref=v04 --panel=v04 --games=250 --rotations=6 --seed=606061 \
  --variants="v04:value=0;v04:vdecl=0"
```

Seeds 7788991, 20260822, 99001, 515253, 606061 are disjoint from every fitting bank
(20260821, 770077, 313131, 888111, 1357911) and from the E1–E17 banks
(90210, 515151, 606060, 717171, 828282, 838383, 848484, 31415).
