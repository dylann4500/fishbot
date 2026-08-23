# P2 — Why 100% of forced-endgame declarations are wrong

Dylan Nguyen, FishLab Research Project
Repository: `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.
Engine built with `cd engine && make`. New instrumentation:
`engine/src/probe_forcedendgame.hpp`, plus two commands appended to `engine/src/main.cpp`
(`fish forcedprobe`, `fish blockalias`). No protected header was modified.

---

## Headline

The 100% figure is **real, not an accounting artefact, and not a hard decision problem**.
It is a single mechanical defect with a deterministic consequence:

> `V04Agent::bestGuess` (`engine/src/v04.hpp:762-794`) picks each card's owner by an
> independent per-card argmax over `bel.marg[c][q]` with **no capacity constraint**. In
> every observed forced endgame the truth splits the unresolved cards across two
> teammates and the argmax piles them all onto one. The named allocation therefore
> violates the declarer's own capacity constraint, has exact posterior probability
> **zero**, and loses the half-suit with certainty.

Measured over 12,000 mirror games (562 forced declarations, seed 777):

| | |
|---|---|
| forced declarations | 562, **562 wrong (100%)** |
| named allocation violates the declarer's own capacity | **562 (100%)** |
| named allocation has exact posterior probability 0 | **562 (100%)** |
| named allocation puts a card at a seat the declarer's own `Knowledge` excludes | 0 (0%) |
| declarer's own stated confidence at the moment of declaring | **exactly 0.0 in 562/562** |
| cards misnamed per declaration | 1 in 518, 2 in 44 — **never more** |
| best *feasible* allocation would have been correct | **228 (40.6%)** |

The bot names an allocation it itself scores at probability zero, and it misses by the
minimum possible margin every time: one or two cards, always the ones that had to be
split.

---

## 1. Is the 100% real, or an artefact of the counting?

**Real.** Three independent checks.

1. `res.forcedDecls` / `res.forcedCorrect` are set in `Game::applyDeclaration`
   (`engine/src/game.hpp:192`) from `correct`, which is computed at `game.hpp:172-178`
   directly against the ground-truth hands `g.hand[]` — a named owner must be on the
   declaring team *and* actually hold the card. `diag.hpp:123` counts
   `declWrongForced` from the emitted event's `success` flag, which is that same
   `correct` (`game.hpp:196-197`). There is no separate path that could double-count or
   mislabel.
2. `fish forcedprobe` independently replays the exact ladder of `Game::forcedEndgame`
   against the live agents at every event that leaves a team cardless. It predicted the
   same `(half-suit, declarer)` the engine actually used in **562/562** cases
   (`probe replay matched 562 / 562`), and independently recomputed the true holder of
   every card from `Game::g.hand`.
3. Every wrong declaration is wrong for a reason the probe can name in the declarer's
   own information set (§3), not merely "the truth happened to differ".

Replicated across three configurations:

| run | forced decls | wrong |
|---|---|---|
| `--a=v04 --b=v04 --games=300 --rotations=2 --seed=31` | 28 | 28 (100%) |
| `--a=v04 --b=v03 --games=300 --rotations=2 --seed=90210` | 16 | 16 (100%) |
| `--a=v04 --b=v04 --games=2000 --rotations=6 --seed=777` | 562 | 562 (100%) |

(The seed-31 mirror run's 28 rows are 14 deals × 2 team orientations; because both teams
are v0.4 the two orientations of a deal produce byte-identical games, so the effective
sample there is 14. The seed-777 run uses `--rotations=6`, which also shifts the deal
across seats, and gives 562 genuinely distinct declarations.)

In `v04 vs v03` all 16 forced declarations were made by v0.4 seats; v0.3 never reached a
forced endgame as the declaring team, so the number is a pure v0.4 measurement.

---

## 2. Which rung of the ladder fires?

`Rules::forcedTh = {0.995, 0.98, 0.95, 0.90, 0.80, 0.65, 0.50, -1.0}`
(`engine/src/fish.hpp:126-127`); `Game::forcedEndgame` sweeps thresholds outermost
(`game.hpp:238`) and calls `bestGuess` only on the final `th < 0` rung (`game.hpp:245`).

**All 562 declarations came from rung 7, the `bestGuess` rung. The seven willingness
rungs fired zero times.** Not once, in any of the three runs.

The reason is not that confidence was merely low. The probe re-asks
`V04Agent::willingForced` (`v04.hpp:751-757`) with threshold 0 and reads out the `pAlloc`
the rungs would have compared against:

```
what the WILLINGNESS rungs saw (evaluateSet at press=2)
  evaluateSet ok        562 / 562
  its pAlloc > 0        0 (0%)   mean pAlloc 0
  its alloc feasible    0 (0%)
  its alloc == truth    0 (0%)
