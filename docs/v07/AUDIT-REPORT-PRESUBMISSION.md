# FishBot v0.7 — Final Adversarial Audit Report

Fourteen auditors, one skeptic per finding, then this pass. Below is one merged list, most severe first. Duplicates filed by different auditors against the same sentence are merged and renumbered. Two findings not filed by any auditor (§1 second location, §4) were flagged in auditors' "could not break" notes and never adjudicated; I verified them myself and they are included with that noted. Nothing has been reinstated against a skeptic's refutation; where I sided with one skeptic over another on adjacent filings, I say so.

**Counts: 1 BLOCKING, 19 SERIOUS, 14 MINOR, 6 UNRESOLVED, 0 UNDERSTATEMENT.**

---

## BLOCKING

### 1. The abstract and the component table say the deployed agent performs exact deal inference. It does not, and the paper's own body says so four times. — BLOCKING

**Location**: `paper/sections_v07/abstract.tex:4` and `paper/tables_v07/components.tex:5` (Table 2, rendered PDF p.17).

> "We develop and evaluate FishBot v0.7, the strongest configuration produced in this project's FishBot lineage. **It combines exact observer-conditioned deal inference with a fitted policy prior** and a linear ask and declaration policy, a public-history tie-breaking rule…, a half-suit contestation weighting, a deduction-state stall detector…, and a guarded determinized test-time search."

> Table 2, row 1: "Deal inference | **exact posterior with fitted policy prior** | unchanged | §5"

**The claim**: that the deployed FishBot v0.7 configuration performs exact observer-conditioned deal inference, with a fitted policy prior on top of it. Every other item in the abstract's list is a deployed component, and the search is listed separately, so item 1 cannot be read as the search's exact sampler. Table 2 makes the same claim in the column headed "FishBot v0.7".

**What the source shows**: the deployed agent never constructs the exact posterior.
- `engine/fishbot_v07.json` `options` keys are `r12, rtie, pool, oppfloor, force, askfloor, stall, s1, det, cand, kappa, rbelief, depth, maxq` — no `belief=` key. `rbelief=indep` sets the *rollout* belief (`engine/src/factory.hpp:247` → `a->x.rollBelief`); `cfg.belief` is written only at `factory.hpp:44-47` and `:377-380`, both keyed on `belief`. So `cfg.belief` keeps `V05Config`'s `BeliefMode::Fast` (`engine/src/v05.hpp:48`).
- A probe built through the engine's own factory on the frozen spec prints `cfg.belief = 5 (Fast)`, `sinkOuter=4 sinkInner=8 priorTheta=0.37062 priorPhi=0.14525`.
- The `Fast` branch calls `bel.sinkhornDisj(k, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi)` and sets `bel.dpOk = false` (`engine/src/v04.hpp:186-190`, `v05.hpp:307-311`); `evaluateSet`'s Fast branch returns before the exact branch (`v05.hpp:1058-1082` vs `:1085-1092`). `grep -rn ensureExactMarginals engine/src/` returns one hit — the definition at `v06.hpp:353` — and **zero call sites**.
- The paper says this itself, four times: `05-inference.tex:200` ("The path the agent deploys adds two such parameters and **gives up exactness** for them. It replaces the count join by iterative proportional fitting"); `06-agent.tex:36-38`, the Fig. 1 caption ("**The deployed path does not construct it**: it runs a cheaper approximate fit"); `06-agent.tex:233` ("The exact object… **sits on no decision path**"); `04-background.tex:71` ("**Exactness and behaviour-awareness are not available in the same object here**").
- Table 2's v0.6 column is wrong for the same reason: `BeliefMode::Fast` is `V05Config`'s initialiser and no v0.6 spec sets `belief=`, so "unchanged" is the only correct cell in that row.

This is documented failure mode (a) in the two places a reader meets the system first, and it asserts the property the paper's own signature negative result (§4.3: the exact posterior is the *worst* predictor row) says was deliberately discarded.

**The narrowest fix**: in `abstract.tex:4-5`, describe the inference as `05-inference.tex:200-224` and the Fig. 1 caption already do — an approximate iterative (Sinkhorn/IPF) fit to the observer-conditioned deal posterior, initialised from a two-parameter fitted policy prior. In `components.tex:5`, replace "exact posterior with fitted policy prior" with the same, in both the v0.6 and v0.7 columns. Three over-corrections to avoid, each contradicted by a source: (a) do **not** write that the agent performs no exact inference — `engine/src/v06.hpp:685-700` shows the deployed search's determinization sampler *is* exact (uniform `DealDP` over C1–C4, C5 by rejection) with the policy prior as an importance weight, correctly documented at `06-agent.tex:337-343`; (b) do **not** drop the policy prior from the inference item — it is the initialiser of the deployed fit, not an add-on; (c) do **not** touch §5's title or introduction contribution 2 (`01-introduction.tex:74`, "An exact observer-conditioned deal inference **method**") or the abstract's first sentence ("the posterior over that deal can be computed exactly") — both are claims about the method and the game, and both are correct.

---

## SERIOUS

### 2. §2.3's throughput sentence is wrong on both halves, and the same paragraph's "within a day" contradicts §11. — SERIOUS

**Location**: `paper/sections_v07/02-game.tex:112-117` (PDF p.7).

> "A deal resolves in roughly a hundred public events, and the engine plays **hundreds of complete six-player games per second per core with exact inference running in every seat**. […] the adversarial searches **all run on one desktop within a day**."

**The claim**: the throughput underwriting the paper's "the domain is cheap" argument is hundreds of games/s/core, achieved with exact inference in all six seats; and the whole programme fits in a day.

**What the source shows**: measured on this machine with the shipped binary, `./fish7 match --a=X --b=X --games=N --rotations=2 --threads=1 --json` (per-core, `arena.hpp:30-31`): FROZEN v0.7 spec **5.82** games/s; v0.6 blueprint **30.99**; v0.5 27.77; `v06:belief=exact` **1.99**; `v06:belief=exactdisj` 1.49. The only configurations reaching hundreds per core carry *no* inference (v03 571.6, v02 890.7, random 2179.2) or the cheapest one (`v05:belief=indep` 132.5). `research/v07/results/T1-throughput.jsonl` (mirror rows, `gamesPerSecOne`) agrees: v07 31.18, v06 31.00, F-cheap 9.93. Every corpus measurement of the exact path is 1.5–2.2 games/s/core (`research/v06/notes/R0-OPPORTUNITY-REGISTER.md:562`, 21.7 g/s at 15 threads, "14× slower"; `docs/v07/SUBOPTIMALITY-LEDGER.md:260`). The two halves are mutually unsatisfiable by 150×–1500×. The paper also contradicts itself: `11-results.tex:502-506` prices the frozen configuration at ≥4.52× the blueprint.

On the day: summing `match.seconds` over all 428 scored cells of `P5-B{2,3,4eval,5,6,7,8,9}.jsonl` gives **24.12 hours** — before the 188,160 B4 and 94,080 B9 fitting games — and `11-results.tex:513` says the battery "ran over **two days**".

`eventsPerGame` is 93.4–101 throughout, so "roughly a hundred public events" is correct.

**The narrowest fix**: strike "per core" and "exact". A statement the sources carry: the engine plays a six-seat mirror of the blueprint policy at 356.4 games/s on a fifteen-thread desktop **with the deployed belief in every seat** (`T1-throughput.jsonl`, `spec:"v07"`, mirror), and the evaluated configuration's search costs at least 4.52× that. Do **not** substitute "roughly 33 games per second" — that is the v0.6 blueprint's figure and would attach a v0.6 number to a v0.7 sentence (failure mode (c)); the frozen configuration's own figure is 5.8/core. Change "within a day" to match `11-results.tex:513`.

---

### 3. §2.1 states a declaration rule the engine does not have, and the frozen agent breaks it on one declaration in five. — SERIOUS

**Location**: `paper/sections_v07/02-game.tex:30-31` (PDF p.5), inside §2.1 "Rules", which opens "This section states the game completely, so that nothing later depends on outside material."

> "A player may not declare a half-suit they hold entirely, so the interesting case is always a claim about cards in teammates' hands."

**The claim**: the evaluated dialect forbids declaring a wholly-held half-suit, hence every declaration is a claim about teammates' cards.

**What the source shows**: `declarationRound` (`engine/src/game.hpp:404-432`) filters a voluntary declaration on exactly the half-suit being active (`:421`), the agent proposing it (`:420`), every named owner being a teammate (`:423-425`), and the two dialect toggles (`:418-419`). There is no cardinality condition. `struct Rules` (`engine/src/fish.hpp:107-128`) has no such field and `rulesFrom` (`engine/src/main.cpp:52-68`) exposes no flag. `game.hpp:530-540` goes further: when `enumerateAsks` returns zero legal asks the engine *compels* a declaration of a wholly-held set. A skeptic's probe built on `factory.hpp::makeAgent` with the frozen spec, mirror, default `Rules`: **361 of 1,783 (20.2%)** of FishBot v0.7's voluntary declarations name the declarer for all six cards and score correct; v0.6, 563 of 3,594 (15.7%). Both clauses of the sentence are false of the evaluated game. `A-dialect.tex` lists no clause or switch for it, and `paper/sections_v06/02-rules.tex:1-16` did not state it — it was newly promoted into the v0.7 rules section from a strategy observation attributed to Develin at `03-related.tex:15-16`.

**The narrowest fix**: delete the whole sentence — both clauses. What §2.1 may say about declaration legality is what `game.hpp:418-425` shows: a voluntary declaration is constrained by the half-suit being active, every named owner being a teammate, and the two dialect toggles; and a player with no legal ask is compelled to declare a half-suit it holds entirely (`game.hpp:530-540`). Do **not** dispose of the clause by leaving it in §3.1 as filed: `03-related.tex:14-16` says Develin's chapter "describes the same dialect" and supplies "two observations this design uses", so §3.1 must be narrowed in the same pass or it will assert that the studied dialect matches a rule it does not contain.

---

### 4. Table 2 names the contestation coordinate after a mechanism the artifacts record as refuted. — SERIOUS

**Location**: `paper/tables_v07/components.tex:8` (Table 2, rendered PDF p.17, printed before §6.3).

> "Half-suit contestation | absent | **information-denial weight** (`r12`) | §6.3"

**The claim**: `r12`'s identity, in the table that introduces the agent's components, is an information-denial weight.

**What the source shows**: `docs/v07/ADVERSARIES.md` §4A is titled "not the mechanism it was built for", and the paper's own §6.3 says so in bold 130 lines after the table (`06-agent.tex:186-191`): "The feature was designed to price *information denial*… the intent was that the fitted coefficient would come out negative… It came out positive. The negative branch was swept separately and is flat to negative throughout… so the design intent is **refuted rather than merely unexplored**." `06-agent.tex:193-201` gives the measured mechanism instead — the target's ask accuracy falls, its declaration accuracy *rises*, "which rules out the declaration channel", and the acting team pays part of the same cost. This is the same phrase, and the same failure mode (a), that a prior pass removed from the abstract; it survived in the table because the table was not swept.

**The narrowest fix**: replace "information-denial weight" with the measured reading §6.3 supports and the row's own first column already uses — e.g. "contested-half-suit weight (`r12`)". Do not add a mechanism claim to the cell; §6.3 carries the account. No other file changes.

---

