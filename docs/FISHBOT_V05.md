# FishBot v0.5: technical specification

FishBot v0.5 is a deterministic, fully inspectable policy for six-player Canadian
Fish. Like v0.4 it sees only its own hand and the public record — asks, answers,
transfers, declarations and card counts — and it has no private channel to its
teammates. It is implemented in `engine/src/v05.hpp`, registered as `v05`; the
belief engines (`engine/src/belief.hpp`, `engine/src/blockdp.hpp`) are shared with
v0.4 and are unchanged. v0.4 stays byte-identical in `engine/src/v04.hpp` and
remains the reference opponent.

v0.5 is not a new architecture. It is v0.4 with three measured defects fixed. The
design spec is `research/v05/DESIGN.md`; every number below is keyed to a report
in `research/v05/results/` or `research/v05/lit/v05-refresh.md`, and every fix is
individually switchable so it can be ablated.

**Scope note.** This document describes what is *built* — mechanisms M1, M2 and
M8. `DESIGN.md` specifies seven further mechanisms (M3–M7, M9, M10); those are
described in §11 in the future tense and are not present in the code.

**Headline, stated the way the evidence supports it.** v0.5 is **not meaningfully
stronger than v0.4 in win rate**: 51.11% head-to-head at the per-opponent
profile seed and 50.79% pooled over five held-out banks, nine-opponent means of
83.60% against 83.57%, and v0.4 marginally better on minimax regret over the style
set (1.61 against 1.78). What v0.5 delivers is the *elimination of a failure mode*
— provably dead asks 39.04% → 0%, dead runs 2,610 → 0, forced-endgame declarations
0.14% correct → 24.35% correct — plus a large, replicated gain against the
deception archetype that motivated the work (**+7.2** points on the withholder) and
a smaller, equally replicated **loss** against another (**−2.2** on the feint).
§8 gives the per-opponent breakdown and the explicit worst case; no aggregate
figure in this document should be quoted without it.

## What changed from v0.4

| | v0.4 | v0.5 |
|---|---|---|
| Ask candidate set | every legal ask | legal asks with strictly positive hard-consistent probability (**M1**) |
| Ownership features | `f[3]`, `f[5]`, `f[7]`, `f[15]` paid in full at `p = 0` | unchanged in the shipped configuration; the `p`-scaling (**M1p**) is built, measured and **off** (§7) |
| Declared allocation | per-card argmax over teammate marginals, no capacity constraint | argmax over the capacity-feasible assignments, scored by the joint estimator (**M2**) |
| Forced endgame | all seven willingness rungs inert; `bestGuess` names a capacity-infeasible allocation | ladder and `bestGuess` both fed by the feasible allocator (**M2**) |
| Voluntary declaration | names the per-card argmax and can score it at 0 | names a feasible allocation, or refuses the half-suit (**M2**) |
| Termination | two-stage event-count guillotine; stage 2 declares before inspecting `pAlloc` | stage 1 only (`pAlloc ≥ 0.5` past event 220), which never fires because no measured game reaches event 220 (**M8**) |
| Repetition guard | — | built, measured and **off** (§7): cards move, so a repeat can be the best ask on the board |
| Belief, features, value function, expectimax | — | unchanged |
| Fitted vector | round 5, panel `v03, lockout, detective, diversifier`, soft-min β = 10 | round 1, panel `v04, v03, lockout, detective, diversifier, hunter`, β = 25 — the mirror is in the panel (§9) |
| Commit gate | head-to-head win rate | the `fish pathology` KPIs (§10) *and* the per-opponent profile |

Everything v0.4's specification says about the structural fact, the constraint
system, exact inference, the deployed Fast path, the 20-feature ask score, the
one-ply expectimax and the value-based stopping rule continues to hold for v0.5
verbatim. `docs/FISHBOT_V04.md` remains the reference for those parts; §2–§4 below
restate only what is needed to read the rest.

## 1. The two findings the v0.5 study establishes

These are the substantive results, and they are what the changed code is for.

### 1.1 The deadlock is an ask-policy fixed point, not an information freeze

The v0.4 study (`research/v04/results/E11-termination.md`, and the paper) explains
non-termination by Theorem 1: a half-suit locked to one team is frozen, "and, for
the same reason, that no further information about its allocation can ever
arrive." **Both halves of that are wrong as an account of the observed deadlock,
and they are wrong in different ways.**

*The deadlock is not located in frozen half-suits.* Across 1,200 mirror games at
six independent seeds, all 140 games exceeding 300 events contain a strictly
2-periodic alternating ask cycle of length ≥ 10, averaging 235.7 events; 80% of
the longest dead runs consist of exactly two distinct (actor, card, target)
triples repeated about 122 times each, and the cycle is opponent-vs-opponent in
140/140 games (`DESIGN.md` §0.1, pooling the runs in `P1-deadlock-forensics.md`
and `P1-verify-e11-deadlock-seed*.txt`; at seed 31 alone the cycle is a pure
two-question cycle in 12 of the 14 long games). 66% of those long games have no
half-suit locked to any team at any point in the dead run, and only 13.0% of
dead-run asks sit in a ground-truth-locked half-suit
(`P1-verify-deadlock-lock-census.md`, pooled over four seeds).

*The informational claim is false on its own terms, independently of the above.*
An ask inside a locked half-suit is legal for a member of the owning team — the
rule requires only that the asker hold *another* card of the set — and
`Knowledge::onEvent` publishes `exclude(card, asker)` before it ever looks at the
outcome, so a guaranteed miss still permanently locates one card. Measured at
three independent seeds over 2,880 such asks, 68.5% strictly increase a teammate's
exact `P(MAP allocation)` (mean +0.096, max +0.664). An independent probe that
never calls `bestTeamAllocation` agrees: over 5,760 (state × locked half-suit ×
owning-team observer) triples, 59.9% strictly lower the observer's exact marginal
entropy (mean 0.325 nats, max 2.115) and 38.6% strictly shrink the support. Asks
*outside* the locked half-suit shrank its support in 0 of 20,898 cases. At the
mover's own decision point the ask v0.4 actually played was informative in 0/46
states while some legal ask was informative in 42/46
(`P1-verify-e11-information.md`).

Ownership monotonicity does not imply informational monotonicity. E11 and the v0.4
paper must retract that sentence; §12 records the retraction as an open item.

*The fix is refusal, not forcing.* v0.4 offers only two settings and both are bad
(`P0b-forcing-dilemma.md`). Raising the forcing horizon until the rule never fires
is worth **+6.2 points** against shipped v0.4 (56.2%, cluster bootstrap
54.9–57.5, n = 1500) and lifts declaration accuracy from 85.7% to 98.4% — but the
patient mirror then ends **22.5%** of games by action-cap adjudication, with a
longest dead run of 379 and 51.2% of asks provably dead. Forcing terminates by
misdeclaring; patience declares well and does not terminate.

The third option is to keep asking but ask productively. Restricting the candidate
set to asks the actor cannot prove are dead (M1) breaks the fixed point, and the
frozen v0.5 mirror measures longest dead run 286 → **0**, games with a dead run
≥ 6 34.33% → **0%**, provably dead asks 39.04% → **0%**, misdeclarations 10.44% →
**2.07%**, and no game reaching the horizon at all (§8; that is the shipped
configuration — M1, M2 and M8, with M1p and the repetition guard off — and the
single-mechanism split is the second table in §8). Note precisely what
this does and does not claim: v0.5 breaks the cycle by *declining* the
guaranteed-zero move, not by pricing the information a locked-suit ask emits. That
pricing is M3, and it is not built.

