# FishBot v0.6 — results summary

Engine: `engine/src/v06.hpp`, shipped vector frozen from `research/v06/runs/fitC.jsonl`
(`sha256:be4a4c31d9153208`, 14 generations, objective **minimax regret**, paired, panel
`v05,v03,withholder,feint`, 200 deals × 2 rotations per cell, fitting seed 20260824, seeded from
`fitA` at seed 20260823). All evaluation banks are disjoint from both fitting seeds.

## The identity control

`v06:legacy=1` — v0.6 carrying v0.5's parameter vector with every mechanism off — produces a
diagnostic transcript **identical to v0.5's under md5**. The ablation table is therefore exact.

## Commit gate: mirror pathology, 600 games per arm, seed 31

| KPI | v0.4 | v0.5 | **v0.6** |
|---|---:|---:|---:|
| provably dead asks | 39.04% | 0.0114% | **0.0117%** |
| longest dead run | 286 | 1 | **1** |
| games with a dead run ≥ 6 | 34.33% | 0% | **0%** |
| declarations wrong | 10.44% | 2.07% | **2.37%** |
| ask hit rate | 34.25% | 55.63% | **53.29%** |
| events/game | 143.60 | 96.72 | **94.59** |
| games killed by the action limit | 0% | 0% | **0%** |

v0.6 passes the gate. Note that its mirror hit rate is *lower* than v0.5's while it wins more: the
refit trades immediate hit probability for half-suit control, and the games are two events shorter.

## Head to head against v0.5: NOT separated from parity

This is stated first because an earlier draft of this summary got it wrong, and the correction is
the most important one an adversarial re-read produced.

The five E3 banks all show v0.6 ahead (51.44, 50.44, 51.33, 50.11, 51.83; mean 51.03% over 9,000
games). But the battery contains **two further head-to-head cells at default rules on further
disjoint banks**, and v0.6 loses both: E7's default-dialect row at seed 828282 is **48.40%**
(n = 1,500) and E5's v0.5 panel cell at seed 606060 is **48.88%** (n = 800).

Pooling every v0.6-vs-v0.5 cell in the battery: **50.53% over 11,300 games, 5 of 7 cells above
parity, naive z = 1.13** — and that z ignores the correlation between the six rotations of a deal,
so it overstates the significance rather than understating it. Against v0.4 the five E3 banks give
50.86%.

**The correct statement is that v0.6 and v0.5 are not separated head to head.** Reporting the five
E3 banks alone would have been selection on the experiment that agreed.

## Per-style profile and the worst case, pooled over three disjoint banks

300 deals x 6 rotations per cell at seeds 515253, 90210 and 424242 (4,800 games per cell for the
styles measured at all three; 1,800 for the rest).

| opponent | **v0.6** | v0.5 | v0.4 | games |
|---|---:|---:|---:|---:|
| v0.5 | 51.00 | 50.00 | 48.39 | 4,800 |
| v0.4 | 50.00 | 50.94 | 50.00 | 4,800 |
| v0.3 | 76.06 | 73.71 | 73.33 | 4,800 |
| v0.2 | 80.00 | 81.72 | 83.06 | 1,800 |
| lockout | 78.25 | 79.50 | 77.94 | 4,800 |
| detective | 76.71 | 75.88 | 77.28 | 4,800 |
| diversifier | 95.44 | 94.11 | 92.89 | 1,800 |
| hunter | 98.44 | 97.67 | 97.67 | 1,800 |
| bluffer | 99.83 | 99.89 | 99.94 | 1,800 |
| random | 100.00 | 100.00 | 100.00 | 1,800 |
| silent (deception) | 83.87 | 81.96 | 78.89 | 4,800 |
| feint (deception) | 53.37 | 53.44 | 51.11 | 4,800 |
| **withholder** (deception) | **79.15** | 70.54 | 67.50 | 4,800 |
| **worst case** | **50.00** | **50.00** | 48.39 | |
| mean (descriptive only) | 78.63 | 77.64 | 76.77 | |
| **minimax regret over the set** | **3.06** | **8.60** | **11.65** | |

