# FishBot v0.7 — CANDIDATES: five architectures, what each was keyed to, and what survived

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`, built on `0c021a3` ("phase 2 v7").
Phase 3 of the v0.7 programme (`docs/v07/PHASE-PROMPTS.md`). Read with `docs/v07/THREAT-MODEL.md`
(what "exploitable" means), `docs/v07/INSTRUMENT.md` (what the instrument can see),
`docs/v07/ADVERSARIES.md` (what beats the frontier) and `docs/v07/SUBOPTIMALITY-LEDGER.md` (the
hypotheses). The session's working record, including every defect found and every battery that did
not finish, is `docs/v07/RESEARCH-LOG.md` §3.

Every candidate was developed in an isolated git worktree, gated before it was scored, and certified
against THREAT-MODEL.md's side-channel definition by a mechanical harness rather than by inspection.
Training banks only: 7030001 and 7030002 for evaluation, 7030004 for fitting, 7030003 for transfer.
The phase-5 holdout was not touched and the binary refuses it.

---

## 0. Summary

> **The verdict, stated plainly.**
>
> **One candidate of five survives, and it is a configuration change plus one small mechanism rather
> than an architecture.** K3 — the incumbent's own defect stack (`rtie=1`, the urgency escalation
> off) plus a termination rule that replaces v0.6's fifteen-point event-count cliff with a stall
> detector — replicates phase 2's **+1.91 [+1.28, +2.53]** over the deployed policy at 48,000 games,
> composes with the truncated search to **+2.60** over `v06` and **+1.14** over `F-cheap`, and passes
> every gate. It does not on its own beat phase 2's composite, and it should not be presented as
> doing so; its case is that it is **additive**, free at runtime, and fixes a defect rather than
> adding a mechanism.
>
> **The four kills are the more valuable half of the session**, because each closes a standing ledger
> entry with a measurement rather than an argument. L1 — rank 2, priority 0.48, the second-largest
> quantified channel in the corpus — is closed at **exactly zero**: its named mechanism does not
> exist. L5's fitting form is closed as a **certified negative**. C1′ is closed in the negative and
> the search's per-decision selection signal is shown to be ~84% winner's curse on its own
> Monte-Carlo draw. And the inherited conditional on the leaf evaluator is discharged — not by the
> new evaluator that was built for the purpose, which bought nothing, but by re-measuring the single
> underpowered cell the conditional rested on.
>
> **Nothing measured in this phase is certified.** Every pooled interval reported below has a lower
> bound under the C1 class detection floor of 1.53 points, which phase 2 established does not buy
> down with evaluation games. Each number here is a *measured edge*, and the distinction is kept
> everywhere.

| # | Finding | Where |
|---|---|---|
| **C1** | **The homogeneity constraint has been checked mechanically for the first time.** `fish7 v7side` implements THREAT-MODEL S3/S4/S5/S6, each calibrated by a planted cheat that fails exactly one test and passes the other three. `v06`, `v05`, `v07:r12=25` and every phase-3 candidate are **CERTIFIED** on both banks. E-1 — the open deal-seed inversion — is now closed **by measurement rather than by grep**: under an independent per-seat stream, 600/600 of v0.6's transcripts are bit-identical, so it provably consumes nothing from its reset seed. | §2 |
| **C2** | **K3 survives.** +1.91 [+1.28, +2.53] over `v06` (48,000 games, replicated), +2.60 composed with the search, +1.14 over `F-cheap` — where phase 2 measured urgency-off alone at −0.69. Mirror misdeclaration falls from 2.556% to 1.028%. Gate-clean and certified. Not certified as an effect: every lower bound is under 1.53. | §5 |
| **C3** | **A termination rule with a fifty-fold margin, established at zero game cost.** Over 800 mirror games and ~456,000 seat-events, v0.6 never produces a no-progress run longer than **6**; the frozen configuration produces **326**. Any threshold in [12, 60] is simultaneously unreachable in ordinary play and immediate in a freeze. With `stall=12` armed the mirror digest is **byte-identical** to the same configuration without it — the rule's cost in ordinary play is not small, it is identically zero, because it never runs. The 220-event clock it replaces has a fifteen-point cliff immediately behind it and no comparable margin. | §5 |
| **C4** | **Ledger L1 is closed at exactly zero, and its proposed fix is not a fix.** The joint argmax and the marginal-product argmax select the **same allocation at every one of ~210,000 recorded voluntary declarations**, across two banks and both urgency arms, with the joint score holding a strict opinion on 95.6–100% of genuinely ambiguous cases. `jointSequential`'s first chain-rule factor *is* `bel.marg[c][p]`, and the sequential re-Sinkhorn reweights the survivors without reordering them. | §4 |
| **C5** | **The exact joint maximiser is worse than the deployed approximation, measured on the declaration decision itself.** 0.96679 against 0.97822 and 0.96774 against 0.97914; paired McNemar +218 fixed, −454 broken, net −1.313 pp ≈ **−1.58 win-rate points**. And 72.4% [69.0, 75.8] / 74.4% [71.0, 77.8] of the L1 error class sits in flat-posterior states where the exact object is provably a coin flip. This is the second independent instance of the project's standing habit paying off: *"exact Bayesian inference" is an assumption to test.* | §4 |
| **C6** | **L1 is largely downstream of the urgency branch, so K2's and K3's numbers must never be added.** Deleting urgency removes **71%** of the entire L1 allocation-error class (1.88%/1.82% → 0.541%/0.533%) and drops the oracle ceiling for a perfect allocator from 2.45 win-rate points to **0.84** — below the detection floor. | §4, §9 |
| **C7** | **L5 is confirmed as an instrument and killed as an objective.** The design effect of deal clustering was measured for the first time (1.03 declarations, 1.01 allocation errors, 3.41 asks), and a one-point-equivalent effect resolves in **284–560 games on the declaration channel against 9,604 on the scoreboard** — L5's precision claim confirmed at 17×–39×. But all three self-oriented per-decision objectives, fitted at matched budget against a per-game control, produced **worse** policies: −2.26, −1.59, −1.20, every one replicated in sign, two clearing the floor as certified negatives. | §6 |
| **C8** | **Each per-decision fit moved its own proxy in the intended direction and lost.** `selfdecl` bought +0.39 pp of declaration accuracy by paying **−2.33 pp** of ask accuracy; `selfask` did the reverse. This is the v0.5→v0.6 fact — v0.6 *lost* 2.3 points of ask hit rate while *gaining* win rate — reproduced with the sign flipped at nearly the same magnitude. The two per-decision channels trade against each other, so a fitter holding 55 coordinates buys one out of the other. | §6 |
| **C9** | **Ledger C1′ closes in the negative: the tie group is real as randomisation and exactly zero as selection.** A learned re-ranker restricted to the bit-for-bit tie group scores +1.19 pooled over `v06` — indistinguishable from `v06:rtie=1`'s free hash tie-break at +1.14 — and **−0.01 ± 0.45 head to head against `rtie=1`** at 48,000 games, replicated. Nothing learned survives the free random tie-break. | §7 |
| **C10** | **The search's per-decision selection signal is ~84% winner's curse, and ~95.5% inside the tie group.** Setting every candidate's true advantage to exactly zero and feeding the capture's real standard errors through the engine's real LCB rule reproduces the search's deviation rate at 0.3207 against an observed 0.3221 — a 0.4% relative error on a quantity the null was never fitted to. Inside the tie group the rule applies `kappaTie = 0`, i.e. **no shrinkage at all**, which is exactly where its selection is most noise-dominated. | §7 |
| **C11** | **The leaf evaluator rebuild is killed by conversion failure, not by fit failure.** Between-candidate R² — the only component the paired LCB rule can consume — goes from **0.00108 to 0.07976** endgame and from **−0.01004 to +0.03668** full-game, a 74× improvement replicated out of sample on ~1.3M leaves. It buys nothing in play: +1.84 against the material control's +2.01, losing on both banks individually. `searchChangeRate` is 0.3205 / 0.3217 / 0.3237 for material / fitted / retuned — the LCB rule at κ=2.5 is nearly **leaf-invariant**. | §3 |
| **C12** | **The inherited conditional is discharged by re-measurement, not by a new evaluator.** INSTRUMENT I4 records full-game truncated search at +0.08, which is one n=4,000 cell at 50.08% [48.52, 51.62]. Re-measured, `depth=12` with no `maxq` is **+1.52 pooled** with v0.6's own leaf, and the corpus's `depth=24` cell re-runs at +1.02 [−0.55, +2.62]. The interval always contained +1.5. **Full-game truncated search already works with the leaf the conclusion said could not support it.** | §3 |
| **C13** | **The cost premise of the whole search question was mis-sized by two orders of magnitude.** INSTRUMENT's 242× (and the v0.6 paper's "three orders of magnitude") is **F-search**, the unrestricted configuration. `F-cheap` — the endgame-restricted operating point that is actually on the frontier — is **~3.2×** the blueprint on a common basis, which agrees with this session's independent 2-thread calibration of 2.95×. | §7, §9 |
| **C14** | **The `F-cheap` S6 anomaly is a defect in the gate, not in the search, and no strength number in the corpus is impugned.** Three candidates reproduced and bisected it independently: it is `v7side` leaking state between its own four passes, the audited decision *count* itself moves with execution context (264,037 / 264,051 / 264,061 / 264,075), and `fish7 match` is **bit-stable across thread counts** for both search and non-search play. | §2, §10 |

---

## 1. What phase 3 inherited, and the two places the brief had to be re-aimed

Phase 2's verdict is the frame for everything here: **nothing anyone has built exploits the v0.6
frontier beyond the detection floor**, the best measured exploitability of the deployed policy is the
in-class +0.79 [+0.48, +1.10] over 96,000 games, and that sits below the class's floor of 1.53. So
"the v0.7 case rests on beating the frontier, not on closing exploits". Phase 3 is therefore a search
for *strength that composes*, conducted under an exploitability charter.

Two of the brief's five items were keyed to antecedents that phase 1 and phase 2 had already
measured as false, and saying so is part of the result rather than a licence to skip them.

**Item (c) was conditional and neither antecedent holds.** The brief asks for "a stochastic policy
trading a measured amount of blueprint strength for unreadability" *if the white-box inversion
responder bites*, and "explicit partner reasoning" *if a coordination exploiter bites*. Phase 1
measured the inverter's contraction at ~2.0 bits per ask — the first such measurement anywhere — and
then measured what it is worth: **+1.52 unfitted, with the excess tracking planted-edge size within
±0.4 on all eight rungs**, which is the signature of a general strength gain and not of an
exploitation instrument. Phase 1's W2 closed the other half: no linear handicap adds more than 0.07
bits/ask, so within this policy family there is **no readability handicap for a stochastic policy to
buy**. On the coordination side, the A2 correlated fit reached +0.70 against a class floor of 2.31,
and ledger L7 — the Hanabi/SPARTA belief-corruption pathology — closed as a *negative*: one searching
seat is worth 20% of three, so three searchers are worth more than three times one. K3 therefore
re-aims item (c) at what phase 2 does indict, which is the incumbent's own machinery, and prices the
stochastic limb anyway rather than merely asserting it away (§5).

**Item (b)'s forced-endgame sub-lead was already dead on arithmetic and is now dead twice over.** The
brief names "forced endgame at 24.35% against a ~40.6% feasible ceiling". At v0.6's incidence closing
the entire gap is worth **0.016 win-rate points** (ledger L13), phase 2 closed it on both counts, and
K2 re-derived it from this session's own capture: 108 forced declarations in 20,000 team-games at
accuracy 0.2778, giving **0.030 points** to close the whole gap — the ledger's own figure corrected
upward by a factor of two and still 1/50th of the detection floor. One thing is new and it is a
negative worth keeping: **deleting urgency raises forced-endgame incidence about six-fold while
raising its accuracy from 0.278 to 0.460**, essentially closing the gap by itself. The single route
by which L13 could have returned — an adversary driving incidence up — is pre-empted by K3's change
rather than by any repair to the forced-endgame machinery.

---

## 2. The mechanical homogeneity gate (K0)

THREAT-MODEL §6.4 specifies six tests and records that **none of them reads only existing artifacts —
each needs harness plumbing, and no phase had built any of them**. The phase-3 brief requires the
three-copies constraint to be "checked mechanically … not by inspection". `fish7 v7side` is that
check, committed at `d1ec24f` before any candidate branched from it.

| test | what it catches | `v06`, bank 7030001 / 7030002 |
|---|---|---|
| **S6 seat isolation** | any state a seat's action depends on that is not (own hand, public event stream, rules, reset seed): cross-game memory, a shared pool, a cross-seat cache, a thread-local. Implemented by **reconstruction**, not IPC — a fresh agent, reset, then a replay of the public stream interleaved with the seat's own past decisions, on a freshly spawned thread, after the game. Covers all four decision types of §6.2 **plus `bestGuess`**, which no prior work in this corpus looked past asks to reach. | **0 of 391,960 / 0 of 396,174** irreproducible |
| **S4 stream independence (T10)** | E-1, the deal-seed inversion. Each certified seat gets a stream keyed off a harness constant and the deal index. | **DETERMINISTIC — 600/600 transcripts bit-identical** |
| **S5 posterior invariance** | hidden-hand leakage, rescored as a clairvoyance statistic (see below) | gap +6.12 / +6.42 pts, threshold 20 |
| **S3 listening substitution** | the encrypted convention itself: a teammate conditioning on an arbitrary tie-break label. Rule-equivalence made concrete as the bit-for-bit tie group, with an **opposing-seat control** — the raw response rate is not the offence, the asymmetry is | asymmetry **+3.83 ± 0.95 / +4.48 ± 0.97** pts, threshold 15 and 3σ |

**The calibration is the result.** Three planted cheats — a seed-inverting clairvoyant, a
process-global shared board, and a card-index parity convention — each fail **exactly one** test and
pass the other three, on both banks: `cheat=seed` moves S4's ask hit rate 87.62%→53.41% and S5's gap
to +68.79 against a +6.16 floor while **passing S6, which is correct and is the point**;
`cheat=shared` gives 5.75% of decisions irreproducible under S6 while passing S3, S4 and S5;
`cheat=conv` gives an S3 asymmetry of +74.72 ± 1.34 against a +3.96 floor while passing the rest. So
the three detectors are genuinely different and **none subsumes another — dropping any one would
certify one of these three cheats.**

**Two corrections to THREAT-MODEL that phase 6 should absorb.**

1. **S5 as §6.4 specifies it is vacuous.** "Resample the other five hands and require the action to
   be unchanged" cannot detect anything inside this engine, because an `Agent` receives its own hand
   once and a public event stream thereafter and nothing else — resampling the other hands changes
   neither input, so *every* policy passes, clairvoyant ones included. The structural fact worth
   keeping is the reason: **within the `Agent` interface the reset seed is the entire
   hidden-information surface**, which is exactly why E-1 is the whole of the clairvoyance threat and
   why closing one channel closes the class. S5 was re-scored as P(hit | truth) against
   P(hit | exact posterior draw) with a matched reference arm, and in that form it separates the seed
   cheat from the incumbent by more than tenfold.
2. **S5 has a large structural floor.** The incumbent's P(hit | truth) beats P(hit | policy-agnostic
   exact posterior) by ~6 points, and switching v0.6's own policy prior off does not remove it
   (+6.37). The floor is not the parameter vector; it is that the true deal is not drawn from the
   policy-agnostic posterior once the transcript is policy-generated — the same channel phase 1
   measured at ~2.0 bits/ask. Any threshold must be set above that floor, as this one is.

**Everything phase 3 built is CERTIFIED on both banks.** The one open item — the K3 composite's S3
cell, which the K3 agent ran at `--s3nodes=1` for cost and which came in at 11.92 ± 2.33 against a
threshold of 15, the closest any configuration came to failing — was re-run at full power and
**passes on both banks: +7.061 ± 1.357 and +8.645 ± 1.353** (`K3-side-composite-s3nodes3.txt`). The
under-powered point estimate was noise around a true value near 7–9.

**One ladder is worth carrying into phase 4 even though nothing on it is an offence.** The S3
teammate-versus-opponent asymmetry rises monotonically as a configuration stacks mechanisms that read
the public transcript harder: `v05` ≈ 0, `v06` +3.83/+4.48, `v07:r12=25` +5.02/+5.55, the K3
composite +7.06/+8.65 — against a threshold of 15. The margin is real and it is shrinking. Phase 4
should keep measuring it rather than assume it is permanent.

**The inherited `F-cheap` anomaly is a defect in the gate.** Phase 3 started with one panel member
NOT CERTIFIED — `F-cheap` on bank 7030002, one ask in 264,051. Three candidates reproduced and
bisected it independently and they converge: run `--tests=s6` alone at 2 threads and it is
**0/264,075, twice, bit-identical including the denominator**; at 1 thread it is 1/264,061; the
full four-test run gives 1/264,051. **The audited decision count itself takes four different values**,
so running S3/S4/S5 in the same process changes which games S6 then sees, and S6 is partly measuring
its own harness. The important half of the negative: `fish7 match` on that spec and bank is
bit-identical at 1, 2 and 3 threads on win rate, events per game, ask accuracy and declaration
accuracy. The phase-3 brief guessed this "bears on every search number in the corpus"; measured, it
does not. **Fix: `v7side` must run S6 in a clean process.** (One agent inferred the stronger claim
that no search number is bit-reproducible across thread settings; two independent direct
measurements of `match` contradict it, and the weaker statement above is the one the evidence
supports.)

A real defect was found while chasing it and then killed as the explanation:
`RolloutEngine::seatAgents()` — the function written to reset the rollout blueprints — **has no
caller anywhere in the tree**, while `Game::emit` feeds those blueprints `observe()` forever, so they
accumulate the events of every rollout of every decision of every deal. The fix (`rreset=1`) is not
the cause of the anomaly and is **inert in play**, byte-identical to six decimals. Which is itself a
lead: the rollout blueprints' accumulated observations have **zero measurable effect on the search's
output**, and whoever owns the search should ask what those blueprints are using at all.

---

## 3. K1 — the leaf evaluator, and the conditional it was sent to discharge

*Brief item (a). Ledger L9 (the leaf evaluator), L2 (search at F-mid), INSTRUMENT I4.* **Verdict:
partial — the evaluator rebuild is killed; the conditional is discharged by other means.**

**Premise.** The v0.6 conclusion conditioned re-opening test-time search on rebuilding the leaf,
because the present one "is algebraically close to a rescaling of the hit probability and cannot
support a depth-limited search". Phase 1 refuted that in the endgame regime (+2.19 with v0.6's own
leaf at `depth=12,maxq=26`) and reported it still binding full-game (+0.08). So the conditional binds
exactly where the search would have to run to be both cheap and general.

**The probe supported the premise emphatically, and that is why the kill is informative.** Fitting on
the reserve bank and scoring on the evaluation banks, over ~1.3M leaves in two regimes and three
banks, the **between-candidate R²** — the only component the paired LCB rule can consume — is:

| regime | `MaterialLeaf` (v0.6's own) | best 13-feature contrast fit |
|---|---:|---:|
| endgame (`depth=12,maxq=26`) | 0.00108 (oos 0.00664) | **0.07976** (oos 0.08424) |
| full game (`depth=12`, no `maxq`) | **−0.01004** | **+0.03668** (oos 0.03756) |

The material leaf full-game is *worse than predicting the group mean*. The probe also produced the
crispest available statement of L9: `leafFeatures` accumulates `f[12] = f[1] + λ·f[2]` **exactly**,
so v0.6's leaf is a two-feature linear function with one shipped constant λ = 1.0, and the
contrast-optimal ratio is λ_eff ≈ 0.13 — a 7–8× miscalibrated scalar.

**It converts to nothing.** Pooled against `v06`: fitted leaf **+1.84** against the material
control's **+2.01** endgame, losing on both banks individually; **+1.60** against **+1.52**
full-game, indistinguishable. `v7decide` gives the mechanism: `searchChangeRate` is 0.3205 / 0.3217 /
0.3237 for material / fitted / λ=0.13 at n ≈ 26,700, where 98/√n = 0.60. **A 74× better leaf changes
which 32% of searched decisions get overridden, not how many, and the swap is near strength-neutral.**
The only arm that moves `searchChangeRate` is the degenerate one — `leaf=0.0` drops it to 0.2937 and
costs 0.72 points. So the leaf matters *as a class* and the frontier between "v0.6's leaf" and "the
best linear leaf fittable" is flat. **The guarded LCB rule at κ = 2.5 is doing the work, and it is
robust to the leaf in exactly the way that makes improving the leaf pointless.**

**A methodological negative that cost the candidate its best-looking result.** A six-point λ sweep on
the reserve bank peaked at λ=0.13 with +2.92 against λ=1.0's +2.02. On the evaluation banks that
became +0.06 pooled, and full-game it did not replicate in sign. Six values swept on one bank, the
maximum taken; the evaluation banks charged for it. Sweep on 7030004, confirm on 7030001/2, and never
quote the sweep maximum.

**What the candidate did discharge, by re-measurement.** INSTRUMENT §4.3's full-game +0.08 is a single
n=4,000 cell on one bank at 50.08% [48.52, 51.62]. Re-measured on the training banks, `depth=12` with
no `maxq` is **+1.78 [+0.42, +3.16]** and **+1.25 [−0.30, +2.78]**, pooled **+1.52**, with v0.6's own
leaf; the corpus's exact `depth=24` configuration re-runs at +1.02 [−0.55, +2.62]. The intervals
overlap almost entirely and **always contained +1.5**. The inherited conditional's "still binds
full-game" rested on one underpowered cell. Full-game truncated search already works.

**It is still not the answer**, and the honest reason is cost, not soundness: +1.52 at 5.81 games/s
(2 threads) against `F-cheap`'s +2.01 at 17.24 — dominated on both axes, with `F-mid` at +2.52 and
the phase-2 composite at +4.40 above it. Its one claim is **generality**: it is the first
configuration measured to hold a positive, sign-replicated edge with the search engaged over the
whole game rather than only where the belief is sharp. If phase 4 wants a general search primitive
rather than an endgame one, this is the spec, and it should be scored at 12,000+ games a bank on a
quiet machine before anyone builds on it.

**What would have to be true for the kill to be wrong.** The function class was linear in the 13
features throughout; a non-linear evaluator could find between-candidate structure another order of
magnitude up — but the burden has moved, because at 74× it did not convert and `searchChangeRate`
says why. The strongest surviving objection is the **target**: the fit regressed on the final
differential under *blueprint* continuation, which is what the rollout plays; if the right target is
the value under the *search's own* continuation, the fit is off-policy and mis-specified. That is
cheap to test. And every cell ran at the corpus's κ = 2.5 — a better leaf deserves a lower deviation
threshold, so a joint (leaf, κ) sweep on the reserve bank with confirmation on the evaluation banks
is the one experiment to run next.

---

## 4. K2 — joint-posterior declaration allocation, and the ceiling on the second-largest channel

*Brief item (b). Ledger L1 (rank 2, priority 0.48), INSTRUMENT I3, register item V6-M9.* **Verdict:
killed, twice over, by the ledger's own stated kill condition and by a mechanism that does not exist.**

**Premise.** The whole of v0.6's margin over v0.5 is declaration accuracy; ~2.1 win-rate points of
misdeclaration remain; and **88.1% [81.0, 94.4]** of what remains is pure allocation error — the team
physically held all six cards and named the wrong teammate. The mechanism is identified in code:
`feasibleAllocation` (`v05.hpp:722`) enumerates the feasible assignments, rejects on certificate mask
and capacity, and then picks the winner by **a product of independent marginals**, scoring it jointly
only afterwards.

**The replay — L1's own "cheapest decisive experiment in this document", never run until now — killed
it in four minutes.** At every voluntary declaration, re-derive the half-suit three ways (shipped
marginal-product, joint argmax over the identical feasible set, and the exact posterior's shape from
a scratch `BlockDP`) and score all three against the deal the driver alone can see:

| quantity | result |
|---|---|
| `jointDiffersRate` | **0.00000** — 0/35,957 and 0/35,957 with urgency on, 0/26,812 and 0/26,808 with urgency off, 0/22,473 and 0/22,478 under `jalloc=1` itself. **~210,000 declarations, not one disagreement.** |
| `jointFixRate` / `jointBreakRate` | 0/783 and 0/750 wrong declarations; 0/35,174 and 0/35,207 correct ones |
| is it a tie artifact? | **No.** Of 8,813 genuinely ambiguous declarations, the joint score resolved **8,813 = 100.0% strictly**; urgency-on gives 7,418 ambiguous and 7,089 = 95.6% strict. 41.3% of declarations have ≥2 feasible allocations, so there was plenty to disagree about |

The reason is visible in the code once you look: **`jointSequential`'s first chain-rule factor is
literally `bel.marg[c][p]`**, and the sequential re-Sinkhorn after each conditioning reweights the
survivors without reordering them. L1's proposed fix is not a fix; it is the same function.

**The ceiling is the number to keep.** The exact joint maximiser — the object the ledger worried
might be *indifferent* — is not merely indifferent, it is **worse**:

| state class | share | shipped marginal product | exact MAP |
|---|---:|---:|---:|
| exactly one feasible allocation | 87% | 0.99846 | — |
| **flat** posterior, ≥2 allocations | 3% | **0.573** (urgency on) / 0.686 (off) | 0.500, provably |
| **non-flat**, ≥2 allocations | 10% | **0.9284** [0.9155, 0.9394] | 0.8220 [0.8036, 0.8391] |

Overall 0.96679 against 0.97822 and 0.96774 against 0.97914; paired McNemar on 17,978 declarations,
**+218 fixed, −454 broken, net −1.313 pp ≈ −1.58 win-rate points**. *The shipped rule beats exact
Bayes by 10.6 points precisely where exact Bayes has an opinion.* And **72.4% [69.0, 75.8] / 74.4%
[71.0, 77.8]** of the L1 error class sits in flat states where the exact MAP is a 0.518/0.514 coin
flip — verbatim the condition L1 named as what would kill it. **The uniform-deal posterior is not a
ceiling on this channel; it is below the floor the deployed approximation already stands on.**

**The cross-cut that changes how phase 6 must read the ledger.** Run in both urgency arms because the
brief insisted: **deleting urgency removes 71% of the entire L1 error class** (1.88%/1.82% →
0.541%/0.533%) and drops the oracle ceiling for a perfect allocator from **2.45 to 0.84** win-rate
points — below the 1.53 floor. L1 is largely a downstream symptom of the branch K3 owns. As a
by-product the same capture reproduced phase 2's urgency-off result through a completely different
channel: **+1.32 and +1.20 pp of declaration accuracy = +1.44 to +1.58 points**, against phase 2's
per-game +1.23 to +1.62.

**A lesson worth generalising.** `jalloc=1` is provably inert at declarations and still moves play —
−0.07 pooled over 48,000 games and +0.167 pp of mirror misdeclaration — because `feasibleAllocation`
also runs on candidate half-suits that are *never declared*, where the rescored `pAlloc` perturbs the
declare/don't-declare comparison. **A mechanism can be provably inert on the decision it targets and
active, harmfully, on a different one.** Future candidates touching a shared subroutine must measure
the subroutine's other callers.

**What would have to be true for the kill to be wrong.** That `jointSequential` is a materially
different object from the marginal product at a declaration — contradicted on ~210,000 decisions with
95.6–100% strict resolution. Or that "flat" is mis-defined: the predicate is the condition
`blockdp.hpp`'s own comment cites for lexicographic tie-breaking, and the claim is the weaker and
correct one — *no rule whose sole input is the uniform-deal posterior can do better*. The one live
route left is the **policy prior** (who asked for what), which the uniform-deal posterior discards by
construction and which the deployed marginals already partly capture (0.573 against a provable 0.500
in flat states). That is L11's territory, not L1's, and its whole post-urgency-off ceiling is 0.46
points.

---

## 5. K3 — the indicted defect stack, and a termination rule that is not a cliff

*Brief item (c), re-aimed at what phase 2 indicts. ADVERSARIES A3 (urgency), A4 (the cliff), §4H (the
one-switch defects).* **Verdict: live — the only survivor.**

**Premise.** Phase 2 hands phase 3 three measured defects in the incumbent: the `urgent` disjunction
costs v0.6 about 1.4 points and an adversary can only take 0.38 of it; `rtie=1` — replacing an
**unstable `std::sort` order that decides 53.80% of contested ask decisions** with a hash of the
public event stream — is worth +1.14; and the stack of both is +1.91, gate-clean, named by phase 2 as
"the configuration phase 3 should inherit". The catch is that `pressure()` is v0.5's **termination
guarantee**: switching the escalation off switches that off too, and in self-play the tail reaches
**405 events** against a fifteen-point cliff at 220.

**The mechanism, and its case was made at zero game cost.** `stall=K` escalates when a seat's own
*hard deduction state* stops changing: each seat hashes `owner[]`, `mask[]`, `unresolved`,
`handCount[]` and `setActive[]`, and a public event that changes the hash is progress. Setting
`stall=999` — a threshold no game can reach — arms the instrumentation without ever firing the rule,
so the distribution of no-progress runs can be read straight off ordinary play:

| configuration | median | p99 | max |
|---|---:|---:|---:|
| `v06` mirror, 800 games, ~456,000 seat-events | 3 | 5 | **6** |
| the K3 stack | 3 | 5 | **6** |
| the frozen configuration (`m1=0` + urgency-off) | — | — | **326** |

**A fifty-fold separation**, so any K in roughly [12, 60] is simultaneously unreachable in ordinary
play and immediate in a freeze. The 220-event clock has no comparable margin: one global threshold
with a fifteen-point cliff immediately behind it.

**And the rule's cost in ordinary play is identically zero, not small.** With `stall=12` or `stall=20`
armed, every non-diagnostic line of the mirror pathology digest is **byte-identical** to the same
configuration without the key — 95.6925 events/game, max 129, misdeclaration 1.02778% — with the rule
firing **zero times** across ~455,000–465,000 `proposeDeclaration` calls per configuration. On the one
configuration that does freeze, K=12 takes the tail from 405 events to 141, the longest dead run from
326 to 12, and action-limit games from 2 to 0, at the cost of two declarations taken under the stall
rung, **both correct**.

**Strength.** Replication was exact and the composition cell is new:

| configuration | opponent | n | edge | 95% CI | replicated |
|---|---|---:|---:|---|:--:|
| K3 stack | `v06` | 48,000 | **+1.91** | [+1.28, +2.53] | yes |
| K3 stack | `F-cheap` | 12,000 | +1.17 | [−0.08, +2.40] | yes |
| K3 stack + search | `v06` | 18,000 | **+2.60** | [+1.52, +3.67] | yes |
| K3 stack + search | `F-cheap` | 20,000 | **+1.14** | [+0.15, +2.11] | yes |
| private per-seat tie draw (`rtie=2`) | K3 stack (`rtie=1`) | 24,000 | −0.31 | [−1.20, +0.59] | yes |

The stack reproduces phase 2's +1.91 [+1.29, +2.54] as +1.91 [+1.28, +2.53]. Composed with the
truncated search — **the cell phase 2 named as the obvious next and did not run** — it beats
`F-cheap` by +1.14. And the configuration change *alone*, at zero throughput cost, reaches +1.17
against a frontier point costing roughly fifteen times the compute, where phase 2 had measured
urgency-off alone at **−0.69**. Mirror misdeclaration on a common base: `v06` 2.556%, `F-cheap`
2.250%, **K3 stack 1.028%**. Declaration accuracy across nine cells: K3 arms 0.9906–0.9914 against
`v06`'s 0.9789–0.9810.

**The stochastic limb priced out at exactly zero, in both directions — and that closes an open
question in the threat model.** A genuinely private per-seat tie draw loses nothing to the publicly
reproducible hash: −0.31 [−1.20, +0.59] over 24,000 games, both banks marginally favouring the random
arm. THREAT-MODEL §10 asks "What does H1 cost in Fish?" and records that nobody has measured it; at
this decision point the **Price of Uncorrelation is zero to within ±0.9**. But it also *buys* nothing,
since phase 1 left no readability handicap for it to purchase. **The recommendation is to keep the
deterministic rule — not because randomising is expensive, but because determinism is free and is
what makes S4's strongest form available** (400/400 transcript identity under an independent per-seat
stream).

**Gate and certification.** All three K3 configurations pass the commit gate (dead asks 0.0088–0.0203%
against `v06`'s 0.0118%, longest dead run 1, **zero** action-limit games, mirror p99 122–125 and max
125–129, nowhere near the 220 rung) and are **CERTIFIED** on both banks, S6 zero irreproducible
decisions in all six runs. The composite's S3 cell, the one open item, passes at full power (§2).
The stall rule does **not** rescue `m1=0`: it bounds that stack's tail from 405 to 141 events and its
dead run from 326 to 12, but the configuration still plays 1.71% provably-dead asks and still fails
gate rule 1. It was the stress case, never a proposal.

**What would have to be true for this to be the answer for v0.7.** Three things, and the candidate
should be described as a configuration change plus one small mechanism, never as an architecture.

1. **It must be read as additive rather than rival.** K3's composite is +2.60 over `v06`, which loses
   to phase 2's composite at +4.40 — as a standalone it does not beat the bar and must not be
   presented as doing so. But the four keys laid **on top of** the phase-2 composite beat that
   composite by **+1.42 [+0.18, +2.68]** — on one bank only, with no mirror gate on the combined
   configuration. That is the shape of the claim and it is the first thing phase 4 must replicate.
2. **The honesty level must hold.** Every pooled lower bound is under 1.53, so all of it is measured
   and none certified; the worst cell against `F-cheap` is +0.72 with an interval containing zero. A
   certified edge needs roughly four times the games.
3. **The termination rule's case is the strongest thing here and is nearly unconditional.** It holds
   if a threshold in [12, 60] keeps the fifty-fold separation on banks beyond the three tested, and if
   no future candidate produces long no-progress runs in ordinary play — a candidate reasoning over a
   longer horizon is the obvious way that could break, and the check costs nothing (`stall=999`).
   One caveat was found and the code comment corrected: the progress hash includes `handCount[]`, so
   a successful ask always scores as progress, and the monotone argument bounds only the
   no-new-certificate mode of non-termination. That happens to be the mode that occurs, but the bound
   that carries is empirical, not total.

---

## 6. K4 — the per-decision objective: confirmed as an instrument, killed as an objective

*Brief item (d). Ledger L5, ADVERSARIES A5 (the floor does not buy down).* **Verdict: killed, with
the instrument half confirmed and worth keeping.**

**Premise.** The v0.6 conclusion names the route — "a study that wants to find a quarter-point
mechanism will need a per-decision objective rather than a per-game one" — and phase 2 made it urgent
by showing the detection floor **does not buy down with evaluation games**: four times the power
bought 0.15 points of floor, not the 0.78 the scaling law predicts, and the +0.88 rung resolves *to*
zero. "A sub-1.5-point claim needs a better responder or a per-decision estimator, not a bigger bank."

**The instrument half is confirmed, and it acquired the correction it never had.** L5's arithmetic
silently assumes decisions are independent; they share a deal. Nobody had measured the design effect,
and nobody could have, because no artifact in the corpus prints the ask count. Measured over 48
independent blocks:

| channel | design effect | games to resolve a 1-point-equivalent effect at 2σ |
|---|---:|---:|
| win rate (the scoreboard) | — | **9,604** |
| declaration accuracy | 1.03 (`v06`) / 1.32 (`r12=25`) | **284–560** |
| allocation-error share | 1.01 / 0.73 | **244–299** |
| ask hit rate | 3.41 / 1.42 | — |

**L5's precision claim is right by 17× to 39×, and deal clustering eats almost none of it on the
declaration channel.**

**The objective half is killed, at matched budget, with the mechanism measured rather than inferred.**
Four fits — same starting vector, same common random numbers, same CEM hyperparameters, 21,600 games
each, only the objective differing — evaluated on both banks at 24,000 games:

| objective | pooled edge vs `v06` | its own proxy, on the evaluation banks | what it paid |
|---|---:|---|---|
| `win` (control) | **+0.40** | — | — |
| `selfdecl` | **−2.26** | declaration accuracy **+0.39 pp**, replicated | ask hit rate **−2.33 pp** |
| `selfask` | **−1.59** | ask hit rate **+2.10 pp**, replicated | declarations −0.34 pp |
| `selfalloc` | **−1.20** | allocation-error share −0.28 pp | — |

Every per-decision fit **moved its own proxy in the intended direction on the evaluation banks,
replicated across both, and lost**. Two clear the 1.53 floor as certified negatives. The ledger's own
conversion prices `selfdecl`'s +0.385 pp at +0.46 points; the sign is wrong and the magnitude off by
2.7. That conversion was fitted where declaration accuracy was the only thing moving — **a fitter
holding 55 coordinates holds nothing else fixed**, and what these fits did was buy one channel out of
the other. This is the v0.5→v0.6 fact (v0.6 *lost* 2.3 pp of ask hit rate and *gained* win rate)
reproduced with the sign flipped at nearly the same magnitude.

**Two probes before the fits, and one of them is a warning about the whole widened class.** Over the
strength-relevant range of the `r12` dose sweep the correlation between each proxy and strength is
strongly **negative** — r = −0.74 for ask accuracy, r = −0.83 for declaration accuracy. The apparent
positive correlation over all eight doses is one leverage point at dose 40, where the policy collapses
and everything falls together. **The proxy only agrees with strength once the policy is already
broken.**

**The surprise, and it is unexplained.** The per-decision objective was expected to make the fit
landscape visibly less flat. It did not: the share of generations in which the CEM could not beat its
own mean vector out of twelve proposals is 2/6 for the per-game objective at 300 decisions a cell, and
2/6 for `selfdecl` at 1,285 and 2/6 for `selfalloc` at 1,303. Only `selfask`, at 12,871, reached 1/6.
**Forty-three times the decision count bought essentially no search traction.** Precision and traction
are apparently different things here.

**What would have to be true for the kill to be wrong**, and the honest reading is that this is a
kill of one *form* of L5, not of L5. (1) **The budget**: at 21,600 games the per-*game* objective also
fails — it scored −1.34 at a second seed. At that rung a CEM in this class random-walks, and it walks
downhill. The test is the budget curve at phase 1's standard 96,000-game rung at two seeds. (2) **The
step size**: at `sigmarel=0.08` the widened coordinates get σ = 1.92 on a [−12, 12] range, so six
generations cannot travel from zero to `r12=25`, and every fit here ended with |r12| < 0.9. This
budget tests whether the objective helps *locally near `v06`*; it does not test whether either
objective can find the distant optimum an unfitted sweep already found. **The strongest remaining form
of L5 — a per-decision fit at a much wider σ for the same total games, which the design effect says is
affordable — has never been run.** (3) **The target**: all three objectives are rates over *all*
decisions, so each was free to pay for its gain out of a different channel, which is exactly how they
lost. A *conditional* objective restricted to the subset where a mechanism fires is a different
hypothesis and was not tested.

---

## 7. K5 — the corpus's first learned component, and what it found out about the search

*Brief item (e), the wildcard. Threat-model class C4, never built anywhere in this corpus.*
**Verdict: killed by the brief's own stated kill condition — and it closes a register entry on the way
out.**

**Premise.** C4 is the one class with the status line "never built; the corpus has no learned agent of
any kind". Large-scale self-play is unaffordable in one session; the affordable learned object with a
measured target is **amortisation of test-time search** — train a function of decision-time
information to reproduce what the search decides, deploy it at blueprint cost.

**The cheap probe killed it on the first pass, and the diagnosis is the contribution.** A conditional
logit over 55 decision-time coordinates, fitted on **88,502 searched decisions / 425,536 candidate
rows** from training bank 7030004 and held out by deal, predicts the search's choice at **0.6783** —
the blueprint-argmax baseline **to four decimal places**. It does this on the training set too, so it
is not an optimisation failure: the model learns "always take candidate 0".

Why: **the label is noise.** Where the search deviates, its winning LCB beats the runner-up by less
than one combined standard error **80.41%** of the time (median z = +0.41). So the candidate built a
null — set every candidate's true advantage to exactly zero, draw observed advantages from the real
recorded standard errors, apply the engine's real LCB rule:

| quantity | observed | winner's-curse null |
|---|---:|---:|
| search deviation rate | 0.3221 | **0.3207** (0.4% relative, out of sample) |
| apparent per-decision advantage | +0.13712 sets | **+0.11578 (84.4%)** |
| the same, inside the bit-for-bit tie group | +0.19111 | **+0.18250 (95.5%)** |

**~84% of what the search appears to be selecting on is winner's curse on its own Monte-Carlo draw,
and ~95.5% of it inside the tie group — where the rule applies `kappaTie = 0`, i.e. no shrinkage at
all.** This bears on every attribution of the search in the corpus and it cost zero games.

**The signal is not entirely absent, and the arithmetic closes.** Refitting as a regression on the
*signed advantage* rather than a classifier on the *choice* — the right move once you know the label
is noise — gives held-out R² = **+0.100**, and the deployed argmax realises +0.0187 sets of held-out
advantage against the search's (upward-biased) +0.1362. The search's non-curse residual is
+0.137 − 0.116 = +0.021. **The learned function recovers essentially everything genuinely predictable.
Everything genuinely predictable is about 15% of what the search looks like it is doing.**

**And then the cell that decides the candidate.** Deployed unrestricted the re-ranker is **−1.83**
pooled over 48,000 games, replicated, clearing the floor in the wrong direction — it deviates on 66%
of decisions, twice the search's rate, and outside the tie group the blueprint's ordering is real
information a 10%-R² signal cannot overturn. Deployed **inside the tie group only** it is **+1.19**
pooled, positive on both banks, gate-clean, certified. That looked live — until the control:
`v06:rtie=1`, a free hash-based tie-break with no learned content whatever, is already +1.14
[+0.52, +1.77]. Head to head, the learned re-ranker against `rtie=1` is **−0.01 [−0.46, +0.44]** at
48,000 games, replicated (+0.00, −0.03).

**This closes ledger C1′**, which has stood Open since v0.6: *is the ensemble result in the tie group
real?* **It is real as randomisation and exactly zero as selection.** Nothing learned survives the
free random tie-break, and `v06` is now the wrong control for anything that touches the tie group.

**A correction the corpus should absorb.** Measured back to back on a common basis, `F-cheap` is
**~3.2×** the blueprint, not 242× — the 242× figure (and the paper's "three orders of magnitude") is
**F-search**, the unrestricted configuration. This session's independent 2-thread calibration agrees
at 2.95×. **The cost problem that motivated the entire candidate is 3× for the operating point whose
decisions were actually distilled.** Even had the distillation worked it would have bought a 2.7×
speedup.

**What survives the kill regardless of the win rate:** the corpus now has a learned component, an
engine capture channel recording one labelled row per *candidate* of every searched decision (the
existing `DecisionRecord` channel records only the chosen candidate, which cannot fit a re-ranker),
and the first demonstration that **a policy whose function class is not fixed in advance passes the
mechanical side-channel gate** — S3 asymmetry +3.45/+4.43 unrestricted and +4.41/+4.91 tie-only
against the incumbent's +3.83/+4.48, S6 zero irreproducible decisions in over a million, S4 fully
deterministic. This was the one thing in the corpus that could plausibly have encoded a convention
nobody wrote.

**What would have to be true for the kill to be wrong.** The head-to-head would have to hide an effect
below 0.45 points — possible, but not a v0.7 case under this corpus's standards. Or the function class
would have to be the binding constraint — but you cannot learn a function of state that predicts a
Monte-Carlo draw, and the falsification is specific and affordable: **recapture at `det=48` or
`det=96` so the label's standard error halves or quarters; if R² then rises steeply and the null's
share falls, the noise attribution was wrong.** That is the single experiment to run next, and anyone
who runs it should capture from **F-mid**, fit the **value** not the **choice**, and measure against
`v06:rtie=1` from the first cell.

---

## 8. The common profile: worst case and minimax regret over the panel

*Pending — the battery is running as this section is written. See `research/v07/results/P3-profile-*.jsonl`
and `RESEARCH-LOG.md` §3 for the completed cells.*

---

## 9. What composes, what competes, and what must never be added

Phase 2 measured its own three mechanisms composing **sub-additively at 83% of their naive sum**. Phase
3 adds three constraints on how its numbers may be combined:

* **K2 and K3 are not independent.** Deleting urgency removes 71% of the L1 allocation-error class and
  drops its oracle ceiling from 2.45 to 0.84 points. Any accounting that credits a declaration-channel
  gain *and* the urgency-off gain is double-counting the same defect.
* **K3 composes with the search at a discount but composes.** Stack alone +1.91 over `v06`; stack +
  search +2.60; the search alone is +1.89. The two are largely the same margin — but the stack's +1.17
  against `F-cheap` and the composed +1.14 say the configuration change survives contact with a
  searching opponent, which urgency-off alone did not (−0.69 in phase 2).
* **`v06` is the wrong control for anything touching the tie group.** 53.80% of contested ask decisions
  are decided by an unstable `std::sort` order, and simply decorrelating it is worth +1.14. Any future
  mechanism that operates inside the tie group must be measured against `v06:rtie=1`, or it will claim
  a gain it did not produce — which is exactly what happened to K5's tie-only arm for twenty minutes.

---

## 10. What phase 4 inherits

**Instruments.**
* `fish7 v7side` — the mechanical side-channel gate, four tests, three calibrated positive controls,
  ~2 minutes per configuration. **Fix S6 to run in a clean process** before quoting its count for a
  searching configuration.
* `stall=K` / `stall2` — the deduction-state stall detector, with `stall=999` as free instrumentation
  that arms the diagnostics without ever firing the rule. Run it on every new candidate; it costs
  nothing and it is how the fifty-fold margin gets re-checked.
* The L1 declaration replay (three allocations plus the exact posterior's shape, scored against
  ground truth), `BlockDP::allocShape`, and the per-decision design-effect machinery.
* Three self-oriented per-decision KPIs in the fitter (`--kpi=selfdecl|selfask|selfalloc`) and an
  `allocErrDecls` counter reading L1's error class — useful as *estimators* even though fitting
  against them failed.
* The K5 search-capture channel: one labelled row per live candidate of every searched decision.
* A leaf-fitting harness (`v7leaffit`) and a non-perturbing continuation hook that records the leaf
  feature row and then keeps playing to the end, so a leaf's target is the realised continuation value.

**Facts phase 4 should treat as established.**
1. K3's four keys on top of phase 2's composite beat that composite by +1.42 [+0.18, +2.68] **on one
   bank, with no mirror gate on the combined configuration**. Replicating that is the first job.
2. L1 is closed at zero; the exact joint maximiser is worse than the deployed approximation; 72–74% of
   the error class is an information limit.
3. Fitting against a per-decision proxy loses. Estimating with one is 17×–39× cheaper. Keep the second
   and drop the first, unless the wide-σ form is tested.
4. The search's per-decision advantage is ~84% winner's curse; the tie group is worth randomising and
   not worth selecting in; `v06:rtie=1` is the control.
5. `F-cheap` costs ~3.2× the blueprint, not 242×.
6. The LCB deviation rule at κ = 2.5 is nearly leaf-invariant. A better leaf is not the lever; κ might
   be.
7. Full-game truncated search works with v0.6's own leaf (+1.52 pooled), at a dominated operating
   point.

**The partner-regime table the phase-4 brief requires has not been run** and nothing here substitutes
for it. Every strength number in this document is self-play or against a fixed opponent; the brief's
cross-play and partner-change checks are phase 4's.
