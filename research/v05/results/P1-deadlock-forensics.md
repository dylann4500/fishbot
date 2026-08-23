# P1 — Deadlock and dead-ask forensics

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.

All numbers come from a new diagnostic command, `fish deadlock`
(`engine/src/probe_deadlock.hpp`, registered in `engine/src/main.cpp`).
Reproduce with, from `engine/`:

```
make
./fish deadlock --games=60 --dump=5 --states=3 --stride=40      # parts 1-3 + causal isolation
./fish deadlock --games=1  --dump=0 --states=1 --h2h=1000       # the head-to-head in §3.4
```

Raw output: `research/v05/results/P1-deadlock-forensics-raw.txt`.
Population: 60 deals × 2 orientations = 120 v0.4-mirror games, seed 31 — the same
generator as `fish pathology`, so the figures line up with `P0-v04-pathology.md`
(events/game 140.6 here vs 143.6 over 600 games).

Every posterior in this report is the **exact block DP** (`BlockDP`,
`engine/src/blockdp.hpp`) — `teamOwnsProbability` and `bestTeamAllocation` — not
the `Fast`/Sinkhorn belief that the shipped policy uses for asking. Every
hypothetical ask is applied through the production certificate machinery
(`Knowledge::onEvent`, `engine/src/belief.hpp:153-190`), so what is measured is
exactly what a real ask would publish: C5 (the asker holds another card of the
half-suit), C3a (the asker lacks the asked card), C3b (on a miss, the target
lacks it).

---

## Headline

1. The mirror deadlock is a **deterministic two-question cycle**, not a
   half-suit freeze. 12 of the 14 long games contain a dead run consisting of
   exactly **two** distinct `(actor, card, target)` triples repeated ~130 times
   each.
2. At the deadlock states the ask v0.4 plays is **information-free**: it moves no
   teammate's exact posterior at all, in **42 / 42** states measured. The public
   information state is a fixed point, so a deterministic policy re-plays it
   forever.
3. **The E11 claim is false.** In a deadlocked state further information *is*
   obtainable: 70.6% of legal asks inside a half-suit the team provably owns
   strictly increase a teammate's exact P(correct MAP allocation), by a mean of
   **+0.126** and up to **+0.658**. A greedy ladder of such asks took the team's
   best allocation probability from **0.220 to 1.000 in a median of 2 asks, in
   21 / 21 attempts.**
4. The cost is small and, inside the deadlock, essentially zero: a donated turn
   is worth 0.128 half-suits in v0.4's own value function, yields the opponents
   1.13 cards / 0.157 half-suits in ordinary play, and **0.006 cards / 0.002
   half-suits inside a dead run** (99.6% of such donations yield the opponents
   nothing at all).
5. But breaking the cycle *without* pricing information is a **loss**: v0.4 with
   exact repetition simply forbidden terminates every game (events/game
   140.6 → 101.1, longest dead run 284 → 4) yet loses to shipped v0.4
   **42.2%** over 2000 games. The information term is load-bearing, not
   optional.

---

## 1. What the deadlock actually is

### 1.1 Census over the long games

`scanned 120 games (v04 mirror, seed 31)`; 14 exceed 300 events.

| quantity | value |
|---|---|
| mean longest dead run in a long game | 245.1 asks |
| mean distinct `(actor, card, target)` triples inside that run | **2.14** |
| runs that are a pure two-question cycle | **12 / 14** |
| runs where any half-suit was already locked to a team (ground truth) at the run's start or end | **5 / 14** |

The second and third rows are the load-bearing ones. The deadlock is a
*policy cycle*: two seats alternate the same two questions until the event-count
forcing horizon fires (`V04Config::forceDeclareEvents = 220`, `v04.hpp:99`; hard
cash at 308, `v04.hpp:583`). And in **9 of 14** long games *no half-suit is
locked to anybody* when the run begins or ends — so the "frozen locked half-suit"
story in `E11-termination.md` cannot be the mechanism for the majority of cases.

