# Information Set MCTS, Particle Filters, and Search under Imperfect Information

**Literature review for the Canadian Fish (Literature) agent, v0.4**
Scope: ISMCTS family (Cowling/Powley/Whitehouse), determinization vs information-set trees, Whitehouse's thesis (ICARUS), particle-filter belief tracking inside search (POMCP, MT-ISMCTS, DSMCP), DESPOT, Smooth UCT, Online Outcome Sampling, continual re-solving / safe subgame solving, and exploitability-aware search. All numbers and equations below were read from primary sources (PDFs fetched and text-extracted); anything I could not verify is explicitly marked **UNVERIFIED**.

---

## 1. Executive summary

1. **ISMCTS = one tree over information sets, a fresh determinization per iteration.** Determinizations are *not* abandoned; they are used to restrict which branches are legal on each iteration, while all statistics accumulate in a single tree. This fixes (a) strategy fusion at your own nodes and (b) the budget-splitting waste of PIMC/determinized UCT.
2. **The single most important formula in ISMCTS is not UCB1 but the *availability count* fix.** At opponent nodes, the legal action set changes between iterations (a "subset-armed bandit"). Replacing the parent visit count `n(parent)` in UCB1 with the child's *availability* count `n'(child)` is what stops rare actions from being wildly over-explored.
3. **For Canadian Fish, SO-ISMCTS ≡ SO-ISMCTS+POM ≡ MO-ISMCTS.** Cowling et al. state explicitly that all three ISMCTS variants coincide when every move is fully observable. Fish has fully public asks, answers, transfers and declarations. So you should build **one tree over public action histories** — do not pay for MO-ISMCTS's multiple trees; they buy nothing here.
4. **Consequently the classic ISMCTS variants do *not* solve Fish's hardest problem.** In Fish, strategy fusion re-enters through the *declaration* action and through *teammate* nodes: a naive determinized search believes that any player (teammate or opponent) who is holding the cards in the sampled deal can also *name the allocation*. The value of "declare" is systematically over-estimated, and the value of *signalling* to a teammate is invisible (the teammate "already knows" in the determinization).
5. **The fix that maps directly onto Fish is Goodman's RIS-MCTS (re-determinizing IS-MCTS, Hanabi, CIG 2018 / CoG 2019):** re-determinize the *acting* player's hidden information at every non-root node, from *that* player's information set, so their decision is made without leaked knowledge. In Hanabi this raised score from 3.87–3.94 (MO-ISMCTS) to 4.28–5.21 (RIS-MCTS) with a random rollout policy, and to 17.4–18.1 with rule-restricted actions.
6. **Belief tracking should be a particle filter over deals, not a uniform information set.** Cowling/Whitehouse/Powley (CIG 2015) give a tree-statistics-based particle-filter update that needs no explicit Bayes rule: `φ(d) ← (1 − n(v)/N)·φ(d) + (n(v)/N)·c(v,d)/n(v)`. In *The Resistance* this inference reduced a non-inferring opponent's win rate by 66.4%.
7. **Uniform determinization is a real, measurable weakness — and so is *biased* determinization.** AI Factory's commercial Spades ISMCTS uses knowledge-based inference to bias the deal; Whitehouse et al. traced at least three specific in-game blunders to *bias* introduced by that sampler (a card with true probability 1/3 appeared in 87% of determinizations). Unbiased sampling fixed those but broke others. Use a *probabilistically sound* sampler (MCMC), not an ad-hoc greedy dealer.
8. **Sampling a deal consistent with the public history is a hard problem in general** (Solinas, Rebstock, Sturtevant & Buro, NeurIPS 2023: the construction problem is FNP-complete in the worst case; polynomial enumeration is possible iff the public tree is *sparse*). Fish's public tree is **dense** (~10^28 deals in the initial information set), so you must sample, not enumerate. Their **TTCG Gibbs sampler (RingSwap + Metropolis–Hastings)** is the right template: it beats uniform-proposal importance sampling and approximates the true joint range with burn-in orders of magnitude smaller than the belief-state size.
9. **ISMCTS is empirically strong but theoretically unsound.** Whitehouse's own thesis reports ISMCTS "does not converge... either oscillating between several policies or settling on a policy which does not form part of a Nash equilibrium." Lisý, Lanctot & Bowling (AAMAS 2015) show a player using ISMCTS "can suffer almost arbitrarily large loss when the opponent knows the algorithm she uses," and that ISMCTS's exploitability *increases* with more computation in Goofspiel.
10. **Smooth UCT is the cheapest available exploitability patch.** Mix the node's empirical average strategy into UCB with probability `1 − η_k`, `η_k = max(γ, η(1 + d√N_k)^{-1})`. Near-zero overhead vs UCT. In Kuhn/Leduc it approached a Nash equilibrium where UCT diverged; it beat UCT in 2- and 3-player Limit Hold'em and won three silver medals at ACPC 2014 (as SmooCT) using <200 MB RAM and one thread.
11. **OOS (Online Outcome Sampling) is the sound alternative — and it is not obviously worth it here.** It converges to equilibrium, but in head-to-head play it is beaten by ISMCTS in large domains (Goofspiel(13): OOS wins 28.3% vs UCT at 1 s/move) and in Poker at all sizes. Its value in Fish would be as an *offline exploitability auditor*, not as the online engine.
12. **Continual re-solving (DeepStack / Libratus / MCCR) is not cheap to adapt to Fish.** The resolve gadget needs a counterfactual value `CBV^{σ_2}(I_1)` per opponent information set at the subgame root. In Fish that is one value per (player, possible 9-card hand) — astronomically many. Šustr, Kovařík & Lisý (AAMAS 2019) implemented domain-independent MCCR and found it *worse than IS-MCTS on all tested games except small Liar's Dice*. Do not build the v0.4 engine on it.
13. **The Long et al. (AAAI 2010) parameters predict determinization will work reasonably well in Fish.** Fish has a *very high disambiguation factor* (every transfer is public, so information sets shrink monotonically and fast), which is exactly where PIMC-family methods gain most over random. Its near-symmetric bias (≈0.5) is the least favourable region, so expect a modest but real equilibrium gap, not a catastrophe.
14. **Enhancements that transfer: EPIC/NAST-2 and MAST; enhancement that does not: RAVE.** In Whitehouse's six-domain ICARUS study, EPIC helped in all games (99.9% ANOVA significance) and NAST with n=2 matched it without domain knowledge, whereas RAVE was *detrimental* in Hearts and as LOTR:C Dark. Fish is order-sensitive like Hearts, so RAVE/AMAF should be assumed harmful until proven otherwise.
15. **Practical budgets from the literature:** ISMCTS is 2–4× slower per iteration than determinized UCT; 10,000 iterations ≈ 1 s in 2012-era C++; the shipped AI Factory Spades player uses **2500 iterations in <0.25 s on a 2011 phone with a 140 KB tree**, and is on par with the strongest hand-written AI. Going from 1200 → 5000 iterations bought only 7–8% win rate. Iteration count is *not* where the wins are in a card game — belief quality and action pruning are.

---

## 2. Algorithms and exact formulations

### 2.0 Notation

Following Cowling, Powley & Whitehouse (2012):

- `S` — set of states; `ρ(s)` — player to act in state `s`; `A(s)` — legal actions in `s`.
- `~_i` — player `i`'s equivalence relation on states; `[s]_i` — the *information set* of `s` from `i`'s view; `S/~_i` — the set of `i`'s information sets.
- `[a]_i` — player `i`'s *observation* of action `a` (a "move from `i`'s point of view"). If all actions are fully observable then `[a]_i = a` for all `i`.
- A **determinization** of information set `I` is a state `d ∈ I` — a concrete hypothesis about the hidden information.
- `f(s, a)` — the state resulting from applying `a` in `s`; `μ(s) ∈ R^κ` — terminal reward vector for `κ` players.

Tree node bookkeeping (per node `v`):

| symbol | meaning |
|---|---|
| `c(v)` | children of `v` |
| `a(v)` | incoming action (SO-ISMCTS) or incoming move `[a]_i` (POM/MO variants) |
| `n(v)` | **visit count** |
| `n'(v)` | **availability count** |
| `r(v) ∈ R^κ` | total backpropagated reward vector |
| `c(v,d) = {u ∈ c(v) : a(u) ∈ A(d)}` | children compatible with determinization `d` |
| `u(v,d) = {a ∈ A(d) : ∄u ∈ c(v), a(u)=a}` | untried compatible actions |

### 2.1 The baseline: UCB1 and determinized UCT (PIMC)

UCB1 (Auer, Cesa-Bianchi & Fischer 2002), as used by UCT:

$$
\mathrm{UCB1}(v) \;=\; \bar{X}(v) \;+\; c\,\sqrt{\frac{\ln n(\mathrm{parent}(v))}{n(v)}}
$$

where `X̄(v) = r(v)_{ρ} / n(v)` is the mean reward for the player to act.

**Determinized UCT / PIMC**: sample `M` states `d_1..d_M` uniformly from the current information set, build an *independent* UCT tree rooted at each with `N/M` iterations, and play the action maximising the *summed root visit count* across trees.

Cowling et al.'s exploration-constant experiment: they swept `c ∈ {0.1, …}` for determinized UCT, SO-ISMCTS and MO-ISMCTS in all three of their domains and found **none of the algorithms are sensitive to `c` in these games, though performance degrades outside roughly [0.3, 1.5]**. They used **c = 0.7 everywhere**. Whitehouse's thesis independently settles on `c = 0.7` (and `k = 250` for RAVE, `τ = 1` for MAST).

**The two classic defects (Frank & Basin 1998):**

- **Strategy fusion** — the solver makes *different* decisions in different states of the same information set, which no real agent can do.
- **Non-locality** — some determinizations are vanishingly unlikely because opponents would have steered play away from them; solving them anyway pollutes the decision.

Cowling et al. add a third, MCTS-specific defect: **the budget must be split across independent trees**, so each tree is shallow. In LOTR:C this dominated: a `1 × 10000` Light player beat a `40 × 250` player by **22.9%**, and the `1 × 10000` tree reached average depth **8.6** vs **4.1** for `40 × 250`.

**Long, Sturtevant, Buro & Furtak (AAAI 2010)** identify three measurable game-tree parameters that predict when PIMC is safe:

- **Leaf correlation `lc`** — P(all sibling terminal nodes share the same payoff). Low `lc` is the *worst* case for PIMC.
- **Bias `b`** — P(the game favours a particular player). Extreme bias helps PIMC (small effect).
- **Disambiguation factor `df`** — how fast a player's information set shrinks per own move. High `df` → the game rapidly becomes perfect-information → PIMC thrives.

