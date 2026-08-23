# FishBot v0.7 — Suboptimality ledger

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`, audited at commit `60fee17` ("v0.6").
Phase 0 of the v0.7 programme. Companion to `docs/v07/THREAT-MODEL.md`.

Every candidate source of remaining exploitable weakness in the v0.6 frontier, ranked by expected
gain × confidence, each with its evidence, the cheapest experiment that would confirm or kill it, and
what would kill it. Then the register of questions the corpus closed; then the power ledger, so
nobody mistakes a null for a zero; then the provenance defects this audit found and who found them
first.

Nothing here is an architecture proposal. Where an entry names a mechanism it is naming the
*hypothesis*, not the design; where it quotes a design instruction, the instruction is the corpus's
and is marked as a quotation.

This document was written, then attacked against the artifacts by an independent adversarial pass,
then corrected. §4 records what that pass changed, including where it overturned a claim in the first
draft.

---

## 0. How to read this

### 0.1 The incumbent is a frontier, and its cheap end is unmeasured

v0.7 must dominate **both** ends of this, and the middle of it has never been timed on the shipped
parameter vector.

| Operating point | What it is | Strength | Throughput |
|---|---|---|---|
| **F-fast** — `v06` | the deployed policy | +0.89 pts vs v0.5 head to head over 126,000 games (§0.4); pooled worst cell 50.00 over 13 styles; minimax regret 3.06 | **303.4 games/s**, all threads (`research/v06/results/E9-throughput.txt`) |
| **F-search** — `v06:s1=1,det=12,cand=4,kappa=2.5` | the strongest configuration measured | **52.08%** against `v06` itself over 2,880 games, 4/4 cells above parity, worst 50.83% (`\vsixSearchLift`, `paper/tables_v06/searchrepl.tex`) | **0.73–1.01 games/s** on the shipped vector — **300×–420×** slower than F-fast (720 games in 709.8–992.3 s, `research/v06/results/F0-search-confirm.jsonl`) |
| **F-mid** — `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26` | the endgame-restricted search | **+1.35 points ahead of `v06`** on a paired six-opponent panel, n = 4,800/arm, ahead on 5 of the 6 opponents (§L2) | **never timed on the shipped vector.** On v0.5's vector the endgame restriction buys **4.9×** over the full search (102.9 s vs 507.5 s per 720 games, `E12-search.jsonl`), which would put F-mid near 60–90× — **an inference, not a measurement** |

Two corrections to the corpus's own framing follow, and one correction to this document's first
draft.

* The paper says the search costs "three orders of magnitude" and that this is why it ships off
  (`paper/sections_v06/08-search.tex`, `sec:search-ships`). Measured on the shipped vector against
  the shipped policy that is **300–420×** — two and a half orders. The "three orders" figure divides
  an all-threads number (303.4 g/s) by a single-thread one (`\vsixSearchGps` = 0.144 g/s) and is not
  a same-basis ratio. The corpus's own final audit flags the inconsistency
  (`research/v06/notes/V2-final-audit.md`, SF-6).
* **This document's first draft put F-search at 214× and F-mid at 43×, using timings from
  `E12-search.jsonl`. Every row of that artifact runs `v06:legacy=1` — v0.5's 34-coordinate vector
  with `extraFeats` cleared (`engine/experiments_v06.sh:92`; `engine/src/factory.hpp:138`). Those are
  timings of a different policy.** The correction is above. It was V1 that first recorded the
  `legacy=1` fact (`research/v06/notes/V1-adversarial-verification.md:314-316`: "Every E12 search row
  runs `v06:legacy=1` … The search was never measured at the shipped v0.6 vector in E12").

**What survives, and it is the item that most changes what phase 1 must do:** there is a search
operating point materially cheaper than F-search whose only well-powered paired measurement is
favourable, and **nobody has ever timed it or head-to-headed it on the shipped vector.**

### 0.2 Units, and the conversion this ledger uses throughout

Fitted from the seven 18,000-game v0.6-vs-v0.5 banks (§0.4), regressing win-rate margin on mean
half-suit differential through the origin:

> **1 unit of mean half-suit differential ≈ 14.7 percentage points of win rate**
> ⟹ 1 point of win rate ≈ 0.068 sets of differential ≈ 0.034 avoided misdeclarations per game
> ⟹ **1 percentage point of declaration accuracy ≈ 1.2 points of win rate** (at 4.48
> declarations/game; the range across the seven banks is 1.0–1.4)

Sanity check on the conversion, which also independently confirms the corpus's central behavioural
claim. v0.6 declares at 98.427% against v0.5's 97.540% over the seven banks, at 4.483 declarations a
game: that is 0.0399 fewer wrong declarations per game, and a wrong declaration hands the half-suit
to the opponents, so the differential it buys is 2 × 0.0399 = **0.080 sets**. The *observed* pooled
differential is **0.062 sets**. **The declaration channel accounts for the entire v0.5 → v0.6 margin
and slightly over-explains it**, the residual being v0.6's marginally *lower* ask accuracy (0.5492 vs
0.5505). This is computed here from the raw artifacts and agrees with
`research/v06/RESULTS-SUMMARY.md`'s statement that "the gain is declaration accuracy, not ask
accuracy."

### 0.3 The power rule, applied to everything

95% half-width ≈ 98/√N points at p ≈ ½.

| N (games) | 400 | 720 | 800 | 1,200 | 1,800 | 3,600 | 4,800 | 18,000 | 126,000 |
|---|---|---|---|---|---|---|---|---|---|
| half-width (pts) | 4.90 | 3.65 | 3.46 | 2.83 | 2.31 | 1.63 | 1.41 | 0.73 | 0.28 |

Two structural caveats. The paired estimator's effective n is the number of **deals**, not games. And
in a **mirror win-rate cell the effective n is zero**, not half: the per-deal outcome is
deterministic and the artifact prints `ci [0.5, 0.5]` (`research/v06/results/E4-perstyle.jsonl`,
`v05` vs `v05`). Halving is the right correction for *rate denominators*, not for win rates.

### 0.4 A provenance notice that affects the incumbent's headline

The v0.6 paper at `60fee17` states that v0.6 is **not separated** from v0.5 head to head:
`\vsixHeadPooled` = 50.53% over 11,300 games, z = 1.13 (`paper/sections_v06/11-results.tex`,
`sec:results-h2h`; `paper/sections_v06/01-introduction.tex`).

A larger re-run exists — 3,000 deals × 6 rotations = **18,000 games a bank on seven banks, 126,000
games**, giving **50.89% [50.61, 51.16], 7/7 banks above parity**, and 51.11% over 36,000 games
against v0.4. It is **not in the working tree and not in HEAD.** It is in the uncommitted `stash@{0}`
created by GitHub Desktop, as `research/v06/results/F11-bighead-v05.jsonl`,
`F12-bighead-extra.jsonl`, `paper/tables_v06/bighead_v05.tex`, `bighead_v04.tex`, plus the
corresponding edits to the abstract, introduction, results, limitations and conclusion — and
`docs/v07/PHASE-PROMPTS.md` is in the same stash. `git reflog` shows `reset: moving to HEAD` as the
most recent operation.

Every "+0.9 head to head" statement in this ledger is sourced to that stash and is quoted as such,
and every per-bank row in §0.2 is read from the stashed JSONL. **Before phase 1 starts, the stash
should be restored and committed, or the result explicitly abandoned.** A frontier whose headline
number lives only in a stash is not a frontier.

### 0.5 Scoring

`Priority = expected gain at the frontier (win-rate points, upper end of the stated range) ×
confidence (0–1)`. Confidence is the probability that the mechanism is real *and* the gain is roughly
the stated size, given today's evidence. Both are judgements; they are written down so phase 2 can
disagree with a number rather than with a mood.

Entries that are not candidate *gains* — instruments, risks, and enabling work — carry no priority
and are listed separately in §2 rather than being interleaved into a ranking they cannot be ranked
in.

---

## 1. The ranked ledger

| Rank | # | Lead | Gain (pts) | Conf. | Priority |
|---:|---|---|---:|---:|---:|
| 1 | **L2** | Test-time search at **F-mid**, whose only well-powered paired measurement is printed with the sign inverted | 1.35 | 0.45 | **0.61** |
| 2 | **L1** | The declaration-allocation channel: 100% of v0.6's margin lives here and 2.1 points of it remain | 0.8 | 0.60 | **0.48** |
| 3 | **L3** | **11.7% of v0.6's asks are guaranteed misses** into half-suits its own team already owns — and the refit made this *worse* | 1.5 | 0.30 | **0.45** |
| 4 | **L10** | M1's live-ask gate as a published exploitability hazard | 1.5 | 0.25 | **0.38** |
| 5 | **L8** | The Price of Uncorrelation (threat-model H1): what forgoing a shared secret seed costs | 1.0 | 0.35 | **0.35** |
| 6 | **L4** | White-box transcript inversion against a deterministic policy (threat-model class C5) | 1.0 | 0.30 | **0.30** |
| 7 | **L11** | Signalling: 1.30 bits/ask of unpriced card-dimension choice, and no term anywhere prices information delivered to a teammate | 1.0 | 0.25 | **0.25** |
| 8 | **L14** | The deliberate miss, priceable only under search | 0.5 | 0.30 | **0.15** |
| 9 | **L13** | Forced-endgame allocation — **demoted**, worth ≈ 0.016 pts at v0.6's incidence | 0.016 | 0.90 | **0.014** |

---

### L2 — Test-time search at F-mid, and the sign inversion that hid it

**Rank 1. Priority 0.61. One command re-run.**

**Claim.** The paired panel ablation of the endgame-restricted search is printed in the paper with
its sign inverted. Corrected, it is the best-powered evidence in the corpus that test-time search
adds *on top of v0.6*, and it applies to a configuration materially cheaper than the headline search.

**Evidence.** `research/v06/results/E5-ablations.json` reports, for
`v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26`: `winRate = 0.69771` against the reference `v06`'s
`0.68417`, `deltaFromRef = -0.01354`, `ci = [-0.02792, +0.00042]`. `deltaFromRef` is **reference
minus variant**: `pairedBootstrap(refPaired, variantPaired)` computes `mean(x − y)`
(`engine/src/arena.hpp:140-158`, called at `engine/src/main.cpp:513`) and the identity checks exactly
on the `v05` row (0.68417 − 0.65729 = +0.02687). So the artifact says the search variant is
**1.35 points better than v0.6**, with an interval running from 0.04 points worse to 2.79 points
better.

`engine/build_tables_v06.py:217` negates `deltaFromRef` when writing `ablations.tex`;
`build_tables_v06.py:813` does **not** negate when writing `fourway.tex`. Both tables carry the
header `\vsix{} $-$ variant`, and `tab:ablations`'s caption says "A positive delta means \vsix{}
beats the variant" (`paper/sections_v06/11-results.tex:173-174`). `ablations.tex` is the one that is
wrong, and it is included twice (`11-results.tex:177`, `D-ablations.tex:71`). Two consequences
visible in the published table:

* the `v05` row prints **−2.69**, i.e. it states that v0.6 *loses* to v0.5 by 2.69 points on the
  panel — flatly contradicting the paired-panel table in the immediately preceding subsection
  (+2.16 mean, all three banks positive, `paper/tables_v06/pairedpanel.tex`) and the abstract. It is
  the same number, `0.02687`, from the same artifact, printed with opposite signs by two lines of one
  generator;
* the search row prints **+1.35**, reading as evidence against the search when the artifact is
  evidence for it.

**Per-opponent profile, because the standing rule forbids headlining the aggregate.** From the same
artifact (`v06` minus variant, so negative means the search is ahead):

| opponent | v06 | F-mid variant | v06 − variant |
|---|---:|---:|---:|
| v05 | 48.88 | 51.25 | **−2.37** |
| v04 | 52.25 | 52.50 | −0.25 |
| v03 | 75.50 | 76.62 | −1.12 |
| lockout | 78.38 | 79.88 | −1.50 |
| detective | 76.62 | 79.88 | **−3.25** |
| withholder | 78.88 | 78.50 | +0.37 |

The search is ahead on five of six and behind only on the withholder, by 0.37 points. The aggregate
is not carried by one column, and the worst cell is small — which is the profile the project's own
reporting rule asks for and which the aggregate alone would have hidden.

Sample size: 400 deals × 2 rotations × 6 opponents = **4,800 games per arm**, paired, seed 606060
(`engine/experiments_v06.sh:60-62`). This is better powered than every cell in the search ladder
(720 games each, ±3.65).

**Prior art within the corpus.** V1 already read the raw artifact correctly:
"the guarded search is the *best* arm in that ablation, point estimate +1.35 points over shipped
v0.6, interval barely touching zero. Reporting it as '≈0 guarded' is defensible on the interval and
is not what the point estimate says" (`research/v06/notes/V1-adversarial-verification.md:317-320`).
**What is new here is only the rendering bug** — that the published table prints the sign backwards,
so a reader of `tab:ablations` sees the opposite of the artifact. The inference was not hidden; the
table hides it.

**What is genuinely unresolved.** The first draft of this document claimed a "tension" with the
48.89% head-to-head row for the same configuration. **There is no such tension: that row is a
different policy.** `paper/tables_v06/search.tex` is generated from `E12-search.jsonl`, every row of
which is `v06:legacy=1,…` — v0.5's vector. So the real state of the evidence is thinner than either
reading suggested: **the F-mid configuration has exactly one measurement on the shipped vector, on
one bank, and no head-to-head number at all.**

**Cheapest experiment.** Re-run `fish ablate` with the same variants on a **second disjoint bank**,
print the sign convention and full spec into the artifact, and add a head-to-head cell of F-mid
against v0.6 at 18,000 games. Fix `build_tables_v06.py:217` at the same time. If F-mid runs near the
inferred 3.5–5 games/s, the ablation arm is ~20 minutes and the head-to-head is ~1–1.5 hours.

**What would kill it.** The second bank returning a delta of the opposite sign; or the head-to-head at
adequate power putting F-mid below v0.6. Also L7: if three simultaneous searchers corrupt each
other's beliefs, the panel result may be an artifact of the panel opponents being non-searchers.

---

### L1 — The declaration-allocation channel

**Rank 2. Priority 0.48. The cheapest decisive experiment in this document requires zero games.**

**Claim.** The whole of v0.6's measured margin is declaration accuracy (§0.2), roughly 2.1 win-rate
points of misdeclaration remain, and the mechanism that produces them is identified in code. What
*fixes* them is not yet identified, and the first draft of this document was too confident that it
was.

**The channel, which is solid.**
* v0.6's mirror misdeclaration rate is `\vsixPathSixDeclwrong` = **2.37%** and its head-to-head rate
  against v0.5 is **1.573%**. Eliminating misdeclarations entirely is worth
  2 × 0.01573 × 4.483 = 0.141 sets of differential = **≈ 2.1 win-rate points** — more than twice
  everything v0.6 achieved.
* **72–75% of the remaining misdeclarations are pure allocation errors** — the team held all six
  cards and named the wrong teammate (46/64 for v0.5, 40/53 for v0.4); proof-backed declarations are
  never wrong (0/1230). All the error lives in the belief-only half
  (`research/v06/notes/R0-OPPORTUNITY-REGISTER.md`, V6-M9, citing R6).

**The mechanism, stated carefully.** The shipped declaration path chooses the six-card assignment by
maximising a **product of independent marginals** subject to capacity and certificate feasibility, and
only then scores the winner jointly: `engine/src/v05.hpp:619-664` (`feasibleAllocation`, M2,
`feasibleDecl = true` at `v05.hpp:133`) enumerates up to 3^nFree assignments, rejects those violating
a certificate mask or a capacity bound, and scores each by `score *= bel.marg[c][p]`
(`v05.hpp:661`). It is a feasibility-constrained per-card argmax.

**Why the obvious fix is not obviously a fix, and this is the correction the adversarial pass
forced.** `engine/src/blockdp.hpp:454-479` (`BlockDP::bestTeamAllocation`) is the exact joint
maximiser and is unreachable in the shipped configuration (`BeliefMode::Block` only,
`v05.hpp:724-735`; shipped belief is `Fast`, `v05.hpp:27`). But it maximises over **count vectors**
and then breaks the within-count-vector tie **lexicographically** — its own comment says why: "any
surviving assignment with this count vector is equally likely; take the lexicographically first that
gcard proves exists" (`blockdp.hpp:470-472`). Under the uniform-deal prior the exact posterior is
*flat* over surviving assignments sharing a count vector. So on exactly the error class this entry
sizes — the team is known to hold all six and the question is which teammate holds which card — the
exact object is **indifferent**, and resolves it arbitrarily. The deployed marginal product is not
indifferent; it may be better or worse.

Two further tensions the first draft did not reconcile:
* Ledger C2 records that the exact posterior under a uniform prior is the **worst** of the three
  inference paths as a predictor (1.42246 nats / 47.94% argmax hit against the approximation's
  1.38218 / 51.49%). Routing declarations through the same object needs an argument, not an
  assumption.
* `belief=block` as a whole loses badly in play: 39.83 / 40.33 / 35.83% at three seeds
  (`R0` §5, Q1), and it costs 14× the deployed belief (21.7 games/s against 251–289).

**So the entry is the channel, and the mechanism is open.** Candidate discriminators for the
within-count-vector choice are the policy prior (which is exactly what the exact object discards and
what makes the deployed approximation the better predictor), and the certificates the count vector
does not already encode. Register item **V6-M9** is open and was ranked below M1–M8 explicitly for
size, not difficulty.

**Cheapest experiment (no games, no policy change).** A pure replay. Instrument the existing
declaration decisions in a recorded battery; at each, record (a) the allocation the shipped
marginal-product rule names, (b) the allocation `BlockDP::bestTeamAllocation` names, (c) the exact
posterior's *support* over feasible allocations, and (d) the truth. That yields three numbers at once:
how many currently-wrong declarations each rule would fix, how many it would break, and — decisively
— **what fraction of the errors are in a state where the exact posterior is flat and therefore no
belief-based rule can do better.** That last number converts the entry from an estimate into a
measured ceiling. It is the same replay pattern `research/v05/results/P2-forced-endgame.md` §4 used
to establish the 40.6% forced-endgame ceiling, applied to the *voluntary* path where the volume is
about 1,400× larger.

**What would kill it.** The replay showing that most allocation errors sit in flat-posterior states.
That would close the entry as an information limit rather than a mechanism defect — and would be a
genuinely valuable negative, because it is the second-largest quantified channel in the corpus.

---

### L3 — 11.7% of v0.6's asks are guaranteed misses, and the refit made it worse

**Rank 3. Priority 0.45. Not addressed by any v0.6 mechanism.**

**Claim.** The largest measured behavioural defect in the shipped policy is that it asks, at a rate of
about one ask in nine, for cards inside half-suits its own team already owns outright. Every such ask
is a certain miss that hands the turn to the opponents.

**Evidence.** `research/v06/results/E2-pathology.txt`, 600 games per arm at seed 31, counters pooled
over all six seats:

| | v0.4 mirror | v0.5 mirror | **v0.6 mirror** | v0.6 vs v0.5 |
|---|---:|---:|---:|---:|
| asks in own-locked half-suits | 16.46% | 9.75% | **11.69%** | 9.01% |
| ask hit rate | 34.25% | 55.63% | **53.29%** | 55.59% |

**v0.6 is 1.94 points *worse* than v0.5 on this KPI**, and its mirror hit rate is 2.3 points lower.
The channel is present in every table composition the register measured (7.5%–12.2% across six
opponent types, `R0` loss channel #2), so it is not a mirror artifact. Independent measurements agree:
R1 at 9.77%, R9 at 10.16% for v0.5.

The mechanism is structural and is named in the register: M1's live-ask gate prunes asks the actor can
**prove** will miss, and no single seat can prove a *teammate* holds a card (`enumerateAsks` only
permits asking opponents, `engine/src/fish.hpp:179-196`). So these asks are "provably dead" from a
team-omniscient view and perfectly legal-looking from the seat's view — which is why the commit-gate
KPI reads `\vsixGateDeadAsk` = 0.01% while this one reads 11.69%. The root cause is the **5.55-event
mean gap between a team physically owning a half-suit and being able to prove the allocation**
(R6, via R0). Locks are never broken (0 of 5,370), so it is pure delay, not risk.

This is a **joint-inference** problem and it is the same underlying object as L1: what the team knows
about its own holdings, as opposed to what any one seat can prove.

**Sizing.** ~5 guaranteed-miss asks per team per game, each surrendering the turn (worth ~0.41
half-suits mid-game rising to 0.826 after event 80, R8 §4.2). The naive arithmetic is enormous and is
certainly wrong, because the counterfactual ask also frequently misses. Stated conservatively: if the
own-locked asks were replaced by asks at the rate the rest of the policy achieves, the overall hit rate
rises from 53.3% to 60.3%. **The conversion from hit rate to win rate is not linear and is not known**
— v0.5 → v0.6 lost 2.3 points of hit rate while gaining win rate. Hence confidence 0.30, not 0.7.

**Cheapest experiment.** A one-sided oracle ablation: give the acting seat ground-truth knowledge of
its **own team's** holdings only (never the opponents'), leave everything else identical, and measure
the paired win-rate delta on a fixed bank. That is an upper bound on the whole channel and settles
whether it is worth 0.2 points or 2. The corpus already has the pattern (`askoracle`, R6). It is a
diagnostic configuration and can never ship.

**What would kill it.** The oracle ablation returning under half a point. It would also be
substantially killed if the gain is recoverable only from information the seat cannot have, in which
case it converts into a declaration-timing question and merges with L1.

---

### L10 — M1 as an untested exploitability hazard

**Rank 4. Priority 0.38. Cannot be tested by the corpus's only responder class.**

M1's live-ask gate restricts candidates to asks with strictly positive hard-consistent probability
(`engine/src/v05.hpp:107`, `:482-500`). Zhang & Sandholm (*Subgame solving without common knowledge*,
NeurIPS 2021) **prove** that naive knowledge-limited solving can *increase* exploitability, and then
develop three avenues that restore safety. M1 is a naive knowledge-limited pruning rule. The corpus's
own literature note flags this and says "any v0.6 pruning must be audited by LBR-team, not assumed
safe" (`research/v06/notes/R7-literature-search.md` §1.5).

It was not audited, and could not have been: the exploitability probe fits a same-family linear
responder, and "manoeuvre the target into a state where its gate prunes its best ask" is not
expressible in that class. M1 is also load-bearing — with `m1=0` and v0.5's weights intact, dead asks
reach 46.28% and 14.4% of games hit the action cap (`R0` loss channel #6) — so this is a
*characterise-and-defend* entry, not a *remove* entry.

**Cheapest experiment.** Phase 2, class C3 or C6: an exploiter whose explicit objective is to maximise
the frequency with which the target's gate binds on what would otherwise have been its argmax. The
corpus measured the gate binding on the argmax at 5.78% of decisions (R1); the question is whether an
adversary can raise that.

**What would kill it.** An exploiter that can raise the binding rate but not the win rate.

---

### L8 — The Price of Uncorrelation

**Rank 5. Priority 0.35. Never measured in Fish.**

**Claim.** Three deterministic identical agents realise a single **pure joint plan** — the degenerate
point of the TMECor polytope. Three agents with independent randomisation sit in the TME regime.
Three with a shared secret pre-play seed span the full correlation polytope, and
`v_Com ≥ v_Cor ≥ v_No` (Celli & Gatti, AAAI-18, Property 2). The gap between the corner v0.7 occupies
and the polytope it could occupy is bounded by theory — Basilico et al.'s `m^{n−2}` in normal form,
and Schulman & Vazirani's defensive gap at ≤ 1 − 2^{1−k} = **3/4 of the payoff range** at k = 3 — and
its size in Fish is unknown.

**Why it may be near-zero here.** Under the white-box threat model the correlation device is not
secret from the opponents, and a correlation device is only worth its TMECor value if the opponent
cannot simulate it. The whole TMECor literature assumes a private device; a team of three identical
*published* deterministic agents inverts that assumption, and no paper covering the inverted case was
found.

**Cheapest experiment.** A shared-seed ablation: three seats drawing tie-breaks from one pre-play seed
versus three drawing independently, paired on the same deal bank, both against a fixed opponent. It
measures the realised PoU directly and needs no new policy — only a seed-plumbing flag, which
threat-model T10 requires anyway. H1 forbids the shared-seed configuration from shipping; measuring
what it is worth is what makes H1 a decision rather than an assumption.

**What would kill it.** A measured gap under 0.3 points, which would close the question permanently
and license H1 as free.

---

### L4 — White-box transcript inversion

**Rank 6. Priority 0.30. Never tested, anywhere, by anyone.**

**Claim.** Every FishBot to date is deterministic (verified in code, `docs/v07/THREAT-MODEL.md` §4.2).
Under the white-box threat model, each observed action exactly partitions the deal space into deals
under which the actor would have played it and deals under which it would not. If that contraction is
large, an adversary's posterior over v0.7's hands is sharper than the certificates alone imply, and
the whole team is more readable than any prior study has assumed.

**Evidence for the premise.** The shipped belief is Sinkhorn (`Fast`) and consumes no randomness
(`engine/src/v05.hpp:185-189`); `chooseAsk` is an argmax whose bit-for-bit ties are resolved by
**unstable `std::sort` order** (`engine/src/v06.hpp:403-405`, `:488`) — deterministic, and an
implementation artifact rather than a documented rule; v0.6's one stochastic switch, `randomTie`, is
seeded from a hash of the **public** event stream and is therefore reproducible by any observer
(`v06.hpp:489-491`). The corpus has measured the size of the unforced choice at **0.959 bits
(target) / 1.301 bits (card within half-suit) / 4.979 bits (joint)** per ask
(`research/v06/notes/R9-human-tactics-catalogue.md`, T#11), and separately established that moving
inside the certificate-equivalence class flips v0.4's own preferred ask at 86–88% of such decisions
(`research/v05/results/P5-verify-target-channel.md` §3) — i.e. the class is indistinguishable to the
*rules* but highly distinguishable to the *policy*, which is exactly the condition that makes
inversion informative.

**Evidence for the doubt.** There is no theorem that a pure strategy is highly exploitable, and the
strongest empirical study runs both ways: Ganzfried, Sandholm & Waugh (AAMAS 2012, Table 4) measured
worst-case exploitability exactly in the full unabstracted game and found purification *reduced* one
bot's exploitability by 25% and *raised* another's by 86%. And the project owner's own live report
(#4, via `R9` blocker B5) is that a human who has played many games learns the policy — which is
evidence for the premise from the one source that has actually played it.

**Cheapest experiment (no games).** Offline replay of an existing battery's transcripts. At each state
compute the size of the deal posterior's support under (a) the certificates alone — which
`BlockDP`/`DealDP` already compute exactly — and (b) the certificates plus the constraint "the
observed ask maximised the known score". Report the mean log-ratio, in bits per ask, as a function of
event index. **If the contraction is under ~1 bit per ask the hypothesis dies for free**, and it dies
before anyone builds a class-C5 responder.

**What would kill it.** The bit measurement coming back small; or the follow-on responder failing to
convert a large contraction into win rate, which would itself be a publishable negative ("readable but
not exploitable").

**A note on the corpus's existing rebuttal.** The v0.6 record offers a negative control for exactly
this — "Two negative controls establish that the gain is information, not variety"
(`research/v06/RESULTS-SUMMARY.md:106`; the paper's version is
`paper/sections_v06/08-search.tex:137-140` and `abstract.tex:23`). **That control is weaker than it
reads; see §4, P-2.**

---

### L11 — Unpriced signalling to teammates

**Rank 7. Priority 0.25. Gated by owner decision D1.**

No FishBot has any term that prices information delivered to a **teammate**. All three information
features (`f[9]` leak, `f[16]` exposure on miss, `f[19]` leak magnitude) price leakage to
*opponents*; a grep for `signal|convention|infoGain|codebook` in `v05.hpp` returns **0** (`R9`
blocker B3). Meanwhile the unforced choice is 1.301 bits/ask on the card dimension alone — larger than
the target dimension that earlier work headlined — and a provably-dead ask (a costly signal naming a
half-suit) is available at 79.2% of decisions at a price of about one expected take (`R9` T#11, T#12).

Two things hold this entry down. **Owner decision D1** is that pre-agreed codebooks ship behind a
flag, default **off**, with the delta published — the source dispute is real (Develin: conventions are
explicitly forbidden in Canadian Fish; pagat documents one in live use). And under the white-box threat
model, any convention a teammate can decode a v0.7-aware opponent can also decode, so the net value is
not the bits but the bits times the asymmetry in who can use them, which for a published policy is
close to zero.

**Cheapest experiment.** Threat-model test **S1**, the grounding budget: the value gap between playing
under the grounded posterior (rules only) and under the full policy-aware posterior. **That single
number is the ceiling on this entire entry.** It is not free — it needs a posterior variant with the
policy prior switched off (which `priorTheta`/`priorPhi` already permit) and a paired match — but it
is one cell.

**What would kill it.** A grounding budget under half a point.

---

### L14 — The deliberate miss

**Rank 8. Priority 0.15. Measured; not separated.**

M1 deleted the move class every multi-step human tactic is built from — blackballing, choosing which
opponent receives the turn, paying for a signal with a safe ask — all of which are a guaranteed miss at
a *chosen* seat, available at 79.2% of v0.5's decisions and at two or more distinct chosen opponents at
50.1% (`paper/sections_v06/08-search.tex`, `sec:search-deadask`).

Unbanning it wholesale is measured and rejected: `\vsixDeadWin` = 51.17% [48.34, 53.99] — *higher* —
while failing the commit gate outright at 35.85% provably dead asks, longest dead run 365, and 10% of
games killed by the action limit. Rationing it does not work either, and the reason is structural and
important: **the linear score cannot price a deliberate miss.** The hit-probability term is zero on
such an ask by definition and every remaining term is a penalty, so sweeping the admission margin gave
bit-identical output at every setting.

Only search can price it. With the deliberate miss in the candidate set the endgame-restricted search
scores `\vsixSearchDead` = **50.28%** [46.67, 53.89] at n = 720. The paper calls this "the best search
configuration measured" (`08-search.tex`, `sec:search-deadask`); read across the whole ladder that is
not so — `paper/tables_v06/search.tex` has `kappa=4` at 53.89% and `kappa=2.5` at 53.75% against the
same opponent at the same n. 50.28% is the best of the *endgame-restricted* rows (48.89 → 50.28), and
all of these are on v0.5's vector (§0.1).

**Cheapest experiment.** It rides for free on L2's re-run: `deadsearch=2` is already a variant in
`E12-search.jsonl`. Include it in the second-bank ablation, on the shipped vector this time.

---

### L13 — Forced-endgame allocation: demoted

**Rank 9. Priority 0.014. The phase brief's lead (b) names this; the arithmetic does not support it.**

The v0.5 study repaired forced-endgame accuracy from 0.14% to 24.35% per declaring team over 24,000
games per arm against a measured feasible ceiling of ≈ 40.6% (`docs/V05_FINDINGS.md:134-135`;
`research/v05/results/P2-forced-endgame.md` §4; the ceiling rises to 46.6% if the best-positioned
teammate declares rather than the lowest seat). That gap is real. It is also worth almost nothing at
v0.6's incidence:

* v0.6's forced-endgame rate is **0.0031 per game** (mean `forcedPerGameA` over the seven stashed
  18,000-game banks, §0.4; the five committed E3 cells at 1,800 games each give the same 0.00311).
  `\vsixPathSixForced` = **0** forced declarations in 600 mirror games.
* Its forced-endgame accuracy is 0.286 (same source).
* Closing the whole gap — 0.286 → 0.466 — is 0.0031 × 0.180 × 2 = 0.0011 sets of differential =
  **0.016 win-rate points.**

That is 1/130th of L1 and 1/55th of the v0.5 → v0.6 margin. As a share of *declarations* the forced
endgame is 0.0031 ÷ 4.483 = **0.069%**. The register reached the same conclusion independently and put
"forced-endgame work of any kind" on its do-not-rebuild list on incidence grounds (`R0` §3 item 4:
0.11% of half-suits in the v0.5 mirror). **This ledger endorses that and re-ranks the brief's lead (b)
accordingly: the *voluntary* declaration channel (L1) is the real item; the forced endgame is not.**

**The one thing that keeps it open at all: incidence is an adversarial variable.** v0.4 had 3% of games
ending in a forced endgame because it walked into them; v0.6 has 0.3%. An opponent that deliberately
drives the game to a cardless team raises the rate, and there is a hint in the data that this is not
hypothetical — the responder fitted against v0.6 has `forcedPerGameA` = 0.0222, roughly **seven times**
the normal rate (`research/v06/results/X1-lbr.jsonl`). **Phase 2 should measure forced-endgame
incidence under adversarial pressure before this entry is closed for good**, and if an adversary can
raise it by an order of magnitude the entry returns at ~0.15 points.

---

## 2. Entries that carry no priority: instruments, risks, enabling work

These are not candidate gains and cannot be ranked against them. They are listed here so that the
ranking above is a ranking, not a mixture.

### L5 — The per-decision objective (enabling; confidence 0.85 that it is necessary)

The v0.6 conclusion names this in terms: "a study that wants to find a quarter-point mechanism will
need a per-decision objective rather than a per-game one"
(`paper/sections_v06/14-conclusion.tex`). The arithmetic behind it is §0.3: at a thousand deals a cell,
paired, on two banks, this policy class looks flat, and the corpus documents five mechanisms that
resolved at a small budget and vanished at a large one (`paper/sections_v06/10-protocol.tex`,
Table `tab:resolution`; `research/v06/RESULTS-SUMMARY.md:255` calls the chain pass a sixth).

Every scored entry is bottlenecked on it. L1 is the clean example: the declaration channel fires 4.48
times a game and its errors are individually labelled by ground truth, so scoring the mechanism on
*declarations* rather than on *games* buys roughly the ratio of decisions to games in effective
sample. It is phase-1 instrumentation and the phase-1 exit criterion already requires it.

### L6 — Partner transfer: the sharpest stated limitation is itself unmeasured (unresolved)

The v0.6 paper calls this "the sharpest limitation in the paper and it is measured, not conjectured"
(`paper/sections_v06/13-limitations.tex`). The table is `paper/tables_v06/partners.tex`, 400 deals × 2
rotations = 800 games per cell at seed 313131 (`engine/experiments_v06.sh:125-131`):

| partners | v0.6 | v0.5 | delta |
|---|---:|---:|---:|
| itself | 52.25 | 50.00 | **+2.25** |
| v0.3 | 34.50 | 33.12 | +1.38 |
| detective | 33.50 | 33.62 | −0.12 |
| withholder | 34.75 | 35.50 | −0.75 |

**At 800 games a cell the half-width is ±3.46 points.** Not one of the four deltas is separated from
any other. (Note that the v0.5 self-play cell is exactly 50.00 with zero variance — it is a mirror —
so the +2.25 is really v0.6's 52.25 against a fixed 50, at ±3.46.) The paper says the mixed rows are
unseparated and then treats the pattern as established. To be fair to the paper: the claim appears
**only** in `13-limitations.tex`, explicitly labelled as a limitation, and is not carried into the
abstract, introduction or conclusion — the first draft of this ledger said otherwise and was wrong.

The honest reading is that **the corpus does not know whether v0.6's advantage transfers**, in either
direction. The Hanabi line reports self-play-to-cross-play collapses that are not subtle (SAD
23.97 → 2.52 at 10,000 games per pair; IPPO 24.04 ± 0.02 → 0.12 ± 0.03 median), so the prior that a
self-play-fitted policy carries a non-transferring convention is strong — but those are neural
policies with free capacity to encode arbitrary maps, and a 37-coordinate linear score has far less
room to hide a handshake. The linear-policy case has never been measured.

**Cheapest experiment.** Re-run the table at 18,000 games a cell (±0.73). It is 4 partner settings × 2
arms = 8 cells = **144,000 games**, about eight minutes of F-fast simulator time. There is no excuse
for this table being underpowered. The more informative follow-up is cross-play between two
independently-fitted runs of the same architecture — fitA and fitC already exist as two such runs.

### L7 — Independent multi-agent search corrupts partners' beliefs (risk; −3.0 to 0 points; gates L2)

**Claim.** `v06:search` seats three identical searchers, each of which deviates from the blueprint its
two partners assume it is playing. This is the configuration the cooperative-search literature measured
collapsing, and the corpus has not tested it.

**Evidence.** Lerer, Hu, Foerster & Brown (SPARTA, AAAI 2020) sweep exactly this: two agents each
searching while assuming the partner plays the blueprint. At deviation threshold 0 the policy goes
from **22.99 to 14.41** — a 37% relative collapse, far below the blueprint alone. Even at the best
threshold setting it "never outperforms single-agent search". Their stated mechanism: "any time a
player deviates from the blueprint, they are corrupting their partner's beliefs, so this should only be
done when it is substantially beneficial." Three agents is strictly worse than two on this axis: each
deviation corrupts two partners, and each partner's own deviation compounds.

The measured deviation rate at κ = 2.5 is `\vsixSearchDevTwoFive` = **33.49%**, and it does not fall
below about 31% however far κ is raised (`paper/sections_v06/08-search.tex`, `sec:search-falsifier`) —
though note that `E14-searchdev.txt` is also a `legacy=1` measurement, so this is v0.5's vector too.
The paper's decomposition is that the 31% floor is churn inside the blueprint's own tie group,
"neutral by construction — the candidates are exchangeable". **That is a claim about the acting seat's
expected value. It is not a claim about the partners' belief models, and the partners do carry a policy
prior** (`priorTheta`, `priorPhi`, applied in `sinkhornDisj`, `engine/src/v05.hpp:186`) that the
tie-group re-choice can perturb. The corpus's own literature note states the requirement from the other
side: any randomisation must be seeded from the public history or the multi-agent common-knowledge
property is lost (`research/v06/notes/R7-literature-search.md` §S9) — `randomTie` respects that, but
the *search's* re-choices are seeded from `srng`, which derives from the deal seed, and are **not**
reproducible by a partner (`engine/src/v06.hpp:237`, `:511-527`).

**Cheapest experiment.** Run the search on **one** seat of the team only and compare, paired, to all
three. Cost: two 4,800-game cells, well under an hour at F-mid's inferred speed. The literature has
priced the available responses to a positive result (single-searcher configurations, replicated
multi-agent search, a larger deviation threshold, a soft rather than argmax update); which of them
applies is a phase-3 question and this document takes no position on it.

**What would kill it.** One-seat and three-seat search measuring the same. That would be a genuinely
interesting negative — it would say Fish's public-action structure protects it from the belief
corruption that damages Hanabi, which is plausible because Fish's belief is dominated by hard
certificates rather than by a soft policy model.

### L9 — The leaf evaluator (enabling, conditional; confidence 0.70 that it is the true gate)

The corpus's own condition for re-opening search: "it should be re-opened only after the leaf evaluator
is rebuilt, because the present one is algebraically close to a rescaling of the hit probability and
cannot support a depth-limited search" (`paper/sections_v06/14-conclusion.tex`).

The measured basis is precise. Over 26,417 shipped ask decisions, **7 of 16 value features are exactly
constant across the candidate set at 100.00% of decisions**, carrying 41.5% of the coefficient mass
including the largest single coefficient; of the 9 that vary, 6 vary only through `p`; the mean R² of
the value term on `p` is **0.84034**; and it flips the pre-search argmax at 0.94% of decisions
(`R0` loss channel #5, from R1 `r1_anatomy`). `declareByValue` is algebraically a threshold on `pAlloc`
at 100.0000% verdict agreement over 4,226 evaluations. This is precisely the configuration Brown,
Sandholm & Amos prove unsound for depth-limited search: a single scalar leaf value with no opponent
continuation represented.

**A constraint the corpus has not stated, recorded as a constraint rather than a recommendation.** A
DeepStack/ReBeL-style evaluator whose *input* is a distribution over infostates is structurally
inapplicable to Fish — the public tree is dense and one opening public state carries
54!/(9!)⁶ ≈ 10³⁸ deals. The evaluator families the literature leaves available are those taking public
state plus a discrete continuation-strategy index, those with no leaf evaluator at all, and those
bootstrapping the blueprint's own Q. Which of them phase 3 pursues, if any, is not this document's
call.

**Cheapest experiment.** Fit a value function on public-belief features with the candidate-varying
component explicitly orthogonalised against `p`, and report the residual R² and the argmax-flip rate
against today's 0.94%. If the residual is negligible the gate stays shut and L2/L7 proceed on
truncation and variance reduction alone.

### L12 — The evaluation has no adversary that models the target (instrument; confidence 0.95)

Verified in code, and narrower than the first draft of this document claimed. The three deception
archetypes are `V04Agent`s whose **only** departure is the set of half-suits they will ask in
(`engine/src/probe_deception.hpp:3-8`) — v0.4's belief, ask score, declaration rule and forced endgame
throughout. The scripted baselines **do** carry a within-match model: `LegacyMemory::signals[6][9]` is
incremented on every ask (`engine/src/baselines.hpp:61`) and read by `detective` and `lockout`
(`baselines.hpp:262`, `:173`), and `bluffer` conditions on the immediately preceding ask
(`baselines.hpp:271`).

**What none of them does is model the target's *policy*, best-respond to it, or carry anything across
matches.** They are fixed hand-tuned functions of a public ask tally. The strongest opponent v0.6 has
ever faced is the previous FishBot. So the exploitability of v0.6 against an opponent that models it is
not merely unmeasured — it is unmeasurable with the current opponent set. Threat-model class C6 is the
fix, and classes C4 (learned) and C5 (white-box inversion) are the two the corpus has never built at
all.

---

## 3. The register of closed questions

Imported with citations so later phases do not re-litigate it. Each entry states the resolution **and
the resolution level**, because "closed" here means several different things. Sign convention: where a
paired `deltaFromRef` is quoted it is **reference minus variant**, so a negative number means the
variant is ahead — the convention §4 P-1 establishes.

| # | Question | Resolution | Basis | What it excludes |
|---|---|---|---|---|
| **C1** | **Exchangeable ties** — can any rule beat the array-order tie-break on the majority of decisions that tie bit-for-bit? | **Closed, structurally.** In the v0.6 mirror, 3,613 of 6,715 contested decisions (**53.80% of contested**, 53.2% of all 6,789 ask decisions) tie bit-for-bit; 93.1% of them are two cards of one half-suit at one target; the **exact** posterior separates **0.00%** of them; and array order / the deployed marginal / the shipped pick / the exact posterior all realise the same hit rate (42.181 / 42.181 / 42.236 / 42.181%) | `research/v06/results/E8-ties.txt` (v0.6 mirror block); `paper/sections_v06/06-diagnosis.tex` | A **logical** closure, not a null: no information available to the seat separates them. It does **not** say the tie group is worthless — a clairvoyant tie-break realises **70.77%** in the same block — and it does not bear on whether an ensemble can. See C1′. *(The paper's headline tie figures — 54.74%, 43.81, n = 3,773 — are the **v0.5 mirror reference** row of the same artifact. The incumbent is v0.6; the v0.6 row is the one quoted here.)* |
| **C1′** | …so is the *ensemble* result in the tie group real? | **Open, and it is L2's territory.** In `F8-tiesearch.txt`, one sampled deal inside the tie group is 49.58% and twelve are 54.17% (both n = 720, one bank); across three banks the tie-only search is 51.94 / 50.69 / 50.00 (`F9-tieonly.txt`), mean 50.88 | `F8-tiesearch.txt`, `F9-tieonly.txt` | **The "exactly zero" random-tie control does not survive audit — see §4, P-2.** And on **v0.6's own vector** the tie-only search is 51.53 / 48.47, mean **50.00** — a result generated, macro'd as `\vsixTieSearchLift`, and printed nowhere (§4, P-4) |
| **C2** | **The exact-posterior matched-budget refit** — three studies listed it as the outstanding experiment | **Closed on a predictive criterion.** Scored as predictors on identical states, the exact posterior under a uniform-deal prior is the **worst** of the three inference paths: 1.42246 nats / 47.94% argmax hit, against the deployed approximation's 1.38218 / **51.49%**. The policy prior is the entire difference (no-prior: 1.39083 / 49.99%) | `paper/sections_v06/05-belief.tex`, `sec:belief-exactworse`; `paper/tables_v06/belief.tex`; `E8-belief.txt`, 140,661 card evaluations | **No win-rate measurement exists**, so no half-width and no quarter-point verdict can be stated. The experiment the register specified — a matched-budget refit under `belief=block` followed by a paired head-to-head and a six-style panel (`R0` V6-M9/Q1) — was **not run**. What *was* measured in play is that `belief=block` loses badly (39.83 / 40.33 / 35.83% at three seeds, `R0` §5 Q1) at 14× the cost. The closure is sound as an argument about informativeness and is not a null at any resolution |
| **C3** | **v0.5's chain/threat re-scoring pass** | **Closed as not separated.** Restoring it at the v0.6 vector (`v06:chain2=1`) gives a paired panel `deltaFromRef` of **−0.75 points [−2.44, +0.92]** over six opponents — i.e. the point estimate says the chain pass is **0.75 points better**, unseparated | `research/v06/results/F1-chain2x2.json`; n = 4,800/arm | **Null at ±1.69 points.** Does not exclude a half-point effect, let alone a quarter. The v0.4 study's own estimate was +0.8 ± 0.5. Consequence: `searchTopK`, `chainWeight` and `threatWeight` are **inert coordinates** in the shipped configuration and must not be presented as fitted quantities. *(The corpus's claim that the pass is "about 60% of that policy's runtime" — `RESULTS-SUMMARY.md:251`, `:261` — is inconsistent with R1's profile, where `chooseAsk` is 12.5% of total runtime and the pass is ~90% of that, i.e. ~11%; and with the measured 276 → 303 games/s, a 10% speedup. Flagged, not resolved.)* |
| **C4** | **The three extra v0.6 ask terms** (`wVoid`, `wTeamHas`, `wLastLive`) | **Closed as ~zero.** Zeroing all three gives a paired panel `deltaFromRef` of **−0.46 points [−1.15, +0.23]** — point estimate says the variant is 0.46 better, unseparated. The fit itself drives them to 0.171 / −0.477 / −0.777, all small | `F1-chain2x2.json`; `engine/src/v06.hpp:181-187`; n = 4,800/arm | **Null at ±0.69 points** — the best-resolved paired panel null in the corpus. It **does** exclude a one-point effect and comes close to excluding a half-point one. Also: the paper's `xf=0` ablation is **confounded** — a non-zero extra weight is what sets `extraFeats`, and `extraFeats` selects the v0.6 scoring path, so `xf=0` silently restores the chain pass. F1 exists to give the clean fourth cell |
| **C5** | **PIMC / double-dummy endgame search** | **Closed, structurally, and re-confirmed.** Under perfect information the team on turn takes every half-suit not dealt outright to its opponents (verified 300/300 games, value 8.867–0.133), so every determinization returns "we take everything" and all legal actions score alike | `R0` §3 item 2; `paper/sections_v06/08-search.tex`, `sec:search-notpimc` | This closes **clairvoyant** determinization. It does **not** close the v0.6 search, which determinizes the deal but reconstructs all six continuation players at their own information sets — a strict under-approximation, verified wider in 346 of 346 cases over 103,644 card checks |
| **C6** | **A repetition guard as a termination backstop** | **Closed.** `v05:norepeat=1` scores **42.93%** against shipped v0.5's 49.07% — **−6.13 points**, the largest single-switch effect in the codebase. Cards move; a repeat after a public transfer is frequently the best ask on the board | `R0` §3 item 1 (E5, seed 606060) | n not stated in the register entry; the effect is 4× the half-width of any plausible cell, so the sign is not in doubt |
| **C7** | **Confidence-ranked declaration arbitration; willingness-bit turn transfer; a stronger provability oracle for D13** | **Closed as ~zero.** Arbitration: **+0.37 pp pooled over 30,000 games**, range +0.28 to +0.45 across five seeds (the frequently-quoted **+0.30 pp is seed 31 alone at 6,000 games**, and the verification note's own headline is that it is the low draw). Turn transfer: multi-candidate transfer arises 0.148 times/game and a ground-truth chooser is worth −0.0007 ± 0.0024 sets/game over 6,000 paired games. Oracle: 0.34% → 0.544% | `research/v05/results/P6-verify-arbitration-cost.md` §§3–4; `P8-coordination.md` §1.4; `R0` §3 item 5 | The turn-transfer number is well powered and excludes a quarter-point effect. The arbitration number is resolved in sign at 30,000 games. The oracle number has no interval |
| **C8** | **A positive holding cost as the stopping fix** | **Closed, with a diagnosis.** −3.9 points when swept; and the literature remedy does not apply because in the v0.4/v0.5 stopping model waiting is free *by construction* — `V(wait at t) ≡ V(wait at t+1)` is an implementation identity (`v05.hpp:795`, all-zero deltas; `value()` has no time input) | `R0` §3 item 6, quoting DESIGN.md §0.3 | n not stated. The register's own instruction — quoted, not endorsed — is "fix the identity, not the cost"; the identity is still in the shipped code |
| **C9** | **Learned / neural belief models** | **Closed.** Belief is a solved problem in Fish: `blockdp.hpp` computes Z, marginals, P(team owns half-suit) and P(named allocation) exactly in closed form, brute-force validated at 0.000e+00 error over 78,516 checks | `R0` §3 item 7 | Correct — with the caveat that "exact" covers the *logical* constraint set, not the *policy-conditioned* likelihood, which is exact only under an assumed opponent model and is precisely what a deceptive opponent attacks. Do not let one word cover both |
| **C10** | **Team belief DAG / TB-DAG subgame solving** | **Closed.** Its polynomial-time escape is conditional on blueprint sparsity, and a Fish blueprint is ~69 actions over ~96 decisions — not sparse | `R0` §3 item 11; `research/v06/notes/R7-literature-search.md` §5.2 | Sharpened by this review: the deeper reason is that Fish's team meta-player fails **A-loss recall**, and it fails it for exactly one reason — each seat privately observes nine cards its teammates do not. Fish would be polynomial-time solvable for TMECor if the deal were public *within* a team (Kaneko & Kline 1995; Zhang & Sandholm AAAI 2022; Cacciamani et al. Thm 2). That is a cleaner statement of the blocker than "10²⁸" and it costs nothing to adopt |
| **C11** | **`priorPhi` as an independent parameter** | **Closed, algebraically.** Provably absorbed by Sinkhorn's column normalisation; verified at max 1.291e-04 on 900 real states | `R0` §3 item 9 | An algebraic closure, not a null. The register's own follow-up instruction (quoted) is to collapse to one `θ_eff` and return the coordinate to the optimiser |
| **C12** | **`patientLocked` / `lockedAllocThresh` as tunable knobs** | **Closed.** Unreachable under the shipped `useValue && valueDeclare` branch; `v05:patient=0` is **bit-identical** to `v05` | `R0` §3 item 10 | Exact. Any ablation on them measures nothing. Two of v0.6's 37 fitted coordinates are therefore inert *in addition* to the three named in C3 |
| **C13** | **Deleting the value function outright** | **Closed.** It is a 1.79% rescaling of `p` but still earns **+1.9 points** as a tie-breaker on the top-K input set | `R0` §3 item 12 | n and interval not stated in the register entry. The register flags that the `value=0` and `topk=0` ablations were measured separately and may not compose. Its own instruction — quoted — is to replace rather than remove it (L9) |
| **C14** | **Scoreboard-selected configurations** | **Closed as a methodology.** M8-alone scores 56.60% against v0.4 while carrying 44.83% dead asks, a 373-ask dead run, and 14% of games killed by the action limit | `R0` §3 item 3 | Win rate and soundness are dissociable in this family. **The commit gate runs before any strength number, always** |

---

## 4. Provenance and correctness defects, and who found them first

Ordered by how much each changes a reading. Where a defect was already recorded by the v0.6 cycle's own
adversarial passes (V1, V2), that is stated — this ledger's contribution to those entries is the
re-measurement or the consequence, not the discovery.

**P-1 — `tab:ablations` prints every sign inverted. (New here.)**
`engine/build_tables_v06.py:217` negates `deltaFromRef` for `ablations.tex`; `:813` does not for
`fourway.tex`; both carry the header `\vsix{} $-$ variant`, and `deltaFromRef` is already
reference-minus-variant (`engine/src/arena.hpp:140-158`, checked against `E5-ablations.json`'s own
arithmetic). As printed, the table says v0.6 **loses** to v0.5 by 2.69 points on the panel —
contradicting the paired-panel table in the immediately preceding subsection — and it says the search
variant is 1.35 points **worse** when the artifact says 1.35 points better. **This is L2.** *The
+1.35 reading of the artifact was already published at
`research/v06/notes/V1-adversarial-verification.md:317-320`; the rendering bug is what is new.*

**P-2 — the paper's random-tie negative control is weaker than it reads. (New here; partially
superseded.)** `research/v06/results/F8-tiesearch.txt` row D reports
"50%  [47.1756, 52.8244]  n=1200, mean sets 4.5 - 4.5". The win rate is exactly 600/1200 and the mean
half-suits are exactly 4.500–4.500 — the signature of a cell with no variance. **The artifact records
no spec**, so this cannot be confirmed from the repository: it is consistent with a mirror, and it is
consistent with a genuine measurement that happened to land on the mode. (The reported interval is
*not* independent evidence: `wilson()` is a deterministic function of (k, n)
(`engine/src/arena.hpp:38-45`), so any cell at 50% and n = 1200 prints that interval. The first draft
of this ledger counted it as a third fact; it is not.)

What can be said without the spec: a **non-mirror, better-powered** measurement of the same switch
exists and gives a different answer. `F1-chain2x2.json`'s `v06:rtie=1` row, paired over six opponents
at n = 4,800/arm, gives `deltaFromRef` = **−0.69 points [−2.54, +1.13]** — point estimate: randomising
the tie group is **0.69 points better** than v0.6, unseparated at ±1.8. That row *is* in the paper
(`paper/tables_v06/fourway.tex`, `\input` at `paper/sections_v06/D-ablations.tex:85`, read at `:91-92`),
and the appendix summarises it as "worth nothing too". "Not separated at ±1.8 points" and "worth
exactly nothing" license very different conclusions, and the abstract uses the strong form
(`paper/sections_v06/abstract.tex:23`; `research/v06/RESULTS-SUMMARY.md:106`). **Fix: print the spec
into F8 and re-run. Until then, L4 is not rebutted.**

**P-3 — the exploitability responder is not in the target's class, and is itself degraded. (New here.)**
See `docs/v07/THREAT-MODEL.md` §7 R-1 for the full statement: `BASE=v05` on all three probe rows
(`research/v06/runs/lbr.log`), so a 34-coordinate `V05Agent` running the chain pass was fitted against a
37-coordinate `V06Agent` that does not — the fitted responder's coordinates 31–33 confirm the pass is
live in it; and the v0.6-targeting responder's own `declAcc` falls 2.76 points to 0.9550 with
`forcedPerGame` roughly seven times normal (`research/v06/results/X1-lbr.jsonl`). The 48.36% is at least
partly a measurement of a broken exploiter.

**P-4 — a result that contradicts the search attribution is generated but never reported. (New here.)**
`\vsixTieSearchLift` = **50.00** over two banks — the tie-only search run on **v0.6's own vector against
v0.6** (`F9-tieonly.txt`: 51.53% and 48.47%, i.e. 371/720 and 349/720, exact complements) — is defined
at `paper/numbers_v06_generated.tex:559` (and inlined at `fishbot_v06_standalone.tex:1391`) and appears
in **no section of the paper**. §8 reports the tie-only search on *v0.5's* vector (51.94 / 50.69 / 50.00)
and reports the *unrestricted* search on v0.6's own vector (52.08%) as the headline. The one cell that
tests the tie-group hypothesis on the shipped vector is null and is not printed. Not misconduct — the
paper is explicit that the attribution is unresolved — but it is the most decision-relevant cell for L2
and it is missing.

**P-5 — F6–F9 have no generating script, and two of their cells disagree. (V2, SF-14 and SF-3.)**
V2 already records that `F6-reconstruction.txt`, `F7-tieguard.txt`, `F7-winrate.txt`,
`F8-tiesearch.txt` and `F9-tieonly.txt` supply ten paper macros — "including both negative controls
that make the search result a claim about information" — and that no committed script produces them
(`research/v06/notes/V2-final-audit.md`, SF-14). It also records that the same configuration reports
**54.17%** in F8 §A and **51.94%** in F9 bank 90210 at the same n = 720, a 2.2-point gap that cannot be
explained from the tree because neither artifact records its spec (SF-3). Both are still open at
`60fee17`. Additionally: `F8-tiesearch.txt` row C ("32 determinizations, tie group only") has a heading
and **no result**.

**P-6 — four macro mis-bindings, one of which typesets nonsense. (New here for three; `\vsixGateStarved`
overlaps V2's B-3 family.)** `\vsixGateStarved` = 31,321,233 is bound to a count of
`(opportunity, half-suit)` pairs and is typeset as a **percentage** at `09-fitting.tex:132` ("starved
turns 31,321,233%"; the real rate is 0.0117%). `\vsixFitBanks` = 40 is bound to a *generation count* and
typeset as "40 are fitting banks" at `10-protocol.tex:184` (there are two fitting seeds).
`\vsixMirrorDuplication` = 2 is typeset as "the effective sample is 2% of the nominal one" at
`10-protocol.tex:42` (for a mirror win-rate cell the effective sample is zero; for rate denominators it
is halved). `\vsixGateN` = 1 is typeset as "The thresholds are 1 in number" immediately before six are
listed.

**P-7 — the shipped vector was never selected on a held-out bank, and the selection among fits is
unrecorded. (V1/V2, N-3; extended here.)** `V6PARAMS` is byte-for-byte the final `weights=` record of
`research/v06/runs/fitC.jsonl` (`engine/src/v06.hpp:181-187`; verified against `fitC.log`).
`engine/select_final.py` — the script whose entire purpose is to re-select on a validation bank the fit
never saw — is v0.4-era (it emits `v04:allparams` and writes to `research/v04/runs/selected.json`) and
there is no v0.6 equivalent and no `selected.json` under `research/v06/`. Meanwhile
`research/v06/runs/fitB.log`, `fitD.log` and `searchtest.json` are **zero bytes**, and nothing records
why fitC was preferred. The fitting section correctly describes the winner's curse and asserts that
every number in §11 comes from a bank the fit never touched — true of the *deals*, but if the choice
among fitA–fitD used any evaluation bank, those banks are model-selection data. V2 raised this
(`V2-final-audit.md`, N-3) and it is still unanswered.

**P-8 — the dialect-robustness claim covers less than it appears to, and its one broad row is a bundle.
(New here.)** `paper/tables_v06/dialects.tex` has four rows at n = 1,500 each (±2.53):
default, `--no-out-of-turn`, `--no-cardless-declare`, `--legacy`. The `--legacy` flag changes **four
things at once** — out-of-turn declaration off, cardless declaration off, `maxAsks` 400 → 360, and the
willingness ladder from eight rungs to two (`{0.38, −1.0}`) — so it does perturb the ladder and the
action cap, but attributes nothing. Genuinely unperturbed by any row: the action-cap **adjudication
rule** (physical majority), the arbitration order (`--arb` exists and is not swept), and the deck size
(`--sets` exists and is not swept).

**P-9 — the search ladder, the deviation-rate probe and their timings are all measured on v0.5's vector.
(V1, `V1-adversarial-verification.md:314-316`.)** Every row of `E12-search.jsonl` and every block of
`E14-searchdev.txt` runs `v06:legacy=1` (`engine/experiments_v06.sh:92`, `:108`). The consequence this
ledger adds: **the paper's cost claim for the search, and the 33.49% deviation rate its falsifier
section turns on, are properties of a policy the project does not ship.** The shipped-vector timings
exist only in `F0-search-confirm.jsonl` and are 300–420× rather than the paper's implied ratio (§0.1).

---

## 5. What phase 1 should measure first, in order

Not a plan — a reading of this ledger's own priorities, so that phase 1 does not have to re-derive it.

1. **Restore or abandon the stash (§0.4).** The incumbent's headline number cannot live in `stash@{0}`.
2. **Fix P-1 and re-run the E5 ablation on a second bank, and time F-mid on the shipped vector (L2).**
   Under two hours, and it may change what v0.7 is trying to beat.
3. **Run the L1 replay.** Zero games, and it converts the second-largest entry from an estimate into a
   measured ceiling — including the fraction of errors that no belief-based rule can fix.
4. **Run the L4 bit measurement.** Zero games, and it kills or confirms the determinism hypothesis
   before anyone builds a class-C5 responder.
5. **Re-run the partner table at 18,000/cell (L6)** — 144,000 games, about eight minutes — and print the
   specs into F7/F8/F9 (P-5).
6. **Measure the grounding budget (threat-model S1, ceiling on L11).** One paired cell plus a posterior
   variant.
7. Only then build the responders, the T10 stream plumbing, and the throughput path the phase-1 exit
   criterion names.

Items 3 and 4 need no simulator time at all. Item 2 needs about two hours. Everything in this document
that could change the shape of v0.7 is measurable in a day.
