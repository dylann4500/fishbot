# FishLab v0.5 findings

Scope: six players, 54 cards, nine half-suits, wrong declarations awarded to the
opposing team, declarations legal at any moment — the project owner's rules of
record, checked point by point against `Rules` in `research/v05/BRIEF.md`. All
evaluation figures come from seed banks disjoint from the fitting bank, and the
configuration was frozen before they were run. Regenerate everything with
`engine/experiments_v05.sh`.

This document describes the study design and what it establishes. The policy
itself is specified in `docs/FISHBOT_V05.md`; the numeric tables it reports are
regenerated from the artifacts by `engine/build_tables_v05.py`; the full study is
`paper/fishbot_v05.tex`.

**The headline, stated the way the evidence supports it.** v0.5 is **not
meaningfully stronger than v0.4 in win rate** — +1.11 points head-to-head, means a
wash (83.60% against 83.57% over nine opponents), 50.79% pooled over five
held-out banks, and v0.4 marginally better on
minimax regret (1.61 against 1.78). What the study delivers is the elimination of
a failure mode, plus a large replicated gain against the deception archetype that
motivated the work (+7.2 on the withholder) and a smaller, equally replicated loss
against a different one (−2.2 on the feint). No aggregate win rate should be
quoted from this study without the per-opponent table and an explicit worst case.

## Study design

| Stage | Purpose | Seed bank |
|---|---|---|
| Reproduction (P0) | reproduce the user's report on the shipped v0.4: mirror pathology, and the same instrument against a weak opponent to show why the published evaluation missed it | 31 (mirror), 90210 (vs v0.3) |
| Forcing dilemma (P0b) | measure both settings of v0.4's forcing rule — never force, always force — and establish that both are bad | 4242 / 31 |
| Deadlock forensics (P1) | locate the deadlock: cycle structure, lock census at every event of the dead run, and the information an in-lock ask actually carries | 31, 777001, 20260822; lock census also 90210 / 777 / 4242 |
| Forced endgame (P2) | measure `bestGuess`'s capacity violation, the inert willingness ladder, and the achievable ceiling by replay | 777, 31, 90210; verification 1234567, 424242, 7 / 2026 / 999983 / 61803 |
| Deception (P3) | measure how the policy prior behaves against silence and against manufactured certificates; test deleting the prior | 20260822, 101, 777; verification 777333, 20260823, 31415926, 515151 |
| Policy review (P4) | line-by-line audit of the ask score, the value function's contribution, and the declaration path | 31, 777001, 424242 |
| Human strategy (P5) | what a strong human does that the policy cannot express | 31, 90210, 4242, 20260822 |
| Declaration races (P6) | arbitration order, willingness ladders, and the cost of not comparing private confidences | 31, 4242, 99001, 777001, 20260822, 424242 |
| Value function (P7) | whether the 16-feature linear `V` carries any signal the ask rule can use | 99001, 1357911, 424242, 7788991, 13579246 |
| Coordination (P8) | the two rules-legal channels the engine leaves unused: turn-transfer willingness, declaration arbitration | 31, 777 |
| Literature refresh | prior work on signalling, holding costs, data-biased response; each imported claim re-tested in the engine | 31, 777001 |
| **Adversarial verification** | every headline finding above re-measured at a seed disjoint from the one it was found at, by a verifier that did not run the original | as listed per stage |
| Corrections register (C1) | every v0.4 claim the diagnosis overturns, and every one it fails to overturn | — |
| Build M1 → M2 → M8 | each mechanism measured alone against the KPI gate before the next was added | 31 (mirror), 90210 (cross) |
| Fitting, round 0 | abandoned at generation 19 | **not recorded** — see "Provenance gaps" |
| Fitting, round 1 (ships) | CEM over the 34-coordinate vector, population 24, elite 6, 40 generations, β = 25, **mirror in the panel** | 505101 |
| Freeze and round-trip | `freeze_config_v05.py` bakes the vector into `V05Config`; the mirror-against-`allparams` assertion checks parser and script agree | 1 |
| E1 verification | rules, information safety, belief soundness, determinism | internal to `fish verify` |
| E2 pathology | the commit-gate KPIs, both mirrors and the cross match | 31; cross match 90210 |
| E3 held-out head-to-head | primary strength result, five disjoint banks | 90210, 31337, 515151, 777001, 424242 |
| E4 per-opponent profile | the worst case across nine styles, for **both** arms | 515253 |
| E5 mechanism ablations | factorial core of M1/M2/M8 plus the two rejected switches | 606060 |
| E6 calibration | forecast reliability of the ask and declaration estimators | 717171 |
| E7 rule dialects | four dialects, including the v0.3 legacy rules | 828282 |
| E8 forced endgame at volume | 24,000 games per arm, accuracy read **per declaring team** | 909090 |
| E9 throughput | whole-game games/s | — |
| E10 deception panel | silent / feint / withholder, two independent banks | 31415926, 8675309 |

