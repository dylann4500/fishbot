# K0 — the mechanical side-channel certification gate

*Dylan Nguyen, FishLab Research Project. FishBot v0.7, phase 3. Main repository, commit `0c021a3`
plus this change. Written so phase 6 can quote it.*

`THREAT-MODEL.md` §6.4 specifies six tests, S1–S6, and says of all six that **none of them reads
only existing artifacts — each needs harness plumbing**. No phase had built any of them. Phase 3's
brief requires the homogeneity constraint (§5, T7) to be *"checked mechanically … not by
inspection"*. This is that check.

Build: `engine/src/v07_side.hpp` (the gate), `engine/src/v07_cheat.hpp` (the planted cheats), one
command block in `engine/src/main.cpp` immediately before `bankdigest`, and a `v07x` branch in
`engine/src/factory.hpp` that reaches the cheats and nothing else.

```
./fish7 v7side --a=<spec> [--b=<opponent, default v06>] --games=N --seed=<bank> --threads=T \
               [--rotations=2] [--tests=s3,s4,s5,s6] [--s3nodes=3] [--s5nodes=6] [--s5draws=8] \
               [--json] [--out=FILE]
```

Three copies of `--a` are seated as one team against `--b`. Exit code **0 = CERTIFIED**,
**1 = NOT CERTIFIED**, 5 = sealed bank, 6 = the `mixSeed` inverse failed its round trip (which
would void the S4/S5 calibration, so the gate refuses rather than printing a hollow PASS).
`./fish7 v7side --help` prints the same summary.

---

## 0. The one-paragraph result

The gate implements the four pass/fail tests, covers **all four decision types** of §6.2 plus
`bestGuess` — no prior work in this corpus looked past asks — and is calibrated by **three planted
cheats, each of which fails exactly one of the four tests and passes the other three**. On both
training banks the incumbent `v06`, `v05` and the phase-2 contestation arm `v07:r12=25` come out
**CERTIFIED**, with **zero** irreproducible decisions in roughly 1.2 million decisions each.

The one exception is the **F-cheap test-time-search configuration**, which is the only member of the
panel that has ever produced a nonzero S6 count: it certifies on bank 7030001 (0/262,920) and
**fails on bank 7030002 by a single ask decision (1/264,051)**. Across four training banks at 400
deals each the rate is **1 irreproducible ask in ~138,000**, it appears only when the process does
other agent work on the same thread, and it is invisible to every existing command (the shipped
`match` runner is thread-count-invariant for this configuration). §3 below states exactly what is
and is not established about it.

---

## 1. What each test does, what it cannot see, what it costs

### S6 — seat isolation. The strongest mechanical test available here.

**Claim certified.** Each team seat's action is a pure function of (its own hand, the public event
stream, the rules, its reset seed) — at every one of the **five** entry points the `Agent` interface
exposes: `chooseAsk` (D1), `proposeDeclaration` (D2), `choosePassTarget` (D3), `willingForced` (D4),
and `bestGuess`, the terminal `-1.0` rung of the forced ladder and the holds-only-complete-sets path,
which §6.2's table folds into D2/D4 without naming.

**Method.** Not IPC. *Reconstruction.* At the end of each game, for each certified seat, a fresh
agent is built from the same spec string, handed `reset(seat, ownHand, rules, seed)`, and then fed
a replay of the public event stream **interleaved with that seat's own past decisions**, at exactly
the event indices where the live seat made them. It must choose the same action at every one. Any
difference is state not derivable from what the seat is allowed to know: cross-game memory, a shared
pool, a memoised cross-seat cache, a thread-local.

Two details are what make it bite.

* **The interleaving** is what makes the test sound for a *stochastic* policy. A seat's own past
  decisions are a function of its own past information, so replaying them is legal — and it advances
  the policy's private RNG exactly as the live run did. Without this, `v06:s1=1` would fail by
  construction, which is the trap §6.4's own note about S4 warns of.
* **The reconstruction runs on a freshly spawned thread, after the game.** The fresh thread gives it
  clean `thread_local`s. Running *after* the game time-shifts it relative to the live decision, and
  that is what catches a **process-global** static too: the reconstruction reads its teammates'
  end-of-game writes where the live seat read their mid-game writes. The `cheat=shared` control is a
  process-global `std::atomic` array precisely so this is not tested against the easy case.

