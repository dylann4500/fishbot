# R7 — Literature refresh: SEARCH and MULTI-STEP REASONING in imperfect-information team games

**Dylan Nguyen, FishLab Research Project**
Repository: `/Users/dylan/Documents/GitHub/fish optimization`, commit `bd812fe` (v0.5).
Searches and fetches run 2026-08-23. Engine measurements run 2026-08-23 at `bd812fe`.

**Question this document answers.** How do you make a policy in a large imperfect-information
*cooperative-team* game reason multiple steps ahead and find non-obvious plans, while staying
sound and terminating — under Fish's constraints: 6 players, 2 teams of 3, 54 cards, **all card
movement public** so the only hidden variable is the initial deal, and a **CPU-only C++ engine
with no neural-network training infrastructure**.

---

## 0. Prior state, method, and the honest headline

### 0.1 What already exists (read first, so this is a delta)

- `research/v05/lit/v05-refresh.md` (799 lines) — exists. Covers stopping games / deadlock,
  information-gain bonuses, online opponent modelling, minimax-regret robustness, 2024–26
  team-zero-sum. Its §5.1 already has LAMIR and MAPLE.
- `research/v04/lit/` — **exists**, 8 files, 7103 lines total:
  `00-SYNTHESIS.md`, `signalling.md`, `cfr-team.md`, `ismcts.md`, `pimc.md`,
  `belief-inference.md`, `evaluation.md`, `engineering.md`, `fish-prior-art.md`.
  The v0.5 brief's references to `research/v04/lit/signalling.md` and
  `research/v04/lit/00-SYNTHESIS.md` are **both valid**; nothing is missing.

Coverage grep over `research/v04/lit/*.md` + `research/v05/lit/*.md` for every topic this task
names:

| topic | already covered? | where |
|---|---|---|
| ReBeL | yes | `cfr-team.md:222–224`, `signalling.md:234` |
| Player of Games / Student of Games | yes | `cfr-team.md:226`, `00-SYNTHESIS.md:629` |
| DeepStack continual re-solving | yes | `ismcts.md:509–546`, `evaluation.md:634` |
| SPARTA | yes | `cfr-team.md:343–352`, `signalling.md:373–393` |
| Lerer et al. cooperative search | yes | same |
| "naive search breaks common knowledge" | **partly** | `cfr-team.md:466` states it as a one-line pitfall; no primary source on the *closure-size* problem |
| Nayyar common-information | **cited but `[U]`** | `00-SYNTHESIS.md:631` — "Nayyar, A., Mahajan, A., Teneketzis, D. … 2013. **[U]**" |
| BAD (Foerster et al.) | yes | `signalling.md:258–283`, `belief-inference.md` |
| Sokota (MMD, CAPI, MDS) | yes | `cfr-team.md:214`, `00-SYNTHESIS.md:98–99` |
| Off-belief learning | yes | `signalling.md:395–450`, `00-SYNTHESIS.md:648` |
| piKL | yes | `signalling.md:441–445` |
| Cicero | **cited but `[U]`** | `00-SYNTHESIS.md:681` |
| LBR / exploitability | yes, thoroughly | `evaluation.md`, `00-SYNTHESIS.md:459–471` |
| αμ Pareto-front search | yes, thoroughly | `pimc.md:218–277` |
| **depth-limited solving with multiple value functions** | **NO — absent entirely** | grep `1805.08195`, `Amos`, `multi-valued state`, `Modicum` → 0 hits in all 9 files |
| **knowledge-limited subgame solving** | **NO — absent entirely** | grep `2106.06068`, `without common knowledge`, `knowledge-limited` → 0 hits |
| **self-explaining deviations / IMPROVISED** | **NO — absent entirely** | grep `2207.12322`, `Self-Explaining`, `IMPROVISED` → 0 hits |
| knowledge-based paranoia search | **NO — absent** | grep `paranoia`, `Edelkamp`, `2104.05423` → 0 hits |
| RL-fine-tuning-as-search | **NO — absent** | grep `2109.15316`, `Fickinger` → 0 hits |
| Subgame solving in adversarial team games | cited, **`[U]`/UNVERIFIED** | `00-SYNTHESIS.md:636`, `cfr-team.md:492`, `evaluation.md:283` |

### 0.2 The honest headline

**v0.5 is already doing depth-limited solving in an imperfect-information game with exactly one
value function, and the single paper that says precisely why that is unsound — and gives the
fix, on a 4-core CPU — is the one paper on this topic missing from the corpus.**

Brown, Sandholm & Amos (NeurIPS 2018) is the missing keystone. §1.4 develops this. It is the
highest-value import in this document because it is (a) directly aimed at the failure mode v0.5
has, (b) demonstrated at *master-level HUNL on 4 CPU cores and 16 GB*, and (c) its main
prerequisite — a portfolio of distinct continuation strategies — **already exists compiled in
this repository** (`engine/src/factory.hpp`, `engine/src/baselines.hpp`: v05, v04, v03, v02,
Random, Hunter, Detective, Lockout, Diversifier, Bluffer, plus the P3 deception archetypes).

Second headline: the corpus contains no theory for the fact that Fish's common-knowledge closure
is *astronomically large in world count but has a compact exact sufficient statistic*. That
combination is unusual and it is exactly the setting Zhang & Sandholm (NeurIPS 2021) and Sokota
et al. (ICLR 2024) attack from opposite directions. §1.5 and §2.3.

### 0.3 Verification marks

- **[V]** — abstract/proceedings page fetched this session; title, authors, venue, year read directly.
- **[V-prior]** — verified in `research/v04/lit/` or `research/v05/lit/`; re-checked for
  consistency here but not re-fetched. Not re-litigated.
- **[V-meta]** — metadata verified this session; a specific numeric claim comes from a search
  snippet and is labelled inline.
- **[U]** — could not fetch. Supports no recommendation.

Upgrades to previously-`[U]` entries: **Nayyar et al.** → `[V]` (§2.1), **Hu et al.
arXiv:2210.05125** → `[V]` (§4.3), **Zhang et al. Subgame Solving in Adversarial Team Games** →
`[V]` (§5.2), **Cicero** → `[V-meta]` (§4.4).

---

## 0.5 What v0.6 is starting from — measured, not asserted

Every number below was produced at commit `bd812fe` today. This is the baseline any search
proposal must beat and the budget it must fit in.

### 0.5.1 v0.5's actual lookahead

`engine/src/v05.hpp` implements exactly three things that could be called search:

1. **One-ply expectimax over a linear value function.** `askExpectedValue`
   (`engine/src/v05.hpp:437–460`) returns `p·V(hit) + (1−p)·V(miss)`, where `V` is
   `value()` (`engine/src/v05.hpp:373`), a 16-coefficient linear function of aggregate public
   features with coefficients frozen at `engine/src/v05.hpp:79–96`. Mixed into the linear ask
   score at `valueWeight = 6.47680` (`engine/src/v05.hpp:71`).
2. **A top-K = 6 two-ply chain/threat re-score** (`engine/src/v05.hpp:520–585`, `searchTopK` at
   `:34`). For each of the 6 leading candidates it *re-derives the belief* on the hit branch
   (`:542–558`) and on the miss branch (`:559–579`), then scores
   ```cpp
   // engine/src/v05.hpp:581
   double u = cs[r].u + cfg.chainWeight * p * follow - cfg.threatWeight * (1 - p) * threat;
   ```
   where `follow` is the *maximum posterior marginal* of any follow-up ask after a hit and
   `threat` the analogous quantity for the opponent after a miss.
3. **A one-step declaration EV comparison** (`engine/src/v05.hpp:791–796`).

Three properties of this that matter for v0.6:

- **It is depth 2, and only along a single hand-picked line.** `follow` and `threat` are
  *scalar probability heuristics*, not backed-up values. No opponent policy is consulted, no
  minimax or expectation is taken over the opponent's reply, no recursion.
- **It uses one value function.** There is no notion of the opponent choosing among
  continuations at the leaf. This is exactly the configuration Brown, Sandholm & Amos prove
  unsound (§1.4).
- **The wait branch is still costless.** `vWait` at `engine/src/v05.hpp:795` is evaluated with
  all-zero deltas, and `value()` (`:373–380`) has no elapsed-time input — `pub.nEvents` appears
  in `v05.hpp` only at `:698`, `:699`, `:836`, all inside the hard cap. So
  `V(wait at t) ≡ V(now)` remains an implementation identity. The v0.5 refresh's R1 (a strictly
  positive holding cost) was **not built**; termination still rests on M1 live-ask gating plus
  `forceDeclareEvents = 220` (`engine/src/v05.hpp:67`). Measured today: 95.68 events/game,
  **limit hits 0%** (`./fish match --a=v05 --b=v05 --games=60 --seed=1`). M1 has made the cap
  inert in the mirror, but the cap, not a cost, is still what makes termination a theorem.

### 0.5.2 Branching factor (new measurement)

Probe: `scratchpad/probe_branch.cpp`, 400 seeded deals, random-legal-ask rollout, 48,000
decisions, using `enumerateAsks` (`engine/src/fish.hpp:179`) unmodified.

```
legal asks per decision: mean 69.09  median 69  p90 96  max 135   (n=48000, 400 games)
at the opening decision: mean 83.43 over 400 games
```

**This is the single most important number for search design.** Fish's action branching factor
(~69) is an order of magnitude larger than Hanabi's (~20 for 2p) and comparable to Bridge
card-play, but *unlike* Bridge it persists for ~96 decisions per game with no trick structure to
collapse it. Full-width depth 3 is 69³ ≈ 3.3 × 10⁵ nodes per decision. Any v0.6 search must be
**candidate-restricted at every ply**, not just at the root — which is what αμ, KLSS and
multi-valued depth-limited solving all do.

### 0.5.3 Compute budget (new measurement)

| quantity | measured value |
|---|---|
| self-play throughput | 115.3 games/s wall on 15 cores; 79.8 games/s (`./fish bench`) |
| CPU per game | 5.20 s CPU / 120 games = **43.3 ms** |
| CPU per decision | 43.3 ms / 95.68 events = **≈ 0.45 ms** |
| one belief re-derivation `Belief::sinkhornDisj(outer=4, inner=8)` (`engine/src/belief.hpp:478`) | **11.9 µs** (probe `scratchpad/probe_cost.cpp`, 20,000 reps) |
| what v0.5's top-K chain already spends | ≤ 6 × 2 × 11.9 µs ≈ **143 µs/ask decision** |