## The defects the diagnosis found in v0.4

Eleven confirmed defects, each keyed to a measurement. The full statement of each,
with the v0.4 sentence it contradicts quoted verbatim with `file:line`, is the
**corrections register of record**, `research/v05/results/C1-v04-corrections.md`.
That register also lists the v0.4 claims the diagnosis *failed* to overturn, which
is most of what the v0.4 study disclosed about itself.

| # | Defect | Measured cost |
|---|---|---|
| A | `bestGuess` picks a per-card argmax with no capacity constraint, so it names allocations exceeding a teammate's hand count — posterior probability exactly **zero** | **100%** of forced-endgame declarations wrong; the same bug zeroes `willingForced`'s `pAlloc` at 99.2% of states, leaving all seven willingness rungs inert |
| B | `pressure()` stage 2 returns true from `declareNow` before inspecting `pAlloc` at all | 8.13 points against a mirror opponent, exactly 0 against every weak one — a pure strong-opponent tax |
| C | `askExpectedValue` opens `(void)target;` — half the ask score cannot see who is being asked | 0.61–0.90 free bits/ask at 44–47% of decisions |
| D | `f[14] = binEnt(p)` carries a **negative** weight: a negative value-of-information term | the agent is paid to avoid uncertainty-reducing asks |
| E | `f[12]` rewards re-asking in the half-suit you last asked in | 40.03% exact repeat asks |
| F | `f[11]` rewards taking an opponent's last card without distinguishing the *last live* opponent | walks into the forced endgame it loses 100% of |
| G | `value()` takes no delta on `myCards`/`minFriendly` | the stopping rule cannot see that a declaration empties a hand |
| H | `computeAggregates()` runs before `refresh()` in `proposeDeclaration` | posterior stale by mean 2.0 events, max 177 |
| I | the 16-feature value function is effectively `bias + score differential`, constant across candidate asks | the terms the ask rule consumes carry no measured outcome information |
| J | the mirror was never in the fitting panel, and the β = 10 "soft minimum" had a max/min gradient weight ratio of 1.9 | it was a weighted mean — the direct explanation for why the mirror pathology survived fitting |
| K | `priorPhi` is not an independent channel: the exponent rearranges so its second term is card-independent and Sinkhorn's capacity normalisation removes it | the "silence" channel the owner attacked does not exist in v0.4 |

Two of these are also **corrections to the v0.4 write-up**, not merely to the code,
and the v0.5 paper states them as corrections in their own section rather than
patching them silently:

- **C1 — the termination theory is wrong in both halves.** `E11-termination.md`
  and `docs/V04_FINDINGS.md` finding 6 claim that a half-suit frozen by the
  locked-half-suit theorem "also admits no further information". It does not: the
  owning team may still legally ask inside it, and such asks carry ask-legality
  certificates. And the observed deadlock is not located in frozen half-suits at
  all — 66% of long games have no half-suit locked to any team anywhere in the
  dead run.
- **C10 — the reported worst case was measured on a panel with no opponent of
  v0.4's own strength.** "v0.4-Fast's lowest win rate against any panel member is
  75.07%" is true of that panel and misleading as a robustness claim; once a
  mirror-strength opponent is included the worst case for both v0.4 and v0.5 is
  ≈ 50%, because the worst case for any policy in this family is a copy of itself.

Four candidate fixes were tested and **rejected**, and are recorded so they are not
rebuilt: a time-varying holding cost in `value()` (a mean-shifter — leaves p90 at
311, loses 3.9 points); deleting the policy prior (worse against deceptive
opponents by 4.60 points, CI [2.63, 6.58]); a turn-transfer willingness ladder
(a genuine multi-candidate decision arises 0.148 times per game, worth ≈ 0.05
cards/game); and confidence-ranked declaration arbitration (+0.37 pp over 30,000
games). Two further mechanisms were **built** and then rejected on measurement:
scaling the ownership features by hit probability (−1.33 points) and a
(card, target) repetition guard (−6.13 points).

## What the study establishes

