# P4 — Adversarial correctness review of the v0.4 policy

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, at commit `fe21e19`.
Target under review: `engine/src/v04.hpp` (822 lines), read end to end.

## Method

`engine/src/v04.hpp` is protected, so the review was run against
**`engine/src/probe_policy_v04.hpp`** — a mechanical copy (`V04Config`→`P4Config`,
`V04Agent`→`P4Agent`, wrapped in `namespace fish::p4`) carrying (a) instrumentation counters
and (b) a `cfg.fix` bitmask that switches individual candidate corrections on. With `fix=0`
and `instrument=0` the copy is **bit-identical to the shipped policy**:

```
$ ./fish p4match --a=p4 --b=p4 --games=100 --seed=31 --threads=8
  sets 900-900  events/game 144.1  A decl 894 wrong 9.62%  forced 6 wrong 100.00%
$ ./fish match   --a=v04 --b=v04 --games=100 --seed=31 --threads=8
  mean sets 4.5-4.5  events/game 144.05  declarations 4.47/game at 90.3803%
  forced decls 0.03/game at 0%
```
(894 = 4.47×200, 9.62% = 100−90.38, 6 = 0.03×200.) Equality was re-checked after every
instrumentation change; one intermediate edit that re-associated a floating-point sum in the
`searchTopK` block was reverted for this reason.

Harness: `engine/src/probe_policy.hpp` (factory, match runner, exact-vs-Fast comparator,
horizon cross-tab) plus three new command blocks appended to `engine/src/main.cpp`:
`p4probe`, `p4match`, `p4blockcmp`, `p4horizon`. Nothing on the shipped decision path was
modified.

Fix bits (`engine/src/probe_policy_v04.hpp`, `enum : int`): 1 `FIX_RUNWAY`, 2 `FIX_STALEAGG`,
4 `FIX_PTEAMMAX`, 8 `FIX_DECLCARD`, 16 `FIX_NEARCOMP`, 32 `FIX_NOPRESS`, 64 `FIX_SOFT2`.

---

## Summary table

| # | Finding | Status | Measured cost |
|---|---|---|---|
| D1 | Forcing horizon's second stage cashes unconditionally | **defect, expensive** | **+8.13 pp** win rate to a policy that drops it |
| D2 | `expectedRun` double-counts the candidate's own entry | defect, nearly inert | 2 sets in 10 800 |
| D3 | Declaration value aggregates built from a one-event-stale posterior | defect, cheap to fix | no measurable win-rate cost |
| D4 | `declareByValue` card deltas wrong on the dominant failure mode | defect | 82 % of `declareMargin`, conservative direction |
| D5 | Effective declaration bar is far below the documented `declThreshold` | defect | breakeven `pAlloc` as low as 0.46 |
| D6 | `value()` ignores the hypothetical for `f[12]`/`f[13]` | defect, bounded | ≤ 8.2 % of `declareMargin` |
| D7 | Two-ply threat term is a different, larger formula than `f[8]` | double counting | 8.5× the influence of the expectimax; no win-rate benefit |
| D8 | `vw` is not the E14 fit — and the E14 fit is worse | doc defect | E14 **−2.67 pp** vs v0.3 |
| C3 | `pTeam = max(cheap, pAlloc)` | **hypothesis did not hold** | literal no-op; the bias is in `cheap` |
| C4 | `threatOf`'s `0.7+0.3*activity` | **hypothesis did not hold** | 0.7 in 87.2 % of evaluations |
| C5 | `exposureOf` saturation | **hypothesis did not hold** | saturates in 1.61 % of calls |
| C6 | `bypass` at `unresolvedCount<=8` | **half held** | disables one gate of three |

---

## D1 — The forcing horizon's second stage is an unconditional cash

**Defect.** `pressure()` returns 2 at `pub.nEvents >= 308` (`v04.hpp:583`). At that point
three separate safeguards are switched off simultaneously:

* `v04.hpp:607` `double teamFloor = press >= 2 ? 0.0 : ...` — the `minTeamProb = 0.7925` floor becomes 0;
* `v04.hpp:608` `if (!ignoreGates && cheap < (press >= 2 ? 0.0 : cfg.marginalGate)) return v;` — the marginal gate becomes 0;
* `v04.hpp:675` `if (press >= 2) return true;` — `declareNow` returns true **before looking at `pAlloc` at all**.

`evaluateSet` therefore returns `ok` for every live half-suit that no opponent *provably*
holds a card of (`v04.hpp:595-596`), and `proposeDeclaration` cashes the `argmax pAlloc`
among them (`v04.hpp:734`) — even when that argmax is ~10⁻⁶.

**Concrete state.** 200 mirror games, seed 31. Declarations bucketed by the confidence the
policy itself attached (`Event::confidence` = `pAlloc`):

```
$ ./fish p4horizon --games=200 --seed=31 --deadcut=6
  stated confidence (pAlloc) bucket -> wrong rate:
    [0.0,0.1)  n=130   wrong 100.00%
    [0.1,0.2)  n=10    wrong  90.00%
    [0.2,0.3)  n=5     wrong  80.00%
    [0.3,0.4)  n=5     wrong  80.00%
    [0.5,0.6)  n=35    wrong  54.29%
    [0.6,0.7)  n=26    wrong  53.85%
    [0.7,0.8)  n=103   wrong  10.68%
    [0.8,0.9)  n=64    wrong  12.50%
    [0.9,1.0)  n=1411  wrong   0.00%
```

