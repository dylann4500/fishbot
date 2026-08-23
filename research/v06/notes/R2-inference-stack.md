# R2 — Complete anatomy of the FishLab inference stack

Recon only. Repo `/Users/dylan/Documents/GitHub/fish optimization`, commit `bd812fe` ("v0.5"), clean tree.
Sources read in full: `engine/src/belief.hpp` (703 lines), `engine/src/blockdp.hpp` (495), `engine/src/oracle.hpp` (289);
plus the consuming policy `engine/src/v05.hpp` and `engine/src/factory.hpp:36-127`.

All numbers below are measured on this machine (Mac17,9, 15 cores, clang++ `-O3 -march=native`).
Wall-clock was unusable (load average 14 during the session), so **every timing is user CPU time**,
and every match timing is `--threads=1`. Two independent samples are given where they differ; treat
the spread (±20%) as the error bar. Harnesses live in the scratchpad, listed in §9.

---

## 1. `Knowledge` — the hard deduction state

`struct Knowledge`, `belief.hpp:46-227`. One instance per agent, owned by `Agent::k` (`game.hpp:14`),
fed by `Agent::observe → k.onEvent` (`game.hpp:20`). It is the only state any policy has.

### 1.1 Fields and what each is

| field | line | meaning |
|---|---|---|
| `owner[NCARD]` | 49 | seat, `OUT_OF_PLAY` (254), or `UNKNOWN` (255) |
| `mask[NCARD]` | 50 | 6-bit set of still-possible owners for an `UNKNOWN` card |
| `unresolved` | 51 | bitboard of active-set cards with `UNKNOWN` owner |
| `handCount[NPLAY]` | 52 | public hand sizes, overwritten wholesale from `e.handCount` each event (`:206`) |
| `askCount[p][S]` | 54 | public tally: times seat `p` asked in half-suit `S` |
| `missCount[p][S]` | 55 | times seat `p` was the target of a **failed** ask in `S` |
| `totalAsks[p]` | 56 | total asks by seat `p` |
| `publicKnown` | 57 | cards whose location every seat has seen (set only on a successful ask, `:179`) |
| `disj` | 58 | live (C5) certificates: `{player, cards}` = "player holds ≥1 of cards" |

### 1.2 The five constraints, and which are proved

The header comment (`belief.hpp:3-30`) states the structural theorem the whole stack rests on: **every
card movement in Fish is public**, so an unresolved card has never moved and still sits with the seat
that was dealt it. The hidden state is therefore exactly the initial deal.

| | constraint | where enforced | status |
|---|---|---|---|
| **C1** | own hand | `init` `belief.hpp:73-74` | **proved** (the agent is handed its hand) |
| **C2** | publicly transferred cards | `onEvent` `:176-179`, `:187-189` | **proved** by the rules |
| **C3** | exclusions: a miss proves the target lacks the card; an ask proves the *asker* lacks it | `exclude` `:147-151`, called at `:167` (asker) and `:182` (miss target) | **proved** |
| **C4** | capacity: seat `p` holds exactly `handCount[p] − |known cards of p|` unresolved cards | `capacities` `:83-90`, propagated to a fixed point by `propagateCapacity` `:214-226` | **proved** |
| **C5** | ask legality: an ask in `S` by `A` proves `A` held ≥1 *other* card of `S` — and because unresolved cards never move, this is time-independent | built at `:158-171`, discharged by `resolveDisjunctions` `:129-138` | **proved**, but see §1.4 |

Everything above is a hard deduction. The **only assumed** object in the deduction state is
`priorWeight` (`belief.hpp:100-108`), the soft policy prior — see §4. `BlockDP`/`DealDP` additionally
assume a **uniform prior over deals consistent with C1–C5**; that assumption is the load-bearing one
and §3.4 shows it is empirically wrong.

### 1.3 What `onEvent` writes, in order (`belief.hpp:153-210`)

On `Kind::Ask` (`:154-183`):
1. `askCount[actor][S]++`, `totalAsks[actor]++` (`:156-157`), saturating at 255/65535.
2. Scan the other five cards of `S`: if the asker is already known to hold one, the certificate is
   **vacuous** and dropped; otherwise collect the candidates `D` (`:160-166`).
3. `exclude(e.card, e.actor)` — C3 for the asker (`:167`).
4. If `|D| == 1`, `setOwner` immediately; else push a `Disjunction` (`:168-171`).
5. On a hit: `setOwner(card, target)` **first** — deliberately revealing the *pre*-transfer holder so
   any outstanding certificate referring to the target is discharged correctly — then overwrite to
   the actor and set `publicKnown` (`:172-179`). This ordering is the subtle correctness point of
   the whole file.
6. On a miss: `missCount[target][S]++` and `exclude(card, target)` (`:180-182`).

On `Kind::Declare`/`ForcedDeclare` (`:184-205`): a correct declaration reveals every unresolved card's
owner (`:187-190`); the half-suit then leaves play (`owner = OUT_OF_PLAY`, `setActive[S] = false`),
every certificate touching it is dropped (`:200-203`), and `myHand` is cleared of it (`:204`).
**Residual count information from a declaration is absorbed only through the public `handCount`**
(`:192-193` comment, applied at `:206`) — an incorrect declaration reveals nothing about the actual
holders, which is right.