Therefore, taking a 1200-game arena run as the unit of evaluation (114,840 decisions):

| per-decision budget | wall time for a 1200-game arena on 15 cores | belief re-derivations affordable per decision |
|---|---|---|
| 0.45 ms (today) | 3.4 s | ~38 (of which ~12 are used) |
| 10 ms | **77 s** | ~840 |
| 100 ms | 12.8 min | ~8,400 |
| 1 s | 2.1 h | ~84,000 |

**Conclusion: a 10 ms/decision search is essentially free** (a full ablation arena still runs in
under two minutes) and buys a ~22× increase in nodes over what v0.5 spends. 100 ms/decision is
affordable for the *final* evaluation but too slow for routine sweeps. This is a much larger
headroom than the v0.4 synthesis assumed, and it is the practical reason the CPU-only
constraint is not fatal.

### 0.5.4 The structural fact that decides everything

From `research/v04/lit/00-SYNTHESIS.md:14` (F1), re-confirmed against the code: card movement is
public, so **the only hidden variable is the initial deal, and all six players compute the
identical posterior from the identical public history**. Fish is an *exact* public-belief-state
game in the Nayyar/Foerster sense.

The subtlety the corpus has not stated explicitly, and which governs which search methods
transfer:

> Fish's common-knowledge closure is **enormous in cardinality** (≈ 1.9 × 10²⁸ deals from one
> seat at deal time, `pimc.md:22`) but has a **compact, exactly computable sufficient statistic**:
> the `Knowledge` object plus capacity and ask-legality (C5) constraints, from which
> `engine/src/blockdp.hpp` computes, in closed form, the partition function `Z` (`:75`),
> per-card marginals (`:316`), `P(team owns half-suit)` (`:409`) and `P(named allocation)`
> (`:456`) — all validated against brute-force enumeration by `engine/src/oracle.hpp`.

Every method below sorts into one of two piles by how it treats that fact:

- **Methods that need to enumerate or sample the closure** (ReBeL/CFR-D subgames, TB-DAG,
  full PBS value networks): blocked by cardinality. Fish's `10²⁸` is not poker's `1326`.
- **Methods that need only a sufficient statistic and a leaf evaluator** (depth-limited solving
  with multi-valued states, KLSS, αμ, MDS/update equivalence, IMPROVISED): **not blocked**. Fish
  hands them the statistic exactly, for free, already validated.

That is the load-bearing distinction of this report.

---

## 1. Test-time search in imperfect information

### 1.1 DeepStack — continual re-solving  **[V-prior]**

Moravčík, Schmid, Burch, Lisý, Morrill, Bard, Davis, Waugh, Johanson & Bowling, *DeepStack:
Expert-Level Artificial Intelligence in Heads-Up No-Limit Poker*, **Science 356(6337):508–513,
2017**. Already at `research/v04/lit/evaluation.md:634` and `ismcts.md:509–546`.

**Transferable idea.** Never store a strategy; re-solve a depth-limited subgame at every
decision from (own range, opponent counterfactual values), which are the two quantities that
make a subgame well-posed. The re-solve gadget lets the opponent choose between entering the
subgame and taking their counterfactual value, which is what preserves safety.

**Applicability to Fish: LOW as stated, and the corpus already says why correctly.**
`research/v04/lit/00-SYNTHESIS.md:290` — the gadget needs `CBV^{σ₂}(I₁)` per opponent
information set, i.e. one value per (player, possible 9-card hand). Fish has three opponents ×
C(45,9)-scale hand spaces. There is no route to that on CPU. **Do not build a re-solve gadget.**
The *idea* that survives is narrower and is stated in §1.4: a depth limit is only sound if
something at the leaf represents the opponent's ability to choose.

### 1.2 ReBeL  **[V-prior]**

Brown, Bakhtin, Lerer & Gong, *Combining Deep Reinforcement Learning and Search for
Imperfect-Information Games*, **NeurIPS 2020**, arXiv:2007.13544. `cfr-team.md:222–224`.

**Transferable idea.** Treat the public belief state β as the "state"; infostate values are
supergradients of `V(β)` (their Thm 1); running the same CFR-D-over-PBS procedure at test time
is safe with no modification (their Thm 3).

**Applicability to Fish: REJECT the algorithm, KEEP one theorem.** The v0.4 verdict
(`cfr-team.md:396` "REJECT as-is; STEAL the PBS idea") stands and this refresh does not overturn
it: CFR-D over Fish subgames requires iterating over the belief support. What is newly worth
saying is that **ReBeL's Theorem 3 is the licence for test-time search in general** — it is the
reason "search at test time" is not automatically a hack — and that the v0.6 design should aim
at the *cheapest* algorithm carrying an analogous guarantee, which is MDS (§2.3), not ReBeL.

### 1.3 Player of Games / Student of Games  **[V]**

Schmid, Moravčík, Burch, Kadlec, Davidson, Waugh, Bard, Timbers, Lanctot, et al.,
*Student of Games: A unified learning algorithm for both perfect and imperfect information
games*, **Science Advances 9(46), 2023**, arXiv:2112.03178 (submitted 6 Dec 2021 as *Player of
Games*; retitled in the 15 Nov 2023 version). Already at `cfr-team.md:226`.

**Transferable idea.** GT-CFR: *grow* the public tree during solving, alternating regret updates
(RM+ with linear averaging) with an expansion phase, guided by a counterfactual value-and-policy
network. Their Thm 2: continual re-solving exploitability grows only **linearly** in game
length. Scotland Yard — a public-observation, hidden-position game — is the closest published
analogue to Fish's information structure.

**Applicability to Fish: DIRECTION yes, IMPLEMENTATION no.** ~3500 concurrent actors and a CVPN.
The transferable piece is architectural and cheap: **grow the candidate tree adaptively rather
than fixing top-K = 6**. v0.5's `searchTopK` is a constant (`engine/src/v05.hpp:34`); GT-CFR's
lesson is that expansion should follow where the search is uncertain. That is a scheduling
change, not a network.

### 1.4 Depth-limited solving with multiple value functions — **the missing keystone**  **[V]**

Brown, Sandholm & Amos, *Depth-Limited Solving for Imperfect-Information Games*,
**NeurIPS 2018**, arXiv:1805.08195. **Absent from all nine prior lit files.**

Abstract, verbatim:

> "A fundamental challenge in imperfect-information games is that states do not have
> well-defined values. As a result, depth-limited search algorithms used in single-agent
> settings and perfect-information games do not apply. This paper introduces a principled way to
> conduct depth-limited solving in imperfect-information games by allowing the opponent to
> choose among a number of strategies for the remainder of the game at the depth limit. Each one
> of these strategies results in a different set of values for leaf nodes. This forces an agent
> to be robust to the different strategies an opponent may employ. We demonstrate the
> effectiveness of this approach by building a master-level heads-up no-limit Texas hold'em
> poker AI that defeats two prior top agents using only a 4-core CPU and 16 GB of memory.
> Developing such a powerful agent would have previously required a supercomputer."

**The transferable idea, stated precisely for Fish.** A single leaf value function silently
assumes the opponent plays one fixed continuation. That assumption is what makes depth-limited
search in an imperfect-information game unsound — the searching player over-fits to one
continuation and becomes exploitable by any other. The fix is *multi-valued states*: at the
depth limit, give the opponent a choice among **k** continuation strategies; each yields its own
leaf-value vector; the search must be good against the opponent's best choice.

**Applicability to Fish: HIGH — the highest in this document.** Four reasons, each checkable:

1. **v0.5 is exactly the unsound configuration.** `askExpectedValue`
   (`engine/src/v05.hpp:437–460`) backs up a single scalar `value()` at the leaf. No opponent
   choice is represented anywhere.
2. **The k continuation strategies already exist as compiled agents.** `engine/src/factory.hpp`
   dispatches on spec strings; `engine/src/baselines.hpp:131–395` implements
   `FishV03`, `FishV02`, `Hunter`, `Detective`, `Lockout`, `Diversifier`, `Bluffer`, `Random`,
   plus `probe_deception.hpp` archetypes, alongside `V04Agent` and `V05Agent`. Brown et al.'s
   main *cost* — generating a diverse strategy portfolio — is already paid.
3. **The compute regime is the same regime.** Their agent ran on 4 CPU cores and 16 GB. This
   engine has 15 cores and a 22× headroom at 10 ms/decision (§0.5.3). This is the one
   state-of-the-art result in the whole search literature whose hardware envelope Fish already
   sits inside.
4. **It composes with the existing value function** rather than replacing it. `value()` becomes
   `value_j()` for j = 1..k, each fitted against a different opponent portfolio member using the
   *existing* ridge-fit pipeline (`engine/build_tables_v05.py`, `freeze_config_v05.py`).

**Honest caveat.** Brown et al. is a two-player zero-sum result. Fish's team-level game is
two-team constant-sum (`pimc.md:20`: "use minimax/αβ on team score"), so the *shape* carries,
but the "opponent" at the depth limit is a *team of three acting without correlation*, and the
guarantee is not inherited. Treat multi-valued states in Fish as a **robustification heuristic
with a measurable exploitability consequence**, not as a theorem, and audit it with LBR-team
(§5.1). Second caveat: their method assumes a blueprint approximating equilibrium; v0.5 is a
tuned heuristic, not a blueprint, so the values are "robustness against the style portfolio,"
not "robustness against equilibrium deviations."

### 1.5 Knowledge-limited subgame solving  **[V]**

Zhang & Sandholm, *Subgame solving without common knowledge*, **NeurIPS 34 (2021)**,
arXiv:2106.06068. **Absent from all nine prior lit files.**

Abstract, key passage verbatim:

> "Current subgame-solving techniques analyze the entire common-knowledge closure of the
> player's current information set, that is, the smallest set of nodes within which it is common
> knowledge that the current node lies. While this is acceptable in games like poker where the
> common-knowledge closure is relatively small, many practical games have more complex
> information structure, which renders the common-knowledge closure impractically large to
> enumerate or even reasonably approximate."

