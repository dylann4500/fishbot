# FishLab v0.3 findings

Run date: 2026-08-20. Scope: six players, 54 cards, nine half-suits, wrong declarations awarded to the opposing team.

## Study size and separation

The current v0.3 program scheduled 126,600 games:

- 24,000 in the initial search and validation;
- 24,600 after adding the literature-derived lockout challenger;
- 54,000 in held-out tests, mechanism ablations, ask-response checks, and the ordered matrix;
- 24,000 in a post-test local stability sweep that did not justify changing the frozen configuration.

Training, validation, held-out, matrix, and paired-ablation stages use separate base seeds. Direct tests swap FishBot between Team A and Team B.

## Held-out performance

Each row pools 1,000 untouched games in each team orientation.

| Opponent | FishBot v0.3 win rate | 95% Wilson interval | Mean score | Ask accuracy | Declaration accuracy |
|---|---:|---:|---:|---:|---:|
| Turn-starvation lockout | 57.85% | 55.67–60.00% | 4.801 | 52.21% | 96.11% |
| Posterior detective | 57.20% | 55.02–59.35% | 4.767 | 53.60% | 95.01% |
| Sanitized FishBot v0.2 | 56.50% | 54.32–58.66% | 4.770 | 52.29% | 96.23% |
| Adaptive diversifier | 86.15% | 84.57–87.59% | 5.978 | 63.70% | 93.95% |
| Focused hunter | 95.15% | 94.12–96.01% | 6.747 | 55.30% | 91.56% |
| Misdirection artist | 99.20% | 98.70–99.51% | 7.438 | 57.33% | 90.24% |
| Random legal control | 100.00% | 99.81–100.00% | 8.400 | 52.38% | 93.91% |

The direct margins over all three credible challengers are statistically separated from 50%. Ask accuracy is not monotonically related to win rate, which is why final score and declaration quality remain essential.

## Paired mechanism results

Positive values mean full v0.3 won more often than the ablated version on matched deals against detective and lockout.

| Ablation | Full minus ablated | 95% paired interval |
|---|---:|---:|
| Remove ask history | +38.75 points | 36.01–41.49 |
| Remove count conditioning | +5.20 points | 2.18–8.22 |
| Keep only immediate transfer | +3.20 points | 0.25–6.15 |
| Remove completion | +1.15 points | -0.52–2.82 |
| Remove continuation | +0.35 points | -1.17–1.87 |
| Remove reply risk | +0.30 points | -1.06–1.66 |
| Remove team control | -0.80 points | -3.29–1.69 |
| Restore entropy premium | -0.50 points | -2.70–1.70 |

The human-facing conclusion is strong: exact ask memory and card-count reconciliation are the priority. Chaining, completion, control, and blackballing remain plausible tie-breaks, but their isolated weights are not established by these samples.

## Psychological response rules

Enabling deterministic reactive bonuses for baseline opponents changed v0.3's matched win rate by -0.8, -0.5, and -0.5 points against detective, lockout, and bluffer. FishBot v0.3 therefore does not hardwire a same-suit emotional response or diversion rule. It always treats public asks as evidence and then maximizes the current numeric utility.

## Reproducibility artifacts

- `research/results/optimization.json`
- `research/results/optimization-lockout.json`
- `research/results/final-evaluation.json`
- `research/results/refinement.json`
- `research/results/paired-ablations.json`
- `paper/fishbot_v03.tex`
- `paper/FISHBOT_V03_OVERLEAF.md`
- `output/pdf/fishbot_v03.pdf`
