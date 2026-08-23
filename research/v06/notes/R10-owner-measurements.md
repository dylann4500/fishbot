# R10 — Direct measurements taken for the v0.6 design

All numbers below were produced by purpose-built probes compiled against
`engine/src` at commit `bd812fe` on this machine (Apple silicon, 15 cores,
`clang++ -std=c++20 -O3 -march=native`). Probe sources are in the session
scratchpad and are reproduced verbatim in `research/v06/probes/` when the
mechanism they justify is built.

## 1. Throughput of every candidate rollout policy (self-play, single thread)

`probe/perev.cpp`, 150 self-play games per spec, seed bank `mixSeed(4242, i*7919+3)`.

| spec | games/s | events/game | us/event | sets to team 0 |
|---|---:|---:|---:|---:|
| `v05` | 18.6 | 95.8 | 560.2 | 4.45 |
| `v05:topk=0` | 29.6 | 96.9 | 348.3 | 4.61 |
| `v05:topk=0,value=0` | 26.7 | 103.1 | 362.8 | 4.59 |
| `v05:belief=indep` | 123.4 | 134.2 | 60.4 | 4.46 |
| **`v05:belief=indep,topk=0`** | **873.9** | 126.1 | **9.07** | 4.43 |
| `v05:belief=indep,value=0,topk=0` | 1006.9 | 126.1 | 7.88 | 4.53 |
| `v04` | 7.9 | 154.0 | 820.1 | 4.30 |
| `v03` | 351.3 | 102.6 | 27.8 | 4.70 |
| `v02` | 553.7 | 115.5 | 15.6 | 4.47 |
| `detective` | 1258.2 | 108.7 | 7.3 | 4.37 |
| `lockout` | 839.2 | 125.6 | 9.5 | 4.40 |
| `hunter` | 1452.4 | 81.1 | 8.5 | 4.35 |

**Reading.** `v05` spends 62% of its wall clock inside the `searchTopK` chain/threat
re-scoring (560.2 → 348.3 us/event when it is switched off) and a further 83% of what
remains inside the Sinkhorn belief (348.3 → 9.07 us/event when the marginals go
independent). `v05:belief=indep,topk=0` retains the exact hard deduction, M1 live-ask
gating, the full 20-feature score, the one-ply expectimax and M2 feasible declarations,
and costs **9.07 us/event for all six seats** — 62x cheaper than the deployed policy.
That is the rollout blueprint v0.6's search can afford.

## 2. The hidden state collapses fast enough to make an exact endgame real

`probe/endgame2.cpp`, 200 `v05` self-play games, seed 31. `DealDP::build` sets
`N = 1` as a sentinel when every unresolved card shares one mask, so the count is
recomputed there as the multinomial `Q! / prod_p q_p!`; the raw `dp.N` understates
the count badly and must not be read directly.

Consistent deals seen by the turn-holder, log10:

| event | n | p10 | median | p90 |
|---|---:|---:|---:|---:|
| 16 | 200 | 21.58 | 23.63 | 25.20 |
| 32 | 200 | 15.77 | 18.42 | 20.86 |
| 48 | 200 | 10.09 | 13.17 | 15.94 |
| 64 | 199 | 4.55 | 8.06 | 11.59 |
| 80 | 185 | 0.48 | 3.57 | 6.97 |
| 96 | 99 | 0.00 | 0.78 | 3.76 |

First event at which the turn-holder's consistent-deal count drops below 10^X
(median game is 95 events):

| 10^X | reached | median event | median active half-suits | mean % of half-suits still undecided |
|---|---:|---:|---:|---:|
| 10^9 | 100% | 61 | 4 | **49.1%** |
| 10^7 | 100% | 68 | 4 | **42.2%** |
| 10^5 | 100% | 76 | 3 | **33.8%** |
| 10^3 | 100% | 82 | 2 | 26.4% |
| 10^1 | 100% | 89 | 1 | 15.8% |

**Reading.** By median event 76 the entire hidden state is one of at most 10^5
possibilities and a third of all half-suits are still unclaimed. An exactly-enumerated
endgame is not a curiosity here; it covers a third of the scoring.

## 3. Cost of one determinized rollout, end to end

`probe/rollcost.cpp`, `v05` self-play, decision points sampled every 16 events from
event 24, 52 points.

| stage | cost |
|---|---:|
| `DealDP::build` on the actor's `Knowledge` | 92.8 us (once per decision) |
| one exact posterior deal sample | 0.54 us |
| reconstructing all six seats' information states by replaying `pub.history` | 8.7 us |
| one rollout to game end with `v05:belief=indep,value=0,topk=0` | 422 us (129 events) |

**Reading.** Determinization is free; reconstruction is free; only the rollout costs
anything, and it costs 3.3 us/event. A depth-limited rollout of 20–24 events with a
leaf evaluation is 180–220 us. A 128-rollout budget per decision is therefore ~25 ms,
and a full game in which our three seats search every ask is ~0.8 s — 1.3 games/s per
thread, ~17 games/s on this machine.

## 4. More than half of v0.5's ask decisions are EXACT ties

`probe/margin.cpp`, 150 `v05` self-play games, seed 31, 13,879 ask decisions. At each
decision the probe recomputes the exact score vector the policy itself uses
(`linearWeight * w.f + valueWeight * askExpectedValue`) over the M1-gated candidate set.

| statistic | value |
|---|---:|
| decisions with exactly one live ask (forced) | **1.19%** |
| mean live candidates per decision | **41.7** |
| candidates within 1% / 5% / 10% of the score range of the top | 2.04 / 2.45 / 3.01 |
| **decisions with an exact tie at the top** (`gap < 1e-9`) | **55.16%** |
| decisions with a degenerate score range (`spread < 1e-9`) | 3.16% |
| median score spread across candidates | 5.91 |
| median top1–top2 gap | **0.000000** |
| decisions where the top-2 gap is under 5% of the range | 64.33% |
| the chosen ask is also the maximum-P(hit) ask | 54.06% |
| mean P(hit) of the pick / of the max-P(hit) ask | 0.4620 / 0.5083 |

**Reading.** v0.5 faces a real choice at 98.8% of its ask decisions, and at **55.16%**
of them its own scoring function cannot separate the leaders at all: two or more
candidates are numerically identical and the winner is whichever `enumerateAsks`
emitted first — lowest card index, then lowest seat. The scores are not degenerate
(median spread 5.91 across the candidate set), so this is not a flat objective; it is
a *blind dimension*. `askExpectedValue` opens with `(void)target;` (v05.hpp:427,
inherited from v04.hpp:435), so the expectimax half of the score is constant across
targets, and the linear half separates two opponents only through `p`,
`threatOf`, `handCount` and `exposureOf` — all of which coincide whenever two
opponents are hard-indistinguishable holders of the card. This independently
reproduces, at the level of the argmax rather than the posterior,
`research/v05/results/P5-verify-target-channel.md`'s finding that 46.6% of decisions
have at least two hard-indistinguishable holders worth 0.639 bits.

This is the single largest *cheap* defect available to v0.6: over half of all asks are
currently chosen by array order.