One caveat constrains any future version that tries. "The half-suit the team
provably owns" is the wrong description of these states: at 15 dumped deadlock
states, over all 36 (owning-team observer × locked half-suit) pairs, the exact
`P(my team owns this half-suit)` had mean 0.068 and **0 of 36** pairs exceeded
0.999 (`P1-verify-e11-information.md` §6.1). The information is real, but no
observer can condition on knowing the half-suit is locked. M3 has to price
expected information against a `pTeam` of about 0.07, not gate on certainty.

### 1.2 A per-card argmax allocation is infeasible, not merely suboptimal

v0.4's `bestGuess` chose, for each card of the half-suit independently, the
teammate with the largest marginal. Independence across cards is exactly what the
capacity constraint C4 forbids: a teammate holding two cards in total cannot be
named as the owner of three. Measured over 562 forced declarations at seed 777,
the named allocation assigned more unresolved cards to a teammate than that
teammate's remaining capacity in **562 of 562 cases** (overshoot of one card in
518, two in 44), and the exact posterior probability of the named allocation was
**0 in 562 of 562** (`P2-forced-endgame.md` §3). Checked model-free against true
hand sizes at the moment of declaration, the named allocation is physically
impossible in 100% of mirror runs at five of six seeds and 98.3% at the sixth
(`P2-verify-forced-endgame.md` §4).

This is a different kind of error from a low-probability guess, and the difference
has three consequences that a "suboptimal heuristic" framing would miss.

1. **The loss is total and structural, not statistical.** 100% of v0.4's
   forced-endgame declarations are wrong — 281 distinct declarations at seed 777
   plus 314 fresh ones, and 899 of 901 (99.78%) pooled over every re-run in
   `P2-verify-forced-endgame.md`. It is not a bad seed: `Baseline::Detective` and
   `Baseline::Lockout`, with their own `bestGuess`, get the same class of decision
   right 76–80% of the time in the same games.
2. **A zero-probability candidate silently disables the machinery meant to catch
   it.** `Belief::jointSequential` conditions card by card and calls
   `propagateCapacity()` after each fix, so as soon as the argmax overfills a
   teammate the next conditional marginal is 0 and the whole product returns 0.0.
   It is *correct* — and because `willingForced` evaluated only that one
   allocation, it saw `pAlloc = 0` at 99.2% of forced-endgame states, refused
   every rung, and handed the decision to `bestGuess`, which declares the same
   impossible allocation because it has no refusal path. All seven willingness
   rungs in `Rules::forcedTh` fired zero times in every run. The engine's own
   trace recorded `conf = 0.0` on all 562 events; re-measured over six
   configurations, `pAlloc` is bitwise zero at 3,252 of 3,278 forced-endgame
   states, 99.21% (`P2-verify-willingness-rungs.md`).
3. **Fixing it does not make the endgame easy; it moves the policy from
   certainly-wrong to the achievable ceiling.** At these states the posterior is
   flat over the surviving feasible allocations — `pbest` takes only the values
   0.5, 0.333, 0.25, 0.167, 0.1 — so the ceiling for any policy is about 40%.
   Replayed from the same endgame-entry states, a feasible argmax scores 228/562
   (**40.6%**) against v0.4's 0/562, and letting the best-positioned teammate
   declare instead of the lowest seat adds a further 6.0 points (46.6%). The whole
   gap is the capacity bug, not inference quality.

## 2. The structural fact, and the constraint system

Unchanged from v0.4, and restated because M2 is a statement about C4.

Cards move only through publicly observed transfers, so a card whose location has
never been revealed is still with whoever was dealt it, and the entire hidden
state of the game is the initial deal. For an observer, with `U` the set of cards
whose holder is not publicly known:

- **C1** own hand — exact, and excludes the observer from every card in `U`;
- **C2** public transfers and correct declarations — exact owner;
- **C3** exclusions — an ask proves the asker lacks the named card, a miss proves
  the target lacks it, and both are permanent;
- **C4** capacities — `q_p = handCount[p] − knownHeld[p]` unresolved cards belong
  to player `p`;
- **C5** ask legality — an ask in half-suit `S` proves the asker held another card
  of `S`.

The posterior is uniform over assignments satisfying C1–C5: the *grounded*
posterior, exact with respect to the rules and independent of how opponents choose
their moves. An allocation violating any of C1–C5 has posterior probability
exactly zero — that is the whole content of §1.2, applied to C4.

## 3. The inference path

Unchanged. `BeliefMode::Fast` is the default and produced every number reported
here: exact C1–C3 bookkeeping and exact C4 capacities fitted by Sinkhorn scaling,
with an independence-conditioning step interleaved for C5 (measured marginal error
against the exact engine: 0.017 mean, 0.498 max). The exact block dynamic program
(`v05:belief=block`) validates the probabilities and serves as an ablation; it is
about 14× slower in whole-game throughput and is not in the inner loop.

`fish oracle` re-validates the estimator v0.5 leans on hardest: named-allocation
probability matches exhaustive enumeration to 0.000e+00 over 227,062 checks, and
`bestTeamAllocation` is consistent and argmax over 1,679 checks
(`P1-verify-e11-information.md` §4). So `pAlloc` genuinely is P(the named
allocation is the true one) under the observer's posterior — which is what makes
"probability exactly zero" a statement about the world and not about the estimator.

## 4. Ask rule

`U(a) = λ_lin · Σ w_k φ_k(a) + λ_val · [ p·V(s⁺) + (1−p)·V(s⁻) ]`

over the same 20 normalised features and the same 16-feature linear value
function, followed by the same top-K re-scoring with a recomputed (still Sinkhorn)
belief on each branch. v0.5 changes the candidate set and four feature
definitions; it does not change the score's form, the weights, or the search.

### M1 — live-ask gating

`provablyDead(card, target)` is a hard deduction from the actor's own hand plus
the public record — `k.owner[card] < NPLAY ? k.owner[card] != target :
!(k.mask[card] & (1u << target))`. No posterior and no policy prior enter it, so
filtering on it cannot introduce a modelling error; it can only remove moves whose
probability is already zero.

`enumerateLive` builds the candidate set in three tiers, falling back only when a
tier is empty:

1. legal, not provably dead, and not already asked as this (card, target) pair —
   this tier is active only when the repetition guard is on, which by default it
   is not (§7);
2. legal and not provably dead (the repetition guard is dropped first);
3. the full legal set — a genuinely starved turn, where the rules still oblige the
   actor to move. Starved turns are 0.20% of asks in the v0.4 mirror and **0** in
   the frozen v0.5 mirror (600 games, seed 31), so tier 3 is a formality, not a
   hiding place.

This matters because **99.3% of v0.4's provably-dead asks are voluntary**: dead
asks are 41.4% of asks while starved turns are 0.29% (`DESIGN.md` §0.2). The agent
chose, repeatedly, a card it could prove the target did not hold.

### M1p — ownership features scaled by `p` (built, measured, **off**)

The reason v0.4 chose dead asks is that the ownership features are not gated by
hit probability. `f[3]` own-set progress (+1.6881), `f[5]` lock completion
(+4.0705), `f[7]` completion bonus (+1.4281) and `f[15]` team-owns-set together
pay up to **+8.76 at `p = 0`**, against `f[0]`'s 11.506·`p`. A card the whole team
already owns therefore outscores a genuine chance at a card it does not, and two
such policies facing each other settle into the 2-periodic cycle of §1.1.

M1p multiplies exactly those four features by `p` (`V05Config::ownershipByP`,
`v05.hpp:314`: `double og = cfg.ownershipByP ? p : 1.0;`). It is deliberately
separate from the gate: the gate removes impossible moves, M1p removes the
incentive that made them attractive.