### 1.2 Five concrete traces

| # | seed / rot | events | dead run | starts at | the entire run | locked half-suits at onset |
|---|---|---|---|---|---|---|
| A | 272135269103994248 / 1 | 317 | 284 | ev 24 | s0 asks 9H of s1 ×142; s1 asks BJ of s0 ×142 | 0 (9 live, all split) |
| B | 17383714354061619071 / 1 | 315 | 264 | ev 44 | s1 asks 5D of s4 ×132; s4 asks 9S of s1 ×132 | 0 (7 live, all split) |
| C | 15182899564772791544 / 1 | 314 | 264 | ev 44 | s0 asks 6S of s5 ×132; s5 asks 5D of s0 ×132 | 0 (6 live, all split) |
| D | 16025379504301535457 / 0 | 323 | 263 | ev 45 | s2 asks 8D of s1 ×132; s1 asks JC of s2 ×131 | **4** (2 to each team), 5 split |
| E | 8064089431629203792 / 0 | 315 | 262 | ev 46 | s1 asks 10H of s4 ×131; s4 asks 2C of s1 ×131 | 0 (7 live, all split) |

Trace B at the onset (event 44, seat 1 to move, score 0–2):

```
  hand counts s0=7 s1=6 s2=9 s3=8 s4=2 s5=10
  live half-suits 7  locked-to-team0 0  locked-to-team1 0  genuinely split 7
  unresolved cards per observer: s0=27 s1=31 s2=25 s3=25 s4=31 s5=25
  exact mean per-card certainty per observer: 0.409702 0.338521 0.432402 0.430814 0.344292 0.420173
  provably-dead legal asks 7 of 36;  best rank of an ask that is NOT provably dead: 4
     #1  ask 2D (set 4) from s4   p(hit)=0.0000   u=5.6118 (lin 4.1685, ev 1.4432)
     #4  ask 2D (set 4) from s2   p(hit)=0.2542   u=3.8639 (lin 2.3490, ev 1.5150)
     WHY: #1 (2D@s4, dead) vs best live #4 (2D@s2, p=0.2542)   linear gap 1.8196, EV gap -0.0718
       top contributions to the gap:  entropy +1.6637  exposureOnMiss +1.3323  targetHand +1.2057
       worst:  p(hit) -2.2421  p^2 -0.1632  runway -0.0918
```

Trace D at the onset is the interesting one for §2, because four half-suits *are*
locked and **no owner knows it**:

```
  live half-suits 9  locked-to-team0 2  locked-to-team1 2  genuinely split 5
  LOCKED set 5 (High Diamonds) to team 0:  truth 9D@s0 10D@s0 JD@s0 QD@s0 KD@s2 AD@s4
      observer s0  pTeam 0.166369  pAlloc(MAP) 0.047956  MAP-is-truth yes
      observer s2  pTeam 0.009110  pAlloc(MAP) 0.000339  MAP-is-truth NO
      observer s4  pTeam 0.004797  pAlloc(MAP) 0.000253  MAP-is-truth NO
  LOCKED set 4 (Low Diamonds) to team 1:  truth 2D@s5 3D@s5 4D@s3 5D@s5 6D@s5 7D@s5
      observer s1  pTeam 0.059421  pAlloc(MAP) 0.009319  MAP-is-truth NO
      observer s3  pTeam 0.042415  pAlloc(MAP) 0.042415  MAP-is-truth yes
      observer s5  pTeam 0.341881  pAlloc(MAP) 0.341881  MAP-is-truth yes
```

### 1.3 Why the top-scoring ask is a guaranteed miss

At every dumped state the ask v0.4 ranks first has `p(hit) = 0` under its own
belief and is provably dead under its own `Knowledge`. Decomposing the linear
score gap between #1 and the best not-provably-dead ask gives the same three
features every time (weights from `V04Config::w`, `v04.hpp:70-92`):