**What it cannot see.** (a) Anything the seat *may* legitimately know — it certifies reconstructibility,
not legality of the inputs, so a policy that reads its reset seed clairvoyantly **passes S6**, and
the `cheat=seed` control demonstrates that (0/84,419 mismatches). S4 and S5 are what catch that.
(b) A convention carried entirely in the public transcript passes trivially, because the transcript
is an input to the reconstruction. S3 is what catches that. (c) A shared static whose value happens
to coincide between the live moment and the end of the game escapes at that node; only the aggregate
rate is meaningful.

**Cost.** Roughly 1× the game cost per certified seat, so ~3× a plain match, plus one thread spawn
per seat per game (~30 µs, negligible). Measured: `v06` mirror, 600 deals × 2 rotations,
`--threads=2` — see §2.

### S4 — stream independence. Catches E-1, the open deal-seed inversion.

**The channel.** `Game::setup` hands seat *p* the reset seed `mixSeed(s, p + 77)` where the same `s`
generates the deal, and `mixSeed(·, b)` is a bijection. Any policy can invert its own seed in closed
form, call `dealCards`, identify the post-deal rotation from its own hand, and read all six hands.
This is **open in the engine at `0c021a3`**. `v07_cheat.hpp` exhibits the inverse (splitmix64's
finaliser inverted step by step, then a modular inverse of `0x9E3779B97F4A7C15`) and the gate
verifies 2,000 round trips before it will run.

**Method (T10).** Each seat receives a stream drawn independently of the deal — keyed off a constant
of the harness (`0x51DE0CA11B`) and the deal *index*. **Implemented in the harness's own agent
wrapper rather than in `Game::setup`**: the wrapper substitutes the reset seed before the inner
policy ever sees it, which is exactly equivalent and leaves the shipped driver byte-for-byte
untouched. That is why the identity control below is clean.

Then, on the same deals:

* a **deterministic** configuration must produce a **transcript-identical** run — proof it consumed
  nothing from the seed. This is the strong form and it is the one that applies to `v06`, `v05`,
  `v07:r12=25` and both deterministic cheats;
* a **stochastic** configuration must show a paired win-rate difference containing zero *and* an ask
  hit rate that does not move by more than 3 points. Both are implemented. The seed cheat's ask hit
  rate collapses **87.62% → 53.41%** and its win rate drops **45 points** when the stream is
  decoupled, which is the signature.

**What it cannot see.** It bounds *use of the deal seed*, not conventions and not shared state. Its
stochastic arm has the resolution of a win-rate cell: at 400 deals × 2 the paired difference resolves
to roughly ±5 points, so it certifies "no clairvoyance-scale use of the seed", not "no use at all".
The deterministic arm has no such limit — transcript identity is exact.

**Cost.** Doubles the number of games played (both stream regimes on every deal). No per-decision
overhead.

### S5 — posterior invariance (clairvoyance).

**Why not the action-identity form.** §6.4 phrases S5 as *"resample the other five hands … and
require the seat's chosen action to be unchanged"*. **Inside this engine that form is vacuous, and
provably so.** An `Agent` is handed its own hand once, at `reset`, and a stream of public events
thereafter, and nothing else. Resampling the other five hands changes neither input, so every policy
passes by construction — including a clairvoyant one, whose clairvoyance is anchored to the reset
*seed* and not to the driver's hand array.

That is worth recording as a structural finding in its own right: **within the `Agent` interface the
reset seed is the entire hidden-information surface.** It is exactly why E-1 is the whole of the
clairvoyance threat, and why closing one channel closes the class.

**Method as implemented.** The posterior resampling is real and is the same construction v0.6's own
determinizer uses — `DealDP` for constraints C1–C4, `Belief::satisfies` for the C5 ask-legality
certificates by rejection. At a sampled ask, *J* worlds are drawn from the exact policy-agnostic
posterior over deals consistent with (public transcript, own hand), and the gate compares

> **P(the chosen ask lands | truth)** against **P(it lands | posterior draw)**.

A policy conditioning only on what it may see has these equal in expectation, because the truth *is*
a draw from that posterior. A clairvoyant policy beats its own posterior, and by a lot: the seed
cheat reads **88.25% vs 20.11%, a gap of 68.1 points**.

