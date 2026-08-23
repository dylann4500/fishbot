# C1 — Corrections the v0.5 study makes to the v0.4 study

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`
(`fe21e193999695792ae598561a91dbaff6ef1b55`), 2026-08-22.

This is the corrections register of record. It lists every v0.4 claim the v0.5 diagnosis overturns, every
v0.4 claim it *fails* to overturn, and the corrections the v0.5 study must make to its own
diagnosis reports before they are quoted in a paper.

---

## How to read this

Each correction carries four fields:

1. **The v0.4 sentence, verbatim**, with `file:line`. LaTeX sources are quoted as source,
   `\num...` macros intact; their values are given from `paper/numbers.tex`.
2. **The correction** — what is actually true.
3. **The measurement that settles it**, with the artifact that carries it. Every headline
   figure below was adversarially re-verified at a seed independent of the one it was first
   measured at; where the verifier corrected the original magnitude, the *corrected* figure is
   the one quoted here and the correction is named.
4. **What the v0.5 paper should say instead.**

Verdict labels: **WRONG** (the claim is false), **INOPERATIVE** (the described mechanism does
not run), **OVERSTATED** (true in a narrower scope than stated), **SCOPE** (true as measured,
but the measurement does not cover the case the reader will apply it to), **BACKWARDS** (the
sign of the stated defect is the wrong way round).

Two things this list deliberately does *not* do. It does not attribute an error to the paper
when the error lives only in a supporting document — §C1 is the clearest case, where
`paper/sections/06-locked.tex` states the correct thing and `E11-termination.md` and
`docs/V04_FINDINGS.md` state the wrong one. And it does not accuse where the v0.4 study
checked itself and was right: §S is as long as it is because most of what v0.4 disclosed about
itself is accurate.

### Index

| # | v0.4 claim | Where | Verdict | Settled by |
|---|---|---|---|---|
| C1 | A locked half-suit is informationally frozen, and that is why two patient policies deadlock | `E11-termination.md:3-7`, `docs/V04_FINDINGS.md:71-76` | **WRONG** (both halves) | P1, P1-verify ×2 |
| C2 | "A graduated in-policy forcing rule removes it entirely" | `docs/V04_FINDINGS.md:75`, `06-locked.tex:186-192` | **WRONG** | P0, P0b, P4, P4-verify |
| C3 | The eight-level forced-endgame willingness ladder is a working selection mechanism | `A-dialect.tex:213-234`, `02-rules.tex:64-65`, `07-policy.tex:195-198` | **INOPERATIVE** | P2, P2-verify ×2, P8 |
| C4 | The fitting objective is a soft *minimum* — the worst case across playstyles | `docs/FISHBOT_V04.md:128-130`, `docs/METHODOLOGY.md:50-52`, `08-fitting.tex:6-21` | **WRONG** + panel omission | P7 §3, arithmetic on `research/v04/runs/` |
| C5 | The 16-feature linear `V` is the fitted evaluation whose differences drive the ask rule | `07-policy.tex:125-141`, `06-locked.tex:96-135` | **WRONG** in the part the ask rule uses | P7 §2, source algebra |
| C6 | The compiled `V04Config::vw` not matching E14 is a known gap/defect | `docs/FISHBOT_V04.md:150-152` | **BACKWARDS** | P7 §1.2, P7-verify |
| C7 | "past a second horizon its best candidate whatever the estimate, an undeclared half-suit scoring nothing" | `06-locked.tex:186-190` | **WRONG** in this harness | P4-verify §5, `game.hpp:271-287` |
| C8 | "Only the willingness bit crosses between seats" | `A-dialect.tex:218`, `A-dialect.tex:231-232`, `02-rules.tex:37-38` | **WRONG** as an information-safety guarantee | P8 §3 |
| C9 | Restarting the ladder lets early declarations sharpen later ones | `A-dialect.tex:232-234` | **INOPERATIVE** | P2 §5, P2-verify §6 |
| C10 | "\vfast's lowest win rate against any panel member is 75.07%"; "declared correctly 98.55% of the time" | `11-discussion.tex:41-46`, `13-conclusion.tex:23-26` | **SCOPE** | P0 |
| C11 | The observed cycling is a property of *an earlier* fitted configuration | `06-locked.tex:170-179`, `13-conclusion.tex:56-58` | **OVERSTATED** in v0.4's favour | P0, P0b |
| C12 | `\phi` is an independent "silence" channel in a two-parameter policy prior | `05-belief.tex:269-282` | **OVERSTATED** | P3 §2/§4/§5, P3-verify |

---

## C1 — The termination theory. Both halves are wrong.

### The v0.4 sentences

`research/v04/results/E11-termination.md:3-7`:

> Nothing in the rules of Fish compels a player to declare. Theorem 1 of the paper
> (a half-suit held entirely by one team cannot be asked in by the other) implies
> that such a half-suit is frozen — and, for the same reason, that no further
> information about its allocation can ever arrive. Two policies that both
> correctly decline to cash an uncertain half-suit therefore deadlock.

`research/v04/results/E11-termination.md:45-47`:

> A tournament form of Fish needs an explicit forced-claims provision. The rules
> literature records such a proposal; the reason it is necessary — that Theorem 1
> freezes information as well as ownership — appears not to have been stated.

`docs/V04_FINDINGS.md:71-76`, finding 6:

> **Termination is a property of the policy, not of the rules.** Because a
> half-suit frozen by the locked-half-suit theorem also admits no further
> information, two correctly patient policies deadlock permanently; a fitted
> configuration failed to terminate in 21% of self-play deals at any action cap
> tried. A graduated in-policy forcing rule removes it entirely. See
> `research/v04/results/E11-termination.md`.

### The correction

Two independent claims are made, and both are false.

**(a) "No further information about its allocation can ever arrive" is false on its own
terms.** It is false at the level of the rules before any measurement: `legalAsk`
(`engine/src/fish.hpp:158-165`) requires only that the set is active, the target is a live
opponent, the actor lacks the asked card and holds another card of the set. In a half-suit
locked to team *T*, any member of *T* holding at least one but not all six of its cards
satisfies all four conditions for every card of the half-suit it does not itself hold — four
cards apiece on the modal 2/2/2 split, and at least two for any member holding four or fewer.
`Knowledge::onEvent` publishes `exclude(e.card, e.actor)` at
`engine/src/belief.hpp:167`, *before* the `if (e.success)` branch — so "the asker does not hold
this card" becomes public, permanent, allocation-relevant information whether the ask hits or
misses. One line of the production belief code refutes the claim.

**(b) The observed deadlock is not the frozen-half-suit position at all.** It is a
deterministic two-question ask cycle in ordinary mid-game positions, most of which contain no
locked half-suit anywhere on the board.

### The measurements

**(a) Information does arrive, and it is decisive.** Exact `BlockDP` posteriors, hypothetical
asks applied through the production certificate machinery (`Knowledge::onEvent`), pooled over
three seeds (31 / 777001 / 20260822), 2,880 legal asks inside a half-suit the asking team in
fact owns — `P1-deadlock-forensics.md` §2.2 and `P1-verify-e11-information.md` §3:

| ask class | n | strictly raise a teammate's exact P(correct MAP allocation) | mean ΔP | max ΔP |
|---|---:|---:|---:|---:|
| inside a half-suit our team in fact owns | 2,880 | **1,972 (68.5%)** | **+0.0957** | **+0.6640** |
| any other legal ask | 8,592 | 2,376 (27.7%) | +0.00117 | — |

Re-measured with two statistics that avoid `bestTeamAllocation` entirely — support size (pure
combinatorics) and Shannon entropy of `BlockDP::marginals` — over 5,760 teammate asks inside
the locked half-suit (`P1-verify-e11-information.md` §5): **38.6%** strictly shrink the
support, **59.9%** strictly lower the exact marginal entropy, mean ΔH **0.325 nats**, max
**2.115 nats**. Asks *outside* the locked half-suit shrank its support in **0 of 20,898**
cases, at every seed — the certificate lands exactly where it is aimed.

The statistic is oracle-validated: `fish oracle` checks `pAlloc` against exhaustive
enumeration, `named allocation prob max abs diff 0.000e+00 over 227062 checks`,
`ORACLE PASS` (`P1-verify-e11-information.md` §4).

**(b) The deadlock is a two-question cycle in unlocked positions.** Ground-truth lock census
taken at *every event* of the dead run (not only its ends), independently re-implemented in
`engine/src/probe_verifylock.hpp`, four seeds, 62 long games
(`P1-verify-deadlock-lock-census.md` §2-3):

| quantity | value |
|---|---:|
| long games with **no** half-suit locked to any team anywhere in the dead run | **41 / 62 (66%)** |
| dead-run asks sitting in a ground-truth-locked half-suit | **1,947 / 14,997 (13.0%)** |
| long games whose *entire* deadlock cycle is inside locked half-suits | **2 / 62 (3.2%)** |
| distinct `(actor, card, target)` triples inside the longest dead run | mean **2.14** |

At trace A's onset (`272135269103994248/1`, event 24) all nine half-suits are live and
genuinely split, **51 of 54 legal asks are not provably dead**, and v0.4 still ranks a
`p(hit)=0` ask first and repeats it 142 times. Nothing is frozen; the policy declines every
live ask on its own score.

**Independent short reproduction for this document.** `./fish deadlock --games=20 --dump=0
--states=2 --stride=40 --seed=424243` — 40 mirror games at a seed used by none of the P1 runs:

```
scanned 40 games (v04 mirror, seed 424243)   games with >300 events: 6
  runs that are a pure two-question cycle: 5 / 6
  runs where ANY half-suit was already locked ... at the run's start or end: 1 / 6
  own-locked asks with dP > 0 for a teammate: 32 / 36 (88.9%)  mean dP 0.537  max dP 0.645
  other asks with dP > 0 for a teammate:      78 / 342 (22.8%) mean dP 0.003
  the ask v0.4 actually played was information-free for its teammates in 12 / 12 states
  at least one legal ask WAS informative in 12 / 12 states
  the most informative legal ask ranks on average #16.2 in v0.4's own ask score
