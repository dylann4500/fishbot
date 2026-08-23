# P3 — Deception and opponent-model misspecification

Dylan Nguyen, FishLab Research Project
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.
All runs below were produced by the engine at that commit plus the new, non-invasive probe files
listed in the appendix. No protected header was modified.

---

## 0. Headline

The brief's stated hypothesis was that v0.4's fixed policy prior — `priorTheta`, `priorPhi`
(`engine/src/v04.hpp:61-62`, consumed in `Knowledge::priorWeight`, `engine/src/belief.hpp:100-110`)
— is a *liability* against adaptive opponents, and that the policy-agnostic configuration
`v04:ptheta=0,pphi=0` would therefore be more robust.

**That did not hold.** The policy-agnostic posterior is worse against every deceptive archetype
tested, by 3.9 to 6.4 points of win rate, and worse by 7.0 points in the honest mirror; in a
paired ablation over a panel containing all three archetypes it is worse by 4.4 points
[1.6, 7.2] (2000 games per arm). The prior loses roughly a third to a half of its value against a
deceiver but never becomes negative. Over-weighting it is the real exposure: doubling the pair
costs 5.3 points [2.5, 8.1] paired, and quadrupling it loses the match outright to the Withholder
(§5).

What *did* hold, in a sharper and smaller form:

1. **The silence channel (`priorPhi`) is decorative.** Deleting it entirely (`v04:pphi=0`) costs
   nothing measurable anywhere: 49.00% [45.8, 52.2] against honest v0.4 over 800 games, and a
   paired delta of +0.3 points [−2.4, +3.1] over a four-opponent panel including all three
   archetypes (2000 games per arm). A 15-fold sweep of `pphi` against the Withholder is flat
   inside its intervals (§5), and the posterior cells that `phi` is supposed to sharpen do not
   move under deception (§2). This is the exact channel the project owner's manoeuvre attacks, and
   it is carrying no information to begin with.
2. **The certificate channel (`priorTheta`) is the one deception actually degrades**, and it
   degrades mostly for a structural reason that no reweighting can repair: a deceiver's asks
   satisfy the C5 disjunction (`belief.hpp:157-171`) with less concentrated evidence. Of the
   0.122 posterior mass v0.4 loses on correctly located opponent cards in that channel against
   the Withholder, only 0.027 (22%) is attributable to the soft prior; 78% survives with
   `ptheta=0,pphi=0`.
