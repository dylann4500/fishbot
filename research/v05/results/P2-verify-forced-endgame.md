# P2 verification — "the 100%-wrong forced-endgame figure is real, not a counting artefact"

Dylan Nguyen, FishLab Research Project
Repository: `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.
Adversarial re-check of the claim in `research/v05/results/P2-forced-endgame.md` §1.

New instrumentation, sharing **no code** with `diag.hpp` or `probe_forcedendgame.hpp`:
`engine/src/probe_vforced.hpp` + `engine/src/probe_vforced_main.cpp` (standalone binary,
`clang++ -std=c++20 -O2 -Isrc src/probe_vforced_main.cpp -o vforced -pthread`).
No protected header and no existing file was modified; `main.cpp` was not touched.

**Verdict: the claim HOLDS.** The 100% is not an artefact of the counting. One
overstatement found in the supporting detail (sample size, not rate) — corrected in §5.

---

## 1. What was checked, and how independently

`probe_vforced.hpp` reruns games with `Game::trace.on`, then replays the trace against the
opening deal `Game::g.dealt` and re-derives the true hands itself. For every
`Kind::ForcedDeclare` event it recomputes correctness from scratch —
*every named owner must be on the declarer's team and actually hold that card at that
moment* — and **ignores `Event::success` and `GameResult` entirely** except to compare
against them afterwards.

| configuration | forced decls | wrong (my recomputation) | wrong (engine flag) | **disagreements** |
|---|---|---|---|---|
| v04 mirror, seed 31, 300×2 | 28 | 28 | 28 | **0** |
| v04 mirror, seed 1234567, 400×6 | 120 | 120 | 120 | **0** |
| v04 mirror, seeds 7 / 2026 / 999983 / 61803, 300×6 | 80 / 92 / 88 / 90 | all | all | **0** |
| v04 vs v03, seed 424242, 400×6 | 85 | 85 | 85 | **0** |
| v04 vs v02 | 247 | 193 | 193 | **0** |
| v04 vs hunter | 273 | 218 | 218 | **0** |
| v04 vs detective | 2217 | 578 | 578 | **0** |
| v04 vs lockout | 2905 | 601 | 601 | **0** |
| v04 vs random | 154 | 150 | 150 | **0** |
| v04 vs silent | 86 | 86 | 86 | **0** |

Zero disagreements over **6,465 forced declarations** across 10 configurations and 7
opponents. The engine's `Event::success` is exactly what an independent ground-truth
replay computes.

Code path, read directly:
`Game::applyDeclaration` computes `correct` at `engine/src/game.hpp:172-177` against
`g.hand[]`; stamps it into `res.forcedDecls/forcedCorrect` at `game.hpp:192`; and into the
emitted event at `game.hpp:196-197`. `diag.hpp:123` reads `declWrongForced` off
`e.success`. `arena.hpp:102-103` reads `r.forcedDecls / r.forcedCorrect`. All three
counters derive from the single `correct` at `game.hpp:172-177`. There is no second path.

## 2. Failure mode ruled out: `diag.hpp` lumps two call sites together

`applyDeclaration(..., forced=true, ...)` has **two** call sites, and `diag.hpp:123`
counts both:

* `game.hpp:251` — inside `Game::forcedEndgame` (one whole team cardless).
* `game.hpp:335` — the "actor holds only complete sets, so must declare" branch, which is
  *not* an endgame.

This was a live way for the statistic to mean something other than advertised. It does
not bite: my probe classifies each event by whether the opposing team held zero cards
before it, and **the `game.hpp:335` branch fired 0 times in all 10 configurations
(0 / 6,465)**. Every `ForcedDeclare` in the data is a genuine forced endgame.

## 3. Failure mode ruled out: the effect vanishing at another seed or opponent

It does not — for v0.4. Splitting by *which policy the declaring team ran* (the P2 report
did not do this, and the pooled mixed-opponent numbers are misleading without it):

| opponent (seed 424242, 400 deals × 6 rotations) | v0.4's own forced decls | v0.4 wrong | opponent's forced decls | opponent wrong |
|---|---|---|---|---|
| v04 (mirror) | 51 | **51 (100%)** | 51 | 51 (100%) |
| v03 | 85 | **85 (100%)** | 0 | — |
| v02 | 52 | **52 (100%)** | 195 | 141 (72.3%) |
| hunter | 150 | **150 (100%)** | 123 | 68 (55.3%) |
| detective | 65 | **65 (100%)** | 2152 | 513 (23.8%) |
| lockout | 32 | **32 (100%)** | 2873 | 569 (19.8%) |
| silent | 53 | **53 (100%)** | 33 | 33 (100%) |
| random | 150 | 148 (98.7%) | 4 | 2 (50%) |

Two things follow, and both strengthen the original claim rather than weakening it:

* The 100% is **v0.4-specific**, not a property of the forced-endgame decision problem.
  `Baseline::Detective` and `Baseline::Lockout` — which use their own `bestGuess`
  (`engine/src/baselines.hpp:383-390`) — get the same class of decision right ~76-80% of
  the time in the same games.
* The pooled mixed-opponent rates in the table above (26%, 21%) are **not** v0.4 numbers.
  Any future report of "forced-declaration accuracy" in a non-mirror match must split by
  declaring team or it will read as v0.4 improving when it has not.

Pooled over every fresh run I made, deduplicated for mirror orientation (§5):
**899 of 901 v0.4 forced declarations wrong = 99.78%.** The only two correct ones in
~900 came against `random`.

## 4. The wrongness is structural, confirmed against ground truth

Independently of the original report's belief-side diagnostics, I checked the named
allocation against the **true hand sizes** at the moment of declaration — a check that
needs no model at all:

| mirror run | forced decls | named alloc gives some seat more cards of the half-suit than it holds in total |
|---|---|---|
| seed 31 | 28 | **28 (100%)** |
| seed 1234567 | 120 | 118 (98.3%) |
| seed 7 | 80 | **80 (100%)** |
| seed 2026 | 92 | **92 (100%)** |
| seed 999983 | 88 | **88 (100%)** |
| seed 61803 | 90 | **90 (100%)** |

And in every mirror run: truth spreads the half-suit over more than one seat 100% of the
time; the declarer is the lowest-seated live teammate 100% of the time; misnamed-card
count is 1 or 2 and never more.

Four concrete states dumped at seed 1234567 (`named` / `truth` are the six seat digits):

```
deal#3  set 6  declarer 0  conf 0   hand sizes s0=2 s2=2 s4=2
   named 440402   truth 420402      -> seat 4 credited with 3 cards, holds 2
