# R0 — FishBot v0.6 Opportunity Register

Synthesis of R1–R10 (`research/v06/notes/R{1..10}*.md`). Baseline is commit `bd812fe` (v0.5).
Every number below is traceable to a named report, a `file:line`, or a command re-run in this
session. Where two reports measured the same quantity with independent probes, both are quoted.

Standing owner preferences applied throughout: **robustness across opponent playstyles outranks
aggregate win rate; no aggregate win rate is ever a headline; produce evidence, not hedging.**

---

## 1. Where v0.5 actually loses value

### 1.0 The frame

R8's score attribution over 400 deals × 6 rotations, seed 606060, identical deals
(`R8-game-structure.md` §3.2):

| ablation | win rate | mean sets |
|---|---:|---:|
| `v05` (reference) | — | 4.500–4.500 |
| `v05:value=0,lweight=0,topk=0` (no ask policy) | 5.83% | 2.447–6.553 |
| `v05:belief=indep` (no inference) | 5.38% | 2.314–6.686 |
| `v05:declare=0` (no voluntary declaration) | 43.25% | 4.268–4.732 |
| `v05:topk=1` (no two-ply refinement) | 49.96% | — |
| `v05:m1=0` (no live-ask gate) | 48.96% | — |
| `random` | 0% | 0.426–8.574 |

**Asking is worth ~4.11 half-suits, inference ~4.37, the declaration decision ~0.46.** The game is
decided by ask selection over declaration roughly 9:1. Two of v0.5's three headline v0.5 mechanisms
(two-ply search, M1 gate) are worth **nothing** head-to-head.

Meanwhile the perfect-information ceiling is 8.867–0.133 (R8, verified 300/300 games) and an
omniscient team beats v0.5 8.932–0.068 at a 100% win rate over 600 games. **The hidden state is
worth ~4.4 half-suits and v0.5 recovers essentially none of the margin above a coin flip.**

### 1.1 Rank-ordered loss channels

---

**#1 — The ask argmax is blind at ~55% of decisions. Value at stake: up to +3.16 sets/game.**

Two independent probes, different authors, different code:

- R6 (`v06probe margin`, 60 games × 2 rot, seed 31, 5,291 decisions): **exact tie `u1==u2`
  bit-for-bit at 54.73%**, identical posterior `p1==p2` at 100.00% of those, same half-suit *and*
  same target at **94.79%**, and the top-2 differ in **ground-truth outcome at 33.22% of ties**.
- R10 (`probe/margin.cpp`, 150 games, seed 31, 13,879 decisions): **exact tie at the top 55.16%**,
  median top1–top2 gap **0.000000** against a median score spread of **5.91** across candidates.

This is not a flat objective — it is a **blind dimension**. The winner of a tie is whichever
candidate `enumerateAsks` emitted first (lowest card index, then lowest seat).

The mechanism is structural and is *three* independent causes stacked:
1. `features()` computes 18 of its 20 features per (half-suit, target); the 3 per-card features are
   all functions of `bel.marg[card][target]` (`engine/src/v05.hpp:285-344`).
2. `sinkhornDisj` (`engine/src/belief.hpp:478`) makes exchangeable cards **identical by
   construction** — the Fast belief cannot represent the distinction at all.
3. `askExpectedValue` opens `(void)target;` (`engine/src/v05.hpp:437-438`, inherited from
   `v04.hpp:435`), so the expectimax half of the score is constant across targets.

Price of resolving it: R6's `askoracle` (same belief, same weights, same features, same top-6
candidate set, same declaration logic — *only* the tie resolved correctly) scores **7.6611–1.3389
over 900 games at a 99.89% win rate**. That is **+3.16 sets/game of headroom sitting inside a
subroutine v0.5 already runs.** It is a hindsight bound, not an achievable policy (R6 risk 2), but
it prices the channel.

Corroborating capacity measurements: 88.02% of decisions already contain a hitting ask inside
v0.5's own top-6 against a realised 56.6% hit rate; 98.6% of misses were recoverable; only 1.39% of
misses had no hitting ask anywhere on the board (R6).

The free bits are measured three ways and agree: 0.639 bits/ask on the target dimension over
154,318 v0.4 decisions (P5, via R5); 0.959 bits target / **1.301 bits card-within-half-suit** /
4.979 bits joint over v0.5 mirror (R9). **The card dimension is larger than the target dimension
that P5 headlined.**

---

**#2 — 9.74% of all asks are guaranteed misses into half-suits the asker's own team already owns
outright. That is 21.97% of every miss v0.5 makes.**

Re-measured this session across six table compositions (`./fish pathology --a=v05 --b=<opp>
--games=100 --seed=31`; counters are pooled over all six seats — see `engine/src/diag.hpp:108-119`
— so only the mirror row is a clean v0.5-only number):

| table | asks in own-locked | hit rate | events/game | repeat (a,suit,t) |
|---|---:|---:|---:|---:|
| v05 vs **v05** (clean) | **9.74%** | 56.03% | 96.58 | 50.63% |
| v05 vs v04 | 10.06% | 54.55% | 99.74 | 51.96% |
| v05 vs v03 | 11.35% | 55.38% | 97.75 | 48.39% |
| v05 vs lockout | 12.11% | 54.28% | 103.10 | 49.87% |
| v05 vs detective | 12.23% | 54.58% | 100.33 | 48.59% |
| v05 vs hunter | 7.50% | 53.20% | 90.21 | 45.65% |
| v05 vs diversifier | 7.73% | 61.60% | 108.21 | 50.50% |

**The channel is present in every regime measured (7.5%–12.2%), so it is not a mirror artifact.**
R1 independently measures 9.77%, R9 measures 10.16%.

M1 **structurally cannot** see these: `enumerateAsks` (`engine/src/fish.hpp:179-197`) only permits
asking opponents, and no single seat can prove a *teammate* holds a card. This is 20× the rate at
which M1's gate binds on the argmax (5.78%, R1). Each one hands the turn to the other team, and the
turn is worth ~0.41 half-suits mid-game rising to 0.826 sets after event 80 (R8 §4.2).

Same root cause produces the 5.55-event mean gap between the team physically owning a half-suit and
being able to *prove* the allocation (R6). Locks are never broken (0 of 5,370), so this is pure
delay, not risk.

---

**#3 — The whole of v0.5's fit was sampling noise. All of v0.5's gain was mechanical.**

- The CEM incumbent objective went 0.4998 → 0.5096 over 40 generations against a per-generation
  sd of 0.0227; OLS slope +0.00049/gen, se 0.00031, **t = 1.60**. Mean best-minus-incumbent gap
  0.0404 vs 0.0442 predicted by E[max of 24 iid] = 1.948 sd (R4, `research/v05/runs/fit-round1.jsonl`).
