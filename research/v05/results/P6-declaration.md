# P6 — Declaration timing and arbitration

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, at commit `fe21e19`.

Scratch code (nothing under `engine/src/{v04,belief,blockdp,game,fish}.hpp` was modified):

* `engine/src/probe_declaration_game.hpp` — a copy of the `Game` driver as `fish::probe::PGame`, with
  per-team arbitration modes (adds mode 3 = confidence ranking, mode 4 = public willingness
  ladder), a per-team out-of-turn permission and confidence gate, a per-team public declaration
  floor, race instrumentation, and a per-half-suit declaration-suppression window.
* `engine/src/probe_declaration.hpp` — the paired mirror match runner and the exact-BlockDP replay
  analysis.
* `engine/src/probe_declaration_main.cpp` — scratch driver (`probe_decl`), built with
  `clang++ -std=c++20 -O3 -march=native src/probe_declaration_main.cpp -o probe_decl -pthread`.

Everything below is v0.4 **mirror** play (`v04` in all six seats, engine defaults, 9 half-suits)
unless stated otherwise.

## Experimental design for the arbitration arms

Arbitration is a `Rules` field, i.e. game-wide, so an A/B in a mirror is degenerate. `PGame` makes
it **per team**, and each deal is played twice — treatment on team 0, then treatment on team 1.
Deal luck cancels exactly: the control arm (treatment = baseline on both sides) returns
**50.0000%** with a zero-width bootstrap interval, because the two orientations of a deal are then
literally the same game. Every deviation from 50% below is therefore caused only by the treatment.
The cross-team tiebreak (both teams want to cash something in the same round) is a deterministic
coin rather than "lowest seat", so a treatment cannot win by also winning cross-team races.

Sanity: the probe reproduces the P0 baseline — 139–145 events/game, 4.47 voluntary
declarations/game/team, 90% declaration accuracy (P0 reports 143.6 events, 10.4% wrong).

---

## 1. Arbitration: what the information-safety constraint costs

### 1.1 The rule as written

`Game::declarationRound` (`engine/src/game.hpp:202-231`) polls every seat, and at
`engine/src/game.hpp:223` takes the **first** proposer in the arbitration order:

```cpp
if (bestSeat < 0) { bestConf = conf; bestSeat = p; bestDecl = d; }
```

`conf` is stored and never compared. `Rules::declArbitration` (`engine/src/fish.hpp:119`) chooses
the order: 0 = lowest seat (default), 1 = highest, 2 = scan from the turn holder.

### 1.2 How often arbitration is decisive

Instrumented over 6,000 mirror games (3,000 deals, seed 31):

| quantity | count | share |
|---|---|---|
| voluntary declarations executed | 53,713 | — |
| rounds in which ≥2 seats of one team proposed simultaneously | 29,750 | 55.4% of declarations |
| ... naming **different** half-suits | 3,365 | 11.3% of races |
| ... with **different** stated confidences | 7,630 | 25.6% of races |
| ... where the confidence argmax is **not** the lowest seat (**contested**) | 4,961 | 16.7% of races |

Declaration races are the norm, not an edge case: more than half of all declarations are made in a
round where a teammate also wanted to declare. But in five sixths of those races the two teammates
are interchangeable, and arbitration only bites on the remaining 4,961.

### 1.3 Ground truth on the contested races

Both candidate declarations were scored against the true hands (diagnostic only, never shown to a
policy — `probe_declaration_game.hpp::declTrue`):

| outcome on the 4,961 contested races | count | share |
|---|---|---|
| lowest seat's allocation is true | 1,218 | 24.6% |
| confidence argmax's allocation is true | 1,415 | 28.5% |
| **both wrong** | 3,501 | **70.6%** |
| both right | 1,173 | 23.6% |
| only the confidence argmax right | 242 | 4.9% |
| only the lowest seat right | 45 | 0.9% |

Mean confidence gap between the two candidates: 0.119.

**The contested races are almost all races the team is going to lose.** 70.6% of the time both
teammates name a false allocation. Confidence ranking is never systematically worse (242 vs 45) but
it converts only +197/4,961 = **+4.0 pp** of contested-race accuracy, i.e. +0.033 correct
declarations per game per team.

### 1.4 Win-rate cost