**The floor, stated because it bounds the resolution.** The sampler is policy-*agnostic*, and the
true deal is not drawn from the policy-agnostic posterior once the transcript is policy-generated —
this is the same channel phase 2 measured as ~2.0 bits/ask of transcript inversion. So a legitimate
policy carries a real positive gap. Measured on the incumbent:

| configuration | S5 gap |
|---|---|
| `v06` (3 copies, mirror), bank 7030001, 200 deals | **+5.98 pts** |
| `v06:ptheta=0,pphi=0` — its own policy prior switched OFF | **+6.37 pts** |
| `v06`, bank 7030002, 60 deals | +5.70 pts |
| `cheat=seed` | **+68.14 pts** |

Switching v0.6's explicit policy prior off does *not* remove the gap, so the floor is structural (the
agnostic posterior is the wrong measure), not a property of v0.6's parameter vector. **The threshold
is therefore set at clairvoyance scale, 20 points, and a gap between the floor and the threshold is
reported, not certified.** The gate also prints a **matched reference arm** — the identical statistic
at the opposing seats, which run `--b` — so the confound is measured on the same deals in every run
rather than asserted. In a mirror the two arms are exchangeable and the difference carries no
information; the output says so explicitly, the same caveat the corpus applies to a mirror win rate.

**What it cannot see.** §6.4 already says it: S5 holds the public transcript fixed, so **a convention
carried in the transcript passes it trivially**. It is an E-1-class test, not a convention test. It
also samples, so a dependence firing on a measure-zero set of posteriors survives.

**Cost.** ~8 posterior draws at up to 6 ask nodes per seat per game, ~1 `DealDP` build per node.
Small relative to S3 and S6.

### S3 — listening substitution. The decisive test for conventions.

**Rule-equivalence, made concrete.** At a node where a certified seat *j* asked card *c* of
half-suit *s* from target *t*, a substitute *c′* is admissible iff

* **(a)** *c′* lies in half-suit *s* and *j* does not hold *c′* — the ask stays legal for *j* and the
  same rule-forced certificates of §6.2's D1 row fire;
* **(b)** `(t holds c′) == (t holds c)` — the public success bit is unchanged and the turn moves the
  same way, so *G* is identical;
* **(c)** the observer holds neither *c* nor *c′* — a cheap sufficient condition for both being
  unresolved from the observer's side, so the observer's exact posterior does not separate them.

This is `research/v06/results/E8-ties.txt`'s bit-for-bit tie group made operational: 53.2% of v0.6's
ask decisions have two or more candidates numerically identical at the top, 93.1% of those are two
cards of one half-suit at one target, and the exact posterior separates 0.00% of them. A policy that
responds to this substitution is responding to an **arbitrary tie-break label** — **I-1** when it came
from sort or enumeration order (v0.6 resolves these by unstable `std::sort`, `v06.hpp:403-405`),
**I-2** when from the card-index encoding. Either way it is on the closed list of §6.1.

**The control that makes the number readable.** The same substituted public event is shown to a
**teammate** observer and to an **opposing** observer, and *both observers run the policy under
certification*, each at its own real information set. Both see the identical public action; the only
difference is which side of the table they sit on.

**The raw response rate is not the offence.** Substituting the named card changes a genuine public
certificate — "*j* lacks *c*" becomes "*j* lacks *c′*" — and every seat's posterior moves. The
incumbent's raw teammate response rate is ~27% and its opponent response rate ~22%. **The offence is
the asymmetry.**

**And the control is not perfect.** The game is not symmetric between a teammate and an opponent: a
teammate cannot ask the asker at all, and the substituted certificate bears directly on its own
team's half-suit completion. So a legitimate policy carries a nonzero floor. Measured:

| configuration | teammate | opponent | asymmetry |
|---|---|---|---|
| `v06` (= `cheat=none`), 200 deals | 26.74% | 22.32% | **+4.42 ± 1.68 pts** |
| `cheat=shared` | 18.68% | 15.14% | +3.54 ± 1.43 pts |
| `cheat=seed` | 20.41% | 9.88% | +10.53 ± 1.85 pts |
| **`cheat=conv`** | **81.79%** | **8.72%** | **+73.08 ± 1.33 pts** |

