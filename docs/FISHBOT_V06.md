# FishBot v0.6 — specification

`engine/src/v06.hpp`, registered as `v06`. Study: `research/v06/`, paper: `paper/fishbot_v06.tex`.

v0.6 **derives** from v0.5 rather than forking it. With every v0.6 switch off and v0.5's parameter
vector restored (`v06:legacy=1`), every override defers to the base class and the two policies are
identical — checked by md5 of the full `fish pathology` transcript, not asserted. That is what makes
the ablation table exact rather than approximate.

## 0. The two shipped configurations

| name | spec | throughput | what it is |
|---|---|---:|---|
| **FishBot v0.6** | `v06` | 303 games/s | the deployed policy: v0.5's mechanisms, a vector from a repaired optimiser |
| **FishBot v0.6-Search** | `v06:s1=1,det=12,cand=4,kappa=2.5,roll=v06` | 0.14 games/s (1 thread) | v0.6 plus determinized information-set search; the strongest configuration measured |

On v0.5's parameter vector the same search takes **52.64%** against v0.5 over 2,160 games and three
banks (none below 52.08%). On v0.6's own vector, v0.6-Search takes **52.08%** against **v0.6
itself** over 2,880 games with all four cells above parity (worst 50.83%). It is off by default on cost grounds, not on evidence.

## 1. What v0.6 is

| component | state |
|---|---|
| M1 live-ask gating, M2 capacity-feasible allocation, M8 no event-count guillotine | inherited from v0.5, unchanged |
| the 37-coordinate parameter vector | **refit** with a repaired optimiser (§3); this is the strength change |
| exact-posterior tie resolution (`extie`) | built; **measured null** (§4.1) |
| three extra ask terms — void creation, team-ownership discount, last-live split (`xf`) | built and fitted; **the fit drives them to ≈0** |
| determinized information-set search (`s1`) | built, instrumented, **shipped off** (§4.3) |
| the rationed deliberate miss (`dead`, `deadsearch`) | built; inexpressible under the linear score, offered to the search |
| engine and harness corrections | shipped (§2) |

## 2. Corrections shipped in the engine

| # | defect | evidence | fix |
|---|---|---|---|
| E2 | `BlockDP::build` parked every instance's tables in one `thread_local` pool, so a second build on the same thread silently repointed the first instance's pointers | **285 mismatches in 294 checks** (`research/v05/results/P2-forced-endgame.md` §6) | a per-thread generation stamp plus a stored copy of the source `Knowledge`; every public query re-derives lazily when its stamp is stale. `fish blockalias` now reports 0 **query** mismatches; raw shared-pool field reads still differ and are documented, not fixed |
| E3a | `gateaudit` was parsed only in the v0.4 branch of `factory.hpp`, so v0.5's declaration pre-gate audit returned a vacuous pass over zero opportunities | — | parsed for v0.5 and v0.6. Result: **0 false negatives in 14,449,770 gate rejections over 5,472,906 declaration opportunities** — v0.5's pre-gates are exact, where v0.4's were not (1,017 / 24.1M) |
| E3b | `tuner.hpp` hard-coded `w.size() > 18` where `NFEAT` (=20) belongs, and `st.games * 2` where `st.games * rotations` belongs | — | both derived; the second would have silently reported a third of the true win rate at six rotations |
| E3c | `Knowledge::onEvent` appended a duplicate `Disjunction` on every repeated ask | store grows 6 → 26 → 46 carrying no information | opt-in `dedupDisj`, **off by default** — de-duplicating changes the Fast posterior, so v0.4 and v0.5 stay bit-identical |
| E5 | `runMatch` built one policy object for all three seats of a team, so the two partner regimes could not be expressed | — | `--partners=SPEC` names the policy for the two non-focal seats |

## 3. The repaired optimiser

Four measured defects in the v0.4/v0.5 fitting harness:

1. **One sigma for 34 coordinates** whose ranges span 0.1 to 40, so 93.4% of `declareMargin`
   proposals landed on a clamp bound at generation 0 while `valueWeight` moved 1.5% of its range;
   `--sigmaparams` was parsed at `main.cpp:224` and never referenced.
2. **An unpaired objective.** Candidates were compared on absolute win rate against a shared seed
   bank; common random numbers pair the deal but not the comparison.
3. **A "soft minimum" that is a weighted mean** — max/min gradient weight ratio 1.88 on v0.4's
   shipped profile.
4. **The rotation defect above.**

The repair: per-coordinate sigma from `sigmaRel * (hi - lo)`; a **paired** per-deal margin over the
incumbent (which is candidate 0 and is played on the same deals, so it is free); an explicit
objective dispatch over `{softmin, min, mean, regret, minimaxregret}`; a JSONL header record
carrying the whole configuration; and per-coordinate clip fractions logged every generation.

