# FishBot v0.7 — Threat model: an operational definition of team exploitability

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`, audited at commit `60fee17` ("v0.6").
Phase 0 of the v0.7 programme (`docs/v07/PHASE-PROMPTS.md`). Read with
`docs/v07/SUBOPTIMALITY-LEDGER.md`.

This document fixes *what "exploitable" means* for a homogeneous team of three v0.7 agents in
six-player Canadian Fish, so that phases 1–5 measure one thing rather than four. It proposes no
architecture, changes no policy, and specifies no engine code. Every mechanical claim about the
engine is cited to `file:line` and was checked against the source at the commit above.

Every claim in this document survived an adversarial re-read against the artifacts. The corrections
that re-read forced are recorded in `SUBOPTIMALITY-LEDGER.md` §4 rather than silently applied.

---

## 0. Summary of decisions

| # | Decision | Section |
|---|---|---|
| **T1** | The unit under evaluation is the **team joint policy** π = (π₁,π₂,π₃) of three identical agents, not a seat. | §2 |
| **T2** | The headline adversary controls **all three opposing seats**. A **one-seat** deviation column is reported alongside it, always, on the same deals. | §4.1 |
| **T3** | The adversary is **white-box**: it is given v0.7's source, its frozen parameter vector, and unlimited offline access to it. It is **not** given the deal, the deal seed, any seat's hand, or `Event::confidence`. | §4.2 |
| **T4** | Three adversary **correlation regimes** are named and reported separately: A0 independent, A1 synchronized (homogeneous), A2 ex-ante-correlated. **A1 is the corpus's existing probe and is empirically the weakest of the three**; the headline is A2. | §4.3 |
| **T5** | Six adversary **policy classes** C1–C6 are admitted, including the "learned" class the phase brief names and a white-box inversion class it asks about separately. A class may contribute to an exploitability claim only after it has passed a **planted-edge calibration** at or below the effect size being claimed. | §4.4 |
| **T6** | Every exploitability number is reported as a **lower bound** (`≥ x`), with class, regime, seat count and budget attached. Computing the true value is NP-hard even white-box (§8). | §4.5, §8 |
| **T7** | The v0.7 team gets **no shared secret pre-play seed**. Homogeneity is defined as exactly two prohibitions (§5); everything else a seat-conditioned policy can do is legal. | §5 |
| **T8** | An **illegal side channel** is defined by the bridge *encrypted-signal* criterion, made mechanical by a **closed list** of implementation-only discriminators plus tests S1–S6, over **all four** engine decision types. | §6 |
| **T9** | The engine at `60fee17` contains **three open channels** that would silently satisfy the illegal definition if a policy used them (the invertible reset seed, the shared `BlockDP` pool, `Event::confidence`). None is currently used; all three must be closed or audited before any v0.7 exploitability number is believed. | §6.3 |
| **T10** | The harness must hand each seat a random stream drawn **independently of the deal**. This is a charter change, not a policy change, and S4 is unfalsifiable without it. | §6.3, §6.4 |

---

## 1. Why this document exists: worst-case-over-a-panel cannot carry a v0.7 claim

The project's standing reporting rule is that the worst case over the opponent panel leads, never an
aggregate (`paper/sections_v06/10-protocol.tex`, `sec:protocol-metrics`). That rule is correct and
it has run out of discriminating power for the comparison v0.7 has to make.

**The structural fact.** For any homogeneous team policy π, the cell π versus π is exactly 50% with
**zero variance**. The arena plays each deal in a balanced block of rotations, and in a mirror match
the arms are exchangeable by construction, so the per-deal win count is deterministic. The artifacts
show it directly: `research/v06/results/E4-perstyle.jsonl` records `v05` against `v05` as
`winRateA = 0.5, ci = [0.5, 0.5]` — a zero-width interval, not a small one. The paper states the
same fact from the sampling side (`paper/sections_v06/10-protocol.tex`, `sec:protocol-blocks`: a
mirror `match` at two rotations "returns exactly 50% for that reason and not because the policies
are balanced").

**The consequence for a panel that contains a near-equal opponent.** Worst-case-over-panel is
determined by whichever panel member is closest in strength to the arm under test. Once v0.7 and
v0.6 are within a point of each other — which, on the corpus's own history of three releases without
a separated head-to-head gain, is the expected case — that cell pins the worst case near 50 no
matter how much better v0.7 is against everything else. The statistic cannot express "harder to
exploit", and against an opponent of equal strength it cannot exceed 50 at all.

**Where v0.6 actually sits, stated correctly.** Pooled over three disjoint banks v0.6's worst cell
is `\vsixPoolWorstSix` = 50.00% and its worst opponent is `\vsixPoolWorstOppSix` = **v04**
(`paper/tables_v06/perstyle_pooled.tex`; `paper/numbers_v06_generated.tex`). That is a *measured*
cell against v0.4, not a mirror — the pooled panel contains no v0.6 row
(`engine/build_tables_v06.py:532` fixes the opponent order and v0.6 is not in it). It is also not
stable: on the single E4 bank the same cell is `\vsixStyleWorstSix` = **48.67%**. So v0.6's worst
case is dominated by sampling noise on a near-equal cell (n = 4,800, half-width 1.41), not by
strategic content — which is the same conclusion by a different route. The v0.5 and v0.4 columns
*do* contain their own mirrors, and those columns are censored in the strict sense.

**Two external results say the same thing from the other side.** Timbers et al. ("Approximate
Exploitability: Learning a Best Response") report that of two AlphaZero policy variants the
**head-to-head-stronger** one (+207 relative Elo) was the **more exploitable** (≈40% vs ≈20% against
a learned best response). Davis, Burch & Bowling report head-to-head margin and exploitability
correlating at 0.15–0.30 in poker. Head-to-head strength and exploitability are not substitutes.

**Therefore: exploitability under the definition below is the axis on which any v0.7 claim to be the
"strongest achievable team" must be made.** Head-to-head and the per-style profile remain reported
quantities; they are no longer the discriminating ones.

---

## 2. The unit of evaluation

Fish is **two-team zero-sum, 3 versus 3**. This is not the setting most of the team-equilibrium
literature the v0.6 paper cites addresses: Celli & Gatti, Farina et al., Zhang et al. and Carminati
et al. are (primarily) *team versus a single adversary*. The distinction changes the complexity
class, and it changes it against us:

| Setting | TMECom | TMECor | TME |
|---|---|---|---|
| team vs **one** adversary | polynomial (Celli & Gatti, AAAI-18, Thm 2) | FNP-hard (Thm 3) | FNP-hard, additively inapproximable (Thm 6) |
| **team vs team** | — | **Δ₂ᴾ-complete** | **Σ₂ᴾ-complete** |

(team-vs-team: Carminati, Zhang, Cacciamani, Li, Farina, Gatti & Sandholm, *Efficient
representations for team and imperfect-recall equilibrium computation*, arXiv:2605.01841, §7.
Anagnostides, Panageas, Sandholm & Yan, *The Computational Complexity of Team Zero-Sum Games*,
arXiv:2606.16139, additionally place Nash at **PPAD-complete** even for two teams of *two* in
polymatrix games.)

The evaluated object is therefore the **team joint policy** π = (π₁,π₂,π₃), realised by three
instances of one binary carrying one frozen parameter vector, seated at the three seats of one team
(`engine/src/arena.hpp:75-79`: `A[i] = makeAgent(...)` builds three separate agent objects; seats
`0,2,4` are team 0 and `1,3,5` team 1, `engine/src/fish.hpp:33`).

A per-seat exploitability number is **not** defined by this document. In a game with more than two
players, a unilateral-deviation quantity (NashConv) does not measure what a coordinating opposing
team can do; the two-team structure is what makes Fish zero-sum at all, and it survives only if the
adversary is treated as one entity.

---

## 3. The engine's action space, stated once

Everything downstream is defined against this list, which is the complete set of decisions an agent
makes in `engine/src/game.hpp`.

| # | Decision | Legal set | Enforcement |
|---|---|---|---|
| **D1** | **Ask** `(card, target)` | half-suit live; actor holds ≥1 other card of that half-suit; actor does not hold the named card; target is a live opponent | `engine/src/fish.hpp:158-165` (`legalAsk`, tests hands), enumerated by `fish.hpp:179-196` (`enumerateAsks`) in order: half-suit 0→8, card index 0→5, seat 0→5 |
| **D2** | **Voluntary declaration** | a half-suit still active, plus an assignment of all six cards to seats **of the declarer's own team**; legal at any moment, in or out of turn, holding cards or not | `engine/src/game.hpp:200-236` (`declarationRound`), gated by `rules.outOfTurnDeclare` and `rules.cardlessMayDeclare` (`fish.hpp:108-109`) |
| **D3** | **Pass target** | when the turn-holder is cardless, choose which live teammate receives the turn | `engine/src/game.hpp:29` (`choosePassTarget`), applied at `game.hpp:296-309` |
| **D4** | **Forced-endgame willingness** | a **bit** per (threshold, half-suit, seat), swept over an eight-rung ladder; the last rung (`-1.0`) forces `bestGuess` | `engine/src/game.hpp:32-35` (`willingForced`), ladder `fish.hpp:126-127`, driver `game.hpp:239-268` |

Two engine facts govern the rest of this document, and both are deliberate design decisions already
taken by the project:

* **Declaration arbitration discards confidence.** `engine/src/game.hpp:203-207`: "Picking the most
  confident proposer would compare private confidences across seats, which is exactly the leak the
  forced endgame was corrected for, so we take the lowest seat that wants to declare and let the
  confidence be a reported diagnostic only." `rules.declArbitration` selects a **seat order**, never
  a confidence order (`fish.hpp:117-120`).
* **The forced endgame shares a willingness bit and nothing else.** `engine/src/fish.hpp:121-125`
  and `game.hpp:237-239`.

The engine therefore already embodies a doctrine: *the only intra-team channel beyond the public
event stream is willingness in the forced endgame.* §6 makes that doctrine mechanical — and notes
that the eight-rung ladder actually carries about three bits per (seat, half-suit), not one.

---

## 4. Definition E1 — v0.7 team exploitability

> **E1.** Let π be the frozen v0.7 team joint policy. For an adversary regime *R*, policy class *C*,
> seat count *k* ∈ {1,3} and search budget *B*, define
>
> **Expl(π ; R, C, k, B) = 100·max{ 0, w\*(π ; R, C, k, B) − 50 }** percentage points,
>
> where w\* is the win rate achieved against π, on a sealed evaluation bank disjoint from the
> adversary's fitting bank, by the best adversary the search (R, C, k, B) found.
>
> Expl is a **lower bound** on the true one-sided exploitability of π. It is reported as
> `≥ x [lo, hi]`, always with (R, C, k, B, bank, n) attached, and never as "π's exploitability".

Four notes on the shape of E1.

1. **It is one-sided.** The literature's two-sided quantity (McAleer, Farina, Zhou, Wang, Yang &
   Sandholm, *Team-PSRO*, NeurIPS 2023, §2.1) is
   `e(μ_T, μ_O) = u_T(BR_T(μ_O), μ_O) + u_O(μ_T, BR_O(μ_T))`, a sum of two best-response gains.
   v0.7 evaluates a policy, not an equilibrium pair, so only the second term is defined; E1 must
   never be described as a distance to TMECor.
2. **The floor at 50 is deliberate.** A weak adversary produces w\* < 50 and E1 clips it to zero
   rather than reporting a negative exploitability. Wang et al. (*Leveraging Team Correlation for
   Approximating Equilibrium in Two-Team Zero-Sum Games*, arXiv:2403.00255, Table 2) measure
   *negative* exploitability against a random adversary in every cell; a negative number is evidence
   about the adversary, not about the target. The corpus's own panel contains an opponent (`random`)
   that v0.6 beats 100.00% of the time (`paper/tables_v06/perstyle_pooled.tex`).
3. **The response bank must be fresh.** The v0.6 probe already does this and states why: the win
   rate reached during fitting is a maximum over a population on shared seeds and is upward biased
   (`engine/exploitability_v06.sh:4-6`).
4. **A failed search proves nothing.** This is the reading error the v0.6 paper corrects in the v0.4
   record (`paper/sections_v06/12-corrections.tex`, `sec:corr-vfour`) and it is the same caveat the
   LBR literature attaches to its own numbers: Lisý & Bowling report LBR scoring −536 mbb/g against
   a bot whose true exploitability was 90 mbb/g. **A negative LBR result is not a robustness
   result.**

### 4.1 T2 — how many seats the adversary controls

**The corpus's choice is kept as the headline and extended with a second, mandatory column.**

The v0.6 probe seats a full opposing team of three identical fitted responders:
`engine/exploitability_v06.sh:44` runs `match --a="$BASE:allparams=$w" --b="$target"`, and
`runMatch` builds three copies of `--a` (`engine/src/arena.hpp:75-79`). Three seats is right as the
headline, for three reasons:

* It is the only seat count under which the game is zero-sum at the level of the object being
  evaluated (§2).
* It is the only one against which a low number could ever certify anything: zero exploitability
  against a *joint* adversary is the TMECor condition (McAleer et al., eq. 3; Wang et al., eq. 1).
* Schulman & Vazirani's **defensive gap** bounds what an uncoordinated adversary misses: at k = 3 it
  can be up to 1 − 2^{1−k} = **3/4 of the payoff range** in the worst case.

The extension is a **one-seat column**, reported on the same deals, for a reason that is
computational rather than aesthetic. Best-responding with a single perfect-recall seat, with every
other seat's strategy fixed, is an LP over that seat's sequence form — polynomial (Koller, Megiddo &
von Stengel 1996). Best-responding with three decentralised seats is not: Celli & Gatti's Theorem 4
makes team best response APX-hard, and Li, Zanuttini & Ventos (*The Complexity of Pure Maxmin
Strategies in Two-Player Extensive-Form Games*, JAIR 82, 2025) put the exact case — chance move
present, MAX a team of decentralised perfect-recall agents ("multi-agent perfect recall", which is
precisely three FishBots) — at **NP-complete against a single fully-known opponent model** and
Σ₂ᴾ-complete in general. The one-seat column is the tractable anchor; the three-seat column is the
meaningful one; **the pair is the measurement**, and reporting only one of them is reporting one of
two numbers that the closest prior art shows to differ materially. (Bridge's two published
partnership-AI evaluations report the one-seat-substitution and whole-partnership-substitution
metrics separately, on shared deals, and the two rank agents differently by about three times the
95% half-width.)

### 4.2 T3 — what the adversary knows: white-box, with an explicit exclusion list

**v0.7's adversary is white-box.** It is given the v0.7 source, the frozen parameter vector, the
build, and unlimited offline compute against it. This is the standard exploitability grant, and in
v0.7's case it is also the *realistic* one: the configuration under evaluation is three copies of a
published open-source policy, so an opponent who has read the repository has exactly this.

The grant matters more here than in most games, because **every FishBot to date is deterministic**.
That is not an assumption; it is checked in the code:

* The deployed belief is `BeliefMode::Fast`, a Sinkhorn fit (`engine/src/v05.hpp:27`, dispatched at
  `v05.hpp:185-189`). The `Rng` member is consumed **only** on the exact/particle branches
  (`v05.hpp:195`, `v05.hpp:764`), which the shipped configuration never takes.
* `chooseAsk` is an argmax over the linear score. **The tie-break differs between versions and this
  matters for §6.** v0.5 scans candidates in enumeration order and keeps the first on a tie
  (`engine/src/v05.hpp:518`, strict `if (u > bestScore)`). v0.6's own scoring path sorts the
  candidate indices with **`std::sort`, which is not stable** (`engine/src/v06.hpp:403-405`), and
  then takes `ord[0]` (`v06.hpp:488`), so a bit-for-bit tie is resolved in introsort order — still
  deterministic, but an *implementation artifact* rather than a documented rule. The corpus
  describes the tie-break as "whichever `enumerateAsks` emitted first", which is true of v0.5 and
  not of v0.6.
* v0.6's only stochastic switch, `randomTie`, is seeded from a **rolling hash of the public event
  stream** (`engine/src/v06.hpp:489-491`), i.e. from data the adversary also has.

So for the shipped policy, `a_t = f(own hand, public transcript)` exactly, and the map `f` is public
under T3.

**The consequence, which no prior FishBot study has tested.** With a stochastic policy, `P(a | deal)`
is a soft likelihood and an observer's posterior stays diffuse. With a deterministic policy,
`P(a | deal) ∈ {0,1}`: each observed action **exactly partitions** the deal space into deals under
which the actor would have played it and deals under which it would not. Belief refinement becomes
constraint propagation rather than reweighting, and it is *complete*. This is LBR's Bayes step run
in the same direction the corpus's own `rebstock-inference` citation (Rebstock, Solinas, Buro &
Sturtevant, *Policy Based Inference in Trick-Taking Card Games*, IEEE CoG 2019) runs it to play
better. For a 20-feature linear ask score the preimage of an observed ask is a polyhedron in feature
space — an object an attacker characterises once, offline, and reuses on every deal.

Concretely: the engine's certificate system (C3 "the asker lacks the named card", C5 "the asker
holds another card of that half-suit", `engine/src/belief.hpp:156-171`) is what the *rules* force an
ask to reveal. A white-box adversary against a deterministic policy gets, in addition, the constraint
"and this seat's hand lies in the region where that ask maximised the score". The corpus has measured
the size of the unforced choice: **0.959 bits on the target dimension, 1.301 bits on the
card-within-half-suit dimension, 4.979 bits jointly**, per ask
(`research/v06/notes/R9-human-tactics-catalogue.md`, T#11). Whether inverting them shrinks the deal
posterior below what the certificates alone imply, and whether that converts into win rate, is
**unmeasured**, and phase 1 must measure it. Either answer is a result.

**What the adversary is NOT given.** These are exclusions, not concessions; each is a harness
artifact with no counterpart in the physical game, and admitting any of them turns an exploitability
number into a statement about the simulator.

| Excluded | Why | Code |
|---|---|---|
| the deal, any seat's hand, `GameState::dealt` | ground truth; never shown to a policy | `engine/src/fish.hpp:148` |
| the deal seed and any seed derived from it | see §6.3 (E-1): the per-seat reset seed inverts to the deal seed in closed form | `engine/src/game.hpp:110` |
| `Event::confidence` | a real-valued diagnostic broadcast with every declaration; it has no counterpart in the physical game and is a direct read-out of the declarer's private posterior | `engine/src/fish.hpp:104`, broadcast at `game.hpp:145` |
| wall-clock timing, memory, or any observation of the target process | not part of the game | — |

An adversary that uses an excluded input is a **harness finding**, filed as such, and its score is
not an exploitability number. (v0.6's `Event::confidence` is currently read by no policy — verified
by grep across `v04.hpp`, `v05.hpp`, `v06.hpp`, `belief.hpp`, `baselines.hpp`, `human.hpp` — so the
exclusion costs nothing today.)

### 4.3 T4 — the adversary's own correlation regime

The corpus's probe fits **one** vector and seats three identical copies. That is a specific and, as
it happens, weak point in a three-point space. Name all three and report them separately:

| Regime | The three adversary seats | What it detects | What it misses |
|---|---|---|---|
| **A0 independent** | three separately fitted vectors, independent per-seat randomisation | exploits requiring simultaneous but uncoordinated deviation | anything needing a shared plan; bounded away from A2 by the defensive gap (≤ 3/4 of range at k = 3) |
| **A1 synchronized** | three copies of one vector — **the corpus's existing probe** | exploits expressible by a symmetric counter-team | role-differentiated attacks (an agreed division of half-suits, an agreed blackballing schedule); empirically the weakest of the four non-random classes at 3v3 in the only published comparison |
| **A2 ex-ante correlated** | one vector plus a **shared pre-play signal secret from the v0.7 team** | the TMECor class; the only regime under which a low score certifies anything | **not nothing.** It still misses everything outside the admitted classes C1–C6, everything below the responder's calibrated detection floor, and everything the budget did not find. Per T6 and §8 this remains a lower bound |

Wang et al. (arXiv:2403.00255, Table 2) measured exploitability of three algorithms against five
adversary correlation classes on MAgent Battle, 3 seeds, and found two things that transfer directly:
**the ranking of policies flips with the adversary class**, and the *synchronized* class — A1, the
parameter-sharing homogeneous adversary — was the **weakest** of the four non-random classes at 3v3
(0.625 for PSRO against 2.122 for the joint class) while being the *most* favourable at 12v12. It is
not monotone. A ledger built against one class is not reproducible under another.

**Headline = A2. A1 is retained as the continuity column** so that v0.7's numbers are comparable to
`\vsixLbrSix` = 48.36% and its two predecessor rows. A0 is cheap and is reported because it is the
only regime whose relationship to the others is theoretically bounded.

A2 requires giving the adversary something the v0.7 team is denied by T7 — a shared secret signal.
That asymmetry is deliberate and is the point: the adversary is a measuring instrument, not a
competitor bound by the same charter.

### 4.4 T5 — the policy classes the adversary may draw from

The phase brief names five classes: in-class linear, extended features, search-based, learned,
scripted-adaptive. All five are admitted here, and a sixth is added because the brief separately
asks whether a white-box adversary can invert the transcript.

| Class | Description | Detects | Blind to | Corpus status |
|---|---|---|---|---|
| **C1 in-class linear** | the 37-coordinate v0.6 vector family, refit by the repaired CEM | mis-set weights; regions of the policy's own class it fails to occupy | anything outside the linear score | **the only class ever run**, and only at 34 coordinates (§7, R-1) |
| **C2 extended features** | C1 plus features v0.6 does not have (opponent-hand modelling, per-target terms, seat-role terms) | exploits requiring a distinction v0.6's feature set cannot represent | multi-step plans | never built |
| **C3 search-based** | the `v06_rollout` machinery pointed at exploitation rather than self-improvement | multi-step manoeuvres: blackballing, turn routing, baiting | anything the rollout blueprint cannot model | machinery exists (`engine/src/v06_rollout.hpp`), never used adversarially |
| **C4 learned** | a policy trained by self-play or by RL best response against the frozen target, with a function class not fixed in advance | out-of-class exploits of a kind nobody anticipated — the class most likely to be genuinely out-of-class | nothing structurally; bounded only by compute and by the reward signal | never built. `docs/METHODOLOGY.md` names large-scale self-play as a roadmap item; the corpus has no learned agent of any kind |
| **C5 white-box inversion** | a responder that inverts the public transcript against the known deterministic policy (§4.2) to sharpen its deal posterior, then plays greedily | exactly the determinism/readability hypothesis | exploits unrelated to belief sharpening | **never built anywhere; no external precedent found** |
| **C6 scripted-adaptive** | hand-built manoeuvres with an **online model of the target's policy** that updates within a match | conventions, prior exploitation, anything with a within-match tell | anything not anticipated by the author | see below |

Two facts about the corpus's existing panel, both verified in code, and the second is narrower than
an earlier draft of this document claimed.

* The deception archetypes `silent`, `feint` and `withholder` are `V04Agent`s whose **only**
  departure is the set of half-suits they are willing to ask in
  (`engine/src/probe_deception.hpp:3-8`). They carry v0.4's belief, ask score, declaration rule and
  forced endgame. They are v0.4-strength opponents with a restriction, not adversaries.
* The scripted baselines **do** maintain a within-match model of their opponents, and it is not
  nothing: `LegacyMemory::signals[NPLAY][NSET]` is incremented on every ask
  (`engine/src/baselines.hpp:61`) and read by `detective` and `lockout` as
  `score += mem.signals[target][s] * .85` (`baselines.hpp:262`) and through `publicReplyThreat`
  (`baselines.hpp:173`); `bluffer` conditions on the immediately preceding ask
  (`baselines.hpp:271`). **What none of them does is model the target's *policy*, best-respond to it,
  or carry anything across matches.** They are fixed hand-tuned functions of a public ask tally. So
  C6 as defined — an online model *of the target's policy* — is empty today, and the correct
  statement is that narrower one.

So C2–C6 have never been instantiated. **The corpus's single exploitability figure is a
C1/A1/k=3 number.**

**The calibration obligation (T5).** A class contributes to an exploitability claim only after it
has demonstrated a **detection floor at or below the effect size being claimed**, by recovering
planted edges of known size. The corpus already contains the pattern: a deliberately handicapped
base built by zeroing the leading ask weight (the hit-probability coefficient) scores
`\numFitSanityBase` = **45.89%** against v0.5, and a 10-generation fit with the repaired optimiser
recovers it to `\numFitSanityAfter` = 48.33% (`paper/sections_v06/09-fitting.tex`,
`sec:fit-objective`). That is a ~4-point planted edge, and it is the only calibration point the
corpus has. A responder that cannot recover a planted **two**-point edge contributes nothing to a
claim about a one-point difference, and the phase-1 report must say so in those words.

The corpus's own literature note also supplies a stronger calibration target that has never been
built: a **small-Fish variant** (4 players, 2 teams, 3 half-suits of 4, 6 cards each) in which an
exact team best response is computable (`research/v06/notes/R7-literature-search.md`, §5.1, citing
`research/v04/lit/00-SYNTHESIS.md:471`). That is the only way to convert a lower bound into a
*calibrated* lower bound, and it is specified but unbuilt.

**The one LBR-specific procedure that must be imported.** Lisý & Bowling report that a greedy LBR
allowed to act too early *understated* exploitability by roughly an order of magnitude; the fix is to
sweep the decision points at which the responder is permitted to deviate and report the **maximum**
over the sweep, not a single setting. v0.7's C3/C5 responders must sweep and report the max.

### 4.5 T6 — protocol and reporting

* **Design.** Six-rotation duplicate blocks, deal-clustered percentile bootstrap, paired against a
  named control, on the same deals (`engine/src/arena.hpp:83-118`, `:140-158`;
  `paper/sections_v06/10-protocol.tex`). Note that `MatchConfig::rotations` defaults to **2**
  (`arena.hpp:57`) and several batteries (E5, E11, E2) run at 2, not 6; the rotation count must be
  stated per cell.
* **The mirror trap, stated correctly.** In a mirror **win-rate** cell the per-deal outcome is
  deterministic, so the effective sample is **zero**, not half — the artifact prints `ci [0.5, 0.5]`
  (`research/v06/results/E4-perstyle.jsonl`, `v05` vs `v05`). Halving is the right correction for
  *rate denominators* (asks, declarations), which is what `10-protocol.tex:38-42` is about. Any
  v0.7 battery containing a mirror control must treat its win-rate cell as carrying no information
  and its rate denominators as halved. (The paper's macro for this, `\vsixMirrorDuplication`, carries
  the value `2` and is typeset as "the effective sample is 2\% of the nominal one" at
  `10-protocol.tex:42`; the prose is wrong. Filed in the ledger as P-6.)
* **Power arithmetic, printed with every cell.** Half-width ≈ 98/√N points at p ≈ ½:

  | N (games) | 400 | 720 | 800 | 1,200 | 1,800 | 3,600 | 4,800 | 18,000 | 126,000 |
  |---|---|---|---|---|---|---|---|---|---|
  | 95% half-width (pts) | 4.90 | 3.65 | 3.46 | 2.83 | 2.31 | 1.63 | 1.41 | 0.73 | 0.28 |

  The corpus's exploitability cells are n = 3,600 → **±1.63 points**. A claim that v0.7 is *less*
  exploitable than v0.6 by half a point needs ≈ 38,000 games a cell unpaired, and the paired design
  buys that back only from deal variance, not from the requirement to replicate on a second bank.
* **Every claim on two disjoint banks**, reported as replicated only if it holds in sign and size on
  both.
* **Never headline an aggregate over the panel.** This applies to exploitability batteries exactly
  as it applies to strength: report the per-adversary profile, and lead with the worst cell.
* **Report the curve, not the scalar.** Parameterise the adversary by budget (generations ×
  population × games, or rollouts/decision) and report Expl as a function of it. Davis, Burch &
  Bowling's difficulty-curve construction and Timbers et al.'s simulation curriculum (which moved a
  Go exploitability estimate from ≈20% to ≈90% purely by curriculum) both show a single-budget
  number is not a property of the target.
* **AIVAT is available and should be considered.** Fish is close to the ideal case: one chance event,
  all six strategies known and deterministic. Burch et al. report a 68% standard-deviation reduction
  (≈10× data saving) in poker. If it is used, the value function must be **frozen and preregistered**
  before the holdout is opened.

---

## 5. T7 — what "homogeneous" is allowed to mean

A homogeneous team of three identical agents sounds like a severe restriction. Measured against the
solution-concept hierarchy it is **exactly two prohibitions**, and stating them precisely is what
lets phases 3–4 know what they may build.

The hierarchy is Celli & Gatti's Property 2: **v_Com ≥ v_Cor ≥ v_No**, where Com is a full
intra-play communication device, Cor a pre-play correlation device, and No is independent
randomisation. Placing FishBot:

* Three **deterministic identical** agents realise **one pure joint plan** — the point mass
  δ_(π₁,π₂,π₃) in the TMECor polytope. It is a legal element of that polytope and the degenerate
  one; its guarantee is the *pure* maxmin value.
* Three identical agents with **independent per-seat randomisation** are a product distribution: the
  **TME / v_No** regime. Randomising independently for unreadability moves the team **down** the
  hierarchy, not up. This is the trade phase 3 must price, not assume.
* Three identical agents with a **shared secret pre-play seed** span the full correlation polytope:
  **TMECor**.

**The naive worry — "identical policies cannot act differently" — does not apply to Fish.** The
counterexample the parameter-sharing literature uses (Team Rock-Paper-Scissors, in the H-PSRO line)
bites only because the shared policy has no agent identifier. Fish seats are *not* exchangeable: they
occupy distinct positions in the turn order relative to both opponents and teammates, and the public
state already distinguishes them (`PublicState::turn`, `handCount[6]`, and the full event history,
`engine/src/fish.hpp:130-143`). Any policy that conditions on the public state already conditions on
seat identity.

So the homogeneity constraint reduces to:

> **H1. No shared secret randomisation.** The three seats draw from no common signal hidden from the
> opposing team. This is the TME-vs-TMECor gap; it is bounded above by the Price of Uncorrelation
> (Basilico et al., m^{n−2} in normal form) and by Schulman & Vazirani's defensive gap (≤ 3/4 of the
> payoff range at k = 3). **Its size in Fish is unknown and nobody has measured it.**
>
> **H2. No role pre-assignment that is not a function of the public state.** "The seat holding the
> most anchors declares" is legal — it is a function of public information plus own hand, and any
> observer can evaluate it. "Seat A blackballs opponent 1, seat B blackballs opponent 2" fixed in
> advance by seat index is **not** legal under T8, because its decoding key is the index convention
> rather than the game.

Everything else — seat-conditioned behaviour, implicit signalling through ask choice, teammates
inferring each other's holdings from the public transcript — is legal, and §6 says why.

A note the literature does not state and v0.7 must: **a shared secret seed is only worth its TMECor
value if it is secret from the opponent.** Under a white-box threat model (T3) it is not. So H1 is
not merely a charter restriction; under T3 it is close to free, and measuring what it costs is one of
the cheapest decisive experiments available (ledger L8).

---

## 6. T8 — illegal side channels, defined mechanically

### 6.1 The criterion

The cooperative-AI literature (Hanabi, other-play, off-belief learning) is about a setting with **no
adversary**. Fish is not that setting: every action is public and there is an opposing team that can
read every signal. The body of rules that has actually adjudicated this exact question — *when is
coordination between partners through legal actions illegitimate?* — is duplicate bridge law, and it
gives three codified distinctions that map cleanly onto the engine:

1. **Communication may come only from legal calls and plays.** Information from outside the game is
   *unauthorized information*. → Anything passing between two seats of a team other than the public
   event stream is illegal, full stop.
2. **Full disclosure.** A *concealed partnership understanding* is an infraction. → A convention is
   legitimate iff its decoding rule is available to the opponents.
3. **Encrypted signals** — "where the precise meaning of a signal depends on a piece of information
   that only the defence know" — are technically legal but banned by regulation nearly everywhere,
   because they defeat disclosure. → **This is the object.**

> **Definition (illegal side channel).** A v0.7 team coordination mechanism is an **illegal side
> channel** iff a teammate's action depends on a distinction whose *meaning* cannot be recovered by
> an observer who knows the rules, the full public transcript, and the general form of the policy,
> but who does **not** have the binary.
>
> Equivalently: **the signal is encrypted, and the key lives in the implementation rather than in the
> game.**

**Making the criterion decidable.** "Recoverable by an observer" is not by itself a formal predicate,
so the check is discharged by a **closed list** of implementation-only discriminators. A
discriminating feature is illegal iff it reduces to one of:

* **I-1 an arbitrary tie-break** — sort order, enumeration order, seat order, hash iteration order,
  or an identically-seeded RNG. Note that this is not hypothetical in v0.6: its own path resolves
  bit-for-bit ties by **unstable `std::sort` order** (`engine/src/v06.hpp:403-405`, `:488`), and at
  `\vsixTieShareSix` = **53.80% of contested decisions** (3,613 of 6,715; 53.2% of all 6,789 ask
  decisions — `research/v06/results/E8-ties.txt`, v0.6 mirror) that is what decides the ask. *Today
  this is harmless, because no FishBot listens to it — S3 is what proves that.* It becomes a channel
  the moment any seat conditions on which of two exchangeable candidates a teammate chose.
* **I-2 a payoff-irrelevant label** — suit identity, card index encoding, seat index used as an
  identifier rather than as a position.
* **I-3 shared internal state** not derivable from the public transcript — cached beliefs, memoised
  search results, floating-point residuals, a shared or deal-derived RNG stream.

A program can decide membership in I-1..I-3 given the feature the substitution test implicates,
because each is a syntactic property of where the value came from. A feature *outside* the list is a
judgement call, and the bridge answer is available and should be used: **disclose the decoding rule
in the paper**, which converts a concealed understanding into a disclosed one. The residual risk of
this construction is stated in §6.5.

Conversely a mechanism is **legal** iff the discriminating feature is a *game* quantity — expected
value under the posterior, card counts, a half-suit's remaining distribution. Then an opponent who
models the policy can decode it too, and under T3 it does.

### 6.2 Rule-equivalence, over all four decision types

Let `s^pub_{≤t}` be the public history (which in Fish determines the posterior exactly) and `h_i` the
actor's hand. For each decision type define its **rule-forced content** `G(·)` — what the rules make
the action mean, independent of who chose it — and call two legal actions **rule-equivalent at a
node** if they share it. This is the relation the tests below quantify over, and it must cover the
whole action space of §3, not only asks.

| Decision | Rule-forced content `G` | Unforced choice that remains |
|---|---|---|
| **D1 ask** | asker holds ≥1 card of that half-suit; asker lacks the named card; target does/does not hold it; turn does/does not pass — the certificates at `engine/src/belief.hpp:156-171` | **which** card within the half-suit and **which** target: measured at 1.301 and 0.959 bits per ask (`R9` T#11) |
| **D2 declaration** | the half-suit is claimed; the six-card assignment is published; the half-suit leaves every hand (`game.hpp:189`); it is scored right or wrong | **when** to declare and **which** half-suit to declare first when more than one is ready; and, among assignments the declarer's belief cannot separate, **which** one is named |
| **D3 pass target** | the turn moves to a named live teammate | **which** teammate, among up to three: up to log₂3 ≈ 1.58 bits, chosen by the acting seat and publicly observed |
| **D4 willingness** | at forced-endgame rung θ, this seat is or is not willing to declare half-suit *s* | **at which of eight rungs** the bit flips: ≈3 bits per (seat, half-suit), on a channel `engine/src/fish.hpp:121-125` describes as "only a willingness bit" |

**D4 deserves emphasis.** The engine's comment sanctions a one-bit channel; the eight-rung ladder
(`fish.hpp:126-127`) actually carries about three bits, and the ladder is swept thresholds-outermost
(`game.hpp:241-243`) so the rung at which a seat becomes willing is publicly inferable from the
order in which declarations occur. That is the single most encrypted-signal-shaped construct in the
engine and the corpus has never audited it as one. It is, separately, measured as worth almost
nothing in strength terms (ledger C7), which is why it is a legality question and not an opportunity.

### 6.3 What is open in the engine today

Three channels exist at `60fee17` that would satisfy the illegal definition if any policy used them.
None is used by v0.4/v0.5/v0.6. All three must be closed or explicitly audited before a v0.7
exploitability number means anything.

**E-1 — the per-seat reset seed inverts to the deal seed, and therefore to the whole deal.**
`Game::setup` hands each agent `mixSeed(s, p + 77)` where `s` is the deal seed
(`engine/src/game.hpp:110`), and the same `s` generates the deal (`dealCards`,
`engine/src/fish.hpp:196-209`). `mixSeed` is `z = a·K + b + C` followed by the splitmix64 finaliser
(`fish.hpp:79-84`): the multiplier `K = 0x9E3779B97F4A7C15` is odd and therefore invertible mod 2⁶⁴,
and every finaliser step is invertible. I verified this numerically — 2,000 random round trips, all
exact — so **for a fixed `b`, `mixSeed(·, b)` is a bijection and any agent can recover `s` in closed
form from its own reset seed, then call `dealCards` and read all six hands.** The post-deal rotation
(`game.hpp:100-104`) does not block it: there are three candidate shifts and the agent's own hand
identifies which one applies. This is a complete clairvoyance oracle sitting inside the
`Agent::reset` signature. It is also why the policies' private streams are not a legitimate
randomisation source: `srng` and `rng` derive from it (`engine/src/v06.hpp:237`, `v05.hpp:149`), so
`v06:search`'s determinization draws are a deterministic function of the ground truth even though
they do not currently exploit it.
**T10 follows: the harness must hand each seat a stream drawn independently of the deal.** Without
that change S4 has no clean form, because a policy that legitimately consumes randomness cannot be
distinguished from one that reads the seed.

**E-2 — `BlockDP` parks every instance's tables in a shared per-thread pool.** This is the defect
v0.6 repaired at the query level and documented as still present at the raw level:
`\vsixAliasRawMismatch` = **175** raw shared-pool field reads differ, against 0 query mismatches
(`research/v06/results/E0-identity.txt`; `paper/sections_v06/11-results.tex`, `sec:results-verify`).
Six seats sharing one pool is exactly the structural shape of a cross-seat channel, and the repair is
a query-level guard rather than isolation. *Test:* S6.

**E-3 — `Event::confidence` is broadcast to all six seats.** The declarer's own stated confidence is
a field of the emitted event (`engine/src/fish.hpp:104`) and `Game::emit` passes the full event to
every agent (`game.hpp:145`). It is documented as "diagnostic only" and no policy reads it (verified
by grep). It has no counterpart in the physical game. *Test:* zero the field before `observe` and
require md5-identical transcripts.

A fourth item is benign but must be checked rather than assumed: **agent objects are constructed once
per thread and reused across games** (`engine/src/arena.hpp:74-79`, outside the deal loop), so any
state `reset` fails to clear is cross-game memory. v0.6's `reset` does clear the behavioural state
(`v06.hpp:235-242`, `v05.hpp:148-155`); the accumulating counters (`searched`, `changed`,
`deadPlayed`, `xbBuilds`, …) are read-only diagnostics. S4 turns this from a reading into a check.

### 6.4 The audit procedure

**S1 and S2 are measurements that calibrate the tests; S3–S6 are the pass/fail gates.** A v0.7
configuration is certified side-channel-free by passing S3, S4, S5 and S6, with S1 and S2 reported
alongside. None of the six reads only existing artifacts: each needs a small amount of harness
plumbing, and phase 1 must budget for it.

| | Test | Type | What it catches | What it needs built |
|---|---|---|---|---|
| **S1** | **Grounding budget.** The policy's expected value under the *grounded* posterior (conditioning on rule-forced observations but not on partner action *choices*) against its value under the full policy-aware posterior. | measurement | nothing by itself — it **bounds how much any side channel can be worth**, and is the number that tells phase 3 whether any of this is worth pursuing | a posterior variant that drops the policy prior (`priorTheta`/`priorPhi` already switch it off, `engine/src/factory.hpp`), plus a paired match |
| **S2** | **Signalling CMI.** `I( a_t ; h_i ∣ s^pub_{≤t}, G(a_t) )` over each decision type of §6.2, in bits. | measurement | how much unforced choice carries hand information. **On its own this is not an offence** — a good player's choice must correlate with its hand | per-decision logging of the rule-equivalence class and the chosen member |
| **S3** | **Listening substitution — the decisive test.** At a node where teammate *j* played `a`, replay with a rule-equivalent `a′` substituted (same `G`, same legality, same revealed cards) and measure ‖π_i(·∣…,a) − π_i(·∣…,a′)‖. **Pass = no change, or a change whose discriminating feature is not in I-1..I-3.** | gate | the offence itself, for all four decision types | an action-substitution replay hook; nearly free in a deterministic engine |
| **S4** | **Stream independence.** With T10's independent per-seat stream in place: a *deterministic* configuration must produce an md5-identical transcript to the deal-seeded run (proving it consumed nothing); a *stochastic* configuration must show a paired win-rate difference of zero within CI against a fixed opponent, and a per-decision action-agreement rate with the deal-seeded run no higher than chance. | gate | E-1; cross-game memory; any use of the deal seed as information — **without rejecting a policy that merely consumes noise**, which the naive md5-only form does (it would fail `v06:search` by construction) | T10's stream plumbing; a per-decision action log |
| **S5** | **Posterior-invariance (clairvoyance test).** At a sample of decision nodes, resample the other five hands from the exact posterior conditioned on the public transcript and this seat's own hand (`DealDP` + `Belief::satisfies` already do exactly this, `engine/src/v06.hpp:511-527`), and require the seat's chosen action to be unchanged. | gate | **teammate- and opponent-hand leakage only.** It holds the public transcript fixed, so an encrypted *convention* — which is carried in the transcript — passes it trivially. This is an E-1-class test, not a convention test, and S3 remains the decisive one | a resampling harness that drives the sampled deal back through the policy |
| **S6** | **Seat isolation.** Instantiate the three team seats in separate processes communicating only the serialised public event stream. Transcript must be md5-identical to the in-process run. | gate | E-2; shared statics and thread-locals; cross-seat memoisation | an event serialisation format and an IPC path — neither exists in `engine/src/` today |

S5 and S6 together are what make a *shared common-knowledge object* legal. The v0.6 search already
relies on the fact that the public deduction state is common knowledge and "is therefore one object
rather than six" (`paper/sections_v06/08-search.tex`, `sec:search-notpimc`), and the register's
V6-M3/V6-M4 items contemplate sharing it for cost. That is legitimate — the object is by construction
what every seat would compute alone — but it is legitimate *because* S5 and S6 pass, not because the
comment says so.

### 6.5 Failure modes of this definition

Stated so that phase 6 does not have to discover them.

1. **S3 is a local test.** It catches conditioning on a single substituted action. A convention spread
   over a *sequence* of actions, where no single substitution changes anything, passes.
2. **The closed list I-1..I-3 is closed by enumeration, not by proof.** It makes the predicate
   decidable for the discriminators anyone has thought of. A convention riding on a genuinely
   game-relevant but very obscure feature falls outside it, and the only remedy on offer is bridge's:
   disclose the rule.
3. **S5 samples**, so a dependence that fires on a measure-zero set of posteriors survives; and by
   construction it cannot see conventions at all (§6.4).
4. **S2 is expensive to estimate well.** Conditional mutual information over a 10³⁸-support posterior
   is estimated, not computed, and a low estimate at small sample is not a zero.
5. **Nothing here bounds legal signalling.** A v0.7 that signals heavily but legally may still be more
   exploitable, because under T3 the opponents decode it too. That is what E1 is for; the
   side-channel tests are a legality gate, not a strength measure.
6. **Passing the gates certifies the configuration, not the design.** A different frozen vector needs
   a fresh certification.

---

## 7. Rejected alternative definitions

Each entry states what the definition **would detect** and what it **would not**.

**R-1 — "keep the v0.6 probe as-is": C1 in-class, A1 synchronized, k = 3, and read the number as
exploitability.** Rejected as a *definition*; retained as a continuity column (§4.3).
*Detects:* whether a same-family counter-team beats the target — a genuine like-for-like comparison
across versions, which is what the v0.6 paper claims for it and no more.
*Misses:* everything out of class (C2–C6); everything requiring role differentiation (A0/A2); and
anything below its detection floor, which has never been measured.
Three specific defects, all verifiable:
* **The responder is not in the target's class.** `engine/exploitability_v06.sh:21` defaults
  `BASE=v05`, and the v0.6 run used it (`research/v06/runs/lbr.log`: `base=v05` on all three rows,
  including the v0.6 target). `v05` is a 34-coordinate `V05Agent` that runs the chain/threat
  re-scoring pass; v0.6 is a 37-coordinate `V06Agent` on a different scoring path with that pass off
  (`engine/src/factory.hpp:138`, `engine/src/v06.hpp:181-187`, `v06.hpp:495`). The fitted
  v0.6-targeting responder's coordinates 31–33 are `searchTopK ≈ 7`, `chainWeight = 3.41`,
  `threatWeight = 2.24`, so it does run the pass. The paper's phrase "the same policy family"
  (`paper/sections_v06/11-results.tex`, `sec:results-exploit`) is true only of the shared vector
  layout.
* **The responder fitted against v0.6 is itself a degraded policy.** In
  `research/v06/results/X1-lbr.jsonl` the v0.6-targeting responder has `declAccA` = **0.9550** and
  `forcedPerGameA` = **0.0222**, against 0.9826 / 0.0033 for the v0.5-targeting responder and
  0.9809 / 0.0039 for the v0.4-targeting one — a **2.76-point** fall in its own declaration accuracy
  and roughly a **sevenfold** rise in how often it walks into a forced endgame. The 48.36% is at
  least partly a measurement of a broken exploiter, not of a hard target.
* **The fitting budget cannot resolve the effect it is looking for.** 12 generations × population 18
  at 180 games per evaluation (`engine/exploitability_v06.sh:22-25`). Pairing buys a large factor,
  but the search is being asked to find a one-point exploit with a per-candidate signal an order of
  magnitude noisier.

**R-2 — single-seat exploitability (k = 1) as the headline.** Rejected as the headline; kept as the
tractable column (§4.1).
*Detects:* unilateral profitable deviations by one opponent — the Nash deviation space — and it is
the only variant that is polynomial.
*Misses:* every coordinated exploit (an agreed division of half-suits to contest, an agreed
blackballing schedule), which in a public-action game where teams must signal through their actions
is the interesting class. It also forfeits the two-team zero-sum structure, so the number stops being
a distance to anything.

**R-3 — exploitability against the thirteen-archetype panel.** Rejected as a definition; kept as a
regression suite.
*Detects:* regressions against known playstyles, cheaply and reproducibly.
*Misses:* everything the panel does not contain. The panel's strongest member is the previous
FishBot; **four** of its thirteen members are beaten above 95% and one (`random`) 100.00%
(`paper/tables_v06/perstyle_pooled.tex`). A fixed panel measures "beating specific opponents", which
the poker literature separates sharply from "being unexploitable" — Johanson et al. found frequentist
best responses that crushed their target and lost to everything else, while an ε-equilibrium averaged
positive against the whole pool.

**R-4 — head-to-head worst case over the panel.**
*Detects:* a catastrophic regression against a panel member, and nothing subtler.
*Misses:* everything, once the panel contains an opponent of near-equal strength: the statistic is
pinned near 50 by that cell (§1), it is exactly 50 with zero variance against a true mirror, and on
v0.6's own numbers it moves 1.33 points between a single bank (48.67) and a three-bank pool (50.00)
purely from sampling.

**R-5 — an Elo or TrueSkill rating over the panel.**
*Detects:* a coarse ordering of arms that beat and lose to the same opponents in a transitive
population.
*Misses:* any non-transitivity, which is the entire content of an exploitability question — a policy
that beats the whole panel and loses to a purpose-built counter has an excellent rating. Rejected
additionally on the project's own standing rule (`paper/sections_v06/10-protocol.tex`,
`sec:protocol-metrics`): a single scalar over a fixed scripted panel is exactly the aggregate the
rule forbids.

**R-6 — exploitability against a *human* panel.**
*Detects:* the only thing that ultimately matters, and the one failure mode every archetype misses —
a human adapting across many games, which the project owner reports happening in live play
(`R9` blocker B5).
*Misses:* nothing in principle; it is rejected on **power**, and the rejection is recorded so it is
not proposed again. At 98/√N a 1-point claim needs ≈9,600 games. No partnership trick-taking paper
found in this review reports an adequately powered human comparison — the published norm is 32 deals
(bridge), 21 matches per cell (Scopone), or a platform log with disclosed selection bias (Spades).
The project's own scoping of human play to a small number of recorded games is more honest than the
published norm and should stay scoped that way.

**R-7 — the two-sided TMECor gap `e(μ_T, μ_O)`.** Rejected as a *measurement*, retained as the object
E1 approximates.
*Detects:* distance to equilibrium — the only quantity that would license the phrase "near-optimal".
*Misses:* nothing; it is rejected because it is not computable here. It requires a best response for
both teams, each of which is NP-hard even with full knowledge of the opponent (§8), and v0.7
evaluates a policy rather than an equilibrium pair.

**R-8 — "count the bits leaked" as the exploitability metric.**
*Detects:* readability — how much of a seat's hand the transcript reveals beyond the certificates.
The corpus can already measure it exactly (0.959 / 1.301 / 4.979 bits, `R9` T#11).
*Misses:* whether any of it is usable. A policy can leak heavily and lose nothing if the leak
concerns things the opponents cannot act on, and can leak one bit and lose a half-suit. Bits belong
in the ledger as a mechanism hypothesis (L4), not in the metric.

---

## 8. What E1 does not license

* **It is not a distance to equilibrium.** Computing the exploitability of a three-seat team is
  NP-complete even when the opposing team knows the target's code exactly — Li, Zanuttini & Ventos
  (JAIR 82, 2025) place "chance move present, MAX is a team of decentralised perfect-recall agents,
  opponent model fully known and singleton" at NP-c, and Σ₂ᴾ-c when the opponent is also such a team;
  Celli & Gatti reach the same conclusion for team best response. **Every number v0.7 produces is a
  lower bound obtained by a heuristically restricted attacker, and this belongs in the definition,
  not in the limitations section.**
* **"Everything is public, therefore the game is tractable" is false.** Li, Zanuttini & Ventos
  consider exactly Fish's structure — every non-nature action publicly observable, nature acting once
  at the beginning, available actions depending only on public history — and answer the tractability
  question "No!" (IJCAI 2024). Fish's public-action property licenses exact *belief* computation,
  which the corpus already exploits correctly. It does not license exact *strategy* computation, and
  the corpus should stop implying that it does.
* **"Deterministic therefore exploitable" is not a theorem and must not be asserted.** There is no
  non-trivial upper bound on a pure strategy's exploitability, and the empirical evidence runs both
  ways: Ganzfried, Sandholm & Waugh (AAMAS 2012, Table 4), measuring worst-case exploitability
  exactly in the full unabstracted game, found purification *reduced* GS6's exploitability from 463.6
  to 349.9 mbb/h and *raised* Hyperborean's from 235.2 to 437.2 — the same operation, opposite signs.
  Determinism is a hypothesis for phase 2 (class C5), not a premise. This is the project's own
  standing habit — treat "exact Bayesian inference" as an assumption to test — applied to
  "deterministic ⇒ readable".
* **"Low measured exploitability therefore near-optimal" is forbidden.** Every method here is a lower
  bound.
* **Self-play convergence is not evidence of equilibrium.** Anagnostides, Panageas, Sandholm & Yan
  (arXiv:2502.08519, Thm 1.7) prove that computing a symmetric ε-Nash equilibrium in symmetric
  6-player (3-vs-3) team zero-sum polymatrix games is PPAD-complete, and (Thm 4.7) that symmetric
  dynamics — both sides running the same procedure, which is exactly the FishBot evaluation
  configuration — cannot converge to a first-order Nash equilibrium in polynomial time unless
  PPAD = P. Read carefully: their "symmetric" means the two *teams* mirror each other, not that the
  three teammates play identically. It is a statement about 3v3 self-play, not about homogeneous
  teams.

---

## 9. What would be new

Recorded so phase 6 can state the contribution without overclaiming, and so it can be dropped without
embarrassment if the review is wrong.

* **No team analogue of Local Best Response appears to exist.** The literature search found none.
  Fish is an unusually good domain for one: the belief is exactly computable and every action is
  public.
* **Nobody in the partnership trick-taking literature computes exploitability of a partnership.**
  Bridge, Skat, Spades, Tichu, Scopone — the strongest thing any of them does is a portfolio of fixed
  opponents.
* **The TMECor literature uniformly assumes the correlation device is the team's own — private, and
  unknown to the opponent.** A homogeneous team of three identical *published deterministic* agents
  inverts that: the device is common knowledge. No paper covering this case was found.
* **Nobody has used transcript inversion as a measured exploitability estimator against a fully
  deterministic team**, where the inversion is exact rather than probabilistic. Rebstock et al. use
  policy-based inference to play better; Lisý & Bowling use the same Bayes step to attack; the
  combination is class C5 and it is unbuilt.

---

## 10. Open questions this document does not settle

1. **What is Fish's analogue of LBR's check/call rollout?** LBR's power comes from a cheap,
   correct-enough "what if nobody does anything clever from here" continuation. Fish has no passive
   continuation — a player must ask or declare. Phase 1 must name its rollout policy, and Timbers et
   al.'s warning that "it is not obvious what a strong choice for value function is in other games"
   is the literature saying this is the hard part.
2. **What is the six-seat duplicate design for an exploitability battery?** Nothing external covers
   it. Skat's convention — play all six seat permutations of two programs over three seats — is the
   closest and should be copied.
3. **What does H1 cost in Fish?** The Price of Uncorrelation is bounded but unmeasured here
   (ledger L8).
4. **Is the A2 adversary buildable at all?** An ex-ante correlated three-seat responder is the
   headline regime and there is no off-the-shelf construction for a 3v3 game of this size. If phase 1
   cannot build one, the headline falls back to A1 **and the report must say the headline fell back**.
5. **Can S2 be estimated at useful precision?** The conditional mutual information is over a
   10³⁸-support posterior. If it cannot, S3 carries the whole definition alone, and §6.5(1) becomes
   the binding limitation rather than a footnote.
