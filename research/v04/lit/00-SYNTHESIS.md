# FishBot v0.4 — Literature Synthesis and Build Specification

**Synthesis of eight literature reviews** (`fish-prior-art`, `belief-inference`, `pimc`, `ismcts`, `cfr-team`, `signalling`, `evaluation`, `engineering`; ~85,000 words, ~230 distinct sources).
**Target:** the strongest possible agent for 6-player Canadian Fish (Literature) — 54 cards, 9 half-suits of 6, 9 cards each, two teams of three on alternating seats, all asks/answers/transfers/declarations/hand-counts public, declare at any time, any error forfeits the set.
**Platform:** C++20 / clang, one ~15-core Apple Silicon machine, no GPU, large-but-finite time budget.
**Date:** 2026-08-21.

---

## 0. The five facts that determine every design decision

Everything below follows from these. They are stated first because most of the literature's advice is *conditional* on structural properties that Fish either has or lacks, and getting these wrong is how the project fails.

**F1 — The only hidden variable is the initial deal.** Cards move only by a publicly observed transfer. Therefore a card whose location has never been revealed is still with whoever was dealt it, hand sizes are public, and every player computes the *identical* posterior from the *identical* public history. Fish is an **exact public-belief-state (PBS) game** in the Nayyar/Foerster sense — not approximately, exactly. No other card game in the literature has this property this cleanly. (`belief-inference` §5.6, `signalling` §2.6, `cfr-team` §6.2.)

**F2 — The belief is exactly computable in milliseconds.** The hidden state is a 54-card partition into six hands of exactly 9. That is a permanent of a matrix with only 6 *distinct column types*, hence polynomial. A DP over the capacity vector costs `O(k·∏_q(c_q+1))` = 5×10⁵ multiply-adds from a player's seat. Exact normaliser, exact per-card marginals, exact `P[named allocation]`, and rejection-free i.i.d. deal sampling all fall out. Verified against brute force to 2.2×10⁻¹⁶. (`belief-inference` §3.2–3.5, §8.2.)

**F3 — The *perfect-information* game is degenerate, so PIMC collapses to a one-ply heuristic.** A clairvoyant player never fails an ask and never mis-declares; the double-dummy value of nearly every position is "the team on turn sweeps what it can reach." Averaging that over sampled worlds gives `argmax_(j,c) Pr(j holds c)`. A Fish DDS will be easy to build, fast, and worthless. **This is the single most important negative result in the entire corpus.** (`pimc` §4 F3–F5.)

**F4 — Declaration is the whole game, and no prior agent models it.** It is four decisions — *when*, *which half-suit*, *which of 3⁶=729 allocations*, *by whom* — under a rule where any error hands the set to the opponents. It is also where every search method's strategy fusion is fatal, because a determinization *tells the searcher the answer*. (`fish-prior-art` §6.6, `ismcts` §4.3, `pimc` §5.10.)

**F5 — Every signal is broadcast to three adversaries who can execute on it immediately and deterministically.** A publicly known card location is a *guaranteed* take plus turn retention for any opponent holding that half-suit. Contrast Hanabi (no adversary) and bridge (diffuse probabilistic harm). Formally: absolute-codebook conventions have **non-positive Wyner secrecy rate** in Fish, because teammate and opponent hands are exchangeable given public history. (`signalling` §2.4, §2.6.)

And one fact about the field: **there is no prior art.** arXiv returns zero for "Canadian Fish", "Literature card game", "half-suit". The only real agent (`neelsomani/literature`) has a verified reward bug (`0 == Team.EVEN` is `False`; every move in every game received terminal reward −100), plays the 4-player 48-card variant, and hard-codes greedy declaration. It is a cautionary test case, not a baseline. Any competent agent here is state of the art by default — which raises the bar on *evidence*, not on cleverness. (`fish-prior-art` §1–2.)

---

## 1. Recommended architecture

### 1.1 The one-sentence version

> **An exact public-belief-state core driving a single information-set tree over public action histories, expanded by Gumbel/Sequential-Halving, evaluated by an incrementally-updated NNUE-style network trained by AlphaZero-style self-play on set-differential targets, with re-determinized non-root seats, declarations decided as exact Bayesian optimal stopping rather than searched, and an exactly-solved ≤2-half-suit endgame.**

Call it **XB-GIS**: eXact Belief + Gumbel Information-set Search. Five modules, built in this order.

### 1.2 Module 1 — Exact belief engine (the foundation; see §4 for full detail)

Not a particle filter. Not Sinkhorn. Not constraint propagation alone. A **half-suit block-enumeration DP over capacity vectors**, recomputed from scratch every turn.

It exposes exactly five queries, and every other module consumes only these:

```
Z                       information-set mass (log-scale)
mu[p][c]                exact Pr(card c was dealt to p | public history, own hand)
q(H, A)                 exact Pr(allocation A of half-suit H is correct)
sample() -> deal        exact, i.i.d., rejection-free
mu_tilde[p][c], q_tilde grounded twins (hard rules only, no policy likelihood)
```

The grounded twins are not optional decoration — they are what make convention value measurable (§5, §6).

### 1.3 Module 2 — Information-set search

**Build ONE tree over public action histories.** Cowling et al. prove SO-ISMCTS ≡ SO-ISMCTS+POM ≡ MO-ISMCTS when all moves are fully observable. Every Fish action is public. MO-ISMCTS would cost 6 identical trees for identical statistics. (`ismcts` §2.5, §4.2.)

**Node structure and selection.**

At *your own* nodes, ask legality depends only on your own hand, so the legal move set is identical across all determinizations — there is no subset-armed bandit and plain selection applies. At *other seats'* nodes (teammates included — their legality is hidden from you too) the availability-count fix is mandatory:

$$
v^\star \;=\; \arg\max_{v \in c(u,d)}\left[\frac{r(v)_{\text{team}(\rho(d))}}{n(v)} \;+\; c\sqrt{\frac{\ln n'(v)}{n(v)}}\right],\qquad c = 0.7
$$

where `n'(v)` counts iterations in which the action was *available*, not parent visits. Cowling found insensitivity across `c ∈ [0.3, 1.5]`; use 0.7 and stop tuning it. Without this, rare opponent actions are wildly over-explored. (`ismcts` §2.2.)

**Root policy: Gumbel Sequential Halving, not PUCT.** This is the largest single throughput lever available on a CPU. Danihelka et al. give *guaranteed* policy improvement at n = 2 simulations:

$$
A_{\text{top-}m} = \operatorname{argtop}\big(g + \text{logits},\; m\big),\qquad g \sim \text{Gumbel}(0)
$$

Run Sequential Halving over `A_top-m` with `n` simulations, comparing `g(a) + logits(a) + σ(q̂(a))` with

$$
\sigma(\hat q(a)) = \big(c_{\text{visit}} + \max_b N(b)\big)\, c_{\text{scale}}\, \hat q(a),\qquad c_{\text{visit}} = 50,\; c_{\text{scale}} = 1.0
$$

and train on

$$
\pi' = \operatorname{softmax}\big(\text{logits} + \sigma(\text{completedQ})\big),\qquad
\text{completedQ}(a) = \begin{cases} q(a) & N(a) > 0\\ \hat v_\pi & \text{otherwise}\end{cases}
$$

Non-root selection: `argmax_a [π'(a) − N(a)/(1 + Σ_b N(b))]`. At `m=16, n=32` this is ~0.2–0.5 ms/decision versus 2–6 ms for 400-sim PUCT — a 12–25× self-play multiplier at equal or better policy quality. With ~40–140 legal asks, `m=16` is a *complete* search in the sense that matters. (`engineering` §9.3.)

**Determinization per iteration.** Draw i.i.d. from the exact posterior (F2). With probability `β_self ∈ [0.375, 0.875]`, draw instead from the *public-only* belief ψ (what an outside observer, including opponents, would believe) — Cowling's self-determinization. This is what lets the search see the value of asks that *mislead*, and in *The Resistance* it produced bluffing without any explicit bluff rule at large budgets. Any fraction in that range worked; start at 0.5. (`ismcts` §2.7, §4.7.)

**Re-determinization at non-root seats (RIS-MCTS).** At every node where a seat other than the root player acts, save that seat's hand, re-sample it from *that seat's own* information set (in practice: from ψ), choose the action from that world, then restore and repair. **This is the only mechanism in the entire literature that makes signalling value visible to a tree search.** In Hanabi it converted a flat, budget-insensitive 3.87 agent into one improving to 5.21. Without it, the search believes your teammate already knows your hand, so informing them is worth exactly zero. Gate it with probability `β_RIS` and tune — Goodman's own caveat is that re-determinization back-propagates outcomes from worlds inconsistent with the root information set and can cap performance at large budgets. Apply it lazily: only at ask-choice and declare nodes, never at forced answer nodes. (`ismcts` §2.8, §4.4.)

**Leaf evaluation: the network, never a double-dummy solver.** F3 disqualifies the DDS. The IIMC/RecPIMC lesson — replace the perfect-information evaluator with something that reflects actual imperfect-information play — is satisfied here by a value head trained on imperfect-information self-play, at ~0.3–1.0 µs versus ~10 ms for a rule-based playout. Normalise the value to unit range so the standard UCB constants transfer:

$$
\hat\mu \;=\; \frac{(\text{our declared sets} + \sum_H \hat q_H^{\text{ours}}) - (\text{their declared sets} + \sum_H \hat q_H^{\text{theirs}})}{9}\;\in[-1,1]
$$

**Backup: scalar team score.** Fish is 2-player constant-sum at the team level (9 sets always allocated). Skip maxⁿ, paranoid, and MP-Mix machinery entirely. Keep exactly two ideas from that literature: Zuckerman et al. name **Go Fish as `OI = 1`** — you can damage any chosen opponent at essentially every state — so *seat choice is a first-class argmax dimension, never a tie-break*; and the teammate strategy-fusion trap (simulate teammates as belief-limited agents, never as MAX nodes holding your knowledge). (`pimc` §5.7.)

**Root move selection with anti-exploitation.** A deterministic root policy fully inverts: an opponent who knows your policy learns your hand from your ask. Two ~free patches. Smooth-UCT root mixing,

$$
\eta_k = \max\{\gamma,\; \eta(1 + d\sqrt{N_k})^{-1}\},\qquad \gamma=0.1,\ \eta=0.9,\ d\in[5\!\times\!10^{-5},10^{-3}]
$$

(with probability `1−η_k`, sample from the visit distribution instead of taking the argmax — this alone turned diverging UCT into converging UCT in Kuhn and Leduc); and the near-noise set

$$
A^\star = \{a : \mu_{a^\star} - \mu_a < \min(\sigma_{a^\star},\sigma_a)\}
$$

sampled by visit frequency. (`ismcts` §2.10, §4.7.)

**Explicitly rejected inside the search:** RAVE/AMAF (*detrimental* in order-sensitive card games; Fish is intensely order-sensitive), full-length random playouts, MO-ISMCTS, MT-ISMCTS, virtual loss.

### 1.4 Module 3 — The declaration module (decision-theoretic, not searched)

`declare(H)` enters the tree as **one information-set-level edge per half-suit**, never as 729 allocation edges. The allocation is a deterministic function of the declarer's belief (the MAP allocation), and the edge's immediate reward is computed against the *belief*, not the determinization:

$$
A^\star = \arg\max_{A \in \mathcal{T}^H} q(A),\qquad
\mathbb{E}[\text{score} \mid \text{declare}] = 2q(A^\star) - 1
$$

with `q(A)` read exactly off the block tables (§4), **not** as a product of per-card marginals — the six cards of a half-suit are strongly dependent and the product is badly wrong.

**The optimal-stopping rule, and the theorem that no prior agent implements.** The full condition is

$$
\text{declare now} \iff (2q_t - 1) \;>\; \underbrace{\mathbb{E}[2q_{t+\Delta}-1]}_{\text{info improves by waiting}} \;-\; \underbrace{C_{\text{waste}}}_{\text{teammates burn asks}} \;+\; \underbrace{B_{\text{control}}(t)}_{\text{control transfer}} \;-\; \underbrace{C_{\text{steal}}}_{\text{opponents take it}}
$$

and the key structural fact is that **`C_steal = 0` exactly whenever your team provably holds all six cards** — the opponents are void in `H`, so they have no legal base card and can never ask in it. Therefore `q` is a martingale that can only improve, and **waiting weakly dominates declaring.** The folk "stalemate-breaker" is not folklore; it is optimal play under this model. Conversely, Develin's worked endgame reduces exactly to requiring `q' > 3/2` to prefer asking over a coin-flip declare that buys a control transfer — impossible — so **declare a 50/50 set to buy a control transfer whenever the transfer secures another set.** (`fish-prior-art` §6.6, §5.1 D15.)

Calibrate the threshold by self-play and ship a **reliability diagram** (predicted `q` vs realised success) as a first-class KPI. A mis-calibrated declaration threshold biases every evaluation in the search above it.

Also model the two decisions everyone else randomises: (a) which teammate receives the turn when a declaration empties your hand; (b) the forced-declaration endgame, where under the pagat rule your **opponents** nominate which of your players declares everything — so your objective there is a *maximin over your own team*:

$$
\text{maximise}\quad \min_{t \in \mathcal{T},\,n_t>0}\ \prod_{H \in \mathcal{R}} \max_A q_t(A)
$$

which yields a genuinely novel implication no source states: **before the endgame, either equalise information across your team, or deliberately empty your least-informed teammate's hand so they cannot be nominated.**

### 1.5 Module 4 — Network and self-play training

**Architecture: NNUE-style incremental evaluation, not a dense MLP.** A Fish event changes ~4–12 input features. An `int16` accumulator updated by weight-column add/subtract, then `int8` hidden layers using NEON `vdotq_s32`, evaluates in ~0.3–1.0 µs on one P-core (~1–3 M evals/s/core, 15–45 M/s machine-wide). A from-scratch dense pass of the same net costs 20–100× more. Encode feature deltas *in the move-application function*; never diff two feature vectors. (`engineering` §8.)

Features, root-player-relative (~750–1130 binary, ~200–400 active):

| Block | Size |
|---|---|
| `KNOWN_HELD[p][c]` | 6 × 54 |
| `KNOWN_NOT_HELD[p][c]` — the negative-information channel where most of Fish's signal lives | 6 × 54 |
| `HAND_SIZE[p][n]` | 6 × 10 |
| `HS_STATUS[h][s]` (undeclared / ours / theirs) | 9 × 3 |
| `HS_KNOWN_COUNT[p][h][k]` | 6 × 9 × 7 |
| turn-to-move | 6 |
| **belief digest**: quantised `mu[p][c]` and per-half-suit `q` | added as dense inputs |

Feeding the exact marginals in as features is the cheapest possible way to give the network what would otherwise take enormous capacity to learn.

**Heads:** value (set differential in `[−1,1]`, *not* win/loss — it is a far denser signal over a 9-set game and is the exact analogue of KataGo's score target); policy over a **fixed index space** of 3×54 asks + 9 declares + 2 turn-gifts = 173 logits; and two auxiliary heads — **per-card ownership** (the KataGo ownership analogue, and the highest-value auxiliary target in Fish because it directly supervises the belief model) and opponent-next-ask.

> **Resolved conflict between reports.** `cfr-team` recommends DouZero-style Deep Monte-Carlo with *action-as-input*; `engineering` recommends Gumbel AlphaZero with a fixed policy head. Action-as-input exists to cope with DouDizhu's 27,000-move and GuanDan's 5,000-move combinatorial action spaces. Fish's ask space is ≤135 and factorises cleanly as (target, card), so a fixed 173-logit head is well-conditioned. **Take Gumbel AlphaZero.** DMC's `γ=1` Monte-Carlo return is retained anyway — it is what the value target is.

**Self-play loop** (`engineering` §9.2, KataGo's non-domain-specific wins, all measured):

- **Playout-cap randomisation** (1.37×): full search (`n = 600`→1000) with probability 0.25, fast search (`n = 100`→200) otherwise; *only full-search turns produce training rows*.
- **Forced playouts** `n_forced(c) = √(k·P(c)·Σ N(c'))`, `k = 2`, plus **policy-target pruning** before writing the target, so the policy is not trained to imitate its own exploration noise.
- **Auxiliary targets** (1.30–1.65×): ownership head, opponent-next-move head at weight 0.15.
- **Sublinear data window** `N_window = c(1 + β((N_total/c)^α − 1)/α)`, `c = 250,000`, `α = 0.75`, `β = 0.4`.
- Dirichlet root noise `α ≈ 10/|A| ≈ 0.1–0.25`, `ε = 0.25`; temperature 1.0 for ~15 decisions then decaying to 0.1.
- Optimiser: SGD + momentum 0.9, per-sample LR 6e-5, batch 256, SWA — more stable and cheaper than Adam for small nets on CPU.

**Three cooperative-learning imports, all cheap and all necessary:**

1. **SAD (decoupled exploratory/greedy action)** — during centralised training, teammates additionally observe the *greedy* action, not the exploratory one. Free at test time (greedy = executed). This is a **prerequisite** for any convention to survive ε-greedy: single-action exploration blurs the Bayesian posterior and cannot evaluate a convention's holistic effect. (`signalling` §4.3.)
2. **A shared per-episode correlation seed `ω`** given to all three teammates. This *literally implements the TMECor correlation device*, lets the team randomise jointly over convention books (an opponent cannot know which is in force this deal), and costs one integer. The price of uncorrelation is unbounded in game size. (`signalling` §3.5, `cfr-team` §2.3.)
3. **FXP counter-population cross-play** at `η ≈ 0.75` (~30% extra compute). FXP proves self-play with preference-preserving updates converges to *local* team equilibria w.h.p. in mixed cooperative-competitive games — this is the formal diagnosis of "my bot's asks are meaningless." (`cfr-team` §2.6, §6.2(16).)

**Optionally, and with care:** a perfect-information critic (PerfectDou PTIE) annealed out on Suphx's dropout schedule `δ_t → 0`. It cuts critic variance in a game whose reward lands 60+ steps later, but it **systematically undervalues information-gathering asks** — it already knows the answer. Anneal it or do not use it.

### 1.6 Module 5 — Endgame solver

When ≤2 half-suits are live and ≤12 cards unresolved, convert to the **Team-Belief-DAG / TPI coordinator game** (which makes the team a single player issuing *prescriptions* and renders the game 2-player zero-sum, with Nash equilibria realization-equivalent to TMECor of the original) and solve with **DCFR(α=3/2, β=0, γ=2)**:

$$
R^t(I,a) = \begin{cases} R^{t-1}\cdot\frac{(t-1)^\alpha}{(t-1)^\alpha+1} + r^t & R^{t-1} > 0\\ R^{t-1}\cdot\frac{(t-1)^\beta}{(t-1)^\beta+1} + r^t & R^{t-1} \le 0\end{cases},
\qquad S^t = S^{t-1}\Big(\tfrac{t-1}{t}\Big)^{\gamma} + \pi_i^{\sigma^t}(I)\sigma^t(I,a)
$$

**Use DCFR, not CFR+**: CFR+ is documented to underperform in games with catastrophically costly actions, and a wrong declaration is exactly that. Expect sizes comparable to the TB-DAG paper's 3K8 instance (1.8M DAG vertices, ε=10⁻³ in 4.7 s) — entirely feasible on 15 cores. Use the solved values as leaf evaluations for the search above, and as the ground truth for §6's open questions. **Do not attempt the conversion mid-game**: TB-DAG edges scale as `O*(3^k)` in `k`-privateness and Fish's mid-game `k` is astronomically large. (`cfr-team` §2.5, §6.2(2).)

### 1.7 Test-time refinement (build last, if at all)

**Mirror-Descent Search** over the top-`k` asks from the network:

$$
\pi_{\text{search}} \;\propto\; \pi_b \exp(\eta\,\hat q)
$$

with 20–50 exact belief particles and `k ≈ 5–10`. MDS matched SPARTA in Hanabi at ~100× less search time and tolerates 10-particle posteriors; SPARTA itself lifted a Hanabi blueprint 24.08 → 24.61 purely at test time. **Critical Fish constraint:** all three teammates must run the *identical* procedure with an *agreed seed*, and it must be a deterministic function of public information plus own hand. If only one seat searches, teammates' beliefs about it go stale and coordination degrades. Budget ~0.1–0.3 s/decision. (`cfr-team` §6.2(12), `signalling` §7.6.)

### 1.8 Engineering substrate (assume, don't debate)

- **One `uint64_t` per hand**, card index `c = 6h + i`, half-suit mask `0x3F << 6h`. Whole state = 72 bytes. **Copy the state in playouts; do not write make/unmake.** Already the layout in `engine/src/fish.hpp`.
- **Parallelism: one whole game per thread** for self-play and match play (degenerate root parallelism — zero sync, linear scaling, bit-reproducible). Reserve WU-UCT (`argmax {V + β√(2log(N+O)/(N'+O'))}`, `O += 1` on descent, `O -= 1; N += 1` on backup) for the single-decision analysis mode. **Never use virtual loss** — Mirsoleimani et al. showed it degrades lock-free tree parallelism across all `C_p` and 2–64 threads. Chaslot's measured 16-thread strength-speedups: root 14.9×, tree+VL 8.5×, tree+local-mutex 3.3×, leaf 2.4×.
- **Pack `(W,N)` into one `atomic<uint64_t>`** (W high 32 fixed-point, N low 32). Backup is one `fetch_add`; the read-tearing race is impossible by construction; results are scheduling-independent. Float accumulators destroy reproducibility.
- **Counter-based RNG addressing**: stream keyed by `(run, generation, game, ply, purpose)`, splitmix64-seeded xoshiro256++ per semantic unit. Deal 12345 must replay identically regardless of thread count. This is worth more than any generator speed difference — and it is *required* by the duplicate-rotation evaluation harness (§5).
- **Set QoS explicitly** (`QOS_CLASS_USER_INITIATED` for P-cores); never put an E-core in a fork-join barrier with P-cores; measure thread count rather than trusting `hardware_concurrency()` (16 threads slower than 12 on M4 Max is a reported failure mode).
- Expected throughput: ~0.9–2.2 M self-play games/hour at Gumbel `n=32` on 12 workers, ~22–55 M training rows/hour.

---

## 2. Ranked techniques by (expected strength gain) / (implementation + compute cost)

Ranking is by ratio, not by absolute gain. Cost is calendar time for one engineer plus compute.

### Tier S — do these before anything else

| # | Technique | Why it ranks here | Cost |
|---|---|---|---|
| **1** | **Exact block-DP belief with disjunctive ask-legality** (§4) | Every other module consumes it. Replaces the current rejection sampling for constraint (C5). Measured 0.72 s for 27 exact constraints in unoptimised NumPy ⇒ single-digit ms in C. Gives declaration probabilities *exactly*, which nothing else can. The alternative (naive per-card sampling + hand-size check) accepts **0.04–0.07%** of the time. | 3–5 days |
| **2** | **6-rotation duplicate evaluation harness + per-seat RNG streams** | Multiplies the effective value of *every subsequent experiment*. Seats alternate teams, so the cyclic group of 6 rotations gives a fully balanced block: odd rotations swap which team holds which hands, even rotations permute hands within a team. Duplicate poker alone cut s.d. to ~2/3; Fish's structure is strictly stronger. Without it, resolving 20 Elo needs ~9,500 games. | 1–2 days |
| **3** | **Declaration as exact optimal stopping** (§1.4), including the `C_steal = 0` martingale theorem and the control-transfer declare | F4. The single largest strategic gap in all prior art, and it is nearly free once (1) exists. Prior agents declare greedily the instant a set is provable, which destroys the stalemate-breaker, the coin-flip control transfer, and the free-information channel. | 1–2 days |
| **4** | **BCD measurement harness** (`lc`, `b`, `df`) | Hours of engineering, minutes of CPU, and it tells you *a priori* how much search architecture to buy. Measure `df` from information-set retention ρ between your own consecutive moves via the derived conversion `df = 2(1−ρ)/(2−ρ)` (Skat/Hearts `df≈0.6` ⇒ `ρ≈0.571`). Measure `lc` **separately for ask-decisions and declare-decisions** — the prediction is that nearly all anti-correlation lives in declarations. Also publishable (§6). | 1 day |
| **5** | **GSPRT on normalized Elo for the commit loop** | `t_n = (μ−½)/σ_pg`, `e_n = 347.43 t_n`, LLR bounds ±2.94 at α=β=0.05, `T ≈ 10⁶/(Δe_n)²` blocks. Prevents the 61% false-positive rate that naive continuous monitoring produces under the null. | 1 day |

### Tier A — the core agent

| # | Technique | Justification | Cost |
|---|---|---|---|
| **6** | **Gumbel Sequential-Halving root search** (`m=16`, `n=16–32`) | 12–25× self-play throughput at equal policy quality, with a *guaranteed* improvement property at n=2 that PUCT lacks. Largest single compute lever on a GPU-less machine. | 3–4 days |
| **7** | **Single info-set tree + availability-count UCB at non-self nodes** | Fixes strategy fusion at your own nodes and the budget-splitting waste of PIMC (in LOTR:C a 1×10000 tree beat a 40×250 ensemble by 22.9%, reaching depth 8.6 vs 4.1). Cheap relative to PIMC. | 4–6 days |
| **8** | **NNUE incremental evaluation + ownership auxiliary head** | 0.3–1.0 µs/eval makes CPU AlphaZero viable at all. The ownership head is a free 1.3–1.65× training speedup *and* it directly supervises the belief model. | 1–2 weeks |
| **9** | **RIS re-determinization at non-root seats** (gated) | The *only* mechanism in the literature that makes signalling and teammate-information value visible to search. Hanabi: 3.87 (budget-insensitive) → 5.21 (budget-responsive). | 3–4 days |
| **10** | **SAD decoupled exploration + shared correlation seed** | Essentially free; both are *prerequisites* rather than improvements. Without SAD, ε-greedy destroys every convention as it forms. Without the seed, you have no correlation device and the price of uncorrelation is unbounded. | 1 day |
| **11** | **Unified lockout / ask-the-asker term** | `Q(a)` penalised by expected control cost `(1−μ_{j,c})·D_j`, with the provable floor `μ_{j,c} ≥ 1/u_{j,H}` for any half-suit `j` has asked in. This *unifies* blackballing (D3/P4/W2) with ask-the-asker (D5/D11) — asking the asker is safe precisely because `μ` is high — and the floor is typically far above the ~0.2 unconditional prior. Cheap, and it matches every human source. | 2 days |
| **12** | **Smooth-UCT root mixing + `A*` near-noise sampling** | ~Zero overhead. Converts a fully invertible deterministic root policy into a mixed one. Turned diverging UCT into converging UCT in Kuhn and Leduc. | Half a day |
| **13** | **Self-determinization from ψ at fraction `β_self`** | Makes deceptive asks visible to search. In *The Resistance*, `SPLIT` alone produced emergent bluffing at large budgets; 3/8–7/8 of budget all worked (broad plateau ⇒ low tuning risk). | 2 days |

### Tier B — worth real effort once Tier A is measured

| # | Technique | Justification | Cost |
|---|---|---|---|
| **14** | **LBR-team exploitability auditor** | Your only *absolute* yardstick. Fish makes LBR easier than poker: tens of legal asks (not thousands of bet sizes), declarations are the natural game-ending action, and the exact posterior is the belief code you already have. Sweep *when* LBR is allowed to act — greedy LBR acting too early understated exploitability ~10× in poker. | 1 week |
| **15** | **Cross-play protocol (SP / Intra-XP / inter-algorithm XP)** | Fish is saturated with implicit signalling; self-play score measures skill *and* convention-sharing and cannot separate them. Hanabi SAD: 23.97 self-play vs **2.52** cross-play. The SP−XP gap is your convention-overfitting metric. | 2 days |
| **16** | **FXP counter-population cross-play** | ~30% compute for direct attack on the local-team-equilibrium trap that FXP proves self-play falls into. | 3 days + compute |
| **17** | **Endgame TB-DAG/TPI + DCFR(3/2,0,2)** | Exact TMECor on ≤2-half-suit endgames; gives sound leaf values and the ground truth for research questions. 2–3 orders of magnitude faster than column generation. | 2 weeks |
| **18** | **Off-Belief Learning levels 1→3** | The sharpest available *dial on convention depth*, and Theorem 1 gives a **unique** policy independent of initialisation — independently trained runs interoperate. Hanabi 21 → 24.10 across levels 1→4. Prediction: OBL-1 is much closer to optimal in Fish than in Hanabi, because ask legality carries so much grounded information. | 1 week + 3–4 training runs |
| **19** | **MDS test-time search** (top-k, 20–50 particles) | Search buys more strength per FLOP than training (Hanabi: 24.08 → 24.61 from search alone), and MDS is ~100× cheaper than SPARTA. Requires all three teammates running identically-seeded search. | 1 week |
| **20** | **CLOP tuning (`H = 3`) for the 3–8 continuous knobs** | Beat RSPSA, hand-tuned SPSA, both cross-entropy variants, and UCT on all smooth noisy-win-rate problems. Coulom explicitly reports UH-CMA-ES "does not work well" here. | 2 days |
| **21** | **MIVAT root control variate** | Fish's chance structure collapses MIVAT's luck term to a *single* root correction `V(deal) − E_d[V(d)]`, and `E_d[V(d)]` is computable offline from millions of sampled deals with zero gameplay. Fit `θ* = [Σ(A_t−Ā)(A_t−Ā)ᵀ]⁻¹[Σ(A_t−Ā)(u_t−ū)]`. Variance falls by `1−Corr²`. Caveat: MIVAT in 6-player poker got only ~18–20% — multiplayer feature engineering is where it lives or dies. | 3 days |
| **22** | **Joint Policy Search over a hand-authored convention library** | The best "learn a convention" tool in the literature for exactly this problem: improving an ask convention needs *simultaneous* changes at several infosets (asker starts signalling, partner starts reading), which no single-agent gradient will find. JPS scores simultaneous multi-infoset changes in `O(|S|+M)` instead of `O(|S|·M)` and took bridge bidding from +0.29 to +0.63 IMPs/board. Bridge bidding *is* Fish asking. Yields interpretable, human-transferable conventions. | 2 weeks |

### Tier C — full AIVAT, α-Rank/Nash averaging, piKL, Team-PSRO

Build only when the agent is strong enough that the measurement is the bottleneck (AIVAT: 68.8% s.d. reduction in HUNL, 99.9% in Leduc with full knowledge — but real engineering cost and the pathology in §3), or when human partnering becomes a goal (piKL), or when you have spare compute (Team-PSRO: each iteration is a full cooperative-RL best-response training run).

---

## 3. What will not work — traps, with the evidence

Ordered by how expensive the mistake is.

**3.1 Building a Fish double-dummy solver as the engine.** F3. It will be fast, correct, and near-constant across root moves, degenerating to `argmax Pr(target holds card)` — blind to information leakage, information gain, declaration timing, and signalling. *Do not mistake solver throughput for progress.* Build a DDS only to generate `Pr(ask succeeds)` features and as a sparring baseline.

**3.2 Importing Hanabi's convention density.** The biggest signalling trap. Cox et al.'s hat-guessing/modular codes work because every Hanabi player *sees everyone else's hand*, giving each receiver a `Σ_{j≠1,i} c_j` term to subtract. In Fish nobody sees any hand, teammate and opponent hands are exchangeable given public history, and an absolute codebook is decoded *identically* by all five listeners: `Σ_j I(X;A|h,x_j) − Σ_k I(X;A|h,x_k) ≤ 0`. Absolute-codebook conventions have **non-positive secrecy rate**. The only positive construction is a **receiver-relative codebook** (semantics defined relative to the receiver's own holdings — a channel with state known only at the decoder, `R_s = max[I(X;Y|S_j) − I(X;Z)] > 0`), and its cost is encoder-side ambiguity, so use it for coarse facts, not exact card identities. Expect equilibrium to sit far toward **pooling on card identity** (Farrell–Gibbons "subversion" regime: `v_i ≥ 0` but `v_i + w_i < 0`).

**3.3 Copying reward shaping from the prior art.** The Somani agent's `±20` per-ask signal *actively punishes* the three strongest human tactics: deliberate misses to inform a partner, free-channel asks inside team-owned half-suits, and lying low. **Shape on set differential, never on ask success.** And guard against its exact bug class: `_team_for_move` stores `int`, `game.winner` returns a plain `Enum`, so `0 == Team.EVEN` is `False` and every move in every game got −100. Add `assert(len(set(returns)) > 1)` to your training loop — this bug survived a public release, a PyPI publication, and CI.

**3.4 Belief traps.**
- **Naive per-card sampling + hand-size check**: 0.04–0.07% acceptance (~1,500–2,600 tries/deal). This is what GIB does, at 1–2 s per 50 deals.
- **Greedy sequential determinization dealers**: AI Factory's Spades sampler put a card with true probability 1/3 into **87%** of determinizations, causing reproducible blunders. Build the sampler so you can A/B it and measure the marginals it induces against ground truth.
- **Particle filters with reinvigoration**: constraints in Fish grow monotonically and capacities are constant at 9, so deprivation is *guaranteed* and "adding noise" injects provably impossible deals — a correctness bug. Recompute exactly each turn instead.
- **JSV FPRAS**: measured at 50,634 s for `n=10` vs Ryser's 0.003 s; the theoretical crossover with Ryser is `n≈68`, where it needs ≈420,984 years. **Documented trap; do not implement.**
- **Ryser/Glynn**: `2^54`. Useful only for unit-testing the DP on tiny instances.
- **Sequential importance sampling**: provably underestimates by an exponential factor for *any* row/column ordering — *and appears to converge while doing so*, defeating the usual diagnostics.
- **Sinkhorn as the acting belief**: measured 2–6% max marginal error, worsening exactly where Fish gets hard (sparse supports, peaked weights — 5.9% at 60% void). Fine as a warm start or move-ordering heuristic, never as the belief you act on.
- **Uniform-over-consistent-worlds**: wrong in a *directional* way. Opponents ask about half-suits they hold, so uniformity systematically underestimates `μ_{j,c}` for `c` in half-suits `j` has asked about. Every one-line-heuristic bot in the wild makes this error.

**3.5 The counter-intuitive negative result you must plan around.** Rebstock et al. found a **cheating** inference module — all mass on the true world — played *worse* than ordinary inference in Skat: −3.25 and −8.49 tournament points/game in suit and grand. They also found PI100 beat PI20 in suit/grand but *lost* in null, "contradicting the idea that a higher TSSR corresponds to better cardplay." **Better beliefs do not monotonically improve a determinizing searcher.** The cause is strategy fusion. Budget for search soundness, not only inference accuracy — and see §6 for why Fish is the ideal place to finally isolate this.

**3.6 Search machinery that does not apply.**
- **MO-ISMCTS / SO-ISMCTS+POM**: collapse to SO-ISMCTS under full move observability. Six identical trees for identical statistics.
- **MT-ISMCTS**: requires small information sets; Fish's are 10²⁸.
- **EPIMC / postponing**: gave large gains only in *private*-observation games (Dark Chess, Dark Hex, Phantom TTT) and **zero** in public-observation games. Fish is fully public. (One speculative counter-argument worth a labelled experiment: Fish's degeneracy is caused by DDS triviality, not private observation, so EPIMC with `d ≥ 2` and a *non-DDS* leaf evaluator might help for a different reason.)
- **RAVE / AMAF**: *detrimental* in Hearts and LOTR:C Dark; no benefit even with move-context bucketing. Card value is entirely context-dependent, and Fish is intensely order-sensitive.
- **maxⁿ / paranoid / MP-Mix leader-targeting**: Fish is 2-player constant-sum at the team level. Also note that in Hearts/Spades a 4-ply depth advantage bought 0.8 points or nothing — depth is not where card-game value lives.
- **Continual re-solving / MCCR / DeepStack gadgets**: the resolve gadget needs `CBV^{σ₂}(I₁)` per opponent information set — one value per (player, possible 9-card hand). And MCCR was measured **worse than IS-MCTS on every domain tested except small Liar's Dice**.
- **OOS as the online engine**: loses head-to-head to ISMCTS in large games (Goofspiel(13), 1 s: OOS wins 28.3% vs UCT). Keep it as an *offline auditor* on a reduced variant only.
- **DESPOT**: its regularizer targets single-agent POMDPs with a *small good policy*; Fish is adversarial with no small policy.
- **Full-length random playouts**: Battleship's lesson — high-variance returns make the tree add little over flat rollouts. And note Sturtevant's counter-intuitive Hearts result: a *stronger* rule-based playout policy performed *worse* than random. Test; never assume.

**3.7 Learning machinery that does not apply.**
- **Deep CFR / DREAM**: DREAM's exploration term is `(|A_i|/ε)^{d_i}` — *exponential in depth*, and Fish is 60–120 decisions deep. Deep CFR's regret bound degrades as `√ε_L` and its ablations require retraining the advantage net from scratch every iteration. Neither is sound in 3v3 without the coordinator conversion, which is what you cannot afford at full scale.
- **Outcome-sampling MCCFR**: `1/q(z)` over 60–120 decisions; the bound carries `1/√δ`. Use external sampling or ESCHER's estimator.
- **CFR+**: documented to underperform in games with catastrophic actions and to interact badly with abstraction and pruning. A wrong declaration is exactly a catastrophic action. Use DCFR(3/2,0,2).
- **NFSP**: dominated on sample efficiency by everything above; keep as a 50-line sanity baseline.
- **Full TB-DAG mid-game**: `O*(3^k)` in `k`-privateness blows up.
- **Un-annealed perfect-information critics**: systematically undervalue information-gathering asks.

**3.8 Parallelism and tuning traps.** Virtual loss on lock-free trees (degrades across all `C_p`, 2–64 threads — it is a *hard additive penalty* that persists even when workers are certain a node is optimal). Float accumulators across threads (non-reproducible). `std::mutex` per node (global-mutex tree parallelism *lost* strength from 4 to 16 threads). E-cores in barriers with P-cores. Root parallelism's apparent super-linearity (partly an artefact of UCT escaping local optima — test against a sequential run with `nThreads ×` time). CMA-ES on raw win rates. Transposition sharing across an information-set DAG when the *beliefs* differ — hash the propagated constraint set into the key or you get a silent, near-undebuggable regression.

**3.9 Evaluation traps.** Fitting the variance-reduction heuristic on evaluation data: Kim & Sandholm gradient-descended AIVAT's heuristic to make **every one of 14 players in the Pluribus dataset appear to win >2000 mbb/h**, and separately p-hacked every player to have both "significantly won" and "significantly lost" at p < 10⁻²⁰⁰. AIVAT is unbiased for *any* `v′` — that constrains the expectation, not your realised estimate. **Freeze `v′` before you see the eval data and record its hash.** Also: continuous monitoring of a fixed-sample CI (61.35% false positives under the null); percentile bootstrap (5.7–11.2% failure at nominal 5%); resampling *games* rather than *deals* (the 6 rotations of one deal are one cluster); a global RNG stream across seats (silently destroys the duplicate pairing); Elo over a population of near-clone checkpoints (Elo is not redundancy-invariant); and interpreting a **negative LBR score as evidence of low exploitability** — LBR lost 536 mbb/g to a bot whose true exploitability was 90.

**3.10 Two Fish-specific structural traps.**
- **Stalemates get *more* likely, not less, as the bot improves.** Lockout-aware policies mean both teams refuse to grant the turn to anyone dangerous, and the position stalls. Somani hit this and patched with jitter plus a 200-move cap. Use **Srinivasan's Forced Claims rule** (a defined terminating device) rather than an arbitrary move cap, and decide it before you measure anything.
- **The conventions fork must be decided now.** Pagat legalises partner signalling (Ali Salahuddin's convention); Develin explicitly forbids pre-agreed conventions. Self-play will invent codes regardless. Either allow them (and accept that your agent will not cooperate with human partners) or forbid them (train with randomly-paired partners from a diverse population, or penalise policies whose action distribution is not invariant under relabelling of hand-consistent worlds). Implement it as a toggle and measure both — the value of the convention is itself publishable (§6).

---

## 4. The exact belief-inference recommendation

**The headline: do not approximate. Fish belief is exactly solvable and cheap. Recompute from scratch every turn.**

### 4.1 The object

Fix a reasoning seat. Let `U` be the cards whose *initial owner* is unknown (`|U| ≤ 45` from a seat), `Q` the candidate owners (`k = 5`), and `c_q = 9` for every player — **constant for the entire game**, because the hidden variable is the initial deal and declarations/transfers never change it. Then

$$
P(x \mid h) \;\propto\; \underbrace{\prod_{i,q} w_{i,q}^{\,x_{i,q}}}_{\text{factorised: legality} \,+\, \text{learned prior}} \;\cdot\; \underbrace{\mathbf{1}[\text{(a)–(e)}]}_{\text{hard constraints}} \;\cdot\; \underbrace{L(h\mid x)}_{\text{policy likelihood}}
$$

with constraints (a) capacity `Σ_i x_{i,q} = c_q`; (b) per-card support sets from public denials, transfers, and own hand; (c) **disjunctive ask-legality** — an ask by `p` in half-suit `S` certifies `⋁_{i∈D_e}[origin(i) = p]`; (d) declaration equalities; (e) drop-out/count observations.

**Engineering decomposition: put (a), (b), (c), (d) and the factorised `w` inside an exact DP; handle only `L` by importance weighting.**

### 4.2 The core DP

Order the unknowns; state = capacity vector `n = (n_1..n_k)`.

$$
F_i(n) = \sum_{q:\,n_q \ge 1} w_{i,q}\,F_{i-1}(n - e_q),\qquad Z = F_N(c)
$$
$$
B_i(n) = \sum_{q:\,n_q \le c_q - 1} w_{i+1,q}\,B_{i+1}(n + e_q)
$$
$$
\mu_{i,q} \;=\; \Pr[\mathrm{origin}(i) = q \mid h] \;=\; \frac{1}{Z}\sum_n F_{i-1}(n)\,w_{i,q}\,B_i(n + e_q)
$$

**The complexity point that is easy to get wrong:** `F_i(n)` is nonzero only when `Σ_q n_q = i`, so the step index is *determined by the state*. `F` and `B` are each a **single array** of size `∏_q(c_q+1)`, iterated by increasing `Σn`, and total work is

$$
O\!\Big(k\prod_q(c_q+1)\Big) \;=\; 5\times10^5 \text{ (agent view)},\qquad 6\times10^6 \text{ (neutral observer)}
$$

— sub-millisecond in C, 0.8 MB / 8 MB. A naive one-array-per-card implementation wastes ~N× memory and time; the current `engine/src/belief.hpp` already indexes by capacity vector, which is right.

**Numerics:** measured `Z ≈ 10²⁰–10²⁴` with unit weights, worse with peaked learned weights. Rescale `F` by `1/max F` after each `Σn` level and accumulate `log` of the scale factors. Assert `Z > 0` on every update — `Z = 0` means a modelling bug or a rules violation, not a case to silently fall back from.

### 4.3 Ask-legality: half-suit block enumeration (the key recommendation)

Ask legality is **the richest signal in Fish**, and it is disjunctive, so it breaks the product form. Inclusion–exclusion costs `2^k` DP runs (measured: `k=6` ⇒ 64 runs ⇒ 4.0 s NumPy) and a real game generates **30–60** such constraints. `2^60` is not an option.

**But every disjunctive constraint is confined to a single half-suit.** So:

1. Partition `U` into the ≤9 half-suit blocks.
2. For each block, enumerate all `≤ k^{m_b}` assignments (`6^6 = 46,656` worst case, far fewer after support pruning) and **filter against every one of that block's disjunctive and declaration constraints exactly**.
3. Aggregate survivors into a count-vector table
$$
g_b(t) = \sum_{\substack{\text{assignments } a \text{ of block } b\\ \mathrm{count}(a) = t,\ a \models \text{constraints}_b}} \prod_{i \in S_b} w_{i,a_i},\qquad t \in \mathbb{Z}_{\ge0}^k,\ \textstyle\sum_q t_q = m_b
$$
4. Run the capacity DP **over blocks** rather than cards: `F_b(n) = Σ_t g_b(t) F_{b-1}(n − t)`. The `Σn`-determines-index identity still holds, so `F` and `B` remain single arrays.

Worst-case support `|supp(g_b)| = C(m_b+k−1, k−1) = 462`; **measured actual supports were 15–33**, because constraints and void information prune hard. Measured full engine — 9 blocks, 27 exact ask-legality constraints, exact marginals — **0.72 s in unoptimised NumPy**, forward/backward agreeing to 4.2×10⁻¹⁶, column sums exactly 9. In C this is single-digit milliseconds.

**This makes exact Fish belief — including full ask-legality reasoning — a solved problem at real-time speeds.** It is also the natural place to fold any residual that is confined to one half-suit: *any within-half-suit policy term can be made exact by folding it into `g_b`*, which is a large and underused lever.

**This is the concrete upgrade to the current `belief.hpp`,** which handles (C5) by exact rejection sampling from the (C1)–(C4) posterior. Rejection is correct but its acceptance rate falls as constraints accumulate — exactly late in the game when precision matters most.

### 4.4 Sampling

Given the backward table, sample sequentially with

$$
\Pr[\mathrm{origin}(i) = q \mid \text{prefix}] = \frac{w_{i,q}\,B_i(n + e_q)}{\sum_{q'} w_{i,q'} B_i(n + e_{q'})}
$$

Every prefix is extendable by construction (any `q` with `B_i(n+e_q) = 0` gets probability zero), so there are **no rejections and no dead ends**. `O(Nk) = 225` operations per sample after one shared backward pass; measured 110 µs/sample even in NumPy, ~1 µs in C. This is the exact version of what SIS approximates — the zero-variance limit, i.e. Diaconis–Kolesnik's "almost perfect" regime where `O(1)` samples suffice. GIB spends 1–2 s to produce 50 deals; this is four to five orders of magnitude better.

### 4.5 The certainty oracle comes free

`fish-prior-art` recommends a max-flow-with-lower-bounds circulation, probed per `(p,c)` pair, as a complete certainty oracle strictly subsuming the four local propagation rules (suit lower bound, half-suit closure, hand-count closure, unique-holder) and catching Hall-condition deductions they miss. That recommendation is correct — **but the exact DP already delivers it**: `μ_{p,c} = 0 ⟺ p provably lacks c`, `μ_{p,c} = 1 ⟺ p provably holds c`, and this is complete by construction over the full constraint set including the disjunctions, which the flow formulation handles only via lower bounds on bucket edges.

**Therefore: keep cheap arc-consistency propagation (O(1) bit operations, <100 ns) as a *pre-filter* that shrinks block supports, and use the DP marginals as ground truth. Do not build both a max-flow oracle and the DP.** Propagation rules worth keeping: singleton card, saturated player (`popcount(may[p]) == n[p]`), exhausted player, and `atLeastOne` collapse (`popcount(may[p] & HS_MASK(h)) == 1`).

### 4.6 The declaration query — the decisive advantage

Declaration requires naming the **exact allocation**, so the quantity to maximise is *not* `Pr[team holds H]` but

$$
\max_{A \in \mathcal{T}^H} \Pr[A \text{ exactly correct} \mid h]
$$

and the block enumeration **already materialises every candidate allocation with its exact probability**. Read the max-weight `t` and the max-weight assignment within it, weighted by `F_{b-1}·B_b`. No sampling error, no independence assumption. This is a decisive advantage over any sampling-based belief, and it is precisely the sub-problem that is *absent* from the only prior agent (which plays 4-player Fish, where allocation with one teammate is near-forced at 2⁶ and heavily constrained).

### 4.7 The residual policy likelihood — the only place approximation is needed

`L(h|x) = Π_t π_{p_t}(a_t | h_{<t}, x)` does not factorise over cards. Two-stage estimator: draw `x ~ q` exactly from the DP, weight by `ω(x) ∝ L(h|x)`, self-normalise. Monitor

$$
\mathrm{ESS} = \frac{1}{\sum_j \tilde\omega_j^2},\qquad \frac{\mathrm{ESS}}{N} \approx e^{-\sigma^2},\quad \sigma = \mathrm{sd}[\log \omega]
$$

The log-normal law held to within 1–2% in the measured sweep (`σ=0.44 → 82.7%`; `σ=0.89 → 45.6%`; `σ=1.77 → 3.0%`). **Design rule: budget `σ ≤ 0.8` nats.** If you exceed it: temper (`ω^{1/τ}`), distil the policy into per-card location logits and move them *inside* the DP as `w_{i,q}` (this bought a **300× ESS improvement** — 82.7% vs 0.3% — in the measured experiment), or fold half-suit-local residuals into `g_b` exactly.

Prefer policy-based reach `η(s|I) = Π π(h,a)` over the independent-card product `Π_c L(h)_{c,loc(c,s)}`. The independence assumption fails *exactly where Fish is correlated*: knowing `j` asked in half-suit `H` makes the presence of *any* `H`-card in `j`'s hand strongly correlated with the presence of others. Also soften every opponent model with a temperature — a deterministic model gives `P(move|world) ∈ {0,1}`, and one surprising opponent action zeroes your entire belief.

### 4.8 Two filters, always

Maintain `β` (with policy likelihood) **and** `β̃` (hard rules only, uniform on rule-consistent deals) in parallel. Then

$$
D_{\mathrm{KL}}(\beta_t \,\|\, \tilde\beta_t) = \text{bits of live convention},\qquad
\Delta_{\text{sig}}(a) = V_{\mathcal{T}}(\beta^{+a}) - V_{\mathcal{T}}(\tilde\beta^{+a}) = \text{signed realised signalling value}
$$

`Δ_sig` is *signed*: a convention that helps opponents more than teammates shows up negative. This is the exact quantity a Fish bot should be trading off, it is the sequential multi-agent restatement of Farrell–Gibbons' `v_i + w_i ≥ 0`, and it is precisely what OBL zeroes out by construction — which is why OBL is the natural training-time control knob.

### 4.9 What not to build

**No particle filter, no reinvigoration, no MCMC.** The constraint set is monotone and capacities are constant at 9, so there is no filtering recursion to degenerate — recompute exactly, in milliseconds. Sequential reweighting of a fixed particle set is exactly what causes deprivation, and "adding noise" to a discrete deal is not a meaningful operation; artificial reinvigoration would inject provably impossible deals. Reserve a swap chain (Solinas et al.'s Metropolis–Hastings `RingSwap`, whose Theorem 4 proves the stationary distribution is the true joint range) *only* as a rejuvenation move if you later adopt a persistent particle set to amortise expensive per-deal policy evaluations — and trigger it on `ESS < N/2`, not on a schedule.

**Test against brute force on 3-player/9-card instances** (machine-precision agreement is achievable: `Z` to 3.1×10⁻¹⁶, marginals to 2.2×10⁻¹⁶) and assert `Z > 0` on every update.

---

## 5. Evaluation protocol and metrics

Fish's chance structure — one root chance node, then a fully public transcript — is a *best case* for variance reduction. Exploit it.

### Tier 0 — harness invariants (free, do first)

- Per-seat deterministic RNG streams keyed by `hash(deal_id, rotation, seat)`. A global stream silently destroys the duplicate pairing.
- Assert `E[Y] = 0` for self-vs-self over rotation blocks — a free correctness test of the whole harness.
- Assert set totals sum to 9; assert training returns vary across games (§3.3).
- Freeze and hash every value function used in evaluation *before* the eval data exists.

### Tier 1 — every commit (minutes)

**6-rotation duplicate blocks.** Seats `0,2,4` = team A. Let `ρ` rotate all hands by one seat. Odd rotations swap which team holds which hands (the duplicate-bridge trick); even rotations permute hands *within* a team (removing "which of my three teammates got the strong hand"). The full cyclic orbit gives 3 even and 3 odd relabellings — a fully balanced block of 6 games per deal, strictly stronger than duplicate bridge:

$$
Y(d) \;=\; \tfrac13\sum_{k \text{ even}} X(d,k) \;-\; \tfrac13\sum_{k \text{ odd}} X(d,k)
$$

**Bootstrap-resample deals, not games** — the 6 games from one deal are one cluster. Prefer PBP-t or BCa over the raw percentile bootstrap.

**GSPRT on normalized Elo.** `t_n = (μ−½)/σ_pg`, `e_n = 347.43 t_n`, `T ≈ 10⁶/(Δe_n)²` blocks (5 nElo ⇒ ~42,000 blocks; 3 nElo ⇒ ~116,000). Use non-regression bounds (`e_{n,0} + e_{n,1} < 0`) for refactors, gainer bounds for changes. **Never eyeball a running mean.**

### Tier 2 — every release (hours)

**MIVAT root control variate**, then full **AIVAT** with a frozen value head, inverse-variance weighting (a GP/Bayesian-ridge heteroscedastic head gives structurally independent mean and variance, so IVW's bias vanishes — measured 24.5% SE reduction ⇒ 43% fewer hands), and an **asymptotic confidence sequence** with an empirical-Bernstein recheck at the stopping index. AV-AIVAT reported a median **74× stopping-time reduction** at ±1 BB in HUNL. In Fish `|u| ≤ 9` sets so `B_X = 9`; keep the value head clipped to `[−9,9]` so `B_Y = 9 + 2KV` stays tight (loose bounds gutted EB-CS to 1.365× in poker). Report the estimate, the confidence sequence, the stopping index, and the value-function hash. Use ε-smoothed policies in evaluation or the importance-sampling unbiasedness proofs break on un-played lines.

### Tier 3 — absolute quality (cluster job)

**LBR-team.** The three LBR-controlled seats share a joint belief over deals (legitimate for a lower bound: a correlated team best response lower-bounds TMECor exploitability).

```
LBR_Fish(π, public_state, my_hand):
  1. exact posterior over deals from the transcript          [the same belief code the agent uses]
  2. every legal declaration D:  U(D) = 2·Pr(D correct | π) − 1  + continuation
  3. every legal ask (target t, card c):
       p_hit  = Σ_d π(d)·1[t holds c in d]
       U(ask) = p_hit·Val(after transfer, keep turn) + (1−p_hit)·Val(after miss, lose turn)
  4. argmax over declarations and asks
```

Cost with `|A| ≈ 30`, `M ≈ 2000` particles, 1 µs rollouts: ~60 ms/decision, ~5 s/game. Two poker lessons transfer verbatim: **sweep when LBR is allowed to act freely** (greedy LBR acting too early *understated* exploitability ~10×), and **a negative LBR score proves nothing**. Report the *maximum* over sweeps — it is a lower bound. Validate the whole LBR implementation on a **small-Fish** variant (4 players, 2 teams, 3 half-suits of 4, 6 cards each) where exact team best response *is* computable — this is Fish's Leduc, and every technique in this document should be validated there first.

### Tier 4 — population and team-specific (periodic)

- **Report SP, Intra-XP (inter-seed cross-play), and inter-algorithm XP separately.** The **SP − XP gap is your convention-overfitting metric**, and it is the single most important team-specific number. Hanabi precedent: 23.97 self-play vs 2.52 cross-play.
- **Do not rank checkpoints by Elo** — the population is full of near-clones and Elo is not redundancy-invariant. Build the antisymmetric logit matrix over *team policies* and use **maxent-Nash averaging** `n_A = A p*_A` for the headline ranking, `mElo₂` residual for cycle detection, **WHR** for the training-time curve (`O(1)` incremental via tridiagonal Newton), and ResponseGraphUCB for adaptive game allocation if you go to α-Rank for per-seat ratings.
- Held-out validation opponents never used in training (AlphaStar protocol); report the fraction beaten decisively.
- **Bonferroni/Holm-correct** simultaneous intervals over everything you report.

### Standing KPIs

| Metric | Definition | Why |
|---|---|---|
| Set differential | `(our sets − their sets)` | Primary objective; never win/loss |
| Declaration reliability | predicted `q` vs realised success, binned | Calibration of the module that decides the game |
| `TSSR` | `p(s*|h)·|I|` | Inference quality; plot by ask-number and seat |
| `D_KL(β‖β̃)` | bits of live convention | How much language the team is speaking |
| `Δ_sig(a)` | `V(β^{+a}) − V(β̃^{+a})` | Signed realised signalling value |
| `Δ_eaves` | `V_priv − V_pub` in an opponent-blind variant | The eavesdropping tax (§6) |
| SP − XP gap | self-play minus cross-play score | Convention overfitting |
| LBR-team lower bound | max over sweeps | Absolute exploitability floor |
| `lc`, `b`, `df` | BCD parameters, split ask/declare | Where the game actually lives |
| Stalemate rate | fraction of games hitting Forced Claims | Rises as the bot improves |

---

## 6. Open research questions this project can actually answer

There is **no published academic work on Literature/Canadian Fish**. That makes several of these first-of-kind results, and the ones below are chosen because (a) this codebase can answer them, (b) the answer is not obvious, and (c) they matter beyond Fish.

**Q1 — A new corner of the Long et al. PIMC map.** Fish sits at very high `df` (every ask is a hard constraint) with a *degenerate* leaf structure: at pre-terminal nodes almost every move is a win in the sampled world. That is the `b→1, lc→1` corner the 2010 paper never measured and dismissed as "very boring games in real life" — yet Fish is manifestly not boring, precisely because of the imperfect information PIMC discards. **Measure `(lc, b, df)` for Fish, separately for ask-decisions and declare-decisions, and show a game where PIMC gains ~nothing over random despite sitting in the region the framework predicts is favourable.** This is a genuine amendment to a well-cited predictive framework, and it costs one harness and minutes of CPU.

**Q2 — The eavesdropping tax, measured directly.** Build `G_priv`: identical to Fish except opponents observe only a coarsening of each ask (e.g. that a transfer occurred in some half-suit) while teammates see it fully. Then `Δ_eaves = V^conv_priv − V^conv_pub ≥ 0` is **the price of the opponents listening** — and no one has ever measured it in a real game. It is a ~20-line environment change and one training run. Decompose `Δ_conv = (gross signalling value) − (incremental eavesdropping tax)` and test Farrell & Gibbons' *subversion* prediction: that Fish sits in the regime where private-credible communication is destroyed by the public channel.

**Q3 — Do conventions saturate under adversarial observation?** Track `D_KL(β_t ‖ β̃_t)` across training. BAD attributed ~40% of Hanabi's information transfer to conventions. **Prediction: Fish's convention bits saturate far below that**, because absolute codebooks have non-positive secrecy rate (§3.2). If they do not saturate, either the bot has found a receiver-relative construction (interesting) or your metric is measuring nothing (run Lowe et al.'s scramble ablation — agents can score high on speaker consistency with *zero* causal effect).

**Q4 — Can a positive-secrecy-rate convention emerge from self-play, or must it be hand-coded?** Hanabi's evidence says conventions do **not** emerge from search: the "playable now" convention was worth ~2.5 points and was hand-written. Test whether a receiver-relative codebook ("the `t`-th card of half-suit `h′` counting up from *your* lowest card in `h′`", with the asked opponent's seat index selecting which teammate's frame applies) emerges under self-play, and if not, whether hand-coding it plus JPS refinement beats the emergent policy. Either answer is publishable.

**Q5 — Does exact belief reverse the "cheating inference plays worse" result?** Rebstock et al.'s finding (a perfect-information inference module scoring −3.25 and −8.49 TP/G) is one of the most cited counter-intuitive results in the field, and its explanation is strategy fusion. **Fish is the ideal testbed to isolate it**, because belief quality is *exactly* controllable and independently variable from search soundness: run the 2×2 of {exact belief, degraded belief} × {info-set search with RIS, plain PIMC}. If exactness only helps under sound search, that is a clean confirmation of the mechanism; if it hurts even under sound search, that is a genuinely new result.

**Q6 — Exact TMECor in a real 3v3 card game.** Solve ≤2-half-suit Fish endgames exactly via TB-DAG + DCFR and report the empirical **price of uncorrelation** — `v_Cor / v_No` — in a natural game rather than a worst-case construction. Worst-case bounds are `|L|/4`; nobody has published what it actually is anywhere real. Also: measure how much of the exact endgame value the learned agent captures.

**Q7 — Is the martingale declaration theorem right in practice?** The claim is that because a team provably holding all six cannot be robbed (opponents have no legal base card), `q` is a martingale and *waiting weakly dominates* — the folk stalemate-breaker is optimal. Sweep the declaration threshold `θ ∈ [0.5, 1]` and the delay policy against the exact endgame solver. Simultaneously test H4 (declare a 50/50 set to buy a control transfer) as an endgame-only ablation.

**Q8 — Resolve two conflicting human heuristics.** P6 says *exhaust one rank across all opponents before switching rank* (minimising the self-leak: two failed asks on the same rank leave one forced-zero in your row, not two). P7 says *after a success, keep hitting the same opponent to void them* (a void opponent can never legally ask in that half-suit again, permanently securing it). These conflict directly on the same decision. The stated reconciliation — P6 governs pre-success search, P7 post-success exploitation — is a hypothesis, not a theorem. Test jointly with paired ablations on identical deals.

**Q9 — A novel endgame implication.** Under the pagat rule, when one team is cardless the *opponents* nominate which of your players must declare everything — and they will pick your worst-informed player. That makes your objective a maximin over your own team and implies: **before the endgame, either equalise information across your team, or deliberately empty your least-informed teammate's hand so they cannot be nominated.** No source states this. It is directly testable.

**Q10 — Is OBL level-1 closer to optimal in Fish than in Hanabi?** Fish's *grounded* belief is far more informative than Hanabi's, because ask legality proves half-suit membership as a hard constraint. Prediction: the level-1→level-4 gap is much smaller in Fish than Hanabi's 21 → 24.10. If so, that is a general statement about which games reward convention depth — and it directly informs whether human-compatible Fish bots are cheap or expensive.

**Q11 — Gumbel planning in imperfect information.** Danihelka et al.'s Sequential-Halving result is perfect-information only. There is no published result on Gumbel + information-set search. If it transfers (and there is no structural reason it should not — the root improvement argument is about bandit allocation, not observability), that is a broadly useful contribution.

**Q12 — The half-suit block DP as a general algorithm.** Exact constrained-deal inference including *disjunctive* constraints, in polynomial time, at real-time speed, verified to machine precision — with measured block supports of 15–33 versus a worst case of 462. That is a contribution to the card-game inference literature independent of Fish, and it directly supersedes the rejection-sampling and Sinkhorn pipelines used by GIB, Kermit, and Solinas et al.

---

## 7. Consolidated bibliography

Deduplicated across all eight reports. `[U]` marks entries the source reports flagged as unverified or read only at abstract level; verify before relying on specifics.

### 7.1 Fish / Literature primary sources and implementations

1. McLeod, J. *Literature — card game rules.* pagat.com (© 2006–2025; last updated 1 Jul 2026). https://www.pagat.com/quartet/literature.html — canonical rules, Tactics, Public Information rules, Endgame protocol, the 54-card two-Joker nine-set variant, the "any error awards the set to opponents" variant, Ali Salahuddin's convention, Guy Srinivasan's Forced Claims / No Probabilistic Information / Challenge proposals.
2. Develin, M. *Canadian Fish*, ch. 9 of his card-games manual. https://web.archive.org/web/20170720075756/http://bantha.org/~develin/cardgames.html#ch9 — the deepest strategy writing found; exactly the 6-player declare-anytime error-forfeits variant; blackball example, "don't declare a set you own", the D13 free-channel corollary, the D15 control-transfer arithmetic, explicit prohibition of conventions.
3. Wikipedia. *Literature (card game).* https://en.wikipedia.org/wiki/Literature_(card_game) — information-asymmetry principle, the "stalemate breaker".
4. Deposit Genius. *Literature Game Strategy (Canadian Fish)* / *How to Play Literature.* https://depositgenius.com/literature-strategy-canadian-fish/
5. Somani, N. *literature.* GitHub (MIT, Python). https://github.com/neelsomani/literature — the only real prior agent; cloned at `3d2c75c`, all 1,940 LOC read, `model_10000.out` pickle disassembled (MLP 1149→100→1, `n_iter_ = 1,030,945`, `t_ = 2,001,821`). Contains the verified `0 == Team.EVEN` reward bug.
6. Somani, N. *literature-server.* https://github.com/neelsomani/literature-server ; live at https://literature.neelsomani.com/
7. Quines, C. *cfish.* https://github.com/cjquines/cfish — verified no AI code. Also `zairza-cetb/literature`, `Dynosol/playfish.io`, `doubleiis02/CanadianFish` (stub `Bot.java`, uniform 1/41 vectors never updated), `david-amirault/fish` [U], `Ryan1729/canadian-fish` [U], `iuruoy-shao/fish` [U], and several UI-only repos — none contains a working agent.
8. *Literature: Fish Card Game* (`com.cards.game.literature`), Google Play — advertises offline 4/6-player bots at three difficulty levels. Closed source, methodology **[U]**. The only known 6-player Fish bot other than yours.
9. **Negative search results** (2026-08-21): arXiv `all:"Canadian Fish"`, `all:"Literature card game"`, `all:"half-suit"`, `abs:"Go Fish"` → 0 results each. GitHub search across five query forms → nothing with search, planning, CFR, or functioning RL. No CS221/CS229/Berkeley/MIT course project found.

### 7.2 PIMC, determinization, and its pathologies

10. Long, J., Sturtevant, N., Buro, M., Furtak, T. *Understanding the Success of Perfect Information Monte Carlo Sampling in Game Tree Search.* AAAI-10, 134–140. https://webdocs.cs.ualberta.ca/~nathanst/papers/pimc.pdf — the `lc`/`b`/`df` framework.
11. Frank, I., Basin, D. *Search in games with incomplete information: a case study using Bridge card play.* AI 100(1–2):87–123, 1998 — origin of strategy fusion and non-locality.
12. Frank, I., Basin, D., Matsubara, H. *Finding Optimal Strategies for Imperfect Information Games.* AAAI-98, 500–507. https://cdn.aaai.org/AAAI/1998/AAAI98-071.pdf — vector minimaxing, payoff-reduction minimaxing (66.3% → **95.8%** on 650 Bridge suit combinations).
13. Frank, I., Basin, D. *Optimal Play against Best Defence: Complexity and Heuristics.* CG 1998, LNCS 1558 — NP-completeness of optimal play in the best-defence model.
14. Ginsberg, M. *GIB: Imperfect Information in a Computationally Challenging Game.* JAIR 14:303–358, 2001. https://www.jair.org/index.php/jair/article/view/10279 — achievable sets, lattice-valued α-β, Bayesian deal reweighting, 50-world samples, Bridge Master 100/180.
15. Ginsberg, M. *Partition Search.* AAAI-96, 228–233. **[U]**
16. Levy, D. *The Million Pound Bridge Program.* First Computer Olympiad, 1989 — the original PIMC proposal. **[U]**
17. Cazenave, T., Ventos, V. *The αμ Search Algorithm for the Game of Bridge.* arXiv:1911.07960. https://arxiv.org/abs/1911.07960
18. Cazenave, T., Legras, S., Ventos, V. *Optimizing αμ.* arXiv:2101.12639. https://arxiv.org/abs/2101.12639
19. Furtak, T., Buro, M. *Recursive Monte Carlo Search for Imperfect Information Games.* IEEE CIG 2013. https://skatgame.net/mburo/ps/recmc13.pdf — IIMC/RecPIMC; exploitability 0.299 → 0.088 at one recursion level; the ISMCTS information-leakage critique.
20. Bouzy, B., Rimbaud, A., Ventos, V. *Recursive Monte Carlo Search for Bridge Card Play.* IEEE CoG 2020, 229–236. https://ieee-cog.org/2020/papers/paper_82.pdf
21. Arjonilla, J., Saffidine, A., Cazenave, T. *Perfect Information Monte Carlo with Postponing Reasoning* (EPIMC). arXiv:2408.02380. https://arxiv.org/abs/2408.02380 — large gains in private-observation games, **nil** in public-observation games.
22. Šustr, M., Schmid, M., Moravčík, M., Burch, N., Lanctot, M., Bowling, M. *Sound Algorithms in Imperfect Information Games.* arXiv:2006.08740. https://arxiv.org/abs/2006.08740
23. Müller, M. *Partial order bounding.* AI 129(1–2):279–311, 2001. **[U]** ; Dasgupta, P., Chakrabarti, P.P., DeSarkar, S.C. *Searching game trees under a partial order.* AI 82(1–2):237–257, 1996. **[U]**
24. Bethe, P. M. *The State of Automated Bridge Play.* NYU, 2010. https://cs.nyu.edu/~pbethe/bridgeReview200908.pdf

### 7.3 Skat / trick-taking engineering, evaluation and inference

25. Buro, M., Long, J., Furtak, T., Sturtevant, N. *Improving State Evaluation, Inference, and Search in Trick-Based Card Games.* IJCAI-09, 1407–1413. https://www.ijcai.org/Proceedings/09/Papers/236.pdf — Kermit; GLEM evaluation; feature-factorised inference; **+217 tournament points/36 games from inference alone**; defender inference dominant.
26. Kupferschmid, S., Helmert, M. *A Skat Player Based on Monte-Carlo Simulation.* CG 2006, LNCS 4630, 135–147. https://ai.dmi.unibas.ch/papers/kupferschmid-helmert-cg2006.pdf — move ordering 3.45×, quasi-symmetry reduction 2.38×, adversarial heuristics 1.80×.
27. Solinas, C., Rebstock, D., Buro, M. *Improving Search with Supervised Learning in Trick-Based Card Games.* AAAI-19; arXiv:1903.09604. https://arxiv.org/abs/1903.09604 — per-card location net; the **TSSR** metric.
28. Rebstock, D., Solinas, C., Buro, M., Sturtevant, N. *Policy Based Inference in Trick-Taking Card Games.* IEEE CoG 2019; arXiv:1905.10911. https://arxiv.org/abs/1905.10911 — reach-probability inference; **the cheating-inference negative result** (−3.25 / −8.49 TP/G).
29. Solinas, C., Rebstock, D., Sturtevant, N., Buro, M. *History Filtering in Imperfect Information Games: Algorithms and Complexity.* NeurIPS 2023; arXiv:2311.14651. https://arxiv.org/pdf/2311.14651 — construction is FNP-complete in the worst case; sparse vs **dense** public trees; the TTCG `RingSwap` + Metropolis–Hastings sampler (Thms 3–4).
30. Furtak, T., Buro, M. *Minimum Proof Graphs and Fastest-Cut-First Search Heuristics.* IJCAI-09. **[U]** ; Buro, M. *From Simple Features to Sophisticated Evaluation Functions* (GLEM). CG'98. **[U]**
31. Schäfer, J. *The UCT Algorithm Applied to Games with Imperfect Information.* MSc thesis, Magdeburg, 2007 — the "Bernie" information-set-UCT Skat player. **[U]**
32. Haglund, B. et al. *DDS — Double Dummy Solver for Bridge.* https://github.com/dds-bridge/dds
33. Pavlicek, R. *Dealing with Constraints.* https://www.rpbridge.net/8h11.htm

### 7.4 ISMCTS and MCTS under hidden information

34. Cowling, P., Powley, E., Whitehouse, D. *Information Set Monte Carlo Tree Search.* IEEE TCIAIG 4(2):120–143, 2012. https://eprints.whiterose.ac.uk/id/eprint/75048/1/CowlingPowleyWhitehouse2012.pdf — the availability-count UCB; **all ISMCTS variants coincide under full move observability**; `c = 0.7`.
35. Whitehouse, D. *Monte Carlo Tree Search for Games with Hidden Information and Uncertainty.* PhD thesis, York, 2014. https://etheses.whiterose.ac.uk/8117/ — ICARUS framework; EPIC/NAST/MAST; RAVE detrimental in Hearts; the non-convergence observation.
36. Powley, E., Cowling, P., Whitehouse, D. *Information capture and reuse strategies in MCTS.* AI 217:92–116, 2014. **[U]**
37. Cowling, P., Whitehouse, D., Powley, E. *Emergent Bluffing and Inference with Monte Carlo Tree Search.* IEEE CIG 2015, 114–121. http://orangehelicopter.com/academic/papers/cig15.pdf — MT-ISMCTS; the tree-statistics particle update; self-determinization; the `BLUFF` rule.
38. Whitehouse, D., Cowling, P., Powley, E., Rollason, J. *Integrating MCTS with Knowledge-Based Methods to Create Engaging Play in a Commercial Mobile Game.* AIIDE 2013. https://cdn.aaai.org/ojs/12679/12679-52-16196-1-2-20201228.pdf — 2500 iterations in <0.25 s / 140 KB; **the 1/3-vs-87% biased-determinization post-mortem**; 1200→5000 iterations bought only 7–8%.
39. Goodman, J. *Re-determinizing Information Set Monte Carlo Tree Search in Hanabi.* arXiv:1902.06075. https://arxiv.org/abs/1902.06075 — RIS-MCTS.
40. Auger, D. *Multiple Tree for Partially Observable Monte-Carlo Tree Search.* EvoApplications 2011, LNCS 6624, 53–62. https://hal.science/hal-00563480v2/document **[U]**
41. Silver, D., Veness, J. *Monte-Carlo Planning in Large POMDPs* (POMCP). NIPS 2010. https://papers.nips.cc/paper/4031-monte-carlo-planning-in-large-pomdps
42. Ye, N., Somani, A., Hsu, D., Lee, W.S. *DESPOT: Online POMDP Planning with Regularization.* JAIR 58:231–266, 2017. arXiv:1609.03250.
43. Lisý, V., Lanctot, M., Bowling, M. *Online Monte Carlo Counterfactual Regret Minimization for Search in Imperfect Information Games* (OOS). AAMAS 2015, 27–36. https://mlanctot.info/files/papers/aamas15-iioos.pdf — ISMCTS exploitability *increases* with computation in Goofspiel.
44. Heinrich, J., Silver, D. *Smooth UCT Search in Computer Poker.* IJCAI 2015, 554–560. https://www.ijcai.org/Proceedings/15/Papers/084.pdf
45. Clark, G. *Deep Synoptic Monte-Carlo Planning in Reconnaissance Blind Chess.* NeurIPS 2021; arXiv:2110.01810.
46. Brown, N., Sandholm, T. *Safe and Nested Subgame Solving for Imperfect-Information Games.* NIPS 2017; arXiv:1705.02955.
47. Šustr, M., Kovařík, V., Lisý, V. *Monte Carlo Continual Resolving.* AAMAS 2019; arXiv:1812.07351 — **MCCR worse than IS-MCTS on all tested games except small Liar's Dice**.
48. Kocsis, L., Szepesvári, C. *Bandit Based Monte-Carlo Planning* (UCT). ECML 2006, 282–293. **[U]** ; Auer, P., Cesa-Bianchi, N., Fischer, P. *Finite-time Analysis of the Multiarmed Bandit Problem.* ML 47:235–256, 2002 (UCB1/UCB-Tuned) **[U]** ; Auer et al. *The Nonstochastic Multiarmed Bandit Problem.* SICOMP 32(1):48–77, 2002 (EXP3) **[U]**
49. Browne, C. et al. *A Survey of Monte Carlo Tree Search Methods.* IEEE TCIAIG 4(1):1–43, 2012 **[U]** ; Świechowski, M. et al. *MCTS: A Review of Recent Modifications and Applications.* arXiv:2103.04931.
50. Bitan, M., Kraus, S. *Combining Prediction of Human Decisions with ISMCTS.* arXiv:1709.09451 **[U]** ; *Incentivizing Information Gain in Hidden Information Multi-Action Games.* Springer LNCS, DOI 10.1007/978-3-031-34017-8_6 **[U]** — an explicit information-gain bonus inside ISMCTS; worth chasing.

### 7.5 Multi-player search

51. Sturtevant, N. *A Comparison of Algorithms for Multi-player Games.* CG 2002, LNCS 2883, 108–122. https://webdocs.cs.ualberta.ca/~nathanst/papers/comparison_algorithms.pdf
52. Sturtevant, N. *Current Challenges in Multi-Player Game Search.* CG 2004. https://cs.du.edu/~sturtevant/papers/Multi-PlayerChallenges.pdf
53. Sturtevant, N. *An Analysis of UCT in Multi-Player Games.* ICGA J. 31(4):195–208, 2008. https://webdocs.cs.ualberta.ca/~nathanst/papers/mpuct_icga.pdf — RAVE/history heuristic give nothing in Hearts; stronger playout policies can be worse.
54. Zuckerman, I., Felner, A., Kraus, S. *Mixing Search Strategies for Multi-Player Games* (MP-Mix). IJCAI-09, 646–651. https://www.ijcai.org/Proceedings/09/Papers/113.pdf — **Opponent Impact; Go Fish has `OI = 1`**.
55. Luckhardt, C., Irani, K. *An Algorithmic Solution of N-Person Games* (maxⁿ). AAAI-86 **[U]** ; Korf, R. *Multi-player alpha-beta pruning.* AI 48(1):99–111, 1991 **[U]** ; Sturtevant, N., Korf, R. *On Pruning Techniques for Multi-Player Games.* AAAI-2000, 201–207 **[U]**
56. Sturtevant, N., White, A. *Feature Construction for Reinforcement Learning in Hearts.* CG 2006, LNCS 4630, 122–134. https://sites.ualberta.ca/~amw8/hearts.pdf

### 7.6 Exact inference, permanents, contingency tables

57. Ryser, H. J. *Combinatorial Mathematics.* Carus Monographs 14, MAA, 1963 **[U]** ; Glynn, D. G. *The permanent of a square matrix.* EJC, 2010 **[U]** ; *Computing the permanent*, Wikipedia.
58. Chuiko, M., Richer, R., Richer, M., Ayers, P.W. et al. *Optimizing and benchmarking the computation of the permanent of general matrices.* arXiv:2510.03421. https://arxiv.org/abs/2510.03421
59. Jerrum, M., Sinclair, A., Vigoda, E. *A polynomial-time approximation algorithm for the permanent of a matrix with non-negative entries.* JACM 51(4), 2004. https://faculty.cc.gatech.edu/~vigoda/Permanent.pdf
60. Newman, A., Vardi, M. *FPRAS Approximation of the Matrix Permanent in Practice.* arXiv:2012.03367 — **50,634 s at n=10 vs Ryser's 0.003 s; crossover at n≈68 ≈ 420,984 years**.
61. Linial, N., Samorodnitsky, A., Wigderson, A. *A deterministic strongly polynomial algorithm for matrix scaling and approximate permanents.* Combinatorica 20(4), 2000. https://www.math.ias.edu/~avi/PUBLICATIONS/MYPAPERS/LSW98/lsw00.pdf
62. Vontobel, P. *The Bethe permanent of a non-negative matrix.* arXiv:1107.4196 / IEEE TIT 2013 ; Anari, N., Oveis Gharan, S. *A tight analysis of Bethe approximation for permanent.* arXiv:1811.02933 ; Chertkov, M., Yedidia, A.B. arXiv:1108.0065 ; Gurvits, L. et al. arXiv:2004.02425.
63. Brègman / Minc inequality; Schrijver's entropy proof and regular-bipartite lower bound; van der Waerden conjecture (Egorychev, Falikman). **[U] venues**
64. Chen, Y., Diaconis, P., Holmes, S., Liu, J. *Sequential Monte Carlo methods for statistical analysis of tables.* JASA 100(469):109–120, 2005.
65. Bezáková, I., Sinclair, A., Štefankovič, D., Vigoda, E. *Negative examples for sequential importance sampling of binary contingency tables.* Algorithmica / ESA 2006. https://people.eecs.berkeley.edu/~sinclair/sis_jasa.pdf — **exponential underestimation that appears converged**.
66. Diaconis, P., Kolesnik, B. *Randomized sequential importance sampling for estimating the number of perfect matchings.* arXiv:1907.02333 ; Chatterjee, S., Diaconis, P. *The sample size required in importance sampling.* **[U]**
67. Cryan, M., Dyer, M. *A polynomial-time algorithm to approximately count contingency tables when the number of rows is constant.* JCSS 2003 ; Cryan, Dyer, Goldberg, Jerrum, Martin. SICOMP 2006. https://webspace.maths.qmul.ac.uk/m.jerrum/papers/CDGJM06.pdf ; Dyer, M., Greenhill, C. TCS 2000 **[U]** ; Greenhill, C. *The switch Markov chain for sampling irregular graphs.* arXiv:1412.5249 ; Diaconis, P., Gangolli, A. *Rectangular arrays with fixed margins*, 1995 **[U]**.
68. Martino, L., Elvira, V., Louzada, F. *Effective sample size for importance sampling based on discrepancy measures.* Signal Processing, 2017.
69. Morenville, A., Piette, É. *Modeling Uncertainty: Constraint-Based Belief States in Imperfect-Information Games.* arXiv:2507.19263 — constraint-based beliefs perform *comparably* to belief-propagation marginals.

### 7.7 Public belief states, CFR, and team equilibria

70. Zinkevich, M., Johanson, M., Bowling, M., Piccione, C. *Regret Minimization in Games with Incomplete Information.* NIPS 2007.
71. Tammelin, O. *Solving Large Imperfect Information Games Using CFR+.* arXiv:1407.5042.
72. Brown, N., Sandholm, T. *Solving Imperfect-Information Games via Discounted Regret Minimization* (DCFR). AAAI 2019; arXiv:1809.04040 — `DCFR(3/2, 0, 2)`; **CFR+ underperforms with catastrophic actions**.
73. Lanctot, M., Waugh, K., Zinkevich, M., Bowling, M. *Monte Carlo Sampling for Regret Minimization in Extensive Games* (MCCFR). NIPS 2009 — constants **[U]**.
74. Brown, N., Lerer, A., Gross, S., Sandholm, T. *Deep Counterfactual Regret Minimization.* ICML 2019; arXiv:1811.00164.
75. Steinberger, E., Lerer, A., Brown, N. *DREAM.* arXiv:2006.10410 — exploration term exponential in depth.
76. McAleer, S., Farina, G., Lanctot, M., Sandholm, T. *ESCHER.* ICLR 2023; arXiv:2206.04122.
77. Sokota, S., D'Orazio, R., Kolter, J.Z., Loizou, N., Lanctot, M., Mitliagkas, I., Brown, N., Kroer, C. *A Unified Approach to RL, Quantal Response Equilibria, and Two-Player Zero-Sum Games* (Magnetic Mirror Descent). ICLR 2023; arXiv:2206.05825.
78. Brown, N., Bakhtin, A., Lerer, A., Gong, Q. *Combining Deep RL and Search for Imperfect-Information Games* (ReBeL). NeurIPS 2020; arXiv:2007.13544.
79. Schmid, M. et al. *Student of Games.* Science Advances 2023; arXiv:2112.03178 — GT-CFR, CVPN; **Scotland Yard is the closest published public-observation analogue**.
80. Kovařík, V., Schmid, M., Burch, N., Bowling, M., Lisý, V. *Rethinking Formal Models of Partially Observable Multiagent Decision Making.* arXiv:2111.05884.
81. Nayyar, A., Mahajan, A., Teneketzis, D. Common-information / public-belief approach, 2013. **[U]**
82. Celli, A., Gatti, N. *Computational Results for Extensive-Form Adversarial Team Games.* AAAI 2018; arXiv:1711.06930 — TMECom/TMECor/TME; **TMECor FNP-hard, BR-T APX-hard; price of uncorrelation `|L|/2`, `|L|/4`, `√|L|`**.
83. Farina, G., Celli, A., Gatti, N., Sandholm, T. *Connecting Optimal Ex-Ante Collusion in Teams to Extensive-Form Correlation.* ICML 2021, PMLR 139:3164–3173 **[U]** ; and *Ex ante coordination and collusion in zero-sum multi-player extensive-form games.* NeurIPS 2018 **[U]**.
84. Carminati, L., Cacciamani, F., Ciccone, M., Gatti, N. *A Marriage between Adversarial Team Games and 2-player Games* (TPI). ICML 2022; arXiv:2206.09161.
85. Zhang, B.H., Farina, G., Celli, A., Sandholm, T. *Team Belief DAG.* arXiv:2202.00789 (ICML 2023) — Thms 4.2/4.3; `O*(3^k)` `k`-private edge bound; 3K8 in 4.73 s vs 3 m 23 s.
86. Zhang, B.H., Farina, G., Celli, A., Sandholm, T. *Subgame Solving in Adversarial Team Games.* NeurIPS 2022. **[U]**
87. McAleer, S., Farina, G., Zhou, G., Wang, M., Yang, Y., Sandholm, T. *Team-PSRO.* NeurIPS 2023.
88. Xu, Z., Liang, Y., Yu, C., Wang, Y., Wu, Y. *Fictitious Cross-Play.* AAMAS 2023; arXiv:2310.03354 — **self-play converges to local team equilibria w.h.p.**
89. Celli, A., Ciccone, M., Bongo, R., Gatti, N. *Soft Team Actor-Critic.* arXiv:1912.07712 ; *Leveraging Team Correlation for Approximating Equilibrium in Two-Team Zero-Sum Games* (S-PSRO/rCTME). arXiv:2403.00255 ; *Team-Fictitious Play.* NeurIPS 2024 **[U]** ; Aggarwal, N., How, J.P. arXiv:2607.09993 **[U]**.
90. Lanctot, M. et al. *A Unified Game-Theoretic Approach to Multiagent RL* (PSRO, NashConv). NeurIPS 2017. **[U]**

### 7.8 Cooperative hidden information, conventions, signalling

91. Bard, N. et al. *The Hanabi Challenge: A New Frontier for AI Research.* AI 280, 2020; arXiv:1902.00506 — babbling equilibria; ad-hoc brittleness; exploration is *holistically* damaging to conventions.
92. Foerster, J. et al. *Bayesian Action Decoder.* ICML 2019; arXiv:1811.01458 — PuB-MDP; prescriptions; 24.174/25; ~40% of information from conventions; 40% less belief uncertainty.
93. Hu, H., Foerster, J. *Simplified Action Decoder.* ICLR 2020; arXiv:1912.02288 — decoupled exploratory/greedy action; free, and a prerequisite for conventions.
94. Hu, H., Lerer, A., Peysakhovich, A., Foerster, J. *"Other-Play" for Zero-Shot Coordination.* ICML 2020; arXiv:2003.02979 — SP 23.97 self-play vs **2.52** cross-play; OP+AUX reaches 22.07.
95. Hu, H., Lerer, A., Cui, B., Pineda, L., Brown, N., Foerster, J. *Off-Belief Learning.* ICML 2021; arXiv:2103.04000 — counterfactual belief `B_{π₀}`; Thm 1 uniqueness; level 1 ≈ 21 grounded → level 4 = 24.10 SP / 23.76 XP.
96. Lerer, A., Hu, H., Foerster, J., Brown, N. *Improving Policies via Search in Cooperative Partially Observable Games* (SPARTA). AAAI 2020; arXiv:1912.02318 — 24.08 → 24.61; `V_{π_s} − V_{π_b} ≥ −2TΔ|A|N^{−1/2}`; **2 core-hours/game single-agent, ~90 multi-agent**.
97. Hu, H., Wu, D.J., Lerer, A., Foerster, J., Brown, N. *Learned Belief Search.* arXiv:2106.09086 — 55–91% of exact-search benefit at 4.6–42× less compute.
98. Sokota, S., Farina, G., Wu, D.J., Hu, H., Wang, K.A., Kolter, J.Z., Brown, N. *The Update-Equivalence Framework for Decision-Time Planning* (MDS/MMDS). arXiv:2304.13138 — matches SPARTA at ~100× less search time with 10-particle posteriors.
99. Sokota, S., Lockhart, E., Timbers, F., Davoodi, E., D'Orazio, R., Burch, N., Schmid, M., Bowling, M., Lanctot, M. *Solving Common-Payoff Games with Approximate Policy Iteration* (CAPI). arXiv:2101.04237 — factorised prescriptions.
100. Jacob, A.P., Wu, D.J., Farina, G., Lerer, A., Hu, H., Bakhtin, A., Andreas, J., Brown, N. *Modeling Strong and Human-Like Gameplay with KL-Regularized Search* (piKL). ICML 2022; arXiv:2112.07544 ; Hu, H. et al. *Human-AI Coordination via Human-Regularized Search and Learning.* arXiv:2210.05125 **[U]**.
101. Cui, B., Hu, H., Pineda, L., Foerster, J. *K-level Reasoning for Zero-Shot Coordination in Hanabi.* NeurIPS 2021; arXiv:2207.07166 **[U]** ; Lupu, A., Cui, B., Hu, H., Foerster, J. *Trajectory Diversity for Zero-Shot Coordination.* ICML 2021 **[U]** ; Anwar, U. et al. *Noisy Zero-Shot Coordination.* arXiv:2411.04976 **[U]**.
102. *Ad-Hoc Human-AI Coordination Challenge (AH2AC2).* arXiv:2506.21490 — **OBL(L4) at 21.04 beats human-data methods that use human data**.
103. Cox, C., De Silva, J., DeOrsey, P., Kenter, F., Retter, T., Tobin, J. *How to Make the Perfect Fireworks Display: Two Strategies for Hanabi.* Mathematics Magazine 88(5):323–336, 2015 — the hat-guessing construction that **does not port to Fish**.
104. Bouzy, B. *Playing Hanabi Near-Optimally.* ACG 2017 **[U]** ; Wu, J. *WTFWThat* hat-player docs **[U]**.
105. Farrell, J., Gibbons, R. *Cheap Talk with Two Audiences.* AER 79(5):1214–1223, 1989 — **separating in public iff `v₁+w₁ ≥ 0` and `v₂+w₂ ≥ 0`**; one-sided discipline / subversion / mutual discipline.
106. Crawford, V., Sobel, J. *Strategic Information Transmission.* Econometrica 50(6), 1982 **[U]** ; Spence, M., job-market signalling **[U]** ; Sobel, J. *Signaling Games* (encyclopedia) **[U]**.
107. Wyner, A.D. *The Wire-Tap Channel.* BSTJ 1975 ; Csiszár, I., Körner, J. *Broadcast Channels with Confidential Messages.* IEEE TIT 1978. **[U] primary**
108. Gossner, O., Mertens, J.-F. *The Value of Information in Zero-Sum Games*, 2001 **[U]** ; Bassan, B., Gossner, O., Scarsini, M., Zamir, S. *Positive Value of Information in Games.* IJGT 2003 **[U]** ; Lehrer, E., Rosenberg, D. MOR 35(4):851–863, 2010 **[U]** ; Kamenica, E., Gentzkow, M. *Bayesian Persuasion.* AER 101(6), 2011 **[U]**.
109. Lowe, R., Foerster, J., Boureau, Y.-L., Pineau, J., Dauphin, Y. *On the Pitfalls of Measuring Emergent Communication.* AAMAS 2019; arXiv:1903.05168 — **high speaker consistency with zero causal effect; always run the scramble test**.
110. Eccles, T., Bachrach, Y., Lever, G., Lazaridou, A., Graepel, T. *Biases for Emergent Communication in Multi-agent RL.* NeurIPS 2019; arXiv:1912.05676.
111. Jaques, N. et al. *Social Influence as Intrinsic Motivation for Multi-Agent Deep RL.* ICML 2019; arXiv:1810.08647 ; Köster, R. et al. arXiv:2010.09054 **[U]** ; Abadi, M., Andersen, D. *Learning to Protect Communications with Adversarial Neural Cryptography.* arXiv:1610.06918 **[U]**.

### 7.9 Bridge bidding as a learned language

112. Rong, J., Qin, T., An, B. *Competitive Bridge Bidding with Deep Neural Networks.* AAMAS 2019; arXiv:1903.00900 — ENN/PNN split; 318-dim bidding history; +0.25 IMP over WBridge5.
113. Tian, Y., Gong, Q., Jiang, T. *Joint Policy Search for Multi-agent Collaboration with Imperfect Information.* NeurIPS 2020; arXiv:2008.06495 — policy-change density `ρ`; `O(|S|+M)`; **+0.29 → +0.63 IMPs/board**. (Exact definition of `c^{(σ,σ')}`: **[U]** — read §3 before implementing.)
114. Lockhart, E., Burch, N., Bard, N., Borgeaud, S., Eccles, T., Smaira, L., Smith, R. *Human-Agent Cooperation in Bridge Bidding.* arXiv:2011.14124 — Borel particle search; soft policy-iteration updates; **+0.97 IMPs/deal with human experts**; "prefers simpler, more direct auctions".
115. NukkAI / NooK, Paris challenge, March 2022 — 67/80 sets vs eight world champions. **Press coverage only; no primary technical source; and the challenge excluded bidding entirely.** Do not cite as evidence about learned signalling. **[U]**

### 7.10 Self-play RL for large card games

116. Zha, D., Xie, J., Ma, W., Zhang, S., Lian, X., Hu, X., Liu, J. *DouZero.* ICML 2021; arXiv:2106.06135 — DMC with action-as-input; 48 cores + 4 GPUs × 30 days; 1st of 344 on Botzone.
117. Zhao, Y., Lu, Y., Zhao, J., Zhou, W., Li, H. et al. *DanZero+.* arXiv:2312.02561 — **GuanDan: 4-player 2v2 no-communication team card game**, the closest published analogue; DMC then top-k PPO (90.12 → 92.70).
118. Guan, Y., Liu, M., Hong, W., Zhang, W., Fang, F., Zeng, G., Lin, Y. *PerfectDou: Dominating DouDizhu with Perfect Information Distillation.* NeurIPS 2022; arXiv:2203.16406 — PTIE; **10× sample efficiency**; 2.5e9 samples.
119. Li, J. et al. *Suphx: Mastering Mahjong with Deep RL.* arXiv:2003.13590 — global reward prediction; **oracle guiding with a dropout schedule `δ_t → 0`**; pMCPA.
120. Heinrich, J., Silver, D. *Deep RL from Self-Play in Imperfect-Information Games* (NFSP). arXiv:1603.01121.
121. Zha, D. et al. *RLCard.* arXiv:1910.04376 — Literature is **not** among its environments.
122. Perolat, J. et al. *Mastering the Game of Stratego* (DeepNash, R-NaD). arXiv:2206.15378 **[U]** ; Bakhtin, A. et al. *CICERO.* Science 378, 2022 **[U]** ; Lanctot, M. et al. *OpenSpiel.* arXiv:1908.09453 — contains `tiny_bridge` and `trade_comm`, recommended as unit tests for the belief/convention machinery.

### 7.11 Search-budget efficiency, engineering, and tuning

123. Danihelka, I., Guez, A., Schrittwieser, J., Silver, D. *Policy Improvement by Planning with Gumbel.* ICLR 2022. https://openreview.net/forum?id=bERaNdoegnO (algorithm also in Danihelka's UCL thesis, ch. 5).
124. Wu, D.J. *Accelerating Self-Play Learning in Go* (KataGo). arXiv:1902.10565 — playout-cap randomisation 1.37×, forced playouts + target pruning, auxiliary targets 1.30–1.65×, global pooling 1.60×, sublinear window; ~50× total.
125. Chaslot, G., Winands, M., van den Herik, H.J. *Parallel Monte-Carlo Tree Search.* CG 2008, LNCS 5131, 60–71. https://dke.maastrichtuniversity.nl/m.winands/documents/multithreadedMCTS2.pdf — 16-thread strength-speedups: root **14.9×**, tree+VL 8.5×, tree+local-mutex 3.3×, leaf 2.4×.
126. Mirsoleimani, S.A., Plaat, A., van den Herik, H.J., Vermaseren, J. *An Analysis of Virtual Loss in Parallel MCTS.* ICAART 2017. https://www.scitepress.org/papers/2017/62058/62058.pdf — **virtual loss degrades lock-free tree parallelism across all `C_p` and 2–64 threads**.
127. Mirsoleimani, S.A., van den Herik, H.J., Plaat, A., Vermaseren, J. *A Lock-free Algorithm for Parallel MCTS.* ICAART 2018. https://liacs.leidenuniv.nl/~plaata1/papers/paper_ICAART18.pdf — the packed `(W,N)` atomic; 18× → 23× → 34×.
128. Liu, A., Chen, J., Yu, M., Zhai, Y., Zhou, X., Liu, J. *Watch the Unobserved* (WU-UCT). ICLR 2020; arXiv:1810.11755.
129. Czech, J., Korus, P., Kersting, K. *Monte-Carlo Graph Search for AlphaZero.* arXiv:2012.11045 — `Q_φ` correction, `Q_ε = 0.01`, 30–70% memory reduction.
130. Childs, B., Brodeur, J., Kocsis, L. *Transpositions and Move Groups in MCTS.* IEEE CIG 2008 **[U]** ; Saffidine, A., Cazenave, T., Méhat, J. *UCD.* KBS 34, 2012 **[U]** ; Enzenberger, M., Müller, M. *A Lock-free Multithreaded MCTS Algorithm.* ACG 12, 2009 **[U]** ; Soejima, Y., Kishimoto, A., Watanabe, O. *Evaluating Root Parallelization in Go.* IEEE TCIAIG 2(4), 2010 **[U]**.
131. Stockfish contributors. *NNUE architecture and quantisation reference.* https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html ; Chess Programming Wiki: *NNUE*, *Zobrist Hashing*, *Shared Hash Table* (Hyatt–Mann lockless XOR) ; Hyatt, R., Cozzie, A. *The Effect of Hash Collisions in a Computer Chess Program.*
132. Kingma, D.P., Ba, J. *Adam.* ICLR 2015; arXiv:1412.6980.
133. Kiessling, T. et al. *Apple vs. Oranges: Evaluating the Apple Silicon M-Series SoCs for HPC.* arXiv:2502.05317 ; *Above the Inner Loop: Exceeding Accelerate at LLM Prefill GEMM on the M1 AMX.* arXiv:2606.25426 ; Apple, *Energy Efficiency Guide for Mac Apps* (QoS classes).
134. Salmon, J., Moraes, M., Dror, R., Shaw, D. *Parallel Random Numbers: As Easy as 1, 2, 3* (Random123). SC11 ; Blackman, D., Vigna, S. *Scrambled Linear Pseudorandom Number Generators.* arXiv:1805.01407 ; https://prng.di.unimi.it/ ; Lemire, D., RNG testing results, 2017.
135. Coulom, R. *CLOP: Confident Local Optimization for Noisy Black-Box Parameter Tuning.* ACG 13, LNCS 7168, 146–157. https://www.remi-coulom.fr/CLOP/CLOP.pdf — **`H = 3`; UH-CMA-ES "does not work well"**.
136. Hansen, N. *The CMA Evolution Strategy: A Tutorial.* arXiv:1604.00772.
137. Chen, Y., Huang, A., Wang, Z., Antonoglou, I., Schrittwieser, J., Silver, D., de Freitas, N. *Bayesian Optimization in AlphaGo.* arXiv:1812.06855 — 50 games/evaluation; 50% → 66.5%.
138. Wu, T.-R., Wei, T.-H., Wu, I-C. *Accelerating and Improving AlphaZero Using Population Based Training.* AAAI 2020; arXiv:2003.06212 ; Jaderberg, M. et al. *Population Based Training of Neural Networks.* arXiv:1711.09846 **[U]**.
139. Spall, J.C. IEEE TAC 37:332–341, 1992 (SPSA) **[U]** ; Kocsis, L., Szepesvári, C. *Universal parameter optimisation in games based on SPSA.* ML 63(3), 2006 **[U]**.
140. Koyamada, S. et al. *Pgx: Hardware-Accelerated Parallel Game Simulators for RL.* NeurIPS 2023 D&B; arXiv:2303.17503 — OpenSpiel 10²–10⁴ steps/s vs 10⁵–10⁶ vectorised.
141. Tian, Y. et al. *ELF OpenGo.* arXiv:1902.04522 **[U]** ; Laurent, J. *AlphaZero.jl parameter reference.*

### 7.12 Evaluation and statistics

142. Zinkevich, M., Bowling, M., Bard, N., Kan, M., Billings, D. *Optimal Unbiased Estimators for Evaluating Agent Performance* (advantage sum / DIVAT). AAAI-06, 573–578. **[U] primary**
143. Bowling, M., Johanson, M., Burch, N., Szafron, D. *Strategy Evaluation in Extensive Games with Importance Sampling.* ICML 2008. https://poker.cs.ualberta.ca/publications/ICML08.pdf — imaginary observations; off-policy s.d. 244,469 → 2,857.
144. White, M., Bowling, M. *Learning a Value Analysis Tool for Agent Evaluation* (MIVAT). IJCAI-09, 1976–1981. https://www.ijcai.org/Proceedings/09/Papers/326.pdf — closed-form `θ*`; **6-player poker only ~18–20%**.
145. Burch, N., Schmid, M., Moravčík, M., Morrill, D., Bowling, M. *AIVAT.* AAAI-18; arXiv:1612.06915 — 68.8% (HUNL) / 99.9% (Leduc, full knowledge).
146. Kim, J., Sandholm, T. *Heuristic Pathologies and Further Variance Reduction via Uncertainty Propagation in the AIVAT Family.* arXiv:2605.14261 — **the p-hacking demonstration; IVW 24.5% SE reduction**.
147. Li, B., Chen, Y., Huang, L. *AV-AIVAT: 74× Cheaper Agent Evaluation with Certified Anytime-Valid Stopping.* arXiv:2608.06362 — predictable value functions; EB-CS and AsympCS; **61.35% false-positive rate under naive continuous monitoring**.
148. Davidson, J., Archibald, C., Bowling, M. *Baseline: Practical Control Variates for Agent Evaluation in Zero-Sum Domains.* AAMAS 2013. **[U]**
149. Lisý, V., Bowling, M. *Equilibrium Approximation Quality of Current No-Limit Poker Bots* (LBR). arXiv:1612.07547 — pseudocode; all ACPC 2016 bots >3,180 mBB/h exploitable; **LBR −536 mbb/g against a bot whose true exploitability was 90**.
150. Johanson, M., Waugh, K., Bowling, M., Zinkevich, M. *Accelerating Best Response Calculation in Large Extensive Games.* IJCAI-11, 258–265. **[U]**
151. Waugh, K., Schnizlein, D., Bowling, M., Szafron, D. *Abstraction Pathologies in Extensive Games.* AAMAS 2009, 781–788. https://bowlingmh.github.io/papers/09aamas-abstraction.pdf — **refining an abstraction does not monotonically reduce exploitability**.
152. Moravčík, M. et al. *DeepStack.* Science 356(6337), 2017 — AIVAT 85% s.d. reduction; significance at 3,000 hands ; Brown, N., Sandholm, T. *Libratus.* Science 359(6374), 2018 **[U]** ; *Pluribus.* Science 365(6456), 2019 **[U] secondary**.
153. Coulom, R. *Whole-History Rating.* CG 2008. https://www.remi-coulom.fr/WHR/WHR.pdf — 55.793% vs Elo's 55.121% on 2.33M KGS games.
154. Herbrich, R., Minka, T., Graepel, T. *TrueSkill.* NeurIPS 19, 2006 — team model `t_j = Σ_i p_i` (formulas **[U]**, re-verify) ; Glickman, M. *Glicko / Glicko-2* **[U]**.
155. Balduzzi, D., Tuyls, K., Pérolat, J., Graepel, T. *Re-evaluating Evaluation.* NeurIPS 2018; arXiv:1806.02643 — maxent-Nash averaging; **Elo and uniform averaging fail redundancy-invariance**; mElo₂.
156. Omidshafiei, S. et al. *α-Rank: Multi-Agent Evaluation by Evolution.* Sci. Rep. 9:9937, 2019 ; Rowland, M. et al. *Multiagent Evaluation under Incomplete Information* (ResponseGraphUCB). NeurIPS 2019; arXiv:1909.09849.
157. Czarnecki, W. et al. *Real World Games Look Like Spinning Tops.* NeurIPS 2020 **[U]** ; Lanctot, M. et al. *Evaluating Agents using Social Choice Theory.* arXiv:2312.03121 **[U]** ; *Soft Condorcet Optimization.* AAMAS 2025 **[U]**.
158. Wellman, M., Tuyls, K., Greenwald, A. *Empirical Game Theoretic Analysis: A Survey.* JAIR 82:1017–1076, 2025; arXiv:2403.04018 — Hoeffding/Bennett sample bounds; bootstrap regret; spurious equilibria.
159. Jordan, S., Chandak, Y., Cohen, D., Zhang, M., Thomas, P. *Evaluating the Performance of Reinforcement Learning Algorithms.* ICML 2020, PMLR 119:4962–4973 — **percentile bootstrap 5.7–11.2% failure at nominal 5%**; `δ' = δ/(|A||M|)`.
160. Agarwal, R. et al. *Deep RL at the Edge of the Statistical Precipice.* NeurIPS 2021 **[U]**.
161. Van den Bergh, M. *Comments on Normalized Elo.* Fishtest. https://cantate.be/Fishtest/normalized_elo_practical.pdf — `T = D/(Δe_n)²`, `D = 1,046,535` ; Stockfish, *Statistical Methods and Algorithms in Fishtest* ; Wald, A. *Sequential Tests of Statistical Hypotheses.* AMS 16(2), 1945 **[U]**.
162. Annual Computer Poker Competition, *Rules* — duplicate matches, common seeds, bootstrapping; duplicate s.d. ≈ 2/3 of plain **[partially verified]** ; Bard, N., Hawkin, J., Rubin, J., Zinkevich, M. *The Annual Computer Poker Competition.* AI Magazine 34(2), 2013 **[U]** ; Wikipedia, *International Match Points*, *Duplicate poker*.
163. Wolski, M., Hoernle, N., Forkel, J., Foerster, J. *Is Inter-Seed Cross-Play Enough?* arXiv:2608.03644 — 176 policies / 22 implementations / 8 seeds; no significant within- vs across-implementation gap for OP+IPPO in Yokai (one environment, one algorithm).
164. Vinyals, O. et al. *Grandmaster level in StarCraft II.* Nature 575, 2019 **[U]** — league + Nash-of-the-league + held-out validation agents.
165. Dehpanah, A., Ghori, M., Gemmell, J., Mobasher, B. *The Evaluation of Rating Systems in Team-based Battle Royale Games.* arXiv:2105.14069 ; Efron & Tibshirani (1993); Davison & Hinkley (1997); Wiedenbeck et al. (2014) **[U]**.

---

## Appendix A — Formula card

| Quantity | Formula |
|---|---|
| Total deals | `54!/(9!)^6 ≈ 1.011e38` (126.25 bits) |
| Info set, one seat | `45!/(9!)^5 ≈ 1.901e28` (93.94 bits) |
| Info set, whole team pooled | `27!/(9!)^3 ≈ 2.279e11` |
| Legal asks | mean 84 (median 81, range 27–135); ≈6.39 raw bits/ask |
| Declaration space | `9 × 3^6 = 6,561`; collapse to 9 info-set-level moves |
| Belief DP work | `O(k ∏_q(c_q+1))` = 5e5 (seat) / 6e6 (observer) |
| Marginals | `μ_{i,q} = Z^{-1} Σ_n F_{i-1}(n) w_{i,q} B_i(n+e_q)` |
| Block DP | `F_b(n) = Σ_t g_b(t) F_{b-1}(n−t)`; measured supports 15–33 |
| Exact sampler | `Pr[q] ∝ w_{i,q} B_i(n+e_q)` — rejection-free |
| ESS law | `ESS/N ≈ e^{−σ²}`; budget `σ ≤ 0.8` nats |
| Declaration EV | `2q(A*) − 1`; `C_steal = 0` when team provably holds all six |
| Develin's endgame | asking beats a coin-flip declare only if `q' > 3/2` — impossible |
| Ask-the-asker floor | `μ_{j,c} ≥ 1/u_{j,H}` for `c ∈ H`, `j` has asked in `H` |
| Lockout cost | `(1 − μ_{j,c})·D_j`; safe set `{a : (1−μ)D_j ≤ τ}` |
| Availability UCB | `X̄(v) + c√(ln n'(v)/n(v))`, `c = 0.7` |
| Gumbel σ | `(c_visit + max_b N(b))·c_scale·q̂`, `c_visit = 50`, `c_scale = 1` |
| Gumbel target | `π' = softmax(logits + σ(completedQ))` |
| Smooth-UCT | `η_k = max{γ, η(1+d√N_k)^{-1}}`, `γ=0.1, η=0.9` |
| MDS | `π_search ∝ π_b exp(η q̂)` |
| DCFR | `α=3/2, β=0, γ=2` |
| Disambiguation | `df = 2(1−ρ)/(2−ρ)`; Skat/Hearts `df≈0.6 ⇒ ρ≈0.571` |
| Duplicate estimator | `Y(d) = ⅓Σ_{k even} X(d,k) − ⅓Σ_{k odd} X(d,k)` |
| MIVAT | `θ* = [Σ(A_t−Ā)(A_t−Ā)ᵀ]⁻¹[Σ(A_t−Ā)(u_t−ū)]`; `Var ↓ by 1−Corr²` |
| GSPRT | `e_n = 347.43·(μ−½)/σ_pg`; `T ≈ 10⁶/(Δe_n)²`; LLR ±2.94 |
| Convention bits | `D_KL(β_t ‖ β̃_t)` |
| Signalling value | `Δ_sig(a) = V(β^{+a}) − V(β̃^{+a})` |
| Eavesdropping tax | `Δ_eaves = V^conv_priv − V^conv_pub ≥ 0` |
| Public-channel separation | `v_i + w_i ≥ 0` for every state (Farrell–Gibbons) |
| Secrecy rate | `ρ = Σ_{j∈T} I(X;A|h,x_j) − Σ_{k∈O} I(X;A|h,x_k)`; `≤ 0` for absolute codebooks |

## Appendix B — Build order

| Phase | Deliverable | Gate |
|---|---|---|
| **0** | Rules fork decided (conventions on/off), Forced Claims terminator, per-seat RNG streams, 6-rotation harness, GSPRT | `E[Y] = 0` self-vs-self; sets sum to 9 |
| **1** | Exact block-DP belief + declaration module | Machine-precision agreement with brute force on 3-player/9-card; declaration reliability diagram calibrated |
| **2** | BCD measurement; PIMC and rule-based sparring baselines | `(lc, b, df)` reported, split ask/declare |
| **3** | Info-set tree + availability UCB + Gumbel root + declare-as-info-set-move | Beats PIMC baseline by >30 nElo under GSPRT |
| **4** | NNUE + AlphaZero self-play loop with ownership head, SAD, correlation seed | Training curve on WHR; ownership head accuracy tracked |
| **5** | RIS re-determinization + self-determinization | `Δ_sig` becomes positive and non-trivial |
| **6** | Cross-play, LBR-team, FXP counter-population | SP−XP gap reported; LBR lower bound reported |
| **7** | Endgame TB-DAG + DCFR; MDS test-time search; OBL ladder | Agreement with exact endgame TMECor |
| **8** | Research questions Q1–Q12 | Paper |
