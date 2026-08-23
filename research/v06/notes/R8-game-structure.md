# R8 — Game structure: where Canadian Fish is actually decided

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, engine at commit
`bd812fe` (working tree clean at start). **Recon only — no file under `engine/src/`, `paper/` or
`docs/` was modified.** All new code lives in the scratchpad
(`…/scratchpad/r8/r8.cpp`, built with `-I engine/src` against the unmodified headers) and is
listed under *Reproduction* at the end.

Everything below is either (a) a line of the engine, cited `file:line`, (b) a closed-form
derivation, or (c) a measurement produced in this session. Nothing is inferred from prose.

*Provenance note.* A sibling agent refactored `engine/src/factory.hpp` (option parsing moved into
`applyV05Opts`, plus a new `v06` branch) and added `engine/src/v06.hpp` at 04:52–04:58 while this
recon was running. The `./fish` binary used for every `match` result predates that change
(`make` reported *Nothing to be done*), and the scratch probe was rebuilt against the post-refactor
headers and re-verified afterwards: `log₁₀Z = 28.279` at event 0 and the perfect-information
identity (100/100 games) reproduce bit-for-bit. All line numbers cited below are for `v05.hpp`,
`v04.hpp`, `fish.hpp`, `game.hpp`, `belief.hpp`, `blockdp.hpp` and `oracle.hpp`, none of which
were touched.

---

## 0. TL;DR — the seven load-bearing facts

| # | Fact | Evidence |
|---|---|---|
| S1 | The hidden state is exactly the initial deal; the opening information set has **1.90 × 10²⁸ deals = 93.9 bits**, and the game reveals **≈ 0.97 bits per public event** (94 bits over 96.6 events). | measured `log₁₀Z = 28.279` at event 0; `= 0` by event ≈ 96 |
| S2 | **Under perfect information the team on turn wins every half-suit except those dealt outright to the opponents.** Verified 300/300 games; perfect-information value **8.87 – 0.13**. | `r8 omni`, §6.2 |
| S3 | Therefore **determinized search (PIMC) is degenerate in Fish**: every sampled deal returns "we take everything", so all actions score alike. A v0.6 "perfect endgame solver" must search the *belief* space, not sampled deals. | corollary of S2 |
| S4 | The deal decides *which* half-suits you win (71.1% go to the deal-majority team) but **almost none of the margin**: the deal-implied expected differential is −0.07 ± 0.08 sets against a realised sd of 2.98, and the full split profile explains **R² = 1.3%** of the final differential. | §3.1 |
| S5 | **Ask selection is worth ~4.1 half-suits; the declaration decision ~0.46.** The game is an asking game with a bookkeeping step at the end. | §3.2 |
| S6 | The turn is worth **≈ 0 at the opening** (+0.017 ± 0.118 sets, matched-deal experiment) and **0.83 sets late** — a renewable resource early, a decisive one after event 80. | §4 |
| S7 | Exhaustive enumeration of consistent deals costs 10 ms at **Q ≈ 13 unresolved cards** (median); p90-safe at **Q ≤ 12**, p99-safe at **Q ≤ 11**. But the exact posterior is *already* closed-form at **0.011 ms** at Q = 12 (`BlockDP`), so enumeration buys nothing. The real endgame budget is **≈ 900 exact belief rebuilds per decision**. | §6.1, §6.3 |

---

## 1. STATE SPACE

### 1.1 Why the deal is the whole hidden state

A card changes hands only through a successful ask, and every ask (hit or miss) is emitted as a
public `Event` observed by all six agents — `game.hpp:148-163` (`emit` pushes to `pub.history`
and calls `observe` on every seat). No private draw, no discard, no shuffle mid-hand. Hence a
card whose location has never been publicly revealed is still with the player dealt it. This is
stated in the engine's own header, `belief.hpp:1-30`, and the constraint set is:

- **C1** own hand (exact) — `belief.hpp:73-74`
- **C2** publicly transferred cards (exact owner) — `belief.hpp:172-179`
- **C3** exclusions: an ask proves the asker lacks the card (`belief.hpp:167`); a miss proves the
  target lacks it (`belief.hpp:182`)
- **C4** capacities: `handCount[p] − |known cards of p|` — `belief.hpp:83-90`, propagated to a
  fixed point at `belief.hpp:214-226`
- **C5** ask legality: an ask in half-suit *S* by *A* proves *A* held ≥ 1 other card of *S*,
  recorded as a time-independent disjunction — `belief.hpp:158-171`

`BlockDP::build` (`blockdp.hpp:157-296`) returns the exact partition function `Z` = the number
of deals consistent with C1–C5. It is validated against exhaustive enumeration by `fish oracle`
(`oracle.hpp`, driven from `main.cpp:327-384`).

### 1.2 Size at the opening — closed form and measured

The observer knows its own 9 cards; the remaining 45 split 9/9/9/9/9 among five seats:

```
Z0 = 45! / (9!)^5 = 1.8996 × 10^28      log10 = 28.279      log2 = 93.94 bits
```

