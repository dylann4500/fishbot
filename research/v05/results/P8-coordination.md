# P8 — The two unexploited legal coordination channels

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, engine at commit
`fe21e19` plus the scratch files listed under *Reproduction*.

**Verdicts up front, because both hypotheses in the task failed.**

| channel | hypothesis | result |
|---|---|---|
| turn transfer | the candidates dominate the cardless player as judges, so a willingness ladder is a rules-legal free win | **No.** A real (2+ candidate) transfer decision arises 0.148 times per game and v0.4's unilateral pick already matches an omniscient oracle at 800 of 886 of them. Handing one team a *ground-truth* chooser is worth **`-0.0007 +/- 0.0024` sets/game** over 6000 paired games. A legal ladder recovers 30–56% of a gap that measures zero. |
| forced-endgame declarer | the existing ladder rarely fires above `bestGuess`; a finer or differently-shaped ladder should help | **No, and provably not.** The ladder never fires at *any* shape — four shapes, including a 0.10 bottom rung and no rungs at all, give bit-identical output — because the statistic it thresholds is **exactly 0 in 210 of 210** surveyed (player, live half-suit) pairs. 100% of those allocations over-fill a teammate under the declarer's own `Knowledge`, which drives `jointSequential` to a hard zero. The allocator is the bug, not the ladder. |
| information safety | a ladder of thresholds may leak more than one bit | **Yes, and the leak equals `H(transcript)` exactly.** Measured: **2.19 bits per candidate** for the shipped 8-rung shape, against the ~1 bit the rules text licenses. Worse, the leaky channel (turn transfer, mid-game, live opponents) is the worthless one; the valuable-if-repaired channel (forced endgame) leaks to an audience that is cardless by construction. |

---

## 0. What was built

`game.hpp`, `belief.hpp`, `blockdp.hpp`, `v04.hpp`, `fish.hpp` are untouched. The driver was
copied to `engine/src/probe_coordination_game.hpp` (namespace `fish::probecoord`), which is
byte-identical to `engine/src/game.hpp` except for the pass-target policy hook, the
forced-endgame rung recorder, and the confidence survey. `engine/src/probe_coordination.hpp`
is the runner; `fish coord` is a new `if (cmd == "coord")` block appended to `main.cpp`.

Ground-truth quality metric used throughout §1:

> **`sureRun(u)`** = the number of cards player `u` could pull with **certainty**, one after
> another: cards sitting with a live opponent in a half-suit where `u` already holds a card
> (`probe_coordination_game.hpp`, `Game::sureRun`). Pulling a card never opens a new
> half-suit — an ask is legal only in a half-suit you already hold — so this count is exact.

---

## 1. Turn transfer

### 1.1 The channel as the engine implements it

`engine/src/game.hpp:298-309`: when the turn-holder is cardless the driver collects the live
teammates into `cand[]` and calls `agents[g.turn]->choosePassTarget(...)` — **the cardless
player decides alone**. `engine/src/v04.hpp:796-819` scores each candidate `u` by

```
v(u) = max over live half-suits s of   (1 - prod_c (1 - P(u holds c)))  x  (max over opponents o of P(o holds c))
```

i.e. "probability `u` holds *something* in `s`" times "best opponent-hold probability in `s`".
For `u == seat` the first factor is exact; for a teammate it is a posterior guess. The teammate
knows it exactly. That is the whole information advantage the channel could buy.

### 1.2 How often it happens, and how much it matters

`./fish coord --games=3000 --seed=31` (v0.4 mirror, 6000 games, 836k events):

```
pass events          2134   per game 0.355667
games with >=1 pass  2044 (34.0667%)
pass decisions       2134   with 2+ candidates 886 (41.5183%)
games with a real (2+ candidate) choice 872 (14.5333%)
post-transfer runs   2134   asks/run 3.36364   hits/run 2.86129   hit rate 85.0655%
  transfers with 0 asks by the receiver 0 (0%)
```

- A transfer occurs **0.356 times per game**; 34% of games contain one.
- **Only 41.5% of transfers are a choice at all** — the rest have a single live teammate.
  A genuine two-or-three-way decision arises **0.148 times per game** (14.5% of games).
