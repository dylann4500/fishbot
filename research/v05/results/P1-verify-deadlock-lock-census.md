# P1 verification — "no half-suit is locked at the deadlock" (adversarial re-check)

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.

## Claim under test

> In 9 of the 14 long games no half-suit is locked to any team at the dead run's start or
> end, so the E11 "frozen locked half-suit" story cannot be the mechanism for the majority
> of deadlocks.

Source: `research/v05/results/P1-deadlock-forensics.md:72,78`.
E11's text: `research/v04/results/E11-termination.md:3-7`.

## Verdict: **holds up**, and the magnitude was *under*stated.

## Method — independent re-implementation

I did not reuse `fish deadlock`. New file `engine/src/probe_verifylock.hpp` (+ scratch driver,
not linked into `fish`) re-derives everything from `Game`/`Knowledge` directly:

* replay v0.4 mirror, `game.trace.on`, both rotations;
* mark a provably-dead ask from the actor's own `Knowledge` (`probe_verifylock.hpp:76-78`,
  the same predicate as `belief.hpp` exposes: `owner[card] != target`, or `target` absent
  from `mask[card]`);
* find the longest dead run, then take a **ground-truth** lock census
  (`(team0Hand & setMask(s)) == setMask(s)` over *active* sets) at **every event of the run**,
  not only its two ends, plus at the game's final state;
* additionally count how many of the run's asks sit in a half-suit that is locked at that
  moment (`probe_verifylock.hpp:113-117`).

Build/run:
```
clang++ -std=c++20 -O3 -march=native -I src <scratch>/vlock_main.cpp -o vlock -pthread
./vlock --games=60 --seed=31        # and --seed=90210 / 777 / 4242
```

## 1. The cited number reproduces exactly

Seed 31, 60 deals × 2 rotations, 14 games over 300 events — identical population to P1.

```
long games with a lock at run start OR end (P1's statistic): 5 / 14  => no lock in 9 / 14
long games with a lock ANYWHERE in the run:                  5 / 14  => no lock in 9 / 14
```

Trace B (`17383714354061619071`, rot 1) reproduces as `live 7, locked 0` at onset, matching
the dump quoted as evidence.

## 2. The two failure modes I looked for, and did not find

**(a) "Start or end" hides a lock in the middle of the run.** It does not. `lockMin ==
lockMax` over every event of the run in 60 of the 62 long games across four seeds; the two
exceptions (`3184844506985063656/0`, `13810545388204420078/0`, seed 90210) *lose* a lock
mid-run to a declaration, so start-or-end already counts them as locked. Column
`lockMax` gives the same 5/14 as `lockS/lockE`.

Note in passing: P1's own census loop (`probe_deadlock.hpp:178-183`) would have missed
`lockedAtEnd` whenever `runStart+runLen == events`, since the loop is `j < gi.events` and
tests `j == end`. That latent hole never fires in this population
(`run-end index unmeasurable: 0 / n` at all four seeds), so it does not affect the number.

**(b) Seed dependence.** It is not seed-specific.

| seed | long games | with a lock in the run | **no lock** |
|---|---:|---:|---:|
| 31 | 14 | 5 | **9 (64%)** |
| 90210 | 15 | 5 | **10 (67%)** |
| 777 | 21 | 8 | **13 (62%)** |
| 4242 | 12 | 3 | **9 (75%)** |
| **pooled** | **62** | **21** | **41 (66%)** |

## 3. The claim understates the result

"Any half-suit locked anywhere on the board" is the *weakest* possible test of the E11
mechanism — E11 needs the deadlock cycle to *be in* the frozen half-suit. Measuring that
directly:

| | seed 31 | pooled (4 seeds) |
|---|---:|---:|
| dead-run asks sitting in a ground-truth locked half-suit | 749 / 3431 (21.8%) | **1947 / 14997 (13.0%)** |
| long games whose *entire* deadlock cycle is inside locked half-suits | **1 / 14** | **2 / 62 (3.2%)** |

The 5 "locked" games at seed 31 break down as: 1 with the whole cycle inside a lock
(`7199022263161008317/0`, 254/254), 4 with exactly one of the two cycled questions inside
one (≈50% of run asks). At seed 777, 4 of the 8 "locked" games have *zero* run asks in the
locked half-suit (`17636980074912531622/0` 0/292, `531997395752505779/1` 0/250,
`694782257455379793/1` 0/250, `5503798527837255511/0` 0/158) — they count toward "any lock"
and still refute E11's mechanism.

## 4. Positive evidence that Theorem 1 is not what is binding

Trace A onset (`P1-deadlock-forensics-raw.txt:13-29`, `272135269103994248/1`, event 24):
all 9 half-suits live and **genuinely split**, and **51 of 54 legal asks are not provably
dead** — yet v0.4 ranks a `p(hit)=0` ask #1 and then repeats it 142 times. Nothing is
frozen; the policy declines every live ask on its own score.

## What would still rescue E11

Only a reading in which "frozen" means "the *policy* will not move", not "Theorem 1 forbids
movement". That is the P1 conclusion, not E11's: E11 attributes the deadlock to a rules
theorem, and in 66% of long games the theorem has nothing to bite on.