Measured, at the very first ask decision of 400 games (`r8 state 400 90210`):
`ev 0-0 … meanLog10Z 28.279 medLog10Z 28.279` — exact agreement to three decimals.

### 1.3 Collapse rate

`log₁₀Z` for the seat on move, 35,085 ask decisions over 400 v0.5-mirror games (seed 90210):

| public events elapsed | 0 | 1–4 | 5–9 | 10–19 | 20–29 | 30–39 | 40–59 | 60–79 | 80–119 |
|---|---|---|---|---|---|---|---|---|---|
| mean log₁₀Z | 28.279 | 27.532 | 25.961 | 23.301 | 19.945 | 16.652 | 11.999 | 6.079 | 2.010 |
| mean unresolved cards | 45.0 | 44.5 | 43.1 | 39.8 | 35.4 | 30.8 | 24.0 | 14.6 | 6.4 |
| mean legal asks | 83.8 | 82.3 | 79.5 | 72.7 | 66.2 | 57.9 | 44.7 | 29.5 | 14.4 |

The decay is very close to linear in event count: 28.279 decades over a mean 96.58 events
(`fish match --a=v05 --b=v05 --games=100 --rotations=2 --seed=31` → `events/game 96.58`) =
**0.293 decades = 0.973 bits of hidden information destroyed per public event**, i.e. each public
event roughly halves the information set. There is no phase transition; the collapse is steady
and then terminal (the last ~20 events run from 10⁶ down to 1).

By unresolved-card count `Q` (the natural endgame axis), same run:

| Q | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 20 | 30 | 45 |
|---|---|---|---|---|---|---|---|---|---|---|
| median log₁₀Z | 0.954 | 1.505 | 2.477 | 3.467 | 4.462 | 5.838 | 6.898 | 9.495 | 16.136 | 27.886 |
| median Z | 9 | 32 | 300 | 2,930 | 29,000 | 6.9e5 | 7.9e6 | 3.1e9 | 1.4e16 | 7.7e27 |
| median public events left | 9 | 12 | 15 | 19 | 23 | 26 | 31 | 39 | 60 | 94 |
| P(Z = 1, i.e. perfect information) | 0.009 | 0.006 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

Two structural regularities worth exploiting:
- `Q = 1` **never occurs** — capacity propagation (`belief.hpp:214-226`) resolves the last card
  automatically. The state jumps 2 → 0.
- **remaining public events ≈ 2Q** (Q=8→15, Q=10→19, Q=12→23, Q=20→39). The game has a
  predictable, short horizon once Q is known.
- **2.85% of all ask decisions are made under perfect information** (Q = 0, Z = 1) — median 4
  public events from the end.

---

## 2. THE ACTION SET

### 2.1 Legal asks

`legalAsk` (`fish.hpp:158-165`) requires four things: the half-suit is active; the target is on
the other team; the target still holds cards; the actor does **not** hold the asked card; and the
actor **does** hold another card of that half-suit (`fish.hpp:164`). `enumerateAsks`
(`fish.hpp:179-196`) is the observer-side enumeration used by every policy.

Measured over 35,085 v0.5-mirror ask decisions (`r8 state 400 90210`):

```
mean 47.27   median 45   p05 6   p95 93   max 135   min 1
```

Analytic cross-check at the opening. A player holding 9 cards touches `k` half-suits and has
`6k − 9` askable cards × 3 live opponents:
`E[legal asks] = 3(6·E[k] − 9)`, `E[k] = 9(1 − C(48,9)/C(54,9)) = 6.1617` → **83.91**.
Measured at event 0: **83.75**. Maximum 135 is attained exactly when all 9 cards sit in 9
different half-suits (9 × 5 × 3).

Distribution by phase (mean legal asks): 83.8 → 79.5 → 66.2 → 44.7 → 29.5 → 14.4 (see §1.3
table). The action set shrinks roughly linearly with cards in play.

### 2.2 After M1 live-gating

M1 (`v05.hpp:107`, `v05.hpp:474-500`) removes asks the actor can *prove* dead —
`provablyDead(card,target)` is `k.owner[card] < NPLAY ? owner != target : !(mask[card] & bit(target))`
(`v05.hpp:474-477`), a hard deduction with no posterior in it.

```
legal   mean 47.27   median 45   p05 6   p95 93
live    mean 43.50   median 42   p05 5   p95 89
```

**M1 removes 8.0% of the candidate set on average**, but the removal is entirely back-loaded:

| events elapsed | 0 | 5–9 | 20–29 | 40–59 | 60–79 | 80–119 | 120+ |
|---|---|---|---|---|---|---|---|
| fraction of legal asks that are provably dead | 0.000 | 0.033 | 0.061 | 0.101 | 0.155 | 0.214 | 0.279 |
| fraction of decisions with **exactly one** live ask | 0 | 0 | 0 | 0 | 0.003 | 0.046 | 0.133 |