**It ships off.** Once M1 has removed the `p = 0` case the incentive has nothing
left to act on, and re-scaling four of twenty features distorts the calibration of
the rest of the fitted linear score. Measured on the frozen configuration
(`E5-ablations.jsonl`, 250 deals × 6 rotations, seed 606060, against shipped
v0.4): `v05` 49.07% against `v05:m1p=1` 47.73%, i.e. turning M1p on costs **1.33
points**. It is retained as an ablation switch and as the record of *why* v0.4
behaved as it did, not as part of the policy. (The `-5.6` figure in the source
comment above the field predates the refit and was measured on v0.4's parameter
vector; the number to quote is the 1.33 above.)

### What M1 does not change

`askExpectedValue` still opens with `(void)target;` — half the ask score cannot
see who is being asked (defect C, worth 0.61–0.90 free bits/ask at 44–47% of
decisions). `f[14] = binEnt(p)` still carries a *negative* weight — the refit
moved it from v0.4's −2.6534 to −2.4266, which is the same sign and the same
defect: a negative value-of-information term (defect D). `f[12]` still rewards
re-asking in the half-suit last asked in, and the refit *raised* it, 1.2697 →
1.3803 (defect E). These are M5, M3 and the ask-score rebuild, and none of them
is built; the refit re-tuned the same feature set rather than changing it.

## 5. Declaration rule

### M2 — capacity-feasible joint allocation

A half-suit has six cards and a team has three seats, so the feasible set is at
most 3⁶ = 729 assignments. `feasibleAllocation` enumerates it exhaustively:

- cards in the actor's own hand and cards publicly located on the team are fixed;
- the half-suit is refused outright if any card is publicly held by an opponent,
  is out of play, or has no teammate left in its `mask`;
- each candidate assignment is rejected if it names a card at a seat that seat's
  `mask` excludes (a C5 or C3 violation) or if it exceeds `k.capacities()` for any
  teammate (a C4 violation);
- surviving assignments are ranked by the product of marginals, and the winner is
  re-scored by `Belief::jointSequential` — the same joint estimator the
  declaration rule already used.

The returned `pAlloc` is therefore a probability of an allocation the posterior has
not already ruled out, which is the property the v0.4 path lacked. The feasibility
test is exact; the ranking among feasible assignments is not (§12).

M2 feeds three call sites:

- **`bestGuess`** (the mandatory forced-endgame declaration). Falls through to
  v0.4's per-card argmax only when *no* feasible allocation exists at all, where
  it is a formality — the rules oblige somebody to name something.
- **`willingForced`** (the willingness ladder). With a non-zero `pAlloc` the seven
  rungs of `Rules::forcedTh` become live for the first time. `DESIGN.md` §M2 notes
  the rungs are badly placed — all sit at ≥ 0.5 while contested confidences
  cluster far below — and proposes ~9 evenly spaced rungs over [0,1]; **that
  re-shaping is not built.**
- **`evaluateSet`** under `BeliefMode::Fast`, i.e. the voluntary path, when
  `feasibleDecl` is on. This adds a refusal that v0.4 did not have: if no feasible
  allocation exists, the half-suit is not a declaration candidate at all.

Under `v05:belief=block` the exact `BlockDP::bestTeamAllocation` is used instead,
as in v0.4; it is the exact MAP over feasible allocations, whereas
`feasibleAllocation` takes the argmax under a product-of-marginals surrogate and
then scores the winner jointly. The two can disagree on which feasible allocation
to name; §12 records this.

### Stopping rule

Unchanged from v0.4. `declareByValue` compares the expected value of cashing —
`pAlloc·V(right) + (1−pAlloc)·V(wrong)` — against the value of waiting, plus a
fitted `declareMargin` (v0.5: −0.03044, itself a constant holding cost). The
urgency valves (`patiencePool`, `oppCardFloor`, `askFloor`, the forcing horizon)
and the cheap capacity/marginal pre-gates are unchanged. The frozen-policy
ablation in the v0.4 study did not resolve a benefit for the value rule over a
fixed threshold (+0.12 points, 95% paired CI −1.23 to +1.47), so the formulation
is not claimed to be optimal in v0.5 either.

Declaration races are still arbitrated by lowest seat (`Rules::declArbitration =
0`), deliberately, to avoid leaking private confidence. Measured cost of that
choice: +0.37 pp pooled over 30,000 games (`P6-verify-arbitration-cost.md`) — real
but small, and not worth engineering for.

## 6. Termination

### M8 — no event-count guillotine

`pressure()` in v0.4 returned 2 at `nEvents ≥ 308`, and `declareNow` returned
`true` on stage 2 **before inspecting `pAlloc` at all** — it cashed whatever was
best, however hopeless. Measured cost: **8.13 pp against a mirror opponent and
exactly 0 against every weak opponent** (`DESIGN.md` §0.4 B), a pure
strong-opponent tax, which is precisely why v0.4's published head-to-head numbers
never exposed it. Downstream, 58.6% of v0.4's declarations at or after event 220
were wrong, on 768 such declarations in 600 mirror games.

In v0.5 stage 2 is **off by default** (`forceStage2 = false`). Stage 1 survives —
past event 220, cash anything better than a coin flip — but with M1 in place no
measured game reaches event 220 at all, so it never fires: 0 declarations at or
after event 220 in the v0.5 mirror and in the v0.5-vs-v0.4 cross match, against
768 in the v0.4 mirror (§8).

**The repetition guard was the intended structural backstop, and it is off.**
`repeatGuard` forbids a (card, target) pair this seat has already asked. It looks
free, and it is not, because *cards move*: if the target has since won the asked
card from somebody else in a public transfer, the repeat is the single most
valuable ask on the board and the guard forbids exactly it. Measured on the frozen
configuration (`E5-ablations.jsonl`, seed 606060, against shipped v0.4): `v05`
49.07% against `v05:norepeat=1` 42.93%, i.e. turning the guard on costs **6.13
points** — the largest single-switch effect in the ablation table. M1 already
subsumes the case the guard was built for, because a repeat after a miss is
provably dead and the gate has already removed it. The guard stays as a switch and
is documented here as a rejected design, not as a shipped mechanism.

**What that leaves as the termination argument.** With stage 2 deleted and the
guard off, v0.5 has *no* structural guarantee of termination — only M1's empirical
one, which is that the two-question fixed point cannot form when neither party
will play a provably dead ask. The engine's 400-ask safety valve
(`Rules::maxAsks`, `engine/src/game.hpp`) remains as the harness-level backstop,
and it is never reached: action-limit games are 0% in E1 (over the full policy
cross-product) and 0% in every E2, E3 and E5 cell. That is evidence, not a proof,
and §12 records it as such.

The owner's rule, and the design principle behind M8: **a risky ask beats a
guaranteed-zero declaration.**

## 7. Ablation switches

Every mechanism is a boolean on `V05Config`, settable through the policy spec
parser (`engine/src/factory.hpp`) as `v05:key=0` or `v05:key=1`:

| switch | field | default | mechanism |
|---|---|---|---|
| `m1` | `liveAskGate` | **1** | restrict candidates to asks not provably dead |
| `m2` | `feasibleDecl` | **1** | feasible allocation on the **voluntary** declaration path |
| `stage2` | `forceStage2` | **0** | v0.4's event-count guillotine (set to 1 to restore it) |
| `m1p` | `ownershipByP` | **0** | scale `f[3]`, `f[5]`, `f[7]`, `f[15]` by `p` — *rejected*, costs 1.33 points (§4) |
| `norepeat` | `repeatGuard` | **0** | (card, target) repetition guard — *rejected*, costs 6.13 points (§6) |

