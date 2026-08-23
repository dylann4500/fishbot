# P6 verification — public willingness ladder for declaration arbitration

**Claim under test.** "A public willingness ladder recovers 89% of the confidence-ranking
win-rate gap, but only if the rungs are spread over [0,1]; the existing `Rules::forcedTh` rung
values recover just 10%."

**Verdict: holds up in direction and mechanism; the "10%" is wrong.** At matched sample size the
`forcedTh`-shaped ladder recovers **≈35% of the win-rate gap and 19–38% of the contested-race
gap**, not 10%. The 10% was produced by dividing a 1,500-deal number by a 3,000-deal number.

## Method

Binary built from the same driver the original used, plus a paired-outcome counter:
`engine/src/probe_vladder_main.cpp` (new; wraps `runArb` from
`engine/src/probe_declaration.hpp:38`, no protected header touched).

```
clang++ -std=c++20 -O3 -march=native src/probe_vladder_main.cpp -o probe_vladder -pthread
./probe_vladder --games=3000 --seed=<S> --x=3 --rungs=0    # confidence ranking (unsafe)
./probe_vladder --games=3000 --seed=<S> --x=4 --rungs=17   # 17 evenly spaced rungs
./probe_vladder --games=3000 --seed=<S> --x=4 --rungs=0    # forcedTh-shaped 8 rungs (default ladder)
```

Harness parity confirmed first: seed 31, x=3 reproduces `runs/P6/A_conf.json` bit for bit
(0.503000, contested 4961, low 1218, conf 1415, ladder 1293, R17 1408), and seed 31, x=4 rungs=17
reproduces `runs/P6/B_ladder17.json` bit for bit (0.502667, contested 5058, low 1224, conf 1423,
ladder 1416, curve R2 1287 / R5 1390 / R9 1414 / R33 1423).

Rung definitions read directly: `probe_declaration_game.hpp:84` `setRungs` (`ladder[i] = 1 -
i/(R-1)`), `:175` `pickLadderR`, `:76` the default ladder `{0.995,0.98,0.95,0.90,0.80,0.65,0.50,0}`,
which is `Rules::forcedTh` (`fish.hpp:127`) with its trailing `-1` sentinel replaced by 0.

## 1. Win-rate recovery, all three arms at 3,000 deals each

| seed | conf (mode 3) Δ | 17-rung ladder Δ (recovery) | `forcedTh` ladder Δ (recovery) |
|---|---|---|---|
| 31 (theirs) | +0.300 pp | +0.267 pp (**89%**) | **+0.117 pp (39%)** |
| 777001 | +0.383 pp | +0.367 pp (96%) | +0.133 pp (35%) |
| 20260822 | +0.433 pp | +0.367 pp (85%) | +0.150 pp (35%) |

The published "10%" is `arb4.json` (+0.033 pp, **1,500 deals**) over `A_conf.json` (+0.300 pp,
**3,000 deals**). Run at 3,000 deals on the same seed 31 the `forcedTh` ladder gives +0.117 pp.
The report's own numbers show the instability that caused this: confidence ranking measures
+0.100 pp at 1,500 deals (`arb3.json`) and +0.300 pp at 3,000 deals (`A_conf.json`) — the same
treatment, the same seed, a factor of three apart.

**Resolution warning.** These win rates come from a paired mirror duplicate, and almost every deal
splits 1–1. Non-split deals per 3,000, seed 31: confidence 30 (24 X / 6 Y, net 18); 17-rung ladder
26 (net 16); `forcedTh` ladder 11 (net 7). The headline "89%" is the ratio 16/18. No recovery
percentage computed from this metric is worth more than one significant figure.

## 2. Contested-race recovery — the well-powered metric

Direct counting on races where the confidence argmax differs from the lowest seat
(`probe_declaration_game.hpp:243-268`); n ≈ 5,000 per run. Recovery = (rule − lowest) /
(confidence − lowest).

| run | contested | low | conf | R=2 | R=5 | R=9 | R=17 | R=33 | `forcedTh` |
|---|---|---|---|---|---|---|---|---|---|
| s31 conf | 4961 | 1218 | 1415 | 31% | 84% | 96% | 96% | 100% | **38%** |
| s31 lad17 | 5058 | 1224 | 1423 | 32% | 83% | 95% | 96% | 100% | — |
| s777001 conf | 4817 | 1128 | 1342 | 19% | 82% | 96% | 101% | 100% | **21%** |
| s777001 ladFT | 5023 | 1152 | 1382 | 18% | 83% | 97% | 101% | 100% | **19%** |
| s20260822 conf | 5130 | 1155 | 1359 | 25% | 91% | 94% | 99% | 98% | **32%** |
| s20260822 ladFT | 5337 | 1162 | 1391 | 24% | 91% | 94% | 98% | 97% | **35%** |

The evenly spaced curve replicates tightly across seeds: R=5 → 82–91%, R=9 → 94–97%,
R=17 → 96–101%, R=33 → 97–100%. The `forcedTh` shape lands at 19–38%.

## 3. The mechanism the claim asserts is confirmed

At a fixed budget of ~3 bits/seat, an evenly spaced 9-rung ladder recovers 94–97% while the
8-rung `forcedTh` shape recovers 19–38%. So rung *placement*, not rung *count*, is what matters —
the claim's core point.

Sharper: the `forcedTh` ladder's recovery (38 / 21 / 19 / 32 / 35%) sits on top of the R=2 ladder's
recovery (31 / 32 / 19 / 18 / 25 / 24%), where R=2 is the single bit `conf ≥ 1.0`. The six
`forcedTh` rungs between 0.50 and 0.995 add roughly nothing beyond that one bit. That is the
predicted signature of contested confidences clustering below 0.5, and it is why a `[0,1]`-spread
ladder is required.

## 4. Corrected statement

> A public willingness ladder with rungs evenly spread over [0,1] recovers essentially all of the
> confidence-ranking advantage: 94–97% of the contested-race gap at 9 rungs (~3 bits/seat) and
> 96–101% at 17 rungs, replicated at three seeds; on the (noisy) win-rate metric, 85–96%. The
> `Rules::forcedTh` rung values recover only about a third — 35–39% of the win-rate gap at matched
> 3,000-deal samples, 19–38% of the contested-race gap. Six of `forcedTh`'s rungs lie between 0.50
> and 0.995 and are worth almost nothing beyond its single top rung.

The practical stake is unchanged and small: the whole confidence-ranking advantage is +0.30 to
+0.43 pp, so the difference between the two rung shapes is worth roughly 0.2 pp of win rate.

## Raw output

`/private/tmp/.../scratchpad/vlad_s31.txt`, `vlad_s777.txt`, `vlad_sB.txt`; regenerate with the
three commands above at seeds 31, 777001, 20260822.