```

Both halves reproduce: 5 of 6 long games are pure two-question cycles, only 1 of 6 has any
locked half-suit at the run's ends, and information is available at 100% of the sampled
decision points and taken at 0% of them.

### Why the mechanism is what it is

Three of v0.4's twenty ask features are functions of `(1−p)` or `H(p)` and outvote the
hit-probability term in low-`p` positions (`P1-deadlock-forensics.md` §1.3): `f[14] = binEnt(p)`
at weight **−2.6534** pays a bonus for asking a card whose location is already certain;
`f[16] = (1−p)·exposureOf(target)` at **+1.9040** is *maximised* when the ask is a certain miss;
`f[10] = handCount[target]/9` at **−2.0219** makes a nearly-empty opponent cheap to ask
regardless of `p`. Trace B's gap decomposition: `entropy +1.6637, exposureOnMiss +1.3323,
targetHand +1.2057` against `p(hit) −2.2421`, net **+1.8196** for the dead ask. The belief
correctly reports `p = 0`; the score prefers the move anyway. Switching to the exact belief
(`v04:belief=block`) cuts dead asks 39% → 28% and leaves the longest run at 280
(`P0-v04-pathology.md`). **A policy defect, not an inference defect.**

### What the v0.5 paper should say

> Theorem 1 constrains ownership monotonicity. It does not constrain allocation information,
> and the v0.4 study's supporting artifacts inferred the second from the first. Legal asks
> inside a locked half-suit raise a teammate's exact probability of the correct allocation in
> 68.5% of cases (mean +0.096, max +0.664, n = 2,880 over three seeds) and strictly lower its
> exact marginal entropy in 59.9% (mean 0.325 nats, n = 5,760). Separately, the deadlock
> reported in E11 is not that position: it is a strictly 2-periodic ask cycle, 66% of whose
> host games contain no locked half-suit at any point in the run and only 13.0% of whose asks
> sit inside one. The v0.4 paper's Observation 2 (`06-locked.tex:53-66`) states the correct
> distinction and is vindicated; `E11-termination.md` and `docs/V04_FINDINGS.md` finding 6 are
> retracted.

**Scope note, which the v0.5 paper must keep.** The half-suits in this measurement are locked
in *ground truth*. At the 15 dumped seed-31 deadlock states, across all 36 (owning-team
observer × locked half-suit) pairs, the observer's own exact `P(my team owns this half-suit)`
has mean 0.068, median 0.030, max 0.342, and **0 of 36** exceed 0.999
(`P1-verify-e11-information.md` §6.1). P1's phrase "a half-suit the team **provably** owns" is
wrong and must not be carried forward: the refutation of E11 is unaffected (E11 quantifies over
half-suits that *are* locked), but a v0.5 ask rule cannot condition on provable ownership — it
must **price** expected information against `pTeam`. `P5-human-strategy.md` §0.2 makes the same
point from the other side: provable team ownership holds at only **0.34%** of mirror decisions.

---

## C2 — "A graduated in-policy forcing rule removes it entirely."

### The v0.4 sentences

`docs/V04_FINDINGS.md:75`:

> A graduated in-policy forcing rule removes it entirely.

`research/v04/results/E11-termination.md:29-34`:

> What works is escalating on the public event count: below the forcing horizon the
> optimal-stopping rule decides; past it the policy cashes any half-suit it is
> better than even money on; past a second horizon it cashes its best candidate
> whatever the probability, since an unclaimed half-suit scores nothing. Under this
> rule every game in every experiment in the study terminates through play and the
> action cap is never reached.

`paper/sections/06-locked.tex:186-192`:

> The shipped alternative is a graduated escalation on the public event
> count: below a forcing horizon \eqref{eq:stop} decides, past it the policy
> declares any half-suit whose estimated allocation probability exceeds one half,
> and past a second horizon its best candidate whatever the estimate, an
> undeclared half-suit scoring nothing. Under this rule every game terminated
> through play and the action-limit rate is \numLimitRate{} in E1, E3, E7, E10 and
> E13

(`\numLimitRate` = `0.000%`.)

### The correction

The rule removes the *action-cap symptom*, not the deadlock. It removes it by misdeclaring.
The cycle itself is untouched — the shipped mirror still contains dead runs up to 286 asks — and
the escalation's second stage converts those cycles into lost half-suits at a measured cost of
**8.13 points against a mirror opponent and at most 0.33 points — inside sampling noise —
against every weaker style tested**.
"Removes it entirely" is true only of the statistic E11 chose to report.

### The measurements

**The deadlock survives the forcing rule.** Shipped v0.4 mirror, `fish pathology`, 600 games,
seed 31 (`P0-v04-pathology.md`): action-limit games **0%** — and, in the same run, dead runs
2,610 with **longest 286**, **34.3%** of games containing a run of ≥ 6 provably-dead asks,
**39.0%** of all asks provably dead, **40.0%** exact repeats.

**What the escalation costs, per opponent.** `P4-policy-review.md` §D1, seed 31; the `fix=32`
variant makes `pressure()` return 0 always:

| A | B | deals | A win rate | A decl wrong | B decl wrong |
|---|---|---:|---:|---:|---:|
| `p4:fix=32` | `p4` (stock v0.4) | 400 | **58.13% [56.38, 60.00]** | 1.61% | 16.44% |
| `p4:fix=32` | `v03` | 400 | 74.62% [71.75, 77.50] | 1.76% | — |
| `p4` | `v03` | 400 | 74.62% [71.75, 77.50] | 1.78% | — |
| `p4:fix=32` | `detective` | 300 | 78.50% | 1.57% | — |
| `p4` | `detective` | 300 | 78.17% | 1.73% | — |
| `p4:fix=32` / `p4` | `lockout`, `hunter`, `bluffer` | 300 ea. | identical to ±0.16 pp | — | — |

Verified independently at seed 777001 by a different route — `v04:force=1000000` on the
**shipped** header rather than a probe copy (`P4-verify-forcing-horizon.md` §4): **58.75%**
[57.00, 60.62] against stock v0.4, i.e. **+8.75 pp**; against `v03` and `detective` the two
configurations are **identical to the digit**. Claimed 8.13, measured 8.75 at a fresh seed.

**Where the cost lives: the unconditional second stage.** At `nEvents >= 308`
(`v04.hpp:583`, `= 7·forceDeclareEvents/5` with `forceDeclareEvents = 220`), three safeguards
switch off at once — `teamFloor` becomes 0 (`v04.hpp:607`), `marginalGate` becomes 0
(`v04.hpp:608`, and `cheap < 0.0` is unsatisfiable, so the gate is *disabled*, not lowered),
and `declareNow` returns `true` as its second statement, **before reading `pAlloc` at all**
(`v04.hpp:675`). Constructed misfire on the shipped header, `nEvents` the only variable, 200
fresh deals, agent given no history at all (`P4-verify-forcing-horizon.md` §2):

```
  declares at nEvents=219 (press 0): 0
  declares at nEvents=307 (press 1): 0
  declares at nEvents=308 (press 2): 200   of which WRONG: 196 (98.00%)
  smallest stated pAlloc it was willing to cash at press 2: 8.457e-04
```

**What it costs in declarations.** Mirror, 600 games, seed 31 (`P0-v04-pathology.md`):

```
declarations       5400   wrong 564 (10.4444%)
  at/after ev>=220 768   wrong 450 (58.5938%)