```

`willingForced` calls `evaluateSet(pub, set, press=2)`. Under the default
`BeliefMode::Fast` that takes the branch at `v04.hpp:621-634`, which builds the candidate
allocation with the *same* capacity-free per-card argmax (`v04.hpp:599-606`) and then
scores it with `Belief::jointSequential` (`belief.hpp:535-552`). `jointSequential`
conditions card by card and calls `propagateCapacity()` after each fix, so as soon as the
argmax overfills a teammate the next card's conditional marginal is 0 and the function
returns 0.0 (`belief.hpp:540-544`).

So the willingness ladder is **structurally dead**: the one allocation it is willing to
evaluate is exactly the infeasible one, it correctly scores it at 0, refuses every rung,
and hands the decision to `bestGuess` — which then declares that same zero-probability
allocation anyway, because `bestGuess` has no refusal path (`game.hpp:245`, and the base
`Agent::bestGuess` at `game.hpp:34-37` likewise always answers).

`conf` recorded on all 562 events is `0.0`, so the engine's own trace already contains the
admission.

---

## 3. Decomposition of the failure

Per-declaration diagnostics, seed 777, n = 562:

| property | count |
|---|---|
| names a card at a seat its own `Knowledge` excludes (`mask` violation) | 0 (0%) |
| **assigns more unresolved cards to a teammate than their remaining capacity** | **562 (100%)** |
| breaks one of its own C5 ask-legality certificates | 218 (38.8%) |
| exact posterior probability of the named allocation | **0 in 562/562** |
| capacity overshoot | 1 card in 518, 2 cards in 44 |
| **the guess spreads the unknowns over fewer seats than the truth does** | **562 (100%)** |
| the truth splits the unknowns over more than one teammate | **562 (100%)** |
| mean (top teammate marginal − second teammate marginal) on unresolved cards | 0.154 |

The mechanism, exactly:

* At the forced endgame the opposing team holds no cards, so every card of every live
  half-suit is on the declaring team. The declarer's problem is purely *which of my two
  teammates holds each unknown card*, subject to their public hand counts.
* In all 562 cases exactly two teammates had spare capacity, and the truth used both.
* `bestGuess` loops the six cards independently (`v04.hpp:766-781`) and takes
  `argmax_q bel.marg[c][q]` with strict `>`, so ties break to the lowest seat. The
  marginals across two teammates for the unknown cards of one half-suit are near-equal by
  symmetry (mean gap 0.154; in the seed-31 sample 22 of 28 declarations had a gap of
  **exactly 0**), so **every** unknown card is assigned to the same teammate.
* That teammate cannot hold them all. The allocation has zero posterior mass, so the
  declaration is a guaranteed loss.

This is why the misnamed-card count equals the capacity overshoot in every single row
(518 rows: overshoot 1, misnamed 1; 44 rows: overshoot 2, misnamed 2). The guess is the
truth with exactly the overflow cards moved onto the wrong seat.

Raw per-declaration tables (one row per forced declaration, 23 columns):
`research/v05/runs/P2-forced-mirror-seed777.csv` (562 rows),
`research/v05/runs/P2-forced-mirror-seed31.csv` (28),
`research/v05/runs/P2-forced-vs-v03-seed90210.csv` (16).
Sample rows (seed 31), `named` / `truth` are the six seat digits of the half-suit:

```
game set declarer nunknown  named   truth   pbest
31    8    1         3      155551  153551  0.333
112   0    1         4      333311  355311  0.167
171   5    0         4      202220  402240  0.167
223   7    0         2      202000  402000  0.500
```

---

## 4. The ceiling: how hard is the decision actually?

Computed with the exact `BlockDP` posterior (`engine/src/blockdp.hpp`) built from the
declarer's own `Knowledge` at the exact state where the engine declared:

| | seed 777 (n=562) | seed 31 (n=28) | vs v03 (n=16) |
|---|---|---|---|
| mean P(best **feasible** allocation) | 0.397 | 0.393 | 0.427 |
| mean P(the **true** allocation) | 0.397 | 0.393 | 0.427 |
| best feasible allocation *is* the truth | 40.6% | 50.0% | 37.5% |
| **v0.4 achieved** | **0%** | **0%** | **0%** |

`P(true) == P(best feasible)` in **every single row**, all three runs. The exact posterior
is flat over the surviving feasible allocations at these states: with 2 unresolved cards
the answer is a 50/50, with 3 it is a 1-in-3, and so on
(`pbest` takes only the values 0.5 / 0.333 / 0.25 / 0.167 / 0.1).

So the decision problem *is* close to a coin flip — and that is the point. The ceiling for
any policy is ~40%; v0.4 scores 0%. The entire gap is the capacity bug, not inference
quality. The bot is not unlucky; it is systematically choosing the one allocation the
posterior has already ruled out.

Counterfactual policies replayed from the same endgame-entry states, using only
information available to the declaring team (`FEProbe::replay`):

| policy (seed 777, 562 half-suits) | correct |
|---|---|
| v0.4 engine as shipped | **0 / 562 (0%)** |
| feasible argmax (`BlockDP::bestTeamAllocation`), engine's set order and declarer | 228 / 562 (**40.6%**) |
| + let the best-positioned teammate declare instead of the lowest seat | 262 / 562 (**46.6%**) |
| + confidence-greedy half-suit order | 262 / 562 (46.6%) |
| + order fixed up front at endgame entry | 262 / 562 (46.6%) |

Recovering the capacity constraint alone is worth **+40.6 points**; letting the
best-positioned teammate declare is worth a further **+6.0**. In expected sets that is
0.406 sets per forced endgame, and forced endgames occur in 4.7% of mirror games.

---

## 5. Does the ORDER of cashing half-suits matter, and does v0.4 exploit it?

**The hypothesis did not hold — but for a reason nobody had checked: the choice never
arises.**

`Rules` documents the intent at `fish.hpp:123-125` (comment above `forcedTh`): "it lets the earlier, safer
declarations reveal allocations that sharpen the later ones." Measured over every
occasion a team went cardless with live half-suits (566 entries, seed 31):

```
live half-suits at the moment a team went cardless (all entries)
  1 sets: 566 entries, of which 28 actually reached a forced declaration