130 declarations in 200 games are made at a self-assessed probability below 0.1 and every
single one is wrong. Pre-horizon declarations are wrong 1.83 %; at/after the horizon, 66.54 %.

**The brief's hypothesis did NOT hold.** "A long but healthy game is forced to cash as hard
as a genuinely deadlocked one" is false as stated:

```
  games reaching the horizon        46 (23.00%)
    of which deadlocked (run>=6)   46 (100.00% of reached)
    of which NOT deadlocked         0 (0.00%)
```
Raising the deadlock threshold to a dead-ask run of **80** leaves the split unchanged
(55/55 of 250 games, seed 31). Every game that reaches event 220 is genuinely deadlocked.
The horizon fires only on deadlocked games — the cost is not *when* it fires but *what it
does* when it fires.

**Measured cost.** `fix=32` makes `pressure()` return 0 always (`urgent` still escalates at
`nEvents >= forceDeclareEvents`, `v04.hpp:710`, so the conservative `declThreshold` path
still applies):

| A | B | deals | A win rate | A decl wrong | B decl wrong |
|---|---|---|---|---|---|
| `p4:fix=32` | `p4` (stock) | 400 | **58.13 % [56.38, 60.00]** | 1.61 % | 16.44 % |
| `p4:fix=32` | `v03` | 400 | 74.62 % [71.75, 77.50] | 1.76 % | — |
| `p4` | `v03` | 400 | 74.62 % [71.75, 77.50] | 1.78 % | — |
| `p4:fix=32` | `detective` | 300 | 78.50 % [75.00, 81.83] | 1.57 % | — |
| `p4` | `detective` | 300 | 78.17 % [74.67, 81.50] | 1.73 % | — |
| `p4:fix=32` | `lockout` | 300 | 80.33 % [77.17, 83.50] | — | — |
| `p4` | `lockout` | 300 | 80.17 % [77.00, 83.33] | — | — |
| `p4:fix=32` | `hunter` | 300 | 96.67 % [95.17, 98.00] | — | — |
| `p4` | `hunter` | 300 | 96.67 % [95.17, 98.00] | — | — |
| `p4:fix=32` | `bluffer` | 300 | 100.00 % | — | — |
| `p4` | `bluffer` | 300 | 100.00 % | — | — |

Per the standing preference, the per-opponent breakdown is the point: **the escalation costs
8.13 pp against a mirror opponent and exactly nothing against every weaker style tested**,
because no non-mirror game reaches event 220. This is the same strong-opponent selectivity
the P0 baseline reported, and it is why the published head-to-head number never exposed it.

**Termination caveat, measured.** The escalation is not gratuitous — it is the termination
device. With both sides running `fix=32`, 108/600 games (18.0 %) hit `Rules::maxAsks = 400`
and are adjudicated neutrally (`game.hpp:358-366`), events/game 156.4.

**A softer variant, measured.** `fix=64` keeps every `press>=2` relaxation but still requires
`pAlloc >= 0.5`:

| A | B | deals | A win rate | limit-hit games |
|---|---|---|---|---|
| `p4:fix=64` | `p4` | 400 | **55.12 % [53.62, 56.62]** | 0/800 |
| `p4:fix=64` | `p4:fix=64` | 300 | 50.00 % | **74/600 (12.3 %)** |
| `p4:fix=64` | `v03` | 600 | 76.83 % [74.33, 79.25] | 0/1200 |
| `p4:fix=64` | `detective` | 300 | 78.17 % [74.67, 81.50] | 0/600 |

So five of the eight lost points come from the *unconditional* stage alone, and removing that
stage costs nothing outside the mirror — but it does not restore termination. The
`press>=2` branch is doing two jobs (terminate, and cash) and only the second one is wrong.
For v0.5 the termination job needs its own mechanism; the information-bearing ask proposed in
the brief is exactly such a mechanism, because it makes the deadlock *break* rather than
requiring it to be paid for.

**Branch attribution** (100 mirror games, seed 31, counting `(opportunity, half-suit)`
authorisations, not final declarations):

```
  press>=2 (unconditional)           2358
  press>=1 && pAlloc>=0.5             210
  urgent && pAlloc>=declThreshold    1594
  urgent && LOCKED && pAlloc>=0.5      26
  declareByValue                     1126
```
The unconditional branch authorises more candidate declarations than the entire optimal-stopping
rule (`declareByValue`) does.

---

## D2 — `expectedRun` double-counts the candidate's own entry

**Defect.** `v04.hpp:275-284`. `runway[0..3]` holds the four largest per-card best-target
probabilities over all legal asks (`prepareRunway`, `v04.hpp:258-273`). The continuation list
is supposed to exclude the candidate's own entry, because after a hit that card is in my hand
and is no longer askable. The skip at `v04.hpp:278`