Always: `handCount` refreshed from the event (`:206`), own-hand bookkeeping (`:207-208`),
`propagateCapacity()` (`:209`).

### 1.4 Facts about C5 worth carrying into v0.6

* `askCount`, `totalAsks`, `publicKnown` all have readers. **`missCount[p][S]` is written at
  `belief.hpp:181` and read by nothing** (grep over `src/` excluding probes). It is a free
  per-(seat, half-suit) hard-evidence channel sitting unused. (R5 §M7 also flags this.)
* Structural census over 361 real v0.5 self-play belief states (`bench_belief`): mean
  `|unresolved|` = **25.28**, max **45**; mean live certificates per state = **4.28** (every
  certificate carried was live); mean `DealDP` state count **21,990**; `DealDP.uniform` in **7.8%**
  of states; mean `BlockDP` groups **5.65**. At most 16 certificates per half-suit are carried into
  `BlockDP` (`blockdp.hpp:211-216`) and the excess is silently dropped — the cap does not bind today.
* **C5 is a massive under-use of the evidence in an ask.** The certificate says "≥1 other card".
  Measured over 40 deals per table (seed 5150, `bench7`), the number of *other* cards of `S` the
  asker actually held at the moment of the ask:

  | table opponent | asks | k=1 | k=2 | k=3 | k=4 | k=5 | mean | P(k≥2) |
  |---|---:|---:|---:|---:|---:|---:|---:|---:|
  | v05 | 1749 | 28.1% | 32.9% | 22.1% | 12.0% | 4.9% | **2.326** | **71.9%** |
  | v04 | 1852 | 27.4% | 33.2% | 22.5% | 11.8% | 5.2% | 2.342 | 72.6% |
  | v03 | 1764 | 26.1% | 31.3% | 21.5% | 14.2% | 6.9% | 2.444 | 73.9% |
  | lockout | 1834 | 26.1% | 32.1% | 22.2% | 13.9% | 5.7% | 2.409 | 73.9% |
  | detective | 1750 | 26.8% | 32.7% | 21.5% | 13.6% | 5.4% | 2.381 | 73.2% |
  | hunter | 1480 | 23.2% | 39.4% | 24.5% | 11.9% | 1.0% | 2.280 | 76.8% |
  | **random** | 1035 | **59.3%** | 31.0% | 9.1% | 0.5% | 0.1% | **1.510** | **40.7%** |

  Against every real policy the asker holds ~2.3–2.4 others; the hard certificate keeps only the
  "≥1" bit. Against `random` the mean drops to 1.51 — i.e. **the discarded information is exactly the
  opponent-model signal**, and it vanishes when the opponent has no model. This single table explains
  §3.4 and is the strongest v0.6 lead in this report.

---

## 2. `DealDP` — exact C1–C4 counting (`belief.hpp:235-445`)

**Quantity.** The uniform posterior over assignments of the `Q = |unresolved|` cards to seats subject
to support (C1–C3) and capacities (C4). Certificates (C5) are **not** in it.

**State space.** Mixed-radix over the remaining-capacity vector, `total = Π_p (q_p + 1)`
(`:274-279`). Because the observer's own capacity is identically 0, only five owners vary, and
`Σ q_p = Q ≤ 45`, so `total ≤ 10^5` by AM–GM. `DP_STATE_CAP = 400000` (`belief.hpp:38`) is therefore
never hit: **0 build failures in 361 + 6,000 states measured.** Measured mean `total` = **21,990**.

**Algorithm.** Forward pull `F[v] = Σ_p F[v + e_p]` (`:301-309`), backward pull `B[v] = Σ_p B[v − e_p]`
(`:312-320`), `N = F[0]` is the count of consistent deals. The "layer = remaining-capacity sum"
identity makes both single flat arrays. Cost `O(total · 6)`.

**Fast path.** `uniform` (`:271`) fires when every unresolved card shares one mask; marginals are then
`q_p / Q` in closed form (`:326-330`) and `countWithMasks` becomes a falling factorial (`:392-428`).
Measured incidence: **6.4–7.8%** of states in ordinary play, but **46.3%** of the certificate-free
states.

**Exposed.** `marginals` (`:325`), an exact whole-deal `sample` (`:345`), `countMasked` (`:374`),
`countWithMasks` (`:391`), `countWith` (`:440`).

**Latent trap (see §8-R1).** In the `uniform` branch, when the requested masks are neither all equal
nor singletons, `countWithMasks` falls into a **greedy sequential approximation** (`:424-426`:
"pick the seat with most remaining capacity"). It is documented as exact. No current caller reaches
it — `evaluateSet` passes `allow[i] = teamMask` for every card (all-equal branch, `v05.hpp:766`) and
`countWith` passes singletons (`:441-442`) — and I verified both live paths agree with the exact block
posterior to **5.6e-16 / 1.7e-15 max** on 673 + 1,633 certificate-free queries (`bench8`). A v0.6
caller asking a heterogeneous per-card question ("is each of these cards on my team?") would get a
silent approximation.

---

## 3. The seven `BeliefMode`s

`enum class BeliefMode : int { Exact=0, ExactDisj=1, Sinkhorn=2, Independent=3, Hybrid=4, Fast=5, Block=6 }`
— `v04.hpp:40`, parsed at `factory.hpp:41-50` / `:131-139`. Default `BeliefMode::Fast` (`v05.hpp:27`).
Dispatch: `V05Agent::refresh` `v05.hpp:162-198` (per-event marginals) and
`V05Agent::evaluateSet` `v05.hpp:703-770` (declaration queries).