Verify the defaults against the source rather than this table:
`grep -n 'liveAskGate\|ownershipByP\|feasibleDecl\|forceStage2\|repeatGuard' engine/src/v05.hpp`
prints `liveAskGate = true`, `ownershipByP = false`, `forceStage2 = false`,
`repeatGuard = false`, `feasibleDecl = true`. The two `false`s that look like
omissions are the two measured rejections above; both were built, both were run at
the frozen configuration, and both lost.

`v05:m1=0,m2=0,stage2=1` is the **v0.5-as-v0.4 control**: same binary, same fitted
parameters, the three shipped mechanisms off. It is the correct baseline for a
single-mechanism ablation, because it isolates the mechanism from any difference
between the two source files. It scores 50.87% against shipped v0.4
(`E5-ablations.jsonl`, seed 606060), and a mirror `fish pathology` run of the
control reproduces the v0.4 pathology class in full — at 200 games, seed 31:
39.14% provably dead asks, longest dead run 280, 26% of games with a run ≥ 6,
39.87% exact repeats, 12.78% of declarations wrong, 254 declarations at or after
event 220. The switches, not a source-file difference, are what the ablation
varies.

```bash
./fish pathology --a="v05:m1=0,m2=0,stage2=1" --b="v05:m1=0,m2=0,stage2=1" --games=100 --seed=31
```

`feasibleAllocation` always feeds `bestGuess` and `willingForced`; `m2=0` turns it
off only on the voluntary path. An ablation of M2 in the forced endgame is
therefore not available through a switch, and is measured by `fish forcedprobe`
against `v04` instead.

All of v0.4's parameter overrides (`belief`, `decl`, `force`, `ptheta`, `pphi`,
`topk`, `chain`, `threat`, `allparams`, per-feature `w0…w19` and `v0…v15`, …) are
accepted with identical names and identical layout, so a v0.4 parameter vector can
seed a v0.5 fit unchanged.

## 8. Measured results

Every figure in this section comes from the frozen configuration of §9 — the
vector in `research/v05/runs/v05-fitted.txt`, baked into `V05Config` by
`engine/freeze_config_v05.py`. The round-trip assertion holds at the precision the
script writes — `v05` against `v05:allparams=<vector with the twenty ask weights
rounded to 4 dp>` returns exactly 50% over 600 games; §10 gives the exact command
and explains why the command the script *prints* does not. Artifacts are named per
table and are regenerated by `engine/experiments_v05.sh`.

### 8.1 The result: the failure mode is gone (E2)

600 games per arm, seed 31, `fish pathology --games=300 --rotations=2`
(`--games` counts *deals*, `--rotations` defaults to 2).
Artifact: `research/v05/results/E2-pathology.txt`.

| KPI | v0.4 mirror | v0.5 mirror |
|---|---:|---:|
| provably dead asks | 39.04% of asks | **0%** |
| dead runs (mean length, longest) | 2,610 (12.05, **286**) | **0** |
| games with a dead run ≥ 6 | 34.33% | **0%** |
| exact repeat (actor, card, target) asks | 40.03% | **2.63%** |
| declarations wrong | 10.44% | **2.07%** |
| declarations at/after event 220 | 768, 58.59% wrong | **0** |
| forced-endgame declarations | 28, **100% wrong** | 2, **0% wrong** |
| starved turns | 0.20% of asks | **0** |
| events/game (median, p90, p99) | 143.60 (106, 312, 321) | **96.56 (96, 112, 125)** |
| ask hit rate | 34.25% | **55.47%** |
| action-limit games | 0% | 0% |

The cross match (`v05` against `v04`, 600 games, seed 90210) is in the same
artifact: 1,665 dead asks (3.07%), every one of them its own dead run of length 1,
0% of games with a run ≥ 6, 0 declarations at or after event 220, 99.83
events/game. Essentially all of those dead asks are v0.4's: with M1 on, v0.5 can
emit a dead ask only from tier 3 of `enumerateLive`, i.e. on a genuinely starved
turn, and that run records **one** starved turn in total, so at most one of the
1,665 can be v0.5's.

**One caveat about the cross match, and it matters.** `fish pathology` pools both
teams' statistics. In a *mirror* every declaration belongs to the policy under
test, so the mirror columns above are attributable; in a *cross* match they are
not. The cross-match line "forced endgame 29, wrong 26 (89.66%)" is dominated by
v0.4's side and is **not** a v0.5 figure. Forced-endgame accuracy must be read per
declaring team, which is what E8 does. The corrections register makes the same
point against the v0.4 study's declaration-accuracy claim (C10, and its "in a
mirror every declaration is a v0.4 declaration, so this is not a pooled figure").

### 8.2 Forced endgame at volume (E8)

24,000 games per arm, seed 909090, `fish match --games=4000 --rotations=6`.
Artifact: `research/v05/results/E8-forced-endgame.txt`.

| | forced declarations/game | correct |
|---|---:|---:|
| v0.4 | 0.0307 | **0.14%** |
| v0.5 | 0.0048 | **24.35%** |

v0.5 enters the forced endgame **6.4× less often** and, when it does, names a
capacity-feasible allocation that is right about a quarter of the time instead of
essentially never. The measured ceiling for the best feasible allocation at those
states is ≈ 40.6% (`P2-forced-endgame.md` §4, replayed from the same
endgame-entry states), so M2 recovers about three-fifths of the 0.14% → 40.6%
gap and roughly two-fifths of the available accuracy is still unclaimed.

### 8.3 Strength: not a headline (E3, E4)

**Head-to-head against v0.4**, five disjoint held-out seed banks, 300 deals × 6
rotations each (`research/v05/results/E3-headtohead.jsonl`):

| seed | 90210 | 31337 | 515151 | 777001 | 424242 | mean |
|---|---:|---:|---:|---:|---:|---:|
| v0.5 win rate | 50.11% | 49.50% | 52.33% | 50.89% | 51.11% | **50.79%** |

One of the five banks (31337) sits below 50%, and every bank's 95% interval
contains 50%. **This is a wash, and it must be reported as
one.** v0.5's case rests on §8.1 and §8.2, not on this row.

**Per-opponent profile**, 300 deals × 6 rotations per cell, seed 515253
(`research/v05/results/E4-perstyle.jsonl`). The project's standing rule is that no
aggregate figure is quoted without this table and an explicit worst case.

| opponent | v0.5 | v0.4 | delta |
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
| minimax regret over the style set | 1.78 (on v0.2) | **1.61** (on lockout) | |

The means are a wash and **v0.4 is marginally better on minimax regret**. Note
also that the v0.4 study's "lowest win rate against any panel member is 75.07%"
was measured on a panel containing no opponent of its own strength; once one is
included the worst case for both policies is ≈ 50%, because the worst case for
any policy in this family is a copy of itself. That is correction C10 in
`research/v05/results/C1-v04-corrections.md`.

### 8.4 Deception: the manoeuvre that started this, and the one it cost (E10)

Two independent seed banks, 400 deals × 6 rotations per cell (n = 2,400 games per
cell). Archetypes are in `engine/src/probe_deception.hpp`, registered in
`engine/src/factory.hpp`. Artifact: `research/v05/results/E10-deception.md`.

- **silent** — never asks in the half-suit it holds most cards of, until forced.
- **feint** — preferentially asks in a half-suit it holds exactly one card of,
  manufacturing a misleading ask-legality certificate, when the cost is small.
- **withholder** — the project owner's own manoeuvre: after being asked in
  half-suit *S* while holding other cards of *S*, avoids asking in *S* for *K*
  turns.

| opponent | v0.5 (31415926) | v0.4 (31415926) | v0.5 (8675309) | v0.4 (8675309) | mean delta |
|---|---:|---:|---:|---:|---:|
| silent | 80.42% | 79.96% | 83.17% | 79.00% | **+2.3** |
| feint | 50.96% | 54.13% | 52.08% | 53.29% | **−2.2** |
| withholder | 73.63% | 66.25% | 71.42% | 64.46% | **+7.2** |

