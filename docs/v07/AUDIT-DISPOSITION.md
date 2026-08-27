# Disposition of the pre-submission audit

Companion to `AUDIT-REPORT-PRESUBMISSION.md`. That document records what the independent adversarial
audit found; this one records what was done about it, and what was deliberately left alone.

All 34 confirmed findings (1 BLOCKING, 19 SERIOUS, 14 MINOR) are fixed. The six UNRESOLVED items are
untouched: each needs a decision or a measurement the repository cannot supply, and guessing at one
would be the failure mode the audit exists to prevent.

## How the fixes were checked

Every fix was applied as the report's "narrowest fix" prescribed, and every explicit guard in the
report ("do NOT do X") was honoured. The load-bearing evidence was re-verified locally before the
edit, not taken from the report:

- **Finding 1** was confirmed with a probe built through the engine's own factory
  (`scratchpad/probe_belief.cpp`): the frozen spec constructs with `belief=Fast`,
  `sinkOuter=4 sinkInner=8 priorTheta=0.37062 priorPhi=0.14525`, and plain `v06` constructs
  identically. `ensureExactMarginals` has one definition and zero call sites.
- **Findings 3, 22, 25** were re-derived from `engine/src/game.hpp` and
  `research/v07/results/P5-B4fits.jsonl` / `P5-B7.jsonl`.
- **Findings 5, 23** were recomputed from `research/v06/results/E8-belief.txt`,
  `F2-belief-noprior.txt` and `research/v07/results/P5-B8.jsonl`; the two-component residual half-width
  reproduces the report's 1.2544 exactly.
- **Finding 2** was checked against `research/v07/results/T1-throughput.jsonl`, and the battery's
  recorded wall time recomputed at 24.12 h over the 428 scored cells, 26.76 h including the fits.
- **Finding 9**'s unrestricted result was re-pooled from `K5-vs-v06-703000{1,2}.jsonl` at
  −1.8333 [−2.2193, −1.4473].