Paired mirror duplicate, treatment vs. `declArbitration = 0`:

| arbitration rule | deals | games | win rate | 95% CI (deal cluster bootstrap) | Δ vs lowest seat |
|---|---|---|---|---|---|
| lowest seat (control) | 200 | 400 | 50.000% | [50.000, 50.000] | 0 (by construction) |
| highest seat (mode 1) | 1,500 | 3,000 | 49.533% | [49.17, 49.90] | **−0.47 pp** |
| scan from turn holder (mode 2) | 1,500 | 3,000 | 49.767% | [49.47, 50.07] | −0.23 pp |
| **confidence ranked (mode 3, information-UNSAFE)** | 3,000 | 6,000 | **50.300%** | **[50.12, 50.48]** | **+0.30 pp** |
| public willingness ladder, 17 rungs (mode 4, safe) | 3,000 | 6,000 | 50.267% | [50.10, 50.43] | +0.27 pp |
| public willingness ladder, `forcedTh` rung shape (mode 4, safe) | 1,500 | 3,000 | 50.033% | [49.93, 50.17] | +0.03 pp |

**The information-safety constraint costs +0.30 pp of win rate (95% CI [+0.12, +0.48]).** The
refusal in `game.hpp:205-209` is essentially free — the price of never comparing private
confidences across seats is three tenths of a percentage point. This is the same conclusion
`research/v04/results/E17-arbitration.jsonl` reached against fixed opponents (low 75.07%, high
74.93%, turn 74.38% vs v0.3; all three within each other's intervals), now with the
confidence-ranked upper bound that E17 could not measure, and in the mirror where declarations
actually go wrong.

Note that "lowest seat" is not an arbitrary choice among the *safe* rules: it beats highest seat by
0.47 pp and scan-from-turn by 0.23 pp, both larger than the entire gap to the unsafe upper bound.
v0.4 happens to have picked the best of the three seat orders.

### 1.5 The information-safe substitute, and how many bits it needs

Design: the team sweeps a descending ladder of **public** thresholds `th_0 > th_1 > ... > th_{R-1} = 0`;
at each rung every seat reveals only the bit `[conf_p ≥ th_r]`, and the lowest seat that answers
"yes" at the highest reached rung declares. This is exactly the channel `Rules::forcedTh`
(`engine/src/fish.hpp:126-127`) already uses in the forced endgame — no private value ever crosses
a seat boundary, only `⌈log2 R⌉` bits of self-classification against public constants.

Recovery measured directly on the contested races (one run, n = 5,058 contested races; lowest seat
= 1,224 correct, full confidence ranking = 1,423 correct):

| rungs R | bits/seat | contested races won | fraction of the confidence gap recovered |
|---|---|---|---|
| 1 (lowest seat) | 0 | 1,224 | 0% |
| 2 | 1 | 1,287 | 32% |
| 3 | 1.6 | 1,289 | 33% |
| 5 | 2.3 | 1,390 | **83%** |
| 9 | 3.2 | 1,414 | **95%** |
| 17 | 4.1 | 1,416 | 96% |
| 33 | 5.0 | 1,423 | 100% |
| 65 | 6.0 | 1,427 | 102% |

The R = 17 ladder recovers +0.27 pp of the +0.30 pp win-rate gap (**89%**). One single public bit
("I am certain") is already worth a third of it.

**The rung *shape* matters more than the rung count.** The ladder built from the existing
`Rules::forcedTh` values `{0.995, 0.98, 0.95, 0.90, 0.80, 0.65, 0.50, −1}` recovers only 10% of the
gap (+0.03 pp), because every one of its rungs sits at or above 0.5 while contested confidences
cluster far below (70.6% of contested races are hopeless for both candidates; mean gap 0.119). A
ladder that is evenly spaced over [0, 1] recovers 95% with nine rungs. If v0.5 reuses `forcedTh`
for declaration arbitration it must not reuse `forcedTh`'s values.

### 1.6 Verdict on task 1

The hypothesis that lowest-seat arbitration is costing anything material **does not hold**. The
whole arbitration question is worth at most half a percentage point, and an information-safe ladder
recovers nine tenths of that. The reason is visible in §1.3: arbitration is contested exactly in
the states where the team is guessing, and choosing the better of two guesses barely helps. The
leverage is in §4 below — not *which* teammate declares, but *whether* anybody should.

---

## 2. Timing: is v0.4 declaring too early or too late?

Corpus: 600 mirror games, seed 31, 5,372 voluntary declarations. For each declaration the
declarer's `Knowledge` is rebuilt from the public event stream and scored with the **exact BlockDP
posterior** (`blockdp.hpp::bestTeamAllocation` / `teamOwnsProbability`), at the declaration and at
5 / 10 / 20 events earlier. 0 BlockDP build failures. "argmax-correct" = the exact posterior's
best team allocation matches the true hands *at that moment*.

### 2.1 Backward: what was knowable earlier

| offset | n | exact P(alloc) | P(team owns) | exact argmax correct |
|---|---|---|---|---|
| −20 | 4,840 | 0.047 | 0.081 | 6.1% |
| −10 | 5,235 | 0.078 | 0.118 | 9.0% |
| −5 | 5,361 | 0.136 | 0.178 | 12.2% |
| **0 (declaration)** | 5,372 | **0.842** | 0.865 | **88.3%** |

Split on whether the half-suit was, in ground truth, locked to the declarer's team at the moment of
declaration:

| split | n | P(alloc) @−5 | P(alloc) @0 | argmax correct @0 | v0.4's own stated conf |
|---|---|---|---|---|---|
| locked to declarer's team | 4,947 (92.1%) | 0.143 | 0.908 | 95.8% | 0.963 |
| **not locked** | 425 (7.9%) | 0.055 | **0.080** | **0.0%** | **0.157** |
| declaration turned out correct | 4,866 | 0.143 | 0.917 | 96.8% | 0.969 |
| **declaration turned out wrong** | 506 (9.4%) | 0.072 | **0.120** | 5.9% | **0.233** |
| at/after event 220 (forcing horizon) | 653 (12.2%) | 0.095 | 0.317 | 35.8% | 0.387 |
| before event 220 | 4,719 | 0.142 | 0.915 | 95.5% | 0.970 |

Three facts:

1. **v0.4 is not late.** Five events before it declares, the exact posterior on the half-suit it is
   about to cash is 0.136 and the best allocation is right 12% of the time. The information arrives
   in a burst — typically the run of hits that strips the last cards — and v0.4 fires within a
   handful of events of it arriving. There is no reservoir of already-knowable half-suits sitting
   uncashed.
2. **v0.4's misdeclarations are not inference failures.** On the 506 wrong declarations the exact
   posterior at the moment of declaration is 0.120, and v0.4's own stated confidence is 0.233. It
   knows it is guessing. 425 of the wrong declarations name a half-suit at least one of whose cards
   is, at that instant, physically in an opponent's hand — the exact argmax is correct 0.0% of the
   time, because no allocation to the declarer's team can be correct.
3. That behaviour is deliberate and is in the code: `V04Agent::declareNow`
   (`engine/src/v04.hpp:675-676`) returns `true` unconditionally once `pressure() ≥ 2`
   (`engine/src/v04.hpp:583`, i.e. from event 308) and on any `pAlloc ≥ 0.5` once
   `pressure() ≥ 1` (event 220).

### 2.2 Forward: the true counterfactual

"What would it have been 5/10/20 events later" cannot be read off the realised trace — the
declaration removes the half-suit from play. `PGame` therefore re-runs the same deal with that
half-suit's voluntary declarations suppressed for a 21-event window (`blockSet` / `blockFromEvent`
/ `blockUntilEvent`) and scores the same seat's exact posterior on the same half-suit as play
continues. "truncated" = the half-suit left play inside the window anyway (forced endgame).

