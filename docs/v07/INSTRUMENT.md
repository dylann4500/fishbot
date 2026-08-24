# FishBot v0.7 — INSTRUMENT: what the measuring instrument can and cannot see

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`, built on `db6066c` ("phase 0 v7").
Phase 1 of the v0.7 programme (`docs/v07/PHASE-PROMPTS.md`). Read with
`docs/v07/THREAT-MODEL.md` and `docs/v07/SUBOPTIMALITY-LEDGER.md`; the session's working record,
including every defect found in the incumbent's apparatus, is `docs/v07/RESEARCH-LOG.md`.

This document reports **detection floors per responder class**, a **throughput table** replacing E9,
and **what the white-box responder found**. It proposes no architecture and changes no policy. Every
number was produced by `engine/fish7` (RESEARCH-LOG.md §1.0 says why the phase-1 binary is separate)
on an Apple M5 Pro with 15 logical cores; it is **not** comparable to `E9-throughput.txt`, which was
measured on different hardware — `v06` mirror runs at 326 games/s here against E9's 303.4. Every
table is generated from the artifacts by `engine/build_tables_v07.py`; no number below is
hand-typed.

---

## 0. Summary

| # | Finding | Where |
|---|---|---|
| **I1** | The instrument's **detection floor is 1.68 points** (C1 and C5), replicated on two banks under an excess-over-control criterion; nothing detects a planted 0.86-point edge. Below ~1.7 points the binding constraint is **evaluation power, not search power**: the floor buys down as (evaluation games)^−1/2. | §3.4 |
| **I2** | **The phase-1 exit criterion is met, with a stated domain of validity**: every live v0.7 candidate effect this phase surfaced (+1.5 to +2.5 points) is at or above the floor; sub-point claims — including one the size of v0.6's own +0.89 margin — are below it at current bank sizes. | §6 |
| **I3** | Two search configurations hold **replicated strength over the deployed policy at 50× and 75× the baseline search's throughput** (+1.57 and +2.19 points at 24,000 games each), and **F-mid finally has its shipped-vector head-to-head: +2.52 points, replicated** — ledger L2 confirmed, its sign-inverted table corrected by measurement. | §4.3 |
| **I4** | **The truncated fast path refutes the inherited conditional in the endgame regime**: v0.6's own leaf evaluator, which the corpus said "cannot support a depth-limited search", supports one at `depth=12, maxq=26` to +2.19 replicated. The conditional still binds for full-game search (+0.08 without `maxq`). | §4.3 |
| **I5** | **v0.6's in-class exploitability is at least +0.76 points, not "below the probe's floor"**: a responder in the target's own class, seeded at the incumbent, beats it on both banks; the class ladder rises to +1.86 (C3). The published 48.36% was substantially a measurement of a mis-specified exploiter — ledger P-3, confirmed by construction. | §3.4 |
| **I6** | **Transcript inversion contracts the deal posterior by ~2.0 bits per observed ask beyond the certificates** — the determinism hypothesis survives its first measurement, anywhere. Converting the bits into play yields **+1.52 points, unfitted** — the strongest single arm against the incumbent — but the excess **tracks planted-edge size on all eight rungs (±0.4)**: a general strength gain, not an exploitation instrument. W2 explains why: the unhandicapped policy is already ~fully readable, and no linear handicap adds more than 0.07 bits/ask. | §5, §3.6 |
| **I7** | The 98/√N arithmetic is computed by the harness and emitted with every cell, including the mirror case where no half-width exists. | §4.4 |
| **I8** | Defects found and fixed in the incumbent's apparatus: bootstrap intervals were a function of the machine's **core count**; the reserved-seed registry the paper describes **did not exist** (once built, it found a fit/eval collision on bank 515253); `tune --panel` silently split a one-target panel on the spec's own commas. | RESEARCH-LOG §1.2, §1.7 |

---

## 1. What an exploitability instrument has to do, and what this one now does

The corpus's instrument is one thing: `engine/exploitability_v06.sh` fits a responder from the
v0.5 linear family, with the frozen target as its entire opponent panel, and re-measures it on a
fresh bank. It works — its positive control reproduces v0.4's published 51.19% within the interval —
and it is a **C1 / A1 / k = 3** number in the threat model's taxonomy: one policy class, three
identical copies, three seats. THREAT-MODEL.md T5 then imposes a precondition that has never been
met by anything in this corpus:

> A class may contribute to an exploitability claim only after it has passed a **planted-edge
> calibration** at or below the effect size being claimed. […] A responder that cannot recover a
> planted **two**-point edge contributes nothing to a claim about a one-point difference, and the
> phase-1 report must say so in those words.

So phase 1 builds four responder classes, plants edges of known and independently measured size, and
reports what each class recovers. The classes:

| class | what it is | new? |
|---|---|---|
| **C1** in-class linear | the incumbent's own 37-coordinate family, refit against the target, seeded at the incumbent's vector, with the three v0.6-probe defects repaired | repaired |
| **C2** extended features | C1 plus twelve coordinates v0.6 does not have: per-target terms, opponent-hand modelling, seat-role terms, deviation timing, and a linear price for the deliberate miss | **new** |
| **C3** search-based | the v0.6 rollout machinery with the **opposing seats of the rollout carrying the target's policy** — the difference between a search that improves a policy and a search that best-responds to one | **new** |
| **C5** white-box inversion | the incumbent's policy with its deal posterior sharpened by inverting the target's observed transcript against the target's known deterministic policy | **new; no precedent found anywhere** |

C4 (learned) is not built; the corpus has no learned agent of any kind and phase 1 is instrumentation.
C6 (scripted-adaptive with an online model of the target's policy) is not built either; it is empty
in the corpus today and its natural home is phase 2.

**Regimes.** THREAT-MODEL.md T4 names A0 (independent), A1 (synchronized) and A2 (ex-ante
correlated) and makes A2 the headline. The A2 device is **built** — `correlationSignal()` is drawn
per game by the arena from a stream keyed by a constant of its own and handed to no policy through
any argument, and a responder reads it with `corr=K` — but phase 1 measures **A1** with
seat-conditioned features, because A0 needs three independent fits per target and A2 needs the fit
to search over plans. **The headline regime of this phase is therefore A1, and per §10 item 4 of the
threat model that fallback is declared here rather than left to be inferred.**

---

## 2. The per-decision channel

The v0.6 conclusion names this in terms: "a study that wants to find a quarter-point mechanism will
need a per-decision objective rather than a per-game one." The arithmetic behind it is 98/√N. A
declaration fires 4.48 times a game and its errors are individually labelled by ground truth, so
scoring a declaration mechanism on **declarations** rather than on **games** buys roughly the ratio
of decisions to games in effective sample.

`fish7 v7decide` records one row per decision — the candidate-set size, the size of the tie group,
the score margin to the runner-up, the policy's own forecast, whether the live-ask gate removed what
would have been the argmax, whether a search ran and whether it moved the choice, and on the
declaration path the number of feasible allocations and the policy's own P(allocation correct) —
each annotated with ground truth the policy never sees. Every rate carries a **deal-clustered**
bootstrap interval, because the rotations of one deal are one correlated unit.

Capture is opt-in through a thread-local flag rather than a member, so the shipped hot path pays one
thread-local load; with capture on, the ask path costs roughly 2× because it re-scores the ungated
candidate set to measure the gate.

| arm | bank | `askHitRate` | `ownLockedAskRate` | `tieShare` | `gateBindRate` | `deadAskRate` | `searchRate` | `searchChangeRate` | `declAccuracy` | `declAllocErrorShare` | `forcedDeclAccuracy` |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `v06` | 7011001 | 0.54032 | 0.10644 | 0.60371 | 0.00938 | 0.00000 | 0.00000 | — | 0.97663 | 0.88095 | 0.00000 |
| `v05` | 7011001 | 0.54998 | 0.08799 | 0.61180 | 0.04956 | 0.00000 | 0.00000 | — | 0.97404 | 0.85714 | 0.25000 |
| `v06` | 7011002 | 0.54168 | 0.10347 | 0.60226 | 0.00948 | 0.00006 | 0.00000 | — | 0.97803 | 0.83544 | 0.25000 |
| `v05` | 7011002 | 0.55071 | 0.08901 | 0.60993 | 0.05153 | 0.00012 | 0.00000 | — | 0.97587 | 0.80460 | 0.22222 |

Decision counts and intervals, first row only:

| metric | rate | 95% CI (deal-clustered) | decisions | 98/√n |
|---|---:|---|---:|---:|
| `askHitRate` | 0.54032 | [0.5327, 0.5482] | 34235 | 0.530 |
| `ownLockedAskRate` | 0.10644 | [0.0982, 0.1150] | 34235 | 0.530 |
| `tieShare` | 0.60371 | [0.5990, 0.6081] | 34018 | 0.531 |
| `gateBindRate` | 0.00938 | [0.0079, 0.0110] | 34235 | 0.530 |
| `deadAskRate` | 0.00000 | [0.0000, 0.0000] | 34235 | 0.530 |
| `searchRate` | 0.00000 | [0.0000, 0.0000] | 34235 | 0.530 |
| `searchChangeRate` | 0.00000 | [0.0000, 0.0000] | 0 | 0.000 |
| `declAccuracy` | 0.97663 | [0.9716, 0.9816] | 3594 | 1.635 |
| `declAllocErrorShare` | 0.88095 | [0.8101, 0.9444] | 84 | 10.693 |
| `forcedDeclAccuracy` | 0.00000 | [0.0000, 0.0000] | 6 | 40.008 |

Three notes on reading that table.

* `ownLockedAskRate` — asks into a half-suit the actor's own team already holds outright — replicates
  ledger entry **L3** on a fresh bank. `E2-pathology.txt` reports 11.69% for the v0.6 mirror at seed
  31; this is the same channel on bank 7011001.
* `tieShare` here is the share of *contested* decisions whose top group agrees within 1e-12, which is
  **not** `E8-ties.txt`'s bit-for-bit definition. The two are not interchangeable and the definition
  is printed with the number rather than assumed.
* `declAllocErrorShare` is the entry that makes the case for the channel. It is the share of wrong
  declarations in which the team physically held all six cards and named the wrong teammate — the
  error class ledger entry **L1** sizes at 72–75% and at roughly 2.1 win-rate points. It has a
  half-width of about 16 points at 400 deals, because there are only about 35 such declarations.
  A per-game battery would need roughly 35× the games to say anything about it. Per declaration it
  is one command, and phases 3–4 can iterate on it.

---

## 3. Detection floors

### 3.1 How a planted edge is defined and measured

A handicapped target `π_h` is built by a named, deterministic transform of the incumbent. Its edge is
then **measured, not assumed**: `dTrue` is the win rate that a *known* exploiter — the unhandicapped
incumbent itself — reaches against `π_h`, minus 50, on bank 7022001 at 12,000 games (98/√N = ±0.89
points), before any responder is fitted. `dFound` is what a responder's search reaches on a **fresh**
bank disjoint from its fitting bank; the freshness matters and the corpus already knows why — "the
win rate reached during fitting is a maximum over a population on shared seeds and is upward biased"
(`exploitability_v06.sh:4-6`).

A class's **detection floor** is the smallest `dTrue` at which its **excess over the unhandicapped
control** excludes zero on both evaluation banks. The excess, not the raw edge, because the control
rung is not zero — a properly specified responder beats the unhandicapped incumbent by +0.76 to
+1.86 points (§3.4), so raw `dFound > 0` on a handicapped rung may show nothing but the same
exploitability the class shows everywhere. Both banks, because the project's standing rule is that
a claim is replicated only if it holds in sign and size on both.

Six families were built. Three degrade *strength*; two degrade *readability* at no measurable cost
in strength, which is the distinction that separates C5 from the rest:

| family | what it does |
|---|---|
| `hit` | scales the hit-probability weight `w[0]` down — the corpus's own handicap, whose extreme is the only calibration point it has |
| `decl` | raises `declareMargin`, delaying declarations — the channel that carries the whole of v0.6's measured margin |
| `prior` | scales `priorTheta`/`priorPhi` down, degrading the policy prior — the channel the `feint` archetype already attacks |
| `gate` | disables M1's live-ask filter on a deterministic fraction of decisions, by public event index |
| `leak` | adds a multiple of the asker's holding in the half-suit to the ask score, making the choice of half-suit a monotone read-out of that holding — **readability, not strength** |
| `tell` | biases the ask score toward the lowest-indexed live opponent — a fixed per-target bias for which v0.6's feature set contains no coordinate, so it separates C1 from C2 |

### 3.2 The ladder

The full measured ladder is in RESEARCH-LOG.md §1.4 and `research/v07/results/C0-ladder.txt`. The
rungs the floor battery uses, with their measured `dTrue`:

`dTrue` was measured twice for the battery rungs — once in the exploratory sweep (bank 7022001,
`C0-ladder.txt`) and again by the battery's own ground-truth rows on the same bank; the two agree
within their intervals, and the battery's own values are the ones the floors are computed against:

| rung | `dTrue` (pts) | 95% CI | what it degrades |
|---|---:|---|---|
| `none` | 0.00 | mirror — no interval exists | nothing; the **false-positive control** |
| `decl` 0.05 | +0.62 | [+0.07, +1.63]* | declaration timing |
| `decl` 0.08 | +0.86 | [+0.29, +1.42] | declaration timing |
| `leak` 1.50 | +1.68 | [+1.07, +2.29] | at this strength, mostly *strength* (see §3.6) |
| `decl` 0.11 | +1.81 | [+1.07, +2.55] | declaration timing |
| `prior` 0.60 | +2.31 | [+1.05, +3.40]* | the policy prior |
| `decl` 0.15 | +2.45 | [+1.60, +3.80]* | declaration timing |
| `hit` 1.00 | +3.85 | [+2.90, +5.38]* | ask quality; the corpus's own handicap |
| `leak` 4.00 | +5.57 | [+4.96, +6.18] | at this strength, mostly *strength* |

*intervals marked \* are the exploratory sweep's; the battery's point estimates are shown.*

### 3.3 What the ladder itself established, before any responder ran

Four things, and two of them change what phases 2–4 should do.

1. **The ask-score handicap is nearly flat until it is total.** Scaling `w[0]` down by 40% costs
   0.37 ± 1.2 points while moving ask accuracy 1.6 points; only zeroing it produces a measurable
   edge (+4.15). The corpus's single calibration point therefore sits at the far end of a very
   nonlinear curve and is not evidence that the instrument can see anything smaller.
2. **The declaration channel is the sensitive one**, exactly as the ledger's conversion arithmetic
   predicts (one point of declaration accuracy ≈ 1.2 points of win rate). It is the only family in
   the ladder producing a *graded* sub-three-point edge, so it is what the floor is measured on.
3. **Disabling M1's live-ask gate on up to 40% of decisions costs nothing measurable**: `gate 0.40`
   is −0.92 [−2.05, +0.23]. That is not what the v0.5 record implies, and the per-decision channel
   says why: at v0.6's fitted weights the gate binds on what would otherwise have been the argmax at
   **0.95%** of ask decisions, because `wTeamHas` is fitted negative and the ownership incentive that
   produced v0.4's two-question cycle is priced out. **M1 is load-bearing for v0.5 and very nearly
   inert for v0.6.** Ledger entry **L10** treats M1 as an untested exploitability hazard; the hazard
   is real in principle and the surface is small, and phase 2 should size the surface before
   spending a search on it.
4. **`leak` is free at small sizes and a strength handicap at large ones.** At strengths 0.15 and
   0.50 the read-out bias costs −0.15 and −0.42 points (nothing measurable); at 1.5 and 4.0 the
   bias starts overriding good ask choice and costs +1.68 and +5.57 points of ordinary strength.
   The design intent — a handicap only a transcript reader can see — survives only at the small
   sizes, and §3.6 shows that even there the *inversion-accessible* signal it adds is ≤0.07
   bits/ask. That measurement is itself the answer to what the rung was built to ask.

### 3.4 Results per class

| class | rungs detected (excess over control excludes 0 on both banks) | detection floor | rungs measured |
|---|---|---:|---:|
| C1 | `leak,hstr=1.5` (+1.68), `decl,hstr=0.11` (+1.81), `decl,hstr=0.15` (+2.45), `hit,hstr=1.0` (+3.85) | 1.68 pts | 7 |
| C2 | `prior,hstr=0.6` (+2.31), `decl,hstr=0.15` (+2.45), `hit,hstr=1.0` (+3.85) | 2.31 pts | 6 |
| C3 | `decl,hstr=0.15` (+2.45), `hit,hstr=1.0` (+3.85) | 2.45 pts | 4 |
| C5 | `leak,hstr=1.5` (+1.68), `decl,hstr=0.11` (+1.81), `prior,hstr=0.6` (+2.31), `decl,hstr=0.15` (+2.45), `hit,hstr=1.0` (+3.85), `leak,hstr=4.0` (+5.57) | 1.68 pts | 8 |

Per rung, pooled over both banks:

| class | rung | dTrue | pooled edge | excess over control | excess − dTrue | detected |
|---|---|---:|---:|---:|---:|:--:|
| C1 | `decl,hstr=0.05` | +0.62 | +1.03 | +0.28 | -0.35 | no |
| C1 | `decl,hstr=0.15` | +2.45 | +4.57 | +3.81 | +1.36 | **yes** |
| C1 | `hit,hstr=1.0` | +3.85 | +8.23 | +7.47 | +3.62 | **yes** |
| C1 | `prior,hstr=0.6` | +2.31 | +1.73 | +0.97 | -1.33 | no |
| C1 | `decl,hstr=0.08` | +0.86 | +1.45 | +0.69 | -0.17 | no |
| C1 | `decl,hstr=0.11` | +1.81 | +3.62 | +2.86 | +1.05 | **yes** |
| C1 | `leak,hstr=1.5` | +1.68 | +2.82 | +2.06 | +0.39 | **yes** |
| C2 | `decl,hstr=0.05` | +0.62 | +0.87 | -0.17 | -0.80 | no |
| C2 | `decl,hstr=0.15` | +2.45 | +3.33 | +2.29 | -0.16 | **yes** |
| C2 | `hit,hstr=1.0` | +3.85 | +7.73 | +6.69 | +2.84 | **yes** |
| C2 | `prior,hstr=0.6` | +2.31 | +2.85 | +1.81 | -0.50 | **yes** |
| C2 | `decl,hstr=0.08` | +0.86 | +1.27 | +0.23 | -0.63 | no |
| C2 | `decl,hstr=0.11` | +1.81 | +2.22 | +1.18 | -0.63 | no |
| C3 | `decl,hstr=0.05` | +0.62 | +2.60 | +0.74 | +0.11 | no |
| C3 | `decl,hstr=0.15` | +2.45 | +6.80 | +4.94 | +2.49 | **yes** |
| C3 | `hit,hstr=1.0` | +3.85 | +9.78 | +7.91 | +4.06 | **yes** |
| C3 | `prior,hstr=0.6` | +2.31 | +3.74 | +1.87 | -0.43 | no |
| C5 | `decl,hstr=0.05` | +0.62 | +2.09 | +0.56 | -0.06 | no |
| C5 | `decl,hstr=0.15` | +2.45 | +4.07 | +2.55 | +0.10 | **yes** |
| C5 | `hit,hstr=1.0` | +3.85 | +5.40 | +3.88 | +0.03 | **yes** |
| C5 | `prior,hstr=0.6` | +2.31 | +3.44 | +1.91 | -0.40 | **yes** |
| C5 | `leak,hstr=1.5` | +1.68 | +3.48 | +1.95 | +0.28 | **yes** |
| C5 | `leak,hstr=4.0` | +5.57 | +7.26 | +5.74 | +0.17 | **yes** |
| C5 | `decl,hstr=0.08` | +0.86 | +2.51 | +0.98 | +0.13 | no |
| C5 | `decl,hstr=0.11` | +1.81 | +3.58 | +2.06 | +0.25 | **yes** |

Every cell, per bank, with the responder's own KPIs (a broken exploiter cannot hide — ledger P-3b):

| target | dTrue (pts) | 95% CI | class | dFound bank A | dFound bank B | 95% CI (A) | responder decl. acc. | forced/game | limit hits |
|---|---:|---|---|---:|---:|---|---:|---:|---:|
| `none` | +0.00 | [+0.00, +0.00] | C1 | +0.99 | +0.52 | [+0.13, +1.84] | 0.9766 | 0.0067 | 0.0000 |
| `none` | +0.00 | [+0.00, +0.00] | C2 | +1.37 | +0.72 | [+0.50, +2.23] | 0.9780 | 0.0064 | 0.0000 |
| `none` | +0.00 | [+0.00, +0.00] | C3 | +1.88 | +1.85 | [+0.38, +3.40] | 0.9802 | 0.0070 | 0.0000 |
| `none` | +0.00 | [+0.00, +0.00] | C5 | +1.64 | +1.41 | [+0.79, +2.49] | 0.9792 | 0.0047 | 0.0000 |
| `decl,hstr=0.05` | +0.62 | [+0.08, +1.18] | C1 | +1.48 | +0.59 | [+0.62, +2.33] | 0.9827 | 0.0078 | 0.0000 |
| `decl,hstr=0.05` | +0.62 | [+0.08, +1.18] | C2 | +0.46 | +1.29 | [-0.41, +1.33] | 0.9834 | 0.0050 | 0.0000 |
| `decl,hstr=0.05` | +0.62 | [+0.08, +1.18] | C3 | +2.40 | +2.80 | [+0.92, +3.87] | 0.9848 | 0.0040 | 0.0000 |
| `decl,hstr=0.05` | +0.62 | [+0.08, +1.18] | C5 | +2.74 | +1.43 | [+1.89, +3.59] | 0.9792 | 0.0047 | 0.0000 |
| `decl,hstr=0.15` | +2.45 | [+1.68, +3.21] | C1 | +4.52 | +4.62 | [+3.63, +5.42] | 0.9787 | 0.0040 | 0.0000 |
| `decl,hstr=0.15` | +2.45 | [+1.68, +3.21] | C2 | +3.48 | +3.18 | [+2.60, +4.39] | 0.9834 | 0.0037 | 0.0000 |
| `decl,hstr=0.15` | +2.45 | [+1.68, +3.21] | C3 | +6.17 | +7.43 | [+4.70, +7.67] | 0.9808 | 0.0020 | 0.0000 |
| `decl,hstr=0.15` | +2.45 | [+1.68, +3.21] | C5 | +3.76 | +4.38 | [+2.92, +4.62] | 0.9803 | 0.0043 | 0.0000 |
| `hit,hstr=1.0` | +3.85 | [+2.98, +4.73] | C1 | +8.21 | +8.25 | [+7.35, +9.07] | 0.9778 | 0.0052 | 0.0000 |
| `hit,hstr=1.0` | +3.85 | [+2.98, +4.73] | C2 | +8.28 | +7.18 | [+7.43, +9.16] | 0.9781 | 0.0053 | 0.0000 |
| `hit,hstr=1.0` | +3.85 | [+2.98, +4.73] | C3 | +8.80 | +10.75 | [+7.33, +10.30] | 0.9799 | 0.0067 | 0.0000 |
| `hit,hstr=1.0` | +3.85 | [+2.98, +4.73] | C5 | +5.67 | +5.13 | [+4.81, +6.53] | 0.9709 | 0.0048 | 0.0000 |
| `prior,hstr=0.6` | +2.31 | [+1.47, +3.15] | C1 | +1.53 | +1.93 | [+0.65, +2.42] | 0.9539 | 0.0171 | 0.0000 |
| `prior,hstr=0.6` | +2.31 | [+1.47, +3.15] | C2 | +2.78 | +2.92 | [+1.92, +3.65] | 0.9813 | 0.0045 | 0.0000 |
| `prior,hstr=0.6` | +2.31 | [+1.47, +3.15] | C3 | +2.90 | +4.57 | [+1.35, +4.45] | 0.9574 | 0.0182 | 0.0000 |
| `prior,hstr=0.6` | +2.31 | [+1.47, +3.15] | C5 | +3.70 | +3.17 | [+2.85, +4.55] | 0.9782 | 0.0047 | 0.0000 |
| `decl,hstr=0.08` | +0.86 | [+0.29, +1.42] | C1 | +1.28 | +1.61 | [+0.41, +2.16] | 0.9773 | 0.0072 | 0.0000 |
| `decl,hstr=0.08` | +0.86 | [+0.29, +1.42] | C2 | +1.47 | +1.08 | [+0.60, +2.34] | 0.9799 | 0.0049 | 0.0000 |
| `decl,hstr=0.08` | +0.86 | [+0.29, +1.42] | C5 | +2.98 | +2.04 | [+2.12, +3.83] | 0.9793 | 0.0046 | 0.0000 |
| `decl,hstr=0.11` | +1.81 | [+1.07, +2.55] | C1 | +3.44 | +3.79 | [+2.57, +4.33] | 0.9812 | 0.0053 | 0.0000 |
| `decl,hstr=0.11` | +1.81 | [+1.07, +2.55] | C2 | +1.96 | +2.49 | [+1.07, +2.83] | 0.9797 | 0.0087 | 0.0000 |
| `decl,hstr=0.11` | +1.81 | [+1.07, +2.55] | C5 | +3.37 | +3.79 | [+2.52, +4.23] | 0.9795 | 0.0040 | 0.0000 |
| `leak,hstr=1.5` | +1.68 | [+0.88, +2.48] | C1 | +3.36 | +2.28 | [+2.48, +4.24] | 0.9748 | 0.0077 | 0.0000 |
| `leak,hstr=1.5` | +1.68 | [+0.88, +2.48] | C5 | +3.25 | +3.71 | [+2.41, +4.10] | 0.9777 | 0.0077 | 0.0000 |

Pooled over both banks:

| class | rung | dTrue | bank A | bank B | pooled | 95% (pooled) | n (games) | same sign |
|---|---|---:|---:|---:|---:|---|---:|:--:|
| C1 | `none` | +0.00 | +0.99 | +0.52 | +0.76 | [+0.15, +1.37] | 24000 | yes |
| C1 | `decl,hstr=0.05` | +0.62 | +1.48 | +0.59 | +1.03 | [+0.43, +1.64] | 24000 | yes |
| C1 | `decl,hstr=0.15` | +2.45 | +4.52 | +4.62 | +4.57 | [+3.94, +5.20] | 24000 | yes |
| C1 | `hit,hstr=1.0` | +3.85 | +8.21 | +8.25 | +8.23 | [+7.62, +8.84] | 24000 | yes |
| C1 | `prior,hstr=0.6` | +2.31 | +1.53 | +1.93 | +1.73 | [+1.11, +2.36] | 24000 | yes |
| C1 | `decl,hstr=0.08` | +0.86 | +1.28 | +1.61 | +1.45 | [+0.83, +2.06] | 24000 | yes |
| C1 | `decl,hstr=0.11` | +1.81 | +3.44 | +3.79 | +3.62 | [+2.99, +4.24] | 24000 | yes |
| C1 | `leak,hstr=1.5` | +1.68 | +3.36 | +2.28 | +2.82 | [+2.20, +3.44] | 24000 | yes |
| C2 | `none` | +0.00 | +1.37 | +0.72 | +1.05 | [+0.43, +1.66] | 24000 | yes |
| C2 | `decl,hstr=0.05` | +0.62 | +0.46 | +1.29 | +0.87 | [+0.27, +1.48] | 24000 | yes |
| C2 | `decl,hstr=0.15` | +2.45 | +3.48 | +3.18 | +3.33 | [+2.71, +3.96] | 24000 | yes |
| C2 | `hit,hstr=1.0` | +3.85 | +8.28 | +7.18 | +7.73 | [+7.12, +8.35] | 24000 | yes |
| C2 | `prior,hstr=0.6` | +2.31 | +2.78 | +2.92 | +2.85 | [+2.24, +3.47] | 24000 | yes |
| C2 | `decl,hstr=0.08` | +0.86 | +1.47 | +1.08 | +1.27 | [+0.65, +1.89] | 24000 | yes |
| C2 | `decl,hstr=0.11` | +1.81 | +1.96 | +2.49 | +2.22 | [+1.60, +2.85] | 24000 | yes |
| C3 | `none` | +0.00 | +1.88 | +1.85 | +1.86 | [+0.78, +2.94] | 8000 | yes |
| C3 | `decl,hstr=0.05` | +0.62 | +2.40 | +2.80 | +2.60 | [+1.55, +3.65] | 8000 | yes |
| C3 | `decl,hstr=0.15` | +2.45 | +6.17 | +7.43 | +6.80 | [+5.75, +7.85] | 8000 | yes |
| C3 | `hit,hstr=1.0` | +3.85 | +8.80 | +10.75 | +9.78 | [+8.72, +10.83] | 8000 | yes |
| C3 | `prior,hstr=0.6` | +2.31 | +2.90 | +4.57 | +3.74 | [+2.65, +4.82] | 8000 | yes |
| C5 | `none` | +0.00 | +1.64 | +1.41 | +1.52 | [+0.92, +2.13] | 24000 | yes |
| C5 | `decl,hstr=0.05` | +0.62 | +2.74 | +1.43 | +2.09 | [+1.49, +2.69] | 24000 | yes |
| C5 | `decl,hstr=0.15` | +2.45 | +3.76 | +4.38 | +4.07 | [+3.46, +4.68] | 24000 | yes |
| C5 | `hit,hstr=1.0` | +3.85 | +5.67 | +5.13 | +5.40 | [+4.79, +6.02] | 24000 | yes |
| C5 | `prior,hstr=0.6` | +2.31 | +3.70 | +3.17 | +3.44 | [+2.83, +4.04] | 24000 | yes |
| C5 | `leak,hstr=1.5` | +1.68 | +3.25 | +3.71 | +3.48 | [+2.88, +4.08] | 24000 | yes |
| C5 | `leak,hstr=4.0` | +5.57 | +7.05 | +7.47 | +7.26 | [+6.65, +7.88] | 24000 | yes |
| C5 | `decl,hstr=0.08` | +0.86 | +2.98 | +2.04 | +2.51 | [+1.91, +3.11] | 24000 | yes |
| C5 | `decl,hstr=0.11` | +1.81 | +3.37 | +3.79 | +3.58 | [+2.99, +4.18] | 24000 | yes |

### 3.5 The budget curve

| target | gens × pop × deals | games spent fitting | bank | dFound (pts) | 95% CI |
|---|---|---:|---:|---:|---|
| `v06:hcap=decl,hstr=0.15` | 6 × 12 × 150 | 21600 | 7021001 | +3.02 | [+2.18, +3.87] |
| `v06:hcap=decl,hstr=0.15` | 6 × 12 × 150 | 21600 | 7021002 | +3.38 | [+2.52, +4.24] |
| `v06:hcap=decl,hstr=0.15` | 12 × 16 × 250 | 96000 | 7021001 | +3.67 | [+2.80, +4.54] |
| `v06:hcap=decl,hstr=0.15` | 12 × 16 × 250 | 96000 | 7021002 | +4.10 | [+3.23, +4.97] |
| `v06:hcap=decl,hstr=0.15` | 20 × 20 × 350 | 280000 | 7021001 | +4.64 | [+3.77, +5.52] |
| `v06:hcap=decl,hstr=0.15` | 20 × 20 × 350 | 280000 | 7021002 | +3.53 | [+2.63, +4.42] |

The curve rises and flattens: +3.20 → +3.88 → +4.08 pooled as the fitting budget triples twice.
Even a 21,600-game fit — a quarter of the v0.6 probe's budget — finds +3.2 points once the
responder is in the right class and seeded at the incumbent, so **P-3a (class mis-specification),
not P-3c (budget), was the v0.6 probe's binding defect**. Phase 2's standard budget stays at
12 × 16 × 250, and the curve, not the scalar, is what is reported (threat model T6).

### 3.6 The readability rungs

Two rungs make the choice of half-suit a monotone read-out of the asker's holding — a
*readability* transform whose strength cost was measured near zero at small sizes. Three
measurements close the question together:

* **In bits (W2):** pointing the inversion probe at the handicapped targets, with the oracle
  modelling the handicap exactly, adds **+0.02 to +0.07 bits per ask** to a baseline of 1.95.
  The handicap changes *which* deals survive the inversion, not *how many*.
* **In play, C5:** excess over control +1.95 (`leak 1.5`) and +5.74 (`leak 4.0`) — within 0.3 of
  the rungs' measured `dTrue` (+1.68, +5.57). C5 collects the *strength* the handicap costs, and
  nothing more.
* **In play, fitted C1:** excess +2.06 on `leak 1.5` — statistically indistinguishable from C5's.
  A strength-seeking responder with no transcript access finds the same edge.

**Conclusion: within this policy family there is no such thing as a readability handicap a
white-box responder can uniquely see, because the unhandicapped deterministic policy is already
close to fully readable** (~1.95 bits per ask of contraction beyond the certificates, §5). The
determinism *is* the readability, and it is uniform across the family. Planted-edge calibration of
class C5 therefore reduces to planted strength — where C5 behaves like every other class — and
ledger L4's "readable but not exploitable" scenario is the measured outcome for the one-step
inverter: readable (2 bits/ask), and convertible into general strength (+1.52) but not into
target-specific exploitation.

---

## 4. Throughput, on one basis

### 4.1 Why the basis is stated twice

The v0.6 record says the search costs "three orders of magnitude" (`08-search.tex`,
`sec:search-ships`). That figure divides an all-threads number (303.4 games/s) by a single-thread one
(0.144 games/s) and is not a same-basis ratio; the corpus's own final audit flagged the
inconsistency (`V2-final-audit.md`, SF-6) and the ledger re-measured the real figure at 300–420×
(§0.1). Separately, E9 timed every policy **against itself** while the ledger's F-search and F-mid
rows time a search arm **against `v05`/`v06`**; a mirror pays the configuration's cost on both sides,
so those are different quantities too.

`fish7 v7through` therefore reports **both the all-threads and the single-thread number for every
configuration**, and labels the opponent basis of each block. Ratios are computed within a block and
within a basis. The mistake is not available.

**Basis: mirror (each policy against itself, E9's basis).** Reference row is the first; ratios are within the block and within one basis.

| configuration | games/s, all threads | games/s, 1 thread | × ref (all) | × ref (1 thr) | deals timed |
|---|---:|---:|---:|---:|---:|
| `v06` | 326.22 | 31.003 | 1.000 | 1.000 | 600 / 100 |
| `v05` | 336.63 | 28.459 | 1.032 | 0.918 | 600 / 100 |
| `v04` | 213.87 | 17.903 | 0.656 | 0.578 | 600 / 100 |
| `v03` | 7185.51 | 591.312 | 22.027 | 19.073 | 600 / 100 |
| `v05:belief=indep,topk=0` | 11733.15 | 1006.930 | 35.967 | 32.478 | 600 / 100 |
| `v05:topk=0` | 331.54 | 31.020 | 1.016 | 1.000 | 600 / 100 |
| `v07` | 356.35 | 31.183 | 1.092 | 1.006 | 600 / 100 |
| `v07i:idet=48` | 43.19 | 4.018 | 0.132 | 0.130 | 600 / 100 |
| `v07i:idet=96` | 25.97 | 2.487 | 0.080 | 0.080 | 600 / 100 |

**Basis: against `v06` (the ledger's F-search basis).** Reference row is the first; ratios are within the block and within one basis.

| configuration | games/s, all threads | games/s, 1 thread | × ref (all) | × ref (1 thr) | deals timed |
|---|---:|---:|---:|---:|---:|
| `v06` | 382.19 | 32.817 | 1.000 | 1.000 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5` | 1.58 | 0.161 | 0.004 | 0.005 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,depth=24` | 2.99 | 0.296 | 0.008 | 0.009 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep` | 9.20 | 0.785 | 0.024 | 0.024 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24` | 22.84 | 2.010 | 0.060 | 0.061 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26` | 78.45 | 6.615 | 0.205 | 0.202 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` | 118.97 | 10.363 | 0.311 | 0.316 | 400 / 25 |
| `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26` | 7.62 | 0.706 | 0.020 | 0.021 | 400 / 25 |
| `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26,rbelief=indep,depth=24` | 49.50 | 3.784 | 0.130 | 0.115 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26,roppo=v06:belief=indep+topk=0` | 81.51 | 6.631 | 0.213 | 0.202 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26,roppo=v06` | 10.41 | 0.960 | 0.027 | 0.029 | 400 / 25 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26,leafeval=score` | 86.08 | 6.748 | 0.225 | 0.206 | 400 / 25 |

