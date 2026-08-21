# CFR Family, Self-Play RL, and Equilibrium Concepts for Imperfect-Information TEAM Games

**Research area:** equilibrium targets and learning algorithms for 3v3, no-communication, public-transfer card games
**Target application:** Canadian Fish / Literature (6 players, 2 teams of 3, 54 cards, 9 half-suits)
**Date compiled:** 2026-08-21

---

## 1. Executive Summary

1. **Nash equilibrium is the wrong target for Fish.** In a game where two *teams* of players share a payoff but cannot communicate during play, the correct family of solution concepts is the **team-maxmin equilibrium** hierarchy of Celli & Gatti (AAAI 2018): TMECom (communication during play), **TMECor** (correlation agreed *before* play only), and TME (no correlation at all). Fish teammates may agree on a convention before the game but may not signal outside legal actions — that is *exactly* TMECor.
2. **TMECor is FNP-hard** to compute and TME is FNP-hard *and* inapproximable in the additive sense; TMECom is poly-time solvable via a reduction to a 2-player maxmin problem. So the tractable relaxation (TMECom) is the one Fish *cannot* use, and the concept Fish needs (TMECor) is the hard one.
3. **The loss from not being able to communicate is unbounded.** Celli & Gatti's "price of uncorrelation" is worst-case $\mathrm{PoU}_{\mathrm{Com/No}} = |L|/2$, $\mathrm{PoU}_{\mathrm{Cor/No}} = |L|/4$, $\mathrm{PoU}_{\mathrm{Com/Cor}} = \sqrt{|L|}$ in the number of leaves $|L|$. Practically: a Fish bot that ignores teammate coordination can lose arbitrarily much versus one that coordinates.
4. **TMECor is now computable by regret minimization**, not just LP/column generation, via two equivalent "coordinator" reformulations: the **team-public-information (TPI) game** of Carminati et al. (ICML 2022) and the **Team Belief DAG (TB-DAG)** of Zhang, Farina, Celli & Sandholm (2022/ICML 2023). Both convert an adversarial team game into a 2-player zero-sum game whose Nash equilibria are realization-equivalent to TMECor of the original, letting you run CFR+/DCFR directly.
5. **The coordinator's action space is a *prescription*** — a function from each teammate's private infoset to an action. For Fish this is catastrophic if built naively (a prescription must specify what each of 3 teammates would do for each of their $\sim 10^{28}$ possible hands), so the TB-DAG must be used only on *abstracted* or *endgame* Fish, not the full game.
6. **Fish has an unusually favourable public/private structure for team methods.** Because every ask, answer, transfer, declaration and hand count is public, the *only* hidden variable in the whole game is the initial 54-card deal. The public state is common knowledge to everyone including the opponents. This makes exact Bayesian belief tracking well-defined and constraint-based (a deal is either consistent with history or not), which is the single biggest algorithmic gift the game gives you.
7. **Combinatorics (computed):** total deals $= 54!/(9!)^6 \approx 1.011\times10^{38}$; deals consistent with your own hand $= 45!/(9!)^5 \approx 1.90\times10^{28}$; **deals consistent with your whole team's pooled 27 cards $= 27!/(9!)^3 \approx 2.28\times10^{11}$.** That last number is the size of the *coordinator's* belief support at the start of the game, and it collapses very fast under ask/answer constraints. It is small enough for particle filtering and, late in the game, for exact enumeration.
8. **Deep CFR / DREAM / ESCHER are the "principled" scalable CFR line.** Deep CFR (Brown et al., ICML 2019) replaces tabular regret tables with an advantage network trained on reservoir-sampled MCCFR regret targets; DREAM (Steinberger, Lerer & Brown, 2020) makes it model-free via a learned baseline; ESCHER (McAleer et al., ICLR 2023) removes importance sampling entirely with a history value function and empirically beats DREAM/NFSP in >90% of head-to-head matches in Dark Chess.
9. **But none of the CFR line has a convergence guarantee in a 3v3 team game unless you first do the coordinator conversion.** Running CFR independently for 6 seats in Fish gives you *no* equilibrium guarantee — it is a >2-player general-sum problem from each seat's point of view.
10. **The self-play RL line is what actually ships in large card games.** DouZero's **Deep Monte-Carlo (DMC)** with *action-as-input* encoding (Zha et al., ICML 2021) reached #1 of 344 bots on Botzone using 48 CPU cores + 4 GPUs for 30 days. **DanZero+** (Lu et al., 2023) applied the same recipe to **GuanDan — a 4-player, 2v2, no-communication team card game**, which is the closest published analogue to Fish. PerfectDou (Guan et al., 2022) beat DouZero with **perfect-training/imperfect-execution (PTIE)**: a perfect-information *critic* and an imperfect-information *actor*, trained by PPO+GAE, with 10× better sample efficiency.
11. **Cooperative-hidden-information work (Hanabi) is the right literature for Fish's signalling problem**, because Fish teammates must signal only through legal asks. Key results: **BAD** 24.174/25 (2p), **SAD** 24.01/25 (2p) and 23.01 (5p), **Other-Play** for zero-shot coordination, **Off-Belief Learning (OBL)** reaching 24.10 self-play / 23.76 cross-play at level 4 while provably converging to a *unique* policy independent of initialization.
12. **Search on top of a blueprint is the highest value-per-compute technique available.** SPARTA (Lerer, Hu, Foerster & Brown, AAAI 2020) lifted a Hanabi blueprint from 24.08 → **24.61/25** purely by test-time belief-conditioned rollouts, with a soundness theorem $V_{\pi_s} - V_{\pi_b} \ge -2T\Delta|A|N^{-1/2}$. Learned Belief Search recovers 55–91% of that benefit at 4.6–35.8× less compute; Mirror-Descent Search (Sokota et al., 2023) matches SPARTA with **two orders of magnitude less search time**.
13. **Joint Policy Search (Tian, Gong & Jiang, NeurIPS 2020) is the single most directly transferable "team convention improvement" algorithm.** It computes the *exact* value change of simultaneously modifying a team's policy at several infosets, without re-evaluating the whole game, in $O(|S| + M)$ rather than $O(|S|\cdot M)$. It raised a bridge-bidding agent from +0.29 to **+0.63 IMPs/board vs WBridge5** — and bridge bidding is structurally the same problem as Fish asking: a legal-action channel that must carry information to a partner while an adversary listens.
14. **PIMC / determinization is the cheap baseline you must beat, and it has known, named failure modes**: *strategy fusion* (the search assumes it will know the hidden state later) and *non-locality* (opponents' reach probabilities from elsewhere in the tree change the value here). Long, Sturtevant, Buro & Furtak (AAAI 2010) characterize when PIMC is safe via leaf correlation, bias, and disambiguation factor. Fish has **very high disambiguation** (each ask/answer is a hard constraint) which favours PIMC, but PIMC will *never* invent a signalling convention, because it always assumes teammates already know the deal.
15. **Realistic compute on 15 CPU cores, no GPU:** roughly $4\times10^{8}$–$1.3\times10^{9}$ decision evaluations/day with a small MLP, i.e. $\sim 4\times10^{6}$–$1.3\times10^{7}$ self-play games/day. That is ~1 order of magnitude below DouZero's 30-day 48-core+4-GPU budget. Verdict: **exact belief tracking + DMC-style value learning + test-time search** is affordable; **Deep CFR/DREAM on the full game, or TB-DAG on the full game, is not.**

---

## 2. Part A — Equilibrium Concepts for Adversarial Team Games

### 2.1 Notation

Extensive-form game $\Gamma$: histories $H$, terminal histories $Z$, players $N$, information sets $\mathcal I_i$ for player $i$, actions $A(I)$, utility $u_i: Z \to \mathbb R$. A behavioural strategy $\sigma_i$ maps $I \in \mathcal I_i$ to $\Delta(A(I))$. Reach probability $\pi^\sigma(h) = \prod_{i} \pi_i^\sigma(h)$ factorizes by player; $\pi^\sigma_{-i}(h)$ excludes $i$ (chance included).

**Fish instantiation.** $N = \{1,\dots,6\}$, team $\mathcal T_A = \{1,3,5\}$, team $\mathcal T_B = \{2,4,6\}$ (alternating seats). Chance moves once at the root, dealing 9 cards to each player: $54!/(9!)^6 \approx 1.01\times10^{38}$ outcomes. Thereafter **every** action is public. Hence for any history $h$, the public component $h^{\mathrm{pub}}$ is the *entire* action sequence, and player $i$'s information set is
$$\mathcal I_i(h) \;=\; \{\, h' : h'^{\mathrm{pub}} = h^{\mathrm{pub}} \ \wedge\ \mathrm{hand}_i(h') = \mathrm{hand}_i(h) \,\}.$$
Equivalently, an infoset for player $i$ is (public history, own current hand), and the residual uncertainty is a distribution over deals consistent with both.

### 2.2 The three team solution concepts (Celli & Gatti, AAAI 2018)

Let team $\mathcal T$ share utility $U_{\mathcal T}$ and face adversary $\mathcal A$ (zero-sum: $U_{\mathcal A} = -U_{\mathcal T}$).

**TMECom — communication during play.** A mediator sees everything each teammate observes and issues recommendations in real time; teammates report truthfully and obey.
$$\mathrm{TMECom}: \quad \max_{\gamma} \ \min_{r_{\mathcal A}} \ U_{\mathcal T}(\gamma, r_{\mathcal A})$$
where $\gamma$ is a distribution over *feedback rules* mapping observed information to recommended actions. Equivalent to collapsing the team into a single player with perfect recall of the union of teammates' observations, hence **solvable in polynomial time** via a 2-player maxmin LP in the auxiliary "T-observable" game.

**TMECor — correlation before play only.** Teammates draw a joint plan from a shared correlation device (a shared random seed + a pre-agreed convention), then play it without further communication.
$$\mathrm{TMECor}: \quad \max_{\sigma_{\mathcal T} \in \Delta(P_{jr})} \ \min_{r_{\mathcal A}} \ U_{\mathcal T}(\sigma_{\mathcal T}, r_{\mathcal A})$$
where $P_{jr}$ is the set of *joint reduced normal-form plans* of the team. **This is the Fish target.**

**TME — no correlation.** Teammates independently randomize:
$$\mathrm{TME}: \quad \max_{r_1,\dots,r_{n-1}} \ \min_{r_{\mathcal A}} \ U_{\mathcal T}\Big(\textstyle\prod_{i \in \mathcal T} r_i\Big).$$

**Values.** $v_{\mathrm{Com}} \ge v_{\mathrm{Cor}} \ge v_{\mathrm{No}}$.

### 2.3 Complexity and the price of uncorrelation

From Celli & Gatti (2018):

| Concept | Complexity |
|---|---|
| TMECom | Poly-time (reduction to 2-player maxmin in auxiliary game) |
| TMECor | **FNP-hard**; admits an equilibrium with support $\le |Q_{\mathcal A}|$ (linear in the adversary's number of sequences) |
| TME | **FNP-hard and inapproximable in the additive sense** |

**Price of uncorrelation** indices:
$$\mathrm{PoU}_{\mathrm{Com/No}} = \frac{v_{\mathrm{Com}}}{v_{\mathrm{No}}},\quad
\mathrm{PoU}_{\mathrm{Cor/No}} = \frac{v_{\mathrm{Cor}}}{v_{\mathrm{No}}},\quad
\mathrm{PoU}_{\mathrm{Com/Cor}} = \frac{v_{\mathrm{Com}}}{v_{\mathrm{Cor}}}.$$
Worst-case bounds in the number of leaves $|L|$:
$$\mathrm{PoU}_{\mathrm{Com/No}} = |L|/2,\qquad \mathrm{PoU}_{\mathrm{Cor/No}} = |L|/4,\qquad \mathrm{PoU}_{\mathrm{Com/Cor}} = \sqrt{|L|}.$$

The *support* result matters practically: **an optimal TMECor exists with support size at most the number of adversary sequences.** For Fish this bounds how many distinct "team conventions" a mixed optimal strategy needs — large, but it justifies population/double-oracle methods (Team-PSRO) that build the support incrementally.

### 2.4 The hybrid maxmin LP for TMECor and the BR-T oracle

Celli & Gatti solve TMECor with a *hybrid* representation: normal form for the team, sequence form for the adversary.
$$
\begin{aligned}
\max_{\sigma_{\mathcal T}, v} \quad & \sum_{h \in H_{\mathcal A} \cup \{h_\varnothing\}} f_{\mathcal A}(h)\, v(h) \\
\text{s.t.} \quad & \sum_{h} F_{\mathcal A}(h, q_{\mathcal A})\, v(h) \;-\; \sum_{p \in P_{jr}} U_h(q_{\mathcal A}, p)\, \sigma_{\mathcal T}(p) \;\le\; 0 \qquad \forall\, q_{\mathcal A} \in Q_{\mathcal A}\\
& \sum_{p \in P_{jr}} \sigma_{\mathcal T}(p) = 1,\qquad \sigma_{\mathcal T}(p) \ge 0 .
\end{aligned}
$$
$Q_{\mathcal A}$ are adversary sequences, $F_{\mathcal A}$ the sequence-form constraint matrix, $f_{\mathcal A}$ its RHS. Because $|P_{jr}|$ is astronomically large, the LP is solved by **column generation** with a *team best-response* oracle (BR-T):
$$
\begin{aligned}
\max_{r_1,\dots,r_{n-1},\,x} \quad & \sum_{l \in L} U_{\mathcal T}(l)\, x(l)\, \bar r_{\mathcal A}(\mathrm{path}(l|\{n\})) \\
\text{s.t.}\quad & \sum_{q_i \in Q_i} F_i(h, q_i)\, r_i(q_i) = f_i(h) \quad \forall i \in \mathcal T,\ h \in H_i \cup \{h_\varnothing\}\\
& x(l) \le r_i(q_i) \quad \forall i,\ l,\ q_i \in \mathrm{path}(l|\{i\}),\qquad x(l) \in \{0,1\}.
\end{aligned}
$$
BR-T is **APX-hard** (it generalizes MAX-SAT; the paper gives $\alpha_{\text{BR-T-}h} \le (\alpha_{\text{MAX-SAT}})^h$ for depth-$3h$ trees). This is the fundamental barrier: *even computing a single joint team best response is hard.*

**Fictitious team play** iterates: (i) solve the restricted meta-matrix game between current team plans and adversary sequences; (ii) call BR-T for a new team plan; (iii) call the adversary's best response; repeat.

### 2.5 Coordinator representations: TPI and TB-DAG (the modern route)

**Team-Public-Information (TPI) game** — Carminati, Cacciamani, Ciccone & Gatti (ICML 2022). Replace the team with a single **coordinator** who knows only what is *common knowledge to the whole team*. The coordinator's actions are **prescriptions**: functions $\Gamma$ assigning an action to *each* team infoset in the current public state,
$$\Gamma \in \prod_{j=1}^{m} A_{I_j}, \qquad I_1,\dots,I_m \text{ the team infosets in the current public state.}$$
**Theorem (4.6):** a Nash equilibrium $\mu_t^*$ of the TPI game $\mathcal G'$ is realization-equivalent to a TMECor $\mu_{\mathcal T}^*$ of $\mathcal G$. Consequently **abstraction, no-regret learning (CFR+), and subgame solving all become available for team games.** Reported: TPI is exponentially smaller than the normal form; pruning/folding gives up to 3 orders of magnitude reduction; CFR+ on the abstracted TPI converges >1 order of magnitude faster than prior methods. Blowup is exponential in general, but **Theorem 4.8**: if all team members observe external actions identically ("common external information"), the TPI tree is *linear* in the original game.

**Team Belief DAG (TB-DAG)** — Zhang, Farina, Celli & Sandholm. Build a DAG $\mathcal D$ from the team decision problem $\mathcal T = (\mathcal H, \mathcal I)$:
- Root: active node labelled $\{\varnothing\}$.
- **Active node** with label $B \subseteq \mathcal H$: identify the infosets $I_1,\dots,I_m$ intersecting $B$; actions are prescriptions $\mathbf a \in \bigtimes_{i} A_{I_i}$; the child is labelled
$$B\mathbf a := \{h a_i : h \in I_i \cap B\} \cup \{h\tilde a : h \in J,\ \tilde a \in A_h\}.$$
- **Inactive node** with label $O$: split into connected components $P_1,\dots,P_m$ of the connectivity graph $G[O]$ (the *public observations*); each becomes an active child.

**Theorem 4.2:** $\mathcal D$ and $\mathcal T$ are strategically equivalent (the sequence-form theorem, generalized to teams / imperfect recall). Team strategies become **flows on the DAG**, i.e. a *convex* set — which is exactly what the joint team strategy space was missing.

**Theorem 4.3:** CFR runs on the TB-DAG with $R^T = O(N\sqrt T)$ for $N$ nodes, $O(E)$ time per iteration.

**Size bounds.** For $k$-private games (each public state contains at most $k$ distinct last infosets), edges $= O^*(3^k)$. Generally, edges $= O^*\big(\sum_{i=1}^{w}\binom{p}{i} b^{w}\big)$ with $p$ = largest public-state effective size, $w$ = largest belief effective size, $b$ = branching factor. **Empirical (from the paper):** 3K8 — 1,783,926 DAG vertices, $\varepsilon=10^{-3}$ in 4.73 s vs 32.36 s (LP) vs 3 m 23 s (column generation); 3L143 — 0.10 s vs 48 s vs 7 m 58 s. Roughly **2–3 orders of magnitude** faster than tree-decomposition/column-generation predecessors.

### 2.6 Learning-based routes to TMECor

- **Team-PSRO** (McAleer, Farina, Zhou, Wang, Yang & Sandholm, NeurIPS 2023). PSRO lifted to teams: maintain a population of *joint team policies*; each iteration, each team learns a **joint best response** to the opponent team's meta-strategy via cooperative RL; the meta-game is solved for a TMECor over the restricted populations. **Guarantee:** as the RL joint best response approaches optimal, Team-PSRO converges to a TMECor. Variant *Mix-and-Match* reuses population members combinatorially. Evaluated on tabular poker variants and Google Research Football.
- **S-PSRO / rCTME** (Leveraging Team Correlation for Approximating Equilibrium in Two-Team Zero-Sum Games, 2024). Defines **restricted Correlated-Team Maxmin Equilibrium** with a *sample factor* limiting the deviation policy space, plus a Sequential Best-Response oracle using multi-agent advantage decomposition. Lower exploitability than Team-PSRO on Seek-Attack-Defend, MAgent 12v12/16v16, and GRF 5v5.
- **Fictitious Cross-Play (FXP)** (Xu, Liang, Yu, Wang & Wu, AAMAS 2023). Proves that self-play with "preference-preserving" updates converges to **local** (team-suboptimal) Nash with high probability in mixed cooperative-competitive games. FXP keeps a *main* population trained by self-play with probability $\eta$ and a *counter* population trained by cross-play (cooperative best responses against frozen main-population checkpoints). $\eta = 1$ recovers SP, $\eta = 0$ approximates PSRO. 94%+ win rates in 11v11 GRF.
- **Soft Team Actor-Critic (STAC)** (Celli, Ciccone, Bongo, Gatti, 2019). Deep MARL that exploits ex-ante shared exogenous signals to realize correlation without domain knowledge — the direct RL analogue of a correlation device.
- **Team-Fictitious Play** (NeurIPS 2024) for team-Nash in multi-team games.

**The critical negative result for Fish:** naive independent self-play among 6 seats has **no** convergence guarantee, and FXP shows it will typically settle in a *local* equilibrium where the team's joint deviation would improve payoff but no single member's unilateral deviation does. This is precisely the failure mode "my bot never learned a signalling convention because unilaterally deviating to signal looks bad."

---

## 3. Part B — The CFR Family, With Equations

### 3.1 Vanilla CFR (Zinkevich, Johanson, Bowling & Piccione, NIPS 2007)

Counterfactual value of infoset $I$ for player $i$ under $\sigma$:
$$v_i^\sigma(I) \;=\; \sum_{h \in I}\ \sum_{z \in Z} \pi_{-i}^\sigma(h)\, \pi^\sigma(h,z)\, u_i(z),$$
$$v_i^\sigma(I,a) \;=\; \sum_{h \in I}\ \sum_{z \in Z} \pi_{-i}^\sigma(h)\, \pi^\sigma(ha,z)\, u_i(z).$$

Instantaneous and cumulative counterfactual regret:
$$r^t(I,a) = v_i^{\sigma^t}(I,a) - v_i^{\sigma^t}(I),\qquad R^T(I,a) = \sum_{t=1}^{T} r^t(I,a).$$

**Regret matching:**
$$\sigma^{T+1}(I,a) \;=\;
\begin{cases}
\dfrac{R^{T,+}(I,a)}{\sum_{b \in A(I)} R^{T,+}(I,b)} & \text{if } \sum_b R^{T,+}(I,b) > 0,\\[8pt]
\dfrac{1}{|A(I)|} & \text{otherwise,}
\end{cases}
\qquad x^{+} := \max\{x,0\}.$$

**Average strategy** (the thing that converges, not the current iterate):
$$\bar\sigma^T(I,a) = \frac{\sum_{t=1}^{T} \pi_i^{\sigma^t}(I)\,\sigma^t(I,a)}{\sum_{t=1}^{T} \pi_i^{\sigma^t}(I)} .$$

**Bound:** $R_i^T \le \Delta_i |\mathcal I_i| \sqrt{|A_i|}\,\sqrt{T}$, so in 2-player zero-sum, $\bar\sigma^T$ is a $2\Delta|\mathcal I|\sqrt{|A|}/\sqrt T$-Nash equilibrium.

### 3.2 CFR+ (Tammelin, 2014)

Three changes: **regret-matching+ (RM+)**, **alternating updates**, **linear averaging**.
$$Q^t(I,a) \;=\; \big[\,Q^{t-1}(I,a) + r^t(I,a)\,\big]^{+},\qquad
\sigma^{t+1}(I,a) = \frac{Q^t(I,a)}{\sum_b Q^t(I,b)} .$$
Because regrets are clipped to $\ge 0$ every iteration, an action that becomes good again recovers immediately instead of paying off a huge negative debt. Averaging weight $w^T = \max\{T-d, 0\}$ for a delay $d$. Empirically ">an order of magnitude" faster than CFR with less memory. Note: the CFR+ paper does not state a formal regret bound theorem for RM+; the bound came later in the literature.

### 3.3 Discounted CFR (Brown & Sandholm, AAAI 2019)

$\mathrm{DCFR}_{\alpha,\beta,\gamma}$: on iteration $t$,
- multiply accumulated **positive** regrets by $\dfrac{t^\alpha}{t^\alpha + 1}$,
- multiply accumulated **negative** regrets by $\dfrac{t^\beta}{t^\beta + 1}$,
- multiply the accumulated **average-strategy** contribution by $\left(\dfrac{t}{t+1}\right)^{\gamma}$.

Explicitly:
$$R^t(I,a) =
\begin{cases}
R^{t-1}(I,a)\cdot\frac{(t-1)^\alpha}{(t-1)^\alpha+1} + r^t(I,a), & R^{t-1}(I,a) > 0,\\[4pt]
R^{t-1}(I,a)\cdot\frac{(t-1)^\beta}{(t-1)^\beta+1} + r^t(I,a), & R^{t-1}(I,a) \le 0,
\end{cases}$$
$$S^t(I,a) = S^{t-1}(I,a)\cdot\Big(\tfrac{t-1}{t}\Big)^{\gamma} + \pi_i^{\sigma^t}(I)\,\sigma^t(I,a).$$
**Recommended default: $\alpha = 3/2,\ \beta = 0,\ \gamma = 2$** — consistently stronger than CFR+, ~2–3× faster on HUNL subgames. **Linear CFR** = $\mathrm{DCFR}_{1,1,1}$: dramatic in games with severe "blunder" actions (970 vs 471,407 iterations in the paper's toy example) but poor on Goofspiel. **Known weakness of CFR+**: it does relatively badly in games where some actions are very costly mistakes, and it interacts badly with abstraction and pruning.

### 3.4 MCCFR (Lanctot, Waugh, Zinkevich & Bowling, NIPS 2009)

Partition terminals $Z$ into blocks $\mathcal Q = \{Q_1,\dots,Q_r\}$, sample block $Q_j$ with prob $q_j$; let $q(z) = \sum_{j : z \in Q_j} q_j$. The **sampled counterfactual value** is
$$\tilde v_i(\sigma, I \mid j) \;=\; \sum_{z \in Q_j \cap Z_I} \frac{1}{q(z)}\; u_i(z)\; \pi_{-i}^{\sigma}(z[I])\; \pi^{\sigma}(z[I], z),$$
which is **unbiased**: $\mathbb E_j[\tilde v_i(\sigma, I \mid j)] = v_i^\sigma(I)$. Sampled regret:
$$\tilde r^t(I,a) = \tilde v_i(\sigma^t_{I \to a}, I) - \tilde v_i(\sigma^t, I).$$

- **Outcome sampling (OS):** one terminal history per iteration; $q(z)$ = probability the sampling policy produced $z$. Cheapest per iteration, highest variance (the $1/q(z)$ importance weight can explode).
- **External sampling (ES):** sample all chance nodes and all *opponent* actions, enumerate the traverser's actions. Variance far lower; cost per iteration $\propto$ traverser's tree only. This is the standard workhorse and what Deep CFR uses.

Regret bounds (Lanctot et al., Thm 5-ish; exact constants **UNVERIFIED** — PDF text extraction failed, cite the paper directly before implementing):
$$\frac{R_i^T}{T} \;\le\; \Big(1 + \frac{2}{\sqrt p}\Big)\frac{1}{\sqrt\delta}\; \frac{\Delta_i |\mathcal I_i| \sqrt{|A_i|}}{\sqrt T} \quad \text{w.p. } \ge 1-p,$$
with $\delta$ the minimum probability any sampled block is chosen. Note $1/\sqrt\delta$: **outcome sampling's bound degrades as $\delta \to 0$**, which is why OS is unusable without variance reduction in deep games. Fish games are ~60–120 decisions deep — this is the regime where OS dies.

### 3.5 Deep CFR (Brown, Lerer, Gross & Sandholm, ICML 2019)

Replace the tabular regret table with an **advantage network** $V(I,a\mid\theta_p)$, trained on external-sampling regret targets stored in a **reservoir buffer** $\mathcal M_{V,p}$:
$$\mathcal L(\theta_p) \;=\; \mathbb E_{(I,\,t',\,\tilde r^{t'}) \sim \mathcal M_{V,p}}\left[\, t' \sum_{a \in A(I)} \big(\tilde r^{t'}(I,a) - V(I,a\mid\theta_p)\big)^2 \right].$$
The weight $t'$ implements **Linear CFR** inside the loss. Play uses regret matching on the network output:
$$\sigma^t(I,a) \propto V^{+}(I,a\mid\theta_p),\quad \text{or } \arg\max_a V(I,a) \text{ if all } \le 0 .$$
A separate **strategy network** distils the average strategy:
$$\mathcal L(\theta_\Pi) = \mathbb E_{(I,t',\sigma^{t'}) \sim \mathcal M_\Pi}\left[\, t' \sum_a \big(\sigma^{t'}(I,a) - \Pi(I,a\mid\theta_\Pi)\big)^2\right].$$
**Theorem 1** (approximation error): average regret is bounded by
$$\Big(1 + \tfrac{\sqrt 2}{\sqrt{\rho K}}\Big)\frac{\Delta |\mathcal I_p| \sqrt{|A|}}{\sqrt T} \;+\; 4|\mathcal I_p|\sqrt{|A| \Delta\, \varepsilon_{\mathcal L}}$$
where $\varepsilon_{\mathcal L}$ is the average network MSE. Note the **square-root dependence on $\varepsilon_{\mathcal L}$** — small approximation error still costs you.