**Corpus A — the first voluntary declaration of each game (600 cases; 99% locked, i.e. the easy
ones):**

| offset | n | exact P(alloc) | Δ vs declaration | argmax correct |
|---|---|---|---|---|
| 0 | 600 | 0.954 | — | 96.2% |
| +5 | 600 | 0.955 | +0.001 | 96.0% |
| +10 | 600 | 0.961 | +0.006 | 96.3% |
| +20 | 597 | 0.962 | +0.003 | 96.8% |

**Corpus B — the first *wrong* voluntary declaration of each game where one exists (600 cases):**

| split | offset | n | exact P(alloc) | Δ | argmax correct | truncated |
|---|---|---|---|---|---|---|
| all | 0 | 600 | 0.743 | — | 72.0% | — |
| all | +5 | 591 | 0.770 | +0.021 | 75.5% | 17 |
| all | +10 | 574 | 0.810 | +0.042 | 78.4% | 17 |
| all | +20 | 529 | 0.849 | +0.026 | **83.9%** | 17 |
| locked to own team | 0 | 485 | 0.878 | — | 89.1% | — |
| locked to own team | +20 | 473 | 0.904 | +0.012 | 92.2% | 2 |
| **not locked** | 0 | 115 | 0.175 | — | **0.0%** | — |
| **not locked** | +10 | 100 | 0.384 | **+0.198** | 12.0% | 15 |
| **not locked** | +20 | 56 | 0.380 | +0.139 | 14.3% | 15 |