- Mechanism: `sigma0 = 0.6` (`engine/src/tuner.hpp:24`) applied to all 34 coordinates whose ranges
  span 0.1 to 40. At generation 0, **93.4% of `declareMargin` candidates land on a clamp bound**,
  70.9% of `declThreshold`, 63.9% of `priorPhi` — while `valueWeight` gets a step of 1.5% of its
  range. The search is bang-bang on bounded knobs and frozen on unbounded ones.
- `--sigmaparams` is parsed at `engine/src/main.cpp:224` and **never referenced again**.
- Confirmation from the other end: running the v0.5 *mechanisms* on v0.4's frozen 34-coordinate
  vector reproduces essentially the whole result — 50.22% [48.40, 52.03] over 6,000 games, mirror
  dead-ask rate 0.02%, longest dead run 1 (R5, `research/v05/runs/v04vector-in-v05.txt`).
- And v0.5 is not distinguishable from v0.4 at either bank: 51.61% [49.30, 53.91] and 50.50%
  [48.19, 52.81], both n=1,800 (R6).

**A v0.6 that reruns the fit gets nothing. The harness must be repaired before any mechanism can be
measured, let alone fitted.**

---

**#4 — 87.2% of runtime is spent on the decision worth 0.46 sets.**

`proposeDeclaration` is 114,486 calls per 200 games at 61 µs = 7.030 s of an 8.058 s total;
`chooseAsk` is 1.010 s / 12.5% (R1 `r1_split`). Five of the six polls per event are discarded by
lowest-seat arbitration (`engine/src/game.hpp:202-231`) and nothing is cached across events or
seats. Inside `chooseAsk`, the top-K chain/threat search is 90% of the time (12 Sinkhorn refits per
decision, 59 µs/ask vs 6 µs at `topk=1`) and is worth **+0.8 ± 0.5 points** pooled over 16,000
games; `chainWeight` alone is worth **exactly 0.0** (R1).

R10 prices the same misallocation from the other side: `v05` = 560.2 µs/event;
`v05:topk=0` = 348.3; `v05:belief=indep,topk=0` = **9.07 µs/event, 62× cheaper**, retaining exact
hard deduction, M1 gating, the full 20-feature score, one-ply expectimax and M2 declarations.

---

**#5 — The value function is a 1.79% rescaling of the hit probability, dressed as a 16-feature
evaluator.**

Over 26,417 shipped ask decisions (R1 `r1_anatomy`, 300 games seed 4242):
**7 of 16 features are EXACTLY constant across the candidate set at 100.00% of decisions**
(bias, scoreDiff, activeSets, myHandSize, smallestFriendly, ourNearComplete, theirNearComplete),
carrying **41.5% of the |vw| mass including the single largest coefficient** (scoreDiff, 0.888965 =
38.5%). Of the 9 that vary, 6 vary only through `p`, because `E[e_S after ask] = e_S + p/6` exactly.
Mean R² of the value term on `p` = **0.84034**; spread across candidates 0.1479 against the linear
score's 8.2699; it flips the pre-search argmax at **0.94%** of decisions.

`declareByValue` is algebraically a threshold on `pAlloc`, not an optimal-stopping rule:
`declare ⟺ pAlloc > 0.5 + (declareMargin − κ)·9/(2·vw[1])`, verified at **100.0000% verdict
agreement over 4,226 evaluations**, implied threshold mean 0.8090 against the fitted `declThreshold`
knob of 0.81991 it replaces (R1 `r1_decl`).

Per R7, this is exactly the configuration Brown, Sandholm & Amos (arXiv:1805.08195) **prove
unsound**: a depth-limited search with a single scalar leaf value function and no opponent choice
represented at any leaf.

---

**#6 — Nobody has ever measured v0.5's exploitability, and its flagship mechanism is a published
exploitability hazard.**

`docs/FISHBOT_V05.md` §12 calls this "the single largest hole in the evaluation". M1's live-ask
gate (`engine/src/v05.hpp:107`, `:482-500`) is a **naive knowledge-limited pruning rule**, and
Zhang & Sandholm (arXiv:2106.06068, absent from all nine prior lit files) *prove* naive
knowledge-limited solving can **increase** exploitability. v0.4's baseline is a fitted response
achieving 51.19% [49.67, 52.72] (`paper/tables/lbr.tex`).

M1's true profile: it buys **zero points** (50.75% [49.20, 52.30], n=4,000) but is the entire
pathology fix — with `m1=0` and all v0.5 fitted weights intact, dead asks are **46.28%, worse than
v0.4's 39.0%**, longest dead run 374, 14.4% of games hit the action cap (R1). Its second half is
switched **off** in the shipped build (`ownershipByP=false`, `v05.hpp:112`).

---

**#7 — Everything else is small.** For completeness and to prevent misallocation:

| channel | size | source |
|---|---|---|
| misdeclarations | 1.96% of half-suits (~0.18 sets/game); 72–75% are "team held all six, named wrong teammate"; proof-backed declarations 0/1230 wrong | R6 |
| forced endgame | 6 of 5,400 half-suits = **0.11%** incidence; 0.0056/game head-to-head | R6 |
| late cashing | mean lateness **0.006 events**, median 0, p99 0 — fully solved | R6 |
| action cap / termination | 0 games hit the cap in any configuration; `forceDeclareEvents=220` fires 0 times (max observed 131) | R6, R1 |
| sniping / lost locks | **0.00%** — no sniping failure mode remains | R6 |

**97.93% of lost half-suits are lost in the collection race, not at the declaration.**

---

## 2. Candidate mechanism register, ranked by expected value

Ranking rule: (payoff × P(the mechanism works) × robustness weight) / difficulty. Mechanisms whose
absence makes *other* mechanisms unmeasurable are promoted, because R4 proved the v0.5 fit resolved
nothing and R6/R7 proved no robustness claim is currently possible.

---

### V6-M1 — Break the exact tie: give the ask score a per-card and per-target dimension
**Payoff: high · Difficulty: medium · Depends on: none (M4 improves it, M2 makes it measurable)**

Score the card-within-half-suit and the target explicitly, and delete `(void)target;` at
`v05.hpp:437-438` so the expectimax half stops being constant across targets.

*Defect:* Loss channel #1. 54.73%/55.16% exact ties broken by array order; 94.79% are two cards of
one half-suit at one target; top-2 differ in ground truth at 33.22% of ties.