The threshold is **15 points** *and* at least 3 standard errors — an order of magnitude below the
convention cheat and three times above the measured floor. **An asymmetry between the floor and the
threshold is a measured signal to be reported, not a certified pass.** That band is S3's detection
floor and it is wide; a convention worth 10 points of teammate response would pass.

**What it cannot see.** §6.5's failure mode 1 in full force: S3 is **local**. It substitutes one
action and looks at the next decision. A convention spread over a *sequence*, where no single
substitution moves anything, passes. It also does not see off-transcript channels at all — the
`cheat=shared` control passes S3 (+3.54 pts, inside the floor) because nothing about it is carried in
the public transcript. S6 is what catches that.

### Not built

* **S1 (grounding budget)** and **S2 (signalling CMI)** are §6.4's *measurements*, not gates. Not
  built. S2 in particular is expensive to estimate well (§6.5 failure mode 4).
* **The I-2 relabelling test** (a consistent permutation of half-suit labels and of card indices
  within a half-suit leaves the game isomorphic, so a policy reading only game quantities must
  reproduce the relabelled transcript exactly). Clean and decidable; **not built**, out of budget.
  It would be the cheapest next addition and it would subsume part of S3's I-2 coverage with an
  exact rather than statistical criterion.
* **The E-3 check** (`Event::confidence` zeroed before `observe`, md5-identical transcripts
  required). Not built; §6.3 records that no policy reads the field and that this was verified by
  grep, which is inspection, not mechanism.

---

## 2. The panel, and the incumbent's verdict

Full output: `research/v07/results/K0-sidechannel.txt`, machine-readable
`research/v07/results/K0-sidechannel.jsonl`. Numbers quoted in this note are from that run.

### The positive controls — the part without which the gate proves nothing

200 deals × 2 rotations, opponent `v06`, `--threads=2`, **both banks**. Each cheat is reachable only
through the probe-only spec base `v07x`, which no tuner, battery or frozen vector in the corpus
produces.

| control | S3 | S4 | S5 | S6 | verdict |
|---|---|---|---|---|---|
| `v07x:cheat=none` (v0.6 exactly, under the probe name) | PASS | PASS | PASS | PASS | **CERTIFIED** |
| `v07x:cheat=seed` — inverts its reset seed to the deal, plays clairvoyantly | PASS | **FAIL** | **FAIL** | PASS | NOT CERTIFIED |
| `v07x:cheat=shared` — process-global board read across the three team seats | PASS | PASS | PASS | **FAIL** | NOT CERTIFIED |
| `v07x:cheat=conv` — reads a teammate's arbitrary tie-break label, ignores an opponent's | **FAIL** | PASS | PASS | PASS | NOT CERTIFIED |

**Each cheat fails exactly its designated test and no other, on both banks.** That is the thing to
take from the table: S3, S4/S5 and S6 are three genuinely different detectors and **none of them
subsumes another**. A gate that dropped any one of the three would certify one of these three cheats.

The magnitudes, bank 7030001 / bank 7030002:

* `cheat=seed`: S4 ask hit **87.62% → 53.41%** / **88.72% → 54.71%** under T10, win rate **−45 points**
  on both; S5 gap **+68.79** / **+71.19 pts** against a +6.16 / +6.50 incumbent. It **passes S6** with
  0/84,832 and 0/85,751 mismatches, which is correct — a seed-reading policy *is* a pure function of
  (own hand, public stream, reset seed). S6 is not its detector; S4 and S5 are.
* `cheat=shared`: S6 **7,914 / 137,737 (5.75%)** and **8,090 / 138,560 (5.84%)** decisions
  irreproducible, all of them asks (≈49% of ask decisions), in **400/400 games** on both banks. It
  passes S3, S4 and S5.
* `cheat=conv`: S3 asymmetry **+74.72 ± 1.34** / **+75.98 ± 1.25 pts** against a +3.96 / +3.47
  incumbent floor. It passes S4, S5 and S6 — it is a pure function of (own hand, public stream) and
  touches no hidden hand.

### The incumbents

600 deals × 2 rotations, opponent `v06`, `--threads=2`, both banks. Every cell CERTIFIED.