### 2.3 Verdict on task 2

**Split by lock, the answer is opposite in the two halves.**

* For a half-suit **locked to the declarer's team** (92% of voluntary declarations) v0.4's timing is
  about right, and waiting is nearly worthless: twenty more events move the exact posterior by
  +0.003 on the easy corpus and +0.012 on the hard one. This is Theorem 1 behaving as advertised —
  once the half-suit is locked, v0.4's ask policy emits no further information about it, so waiting
  buys nothing. Cashing immediately is correct.
* For a half-suit **not locked to the declarer's team** (7.9% of voluntary declarations, and 84% of
  all misdeclarations) v0.4 declares **far too early**. These declarations are correct 0% of the
  time by construction, and letting play continue raises the exact posterior from 0.175 to 0.384 in
  ten events and converts 12–14% of them into correct allocations. The +11.9 pp swing on corpus B
  (72.0% → 83.9% at +20) is almost entirely this class.
* The 15/115 truncations in the not-locked class are themselves a finding: for a hopeless half-suit,
  "wait" often means "reach the forced endgame and misdeclare there anyway". Delay alone does not
  fix the class; it only helps where the missing card can still be won back.

This is exactly the user's judgement in the brief: *it is always better to try asking someone else,
even if a little risky, than to misdeclare at the end because of incomplete information.* §4.2
prices that judgement.

---

## 3. The stopping rule at higher power, in the mirror

`docs/FISHBOT_V04.md` records the frozen-policy ablation as unresolved (+0.12 points, 95% CI
−1.23 to +1.47) — measured against v0.3. Re-run as `v04` vs `v04:vdecl=0` (identical policies
except that the treatment decides declarations by the value function, `V04Config::valueDeclare`,
while the control uses the fixed `declThreshold = 0.8177` rule), seed-paired with both
orientations of every deal:

| run | deals | games | value-rule win rate | 95% CI | sets/game | decl acc | mean lock-hold |
|---|---|---|---|---|---|---|---|
| C (seed 4242) | 2,500 | 5,000 | 50.96% | [49.80, 52.14] | 4.552 – 4.448 | 0.903 / 0.904 | 7.6 vs 15.8 events |
| **F (seed 99001)** | **6,000** | **12,000** | **50.97%** | **[50.23, 51.72]** | 4.531 – 4.469 | 0.898 / 0.904 | 8.1 vs 16.7 events |

**Yes — in the mirror the value rule does resolve a benefit: +0.97 pp of win rate, 95% CI
[+0.23, +1.72]** (F; C reproduces the same point estimate at lower power, and the two runs pool to
+0.97 pp over 17,000 games). It is a small effect, roughly +0.06 half-suits per game, and it is
*not* an accuracy effect — declaration accuracy is if anything slightly lower with the value rule
(0.898 vs 0.904). It is a *speed* effect: the value rule cashes a locked half-suit after 8.1 events
instead of 16.7, i.e. it spends half as long sitting on points.

Two caveats worth recording. First, the coefficients being exercised are the ones compiled into
`V04Config::vw`, which `docs/FISHBOT_V04.md` already flags as **not** the fitted vector in
`research/v04/results/E14-valuefit.txt`; the measured +0.97 pp is a property of the shipped
constants, not of the fitted value function. Second, an effect of this size is an order of
magnitude smaller than the declaration-floor effect in §4.2, so the stopping rule is not where
v0.5's declaration effort belongs.

---

