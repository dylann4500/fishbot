# FishBot v0.5 — design specification

Derived from the v0.5 diagnosis workflow (36 agents, every headline finding adversarially
re-verified at independent seeds). Reports: `research/v05/results/P0*–P8*`,
`research/v05/lit/v05-refresh.md`. Decisions D1/D2 in `research/v05/BRIEF.md`.

Everything below is keyed to a *measured* defect. Where a plausible fix was tested and failed,
it is recorded as rejected so it is not rebuilt.

---

## 0. What the diagnosis established

### 0.1 The deadlock is an ask-policy fixed point, not an information freeze

The v0.4 study explains non-termination by Theorem 1: a half-suit locked to one team is frozen,
"and, for the same reason, no further information about its allocation can ever arrive."
**Both halves of that are wrong as an account of the observed deadlock.**

- The deadlock is a **deterministic two-question cycle**. Across 1,200 mirror games at six
  independent seeds, all 140 games exceeding 300 events contain a strictly 2-periodic
  alternating ask cycle of length ≥ 10 (mean 235.7 events); 80% of the longest dead runs consist
  of exactly two distinct (actor, card, target) triples repeated ~122 times each. The cycle is
  opponent-vs-opponent in 140/140 games.
- It is **not** happening inside frozen half-suits. 66% of long games have no half-suit locked to
  any team at any point in the dead run, and only 13.0% of dead-run asks sit in a
  ground-truth-locked half-suit.
- The informational-freeze claim is **false on its own terms**, independently of the above:
  68.5% of legal asks inside a half-suit the team in fact owns strictly increase a teammate's
  exact P(correct MAP allocation) (mean +0.096, max +0.664), and 59.9% strictly lower its exact
  marginal entropy. `E11-termination.md` and the v0.4 paper must retract it. Ownership
  monotonicity does not imply informational monotonicity.

### 0.2 The cause, and the fix that was measured

**99.3% of v0.4's provably-dead asks are voluntary** — dead asks are 41.4% of asks while starved
turns (no live ask existed) are 0.29%. The agent chooses, over and over, a card it can prove the
target does not hold.

It does so because the ownership features are **not gated by hit probability**. `f[3]` own-set
progress (+1.688), `f[5]` lock completion (+4.071), `f[7]` completion bonus (+1.428) and `f[15]`
pTeamAll together pay up to **+8.76 at p = 0**, against `f[0]`'s 11.506·p. A card the whole team
already owns therefore outscores a genuine chance at a card it does not.

Filtering provably-dead asks against the actor's own public-information `Knowledge`:

| | v0.4 | + dead-ask filter |
|---|---|---|
| longest dead run | 289 | **1** |
| games with a dead run ≥ 6 | 32% | **0%** |
| declarations at/after the horizon | 474 | **0** |
| misdeclarations | 10.9% | **1.9%** |
| win rate vs shipped v0.4 | — | 49.75% (neutral) |

This single change removes the pathology at no cost in strength. It is the load-bearing fix.

### 0.3 Fixes that were tested and REJECTED — do not rebuild

- **A time-varying holding cost in `value()`** (lit R1). Sweeping `vWait -= c·nEvents/220` over
  c ∈ [0.5, 20] cuts events/game 146→121 and dead-run frequency 32%→18.5% but leaves the tail
  untouched (p90 313→311, longest run 289→289), raises declaration error 10.9%→13.4%, and loses
  3.9 points head-to-head. It is a mean-shifter. Also, a constant holding cost is not missing:
  `declareMargin = −0.0342` already is one. And in deadlocks the `urgent` flag bypasses
  `declareByValue` entirely — that line binds in only 9.4% of late blocked opportunities.
- **Deleting the policy prior** (`ptheta=0, pphi=0`) for robustness. It is **worse against
  deceptive opponents**, by 4.60 points [2.63, 6.58]. The exposure is over-weighting, not
  weighting.