### 3.1 What each computes

| mode | per-event marginals (`refresh`) | declaration query (`evaluateSet`) |
|---|---|---|
| **Independent** | `1/|mask[c]|` on the support; C4 and C5 dropped (`v05.hpp:176-183`) | `pTeam = pAlloc = cheap` (independent product), `v05.hpp:764-767` falls through `dpOk == false` |
| **Sinkhorn** | `Belief::sinkhorn` — 40 IPF sweeps to row sums 1 / column sums `q_p`; no prior, no certificates (`belief.hpp:592-609`) | same `cheap` fallback |
| **Fast** *(shipped)* | `Belief::sinkhornDisj(4, 8, θ, φ)` — IPF **plus** independence-conditioning on every certificate **plus** the policy prior (`belief.hpp:478-529`) | M2 `feasibleAllocation` (`v05.hpp:619-679`) + `jointSequential` (`belief.hpp:535-553`) |
| **Exact** | `Belief::compute(…, particles=96, useDisj=false)` — exact `DealDP` marginals + 96 unfiltered deal samples (`belief.hpp:638-674`) | `ensureDP`, then `pTeam = dp.countWithMasks/N` (exact C1–C4), `pAlloc = dp.countWith/N` |
| **ExactDisj** | `compute(…, useDisj=true)` — same DP, but samples are **rejection-filtered** on C5 (`belief.hpp:658`) and marginals recomputed from the accepted particles when ≥8 survive (`:663-673`) | as Exact, but `jointProbability` uses the particle frequency (`belief.hpp:689-697`) |
| **Hybrid** | plain `sinkhorn` (identical to Sinkhorn) — `v05.hpp:190-193` | exact `DealDP` built lazily on the first declaration query (`ensureDP`, `v05.hpp:764`) |
| **Block** | `BlockDP::build` + `marginals` — **the exact C1–C5 posterior**, no sampling; falls back to `sinkhornDisj` only if `build` fails (`v05.hpp:166-174`) | `block.teamOwnsProbability` + `block.bestTeamAllocation` (`v05.hpp:727-733`) |

Note `Hybrid` never uses the certificates for its ask marginals and never uses the policy prior at
all; `Block` **ignores `priorTheta`/`priorPhi` entirely** unless `build` fails.

### 3.2 Measured cost

**Kernel cost, per observer per belief refresh** (361 real v0.5 self-play states, 5–20 repetitions,
`bench_belief` / `bench3`; the two harnesses agree to ~25%):

| kernel | µs/state |
|---|---:|
| Independent (mask-uniform fill) | **0.29** |
| `sinkhornDisj(4, 8)` + prior — **the shipped path** | **3.9 – 4.2** |
| `sinkhorn` (40 IPF sweeps) | 5.0 |
| `Knowledge` copy + `propagateCapacity` (one lookahead branch) | 0.08 |
| `jointSequential` over one half-suit (6 cards) | 15.7 |
| `DealDP::build` | 230 – 268 |
| `DealDP::build + marginals` | 282 – 340 |
| `BlockDP::build` | 229 – 336 |
| `BlockDP::build + marginals` | **441 – 594** |
| `BlockDP` + all group tables (joint allocation laws) | 1,183 – 1,433 |
| `BlockDP` + group tables + `P(team owns)` × 9 sets × 2 teams | 1,180 – 1,544 |
| `Belief::compute(particles=96, disj=0)` | 533 |
| `Belief::compute(particles=96, disj=1)` | 769 |
| `BlockDP` × 2 = exact posterior after one hypothetical hit **and** miss | **1,255** |
| `sinkhornDisj` × 2 = same lookahead, Fast | **8.2** |

**End-to-end, `./fish match --a=v05:belief=M --b=v05:belief=M --threads=1 --seed=31`**, user CPU
normalised by (games × events/game); two samples (20 deals, 10 deals):

| mode | events/game | µs per public event | vs Fast |
|---|---:|---:|---:|
| Independent | 132 | **59 / 61** | 0.15× |
| Sinkhorn | 121 / 124 | **81 / 85** | 0.21× |
| **Fast (shipped)** | 98 – 99 | **393 / 418** | 1.00× |
| Block | 115 – 116 | **3,860 / 5,093** | ~11× |
| Hybrid | 137 / 140 | **4,149 / 4,175** | ~10× |
| Exact | 136 / 138 | **4,866 / 5,327** | ~13× |
| ExactDisj | 119 / 131 | **5,287 / 6,169** | ~14× |

Cost decomposition of the Fast path (`--games=20 --threads=1 --seed=31`, user seconds):
`v05` 1.56 · `topk=1` 1.38 · `chain=0,threat=0` 1.35 · `topk=1,declare=0` 0.11. So the two-ply
refinement is only ~13% of the cost and **the declaration path dominates** — six `proposeDeclaration`
polls per public event (`game.hpp:202-229`, the seat loop at `:211-223`), each running `feasibleAllocation` + `jointSequential`
per gate-passing half-suit. The Independent↔Sinkhorn delta (22 µs/event for a 4.7 µs/call kernel
difference) implies **≈ 5 belief refreshes per public event**.

