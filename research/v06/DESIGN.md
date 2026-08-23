# FishBot v0.6 — design specification

> **Outcome, added after the study ran.** This document is the plan, kept as a record of intent.
> What actually happened is in `research/v06/RESULTS-SUMMARY.md`, and it differs from the plan in
> three places worth naming here, because the plan was wrong in each:
>
> * **A1, exact-posterior tie resolution, is null** — the exact posterior separates 0.00% of the
>   ties. But the ties are *not* irreducible: an ensemble of whole sampled deals separates them
>   where no marginal can, which is the study's one positive mechanism.
> * **A2, A3, A4, D1, D2, D3 are null or inert.** Five mechanisms cleared a 95% paired interval at
>   the per-cell budget this project has always used and returned zero at three times it.
> * **S1, the search, was expected to fail and did not.** It is the only mechanism that beats a
>   static rule, and it ships as the named `FishBot v0.6-Search` configuration.
>
> The strength of the *deployed* policy comes from none of the designed mechanisms. It comes from
> E4, the harness repair, which was written down as a prerequisite and turned out to be the work.

Derived from the v0.6 recon (`research/v06/notes/R0`–`R11`: nine parallel readers plus a synthesis,
plus two probe suites written for this study). Baseline is `bd812fe` (v0.5).

Every mechanism below is keyed to a measured defect, is individually switchable, and carries a
measurable gate. Where a plausible mechanism was tested and failed it is recorded as rejected so it
is not rebuilt.

---

## 0. What the recon established

### 0.1 v0.5's headline mechanisms are worth approximately zero

Pooled over 16,000 games at three seeds (R1), `v05` beats `v05:topk=1` by 0.81 points, `v05:chain=0`
by 0.08, `v05:threat=0` by −0.45, and `v05:m1=0` by 0.75 [−0.80, +2.30]. The 40-generation CEM refit
moved the incumbent objective 0.4998 → 0.5096 against a per-generation sd of 0.0227 (OLS slope
+0.00049/gen, se 0.00031, **t = 1.60**): indistinguishable from sampling noise (R4). Running v0.5's
*mechanisms* on v0.4's frozen vector reproduces the whole v0.5 result (R5). **v0.5's gain was
mechanical, not parametric, and its parametric machinery does not work.**

### 0.2 The defect that decides more than half of all asks

Two independent probes (R6 `v06probe margin`, 5,291 decisions; R10 `probe/margin.cpp`, 13,879
decisions) agree: at **54.7%–55.2% of ask decisions two or more candidates score bit-identically**
and the winner is whichever `enumerateAsks` emitted first — lowest card index, then lowest seat.
The score is not flat (median spread 5.91 across the candidate set); the dimension is blind. Three
stacked causes: 18 of 20 features are per-(half-suit, target) (`v05.hpp:285-344`); `sinkhornDisj`
makes exchangeable cards identical **by construction** (`belief.hpp:478`); and `askExpectedValue`
opens `(void)target;` (`v05.hpp:437`).

94.79% of ties are two cards of one half-suit at one target, so this is primarily a **card**
dimension, not the target dimension the v0.5 study headlined.

A hindsight oracle that resolves the tie correctly and changes nothing else scores 7.66–1.34
(+3.16 sets/game, R6). That is a **bound, not a target**: an exchangeable pair is unpredictable by
construction. The recoverable part is exactly the fraction of ties the **exact** posterior separates,
measured at 10.9 points of the 55 (R6: block ties 43.36% vs Fast 54.22%).

### 0.3 The second-largest channel is a belief-accuracy problem

**9.74% of all v0.5 asks — 21.97% of all its misses — are asks into a half-suit its own team
already owns outright**, so they are guaranteed misses that donate the turn. The rate is 7.5%–12.2%
across every table composition measured, so it is not a mirror artifact. M1 structurally cannot see
them: `enumerateAsks` only permits asking opponents, and no single seat can prove a *teammate* holds
a card. The Fast posterior assigns those cards non-zero opponent mass; the exact one does not.

### 0.4 Mechanisms measured and REJECTED — do not rebuild

Carried forward from `research/v05/DESIGN.md` §0.3 and R0 §3, plus this study's own negatives:

- **Repetition guard** (−6.13 points), **time-varying holding cost** (−3.9), **deleting the policy
  prior** (−4.60 against deception), **turn-transfer willingness ladder** (0.148 events/game),
  **confidence-ranked declaration arbitration** (+0.30 pp against a clairvoyant ceiling of +0.35),
  **forced-endgame work of any kind** (0.11% incidence), **`m1p` ownership scaling** (−1.33).