- A transfer is individually consequential: the receiver averages **3.36 asks at an 85.1% hit
  rate**, pulling **2.86 cards**, against the 34.2% global hit rate of the v0.4 mirror
  (`P0-v04-pathology.md`). No transfer was followed by zero asks.
- But the channel governs **under 1% of all asks**. All 9 half-suits are resolved by
  declaration in every game (0% limit hits), so asks/game = 139.418 - 9 - 0.356 = 130.06, i.e.
  ~780,000 asks; the 2134 transfers account for 2134 x 3.364 = **7,178 of them (0.92%)**.

### 1.3 The oracle gap

At the 886 multi-candidate decisions, five selection rules were scored against ground truth.
`ORACLE` = argmax `sureRun`, ties broken toward v0.4's own pick so the comparison isolates
information, not tie-breaking luck.

```
   at least one candidate had sureRun>0: 882 (99.5485%)
   lowest seat     live 99.3228%   mean sureRun 4.62077   agrees with oracle 59.5937%
   most cards      live 99.0971%   mean sureRun 4.99774   agrees with oracle 82.3928%
   v0.4 unilateral live 99.5485%   mean sureRun 5.07901   agrees with oracle 90.2935%
   willing ladder  live 99.5485%   mean sureRun 5.27540   agrees with oracle 81.9413%
   ORACLE          live 99.5485%   mean sureRun 5.42664
```

Two things kill the hypothesis:

1. **The choice is almost never decisive.** Candidates differ in `sureRun` at only
   **300/886 = 33.9%** of decisions, and only **6 decisions in 6000 games (0.68% of multi
   decisions, 0.11% of games)** are *decisive* in the strong sense that one candidate has no
   certain hit at all while another does. v0.4 matches the oracle on the `live` column
   **exactly** (99.5485% both): it never hands the turn to a teammate with nothing to ask
   when a teammate with something to ask was available.
2. **The residual gap is tiny.** Shortfall distribution (oracle `sureRun` minus chosen
   `sureRun`), over 886 decisions:

   | shortfall | 0 | 1 | 2 | 4 | 5 | 6 | 7 | 10 | 12 |
   |---|---|---|---|---|---|---|---|---|---|
   | v0.4 | 800 | 34 | 6 | 10 | 24 | 4 | 2 | 4 | 2 |

   Total 308 cards over 6000 games = **0.051 certain cards per game**, i.e. 0.0086 half-suits
   per game at a 6-cards-per-set exchange rate, against a 4.5 sets/game mean. Even an
   omniscient teammate-chooser is worth **under 0.2% of a set per game**.

### 1.4 The oracle A/B, run rather than argued

`--pass=oracle` gives one team ground-truth-optimal turn-transfer selection; the other keeps
v0.4's unilateral rule. Same 3000 deals x 2 orientations, same seed, per-game A scores dumped
and paired:

```
$ ./fish coord --games=3000 --seed=31 --no-measure --dump=uni.csv
$ ./fish coord --games=3000 --seed=31 --pass=oracle --no-measure --dump=ora.csv
n=6000  mean diff (oracle - unilateral) = -0.00067 sets/game  se=0.00118
        95% CI [-0.0030, +0.0016]
games where the two runs diverged at all: 29 (0.48%)
A sets total: unilateral 27000    oracle 26996
```

Because only 29 of 6000 games diverge, the paired interval is very tight. **An omniscient
teammate-chooser is worth `-0.0007 +/- 0.0024` sets per game** — the upper end of the interval
is +0.0016 sets/game, i.e. one extra half-suit every 600 games. A legal mechanism cannot beat
the oracle, so this bounds the whole channel.

### 1.5 What a legal willingness ladder recovers

Implementation (`probe_coordination_game.hpp`, `Game::ladderPick`): a public descending
sequence of thresholds `{0.98, 0.90, 0.80, 0.65, 0.50, 0.35, 0.20, 0.05}`. At each rung every
candidate publishes one bit — whether `V04Agent::bestAskProbability` (`v04.hpp:558-564`,
the highest posterior hit probability among *its own legal asks*) clears the rung. The first
rung with a taker decides; ties inside a rung go to the cardless player's own preference,
which is information-free.