Measured values: Skat and Hearts both have `lc ∈ [0.8, 1.0]`, `df ≈ 0.6`, variable bias; this places PIMC at roughly **−0.1 points/game vs equilibrium** and **+0.4 over random** on their [−1,1] scale. Kuhn poker: PIMC as p1 gets −0.056 vs Nash and −0.083 vs a best response (random: −0.161 / −0.417).

### 2.2 The subset-armed bandit and the availability-count UCB — **the key ISMCTS equation**

At a node whose information set belongs to player 1 but where *another* player acts, different states in the information set have different legal action sets. The node must have a branch for every action legal in *some* state, but only a subset is available on any iteration. Cowling et al. call this a **subset-armed bandit**.

Their fix: **replace the parent's visit count with the child's availability count.** Define, over iterations `t = 1..N` with determinization `d_t`:

$$
n(v) \;=\; \sum_t \mathbb{1}\!\left[\text{$v$ was selected at iteration } t\right],
\qquad
n'(v) \;=\; \sum_t \mathbb{1}\!\left[\text{parent}(v)\text{ visited at }t \;\wedge\; a(v) \in A(d_t)\right]
$$

and select

$$
\boxed{\;
v^\star \;=\; \arg\max_{v \in c(u,d)} \left[ \frac{r(v)_{\rho(d)}}{n(v)} \;+\; c \sqrt{\frac{\ln n'(v)}{n(v)}} \right]
\;}
$$

Rationale (verbatim in spirit): *"Without this modification, rare actions... are over-explored: whenever they are available for selection their ratio of visits to parent visits is very small, resulting in a disproportionately large UCB value. If every state in the information set has a rare action, this results in the search doing almost no exploitation and almost all exploration."*

Known drawback, stated by the authors: the value of an action is averaged over all the subsets in which it appeared, so an action cannot have a subset-dependent value. They tried per-subset statistics and found it impractical (too many subsets). *"An in-depth analysis of the mathematics of subset-armed bandits is a subject for future work."*

### 2.3 SO-ISMCTS — complete pseudocode

```
function SO-ISMCTS(I0, N):
    v0 ← new node for I0;  T ← {v0}
    for t = 1 .. N:
        d ← sample uniformly from I0                # determinization for this iteration
        (v, d) ← Select(v0, d)
        if u(v, d) ≠ ∅:
            (v, d) ← Expand(v, d)
        μ ← Simulate(d)
        Backpropagate(μ, v)
    return a(v*) where v* = argmax_{u ∈ c(v0)} n(u)

function Select(v, d):
    while d is nonterminal and u(v, d) = ∅:
        v ← argmax_{w ∈ c(v,d)}  r(w)_{ρ(d)}/n(w) + c·sqrt( ln n'(w) / n(w) )   # (†)
        d ← f(d, a(v))
    return (v, d)

function Expand(v, d):
    a ← uniform choice from u(v, d)
    w ← new child of v with a(w)=a, n(w)=n'(w)=0, r(w)=0
    d ← f(d, a)
    return (w, d)

function Simulate(d):
    while d is nonterminal:
        a ← uniform choice from A(d)      # replace with a heavy playout policy in practice
        d ← f(d, a)
    return μ(d)

function Backpropagate(μ, v):
    for each node u on the path from v to the root:
        n(u) ← n(u) + 1
        r(u) ← r(u) + μ
        let d_u be the determinization in force when u was visited
        for each sibling w of u with a(w) ∈ A(d_u), including u itself:
            n'(w) ← n'(w) + 1
```

Two details that are easy to get wrong:

- The restricted tree is **never materialised**; the tree policy is restricted *as it descends*, by carrying `d` down and applying selected actions to it.
- The availability increment covers **siblings that were available but not chosen**, and includes the chosen node itself.

Cowling et al. note this is close in spirit to Silver & Veness's PO-UCT (but adversarial rather than single-agent), and to Schäfer's Skat information-set tree (whose nodes are information sets from the *acting* player's view, not one fixed root player's view).

### 2.4 SO-ISMCTS + POM (partially observable moves)

Edges now carry `[a]_1` — the *root player's observation* of the action — so actions the root player cannot distinguish share one edge. Redefine:

- `c(v,d) = {u ∈ c(v) : a(u) ∈ [A(d)]_1}`
- `u(v,d) = {[a]_1 : a ∈ A(d)}` minus moves already having children.

The extra machinery is how to advance the determinization through a *move*: for determinization `d` and move `m`, let the compatible action set be `m ∩ A(d)`. If it is a singleton, apply it. If `|m ∩ A(d)| > 1`, **choose an action from it uniformly at random**, because the tree stores nothing with which to choose better.

That uniform choice *is* the algorithm's weakness: it amounts to assuming the opponent randomises among moves the root player cannot distinguish. In a phantom game where *all* opponent actions are indistinguishable, the opponent model degenerates to uniform random.

### 2.5 MO-ISMCTS (multiple-observer)

Maintain one tree `T_i` per player `i`, with nodes = `i`'s information sets and edges = `[a]_i`. Each iteration descends **all trees simultaneously**; selection at a node uses the tree of the player about to act in the current determinization; every tree is then advanced along its own observation of the chosen action.

```
function MO-ISMCTS(I0, N):
    for each player i: v0^i ← root of T_i representing [I0]_i
    for t = 1..N:
        d ← sample from I0
        (v^1..v^κ, d) ← Select(v0^1..v0^κ, d)      # bandit in T_{ρ(d)}; descend all trees by [a]_i
        if u(v^{ρ(d)}, d) ≠ ∅:
            a ← uniform from u(v^{ρ(d)}, d)
            for each player i: ensure v^i has a child through edge [a]_i (create if needed)
            descend all trees; d ← f(d,a)
        μ ← Simulate(d)
        for each player i: Backpropagate(μ, v^i)   # visit, reward, and availability updates in every tree
    return argmax over children of v0^{1} by visit count
```

The trees are "projections" of one underlying game tree: each iteration induces one path through the game tree, which projects onto one path in each information-set tree. The comparison point is Auger's MMCTS, which also uses per-player trees but **does not use determinizations to restrict iterations**, and is run offline over very many simulations rather than online.

**Critical for Fish:** when all moves are fully observable, `[a]_i = a` for every `i`, all `κ` trees have identical shape, and every node is visited and updated on every iteration in every tree. Cowling et al. say this outright: *"all three variants of ISMCTS are equivalent for a game with fully observable moves"*, which is why they tested only SO-ISMCTS for Dou Di Zhu.

### 2.6 Simultaneous moves: EXP3

Cowling et al. use EXP3 (not UCB) at simultaneous-move nodes, following Teytaud & Flory, because the optimal policy there is typically mixed and UCB converges to a pure policy. Standard EXP3 (Auer et al. 2002), with `K` arms and `n` trials:

$$
p_t(a) \;=\; (1-\gamma)\,\frac{\exp(\eta\,\hat G_a)}{\sum_{b}\exp(\eta\,\hat G_b)} \;+\; \frac{\gamma}{K},
\qquad
\hat G_a \;=\; \sum_{t: a_t = a} \frac{r_t}{p_t(a)}
$$

$$
\gamma \;=\; \min\left\{1,\ \sqrt{\frac{K\ln K}{(e-1)\,n}}\right\},
\qquad \eta \;=\; \frac{\gamma}{K}
$$

Cowling et al. state an algebraically equivalent but more numerically stable form and take `γ`, `η` "after [Auer et al., Corollary 4.2]". *(The precise printed form of their eq. (4) did not survive PDF text extraction; the above is the standard formulation they cite. Marked as reconstructed.)*

**Chance nodes** with `k` equiprobable branches: they force stratification — the first `k` visits take all outcomes in a random permutation, the next `k` visits another permutation, etc. Implemented by treating the environment as a perfect-information agent with reward 0 everywhere, so UCB with random tie-breaking produces exactly this pattern.

### 2.7 MT-ISMCTS + particle-filter inference + self-determinization (bluffing)

Cowling, Whitehouse & Powley, *"Emergent bluffing and inference with Monte Carlo Tree Search"*, CIG 2015. This is the most Fish-relevant of the ISMCTS papers.

**Bayes baseline.** For state `s` in the current information set `I`, prior `P(s)`, opponent model `P(a|s)`:

$$
P(s \mid a) \;=\; \frac{P(a\mid s)\,P(s)}{\sum_{u \in I} P(a \mid u)\,P(u)} \tag{1}
$$

**MT-ISMCTS.** Build *several* trees per player: one per information set that player could currently be observing. A determinization dictates which combination of trees an iteration uses. Each node then corresponds to a *single* information set. Trade-off, stated by the authors: the trees learn more slowly (not every tree is updated per iteration) but the opponent model is much better — which matters precisely when you intend to *use* the opponent model for inference. Restriction: **only applicable when the number of states per information set is small.** For combinatorially large information sets they say some bucketing mechanism would be needed — "a subject for future work."

They use **UCB-Tuned** rather than UCB1 (no parameter to tune; ~3–4% win-rate benefit in *The Resistance*).

**Particle-filter inference without Bayes' rule.** Let `c(u, d)` = number of iterations that visited node `u` using determinization `d` (incremented during backpropagation). Maintain a belief distribution `φ` over determinizations, initialised to the true prior. After observing an opponent action `a` from node `u`, let `v = u.⟨a⟩_i`, `n(v)` its visit count, `N` the total iterations of the last search:

$$
\boxed{\;
\varphi(d) \;\leftarrow\; \left(1 - \frac{n(v)}{N}\right)\varphi(d) \;+\; \frac{n(v)}{N}\cdot\frac{c(v,d)}{n(v)}
\;} \tag{2}
$$

The second term is an empirical estimate of `P(d | a)`; the weight `n(v)/N` means rarely-visited branches barely move the belief, and since `n(v)/N < 1` a nonzero probability never collapses to zero by accident. **Hard constraints are applied separately**: when an observation logically rules out a determinization, set `φ(d) = 0` directly.

**Self-determinization (information hiding / bluffing).** Standard ISMCTS only samples states in the *searching player's* information set, which makes bluffing impossible — the agent implicitly assumes opponents see what it sees. Self-determinization samples states the agent *knows to be false* but that opponents may be considering. Two distributions are maintained:

- `φ` — true beliefs, used to sample real determinizations, updated from the agent's own decision tree.
- `ψ` — the agent's model of *what opponents have inferred*, initialised and updated **without knowledge of the agent's own hidden information**, so that private information does not leak into the modelled opponent policy. `ψ` is updated from a **merged tree** obtained by summing visit counts across the decision tree and all self-determinization trees — "the tree that would be built by an external observer."

Four schemes were tested:

| scheme | description |
|---|---|
| `TRUE ONLY` | no self-determinization (baseline) |
| `PURE` | sample everything from `ψ` |
| `SPLIT` | half the budget from `ψ`, then half from `φ` (opponent trees pre-seeded) |
| `TWO STEP` | as SPLIT, but after phase 1 freeze the opponent trees into a visit-proportional mixed policy and stop updating them in phase 2 (prevents leakage) |
| `BLUFF` | as SPLIT, plus a final move-selection rule (below) |

**BLUFF move selection.** Let `μ_a`, `σ_a` be the mean and standard deviation of reward for action `a` at the root of the decision tree, and `a*` the most-visited action. Define

$$
A^{*} \;=\; \{\, a : \mu_{a^{*}} - \mu_{a} < \min(\sigma_{a^{*}},\, \sigma_{a}) \,\} \tag{3}
$$

Then among `a ∈ A*`, play the one with the largest **summed** root visit count across the decision tree *and* all self-determinization trees. In words: *the bluff that is not significantly worse than the best non-bluff.*

### 2.8 RIS-MCTS — re-determinizing IS-MCTS (Goodman, Hanabi)

The problem RIS-MCTS names is precisely Fish's problem: in a cooperative/team setting, standard MO-ISMCTS lets *the root player's* private knowledge leak into the *teammate's* modelled decision, so the search believes the teammate will act on information the teammate cannot have.

**Modification.** At every node where a **non-root** player acts:

- `EnterNode()` — save that player's hidden information and **re-determinize it from that player's own information set**, even though the resulting world state may be inconsistent with the root information set.
- The action is then chosen from *that* re-sampled world.
- `ExitNode()` — restore the saved hand, remove cards now known to be incompatible, re-determinize empty slots.

**Acknowledged theoretical weakness (authors' own words, paraphrased):** re-determinization *"creates determinized game states that are incompatible with the root information set"* and *"pollutes the tree statistics"* because impossible outcomes get back-propagated; this may explain why performance plateaus at larger budgets.

**Hanabi results (4-player, 100 ms – 3 s per decision):**

| agent | score |
|---|---|
| MO-ISMCTS, random rollouts | 3.87 – 3.94 (flat in budget) |
| RIS-MCTS, random rollouts | 4.28 – 5.21 (improves with budget) |
| RIS-MCTS + rule-restricted actions | 17.43 – 18.14 |
| Van den Bergh heuristic baseline | 17.2 |
| RIS-MCTS + "playable now" convention | 20.6 (2p) / 19.8 (3p) / 19.7 (4p) / 18.5 (5p), 10 s |

Note the size of the jump from *conventions*: hand-coding one signalling convention was worth ~2.5 points on top of RIS-MCTS. Conventions did **not** emerge from search.

### 2.9 ICARUS: a formal framework for MCTS enhancements (Whitehouse thesis, ch. 8)

An **information capture and reuse strategy (ICARUS)** is a 7-tuple `(R, Θ, θ_initial, α, Ψ, ξ, ω)`:

1. `R` — a nonempty set of **records**.
2. `Θ` — the **information domain**.
3. `θ_initial : R → Θ` — initial information per record.
4. `α : M* × (R → Θ) × 2^A → (A → [0,1])` — the **policy function**, used in *both* selection and simulation.
5. `Ψ` — **capture contexts** (communicate between `ξ` and `ω`).
6. `ξ : S × M* → (R × Ψ)*` — the **capture function**: which (record, context) pairs to update after a playout.
7. `ω : Θ × Ψ × R^κ → Θ` — the **backpropagation function**.

**Baseline ICARUS** (equivalent to UCT in perfect information and MO-ISMCTS with UCB1 otherwise):

$$
R_{\text{base}} = M^{*},\qquad
\Theta_{\text{base}} = \mathbb{R}^{\kappa}\times\mathbb{N}_0\times\mathbb{N}_0,\qquad
\theta^{\text{base}}_{\text{initial}}(h) = (0,0,0)
$$

$$
\alpha_{\text{base}}(h,\theta,A_s) = \mathcal{U}\!\left[\arg\max_{a\in A_s} v\big(\theta([h\!+\!\!+a])\big)\right],
\quad
v\big((q,n,m)\big)=\begin{cases}\dfrac{q_\rho}{n}+c\sqrt{\dfrac{\ln m}{n}} & n>0,\ m>0\\[4pt] +\infty & \text{otherwise}\end{cases}
$$

$$
\omega_{\text{base}}\big((q,n,m),\psi,\mu\big)=\begin{cases}(q+\mu,\ n+1,\ m+1)&\psi=\psi_{\text{visit}}\\ (q,\ n,\ m+1)&\psi=\psi_{\text{avail}}\end{cases}
$$

Here `q` is total reward (a vector — the algorithm is max^n and handles multiplayer natively), `n` is visits and **`m` is the availability count** — the same quantity as `n'` in §2.2.

**NAST (n-gram average sampling technique)** — the enhancement I would actually port to Fish:

$$
R = M^{n}\times\{1..\kappa\},\qquad \Theta = \mathbb{R}\times\mathbb{N}_0,\qquad \theta_{\text{initial}}=(0,0)
$$

$$
\alpha(\langle a_1..a_t\rangle,\theta,A_s) = \mathcal{U}\!\left[\arg\max_{a\in A_s} v\big(\theta(\langle a_{t-n+2},..,a_t,a\rangle,\rho_t)\big)\right],
\quad
v((q,n)) = \frac{q}{n} + c_{\mathrm{NAST}}\sqrt{\frac{\ln \Sigma}{n}}
$$

with `Σ = Σ_{b ∈ A_s} θ_2(⟨a_{t-n+2},..,a_t,b⟩)`, capture `ξ(s,⟨a_1..a_t⟩) = ⟨(⟨a_i..a_{i+n-1}⟩, ρ_{i+n-1}) : i = 1..t-n+1⟩`, and `ω((q,n),ρ,μ) = (q + μ_ρ, n+1)`. `n = 1` recovers MAST; `n = 2` approximates Last-Good-Reply.

**MAST** uses a Gibbs simulation policy: `α(a) = e^{v(a)/τ} / Σ_b e^{v(b)/τ}` with `v(a) = q/n` (or 1 if unvisited); tuned `τ = 1`.

**EPIC** partitions the game into **episodes** and shares statistics between the same *position-in-episode* across different episodes; records are `E × M*`, and it is used as a *simulation* policy only, so the baseline tree policy can still be context-sensitive.

**Convergence caveat (verbatim in substance):** the ICARUS convergence theorems apply *only to perfect-information games*. For imperfect information, *"no proof equivalent to that of Kocsis and Szepesvári can be presented for ISMCTS. Indeed, in experiments which are not detailed here it was observed that ISMCTS does not converge... either oscillating between several policies or settling on a policy which does not form part of a Nash equilibrium."*

### 2.10 Smooth UCT (Heinrich & Silver, IJCAI 2015)

Self-play MCTS with one tree per player over information states, where the tree policy mixes UCB with the node's own **empirical average strategy**, resembling extensive-form fictitious play.

Mixing schedule (iteration-`k`-adapted, `η_k → γ > 0`):

$$
\boxed{\;\eta_k \;=\; \max\!\left\{\gamma,\ \eta\left(1 + d\sqrt{N_k}\right)^{-1}\right\}\;} \tag{2 in paper}
$$

where `N_k` is the total visit count of the node.

```
function SELECT(u_i):
    z ~ U[0,1]
    if z < η_t(u_i):
        return argmax_a  Q(u_i,a) + c·sqrt( log N(u_i) / N(u_i,a) )
    else:
        p(a) ← N(u_i,a) / N(u_i)   for all a ∈ A(u_i)
        return a ~ p

function UPDATE(u_i, a, r_i):
    N(u_i)   ← N(u_i) + 1
    N(u_i,a) ← N(u_i,a) + 1
    Q(u_i,a) ← Q(u_i,a) + (r_i − Q(u_i,a)) / N(u_i,a)
```

`η_k ≡ 1` recovers UCT exactly. **Overhead vs UCT is essentially zero** (the average strategy is already implicit in the visit counts).

Exploitability metric used: `δ = R_1(b_1(π_2), π_2) + R_2(π_1, b_2(π_1))`; a profile with exploitability `δ` is a `δ`-Nash equilibrium.

Empirical:

- **Kuhn poker**: γ=0.1, η=0.9, d=0.001; best `c` = 1.75 (Smooth UCT) / 2 (UCT). Smooth UCT approached Nash; **UCT diverged**. Smooth UCT strictly better after 600 simulated episodes.
- **Leduc**: γ=0.1, η=0.9, d=0.002; best `c` = 18 (Smooth) / 20 (UCT). Both UCT variants learned faster than Outcome Sampling initially; UCT then diverged; Smooth UCT kept improving. Alternating OS overtook it only after **85 M episodes at exploitability 0.036**; Parallel OS after **170 M at 0.028**.
- **2-player LHE**: 14 days, ~62 B episodes, **single thread, <200 MB RAM**, 8.1 MB greedy strategy. UCT was slightly better under 72 h; after 72 h Smooth UCT pulled ahead and widened the gap. Beat UCT head-to-head; better than UCT against 6 of the top-7 ACPC-2014 agents; still lost to 5 of the top 7.
- **3-player LHE**: 10 days, ~50 B episodes, <3.6 GB RAM, 435 MB strategy; Smooth UCT beat UCT in all but 3 match-ups.
- Pot-scaled exploration: `c = min(C, potsize + k · remaining betting potential)` with `k=0.5`, `C=24` (2p) / 36 (3p).

### 2.11 POMCP / PO-UCT + unweighted particle filter (Silver & Veness, NIPS 2010)

Tree over **histories** `h`, node `T(h) = ⟨N(h), V(h), B(h)⟩`. Selection:

$$
V^{\oplus}(ha) \;=\; V(ha) \;+\; c\sqrt{\frac{\log N(h)}{N(ha)}},\qquad a \leftarrow \arg\max_a V^{\oplus}(ha)
$$

**Unweighted particle filter belief.** `K` particles `B_t^i ∈ S`:

$$
\hat{B}(s, h_t) \;=\; \frac{1}{K}\sum_{i=1}^{K}\delta_{s\,B_t^i},\qquad \lim_{K\to\infty}\hat B(s,h_t)=B(s,h_t)
$$

Update after real action `a_t` and real observation `o_t`: draw `s ~ B̂(·, h_t)`, push through the black-box simulator `(s', o, r) ~ G(s, a_t)`, and **accept `s'` into `B_{t+1}` iff `o = o_t`** (rejection sampling). Repeat until `K` particles accumulate.

```
procedure Search(h):
    repeat
        s ~ I   if h = empty   else   s ~ B(h)
        Simulate(s, h, 0)
    until Timeout()
    return argmax_b V(hb)

procedure Simulate(s, h, depth):
    if γ^depth < ε: return 0
    if h ∉ T:
        for all a ∈ A: T(ha) ← (N_init(ha), V_init(ha), ∅)
        return Rollout(s, h, depth)
    a ← argmax_b  V(hb) + c·sqrt( log N(h) / N(hb) )
    (s', o, r) ~ G(s, a)
    R ← r + γ · Simulate(s', hao, depth+1)
    B(h) ← B(h) ∪ {s};  N(h) += 1;  N(ha) += 1
    V(ha) ← V(ha) + (R − V(ha)) / N(ha)
    return R
```

**Theorem 1.** For suitable `c`, `V(h) →_p V*(h)` for all histories prefixed by `h_t`, with bias `E[V(h) − V*(h)] = O(log N(h)/N(h))`.

**Particle deprivation and reinvigoration.** Rejection sampling starves as `t` grows. Silver & Veness add **`n/16` new particles per real step** (for `n` simulations), generated by *domain-specific local transformations* of existing particles, accepted if consistent with the last observation. In Battleship: swap two ships of different sizes, swap two small ships into a large one's slot, or move 1–4 ships to new random legal positions. In Pocman: teleport 1–2 ghosts.

Domain knowledge via **preferred actions** `A_p`: rollouts sample uniformly from `A_p`; new nodes initialised `V_init(ha)=R_hi, N_init=10` for `a ∈ A_p`, and `V_init=R_lo, N_init=0` otherwise. Exploration constant set to `c = R_hi − R_lo`.

Results (1 s per action):

| Rocksample | (7,8) 12,544 states | (11,11) 247,808 | (15,15) 7,372,800 |
|---|---|---|---|
| AEMS2 | 21.37±0.22 | – | – |
| SARSOP (≈1000 s offline) | 21.39±0.01 | 21.56±0.11 | – |
| Rollout | 9.46±0.27 | 8.70±0.29 | 7.56±0.25 |
| **POMCP** | 20.71±0.21 | 20.01±0.23 | 15.32±0.28 |

Battleship (~10^18 states): POMCP sank all ships >50 moves faster than random and >25 faster than random-among-preferred. Notably, *"the search tree only provided a small benefit over the PO-rollout algorithm, due to small differences between the value of actions but high variance in the returns"* — a warning about noisy card-game rollouts. Pocman (~10^56 states): 300+ undiscounted return with preferred actions, 260 without (rollouts: 230 / 130).

### 2.12 DESPOT / R-DESPOT (Ye, Somani, Hsu & Lee, JAIR 58, 2017)

A **scenario** for belief `b` is `φ = (s_0, φ_1, φ_2, …)` with `s_0 ~ b` and `φ_i ~ U[0,1]` i.i.d.; a deterministic simulative model `g : S × A × R → S × Z` satisfies `(s',z') = g(s,a,φ) ~ p(s',z'|s,a) = T(s,a,s')O(s',a,z')`. A DESPOT is the subtree of the belief tree traced out by all action sequences under `K` sampled scenarios: `O(|A|^D K)` nodes instead of `O(|A|^D |Z|^D)`.

Empirical value of a policy under the scenarios at node `b`:

$$
\hat V_\pi(b) \;=\; \frac{1}{|\Phi_b|}\sum_{\varphi \in \Phi_b} V_{\pi,\varphi}
$$

**Theorem 3.1 (generalisation bound).** For any `τ, α ∈ (0,1)`, any `b_0`, and any `D, K`, every DESPOT policy tree `π ∈ Π_{b_0,D,K}` satisfies, w.p. ≥ `1 − τ`:

$$
V_\pi(b_0) \;\ge\; \frac{1-\alpha}{1+\alpha}\hat V_\pi(b_0) \;-\; \frac{R_{\max}}{(1+\alpha)(1-\gamma)}\cdot\frac{\ln(4/\tau) + |\pi|\ln\!\big(K D |A||Z|\big)}{\alpha K}
$$

**Theorem 3.2** then says maximising the RHS gives a policy competitive with the best *small* policy; choosing `K = O(|π| ln(|π||A||Z|))` suffices. This motivates the regularized objective

$$
\max_{\pi \in \Pi_D}\Big\{\hat V_\pi(b_0) - \lambda|\pi|\Big\}
$$

solved by dynamic programming on the **regularized weighted discounted utility**

$$
\nu_\pi(b) \;=\; \frac{|\Phi_b|}{K}\,\gamma^{\Delta(b)}\,\hat V_{\pi_b}(b) \;-\; \lambda|\pi_b|,
\qquad
\nu^{*}(b) = \frac{|\Phi_b|}{K}\gamma^{\Delta(b)}\hat V_{\pi_0}(b) \ \ \text{at leaves}
$$

**Anytime heuristic search.** With bounds `ℓ(b) ≤ ν*(b) ≤ μ(b)` and gap `ε(b) = μ(b) − ℓ(b)`, each exploration targets shrinking `ε(b_0)` to `ξ·ε(b_0)`:

$$
a^{*} = \arg\max_{a\in A}\Big\{\rho(b,a) + \sum_{z\in Z_{b,a}}\mu(\tau(b,a,z))\Big\},
\qquad
z^{*} = \arg\max_{z\in Z_{b,a^{*}}}\Big\{\underbrace{\epsilon(b') - \tfrac{|\Phi_{b'}|}{K}\,\xi\,\epsilon(b_0)}_{\text{excess uncertainty } E(b')}\Big\}
$$

Belief updates use **sequential importance resampling (SIR)**. Typical `K = 500` (100 for Pocman, to stay inside 1 s). DESPOT beat POMCP substantially on Laser Tag (large observation space, ~1.5×10^6 observations) and on Rock Sample; SARSOP was best on the small domains; on Pocman DESPOT was slightly ahead of POMCP.

### 2.13 Online Outcome Sampling (Lisý, Lanctot & Bowling, AAMAS 2015)

OOS = MCCFR outcome sampling with **incremental tree building** plus **targeting** of the current match position, with importance weights to keep updates unbiased. It is the *first* online algorithm for imperfect-information games guaranteed to converge to an equilibrium strategy.

Two targeting schemes:

- **IST (information-set targeting)** — bias samples to `Z_{I(m)}`, terminals passing through the current information set.
- **PST (public-subgame targeting)** — bias to `Z_{p,I(m)} = {(h',z) : p(h') = p(h)}`, i.e. all terminals whose public action prefix matches. PST avoids "revealing" private information through the targeting itself.

Per iteration, with probability `δ` target the subgame; with probability `1−δ` sample normally. `δ = 0` reduces to MCCFR with incremental tree building.

Sampling distribution:

$$
\Phi(I,i) = \begin{cases}\epsilon\cdot\mathcal U(A(I)) + (1-\epsilon)\,\sigma_i(I) & P(I)=i\\ \sigma_i(I) & \text{otherwise}\end{cases}
$$

Regret and average-strategy updates (`W = u\,\pi_{-i}/l`, `c` = tail reach before multiplying in `σ(I,a)`, `x` = tail reach after):

$$
r_I[a'] \mathrel{+}= \begin{cases}(c - x)\,W & a' = a\\ -\,x\,W & a' \ne a\end{cases}
\qquad\quad
s_I[a'] \mathrel{+}= \frac{\pi_{-i}\,\sigma(I,a')}{\delta s_1 + (1-\delta)s_2}
$$

**Weighting factor `w_T`** (this is the subtle part — without it, the pre-match samples get drowned out by the newly-targeted ones):

$$
\frac{1}{w_T(m)} \;=\; (1-\delta) \;+\; \delta\,\frac{\sum_{(h,z)\in I(m)}\bar\pi(h)}{\sum_{z\in Z_{\text{sub}}(m)}\bar\pi(z)} \tag{3}
$$

Initial calls are `OOS(∅, 1, 1, 1/w_T, 1/w_T, i)`, alternating update player `i`.

**Explorative regret matching** to escape zero-reach subtrees:

$$
\sigma^{T+1}_{\gamma}(I,a) \;=\; \frac{\gamma}{|A(I)|} + (1-\gamma)\,\sigma^{T+1}(I,a),\qquad \gamma = 0.01
$$

Tuned settings in their experiments: `δ = 0.9`, `ε = 0.4` (ε=0.8 in large Liar's Dice); ISMCTS baselines used `C = 2 × max payoff` for UCT and exploration 0.2 for Regret Matching selection.

**Results that matter for us:**

- **Exploitability**: in Goofspiel, *both* ISMCTS variants get **more exploitable with more computation time**. In Liar's Dice ISMCTS plateaus. In Poker, ISMCTS is *less* exploitable than OOS at short time controls.
- **Head-to-head**: II-Goofspiel(6), 0.1 s: OOS ties both ISMCTS variants (~50%) while UCT/RM beat each other 62–73%; Goofspiel(13), 1 s: OOS wins only **28.3%** vs UCT and **35.1%** vs RM. Liar's Dice(2,2), 5 s: roughly even. Generic Poker: OOS loses from position 1 at every size.
- Conclusion the authors draw: OOS wins on **worst-case guarantees**, ISMCTS wins on **raw strength per second in large games**.

### 2.14 Continual re-solving and safe subgame solving

Definitions (Brown & Sandholm, NIPS 2017):

$$
v^{\sigma}_i(I_i) = \frac{\sum_{h\in I_i}\pi^{\sigma}_{-i}(h)\,v^{\sigma}_i(h)}{\sum_{h\in I_i}\pi^{\sigma}_{-i}(h)},
\qquad
v^{\sigma}_i(I_i,a) = \frac{\sum_{h\in I_i}\pi^{\sigma}_{-i}(h)\,v^{\sigma}_i(h\cdot a)}{\sum_{h\in I_i}\pi^{\sigma}_{-i}(h)}
$$

`CBR(σ_{−i})` is a best response that also maximises value in unreached information sets; `CBV^{σ_{−i}}(I_i) = v_i^{⟨CBR(σ_{−i}),σ_{−i}⟩}(I_i)`.

**Unsafe subgame solving**: reach `h ∈ S_top` with probability `π^σ(h) / Σ_{h'∈S_top} π^σ(h')` and re-solve. No guarantees; catastrophic in the Coin Toss example; but empirically often good (65.59 mbb/h in Large NLFH at 200 buckets vs Resolve's 179.6) *and occasionally catastrophic* (396.8 at 30,000 buckets, worse than the trunk).

**Resolve gadget**: insert a `P1` node `h_r` before each `h_top ∈ S_top`, reached in proportion to `π^σ_{−1}(h_top)`. At `h_r`, `P1` chooses `a'_S → h_top` or `a'_T →` terminal payoff `CBV^{σ_2}(I_1(h_top))`. Solving this augmented subgame guarantees `P2`'s exploitability is **no worse than the blueprint's**.

**Maxmargin** maximises the minimum margin; **Reach** subgame solving increases the alternative payoff by "gifts" `⌊g^{σ−S}⌋` the opponent gave up en route, producing margins ≥ Maxmargin's.

**Nested subgame solving** (the "inexpensive method"): when the opponent plays an off-tree action `a`, generate subgame `S` such that `I_1·a ∈ S_top` for every `I_1`, solve it, and splice the solution into the blueprint. DeepStack's *continual re-solving* is the independently-developed twin of this.

Exploitability (mbb/h, measured with no information abstraction):

| Turn Hold'em, buckets: | 200 | 2,000 | 20,000 |
|---|---|---|---|
| Trunk strategy | 684.6 | 465.1 | 345.5 |
| Unsafe | 130.4 | 85.95 | 79.34 |
| Resolve | 454.9 | 321.5 | 251.8 |
| Maxmargin | 427.6 | 299.6 | 234.4 |
| Reach-Maxmargin | 424.4 | 298.3 | 233.5 |
| Reach-Estimate + Distributional (not split) | **113.3** | **83.24** | **70.68** |

**MCCR** (Šustr, Kovařík & Lisý, AAMAS 2019) generalises CR to any 2p zero-sum EFG using MCCFR as the resolver, with an extended gadget that handles non-poker-like public trees. Exploitability bound `O(T^{-1/2})`:

> With probability ≥ `(1−p)^{N+1}`, the exploitability of MCCR's strategy is bounded by a sum of terms in `1/√T_0` and `1/√T_R` (pre-play and resolving iterations respectively).

**Their honest empirical bottom line:** *"MCCR is worse than IS-MCTS on all games with the exception of small Liar's Dice."* Also: CFV estimates stabilise long before exploitability drops in small domains, but in large domains (Goofspiel, Phantom TTT) the CFV error decreases only slowly — and CFV quality is exactly what CR depends on. They found **no noteworthy influence** of the MCCFR exploration rate `ε ∈ {0.2,…,0.8}`. Keeping (rather than resetting) regrets between resolves was better in practice but is not backed by theory (needs warm-starting).

**Soundness framing (Šustr, Schmid, Moravčík, Burch, Lanctot & Bowling, 2020):** an online algorithm `Ω` is `(k,ε)`-sound iff for all `k' ≥ k` and all opponents `Ω_2`, `E[R] ≥ E[R under a fixed ε-Nash σ]`. They define a three-level consistency hierarchy and show that **OOS provides only the weakest level (local consistency)**, which gives no bound at all — locally consistent algorithms "can be highly exploited by an [adversary]." So even OOS's guarantee is weaker than commonly assumed.

### 2.15 DSMCP — deep synoptic Monte Carlo planning (Clark, NeurIPS 2021; program *Penumbra*, winner of the official 2020 RBC competition)

Relevant because it is the strongest published example of *particle filter + neural nets + Smooth-UCB-style bandit* in a large hidden-information game, and because it solves the leakage problem with a "second-order" belief representation.

**Bandit (Smooth-UCB relative):**

```
function Bandit(π, n⃗, q⃗):          # c > 0, m ≥ 0
    n ← Σ_a n⃗_a
    if e^{−mn} > U[0,1] and not root:
        return a ~ π                                    # mix in a learned policy
    else:
        return argmax_a ( q⃗_a/n⃗_a + c·π_a·sqrt(ln n / n⃗_a) )   # PUCT-style
```

`m = 0` ⇒ always follow `π`; `m = ∞` ⇒ never mix. Unlike Smooth UCB (which mixes in the *empirical average* policy), DSMCP mixes in a *neural network* policy.

**Second-order belief.** `B̂ ⊂ L`, where each element `L ∈ B̂` is itself a *set* of possible world states **from the opponent's perspective**. This lets the planner reason about the opponent's uncertainty rather than about a single world. Particles are sampled with rejection (`DrawSample`), guided by a learned opponent policy `τ̂`; if `k` consecutive candidates are rejected as inconsistent, fall back to a singleton drawn from the known-possible set `X` — the deprivation guard.

**Belief maintenance (`PlayGame`)** tracks all possible world states `X_t` forward, then **retrospectively filters** earlier particle sets: for `i = t−1 … 0`, drop states from `X_i` with no legal successor in `X_{i+1}`, and drop belief particles `I ∈ B̂_i` with `I ∩ X_i = ∅`. Then repopulate to `n_particles`.

A **synopsis function** `σ` produces a fixed-size (<1 kB) summary of a node for the network — the abstraction that makes learning over information states tractable. `ℓ` bounds the size of an approximate information state; `n_vl` is a virtual-loss weight; `z` is a threshold for deepening the search.

### 2.16 History filtering: complexity, and the trick-taking Gibbs sampler (Solinas, Rebstock, Sturtevant & Buro, NeurIPS 2023)

This paper is the missing formal piece for Fish's belief module.

- **Theorem 1.** There is a joint policy `π` for which the *construction* problem (produce **one** history consistent with a public state) is **FNP-complete**. Their 3-FSAT-GAME reduction: the hidden state is a truth assignment and each public observation is a 3-CNF clause it satisfies.
- **Definition (sparsity).** A public tree is *sparse* iff every public state `S` satisfies `|H_S| ≤ p(t)` for some polynomial `p`; otherwise *dense*.
- **Theorem 2.** Enumeration is polynomial **iff** the public tree is sparse. Heads-up Hold'em is sparse (constant-size public states). "With `n` cards in the deck and `k` cards dealt to each player, the number of histories is exponential in `k`, so the public tree is dense." **Fish has `k = 9` and 6 players — emphatically dense.**
- Even sparse is not sufficient in practice: they note memory/time still prevents enumeration in Skat and Hearts.

**TTCG Gibbs sampler.** Encode the unknown-card assignment as a **suit-length assignment matrix** `A` with row sums = each player's number of unknown cards and column sums = unknown cards per suit; voids are entries pinned to zero. `RingSwap(S,σ)`: for each player `i` and each pair of non-void suits `j,k`, add one to `A_{i,j}` and subtract one from `A_{i,k}`, then repair the column sums by BFS over sequences of compensating swaps of length `< n`, selecting a valid assignment **proportional to the number of histories it corresponds to**. Then Metropolis–Hastings:

1. Compute `Ω_σ = RingSwap(S, σ)`
2. `σ' ~ Uniform(Ω_σ)`
3. Compute `Ω_{σ'}` and `h'` by substituting `σ'` for `σ` in `h`
4. $z = \min\!\left\{1,\ \dfrac{\bar P_\pi(h')\,|\Omega_\sigma|}{\bar P_\pi(h)\,|\Omega_{\sigma'}|}\right\}$
5. Accept `h'` with probability `z`, else stay.

**Theorem 3.** The chain is aperiodic and irreducible. **Theorem 4.** Its stationary distribution is `P_π(·|S)` — the true joint range. All steps are polynomial in history length; no explicit belief representation is ever built.

Empirically (Oh Hell, public belief states of 192 / 12,960 / 544,320 histories): the Gibbs sampler **beats uniform-proposal importance sampling** and closely approximates exact joint-range sampling, with **burn-in orders of magnitude smaller than the belief-state size**. They also found that *burning fewer samples can give a better value estimate for the same number of chain transitions*, because sample count matters more than perfect mixing when evaluation is the expensive step.

---

## 3. Consolidated empirical results

**ISMCTS vs determinized UCT vs cheating (Cowling et al. 2012, 10,000 iterations/decision):**

| Domain | Ordering (best → worst) | Notes |
|---|---|---|
| LOTR:C | cheating UCT ≫ {cheating ensemble UCT, MO/SO-ISMCTS} > determinized UCT | ISMCTS beats det-UCT by ~4.4% (Light) and by a large margin (Dark). No significant difference among the three ISMCTS variants. |
| Phantom (4,4,4) | cheating ens. UCT > cheating UCT > **MO-ISMCTS** > det-UCT > SO-ISMCTS > SO-ISMCTS+POM | All orderings significant at 95%. Here the SO variants are *worse* than plain determinization. |
| Dou Di Zhu (5,000 deals × 75 plays) | cheating UCT 56.5% > det-UCT 43.6% ≈ ISMCTS 42.3% | ISMCTS wins on deals where cheating helps (category `C>D`, 1421 deals); det-UCT wins on the 3562 deals where hidden information barely matters. |

**Time-controlled versions:** SO/MO-ISMCTS run **2–4× fewer iterations per second** than determinized UCT. LOTR:C: MO-ISMCTS is slightly *inferior* at 1 s, significantly *stronger* from 3 s, gap widening to 30 s. Phantom (4,4,4): above 1.5 s/move both SO- and MO-ISMCTS become *relatively weaker* than det-UCT — because the pessimistic assumption that the opponent knows the state makes them conclude the game is lost and play randomly.

**Absolute strength:** MO-ISMCTS vs 7 human LOTR:C players (2 expert, 5 intermediate): 14/32 as Light, 16/32 as Dark — evenly matched. Dou Di Zhu: both det-UCT (40×250) and ISMCTS (10,000) significantly beat AI Factory's commercial flat-Monte-Carlo+heuristics agent; giving the commercial agent 100× its iterations equalises them.

**Commercial Spades (Whitehouse, Powley & Cowling, AIIDE 2011 / AAAI):** ISMCTS with **2500 iterations, <0.25 s on a Samsung Galaxy S II, 2500 × 56 B = 140 KB** of tree. Significantly stronger than the default knowledge AI at all iteration counts; on par with the "hardest" (6-star-equivalent) knowledge AI from 2600 iterations. 1200 → 5000 iterations = **+7–8% win rate**. Progressive-bias-style knowledge injection (skew ∈ [½, 3/2] multiplying backpropagated reward, decayed by `t/(k+t)`) had **no statistically significant effect on strength** but a large effect on perceived plausibility. Playouts were truncated at the end of the round with heuristic evaluation

$$
\frac{(s_p - 10 b_p) - (s_o - 10 b_o)}{c}
$$

(`s` = points, `b` = bags, `c` normalising so values lie roughly in `[−½, ½ ]`, giving unit range so standard UCB constants apply).

**Resistance / MT-ISMCTS (20,000–40,000 iterations, 1000 games):** particle-filter inference cut the `TRUE ONLY` spy team's win rate by **66.4%**. `PURE`/`SPLIT`/`TWO STEP` were each ~2× stronger than no self-determinization (no significant difference between them). `BLUFF` gave **+31.7%** over the non-self-determinizing spy, recovering roughly half the advantage inference gave the non-spies. Optimal split: devoting **3/8 to 7/8** of the budget to self-determinizations all worked; ~5000 true determinizations sufficed. At very large budgets `SPLIT` overtook `BLUFF` — i.e. **bluffing emerges from self-determinization alone if you can afford ~10^6 iterations** (≈10 s/decision, ~1 GB of trees in single-threaded C++ on a 2.53 GHz Xeon).

---

## 4. Applicability to Canadian Fish — technique by technique

### 4.0 Structural facts about Fish that drive every choice

| property | value | consequence |
|---|---|---|
| Deals | `54!/(9!)^6 ≈ 10^{38}` | matches the stated ~10^40 state space |
| Initial information set size (one player) | `45!/(9!)^5 ≈ 2×10^{28}` | **cannot enumerate**; particle filter mandatory |
| Move observability | **all moves fully observable** | `SO-ISMCTS ≡ SO-ISMCTS+POM ≡ MO-ISMCTS`; build **one** tree over public histories |
| Public tree | dense (Solinas Def. 3) | no explicit range/PBS representation; MCMC sampling required |
| Disambiguation factor | **very high** — every transfer, answer and declaration permanently localises cards | favours determinization-family methods (Long et al.) |
| Bias | ≈ 0.5 (balanced teams) | least favourable region for PIMC; expect a real but modest equilibrium gap |
| Own-node legality | depends **only on your own hand** (hold a card of the half-suit, not the asked card) | **no subset-armed bandit at your own nodes** — legal move set is identical across all determinizations |
| Opponent-node legality | depends on their hidden hand | subset-armed bandit applies; **availability-count UCB is required** at opponent/teammate nodes |
| Branching (asks) | `Σ_{H: k_H ≥ 1}(6 − k_H) × 3` opponents; ≈ 9 (concentrated hand) to 135 (one card in each of 9 half-suits); typically 60–90 raw, 20–50 after pruning publicly-located cards | manageable; prune aggressively |
| Branching (declare) | ≤ 9 half-suits **if** you collapse the allocation into a single deterministic function of the information set; `3^6 = 729` per half-suit if you enumerate | **must collapse** — see §4.3 |
| Horizon | ~40–100 asks per game, ~10–20 decisions per player | depth-limit + evaluation function, do not roll out to the end |

### 4.1 SO-ISMCTS with availability-count UCB — **adopt, it is the backbone**

**Would it help?** Yes, and it is essentially free relative to PIMC. The three Cowling defects all bite in Fish: strategy fusion (worst at declarations), non-locality (opponents steer play away from deals in which they are weak), and budget splitting.

**Cost.** 2–4× per-iteration overhead vs determinized UCT in Cowling's implementation, but Fish states are tiny (6 hands ≤ 9 cards + a public log), so cloning is cheap. Budget 10^4–10^5 iterations per decision at 0.1–1 s in a lean implementation.

**Adaptation for a 6-player, 2-team game.** Use **reward vectors** and max^n backup, as the ICARUS baseline does. Because teammates share a reward, back up the *team* score to all three team members; then selection at a node uses `r(v)_{team(ρ(d))}/n(v)`. Do **not** treat the game as 2-player zero-sum between teams for *selection purposes* — each seat has its own information and its own action set.

**Pitfalls specific to Fish.**
- The availability-count fix must be applied at *teammate* nodes too, not just opponent nodes — a teammate's ask legality is hidden from you as well.
- Fish has no chance nodes after the deal, so the chance-node stratification machinery is unnecessary. The deal is a chance node *before* any decision, which is exactly why it never appears in the tree and is handled by determinization instead.
- There are no simultaneous moves in Fish (declarations are asynchronous but can be serialised), so EXP3 is not needed structurally — but see §4.7 for why you may want a stochastic root policy anyway.

### 4.2 SO-ISMCTS+POM and MO-ISMCTS — **skip both**

They collapse to SO-ISMCTS in a fully-observable-move game. MO-ISMCTS would cost `κ = 6` trees for identical statistics. **Do not build them.** The thing you actually want from MO-ISMCTS — modelling the other players' *own* uncertainty — comes from re-determinization (§4.4), not from multiple trees.

### 4.3 The declaration problem — **the single biggest design risk**

A declaration is correct only if the declarer can *name* the card→teammate allocation. A determinization *tells the search the answer*. Therefore:

- **Naive ISMCTS massively over-values declaring.** Every determinization in which the team holds all six cards makes "declare" a guaranteed +1.
- **It also under-values signalling to zero.** In the determinization, your teammate already "holds" a known hand, so informing them is worth nothing.

Three layered fixes, in increasing cost:

**(a) Collapse the declare action into an information-set-level move (SO-ISMCTS+POM style).** The tree edge is `declare(H)`; the *allocation* is computed by a deterministic function of the declarer's information set (the MAP allocation under the declarer's belief). Never let the determinization choose the allocation.

**(b) Evaluate `declare(H)` against the particle set, not against the determinization.** With particle pool `P` and weights `w_p`, let `α*` be the allocation maximising particle-weighted agreement:

$$
\alpha^{*} \;=\; \arg\max_{\alpha}\ \sum_{p\in P} w_p\,\mathbb{1}[\alpha \text{ correct in } p],
\qquad
\hat q \;=\; \sum_{p\in P} w_p\,\mathbb{1}[\alpha^{*}\text{ correct in }p]
$$

and give `declare(H)` an immediate reward of `q̂ · (+1) + (1 − q̂) · (−1)` for that half-suit *before* continuing the simulation, with the set removed either way. This makes the search see the real risk. Because a Fish declaration must be *exactly* right, `q̂` is a product over the six cards of the per-card allocation confidence when the cards are conditionally independent under the belief, which they are not — so compute it by counting particles, not by multiplying marginals.

**(c) Use RIS-MCTS at teammate/opponent declare nodes** (§4.4) so that *their* declarations are evaluated from *their* belief. Practically: maintain a lightweight per-seat belief summary (which cards each seat has publicly seen) and let a simulated seat declare only if that seat's own public+private knowledge determines the allocation.

**Compute cost.** (a) is free. (b) costs one pass over the particle pool per `declare` evaluation; with `|P| = 10^3` and caching per (half-suit, public state) this is negligible relative to a playout. (c) is the expensive one — see below.

### 4.4 RIS-MCTS re-determinization — **adopt, this is where the signalling value comes from**

**Would it help?** This is the technique that makes signalling and information-gathering *visible to the search*. In Hanabi it converted a flat, budget-insensitive agent (3.87–3.94) into one that improved with thinking time (4.28–5.21), and it is the reason the agent could exploit conventions. Fish's core team dynamic — "my ask tells my partner I hold something in this half-suit, so they can declare later" — is structurally identical.

**Adaptation.** At every non-root seat's node:
1. Save that seat's hand.
2. Re-sample it from *that seat's* information set: the seat's own knowledge = public history + (in a hierarchical belief) whatever that seat is modelled as knowing. A cheap approximation that works in Fish: re-sample the seat's hand from the **public-only** belief (i.e. the belief an outside observer would hold), which is exactly the `ψ` distribution of §2.7.
3. Choose the action from that world.
4. Restore, remove now-impossible cards, re-fill.

**Cost.** Naively this is a belief-sample per node visit — far too expensive. Two mitigations, both used in the literature:
- **Lazy/cached re-determinization**: re-determinize only at nodes where the acting seat's decision plausibly depends on hidden information — in Fish, that is (i) declare nodes and (ii) the choice of *which* card to ask, but not (iii) whether an answer is yes/no (that is forced by the world).
- **Pooled sampling**: keep a persistent pool of public-belief deals (`ψ`-particles) and draw from it in O(1), refreshing it between real decisions.

**Pitfall.** The authors' own warning applies verbatim: re-determinization produces world states inconsistent with the root information set, and back-propagating those outcomes pollutes the statistics; expect a performance plateau at large budgets. Consider gating it — e.g. re-determinize with probability `β`, tune `β ∈ [0.25, 1]` empirically, analogously to the `SPLIT` fraction in §2.7 (where 3/8–7/8 all worked).

### 4.5 Belief representation and the particle filter — **adopt; build it before the search**

**Representation.** A particle is a full deal of the currently-unseen cards to the seats, respecting:
- **Capacity constraints**: seat `i` holds exactly `m_i` unknown cards (hand sizes are public).
- **Negative constraints** (forbidden edges): seat `p` does not hold card `c` — from a "no" answer at time `t`, plus "the asker did not hold `c`", plus every publicly located card.
- **Positive disjunctive constraints**: at the time seat `A` asked for a card of half-suit `H`, `A` held at least one card of `H`. These are *not* representable as forbidden edges and must be checked by rejection or included in the MH acceptance test.

**Sampler.** Do not use a greedy sequential dealer — that is precisely what produced AI Factory's blunders (a card with true probability 1/3 appearing in 87% of determinizations). Instead:

1. Get one feasible deal via constraint propagation + bipartite max-flow feasibility (Hall's condition on the forbidden-edge bipartite graph).
2. Run the **Solinas et al. Metropolis–Hastings chain**: propose a swap of cards between two seats (the Fish analogue of `RingSwap` on the card×seat incidence matrix), accept with

$$
z = \min\left\{1,\ \frac{\bar P_\pi(h')\,|\Omega_\sigma|}{\bar P_\pi(h)\,|\Omega_{\sigma'}|}\right\}
$$

where `P̄_π(h)` is the **unnormalised reach probability** of history `h` under your current policy model `π` — this is what folds *soft* inference (which asks a player would plausibly have made) into the sampler on top of the hard constraints.

3. Maintain a **pool of ~10^3–10^4 particles** persistently across the game; between real decisions, filter out particles inconsistent with the new observation and top up with fresh chain samples. This is POMCP's `B(h)` idea plus DSMCP's retrospective filtering (`for i = t−1 … 0: drop particles with `I ∩ X_i = ∅`').

**Deprivation guard.** Both POMCP (`n/16` reinvigorated particles per step) and DSMCP (`k` consecutive rejections → fall back to a singleton from the known-possible set) show you need one. In Fish the natural reinvigoration is a *targeted* MH restart: keep the publicly-forced assignments, re-randomise the free cards.

**Soft inference from actions.** Two options, both cheap:
- **Bayes with an explicit ask-model**: `P(s|a) ∝ P(a|s)P(s)` (eq. 1). Fish's ask model is unusually tractable because ask legality reveals a lot: asking `(p, c)` implies the asker held some card of `HS(c)` and not `c`, which is a *hard* constraint; the *choice* among legal asks is the soft signal.
- **Tree-statistics update (eq. 2)**: `φ(d) ← (1 − n(v)/N)φ(d) + (n(v)/N)·c(v,d)/n(v)`, using the previous search's own tree as the opponent model. This requires storing `c(v,d)` per (node, particle), which is `O(|tree| × |P|)` in the worst case — in practice store it only for the root's children (which is all eq. 2 uses).

**Cost estimate.** MH transitions are cheap (a swap + a feasibility test + a reach-probability ratio). Solinas et al. found burn-in "orders of magnitude smaller than the belief-state size", and that *fewer* burned samples can be better per unit compute. Budget maybe 10–50 µs per particle refresh; refreshing 10^3 particles between decisions ≈ 10–50 ms. Acceptable.

### 4.6 Playouts, evaluation and enhancements

**Truncate playouts.** Copy the Spades design: do not play to the end of the game. Terminate at a horizon (e.g. after the current half-suit resolves, or after `k` asks) and back up a normalised heuristic, e.g.

$$
\hat{\mu} \;=\; \frac{\big(\text{our declared sets} + \sum_H \hat q_H^{\text{ours}}\big) - \big(\text{their declared sets} + \sum_H \hat q_H^{\text{theirs}}\big)}{c}
$$

with `c` chosen so values lie in roughly `[−½, ½]` (unit range) so that the standard `c = 0.7` UCB constant transfers. Cowling/Whitehouse used exactly this normalisation trick.

**Enhancements to try, ranked by expected value:**

1. **NAST with n = 2** — records are (previous move, this move, player). Fish is full of "good replies": if an opponent just asked for a card of half-suit `H`, asking them back in `H` is often right. NAST-2 approximated LGR and matched EPIC across six domains with no domain knowledge. Cheap: one hash table.
2. **MAST (NAST n=1)** with Gibbs `τ = 1` — robust everywhere, never significantly harmful in Whitehouse's study.
3. **EPIC** with episodes = "one player's uninterrupted run of successful asks" (Fish is naturally episodic: a turn is a maximal run of yes-answers). EPIC helped in *all six* of Whitehouse's domains at 99.9% ANOVA significance.
4. **Progressive-bias knowledge injection**, decayed by `t/(k+t)`, for plausibility — but note it had *no significant strength effect* in Spades. Use it only if the bot must look sensible to humans.
5. **RAVE/AMAF — do not use.** It was *detrimental* in Hearts and as LOTR:C Dark precisely because move ordering matters. Fish is intensely order-sensitive (asking in the wrong order leaks information and burns tempo).

### 4.7 Exploitability, signalling leakage and countermeasures

**The threat model matters.** Against humans who do not model your algorithm, ISMCTS's unsoundness is mostly harmless. Against a repeated opponent (or a self-play evaluation) it is not: Lisý et al. showed ISMCTS's exploitability *grows* with search time in Goofspiel, and Šustr et al. showed that even OOS's guarantee (local consistency) gives no bound against an adversary.

**Concrete Fish leak:** a deterministic ISMCTS root policy means your ask *fully determines* a set of hands you might hold. An opponent who knows your policy can invert it.

**Cheap patches, in order of cost/benefit:**

1. **Smooth-UCB at the root (and optionally throughout).** `η_k = max(γ, η(1 + d√N_k)^{-1})`; with probability `1 − η_k` sample from the visit-count distribution instead of taking the argmax. Overhead ≈ 0. Heinrich & Silver's tuned values (`γ = 0.1`, `η = 0.9`, `d ∈ [10^{-3}, 5×10^{-5}]`) are a sensible starting grid. This alone converts a pure root policy into a mixed one and was enough to turn a *diverging* UCT into a converging one in Kuhn and Leduc.
2. **Sample the final move from root visit frequencies** rather than argmax, at least when the top few actions are within noise (the `A*` rule of eq. 3 is a principled way to define "within noise": `μ_{a*} − μ_a < min(σ_{a*}, σ_a)`).
3. **Self-determinization (`SPLIT`)** for deliberate information hiding: devote a fraction of iterations to determinizations drawn from `ψ` (the public-only belief) so the search sees the value of asks that *mislead* opponents about your hand. Cowling et al. found 3/8–7/8 of budget all worked; and at very large budgets `SPLIT` alone produced bluffing without the explicit `BLUFF` rule.
4. **The `BLUFF` selection rule** (eq. 3) as an explicit override when you want deception now rather than at 10^6 iterations.

**The tension unique to Fish:** the same public ask both *signals to your team* and *leaks to opponents*. Unlike Resistance (where `ψ` is purely about hiding), Fish needs `ψ` to serve two masters. Practical resolution: keep a single public-belief distribution `ψ`, and let the *team reward* arbitrate — because teammates' declarations are evaluated from *their* belief (§4.3c) and opponents' counter-play is evaluated from *theirs*, a search that re-determinizes both will automatically trade signal against leak. This only works if §4.4 is implemented; without it the search sees only the leak side or neither.

### 4.8 Techniques to *not* build for v0.4

| technique | verdict | reason |
|---|---|---|
| MO-ISMCTS / SO-ISMCTS+POM | skip | collapse to SO-ISMCTS under full move observability |
| MT-ISMCTS (tree per possible opponent infoset) | skip | requires small information sets; Fish's are `10^{28}` |
| Full continual re-solving / DeepStack-style gadget | skip for now | needs `CBV^{σ_2}(I_1)` per opponent hand; dense public tree; MCCR empirically loses to IS-MCTS on nearly every domain tested |
| OOS as the online engine | skip as engine | beaten head-to-head by ISMCTS in large games; **but keep as an offline exploitability auditor** on a reduced Fish variant |
| DESPOT | skip | its regret bound and regularizer are designed for single-agent POMDPs with a *small good policy*; Fish is adversarial and has no small policy. The *scenario* concept is worth borrowing conceptually — a DESPOT is essentially "ISMCTS with a fixed particle pool and a policy-size penalty" |
| RAVE / AMAF | skip | demonstrably harmful in order-sensitive card games |
| Full-length random playouts | skip | Battleship lesson: high-variance returns make the tree add little over flat rollouts; Spades lesson: random play to game end never reaches the score threshold |

### 4.9 A concrete recommended architecture

```
Belief module (runs between real decisions)
  ├── hard-constraint propagator  (forbidden card→seat edges, capacities, ≥1-in-half-suit)
  ├── feasibility check           (bipartite max-flow / Hall)
  ├── MH swap chain               (Solinas RingSwap analogue, reach-weighted acceptance)
  ├── particle pool P  (|P| ≈ 2·10^3), filtered forward + retrospectively (DSMCP style)
  └── two distributions:  φ (my true belief)   and   ψ (public-only belief, for hiding + re-determinization)

Search module (per decision, 10^4–10^5 iterations)
  ├── ONE tree over public action histories
  ├── determinization per iteration: draw from φ with prob (1−β_self), from ψ with prob β_self
  ├── selection: availability-count UCB (c ≈ 0.7 on unit-range rewards)
  │              + Smooth-UCB mixing at the root (η_k schedule)
  ├── non-root seat nodes: RIS-MCTS re-determinize from ψ (lazily, at ask-choice and declare nodes)
  ├── declare(H) : information-set-level move; reward = 2·q̂_H − 1 computed against the particle pool
  ├── simulation: NAST-2 / MAST policy, truncated at horizon
  └── backup: team-reward vector, max^n

Move selection
  └── A* = {a : μ_{a*} − μ_a < min(σ_{a*}, σ_a)};  sample within A* by visit frequency (or BLUFF rule)
```

Tuning knobs that the literature says actually matter, in priority order:
1. **Belief sampler bias** (Spades blunders; 87%-vs-33% example) — measure it, do not assume it.
2. **`β_self`, the self-determinization fraction** — 3/8 to 7/8 worked in Resistance; expect a broad plateau.
3. **RIS-MCTS gating probability** — trades leakage-correctness against statistical pollution.
4. **Playout horizon and the evaluation normaliser `c`** — get the reward into unit range or every UCB constant in the literature stops transferring.
5. **Iteration count** — *least* important. 1200 → 5000 bought only 7–8% in Spades.
6. **UCB exploration constant** — Cowling et al. found insensitivity within `[0.3, 1.5]`; use 0.7 and move on.

---

## 5. Pitfalls, negative results and failure modes

1. **ISMCTS does not converge.** Whitehouse's thesis: it "oscillat[es] between several policies or settl[es] on a policy which does not form part of a Nash equilibrium." Do not build evaluation infrastructure that assumes more iterations ⇒ better play.
2. **ISMCTS's exploitability can *increase* with computation.** Demonstrated in II-Goofspiel by Lisý et al. If you self-play-tune against your own ISMCTS, you may be optimising into an exploitable corner.
3. **Pessimism collapse.** In Phantom (4,4,4), SO/MO-ISMCTS get *worse* than determinized UCT above 1.5 s/move: assuming the opponent knows the hidden state makes the search conclude the game is lost, at which point all lines look equal and it plays randomly. The same effect was seen for the `TRUE ONLY` spy in Resistance. **In Fish this could appear as a bot that stops trying once the opponents "obviously" know everything.** Guard against it: never let the opponent model be strictly better-informed than reality (this is another argument for re-determinizing from `ψ`).
4. **Node-expansion bottleneck from information-set-wide action sets.** In Dou Di Zhu, 1000 determinizations surfaced ~1500 unique leading plays while any single determinization has only ~88. ISMCTS then spends its budget expanding opponent nodes near the root and "very rarely explores beyond these nodes with only 10,000 simulations." *Fish is safe from this at your own nodes* (own-hand-determined legality) *but not at opponent nodes* — an opponent could legally ask about any half-suit they might hold. **Mitigation: prune opponent asks to half-suits the opponent is publicly known or strongly believed to hold, and move-group the ask as (choose half-suit) → (choose card, choose target)**, mirroring Cowling's kicker move-grouping.
5. **Biased determinization causes specific, diagnosable blunders.** The Spades post-mortem is the best cautionary tale in this literature: their inference-driven dealer put a card with true probability 1/3 into 87% of determinizations, causing a systematic misplay. Replacing it with an unbiased-but-hard-constraint-only sampler fixed those cases and broke others. **Build the sampler so you can A/B it and measure the marginals it induces against ground truth in self-play.**
6. **Particle deprivation.** Guaranteed in Fish, because the constraint set grows monotonically. Without reinvigoration you will end up with `|P| = 1` and silently degenerate to PIMC-with-one-world.
7. **Re-determinization pollutes statistics.** Goodman's own caveat: RIS-MCTS back-propagates outcomes from worlds inconsistent with the root information set, which may cap performance at large budgets.
8. **Conventions do not emerge.** In Hanabi the "playable now" convention was worth ~2.5 points and was **hand-coded**, not discovered. Expect the same in Fish: profitable ask conventions ("I only ask in half-suit H if I hold ≥2 of it") will probably have to be encoded, or learned by a separate self-play process, not found by tree search.
9. **Unsafe subgame solving is variable, not merely suboptimal.** Brown & Sandholm: 65.59 mbb/h at 200 buckets but **396.8** at 30,000 buckets — *worse than the trunk strategy it was improving*. Any ad-hoc "re-solve this half-suit endgame" heuristic in Fish inherits this risk.
10. **Continual resolving's dependence on value estimates is fragile in non-poker domains.** MCCR's CFV estimates stabilised quickly only in small domains; in Goofspiel and Phantom TTT the error decreased slowly, and CR's guarantee degrades directly with CFV error (`Theorem 3.6`: exploitability bounded by a sum of per-resolver exploitability and value-estimation error terms).
11. **The tree may add little over flat rollouts when returns are high-variance.** POMCP on Battleship. In Fish, if your evaluation is noisy (e.g. full random playouts), you may be paying for a tree that is not earning its keep — measure ISMCTS against a flat information-set Monte Carlo baseline before optimising the tree.
12. **Determinization never plays to gather or hide information** (Russell & Norvig's "averaging over clairvoyance"; Ginsberg conceded the point for GIB). In Fish, where *every* ask is simultaneously an information probe and a broadcast, this is not a minor defect — it removes an entire strategic dimension unless §4.4 is implemented.
13. **Sampling a consistent history can be NP-hard in general** (Solinas et al., Theorem 1). Fish's constraint structure is benign (mostly forbidden edges plus a few disjunctions), so max-flow feasibility works, but any future rule addition that creates disjunctive constraints (e.g. "you must have had a card in that half-suit at that time") can push feasibility checking into search. Keep the constraint store explicit and testable.
14. **Subset-armed bandit statistics are averaged over subsets.** Cowling et al. flag that an action cannot have a subset-dependent value under the availability-count fix, and that per-subset statistics are impractical. In Fish this means an opponent action's value is averaged over all the hands in which it was legal — acceptable, but a known approximation.

---

## 6. Bibliography

All entries below were retrieved and read (full text or the relevant sections) unless marked otherwise.

1. Cowling, P. I., Powley, E. J., & Whitehouse, D. (2012). **Information Set Monte Carlo Tree Search.** *IEEE Transactions on Computational Intelligence and AI in Games*, 4(2), 120–143. DOI 10.1109/TCIAIG.2012.2200894. https://eprints.whiterose.ac.uk/id/eprint/75048/1/CowlingPowleyWhitehouse2012.pdf
2. Whitehouse, D. (2014). **Monte Carlo Tree Search for Games with Hidden Information and Uncertainty.** PhD thesis, University of York. https://etheses.whiterose.ac.uk/8117/1/Feb%2016%20-%20FINAL.pdf
3. Cowling, P. I., Whitehouse, D., & Powley, E. J. (2015). **Emergent Bluffing and Inference with Monte Carlo Tree Search.** *IEEE Conference on Computational Intelligence and Games (CIG 2015)*, Tainan, Taiwan, pp. 114–121. http://orangehelicopter.com/academic/papers/cig15.pdf
4. Whitehouse, D., Cowling, P. I., Powley, E. J., & Rollason, J. (2013). **Integrating Monte Carlo Tree Search with Knowledge-Based Methods to Create Engaging Play in a Commercial Mobile Game.** *AAAI Conference on Artificial Intelligence and Interactive Digital Entertainment (AIIDE)*. https://cdn.aaai.org/ojs/12679/12679-52-16196-1-2-20201228.pdf
5. Heinrich, J., & Silver, D. (2015). **Smooth UCT Search in Computer Poker.** *IJCAI 2015*, pp. 554–560. https://www.ijcai.org/Proceedings/15/Papers/084.pdf
6. Silver, D., & Veness, J. (2010). **Monte-Carlo Planning in Large POMDPs.** *Advances in Neural Information Processing Systems 23 (NIPS 2010)*. https://papers.nips.cc/paper/4031-monte-carlo-planning-in-large-pomdps
7. Ye, N., Somani, A., Hsu, D., & Lee, W. S. (2017). **DESPOT: Online POMDP Planning with Regularization.** *Journal of Artificial Intelligence Research*, 58, 231–266. arXiv:1609.03250. https://arxiv.org/pdf/1609.03250
8. Lisý, V., Lanctot, M., & Bowling, M. (2015). **Online Monte Carlo Counterfactual Regret Minimization for Search in Imperfect Information Games.** *AAMAS 2015*, pp. 27–36. https://mlanctot.info/files/papers/aamas15-iioos.pdf
9. Brown, N., & Sandholm, T. (2017). **Safe and Nested Subgame Solving for Imperfect-Information Games.** *Advances in Neural Information Processing Systems 30 (NIPS 2017)*. arXiv:1705.02955. https://arxiv.org/pdf/1705.02955
10. Šustr, M., Kovařík, V., & Lisý, V. (2019). **Monte Carlo Continual Resolving for Online Strategy Computation in Imperfect Information Games.** *AAMAS 2019*. arXiv:1812.07351. https://arxiv.org/pdf/1812.07351
11. Šustr, M., Schmid, M., Moravčík, M., Burch, N., Lanctot, M., & Bowling, M. (2020). **Sound Algorithms in Imperfect Information Games.** arXiv:2006.08740. https://arxiv.org/pdf/2006.08740
12. Long, J. R., Sturtevant, N. R., Buro, M., & Furtak, T. (2010). **Understanding the Success of Perfect Information Monte Carlo Sampling in Game Tree Search.** *AAAI 2010*, pp. 134–140. https://webdocs.cs.ualberta.ca/~nathanst/papers/pimc.pdf
13. Solinas, C., Rebstock, D., Sturtevant, N. R., & Buro, M. (2023). **History Filtering in Imperfect Information Games: Algorithms and Complexity.** *NeurIPS 2023*. arXiv:2311.14651. https://arxiv.org/pdf/2311.14651
14. Clark, G. (2021). **Deep Synoptic Monte-Carlo Planning in Reconnaissance Blind Chess.** *Advances in Neural Information Processing Systems 34 (NeurIPS 2021)*. arXiv:2110.01810. https://arxiv.org/pdf/2110.01810
15. Goodman, J. (2019). **Re-determinizing Information Set Monte Carlo Tree Search in Hanabi.** arXiv:1902.06075; presented at CIG 2018 (competition winner) and IEEE Conference on Games (CoG) 2019. https://arxiv.org/abs/1902.06075 — *(read via the ar5iv HTML rendering; the venue attribution "CIG 2018 / CoG 2019" comes from that rendering and is **UNVERIFIED** against the official proceedings.)*
16. Auger, D. (2011). **Multiple Tree for Partially Observable Monte-Carlo Tree Search.** *Applications of Evolutionary Computation (EvoApplications 2011)*, LNCS 6624, pp. 53–62. https://hal.science/hal-00563480v2/document — *(abstract and description confirmed via search and via citations in [1] and [8]; **full text not read**, so the reported "93% win rate vs random, 82% vs belief samplers at 50 M simulations" is **UNVERIFIED**.)*
17. Frank, I., & Basin, D. (1998). **Search in Games with Incomplete Information: A Case Study Using Bridge Card Play.** *Artificial Intelligence*, 100(1–2), 87–123. — *cited in [1]; **not read directly**.*
18. Ginsberg, M. L. (2001). **GIB: Imperfect Information in a Computationally Challenging Game.** *Journal of Artificial Intelligence Research*, 14, 303–358. — *cited in [1] and [4]; **not read directly**.*
19. Auer, P., Cesa-Bianchi, N., & Fischer, P. (2002). **Finite-time Analysis of the Multiarmed Bandit Problem.** *Machine Learning*, 47(2–3), 235–256. — *source of UCB1 and UCB-Tuned; cited in [1] and [3]; **not read directly**.*
20. Auer, P., Cesa-Bianchi, N., Freund, Y., & Schapire, R. E. (2002). **The Nonstochastic Multiarmed Bandit Problem.** *SIAM Journal on Computing*, 32(1), 48–77. — *source of EXP3 and of the `γ`, `η` schedule used by [1, Corollary 4.2]; **not read directly**.*
21. Kocsis, L., & Szepesvári, C. (2006). **Bandit Based Monte-Carlo Planning.** *ECML 2006*, pp. 282–293. — *source of UCT; cited throughout; **not read directly**.*
22. Lanctot, M., Waugh, K., Zinkevich, M., & Bowling, M. (2009). **Monte Carlo Sampling for Regret Minimization in Extensive Games.** *NIPS 2009*, pp. 1078–1086. — *source of MCCFR/outcome sampling underlying [8, 10]; **not read directly**.*
23. Whitehouse, D., Powley, E. J., & Cowling, P. I. (2011). **Determinization and Information Set Monte Carlo Tree Search for the Card Game Dou Di Zhu.** *IEEE CIG 2011*, Seoul, pp. 87–94. — *the Dou Di Zhu results reported in §3 are quoted from [1], which supersedes and re-runs them; **the 2011 paper itself was not read**.*
24. Powley, E. J., Cowling, P. I., & Whitehouse, D. (2014). **Information Capture and Reuse Strategies in Monte Carlo Tree Search, with Applications to Games of Hidden Information.** *Artificial Intelligence*, 217, 92–116. https://www.sciencedirect.com/science/article/pii/S0004370214001052 — *the journal version of thesis chapter 8 [2]; **only the thesis chapter was read**, and the ICARUS specifications above are quoted from the thesis.*
25. Bitan, M., & Kraus, S. (2017). **Combining Prediction of Human Decisions with ISMCTS in Imperfect Information Games** (SDMCTS). arXiv:1709.09451. https://arxiv.org/abs/1709.09451 — *identified via search; **full text not read**; author attribution **UNVERIFIED**.*
26. "Incentivizing Information Gain in Hidden Information Multi-Action Games." *Springer LNCS chapter*, DOI 10.1007/978-3-031-34017-8_6. Evaluates an information-gain incentive plus a "risk determinization" inside PIMCTS and ISMCTS on the multi-action hidden-information game TUBSTAP. https://link.springer.com/chapter/10.1007/978-3-031-34017-8_6 — ***UNVERIFIED**: authors and year could not be confirmed; full text not accessible (the DTIC mirror returned HTTP 403). Listed because the technique (an explicit information-gain bonus inside ISMCTS) is directly relevant to Fish's ask-as-probe problem and is worth chasing down.*
27. Ryan1729. **canadian-fish** (single-player implementation of the card game). GitHub. https://github.com/Ryan1729/canadian-fish — *found via search; a rules implementation with simple inference heuristics, **not** an academic reference and **not read in detail**. Noted only to record that no published academic AI work on Canadian Fish / Literature was found.*

**Search coverage note.** I searched specifically for prior academic work on Canadian Fish / Literature and found **none**. The closest published domains are Dou Di Zhu (3-player, 2-vs-1 team, ladder card game), Spades and Hearts (4-player trick-taking with partnerships), Skat (3-player, 2-vs-1), Hanabi (fully cooperative with implicit signalling), and The Resistance (hidden-role team deduction). Of these, **Hanabi is the closest structural analogue for the signalling problem and The Resistance for the inference/bluffing machinery**; neither has Fish's public-transfer property, which makes Fish *easier* on belief tracking (monotone information gain, high disambiguation) and *harder* on signalling (every signal is broadcast to opponents too).