Note the asymmetry with the v0.4 pathology number in `research/v05/BRIEF.md` (39.0% of *chosen*
asks were provably dead): only 8% of the *candidate set* is dead, so v0.4 was actively selecting
into the dead 8%. With M1 on, v0.5 never does. But the head-to-head price of M1 at the shipped
weights is nil — `v05:m1=0` vs `v05` = **48.96%** [46.96, 50.96], 4.475 – 4.525 sets
(400 deals × 6 rotations, seed 606060). M1 is a pathology fix, not a strength fix.

### 2.3 The action set nobody counts: declarations

`declarationRound` (`game.hpp:202-231`) polls **every seat before every ask**, including seats
without the turn (`Rules::outOfTurnDeclare`, `fish.hpp:108`) and cardless seats
(`Rules::cardlessMayDeclare`, `fish.hpp:109`; gate at `game.hpp:216`). A declaration names an
owner for each of the six cards, constrained only to the declarer's own team
(`game.hpp:221`). So the per-seat declaration action space is

```
(active half-suits) × 3^6 = up to 9 × 729 = 6,561 named allocations, plus "pass"
```

which dwarfs the ~45 asks. v0.5 collapses it to one candidate per half-suit — the
capacity-feasible MAP allocation, enumerated exhaustively over ≤ 3⁶ = 729 assignments
(`feasibleAllocation`, `v05.hpp:615-619`). Measured: v0.5 makes 4.50 voluntary declarations/game,
of which **3.34/game are out of turn** (`fish match` line `out-of-turn 3.34 / 3.34 per game`).

---

## 3. WHERE THE GAME IS DECIDED

Method: identical deals, full 6-rotation duplicate blocks (`arena.hpp:80-95`), one component of
the policy replaced by a null at a time; 400 deals × 6 rotations = 2,400 games, seed 606060.
Reference is v0.5 mirror = 50.0%, 4.5 – 4.5.

### 3.1 (a) The deal — worth ≈ 0 in expectation

`r8 deal 250 777` (1,500 games, 13,500 half-suits):

| team-0 cards of the half-suit at the deal | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| P(team 0 wins that half-suit) | .074 | .182 | .333 | .497 | .654 | .833 | .894 |
| share of half-suits | 1.4% | 7.8% | 23.4% | 34.7% | 23.4% | 7.8% | 1.4% |

- The deal-majority team wins the half-suit **71.08%** of the time (n = 8,814 non-3–3 sets).
- **34.7% of all half-suits are dealt 3–3** and go 49.7 / 50.3 — pure play.
- But the map above is very nearly affine in the split, and each team is dealt exactly 27 cards,
  so **the deal-implied expected score is conserved**: the deal-implied expected differential
  Σ_s (2·P(win | split_s) − 1) has mean **−0.065** and **sd 0.078 half-suits** across 1,500 deals
  (i.e. an expected 4.467 – 4.533), against sd **2.978** for the realised differential — a
  38-fold gap.
- A ridge regression of the final differential on the whole split profile (counts n₀…n₆, 8
  parameters, 1,500 points) gives **in-sample R² = 0.0126**, residual sd 2.959 vs 2.978.

**Conclusion.** The deal determines *which* half-suits a team is favoured in, and essentially
nothing about *how many* it will win. Fish is not a card-luck game at the level of the margin;
it is card-luck at the level of the individual half-suit, and those cancel by conservation.

### 3.2 (b) and (c) — declaration vs ask decisions

| variant (team A) vs v0.5 | win rate | mean sets A – B | Δ half-suits |
|---|---|---|---|
| **ask null** — `v05:value=0,lweight=0,topk=0` (take the first *live* legal ask; belief + declaration untouched) | 5.83% | 2.447 – 6.553 | **−4.11** |
| **belief null** — `v05:belief=indep` (independent per-card marginals) | 5.38% | 2.314 – 6.686 | **−4.37** |
| **declaration null** — `v05:declare=0` (never declare voluntarily) | 43.25% | 4.268 – 4.732 | **−0.46** |
| declaration timing — `v05:vdecl=0,decl=0.55` | 47.29% | 4.397 – 4.603 | −0.21 |
| declaration timing — `v05:vdecl=0,decl=0.995` | 46.63% | 4.428 – 4.572 | −0.14 |
| declaration rule — `v05:vdecl=0` (threshold instead of EV) | 49.00% | 4.468 – 4.533 | −0.065 |
| M1 live gate off — `v05:m1=0` | 48.96% | 4.475 – 4.525 | −0.049 |
| two-ply refinement off — `v05:topk=1` | 49.96% | 4.506 – 4.494 | −0.013 (n.s.) |
| M2 feasible allocation off — `v05:m2=0` | 50.46% | 4.516 – 4.484 | +0.032 (n.s.) |
| `v05:patient=0` | 50.00% | 4.500 – 4.500 | **0.000 — bit-identical** |
| `random` | 0.00% | 0.426 – 8.574 | −8.15 |

**Attribution.** Of the ≈ 8.15 half-suits that separate v0.5 from `random`, ask selection
(scoring + belief) accounts for ~4.1–4.4 and declaration for ~0.46 — roughly **9 : 1**. Two
qualifications, both real:

1. `declare=0` is a *weak* null, because `forcedEndgame` (`game.hpp:235-269`) still resolves every
   half-suit through the willingness ladder, and the residual is adjudicated neutrally by
   physical majority (`game.hpp:261-268`). The declaration decision proper is worth ≤ 0.46 sets
   *given a correct endgame*; it is not worth ≤ 0.46 sets in a design without one.
2. `belief=indep` degrades ask *and* declaration simultaneously, so −4.37 is an upper bound on the
   value of inference, not a clean ask-only figure.

### 3.3 The exact posterior loses to the approximate one

`v05:belief=block` (the exact C1–C5 posterior, `blockdp.hpp`) vs shipped `v05` (Sinkhorn + the
`priorTheta/priorPhi` opponent-policy prior, `v05.hpp:29-30`), 300 deals × 6 rotations, seed 606060:

```
win rate 39.83% [37.60, 42.11]     mean sets 4.147 – 4.853     Δ = −0.71 half-suits
throughput 21.7 games/s vs ~300 games/s          (14× slower)
```

Caveat, and it matters: the 20 ask weights were fitted with `belief=Fast`, so this compares
*exact-posterior-with-stale-weights* against *approximate-posterior-with-matched-weights*. What
it does establish, at the shipped weights, is that the **policy prior is worth more than
exactness** — consistent with the standing instruction to treat "exact Bayesian inference" as an
assumption under test, not a claim.

---

## 4. THE TURN AS A RESOURCE

Mechanics: a hit retains the turn (`game.hpp:348-351`), a miss hands it to the **target the
asker chose** (`game.hpp:357`). So a miss is not a pure loss — it is a *directed* donation, and
the asker picks the recipient.

### 4.1 The opening turn is worth nothing (matched-deal natural experiment)

Under rotation `r`, seat `i` receives dealt hand `(i+r) mod 6` (`game.hpp:101-105`), so dealt
hand `h` sits on team `(h−r) mod 2`, while the opening seat is fixed by the deal
(`fish.hpp:208-209`, untouched by rotation). Therefore across `r = 0…5` **the same partition of
the 54 cards into two hand-triples leads in exactly half the rotations**. Comparing the same
hand-triple's score when it leads against when it does not (`r8 turn 400 31 v05`, 400 deals ×
6 rotations = 2,400 games):

```
leads:    n=1200  mean (own − other) = −0.0317 sets
does not: n=1200  mean               = −0.0483 sets
effect of holding the opening turn   = +0.0167 sets   SE 0.1179   z = 0.14
```

**No measurable first-move advantage** (95% CI ≈ [−0.21, +0.25] half-suits).

### 4.2 Mid-game, a turn transfer is worth ~0.5 half-suits, rising to 0.83

Matched contrast on near-coin-flip asks (`r8 flip 250 4242`, 131,149 asks). Conditioning on the
actor's own forecast `p` isolates decisions where the hit/miss split is close to a fair coin
given the actor's information set; outcome is the final differential from the asker's team's view.

| forecast band | hits | miss | Δ (half-suits) | SE | z |
|---|---|---|---|---|---|
| p ∈ [0.45, 0.55] | 0.4754 (n=6,481) | −0.1120 (n=5,297) | **0.587** | 0.052 | 11.3 |
| p ∈ [0.40, 0.60] | 0.4619 | −0.0799 | 0.542 | 0.039 | 13.9 |
| p ∈ [0.30, 0.70] | 0.4711 | −0.0522 | 0.523 | 0.028 | 18.7 |

By phase (p ∈ [0.40, 0.60]):

| events elapsed | 0–19 | 20–39 | 40–59 | 60–79 | 80+ |
|---|---|---|---|---|---|
| Δ (half-suits) | **0.280** | 0.508 | 0.587 | 0.520 | **0.826** |
| SE | 0.092 | 0.088 | 0.085 | 0.083 | 0.089 |

The hit/miss swing bundles three things — one card of material, the turn, and the information the
outcome reveals. It is *not* pure turn value, and hitting is correlated with the rest of the deal.

### 4.3 Cross-check: the fitted value function's own price of the turn

`stateFeatures` (`v04.hpp:405-429`) has `f[5] = turnSign ∈ {−1,+1}` and
`f[6] = (ourCards − theirCards)/54`; the training label is `(score₀ − score₁)/9`
(`game.hpp:372-374`). With the shipped `vw` (`v05.hpp:79-96`):

```
turn      : 2 × vw[5] × 9 = 2 × 0.022896 × 9 = 0.412 half-suits
one card  : (2/54) × vw[6] × 9 = (2/54) × 0.422207 × 9 = 0.141 half-suits
sum                                        = 0.553 half-suits
```

against the measured hit-minus-miss swing of **0.542 ± 0.039**. Two independent routes agree.
So: **the turn is ≈ 0.41 half-suits, about three quarters of the value of a successful ask** —
but only mid-game. It is worth 0 at the opening (§4.1) and ~0.7–0.8 after event 80.

