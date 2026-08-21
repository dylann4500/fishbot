# Perfect Information Monte Carlo (Determinization) in Trick-Taking and Hidden-Hand Card Games

**Literature review for the Canadian Fish / Literature agent project — v0.4**
Scope: PIMC / determinization, its two classical pathologies, the Long–Sturtevant–Buro–Furtak predictive framework, Skat/Bridge/Hearts/Spades engineering, multi-player search (maxn / paranoid / MP-Mix / multi-player UCT), and PIMC repairs (vector minimaxing, payoff-reduction minimaxing, αμ, IIMC/RecPIMC, EPIMC).

---

## 1. Executive summary

1. **PIMC = determinize + solve + vote.** Sample a full assignment $w$ of hidden cards consistent with the public history, solve the resulting *perfect-information* game exactly (a "double dummy solver", DDS), record the value of each root move, repeat, play the argmax of the average. Proposed by Levy (1989) for Bridge, made to work by Ginsberg's GIB (JAIR 2001), and still state of the art in Bridge and Skat.
2. **PIMC is provably unsound.** Frank & Basin (AIJ 1998) identified two failure modes that persist *no matter how many worlds are sampled*: **strategy fusion** (PIMC secretly plays a different strategy in each world, whereas a real player must play one strategy for the whole information set) and **non-locality** (a node's true value depends on parts of the tree outside its subtree, because informed opponents steer play away from regions bad for them).
3. **Yet PIMC works in trick games.** Long et al. (AAAI 2010) explain why with three measurable tree properties — **leaf correlation $lc$**, **bias $b$**, **disambiguation factor $df$** — and show PIMC is near-optimal in the region occupied by Skat and Hearts ($lc \in [0.8,1.0]$, $df \approx 0.6$) and bad in the region occupied by poker ($lc = 0.5$, $df = 0$).
4. **Concrete PIMC error size (Skat).** Against a CFR solution in 3-tricks-left endgames PIMC loses **0.42 tournament points/deal**; only 15% of deals are still undecided that late, so the average loss is **0.063 TP/deal**, i.e. ~11.3 TP over a 180-deal tournament against an empirical TP standard deviation of 778 — negligible in practice.
5. **PIMC never makes an information-gathering play.** Ginsberg states this explicitly: because the perfect-information variant assumes the information will be available at the *next* decision, PIMC's tendency is to *defer* decisions rather than *resolve* them. This is the single most important structural fact for Canadian Fish.
6. **Repairs that work, ranked by measured payoff.** Payoff-reduction minimaxing (prm) turned 66.3% → **95.8%** correct on 650 Bridge single-suit problems at a small constant-factor slowdown. αμ (Pareto-front search) beats PIMC in Bridge 3NT (60.2% → 63.0% at M=2, 20 worlds). RecPIMC/IIMC drops best-response exploitability on Hearts-like synthetic trees from **0.299 → 0.088** and beats PIMC-Kermit *past PIMC's saturation point*. Better *inference* (biasing which worlds you sample) gave Kermit +217 tournament points per 36 games.
7. **PIMC saturates.** Kermit's Skat strength stops improving beyond ~160 sampled worlds; GIB used only 50 worlds normally and saw "little or no improvement" beyond 500. Extra compute must be spent on *better evaluation or better inference*, not more determinizations.
8. **Recursion beats width.** IIMC/RecPIMC replaces the perfect-information leaf evaluator with an actual *imperfect-information playout* by a player module. One level of recursion captures nearly all the gain (R1 ≈ 0.088 exploitability, R8 ≈ 0.055 on a 0–4 scale).
9. **Postponing the leaf evaluator only helps when observations are private.** EPIMC (Arjonilla, Saffidine, Cazenave 2024) shows large gains in Dark Chess/Dark Hex/Phantom TTT but **zero gain in games whose observations are mostly public** (their "Card game", Battleship). Canadian Fish is a fully-public-observation game, so this specific repair is predicted not to help.
10. **Inference is where the points are.** Kermit(with inference) vs Kermit(no inference): **996 vs 779** points per 36 games. Defender-side inference mattered most. Neural individual-card inference (Solinas et al. 2019) and policy-based reach-probability inference (Rebstock et al. 2019) both improved on the table-based baseline, measured by **True State Sampling Ratio** $\mathrm{TSSR} = p(s^\*\mid h)\cdot n$.
11. **Multi-player search theory mostly *does not* apply to Fish.** Fish is two teams with identical intra-team payoffs, so the team-level game is 2-player constant-sum: use minimax/αβ on team score. maxn's equilibrium-selection and tie-breaking pathologies (Sturtevant) apply to free-for-all games, not to 2-team games.
12. **But the MP-Mix "Opponent Impact" analysis names Go Fish directly.** Zuckerman, Felner & Kraus define $OI(G,H) = |\text{InfluentialStates}|/|\text{TotalStates}|$ and state that **Go Fish has $OI = 1$** (every state lets you directly damage a chosen opponent) versus Hearts where it is small. Target selection is therefore a first-class strategic axis in Fish in a way it is not in trick games.
13. **Fish's information set is enormous compared to prior PIMC domains.** From one seat at deal time: $45!/(9!)^5 \approx 1.9\times10^{28}$ worlds, versus $\approx 8.4\times10^{16}$ for a Bridge hand and $4.3\times10^{7}$ for a Skat defender. Enumerate-and-weight (Kermit, Solinas) is impossible; you need a constrained *sampler*.
14. **Fish's perfect-information game is nearly trivial, which is exactly the worst case for PIMC.** Under full information the player on turn essentially never fails an ask and never mis-declares, so the DDS value collapses to "whichever team is on turn sweeps what it can reach". PIMC then degenerates to "maximise $P(\text{target holds the card})$" — a greedy one-ply heuristic that ignores information leakage, information gain, declaration option value, and partner signalling.
15. **Recommended architecture (detail in §5):** constrained belief sampler → **vector/Pareto-valued search over sampled worlds** (prm or αμ, not plain PIMC) with an **imperfect-information playout evaluator** (IIMC) → separate **Bayesian declaration module** that maximises expected sets rather than searching.

---

## 2. Algorithms and formalisms

Notation. $\mathcal{I}$ is the acting player's information set (set of world states $w$ indistinguishable given the public history $h$). $A(\mathcal{I})$ is the legal move set (identical in every $w\in\mathcal{I}$ in a well-formed game). $\Pr(w\mid h)$ is the belief. $V^{\text{PI}}(w,m)$ is the perfect-information (double-dummy) value of playing $m$ in world $w$.

### 2.1 PIMC / determinization

The canonical formulation (Long et al. 2010; Furtak & Buro 2013; Ginsberg 2001, Alg. 3.0.1):

$$
m^\* \;=\; \arg\max_{m \in A(\mathcal I)} \; \sum_{w \in D} \Pr(w \mid h)\; V^{\text{PI}}\!\big(w, m\big),
\qquad D \subset \mathcal I,\; |D| = N .
$$

Ginsberg writes it as $\arg\max_m \sum_d w_d\, s(m,d)$ with per-deal weights $w_d$ obtained by Bayesian adjustment (see §2.6). Frank/Basin/Matsubara write the same object as $f(M_i)=\sum_{j=1}^n e_{ij}\Pr(w_j)$ where $e_{ij}$ is the minimax value of the node under branch $M_i$ in world $w_j$.

Pseudocode (Furtak & Buro, Alg. 1):

```
PIMC(InfoSet I, int N):
  for m in Moves(I): val[m] = 0
  for i in 1..N:
      x = Sample(I)                       # inference module
      for m in Moves(I):
          val[m] += PerfInfoValue(x, m)   # DDS / alpha-beta
  return argmax_m val[m]
```

Two knobs dominate strength: the **quality of `Sample`** (inference) and the **speed of `PerfInfoValue`**. Buro et al. note the trivial parallelisation (root/world parallelism) and that continuing to evaluate obviously-inferior moves is wasteful — UCB or OCBA-style budget allocation at the root is an easy win.

Variant with an explicit belief (Solinas et al. 2019, Alg. 1): compute $p[s] \leftarrow \text{ProbabilityEstimate}(s,h)$ for $s \in \mathcal I$, normalise, then sample $s \sim p$. This is only feasible when $|\mathcal I|$ is enumerable (Skat: up to $4.3\times10^7$, "manageable in around 2 seconds").

Ginsberg's scoring nuance (also Kupferschmid & Helmert): raw expected *points* and raw *win/loss* are both wrong. K&H's rule: first compute the set of cards that win the maximal number of sample deals, then break the tie by average point total.

### 2.2 Strategy fusion — formal statement

**Definition (Arjonilla et al. 2024, Def. 1).** For a game $G$, a policy $\pi \in \Pi(G)$ and an infostate $s\in I(G)$ *creates strategy fusion* if $\exists h,h' \in H(s)$ with $\pi(h) \neq \pi(h')$. The quantity of strategy fusion is

$$
\mathrm{SF}(\pi, G) \;=\; \big|\{\, s \in S(G) \;:\; \pi \text{ creates strategy fusion in } s \,\}\big| .
$$