### 3.3 Measured accuracy against the exact posterior

Reference = `BlockDP` marginals (C1–C5 exact). 361 states, 9,127 unresolved cards, `bench_belief`:

| mode | max abs Δ | mean abs Δ |
|---|---:|---:|
| Independent | 8.00e-01 | 4.05e-02 |
| Sinkhorn | 8.82e-01 | 2.366e-02 |
| Exact `DealDP` (C1–C4) | 8.83e-01 | 2.365e-02 |
| Fast, prior off | 4.36e-01 | 1.89e-02 |
| Fast, shipped prior | 5.92e-01 | 2.57e-02 |
| ExactDisj, 96 particles | 1.91e-01 | 2.43e-02 |

Two things fall out. **(a) Sinkhorn is an essentially perfect solver for the C1–C4 posterior**:
against `DealDP` directly, max 2.96e-02 / **mean 1.63e-04** over 300 states (`bench2`). Its 2.37e-02
mean error versus Block is *entirely* the missing certificates. **(b) The certificates are the whole
gap** — Sinkhorn and the exact C1–C4 DP are indistinguishable at 2.37e-02, and closing C5 is what
buys accuracy, not exactness in the counting.

### 3.4 Which mode ships, and the reason the exact one is not it

`V05Config::belief = BeliefMode::Fast` (`v05.hpp:27`); nothing in `experiments_v05.sh` or
`freeze_config_v05.py` overrides it. Prior work (R5 §b.2, v0.4 E5 ablation, 1,500 games) measures
`belief=block` at **−6.20 points [+4.35, +8.05 for Fast]** and ~14× slower, with the acknowledged
confound that the ask weights were fitted under Fast and no matched-budget refit was ever run.

**This report supplies the mechanism, and it is not (only) the fitting confound.** Scoring each
posterior against the *ground-truth deal* on the unresolved cards (`bench4`, 900 states; `bench6`,
500 states per table, seed 909):

| observer = v05, table | Block (C1–C5 **exact**) NLL | Fast prior-off NLL | Fast shipped NLL | Block × prior tilt |
|---|---:|---:|---:|---:|
| vs v05 | 1.4245 | 1.3936 | **1.3817** | 1.3899 |
| vs v04 | 1.4017 | 1.3732 | **1.3646** | 1.3725 |
| vs v03 | 1.4158 | 1.3892 | **1.3836** | 1.3954 |
| vs lockout | 1.3750 | 1.3509 | **1.3414** | 1.3457 |
| vs detective | 1.3831 | 1.3525 | **1.3409** | 1.3519 |
| vs **random** | **1.3393** | 1.3426 | 1.3379 | **1.3314** |

Same ordering in Brier and top-1 (`bench4`, 7,513 cards: Block 0.7188 / 35.39%, Fast shipped
0.6979 / **35.91%**). **The approximate posterior is a better predictor of the truth than the exact
one against every modelled opponent, and the advantage disappears exactly against `random`.**