v0.6 is ahead of v0.5 on **7** of thirteen styles, behind on **5** and equal on **1**, with a
largest single-style loss of **1.72** points.
Its worst cell equals v0.5's (v0.5's worst is its own mirror, which is 50% by construction).
**Minimax regret 3.06 against 8.60** — a factor of 2.8 on the criterion this project says it
optimises.

**The single large cell is the withholder**, the deception manoeuvre the project owner brought here
from live play: **+8.60 points over 4,800 games, replicating at all three banks (+9.06, +8.33,
+8.34)**. Two qualifications travel with it. Minimax regret here is measured against the best of the
three arms on each opponent, so it is relative and rewards whichever arm is best most often; and
delete the withholder column and the three arms converge. The withholder was in the fitting panel of
both fits, and buying robustness against a panel member is what a minimax-regret objective is for.

## Test-time search: what it is worth, and where that is not resolved

A determinized information-set search — the deal sampled from the exact posterior, the six
continuation players reconstructed at their own information sets rather than made clairvoyant — was
built, and it is the one place in this study where a multi-step method beats a static rule.

| configuration | win rate vs v0.5 | n |
|---|---:|---:|
| blueprint-forced control (same code, same budget, same seed) | 49.31% [45.83, 52.78] | 720 |
| **unguarded argmax over the rollout means**, det=8 | **13.61%** [10.97, 16.39] | 720 |
| unguarded argmax, det=12 | 23.33% [20.42, 26.39] | 720 |
| paired LCB deviation rule, kappa=1 | 42.78% [39.31, 46.25] | 720 |
| **paired LCB, kappa=2.5** | **53.75 / 52.08 / 52.08**, pooled **52.64%** | 2,160 |
| paired LCB, kappa=4 | 53.89 / 51.81 / 50.56 | 2,160 |
| the same on the v0.6 vector, vs **v0.6** (4 cells, 2 banks x 2 rollouts) | **52.08%**, all four above parity, worst 50.83% | 2,880 |

**The optimizer's curse is worth 35.69 points**: an unguarded argmax over D noisy rollout means is
that much worse than the blueprint it searches from, because one determinization's return is the
final half-suit differential (sd about 2.5) while genuine differences between candidates are an
order of magnitude smaller. A paired lower-confidence-bound rule — deviate only when the improvement
over the blueprint's own choice, on the same determinizations, clears kappa standard errors —
recovers all of it.

**Two negative controls establish that the gain is information, not variety.** Resolving the tie
group uniformly at random, seeded from the public event stream, is worth *exactly* nothing —
50.00% [47.18, 52.82], mean half-suits 4.500 to 4.500. Resolving it with a *single* sampled deal is
worth nothing — 49.58%. Twelve sampled deals are worth points. So the separating signal is in the
**joint** posterior, which every marginal integrates away, and which is why the exact-marginal
tie-break in the section below measures zero.

**Where inside the search the gain sits is NOT resolved.** Guarding the tie group gives 49.86%;
restricting the search to the tie group gives 51.94 / 50.69 / 50.00; the unrestricted search gives
52.64%. At 720 games a cell those intervals all overlap. An earlier draft read a single favourable
bank (54.17%) as an attribution; it is not one.

## Exploitability — the hole v0.5 named in its own evaluation

A responder is fitted from the same policy family with the repaired optimiser and the frozen target
as its entire opponent panel, then re-measured on a fresh seed bank (6543210, 600 deals x 6
rotations = 3,600 games). The response win rate is what the exploiter achieved, so **lower is better
for the frozen policy**.

| frozen target | response win rate | 95% CI |
|---|---:|---|
| v0.4 (positive control; published 51.19% [49.67, 52.72]) | 50.69% | [49.06, 52.31] |
| v0.5 | 50.31% | [48.75, 51.89] |
| **v0.6** | **48.36%** | **[46.75, 49.97]** |

The control reproduces the published figure within its interval. **The same optimiser, budget,
policy family and evaluation bank reach parity against v0.4 and v0.5 and fail to reach it against
v0.6**, with an interval that excludes 50.