$\mathrm{SF}(\pi)=0$ means none. PIMC has $\mathrm{SF}>0$ by construction: it re-solves each world independently, so it silently permits $\pi(h)\neq\pi(h')$ for $h,h'$ in the same infostate at every node below the root.

Long et al.'s structural condition for fusion to actually *cost* you a move: (i) there must be moves with **anti-correlated** values in one part of the tree, and (ii) there must be a move elsewhere that is guaranteed better. If the "safe" alternative were also losing, PIMC would still pick correctly (it would merely overestimate the value).

Canonical Bridge instance (Ginsberg's ♠KJT7 opposite ♠A986): PIMC believes it always takes four spade tricks, because in each determinized world the finesse "works". The truth is a 50% proposition — and the fusion also corrupts the bidding, since the program thinks it makes 4 tricks with certainty.

### 2.3 Non-locality — formal statement

In a perfect-information game the value of a node is a function only of its subtree. In an imperfect-information game it is not: an opponent with private information steers play toward regions favourable to them, so a node may simply *never be reached* in some worlds, and payoffs in those worlds should not influence the choice made at that node.

Cazenave & Ventos's example (from Frank, Basin & Matsubara), 3 worlds, strategy fusion at Max and perfect info at Min. Max node $d$ has children $[1\,0\,0]$ (mean $\tfrac13$) and $[0\,1\,1]$ (mean $\tfrac23$); greedy mean-maximisation backs up $[0\,1\,1]$ at $d$, and node $b$ then evaluates to $[0\,0\,0]$. Choosing the *locally worse* $[1\,0\,0]$ at $d$ makes $b$ back up $[1\,0\,0]$, which is globally better. Local optimality destroys global optimality — hence the need to back up **sets** of vectors (§2.5).

### 2.4 The Long–Sturtevant–Buro–Furtak (BCD) predictive framework

This is the single most transferable tool in the literature: it tells you *a priori* whether PIMC will work in a new game.

**Synthetic tree generator.** Two-player zero-sum, alternating moves, all chance at the root, binary branching, depth 8, $W = 8$ worlds per player (chance node degree $W^2 = 64$), players' hidden information strictly disjoint, all moves publicly observed, leaf payoffs in $\{+1,-1\}$.

**Leaf correlation $lc \in [0,1]$.** With probability $lc$ a sibling pair of terminal nodes gets the *same* payoff; with probability $1-lc$ the pair is **anti-correlated**, one leaf $+1$ and its sibling $-1$ at random.

$$
\Pr\big[\,\mathrm{val}(\ell_L)=\mathrm{val}(\ell_R)\,\big] \;=\; lc .
$$

**Bias $b \in [0,1]$.** Applied only to *correlated* pairs: the shared value is $+1$ with probability $b$, $-1$ with probability $1-b$. Anti-correlated pairs are unaffected.

$$
\Pr\big[\,\mathrm{val}=+1 \;\big|\; \text{correlated}\,\big] \;=\; b .
$$

**Disambiguation factor $df \in [0,1]$.** Each information set starts with $W$ nodes. Each time player $p$ is to move, every one of $p$'s information sets is split in half with probability $df$, **recursively** (each resulting half is again split with probability $df$, and so on). $df = 0$ ⇒ the player never learns anything; $df = 1$ ⇒ instant collapse to a perfect-information game.

*Derived closed form (mine, from their generative rule — not stated in the paper, but needed to convert real measurements into $df$).* Let $S(W)$ be the expected size of the piece containing the true world after one move:

$$
S(W) = (1-df)\,W + df \cdot S(W/2), \qquad S(1)=1
\;\;\Longrightarrow\;\;
S(W) = \frac{(1-df)\,W}{1-\tfrac{df}{2}} .
$$

So the expected per-move information-set retention ratio is

$$
\rho \;=\; \mathbb{E}\!\left[\frac{|\mathcal I_{t+1}|}{|\mathcal I_{t}|}\right] \;=\; \frac{2(1-df)}{2-df},
\qquad\text{and inverting,}\qquad
\boxed{\;df \;=\; \frac{2(1-\rho)}{2-\rho}\;}
$$

Sanity checks: $\rho=1 \Rightarrow df=0$; $\rho=0 \Rightarrow df=1$; the paper's measured $df\approx0.6$ for Skat/Hearts corresponds to $\rho \approx 0.571$, i.e. an information set shrinks to ~57% of its size between one's own consecutive moves. Use this formula to place a new game on their maps.

**How to measure the three properties in a real game (the paper's recipe, verbatim in substance):**

- Run **random playouts** from the position class of interest.
- Walk down avoiding moves that lead *directly* to terminal positions (after collapsing chains with only one legal move). A node all of whose moves lead directly to terminal positions is **pre-terminal**.
- A pre-terminal node is **correlated** iff all of its move values are equal. Then
  $$ \widehat{lc} = \frac{\#\{\text{correlated pre-terminal nodes}\}}{\#\{\text{pre-terminal nodes}\}}, \qquad \widehat{b} = \frac{\#\{\text{correlated pre-terminal nodes that are wins for the reference player}\}}{\#\{\text{correlated pre-terminal nodes}\}} . $$
- **Disambiguation**: compare the number of consistent worlds now versus the last time the same player was to move, average the ratio to get $\rho$, convert with the boxed formula. (Skat needed only 10 rollouts/world because the ratios clustered tightly around $df=0.6$.)
- Sampling budgets used: 10,000 games per Skat type with 1,000 leaf measurements each; 3,000 Hearts games with 500 sample points each.

**Findings.**

- PIMC is *worst when $lc$ is low.* Explanation: with anti-correlation deep in the tree, PIMC always believes the critical decision comes "later", so early moves look irrelevant — yet under a real information-set structure early moves matter enormously.
- $b$ has a small effect; extreme bias helps PIMC (fewer effectively anti-correlated interior nodes near the leaves). At $b=1, lc=1$ even a random player plays perfectly.
- $df$ behaves counter-intuitively in absolute terms (low $df$ looks "good" for PIMC) but the **relative** picture — PIMC's gain over a random player — reverses cleanly: low $df$ ⇒ lots of luck ⇒ random does fine and PIMC gains little; as $df$ rises, random collapses and PIMC holds its own; as $df \to 1$ the game becomes perfect information and PIMC → optimal.
- Interesting corner: at high $df$ (0.7–0.9), *low* $lc$ actually helps PIMC, because the game becomes perfect-information quickly and low correlation means a win is still available when PIMC starts playing perfectly.

**Measured placements.**

| Game | $lc$ | $b$ | $df$ | PIMC verdict |
|---|---|---|---|---|
| Skat (suit, grand, null) | 0.8–1.0 | wide spread | ≈ 0.6 | loses ≈ 0.1 pts/game to equilibrium (payoffs ±1), gains ≈ 0.4 over random |
| Hearts | 0.8–1.0 | wide spread | ≈ 0.6 | same regime |
| Kuhn poker | 0.5 | 0.5 | 0 | PIMC weak; near-random improvement |

**Kuhn poker table (their Table 1)** — average payoff of the row player:

| Player | vs Nash | vs Best-Response |
|---|---|---|
| Random (p1) | −0.161 | −0.417 |
| Random (p2) | −0.130 | −0.500 |
| PIMC (p1) | −0.056 | −0.083 |
| PIMC (p2) | +0.056 | −0.166 |

Reading: PIMC achieves the *equilibrium* payoff against a Nash opponent (because it never takes a dominated action), but is **significantly exploitable** by a best-responder. This is the key warning: PIMC's loss vs. equilibrium is a *lower bound* on its loss vs. an opponent that models it.

Also from the same paper, on why abstraction+CFR is not an option in trick games: CFR runs in space proportional to the number of information sets and time $O(I^2 N)$; a Bridge hand alone is $\binom{52}{13}\approx6.35\times10^{11}$ and Skat has $\ge 10! \cdot H \approx 1.54\times10^{14}$ information sets with $H=\binom{22}{10}\binom{12}{10}=42{,}678{,}636$.

### 2.5 Repairs that back up richer values

#### (a) Vector minimaxing (Frank, Basin & Matsubara, AAAI-98)

Give every leaf a payoff **vector** $\vec P(v)$ with $\vec P[j](v)$ the payoff in world $w_j$. Then

$$
\text{vector-mm}(t) =
\begin{cases}
\vec P(t) & t \text{ leaf} \\[4pt]
\displaystyle\min_{t_i \in \mathrm{sub}(t)} \text{vector-mm}(t_i) & \text{MIN node} \\[4pt]
\displaystyle\max_{t_i \in \mathrm{sub}(t)} \text{vector-mm}(t_i) & \text{MAX node}
\end{cases}
$$

with **asymmetric** operators:

$$
\max\{\vec P_1,\dots,\vec P_m\} \;=\; \vec P_{i^\*},\quad i^\* = \arg\max_i \sum_{j=1}^n \Pr(w_j)\,\vec P_i[j]
\qquad\text{(one whole vector — no fusion)}
$$

$$
\min\{\vec P_1,\dots,\vec P_m\} \;=\; \Big(\min_i \vec P_i[1],\; \min_i \vec P_i[2],\;\dots,\; \min_i \vec P_i[n]\Big)
\qquad\text{(componentwise — MIN is clairvoyant)}
$$

The MAX rule ("commit to one branch in all worlds") eliminates strategy fusion for MAX. The MIN rule encodes the **best defence model**: opponents know the world exactly, which is the most conservative assumption and is *also the assumption human expert texts make* when quoting lines of play. Non-locality remains.

#### (b) Payoff-reduction minimaxing (prm)

```
prm(t):
 1. Minimax each world w_k separately; record for every MIN node its minimax value m_k in that world.
 2. For every leaf, reduce its payoff P_k in world w_k to  min( P_k , min over MIN-ancestors of m_k ).
 3. Run vector-mm on the reduced tree.
```

$$
\tilde P[k](v) \;=\; \min\Big( P[k](v),\; \min_{u \in \mathrm{MINanc}(v)} m_k(u) \Big)
$$

The reduction parameterises each leaf with information from elsewhere in the tree — it removes payoffs that MIN would never allow to be realised — while provably not changing the game-theoretic value of any individual world (no payoff is cut so far that MIN gains a better branch anywhere).

**Results (650 single-suit problems from the ACBL *Official Encyclopedia of Bridge*):**

| Algorithm | Correct | Incorrect | Expected losses over the 650 |
|---|---|---|---|
| Monte-carlo sampling | 431 (66.3%) | 219 (33.7%) | 16.97 |
| Vector minimaxing | 462 (71.1%) | 188 (28.9%) | 12.78 |
| **prm** | **623 (95.8%)** | 27 (4.2%) | **0.83** |

prm was good enough to find **five errors in the Encyclopedia**.

On random binary trees with $n=10$ worlds, payoffs assigned via the Last Player Theorem with $p=(3-\sqrt5)/2\approx0.38197$ (which keeps the probability of a forced win constant across depths), and MIN knowledge level $i/(n-1)$: at depth 9 with full MIN knowledge MC errs 99.9% and vector-mm 96%, while prm still finds the optimum >40% of the time; at depths 11–13 MC and vector-mm *never* find a correct solution and prm still scores 30–40%.

**Cost:** all three are polynomial in tree size. 1,000 depth-13 trees: prm 571s, vector-mm 333s, MC 372s — i.e. **prm is a ~1.5–1.7× constant-factor slowdown for a ~8× error reduction.** This is by far the best measured effort/benefit ratio in this literature.

**World-similarity parameter $q$.** They also parameterised random trees by $q$ = probability that a world's payoff vector is overwritten by a shared "dummy world" vector (i.e. how similar worlds are). At $q\approx0.75$ the three algorithms' error rates (34.1%, 31.5%, 6.1%) match their Bridge error rates almost exactly. This is a second, simpler "is my game PIMC-friendly?" statistic worth measuring.

**Complexity.** Frank & Basin, *Optimal Play against Best Defence: Complexity and Heuristics* (CG 1998): finding optimal strategies in the best defence model is **NP-complete in the size of the game tree**; they introduce two heuristics that outperform earlier algorithms and, on a hard Bridge set, outperform the human experts who wrote the model solutions.

#### (c) αμ (Cazenave & Ventos, 2019; Cazenave, Legras & Ventos, 2021)

αμ generalises PIMC ($M=1$ *is* PIMC) and repairs both defects by backing up **Pareto fronts of boolean world-vectors**.

*Vectors.* Given $n$ possible worlds, a vector $x\in\{0,1\}^n$ records win(1)/loss(0) per world, plus a companion boolean vector marking which worlds are still *possible* at this node. Score of a vector $\mu(x) = \frac1{|\text{possible}|}\sum_{j \text{ possible}} x[j]$.

*Dominance.*
$$
x_1 \succ x_2 \iff \big(\forall i\in[1,n]: x_1[i] \ge x_2[i]\big) \wedge \big(\exists i: x_1[i] > x_2[i]\big)
$$
$$
f_1 \succeq f_2 \iff \forall x_2 \in f_2,\ \exists x_1 \in f_1 \text{ s.t. } x_1 \succ x_2 \ \text{ or } \ x_1 = x_2
$$

A Pareto front is the set of mutually non-dominated vectors; inserting a candidate removes everything it dominates and is itself discarded if dominated.

*Max nodes.* Front = **union** of the children's fronts, then reduced. Max keeps *all* its options open — this is what fixes non-locality.

*Min nodes.* Min is clairvoyant (knows Max's cards) so it takes the componentwise minimum; but because Max may later choose any member of any child front, the Min front is the **Pareto product**:

$$
f_{\min} \;=\; \mathrm{ParetoReduce}\Big(\big\{\, \textstyle\min_{k}\, x_k \;:\; (x_1,\dots,x_m) \in f_1\times\cdots\times f_m \,\big\}\Big)
$$

Worked example from the paper: children fronts $\{[0\,1\,1],[1\,1\,0]\}$ and $\{[1\,1\,0],[1\,0\,1]\}$ give the product $\{[0\,1\,0],[0\,0\,1],[1\,1\,0],[1\,0\,0]\}$ which reduces to $\{[0\,0\,1],[1\,1\,0]\}$.

*Depth parameter $M$* counts **Max moves only** — Min nodes do not decrement $M$, because searching one ply deeper at a Min node cannot change the result (the DDS already searched all worlds and Min may choose per world). At $M=0$ each remaining possible world is evaluated by a double dummy search.

*Cuts.*
- **Early cut**: if a Min node's front is dominated by the front of the Max node above, cut — a world lost at a node is lost at every node below, so more search can only shrink the front.
- **Root cut**: if a root move at depth $M$ gives the same $\mu$ as the best move at depth $M-1$, stop — deeper search can only lower the probability (strategy fusion is monotone), so the value cannot improve.
- **Transposition table** stores the front and the best move per node; iterative deepening over $M$.
- 2021 additions: *useful/useless worlds* tracking, *empty entry*, *α* cut, *world cuts*, *cut-on-win*.

*Results.* Bridge, fixed 1NT–3NT auction, duplicate scoring, deals filtered to those where PIMC wins 30–70% (13,000 scored games per row):

| Cards | M | Worlds | Discrepancies | Score |
|---|---|---|---|---|
| 52 | 1 (=PIMC) | 20 | 0/13000 | 60.2% |
| 52 | 2 | 20 | 169/13000 | **63.0%** |
| 52 | 3 | 20 | 276/13000 | 62.0% |
| 52 | 1 (=PIMC) | 40 | 0/13000 | 62.4% |
| 52 | 3 | 40 | 388/13000 | **63.2%** |
| 36 | 1 (=PIMC) | 20 | 0/13000 | 46.4% |
| 36 | 3 | 20 | 190/13000 | **48.2%** |

Note how *rarely* αμ differs from PIMC (≈1–3% of decisions) yet still gains 1–3 percentage points — PIMC's errors are concentrated in a small number of high-leverage positions.

Timing (52 cards, 20 worlds): $M=1$ 0.096 s/move; $M=3$ with no optimisations 18.678 s; with TT + root cut + early cut **1.228 s** (15× faster, identical move). The 2021 optimisations cut $M=3$/20-world/52-card time from 3.020 s to **1.032 s**, 32-card from 2.016 s to 0.605 s.

Duplicate 7NT results over 10,000 deals (winrate among *differing* games):

| P1 | P2 | Defence | # differ | P1 winrate | σ |
|---|---|---|---|---|---|
| αμ | WBridge5 | PIMC | 812 | 0.567 | 0.0174 |
| αμ | WBridge5 | WBridge5 | 755 | 0.428 | 0.0180 |
| αμ | WBridge5 | DDS | 567 | **0.697** | 0.0193 |
| αμ | PIMC | PIMC | 757 | 0.551 | 0.0181 |
| αμ | PIMC | WBridge5 | 687 | 0.569 | 0.0189 |
| αμ | PIMC | DDS | 467 | 0.647 | 0.0221 |

Key observations: (i) αμ's advantage is largest against a *clairvoyant* (DDS) defence — exactly the model it assumes; (ii) going from $M=2$ to $M=3$ or $4$ does not help; (iii) **PIMC's winrate curve asymptotes below αμ's** as worlds increase from 20 to 320, and αμ with $M=2$ beats PIMC-with-320-worlds at equal thinking time. (iv) Leaf parallelisation of the DDS calls is trivially safe.

*Related:* Ginsberg's GIB uses the same idea in lattice form (§2.6); Müller's *partial order bounding* and Dasgupta et al.'s vector-valued game tree search are the general theory.

#### (d) IIMC / RecPIMC (Furtak & Buro, CIG 2013)

Replace the perfect-information leaf evaluator with an **actual imperfect-information playout**:

```
IIMC(InfoSet I, int N, Player P):
  for m in Moves(I): val[m] = 0
  for i in 1..N:
      x = Sample(I)
      for m in Moves(I):
          val[m] += FinishedGameValue(x, m, P)
  return argmax_m val[m]

FinishedGameValue(Node x, Move m, Player P):
  y = MakeMove(x, m)
  while y not terminal:
      y = MakeMove(y, ComputeMove(P, I(y)))    # P sees only the infostate
  return value of y from the perspective of the player to move in x
```

Define $R_0 = $ PIMC, $R_1 = $ IIMC using $R_0$ as its player module (= "RecPIMC"), $R_2 = $ IIMC($R_1$), etc. Runtime $\approx C\cdot N\cdot M$ where $N$ = top-level worlds and $M$ = worlds used inside each playout.

**Critical design property:** IIMC does **not leak** private information at the top level, because the playout policies do not adapt across playouts. This is precisely the criticism they level at Information Set MCTS: if you sample worlds consistent with *your* view and let opponents' in-tree strategies adapt across rollouts, the opponents implicitly converge to "knowing" your hand.

**Results on BCD synthetic trees** (height 8, $8\times8$ root chance, exhaustive uniform information sets, exploitability = sum over both seats, max possible 4). At the Hearts-like point $b=0.8$, $corr=0.9$, $dis=0.6$:

| Policy | Best-response exploitability |
|---|---|
| $R_0$ (PIMC) | 0.299 |
| $R_1$ (RecPIMC) | **0.088** |
| $R_2$ | 0.071 |
| $R_3$ | 0.059 |
| $R_5$ | 0.055 |
| $R_8$ | 0.055 |

Almost the whole gain is at the first level of recursion. **Negative result:** at low disambiguation ($dis \le 0.3$, poker-like) $R_1$ is *worse* than $R_0$; the effect vanishes once $dis > 0.3$.

**Results in Skat.** Kermit saturates: its score stops improving beyond ~160 sampled worlds (their Fig. 3 grid, Kermit-vs-Kermit soloist scores; the "Perf" clairvoyant row scores 60–69 vs the 54–55 self-play plateau). Recursive Kermit with 1,600 level-1 and 10–20 level-2 worlds gains up to ≈ +9 soloist points over the 55.19 Kermit-vs-Kermit baseline, and correspondingly better on defence — i.e. **it breaks through PIMC's saturation point, so it can convert hardware into strength where PIMC cannot.**

RXSkat (IIMC using the fast rule-based XSkat as playout module, 160 worlds) vs Bernie (information-set UCT with XSkat playouts, 1,600 playouts):

| Soloist | Defenders | Soloist score |
|---|---|---|
| Bernie | RXSkat (no inference) | 61.34 ± 1.30 |
| RXSkat (no inference) | Bernie | 74.14 ± 1.12 |
| Bernie | RXSkat (inference) | 53.99 ± 1.97 |
| RXSkat (inference) | Bernie | 74.37 ± 1.58 |

RXSkat is ~35× faster than Bernie and worth "over 200 points more in a 36-deal list" — described as roughly the gap between a world-championship player and an average club player.

**Bridge replication (Bouzy, Rimbaud & Ventos, CoG 2020).** With 13 cards/player and no trump, level-1 RMC beats depth-one MC by **+0.5 tricks per deal** (statistically significant). With 5 cards/player, MC3-R-25-25-25 scores +0.20 and MC4-R-20-20-20-20 +0.16 versus MC-D-20 ($\sigma = 0.05$ over 100 card distributions). Increasing the per-level world count above ~40 was *not* beneficial. Confirms the "MC plateaus, recursion breaks the plateau" story, at heavy compute cost (2–5 CPU-hours per 100 deals).

#### (e) EPIMC — postponing the leaf evaluator (Arjonilla, Saffidine & Cazenave, 2024)

Delay the perfect-information evaluator to depth $d$; build a small subgame $U$ of depth $d$ from the sampled trajectories, whose leaves are averaged DDS/rollout scores, and solve $U$ with a method that operates on *infostates* (information-set search, or CFR/CFR+). $d=1$ is PIMC. Exploration strategy: one action per sampling iteration (otherwise $|A|^d$ world evaluations).

Theory: $\mathrm{SF}$ is monotone non-increasing in $d$ (Prop. 1); there always exists $d' $ that strictly reduces it if it is nonzero (Prop. 2); in a finite-horizon game $d' = T-d$ removes it entirely (Prop. 3).

**Empirically decisive negative result for our purposes:** gains are large in games with *private* observations (Dark Chess at 100 s: 80% / 65% / 45% win rate at $d = 3/2/1$; near 100% at 1000 s for Dark Hex and Dark Chess) and **nil in games whose observations are mostly public** (their two-player trick-taking "Card game" and Battleship). The authors state the mechanism explicitly: private observations multiply the number of world states in one infostate and therefore the opportunity for strategy fusion.

### 2.6 GIB — the reference PIMC engineering package (Ginsberg, JAIR 2001)

Five components: partition search, practical Monte Carlo, **achievable sets**, α-β over distributive lattices, and squeaky-wheel optimisation.

**Achievable sets / lattice-valued search.** Restrict to a $\{0,1\}$ game (make the contract or not). Let $S$ be the set of possible distributions of unseen cards. Then the value of the game is a *function* $f: S\to\{0,1\}$, i.e. an element of $2^S$, and max/min extend pointwise:

$$
\min(f,g)(s) = \min\big(f(s), g(s)\big), \qquad \max(f,g)(s) = \max\big(f(s), g(s)\big).
$$

$2^S$ is a distributive **lattice**, not a total order. Generalised game definition: an octuple $(G,V,p_I,s,ev,f_+,f_-)$ with $V = 2^S$ and combination functions $f_+, f_-: \mathcal P(V)\to V$; the value is

$$
ev_c(p) =
\begin{cases}
ev(p), & ev(p)\in V\\
f_+\{ev_c(p') : p' \in s(p)\}, & ev(p)=\max\\
f_-\{ev_c(p') : p' \in s(p)\}, & ev(p)=\min .
\end{cases}
$$

Positions are pairs $(p, Z)$ where $Z \subseteq S$ is the set of situations consistent with the play so far; when the *minimiser* moves, playing card $c$ restricts $Z$ to the subset in which $c$ is legal. This is the direct ancestor of αμ's Pareto fronts and is the mechanism by which GIB can prefer "line D (works regardless of the ♠Q)" over "line C (defer the guess)". **Measured gain: only 0.1 IMPs/deal**, applied to declarer play only, and only because declarer play was already GIB's strongest component.

**Inference by Bayesian re-weighting.** GIB cannot afford to test each hypothetical opponent decision recursively against its own cardplay module. Instead it estimates $\Pr(\text{West holds } \spadesuit K)$: if analysis says that 80% of the time West holds the ♠K it is a mistake not to play it, then West's failure to play it gives 4:1 odds he does not hold it; apply Bayes, fold in defensive signalling conventions, and use the posterior to weight the sample: $\sum_d s(m,d) \to \sum_d w_d\, s(m,d)$.

**Deal construction.** Simplify the auction into per-hand constraints; deal unbiased hands subject to suit-length restrictions; reject hands violating remaining constraints; pass each survivor through the *bidding module* and discard those for which the observed bids would not have been made. Takes 1–2 seconds per decision.

**Engineering:** greedy elimination of moves provably dominated ($\sum_d s(d,m) \le \sum_d s(d,m')$), **iterative broadening** so a low-width answer exists if the high-width search times out, and partition search so that $m_1$ and $m_2$ hit the same transposition entry long before they transpose exactly.

**Results.** Bridge Master (180 declarer-play problems, 5 difficulty levels, 90 s/deal, 50-world sample) — GIB **100/180 = 55.6%** vs Bridge Baron 6 **33/180 = 18.3%**; with 100 s/play, 100 worlds and hand-written bid explanations, 116/180 = 64.4% (each of the three factors contributed roughly equally). Forrester's independent test: 68% at 20 s/play, 74% at 30 s/play. 1998 world-championship invitational (34 of the world's best card players, 12 problems over 2 days, 90 min/deal for humans, ~10 min/deal and 500 worlds for GIB): **GIB led at halfway and finished 12th.** Error-rate-by-trick profiles for GIB and human declarers are strikingly similar. Bidding contests: **+2 IMPs (1998), +9 IMPs (2000)**, narrowly beating the expert field; runner-up Blue Chip Bridge scored −35 and −2. Full-deal α-β with partition search solves a 52-card deal in ≈18,000 nodes (~1 s).

### 2.7 Skat engineering: solvers, evaluation, inference

**Kupferschmid & Helmert (CG 2006).** Double dummy solver for Skat: MTD(0) for exact scores, zero-window α-β for the qualitative "does declarer reach 61?" question, transposition table storing *exact point values or bounds* (not win/loss, because the target threshold shifts as points accumulate), plus:
- **Move ordering** (TT move, killer, domain rules): node reduction **3.45×**.
- **Quasi-symmetry reduction** — rank-equivalence (two cards of the same suit with no unplayed card ranked between them are equivalent), used not only for TT lookups (as in partition search) but to *reduce the branching factor* at interior nodes: **2.38×** node reduction, **2.03×** time.
- **Adversarial heuristics** — admissible lower/upper bounds on the declarer score computed in $O(N)$ by relaxing the move rules, i.e. A*-style heuristics transplanted into a two-player search: **1.80×** nodes, **1.58×** time.

Combined (100,000 random suit games): mean nodes 2,772k → **244k**, mean time 0.84 s → **0.11 s**, median 181k → **7k** nodes, 0.04 s → 0.01 s. Note the enormous standard deviations (8,853k nodes) — the distribution is heavy-tailed, most deals are trivial and a few are murder. A typical Skat hand solves in ~10 ms.

**Buro, Long, Furtak & Sturtevant (IJCAI 2009) — Kermit.** Two contributions.

*(1) Learned imperfect-information state evaluation via GLEM.* Generalised linear model with table-based features:

$$
e(s) = l\Big(\sum_i w_i f_i(s)\Big), \qquad f(s) = T[h_1(s)]\cdots[h_n(s)]
$$

with $l(x)=1/(1+e^{-x})$ (logistic). Skat trump-game "10+2" evaluation:

$$
e^g(h,s,p) \;=\; \frac{1}{1+\exp\!\Big(w_0^g + \sum_{i=1}^{4} w_i^g\, f_i^g(h,s,p)\Big)}
$$

where $f_1,f_2$ score side-suit points/tricks and $f_3,f_4$ trump points/tricks, e.g.

$$
f_1^{\clubsuit}(h,s,p) = \sum_{x\in\{\spadesuit_\star,\heartsuit_\star,\diamondsuit_\star\}} \mathrm{sidePoints}\big[tc(\clubsuit, h\cup s)\big]\big[x(s)\big]\big[x(h)\big]
$$

$$
f_3^{\clubsuit}(h,s,p) = \mathrm{trumpPoints}\big[\clubsuit_\star(h)\big]\big[tt(\clubsuit,(h\cup s)^c)\big]\big[vt(\clubsuit,h)\big]
$$

Index features are bit-operations on 32-bit card sets. Side-suit tables: $2\times128\times128 = 32{,}768$ entries (4,374 used); trump tables $2048\times4\times3 = 24{,}576$. Entries are estimated by scanning millions of human games and crediting/debiting each observed suit configuration with the points, tricks and high cards it won or lost. Bidding is an expectimax over 231 skats × 6 game types × 66 discards ≈ 92,862 leaf evaluations in a fraction of a second; bid iff root value $\ge B = 0.6$.

The crucial insight for us: **DDS values systematically misprice positions where the *opponents' ignorance* is the asset.** Skat null games are almost always DDS losses (the defenders "see" the weakness and exploit it perfectly) yet are often winnable in practice; the human-data-trained evaluator captures this and the DDS evaluator cannot.

*(2) Feature-level inference.* Rather than compute $\Pr(\text{move}\mid\text{world})$ at runtime — intractable, and degenerate (0/1) for a deterministic agent — learn $\Pr(\text{world}\mid\text{move})$ offline over *features* of worlds, assuming feature independence and conditional independence of worlds and moves given the features:

$$
\Pr(\text{world}\mid \text{move}) \;=\; \prod_i \Pr(f_i)\,\Pr(f_i \mid \text{move})
$$

with $\Pr(f_i\mid\text{move})$ a database lookup and $\Pr(f_i)$ a runtime combinatorial correction. As soloist: sample uniformly without replacement from worlds consistent with void-suit history, then weight

$$
\Pr(w_i \mid \mathit{bid}_1, \mathit{bid}_2) \;=\; \Pr\big(h_1(w'_i)\mid \mathit{bid}_1\big)\cdot \Pr\big(h_2(w'_i)\mid \mathit{bid}_2\big),
$$

normalise, and resample for PIMC. As defender, condition on the *announced contract* (more informative than the bid) and marginalise over the soloist's possible discards.

**Kermit results (Fabian–Seeger scoring, points per 36 games, ±std dev):**

| Type | Player A | A pts | Player B | B pts | n |
|---|---|---|---|---|---|
| Cardplay | Kermit(SD) | **996** ± 50 | Kermit(NI, no inference) | 779 ± 54 | 1600 |
| Cardplay | Kermit(SD) | 986 ± 51 | Kermit(S, soloist infer only) | 801 ± 53 | 1600 |
| Cardplay | Kermit(SD) | 861 ± 53 | Kermit(D, defender infer only) | 820 ± 54 | 1600 |
| Cardplay | Kermit(SD) | 1201 ± 48 | XSkat (rule-based) | 519 ± 56 | 1600 |
| Cardplay | Kermit(SD) | 1012 ± 51 | KNNDDSS (uniform PIMC) | 710 ± 53 | 1600 |
| Full game | Kermit(SD) | 1188 ± 30 | XSkat | 629 ± 26 | 4800 |
| Full game | Kermit(SD) | 1031 ± 32 | KNNDDSS | 501 ± 21 | 4800 |

Against world-class humans on a Skat server: Kermit 876 vs Ron Link 898 (408 hands); Kermit 836 vs Eric Luz 739 (2.3k hands) — i.e. expert parity. **Defender-side inference had the biggest single impact** (SD vs S: 986 vs 801), and inference proved robust against a *dissimilar* opponent (XSkat) rather than only against self.

Other Kermit search enhancements: transposition tables, shallow sorting searches near the root, **fastest-cut-first** move ordering (reward moves by β-cutoff potential ÷ estimated subtree size) — 40% search-effort reduction; card-group equivalence forward pruning (sound, with the caveat that current-trick-winning cards must not be treated as already played); pre-computed trick-6/trick-7 endgame tables and ProbCut forward pruning bring the solver to ~68 exact all-move world evaluations/sec at 30 cards to play, or 237 approximate.

**Solinas, Rebstock & Buro (AAAI 2019) — neural card-location inference.** Predict a real $|C|\times l$ matrix $L(h)$ of per-card location probabilities from move history with a fully-connected net (softmax over rows optional), then score worlds by an independence product:

$$
p(s \mid h) \;\propto\; \prod_{c\in C} L(h)_{c,\,\mathrm{loc}(c,s)}
$$

(Skat: $|C|=32$, $l=4$.) Trained on 20M human games; dropout 0.8 on layers 2–4, ADAM, LR $10^{-4}$ with exponential decay 0.96 per $10^7$ batches, batch 32, ELU, early stopping; loss = mean per-card cross-entropy. Practical insight: **predict the full 32-card configuration, not only the unknown cards** — learning is easier. Inputs include played/lead/sloughed cards per opponent, void suits, bid type and bid-magnitude buckets (18–24, 27–36, 40–48, 50–72, >72), soloist, trump, current trick and the padded $32\times24$ cardplay history compressed through 4 layers down to 32 dims.

Metric — **True State Sampling Ratio**:

$$
\mathrm{TSSR} \;=\; \frac{p(s^\*\mid h)}{1/n} \;=\; p(s^\*\mid h)\cdot n
$$

BDCI (with card history) beat both the Kermit baseline and history-free variants, peaking around tricks 3–5 rather than trick 1, with statistically significant tournament gains in null and suit games. Grand games showed no significant gain because human bidding conservatism made them lopsided.

**Rebstock, Solinas, Buro & Sturtevant (CoG 2019) — policy-based inference.** Drop the independence assumption; compute the state's reach probability directly:

$$
\eta(s\mid \mathcal I) \;=\; \prod_{h\cdot a \sqsubseteq s} \pi(h,a)
$$

Chance transitions are analytic; your own actions have probability 1 (you know your policy); the work is estimating the other players' $\pi$, done with neural policies trained on human data rather than with the (intractable) search agent's own policy.

```
EstimateDist(InfoSet I, int k, OppModel pi):
  S = SampleSubset(I, k)
  for s in S:
      eta(s) = 1
      for (h,a) in StateActionHistory(I): eta(s) *= pi(h,a)
  return Normalize(eta)
```

Sampling-corrected TSSR estimator when you cannot enumerate:

$$
\mathrm{TSSR} \;=\; |\mathcal I| \cdot \sum_k \mathrm{Bin}(k;p)\,k\,\eta(s^\*\mid \mathcal I, k), \qquad p = 1/|\mathcal I|
$$

(terms below $10^{-7}$ dropped). PI20/PI100 dominate both card-location and Kermit inference across all game types and roles.

### 2.8 Multi-player search: maxn, paranoid, MP-Mix, multi-player UCT

**maxn (Luckhardt & Irani 1986).** Leaves carry $n$-tuples; at a node where player $i$ moves, back up the child maximising component $i$.

$$
\mathrm{maxn}(v) = \begin{cases}\vec u(v) & v \text{ terminal}\\ \vec u(c^\*) ,\; c^\* = \arg\max_{c\in \mathrm{succ}(v)} \big[\mathrm{maxn}(c)\big]_{p(v)} & \text{otherwise}\end{cases}
$$

Pathologies (Sturtevant, CG 2002 / *Current Challenges*, CG 2004):
- **Lemma 1: changing the tie-breaking rule can arbitrarily change the maxn value of the tree.** Multi-player trees have many equilibria with different values.
- **Theorem 1: every directional algorithm computing the maxn value *or a bound on it* must evaluate every terminal node evaluated by maxn with shallow pruning under the same ordering.** Corollary: **zero-window search is impossible in maxn trees**, which kills partition search and most of the two-player pruning toolkit.
- Korf (AIJ 1991): with no bounds, no pruning is possible; with a lower bound per player and an upper bound on the sum, **immediate**, **shallow**, and (unsoundly) **deep** pruning exist. Shallow pruning's best case reduces $b \to O(\sqrt b)$ *independently of $n$*, which in practice degenerates to the immediate-pruning case.
- Empirically their tie-break of choice: *assume opponents break ties to minimise your score.* Without it, maxn "plays much worse".

**Paranoid (Sturtevant & Korf 2000).** Collapse to two players: all $n-1$ opponents minimise the root player's score. Inherits every two-player technique; best case expands $b^{d(n-1)/n}$ nodes (vs $b^{d/2}$ for two players), so it searches **20–50% deeper** for $n = 3$–$6$. Cost: an unrealistic opponent model; it may walk straight into a loss rather than making opponents work for it.

**Empirical comparison (Sturtevant, CG 2002), 250k nodes/move:**

| Domain | Paranoid | maxn |
|---|---|---|
| Chinese Checkers 3p | 60.6% wins, depth 4.9 | 39.4%, depth 3.1 |
| Chinese Checkers 4p | 59.3%, depth 4.0 | 40.7%, depth 3.2 |
| Chinese Checkers 6p | 58.2%, depth 4.6 | 41.8%, depth 3.85 |
| Chinese Checkers 3p, branching capped at 6 | **71.4%**, depth 8.2 | 28.6%, depth 5.8 |
| Hearts 3p (perfect info) | avg 8.1 pts, depth 15.2 | avg 8.9, depth 11.0 |
| Hearts 4p (perfect info) | 6.45, depth 14.3 | 6.55, depth 11.2 |
| Spades (perfect info) | 5.67, depth 15.4 | 5.67, depth 10.6 |

Summary: paranoid dominates in Chinese Checkers, edges maxn in Hearts, **ties in Spades**. Note the *huge* depth advantage in card games (15 vs 11 ply) buys almost nothing — a warning that in trick-like games extra search depth has very low marginal value compared with better evaluation/inference.

**Opponent modelling in multi-player games (Sturtevant, CG 2004).** Because there is no single "optimal" opponent in a multi-player game, an opponent model is unavoidable — and a *wrong* one is expensive.

| Spades | max-tricks player | min-overtricks player |
|---|---|---|
| $MT_m$ vs $mOT_m$ (both model correctly) | 139.0 / 47.0% wins | 149.2 / 53.0% |
| $MT_m$ vs $mOT$ (mOT unmodelled) | 145.0 / **65.0%** | 78.0 / 35.0% |
| $MT$ vs $mOT_m$ | 144.9 / 49.3% | 144.1 / 50.7% |
| $MT$ vs $mOT$ | 147.9 / 63.2% | 95.8 / 36.8% |

| Hearts | minimise-score | maximise-lead |
|---|---|---|
| $mS_m$ vs $ML_m$ | 86.6 / 45.5% wins | 92.0 / **54.5%** |
| $mS$ vs $ML_m$ | 86.5 / 41.9% | 89.3 / **58.1%** |
| $mS_m$ vs $ML$ | 72.7 / **54.5%** | 94.4 / 46.5% |

Also from Carmel & Markovitch (Checkers) via Sturtevant: opponent-modelling methods beat plain search at **equal depth** but lose at **equal node budget**, because modelling destroys pruning. In multi-player games, where pruning is weak anyway, this trade-off is much more favourable to modelling.

**MP-Mix (Zuckerman, Felner & Kraus, IJCAI 2009).** Switch propagation strategy per turn:

```
MP-Mix(T_d, T_o):
  for i in Players: H[i] = evaluate(i)
  sort(H) descending
  leadingEdge = H[1] - H[2];  leader = argmax H
  if leader == root player:
      if leadingEdge >= T_d: return Paranoid(...)
  else:
      if leadingEdge >= T_o: return Offensive(...)   # directed at the leader
  return MaxN(...)
```

Directed-offensive backs up, at the root, the child *minimising the target opponent's* evaluation while other players still play maxn on their own levels. Setting $T_d, T_o$ above the heuristic's maximum recovers pure maxn.

*Hearts results:* with 3 fixed players (PAR, PAR, MAXN) and search depth 6, the MIX fourth player's win-rate advantage over the best of the other three grows nearly linearly in $T_d$, peaking at $T_d = 25$ with **+6%**, vs **−5%** for a MAXN fourth player and **−11%** for PAR ($p<0.05$). In a randomised 5-player-type pool with 20k node budgets over 1,200 tournaments: MIX 27%, EvilPixie 20%, MAXN 19%, PAR 17%, Yakool 15%, Angry 2%.

***Opponent Impact* — directly relevant to Fish.**

> **Definition (Influential state).** A state for player $A$ w.r.t. $B$ is *influential* if there exists an action $\alpha$ available to $A$ that reduces $B$'s heuristic evaluation.
>
> **Definition (Opponent Impact).** $\displaystyle OI(G,H) = \frac{|\mathrm{InfluentialStates}(G,H)|}{|\mathrm{TotalStates}(G,H)|}$.

The paper's own worked examples: **Bingo has $OI = 0$**; **Go Fish has $OI = 1$** — "at any given state the player can decide to impact a player's well being by asking him for a card"; Hearts has a small $OI$ ("a player's ability to directly hurt a specific player is considerably limited"); Backgammon is positive during contact and zero during the race. MP-Mix's advantage was much larger in Risk (high $OI$) than in Hearts (low $OI$).

**Multi-player UCT (Sturtevant, ICGA 2008).** Theorem: **UCT computes an equilibrium strategy in a multi-player game tree, which may be mixed**, whereas maxn computes a pure-strategy equilibrium. Selection rule:

$$
a^\* = \arg\max_a \Big[ \bar X_{i}(a) + C\sqrt{\tfrac{\ln N(s)}{N(s,a)}} \Big]
$$

with $\bar X_i$ the $i$-th component for the player to move. Empirics in Hearts: UCT with 50,000 playouts is the best *anti-shooting* algorithm (opponent shot the moon only 250/3,244 times, vs 1,377 for simple maxn and 411 for a random player); asymmetric exploration ($C=0$ for the root player, $C=0.4$ for others) beat any single $C$. But in full-game quality, **learned linear evaluators beat UCT by ~20 points** (UCT 46.12 vs learned 67.30 — lower is better, so UCT wins that pairing; against a hand-tuned player UCT 51.77 vs 88.31). Doubling playouts is worth only 4–5 points. **RAVE and the history heuristic gave no benefit in Hearts**, even with move-context bucketing (lead / follow / slough) — card values are entirely context-dependent. Playout policy matters enormously and counter-intuitively: a *stronger* rule-based playout policy performed *worse* than random playouts; the only rule that helped was "dump the ♠Q on another player when possible".

---

## 3. Consolidated empirical picture

- **PIMC is a strong baseline in high-$df$, high-$lc$ games and is close to a Nash payoff there** — but it is *exploitable*, and its measured loss vs. equilibrium is only a floor on its loss vs. a modeller (Kuhn poker: −0.056 vs Nash, −0.083 vs best response as p1; +0.056 vs Nash, −0.166 vs BR as p2).
- **Width saturates; depth/recursion/value-structure does not.** GIB: 50 worlds normally, "little or no improvement" above 500. Kermit: saturates at ~160. αμ: PIMC's winrate curve asymptotes below αμ's. Bouzy: MC plateaus, RMC does not.
- **Ordering of repair value/effort:** better inference (huge, Kermit +217 pts/36) > prm-style payoff reduction (66% → 96% correct on Bridge suit combinations, ~1.5× slowdown) > IIMC one level of recursion (exploitability 0.299 → 0.088; but runtime $\propto N\cdot M$) > αμ at $M=2$ (+1–3 points, ~4× PIMC time with all cuts) > lattice/achievable-set declarer play (+0.1 IMP/deal) > extra worlds (≈0 past saturation).
- **Solver engineering is worth an order of magnitude, not a constant.** K&H: 11.4× fewer nodes, 7.6× less time from three enhancements; Kermit's fastest-cut-first alone 40%.
- **Negative results worth remembering:** IIMC is *worse* than PIMC at $dis \le 0.3$; EPIMC gives *nothing* in public-observation games; RAVE/history heuristic give nothing in Hearts; stronger playout policies can make UCT weaker; opponent modelling loses to plain search at equal node budgets in two-player games; paranoid's 4-ply depth advantage in Hearts/Spades buys ~0.8 points or nothing.

---

## 4. Canadian Fish: structural diagnosis

Before the per-technique analysis, the facts that drive everything.

**F1. The only hidden variable is the initial deal, and every subsequent event is public.** The information set is exactly the set of assignments of the still-unrevealed cards to the five other players, subject to: exact per-player hand sizes (public), plus hard *possession* and *void* constraints generated by every ask.

**F2. Each ask is a hard logical constraint generator, not a soft signal.** An ask by $P$ for card $c$ of half-suit $H$ from $Q$ implies, deterministically:
- $P$ holds at least one card of $H\setminus\{c\}$;
- $P$ does **not** hold $c$;
- if the answer is *yes*: $c$'s location becomes public forever;
- if *no*: $Q$ does not hold $c$ at that instant (and, because $c$ can only move by being asked, $Q$ never holds $c$ again unless $Q$ later asks for it).

This is far sharper than void-suit tracking in Bridge/Skat. Consequently $df$ in Fish should be **much higher than the ≈0.6 measured in Skat/Hearts**, which is nominally the *best* regime for PIMC.

**F3. But the perfect-information game is nearly degenerate.** In a determinized world, the player to move knows exactly who holds every card, so:
- they never make a failing ask (they only ask cards the target actually holds), hence **never lose the turn**;
- they declare every half-suit the instant their team completes it, always correctly.

So the double-dummy value of almost every Fish position is "the team on turn farms everything it can reach". The DDS is *fast and useless*: it barely discriminates between the moves at the root.

**F4. Therefore plain PIMC in Fish collapses to a one-ply greedy heuristic.** Averaging $V^{PI}$ over worlds, the value of asking $c$ from $j$ is approximately

$$
\mathbb{E}\big[V^{PI}\big] \;\approx\; \Pr(j \text{ holds } c \mid h)\cdot V_{\text{sweep}} \;+\; \big(1-\Pr(j\text{ holds }c\mid h)\big)\cdot V_{\text{opp sweep}}
$$

i.e. $\arg\max_m \mathbb E[V^{PI}] \approx \arg\max_{(j,c)} \Pr(j \text{ holds } c\mid h)$. That is a *coherent* heuristic (it is roughly what beginners do), but it is blind to everything that actually matters in Fish: the information you leak by asking, the information you gain from a "no", the option value of waiting to declare, and partner signalling.

**F5. In BCD terms Fish sits in a regime the 2010 paper never measured.** $df$ very high (good), $lc$ probably high near the end (most half-suits are decided) — but the *effective* leaf correlation from a PIMC agent's viewpoint is degenerate in the other direction: at the pre-terminal level almost every move is a win in the sampled world, which is the $b\to1$, $lc\to1$ corner where "even the random player will play perfectly" and PIMC gains **nothing over random**. Long et al. explicitly note this corner ("we can only suppose these would be very boring games in real life"). The reason Fish is *not* boring in real life is precisely the imperfect information that PIMC discards.

**F6. Information-set size.** From one seat at deal time,

$$
|\mathcal I_0| = \frac{45!}{(9!)^5} \approx 1.90\times10^{28},
$$

versus $\approx 8.45\times10^{16}$ for a Bridge hand ($39!/(13!)^3$) and $\binom{22}{10}\binom{12}{10} = 42{,}678{,}636$ for a Skat defender. Total deal space $54!/(9!)^6 \approx 10^{38}$, matching the project's ~$10^{40}$ estimate. Enumerate-and-weight is out; you need a *constrained sampler*.

**F7. Team structure simplifies the search but complicates the equilibrium.** Both teams have three players with identical payoffs, so the *team-level* perfect-information game is 2-player constant-sum (9 sets). No maxn/paranoid needed inside a determinization — plain minimax/αβ on team score suffices. What is *not* simple is that the three teammates have different information sets and no communication channel; the correct solution concept is a **team-maxmin equilibrium with imperfect recall at the team level**, strictly harder than 2p0s, and PIMC silently assumes perfect intra-team knowledge sharing (a second, teammate-directed flavour of strategy fusion). Buro et al. flag exactly this for Skat defenders.

**F8. Declarations are interrupt moves.** "Any player may declare at any time, including during an opponent's turn" breaks the alternating-move assumption of every algorithm above. You need to model declaration either as an option set attached to each decision point, or as an explicit public "declare / pass" micro-phase after each ask resolution.

---

## 5. Applicability of each technique to Canadian Fish

### 5.1 Plain PIMC with a Fish double-dummy solver — **do not build this as the main engine**

*Would it help?* As a **baseline and as an inference test-harness**, yes. As the move-selection engine, no: by F3/F4 it degenerates into "maximise ask success probability" and throws away the entire information game.

*Compute.* The Fish DDS is cheap: under perfect information a turn is a near-forced sweep, so the solver terminates almost immediately. Move generation is the expensive part — up to $3 \times |\{c : c \notin \mathrm{hand}, \exists c'\in \mathrm{hand} \cap H(c)\}|$ asks, up to ~90 at the start, plus a combinatorially large declaration action set. Budget: microseconds per world once the sweep logic is written; the cost is entirely in sampling worlds (F6).

*Pitfalls.* (i) It will never ask a "probe" card whose value is the *information* the answer carries. (ii) It will systematically over-value its own position, exactly like GIB's finesse example — which will corrupt any downstream "should I declare?" threshold. (iii) It will assume teammates know what you know.

*Adaptation.* Use it only to produce $\Pr(\text{ask succeeds})$ features and as a fast lower-bound evaluator inside a better search. Two mandatory changes if you do use it: **(a)** replace the DDS with an evaluator that respects opponent ignorance (§5.4); **(b)** score by *sets won* over the whole deal, not by the immediate ask outcome.

### 5.2 The BCD framework — **build the measurement harness first; it is cheap and decisive**

*Would it help?* Yes, immediately, as project instrumentation. Implement exactly the recipe in §2.4 for Fish:
- Binarize payoff: $+1$ if your team ends with $\ge5$ of 9 sets, $-1$ otherwise.
- Random playouts from a random legal Fish position; identify pre-terminal nodes (all moves lead directly to terminal); $\widehat{lc}$ = fraction correlated; $\widehat b$ = fraction of correlated pre-terminals that are team-A wins.
- Track $|\mathcal I_t|$ (or a log-estimate; see §5.6) between your own consecutive moves, average the ratio to get $\rho$, and convert with $df = 2(1-\rho)/(2-\rho)$.
- Suggested budget mirroring the paper: 10,000 deals with 1,000 leaf measurements each for $lc,b$; 10 rollouts/world for $df$.
- Also measure Frank/Basin's **world-similarity $q$** by their overwrite construction, or its empirical analogue: the fraction of world pairs for which the optimal root move coincides.

*What to do with the answer.* If $\widehat{lc}\to1$ and $\widehat b \to$ extreme, you are in the "even random plays perfectly" corner and PIMC will show almost no gain over random — which is the signature that all the value in Fish is in inference and declaration timing, not in lookahead. Also compute $\widehat{lc}$ **restricted to the ask-decision only** (holding declaration policy fixed) versus **restricted to the declare-decision only**; my expectation is that declaration decisions carry nearly all the leaf anti-correlation.

*Cost.* Hours of engineering, minutes of CPU. Highest information-per-effort item in this report.

### 5.3 Vector minimaxing + payoff-reduction minimaxing (prm) — **the highest-ROI repair; implement this**

*Why it fits Fish exceptionally well.* Fish payoffs are naturally low-cardinality per half-suit (won / lost by your team), and the "best defence" assumption — opponents know your hand — is *the* right conservative model for a game where every ask you make publicly narrows your hand. The MAX rule "commit to one move across all sampled worlds" directly kills F4's degeneracy: you can no longer pretend that you will ask the right card in every world; you must pick one $(j,c)$ pair and eat the failures.

*Concrete adaptation.*
- Sample $D = \{w_1,\dots,w_n\}$, $n \approx 20$–$100$, from the constrained belief (§5.6).
- Leaf value $\vec P(v)[j] \in \{0,1\}$ = "my team won $\ge5$ sets in world $w_j$", or better, a real-valued $\vec P[j] = (\text{my sets} - \text{their sets})/9$ so the vector operators still work.
- MAX nodes = my team's decision points (asks and declarations by me or my teammates — see the teammate caveat below). MIN nodes = opponent decision points, evaluated **componentwise minimum** (they know my hand).
- prm's reduction step: run a *per-world* minimax first (cheap in Fish, by F3), record each MIN node's per-world value $m_k$, and clip each leaf: $\tilde P[k](v) = \min(P[k](v), \min_{u\in\text{MINanc}(v)} m_k(u))$.

*Cost.* $O(|T| \cdot n)$ where $|T|$ is the searched tree size and $n$ the number of worlds — the same asymptotics as PIMC with a ~1.5–1.7× constant, per the CG-98 timings. Very affordable.

*Pitfalls.* (i) prm's reduction is defined for a *two-player* MIN/MAX tree; in Fish you must decide whether teammates are MAX nodes (they share your payoff but not your information — treating them as MAX is another strategy fusion, see §5.7). (ii) prm *introduces* errors when the opponent has **no** knowledge (their Fig. 8 shows prm worst at MIN-knowledge 0 and best at MIN-knowledge 1). In Fish, opponents genuinely know a great deal, so this is the favourable regime — but verify with the $\rho$/$df$ measurement. (iii) It is still a heuristic: optimal play against best defence is NP-complete.

### 5.4 IIMC / RecPIMC — **the right way to make the evaluator respect ignorance**

*Why it fits.* IIMC's whole point is to replace $V^{PI}$ with "what actually happens when real imperfect-information agents finish this game". In Fish that is decisive, because $V^{PI}$ is degenerate (F3). An IIMC playout in Fish *will* fail asks, *will* mis-declare, *will* leak information — so the value of an ask reflects its real cost/benefit including information leakage. This is the mechanism that lets a Fish agent discover probing asks and declaration timing without hand-coding them.

*Concrete adaptation.*
- $R_0$ = a fast rule-based Fish policy (the XSkat role): "ask the card with the highest posterior in the half-suit where I hold most; declare when posterior of the exact allocation exceeds $\tau$".
- $R_1$ = IIMC with $R_0$ playouts. Runtime $\approx C\cdot N\cdot M$; with a rule-based $R_0$, $M$ is not a world count at all — just make $R_0$ deterministic-given-belief and cheap.
- Do **not** let the playout module see the sampled world. Furtak & Buro's leakage argument applies with full force in Fish, where hand knowledge is nearly the whole game.

*Cost.* With ~90 root moves and $N$ worlds, one IIMC iteration costs $90 \times$ (one full Fish playout, ~60–120 decisions). A rule-based playout at ~1 µs/decision gives ~10 ms per world; $N = 200$ ⇒ ~2 s/move on one core, trivially parallelised over worlds. This is affordable and is my recommended primary engine.

*Pitfalls.* (i) The measured **negative result at low disambiguation does not apply** to Fish (its $df$ is high), but verify. (ii) One level of recursion captures ~90% of the benefit; do not build $R_2+$. (iii) IIMC's move ranking is only as good as $R_0$; a badly-calibrated declaration threshold in $R_0$ will systematically bias every evaluation. Tune $\tau$ by self-play before trusting the search.

### 5.5 αμ / Pareto fronts — **worth building if you want the strongest declaration and probe reasoning**

*Why it fits.* Fish's outcome per half-suit is exactly binary per world, so αμ's $\{0,1\}^n$ vectors are a native representation: $x[j] = 1$ iff "my team ends up owning this half-suit in world $j$". The Max-node union-of-fronts is precisely how you keep alive the option "I have *two* different ways to secure this half-suit depending on what I learn", which is the essence of a probing ask. The Min-node Pareto product is how you avoid the non-locality trap of committing to a line that a knowing opponent will simply pre-empt by declaring first.

*Concrete adaptation.*
- $M$ counts *your team's* decisions. Fish has no "trick" structure so the Min-skipping rule ("recursive calls at Min nodes do not decrement $M$") transfers directly *only if* your leaf evaluator is world-exact; with a heuristic evaluator you must decrement.
- The "possible worlds" bitmask maintenance maps perfectly onto Fish: after any opponent ask, the set of consistent worlds shrinks by the F2 constraints — implement `W1 = {w in Worlds : move legal in w}` as a bitset AND with a precomputed per-(player, card) mask.
- Implement the four cheap wins first: transposition table on (public history hash), root cut, early cut, and the useful/useless-world tracking from the 2021 paper — together worth ~15× and then a further ~3×.
- Equivalent-move detection (Ginsberg's partition search idea): in Fish, two cards of the same half-suit that are *both* unlocated and symmetric in every consistent world are equivalent asks — normalise them away. This is a large branching-factor reduction.

*Cost.* Pareto fronts can blow up: the Min-node product is $\prod_k |f_k|$ before reduction. With $n=20$ worlds and $M=2$, Bridge measured ~0.36 s/move fully optimised. Fish has a larger branching factor, so expect 1–5 s/move at $M=2$, $n=20$. Cap front size (keep top-$K$ by $\mu$) if it explodes — this makes it heuristic but bounded.

*Pitfalls.* (i) αμ assumes **Min has perfect information**; in Fish that means "opponents know your entire hand", which will make you excessively cagey about asking. The authors list "take into account that the defense only has incomplete information" as future work — you may need to soften it (e.g. sample opponent beliefs). (ii) $M>2$ did not help in Bridge; do not chase depth. (iii) The union-at-Max rule assumes a *single* Max player. With three teammates who cannot communicate, a union-of-fronts across teammates over-claims coordination.

### 5.6 Inference / world sampling — **where most of the strength will come from**

Every result in §3 says the same thing: inference beats search. In Fish this is even more true because the constraints are hard and abundant (F2).

*The sampling problem.* You need samples from the uniform-or-weighted distribution over assignments of $U$ unrevealed cards to 5 players with exact capacities $n_1..n_5$ and a forbidden-pair matrix $F$ ($F_{jc} = 1$ if player $j$ provably cannot hold card $c$). This is sampling a 0-1 bipartite assignment with margins — equivalent to sampling from a permanent, #P-hard exactly, but with excellent practical algorithms:
- **Sequential importance sampling with Sinkhorn scaling.** Scale the $|U|\times5$ availability matrix to doubly-stochastic-with-margins, deal cards one at a time proportional to scaled entries, carry the importance weight. Gives unbiased weighted samples and a permanent estimate (hence $|\mathcal I_t|$, which you need for the $df$ measurement and for TSSR).
- **Gibbs / swap MCMC.** Start from any feasible assignment (found greedily or by Hall's-theorem matching), then repeatedly propose swapping two cards between two hands; accept if feasible (and by Metropolis if you have a non-uniform target). Cheap, and easy to make respect a learned prior.
- Do **not** use rejection sampling from unconstrained deals — with $|\mathcal I_0|\approx10^{28}$ and dozens of hard constraints, the acceptance rate collapses.

*The weighting problem.* Beyond hard constraints, weight worlds by how likely the observed *action sequence* is:
- **Policy-based (best; Rebstock et al. 2019):** $\eta(s\mid\mathcal I)=\prod_{h\cdot a\sqsubseteq s}\pi(h,a)$, with $\pi$ a learned or rule-based opponent model. In Fish this is unusually tractable because the action space per turn is small and interpretable: "who did they ask, for what, why not the obvious alternative?" A player who asks for the $9\heartsuit$ from seat 4 rather than seat 2 is telling you something about their model of seat 2 — and their choice of *half-suit* tells you what they hold.
- **Individual-card network (Solinas et al. 2019):** $p(s\mid h)\propto \prod_{c} L(h)_{c,\mathrm{loc}(c,s)}$ with $L$ a net predicting per-card location. In Fish, $|C| = 54$, $l = 6$; predict the **full 54-card, 6-location configuration** (their "use full targets" insight), and feed the padded ask/answer history. Combine multiplicatively with the hard constraints (zero out impossible entries, renormalise rows).
- **Table/count-based (Kermit-style):** the cheap fallback — per-half-suit count histograms conditioned on "player $j$ has asked $k$ times in half-suit $H$".

*Instrumentation.* Adopt **TSSR** as your inference KPI:
$$
\mathrm{TSSR} = p(s^\*\mid h)\cdot|\mathcal I|,\qquad\text{or, under sampling,}\quad |\mathcal I|\sum_k \mathrm{Bin}(k;1/|\mathcal I|)\,k\,\eta(s^\*\mid \mathcal I,k).
$$
Plot it by ask-number and by seat. Because Fish's constraints are hard, expect much higher TSSR than Skat's — and expect the *marginal* value of a learned model to be concentrated in the early game before the constraints bite.

*Cost.* Sampler: microseconds per world with Gibbs after warm-up. Neural inference: one forward pass per decision (not per world) to produce $L(h)$, then $O(|U|)$ per world to score. Entirely affordable.

*Pitfall.* The independence assumption in $\prod_c L(h)_{c,\mathrm{loc}}$ is *wrong in Fish in a specific, exploitable way*: knowing that player $j$ asked in half-suit $H$ makes the presence of *any* $H$ card in $j$'s hand strongly correlated with the presence of others. Rebstock et al. flagged exactly this failure for Skat jacks. Either model per-half-suit *counts* jointly (a 6-way multinomial per half-suit per player) or use the policy-based formulation which has no independence assumption.

### 5.7 Multi-player search machinery — **mostly skip; keep two ideas**

*Skip:* maxn, paranoid, MP-Mix's leader targeting, speculative pruning. Fish is two teams with identical intra-team payoffs, hence a 2-player constant-sum game at the team level; use plain minimax/αβ inside determinizations. Sturtevant's maxn tie-breaking and zero-window impossibility results do not bite. (And note his empirical finding that in Hearts/Spades a 4-ply depth advantage bought ~0.8 points or nothing — depth is not where Fish's value lives either.)

*Keep #1 — Opponent Impact.* Zuckerman et al. put **Go Fish at $OI=1$**: you can damage any chosen opponent at essentially every state. In a 2-team game this becomes *target selection*: which of the three opponents to ask matters as much as which card. Expect a large fraction of your agent's edge to come from choosing the seat, because (a) it maximises $\Pr(\text{success})$, (b) it minimises information given to the most dangerous opponent, and (c) it manages hand sizes (draining a player toward zero cards removes them from the game and changes turn dynamics). Build the seat choice as a first-class argmax over $(j,c)$ pairs, never as a tie-break.

*Keep #2 — opponent modelling is unavoidable and asymmetric.* Sturtevant's Spades/Hearts tables show that a *correct* model is worth 10–15% win rate and an *incorrect* one costs 15–30%. In Fish, your teammates' policies are as important to model as your opponents' — you must predict what your partner will infer from your ask in order to signal, and predict what they hold in order to declare. Model teammates and opponents with the *same* machinery but *different* priors (teammates are cooperating; opponents are not).

*The teammate strategy-fusion trap.* Whatever search you build, a determinized solver will happily plan lines that require a teammate to make a move only justified by knowledge they do not have. Mitigation: in playouts (IIMC) and in vector/Pareto search, force teammates to act through the same *information-set-restricted* policy $R_0$ you use for opponents, i.e. teammates are simulated as agents with their own beliefs, not as MAX nodes with your knowledge. This is the single most important non-obvious implementation detail for a 3-person-team game.

### 5.8 EPIMC (postponing the leaf evaluator) — **predicted not to help; deprioritise**

Fish is a fully-public-observation game (F1). Arjonilla et al.'s clearest empirical finding is that postponing the leaf evaluator gives large gains only when observations are *private* and gives **no measurable gain** in their public-observation benchmarks (a two-player trick-taking card game and Battleship). Their stated mechanism — private observations multiply worlds per infostate — does not apply. Cost of trying it anyway is low (it is $d$-deep sampling plus an information-set-search of a tiny subgame), so it is a fine experiment, but not a priority.

Counter-argument worth testing: Fish's degeneracy (F3) is not caused by private observations but by the *triviality of the perfect-information game*. Postponing the DDS by $d$ asks means the first $d$ asks are evaluated by an infostate-level solver, which is exactly the fix. So EPIMC-with-$d\ge2$ may help in Fish *for a different reason than the paper's* — and if you run it, use a good playout evaluator, not the near-constant Fish DDS, at the leaves. Treat this as a speculative experiment, clearly labelled.

### 5.9 Solver / search engineering to steal wholesale

- **Equivalent-move (partition/quasi-symmetry) reduction.** In Fish, unlocated cards within a half-suit that are symmetric across all consistent worlds are interchangeable asks. Also, the *jokers plus four 8s* half-suit has heavy internal symmetry. K&H got 2.38× node reduction from rank equivalence in Skat; expect more in Fish, where card *rank* has no intrinsic value at all — only half-suit membership and location matter, so equivalence classes are large. **This is likely the largest single search speedup available.**
- **Fastest-cut-first** move ordering (reward by cutoff potential ÷ estimated subtree size): 40% effort reduction in Kermit.
- **Transposition table keyed on the public history's *induced constraint set*,** not on the move sequence — many ask orders yield identical public knowledge.
- **Iterative broadening** (GIB): guarantee a low-width answer if the wide search times out.
- **Greedy move elimination**: drop move $m$ once $\sum_d s(d,m) \le \sum_d s(d,m')$ is provable.
- **Root-level bandit budget allocation** (UCB/OCBA over $(j,c)$ pairs) instead of evaluating all ~90 moves in every world — Buro et al. explicitly recommend this and it is free.
- **Leaf parallelisation** of world evaluations (Cazenave et al.): embarrassingly parallel and safe.
- **Endgame tables.** Skat used trick-6/trick-7 tables. Fish's endgame is small once ≤2 half-suits remain: precompute or solve exactly and switch to an exact information-set solver (or CFR on the residual game, as Long et al. did for Skat's last three tricks) when $|\mathcal I|$ drops below ~$10^5$.

### 5.10 The declaration module — **treat as a decision-theoretic problem, not a search problem**

Nothing in the PIMC literature covers a "declare with exact allocation, or lose the set" mechanic. The closest analogue is Skat's *null* game observation from Buro et al.: DDS assigns near-certain loss because it assumes clairvoyant defenders, whereas the true winning chance depends on the *defenders' ignorance* — and only a human-data-trained evaluator captured that. The Fish parallel is exact: a DDS thinks declaring is always safe (it knows the allocation) and thinks opponents will always snipe first (they know too).

Recommended formulation. Let $H$ be a half-suit your team might own, $A$ a candidate card→teammate allocation, $\mathcal A_H$ the set of allocations consistent with the public history. Declare now iff

$$
\Pr(A \mid h)\;\cdot\;1 \;+\;\big(1-\Pr(A\mid h)\big)\cdot 0
\;>\;
\mathbb E\Big[\text{value of waiting}\Big]
$$

where the right-hand side must account for (i) the chance an opponent completes and declares $H$ first, (ii) the chance you learn more (each future ask in $H$ sharpens $\Pr$), and (iii) the chance a teammate's cards get pulled out from under you. Estimate $\Pr(A\mid h) = \sum_{w:\,A(w)=A} \Pr(w\mid h)$ directly from your world sampler — this is a pure inference query, needs no search, and is where the sampler's calibration is tested. Calibrate the threshold by self-play; report a reliability diagram (predicted $\Pr$ vs realised success) as a first-class KPI.

---

## 6. Pitfalls, negative results, and failure modes

1. **PIMC never gathers information** (Ginsberg): it defers rather than resolves. In Fish, where the entire game is information gathering, this is disqualifying for the main engine.
2. **Strategy fusion is unbounded in sample size** (Frank & Basin): more worlds never fixes it. Do not respond to weak play by raising $N$.
3. **Non-locality is harder to remove than fusion** (Frank/Basin/Matsubara): vector minimaxing (which removes fusion alone) improved Bridge accuracy only 66.3% → 71.1%; prm (which also attacks non-locality) got 95.8%.
4. **PIMC is exploitable even where it is near-Nash.** Kuhn poker: PIMC as p2 earns +0.056 against Nash but −0.166 against a best responder. Fish opponents *will* model a deterministic bot's ask policy — randomise among near-equal asks.
5. **prm can be worse than MC when the opponent is ignorant.** Its advantage appears above MIN-knowledge ≈ 5/9 in their experiments. Measure Fish opponents' effective knowledge before committing.
6. **IIMC is worse than PIMC at low disambiguation** ($dis \le 0.3$). The mechanism is not fully understood even by the authors ("the precise reason for this is unclear").
7. **EPIMC gives nothing in public-observation games.** Directly relevant.
8. **ISMCTS leaks private information.** Sampling worlds consistent with your own view while letting opponents' in-tree policies adapt across rollouts makes them converge to "knowing" your hand. Furtak & Buro's critique; Solinas et al. repeat it. In Fish, where hand knowledge is nearly everything, this bias would be severe.
9. **RAVE and the history heuristic do not work in card games** (Sturtevant, Hearts) — card value is entirely context-dependent. Same will hold in Fish, where the *same* ask is brilliant or terrible depending on hand composition.
10. **Stronger playout policies can make MCTS weaker** (Sturtevant: a sensible novice Hearts policy was worse than random playouts). Test playout policies empirically; do not assume.
11. **Pre-initialising search values from a learned evaluator helps at low budget and *hurts* at high budget** (Sturtevant, Hearts, statistically significant).
12. **Opponent modelling loses at equal node budgets in two-player games** (Carmel & Markovitch) because it destroys pruning — but this is much less binding in games with weak pruning, which includes Fish.
13. **Wrong opponent models are expensive.** Spades: the min-overtricks player went from 53.0% wins (correct model) to 35.0% (no model). Hearts: max-lead player 58.1% → 46.5%.
14. **Search-depth advantage does not translate to card-game strength.** Paranoid searched 15.2 ply vs maxn's 11.0 in Hearts and gained 0.8 points; in Spades, 15.4 vs 10.6 for a dead tie.
15. **Node-count distributions are heavy-tailed.** K&H: mean 2,772k nodes, median 181k, std dev 8,853k. Any per-move time budget must be an anytime budget, not a node budget.
16. **Optimal play against best defence is NP-complete** in tree size (Frank & Basin, CG 1998). Everything above is a heuristic; do not promise optimality.
17. **Determinization has no soundness guarantees.** Arjonilla et al. describe determinization methods as "offering scalability but lacking theoretical guarantees"; the sound-search literature (Šustr et al., *Sound Algorithms in Imperfect Information Games*, AAMAS 2021) formalises what determinization gives up. *(I located this paper but did not read the primary text — treat the characterisation as second-hand.)*
18. **Fish-specific: the DDS is a trap.** Because the perfect-information Fish game is near-trivial, a fast, correct double-dummy solver will be easy to build, will run beautifully, and will tell you almost nothing. Do not mistake solver throughput for progress.
19. **Fish-specific: intra-team strategy fusion.** A determinized or vector search that treats teammates as MAX nodes with your knowledge will plan uncommunicable coordination. Simulate teammates as belief-limited agents.
20. **Fish-specific: interrupt declarations break the alternating-move assumption** of maxn, paranoid, αμ's Min-skipping rule, and standard αβ. Model them explicitly.

---

## 7. Bibliography

Primary sources I fetched and read in full or in substantial part are marked **[read]**; sources I located and whose claims I take from abstracts/citations are marked **[located]**; anything I could not verify directly is marked **UNVERIFIED**.

1. **[read]** Jeffrey Long, Nathan R. Sturtevant, Michael Buro, Timothy Furtak. *Understanding the Success of Perfect Information Monte Carlo Sampling in Game Tree Search.* AAAI-10, pp. 134–140, 2010. https://ojs.aaai.org/index.php/AAAI/article/view/7562 · PDF: https://webdocs.cs.ualberta.ca/~nathanst/papers/pimc.pdf
2. **[read]** Matthew L. Ginsberg. *GIB: Imperfect Information in a Computationally Challenging Game.* Journal of Artificial Intelligence Research 14:303–358, 2001. https://www.jair.org/index.php/jair/article/view/10279
3. **[read]** Ian Frank, David Basin, Hitoshi Matsubara. *Finding Optimal Strategies for Imperfect Information Games.* AAAI-98, pp. 500–507, 1998. https://cdn.aaai.org/AAAI/1998/AAAI98-071.pdf
4. **[located]** Ian Frank, David Basin. *Search in games with incomplete information: a case study using Bridge card play.* Artificial Intelligence 100(1–2):87–123, 1998. https://www.sciencedirect.com/science/article/pii/S0004370297000829 — origin of "strategy fusion" and "non-locality"; I read its content as reported verbatim in [1], [3], [10], [11], [22].
5. **[located]** Ian Frank, David Basin. *Optimal Play against Best Defence: Complexity and Heuristics.* Computers and Games (CG 1998), LNCS 1558, Springer. https://link.springer.com/chapter/10.1007/3-540-48957-6_4 — NP-completeness of optimal play in the best defence model; two new heuristics beating prior algorithms and, on a hard Bridge set, the human expert solutions.
6. **UNVERIFIED** Ian Frank, David Basin. *A theoretical and empirical investigation of search in imperfect information games.* Theoretical Computer Science 252(1–2):217–256, 2001. (Cited as reference [6] in Cazenave & Ventos 2019; I did not obtain the text.)
7. **[read]** Michael Buro, Jeffrey R. Long, Timothy Furtak, Nathan R. Sturtevant. *Improving State Evaluation, Inference, and Search in Trick-Based Card Games.* IJCAI-09, pp. 1407–1413, 2009. https://www.ijcai.org/Proceedings/09/Papers/236.pdf
8. **[read]** Timothy Furtak, Michael Buro. *Recursive Monte Carlo Search for Imperfect Information Games.* IEEE CIG 2013. https://skatgame.net/mburo/ps/recmc13.pdf
9. **[read]** Sebastian Kupferschmid, Malte Helmert. *A Skat Player Based on Monte-Carlo Simulation.* Computers and Games (CG 2006), LNCS 4630, pp. 135–147, Springer, 2007. https://ai.dmi.unibas.ch/papers/kupferschmid-helmert-cg2006.pdf
10. **[read]** Tristan Cazenave, Véronique Ventos. *The αμ Search Algorithm for the Game of Bridge.* arXiv:1911.07960, 2019. https://arxiv.org/abs/1911.07960 (also published in Monte Carlo Search / Springer CCIS, https://link.springer.com/chapter/10.1007/978-3-030-89453-5_1)
11. **[read]** Tristan Cazenave, Swann Legras, Véronique Ventos. *Optimizing αμ.* arXiv:2101.12639, 2021. https://arxiv.org/abs/2101.12639
12. **[read]** Nathan R. Sturtevant. *A Comparison of Algorithms for Multi-player Games.* Computers and Games 2002, LNCS 2883, pp. 108–122, Springer, 2003. https://webdocs.cs.ualberta.ca/~nathanst/papers/comparison_algorithms.pdf
13. **[read]** Nathan R. Sturtevant. *Current Challenges in Multi-Player Game Search.* Computers and Games 2004. https://cs.du.edu/~sturtevant/papers/Multi-PlayerChallenges.pdf
14. **[read]** Nathan R. Sturtevant. *An Analysis of UCT in Multi-Player Games.* ICGA Journal 31(4):195–208, 2008; also Computers and Games 2008, LNCS 5131, pp. 37–49. https://webdocs.cs.ualberta.ca/~nathanst/papers/mpuct_icga.pdf
15. **[read]** Inon Zuckerman, Ariel Felner, Sarit Kraus. *Mixing Search Strategies for Multi-Player Games.* IJCAI-09, pp. 646–651, 2009. https://www.ijcai.org/Proceedings/09/Papers/113.pdf (journal version: *The MP-Mix Algorithm: Dynamic Search Strategy Selection in Multiplayer Adversarial Search*, IEEE TCIAIG — **[located]**)
16. **[read]** Christopher Solinas, Douglas Rebstock, Michael Buro. *Improving Search with Supervised Learning in Trick-Based Card Games.* AAAI-19; arXiv:1903.09604. https://arxiv.org/abs/1903.09604
17. **[read]** Douglas Rebstock, Christopher Solinas, Michael Buro, Nathan R. Sturtevant. *Policy Based Inference in Trick-Taking Card Games.* IEEE Conference on Games (CoG) 2019; arXiv:1905.10911. https://arxiv.org/abs/1905.10911
18. **[read]** Bruno Bouzy, Alexis Rimbaud, Véronique Ventos. *Recursive Monte Carlo Search for Bridge Card Play.* IEEE CoG 2020, pp. 229–236. https://ieee-cog.org/2020/papers/paper_82.pdf
19. **[read]** Jérôme Arjonilla, Abdallah Saffidine, Tristan Cazenave. *Perfect Information Monte Carlo with Postponing Reasoning.* arXiv:2408.02380, 2024. https://arxiv.org/abs/2408.02380
20. **[read, partial]** Paul M. Bethe. *The State of Automated Bridge Play.* NYU, January 2010. https://cs.nyu.edu/~pbethe/bridgeReview200908.pdf — useful survey confirming that the vector approaches "did not provide much improvement at an unreasonable speed" in Bridge practice, and that Jack and WBridge5 use customised Monte Carlo + double dummy.
21. **UNVERIFIED** David N. L. Levy. *The Million Pound Bridge Program.* In *Heuristic Programming in Artificial Intelligence: The First Computer Olympiad*, Ellis Horwood, pp. 95–103, 1989. (The original PIMC proposal; cited by [1], [2], [8], [9], [10], [16].)
22. **UNVERIFIED** Matthew L. Ginsberg. *Partition Search.* AAAI-96, pp. 228–233, 1996. (Cited by [2], [9], [10].)
23. **UNVERIFIED** Peter I. Cowling, Edward J. Powley, Daniel Whitehouse. *Information Set Monte Carlo Tree Search.* IEEE Transactions on Computational Intelligence and AI in Games 4(2):120–143, 2012. (Cited by [8], [10], [16], [19].)
24. **UNVERIFIED** Carol A. Luckhardt, Keki B. Irani. *An Algorithmic Solution of N-Person Games.* AAAI-86. (Origin of maxn; cited by [12], [13], [15].)
25. **UNVERIFIED** Richard E. Korf. *Multi-player alpha-beta pruning.* Artificial Intelligence 48(1):99–111, 1991. (Immediate/shallow/deep pruning analysis; cited by [12], [13], [15].)
26. **UNVERIFIED** Nathan R. Sturtevant, Richard E. Korf. *On Pruning Techniques for Multi-Player Games.* AAAI-2000, pp. 201–207. (Formal analysis of the paranoid algorithm; cited by [12], [13].)
27. **[located]** Nathan R. Sturtevant, Adam M. White. *Feature Construction for Reinforcement Learning in Hearts.* Computers and Games 2006, LNCS 4630, pp. 122–134. https://sites.ualberta.ca/~amw8/hearts.pdf — TD-learned linear evaluator that beat the then-best search-based Hearts program; the "learned players" that outscore UCT in [14].
28. **UNVERIFIED** Martin Müller. *Partial order bounding: A new approach to evaluation in game tree search.* Artificial Intelligence 129(1–2):279–311, 2001. (Theoretical antecedent of αμ; cited by [10].)
29. **UNVERIFIED** Pallab Dasgupta, P. P. Chakrabarti, S. C. DeSarkar. *Searching game trees under a partial order.* Artificial Intelligence 82(1–2):237–257, 1996. (Cited by [10].)
30. **UNVERIFIED** Timothy Furtak, Michael Buro. *Minimum Proof Graphs and Fastest-Cut-First Search Heuristics.* IJCAI-09, 2009. (The 40% search-effort reduction cited in [7].)
31. **UNVERIFIED** Michael Buro. *From Simple Features to Sophisticated Evaluation Functions* (GLEM). Computers and Games CG'98, LNCS 1558, pp. 126–145, Springer, 1999. (The evaluation framework used in [7].)
32. **UNVERIFIED** Martin Zinkevich, Michael Johanson, Michael Bowling, Carmelo Piccione. *Regret Minimization in Games with Incomplete Information.* NIPS 20, pp. 1729–1736, 2008. (CFR; the equilibrium baseline in [1].)
33. **[located]** Michal Šustr, Martin Schmid, Matej Moravčík, Neil Burch, Marc Lanctot, Michael Bowling. *Sound Algorithms in Imperfect Information Games.* AAMAS 2021 (extended abstract); arXiv:2006.08740. https://arxiv.org/abs/2006.08740 — formal treatment of soundness that determinization lacks. I did not read the primary text.
34. **[located]** Jan Schäfer. *The UCT Algorithm Applied to Games with Imperfect Information.* MSc thesis, University of Magdeburg, 2007. (The "Bernie" information-set-UCT Skat player benchmarked in [7], [8].)
35. **UNVERIFIED — press coverage only** NukkAI "NooK" Bridge Challenge, Paris, March 2022: NooK reportedly won 67 of 80 sets (83%) against eight world champions in a declarer-play-only, bidding-excluded format, each side playing 800 deals against WBridge5. Reported by CBC, Fortune, SingularityHub, Slashdot; I found **no peer-reviewed primary source** and could not verify the protocol. Do not cite this as a result.
36. **[read]** Bo Haglund's Double Dummy Solver is referenced as the DDS backend in [10] and [11]; I did not access its documentation directly. **UNVERIFIED** as a primary source.

---

### Appendix A — quick-reference formula card

| Quantity | Formula |
|---|---|
| PIMC move choice | $m^\*=\arg\max_m \sum_{w\in D}\Pr(w\mid h)\,V^{PI}(w,m)$ |
| Leaf correlation | $lc=\Pr[\mathrm{val}(\ell_L)=\mathrm{val}(\ell_R)]$, estimated at pre-terminal nodes |
| Bias | $b=\Pr[\mathrm{val}=+1\mid \text{correlated}]$ |
| Disambiguation from measured retention $\rho$ *(derived here)* | $df=\dfrac{2(1-\rho)}{2-\rho}$, $\;\rho=\dfrac{2(1-df)}{2-df}$ |
| Vector-mm MAX | $\max\{\vec P_i\}=\vec P_{i^\*},\; i^\*=\arg\max_i\sum_j \Pr(w_j)\vec P_i[j]$ |
| Vector-mm MIN | $\min\{\vec P_i\}=(\min_i \vec P_i[1],\dots,\min_i \vec P_i[n])$ |
| prm reduction | $\tilde P[k](v)=\min\big(P[k](v),\min_{u\in\mathrm{MINanc}(v)} m_k(u)\big)$ |
| Vector dominance (αμ) | $x_1\succ x_2 \iff \forall i\, x_1[i]\ge x_2[i]\ \wedge\ \exists i\, x_1[i]>x_2[i]$ |
| Front dominance (αμ) | $f_1\succeq f_2\iff \forall x_2\in f_2\ \exists x_1\in f_1:\ x_1\succ x_2 \vee x_1=x_2$ |
| αμ Min node | $f_{\min}=\mathrm{Pareto}\big(\{\min_k x_k : (x_1..x_m)\in f_1\times\cdots\times f_m\}\big)$ |
| Lattice minimax (GIB) | $\min(f,g)(s)=\min(f(s),g(s))$ on $2^S$ |
| Kermit feature inference | $\Pr(\text{world}\mid\text{move})=\prod_i \Pr(f_i)\Pr(f_i\mid\text{move})$ |
| Card-location inference | $p(s\mid h)\propto\prod_{c\in C} L(h)_{c,\mathrm{loc}(c,s)}$ |
| Policy-based reach | $\eta(s\mid\mathcal I)=\prod_{h\cdot a\sqsubseteq s}\pi(h,a)$ |
| Inference KPI | $\mathrm{TSSR}=p(s^\*\mid h)\cdot n$ |
| Strategy-fusion count | $\mathrm{SF}(\pi)=|\{s:\exists h,h'\in H(s),\ \pi(h)\neq\pi(h')\}|$ |
| GLEM evaluation | $e(s)=l(\sum_i w_i f_i(s))$, $f(s)=T[h_1(s)]\cdots[h_n(s)]$ |
| Paranoid best-case nodes | $b^{d(n-1)/n}$ (vs $b^{d/2}$ for two players) |
| Opponent Impact | $OI(G,H)=|\mathrm{InfluentialStates}|/|\mathrm{TotalStates}|$; Go Fish $=1$, Bingo $=0$ |
| Fish info-set size at deal | $|\mathcal I_0|=45!/(9!)^5\approx1.90\times10^{28}$ |