The reason is that "exact" means *exact under a uniform prior over deals consistent with C1–C5*. The
public record carries soft policy information that C1–C5 discards (§1.4's table), and two things in
the Fast path accidentally recover part of it: the `priorWeight` tilt, and — larger — the
independence conditioning on certificates (`belief.hpp:500-527`), which over-sharpens the asker's
ownership inside the half-suit in exactly the direction the count law of §1.4 says is right. Fast
with the prior **off** already beats Block; against `random` it loses to Block, which is the control.

**Consequence for v0.6:** "use the exact engine" is not an upgrade by itself; a policy-aware
likelihood is. See §6-O1.

---

## 4. The Sinkhorn disjunction filter, the policy prior, and defect K

### 4.1 The kernel (`Belief::sinkhornDisj`, `belief.hpp:478-529`)

```
init   M[c][p] = mask[c][p] ? priorWeight(c, p, θ, φ) : 0                     :484-485
outer × 4:
   inner × 8:   row-normalise each card to 1                                   :488-490
                column-scale seat p to capacity q_p                            :491-494
   row-normalise                                                               :496-498
   if last outer: break                                                        :499
   for each certificate (A, D):  independence conditioning                     :500-527
       P(owner(d)=A | E) = p_{d,A} / (1 − Π_{e∈D}(1 − p_{e,A}))
       P(owner(d)=p | E) = p_{d,p} · (1 − Π_{e∈D\{d}}(1 − p_{e,A})) / (1 − P(no A))
```
with two guards: degenerate `pAny < 1e-9` forces the mass in (`:510-514`), and `pNone < 1e-12`
short-circuits (`:515`). Cost `O(outer · inner · |U| · 6)` = 3.9–4.2 µs measured.

### 4.2 The prior (`Knowledge::priorWeight`, `belief.hpp:100-108`)

```cpp
double a     = askCount[p][S];
double other = totalAsks[p] - a;
double z     = theta * a - phi * other;
if (z > 2.6) z = 2.6; else if (z < -2.6) z = -2.6;
return std::exp(z);
```
Shipped values `priorTheta = 0.44458`, `priorPhi = 0.12198` (`v05.hpp:29-30`). It enters at exactly
one place, the `sinkhornDisj` initialiser (`belief.hpp:485`), and nowhere else in the stack.

### 4.3 Defect K: **CONFIRMED**, with one qualification

The claim under test (`research/v05/DESIGN.md:88`): *"`priorPhi` is not an independent channel: the
exponent rearranges so its second term is card-independent and Sinkhorn's capacity normalisation
removes it."*

**Algebra, from the code.** With `T_p = totalAsks[p]` and `a_{p,S} = askCount[p][S]`:

```
z(c,p) = θ·a_{p,S(c)} − φ·(T_p − a_{p,S(c)})
       = (θ+φ)·a_{p,S(c)}  −  φ·T_p
exp z  = A[c][p] · g[p],   A[c][p] = exp((θ+φ)·a_{p,S(c)}),   g[p] = exp(−φ·T_p)
```

`g[p]` depends on the **seat alone**. `sinkhornDisj`'s two operations are row scaling (`:488-490`)
and column scaling (`:491-494`), so every iterate stays inside the scaling class
`{diag(u)·M·diag(v)}`, and `M = A·diag(g)` has the same class as `A`. Explicitly, one full sweep
kills `g`: after row-normalisation `M₁ = diag(r)·A·diag(g)` with `r_c = 1/Σ_p A[c][p]g[p]`; the
column sums are `g_p·Σ_c r_c A[c][p]`, so the scale `q_p / colsum_p` cancels `g_p` and
`M₂ = diag(r)·A·diag(q_p / Σ_c r_c A[c][p])` — no `g`. The next row-normalisation erases `r` too.
The certificate step (`:500-527`) acts on already row-normalised values, so it inherits the property.
**φ enters the fixed point only through θ_eff = θ + φ.**

**Verification** — call the shipped `Belief::sinkhornDisj` with `(θ, φ)` and with `(θ+φ, 0)` on 900
real v0.5 belief states (`bench4`, 30 deals, seed 31, every 2nd event):

| configuration | max abs Δ marginal | mean abs Δ |
|---|---:|---:|
| shipped, outer=4 inner=8, **all** states | 1.018e-01 | 1.721e-04 |
| shipped, outer=4 inner=8, states where **neither** side clips | **1.291e-04** | **1.236e-08** |
| shipped, outer=1 inner=8, neither clips | 8.829e-03 | 5.281e-07 |
| shipped, outer=1 inner=64, neither clips | 2.120e-04 | 1.003e-08 |
| θ/10, φ/10 (clip unreachable), outer=1 inner=8 | 2.557e-04 | 1.396e-08 |
| θ/10, φ/10, outer=1 inner=64 | 4.956e-06 | 2.166e-10 |
| θ/10, φ/10, outer=4 inner=64 | **7.732e-08** | **3.299e-12** |
| *control:* `(θ,φ)` vs `(θ,0)` — a real change of θ_eff | 8.504e-02 | 2.208e-03 |
| *control:* `(θ,φ)` vs `(0,0)` | 3.402e-01 | 9.113e-03 |

The residual falls to machine precision as IPF converges. Against the control — deleting φ *without*
folding it into θ moves the marginals by mean 2.2e-03, ~180× the folded residual — the claim is not a
small-effect artefact.

**Qualification (the one thing the claim omits).** The **±2.6 clip is applied to the full exponent**
(`belief.hpp:106`), and `min(e^{2.6}, A·g)` does not factor. The clip binds in **74 / 900** states
under the shipped `(0.44458, 0.12198)` but **203 / 900** under the equivalent `(0.56656, 0)` —
because folding φ raises the exponent's positive part. That asymmetry is the entire residual
(1.7e-04 mean over all states vs 1.2e-08 on clip-free states). So φ is not *bit*-identical to 0, but
its only surviving channel is a saturation artefact of a numerical guard, not a modelled signal.

**Policy level** (`./fish match --games=40 --threads=4 --seed=77`):

| arms | win rate | events/game | ask accuracy |
|---|---|---|---|
| `v05` vs `v05` | 50.00% [39.3, 60.7] | 95.775 | 55.31 / 55.31 |
| `v05` vs `v05:ptheta=0.56656,pphi=0` | 50.00% [39.3, 60.7] | 95.788 | 55.98 / 56.92 |
| `v05` vs `v05:pphi=0` (θ_eff genuinely cut) | 51.25% [40.5, 61.9] | 97.063 | 55.10 / 55.38 |

**Net:** the v0.5 ask prior has **one** free parameter, `θ_eff = θ + φ = 0.56656`, not two. (v0.4's
was 0.39660.) This replicates `research/v05/results/M7-design.md` §1 on v0.5's parameters and adds
the clip-incidence asymmetry. One discrepancy worth flagging: M7-design reports a clip-off
shipped-iteration residual of max 8.60e-01 / mean 1.39e-03 from an out-of-tree replica on v0.4
mirror states; calling the shipped function directly on v0.5 states I get max 1.29e-04 / mean
1.24e-08 at the same iteration counts. Different state distributions (v0.4 deadlocks, so its
`askCount`s are far larger) is the likely cause, but it is unresolved.

---

## 5. `BlockDP` — the exact C1–C5 posterior (`blockdp.hpp:66-493`)

**Exact quantity.** The uniform distribution over deals satisfying C1–C5, and four functionals of it
(`blockdp.hpp:19-24`): `Z`, `mu[c][p]`, `P(allocation A) = S_{t(A)} / Z`,
`P(team owns half-suit) = Σ_{t on team} g(t)·S_t / Z`.

**Key structural fact** (`blockdp.hpp:4-13`): every certificate constrains only cards of one
half-suit, so no certificate couples two half-suits. Unresolved cards are grouped by half-suit:

* **CARD group** (no live certificate): expanded one card at a time, branching ≤ 5.
* **BLOCK group** (≥1 live certificate): the group's ≤ 6 cards are enumerated exhaustively
  (`enumerateGroup`, `:112-155`), filtered against its certificates exactly (`:133-138`), and
  aggregated into a **count-vector table** `(tpack, toff, g, gcard)`.

Because each group consumes a known number of cards, the "remaining capacity sum = layer" identity
survives and forward/backward stay flat arrays.

**State space and cost.** `total = Π_p (q_p + 1) ≤ 10^5` as in §2; `BLOCK_STATE_CAP = 200000`
(`:34`). SWAR lane comparison `geLanes` (`:104-106`) does the six-way capacity test in one 64-bit op.
Forward `:251-270`, `Z = F[0]` `:271`, backward `:273-293`. Measured: **229–336 µs build**,
**441–594 µs build+marginals**, **1,183–1,433 µs with every group's count-vector table**.
Mean 5.65 groups/state; 56.5 count-vector entries at build (block groups only), ~481 after
`ensureGroupTable` on all groups (≈85/group). Measured build failures: **0 / 6,361 states.**

**Where it is affordable.** At 11–13× Fast's per-event cost (§3.2) it is not affordable as the
per-event ask posterior at v0.5's throughput (R5 §b.3 E9 reports 286 games/s multi-threaded → ~22 games/s at 13×). It **is** affordable:
(a) on the declaration path only, which is what `Hybrid` does for `DealDP`; (b) once per turn rather
than per event; (c) at ~1.3 ms for a single hypothetical hit+miss pair, i.e. ~55 ms to score all 44
live asks exactly — too slow for the inner loop, fine for offline distillation or a top-K rescore of
2–4 candidates (2.5–5 ms/turn).

**Validation.** `engine/src/oracle.hpp` brute-forces the posterior with no DP, no factorisation and
no sampling (`BruteForce::enumerate`, `:80-135`) and compares all four quantities plus the sampler
(`oracleCheckState`, `:139-287`). Run at commit `bd812fe`:

```
$ ./fish oracle --games=20 --a=v05 --b=v03 --seed=20260822 --samples=1200
states enumerated 1862 (with a live C5 certificate: 1791);  skipped (too large) 9592
Z max rel diff 0.000e+00 | marginals 0.000e+00 (78,516 checks) | team-ownership 0.000e+00 (3,185)
named allocation 0.000e+00 (381,888) | equal-prob corollary 0.000e+00
bestTeamAllocation 1969 checks, 0 inconsistent, 0 not argmax
sampler vs exact 0.0575 over 2,195,408 draws          ORACLE PASS
```
Coverage caveat is in the tool's own output: **16.3%** of states are small enough to enumerate.

---

## 6. The menu — exact quantities available cheaply that the policy does **not** consume

This is the key output. "Free" = already computed by a `BlockDP::build`; "µs" = marginal cost on top.

| # | exact quantity | source | marginal cost | consumed today? |
|---|---|---|---|---|
| **Q1** | `Z` — the information-set mass (count of deals consistent with C1–C5) | `blockdp.hpp:271` | free with build | **no** |
| **Q2** | `mu[c][p]` exact per-card ownership | `blockdp.hpp:316-358` | ~210 µs after build | only in `belief=block` |
| **Q3** | `P(team T owns half-suit s)`, both teams, all 9 sets | `blockdp.hpp:409-422` | ~590 µs (needs group tables) | only in `belief=block` (`v05.hpp:727`) |
| **Q4** | `P(named allocation A)` — survival-checked | `blockdp.hpp:456-480` | free after group tables | **no** — the header itself says "not on any decision path" (`:455`) |
| **Q5** | **the full joint law of per-half-suit count vectors** `{(t, g(t)·S_t/Z)}` — i.e. exactly how many cards of `s` each seat holds | `Group::tpack/g/S` `blockdp.hpp:58-62` | free after group tables (~85 entries/group) | **no** |
| **Q6** | exact per-card posterior entropy `H(owner(c))` (mean **1.4486** nats) | linear in `mu`, `O(|U|·6)` | ~1 µs | **no** — `f[14]` is `binEnt(p)` of a single scalar (`v05.hpp:322`), a 2-way entropy |
| **Q7** | exact per-half-suit count-vector entropy (mean **3.3584** nats over 1,699 (state,set)) | Q5 | ~1 µs | **no** |
| **Q8** | exact expected hand composition `E[#cards of s held by p]` and per-seat entropy | linear in `mu` | free | **no** |
| **Q9** | exact conditional posterior after a hypothetical hit / miss | rebuild on the perturbed `Knowledge` | **1,255 µs per (hit,miss) pair**; Fast equivalent **8.2 µs** | the two-ply search uses `sinkhornDisj` (`v05.hpp:768, 785`), never the exact one |
| **Q10** | exact expected information gain of an ask (Q9 + Q6) | 2 builds/candidate | ~55 ms for all 44 live asks; ~5 ms for top-4 | **no** — no information-gain term exists in the 20 ask features |
| **Q11** | exact whole-deal particles, C5-filtered by exact rejection | `DealDP::sample` `belief.hpp:345`, `Belief::compute` `:638` | 533 µs (no C5) / 769 µs (C5) for 96 | built only in `Exact`/`ExactDisj` |
| **Q12** | `acceptRate` = `P(C5 | C1–C4)` — a direct scalar measure of how much the certificates bind | `belief.hpp:631, 662` | free where particles exist | written, **read by nothing** |
| **Q13** | `missCount[p][S]` — per-(seat, half-suit) refuted-ask tally | `belief.hpp:181` | free | written, **read by nothing** |
| **Q14** | `k.cheapTeamProb(s, team)` capacity-only screen (mean 0.0358 vs exact 0.0435) | `belief.hpp:112-126` | ~1 µs | only as a gate (`v05.hpp:828, 843`) |

Measured composite: **build + marginals + every group table + all per-card entropies = 1,183–1,667
µs/state**, i.e. **Q1–Q8 together for one price**, ~3× a `BlockDP` build and ~300× a `sinkhornDisj`.

### Named v0.6 opportunities

* **O1 — soft C5 (highest value).** Replace the 0/1 certificate filter in
  `BlockDP::enumerateGroup` (`blockdp.hpp:133-138`) with a **weight** `w_p(n)` = learned probability
  that seat `p` held `n` other cards of `S` when it asked. The group already enumerates and buckets
  by count vector, so this is a filter→weight change at **zero asymptotic cost**. §1.4 gives the
  target law (mean 2.33, P(≥2) = 72%) and its opponent dependence (random: 1.51, 40.7%). §3.4 says
  this is where the exact engine's whole deficit lives. Difficulty: medium (the count is measured at
  ask time and cards resolved later must be excluded from the conditioning — the time-independence
  argument in `blockdp.hpp:4-13` needs re-checking for a soft version). Payoff: high.