```

**58.6%** of declarations made at or after the forcing horizon are wrong — 450 half-suits
handed to the opponents in 600 games. Bucketed by the confidence the policy itself attached
(`P4-policy-review.md` §D1, 200 mirror games): 130 declarations at a self-assessed `pAlloc`
below 0.1, **100% of them wrong**; pre-horizon declarations 1.83% wrong, at/after 66.54% wrong.
Reproduced at seed 777001: 136 declarations in `[0.0, 0.1)`, **99.26% wrong**; at/after the
horizon **68.35% wrong** (`P4-verify-forcing-horizon.md` §3).

**The isolation.** Keeping every `press>=2` relaxation but requiring `pAlloc >= 0.5` (`fix=64`)
recovers 5.12 of the 8.13 points at seed 31 and **7.25 of the 8.75** at seed 777001. The cost
is the *unconditional* cash, not the horizon as a concept.

**The honest counterweight, which v0.5 must carry.** The escalation is doing two jobs, and only
one of them is wrong. Removing it entirely leaves the game unterminated: with both sides at
`force=2000`, **22.5%** of mirror games hit the action cap (`P0b-forcing-dilemma.md`,
`fish pathology --a=v04:force=2000 --b=v04:force=2000 --games=120 --seed=31`), and at seed
777001 with `force=1000000`, **23%** (`P4-verify-forcing-horizon.md` §6). Pure patience is also
worse on the pathology it was supposed to fix — 51.2% dead asks, longest run 379.

Also worth recording so it is not re-litigated: the brief's own hypothesis that the horizon
fires on healthy long games **did not hold**. Every game reaching event 220 is genuinely
deadlocked — 46/46 at seed 31, and raising the deadlock threshold to a dead-ask run of 80
leaves the split unchanged (`P4-policy-review.md` §D1). The problem is not *when* it fires.

### What the v0.5 paper should say

> The graduated escalation does not remove the deadlock; it removes the action-cap statistic.
> In shipped mirror play with the escalation active, 34.3% of games still contain a run of six
> or more consecutive provably-dead asks and the longest run is 286. What the escalation adds
> is a termination device whose second stage cashes unconditionally: 58.6% of declarations made
> at or after the horizon are wrong, 100% of those made at a self-assessed probability below
> 0.1, and the stage costs 8.13–8.75 points against a mirror opponent while costing at most
> 0.33 points — within sampling noise, and 0.00 against the hunter and the bluffer — against
> v0.3, the detective and the lockout. It is a pure
> strong-opponent tax, which is why the published head-to-head number never exposed it.
> Removing it without replacement is not the fix either: 22.5% of mirror games then fail to
> terminate. v0.5's M1 supplies the missing third option — the deadlock is prevented rather
> than paid for.

For reference, v0.5 as built (`v05`, `fish pathology`, 600 games, seed 31) reports longest dead
run 286 → 1, games with a dead run ≥ 6 34.3% → 0%, post-horizon declarations 768 → 0,
declarations wrong 10.4% → 1.85%.

---

## C3 — The forced endgame, and the willingness ladder the paper describes as a mechanism

### The v0.4 sentences

`paper/sections/A-dialect.tex:216-231`:

> When one team holds no cards at all, no legal ask remains for either side, and
> the live team must declare every remaining half-suit. Teammates may negotiate
> openly but may exchange nothing except willingness to declare, so the driver
> implements the selection as a threshold sweep rather than a comparison of
> confidences. The willingness ladder is
> \[ \Theta \;=\; \{\,0.995,\; 0.98,\; 0.95,\; 0.90,\; 0.80,\; 0.65,\; 0.50,\; \text{best guess}\,\}, \]
> eight levels held in \texttt{forcedTh} ... For each
> threshold $\theta \in \Theta$ in descending order, the driver scans the live
> team's seats over the remaining half-suits and asks each seat whether it is
> willing to declare that half-suit at confidence at least $\theta$; the first
> seat that says yes declares, the ladder restarts at its top, and the sweep
> repeats until every half-suit is resolved.

`paper/sections/02-rules.tex:64-65`:

> \item \textbf{Forced endgame:} an eight-level willingness ladder

`paper/sections/07-policy.tex:195-198`:

> Two further decisions belong to the policy rather than to the driver: which
> live teammate receives the turn when a declaration leaves a seat cardless, and
> the willingness bit the policy supplies at each level of the forced-endgame
> ladder of \S\ref{app:dialect-endgame}.

### The correction

The driver's ladder is implemented as described. **v0.4's willingness bit is inert**, so the
seven confidence rungs never select anybody, and every forced declaration falls through to
`bestGuess` — which names a capacity-infeasible allocation whose posterior probability under
the declarer's own belief is exactly zero. The paper describes an eight-level mechanism; what
runs is a one-level mechanism that loses with certainty.

The root cause is one line of arithmetic. `V04Agent::bestGuess` (`v04.hpp:762-794`) picks each
card's owner by an independent per-card `argmax` over `bel.marg[c][q]` with **no capacity
constraint**. The `BRIEF` already lists this as a known gap; the 100%-loss consequence, and the
fact that the *same* capacity-free argmax inside `evaluateSet` (`v04.hpp:599-606`) is what
zeroes the willingness rungs, were not documented anywhere in `docs/`, `paper/` or
`research/v04/`.

### The measurements

**Every forced-endgame declaration is wrong, and wrong for a nameable reason.**
`P2-forced-endgame.md` §1/§3, mirror, seed 777:

| | |
|---|---|
| forced declarations | 562 rows, **562 wrong (100%)** |
| named allocation violates the declarer's own capacity | **562 (100%)** |
| named allocation has exact posterior probability 0 | **562 (100%)** |
| named allocation puts a card at a seat the declarer's own `Knowledge` excludes | 0 (0%) |
| the declarer's own stated confidence at the moment of declaring | **exactly 0.0 in 562/562** |
| cards misnamed per declaration | 1 in 518, 2 in 44 — never more |
| the truth splits the unknowns over more than one teammate | **562 (100%)** |
| best *feasible* allocation would have been correct | **228 (40.6%)** |

Independently re-verified with instrumentation sharing **no code** with `diag.hpp` or the P2
probe: correctness recomputed from the opening deal and the replayed trace, `Event::success`
and `GameResult` ignored — **zero disagreements over 6,465 forced declarations across 10
configurations and 7 opponents** (`P2-verify-forced-endgame.md` §1). The check that needs no
model at all: the named allocation gives some seat more cards of the half-suit than it holds in
total, in 28/28, 118/120, 80/80, 92/92, 88/88, 90/90 across six mirror runs (§4).

**The 100% is v0.4-specific, not a property of the decision problem.** At seed 424242, in the
same games, `Baseline::Detective` gets 76.2% of its forced declarations right and
`Baseline::Lockout` 80.2%, while v0.4 is at 100% wrong against every opponent except `random`
(`P2-verify-forced-endgame.md` §3). The ceiling for any policy at these states is ~40%
(exact `BlockDP`: mean P(best feasible allocation) 0.397, and `P(true) == P(best feasible)` in
every single row — the posterior is flat over the surviving feasible allocations). v0.4 scores
0%. **The entire gap is the capacity bug, not inference quality.**

**The willingness rungs are inert, and the cause is the same argmax.** Intercepting the calls
`Game::forcedEndgame` actually makes, re-asking `V04Agent::willingForced` with threshold −1 and
testing **bitwise** against `0.0` (`P2-verify-willingness-rungs.md`):

```
willingForced calls intercepted    11802   (first rung 1686)
  pAlloc bitwise == 0.0            11802 (100% of ok)
  a willingness rung was ACCEPTED  0
  bestGuess (rung 7) invoked       562