The mechanism is visible in the P8 possession data (`research/v05/results/P8-verify-turn-transfer.md`):
opening possessions run at a 20.6% hit rate, ordinary miss-in possessions at 35.1%, and
endgame possessions at 78–85%. Early, the turn is cheap because nobody can do much with it;
late, it is a licence to empty the table.

Correlation of net retained turns with the final differential is r = 0.960, slope 0.325
half-suits per net retained turn — this is *mechanical* (a retained turn **is** a won card) and
should not be quoted as a causal price.

---

## 5. STRUCTURAL FACTS AND WHETHER v0.5 USES THEM

### 5.1 A half-suit with all six cards on one team is unstealable (v0.4 Theorem 1)

*Why it holds in the implementation.* `legalAsk` (`fish.hpp:161`) forbids asking a teammate, so
no card can cross back once both remaining holders are on the same team. The engine instruments
this at `game.hpp:154-160` (`lockedAt[st]`).

*Incidence.* Analytically, `2·C(27,6)/C(54,6) = 2.292%` of half-suits are locked **at the deal**
before a card moves; measured over 1.8 M dealt half-suits: **2.297%** (`r8 census`, combinatorial
sub-check). Every *correct* declaration is by definition of a locked half-suit; v0.5 holds a lock
for a mean of **5.43 public events** before cashing it (3,529 locked declarations / 19,145
lock-held events over 400 games).

*Does v0.5 exploit it?* Partly, and less than the source says.
- `f[5] = pTeamOther` (`v04.hpp:314`, weight +4.046) and `f[15] = pTeamAll` (`v04.hpp:324`,
  weight **−0.958**) price lock completion and — correctly, post-M1 — *penalise* asking inside a
  half-suit the team already owns.
- `declareNow` computes `locked = v.pTeam > .9995` (`v05.hpp:800`).
- **But the two parameters that implement patience on a lock — `patientLocked` and
  `lockedAllocThresh` — are unreachable in the shipped configuration.** They appear only at
  `v05.hpp:808`, inside the `if (locked)` block that `v05.hpp:803-806` can only reach when
  `!(cfg.useValue && cfg.valueDeclare)`; both are `true` by default (`v05.hpp:70,73`).
  Confirmed empirically: `v05:patient=0` vs `v05:patient=1` is **bit-identical** — 50.00%,
  4.500 – 4.500, ask accuracy 56.19% / 56.19%, declarations 4.49458 both sides. Inside a
  reachable configuration (`vdecl=0`) patience is worth **+0.077 sets** (50.5%, 4.538 – 4.462),
  which is not significant at n = 2,400.
  → **v0.6 should treat "when to cash a lock" as an open, unimplemented question.**

### 5.2 Voiding an opponent of a half-suit removes their right to ask in it — permanently

*Why.* `legalAsk` requires `(g.hand[actor] & setMask(s)) != 0` — `fish.hpp:164`. Once a player
holds no card of *S*, they can never regain one except by asking in *S* (impossible) or being
dealt one (impossible). The right is destroyed for the rest of the hand. This is a **permanent,
one-way asset**, and it is exactly computable: `P(target holds ≥ 1 card of S) = 1 − Π_c (1 − μ_{c,target})`
is already implemented — twice.

*Incidence* (`r8 census 400 31`, 34,823 asks / 400 games):
- **35.07%** of (live opponent, active half-suit) pairs at ask time are already voids.
- **23.15% of all asks — 41.65% of all successful asks — take the target's last card of that
  half-suit**, i.e. permanently disenfranchise them there.

*Does v0.5 exploit it?* **Only defensively, never offensively.**
- Defensive use: `threatOf` computes `canAsk = 1 − Π(1−pt)` (`v04.hpp:214-222`) and feeds
  `f[8] = (1−p)·threatOf(target)` (weight −2.906, `v04.hpp:317`); `choosePassTarget` uses the same
  quantity (`v05.hpp:951-963`).
- Offensive use: **none**. Scan the whole feature vector `v04.hpp:309-328` — there is no term for
  "this hit would void the target of this half-suit". The nearest, `f[11] = (handCount[target] == 1) ? p : 0`
  (`v04.hpp:320`, weight +1.160), fires only when the target's *entire hand* is one card, which
  happens on 7.55% of asks — a third as often as the void event, and a strictly different one.
- This is the single largest cheap, provably-correct, unexploited quantity found in this recon.

### 5.3 The certificate an ask emits

Each ask publishes three facts, all recorded by `Knowledge::onEvent` (`belief.hpp:153-183`):
1. **the asker does not hold the card** — `exclude(e.card, e.actor)`, `belief.hpp:167` (C3);
2. **the asker holds ≥ 1 other card of the half-suit** — the disjunction built at
   `belief.hpp:158-171` (C5); it collapses to a certainty when only one candidate card remains
   (`belief.hpp:169`);