- **A turn-transfer willingness ladder.** A genuine multi-candidate decision arises 0.148 times
  per game and governs 0.92% of asks; v0.4's unilateral choice already never passes to a
  teammate with no certain hit. Worth ~0.05 cards/game. Budget nothing.
- **Confidence-ranked declaration arbitration.** Lowest-seat costs only +0.37 pp pooled over
  30,000 games. Add the (free, rules-legal) ladder if convenient; do not engineer for it.

### 0.4 The other confirmed defects

| # | Defect | Measured cost |
|---|---|---|
| A | `bestGuess` picks a per-card argmax with no capacity constraint, so it names allocations that exceed a teammate's hand count — posterior probability exactly **zero** | **100%** of forced-endgame declarations wrong (281 distinct + 314 fresh). The same bug zeroes `willingForced`'s `pAlloc` at 99.2% of states, so all seven willingness rungs are inert |
| B | `pressure()` stage 2 (`nEvents ≥ 308`) returns true from `declareNow` before inspecting `pAlloc` at all | 8.13 pp against a mirror opponent; exactly 0 against every weak opponent. A pure strong-opponent tax |
| C | `askExpectedValue` opens `(void)target;` — half the ask score cannot see who is being asked | 0.61–0.90 free bits/ask at 44–47% of decisions; ~41 bits/team/game unused |
| D | `f[14] = binEnt(p)` carries weight **−2.6534**: a *negative* value-of-information term | the agent is paid to avoid uncertainty-reducing asks |
| E | `f[12]` rewards re-asking in the half-suit you last asked in (+1.270) | 40.0% exact repeat asks |
| F | `f[11]` rewards taking an opponent's last card without distinguishing the *last live* opponent | walks into the forced endgame it loses 100% of |
| G | `value()` takes no delta on `myCards`/`minFriendly` | the stopping rule cannot see that a declaration empties a hand — kills the stalemate-breaker and declare-to-protect lines |
| H | `computeAggregates()` runs before `refresh()` in `proposeDeclaration` | posterior stale by mean 2.0 events, max 177 |
| I | The 16-feature value function is effectively `bias + score differential`; the fitted signal is **constant across candidate asks** | the terms the ask rule consumes carry no measured outcome information |
| J | The mirror was never in the fitting panel; the "soft minimum" has a max/min gradient weight ratio of 1.9 | it is a weighted mean. Direct explanation for why the mirror pathology survived fitting |
| K | `priorPhi` is not an independent channel: the exponent rearranges so its second term is card-independent and Sinkhorn's capacity normalisation removes it | the "silence" channel the owner attacked does not exist in v0.4 |

---

## 1. Architecture

v0.5 is a new policy in `engine/src/v05.hpp`, registered as `v05`. v0.4 stays byte-identical and
remains the reference opponent. Every mechanism below is individually switchable so the paper can
ablate it.

### M1 — Live-ask gating (fixes the deadlock)
Restrict the candidate set to asks with a strictly positive hard-consistent probability, computed
from the actor's own `Knowledge` (`owner`/`mask`), falling back to the full legal set only when
that set is empty (0.29% of turns). Additionally, **gate the ownership features by p**: multiply
`f[3]`, `f[5]`, `f[7]`, `f[15]` by `p` so a card nobody can give you cannot outscore a live one.
M1 is the minimum viable v0.5 and must be measured alone before anything else is added.

### M2 — Feasible joint allocation (fixes the forced endgame)
Replace `bestGuess`'s per-card argmax with a capacity-feasible joint allocation search: the exact
`BlockDP::bestTeamAllocation` where it is affordable, otherwise a Hungarian/flow assignment over
the degree-constrained bipartite polytope. The same routine feeds `willingForced`, which
resurrects the willingness ladder for free. Re-shape the ladder to ~9 evenly spaced rungs over
[0,1] (the existing rungs all sit ≥ 0.5 while contested confidences cluster far below).