### 5. "The policy prior accounts for the entire difference" is refuted by the table three lines above it, whose middle row is also mislabelled. — SERIOUS

**Location**: `paper/sections_v07/04-background.tex:65` and `:71` (PDF p.11-12).

> Row 2: "**Rules posterior, no policy prior** | 1.39083 | 49.99%"
> ":71 — The exact object is the worst row. **The policy prior accounts for the entire difference**: exactness in this game is purchased by discarding behaviour…"

**The claim**: the whole predictive gap between the exact posterior (1.42246 / 47.94%) and the deployed approximation (1.38218 / 51.49%) is the policy prior; and row 2 is the posterior with the policy prior removed.

**What the source shows**: row 2 is `sinkhorn th=0.000 ph=0.122` (`research/v06/results/E8-belief.txt`) — θ=0 only, with φ at its fitted 0.122. `paper/numbers_v06_generated.tex:302-305` labels both macros "% E8-belief.txt **theta=0** row". The paper's own notation table (`05-inference.tex:40`) defines θ *and* φ as "the two policy-prior parameters", so θ=0 is half the prior, not none of it. The genuinely prior-free row exists: `research/v06/results/F2-belief-noprior.txt`, `sinkhorn th=0.000 ph=0.000` = **1.39339 / 49.14% — still ahead of the exact law on both measures**. Decomposition from the two files: total gap 0.04028 nats / 3.55 pp; prior deleted outright accounts for only 0.01121 nats (27.8%) and 2.35 pp (66.2%); the remaining 0.02907 nats is approximation error with no behavioural content at all. The v0.6 paper states it correctly and narrowly (`paper/sections_v06/05-belief.tex:264-267`: "with the prior deleted outright (θ = φ = 0…) it scores 1.39339 and 49.14% — still ahead of the exact law on both measures… the gap between the **second and third** rows is the policy prior alone"). Note also that row 3, labelled "Deployed approximation", is `th=0.445 ph=0.122` under a header reading `A=v05 B=v05` — v0.5's prior in a v0.5 mirror, where FishBot v0.7 deploys θ=0.37062, φ=0.14525.

**The narrowest fix**: two coupled edits. At `:65`, relabel the row as `sections_v06/05-belief.tex:262-263` does — "Approximation, certificate half of the prior deleted (θ = 0)". At `:71`, replace "accounts for the entire difference" with what the artifacts show: the gap between the second and third rows is the policy prior alone, and the exact object stays worst even with the prior deleted outright (θ = φ = 0: 1.39339 / 49.14%, `F2-belief-noprior.txt`). Keep the following clause about the block construction having no behavioural parameter — it is true and is the paragraph's point. Optionally qualify row 3 as the v0.5 prior in the v0.5 mirror, matching the care line 48 already takes. The paragraph's methodological conclusion is unaffected and in fact strengthened.

---

### 6. §9.3 says the whole of the cycle's gain originates in one coordinate; §11.4 says that statement is not supported. — SERIOUS

**Location**: `paper/sections_v07/09-development.tex:72-74` (PDF p.30).

> "The second of those is the contestation weight of §6.3, and **it is where this cycle's entire strength gain originates**."

**The claim**: the whole of v0.7's strength gain is attributable to `r12=25`.

**What the source shows**: `11-results.tex:248-251` — "**The stronger statement, that this coordinate is where the gain lives, is not supported at this power**: the drop's lower bound is +0.80, and §11.2.2 reports the protocol's own location test failing to replicate." `12-discussion.tex:21-24` repeats the disclaimer and words the claim as "the largest of the five". Recomputed from `research/v07/results/P5-B5.jsonl` (pooled edge = mean of banks, half-width = hypot/2, drops in quadrature): REF +4.7291 [+4.10, +5.36]; leave-one-out drops r12 **+1.6874 [+0.80, +2.57]** (36% of the whole), rtie **+1.0833 [+0.20, +1.97] — interval excludes zero**, urgoff +0.6375, search +0.5667, stall 0.0000; add-one-in urgoff **+1.3750 [+1.12, +1.63]**, also excluding zero. The paper's own attribution model is five contributors summing to +6.18 against a whole of +4.73. It is contradicted 40 words later in its own subsection: `09-development.tex:76-80` says the urgency channel "is worth +1.23 to +1.62 pp to a configuration that simply disables it".

**The narrowest fix**: delete the totality clause and cross-reference the attribution section rather than importing holdout figures into §9 (whose preamble declares all its measurements training material). E.g. "…the contestation weight of §6.3; §11.4 measures on holdout what it contributes, and finds it the largest of the five components FishBot v0.7 carries rather than the whole of the gain." "The largest of the five" is a point-estimate ordering (the r12 and rtie intervals overlap) and is the phrase `12-discussion.tex:22-23` already uses. The attribution section is §11.4, not §11.2.4.

---

### 7. The measurement offered as discharging the inherited leaf-evaluator conditional is the measurement that left it binding. — SERIOUS

**Location**: `paper/sections_v07/09-development.tex:144-146` (PDF p.32).

> "The inherited hypothesis was discharged separately and by re-measurement rather than by the new evaluator. The single underpowered cell on which it rested, one 4,000-game cell at +0.08 pp, **re-runs at +2.19 pp replicated in the endgame regime** using v0.6's own leaf."

**The claim**: the +0.08 cell was re-run and came back at +2.19, which is what discharged the conditional.

**What the source shows**: the two figures are different rows of one phase-1 table. `docs/v07/INSTRUMENT.md:461` — the +0.08 cell is `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24`, one bank, n=4,000, 50.08% [48.52, 51.62]. `INSTRUMENT.md:455-456` — the +2.19 is `…,depth=12,maxq=26`, two banks, n=12,000 each. Different depth, different `maxq`, different power. The endgame re-run of the +0.08 cell's *own* configuration exists and is not +2.19: `INSTRUMENT.md:453-454`, `depth=24,maxq=26` = **+1.57**. And the corpus records +2.19 as the measurement that left the conditional **binding**: `INSTRUMENT.md:26` (I4) — "supports one at `depth=12, maxq=26` to +2.19 replicated. **The conditional still binds for full-game search (+0.08 without `maxq`)**"; `CANDIDATES.md:185-187` inherits it as the premise. The actual discharge is phase 3's, `docs/v07/CANDIDATES.md:220-225`, headed "What the candidate did discharge, by re-measurement": "Re-measured on the training banks, `depth=12` with no `maxq` is **+1.78 [+0.42, +3.16]** and **+1.25 [−0.30, +2.78]**, pooled **+1.52**, with v0.6's own leaf; the corpus's exact `depth=24` configuration re-runs at **+1.02 [−0.55, +2.62]**." The macro provenance shows the splice: `+2.19` and `+0.08` are filed under the phase-1 INSTRUMENT.md block (`numbers_v07.tex:31-32`) while the `4,000` is filed under the phase-3 CANDIDATES.md block (`:160`).

**The narrowest fix**: keep the first sentence (it matches ledger C12) and replace the second's evidence with `CANDIDATES.md:220-225`'s figures — full-game re-measurement at +1.78 and +1.25, pooled +1.52, with the exact `depth=24` configuration at +1.02 [−0.55, +2.62]. Two guards: do **not** reuse `\vsevenInversionUnfitted`, which also expands to +1.52 but is a phase-2 adversary-inversion figure sourced to INSTRUMENT.md and used at `:37`; new macros must be transcribed under the existing CANDIDATES.md provenance block. Do **not** substitute +1.57 as "the endgame re-run" — it is endgame-restricted and so is not the discharge either. If `\vsevenTruncatedEndgame` (+2.19) is kept in the sentence, describe it as `INSTRUMENT.md:26` does: phase 1's endgame-regime refutation, the measurement that left the conditional binding for full-game search.

---

### 8. "The strongest single arm anyone constructed against the deployed policy" is false, and the paper refutes it one page later. — SERIOUS

**Location**: `paper/sections_v07/09-development.tex:37` (PDF p.30).

> "Converting those bits into play is worth +1.52 pp unfitted, **the strongest single arm anyone constructed against the deployed policy**."

**The claim**: the white-box transcript-inversion arm is the strongest arm ever built against the deployed v0.6 policy.

**What the source shows**: `docs/v07/ADVERSARIES.md:885-905`, the phase-2 table headed "cluster | best arm | edge vs `v06`", contains **eight** arms above +1.52: contestation×m2×search +4.40, contestation×search +3.99, cross-inclass-search +3.58, contestation×m2 +3.17, cross-inclass-whitebox +2.87, contestation `v07:r12=25` **+2.71 [+2.27, +3.15]**, m2×search +2.48, search-strength +1.89. `ADVERSARIES.md:42` calls the +2.71 configuration "**One arm** clears the bar against the deployed policy… a single hand-set coordinate… **with no fit at all**" — so "unfitted" does not save the superlative and the corpus uses the same noun. The intervals do not overlap: +1.52 [+0.92, +2.13] vs +2.71 [+2.27, +3.15]. The paper prints both +2.71 and +1.89 against "the deployed policy" one page later at `09-development.tex:69-71`. `INSTRUMENT.md:28` (I6) is a phase-1 statement; the paper restated it without its bound.

**The narrowest fix**: the filed fix ("the strongest single arm phase 1 constructed") is **also false** — `INSTRUMENT.md:27` and `:597` record the phase-1 ladder as +0.76 → +1.05 → +1.52 → **+1.86 (C3)** against the same incumbent. Either delete the appositive, leaving "Converting those bits into play is worth +1.52 pp unfitted", or use the doubly-bounded form the sources support: the strongest **unfitted** arm the instrument's responder classes produced, noting that phase 1's fitted C3 responder reached +1.86 and §9.3 later constructed an unfitted arm at +2.71.

---

### 9. The learned candidate's purpose and restriction are back-formed, and its primary deployment result (−1.83) appears nowhere in the paper. — SERIOUS

**Location**: `paper/sections_v07/09-development.tex:173-180` (PDF p.33).

> "…the open question was whether anything can be selected within that set rather than merely randomised. **A learned re-ranker restricted to the tied set was built for the purpose.** Against the deployed policy it scores +1.19 pp, indistinguishable from the free public-history tie-break's +1.14 pp…"

**The claim**: the learned component was built to answer the tie-group question, was restricted to the tied set by design, and its only deployment result is +1.19.

**What the source shows**: the restriction is a deployment switch on an already-fitted model, not a build property. `research/v07/results/K5-vs-v06-{7030001,7030002}.jsonl` and `K5tie-vs-v06-*.jsonl` carry the **same fitted weight vector, character for character** (`v07l:cand=4,lmaxq=26,lw=0.02225374357+0:…`); the only textual difference is the added switch `ltie=1`. Re-pooled: unrestricted **−1.8333 [−2.2193, −1.4473]**, banks −1.8583 / −1.8083; tie-restricted +1.1875 [+0.82, +1.56]; head-to-head vs `rtie` −0.0146 [−0.45, +0.43]. The build's purpose is on record: `docs/v07/PHASE-PROMPTS.md:202` (brief item (e), the wildcard); `docs/v07/CANDIDATES.md:489-495` — class C4, "the affordable learned object with a measured target is **amortisation of test-time search** — train a function of decision-time information to reproduce what the search decides"; fitted on "88,502 **searched** decisions / 425,536 candidate rows" (`:499-500`). The ordering is explicit in both narratives (`CANDIDATES.md:537-541`, `RESEARCH-LOG.md:1711-1720`): unrestricted deployment first, then "Deployed inside the tie group only… which looked live for about twenty minutes, until the control", then "This closes ledger C1'" — a by-product. `SUBOPTIMALITY-LEDGER.md:675` assigns C1' to L2, not to this candidate. `grep -rn "1.83" paper/sections_v07/` finds no macro for the unrestricted result.