3. **on a miss, the target does not hold the card** — `exclude(e.card, e.target)`, `belief.hpp:182`.

Fact 2 is the only *voluntary* signalling channel in the game: it is emitted to teammates and
opponents simultaneously, i.e. a wiretap channel.

*Does v0.5 exploit it?* **It prices only the leak, never the gift, and it prices the leak as a
binary.** `f[9] = teamRevealedSet(S) ? 0 : 1` (`v04.hpp:318`, weight −1.220) and
`f[19] = f[9] × myHave/6` (`v04.hpp:328`, weight −1.403). `teamRevealedSet`
(`v04.hpp:241-247`) asks only "has my team ever asked/been publicly located in this half-suit",
a single bit per half-suit. Nothing measures *how much* the certificate narrows the posterior,
and — the important gap — **no feature values the certificate positively for the teammate who
receives it.** This is the Farrell–Gibbons wiretap trade-off flagged in
`research/v05/BRIEF.md` §"central suspected error" as formalised but never built. It is still
never built.

### 5.4 A cardless player may still declare

`Rules::cardlessMayDeclare = true` (`fish.hpp:109`); the gate is `game.hpp:216` for the
voluntary path and `game.hpp:243` for the forced ladder; v0.5's own gate is `v05.hpp:817`.
A declarer also needs no card of the declared half-suit — `applyDeclaration` (`game.hpp:170-199`)
checks only that every named owner is on the declarer's team and truly holds the card.

*Does v0.5 exploit it?* **Yes, and materially** (`r8 census 400 31`, 3,600 declarations):
- **6.44%** of declarations come from a seat with **zero cards**;
- **23.58%** come from a seat holding **no card of the declared half-suit**;
- 98.03% of all declarations are correct.

This is the one structural fact on the list that v0.5 already uses well. Note the asymmetry with
§5.1: v0.5 exploits *who may declare* but not *when to declare*.

### 5.5 Two further facts the recon turned up

- **A player holding only complete half-suits has no legal ask and is forced to declare** —
  `game.hpp:328-336` ("holds only complete sets: must declare"), reached when `enumerateAsks`
  returns 0. This is a *zugzwang* channel: stripping an opponent to complete-sets-only forces a
  declaration at a moment you choose. v0.5 has no concept of it.
- **A miss is a directed donation.** The asker names the recipient of the turn
  (`game.hpp:357`). v0.5 prices this through `f[8]` reply threat and `f[16]` exposure
  (`v04.hpp:317, 325`) but only as a *scalar discount on the ask*, never as a *choice of victim*
  among otherwise equal-probability asks.

---

## 6. IS THERE A PROVABLY-CORRECT ENDGAME REGIME?

### 6.1 Cost of exhaustive enumeration — the number you asked for

An independent enumerator (no `oracle.hpp` pre-filter, see the bug note in §6.4) was run on real
v0.5 states, budgeting 2 × 10⁸ DFS nodes, sampling capped per Q
(`r8 endgame 200 31 200000000`; 3,753 states, single thread, Apple M-series, `-O3 -march=native`):

| Q | median Z | median ms | p90 ms | p99 ms | throughput (deals/s) | DFS nodes / consistent deal |
|---|---|---|---|---|---|---|
| 8 | 324 | 0.016 | 0.048 | 0.086 | 1.08e7 | 11.3 |
| 10 | 2,850 | 0.175 | 0.606 | 0.965 | 2.53e7 | 12.6 |
| 11 | 7,640 | 0.577 | 2.41 | **4.09** | 2.27e7 | 14.1 |
| 12 | 27,720 | 2.252 | **8.74** | 19.5 | 2.39e7 | 14.4 |
| 13 | 232,110 | **9.850** | 40.4 | 60.7 | 2.97e7 | 12.3 |
| 14 | 671,930 | 50.3 | 161 | 272 | 2.65e7 | 13.1 |
| 15 | 783,405 | 140 | 351 | 510 | 1.91e7 | 17.5 |

**Answer:** at a 10 ms budget per decision, exhaustive enumeration of all consistent deals is
affordable up to

- **Q = 13 unresolved cards** if you accept the *median* decision (9.85 ms) — half the decisions
  at Q = 13 blow the budget;
- **Q = 12** for a p90 guarantee (8.74 ms);
- **Q = 11** for a p99 guarantee (4.09 ms).

Coverage: **Q ≤ 12 is 21.8% of all ask decisions; Q ≤ 13 is 24.0%; Q ≤ 8 is 14.1%**
(`r8 state 400 90210`, SOLVABILITY block). At Q = 12 the median decision has 23 public events
and ~22 live asks left.

Throughput is ~2.4 × 10⁷ consistent deals/s single-threaded, so a 6-thread build moves the
thresholds up by roughly one card (10 ms × 6 ≈ 1.4 M deals ≈ Q = 14 median).

### 6.2 …but enumeration is the wrong mechanism, and here is the proof

**Theorem (perfect-information Fish).** If both teams know the deal, the team on turn wins every
half-suit except those dealt outright to the opposing team.

