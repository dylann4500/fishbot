# FishBot v0.5 — Literature refresh

**Dylan Nguyen, FishLab Research Project**
Repository: `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19` (2026-08-22).
Scope: what has appeared, or was overlooked, since `research/v04/lit/` (8 reviews, ~100k words,
~230 sources) that bears on the six v0.5 problems in `research/v05/BRIEF.md`.
Searches run 2026-08-22.

**This is a delta, not a replacement.** `research/v04/lit/00-SYNTHESIS.md` and
`signalling.md` remain correct and are assumed below. Where a v0.4 claim is now
contradicted or sharpened, it is called out explicitly.

---

## 0. Method, and what "verified" means here

Every citation below was fetched and checked for title, authors, venue and year. Entries are
marked:

- **[V]** — page fetched, metadata read directly off the abstract page or publisher page.
- **[V-abs]** — metadata verified, but a specific numeric claim attributed to it comes from a
  secondary source (search snippet) and is labelled as such inline.
- **[U]** — could not fetch; **not used to support any recommendation**. Listed only so the
  next reader does not re-spend the search.

Three v0.4 bibliography entries flagged `[U]` there are now upgraded: **Team-PSRO successors**,
**Aggarwal & How arXiv:2607.09993**, and **"Incentivizing Information Gain"**. Two remain
unfetchable (§7).

### 0.1 The single largest gap in the v0.4 corpus

Coverage grep over `research/v04/lit/*.md` (all eight reviews):

| term | files containing it |
|---|---|
| `war of attrition`, `attrition` | **none** |
| `minimax regret`, `regret-averse` | **none** |
| `distributionally robust`, `CVaR`, `soft-min` | **none** |
| `Dynkin` | **none** |
| `Restricted Nash`, `data-biased`, `safe opponent exploitation` | **none** |
| `Bayes' Bluff`, `Bayesian Policy Reuse`, `NeuPL`, `Q-Mixing` | **none** |
| `Hernandez-Leal`, `non-stationarity` | **none** |
| `deadlock` | `fish-prior-art.md` only (1 hit) |
| `opponent model` | 7 files, but see §3.0 |

So: the v0.4 corpus is excellent on *inference*, *search*, *team equilibria* and *signalling*,
and has **essentially zero coverage of the two things v0.5 actually needs** — the theory of
voluntary stopping when both sides may wait, and the theory of performing well against the
*worst* opponent style rather than the average one. That is the honest headline of this refresh.

---

## 1. Deadlock and non-termination when claiming is voluntary

### 1.1 The v0.4 theory is not wrong; it is *incomplete in a specific, named way*

`research/v04/results/E11-termination.md` argues the deadlock follows from "no further
information can arrive." The BRIEF already contradicts that (asks inside a locked half-suit
remain legal and emit C5 certificates). The literature supplies the *correct* diagnosis, and it
is sharper than either account:

**Fish's declaration decision is an undiscounted, multi-player, nonzero-sum Dynkin game
(optimal stopping game) with a costless "wait" action, and such games are known not to have
Nash equilibria.**

- Christensen, Lindensjö & Neumann, *Markovian randomized equilibria for general Markovian
  Dynkin games in discrete time*, arXiv:2307.13413v2 (12 Aug 2025) **[V]**, §1.1, verbatim:
  > "for games without discounting only existence of ϵ-equilibria has been established and there
  > are counter-examples showing that Nash equilibria do not exist even for deterministic and
  > stationary rewards"

  and, on randomised stopping times:
  > "even for this larger strategy class it can be the case that no Nash equilibrium exists even
  > in the case where an ϵ-equilibrium, for every ϵ>0, does exist"

  https://arxiv.org/html/2307.13413 (the counterexample is their ref [40]; I could not resolve
  [40] from the HTML — flagged, and no claim below depends on the identity of [40]).
- Hamadène, Hassani & Morlais, *ε-Nash Equilibria of a Multi-player Nonzero-sum Dynkin Game in
  Discrete Time*, arXiv:2201.03562 (10 Jan 2022), *Dynamic Games and Applications*
  DOI 10.1007/s13235-023-00500-3 **[V]**. N ≥ 2 players, infinite horizon, discrete time; they
  prove existence of an **ε**-Nash equilibrium by a constructive algorithm — not an exact one.
  Their payoff structure is the Fish structure: *"payoffs of the players depend on the set of
  players that stop at the termination stage, which is the minimal stage in which at least one
  player stops."* That is literally "whoever declares first, and the correctness of that
  declaration, determines the payoff."
  https://arxiv.org/abs/2201.03562

**Confidence: strongly-indicated, not measured.** Fish is discrete-time, finite-card, and
multi-player; the cited theorems are for general Markovian reward processes, so they are an
*analogy of structure*, not a theorem about Fish. But the analogy is tight enough to name the
missing ingredient exactly, and the missing ingredient is present in the code (§1.3).

### 1.2 The classical model for "both sides refuse to move": war of attrition

- Hendricks, Weiss & Wilson, *The War of Attrition in Continuous Time with Complete
  Information*, **International Economic Review 29(4):663–680, Nov 1988** **[V]**.
  https://users.ssc.wisc.edu/~khendricks2/publications/WarofAttritioninContinuousTime.pdf
  Two-player, complete information, asymmetric players; characterises the full equilibrium set.
  The relevant fact for Fish: under complete information the war of attrition has a
  **continuum** of equilibria including arbitrarily long delay, and delay is not a mistake —
  it is an equilibrium phenomenon that any symmetric deterministic policy pair will select.
  v0.4's mirror match is exactly symmetric and exactly deterministic.
- Mguni, *Stochastic Games with Minimally Bounded Action Costs*, arXiv:2407.18010 (25 Jul 2024)
  **[V]**. https://arxiv.org/abs/2407.18010
  The constructive converse: impose a **strictly positive cost on each action** and the
  two-player impulse-control stochastic game acquires a **unique value** and a Markovian saddle
  point at *strategically chosen stopping times*, computable by iterated Bellman operations.
  This is the cleanest published statement of the fix: *the well-posedness of the stopping
  problem comes from the strict positivity of the holding/action cost, not from the payoff
  structure.*

### 1.3 Where this lives in the v0.4 code, exactly

`engine/src/v04.hpp:669`:

```cpp
double vWait = value(pub, 0, 0, 0, 0, scoreDiff, turnSign, 0, 0, 0, 0);
```

The wait branch is evaluated by passing **all-zero deltas** to `value()`. And `value()`
(`v04.hpp:370–399`) computes 16 features from the 16-coefficient vector at `v04.hpp:111–127`:
score differential, control, sharpened control, locked differential, side to move, card
differential, unresolved pool, active half-suits, turn×control, my hand size, smallest friendly
hand, our/their near-complete half-suits, contested mass, turn×unresolved.