```

**Every** forced endgame in v0.4 play has exactly one live half-suit, and exactly one
forced declaration (`nactive == 1` and `ordinal == 0` in 562/562 rows of the seed-777
table). There is nothing to order. The three ordering counterfactuals above are therefore
identical to the non-ordering ones by construction, and the ladder's documented rationale
about earlier declarations sharpening later ones is, in practice, inoperative.

Two consequences for v0.5:

* The multi-set forced endgame is not structurally impossible — a cardless team simply
  requires the other team to hold 6k cards — it just never happens because v0.4 cashes
  locked half-suits promptly. Any v0.5 change that makes the bot *more* patient about
  declaring (which the P0/P0b deadlock findings point toward) will start producing
  multi-set forced endgames, and then the order **will** matter. The machinery should be
  built before the patience change lands, not after.
* Note that information arrives even from a *wrong* forced declaration: `Event::handCount`
  is filled after the set is removed (`game.hpp:148-149`, `emit`), so the post-hoc hand
  counts publicly reveal how many cards of that half-suit each player held. The rules
  permit querying any player's remaining card count, so this is legal information. The
  counterfactual replay already feeds it back through `Knowledge::onEvent`
  (`belief.hpp:206`).

---

## 6. A second, independent defect found on the way: `BlockDP` instances alias

`bestGuess` already contains a correct capacity-aware path — `BlockDP::bestTeamAllocation`
at `v04.hpp:782-790` — but it is gated on `cfg.belief == BeliefMode::Block`, which is not
the default (`V04Config::belief = BeliefMode::Fast`, `v04.hpp:59`). Switching it on does
**not** fix the endgame:

```
fish pathology --a=v04:belief=block --b=v04:belief=block --games=150 --rotations=2 --seed=31
  forced endgame   6   wrong 6 (100%)
