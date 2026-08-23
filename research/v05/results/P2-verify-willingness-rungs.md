# P2 verification — "the willingness rungs never fire because pAlloc is EXACTLY 0.0"

Dylan Nguyen, FishLab Research Project
Repository: `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.
New instrumentation (no protected header touched, no `main.cpp` edit):
`engine/src/probe_vwill.hpp`, `engine/src/probe_vwill_main.cpp`.

Build / run:

```
cd engine
clang++ -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter src/probe_vwill_main.cpp -o /tmp/vwill -pthread
/tmp/vwill --a=v04 --b=v04 --games=2000 --rotations=6 --seed=777 --threads=10
/tmp/vwill --a=v04 --b=v04 --games=600  --rotations=6 --seed=31    --threads=8
/tmp/vwill --a=v04 --b=v04 --games=600  --rotations=6 --seed=4242  --threads=8
/tmp/vwill --a=v04 --b=v04 --games=600  --rotations=6 --seed=555   --threads=8
/tmp/vwill --a=v04 --b=v04 --games=300  --rotations=2 --seed=31    --threads=6
/tmp/vwill --a=v04 --b=v03 --games=600  --rotations=2 --seed=90210 --threads=8
/tmp/vwill --a=v04:gmap=1 --b=v04:gmap=1 --games=600 --rotations=6 --seed=31 --threads=8
```

## Verdict

**Holds up, with the universal quantifier corrected.** The defect is real, the
mechanism is exactly the one claimed, and the cited number reproduces bit-for-bit at
the cited seed. It is *not* a property of the state class: at two of six
configurations pAlloc is sometimes strictly positive, and at one of them the ladder
actually fired.

## Method (deliberately different from `probe_forcedendgame.hpp`)

P2 *replayed* the ladder from a reconstructed state. This probe instead wraps every
agent in a pass-through `Agent` proxy and intercepts the calls
`Game::forcedEndgame` (`engine/src/game.hpp:246`) really makes, in situ. At each
intercepted call it

1. re-asks the inner `V04Agent::willingForced` (`engine/src/v04.hpp:751-757`) with
   threshold **-1** (P2 used 0) so the rung's own `pAlloc` is readable whatever it is,
   and tests it **bitwise** against `0.0` rather than testing `> 0`;
2. rebuilds the candidate exactly as `evaluateSet` does (`v04.hpp:593-606`) and
   rescores it with a **local instrumented copy** of `Belief::jointSequential`
   (`belief.hpp:535-553`) that labels which `return` produced the value;
3. computes the capacity-feasible alternative `Belief::jointSequentialMAP`
   (`belief.hpp:560-587`) at the same state and checks it against every rung of
   `Rules::forcedTh` (`fish.hpp:126-127`).

The independent recompute in (2) agreed with the engine's `pAlloc` to within 1e-12 on
**every** call under the shipped config (`my independent recompute differs 0`), so the
proxy is reading the real quantity.

## 1. The cited number reproduces exactly

`--a=v04 --b=v04 --games=2000 --rotations=6 --seed=777` (P2's own configuration):

```
willingForced calls intercepted    11802   (first rung 1686)
  evaluateSet ok                   11802 (100%)
  pAlloc bitwise == 0.0            11802 (100% of ok)
  pAlloc > 0                       0 (0%)      mean pAlloc 0
PER-STATE (first rung = one row per distinct (half-suit, seat) poll)
  states                           1686
  pAlloc exactly 0.0               1686 (100%)
engine outcome
  a willingness rung was ACCEPTED  0
  bestGuess (rung 7) invoked       562
