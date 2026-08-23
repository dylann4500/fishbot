# R5 — State of knowledge register (v0.3 → v0.5), for FishBot v0.6

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, HEAD `bd812fe`
(`bd812fed0e42f9b873e148aef4d592b4c2db437e`, "v0.5"), 2026-08-23. Recon only; nothing under
`engine/src/`, `paper/` or `docs/` was modified.

Sources distilled: `paper/fishbot_v03.tex`, `paper/fishbot_v04.tex` + `paper/sections/*` (22 files),
`paper/fishbot_v05.tex` + `paper/sections_v05/*` (12 files), `docs/FISHBOT_V03.md`,
`docs/FISHBOT_V04.md`, `docs/FISHBOT_V05.md`, `docs/V03_FINDINGS.md`, `docs/V04_FINDINGS.md`,
`docs/V04_RESULTS.md`, `docs/V05_FINDINGS.md`, `docs/METHODOLOGY.md`,
`docs/EXTERNAL_STRATEGY_REVIEW.md`, `research/v05/RESULTS-SUMMARY.md`,
`research/v05/results/C1-v04-corrections.md` (1,334 lines — **the corrections register of
record**), `research/v05/{BRIEF,DESIGN,PAPER_PLAN}.md`, and the P0–P8 diagnosis reports where a
figure needed sourcing.

**Engine state checked, not assumed.** `cd engine && make` → up to date; binary present.
Sanity run `./fish match --a=v05 --b=v04 --games=100 --rotations=6 --seed=90210` returns
50.667% [46.83, 54.50] cluster-bootstrap, 100.4 events/game, 0% limit hits, 245.7 games/s on this
machine — consistent with the recorded 50.11% on the full 300-deal 90210 bank
(`research/v05/results/E3-headtohead.jsonl`) and with E9's 286 games/s. The register below is
therefore reproducible at HEAD.

Status vocabulary used throughout:
**STANDS** — tested with intent to break, held. **CORRECTED** — false, and the v0.5 paper states
the correction. **UNRETRACTED** — false or misleading, and the text still says it at HEAD with no
correction anywhere. **SCOPE** — true as measured, misleading as generalised.

---

# (a) THEOREMS AND CLAIMS OF RECORD

## a.1 The six formal (theorem-environment) claims — all in the v0.4 paper

v0.3 states no formal claims: `paper/fishbot_v03.tex` has no `\newtheorem`, and its "three bounded
claims" (`fishbot_v03.tex:44-49`) are empirical. v0.5 states none either: `sections_v05/` contains
zero theorem environments. Every formal object in the corpus is in the v0.4 paper.

| # | Object | Location | Statement | Status |
|---|---|---|---|---|
| F1 | Proposition (Public transfer) | `paper/sections/05-belief.tex:10` | Every card movement is publicly observed, so a card whose location was never revealed is still with whoever was dealt it; the hidden state is exactly the initial deal | **STANDS**. Load-bearing for everything downstream; never challenged |
| F2 | Corollary (Equal probability within a count vector) | `paper/sections/05-belief.tex:207` | Under the uniform prior, every *surviving* allocation sharing a count vector has identical probability, so the MAP allocation is fixed by the count vector alone | **STANDS**. Confirmed by oracle: `named allocation prob max abs diff 0.000e+00 over 227062 checks`, `ORACLE PASS` (`P1-verify-e11-information.md` §4) |
| F3 | **Theorem 1 (Locked half-suits)** | `paper/sections/06-locked.tex:12` | If every card of a half-suit is held by one team, no opponent can legally ask in it; the half-suit is frozen until claimed | **STANDS**. Verified at the level of the rules and in every measurement (C1 §S1) |
| F4 | Corollary (Waiting carries no ownership risk) | `paper/sections/06-locked.tex:40` | The "steal" risk of waiting on a locked half-suit is exactly zero | **STANDS** (C1 §S1) |
| F5 | Observation (Ownership freezes; allocation information need not) | `paper/sections/06-locked.tex:53-66` | Ownership monotonicity alone establishes neither informational freezing nor non-termination | **STANDS — and vindicated.** The v0.4 *paper* got this right; its supporting artifacts got it wrong (see C1 below). Support monotonicity confirmed: asks outside a locked half-suit shrank its support in **0 of 20,898** cases (C1 §S2) |
| F6 | Observation (Observed non-termination in mirror self-play) | `paper/sections/06-locked.tex:170-179` | "An earlier fitted configuration … failed to terminate within the action cap in 21.0% of games … a property of that fitted configuration, not a theorem about the rules dialect" | **OVERSTATED in v0.4's favour** (register C11). The **shipped** configuration cycles at the same rate: 22.5% at seed 31, 23% at seed 777001; with the forcing rule active it still produces dead runs to 286 asks in 34.3% of mirror games |

## a.2 The twelve-entry corrections register (C1–C12), and where each landed

`research/v05/results/C1-v04-corrections.md` is the register of record. Verdicts are its own.
"Landed" = whether the v0.5 paper (`paper/sections_v05/`) states the correction in print.

| # | v0.4 claim | Where it is written | Verdict | Landed in v0.5 paper? |
|---|---|---|---|---|
| C1 | A locked half-suit is informationally frozen, and that is why patient policies deadlock | `research/v04/results/E11-termination.md:3-7,45-47`; `docs/V04_FINDINGS.md:69-76` | **WRONG (both halves)** | Yes — `sections_v05/11-corrections.tex` ¶C1, explicit retraction of all three statements |
| C2 | "A graduated in-policy forcing rule removes it entirely" | `docs/V04_FINDINGS.md:75`; `paper/sections/06-locked.tex:186-192` | **WRONG** — it removes the *statistic*, by misdeclaring | Yes — ¶C2 |
| C3 | The eight-level forced-endgame willingness ladder is a working selection mechanism | `paper/sections/A-dialect.tex:216-234`; `02-rules.tex:64-65`; `07-policy.tex:195-198` | **INOPERATIVE** | Yes — ¶C3 (allocator) and ¶C7 (ladder) |
| C4 | The fitting objective is a soft *minimum* over playstyles | `docs/FISHBOT_V04.md:128-130`; `docs/METHODOLOGY.md:50-52`; `paper/sections/08-fitting.tex:6-17`; `engine/src/tuner.hpp:3-7` | **WRONG** (weighted mean, ratio 1.655 at β=8) **+ panel omission** (no self-play opponent in any recorded generation of any round; min per-opponent win rate ever recorded 0.5786, and 0.7273 in the shipping round) | Partly — stated in `sections_v05/08-fitting.tex:153-211`, **not** in the corrections section |
| C5 | The 16-feature linear `V` is the fitted evaluation whose differences drive the ask rule | `paper/sections/07-policy.tex:125-141`; `06-locked.tex:96-135` | **WRONG in the part the ask rule uses** — the one feature carrying the fitted signal (`score differential`) is constant across candidate asks; 2 of 16 features algebraically identical (corr 0.9999959); 5 more cancel at all 5 call sites (perturbing ×1,000–7,000 changes **0 of 31,788** declaration decisions) | Yes — ¶C4 |
| C6 | The compiled `V04Config::vw` ≠ E14 fit is a "known gap" | `docs/FISHBOT_V04.md:150-152` | **BACKWARDS** — compiled vector beats E14 at 9/9 opponents at two seeds, 1–5 points, sign stable | Yes — ¶C4 |
| C7 | "past a second horizon its best candidate whatever the estimate, an undeclared half-suit scoring nothing" | `paper/sections/06-locked.tex:186-190`; `E11-termination.md:31-32` | **WRONG in this harness** — `game.hpp:271-287` awards residue by physical majority; on a wrong post-horizon declaration the declaring team holds a mean **4.08 of 6** cards | **NO — not in the v0.5 paper.** Register only |
| C8 | "Only the willingness bit crosses between seats" | `paper/sections/A-dialect.tex:218,231-232`; `02-rules.tex:37-38` | **WRONG as an information-safety guarantee** — `I(X;R)=H(R)`; ceiling log₂9 = 3.17 bits/candidate, measured **2.19 bits** (n = 336) | Yes — ¶C7 |
| C9 | Restarting the ladder lets early declarations sharpen later ones | `paper/sections/A-dialect.tex:232-234` | **INOPERATIVE** — essentially every v0.4 forced endgame has exactly one live half-suit (566/566 at seed 31; 118/120 at 1234567) | **NO — not in the v0.5 paper.** Register only |
| C10 | "\vfast's lowest win rate against any panel member is 75.07%"; "declared correctly 98.55%" | `paper/sections/11-discussion.tex:41-46`; `13-conclusion.tex:23-26` | **SCOPE** — panel excluded a mirror-strength opponent; mirror declaration accuracy is 89.56%, not 98.55% | Yes — ¶C9 |
| C11 | The observed cycling is a property of *an earlier* fitted configuration | `paper/sections/06-locked.tex:170-179`; `13-conclusion.tex:56-58`; `12-limitations.tex:66-68` | **OVERSTATED** | Partly — the v0.5 paper measures the shipped config (`\numPatientCap`), but does not name the v0.4 attribution as an error |
| C12 | `φ` is an independent "silence" channel in a two-parameter policy prior | `paper/sections/05-belief.tex:265-282` | **OVERSTATED** — deleting φ costs −0.30 pp, CI [−3.05, +2.40]; silence cells move ≤ 0.007 under every deception archetype; the statistic is whole-game, not time-local | Yes — ¶C5 |