- **Double-dummy / perfect-information Monte Carlo.** Under perfect information the team on turn
  wins every half-suit not dealt outright to the opponents (8.867–0.133, verified 300/300 games,
  R8), so every determinization returns "we take everything" and no action is discriminated. This
  eliminates *double-dummy* rollouts. It does **not** eliminate determinized rollouts whose players
  are seated at their own information sets, which is what S1 below is.
- **An unguarded search argmax.** Measured at **−27 points** (R11). Any search must carry a paired
  significance guard.
- **Rollout search as the instrument for the tie channel.** Belief-symmetric ties are symmetric in
  the determinizations too, so the rollout mean carries variance and no signal on ~80% of them
  (R11 §4b). The exact posterior resolves the same question in ~11 µs.
- **Learned or neural belief.** Belief is closed-form and brute-force validated in this game.
- **`priorPhi` as an independent parameter** (absorbed by Sinkhorn's column normalisation to
  max 1.29e-04), **`patientLocked`/`lockedAllocThresh`** (unreachable under the shipped config;
  `v05:patient=0` is bit-identical to `v05`).

### 0.5 Latent engine defects that gate the work

`BlockDP::build` parks its tables in a `thread_local` pool, so a second agent's `build()` repoints
the first agent's tables — **285 mismatches in 294 checks**. Harmless under Fast, fatal under any
exact-belief mechanism. `BruteForce::enumerate` reads uninitialised stack at `nU == 0` (2.85% of real
decisions). `Knowledge::onEvent` appends duplicate disjunctions. `tuner.hpp:35` hard-codes
`w.size() > 18` where `NFEAT` (20) belongs and `tuner.hpp:59` hard-codes `st.games * 2`, so fitting
at `--rotations=6` would silently report a third of the true win rate.

---

## 1. Architecture

`engine/src/v06.hpp` defines `V06Agent : V05Agent`. **With every v0.6 switch off, v0.6 is v0.5 bit
for bit** — verified, not asserted (`fish pathology` output is md5-identical). Every mechanism is a
separate switch so the ablation table is exact rather than approximate.

### E — Engine and harness repairs (blocking; no strength claim)

| id | change | gate |
|---|---|---|
| **E1** | Declaration memoisation: cache `proposeDeclaration`'s verdict per (agent, event index), and cache `evaluateSet` within a decision. 87.2% of runtime is spent on a decision worth 0.46 sets, and five of six polls per event are discarded by lowest-seat arbitration | ≥ 3× mirror games/s **and bit-identical play** over ≥ 1,000 games at three seeds |
| **E2** | `BlockDP` owns its tables | `fish blockalias` reports 0 mismatches |
| **E3** | Disjunction dedup on insert; `BruteForce` init fix; `tuner.hpp` `NFEAT`/rotation fixes; `gateaudit` parsed for v0.5+; unknown-flag detection in `main.cpp` | `fish verify` clean; `gateaudit` non-vacuous |
| **E4** | Harness: per-coordinate CEM sigma from `--sigmaparams` (currently parsed and dropped), a **paired** CRN objective (candidate − incumbent on the same deals), an objective dispatch `{softmin, min, regret, minimaxregret}`, and `pathology --json --gate` as a commit gate | self-test: paired delta of a policy against itself is exactly 0 with zero-width CI; no coordinate above 20% clip at generation 0 (from 93.4%); a 10-generation fit on a handicapped base (`v05:w0=0`) recovers ≥ 60% of the known gap |
| **E5** | Per-seat policy specs in `runMatch` so "v0.6 with two bot partners" and "v0.6 with two human-model partners" can be expressed at all (owner decision D2) | the results table reports both regimes as separate columns |
| **E6** | Local-best-response / exploitability auditor, parameterised (currently hard-wired to v0.4) | reproduces v0.4's published 51.19% [49.67, 52.72] as a positive control, then returns a number for `v05`, `v05:m1=0` and every v0.6 candidate |

### A — Ask-side mechanisms