- Mean `sureRun` 5.079 -> **5.275**, i.e. **56.5% of the oracle gap recovered**.
- On the decisive `live` metric it recovers **0%** — because there was nothing to recover.
- It *lowers* raw agreement with the oracle (90.3% -> 81.9%) while raising mean quality: the
  ladder trades many small-loss picks for fewer large-loss ones.
- On the 300 decisions where the candidates actually differ, v0.4 picks an argmax 71.3% of the
  time (that count comes from the `--no-measure` pass, in which the ladder is not evaluated; the
  ladder's own argmax rate is measured on the smaller consistent sample in the table below,
  where it rises 69.4% -> 83.3%).

**Converted to value: 0.029 certain cards per game, ~0.005 half-suits per game** — and §1.4
already showed that even the *full* 0.051 cards/game buys `-0.0007 +/- 0.0024` sets/game. The
ladder recovers a little over half of nothing.

Ladder depth trades recovery against leakage monotonically. One consistent sample — 600 deals
/ 1200 games, seed 31, the same 190 multi-candidate decisions in every row, `--rungs=...`:

| ladder | rungs | mean sureRun | oracle gap recovered | argmax picked among the 72 differing decisions |
|---|---|---|---|---|
| v0.4 unilateral | — | 4.768 | 0% | 69.4% |
| 1 rung | 0.50 | 4.884 | 29.7% | 77.8% |
| 2 rungs | 0.90, 0.50 | 4.895 | 32.4% | 80.6% |
| 8 rungs | 0.98 … 0.05 | 4.916 | 37.8% | 83.3% |
| ORACLE | — | 5.158 | 100% | 100% |

(The 56.5% figure quoted above for the 8-rung ladder comes from the 5x larger 6000-game sample;
the two are consistent within sampling noise on 190 vs 886 decisions.)

The one-rung version — the only version the rules text plainly licenses (§3) — recovers under a
third of a gap that is already worth nothing.

### 1.6 Why the intuition failed

The intuition — "the candidates know their own hands, the cardless player does not" — is
correct but arrives too late. A player only goes cardless near the end of a game, when the
posterior has already collapsed: at these decision points even *lowest seat* achieves 4.62 of
the oracle's 5.43, and the oracle's own mean is 5.43 certain pulls, meaning nearly every
candidate is sitting on a long guaranteed run. The private information the channel would
transmit is information the public record has already mostly revealed.

---

## 2. Forced-endgame declarer selection

### 2.1 The rung distribution

`Rules::forcedTh` (`fish.hpp:126-127`) is `{0.995, 0.98, 0.95, 0.90, 0.80, 0.65, 0.50, -1.0}`;
`Game::forcedEndgame` (`game.hpp:236-250`) sweeps rungs outermost, then half-suits in index
order, then declaring-team players in **seat order** (`game.hpp:242`). Over 6000 mirror games:

```
forced sweeps        284   forced declarations 284   wrong 284 (100%)
residue (nobody declared) 0
rung   threshold    fired     wrong      mean conf
  0     0.9950  0 (0%)   0 (0%)   0
  1     0.9800  0 (0%)   0 (0%)   0
  2     0.9500  0 (0%)   0 (0%)   0
  3     0.9000  0 (0%)   0 (0%)   0
  4     0.8000  0 (0%)   0 (0%)   0
  5     0.6500  0 (0%)   0 (0%)   0
  6     0.5000  0 (0%)   0 (0%)   0
  7   bestGuess  284 (100%)   284 (100%)   0
```

**Every rung above `bestGuess` is dead.** P2's suspicion is confirmed and then some: it is not
"almost never", it is never.

### 2.2 Reshaping the ladder is a provable no-op

Four shapes, 3000 games each (1500 deals x 2 orientations), seed 31, `--forcedth=...`:

| ladder | rungs | fired above `bestGuess` | forced declarations | wrong |
|---|---|---|---|---|
| shipped | 0.995 … 0.50, -1 | 0 | 142 | 142 (100%) |
| fine-low | 0.50, 0.40, 0.30, 0.25, 0.20, 0.15, 0.10, -1 | 0 | 142 | 142 (100%) |
| coarse-low | 0.30, 0.15, 0.05, -1 | 0 | 142 | 142 (100%) |
| none | -1 only | (n/a) | 142 | 142 (100%) |

Identical in every cell, including the forced-sweep count — the ladder does not alter a single
decision. Lowering the bottom rung from 0.50 to 0.05 changes nothing.

### 2.3 Why: the thresholded statistic is exactly zero

At the moment each forced endgame opens, every declaring-team player was asked, for every
still-live half-suit, for its own confidence in its own best allocation — `willingForced` with
`threshold = 0`, so it reports instead of gating (`Game::surveyForcedConfidence`):

```
$ ./fish coord --games=600 --seed=77 --leak            # 1200 games, 70 forced sweeps
confidence every declaring-team player attaches to its OWN best allocation,
surveyed over all (player, live half-suit) pairs at the moment the forced endgame opens:
   n=210  mean 0  max 0
     exactly 0  210 (100%)
   allocation named vs the declarer's OWN Knowledge:
       contradicts a known owner 0%
       names a card at a seat its own mask excludes 0%
       exceeds a teammate's capacity 100%
       self-consistent 0%
   picking the ARGMAX-confidence player for each half-suit would be right 0% of the time (n=70)
```

The same survey at seed 31 / 800 games gives `n=114, mean 0, max 0, capacity-violating 100%`:
it is not a seed artefact.

The chain is mechanical:

1. When a whole team is cardless, **every remaining card is on the declaring team**, so
   `P(team owns the half-suit) = 1` and the only open question is the 3-way split among
   teammates.
2. `V04Agent::evaluateSet` builds its allocation by an **independent per-card argmax over
   teammates** (`v04.hpp:599-605`), with no capacity constraint — exactly the defect the BRIEF
   lists for `bestGuess`, present in `evaluateSet` too.
3. `Belief::jointSequential` (`belief.hpp:535-552`) evaluates that allocation by conditioning card by
   card and calling `propagateCapacity()`. Once the argmax has over-filled a teammate, the next
   card's mask excludes that teammate and the routine returns a hard `0.0`.
4. `willingForced` (`v04.hpp:751-757`) therefore compares `0.0` against every rung and refuses.
5. Control drops to the `th < 0` rung, which ignores confidence entirely and hands the
   declaration to the **lowest-seated teammate** (`game.hpp:242`) — and the resulting
   declaration is wrong 100% of the time.

So the willingness ladder is not badly shaped. It is *inert*: it is thresholding a number that
is structurally zero. **No reshaping of `Rules::forcedTh` can recover anything.** The fix is
upstream — produce a capacity-feasible allocation, which the engine already knows how to do
(`BlockDP::bestTeamAllocation`, used only on the `belief=block` path, `v04.hpp:609-620` and
`v04.hpp:782-788`). See §2.4. Detailed anatomy of the resulting misdeclarations is P2's
(`probe_forcedendgame.hpp`); this section only establishes that the ladder cannot be the lever.

### 2.4 The prediction, tested: fix the allocator and the statistic becomes non-zero

If the diagnosis is right, switching to the belief mode whose allocator *is* capacity-feasible
(`BlockDP::bestTeamAllocation`) should turn the identically-zero statistic into a positive one.
`./fish_p8 coord --a=v04:belief=block --b=v04:belief=block --games=250 --seed=31 --no-measure`
(500 games):

```
   n=24  mean 0.25  max 0.333333
     <0.25  12 (50%)
     <0.50  12 (50%)
   allocation named vs the declarer's OWN Knowledge:
       contradicts a known owner 50%
       names a card at a seat its own mask excludes 50%
       exceeds a teammate's capacity 66.6667%
       self-consistent 33.3333%
rung   threshold    fired     wrong
  ... all rungs 0.995 .. 0.50 :  0
  7   bestGuess  8 (100%)   8 (100%)   mean conf 0.25
```