1. **v0.4's published evaluation could not see its own failure mode.** Against
   v0.3, 2.8% of v0.4's asks are provably dead and no game passes event 220. In
   mirror play 39.04% are provably dead, 40.03% are exact repeats, 34.33% of games
   contain a run of six or more consecutive dead asks, and 100% of forced-endgame
   declarations are wrong. Strong opponents — and strong humans — trigger it; weak
   ones do not, which is precisely why a weak-opponent win rate is not a
   sufficient commit gate.
2. **The deadlock is an ask-policy fixed point, not an information freeze.** It is
   a deterministic two-question cycle in ordinary mid-game positions, and the
   informational half of the v0.4 explanation is false on its own terms: 68.5% of
   legal asks inside a half-suit the asking team in fact owns strictly raise a
   teammate's exact probability of the correct allocation, and 59.9% strictly lower
   its exact marginal entropy. Ownership monotonicity does not imply informational
   monotonicity.
3. **The fix is refusal, not forcing.** Restricting the candidate set to asks the
   actor cannot prove are dead removes the pathology entirely — dead asks 39.04% →
   0%, dead runs 2,610 → 0, longest run 286 → 0, misdeclarations 10.44% → 2.07%.
   It is not free: on its own against v0.4 at seed 606060 the gate costs 1.33
   points (50.87% → 49.53%, intervals overlapping), and the shipped package is
   level with v0.4 head-to-head. What it buys is soundness, not points.
   v0.4's own two settings are both bad: never forcing
   deadlocks 22.5% of mirror games, always forcing terminates by misdeclaring.
4. **A capacity-infeasible allocation is a different kind of error from a
   low-probability guess.** It has posterior probability exactly zero, so it loses
   with certainty, and it silently inerts the willingness ladder built to prevent
   exactly that. Enumerating the ≤ 3⁶ feasible assignments and rejecting on
   capacity moves forced-endgame accuracy from 0.14% to 24.35% per declaring team
   over 24,000 games per arm, against a measured feasible ceiling of ≈ 40.6%.
5. **Removing the failure mode is worth almost nothing in win rate, and that is
   the result.** +1.11 points head-to-head at seed 515253, +0.79 pooled over five
   held-out banks — 50.11, 49.50, 52.33, 50.89, 51.11, so one bank below 50% and
   all five intervals containing it — means of 83.60% against 83.57%, and
   v0.4 better on minimax regret. The two configurations that score *highest* in
   the ablation table (56.6%, 56.4%) are the ones that keep the pathology. Win rate
   and soundness are dissociable here, and the study gates on soundness.
6. **The policy is markedly more robust to the manoeuvre that started the work.**
   The withholder archetype — hold cards of a half-suit you were asked for, then
   decline to ask back in it — holds v0.4 to 66.25% and 64.46% at two banks and
   v0.5 to 73.63% and 71.42%, a **+7.2** point gain replicated at both. The mechanism is not an opponent
   model; M3–M7 are unbuilt. It is that a misleading *absence* of asks has far less
   leverage over a policy that never spends turns on asks it can prove will miss.
7. **The same fit created a new, measured deception exposure.** The feint
   archetype manufactures a *false* ask-legality certificate, and the fit raised
   `priorTheta` from 0.2638 to 0.4446, so v0.5 weights "this player asked here"
   more heavily than v0.4 did. v0.5 is **2.2 points worse** against the feint,
   replicated at both banks. A five-point sweep over `priorTheta` at two seeds does
   not identify the parameter, so it cannot be tuned away; the principled fix is a
   per-seat online type posterior with data-biased shrinkage, and that is
   specified and not built.
8. **The declaration forecasts remain calibrated** — declaration Brier 0.0150,
   ECE 0.0154 over 3,609 forecasts — which is what makes the stopping rule a
   decision rule rather than a heuristic, and **the belief engine remains sound**:
   0 audit violations in 23,594,580 checks, 0 set-conservation failures,
   determinism PASS, and named-allocation probabilities matching exhaustive
   enumeration to 0.000e+00.
9. **The result is dialect-robust.** Across the default rules, `--no-out-of-turn`,
   `--no-cardless-declare` and the v0.3 legacy dialect, v0.5 against v0.4 stays
   between 51.60% and 52.47% at seed 828282 — no dialect moves it outside its own
   interval.

## What the study does not establish

Named here rather than buried, with the experiment that would settle each:

1. **No exploitability probe has been run against v0.5.** The v0.4 study fitted an
   adversary in the same policy class with the same optimiser and reported the
   achieved lower bound. v0.5 must not claim comparable robustness until
   `fish tune --panel=v05` followed by a held-out re-match is run. This is the
   largest hole in the evaluation.
