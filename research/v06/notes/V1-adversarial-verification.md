# V1 — Adversarial verification of the FishBot v0.6 headline claims

Adversarial pass: the brief was to try to break the claims, not to confirm them. Everything below
was recomputed from the artifacts in `research/v06/results/` or read out of `engine/src/`. Four
short engine runs were made (`--threads=2`, ≤40 games each); they are labelled where used.

Verdict key: **SURVIVES** / **WEAKENED** / **FALSIFIED**.

Environment note that bounds all of this: the artifacts in `research/v06/results/` were written by
`engine/fish` built at 07:03:51, while `src/v06.hpp` and `src/factory.hpp` carry an mtime of
07:22:23 — i.e. the sources were edited **after** the binary that produced E0–E8 was built. I
checked the one thing that would matter most: the binary's built-in `V6PARAMS` reproduces the
source vector exactly (`fish pathology --a=v06` md5 `eaeaeff0b7a662bc3927dafcebe8b903` equals the
same run with `--a=v06:allparams=<the 37 numbers from v06.hpp>`), and the source vector is
character-for-character the final `weights` record of `research/v06/runs/fitC.jsonl`. The shipped
vector and the fit artifact do not drift apart. Other post-hoc source edits are unverified.

---

## Claim 1 — "v0.6 beats v0.5 on every one of five held-out banks, 51.03% over 9,000 games"

### Verdict: **WEAKENED**, and the phrase "every bank" is **FALSIFIED** by the battery's own artifacts.

**Arithmetic: correct.** From `research/v06/results/E3-headtohead.jsonl`, the five `"b":"v05"`
records give `winRateA` = 0.514444, 0.504444, 0.513333, 0.501111, 0.518333 → 926 + 908 + 924 + 902 +
933 = **4,593 / 9,000 = 51.0333%**. The v0.4 side is 905 + 940 + 912 + 911 + 909 = 4,577 / 9,000 =
**50.8556%**. Both round as reported.

**Bank disjointness: holds.** `experiments_v06.sh:43` sweeps `S in 90210 31337 515151 777001
424242`; `fitA.jsonl` header `"seed":20260823`, `fitC.jsonl` header `"seed":20260824`,
`fitD` `20260825`. Disjoint. (Cosmetic hazard only: `TunerSpec::seed` defaults to **424242**
at `engine/src/tuner.hpp:58`, which is one of the five banks — an unseeded exploratory fit would
have burned it. No such artifact exists.)

**Intervals: not one of the ten excludes 50.** This is the first real weakening. Every `wilsonCI`
in `E3-headtohead.jsonl` straddles parity:

| bank | v0.6 vs v0.5 | Wilson CI | vs v0.4 | Wilson CI |
|---|---:|---|---:|---|
| 90210 | 51.44 | [49.14, 53.75] | 50.28 | [47.97, 52.58] |
| 31337 | 50.44 | [48.14, 52.75] | 52.22 | [49.91, 54.52] |
| 515151 | 51.33 | [49.02, 53.64] | 50.67 | [48.36, 52.97] |
| 777001 | 50.11 | [47.80, 52.42] | 50.61 | [48.30, 52.92] |
| 424242 | 51.83 | [49.52, 54.14] | 50.50 | [48.19, 52.81] |

So "every bank above parity" is true of the **point estimates only**. Ten of ten intervals contain
50. `RESULTS-SUMMARY.md:35` and `README.md:13-14` do not say which, and `README.md:66` applies the
opposite standard to v0.5 in the same file — "one of the five below 50%, **and every bank's interval
contains 50%**" — so the project's own stated evidence bar is being met by v0.5 and v0.6 equally.

**Pooled significance is marginal, not comfortable.** 4,593/9,000 = 51.033%; under an independence
assumption SE = 0.527 pts, z = **1.961**, p ≈ 0.0499. That assumption is generous: the 1,800 games
per bank are 300 deals × 6 rotations, so the games are clustered in sixes. Any cluster correction
pushes p above 0.05. The v0.4 side (50.856%) is z = 1.62, p ≈ 0.105 — not separated from 50 at all,
which `README.md:13-14` reports alongside the v0.5 figure without distinction.

**The falsification: there are two more held-out banks in the same battery, and v0.6 loses both.**
`E7-rules.jsonl` line 2 is `fish match --a=v06 --b=v05 --games=250 --rotations=6 --seed=828282` under the
**default** dialect — the same command shape as E3, a seed disjoint from every fit —
and it reports `"winRateA":0.484` over 1,500 games, i.e. **48.40% [45.88, 50.93]**. `E5-ablations.json`
runs the same match-up at seed 606060 (400 deals × 2 rotations = 800 games) and reports
`"reference":"v06" ... "perOpponent":[0.48875, …]` against the same panel whose first entry is `v05`,
i.e. **48.875%**.

