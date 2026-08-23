# Adversarial verification — P5 claim: "the D13 free information channel is real but rarely provable"

Verifier task, FishBot v0.5. Target: `research/v05/results/P5-human-strategy.md` §0.2, §3B, §6.1.
All numbers below are from runs made in this session. New probe:
`engine/src/probe_verifyd13.hpp` (no protected header touched; `engine/src/main.cpp` left
unmodified — the probe is driven from a standalone TU, command in §5).

**Verdict: the claim HOLDS UP.** Reproduced exactly at the reported seed, survives two fresh
seeds, and survives replacing the hard-mask provability test with the *exact posterior over
consistent deals*. Three magnitude corrections below; none reverses the conclusion.

---

## 1. Exact reproduction of the reported numbers (seed 31)

`./fish humanchan --a=v04 --b=v04 --games=600 --seed=31` (1200 games, 154,318 ask decisions):

| P5 reported | reproduced |
|---|---|
| D13 provably available at 0.34% of mirror decisions (524/154,318) | **524 (0.339559%)** — identical |
| of those, 31.3% forced (no other legal ask) | **164 (31.2977%)** — identical |
| dead asks in a provably team-owned half-suit 464/56,422 = 0.82% | **464 / 56,422 = 0.822%** — identical |
| v04-vs-v03 provable rate 2.02% (seed 90210) | **750/37,081 = 2.0226%** — identical |
| P0 ground truth: 16.5% of asks land in a team-owned half-suit | `fish pathology --games=300 --seed=31` → **16.4639%** — identical |

`probe_human.hpp:73` `provablyTeamOwned` was read line by line: for each card of an *active*
half-suit it requires either a resolved owner on the actor's team, or `mask[c] ⊆ teamMask`.
That is a valid certificate, and I checked it empirically rather than by inspection: over
154,318 mirror decisions the test fired on **0** half-suits that the team did not in fact own
(`UNSOUND certificates: HARD 0`, and `EXACT 0` for my own stronger test).

## 2. The obvious attack — "hard masks understate provability" — does not rescue the channel

P5's own §6 flags this as an open caveat ("a degree-constrained-flow / Hall oracle … would prove
strictly more"). I closed it. `blockdp.hpp:409 teamOwnsProbability` is the exact posterior over
deals consistent with *all* public information including the disjunctive C5 certificates and the
joint capacity coupling — strictly stronger than any Hall/flow argument on the transportation
polytope, and v0.4 already builds that object every ply (`v04.hpp:170`). Availability of a legal
ask inside a team-owned live half-suit, as a fraction of ask decisions:

```
                          mirror (v04 v04)                  vs v03
                   seed 31/600  s777/200  s20260822/200   s90210/200  s4242/200
GT (ground truth)     22.26%      21.79%      23.63%         31.09%     30.60%
exact >= 0.50          2.83%*      --          --            10.00%     11.30%
exact >= 0.90          0.547%     0.779%      0.432%          5.31%      6.46%
exact == 1 (EXACT)     0.544%     0.771%      0.400%          5.30%      6.44%
hard masks (P5's)      0.340%     0.684%      0.063%          2.02%      2.60%
```
\* P50 row for the mirror measured on the 200-deal subsample (0.652% EXACT there).

Readings:

- In the **mirror — the regime where the deadlock lives** — exact inference raises provability
  from 0.34% to **0.544%** (×1.6). Still under 1% of decisions at every seed tried
  (0.40%–0.77%). The channel remains ~40× rarer than its ground-truth availability.
- Relaxing "provable" to "believed at ≥0.90" changes **nothing** (0.547% vs 0.544%): the
  posterior mass on team ownership is bimodal, so the result is not an artefact of demanding
  certainty. Even at ≥0.50 the mirror rate is 2.8%, still an order of magnitude below GT.
- Per truly-owned live half-suit: only **1.92%** are certifiable by the exact posterior
  (844 of 43,994), 1.19% by hard masks.

## 3. Corrections to the magnitudes

1. **Provable rate is understated.** Mirror 0.34% → **0.544%** exact; v04-vs-v03 2.02% →
   **5.30%** (seed 90210) and **6.44%** (seed 4242) — a ×2.6 understatement in the asymmetric
   arm. The qualitative statement ("almost never provable") holds in the mirror; against a
   weaker opponent the channel is materially bigger than P5 says, ~6% of decisions.
2. **The "~50× gap" compares unlike quantities**: 16.5% is a fraction of *asks landing in* an
   owned half-suit (`diag.hpp:106-118`, ground truth); 0.34% is a fraction of *decisions where
   the channel is provably available*. Like-for-like on availability: GT **22.26%** vs EXACT
   **0.544%** = **41×** (vs hard masks, 66×). Direction and order of magnitude survive; the
   specific "50×" is a coincidence of mismatched denominators. Note also the GT side is itself
   seed-sensitive: `asks in own-locked` is 16.46% at seed 31 but **11.16%** at seed 20260822.
3. **"31.3% forced" is a single-game artefact and should not be quoted as a rate.** The 164
   forced decisions come from **2 games out of 1200**, max 82 in one game (they are the same 164
   as `pathology`'s `starved turns 164`, seed 31). At seed 20260822 the count is **0** (0% of 16
   available); at seed 777 it is 162 of 188 (86%). What is stable is the mechanism — one
   deadlocked game can spend a hundred plies with no non-owned legal ask — not the percentage.

## 4. What this does and does not license

Supported: a v0.5 ask rule that fires only when team ownership is *provable* (or ≥0.9 credible)
will fire at well under 1% of mirror decisions and cannot by itself break the deadlock, and the
gap is not closable by better inference — the exact posterior already leaves it under 1%.

Not supported by these data (and P5 does not claim it): that asks in *partially* owned or merely
*likely*-owned half-suits are useless. At P≥0.50 the mirror channel is 2.8% of decisions, and
36.6% of mirror asks are provably dead anyway (56,422/154,318) — a priced, not free, signalling
channel is where the volume is, which is exactly P5's conclusion.

## 5. Reproduction

```
cd "/Users/dylan/Documents/GitHub/fish optimization/engine"
./fish humanchan --a=v04 --b=v04 --games=600 --seed=31
./fish pathology --a=v04 --b=v04 --games=300 --seed=31
c++ -std=c++20 -O3 -march=native -Isrc <scratch>/vd13main.cpp -o <scratch>/vd13 -pthread
<scratch>/vd13 --a=v04 --b=v04 --games=600 --seed=31
<scratch>/vd13 --a=v04 --b=v04 --games=200 --seed=777
<scratch>/vd13 --a=v04 --b=v04 --games=200 --seed=20260822
<scratch>/vd13 --a=v04 --b=v03 --games=200 --seed=90210
<scratch>/vd13 --a=v04 --b=v03 --games=200 --seed=4242
```
`vd13main.cpp` is a 25-line driver over `fish::vd13::run` in
`engine/src/probe_verifyd13.hpp`; a copy is at
`/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/22257a04-aa58-47f0-b0e0-e93f08cd9260/scratchpad/vd13main.cpp`.

Byline: Dylan Nguyen, FishLab Research Project.