**The narrowest fix**: state the provenance the sources give — built as the brief's wildcard, class C4, targeting amortisation of test-time search; fitted unrestricted over 88,502 searched decisions; deployed unrestricted it scores −1.83 pp pooled over 48,000 games, negative on both banks, interval [−2.22, −1.45]; the tie-group restriction is one added switch applied after that, and it is there that +1.19 is measured; closing C1' is a by-product. Keep the existing +1.19 / +1.14 / −0.01 ± 0.45 sentence — those reproduce exactly. Do **not** import CANDIDATES.md's "clearing the floor in the wrong direction": the interval's bound nearest zero is 1.447, *below* the 1.53 floor, and `09-development.tex:6-9` asserts that every pooled interval in §9 has a bound below the floor. Consequential: `12-discussion.tex:56` and `13-conclusion.tex:27` name the rejected candidate "a learned tie-break", which is its salvage configuration rather than the object that was built and rejected.

---

### 10. The forced-endgame ladder's "inoperative" argument is a v0.4 measurement, and its own source names v0.7's defining change as what breaks it. — SERIOUS

**Location**: `paper/sections_v07/A-dialect.tex:38-43` (PDF p.57).

> "Restarting the forced-endgame ladder sharpens nothing. … It is sound and inoperative: on every occasion a team went cardless with live half-suits, **566 of them at seed 31, exactly 1 half-suit was live**, so there is no ordering to exploit."

**The claim**: stated in the present tense in the v0.7 appendix, that the ladder's ordering rationale is inoperative because a multi-set forced endgame never occurs.

**What the source shows**: the sample is **v0.4 mirror self-play** — `research/v05/results/P2-forced-endgame.md` §5, run `--a=v04 --b=v04 --games=300 --rotations=2 --seed=31`, concluding "**Every** forced endgame in **v0.4 play** has exactly one live half-suit." The universal was refuted at a second seed and the correction is on record: `P2-verify-forced-endgame.md:156-161` — "At seed 1234567 I observe 118 forced declarations with 1 live half-suit and **2 with 2 live half-suits**. The claim is right as a strong tendency (~98%) and **wrong as a universal**", filed at `C1-v04-corrections.md:1255`. And the source names v0.7's change as the breaker: `P2-forced-endgame.md:223-229` — "it just never happens because v0.4 cashes locked half-suits promptly. **Any v0.5 change that makes the bot *more* patient about declaring… will start producing multi-set forced endgames, and then the order will matter.**" FishBot v0.7 disables the entire urgency escalation (`askfloor=-1,pool=-1,oppfloor=-1,force=1000000`), making `urgent` identically false; `P5-mech.jsonl` shows the comparator using that path on **50.2%** of its declarations. No v0.7-era measurement of live-half-suit multiplicity at forced-endgame entry exists anywhere under `research/v07/results/`. The v0.6 paper carried the caveat (`paper/sections_v06/A-dialect.tex:163-176`, "A policy made more patient about declaring would begin to produce multi-half-suit forced endgames") and the v0.7 compression deleted it.

**The narrowest fix**: three changes, not one. (i) Attribute the sample: "in v0.4 self-play". (ii) Downgrade the universal to the tendency the corpus recorded — `C1-v04-corrections.md` C9 prescribes "essentially every v0.4 forced endgame"; if that wording is adopted, `\numLadderLiveSets` can no longer carry "exactly 1", and the second sample (118/120 at seed 1234567) should be transcribed alongside as `R5-state-of-knowledge.md:63` does. (iii) Restore the policy-dependence caveat from `sections_v06/A-dialect.tex:171-176`, adding that the frozen configuration is the direction of change the v0.4 source warned about and that no v0.7-era measurement exists. Deleting the item from the v0.7 appendix entirely is also defensible — nothing else depends on it.

---

### 11. The white-box adversary's policy oracle is pinned to v0.6, not to the target, and the paragraph's "two limits" omit it. — SERIOUS

**Location**: `paper/sections_v07/11-results.tex:424-435` (PDF p.45), restated at `13-conclusion.tex:15`.

> "Z08 is the adversary class the threat model of §8 was written for: an opponent that **reads the target's own deterministic policy** and reconstructs the deal posterior from the public transcript… **Two limits belong with that cell**, since it is the one a sceptical reader should weigh most heavily. 18 of its fitted coordinates are inert… And the budget is small."

**The claim**: the deployed Z08 inverts the frozen target's transcript against the *target's own* policy; and the only two limits worth weighing are the inert coordinates and the budget.

**What the source shows**: both Z08 rows of `research/v07/results/P5-B4eval.jsonl` carry `advSpec` base **`v07i:idet=48,imodel=v06`** against `match.b` = the frozen v0.7 spec, and the row's literal `argv` confirms it. `engine/src/factory.hpp:336-339` reads `imodel` into `a->inv.oracle.spec` (defaulting to the literal string `"v06"` even when absent); `v07_invert.hpp:70-84` builds that spec with `makeAgent` and asks it what it would have played; `:204` is the sole call site, inside `invertAsk`; `v07_responder.hpp:296-299` inverts only asks by the opposing team, i.e. the frozen target. So the reconstruction Π in the likelihood is a bare `V06Agent`. That is not "v0.7 minus one weight": `v06.hpp:42` has `search = false` and `factory.hpp:216` turns it on only via `s1`, which the oracle spec does not carry — so the oracle has no determinized search, urgency escalation **on**, `rtie` off, and no `r12` coordinate, while the target searches on roughly half its decisions, carries `r12=25`, has urgency identically disabled, and breaks ties by a public hash. By contrast `engine/src/v07_probe.hpp:93` sets `inv.oracle.spec = bc.model.empty() ? bc.specB : bc.model` — the offline bit probe falls back to the actual target; the agent path has no such fallback. The setting is registered (`PREREGISTRATION.md:357-358`, "which is how phase 1's C5 responder is specified") and was carried forward unchanged from a phase whose target *was* v0.6 (`ADVERSARIES.md:983`, `P1-screen.md:9`, column "edge vs `v06`") — so it needs no new deviation row, but it is undisclosed: `grep -rn "imodel\|oracle" paper/sections_v07/ paper/tables_v07/` finds nothing describing it, and D16 covers only the 55-vs-37 coordinate count.

**The narrowest fix**: prose only. Either narrow the opening sentence to say the inverter reconstructs the posterior by asking a **v0.6 model of the target** what it would have played, or — preferred, since it keeps the class/instance distinction — leave the class sentence and change "Two limits" to three, adding the oracle first: the inverter the base fixes queries `imodel=v06`, so the likelihood is computed against the v0.6 policy rather than the frozen target, a setting carried forward from phase 1 where the target was v0.6. Say explicitly that the corpus does not measure how far the v0.6 oracle's argmax diverges from the frozen target's ask, so the size of the degradation is **unquantified** — that makes the null weaker evidence, in the same direction the other two limits point. Apply the same narrowing at `13-conclusion.tex:15`. Do not change §8's threat-model definition (the class genuinely has the target's source and vector), and do not touch `\vsevenZeightInert`, `\vsevenZeightLive` or D16.

---

### 12. "The only hand-entered numbers in this section" is false under every reading, including the narrowest. — SERIOUS

**Location**: `paper/sections_v07/11-results.tex:314-320`, Table 9's caption (PDF p.42-43).

> "The two training rows are quoted from the registered protocol and **are the only hand-entered numbers in this section**…"

**The claim**: the two training rows of Table 9 are the only numbers in the Results section entered by hand rather than generated from artifacts.

**What the source shows**: false under all three readings.
- **Whole section**: 21 macros used in `11-results.tex` resolve only from `paper/numbers_v07.tex` (the hand-transcribed file): `vsevenKthreeOverComposite{,Lo,Hi}`, `vsevenSubAddPhaseTwo`, `vsevenSubFloorExcess{,Lo,Hi}`, `vsevenTrainGroup{,Lo,Hi}`, `vsevenTrainOneSeatFive/Six`, `vsevenTrainOppSix{Self,Lo,Hi}`, `vsevenTrainRtwelveLoo`, `vsevenVsixCoords`, `vsevenXpDistance{Lo,Hi}`, `vsevenZeightInert/Live`.
- **Same subsection as the caption** (§11.5, `:279-336`): five of them — `\vsevenTrainOppSixSelf` +4.82, `Lo` +2.50, `Hi` +2.88 at `:301-302`, and `\vsevenTrainOneSeatFive` +1.26, `\vsevenTrainOneSeatSix` +2.88 at `:333-334` — all defined at `numbers_v07.tex:135-139` under the header "% docs/v07/PREREGISTRATION.md". On PDF p.43 the reader sees "(registered expectation +1.26)" three lines below the caption, and +1.26 is a cell of the very training row the caption describes.
- **Narrowest reading** ("hardcoded into the generator"): `engine/build_tables_v07.py:1456-1470` hardcodes the entire Training column of Table 6 (+0.78, +2.94, +4.51, +4.48) under its own comment "the training column is TRANSCRIBED from the protocol"; that table is `\input` at `11-results.tex:136`, inside the same section, and its Shortfall column derives from those four constants.

The section's opening sentence (`:4-6`, "Every measurement in this section is produced from the artifacts by the generator… and emitted into this document mechanically") is overbroad for the same reason. Appendix G contradicts the caption directly: `G-reproduction.tex:95,103` prints 250 generated / 24 inherited / **121 transcribed**.

**The narrowest fix**: delete the clause "and are the only hand-entered numbers in this section", leaving the rest of the caption intact. Do **not** replace it with a count: the 21 figure omits the four registered constants at `build_tables_v07.py:1461-1464`, which are printed in Table 6 and emitted into the *generated* file (`numbers_v07_generated.tex:914/920/926/932`), so a stated count would be wrong again in the same direction. The section-opening sentence at `:4-6` should be narrowed in the same pass.

---

### 13. The conclusion says the advantage survives every coordination and robustness test; §11.5 says the deciding row is unresolved. — SERIOUS

**Location**: `paper/sections_v07/13-conclusion.tex:10` (PDF p.52).

> "**That advantage survives the tests of coordination and robustness this paper applies.**"

**The claim**: the advantage passed every such test in the paper.

**What the source shows**: the relative clause quantifies over every such test in the paper, and the paper enumerates that set at `01-introduction.tex:52-56` — "against a fixed opponent panel, under substituted partners, across rule dialects, and against adversaries fitted to exploit it. This the paper evaluates extensively, and **reports mixed results**."
- `11-results.tex:283-289`: "The minimum is −0.19 [−1.04, +0.67], which is above the registered −1.00 collapse threshold on the point estimate but ***not resolved against it***… **This battery does not separate a pass from a failure on the row that decides the claim**, and reading a threshold from a point estimate is a move this paper declines elsewhere… and declines here." Recomputed from `P5-B6.jsonl`: withholder −0.1875 [−1.0438, +0.6688], banks +0.000 / −0.375, table marks Repl. **no**; v05 partner +0.7833 [−0.09, +1.66] also contains zero.
- `11-results.tex:305-311`: against a v0.6 opponent, of three changed rows **two fall below the protocol's stated range** (v03 +1.46 against a stated floor of +2.50; detective +2.34).
- `11-results.tex:215-217` and `:192-198`: third of four on minimax regret; worst panel cell does not replicate in sign. The conclusion's enumeration omits the panel axis entirely.
- The discussion's parallel passage is correctly scoped: `12-discussion.tex:13-19` claims survival only for the dialects, under the narrower topic sentence "The advantage is not an artefact of self-play coordination", and `:41-45` adds "**Three further results are consistent with that account and none is comfortable.**" `13-conclusion.tex:18` says "**Two** findings qualify that picture" and names neither robustness result.