*Proof sketch, from the implementation.* With perfect information the actor asks the true holder,
so every ask hits and the turn is never lost (`game.hpp:348-357`). The actor therefore takes every
opponent-held card of every half-suit it personally holds a card in. Any half-suit wholly owned
by the team is declared in the pre-ask `declarationRound` (`game.hpp:294`, `202-231`), so the
actor is never left with a guaranteed-miss ask; when it has none, `game.hpp:328-336` forces a
(correct) declaration; when it empties, the turn passes within the team (`game.hpp:298-308`) and
the next teammate repeats. The defending team can only declare what it already owns outright.

*Verified.* `r8 omni 300 31 v05`, both teams omniscient, 300 games:

```
PERFECT INFO: mover 8.8667 sets, other 0.1333 sets
  mean half-suits dealt LOCKED to the non-moving team: 0.1333
  games where non-mover's score == its deal-time locked count: 300/300 = 1.0000
```

An exact identity in every single game. And an omniscient team against v0.5:

```
OMNISCIENT vs v05 : 8.9317 - 0.0683 sets, win rate 1.0000 over 600 games
```

**Three consequences.**

1. **The hidden state is worth ≈ 4.4 half-suits.** v0.5 mirror scores 4.5 – 4.5; knowing the deal
   converts that to 8.93 – 0.07. Everything else in this report is small next to this.
2. **Determinized search (PIMC / "sample deals, solve each, average") is provably degenerate in
   Fish.** Every determinization returns "the side to move takes everything", so every legal
   action in every sampled world has (nearly) the same value and the average cannot discriminate.
   This is strategy fusion at its theoretical maximum. **A v0.6 "perfect endgame solver" built on
   deal sampling will not work**, and the §6.1 threshold, while real, is a threshold for a
   mechanism that should not be built.
3. The perfect-information value also explains §4.1: the opening turn is worth ~0 *because* the
   mover cannot use it, and worth ~9 sets once it can.

### 6.3 What IS affordable, and provably correct

The posterior is already exact and closed-form. `BlockDP::build` is exact under C1–C5
(`blockdp.hpp:19-26`; validated by `fish oracle` to `< 1e-9` on Z, marginals, team-ownership
and every named allocation, `oracle.hpp:55-64`), and it is **100–1,000× cheaper than
enumeration**:

| Q | 8 | 10 | 12 | 14 | 20 | 30 | 45 |
|---|---|---|---|---|---|---|---|
| BlockDP build, mean ms | 0.0049 | 0.0081 | 0.0111 | 0.0187 | 0.0454 | 0.169 | 0.995 |
| BlockDP build, p99 ms | 0.023 | 0.038 | 0.047 | 0.067 | 0.144 | 0.348 | 1.400 |
| enumeration, median ms | 0.016 | 0.175 | 2.25 | 50.3 | — | — | — |

So there is **no Q at which enumerating deals tells you anything the DP does not already give
exactly.** (Certificates never couple two half-suits — `blockdp.hpp:4-13` — and cross-half-suit
coupling runs entirely through capacity, which the DP handles exactly.)

**The provably-correct endgame regime that does exist is a belief-space search**, and the budget
is concrete:

- **10 ms ÷ 0.0111 ms = ~900 exact belief rebuilds per decision at Q = 12** (~2,000 at Q = 8,
  ~220 at Q = 20).
- Full-width branching at Q = 12 is 22.4 live asks × 2 outcomes = ~45 children → depth ≈ 1.6.
- Restricted to v0.5's existing top-K = 6 candidate set (`v05.hpp:34, 520-535`) → 12 children per
  ply → **depth ≈ 2.7**, and with the near-deterministic late-game hit probabilities (many asks
  have p ∈ {0, 1}) the effective depth is higher.
- At **Q ≤ 8 (14.1% of decisions, median 9 public events left, mean 8.4 live asks; ~2,000
  rebuilds per 10 ms)** a depth-3 to depth-4 exact expectimax over the belief state is
  comfortably inside 10 ms.
- **Q = 0 occurs on 2.85% of decisions** — genuinely perfect information for that seat, median 4
  events from the end. Those are *provably* solvable exactly, today, at negligible cost, and v0.5
  does not treat them specially.

### 6.4 A latent bug in the enumeration path (report, do not fix here)

`BruteForce::enumerate` (`oracle.hpp:80-135`) initialises the DFS stack with
`for (int i = 0; i < nU; i++) stack[i] = -1;` (`oracle.hpp:102`). When `nU == 0` — i.e.
`k.unresolved == 0`, which happens on **2.85% of real decisions** — `stack[0]` is read
uninitialised at `oracle.hpp:108-109`, and `cnt[nxt]` / `cmask[depth]` are then indexed out of
bounds at `oracle.hpp:112-113`. AddressSanitizer:

```
ERROR: AddressSanitizer: stack-buffer-underflow ... in fish::BruteForce::enumerate oracle.hpp:113
```