The reading must be careful, and the v0.4 findings document got it wrong in exactly this place: the
number is a **lower bound within the searched class**, so a response that fails to reach parity does
not prove the target is hard to exploit — only that this search did not find the exploit. What the
three rows support is the *comparison*, which is like-for-like. This is also the first exploitability
number this project has for v0.5, whose own study called its absence the single largest hole in its
evaluation.

## Partner regimes — the owner's decision D2, and it does not flatter v0.6

`--partners=SPEC` names the policy for the two seats of the acting team that are not the seat under
study, so "v0.6 with two bot partners" and "v0.6 with two other-model partners" can be expressed at
all. Three v0.5 opponents throughout; 800 games per cell, seed 313131.

| partners | **v0.6** | v0.5 |
|---|---:|---:|
| itself (the self-play condition) | **52.25%** | 50.00% |
| v0.3 | 34.50% | 33.12% |
| detective | 33.50% | 33.62% |
| withholder | 34.75% | 35.50% |

**v0.6's advantage over v0.5 is a self-play advantage and it does not survive a change of partner.**
With two v0.3 partners it is +1.4 points; with detective partners −0.1; with withholder partners
−0.8. At 800 games a cell none of the three mixed rows is separated from zero.

This is the owner's standing decision D2 measured rather than asserted: *never headline the
self-play configuration as though it were the one a human will meet*. A refit whose objective is
scored in self-play buys a policy that coordinates with copies of itself, and the coordination does
not transfer. It is the sharpest limitation on everything above.

## In-panel and out-of-panel, stated separately

The fitting panel was `v05, v03, withholder, feint`. Splitting the per-style deltas by whether the
optimiser saw that opponent:

| | opponents | mean delta over v0.5 |
|---|---|---:|
| **in panel** | v0.5, v0.3, withholder, feint | **+3.12** |
| **out of panel** | v0.4, v0.2, lockout, detective, diversifier, hunter, bluffer, random, silent | **−0.20** |

Read at face value this says the refit buys robustness on the styles it was shown and is neutral on
the rest, and that is the conservative reading to carry. Two qualifications, both measured:

* fitC was seeded from fitA, whose panel also contained **lockout**. Counting lockout as in-panel the
  split is **+2.21 in / −0.05 out**. Either way the sign is the same.
* Excluding the withholder, the in-panel mean over the remaining three cells is **+1.15**, which at
  these sample sizes is not separated from zero. The in-panel gain is the withholder.
* The out-of-panel mean is dominated by the single v0.4 cell at seed 515253 (−2.94). Over five
  independent banks and 9,000 games v0.6 is at 50.86% against v0.4, so that cell sits about 1.7
  standard errors below its own five-bank mean (p ≈ 0.09) — unusual but not resolved either way.

## Paired panel comparison — the separated result

Every variant plays the same deals against the same panel; the per-deal margin is bootstrapped
resampling deals as clusters. Seven opponents (v0.5, v0.4, v0.3, lockout, detective, withholder,
feint) at 515253 and 90210; six (no feint) at 606060.

| seed bank | v0.6 − v0.5 | 95% paired CI | artifact |
|---|---:|---|---|
| 606060 | **+2.69** | [+0.98, +4.35] | `E5-ablations.json` |
| 515253 | **+2.67** | [+1.24, +4.11] | `F10-panel-banks.json` |
| 90210 | +1.13 | [−0.31, +2.56] | `F10-panel-banks.json` |

All three banks positive, two with intervals excluding zero, mean **+2.16** points.
**This is the separated result**; the head-to-head above is not.

## What did NOT contribute

Every mechanism built for v0.6 other than the refit measured null or was rejected:
exact-posterior tie resolution (the exact posterior separates 0.00% of the ties), the three extra
ask terms (the fit drives all three to near zero), the deliberate miss (raises the win rate,
fails the KPI gate). Test-time search is the exception and is reported above. See
`research/v06/notes/R12-mechanism-trials.md`.