*Evidence:* R6 `v06probe margin` + `askoracle` (+3.16 sets/game hindsight bound, 99.89% win rate);
R10 `probe/margin.cpp`; R9 bit-counts (card 1.301 / target 0.959 / joint 4.979 bits);
R5/P5 (46.6% of 154,318 v0.4 decisions have ≥2 hard-indistinguishable holders).

*The hard part, stated plainly:* the signal **does not exist inside the current belief**.
`sinkhornDisj` (`belief.hpp:478`) makes exchangeable cards numerically identical by construction, so
any tie-break must come from outside the Fast marginal. The three named candidate sources are
(a) the exact BlockDP count law, which R6 measured as breaking **10.9 points more ties** (43.36% vs
54.22%); (b) per-card ask history / rank-position priors; (c) a teammate-derived per-card term
from M4. Cost of (a) is 0.0111 ms mean per exact rebuild at Q=12 (R8).

*Gate:* **(i)** exact-tie-at-top rate falls below 10% on the R10 probe at the same seed;
**(ii)** paired delta (CRN, same deals) vs `v05` ≥ **+0.30 sets/game** with a paired CI excluding
zero — i.e. ≥ 10% of the askoracle bound realised; **(iii)** no style in the six-opponent panel is
worse than −0.10 sets vs `v05`; **(iv)** `asks in own-locked` and `repeat (a,suit,t)` do not rise
above the v0.5 mirror baselines of 9.74% / 50.63%.

---

### V6-M2 — Harness repair: per-coordinate sigma, paired CRN estimator, minimax-regret objective, pathology commit gate
**Payoff: high · Difficulty: medium · Depends on: none. BLOCKS the credible measurement of every other mechanism.**

Four changes: (H2) route the already-parsed `--sigmaparams` (`main.cpp:224`) into
`TuneSpec::sigma0Vec` used at `tuner.hpp:71`, defaulting to `sigma[d] = 0.15·(hi[d]−lo[d])`;
(H3) score candidates as `wr_candidate − wr_incumbent` on the same deals inside
`evaluateCandidate` (`tuner.hpp:47-65`) — the incumbent is already evaluated as `cand[0]`
(`tuner.hpp:87`) — and expose `--pair=SPEC` on the match block; (H1) replace the fixed soft-min at
`tuner.hpp:61-63` with a dispatch over `{SoftMin, Min, Regret, MinimaxRegret}`; (H4) give
`pathology` a `--json` and a `--gate` non-zero exit and make it step 1 of `experiments_v06.sh`.

*Defect:* Loss channel #3, plus loss channel #6's measurement gap. Also defect J: at beta=10 the
"soft minimum" is a weighted mean with max/min gradient weight ratio exp(β·δ) = 1.8776 on v0.4's
shipped profile, and the "minimum" sits **11.2 points below min(r)** (R4).

*Evidence:* R4 throughout — CEM trace t=1.60; 93.4% clamp-bound rate; measured paired-vs-unpaired
width 1.042 pt vs 5.66 pt on identical compute (2–5× width, up to 29× variance); every headline
number in E3/E4/E5/E7/E8 uses the unpaired one-arm cluster bootstrap while only `ablate` is paired;
`select_final.py` was **never run for v0.5** (no `selection.log` under `research/v05/runs/`);
two different betas in one pipeline (tuner compiles 10, `select_final.py` defaults 8) recorded in
no artifact.

*Gate:* **(i)** a startup self-test asserts `pairedDelta == 0.0000` with zero-width CI for a
self-variant (the measured invariant); **(ii)** per-coordinate clip fractions logged into the JSONL
and **no coordinate exceeds 20%** clip at generation 0 (from 93.4%); **(iii)** a 10-generation
sanity fit on a deliberately handicapped base (`v05:w0=0`) recovers ≥ 60% of the known gap, which
the current tuner provably cannot; **(iv)** `fish pathology --gate` returns non-zero on a seeded
`m1=0` build.

---

### V6-M3 — Ship M4: one shared common-knowledge object plus per-seat refinement
**Payoff: high · Difficulty: medium (code exists, unit-tested, not compiled in) · Depends on: none. PREREQUISITE for M5, M7, M8, M11.**

`engine/src/v05_target.hpp` is written, argued and unit-tested (`probe_m45_test.cpp`) and is
**included by nothing**: `v05.hpp:22` includes only `v04.hpp`, and
`grep -rln v05_target.hpp src/` returns only the probe. The structural claim — the public deduction
state is a single object, not six — makes per-seat modelling ~O(1) rather than 6×.

*Defect:* Loss channel #2 (9.74% guaranteed-miss asks, 21.97% of all misses), plus the 5.55-event
allocation-proof delay.

*Evidence:* R3 — soundness direction (i) is **empirically exact: 0 unsound fires in 2,532,636
(observer, opponent, card) checks** across v0.5 and v0.4 mirrors; both patches compile clean at
`-O3 -Wall -Wextra`; both off-switches reproduce shipped v0.5 to every printed digit. R9 — v0.5
still builds no knowledge model of any other seat (`v05.hpp:543`, `:560` are both clones of its
own). R6/R1/R9 all measure the waste channel independently at 9.74/9.77/10.16%.

*Known defects to fix on the way in, all from R3:* the patches do not apply to HEAD (both fail at
`v05.hpp:114`; `patch -p1 -F 3` recovers each with one fuzz-2 hunk); M4/M5 and M7 collide at five
identical anchors and must be hand-merged once; the M5 weights are **mis-scaled by 1/linearWeight**
(0.75393) because `m45.score()` is added after `u *= cfg.linearWeight`; `postMissLockout`
(`v05_target.hpp:386`) uses **model ignorance as evidence of target ignorance** — because
`mw[LOCKOUT]` is negative, a suppressed leak scores the ask *higher* — with 36.4–40.9% recall on
real one-survivor leaks.

*Gate:* **(i)** `asks in own-locked` falls below **3.0%** in the v0.5 mirror (from 9.74%) and below
**4.5%** pooled against every one of the six panel styles; **(ii)** `probe_ignorance` still reports
0 unsound fires over ≥ 2M checks; **(iii)** mean events from physical-ownership to
allocation-proof falls below 3.0 (from 5.546); **(iv)** paired delta ≥ +0.15 sets/game with the
tuned `mw[]` (R3 measured 49.0% [45.3, 52.8] at the shipped *placeholder* weights — the mechanism
has never been fitted).

---

### V6-M4 — Memoise `proposeDeclaration` across seats and events
**Payoff: high (enabling) · Difficulty: low · Depends on: none. FUNDS M8, M9.**