| configuration | S3 asymmetry (7030001 / 7030002) | S4 | S5 gap | S6 |
|---|---|---|---|---|
| `v06` (mirror) | +3.83 ± 0.95 / +4.48 ± 0.97 | DETERMINISTIC, 600/600 identical | +6.12 / +6.42 | 0 / 391,960 and 0 / 396,174 |
| `v05` | −0.30 ± 1.04 / −1.45 ± 1.04 | DETERMINISTIC, 600/600 | +6.56 / +6.18 | 0 / 396,679 and 0 / 397,881 |
| `v07:r12=25` | +5.02 ± 0.95 / +5.55 ± 0.94 | DETERMINISTIC, 600/600 | +5.76 / +5.95 | 0 / 403,830 and 0 / 406,499 |
| F-cheap search, 400 deals | +3.44 ± 1.28 / +5.90 ± 1.28 | stochastic, win diff −2.63 [−6.63, +1.63] / +0.63 [−3.38, +4.63] | +5.52 / +6.03 | **0 / 262,920 and 1 / 264,051** |

Two things are worth noting rather than skipping past.

* **`v05`'s S3 asymmetry is zero and `v06`'s and `v07:r12=25`'s are not.** v0.6 responds to a
  teammate's rule-equivalent substitution about four points more often than an opposing seat does,
  and the phase-2 contestation arm about five. Both are inside S3's measured floor and below its
  threshold, so neither is a certified finding — but the *ordering* replicates across both banks and
  it is the direction a listening convention would produce. Anyone building a phase-3 candidate on
  top of `v07:r12=` should re-measure it rather than assume it is noise.
* **`v07:r12=25`'s S5 reference-arm difference is +5.36 / +5.49 points** — the certified arm beats its
  own policy-agnostic posterior by that much *more* than the `v06` opponent arm does on the same
  deals. That is a strength signal, not a legality one (the contestation arm is a better predictor),
  and it is exactly why S5's absolute gap and its reference difference are both printed.

### Decision-type coverage

The `cheat=seed` panel is where the forced endgame is common (it wins fast, so a team goes cardless
often) and it is what exercises **D4**: **23,165 and 24,729 `willingForced` reconstructions, 0
mismatches**, plus 413 and 439 `bestGuess` reconstructions. `v06` in a mirror reaches the forced
endgame about six times in 800 games, so its D4 column at 600 deals carries only ~105 decisions —
thin, and that is a coverage limitation rather than a pass. Panel C exists for this: against `random`
and `hunter` a whole team goes cardless often, and `v06` there carries 420 and 168 `willingForced`
and 239 and 135 `choosePassTarget` reconstructions, all 0 mismatches. **D1 and D2 are saturated** —
the `v06` cells alone carry ~52,000 ask and ~344,000 declaration reconstructions per bank.

---

## 3. The one S6 mismatch, and what is and is not established about it

The F-cheap test-time-search configuration
(`v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26`) is the **only** member of the
panel that has ever produced a nonzero S6 count.

**What is established.** The mismatch is reproducible in kind and it depends on what else the harness
does on the worker thread. Same command, 150 deals, bank 7030002:

| flags | decisions | irreproducible |
|---|---|---|
| `--tests=s6` | 98,816 | **0** — and thread-count-invariant at `--threads=1`, `2`, `4` |
| `--tests=s5,s6` (S5 builds `DealDP` only) | 98,816 | **0** |
| `--tests=s4,s6` (S4 plays a second game per deal on the thread) | 98,792 | **1** ask |
| `--tests=s3,s6` (S3 constructs and replays agents on the thread) | 98,792 | **1** ask |

S3 and S4 produce the **identical** altered decision count, so the perturbation is not *what* the
extra work is, only *that* it happened on the thread. Across four training banks at 400 deals × 2
with `--tests=s3,s6` the count is 0, 0, 1, 0 — **1 irreproducible ask in ~138,000 asks**, or ~1 in a
million decisions of all kinds. The comparison configurations produce **0 in ~1.2 million decisions
each** over two banks.

