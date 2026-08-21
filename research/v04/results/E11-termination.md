# Termination study

Nothing in the rules of Fish compels a player to declare. Theorem 1 of the paper
(a half-suit held entirely by one team cannot be asked in by the other) implies
that such a half-suit is frozen — and, for the same reason, that no further
information about its allocation can ever arrive. Two policies that both
correctly decline to cash an uncertain half-suit therefore deadlock.

## Measurements

All figures are `v0.4 (frozen configuration)` self-play, 150 deals × 2
orientations, seed 31, and `v0.4 vs v0.3`, seed 90210.

| Configuration | Mirror win rate | Mirror events/game | Mirror cap hits | vs v0.3 |
|---|---:|---:|---:|---:|
| No forcing rule, cap 400 asks | 50.0% | 188 | 21.0% | — |
| No forcing rule, cap 1000 asks | 50.0% | 314 | 21.0% | — |
| Dead-ask test (`best ask p < 0.02` ⇒ force) | 44.3% | 160 | 16.7% | **41.17%** |
| Graduated event-count escalation (shipped) | 50.0% | 174 | **0.0%** | **69.50%** |

Raising the action cap from 400 to 1000 asks left the non-termination rate
unchanged at 21%, which distinguishes a genuine cycle from slow convergence.

The obvious in-policy fix is wrong. Forcing a claim as soon as no productive ask
remains also fires in ordinary early positions — a player holding only cards
their own team already owns has no productive ask but every reason to wait — and
cost 28.3 win-rate points against FishBot v0.3.

What works is escalating on the public event count: below the forcing horizon the
optimal-stopping rule decides; past it the policy cashes any half-suit it is
better than even money on; past a second horizon it cashes its best candidate
whatever the probability, since an unclaimed half-suit scores nothing. Under this
rule every game in every experiment in the study terminates through play and the
action cap is never reached.

## Incidence across the population

`E13-termination.jsonl` measures the action-cap rate of the shipped
configuration against every member of the population, including a copy of
itself. The mirror match is the adversarial case for a horizon-based forcing
rule and is the only place the rate is materially above zero.

## Implication for the rules

A tournament form of Fish needs an explicit forced-claims provision. The rules
literature records such a proposal; the reason it is necessary — that Theorem 1
freezes information as well as ownership — appears not to have been stated.