```cpp
if (bestPerCard[card] >= 0 && std::fabs(runway[i] - bestPerCard[card]) < 1e-12 && m == 0 && i == 0) continue;
```
fires **only when the candidate's entry is `runway[0]`**. If the candidate is ranked 1 or 2,
nothing is skipped and its own probability re-enters the continuation product it is
multiplying. (Ranks 3 and beyond are excluded for free by the `m < 3` cap, so the divergence
is exactly ranks 1 and 2. Ties are handled identically by both versions.)

**Concrete state.** `runway = [0.90, 0.60, 0.40, 0.30]`, candidate card has
`bestPerCard = 0.60` (rank 1), asked at `p = 0.60`:

* shipped: `q = [0.90, 0.60, 0.40]` → `tail = 1.656`, `f[18] = 0.60·2.656/4 = 0.3984`
* intended: `q = [0.90, 0.40, 0.30]` → `tail = 1.368`, `f[18] = 0.60·2.368/4 = 0.3552`

Error 0.0432, i.e. `linearWeight·w[18] = 0.7667·1.4108 = 1.0817` → **0.0467 of ask score**.

**Measured.** 100 mirror games, seed 31, 1 086 694 calls:

```
  own entry dropped         400626 (36.87%)
  own entry DOUBLE-COUNTED  178842 (16.46%)
  mean |f18 shipped-fixed|  0.00016   max 0.24142
```
16.46 % of calls double count. Mean absolute feature error is 0.00016 over all calls
(0.00092 conditional on double-counting), but the maximum is 0.2414 → **0.261 of ask score,
2.2× the entire one-ply expectimax spread** (see E2).

**Cost, measured.** `fix=1` corrects the skip:

```
$ ./fish p4match --a=p4:fix=1 --b=p4 --games=600 --seed=20260821
  A win 50.00% [50.00, 50.00]   sets 5399-5401   events/game 133.7
```
Two sets differ in 10 800. **The bug is real and almost entirely inert** — the runway feature
contributes only 2.0 % of the ask score's discriminating range (E2), and the error rarely
reaches the top of that range. Reporting it as a defect, not as a cost.

---

## D3 — Declaration value aggregates are built from a stale posterior

**Defect.** `v04.hpp:698` calls `computeAggregates(pub)` — which reads `bel.marg` through
`pTeamCard` (`v04.hpp:358`, `v04.hpp:201-205`) — **seven lines before** `refresh()` at
`v04.hpp:705`. `observe()` sets `dirty = true` on every public event (`v04.hpp:161`), and the
last `refresh()` happened inside the previous `chooseAsk` (`v04.hpp:468`), i.e. *before* the
most recent ask. So `eH[]` and `agg` — the state the optimal-stopping rule compares against —
describe the position one event ago, while `k` and `v.pAlloc` are current.

`chooseAsk` gets the order right (`refresh()` at 468, `computeAggregates` at 472). The bug is
specific to the declaration path.

**Measured** (100 mirror games, seed 31):

```
  declaration opportunities                86430  (press0 76734 press1 9036 press2 660)
  opportunities with a STALE belief        81965 (94.83%)
  mean max|eH stale-fresh|                 0.0636   max 0.9091
  declareNow decisions flipped by staleness   10
```
94.8 % of declaration opportunities use a stale posterior; the worst single half-suit
disagreement is 0.909 in expected team control. It flips a `declareNow` verdict 10 times per
100 games (0.16 % of the 6340 value-rule calls).

**Cost, measured.** `fix=2` moves `refresh()` first:

```
$ ./fish p4match --a=p4:fix=2 --b=p4 --games=600 --seed=20260821
  A win 49.83% [49.25, 50.42]   sets 5405-5395
```
No measurable cost. Still worth fixing in v0.5: it is a one-line reordering with no downside,
and the 0.909 worst case is the kind of thing that becomes load-bearing once the declaration
rule is retuned.

---

## D4 — `declareByValue` misstates the card flow, and the wrong branch is wrong

**Defect.** `v04.hpp:664-667`:

```cpp
int mine = __builtin_popcountll(k.myHand & setMask(S));
double vRight = value(pub, dC, dS, dL, dK, scoreDiff + 1, turnSign, -SETSZ + 0, 0, dUnres, -1);
double vWrong = value(pub, dC, dS, dL, dK, scoreDiff - 1, turnSign, -SETSZ + 0, 0, dUnres, -1);
(void)mine;
```

* **Correct-declaration branch: checked, correct.** A correct declaration means the team held
  all six, so `dOur = -6, dTheir = 0` is exactly right. `scoreDiff + 1` is right
  (`game.hpp:182-184`). `mine` is genuinely not needed here.
* **Wrong-declaration branch: defect.** Two disjoint failure modes are conflated. If the team
  held all six but the allocation was misnamed, `dOur = -6, dTheir = 0` is still right. If an
  opponent held one or more, our team loses fewer than six and the opponents lose the rest —
  `game.hpp:189` strips the half-suit from *every* hand.

**Measured decomposition** (200 mirror games, seed 31, ground truth reconstructed from the
opening deal through the trace):

