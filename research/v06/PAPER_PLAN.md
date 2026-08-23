# FishBot v0.6 — paper plan

Target: a technical report in the shape of the v0.4 paper (14 body sections, 9 appendices, ~70
pages), built by `npm run paper:v06`. Skeleton: `paper/fishbot_v06.tex`, `paper/sections_v06/`.
Numbers macros: `paper/numbers_v06.tex` (placeholders) and `paper/numbers_v06_generated.tex`
(written by `engine/build_tables_v06.py` from `research/v06/results/`).

## The claim structure

v0.6 is **not** a paper that says "the new bot wins more". Three of its four contributions are
negative or methodological, and the paper is honest about that from the abstract onward. The
positive claim is narrow, replicated, and on the axis the project cares about.

### C1 — The 55% tie channel is not a defect; it is exchangeability
*Sections 6, 8. Evidence: `fish v6probe --mode=ties` (E8).*
55.6% of v0.5's ask decisions end in a bit-for-bit tie at the top of its score. 94.0% of those ties
are two cards of one half-suit at one target. The **exact** posterior separates **0.00%** of them,
and every tie-break rule — array order, the Fast argmax, the shipped policy's own chain/threat pass,
the exact argmax — realises the same 43.75% hit rate. The 70.68% hindsight figure prices
clairvoyance. Corollary: the "+3.16 sets/game of headroom" that a hindsight ask-oracle appears to
show is unreachable in principle, and the two corrections it forces on the v0.5 record are that
array order decides 19.2% of contested decisions rather than 55%, and that the channel is the
**card** dimension, not the target dimension the v0.5 study headlined.

### C2 — The exact posterior is a worse predictor than the deployed approximation
*Section 5. Evidence: `fish v6probe --mode=belief` (E8).*
On 118,616 unresolved cards from identical states: exact (uniform deal prior) mean NLL 1.42300 and
argmax hit 47.04%; Sinkhorn with θ = φ = 0, 1.39437 and 47.96%; the shipped policy prior, 1.38358
and 50.12%. The policy prior is worth +2.16 points of argmax hit rate and is the entire difference.
This settles the corpus's oldest open question — whether a matched-budget refit under the exact
belief would pay — in the negative, and it re-frames the v0.4 study's "−6.20 points for exact
inference" as a statement about the *prior*, not about exactness. It also shows the predictive
optimum (θ ≈ 0.6–0.8) is not the playing optimum (θ = 0.44458 fitted; θ = 0.60 costs 4.4 points),
so belief and policy must be fitted jointly.

### C3 — Determinized search: the optimizer's curse is worth 27 points
*Section 8. Evidence: R11, E5.*
A determinized information-set search — the deal sampled from the exact posterior, the six players
reconstructed at their own information sets, not a double-dummy rollout — is **27 points worse than
the blueprint it searches from** when the action is chosen by the argmax of the rollout means, and a
paired lower-confidence-bound rule recovers all 27 monotonically in the deviation threshold. The
control that isolates it: the same code with the blueprint forced to decide scores 50.83%. Guarded,
the search converges to the blueprint and is shipped **off**. Reported as a property of determinized
evaluation in this game: one determinization's return is the final half-suit differential with
sd ≈ 2.5, against candidate differences an order of magnitude smaller.
The deliberate miss is analysed here too: it is the move class M1 deleted, it exists at 79.2% of
decisions, the linear score provably cannot price it (its output is bit-identical at every admission
margin), and unbanning it fails the pathology gate at 7.5% action-limit games.

### C4 — The plateau, and the evidence standard that hid it
*Sections 7, 9, 11. Evidence: R12 §6.1, E5.*
Five mechanisms cleared a 95% paired interval at 400–1,600 games per cell and returned exactly zero
at 3,000, on two disjoint banks. Single-coordinate and single-feature perturbations of v0.5's policy
class are worth nothing. Combined with the finding (R4) that v0.5's own 40-generation fit moved its
objective by t = 1.60, this says the v0.4 → v0.5 → v0.6 sequence has been measuring seed draws, and
it is the reason v0.6 rebuilt the harness before it built any mechanism.

### C5 — The positive result: a repaired optimiser buys deception robustness
*Sections 9, 11. Evidence: `research/v06/runs/fitA.jsonl`, fitC, E3, E4.*
Four defects in the fitting harness, each measured: one sigma for 34 coordinates whose ranges span
0.1 to 40 (93.4% of `declareMargin` proposals landed on a clamp bound at generation 0); an unpaired
objective; a "soft minimum" that is a weighted mean; and a hard-coded `st.games * 2` that would have
silently reported a third of the true win rate at six rotations. With all four fixed — per-coordinate
sigma, a paired per-deal margin over the incumbent, an explicit minimax-regret objective, and a
recovery test that the old harness provably fails — a refit moves the policy where thirty
generations of the old one did not. Held-out, paired, 1,000 games per cell, two banks: **+9.5 and
+7.7 points against the withholder**, the deception archetype that is the project owner's own
manoeuvre and the exposure the v0.5 study named; head to head against v0.5 a wash.

### C6 — Engine corrections
*Section 4, section 12. Evidence: E0, E10.*
`BlockDP` parked its tables in a thread-local pool, so a second agent's build silently repointed the
first's: **285 mismatches in 294 checks**, harmless under the deployed approximate belief and fatal
under any exact one. Fixed by a generation stamp with lazy rebuild; the query-level check now reports
0 mismatches. `gateaudit` was parsed only in the v0.4 branch, so v0.5's declaration pre-gate audit
returned a vacuous pass over zero opportunities; enabled, it reports **0 false negatives in 666,689
rejections** — v0.5's pre-gates are exact, where v0.4's were not (1,017 / 24.1M). Plus the tuner's
`NFEAT` and rotation defects, and an opt-in certificate de-duplication.

### C7 — Corrections to the v0.4 and v0.5 records
*Section 12.* Carries forward the four register entries that never reached print (C7 and C9 from the
v0.5 corrections register; the two `docs/V04_FINDINGS.md` claims at `:66-68` and `:77-80`), and adds
this study's own: the tie channel's size and dimension, and the "exact inference is worse" reading.

## Structural obligations

- Every headline is a per-opponent profile with an explicit worst case, never an aggregate.
- The identity control (`v06:legacy=1` is v0.5 bit for bit, md5-checked) is stated up front, because
  it is what makes the ablation table exact rather than approximate.
- The partner regimes are separate columns; the self-play row is never the headline.
- Every mechanism that was built and measured worse is listed with its measured cost, so the next
  study does not rebuild it.
- A limitations section that states plainly what v0.6 is not: it is not a large win-rate improvement
  over v0.5, and the paper says so in the abstract.