* **O2 — recalibrate `pAlloc`, do not "fix" it.** §7 shows the shipped statistic is ~1.9× the exact
  posterior probability yet **under**-confident against truth. Either number needs a calibration map
  before it can drive `declThreshold`/`declareMargin`. Low difficulty, unknown payoff, but it is a
  prerequisite for any belief-mode swap.
* **O3 — information-gain ask feature.** Q6+Q9 give an exact one-ply expected entropy reduction. The
  Fast version costs 8.2 µs/candidate (≈360 µs for all 44 live asks per turn, comparable to the
  current per-event budget); the exact version costs 1.25 ms/candidate. No such feature exists today.
* **O4 — consume Q5 directly.** The declaration decision currently asks two scalar questions
  (`pTeam`, `pAlloc`). The count-vector law answers "which teammate holds how many" jointly and
  exactly, at zero extra cost once the group table exists — this is the natural input to a
  *sequenced* declaration (declare the safe half-suits first, letting their reveals sharpen the rest,
  which the forced-endgame ladder at `fish.hpp:126-127` already assumes but is not fed).
* **O5 — a per-seat prior.** `θ_eff` is one global scalar shared by all five other seats (§4.3). Q13
  (`missCount`) and Q12 (`acceptRate`) are free per-seat statistics the current model ignores.