```

Pooled over six configurations and 3,278 distinct (half-suit, seat) polls: **3,252 (99.21%)
have `pAlloc` bitwise 0.0**, and the ladder fired **2 times in 1,088 forced declarations
(0.18%)**. The emitting line is `belief.hpp:542` (`tmp.owner[c] != p`, after
`propagateCapacity` collapses the remaining cards onto the teammate the argmax did *not* name),
and the candidate overfills a teammate in **100%** of the zero cases.

**It is the zero, not the rung heights.** Computing `Belief::jointSequentialMAP` — the
capacity-feasible alternative that already exists in the codebase — at the identical states:
mean `pMAP` 0.390, positive at 100% of states, and **45.1%** clear rung 6 (θ ≥ 0.50). Had
`evaluateSet` scored a feasible allocation, the ladder would have selected a declarer on
roughly half of these polls.

`P8-coordination.md` reaches the same verdict from a third direction: four ladder shapes,
including a 0.10 bottom rung and no rungs at all, give **bit-identical output**, because the
statistic being thresholded is exactly 0 in **210 of 210** surveyed (player, live half-suit)
pairs. "The allocator is the bug, not the ladder."

### What the v0.5 paper should say

> The v0.4 dialect implements an eight-level willingness ladder, and the v0.4 paper describes
> it as the selection mechanism for the forced endgame. It never selected anybody. v0.4's
> willingness statistic is bitwise zero at 99.2% of the states the driver polls (n = 3,278 over
> six configurations), because `evaluateSet` scores a capacity-free per-card argmax that
> overfills a teammate in 100% of those cases and `jointSequential` correctly returns 0. Every
> forced declaration therefore fell through to `bestGuess`, which names that same
> zero-probability allocation: 314 distinct forced declarations in v0.4 mirror play, 100%
> wrong, misnamed by exactly the capacity overshoot (one card in 92%, two in 8%) every time.
> The decision was not hard — the exact posterior gives the best feasible allocation a mean
> probability of 0.397 and it is the truth 40.6% of the time. v0.5's M2 replaces the argmax
> with a capacity-feasible joint allocation and, in doing so, revives the ladder for free.

**Sample-size correction to carry forward.** `P2-forced-endgame.md` calls the seed-777
`--rotations=6` run "562 genuinely distinct declarations". It is not: in a mirror match the two
team orientations of a deal are byte-identical games, so it is **281 distinct declarations,
each counted twice**. The rate is unaffected (281/281 = 100%); combined with the verifier's
fresh seeds the independent distinct evidence is **314/314** (`P2-verify-forced-endgame.md`
§5). Quote 281 or 314, never 562.

**Also correct.** `P2-forced-endgame.md` §5 states that every v0.4 forced endgame has exactly
one live half-suit. At seed 1234567 two of 120 have two. True as a ~98% tendency, false as a
universal (`P2-verify-forced-endgame.md` §6).

---

## C4 — The fitting objective is a weighted mean, and the mirror was never in the panel

### The v0.4 sentences

`docs/FISHBOT_V04.md:128-130`:

> Cross-entropy method over a 34-coordinate vector, with common random numbers
> within a generation and fresh banks between generations. The objective is a
> **soft minimum** over the opponent panel rather than the mean, because the
> question is whether one policy can be best across playstyles, not on average.

`docs/METHODOLOGY.md:50-52`:

> Fitting
> maximises a soft minimum over the opponent panel rather than the mean, so the
> reported quantity is the worst case across playstyles rather than an average
> that can hide a collapse.

`paper/sections/08-fitting.tex:6-17`:

> The quantity we optimise is not the mean win rate against a fixed pool but the
> worst win rate across a panel of opponents, softened so that a noisy search can
> make progress on it. ... This tends to $\min_o \mathrm{wr}_o(w)$ as $\beta$ grows and is smooth in $w$ for
> finite $\beta$. The selection reported here used $\beta = 8$ ...

`engine/src/tuner.hpp:3-7`:

> The objective is deliberately NOT mean win rate against a fixed pool: a policy
> that averages well but collapses against one playstyle is not what we want.
> We optimise a soft minimum over the opponent panel ...
> which converges to min_o winRate_o as beta grows, and report the full profile.

`paper/sections/11-discussion.tex:36-39`:

> The fitting objective
> \eqref{eq:softmin} is a deliberate finite-panel approximation of it: a soft
> minimum over a hand-built six-opponent panel rather than a minimum over the whole
> policy space.

### The correction

Both halves fail.

**(a) At the temperature and the spread actually used, the objective is a weighted mean.**
The gradient weight on opponent *o* is `∝ exp(−β·wr_o)`. On the selected generation's own
profile (`research/v04/runs/selected.json`: 0.756 / 0.802 / 0.765 / 0.819, a span of 0.063),
the normalised weights are 0.3099 / 0.2145 / 0.2884 / 0.1872 at the reported **β = 8** — a
max/min ratio of **1.655**. At the tuner's default **β = 10** the ratio is **1.878**. For
comparison, β = 100 gives 544.6.

The sharpest form: `−(1/β)·log Σ_o exp(−β·wr_o)` on this profile evaluates to **0.60957**,
against `mean − log(4)/β = 0.61221`. That reproduces `selected.json`'s recorded
`"softmin": 0.6095652811544355` to seven digits and confirms β = 8 was the selection
temperature. The objective is within **0.0027** of an exact affine function of the mean. It is
not "closer to a mean than to a minimum" — over this panel it is a weighted mean with a
constant offset, and the weights vary by a factor of 1.66.

**(b) The mirror was never in the panel, in any recorded round.** The panel is
`v03, lockout, detective, v02` in `selected.json` (the same four are reused as the E5 ablation
panel at `experiments.sh:49`), `v03,lockout,detective,diversifier` as the `fish tune` default
(`main.cpp:191`), and six
opponents in rounds 2–5 per `research/v04/runs/README.md`. Direct check on the committed
traces — the minimum per-opponent win rate over **every generation of every round**:

| trace | generations with `winRates` | min per-opponent win rate |
|---|---:|---:|
| `tune-round1.jsonl` | 23 | 0.5786 |
| `tune-round2.jsonl` | 14 | 0.6000 |
| `tune-round3.jsonl` / `-final` | 15 | 0.6636 |
| `tune-round4.jsonl` | 12 | 0.6682 |
| `tune-round5.jsonl` (the shipping trace) | 12 | **0.7273** |

A self-play opponent sits at ≈ 0.50 by construction. **No recorded generation of any round has
any opponent below 0.5786**, and the shipping round never goes below 0.7273. The legacy
TypeScript optimisers have the same omission (`scripts/optimize-fishbot.ts:29-35`,
`scripts/refine-fishbot.ts:7-11`): no self-play term in either. (`P7-valuefn.md` §3.3.)

The panel is also unbalanced: every member is beaten by 72–99%, so the objective spends its
resolution on opponents v0.4 already dominates, and `hunter` at 0.97 and `diversifier` at 0.93
contribute almost no gradient.

### Why this matters, and it is not a nitpick

This is the **direct explanation** for why the P0 pathology survived fitting. The deadlock is a
mirror/strong-opponent phenomenon — 39.0% provably-dead asks and a 286-ask dead run in the
mirror, versus 2.8% and 5 against v0.3 (`P0-v04-pathology.md`). Nothing in the 34-parameter
objective ever evaluated the policy against itself, so no generation of CEM was ever penalised
for it. A candidate that deadlocks in self-play but converts against v0.3/lockout/detective/v02
scored exactly as well as one that did not. **The pathology was not traded away; it was never
priced.**

Note the value fit *was* run on self-play (`experiments.sh:116`), unlike the policy fit — but
that artifact is not the vector that ships (§C6), and a regression target cannot penalise a
deadlock in any case.

### What the v0.5 paper should say

> The v0.4 objective is described as a soft minimum standing in for the worst case across
> playstyles. At the temperature used for selection (β = 8) and the panel spread actually
> observed (0.756–0.819), the gradient weight ratio between the hardest and easiest panel
> member is 1.66, and the objective's value lies within 0.0027 of `mean − log|O|/β`. It is a
> weighted mean with a constant offset. Separately, no fitting or selection round ever included
> a self-play opponent: the minimum per-opponent win rate recorded in any generation of any of
> the five committed traces is 0.5786, and 0.7273 in the round the shipping configuration came
> from. The two facts together are the recorded reason the mirror pathology survived fitting.
> v0.5 puts the mirror in the panel and reports both a true worst case and minimax regret; a
> β = 8 soft-min over a panel containing a ≈0.50 opponent would weight that opponent 7.8× the
> next member, which is probably the intent but should be a decision rather than an accident of
> the spread.

---

## C5 — The value function: the fitted signal and the used signal are disjoint

### The v0.4 sentences

`paper/sections/07-policy.tex:133-141`:

> The fit reported as E14 has \numValueRows{} rows and an in-sample $R^2$ of
> \numValueRSq{} (RMSE \numValueRmse{}) on the same self-play corpus that
> generated the rows; no held-out evaluation of the value model was run.

(`\numValueRSq` = 0.2909.)

`paper/sections/06-locked.tex:129-131`:

> One reason
> the achievable effect is small is the resolution of the model class $V$ is drawn
> from: a linear function of \numValueFeats{} summary features caps what any rule
> built on differences of $V$ can extract.

### The correction

The v0.4 paper is right that the model class is the limit, and right that E14's `R²` is
in-sample and belongs to the artifact rather than to the deployed vector. What it does not say,
and what v0.5 must say, is **which part of the model carries the fitted signal**. The answer is:
one feature, and it is a feature that is *constant across the candidates the ask rule compares*.

Three findings, in increasing order of how hard they are to argue with.

**(a) The regression is effectively `bias + score differential`.** On 551,484 rows from 700
held-out mirror games with a **game-clustered** 5-fold split, the full 16-feature linear model
and the two-parameter model `bias + score differential` land within noise of each other, with
the two-parameter model marginally ahead; dropping `score differential` alone collapses the
held-out `R²` essentially to zero, while every other drop-one loss is ≤ 0.0021 and four
features have a *non-negative* drop-one loss (`P7-valuefn.md` §2.1).

> **Do not quote P7's specific `R²` values.** The verifier established that they were produced
> under a single hard-coded fold seed and move with it. The seed-robust statements are the
> *ordering* and the *drop-one structure*: the 16-feature model does not beat
> `bias + score differential` out of sample, and removing `score differential` destroys the
> fit. Quote those, and report a fold-seed-averaged `R²` with a spread if a number is needed.

**(b) The consequence, which is exact and needs no regression.** The policy consumes
*differences* of `V`, never its level. `score differential` (`f[1]`) is identical across every
candidate ask at a state — `chooseAsk` (`v04.hpp:467-483`) is a pure argmax at one state — so
the single feature that carries the fitted signal contributes **nothing** to ask selection. The
terms the ask rule does consume are precisely the ones the regression finds carry no outcome
information. This is a statement about the algebra of the call sites, not about a fold split.

**(c) Seven of the sixteen coefficients cannot matter, by construction.**

*Two features are algebraically the same feature.* From source: `f[2] = sumControl/9` where
`sumControl = Σ_active (2e_s − 1)` and `e_s = (1/6)Σ_{c∈s} P(our team holds c)`
(`v04.hpp:355-362`), so `sumControl = ourCards/3 − active` under any posterior consistent with
the public hand counts; and `f[6] = (ourCards − theirCards)/54`, where
`ourCards + theirCards = 6·active` over active half-suits, giving
`f[6] = (ourCards/3 − active)/9 ≡ f[2]`. Measured on 551,484 rows: `corr = 0.9999959`,
`mean |f2 − f6| = 7.9e-5` — the residual is Sinkhorn failing to enforce the capacity constraint
exactly. Only `w2 + w6` is identified *by the regression*; the split between them is arbitrary
to the fit. It is not arbitrary to the deployed policy: at the ask call sites the two features
take different perturbation arguments (`dControl` for `f[2]`, `dOur`/`dTheir` for `f[6]`,
`v04.hpp:454-455`), so they coincide only at the unperturbed state and their differences do
not. That is the gap the E14 substitution falls into — see §C6, where restoring `v2` and `v6`
alone closes ~84% of the loss.

*Five coefficients cannot change any decision.* `cfg.vw` is read at exactly one place
(`v04.hpp:373`) and `value()` is called at exactly five sites (`v04.hpp:454-455`,
`v04.hpp:665-669`). At all five, `f[0]`, `f[10]`, `f[11]`, `f[12]`, `f[13]` are recomputed from
`agg`/`pub` and take **no perturbation argument** (`v04.hpp:380`, `390-397`), so they are
identical across every pair of branches being compared and cancel in every difference.
Confirmed empirically with a shadow-agent probe fed identical public history: moving those five
coefficients by a factor of ~1,000–7,000 changes **0 of 31,788** declaration decisions, while a
0.5 change to `v1` changes 97 (`P7-valuefn.md` §2.3).

**(d) The state distribution excludes the states `V` decides.** Rows are pushed only
immediately before an ask and only when the mover has cards (`game.hpp:310-323`); **no row is
ever collected at a declaration decision point** — yet `declareByValue` (`v04.hpp:653-671`)
evaluates `V` at exactly those points. And the training set is skewed toward the pathology: the
longest 10% of games supply 23.4% of all rows.

**(e) Which half of `V` earns its keep — measured in the mirror, which E5 never was.**
`./fish ablate --ref=v04 --panel=v04 --games=250 --rotations=6 --seed=606061` (1,500 games per
arm, paired bootstrap over deals): deleting the entire ask-side expectimax is worth
**+0.006, CI [−0.016, +0.029]** — nothing — while reverting declarations to fixed thresholds
costs **+0.025, CI [+0.004, +0.047]**. The ask-side use of `V` — the side the deadlock lives
on — is the part not earning its keep. P7 flags the small bank; re-run at ≥1,000 deals before
quoting the magnitude.

### What the v0.5 paper should say

> The v0.4 value function is a 16-feature linear model of which one feature carries the fitted
> signal, and that feature is constant across the candidates the ask rule compares. Its
> differences — the only thing the policy consumes — are therefore built from terms with no
> measured relationship to the outcome. Two of the sixteen features are algebraically identical
> (`expected control ≡ card differential`, corr 0.999996, the residual being Sinkhorn error),
> and five more cancel identically at all five call sites, verified by a decision-equality
> probe in which perturbing them by three orders of magnitude changes 0 of 31,788 declaration
> decisions. A sixteen-term form with seven wasted terms is a nine-term model. The v0.4 paper
> correctly identifies the model class as the limit; what it could not see is that the class is
> smaller than sixteen and that the fitted and used signals are disjoint. Capacity is
> available if the term is wanted — a depth-3 gradient-boosted tree on the identical rows
> improves held-out `R²` by roughly two thirds relative — but only after rows are collected at
> the declaration points where `V` is actually consumed.

The gradient-boosted comparison shares P7's single hard-coded fold seed, so re-measure it
across fold seeds before the "two thirds" survives into the paper; the direction (a nonlinear
model on the same rows does materially better) is what the current evidence supports.

---

## C6 — The documented "known gap" about `V04Config::vw` is backwards

### The v0.4 sentences

`docs/FISHBOT_V04.md:150-152`, under "Known gaps":

> - The 16 value-function coefficients compiled into `V04Config::vw` are **not**
>   those in `research/v04/results/E14-valuefit.txt`; `freeze_config.py` writes only
>   the 34 policy parameters.

`paper/sections/12-limitations.tex:62-66`, under "Computational and model-class limits":

> No goodness-of-fit
> figure is reported for the deployed value coefficients: the in-sample
> $R^2 = \numValueRSq$ of E14 belongs to the artifact's ridge fit, whose
> coefficients are not the compiled ones, and neither vector was evaluated out of
> sample (\S\ref{app:params}).

`paper/sections/C-parameters.tex:181-187`:

> The two vectors come from different runs, and no step in the pipeline copies the
> second into the first. ... We
> record this rather than repair it, because repairing it would change the
> configuration that produced every number in this paper.

### The correction

The **fact** is correct and was disclosed in four places. The **framing** is not. Listing it
under "Known gaps" invites the reading that the shipped vector is a bug that costs strength.
It is the opposite: the compiled vector **beats** the E14-fitted vector against all nine
opponents, at two independent seed banks, and "repair it by pasting the fitted numbers in"
is measurably the wrong repair. The paper's decision at `C-parameters.tex:186-187` — record
rather than repair — was correct for a reason the v0.4 study did not know.

### The measurement

`./fish ablate --ref=v04 --variants="v04:vweights=<E14>" --panel=<one opponent> --games=500
--rotations=6 --seed=7788991`, one run per opponent (3,000 games per cell, paired bootstrap
over deal clusters). Δ = compiled − E14; positive favours the shipped vector.

| opponent | Δ, seed 7788991 | Δ, seed 424242 (independent verifier) |
|---|---:|---:|
| v04 (mirror) | +0.0290 | +0.0133 |
| v03 | +0.0203 | +0.0360 |
| lockout | +0.0180 | +0.0183 |
| detective | +0.0223 | +0.0220 |
| v02 | +0.0187 | +0.0207 |
| diversifier | +0.0323 | +0.0480 |
| hunter | +0.0167 | +0.0197 |
| bluffer | +0.0010 | +0.0020 |
| random | +0.0033 | +0.0023 |

**9/9 opponents favour the compiled vector at both seeds; 8/9 CIs exclude zero at the second
seed; the sign never flips.** A third mirror bank (`--games=600 --seed=13579246`) gives
+0.0253, CI excluding zero (`P7-valuefn.md` §1.2, `P7-verify-valuefn.md` §2).

**Mechanism, tested directly.** The 34 CEM policy parameters were fitted *with the compiled
`vw` in the loop*, so `valueWeight = 6.043`, `linearWeight = 0.767` and
`declareMargin = −0.034` are co-adapted to that vector's **scale**. The E14 vector has a much
smaller collapsed control block (`w2 + w6` = 0.2297 vs 0.8435), so substituting it silently
shrinks the expectimax term relative to the linear ask score. A hybrid — E14 everywhere except
`v2` and `v6` restored — closes ~84% of the gap and makes the difference statistically
indistinguishable from zero (+0.0067, CI [−0.0089, +0.0217], vs +0.0417 for E14 as fitted)
(`P7-verify-valuefn.md` §4).

**Correction to P7 itself.** P7 §1 says the largest loss is in mirror play. It is not: at seed
7788991 diversifier (+0.0323) exceeds the mirror (+0.0290) on P7's own table, and at seed
424242 the mirror is the *smallest* non-degenerate effect and the only CI including zero
(`P7-verify-valuefn.md` §3). The honest per-opponent statement is **"1–5 win-rate points, sign
stable"**, not P7's "1.7–3.2"; the deal-cluster bootstrap under-covers seed-to-seed variation.

### What the v0.5 paper should say

> The v0.4 documentation lists the divergence between `V04Config::vw` and the E14 ridge fit
> under "Known gaps", and the v0.4 paper records it as a reproducibility defect with no
> goodness-of-fit figure for the deployed vector. The reproducibility point stands. The defect
> framing does not: the compiled vector beats the E14 vector against all nine opponents at two
> independent seed banks, by 1–5 win-rate points with a stable sign, and roughly 84% of the gap
> is attributable to two coefficients whose sum sets the scale of the expectimax term relative
> to the fitted `linearWeight`. The 34 policy parameters are co-adapted to the compiled
> vector's scale; substituting the fitted vector without refitting them is a regression, not a
> repair. v0.5 removes the class of gap by putting the value coefficients inside the CEM vector
> so the freeze step covers them. `docs/FISHBOT_V04.md` should move this item from "Known gaps"
> to "Reproducibility notes", and the v0.4 limitations text should note that a goodness-of-fit
> figure for the deployed vector now exists (§C5).

---

## C7 — "an undeclared half-suit scoring nothing" is false in this harness

### The v0.4 sentence

`paper/sections/06-locked.tex:186-190`:

> The shipped alternative is a graduated escalation on the public event
> count: below a forcing horizon \eqref{eq:stop} decides, past it the policy
> declares any half-suit whose estimated allocation probability exceeds one half,
> and past a second horizon its best candidate whatever the estimate, an
> undeclared half-suit scoring nothing.

(Same justification at `E11-termination.md:31-32`: "since an unclaimed half-suit scores
nothing", and `v04.hpp:565-575`.)

### The correction

Inside the evaluation harness an undeclared half-suit does **not** score nothing. At the action
cap, `game.hpp:358-366` calls `adjudicateRemaining()`, and `game.hpp:271-287` awards each
unresolved half-suit to **the team physically holding the majority of its cards**, ties to the
holder of the lowest card. So the arithmetic behind the second stage is inverted: cashing at
`pAlloc ≈ 10⁻³` converts a half-suit the team would have been *awarded* into one handed to the
opponents.

### The measurement

On a wrong forced-horizon declaration, the declaring team holds a mean of **4.08 of the six
cards** — i.e. the majority, i.e. the set adjudication would have given it
(`P4-verify-forcing-horizon.md` §5, from `fish p4horizon`). The declaration is therefore not
"converting nothing into a coin flip"; it is converting a likely award into a certain loss.

The premise is only true of a rules dialect that awards nothing at the cap. The v0.4 study
itself changed the harness away from that dialect after the parameter vector was frozen —
`research/v04/runs/README.md` records neutral majority adjudication as engine change 1 — and
the justification text was never updated to match. The paper therefore contradicts itself
within two sections: `02-rules.tex:66-67` states the dialect correctly ("any residue
adjudicated neutrally by majority physical holding"), and `06-locked.tex:189-190` justifies the
second escalation stage on the premise that dialect abolished.

### What the v0.5 paper should say

> The unconditional second stage of the v0.4 forcing rule is justified in the paper and in the
> source by the premise that an undeclared half-suit scores nothing. Under the dialect actually
> evaluated, unresolved half-suits at the action cap are awarded by physical majority
> (`game.hpp:271-287`), and on a wrong post-horizon declaration the declaring team holds a mean
> 4.08 of the six cards. The stage converts a probable award into a certain concession. The
> premise appears to predate the harness's own change to neutral majority adjudication.

---

## C8 — "Only the willingness bit crosses between seats" is not an information-safety guarantee

### The v0.4 sentences

`paper/sections/02-rules.tex:37-38`:

> In both situations teammates negotiate openly but exchange
> nothing beyond willingness (\S\ref{app:dialect-endgame}).

`paper/sections/A-dialect.tex:217-219`:

> Teammates may negotiate
> openly but may exchange nothing except willingness to declare, so the driver
> implements the selection as a threshold sweep rather than a comparison of
> confidences.

`paper/sections/A-dialect.tex:231-232`:

> Only the willingness bit crosses
> between seats.

`paper/sections/A-dialect.tex:191-194`, on arbitration:

> Selecting the most confident proposer is deliberately not offered. Confidence is
> private to a seat, and comparing confidences across seats would move information
> between opponents as well as teammates; it is the same leak that the forced
> endgame was corrected to remove.

### The correction

A **threshold sweep is not one bit**. Willingness is a deterministic function of the
responder's private state given the public record, so for a transcript `R` of a ladder sweep,
`H(R | X) = 0` and therefore

```
I(X ; R) = H(R) − H(R | X) = H(R).
```

The bits an observer gains are exactly the observer's predictive entropy of the transcript. An
`r`-rung ladder publishes one of `r+1` values per candidate, so `I ≤ n_cand · log₂(r+1)` —
**3.17 bits per candidate** for the shipped 8-rung shape, 6.34 for a two-candidate decision. The
rules text ("cannot share any information other than their willingness") reads naturally as one
bit per candidate. The design substitutes a quantised confidence for a bit and calls it a bit.

### The measurement

`./fish coord --games=600 --seed=77 --leak` — 1,200 games, 168 multi-candidate decisions, 336
candidate reports (`P8-coordination.md` §3.2):

```
  rung distribution over candidates (n=336):
    r0=37.5%  r1=1.19%  r2=0%  r3=5.36%  r4=17.86%  r5=19.05%  r6=19.05%  r7=0%  r8=0%
  H(rung) = 2.18812 bits