## 4. Out-of-turn declarations

### 4.1 Accuracy

From the same 600-game corpus (5,372 voluntary declarations; turn holder reconstructed exactly from
the event stream):

| | n | share | realised correct | exact P(alloc) at declaration | v0.4 stated conf | locked to own team | made at/after event 220 |
|---|---|---|---|---|---|---|---|
| on-turn | 1,391 | 25.9% | **95.1%** | 0.933 | 0.947 | 95.8% | 9.2% |
| out-of-turn | 3,981 | **74.1%** | **89.0%** | 0.810 | 0.883 | 90.8% | 13.2% |

Out-of-turn declarations are **6.1 pp less accurate**, and the exact posterior confirms the gap is
real rather than a selection artefact (0.810 vs 0.933). Three quarters of all voluntary
declarations are out-of-turn, so this class carries most of v0.4's misdeclarations. Note the
brief's "~3.3/game" is per team; the table counts both teams.

### 4.2 Is the ability worth anything?

Per-team A/B, same paired mirror design (1,500 deals / 3,000 games each, seed 31). Every arm is
measured against unmodified v0.4 (out-of-turn always allowed) unless stated:

| treatment on the X team | X win rate | 95% CI | Δ | X decl acc | Y decl acc | events/game |
|---|---|---|---|---|---|---|
| out-of-turn allowed (= v0.4, control) | 50.00% | [50.00, 50.00] | 0 | — | — | 139 |
| out-of-turn **forbidden** | **53.37%** | [51.73, 55.00] | **+3.37 pp** | 0.911 | 0.804 | 168 |
| out-of-turn allowed only when conf ≥ 0.90 | **55.23%** | [54.03, 56.43] | **+5.23 pp** | 0.937 | 0.847 | 146 |
| out-of-turn allowed only when conf ≥ 0.995 | **56.07%** | [54.83, 57.27] | **+6.07 pp** | 0.930 | 0.824 | 155 |
| conf ≥ 0.995 gate, measured against **out-of-turn forbidden** | 53.47% | [51.90, 55.07] | +3.47 pp | 0.823 | 0.804 | 192 |

**The ability is worth about +3.5 pp — but only under a gate.** As v0.4 exercises it, out-of-turn
declaration is a net **loss** of 3.37 pp: a team forbidden from declaring out of turn beats
unmodified v0.4. Gate it on the declarer's own confidence against a public constant and it becomes
a net gain of 6.07 pp over v0.4, and 3.47 pp over never using it at all. The gate is
information-safe in exactly the sense §1.5 requires: a seat compares its own private confidence to
a public number and never reveals the number, so no private value crosses a seat boundary.

### 4.3 The gate generalises: a public declaration floor is the largest single lever found

The gate above only restricts *out-of-turn* declarations. Applying the same public floor to **all**
voluntary declarations (`PGame::declFloor`, i.e. a seat proposes only when its own confidence
clears a public constant) sweeps as follows (1,500 deals / 3,000 games each, X = floor, Y = v0.4):

| public floor | X win rate | 95% CI | X sets | X decl acc | Y decl acc | X decls/game | events/game | 400-ask limit hit |
|---|---|---|---|---|---|---|---|---|
| 0.30 | 55.53% | [54.73, 56.33] | 4.755 | 0.961 | 0.856 | 4.21 | 139.5 | 0% |
| 0.50 | 55.77% | [54.97, 56.60] | 4.777 | 0.970 | 0.853 | 4.18 | 139.7 | 0% |
| 0.70 | 57.30% | [56.37, 58.27] | 4.864 | 0.982 | 0.832 | 4.10 | 141.6 | 0% |
| 0.90 | 59.57% | [58.23, 60.90] | 4.939 | 0.992 | 0.811 | 4.01 | 149.1 | 0% |
| 0.95 | 59.33% | [58.03, 60.67] | 4.967 | 0.993 | 0.796 | 3.94 | 154.7 | 0% |
| **0.99** | **61.17%** | **[59.83, 62.53]** | 5.067 | **0.997** | 0.766 | 3.84 | 163.1 | 0% |
| 0.999 | 61.20% | [59.87, 62.57] | 5.068 | 0.997 | 0.764 | 3.82 | 164.0 | 0% |