Confirmed, with a sting: the statistic is now non-zero (mean 0.25) but its **maximum over 24
surveyed pairs is 0.333**, still below the shipped ladder's lowest rung of 0.50. So even with
the exact allocator the ladder stays inert **until the bottom rung drops to about 0.25**. And
the block allocator is itself still self-inconsistent two thirds of the time.

### 2.5 With the allocator fixed and the bottom rung lowered, the ladder fires — and still buys nothing

`./fish_p8 coord --a=v04:belief=block --b=v04:belief=block --games=900 --seed=31 --no-measure
--forcedth='0.33|0.30|0.25|0.20|0.15|0.10|0.05|-1'` (1800 games, 20 forced sweeps):

```
forced sweeps        20   forced declarations 20   wrong 18 (90%)
rung   threshold    fired     wrong      mean conf
  0     0.3300  20 (100%)   18 (90%)   0.333333
  1..6  0.30 .. 0.05        0
  7   bestGuess  0 (0%)

   n=60  mean 0.292222  max 0.333333
   allocation named vs the declarer's OWN Knowledge:
       contradicts a known owner 56.6667%
       names a card at a seat its own mask excludes 56.6667%
       exceeds a teammate's capacity 63.3333%
       self-consistent 36.6667%
   picking the ARGMAX-confidence player for each half-suit would be right 10% of the time (n=20)
```

The ladder now selects the declarer at rung 0 in 100% of sweeps and never reaches `bestGuess`.
Forced-declaration error goes 100% -> 90%, on n=20 — **not significant, and not a result**.
Three observations that matter more than the point estimate:

- The confidence statistic tops out at **exactly 0.3333** in every one of 60 surveys. It is
  pinned at 1/3, i.e. one factor of "which of my three teammates holds this card" is entering
  the product with a flat posterior. A ladder cannot rank declarers whose statistic is a
  constant.
- Even on the exact `BlockDP` path, **56.7% of named allocations contradict an owner the
  declarer's own `Knowledge` already resolved**, and 63.3% still over-fill a teammate. The
  allocator is broken in more ways than the capacity constraint. (Flagging for P2, whose
  `probe_forcedendgame.hpp` carries the per-declaration anatomy.)
- Picking the argmax-confidence declarer would be right 10% of the time. There is no
  willingness protocol on top of a statistic this uninformative that produces a correct
  declaration.