| feature | code | weight | why it favours a dead ask |
|---|---|---|---|
| `f[14] = binEnt(p)` "location entropy" | `v04.hpp:323` | −2.6534 | a **certain** miss has zero entropy, so this term *pays a bonus for asking a card whose location you already know* |
| `f[10] = handCount[target]/9` "target hand size" | `v04.hpp:319` | −2.0219 | a nearly-empty opponent is cheap to ask, regardless of `p` |
| `f[16] = (1−p)·exposureOf(target)` | `v04.hpp:325` | +1.9040 | scales with `(1−p)`, so it is **maximised exactly when the ask is certain to miss** |
| `f[8] = (1−p)·threatOf(target)` | `v04.hpp:317` | −3.0978 | the only counterweight; it collapses when the target has 2–4 cards |

Trace B, gap contributions: `entropy +1.6637, exposureOnMiss +1.3323,
targetHand +1.2057` against `p(hit) −2.2421`. Net linear gap **+1.8196** for the
dead ask; the one-ply value term only claws back −0.0718. Three of v0.4's twenty
ask features are functions of `(1−p)` or of `H(p)`, and in a low-`p` position
they outvote the hit-probability term.

This is a **policy defect, not an inference defect** — the belief correctly
reports `p = 0`; the score prefers the move anyway. That matches the P0 finding
that switching to `belief=block` cuts dead asks 39% → 28% but leaves the longest
run at 280.

### 1.4 The state is a fixed point

Across the three sampled states of each dumped run (onset, +40, +80 events), the
**exact mean per-card certainty of every observer is byte-identical**:

```
trace B  event  44 : 0.409702 0.338521 0.432402 0.430814 0.344292 0.420173
trace B  event  84 : 0.409702 0.338521 0.432402 0.430814 0.344292 0.420173
trace B  event 124 : 0.409702 0.338521 0.432402 0.430814 0.344292 0.420173
```

so are the hand counts, the unresolved counts and the score. The only thing that
changes is a counter: the number of live ask-legality certificates each observer
stores grows without bound (trace A: `s0 = 6 → 26 → 46`) because
`Knowledge::onEvent` appends a fresh `Disjunction` on every repeated ask without
checking whether an identical one is already held (`belief.hpp:168-171`). Those
duplicates carry no information — the exact posterior is unchanged — but they do
make the block DP progressively more expensive. Minor engine finding, worth
de-duplicating in v0.5.

Because the information state is a fixed point and both policies are
deterministic, each seat re-derives the same argmax and the cycle is closed. This
is precisely the user's report: *"repeatedly asking the same bot the same
question."*

---

## 2. Is further information obtainable in a deadlocked state? **Yes.**

`E11-termination.md` states:

> "Theorem 1 … implies that such a half-suit is frozen — and, for the same
> reason, that no further information about its allocation can ever arrive."

**This is false, and the numbers below say so.** (`docs/FISHBOT_V04.md:97-101`
already flags the hole; this quantifies it.) Theorem 1 constrains *ownership*
monotonicity; it says nothing about the C5/C3 certificates that the owning team's
own asks keep publishing.

### 2.1 What is actually informative, and what is not

An ask by A of card `c` in half-suit `S` from opponent `t` publishes three facts.
In a half-suit **locked to A's team**:

| certificate | informative to A's teammates? |
|---|---|
| C3b — `t` lacks `c` (on the miss) | **No.** `t` is an opponent of a locked half-suit; its posterior mass on `c` is already exactly zero. Vacuous by construction. |
| C3a — **A lacks `c`** | **Yes, decisively.** With six cards split among three teammates, removing A from `c`'s candidate set typically resolves `c` outright to the third teammate. This is the whole of the effect. |
| C5 — A holds another card of `S` | Yes when A has not already publicly shown a card of `S`; usually already discharged (`vacuous` test, `belief.hpp:159-166`), which is why repeats are worthless. |