```
  wrong voluntary declarations: team DID hold all six (allocation misnamed) 32;
                                an opponent held one or more                167
  mean cards of the six actually held by the declaring team, on a WRONG declaration: 4.21
```

**83.9 % (167/199) of wrong declarations are the case the code does not model.** The true deltas average
`dOur = -4.21, dTheir = -1.79`, a net card differential of **-2.42, not -6**. In `f[6]`
(`v04.hpp:386`) that is `3.58/54 = 0.0663`, times `vw[6] = 0.422207` → **0.0280 of value**,
i.e. **82 % of `|declareMargin| = 0.0342`**, applied to the wrong branch with weight
`(1 - pAlloc)`.

**Direction and cost.** The error makes `vWrong` *more* pessimistic than the truth, so the
shipped code is biased toward *not* declaring — the safe direction. `fix=8` substitutes the
expected team ownership of the six cards, and is a **literal no-op**:

```
$ ./fish p4match --a=p4:fix=8 --b=p4 --games=600 --seed=20260821
  A win 50.00% [50.00, 50.00]   sets 5400-5400
```
because at the moment a declaration is even considered, expected team ownership rounds to 6.
The correct fix has to condition on *being wrong* (a `pTeam`/`pAlloc` mixture), not on the
prior mean. That is a v0.5 design item, not a one-line patch.

---

## D5 — The effective declaration bar is far below the documented threshold

**Defect.** `V04Config::declThreshold = 0.81770` is documented at `v04.hpp:93` as
"unlocked half-suits: P(allocation correct)". With the shipped defaults
(`useValue = true`, `valueDeclare = true`, `v04.hpp:102/105`), `declareNow` reaches that
constant only in `urgent` states (`v04.hpp:678`); otherwise `declareByValue` decides
(`v04.hpp:679`), and its implied threshold is a *state-dependent* solution of

`pAlloc·vRight + (1-pAlloc)·vWrong > vWait + declareMargin`.

**Measured breakeven `pAlloc`** (100 mirror games, seed 31, 6340 calls):

```
  breakeven pAlloc: mean 0.7317  min 0.4600  max 0.8599  <0.70 6.78%  <0.60 0.19%
```

Worked analytically for a partly-unresolved half-suit (`eOld = 0.85`, 3 unresolved cards,
turn held): the non-score terms sum to −0.0633, the score swing is `(2p−1)·vw[1]/9 =
(2p−1)·0.09877`, so the bar is `p > 0.647`. For a fully resolved locked half-suit the same
algebra gives `p > 0.8495`. Both are inside the measured [0.46, 0.86] range.

**Calibration consequence** (from the D1 table): the 0.7–0.8 bucket is wrong 10.68 % of the
time and the 0.8–0.9 bucket 12.50 % — **non-monotone**, so the `pAlloc` forecast is not
calibrated below 0.9, which is exactly the region `declareByValue` operates in. 211 of 1789
declarations (11.8 %) are made at stated confidence below 0.7.

**Related, `v04.hpp:678`.** `if (urgent) return v.pAlloc >= cfg.declThreshold || (locked && v.pAlloc >= 0.5);`
cashes a *locked* half-suit on a coin flip. This contradicts the same file's own justification
for patience (`v04.hpp:568-574`, `v04.hpp:648-652`: a locked half-suit has `C_steal = 0`, so
waiting is risk-free). Exercised 26 times per 100 games of authorisations — rare, but it is
the policy arguing against itself.

**Related, `v04.hpp:563`.** `bestAskProbability` returns `0.0` when the seat has no legal ask.
`urgent` therefore latches permanently true (`v04.hpp:711`, `askFloor = 0.3325`) for any seat
holding only complete sets — and, since `Rules::cardlessMayDeclare` defaults true
(`fish.hpp:109`), for every cardless seat, which still participates in `declarationRound`
(`game.hpp:216`). Those seats spend the rest of the game in the urgent regime, including the
locked-coin-flip branch above.

---

## D6 — `value()` ignores the hypothetical for two of its sixteen features

**Defect.** `v04.hpp:392-397` computes `f[12]` ("our near-complete half-suits") and `f[13]`
("their near-complete half-suits") by scanning the **cached, unperturbed** `eH[]`, while every
other feature receives its `d*` perturbation.

* **For ask EV: no effect at all.** `askExpectedValue` (`v04.hpp:454-455`) evaluates both
  branches from the same cached `eH`, so the two terms are identical in `vHit` and `vMiss` and
  cancel exactly in `p·vHit + (1−p)·vMiss` minus any baseline. Checked; confirmed inert.
* **For `declareByValue`: a real, one-sided bias.** `vRight`/`vWrong` pass `dActive = -1`
  (`v04.hpp:665-666`), so `f[8]` knows the half-suit left play but `f[12]`/`f[13]` still count
  it as live, while `vWait` (`v04.hpp:669`) counts it live too. Bound:
  `max(|vw[12]|, |vw[13]|)/9 = 0.025189/9 = 0.0028` = **8.2 % of `|declareMargin|`**.

**Cost, measured.** `fix=16` perturbs `f[12]`/`f[13]` consistently (dropping the declared set,
using `eHit`/`eMiss` for asks):