**`pub.nEvents` never enters `value()`.** There is no elapsed-time, discount, or holding-cost
feature anywhere in the value function. Therefore

$$V(\text{wait at } t) \;=\; V(\text{wait at } t+1) \;=\; \dots \;=\; V(\text{now})$$

is an identity of the implementation, not an approximation. v0.4's declaration rule is an
undiscounted Dynkin stopping rule with a **provably** zero-cost wait action. The martingale
argument in the v0.4 paper is therefore self-fulfilling: the code was built so that waiting is
free, and then observed that waiting weakly dominates.

The only thing that ever breaks the tie is the hard horizon `forceDeclareEvents = 220`
(`v04.hpp:99`, consumed at `v04.hpp:583–584, 710`) — an arbitrary move cap, precisely the
device the v0.4 synthesis §3.10 warned against ("use Srinivasan's Forced Claims rule rather
than an arbitrary move cap"). And the measured consequence is in
`research/v05/results/P0-v04-pathology.md`: declarations at/after event 220 are **58.6% wrong**
(450/768) and forced-endgame declarations are **100% wrong** (28/28), against a baseline
declaration error of 10.4%.

### 1.4 Spence signalling with option value: delay *is* a signal, and that is the danger

- Admati & Perry, *Strategic Delay in Bargaining*, **Review of Economic Studies 54(3):345–364,
  July 1987** **[V]**.
  https://gsb-faculty.stanford.edu/anat-r-admati/publications/strategic-delay-in-bargaining/
  Delay is used as a signalling device *because it is a more efficient signal of strength than
  the alternative* — low types find it harder to imitate. Crucially, the delay **does not vanish
  as the minimum time between offers goes to zero**.

  **Implication for Fish, and it is uncomfortable:** if v0.5 introduces a holding cost to break
  the deadlock, "how long you waited before declaring" becomes a costly, credible, *public*
  signal of your confidence — readable by three opponents. This is Farrell–Gibbons two-audience
  cheap talk (v0.4 `signalling.md` §2.2) applied to the *timing* channel rather than the ask
  channel, and the v0.4 review never considered timing as a channel at all. Any holding cost
  must therefore be (a) common-knowledge and deterministic, so it carries no private
  information, or (b) deliberately randomised. A cost that is a function of the agent's private
  confidence is a leak.

### 1.5 Self-inflicted non-termination in game AI: a genuine hole in the literature

Searched: `multi-agent RL agents fail to terminate / learn to stall / reward hacking
"never ends"`; `game AI infinite loop non-termination self-play repetition draw rule`;
`Hanabi infinite hint loop deadlock`.

**Result: no substantive literature.** What exists is anecdotal and always resolved by an
arbitrary cap:

- Somani's Fish agent hit it and patched with jitter plus a 200-move cap (already documented in
  v0.4 `fish-prior-art.md`).
- Perolat et al., *Mastering the Game of Stratego with Model-Free Multiagent Reinforcement
  Learning*, **Science 378 (2022)**, arXiv:2206.15378 **[V for metadata]**. Stratego itself
  carries anti-repetition rules ("two-squares"/"more-squares"). I **could not fetch the full
  text** to quote the authors' own description of DeepNash's repetitive play — science.org
  returns 403 and the arXiv PDF did not decode. A secondary search snippet asserts agents
  "repetitively move pieces back-and-forth on two squares … resulting in draws where the agent
  could have won." **Treat that as unverified; do not cite it as evidence.**

**This is a finding, not an absence of one.** Voluntary-claiming games are a real and
under-studied class, and Fish is a clean instance of one. §8 item R5 turns this into a
publishable measurement rather than a footnote.

---

## 2. Information-seeking actions in adversarial team games

### 2.1 The v0.4 review formalised the trade-off and never priced the *benefit* side

`signalling.md` §2.4 gives the per-decision secrecy rate

$$\rho(\pi;h)=\sum_{j\in\mathcal T\setminus\{i\}} I(X;A\mid h,x_j)-\sum_{k\in\mathcal O} I(X;A\mid h,x_k)$$

and §6.4 lists four instruments. None of them was ever built. What the v0.4 ask policy actually
does, from `engine/src/v04.hpp:70–90` (weights) and `:309–328` (features):

| feature | line | definition | weight |
|---|---|---|---|
| `f[0]` hit probability | 309 | `p` | **+11.5060** |
| `f[9]` information leak | 318 | `teamRevealedSet(S) ? 0 : 1` — is this half-suit *fresh*? | **−0.8536** |
| `f[14]` location entropy | 323 | `binEnt(p)` — binary entropy of the ask outcome | **−2.6534** |
| `f[15]` team owns set | 324 | `pTeamAll` | −0.8045 |
| `f[19]` leak magnitude | 328 | `fresh × (myHave/6)` | **−0.9990** |

`binEnt` is defined at `v04.hpp:135–138` as `−p log₂ p − (1−p) log₂(1−p)`.

**So v0.4 has exactly three information terms, and all three are negative.** `f[14]` is a
*negative value-of-information* term in the literal sense: the ask whose outcome resolves the
most uncertainty (p = 0.5, one full bit) is penalised by −2.65, and the ask whose outcome is
already known (p ∈ {0,1}, zero bits) is penalised by nothing. The v0.5 hypothesis in the BRIEF
("an ask policy that values only material and never information") is **understated**: v0.4 does
not merely ignore information, it *charges for it*.

Evaluating the compiled weight vector at two representative feature vectors (arithmetic over
`v04.hpp:70–90`, not a game run — labelled as an illustration):

| ask | score | breakdown |
|---|---|---|
| provably dead ask inside a half-suit the team already owns outright (`p=0`, `myHave=4`, `teamRevealedSet=true`) | **7.44** | ownership features 3/4/5/7 contribute **+8.76** at zero hit probability; `f[15]` −0.80; `f[10]` −1.12 |
| maximally informative fresh ask (`p=0.5`, `myHave=2`, `teamRevealedSet=false`) | **5.09** | `f[0]+f[1]` +6.58; information features 9/14/19 contribute **−3.84** |

That is a concrete mechanism for the two headline pathology numbers in
`research/v05/results/P0-v04-pathology.md` — 39.0% provably dead asks and 16.5% asks inside a
half-suit the actor's own team already owns. The ownership features pay ~8.8 regardless of
whether the ask can possibly succeed, and the information features tax the alternative by ~3.8.

*Caveat:* this is a static evaluation of the linear score. The shipped policy adds a one-ply
expectimax over `value()` at `linearWeight = 0.7667` / `valueWeight = 6.0432`
(`v04.hpp:107–109`), so the argmax is not the linear argmax. The sign structure of the
information terms is nevertheless exactly as stated, and `value()` has no information feature at
all (§1.3).