*Defect:* Loss channel #4. 87.2% of runtime for a decision worth 0.46 sets; five of six polls
discarded by lowest-seat arbitration (`game.hpp:202-231`); nothing cached.

*Evidence:* R1 `r1_split` (114,486 calls / 200 games at 61 µs; 7.030 s of 8.058 s); R2 (Fast path
cost is dominated by the declaration path, not the two-ply search: `topk=1,declare=0` = 0.11 s vs
`v05` = 1.56 s over 20 games); R10 (`v05:topk=0` = 348.3 µs/event).

*Gate:* **(i)** ≥ 3× games/s on the mirror bench; **(ii)** **bit-identical play** — assert identical
event streams over ≥ 1,000 games at three seeds, and identical `pathology` output to every printed
digit. A memoisation that changes any decision is a bug, not a mechanism.

---

### V6-M5 — LBR / best-response exploitability auditor, built BEFORE any search work
**Payoff: high · Difficulty: medium · Depends on: V6-M2 (needs the paired estimator and a
parameterised harness)**

*Defect:* Loss channel #6. No exploitability number exists for v0.5 at all, and M1 is an unaudited
naive knowledge-limited pruning rule.

*Evidence:* R7 — Zhang & Sandholm (arXiv:2106.06068) *prove* naive knowledge-limited solving can
increase exploitability; Brown et al.'s entire motivation is robustness; MDS's guarantee does not
cover two-teams-of-three. R4 — the machinery already exists: `exploitability.sh:19-31` fits an
exploiter via `tune --panel=<target> --beta=1 --sigma=0.7 --seed=515253` and re-measures on fresh
bank 6543210, but is hard-wired to v0.4 (lines 9, 23, 27). Budget: 12 gens × 18 pop × 360 games =
**77,760 games/target = 3–6 min** at measured rates. R5 — v0.4's baseline response achieves
51.19% [49.67, 52.72].

*Gate:* **(i)** the probe reproduces v0.4's published 51.19% to within its CI as a positive control;
**(ii)** it returns a number for `v05`, `v05:m1=0` and `v06`; **(iii)** v0.6's best-response margin
is **no worse than v0.5's**, reported with the sweep over when the responder may act and the **max**
taken (both poker pitfalls honoured — a negative LBR score proves nothing).

---

### V6-M6 — Multi-valued leaf states: replace `value()` with `min_j value_j()`
**Payoff: high · Difficulty: low · Depends on: V6-M5 (soundness argument is 2p0s applied to a team game)**

Brown/Sandholm/Amos at depth 1. The k continuation strategies the method needs **already exist
compiled**: `engine/src/factory.hpp` dispatches v05/v04/v03/v02 and `baselines.hpp:131-395` supplies
Hunter, Detective, Lockout, Diversifier, Bluffer, Random.

*Defect:* Loss channel #5. v0.5 is exactly the single-value-function configuration proved unsound;
the leaf value is a 1.79% rescaling of `p` (R² 0.840) with 41.5% of its coefficient mass on
features that are exactly constant across candidates.

*Evidence:* R7 (the keystone gap — arXiv:1805.08195 absent from all nine prior lit files; master-level
HUNL on 4 cores / 16 GB); R1 (the anatomy above, plus: the value term flips the pre-search argmax at
0.94% of decisions but still earns +1.9 points as a tie-breaker on the search *input* set, so it must
be **replaced, not deleted**).

*Gate:* R7's own falsifier, adopted verbatim: **if `min_j`, `mean_j` and single-`value()` are within
noise on the six-style matrix AND have equal LBR bounds (V6-M5), the mechanism is inert and is
retired.** Positive gate: leaf-value spread across the k continuation policies is non-degenerate
(sd > 0.05 sets) at ≥ 25% of decisions, and `min_j` improves the panel **worst case** by ≥ 0.10 sets.

---

### V6-M7 — Depth-limited rollout search on the 9.07 µs/event blueprint
**Payoff: high · Difficulty: high · Depends on: V6-M4 (compute), V6-M6 (leaf values), V6-M5 (audit), V6-M1 (candidate ordering)**

Replace the top-K chain/threat heuristic (`v05.hpp:520-585`) with a real expectimax
(my ask → opponent reply → my follow-up), candidate-restricted at **every** ply by the M1 live-ask
predicate, leaves evaluated by V6-M6.

*Defect:* The current "two-ply" is depth-2 along one hand-picked line using max-posterior-marginal
scalars; no opponent policy is consulted, no recursion, no backed-up values (R7).

*Evidence:* R10 §3 — determinization is free (`DealDP::build` 92.8 µs once per decision, one exact
posterior sample 0.54 µs, six-seat information-state reconstruction 8.7 µs) and **only the rollout
costs anything, at 3.3 µs/event**; a 20–24-event depth-limited rollout is 180–220 µs, so 128
rollouts ≈ 25 ms/decision. R8 — a 10 ms budget affords ~900 exact BlockDP rebuilds at Q≤12 (depth
~2.7 with the existing top-6 set); branching is mean 69.09 legal / **43.50 live** so restriction is
mandatory at every ply. R10 §2 — by median event 76 the hidden state is one of ≤10^5 possibilities
while **33.8% of half-suits are still undecided**, so an exactly-enumerated endgame covers a third of
the scoring.

*Gate:* R7's alpha-mu falsifier: **a good search differs from the baseline on 1–3% of decisions;
a search that differs on 40% is mis-scaled, not smart.** Plus: paired delta ≥ +0.25 sets/game;
worst panel style no worse than −0.10 sets; LBR margin (V6-M5) not worse than v0.5's; and
`fish pathology --gate` clean (events/game p99, longest dead run, action-limit games).

---

### V6-M8 — Recalibrate `pAlloc` and the declaration thresholds against measured under-confidence
**Payoff: medium · Difficulty: low · Depends on: none (prerequisite for any belief-mode swap)**

*Defect:* The deployed `pAlloc` is **1.94× the exact posterior probability yet under-confident
against ground truth**, and `declThreshold = 0.81991` (`v05.hpp:60`) rejects a band that is right
79% of the time.

*Evidence:* R2 bench4, 4,064 half-suit evaluations — the 0.5–0.8 band (n=68) has mean predicted
0.591, mean *exact* 0.224, and is **actually right 79.41%**; the 0.2–0.5 band (n=142) is right
50.00%. Corroborated by `./fish calibrate`: decl bin [0.7, 0.8) predicts 0.7481, observes 1.0000
(n=22). R6 independently: meanPred 0.9645 vs meanObs 0.9839; [0.7, 0.8) predicts 0.759 observes
0.955 (n=112). Declaration payoff is +1/−1, so break-even is 0.5.