Counting every default-rules v0.6-vs-v0.5 cell in the v0.6 battery:

| seed | n | v0.6 win rate | source |
|---|---:|---:|---|
| 90210 | 1800 | 51.44 | E3 |
| 31337 | 1800 | 50.44 | E3 |
| 515151 | 1800 | 51.33 | E3 |
| 777001 | 1800 | 50.11 | E3 |
| 424242 | 1800 | 51.83 | E3 |
| 515253 | 1800 | 51.22 | E4 |
| 606060 | 800 | **48.88** | E5 |
| 828282 | 1500 | **48.40** | E7 |

Six of eight, not five of five. Pooling the two plain `fish match` sets (E3 + E7) gives
5,319 / 10,500 = **50.66%**, z = 1.35, p ≈ 0.18. Pooling all eight gives 6,632 / 13,100 = **50.63%**,
z = 1.43. The headline survives as a point estimate over a hand-chosen five; it does not survive as
"every held-out bank", and it is not separated from parity once the battery's other banks are read.

**One more leak.** `RESULTS-SUMMARY.md:5-6` says "All evaluation banks are disjoint from both fitting
seeds." True as written. But `R12-mechanism-trials.md:216-222` evaluates the ancestor fit `fitA` on
banks **515253 and 90210** and uses that result to justify continuing to fitB/fitC. 90210 is E3's
first bank and 515253 is E4's only bank. They are disjoint from the *fitting* seeds and not disjoint
from the *selection* process that produced fitC out of {fitA, fitB, fitC, fitD}.

---

## Claim 2 — "Minimax regret over the 13-style set is 3.06 for v0.6 against 9.06 for v0.5"

### Verdict: **WEAKENED**. The arithmetic is exactly right; the statistic is one cell wide.

Recomputed from `E4-perstyle.jsonl` (39 records, 13 opponents × 3 arms, all at seed 515253,
300 × 6). Per-opponent regret = (best of {v06, v05, v04}) − arm:

| opponent | v0.6 | v0.5 | v0.4 | best | r(v06) | r(v05) | r(v04) |
|---|---:|---:|---:|---:|---:|---:|---:|
| v0.5 | 51.22 | 50.00 | 48.39 | 51.22 | 0 | 1.22 | 2.83 |
| v0.4 | 48.67 | 51.61 | 50.00 | 51.61 | **2.94** | 0 | 1.61 |
| v0.3 | 75.39 | 72.28 | 73.33 | 75.39 | 0 | 3.11 | 2.06 |
| v0.2 | 80.00 | 81.72 | 83.06 | 83.06 | **3.06** | 1.33 | 0 |
| lockout | 78.56 | 80.00 | 77.94 | 80.00 | 1.44 | 0 | 2.06 |
| detective | 76.83 | 75.61 | 77.28 | 77.28 | 0.44 | 1.67 | 0 |
| diversifier | 95.44 | 94.11 | 92.89 | 95.44 | 0 | 1.33 | 2.56 |
| hunter | 98.44 | 97.67 | 97.67 | 98.44 | 0 | 0.78 | 0.78 |
| bluffer | 99.83 | 99.89 | 99.94 | 99.94 | 0.11 | 0.06 | 0 |
| random | 100.00 | 100.00 | 100.00 | — | 0 | 0 | 0 |
| silent | 82.94 | 81.94 | 78.89 | 82.94 | 0 | 1.00 | 4.06 |
| feint | 52.72 | 53.61 | 51.11 | 53.61 | 0.89 | 0 | 2.50 |
| withholder | 79.06 | 70.00 | 67.50 | 79.06 | 0 | **9.06** | **11.56** |
| **max** | | | | | **3.06** | **9.06** | **11.56** |

3.0556 / 9.0556 / 11.5556. The published figures reproduce exactly.

**Where it breaks.** The brief's observation about relativity is correct but not the fatal one. The
fatal one is that the 3× factor is carried by a **single opponent**, and that opponent was in the
fitting panel.

* **Drop the withholder column** and the statistic collapses: v0.6 **3.06**, v0.5 **3.11**, v0.4
  **4.06**. The "factor of three" becomes a 0.05-point difference between v0.6 and v0.5.
* **Use the reference set the previous release used.** `README.md:128` reports this same statistic
  for v0.5 over the **nine standard styles** (v0.4, v0.3, v0.2, lockout, detective, diversifier,
  hunter, bluffer, random). Recomputing v0.6 on that nine-style set from E4 gives
  v0.6 **3.06**, v0.5 **3.11**, v0.4 **2.56** — **v0.4 is the best arm**. So the sentence
  "on the criterion this project has always said it optimises … v0.6 is a factor of three better"
  is not stable under the project's own prior definition of the criterion; it is a factor of three
  better only under a reference set that was enlarged with the deception panel this cycle.