**v0.6's strength gain is entirely parametric, and it is available only because the optimiser was
repaired first.** v0.5's own forty-generation refit was indistinguishable from sampling noise.

## What the refit actually changed

Behaviourally, pooled over the 9,000 held-out head-to-head games against v0.5:

| | v0.6 | v0.5 |
|---|---:|---:|
| mean sets | 4.539 | 4.461 |
| ask accuracy | 55.00% | 55.12% |
| **declaration accuracy** | **98.50%** | **97.47%** |
| declarations/game | 4.483 | 4.503 |
| events before cashing a locked half-suit | 4.578 | 4.816 |

**The gain is declaration accuracy, not ask accuracy.** v0.6 asks no better than v0.5 — its ask
accuracy is a tenth of a point lower — and declares about as often, but is right about it one point
more of the time. Over 4.5 declarations a game, and with a wrong declaration handing the half-suit
to the opponents, that accounts for the whole of the +0.078 sets/game.

Parametrically, the coordinates that moved most (v0.5 → v0.6):

| coordinate | v0.5 | v0.6 | note |
|---|---:|---:|---|
| `f[14]` location entropy | **−2.4266** | **+1.0621** | **the sign flips.** v0.5 carried a *negative* value-of-information term — it was paid to avoid uncertainty-reducing asks. The v0.6 fit turns it positive on its own, which is what the v0.5 design register argued for on theoretical grounds and what a hand-set flip did not achieve (measured null) |
| `f[9]` information leak | −1.2201 | −6.2453 | a much heavier penalty on being the first to reveal interest in a half-suit |
| `f[13]` known team cards | 0.8384 | 3.9689 | |
| `f[4]` team control | 1.6882 | 4.3532 | |
| `f[5]` lock completion | 4.0462 | 6.4234 | |
| `f[8]` reply threat | −2.9058 | −0.8155 | markedly less afraid of the reply |
| `f[7]` completion bonus | 1.2189 | −0.3819 | sign flip |
| `declThreshold` | 0.8199 | 0.8422 | more conservative declarations |
| `lockedAllocThresh` | 0.7325 | 0.7653 | more conservative on locked half-suits |
| `priorTheta` | 0.4446 | 0.3706 | **down** — v0.5's own study identified its raised `priorTheta` as the deception exposure it could not tune away at fixed weights; the joint refit moves it back |
| `threatWeight` | 2.7047 | 3.7658 | |

The `priorTheta` move is worth stating carefully. Changing it *alone* is harmful — `v05:ptheta=0.30`
scores 48.75% against v0.5 over 1,200 games — because the ask weights were fitted at v0.5's value.
Only the joint refit realises it, and the withholder gain is the measured consequence.

## v0.6 does not run v0.5's chain/threat re-scoring

v0.5 re-scores its top six candidates with a one-step pass that rebuilds the approximate posterior
twice per candidate. It is about 60% of that policy's runtime, and the v0.4 study measured it at
+0.8 ± 0.5 points — not separated from zero. The v0.6 scoring path does not run it.

Restoring it at the v0.6 vector (`v06:chain2=1`) is **not separated from zero**: 45.89%
[42.66, 49.16] at bank 90210 and 51.78% [48.51, 55.03] at bank 31337 head to head, and
−0.75 points [−2.44, +0.92] paired over a six-opponent panel. An earlier draft of this summary
quoted the 90210 bank alone and called it four points worse. It is the sixth mechanism in this study
to look significant on one bank and vanish on two, and it is recorded here as such rather than
quietly deleted.

So v0.6 drops the pass because it is worth nothing and costs about 60% of the policy's runtime,
which is why v0.6 runs at 303 games/s against v0.5's 276.

Consequence for the parameter vector: `searchTopK`, `chainWeight` and `threatWeight` are **inert
coordinates** in the shipped v0.6 configuration. They are retained at their fitted values so that the
flat vector layout stays compatible with v0.4's and v0.5's, and they are flagged as inert in the
parameter appendix rather than presented as fitted quantities that do something.