```

and the probe shows the failure mode *changes*: under `belief=block` the violations are
`mask` violations (2/2), not capacity violations — the declarer names a card at a seat its
own `Knowledge` has already excluded, which is impossible from its own information set.

Cause: `BlockDP::build` parks all of its tables in a `thread_local` buffer pool
(`blockdp.hpp:83-97`, `blockdp.hpp:175-176` — `units = bf.units.data(); groups =
bf.groups.data(); F = bf.F.data(); …`). Every `V04Agent` owns its own `block` member and
only rebuilds when `dirty` (`v04.hpp:163-165`), but `Game::forcedEndgame` and
`Game::declarationRound` poll several agents back to back **between** public events. The
second agent's `build()` silently repoints the first agent's tables at its own data.

Direct measurement (`fish blockalias --a=v04 --games=40 --seed=31`), reading one
`BlockDP`'s own group table twice with an unrelated `build()` in between:

```
same BlockDP object, same query, before/after a second build() elsewhere
  checks 294   mismatches 285   worst |delta| 0
```

**285 of 294** — agent 0's posterior tables were replaced by agent 2's. An earlier version
of this check that issued a real query after the clobber died with `SIGBUS`, because the
stale `F`/`B` pointers index a table sized for a different capacity vector.

This does not affect the default `Fast` configuration (which never builds a `BlockDP`), and
it does not affect the numbers in §4 (the probe builds a `BlockDP` and queries it before
anything else can rebuild). But **any v0.5 that moves to exact block beliefs will hit it
immediately**, and it is currently masked by the fact that `belief=block` is off by
default. Fix before adopting: give `BlockDP` owned storage, or make `refresh()`
unconditional, or stamp the buffer with an owner id and rebuild on mismatch.

---

## 7. What v0.5 should do

1. **Replace the per-card argmax in `bestGuess` with a capacity-feasible allocation.**
   The code already exists twice over: `Belief::jointSequentialMAP` (`belief.hpp:560-587`,
   whose own comment says "Choosing each card's owner by its *unconditioned* marginal is
   not the same as maximising the joint, because the six cards of a half-suit are coupled
   through the capacity constraint") and `BlockDP::bestTeamAllocation`. Neither is on the
   default forced-endgame path. Expected value: 0% → ~41%.
2. **Use `jointSequentialMAP` inside `evaluateSet` too** (`V04Config::greedyMAP`, currently
   `false`, `v04.hpp:65`), so the willingness rungs stop reporting `pAlloc = 0` on states
   where a feasible allocation carries 0.4 probability. Turning `gmap=1` on alone does not
   fix the endgame (10/10 still wrong, because `bestGuess` is a separate code path), but it
   is required for the ladder to mean anything.
3. **Choose the declarer, not the lowest seat.** `game.hpp:242` scans seats in order and
   the `th < 0` rung always accepts, so the lowest-seated teammate always declares — its
   hand size was 1 or 2 cards in 408 of 562 cases. Adding one more willingness rung below
   0.5 (say 0.4 / 0.3 / 0.2 / 0.1, still one bit each, so still rules-legal) lets the
   best-positioned teammate take it: +6.0 points on top of the capacity fix.
4. **Fix the `BlockDP` aliasing before switching the default belief to `block`** (§6).
5. Build the multi-set forced-endgame ordering machinery only alongside the patience
   changes that will actually create multi-set endgames (§5).

---

## Reproduction

```
cd engine && make
./fish forcedprobe --a=v04 --b=v04 --games=300  --rotations=2 --seed=31
./fish forcedprobe --a=v04 --b=v03 --games=300  --rotations=2 --seed=90210
./fish forcedprobe --a=v04 --b=v04 --games=2000 --rotations=6 --seed=777 --csv=fe.csv
./fish blockalias  --a=v04 --games=40 --seed=31
./fish pathology   --a=v04:belief=block --b=v04:belief=block --games=150 --rotations=2 --seed=31
./fish pathology   --a=v04:gmap=1 --b=v04:gmap=1 --games=300 --rotations=2 --seed=31
```

`fish verify` still passes after the instrumentation
(`audit violations: 0 / 6737436`, `determinism: PASS`), and
`fish pathology --a=v04 --b=v04 --games=300 --rotations=2 --seed=31` reproduces the
baseline numbers unchanged (`declarations 5400 wrong 564`, `forced endgame 28 wrong 28`).