```

**2.19 bits per candidate**, against a 3.17-bit combinatorial ceiling and the ~1 bit the rules
text licenses; six of the nine values are actually used and the mode carries only 37.5% of the
mass, so the realised leak is not far below the bound. A two-candidate transfer publishes up to
~4.4 bits about two teammates' hands, mid-game, to opponents who still hold cards.

**Honest scoping, which the v0.5 paper must keep.** This is measured on a *turn-transfer*
ladder of the shipped 8-rung shape, built by P8 to make the shape's leak measurable. In v0.4's
actual **forced endgame** the realised leak is zero — because the statistic being thresholded is
identically zero (§C3) and every candidate reports the same rung. v0.4's forced endgame is
information-safe only by virtue of being broken; the moment M2 makes the ladder functional, the
leak becomes real. And the two channels' leak profiles are inverted relative to their value:
the leaky channel (turn transfer, mid-game, live opponents) is the worthless one, while the
valuable-if-repaired channel (forced endgame) leaks to an audience that is cardless by
construction.

### What the v0.5 paper should say

> The v0.4 dialect justifies a threshold sweep over a confidence comparison on the ground that
> only a willingness bit crosses between seats. Willingness is deterministic given the public
> record, so the information an observer gains from a sweep is exactly the entropy of its
> transcript: `I(X;R) = H(R)`. An eight-rung ladder publishes up to log₂(9) = 3.17 bits per
> candidate, and a measured instantiation of that shape on a real multi-candidate decision
> publishes 2.19 bits (n = 336 candidate reports). v0.4's own forced-endgame ladder leaks
> nothing only because its statistic is identically zero. A v0.5 that repairs the allocator
> must also decide the rung count as an information-budget question, and must price the ladder
> at its measured entropy rather than at an assumed one bit.

---

## C9 — The ladder-restart rationale is inoperative

### The v0.4 sentence

`paper/sections/A-dialect.tex:232-234`:

> A consequence of restarting the ladder is that early, safer
> declarations resolve cards and thereby sharpen the allocations available for the
> later ones.

(The same intent is stated in the source, `fish.hpp:123-125`.)

### The correction

The choice never arises. Over every occasion a team went cardless with live half-suits (566
entries, seed 31), the number of live half-suits at that moment was **1** in every case, and
`nactive == 1` with `ordinal == 0` in 562/562 rows of the seed-777 table
(`P2-forced-endgame.md` §5). At seed 1234567, 118 of 120 have one live half-suit and two have
two (`P2-verify-forced-endgame.md` §6). There is essentially nothing to order, so all three
ordering counterfactuals in P2 are identical to the non-ordering ones by construction.

The structure is not impossible — a cardless team simply requires the other team to hold `6k`
cards — it just never happens because v0.4 cashes locked half-suits promptly.

### What the v0.5 paper should say

> The restart rationale is sound in principle and inoperative in v0.4 play: essentially every
> v0.4 forced endgame has exactly one live half-suit, so the ordering the restart is supposed
> to exploit does not exist. Any v0.5 change that makes the policy more patient about declaring
> — which the deadlock findings point toward — will start producing multi-set forced endgames,
> and the machinery should be built alongside the patience change rather than after it.

---

## C10 — The headline "worst case across playstyles" does not include the case that fails

### The v0.4 sentences

`paper/sections/11-discussion.tex:41-46`:

> \textbf{What the evidence supports.} Within the eight-policy panel of
> Table~\ref{tab:h2h}, one policy is ahead of every member simultaneously:
> \vfast's lowest win rate against any panel member is \numWorstCase\%, recorded
> against \vthree{} ...

(`\numWorstCase` = 75.07.)

`paper/sections/13-conclusion.tex:23-26`:

> The margin is declaration
> reliability rather than ask conversion: on the same games \vfast{} declared
> correctly \numVsVthreeDeclAcc\% of the time against \numVthreeDeclAccSame\% for
> \vthree{} ...

(`\numVsVthreeDeclAcc` = 98.55.)

### The correction

Neither sentence is false, and both are correctly scoped in their own text ("within the
eight-policy panel", "on the same games"). But the eight-policy panel does not contain the
opponent against which the policy fails, and the reader's natural generalisation — "this agent
declares correctly 98.55% of the time" — understates the error rate by a factor of seven
(1.45% against 10.44%).

### The measurement

Same policy, mirror play, `fish pathology`, 600 games, seed 31 (`P0-v04-pathology.md`). In a
mirror every declaration is a v0.4 declaration, so this is not a pooled figure:

| statistic | vs v0.3 (the reported case) | mirror |
|---|---:|---:|
| declaration accuracy | 98.55% (paper, E3 bank) | **89.56%** (564 of 5,400 wrong) |
| declarations at/after the forcing horizon | 0 | 768, **58.59% wrong** |
| forced-endgame declarations | 16, 100% wrong | 28, **100% wrong** |
| provably dead asks | 2.82% | **39.04%** |
| games with a dead run ≥ 6 | 0% | **34.33%** |
| longest dead run | 5 | **286** |

The v0.4 study's own standing methodology (`docs/METHODOLOGY.md:50-52`) says the reported
quantity should be "the worst case across playstyles rather than an average that can hide a
collapse". The panel construction (§C4) is what prevented that from happening.

### What the v0.5 paper should say

> v0.4's reported worst case across playstyles is bounded by an eight-policy panel from which
> the strongest available opponent — a copy of the policy itself — was excluded, at fitting, at
> selection and at evaluation. Against that opponent the same policy declares correctly 89.6%
> of the time rather than 98.6%, is wrong on 58.6% of the declarations it makes past its own
> forcing horizon, and spends 39.0% of its asks on questions it can prove will miss. A
> worst-case claim over a panel that excludes self-play is a claim about the panel. v0.5 makes
> the mirror a first-class member of both the fitting and the evaluation panel and publishes
> per-opponent, worst-case and regret columns.

---

## C11 — The cycling is attributed to "an earlier fitted configuration"

### The v0.4 sentences

`paper/sections/06-locked.tex:170-178`, Observation 3:

> An earlier fitted configuration of the policy, played against a copy of itself
> with no forcing rule, failed to terminate within the action cap in
> \numMirrorCycle\% of games at the 400-ask cap ... This is a
> measured property of that fitted configuration on that corpus, not a theorem
> about the rules dialect.

(`\numMirrorCycle` = 21.0.)

`paper/sections/13-conclusion.tex:56-58`:

> Termination is imposed by hand-set horizons rather than derived, and the
> observed cycling is a property of specific fitted configurations in self-play,
> not of the game.

`paper/sections/12-limitations.tex:66-68`:

> Termination is imposed by hand-set escalation
> horizons rather than derived, and a differently tuned pair of policies could
> cycle inside them

### The correction

The distancing is not warranted. The **shipped** configuration cycles, at essentially the same
rate, and it cycles *inside* the horizons.

- Shipped configuration with the forcing rule disabled: **22.5%** of mirror games hit the
  action cap (`P0b-forcing-dilemma.md`, `v04:force=2000` both sides, 240 games, seed 31), and
  **23%** at seed 777001 with `force=1000000` (`P4-verify-forcing-horizon.md` §6). The paper's
  21.0% for "an earlier fitted configuration" is the same phenomenon in the same place.
- Shipped configuration **with** the forcing rule active: the longest dead run is **286 asks**
  and 34.3% of mirror games contain a run of ≥ 6 (`P0-v04-pathology.md`). The cycle is not
  prevented by the horizons; it is terminated by them, at the cost in §C2. The limitations
  text's conditional — "a differently tuned pair of policies *could* cycle inside them" — is
  satisfied by the shipped pair.

### What the v0.5 paper should say

> The v0.4 paper attributes the observed non-termination to an earlier fitted configuration and
> notes that a differently tuned pair could cycle inside the horizons. Both readings understate
> the shipped policy's behaviour: with the forcing rule disabled the shipped configuration
> reaches the action cap in 22.5% of mirror games (23% at a second seed), and with the rule
> active it still produces dead runs up to 286 asks in 34.3% of mirror games. The horizons do
> not prevent the cycle; they end the game while it is running.

---

## C12 — The "silence" half of the policy prior

### The v0.4 sentences

`paper/sections/05-belief.tex:265-282`:

> That likelihood is playstyle-dependent: a player who has taken many turns
> and never asked in half-suit $H$ is, under any plausible policy, less likely
> to hold cards of $H$, and the rules alone say nothing about this.
> We implement a two-parameter family of such corrections as prior weights inside
> the same machinery,
> ... where $a_{p,H}$ counts $p$'s public asks in half-suit $H$, $a_p$ their total
> public asks ... Equation \eqref{eq:prior} is \emph{fitted}:
> $\theta$ and $\phi$ are two coordinates of the \numParams-coordinate vector
> optimised jointly with the rest of the policy.

### The correction

`θ` is load-bearing (see §S7 — the v0.5 brief's hypothesis that the prior should be deleted for
robustness **failed**). `φ` is not. As implemented it does not function as an independent
"silence" channel: it neither moves the posterior cells it targets nor changes the win rate,
and the manoeuvre the project owner used against the bot — deliberate silence in a half-suit —
is not the manoeuvre `φ` measures.

### The measurements

**The silence cells do not move.** Marginals bucketed by the two statistics `priorWeight`
actually uses, opponent at seat 1 (`P3-deception.md` §2):

| opponent at seat 1 | holder & has asked | **holder & silent** | non-holder & has asked | **non-holder & silent** |
|---|---:|---:|---:|---:|
| `v04` (control) | 0.6652 | 0.2284 | 0.3172 | 0.1767 |
| `silent` | 0.5875 | **0.2268** | 0.3142 | 0.1708 |
| `silent:tol=0.10` | 0.6055 | **0.2268** | 0.3205 | 0.1692 |
| `feint` | 0.6472 | **0.2358** | 0.3357 | 0.1856 |
| `withholder:k=6` | **0.5428** | **0.2330** | 0.3053 | 0.1740 |

The damage a deceiver does is concentrated entirely in the *has-asked* column (`withholder`
costs 0.122 of posterior mass there); the silence cells move by ≤ 0.007. And **78% of that
damage is structural** — the policy-agnostic posterior loses 0.095 of the same 0.122 — so no
choice of `θ`/`φ` recovers it.

**Deleting `φ` costs nothing.** `./fish ablate --ref=v04 --variants="v04:pphi=0"`: paired
Δ = **−0.30 pp, CI [−3.05, +2.40]**; the independent verifier reproduces it
(`−0.83 pp, CI [−3.33, +1.67]`) (`P3-deception.md` §4, `P3-verify-deception.md` §2). A 15-fold
sweep of `pphi` at fixed `θ` is flat inside its intervals; the whole cliff is in `θ`
(`P3-deception.md` §5).

**The statistic is the wrong one.** `φ` multiplies the *whole-game* count of asks elsewhere. It
cannot distinguish "silent about `S` for three turns" from "silent all game", and the two
statistics that *would* capture the owner's manoeuvre — asked-in-`S`-and-did-not-reply, and
turns since `p`'s last ask in `S` — are not derivable from what v0.4 keeps. `missCount[p][S]`
is maintained in `onEvent` (`belief.hpp:181`) and read by **nothing**
(`P3-deception.md` §6.1). Fitted `φ` is −0.133 against an empirical coefficient of −0.03 to
−0.07: over-weighted as well as mis-specified.

**Caveat, deliberately stated.** There is an appealing algebraic argument that `φ` is *exactly*
inert: `z = θ·a_{p,S} − φ·(A_p − a_{p,S}) = (θ+φ)·a_{p,S} − φ·A_p`, whose second term is
card-independent, i.e. a per-column diagonal scaling that Sinkhorn's capacity normalisation
(`belief.hpp:493-494`) removes at its fixed point. **Do not state this as an exact
annihilation.** The exponent is clamped to `[−2.6, 2.6]` (`belief.hpp:106`), a per-cell
nonlinearity, and the iteration budget is finite. A direct check for this document —
`./fish match --a=v04:ptheta=0.3966,pphi=0 --b=v04 --games=40 --rotations=2 --seed=31`, where
0.3966 = θ+φ at the shipped values — is *not* bit-identical to `v04 vs v04` (ask accuracy
30.59% vs 31.17%; declaration counts differ). The invariance is approximate, not exact. The
supportable claim is the measured one: **`φ` is free to delete.**

### What the v0.5 paper should say

> v0.4's policy-aware prior is presented as a two-parameter family, one parameter for "asked in
> this half-suit" and one for "took turns but never asked here". Only the first is load-bearing.
> Deleting the second changes the win rate by −0.30 pp with a CI spanning zero, a 15-fold sweep
> of it is flat, and the posterior cells it is meant to sharpen move by at most 0.007 under
> every deception archetype tested — including the deliberate silence the project owner used.
> The statistic is whole-game rather than time-local and cannot represent "silent about this
> half-suit lately"; the statistic that could (`missCount[p][S]`) is maintained by the belief
> and read by nothing. v0.5's M7 replaces the pair with a per-seat, time-local, data-biased
> response model. Note that the first parameter must be kept: removing both costs 4.6 points
> against deceptive opponents.

---

## N — Gaps the v0.4 study flagged as unmeasured, which v0.5 now measures

These are not corrections. They are the v0.4 study's own open items, closed. The standing
project preference is to produce the evidence rather than hedge the sentence, so these should
be reported as filled rather than repeated as limitations.

**N1 — the ask-floor firing rate.** `paper/sections/07-policy.tex:185-186`: "The rate
at which the floor fires was not measured." Measured: **39.0%** of all mirror asks are provably
dead (`P0-v04-pathology.md`), 41.4% on the DESIGN §0.2 census, and 51.2% in patient mirror play
(`P0b-forcing-dilemma.md`) — against starved turns, where no legal ask had a live possibility
at all, of **0.20–0.90%**. **99.3% of provably-dead asks are therefore voluntary**
(DESIGN §0.2).

**N2 — a goodness-of-fit figure for the deployed value coefficients.**
`paper/sections/12-limitations.tex:62-66`: "No goodness-of-fit figure is reported for the
deployed value coefficients ... neither vector was evaluated out of sample." Both are now
evaluated out of sample, on a held-out bank with a game-clustered split (`P7-valuefn.md` §2.1).
Report the fold-seed-averaged figure with its spread, not P7's single-fold numbers (§C5).

**N3 — the locked-half-suit ask channel.** `paper/sections/12-limitations.tex:55-57`: "The
asks inside a locked half-suit that only the owning team may make are a channel the fitted
configuration has no term for ...: unexploited here, not shown to be worthless." Now priced:
68.5% of such asks raise a teammate's exact allocation probability (mean +0.096); a greedy
certificate ladder resolves the half-suit outright in **82.9% of attempts, median 4 asks**
(pooled over three seeds); the exchange rate against the opponents' own gain is roughly 37:1 in
the sender's favour; and a donated turn costs 0.157 half-suits against a live opponent, 0.002
inside a stalled position. **Not worthless.** But also not free — see the scope note in §C1.

---

## S — v0.4 claims that survive scrutiny

A corrections list that only accuses is not credible. Each of the following was tested in the
v0.5 workflow with an explicit attempt to break it, and held.

**S1 — Theorem 1 (locked half-suits).** Verified at the level of the rules and in every
measurement. A locked half-suit cannot change hands and an opposing declaration of it awards it
to the owning team. The corollary that waiting carries no *ownership* risk is likewise correct.
The error in §C1 is the inference *from* the theorem, not the theorem.

**S2 — Support monotonicity, and the explicit refusal to claim probability monotonicity.**
`06-locked.tex:33-38`: "Theorem~\ref{thm:locked} does not assert that the probability of a
correct claim rises monotonically, and it does not ... What is monotone is the support." Exactly
right, and confirmed: asks outside a locked half-suit shrank its support in **0 of 20,898**
cases, while own-locked asks shrink it in 38.6% (`P1-verify-e11-information.md` §5).

**S3 — Observation 2, `06-locked.tex:53-66`.** "Ownership freezes; allocation information need
not ... Ownership monotonicity alone therefore does not establish that a position with only
locked half-suits is informationally frozen, and does not prove universal non-termination."
This is the correct statement, made in the paper, before v0.5 measured it. The same statement
appears at `01-introduction.tex:74-79`, `13-conclusion.tex:47-52` and
`docs/FISHBOT_V04.md:97-101`. **The paper is not the place the error lives.** §C1 corrects
`E11-termination.md` and `docs/V04_FINDINGS.md`, which contradict the paper they support.

**S4 — The exact posterior and its validation.** `fish oracle` re-run independently:
`named allocation prob max abs diff 0.000e+00 over 227062 checks`; `bestTeamAllocation` 1,679
checks, 0 inconsistent, 0 not argmax; `ORACLE PASS` (`P1-verify-e11-information.md` §4). The
belief reports `p = 0` correctly at every deadlock state examined; the deadlock is a policy
defect throughout.

**S5 — Lowest-seat declaration arbitration.** `A-dialect.tex:191-194` declines confidence
ranking on information-safety grounds. The cost of that choice is **+0.37 pp**, range +0.28 to
+0.45 across five seeds, pooled over 15,000 deals / 30,000 games — and a **clairvoyant**
arbitrator with ground-truth access is worth the same +0.35 pp, so the channel is saturated and
the constraint is nearly free (`P6-verify-arbitration-cost.md`). The design decision was
correct; v0.5 should not engineer for it.

**S6 — The cardless player choosing the successor unilaterally.** `02-rules.tex:33-34`. A
genuine multi-candidate transfer decision arises **0.148 times per game** and governs 0.92% of
asks; v0.4's unilateral pick already matches an omniscient oracle at 800 of 886 of them; and
handing one team a **ground-truth** chooser is worth `−0.0007 ± 0.0024` sets/game over 6,000
paired games (`P8-coordination.md` §1). The unexploited channel is worth nothing. Budget
nothing for it.

**S7 — The policy prior is load-bearing (the `θ` half).** `05-belief.tex:284-290` reports that
disabling the prior lowers the win rate and correctly scopes this as a frozen-policy
sensitivity. The v0.5 brief's hypothesis — that deleting it would be *more* robust against
deception — was tested and **rejected**: `ptheta=0,pphi=0` is worse against deceptive opponents
by **4.60 points, CI [2.63, 6.58]** (`P3-deception.md` §4, `P3-verify-deception.md`). The
exposure is over-weighting, not weighting.

**S8 — The paper's disclosure of its own approximations in the stopping rule.**
`06-locked.tex:96-109` states that `s ⊖ H` updates aggregates only, that the six-card reduction
is applied on both the correct and the incorrect branch although the true post-declaration
counts differ, and that player-specific features are not re-derived. All three read true
against `declareByValue` (`v04.hpp:653-671`), where `vRight` and `vWrong` differ **only** in
`scoreDiff`. The paper also declines to call the rule optimal (`06-locked.tex:98-99`). Correct
on every point.

**S9 — The declaration pre-gates are heuristics, not bounds.** Disclosed in the conclusion and
the known-gaps list with the measured false-negative count. Accurate self-report.

**S10 — The round-5 base seed is unrecoverable.** Disclosed in `research/v04/runs/README.md`,
`12-limitations.tex:70-72` and `G-reproducibility.tex:230-234`. Accurate self-report;
independently confirmed while auditing the traces for §C4.

**S11 — The forcing horizon does not misfire on healthy games.** The v0.5 brief hypothesised
that a long-but-healthy game would be forced to cash as hard as a deadlocked one. Tested and
**rejected**: 46/46 games reaching event 220 at seed 31 are genuinely deadlocked, and raising
the dead-run threshold to 80 leaves the split unchanged (`P4-policy-review.md` §D1). v0.4's
horizon placement is not the defect; its behaviour on arrival is.

**S12 — Three further v0.5 hypotheses that failed against v0.4, recorded so they are not
rebuilt.** A time-varying holding cost in `value()` (cuts events/game but leaves the tail
untouched, raises declaration error, loses 3.9 points head-to-head); confidence-ranked
declaration arbitration (S5); a turn-transfer willingness ladder (S6). See DESIGN §0.3.

---

## D — Corrections the v0.5 study must make to its own diagnosis reports

Every headline below was re-measured by an independent verifier at an independent seed. Where
the verifier corrected a magnitude, **the corrected figure is what the v0.5 paper must quote.**

| Report | Original figure | Corrected figure | Source |
|---|---|---|---|
| P1 §2.2 | own-locked asks informative 70.6%, mean ΔP +0.126, max +0.658 (seed 31) | **68.5%, +0.0957, +0.6640** pooled over 3 seeds, n = 2,880 | `P1-verify-e11-information.md` §3 |
| P1 §2.3 | ladder resolves 21/21 (100%), median 2 asks | **82.9% (68/82), median 4 asks** | ibid. §6.2 |
| P1 §2.2 | MAP flips toward truth 14.3 : 1 | **3.3 : 1** pooled; counter also spans non-locked asks | ibid. §6.3 |
| P1 §2.2 | "a half-suit the team **provably** owns" | ground truth, not provable: observer `pTeam` mean 0.068, 0/36 above 0.999 | ibid. §6.1 |
| P1 §1.1 | 9/14 long games with no lock (seed 31) | **41/62 (66%)** pooled over 4 seeds; and only **13.0%** of dead-run asks sit in a lock | `P1-verify-deadlock-lock-census.md` |
| P2 §1 | 562 distinct forced declarations | **281 distinct** (mirror orientations are byte-identical); 314 with fresh seeds | `P2-verify-forced-endgame.md` §5 |
| P2 §2 | zero emitted at `belief.hpp:543` (mask check) | **`belief.hpp:542`** (owner mismatch); 0 mask violations in 15k+ calls | `P2-verify-willingness-rungs.md` §2 |
| P2 §2 | `pAlloc` is exactly 0.0 at every state; the rungs never fire | **99.21%** of 3,278 states; rungs fire **2 / 1,088 (0.18%)** | ibid. §3 |
| P2 §5 | every forced endgame has exactly one live half-suit | ~98%, not universal | `P2-verify-forced-endgame.md` §6 |
| P7 §1 | compiled `vw` loses worst in mirror play, 1.7–3.2 pts | **mirror is mid-pack or smallest; diversifier is largest; "1–5 points, sign stable"** | `P7-verify-valuefn.md` §3 |
| P7 §2 | specific held-out `R²` values | single hard-coded fold seed; quote the ordering and drop-one structure, or a fold-averaged figure with spread | verifier |
| P8 §1.2 | post-transfer hit rate 85.1% vs a 34.2% baseline | phase-matched control is **78.1%**; transfer-specific increment **+7.5 pts** | `P8-verify-turn-transfer.md` |
| P4 §D1 | stage 2 costs 8.13 pp | **8.13 pp (seed 31) / 8.75 pp (seed 777001)**; `fix=64` isolation recovers 5.12 / 7.25 of it | `P4-verify-forcing-horizon.md` §4 |

Two further engine findings surfaced during the diagnosis that belong in the v0.5 paper's
engineering section rather than here, but must not be lost:

- **`BlockDP` instances alias.** `BlockDP::build` parks its tables in a `thread_local` buffer
  pool (`blockdp.hpp:83-97`, `175-176`), so a second agent's `build()` silently repoints the
  first agent's tables. Measured: `checks 294, mismatches 285`. Harmless under the default
  `Fast` belief; fatal for any v0.5 that adopts exact block beliefs
  (`P2-forced-endgame.md` §6).
- **Duplicate certificates accumulate.** `Knowledge::onEvent` appends a fresh `Disjunction` on
  every repeated ask without checking whether an identical one is held
  (`belief.hpp:168-171`), so a deadlocked observer's certificate store grows without bound
  (trace A: 6 → 26 → 46) carrying no information and making the block DP progressively more
  expensive (`P1-deadlock-forensics.md` §1.4).

---

## Reproduction

From `engine/` unless noted. Long runs are marked; the short ones suffice to re-derive the
headline of each correction.

```bash
cd engine && make

