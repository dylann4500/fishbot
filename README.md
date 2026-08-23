# FishLab

FishLab is a Canadian Fish simulation and research workbench. **FishBot v0.6** is the current
release. It combines an exact observer-conditioned posterior over the initial deal — used as a
reference and validation oracle — with a faster approximate inference path that the deployed policy
runs. v0.4 and v0.5 stay byte-identical and remain the reference opponents. All three live in the
C++ engine under `engine/`. The browser lab (`app/`, `lib/fish-engine.ts`) hosts the earlier v0.3
population and the interactive replay.

## FishBot v0.6

Two configurations ship, following this project's convention of naming both:

- **FishBot v0.6** — the deployed policy. Same mechanisms as v0.5, a parameter vector found by a
  repaired optimiser, and **303 games/s** (v0.5: 276, v0.4: 148).
- **FishBot v0.6-Search** — v0.6 plus a determinized information-set search at test time. The
  strongest configuration measured, at three orders of magnitude less throughput.

### What is established

- **Robustness across playstyles.** Pooled over three disjoint seed banks and a thirteen-style set,
  **minimax regret 3.06 for v0.6 against 8.60 for v0.5 and 11.65 for v0.4**. v0.6 is ahead of v0.5 on
  **7** of the thirteen styles, behind on **5** and equal on **1**, with a largest single-style loss
  of 1.72 points, and its worst cell equals v0.5's.
- **Against the withholder** — the deception manoeuvre the project owner brought here from live play
  (hold cards of a half-suit you were asked for, then decline to ask back in it) — **+8.60 points**
  over 4,800 games, replicating at all three banks (+9.06, +8.33, +8.34).
- **Head to head, v0.6 and v0.5 are NOT separated:** 50.53% over 11,300 games, 5 of 7 cells above
  parity, naive z = 1.13. Five held-out banks show v0.6 ahead and two further default-rules cells
  show it behind. Quoting the five that agreed would have been selection on the experiment.
- **Test-time search is the one place a multi-step method beats a static rule.** On v0.5's vector it
  takes **52.64%** against v0.5 over 2,160 games and three banks, none below 52.08%. On v0.6's own
  vector it takes **52.08%** against v0.6 over 2,880 games, **all four cells above parity**, worst
  50.83%. That second number is why v0.6-Search is a configuration worth naming.
- **The optimizer's curse is worth 35.69 points.** An unguarded argmax over the rollout means scores
  13.61% against a 49.31% blueprint-forced control at identical budget, seed and sample size. A
  paired lower-confidence-bound deviation rule recovers all of it. This is the largest single effect
  measured anywhere in this codebase.
- **Two negative controls make the search result about information, not variety.** Resolving the
  55%-of-decisions tie group uniformly at random, seeded from the public event stream, is worth
  *exactly* nothing — 50.00%, mean half-suits 4.500 to 4.500. One sampled deal is worth nothing.
  Twelve are worth points. The signal is in the **joint** posterior, which every marginal discards.
- **Exploitability, measured for the first time in this project.** A best response fitted from the
  same policy family with the same budget reaches parity against v0.4 (50.69% [49.06, 52.31],
  reproducing the published 51.19% control) and against v0.5 (50.31% [48.75, 51.89]), and **fails to
  reach parity against v0.6 (48.36% [46.75, 49.97])**. It is a lower bound within the searched
  class, so the comparison is the result, not the level.
- **The advantage is a self-play advantage.** Against three v0.5 opponents, v0.6 beats v0.5 by 2.25
  points with two copies of itself as partners, by 1.4 with v0.3 partners, by −0.1 with detective
  partners and by −0.8 with withholder partners. A refit scored in self-play buys coordination with
  copies of itself, and it does not transfer. This is the owner's decision D2 measured rather than
  asserted, and it is the sharpest limitation on everything above.
- v0.6 passes the pathology commit gate identically to v0.5 (0 games at the action limit, longest
  dead run 1) and declares more accurately: 98.50% against 97.47% over 9,000 games.

**The deployed gain is entirely parametric, and three of the four things built are negative results.**