deal#23 set 7  declarer 0  conf 0   hand sizes s0=2 s2=3 s4=1
   named 202220   truth 202240      -> seat 2 credited with 4 cards, holds 3
deal#58 set 5  declarer 0  conf 0   hand sizes s0=1 s2=1 s4=4
   named 204442   truth 204444      -> seat 2 credited with 2 cards, holds 1
deal#65 set 4  declarer 0  conf 0   hand sizes s0=1 s2=2 s4=3
   named 224220   truth 224440      -> seat 2 credited with 3 cards, holds 2
```

Each named allocation is physically impossible, not merely unlucky. This matches the
mechanism the original report attributes to the unconstrained per-card argmax in
`V04Agent::bestGuess` (`engine/src/v04.hpp:762-794`), which `BRIEF.md` already lists as a
known gap ("no capacity constraint, so it can name an allocation giving a teammate more
cards than they hold"). The gap was documented; the 100%-loss consequence was not, and it
is real.

## 5. Correction: the largest run's sample size is double-counted

`P2-forced-endgame.md` §1 states that the seed-777 `--rotations=6` run "gives 562
genuinely distinct declarations". It does not.

In `runForcedProbe` / `runPathology` / my own runner, `rotations=6` sets
`orient = rot / 3` and `shift = rot % 3` — i.e. three deal shifts × two team orientations.
In a **mirror** match both orientations run identical policies, so the two games at the
same `shift` are byte-identical. Evidence:

* In every mirror `rotations=6` run, forced declarations split *exactly* evenly by
  orientation: 51/51, 40/40, 46/46, 44/44, 45/45.
* Re-running seed 1234567 with `--rotations=3` (orientation 0 only, same three shifts)
  gives **60** forced declarations, exactly half of the 120 from `--rotations=6`.

So the headline 562 is **281 distinct forced declarations, each counted twice**. The
report already applied this caveat correctly to the seed-31 `rotations=2` run (28 → 14
effective) and then failed to apply it to the `rotations=6` run.

**This changes the sample size, not the rate: 281/281 = 100%.** Combined with my fresh
seeds, the total independent distinct evidence for v0.4 mirror play is 314/314 wrong.

## 6. Minor: one §5 claim of the original report does not generalise

`P2-forced-endgame.md` §5 states that every v0.4 forced endgame has exactly one live
half-suit. At seed 1234567 I observe 118 forced declarations with 1 live half-suit and
**2 with 2 live half-suits**. The claim is right as a strong tendency (~98%) and wrong as
a universal. It does not affect the 100% figure. (This is outside the claim I was asked
to verify; noted so it is not carried forward as an absolute.)

## Reproduction

```
cd engine
clang++ -std=c++20 -O2 -w -Isrc src/probe_vforced_main.cpp -o /tmp/vforced -pthread
/tmp/vforced --a=v04 --b=v04       --games=300 --rotations=2 --seed=31
/tmp/vforced --a=v04 --b=v04       --games=400 --rotations=6 --seed=1234567
/tmp/vforced --a=v04 --b=v04       --games=400 --rotations=3 --seed=1234567   # halves
/tmp/vforced --a=v04 --b=detective --games=400 --rotations=6 --seed=424242
/tmp/vforced --a=v04 --b=lockout   --games=400 --rotations=6 --seed=424242
```
