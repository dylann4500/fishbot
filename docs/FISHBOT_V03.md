# FishBot v0.3: technical specification

FishBot v0.3 is the strongest policy in the tested FishLab population. It is a deterministic, inspectable policy for the existing six-player, 54-card, nine-half-suit variant. It is not a claim that Fish is solved.

## Information safety

The policy sees only its own hand and public asks, hits, misses, transfers, declarations, and hand counts. The v0.2 audit found that its reply-risk calculation indirectly consulted a target's hidden hand by generating that target's legal actions. v0.3 replaces this with a public-history threat estimate. The legacy v0.2 policy exposed in the current simulator is sanitized at the same boundary.

## Count-conditioned beliefs

Known transfers are exact. An ask excludes the named card from the asker and provides soft evidence that the asker holds another card in that half-suit. A miss additionally excludes the target.

For unresolved card `c` and player `p`, the prior weight is:

```text
exp(min(2.4, 0.453 × publicAsks[p][halfSuit(c)]))
```

Twelve alternating row/column scaling steps enforce two public constraints:

- each unresolved card has total ownership probability 1;
- each player's unresolved ownership mass equals public hand count minus exactly known active cards.

This is an efficient approximation to conditioning over every legal deal.

## Ask utility

Every legal `(card, target)` pair is evaluated with the selected held-out configuration:

```text
U = 22.0 × P(hit)
  +  2.5 × actor set progress
  +  4.0 × expected team control
  +  0.5 × target ask evidence
  +  4.0 × P(hit) × continuation value
  +  4.0 × P(hit) × completion value
  +  0.5 × repeated-set indicator
  -  1.0 × P(miss) × public reply threat
```

The selected entropy coefficient is zero. Entropy remains reported, but v0.2's direct information premium did not improve the frozen policy reliably.

## Declarations

The base threshold is 0.963 over geometric team-ownership and exact-allocation confidence. It moves by -0.016 while trailing by at least two sets and +0.005 while leading by at least two. Exact allocation receives only 0.008 slack. This score-aware change is deliberately small.

## Supported mechanisms

Paired ablations against the detective and lockout challengers show:

- ask-history inference: +38.75 win-rate points, 95% CI 36.01–41.49;
- public count conditioning: +5.20 points, 95% CI 2.18–8.22;
- the full auxiliary bundle versus immediate-transfer-only: +3.20 points, 95% CI 0.25–6.15.

Continuation, completion, team control, reply risk, and entropy were not individually separated from zero. They should be treated as interacting secondary features, not independently proven mechanisms.