# C1  the deadlock is a two-question cycle in unlocked positions, and information is available
./fish deadlock  --games=20 --dump=0 --states=2 --stride=40 --seed=424243     # short, used above
./fish deadlock  --games=60 --dump=5 --states=3 --stride=40 --seed=31         # the P1 population
./fish vdeadlock --games=60 --dump=1 --states=3 --stride=40 --seed=777001     # support + entropy
./fish oracle    --games=25 --maxdeals=40000 --samples=500 --seed=555         # pAlloc validation

# C2  what the forcing rule costs, per opponent
./fish pathology --a=v04 --b=v04 --games=600 --rotations=2 --seed=31
./fish pathology --a=v04:force=2000 --b=v04:force=2000 --games=120 --rotations=2 --seed=31
./fish vhorizon  --seed=4242 --deals=200
./fish match     --a=v04:force=1000000 --b=v04 --games=400 --seed=777001      # LONG

# C3  the forced endgame and the willingness ladder
./fish forcedprobe --a=v04 --b=v04 --games=300 --rotations=2 --seed=31
clang++ -std=c++20 -O2 -Isrc src/probe_vforced_main.cpp -o /tmp/vforced -pthread
clang++ -std=c++20 -O2 -Isrc src/probe_vwill_main.cpp   -o /tmp/vwill   -pthread
/tmp/vwill --a=v04 --b=v04 --games=600 --rotations=6 --seed=4242 --threads=8

