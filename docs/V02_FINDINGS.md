# FishLab v0.2 findings

Run date: 2026-08-20. This study evaluates the simulator’s policy population; it is not yet evidence about expert human play.

## Six-policy matrix

The v0.2 baseline ran 1,000 games for every ordered matchup (36,000 games). Rows play Team A; columns play Team B.

| Team A policy | FishBot | Focus | Diversify | Infer | Bluff | Random | Row average |
|---|---:|---:|---:|---:|---:|---:|---:|
| FishBot v0.2 | 49.2% | 92.8% | 73.4% | 47.9% | 99.1% | 100.0% | 77.1% |
| Focused hunter | 7.6% | 50.6% | 28.2% | 6.4% | 82.0% | 81.2% | 42.7% |
| Adaptive diversifier | 23.4% | 70.8% | 49.5% | 18.7% | 96.6% | 99.9% | 59.8% |
| Bayesian detective | 51.4% | 91.7% | 81.1% | 51.1% | 98.4% | 100.0% | 79.0% |
| Misdirection artist | 1.0% | 17.2% | 3.6% | 1.4% | 48.2% | 17.0% | 14.7% |
| Unpredictable novice | 0.1% | 19.6% | 0.1% | 0.1% | 82.4% | 52.5% | 25.8% |

FishBot is dramatically stronger than focus, diversification, bluffing, and random control, but it does **not** beat the posterior-greedy detective. That negative result is valuable: optimizing the current one-ply utility formula is not the same thing as optimizing game win probability.

## FishBot versus detective replication

An additional 10,000 games used a new seed bank and swapped team orientation after 5,000 games.

- FishBot as Team A won 47.58% (95% Wilson interval 46.20–48.97%).
- FishBot as Team B won 48.90% (the detective, as Team A, won 51.10%).
- Combined FishBot win rate: 48.24%; combined detective win rate: 51.76%.
- Median game length was 117 actions and p90 was 133 in both orientations.
- FishBot declaration accuracy was about 94.3%; detective accuracy was about 92.5%.
- Detective ask accuracy was about one percentage point higher, while FishBot questions resolved slightly more binary entropy per ask.

The likely interpretation is that FishBot v0.2 overprices information gain or underprices the detective’s immediate transfer probability. FishBot’s zero internal “regret” only means it perfectly optimizes its own formula; it does not prove that the formula represents true long-run value.

## Current answer to “optimal Fish”

There is no defensible single optimal policy yet. The strongest tested baseline is the detective, narrowly ahead of FishBot. The common characteristics of both are more robust than their difference:

1. maintain explicit card-location beliefs;
2. update those beliefs after every public ask, hit, and miss;
3. prefer high-conversion questions without ignoring information value;
4. declare conservatively because exact teammate allocation is the major failure mode;
5. avoid stylistic deception unless an ablation shows it improves outcomes independently of declaration risk.

## Next model revision

FishBot v0.3 should learn or tune its utility weights against held-out seeds, add exact card-count conditioning, and replace the one-ply reply penalty with counterfactual self-play values. The detective must remain a fixed exploitability probe: a new model is not an improvement unless it reliably beats that baseline after swapping orientation.