2. **Robustness of the fitted `priorTheta` is measured only against three
   archetypes.** The exposure is real and replicated; its extent across a wider
   deception space is unknown. M7 plus a wider archetype panel would settle it.
3. **Termination is empirical, not structural.** With stage 2 deleted and the
   repetition guard rejected, nothing proves the game ends; what is measured is
   that no game in E1–E5 or E7 — 64,800 games, every one of the 37 recorded match
   rows at `limitHitRate` 0 and all three E2 columns at `action-limit games 0
   (0%)` — reaches the engine's 400-ask safety valve or the event-220 horizon.
   E8 and E10 cannot be cited for this: E8's artifact keeps only the declaration
   lines and E10 is a markdown table, so neither preserves the action-limit
   counter.
4. **The pre-gate false-negative audit has not been run for v0.5.** `gateaudit` is
   parsed only in the v0.4 branch of `engine/src/factory.hpp`, so the command
   reports a vacuous pass over zero opportunities.
5. **Decisions D1 and D2 are unimplemented.** The conventions flag and the
   partner-aware bot-teammate/human-teammate regimes are specified in
   `research/v05/BRIEF.md` and do not exist in the code, so this study reports one
   configuration where the brief asks for two, reported separately.

## Provenance gaps

- **Round 0 of the fit was abandoned at generation 19 and no record says why.**
  `research/v05/runs/fit-round0-abandoned.jsonl` is committed; the reason is not.
  This is the same class of gap as the v0.4 study's unrecorded round-5 base seed.
- **`freeze_config_v05.py` prints a round-trip assertion command that does not
  round-trip.** It writes ask weights with `%.4f` but prints the assertion with
  `%.5f`; at five decimals the mirror returns 50.83%, not 50%. The frozen vector is
  correct; the suggested check is mis-formatted. `docs/FISHBOT_V05.md` §10 gives
  the command that does hold.
- **The fitting hyperparameters are recorded in no machine artifact.**
  `research/v05/runs/fit-round1.jsonl` carries only per-generation scores, win
  rates and means plus the three final records; β, population, elite count, deals
  per cell and the panel membership appear nowhere in it. `build_tables_v05.py`
  therefore sources `\numFitBeta`, `\numFitElite`, `\numFitDeals`,
  `\numFitCellGames` and `\numFitPanel` from §9 of `docs/FISHBOT_V05.md` — a
  prose document, not an artifact — which is circular provenance and should be
  closed by having the tuner write a header record into the jsonl.
- **E10's exact invocation is not in `experiments_v05.sh`.** The deception panel
  was run standalone; the reconstructed command is recorded in
  `research/v05/results/MANIFEST.json` and reproduces the reported figures.

## Artifacts

Digests, commands and design notes for every file below are in
`research/v05/results/MANIFEST.json`, regenerated by
`python3 engine/build_manifest.py v05`.

- `research/v05/results/E1-verify.txt` — rules, information safety, belief soundness
- `research/v05/results/E2-pathology.txt` — the commit-gate KPIs, both mirrors and the cross match
- `research/v05/results/E3-headtohead.jsonl` — held-out head-to-head, five banks
- `research/v05/results/E4-perstyle.jsonl` — per-opponent profile, both arms
- `research/v05/results/E5-ablations.jsonl` — mechanism ablations
- `research/v05/results/E6-calibration-v05.txt` — forecast reliability
- `research/v05/results/E7-rules.jsonl` — rule dialects
- `research/v05/results/E8-forced-endgame.txt` — forced endgame at volume, per declaring team
- `research/v05/results/E9-throughput.txt` — engine throughput
- `research/v05/results/E10-deception.md` — deception panel, two banks
- `research/v05/results/C1-v04-corrections.md` — **the corrections register of record**
- `research/v05/results/P0-v04-pathology.md` … `P8-verify-turn-transfer.md` — the 28 diagnosis reports, each headline finding paired with its adversarial verification at an independent seed
- `research/v05/results/Plit-verify-holding-cost-claim.md` — the literature refresh's headline recommendation, re-tested in the engine: code facts confirmed, causal attribution refuted
- `research/v05/runs/fit-round1.jsonl`, `fit-round1.log` — the fitting trace that ships
- `research/v05/runs/v05-fitted.txt` — the frozen 34-coordinate vector
- `research/v05/runs/experiments_v05.log` — the battery log
- `research/v05/patches/` — M4/M5 and M7 patches, **unmeasured**
- `research/v05/lit/v05-refresh.md` — the literature refresh the design was drawn from
- `research/v05/BRIEF.md`, `DESIGN.md`, `PAPER_PLAN.md`, `RESULTS-SUMMARY.md`