| what was built | verdict |
|---|---|
| a **repaired optimiser** — per-coordinate step sizes, a paired per-deal objective, an explicit minimax-regret dispatch | **this is the strength change.** v0.5's own 40-generation refit was indistinguishable from sampling noise (OLS slope +0.00049/gen, t = 1.60), so its shipped vector was v0.4's |
| **exact-posterior tie resolution** | **null.** 54.74% of v0.5's ask decisions end in a bit-for-bit tie; 93.77% are two cards of one half-suit at one target; the **exact** posterior separates **0.00%** of them, and enumeration order, the deployed marginal, v0.5's own two-ply pass and the exact posterior all hit at 43.81/43.81/43.76/43.81%. The ties are exchangeable and irreducible, so the mechanism was never wired into the policy |
| **exactness in the ask marginal** | **worse.** Scored as predictors on 140,661 unresolved cards from identical states, the exact count law hits 47.94% at 1.42246 nats against the deployed approximation's 51.49% at 1.38218. The policy prior is worth about 1.5 points of that; the rest survives with the prior deleted, so the approximation's max-entropy smoothing is itself the better predictor |
| **determinized information-set search** | **the one positive mechanism.** Unguarded it is −35.69 points; guarded by a paired lower-confidence-bound rule it takes 52.64% against v0.5 over 2,160 games and 52.08% against v0.6 over 2,880 games with all four cells above parity. Shipped as the separate **v0.6-Search** configuration, off by default, because it costs three orders of magnitude in throughput |
| three extra ask terms (void creation, team-ownership discount, last-live split) | **null**; the fit gives them small weights (0.171, −0.477, −0.777) and removing them is −0.13 points [−1.92, +1.67] |
| the **deliberate miss** that M1 deletes | raises the win rate to 51.17% [48.34, 53.99] and fails the commit gate: 35.85% dead asks, longest dead run 365, 10% of games killed by the action limit |

Five mechanisms in this study cleared a 95% paired interval at 400–1,600 games per cell — the budget
most published ablation rows in this project were collected at — and returned **exactly zero** at
3,000 on two disjoint banks. Repairing the evidence standard is why the refit was worth anything,
and it is also why the head-to-head claim above is stated as a null.

**Engine corrections shipped**: the exact block dynamic program parked every instance's tables in one
per-thread pool, so a second construction silently repointed the first instance's tables (**285
mismatches in 294 checks**) — harmless under the deployed approximate belief, fatal under any exact
one, which is exactly how it survived three studies; and the declaration pre-gate audit, dead code
for v0.5 because the option was parsed only in the v0.4 branch, now reports **0 false negatives in
14,449,770 gate rejections over 5,472,906 declaration opportunities**.

Specification: `docs/FISHBOT_V06.md`. Results of record: `research/v06/RESULTS-SUMMARY.md`. Design
spec, recon reports R0–R12 and fitting artifacts: `research/v06/`. Paper: `paper/fishbot_v06.tex`.

```bash
cd engine && make                        # clang++ -std=c++20 -O3, produces ./fish

./fish verify --games=600                # rules + information safety + belief soundness
./fish pathology --a=v06 --b=v06 --games=300 --seed=31       # the commit-gate KPIs
./fish match --a=v06 --b=v05 --games=300 --rotations=6 --seed=90210
./fish v6probe --mode=ties   --a=v05 --b=v05 --games=150 --seed=31   # the tie structure
./fish v6probe --mode=belief --a=v05 --b=v05 --games=120 --seed=31   # belief as a predictor
./fish v6probe --mode=search --a="v06:s1=1,det=12,cand=4,kappa=2.5" --b=v05 --games=20

./experiments_v06.sh                     # the full battery E0-E15
./exploitability_v06.sh                  # best-response probe, positive-controlled on v0.4
python3 build_tables_v06.py              # artifacts -> paper/numbers_v06_generated.tex
cd .. && npm run paper:v06               # -> output/pdf/fishbot_v06.pdf
```

## FishBot v0.5 (previous release)