---

## 7. The deployed declaration path, measured against exact and against truth

`evaluateSet` under `Fast` (`v05.hpp:735-757`) sets `pTeam = cheap` — an **independent product** of
per-card team marginals (`v05.hpp:713`) — and `pAlloc` from M2 `feasibleAllocation`
(`v05.hpp:619-679`), which enumerates ≤ 3⁶ = 729 assignments, rejects on support and capacity, scores
by `Π_i marg[c_i][p_i]`, then re-scores the winner with `jointSequential`.

Measured (`bench3`/`bench4`, 900 states, 4,064 half-suit evaluations with a feasible allocation):

* `pTeam` (independent product) vs exact `P(team owns)`: mean |Δ| **3.04e-02**, max 1.00.
* `pAlloc` vs exact `P(that named allocation)`, aggregated over the buckets below: mean **0.0328**
  vs **0.0169** — a **1.94× overstatement**. (An earlier pass got 4.8× by indexing `seats` over the
  half-suit's six positions; `BlockDP::allocationProbability` indexes it over the *group's*
  unresolved cards, `blockdp.hpp:461-463`. Corrected numbers only are reported here.)
* Bucketed against **ground truth** (does the named allocation actually match the deal?):

  | `pAlloc` bucket | n | mean `pAlloc` | mean exact P | **actually right** | exact P = 0 |
  |---|---:|---:|---:|---:|---:|
  | < 0.2 | 3,829 | 0.0056 | 0.0030 | 1.12% | 13.7% |
  | 0.2 – 0.5 | 142 | 0.3339 | 0.1400 | **50.00%** | 0% |
  | 0.5 – 0.8 | 68 | 0.5908 | 0.2236 | **79.41%** | 0% |
  | 0.8 – 0.95 | 4 | 0.8189 | 0.2927 | 100% | 0% |
  | ≥ 0.95 | 21 | 1.0000 | 1.0000 | 100% | 0% |

  Soundness cross-check: **0** allocations with exact probability 0 were nevertheless true.

