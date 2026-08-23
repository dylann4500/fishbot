# FishBot v0.6 — investigation brief

Repository root: `/Users/dylan/Documents/GitHub/fish optimization`.
Engine: `engine/` (C++20, `cd engine && make` → `./fish`). Baseline: `bd812fe` ("v0.5").

## The mandate

Build the next FishBot: one that finds the best move in the current position through whatever
multi-step reasoning the position needs, for the **team** rather than for the seat, while staying
sound — no cycling, no misdeclaration at the action cap — and that is *demonstrably* the strongest
FishBot, not merely the newest.

## What v0.5 left behind

`research/v05/DESIGN.md` §1 designed ten mechanisms and built three (M1 live-ask gating, M2 the
capacity-feasible allocator, M8 the removal of the event-count guillotine). It shipped with
M3–M7, M9 and M10 unbuilt, with no exploitability measurement of any kind, and with an explicit
non-claim: *"Removing the failure mode is worth almost nothing in win rate, and that is the result."*

## The owner's question, answered from the code

**Does FishBot optimise for itself or for the team?** For the team, unambiguously, in its objective:
every quantity the value function reads is team-relative — `scoreDiff` is the team's set
differential, `ourCards`/`theirCards` are team card totals, and `eH[s]` is the expected fraction of
half-suit *s* held by the team, not by the seat (`engine/src/v05.hpp`, `computeAggregates` and
`value`). There is no per-seat score in Fish and none in the policy.

Its *policy*, though, is unilateral. v0.5 builds a `Knowledge` object for itself and for nobody
else — the only two clones in the whole policy are clones of its own — so no term anywhere can ask
"what does my partner know?" or "which of us should act?". It is team-objective and
partner-blind. Where teammates enter at all they enter as aggregate probability mass. That
distinction is what this study set out to close, and §mechanisms records how far it got.

## Standing preferences of record (carried forward)

- Never headline an aggregate win rate alone: report the per-opponent breakdown, an explicit
  worst case across styles, and an exploitability probe.
- Treat "exact Bayesian inference" as an assumption to test, not a claim.
- When a claim is unsupported and the evidence is cheap to produce in the engine, produce the
  evidence rather than hedging the prose.
- Partner-aware play: report the bot-teammate and human/unknown-teammate regimes separately, and
  never headline the self-play configuration as though it were the one a human will meet.
- Conventions ship behind a flag, off by default, with the delta published as a result.

## Method note that governs this study

Everything here is measured with a **paired** estimator (`fish ablate`: every variant plays the same
deals against the same panel; the per-deal margin is bootstrapped resampling deals as clusters).
This was not a stylistic choice. Five mechanisms in this study cleared a 95% interval at
400–1,600 games per cell and returned exactly zero at 3,000 (`research/v06/notes/R12` §6.1). The
evidence standard used by most published ablation rows in this project cannot separate a real
one-point effect from a seed draw, and v0.6's first obligation was to fix that.

## Index

| document | contents |
|---|---|
| `research/v06/DESIGN.md` | the mechanism specification and build order |
| `research/v06/notes/R0-OPPORTUNITY-REGISTER.md` | synthesis of the nine recon reports |
| `research/v06/notes/R1`–`R9` | recon: policy anatomy, inference stack, unshipped M4/M5/M7, harness, prior studies, residual diagnosis, literature, game structure, human tactics |
| `research/v06/notes/R10-owner-measurements.md` | throughput, state-space collapse, rollout cost, the tie structure |
| `research/v06/notes/R11-search-feasibility.md` | the search build and the optimizer's-curse result |
| `research/v06/notes/R12-mechanism-trials.md` | every mechanism tried, with its verdict |
| `research/v06/runs/` | fitting artifacts |
| `research/v06/results/` | the experiment battery |