**Empirical:** Flop Hold'em Poker (FHP) ~37 mbb/g exploitability vs NFSP's ~47; competitive with a 3.6M-cluster abstraction at 2–3 orders of magnitude fewer samples. HULH: loses to the largest ($3.3\times10^8$-bucket) abstraction by $-11\pm2$ mbb/g but beats NFSP by $+43\pm2$ mbb/g. ~98,948-parameter, 7-layer network with card and bet embeddings. **Ablations:** retraining the network from scratch each CFR iteration beats fine-tuning; reservoir sampling is essential; linear weighting helps.

### 3.6 DREAM (Steinberger, Lerer & Brown, 2020) — model-free Deep CFR

DREAM makes Deep CFR model-free (no perfect simulator needed for enumeration) using outcome sampling plus a **learned history baseline** $\hat Q_i^t(s^*(h), a_i \mid \phi_i^t)$ trained by expected SARSA. The variance-reduced estimator:
$$\tilde v_{i,\mathrm{DREAM}}^{\pi^t}(h,a_i \mid z) \;=\; \hat Q_i^t(s^*(h),a_i) \;+\; \frac{\mathbb 1[a_i \text{ sampled}]}{\xi_i^t(s_i,a_i)}\Big(\tilde v_{i,\mathrm{DREAM}}^{\pi^t}(h' \mid z) - \hat Q_i^t(s^*(h),a_i)\Big),$$
which is unbiased for any baseline and has variance $\to 0$ as $\hat Q \to Q$. Convergence $\Theta(\sqrt{|\mathcal A_i||\mathcal I_i|}/\sqrt T)$ with an exploration term scaling as $(|\mathcal A_i|/\epsilon)^{d_i}$ — **exponential in depth $d_i$**, again the Fish-killer.

### 3.7 ESCHER (McAleer, Farina, Lanctot & Sandholm, ICLR 2023)

Removes importance sampling entirely. Learn a **history value function** $q_i(\pi, h, a\mid\theta)$ and estimate regret directly:
$$\hat r_i(\pi, s, a \mid z) \;=\; q_i(\pi, z[s], a \mid \theta) \;-\; \sum_{a'} \pi_i(s,a')\, q_i(\pi, z[s], a' \mid \theta).$$
Sampling for the updating player uses a **fixed** distribution with full support, giving $O(\sqrt T \cdot \mathrm{polylog}(1/p))$ regret w.p. $\ge 1-p$. **Empirical:** orders of magnitude lower regret-estimator variance than DREAM; beats DREAM and NFSP in >90% of head-to-head matches in Dark Chess. Ablations confirm both design choices matter.

### 3.8 Magnetic Mirror Descent (Sokota et al., ICLR 2023)

A single, cheap, last-iterate-convergent alternative to CFR that is much friendlier to deep RL:
$$z_{t+1} = \arg\min_{z}\ \eta\big(\langle F(z_t), z\rangle + \alpha\,\psi(z)\big) + B_\psi(z; z_t),$$
with entropic mirror map giving the closed form
$$\pi_{t+1} \;\propto\; \big[\, \pi_t\, \rho^{\eta\alpha}\, e^{\eta q_t} \,\big]^{\frac{1}{1+\eta\alpha}},$$
$\rho$ the "magnet" (anchor policy), $q_t$ the action-value vector. **Linear last-iterate convergence to QRE:** $B_\psi(z^*, z_{t+1}) \le (1+\eta\alpha)^{-t} B_\psi(z^*, z_1)$. In deep RL form it substantially outperformed NFSP and PPO on 3×3 Dark Hex and Phantom Tic-Tac-Toe at the cost of ordinary mirror descent. **This is arguably the best CFR-substitute for a CPU-only budget**: no reservoir buffers, no importance weights, no average-strategy network.

### 3.9 ReBeL and Student of Games (search + RL over public belief states)

**ReBeL** (Brown, Bakhtin, Lerer & Gong, NeurIPS 2020) operates on **public belief states (PBS)** $\beta$ — joint distributions over each agent's possible infostates given common knowledge. **Theorem 1:** infostate values are supergradients of the PBS value function, $v_1^{\pi^*}(s_1\mid\beta) = V_1(\beta) + \bar g \cdot \hat s_1$. At each step: build a depth-limited subgame at $\beta_r$, run $T$ iterations of CFR-D with leaf values from $\hat v(s_i(z) \mid \beta_z^{\pi^t})$, then **sample a uniformly random iteration $t \sim \mathrm{unif}\{0,T-1\}$** and continue from $\beta^{\pi^t}$ — this makes the value targets unbiased. **Theorem 2:** $O(1/\sqrt T)$ value error; **Theorem 3:** running the same procedure at test time is safe (approximate Nash) with no modification. Networks: 6×1536 GeLU + LayerNorm, Huber loss for values, MSE for policy. **Results:** beat Dong Kim over 7,500 hands of HUNL at $165 \pm 69$ thousandths of a big blind per game; beat BabyTartanian8 and Slumbot; matched full-game tabular CFR exploitability in Liar's Dice with depth-2 subgames.

**Student of Games** (Schmid et al., Science Advances 2023) generalizes: **GT-CFR** grows the public tree during solving (regret-update phase with RM+ and linear averaging, alternating with an expansion phase), guided by a **counterfactual value-and-policy network (CVPN)** taking a PBS and emitting one counterfactual value per infostate per player plus a prior policy. Theorem 1: $O(1/\sqrt T)$ regret; Theorem 2: continual re-solving exploitability grows only linearly in game length. Beat Stockfish (4 threads, 1 s), ~1100 Elo above Pachi, beat Slumbot, 55% vs PimBot in Scotland Yard — with ~3500 concurrent actors.

**Why this matters for Fish and why it does not transfer directly:** ReBeL/SoG guarantees are **two-player zero-sum**. A 3v3 team game only becomes 2p0s *after* the coordinator conversion (§2.5), at which point the PBS is over *joint deals* and the coordinator's action set is a prescription set — astronomically large in Fish.

---

## 4. Part C — Self-Play RL for Large Card Games

### 4.1 NFSP (Heinrich & Silver, 2016)

Each agent keeps two networks: a best-response $Q(s,a;\theta_Q)$ trained by off-policy RL from a **circular** buffer $\mathcal M_{RL}$, and an average policy $\Pi(s,a;\theta_\Pi)$ trained by supervised learning from a **reservoir** buffer $\mathcal M_{SL}$ (reservoir sampling avoids windowing artefacts). Behaviour is the anticipatory mixture
$$\sigma \;=\; \eta\,\beta \;+\; (1-\eta)\,\Pi, \qquad \eta \approx 0.1 .$$
$$\mathcal L_{RL} = \mathbb E\big[(r + \gamma \max_{a'} Q(s',a';\theta^-) - Q(s,a;\theta))^2\big],\qquad
\mathcal L_{SL} = \mathbb E\big[-\log \Pi(s,a;\theta_\Pi)\big].$$
Leduc exploitability ~0.06 where plain DQN self-play diverges; competitive with abstraction-based bots in LHE. **NFSP is now dominated by Deep CFR / DREAM / ESCHER / MMD on sample efficiency** and is mainly useful as a sanity baseline.

### 4.2 DouZero — Deep Monte-Carlo with action-as-input (Zha et al., ICML 2021)

The most transferable recipe in this entire report for a CPU-limited Fish bot.

**Encoding.** Cards are $4\times15$ one-hot matrices (13 ranks + 2 jokers × card copies), flattened to 54-d after removing constant entries. **Crucially, the action is an *input* to the network, not an output index.** This lets the same network score arbitrary, combinatorially structured, never-before-seen legal moves, and sidesteps the enormous discrete action space.

**Architecture.** LSTM encodes the sequence of historical moves; the LSTM output is concatenated with (state, action) features and passed through 6 MLP layers of width 512 → scalar $Q(s,a)$.

**Deep Monte-Carlo (DMC).** No bootstrapping:
1. Generate a full episode with $\epsilon$-greedy over $\arg\max_a Q(s,a)$ on legal actions;
2. For each visited $(s,a)$ compute the Monte-Carlo return $G$ (with $\gamma = 1$ this is just the final reward);
3. Update $\theta \leftarrow \theta - \psi \nabla_\theta \big(G - Q(s,a;\theta)\big)^2$;
4. Policy improvement is implicit: act greedily w.r.t. $Q$.

This avoids the overestimation bias of $\max$-bootstrapping and is well suited to long horizons and sparse terminal rewards — **exactly Fish's reward structure** (score only at set-declaration time).

**Compute & results.** One server, 48 Xeon cores + 4× 1080Ti; 45 actors on three GPUs, one learner on the fourth; $\epsilon = 0.01$, $\gamma = 1$, lr $10^{-4}$, batch 32; **30 days** to surpass DeltaDou. WP 0.586 / ADP 0.258 vs DeltaDou; WP 0.659 / ADP 0.700 vs SL. Ranked **1st of 344 agents on Botzone (Elo 1625.11)**. Single forward pass per decision — orders of magnitude faster inference than MCTS-based DeltaDou.

### 4.3 DanZero / DanZero+ — GuanDan: a 2v2 no-communication team card game

**This is the closest published analogue to Fish.** GuanDan: 4 players in 2 fixed teams, two decks, imperfect information, **teams cooperate without any communication channel**, and the legal action set can exceed **5,000** moves.

- State: 513-d vector (own hand, remaining deck, recent plays, teammate status, opponent card counts, level info); action: 54-d vector with entries in $\{0,1,2\}$ (number of cards of each rank/suit).
- **DanZero** = DMC, 80 parallel actors, 4× Xeon Gold 6252 + 1× RTX 3070, **30 days**. ~80% win rate vs 8 rule-based baselines.
- **DanZero+** = use the pre-trained DMC $Q$ to **filter to the top-$k$ candidate actions**, then run PPO over that small candidate set. This cuts the effective action space from $O(n)$ to $O(k)$ and improved win rate vs the strongest baseline from 90.12 → **92.70**. Human eval: 71/100 rounds won vs proficient students.

**Lesson for Fish:** DMC gives you a robust, GPU-light $Q$ over a combinatorial action space; then a *small* policy-gradient refinement over the top-$k$ actions buys the last few points. Fish's ask branching factor is ~50–135 (see §6.1), far smaller than GuanDan's 5,000, so both stages are cheaper.

### 4.4 PerfectDou — Perfect-Training / Imperfect-Execution (Guan et al., NeurIPS 2022)

Asymmetric actor-critic: the **value network sees perfect information** (all hands), the **policy network sees only the observation**. Gradient:
$$\nabla_{\theta_p} J = \mathbb E_{\pi_p}\big[\nabla_{\theta_p} \log \pi_{\theta_p}(a\mid h)\; Q_{\pi_p}(D(h), a)\big],$$
where $D(h)$ denotes the perfect-information distinguishable node. Trained with PPO + GAE:
$$\mathcal L^{\mathrm{CLIP}}(\theta) = \mathbb E_t\Big[\min\big(\rho_t(\theta)\hat A_t,\ \mathrm{clip}(\rho_t(\theta), 1-\epsilon, 1+\epsilon)\hat A_t\big)\Big],\quad \rho_t = \frac{\pi_\theta(a_t\mid o_t)}{\pi_{\theta_{\mathrm{old}}}(a_t\mid o_t)},$$
$$\hat A_t = \sum_{l \ge 0} (\gamma\lambda)^l \delta_{t+l},\qquad \delta_t = r_t + \gamma V_\psi(D(h_{t+1})) - V_\psi(D(h_t)).$$
Extras: 12×15 card matrix encoding, and an **oracle reward** from a DP computing the minimum number of moves to empty a hand (dense intermediate reward). **Results:** 54.3% win rate and +0.143 ADP vs DouZero, with **10× better sample efficiency** (2.5B vs >10B samples).

**Note on soundness:** a perfect-information critic makes the advantage estimate biased with respect to the true imperfect-information value — it is the classic *centralized-critic* bias. It works empirically in Dou Dizhu; it is not equilibrium-sound. Suphx's *oracle guiding* (below) mitigates by annealing the perfect features away.

### 4.5 Suphx — Mahjong (Li et al., 2020)

Three techniques worth stealing:
1. **Global reward prediction:** a GRU predicts the final game reward from the current round's information, because per-round scores are a bad training signal. Use $\hat R$ as the RL return.
2. **Oracle guiding:** train an oracle agent with perfect-information features, then **drop out the privileged features on a schedule $\delta_t \to 0$**, so the policy degrades gracefully to an imperfect-information policy rather than being fine-tuned from scratch.
3. **pMCPA (parametric Monte-Carlo policy adaptation):** at *run time*, sample completions of the hidden state consistent with the current observation, roll out, and **fine-tune the policy parameters** on those simulated trajectories for this specific round. Parametric adaptation generalizes beyond the simulated trajectories, unlike tabular MCTS statistics.

Five specialized nets (discard/Riichi/Chow/Pong/Kong), 34×N channel input, 44 GPUs (~2 days per run), SL init from pro logs then RL. **Stable rank 8.74 dan on Tenhou, above 99.99% of ranked humans**, ~2 dan above Bakuuchi/NAGA.

### 4.6 Bridge — the closest "signalling through legal actions" literature

- **Rong, Qin & An, "Competitive Bridge Bidding with Deep Neural Networks", AAMAS 2019** (arXiv 1903.00900). Two networks: **ENN** $\phi_\omega: C \times V \times H \mapsto [0,1]^{52}$ (8 layers × 1500, skip connections every 2, cross-entropy over 52 sigmoids) infers the *partner's* cards; **PNN** $\sigma_\theta: C\times V\times H\times[0,1]^{52}\mapsto[0,1]^{38}$ (10 layers × 1200) selects the bid, taking the ENN output as input. Bid history is compressed to a **318-d binary vector** (one bit per position in the maximal bidding sequence) rather than one-hot per bid (>10,000 dims). RL by REINFORCE:
$$\theta \leftarrow \theta + \alpha\, r\, \frac{1}{M}\sum_{i=1}^{M} \nabla_\theta \log \sigma_\theta(b_i \mid s_i),$$
with $r$ the duplicate-bridge score from double-dummy analysis; opponent pool of historical checkpoints refreshed every 100 iterations; 2M random deals. **+0.25 IMP over WBridge5** (64 boards). In computer bridge, +0.1 IMP/board is considered significant.
- **Joint Policy Search (Tian, Gong & Jiang, NeurIPS 2020).** Defines a **policy-change density** $\rho^{(\sigma,\sigma')}(h)$ such that
$$V(\sigma') - V(\sigma) \;=\; \sum_{h} \rho^{(\sigma,\sigma')}(h), \qquad \rho^{(\sigma,\sigma')}(h) = -c^{(\sigma,\sigma')}(h) + \sum_a c^{(\sigma,\sigma')}(ha),$$
where $c$ is a reachability-weighted value term (**exact definition of $c$: UNVERIFIED from my extraction — read §3 of the paper before implementing**). The key property is that **$\rho$ vanishes at every $h$ where the policy is unchanged**, regardless of what changed upstream or downstream. Therefore evaluating a simultaneous multi-infoset joint policy change costs $O(|S| + M)$ instead of $O(|S|\cdot M)$. The algorithm does depth-first search over candidate infoset subsets to modify jointly, ordered upstream→downstream. **Results:** communication game (len 6) CFR 0.85 → 1.00; 2-suit mini-bridge CFR 2.60 → 2.74; and on real Contract Bridge bidding, A2C baseline **+0.29 IMPs/board vs WBridge5 → +0.63 IMPs/board with JPS**, beating the prior SOTA of +0.41. A sample-based online variant handles the $6.35\times10^{11}$ hands per player.
- **NooK / NukkAI (2022).** Hybrid symbolic + deep-learning bridge AI. In a two-day Paris challenge, NooK played the *card-play* phase (bidding excluded) on 800 deals in 80 sets of 10, against 8 world champions playing the same cards vs the same opponents, and **won 67 of 80 sets (83%)**. NooK is also explicitly explainable/rule-emitting. Primary technical publication not located — treat mechanism details as **UNVERIFIED** (press reports only). "Lorraine" as a NukkAI system name: **could not verify; no primary source found.**

---

## 5. Part D — Cooperative Hidden-Information Learning (Hanabi line) — Fish's Signalling Problem

Fish teammates communicate *only* through the choice of which card to ask from which opponent (and through declarations and pass-the-turn choices). That is a legal-action communication channel, listened to by adversaries. This is structurally Hanabi's hint channel plus an eavesdropper.

### 5.1 The Hanabi benchmark (Bard et al., AIJ 2020)

Two settings: **self-play** (you control all agents; pre-coordination allowed) and **ad-hoc team play** (unknown partners). Self-play baselines:

| Agent | 2p | 3p | 4p | 5p |
|---|---|---|---|---|
| ACHA (unlimited) | 22.73 | 20.24 | 21.57 | 16.80 |
| Rainbow (100M) | 20.64 | 18.71 | 18.00 | 15.26 |
| BAD | 23.92 | — | — | — |
| SmartBot (hand-coded) | 22.99 | 23.12 | 22.19 | 20.25 |

Stated open problems: escaping **babbling equilibria** (communication collapses to nothing); **ad-hoc brittleness** (independently trained agents score near zero together); scaling theory-of-mind past 2 players; and the failure of standard exploration for *entangled* policies where an action's meaning depends on the global convention rather than its immediate consequence. **All four apply verbatim to Fish.**

### 5.2 Bayesian Action Decoder (Foerster et al., ICML 2019)

A **public agent** samples a *deterministic partial policy* $\pi_\Delta$ mapping private observations to actions; the acting agent then executes $\pi_\Delta(f^a_t)$ using its private info. Because $\pi_\Delta$ is public, observing the action is a clean Bayesian filter:
$$P(f^a_t \mid u^a_t, \mathcal B_t, f^{\mathrm{pub}}_t, \pi_\Delta) \;\propto\; \mathbb 1\big[\pi_\Delta(f^a_t) = u^a_t\big]\; P(f^a_t \mid \mathcal B_t, f^{\mathrm{pub}}_t).$$
Beliefs are factorized for tractability: $P(f^{\mathrm{pri}}_t \mid f^{\mathrm{pub}}_{\le t}) \approx \prod_i P(f^{\mathrm{pri}}_t[i] \mid f^{\mathrm{pub}}_{\le t})$. The result is a **public-belief MDP** whose transitions depend on *counterfactual* actions ($\pi_\Delta$'s prescriptions for private states that did not occur) — i.e. exactly the prescription/coordinator object of §2.5. BAD reached **24.174/25** in 2p Hanabi; the paper attributes ~40% of information transfer to conventions rather than grounded content.

### 5.3 Simplified Action Decoder (Hu & Foerster, ICLR 2020)

The exploration/interpretability tension: $\epsilon$-greedy noise blurs the Bayesian posterior with an $\epsilon$-dependent term. SAD's fix is almost trivially cheap: during training, the acting agent executes the exploratory action $u^a$ **but teammates additionally observe the greedy action $u^*$**. Conditioning on $u^*$ removes the exploration-induced distortion in the posterior. At execution time, actions are greedy, so $u^a = u^*$ and nothing changes. Implementation: distributed recurrent DQN + prioritized replay + double Q + VDN, plus an optional auxiliary task predicting card playability from the action-observation history. **Results:** 2p **24.01** (52.39% perfect), 3p 23.93, 4p 23.81, 5p 23.01 — new learned SOTA at less compute than BAD.

### 5.4 Other-Play (Hu, Lerer, Peysakhovich & Foerster, ICML 2020)

Self-play picks *arbitrary* symmetry-breaking conventions. Given the symmetry group $\Phi$ of payoff-irrelevant relabelings,
$$\pi^* = \arg\max_{\pi}\ \mathbb E_{\phi \sim \Phi}\ J\big(\pi^1,\ \phi(\pi^2)\big).$$
Implemented as domain randomization over $\phi$. Intuition (lever game): 10 levers paying 1.0 and one paying 0.9 — SP picks a 1.0 lever and mis-coordinates; OP picks the unique 0.9 lever. **Results:** SP agents scored ~24 with themselves but **2.52 in cross-play**; OP scored **22.07 in cross-play**; with humans, OP 15.75 vs SP 9.15.

### 5.5 Off-Belief Learning (Hu, Lerer, Cui, Pineda, Brown & Foerster, ICML 2021)

Let $\pi_0$ be a fixed "grounding" policy. Define the **counterfactual belief**: the posterior over the hidden state assuming *all past actions were generated by $\pi_0$*,
$$\mathcal B_{\pi_0}(\xi \mid \tau) \;\propto\; P(\xi)\prod_{t' < t} \pi_0\big(a_{t'} \mid \tau_{t'}(\xi)\big).$$
The OBL target assumes the past came from $\pi_0$ but the *future* comes from $\pi_1$:
$$Q_1(\tau, a) \;=\; \mathbb E_{\xi \sim \mathcal B_{\pi_0}(\cdot\mid\tau)}\ \mathbb E_{\pi_1}\Big[\, r(\xi, a) + \max_{a'} Q_1(\tau', a') \,\Big].$$
**Theorem 1 (uniqueness):** OBL converges to the same policy regardless of initialization — the property that makes independent training runs interoperable. **Theorem 2:** policy improvement per iteration. **Theorem 4:** with $\pi_0$ uniform random, the fixed point is the optimal **grounded** policy — one that reads no conventions into partner actions. Iterating ($\pi_0 \leftarrow$ level-$k$ policy) yields hierarchical levels of theory-of-mind. **Results:** level 1 ≈ 21 self-play with *no* conventions; **level 4 = 24.10 self-play / 23.76 cross-play**.

### 5.6 Search at test time: SPARTA, LBS, MDS

**SPARTA** (Lerer, Hu, Foerster & Brown, AAAI 2020). Given a common-knowledge blueprint $\pi_b$, agent $i$ maintains an exact belief over hidden trajectories:
$$\mathcal B^i(\tau_t) \;=\; \frac{\mathcal B^i(\tau_{t-1})\,\pi^j(a^j_t \mid \tau_{t-1})\, P(o^i_t \mid \tau_{t-1}, a^j_t)}{\sum_{\tau'_{t-1}} \mathcal B^i(\tau'_{t-1})\,\pi^j(a^j_t\mid\tau'_{t-1})\,P(o^i_t\mid\tau'_{t-1},a^j_t)},$$
then evaluates each candidate action by Monte-Carlo rollouts with all others following $\pi_b$. **Multi-agent search:** all agents pre-agree on the blueprint *and* the search procedure (including RNG seed), and each replicates the searching agent's computation across all its possible AOHs, preserving common knowledge. **Theorem:** $V_{\pi_s} - V_{\pi_b} \ge -2T\Delta|A|N^{-1/2}$ with $N$ rollouts per step — search cannot hurt beyond a $\sqrt N$ term. **Results:** blueprint 24.08 → **24.61/25**, 75.5% perfect games. **Cost: ~2 core-hours per game for single-agent search; ~90 core-hours for retrospective multi-agent search.**

**Learned Belief Search** (Hu, Wu, Lerer, Foerster & Brown, 2021): replace the exact belief with a **learned autoregressive counterfactual belief model** trained supervised, plus a public-private policy architecture for cheap rollouts. Recovers **55–91% of exact search's benefit at 4.6–35.8× less compute.**

**Update-Equivalence / Mirror-Descent Search** (Sokota, D'Orazio, Kolter, Bard et al., 2023). Critique: PBS-based decision-time planning must spread its budget over *every* decision point in the PBS, so it degenerates to solving the whole game when public information is scarce. Instead, define a search algorithm to be **update-equivalent** to a last-iterate learning algorithm if the distribution it outputs at $h$ under $\pi$ matches what the learning algorithm would produce on the next iteration. Instantiating mirror descent gives **MDS** (KL-regularized toward the blueprint; Theorem 3.3 guarantees improvement in common-payoff games for small stepsizes) and magnetic mirror descent gives **MMDS** for adversarial games. **Results:** matches SPARTA/RLSearch in Hanabi with **two orders of magnitude less search time** and only approximate beliefs; cuts exploitability by >1/3 in 3×3 Abrupt Dark Hex and Phantom TTT with only 10-particle posteriors.

### 5.7 piKL / KL-regularized search

Jacob, Wu, Farina, Lerer, Hu, Bakhtin, Andreas & Brown (ICML 2022) regularize search toward an imitation-learned anchor $\tau$:
$$\pi_i^* = \arg\max_{\pi_i}\ \Big[\, u_i(\pi_i, \pi_{-i}) - \lambda_i\, \mathrm{KL}(\pi_i \,\|\, \tau_i) \,\Big].$$
The fixed point is an anchored/quantal-response-like equilibrium; as $\lambda \to \infty$ you recover the anchor, as $\lambda\to0$ you recover unregularized search. (The exact piKL-hedge update rule is **UNVERIFIED** here — PDF extraction failed; take it from §3 of the paper.) Directly useful for Fish if you want a bot that plays *with humans* who use a standard convention.

---

## 6. Part E — Applicability to Canadian Fish

### 6.1 The numbers that decide everything (computed)

| Quantity | Value |
|---|---|
| Total deals $54!/(9!)^6$ | $1.011\times10^{38}$ |
| Deals consistent with **one player's** hand $45!/(9!)^5$ | $1.901\times10^{28}$ |
| Deals consistent with the **whole team's** 27 cards $27!/(9!)^3$ | $\mathbf{2.279\times10^{11}}$ |
| Max legal asks per turn ($\le 9$ half-suits × 5 cards × 3 opponents) | $135$ |
| Declaration action space (half-suit × allocation) $9 \times 3^6$ | $6{,}561$ |
| Decisions per game | ~60–120 |

**Interpretation.** (a) Tabular anything is dead. (b) The *coordinator's* uncertainty at the start of a Fish hand is only $2.3\times10^{11}$ — and every ask/answer imposes a hard constraint that typically cuts this by 1–3 orders of magnitude, so within a few turns the coordinator's belief support is small enough to enumerate or particle-filter exactly. **This is the single most exploitable structural fact about Fish.** (c) Branching factor ~50–135 for asks is small by card-game standards (GuanDan: >5,000), so action-as-input DMC is very comfortable here.

### 6.2 Per-technique verdicts

**(1) TMECor as the equilibrium target — ADOPT (conceptually).**
Fish teammates can agree on a convention before play but cannot communicate during it. That is TMECor exactly. *What it buys:* a principled definition of "optimal", and a reason to build a *correlation device* — a shared, deterministic, publicly-known convention mapping (public history, own hand) → ask. *Cost:* computing an exact TMECor is FNP-hard and hopeless at full Fish scale. *Adaptation:* use TMECor as the target for **small solved sub-problems** (endgames with ≤3 unresolved half-suits; the "one team is cardless, other must declare everything" terminal phase; single-half-suit races) and as the *evaluation criterion* (measure exploitability of your bot's team policy against a best-responding opponent team). Never try to solve full Fish.

**(2) TB-DAG / TPI coordinator conversion — ADOPT for endgames only.**
*Would it help:* Yes, it is the only route that gives CFR+ a correctness guarantee in a team game, and it makes subgame solving legal for teams. *Cost:* the coordinator's action set is a prescription over all teammates' infosets in the public state. In Fish mid-game a public state contains all deals consistent with public history; the number of *distinct* team infosets in it is huge. Edges scale as $O^*(3^k)$ for $k$-private games, and Fish's $k$ (distinct last-infosets per public state) is far too large mid-game. *Adaptation:* apply the conversion to **abstracted endgame Fish**: restrict to $\le 2$ live half-suits and $\le 12$ unseen cards, abstract hands to "which cards of the live half-suits you hold" (a bitmask), and run CFR+/DCFR on the resulting TB-DAG. Expect solvable sizes comparable to the paper's 3K8 (1.8M DAG vertices, 4.7 s) — i.e. entirely feasible on 15 cores. Use the solved endgame values as the leaf evaluator for search earlier in the game.

**(3) Vanilla CFR / CFR+ / DCFR — ADOPT for abstracted subgames; REJECT for the full game.**
*Cost:* per-iteration cost is $O(|\mathcal I|\cdot|A|)$; with $10^{28}$ infosets per player this is not a discussion. *Adaptation:* run CFR+ with $\mathrm{DCFR}_{3/2,0,2}$ on (i) single-half-suit races, (ii) 2-half-suit endgames, (iii) declaration-timing sub-games. Use RM+ with linear averaging. Use DCFR rather than CFR+ **because Fish has severe blunder actions** — a wrong declaration hands a whole set to the opponents — and CFR+ is documented to do relatively poorly exactly in games with very costly mistake actions.

**(4) MCCFR (external sampling) — CONDITIONAL.**
*Would it help:* ES is the right sampler if you do run CFR on abstracted Fish; it enumerates the traverser's actions (≤135, fine) and samples chance + opponents. *Pitfall:* **outcome sampling is unusable** — the $1/q(z)$ importance weight over a 60–120-step horizon has catastrophic variance, and the MCCFR bound carries a $1/\sqrt\delta$ factor. If you need a model-free sampler, use **ESCHER's** estimator (no importance sampling at all) rather than DREAM's.

**(5) Deep CFR / DREAM / ESCHER — REJECT for the primary agent on this budget; consider ESCHER for research.**
*Cost reality:* Deep CFR's ablations require *retraining the advantage network from scratch every CFR iteration*, and its error bound degrades as $\sqrt{\varepsilon_{\mathcal L}}$. DREAM's exploration term is exponential in depth $d_i$; Fish's depth is 60–120. On 15 CPU cores with no GPU you will not get within an order of magnitude of the sample counts these methods need. *Also fatal:* none of them is sound in a 3v3 team game without the coordinator conversion first, and the conversion is what you cannot afford at full scale. **ESCHER** is the one worth prototyping on abstracted Fish because its variance profile is the only one compatible with deep games.

**(6) Magnetic Mirror Descent — ADOPT as the CFR replacement.**
$\pi_{t+1} \propto [\pi_t \rho^{\eta\alpha} e^{\eta q_t}]^{1/(1+\eta\alpha)}$ is a one-line update with **last-iterate** convergence, no reservoir buffers, no average-strategy network, and cost identical to plain mirror descent. On a CPU-only budget this is the highest-quality-per-FLOP equilibrium-ish learner available, and its "magnet" $\rho$ doubles as the mechanism for anchoring to a hand-written Fish convention (see (12)). *Pitfall:* its guarantees are 2p0s; in 3v3 it is a heuristic. Use it on the *coordinator-converted* endgames for guarantees, and as a heuristic learner elsewhere.

**(7) ReBeL / Student of Games — REJECT as-is; STEAL the PBS idea.**
*Cost:* thousands of actors. *But:* Fish's public belief state is unusually clean — because every action is public, the PBS is a distribution over the $\le 2.3\times10^{11}$ (team view) or $1.9\times10^{28}$ (single-player view) consistent deals, updated by *hard constraints* rather than soft Bayesian smearing. Concretely, an ask "$P_i$ asks $P_j$ for card $c$" implies: $P_i$ does **not** hold $c$; $P_i$ **does** hold $\ge 1$ other card of $c$'s half-suit; and (with a convention-dependent prior) $P_i$ believes $P_j$ likely holds $c$. A "no" answer implies $P_j$ does **not** hold $c$. These are exactly representable as a bipartite assignment feasibility problem (cards → players with capacity = hand size). **Use exact constraint propagation + weighted sampling (or a permanent/Sinkhorn approximation of the assignment polytope) as your belief module.** This is the Fish equivalent of ReBeL's PBS, and it is cheap.

**(8) DouZero-style Deep Monte-Carlo with action-as-input — ADOPT. This is the backbone.**
*Fit:* Fish's reward is terminal and sparse (sets scored at declaration); $\gamma = 1$ MC returns are natural; the action space is combinatorial and structured (which card, from whom), which action-as-input handles natively. *Concrete encoding for Fish:* state = (own 9-card bitmask over 54; per-player public hand counts; per-(player, half-suit) known-holds / known-lacks matrices derived from ask history; sets already declared and by whom; whose turn), action = (target player one-hot ×5, card one-hot ×54, half-suit one-hot ×9, plus derived features: "do I hold $k$ cards of this half-suit", "how many of this half-suit are unaccounted", "posterior probability target holds this card"). Feed the ask history through a GRU/LSTM as in DouZero. Network: 5–6 MLP layers × 256–512 → scalar $Q(s,a)$. *Compute:* see §6.3 — this fits in ~1–2 weeks on 15 cores.
*Pitfall:* DMC learns $Q$ under the *current* self-play distribution; in a team game it will happily converge to a local team equilibrium with a degenerate (babbling) ask convention. Mitigate with (10) and (11).

**(9) DanZero+ two-stage (DMC → top-$k$ PPO) — ADOPT as the refinement stage.**
Use the DMC $Q$ to filter to the top-$k$ ($k \approx 5$–10) asks, then run PPO over that reduced set with a proper policy. Documented gain in GuanDan: 90.12 → 92.70 win rate vs the strongest baseline. Cheap, and PPO over $k$ actions is trivial on CPU.

**(10) PerfectDou PTIE / Suphx oracle guiding — ADOPT with the annealing schedule.**
Fish is *ideal* for a perfect-information critic: the value network can see all six hands (that is just the deal), while the policy sees only its own hand + public history. This drastically reduces critic variance in a game whose reward is decided 60+ steps later. **Use Suphx's dropout schedule $\delta_t$ on the privileged features rather than PerfectDou's hard split**, so the critic degrades toward an honest imperfect-information value and the bias does not lock in. Also steal Suphx's **global reward prediction**: train a small GRU to predict final set-count differential from mid-game state, and use it as a shaped return instead of the raw ±1.
*Pitfall:* a perfect-information critic systematically *under*-values information-gathering asks (it already knows the answer), which is the exact behaviour Fish needs to learn. Anneal it out, and add an explicit information-gain term (see (14)).

**(11) Other-Play / Off-Belief Learning — ADOPT OBL; ADOPT OP only if you care about ad-hoc partners.**
*Why OBL matters for Fish specifically:* Fish's ask channel will otherwise produce arbitrary, self-play-specific conventions that (a) don't survive a fresh training run, (b) are exploitable once an opponent models them, and (c) are the "babbling equilibrium" failure mode from the Hanabi challenge. OBL's counterfactual belief $\mathcal B_{\pi_0}$ is directly computable in Fish because the belief is over deals and the past-action likelihood under $\pi_0$ is cheap. Level-1 OBL with $\pi_0$ = uniform-over-legal-asks gives you an optimal **grounded** Fish policy — one that treats every ask as pure evidence about the asker's holdings ("must hold a card of that half-suit; must not hold the asked card") and reads *no* extra convention into it. Then level-2, level-3 add controlled theory-of-mind. *Cost:* each OBL level is a full training run; budget 3–4 levels. Hanabi got 21 → 24.10 across levels 1→4.
*OP:* the natural Fish symmetry group $\Phi$ includes suit relabelings (♠↔♥↔♦↔♣) and the joker↔joker swap — but **not** rank relabelings (ranks are ordered only in that half-suits are 2–7 and 9–A; within a half-suit, ranks are interchangeable *for the ask mechanic* but not for human convention). Randomizing over suit permutations during training is nearly free and should improve robustness. Careful: seat/team symmetries are also present and should be randomized.

**(12) SPARTA / Learned Belief Search / Mirror-Descent Search — ADOPT MDS. Highest ROI at test time.**
Fish's structure makes belief-conditioned search unusually cheap: beliefs are constraint sets over deals, and a rollout is fast. *Recipe:* (i) train a blueprint $\pi_b$ by DMC; (ii) at each decision, sample $N$ deals from the constrained posterior (importance-weight by consistency with $\pi_b$'s action likelihoods, à la SPARTA's $\mathcal B^i$ update); (iii) evaluate candidate asks by rollout under $\pi_b$; (iv) instead of raw argmax (SPARTA), use **MDS**: $\pi_{\text{search}} \propto \pi_b \exp(\eta \hat q)$, i.e. a KL-regularized improvement. MDS matched SPARTA in Hanabi at ~100× less search time and tolerates approximate beliefs (10 particles!). *Critical Fish-specific constraint:* to preserve common knowledge (SPARTA's multi-agent condition), **all three teammates must run the identical search procedure with an agreed seed**, and the search must be a deterministic function of *public* information plus own hand. If only one seat searches, your teammates' beliefs about you go stale and coordination degrades.
*Cost:* SPARTA in Hanabi cost 2 core-hours/game single-agent, 90 core-hours multi-agent. Fish rollouts are cheaper (shorter, smaller branching) but you must budget: with 15 cores and $N=200$ particles × 30 candidate asks × 1 rollout each = 6,000 rollouts/decision × ~1 ms = 6 s/decision. Too slow for a 100-decision game at full width. **Use MDS with $N \approx 20$–50 particles and top-$k$ candidate filtering from the DMC $Q$** → ~0.1–0.3 s/decision. That is the practical operating point.

**(13) Joint Policy Search — ADOPT. The best "learn a convention" tool in the literature for this exact problem.**
Fish's core difficulty is that improving the team's ask convention requires *simultaneous* changes at several infosets: the asker must start asking $X$ in situation $S$ **and** the partner must start reading $X$ as meaning $Y$. No single-agent policy gradient will find this — it is exactly the local-optimum trap FXP proves self-play falls into. JPS computes the exact value delta of a simultaneous multi-infoset change in $O(|S|+M)$ using the vanishing-$\rho$ property, and its sample-based variant handles $10^{11}$-scale hidden state (bridge). *Adaptation to Fish:* define candidate joint changes over **abstract** infoset classes rather than raw infosets — e.g. "when I hold exactly 1 card of half-suit $H$ and it's my first ask of the hand, ask the *lowest* missing card of $H$ from the opponent to my left" paired with "when a teammate does that, infer they hold exactly 1 card of $H$". Search over a hand-authored library of such convention-pairs with JPS scoring on sampled deals. This gives you *interpretable, human-transferable* conventions — a big practical win.
*Precedent:* +0.29 → +0.63 IMPs/board vs WBridge5. Bridge bidding is the closest published problem to Fish asking.

**(14) PIMC / determinization — USE AS BASELINE ONLY, and know why it caps out.**
*Why it's tempting in Fish:* the **disambiguation factor is very high** — every ask and answer is a hard logical constraint, so hidden information resolves fast, which is the regime Long et al. identify as favourable for PIMC. *Why it caps out:*
- **Strategy fusion.** PIMC solves each sampled deal with perfect information, so it assumes it will know which deal it's in at every future node and can play a different (perfect) line in each. It therefore *never values information-gathering asks*, and it will happily "declare" in the determinization because in that determinization it knows the allocation. In Fish, correctly timing a declaration under uncertainty is a large fraction of the skill. PIMC is structurally blind to it.
- **Non-locality.** Opponents' reach probabilities elsewhere in the tree (i.e., what their earlier asks reveal about their reasoning) change the value here; PIMC ignores this.
- **No convention.** PIMC assumes teammates already know the deal, so it can never invent or exploit a signalling convention. In a game where signalling is half the value, this is a hard ceiling.
*Verdict:* build it (it's 200 lines and will beat casual humans), use it as the sparring partner and as the rollout policy inside search, but do not ship it as the agent. **ISMCTS** (Cowling, Powley & Whitehouse, IEEE TCIAIG 2012) — searching trees of *information sets* rather than states, with the multiple-observer variant — is the correct fix for strategy fusion and is a reasonable middle rung.

**(15) NFSP — REJECT.** Dominated on sample efficiency by everything above; no team guarantee. Keep as a 50-line sanity baseline only.

**(16) Team-PSRO / FXP — ADOPT FXP's structural insight; TEAM-PSRO only if you have spare compute.**
FXP's theorem (SP with preference-preserving updates converges to *local* team equilibria w.h.p.) is the diagnosis for "my Fish bot's asks are meaningless". *Cheap adaptation:* maintain a small **counter-population**: freeze checkpoints of your main team policy, and periodically train a *fresh cooperative team* to best-respond to a frozen opponent checkpoint, then fold it back in with cross-play probability $1-\eta$ ($\eta \approx 0.7$–0.8). This is maybe 30% extra compute and directly attacks the local-optimum problem. Full Team-PSRO (population + meta-solver + joint BR oracle per iteration) is 5–20× more expensive; skip on 15 cores.

### 6.3 Realistic compute budget on one 15-core CPU machine

Assume a tight simulator (Rust/C++ or numpy-vectorized), a 5-layer × 384 MLP (~0.5M params) with batched inference, and ~100 decisions/game.

| Component | Estimate |
|---|---|
| Decision evaluations/s/core (batched, small MLP) | 300–1,000 |
| Decisions/day (15 cores, 80% util) | $3\times10^8$ – $1\times10^9$ |
| Self-play games/day | $3\times10^6$ – $1\times10^7$ |
| Games in a 10-day run | $3\times10^7$ – $1\times10^8$ |
| DouZero's published budget (48 cores + 4 GPUs × 30 d) | roughly 10–30× more |
| PerfectDou's published sample budget | $2.5\times10^9$ samples ≈ $2.5\times10^7$ games — **achievable in ~3–8 days here** |

**Conclusions.** (a) A PerfectDou-scale run is *just* affordable; a DouZero-scale run is not, but PerfectDou reported 10× sample efficiency over DouZero precisely from the perfect-information critic, which is the technique you should copy. (b) Deep CFR/DREAM at poker scale (billions of ES traversals with per-iteration network retraining) is not affordable. (c) Test-time search is where your compute should go: search buys more strength per FLOP than training (Hanabi: 24.08 → 24.61 from search alone), and MDS makes it ~100× cheaper than SPARTA. (d) Keep the network small — you are CPU-bound on inference, and DouZero's 6×512 MLP is already at the edge. Consider a 3×256 network plus richer hand-engineered features (posterior probabilities from the belief module) instead of a bigger network.

---

## 7. Pitfalls, Negative Results, and Known Failure Modes

1. **Independent self-play converges to *local* team equilibria.** Proved for preference-preserving updates in mixed cooperative-competitive games (Xu et al., FXP). Symptom in Fish: asks carry no information beyond the logically forced content; teammates never develop conventions. Fix: cross-play/counter-population (FXP), JPS, or OBL levels.
2. **Babbling equilibria.** Named as an open problem in the Hanabi Challenge. Communication collapses because unilateral deviation to a signalling policy is punished before the partner learns to read it. Same fix set.
3. **Ad-hoc brittleness.** Independently trained Hanabi self-play agents score **2.52** in cross-play vs ~24 in self-play (Other-Play paper). If you train two Fish teams separately and swap a member, expect collapse. OP/OBL fix this; plain self-play does not.
4. **Outcome-sampling MCCFR variance.** The $1/q(z)$ weight over a 60–120-decision horizon makes the estimator useless; MCCFR's bound carries $1/\sqrt\delta$. Use external sampling or ESCHER.
5. **Deep CFR's $\sqrt{\varepsilon_{\mathcal L}}$ term.** Function-approximation error enters the regret bound under a square root — modest network error translates to large exploitability. Also: the ablations show you must retrain from scratch each iteration (fine-tuning is worse), multiplying cost.
6. **DREAM's exponential-in-depth exploration term** $(|\mathcal A_i|/\epsilon)^{d_i}$. Fatal in deep games like Fish.
7. **CFR+ underperforms when some actions are catastrophic mistakes** (Brown & Sandholm). Fish declarations are exactly such actions (a wrong declaration gifts the set). Use DCFR$_{3/2,0,2}$.
8. **CFR+ interacts badly with abstraction and pruning** — relevant because any Fish CFR must be abstracted.
9. **Perfect-information critics are biased.** PerfectDou/oracle-guiding work empirically but are not equilibrium-sound and specifically undervalue information-gathering. Anneal (Suphx $\delta_t$) and/or add explicit information-gain shaping.
10. **PIMC's strategy fusion and non-locality** (Frank & Basin; Long et al.). In Fish these manifest as never asking to gather information and mistimed declarations.
11. **PBS-based decision-time planning degenerates when public information is scarce** (Sokota et al.). Fish is the *opposite* case — public info is abundant — so this critique bites less, but note the corollary: because Fish's public state is so informative, the *belief* is narrow and PBS methods are unusually cheap. Exploit that.
12. **BR-T is APX-hard**, and TMECor is FNP-hard, TME FNP-hard and additively inapproximable. Do not promise "we will compute the optimal team strategy."
13. **The TB-DAG / TPI blowup is $O^*(3^k)$ in $k$-privateness.** Fish's mid-game $k$ is large. Only endgames are convertible.
14. **Multi-agent search breaks common knowledge if only one seat searches** (SPARTA). All three Fish teammates must run identical, publicly-agreed search with an agreed seed, or beliefs desynchronize.
15. **No published academic work on Fish/Literature exists.** Searches returned only a hobby GitHub project (`iuruoy-shao/fish`) and general card-game RL toolkits (RLCard). You are building from first principles; the transferable literature is GuanDan (2v2 team, no comms), Bridge bidding (partner signalling through legal actions with an eavesdropper), and Hanabi (convention formation).
16. **Unverified items in this report:** exact MCCFR regret-bound constants; exact JPS $c^{(\sigma,\sigma')}$ definition; exact piKL-hedge update; the per-game leaf-correlation/bias/disambiguation numbers for Skat/Hearts/Bridge (two extraction passes disagreed — read the AAAI PDF directly); NooK's internal mechanism and the existence of a NukkAI system named "Lorraine".

---

## 8. Recommended Stack for the Fish Bot (synthesis)

1. **Belief module (exact, not learned).** Maintain the constraint set over deals: per-(player, card) must-hold / cannot-hold / unknown, plus hand-size capacities. Sample consistent deals by constrained rejection or Sinkhorn/permanent-based weighted assignment sampling. This is Fish's PBS and it is cheap because all actions are public.
2. **Blueprint.** DouZero-style **Deep Monte-Carlo, action-as-input**, GRU over ask history, 4–6 MLP layers × 256–512, $\gamma=1$, $\epsilon = 0.01$. Feed the belief module's posteriors in as features. Add a **perfect-information critic annealed out on a Suphx-style schedule** and a **global reward predictor**.
3. **Anti-local-optimum.** FXP-style counter-population cross-play ($\eta \approx 0.75$), plus **OBL levels 1→3** to force a grounded-then-conventional policy that is reproducible across runs.
4. **Convention layer.** Hand-author a library of candidate ask/read convention pairs; score and select them with **sample-based JPS** on held-out deals. Keep them interpretable so humans can play with the bot.
5. **Test-time search.** **Mirror-Descent Search** over the top-$k$ asks from the DMC $Q$, with 20–50 belief particles, KL-anchored to the blueprint. All three teammates run the identical procedure with an agreed seed.
6. **Endgame solver.** When ≤2 half-suits remain live and ≤12 cards are unseen, build the **TB-DAG / TPI coordinator game** and solve it with **DCFR$_{3/2,0,2}$** to near-exact TMECor. Use these values as search leaf evaluations.
7. **Evaluation.** Measure (a) win rate vs PIMC and vs rule-based baselines; (b) **cross-play** between independently trained teams (the Hanabi 2.52-vs-24 test); (c) **approximate exploitability** by training a best-responding opponent team with RL against a frozen team policy; (d) endgame agreement with the exact TMECor solver.

---

## 9. Bibliography

**Team-game equilibrium theory and algorithms**

1. Andrea Celli, Nicola Gatti. *Computational Results for Extensive-Form Adversarial Team Games.* AAAI 2018 (arXiv:1711.06930). https://arxiv.org/abs/1711.06930 · https://ojs.aaai.org/index.php/AAAI/article/view/11462
2. Gabriele Farina, Andrea Celli, Nicola Gatti, Tuomas Sandholm. *Ex ante coordination and collusion in zero-sum multi-player extensive-form games.* NeurIPS 2018. (Located via citation; abstract page not fetched — **UNVERIFIED** details.)
3. Brian Hu Zhang, Gabriele Farina, Andrea Celli, Tuomas Sandholm. *Team Belief DAG: Generalizing the Sequence Form to Team Games for Fast Computation of Correlated Team Max-Min Equilibria via Regret Minimization.* arXiv:2202.00789 (ICML 2023). https://arxiv.org/abs/2202.00789
4. Luca Carminati, Federico Cacciamani, Marco Ciccone, Nicola Gatti. *A Marriage between Adversarial Team Games and 2-player Games: Enabling Abstractions, No-regret Learning, and Subgame Solving.* ICML 2022, PMLR 162 (arXiv:2206.09161). https://proceedings.mlr.press/v162/carminati22a.html · https://arxiv.org/abs/2206.09161
5. Brian Hu Zhang, Gabriele Farina, Andrea Celli, Tuomas Sandholm. *Subgame Solving in Adversarial Team Games.* NeurIPS 2022. https://www.mit.edu/~gfarina/2022/subgame_solving_teams_neurips22/subgame_solving_teams_neurips22.pdf (PDF text extraction failed — **contents UNVERIFIED**)
6. Stephen McAleer, Gabriele Farina, Gaoyue Zhou, Mingzhi Wang, Yaodong Yang, Tuomas Sandholm. *Team-PSRO for Learning Approximate TMECor in Large Team Games via Cooperative Reinforcement Learning.* NeurIPS 2023. https://proceedings.neurips.cc/paper_files/paper/2023/hash/8e4ccc9ca6ae2225c4cbb7782ab48daf-Abstract-Conference.html
7. *Leveraging Team Correlation for Approximating Equilibrium in Two-Team Zero-Sum Games* (rCTME, S-PSRO). arXiv:2403.00255, 2024. https://arxiv.org/html/2403.00255
8. Zelai Xu, Yancheng Liang, Chao Yu, Yu Wang, Yi Wu. *Fictitious Cross-Play: Learning Global Nash Equilibrium in Mixed Cooperative-Competitive Games.* AAMAS 2023 (arXiv:2310.03354). https://arxiv.org/abs/2310.03354
9. Andrea Celli, Marco Ciccone, et al. *Coordination in Adversarial Sequential Team Games via Multi-Agent Deep Reinforcement Learning* (Soft Team Actor-Critic). arXiv:1912.07712, 2019. https://arxiv.org/abs/1912.07712
10. *Team-Fictitious Play for Reaching Team-Nash Equilibrium in Multi-team Games.* NeurIPS 2024. https://proceedings.neurips.cc/paper_files/paper/2024/file/c9fd326fd03eaf52f672c31cde9658af-Paper-Conference.pdf (abstract only — **details UNVERIFIED**)
11. Naman Aggarwal, Jonathan P. How. *Beyond Bayesian Nash: Learning Minimax-Regret Equilibria for Adversarial Team Games under Asymmetric Information.* arXiv:2607.09993, July 2026. https://arxiv.org/abs/2607.09993
12. *A Generic Multi-Player Transformation Algorithm for Solving Large-Scale Zero-Sum Extensive-Form Adversarial Team Games.* arXiv:2307.01441. https://arxiv.org/abs/2307.01441 (**UNVERIFIED** — listing only)
13. *PRR-TM: finding equilibria in adversarial team games via perfect-recall refinement and teammate modeling.* Int. J. Machine Learning and Cybernetics, 2025. https://link.springer.com/article/10.1007/s13042-025-02607-y (**UNVERIFIED** — listing only)

**CFR family**

14. Martin Zinkevich, Michael Johanson, Michael Bowling, Carmelo Piccione. *Regret Minimization in Games with Incomplete Information.* NIPS 2007. (Foundational; equations reproduced here are standard.)
15. Oskari Tammelin. *Solving Large Imperfect Information Games Using CFR+.* arXiv:1407.5042, 2014. https://arxiv.org/abs/1407.5042
16. Marc Lanctot, Kevin Waugh, Martin Zinkevich, Michael Bowling. *Monte Carlo Sampling for Regret Minimization in Extensive Games.* NIPS 2009, pp. 1078–1086. https://proceedings.neurips.cc/paper_files/paper/2009/file/00411460f7c92d2124a67ea0f4cb5f85-Paper.pdf (exact theorem constants **UNVERIFIED** — extraction failed)
17. Noam Brown, Tuomas Sandholm. *Solving Imperfect-Information Games via Discounted Regret Minimization.* AAAI 2019 (arXiv:1809.04040). https://arxiv.org/abs/1809.04040
18. Noam Brown, Adam Lerer, Sam Gross, Tuomas Sandholm. *Deep Counterfactual Regret Minimization.* ICML 2019 (arXiv:1811.00164). https://arxiv.org/abs/1811.00164
19. Eric Steinberger, Adam Lerer, Noam Brown. *DREAM: Deep Regret minimization with Advantage baselines and Model-free learning.* arXiv:2006.10410, 2020. https://arxiv.org/abs/2006.10410
20. Stephen McAleer, Gabriele Farina, Marc Lanctot, Tuomas Sandholm. *ESCHER: Eschewing Importance Sampling in Games by Computing a History Value Function to Estimate Regret.* ICLR 2023 (arXiv:2206.04122). https://arxiv.org/abs/2206.04122
21. Samuel Sokota, Ryan D'Orazio, J. Zico Kolter, Nicolas Loizou, Marc Lanctot, Ioannis Mitliagkas, Noam Brown, Christian Kroer. *A Unified Approach to Reinforcement Learning, Quantal Response Equilibria, and Two-Player Zero-Sum Games* (Magnetic Mirror Descent). ICLR 2023 (arXiv:2206.05825). https://arxiv.org/abs/2206.05825

**Search + RL over belief states**

22. Noam Brown, Anton Bakhtin, Adam Lerer, Qucheng Gong. *Combining Deep Reinforcement Learning and Search for Imperfect-Information Games* (ReBeL). NeurIPS 2020 (arXiv:2007.13544). https://arxiv.org/abs/2007.13544
23. Martin Schmid, Matej Moravčík, Neil Burch, Rudolf Kadlec, Josh Davidson, Kevin Waugh, Nolan Bard, Finbarr Timbers, Marc Lanctot, et al. *Student of Games: A unified learning algorithm for both perfect and imperfect information games.* Science Advances, 2023, DOI 10.1126/sciadv.adg3256 (arXiv:2112.03178). https://arxiv.org/abs/2112.03178 · https://www.science.org/doi/10.1126/sciadv.adg3256
24. Vojtěch Kovařík, Martin Schmid, Neil Burch, Michael Bowling, Viliam Lisý. *Rethinking Formal Models of Partially Observable Multiagent Decision Making / Search in Imperfect Information Games.* arXiv:2111.05884. https://arxiv.org/abs/2111.05884
25. Adam Lerer, Hengyuan Hu, Jakob Foerster, Noam Brown. *Improving Policies via Search in Cooperative Partially Observable Games* (SPARTA). AAAI 2020 (arXiv:1912.02318). https://arxiv.org/abs/1912.02318
26. Hengyuan Hu, David J. Wu, Adam Lerer, Jakob Foerster, Noam Brown. *Learned Belief Search: Efficiently Improving Policies in Partially Observable Settings.* arXiv:2106.09086, 2021. https://arxiv.org/abs/2106.09086
27. Samuel Sokota, Gabriele Farina, David J. Wu, Hengyuan Hu, Kevin A. Wang, J. Zico Kolter, Noam Brown. *The Update-Equivalence Framework for Decision-Time Planning.* arXiv:2304.13138, 2023. https://arxiv.org/abs/2304.13138
28. Athul Paul Jacob, David J. Wu, Gabriele Farina, Adam Lerer, Hengyuan Hu, Anton Bakhtin, Jacob Andreas, Noam Brown. *Modeling Strong and Human-Like Gameplay with KL-Regularized Search* (piKL). ICML 2022, PMLR 162:9695–9728 (arXiv:2112.07544). https://proceedings.mlr.press/v162/jacob22a/jacob22a.pdf (update equations **UNVERIFIED** — extraction failed)

**Cooperative hidden-information / Hanabi**

29. Nolan Bard, Jakob N. Foerster, Sarath Chandar, Neil Burch, Marc Lanctot, H. Francis Song, Emilio Parisotto, Vincent Dumoulin, Subhodeep Moitra, Edward Hughes, Iain Dunning, Shibl Mourad, Hugo Larochelle, Marc G. Bellemare, Michael Bowling. *The Hanabi Challenge: A New Frontier for AI Research.* Artificial Intelligence 280 (2020) (arXiv:1902.00506). https://arxiv.org/abs/1902.00506
30. Jakob N. Foerster, Francis Song, Edward Hughes, Neil Burch, Iain Dunning, Shimon Whiteson, Matthew Botvinick, Michael Bowling. *Bayesian Action Decoder for Deep Multi-Agent Reinforcement Learning.* ICML 2019 (arXiv:1811.01458). https://arxiv.org/abs/1811.01458
31. Hengyuan Hu, Jakob N. Foerster. *Simplified Action Decoder for Deep Multi-Agent Reinforcement Learning.* ICLR 2020 (arXiv:1912.02288). https://arxiv.org/abs/1912.02288
32. Hengyuan Hu, Adam Lerer, Alex Peysakhovich, Jakob Foerster. *"Other-Play" for Zero-Shot Coordination.* ICML 2020 (arXiv:2003.02979). https://arxiv.org/abs/2003.02979
33. Hengyuan Hu, Adam Lerer, Brandon Cui, Luis Pineda, David Wu, Noam Brown, Jakob Foerster. *Off-Belief Learning.* ICML 2021 (arXiv:2103.04000). https://arxiv.org/abs/2103.04000
34. Brandon Cui, Hengyuan Hu, Luis Pineda, Jakob Foerster. *K-level Reasoning for Zero-Shot Coordination in Hanabi.* NeurIPS 2021. https://proceedings.neurips.cc/paper/2021/file/4547dff5fd7604f18c8ee32cf3da41d7-Paper.pdf (**UNVERIFIED** — listing only)

**Self-play RL for card games**

35. Johannes Heinrich, David Silver. *Deep Reinforcement Learning from Self-Play in Imperfect-Information Games* (NFSP). arXiv:1603.01121, 2016. https://arxiv.org/abs/1603.01121
36. Daochen Zha, Jingru Xie, Wenye Ma, Sheng Zhang, Xiangru Lian, Xia Hu, Ji Liu. *DouZero: Mastering DouDizhu with Self-Play Deep Reinforcement Learning.* ICML 2021 (arXiv:2106.06135). https://arxiv.org/abs/2106.06135
37. Youpeng Zhao, Yudong Lu, Jian Zhao, Wengang Zhou, Houqiang Li, et al. *DanZero+: Dominating the GuanDan Game through Reinforcement Learning.* arXiv:2312.02561, 2023. https://arxiv.org/abs/2312.02561
38. Yang Guan, Minghuan Liu, Weijun Hong, Weinan Zhang, Fei Fang, Guangjun Zeng, Yue Lin. *PerfectDou: Dominating DouDizhu with Perfect Information Distillation.* NeurIPS 2022 (arXiv:2203.16406). https://arxiv.org/abs/2203.16406
39. Junjie Li, Sotetsu Koyamada, Qiwei Ye, Guoqing Liu, Chao Wang, Ruihan Yang, Li Zhao, Tao Qin, Tie-Yan Liu, Hsiao-Wuen Hon. *Suphx: Mastering Mahjong with Deep Reinforcement Learning.* arXiv:2003.13590, 2020. https://arxiv.org/abs/2003.13590
40. Daochen Zha, Kwei-Herng Lai, Yuanpu Cao, Songyi Huang, Ruzhe Wei, Junyu Guo, Xia Hu. *RLCard: A Toolkit for Reinforcement Learning in Card Games.* arXiv:1910.04376. https://arxiv.org/abs/1910.04376

**Bridge**

41. Jiang Rong, Tao Qin, Bo An. *Competitive Bridge Bidding with Deep Neural Networks.* AAMAS 2019, pp. 16–24 (arXiv:1903.00900). https://arxiv.org/abs/1903.00900 · https://aamas.csc.liv.ac.uk/Proceedings/aamas2019/pdfs/p16.pdf
42. Yuandong Tian, Qucheng Gong, Tina Jiang. *Joint Policy Search for Multi-agent Collaboration with Imperfect Information.* NeurIPS 2020 (arXiv:2008.06495). https://arxiv.org/abs/2008.06495 · https://proceedings.neurips.cc/paper/2020/file/e64f346817ce0c93d7166546ac8ce683-Paper.pdf
43. NukkAI / NooK bridge challenge, Paris, March 2022 — 800 deals in 80 sets of 10, card-play only, NooK won 67/80 sets (83%) against 8 world champions. Press coverage only; no primary technical publication found. https://www.cbc.ca/radio/asithappens/as-it-happens-the-wednesday-edition-1.6402751/an-artificial-intelligence-just-beat-8-world-champions-at-bridge-1.6402861 · https://singularityhub.com/2022/04/03/a-hybrid-ai-just-beat-eight-world-champions-at-bridge-and-explained-how-it-did-it/ — **mechanism UNVERIFIED**. A NukkAI system named "Lorraine": **NOT FOUND / UNVERIFIED.**
44. *Human-Agent Cooperation in Bridge Bidding.* arXiv:2011.14124. https://arxiv.org/abs/2011.14124 (**UNVERIFIED** — listing only)

**Search under imperfect information (baselines and failure modes)**

45. Jeffrey Long, Nathan Sturtevant, Michael Buro, Timothy Furtak. *Understanding the Success of Perfect Information Monte Carlo Sampling in Game Tree Search.* AAAI 2010, 24(1):134–140. https://ojs.aaai.org/index.php/AAAI/article/view/7562 · https://cdn.aaai.org/ojs/7562/7562-13-11092-1-2-20201228.pdf — parameter definitions summarized here; **exact per-game leaf-correlation/bias/disambiguation values UNVERIFIED** (two extraction passes disagreed).
46. Peter I. Cowling, Edward J. Powley, Daniel Whitehouse. *Information Set Monte Carlo Tree Search.* IEEE Transactions on Computational Intelligence and AI in Games 4(2):120–143, 2012. DOI 10.1109/TCIAIG.2012.2200894. https://eprints.whiterose.ac.uk/id/eprint/75048/1/CowlingPowleyWhitehouse2012.pdf

**Fish/Literature specifically**

47. No peer-reviewed work located. Only hobby project: `iuruoy-shao/fish` — *Teaching a Reinforcement Learning agent to play Fish.* https://github.com/iuruoy-shao/fish (**UNVERIFIED**, not inspected in depth.)
