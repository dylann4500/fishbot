# P8-verify — adversarial check of the turn-transfer "individually consequential" claim

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, engine at commit
`fe21e19` plus the scratch files listed under *Reproduction*.

## Claim under test

`research/v05/results/P8-coordination.md` §1.2:

> A transfer is individually consequential: the receiver averages **3.36 asks at an 85.1% hit
> rate**, pulling **2.86 cards**, against the 34.2% global hit rate of the v0.4 mirror. No
> transfer was followed by zero asks.

## Verdict

**The numbers hold up exactly. The comparison does not.**

The measurement reproduces bit-for-bit under an independent reimplementation and at a second
seed. But the 34.2% comparator is the whole-game per-ask hit rate, and every transfer happens
in the endgame with a cardless player on the table. Against a **structurally matched control**
— ordinary miss-in possessions that begin while some seat is already cardless, reweighted to
the transfer possessions' cards-in-play profile — the hit rate is **78.1%**, not 34.2%.

**85% of the quoted 50-point gap is the phase, not the transfer. The transfer-specific
increment is +7.5 points of hit rate and +0.5 asks per possession.**

The report's own §1.6 already states the mechanism ("the posterior has already collapsed"), so
this is a mis-stated comparator inside a section whose conclusion is otherwise unaffected.

## 1. The cited number reproduces exactly

`./fish coord --games=3000 --seed=31 --no-measure` (6000 games), verbatim:

```
post-transfer runs   2134   asks/run 3.36364   hits/run 2.86129   hit rate 85.0655%
  transfers with 0 asks by the receiver 0 (0%)
```

Different seed, same tool — `./fish coord --games=600 --seed=777 --no-measure` (1200 games):

```
post-transfer runs   400   asks/run 3.355   hits/run 2.84   hit rate 84.6498%
  transfers with 0 asks by the receiver 0 (0%)
```

Independent reimplementation (`engine/src/probe_turnrun.hpp`, which reconstructs possessions
from the `Trace` event log and shares no counter with
`engine/src/probe_coordination_game.hpp:553-605`), same 1200 games at seed 777:

```
PASS   400 runs   3.35500 asks/run   2.84000 hits/run   84.6498%   0-ask 0.000%
```

**Bit-identical.** The statistic is real and correctly computed.

Two secondary attributions also check out:

- *"the receiver"* — the P8 counter is team-scoped (`postPassTeam = teamOf(rcv)`,
  `probe_coordination_game.hpp:556`), which would be wrong if the turn could move inside a team
  without a `Pass` event. It cannot; measured directly, **asks inside a PASS possession not made
  by the pass receiver: 0** (seed 31 and seed 777).
- *"34.2% global"* is `./fish pathology --games=300 --seed=31` → `hit rate 34.2462%`. At the
  1200-game sample used below the same quantity is 35.47% (seed 31) / 34.98% (seed 777).

## 2. The comparator is wrong: possessions by how the turn was acquired

A *possession* = a maximal run of asks by one team from the moment it acquires the turn. A miss
always crosses teams (asks must target a live opponent), so a possession is `k` hits and at most
one miss. `probe_turnrun.hpp` classifies each by acquisition: `LEAD` (opening), `MISS-IN` (an
opponent missed into us), `PASS` (a cardless teammate handed it over), and `MISSCLES` = the
subset of `MISS-IN` that opens while **some seat is already cardless** — the structural
precondition for a transfer to exist at all.

`./probe_turn --games=600 --seed=31 --ctrl=misscl` (1200 games, 154,318 asks):

| acq | runs | asks/run | hits/run | hit rate | ends in a miss |
|---|---|---|---|---|---|
| LEAD | 1200 | 1.260 | 0.260 | 20.63% | 100.0% |
| MISS-IN (all) | 99374 | 1.523 | 0.535 | 35.13% | 98.8% |
| **MISSCLES (matched)** | 6032 | 2.872 | 2.044 | **71.16%** | 82.8% |
| **PASS** | 422 | 3.517 | 3.009 | **85.58%** | 50.7% |

Same at seed 777 (1200 games, 156,884 asks):

| acq | runs | asks/run | hits/run | hit rate |
|---|---|---|---|---|
| MISS-IN (all) | 101758 | 1.513 | 0.525 | 34.68% |
| **MISSCLES** | 6914 | 2.539 | 1.685 | **66.36%** |
| **PASS** | 400 | 3.355 | 2.840 | **84.65%** |