*Critical corollary:* **any belief-mode swap without a calibration map will declare strictly less
often and lose points for the wrong reason.** This is the likeliest single confound in the
"exact inference loses 6.20 points" result.

*Gate:* Reliability diagram flat to within ±0.03 across all bins with n ≥ 30; declarations/game
rises above 4.49 while wrong-declaration rate stays ≤ 2.90% in the mirror; **measured against the
misdeclaration rate, not assumed free** (R6).

---

### V6-M9 — Joint allocation naming: make `feasibleAllocation` maximise the joint
**Payoff: medium · Difficulty: low · Depends on: V6-M4 (funds the cost)**

*Defect:* `feasibleAllocation` picks the MAP allocation by a **product of unconditioned marginals**
(`v05.hpp:661`) over a search space of at most 729 (mean 165 enumerated) and only joint-scores the
winner. `belief.hpp:557-563` states why that differs from the joint.

*Evidence:* R6 — **72–75% of v0.5's remaining misdeclarations are pure allocation errors**: the team
held all six cards and named the wrong teammate (46/64 for v0.5, 40/53 for v0.4). Proof-backed
declarations are never wrong (0/1230); all 4.35% of error lives in the belief-only 54%.
R2 O6 — `pTeam` on the Fast declaration path is an independent product (`v05.hpp:713`, `:736`),
mean |pTeam − exact| = 3.04e-02, max 1.00 over 1,419 evaluations.

*Honest sizing:* the whole channel is 1.96% of half-suits ≈ 0.18 sets/game, so the ceiling here is
~0.13 sets. Ranked below M1–M8 for that reason, not for difficulty.

*Gate:* belief-only misdeclaration rate falls from 4.354% below 2.0%; proof-backed stays at 0.000%.

---

### V6-M10 — Repair the three sign-broken ask weights before any refit
**Payoff: medium · Difficulty: low · Depends on: V6-M2 (otherwise the refit re-learns them)**

| feature | v0.5 weight | pathology |
|---|---:|---|
| `f[14]` binEnt(p) (`v05.hpp:54`) | **−2.42663** | a **negative** value-of-information term, and the **2nd-largest discriminator** in the entire score (contribSpread 1.068) |
| `f[16]` exposureOnMiss (`v05.hpp:56`) | **+2.53330** | **rewards** handing the turn to a well-placed opponent |
| `f[12]` repeatsSet (`v05.hpp:52`) | **+1.38026** | an explicit perseveration bonus, **up** from v0.4's +1.2697; 50.24–50.63% of asks repeat an (actor, half-suit, target) triple |

*Evidence:* R1 (weights, contribSpread, repeat rate re-measured this session at 50.63% mirror /
45.65–51.96% across styles); R7 (`f[14]` is why v0.5 **charges** for information and cannot express
a self-explaining deviation even in principle); R9 (all three information features — `f[9]`, `f[16]`,
`f[19]` — price leakage to opponents and **none** prices gain to the team).

*Gate:* after re-parameterisation and refit under V6-M2, all three coefficients have the
theoretically defensible sign, and the paired delta vs v0.5 is ≥ 0 (this is a hygiene fix, not a
strength claim — it must not *cost* anything).

---

### V6-M11 — Offensive void value: reward asks that strip an opponent's last card of a half-suit
**Payoff: medium · Difficulty: low · Depends on: none**

`legalAsk` (`fish.hpp:164`) requires the actor to hold a card of the set, so voiding an opponent
**permanently destroys their right to ask there** — a one-way asset.