**Conclusion for channel 2: the ladder is not the lever, at any shape, before or after the
allocator is repaired.** The forced endgame has to be fixed by not arriving there with an
unallocatable half-suit (P0b's dichotomy), not by re-tuning `Rules::forcedTh`.

---

## 3. Information safety of the two channels

### 3.1 The leak is exactly computable, not merely boundable

A willingness bit is a **deterministic** function of the responder's private state given the
public record. Let `X` be the candidates' hands, `R` the full transcript of a ladder sweep
(the rung index each candidate publishes), and condition throughout on the public record. Then
`H(R | X) = 0`, so

```
I(X ; R)  =  H(R) - H(R | X)  =  H(R).
```

**The number of bits an observer gains is exactly the observer's predictive entropy of the
transcript.** That identity is what makes a threshold ladder dangerous: any statistic rich
enough to be worth thresholding is rich enough that `H(R)` grows with the number of rungs. An
`r`-rung ladder publishes one of `r+1` values per candidate, so `I <= n_cand * log2(r+1)` —
6.34 bits for two candidates on the shipped 8-rung shape, 9.51 for three. The rules text
("cannot share any information other than their willingness to receive the turn") reads
naturally as **one bit per candidate**: `n_cand` bits, not `n_cand * log2(r+1)`.

### 3.2 Measured, for the turn-transfer ladder

`./fish coord --games=600 --seed=77 --leak` (1200 games, 168 multi-candidate decisions,
336 candidate reports, 624 (candidate, live half-suit) pairs):

```
  rung distribution over candidates (n=336):
    r0=37.5%  r1=1.19%  r2=0%  r3=5.36%  r4=17.86%  r5=19.05%  r6=19.05%  r7=0%  r8=0%
  H(rung) = 2.18812 bits
```

This 2.19 bits is the entropy of the *population* distribution of reported rungs. Conditioning
only reduces entropy, so `E[H(R | public)] <= H(R) = 2.19`: **the shipped 8-rung shape leaks at
most 2.19 bits per candidate on average, against a 3.17-bit combinatorial ceiling and the ~1
bit the rules text licenses.** A two-candidate transfer publishes up to ~4.4 bits about the two
teammates' hands, in the middle of a game, to opponents who still hold cards. The distribution
is also far from degenerate — six of the nine values are actually used, and the modal value
carries only 37.5% of the mass — so the realised leak is not far below the bound.

What that buys an observer, concretely — bucketing the predicate "this candidate holds at least
one card of this live half-suit" by the rung the candidate reported, with the prior taken from
the observing opponent's own posterior (`V04Agent::bel.marg`):

| rung | n | mean prior | realised | lift | dH (bits) |
|---|---|---|---|---|---|
| r0 (>=0.98) | 258 | 0.7022 | 0.8450 | +0.1428 | +0.256 |
| r1 (>=0.90) | 10 | 0.7503 | 0.8000 | +0.0497 | +0.089 |
| r3 (>=0.65) | 24 | 0.8682 | **1.0000** | +0.1318 | +0.562 |
| r4 (>=0.50) | 88 | 0.7249 | 0.8409 | +0.1160 | +0.217 |
| r5 (>=0.35) | 128 | 0.6860 | 0.7344 | +0.0484 | +0.063 |
| r6 (>=0.20) | 116 | 0.7373 | 0.7414 | +0.0040 | +0.006 |
| all | 624 | 0.716 | 0.808 | +0.092 | **+0.174 mean** |

**Honest caveat:** the realised rate exceeds the prior at *every* rung, including the
least-informative one, so a large part of that +0.092 is v0.4's own belief being systematically
under-confident about teammates rather than anything the ladder revealed. The ladder-specific
part is the *spread across rungs*: realised varies 0.734 -> 1.000 (range 0.266) while the prior
varies 0.686 -> 0.868 (range 0.182). The rigorous number is the
`I(X;R) = H(R | public) <= 2.19` bits bound; this table is the tangible illustration, with
buckets as small as n=10 and n=24.

### 3.3 The two channels have opposite safety profiles

- **Forced endgame — safe by construction.** The ladder only ever runs when the opposing team
  is *cardless*. A cardless player cannot ask, cannot be asked, and may not declare for the
  opposing team (rules of record). Every bit the ladder emits goes to an audience that can
  never act on it. Leakage there costs exactly nothing, and the shipped 8-rung shape is
  defensible on those grounds even though it is inert (§2).
- **Turn transfer — the leak lands mid-game.** Opponents hold cards, and the transfer is
  followed by a run of 3.36 asks at an 85% hit rate that ends with the turn crossing back to
  them; they can price the extra bits about each teammate straight into their next ask, and
  into their own declaration timing.

So the channel that leaks is the one with no value, and the channel with value (if it is ever
repaired) does not leak. Building the turn-transfer ladder means paying a mid-game
information price of up to ~4.4 bits per transfer for an expected return of `-0.0007 +/- 0.0024`
sets/game. **In spirit and in arithmetic, it is a bad trade.**

A one-rung protocol ("are you willing to receive the turn? yes/no") is within the literal rule,
caps the leak at 1 bit per candidate, and recovers 29.7% of the (worthless) gap. That is the
only version of this mechanism worth considering, and it is worth considering only because it
costs one line.

---

## 4. What this means for v0.5

1. **Do not build the turn-transfer willingness ladder.** The ceiling is measured, tight, and
   zero: `-0.0007 +/- 0.0024` sets/game for an *omniscient* chooser (§1.4). v0.4's unilateral
   rule already matches the oracle on the only metric that would matter (never passing to a
   teammate with no live ask: 0 failures in 6000 games) and is within 0.35 certain cards of it
   on the marginal metric. If anything is done here, do the one-rung version for the leak
   budget, and expect no measurable change.
2. **The forced-endgame ladder is inert, and reshaping it is provably a no-op.** Four ladder
   shapes — including one whose bottom rung is 0.10 and one with no rungs at all — produce
   *bit-identical* forced-declaration counts and 100% error. The lever is not
   `Rules::forcedTh`. (§2.2)
3. **The actual bug is the allocator, and it is in `evaluateSet` as well as `bestGuess`.**
   The BRIEF lists "no capacity constraint" as a `bestGuess` gap; the same independent
   per-card argmax sits at `v04.hpp:599-605` inside `evaluateSet`, which is what
   `willingForced` calls. 100% of surveyed allocations over-fill a teammate, which drives
   `Belief::jointSequential` to a hard `0.0` and shorts out every rung. Fix that first; only
   then does any ladder-shape question become meaningful.
4. **A live ladder is necessary but nowhere near sufficient.** With the capacity-feasible
   `belief=block` allocator *and* a bottom rung under 1/3, the ladder does fire in 100% of
   sweeps and never reaches `bestGuess` (§2.5) — but the statistic is pinned at exactly 0.3333
   in all 60 surveys, forced-declaration error moves 100% -> 90% on n=20 (not significant), and
   the argmax-confidence declarer is right 10% of the time. Do not budget any win here.
5. **Carry the `I(X;R) = H(R)` identity into task P6 (declaration arbitration).** That
   channel is mid-game with live opponents, exactly like the turn transfer, so a multi-rung
   confidence ladder there buys the same `log2(r+1)` bits of leak per participant. The
   comment at `game.hpp:205-209` already declines to rank by confidence for this reason; a
   ladder is a *softened* version of the same leak, not an escape from it, and should be
   priced with the measured 2.19 bits rather than assumed to be "one bit".

---

## Reproduction

New scratch files (none of the protected headers were touched):

- `engine/src/probe_coordination_game.hpp` — `fish::probecoord::Game`, a copy of
  `engine/src/game.hpp`'s driver with (a) `selectPassTarget` / `ladderPick` / `sureRun`,
  (b) forced-endgame rung recording, (c) `surveyForcedConfidence`, (d) `collectLeak`.
- `engine/src/probe_coordination.hpp` — threaded runner + report.
- `engine/src/main.cpp` — one appended `if (cmd == "coord")` block; no existing block edited.

Build: `cd engine && make` (the `belief=block` runs above used a second binary
`clang++ -std=c++20 -O3 -march=native src/main.cpp -o fish_p8 -pthread`, built only to avoid
overwriting `./fish` while other sessions were using it; the sources are identical).

```
# baseline + per-decision comparison (1.2, 1.3, 1.5)
./fish coord --games=3000 --seed=31

# paired oracle A/B (1.4)
./fish coord --games=3000 --seed=31 --no-measure --dump=uni.csv
./fish coord --games=3000 --seed=31 --pass=oracle --no-measure --dump=ora.csv

# ladder depth table (1.5)
./fish coord --games=600 --seed=31 --rungs='0.5'
./fish coord --games=600 --seed=31 --rungs='0.9|0.5'
./fish coord --games=600 --seed=31

# forced-endgame ladder shapes (2.2) -- all four give identical output
./fish coord --games=1500 --seed=31 --no-measure
./fish coord --games=1500 --seed=31 --no-measure --forcedth='0.50|0.40|0.30|0.25|0.20|0.15|0.10|-1'
./fish coord --games=1500 --seed=31 --no-measure --forcedth='0.30|0.15|0.05|-1'
./fish coord --games=1500 --seed=31 --no-measure --forcedth='-1'

# the confidence survey and the capacity diagnosis (2.3, 2.4)
./fish coord --games=400 --seed=31 --no-measure
./fish coord --a=v04:belief=block --b=v04:belief=block --games=250 --seed=31 --no-measure

# leakage (3.2)
./fish coord --games=600 --seed=77 --leak
```

Flags: `--pass=unilateral|oracle|ladder|low|cards`, `--rungs=a|b|c`, `--forcedth=a|b|c`,
`--leak`, `--no-measure` (skip the per-decision comparison, which is the expensive part),
`--dump=FILE` (per-game A-team set count, `games*rotations` lines, index `i*rot+r`),
`--both-teams` (apply the pass policy to both teams instead of only team A).