* **The withholder was in the panel.** `fitC.jsonl` header `"panel":["v05","v03","withholder","feint"]`
  and `fitA.jsonl` header `"panel":["v05","v03","lockout","withholder"]`. The one cell that produces
  the 3× is the cell the optimiser was pointed at, in both the shipped fit and its ancestor.
* **v0.6's own 3.06 is at the noise floor.** It is the max of 13 near-zero regrets estimated at
  1,800 games each (per-cell SE ≈ 1.18 pts; per-cell difference SE ≈ 1.2–1.7 even paired). The
  expected max of 13 such half-normal draws is ≈3 points, so a v0.6 that were exactly tied with
  its predecessors everywhere would still be *measured* at ≈3.06. That direction is favourable to
  v0.6 (its true worst-case shortfall is likely smaller), but it means 3.06 is not a measurement of
  anything; only v0.5's 9.06 is above noise.

**What survives.** The withholder gain itself is real and large: 79.06 [77.11, 80.87] against 70.00
[67.84, 72.07], non-overlapping, paired seeds. Everything the minimax-regret headline is doing is
carried by that one measured gain, and it would be more honest to report it as such.

---

## Claim 3 — "The exact posterior separates 0.00% of the ties"

### Verdict: the **measured statement SURVIVES** every attack I could make on it. The claims built
### on top of it are **WEAKENED**, and one supporting claim in the docs is **FALSIFIED**.

**What I tried, and what held.**

1. *The epsilons.* `probe_v06.hpp:168` takes the argmax with `q > hi + 1e-12` and
   `probe_v06.hpp:171` declares separation on `hi - lo > 1e-9`. Neither can hide a real separation.
   The marginals live in [0,1]; 1e-9 is ~7 orders of magnitude above double round-off on a DP whose
   count tables are exactly-representable integers held in `float` (`blockdp.hpp:45,61,63`), and it
   is many orders below any separation that could matter to a policy. If anything the float tables
   would manufacture *spurious* separations at that threshold. Zero were found, which means the
   tied candidates' exact marginals agree to better than 1e-9 — i.e. structural exchangeability,
   not a threshold artifact.
2. *Is `BlockDP` queried correctly, and does the alias guard apply?* Yes on both. `marginals()`
   (`blockdp.hpp:343`) opens with `if (!ensureCurrent()) return;`, which is the v0.6 generation-stamp
   guard, so the E2 aliasing defect cannot bite on this path. `BlockDP`'s header states it carries
   the C5 ask-legality disjunctions exactly (BLOCK groups) as well as the capacity constraints, and
   `E0-identity.txt` records 0 query mismatches in 301 checks. The probe builds a fresh local
   `BlockDP` and reads it immediately, with no intervening build.
3. *A counter-example run.* `./fish v6probe --mode=ties --a=v06 --b=v04 --games=40 --seed=777001
   --threads=2` — a seed and an opponent that appear nowhere in E8. Result: 1,007 ties,
   **`EXACT posterior SEPARATES the tied candidates 0 (0% of ties)`**, and all three tie-break rules
   again at an identical 43.0983%. I could not produce a separation.

**What the claim does not cover, and where the surrounding text overreaches.**

* **The mechanism was never built into the policy.** `docs/FISHBOT_V06.md:16` lists
  "exact-posterior tie resolution (`extie`) | built; **measured null**", and `docs/FISHBOT_V06.md:119`
  lists `extie=1` as a shipped default. In fact `x.exactTie` is declared at `v06.hpp:27`, parsed
  at `factory.hpp:169`, and **never read anywhere in the decision path**. `ensureExactMarginals()`
  (`v06.hpp:236`) and its output buffer `xmu` (`v06.hpp:194`) are never called or read outside their
  own definitions — `grep -n "xmu\|ensureExactMarginals" src/` returns only the definition block.
  The same is true of `exactP`, `exactFloor`, `exactMix`, `exactDecl` and `declThresh`. So the
  verdict "built; measured null" is wrong twice over: it was not built into the agent, and the
  "null" comes from a diagnostic probe, not from an ablation of a working mechanism.
* **The probe tests a narrower proposition than the mechanism it retires.**
  `R12-mechanism-trials.md:39` describes A1 as "Re-score the tied candidates under the exact count
  law". The probe does not re-score anything: it compares the single ask marginal
  `mu[card][target]` inside the tie group. A full re-score under exact marginals would move *every*
  feature, not just `f[0]`, and is untested.