Monotone up to a plateau at ~0.99, worth **+11.2 pp of win rate** over v0.4. The 400-ask column for
floors 0.30–0.90 comes from a separate 700-deal confirmation run of the same arms (`W_*` in the
reproduction log), which also reproduced the win rates within 1 pp: 55.36 / 55.57 / 57.50 / 58.71%.
No arm in this table ever hit the safety valve.

**Independent replication on the unmodified engine.** The same policy is expressible with
`V04Config` knobs alone — `v04:vdecl=0,decl=0.99,lockthr=0.99,patient=0,force=1000000` (fixed
threshold at 0.99, no patience delay, forcing horizon disabled) — and reproduces the effect through
the ordinary `fish match` path with no probe code in the loop:

| opponent | v0.4 | threshold-0.99 variant | Δ | variant decl acc | opponent decl acc |
|---|---|---|---|---|---|
| v0.4 (2,000 games) | 50% | **62.0%** [60.3, 63.7] | +12.0 pp | 0.998 | 0.760 |
| v0.3 (1,200 games) | 74.3% [71.8, 76.8] | 76.7% [74.3, 79.1] | +2.3 pp | 0.997 | 0.818 |
| lockout (1,000 games) | 77.6% [75.0, 80.1] | 79.5% [77.1, 81.8] | +1.9 pp | 0.997 | 0.824 |
| detective (1,000 games) | 75.2% [72.6, 77.8] | 78.2% [75.7, 80.7] | +3.0 pp | 0.997 | 0.895 |
| bluffer (1,000 games) | 99.8% [99.5, 100] | 100.0% [100, 100] | +0.2 pp | 0.998 | 0.139 |

Worst case across the panel is +0.2 pp; the variant is not worse against any opponent tested, and
its own declaration accuracy is 99.7–99.8% everywhere.

### 4.4 The caveat that matters: the floor is an exploit, not a cure

Most of the +11 pp is **the opponent's donations, not the floor's own production**. In the floor-0.99
arm the opposing v0.4's declaration accuracy collapses from 0.90 to 0.766: when the floor team
refuses a half-suit it cannot allocate, the unmodified opponent eventually grabs it wrongly and
hands it over. Running the floor on **both** teams (800 deals / 1,600 games, floor 0.99 on both
sides) shows what is left when there is nobody to donate:

| both teams at floor 0.99 | value |
|---|---|
| declaration accuracy | 0.996 |
| voluntary declarations per team per game | 3.21 (of 4.5 half-suits) |
| events/game | **228** (v0.4 mirror: 139) |
| **games hitting the 400-ask safety valve** | **42.0%** |

A universal 0.99 floor turns the misdeclaration pathology into the **deadlock** pathology: 42% of
games run out of asks and are settled by neutral majority adjudication rather than by play. That is
the same failure the brief describes, arrived at from the other side. The floor is a correct
diagnosis of the declaration rule and a real unilateral gain, but on its own it converts one
pathology into another — it only becomes a genuine fix if paired with an ask rule that keeps
producing information about unresolved half-suits, which is P0's central hypothesis.

---

## Summary of what did and did not hold

| hypothesis | verdict |
|---|---|
| Lowest-seat arbitration costs the team real win rate | **No.** +0.30 pp [+0.12, +0.48] against the unsafe confidence-ranked upper bound. |
| A public willingness ladder can recover most of that gap | **Yes.** 89% of the win-rate gap with 17 rungs; 95% of the contested-race gap with 9 rungs (~3 bits/seat). But the existing `forcedTh` rung values recover only 10% — the rungs must be spread over [0,1], not bunched above 0.5. |
| v0.4 declares too late (sits on knowable half-suits) | **No.** Five events earlier the exact posterior is 0.136 and the argmax is right 12% of the time. It fires within a few events of the information arriving. |
| v0.4 declares too early | **Yes, on the 7.9% of declarations where the half-suit is not locked to its team.** Those are 0% correct at declaration; twenty more events of play convert 14% of them and raise the exact posterior from 0.175 to 0.384. |
| Waiting helps on locked half-suits | **No.** +0.003 to +0.012 of exact P(alloc) over twenty events. Theorem 1 holds; cashing immediately is right. |
| The value-based stopping rule beats a fixed threshold in mirror play | **Yes, weakly.** +0.97 pp [+0.23, +1.72] over 12,000 games — resolved where the v0.3 ablation was not. It is a speed effect (8.1 vs 16.7 events of lock-hold), not an accuracy effect, and it uses the un-refitted `vw` constants. |
| Out-of-turn declarations are as accurate as on-turn ones | **No.** 89.0% vs 95.1%; exact P(alloc) 0.810 vs 0.933. They are 74% of all voluntary declarations. |
| The out-of-turn ability is worth having | **Only if gated.** Ungated it costs 3.37 pp against a team that never uses it; gated at conf ≥ 0.995 it gains 3.47 pp over never using it and 6.07 pp over v0.4. |
| A public confidence floor on declarations is the big lever | **Yes unilaterally (+11.2 pp, replicated on the unmodified engine against five opponents), no universally** — both teams at floor 0.99 deadlock 42% of games into the 400-ask valve. |

