# Exact and Approximate Inference over Card Deals for Canadian Fish

**Area:** permanents, contingency tables, constrained deal sampling, and policy-conditioned deal weighting
**Audience:** engineer implementing the belief module of a Canadian Fish (Literature) agent
**Status:** literature review + original computational experiments (all experiments in §8 were run for this report; code paths noted)

---

## 1. Executive summary

1. **The Fish belief problem is not #P-hard, and does not need a permanent approximation at all.** The hidden state is a partition of 54 cards into 6 labelled hands of exactly 9. Because the number of *bins* (players) is a constant 6 and each capacity is a constant 9, the normalising constant and all per-card marginals are computable **exactly** by a dynamic program over the capacity vector $n=(n_1,\dots,n_6)$ with $\prod_q (c_q+1) = 10^6$ states.
2. The correct complexity bound is **$O(k\prod_q(c_q+1))$ total** — *not* $O(N k \prod_q (c_q+1))$ — because the step index is determined by the state: $i=\sum_q n_q$. For 6 players × 9 cards this is $6\times10^6$ multiply–adds; for the 5-unknown-player view (an agent reasoning about the other five hands) it is $5\times10^5$. This is **microseconds to low milliseconds in C**.
3. Measured in this report: exact $Z$, exact forward/backward tables, and exact per-card marginals over the full constrained deal space, in **0.10 s in unoptimised NumPy** (naive per-step arrays, ~100× off the optimal indexing). Marginals agree with brute-force enumeration to **2.2 × 10⁻¹⁶** (machine precision).
4. **Exact, rejection-free sampling** of deals from the constrained posterior follows immediately from the backward table: sample card-by-card with $p(q)\propto w_{i,q}B_i(n+e_q)$. Verified against brute force (§8.2). Zero rejections, no burn-in, no mixing question.
5. By contrast **naive per-card sampling followed by a hand-size check accepts only 0.04 %–0.07 % of the time** (measured, §8.3) — roughly 1 500–2 500 attempts per usable deal, and that is the *easy* uniform case. This is the trap most card-game agents fall into.
6. **Disjunctive constraints** ("player $p$ held ≥1 card of half-suit $S$ at time $t$", implied by the legality of every ask) do not factorise. Two exact treatments: inclusion–exclusion ($2^k$ DP runs — fine for $k\le6$, hopeless for the 30–60 asks in a real game), or, far better, **half-suit block enumeration**: enumerate the $\le 6^6$ assignments of each half-suit's cards, filter *all* of that half-suit's constraints exactly, and aggregate into a per-block count-vector table. Measured: full engine with **27 disjunctive constraints handled exactly in 0.72 s NumPy** (§8.4), block tables containing only 15–33 nonzero count vectors.
7. Ryser's and Glynn's exact permanent formulas ($O(2^n n)$) are **irrelevant** here: $n=54$ means $2^{54}$. Published benchmarks put the naive combinatoric algorithm past 15 min at $n>14$ while Ryser/Glynn stay under 1 s at comparable sizes (Chuiko et al. 2025).
8. The **JSV FPRAS is a theoretical result only**. Newman & Vardi (2020) measured it: $n=10$ took **50 634 s** versus **0.00304 s** for Ryser; the theoretical crossover with Ryser is at **$n\approx68$**, where the FPRAS needs $\sim1.3\times10^{22}$ steps ≈ 420 984 years. Do not implement it.
9. **Sinkhorn / matrix scaling** (Linial–Samorodnitsky–Wigderson) gives an $e^n$ permanent approximation in $O((n/\epsilon)^2)$ iterations, $O(n^6)$ total. As an *approximate marginal oracle* for Fish it is ~300× faster than the exact DP but measured **max marginal error 2 %–6 %** (§8.5). Useful only as a warm-start or an inner-loop heuristic; the exact DP is cheap enough that Sinkhorn is not needed.
10. **Bethe permanent / belief propagation**: $\mathrm{perm}(\theta)\ge\mathrm{perm}_B(\theta)$ and, tightly, $\mathrm{perm}(\theta)\le\sqrt2^{\,n}\,\mathrm{perm}_B(\theta)$ (Anari & Oveis Gharan 2021, resolving Gurvits' conjecture). Again unnecessary given exact DP, but it is the right fallback if the state space is ever enlarged.
11. **Sequential importance sampling** (Chen–Diaconis–Holmes–Liu) is the standard fallback for constrained tables, but Bezáková–Sinclair–Štefankovič–Vigoda proved it can **underestimate by an exponential factor for any subexponential sample count**, for *any* row/column ordering. The Fish DP sidesteps SIS entirely because it supplies the *exact* sequential conditionals — which is precisely the "almost perfect" regime Diaconis & Kolesnik show reduces the required sample size to $O(1)$.
12. **The residual non-factorised term is the real bottleneck, not the constraints.** The policy likelihood $\prod_t\pi(a_t\mid h_{<t},x)$ does not factorise over cards. Measured ESS follows the classical log-normal law $\mathrm{ESS}/N\approx e^{-\sigma^2}$ *exactly* (§8.6): $\sigma=0.44$ nats → 83 % ESS; $\sigma=0.89$ → 46 %; $\sigma=1.77$ → 3 %. **Design rule: keep $\sigma=\mathrm{sd}[\log(\text{residual})]\lesssim0.8$ nats, or the estimator collapses.**
13. Folding the learned per-card weights *into* the DP rather than into the importance weight bought a **300× ESS improvement** in the same experiment (82.7 % vs 0.3 %). Whatever can be made factorised should live inside the DP.
14. **Fish is an unusually clean public-belief-state game.** Every ask, answer, transfer and declaration is public, so the *only* hidden variable is the initial deal, and capacities stay at exactly 9 for the entire game. Every player computes the *same* posterior from the *same* public history — this is literally the PuB-MDP of Nayyar et al. / Foerster et al. (BAD). Particle filters, degeneracy and reinvigoration are **structurally unnecessary**: recompute the exact belief from scratch each turn.
15. **Critical negative result to respect:** Rebstock et al. (2019) found that a *cheating* inference module (all probability mass on the true world) **played worse** than ordinary inference in Skat — −3.25 and −8.49 tournament points/game in suit and grand. Better beliefs do not monotonically improve a PIMC-style searcher, because of strategy fusion. Belief quality and search soundness must be fixed together.

---

## 2. Formalising the Fish belief problem

### 2.1 Variables and constraints

Fix a reasoning seat. Let

- $U$ = set of cards whose **initial owner** is unknown. $|U|=45$ from a player's seat (they know their own 9), $|U|=54$ for a neutral observer.
- $Q$ = set of candidate owners, $k=|Q|$ ($5$ or $6$).
- $c_q$ = number of cards of $U$ initially dealt to $q$. **In Fish this is 9 for every player, constant for the whole game**, because the hidden variable is the *initial deal* and declarations/transfers never change it.
- $x_{i,q}\in\{0,1\}$: card $i$ was dealt to $q$. Constraints $\sum_q x_{i,q}=1$, $\sum_i x_{i,q}=c_q$.

This is a $|U|\times k$ **0/1 contingency table with all row sums 1 and column sums $c$**, plus structural zeros. Equivalently a perfect matching in a bipartite graph where each player vertex is split into $c_q$ copies.

The constraint families from the game:

**(a) Capacity.** $\sum_i x_{i,q}=c_q=9$.

**(b) Per-card support sets** $A_i\subseteq Q$. A public "no" from $p$ for card $x$ at time $t$ means $p$ did not hold $x$ at $t$; combined with the public transfer log this determines whether $\mathrm{origin}(x)=p$ is still possible. Encode as $w_{i,q}=0$ for $q\notin A_i$. Because *every* transfer is public, $A_i$ is common knowledge.

**(c) Disjunctive ask-legality constraints.** Every ask by $p$ for a card of half-suit $S$ certifies that $p$ held **at least one other card of $S$** at that moment:
$$\bigvee_{i\in D_e}\big[\mathrm{origin}(i)=p\big],\qquad D_e\subseteq S,$$
where $D_e$ is the set of $S$-cards whose origin is still unknown *and* which, if dealt to $p$, would still be in $p$'s hand at time $t$ (computable from the public log). This is the only genuinely non-product-form constraint, and it is **the single richest source of information in Fish** — a strong agent must use it.

**(d) Declaration constraints.** A declaration names the exact card→teammate allocation for all 6 cards of a half-suit at time $t$. Pulled back through the public transfer log, these become equalities or small disjunctions on origins. They typically *collapse* the block, making it cheaper, not more expensive.

**(e) Cardless drop-out / count observations.** Public hand counts are deterministic functions of (initial deal, public log), so they yield further support restrictions rather than new constraint types.

### 2.2 The target distribution

$$
P(x\mid h)\;\propto\;\underbrace{\prod_{i\in U}\prod_{q\in Q} w_{i,q}^{\,x_{i,q}}}_{\text{factorised: legality + learned card-location prior}}\;\cdot\;\underbrace{\mathbf 1[\text{constraints (a)–(e)}]}_{\text{hard, (c) non-product-form}}\;\cdot\;\underbrace{L(h\mid x)}_{\text{policy likelihood, non-factorised}}
$$
with
$$
L(h\mid x)\;=\;\prod_{t}\pi_{p_t}\!\big(a_t\mid h_{<t},\,x\big).
$$

The engineering decomposition that follows from this report: **put (a), (b), (c), (d) and the factorised $w$ inside an exact DP; handle only $L$ by importance weighting.**

### 2.3 Relation to the permanent

$$
Z_0=\sum_x\prod_{i,q}w_{i,q}^{x_{i,q}}\;=\;\frac{\mathrm{per}(M)}{\prod_q c_q!},
$$
where $M\in\mathbb R^{|U|\times|U|}$ replicates player $q$'s weight column $c_q$ times. So the belief normaliser *is* a permanent — but a permanent of a matrix with only $k=6$ **distinct column types**. Computing permanents of general 0/1 matrices is #P-complete (Valiant); the constant-column-type case is polynomial, which is exactly why Fish is tractable. (Compare: Barvinok's result that permanents of fixed-rank matrices are polynomial-time; and Cryan–Dyer's FPRAS for contingency tables with a constant number of rows. Note counting general contingency tables is #P-complete *even with two rows* — but that hardness needs entries encoded in binary; here every count is $\le 9$ in unary.)

---

## 3. Exact algorithms

### 3.1 Ryser and Glynn (and why they don't apply)

Ryser's formula, for $A\in M_{n}$:
$$
\mathrm{per}(A)=(-1)^n\sum_{S\subseteq\{1..n\}}(-1)^{|S|}\prod_{i=1}^n\sum_{j\in S}a_{ij}
$$
Evaluated with $S$ in Gray-code / minimal-change order this costs $O(2^n n)$ arithmetic operations (naively $O(2^n n^2)$).

Glynn / Balasubramanian–Bax–Franklin (char ≠ 2):
$$
\mathrm{per}(A)=\frac1{2^{\,n-1}}\sum_{\delta\in\{\pm1\}^n}\Big(\prod_{k=1}^n\delta_k\Big)\prod_{j=1}^n\sum_{i=1}^n\delta_i a_{ij}
$$
over $2^{n-1}$ sign vectors — asymptotically about a factor 2 faster than Ryser.

**Practical numbers** (Chuiko et al. 2025, Apple M1 Pro, `-O3`, sequential): the naive combinatoric algorithm exceeds 15 min beyond $n=14$ while Ryser and Glynn stay under 1 s; Glynn wins for near-square matrices, Ryser exploits rectangularity better because Glynn pads rectangular matrices with rows of 1s. Their `opt` dispatcher switches algorithm at $n=13$, aspect ratio $0.29$. Ryser is numerically more stable (never loses precision on the identity; Glynn does for non-square).

**Verdict for Fish:** $n=54\Rightarrow2^{54}\approx1.8\times10^{16}$. Unusable. Use §3.2 instead.

### 3.2 The capacity-vector DP (the core recommendation)

Order the unknown cards $1..N$ ($N=|U|$). State = capacity vector $n=(n_1,\dots,n_k)$, $0\le n_q\le c_q$, with $e_q$ the $q$-th unit vector.

**Forward:**
$$
F_0(\mathbf 0)=1,\qquad
F_i(n)=\sum_{q:\,n_q\ge1} w_{i,q}\,F_{i-1}(n-e_q),\qquad
Z=F_N(c)
$$

**Backward:**
$$
B_N(c)=1,\qquad
B_i(n)=\sum_{q:\,n_q\le c_q-1} w_{i+1,q}\,B_{i+1}(n+e_q)
$$

**Exact marginals** (forward–backward):
$$
\mu_{i,q}\;=\;\Pr[\mathrm{origin}(i)=q\mid h]\;=\;\frac{1}{Z}\sum_{n} F_{i-1}(n)\,w_{i,q}\,B_i(n+e_q)
$$

**Exact pairwise marginals** (needed for "does $p$ hold ≥1 of $S$?"): same trick with two insertion points, or via §3.5.

**Complexity — the key point.** $F_i(n)$ is nonzero only when $\sum_q n_q=i$. The step index is *determined by the state*. Hence $F$ and $B$ are each a **single array of size $\prod_q(c_q+1)$**, iterated in order of increasing $\sum_q n_q$, and total work is
$$
O\!\Big(k\prod_q (c_q+1)\Big)
$$
| view | $k$ | $\prod(c_q+1)$ | multiply–adds | memory |
|---|---|---|---|---|
| agent (5 unknown hands) | 5 | $10^5$ | $5\times10^5$ | 0.8 MB |
| neutral observer (6 hands) | 6 | $10^6$ | $6\times10^6$ | 8 MB |

That is **sub-millisecond in C** for the agent view. A naive implementation that stores one array per card step (as the benchmark in §8.1 does) wastes ~$N$× the memory and time — still only 0.10 s in Python, but do not ship that.

**Numerical care.** Measured $Z\approx10^{20}$–$10^{24}$ with unit weights, and learned weights push it further. Either work in logs with log-sum-exp, or rescale $F$ by $1/\max F$ after each $\sum n$ level and accumulate the log of the scale factors. Float64 alone is *probably* fine ($10^{308}$ headroom) but will break with peaked policy weights.

### 3.3 Exact rejection-free sampling

Given the backward table, sample sequentially. With state $n$ after cards $1..i-1$:
$$
\Pr[\mathrm{origin}(i)=q\mid \text{prefix}] = \frac{w_{i,q}\,B_i(n+e_q)}{\sum_{q'}w_{i,q'}B_i(n+e_{q'})}
$$
This is an **exact** draw from the constrained posterior: every prefix is extendable by construction (any $q$ with $B_i(n+e_q)=0$ gets probability 0), so there are **no rejections and no dead ends**. Cost per sample: $O(Nk)$ = 225 operations, after one $O(k\prod(c_q+1))$ backward pass shared by all samples.

This is the same object SIS tries to approximate. Diaconis & Kolesnik's "almost perfect" algorithms $A_f^*,A_g^*$ — which set the sequential probabilities to the true asymptotic conditionals — achieve $\mathrm{Var}[\log T^*]=O(1)$ and therefore need only $O(1)$ samples. The Fish DP gives the *exact* conditionals, i.e. variance exactly zero. **There is no importance weight at all from the constraint part.**

### 3.4 Handling disjunctive constraints by inclusion–exclusion

For a single constraint $C_e$: "$p$ holds ≥1 of $D_e$",
$$
Z[\,C_e\,]=Z-Z\big[\text{no card of }D_e\text{ to }p\big]
$$
and the bracketed term is another product-form problem (set $w_{i,p}=0$ for $i\in D_e$), so it is one more DP run. With $k$ constraints:
$$
Z\Big[\bigwedge_{e=1}^{k}C_e\Big]=\sum_{T\subseteq\{1..k\}}(-1)^{|T|}\,Z\big[\textstyle\bigwedge_{e\in T}\neg C_e\big]
$$
$2^k$ DP runs. **Verified numerically** (§8.4): $\Pr[p_2\text{ holds}\ge1\text{ of }S]=0.629172$ by inclusion–exclusion vs $0.629990$ from $10^5$ exact samples.

Cost measured: $k=6\Rightarrow64$ DP runs $\Rightarrow$ 4.0 s NumPy (≈ 60 ms optimised C). **A real Fish game generates 30–60 ask-legality constraints. $2^{60}$ is not an option.** Use §3.5.

### 3.5 Half-suit block enumeration (the right way for Fish)

**Key structural observation: every disjunctive constraint is confined to a single half-suit.** A constraint from an ask in half-suit $S$ mentions only $S$'s 6 cards. So:

1. Partition $U$ into the 9 half-suit blocks $S_1..S_9$ (block $b$ has $m_b\le6$ unknown cards).
2. For each block, **enumerate all $\le k^{m_b}$ assignments** ($6^6=46\,656$ worst case, usually far fewer after support pruning), evaluate the product weight, and **filter against every one of that block's disjunctive and declaration constraints exactly**.
3. Aggregate survivors into a table
$$
g_b(t)=\sum_{\substack{\text{assignments }a\text{ of block }b\\ \text{count}(a)=t,\ a\models \text{constraints}_b}}\ \prod_{i\in S_b}w_{i,a_i},\qquad t\in\mathbb Z_{\ge0}^{k},\ \textstyle\sum_q t_q=m_b
$$
4. Run the capacity DP **over blocks** instead of cards:
$$
F_b(n)=\sum_{t} g_b(t)\,F_{b-1}(n-t)
$$

Same $\sum n$ ⇒ block-index identity holds, so $F,B$ remain single arrays. Work $=\prod_q(c_q+1)\times|\mathrm{supp}(g_b)|$ summed over blocks. Worst case $|\mathrm{supp}(g_b)|=\binom{m_b+k-1}{k-1}=462$; **measured actual support sizes were 15–33** because constraints and void information prune hard.

**Measured full engine (§8.4):** 9 blocks, 5 unknown cards each, 5 players × 9, **27 disjunctive constraints applied exactly**: block tables 0.006 s, forward 0.065 s, backward 0.064 s, exact marginals 0.59 s — **0.72 s total in NumPy**, forward/backward agreeing to 4.2 × 10⁻¹⁶, all card rows summing to 1 and all player columns to exactly 9.

This is the single most important algorithmic recommendation in this report. **It makes exact Fish belief — including full ask-legality reasoning — a solved problem at real-time speeds.**

**Failure mode to handle:** if the constraint set is jointly unsatisfiable, some $g_b$ is empty and $Z=0$. This happened in an early run of the experiment with randomly generated (rather than deal-derived) constraints. In a real agent $Z=0$ means a modelling bug or an opponent rules violation; assert on it and fall back to dropping the newest constraint.

---

## 4. Approximate methods (fallbacks; not needed at Fish scale)

### 4.1 Sinkhorn / matrix scaling and the LSW bound

Scale $B=XAY$ with diagonal $X=\mathrm{diag}(x)$, $Y=\mathrm{diag}(y)$. Then
$$
\mathrm{per}(B)=\Big(\prod_i x_i\Big)\Big(\prod_j y_j\Big)\mathrm{per}(A)
$$
so any $\kappa$-approximation of $\mathrm{per}(B)$ transfers to $A$. If $B$ is doubly stochastic then $\mathrm{per}(B)\le1$ and, by the van der Waerden bound (Egorychev, Falikman), $\mathrm{per}(B)\ge n!/n^n>e^{-n}$. Hence
$$
\frac{1}{\prod_i x_i\prod_j y_j}\;\ge\;\mathrm{per}(A)\;\ge\;\frac{e^{-n}}{\prod_i x_i\prod_j y_j}
$$

**Linial–Samorodnitsky–Wigderson (2000), Theorem 1.1:** there is a strongly polynomial $f$ with $\mathrm{per}(A)\le f(A)\le e^n\,\mathrm{per}(A)$. Their $(\mathbf 1,\mathbf 1)$-scaling needs $O((n/\epsilon)^2)$ Sinkhorn iterations after a matching-based preprocessing step; taking $\epsilon=n^{-2}$ gives total $O(n^6)$. For general $(r,c)$ margins — which is exactly the Fish case, $r_i=1$, $c_q=9$ — they give a genuinely fully-polynomial scheme with $\tilde O(n^7\log(1/\epsilon))$ iterations. Approximate scalability is measured by $\sum_j(c'_j-c_j)^2\le\epsilon$.

**As a marginal oracle:** the scaled matrix entry $B_{iq}$ approximates $\Pr[\mathrm{origin}(i)=q]$. **Measured accuracy vs the exact DP** (§8.5), 45 cards / 5 players × 9:

| weight spread $\sigma$ | void frac | exact DP | Sinkhorn | iters | max abs marginal error | mean abs error |
|---|---|---|---|---|---|---|
| 0.0 | 0.35 | 0.103 s | 0.0003 s | 50 | 0.0021 | 0.00025 |
| 1.0 | 0.35 | 0.101 s | 0.0003 s | 50 | 0.0230 | 0.00498 |
| 2.0 | 0.35 | 0.097 s | 0.0005 s | 100 | 0.0329 | 0.00708 |
| 1.0 | 0.00 | 0.143 s | 0.0003 s | 50 | 0.0180 | 0.00370 |
| 1.0 | 0.60 | 0.060 s | 0.0009 s | 200 | 0.0591 | 0.00615 |

~300× faster, but **2–6 % max error, worsening with sparsity and with peaked weights** — precisely the regimes Fish enters late in a game. Recommendation: use Sinkhorn only as a cheap inner-loop heuristic (e.g. move ordering) or as a warm start; never as the belief the agent acts on.

### 4.2 Bregman–Minc, Schrijver, and cheap bounds

**Bregman–Minc** (conjectured Minc, proved Brègman; entropy proof by Radhakrishnan/Schrijver): for a 0/1 matrix with row sums $r_i$,
$$
\mathrm{per}(A)\le\prod_{i=1}^n (r_i!)^{1/r_i}
$$
**van der Waerden** (Egorychev, Falikman): doubly stochastic $A\Rightarrow\mathrm{per}(A)\ge n!/n^n$.
**Schrijver:** every $a$-regular bipartite graph with $b$ vertices per side has at least $\big((a-1)^{a-1}/a^{a-2}\big)^{b}$ perfect matchings.

Use: instant sanity bounds on information-set size, e.g. to decide whether the exact DP is even needed or whether the set has collapsed to a handful of deals.

### 4.3 Bethe permanent / belief propagation

$$
\mathrm{perm}_B(\theta)=\exp\big(-\min_{\gamma\in\Gamma_{n\times n}}F_B(\gamma)\big),\qquad F_B=U_B-H_B
$$
$$
U_B(\gamma)=-\sum_{i,j}\gamma_{ij}\log\theta_{ij},\qquad
H_B(\gamma)=-\sum_{i,j}\gamma_{ij}\log\gamma_{ij}+\sum_{i,j}(1-\gamma_{ij})\log(1-\gamma_{ij})
$$
minimised over doubly stochastic $\gamma$ (Vontobel 2013). $F_B$ is convex (via concavity of the Bethe entropy), so BP / a convex solver finds the global optimum, and the minimiser $\gamma^*$ *is* the approximate marginal matrix.

**Quality:** $\mathrm{perm}_B(\theta)\le\mathrm{perm}(\theta)$ always, and tightly $\mathrm{perm}(\theta)\le\sqrt2^{\,n}\,\mathrm{perm}_B(\theta)$ (Anari & Oveis Gharan 2021, resolving a Gurvits conjecture). Vontobel also gives $\mathrm{perm}(1_{n\times n})/\mathrm{perm}_B(1_{n\times n})=\sqrt{2\pi n}/e\,(1+o(1))$.

Reported practical comparison: Bethe gives better *permanent* estimates, Sinkhorn often better *marginals* and better scaling at moderate $n$. For Fish neither is required.

### 4.4 The JSV FPRAS — a documented trap

Jerrum, Sinclair & Vigoda (JACM 2004) give an FPRAS for the permanent of any non-negative matrix via simulated annealing over perfect and near-perfect matchings. State space: perfect matchings plus near-perfect matchings with a hole $(u,v)$, $(n^2+1)n!$ states. Weights $w(M)=\lambda(M)=\prod_{(u,v)\in M}\lambda(u,v)$ for perfect $M$, $w(M)=w(u,v)\lambda(M)$ for a near-perfect matching with hole $(u,v)$, with $\lambda(u,v)=1$ if $(u,v)\in E$ and $\lambda$ otherwise. Stationary $\pi(M)=w(M)/Z$. Annealing from $\lambda_0=1$ down to $\lambda=1/n!$ via $\lambda_{j+1}=2^{-1/2i}\lambda_j$. Best known runtime $O(n^7\log^4 n)$ (Bezáková–Štefankovič–Vazirani–Vigoda).

**Newman & Vardi (2020) measured it**, with the algorithm already relaxed by factors of 1/256 in sample count and 1/1024 in chain steps:

| $n$ | relaxed FPRAS | Ryser |
|---|---|---|
| 4 | 30 s | 0.0000377 s |
| 6 | 738 s | 0.000151 s |
| 8 | 10 528 s | 0.000661 s |
| 10 | 50 634 s | 0.00304 s |

80 matrices took 344 CPU-hours. Mixing bound $\tau_x(\delta)\le336(n^4+n^2)\ln(1/\delta)$ per sample; $|S_w|\le475(n^2+1)\ln(24\ell(n^2+1))$ samples per phase. Asymptotic total $\lim_{n\to\infty}T=\frac{14\,515\,200}{\ln^2 2}\cdot\frac{n^6\ln^5 n}{\epsilon^2}$. Crossover with Ryser at $n\ge68$, where the FPRAS needs $13\,285\,251\,197\,747\,730\,326\,655$ steps ≈ **420 984 years** at $10^9$ steps/s. Their conclusion: "The FPRAS for the matrix permanent is clearly computationally infeasible."

Accuracy was fine (mean error 0.046–0.055 against an $\epsilon=0.5$ bound, 0/640 trials exceeded it) — the algorithm is *correct*, just unusably slow. **Do not implement.**

### 4.5 Sequential importance sampling and its negative results

Chen–Diaconis–Holmes–Liu (JASA 2005) sample binary contingency tables column by column. Assigning column $j$ the vector $(t_1..t_m)\in\{0,1\}^m$ with $\sum_i t_i=c_j$ with probability proportional to
$$
\prod_i\Big(\frac{r'_i}{n'-r'_i}\Big)^{t_i},\qquad n'=n-j+1
$$
($r'_i$ = residual row sums), sampled by DP. Estimator over $t$ trials:
$$
X_t=\frac1t\sum_{i=1}^{t}\frac{1}{\mu(T_i)},\qquad \mathbb E[1/\mu(T)]=|\Omega|
$$
If a column cannot be assigned, restart with $1/\mu=0$.

**Negative result (Bezáková, Sinclair, Štefankovič, Vigoda).** For $r=(1,\dots,1,\lfloor\beta m\rfloor)$, $c=(1,\dots,1,\lfloor\gamma m\rfloor)$ with $\beta\ne\gamma$, **for any ordering of rows or columns**, there exist $s_1\in(0,1),s_2>1$ with
$$
\Pr\Big(X_t\ge \frac{|\Omega_{r,c}|}{s_2^{\,m}}\Big)\le 3s_1^{\,m}\quad\text{for all }t\le s_2^{\,m}
$$
i.e. exponential underestimation unless the sample count is exponential — **and the estimator appears to converge while doing so**, defeating the usual convergence heuristics. (They report SIS *is* efficient for regular margins.)

**Diaconis & Kolesnik (2019/2021)** analyse SIS for perfect matchings. With $T(\pi)=P(\pi)^{-1}$, $\mathbb E T=M_n$. The sample size criterion is the KL divergence:
$$
L=D(\nu\|\mu)=\mathbb E_\nu\log\rho(\pi),\qquad N^*\gg e^{L+\sigma},\ \sigma^2=\mathrm{Var}_\nu\log\rho
$$
"$N=e^L$ is necessary and sufficient for accuracy" (Chatterjee–Diaconis). Typically $L\sim cn$ — exponential, but with small $c$, so SIS is competitive with polynomial MCMC at practical $n$. Crucially, their **"almost perfect" variants** set the sequential probabilities to the true conditionals and achieve $\mathrm{Var}[\log T^*]=O(1)$, hence $N^*=O(1)$. **The Fish DP is the exact version of this.**

### 4.6 MCMC on assignments (Diaconis–Gangolli / switch chains)

The natural chain for Fish: pick two cards currently assigned to different players and swap them if both assignments remain legal (a $2\times2$ switch). This is the Diaconis–Gangolli chain / bipartite switch chain. Known results: Dyer & Greenhill proved rapid mixing for two-rowed tables via a $2\times2$ heat-bath chain; Cryan–Dyer–Goldberg–Jerrum–Martin extended to any constant number of rows; Greenhill proved rapid mixing of the switch chain for degree sequences with $3\le d_{\max}\le\frac14\sqrt M$; Miklós–Erdős–Soukup handled half-regular bipartite sequences.

**Verdict for Fish:** correct but pointless. The exact sampler (§3.3) is unbiased, i.i.d., and cheaper. The only use for a swap chain is as an MCMC *rejuvenation* move if you ever adopt a particle representation for the non-factorised residual — see §6.

---

## 5. Policy-conditioned inference (the part that actually needs approximation)

### 5.1 The general Bayes formulation

Buro, Long, Furtak & Sturtevant (IJCAI 2009) state it plainly: inference is finding $P(\text{world}\mid\text{move})$; an agent that can play gives $P(\text{move}\mid\text{world})$; Bayes does the rest. They name two problems: computing $P(\text{move}\mid\text{world})$ for many worlds is intractable, and a **deterministic** player gives $P(\text{move}\mid\text{world})\in\{0,1\}$, which "makes the prediction brittle in the face of players who do not play identically to ourselves." Their fix: learn $P(\text{world}\mid\text{move})$ offline, and generalise over *features* of worlds rather than worlds:
$$
P(\text{world}\mid\text{move})=\prod_i P(f_i)\,P(f_i\mid\text{move})
$$
with $P(f_i\mid \text{move})$ from a lookup table and $P(f_i)$ computed analytically. In Skat they sample worlds uniformly without replacement from the void-consistent set, then compute
$$
P(w_i\mid \mathrm{bid}_1,\mathrm{bid}_2)=P(h_1(w_i')\mid \mathrm{bid}_1)\cdot P(h_2(w_i')\mid \mathrm{bid}_2)
$$
and resample for PIMC after normalisation. They could only evaluate 160 states/s with the real bidding evaluator, hence the table-based feature approximation.

**Kermit results (IJCAI 2009), points per 36 games, 1600 games each:** Kermit(SD) 996 vs Kermit(NI) 779; Kermit(SD) 986 vs Kermit(S) 801; Kermit(SD) 861 vs Kermit(D) 820. **Defender inference (inferring the soloist's cards) had by far the biggest impact.**

### 5.2 GIB: constrained deal generation in practice

Ginsberg (JAIR 2001) describes exactly the pipeline to avoid: simplify the auction into per-hand constraints, "deal hands consistent with the constraints using a deal generator that deals unbiased hands given restrictions on the number of cards held by each player in each suit. This set of deals is then tested to remove elements that do not satisfy the remaining constraints, and each of the remaining deals is passed to the bidding module to identify those for which the observed bids would have been made." **That is rejection sampling against a policy.** He reports "the overall dealing process typically takes one or two seconds to generate the full set of deals needed," with a Monte Carlo sample size **fixed at 50** (100 with extra resources).

For card play GIB does not test each decision recursively ("it is simply impractical"); instead it estimates per-card probabilities and reweights: if analysis says 80 % of the time West holds ♠K it is a mistake not to play it, West's failure gives 4:1 odds against, applied via Bayes, and $\sum_d s(m,d)$ becomes $\sum_d w_d s(m,d)$.

**Lesson for Fish:** GIB spends 1–2 s to produce 50 deals. The Fish DP produces *exact* deals at ~110 µs each (measured, §8.6) with zero rejection — four to five orders of magnitude better, because the constraints are folded into the sampler instead of being tested after the fact.

### 5.3 Factorised card-location beliefs (the piece to reuse)

Solinas, Rebstock & Buro (AAAI 2019) predict **individual card locations** with a neural net and combine:
$$
p(s\mid h)\;\propto\;\prod_{c\in C} L(h)_{c,\,\mathrm{loc}(c,s)}
$$
where $L(h)$ is $|C|\times \ell$ (Skat: $32\times4$), row-softmaxed. They explicitly note: "Our work does not impose any additional constraints, but constraints on the number of total cards in each hand or each suit's length could be added as well." **In Fish, the DP of §3.2 is exactly that addition** — it imposes the hand-size constraints on a product-of-marginals model *exactly*, at negligible cost. Their normalisation was brute force over the information set ("a rough maximum of 42 million states in Skat is manageable in around 2 seconds"); at $10^{40}$ Fish states this is impossible, so the DP is not a nicety, it is the enabling step.

Metric they introduce, worth copying:
$$
\mathrm{TSSR}=\frac{p(s^*\mid h)}{1/n}=p(s^*\mid h)\cdot n
$$
— how many times more likely the true state is than under uniform sampling. **Results:** BDCI beat Kermit's inference (KI) by >4 tournament points/game in suit and null; 3.1× slower per move (0.286 s vs 0.093 s at 320 states/move). Note KI saturates at 160 states/move (Furtak & Buro 2013).

### 5.4 Policy Inference — weighting deals by the action likelihood

Rebstock, Solinas, Buro & Sturtevant (CoG 2019) weight each world by its reach probability under models of the other players:
$$
\eta(s\mid I)=\prod_{h\cdot a\sqsubseteq s}\pi(h,a)
$$
Chance and own-action factors are known/1, so only opponent and partner policy models matter. They sample a subset (20 k or 100 k worlds out of up to 2.8 billion), multiply the model probabilities along the history, and normalise (their Algorithm 1). Policies are deep nets trained on human data.

**Results:** PI beat CLI by **+2.32 / +0.64\* / +1.57 TP/G** (suit / grand / null; \* = not significant); PI beat KI by 5.13 / 3.12 / 2.37; NI (no inference) lost to PI by 10.43 in suit. Most of the gain came from **defence** ($\Delta$Def consistently larger than $\Delta$Sol). Cost: PI20 ≈ **5× slower per move** than CLI.

**This is the template for the Fish residual term.** In Fish, $\eta$ has a beautiful property absent in Skat: every ask, answer and declaration is public, so $\eta$ decomposes over a *public* action sequence, and each factor $\pi_{p_t}(a_t\mid h_{<t},x)$ depends on $x$ only through $p_t$'s own hand at time $t$ — which is a deterministic function of $x$ restricted to the cards $p_t$ could hold. The residual is therefore much more local than in Skat.

### 5.5 The two-stage estimator (recommended architecture)

$$
\underbrace{q(x)\;\propto\;\prod_{i,q}w_{i,q}^{x_{i,q}}\cdot\mathbf 1[\text{(a)–(e)}]}_{\text{exact DP proposal, §3.2–3.5}},\qquad
\underbrace{\omega(x)=\frac{P(x\mid h)}{q(x)}\propto L(h\mid x)}_{\text{residual policy likelihood}}
$$
Draw $x^{(1..N)}\sim q$ exactly, weight by $\omega$, self-normalise:
$$
\hat{\mathbb E}[f]=\frac{\sum_j \omega(x^{(j)}) f(x^{(j)})}{\sum_j \omega(x^{(j)})},\qquad
\mathrm{ESS}=\frac{1}{\sum_j \tilde\omega_j^2},\ \ \tilde\omega_j=\frac{\omega_j}{\sum_l\omega_l}
$$

**Measured ESS behaviour** (§8.6), $N=3000$, with a deliberately non-factorised (pairwise, within-half-suit) residual whose strength is swept:

| scale | sd$[\log\omega]$ (nats) | ESS % — exact DP proposal | ESS % — uniform-legal proposal |
|---|---|---|---|
| 0.02 | 0.09 | **99.2** | 0.3 |
| 0.05 | 0.22 | **95.3** | 0.3 |
| 0.10 | 0.44 | **82.7** | 0.3 |
| 0.20 | 0.89 | **45.6** | 0.3 |
| 0.40 | 1.77 | 3.0 | 0.2 |
| 0.80 | 3.54 | 0.1 | 0.2 |
| 1.50 | 6.64 | 0.0 | 0.1 |

Two conclusions, both actionable:

- The classical log-normal law $\mathrm{ESS}/N\approx e^{-\sigma^2}$ holds essentially exactly here ($\sigma=0.44\Rightarrow e^{-0.19}=0.83$ vs measured 0.827; $\sigma=0.89\Rightarrow0.45$ vs 0.456; $\sigma=1.77\Rightarrow0.044$ vs 0.030). **Budget: $\sigma\le0.83$ nats for ESS ≥ 50 %.** Equivalently, this is Chatterjee–Diaconis' $N\approx e^L$ criterion in disguise.
- Moving the learned factorised weights *out* of the importance weight and *into* the DP proposal changed ESS from 0.3 % to 82.7 % — a **~300× improvement**. Anything expressible per-card belongs in the DP.

If $\sigma$ exceeds budget: temper the residual ($\omega^{1/\tau}$), or push more of $L$ into the factorised $w$ by distilling the policy model into per-card location logits (Solinas et al.'s $L(h)$ matrix, but re-trained conditioned on the public history), or introduce the block-level residual into the block tables $g_b$ (any residual that is confined to one half-suit can be made *exact* by folding it into $g_b$ — this is a large and underused lever in Fish).

### 5.6 Fish as a public belief state game

In Fish, "All asks, answers, transfers, declarations and hand counts are PUBLIC. The ONLY hidden state is the initial 9-card deal." This is precisely the structure that makes the **public belief MDP** (Nayyar et al. 2013; Foerster et al., BAD, ICML 2019) exact rather than approximate:
$$
B_t=P\big(f^{\mathrm{pri}}_t\mid f^{\mathrm{pub}}_{\le t}\big),\qquad
P(f^a_t\mid u^a_t,B_t,f^{\mathrm{pub}}_t,\Delta_\pi)\;\propto\;\mathbf 1\big(\Delta_\pi(f^a_t),u^a_t\big)\,P(f^a_t\mid B_t,f^{\mathrm{pub}}_t)
$$
Every player can compute the same $B_t$ by the same algorithm. BAD in Hanabi reported beliefs with **40 % less uncertainty** over possible hands than non-Bayesian baselines and 24.174 points in 2-player self-play. Learned Belief Search (Hu, Lerer, Brown, Foerster 2021) replaces the exact belief with an auto-regressive learned sampler, capturing **55–91 % of the benefit of exact search at 4.6×–35.8× less compute** — relevant only if you later need a *learned* belief; for Fish the exact belief is cheaper than the learned one.

**Implication for signalling:** because the belief is common knowledge, a Fish ask is simultaneously an information-gathering move and a signal, and the *counterfactual* structure matters — the PuB-MDP transition depends on all of $\Delta_\pi$, not just the action taken. Any convention the bot develops with its teammates is automatically legible in $B_t$.

---

## 6. Particle filters, degeneracy and reinvigoration

Standard practice in POMDP planners (Silver & Veness, POMCP, NIPS 2010) is an unweighted particle filter with **particle reinvigoration** — "particle deprivation is possible for large $t$... new particles can be introduced by adding artificial noise to existing particles" — plus resampling when $\mathrm{ESS}=1/\sum_j\tilde\omega_j^2$ drops below a threshold (typically $N/2$), optionally with an MCMC rejuvenation move.

**For Fish this machinery is unnecessary and should be avoided.** Reasons:

1. The constraint set is *monotone* and the capacity vector is *constant* (always 9). There is no filtering recursion to degenerate: the exact belief at time $t$ can be recomputed from scratch from the public history in milliseconds (§3.5).
2. Sequentially reweighting a fixed particle set is exactly what causes deprivation: as public misses accumulate, particles die and cannot be resurrected by "adding noise" (there is no meaningful noise on a discrete deal). Recomputation has no such failure mode.
3. If you nonetheless want a persistent particle set (e.g. to amortise expensive per-deal policy evaluations), the correct rejuvenation move is not noise but the **swap chain of §4.6** (pick two cards held by different players, swap if legal), targeting $q$ or $P$; and the correct trigger is $\mathrm{ESS}<N/2$.
4. Artificial reinvigoration would *violate* hard constraints, injecting deals that are provably impossible — a correctness bug, not just an inefficiency.

**Recommendation: no particle filter. Recompute exactly each turn; use i.i.d. exact samples for the search.**

---

## 7. Applicability to Canadian Fish — technique by technique

| Technique | Helps? | Compute at Fish scale | Pitfalls | Adaptation |
|---|---|---|---|---|
| **Capacity-vector DP (§3.2)** | **Yes — core** | $6\times10^6$ ops, 8 MB; sub-ms in C | float overflow with peaked weights; naive per-step arrays waste ~$N$× | Index states by $n$ only ($i=\sum n$). Rescale per level, accumulate log-scale. |
| **Forward–backward exact marginals (§3.2)** | **Yes — core** | 2× the DP | Needs $F$ and $B$ at block boundaries; recompute or store 9 arrays | Gives $\Pr[\mathrm{origin}(c)=p]$ for every card — directly drives ask selection. |
| **Exact rejection-free sampling (§3.3)** | **Yes — core** | ~110 µs/deal NumPy; ~1 µs C | None; it is exact | Replaces GIB/Kermit-style generate-and-test entirely. |
| **Half-suit block enumeration (§3.5)** | **Yes — core** | $\le6^6$ per block; measured supports 15–33 | Unsatisfiable constraint set ⇒ $Z=0$; assert | The only exact way to use ask-legality. Also the place to fold any within-half-suit residual. |
| **Inclusion–exclusion for disjunctions (§3.4)** | Only for small $k$ | $2^k$ DP runs; 64 runs ≈ 60 ms C | $2^{60}$ for a real game | Use for one-off queries ("does $p$ hold ≥1 of $S$?"), not for the main belief. |
| **Ryser / Glynn (§3.1)** | No | $2^{54}$ | — | Useful only for unit-testing the DP on tiny instances. |
| **Sinkhorn / LSW (§4.1)** | Marginal | ~0.3 ms, ~300× faster than DP | 2–6 % marginal error, worst when sparse/peaked | Warm start or move-ordering heuristic only. |
| **Bethe permanent / BP (§4.3)** | No | convex opt, comparable to DP | $\sqrt2^{\,n}$ worst case | Keep in reserve if state space is enlarged (e.g. joint belief over deal + opponent type). |
| **JSV FPRAS (§4.4)** | **No** | 420 984 years at $n=68$ | — | Documented trap. Do not implement. |
| **SIS (Chen et al., §4.5)** | Superseded | — | Provable exponential underestimation *that looks converged* | The DP is the zero-variance limit of SIS. Skip SIS. |
| **Diaconis–Gangolli / switch chain (§4.6)** | Only as rejuvenation | polynomial mixing known for related margins | Burn-in, autocorrelation, no exact-sample guarantee | Use as an MCMC move if a persistent particle set is ever needed. |
| **Learned per-card location net (§5.3)** | **Yes** | one forward pass | Independence assumption; must be trained on *public-history* features | Its $|C|\times k$ output is exactly the $w_{i,q}$ matrix the DP consumes. Softmax rows. |
| **Policy-likelihood reweighting (§5.4–5.5)** | **Yes, with care** | $N\times$ policy evaluations; 5× slowdown in Skat | ESS collapse for $\sigma>0.8$ nats | Two-stage: exact DP proposal + self-normalised weights; temper if needed; fold half-suit-local residual into $g_b$ exactly. |
| **PuB-MDP framing (§5.6)** | **Yes — architectural** | free | Counterfactual dependence on the full partial policy | Fish is an exact PBS game; every seat computes the same $B_t$. Basis for conventions/signalling. |
| **Particle filter + reinvigoration (§6)** | **No** | — | Reinvigoration injects impossible deals | Recompute exactly each turn instead. |

### Fish-specific notes

- **6 players, 2 teams, alternating seats.** The DP is agnostic to teams; teams enter only through the *policy* model $\pi$ and the declaration decision. But teammates' beliefs are identical (public), so a declaration decision can be made on a *shared* posterior — compute $\Pr[\text{team holds all 6 of }S]$ by summing block-table entries $g_b(t)$ with $t$ supported on teammates, weighted by $F_{b-1}\!\cdot\!B_b$. This is exact and directly answers "is it safe to declare?"
- **Declaration requires naming the exact allocation.** So the quantity to maximise is not $\Pr[\text{team holds }S]$ but $\max_{\text{allocation}}\Pr[\text{that exact allocation}]$ — obtainable from the same block table by taking the max-weight $t$ *and* the max-weight assignment within it. The block enumeration already materialises every candidate allocation with its exact probability. **This is a decisive advantage over any sampling-based belief.**
- **Cards leaving the game on declaration** shrink the *live* card set but not the initial-deal variable. Keep reasoning about the initial deal (capacities fixed at 9); derive current holdings deterministically. This keeps the DP dimensions constant all game.
- **A player who drops out (no cards)** is a strong observation: it fixes the total number of their dealt cards that have been publicly extracted, which usually collapses several support sets. Feed it in as support restrictions, not as a capacity change.

---

## 8. Original experiments run for this report

All in Python 3.14 / NumPy on Apple Silicon; scripts in the session scratchpad (`dp_bench.py`, `verify.py`, `sinkhorn_ie.py`, `block_engine.py`, `ess.py`, `ess2.py`). NumPy timings are unoptimised and are upper bounds; C would be ~100× faster for the DP passes.

**8.1 Exact DP at Fish scale** (45 unknown cards, 5 players × 9, 35 % void rate, log-normal weights): $10^5$ states, forward 0.038 s, backward 0.035 s, marginals 0.024 s, $Z=1.50\times10^{20}$. Forward/backward consistent to $<10^{-9}$; marginal row sums = 1; **column sums exactly $[9,9,9,9,9]$**.

**8.2 Exactness vs brute force** (9 cards, 3 players × 3, full enumeration): $Z$ relative error **$3.1\times10^{-16}$**, max marginal error **$2.2\times10^{-16}$**. Exact sampler over $2\times10^5$ draws reproduced the true deal probabilities (e.g. 0.33106 true vs 0.33033 empirical; 0.11956 vs 0.11842; 0.05542 vs 0.05509).

**8.3 Rejection-sampling acceptance rates** (45 cards, 5 players × 9, per-card uniform over legal holders then check counts), $2\times10^5$ trials each:

| void fraction | acceptance |
|---|---|
| 0.00 | 0.061 % |
| 0.20 | 0.068 % |
| 0.35 | 0.057 % |
| 0.50 | 0.039 % |

≈ 1 500–2 600 attempts per accepted deal, *before* any policy weighting.

**8.4 Full engine with disjunctive constraints.** 9 half-suit blocks × 5 unknown cards, 5 players × 9, **27 ask-legality constraints derived from a real sampled deal**: block tables 0.0062 s (supports 15–33), forward 0.065 s, backward 0.064 s (agreement $4.2\times10^{-16}$), exact marginals 0.588 s; $Z=3.955\times10^{22}$; column totals exactly 9. **Total 0.72 s NumPy.** Separately, the single-constraint inclusion–exclusion check gave $\Pr=0.629172$ vs $0.629990$ over $10^5$ exact samples. Inclusion–exclusion cost: $k=1$ 0.14 s, $k=3$ 0.52 s, $k=6$ 3.99 s (64 DP runs).

**8.5 Sinkhorn vs exact marginals** — table in §4.1.

**8.6 ESS of the two-stage estimator** — table in §5.5; exact DP sampler measured at **110.7 µs/sample** in NumPy.

---

## 9. Pitfalls, negative results, failure modes

1. **JSV FPRAS is infeasible in practice** despite $O(n^7\log^4 n)$: hidden constants of order $10^7$; 420 984 years at the Ryser crossover $n=68$ (Newman & Vardi 2020).
2. **SIS can underestimate exponentially while appearing to converge**, for any row/column ordering (Bezáková–Sinclair–Štefankovič–Vigoda). Convergence diagnostics on SIS estimators are not trustworthy.
3. **Rejection sampling on hand sizes is a 0.04–0.07 % acceptance trap** (measured). GIB's 1–2 s to produce 50 deals is the visible symptom.
4. **Importance weights collapse fast:** $\mathrm{ESS}/N\approx e^{-\sigma^2}$. At $\sigma=1.8$ nats you retain 3 % of your samples. Measured and confirmed.
5. **Independence assumptions are wrong but useful.** Buro et al.: independence between features and conditional independence given features "may not be true, but allows for tractability." Solinas et al. multiply per-card location probabilities. In Fish the DP *repairs* the worst violation (hand sizes) exactly, but within-half-suit correlations remain — fold them into $g_b$.
6. **Deterministic opponent models give $P(\text{move}\mid\text{world})\in\{0,1\}$**, which is "brittle in the face of players who do not play identically to ourselves" (Buro et al.). Always soften with a temperature or a learned stochastic policy, or a single surprising opponent action zeroes your entire belief.
7. **Perfect inference can make a PIMC agent worse.** Rebstock et al.: a cheating inference module scored **−3.25** (suit) and **−8.49** (grand) TP/G against CLI, while gaining +9.82 in null. They also found PI100 beat PI20 in suit/grand but *lost* in null, "contradict[ing] the idea that a higher TSSR value corresponds to better cardplay performance." **Belief quality does not monotonically improve PIMC.** The cause is strategy fusion (Frank & Basin): a determinizing searcher chooses different actions in different states of the same information set, which it cannot actually do. Fix the search (ISMCTS, $\alpha\mu$, or a sound PBS method) alongside the belief.
8. **Non-locality** (Frank & Basin): some determinizations are vanishingly unlikely because opponents can steer play away from them; subgame values computed on them are misleading.
9. **Unsatisfiable constraint sets** give $Z=0$ silently. Observed in this study when constraints were generated independently of a true deal. Assert $Z>0$; on failure, diagnose rather than fall back silently.
10. **Numerical overflow.** $Z\sim10^{20}$–$10^{24}$ with unit weights; peaked learned weights will exceed float64. Use per-level rescaling or logs.
11. **Sinkhorn degrades exactly where Fish gets hard** — sparse supports and peaked weights (measured 5.9 % max error at 60 % void). Do not use it late-game.
12. **Counting contingency tables is #P-complete even with two rows** — the Fish tractability comes from constant column count *and* small unary capacities. If you ever generalise (e.g. variable-size hands with large counts), the tractability argument breaks.

---

## 10. Bibliography

**Permanents — exact**

- H. J. Ryser, *Combinatorial Mathematics*, Carus Mathematical Monographs No. 14, MAA, 1963. (Ryser's formula.) — cited via Chuiko et al. 2025 and Wikipedia; **original not fetched: UNVERIFIED venue details**
- D. G. Glynn, "The permanent of a square matrix," *European Journal of Combinatorics*, 2010. — cited via Chuiko et al. 2025; **UNVERIFIED venue details**
- "Computing the permanent," Wikipedia. https://en.wikipedia.org/wiki/Computing_the_permanent (Ryser and Balasubramanian–Bax–Franklin–Glynn formulas, $O(2^n n)$ with Gray code.)
- M. Chuiko, R. Richer, M. Richer, P. W. Ayers et al., "Optimizing and benchmarking the computation of the permanent of general matrices," arXiv:2510.03421, 2025. https://arxiv.org/abs/2510.03421 (Ryser Eq. 5–7, Glynn Eq. 8, rectangular Glynn extension, M1 Pro benchmarks, accuracy analysis, `opt` dispatcher.) Repo: https://github.com/theochem/matrix-permanent
- A. I. Barvinok, "Two algorithmic results for the traveling salesman problem" / work on permanents of fixed-rank matrices. — found only via search summary; **UNVERIFIED exact citation**

**Permanents — approximation**

- M. Jerrum, A. Sinclair, E. Vigoda, "A polynomial-time approximation algorithm for the permanent of a matrix with non-negative entries," *Journal of the ACM* 51(4), 2004. https://dl.acm.org/doi/10.1145/1008731.1008738 ; PDF https://faculty.cc.gatech.edu/~vigoda/Permanent.pdf
- A. Newman, M. Vardi, "FPRAS Approximation of the Matrix Permanent in Practice," arXiv:2012.03367, 2020. https://arxiv.org/abs/2012.03367 (Measured infeasibility; timing tables; $n\ge68$ crossover; mixing/sample constants.)
- N. Linial, A. Samorodnitsky, A. Wigderson, "A deterministic strongly polynomial algorithm for matrix scaling and approximate permanents," *Combinatorica* 20(4), 2000. https://www.math.ias.edu/~avi/PUBLICATIONS/MYPAPERS/LSW98/lsw00.pdf (Theorem 1.1: $\mathrm{per}(A)\le f(A)\le e^n\mathrm{per}(A)$; $O((n/\epsilon)^2)$ Sinkhorn iterations, $O(n^6)$ total; $\tilde O(n^7\log(1/\epsilon))$ for general $(r,c)$; Prop. 2.2 scalability characterisation.)
- P. O. Vontobel, "The Bethe permanent of a non-negative matrix," arXiv:1107.4196 / *IEEE Trans. Inform. Theory*, 2013. https://arxiv.org/abs/1107.4196 (Bethe free energy, Corollary 15, concavity, Lemma 48.)
- N. Anari, S. Oveis Gharan, "A tight analysis of Bethe approximation for permanent," arXiv:1811.02933. https://arxiv.org/abs/1811.02933 ($\mathrm{perm}\le\sqrt2^{\,n}\,\mathrm{perm}_B$; resolves Gurvits' conjecture.)
- M. Chertkov, A. B. Yedidia, "Approximating the permanent with fractional belief propagation," arXiv:1108.0065. https://arxiv.org/abs/1108.0065
- L. Gurvits, P. Samorodnitsky et al., "The Bethe and Sinkhorn permanents of low rank matrices and implications for profile maximum likelihood," arXiv:2004.02425. https://arxiv.org/abs/2004.02425
- L. M. Brègman (upper bound conjectured by H. Minc), Bregman–Minc inequality $\mathrm{per}(A)\le\prod_j(r_j!)^{1/r_j}$; A. Schrijver's entropy proof and his regular-bipartite lower bound $\big((a-1)^{a-1}/a^{a-2}\big)^b$. — found via search summaries; **UNVERIFIED exact venues**
- B. L. van der Waerden conjecture, proved by D. I. Falikman and G. P. Egorychev: doubly stochastic $\Rightarrow \mathrm{per}\ge n!/n^n$. — **UNVERIFIED exact venues**

**Contingency tables / SIS / MCMC**

- Y. Chen, P. Diaconis, S. P. Holmes, J. S. Liu, "Sequential Monte Carlo methods for statistical analysis of tables," *JASA* 100(469):109–120, 2005. https://www.semanticscholar.org/paper/c4ed10cc26bc3409c250cfb974c6931f232406bd
- I. Bezáková, A. Sinclair, D. Štefankovič, E. Vigoda, "Negative examples for sequential importance sampling of binary contingency tables," *Algorithmica* (and ESA 2006). https://people.eecs.berkeley.edu/~sinclair/sis_jasa.pdf (SIS proposal Eq. 2; Theorem 2 exponential underestimation.)
- P. Diaconis, B. Kolesnik, "Randomized sequential importance sampling for estimating the number of perfect matchings in bipartite graphs," arXiv:1907.02333; *Advances in Applied Mathematics*, 2021. https://arxiv.org/abs/1907.02333 (KL criterion $N^*\gg e^{L+\sigma}$; almost-perfect algorithms with $O(1)$ samples.)
- S. Chatterjee, P. Diaconis, "The sample size required in importance sampling." — cited as [9] in Diaconis–Kolesnik (Theorem 2.1); **original not fetched: UNVERIFIED**
- P. Diaconis, A. Gangolli, "Rectangular arrays with fixed margins," in *Discrete Probability and Algorithms*, Springer, 1995, pp. 15–41. — via search summary; **UNVERIFIED**
- M. Dyer, C. Greenhill, "Polynomial-time counting and sampling of two-rowed contingency tables," *Theoretical Computer Science*, 2000. — via search summary; **UNVERIFIED**
- M. Cryan, M. Dyer, "A polynomial-time algorithm to approximately count contingency tables when the number of rows is constant," *JCSS*, 2003. https://www.sciencedirect.com/science/article/pii/S002200000300014X
- M. Cryan, M. Dyer, L. A. Goldberg, M. Jerrum, R. Martin, "Rapidly mixing Markov chains for sampling contingency tables with a constant number of rows," *SIAM J. Comput.*, 2006. https://webspace.maths.qmul.ac.uk/m.jerrum/papers/CDGJM06.pdf
- C. Greenhill, "The switch Markov chain for sampling irregular graphs," arXiv:1412.5249 / SODA 2015. https://arxiv.org/abs/1412.5249
- I. Beichl, F. Sullivan, "Approximating the permanent via importance sampling with application to the dimer covering problem," *J. Comput. Phys.*, 1999. — via search summary; **UNVERIFIED**

**Card-game inference and search**

- M. L. Ginsberg, "GIB: Imperfect information in a computationally challenging game," *JAIR* 14:303–358, 2001. https://www.jair.org/index.php/jair/article/view/10279 (Constrained deal generator + rejection against the bidding module; 1–2 s per deal set; sample size 50/100; Bayes reweighting $\sum_d w_d s(m,d)$.)
- M. Buro, J. R. Long, T. Furtak, N. Sturtevant, "Improving state evaluation, inference, and search in trick-based card games," *IJCAI 2009*, pp. 1407–1413. https://www.ijcai.org/Proceedings/09/Papers/236.pdf (Eq. 1 feature-factorised inference; Kermit; Table 1 tournament results.)
- C. Solinas, D. Rebstock, M. Buro, "Improving search with supervised learning in trick-based card games," *AAAI 2019*. https://arxiv.org/abs/1903.09604 (Eq. 1 product of card-location probabilities; TSSR Eq. 2; BDCI vs KI vs NI; 3.1× slowdown.)
- D. Rebstock, C. Solinas, M. Buro, N. R. Sturtevant, "Policy based inference in trick-taking card games," *IEEE CoG 2019*. https://arxiv.org/abs/1905.10911 (Reach probability Eq. 1; Algorithm 1; Tables III–V including the cheating-inference negative result.)
- I. Frank, D. Basin (and H. Matsubara), work on strategy fusion and non-locality in imperfect-information search, with bridge card-play case study, 1998. — via search summaries and Sturtevant lecture notes https://www.cs.du.edu/~sturtevant/w13-games/Lecture10.pdf ; **UNVERIFIED exact venue**
- N. Sturtevant et al., "Understanding the success of perfect information Monte Carlo sampling in game tree search." https://webdocs.cs.ualberta.ca/~nathanst/papers/pimc.pdf
- T. Cazenave, V. Ventos, "The $\alpha\mu$ search algorithm for the game of bridge," arXiv:1911.07960. https://arxiv.org/abs/1911.07960
- R. Pavlicek, "Dealing with Constraints." https://www.rpbridge.net/8h11.htm (Pattern enumeration + weighted selection instead of rejection; honour-bitmap lookup tables; 62 M-case tables.)
- T. Andrews, *Deal* bridge hand generator, "smart stack" optimisation. — via search summary of https://medium.com/@georgeleung_7777/generating-bridge-deals-100x-faster-than-python-957839cac3d4 ; **UNVERIFIED primary source**
- "Literature (card game)," Wikipedia. https://en.wikipedia.org/wiki/Literature_(card_game) ; rules at https://www.pagat.com/quartet/literature.html ; an open-source implementation with elimination-based inference: https://github.com/Ryan1729/canadian-fish

**Public belief states, POMDPs, particle methods**

- J. N. Foerster, F. Song, E. Hughes, N. Burch, I. Dunning, S. Whiteson, M. Botvinick, M. Bowling, "Bayesian action decoder for deep multi-agent reinforcement learning," *ICML 2019*. https://arxiv.org/abs/1811.01458 (Public belief $B_t=P(f^{\mathrm{pri}}_t\mid f^{\mathrm{pub}}_{\le t})$; update Eq. 1–2; PuB-MDP; reward Eq. 3; 24.174 Hanabi; 40 % less belief uncertainty.)
- A. Nayyar, A. Mahajan, D. Teneketzis, common-information / public-belief approach, 2013. — cited throughout BAD; **original not fetched: UNVERIFIED**
- H. Hu, A. Lerer, N. Brown, J. Foerster, "Learned belief search: efficiently improving policies in partially observable settings," arXiv:2106.09086, 2021. https://arxiv.org/abs/2106.09086 (55–91 % of exact-search benefit at 4.6×–35.8× less compute.)
- D. Silver, J. Veness, "Monte-Carlo planning in large POMDPs," *NIPS 2010*. https://dspace.mit.edu/bitstream/handle/1721.1/100395/Silver_Monte-carlo.pdf (POMCP; particle deprivation and reinvigoration.)
- L. Martino, V. Elvira, F. Louzada, "Effective sample size for importance sampling based on discrepancy measures," *Signal Processing*, 2017. https://www.sciencedirect.com/science/article/abs/pii/S0165168416302110 ($\mathrm{ESS}=1/\sum \tilde w^2$, range $[1,N]$.)
- N. Chopin, work on SMC resampling/rejuvenation. — referenced via search summaries; **UNVERIFIED exact citation**

---

## 11. Recommended implementation plan (condensed)

1. Represent the belief as: per-card support sets $A_i$, per-card weights $w_{i,q}$ (from a learned card-location net, row-softmaxed), a per-half-suit list of disjunctive constraints, and constant capacities $c_q=9$.
2. Build the 9 half-suit block tables $g_b$ by exhaustive enumeration + exact constraint filtering (§3.5). Cache and update incrementally as new constraints arrive.
3. Run the capacity DP forward and backward over blocks, indexing states by $n$ alone. Rescale per level; keep a running log-scale.
4. Expose: $Z$ (information-set mass), exact per-card marginals, exact $\Pr[\text{team holds all of }S]$ and the arg-max exact allocation (for the declaration decision), and an exact i.i.d. deal sampler.
5. For search, draw $N$ exact deals; reweight only by the residual policy likelihood; monitor $\mathrm{ESS}$ and temper the residual whenever $\sigma>0.8$ nats.
6. Do **not** build a particle filter; recompute each turn.
7. Test the DP against brute force on 3-player/9-card instances (machine precision agreement is achievable, §8.2) and assert $Z>0$ on every update.