**The narrowest fix**: replace the topic sentence with a no-overturn claim and add one clause after the enumeration carrying what the body already states, e.g. "No coordination or robustness test in this paper overturns that advantage, though the picture these tests give is mixed." … "Two of those results are not clean. The partner row that decides the coordination claim is −0.19 [−1.04, +0.67], above the registered −1.00 collapse threshold on the point estimate but not resolved against it (§11.5); and over the shared panel FishBot v0.7 is 3rd of four on minimax regret with a worst cell that does not replicate in sign (§11.3)." Every macro exists and is generated. Do **not** use the filed wording "survives the coordination and dialect tests, with the partner-substitution row unresolved" — the partner battery *is* the coordination test (it supplies the "+1.93 of +4.49" clause in the next sentence), so that phrasing would assert both that the coordination tests passed and that the row deciding them did not, and it silently drops the eight adversarial searches, which did resolve. While editing, check `13-conclusion.tex:18`'s count against `12-discussion.tex:41-45`'s three.

---

### 14. The abstract says the advantage persists under partner substitution; §11.5 says that battery cannot separate a pass from a failure. — SERIOUS

**Location**: `paper/sections_v07/abstract.tex:16-17` (PDF p.1).

> "The advantage **persists under partner substitution**, cross-play between independently trained runs, and eight rule dialects."

**The claim**: unqualified persistence under partner substitution, alongside cross-play and dialects.

**What the source shows**: `11-results.tex:282-288` refuses exactly this conclusion (quoted in full in finding 13). Recomputed from `P5-B6.jsonl`, the seven changed-partner settings against a v0.5 opponent: v06 +1.93, detective +1.83, lockout +1.89, v03 +1.40, v04 +1.34 — five clearing zero; v05 **+0.78 [−0.09, +1.66]** containing zero; withholder **−0.19 [−1.04, +0.67]**, per-bank +0.000 / −0.375, Repl. **no**, lower bound below the −1.00 threshold. The paper's own conclusion has already been brought into line and the abstract has not: `13-conclusion.tex:11-14` keeps "persists" for the dialects and cross-play and replaces the partner claim with the one-seat statistic. `01-introduction.tex:53-54` calls this axis "mixed results". Cross-play (+0.01 pp against a registered 1.50 threshold) and the dialect sweep (all eight rows keep their sign, max excursion +0.78 inside the registered 2.00 tolerance) are cleanly supported and should not be weakened.

**Counter-argument the author should weigh, and why I keep the finding.** An adjacent filing against this same line was refuted, on two grounds: registered claim S1 (≥5 of 8 positive, minimum not below −1.00) resolves **PASS**, and the v0.6-opponent partner battery (Table 15) is all-positive with intervals excluding zero (itself +4.55, v06 +2.75, detective +2.34, v03 +1.46). Both facts are correct. I keep the finding because the defect is not that S1 failed — it is that the abstract asserts, without hedge, the proposition §11.5 explicitly declines to assert, and that the conclusion already declines it too. The abstract is the one place in the paper where this axis is presented as clean.

**The narrowest fix**: qualify only the partner clause, using §11.5's own words and existing generated macros: "The advantage persists under cross-play between independently trained runs and eight rule dialects. Under partner substitution 6 of 7 changed-partner rows stay positive; the worst is −0.19 [−1.04, +0.67], which this battery does not resolve against the registered −1.00 collapse threshold." Do **not** adopt the filed instruction to "name the opponent each battery was played against": B6 was played against **two** opponents (40 cells `--b=v05`, 16 cells `--b=v06`), so it cannot be followed accurately. See UNRESOLVED U-6 for the related antecedent question.

---

### 15. RQ1 calls the registered target the strongest available v0.6 configuration; the registration says it was chosen for being the cheapest. — SERIOUS

**Location**: `paper/sections_v07/01-introduction.tex:31-32` (PDF p.4).

> "**RQ1** Does FishBot v0.7 achieve a replicated improvement over **the strongest relevant configuration available from v0.6**, measured on material sealed before FishBot v0.7 existed?"

**The claim**: the registered comparison target was the strongest configuration v0.6 had available.

**What the source shows**: every other statement of the target in the corpus says *cheapest*. `09-development.tex:20-22` — "Beating the deployed policy is the easy comparison… The comparison target was therefore fixed as F-cheap, **the cheapest configuration that is genuinely on the frontier**." `abstract.tex:13`; Table 4 at `10-design.tex:31-32` ("cheapest configuration genuinely on the v0.6 frontier… **the registered primary comparison target**"); `12-discussion.tex:7-8`; `13-conclusion.tex:4-5`. `docs/v07/PREREGISTRATION.md:506-507` and `:519-521` — "the cheapest point of the v0.6 frontier that is actually on the frontier… `F-cheap` is named as the bar rather than `v06` deliberately." A frontier is a cost/strength Pareto set; the cheapest genuine point on it is the weakest genuine point on it. "Relevant" cannot exclude F-mid, which Table 4 lists as "a more expensive frontier configuration", origin "pre-existing", role "secondary frontier comparison", drawn inside the frontier box at `10-design.tex:63`. Empirically the artifacts give the claim no support: from `P5-B3.jsonl`, F-cheap vs F-mid is **−0.104 [−1.320, +1.111]** (unresolved), and all four arms order F-mid at or above F-cheap on the point estimate, one difference marginally excluding zero (INCUMBENT −1.312 [−2.608, −0.017]). So the claim is unsupported rather than measurably false, in the direction that flatters the primary result.

**The narrowest fix**: in `01-introduction.tex:31-32` only, replace "the strongest relevant configuration available from v0.6" with either §11.1's own heading ("a replicated improvement over the v0.6 frontier") or Table 4's wording ("over F-cheap, the cheapest configuration **genuinely** on the v0.6 frontier and the target registered in advance"). The word "genuinely"/"actually" is load-bearing and must not be dropped — the deployed v0.6 policy is cheaper than F-cheap and is also drawn inside the frontier box, so "the cheapest configuration on the v0.6 frontier" without it would be false in the opposite direction. Do **not** add any claim that F-mid is stronger; that comparison does not resolve.

---

### 16. Contribution 5 converts "the mechanisms we investigated" into a property of the policy class; §12.3 refuses that conversion by name. — SERIOUS

**Location**: `paper/sections_v07/01-introduction.tex:90-93`, contribution 5 (PDF p.4).

> "…identifying a resolution barrier: **the gains that remain in this policy class, if any, are smaller than the evaluation and adversary-search machinery can reliably detect**."

**The claim**: a universal over the class's remaining gains — every one of them is sub-resolution. "If any" hedges existence, not magnitude.

**What the source shows**: `12-discussion.tex:63-64`, the section the bullet cites, opens by refusing exactly this: "Answering RQ4, the natural summary is that **this policy class is flat, and that summary would overstate the evidence**. What the evidence supports is narrower." The narrower statement it bolds (`:66-68`) is quantified over what was tried: "At the calibrated detection scale of this evaluation, the **tested** policy class **appears** locally flat: **none of the investigated architectural mechanisms** produced a replicated gain above the instrument's effective resolution", followed at `:71` by "That is not a claim that no further gains exist." `12-discussion.tex:84-88` forbids the universal from the other side: the eight searches give "a *lower bound* on exploitability rather than an upper bound. **A stronger response outside the tested classes, or the same classes at a larger budget, could give a larger figure**" — a stronger response is by construction a detectable gain remaining in this class. The abstract (`:24-25`) and conclusion (`13-conclusion.tex:30-34`) both carry the hedges the introduction drops. The evidence base is four rejected mechanisms, one null composite cell, and eight CEM searches at 8 generations × 12 candidates × 240 deals — none of which constrains an untried mechanism.

**The narrowest fix**: replace only the colon clause with §12.3's own scoping, e.g. "…and identifying a resolution barrier: none of the investigated architectural mechanisms produced a replicated gain above the instrument's effective resolution, and that resolution does not fall as evaluation games increase." Both clauses are `12-discussion.tex:66-68` and `:75-77` near-verbatim. If the duplication with the bullet's own first half is unwanted, reduce the clause to the barrier alone: "the instrument's effective resolution does not fall as evaluation games increase, so the constraint is search power rather than sample size." Do not substitute "the class is flat" or "locally flat" without the three hedges ("tested", "appears", "at the calibrated resolution").

---

### 17. §3.2 says the exact-counting machinery is "sidestepped entirely"; matrix scaling is the deployed agent's only belief computation. — SERIOUS

**Location**: `paper/sections_v07/03-related.tex:138-145` (PDF p.9).

> "**Two adjacent lines are named and not used.** … And the exact-counting machinery this study does not need to approximate, Ryser's inclusion–exclusion formula, Glynn's sign-vector formula, the Jerrum–Sinclair–Vigoda chain, **matrix scaling** and sequential importance sampling, **is sidestepped entirely** by the block structure of §5."

**The claim**: none of the five named permanent-approximation techniques, matrix scaling included, is used by this study.

**What the source shows**: the deployed agent's belief *is* matrix scaling. `05-inference.tex:117-120` lists "matrix scaling `\cite{lsw}`" among the literature's methods; `:200-201` says the deployed path "replaces the count join by iterative proportional fitting, **better known here as Sinkhorn scaling**"; Eq. (eq:sinkhorn) at `:211-215` is row-normalise / column-scale-to-capacity alternation — RAS scaling to prescribed margins, the object of `\cite{lsw}`. In the engine: `v05.hpp:48` `BeliefMode::Fast`; `v05.hpp:307-311` unconditionally calls `bel.sinkhornDisj(...)`; `belief.hpp:519-543` is the iteration, with the engine's own comment "Sinkhorn (iterative proportional fitting)"; `ensureExactMarginals` has zero call sites — so matrix scaling is not one path among several, it is the only belief the frozen agent ever computes. The vetted v0.6 text said the opposite and said it explicitly: `paper/sections_v06/I-related-extended.tex:359-361` — "Matrix scaling is the usual practical approximation… **a Sinkhorn iteration of this kind is what the deployed approximate path uses**", closing at `:364` with "**Cited as context, with one used.**" v0.7 dropped that appendix and hardened the main-text deferral into an assertion, deleting the correction.

**The narrowest fix**: two edits. At `:138`, the paragraph-opening tally must stop claiming both lines are unused; v0.6's own wording is "Cited as context, with one used." At `:141-145`, keep the block-structure claim, which `05-inference.tex:121-124` supports, and restore the exception: four of the five (Ryser, Glynn, JSV, sequential importance sampling) are unused, and matrix scaling is the exception because the deployed path replaces the exact count join with a Sinkhorn/IPF iteration of exactly that kind — not because the exact count is unaffordable, but to buy θ and φ (`05-inference.tex:197-199, 227`). Do **not** write a bare "matrix scaling is not sidestepped", which would read as retracting the block-structure result; the precise statement is that the block structure removes the *necessity* and the deployed configuration runs one of the five by choice.