Two acceptance tests, both passing:

- a policy scored against itself returns a paired margin of exactly `0.0000` with a zero-width
  interval;
- a 10-generation fit from a deliberately handicapped base (`v05:w0=0`, 45.89% against v0.5)
  recovers to 48.33% against v0.5 and 76.83% against v0.3.

**The shipped fit.** `fitC` (`research/v06/runs/fitC.jsonl`): base `v06`, panel
`v05,v03,withholder,feint`, objective **minimax regret**, paired, 200 deals × 2 rotations per cell,
population 20 → 14, elite 4, 14 generations, per-coordinate sigma at 4% of range, fitting seed
**20260824** (disjoint from every evaluation bank), seeded from `fitA`
(seed 20260823, panel `v05,v03,lockout,withholder`, 30 generations).
`engine/freeze_config_v06.py` writes the **whole** 37-coordinate vector into `v06.hpp` together with
a provenance string naming the run file, its sha256 prefix, generation count, objective, panel and
seed — the v0.4 study shipped a value-function vector that no recorded fit produced, and this closes
that hole.

## 4. Mechanisms measured and their verdicts

### 4.1 Exact-posterior tie resolution — NULL

`fish v6probe --mode=ties --a=v05 --b=v05 --games=150 --seed=31` (artifact
`research/v06/results/E8-ties.txt`, 6,978 ask decisions): **54.74%** of contested decisions end in a
bit-for-bit tie at the top of the score; **93.77%** of those ties are two cards of one half-suit at
one target. The **exact** count law separates **0.00%** of them, and every tie-break rule —
enumeration order 43.81%, the deployed marginal 43.81%, v0.5's own chain/threat pass 43.76%, the
exact posterior 43.81% — realises the same hit rate; hindsight gets 70.24%. The ties are
exchangeable and irreducible.

**Consequence for the code:** because the answer is null, the exact-posterior machinery is an
*instrument*, not a mechanism. `V06Extra::exactTie` is not read by the ask rule and the shipped
policy never builds the exact posterior.

### 4.2 Three extra ask terms — NULL

Void creation, team-ownership discount, last-live split. The void term cleared a 95% paired interval
at 400 games per cell on one bank and returned zero at 1,000 on two. The fit assigns them
0.171, −0.477 and −0.777, and removing them (`v06:xf=0`) is **−0.13 points [−1.92, +1.67]**.

**A confound to state:** because a non-zero extra weight is what sets `extraFeats`, and `extraFeats`
is what selects the v0.6 scoring path, `xf=0` also restores v0.5's chain/threat re-scoring. The
clean four-way ablation (`research/v06/results/F1-chain2x2.json`, 800 games per cell, six-opponent
panel, seed 606060; negative delta = the variant is better):

| arm | extras | chain pass | v0.6 − variant | 95% paired CI |
|---|---|---|---:|---|
| `v06` | on | off | reference | |
| `v06:wvoid=0,wteam=0,wlast=0` | off | off | −0.46 | [−1.15, +0.23] |
| `v06:chain2=1` | on | on | −0.75 | [−2.44, +0.92] |
| `v06:xf=0` | off | on | −0.13 | [−1.96, +1.69] |
| `v06:rtie=1` (random tie-break) | on | off | −0.69 | [−2.54, +1.12] |

**All four are null.** v0.6 drops the chain pass because it is worth nothing and costs about 60% of
the policy's runtime (303 games/s against v0.5's 276), not because it hurts.

### 4.3 Determinized information-set search — POSITIVE, shipped as a named configuration

Not perfect-information Monte Carlo: the deal is determinized, but the six continuation players are
reconstructed at their own information sets (public deduction state refined by the determinized
hand), verified as a strict under-approximation of the seat's real knowledge — 0.334% of cards
differ, wider in 346 of 346 cases.

**Result 1, the correction that makes the rest possible.** Choosing by the argmax of the rollout
means is **35.69 points worse** than the blueprint-forced control at the same budget, sample size,
seed and rollout policy (49.31% against 13.61%, 720 games each). One determinization's return is the
final half-suit differential, sd about 2.5, while genuine differences between candidates are an
order of magnitude smaller. A paired lower-confidence-bound rule — deviate only when the improvement
over the blueprint's own choice, on the same determinizations, clears κ standard errors — recovers
all of it, monotonically in κ.

**Result 2, the positive one.** At κ = 2.5 the guarded search takes **52.64%** against v0.5 over
2,160 games and three banks (53.75 / 52.08 / 52.08, none below 52.08), and — the number that matters
— **52.08%** against **v0.6 itself** over 2,880 games with all four cells above parity
(52.92 / 50.83 with a v0.6 rollout, 51.81 / 52.78 with a v0.5 rollout; worst cell 50.83%).