**Two corrections the v0.5 paper adds that are not in the register:**
¶C6 (the locked-half-suit ask channel is priced, and is **not gateable** — provable team
ownership holds at only **0.34%** of mirror decisions) and ¶C8 (**waiting was free by
construction**: the waiting branch is evaluated by passing all-zero deltas to a value model in
which the public event count appears nowhere, so `V(wait at t) ≡ V(wait at t+1)` as an identity
of the implementation — any "patience dominates" argument in that model is self-fulfilling).
¶C8 is the deepest structural finding in the corpus and is directly actionable for v0.6.

## a.3 v0.4 claims that survived the diagnosis (C1 §S) — do not re-litigate

S1 Theorem 1 and the no-ownership-risk corollary. S2 Support monotonicity, and the paper's
explicit refusal to claim probability monotonicity. S3 Observation 2 (`06-locked.tex:53-66`) —
the correct statement, made in the paper before v0.5 measured it. S4 The exact posterior and its
three-way validation (`max abs diff 0.000e+00 over 227,062 checks`). S5 Lowest-seat declaration
arbitration — the cost of the information-safe choice is **+0.37 pp** over 30,000 games, and a
*clairvoyant* arbitrator is worth **+0.35 pp**, so the channel is saturated. S6 The cardless
player choosing the successor unilaterally — a ground-truth chooser is worth **−0.0007 ± 0.0024
sets/game** over 6,000 paired games. S7 The `θ` half of the policy prior is load-bearing —
deleting the prior is **worse by 4.60 points, CI [2.63, 6.58]**, against deceptive opponents.
S8 The paper's disclosure of its own stopping-rule approximations — correct on every point.
S9 Pre-gates are heuristics, not bounds (accurate self-report). S10 The round-5 base seed is
unrecoverable (accurate self-report). S11 The forcing horizon does **not** misfire on healthy long
games — 46/46 games reaching event 220 at seed 31 are genuinely deadlocked. S12 Three v0.5
hypotheses that failed (§c below).

## a.4 STILL UNRETRACTED AT HEAD — flag list

These are false or misleading, the v0.5 diagnosis showed them so, and the text at HEAD still says
them with no correction note in the file. v0.6 must not cite any of them, and the v0.6 paper
should carry the outstanding ones forward.

| # | Text at HEAD | file:line | Why it is false |
|---|---|---|---|
| U1 | "Theorem 1 … implies that such a half-suit is frozen — and, for the same reason, that no further information about its allocation can ever arrive." | `research/v04/results/E11-termination.md:3-7` | 68.5% of legal asks inside an own-owned half-suit strictly raise a teammate's exact P(correct allocation), mean +0.0957, max +0.6640, n = 2,880 over 3 seeds; 59.9% strictly lower exact marginal entropy (mean 0.325 nats, n = 5,760). Refuted by one line of production code (`belief.hpp:167` publishes `exclude(card, actor)` before the `if (e.success)` branch) |
| U2 | "the reason it is necessary — that Theorem 1 freezes information as well as ownership — appears not to have been stated." | `research/v04/results/E11-termination.md:45-47` | Same. Also offered as a contribution to the rules literature |
| U3 | Finding 6: "Because a half-suit frozen by the locked-half-suit theorem also admits no further information, two correctly patient policies deadlock permanently; … A graduated in-policy forcing rule removes it entirely." | `docs/V04_FINDINGS.md:71-76` | Both clauses false (C1, C2). The rule removes the action-cap statistic by misdeclaring: 58.6% of at/after-horizon declarations are wrong, 100% of those below self-assessed pAlloc 0.1 |
| U4 | Finding 7: "An adversary fitted specifically against FishBot v0.4 … **fails to reach 50% against it**" | `docs/V04_FINDINGS.md:77-80` | **Not in the v0.5 corrections register — new flag.** The v0.4 paper's own E10 table reports the response achieving **51.19% [49.67–52.72]** against v0.4-Fast (`paper/tables/lbr.tex`; caption at `paper/sections/10-results.tex:299-312`: "The win rate is what the response achieved, so lower is better for the frozen policy"). The response *did* reach 50%; the paper states this correctly at `10-results.tex:314-323` ("the interval includes $50\%$, so this search did not resolve an advantage in either direction"). The findings doc turns a null result into a win |
| U5 | Finding 4: "Declaration timing is a first-order decision … and modelling it as **optimal stopping is worth measurable win rate**" | `docs/V04_FINDINGS.md:66-68` | **Not in the register — new flag.** Contradicted by the same study: the `vdecl=0` ablation is **+0.12, CI [−1.23, +1.47]** (`docs/V04_RESULTS.md`), and `docs/FISHBOT_V04.md:104-106` itself says "The frozen-policy ablation does not resolve a benefit for that rule over a fixed threshold". Also note `paper/check.py` BANNED-lists the phrase "optimal stopping" for v0.4 |
| U6 | "Fitting maximises a soft minimum over the opponent panel rather than the mean, so the reported quantity is the worst case across playstyles" | `docs/METHODOLOGY.md:50-52` | C4. At β = 8 over the observed spread the objective is within 0.0027 of `mean − log 4 / β`; gradient weight ratio 1.655 |
| U7 | Known gap: "The 16 value-function coefficients compiled into `V04Config::vw` are **not** those in E14-valuefit.txt" (framed as a defect) | `docs/FISHBOT_V04.md:150-152` | C6 — the framing has the sign backwards. The compiled vector wins 9/9 opponents at two seeds. The register asks that this move from "Known gaps" to "Reproducibility notes" |
| U8 | "past a second horizon its best candidate whatever the estimate, **an undeclared half-suit scoring nothing**" | `paper/sections/06-locked.tex:186-190` | C7 — false in this harness (`game.hpp:271-287` adjudicates residue by physical majority; the declaring team holds a mean 4.08/6). The paper contradicts itself: `02-rules.tex:66-67` states the dialect correctly. **Not corrected in the v0.5 paper either** |
| U9 | "A consequence of restarting the ladder is that early, safer declarations resolve cards and thereby sharpen the allocations available for the later ones." | `paper/sections/A-dialect.tex:232-234` | C9 — inoperative; every v0.4 forced endgame has one live half-suit. **Not corrected in the v0.5 paper either** |
| U10 | "Only the willingness bit crosses between seats" | `paper/sections/A-dialect.tex:231-232` | C8 — measured 2.19 bits/candidate against a 3.17-bit ceiling |
| U11 | "the observed cycling is a property of specific fitted configurations in self-play, not of the game" | `paper/sections/13-conclusion.tex:56-58` | C11 — the shipped configuration cycles at 22.5%/23% |
| U12 | Stale source comment above `V05Agent::pressure` repeating v0.4's "no information about it can ever arrive" | `engine/src/v05.hpp` | C1. `docs/FISHBOT_V05.md:1020-1024` records it; the comment is still there |