---

### 18. DIVAT and AIVAT are named as the reason the paper's cells are paired duplicates; the project's own record says they were declined and pairing adopted instead. — SERIOUS

**Location**: `paper/sections_v07/03-related.tex:174-177` (PDF p.10).

> "**Duplicate-style variance reduction and its estimators, DIVAT and AIVAT, are the reason every cell in this paper is a paired duplicate** rather than an independent sample. The Annual Computer Poker Competition's protocols are the model for a fixed opponent panel scored identically for every arm."

**The claim**: Zinkevich et al. and Burch et al. are duplicate-style estimators and are why the paper's cells are paired duplicates.

**What the source shows**: neither is a duplicate-match technique; both are estimator-side methods that subtract a learned or hand-built value function from a single sample and require no duplicate rotation. `paper/sections_v06/I-related-extended.tex:380-387` — "Advantage-sum estimators and AIVAT learn or hand-build a value function and subtract it… They remain **declined**." `:461-464` — "Fitted control variates for evaluation. **Declined**…; **pairing on common random numbers is adopted instead**." `:389-398` names the actual source: "duplicate bridge replays each board with the same cards in the same seats, and the computer poker competition added common seeds across pairings for the same reason `\cite{acpc}`." `paper/sections_v06/10-protocol.tex:76-80` states the relation correctly: "The pairing is what makes the design competitive with the estimator-side variance reduction developed for poker **without needing an auxiliary value function**." The paper's own bibliography convicts the sentence: `bibliography.tex:338-341`, the `acpc` entry, reads "…and the competition rules describing **duplicate matches and common seeds**" — yet the next sentence credits ACPC only with the opponent panel. No such estimator is used anywhere: `grep -rni "aivat|divat|control variate|advantage.sum"` over `engine/src/`, `research/v07/`, `docs/v07/` returns exactly one hit, `THREAT-MODEL.md:382`, a future-work recommendation ("AIVAT is available and should be considered… the value function must be frozen and preregistered before the holdout is opened"). Every P5 interval is the deal-clustered bootstrap (`arena.hpp:293`). `07-evaluation.tex:34-36` is correct as written ("in the spirit of duplicate bridge and of the variance-reduction estimators developed for poker") — this is the restatement that widened.

**The narrowest fix**: attribute the duplicate design to duplicate bridge and to the ACPC's duplicate-match and common-seed rules — which the paper's own `\bibitem{acpc}` already says that citation covers — and place DIVAT and AIVAT as estimator-side alternatives not used here, with the pairing being what makes the design competitive with them without introducing a fitted object. Stay inside the self-contained v0.7 document: do not cite `sections_v06`, and do not import v0.6's flat word "declined" as a v0.7 decision, since `THREAT-MODEL.md:382` records AIVAT as something a future study should consider. Leave `07-evaluation.tex:34-36` untouched.

---

### 19. A 2015 paper is listed among "the ad-hoc-teamplay results that followed" a 2019/2020 benchmark proposal, and a bridge-bidding paper is listed as ad-hoc teamplay. — SERIOUS

**Location**: `paper/sections_v07/03-related.tex:160-167` (PDF p.10).

> "Hanabi was proposed as a benchmark precisely because self-play scores overstate coordination [39], and the **ad-hoc-teamplay results that followed** [41, 42, 43, 45, **46, 47**, 48, 49, 51] are the reason the cross-play measurement of §11.6 exists at all."

**The claim**: all nine cited works are ad-hoc-teamplay results, and all followed the Hanabi benchmark proposal.

**What the source shows**: refuted by the paper's own reference list one page later. [39] `bard-hanabi` is "Artificial Intelligence 280, **2020**; arXiv:1902.00506" (`bibliography.tex:211-216`); [46] `cox-hanabi` is "Mathematics Magazine 88(5):323–336, **2015**" (`:243-247`). A 2015 paper cannot be among the results that followed. [47] `tian2020jps` is Joint Policy Search, evaluated on contract-bridge bidding. The corpus characterises both correctly and the v0.7 condensation lost it: `paper/sections_v06/03-related.tex:282-284` — "Joint policy search is the **bridge-bidding analogue**, scoring simultaneous changes at several information sets, at +0.63 IMPs per board"; `:284-287` — "the Hanabi construction that most clearly **fails to port** is the hat-guessing code, which works because each receiver sees every other receiver's hand, and in Fish no player sees any hand." In v0.7 each key occurs exactly twice — its `\bibitem` and this one `\cite` — so no other characterisation exists.

**The narrowest fix**: remove `cox-hanabi` and `tian2020jps` from the bundle and cite each separately with the characterisations `sections_v06/03-related.tex:282-287` already supplies (JPS as the bridge-bidding analogue at +0.63 IMPs/board; the hat-guessing construction as the one that most clearly fails to port). Keeping both cited matters: `paper/check.py:324-326` raises "STRUCT bibitems never cited" for orphans, so simply deleting the two keys would leave [46] and [47] uncited unless their `\bibitem` entries are deleted too (which renumbers every later reference). The surviving collective label is still inexact for `hu-sad` (a self-play training method) and `jacob2022pikl` (human-regularised play); v0.6's framing split them into a coordination/convention line and a human-compatibility line.

---

### 20. "Every method above is published with a caveat that it can increase exploitability" — ReBeL's Theorem 3 is a safety guarantee, and the paper's own preceding paragraph lists six different caveats. — SERIOUS

**Location**: `paper/sections_v07/03-related.tex:112-113` (PDF p.8).

> "**Every method above is published with a caveat that it can increase exploitability**, so none can be evaluated by win rate alone."

**The claim**: every search method in §3.3 — including the full-solver line (DeepStack, ReBeL, Student of Games) — carries an author-stated exploitability-increase caveat.

**What the source shows**: the paragraph immediately above states, work by work, what the published caveat actually is, and none of the six is exploitability — update equivalence ("its improvement guarantee is for common-payoff games"), Learned Belief Search ("redundant here"), RL fine-tuning ("requires gradient updates at decision time"), kubicek ("recorded for its meta-lesson"), the CDR line ("do not compose with limited-look-ahead solving off the shelf"), the full-solver line ("established that test-time re-solving… is principled rather than a hack"). ReBeL is published with the opposite, and this programme wrote it down twice: `research/v04/lit/cfr-team.md:224` — "**Theorem 3:** running the same procedure at test time is **safe** (approximate Nash) with no modification"; `paper/sections_v06/I-related-extended.tex:184-187` — ReBeL "proves that running the same procedure at test time is safe, **which is the licence under which §search operates at all**", and `:435-437` "ReBeL's safety result is retained as the licence for test-time search." v0.7 dropped that appendix; the surviving sentence asserts the opposite of the programme's own recorded reading. `research/v04/lit/evaluation.md:275` records DeepStack's LBR result as "could not establish any positive lower bound (reported as 0)"; `cfr-team.md:226` records SoG's Theorem 2 as bounding exploitability growth as linear in game length. The claim is true of `zhang2021subgamesolvingwithoutck`, which the paper already reports correctly and specifically at `:77-78` ("prove that the naive form can *increase* exploitability").