**What this means for the legality claim.** At that rate it cannot move a win rate and it is not a
strength result. It is a legality result: it says the searching seat's action is **not** a function of
(own hand, public stream, reset seed) alone at a rate of order 10⁻⁵, and the residual dependence is on
state shared across seats and across games inside a process. That is **I-3** on §6.1's closed list.
It is invisible to every existing command — the shipped `match` runner is thread-count-invariant for
this configuration (`winRateA` 0.523333, `askAccA` 0.545112, 95.2233 events/game at `--threads=1`,
`2` and `3`; seed 7030002, 150 × 2), which is why nothing in the corpus has seen it.

**What is NOT established.** The mechanism. The pattern — perturbed by anything that builds a
`BlockDP`, not perturbed by S5, which builds only a `DealDP` — points at THREAT-MODEL **E-2**, the
`BlockDP` shared per-thread pool, which §6.3 flags as still open at the raw level with
`\vsixAliasRawMismatch` = 175 raw field mismatches against 0 query mismatches. But `blockdp.hpp`'s
`stamp`/`generation()` guard and its `gr.sReady = false` reset were both inspected and are in place,
so **the mechanism is something else in that layer and I did not find it.** `--reconinline` is
committed as the diagnostic that separates thread-local from cross-seat state; it did not separate
this one, because the effect is too rare to catch in a single cell.

**Recommendation.** Treat the search family as **certified with a caveat**, not certified, until this
is root-caused. A phase-3 candidate that carries the search should run `v7side --tests=s3,s6` on at
least two banks at 400 deals and report the count rather than the verdict. Anyone who does root-cause
it should record it against E-2 in `ADVERSARIES.md` §7.

---

## 4. Cost, and what a phase-3 candidate should budget

`--threads=2`, per the phase's CPU discipline. Roughly, on this machine:

| configuration | cell | wall clock |
|---|---|---|
| `v06` (fast, deterministic) | 200 deals × 2, all four tests | 17 s |
| `v06` | 600 deals × 2, all four tests | 51 s |
| `v05` | 600 deals × 2, all four tests | 56 s |
| `v07:r12=25` | 600 deals × 2, all four tests | 55 s |
| F-cheap (truncated search) | 400 deals × 2, all four tests | 118 s |

S3 and S6 dominate: S3 costs two full transcript replays per observer per sampled node (five
observers), S6 costs one full replay per certified seat. `--tests=s4,s6` is the cheap subset;
`--tests=s6` alone is the cheapest test with real detection power. F-mid-class configurations should
be run at a few hundred deals at most, and `--s3nodes=1` if they are slower still.

**Recommended standard cell for a phase-3 candidate:** `--games=400 --seed=7030001 --threads=2`,
replicated on `--seed=7030002`. That gives ~10,000 ask reconstructions and ~2,000 S3 teammate
queries, which puts S3's standard error near 1.3 points. At 600 deals the standard error is ~0.95.
The whole two-bank certification of a `v06`-speed configuration costs under two minutes; there is no
reason for a phase-3 candidate not to carry one.

---

## 5. Identity control

`fish7 v7side` adds a new command (`v7side`), two new headers, and a new spec base (`v07x`).
**No existing policy or driver path is touched** — `game.hpp`, `v05.hpp`, `v06.hpp`, `arena.hpp` and
`belief.hpp` are unmodified. T10 lives in the harness's wrapper, not in `Game::setup`; the cheats live behind `v07x`.
Proof rather than assertion:

```
./fish7 pathology --a=v06 --b=v06 --games=400 --rotations=2 --seed=31
```

run from the pre-change binary and from the post-change binary is **byte-identical**, with and
without `--threads=2` (`diff` clean). The reference figures are unchanged: 94.1925 events/game,
p99 120, max 131, dead asks 0.011781%, longest dead run 1, action-limit games 0, declarations wrong
2.55556%.

---

## 6. What this does and does not license

It licenses one sentence, and phase 6 should use exactly this one:

> Three copies of the configuration coordinating through legal play only was checked mechanically
> against `THREAT-MODEL.md` §6's side-channel definition — S3, S4, S5 and S6, over all four decision
> types, on an instrument whose three planted cheats each fail exactly one of the four — and the
> configuration passed.

It does **not** license "no side channel exists". §6.5's failure modes stand: S3 is local and has a
15-point detection band; the closed list I-1..I-3 is closed by enumeration, not by proof; S5 samples
and cannot see conventions at all; and **passing certifies the configuration, not the design** — a
different frozen vector needs a fresh certification, which now costs about a minute.
