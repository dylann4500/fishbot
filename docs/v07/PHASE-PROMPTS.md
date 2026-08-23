# FishBot v0.7 — phase prompts and session settings (paper-informed, final)

Six sessions. Each starts with a cleared context, reads the files the previous phase wrote, and
ends by writing its own. Commit between phases. This version supersedes the pre-paper draft: it
incorporates the v0.4/v0.5/v0.6 papers' own findings — the inherited search conditional, the
declaration-accuracy channel, the resolution economics (half-width ~98/sqrt(N) points), the
closed register, the in-class-only exploitability probe, and the partner-transfer baseline.

Ultracode is triggered by the literal word `ultracode` in a prompt; `+NNNk`/`+1M` sets the turn's
token target. Both are embedded in the prompts — send them verbatim.

| Phase | Model | Effort | Ultracode | Workflow size | Permission mode | Budget |
|---|---|---|---|---|---|---|
| 0 Threat model | Opus 5 | max | ON | medium | plan / read-only | +300k |
| 1 Instrument | Opus 5 | max | OFF | — | bypass | — |
| 2 Adversaries | Opus 5 | max | ON | large | bypass | +1M |
| 3 Candidates | Opus 5 | max | ON | large | bypass | +1M |
| 4 Bake-off | Opus 5 | max | ON | medium | bypass | +1M |
| 5 Final eval | Opus 5 | max | ON | medium | bypass | +1M |
| 6 Report | Opus 5 | max | OFF | — | accept edits | — |

## Gates and loop-backs

- P0 -> P1: both files committed; the white-box decision made; the ledger ranked with citations.
- P1 -> P2: a responder recovers planted edges at the sizes v0.7 will claim; >=10x search
  throughput. If not, repeat phase 1. Never proceed on a weak instrument.
- P2 -> P3: taxonomy written; train/holdout split committed. If nothing clears the floor,
  phase 3 still runs — the breakthrough case then rests on beating the frontier, not closing
  exploits.
- P3 -> P4: at most two survivors; everything else killed with numbers in RESEARCH-LOG.md.
- P4 -> P5: frozen JSON + preregistration committed before anything holdout is touched. If
  phase 5 tempts any adjustment, that is phase 4 reopening: amend the prereg, draw fresh
  holdout banks, re-run.
