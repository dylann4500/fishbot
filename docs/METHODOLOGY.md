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

## Current v0.3 methodology

FishBot v0.3 adds public hand-count conditioning, a literature-derived lockout challenger, deterministic candidate search, separate train/validation/test seed banks, and paired mechanism ablations. Direct policy claims pool both team orientations. The 250-game ordered matrix is descriptive; primary claims use 2,000-game held-out matchups with Wilson intervals.

The strongest current evidence supports exact ask-history inference and count reconciliation. Smaller one-ply utility terms interact and are not individually established.

## v0.4 methodology (current)

FishBot v0.4 replaces the approximate belief model with an exact one and the
declaration threshold with a stopping rule. The reasoning that makes this
possible is that every card movement in Fish is public, so the hidden state is
exactly the initial deal; inference is then a degree-constrained counting problem
solved by a forward/backward dynamic program over capacity vectors, with the
disjunctive ask-legality certificates handled by enumerating the half-suit they
live in. See `docs/FISHBOT_V04.md` and `paper/fishbot_v04.tex`.

Four rule omissions in the v0.3 simulator are corrected and exposed as switches:
declarations are legal at any moment; a cardless player chooses the teammate who
receives the turn; the forced endgame exchanges only willingness; and games end
through an in-policy cashing rule rather than adjudication after an action cap.

Evaluation moved from two-orientation swaps with Wilson intervals to six-rotation
duplicate blocks with a cluster bootstrap that resamples deals rather than games,
because the six rotations of one deal are a single correlated cluster. Fitting
maximises a soft minimum over the opponent panel rather than the mean, so the
reported quantity is the worst case across playstyles rather than an average
that can hide a collapse.

## Path toward stronger play

The archetypes, v0.3 and v0.4 are transparent numeric policies, which makes them useful experimental instruments. Note one negative result from the v0.4 literature survey: determinized (perfect-information Monte Carlo) search is degenerate for Fish, because a clairvoyant player never fails an ask and never mis-declares, so a double-dummy evaluator collapses toward `argmax P(target holds card)`. Search for this game has to be built over information sets with a belief-limited evaluator, not over determinizations. The remaining route toward stronger play is:

1. Encode public history and each player’s private hand as an information set.
2. Train a value/policy model from millions of fast deterministic self-play games.
3. Use population-based training so multiple policies survive rather than collapsing to one brittle convention.
4. Evaluate exploitability against the whole policy population and the fixed archetypes.
5. Use an LLM offline to propose interpretable policy features or label unusual replays—not inside the inner simulation loop, where it would be slow, costly, and irreproducible.

## Current limitations

The belief updater uses alternating scaling rather than exact Bayesian conditioning over every legal deal. It enforces public card counts but not every higher-order dependency. Agents do not learn conventions with teammates beyond inference from ordinary public actions. The model is therefore a serious baseline and research workbench, not a proof of equilibrium or “solved Fish.”