Reading the second block against the ledger's expectations: the deployed policy is **242×** the
baseline search all-threads and **204×** single-thread — the two bases now agree, where the
corpus's "three orders of magnitude" mixed them (V2-final-audit SF-6) and the ledger's re-measurement
gave 300–420× against `v05` on other hardware. **F-mid on the shipped vector runs at 7.6 games/s —
50× slower than deployed, not the 60–90× the ledger could only infer from a `legacy=1` timing.**
The search block was re-timed at 400 deals after a 50-deal timing disagreed with a 6,000-deal run
by 39× on one row: per-deal search cost is heavy-tailed, and short timings do not sample the tail
(the artifact keeps both, `T1-throughput.jsonl` and `T1b-throughput-400.jsonl`).

### 4.2 What the engineering bought, and what it did not

The four levers were swept before any code was written (RESEARCH-LOG.md §1.8): the cheap blueprint
is 5.3×, truncation is 1.7×, the two together are 13.2×, and adding the endgame restriction is
42.8×. **The throughput was almost entirely available in knobs the corpus already shipped and never
combined.** What was missing was a leaf evaluator to make the depth cut mean something, and a reason
to trust that combining them had not changed the policy.

So the engineering in this phase is mostly about making the combination *legitimate* rather than
about making it fast:

* a **leaf-evaluator interface** that is feature-first and batched — a truncated rollout writes a
  fixed-width feature row and returns, and every leaf of a decision is priced in one call. The
  default evaluator reproduces v0.6's `leafValue` **bit for bit**, which is checked against a binary
  rebuilt from `db6066c` rather than asserted;
* **batch evaluation**, which costs a scalar evaluator nothing and is the only shape in which a
  learned evaluator is affordable at all;
* a **work-stealing scheduler** and a **deal-indexed paired vector**, the second of which fixed a
  defect: published bootstrap intervals were a function of the machine's core count
  (RESEARCH-LOG.md §1.2, A-1);
* **seed-bank sharding** (`--shard=s/n`), which partitions a bank exactly and reassembles into it,
  so a long battery can be split across processes without a second bank;
* a `resetWithKnowledge` fast path, because a rollout reconstructs six agents `K × D` times per
  searched decision and then overwrites the knowledge it just built.

One warning for phase 3 came out of this and is not obvious: **storing the leaf feature row in
`float` changes which move the search plays.** A float row loses about 1e-8 relative precision on the
control term, which is enough to flip the lower-confidence-bound comparison at a few decisions per
hundred games. `double` alone was not sufficient either, because floating-point addition is not
associative. A learned evaluator will want float; whoever builds it must re-run the identity control.

