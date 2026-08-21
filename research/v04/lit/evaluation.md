# Rigorous Evaluation of Imperfect-Information Game Agents
### Literature review for the Canadian Fish / Literature bot project — v0.4
*Scope: how serious researchers prove one agent is better than another in a stochastic imperfect-information game; variance reduction, exploitability, rating systems, population evaluation, and the statistics of paired game outcomes.*

---

## 1. Executive summary

1. **Raw head-to-head win rates are catastrophically sample-inefficient in stochastic hidden-information games.** In heads-up limit poker the per-hand standard deviation is ~5,100 mbb; a typical target precision of 50 mbb/hand needs ~50,000+ hands (White & Bowling 2009). Fish has an analogous problem: the entire outcome is dominated by the initial 9-card deal.
2. **The single highest-leverage technique is a duplicate / paired-deal design.** In duplicate poker, replaying the same card sequence with seats reversed cuts the standard deviation to roughly 2/3 (≈33% reduction) at 2× the compute (ACPC rules). Bridge has used this for a century (IMP scoring on duplicate boards). Fish admits a *stronger* version: the cyclic group of 6 seat rotations gives a fully balanced block design.
3. **Control-variate estimators (advantage sum → DIVAT → MIVAT → AIVAT) are the state of the art** and are *provably unbiased for any choice of heuristic value function*. AIVAT reduced HUNL standard deviation by 68.8% and Leduc by up to 99.9% (Burch et al. 2018); DeepStack reported an **85% standard-deviation reduction**, enabling significance with as few as 3,000 hands (Moravčík et al. 2017).
4. **AIVAT's power scales with how many players' strategies you know.** With only chance known it degrades to MIVAT. In Fish you know *all three* of your own team's policies and (in offline benchmarking) usually the opponents' too — the best case for AIVAT.
5. **Fish has an unusually favourable structure for these estimators**: the *only* chance node is the root deal, and every subsequent event is public. So MIVAT's "luck" term collapses to a single root correction `V(deal) − E_d[V(d)]`, whose mean can be estimated offline to arbitrary precision by cheap Monte Carlo over deals.
6. **Danger: a learned heuristic value function fitted on the evaluation data destroys all guarantees.** Kim & Sandholm (2026) show you can gradient-descend the heuristic to make *every* player in the Pluribus dataset appear to win >2000 mbb/h, and separately to p-hack any player to both "significantly won" and "significantly lost" (p < 10⁻²⁰⁰). **The value function must be frozen before the evaluation data is seen.**
7. **Inverse-variance weighting of AIVAT estimates buys a further ~24.5% standard-error reduction (≈43% fewer trials)** at the cost of possible bias, which vanishes when the weights are statistically independent of the estimates (Kim & Sandholm 2026).
8. **Continuous monitoring of a fixed-sample CI is fraudulent**: 61.35% false-positive rate under the null in Li, Chen & Huang (2026). Use anytime-valid confidence sequences (empirical-Bernstein or asymptotic CS) if you want to stop early; they report a **median 74× reduction in games needed** vs. raw outcomes for HUNL at ±1 BB.
9. **Exploitability, not head-to-head, is the absolute measure.** Head-to-head and exploitability can move in opposite directions: Act1 lost to Slumbot head-to-head in ACPC 2016 yet was *less* exploitable under LBR (Lisý & Bowling 2017). Waugh et al. (2009) prove that refining an abstraction does *not* monotonically reduce exploitability.
10. **Local Best Response (LBR) is a cheap, implementable lower bound on exploitability** — a one-ply greedy best response using the exact Bayesian posterior over the opponent's private information plus a rollout heuristic. It showed top ACPC bots were exploitable for >3,180 mbb/h, i.e. 4× worse than folding every hand.
11. **LBR's failure mode matters: it only ever gives a lower bound, and can return a negative (useless) one.** LBR scored 0 against DeepStack and −536 mbb/g against a no-card-abstraction bot whose true exploitability was 90 mbb/g.
12. **Elo/Bradley-Terry assume transitivity and are inflated by redundant clones.** Nash averaging (Balduzzi et al. 2018) is invariant to redundant agents; α-Rank (Omidshafiei et al. 2019) generalises to n-player, general-sum, asymmetric games in polynomial time — the right tool for a 6-player 2-team meta-game. Whole-History Rating (Coulom 2008) beat Elo/Glicko/TrueSkill/decayed-history on 2.3M KGS Go games and is the right tool for a *training-time* rating curve.
13. **Sequential testing (SPRT/GSPRT) is the practical workhorse for "is this commit better?"** Chess engine testing has a clean, directly transferable formalism: normalised Elo, `T ≈ 1,046,535 / (e_{n,1} − e_{n,0})²` worst-case expected games at α = β = 0.05.
14. **Multiple-comparison correction is not optional.** Jordan et al. (2020) use `δ′ = δ/(|A||M|)` Bonferroni-style corrections and show naive percentile bootstrap had a 5.7–11.2% failure rate at nominal 5%.
15. **Team play adds a whole extra evaluation axis: cross-play.** Self-play score massively overstates competence when conventions are private. Inter-seed cross-play (train N seeds, evaluate mixed teams) is the accepted standard; Wolski et al. (2026) give evidence it is a reliable proxy for cross-implementation robustness, but only tested one environment/algorithm.

---

## 2. Techniques, with mathematical formulations

### 2.0 Notation

Following the standard extensive-form model (Osborne & Rubinstein 1994, as used by Bowling et al. 2008):
- players `N`, chance `c`; histories `H`, terminal histories `Z ⊆ H`; actions `A(h)`.
- `P(h)` is the player to act; `f_c(a|h)` the chance distribution.
- information partition `I_i` for player `i`; behaviour strategy `σ_i(I, a)`.
- `π^σ(h) = Π_{i ∈ N ∪ {c}} π^σ_i(h)` is the reach probability, decomposed by contributor; `π^σ_{−i}(h)` is everything except `i`.
- utility `u_i : Z → ℝ`; the target quantity is

$$\mathbb{E}_{z \mid \sigma}[V(z)] \;=\; \sum_{z \in Z} \pi^{\sigma}(z)\, V(z), \qquad V = u_i \text{ usually.}$$

- Plain Monte Carlo from `t` i.i.d. samples:

$$\hat{\mu}_{\text{MC}} \;=\; \frac{1}{t}\sum_{k=1}^{t} V(z_k), \qquad \operatorname{Var}(\hat{\mu}_{\text{MC}}) = \tfrac{1}{t}\operatorname{Var}\!\big(V(z)\mid \sigma\big).$$

**Fish instantiation.** `N` = 6 players in two teams; chance acts exactly once, at the root, dealing 9 cards to each of 6 players — `54! / (9!)^6 ≈ 10^{38}` outcomes. All subsequent nodes are player nodes and all actions/outcomes are public. Hence `π^σ_c(z) = P(\text{deal})` is a single factor, and `π^σ_i(z)` is a product over player `i`'s asks/declarations. `V(z)` is naturally the *set differential* `s_A(z) − s_B(z) ∈ {−9, …, +9}` (or the win indicator).

---

### 2.1 Duplicate / paired-deal designs and matched seeds

**Bridge (the origin).** Duplicate bridge replays each *board* (a fixed deal) at multiple tables with the same cards held by the corresponding compass directions, then compares. Raw point differences are converted to International Match Points via a nonlinear, saturating table (0–24 IMPs) which damps the influence of single huge scores; World Bridge Federation Victory Point scales are calibrated per match length with IMP standard deviations of roughly `15√N` for `N` boards (Wikipedia, *International Match Points*; WBF VP scales — **the `15√N` figure is a secondary source and should be treated as UNVERIFIED**).