```
$ ./fish p4match --a=p4:fix=16 --b=p4 --games=600 --seed=20260821
  A win 49.42% [48.50, 50.33]   sets 5392-5408
```
No measurable effect, as the 0.0028 bound predicts.

---

## D7 — The two-ply block double counts, with a *different* formula, and buys nothing

**Defect 1 — different formula.** The one-ply reply threat is
`v04.hpp:220`: `bestCard = std::max(bestCard, friendly * (1 - pt));`
The two-ply threat is
`v04.hpp:540`: `bestCard = std::max(bestCard, fr);`
where `fr` is the same team mass but the `(1 - pt)` legality factor and the
`(0.7 + 0.3*activity)` multiplier (`v04.hpp:224`) are both absent. The two-ply term therefore
overstates the same quantity by up to `1/(1-pt)` on the maximising card, and is added on top
of the one-ply term rather than replacing it (`v04.hpp:545`).

**Defect 2 — double counting.** `cs[r].u` at `v04.hpp:545` already contains
`f[6]` continuation, `f[8]` reply threat and `f[18]` runway, plus the one-ply expectimax.
`chainWeight·p·follow` re-prices continuation; `threatWeight·(1-p)·threat` re-prices reply
threat. Weights: `chainWeight = 3.358`, `threatWeight = 2.6127`, against
`linearWeight·|w[8]| = 0.7667·3.0978 = 2.375`.

**Measured influence** (mean spread of each term across the candidate list, 100 mirror games,
seed 31 — the spread, not the level, is what selects an ask):

```
  f6 continuation 0.2492   f8 reply threat 0.8307   f18 runway 0.2380   (sum 1.3179)
  linear total spread 6.2801   one-ply EV spread 0.1203   two-ply add-on spread 1.0235
```

The un-fitted two-ply add-on (1.0235) is **8.5× the influence of the one-ply expectimax over
the fitted value function (0.1203)** and 78 % of the influence of the three linear features it
duplicates. It changes the chosen ask in **51.7 % of ask decisions**.

**Is `topk > 1` worth its cost? Measured — no.**

| A | B | deals | A win rate |
|---|---|---|---|
| `p4:topk=1` | `p4` (topk=6) | 600 | 49.17 % [46.42, 51.83] |
| `p4:topk=1` | `v03` | 1200 | 76.62 % [75.00, 78.25] |
| `p4` (topk=6) | `v03` | 1200 | 75.88 % [74.12, 77.58] |

Head-to-head is a wash; against v0.3 the *cheaper* setting is nominally ahead. Mirror quality
metrics do favour topk=6 (declaration errors 9.22 % vs 13.01 %, events/game 137.0 vs 146.8),
but that does not convert into wins.

**Wall-clock, measured** (both sides same setting, 300 deals, seed 20260821):
topk=6 → 26.2 games/s; topk=1 → 21.8 games/s. **topk=6 is faster overall**, because its games
are 7 % shorter. Per-decision the two-ply block is of course dearer; it is simply not the
bottleneck. `proposeDeclaration` is: it runs for all six agents at every loop iteration
(`game.hpp:294`, `game.hpp:218`) and each `evaluateSet` in Fast mode calls
`jointSequential` → six `sinkhornDisj` solves (`belief.hpp:535-553`). That is the cost centre
to attack in v0.5, not the ask search.

---

## D8 — `V04Config::vw` is not the E14 fit, and adopting the E14 fit is a regression

**Numeric comparison.** `v04.hpp:111-128` versus `research/v04/results/E14-valuefit.txt`
(rows = 405 348, R² = 0.2909):

| i | name | shipped `vw` | E14 fit | note |
|---|---|---|---|---|
| 0 | bias | 0.001242 | 0.007023 | |
| 1 | score differential | 0.888965 | 0.909779 | agrees |
| 2 | expected control | 0.421266 | 0.115403 | **3.65×** |
| 3 | sharpened control | −0.145791 | −0.249275 | |
| 4 | locked differential | 0.225573 | 0.011511 | **19.6×** |
| 5 | side to move | 0.022896 | 0.026911 | agrees |
| 6 | card differential | 0.422207 | 0.114251 | **3.69×** |
| 7 | unresolved pool | 0.007678 | −0.079727 | **sign flip** |
| 8 | active half-suits | 0.005904 | 0.035886 | |
| 9 | turn × control | −0.000997 | −0.003278 | |
| 10 | my hand size | −0.006601 | −0.011850 | |
| 11 | smallest friendly hand | −0.007472 | 0.062496 | **sign flip** |
| 12 | our near-complete | −0.022484 | 0.005226 | **sign flip** |
| 13 | their near-complete | −0.025189 | −0.034832 | |
| 14 | contested mass | 0.080416 | 0.027557 | 2.92× |
| 15 | turn × unresolved | −0.021409 | −0.019608 | agrees |

Three sign flips and three coefficients off by 3.7–19.6×. The comment at `v04.hpp:107-110`
("Ridge-fitted on self-play decision points; see `fitvalue`") describes the E14 vector, not
the one below it.

**Measured, 1200 deals, seed 555, same deals for both arms:**