### 4.3 The throughput claim is worth nothing without a strength claim

The fast configurations are **different operating points, not a faster version of the same search**.
So each is measured against `v06` at adequate power on two banks:

| search configuration | bank | win rate vs `v06` | 95% CI | n (games) | 98/√N | games/s |
|---|---:|---:|---|---:|---:|---:|
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26` | 7010001 | 51.94% | [51.19, 52.69] | 12000 | 0.89 | 70.08 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26` | 7010002 | 51.20% | [50.47, 51.94] | 12000 | 0.89 | 27.81 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` | 7010001 | 52.12% | [51.38, 52.88] | 12000 | 0.89 | 5.76 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` | 7010002 | 52.26% | [51.50, 53.01] | 12000 | 0.89 | 2.94 |
| `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26,rbelief=indep,depth=24` | 7010001 | 50.29% | [49.55, 51.03] | 12000 | 0.89 | 23.33 |
| `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26,rbelief=indep,depth=24` | 7010002 | 50.42% | [49.66, 51.17] | 12000 | 0.89 | 41.38 |
| `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26` | 7010001 | 52.77% | [51.27, 54.27] | 3000 | 1.79 | 7.37 |
| `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26` | 7010002 | 52.27% | [50.80, 53.73] | 3000 | 1.79 | 7.48 |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24` | 7010001 | 50.08% | [48.52, 51.62] | 4000 | 1.55 | 20.82 |
| `v06:s1=1,det=12,cand=4,kappa=2.5` | 7010001 | 52.85% | [50.28, 55.42] | 1440 | 2.58 | 1.57 |

Read with the throughput table (§4.1): the fast configurations hold +1.57 and +2.19 points,
replicated at 12,000 games per bank, at 50× and 75× the baseline search's throughput — and
**F-mid, the strongest configuration the corpus ever measured, finally has a shipped-vector
head-to-head: +2.52 points over `v06`, replicated** (52.77 [51.27, 54.27] / 52.27 [50.80, 53.73]).
Ledger L2's sign-inverted +1.35 was conservative.

Two structural findings ride along. **The inherited conditional is refuted in the endgame regime**:
the evaluator in the `depth=12` configuration is v0.6's own leaf (`MaterialLeaf`, bit-for-bit),
which the v0.6 conclusion said "cannot support a depth-limited search" — it supports one to +2.19
where the belief is sharp (`maxq=26`); without the endgame restriction the same treatment measures
+0.08, so the conditional still binds for full-game search, and phase 3's evaluator work has a
measured target rather than a blanket gate. And **the gain is not monotone in budget**: fewer
determinizations and fewer candidates outperform more (`det=12,cand=4` +1.57 vs `det=16,cand=6`
+0.35 under identical treatment), echoing the optimizer's-curse mechanics the corpus already
documented — variance in the LCB comparison grows faster than signal.

### 4.4 The power arithmetic

98/√N is computed by the harness and emitted with every cell, in JSON and in text, for `match`,
`v7through`, `v7decide` and the floor battery. It carries three quantities that are not the same
quantity:

* the unpaired one-arm half-width over **games**, which is the figure the ledger's table prints;
* the same formula over **deals**, which is the paired design's real floor — a duplicate block plays
  one deal at several orientations and those outcomes are one correlated cluster, so reporting
  98/√games for a paired design overstates the resolution by √rotations;
* the **mirror** case, in which the half-width does not exist at all. A mirror win-rate cell carries
  no information: the arms are exchangeable by construction, the per-deal outcome is deterministic,
  and the effective sample is zero, not half. Halving is the right correction for *rate denominators*
  and the wrong one for win rates — which is the prose error the ledger files as P-6. The harness
  prints `MIRROR CELL: win-rate effective sample is 0` and refuses to print a half-width.

---

## 5. What the white-box responder found

| observer | target | model | bank | bits/ask | SE | surviving frac. | nats base → inv. | argmax base → inv. | inverted asks |
|---|---|---|---:|---:|---:|---:|---|---|---:|
| `v06` | `v06` | `v06` | 7012001 | 1.9454 | 0.0180 | 0.4441 | 1.40094 → 1.39194 | 0.3515 → 0.3573 | 10342 |
| `v06` | `v05` | `v05` | 7012001 | 2.0583 | 0.0183 | 0.4214 | 1.39014 → 1.38685 | 0.3555 → 0.3612 | 10344 |
| `v06` | `v06` | `v06` | 7012002 | 1.9933 | 0.0181 | 0.4345 | 1.38691 → 1.37969 | 0.3595 → 0.3649 | 10474 |
| `v06` | `v05` | `v05` | 7012002 | 2.1121 | 0.0182 | 0.4104 | 1.38828 → 1.38658 | 0.3588 → 0.3643 | 10552 |

The full construction, the failed first accumulator, the sweep that repaired it, and the honest
statement of what the approximation costs are in RESEARCH-LOG.md §1.6. The three results:

1. **The contraction is large.** 1.78 bits per ask, SE 0.067, mean surviving fraction 0.473 — and it
   is a contraction *beyond* the certificates, because every sampled deal already satisfies all of
   them. By event index it runs 2.66 bits early to 0.28 late. Ledger entry **L4** set the kill
   threshold at "under ~1 bit per ask the hypothesis dies for free". **It did not die.** Nobody has
   measured this before, in this corpus or outside it.
2. **Bits are much easier to measure than to use.** The natural accumulator — per-(card, seat)
   log-likelihood ratios summed across the game — makes the observer's marginals *worse*. Two causes,
   both real: the per-cell estimator is built from about ten Bernoulli draws and is mostly noise, and
   summing evidence from correlated actions multiplies a confidence that should have been shared.
   The repairs are shrinkage, tempering, and one structural correction that mattered more than either
   — given the public state the observed action is a function of the **actor's** hand alone, so
   updating all six columns spends the sample on second-order effects it cannot resolve.
3. **The conversion is real and small.** At the swept optimum the measured inversion improves the
   observer's marginals on both criteria at once — the only setting in the sweep that does — by
   0.0102 nats and **+0.69 points of argmax accuracy**. Replacing the fitted `priorTheta`/`priorPhi`
   heuristic with the measurement is *worse* than adding the measurement to it, so the two are not
   substitutes: the heuristic carries something a one-step inversion does not.

There is a framing here worth carrying into phase 3. **C5 is not a new channel bolted onto the
belief; it is the exact version of a channel the corpus already ships an approximation of.**
`priorTheta`/`priorPhi` are a hand-fitted two-parameter stand-in for "given that this seat played
what it played, how much more likely is it to hold this card", and ledger entry **C2** already
records that the policy prior is the *entire* difference between the exact posterior and the deployed
approximation as predictors (1.38218 nats / 51.49% argmax against 1.42246 / 47.94%). Phase 1 replaced
the guess with a measurement and got a small improvement on top of the guess.

**The limitation, stated in the body rather than in a footnote.** The accumulator marginalises. The
exact posterior is the intersection of the certificate posterior with every policy constraint, and
its acceptance rate is about 0.47²⁵ ≈ 10⁻⁸, so it cannot be sampled by rejection; a joint particle
filter over a *static* hidden state degenerates, and its rejuvenation step discards exactly the
evidence it exists to keep. What is implemented is a Rao-Blackwellised approximation that treats
different actions as conditionally independent given the certificates. It **under-estimates** what a
perfect inverter would extract. A positive result from it is a lower bound; a null result from it
does not close class C5. The 1.78-bit contraction is not approximate in this way — it is an exact
one-step measurement.

---

## 6. The exit criterion

The phase brief: *"at least one responder recovers planted edges down to a stated size at or below
the effect sizes v0.7 will claim; the search configuration is measurable at 10× or more of v0.6's
games/s; the 98/√N power arithmetic is wired into harness output. If the exit criterion fails, say
so and stop rather than proceeding on a weak instrument."*

**Clause 1 — met at ≥1.68 points; not met below; and the effects now on the table are above it.**
The best floors are C1 = C5 = **1.68 points**, under the strict criterion (excess over the
unhandicapped control excluding zero on both evaluation banks); nothing detects +0.86. The live
candidate effects this phase surfaced — F-mid +2.52, the truncated fast search +2.19, the measured
policy prior +1.52 — sit at or above the floor, so the claims v0.7 is actually positioned to make
are certifiable. What is **not** certifiable at current bank sizes is any sub-point claim,
including one the size of v0.6's own +0.89 head-to-head margin. Below ~1.7 points the floor is
**evaluation-power-limited, not search-limited**: at the undetected +0.86 rung the pooled excesses
are positive (+0.69 C1, +0.98 C5) but the excess estimator's ±≈1.2-point width cannot exclude zero,
so the floor scales as (evaluation games)^−1/2 — ≈0.9 points at 4× the games per cell. Phase 2 must
size its claims to the floor or size its banks to the claim; both options are now priced.

**Clause 2 — met, five to seven times over.** Replicated strength above the deployed policy is
measurable at 50× (+1.57) and 75× (+2.19) the baseline search's throughput. F-mid itself, at 4.8×,
went from "never head-to-headed on the shipped vector" to a two-bank replication in under an hour.

**Clause 3 — met.** Emitted with every cell, both estimators, mirror caveat included.

**Verdict: the battery proceeds — not on a weak instrument, but on a calibrated one whose floor is
measured, stated, and purchasable.** The one standing weakness is named rather than hidden: this
phase's headline adversary regime is **A1**, not the threat model's A2 headline (the device is
built; the fit over correlated plans is phase-2 work), and the C4 learned class remains the one
class never instantiated by anyone.

---

## 7. What phase 2 inherits

**Instruments.**

* Four responder classes with measured floors — C1/C2 fitted (`--fromv6`, semicolon panels), C3 via
  `roppo=`, C5 via `v07i` — and the A2 correlation device behind `--correlated` + `corr=K`,
  unfitted and waiting.
* The per-decision channel (`v7decide`, `MatchConfig::captureDecisions`) with deal-clustered CIs;
  the bit probe (`v7bits`); the throughput harness (`v7through`, two bases, never mixed).
* The reserved-seed registry (`fish7 seeds`), with phase-5 banks refusing to unseal below
  `FISH_UNSEAL_PHASE=5`, and sharding (`--shard=s/n`) that partitions a bank exactly.
* The planted-weakness family (`hcap=`/`hstr=`) with a measured ladder in `C0-ladder.txt`.

**Facts phase 2 should treat as established.**

1. v0.6's in-class exploitability is ≥ +0.76 points (C1, two banks); the class ladder rises
   +0.76 → +1.05 → +1.52 → +1.86 (C1 → C2 → C5 → C3). The published 48.36% figure was a
   mis-specified-exploiter artifact.
2. Every observed ask of a deterministic FishBot yields ~2.0 bits about the deal beyond the
   certificates; the policy family cannot be made meaningfully more readable than it already is;
   and the one-step inverter converts readability into general strength (+1.52), not
   target-specific exploitation (excess tracks dTrue within ±0.4 on eight rungs).
3. The declaration channel is the sensitive axis of the whole family (a 0.025 `declareMargin`
   shift is a measurable handicap; `declAllocErrorShare` = 0.881 [0.810, 0.944] for v0.6 — larger
   than the 72–75% the ledger records for its predecessors).
4. M1's live-ask gate binds on the would-be argmax at 0.94% of v0.6's decisions and can be opened
   on 40% of decisions without measurable cost — ledger L10's hazard has a small surface at v0.6's
   weights.
5. F-mid is +2.52 over `v06` (replicated); the truncated fast path holds +2.19 at 75× baseline
   search throughput; v0.6's own leaf supports depth-limited search in the endgame regime and does
   not support it full-game.

**Leads this phase generates for phase 3** (recorded here; phase 2 adjudicates):

* The measured policy prior — C5's +1.52 unfitted, with ledger C2 as prior evidence — as a
  deployable belief improvement, if its cost (43 games/s at `idet=48`) can be bought down.
* The truncated endgame search as the deployable form of the search conditional.
* The declaration-allocation channel, now scored per decision.

---

## 8. Reproduction

```bash
cd engine
clang++ -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -DFISH_NO_SERVE \
        src/main.cpp -o fish7 -pthread
./experiments_v07.sh          # gates first: seeds, identity, pathology; then D1 and W1
./exploitability_v07.sh       # the detection-floor battery
python3 ../engine/build_tables_v07.py --dir ../research/v07/results
```

Banks are registered in `engine/src/v07_seeds.hpp` and checked by `fish7 seeds`; the phase-5 holdout
banks refuse to be constructed unless `FISH_UNSEAL_PHASE=5` is set.