```

562 `bestGuess` invocations — P2's 562 forced declarations, recovered independently.
1686 = 562 x 3 seats polled per rung sweep. The zero is a genuine bitwise zero, not a
value rounded down in printing, so P2's `pAlloc > 0: 0` does mean what it says.

## 2. The mechanism is confirmed — but the cited line is the wrong one

P2 says `jointSequential` "returns 0.0 the moment the argmax overfills a teammate
(`belief.hpp:540-544`)", naming the mask check. Instrumenting every return site:

| return site | seed 777 (11802 calls) |
|---|---|
| `belief.hpp:542` owner mismatch | **11802 (100%)** |
| `belief.hpp:543` mask violation | 0 |
| `belief.hpp:546` zero conditional marginal | 0 |
| `belief.hpp:548` underflow early-out (non-zero) | 0 |

The zero comes from line **542**, `if (tmp.owner[c] != p) return 0.0`, not 543. The
causal agent is still the one P2 names: in 11802/11802 the card was **UNRESOLVED in
the agent's own `Knowledge` before the loop**, so the only thing that can have
resolved it to a different seat is the in-loop
`tmp.setOwner(...)` / `tmp.propagateCapacity()` at `belief.hpp:549-550` — once the
capacity-free argmax fills one teammate, `propagateCapacity` (`belief.hpp:214-226`)
excludes that seat from the remaining cards and collapses them onto the other
teammate, whom the argmax did not name. And in 11802/11802 the candidate does
overfill: counting the argmax's unresolved-card assignments against
`Knowledge::capacities`, **100%** of candidates hand some teammate more cards than
that teammate can hold.

So: mechanism correct, `belief.hpp:543` → `belief.hpp:542` is the only correction.
(Both lie inside the range 540-544 that P2 cited, so the citation is not wrong, just
imprecise about which of the two guards fires.)

The BRIEF's known-gaps list documents the capacity-free argmax for `bestGuess` only.
The *same* defect inside `evaluateSet` (`v04.hpp:599-606`) is not documented anywhere
in `docs/`, `paper/`, or `research/v04/`, so this is not a deliberate-and-documented
behaviour.

## 3. Where the claim overreaches: "EXACTLY 0.0 at every one of those states"

Re-run at four other seeds and against a different opponent. One row per distinct
(half-suit, seat) poll:

| run | states | pAlloc == 0.0 | pAlloc > 0 | range of the positives | rung fired | bestGuess |
|---|---|---|---|---|---|---|
| mirror seed 777, 2000x6 | 1686 | 1686 (100%) | 0 | — | 0 | 562 |
| mirror seed 555, 600x6 | 450 | 450 (100%) | 0 | — | 0 | 150 |
| mirror seed 31, 300x2 | 84 | 84 (100%) | 0 | — | 0 | 28 |
| v04 vs **v03** seed 90210, 600x2 | 72 | 72 (100%) | 0 | — | 0 | 24 |
| mirror seed **31**, 600x6 | 540 | 528 (97.8%) | **12 (2.2%)** | 0.0179 – 0.485 | 0 | 178 |
| mirror seed **4242**, 600x6 | 446 | 432 (96.9%) | **14 (3.1%)** | 0.216 – **1.000** | **2** | 146 |
| **pooled** | **3278** | **3252 (99.21%)** | **26 (0.79%)** | | **2** | **1088** |

Two corrections follow.

* **"exactly 0.0 at every one of those states" is 99.2%, not 100%.** In 26 of 3278
  states the argmax candidate happens to be capacity-feasible (typically because the
  declarer's `Knowledge` has already resolved enough of the half-suit) and
  `jointSequential` runs to completion; one such state returned **1.0** — the
  declarer knew the whole allocation. So `pAlloc` is sometimes "merely low", and
  occasionally not low at all.
* **"the rungs never fire" is 2 firings in 1088 forced declarations (0.18%), not
  zero.** At seed 4242 the engine accepted a willingness rung twice
  (`a willingness rung was ACCEPTED 2`). The ladder is dead with probability ~99.8%,
  not structurally dead.

Neither correction rescues the ladder: 1086 of 1088 forced declarations still fall
through to `bestGuess`.

## 4. The causal half of the claim, tested directly

Two independent tests that the zero is what kills the ladder, rather than the rung
thresholds simply being too high.

**(a) The feasible alternative at the same states clears the bottom rung ~45% of the
time.** Computing `jointSequentialMAP` at the identical states, seed 777:

```
mean pMAP                  0.38966      pMAP > 0   11802 (100%)
pMAP >= 0.50 (rung 6)      5320 (45.1%)
pMAP >= 0.65 (rung 5)      0
pMAP quantiles  p10 0.25  p50 0.333  p90 0.5  max 0.5
```

(40.4% at seed 31, 48.7% at seed 4242, 49.3% at seed 555, 55.6% vs v03.) So had
`evaluateSet` scored a capacity-feasible allocation, rung 6 would have fired on a
little under half of these polls. The rungs are not merely mis-calibrated; they are
being handed a zero.

**(b) Switching the capacity-free argmax off flips the sign.** With
`--a=v04:gmap=1 --b=v04:gmap=1` (which routes `evaluateSet` through
`jointSequentialMAP`, `v04.hpp:623-627`), seed 31, 600x6:

```
  pAlloc bitwise == 0.0            0 (0% of ok)
  pAlloc > 0                       1172 (100%)   min 0.0536  max 0.5
  mean pAlloc                      0.2988
  a willingness rung was ACCEPTED  2
  bestGuess (rung 7) invoked       52
```

pAlloc is positive at 100% of states and the ladder fires — 2 of 54 forced
declarations vs 0 of 178 in the same seed's shipped run. (It fires *rarely* because
the states reached under `gmap=1` play are harder: only 2.4% have pMAP >= 0.50 there,
against 40% in shipped play. `gmap=1` changes the whole policy, so this is a
directional test of causality, not an estimate of the fix's value.)

## 5. Summary

| element of the claim | verdict |
|---|---|
| `evaluateSet` at press=2 returns ok at these states | confirmed, 100% |
| its `pAlloc` is bitwise 0.0, not merely small | confirmed at seed 777 (1686/1686); **99.21% pooled over 6 configs** |
| the willingness rungs never fire | **0.18% fire (2 / 1088)**, not 0% |
| cause is the capacity-free per-card argmax at `v04.hpp:599-606` | confirmed; the candidate overfills a teammate in 100% of the zero cases |
| the zero is emitted by `jointSequential` after `propagateCapacity` | confirmed; the card was unresolved pre-loop in 100% of the zero cases |
| the emitting line is `belief.hpp:543` (mask check) | **wrong — it is `belief.hpp:542` (owner mismatch); 0 mask violations in 15k+ calls** |