**v0.5 is not meaningfully stronger than v0.4.** It is +1.11 points head-to-head
at the profile seed and +0.79 pooled over five held-out banks (one of the five
below 50%, and every bank's interval contains 50%), the nine-opponent means are a wash (83.60% against 83.57%), and v0.4
is marginally better on minimax regret over the style set (1.61 against 1.78). What v0.5
delivers is the **elimination of a failure mode**, plus a large replicated gain
against the deception archetype that motivated the work and a smaller, equally
replicated loss against a different one. Read the per-opponent table below; there
is no headline win rate here.

v0.4 had a failure mode its published evaluation could not see. Against a weak
opponent it is fine; in **mirror play** 39.04% of its asks are ones it could prove
are guaranteed misses, 40.03% are exact repeats, 34.33% of games contain a run of
six or more consecutive dead asks, and 100% of its forced-endgame declarations are
wrong. A strong human is closer to the mirror case than v0.3 is, which is why the
user report that started v0.5 — bots looping forever, then misdeclaring at the end
— matches the mirror numbers and not the published ones.

**What was fixed** (`engine/src/v05.hpp`, registered as `v05`): **M1** live-ask
gating, which restricts the candidate set to asks the actor cannot prove are dead;
**M2** a capacity-feasible joint allocation for every declaration path; **M8**
removal of v0.4's event-count forcing guillotine. Mechanisms M3–M7, M9 and M10 are
designed and **not built**. Two further mechanisms were built, measured and
**rejected**: scaling the ownership features by hit probability (`m1p`, −1.33
points) and a (card, target) repetition guard (`norepeat`, −6.13 points).

**The failure mode is gone** (600 mirror games per arm, seed 31,
`research/v05/results/E2-pathology.txt`):

| | v0.4 mirror | v0.5 mirror |
|---|---:|---:|
| provably dead asks | 39.04% of asks | **0%** |
| dead runs (longest) | 2,610 (**286**) | **0** |
| games with a dead run ≥ 6 | 34.33% | **0%** |
| exact repeat asks | 40.03% | **2.63%** |
| declarations wrong | 10.44% | **2.07%** |
| declarations at/after event 220 | 768, 58.59% wrong | **0** |
| ask hit rate | 34.25% | **55.47%** |
| events/game (p90, p99) | 143.6 (312, 321) | **96.6 (112, 125)** |

Forced-endgame declarations, read per declaring team over 24,000 games per arm
(`E8-forced-endgame.txt`): v0.4 enters one 0.0307 times per game and is right
**0.14%** of the time; v0.5 enters one 0.0048 times per game and is right
**24.35%** of the time, against a measured feasible ceiling of ≈ 40.6%.

**Per-opponent, with the worst case stated** (300 deals × 6 rotations per cell,
seed 515253, `E4-perstyle.jsonl`; the deception panel is 400 × 6 at two seed
banks, `E10-deception.md`):

| opponent | v0.5 | v0.4 | delta |
|---|---:|---:|---:|
| v0.4 (mirror strength) | 51.11% | 50.00% | +1.11 |
| v0.3 | 72.33% | 73.33% | −1.00 |
| v0.2 | 81.28% | 83.06% | −1.78 |
| lockout | 79.56% | 77.94% | +1.61 |
| detective | 76.78% | 77.28% | −0.50 |
| diversifier | 93.78% | 92.89% | +0.89 |
| hunter | 97.72% | 97.67% | +0.06 |
| bluffer | 99.89% | 99.94% | −0.06 |
| random | 100.00% | 100.00% | 0.00 |
| **withholder** (deception) | **73.63% / 71.42%** | 66.25% / 64.46% | **+7.2** |
| silent (deception) | 80.42% / 83.17% | 79.96% / 79.00% | +2.3 |
| **feint** (deception) | **50.96% / 52.08%** | 54.13% / 53.29% | **−2.2** |
| **worst case over all twelve** | **50.96% (feint)** | **50.00% (mirror)** | |
| mean over the nine standard styles | 83.60% | 83.57% | |
| minimax regret over the nine | 1.78 (on v0.2) | **1.61** (on lockout) | |

The withholder is the project owner's own manoeuvre — hold cards of a half-suit
you were asked for, then decline to ask back in it — and v0.5 gains 7.2 points on
it at both seed banks, not because it models the opponent (M7 is unbuilt) but
because it no longer spends turns on asks it can prove will miss, so a misleading
*absence* of asks has far less leverage. The feint manufactures a false ask-legality
certificate instead, and v0.5 is 2.2 points worse against it at both banks, because
the fit raised `priorTheta` from 0.2638 to 0.4446. Both directions replicate; both
are reported.

Specification: `docs/FISHBOT_V05.md`. Study design, seed banks and artifact index:
`docs/V05_FINDINGS.md`. Corrections the v0.5 study makes to the v0.4 study:
`research/v05/results/C1-v04-corrections.md`. Design spec, diagnosis reports and
literature refresh: `research/v05/`.

```bash
cd engine && make                        # clang++ -std=c++20 -O3, ~6 s, produces ./fish

./fish verify --games=600                # rules + information safety + belief soundness
./fish pathology --a=v05 --b=v05 --games=300 --seed=31    # the commit-gate KPIs
./fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed=90210

./experiments_v05.sh                     # the full battery E1-E9, ~7 min
python3 build_tables_v05.py              # artifacts -> paper/numbers_v05_generated.tex
python3 build_manifest.py v05            # artifact digests -> research/v05/results/MANIFEST.json
cd .. && npm run paper:v05               # -> output/pdf/fishbot_v05.pdf

cd engine && ./fish serve                # then open http://127.0.0.1:8173
```

`--games` counts *deals* and `--rotations` defaults to 2, so `--games=300` alone
is 600 games and `--games=300 --rotations=6` is 1,800. Re-running
`./experiments_v05.sh` from a clean rebuild reproduces E1, E2, E6 and E8
byte-for-byte; E3, E4, E5 and E7 reproduce identically in every reported quantity
but carry a wall-clock `seconds` field, so their digests move; E9 is a throughput
measurement and is machine-dependent. `MANIFEST.json` pins the artifacts of
record, not the re-run.

## FishBot v0.4 (published)

Because every card movement in Fish is public, the entire hidden state is the
initial deal. That makes the posterior exactly computable, including the
certificate that an ask carries about the asker's own hand, and it yields a
theorem: a half-suit held entirely by one team can never be asked in by the
other, so its ownership can never change, so waiting to claim it carries no
ownership risk.

Two configurations are distinguished throughout:

- **v0.4-Fast** — the default, deployed and primarily evaluated policy
  (`BeliefMode::Fast`). Every reported performance number is this one.
- **v0.4-Block** — the same fitted policy with the exact reference belief
  substituted (`v04:belief=block`). It validates the probabilities and serves as
  an ablation; it does not run in the inner loop.

See `docs/FISHBOT_V04.md` for the specification and `paper/fishbot_v04.tex` for
the full study.

```bash
cd engine && make
./fish verify    --games=600                    # rules + belief soundness audit
./fish selftest  --games=40                     # reference engine vs card DP vs sampling
./fish oracle    --games=150                    # brute-force allocation oracle
./fish gateaudit --games=700 --rotations=6      # declaration pre-gate false-negative audit
./fish match --a=v04 --b=v03 --games=700 --rotations=6 --seed=90210
./experiments.sh                                # the full battery
python3 build_manifest.py                       # artifact checksums + MANIFEST.json
```

## Play it yourself

`fish serve` opens a browser table where any mix of humans and bots takes the six
seats. It is the same `Game` driver every published number came from — a human
seat is just another `Agent` — so what you are playing is the deployed policy,
not a reimplementation of it.

```bash
cd engine && make
./fish serve                                    # then open http://127.0.0.1:8173
```

Give each seat an engine and a name — six seats all labelled "FishBot v0.4" are
impossible to track — then deal. Presets cover you plus two v0.4 teammates against
three v0.4s, and you plus two v0.3 teammates against three v0.4s. You are sent
your own hand and the public event stream and nothing else. See `docs/PLAY.md`.

## Run locally

```bash
npm install
npm run dev
```

Open `http://localhost:3000`.

## Headless research

Run a full pairwise strategy matrix:

```bash
npm run research -- --games=1000
```

The browser interface supports 100–5,000 games per experiment. The engine is in `lib/fish-engine.ts`; FishBot v0.2 is specified in `docs/FISHBOT_V02.md`, and the path toward equilibrium play is documented in `docs/METHODOLOGY.md`. Reproducible findings from the initial 85,000-game study and the 46,000-game v0.2 study are in `docs/BASELINE_FINDINGS.md` and `docs/V02_FINDINGS.md`.

FishBot v0.3 is specified in `docs/FISHBOT_V03.md`; its held-out results are in `docs/V03_FINDINGS.md`. The v0.3 paper is `paper/fishbot_v03.tex` with a verified PDF at `output/pdf/fishbot_v03.pdf`. FishBot v0.4 is specified in `docs/FISHBOT_V04.md`, its study design in `docs/V04_FINDINGS.md`, its generated result tables in `docs/V04_RESULTS.md`, and its paper in `paper/fishbot_v04.tex` (single-file Overleaf copy: `paper/fishbot_v04_standalone.tex`, built PDF: `output/pdf/fishbot_v04.pdf`).

FishBot v0.5 is specified in `docs/FISHBOT_V05.md`, its study design in `docs/V05_FINDINGS.md`, and its paper in `paper/fishbot_v05.tex` (built PDF: `output/pdf/fishbot_v05.pdf`). Its investigation brief, design spec and paper plan are `research/v05/BRIEF.md`, `research/v05/DESIGN.md` and `research/v05/PAPER_PLAN.md`; the diagnosis reports P0–P8 (each headline finding adversarially re-verified at independent seeds), the battery artifacts E1–E10 and the corrections register C1 are in `research/v05/results/`, with digests in `research/v05/results/MANIFEST.json`; the literature refresh is `research/v05/lit/v05-refresh.md`.

## Reproduce v0.3

```bash
npm run verify:engine
npm run optimize:fishbot
npm run evaluate:fishbot
npm run refine:fishbot
npm run ablations:fishbot
npm run paper:markdown
```

## Design principles

- Hidden information stays hidden from acting agents.
- Every deal and decision is reproducible from a seed.
- Strategy is expressed through inspectable numeric policies, not opaque prose calls in the hot loop.
- LLMs are best used outside the loop for policy ideation and replay interpretation; deterministic simulation supplies the evidence.
- Final claims use held-out, orientation-balanced seeds; matrix row averages are descriptive, not substitutes for direct tests.