**v0.5 is markedly more robust to the manoeuvre that motivated this work**:
+7.2 points on the withholder, replicated at both seeds. The mechanism is *not* a
new opponent model — M3–M7 are unbuilt. It is that v0.5 no longer spends turns on
asks it can prove will miss, so a misleading *absence* of asks has far less
leverage over the position.

**v0.5 is worse against the feint by 2.2 points, and that replicates too.** The
feint manufactures a *false positive* certificate rather than withholding a true
one, and the fit raised `priorTheta` from 0.2638 to 0.4446 — v0.5 weights "this
player asked here" more heavily than v0.4 did. The diagnosis predicted exactly
this exposure: over-weighting the policy prior, not weighting it at all, is the
vulnerability (deleting the prior is *worse*, by 4.60 points, CI [2.63, 6.58]).
A sweep over `priorTheta ∈ {0.20, 0.26, 0.35, 0.445, 0.60}` at two seeds does not
identify the parameter — the ordering is unstable across seeds and every pairwise
difference sits inside the cluster-bootstrap interval — so the fitted value is
retained rather than tuned on noise. The principled fix is M7, and M7 is not
built.

**Consequence for the worst case.** Across the full twelve-style set (nine
standard plus three deceptive), v0.5's worst case is the **feint at 50.96%**,
marginally below its mirror worst case of 51.11%; v0.4's worst case remains its own
mirror at 50.00%. Neither policy is worse than a coin flip against anything in the
set, and no aggregate should be quoted without this table beside it.

### 8.5 Mechanism ablations (E5)

250 deals × 6 rotations, seed 606060, every arm against shipped v0.4.
Artifact: `research/v05/results/E5-ablations.jsonl`.

| configuration | win rate vs v0.4 | events/game | decl. accuracy |
|---|---:|---:|---:|
| control `m1=0,m2=0,stage2=1` | 50.87% | 141.2 | 89.02% |
| M1 alone `m1=1,m2=0,stage2=1` | 49.53% | 99.7 | 98.76% |
| M2 alone `m1=0,m2=1,stage2=1` | 52.60% | 141.4 | 91.20% |
| M8 alone `m1=0,m2=0,stage2=0` | 56.60% | 141.7 | 96.60% |
| M1+M2 `m1=1,m2=1,stage2=1` | 49.07% | 99.7 | 98.31% |
| M1+M8 `m1=1,m2=0,stage2=0` | 49.53% | 99.7 | 98.76% |
| M2+M8 `m1=0,m2=1,stage2=0` | 56.40% | 141.7 | 96.16% |
| **shipped `v05`** | **49.07%** | **99.7** | **98.31%** |
| `v05:m1p=1` (M1p on) | 47.73% | 99.2 | 97.50% |
| `v05:norepeat=1` (guard on) | 42.93% | 99.6 | 98.37% |

Read this table together with §1.1, and read it honestly.

1. **M1 is worth nothing in win rate and is still the load-bearing change.** It
   costs 1.33 points against the control while cutting events/game from 141 to 100
   and lifting declaration accuracy from 89.0% to 98.8%. It is what makes the game
   terminate on its own, which is what permits stage 2 to be deleted.
2. **The 5–6 points in this table are M8's**, and they are only available *because*
   M1 removed the deadlock that stage 2 existed to break. Deleting stage 2 without
   M1 makes the pathology *worse*, not better: on the mirror instrument
   (`./fish pathology --a=v05:m1=0,m2=0,stage2=0 --b=<same> --games=100 --seed=31`,
   200 games — an ad-hoc run, not a battery artifact) that configuration records
   **44.83%** dead asks against the control's 39.14%, a longest dead run of
   **373** against 280, 26% of games with a run ≥ 6, 76 declarations at or after
   event 220, and **14% of games terminated by the engine's 400-ask action
   limit**. It wins more games at seed 606060 against v0.4 — where it does not
   deadlock, so E5 records `limitHitRate` 0 — and it is the pathological policy
   the study set out to remove.
3. **The shipped configuration is not the highest row.** M2+M8 without M1 scores
   56.40% and has the identical mirror pathology to the row above (44.83% dead
   asks, longest run 373, 14% action-limit games — M2 changes what is declared,
   not what is asked). Both are rejected on the KPIs of §8.1, not on win rate —
   which is the whole point of gating commits on the pathology table rather than
   the scoreboard. Note in particular that a win rate collected against v0.4 at
   seed 606060 cannot see the action-limit failure, because it is a mirror
   phenomenon: this is the same blind spot as §8.3, one level down.
4. **Both rejected switches lose**: M1p −1.33, the repetition guard −6.13.

### 8.6 Calibration, dialects, throughput, audit (E1, E6, E7, E9)

- **E1** `fish verify --games=600`: 0 audit violations in 23,594,580 checks,
  0 set-conservation failures, 0 action-limit games, determinism PASS.
- **E6** `fish calibrate --a=v05 --b=v04 --games=400 --seed=717171`: ask forecasts
  n = 36,467, Brier 0.1219, ECE 0.0223; declaration forecasts n = 3,609, Brier
  0.0150, ECE 0.0154. The declaration estimator the stopping rule consumes is
  calibrated, which is what makes it a decision rule rather than a heuristic.
- **E7** rule dialects, 250 deals × 6 rotations, seed 828282, v0.5 against v0.4:
  default 51.60%, `--no-out-of-turn` 52.47%, `--no-cardless-declare` 51.87%,
  `--legacy` 52.13%. No dialect moves the result outside its own interval.
- **E9** `fish bench --a=v05 --b=v05 --games=300`: 286 games/s.

## 9. Fitting, and the frozen vector

Cross-entropy method over the same 34-coordinate vector as v0.4 (20 ask-score
weights plus 14 decision knobs), common random numbers within a generation and
fresh banks between generations, soft minimum over the opponent panel.

```bash
./fish tune --base=v05 \
  --panel=v04,v03,lockout,detective,diversifier,hunter \
  --full --games=200 --pop=24 --elite=6 --gens=40 --beta=25 --seed=505101 \
  --out=../research/v05/runs/fit-round1.jsonl
```

Two deliberate changes from v0.4's fitting, both from `DESIGN.md` §M10:

- **The mirror is in the panel.** v0.4's panel was `v03, lockout, detective,
  diversifier` — every member weak enough that the mirror pathology never
  appeared. `v04` is now the first entry, which is the direct fix for defect J.
- **β raised from 10 to 25.** The gradient weight on opponent *o* is
  ∝ exp(−β·wr_o), so the max/min weight ratio over a panel is exactly
  exp(β·Δ) where Δ is the panel's win-rate spread. At β = 10 over a panel
  spanning Δ = 0.063, v0.4's "soft minimum" had a ratio of 1.9 — a weighted mean
  wearing a minimum's name. v0.5's best-generation profile spans Δ = 0.415, and
  at β = 25 the ratio is 32048.3. Neither change is sufficient alone: β = 25 over
  v0.4's spread gives only 4.83, and β = 10 over v0.5's spread gives 63.43 — the
  temperature sharpens the objective, and the even-strength opponent is what
  gives it something to sharpen. It is still a partial fix: `DESIGN.md` asks
  for an explicit worst-case or minimax-regret objective reporting `min` *and*
  regret, and that is not built (§12).

**What the run did.** 40 generations, population 24, elite 6
(`research/v05/runs/fit-round1.jsonl`). In-panel win rates at the best generation
were 57.0 / 75.2 / 80.5 / 79.0 / 94.8 / 98.5 against
`v04 / v03 / lockout / detective / diversifier / hunter`. The tuner's final
common-seed re-evaluation preferred the **distribution mean** (0.5272) over the
best single generation (0.4798) — the winner's-curse guard doing its job — so the
shipped vector is the mean, not the best generation's sample.

**In-panel 57% against held-out ≈ 51% is the regression, and the held-out figure
is the one to report.** §8.3 reports it.

**What the refit was worth: nothing measurable.**
`research/v05/runs/v04vector-in-v05.txt` runs the v0.5 mechanisms on v0.4's
*frozen* 34-coordinate vector (`research/v04/runs/selected.json`, generation 8 of
the round-5 trace) instead of on the vector above. The **shipped** assembly on
that vector already plays level against v0.4 — 50.22% [48.40–52.03] over 6,000
games at two banks, range 50.13–50.30%, 4.49 declarations per game against 4.47 —
and on E2's pathology instrument and seed it already takes the mirror dead-ask
rate to 0.02%, the longest dead run to 1, games with a run ≥ 6 to 0%, exact
repeats to 2.97%, declarations at or after the old horizon to 0, and declaration
error to 1.89%. Compare the fitted vector's 0% / 0 / 0% / 2.63% / 0 / 2.07% in
`research/v05/results/E2-pathology.txt`: the repair is **mechanical, not
parametric**, on every row the instrument reports. The forced endgame is the one
row that cannot be read off this bank — it fires four times in 600 games. The
refit is still necessary bookkeeping (the declaration thresholds were fitted
underneath a forcing rule that no longer fires), but no figure in §8 or §10
should be read as something it bought. §8.1 of the paper states this.

**A correction that goes with it.** The pre-refit shortfall the paper's
mechanisms section quotes — 41.8% head-to-head, 4.21 declarations per game
against 4.75 — was measured on the **design-time** assembly, which carried the
ownership-by-*p* scaling and the repetition guard. Both were later measured,
rejected, and ship **off**. On the identical bank (200 deals × 6 rotations, seed
90210) and the identical v0.4 coordinates, the assembly that actually ships
scores 49.08% [46.67–51.58]: **7.3 of those points belong to the two rejected
mechanisms, not to the parameter mismatch.** Both rows are in the artifact so
the two configurations cannot be confused again.

**The layout assertion, and its negative control.**
`research/v05/runs/roundtrip-assert.txt` is the artifact: `v05` against
`v05:allparams=<vector as baked>` returns exactly 50.0000% over 6,000 games at two
independent seeds, with a degenerate deal-clustered interval. Re-encoding the same
vector so that the correct parser rebuilds precisely what a parser hard-coded at
offset 18 would have produced gives 45.87% [44.07–47.70] over 3,000 games — a
4.13-point shortfall, which is the size of the error v0.4's pipeline could not
see. The full-precision (unrounded) vector plays the baked one level, 50.43%
[49.43–51.40] over 3,000 games; see §10 for why the assertion is stated at the
precision the freezer writes.

### The frozen vector

`research/v05/runs/v05-fitted.txt` holds the 34 coordinates;
`python3 engine/freeze_config_v05.py research/v05/runs/v05-fitted.txt` rewrites
`V05Config`'s defaults in place and prints the round-trip command that asserts the
parser and the script agree on the layout — the defect that cost v0.4 two aliased
ask weights.

| # | field | v0.4 | v0.5 |
|---|---|---:|---:|
| w0 | hit probability | 11.5060 | 11.6423 |
| w1 | squared hit | 3.2948 | 3.3041 |
| w2 | certain hit | 3.1978 | 3.4785 |
| w3 | own set progress | 1.6881 | 2.5071 |
| w4 | team control | 2.1333 | 1.6882 |
| w5 | lock completion | 4.0705 | 4.0462 |
| w6 | continuation | 1.4679 | 1.6052 |
| w7 | completion bonus | 1.4281 | 1.2189 |
| w8 | reply threat | −3.0978 | −2.9058 |
| w9 | information leak | −0.8536 | −1.2201 |
| w10 | target hand size | −2.0219 | −2.2482 |
| w11 | empties target | 1.1660 | 1.1596 |
| w12 | repeats set | 1.2697 | 1.3803 |
| w13 | known team cards | 0.9142 | 0.8384 |
| w14 | location entropy | −2.6534 | −2.4266 |
| w15 | team owns set | −0.8045 | −0.9583 |
| w16 | exposure on miss | 1.9040 | 2.5333 |
| w17 | trailing pressure | 0.0473 | −0.2242 |
| w18 | runway | 1.4108 | 1.2455 |
| w19 | leak magnitude | −0.9990 | −1.4031 |
| | `declThreshold` | 0.81770 | 0.81991 |
| | `lockedAllocThresh` | 0.79690 | 0.73250 |
| | `askFloor` | 0.33250 | 0.25742 |
| | `patiencePool` | 6 | 6 |
| | `oppCardFloor` | 2.86760 | 2.61651 |
| | `valueWeight` | 6.04320 | 6.47680 |
| | `linearWeight` | 0.76670 | 0.75393 |
| | `minTeamProb` | 0.79250 | 0.85876 |
| | `declareMargin` | −0.03420 | −0.03044 |
| | `priorTheta` | 0.26380 | **0.44458** |
| | `priorPhi` | 0.13280 | 0.12198 |
| | `searchTopK` | 6 | 6 |
| | `chainWeight` | 3.35800 | 3.58301 |
| | `threatWeight` | 2.61270 | 2.70470 |

Most coordinates moved a little; three moved enough to be worth naming.
`minTeamProb` rose 0.7925 → 0.8588 and `askFloor` fell 0.3325 → 0.2574 — the
policy became choosier about which half-suits it will name and more willing to
keep asking rather than cash out. The one to *worry* about is
**`priorTheta`, 0.2638 → 0.4446**: the diagnosis established that over-weighting
the policy prior is precisely the deception exposure, §8.4 measures the
consequence (−2.2 on the feint, replicated), and §12 records it as the first thing
a v0.5.1 must address.

The 16 value-function coefficients are still outside the fit, exactly as in v0.4;
`freeze_config_v05.py` writes only the 34 policy parameters. See §12.

## 10. Reproducing, and the commit gate

Everything below was run at the frozen configuration on a clean rebuild
(`cd engine && rm -f fish && make`). The full battery is one script; the
individual commands are what to run when you want one number.

```bash
cd engine && make

# --- the battery, E1-E9, into research/v05/results/ --------------------------
./experiments_v05.sh                    # ~7 min; see the reproducibility note below
python3 build_tables_v05.py             # -> paper/numbers_v05_generated.tex
python3 build_manifest.py v05           # -> research/v05/results/MANIFEST.json

# --- engine soundness (policy-independent) -----------------------------------
./fish verify   --games=600             # rules + information safety + belief soundness
./fish selftest --games=40              # reference engine vs card DP vs exact sampling
./fish oracle   --games=150             # brute-force allocation oracle

# --- the KPI gate.  --games counts DEALS; --rotations defaults to 2, so
#     --games=300 is the 600-game population 8.1 reports.
./fish pathology --a=v05 --b=v05 --games=300 --seed=31
./fish pathology --a=v04 --b=v04 --games=300 --seed=31   # the baseline it is read against

# --- strength.  Never quote one of these without the per-style table. --------
./fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed=90210
for OPP in v04 v03 v02 lockout detective diversifier hunter bluffer random; do
  ./fish match --a=v05 --b=$OPP --games=300 --rotations=6 --seed=515253
done
for OPP in silent feint withholder; do                   # the deception panel
  ./fish match --a=v05 --b=$OPP --games=400 --rotations=6 --seed=31415926