The applied fixes were then attacked by seven independent reviewers, which found **30 defects in the
fixes themselves** — four of them over-corrections or new false claims of exactly the kind the audit
brief warns about (finding 20's replacement reinstated a universal; finding 7's replacement attached
the wrong configuration to the discharge and inverted what phase 1's +2.19 did; finding 3's collateral
repair promoted Develin's *tactic* D8, "no risk at all in holding", into a rule of his dialect;
finding 2's "over two days" replaced an 11%-wrong figure with a 2×-wrong one). All 30 were fixed.

## Fixes that changed more than prose

| Where | What |
|---|---|
| `engine/build_tables_v07.py` | Table 2's deal-inference and contestation rows (findings 1, 4); the legacy residual's variance now excludes the bit-identical ask-cap cell (finding 23), giving [−1.55, +0.96] at 8.6× instead of [−1.69, +1.11] at 9.6× |
| `paper/numbers_v07.tex` | 19 new transcribed macros, each under a header naming the artifact it comes from; `\vsevenLthirteenAdvCost` moved from the CANDIDATES.md block to the ADVERSARIES.md block it belongs to (finding 33) |
| `paper/sections_v07/E-corrections.tex` | New entry reconciling the residual interval this paper prints against the one the phase-5 recording publishes |
| `paper/sections_v07/08-threat-model.tex` | New paragraph naming the three correlation regimes, so §12.7's premise and §12.4's "synchronised regime" have a referent (finding 27) |

## The three residual risks the audit named, and what was done about them

The audit's own "where this audit is thin" section named three. All three were worked, after the
34 findings were closed.

### 1. "No reference was checked for existence"

All **71** were, against the arXiv abstract page, the publisher record, or the archived page itself.
Sixty-two were confirmed correct in every printed field. **Eight carried a metadata error, every one
of them inherited from the vetted v0.6 bibliography rather than introduced here**, and four were
author lists:

| entry | printed | source |
|---|---|---|
| `hu2021lbs` | five authors, Hu / Wu / Lerer / Foerster / Brown | arXiv:2106.09086 has four: Hu, Lerer, Brown, Foerster. The printed list is the adjacent entry's, and that entry's own list is correct |
| `rlcard` | nine authors | arXiv:1910.04376 has seven; the nine are the later IJCAI-20 "Platform" paper's |
| `zhang-tbdag` | includes A. Celli | arXiv:2202.00789 has three authors and he is not one |
| `somani` | "2018--" | GitHub API: repository created 2019-06-10, last push 2025-09-22 |
| `acpc` | pages 112--118 | AI Magazine 34(2):112--114 |
| `develin` | containing work "Card Games" | the archived page is titled "The Ten Best Card Games You've Never Heard Of" |
| `dorsa` | "Deposit Genius" as author | the byline is D. Dorsa; Deposit Genius is the site |
| `lisy2017lbr` | 2017 | arXiv:1612.07547 v1 posted 22 December 2016 |

All eight are corrected, the bibliography header now records them, and Appendix E carries the
correction in the paper's own idiom. **One entry remains unresolved**: `strategy-site`
(`canadian-fish.vercel.app/strategy`, dated 2026) is a single-page app that serves the same shell for
every path, so its date cannot be established from outside. Nothing in the paper rests on it.

The three 2026 arXiv identifiers the audit flagged as "taken on trust" all resolve, with matching
titles, author lists and years: `2603.03252` (Valet), `2601.17131` (Kubicek et al.), `2607.06854`
(Kelidari et al.).

### 2. "Nothing was re-executed"

Now re-executed on this machine, against the frozen binary:

| check | result |
|---|---|
| `engine/fish7` SHA-256 and byte count | `cf6d5ea2…f4e3`, 1,242,312 bytes — both as printed |
| `fish7 verify` | `audit violations: 0 / 6737436 checks`, determinism PASS — exactly the figures §10.2 prints |
| seven bank digests, 24,000 deals each | all seven reproduce against the commitments recorded at sealing |
| sealed adversary half | plaintext SHA-256 matches `SEAL.json`'s commitment; 14 rows decode |
| freeze artifact round-trip | `VERIFY PASS`, digest `5f81f440fc9c272a87e87c05fecc7b74` |
| the seal itself | naming a sealed seed without `FISH_UNSEAL_PHASE` **fails**, as Figure 2's caption claims |
| **scored cell B2.2** (v0.7 vs `F-cheap`, bank 7090001), re-run from its literal `argv` | `winRateA` 0.536667, CI [0.530333, 0.543042], 24,000 games — **every field identical to the recorded row** |

The last is the paper's primary comparison, re-executed end to end rather than re-read.

### 3. "The transcribed numbers were never swept as a class"

Swept, all of them. `paper/numbers_v07.tex` declared 153 macros; 129 of those are hand-carried and
actually reach the page, and **all 129 were checked** against their named artifact for four things at
once: the value, the source, the policy version and phase, and whether the sentence's subject is the
thing the number measures.

| | count | rate |
|---|---:|---:|
| checked | 129 | — |
| version, phase, arm, rung, cell or measurement mismatch | **9** | 7.0% |
| — the number is wrong for the sentence's subject | 6 | 4.7% |
| — the number is right and the attribution is wrong | 3 | 2.3% |
| unsettled by the artifacts | 2 | 1.6% |
| clean on all four | 118 | 91.5% |

**Not one of the 129 is a numeric transcription error.** Every value is present in its named artifact
and every macro satisfies the nearest-preceding-header rule, which is why `check_provenance` passed
all of them. What failed is the pairing: a correct number attached to a different handicap rung, a
different leaf arm, a different cell of the same table, a different phase, or a metric glossed with
the wrong definition. That is the failure mode the audit brief called (c), and it is invisible to any
automated check this repository has.

Four of the nine are inherited verbatim from a project document that was already wrong
(`PREREGISTRATION.md:466` and `:93`, `C1-v04-corrections.md:917`, `CANDIDATES.md:57`), so the paper
was faithfully reproducing an upstream error in each case. All nine are fixed, and the five that
correct the record rather than only the prose are written into Appendix E.

Two were left unsettled by the artifacts and both were resolved by other means. The forced-endgame
adversary's "&minus;17 points" has no cell anywhere in the results tree --- no recorded spec describes
that construction, and the only measurement near &minus;17 is an unrelated belief ablation --- so the
paper no longer prints it as a measured cost and says instead that the route is closed for want of a
mechanism. And the leaf evaluator's "~1.3M leaves" could not be reproduced from any union of the K1
row counts; its unreproducible half is not load-bearing, but the half that was --- attaching a
fit-plus-evaluation total to an out-of-sample claim --- is corrected.

### A measurement rather than a hedge

One of the nine could be fixed either by qualifying the sentence or by measuring the right thing. The
live-ask gate's binding rate was the *incumbent's*: the phase-2 characterisation captures the target
arm, so it measures v0.6, and the gate acts on the argmax of an ask score that FishBot v0.7
extends, which makes it a different quantity. Rather than print the incumbent's number with a
footnote, the frozen configuration was captured from its own seats on the same diagnostic bank:

```
fish7 v7decide --a='<frozen spec>' --b=v06 --capture=a --games=3000 --rotations=2 --seed=7050001
  -> gateBindRate 0.014108 [0.013494, 0.014716] over n = 269,423 ask decisions
```

recorded as `research/v07/results/P5-mech-arm-a.jsonl`. The gate binds at 1.4% on the deployed policy
against 1.0% on the incumbent, so the hedge would have been misleading in a way the measurement is
not. Seed 7050001 is a registered diagnostic seed, not sealed material, and no scored quantity moves.

### A certificate re-checked because a fix depended on it

Appendix B now states that the extended responder block is all zero but one, and that all-zero is the
identity that reproduces v0.6 bit for bit. The stored certificate,
`research/v07/results/G1-identity.txt`, says "twelve responder coordinates" --- a stale label from
when the block was smaller than the eighteen it now holds. Re-run against the current engine, `v07`
at its defaults against `v06` is 0.500000 with a zero-width interval and every derived statistic
byte-identical, so the identity covers all eighteen and the appendix's claim is sound.

### A fourth item, now settled

The adjudicator left "the battery ran over two days" (§11.10) unresolved for want of a timestamp.
File mtimes across the phase-5 artifacts span **56.1 hours (2.34 days)**, from `P5-mech.jsonl` on
2026-08-24 to `P5-drift.json` on 2026-08-26, against 26.8 hours of recorded per-cell compute. The
caption is about elapsed span and is correct; the compute figure is a different quantity. No change.

## Left alone deliberately

- **U-1 … U-6**, the audit's unresolved items. U-1 (seal ordering) needs evidence outside git; U-4
  (whether the 1.53 floor may be applied to a directly measured paired difference) needs a reading of
  the preregistration that is the author's to make; U-5 needs one instrumented run.
- **`\vsevenSelExpectedMax` = 0.74**, the registered selection bound. The audit was explicit that the
  figure must not move after the data were seen; only the prose around it changed.
- The **duplicate battery table** (Table 7 in §10.5 and Table 26 in Appendix G print the same
  content under different captions) and the **mixed appendix table style** (numbered floats in the
  body, inline uncaptioned tables in Appendices A–D and F). Both are structural choices, not defects.