3. **The damage that does exist is a signed, cell-local miscalibration, and the Feint is the
   cheapest way to inflict it.** Against a table of Feinters, a seat that has asked once in a
   half-suit really holds a given unresolved card of it 28.2% of the time and v0.4 believes 37.8%
   — an over-confidence of **+0.096** (+0.152 at two asks). Against honest opponents the same cells
   are calibrated to within 0.07 and conservative. The Feint costs its user *nothing* (75.4% vs
   v0.3, against v0.4's 75.0%). Repeating the measurement with `ptheta=0,pphi=0` reproduces
   +0.088 of that +0.096, so the exploit runs through the **hard C5 certificate**, which the soft
   prior only slightly amplifies.
4. **Global belief accuracy is essentially untouched by deception.** Mean absolute marginal error
   moves from 0.2255 (honest control) to 0.2219–0.2275 across all archetypes — in the wrong
   direction as often as the right one. The hard certificates dominate, and a deceiver cannot make
   v0.4 believe anything provably false, only leave it less informed.
5. **Wrong declarations do not concentrate on the deceptive seat.** Cards mis-assigned in a wrong
   v0.4 declaration sit at the marked deceptive seat 27.2 / 23.0 / 27.2 / 27.3% of the time
   (strict Silent / capped Silent / Feint / Withholder) against a positional baseline of 25.3%
   with an honest opponent at the same seat. The shift is ≤ 2 points.

6. **No archetype converts its corruption of v0.4's beliefs into wins.** Shipped v0.4 beats all
   three (54.0% vs Feint, 56.1% vs capped Silent, 67.8% vs Withholder), and the Feint achieves that
   while being exactly as strong as v0.4 against v0.3. The owner's live result — that deception
   confused the bot — is therefore **not reproduced by commitment deception**. Two differences
   remain untested and are the natural next step: a human best-responds (these archetypes do not
   observe v0.4's beliefs at all), and a human plays a v0.4 that is simultaneously suffering the P0
   deadlock and the 58.6%-wrong late declarations, which is where an extra scrap of confusion is
   most likely to be decisive.

The practical reading for v0.5: the opponent model is not what deception breaks. What deception
breaks is the *supply of information*, and v0.4 has no mechanism that reacts to an information
drought — which is the same defect P0 found as the dead-ask deadlock. An online opponent model
(§6) is worth building for other reasons, but it should be built as a bounded tilt on top of the
hard certificates and should not be expected to recover the loss measured here.

---

## 1. The archetypes

`engine/src/probe_deception.hpp`. Each is a `V04Agent` subclass whose **only** departure is the set
of half-suits it is willing to ask in; belief, ask scoring, expectimax, declaration stopping rule,
forced endgame and pass-target choice are the shipped v0.4 code. The restriction is applied by
temporarily hiding the agent's own cards of the forbidden half-suits from `k.myHand` and
re-entering `V04Agent::chooseAsk`, which is exact rather than approximate because every v0.4
feature that consults `k.myHand` does so in the form `mine ? 1.0 : pTeamCard(c)`
(`v04.hpp:294`, `v04.hpp:305`, `v04.hpp:352`) and `pTeamCard` already returns exactly 1.0 for a
card the agent holds. Hiding a card changes ask legality (`fish.hpp:186`) and nothing else that
the surviving candidates see.

Validation of the machinery: `withholder:k=0` (an archetype that can never deviate) reproduces the
v0.4 mirror bit-for-bit — identical win rate, ask accuracy, declaration accuracy, events/game and
lock-hold statistics on the same seeds.

| archetype | rule | asks deviating from the v0.4 pick |
|---|---|---|
| `silent` | never ask in the half-suit it holds most cards of, until no other legal ask exists | 37.3% |
| `silent:tol=0.10` | the same, but break silence rather than give up more than 0.10 of hit probability | 28.3% |
| `feint` | prefer a half-suit it holds **exactly one** card of, when the hit probability given up is ≤ 0.10 | 15.2% |
| `withholder:k=6` | after being asked in half-suit S while still holding cards of S, do not ask in S for the next 6 of its own asks | 27.1% |

`feint` manufactures a *true but misleading* C5 certificate: the ask proves it holds another card
of that half-suit, which is correct, while it in fact holds only that one.

### Material given up

Two independent measurements. First, the archetype's own hit probability under its own posterior,
averaged over every ask it makes, versus what v0.4 would have picked in the same position
(`fish deceit`, one deceiver at seat 1, 800 games):

| archetype | v0.4 pick P(hit) | archetype pick P(hit) | on deviating decisions |
|---|---|---|---|
| `silent` | 0.443 | 0.362 | 0.469 → 0.250 |
| `silent:tol=0.10` | 0.313 | 0.316 | 0.148 → 0.160 |
| `feint` | 0.335 | 0.337 | 0.177 → 0.189 |
| `withholder:k=6` | 0.478 | 0.312 | 0.883 → 0.270 |

The capped archetypes give up essentially nothing in hit probability (they trade down on v0.4's
*score*, not on P(hit) — v0.4's ask rule is not a hit-probability maximiser).

Second, and more honestly, the price in wins against a fixed weaker reference opponent
(v0.3, 400 deals = 800 games, seed 20260822):

| policy | win rate vs `v03` | cost relative to v0.4 |
|---|---|---|
| `v04` | 75.00% [71.9, 77.9] | — |
| `feint` | 75.38% [72.2, 78.5] | **0.0** |
| `silent:tol=0.10` | 69.75% [66.6, 72.9] | −5.2 |
| `withholder:k=6` | 57.75% [54.5, 61.0] | −17.2 |
| `silent` (uncapped) | 42.88% [39.4, 46.2] | −32.1 |

Read the rest of this report with that column in hand. `feint` is a free deception; capped
`silent` is cheap; `withholder` is expensive; uncapped `silent` is not a competent player and its
numbers measure weakness, not deception. It is reported only as the extreme of the family.

---

## 2. (a) Belief damage

`fish deceit` reconstructs nothing: it reads the three measured v0.4 agents' *own* posteriors
(`V04Agent::bel.marg`) at every public event through `Game::observer` and scores them against the
true hands. Measured team is always seats {0,2,4} running plain v0.4. 400 deals × 2 hand
rotations = 800 games per row, seed 101, every second event sampled, ≈ 24 M
(unresolved card, seat) pairs per row.

Deceiver at seat 1 only, the other two opponents plain v0.4:

| opponent at seat 1 | mean abs. error | mean P(true holder) | "confidently wrong" (<0.10 on truth) |
|---|---|---|---|
| `v04` (control) | 0.2255 | 0.3234 | 3.46% |
| `silent` | 0.2240 | 0.3279 | 3.68% |
| `silent:tol=0.10` | 0.2248 | 0.3256 | 3.84% |
| `feint` | 0.2256 | 0.3232 | 3.89% |
| `withholder:k=6` | 0.2258 | 0.3226 | 3.80% |

The **maximum** absolute error is 1.000 in every arm including the honest control, so it separates
nothing: with 45 unresolved cards and six candidate seats, some card is always confidently
mislocated somewhere in an 800-game sample. The `confidently wrong` column is the usable tail
statistic. With all three opponent seats deceptive the ordering is unchanged (mean error
0.2201–0.2275: `silent` 0.2201, `silent:tol=0.10` 0.2228, `withholder:k=6` 0.2219, `feint` 0.2275,
against the same 0.2255 control).
**No archetype degrades v0.4's aggregate posterior.** The hard constraints C1–C5 do the work; the
deceiver removes evidence rather than injecting falsehood, and removing evidence moves the
posterior toward uniform rather than toward a wrong answer. `confidently wrong` rises by 0.2–0.4
points, which is the only aggregate signal, and it is small.

### Where the damage actually is

Bucketing the same marginals by the two statistics `priorWeight` uses — has this seat asked in
this half-suit, and how many turns has it spent elsewhere — localises it. The cell that matters is
*the seat holds this card and has asked in its half-suit*, which is where both the hard C5
certificate and the `theta` tilt deposit mass:

| opponent at seat 1 | holder & has asked | holder & silent | non-holder & has asked | non-holder & silent |
|---|---|---|---|---|
| `v04` (control) | **0.6652** | 0.2284 | 0.3172 | 0.1767 |
| `silent` | 0.5875 | 0.2268 | 0.3142 | 0.1708 |
| `silent:tol=0.10` | 0.6055 | 0.2268 | 0.3205 | 0.1692 |
| `feint` | 0.6472 | 0.2358 | **0.3357** | 0.1856 |
| `withholder:k=6` | **0.5428** | 0.2330 | 0.3053 | 0.1740 |

Three things to read here.

* The Withholder costs v0.4 **0.122 of posterior mass** on the cards it actually holds in
  half-suits it has asked in. That is the deception working, and it is large.
* The Feint raises false attribution — mass placed on a seat that has asked in a half-suit but does
  **not** hold the card — from 0.3172 to 0.3357 (+0.019). Small, but it is the only archetype that
  moves this cell, and it is exactly the cell the feint is designed to poison.
* The **silence cells barely move at all** (0.2284 → 0.2268–0.2358). The owner's manoeuvre attacks
  the `phi` channel, and the `phi` channel is not carrying information in v0.4's posterior to
  begin with. §4 shows the same thing from the other end: deleting `phi` costs no win rate.

Re-running the whole table with the measured team set to `v04:ptheta=0,pphi=0` separates prior
from structure. The policy-agnostic posterior loses the same cell against the Withholder:
0.6101 (control) → 0.5148, i.e. −0.095 of the −0.122 that full v0.4 loses. **78% of the damage is
structural** — a deceiver's asks satisfy the C5 disjunction with less concentrated support, and no
choice of `theta`/`phi` recovers it. Only 22% is prior misspecification.

### The sharpest form of the damage: cell-local miscalibration

The same events, bucketed by the two statistics and reported as **truth rate / v0.4's own mean
marginal** for the marked seats. 200 deals × 2 = 400 games per block, all three opponent seats
deceptive, seed 777, every third event (`research/v05/runs/P3-prior-calib.txt`). Cell counts in
thousands of (card, seat) observations.

`a` = asks by that seat in this half-suit, `b` = its asks elsewhere.

| opponents | a=0, b:0-3 | a=1, b:0-3 | a=2, b:0-3 | a=3+, b:0-3 |
|---|---|---|---|---|
| `v04` (honest) | 0.2075 / 0.2108 (1544k) | 0.4408 / 0.3880 (45k) | 0.5498 / 0.4773 (28k) | 0.7990 / 0.7227 (48k) |
| `silent:tol=0.10` | 0.2136 / 0.2138 (1612k) | 0.3782 / 0.3734 (63k) | 0.4468 / 0.4568 (31k) | 0.6997 / 0.7275 (36k) |
| `feint` | 0.2146 / 0.2098 (1531k) | **0.2815 / 0.3776** (69k) | **0.3223 / 0.4747** (25k) | **0.5559 / 0.6713** (23k) |
| `withholder:k=6` | 0.2020 / 0.2070 (1589k) | **0.4367 / 0.3464** (86k) | 0.4571 / 0.4375 (45k) | 0.6369 / 0.6079 (40k) |

Against honest opponents v0.4 is well calibrated in every cell (largest gap 0.07, and conservative).
Against the Feint it is **over-confident by +0.096, +0.152, +0.115** in exactly the cells the
manufactured certificate targets: a seat that has asked once in a half-suit really holds a given
unresolved card of it 28% of the time, and v0.4 believes 38%. Against the Withholder it is
**under-confident by −0.090** in the a=1 cell: the seat asks in half-suits it holds well, but only
after a delay, and v0.4 discounts it.

This is the answer to "how much damage": not a diffuse loss of accuracy, but a 0.09–0.15 signed
error concentrated in the one evidence cell each archetype manipulates. Note what it does **not**
buy: the Feint is exactly as strong as v0.4 against v0.3 (75.4% vs 75.0%) and still loses to v0.4
46.0–54.0 (§4). Corrupting the certificate cell by +0.10 did not convert into a single point of
win rate for the archetype that does it for free. Either the corrupted cell is not where v0.4's
decisions are made, or the ask v0.4 wastes on an over-attributed card costs it less than the ask
the Feint wastes manufacturing the certificate. Distinguishing those two is a v0.5 experiment, not
something this data settles.

Repeating the table with the measured team set to `v04:ptheta=0,pphi=0` shows where the error is
manufactured. Against the Feint the policy-agnostic posterior is over-confident by +0.088 in the
same a=1 cell (0.2816 truth / 0.3691 model) — i.e. **essentially all of the Feint's damage comes
from the hard C5 certificate, not from the soft prior**; the prior contributes under 0.01 of the
0.096. In the honest a=1 cell the prior earns its keep: full v0.4 is off by −0.053, the
policy-agnostic posterior by −0.084.

Caveat of record: the arms are not matched games. A deceptive opponent changes the trajectory, so
these are population means over different game populations, not paired differences on identical
positions.

---

## 3. (b) Do wrong declarations trace to the deceptive seat?

Same runs, replayed against the true deal (`attributeDeclarations`,
`engine/src/probe_deception_run.hpp`). For every wrong declaration by the measured v0.4 team, each
mis-assigned card is attributed to the seat that really held it at that moment. With one marked
opponent seat out of three, the null baseline is the honest control column.

| opponent at seat 1 | measured decls | wrong | mis-assigned cards at the **marked** seat | at another opponent | at a teammate | wrong decls touching the marked seat |
|---|---|---|---|---|---|---|
| `v04` (control) | 3787 | 522 (13.78%) | **25.3%** | 52.8% | 21.9% | 50.4% |
| `silent` | 3955 | 441 (11.15%) | 27.2% | 50.4% | 22.4% | 48.3% |
| `silent:tol=0.10` | 3799 | 504 (13.27%) | 23.0% | 54.6% | 22.4% | 43.3% |
| `feint` | 3739 | 552 (14.76%) | 27.2% | 52.3% | 20.5% | 43.5% |
| `withholder:k=6` | 3944 | 456 (11.56%) | 27.3% | 48.0% | 24.7% | 49.8% |

**The hypothesis does not hold.** Wrong declarations do not concentrate on the deceptive seat
beyond a ≤ 2-point shift, and the *rate* of wrong declarations against a deceiver is flat or lower
than against an honest opponent (11.2–14.8% vs 13.8%). v0.4's declaration errors are not caused by
being lied to; P0 already located their cause — the forcing horizon, where 58.6% of late
declarations and 100% of forced-endgame declarations are wrong.

---

## 4. (c) Win rates, and the policy-agnostic ablation

400 deals × 2 orientations = 800 games per cell, seed 20260822, cluster-bootstrap 95% intervals
over deals (`fish match --json`, `arena.hpp:clusterBootstrap`). Row = the *measured* policy
(three seats), column = opponent (three seats).

| A \ B | `v04` | `silent` | `silent:tol=0.10` | `feint` | `withholder:k=6` | `v03` |
|---|---|---|---|---|---|---|
| `v04` | 50.00 | 80.25 [77.5,83.0] | 56.12 [52.6,59.6] | 54.00 [50.5,57.5] | 67.75 [64.5,71.0] | 75.00 [71.9,77.9] |
| `v04:ptheta=0,pphi=0` | 43.00 [39.8,46.2] | 76.38 [73.5,79.2] | 51.50 [48.1,54.9] | 47.62 [44.2,51.0] | 62.88 [59.6,66.1] | 69.00 [65.8,72.1] |
| `v04:ptheta=0` | 46.38 [43.1,49.8] | 78.75 [75.9,81.5] | 53.37 [49.8,57.0] | 56.88 [53.5,60.2] † | 64.75 [61.6,67.9] | 76.00 [73.0,79.0] |
| `v04:pphi=0` | 49.00 [45.8,52.2] | 75.75 [72.8,78.6] | 58.38 [54.9,61.9] † | 56.12 [52.6,59.5] † | 68.12 [64.9,71.2] | 75.25 [72.2,78.2] |

† did not replicate in the independent paired run below; not claimed.

Differences from the shipped configuration, in points of win rate:

| ablation | vs `v04` | vs `silent` | vs `silent:tol` | vs `feint` | vs `withholder` | vs `v03` |
|---|---|---|---|---|---|---|
| `ptheta=0,pphi=0` | **−7.0** | −3.9 | −4.6 | −6.4 | −4.9 | −6.0 |
| `ptheta=0` | −3.6 | −1.5 | −2.8 | +2.9 † | −3.0 | +1.0 |
| `pphi=0` | −1.0 | −4.5 | +2.3 † | +2.1 † | +0.4 | +0.3 |

A second, independent and **paired** run settles which of those differences are real. `fish ablate`
plays every variant on the same deals against the same four-opponent panel and bootstraps the
matched difference over deals (250 deals × 4 opponents × 2 orientations = 2000 games per arm,
seed 515151, `research/v05/runs/P3-ablate.json`). Positive = worse than shipped v0.4.

| variant | pooled win rate | loss vs shipped v0.4 | 95% CI | per-opponent (v04 / silent:tol / feint / withholder) |
|---|---|---|---|---|
| `v04` (reference) | 56.75% | — | — | 50.0 / 58.2 / 51.4 / 67.4 |
| `ptheta=0,pphi=0` | 52.35% | **+4.40** | [1.60, 7.15] | 43.6 / 55.8 / 47.0 / 63.0 |
| `ptheta=0` | 55.80% | +0.95 | [−1.75, 3.70] | 49.2 / 56.0 / 50.6 / 67.4 |
| `pphi=0` | 57.05% | −0.30 | [−3.05, 2.40] | 52.0 / 53.4 / 54.8 / 68.0 |
| `ptheta=0.5276,pphi=0.2656` (2×) | 51.50% | **+5.25** | [2.45, 8.05] | 47.8 / 49.0 / 50.8 / 58.4 |

Readings:

* **The policy-agnostic posterior is not more robust.** It is uniformly worse — unpaired against
  every opponent, and paired by 4.40 points [1.60, 7.15]. The brief's candidate headline is refused
  by the data. The prior's *value* does shrink against deceivers (7.0 points in the mirror, 3.9–4.9
  against Silent and Withholder), which is misspecification — but a shrunken positive is not a
  liability.
* **Doubling the prior is as costly as deleting it** (+5.25 [2.45, 8.05]). The shipped values sit
  near a broad optimum whose dangerous side is *up*, not down (§5).
* **`priorPhi` is free to delete**: paired −0.30 [−3.05, 2.40], unpaired 49.00% [45.8, 52.2] in the
  mirror. Retraction of record: the first (unpaired, seed 20260822) run showed `pphi=0` gaining
  +2.3 against capped Silent and +2.1 against Feint; the paired run at seed 515151 reproduces the
  Feint gain (+3.4) but reverses the Silent one (−4.8). Neither per-opponent effect replicates, so
  the only supportable claim is the null: deleting `phi` changes nothing.
* **`priorTheta` is the load-bearing half** (−3.6 in the mirror unpaired, +0.95 [−1.75, 3.70]
  paired over the panel). Second retraction of record: the unpaired run showed `ptheta=0` *gaining*
  2.9 points against the Feint, which is what theory predicts; the paired run gives −0.8 in the
  same cell. Not replicated, so it is not claimed. The Feint's damage is real and localised (§2),
  but the fix is not simply turning `theta` off — because §2 shows the over-attribution comes from
  the hard certificate, which `theta` only slightly amplifies.
* Worst case across styles for shipped v0.4 is `feint` at 54.00% [50.5, 57.5] — no archetype beats
  v0.4 outright, but `feint` costs nothing to run (§1) and takes v0.4 from a 75% score against v0.3
  down to a near-even game.

---

## 5. The misspecification cliff

300 deals × 2 = 600 games per cell, seed 20260823, cluster-bootstrap intervals. The shipped pair is
`(theta, phi) = (0.26380, 0.13280)`; `s` scales both.

| s | ptheta | pphi | vs `v04` | vs `silent:tol=0.10` | vs `withholder:k=6` |
|---|---|---|---|---|---|
| 0 | 0 | 0 | 41.00 [37.2,44.8] | 49.33 [45.3,53.3] | 60.67 [56.5,64.7] |
| 0.25 | 0.0660 | 0.0332 | 45.83 [42.2,49.5] | 49.33 [45.2,53.5] | 61.50 [57.5,65.5] |
| 0.5 | 0.1319 | 0.0664 | 46.67 [42.7,50.7] | 57.67 [53.8,61.5] | 66.83 [63.0,70.5] |
| **1 (shipped)** | 0.2638 | 0.1328 | 50.00 | 57.50 [53.8,61.2] | 69.00 [65.3,72.7] |
| 2 | 0.5276 | 0.2656 | 44.33 [40.7,48.0] | 55.83 [52.0,59.7] | 61.83 [58.0,65.8] |
| 4 | 1.0552 | 0.5312 | 41.50 [37.7,45.5] | 45.17 [41.2,49.2] | **45.67 [41.5,49.8]** |

**The cliff is one-sided and it is steep on the over-trusting side.** Halving the prior costs
3.3 points in the mirror and 2.2 against the Withholder; doubling it costs 5.7 and 7.2; quadrupling
it costs 8.5 in the mirror and **23.3** against the Withholder — enough that v0.4 loses outright to
an archetype that is itself 17 points weaker against v0.3 (§1). That last cell is the exploitability
statement this task was looking for: an over-weighted policy prior is a lever a deceptive opponent
can pull, even though the shipped weight is not.

Separating the two scalars against the Withholder (same protocol):

| ptheta (phi = 0.1328) | win rate | | pphi (theta = 0.2638) | win rate |
|---|---|---|---|---|
| 0 | 65.33 [61.5,69.2] | | 0 | 65.50 [61.7,69.3] |
| 0.0659 | 66.17 [62.3,70.0] | | 0.0332 | 63.67 [60.0,67.3] |
| 0.1319 | 67.67 [63.7,71.5] | | 0.0664 | 66.17 [62.5,69.8] |
| 0.2638 (shipped) | 69.00 [65.3,72.7] | | 0.1328 (shipped) | 69.00 [65.3,72.7] |
| 0.5276 | 62.67 [58.5,66.7] | | 0.2656 | 64.50 [60.7,68.3] |
| 1.0 | **51.67 [47.5,55.8]** | | 0.5 | 64.33 [60.5,68.2] |

The whole cliff is in `theta`. The `phi` column is flat inside its intervals over a 15-fold range —
a third independent demonstration that v0.4's silence statistic carries no information, alongside
the `pphi=0` win rate (§4) and the unchanged silence cells of the posterior (§2).

---

## 6. Proposal: what an online per-seat opponent model needs

Design only; nothing below is implemented.

### 6.1 What the public record actually affords

Already materialised in `Knowledge` / `PublicState` and free to read:

| statistic | where | used by v0.4 |
|---|---|---|
| `askCount[p][S]` — asks by p in half-suit S | `belief.hpp:54` | yes, as `theta` |
| `totalAsks[p]` | `belief.hpp:56` | yes, as `phi` (via asks-elsewhere) |
| `missCount[p][S]` — times p was asked in S and did not have it | `belief.hpp:55` | maintained in `onEvent` (`belief.hpp:181`) and read by **nothing** in v0.4 |
| hand counts over time | `Event::handCount`, `fish.hpp:103` | only through capacity (C4) |
| turn possession and turn transfers | `PublicState::history` | no |
| who p asked (target identity), and whether it hit | history | only through C3/C5 |
| declarations made, their timing and correctness | history | no |
| pass-target choice when cardless | `Agent::choosePassTarget` | no |

Two statistics that are cheap to record and are **not** derivable from what v0.4 keeps:

* **asked-and-did-not-reply**: p was the target of an ask in S, and has since taken ≥ 1 turn without
  asking in S. One `uint8` per (seat, half-suit). This is precisely the owner's manoeuvre.
* **time since p's last ask in S**, in p's own turns. `phi` uses thewhole-game count of asks elsewhere,
  which is a poor proxy: it cannot distinguish "silent about S for three turns" from "silent all
  game", and the sweep in §5 shows the whole-game version carries no usable information.

### 6.2 Identifiability inside one game

Budget, measured (`fish pathology --games=60 --seed=31`, 120 games): 132.3 asks per game, median
game 104 events. That is **≈ 22 asks per seat per game (≈ 16 in the median game)**, spread over
9 half-suits — **1.8 to 2.4 asks per (seat, half-suit) cell**. The evidence table in §2 shows the
consequence directly: across the honest-opponent block, the a=0 row holds 3.51 M of the 3.84 M
card-observations (91%), and the individual a ≥ 1 cells are one to two orders of magnitude smaller
(12k–48k each).

A logistic tilt needs on the order of ten informative events per parameter before its standard
error is smaller than the effects at stake (differences of ~0.05 in a probability near 0.3). So:

* **per (seat, half-suit) parameters: not identifiable.** Most cells never receive an observation.
* **per-seat parameters: 1–2 at most, and only late in a game**, attached to statistics that have
  per-seat support (`askCount`, asked-and-did-not-reply).
* **pooled parameters: 5–10 are comfortably identifiable** across the ~120 asks of a whole table,
  and thousands across a training corpus.

The recommendation is therefore a **hierarchical tilt**, not a per-seat fit:

  w(c, p) = exp( x(c, p) · (β + δ_p) ),  δ_p ~ N(0, τ²)

with β and τ fitted offline over a panel of opponents, and δ_p updated online by a shrunken
one-step estimator that can only move materially for a seat that has acted 15+ times. This is
exactly what v0.4 has (`priorWeight`) with δ_p ≡ 0 and β = (0.2638, −0.1328) fixed by an offline
self-play fit against a mirror opponent — the misspecification is not that the form is wrong, it is
that there is no δ and that β was fitted against one opponent type.

**How wrong β is, measured.** Empirical log-odds coefficients from the §2 table (the tilt that would
reproduce the raw conditional):

| opponents | theta from a=0→1 | theta per ask, a=0→3+ | phi per ask elsewhere |
|---|---|---|---|
| `v04` (honest) | 1.102 | 0.907 | −0.045 |
| `silent:tol=0.10` | 0.806 | 0.716 | −0.057 |
| `feint` | **0.360** | 0.507 | −0.026 |
| `withholder:k=6` | 1.119 | 0.645 | −0.065 |
| **v0.4's fixed value** | **0.264** | **0.264** | **−0.133** |

Three conclusions. (i) The identified certificate coefficient varies by a **factor of three** across
opponent types (0.36 to 1.12) — that is the misspecification, quantified in the natural parameter,
and it is what a per-seat δ would have to track. (ii) v0.4's `theta` is far *below* every empirical
value, and yet §5 shows raising it makes things worse: the raw conditional is not the right target
because C5 already encodes the same evidence as a hard disjunction, so fitting the tilt to the raw
conditional double-counts. **The fit must be on the residual after C1–C5** — which is what the
truth-vs-marginal table in §2 measures, and which says the shipped `theta` is roughly right for
honest play. (iii) `phi`'s empirical coefficient is −0.03 to −0.07 while v0.4 uses −0.133: it is
the one coefficient that is over-weighted, and it is the one with no measurable effect either way,
because the capacity constraint (C4) already explains most of what "has taken many turns" predicts.

### 6.3 Keeping a deceptive opponent from driving the model

1. **Tilt only, never a constraint.** The model must reweight deals that satisfy C1–C5, never
   exclude one. v0.4 already has this property and §2 is the payoff: no archetype raised the
   "confidently wrong" rate above 4.5%, because a deceiver can withhold evidence but cannot
   manufacture a contradiction. Any online model that can *eliminate* a deal is a new attack
   surface and should be rejected on that ground alone.
2. **Bound the tilt, and calibrate the bound against the measured cliff.** Keep the ±2.6 clip
   (`belief.hpp:106`) and cap `‖δ_p‖` by a KL budget against the pooled β. §5 gives the empirical
   safe radius: at 2× the shipped scale v0.4 loses 5.3 points [2.5, 8.1]; at 4× it loses the match
   to an archetype 17 points weaker than itself. A per-seat model that can wander to 4× is worse
   than no model.
3. **Weight each statistic by its measured cost to fake.** §1 prices the three manoeuvres:
   asking where you hold one card is free (Feint: 75.4% vs v0.3, identical to v0.4's 75.0%),
   capped silence costs 5.2 points, withholding costs 17.2. A statistic that is free to fake must
   receive near-zero weight; one that costs 17 points can be trusted in proportion. This is the
   costly-signalling screen from `research/v04/lit/signalling.md` §2.4 made operational, and the
   engine can price any candidate statistic the same way in an afternoon.
4. **Model a type, not a coefficient.** Maintain P(seat p is deceptive) and use the mixture
   posterior rather than a point estimate of δ_p. The worst case is then bounded by the prior mass
   on the deceptive type, and a committed deceiver pays the §1 material price to move a bounded
   quantity. A Γ-minimax variant (act against the least favourable δ in the credible set) is the
   robust-Bayes version of the same idea and needs no new statistics.
5. **Do not let the tilt unlock a declaration.** Declarations are the irreversible action, and §3
   shows their errors are *not* deception-driven today. Gate `proposeDeclaration` /
   `willingForced` on the policy-agnostic posterior (θ = φ = 0) and use the tilt only for ask
   selection. Supporting evidence: the policy-agnostic configuration's "confidently wrong" rate is
   lower than shipped v0.4's in every arm (2.89–3.78% vs 3.46–4.53%) — the tilt buys ask quality by
   accepting more over-confident marginals, which is a good trade for asking and a bad one for
   declaring.
6. **Decay the statistics.** The manoeuvre is time-local: the Withholder is silent for six of its
   own asks, not forever. Replace "asks elsewhere all game" with "own turns since last ask in S",
   which is the statistic a human actually reads and the one v0.4 lacks.
7. **Validate against a best-responder, not against these three.** Limitation of record: the
   archetypes here are *commitment* strategies — they do not observe v0.4's beliefs and do not
   best-respond. A per-seat online model creates a data-poisoning surface that only an adaptive
   opponent can probe, and nothing in this report measures it. The minimum acceptable validation
   for an online model in v0.5 is an opponent that is allowed to condition its asks on a simulated
   copy of the model it is trying to move.

---

## Appendix — files and commands

New files, none of them touching a protected header:

* `engine/src/probe_deception.hpp` — the three archetypes and the material-cost counters.
* `engine/src/probe_deception_run.hpp` — the `deceit` measurement harness (belief error against
  ground truth, declaration attribution, prior-statistic calibration tables).
* `engine/src/factory.hpp` — one appended `if` block registering `silent`, `feint`, `withholder`
  (options `tol=`, `k=`, plus the usual v0.4 knobs). No existing branch altered.
* `engine/src/main.cpp` — one appended `if (cmd == "deceit")` block. No existing block altered.

Raw output: `research/v05/runs/P3-belief.txt` (belief error and declaration attribution, 20 arms),
`P3-winrate.txt` (win-rate matrix, 29 matches), `P3-sweep.txt` (28 matches), `P3-ablate.json`
(paired ablation), `P3-prior-calib.txt` (evidence-cell calibration).

The evidence-cell table of §2/§6 was added to the harness after the first four batches and its runs
were produced by a separately-linked binary `engine/fish_p3` (same sources, same flags, built while
another agent held `./fish`). A current `make` puts the same table in `./fish`; the addition is
observer-side bookkeeping and touches no policy code.

Commands (from `engine/`):

```
./fish deceit --games=400 --m=v04 --d=withholder:k=6 --dseats=1 --seed=101 --stride=2
./fish match  --a=v04:pphi=0 --b=feint --games=400 --seed=20260822 --json
./fish ablate --ref=v04 --variants="v04:ptheta=0,pphi=0;v04:ptheta=0;v04:pphi=0" \
              --panel="v04,silent:tol=0.10,feint,withholder:k=6" --games=250 --seed=515151
./fish    deceit --games=200 --d=feint --dseats=1,3,5 --seed=777 --stride=3   # evidence-cell table
```