*Evidence:* R8 — **41.65% of v0.5's successful asks already do this by accident** (8,063/19,358) and
35.07% of (live opponent, active set) pairs are already voids (202,653/577,788), yet **nothing in the
20-feature ask score rewards creating one**. The defensive dual *is* implemented (`v04.hpp:214-222`
feeding `f[8]`). The nearest offensive feature, `f[11]` (`v04.hpp:320`), fires only when the target's
whole hand is one card — 7.55% of asks. P(this hit takes the target's last card of S) is closed-form
from BlockDP marginals, so the term is exact and costs nothing.

*Gate:* void-creation rate rises above 45% of successful asks with the paired delta ≥ 0 and no panel
style worse than −0.10 sets. Note R9's counterweight: a fully locked-out opponent exists at only
1.25% of v0.5 decisions (v0.4: 12.98%), so the *terminal* value of the asset is small under v0.5's
96-event games — this term is worth building because it is nearly free, not because it is large.

---

### V6-M12 — Per-seat policy specs, enabling the partner-regime split (owner decision D2)
**Payoff: unknown · Difficulty: medium · Depends on: V6-M2**

`runMatch` builds one policy for all three A seats (`arena.hpp:68`, `:88-92`), so "FishBot with two
bot partners" and "FishBot with two human-model partners" **cannot be expressed**. The mechanism
already exists privately as `deceit`'s `--dseats` bitmask (`main.cpp:717-721`) — lift it into
`arena.hpp` and delete the duplicate.

*Evidence:* R4 H6; R5 (owner decision D2 of record: report bot-teammate and human/unknown-teammate
regimes separately and never headline the self-play configuration); R9 (chooseAsk is a plain argmax
at `v05.hpp:512-518`, `:536-583`; the agent's `rng` reaches only belief sampling — a deterministic
policy is maximally exploitable by a human who has played many games, the owner's live report #4).
R7 S9 — **any randomisation must be seeded from the public history** or Fish's free
multi-agent-common-knowledge property (SPARTA) is lost.

*Gate:* `TuneSpec::panel` entries become (opponentSpec, partnerSpec) pairs; the v0.6 results table
reports the two regimes as separate columns with separate worst cases; and a temperature sweep shows
the exploitability margin (V6-M5) is monotone in temperature, with λ=∞ / T=0 recovering the
deterministic policy **exactly** as a guaranteed floor (R7 S3, piKL).

---

## 3. DO-NOT-REBUILD

Drawn from the 24-row rejected register plus this recon's own negative results. Each line is a
mechanism that a plausible v0.6 design would otherwise re-derive.

1. **A repetition guard as a termination backstop.** `v05:norepeat=1` scores **42.93% against
   shipped v0.5's 49.07% — −6.13 points**, the largest single-switch effect ever measured in this
   codebase (E5, seed 606060). Cards move; a repeat after a public transfer can be the best ask on
   the board. M1 already subsumes the case the guard was built for.
2. **A determinized / PIMC endgame solver.** R8's theorem, verified 300/300 games: under perfect
   information the team on turn wins every half-suit except those dealt outright to the opponents
   (value 8.867–0.133). **Every determinization returns "we take everything", so all legal actions
   score alike and the average cannot discriminate** — maximal strategy fusion. The 10 ms
   enumeration threshold at Q=13 is real but is a threshold for a mechanism that will not work.
3. **Any scoreboard-selected configuration from the E5 table.** M8-alone scores 56.60% and M2+M8
   56.40% against v0.4 while carrying **44.83% dead asks, a 373-ask dead run, and 14% of games killed
   by the action limit**. Win rate and soundness are dissociable in this family; a v0.6 gated on a
   scoreboard will re-select the broken policy.
4. **Forced-endgame work of any kind.** Incidence is 6 of 5,400 half-suits = **0.11%** in the v0.5
   mirror and 0.0056/game head-to-head. R5's +6.0 points of forced-endgame accuracy from a
   best-positioned declarer is +6.0 points of a 0.11% channel. Explicitly recorded as a
   non-opportunity (R6 V6-9, R8).
5. **Confidence-ranked declaration arbitration (+0.30 pp), willingness-bit turn transfer
   (0.148 events/game; a ground-truth chooser is worth −0.0007 ± 0.0024 sets/game), and a stronger
   provability oracle for the D13 channel (0.34% → 0.544%).** All three measured near zero (R9 V6-12).
6. **A positive holding cost as the stopping fix, as previously implemented.** Swept: mean-shifter
   only, p90 313 → 311, longest run 289 → 289, **−3.9 points** (DESIGN.md §0.3). The literature
   remedy does not apply because in the v0.4/v0.5 stopping model waiting is free *by construction* —
   `V(wait at t) ≡ V(wait at t+1)` is an implementation identity (`v05.hpp:795`, all-zero deltas;
   `value()` has no time input). Fix the identity, not the cost.
7. **Learned Belief Search, approximate belief models, or any neural belief.** Belief is a **solved
   problem** in Fish: `blockdp.hpp` computes Z, marginals, P(team owns half-suit) and
   P(named allocation) exactly in closed form, brute-force-validated (`fish oracle`: Z 0.000e+00,
   marginals 0.000e+00 over 78,516 checks). Only *search* is open.
8. **`BeliefMode::Hybrid` and `BeliefMode::Exact`.** Dominated points on the cost/accuracy frontier:
   Hybrid's ask marginals are plain sinkhorn with no certificates and no prior, yet it costs
   4,149 µs/event — more than full Block in one of two samples. Both silently delete the policy prior
   and therefore confound every belief-mode ablation (R2 O7).
9. **`priorPhi` as an independent parameter.** Provably absorbed by Sinkhorn's column normalisation:
   `z = (θ+φ)·a − φ·totalAsks`, second term seat-only. Verified at max 1.291e-04 / mean 1.236e-08 on
   900 real states where the clip does not bind, against a 180× larger control. The **only** residual
   channel is the ±2.6 clip on the full exponent (`belief.hpp:106`). Collapse to one `θ_eff = 0.56656`
   and give the CEM its coordinate back.
10. **`patientLocked` / `lockedAllocThresh` as tunable knobs.** Unreachable under the shipped
    `useValue && valueDeclare` configuration (only reference `v05.hpp:808`, guarded by `:803-806`);
    `v05:patient=0` vs `v05` is **bit-identical** (50.00%, 4.500–4.500, ask accuracy 56.19%/56.19%).
    Any ablation on it measures nothing. Resurrect it inside a reachable branch or delete it.
11. **TB-DAG subgame solving for adversarial team games** (Zhang et al., NeurIPS 2022). Verified this
    recon: its polynomial-time escape hatch is **conditional on blueprint sparsity**, and a Fish
    blueprint is ~69 actions × ~96 decisions — not sparse (R7).
12. **Deleting the value function outright.** It is a 1.79% rescaling of `p`, but it still earns
    **+1.9 points** as a tie-breaker on the top-K search input set. Replace it (V6-M6); do not remove
    it. R1 flags that the value=0 and topk=0 ablations were measured **separately and may not
    compose**.

---

## 4. Compute budget

### 4.1 Measured this session
Apple M5 Pro, 15 cores, **load average 30.4** (concurrent recon agents — all figures are therefore
*depressed*; R4's quiet-machine measurements are quoted alongside):

| spec | threads | games/s (this session) | games/s (R4/R10, quiet) |
|---|---:|---:|---:|
| `v05` mirror | 15 | 132.8 / 135.6 / 143.7 / 162.0 | 251–289 |
| `v05:topk=1` | 15 | 150.6 | — |
| `v05:belief=indep,topk=0` | 15 | **7,753.9** | — |
| `v05:belief=indep,topk=0` | 1 | **710.8** | 873.9 |
| `v05` | 1 | 17.3 | 21.7 |
| `v04` mirror | 15 | — | 83.4 (a 3.1× compute tax from the deadlock) |
| `v05:belief=block` | 15 | — | 21.7 (14× slower) |

**The rollout blueprint is 54× the deployed policy on identical hardware** (7,754 vs 143.7 g/s),
confirming R10's 62×-per-event figure at the match level. Per-event: `v05` 560.2 µs,
`v05:topk=0` 348.3, `v05:belief=indep,topk=0` **9.07**.

Planning rule (R4, validated against E3's five cluster-bootstrap widths of 4.33–4.94 pts):
**half-width in points = 98 / sqrt(N_games)**, because the 6-rotation duplicate block fully cancels
deal-level luck, so effective n = games, not deals. Mirror runs at `--rotations=2` are **exactly 50%
duplicated work** (verified: every count doubles, every rate identical to the last digit).

### 4.2 What that buys

| activity | games | wall clock @ 250 g/s | verdict |
|---|---:|---:|---|
| one 40-gen CEM at v0.5's schedule | 2,308,800 | **1.8 h** | affordable |
| E3-style head-to-head cell (±2.3 pt) | 1,800 | 7 s | free |
| resolving a **1-point** improvement, unpaired | ~5,800 games/**cell** | **~26 h** for a 40-gen fit | **NOT affordable** |
| the same, paired CRN (29× variance reduction measured) | ~200–800 games/cell | **1–3 h** | affordable |
| exploitability probe, one target | 77,760 | **3–6 min** | free |
| 40-gen fit **with a 25 ms/decision search in the loop** (≈17 g/s) | 2,308,800 | **~38 h** | **NOT affordable** |
| E3 cell with the search on (1,800 games @ 17 g/s) | 1,800 | 106 s | affordable |

**Three budget conclusions, in order of consequence:**

1. **Per-cell noise, not wall clock, is the binding constraint.** V6-M2's paired estimator is not a
   nicety — it is the difference between a 1.8 h fit and a 26 h fit at the resolution v0.6 needs.
2. **A search-based v0.6 cannot be fitted with the search in the inner loop.** Fit on the
   9.07 µs/event blueprint (or with search off) and *evaluate* with search on. R5's warning applies:
   frozen-parameter ablations understate any mechanism whose parameters were fitted for a different
   policy.
3. **Panel budget is 98% misallocated.** At beta=25 over v0.5's best generation, the gradient weights
   are [0.9829, 0.0103, 0.0028, 0.0040, 0.0001, 0.0000] for v04/v03/lockout/detective/diversifier/hunter
   while cell times split 25.5/20.5/12.8/12.2/15.4/13.7%. **Five of six cells carry 1.71% of the
   gradient for 74.5% of the compute.** Keeping only v04+v03 (99.3% of gradient) frees 54%. *But*
   this is a statement about the **soft-min** objective; V6-M2's minimax-regret objective changes
   which cells matter, so prune only after H1 lands.

### 4.3 Caveat
`./fish bench --a=v06 --b=v06 --games=20` did not complete in 120 s in this session (v0.5 does 20
games in ~0.15 s). `engine/src/v06.hpp` and `engine/src/v06_rollout.hpp` are untracked work in
progress from a concurrent agent and `engine/src/factory.hpp` has 92 uncommitted insertions. **No
budget figure in this report was taken against v0.6 code**, and the v0.6 slowness is reported as an
observation, not a finding.

---

## 5. The three measurements v0.6's design is blocked on

### Q1 — Does the exact posterior's finer *resolution* convert into wins once the weights are fitted for it?

This is the sharpest unresolved contradiction in the corpus, and it sits directly under the #1 loss
channel.

- The exact C1–C5 posterior is a **worse predictor** of the true deal than the shipped approximation
  against every modelled opponent, and the advantage **inverts exactly against `random`**
  (R2 bench6: vs v05 Block NLL 1.4245 / Fast 1.3817; vs random 1.3393 / 1.3379). "Exact" means exact
  *under a uniform-deal prior*, which discards the soft policy information in the public record.
- `belief=block` loses 39.83% / 40.33% / 35.83% at three seeds and runs 14× slower (R6, R8).
- **Yet `belief=block` breaks 10.9 points more of the exact argmax ties (43.36% vs 54.22%, R6)** —
  and the Fast belief **cannot break them at all**, because `sinkhornDisj` makes exchangeable cards
  identical by construction.

Read together: **Fast is better calibrated; block is better resolved.** The +3.16-set headroom lives
in resolution. The 6.20-point published loss for exact inference is confounded three ways — weights
fitted under Fast (R5, `paper/sections/11-discussion.tex:93-95` names the matched refit as unrun),
the policy prior silently deleted (R2 risk 6), and `pAlloc` recalibration never applied (V6-M8).

**Measurement:** matched-budget refit under `belief=block` (or under the soft-C5 fusion of R2 O1 /
R6 V6-2), then a paired head-to-head *plus* the six-style panel. Cost at 21.7 g/s: a 12-generation
reduced-panel fit is ~200k games ≈ 2.6 h. Report the tie-break rate and the argmax agreement
alongside the win rate — the win rate alone has already misled this project once.

**Gating dependency:** must not be attempted before the `BlockDP::build` `thread_local` table
aliasing is fixed (`blockdp.hpp:83-97`, `:175-176`; measured **285 mismatches in 294 checks**).
Harmless under Fast, fatal under block — which is exactly how it survives into a release.

### Q2 — What fraction of the +3.16-set askoracle bound is reachable by a *causal* tie-break?

Nobody knows whether it is 5% or 50%, and the answer decides whether V6-M1 is v0.6's headline or a
footnote. The bound is **hindsight** (R6 risk 2): it resolves the tie using ground truth.

**Measurement:** build the three cheapest candidate signals and measure the realised fraction of the
bound for each, independently, before building anything expensive:
(a) BlockDP count-law resolution — cost 0.0111 ms/rebuild at Q=12 (R8), and R2 O1's soft-C5 weight
is a **filter-to-weight edit at zero asymptotic cost** at `blockdp.hpp:133-138`, exploiting the fact
that a real ask carries **2.33 mean other-cards-of-S** (P(k≥2) = 72–77%) against `random`'s 1.51 /
40.7% (R2 bench7);
(b) per-card ask history and rank-position priors — free;
(c) a teammate-derived per-card term via V6-M3.
Report each as *fraction of the askoracle bound realised*, on the same seed bank, paired.

**Second-order but decisive:** measure the tie rate **against every panel style, not just the
mirror**. R6 spot-checked v0.4 at 52.51% and nothing else. If the tie rate collapses against
deceptive archetypes, the mechanism's robustness profile is entirely different from its mirror
profile.

### Q3 — What is v0.5's exploitability, and does M1 increase it?

Currently **unmeasured for v0.5** and named as "the single largest hole in the evaluation" by v0.5's
own paper. Until it exists, **v0.6 can claim no robustness property at all** — which, given the
owner's standing preference, means v0.6 cannot be published on its own terms.

The specific hazard is not hypothetical: M1 is a naive knowledge-limited pruning rule and
Zhang & Sandholm *prove* that class can increase exploitability. M1's measured profile is exactly
the shape that hides such a cost — **zero head-to-head points (50.75% [49.20, 52.30], n=4,000) with
a huge pathology effect** (0% vs 46.28% dead asks). A mechanism that changes behaviour that much for
no measured gain is either free structure or a leak, and mirror self-play cannot tell the difference
(R1's standing warning: mutual deadlock is resolved by neutral adjudication, so both sides lose
equally).

**Measurement:** parameterise `exploitability.sh` (BASE/OUT/SPECPREFIX), positive-control it against
v0.4's published 51.19% [49.67, 52.72], then run it on `v05`, `v05:m1=0` and each v0.6 candidate.
77,760 games/target ≈ 3–6 min. Sweep when the responder may act and **report the max**.

Related and cheap, from the same budget: `v0.5`'s deception exposure is a *fitted* parameter that
provably cannot be tuned away — `priorTheta` went 0.2638 → 0.4446 in the v0.5 fit, costing 2.2 points
against the feint at both banks, and a five-point sweep at two seeds does not identify it (R5).
Any v0.6 refit must be re-measured against the deception panel before it ships.

---

## 6. Cross-report contradictions and corrections recorded

1. **"Exact inference is worse" vs "exact inference breaks more ties."** Both true; they measure
   calibration and resolution respectively. Resolved in Q1 above. Do not cite the −6.20 / −0.71-set
   figures as evidence that exact inference is harmful without the matched refit.
2. **M7 sign.** R3 measured M7 as **negative against `withholder:k=6`, the archetype it was built
   for** (71.17% ON vs 72.33% OFF), and the two best cells at `--rotations=1` were the ones that
   *remove* the silence channel (`m7sil=0` 77.0%, `m7carry=0.95` 76.3%, vs `m7=0` 71.0% and default
   72.0%). All at n=300–600, none separating. R5 lists M7 as high-payoff on design grounds. **The
   register places M7's *substrate* (M4, = V6-M3) high and M7 itself nowhere** until R3's
   `V06-M7-SILENCE-TEST` runs. M7's calibration is also derived at v0.4's `θ_eff = 0.39660` against
   v0.5's actual 0.56656 — 43% larger — so every quoted separability and safety number describes a
   parameter point v0.5 no longer occupies.
3. **Defect H (stale `computeAggregates`) is real but inert.** `eH` differs pre- vs post-refresh at
   **92.52%** of gated declaration opportunities (mean max|dEH| 0.10057) but flips the verdict at only
   **0.30%** — precisely because `declareByValue` is a `pAlloc` threshold in which `eH` enters as a
   small additive constant (R1). Fix it when V6-M6 replaces the value function, not before.
4. **Two false claims in `docs/V04_FINDINGS.md` remain unretracted and are absent from the v0.5
   corrections register** (R5): `:77-80` says the fitted adversary "fails to reach 50%" against v0.4
   while `paper/tables/lbr.tex` records 51.19% [49.67, 52.72]; `:66-68` says value-based declaration
   "is worth measurable win rate", contradicted by that study's own `vdecl=0` ablation
   (+0.12, CI −1.23 to +1.47). Two further register corrections (C7, C9) never landed in any paper.
   These are v0.6 corrections-section obligations.
5. **`gateaudit` is dead code for v0.5.** `factory.hpp`'s v0.5 branch (`:39-128`) never parses the
   option; only the v0.4 branch does (`:141`). `./fish gateaudit --a=v05:gateaudit=1` returns a
   **vacuous PASS with 0 opportunities**, while the equivalent v0.4 audit does **not** pass
   (132 false negatives, 132 actions changed). One line restores an existing commit gate (R6 V6-6).
6. **`fish m7check` is cited three times in `docs/FISHBOT_V05.md` and `research/v05/DESIGN.md` and
   does not exist** in `main.cpp`'s dispatch chain (R4 H12). Build it or retract the citations.

---

## 7. Latent defects to fix before the code they sit under is used

| defect | location | status |
|---|---|---|
| `BlockDP::build` `thread_local` table aliasing | `blockdp.hpp:83-97`, `:175-176` | **285 mismatches / 294 checks.** Gating defect for all exact-belief work. |
| `BruteForce::enumerate` uninitialised stack at `nU==0` | `oracle.hpp:102`, `:108-113` | ASAN stack-buffer-underflow; **2.85% of real decisions are at Q=0**. Shipped `fish oracle` is safe only via the `main.cpp:353` guard. One-line fix. |
| `DealDP::countWithMasks` greedy approximation inside a function documented as exact | `belief.hpp:424-426` | Unreachable from all current callers (verified to 1.7e-15); a v0.6 caller passing heterogeneous non-singleton masks gets a **silent** approximation. |
| `BlockDP::bestTeamAllocation` does not re-check C5 survival | `blockdp.hpp:425-449`, `:482-492` | On the decision path (`v05.hpp:730`, `:932`). 0 failures in 35,161 checks — unfalsified, unproven. |
| `enumerateGroup` silent truncation at MAXENT=512 | `blockdp.hpp:397` | Leaves a **partial** count-vector table that `teamOwnsProbability` normalises as if complete. Currently unreachable (max C(11,5)=462) — a silent failure mode if SETSZ or MAXENT move. |
| `Belief::marg` never zero-initialised; `V05Agent::reset` never refreshes | `belief.hpp:454` | The first `proposeDeclaration` of every game (`game.hpp:294`) reads `bel.marg` before it has ever been written; masked only because `factory.hpp:40` uses `make_unique`. |
| `Knowledge::onEvent` appends duplicate Disjunctions | `belief.hpp:168-171` | Certificate store grows 6 → 26 → 46 with **no information**, making the block DP progressively more expensive. |
| `tuner.hpp:35` hard-codes `w.size() > 18` where NFEAT (=20) belongs; `tuner.hpp:59` hard-codes `st.games * 2` | `tuner.hpp` | Same class as the v0.4 aliasing defect; the second means fitting at `--rotations=6` would silently report a third of the true win rate. |
| `chooseAsk` scores every candidate **twice** at `searchTopK=6` | `v05.hpp:512-519` vs `:526-533`, dead reader at `:588-590` | Pure waste; ~15 further dead sites catalogued in R1 V6-10, each a knob the ablation table may be reporting as measured. |
| `collectCalibration` uses the wrong rotation coupling | `arena.hpp:212`, `:215` | Dormant only because `calibrate` never passes rotations. |
| No unknown-flag detection in `main.cpp:28-38` | — | A mistyped `--rotation=6` silently runs at 2. |

---

## 8. Recommended build order

```
Phase 0 (must precede everything)   V6-M2 harness   +  latent defects §7 rows 1-2, 8
Phase 1 (cheap, independent)        V6-M4 memoise   +  V6-M10 weight signs  +  V6-M11 void
Phase 2 (the headline)              V6-M1 tie-break +  V6-M3 shared knowledge      [Q1, Q2 first]
Phase 3 (audit before search)       V6-M5 LBR       +  V6-M8 recalibration          [Q3]
Phase 4 (the research bet)          V6-M6 multi-valued leaves -> V6-M7 rollout search
Phase 5 (reporting obligations)     V6-M12 partner regimes  +  §6 corrections
V6-M9 slots anywhere after V6-M4; it is worth ~0.13 sets and is not on the critical path.
```

The single most important structural statement in this register: **v0.5's headline mechanisms
(M1, the two-ply search, the value function, the 40-generation refit) are collectively worth
approximately zero head-to-head, while a defect that decides more than half of all asks by array
order has never been addressed.** v0.6's opportunity is not incremental.

---

*Sources: `research/v06/notes/R1`–`R10`. Re-measured in this session: mirror and six-style pathology
KPIs (`./fish pathology --a=v05 --b=<opp> --games=100 --seed=31`), throughput for `v05`,
`v05:topk=1`, `v05:belief=indep,topk=0` at 1 and 15 threads. Machine load average 30.4 throughout;
throughput figures are lower bounds.*