| id | mechanism | defect | gate |
|---|---|---|---|
| **A1** | **Exact tie resolution.** When the fitted score cannot separate the leading candidates, re-score exactly that set under the exact posterior (`DealDP` count law over the actor's own `Knowledge`) and pick on the exact marginal, then on the exact P(this hit completes the team's ownership of the half-suit). Fast keeps the bulk score; exactness is spent only where the bulk score is blind | §0.2 | exact-tie-at-top rate below 10% on the R10 probe; paired delta vs `v05` with a CI excluding zero; no panel style worse than −0.10 sets |
| **A2** | **Team-ownership discount.** Price the exact P(our team already holds this card) against the ask, so guaranteed-miss asks into own-locked half-suits stop being chosen | §0.3 | `asks in own-locked` below 3.0% in the mirror (from 9.74%) and below 4.5% against every panel style |
| **A3** | **Target and card dimension (M4/M5).** Delete `(void)target;`. Add: knowledge-based lockout value read off a per-seat model of what the target provably knows; per-(target, half-suit) void progress, since `legalAsk` requires the actor to hold a card of the set so voiding an opponent permanently removes their right to ask there; and the last-live-opponent split. The per-seat model is one shared common-knowledge object plus a per-seat refinement, not six objects | §0.2, R0 #2, V6-M11 | void-creation rate above 45% of successful asks; paired delta ≥ 0; the M4 soundness probe still reports 0 unsound fires over ≥ 2M checks |
| **A4** | **Sign repairs.** `f[14] = binEnt(p)` carries −2.42663 — a *negative* value-of-information term and the second-largest discriminator in the score; `f[16]` *rewards* handing the turn to a well-placed opponent; `f[12]` is an explicit perseveration bonus at +1.38 while 50.6% of asks repeat an (actor, half-suit, target) triple | R0 #5, V6-M10 | after refit all three have a defensible sign and the paired delta is ≥ 0 (hygiene, not a strength claim) |

### D — Declaration-side mechanisms

| id | mechanism | defect | gate |
|---|---|---|---|
| **D1** | **Recalibrate `pAlloc`.** The deployed `pAlloc` is 1.94× the exact posterior yet *under*-confident against ground truth: the 0.5–0.8 band is right 79.41% of the time while `declThreshold = 0.81991` rejects it, against a +1/−1 payoff whose break-even is 0.5 | R0 #8 | reliability flat to ±0.03 in every bin with n ≥ 30; declarations/game above 4.49 with the wrong-declaration rate no worse than 2.90% |
| **D2** | **Joint allocation naming.** `feasibleAllocation` picks the MAP by a product of *unconditioned* marginals; 72–75% of remaining misdeclarations are "team held all six, named the wrong teammate" | R0 #7/V6-M9 | belief-only misdeclaration rate below 2.0% (from 4.354%); proof-backed stays at 0.000% |
| **D3** | **A time-aware stopping rule.** `value()` takes no time input, so `V(wait at t) ≡ V(wait at t+1)` is an identity of the implementation and "patience dominates" is self-fulfilling. Fix the identity, not the cost — the positive holding cost was swept and rejected at −3.9 points | R5 ¶C8 | declaration timing changes measurably with no loss against any panel style |

### S — Search (the research bet, reported honestly whatever it measures)

**S1 — Determinized information-set search.** Sample deals from the exact posterior; seat all six
players at their own information sets (public deduction state refined by the determinized hand);
apply the candidate action; play the continuation with a reduced-cost blueprint; average under
common random numbers; and **deviate from the blueprint's own choice only when the paired
improvement clears κ standard errors**. R11 measures the unguarded version at −27 points and the
guarded version at +2 (not resolved at n = 240). S1 ships only if it clears its gate.

*Gate:* paired delta ≥ +0.25 sets/game with a CI excluding zero; the search differs from the
blueprint on 1–3% of decisions, not 40% (a search that moves 40% of decisions is mis-scaled, not
smart); no panel style worse than −0.10 sets; the LBR margin from E6 no worse than v0.5's; and the
pathology gate clean.

### R — Regimes and reporting

**R1 — Partner-aware play (owner decision D2).** Two regimes, evaluated and reported separately and
never collapsed: bot teammates (the self-play condition) and human/unknown teammates. Stochastic
selection among near-optimal asks, seeded **from the public history** so the multi-agent
common-knowledge property is preserved.

**R2 — Conventions behind `--conventions=off|on` (owner decision D1)**, shipped off as the headline
configuration, with the with/without delta published as a result.

---

## 2. Build order

```
Phase 0   E1 E2 E3 E4            harness and engine, no strength claim
Phase 1   A1 A2 D1               the two measured loss channels, cheapest instruments first
Phase 2   A3 A4 D2 D3            the rest of the deterministic mechanisms
Phase 3   E6                     exploitability, BEFORE any search claim
Phase 4   refit under E4         minimax regret, mirror in panel, per-coordinate sigma
Phase 5   S1                     the research bet, gated
Phase 6   E5 R1 R2               regimes, then the full battery and the paper
```

Every phase ends with the pathology KPI gate (`longest dead run`, `% games with a dead run ≥ 6`,
`% post-horizon declarations wrong`, `action-limit games`) and a paired head-to-head against v0.5 on
a held-out bank. **A configuration that scores higher while failing the KPI gate is rejected**: the
two highest-scoring rows of the v0.5 ablation table were the ones that kept the pathology.
