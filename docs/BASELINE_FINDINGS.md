# Baseline findings

Run date: 2026-08-20. These are simulator findings, not claims about human tournament play.

## Pairwise policy study

The baseline used 1,000 seeded games for every ordered matchup (25,000 games total), with psychological tells and strategic declarations enabled. Rows played Team A; columns played Team B.

| Team A policy | Focus | Diversify | Infer | Bluff | Random | Row average |
|---|---:|---:|---:|---:|---:|---:|
| Focused hunter | 50.6% | 28.2% | 6.4% | 82.0% | 81.0% | 49.6% |
| Adaptive diversifier | 70.8% | 49.5% | 18.7% | 96.6% | 99.5% | 67.0% |
| Bayesian detective | 91.7% | 81.1% | 51.1% | 98.4% | 99.9% | 84.4% |
| Misdirection artist | 17.2% | 3.6% | 1.4% | 48.2% | 17.1% | 17.5% |
| Unpredictable novice | 18.7% | 0.1% | 0.0% | 81.2% | 50.7% | 30.1% |

The diagonal is approximately 50%, which is a useful check that the deal/seat orientation is not creating a large phantom advantage. The Bayesian detective is the strongest policy in this population. Its advantage is not merely “asking more”: in a separate 5,000-game orientation-balanced comparison with the diversifier, combined declaration accuracy was about 90%, versus roughly 62.5% in hunter/diversifier games and 45% in bluffer/diversifier games. The present model strongly rewards waiting until card-to-teammate allocation is well supported.

## Diversion ablation

Matched 5,000-game runs toggled psychological tells while preserving seeds. The win-rate change was small:

- Bluffer versus diversifier: 3.58% with tells, 3.34% without (+0.24 points).
- Detective versus diversifier: 80.14% with tells, 79.92% without (+0.22 points).
- Hunter versus diversifier: 28.38% with tells, 27.58% without (+0.80 points).

Within this belief model, the immediate diversion response is at most a marginal advantage. The large weakness of the bluffer archetype comes primarily from its aggressive declaration threshold, not from diversion itself. That is an important negative result: it suggests that a human-looking bluff policy should be tested independently of declaration risk rather than bundling the two.

## What “strong play” currently looks like

The best baseline play is conservative about declarations and aggressive about information quality. It treats a public ask as evidence, targets locations with the highest posterior probability, and avoids donating sets on uncertain teammate allocation. Persistent half-suit pursuit beats random play, but diversification beats focus in their direct matchup, suggesting that maintaining multiple live avenues is useful until ownership evidence becomes concentrated.

## Next experiments

1. Factor declaration threshold out of every archetype and sweep it independently.
2. Add exact card-count conditioning to the belief state and measure whether detective performance survives.
3. Separate “diversion frequency” from “belief that others respond to diversions” in a 2×2 experiment.
4. Run policies in both team orientations on paired seeds and bootstrap confidence intervals.
5. Begin outcome-sampling MCCFR self-play, using these fixed archetypes as exploitability probes.