**Policy for v0.6:** U1–U3 and U8–U9 are the ones a reader of the repo can still be misled by.
The v0.5 paper retracted U1–U3 *in the v0.5 paper*; the v0.4 artifacts themselves are unpatched by
design (`research/v05/PAPER_PLAN.md`: "corrections are stated as corrections, in their own
subsection, not silently patched"). U4, U5 and U8/U9 have no correction anywhere and are the new
work for the v0.6 corrections section.

## a.5 v0.3's three bounded claims (`paper/fishbot_v03.tex:44-49`)

1. "FishBot v0.3 is the strongest policy in the evaluated simulator population" — **superseded**
   (v0.4 beats it 75.07%; the C++ port is validated to within 1.9 pp of every published row).
2. "Its advantage is robust to team orientation and survives an untouched seed bank" — **STANDS**
   within its dialect. Note the v0.3 evaluation used two-orientation swaps + Wilson intervals,
   which the v0.4 methodology replaced with six-rotation duplicate blocks + deal-clustered
   bootstrap; v0.3's intervals are therefore **anti-conservative** relative to the current
   protocol (cluster intervals turned out *narrower* than Wilson only after the rotation bug was
   fixed — `docs/V04_FINDINGS.md:44-51`).
3. "Ask-history inference and public card-count conditioning are causally important within this
   simulator" — **STANDS and strengthened**: v0.4 re-derives ask history as a *logical certificate*
   (C5 ask legality) rather than a fitted weight, and the belief ablations are the largest effects
   in the corpus (+56.03 for capacity conditioning, +48.90 for certificates).

## a.6 v0.5's claims of record (`docs/V05_FINDINGS.md`, "What the study establishes")

All nine are stated conservatively and none has been challenged. The two that most constrain v0.6:
**(5)** "Removing the failure mode is worth almost nothing in win rate, and that is the result" —
win rate and soundness are dissociable, and the two highest-scoring ablation rows (56.6%, 56.4%)
are the ones that *keep* the pathology. **(7)** the fit created a new, measured deception exposure
(`priorTheta` 0.2638 → 0.4446, −2.2 on the feint, replicated), and a five-point sweep at two seeds
does not identify the parameter, so it **cannot be tuned away**.

v0.5's own named non-claims: no exploitability probe was run at all; `priorTheta` robustness is
measured against only three archetypes; termination is empirical, not structural; the pre-gate
audit does not run for v0.5 (`gateaudit` parsed only in the v0.4 branch of
`engine/src/factory.hpp`, so it reports a vacuous pass over zero opportunities); decisions D1
(conventions flag) and D2 (partner-aware regimes) are unimplemented.

---

# (b) MEASURED RESULTS OF RECORD

## b.0 Protocol drift — read before comparing across versions

| | v0.3 | v0.4 / v0.5 |
|---|---|---|
| Duplication | 2 team orientations | **6 rotations** (3 deal shifts × 2 team labels) |
| Interval | Wilson, over games | **Deal-clustered percentile bootstrap, 20,000 resamples** |
| Rules dialect | declarations only on own turn; adjudication after an action cap | declarations at any moment; chosen successor on a turn gift; willingness-only endgame; in-policy cashing. Residue at the 400-ask cap adjudicated by **physical majority** (`game.hpp:271-287`) |
| Fitting objective | grid/CEM on mean | soft-min over panel (β = 8 v0.4 — effectively a weighted mean; β = 25 v0.5, mirror in panel) |
| `--games=N` semantics | games | **deals**; games = N × rotations |

v0.3 numbers are only comparable through the **port validation** (E8, `--legacy`, seed 20260820,
1,000 deals × 2 rotations = 2,000 games): published vs C++ port agree to ≤ 1.9 pp on every row.

## b.1 FishBot v0.3 (`docs/V03_FINDINGS.md`, `paper/fishbot_v03.tex`)

Study size **126,600 games** (24,000 search+validation; 24,600 after adding the lockout
challenger; 54,000 held-out/ablation/matrix; 24,000 post-test stability sweep that did not change
the frozen config). Held-out rows: **1,000 games in each of two orientations = 2,000 games/row**,
95% Wilson. Individual seed values are not recorded in the findings doc — only "separate base
seeds per stage". This is a provenance gap inherited into any v0.3 comparison.

| Opponent | Win rate | 95% Wilson | Mean score | Ask acc. | Decl. acc. |
|---|---:|---:|---:|---:|---:|
| Turn-starvation lockout | 57.85% | 55.67–60.00 | 4.801 | 52.21% | 96.11% |
| Posterior detective | 57.20% | 55.02–59.35 | 4.767 | 53.60% | 95.01% |
| Sanitized v0.2 | 56.50% | 54.32–58.66 | 4.770 | 52.29% | 96.23% |
| Adaptive diversifier | 86.15% | 84.57–87.59 | 5.978 | 63.70% | 93.95% |
| Focused hunter | 95.15% | 94.12–96.01 | 6.747 | 55.30% | 91.56% |
| Misdirection artist | 99.20% | 98.70–99.51 | 7.438 | 57.33% | 90.24% |
| Random legal control | 100.00% | 99.81–100.00 | 8.400 | 52.38% | 93.91% |

Paired mechanism ablations (matched deals vs detective and lockout; positive = full beats ablated):
ask history **+38.75** [36.01, 41.49]; count conditioning **+5.20** [2.18, 8.22]; full auxiliary
bundle vs immediate-transfer-only **+3.20** [0.25, 6.15]; completion +1.15 [−0.52, 2.82];
continuation +0.35 [−1.17, 1.87]; reply risk +0.30 [−1.06, 1.66]; team control **−0.80**
[−3.29, 1.69]; restore entropy premium **−0.50** [−2.70, 1.70].

## b.2 FishBot v0.4 (`docs/V04_RESULTS.md`, `paper/numbers.tex`, `engine/experiments.sh`)

Every bank disjoint from fitting; configuration frozen before any of them ran.
Fitting banks: R1 20260821, R2/R3 770077 / 313131, R4 888111, **R5 606111 (ships)**,
validation selection 1357911. Round-5 base seed unrecoverable from any committed script.

| Exp | What | Sample | Seed | Headline |
|---|---|---|---|---|
| E1 | verify: rules, info safety, belief soundness | 600 games | internal | **23,594,580 audit checks, 0 violations**; legacy 10,910,844 |
| E2 | belief self-test | 40 games, 293,524 checks | internal | vs card DP **3.414e−15**; vs exact sampling 4.066e−2; Sinkhorn marginal error **mean 0.01705, max 0.4982** |
| **E3** | **held-out head-to-head (primary)** | **700 deals × 6 rot = 4,200 games/row** | **90210** | see table below |
| E4 | round-robin matrix | 200 deals × 6 rot (1,200/pair, 2,400/cell) | 515151 | Elo **+216** over v0.3 (lockout +5, detective +8) |
| E5 | paired ablations | 500 deals × 2 rot = 1,000 games; panel `v03,lockout,detective,v02` | 606060 | reference **77.50%**; see table below |
| E6 | calibration | 600 deals | 717171 | decl **n = 5,816, Brier 0.0155, ECE 0.0222**, observed 97.89%; ask n = 56,204, Brier 0.1297, ECE 0.0288; v0.3 decl ECE 0.1257 (94.59 predicted vs 82.02 observed) |
| E7 | rule dialects | 400 deals × 6 rot = 2,400 games | 828282 (legacy), 838383 (`--sets=8`), 848484 (`--no-out-of-turn`) | vs v0.3: legacy 64.46, 8-set 72.54, no-OOT 67.71 |
| E8 | v0.3 port validation | 1,000 deals × 2 rot = 2,000 games, `--legacy` | 20260820 | all rows within 1.9 pp of published |
| **E9** | **throughput** | 200 / 60 / 400 games | — | **Fast 162.8 g/s, block 11.9 g/s (14×), v0.3 6,954 g/s** |
| E10 | local best response (exploitability) | fit 515253; eval 600 deals × 6 rot = 3,600 games | 6543210 | **response achieves 51.19% [49.67–52.72] vs v0.4**; 77.31% [75.97–78.67] vs v0.3; 76.47% vs detective. A **lower bound within the searched class** |
| E11 | termination | 150 deals × 2 rot | 31 (mirror), 90210 (vs v0.3) | unforced mirror cap-hit **21.0% at cap 400 and unchanged at cap 1000** |
| E12 | belief exactness under an unfitted policy | 400 deals × 6 rot = 2,400 games | 959595 | fast **49.29%** [47.29–51.25] vs block **48.17%** [46.29–50.04] — overlapping |
| E13 | termination incidence across population | 300 deals × 6 rot | 464646 | mirror the only non-zero cell |
| E14 | value-function fit | 250 games | 31415 | **405,348 rows, in-sample R² 0.2909, RMSE 0.3047** (not the deployed vector) |
| E15 | brute-force oracle | 150 games | — | 15,544 states, 32,727,257 deals, 3,189,103 allocation checks, 0 bad |
| E16 | declaration pre-gate audit | 700 deals × 6 rot | 90210 | 10,102,149 opportunities, 24,114,786 rejections (41.20%), **1,017 false negatives (0.00422%), 1,016 actions changed (0.0101%)** |
| E17 | arbitration sensitivity | 700 deals × 6 rot | 90210 | max move **0.7 pp** across arbitration orders |

**E3 held-out head-to-head, seed 90210, 700 deals × 6 rotations = 4,200 games per row:**

| Opponent | Win rate | 95% cluster CI | Mean sets | Ask acc. | Decl. acc. | Decl./game | OOT/game |
|---|---:|---:|---:|---:|---:|---:|---:|
| v0.3 | **75.07%** (3,153 wins) | 73.71–76.40 | 5.457 | 55.8% | 98.55% (v0.3: 82.59%) | 4.80 | 3.43 |
| lockout | 78.12% | 76.9–79.4 | 5.542 | 48.0% | 98.1% | 5.16 | 3.59 |
| detective | 75.74% | 74.5–77.0 | 5.440 | 52.8% | 98.4% | 5.12 | 3.57 |
| v0.2 | 84.24% | 83.1–85.3 | 5.857 | 54.4% | 98.9% | 5.17 | 3.63 |
| diversifier | 92.14% | 91.3–93.0 | 6.442 | 64.0% | 97.4% | 6.14 | 4.16 |
| hunter | 97.64% | 97.2–98.1 | 7.000 | 55.7% | 97.5% | 5.09 | 3.49 |
| bluffer | 99.95% | 99.9–100.0 | 8.161 | 55.1% | 98.2% | 3.80 | 2.66 |
| random | 100.00% | 100.0–100.0 | 8.579 | 54.3% | 96.7% | 6.99 | 4.64 |

Reported "worst case 75.07%" — **SCOPE-corrected (C10)**: this panel has no mirror-strength member;
against a copy of itself v0.4 is at 50.00% and declares correctly 89.56% (564/5,400 wrong).

**E5 paired ablations, 1,000 games, seed 606060, reference 77.50% (positive = full beats ablated):**

| Change | Abs. win rate | Full − ablated | 95% paired CI |
|---|---:|---:|---:|
| belief=indep (drop C4 capacity + C5) | 21.48% | **+56.03** | +54.27–+57.75 |
| belief=sinkhorn | 24.73% | **+52.78** | +50.98–+54.55 |
| belief=exact (card DP, **no C5 certificates**) | 28.60% | **+48.90** | +47.00–+50.78 |
| belief=block (the *exact reference* engine) | 71.30% | **+6.20** | +4.35–+8.05 |
| ptheta=0,pphi=0 (delete policy prior) | 73.62% | +3.88 | +2.08–+5.73 |
| w5=0 (lock completion) | 74.30% | +3.20 | +1.55–+4.85 |
| w0=0 (hit probability) | 75.95% | +1.55 | −0.25–+3.35 |
| topk=0 (two-ply lookahead) | 76.28% | +1.23 | −0.53–+3.05 |
| decl=0.99 | 77.30% | +0.20 | −0.20–+0.60 |
| gmap=1 | 77.33% | +0.18 | −0.12–+0.47 |
| vdecl=0 (fixed threshold, not value stopping) | 77.38% | +0.12 | −1.23–+1.47 |
| w9=0,w19=0 (information-leak features) | 77.38% | +0.12 | −1.38–+1.65 |
| decl=0.80 | 77.48% | +0.03 | −0.18–+0.22 |
| patient=0 | 77.50% | +0.00 | +0.00–+0.00 |
| value=0 (delete expectimax) | 77.83% | **−0.33** | −1.88–+1.23 |
| w8=0 (reply threat) | 77.85% | **−0.35** | −1.98–+1.27 |
| w18=0 (runway) | 77.85% | **−0.35** | −1.65–+0.97 |

Naming trap the paper flags (`sections/D-ablations.tex:120-138`): `belief=exact` is the **card-level
capacity DP without C5 certificates**, `belief=sinkhorn` is plain scaling, `belief=indep` drops
capacity too. **The exact reference engine is `belief=block` alone.**

## b.3 FishBot v0.5 (`docs/FISHBOT_V05.md` §8, `research/v05/RESULTS-SUMMARY.md`)

Fitting: single round that ships, `./fish tune --base=v05 --panel=v04,v03,lockout,detective,diversifier,hunter --full --games=200 --pop=24 --elite=6 --gens=40 --beta=25 --seed=505101`. Round 0 abandoned at generation 19, reason unrecorded. Shipped vector is the CEM
**distribution mean** (common-seed re-eval 0.5272) not the best generation (0.4798).
In-panel best generation 57.0 / 75.2 / 80.5 / 79.0 / 94.8 / 98.5 → **held-out ≈ 51%**. That
in-panel-to-held-out gap (57 → 51) is the regression to quote.

| Exp | Sample | Seed | Headline |
|---|---|---|---|
| **E2 pathology (the commit gate)** | 300 deals × 2 rot = 600 games/arm | 31 (mirrors), 90210 (cross) | see KPI table below |
| E3 head-to-head | 300 deals × 6 rot = 1,800 games per bank, **5 banks** | 90210, 31337, 515151, 777001, 424242 | 50.11 / 49.50 / 52.33 / 50.89 / 51.11 → **mean 50.79%**, one bank below 50, all five intervals contain 50 |
| E4 per-style profile, **both arms** | 300 deals × 6 rot = 1,800 games/cell | 515253 | see table below |
| E5 mechanism ablations | 250 deals × 6 rot = 1,500 games/arm, all vs shipped v0.4 | 606060 | see table below |
| E6 calibration | 400 games | 717171 | ask n = 36,467, Brier 0.1219, ECE 0.0223; **decl n = 3,609, Brier 0.0150, ECE 0.0154** |
| E7 rule dialects | 250 deals × 6 rot = 1,500 games | 828282 | default 51.60, `--no-out-of-turn` 52.47, `--no-cardless-declare` 51.87, `--legacy` 52.13 |
| **E8 forced endgame at volume** | **4,000 deals × 6 rot = 24,000 games/arm** | 909090 | v0.4 0.0307 forced decl/game at **0.14% correct**; v0.5 0.0048/game at **24.35% correct**; measured feasible ceiling ≈ **40.6%** |
| E9 throughput | 300 games | — | **286 games/s** |
| E10 deception panel | 400 deals × 6 rot = 2,400 games/cell, **2 banks** | 31415926, 8675309 | see table below |
| E1 verify | 600 games | internal | 23,594,580 checks, 0 violations, determinism PASS, `pAlloc` vs exhaustive enumeration **0.000e+00** |

**E2 KPI table — this is the v0.5 result:**

| KPI | v0.4 mirror | v0.5 mirror |
|---|---:|---:|
| provably dead asks | 39.04% | **0%** |
| dead runs (mean len, longest) | 2,610 (12.05, **286**) | **0** |
| games with a dead run ≥ 6 | 34.33% | **0%** |
| exact repeat (actor, card, target) asks | 40.03% | **2.63%** |
| declarations wrong | 10.44% | **2.07%** |
| declarations at/after event 220 | 768, 58.59% wrong | **0** |
| forced-endgame declarations | 28, **100% wrong** | 2, **0% wrong** |
| starved turns | 0.20% | **0** |
| events/game (median, p90, p99) | 143.60 (106, 312, 321) | **96.56 (96, 112, 125)** |
| ask hit rate | 34.25% | **55.47%** |
| action-limit games | 0% | 0% |

Against v0.3 on the same instrument, v0.4's dead-ask rate is **2.82%**, longest run 5, no game past
event 220 — a factor of ~14. This is why the published evaluation could not see it.

**E4 per-style, seed 515253, 1,800 games/cell:**

| Opponent | v0.5 | v0.4 | Δ |
|---|---:|---:|---:|
| v0.4 (mirror-strength) | 51.11% | 50.00% | +1.11 |
| v0.3 | 72.33% | 73.33% | −1.00 |
| v0.2 | 81.28% | 83.06% | −1.78 |
| lockout | 79.56% | 77.94% | +1.61 |
| detective | 76.78% | 77.28% | −0.50 |
| diversifier | 93.78% | 92.89% | +0.89 |
| hunter | 97.72% | 97.67% | +0.06 |
| bluffer | 99.89% | 99.94% | −0.06 |
| random | 100.00% | 100.00% | 0.00 |
| **worst case** | **51.11%** | **50.00%** | |
| mean | 83.60% | 83.57% | |
| **minimax regret** | 1.78 (on v0.2) | **1.61** (on lockout) | v0.4 better |

**E10 deception panel, 2,400 games/cell, two banks:**

| Archetype | v0.5 (31415926) | v0.4 (31415926) | v0.5 (8675309) | v0.4 (8675309) | mean Δ |
|---|---:|---:|---:|---:|---:|
| silent | 80.42% | 79.96% | 83.17% | 79.00% | **+2.3** (size does not replicate) |
| feint | 50.96% | 54.13% | 52.08% | 53.29% | **−2.2** (replicates in sign **and** size) |
| withholder | 73.63% | 66.25% | 71.42% | 64.46% | **+7.2** (replicates in sign **and** size) |

Across the full twelve-style set, **v0.5's worst case is the feint at 50.96%**, marginally below
its mirror worst case; v0.4's worst case is its own mirror at 50.00%.

**E5 v0.5 mechanism ablations, 1,500 games/arm, seed 606060, all against shipped v0.4:**

| Configuration | Win rate | events/game | decl. acc. |
|---|---:|---:|---:|
| control `m1=0,m2=0,stage2=1` (v0.5-as-v0.4) | 50.87% | 141.2 | 89.02% |
| M1 alone | 49.53% | 99.7 | 98.76% |
| M2 alone | 52.60% | 141.4 | 91.20% |
| **M8 alone** | **56.60%** | 141.7 | 96.60% |
| M1+M2 | 49.07% | 99.7 | 98.31% |
| M1+M8 | 49.53% | 99.7 | 98.76% |
| **M2+M8** | **56.40%** | 141.7 | 96.16% |
| **shipped `v05`** | **49.07%** | **99.7** | **98.31%** |
| `v05:m1p=1` | 47.73% | 99.2 | 97.50% |
| `v05:norepeat=1` | 42.93% | 99.6 | 98.37% |

**The two highest rows keep the pathology** (`m1=0`: 44.83% dead asks, longest run 373, 26% of
games with a run ≥ 6, **14% of games terminated by the 400-ask action limit**). They are rejected
on the KPI table, not on win rate. This is the single most important methodological fact in the
corpus for v0.6: *a win rate collected against v0.4 at one seed cannot see a mirror failure mode.*

**Was the refit worth anything? No.** Running the v0.5 mechanisms on v0.4's *frozen* 34-coordinate
vector already scores 50.22% [48.40–52.03] over 6,000 games at two banks and already takes the
mirror dead-ask rate to 0.02%, longest run to 1, post-horizon declarations to 0, declaration error
to 1.89% (`research/v05/runs/v04vector-in-v05.txt`). **The repair is mechanical, not parametric.**

---

# (c) TRIED AND REJECTED — do not rebuild

Exhaustive across all three studies. Every row has a measured cost and a source.

## c.1 Rejected in the v0.3 study

| # | Mechanism | Measured cost | Source |
|---|---|---|---|
| R1 | **Direct information/entropy premium** in the ask utility | selected coefficient is **zero**; restoring it **−0.50 pts** [−2.70, +1.70] | `docs/V03_FINDINGS.md`, `docs/FISHBOT_V03.md` |
| R2 | **Deterministic psychological response rules** (hardwired same-suit emotional response, diversion rule) | **−0.8 / −0.5 / −0.5 pts** vs detective / lockout / bluffer on matched deals | `docs/V03_FINDINGS.md` "Psychological response rules" |
| R3 | Post-test local stability sweep of the frozen config | 24,000 games; **no change justified** | `docs/V03_FINDINGS.md` |

## c.2 Measured-not-to-help in the v0.4 frozen-policy ablations (1,000 games, seed 606060)

These are not "built and worse" so much as "built and not separable from zero, or negative". v0.6
should not assume any of them is load-bearing.

| # | Mechanism | Effect (full − ablated) |
|---|---|---|
| R4 | one-ply/two-ply expectimax over `V` (`value=0`) | **−0.33** [−1.88, +1.23] — deleting it is *not worse* |
| R5 | reply-threat feature `w8` | **−0.35** [−1.98, +1.27] |
| R6 | runway feature `w18` | **−0.35** [−1.65, +0.97] |
| R7 | value-based stopping vs a fixed threshold (`vdecl=0`) | **+0.12** [−1.23, +1.47] — no resolved benefit |
| R8 | information-leak features `w9`, `w19` | **+0.12** [−1.38, +1.65] |
| R9 | two-ply top-K re-scoring (`topk=0`) | **+1.23** [−0.53, +3.05] — not separated from zero |
| R10 | declaration-threshold knobs (`decl=0.80/0.99`, `gmap=1`, `patient=0`) | +0.03 / +0.20 / +0.18 / **+0.00** — inert |
| R11 | ask-side use of `V` measured **in the mirror** (E5 never was) | deleting the entire ask-side expectimax: **+0.006, CI [−0.016, +0.029]** — nothing. Reverting declarations to fixed thresholds costs +0.025 [+0.004, +0.047] | `C1 §C5(e)` |

## c.3 Rejected in the v0.4 study

| # | Mechanism | Measured cost | Source |
|---|---|---|---|
| R12 | **Exact reference belief (`belief=block`) in the deployed inner loop** | **−6.20 pts** under Fast-fitted weights [+4.35–+8.05 for Fast] and **14× slower** (162.8 → 11.9 games/s). Confounded: under a deliberately unfitted greedy policy the gap is 49.29 vs 48.17 with overlapping intervals (E12). **A matched-budget refit was never run** and remains the open experiment | `docs/V04_RESULTS.md`, `sections/11-discussion.tex:57-95`, `sections/D-ablations.tex:213-252` |
| R13 | **Dead-ask forcing rule** ("force a claim as soon as no productive ask remains", best ask `p < 0.02` ⇒ force) | mirror 44.3%, cap hits 16.7%, and **−28.3 pts against v0.3** | `research/v04/results/E11-termination.md:24-28` |
| R14 | **Adopting the E14-fitted value vector** in place of the compiled `V04Config::vw` | **−1 to −5 pts, 9/9 opponents, two independent seeds, sign never flips**. ~84% of the gap is two coefficients (`v2`,`v6`) that set the expectimax scale relative to the fitted `linearWeight` | `C1 §C6`, `P7-verify-valuefn.md` |

## c.4 Rejected in the v0.5 diagnosis, before anything was built (`DESIGN.md` §0.3, C1 §S12)

| # | Mechanism | Measured cost | Source |
|---|---|---|---|
| R15 | **Time-varying holding cost in `value()`** (`vWait -= c·nEvents/220`, c ∈ [0.5, 20]) | a mean-shifter: events/game 146 → 121 and dead-run frequency 32% → 18.5%, but **p90 313 → 311 and longest run 289 → 289**; declaration error 10.9% → 13.4%; **−3.9 pts head-to-head**. Also: a constant holding cost already exists (`declareMargin = −0.0342`), and in deadlocks the `urgent` flag bypasses `declareByValue` (binds in only 9.4% of late blocked opportunities) | `DESIGN.md` §0.3, `Plit-verify-holding-cost-claim.md` |
| R16 | **Deleting the policy prior** (`ptheta=0,pphi=0`) for deception robustness | **−4.60 pts, CI [2.63, 6.58]** against the deceptive panel. The exposure is over-weighting, not weighting | `P3-deception.md` §4, `P3-verify-deception.md` |
| R17 | **Turn-transfer willingness ladder** | a genuine multi-candidate decision arises **0.148 times per game**, governs 0.92% of asks; a **ground-truth** chooser is worth **−0.0007 ± 0.0024 sets/game** over 6,000 paired games (≈ 0.05 cards/game). Budget nothing | `P8-coordination.md` §1, `P8-verify-turn-transfer.md` |
| R18 | **Confidence-ranked declaration arbitration** | **+0.37 pp** pooled over 30,000 games (range +0.28 to +0.45 over five seeds); a **clairvoyant** arbitrator is worth +0.35 pp, so the channel is saturated | `P6-verify-arbitration-cost.md` |
| R19 | **Reshaping the forced-endgame willingness ladder** | four shapes including a 0.10 bottom rung and **no rungs at all** give **bit-identical** output while the allocator is broken (statistic exactly 0 in 210/210 pairs). With the feasible allocator *and* a bottom rung < 1/3 the ladder fires in 100% of sweeps, but the statistic pins at exactly 0.3333 in all 60 surveys and forced-declaration error moves 100% → 90% on n = 20 (not significant) | `P8-coordination.md` §2.2, §2.5 |
| R20 | **Deleting `priorPhi`** | free but worthless: **−0.30 pp, CI [−3.05, +2.40]**, verifier −0.83 [−3.33, +1.67]; a 15-fold sweep is flat | `C1 §C12` |

## c.5 Built, measured at the frozen v0.5 configuration, and shipped OFF

| # | Mechanism | Measured cost | Switch |
|---|---|---|---|
| R21 | **M1p — ownership features scaled by `p`** (multiply `f[3]`,`f[5]`,`f[7]`,`f[15]` by hit probability) | **−1.33 pts** (49.07% → 47.73%, E5 seed 606060). Once M1 removes the `p = 0` case the incentive has nothing to act on, and rescaling 4 of 20 features distorts the rest of the fitted linear score | `v05:m1p=1`, `V05Config::ownershipByP = false` |
| R22 | **(card, target) repetition guard** | **−6.13 pts** (49.07% → 42.93%) — the largest single-switch effect in the corpus. Cards *move*: a repeat after a public transfer can be the single best ask on the board, and M1 already removes the case the guard was built for | `v05:norepeat=1`, `V05Config::repeatGuard = false` |
| R23 | **Deleting stage 2 without M1** (`m1=0,m2=0,stage2=0`) | scores **higher** (56.60%) and is the pathological policy: 44.83% dead asks, longest run **373**, 26% of games with a run ≥ 6, 76 post-horizon declarations, **14% of games killed by the 400-ask action limit** | rejected on KPI |
| R24 | **M2+M8 without M1** | 56.40%, identical mirror pathology to R23 (M2 changes what is declared, not what is asked) | rejected on KPI |

## c.6 Structural negative results (design-space eliminations)

| # | Result | Source |
|---|---|---|
| R25 | **Determinized / perfect-information Monte Carlo (PIMC) is degenerate for Fish.** A clairvoyant player never fails an ask and never mis-declares, so a double-dummy evaluator collapses toward `argmax P(target holds card)`. Search must be over information sets with a belief-limited evaluator | `docs/METHODOLOGY.md`, `research/v04/lit/pimc.md`, `sections/03-related.tex:39-60` |
| R26 | **Gating on provable team ownership will not work.** The condition holds at **0.34%** of mirror decisions and 2.0% vs v0.3, and 31.3% of those are forced. At the 15 dumped deadlock states, the observer's exact `P(my team owns this half-suit)` has mean 0.068, max 0.342, and **0 of 36** exceed 0.999. The channel must be **priced against `pTeam`, not gated** | `P5-human-strategy.md` §0.2, `P1-verify-e11-information.md` §6.1 |
| R27 | **v0.5 brief hypothesis that the forcing horizon misfires on healthy long games — rejected.** 46/46 games reaching event 220 at seed 31 are genuinely deadlocked; raising the dead-run threshold to 80 leaves the split unchanged | `P4-policy-review.md` §D1, C1 §S11 |
| R28 | **Four P4 code hypotheses that did not hold**: `pTeam = max(cheap, pAlloc)` is a literal no-op; `threatOf`'s `0.7+0.3·activity` is 0.7 in 87.2% of evaluations; `exposureOf` saturates in 1.61% of calls; the `unresolvedCount<=8` bypass disables one gate of three | `P4-policy-review.md` §"Checked and correct" |
| R29 | **The literature's holding-cost recommendation: code facts confirmed, causal attribution refuted** | `Plit-verify-holding-cost-claim.md` |

---

# (d) OPEN / UNBUILT

## d.1 The v0.5 mechanism backlog (`research/v05/DESIGN.md` §1; `docs/FISHBOT_V05.md` §11)

| ID | Mechanism | Design note | State | Why it matters |
|---|---|---|---|---|
| **M3** | **Net-information term in the ask score**: `+ν·ΔI_team − λ·ΔI_opp` from the grounded (`β̃`) and policy-weighted (`β`) filters, replacing `f[14]`'s sign | `DESIGN.md` §M3; wiretap/Farrell–Gibbons framing in `research/v04/lit/signalling.md` §2.4/§2.6 | **unbuilt**; **requires M4** | The channel is provably free at only 0.54% of decisions → must be **priced, not gated** (R26). This is the strategic upgrade the whole corpus points at |
| **M4** | **Knowledge model of the other five seats** (public deduction state + posterior over each seat's hand) | `DESIGN.md` §M4; `P5-human-strategy.md` §0 ("a fourth, structural") | **unbuilt**; patch `research/v05/patches/M4-M5.patch` exists, **unmeasured** | v0.4/v0.5 build `Knowledge` only for themselves (`v04.hpp:507`, `v04.hpp:524`, both `Knowledge kh = k;`). Prerequisite for signalling, blackballing, best-informed-declarer selection, baiting. "Nearly free — the transcript is public" |
| **M5** | **Target-dimension selection** — delete `(void)target;`, score the target on lockout value and void progress | `DESIGN.md` §M5; `P5-human-strategy.md` §0.1 | **unbuilt**; same patch | **The largest unused channel measured anywhere.** At **46.6%** of 154,318 mirror ask decisions ≥ 2 opponents are hard-indistinguishable holders of the card actually asked for; mean **0.639 free bits/ask** at a hit-probability spread of only 0.081. vs v0.3: **66.2%**, 0.919 bits. ~41 bits/team/game unused. `askExpectedValue` opens `(void)target;` (`v04.hpp:435`) |
| **M6** | **Partner-aware stochastic action selection** (decision D2): softmax over `A* = {a : μ* − μ_a < min(σ*, σ_a)}`, temperature *and* convention flag keyed on bot-teammate vs human/unknown-teammate regime | `DESIGN.md` §M6; `research/v05/BRIEF.md` D2 | **unbuilt** | Owner decision of record: *report both regimes separately, never headline the self-play configuration.* Also the OBL/piKL knob. A deterministic argmax sits at the deterministic leakage bound whatever its weights |
| **M7** | **Online per-seat opponent model** replacing `priorTheta`/`priorPhi` — type library (aggressive/silent/v0.3-like/deceptive), updated from public asks **and a time-local silence statistic**, shaped as a **data-biased response** (Johanson & Bowling, AISTATS 2009) | `DESIGN.md` §M7; full spec `research/v05/results/M7-design.md` (425 lines) | **unbuilt**; patch `research/v05/patches/M7.patch` exists, **unmeasured** | **Highest priority named by the v0.5 study.** The fitted `priorTheta = 0.4446` *is* the feint exposure (−2.2 pts, replicated), and a 5-point sweep at two seeds does not identify it → cannot be tuned away. Also: `missCount[p][S]` is maintained at `belief.hpp:181` and **read by nothing** |
| **M9** | **Value function rebuilt or retired**: (a) collect rows at declaration points too, drop the 7 degenerate features, refit **inside** the CEM so the freeze step covers it; or (b) retire it and score declarations on `pAlloc` against a fitted threshold | `DESIGN.md` §M9; `P7-valuefn.md` | **unbuilt** | Capacity exists: a depth-3 GBT on identical rows improves held-out R² by ~two thirds relative (re-measure across fold seeds first). Currently **no row is ever collected at a declaration decision point** (`game.hpp:310-323`) yet `declareByValue` (`v04.hpp:653-671`) evaluates `V` at exactly those points |
| **M10** | **Fitting/evaluation harness**: a true worst-case or minimax-regret objective reporting `min` *and* regret; the three pathology KPIs wired into the commit gate | `DESIGN.md` §M10 | **partially built** — mirror is in the panel and β = 25, but the objective is still a soft-min and the KPI gate is manual | β = 25 over v0.5's Δ = 0.415 gives ratio 32,048; neither change is sufficient alone (β = 25 over v0.4's spread → 4.83; β = 10 over v0.5's spread → 63.43) |
| **D1** | **Conventions behind `--conventions=off|on`**, ship off as headline, publish the with/without delta as a result | `research/v05/BRIEF.md` D1 | **unbuilt** | Owner decision of record |
| **D2** | **Two partner regimes reported separately** | `research/v05/BRIEF.md` D2 | **unbuilt** | Owner decision of record; also the auto-memory note `fishbot-partner-aware-policy` |

## d.2 Smaller open items with a measured prize

| Item | Prize | Source |
|---|---|---|
| **Best-positioned teammate declares in the forced endgame** (currently lowest live seat) | **+6.0 points of forced-endgame accuracy on top of M2** | `P2-forced-endgame.md` §4; `docs/FISHBOT_V05.md` §12 |
| **Close the forced-endgame gap**: 24.35% achieved vs **≈ 40.6%** measured feasible ceiling | ~16 points of forced-endgame accuracy; it is the *ranking rule and declarer choice*, not the feasibility test | `docs/FISHBOT_V05.md` §8.2, §12 |
| **Exact MAP over the feasible set** (`BlockDP::bestTeamAllocation`) instead of the product-of-marginals surrogate `feasibleAllocation` uses | unknown — "the size of that disagreement has not been measured" | `docs/FISHBOT_V05.md` §12 |
| **Re-shape `Rules::forcedTh`** to ~9 evenly spaced rungs over [0,1] (every real rung currently ≥ 0.5 while contested confidences cluster far below) — **only meaningful now that M2 makes the statistic non-zero** | small; see R19 for the ceiling | `DESIGN.md` §M2, `docs/FISHBOT_V05.md` §12 |
| **Exploitability probe against v0.5/v0.6** (`fish tune --panel=v05` + held-out re-match) | none directly; it is the **single largest hole in the v0.5 evaluation** | `docs/V05_FINDINGS.md`, `sections_v05/12-limitations.tex` L2 |
| **`gateaudit` for v0.5+** — one line of spec parsing missing (`gateaudit` parsed only in the v0.4 branch of `engine/src/factory.hpp`) | v0.4's audit found 1,017 false negatives / 24.1M rejections; the v0.5 number is unknown | `docs/FISHBOT_V05.md` §12 |
| **Matched-budget belief refit** (CEM run twice from the same seeds, once per inference path) — the experiment that would settle R12 | resolves whether exact inference is worth anything after retraining; at 11.9 g/s it is expensive but feasible | `sections/11-discussion.tex:93-95`, `sections/12-limitations.tex` |
| **P4 "free wins"** — reorder `refresh()` before `computeAggregates` in `proposeDeclaration` (posterior stale by mean 2.0 events, max 177); fix `expectedRun` double-count; delete `pTeam = max(cheap, pAlloc)`; delete `f17`/`f11`/`f13` | none change the win rate today; all remove confusion for whatever replaces the ask rule | `P4-policy-review.md` §"What v0.5 should take from this" |
| **Calibrated `pAlloc` for the stopping rule** — the value rule fires down to `pAlloc = 0.46`, and the 0.7–0.9 region is 10–13% wrong and non-monotone; `cheap` overestimates exact team-ownership probability in 96% of samples | correctness of the stopping rule | `P4-policy-review.md` §D5 |
| **`f[11]` sign asymmetry** — emptying *an* opponent is good, emptying the **last live** one walks into a forced endgame | avoids the state v0.4 lost 100% of | `DESIGN.md` §0.4 F |
| **Tuner should write a header record** (β, population, elite, deals/cell, panel) into the fit jsonl | closes circular provenance: `build_tables_v05.py` currently sources `\numFitBeta` etc. from **prose** (`docs/FISHBOT_V05.md` §9) | `docs/V05_FINDINGS.md` "Provenance gaps" |

## d.3 Two engine defects that block any exact-belief v0.6 — build-order critical

| Defect | Evidence | Consequence |
|---|---|---|
| **`BlockDP` instances alias.** `BlockDP::build` parks its tables in a `thread_local` buffer pool (`blockdp.hpp:83-97`, `175-176`), so a second agent's `build()` silently repoints the first agent's tables | measured **`checks 294, mismatches 285`** (`P2-forced-endgame.md` §6) | Harmless under the default `Fast` belief; **fatal for any v0.6 that adopts exact block beliefs**, including M3's information terms and the exact feasible MAP |
| **Duplicate certificates accumulate.** `Knowledge::onEvent` appends a fresh `Disjunction` on every repeated ask without checking for an identical one (`belief.hpp:168-171`) | a deadlocked observer's store grows 6 → 26 → 46 carrying no information | Makes the block DP progressively more expensive; a correctness-neutral perf bug that bites exactly where M3 needs the block DP |

---

# (e) PAPER STRUCTURE — the v0.4 template

The owner wants the v0.6 paper written like the **v0.4** one. Precise inventory follows.

## e.1 Main file

`paper/fishbot_v04.tex`, **110 lines**. `\documentclass[11pt]{article}`.
Packages, in load order (the order is load-bearing and commented in-source at lines 15-18):
`geometry[margin=0.9in]`, `fontenc[T1]`, `lmodern`, `microtype`, `amsmath,amssymb,amsthm`,
`booktabs`, `array`, `longtable`, `algorithm`, `algpseudocode`, `xcolor[dvipsnames]`, `graphicx`,
**`hyperref` (before `tikz`)** with `\DeclareUrlCommand\filepath{\urlstyle{tt}}` to preserve url's
verbatim `\path` before TikZ takes it back, `tikz` + `\usetikzlibrary{arrows.meta,positioning,calc}`,
`enumitem`, `fancyhdr`, `caption[font=small,labelfont=bf,skip=6pt]`.

House colours: `fishgreen #0B5D46`, `fishlight #EAF4EF`, `fishgray #4B5954`;
`\hypersetup{colorlinks=true, linkcolor=fishgreen, citecolor=fishgreen, urlcolor=fishgreen}`.
`fancyhdr`: `\lhead{\small FishBot v0.4}`, `\rhead{\small <one-line subtitle>}`, `\cfoot{\thepage}`.

Theorem environments (`fishbot_v04.tex:41-46`): `theorem` numbered; `proposition`, `corollary`
sharing its counter; `definition` and `observation` under `\theoremstyle{definition}`, same counter.

Policy-name macros are `\DeclareRobustCommand` (they appear in captions, section titles and PDF
bookmarks) with `\texorpdfstring` fallbacks: `\vfast`, `\vblock`, `\vthree`, `\vtwo`
(v0.5 adds `\vfive`, `\vfour`).

Title block: two-line bold title + `\large` subtitle; `\author{Dylan Nguyen\\\small FishLab
Research Project}`; `\date{}` carries "Technical report --- \today", the repository URL through
`\filepath`, **the exact commit each experiment group ran at** ("E1--E14 … at commit `bb3bc8a`;
E15--E17 are newer and are regenerated by the commands in Appendix~\ref{app:repro}"), and a pointer
to `research/v04/results/MANIFEST.json`.

`\input{numbers}` sits **before** `\begin{document}`.

## e.2 Section structure — exact, in order

Body (13 sections, `paper/sections/`):

| File | `\section{...}` | Subsections |
|---|---|---|
| `abstract.tex` | (abstract) | — |
| `01-introduction.tex` | Introduction | — |
| `02-rules.tex` | The game and the rules dialect studied | The dialect studied; What the v0.3 simulator modelled and what it did not |
| `03-related.tex` | Related work | Fish and Literature; Determinization, and why it is not the foundation used here; Policy-aware inference over deals; Counting deals under constraints; Team games and common information |
| `04-engine.tex` | The simulator: implementation, information safety, verification | Implementation; Information safety; Verification suite |
| `05-belief.tex` | The observer-conditioned deal posterior | The hidden state is the initial deal; Notation; The constraint system; Counting: a capacity-vector dynamic program; Ask legality by half-suit block enumeration; A fast approximate posterior; The residual: policy-aware inference |
| `06-locked.tex` | Locked half-suits, declaration timing, and termination | A half-suit held by one team cannot be taken back; A value-based declaration rule; Termination |
| `07-policy.tex` | `\vfast{}` | Overview; Choosing an ask; Two-ply lookahead with approximate branch beliefs; The value function; Declaring |
| `08-fitting.tex` | Fitting | A population-robust objective; Optimiser; Schedule and selection |
| `09-protocol.tex` | Evaluation protocol | Six-rotation duplicate blocks; Clustered interval construction; Seed separation and what was held out; Metrics |
| `10-results.tex` | Results | Engine and belief validation; Primary result; Calibration; Which mechanisms matter: frozen-policy ablations; Robustness: rules dialects and arbitration; Local response probe; Throughput |
| `11-discussion.tex` | Discussion | Whether one policy can be best across playstyles; Interpreting the belief-substitution result; Implications for human play |
| `12-limitations.tex` | Threats to validity | — |
| `13-conclusion.tex` | Conclusion | — |
| `bibliography.tex` | (thebibliography) | — |

Appendices (`\appendix`, 9 sections, A–I):

| File | `\section{...}` | # subsections |
|---|---|---|
| `A-dialect.tex` | The rules dialect in full | 11 |
| `B-inference.tex` | Inference: derivations and implementation notes | 9 |
| `C-parameters.tex` | Configuration and fitted parameters | 7 |
| `D-ablations.tex` | Extended frozen-policy ablations | 5 |
| `E-dialects.tex` | Population, rules-dialect and arbitration results | 5 |
| `F-calibration.tex` | Calibration detail | 4 |
| `G-reproducibility.tex` | Reproducibility and artifact manifest | 4 |
| `H-human-play.tex` | Extended implications for human play | 8 |
| `I-related-extended.tex` | Extended related work | 9 |

## e.3 Length

- **70 pages** (PDF `/Count`, `output/pdf/fishbot_v04.pdf`, 435 KB; the retained build log records
  71 xdv pages and predates the final rebuild).
- **37,358 words of LaTeX source** across `paper/sections/*.tex`; 4,178 lines of section source
  plus a 110-line main file; the flattened Overleaf file `fishbot_v04_standalone.tex` is **5,496
  lines**.
- For scale: v0.3 is **12 pages**, a single 409-line `.tex`. v0.5 is **43 pages**, 26,631 words,
  10 sections, **no appendices** (slots 06, 07, 09 deliberately reserved and unwritten —
  `fishbot_v05.tex:95-97`).

## e.4 Table and figure inventory

**21 labelled tables.** 15 are `\input{tables/<name>}` and are **generated** by
`engine/build_tables.py` into `paper/tables/`; 6 are hand-written inline.

| Generated (`paper/tables/`) | Label | Where used |
|---|---|---|
| `calibration.tex` | `tab:calib` | 10-results |
| `ablations.tex` | `tab:ablations` | 10-results |
| `lbr.tex` | `tab:lbr` | 10-results |
| `throughput.tex` | `tab:throughput` | 10-results |
| `oracle.tex` | `tab:oracle` | B-inference |
| `params.tex` | `tab:params` | C-parameters |
| `gateaudit.tex` | `tab:gateaudit` | D-ablations |
| `headtohead.tex` | `tab:h2h` | E-dialects |
| `elo.tex` | `tab:elo` | E-dialects |
| `matrix.tex` | `tab:matrix` | E-dialects |
| `port.tex` | `tab:port` | E-dialects |
| `rules.tex` | `tab:rules` | E-dialects |
| `arbitration.tex` | `tab:arbitration` | E-dialects |
| `reliability.tex` | `tab:reliability` | F-calibration |
| `manifest.tex` | `tab:manifest` | G-reproducibility |

Hand-written inline: `tab:dialect` (A-dialect), `tab:features` (C-parameters), `tab:bounds`
(C-parameters), `tab:valuecoef` (C-parameters), `tab:ablspecs` (D-ablations, `longtable`),
`tab:ablopp` (D-ablations, `longtable`).

**2 figures**, both TikZ, both `\resizebox{\linewidth}{!}{\input{figures/...}}`:
`fig:architecture` (`paper/figures/architecture.tex`, hand-written — the inference/decision
pipeline) and `fig:reliability` (`paper/figures/reliability.tex`, **generated** by
`build_tables.py`). No external image files; no `graphicx` includes.

**18 labelled equations**: `eq:Z, eq:askutil, eq:blocktable, eq:bound, eq:bwd, eq:capacity,
eq:cond, eq:disj, eq:fwd, eq:marg, eq:pathmass, eq:prior, eq:q1, eq:q2, eq:q3, eq:softmin,
eq:stop, eq:twoply`.

**6 theorem environments**: 2 in `05-belief.tex`, 4 in `06-locked.tex` (see §a.1).

## e.5 Citations and .bib handling

**There is no `.bib` file anywhere in the repository** (`find . -name '*.bib'` → empty).
Bibliographies are hand-maintained `\begin{thebibliography}{99}` blocks:

- `paper/sections/bibliography.tex` — **78 `\bibitem`s**, 391 lines, grouped under `%%`-comment
  section headers by topic ("Fish and Literature", etc.). Keys are short mnemonics
  (`fishbot03`, `pagat`, `develin`, `dorsa`, `strategy-site`, `wikiliterature`, `somani`,
  `somani-server`, `lisy-lbr`, …). Entries carry full URLs via `\url{}` and, for web sources, an
  access/last-updated date; archived links use `web.archive.org`.
- **97 `\cite{}` uses** across `paper/sections/*.tex`.
- v0.5: `paper/sections_v05/bibliography.tex`, **46 `\bibitem`s**, same style.

No `bibtex`/`biber` step exists in any build command.

## e.6 Numbers discipline — `paper/numbers*.tex` are GENERATED

- `paper/numbers.tex` — **182 `\newcommand`s**, written wholesale by
  `engine/build_tables.py` (`flush_numbers()` at line 146, writes
  `paper/numbers.tex` at line 152; `TAB = paper/tables`, `FIG = paper/figures`). Entry point
  `flush_numbers()` at line 810; the script also regenerates `docs/V04_RESULTS.md` (line 851
  prints "tables, numbers and docs/V04_RESULTS.md written"). **No number is typed by hand into a
  section.**
- v0.5 uses a two-file scheme: `paper/numbers_v05.tex` (801 lines, hand-written
  `\providecommand` placeholders, each under a `%%` comment naming its source artifact) plus
  `paper/numbers_v05_generated.tex` (912 lines, `% GENERATED by engine/build_tables_v05.py --
  do not edit`, every macro `\providecommand{}{}\renewcommand{}{value}` with a per-macro source
  comment). `fishbot_v05.tex:75-80` inputs the placeholder file then
  `\InputIfFileExists{numbers_v05_generated}{}{}`, so a stale default can never reach the PDF once
  the battery has run. **Adopt this two-file scheme for v0.6** — it is strictly better than v0.4's
  single generated file.

## e.7 Consistency tooling

- `paper/check.py` — three classes, each of which has caught a real defect:
  **BANNED** (per-version phrase blacklist: for v0.4 e.g. `optimal[- ]stopping`, `\bexact
  two-ply\b`, `upper bound.{0,40}exploitab`, `rejection-free(?!.{0,80}C1)`,
  `(safe|lossless) pre-?gate`, `\bthe first (Fish|agent|engine)\b`; for v0.5 e.g. calling the
  deadlock an information freeze, calling a `Fast`-belief allocation probability "exact");
  **MACROS** (every `\num…` referenced by a section must be defined in that manuscript's numbers
  file; no doubled percent signs — with per-profile `SELF_PERCENT` sets);
  **STRUCTURE** (every `\ref`/`\eqref`/`\cite`/`\input` resolves; no float without a caption and a
  label). Profiles are declared at `check.py:160-171`; run `python3 paper/check.py [--v04|--v05]`.
  Non-zero exit on any class-1/2 hit or dangling reference.
- `paper/check_provenance.py` (v0.5 only) — splits macros into **GENERATED** vs **TRANSCRIBED**
  and fails if a transcribed macro has no source-comment header or names a missing artifact;
  reports the transcribed/generated split that the reproducibility appendix quotes.
- `paper/inline.py` — flattens `\input` trees (depth ≤ 6, also handles mid-line `\input`) into a
  single Overleaf-ready file. `python3 inline.py <src.tex> <dst.tex>`; defaults to
  `fishbot_v04.tex` → `fishbot_v04_standalone.tex`.
- `engine/build_manifest.py [v05]` — artifact checksums into `research/v0N/results/MANIFEST.json`.

## e.8 Build

```
npm run paper:v04   # cd paper && python3 inline.py \
                    #   && tectonic -X compile fishbot_v04.tex --outdir ../output/pdf
npm run paper:v05   # cd paper && python3 inline.py fishbot_v05.tex fishbot_v05_standalone.tex \
                    #   && tectonic -X compile fishbot_v05.tex --outdir ../output/pdf
```

(`package.json:32-33`.) The toolchain is **tectonic only** — `/opt/homebrew/bin/tectonic`; there
is no `pdflatex` or `latexmk` in this environment (`research/v05/PAPER_PLAN.md`). Output lands in
`output/pdf/`, alongside the `.log`. A `paper:v06` script must be added the same way.

Byline of record (auto-memory `fishlab-paper-authorship`): **`Dylan Nguyen\\\small FishLab
Research Project`**, with the repository URL and the exact commit the experiments ran at, stated
per experiment group when they differ.

---

# (f) Like-for-like checklist for v0.6

1. **Report the E4 nine-style profile (seed 515253, 300 deals × 6 rotations) plus the E10 three
   deception archetypes (seeds 31415926, 8675309, 400 deals × 6 rotations)** as one twelve-style
   table with an explicit worst case and minimax regret. That is the only comparison that is
   like-for-like against both v0.4 and v0.5. Standing owner rule: never headline an aggregate.
2. **Gate on the E2 pathology KPIs before the scoreboard** (`fish pathology --games=300
   --rotations=2 --seed=31`, both mirrors + the 90210 cross). The two highest-scoring v0.5
   ablation rows are the pathological ones.
3. **Held-out head-to-head over the five E3 banks** (90210, 31337, 515151, 777001, 424242 —
   300 deals × 6 rotations each) so the v0.5 row 50.11/49.50/52.33/50.89/51.11 is directly
   comparable.
4. **Run the exploitability probe** — the v0.5 study's own named largest hole; the v0.4 baseline
   is 51.19% [49.67–52.72] achieved by a same-class exploiter (lower bound within the class).
5. **Fitting banks must stay disjoint** from 90210, 31337, 515151, 515253, 777001, 424242, 606060,
   717171, 828282, 909090, 31415926, 8675309, 31, 20260820.
6. **Fix `BlockDP` thread_local aliasing before adopting exact block beliefs** (§d.3).

Byline: Dylan Nguyen, FishLab Research Project.