The shipped `fish oracle` never triggers it because `main.cpp:353` guards with
`if (!kk.unresolved) continue;`. Any v0.6 code that reuses `BruteForce` without that guard will
crash. Cost to fix: one line (`stack[0] = -1;` unconditionally, plus an `nU == 0` early return).

---

## 7. Reproduction

Engine, unmodified, at `bd812fe`:

```
cd engine && make
./fish match --a=v05 --b=v05 --games=100 --rotations=2 --seed=31
./fish match --a="v05:value=0,lweight=0,topk=0" --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:declare=0"  --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:belief=indep" --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:m1=0"        --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:topk=1"      --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:vdecl=0"     --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:vdecl=0,decl=0.55"  --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:vdecl=0,decl=0.995" --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:m2=0"        --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:patient=0"   --b=v05 --games=400 --rotations=6 --seed=606060
./fish match --a="v05:vdecl=0,patient=0" --b="v05:vdecl=0,patient=1" --games=400 --rotations=6 --seed=606060
./fish match --a="v05:belief=block" --b=v05 --games=300 --rotations=6 --seed=606060
./fish match --a=random            --b=v05 --games=400 --rotations=6 --seed=606060
```

Scratch probe (subclasses `V05Agent`, includes the unmodified headers; **not** part of the engine):

```
SRC=<scratchpad>/r8/r8.cpp
clang++ -std=c++20 -O3 -march=native -fno-math-errno -I engine/src $SRC -o r8 -pthread
./r8 state   400 90210            # info-set size, action-set size, DP cost, horizon, solvability
./r8 endgame 200 31 200000000     # exhaustive-enumeration cost by unresolved-card count
./r8 turn    400 31 v05           # opening-lead matched-deal experiment
./r8 deal    250 777              # deal-strength → outcome
./r8 flip    250 4242             # near-coin-flip hit/miss matched contrast
./r8 census  400 31               # locks, voids, declaration provenance
./r8 omni    300 31 v05           # perfect-information value
```

The probe source is committed at
`research/v06/notes/probes/R8-game-structure-probe.cpp` (build it with
`clang++ -std=c++20 -O3 -march=native -fno-math-errno -I engine/src <file> -o r8 -pthread`
from the repository root).

---

## 8. Candidate v0.6 mechanisms, ranked by (evidence × cheapness)

| id | mechanism | why the evidence supports it | difficulty | payoff |
|---|---|---|---|---|
| **V1** | **Offensive void value.** Add an exact term `P(this hit takes the target's last card of S)` and reward it — it permanently destroys their right to ask in S (`fish.hpp:164`). Computable in closed form from `BlockDP` marginals. | 41.7% of hits already do this by accident; no feature scores it (`v04.hpp:309-328`); the defensive dual is already implemented (`v04.hpp:214-222`) | low | high |
| **V2** | **Certificate pricing.** Replace the binary `teamRevealedSet` leak feature with the measured posterior-narrowing the C5 disjunction causes, split into *what my teammate gains* and *what the opponents gain*. | `f[9]/f[19]` are one bit per half-suit (`v04.hpp:318,328`); the wiretap trade-off is formalised in `research/v04/lit/signalling.md` and never built | medium | high |
| **V3** | **Belief-space endgame search** at Q ≤ 8–12: exact expectimax over `BlockDP` rebuilds, ~900 nodes / 10 ms at Q = 12, depth 3–4 at Q ≤ 8; special-case Q = 0 (2.85% of decisions) as solved. | §6.1, §6.3; DP is 100–1000× cheaper than enumeration and already exact | medium | medium-high |
| **V4** | **Do NOT build a determinized/PIMC endgame solver.** | §6.2: perfect-information Fish is 8.87–0.13 for the mover, verified 300/300; every determinization scores every action alike | — | — |
| **V5** | **Resurrect lock patience.** `patientLocked` / `lockedAllocThresh` are unreachable under the shipped `useValue && valueDeclare` (`v05.hpp:803-812`, confirmed bit-identical). Decide when to cash a lock inside the EV path. | §5.1; 5.43 events of lock latency is currently an accident, not a decision | low | unknown |
| **V6** | **Turn-donation targeting.** Among near-equal asks, choose *which opponent receives the turn on a miss* as a first-class decision, keyed to that opponent's void profile. | the turn is worth 0.41 sets mid-game and 0.83 late (§4); v0.5 only discounts the ask, never picks the victim (`v04.hpp:317,325`) | low | medium |
| **V7** | **Zugzwang.** Track when an opponent is one card from holding only complete half-suits, which forces a declaration at a moment of your choosing (`game.hpp:328-336`). | rule verified in code; zero v0.5 awareness | medium | unknown |
| **V8** | **Re-fit the ask weights under `belief=block`.** The exact posterior currently *loses* 0.71 sets, but the weights were fitted under `belief=Fast` — the comparison is confounded and the question is open. | §3.3 | medium | unknown |
| **V9** | **Fix `oracle.hpp:102`** before any v0.6 code reuses `BruteForce`. | §6.4, ASAN trace | trivial | — |
