# FishLab research methodology

Fish is a partially observable, team-based stochastic game. “Optimal” therefore does not mean a single best move in the Stockfish sense: the value of an ask depends on the other players’ policies and on what they infer from the ask itself. A useful research program should look for robust policies and approximate equilibria, not one immutable move table.

## What the current simulator models

- All 54 cards, nine half-suits, six alternating seats, and two teams.
- Legal specific-card asks, successful transfers, misses, retained/transferred turns, and cardless players.
- Exact declarations, including card-to-teammate allocation, misdeclarations, and forced endgame declarations.
- A private belief state for every player. An agent sees its own hand plus public asks, hits, misses, transfers, declarations, and implied half-suit interest. It never reads an opponent’s hidden hand.
- Five parameterized policies: focused pursuit, diversification, Bayesian location inference, deliberate misdirection, and a low-information random control.
- Seeded randomness. A game seed plus a configuration reproduces the exact deal and action sequence.

## Primary hypotheses

1. **Focus versus diversification.** Compare win rate, ask accuracy, and game length. Focus may improve conversion once a half-suit is identified, while diversification may make a player harder to read.
2. **Diversion value.** Run matched experiments with psychological tells on and off. The causal estimate is the difference in the bluffer’s win rate, not the raw correlation between diversions and wins.
3. **Declaration timing.** Sweep belief thresholds. Plot sets won through correct declarations against sets donated through allocation errors.
4. **Information efficiency.** Compare successful cards acquired per public ask. A policy can ask less often and still be superior if its questions partition the ownership possibilities more effectively.
5. **Endgame quality.** Filter for cardless players, forced declarations, comebacks, lead changes, and long unresolved half-suits; inspect the full replay rather than relying only on aggregate scores.

## Experimental discipline

Use paired seeds when comparing two configurations so each policy sees the same sequence of deals. Change one factor at a time. Report a confidence interval (or a bootstrap interval) alongside win rate before treating a small difference as meaningful. Test both seat orientations because first-player and team-label effects can otherwise masquerade as strategy effects. Keep a held-out bank of seeds for final comparisons.

## Path toward stronger play

The current archetypes are transparent heuristics, which makes them good experimental instruments. The next technical step is self-play policy optimization over a compact information state. Outcome-sampling Monte Carlo CFR is a better fit than a chess search tree because Fish has hidden cards and strategic signaling. A practical sequence is:

1. Encode public history and each player’s private hand as an information set.
2. Train a value/policy model from millions of fast deterministic self-play games.
3. Use population-based training so multiple policies survive rather than collapsing to one brittle convention.
4. Evaluate exploitability against the whole policy population and the fixed archetypes.
5. Use an LLM offline to propose interpretable policy features or label unusual replays—not inside the inner simulation loop, where it would be slow, costly, and irreproducible.

## Current limitations

The belief updater uses a tractable weighted posterior rather than exact Bayesian conditioning over every legal deal. It treats asks as soft evidence that the asker is active in a half-suit but does not enumerate all card-count constraints. Agents do not communicate with teammates beyond the public actions allowed by the rules. The model is therefore a serious baseline and research workbench, not yet a proof of equilibrium or “solved Fish.”