### 2.2 What the literature actually offers on explicit information-gain bonuses

Searched hard; the field is thin.

- Lervold, N., Peterson, G.L., King, D.W. *Incentivizing Information Gain in Hidden Information
  Multi-Action Games*, in **Computers and Games (CG 2022), Virtual Event, 22–24 Nov 2022, Revised
  Selected Papers, LNCS 13865**, DOI 10.1007/978-3-031-34017-8_6 **[V-abs]**.
  https://link.springer.com/chapter/10.1007/978-3-031-34017-8_6 (Springer redirects to an IdP;
  DTIC mirror https://apps.dtic.mil/sti/trecms/pdf/AD1181184.pdf returns 403).
  Per the publisher abstract: adds an **information-gain incentive** and a **risk
  determinization** to both PIMCTS and ISMCTS, evaluated on the multi-action hidden-information
  wargame **TUBSTAP**; reports improvement over baseline and over a cheating MCTS.
  **The exact mathematical form of the bonus is NOT verified** — I could not obtain the full
  text. v0.4's bibliography entry 50 flagged this as `[U]`; it stays partially `[U]`. Do not
  copy a formula from it; do copy the *finding* that an explicit info-gain term helps in a
  hidden-information multi-action game.
- This appears to be **the only** published work that puts an explicit information-gain term
  inside an information-set tree search. No card-game agent in the corpus prices information at
  all. Confirmed against DouZero/DouZero+/DanZero+/PerfectDou/Suphx (v0.4 §7.10) and against a
  fresh 2026 search for GuanDan/DouDizhu successors, which turned up nothing newer than
  DouZero+ on this axis.

### 2.3 The Wyner/Csiszár–Körner framing, applied to policies rather than channels

The v0.4 review derives the secrecy rate but has no *algorithm*. The missing bridge exists and
was not cited:

- Alvim, Chatzikokolakis, Kawamoto & Palamidessi, *Information Leakage Games*, **GameSec 2017,
  LNCS 10575, pp. 437–457**; extended journal version **ACM TOPS 25(3):20, 1–36, 2022** **[V]**.
  https://arxiv.org/abs/1705.05030
  Abstract, verbatim: *"we consider a game-theoretic setting to model the interplay between
  attacker and defender in the context of information flow … in our games the utility of a mixed
  strategy is a **convex function of the distribution on the defender's pure actions**, rather
  than the expected value of their utilities. Nevertheless, the important properties of game
  theory, notably the existence of a Nash equilibrium, still hold."*

  **Why this matters for Fish, and why it is the most important technical import in this
  document:** leakage is a function of the *distribution over your actions*, not of the action
  you played. A deterministic ask policy has maximal leakage by construction, no matter which
  ask it picks. v0.4's ask rule is a deterministic argmax (`v04.hpp:467–495`). Therefore no
  choice of the 20 linear weights can reduce leakage below the deterministic bound — the leak
  penalty on `f[9]`/`f[19]` is *fighting the wrong variable*. The fix is randomisation over
  near-tied asks (v0.4 synthesis §1.3 already prescribes exactly this as "Smooth-UCT root mixing
  + near-noise set `A*`", Tier A item 12, half a day of work, **never built**).

  Second consequence: because utility is convex in the defender's mixed strategy, the leakage
  game is **not** a bilinear matrix game and standard regret matching does not apply
  unmodified — Alvim et al. give the algorithms. This is a real caveat if v0.5 tries to solve
  for a leakage-optimal ask distribution rather than merely smoothing.

### 2.4 Deduction-aware search in an adversarial *team* game

- Serrino, Kleiman-Weiner, Parkes & Tenenbaum, *Finding Friend and Foe in Multi-Agent Games*
  (DeepRole), **NeurIPS 32 (2019)** **[V]**.
  https://papers.nips.cc/paper/8408-finding-friend-and-foe-in-multi-agent-games
  Integrates **deductive reasoning directly into vector-form CFR** so the agent reasons about
  joint beliefs and deduces partially observable actions; beats hand-crafted and learned agents
  in 5-player Avalon and beats humans as both cooperator and competitor.

  **Not in the v0.4 bibliography at all** (grep: `DeepRole`, `Avalon`, `Serrino` → 0 hits in
  `research/v04/lit/`). It is the closest published system to Fish's actual structure — a
  hidden-role adversarial team game where every action is public and its *informational* content
  is the whole game — and it is the existence proof that hard deduction can live inside a CFR
  solver rather than beside it.

---

## 3. Online opponent modelling within a single episode (~100 observations)

### 3.0 What v0.4 has, and why the grep count is misleading

`opponent model` appears in 7 of 8 v0.4 reviews, but always as *inference over the deal given a
fixed opponent policy* (`belief-inference.md` §4.7, policy-likelihood reweighting), never as
*inference over which policy the opponent is running*. In the shipped agent this collapses to
two global scalars, `priorTheta = 0.26380` and `priorPhi = 0.13280` (`v04.hpp:61–62`), identical
for all five other players and never updated during a game — as the BRIEF states. A deliberately
silent opponent (the user's manoeuvre) is misread by construction.

The relevant literature is a coherent body of work that the v0.4 review missed entirely.

### 3.1 Bayesian posterior over opponent *types*, updated online

- Ganzfried, Wang & Chiswick, *Opponent Modeling in Multiplayer Imperfect-Information Games*,
  arXiv:2212.06027 (12 Dec 2022, rev. 29 Jul 2024) **[V]**. https://arxiv.org/abs/2212.06027
  Abstract, verbatim: *"We present an approach for opponent modeling in multiplayer
  imperfect-information games where we collect observations of opponents' play through repeated
  interactions. We run experiments against a wide variety of real opponents and exact Nash
  equilibrium strategies in three-player Kuhn poker and show that our algorithm significantly
  outperforms all of the agents, including the exact Nash equilibrium strategies."*
  **Multiplayer**, not two-player — the only opponent-modelling paper found that is explicitly
  in the >2-player regime Fish lives in.
- Rosman, Hawasly & Ramamoorthy, *Bayesian Policy Reuse*, **Machine Learning 104(1):99–127,
  July 2016**, DOI 10.1007/s10994-016-5547-y; arXiv:1505.00284 **[V]**.
  https://link.springer.com/article/10.1007/s10994-016-5547-y
  The framework is *exactly* the v0.5 problem statement: select online, from a **library** of
  policies, a response to a novel task instance, where *"acting online requires 'fast' responses,
  in terms of rapid convergence, especially when the task instance has a short duration."*
  A Fish game is ~100–140 public events (`P0-v04-pathology.md`: 143.6 events/game mirror, 101.7
  vs v0.3). BPR's whole design point is the short-episode regime.
- Smith, Anthony, Wang & Wellman, *Learning to Play against Any Mixture of Opponents*,
  arXiv:2009.14180 (29 Sep 2020, rev. 3 Jun 2021); journal version in **Frontiers in Artificial
  Intelligence, 2023** **[V]**. https://arxiv.org/abs/2009.14180
  **Q-Mixing**: learn Q separately against each pure opponent strategy, then obtain Q against
  *any* mixture by weighted averaging — no retraining. Plus an **Opponent Policy Classifier**
  trained on the same data, which observes play and refines the mixture weights *during* the
  episode. Domains: grid-world soccer, a cyber-security game.
  This is the cheapest architecture that answers the user's report #3: a per-opponent posterior
  that starts at the population prior and is driven by observed asks/silences.
- Liu, Lanctot, Marris & Heess, *Simplex Neural Population Learning: Any-Mixture
  Bayes-Optimality in Symmetric Zero-sum Games*, **ICML 2022**, arXiv:2205.15879 **[V]**.
  https://arxiv.org/abs/2205.15879
  A single network **conditioned on the opponent mixture**, giving *"near optimal returns
  against arbitrary mixture policies."* This is the scalable version of Q-Mixing and it is the
  natural v0.6+ architecture if v0.5's per-opponent scalars work. Domains not stated on the
  abstract page — flagged.

### 3.2 The robustness question: stopping a deceptive opponent from driving your model

This is the user's report #3 and it has a direct, mature answer that v0.4 never cites.

- Johanson, Zinkevich & Bowling, *Computing Robust Counter-Strategies*, **NIPS 20 (2007)**
  **[V]**. https://poker.cs.ualberta.ca/publications/NIPS07-rnash.pdf
  **Restricted Nash Response.** Choose `p`; solve a modified game in which the opponent is
  forced to play a fixed model with probability `p` and is free with probability `1−p`.
  `p=1` ⇒ best response (maximally exploitative, maximally exploitable); `p=0` ⇒ equilibrium;
  intermediate `p` traces the exploitation/robustness frontier. **This is the correct shape of
  the knob for v0.5's opponent model**: not "trust the model" vs "ignore it", but a dial with a
  measurable frontier.
- Johanson & Bowling, *Data Biased Robust Counter Strategies*, **AISTATS 2009** **[V]**.
  https://johanson.ca/publications/poker/2009-aistats-dbr/2009-aistats-dbr.pdf — the refinement
  that sets `p`
  per-information-set as a function of how much data you actually have there. In Fish, where the
  per-half-suit evidence about a given opponent is 0–3 observations, this is the *right* version:
  a silent opponent produces **no** evidence and so must fall back toward the population prior
  automatically, which is exactly the failure mode the user exploited.
- Ganzfried & Sandholm, *Safe Opponent Exploitation*, **ACM Transactions on Economics and
  Computation, 2015** (Best-of-EC invited from **ACM EC 2012**) **[V]**.
  https://dl.acm.org/doi/abs/10.1145/2716322 ; PDF
  https://www.cs.cmu.edu/~sandholm/safeExploitation.teac15.pdf
  Guarantees at least the game value per period *regardless* of the opponent's strategy while
  still exploiting mistakes. The motivating scenario in this line is literally the user's
  manoeuvre: play one way to induce a model, then counter-exploit it.
- Müller, Schneider, Skoulakis, Viano & Cevher, *Best of Both Worlds: Regret Minimization versus
  Minimax Play*, arXiv:2502.11673 (17 Feb 2025, rev. 4 Jun 2025) **[V]**.
  https://arxiv.org/abs/2502.11673
  Bandit-feedback algorithms with **simultaneous** O(1) regret vs a given comparator and
  Õ(√T) regret vs any fixed strategy. In a zero-sum game with value zero this means: lose at
  most O(1) to a strong opponent while gaining Ω(T) from an exploitable one. The modern
  formalisation of "exploit without becoming exploitable."
- Yang, Meng, Hao, Zhang, Zheng & Zheng, *Towards Efficient Detection and Optimal Response
  against Sophisticated Opponents* (Bayes-ToMoP), **IJCAI 2019**, arXiv:1809.04240 **[V]**.
  https://arxiv.org/abs/1809.04240
  Handles opponents that are *themselves* running Bayesian opponent models — i.e. second-order
  reasoning — with a theoretical optimality guarantee on strategy detection, plus detection of
  previously unseen policies. Directly the "opponent is deliberately driving your model" case.
- Milec, Kovařík & Lisý, *Adapting Beyond the Depth Limit: Counter Strategies in Large Imperfect
  Information Games* (ABD), arXiv:2501.10464 (15 Jan 2025, rev. 9 Feb 2025) **[V]**.
  https://arxiv.org/abs/2501.10464
  Matrix-valued states / strategy-portfolio depth-limited search that adapts to sub-rational
  opponents **while remaining robust against rational play**. Reports *"more than a twofold
  increase in utility when facing opponents who make mistakes beyond the depth limit"* in poker
  and Battleship. The most directly transplantable 2025 result for a depth-limited Fish search.

**Summary of §3:** the entire toolkit for "model the opponent within one episode without being
manipulated" exists, is 2007–2025, and is absent from the v0.4 corpus. Nothing here needs a
neural network.

---

## 4. Worst-case rather than mean performance across opponent styles

This is stated in the BRIEF as the owner's *actual research question*, and the v0.4 corpus has
zero coverage of the corresponding literature (§0.1). It is the largest gap found.

### 4.1 The equilibrium concept that matches the question

- Aggarwal & How, *Beyond Bayesian Nash: Learning Minimax-Regret Equilibria for Adversarial Team
  Games under Asymmetric Information*, arXiv:2607.09993 (10 Jul 2026), **Transactions on Machine
  Learning Research** **[V]**. https://arxiv.org/abs/2607.09993
  v0.4's bibliography entry 89 listed this as `[U]`. It is now verified and it is the single
  most on-topic paper in this refresh: **adversarial team games** (Fish's structure) + **minimax
  regret over opponent types** (the owner's objective) + **asymmetric information** (Fish's
  hidden deal) + **PSRO-style learning** (implementable).
  - Introduces **PR-MRE** (Probabilistically Robust Minimax-Regret Equilibrium): minimises
    worst-case regret over a **high-confidence subset** of the type space, *"providing protection
    against strategic redistribution of probability mass while avoiding the conservatism of fully
    distribution-free approaches."*
  - Formulated as a robust bilinear program with a tractable **semidefinite relaxation** for
    normal-form Bayesian games; the relaxation becomes a meta-solver inside a robust
    double-oracle framework, **PRMRE-PSRO**, with deep-RL best responses.
  - Domains: graph-structured adversarial team games (adversarial path-finding, goal search,
    reachability). Reports *"substantially improved worst-case performance across hidden types."*
    **No numeric table was extractable from the abstract page — flagged.**
  - Fish translation: the "type" is the opponent *style* (aggressive/silent/deceptive/v0.3-like),
    the "high-confidence subset" is the styles you actually built, and PR-MRE says: do not
    optimise the mean over your style population, and do not optimise the absolute worst case
    either — optimise worst-case *regret* over the styles that carry real prior mass. That is
    precisely the reporting discipline the BRIEF's standing preferences already demand, promoted
    from a reporting rule to a **training objective**.

### 4.2 Minimax-regret machinery that is cheaper to build

- Xu, Perrault, Fang, Chen & Tambe, *Robust Reinforcement Learning Under Minimax Regret for Green
  Security* (MIRROR), **UAI 2021**, arXiv:2106.08413 (15 Jun 2021) **[V]**.
  https://arxiv.org/pdf/2106.08413 ; code https://github.com/lily-x/mirror
  The practical note that matters: computing max-regret requires knowing the optimal
  per-environment value, i.e. **you must first compute the best achievable score against each
  opponent style separately**. For Fish that is cheap — one arena run per style — and it turns
  minimax regret into a post-hoc *selection* criterion over checkpoints, not only a training
  objective.
- Beukman, Coward, Matthews, Fellows, Jiang, Dennis & Foerster, *Refining Minimax Regret for
  Unsupervised Environment Design*, arXiv:2402.12284 (19 Feb 2024), **ICML 2024** **[V]**.
  https://arxiv.org/abs/2402.12284
  Documents the failure mode: once the max-regret bound is attained on all levels, **learning
  stagnates** — the objective stops distinguishing policies. Their fix is Bayesian level-perfect
  MMR (**BLP**), realised by an algorithm called **ReMiDi**; BLP policies *"act consistently with
  a Perfect Bayesian policy over all levels"* and continue learning past the regret plateau.
  Relevant warning for v0.5: a naive `min` over four opponent styles will saturate on the
  hardest style and stop improving on the other three.
- Vinitsky, Du, Parvate, Jang, Abbeel & Bayen, *Robust Reinforcement Learning using Adversarial
  Populations* (RAP), arXiv:2008.01825 (4 Aug 2020, rev. 22 Sep 2020) **[V]**.
  https://arxiv.org/abs/2008.01825 — *"a single adversary … creates exploitable policies"*;
  their fix is a randomly-initialised **population** of adversaries sampled uniformly per rollout.
  Replaces a single minimax adversary with a **population** sampled per rollout, which avoids
  overfitting to one adversary. The population version of Fish's "four opponent styles."

### 4.3 Evaluation-side worst-case, and what v0.4 already prescribed

v0.4 `evaluation.md` §2.11 and synthesis §5 Tier 4 already prescribe maxent-Nash averaging
(Balduzzi et al., NeurIPS 2018), α-Rank, ResponseGraphUCB and held-out validation opponents,
and explicitly note **Elo is not redundancy-invariant**. That is the right stack and it is
*already specified and not built*. The refresh adds only: report **minimax regret over the style
set** alongside the Nash-averaged score, since regret and raw worst-case rank checkpoints
differently whenever the styles differ in intrinsic difficulty (which they do — the mirror match
and the v0.3 match differ by 12× in dead-ask rate per `P0-v04-pathology.md`).

---

## 5. 2025–2026 work on team-zero-sum, TMECor, Team-PSRO successors, public-belief-state search

### 5.1 Verified new entries

- Anagnostides, Panageas, Sandholm & Yan, *The Computational Complexity of Team Zero-Sum Games*,
  arXiv:2606.16139 (15 Jun 2026) **[V]**. https://arxiv.org/abs/2606.16139
  Computing **Nash equilibria** in team zero-sum games is **PPAD-complete**, and remains so at
  inverse-polynomial precision, with **two-player teams**, and in polymatrix structure. Also:
  first-order stationary points in min–max optimisation are PPAD-complete even for quadratic
  multilinear objectives.
  *Correctly scoped:* this is about **TME** (no correlation), not TMECor, which Celli & Gatti
  already showed FNP-hard. Its practical message for v0.5 is negative and clarifying: the
  *uncorrelated* team-equilibrium concept — which is what three independently-acting FishBot
  seats without a shared per-episode seed actually implement — is hard in the strongest sense.
  This strengthens, by a different route, v0.4's Tier-A item 10 (shared correlation seed ω,
  "one integer", still not built).
- Liu, Wang, Wang, Zhang, Yang, Zhang, An & Wen, *Computing Ex Ante Equilibrium in Heterogeneous
  Zero-Sum Team Games* (**H-PSRO**), arXiv:2410.01575 (2 Oct 2024) **[V]**.
  https://arxiv.org/abs/2410.01575
  Identifies a concrete defect in Team-PSRO: its **policy-sharing** mechanism means the joint
  team policy space *"cannot cover the entire team policy space in heterogeneous team games where
  teammates play distinct roles,"* trapping it in sub-optimal equilibria with **higher
  exploitability**. H-PSRO adds a sequential correlation mechanism, guarantees monotonic team
  reward improvement, achieves lower exploitability than Team-PSRO, and converges on matrix
  heterogeneous games that were previously unsolvable.
  **Fish is heterogeneous in the relevant sense**: seats 0/2/4 are not interchangeable — turn
  flow, distance to the next opponent, and who receives the pass differ by seat. Any future
  Team-PSRO outer loop for Fish should be H-PSRO.
- Li, Guei, Wu & Wu, *MAPLE: Multi-State Aggregated Policy Evaluation for AlphaZero in
  Imperfect-Information Games*, arXiv:2605.24139 (22 May 2026), **IEEE CoG 2026** **[V]**.
  https://arxiv.org/abs/2605.24139
  Aggregates policy *and* value evaluations across multiple sampled world states inside an
  AlphaZero-style search. **+291 Elo on Phantom Go, +136 Elo on Dark Hex.**
  Relevant because it is the 2026 answer to the PIMC-vs-information-set-tree question that
  v0.4 §1.3 resolved by argument; MAPLE is a third option (aggregate over determinizations at
  the *evaluation* level rather than at the tree level) with large measured gains in
  public-action hidden-state games.
- Kubíček & Lisý, *Look-ahead Reasoning with a Learned Model in Imperfect Information Games*
  (**LAMIR**), arXiv:2510.05048 (6 Oct 2025) **[V]**. https://arxiv.org/abs/2510.05048
  Learns an **abstracted model** of an imperfect-information game from interaction, then does
  test-time look-ahead in it; the learned abstraction *"limits the size of each subgame to a
  manageable size, making theoretically principled look-ahead reasoning tractable even in games
  where previous methods could not scale."*
  Directly attacks v0.4 §7.1's stated blocker ("a single subgame at a mid-game PBS in Fish still
  has an astronomically large belief support ≤10²⁸"). Not a drop-in — but it is the first method
  that claims to make principled subgame look-ahead tractable at Fish's belief scale.

### 5.2 Verified-but-lower-priority

- *Considering the Difference in Utility Functions of Team Players in Adversarial Team Games*,
  arXiv:2512.18989 **[U]** — surfaced in search, not fetched. Listed for the next reader.
- *Solving equilibrium for adversarial team games utilizing fictitious team play with refined
  team plans*, Expert Systems with Applications (2025), DOI prefix S0957417425031112 **[U]** —
  paywalled, not fetched.

### 5.3 Nothing found that overturns the v0.4 architecture

No 2025–26 result contradicts: exact block-DP belief, single information-set tree, Gumbel
Sequential-Halving, declaration as optimal stopping, or the DDS-is-degenerate finding (F3).
The v0.4 synthesis stands.

---

## 6. Published work on Fish / Literature specifically

Re-run 2026-08-22 via the arXiv API:

```
http://export.arxiv.org/api/query?search_query=
  all:"Canadian Fish" OR all:"Literature card game" OR all:"half-suit"
→ opensearch:totalResults = 0
```

Web search for `"Literature" OR "Canadian Fish" card game AI ... half-suit declare` 2025–2026
returned only adjacent games (Thousand, Jass, Gin Rummy, Indian Rummy arXiv:2606.21975,
collectible card games, DTCard, PyTAG).

**The v0.4 finding stands: there is no published academic work on Literature / Canadian Fish.**
Any competent v0.5 agent remains state of the art by default, which raises the evidentiary bar,
not the cleverness bar.

---

## 7. Flagged as unverifiable — do not cite

| item | why |
|---|---|
| DeepNash/Stratego "repetitive back-and-forth moves" claim | science.org 403; arXiv PDF did not decode. Metadata verified (Science 378, 2022, arXiv:2206.15378); the *behavioural* claim is from a search snippet only. |
| Exact formula of the info-gain bonus in *Incentivizing Information Gain* (LNCS 13865) | Springer IdP redirect; DTIC mirror 403. Venue and finding verified from the publisher abstract; the mathematics is not. |
| Reference [40] of arXiv:2307.13413 (the Dynkin non-existence counterexample) | bibliography truncated in the fetched HTML. No claim here depends on it. |
| PR-MRE numeric results (arXiv:2607.09993) | abstract page gives qualitative claims only. |
| Simplex-NeuPL test domains | not stated on the abstract page. |
| arXiv:2512.18989; ESWA S0957417425031112 | not fetched. |

---

## 8. What to actually build for v0.5

Ranked by (expected gain) / (cost). "Gain" is judged against the four measured pathologies in
`research/v05/results/P0-v04-pathology.md`: 39.0% dead asks, 40.0% exact repeat asks, 34.3% of
games containing a dead run ≥ 6, 58.6% of post-horizon declarations wrong.

### R1 — Put a strictly positive, common-knowledge holding cost in `value()`. **Half a day.**

`v04.hpp:370` has no time feature and `v04.hpp:669` evaluates the wait branch with all-zero
deltas, so V(wait) ≡ V(now) is an identity (§1.3). Add one feature — `nEvents / 220`, or a
discount `γ^{Δevents}` on the wait branch — and the stopping problem becomes well-posed
(Mguni, arXiv:2407.18010: strictly positive action costs give a **unique value** and a Markovian
saddle point).

Two design constraints from §1.4 that are easy to get wrong:
- The cost must be a **deterministic function of public state only** (event count, cards
  remaining), never of the agent's private confidence — otherwise "how long they waited" becomes
  a costly Spence signal readable by three opponents (Admati & Perry, RES 1987).
- Sweep the cost magnitude and report the resulting **frontier** of (dead-run length) vs
  (declaration error rate). The two trade off directly; there is no free setting.

This is the highest ratio in the document: one feature, one coefficient, and it removes the
structural cause of the deadlock rather than capping it at event 220.

### R2 — Flip the sign of the information terms in the ask score. **1–2 days.**

`f[14] = binEnt(p)` at weight **−2.6534** (`v04.hpp:83, 323`) is a negative
value-of-information term. Replace with a signed *net* information term of the shape v0.4's own
`signalling.md` §2.4 already specifies:

$$\text{score} \mathrel{+}= \lambda\Big[\underbrace{\textstyle\sum_{j\in\mathcal T} \Delta H_j}_{\text{certificate to teammates}} - \underbrace{\textstyle\sum_{k\in\mathcal O} \Delta H_k}_{\text{leak}}\Big]$$

with `ΔH` computed off the two filters `β` and `β̃` that `belief.hpp`/`blockdp.hpp` can already
produce (v0.4 §4.8). Also cap the ownership features 3/4/5/7 by `p`: at present they pay +8.76
at `p = 0` (§2.1), which is the direct mechanism of the 16.5% own-locked asks.

Evidence it will help outside Fish: the only published info-gain-bonus-in-ISMCTS result
(LNCS 13865, §2.2) reports improvement in a hidden-information multi-action game.
Caveat: that paper's exact bonus formula is unverified.

### R3 — Randomise over near-tied asks. **Half a day. Already specified in v0.4 and never built.**

Alvim et al. (GameSec 2017 / TOPS 2022, §2.3): leakage is a **convex function of the
distribution over your actions**, not of the action played. `V04Agent::chooseAsk`
(`v04.hpp:467–495`) is a deterministic argmax, so its leakage is pinned at the deterministic
bound and **no reweighting of `f[9]`/`f[14]`/`f[19]` can lower it**. v0.4 synthesis Tier A item
12 already prescribes Smooth-UCT root mixing plus the near-noise set
`A* = {a : μ_{a*} − μ_a < min(σ_{a*}, σ_a)}` at "half a day". Build it now; R2's leak penalty is
only meaningful once the policy is stochastic.

Secondary benefit, measurable immediately: exact repeat asks are 40.0% of all asks in the mirror
(`P0`); any root mixing breaks the repetition cycle mechanically.

### R4 — Per-opponent, per-half-suit online type posterior with a data-biased shrinkage. **3–5 days.**

Replace the two global scalars `priorTheta`/`priorPhi` (`v04.hpp:61–62`) with a per-player
posterior over a small **type library** (aggressive / silent / v0.3-like / deceptive), updated
from that player's public asks and, crucially, from their *silences*.

Two literature constraints, both load-bearing:
- **Bayesian Policy Reuse** (Rosman et al., MLJ 104:99–127, 2016) is designed for exactly this
  short-episode regime; a Fish game is 100–144 events (`P0`).
- **Data-biased response** (Johanson & Bowling, AISTATS 2009) and **Restricted Nash Response**
  (Johanson, Zinkevich & Bowling, NIPS 2007) give the anti-manipulation shape: confidence must
  be **per-information-set** and must decay to the population prior where evidence is absent.
  A deliberately silent opponent then produces no evidence and gets the prior — instead of being
  misread, which is what the user exploited. Expose `p` as an explicit dial and report the
  exploitation/robustness frontier, not a single point.

Do **not** build a neural opponent model for v0.5. Q-Mixing / Simplex-NeuPL (§3.1) are the v0.6
path once the scalar version is shown to work.

### R5 — Report worst-case *and* minimax regret over the style set; add a deadlock KPI. **1–2 days.**

The BRIEF's standing preference already demands per-opponent breakdown + explicit worst case.
Add, from §4:
- **minimax regret** over the style set: `max_s [V*(s) − V_π(s)]`, where `V*(s)` is the best
  score any of your checkpoints achieves against style `s` (MIRROR's practical note: you must
  compute the per-style optimum first, which is one arena run per style).
- the saturation warning from *Refining Minimax Regret for UED* (arXiv:2402.12284): a raw `min`
  over four styles will stagnate on the hardest one. Report both `min` and regret.
- a first-class **deadlock KPI**: `longest dead run`, `% games with dead run ≥ 6`, and
  `% declarations at/after the horizon that were wrong` — all three already emitted by
  `fish pathology` (`engine/src/diag.hpp:17–32, 200–209`). Gate every v0.5 commit on them.

### R6 — Use the willingness-ladder channels the rules already permit. **2–3 days.**

Not a literature item — a rules item from the BRIEF — but it ranks here because the machinery
exists (`Rules::forcedTh`) and it is the only *sanctioned* extra coordination bandwidth in the
game. Turn transfer (`Agent::choosePassTarget`, `game.hpp`) currently has the cardless player
decide unilaterally; the rules permit soliciting a willingness bit. Same for declaration
arbitration (`Rules::declArbitration = 0`, lowest seat). A willingness ladder is
information-safe by construction: it reveals one bit that the rules have already declared
public, so its secrecy rate is not negative — it is *exempt*.

### R7 — H-PSRO rather than Team-PSRO, if and when a population loop is built. **Weeks; defer.**

Liu et al. (arXiv:2410.01575) show Team-PSRO's policy sharing under-covers the joint policy space
in **heterogeneous** team games and yields *higher* exploitability. Fish seats are heterogeneous
(turn flow is seat-dependent). If v0.4's Tier-B item 16 (FXP) or a Team-PSRO outer loop is ever
built, build the heterogeneous variant. Do not start here — R1–R5 are cheaper by an order of
magnitude and address measured defects.

### R8 — Publishable measurement: the first quantitative study of self-inflicted deadlock in a voluntary-claiming game. **Free; it is a by-product of R1.**

§1.5 established that this literature does not exist. `fish pathology` already produces the
instrument. Sweeping the R1 holding cost and reporting the (deadlock, misdeclaration) frontier
in a game where the stopping structure is *exactly* an undiscounted multi-player Dynkin game
(§1.1) is a first-of-kind empirical result, and it costs one parameter sweep on top of work
already justified.

### Explicitly *not* recommended for v0.5

- **CVaR / spectral-risk objectives.** Searched; the 2024–25 body is about transition-kernel
  robustness in single-agent safe RL, not opponent-style robustness. Minimax regret (R5) is the
  right object here, not a risk measure over returns.
- **A neural opponent model.** R4's scalar version is untested; there is no evidence yet that
  the bottleneck is model capacity rather than the model being frozen.
- **LAMIR / MAPLE / PR-MRE as implementations.** All three are the right *direction* and all
  three are weeks-to-months of work against defects that a coefficient sign fix addresses.
  Revisit after R1–R5 are measured.
- **Anything derived from the DeepNash repetition anecdote or from the unverified info-gain
  formula** (§7).

---

## 9. Bibliography (new to v0.5; deduplicated against `research/v04/lit/`)

**Stopping games, wars of attrition, strategic delay**

1. Christensen, S., Lindensjö, K., Neumann, B.A. *Markovian randomized equilibria for general
   Markovian Dynkin games in discrete time.* arXiv:2307.13413v2, 12 Aug 2025. **[V]**
   https://arxiv.org/html/2307.13413
2. Hamadène, S., Hassani, M., Morlais, M.-A. *ε-Nash Equilibria of a Multi-player Nonzero-sum
   Dynkin Game in Discrete Time.* arXiv:2201.03562 (Jan 2022); *Dynamic Games and Applications*,
   DOI 10.1007/s13235-023-00500-3. **[V]** https://arxiv.org/abs/2201.03562
3. Hendricks, K., Weiss, A., Wilson, C.A. *The War of Attrition in Continuous Time with Complete
   Information.* **International Economic Review 29(4):663–680, Nov 1988.** **[V]**
   https://users.ssc.wisc.edu/~khendricks2/publications/WarofAttritioninContinuousTime.pdf
4. Mguni, D. *Stochastic Games with Minimally Bounded Action Costs.* arXiv:2407.18010, Jul 2024.
   **[V]** https://arxiv.org/abs/2407.18010
5. Admati, A.R., Perry, M. *Strategic Delay in Bargaining.* **Review of Economic Studies
   54(3):345–364, Jul 1987.** **[V]**
   https://gsb-faculty.stanford.edu/anat-r-admati/publications/strategic-delay-in-bargaining/
6. Fudenberg, D., Koh, A. *Racing to Ruin.* arXiv:2607.27638, 30 Jul 2026. **[V]** — stopping
   game under catastrophic risk; verified but judged **not** applicable to Fish (no disaster
   term). https://arxiv.org/pdf/2607.27638

**Information, leakage, and information-seeking actions**

7. Alvim, M.S., Chatzikokolakis, K., Kawamoto, Y., Palamidessi, C. *Information Leakage Games.*
   **GameSec 2017, LNCS 10575:437–457**; journal version **ACM TOPS 25(3):20, 2022.** **[V]**
   https://arxiv.org/abs/1705.05030
8. Lervold, N., Peterson, G.L., King, D.W. *Incentivizing Information Gain in Hidden Information
   Multi-Action Games.* **Computers and Games (CG 2022), LNCS 13865**,
   DOI 10.1007/978-3-031-34017-8_6. **[V-abs; author list and venue verified, formula
   unverified]** https://link.springer.com/chapter/10.1007/978-3-031-34017-8_6
9. Serrino, J., Kleiman-Weiner, M., Parkes, D.C., Tenenbaum, J. *Finding Friend and Foe in
   Multi-Agent Games* (DeepRole). **NeurIPS 32, 2019.** **[V]**
   https://papers.nips.cc/paper/8408-finding-friend-and-foe-in-multi-agent-games

**Online opponent modelling and robustness to deception**

10. Ganzfried, S., Wang, K.A., Chiswick, M. *Opponent Modeling in Multiplayer
    Imperfect-Information Games.* arXiv:2212.06027, Dec 2022 (rev. Jul 2024). **[V]**
    https://arxiv.org/abs/2212.06027
11. Rosman, B., Hawasly, M., Ramamoorthy, S. *Bayesian Policy Reuse.* **Machine Learning
    104(1):99–127, 2016**, DOI 10.1007/s10994-016-5547-y; arXiv:1505.00284. **[V]**
    https://link.springer.com/article/10.1007/s10994-016-5547-y
12. Smith, M.O., Anthony, T., Wang, Y., Wellman, M.P. *Learning to Play against Any Mixture of
    Opponents.* arXiv:2009.14180 (2020); **Frontiers in Artificial Intelligence, 2023.** **[V]**
    https://arxiv.org/abs/2009.14180
13. Liu, S., Lanctot, M., Marris, L., Heess, N. *Simplex Neural Population Learning: Any-Mixture
    Bayes-Optimality in Symmetric Zero-sum Games.* **ICML 2022**; arXiv:2205.15879. **[V]**
    https://arxiv.org/abs/2205.15879
14. Johanson, M., Zinkevich, M., Bowling, M. *Computing Robust Counter-Strategies.*
    **NIPS 20, 2007.** **[V]** https://poker.cs.ualberta.ca/publications/NIPS07-rnash.pdf
15. Johanson, M., Bowling, M. *Data Biased Robust Counter Strategies.* **AISTATS 2009.**
    **[V]** https://johanson.ca/publications/poker/2009-aistats-dbr/2009-aistats-dbr.pdf
16. Ganzfried, S., Sandholm, T. *Safe Opponent Exploitation.* **ACM EC 2012**; **ACM TEAC, 2015.**
    **[V]** https://dl.acm.org/doi/abs/10.1145/2716322
17. Müller, A., Schneider, J., Skoulakis, S., Viano, L., Cevher, V. *Best of Both Worlds: Regret
    Minimization versus Minimax Play.* arXiv:2502.11673, Feb 2025. **[V]**
    https://arxiv.org/abs/2502.11673
18. Yang, T., Meng, Z., Hao, J., Zhang, C., Zheng, Y., Zheng, Z. *Towards Efficient Detection and
    Optimal Response against Sophisticated Opponents* (Bayes-ToMoP). **IJCAI 2019**;
    arXiv:1809.04240. **[V]** https://arxiv.org/abs/1809.04240
19. Milec, D., Kovařík, V., Lisý, V. *Adapting Beyond the Depth Limit: Counter Strategies in
    Large Imperfect Information Games.* arXiv:2501.10464, Jan 2025. **[V]**
    https://arxiv.org/abs/2501.10464
20. Zhao, Y., Zhao, J., Hu, X., Zhou, W., Li, H. *DouZero+: Improving DouDizhu AI by Opponent
    Modeling and Coach-guided Learning.* arXiv:2204.02558, Apr 2022; **IEEE CoG 2022.** **[V]**
    https://arxiv.org/abs/2204.02558

**Worst-case / minimax-regret objectives**

21. Aggarwal, N., How, J.P. *Beyond Bayesian Nash: Learning Minimax-Regret Equilibria for
    Adversarial Team Games under Asymmetric Information.* arXiv:2607.09993, 10 Jul 2026;
    **TMLR.** **[V]** https://arxiv.org/abs/2607.09993
22. Xu, L., Perrault, A., Fang, F., Chen, H., Tambe, M. *Robust Reinforcement Learning Under
    Minimax Regret for Green Security* (MIRROR). **UAI 2021**; arXiv:2106.08413. **[V]**
    https://arxiv.org/abs/2106.08413
23. Beukman, M., Coward, S., Matthews, M., Fellows, M., Jiang, M., Dennis, M., Foerster, J.
    *Refining Minimax Regret for Unsupervised Environment Design.* **ICML 2024**;
    arXiv:2402.12284. **[V]** https://arxiv.org/abs/2402.12284
24. Vinitsky, E., Du, Y., Parvate, K., Jang, K., Abbeel, P., Bayen, A. *Robust Reinforcement
    Learning using Adversarial Populations* (RAP). arXiv:2008.01825, Aug 2020. **[V]**
    https://arxiv.org/abs/2008.01825

**Team-zero-sum, TMECor, PBS search (2024–2026)**

25. Anagnostides, I., Panageas, I., Sandholm, T., Yan, J. *The Computational Complexity of Team
    Zero-Sum Games.* arXiv:2606.16139, 15 Jun 2026. **[V]** https://arxiv.org/abs/2606.16139
26. Liu, N., Wang, M., Wang, X., Zhang, W., Yang, Y., Zhang, Y., An, B., Wen, Y. *Computing Ex
    Ante Equilibrium in Heterogeneous Zero-Sum Team Games* (H-PSRO). arXiv:2410.01575, Oct 2024.
    **[V]** https://arxiv.org/abs/2410.01575
27. Li, Q.-R., Guei, H., Wu, I-C., Wu, T.-R. *MAPLE: Multi-State Aggregated Policy Evaluation for
    AlphaZero in Imperfect-Information Games.* arXiv:2605.24139, May 2026; **IEEE CoG 2026.**
    **[V]** https://arxiv.org/abs/2605.24139
28. Kubíček, O., Lisý, V. *Look-ahead Reasoning with a Learned Model in Imperfect Information
    Games* (LAMIR). arXiv:2510.05048, Oct 2025. **[V]** https://arxiv.org/abs/2510.05048
29. Perolat, J. et al. *Mastering the Game of Stratego with Model-Free Multiagent Reinforcement
    Learning.* **Science 378, 2022**; arXiv:2206.15378. **[V metadata only]**
    https://arxiv.org/abs/2206.15378

**Negative search results (2026-08-22)**

30. arXiv API, `all:"Canadian Fish" OR all:"Literature card game" OR all:"half-suit"` →
    `opensearch:totalResults = 0`. Web search for Literature/Canadian Fish card-game AI 2025–26 →
    adjacent games only. **The absence of prior art on Fish is re-confirmed.**
31. No substantive literature found on self-inflicted non-termination in game-playing agents
    (three distinct query formulations). The field's standard response is an arbitrary move cap.