### M3 — Net-information term in the ask score (the strategic upgrade)
Add a signed term `+ν·ΔI_team − λ·ΔI_opp` computed from the two filters the engine already
produces (the grounded posterior `β̃` and the policy-weighted `β`). Replace `f[14]`'s sign.
Because the channel is rarely *provably* free (0.54% of decisions under the exact posterior), it
must be **priced, not gated** — a v0.5 that only signals when signalling is free will never
signal. Requires M4.

### M4 — A knowledge model of the other five seats
v0.4 builds `Knowledge` only for itself. Every technique that turns on "what does *he* know" —
teammate-directed signalling, blackballing, best-informed-declarer selection, baiting — needs
this, and it is nearly free because the transcript is public: seat *j*'s knowledge is the public
deduction state plus a posterior over *j*'s hand.

### M5 — Target-dimension selection
Delete `(void)target;`. Score the target dimension explicitly on lockout value (who must not get
the turn), on void progress within the half-suit, and — under `--conventions=on` — on codebook
value (signalling.md §7.9 Construction B).

### M6 — Partner-aware stochastic action selection (decision D2)
Softmax over the near-optimal set `A* = {a : μ* − μ_a < min(σ*, σ_a)}`. Temperature and the
convention flag are both keyed on the partner regime:
- *bot teammates* — low temperature, conventions available;
- *human/unknown teammates* — higher temperature, grounded play, no codebook.
Report both regimes separately; never headline the self-play configuration.

### M7 — Online per-seat opponent model (replaces the two global scalars)
A per-player posterior over a small type library (aggressive / silent / v0.3-like / deceptive),
updated from that player's public asks **and** from a *time-local* silence statistic ("own turns
since last ask in S"; "was asked in S and did not reply") — not v0.4's whole-game `totalAsks`,
which the algebra shows is inert. Shape it as a **data-biased response** (Johanson & Bowling,
AISTATS 2009): confidence per information set, decaying to the population prior where evidence is
absent, so a deliberately silent opponent produces no evidence and gets the prior instead of being
misread. Expose the exploitation/robustness dial explicitly and report the frontier.

### M8 — Termination without misdeclaration
Delete `pressure()` stage 2. With M1 in place the deadlock is gone, so the horizon has almost
nothing to do; retain a far-out repetition guard (no (actor, card, target) triple twice) as a
structural backstop rather than an event-count guillotine. The owner's rule stands: **a risky ask
beats a guaranteed-zero declaration.**

### M9 — Value function, rebuilt or retired
The current one contributes nothing to ask selection in principle. Two options, decide by
measurement: (a) collect rows at declaration points as well as ask points, drop the seven
degenerate features, and refit jointly with the policy vector inside the CEM so `freeze_config.py`
covers it; or (b) retire it and score declarations directly on `pAlloc` against a fitted
threshold. A gradient-boosted tree reaches held-out R² 0.388 vs 0.233 linear, so capacity is
available if (a) is chosen.

### M10 — Fitting and evaluation
Put the **mirror in the panel**. Replace the β=10 soft-min (effectively a weighted mean) with a
true worst-case or minimax-regret objective over the style set, and report `min` *and* regret
(a raw `min` over four styles stagnates on the hardest). Promote the three pathology statistics —
longest dead run, % games with a dead run ≥ 6, % post-horizon declarations wrong — to
first-class KPIs gating every commit.

---

## 2. Build order

1. **M1** alone → measure against v0.4 and in mirror. Gate: dead runs ≥ 6 must reach 0%.
2. **M2** → gate: forced-endgame accuracy must leave 0%.
3. **M8** → gate: no game hits the action cap; post-horizon declarations 0.
4. **M9** decision, **M10** harness, then refit. This is the first configuration worth a number.
5. **M4** → **M5** → **M3** → **M6** → **M7**, each measured alone and then jointly.
6. Exploitability probe and the per-style worst-case table, per the owner's standing preference.