**Duplicate poker (ACPC).** A *duplicate match pair* consists of two matches in which the same card sequence is dealt with the seats of the players reversed. The 2017 ACPC used three variance-reduction devices: duplicate matches, **common seeds** across pairings (match #1 of A-vs-B uses the same cards as match #1 of A-vs-C and B-vs-C, so total-bankroll comparisons are made against the same opponents with the same cards), and bootstrapping for significance. Reported effect: the duplicate standard deviation is roughly `2/3` of the plain standard deviation (≈1/3 reduction). *(ACPC competition rules page; retrieved via search snippet — the site's TLS certificate blocked direct fetch, so treat the exact wording as **partially verified**.)* Libratus and Claudico both used mirrored hands (Brown & Sandholm 2018).

**Variance algebra.** Let `X` be team A's score, `d` the deal, `s` the seating. Decompose

$$\operatorname{Var}(X) \;=\; \underbrace{\operatorname{Var}_d\!\big(\mathbb{E}[X \mid d]\big)}_{\text{deal luck}} \;+\; \underbrace{\mathbb{E}_d\!\big[\operatorname{Var}(X \mid d)\big]}_{\text{play + policy noise}}.$$

A duplicate design annihilates (most of) the first term. Let `G` be a group of seat relabellings and let `ε(g) = +1` if relabelling `g` leaves your agent-team on the "even" seats and `−1` otherwise. The **balanced block estimator** is

$$\hat{\theta} \;=\; \frac{1}{n}\sum_{i=1}^{n} Y(d_i), \qquad Y(d) \;=\; \frac{1}{|G|}\sum_{g \in G} \varepsilon(g)\, X\big(d, g\big).$$

`\hat\theta` is unbiased for the expected edge (up to the group-orbit normalisation), and

$$\operatorname{Var}(\hat{\theta}) \;=\; \tfrac{1}{n}\operatorname{Var}_d\!\big(Y(d)\big) \;\ll\; \tfrac{1}{n\,|G|}\operatorname{Var}(X) \quad\text{whenever deal luck dominates.}$$

Note the *resampling unit is the deal*, not the game — all `|G|` games from one deal are one cluster.

**Fish-specific construction.** Seats `0..5` alternate teams (`0,2,4` = team A). Let `ρ` be "rotate all hands by one seat". Then:
- `ρ` (odd rotations) **swaps which team holds which set of hands** — the exact analogue of duplicate bridge's N-S/E-W reversal.
- `ρ²` (even rotations) **permutes which teammate holds which hand within a team** — removes "which of my three teammates got the strong hand".
- The full cyclic orbit `{ρ⁰,…,ρ⁵}` gives 3 even and 3 odd relabellings, i.e. a fully balanced block of 6 games per deal.

This is strictly stronger than duplicate bridge and costs exactly 6× the games per deal. Because Fish is (approximately) constant-sum in sets — 9 sets are always allocated — `Σ_g ε(g) X(d,g)` cancels the deal's intrinsic "one side got the cards" component almost exactly.

---

### 2.2 Advantage sum / control variates (DIVAT)

The general recipe (Zinkevich et al. 2006, as restated by White & Bowling 2009). Given any real-valued function `V_j : H → ℝ` on histories, define

$$S_{V_j}(z) = \sum_{\substack{ha \sqsubseteq z \\ P(h) \neq c}} \big[V_j(ha) - V_j(h)\big] \quad\text{(\emph{skill})},\qquad
L_{V_j}(z) = \sum_{\substack{ha \sqsubseteq z \\ P(h) = c}} \big[V_j(ha) - V_j(h)\big] \quad\text{(\emph{luck})},$$

$$P_{V_j} = V_j(\varnothing).$$

Telescoping gives the exact decomposition

$$u_j(z) \;=\; S_{V_j}(z) + L_{V_j}(z) + P_{V_j},$$

and the **advantage-sum estimator** is

$$\hat{u}_{V_j}(z) \;=\; S_{V_j}(z) + P_{V_j} \;=\; u_j(z) - L_{V_j}(z).$$

If `E[L_{V_j}(z) | σ] = 0` (the **zero-luck constraint**) then `\hat u` is unbiased. DIVAT is the hand-crafted instantiation for two-player limit hold'em, giving a ~3× per-trial variance reduction (⇒ 9× fewer hands).

The general control-variate view: with `w(·)` of known mean `ω`, `\hat v(z) = v(z) − c(w(z) − ω)`, optimal `c^* = \operatorname{Cov}(v,w)/\operatorname{Var}(w)` and

$$\operatorname{Var}(\hat v) \;=\; \big(1 - \operatorname{Corr}(v,w)^2\big)\operatorname{Var}(v).$$

**This is the key number to design against:** a control variate with correlation 0.9 to the outcome gives a 4.8× variance reduction; 0.95 gives 10.3×.

---

### 2.3 MIVAT: learning the value function (White & Bowling 2009)

MIVAT removes the need for a hand-designed `V`. Define `V_j` only on histories directly after chance nodes, and *define* it at chance nodes so the zero-luck constraint holds automatically:

$$V_j(h \text{ s.t. } P(h)=c) \;\equiv\; \sum_{a' \in A(h)} f_c(a'\mid h)\, V_j(h a'),$$

so that

$$L_{V_j}(z) \;=\; \sum_{\substack{ha \sqsubseteq z \\ P(h)=c}} \Big( V_j(ha) - \sum_{a' \in A(h)} f_c(a'\mid h) V_j(ha') \Big).$$

Restrict to linear `V_j(h) = φ(h)^\top θ_j` with features `φ : H → ℝ^d`. Define

$$A_t \;=\; \sum_{\substack{ha \sqsubseteq z_t \\ P(h)=c}} \Big( φ(ha) - \sum_{a' \in A(h)} f_c(a'\mid h) φ(ha') \Big), \qquad \bar A = \tfrac{1}{T}\sum_t A_t,\quad \bar u = \tfrac{1}{T}\sum_t u_t .$$

Then `\hat u_{θ_j}(z_t) = u_t − A_t^\top θ_j` and minimising the *sample* variance

$$C(θ_j) = \sum_{t=1}^{T}\Big[(u_t - \bar u) - (A_t - \bar A)^\top θ_j\Big]^2$$

has the ordinary-least-squares closed form

$$\boxed{\;θ_j^{*} \;=\; \Big[\sum_{t=1}^{T}(A_t - \bar A)(A_t - \bar A)^\top\Big]^{-1}\Big[\sum_{t=1}^{T}(A_t - \bar A)(u_t - \bar u)\Big].\;}$$

**Empirical results (mbb/hand standard deviations):**

| Domain | Money (raw) | MIVAT | MIVAT+ (DIVAT feature) | DIVAT |
|---|---|---|---|---|
| 2p limit, bot-vs-human | 5.669 | 2.387 | 2.220 | 2.238 |
| 2p limit, bot-vs-bot | 5.438 | 2.542 | 2.454 | 2.506 |
| 2p **no-limit** | 42.34 | 32.41 | — | none exists |
| **6-player** limit | 28.01 | 22.92 | — | none exists |

The 6-player result — a mere **~18–20% standard deviation reduction** — is the most directly relevant and most sobering data point for Fish. White & Bowling attribute it to a poor feature set (`HS^{k}` for `k` = number of unfolded players, with no pot-equity and no positional features). **Lesson: in multiplayer games, feature engineering for the control variate is the whole game.**

---

### 2.4 Imaginary observations / importance sampling (Bowling, Johanson, Burch & Szafron, ICML 2008)

Observe `z ~ π^{\hatσ}` but evaluate `E_{z|σ}[V(z)]` (possibly `\hatσ = σ`). Choose a map `U : Z → 2^Z` with `z' ∈ U(z')`. The estimator is

$$\boxed{\;V_U(z) \;\equiv\; \sum_{z' \in U^{-1}(z)} V(z')\,\frac{\pi^{\sigma}(z')}{\pi^{\hat{\sigma}}(U(z'))}.\;}$$

**Theorem 1 (unbiasedness).** If `π^{\hatσ}_i(z) > 0` for all `z ∈ Z`, then `E_{z|\hatσ}[V_U(z)] = E_{z|σ}[V(z)]`.
*Proof sketch:* `E_{z|\hatσ}[1(z ∈ U(z'))] = π^{\hatσ}(U(z'))`, which cancels the denominator, leaving `Σ_{z'} V(z')π^σ(z')`.

Four concrete choices of `U` for which the weight depends only on player `i`'s own (known) strategy:

1. **Basic importance sampling.** `U(z) = {z}` ⇒ weight `= π^σ_i(z')/π^{\hatσ}_i(z')`.
2. **Game-ending actions.** Let `S_{−i}(z)` be the shortest prefix of `z` after which all remaining actions belong to `i` or chance; `U(z) = {z' : S_{−i}(z) \sqsubseteq z'}`. Weight `= π^σ_i(z') / π^{\hatσ}_i(S_{−i}(z'))`. (In poker: "Early Folds" — sum over every point at which the player *could* have folded.)
3. **Private information.** `U(z) = \{z' : ∀σ,\ π^σ_{−i}(z') = π^σ_{−i}(z)\}` — all histories the opponents cannot distinguish. Weight `= π^σ_i(z') / π^{\hatσ}_i(U(z'))`. (In poker: "All Cards" — sum over all private hands you could have held.)
4. **Combined** (private info × game-ending action). Weight `= π^σ_i(z') / π^{\hatσ}_i(Q(z'))`.

**Empirical results (1M hands, mbb/hand; RMSE for a 1000-hand match):**

| Estimator | On-policy StdDev | Off-policy StdDev (min–max) | Off-policy bias |
|---|---|---|---|
| Basic (= Monte Carlo on-policy) | 5,102–5,385 | 20,559–244,469 | 49–200 |
| DIVAT | 1,935–2,011 | 11,350–138,834 | 2–62 |
| BC-DIVAT | 2,891–2,930 | 12,862–173,715 | 10–103 |
| **AC+GE+BC-DIVAT** | **1,701–1,778** | **1,816–2,857** | **2–9** |

Two lessons that transfer directly: (a) *"Early Folds" alone gave essentially zero variance reduction on-policy but was valuable in combination*; (b) *the All-Cards (private-information) estimator is what makes off-policy evaluation practical at all*, cutting off-policy standard deviation by up to two orders of magnitude. In the **partial-information** case (opponent's cards never revealed) unbiasedness is lost, but All-Cards showed no statistically significant empirical bias whereas DIVAT variants did (bias 56–282 mbb/h).

---

### 2.5 AIVAT (Burch, Schmid, Moravčík & Bowling, AAAI 2018)

AIVAT = advantage sum (correction terms for *both* chance and known-strategy player actions) ⊗ imaginary observations. Writing `U(h)` for the set of histories differing from `h` only in the private information of `P(h)`, and `K(z)` for the set of prefixes `h·a ⊑ z` where the action distribution at `h` is known (chance, or a player whose strategy we hold):

$$\hat{v}(z) \;=\; \underbrace{\frac{\sum_{z' \in U(z)} \pi(z')\, v(z')}{\sum_{z' \in U(z)} \pi(z')}}_{\text{base term } \hat v_b(z)} \;+\; \underbrace{\sum_{h\cdot a \in K(z)} \left(\frac{\sum_{a' \in A(U(h))}\sum_{h' \in U(h)} \pi(h'a')\,v'(h'a')}{\sum_{h' \in U(h)} \pi(h')} \;-\; \frac{\sum_{h' \in U(h)} \pi(h'a)\,v'(h'a)}{\sum_{h' \in U(h)} \pi(h'a)}\right)}_{\text{correction term } \hat v_c(z)}$$

**Lemma 1.** `E_{z∈Z}[k_H(z)] = 0` for each correction term. **Theorem 1.** `E_{z∈Z}[Σ_H k_H(z)] = 0`, hence AIVAT is unbiased **for any** `v'`.

**Empirical results (Burch et al. 2018):**

| Domain / setting | Estimator | Mean | Std Dev | Reduction |
|---|---|---|---|---|
| Leduc self-play (100k) | chip count | 0.0137 | 3.513 | — |
| | MIVAT | 0.0045 | 2.327 | 33.8% |
| | MIVAT+IO | 0.0099 | 1.928 | 45.1% |
| | **AIVAT (one agent known)** | −0.00009 | 0.00643 | **99.8%** |
| | **AIVAT (full knowledge)** | −0.00001 | 0.00377 | **99.9%** |
| Leduc mismatched | AIVAT | 0.6905 | 1.437 | 75.1% |
| HUNL self-play (1M) | chip count | 0.0387 | 25.962 | — |
| | MIVAT | 0.0204 | 21.293 | 18.0% |
| | MIVAT+IO | 0.0260 | 16.073 | 38.1% |
| | **AIVAT** | 0.0019 | 8.095 | **68.8%** |
| HUNL mismatched | AIVAT | −0.1097 | 8.301 | 68.5% |

Headline claim: *"reduce the number of hands needed to draw statistical conclusions by more than a factor of 10."* DeepStack reported an **85% standard-deviation reduction** with its own value network as `v'`, giving significance at 3,000 hands.

**Landmark applications.** DeepStack (Science 2017): 33 professionals, 44,852 hands, +492 mbb/g raw, +486 mbb/g under AIVAT — the AIVAT margin being >20 standard deviations from zero versus >4 for the raw estimate. Pluribus (Science 2019, 6-player): 5H+1AI, 10,000 hands, **+48 mbb/game with SE 25, p = 0.028**; 1H+5AI, 10,000 hands, **+32 mbb/game with SE 15, p = 0.014**; one-tailed t-tests at 95%. *(Pluribus figures read from a secondary summary of the Science paper plus Kim & Sandholm's Table 3, which independently reports "AIVAT [6,5] Uniform 48 SE 25" — treat as **cross-verified secondary**.)*

Note that **Pluribus finished the human experiment with a negative raw payoff overall; AIVAT is what showed it was superhuman** (Kim & Sandholm 2026). That is either the strongest advertisement for variance reduction or the strongest warning about it, depending on your temperament — see §5.

---

### 2.6 AIVAT pathologies and inverse-variance weighting (Kim & Sandholm, arXiv 2026)

Rewrite AIVAT as an affine function of the heuristic outputs:

$$\hat v(z) \;=\; b(z) + \sum_{h \in H} c(z)_h\, v'(h) \;=\; b(z) + \langle c(z), θ\rangle \quad\text{with } v'_θ(h) = θ_h .$$

**Proposition 1.** There exists `θ^*` minimising the sample variance of the estimates (a least-squares problem). Fitting it on the *evaluation data* is a disaster: on the 10,000-hand Pluribus dataset it produced win rates of **>2,000 mbb/h for every one of the 14 players simultaneously**, violating the zero-sum constraint.

Worse, gradient ascent/descent on the **t-statistic**

$$\text{Optimize}_{θ} \quad \frac{\bar v_θ - μ_0}{s_θ / \sqrt{T}}, \qquad s_θ^2 = \tfrac{1}{T-1}\sum_t\big(\hat v_θ(z_t) - \bar v_θ\big)^2$$

let them prove, for every player, both "significantly won" and "significantly lost", with p-values down to `10^{-1399}`.

> **Takeaway (verbatim in spirit): the heuristic value function must be fixed prior to observing the evaluation data.**

**Uncertainty propagation.** Treating `v'` outputs as random with covariance `Σ(z)`,

$$\operatorname{Var}(\hat v(z)) \;=\; c(z)^\top Σ(z)\, c(z) \;\;\xrightarrow{\text{uncorrelated}}\;\; \sum_{h} c(z)_h^2 \operatorname{Var}(v'(h)).$$

Inverse-variance weighting with `w_t = 1/\operatorname{Var}(\hat v(z_t))`:

$$\bar v^{*} = \frac{\sum_t w_t \hat v(z_t)}{\sum_t w_t}, \qquad \operatorname{Var}(\bar v^{*}) = \Big(\sum_{t=1}^{T} \tfrac{1}{\operatorname{Var}(\hat v(z_t))}\Big)^{-1}.$$

**Proposition 2.** IVW is the minimum-variance weighted average. **Proposition 3.** It is unbiased iff the weights are independent of the data being averaged; otherwise the asymptotic bias is `\operatorname{Cov}(w, \hat v)/\mathbb{E}[w]`. Gaussian-process / Bayesian-ridge / ARD regressors give structurally independent mean and variance, so the bias vanishes under Gaussian priors. Empirically (10-fold CV on 10,000 Pluribus hands, MIVAT-GPR): SE 99 → 53 mbb/h, a **24.5% SE reduction ⇒ 43.0% fewer hands**.

---

### 2.7 Anytime-valid stopping: AV-AIVAT (Li, Chen & Huang, arXiv 2026)

Combines AIVAT with confidence sequences so you may monitor continuously and stop when precision is reached.

- **Predictable value functions:** `v_t` must be `F_{t−1}`-measurable (fixed before hand `t` is seen). This preserves `E[C_t | F_{t−1}] = 0` for the correction `C_t`, so `Y_t = X_t + C_t` is a martingale-difference-plus-mean sequence.
- **Empirical-Bernstein CS (exact, finite-sample):** half-width lower-bounded by `w^{EB}_t ≥ 4B\log(2/α)/t` where `|Y_t| ≤ B = B_X + 2KV`.
- **Asymptotic CS:** `w^{A}_t = \hatσ_t\sqrt{\dfrac{2(tρ^2+1)}{t^2ρ^2}\log\!\Big(\dfrac{\sqrt{tρ^2+1}}{α}\Big)}`.
- **Theorem 1 (stopping time, EB-CS):** `τ_{EB}(ε) ≥ 4B\log(2/α)/ε` always, and `τ_{EB}(ε) ≤ 8\max\{B\log(2/α)/ε,\; σ^2\log(2/α)/ε^2\}` w.h.p. The deterministic floor explains why loose bounds `B` destroy the benefit.
- **Theorem 2 (online value learning):** if the variance regret `R_t = o(t)` then `τ(ε) ≤ τ^*(ε)[1 + 2R_{2τ^*}/(σ^{*2}τ^*) + o(1)]` — online fitting of `v_t` is asymptotically free.

**Results:** HUNL (71,439 hands, 15 configurations) — variance reduction median 54.4× (24.2–86.0×); AsympCS stopping-time ratio at ±1 BB **median 74.17× (54.53–97.62×)**; EB-CS only 1.365× (loose bound). Leduc: variance ratio 8.08×, structural bound `B_Y = 117`. Online predictable value learning recovered 77–79% of the frozen→oracle gap. **Continuous monitoring with a naive fixed-sample CI gave a 61.35% false-positive rate under the null; EB-CS rechecking screened out 100% of the 1,227 invalid claims, AsympCS 99.92%.**

---

### 2.8 Exploitability and its approximations

**Definition.** For a two-player zero-sum game, `ε_i(σ_i) = \max_{σ_{-i}} u_{-i}(σ_i, σ_{-i}) - v^*`; the standard aggregate is

$$\text{NashConv}(σ) \;=\; \sum_{i \in N}\Big[\max_{σ'_i} u_i(σ'_i, σ_{-i}) - u_i(σ)\Big], \qquad \text{Exploitability} = \tfrac{1}{|N|}\text{NashConv}.$$

**Exact best response** requires a full game-tree traversal; Johanson, Waugh, Bowling & Zinkevich (IJCAI 2011) give an accelerated, parallelisable algorithm that avoids the full traversal and made HULH exploitability computation feasible for the first time.

**Local Best Response (Lisý & Bowling 2017).** Maintain the opponent's exact *range* `π : H → [0,1]` over private hands; after each opponent action `a` at public state `s`, Bayes-update `π(h) \mathrel{{*}{=}} σ(s,h,a)` and renormalise. Then act greedily one ply ahead assuming check/call to showdown:

```
LocalBR(π, s, h_i):
 1  wp   = WpRollout(h_i, π, s)
 2  asked = pot_{-i}(s) - pot_i(s)
 3  U(call) = wp·pot(s) − (1−wp)·asked
 4  for each considered bet/raise a:
 5     fp = Σ_{h_{-i}} π(h_{-i})·σ(s, h_{-i}, fold)
 6     π'(h) = π(h)·(1 − σ(s, h, fold));  normalise π'
 7     wp' = WpRollout(h_i, π', s)
 8     U(a) = fp·pot(s) + (1−fp)·[ wp'·(pot(s)+a) − (1−wp')·(asked+a) ]
 9  return argmax_a U(a) if max_a U(a) > 0 else fold
```

Cost: with `n` candidate bet sizes and `|H|` private hands, LBR costs at most `(n|H| + 1)` × a normal hand; only `(n+1)×` if the opponent's engine already solves for all hands at once.

**Results (mBB/h lower bounds on exploitability), 2 × 50,000 duplicate hands:**

| LBR action set / rounds | Hyp. 2013 | Hyp. 2014 | Slumbot 2016 | Act1 2016 | Full-cards bot |
|---|---|---|---|---|---|
| fc, rounds 1–4 | 1048 ± 68 | 721 ± 56 | 522 ± 50 | 407 ± 47 | −424 ± 37 |
| fcpa, rounds 3–4 | 4040 ± 147 | 3852 ± 141 | 4020 ± 115 | 2597 ± 140 | −536 ± 87 |
| 56 bets, rounds 3–4 | 5062 ± 152 | 4675 ± 152 | 3763 ± 104 | 3302 ± 122 | 2403 ± 87 |

Always-fold loses 750 mBB/h. Every bot was exploitable for **>3,180 mBB/h at 97.5% confidence** — over 4× worse than folding every hand. DeepStack: LBR could not establish any positive lower bound (reported as 0).

**Structural lessons, all of which transfer to Fish:**
- **Greedy LBR bets too early.** Forcing check/call in the first two rounds *increased* the measured exploitability by nearly an order of magnitude ("it is important to wait when using LBR").
- **Most exploitation came from a single extra action** (`fcpa` — one pot-sized bet), not from a rich action set.
- **A negative LBR score is uninformative**, not evidence of low exploitability: against a bot whose true `fcpa`-restricted exploitability was 90 mbb/g, LBR *lost* 536 mbb/g.
- Lisý & Bowling explicitly used duplicate matches + imaginary observations, reporting a ~20% shrinkage of confidence intervals for the same number of matches.

**Team games.** Fish is a *two-team zero-sum* game, so exploitability is well defined **at the team level**. The relevant solution concept is the **Team-Maxmin Equilibrium with Correlation (TMECor)**: a pair of correlated strategies, one per team, each a best response to the other, with ex-ante correlation inside a team but no in-game communication. Computing TMECor is APX-hard, and team best-response computation is much harder than single-player BR. *(Farina, Celli, Gatti, Sandholm and successors; Zhang et al., "A Marriage between Adversarial Team Games and 2-player Games", arXiv 2206.09161; Zhang, Farina et al., "Subgame Solving in Adversarial Team Games", NeurIPS 2022 — **these were located via search and abstract-level reading only; the algorithmic details are UNVERIFIED here**.)*

---

### 2.9 Head-to-head vs. exploitability discrepancies; abstraction pathologies

Waugh, Schnizlein, Bowling & Szafron (AAMAS 2009) formalise abstraction refinement (`α ⊒ β` iff `α`'s information partition is finer and its action sets are supersets) and the monotonicity properties one would like:
- **Strong monotonicity for `i`:** every equilibrium of the finer abstract game is less exploitable than every equilibrium of the coarser one.
- **Weak monotonicity:** the *best* equilibrium of the finer game is less exploitable than the best equilibrium of the coarser game.

**All useful forms fail.** They exhibit counterexamples in Leduc hold'em. The one positive result:

**Theorem 3.** If `α ⊒ β` and `α_2 ≡ β_2 ≡ φ_2` (the *opponent* plays in the null/full abstraction), then `ε_1(σ^α_1) ≤ ε_1(σ^β_1)`.
*Proof:* `v^* − ε_1(σ^α_1) = \min_{σ_2} u_1(σ^α_1,σ_2) = \max_{σ_1∈Σ^α_1}\min_{σ_2} u_1(σ_1,σ_2) \ge \max_{σ_1∈Σ^β_1}\min_{σ_2} u_1(σ_1,σ_2) = v^* − ε_1(σ^β_1)`.

Their conclusion: *"creating larger abstract games is not guaranteed to improve the quality of the strategies found. Also, different solutions to the same abstract game are not equally strong in the full game."*

Lisý & Bowling supply the matching empirical fact: **Act1 lost head-to-head to Slumbot in ACPC 2016 but was the least exploitable bot in their LBR table** — "even LBR may not be indicative of actual one-on-one performance (and vice versa)."

---

### 2.10 Rating systems for populations of agents

**Bradley-Terry / Elo.** `P(i \text{ beats } j) = γ_i/(γ_i+γ_j)`; with `r = \lnγ`, `\hat p_{ij} = σ(r_i − r_j)`. The log loss is `ℓ_{Elo}(p_{ij},\hat p_{ij}) = −p_{ij}\log\hat p_{ij} − (1−p_{ij})\log(1−\hat p_{ij})`, and online gradient descent `r_i^{t+1} = r_i^t − η∇_{r_i}ℓ` with `η ∈ {16,32}` recovers Arpad Elo's original updates (Balduzzi et al. 2018, §2.1).

**Proposition 1 (Balduzzi et al.).** Elo ratings are at a stationary point under batch updates iff the empirical and predicted win-probability matrices have equal row sums.

**Glicko** (Glickman) adds a rating deviation `RD` (posterior s.d.); Glicko-2 adds a volatility `σ`. Bayesian: `θ_i ~ N(μ_i, σ_i^2)` prior, closed-form Gaussian-approximation updates per rating period. *(Search-level only — **formulas UNVERIFIED here**.)*

**TrueSkill (Herbrich, Minka & Graepel, NIPS 2006)** — the one that natively handles *teams* and *multiplayer* outcomes:
- skill `s_i ~ N(μ_i, σ_i^2)`; performance `p_i ~ N(s_i, β^2)`;
- **team performance is the sum of member performances**, `t_j = \sum_{i \in \text{team } j} p_i`;
- outcome/draw condition `t_A > t_B + \varepsilon` (win) or `|t_A - t_B| \le \varepsilon` (draw);
- approximate message passing on a factor graph, with truncated-Gaussian moment functions

$$v(t,\varepsilon) = \frac{\mathcal N(t-\varepsilon)}{\Phi(t-\varepsilon)},\qquad w(t,\varepsilon) = v(t,\varepsilon)\big[v(t,\varepsilon) + (t-\varepsilon)\big].$$

*(The `v`/`w` forms above are the standard published forms; the automated extraction of the NIPS PDF returned a garbled variant, so **verify against the paper before implementing**.)*

**Whole-History Rating (Coulom 2008)** — the right choice when agents *improve over training*:
- natural rating `r_i(t) = \lnγ_i(t) = R_i(t)\ln 10/400`;
- Wiener-process prior on rating drift: `r_i(t_2) − r_i(t_1) ∼ N(0, |t_2 − t_1| w^2)`;
- Bayesian posterior `p(γ|G) ∝ P(G|γ)p(γ)`, maximised exactly over the *whole* history;
- Newton update on each player's rating vector `r ← r − \big(\tfrac{∂^2\log p}{∂r^2}\big)^{-1}\tfrac{∂\log p}{∂r}`;
- because the Wiener process is Markov, the Hessian is **tridiagonal**, so each Newton step is `O(\#\text{ratings})`;
- rating uncertainty ≈ `−(\text{Hessian})^{-1}` diagonal.

**Prediction rates on 10.8M KGS Go games (2.33M test games):**

| Algorithm | Time | Training | Test | Optimal parameters |
|---|---|---|---|---|
| Elo | 0.41 s | 56.001% | 55.121% | k = 20 |
| Glicko | 0.73 s | 56.184% | 55.522% | σ₀ = 150 Elo, w² = 20 Elo²/day |
| TrueSkill | 0.40 s | 56.212% | 55.536% | β²=1, σ₀²=0.5, w²=0.000975/game |
| Bayeselo | 88.66 s | 56.216% | 55.671% | prior = 1 |
| Decayed history | 89.86 s | 56.260% | 55.698% | prior = 1, τ = 400 days |
| **WHR** | 252.00 s | **56.356%** | **55.793%** | prior = 1.2, w² = 14 Elo²/day |

(95% significance threshold on the test set: 0.091% difference. WHR adds a full game in <0.001 s incrementally.)

---

### 2.11 Population / meta-game evaluation

**Nash averaging (Balduzzi, Tuyls, Pérolat & Graepel, NeurIPS 2018).** Represent agent-vs-agent evaluation by an antisymmetric logit matrix `A` (`A_{ij} = \operatorname{logit} p_{ij}`, `A^\top = −A`). Define the symmetric zero-sum meta-game `μ_1(p,q) = p^\top A q`.

**Desiderata.** *P1 Invariant* (redundant copies of an agent change nothing); *P2 Continuous*; *P3 Interpretable*. **Elo and uniform averaging fail P1.**

**Proposition 4 (maxent NE).** For antisymmetric `A` there is a unique symmetric NE `(p^*,p^*)` solving `\max_{p∈Δ_n}\min_{q∈Δ_n} p^\top A q` with greater entropy than any other NE.

**Definition 2 (Nash average).** `n_A := A · p^*_A`.

*Worked invariance example (theirs).* For the RPS-like logit matrix with entries ±4.6, `p^*_A = (1/3,1/3,1/3)`. Duplicating agent C gives `p^*_{A'} = (1/3,1/3,1/6,1/6)` — the mass splits automatically. Uniform averaging is not invariant: `\operatorname{div}(A)=0` but `\operatorname{div}(A') = (−1.15, 1.15, 0, 0)`, *falsely suggesting agent B is superior*. Nash averaging correctly reports `n_A = 0` in both cases.

**Multidimensional Elo (mElo₂ₖ).** Handles cycles by adding a low-rank antisymmetric term:

$$\hat p_{ij} = σ\big(r_i - r_j + c_i^\top Ω\, c_j\big),\qquad Ω = \sum_{k} (e_{2k-1}e_{2k}^\top - e_{2k}e_{2k-1}^\top).$$

On 8 Go algorithms from the AlphaGo paper, Elo gave `‖P−\hat P‖_F = 0.85`, `ℓ_{\log}=1.41`; mElo₂ gave `0.35` and `1.27`, and correctly predicted the `αv / αp / Zen` cycle that Elo got backwards.

**α-Rank (Omidshafiei et al., Scientific Reports 2019)** — the general `K`-population, general-sum, asymmetric method.

Fermi selection: `P(τ→σ, s_{−k}) = \big(1 + e^{α(f^k(τ,s_{−k}) − f^k(σ,s_{−k}))}\big)^{-1}`, with `α` the *ranking intensity*.

Fixation probability of a single `τ`-mutant in a population of `m` playing `σ`:

$$ρ^k_{σ,τ}(s_{−k}) \;=\; \Big(1 + \sum_{l=1}^{m-1} e^{-lα\,(f^k(τ,s_{−k}) - f^k(σ,s_{−k}))}\Big)^{-1}
\;=\;
\begin{cases}
\dfrac{1-e^{-α(f^k(τ) - f^k(σ))}}{1-e^{-mα(f^k(τ) - f^k(σ))}} & f^k(τ) \ne f^k(σ)\\[2mm]
\tfrac{1}{m} & \text{otherwise.}
\end{cases}$$

Markov chain over the `\prod_k |S_k|` pure profiles, with `η = \big(\sum_l(|S_l|-1)\big)^{-1}`:

$$C_{ij} = \begin{cases} η\,ρ^k_{s^k_i, s^k_j}(s^{-k}_i) & \exists k:\ s^k_i \ne s^k_j \text{ and } s^{-k}_i = s^{-k}_j\\ 1 - \sum_{j\ne i} C_{ij} & s_i = s_j\\ 0 & \text{otherwise.}\end{cases}$$

**Theorem 2.1.2.** `C` is irreducible for finite payoffs, so a unique stationary `π` (with `π^\top C = π^\top`, `\sum_i π_i = 1`) exists; the ordered masses of `π` are the ranking. Complexity is polynomial in the number of profiles (vs. PPAD-hard Nash).

**Sampling α-Rank under noise (Rowland et al., NeurIPS 2019).**
- **Theorem 3.1 (finite α).** With `L(α,M_{\max}) = 2α e^{2αM_{\max}}` and `g(α,η,m,M_{\max}) = η\frac{e^{2αM_{\max}}-1}{e^{2αmM_{\max}}-1}`, `\max_s|π(s)-\hatπ(s)| ≤ ε` w.p. `≥ 1−δ` if

$$N_s \;>\; \frac{648\,M_{\max}^2\log(2|S|K/δ)\,L(α,M_{\max})^2\big(\sum_{n=1}^{|S|-1}\binom{|S|}{n}n|S|\big)^2}{ε^2\,g(α,η,m,M_{\max})^2}\quad\forall s.$$

- **Theorem 3.2 (infinite α).** If all payoff gaps `|M_k(σ,s_{−k}) − M_k(τ,s_{−k})| ≥ Δ`, the transition matrix (and hence all MCCs) is recovered exactly w.p. `≥ 1−δ` if `N_s > 8Δ^{-2}M_{\max}^2\log(2|S|K/δ)`.
- **ResponseGraphUCB**: reduce ranking to a collection of pure-exploration two-armed bandit problems (one per pair of profiles differing in one player), sample adaptively, stop each edge when its confidence bound resolves the sign. Variants: Uniform (U), Uniform-Exhaustive (UE), Valence-Weighted (VW) sampling; Hoeffding (UCB), Clopper-Pearson (CP-UCB), and relaxed (R-UCB) bounds. **Theorems 4.1/4.2** give correctness w.p. `≥ 1−δ` and a sample-complexity bound. Clopper-Pearson bounds are strictly tighter than Hoeffding for Bernoulli outcomes.

**Voting-as-Evaluation (Lanctot et al. 2023).** Treat each task/matchup as a *voter* over agents; aggregate with a social welfare function (maximal lotteries is highlighted as satisfying the relevant consistency properties and being polynomial in the data size). Reported to be more robust than Elo *and* Nash averaging, and to **predict outcomes better than Elo in a complex seven-player game**. *(Abstract-level reading only.)*

**Spinning tops (Czarnecki et al., NeurIPS 2020).** Real-world games have a geometry in which the *non-transitive dimension* (number of cycles at a given transitive strength) is large in the middle of the skill range and collapses at the top and bottom. Consequence: **populations of strategies are necessary for training, and required population size is a function of that geometry.** *(Abstract-level.)*

**AlphaStar league (Vinyals et al., Nature 2019).** Main agents + league exploiters + main exploiters, trained with prioritized fictitious self-play; evaluated by (i) Elo against the entire league plus the elite built-in bot, (ii) the **Nash distribution of the league** over time, and (iii) held-out *validation agents* — measuring what fraction beat the main agents in >80/160 games. The Nash distribution putting most weight on recent players is the diagnostic for "no forgetting, no cycling". *(Search-level + figure-caption reading; **the specific numbers are UNVERIFIED here**.)*

---

### 2.12 Statistical testing for paired game outcomes

**Paired/blocked estimation.** With duplicate blocks, the analysis unit is the block-level difference `Y_i = Y(d_i)`. Use the paired `t`-statistic `t = \bar Y/(s_Y/\sqrt n)`, or better, the **cluster/block bootstrap**: resample *deals* (with replacement), recompute `\bar Y^{(b)}`, and take the `100(δ/2)` and `100(1−δ/2)` percentiles. Resampling individual games would understate the variance because games sharing a deal are strongly correlated.

**Permutation test (exact, distribution-free).** Under the sharp null "the two agent-teams are exchangeable", the sign of each block difference is exchangeable; flip signs uniformly at random `B` times to build the null distribution of `\bar Y`, and take `p = (1 + \#\{|\bar Y^{(b)}| \ge |\bar Y|\})/(B+1)`. This is the right test when `Y` is heavy-tailed or bounded and small-`n`.

**Multiple comparisons.** Jordan et al. (ICML 2020) build simultaneous intervals over `|A|` algorithms and `|M|` environments using `δ' = δ/(|A||M|)` (noting `δ/(|A|^2|M|)` is the fully correct Bonferroni level for all pairwise comparisons, and that they use the looser one as a heuristic). Their PBP-t interval:

$$Z^{\pm}_{i,j,k} = μ_{i,j,k} \pm \frac{\hatσ}{\sqrt{T_{i,j}}}\, t_{1-δ',\,T_{i,j}-1}.$$

**Their measured failure rates at nominal δ = 0.05** (1,000 replications):

| Samples | PBP FR / SIG | PBP-t FR / SIG | Bootstrap FR / SIG |
|---|---|---|---|
| 10 | 0.000 / 0.00 | 1.000 / 0.00 | 0.112 / 0.11 |
| 100 | 0.000 / 0.00 | 0.000 / 0.02 | 0.084 / 0.74 |
| 1,000 | 0.000 / 0.00 | 0.000 / 0.34 | 0.057 / 0.83 |
| 10,000 | 0.000 / 0.33 | 0.003 / 0.83 | 0.069 / 0.83 |

Their recommendation: **1,000–10,000 trials per algorithm per environment, and prefer PBP-t over the raw percentile bootstrap** (which had a 5.7–11.2% failure rate at a nominal 5%). The Šidák correction `δ' = 1-(1-δ)^{1/m}` and Holm's step-down are the standard less-conservative alternatives.

**Sequential testing: GSPRT / normalized Elo (Van den Bergh; Fishtest).** Define the **normalized t-value**

$$t_n := \frac{μ - 1/2}{σ_{pg}},$$

where `μ` is the expected score and `σ_{pg}` the per-game standard deviation (in the *pentanomial* / game-pair model, the s.d. of the block outcome scored `{0, 1/4, 2/4, 3/4, 1}` times `\sqrt 2`). Asymptotically `\hat t_n ∼ N(t_n, 1/N)`. With `C_{e/t} := 800/\log 10 = 347.43`, **normalized Elo** is `e_n := C_{e/t} t_n`, and for a balanced book `e_n = e_l/\sqrt{1-d}` where `d` is the draw ratio.

The SPRT monitors the (generalized) log-likelihood ratio and stops at the Wald bounds `\log\frac{β}{1-α}` and `\log\frac{1-β}{α}`; for `α=β=0.05` these are `≈ ∓2.94`. The **worst-case expected duration** (true strength halfway between H₀ and H₁) is

$$\boxed{\;T \;=\; \frac{D}{(e_{n,1} - e_{n,0})^2},\qquad D := C_{e/t}^2\log(19)^2 = 1{,}046{,}535 \;\approx\; 10^6 .\;}$$

Derivation: with `b = \log\frac{1-α}{α}`, `w = (s_1-s_0)/σ_{pg} = t_{n,1}-t_{n,0}`, `h_μ = \frac{2t_n - (t_{n,0}+t_{n,1})}{t_{n,1}-t_{n,0}}`, and `T(h) = \frac{2b}{h}\cdot\frac{1-e^{-hb}}{1+e^{-hb}}`, the duration is `T = T(h_μ)/w^2`; l'Hôpital at `h_μ = 0` gives `T(0) = b^2`. At `t_n = t_{n,0}` or `t_{n,1}` the numerator becomes `D' = \tfrac{9}{5}C_{e/t}^2\log 19 = 639{,}770`.

| Normalized Elo difference | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| Expected games | 1,046,535 | 261,634 | 116,282 | 65,408 | 41,861 | 29,070 |

**EGTA sample-complexity bounds (Wellman, Tuyls & Greenwald, JAIR 2025, §6.3).** `\hatΓ` is an `ϵ`-uniform approximation of `Γ` iff `\|u-\hat u\|_∞ ≤ ϵ`; then `E(Γ) ⊆ E_{2ϵ}(\hatΓ) ⊆ E_{4ϵ}(Γ)`. Global sampling with Hoeffding + Bonferroni needs

$$m \;\ge\; \frac{c^2\ln(2|Γ|/δ)}{2ϵ^2}$$

samples per profile, where `c` is the payoff range and `|Γ|` the number of game parameters. Bennett's inequality replaces the `c^2` dependence with the *wimpy variance* `\|v\|_∞`:

$$m \;\ge\; 2\ln\!\frac{2|Γ|}{δ}\left(\frac{c}{3ϵ} + \frac{\|v\|_∞}{ϵ^2}\right).$$

Bootstrapping the **regret distribution** (Wiedenbeck et al. 2014, per the survey): resample payoff data with replacement, rebuild `\hatΓ'`, compute `ϵ_{\hatΓ'}(σ^*)`, and read the distribution over regret. Also: **spurious equilibria** are common with few samples per profile and disappear as `m` grows (their experiment: many at `m=100`, steadily eliminated by `m=1000`).

---

### 2.13 Evaluating *team* / cooperative agents: cross-play

- **Self-play score is not competence.** Two copies of the same network can share an arbitrary private convention; the score says nothing about robustness.
- **Intra-algorithm (inter-seed) cross-play (Intra-XP):** train `N` seeds of the same algorithm; evaluate teams assembled from *different* seeds. Cheap and objective but blind to whether the agent can cooperate with a differently-built agent.
- **Inter-algorithm cross-play:** teams assembled from an experiment pool with no assumption of algorithmic similarity. State-of-the-art cooperative algorithms (Other-Play, Off-Belief Learning) *under-perform* in this paradigm.
- **Cross-implementation cross-play (XIXP)** (Wolski, Hoernle, Forkel & Foerster, arXiv 2026): vary implementation details (learning rate, architecture, gradient clipping) to simulate independent implementations. 176 policies across 22 implementations, 8 seeds each; 11 implementations discarded for insufficient self-play. **Finding: no statistically significant gap between within-implementation and across-implementation cross-play for Other-Play + IPPO in Yokai**, i.e. inter-seed cross-play was a reliable proxy. Caveat stated by the authors: single environment, single algorithm.
- Hanabi (Bard et al., AIJ 2020) established the two-regime protocol — *self-play* and *ad-hoc team* (held-out partner pool) — plus a 100M-environment-step training budget for the sample-limited regime. *(Direct fetch of the paper failed; **details here are search-level and should be re-verified**.)*

---

## 3. Empirical results at a glance

| Technique | Domain | Headline number | Source |
|---|---|---|---|
| Duplicate matches | ACPC poker | s.d. → ~2/3 (≈33% reduction) | ACPC rules (partially verified) |
| Duplicate + imaginary observations | HUNL (LBR eval) | ~20% narrower CIs at same #matches | Lisý & Bowling 2017 |
| DIVAT | 2p limit hold'em | ~3× variance reduction ⇒ 9× fewer hands | White & Bowling 2009 (citing Zinkevich et al. 2006) |
| MIVAT (linear, learned) | 2p limit | 5.669 → 2.220 s.d. | White & Bowling 2009 |
| MIVAT | 2p no-limit | 42.34 → 32.41 s.d. (≈25%) | ibid. |
| MIVAT | **6-player limit** | 28.01 → 22.92 s.d. (≈18–20%) | ibid. |
| AC+EF+BC-DIVAT (imaginary obs.) | 2p limit, off-policy | off-policy s.d. 244,469 → 2,857 | Bowling et al. 2008 |
| AIVAT | HUNL | 68.8% s.d. reduction; >10× fewer hands | Burch et al. 2018 |
| AIVAT | Leduc, full knowledge | 99.9% s.d. reduction | ibid. |
| AIVAT (DeepStack value net) | HUNL vs. 33 pros | 85% s.d. reduction; significance at 3,000 hands | Moravčík et al. 2017 |
| AIVAT | Pluribus 5H+1AI, 10k hands | +48 mbb/g, SE 25, p = 0.028 | Brown & Sandholm 2019 (secondary) |
| MIVAT-GPR + IVW | Pluribus data, 10k hands | 24.5% SE reduction ⇒ 43.0% fewer hands | Kim & Sandholm 2026 |
| AIVAT + AsympCS | HUNL, 71k hands | median 74× fewer games at ±1 BB | Li, Chen & Huang 2026 |
| Naive continuous monitoring | HUNL simulation | 61.35% false-positive rate under null | ibid. |
| LBR | ACPC 2016 bots | all >3,180 mBB/h exploitable (4× worse than folding) | Lisý & Bowling 2017 |
| LBR | DeepStack | 0 (no positive lower bound found) | Moravčík et al. 2017 |
| LBR | no-card-abstraction bot | −536 mBB/h (uninformative) vs. true 90 mbb/g | Lisý & Bowling 2017 |
| WHR vs Elo | 2.33M KGS Go games | 55.793% vs 55.121% prediction (0.091% = 95% threshold) | Coulom 2008 |
| mElo₂ vs Elo | 8 Go agents | `ℓ_log` 1.27 vs 1.41; cycles predicted correctly | Balduzzi et al. 2018 |
| Percentile bootstrap | RL eval, 1k–10k samples | 5.7–11.2% failure rate at nominal 5% | Jordan et al. 2020 |

---

## 4. Applicability to Canadian Fish

Recall the Fish-specific structure: **one root chance node (the deal), then a fully public transcript**. This is a best case for several of these techniques and a worst case for others.

### 4.1 Duplicate / rotation blocks — **DO THIS FIRST. Highest value/effort ratio.**
- **Would it help?** Enormously. The set differential is dominated by "which side of the table got the cards." A 6-rotation block per deal removes essentially all of it.
- **Cost.** 6× games per deal. If a self-play game costs `c`, a block costs `6c`. Because the deal-luck term typically dominates, this trades a `6×` cost for a variance reduction plausibly `>6×` on the *difference* — measure `\operatorname{Var}_d(\mathbb E[X|d])/\operatorname{Var}(X)` empirically once, then decide between `|G|=2` (team-swap only, 2× cost) and `|G|=6`.
- **Implementation.** Seed the deal RNG; store the 54-card permutation; replay with hands assigned to seats `(i+k) \bmod 6` for `k = 0..5`. Score `Y(d) = \tfrac13\sum_{k \text{ even}} X(d,k) - \tfrac13\sum_{k\text{ odd}} X(d,k)`.
- **Pitfalls.** (a) Your engine must be deterministic given (deal, RNG stream) or you lose the pairing; use *separate*, per-seat RNG streams so an agent's internal randomness doesn't leak across rotations. (b) The cluster is the deal — resample deals, not games, in the bootstrap. (c) If both teams use the *same* policy, `E[Y] = 0` by symmetry, which is a free correctness test of your harness. (d) Rotations by 2 and 4 keep the team assignment fixed, so they contribute to the *within-team* balance but not the between-team contrast; include them anyway to cancel intra-team hand-quality luck.

### 4.2 MIVAT-style root control variate — **DO THIS SECOND. Cheap, unbiased, easy.**
Because chance acts once, the MIVAT luck term collapses to a *single* term:

$$L(z) \;=\; V(\text{deal}) - \mathbb{E}_{d}\big[V(d)\big], \qquad \hat u(z) \;=\; u(z) - L(z).$$

`E_d[V(d)]` can be computed offline to arbitrary precision by sampling millions of deals — no game play needed. So:
- Define features `φ(d)` of the deal from the perspective of team A: e.g. per-half-suit counts held by team A vs team B (`Σ_s |n_A(s) - n_B(s)|`), number of half-suits where one team already holds ≥4 or all 6, number of "void" players per half-suit, the joker/8s split, hand concentration (Herfindahl over half-suits within a hand), and the count of half-suits where team A has strict majority.
- Fit `θ` by the MIVAT closed form (or plain OLS of `u` on `φ(d) - \barφ`) **on a frozen training set of deals, never on the evaluation set.**
- Expected payoff: `\operatorname{Var}` falls by `1 - \operatorname{Corr}(u, φ^\topθ)^2`. My honest prior for Fish: a good deal-strength feature set should reach `Corr ≈ 0.6–0.85` (the deal really does determine a lot), i.e. **1.5×–3.6× variance reduction**. That is worth having, but note that the 6-player MIVAT result in poker was only ~18–20% s.d. — multiplayer feature engineering is hard.
- **Composition:** duplicate blocking and the root control variate attack the *same* variance component, so they do **not** multiply. Blocking is strictly stronger (it removes the deal component nonparametrically). Use the control variate as the cheaper alternative when you cannot afford 6× games, or use it *on top of* a 2-rotation block to soak up residual imbalance.

### 4.3 AIVAT proper — **DO THIS THIRD. High payoff, real engineering cost.**
The second half of AIVAT — correcting for the *known-strategy players' actions* and averaging over imaginary private information — is where the 68.8%–99.9% numbers come from, and Fish is unusually well suited:
- **The `U(h)` set is exactly the "range."** Because all transfers are public, the set of deals consistent with the public transcript is a constraint-satisfaction object: player `p` asked card `c` of half-suit `s` ⟹ `p` held ≥1 card of `s` and not `c`; the answer reveals whether the target held `c`. Maintaining `π(d)` over consistent deals is exactly the belief-tracking module the bot needs anyway. **The evaluation infrastructure and the agent's inference engine are the same code.**
- **Every known-strategy decision node contributes a correction term.** In offline bot-vs-bot benchmarking you know *all six* policies — the "full knowledge" case, which in Leduc gave 99.9% s.d. reduction.
- **Declarations are the "game-ending action" analogue.** A declaration removes 6 cards and scores a set — it is the Fish equivalent of a fold, and the "game-ending actions"/Early-Folds imaginary-observation construction applies directly: sum over the prefixes at which your team *could* have declared, weighted by `π^σ_i(z')/π^{\hatσ}_i(S_{-i}(z'))`.
- **Compute cost.** Each correction term requires summing `v'(h'a')` over `h' ∈ U(h)` and `a' ∈ A(U(h))`. `|U(h)|` (the number of hands the acting player could hold consistent with the public record) is large early and collapses fast as cards move. Practical approach: represent the belief as a factorised/sampled particle set over deals (`10^3`–`10^4` particles), and compute the corrections against the particle set. Expect **10–100× the cost of a plain game per evaluated game**, which is still a win if it buys `>10×` variance reduction.
- **Value function `v'`:** the natural choice is your agent's own value head (as DeepStack did) — expected final set differential given the public state and both ranges. **Freeze it before evaluation (see §5).**
- **Pitfall specific to Fish:** the "all cards" imaginary-observation trick relies on the opponents' strategies not depending on your private information. That holds. But it also relies on `π^{\hatσ}_i(z) > 0` everywhere for unbiasedness; a deterministic (argmax) policy violates this and reintroduces bias exactly as in Table 1 of Bowling et al. 2008 (basic IS showed statistically significant bias precisely because S2298 never played some lines). **Use `ε`-smoothed policies in evaluation, or accept and measure the bias.**

### 4.4 Local Best Response — **DO THIS. The only absolute yardstick you will get.**
Fish makes LBR *easier* than poker in several ways:
- **Action set is small.** A legal ask is (target opponent ∈ 3 opponents on the other team) × (a card in a half-suit you hold, that you don't hold). Typically tens of options, not the thousands of no-limit bet sizes. There is no "56 bets" combinatorial blow-up.
- **The range is exactly computable in principle**, and the opponent's policy is queryable.
- **A natural rollout heuristic exists:** `WpRollout` becomes "expected number of remaining sets we win", estimable by (i) counting half-suits where our team already holds a majority, (ii) a learned value head, or (iii) a fast greedy playout.
- **The declaration decision is the natural "game-ending action"** in the LBR sense.

**Concretely: an LBR-team.** Let the three LBR-controlled seats share a joint belief `π` over deals (this is *legitimate for a lower bound* — a correlated team best response lower-bounds the TMECor exploitability). At each LBR turn:
```
LBR_Fish(π, public_state, my_hand):
  # 1. exact/particle posterior over deals consistent with transcript
  # 2. evaluate every legal declaration D:  U(D) = P(D correct | π)·(+1) + (1-P)·(-1)  [+ continuation]
  # 3. evaluate every legal ask (target t, card c):
  #      p_hit = Σ_d π(d)·1[t holds c in d]
  #      U(ask) = p_hit·[Val(state after transfer, keep turn)] + (1-p_hit)·[Val(state after miss, lose turn)]
  #      where Val(·) is a shallow rollout / learned head
  # 4. return argmax over declarations and asks
```
**Cost.** With `|A|` legal actions and `M` belief particles, one LBR decision costs `O(|A|·M·\text{rollout})`. With `|A|≈30`, `M≈2000`, and a 1µs rollout, that's ~60 ms/decision, ~5 s/game — perfectly feasible for 50,000 games on a cluster.

**Pitfalls (all observed in poker, all likely to recur):**
- **Greedy myopia will make LBR under-exploit.** In poker, forcing LBR to check/call in early rounds increased the measured exploitability ~10×. The Fish analogue: LBR that declares too eagerly, or that asks the "greedy information-maximal" card too early, burning its information advantage. **Run an ablation over *when* LBR is allowed to act freely** (e.g. force it to play a fixed simple policy until half-suit 5 is resolved) exactly as Lisý & Bowling swept "rounds 1–4" vs "rounds 3–4".
- **A negative LBR result proves nothing.** If your LBR-team loses to the agent, do not report "the agent is unexploitable."
- **Sweep action subsets** (declarations-only, asks-only, both) — the poker result was that most exploitation came from one extra action type.

### 4.5 Rating systems
- **For a fixed final tournament:** don't use Elo. The population will be full of near-clones (training checkpoints, seeds), which is precisely the redundancy that inflates Elo and that maxent-Nash averaging is invariant to. Build the antisymmetric logit matrix `A` over *team policies*, compute `p^*_A` and `n_A = A p^*_A`.
- **For training-time curves:** WHR is the right tool — it handles time-varying strength exactly, is `O(1)` incremental per game via tridiagonal Newton, and gave the best prediction rate in Coulom's Go experiment.
- **For per-seat / heterogeneous teams:** TrueSkill's team model (`t_j = \sum_i p_i`) is the natural fit if you want *individual* agent ratings from *team* outcomes — exactly the 3-vs-3 structure of Fish. But TrueSkill inherits Elo's transitivity assumption.
- **For a 6-player, 2-team meta-game with heterogeneous seats:** α-Rank is the only listed method that is defined for `K`-population asymmetric general-sum games in polynomial time. Set `K = 2` (team A policy-pool, team B policy-pool) if teams are homogeneous, or `K = 6` if you want per-seat specialisation. Use `ResponseGraphUCB` to allocate games adaptively rather than filling the whole `|S_1|×…×|S_K|` table uniformly — with Clopper-Pearson bounds if your outcome is Bernoulli (team A wins).
- **Cost warning:** α-Rank's profile space is `\prod_k|S_k|`; with 6 populations of 10 policies that's `10^6` profiles, and Theorem 3.1's `N_s` bound is astronomically loose. In practice use the infinite-`α` regime + ResponseGraphUCB (Theorem 3.2: `N_s > 8Δ^{-2}M_{\max}^2\log(2|S|K/δ)`), and exploit Fish's symmetries (permutation invariance within a team) to collapse the profile space by `3! × 3! = 36`.

### 4.6 Sequential testing for the dev loop — **DO THIS. Immediately actionable.**
The chess-engine setup transfers almost verbatim, with the **duplicate block replacing the game pair**:
- Score each 6-rotation block as `Y ∈ [0,1]` (e.g. `(\text{sets won by A}) / (\text{total sets})` averaged over the block, or a normalized set-differential).
- Compute `σ_{pg}` from the block outcome distribution, then `t_n = (μ - 1/2)/σ_{pg}` and `e_n = 347.43\, t_n`.
- Run a GSPRT with `H_0: e_n = e_{n,0}` vs `H_1: e_n = e_{n,1}`, LLR bounds `\log\frac{β}{1-α}` and `\log\frac{1-β}{α}` (`≈ ∓2.94` at 5%/5%).
- Back-of-envelope planning: `T ≈ 10^6/(e_{n,1}-e_{n,0})^2` blocks. Detecting a 5-normalized-Elo improvement needs ~42,000 blocks; 3 nElo needs ~116,000. **Budget accordingly, and prefer "non-regression" bounds (`e_{n,0}+e_{n,1} < 0`) for refactors.**
- **Or**, if you're layering AIVAT/control variates on top, use anytime-valid confidence sequences (AsympCS) instead — this is exactly what AV-AIVAT does, and it gives you a legitimate "stop when the CI is tight enough" rule with a documented 74× median saving.

### 4.7 Cross-play — **DO THIS. Fish is a convention game; self-play score will lie to you.**
Fish has no explicit communication channel but is *saturated* with implicit signalling: every ask reveals that you hold that half-suit, and asks to teammates vs. opponents carry different meanings. A self-play-trained team will invent a private convention (e.g. "asking 9♠ from a teammate means I have exactly the A♠"). **Self-play win rate against a fixed opponent measures both skill and convention-sharing, and cannot separate them.**

Recommended protocol:
1. **Self-play (SP)**: seeds `i` vs seeds `i` — upper bound, reported but never headline.
2. **Inter-seed cross-play (Intra-XP)**: team A drawn from seeds `{i,j,k}` with `i≠j≠k`, likewise team B. The SP−XP gap is your *convention-overfitting* metric.
3. **Inter-algorithm cross-play**: mix your bot with rule-based/heuristic/older-checkpoint teammates from a held-out pool.
4. **Held-out validation opponents** (AlphaStar style): a frozen adversary pool never used for training; report the fraction of validation teams beaten by a decisive margin.

Wolski et al. (2026) give some reassurance that inter-seed cross-play tracks cross-implementation robustness — but on one environment and one algorithm, so treat it as a working assumption, not a theorem.

### 4.8 What does *not* transfer well
- **Exact best-response / exact exploitability.** `10^{40}` states plus a 3-player team best response (APX-hard even in the TMECor formulation) puts exact NashConv out of reach. Don't plan around it. Use LBR lower bounds plus a *small-Fish* testbed (e.g. 4 players / 2 teams, 3 half-suits of 4 cards, 6 cards each) where exact BR *is* computable — this is the Leduc-hold'em role, and every technique above should be validated there first.
- **Nash averaging in its published form** assumes a *two-player zero-sum* meta-game. Fish's meta-game *is* two-player zero-sum if you treat "team policy" as the meta-strategy — so it applies cleanly at the team level, but not if you want per-seat ratings. For per-seat, use α-Rank.
- **Off-policy importance sampling across very different policies.** In poker, basic IS off-policy blew up to `σ = 244,469` mbb/h. In Fish, importance weights `π^σ_i(z)/π^{\hatσ}_i(z)` over 60–120 decisions will have astronomically heavy tails. Only the *All-Cards*-style (private-information) estimator made off-policy tractable in poker; expect the same, and cap/clip weights if you use them at all.

---

## 5. Pitfalls, negative results, and failure modes

1. **Fitting the variance-reduction heuristic on the evaluation data invalidates everything.** Kim & Sandholm's demonstration is the single most important negative result in this literature: with enough degrees of freedom in `v'`, you can produce any conclusion at any p-value from the *same unbiased estimator*. **Freeze `v'` before you see the eval data, and record its provenance.** AV-AIVAT's "predictable value function" (`F_{t-1}`-measurable) is the formal version of this discipline.
2. **Unbiasedness ≠ correctness of the conclusion.** AIVAT is unbiased for *any* `v'`, including adversarial ones. Unbiasedness constrains the expectation, not the realised estimate on your particular dataset.
3. **Continuous monitoring of a fixed-sample CI.** 61.35% false positives under the null. If you look at the running average and stop when it looks good, you have no test.
4. **Loose almost-sure bounds gut empirical-Bernstein confidence sequences.** EB-CS gave 1.365× in HUNL vs 74× for AsympCS, purely because `B` was loose. Derive a *structural* bound from the game tree (Li et al. got `B_Y = 117` analytically for Leduc). In Fish, `|u| ≤ 9` sets, so `B_X = 9` and `B_Y = 9 + 2KV` where `K` is the number of correction nodes and `V = \|v'\|_∞` — keep `V` tight (clip the value head to `[-9,9]`).
5. **Asymptotic CSs are asymptotic.** Li et al. measured 7.1–10.4% finite-horizon false-positive rates for AsympCS. Recheck with EB-CS at the selected stopping index.
6. **Percentile bootstrap over-rejects.** 5.7–11.2% failure at nominal 5% (Jordan et al.). Prefer BCa, or `t`-intervals with a Bonferroni/Holm correction, and *cluster at the deal level*.
7. **Head-to-head and exploitability disagree.** Act1 < Slumbot head-to-head but Act1 < Slumbot in exploitability too. A bot that beats your current opponent pool may be *more* exploitable.
8. **Refining the model does not monotonically improve the strategy** (Waugh et al. 2009): none of strong/weak, player/opponent monotonicity holds in general. The one safe case (Theorem 3) requires the opponent to play the *unabstracted* game, which is infeasible.
9. **LBR is greedy and under-exploits.** Acting too early cost it up to ~10× measured exploitability in poker. Sweep the "when is LBR allowed to act" axis.
10. **LBR can return an uninformative negative bound** (−536 mbb/g vs a bot whose true exploitability was 90 mbb/g). Never interpret a non-positive LBR score as evidence of near-equilibrium.
11. **MIVAT in multiplayer poker gave only ~18–20% s.d. reduction** because the feature set omitted position and pot equity. Expect to spend real effort on Fish deal/state features; a bad control variate is nearly worthless (and, if `\operatorname{Corr}` is low but `c` is mis-set, can *increase* variance — always use `c^* = \operatorname{Cov}/\operatorname{Var}` or the OLS solution).
12. **Elo is meaningless in cyclic games** and can be inflated by instantiating many copies of an agent it beats. Rock-paper-scissors receives identical Elo for all three, predicting `\hat p_{ij}=1/2` where the truth is 1.
13. **Uniform averaging is not redundancy-invariant** — the `div(A') = (−1.15, 1.15, 0, 0)` example falsely crowns agent B.
14. **Spurious equilibria** appear in empirical games with few samples per profile and only disappear slowly (EGTA survey, Fig. 11).
15. **Partial information breaks the unbiasedness proofs.** The imaginary-observation estimators lose their guarantee when the terminal history isn't fully observed. In Fish this is mild — the transcript is public — but a player who drops out with cards unrevealed, or a declaration that ends a half-suit without revealing everything, creates exactly this situation. Measure the empirical bias (Bowling et al. showed All-Cards had no significant bias while DIVAT variants had 56–282 mbb/h).
16. **Deterministic policies break importance sampling.** `π^{\hatσ}_i(z) = 0` for un-played lines ⇒ bias. Bowling et al. observed exactly this (S2298 never plays some outcomes ⇒ statistically significant bias in basic IS).
17. **Population dependence / opponent-pool overfitting.** Fixing an opponent pool and optimising against it is a fixed-opponent best-response, not a robustness measurement. AlphaStar's answer: held-out validation agents + Nash-of-the-league; the meta-game answer: Nash averaging / α-Rank with adaptive sampling.
18. **Non-transitivity is a *feature* of real games, not a measurement artefact** (Czarnecki et al. 2020): the non-transitive dimension is widest at intermediate skill, which is exactly where your bot will spend most of its development. Expect cycles between mid-strength Fish bots and design the evaluation to detect them (mElo₂ residual `\|A - \hat A\|_F`, or the sink components of α-Rank).
19. **Convention overfitting in team play.** Self-play score is not a competence measure in a game with implicit signalling. The SP−XP gap is the metric that matters.
20. **Randomness leaks across duplicate rotations.** If a single global RNG stream feeds all six seats, replaying the same deal in a different rotation will not reproduce the counterfactual, silently destroying the pairing. Use per-seat, per-deal, per-rotation streams derived from a hash.

---

## 6. Recommended evaluation stack for the Fish bot (concrete)

**Tier 0 — harness invariants (free).** Per-seat deterministic RNG streams keyed by `hash(deal_id, rotation, seat)`. Assert `E[Y] = 0` for self-vs-self over rotation blocks. Assert set totals sum to 9.

**Tier 1 — every commit (minutes).** 6-rotation duplicate blocks + frozen root control variate + GSPRT on normalized-Elo bounds. Non-regression bounds for refactors; gainer bounds for changes. Budget from `T ≈ 10^6/(Δe_n)^2`.

**Tier 2 — every release (hours).** Full AIVAT with a frozen value head, inverse-variance weighting (GP/Bayesian-ridge heteroscedastic head), and an asymptotic confidence sequence with EB-CS recheck at the stopping index. Report the estimate, the CS, the stopping index, and the value-function hash.

**Tier 3 — absolute quality (cluster job).** LBR-team lower bound on team exploitability, swept over (i) which decisions LBR is allowed to make, (ii) declaration-only / ask-only / both, (iii) belief-particle count. Report the **maximum** over sweeps (it's a lower bound). Validate the whole LBR implementation on a small-Fish variant where exact team best response is computable.

**Tier 4 — population (periodic).** Team-policy meta-game: antisymmetric logit matrix over `\{`checkpoints, seeds, baselines, rule-based bots`\}`; maxent-Nash averaging for the headline ranking, mElo₂ residual for cycle detection, WHR for the training-time curve, ResponseGraphUCB for adaptive game allocation. Report SP, Intra-XP, and inter-algorithm XP separately, with Bonferroni/Holm-corrected simultaneous intervals over all reported comparisons.

---

## 7. Bibliography

**Variance reduction / estimators**

1. Zinkevich, M., Bowling, M., Bard, N., Kan, M., & Billings, D. (2006). *Optimal Unbiased Estimators for Evaluating Agent Performance.* AAAI-06, 573–578. (The advantage-sum / DIVAT foundation; **read only through the restatements in White & Bowling 2009 and Bowling et al. 2008 — primary text UNVERIFIED**.)
2. Bowling, M., Johanson, M., Burch, N., & Szafron, D. (2008). *Strategy Evaluation in Extensive Games with Importance Sampling.* ICML 2008. https://poker.cs.ualberta.ca/publications/ICML08.pdf — also https://dl.acm.org/doi/abs/10.1145/1390156.1390166 (verified, full text)
3. White, M., & Bowling, M. (2009). *Learning a Value Analysis Tool for Agent Evaluation* (MIVAT). IJCAI-09, 1976–1981. https://www.ijcai.org/Proceedings/09/Papers/326.pdf (verified, full text)
4. Davidson, J., Archibald, C., & Bowling, M. (2013). *Baseline: Practical Control Variates for Agent Evaluation in Zero-Sum Domains.* AAMAS 2013. https://poker.cs.ualberta.ca/publications/AAMAS13-baseline.pdf (**located and downloaded; contents not read in detail — UNVERIFIED**)
5. Burch, N., Schmid, M., Moravčík, M., Morrill, D., & Bowling, M. (2018). *AIVAT: A New Variance Reduction Technique for Agent Evaluation in Imperfect Information Games.* AAAI-18. arXiv:1612.06915. https://arxiv.org/abs/1612.06915 · https://ar5iv.labs.arxiv.org/html/1612.06915 · https://cdn.aaai.org/ojs/11481/11481-13-15009-1-2-20201228.pdf (verified: equations + all four result tables)
6. Kim, J., & Sandholm, T. (2026). *Heuristic Pathologies and Further Variance Reduction via Uncertainty Propagation in the AIVAT Family of Techniques.* arXiv:2605.14261 (14 May 2026). https://arxiv.org/abs/2605.14261 (verified, full text)
7. Li, B., Chen, Y., & Huang, L. (2026). *AV-AIVAT: 74× Cheaper Agent Evaluation with Certified Anytime-Valid Stopping in Imperfect-Information Games.* arXiv:2608.06362 (7 Aug 2026), IIIS Tsinghua. https://arxiv.org/html/2608.06362 (verified via HTML; equations transcribed from the rendered page)

**Exploitability / best response**

8. Johanson, M., Waugh, K., Bowling, M., & Zinkevich, M. (2011). *Accelerating Best Response Calculation in Large Extensive Games.* IJCAI-11, 258–265. https://www.ijcai.org/Proceedings/11/Papers/054.pdf (abstract-level; **full text UNVERIFIED**)
9. Lisý, V., & Bowling, M. (2017). *Equilibrium Approximation Quality of Current No-Limit Poker Bots* (Local Best Response). AAAI-17 Workshop on Computer Poker and Imperfect Information Games; arXiv:1612.07547. https://arxiv.org/abs/1612.07547 · https://poker.cs.ualberta.ca/publications/aaai17ws-lisy-lbr.pdf (verified: pseudocode + all result tables)
10. Waugh, K., Schnizlein, D., Bowling, M., & Szafron, D. (2009). *Abstraction Pathologies in Extensive Games.* AAMAS 2009, 781–788. https://bowlingmh.github.io/papers/09aamas-abstraction.pdf (verified: definitions, Theorem 3, conclusions)
11. Lanctot, M., Zambaldi, V., Gruslys, A., Lazaridou, A., Tuyls, K., Pérolat, J., Silver, D., & Graepel, T. (2017). *A Unified Game-Theoretic Approach to Multiagent Reinforcement Learning* (PSRO, NashConv). NeurIPS 2017. https://proceedings.neurips.cc/paper/2017/hash/3323fe11e9595c09af38fe67567a9394-Abstract.html (search-level only — **UNVERIFIED**)
12. Zhang, B. H., Farina, G., Celli, A., & Sandholm, T. (2022). *Subgame Solving in Adversarial Team Games.* NeurIPS 2022. https://www.mit.edu/~gfarina/2022/subgame_solving_teams_neurips22/subgame_solving_teams_neurips22.pdf (search-level only — **UNVERIFIED**)
13. Zhang, B. H., & Sandholm, T. et al. (2022). *A Marriage between Adversarial Team Games and 2-player Games: Enabling Abstractions, No-regret Learning, and Subgame Solving.* arXiv:2206.09161. https://arxiv.org/pdf/2206.09161 (search-level only — **UNVERIFIED**)

**Landmark agent evaluations**

14. Moravčík, M., Schmid, M., Burch, N., Lisý, V., Morrill, D., Bard, N., Davis, T., Waugh, K., Johanson, M., & Bowling, M. (2017). *DeepStack: Expert-Level Artificial Intelligence in Heads-Up No-Limit Poker.* Science 356(6337). https://poker.cs.ualberta.ca/publications/17science.pdf (verified: "Evaluating DeepStack" section + LBR table)
15. Brown, N., & Sandholm, T. (2018). *Superhuman AI for heads-up no-limit poker: Libratus beats top professionals.* Science 359(6374). https://www.science.org/doi/10.1126/science.aao1733 (secondary sources only: 120,000 hands, 147 mbb/game, p = 0.0002, mirrored hands — **UNVERIFIED primary**)
16. Brown, N., & Sandholm, T. (2019). *Superhuman AI for multiplayer poker* (Pluribus). Science 365(6456). https://www.science.org/doi/10.1126/science.aay2400 (primary blocked by paywall; numbers cross-checked against a secondary summary at https://wiki.math.uwaterloo.ca/statwiki/index.php?title=Superhuman_AI_for_Multiplayer_Poker and against Kim & Sandholm 2026 Table 3 — **secondary/cross-verified**)
17. Vinyals, O., et al. (2019). *Grandmaster level in StarCraft II using multi-agent reinforcement learning.* Nature 575. https://storage.googleapis.com/deepmind-media/research/alphastar/AlphaStar_unformatted.pdf (search + figure-caption level — **UNVERIFIED**)
18. Bard, N., Foerster, J. N., Chandar, S., Burch, N., Lanctot, M., Song, H. F., Parisotto, E., Dumoulin, V., Moitra, S., Hughes, E., Dunning, I., Mourad, S., Larochelle, H., Bellemare, M. G., & Bowling, M. (2020). *The Hanabi Challenge: A New Frontier for AI Research.* Artificial Intelligence 280. (direct fetch failed; search-level only — **UNVERIFIED**)

**Rating systems**

19. Coulom, R. (2008). *Whole-History Rating: A Bayesian Rating System for Players of Time-Varying Strength.* Computers and Games 2008, Beijing. https://www.remi-coulom.fr/WHR/WHR.pdf · https://inria.hal.science/inria-00323349v1 (verified: model, Newton update, Table 1)
20. Herbrich, R., Minka, T., & Graepel, T. (2007). *TrueSkill™: A Bayesian Skill Rating System.* NeurIPS 19 (2006). https://proceedings.neurips.cc/paper_files/paper/2006/file/f44ee263952e65b3610b8ba51229d1f9-Paper.pdf (extraction garbled; **the `v`/`w` formulas above are the standard published forms and should be re-verified**)
21. Glickman, M. E. *The Glicko / Glicko-2 rating system.* http://www.glicko.net/glicko.html (search-level — **UNVERIFIED**)
22. Van den Bergh, M. *Comments on Normalized Elo.* Fishtest technical note. https://cantate.be/Fishtest/normalized_elo_practical.pdf (verified: normalized Elo, `T = D/(Δe_n)²`, `D = 1,046,535`, GSPRT MLE procedure)
23. Stockfish contributors. *Statistical Methods and Algorithms in Fishtest.* https://official-stockfish.github.io/docs/fishtest-wiki/Fishtest-Mathematics.html (verified conceptually: GSPRT, pentanomial model, normalized-Elo bounds)
24. Chess Programming Wiki. *Sequential Probability Ratio Test.* https://chessprogramming.org/Sequential_Probability_Ratio_Test (search-level; LLR bounds ≈ ±2.94 at α=β=0.05)
25. Dehpanah, A., Ghori, M. F., Gemmell, J., & Mobasher, B. (2021). *The Evaluation of Rating Systems in Team-based Battle Royale Games.* arXiv:2105.14069. https://arxiv.org/pdf/2105.14069 (verified authors/abstract/metric list: accuracy, MAE, Kendall's τ, NDCG over Elo/Glicko/TrueSkill)

**Population / meta-game evaluation**

26. Balduzzi, D., Tuyls, K., Pérolat, J., & Graepel, T. (2018). *Re-evaluating Evaluation.* NeurIPS 2018; arXiv:1806.02643. https://arxiv.org/pdf/1806.02643 · https://proceedings.neurips.cc/paper/2018/hash/cdf1035c34ec380218a8cc9a43d438f9-Abstract.html (verified: Props 1–4, Def 1–2, Theorem 1 statement, mElo₂, invariance example)
27. Omidshafiei, S., Papadimitriou, C., Piliouras, G., Tuyls, K., Rowland, M., Lespiau, J.-B., Czarnecki, W. M., Lanctot, M., Pérolat, J., & Munos, R. (2019). *α-Rank: Multi-Agent Evaluation by Evolution.* Scientific Reports 9, 9937; arXiv:1903.01373. https://www.nature.com/articles/s41598-019-45619-9 · https://arxiv.org/pdf/1903.01373 (verified: Eqs. 4–10, Theorem 2.1.2)
28. Rowland, M., Omidshafiei, S., Tuyls, K., Pérolat, J., Valko, M., Piliouras, G., & Munos, R. (2019). *Multiagent Evaluation under Incomplete Information.* NeurIPS 2019; arXiv:1909.09849. https://arxiv.org/pdf/1909.09849 (verified: Theorems 3.1, 3.2, ResponseGraphUCB Algorithm 1)
29. Czarnecki, W. M., Gidel, G., Tracey, B., Tuyls, K., Omidshafiei, S., Balduzzi, D., & Jaderberg, M. (2020). *Real World Games Look Like Spinning Tops.* NeurIPS 2020; arXiv:2004.09468. https://proceedings.neurips.cc/paper_files/paper/2020/file/ca172e964907a97d5ebd876bfdd4adbd-Paper.pdf (abstract-level — **partially verified**)
30. Lanctot, M., Larson, K., Bachrach, Y., Marris, L., Li, Z., Bhoopchand, A., Anthony, T., Tanner, B., & Koop, A. (2023). *Evaluating Agents using Social Choice Theory.* arXiv:2312.03121. https://arxiv.org/abs/2312.03121 (abstract-level — **partially verified**)
31. Lanctot, M., et al. (2025). *Soft Condorcet Optimization for Ranking of General Agents.* AAMAS 2025, 1253–…; arXiv:2411.00119. https://www.ifaamas.org/Proceedings/aamas2025/pdfs/p1253.pdf (located only — **UNVERIFIED**)
32. Wellman, M. P., Tuyls, K., & Greenwald, A. (2025). *Empirical Game Theoretic Analysis: A Survey.* JAIR 82, 1017–1076 (submitted 03/2024, published 02/2025); arXiv:2403.04018. https://arxiv.org/pdf/2403.04018 (verified: §6.1–6.4, Hoeffding/Bennett sample bounds, bootstrap regret, spurious equilibria)

**Statistics of evaluation**

33. Jordan, S. M., Chandak, Y., Cohen, D., Zhang, M., & Thomas, P. S. (2020). *Evaluating the Performance of Reinforcement Learning Algorithms.* ICML 2020, PMLR 119:4962–4973. https://proceedings.mlr.press/v119/jordan20a.html · https://all.cs.umass.edu/pubs/2020/Jordan%20et%20al%20-%20Evaluating%20the%20Performance%20of%20Reinforcement%20Learning%20Algorithms.pdf (verified: PBP/PBP-t/bootstrap, Table 2 failure rates, Bonferroni `δ' = δ/(|A||M|)`)
34. Agarwal, R., Schwarzer, M., Castro, P. S., Courville, A., & Bellemare, M. G. (2021). *Deep Reinforcement Learning at the Edge of the Statistical Precipice.* NeurIPS 2021. https://proceedings.neurips.cc/paper/2021/hash/f514cec81cb148559cf475e7426eed5e-Abstract.html (search-level: interval estimates, stratified bootstrap, interquartile mean, performance profiles — **UNVERIFIED**)
35. Wald, A. (1945). *Sequential Tests of Statistical Hypotheses.* Annals of Mathematical Statistics 16(2), 117–186. (standard reference for the SPRT and Wald bounds — **URL/primary text UNVERIFIED**)
36. Efron, B., & Tibshirani, R. J. (1993). *An Introduction to the Bootstrap.* Chapman & Hall. (standard — **UNVERIFIED**)
37. Davison, A. C., & Hinkley, D. V. (1997). *Bootstrap Methods and their Application.* Cambridge University Press. (cited by the EGTA survey as the general bootstrap reference — **primary UNVERIFIED**)
38. Wiedenbeck, B., et al. (2014). Bootstrap approach to statistical questions about empirical games. (as described in Wellman, Tuyls & Greenwald 2025 §6.4 — **primary UNVERIFIED**)
39. Areyan Viqueira, E., et al. (2020); Cousins, C., et al. (2022); Tuyls, K., et al. (2020). Uniform-approximation and empirical-Bennett bounds for empirical games. (as described in the EGTA survey §6.3 — **primary UNVERIFIED**)

**Cooperative / team evaluation**

40. Hu, H., Lerer, A., Peysakhovich, A., & Foerster, J. (2020). *"Other-Play" for Zero-Shot Coordination.* ICML 2020; arXiv:2003.02979. https://arxiv.org/abs/2003.02979 (search-level — **UNVERIFIED**)
41. Wolski, M., Hoernle, N., Forkel, J., & Foerster, J. (2026). *Is Inter-Seed Cross-Play Enough? Evaluating the Robustness of Zero-Shot Coordination Algorithms to Implementation Details.* arXiv:2608.03644 (4 Aug 2026). https://arxiv.org/html/2608.03644v1 (verified via HTML: 176 policies / 22 implementations / 8 seeds, XIXP vs WIXP result)

**Competition / duplicate designs**

42. Annual Computer Poker Competition, *Rules.* http://www.computerpokercompetition.org/index.php/competitions/rules?id=80 (direct fetch blocked by a self-signed TLS certificate; content quoted from search-engine snippets — **partially verified**: duplicate matches, common seeds, bootstrapping; duplicate s.d. ≈ 2/3 of plain s.d.)
43. Bard, N., Hawkin, J., Rubin, J., & Zinkevich, M. (2013). *The Annual Computer Poker Competition.* AI Magazine 34(2). (located only — **UNVERIFIED**)
44. *International Match Points.* Wikipedia. https://en.wikipedia.org/wiki/International_Match_Points (verified conceptually: IMP conversion scale 0–24, damping of large scores; the `15√N` VP-scale figure is **UNVERIFIED**)
45. *Duplicate poker.* Wikipedia. https://en.wikipedia.org/wiki/Duplicate_poker (located only — **UNVERIFIED**)

---

*Report compiled 2026-08-21. Every equation reproduced above was read from the source indicated as "verified"; items marked UNVERIFIED were located but not read in full and must be checked before being relied upon.*