# C4  the fitting objective (no engine run needed)
python3 - <<'PY'
import json, math, glob
wr = json.load(open('../research/v04/runs/selected.json'))['winRates']
for b in (8, 10):
    w = [math.exp(-b*x) for x in wr]; Z = sum(w); w = [x/Z for x in w]
    print(b, 'ratio %.3f' % (max(w)/min(w)),
          'softmin %.5f' % (-math.log(Z)/b), 'mean-log4/b %.5f' % (sum(wr)/len(wr)-math.log(4)/b))
for f in sorted(glob.glob('../research/v04/runs/tune-round*.jsonl')):
    m = [min(json.loads(l)['winRates']) for l in open(f) if l.strip() and 'winRates' in l]
    print(f, 'min per-opponent win rate over all generations %.4f' % min(m))
PY

# C5/C6  the value function
./fish dumpvalue --a=v04 --b=v04 --games=350 --rotations=2 --seed=20260822 --out=rows.csv  # LONG
./fish shadow    --base=v04 --games=40 --seed=99001 \
   --variants="v04:v0=9;v04:v10=9;v04:v11=9;v04:v12=9;v04:v13=9;v04:v1=0.5;v04:v2=0.5;v04:v6=0.1"
./fish ablate --ref=v04 --variants="v04:vweights=<E14 vector, '|'-delimited>" \
   --panel=<one opponent> --games=500 --rotations=6 --seed=424242              # LONG, per opponent

# C8  the ladder's information leak
./fish coord --games=600 --seed=77 --leak

# C12  phi is free to delete, and the reparameterisation is not exact
./fish match --a=v04:ptheta=0.3966,pphi=0 --b=v04 --games=40 --rotations=2 --seed=31
./fish ablate --ref=v04 "--variants=v04:pphi=0;v04:ptheta=0;v04:ptheta=0,pphi=0" \
   --panel=v04,feint,silent,withholder:k=6 --games=400 --seed=20260822          # LONG
```

Seeds used for verification (424243, 777001, 20260822, 424242, 1234567, 4242, 13579246, 777) are
disjoint from every v0.4 fitting bank (20260821, 770077, 313131, 888111, 1357911) and from the
E1–E17 evaluation banks (90210, 515151, 606060, 717171, 828282, 838383, 848484, 31415).