Merely conditioning on "somebody is cardless" — with no transfer involved — moves an ordinary
possession from 1.5 asks at 35% to 2.5–2.9 asks at 66–71%.

## 3. Phase-matched control

Cards in play is always a multiple of 6 (sets leave the table whole), so the phase bucket is
exact. Seed 31, PASS vs MISSCLES at identical cards-in-play:

| cards in play | PASS runs | asks/run | hit rate | MISSCLES runs | asks/run | hit rate |
|---|---|---|---|---|---|---|
| 6 | 270 | 3.089 | 88.73% | 1340 | 3.219 | **83.45%** |
| 12 | 110 | 3.836 | 81.04% | 2074 | 2.626 | 67.83% |
| 18 | 26 | 4.769 | 80.65% | 1300 | 2.549 | 63.01% |
| 24 | 14 | 6.000 | 83.33% | 854 | 2.490 | 60.40% |
| 30 | 2 | 10.000 | 90.00% | 266 | 3.406 | 70.86% |

64% of all transfers happen with one half-suit left on the table, and in that bucket the
control possession is **longer** than the transfer possession (3.219 vs 3.089 asks) and only
5 points behind on hit rate.

Reweighting the control to the PASS phase profile:

| | asks/run | hits/run | hit rate |
|---|---|---|---|
| PASS actual (seed 31) | 3.517 | 3.009 | 85.58% |
| control, phase-matched | 3.000 | 2.343 | **78.11%** |
| PASS actual (seed 777) | 3.355 | 2.840 | 84.65% |
| control, phase-matched | 2.923 | 2.285 | **78.19%** |

Gap decomposition, seed 31 / seed 777:

- quoted gap (PASS − global) = **50.11 / 49.67** points
- explained by phase + cardless conditioning (control − global) = **42.64 / 43.21** points = **85.1% / 87.0%**
- transfer-specific residual = **7.47 / 6.46** points, and **+0.52 / +0.43** asks per possession

The control is stable at 78.1% across both seeds while the raw comparator moves the conclusion
by 50 points. The residual is also not established as causal: a transfer fires exactly when a
teammate has just been emptied, which is itself correlated with the team having just cashed sets
and holding a concentrated remainder.

## 4. One definitional asymmetry worth recording

`endmiss%`: **50.7% of PASS possessions never reach their terminal miss** (they are cut short
when a declaration empties the receiver and a second `Pass` fires, or by game end), against
98.8% for ordinary MISS-IN possessions. This does not bias the hit rate — every ask observed is
correctly scored — but it does mean "3.36 asks/run" and "1.52 asks/run" are lengths measured
under different stopping rules, which is a second reason not to read the two side by side. It
is also why `hits/run` (2.861) is not `asks/run − 1` (2.364).

## 5. What is unaffected

- **Channel size.** PASS possessions hold **0.96% (seed 31) / 0.86% (seed 777)** of all asks,
  against P8's quoted 0.92%. Confirmed.
- **P8's headline verdict** (§1.4: an omniscient chooser is worth `-0.0007 ± 0.0024` sets/game;
  do not build the ladder) does not rest on this sentence and is untouched by this check.
- **"No transfer was followed by zero asks"** reproduces (0 of 422, 0 of 400) and is not
  vacuous: `probe_coordination_game.hpp:583-591` shows a receiver holding only complete sets is
  routed to a forced declaration with no ask, which would have registered.

## Reproduction

New scratch files; no protected header touched, and `main.cpp` was **not** edited (the driver is
standalone, following the `probe_declaration_main.cpp` precedent):

- `engine/src/probe_turnrun.hpp` — possession reconstruction from `Trace` events.
- `engine/src/probe_turnrun_main.cpp` — driver.

```
cd engine
clang++ -std=c++20 -O3 -march=native src/probe_turnrun_main.cpp -o probe_turn -pthread

./probe_turn --games=600 --seed=31  --ctrl=misscl
./probe_turn --games=600 --seed=777 --ctrl=misscl
./probe_turn --games=600 --seed=31              # --ctrl=miss, unmatched control

# the originals, re-run
./fish coord --games=3000 --seed=31  --no-measure
./fish coord --games=600  --seed=777 --no-measure
./fish pathology --games=300 --seed=31
```

Flags: `--games`, `--seed`, `--a`, `--b`, `--threads`, `--ctrl=miss|misscl`.