So only the **asker's own exclusion** is load-bearing. That is what the numbers
below measure.

### 2.2 Distribution of ΔP(correct MAP allocation)

63 (seat × state) pairs that possess a live half-suit their team provably owns,
2 979 legal asks enumerated. For each ask, for each **teammate** of the asker,
the exact posterior is rebuilt after the certificate and `bestTeamAllocation` is
re-queried on the team's best candidate half-suit; `dP` is the best improvement
over the asker's teammates.

| ask class | n | with `dP > 0` | mean `dP` | p50 | p90 | max |
|---|---:|---:|---:|---:|---:|---:|
| **inside our own locked half-suit** | 918 | **648 (70.6%)** | **+0.1261** | +0.0347 | +0.5876 | **+0.6581** |
| any other legal ask | 2 061 | 550 (26.7%) | +0.0014 | 0.0000 | +0.0037 | +0.0397 |

Direction, checked against ground truth: the teammate's MAP allocation flipped
**to** the truth 258 times and **away** from it 18 times (14.3 : 1).

Team-best `P(MAP allocation correct)` on the candidate half-suit rose from a
mean of **0.2197** to **0.3961** from a *single* ask.

### 2.3 The certificate ladder: a locked half-suit resolves in ~2 asks

Greedily playing certificate-only asks inside one locked half-suit, applying each
public certificate to all six observers:

```
ladders run 21   mean team-best pAlloc 0.219670 -> 1.000000
reached pAlloc > 0.9995 (fully resolved): 21 / 21 (100%)
mean asks needed 2.29   median 2
```

Worked example (trace D, event 45):

```
certificate ladder in set 5 for team 0: pAlloc 0.0480 -> 0.3919 -> 1.0000   (2 asks)
certificate ladder in set 4 for team 1: pAlloc 0.3419 -> 1.0000              (1 ask)
```

Two legal, guaranteed-miss questions take High Diamonds from
"4.8% confident" to **certain**. v0.4 instead played `8D` at seat 1 for the 132nd
time.

### 2.4 The leak (the wiretap side)

The certificates are public, so the opponents read them too:

| metric | before | after the whole ladder | Δ |
|---|---:|---:|---:|
| opponents' mean per-card certainty | 0.3936 | 0.4245 | **+0.0309** |
| opponents' own best locked-half-suit `pAlloc` (n = 12) | 0.2168 | 0.2375 | **+0.0208** |

The team gains **+0.780** of allocation probability on the half-suit it is trying
to cash; the opponents gain **+0.021** on theirs. The exchange rate is about
**37 : 1 in the sender's favour**. The mechanism of the leak is capacity
coupling, not the half-suit itself: resolving a card to a specific teammate
lowers that seat's spare capacity and sharpens everyone's counting elsewhere
(`Knowledge::propagateCapacity`, `belief.hpp:214-227`). The opponents can never
take a card of a locked half-suit (Theorem 1 holds), so the leak is purely
indirect.

### 2.5 v0.4 never chooses these asks, and it is not close

At the mover's decision point in the 42 deadlock states examined:

| statement | value |
|---|---|
| the ask v0.4 actually played moved **no** teammate's exact posterior | **42 / 42 (100%)** |
| at least one legal ask **would** have moved it | **42 / 42 (100%)** |
| mean Δ(teammate certainty): played | **0.000000** |
| mean Δ(teammate certainty): best available | **+0.0359** (p90 +0.042, max +0.059) |
| rank of the most informative legal ask in v0.4's own score | **#17.6 of ~40 on average**, a mean deficit of **2.375 score units** below the leader |

In trace A the most informative ask (`9D` at s5) ranks **#40 of 54**, 5.10 score
units behind. The ask rule is not "slightly mispriced" on information — it does
not contain the term at all.