So in the decision-relevant band the shipped statistic is **under**-confident (0.591 predicted vs
0.794 realised) and the exact posterior is *far* more under-confident (0.224). Since a declaration is
+1 correct / −1 wrong, break-even is 0.5; `declThreshold = 0.81991` (`v05.hpp:60`) is rejecting a
band that is right 79% of the time. The shipped calibration diagnostic agrees:
`./fish calibrate --a=v05 --b=v05 --games=60` gives decl `[0.7,0.8)` predicted 0.7481 → observed
**1.0000** (n=22), ask `[0.3,0.4)` 0.3462 → 0.4041, `[0.5,0.6)` 0.5402 → 0.6014; ask ECE 0.0275,
decl ECE 0.0163.

---

## 8. Risks and latent defects found

* **R1 — `countWithMasks` uniform/greedy branch** (`belief.hpp:424-426`) is an approximation inside a
  function documented as exact. Unreachable from every current caller (verified: 673 uniform + 1,633
  general queries agree with the exact posterior to 1.7e-15 on certificate-free states). A v0.6
  caller passing heterogeneous non-singleton masks gets a silent approximation.
* **R2 — `BlockDP::bestTeamAllocation` does not re-check C5 survival** for the assignment `pick`
  materialises (`blockdp.hpp:425-449`, `pick` at `:482-492`), whereas `allocationProbability`
  explicitly does and says why (`:450-455, :468-476`). It is on the decision path (`v05.hpp:730,
  932`). Measured **0 failures in 35,161 checks** (10,374 with a live certificate, 40 deals v05 vs
  v03, seed 4242) — unfalsified, unproven.
* **R3 — silent truncation.** `enumerateGroup` returns `false` at `MAXENT = 512` (`blockdp.hpp:145`)
  but `ensureGroupTable` just `return`s (`:397`), leaving a **partial** table that
  `teamOwnsProbability` will then normalise as if complete. Currently unreachable (max count vectors
  for 6 cards over 6 seats is C(11,5) = 462 < 512; measured 0 hits in 35,161), but it is a silent
  failure mode if `SETSZ` or `MAXENT` ever move. Same shape: the 16-certificate cap at
  `blockdp.hpp:215` and the 2,000,000-node guards at `:121`/`:381`.
* **R4 — the exact posterior is *not* the best posterior.** §3.4. Any v0.6 that swaps `belief=block`
  into the inner loop without a policy-aware likelihood is trading a better predictor for a worse
  one, at 11–13× the cost. This reframes R5's R12 ("matched-budget refit never run"): a refit alone
  may not recover the 6.2 points.
* **R5 — mode swap silently deletes the opponent model.** `belief=block` never calls `priorWeight`
  unless `build` fails (`v05.hpp:166-174`); `belief=hybrid`/`sinkhorn` never call it at all. Any
  belief-mode ablation is therefore confounded with a prior ablation.
* **R6 — `Hybrid` is mis-named.** Its ask marginals are plain `sinkhorn` (no certificates, no prior),
  and only its declaration query is exact — yet it costs 4,149 µs/event, more than `Block` in one of
  my two samples. It is the worst point on the cost/accuracy frontier of the seven.
* **R7 — timing error bars.** Machine load average was 14 throughout; all figures are user CPU time
  and two independent samples differ by up to 25% (`block`: 3,860 vs 5,093 µs/event). Re-measure on
  a quiet machine before quoting in the paper.

---

## 9. Reproduction

Engine commands (from `engine/`, after `make`):
```
./fish match --a=v05:belief=M --b=v05:belief=M --games=20 --threads=1 --seed=31    # M ∈ {fast,indep,sinkhorn,hybrid,exact,exactdisj,block}
./fish match --a=v05 --b=v05:ptheta=0.56656,pphi=0 --games=40 --threads=4 --seed=77
./fish oracle --games=20 --a=v05 --b=v03 --seed=20260822 --samples=1200
./fish calibrate --a=v05 --b=v05 --games=60 --seed=8181
```
Out-of-tree harnesses (read-only w.r.t. `engine/src`), in
`/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/eacfe5c8-cfa1-4aef-ad3c-cf4e017d6982/scratchpad/`,
each built with `clang++ -std=c++20 -O3 -march=native -fno-math-errno -pthread`:

| file | what it measures | §|
|---|---|---|
| `bench_belief.cpp` | structural census; per-kernel cost; accuracy vs Block; first defect-K pass | 3.2, 3.3 |
| `bench2.cpp` | defect-K decomposition; Sinkhorn vs `DealDP`; predictive quality; candidate-set size | 3.3, 3.4 |
| `bench3.cpp` | deployed M2 declaration path vs exact; repeated-cost timings; exact aggregates | 3.2, 7 |
| `bench4.cpp` | clip-controlled defect K; `pAlloc` calibration vs truth; soundness cross-check | 4.3, 7 |
| `bench5.cpp` | `bestTeamAllocation` survival soundness at 35,161 checks | 8-R2 |
| `bench6.cpp` | predictive quality across the opponent panel | 3.4 |
| `bench7.cpp` | the empirical C5 count law per policy | 1.4 |
| `bench8.cpp` | `countWithMasks` uniform-branch exactness on certificate-free states | 2, 8-R1 |

All harvest states through `Game::observer` (`game.hpp:168`), the same hook `fish oracle` uses, so
they see exactly the belief states real play produces.