Their approach works with **low-order knowledge**: on arriving at an infoset, prune any node no
longer reachable, massively reducing the tree relative to the common-knowledge subgame. They
**prove that, as is, this can increase exploitability**, then develop three avenues that restore
safety. Experimentally it reduced exploitability in every practical game tested even when
applied at every infoset, and a depth-limited version produced the first strong AI for dark
chess.

**Applicability to Fish: HIGH, and it is the exact diagnosis of the v0.4 blocker.**
`research/v04/lit/signalling.md:545` states the blocker as "a single subgame at a mid-game PBS in
Fish still has an astronomically large belief support (≤ 10²⁸). Full CFR-D is not viable." KLSS
is the published answer to precisely that sentence. Two Fish-specific notes:

- Fish's "low-order knowledge" is already computed and already exact: hard exclusions from
  missed asks, capacity constraints, and C5 ask-legality certificates all live in `Knowledge`,
  and `engine/src/v05.hpp`'s M1 live-ask gate (`liveAskGate`, `:107`) is *already* a crude
  knowledge-limited pruning rule — restrict to asks with strictly positive hard-consistent
  probability. The measured effect of that one pruning rule was large (file header,
  `engine/src/v05.hpp:19–20`: longest dead run 289 → 1, misdeclarations 10.9% → 1.9%).
  **KLSS says the same move generalises from the root move set to every node of a search tree.**
- **Take the warning seriously**: they *prove* naive knowledge-limited solving can *increase*
  exploitability. Any v0.6 pruning must be audited by LBR-team (§5.1), not assumed safe.

### 1.6 Equilibrium refinements in gadget games — 2026  **[V]**

Kubíček, Lisý & Sandholm, *Equilibrium Refinements Improve Subgame Solving in
Imperfect-Information Games*, arXiv:2601.17131 (2026). New.

Gadget games contain infinitely many Nash equilibria that are theoretically equivalent but play
differently; they advocate **gadget-game sequential equilibria** as the solution concept and
report that refined equilibria "reduce the exploitability of the overall strategy by more than
50%."

**Applicability to Fish: LOW near-term, worth one sentence in the paper.** Fish v0.6 will not be
running gadget games. The transferable meta-lesson is real though: *when a search subproblem has
many optima, which one you pick is not a tie-break — it is a 50% exploitability difference.*
That directly applies to v0.5's `argmax` over near-tied ask scores
(`engine/src/v05.hpp:582`), which currently resolves ties by enumeration order.

---

## 2. Search in cooperative games, and why naive search breaks

### 2.1 The common-information / public-belief foundation  **[V]** — upgraded from `[U]`

Nayyar, Mahajan & Teneketzis, *Decentralized Stochastic Control with Partial History Sharing:
A Common Information Approach*, **IEEE Transactions on Automatic Control 58(7):1644–1658, 2013**;
arXiv:1209.1695. Previously `00-SYNTHESIS.md:631` **[U]**; now fetched and verified.

Abstract, verbatim:

> "A general model of decentralized stochastic control called partial history sharing information
> structure is presented. In this model, at each step the controllers share part of their
> observation and control history with each other. This general model subsumes several existing
> models of information sharing as special cases. Based on the information commonly known to all
> the controllers, the decentralized problem is reformulated as an equivalent centralized problem
> from the perspective of a coordinator. The coordinator knows the common information and select
> prescriptions that map each controller's local information to its control actions. The optimal
> control problem at the coordinator is shown to be a partially observable Markov decision
> process (POMDP) which is solved using techniques from Markov decision theory."

**Transferable idea.** The coordinator does not choose *actions*; it chooses **prescriptions** —
maps from each controller's private information to actions. This is the formal reason a
cooperative team's decision problem becomes a single-agent POMDP over common information, and it
is the ancestor of BAD, CAPI and the Team Belief DAG.

**Applicability to Fish: FOUNDATIONAL, and it names v0.6's real design object.** In Fish the
common information is the entire public history (F1), so the coordinator's information state is
*exactly* the belief object the engine already maintains. A prescription for a Fish seat is a map
from that seat's 9-card hand to an ask. **This is the correct formal object for the "three
teammates must search identically" constraint** (`cfr-team.md:466`) — it explains *why*: if seat A
searches and seats B, C do not, then B and C are no longer executing the prescription that A's
belief update assumes, and the common-information reduction silently fails.

**Honest limit.** The prescription space in Fish is astronomically large (a map from C(45,9)
hands to ~69 asks), so *solving* the coordinator POMDP is out of reach. Nayyar et al. is a
correctness lens for v0.6, not an algorithm for it.

### 2.2 SPARTA and the naive-search failure  **[V]**

Lerer, Hu, Foerster & Brown, *Improving Policies via Search in Cooperative Partially Observable
Games*, **AAAI 2020**, arXiv:1912.02318. `cfr-team.md:343–352`, `signalling.md:373–393`.

The two procedures, in the authors' framing:

- **Single-agent search**: all agents but one play the agreed-upon policy; that converts the
  problem to a single-agent setting. It **assumes the other agents strictly follow the
  agreed-upon policy**.
- **Multi-agent search**: *all* agents carry out the **same common-knowledge search procedure**
  whenever computationally feasible, and fall back to the agreed-upon policy otherwise.

Results: Hanabi blueprint 24.08 → **24.61** purely at test time.

**The precise statement of "naive search breaks the common-knowledge assumption."** The
guarantee of single-agent search is conditional on the *other* agents' beliefs being computed
under the blueprint. The moment a second agent also searches, its actions are no longer
blueprint actions, the first agent's belief over its partner's private state is stale, and the
improvement guarantee is void. This is why SPARTA's multi-agent variant requires the search
procedure itself to be **common knowledge and identically executed**. The v0.4 corpus states
this as a one-line pitfall (`cfr-team.md:466`); it is worth promoting to a design constraint,
because it is the constraint most likely to be violated silently by a v0.6 implementation.

**Applicability to Fish: the CONSTRAINT transfers verbatim; the ALGORITHM's cost does not.**
`signalling.md:634` records multi-agent SPARTA at 1.8 × 10⁸ rollouts/game — dead on arrival.
But Fish makes the *constraint* unusually easy to satisfy, and this is a genuine structural
advantage worth stating in the v0.6 paper:

> Because all card movement is public and the belief is a deterministic function of the public
> history plus own hand, **any deterministic search procedure that reads only public state plus
> own hand is automatically common knowledge in Fish** — the other two teammates can reproduce
> its *decision rule* exactly, and each can reproduce its *output* on their own hand. No shared
> seed, no communication, no synchronisation is needed, provided the procedure is (a)
> deterministic and (b) a function of public state and own hand only.

v0.5 satisfies (a) and (b) today. If v0.6 introduces randomised root mixing (v0.5 refresh R3),
this property is **lost unless the randomisation is seeded from the public history** — a
one-integer change with a real correctness consequence. That is the single most important
implementation caveat in this report.

### 2.3 The update-equivalence framework: MDS — the cheap sound alternative  **[V]**

Sokota, Farina, Wu, Hu, Wang, Kolter & Brown, *The Update-Equivalence Framework for
Decision-Time Planning*, **ICLR 2024**, arXiv:2304.13138. Present in the corpus as
`00-SYNTHESIS.md:98` / `cfr-team.md`, but its significance for v0.6 is understated there.

**Transferable idea.** Do not define search as "solve a subgame." Define it as "**replicate the
update of a last-iterate-convergent algorithm**, locally, at decision time." Because the update
of mirror descent is a local policy-improvement step, this needs **no public information at
all**. Named algorithms: **mirror descent search (MDS)** for fully cooperative games — proved to
guarantee policy improvement — and **magnetic mirror descent search (MMDS)** for adversarial
games.

Reported result: in Hanabi, MDS "exceeds or matches the performance of public information-based
search while using **two orders of magnitude less search time**," the first case of a
non-public-information method beating public-information methods in that setting.

**Applicability to Fish: HIGH, and it is the best fit for a CPU-only engine.** Three reasons:

1. It sidesteps the 10²⁸-cardinality blocker entirely — by construction it needs no PBS
   enumeration and no common-knowledge closure.
2. Its unit of work is a *local policy improvement step*, which is close in shape to what
   v0.5's top-K re-score already is (`engine/src/v05.hpp:520–585`) — v0.6 would be
   **replacing an ad-hoc chain heuristic with a principled improvement operator**, not adding a
   new subsystem.
3. Two orders of magnitude cheaper than PBS search is exactly the ratio that turns "not
   affordable on CPU" into "affordable at 10 ms/decision" (§0.5.3).

**Honest caveat, and it is not small.** MDS's improvement guarantee is for **fully cooperative**
(common-payoff) games. Fish is *not* common-payoff: it is two teams, and the opponents are
adversarial. The cooperative guarantee applies *within* a Fish team only if the three opponents
are treated as part of a fixed environment — which is exactly the assumption that makes
single-agent search unsound (§2.2). The correct reading is: **MDS is the right operator for the
intra-team coordination part of the problem, and MMDS is the nominally right operator for the
adversarial part, but neither is proved for the two-teams-of-three structure.** Report it as
adopted-by-analogy and audit with LBR-team.

### 2.4 Learned Belief Search and CAPI  **[V-prior]**

- Hu, Wu, Lerer, Foerster & Brown, *Learned Belief Search*, arXiv:2106.09086 — 55–91% of exact
  search's benefit at 4.6–42× less compute. `00-SYNTHESIS.md:97`.
- Sokota, Lockhart, Timbers, Davoodi, D'Orazio, Burch, Schmid, Bowling & Lanctot, *Solving
  Common-Payoff Games with Approximate Policy Iteration* (CAPI), **AAAI 2021**,
  arXiv:2101.04237 — factorised prescriptions. `00-SYNTHESIS.md:99`.

**Applicability to Fish: LBS is REDUNDANT; CAPI is a formalism, not a tool.** LBS learns an
approximate belief model because Hanabi's exact belief is intractable. **Fish's exact belief is
already computed** — `engine/src/blockdp.hpp` gives exact `Z`, marginals, team-ownership and
named-allocation probabilities, validated against brute force by `engine/src/oracle.hpp`.
Learning an approximate belief here would be strictly worse than what exists. This is a real
Fish advantage over Hanabi and should be said plainly in the v0.6 paper: **the belief half of
the problem is solved; only the search half is open.**