**Result 3, two negative controls that make it about information.** Resolving the tie group
uniformly at random, seeded from the public event stream, is worth *exactly* nothing —
50.00% [47.18, 52.82], mean half-suits 4.500 to 4.500. Resolving it with one sampled deal is worth
nothing — 49.58%. Twelve are worth points. The separating signal is therefore in the **joint**
posterior, which every marginal integrates away — and that is exactly why §4.1's exact-marginal
tie-break measures zero.

**Not resolved:** where inside the search the gain sits. Guarding the tie group gives 49.86%;
restricting the search to the tie group gives 51.94 / 50.69 / 50.00; the unrestricted search gives
52.64%. At 720 games a cell those intervals overlap.

**Deviation rate.** At kappa = 2.5 the search moves 33.49% of the decisions it searches, and that
rate never falls below ~31% however far kappa is raised — because the deviation penalty is waived
inside the blueprint's tie group, which is more than half of all decisions. Applying the penalty
there too collapses the rate to **2.70%**, inside the band the literature associates with a
refinement, and takes the win rate to 49.86% — the advantage goes with the waiver.

**Cost.** 0.144 games/s on one thread against v0.6's 361.8 across all threads, which is why the
search is a separately named configuration and not the default.

### 4.4 The deliberate miss

M1 deletes every provably-dead ask and with it blackballing, deliberate turn donation and the costly
safe-ask signal, which exist at 79.2% of v0.5's decisions. Unbanning it wholesale scores **higher**
(51.17% [48.34, 53.99] against v0.5) and fails the commit gate: 35.85% dead asks, longest dead run
365, 10% of games killed by the action limit. Rationing it against the linear score is inert — the output is
bit-identical at every admission margin, because the hit-probability term is zero on such an ask by
definition and every remaining term is a penalty. It is offered to the search instead, under an
anti-repeat guard that is **provably free**: a dead (card, target) pair stays dead unless the card
publicly moves to that target, and that movement is observable.

## 5. Configuration grammar

`v06[:key=value,...]`. Accepts every v0.5 key, plus:

| key | meaning | default |
|---|---|---|
| `legacy=1` | restore v0.5's parameter vector (the identity control) | off |
| `s1` | test-time search | **off** |
| `det`, `cand`, `maxcand`, `depth`, `kappa`, `kappatie`, `tieonly`, `maxq`, `from`, `blend` | search knobs | see `v06.hpp` |
| `roll`, `rbelief`, `rsouter`, `rsinner`, `rvalue` | the rollout blueprint | `v05:topk=0,souter=1,sinner=1` |
| `dead`, `deadmargin`, `deadbudget`, `deadsearch` | the deliberate miss | off |
| `rtie` | resolve the blueprint's tie group at random, seeded from the public event stream | off; measured at **exactly 50.00%**, so it costs nothing and removes a deterministic tie-break a repeat human opponent could learn |
| `chain2` | restore v0.5's top-K chain/threat re-scoring | off |
| `xf`, `wvoid`, `wteam`, `wlast` | the three extra ask terms | fitted |
| `extie`, `exactp`, `exmaxq` | exact-posterior resolution | `extie=1`, `exactp=0` |
| `gateaudit` | declaration pre-gate audit (now parsed for v0.5+) | off |

## 6. Reproducing

```bash
cd engine && make
./experiments_v06.sh                 # the battery, E0-E15
./exploitability_v06.sh              # the best-response probe, positive-controlled on v0.4
python3 build_tables_v06.py          # artifacts -> paper/numbers_v06_generated.tex
cd .. && npm run paper:v06           # -> output/pdf/fishbot_v06.pdf
```

## 6b. Exploitability

`./exploitability_v06.sh` with `BASE=v05 TARGETS="v04 v05 v06" GENS=12 GAMES=180 POP=18 EVAL=600`,
fit seed 515253, fresh evaluation bank 6543210. Lower is better for the frozen policy.

| frozen target | response win rate | 95% CI |
|---|---:|---|
| v0.4 (positive control; published 51.19% [49.67, 52.72]) | 50.69% | [49.06, 52.31] |
| v0.5 | 50.31% | [48.75, 51.89] |
| **v0.6** | **48.36%** | **[46.75, 49.97]** |

A lower bound within the searched class. The comparison is like-for-like and is the result.

## 7. Known gaps

- The null results are null at 1,000 deals per cell against this panel. They do not exclude effects
  of a quarter of a point.
- The search verdict is conditional on the variance of the terminal return and the fidelity of the
  rollout blueprint. Rebuilding the leaf evaluator should re-open it.
- Exploitability is a lower bound within the searched policy class.
- Termination remains empirical, not proved.