**The narrowest fix**: drop the universal, keep the conclusion. Attribute the exploitability-increase result to `zhang2021subgamesolvingwithoutck` alone; note that the guarantees the other methods carry are conditional on settings Fish does not satisfy — two-player zero-sum for the full-solver and depth-limited lines, common-payoff for update equivalence, a blueprint-following partner for SPARTA/LBS — which the preceding paragraph already establishes work by work; and state the win-rate conclusion as this study's own evaluation stance. Do **not** cite `lerer2020sparta` or `milec2021cdr` as sources of an exploitability-increase caveat, as the filed fix proposed: SPARTA's voided quantity is a blueprint *improvement* guarantee in a common-payoff game (the paper's own `:87-89`), and the CDR warning is about composition (`:106-108`).

---

## MINOR

### 21. The abstract's negative paragraph omits the one robustness battery that goes against the subject. — MINOR

**Location**: `paper/sections_v07/abstract.tex:16-25` (PDF p.1); same omission at `13-conclusion.tex:10,18`.

**The claim**: as written, the abstract's robustness account is uniformly favourable — every battery it names (partners, cross-play, dialects, adversarial search) is a pass, and the negative paragraph carries only the composite null and the four rejected mechanisms.

**What the source shows**: the paper's own §1.2 Robustness bullet lists "against a fixed opponent panel" **first** and calls the whole bullet "mixed results", and the panel is the only mixed battery. §11.3 reports two results, both against the subject: recomputed from `P5-B3.jsonl` (248 cells, 2,102,400 games — roughly half the scored battery), FROZEN's worst cell is **−0.0417 [−1.412, +1.328]** at `composite` and **does not replicate in sign** (banks +1.62 at SEALED:X01xC3f / −1.75 at composite, using the paper's own within-bank macros), and minimax regret over the 31 members is F-cheap 4.000, composite 4.083, FROZEN **4.533 (third of four)**, INCUMBENT 5.671 — the phase-2 composite ahead on both. `12-discussion.tex:40-45` says "Three further results are consistent with that account and **none is comfortable**"; the abstract's dedicated negative paragraph carries two of the three, and `13-conclusion.tex:18` says "**Two** findings qualify that picture".

Severity is MINOR, not higher: the panel is not one of RQ2's four axes as the introduction defines them; the registered panel claim (S6, worst case not catastrophic) **passed**; `11-results.tex:196-197` records that the protocol "sets no threshold on this quantity and requires only that it be reported"; the adverse cell is not distinguishable from zero on 4,800 games and the same pairing at 48,000 games is +0.15; and FROZEN leads the deployed policy on 25 of 31 members with intervals excluding zero.

**The narrowest fix**: one clause in the abstract's negative paragraph, in §11.3's own terms — over a shared 31-member opponent panel its worst cell is −0.04 pp, which does not replicate in sign, and it is 3rd of four on minimax regret. Use the paper's macros (`\vsevenWorstFrozen`, `\vsevenWorstFrozenRepl`, `\vsevenRegretFrozenRank`); if per-bank values are stated, use +1.62 at SEALED:X01xC3f (`numbers_v07_generated.tex:32,36`), **not** FINAL-RESULTS' +1.67. Do not imply the advantage failed to persist, and do not present minimax regret as a failed threshold. Check `13-conclusion.tex:18`'s count in the same pass.

### 22. "Four of the eight adversarial searches" maximise win rate; five do. — MINOR

**Location**: `paper/sections_v07/11-results.tex:479` (PDF p.45).

> "they maximise win rate as **four of the eight** adversarial searches do, so this is a second sample rather than an independent method."

**The claim**: four of the eight B4 searches share the B9 responders' win-rate objective.

**What the source shows**: five. `python3 -c "import json;rs=[json.loads(l) for l in open('research/v07/results/P5-B4fits.jsonl')];print(sum(1 for r in rs if r['kpi']=='win'))"` → **5** (Z01 win, Z02 win, Z03 declerr, Z04 events, Z05 forced, Z06 win, Z07 win, Z08 win). Corroborated twice more: the generated macros `\vsevenZ*Kpi` and the paper's **own Table 20** (`paper/tables_v07/adversaries.tex`), whose Objective column prints "win" on five rows. All four rows of `P5-B9fits.jsonl` carry `--kpi=win`, so the premise about the responders is right. "four" is hand-typed, so no provenance check covers it. The error runs against the paper's interest: it understates the overlap the sentence exists to confess.

**The narrowest fix**: "five of the eight". Do **not** touch `:411`, "They maximise four different objectives, being win rate, declaration error, event count and forced-endgame incidence" — that four is the count of distinct *objectives* and is correct.

### 23. The legacy residual's interval follows from no coherent variance calculation. — MINOR

**Location**: `paper/sections_v07/11-results.tex:383-389` and `12-discussion.tex:152-153` (PDF p.43, p.50).

> "…it is −0.292 [−1.69, +1.11], an interval **9.6 times** the size of the residual, so the data are equally consistent with the ladder contributing nothing."

**The claim**: a 95% interval of [−1.69, +1.11], width 9.6× the residual.

**What the source shows**: the point estimate reproduces exactly (−0.29165 from `P5-B8.jsonl`); neither bound does. `engine/build_tables_v07.py:1069-1072` builds `ses` over five cells (`legacy, no-out-of-turn, no-cardless-declare, maxasks=360, default`) with **unit coefficients** and takes `h = 1.96*sqrt(sum s²)`. But the residual is `legacy + 2·default − n1 − n2 − n3`, so `default` carries coefficient +2 (variance ×4). Candidate half-widths: as coded **1.4033** → [−1.695, +1.112], ratio 9.6 (published); coefficient-correct three-component **1.7766** → ratio 12.2; the two-component form the sentence itself asserts **1.2544** → [−1.55, +0.96], ratio **8.6**; `p5_analyse.py:525`'s `3·var(default)` 1.6617 (the value FINAL-RESULTS.md publishes). The generator's own comment at `:1062-1064` contradicts its code: "`--maxasks=360` is bit-identical to default, so the residual is legacy minus **TWO** effective components and not three." That identity is exact in the artifact — `winRateA` 0.545750 / 0.548833 and the `ci` tuples are equal element-for-element on both banks — so that term has variance **0**, not variance `var(maxasks)+var(default)`.

**The narrowest fix**: at `build_tables_v07.py:1069`, drop `'maxasks=360'` from the tuple, leaving the coefficient-correct variance for the two-component residual the prose already asserts: `\vsevenLegacyResidualLo` −1.55, `Hi` +0.96, `Ratio` 8.6. No prose change; the conclusion holds under every candidate. Do **not** adopt 12.2, which is correct only for the three-component definition the paper's own sentence disclaims, and would print an inflated interval beside prose saying the third row measures nothing. Neither figure is a true 95% interval — all five cells share banks and deals, and the maxasks/default identity proves the correlation is large — so the surrounding text should not call it one without qualification. No verdict moves: S5 is decided by the largest excursion (+0.78 on `arb=turn`).

### 24. σ√(2 ln K) is stated as the expected maximum; it is an upper bound, 34% above the true expectation. — MINOR

**Location**: `paper/sections_v07/11-results.tex:120-123` and `:139-141` (PDF p.39).

> "At the lattice cell size the per-cell standard deviation is 0.3163 pp, so **the expected maximum of 15 draws** under the null that none differs from the reference **is** σ√(2 ln K) = 0.74 pp. […] The largest shortfall, 0.63 pp against an expected maximum under the null of 0.74…"

**The claim**: 0.74 pp is the expected maximum of 15 null draws at σ = 0.3163.

**What the source shows**: σ = 98/2/√24000 = 0.316294 reproduces exactly and σ√(2 ln 15) = 0.7361 → 0.74. But σ√(2 ln K) is the standard sub-Gaussian upper bound on E[max], not the expectation. By Simpson integration of E[max] = ∫x·K·φ(x)·Φ(x)^(K−1)dx (validated against the exact n=2, 5, 10 order-statistic means): E[max of 15 iid N(0,1)] = 1.735913, so the exact expected maximum is **0.5491 pp** — 0.74 is 34% high. The protocol hedges where the paper does not: `docs/v07/PREREGISTRATION.md:489-491` — "has expectation **approximately** σ√(2 ln K)". Eleven of the fifteen cells are add-one-in or leave-one-out variants of one configuration on the same banks against the same opponent, so the correlated-null expectation is lower still. The direction matters: the 0.63 pp shortfall is below the printed 0.74 but **above** the exact 0.549 — which strengthens rather than weakens the paragraph's own reading ("the pattern the check was written to look for, and it appears where selection occurred").

**The narrowest fix**: prose only; do **not** change `\vsevenSelExpectedMax` or `build_tables_v07.py:1441`, since 0.74 is the registered figure and replacing it would move a pre-committed bar after the data were seen. Restore the protocol's qualifier ("has expectation approximately…") or state it as what it provably is (E[max] ≤ σ√(2 ln K) = 0.74 pp, the quantity the protocol registers). At `:140`, "against an expected maximum under the null of 0.74" should become "against the registered bound of 0.74". If the exact figure is wanted, it belongs in a footnote marked as a recomputation, with a hand-transcribed entry naming its source, since no artifact carries it.

### 25. The cross-play difference is printed in the reverse of the order the sentence sets up. — MINOR

**Location**: `paper/sections_v07/11-results.tex:340` and `:159` (PDF p.42).

> "Diagonal mean +4.55, off-diagonal mean +4.55, **difference +0.01 pp** against a per-cell half-width of 0.63."

**The claim**: read in the order stated (diagonal, then off-diagonal, then "difference"), the diagonal exceeds the off-diagonal by 0.01.

**What the source shows**: recomputing the nine `crossplay` cells of `P5-B7.jsonl`, diagonal mean 4.548617, off-diagonal mean 4.554167; **off − diag = +0.00555** (→ +0.01), diag − off = −0.00555. `engine/build_tables_v07.py:992` emits `\vsevenXpGap` as `_sg(om - dm)` with the provenance string "OFF-DIAGONAL MINUS DIAGONAL". Both means print as +4.55, so the reader cannot recover the sign from the operands. The paper litigates this exact sign in its own correction appendix: `E-corrections.tex:126-131` — "The phase-5 document reports the gap as −0.01, which is diagonal minus off-diagonal… the informative direction is off-diagonal minus diagonal, which is +0.01. Neither sign is resolved… the sign as printed implies a small collapse, and the data support no direction at all." `:159`'s condition-4 row prints the signed value with no antecedent. No verdict moves (0.01 against a 1.50 threshold), and the abstract, discussion and conclusion all use the unsigned `\vsevenXpGapAbs`.

**The narrowest fix**: name the subtraction order at `:340` — "off-diagonal minus diagonal +0.01 pp". Do **not** negate the number: −0.01 is `\vsevenXpGapRecorded`, the reversed convention `E-corrections.tex` records as corrected. Do **not** write "off-diagonal exceeds diagonal", which asserts a direction the appendix says is unresolved. At `:159`, either name the order or use `\vsevenXpGapAbs`.

### 26. "Any self-oriented per-decision objective" quantifies over a class; three instances were tested. — MINOR

**Location**: `paper/sections_v07/12-discussion.tex:54-56` (PDF p.48).

> "Fitting against **any** self-oriented per-decision objective produces a worse policy while moving its own proxy in the intended direction, because the ask and declaration channels trade against each other."

**The claim**: every self-oriented per-decision objective produces a worse policy.

**What the source shows**: three were fitted. `research/v07/results/K4-SUMMARY.txt` §3, matched budget (6 gens × pop 12 × 150 deals × 2 rotations, same start vector, same CEM seed, same CRN, `--sigmarel=0.08`): selfdecl −2.26, selfask −1.59, selfalloc −1.20, against a per-game control at +0.40. `ls research/v07/results | grep K4` shows exactly those four fits. §9.4.3 states it correctly and with a qualifier the discussion drops: "all **three** self-oriented per-decision objectives, **fitted at matched budget against a per-game control**". The class is expressly not exhausted: `docs/v07/CANDIDATES.md` §6 — "the honest reading is that this is a kill of one *form* of L5, not of L5"; "The strongest remaining form of L5 — a per-decision fit at a much wider σ for the same total games… has never been run"; "A *conditional* objective restricted to the subset where a mechanism fires is a different hypothesis and **was not tested**." (`engine/src/tuner.hpp:78-91` implements exactly three self-* KPIs, but a conditional objective is not one of them.) K4-SUMMARY's own verdict is "L5 is KILLED **in the form the ledger states it**", and it records that the per-game control also lost at that rung ("no fit at this budget beat v06 at all"). The paper polices this exact error against itself 40 lines later: `12-discussion.tex:88-91`, "in the tested classes… Every one of those qualifiers is load-carrying."

**The narrowest fix**: "Each of the three self-oriented per-decision objectives tested produced a worse policy…". Optionally add "at matched budget", which §9.4.3 already carries. Do not extend this into a claim that the direction is still open. Separate observation, not part of the fix: the trailing causal clause is measured for selfdecl and selfask, not for selfalloc, which lost on all three per-decision rates (K4-SUMMARY §4); §9.4.3 handles this correctly by naming the two fits before generalising.

### 27. §12.7's premise — three correlation regimes with the ex-ante one principal — is not in §8 or anywhere else in the paper. — MINOR

**Location**: `paper/sections_v07/12-discussion.tex:180-181` (PDF p.51).

> "**The threat model names three correlation regimes and makes the ex-ante correlated one the principal case.**"

**The claim**: §8, the paper's threat model, names three correlation regimes and designates the ex-ante correlated one principal.

**What the source shows**: `paper/sections_v07/08-threat-model.tex` is 43 lines and does neither. "Correlation regime" occurs once, at `:28`, only as one of four qualifiers that must be attached to each exploitability figure; §8 never enumerates the values, never uses "A0/A1/A2", "ex-ante" or "synchronised", and its only "principal" (`:9`) is on a different axis ("The principal adversary controls all three opposing seats"). Across the whole paper, "ex-ante" occurs once — in the sentence under audit; `09-development.tex:59` mentions "three correlation regimes" without naming them or a principal; "synchronised correlation regime" is used undefined at `11-results.tex:409` and `12-discussion.tex:87-88`, the latter while insisting "Every one of those qualifiers is load-carrying". Every other use of "the threat model" resolves to §8 via `\S\ref{sec:threat}`, and the paper cites no threat-model document. The claim is true only of `docs/v07/THREAT-MODEL.md:25` (T4), `:267-287` (§4.3 table) and `:285` ("Headline = A2"), which are outside the paper.

**The narrowest fix**: put the premise in §8, in the paragraph at `:27-29` that already demands the qualifier — three regimes named and reported separately (A0 independent, A1 synchronised, A2 ex-ante correlated with a shared pre-play signal secret from the target team), with A2 the registered headline and A1 the continuity column (`THREAT-MODEL.md:25, 274-276, 285-286`). That makes §12.7's premise checkable and gives §12.4's "synchronised correlation regime" a referent. Do **not** instead reattribute the sentence to an external document — the paper cites none, and "synchronised" would remain undefined at two other sites. Note that `THREAT-MODEL.md:764-766` pre-committed the report to state the fallback, so removing the concession is the worse of the two remaining options.

### 28. §9.2 credits the instrument phase with a floor value and a refutation that belong to the next phase, and misnames §7.9's two findings. — MINOR

**Location**: `paper/sections_v07/09-development.tex:28-29` (PDF p.30); duplicated at `paper/fishbot_v07_standalone.tex:6354`.

> "The detection-floor procedure of §7.9 was calibrated **in this phase** and produced **the two findings reported there**: the floor is 1.53 pp, and it does not decrease with evaluation games."

**The claim**: the phase this subsection reports (phase 1, the instrument) produced both the 1.53 floor and the finding that the floor does not fall with evaluation games; and those are §7.9's two findings.

**What the source shows**: two errors. (i) Phase 1 measured **1.68** and predicted the opposite: `docs/v07/INSTRUMENT.md:23` (I1) — "the detection floor is 1.68 points… the floor **buys down** as (evaluation games)^−1/2"; `:555` repeats 1.68. The 1.53 and the refutation are phase 2's: `docs/v07/ADVERSARIES.md:46` (A5) — "Phase 1's claim that the detection floor buys down with evaluation games is **refuted at its own predicted value**… at exactly 4× the games the floor is 1.53." The paper agrees everywhere else: `numbers_v07_generated.tex:1227` comments `\vsevenPrimaryFloor` as "the **phase-2** C1-class detection floor", and `E-corrections.tex:57-59` attributes it correctly. Every other macro in §9.2 is transcribed from the phase-1 INSTRUMENT.md block. (ii) §7.9 names its two findings explicitly (`07-evaluation.tex:173-181`): "First, the floor does not decrease with evaluation games… Second, **floors are family-specific**: the declaration channel has its own and higher floor of 2.13 pp." The value 1.53 is not the second finding.

**The narrowest fix**: state that this phase built and calibrated the procedure and measured the floor at 1.68 pp, predicting it would fall as (evaluation games)^−1/2 (INSTRUMENT.md I1); and that the phase of §9.3 re-measured the same rung at four times the games and got 1.53, refuting the prediction (ADVERSARIES.md A5), which is the first of §7.9's two findings. Do **not** simply swap 1.68 for 1.53 — that leaves the refutation attributed to the phase that predicted the opposite and still misnames §7.9's second finding, which must be named as family-specific floors with the declaration channel's 2.13 pp.

### 29. Phase 2 spanned two correlation regimes, not three. — MINOR

**Location**: `paper/sections_v07/09-development.tex:58-59` (PDF p.31).

> "This phase constructed six adversary classes **across three correlation regimes**, and measured channel ceilings before fitting anything against them."

**The claim**: phase 2 built adversaries spanning three correlation regimes.

**What the source shows**: `docs/v07/ADVERSARIES.md:70`, the document's own axis table, records the regime axis as "k = 3 and k = 1, **A1 and A2 ex-ante correlated**" — two regimes plus two seat counts. `grep -c "A0" docs/v07/ADVERSARIES.md` returns **0**; A0-as-regime appears only in THREAT-MODEL.md and INSTRUMENT.md, and `INSTRUMENT.md:62-68` explains why it was never run ("A0 needs three independent fits per target"). Of the two designed A2 searches only X14 completed (28,800 games); X24 was retired unrun, as were both k=1 fitted searches (X17, X28) — 9 of 31 designed searches completed. The paper contradicts itself at `12-discussion.tex:180-185`: "What has been built and measured is the synchronised regime, plus one correlated fit reaching +0.70 pp against a class floor of 2.31, which is nothing." ("Six adversary classes" is supported as a quotation of `ADVERSARIES.md:1177`.)

**The narrowest fix**: change only the regime clause, e.g. "…six adversary classes in the synchronised regime, plus one fitted search in the ex-ante-correlated regime, and measured channel ceilings before fitting anything against them." Do **not** adopt the filed wording "two correlation regimes and two seat counts": both fitted k=1 searches were retired unrun, so the fitted seat-count axis was designed but not exercised (phase 2's k=1 column is unfitted single-knob arms, a different object).

### 30. The 17×–39× design-effect range spans two channels; the sentence names one. — MINOR

**Location**: `paper/sections_v07/09-development.tex:156-159` (PDF p.32).

> "…a one-point-equivalent effect resolves in **284 to 560 games on the declaration channel** against 9,604 on the scoreboard, confirming the precision claim at **17 to 39 times**."

**The claim**: the declaration channel resolves a one-point-equivalent effect 17×–39× more cheaply than the scoreboard.

**What the source shows**: `research/v07/results/K4-designeffect.txt` gives, per arm, declAcc 284 (v06) and 560 (`v07:r12=25`), and allocErr **299** (v06) and **244** (`v07:r12=25`). 9604/560 = 17.15 and 9604/284 = 33.82, so the declaration channel alone is 17×–34×; 39 is 9604/**244**, the allocation-error channel, which the paragraph never mentions. The source's envelope is explicitly cross-channel and prints the full table one line above the ratio (`docs/v07/CANDIDATES.md:428-436`; the same one-channel juxtaposition is inherited from C7 at `:53`). The restatement at `12-discussion.tex:188-192` omits the ratio and is clean. Nothing load-bearing rests on it — the paper calls the estimator "confirmed and unused" — but it is a 16% inflation of the named channel's advantage that is not derivable from the paper.

**The narrowest fix**: restore the second channel, e.g. "…resolves in 284 to 560 games on the declaration channel and 244 to 299 on the allocation-error channel, against 9,604 on the scoreboard, confirming the precision claim at 17 to 39 times", with two new transcribed macros sourced to `CANDIDATES.md:432`. Do **not** simply change 39 to 34: 34 appears nowhere in the named source, so `check_provenance` would fail; the single-channel alternative requires dropping the ratio from the sentence entirely.

### 31. "Every contested rule choice is an engine switch" is contradicted by the appendix table it introduces, and the dialect-row count is attached to the wrong table. — MINOR

**Location**: `paper/sections_v07/02-game.tex:42-44`, `A-dialect.tex:4`, `A-dialect.tex:27` (PDF p.5, p.57).

> "Every contested choice is an engine switch, so its effect is measurable rather than assumed, and §11.7 measures the principal comparison under **eight of them**."
> "Every contested rule choice of §2.2 is an engine switch… This appendix lists each choice against the switch that changes it."
> "§11.7 measures the paper's headline comparison under **8 rows of this table**."

**The claim**: all eight clauses of §2.2 have a switch; and §11.7's eight dialect cells correspond to eight rows of the clause table.

**What the source shows**: `rulesFrom` (`engine/src/main.cpp:52-68`) — the only place `Rules` is populated from argv for `match` — exposes exactly `--arb`, `--legacy`, `--sets`, `--maxasks`, `--no-out-of-turn`, `--no-cardless-declare`: **six** of the eight clauses. The misdeclaration award is hardcoded (`engine/src/game.hpp:384`, `int awarded = correct ? team : 1 - team;`) with no field in `struct Rules` and no flag anywhere. The forced-endgame ladder has an isolable flag (`--forcedth`, `main.cpp:1299-1302`) but only inside the `coord` subcommand, absent from `rulesFrom`, so within the scored harness it moves only inside `--legacy` — which is why `11-results.tex:386` says the residual is assigned to the ladder "for want of a flag of its own". Both rows print `---` in the Switch column (`A-dialect.tex:19-20`, U+2014 in the PDF). Separately, `\vsevenDiaRows` = 8 is generated as the row count of the **dialect results table** (`build_tables_v07.py:1057`), whose rows are default, no-out-of-turn, no-cardless-declare, maxasks=360, arb=high, arb=turn, sets=8, legacy — one is the unmodified default, two come from one clause, and neither Misdeclaration nor Forced endgame appears. The sweep exercises **six** clause rows via seven non-default settings.

**The narrowest fix**: three wording changes. `A-dialect.tex:4` — "Six of the eight clauses of §2.2 are engine switches…; the misdeclaration award is hardcoded and the forced-endgame ladder moves only inside the `--legacy` bundle." `02-game.tex:42` — qualify "every" and say §11.7 measures the comparison under eight dialect *settings*, noting the two clauses that have no switch and were not varied. `A-dialect.tex:27` — "§11.7 measures the paper's headline comparison under `\vsevenDiaRows{}` dialect settings drawn from six of these clauses." Do **not** change the Switch column: there are no stray commas (the filed evidence's extractor mangled the em dash), and `---` already reads as "no switch". Do **not** change `\vsevenDiaRows` from 8, which is correct for the object it counts and is used in that sense in the abstract, §2.3, §12.1 and §13.

### 32. "The standard card-game reinforcement learning libraries" — one was checked. — MINOR

**Location**: `paper/sections_v07/01-introduction.tex:14-15` (PDF p.4).

> "…and no coverage in **the standard card-game reinforcement learning libraries** (§3.1)."

**The claim**: a plural class of standard card-game RL libraries was checked and none covers this game.

**What the source shows**: §3.1, the section cited, is a near-verbatim source for this sentence and names exactly one: `03-related.tex:22` — "no coverage in **RLCard** `\cite{rlcard}`". The substitution of a definite plural for one named, cited instance is the only difference between the two sentences. §3.1 also states the paper's own standard (`:24-26`): novelty statements "should be read against that scope." Exactly one card-game RL library is cited in the 72-page paper (`rlcard`, ref [9]); `grep -icE "openspiel|pettingzoo|pgx"` over the standalone returns 0. The only prior-art artifact recording a library-coverage check is `research/v04/lit/fish-prior-art.md:185` — RLCard, singular. This project's own v0.6 paper said "no coverage in RLCard `\cite{rlcard}`" (`sections_v06/03-related.tex:41`).

**The narrowest fix**: "no coverage in RLCard `\cite{rlcard}`", the exact wording of the section already cited. Do not use "the card-game reinforcement-learning library we reviewed" — the source names it, so the paper should.

### 33. `\vsevenLthirteenAdvCost` sits under the wrong provenance header. — MINOR

**Location**: `paper/numbers_v07.tex:159`, under the header at `:150`.

> "% docs/v07/CANDIDATES.md … `\providecommand{\vsevenLthirteenAdvCost}{17}`"

**The claim**: the 17 pp cost to an adversary of driving forced-endgame incidence upward is recorded in `docs/v07/CANDIDATES.md`, the artifact named by the nearest preceding header.

**What the source shows**: the value is correct and the sentence using it (`09-development.tex:197`) is sound, but the header names the wrong artifact. `grep -rn "17 point\|17 pp\|-17" docs/v07/CANDIDATES.md` returns nothing; every "17" in that file is unrelated. `CANDIDATES.md:97-99` names the route with no figure and closes it by a different argument (K3's change pre-empts it). The measurement is `docs/v07/ADVERSARIES.md:674` — "The construction that raises the target's is the adversary going cardless itself, and that was measured at **−17 points**" — corroborated at `:48` (A7). The file states its own convention at `numbers_v07.tex:185-192` ("a macro's source is the nearest preceding comment naming a path"), and `paper/check_provenance.py:15-27` verifies only that the named artifact exists on disk, so the check passes. It is also a phase-2 measurement under a block preamble reading "phase 3".

**The narrowest fix**: delete the line from `:159` and re-insert it inside the existing ADVERSARIES block at `:163-167`, immediately after the `% docs/v07/ADVERSARIES.md` header. That block is already terminated by a restated `% docs/v07/CANDIDATES.md` header at `:169`, so no comment lines need adding and `\vsevenFullGameOldN` (`:160`) keeps its correct attribution — it *is* a CANDIDATES.md number (`:221`, "one n=4,000 cell on one bank at 50.08% [48.52, 51.62]"). Do not touch the value or `09-development.tex:197`.

### 34. The bibliography header says the only editorial act was selection; 23 entries were edited. — MINOR

**Location**: `paper/sections_v07/bibliography.tex:8-10` (LaTeX comment; not rendered).

> "Every entry below is carried **VERBATIM**… **No entry has been added, edited or invented** while writing this paper — the only editorial act here is **SELECTION**…"

**The claim**: the only editorial act applied to the bibliography was selection.

**What the source shows**: a normalised entry-by-entry diff against `paper/sections_v06/bibliography.tex` (split on `\bibitem`, strip comments and `\url`, whitespace-normalise) shows 23 of the 71 surviving entries differ: 8 lost a trailing evaluative annotation sentence (goadrich2026valet, edelkamp2021paranoia, zhang2022teamsubgame, lerer2020sparta, milec2021cdr, lisy2017lbr, tian2020jps, kelidari2026goldstandard) and **21** lost their trailing `\url` line (the filed count of sixteen is wrong; it omits goadrich2026valet and edelkamp2021paranoia). The file's own history confirms it: `git show b71c0c6 -- paper/sections_v07/bibliography.tex` removes the annotations, `1e50474` removes the URLs, and neither diff touches the header — b71c0c6's commit message even claims "the header records the edit", which it does not. The exculpatory half is verified: **no metadata field changed anywhere** (every v0.7 entry is an exact prefix of its v0.6 text after url-stripping), and the "71 cited / 3 dropped" clause is exact (71 bibitems, 71 distinct cite keys, zero orphans, zero dangling).

**The narrowest fix**: keep the provenance and count sentences; replace "No entry has been added, edited or invented… the only editorial act here is SELECTION" with what the history shows — no entry added or invented, no metadata field altered (author list, title, venue, volume, pages, year and arXiv identifier identical to v0.6 for all 71), but two deletions beyond selection: evaluative annotations from eight entries (b71c0c6) and the trailing `\url` line from twenty-one (1e50474), each of which retains its arXiv identifier or venue. Do not write "sixteen" or "nineteen". `:12-15` is correct and needs no change.

---

## UNRESOLVED

### U-1. Whether the holdout seal genuinely predates the candidate architecture.

`07-evaluation.tex:§7.6` and §10 assert the banks were sealed "**Before any candidate existed**". The seed-registry entries for 7090001/2/3 and 7091001 land in the phase-1 commit d45b688, but the digests, `SEAL.json`, the sealed adversary half, the `runMatch` enforcement and three further seeds all land in **one commit, 0c021a3, "phase 2 v7: open-ended adversary generation"** — the same commit that contains phase 2's adversary population and the `r12=25` dose sweep the whole configuration rests on. `RESEARCH-LOG.md` puts the seal (§2.10) *after* the r12 discovery (§2.4). The README's own phrasing is weaker than the paper's ("before any candidate **architecture** exists"). All 15 selection candidates postdate the seal either way, so the selection-bias argument is intact; what is unresolved is whether the paper's stronger sentence is true. **What would settle it**: intra-commit ordering evidence outside git — a working-tree timestamp, a session transcript, or the author's own recollection recorded as a deviation.

### U-2. Whether S7's PASS is awarded on the point-estimate or the lower-bound reading of the 1.53 floor.

Two auditors filed this and two skeptics independently refuted it on the same argument: condition 7 triggers only under a *mixed* reading (plant supra-floor by point estimate, recovery by lower bound), and under either self-consistent reading it does not trigger — point-estimate: supra-floor rungs {0.08, 0.11, 0.15}, recovered excesses +2.38 / +9.42 / +12.64, all ≥ 1.53; lower-bound: the 0.08 rung's planted cost is +1.59 **[+1.05, +2.13]** so it is not an established supra-floor rung, leaving {0.11, 0.15} recovered at lower bounds +8.54 and +11.75. I accept the refutation. What remains unresolved is a disclosure question the skeptics themselves conceded: `\vsevenBnineHeightRecovered = no` exists in the generated macros (from `build_tables_v07.py:1104`'s `dlo > 1.53` test), the summary row at `11-results.tex:162` reads "not met. Every control behaves as specified" six pages before the qualification at `:465`, and the paper never states which reading its verdict is awarded on. **What would settle it**: a one-clause statement at `11-results.tex:162` or `C-thresholds.tex:44` naming the reading, or a decision to retire the unused macro.

### U-3. The policy version behind §2.2's turn-transfer and arbitration channel figures.

`02-game.tex` reports the cardless-transfer and declaration-arbitration channel costs (0.37 pp over 30,000 games; the clairvoyant arbitrator at +0.35 pp) without saying which policy generated the play. The auditor who checked the values found them correctly transcribed but the version unstated; the source study is v0.4/v0.5-era and is explicitly scoped to mirror play, and its probe resolves cross-team races differently from `Game` (`probe_declaration_game.hpp:270-273` vs global lowest seat), so the measured figure may be the within-team component only. This is the same class as finding 10. **What would settle it**: reading `research/v05/results/P6-verify-arbitration-cost.md` §§3-4 for the arm specs and deciding whether the v0.7 dialect's arbitration rule is unchanged enough for the figure to carry.

### U-4. Whether the primary decision rule may apply the 1.53 floor to a directly measured paired difference.

`10-design.tex:153` applies the 1.53 pp detection floor to the primary comparison, which is a directly measured paired difference. `docs/v07/PREREGISTRATION.md` §3 note 3 says "Nothing in the corpus shows that a responder-recovery floor bounds a directly measured difference", and `07-evaluation.tex:§7.9` correctly scopes the floor to exploitability statements. The auditor who found this declined to file it as out of assignment; no skeptic examined it. It touches the primary claim's decision rule, so it should not be left unexamined. **What would settle it**: reading PREREGISTRATION §3 note 3 and §5.1 together and deciding whether §10.2's use is the registered one or an extension needing a deviation row.

### U-5. Whether "endgame only" is a fair label for the search gate at `maxq=26`.

Figure 1 and §6.7 label the search "endgame only", firing at ≤26 of 54 unresolved cards. The only measured search-fire rate in the corpus (0.487–0.519) is **F-cheap's**, captured as side B; no capture of the frozen agent's own rate exists. If the frozen agent searches on roughly half of decisions, "endgame only" understates the gate's reach — which would also bear on finding 1's reading of the abstract's separate search item. **What would settle it**: one instrumented mirror run of the frozen spec recording the fraction of decisions at which the `maxq` gate opens.

### U-6. Whether "the advantage" in the abstract's robustness sentence has the right antecedent.

Reading the literal `argv` of every row: `P5-B6` is 40 cells `--b=v05` and 16 cells `--b=v06`; `P5-B7` is `--b=v05` plus three head-to-head pairs; `P5-B8` is `--b=v06`. **No cell in any of the three batteries is played against F-cheap.** So "The advantage persists…", immediately following the sentence stating the +3.33 pp F-cheap edge, carries a quantity none of the three batteries measures. Against this: the body uses the same locution for the v0.6 comparison (`11-results.tex:378`, "The advantage survives 48-card Literature…"), and the discussion and conclusion do the same, so a careful author can defend "the advantage" as the lineage advantage. I could not settle it and record it as a scope looseness rather than a defect. **What would settle it**: an authorial decision on whether "the advantage" is the F-cheap edge or the lineage advantage, applied consistently across the abstract, §12.1 and §13.

---

## UNDERSTATEMENT

**None survived.** One finding was filed under this label — that §11.2.3's selection check reports "3 of the four reproduce" while two further artifact-derived comparisons exist that would make it five of six — and its skeptic refuted it on the paper's own sign rule (`build_tables_v07.py:1470`, `if ho - tr < 0: short`): one of the two additional comparisons is −0.192, i.e. a shortfall, so including both would move the tally from 3-of-4 (75%) to **4-of-6 (67%)**. The omission therefore does not understate; the caption already discloses non-exhaustiveness in the unfavourable direction ("These are not every figure the protocol states in advance, and two of the partner-regime predictions… also come in low"). I do not reinstate it.

Two confirmed defects do err *against* the paper's interest and are noted here so they are not read as inflation: finding 22 (four → five) understates by one arm how far the B9 control set duplicates the B4 objective, and finding 24 (σ√(2 ln K)) prints a bound that makes the 0.63 pp selection shortfall look more ordinary than the exact expectation would. Both remain errors and both should be fixed.

---

## What the audit tried hardest to break and could not

The three headline numbers. Fourteen auditors recomputed `research/v07/results/P5-B2.jsonl` independently, and the primary result is +3.3271 [+2.8778, +3.7764] over 48,000 games with +3.667 / +2.987 per bank, and the composite cell +0.1458 [−0.2947, +0.5863] with +0.133 / +0.158 — both reproduce the printed digits exactly, and both are generated macros, so failure mode (c) has almost no surface there. The whole B5 attribution lattice, all 124 pooled B3 panel cells, minimax regret, the eight partner deltas, every B7 and B8 cell, all eight B4 arms, all four B9 rungs, every B10 residual count, the 428 scored cells / 4,322,400 games accounting, all 27 deviation rows, all 31 panel members with their class assignments, the seven bank digests, MANIFEST-P5's 29 runs re-verified by SHA-256, and `check_provenance`'s 250/24/121 split all hold. So does the scope limit, which is the paper's strongest section: I could not find a single sentence in 72 pages claiming near-optimality, unexploitability or global standing, and §12.2, §12.4, §12.5, §12.6 and Appendix E state negative and self-damaging results at or beyond the strength the artifacts require — E.2 volunteers four corrections against the author's own record that no reader would have found. The central qualification is carried at full strength in six places. Every registered battery is reported and no registered cell was dropped. The two prior passes' work on §5 and §6 held: no auditor found positive evidence of error in the inference equations, the Sinkhorn deployment, the feature score, the tie-break, the declaration rule, the stall detector or the determinized search.

## Where this audit is thin

Nothing was re-executed. No bank digest, no `fish7 verify`, no `selftest`, no sealed-half SHA-256, and no scored cell was re-run; every recomputation above is a re-reduction of recorded `match --json` objects, so a defect inside the harness would be invisible to the entire audit. Three probes were built (belief mode, declaration legality, throughput) and one binary was exercised; everything else about the agent rests on source reading plus the supplied ground-truth memo. The correlation structure between B8 dialect cells and between B5 lattice cells is unmodelled by the generator, by FINAL-RESULTS and by us, so finding 23 stands only under an independence assumption the artifacts partly refute. No reference in the 71-entry bibliography was checked for existence — four carry 2026 arXiv identifiers taken on trust — and only two works' contents were read directly. The 121 hand-transcribed numbers were not swept as a class for version mismatch; auditors checked the ones their sentences leaned on, and three of the confirmed findings above (7, 8, 10) are version or phase mismatches found that way, which suggests the remaining transcriptions deserve a dedicated pass. Sections 7 and 8 were screened by grep rather than read line by line. Appendix D's 31 rows and Appendix G's hashes were checked for consistency, not regenerated. Fifteen transcribed macros are defined and unused, and were not verified — one of them, `\vsevenMisdeclAfter = 1.028`, is a K3-stack figure against a measured v0.7 mirror rate of 2.556%, and would be a defect the moment it is used. And nobody adjudicated U-4, which touches the primary claim's own decision rule.