### 2.5 RL fine-tuning as online planning  **[V]**

Fickinger, Hu, Amos, Russell & Brown, *Scalable Online Planning via Reinforcement Learning
Fine-Tuning*, **NeurIPS 2021**, arXiv:2109.15316. **Absent from the corpus.**

Replaces tabular search with online model-based fine-tuning of a policy network via RL; new
state of the art in self-play Hanabi at the time, and beats tabular search on Ms. Pac-Man.

**Applicability to Fish: DOES NOT TRANSFER.** It requires a policy neural network and gradient
updates at decision time. There is no NN training infrastructure in this repo and no reason to
build one for v0.6. Listed so the next reader does not re-spend the search. The one idea worth
carrying: **search and learning are interchangeable ways of spending compute at a decision
point**; in a CPU-only engine the search side is the affordable one.

---

## 3. Public belief states and common-knowledge MDPs

### 3.1 BAD  **[V-prior]**

Foerster, Song, Hughes, Burch, Dunning, Whiteson, Botvinick & Bowling, *Bayesian Action Decoder
for Deep Multi-Agent Reinforcement Learning*, **ICML 2019**. `signalling.md:258–283`,
`belief-inference.md`.

**Transferable idea.** The public-belief MDP: agents act on *public* beliefs, choosing
deterministic partial policies (prescriptions, in Nayyar's sense) that map private observations
to actions, which lets a joint policy be improved by a single-agent RL algorithm over public
belief states.

**Applicability to Fish: the FORMALISM is exactly right and already exploited; the FACTORISATION
is wrong for Fish and the corpus already caught this.** `signalling.md:635`: "BAD's independent
marginals approximation must be repaired (Sinkhorn / particle filter) for Fish's fixed hand
sizes and card conservation." That repair is what `engine/src/belief.hpp:478` (`sinkhornDisj`)
and `engine/src/blockdp.hpp` are. Nothing further to import.

### 3.2 Sokota et al. — three distinct contributions  **[V-prior]/[V]**

Already covered: MMD (ICLR 2023, `cfr-team.md:214`), CAPI (§2.4), MDS (§2.3). MDS is the one
with direct v0.6 consequence and is the one the corpus under-weights.

### 3.3 The scaling problem, stated as a Fish fact

Combining §1.5 and §2.1: Fish is simultaneously the *best* case and the *worst* case for
PBS methods. Best case, because the PBS is exact, public, and cheaply computable, and because
common knowledge of a deterministic public-state-only search procedure is automatic (§2.2).
Worst case, because the closure's cardinality (10²⁸) forbids any method that iterates over its
support. **v0.6's design principle follows directly: use the PBS as a sufficient statistic for
evaluation, never as an index set for iteration.**

---

## 4. Human compatibility, grounded play, and non-obvious plans

### 4.1 Off-Belief Learning  **[V-prior]**

Hu, Lerer, Cui, Pineda, Brown & Foerster, *Off-Belief Learning*, **ICML 2021**,
arXiv:2103.04000. `signalling.md:395–450`, `00-SYNTHESIS.md:648`. Level 1 ≈ 21 (grounded) →
level 4 = 24.10 self-play / 23.76 cross-play; Thm 1 gives a *unique* policy independent of
initialisation.

**Applicability to Fish: the DIAL is the right concept; the TRAINING is out of reach.** OBL needs
repeated RL training runs with a counterfactual belief model. But OBL's *insight* is
implementable without training and is directly relevant to search: OBL-level-1 play is
**grounded** — it assumes partners played the blueprint *for grounded reasons*, refusing to read
conventional meaning into their actions. In Fish, ask legality is itself a hard grounded
certificate (C5), so a grounded reading of teammates' asks is not an approximation, it is exact.
`00-SYNTHESIS.md:250` already predicts "OBL-1 is much closer to optimal in Fish than in Hanabi,
because ask legality carries so much grounded information." **That prediction is testable
without any training**, by comparing a v0.6 that reads only C5-hard information from teammate
asks against one that also uses the soft `priorTheta`/`priorPhi` conventional weights
(`engine/src/v05.hpp:29–30`). That is a cheap, publishable ablation.

### 4.2 piKL  **[V-prior]**

Jacob, Wu, Farina, Lerer, Hu, Bakhtin, Andreas & Brown, *Modeling Strong and Human-Like Gameplay
with KL-Regularized Search*, **ICML 2022**, arXiv:2112.07544. `signalling.md:441–445`: Thm 1
bounds KL from the anchor as `O(1/λ)`, Thm 2 gives an `O(λ)`-approximate Nash in 2p0s;
chess top-1 human-move accuracy 53.2% → 54.3%, Go 57.8% → 58.5%, Diplomacy piKL-HedgeBot 32.9%.

**Applicability to Fish: MEDIUM, and it is the correct *safety rail for search itself*, not just
for human-likeness.** The framing the corpus misses: piKL is a **regulariser that keeps a search
result from drifting arbitrarily far from a reference policy**, with a proved KL bound. For v0.6
this is directly useful even with no humans in the loop: it is the mechanism that keeps a
deeper search from breaking the *implicit conventions v0.5's teammates rely on*. A search that
finds a clever line its own teammates will misread is a net loss in a team game. A KL-anchored
search to the v0.5 policy, with λ swept, gives a *tunable, measurable* interpolation between
"v0.5 exactly" and "unconstrained search," which is also the cleanest possible ablation axis for
the paper. **λ = ∞ recovers v0.5, so the ablation cannot lose.**

### 4.3 Human-regularised search and learning  **[V]** — upgraded from `[U]`

Hu, Wu, Lerer, Foerster & Brown, *Human-AI Coordination via Human-Regularized Search and
Learning*, arXiv:2210.05125 (2022). Previously `[U]` at `00-SYNTHESIS.md:653` and
`signalling.md:675`; now fetched.

Three steps: (1) regularised search + behavioural cloning to build an improved human model;
(2) policy regularisation inside RL to get a human-like best response; (3) **regularised search
at test time to handle distribution shift when meeting real humans**. Outperforms experts with
diverse human players in ad-hoc teams, and beats a vanilla best-response-to-BC baseline in
repeated expert play.

**Applicability to Fish: LOW today (no human Fish dataset), but step (3) is the general lesson**
— test-time regularised search is the mechanism that absorbs partner distribution shift. Given
the standing memory that FishBot's playstyle should adapt to whether teammates are bots or a
human, this is the published architecture for that adaptation, and it is search-based rather
than training-based, which suits the engine.

### 4.4 Cicero  **[V-meta]** — upgraded from `[U]`

Meta Fundamental AI Research Diplomacy Team (FAIR): Bakhtin, Brown, Dinan, Farina, Flaherty,
Fried, Goff, Gray, Hu, et al., *Human-level play in the game of Diplomacy by combining language
models with strategic reasoning*, **Science 378(6624):1067–1074, 2022**,
DOI 10.1126/science.ade9097. Metadata verified this session (Science page, PubMed 36413172);
**no numeric claim below is sourced from a snippet**.

**Transferable idea (structural, not linguistic).** Diplomacy is a 7-player mixed
cooperative/competitive game; Cicero's planning core is piKL-style KL-anchored equilibrium
search over *actions*, with the language model bolted on for negotiation. The strategic lesson
that transfers to Fish: **in a multi-player game with allies, the planner's job is to compute an
action that is good given a *model of what the other players will actually do*, anchored to
human-like behaviour, rather than an equilibrium action.**

**Applicability to Fish: CONCEPTUAL only.** Fish has no negotiation channel beyond the
rules-sanctioned willingness bit (v0.5 refresh R6). Do not import Cicero's architecture. Do
import the framing for the paper's related-work section: Fish sits in the same
adversarial-team-with-public-actions family as Diplomacy, and the planning-side answer in that
family has consistently been *anchored search*, not equilibrium computation.

### 4.5 AH2AC2 — the human-compatibility benchmark  **[V-prior]**

Dizdarević et al., *Ad-Hoc Human-AI Coordination Challenge*, **ICML 2025**, arXiv:2506.21490.
Already at `signalling.md:447` with the full table. The headline stands: OBL(L4) scores 21.04
with human proxies **using no human data at all**, beating BR-BC at 19.41. Do not build v0.6
around a small human Fish corpus.

### 4.6 Self-Explaining Deviations / IMPROVISED — **finding non-obvious plans**  **[V]**

Hu, Sokota, Wu, Bakhtin, Lupu, Cui & Foerster, *Self-Explaining Deviations for Coordination*,
**NeurIPS 35 (2022)**, arXiv:2207.12322. **Absent from the corpus.** This is the single most
on-topic paper for the "find non-obvious plans" half of the task.

**Transferable idea.** A **self-explaining deviation** is an action that deviates from what
common understanding says is reasonable, taken *specifically so that a teammate, reasoning by
theory of mind, concludes that circumstances must be abnormal*. The algorithm, **IMPROVISED**
(improvement-maximising self-explaining deviations), searches for such deviations. In Hanabi it
is the **first method to produce finesse plays** — the canonical human theory-of-mind play.

**Applicability to Fish: HIGH conceptually, MEDIUM–HIGH practically, and it is the most
interesting v0.6 research direction in this document.** The Fish analogue is exact and is
currently unexploited:

- v0.5's information features are all *negative* — the v0.5 refresh §2.1 established that
  `f[14] = binEnt(p)` carries weight **−2.42663** (`engine/src/v05.hpp:53`), i.e. the policy
  charges for information. So v0.5 cannot, even in principle, play an ask *for its effect on a
  teammate's beliefs*.
- But Fish's ask-legality rule makes deviations **self-explaining by construction**: asking for a
  card in half-suit S proves the asker holds another card of S (C5). A deliberately "wrong-looking"
  ask — one with low hit probability but which certifies a holding a teammate could not otherwise
  deduce — is precisely an SED. **Fish's C5 certificate is a built-in, rules-guaranteed
  theory-of-mind channel that Hanabi has to learn.**
- The measurable prediction: an IMPROVISED-style search — enumerate asks, score each by
  `(expected material) + λ·(improvement in the team's joint posterior conditioned on the
  certificate it emits)` — should produce Fish's equivalent of a finesse. The belief machinery to
  compute that second term already exists: run `blockdp` marginals for a teammate's viewpoint
  with and without the C5 constraint the ask would emit.

**Honest caveat.** IMPROVISED in Hanabi assumes partners run a known blueprint and do theory of
mind against it. Three independent FishBot seats do satisfy "known blueprint" (they are the same
compiled policy), so this holds *in self-play* — but an SED is exactly the kind of play that
**breaks against a human partner or a different bot version**, which is the same failure mode
the corpus already flags for conventions (`signalling.md:639`). Gate SEDs behind the
partner-awareness switch, and measure cross-play, not just self-play.

---

## 5. Best response and exploitability for team games

### 5.1 LBR  **[V-prior]**

Lisý & Bowling, *Equilibrium Approximation Quality of Current No-Limit Poker Bots*,
arXiv:1612.07547 (AAAI-17 Workshop). `evaluation.md`, `00-SYNTHESIS.md:459–471, 714`.

LBR uses a tiny search tree — looking about two actions ahead, at most one opponent action, over
a restricted action subset — and greedily maximises expected utility under a "check/call to the
end" assumption, giving a **lower bound** on exploitability. All ACPC 2016 bots came out >3,180
mBB/h exploitable.

The two pitfalls the corpus already records and that must be honoured:
**(i)** greedy LBR acting too early *understated* exploitability ~10× in poker — sweep when LBR
is allowed to act and report the **maximum** over sweeps; **(ii)** LBR scored −536 mbb/g against
a bot whose true exploitability was 90 — **a negative LBR score proves nothing.**

**Applicability to Fish: HIGH, already specified at `00-SYNTHESIS.md:246, 459–471` as Tier-B item
14 ("1 week"), and still not built.** This refresh adds one argument for building it *now* rather
than later: **every method in §1 and §2 that this document recommends comes with a published
warning that it can increase exploitability** (Zhang & Sandholm prove it for KLSS; Brown et al.'s
whole motivation is robustness; MDS's guarantee does not cover two-teams-of-three). Without
LBR-team, v0.6 cannot tell a genuine search improvement from an over-fit to the opponent
portfolio. **LBR-team is now a prerequisite for the search work, not a follow-up to it.** The
corpus's validation plan is also right and cheap: validate the LBR implementation on the
"small-Fish" variant (4 players, 2 teams, 3 half-suits of 4, 6 cards each) where exact team best
response is computable (`00-SYNTHESIS.md:471`).

### 5.2 Subgame solving in adversarial team games  **[V]** — upgraded from `[U]`

Zhang, Carminati, Cacciamani, Farina, Olivieri, Gatti & Sandholm, *Subgame Solving in Adversarial
Team Games*, **NeurIPS 35 (2022)**. Previously `[U]` (`00-SYNTHESIS.md:636`) and explicitly
UNVERIFIED (`cfr-team.md:492`, `evaluation.md:283`); the NeurIPS proceedings abstract page was
fetched this session.

The first approach for refining strategies via subgame solving in adversarial team games: build
a gadget game from the **team belief DAG**, refine the blueprint by **column generation** in
anytime fashion. Key result: **when the blueprint is sparse, the algorithm runs in polynomial
time given a best-response oracle, avoiding full expansion of the exponential worst-case team
belief DAG.**

**Applicability to Fish: LOW as an algorithm, IMPORTANT as a negative result.** The v0.4 corpus
already establishes the blocker independently (`signalling.md:633`): "Team Belief DAG is not
buildable for Fish — the k-private bound `O*((b+1)^k)` blows up because each of three teammates
has an unrestricted 9-card hand." The newly verified detail *sharpens* rather than rescues this:
the polynomial-time escape hatch is conditional on **blueprint sparsity**, and a Fish blueprint
is not sparse — v0.5's ask policy has support over a ~69-action set at ~96 decisions per game.
**State this explicitly in the v0.6 paper as a verified non-transfer**, since it is the most
natural thing a reviewer will ask about.

### 5.3 Adapting beyond the depth limit and continual depth-limited responses  **[V]**

- Milec, Kubíček & Lisý, *Adapting Beyond the Depth Limit: Counter Strategies in Large
  Imperfect Information Games* (ABD), arXiv:2501.10464 (2025). Already in `v05-refresh.md` §3.2.
- Milec, Kubíček & Lisý, *Continual Depth-limited Responses for Computing Counter-strategies in
  Sequential Games* (CDR), arXiv:2112.12594 (2021, rev. 2024). **New to the corpus.** Abstract:
  combines limited look-ahead solving with an opponent model to (1) approximate a best response
  in large games or (2) compute a **robust response with control over the robustness of the
  response**, in real time against previously unseen strategies; and — the load-bearing sentence
  — *"existing robust response methods do not work combined with limited look-ahead solving of
  the shelf, and we propose a novel solution for the issue."*

**Applicability to Fish: HIGH, and CDR is the paper that connects §1.4 to the v0.5 refresh's R4.**
The v0.5 refresh recommended Restricted Nash Response / data-biased shrinkage as the shape of the
opponent-model knob. CDR is the published warning that **you cannot naively bolt a robust-response
method onto a depth-limited search** — the composition breaks — and it supplies the fix. If v0.6
builds both a depth-limited search (§1.4) and an online opponent model (v0.5 R4), CDR is the
paper that tells you how they compose. This is the most specific "do not make this mistake" item
in the document.

---

## 6. Trick-taking search that runs on a CPU with no network

This section exists because the CPU-only constraint eliminates most of §1–§4 as implementations,
and the trick-taking literature is where the CPU-only, no-NN, sound-ish search methods live.

### 6.1 αμ  **[V-prior]** — already well covered, and the covering is good

Cazenave & Ventos (2019); Cazenave, Legras & Ventos (2021). `pimc.md:218–277`,
`00-SYNTHESIS.md`, `belief-inference.md`.

Backs up **Pareto fronts of boolean world-vectors** instead of scalar averages, repairing both
strategy fusion and non-locality. Bridge 3NT 60.2% → 63.0% at M=2 with 20 worlds. Timing
(52 cards, 20 worlds): M=1 0.096 s/move; M=3 optimised **1.032 s** after the 2021 improvements.
`pimc.md:264`: αμ differs from PIMC on only ~1–3% of decisions yet gains 1–3 points — "PIMC's
errors are concentrated in a small number of high-leverage positions."

**Applicability to Fish: MEDIUM, and the corpus's own reasoning caps it.** `pimc.md:23` (F-14) is
decisive and this refresh confirms it: **"Fish's perfect-information game is nearly trivial,
which is exactly the worst case for PIMC"** — under full information the player on turn never
fails an ask, so the double-dummy value collapses to "whichever team is on turn sweeps what it
can reach," and PIMC degenerates to greedy `max P(target holds card)`. αμ repairs PIMC's
*unsoundness*, not PIMC's *degeneracy*, and Fish suffers the latter. The transferable piece is
narrower but real: **Pareto-front backup over worlds is the right way to represent "this plan
works in world set A, that plan works in world set B" without committing to a scalar average** —
which is structurally the same move as multi-valued states (§1.4), one ply down. And αμ's timing
table is the existence proof that vector-valued search over ~20 worlds is affordable in
~1 s/move on a CPU, i.e. ~100× the budget v0.6 should be targeting.

### 6.2 Knowledge-based paranoia search  **[V]** — new to the corpus

Edelkamp, *Knowledge-Based Paranoia Search in Trick-Taking*, arXiv:2104.05423 (2021).

Combines **partial-information game-tree search with knowledge representation and reasoning**,
initiating a **worst-case analysis** after several tricks to guide card selection. Applied to
3-player Skat, with variants for declarer and defenders. Reported result: over **1,000 points**
on the extended Seeger tournament scale, described as superior to human play when replaying
thousands of expert games. Compute cost is not stated on the abstract page — **flagged; do not
cite a cost for it.**

**Applicability to Fish: HIGH, and it is the closest methodological neighbour v0.6 has.** It is
the only found method that (a) runs on CPU with no network, (b) is built around *deduced
knowledge* rather than sampled worlds, and (c) reasons about the **worst case over consistent
worlds** rather than the average. All three match Fish exactly:

- Fish's deduction engine is stronger than Skat's: `Knowledge` carries hard exclusions,
  capacities and C5 certificates, and `blockdp.hpp` closes them exactly.
- Worst-case-over-consistent-worlds is the right criterion for a *declaration*, which is Fish's
  irreversible action. v0.5 declares on `P(allocation correct)` thresholds and an EV comparison
  (`engine/src/v05.hpp:791–796`), i.e. an *average-case* criterion. A paranoia-style
  "is there any consistent world in which this declaration fails, and how much mass does it
  carry" check is a natural, cheap, and *provably sound* guard — and `blockdp.hpp:456`
  (`allocationProbability`) already computes the ingredient.
- The v0.5 misdeclaration rate is 1.9% (`engine/src/v05.hpp:20`) and today's mirror run shows
  declarations at **97.77%** accuracy. A worst-case guard attacks the residual directly.

**Honest caveat.** Skat's search is over a ~10-trick horizon with a small branching factor; Fish
has ~69 actions over ~96 decisions. The *worst-case-over-worlds evaluation* transfers; the
*game-tree search* around it does not, at least not full-width.

### 6.3 Newer trick-taking infrastructure — 2025/2026  **[V-meta]**

- Goadrich, Morenville & Piette, *Valet: A Standardized Testbed of Traditional
  Imperfect-Information Card Games*, arXiv:2603.03252 (2026). 21 traditional
  imperfect-information card games encoded in the RECYCLE card-game description language, with
  empirical characterisation of branching factors and game duration, and MCTS baselines against
  random opponents. **Whether any of the 21 is a team game is not stated on the abstract page —
  flagged.**
  **Applicability: LOW for algorithms, MEDIUM for the paper.** If Valet's branching-factor
  characterisation includes team card games, Fish's measured mean of 69.1 asks/decision (§0.5.2)
  becomes directly comparable to a published table — cheap external calibration for the v0.6
  paper's "Fish is hard" claim. Worth one fetch of the full PDF before the paper is written.
- *Outer-Learning Framework for Playing Multi-Player Trick-Taking Card Games: A Case Study in
  Skat*, arXiv:2512.15435 (Dec 2025) **[U]** — surfaced in search, not fetched. Listed so the
  next reader does not re-spend it.

### 6.4 A negative search result worth recording

Query: a 2026 method for information-set search in a *team* card game that is CPU-efficient and
network-free. **Nothing found.** Four query formulations returned only: single-player or
2-player MCTS work, transformer/observation-space planners (GO-MCTS, arXiv:2404.13150), DQN
variants for Wizard/Schnapsen, and LLM-agent card benchmarks. Combined with the v0.5 refresh's
re-confirmation that there is **no published academic work on Literature / Canadian Fish at all**,
the position is: *a competent CPU-only search for a 6-player 2-team public-action card game
would itself be a contribution*, because no one has published one.

---

## 7. 2025–2026 specifically

Verified this session, ordered by relevance to v0.6:

| item | year/venue | one-line verdict for Fish |
|---|---|---|
| Kubíček & Lisý, LAMIR, arXiv:2510.05048 — **ICLR 2026** (per the PDF header on arXiv) | 2025/2026 | Already in `v05-refresh.md` §5.1. Learns an abstracted model so subgame look-ahead is tractable. **Right direction, needs a learned model — defer.** |
| Kubíček, Lisý & Sandholm, *Equilibrium Refinements Improve Subgame Solving*, arXiv:2601.17131 | 2026 | >50% exploitability reduction from picking the *right* gadget equilibrium. **Not directly applicable; the tie-breaking lesson is (§1.6).** |
| Dizdarević et al., AH2AC2, **ICML 2025**, arXiv:2506.21490 | 2025 | Already in the corpus. Do not build v0.6 on a small human corpus. |
| Milec, Kubíček & Lisý, ABD, arXiv:2501.10464 | 2025 | Already in `v05-refresh.md`. Depth-limited search that adapts to sub-rational opponents while staying robust to rational ones. **Most transplantable 2025 result; pairs with §1.4 and §5.3.** |
| Carminati, Zhang, Farina, Gatti & Sandholm, *Hidden-Role Games*, **EC 2024**, arXiv:2308.16017 (v5, Mar 2026) | 2024/2026 | **Does not transfer.** Fish teams are public from the deal; there is no role uncertainty. Cite only to delimit scope. |
| Goadrich, Morenville & Piette, *Valet*, arXiv:2603.03252 | 2026 | Benchmark infrastructure; possible external calibration (§6.3). |
| Kelidari, Haghi & Salmani, *A Gold-Standard Study of What Makes a Lightweight Game-Playing Agent Strong*, arXiv:2607.06854 | 2026 | Gin Rummy + Leduc. Concludes the binding constraint is **information, not network capacity**, and that training recipe (trust region, curriculum, warm start, checkpoint selection) beats deeper search or bigger nets. **Applicability: MEDIUM, as a caution.** It is an RL-training paper, so it does not transfer directly, but its "held-out rule-based expert used only for evaluation, never for training" methodology is exactly the evaluation discipline v0.6 should adopt, and its headline is a real counterweight to this document's search enthusiasm. |
| LLM-Hanabi (arXiv:2510.04980), *Sparks of Cooperative Reasoning* (arXiv:2601.18077) | 2025/2026 | **Does not transfer.** LLM agents in Hanabi; no CPU-only, no engine-relevant algorithm. Recorded so the search is not repeated. |

**Nothing found in 2025–26 overturns the v0.4 architecture** (exact block-DP belief, single
information-set view, declaration-as-optimal-stopping, DDS-is-degenerate). The 2025–26 movement
is entirely in *how to spend test-time compute soundly*, which is exactly v0.6's question.

---

## 8. Consolidated applicability verdicts

| # | method | citation | transferable idea | Fish verdict |
|---|---|---|---|---|
| 1 | **Depth-limited solving, multi-valued states** | Brown, Sandholm & Amos, NeurIPS 2018 | Opponent picks among k continuations at the leaf; k value functions, not one | **ADOPT.** v0.5 is the unsound single-value case; the k strategies exist in `factory.hpp`; demonstrated on 4 CPU cores |
| 2 | **Knowledge-limited subgame solving** | Zhang & Sandholm, NeurIPS 2021 | Prune to low-order knowledge instead of the full CK closure | **ADOPT the principle.** Generalises v0.5's M1 gate from the root to every ply. Proven capable of *increasing* exploitability — must be LBR-audited |
| 3 | **Mirror Descent Search** | Sokota et al., ICLR 2024 | Search = replicate a last-iterate update locally; needs no public-info enumeration; 100× cheaper than PBS search | **ADOPT by analogy.** Best CPU fit. Guarantee is for common-payoff games only — Fish is not; say so |
| 4 | **Self-explaining deviations / IMPROVISED** | Hu et al., NeurIPS 2022 | Deviate deliberately so a teammate's theory of mind infers the abnormality | **ADOPT as the v0.6 research bet.** Fish's C5 ask-legality certificate is a rules-guaranteed SED channel; produces the Fish analogue of a finesse. Gate behind partner-awareness |
| 5 | **Knowledge-based paranoia search** | Edelkamp, arXiv:2104.05423 | Worst-case analysis over deduced-consistent worlds, CPU-only, no network | **ADOPT for declarations.** `blockdp.hpp:456` already computes the ingredient. Search half does not transfer |
| 6 | **piKL / KL-anchored search** | Jacob et al., ICML 2022 | Bound KL from a reference policy; `O(1/λ)` | **ADOPT as the safety rail.** λ = ∞ recovers v0.5, so the ablation cannot lose |
| 7 | **CDR / ABD** | Milec, Kubíček & Lisý, 2021/2025 | Robust responses *composed with* depth-limited solving — and the warning that the naive composition fails | **ADOPT the warning.** Required reading if v0.6 builds both search and an opponent model |
| 8 | **LBR-team** | Lisý & Bowling, 2017 | Cheap 2-ply lower bound on exploitability | **BUILD FIRST.** Prerequisite, not follow-up: every method above can silently increase exploitability |
| 9 | **SPARTA multi-agent search** | Lerer et al., AAAI 2020 | All teammates must run an identical, common-knowledge procedure | **CONSTRAINT transfers verbatim; algorithm does not** (1.8×10⁸ rollouts/game). Fish satisfies the constraint free *iff* the procedure is deterministic in (public state, own hand) |
| 10 | **Nayyar common information** | Nayyar, Mahajan & Teneketzis, TAC 2013 | Coordinator chooses prescriptions over common information | **FOUNDATIONAL lens.** Explains *why* #9's constraint exists. Not solvable at Fish scale |
| 11 | **αμ** | Cazenave & Ventos, 2019/2021 | Pareto fronts of world-vectors instead of scalar averages | **PARTIAL.** Repairs PIMC unsoundness, but Fish's DDS is degenerate (`pimc.md:23`). Keep the vector-backup idea, drop the DDS |
| 12 | **ReBeL / SoG / DeepStack** | 2017–2023 | PBS-indexed value, continual re-solving | **REJECT as implementations.** All require iterating or learning over a 10²⁸-cardinality closure. Keep ReBeL Thm 3 as the licence for test-time search |
| 13 | **Team belief DAG subgame solving** | Zhang et al., NeurIPS 2022 | Gadget over the TB-DAG + column generation; polynomial *given blueprint sparsity* | **VERIFIED NON-TRANSFER.** Fish blueprints are not sparse (~69 actions × ~96 decisions). Cite to pre-empt the reviewer question |
| 14 | **LBS, CAPI** | 2021 | Learned/approximate belief | **REDUNDANT.** Fish's belief is exact and oracle-validated (`blockdp.hpp`, `oracle.hpp`) |
| 15 | **RL fine-tuning as planning** | Fickinger et al., NeurIPS 2021 | Spend decision-time compute on gradient steps | **DOES NOT TRANSFER.** No NN infrastructure, and none warranted |
| 16 | **Hidden-role games** | Carminati et al., EC 2024 | Equilibrium for games with private team assignment | **DOES NOT TRANSFER.** Fish teams are public |
| 17 | **OBL** | Hu et al., ICML 2021 | Grounded play; a dial on convention depth | **TRAINING out of reach; the GROUNDEDNESS TEST is free** — ablate C5-hard reading vs `priorTheta`/`priorPhi` soft reading |

---

## 9. Recommended v0.6 search programme, ranked by (expected gain)/(cost)

Each item names the code it touches and the measurement that would falsify it.

**S0 — LBR-team exploitability auditor. Build before anything else. ~1 week.**
Already fully specified at `research/v04/lit/00-SYNTHESIS.md:459–471`, still not built. Every
search method in §8 rows 1–5 is published with an explicit "this can increase exploitability"
caveat. Without S0 the v0.6 paper cannot distinguish a real improvement from portfolio over-fit.
Honour both poker pitfalls: sweep *when* LBR may act and report the max; never read a negative
score as safety. Validate on the small-Fish variant first.

**S1 — Multi-valued leaf states in the existing one-ply expectimax. 2–4 days.**
Replace the single `value()` call in `askExpectedValue` (`engine/src/v05.hpp:437–460`) with
`min_j value_j(...)` over k ≈ 3–4 continuation value functions, each ridge-fitted against a
different opponent from `factory.hpp` using the existing `build_tables_v05.py` pipeline. This is
Brown–Sandholm–Amos's multi-valued states at depth 1. Cost: k× the value evaluations, which is
negligible (value evaluation is arithmetic over 16 cached aggregates). Falsifier: if `min_j` and
`mean_j` and single-`value()` all score within noise on the style matrix *and* have equal LBR
lower bounds, the mechanism is inert.

**S2 — Turn the top-K chain into a real depth-limited search. 1–2 weeks.**
`engine/src/v05.hpp:520–585` already pays for 12 belief re-derivations per decision and throws
the result into two scalar heuristics (`follow`, `threat`, combined at `:581`). At a 10 ms/decision
budget the engine can afford ~840 re-derivations (§0.5.3) — a **70× increase**. Spend it on an
actual expectimax over (my ask → opponent's best reply → my follow-up), with candidate sets
restricted at *every* ply by the M1 live-ask predicate (this is KLSS's principle, §1.5) and leaf
values from S1's multi-valued function. Falsifier: measure branching-restricted node counts and
check that the depth-3 policy differs from v0.5 on more than ~2% of decisions — αμ's experience
(`pimc.md:264`) says a good search differs *rarely*; a search that differs on 40% of decisions is
mis-scaled, not smart.

**S3 — piKL-style KL anchor to v0.5, with λ swept. 2–3 days.**
Wrap S2's output in a KL-regularised selection against the v0.5 policy. λ = ∞ recovers v0.5
exactly, so the ablation has a guaranteed floor, and the λ sweep is the paper's cleanest figure:
strength and cross-play compatibility as a function of how far search is allowed to drift.
This is also the mechanism that keeps S2 from finding lines its own teammates misread (§4.2).

**S4 — Worst-case-over-consistent-worlds guard on declarations. 2–3 days.**
Edelkamp's paranoia criterion (§6.2) applied to the one irreversible action. `blockdp.hpp:456`
(`allocationProbability`) and `:409` (`teamOwnsProbability`) already supply the quantities;
v0.5's declaration path (`engine/src/v05.hpp:791–796`) is an average-case EV comparison. Add the
worst-case check as a *veto*, not a replacement, and report the resulting frontier of
(declaration rate, declaration accuracy). Today's baseline: 4.49 declarations/game at 97.77%.

**S5 — Self-explaining deviations: the research bet. 2–4 weeks, highest variance.**
Score each candidate ask by `material + λ_SED · ΔH_team(certificate)`, where the second term is
computed by re-running `blockdp` marginals from a teammate's viewpoint with and without the C5
constraint the ask would emit. Fish's ask-legality rule makes the certificate *free and
guaranteed*, which is the structural advantage over Hanabi (§4.6). Two things make this
publishable rather than merely clever: it is the first SED construction in a game where the
theory-of-mind channel is a *rule* rather than a *convention*, and its cost is one extra belief
re-derivation per candidate (11.9 µs, §0.5.3). Must be reported with cross-play, not self-play
only.

**S6 — Grounded-vs-conventional ablation (the free OBL test). 1 day.**
Compare v0.6 reading only C5-hard information from teammate asks against v0.6 also using the
soft conventional weights `priorTheta = 0.44458` / `priorPhi = 0.12198`
(`engine/src/v05.hpp:29–30`). `00-SYNTHESIS.md:250` predicts grounded play is much closer to
optimal in Fish than in Hanabi; nobody has tested it. One flag, one arena run.

**Explicitly not recommended for v0.6.** ReBeL/SoG/DeepStack implementations; team belief DAG;
learned belief models; any neural network; LLM-based agents; hidden-role machinery; gadget-game
equilibrium refinement. Reasons in §8 rows 12–16.

---

## 10. Risks and honest limits of this document

1. **The strongest recommendation (S1) rests on a two-player theorem applied to a two-team
   game.** Brown–Sandholm–Amos's soundness argument is for 2p0s. Fish's team-level game is
   two-team constant-sum, but the "opponent" at the leaf is three uncorrelated seats. Nothing is
   inherited. S1 is a robustification heuristic until LBR-team says otherwise — which is why S0
   precedes it.
2. **MDS's improvement guarantee does not cover Fish.** It is proved for fully cooperative games.
   Adopting it for the intra-team part while opponents are adversarial is an analogy.
3. **KLSS is proved able to *increase* exploitability in its naive form.** v0.5's M1 gate is
   already a naive knowledge-limited pruning rule that has never been exploitability-audited. It
   is possible that M1's large measured gains against the mirror conceal an exploitability cost
   against a best responder. This is a live, unmeasured risk **in shipped code**.
4. **SEDs are conventions, and conventions break in cross-play.** §4.6's proposal is the highest
   variance item and the one most likely to look excellent in self-play and worse against a
   human or a v0.5 partner.
5. **Compute figures are single-machine, single-run.** 115.3 games/s, 0.45 ms/decision and
   11.9 µs/`sinkhornDisj` were each measured once, on 15 cores, at the opening state for the
   belief probe. The belief cost will rise mid-game as constraints accumulate; the 10 ms budget
   table should be treated as an order-of-magnitude argument, not a schedule.
6. **The branching-factor probe uses random legal play, not v0.5 play.** Mean 69.1 characterises
   the *rule-induced* branching, which is the right number for search sizing, but v0.5's M1 gate
   restricts the *live* candidate set to a smaller number that was not measured here. Measure
   `enumerateLive`'s output size before sizing S2.
7. **Two 2025–26 items remain `[U]`**: arXiv:2512.15435 (Skat outer-learning) and the full text of
   Valet (arXiv:2603.03252, whether any of its 21 games is a team game). Neither supports any
   recommendation.
8. **Edelkamp's compute cost is unverified** — the abstract page states results, not runtime. Do
   not cite a cost for knowledge-based paranoia search.

---

## 11. BibTeX

```bibtex
@inproceedings{brown2018depthlimited,
  author    = {Brown, Noam and Sandholm, Tuomas and Amos, Brandon},
  title     = {Depth-Limited Solving for Imperfect-Information Games},
  booktitle = {Advances in Neural Information Processing Systems 31 (NeurIPS)},
  year      = {2018},
  eprint    = {1805.08195},
  archivePrefix = {arXiv},
  primaryClass  = {cs.GT},
  url       = {https://arxiv.org/abs/1805.08195},
  note      = {Master-level HUNL on a 4-core CPU and 16 GB; multi-valued states}
}

@inproceedings{zhang2021subgamesolvingwithoutck,
  author    = {Zhang, Brian Hu and Sandholm, Tuomas},
  title     = {Subgame Solving without Common Knowledge},
  booktitle = {Advances in Neural Information Processing Systems 34 (NeurIPS)},
  year      = {2021},
  eprint    = {2106.06068},
  archivePrefix = {arXiv},
  url       = {https://proceedings.neurips.cc/paper/2021/hash/c96c08f8bb7960e11a1239352a479053-Abstract.html}
}

@inproceedings{zhang2022teamsubgame,
  author    = {Zhang, Brian Hu and Carminati, Luca and Cacciamani, Federico and
               Farina, Gabriele and Olivieri, Pierriccardo and Gatti, Nicola and
               Sandholm, Tuomas},
  title     = {Subgame Solving in Adversarial Team Games},
  booktitle = {Advances in Neural Information Processing Systems 35 (NeurIPS)},
  year      = {2022},
  url       = {https://proceedings.neurips.cc/paper_files/paper/2022/hash/aa5f5e6eb6f613ec412f1d948dfa21a5-Abstract-Conference.html}
}

@inproceedings{lerer2020sparta,
  author    = {Lerer, Adam and Hu, Hengyuan and Foerster, Jakob and Brown, Noam},
  title     = {Improving Policies via Search in Cooperative Partially Observable Games},
  booktitle = {Proceedings of the AAAI Conference on Artificial Intelligence (AAAI)},
  year      = {2020},
  eprint    = {1912.02318},
  archivePrefix = {arXiv},
  url       = {https://ojs.aaai.org/index.php/AAAI/article/view/6208},
  note      = {SPARTA; Hanabi 24.08 to 24.61 at test time}
}

@inproceedings{sokota2024updateequivalence,
  author    = {Sokota, Samuel and Farina, Gabriele and Wu, David J. and Hu, Hengyuan and
               Wang, Kevin A. and Kolter, J. Zico and Brown, Noam},
  title     = {The Update-Equivalence Framework for Decision-Time Planning},
  booktitle = {International Conference on Learning Representations (ICLR)},
  year      = {2024},
  eprint    = {2304.13138},
  archivePrefix = {arXiv},
  url       = {https://openreview.net/forum?id=JXGph215fL},
  note      = {Mirror descent search; two orders of magnitude less search time than PBS search in Hanabi}
}

@inproceedings{hu2022sed,
  author    = {Hu, Hengyuan and Sokota, Samuel and Wu, David J. and Bakhtin, Anton and
               Lupu, Andrei and Cui, Brandon and Foerster, Jakob N.},
  title     = {Self-Explaining Deviations for Coordination},
  booktitle = {Advances in Neural Information Processing Systems 35 (NeurIPS)},
  year      = {2022},
  eprint    = {2207.12322},
  archivePrefix = {arXiv},
  url       = {https://proceedings.neurips.cc/paper_files/paper/2022/hash/faa6276ea12d7afeb3e42b210c86f688-Abstract-Conference.html},
  note      = {IMPROVISED; first algorithmic Hanabi finesse plays}
}

@article{nayyar2013common,
  author  = {Nayyar, Ashutosh and Mahajan, Aditya and Teneketzis, Demosthenis},
  title   = {Decentralized Stochastic Control with Partial History Sharing:
             A Common Information Approach},
  journal = {IEEE Transactions on Automatic Control},
  volume  = {58},
  number  = {7},
  pages   = {1644--1658},
  year    = {2013},
  eprint  = {1209.1695},
  archivePrefix = {arXiv},
  url     = {https://arxiv.org/abs/1209.1695}
}

@inproceedings{foerster2019bad,
  author    = {Foerster, Jakob N. and Song, H. Francis and Hughes, Edward and Burch, Neil and
               Dunning, Iain and Whiteson, Shimon and Botvinick, Matthew and Bowling, Michael},
  title     = {Bayesian Action Decoder for Deep Multi-Agent Reinforcement Learning},
  booktitle = {Proceedings of the 36th International Conference on Machine Learning (ICML)},
  year      = {2019},
  eprint    = {1811.01458},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/1811.01458}
}

@inproceedings{hu2021obl,
  author    = {Hu, Hengyuan and Lerer, Adam and Cui, Brandon and Pineda, Luis and
               Brown, Noam and Foerster, Jakob},
  title     = {Off-Belief Learning},
  booktitle = {Proceedings of the 38th International Conference on Machine Learning (ICML)},
  year      = {2021},
  eprint    = {2103.04000},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2103.04000}
}

@inproceedings{jacob2022pikl,
  author    = {Jacob, Athul Paul and Wu, David J. and Farina, Gabriele and Lerer, Adam and
               Hu, Hengyuan and Bakhtin, Anton and Andreas, Jacob and Brown, Noam},
  title     = {Modeling Strong and Human-Like Gameplay with {KL}-Regularized Search},
  booktitle = {Proceedings of the 39th International Conference on Machine Learning (ICML)},
  year      = {2022},
  eprint    = {2112.07544},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2112.07544}
}

@misc{hu2022humanregularized,
  author = {Hu, Hengyuan and Wu, David J. and Lerer, Adam and Foerster, Jakob and Brown, Noam},
  title  = {Human-{AI} Coordination via Human-Regularized Search and Learning},
  year   = {2022},
  eprint = {2210.05125},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2210.05125}
}

@article{fair2022cicero,
  author  = {{Meta Fundamental AI Research Diplomacy Team (FAIR)} and Bakhtin, Anton and
             Brown, Noam and Dinan, Emily and Farina, Gabriele and Flaherty, Colin and
             Fried, Daniel and Goff, Andrew and Gray, Jonathan and Hu, Hengyuan and others},
  title   = {Human-level play in the game of {Diplomacy} by combining language models with
             strategic reasoning},
  journal = {Science},
  volume  = {378},
  number  = {6624},
  pages   = {1067--1074},
  year    = {2022},
  doi     = {10.1126/science.ade9097},
  url     = {https://www.science.org/doi/10.1126/science.ade9097}
}

@inproceedings{brown2020rebel,
  author    = {Brown, Noam and Bakhtin, Anton and Lerer, Adam and Gong, Qucheng},
  title     = {Combining Deep Reinforcement Learning and Search for Imperfect-Information Games},
  booktitle = {Advances in Neural Information Processing Systems 33 (NeurIPS)},
  year      = {2020},
  eprint    = {2007.13544},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2007.13544}
}

@article{schmid2023sog,
  author  = {Schmid, Martin and Moravčík, Matej and Burch, Neil and Kadlec, Rudolf and
             Davidson, Josh and Waugh, Kevin and Bard, Nolan and Timbers, Finbarr and
             Lanctot, Marc and Holland, G. Zacharias and Davoodi, Elnaz and Christianson, Alden
             and Bowling, Michael},
  title   = {Student of Games: A unified learning algorithm for both perfect and
             imperfect information games},
  journal = {Science Advances},
  volume  = {9},
  number  = {46},
  year    = {2023},
  doi     = {10.1126/sciadv.adg3256},
  eprint  = {2112.03178},
  archivePrefix = {arXiv},
  note    = {Circulated 2021--2022 as ``Player of Games''}
}

@article{moravcik2017deepstack,
  author  = {Moravčík, Matej and Schmid, Martin and Burch, Neil and Lisý, Viliam and
             Morrill, Dustin and Bard, Nolan and Davis, Trevor and Waugh, Kevin and
             Johanson, Michael and Bowling, Michael},
  title   = {{DeepStack}: Expert-Level Artificial Intelligence in Heads-Up No-Limit Poker},
  journal = {Science},
  volume  = {356},
  number  = {6337},
  pages   = {508--513},
  year    = {2017},
  doi     = {10.1126/science.aam6960}
}

@misc{lisy2017lbr,
  author = {Lisý, Viliam and Bowling, Michael},
  title  = {Equilibrium Approximation Quality of Current No-Limit Poker Bots},
  year   = {2017},
  eprint = {1612.07547},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/1612.07547},
  note   = {Local Best Response; all ACPC 2016 bots exceed 3,180 mBB/h exploitable}
}

@misc{milec2021cdr,
  author = {Milec, David and Kubíček, Ondřej and Lisý, Viliam},
  title  = {Continual Depth-limited Responses for Computing Counter-strategies in
            Sequential Games},
  year   = {2021},
  eprint = {2112.12594},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2112.12594},
  note   = {Robust-response methods do not compose with limited look-ahead solving off the shelf}
}

@misc{milec2025abd,
  author = {Milec, David and Kovařík, Vojtěch and Lisý, Viliam},
  title  = {Adapting Beyond the Depth Limit: Counter Strategies in Large
            Imperfect Information Games},
  year   = {2025},
  eprint = {2501.10464},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2501.10464}
}

@misc{kubicek2025lamir,
  author = {Kubíček, Ondřej and Lisý, Viliam},
  title  = {Look-ahead Reasoning with a Learned Model in Imperfect Information Games},
  year   = {2025},
  eprint = {2510.05048},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2510.05048},
  note   = {ICLR 2026 per the arXiv PDF header}
}

@misc{kubicek2026refinements,
  author = {Kubíček, Ondřej and Lisý, Viliam and Sandholm, Tuomas},
  title  = {Equilibrium Refinements Improve Subgame Solving in Imperfect-Information Games},
  year   = {2026},
  eprint = {2601.17131},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2601.17131}
}

@misc{edelkamp2021paranoia,
  author = {Edelkamp, Stefan},
  title  = {Knowledge-Based Paranoia Search in Trick-Taking},
  year   = {2021},
  eprint = {2104.05423},
  archivePrefix = {arXiv},
  primaryClass = {cs.AI},
  url    = {https://arxiv.org/abs/2104.05423},
  note   = {Skat; over 1,000 points on the extended Seeger scale. Runtime not stated}
}

@misc{cazenave2019alphamu,
  author = {Cazenave, Tristan and Ventos, Véronique},
  title  = {The $\alpha\mu$ Search Algorithm for the Game of Bridge},
  year   = {2019},
  eprint = {1911.07960},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/1911.07960}
}

@inproceedings{fickinger2021rlfinetuning,
  author    = {Fickinger, Arnaud and Hu, Hengyuan and Amos, Brandon and Russell, Stuart and
               Brown, Noam},
  title     = {Scalable Online Planning via Reinforcement Learning Fine-Tuning},
  booktitle = {Advances in Neural Information Processing Systems 34 (NeurIPS)},
  year      = {2021},
  eprint    = {2109.15316},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2109.15316}
}

@inproceedings{sokota2021capi,
  author    = {Sokota, Samuel and Lockhart, Edward and Timbers, Finbarr and Davoodi, Elnaz and
               D'Orazio, Ryan and Burch, Neil and Schmid, Martin and Bowling, Michael and
               Lanctot, Marc},
  title     = {Solving Common-Payoff Games with Approximate Policy Iteration},
  booktitle = {Proceedings of the AAAI Conference on Artificial Intelligence (AAAI)},
  year      = {2021},
  eprint    = {2101.04237},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2101.04237}
}

@misc{hu2021lbs,
  author = {Hu, Hengyuan and Wu, David J. and Lerer, Adam and Foerster, Jakob and Brown, Noam},
  title  = {Learned Belief Search: Efficiently Improving Policies in Partially Observable Settings},
  year   = {2021},
  eprint = {2106.09086},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2106.09086}
}

@inproceedings{dizdarevic2025ah2ac2,
  author    = {Dizdarević, Tin and others},
  title     = {Ad-Hoc Human-{AI} Coordination Challenge},
  booktitle = {Proceedings of the 42nd International Conference on Machine Learning (ICML)},
  year      = {2025},
  eprint    = {2506.21490},
  archivePrefix = {arXiv},
  url       = {https://proceedings.mlr.press/v267/dizdarevic25a.html}
}

@inproceedings{carminati2024hiddenrole,
  author    = {Carminati, Luca and Zhang, Brian Hu and Farina, Gabriele and Gatti, Nicola and
               Sandholm, Tuomas},
  title     = {Hidden-Role Games: Equilibrium Concepts and Computation},
  booktitle = {Proceedings of the 25th ACM Conference on Economics and Computation (EC)},
  year      = {2024},
  doi       = {10.1145/3670865.3673616},
  eprint    = {2308.16017},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2308.16017}
}

@misc{goadrich2026valet,
  author = {Goadrich, Mark and Morenville, Achille and Piette, Éric},
  title  = {Valet: A Standardized Testbed of Traditional Imperfect-Information Card Games},
  year   = {2026},
  eprint = {2603.03252},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2603.03252},
  note   = {21 games in the RECYCLE description language; team-game coverage unverified}
}

@misc{kelidari2026goldstandard,
  author = {Kelidari, Nima and Haghi, Mohammadsaeed and Salmani, Mahdi},
  title  = {A Gold-Standard Study of What Makes a Lightweight Game-Playing Agent Strong},
  year   = {2026},
  eprint = {2607.06854},
  archivePrefix = {arXiv},
  url    = {https://arxiv.org/abs/2607.06854},
  note   = {Gin Rummy and Leduc; concludes the binding constraint is information, not capacity}
}

@inproceedings{tian2020jps,
  author    = {Tian, Yuandong and Gong, Qucheng and Jiang, Tina},
  title     = {Joint Policy Search for Multi-agent Collaboration with Imperfect Information},
  booktitle = {Advances in Neural Information Processing Systems 33 (NeurIPS)},
  year      = {2020},
  eprint    = {2008.06495},
  archivePrefix = {arXiv},
  url       = {https://arxiv.org/abs/2008.06495},
  note      = {Contract Bridge: +0.63 IMPs/board vs WBridge5. Already in research/v04/lit/}
}
```

### Unfetchable — supports no recommendation

| item | why |
|---|---|
| arXiv:2512.15435, *Outer-Learning Framework … Skat* | surfaced in search only; not fetched |
| Full text of arXiv:2603.03252 (*Valet*) — whether any of the 21 games is a team game | abstract page does not say |
| Runtime/compute cost of Edelkamp, arXiv:2104.05423 | not stated on the abstract page |
| PDF of *Subgame Solving in Adversarial Team Games* (`mit.edu` mirror) | PDF stream did not decode; **NeurIPS proceedings abstract page fetched instead, which is what §5.2 cites** |

---

### Reproducibility of the measurements in §0.5

```
cd engine && make
./fish match --a=v05 --b=v05 --games=60 --seed=1     # 115.3 games/s, 95.68 events/game, 0% limit hits
./fish bench                                          # 79.76 games/s over 400 games
```
Branching-factor and belief-cost probes: `research/v06/notes/probe_branch.cpp`, `research/v06/notes/probe_cost.cpp`
(compiled with `clang++ -std=c++20 -O3 -march=native -I engine/src`). Neither touches
`engine/src/`. Machine: 15 physical cores, darwin 25.5.0.