* **The corpus contradicts itself on this number and the contradiction is unaddressed.**
  `engine/src/v06.hpp:25` states, in the comment that motivates the whole mechanism, "The exact
  count law in blockdp.hpp **separates 10.9 points of those ties**". `R0-OPPORTUNITY-REGISTER.md:222`
  and `R6-v05-residual-diagnosis.md:187` say the same ("Exactness **does** break 10.9 points of ties
  (20% of them)"), and `R11-search-feasibility.md:87` repeats it. R6's measurement is a different
  object — the tie *rate* under `belief=block` (43.36% vs 54.22%) — but nothing in the v0.6 write-up
  reconciles "breaks 20% of them" with "separates 0.00% of them", and the shipped source still
  carries the 10.9 figure as live justification.
* **The E8 artifact's own label is wrong for the v0.6 rows.** `E8-ties.txt` prints "the shipped
  chain/threat pass MOVES the pick 26 (0.72% of ties)" for the v0.6 mirror. The shipped v0.6 never
  runs that pass: `V6PARAMS` gives the three extra ask terms non-zero values, so
  `x.extraFeats == true`, so `chooseAsk` takes the v0.6 branch, and `v06.hpp:458` only calls
  `chainRescore` when `x.chainPass` — which is `false` by default. The 0.72% is the effect of the
  three extra ask terms, mislabelled. (For the v0.5 rows the label is correct: 66.2%.)
* **The tie group is measured on v0.5's score even when `--a=v06`.** `v6ScoreCandidates`
  (`probe_v06.hpp:59`) computes the v0.5 blueprint score without the v0.6 extra terms. So
  "53.8% of contested decisions are exact ties" in the v0.6 mirror row is a statement about the
  v0.5 score vector evaluated at v0.6's weights, not about v0.6's own score.
* **The doc's numbers do not come from the shipped artifact.** `docs/FISHBOT_V06.md:71` and
  `README.md:26` quote 55.6% / 94.0% / 43.75%. `E8-ties.txt` (the artifact the battery writes)
  gives 54.74% / 93.77% / 43.81% for the v0.5 mirror. The quoted figures are from the 100-game probe
  transcribed into `R12-mechanism-trials.md:59-72`, which has no file in `research/v06/results/`.
  Relatedly, R12's own sentence "every tie-break rule realises the same 43.75% hit rate to three
  significant figures" is contradicted by the table three lines above it, which gives the shipped
  pick at **44.13%**.

---

## Claim 4 — "The exact posterior is a worse predictor than the deployed approximation"

### Verdict: **WEAKENED**; the causal attribution to the policy prior is **FALSIFIED** by measurement.

**Fairness checks that pass.**

* *Same information on the hard side.* Both paths take the same `Knowledge& k`. `BlockDP` carries
  the C5 disjunctive certificates and the capacity constraints exactly (blockdp.hpp header;
  `R6-v05-residual-diagnosis.md` records `fish oracle` matching brute force at
  `max abs diff 0.000e+00` over 656,826 marginal checks). The Sinkhorn path
  (`probe_v06.hpp:270`) gets nothing extra but `(theta, phi)`.
* *Same Sinkhorn budget as deployed.* The probe hard-codes `sinkhornDisj(k, 4, 8, …)`, and
  `v05.hpp:28` gives `sinkOuter = 4, sinkInner = 8`. Correct.
* *Same card set.* `runV6Belief` has a latent hazard — `probe_v06.hpp:265`,
  `if (!b.build(k)) continue;`, `continue`s the *config* loop, so a `BlockDP` build failure would
  silently drop that state from the exact row only and leave it in the Sinkhorn rows. It did not
  fire: every row of `E8-belief.txt` reports `cards = 140661`. The log loss is over the same set.

**The falsification.** `R12-mechanism-trials.md:112` claims the policy prior "is the **only** thing
separating the deployed posterior from the exact one". I ran the probe with the prior switched off
entirely — `./fish v6probe --mode=belief --a=v05 --b=v05 --games=25 --seed=31 --theta=0,0.44458
--phi=0 --threads=2`:

```
posterior                         cards     mean NLL     argmax p   argmax HIT
exact (uniform prior)             29628      1.42932       0.4690       44.55%
sinkhorn th=0.000 ph=0.000        29628      1.40210       0.4957       45.68%
sinkhorn th=0.445 ph=0.000        29628      1.39403       0.5076       47.95%
```

With **θ = 0 and φ = 0** — no policy prior at all, the Sinkhorn fit aiming at the same
uniform-over-consistent-deals object the DP computes exactly — the approximation still beats the
exact posterior by **0.027 nats and 1.1 points of argmax hit rate**. In `E8-belief.txt` the full
exact→best-Sinkhorn gap is 0.042 nats; roughly two thirds of it survives with the prior removed.
The prior is therefore *not* the only thing separating them, and the study's stated explanation
("exact is exact under a uniform prior, and that prior is wrong") accounts for only part of the
effect. The residue is either a shrinkage artifact of the Sinkhorn marginals under log loss or a
defect in the exact path on the 82.9% of states `fish oracle` cannot brute-force
(`R6-v05-residual-diagnosis.md`: "17.1% of encountered states are verified"). Neither is
investigated.

**Two further gaps.**

* `E8-belief.txt` contains **no φ = 0 row** — the CLI default is `--phi=0.12198`
  (`main.cpp:621`), and the battery does not override it. So the shipped artifact cannot decompose
  the gap into prior and approximation at all. The "sinkhorn, θ = 0, φ = 0 → 1.39437" row quoted at
  `R12-mechanism-trials.md:101` corresponds to no file in `research/v06/results/`.
* The battery runs `--mode=belief --a=v05 --b=v05` (`experiments_v06.sh:83`), so "the deployed
  approximation" in the artifact is **v0.5's** (θ = 0.44458, φ = 0.12198). v0.6's deployed values
  are θ = 0.37062, φ = 0.14525 (`V6PARAMS[29]`, `[30]`), which appear nowhere in the table. The
  README's headline numbers for this claim — "48.30% against the deployed approximation's 51.04%"
  (`README.md:27`) — appear in **no artifact in `research/v06/results/`**; `E8-ties.txt` gives
  48.10 / 51.26 (v0.6 mirror) and 48.36 / 51.96 (v0.5 mirror).

---

## Claim 5 — "An unguarded determinized search is 27 points worse than its blueprint"

### Verdict: **FALSIFIED as stated.** The two rows are not comparable, their difference is not 27,
### and no pair of rows in R11 differs by 27.

**The cited pair is not comparable.** `R11-search-feasibility.md:57-59`:

| row | det | cand | rollout | n |
|---|---:|---:|---|---:|
| unguarded argmax, **9.44%** | 8 | 6 | `belief=indep,topk=0` | 720 |
| "plumbing control", **50.83%** | **1** | **4** | unstated | **240** |

Different determinization count (8 vs 1), different candidate count (6 vs 4), different n (720 vs
240), and different rollout policy — R11 §3 measures `belief=indep,topk=0` at **11.78%** standalone,
i.e. the 9.44% row searches from a blueprint that is itself 38 points weaker than the one the
control uses. Their difference is **41.39** points, not 27. I could not find any pair of rows in
R11's table that differs by 27: the candidate pairs are 41.39, 35.70, 28.66, 28.05, 25.41 and 24.16.
The figure propagates unchanged into `README.md:28`, `docs/FISHBOT_V06.md:90`, `DESIGN.md:62,123`,
`R12-mechanism-trials.md:41,159,161`, `paper/sections_v06/abstract.tex:18` and
`paper/sections_v06/08-search.tex:62`, and into the source comment at `v06.hpp:53`.

**The comparable pair, from the battery's own artifact.** `E12-search.jsonl` runs the control and
the unguarded arm at *identical* det, cand, rollout, seed and n:

| row | config | n | win rate | Wilson CI |
|---|---|---:|---:|---|
| control | `s1=1,det=8,cand=6,blend=1000000` | 720 | **49.31** | [45.67, 52.95] |
| unguarded | `s1=1,det=8,cand=6,kappa=0` | 720 | **13.61** | [11.30, 16.31] |

**35.70 points**, seed 90210, 120 deals × 6 rotations, default rollout for both. That is the number
the claim should carry. (A second unguarded row now present, `det=12,cand=4,kappa=0`, gives 23.33%
at n=720 against R11's 26.67% at n=150.) The direction and the magnitude of the finding are
comfortably real — the optimizer's curse is worth *more* than 27 points, not less — but "27" is not
traceable to any measurement.

**The control is not blueprint-forced.** `v06.hpp:553`: `double kap = (r < tie) ? x.kappaTie :
x.kappa;` and `lcb = m - kap*se + (r < K ? x.blend * (u[ord[r]] - u[ord[0]]) : 0)`. For any candidate
inside the blueprint's own tie group `u[ord[r]] - u[ord[0]] == 0`, so `blend` contributes nothing,
and `kappaTie` defaults to 0 — the search still selects by **unguarded rollout argmax** on the ~55%
of decisions that are ties. R11's control is worse still: at `det=1` the effective sample size is 1,
so `se = 1e9` for the non-tied candidates and the tie group is decided by a *single* determinization.
Calling either row "the same code with the blueprint forced to decide" is inaccurate.

**Every E12 search row runs `v06:legacy=1`** (the `for CFG` loop, `experiments_v06.sh:93-102`), i.e. v0.5's parameter
vector. The search was never measured at the shipped v0.6 vector in E12. The one row that does
(`E5-ablations.json`, `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26`) reports
`deltaFromRef = -0.01354, ci [-0.02792, +0.00042]` — the guarded search is the *best* arm in that
ablation, point estimate +1.35 points over shipped v0.6, interval barely touching zero. Reporting it
as "≈0 guarded" is defensible on the interval and is not what the point estimate says.

---

## Claim 6 — "v0.6 is v0.5 bit-identical with every switch off"

### Verdict: **SURVIVES.** I could not break it. One doc phrase overstates the evidence, and one
### *supporting* claim in R11 is now stale and false.

**Reproduced.** `./fish pathology --a=v06:legacy=1 --b=v06:legacy=1 --games=40 --seed=31
--threads=2` and the same on `v05` both give md5 **`7d2865b9a6614ce59cd0516f84e83b76`**, matching
`R12-mechanism-trials.md:22`. `E0-identity.txt` reports the pair matching at 60 games
(`47c3e2bb6a1f079b9c05a3c3c6df2e13`).

**Independent corroboration, stronger than the md5.** `E5-ablations.json` runs `v06:legacy=1` as a
variant in a paired six-opponent ablation and gets `"winRate":0.65729, "deltaFromRef":0.02687,
"ci":[0.00979,0.04354], "perOpponent":[0.50000,0.48500,0.71625,0.77625,0.74875,0.71750]` — every
figure identical to the `v05` variant row, to the last digit, across 4,800 games.

**Does `legacy=1` reset everything `applyV6Params` writes?** Yes, exactly. `applyV6Params`
(`v06.hpp:165-204`) writes only `c.w[0..19]`, the fourteen `V05Config` knobs, and
`x.wVoid / x.wTeamHas / x.wLastLive / x.extraFeats`. `factory.hpp:138` is
`{ V05Config d; a->cfg = d; a->x.wVoid = a->x.wTeamHas = a->x.wLastLive = 0.0; a->x.extraFeats = false; }`
— a whole-struct reset of the first two groups plus the four v0.6 fields. Nothing is missed. And the
residual `V06Extra` state cannot matter, because `v06.hpp:356` is
`if (!x.search && !x.extraFeats && !x.deadAsk) return V05Agent::chooseAsk(pub);` — with all three
off the override is never entered at all, so `chainPass`, `exactTie`, `nDet`, `kappa`, the rollout
knobs and the `deadTried` bookkeeping are unreachable. `legacy=1` also runs before
`applyV05Opts`, so explicit knobs still win; there is no ordering trap.

**Overstatement to fix.** `RESULTS-SUMMARY.md:11` and `docs/FISHBOT_V06.md:7` describe the md5 as
covering "a diagnostic transcript" / "the full `fish pathology` transcript". `fish pathology` prints
~18 lines of aggregate KPIs (see `E2-pathology.txt`), not a per-event transcript. The claim is
"identical on every KPI the pathology harness computes", which is weaker than "bit for bit" but is
in practice underwritten by the E5 per-opponent identity above.

**Stale and now false.** `R11-search-feasibility.md:20` claims
`./fish pathology --a=v06:s1=0 --b=v06:s1=0 --games=60 --seed=31` is byte-identical to `v05`
(md5 `47c3e2bb…`). That was written before v0.6 had its own vector. Re-run at 40 games:
`v06:s1=0` → `effdbe791e1ddda0729e219fd37ba132`, `v05` → `7d2865b9a6614ce59cd0516f84e83b76`.
**DIFFERENT**, as it must be — `s1=0` leaves `V6PARAMS` and `extraFeats=true` in place. The identity
control is `legacy=1`, not `s1=0`, and R11 should be corrected.

---

## Claim 7 — "The gains are in-panel" (+3.12 in / −0.20 out)

### Verdict: **WEAKENED**. The arithmetic reproduces; the panel definition is understated, the
### in-panel figure is one cell, and the "substituting the five-bank estimate" step is not
### like-for-like.

**Arithmetic reproduces exactly.** Deltas (v0.6 − v0.5) from `E4-perstyle.jsonl`:

```
v05 +1.2222   v04 -2.9444   v03 +3.1111   v02 -1.7222   lockout -1.4444
detective +1.2222   diversifier +1.3333   hunter +0.7777   bluffer -0.0556
random 0.0000   silent +1.0000   feint -0.8889   withholder +9.0556
```

In-panel {v05, v03, withholder, feint} = 12.5000 / 4 = **+3.125**. Out-of-panel, the other nine =
−1.8334 / 9 = **−0.2037**. Both round as published.

**The panel is understated.** `RESULTS-SUMMARY.md:73` says "The fitting panel was `v05, v03,
withholder, feint`", which is fitC's header. But the same document's line 4 says the shipped vector
was "seeded from `fitA` at seed 20260823", and `fitA.jsonl`'s header is
`"panel":["v05","v03","lockout","withholder"]` — **lockout was an optimiser target for the 30
generations that produced fitC's starting point**. (fitB, the other v0.6-base fit at the same seed
20260824, used `["v05","v03","lockout","withholder","feint"]`.) Counting lockout as in-panel:
in-panel becomes 11.0556 / 5 = **+2.21** and out-of-panel −0.3890 / 8 = **−0.05**. Both published
figures move, and the honest statement of what the optimiser saw is the five-opponent set.

**The in-panel mean is one cell.** Excluding withholder, in-panel is (1.2222 + 3.1111 − 0.8889)/3 =
**+1.15** over three cells, against a per-cell 95% half-width of ±2.3 points. "The refit buys
robustness on the styles it was shown" is, on this evidence, "the refit buys 9 points on the
withholder and nothing separable anywhere else".

**The "+0.22" substitution is not like-for-like.** `RESULTS-SUMMARY.md:86` replaces the v0.4
out-of-panel delta (−2.94) with "+0.86 … the five-bank estimate", reaching −0.20 → +0.22. I can
reproduce the arithmetic (−1.8334 + 2.9444 + 0.8556 = 1.9666, /9 = +0.2185), but the substituted
quantity is a different object: every other entry in the column is *v0.6 minus v0.5 against the same
opponent*, whereas +0.86 is *v0.6's win rate against v0.4 minus 50*. That equals the paired delta
only if v0.5 vs v0.4 is exactly 50.00, and `E4-perstyle.jsonl` measures it at **51.61**. E3 never
ran v0.5 against v0.4, so no matching reference exists on those five banks. The correction should
not be made at all with the artifacts on hand.

**"Does not replicate" is overstated.** `RESULTS-SUMMARY.md:69`: "At 1,800 games a cell carries
roughly ±2.3 points, and the 515253 value is about one standard error below the five-bank mean."
±2.3 is the **95% half-width** at n = 1,800 (SE = 1.179 pts, ×1.96 = 2.31), not one SE. The gap is
50.856 − 48.667 = **2.19 points = 1.7 SE** of the difference (p ≈ 0.09), not 1.0. The v0.4 cell is
right at the edge of replication, and the summary's own framing — "the one regression to state
plainly" followed by "it does not replicate" — resolves that edge in v0.6's favour on a factor-of-two
misreading of the interval.

**Supporting claim that does not trace.** `RESULTS-SUMMARY.md:64`: the withholder gain "replicates
at two further banks in the paired ablation (**+11.0 and +9.5**)". The only replication in the
corpus is `R12-mechanism-trials.md:216-225`, which gives **+9.5 and +7.7** — and it is for **fitA**,
not the shipped fitC vector, and one of its two banks is **515253**, the same bank the +9.06
headline comes from, so it is at most one further bank. E5 (seed 606060) gives
0.78875 − 0.71750 = **+7.13**. "+11.0" appears in no artifact or note in the repository.

---

## Claim 8 — Other numbers in `docs/FISHBOT_V06.md` and `README.md` that overstate or do not trace

Checked every quantitative statement in both files against the artifacts. Findings, worst first.

**8a. `README.md:12` — "the first FishBot transition in this project that beats its predecessor on
every held-out seed bank."** Falsified by E7 and E5 (Claim 1). Also inconsistent with the same
file's v0.5 standard at line 76, which counts interval coverage of 50.

**8b. `docs/FISHBOT_V06.md:14` / `README.md:29` — three extra ask terms, "the fit drives them to
≈0" / "near zero".** `V6PARAMS[34..36]` = **0.17133, −0.47667, −0.77680**. For comparison the
summary's own parameter table presents `f[7] = −0.38185` as a meaningful sign flip and `f[19] =
−0.79873` as a fitted weight. More importantly, non-zero is not cosmetic here: it sets
`x.extraFeats = true` (`v06.hpp:203`), which is the single flag that routes v0.6 away from
`V05Agent::chooseAsk` and therefore **switches off v0.5's chain/threat pass entirely**. Describing
the fitted values as "≈0" hides the largest structural difference between v0.5's and v0.6's ask path.

**8c. The chain/threat attribution is contradicted by E5.** `RESULTS-SUMMARY.md:154-156` says restoring
the pass at the v0.6 vector (`v06:chain2=1`) "scores 45.89% … 4.11 points worse", explained as the
pass's weights being "out of calibration under the refit". But `v06:xf=0` also restores the pass —
with `extraFeats=false`, `v06.hpp:356` short-circuits to `V05Agent::chooseAsk`, which runs
`cfg.searchTopK`/`chainWeight`/`threatWeight` at the v0.6 vector — and `E5-ablations.json` measures
it at `deltaFromRef = -0.00125, ci [-0.01917, +0.01667]`, i.e. **0.1 points better**, null. The two
measurements of "run the pass at the refit vector" differ by 4 points and are not reconciled. It
also means the `xf=0` row is a **confounded ablation**: it removes the three ask terms *and*
reinstates the chain/threat pass, so it cannot isolate either.

**8d. `45.89%` is used for two different things.** `docs/FISHBOT_V06.md:54` — the handicapped-base
sanity fit `v05:w0=0` scores 45.89% against v0.5. `RESULTS-SUMMARY.md:155` — `v06:chain2=1` scores
45.89% against v0.6. Identical to four significant figures, for unrelated configurations, neither
with an artifact. One of them is very likely a transcription error.

**8e. Numbers with no artifact in `research/v06/results/`.** As of this pass the battery has written
E0, E1, E2, E3, E4, E5, E6, E7, E8-ties, E8-belief and a partial E12. Everything below is quoted in
the docs and cannot currently be traced:
* "0 false negatives in **666,689** rejections" (`README.md:36`, `docs/FISHBOT_V06.md:27`) — needs
  E10-gateaudit.txt.
* "moves the action on ~34% of searched decisions … at ~0.1 games/s on one thread"
  (`docs/FISHBOT_V06.md:92`) — needs E14-searchdev.txt.
* "27.1% dead asks, longest dead run 364, 7.5% of games killed by the action limit" and the 52.33%
  figure (`README.md:30`, `docs/FISHBOT_V06.md:100`) — needs E15-deliberate-miss.txt.
* "**285 mismatches in 294 checks**" (`README.md:36`, `docs/FISHBOT_V06.md:26`) — cited to
  `research/v05/results/P2-forced-endgame.md` §6, outside this study.
* "0.334% of cards differ, wider in 346 of 346 cases" (`docs/FISHBOT_V06.md:88`) — cited to
  `probe/reconcheck.cpp`, which is not in the tree.
* The tie-structure numbers 55.6% / 94.0% / 43.75% and the belief numbers 48.30% / 51.04% are from
  R12's 100-game probe, not from E8 (see Claims 3 and 4).

**8f. `docs/FISHBOT_V06.md:92` — "Guarded, the search converges to the blueprint **and** moves the
action on ~34% of searched decisions — an order of magnitude above the healthy band."** These two
halves contradict each other in one sentence. (The reconciliation is that `kappaTie = 0` waives the
guard inside the blueprint's tie group, so most of the 34% is tie-group churn — but the doc does not
say so, and `probe_v06.hpp`'s own comment names 1–3% as the falsifier for a healthy search.)

**8g. `docs/FISHBOT_V06.md:59` — "population 20 → 14".** fitC's header is `"pop":14,"elite":4`;
fitA's is `"pop":20,"elite":5`. The "20 → 14" reads as a within-run schedule; it is two different
runs. Minor, but the elite count also changed (5 → 4) and is reported only as "elite 4".

**8h. `RESULTS-SUMMARY.md:123` — "The gain is declaration accuracy, not ask accuracy."** The pooled
figures reproduce exactly from E3 (mean sets 4.5390 / 4.4610, ask acc 55.00% / 55.12%, decl acc
98.5005% / 97.4659%, decl/game 4.4831 / 4.5028, lock-hold 4.578 / 4.816). And the accounting works:
v0.6's own correct declarations plus v0.5's wrong ones give 4.530 sets/game against 4.456, i.e.
0.074 of the observed 0.078. But this is close to an identity rather than a mechanism — with total
declarations pinned near 9 per game across both teams, `sets` is an affine function of declaration
accuracy, so "the gain is declaration accuracy" and "the gain is sets" are nearly the same sentence.
It locates the gain; it does not explain it.

**8i. `E4-perstyle.jsonl` per-cell CIs are absent from the headline table.** Of the twelve non-trivial
per-style deltas in the in/out-of-panel split, exactly one (withholder) has non-overlapping intervals.
`RESULTS-SUMMARY.md:88` does say this; `README.md:12-16` does not.

---

## Things I could not check

* **E9–E15 claims.** The battery was still running (E12 partial at the time of this pass), so
  E9 throughput, E10 gate audit, E11 partner regimes, E13 rollout fidelity, E14 search deviation and
  E15 deliberate miss have no artifacts to check against. Everything in 8e is pending those files.
* **`fitB` / `fitD` selection criterion.** Four fits exist; fitC was shipped. `freeze_config_v06.py`
  records provenance but not *why* fitC over fitB/fitD, and neither `select_final.py` nor any note
  records the selection. If the selection used bank 515253 or 90210 (as `R12` §6.2 did for fitA),
  those banks are model-selection data, not held-out data.
* **Whether the exact posterior is correct on large states.** `fish oracle` verifies 17.1% of
  encountered states; the residual NLL gap at θ = φ = 0 (Claim 4) is exactly the size a small
  systematic error on the unverified 82.9% could produce. Testing this needs a brute-force oracle
  extension, not a short run.
* **Post-07:22 source edits.** `src/v06.hpp` and `src/factory.hpp` were modified after the binary
  that produced E0–E8 was built. I confirmed the shipped parameter vector is unaffected; I did not
  diff the rest (both files are untracked, so git cannot show what changed).
* **Rebuild-and-rerun of anything with `s1=1`.** Out of scope under the run budget; the E12 artifact
  supplied the comparable pair needed for Claim 5.