done
./fish forcedprobe --a=v05 --b=v04 --games=300 --rotations=6 --seed=777

# --- single-mechanism ablation (control, then one mechanism at a time) -------
./fish match --a=v05:m1=0,m2=0,stage2=1 --b=v04 --games=250 --rotations=6 --seed=606060
./fish match --a=v05:m1=1,m2=0,stage2=1 --b=v04 --games=250 --rotations=6 --seed=606060
./fish match --a=v05:m1=0,m2=1,stage2=1 --b=v04 --games=250 --rotations=6 --seed=606060
./fish match --a=v05:m1=0,m2=0,stage2=0 --b=v04 --games=250 --rotations=6 --seed=606060
./fish match --a=v05:m1p=1              --b=v04 --games=250 --rotations=6 --seed=606060
./fish match --a=v05:norepeat=1         --b=v04 --games=250 --rotations=6 --seed=606060
```

**How reproducible the artifacts actually are.** Re-running `./experiments_v05.sh`
from a clean rebuild reproduces **E1, E2, E6 and E8 byte-for-byte**. **E3, E4, E5
and E7** are `--json` match records that carry a wall-clock `seconds` field, so
they reproduce identically in every reported quantity but *not* byte-for-byte;
their digests in `MANIFEST.json` pin the artifacts of record, not the re-run.
**E9** is a throughput measurement and is machine-dependent by construction (the
artifact of record says 286 games/s; re-runs of the same binary on this machine
have returned 274 and 320). Do not read a digest
mismatch on E3–E5, E7 or E9 as a change in result — diff the records with the
`seconds` field dropped.

**The round-trip assertion, and the precision it holds at.**
`freeze_config_v05.py` writes the twenty ask weights with `%.4f` and the fourteen
knobs with `%.5f`, but the assertion command it *prints* formats every coordinate
with `%.5f`. At five decimals the weights no longer match what is compiled in, and
the assertion does not hold — `v05` against `v05:allparams=<raw 5-dp vector>`
returns 50.83% over 600 games, not 50%. Round the first twenty coordinates to four
decimals and it is exact:

```bash
V=$(python3 -c "v=open('../research/v05/runs/v05-fitted.txt').read().strip().split('|'); \
print('|'.join(['%.4f'%float(x) for x in v[:20]]+v[20:]))")
./fish match --a=v05 --b="v05:allparams=$V" --games=100 --rotations=6 --seed=1
#   win rate  50%  n=600
```

§12 records the printed-command mismatch as a defect to fix in the script.

**The pre-gate audit does not currently run for v0.5.** `fish gateaudit` defaults
to `--a=v04:mgate=0.008,gateaudit=1`; the `gateaudit` key is parsed only in the
v0.4 branch of `engine/src/factory.hpp`, so `--a=v05:...,gateaudit=1` leaves
`V05Config::gateAudit` false, the counters stay at zero and the command prints
`GATEAUDIT PASS (no false negative observed)` over **0 opportunities** — a vacuous
pass, not a result. The audit machinery inside `V05Agent::proposeDeclaration` is
present and correct; only the spec key is missing. Do not quote a v0.5 gate audit
until that one line exists (§12).

### The gate

A v0.5 commit is admissible only if the mirror `fish pathology` run above meets
every row. The thresholds are set at the shipped configuration's measured values
with headroom, and both columns are stated so a reader can see how much headroom
there is.

| # | KPI (`printPathology`, `engine/src/diag.hpp`) | v0.4 mirror | shipped v0.5 | threshold |
|---|---|---:|---:|---|
| 1 | `games w/ run>=6` | 34.33% | **0%** | **0%** |
| 2 | `dead runs … longest` | 286 | **0** | ≤ 2 |
| 3 | `DEAD asks` | 39.04% | **0%** | < 1% of asks |
| 4 | `at/after ev>=220` count | 768 | **0** | **0** |
| 5 | forced-endgame accuracy, **per declaring team**, from E8 | 0.14% | **24.35%** | ≥ 20% |
| 6 | `declarations … wrong` | 10.44% | **2.07%** | ≤ 3% |
| 7 | `action-limit games` | 0% | **0%** | **0%** |
| 8 | `repeat (a,c,t)` | 40.03% | **2.63%** | < 5% |

Three thresholds are looser here than in the pre-fit draft of this document,
which asked for ≤ 2% wrong declarations, < 2% repeats, and < 60% of forced
declarations wrong. The shipped configuration measures 2.07%, 2.63% and 75.65%
respectively, so those three would have failed their own gate. They are restated
at the measured values rather than left as aspirations the policy does not meet;
the aspiration is recorded instead in §12, where the forced-endgame gap to the
≈ 40.6% feasible ceiling belongs.

KPI 5 cannot be read from the mirror pathology run. v0.4 reaches a forced endgame
in 28 declarations per 600 mirror games; v0.5 reaches it in 2, which makes the
mirror ratio too small to mean anything. Read it from E8, whose 24,000 games per
arm give 737 forced declarations for v0.4 and 115 for v0.5 (rates 0.0307 and
0.0048 per game) — and read it **per declaring team**, because
`fish pathology` pools both sides and a pooled figure in a cross match is
dominated by whichever arm is worse (§8.1).

The pathology statistics are computed by replaying a traced game against the
ground-truth deal and reconstructing each seat's `Knowledge` at every decision
point, so "the actor could prove this ask was dead" is answered from public
information only, with no instrumentation of the policy. **Win rate alone is not a
sufficient gate**: the entire v0.4 failure is a pathology that a weak-opponent win
rate cannot see, and §8.5 shows two configurations that beat the shipped one on
win rate while failing every gate row the mirror run can measure (rows 1-4 and
6-8; row 5 needs an E8-scale bank and was not run for them).

When comparing forced-endgame counts across runs, note that in a **mirror** match
`--rotations=6` sets `orient = rot / 3` and `shift = rot % 3`, so the two
orientations run identical policies and every game is counted twice
(`P2-verify-forced-endgame.md` §5). Halve mirror counts, or use `--rotations=3`.

## 11. Designed but not built

`research/v05/DESIGN.md` specifies these; none of them exists in `v05.hpp`, and no
number in this document depends on any of them.

- **M3 — net-information term in the ask score.** `+ν·ΔI_team − λ·ΔI_opp` from the
  grounded and policy-weighted filters, replacing `f[14]`'s sign. The channel is
  provably free at only 0.54% of decisions, so it must be *priced*, not gated.
  Requires M4.
- **M4 — a knowledge model of the other five seats.** v0.4 and v0.5 build
  `Knowledge` only for themselves. Nearly free, since the transcript is public.
- **M5 — target-dimension selection.** Delete `(void)target;` and score the target
  on lockout value and void progress.
- **M6 — partner-aware stochastic action selection** (decision D2): softmax over
  near-optimal asks, with temperature and the convention flag keyed on whether the
  teammates are bots or humans. Both regimes to be reported separately.
- **M7 — an online per-seat opponent model** replacing `priorTheta`/`priorPhi`,
  shaped as a data-biased response so a deliberately silent opponent produces no
  evidence and receives the prior rather than being misread. §8.4 measures why
  this is the highest-priority item: the fitted `priorTheta` is the feint
  exposure, and it cannot be tuned away.
- **M9 — value function rebuilt or retired.**
- **M10 — the fitting and evaluation harness**: a true worst-case or
  minimax-regret objective, and the KPI gate wired into the commit path.

Two patches exist under `research/v05/patches/` — `M4-M5.patch` and `M7.patch` —
and are **unmeasured**. Nothing in this document depends on them.

Four candidate fixes were **designed, tested and rejected** before anything was
built; they should not be rebuilt (`DESIGN.md` §0.3): a time-varying holding cost
in `value()` (a mean-shifter — it leaves p90 at 311 and loses 3.9 points);
deleting the policy prior (worse against deceptive opponents by 4.60 points, CI
[2.63, 6.58]); a turn-transfer willingness ladder (a genuine multi-candidate
decision arises 0.148 times per game and is worth ≈ 0.05 cards/game); and
confidence-ranked declaration arbitration (+0.37 pp over 30,000 games).

Two further mechanisms were **built, measured at the frozen configuration and
rejected**, and remain in the source as ablation switches that default to off:
the ownership-by-`p` scaling M1p (−1.33 points, §4) and the (card, target)
repetition guard (−6.13 points, §6). They are the reason `ownershipByP` and
`repeatGuard` read `false` in `V05Config`.

## 12. Known gaps

This list is the honest limitations section, and it is ordered by how much it
should change what a reader concludes from §8. The first four are the ones that
constrain the claim; the rest are inherited or cosmetic.

### The claim itself

- **The strength gain is not there.** v0.5 is +1.11 points against v0.4 at seed
  515253 and +0.79 pooled over five held-out banks, one of which is below
  50% and all five of whose intervals contain it; the nine-opponent means are 83.60% against 83.57%; and **v0.4 is better on
  minimax regret over the style set** (1.61 against 1.78). v0.5's case is §8.1 and
  §8.2 — the elimination of the failure mode — and §8.4's +7.2 on the withholder.
  It is not a win rate, and this document should never be cited as though it were.
- **The fit raised `priorTheta` from 0.2638 to 0.4446, and that is measurably a
  deception exposure.** §8.4 measures the cost: −2.2 points against the feint,
  replicated at two independent seed banks. A sweep over
  {0.20, 0.26, 0.35, 0.445, 0.60} at two seeds does not identify the parameter
  — the ordering is unstable across seeds and every pairwise difference sits
  inside the cluster-bootstrap interval — so it cannot be fixed by retuning. The
  principled fix is M7, a per-seat online type posterior with data-biased
  shrinkage, and M7 is specified and not built. **This is the first thing a
  v0.5.1 should address.**
- **No exploitability probe has been run against v0.5.** The v0.4 study fitted an
  adversary in the same policy class with the same optimiser and reported the
  achieved lower bound. v0.5 must not claim comparable robustness until the same
  probe is run. This is the single largest hole in the evaluation.
- **Termination is empirical, not structural.** Stage 2 is deleted and the
  repetition guard is off (§6), so nothing in v0.5 *proves* the game ends. What is
  measured is that no game in E1, E2, E3, E4, E5 or E7 reaches the engine's
  400-ask safety valve, and no game reaches the event-220 horizon. Those are the
  battery's action-limit-bearing artifacts: 64,800 games, of which 37 recorded
  match rows report `limitHitRate` 0 and E2 reports `action-limit games 0 (0%)`
  on all three columns. (E8's artifact greps out everything but the declaration
  lines and E10 is a markdown table, so neither can be cited here.) That is
  strong evidence and it is not a proof. A structural backstop that does not cost
  6 points — the repetition guard costs exactly that — has not been designed.

### The mechanisms, as built

- **Forced-endgame accuracy is 24.35% against a measured feasible ceiling of
  ≈ 40.6%.** M2 moved the policy from certainly-wrong to a quarter right; the
  remaining 16 points are the ranking rule and the choice of declarer, not the
  feasibility test.
- **`feasibleAllocation` is exact on feasibility, not on MAP.** It takes the
  argmax over feasible assignments under a product-of-marginals surrogate and
  scores the winner jointly. The exact MAP over the feasible set is
  `BlockDP::bestTeamAllocation`, which runs only under `belief=block`. The two can
  name different allocations; the size of that disagreement has not been measured.
- **The willingness ladder is live but badly calibrated.** `Rules::forcedTh` =
  {0.995, 0.98, 0.95, 0.90, 0.80, 0.65, 0.50, −1.0} — every real rung at ≥ 0.5,
  while the contested confidences M2 now produces cluster far below. The
  re-shaping to ~9 evenly spaced rungs over [0,1] is specified and not built.
- **The best-positioned teammate still does not declare.** The forced endgame
  declares from the lowest live seat. Measured value of changing this: +6.0 points
  of forced-endgame accuracy on top of M2 (`P2-forced-endgame.md` §4).
- **Mechanisms M3–M7, M9 and M10 are specified and not built** (§11). Patches for
  M4/M5 and M7 exist under `research/v05/patches/` and are **unmeasured**. In
  particular the conventions flag (decision D1) and the partner-aware regimes
  (decision D2) do not exist, so this document reports one configuration where the
  brief asks for two reported separately.

### Inherited from v0.4, still present

- **Half the ask score cannot see the target** (`(void)target;` in
  `askExpectedValue`), `f[14] = binEnt(p)` at weight −2.4266 still pays the agent
  to *avoid* uncertainty-reducing asks, and `f[12]` at +1.3803 still rewards
  re-asking in the half-suit last asked in. M1 masks the consequences of all
  three; it does not remove the causes.
- **`computeAggregates()` still runs before `refresh()` in `proposeDeclaration`**,
  leaving the posterior stale by mean 2.0 events, max 177 (defect H).
- **The opponent model is still two global scalars**, identical for all five other
  players and never updated in-game. Worse, `priorPhi` is not an independent
  channel at all: the exponent rearranges so its second term is card-independent
  and Sinkhorn's capacity normalisation removes it (defect K). The "silence"
  channel does not exist, so a deliberately silent opponent is misread by
  construction — which is exactly the manoeuvre that started this work.
- **The 16 value-function coefficients compiled into `V05Config::vw` are not
  fitted for v0.5** — they are v0.4's, which are themselves not the ones in
  `research/v04/results/E14-valuefit.txt`. `freeze_config_v05.py` writes only the
  34 policy parameters. Inherited defect, deliberately not touched.

### Tooling and provenance

- **The pre-gate audit does not run for v0.5.** `gateaudit` is parsed only in the
  v0.4 branch of `engine/src/factory.hpp`, so `--a=v05:...,gateaudit=1` audits
  nothing and prints a vacuous `GATEAUDIT PASS` over 0 opportunities (§10). The
  audit machinery in `V05Agent::proposeDeclaration` is present and correct; one
  line of spec parsing is missing. For v0.4 the audit found 1,017 false negatives
  over 24.1M gate rejections; the equivalent number for v0.5 is unknown.
- **`freeze_config_v05.py` prints a round-trip command that does not round-trip.**
  It writes ask weights with `%.4f` and prints the assertion with `%.5f`; at five
  decimals the mirror returns 50.83%, not 50%. §10 gives the command that does
  hold. The script's *output* is correct — only the assertion it suggests is
  mis-formatted — but a provenance check that silently fails is worse than none,
  and this is the same class of defect the script was written to prevent.
- **Stale comment in the source.** The comment block above `V05Agent::pressure`
  (`engine/src/v05.hpp`) still repeats v0.4's claim that in a locked half-suit
  "no information about it can ever arrive". §1.1 refutes it. The code is correct;
  the comment is not. Two other comments — the `-5.6` beside `ownershipByP` and
  the `-6.0` beside `repeatGuard` — quote pre-fit measurements; the frozen-config
  figures are −1.33 and −6.13 (§8.5).
- **`research/v04/results/E11-termination.md` and the v0.4 paper carry a false
  claim** and need an explicit retraction, not a silent patch — the informational
  half of Theorem 1's corollary. The retraction is drafted in
  `research/v05/results/C1-v04-corrections.md` §C1; `PAPER_PLAN.md` requires it to
  appear in the v0.5 paper as a correction, in its own subsection.