Note the scope of "42/42": these are states already inside a repeat cycle. It is
not a claim that every v0.4 ask is information-free — P0 records 16.5% of all
asks landing inside a half-suit the actor's team already owns, which do emit
certificates. v0.4 makes information asks by accident, never on purpose.

---

## 3. What the donated turn costs

### 3.1 In v0.4's own value function

`V04Agent::value` with `turnSign` flipped from +1 to −1, evaluated at the 15
dumped deadlock states. The relevant coefficients are `vw[5] = 0.022896` (side to
move), `vw[9] = −0.000997` (turn × control) and `vw[15] = −0.021409`
(turn × unresolved) (`v04.hpp:111-128`).

| quantity | value |
|---|---|
| mean turn-donation cost | **0.128 half-suits** of final set differential |
| max over the sampled states | 0.188 |
| range across traces | 0.064 (A) … 0.188 (C) |

### 3.2 Empirically, as the opponents' take

Every miss in all 120 games, classified by whether it sits inside a dead run of
length ≥ 6. "Take" = cards the receiving team wins before its own first miss,
plus half-suits it declares in that window.

| donation context | n | cards taken | half-suits scored | yielded nothing |
|---|---:|---:|---:|---:|
| **inside a dead run** | 5 547 | **0.006** | **0.0023** | **99.6%** |
| everywhere else | 4 761 | 1.131 | 0.157 | 55.2% |

Inside the deadlock the turn is worthless to the receiver — it is stuck too. The
right conservative figure for a *general* position, and the one to price against,
is the second row: **0.157 half-suits per donated turn**.

### 3.3 The trade, in one line

A ladder of 2.29 certificate asks costs 2.29 donated turns.

* Cost, worst case (opponent not stalled): 2.29 × 0.157 = **0.36 half-suits**.
* Cost inside the observed deadlock: 2.29 × 0.0023 = **0.005 half-suits**.
* Gain: a half-suit whose expected declared value is `2·pAlloc − 1` goes from
  `2(0.2197) − 1 = −0.56` to `+1.00`, a swing of **1.56 differential units =
  0.78 half-suits** for the asking team.

Net **+0.42 half-suits even against a fully live opponent**, and effectively
+0.78 inside a stalled position. The leak of +0.021 to the opponents' own
allocation is a rounding error against that.

### 3.4 The honest counter-result: breaking the loop is not free

A control variant, `NoRepeatV04` (`probe_deadlock.hpp`), changes exactly one
thing: it never repeats an exact `(card, target)` question it has already asked,
falling back to its highest-scoring un-asked question. No information is priced.

| | shipped v0.4 | no-repeat v0.4 |
|---|---:|---:|
| events/game (same 120 deals) | 140.6 | **101.1** |
| provably-dead asks | 39.0% | **3.1%** |
| longest dead run | 284 | **4** |
| games over 300 events | 14 / 120 | **0 / 120** |

The cycle is *entirely* a repetition artefact. But head-to-head against shipped
v0.4 (1 000 deals × 2 orientations, seed 90210):

```
no-repeat v0.4 vs shipped v0.4: 2000 games, win rate 42.200%,
  mean sets 4.201 vs 4.799,  events/game 101.8
  misdeclaration rate: no-repeat 1.89% vs shipped 1.97%
```

**−7.8 win-rate points.** Fewer misdeclarations, faster games, and still a loss:
the substituted asks give away material. Two conclusions:

* the deadlock is caused by repetition, so any fix must forbid it — but
* forbidding it *blindly* is worse than the disease, so the replacement ask has
  to be chosen for what it publishes.

Caveat: the substitute is ranked by the pre-refinement linear + one-ply-EV score
(the shipped `searchTopK` chain/threat re-ranking is applied only to the primary
pick), so a small part of the −7.8 may be that simplification rather than the
tie-break itself.

---

## 4. The decision rule this implies

Write, for a candidate ask `a` by seat `A`:

* `p(a)` — posterior hit probability (already `f[0]`);
* `I(a) = max_{B ∈ team(A), B ≠ A} [ P_B(MAP alloc | a) − P_B(MAP alloc) ]` on the
  team's best **provably-owned** live half-suit — computable exactly by two
  `BlockDP` queries, and computable *by the asker* because the certificate is a
  public function of `(A, card, S)`, not of A's hand;
* `L(a)` — the same quantity for the opponents' best provably-owned half-suit;
* `T` — the turn-donation cost, **0.157 half-suits** against a live opponent,
  **0.002** in a stalled position (§3.2); v0.4's value function says 0.128 (§3.1),
  so 0.13–0.16 is the defensible band.

A declared half-suit is worth `2·pAlloc − 1` in set differential, so a gain of
`I` in allocation probability is worth `2I`. The ask rule becomes

```
U(a) = λ_lin·Σ w_k φ_k(a)  +  λ_val·EV(a)  +  λ_info·2·[ I(a) − κ·L(a) ]
```

with the material part unchanged. **The information ask beats the material ask
exactly when**

> `2·[ I(a) − κ·L(a) ]  >  [ p(a*) − p(a) ] · ( G + T )`
>
> where `a*` is the best material ask, `G` the material value of a hit (the card
> plus the retained turn) and `T` the turn-donation cost — because switching from
> `a*` to `a` forgoes a hit with probability `p(a*) − p(a)` and donates the turn
> that much more often.

which, in the states that matter, simplifies to four checkable conditions:

1. **The team provably owns a live half-suit it cannot allocate.**
   `P(team owns S) > 1 − ε` and `P(MAP allocation) < declThreshold` (0.7969 for
   locked half-suits, `v04.hpp:94`; 0.8177 otherwise, `v04.hpp:93`). Theorem 1 guarantees the half-suit cannot be
   stolen while this is resolved, so the *only* risk is the donated turn.
2. **The best material ask is itself a miss.** When `p(a*) ≈ 0` the turn is
   donated either way and the information ask is strictly dominant — the
   right-hand side is zero. This held in **42 / 42** deadlock states.
   Equivalently: whenever `p(a*) < askFloor` (0.3325, `v04.hpp:97`), prefer
   information.
3. **The certificate is not vacuous.** `I(a) > 0` requires that the exclusion
   "A lacks this card" is not already public — i.e. **never repeat an exact
   `(card, target)` pair**, and prefer a card of `S` that A has never asked. This
   single condition removes the cycle (§3.4) and, unlike the blind version, only
   fires when it buys something.
4. **The net beats the turn.** `I(a) − κ·L(a) > T/2 ≈ 0.079`. Measured own-locked
   asks have mean `I = 0.126` and `L ≈ 0.021` per ladder step, so the typical
   certificate ask clears this bar by roughly 1.5×, and the ladder as a whole
   clears it by 5×.

Two riders from the evidence:

* **Target selection is part of the information choice.** C3b is vacuous against
  an opponent already excluded from the half-suit, so in a locked half-suit the
  target is free — pick the one that minimises `L` (fewest cards, least capacity
  information handed over), not the one v0.4's `f[10]`/`f[8]` currently prefer.
* **Do not price `f[14] = binEnt(p)` and `f[16] = (1−p)·exposure` as information.**
  They are the terms that currently *reward* certain misses (§1.3) while
  delivering none of the actual certificate value. A real `I(a)` term should
  replace them, not sit beside them.

Untested: whether a policy implementing this actually gains against v0.4 and
against a human-like opponent. §3.4 establishes that the repetition guard alone
loses 7.8 points, so the `2·I(a)` term must carry more than that — which the
§2.2/§3.3 magnitudes (+0.78 half-suits per resolved lock, at 0.36 half-suits of
turn cost) say it plausibly can, but that is a prediction for P2/P3 to test, not
a measurement.
