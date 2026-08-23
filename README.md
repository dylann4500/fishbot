# FishLab

FishLab is a Canadian Fish simulation and research workbench. **FishBot v0.4** and
**FishBot v0.5** both combine an exact observer-conditioned posterior over the
initial deal — used as a reference and validation oracle — with a faster
approximate inference path that the deployed policy runs. v0.5 is v0.4 with three
measured defects fixed; v0.4 stays byte-identical and is the reference opponent.
Both live in the C++ engine under `engine/`. The browser lab (`app/`,
`lib/fish-engine.ts`) hosts the earlier v0.3 population and the interactive
replay.

## FishBot v0.5

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