## Recommendations for v0.5

1. **Put a public confidence floor on voluntary declarations and delete the unconditional
   `press ≥ 2` escalation** (`engine/src/v04.hpp:675`). This is the single largest measured
   improvement in this study and it is information-safe.
2. **Do not ship the floor without an information-producing ask rule.** §4.4 shows a universal
   floor deadlocks 42% of mirror games. The floor and P0's wiretap-priced ask rule are one change,
   not two.
3. **Gate out-of-turn declarations** on the declarer's own confidence against a public constant
   (≈0.99). Keep the ability; it is worth +3.5 pp over disabling it.
4. **Arbitration is not worth engineering effort.** If a willingness ladder is added anyway (it is
   free and it is what the rules permit), use ~9 evenly spaced rungs over [0,1], not the
   `Rules::forcedTh` values.
5. Re-fit `V04Config::vw` before drawing any further conclusion about the value-based stopping
   rule; the +0.97 pp measured here belongs to the shipped constants.

## Raw run outputs

Every run cited above is archived under `research/v05/runs/P6/`:
`arb1..4` (arbitration modes 1/2/3/4 at 1,500 deals), `A_conf` / `B_ladder17` (3,000-deal
arbitration arms and the ladder recovery curve), `G_timing_first` / `H_timing_wrong` (timing),
`C_vdecl` / `F_vdecl_big` (stopping rule), `D_oot` / `E_oot_off` / `I_ootgate90` / `J_ootgate995` /
`K_gate_vs_off` (out-of-turn), `L_veto50` and `V_*` / `W_*` (declaration floor sweep and its
safety-valve confirmation), `O_bothveto` (both-team floor), `M_*` / `N*` / `P_*` (unmodified-engine
replication and the opponent panel).

## Reproduction

```
cd engine
clang++ -std=c++20 -O3 -march=native src/probe_declaration_main.cpp -o probe_decl -pthread

# 1.4 arbitration win-rate arms  (x = 1 highest, 2 from-turn, 3 confidence, 4 ladder)
./probe_decl arb  --games=3000 --x=3 --y=0 --seed=31
./probe_decl arb  --games=3000 --x=4 --y=0 --rungs=17 --seed=31     # ladder recovery curve in the same output
./probe_decl arb  --games=1500 --x=4 --y=0 --seed=31                # forcedTh-shaped ladder
# 2 timing
./probe_decl timing --games=600 --seed=31 --pick=0                  # forward corpus A
./probe_decl timing --games=600 --seed=31 --pick=2                  # forward corpus B (first wrong)
# 3 stopping rule
./fish match --a=v04 --b=v04:vdecl=0 --games=6000 --seed=99001 --json
# 4 out-of-turn
./probe_decl oot  --games=1500 --on=1     --seed=31                 # ability ON vs OFF
./probe_decl oot  --games=1500 --ootth=0.995 --seed=31              # gated vs v0.4
./probe_decl oot  --games=1500 --ootth=0.995 --yoff=1 --seed=31     # gated vs OFF
./probe_decl veto --games=1500 --fx=0.99  --seed=31                 # public declaration floor
./probe_decl veto --games=800  --fx=0.99 --fy=0.99 --seed=31        # both-team floor (deadlock check)
./fish match --a='v04:vdecl=0,decl=0.99,lockthr=0.99,patient=0,force=1000000' --b=v04 --games=1000 --seed=515 --json
```