- All-negative path: phases 4-6 still run with v0.6 frozen as incumbent; the barrier report is
  a legitimate v0.7 deliverable (the task prompt's option 2).

The binding resource is simulator time on the search configuration, not tokens; phase 1 exists
to buy it and is the one phase run without fan-out.

---

## Phase 0 — Threat model and lineage re-audit

Read the full FishBot lineage: docs/, paper/ (the v0.4 and v0.5 standalone papers and
paper/sections_v06/), research/, engine/src/. Re-evaluate prior conclusions rather than assuming
they hold — but distinguish re-auditing from re-building: check power, provenance, and artifacts;
do not propose re-running mechanisms the corpus closed unless you find a specific defect in the
closure. Review external literature on imperfect-information team games, best-response and
exploitability estimation, search with belief-limited evaluators, and cooperative game AI.
ultracode +300k

Produce exactly two files and no other changes:

1. docs/v07/THREAT-MODEL.md — an operational definition of exploitability for a homogeneous team
   of three v0.7 agents in six-player Fish. Specify: how many seats the adversary controls (the
   corpus's LBR probe uses a full opposing team of three identical responders — keep, extend, or
   justify an alternative); what the adversary knows (standard exploitability grants it the
   target's exact policy — decide whether v0.7's adversary is white-box, and note that every
   FishBot to date is deterministic, so a white-box adversary can in principle invert the public
   transcript against the known policy and shrink the deal posterior below what the certificates
   alone imply; no prior study has tested this); which policy classes the adversary may draw from
   (in-class linear, extended features, search-based, learned, scripted-adaptive); and what counts
   as an illegal side channel for the v0.7 team itself, defined mechanically in terms of the
   engine's action space. For each rejected alternative definition, state what it would and would
   not detect. Note that head-to-head worst case saturates at the mirror (50% by construction), so
   exploitability under this definition is the axis on which "strongest achievable team" claims
   must be made.

2. docs/v07/SUBOPTIMALITY-LEDGER.md — every candidate source of remaining exploitable weakness,
   ranked by expected gain times confidence, each with its evidence and the cheapest experiment
   that would confirm or kill it. Seed it with the leads the corpus itself names, cited to the
   v0.6 paper: (a) test-time search is the strongest configuration measured and ships off only
   for cost and unresolved attribution, explicitly gated on rebuilding the leaf evaluator (§8,
   §13, conclusion); (b) declaration allocation — the whole of v0.6's +0.9 head-to-head is
   declaration accuracy, 2.37% of mirror declarations remain wrong, and forced-endgame accuracy
   reached 24.35% against a measured ~40.6% feasible ceiling (v0.5); (c) the joint posterior
   carries real signal that every marginal integrates away (the twelve-deal ensemble result),
   yet asks and declarations both consume marginals; (d) the per-decision objective the v0.6
   conclusion names as the only route to sub-quarter-point mechanisms; (e) determinism and
   invertibility, if the threat model admits white-box adversaries; (f) the partner-transfer gap
   (v0.6's self-play advantage does not survive a partner change). Import the CLOSED register
   with citations so later phases do not re-litigate it: exchangeable ties, the exact-posterior
   matched-budget refit, the chain/threat pass, the three extra ask terms. For every null you
   audit, state the resolution it was measured at (half-width ~98/sqrt(N) points) and whether it
   excludes quarter-point effects — most v0.6 nulls are null at ~1 point, not 0.25.

Fix the incumbent: the baseline to beat is the v0.6 FRONTIER — v06 at ~300 games/s AND
v06:search, the strongest policy measured, three orders of magnitude slower. v0.7's claim must
dominate that frontier, not just the fast point.

Constraints: propose no architecture, write no engine code, change no policy.

---

## Phase 1 — Rebuild the measuring instrument

Read docs/v07/THREAT-MODEL.md and docs/v07/SUBOPTIMALITY-LEDGER.md, then engine/src/ (arena.hpp,
tuner.hpp, the v05/v06 policies, the probe harnesses).

This session builds measurement and throughput infrastructure only. No policy changes.

Context: the corpus's only exploitability instrument fits a responder from the same 34-coordinate
linear family with the frozen target as its entire panel. Its positive control reproduces v0.4's
published 51.19% within the interval, it reaches parity against v0.5, and 48.36% against v0.6 —
so it works, but its detection floor is around a point, it is blind to out-of-class exploits by
construction, and a response that fails to reach parity proves only that this search did not find
the exploit (v0.6 §11, §13).

Build three things:

1. Stronger responders, per THREAT-MODEL.md's adversary classes: (a) the in-class fitter at
   higher budget as baseline; (b) an extended-class responder (wider features or different
   structure); (c) a search-based responder — the v06 rollout machinery pointed at exploitation;
   (d) if the threat model admits white-box adversaries, a transcript-inversion responder that
   uses the target's known deterministic policy to shrink the deal posterior beyond what
   certificates imply. No prior study has tested whether that bites; either answer is a result.

2. Calibration by planted weakness: construct target policies with deliberately installed edges
   of known, varying size (the corpus's own handicap test — zeroing the hit-probability weight —
   is the pattern). Report each responder's detection floor with CIs: the smallest planted edge
   it reliably recovers. A responder that cannot recover a planted two-point edge contributes
   nothing to an exploitability claim, and the report must say so.

3. Throughput, aimed at the named bottleneck: v0.6 could not resolve where its search's gain sits
   because the search configuration runs ~3 orders of magnitude slower than the deployed policy
   and attribution needed roughly an order of magnitude more games (§8). Parallelise the arena
   across cores, shard seed banks, and build the fast-search path: truncated rollouts behind a
   leaf-evaluator interface (the evaluator itself comes later; the interface and truncation come
   now), the cheap-blueprint frontier the corpus measured, batch evaluation. Add the per-decision
   evaluation channel the v0.6 conclusion calls for: per-decision records so mechanisms can be
   scored on decisions, not only games. Report games/s for every configuration, replacing E9.

Register new seed banks in the reserved-seeds registry; keep the commit-gate-first battery
ordering.

Exit criterion: at least one responder recovers planted edges down to a stated size at or below
the effect sizes v0.7 will claim; the search configuration is measurable at 10x or more of
v0.6's games/s; the 98/sqrt(N) power arithmetic is wired into harness output. If the exit
criterion fails, say so and stop rather than proceeding on a weak instrument.

Write docs/v07/INSTRUMENT.md (detection floors per responder class, throughput table, what the
white-box responder found) and append to docs/v07/RESEARCH-LOG.md.

---

## Phase 2 — Open-ended adversary generation

Read docs/v07/THREAT-MODEL.md, INSTRUMENT.md, and RESEARCH-LOG.md. ultracode +1M

Search for strategies that exploit the v0.6 frontier — both v06 and v06:search — including
strategies outside the thirteen scripted archetypes. The archetypes are evaluation tools, not the
definition of strength; the corpus itself says the deception archetypes are three stylised
hand-built manoeuvres and the wider deception space is unmeasured.

Run many exploiter searches that do not share a search bias: vary the responder class (in-class,
extended, search-based, white-box/inversion, scripted-adaptive), the objective, the seeds, and
the structural hypothesis — each SUBOPTIMALITY-LEDGER.md entry is a hypothesis (attack
declaration allocation; attack the policy prior the feint already attacks; attack partner
coordination; attack the search's assumptions). Fifteen runs of one search are one run.

Every exploiter that clears its responder class's detection floor gets characterised, not just
recorded: what mechanism it attacks, what v0.6 does wrong against it, whether it is distinct or
a known weakness found again. Cluster by mechanism, not by score. An exploiter that works only
through a harness defect is a finding about the harness — file it as such.

Then seal the evaluation material, physically, before anything can be tuned against it:
  research/v07/banks/train/    — seed banks usable in phases 3-4
  research/v07/banks/holdout/  — sealed until phase 5
and split the adversary bank the same way (train half / sealed half). Register all seeds in the
reserved-seeds registry. Record the split in RESEARCH-LOG.md.

Deliverable: docs/v07/ADVERSARIES.md — taxonomy by mechanism, severity per cluster with CIs, and
how much of the frontier's measured exploitability each cluster accounts for. If the honest
result is that nothing beats the frontier beyond the detection floor, say so plainly — that is a
result, and it reshapes phase 3.

---

## Phase 3 — Candidate architectures

Read THREAT-MODEL.md, INSTRUMENT.md, ADVERSARIES.md, SUBOPTIMALITY-LEDGER.md. Use only
research/v07/banks/train/; do not read the holdout bank. ultracode +1M

Develop three to five candidate architectures for v0.7, each keyed to evidence, and kill
aggressively. The corpus already names the primary bet, so this is not a blind sweep:

(a) The inherited conditional: rebuild the leaf evaluator — a value function that is not
    "algebraically close to a rescaling of the hit probability" — then re-open test-time search
    on top of it: truncated rollouts, variance-reduced returns, the guarded LCB deviation rule
    the corpus validated, at phase 1's fast-search throughput. Target: the strength of
    v06:search at deployable cost, or more.
(b) Joint-posterior consumption beyond ask ties: v0.6's entire gain was declaration accuracy,
    and the corpus proved the joint posterior carries signal marginals integrate away. Attack
    declaration allocation (2.37% wrong in mirror; forced endgame at 24.35% against a ~40.6%
    feasible ceiling) with joint sampling rather than per-card argmax.
(c) Whatever phase 2's strongest exploiter cluster indicts: if the white-box inversion responder
    bites, a stochastic policy trading a measured amount of blueprint strength for
    unreadability; if a coordination exploiter bites, explicit partner reasoning under the
    legal-information constraint.
(d) A per-decision-objective refit of a widened policy class, using phase 1's per-decision
    records — the corpus's stated route to sub-point mechanisms.
(e) One wildcard structurally unlike the lineage (e.g. a learned policy/value from large-scale
    self-play, per docs/METHODOLOGY.md's roadmap), if a cheap probe justifies it.

Each candidate is developed in an isolated worktree to a measurable state, then scored with
phase 1's instrument against the v0.6 frontier (both configurations), the phase-2 training
adversaries, and the archetype panel — paired, respecting 98/sqrt(N), never headlining an
aggregate: worst case and minimax regret lead, per the project's standing rule.

Constraints: three copies coordinating through legal play only — checked mechanically against
THREAT-MODEL.md's side-channel definition, not by inspection. Success for this session is
narrowing with evidence, not a winner. A candidate killed for an interesting reason goes into
RESEARCH-LOG.md with its number.

Deliverable: docs/v07/CANDIDATES.md — premise, result, and for survivors, what would have to be
true for each to be the answer.

---

## Phase 4 — Bake-off, iteration, and freeze

Read CANDIDATES.md, ADVERSARIES.md, INSTRUMENT.md. Training banks only. ultracode +1M

Iterate the surviving candidates against the mechanisms the exploiters revealed, not against the
exploiters: after each fix, re-run adversary search against the improved policy and check whether
the weakness closed or merely moved. Keep the commit gate ordered before strength — the corpus
contains configurations that score higher while playing 15% dead asks, and the gate is what
catches them.

Resolve attribution at adequate power: v0.6 shipped its search off partly because it could not
afford to locate its own gain. v0.7 must not headline a gain it cannot attribute — budget the
games; phase 1's throughput exists for this.

Check for brittle self-play conventions against the corpus's own baseline: v0.6 gains +2.25 in
self-play and -0.8 to +1.4 under partner change. For every survivor, run the partner-regime
table (copies of itself / previous versions / archetype partners) and cross-play between
independently-trained runs of the same architecture. The target configuration is three copies,
so self-play coordination is legitimate — but a convention that collapses in cross-play is
brittleness, and the report must be able to say which one it has.

Before the session ends, in order:
1. Freeze one configuration to JSON under engine/ (freeze_config_v07.py with a round-trip
   assertion, per project convention). No tuning after this point.
2. Write docs/v07/PREREGISTRATION.md: the exact phase-5 battery — opponent populations including
   the sealed adversary half and holdout banks by name, sample sizes with the 98/sqrt(N)
   arithmetic shown, paired designs, every claim re-run on a second disjoint bank, ablations,
   negative controls (planted-weakness recovery; a configuration the commit gate must reject),
   the partner-regime and rule-dialect tables, and pass/fail thresholds — committed before any
   holdout result is known. State in advance what result would mean v0.7 is not an advancement.

If nothing dominates the v0.6 frontier on the training adversaries, freeze v0.6 as the incumbent
and write the preregistration anyway — the well-measured negative is then the deliverable.

---

## Phase 5 — Frozen final evaluation

Fresh session. Inputs: the frozen v0.7 configuration under engine/, docs/v07/PREREGISTRATION.md,
and the engine. Do not read CANDIDATES.md or the training logs — the preregistration is the only
protocol. ultracode +1M

Run the preregistered battery exactly as written: sealed holdout banks and the sealed adversary
half for the first time; commit gate before any strength number; paired designs; every claim
re-run on a second disjoint bank; a fresh adversary search against the frozen configuration,
including the white-box responder; the negative controls; and the prereg's selection-bias check
— how much of the measured gain would be expected from selecting the best of N candidates under
the null.

No tuning of any kind — policy, hyperparameters, or evaluation. If you find a genuine flaw in
the preregistered protocol, stop and report it; an amended protocol is a training run. Any
deviation is recorded as a deviation.

Report every prespecified result including failures, with intervals throughout, worst case and
minimax regret headlined rather than aggregates. Raw output to research/v07/results/ with a
MANIFEST (build_manifest.py pattern); plain recording, no interpretation, to
docs/v07/FINAL-RESULTS.md.

---

## Phase 6 — The v0.7 technical report

Read everything under docs/v07/ and research/v07/, plus paper/sections_v06/ and
paper/fishbot_v06_standalone.tex for structure, register, and the evidence standard.

Write the FishBot v0.7 technical report at or above the v0.6 standard: sectioned tex under
paper/sections_v07/, every number generated into tex by a build_tables_v07.py from the artifacts
(no hand-typed numbers), provenance checked with paper/check_provenance.py, a
corrections-register section stating any corrections to the v0.4-v0.6 record this cycle found,
and the project's byline conventions.

The paper must make clear: what v0.7 changes; which hypotheses were explored and rejected, with
the numbers that killed them; the breakthrough, if one exists, stated no more strongly than the
evidence; strength and exploitability against the v0.6 frontier, with intervals; whether the
improvement survives unseen opponents and fresh adversarial search; which weaknesses remain,
named; and exactly what evidence would still be required before v1.0 could credibly be called
near-optimal or the strongest known Fish-playing team. Never headline an aggregate over the
panel — the worst case and minimax regret lead. The partner-regime table appears whether or not
it flatters v0.7.

When the draft is complete, spawn one adversarial reader whose only task is to attack every
claim against the artifacts and flag each place the prose outruns the evidence. Fix what it
finds; record what you disagree with and why.

If the honest conclusion is that v0.7 is not a meaningful advancement, the report says that, and
its contribution is the closed directions and the named barrier.