| A | B | A win rate | A decl wrong |
|---|---|---|---|
| `p4` (shipped `vw`) | `v03` | **75.88 % [74.12, 77.58]** | 1.46 % |
| `p4:vweights=<E14>` | `v03` | **73.21 % [71.42, 74.96]** | 4.84 % |
| `p4:vweights=<E14>` | `p4` | 49.88 % [47.00, 52.75] (400 deals) | 11.69 % vs 9.14 % |

The shipped vector is **2.67 pp better against v0.3** and, more sharply, declares wrongly
**3.3× less often** (1.46 % vs 4.84 %) — the E14 coefficients (notably `vw[4] = 0.0115` on
locked differential, 19.6× smaller) make the optimal-stopping rule cash far too readily.
Head-to-head the two are indistinguishable.

**Conclusion.** The documentation gap flagged in `docs/FISHBOT_V04.md` is real: the compiled
vector's provenance is unrecorded and it is not the vector E14 reports. But "fix it by using
E14" would cost 2.67 pp. v0.5 should record the provenance of the shipped vector and refit
against the *declaration* loss, not adopt E14.

---

## Checked and correct / hypotheses that did not hold

**C1 — `askExpectedValue` miss renormalisation: correct.** `v04.hpp:443-445` divides the team
mass by `1-p`. This is valid precisely because the ask target is always an opponent —
`enumerateAsks` skips same-team targets (`fish.hpp:189`) — so `pt = pTeamCard(card)` never
contains the mass being removed. (Degenerate corner: at `p → 1` the clamp
`denom = max(1e-6, 1-p)` saturates `ptMiss` to 1, making the miss branch's control delta equal
the hit branch's; it is weighted by `(1-p) ≈ 0`, so it is harmless.)

**C2 — `askExpectedValue` hit branch: not double counting.** `dOur=+1, dTheir=-1` moves
`f[6]` (card differential) while `eHit` moves `f[2]/f[3]/f[4]` (control). These are distinct
features of a jointly-fitted linear value function and a real hit moves both. Minor
inconsistency: `f[10]` (`myCards`, `v04.hpp:390`) and `f[11]` (`minFriendly`) are *not*
updated on the hit branch even though `dOur=+1` says a card arrived in my hand — worth
`|vw[10]|/9 = 0.00073`, negligible but incoherent.

**C3 — `evaluateSet` Fast `pTeam = max(cheap, pAlloc)`: the hypothesis did NOT hold.**
The brief expected a large error. Measured over 231 955 Fast evaluations (100 mirror games,
seed 31):

```
  pAlloc > cheap                          771 (0.33%)  mean excess 0.0014  max 0.0609
  teamFloor passed ONLY because of the max()   0
```
The `max()` never once rescued a half-suit past the `minTeamProb = 0.7925` floor. `fix=4`
(delete the line) is a literal no-op: `sets 5400-5400`, win 50.00 % over 1200 games.

**But the underlying quantity is biased.** Against the exact `BlockDP` (`blockdp.hpp:409`,
`blockdp.hpp:425`), sampled at every public event for seat 0
(`./fish p4blockcmp --games=12 --seed=31`):

| | shipped priors (θ=0.264, φ=0.133) | matched priors (θ=φ=0) |
|---|---|---|
| samples | 8215 | 7804 |
| `cheap − exact P(team owns)`, mean signed | **+0.0338** | **+0.0448** |
| mean abs / max abs | 0.0413 / 0.7341 | 0.0457 / 0.5564 |
| `cheap > exact` | 84.83 % | **95.94 %** |
| Fast `pAlloc` > exact `P(team owns)` | 14.70 % | 4.87 % |
| `|Fast pAlloc − exact best alloc|` mean / max | 0.0210 / 0.6334 | 0.0092 / 0.5000 |
| teamFloor cleared by shipped value but not by exact | 2.07 % | 3.75 % |
| half-suits called LOCKED (`pTeam>.9995`) that are not locked exactly | 0/54 | 0/175 |

So the answer to "which of the two is wrong" is *neither, relative to the other*: `cheap` — a
product of marginals that ignores the negative correlation the capacity constraint induces —
**systematically overestimates** the exact team-ownership probability (96 % of samples with
matched priors), and that biased value is what the 0.7925 floor is compared against.
The floor is cleared by the biased estimate but not by the exact one in 2–4 % of samples.
Note the shipped-prior column is not an apples-to-apples exactness test: `sinkhornDisj`
applies the θ/φ policy prior (`belief.hpp:100-108`) while `BlockDP` is the policy-agnostic
uniform posterior, so part of that gap is a deliberate modelling difference — which is exactly
the assumption the standing preference says to treat as testable. The `lockClaim` row is
reassuring: `pTeam > .9995` was never a false lock claim in 229 samples.

**C4 — `threatOf`'s `0.7 + 0.3*activity`: the hypothesis did NOT hold.** Measured over
7 387 618 set evaluations:

```
  activity = 0 in 87.20% of evaluations,  activity = 1 in 6.56%,  mean 0.0949
```
`k.askCount[t][s]` (`belief.hpp:54`, incremented at `belief.hpp:156`, never decayed) is zero
for the target/half-suit pair 87 % of the time, so the multiplier is the constant 0.7 in
87 % of cases and averages 0.728. It is a 0.7 rescaling of `threatOf` that `w[8] = -3.0978`
absorbs, modulated by ≈4 % on average. Not a behavioural defect — but it is undocumented
(the comment at `v04.hpp:207-209` describes the term without it) and it is the *only* place a
raw whole-game counter enters the ask score.

Two structural approximations in `threatOf`, both second-order and both correct in intent:
`canAsk = 1 - Π(1-pt)` treats the six cards as independent and does not exclude the
self-referential case (asking for `c` requires holding a card of `S` other than `c`); and the
whole quantity is computed on the *current* posterior, not the post-miss one — which is
precisely the gap the two-ply block tries to close, and why D7 double counts.

**C5 — `exposureOf` saturation: the hypothesis did NOT hold.** `v04.hpp:229-236`. The
`min(1.0, e/12.0)` cap almost never binds:

```
  exposure calls 1086694   saturated (=1) 1.61%   mean 0.1727
```
It is a live feature with a decision spread of 0.5344 (4.5 % of the linear score), not a
disguised constant. Two things are still worth flagging: its fitted weight is **positive**
(`w[16] = +1.9040`) on a quantity named "exposure on miss" — the policy *prefers* handing the
turn to an opponent who has shown interest in half-suits where our team holds material — and
it sums `pTeamCard`, which is 1.0 for cards in my own hand, so a feature about what the
opponent has publicly revealed is driven partly by private holdings. Against `f[8]` (whose
weight is negative) the two `(1-p)`-gated terms partially cancel: combined spread 0.9790
versus 1.3651 summed separately, i.e. 28 % cancellation. Not near-duplicates.

**C6 — `proposeDeclaration`'s `bypass`: half of the claim holds.** `v04.hpp:697`
`bool bypass = unresolvedCount <= 8 || press >= 1;`

* It disables **one** gate: the `k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb` pre-filter
  (`v04.hpp:702` and `v04.hpp:717`).
* It does **not** disable `marginalGate` — that lives inside `evaluateSet` at `v04.hpp:608`
  and is only relaxed by `press >= 2`.
* It does **not** touch `teamFloor` — that is `press`-driven at `v04.hpp:607`.

So the teamFloor interaction the brief asks about is real, but it arrives through the
`press >= 1` disjunct, not through `unresolvedCount <= 8`. The `unresolvedCount <= 8` disjunct
only makes the policy evaluate *more* half-suits, all of which still face the marginal gate
and the full 0.7925 floor.

Measured (100 mirror games, seed 31):
```
  (opportunity,set) pairs 473859   cheapTeamProb below the gate 213723 (45.10%)
  opportunities with bypass on 16428, of which bypass re-admitted a set 14137 (86.05%)
```
The pre-gate rejects 45 % of pairs and `bypass` is load-bearing in 86 % of the opportunities
where it is on — but re-admitted sets are then killed by `marginalGate` unless `press >= 2`.
Which is to say: `bypass` is harmless on its own and dangerous only in combination with D1.

**C7 — `teamRevealedSet`: correct.** `v04.hpp:241-247` judges "has my team told the table it
wants this half-suit" from `k.askCount` and `k.publicKnown` only, deliberately excluding cards
sitting in my own hand. `publicKnown` is set only on a successful ask (`belief.hpp:179`), so
locations that a seat deduces privately (using its own hand) do not count as public. The
comment and the code agree, and the choice is the conservative one.

---

## Additional findings not on the brief's list

**E1 — the one-ply expectimax cannot see the target.** `askExpectedValue` opens with
`(void)target;` (`v04.hpp:435`) and uses only `p` and the half-suit. `turnSign` is `-1` on a
miss regardless of *which* opponent receives the turn (`v04.hpp:455`). The value function
therefore has no channel through which "handing the turn to a dangerous opponent" can differ
from "handing it to a harmless one" — that lives entirely in the linear `f[8]`/`f[16]`. The
"one consistent scale" claimed at `v04.hpp:45-49` does not extend to opponent choice, which is
the core of the wiretap trade-off v0.5 needs to price.

**E2 — the value function barely influences ask selection.** Mean spread across the candidate
list, 100 mirror games, seed 31 (`linearWeight` folded in; `valueWeight = 6.0432` folded into
the EV row):

| feature | w | spread | share of linear |
|---|---|---|---|
| f0 hit p | 11.5060 | 3.7477 | 31.8 % |
| f14 location entropy | −2.6534 | 1.5880 | 13.5 % |
| f8 reply threat | −3.0978 | 0.8307 | 7.1 % |
| f5 lock completion | 4.0705 | 0.8156 | 6.9 % |
| f1 hit p² | 3.2948 | 0.7010 | 6.0 % |
| f12 repeats set | 1.2697 | 0.5988 | 5.1 % |
| f10 target hand | −2.0219 | 0.5661 | 4.8 % |
| f16 exposure on miss | 1.9040 | 0.5344 | 4.5 % |
| f9 info leak | −0.8536 | 0.4341 | 3.7 % |
| f2 certain hit | 3.1978 | 0.3990 | 3.4 % |
| f4 team control | 2.1333 | 0.3166 | 2.7 % |
| f6 continuation | 1.4679 | 0.2492 | 2.1 % |
| f3 own progress | 1.6881 | 0.2373 | 2.0 % |
| f18 runway | 1.4108 | 0.2380 | 2.0 % |
| f7 completion bonus | 1.4281 | 0.2181 | 1.9 % |
| f19 leak magnitude | −0.9990 | 0.1419 | 1.2 % |
| f15 team owns set | −0.8045 | 0.0964 | 0.8 % |
| f13 known team cards | 0.9142 | 0.0323 | 0.3 % |
| f11 empties target | 1.1660 | 0.0270 | 0.2 % |
| f17 trailing pressure | 0.0473 | 0.0031 | **0.0 %** |
| **linear total** | | **6.2801** | 100 % |
| **one-ply EV** (`valueWeight ×`) | | **0.1203** | **1.9 %** |
| **two-ply add-on** | | **1.0235** | 16.3 % |

The headline v0.4 mechanism — "one-ply expectimax over a 16-feature learned value function,
so asking and declaring are scored on one consistent scale" (`v04.hpp:45-49`) — contributes
**1.9 %** of the ask score's discriminating range, less than one fifth of what the un-fitted
two-ply add-on contributes and one thirtieth of `f[0]`. Three of the twenty features (`f17`,
`f11`, `f13`) contribute under 0.5 % between them and could be deleted without effect.
The ask rule is, empirically, hit probability plus an entropy penalty plus a reply-threat
penalty. This is direct evidence for the brief's central claim that v0.4's ask rule "values
only material (hits) and never information".

**E3 — `press >= 2` disables confidence-based selection entirely.** Because
`declareNow` returns true for every viable half-suit at `press >= 2` (`v04.hpp:675`), all
three teammates propose, and `Game::declarationRound` takes the **lowest seat** that proposes
(`game.hpp:210-224`, `Rules::declArbitration = 0`). The arbitration rule was chosen
deliberately to avoid leaking private confidence (`fish.hpp:113-119`) — but at `press >= 2` it
becomes the only selector among three simultaneously-willing declarers, so the seat that
declares is chosen by seat index rather than by anything about the position. This is the
interaction task P6 should account for.

---

## Reproduction

```
cd engine && make
./fish p4match  --a=p4 --b=p4 --games=100 --seed=31 --threads=8      # equivalence with v04
./fish p4probe  --a='p4:instr=1' --games=100 --seed=31               # all instrumentation
./fish p4blockcmp --games=12 --seed=31                               # Fast vs exact BlockDP
./fish p4blockcmp --games=12 --seed=31 --a='p4:ptheta=0,pphi=0'      # ... with matched priors
./fish p4horizon --games=200 --seed=31 --deadcut=6                   # horizon cross-tab
./fish p4match --a=p4:fix=32 --b=p4    --games=400 --seed=20260821 --threads=6
./fish p4match --a=p4:fix=64 --b=p4    --games=400 --seed=20260821 --threads=6
./fish p4match --a=p4:topk=1 --b=v03   --games=1200 --seed=555 --threads=6
./fish p4match --a='p4:vweights=0.007023|0.909779|0.115403|-0.249275|0.011511|0.026911|0.114251|-0.079727|0.035886|-0.003278|-0.011850|0.062496|0.005226|-0.034832|0.027557|-0.019608' --b=v03 --games=1200 --seed=555 --threads=6
```

Scratch files added (none of them on the shipped decision path):
`engine/src/probe_policy_v04.hpp`, `engine/src/probe_policy.hpp`, and four appended
`if (cmd == ...)` blocks in `engine/src/main.cpp`.

## What v0.5 should take from this

1. **The forcing horizon is the expensive defect (D1), and it is expensive only against a
   strong opponent.** 8.13 pp in the mirror, 0 pp against every weaker style. Do not simply
   delete it — 18 % of both-sides games then fail to terminate. Termination has to come from
   an ask rule that can break the deadlock (the information-bearing ask), and the cashing
   escalation should keep a hard `pAlloc` floor.
2. **The declaration forecast is miscalibrated exactly where the policy uses it** (D5): the
   value rule fires down to `pAlloc = 0.46`, and the 0.7–0.9 region is 10–13 % wrong and
   non-monotone. Any v0.5 stopping rule needs a calibrated `pAlloc`, and `cheap` is not one —
   it overestimates the exact team-ownership probability in 96 % of samples (C3).
3. **The value function is not doing the job it is credited with** (E2, E1): 1.9 % of the ask
   score's discriminating range, and structurally blind to which opponent receives the turn.
   The wiretap trade-off cannot be expressed in the current feature set at all.
4. **Free wins**: reorder `refresh()` before `computeAggregates` (D3), fix the `expectedRun`
   skip (D2), delete `pTeam = max(cheap, pAlloc)` (C3) and `f17`/`f11`/`f13` (E2). None of
   them changes the win rate today; all of them remove a source of confusion for whatever
   replaces the ask rule.
5. **Do not adopt the E14 value vector** (D8). Record the shipped vector's provenance instead.
