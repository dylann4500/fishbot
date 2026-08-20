# FishBot v0.2: technical specification

FishBot is the first search policy in FishLab. It is intended to be a transparent “best current computer,” not a claim that Canadian Fish has been solved.

## Information state

Each player has a separate belief state. It contains:

- the player’s current hand;
- an exact owner for any publicly transferred card;
- player/card exclusions established by misses;
- a public ask count for every player and half-suit.

For an unresolved card, each non-excluded owner starts with weight 1. Every ask in the card’s half-suit adds `0.38` to that asker’s weight, capped at an additional `3.0`. Normalizing these weights produces the current approximate owner probabilities. A hit makes ownership exact; a miss sets the target’s probability to zero and renormalizes the remaining possibilities.

That is the technical meaning of “react to an ask” in v0.2. It is a numeric Bayesian-style update from an observed action, not an LLM interpreting dialogue.

## Ask search

FishBot enumerates every legal `(specific card, opposing player)` pair. For each candidate it calculates:

- `P(hit)`: current probability that the target owns the requested card;
- `information gain`: binary entropy of the hit/miss outcome;
- `set progress`: fraction of the half-suit held or publicly known on the team;
- `target evidence`: how often that target has asked in this half-suit;
- `reply threat`: the highest `P(hit)` available to the target if the ask misses and transfers the turn.

The current expected-utility function is:

```text
U(ask) = 13.0 × P(hit)
       +  4.5 × information_gain
       +  3.2 × set_progress
       +  1.6 × target_evidence
       +  2.2 × P(hit)                  [turn retention]
       -  3.8 × P(miss) × reply_threat
```

FishBot selects the legal ask with maximum utility. Ties are deterministic. For any action chosen by another policy, FishLab reports *decision regret*: `best FishBot utility − chosen action’s FishBot utility`.

This is a one-ply expectimax approximation: it prices the immediate stochastic outcome and the opponent’s strongest estimated reply, but it does not recursively search an entire hidden-information game tree.

## Declarations

For every unclaimed half-suit, FishBot estimates:

1. the probability that each card is on its team; and
2. the probability of the most likely exact teammate allocation.

It combines the six card probabilities geometrically. Its declaration threshold varies from 94% while trailing to 97% while comfortably ahead. If all legal asks have zero modeled success probability, the threshold falls because additional questions cannot improve the belief state. A long-game safeguard lowers the threshold again rather than cycling forever.

## Pivotal moments

Each action receives an impact score derived from:

- whether a likely ask missed or an unlikely ask hit;
- whether a transfer materially advanced a half-suit;
- the opponent’s reply threat after a miss;
- disagreement with FishBot’s top action;
- response/diversion context;
- lead changes and declaration failures.

Actions at or above the configured threshold appear as replay landmarks. The score is a navigation heuristic—not a causal estimate of how much the action changed win probability. A later value model can replace it with estimated win-probability swing.

## What is needed for a stronger “Stockfish for Fish”

Chess engines can search a fully observed state. Fish requires policies over information sets because cards are hidden and an ask itself changes what other players believe. The natural next architecture is:

1. exact belief conditioning over legal deals and card counts;
2. outcome-sampling Monte Carlo CFR self-play;
3. a learned value network for long-horizon counterfactual value;
4. a population of conventions/archetypes to avoid overfitting one self-play dialect;
5. exploitability and held-out-opponent evaluation rather than win rate against one fixed bot.

An LLM can help propose human-interpretable features or label replay patterns offline. It should not sit in the inner simulation loop: that would be slower, more expensive, and less reproducible than a numeric policy.
