# High-Performance Game-Engine Engineering and Neural Evaluation on Commodity CPU
### Literature review and implementation guide for a Canadian Fish (Literature) agent
### Target platform: single ~15-core Apple Silicon machine, clang++ / C++20

---

## 1. Executive summary

1. **The whole 54-card deck fits in one `uint64_t`.** With the card index `c = 6*h + i` (h = half-suit 0..8, i = position 0..5), a half-suit is the contiguous 6-bit field `0x3Full << (6*h)`. Every legality test in Fish ("do I hold a card of this half-suit?", "is this card in my hand?", "is this half-suit complete for my team?") becomes 1–3 ALU instructions. This is the single highest-leverage engineering decision in the project.
2. **Because all transfers are public, the entire hidden state is one bipartite assignment problem**: which of the 54 cards each of 6 players started with. A belief state is exactly a `6 × 54` bit-possibility matrix (six `uint64_t`) plus cardinality constraints. Updates are O(1) bit operations per observed event; determinization sampling is ~200–800 ns with constraint propagation.
3. **Root/ensemble parallelisation is the best-documented and safest parallel MCTS scheme.** Chaslot, Winands & van den Herik (2008) measured **strength-speedup 14.9× at 16 threads** for root parallelisation versus **8.5×** for tree parallelisation with virtual loss, **3.3×** with local mutexes and no virtual loss, and **2.4×** for leaf parallelisation.
4. **For self-play data generation, use game-level parallelism (one whole game per thread).** This is a degenerate, perfect form of root parallelism: zero synchronisation, linear scaling, and bit-exact reproducibility if you use a counter-based RNG.
5. **Virtual loss is not free and has documented negative results.** Mirsoleimani et al. (2017) found that adding virtual loss to *lock-free* tree parallelisation **increased search overhead and reduced time efficiency** across a full sweep of `C_p ∈ [0,1]` and 2–64 threads. Chaslot's positive result was for *lock-based* trees.
6. **WU-UCT (Liu et al., ICLR 2020) is the modern replacement for virtual loss**: track `O_s`, the number of in-flight (unobserved) simulations, and use `argmax { V_{s'} + β·sqrt(2 log(N_s + O_s) / (N_{s'} + O_{s'})) }`. It achieved near-linear speedup with negligible strength loss at 16 workers and beat TreeP/LeafP/RootP on 12 of 15 Atari games.
7. **Lock-free MCTS in C++ is a solved problem**: pack `(W, N)` into one 64-bit atomic (W in the high 32 bits, N in the low 32) so backup is a single `fetch_add` and selection a single `load` — this eliminates the read-tearing race by construction (Mirsoleimani et al., ICAART 2018). They measured playout-speedup rising from 18× (coarse lock) to **23×** (lock-free) on 24 cores, and up to **34×** at `C_p = 0`.
8. **Transposition tables in imperfect information are dangerous but tractable.** Monte-Carlo Graph Search (Czech, Korus & Kersting 2020) gives a concrete, implementable fix for the "information leak" when a node has several parents: a Q-correction `Q_φ = N(s,a)·Q_δ(s,a) + V*(s_{t+1})` clipped to `[V_min, V_max]`, applied only when `|Q_δ| > Q_ε = 0.01`. They report **30–70 % memory reduction** and Elo gains in chess/crazyhouse.
9. **Use a counter-based RNG (Philox/Threefry) or splitmix64-seeded xoshiro256++ per stream.** Counter-based generators are stateless: `x = b_k(counter)`, so `(run, generation, game, ply, purpose)` deterministically indexes the random stream regardless of thread scheduling. xoshiro256++ is 0.75 ns/64 bits; splitmix64 0.63 ns; PCG-128-XSH-RS 1.70 ns; MT19937-64 1.36 ns.
10. **NNUE-style incremental evaluation is the right neural architecture for a CPU-only Fish bot.** Because each Fish event changes only ~4–12 input features, a `Features → 2×M → K → outputs` net with an int16 accumulator updated by column add/subtract and int8 hidden layers evaluates in **~0.1–1 µs** on one Apple P-core. A from-scratch dense forward pass of the same net costs 20–100× more.
11. **Gumbel AlphaZero (Danihelka et al., ICLR 2022) is the single most important algorithmic finding for a CPU budget.** It gives *guaranteed* policy improvement at **n = 2 simulations**. On 9×9 Go, Gumbel MuZero trained with n = 2, 4, 16, 32 dramatically outperformed MuZero at the same budget. For a game with ~40–140 legal asks and no GPU, planning with `m = 16` sampled actions and `n = 16–50` simulations is a far better use of cycles than 800 PUCT simulations.
12. **KataGo's non-domain-specific tricks transfer directly and are cheap**: playout cap randomisation (full search on p = 25 % of turns at N = 600, fast search at n = 100 otherwise, only full-search turns become training data) gave a 1.37× speedup; forced playouts `n_forced(c) = sqrt(k·P(c)·Σ_{c'} N(c'))` with k = 2 plus policy target pruning; a sublinear data window `N_window = c(1 + β((N_total/c)^α − 1)/α)`, c = 250 000, α = 0.75, β = 0.4.
13. **For tuning a handful of continuous weights against noisy win rates, CLOP (Coulom 2011) beats CMA-ES, SPSA, cross-entropy and UCT** on smooth objectives, and has essentially one meta-parameter (`H = 3`). Coulom explicitly reports that **UH-CMA-ES "does not work well"** on this class of problem. Bayesian optimisation with EI was what DeepMind actually used for AlphaGo — 3–10 hyper-parameters per task, **50 games per evaluation**, and a **50 % → 66.5 %** self-play win rate improvement before the Lee Sedol match.
14. **Budget arithmetic for the target machine.** With a bitboard engine, a random Fish playout (~60–120 decisions) costs ~1.5–3 µs; a full PUCT iteration with a small NNUE-style net costs ~5–15 µs. That is ~10⁸–10⁹ MCTS iterations/hour machine-wide, or **roughly 10⁵ full self-play games per hour** at 400 sims/decision — and ~10⁶–10⁷ games/hour with a Gumbel search at n = 16.
15. **Match-evaluation statistics dominate the tuning loop, not compute.** Resolving a 10-Elo difference needs ≈ 4 600 games for 95 % significance and ≈ 9 500 games for 80 % power. Use *duplicate deals* (replay each deal with the teams swapped, and with fixed determinization seeds) — the standard variance-reduction trick from bridge — before spending 10× more CPU.

---

## 2. The hardware you are actually programming

Numbers from Kiessling et al., *Apple vs. Oranges* (arXiv:2502.05317) and Apple/LLVM documentation:

| Feature | M1 | M2 | M3 | M4 |
|---|---|---|---|---|
| ISA | ARMv8.5-A | ARMv8.6-A | ARMv8.6-A | ARMv9.2-A |
| P clock | 3.2 GHz | 3.5 GHz | 4.05 GHz | 4.4 GHz |
| E clock | 2.06 GHz | 2.42 GHz | 2.75 GHz | 2.85 GHz |
| Vector unit | NEON/128 | NEON/128 | NEON/128 | NEON/128 |
| L1 (P/E) | 128/64 KB | 128/64 KB | 128/64 KB | 128/64 KB |
| L2 (P/E) | 12/4 MB | 16/4 MB | 16/4 MB | 16/4 MB |
| Measured STREAM (CPU) | 59 GB/s | 78 GB/s | 92 GB/s | 103 GB/s |

Pro/Max variants scale this up: an M4 Pro is 8–10 P + 4 E cores with 273 GB/s theoretical unified-memory bandwidth; P-cores report ~192 KB L1I + 128 KB L1D and share a 32 MB L2 in the 14-core part.

Engineering consequences:

- **No SVE.** Apple M-series exposes NEON 128-bit only (plus SME on M4+ via Accelerate). Write NEON intrinsics (`int8x16_t`, `int16x8_t`, `int32x4_t`, `vdotq_s32`) or plain autovectorisable loops; do **not** plan on 256/512-bit vectors.
- **AMX/SME is only reachable through Accelerate/vDSP/BNNS**, not through documented intrinsics. Measured `SGEMM` peaks: 0.90 TFLOPS (M1), 1.09 (M2), 1.38 (M3), 1.49 (M4) via vDSP. This only helps if you batch evaluations into matrices of ≥ 64 rows; for single-position latency it is useless.
- **Heterogeneous cores wreck barrier-synchronised parallelism.** A 4.4 GHz P-core and a 2.85 GHz E-core in the same fork-join barrier means ~35 % of every barrier interval is idle. Prefer *work-stealing over independent games* (no barriers at all). Empirically, users report 16 threads being **slower** than 12 on M4 Max for naive thread pools.
- **QoS classes control which cores you get.** `pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0)` gets you P-cores; `QOS_CLASS_BACKGROUND` / `PRIO_DARWIN_BG` confines the thread to E-cores. Set QoS explicitly on every worker; the default inherits from the spawning context and can silently demote your whole pool.
- **Recommended thread count:** `#P-cores` workers at `USER_INITIATED` plus optionally `#E-cores` workers at `UTILITY` doing *independent* games. Do not put E-cores in a shared MCTS tree.

---

## 3. Bitboard representation for a 54-card Fish deck

### 3.1 The layout

```
card index  c ∈ [0,54)
half-suit   h = c / 6      (compile to c * 0xAAAB >> 19, or just c/6 — clang emits a multiply)
slot        i = c % 6
HS_MASK(h)  = UINT64_C(0x3F) << (6*h)
```

Nine half-suits × 6 = 54 bits ≤ 64. The 10 unused high bits are free for flags. Half-suit assignment: `h = 0..3` → the low half-suits (2–7 of each suit), `h = 4..7` → the high half-suits (9–A), `h = 8` → `{8♠,8♥,8♦,8♣, Joker₁, Joker₂}`.

### 3.2 Core primitives

```cpp
struct Hand { uint64_t bb; };                       // 8 bytes
inline bool  has(uint64_t bb, int c)      { return (bb >> c) & 1u; }
inline bool  hasHS(uint64_t bb, int h)    { return bb & HS_MASK(h); }
inline int   count(uint64_t bb)           { return __builtin_popcountll(bb); }
inline int   countHS(uint64_t bb,int h)   { return __builtin_popcountll(bb & HS_MASK(h)); }
inline int   pop_lsb(uint64_t& bb)        { int c=__builtin_ctzll(bb); bb&=bb-1; return c; }
```

On AArch64 `__builtin_popcountll` lowers to `fmov d0,x0; cnt v0.8b; addv b0,v0.8b; fmov w0,s0` (~4 cycles, ~2 with pipelining); `__builtin_ctzll` lowers to `rbit; clz` (2 cycles). Both are essentially free relative to a memory access.

### 3.3 Move generation

An ask `(target q, card c)` is legal for actor p iff:

```
hasHS(hand[p], c/6) && !has(hand[p], c) && team(q) != team(p) && hand[q] != 0
```

Enumerate directly from bitboards:

```cpp
uint64_t occupiedHS = 0;                       // half-suits p holds at least one card of
for (int h = 0; h < 9; ++h) if (hand[p] & HS_MASK(h)) occupiedHS |= HS_MASK(h);
uint64_t askable = occupiedHS & ~hand[p] & ~declared;   // cards p may name
for (int q : opponents_of(p)) if (hand[q]) 
    for (uint64_t b = askable; b; ) { int c = pop_lsb(b); emit(q, c); }
```

Branch-count bound: `|askable| = 6k − |hand[p]|` where `k` = number of half-suits held, so ≤ `54 − 9 = 45`, times 3 opponents = **≤ 135 asks**, typically 40–90 early and shrinking fast. Declaration actions add a small number of "declare half-suit h with allocation A" moves; in practice you should not enumerate all `3^6 = 729` allocations — generate a declaration action only when your team *provably* holds all six cards, in which case the allocation is forced and unique, plus optionally a small set of "gamble" declarations.

**Whole-state footprint.** `6 × uint64_t` hands + `uint64_t declared` + `uint8_t declaredOwner[9]` + `uint8_t turn` + `uint8_t scores[2]` = **72 bytes**, one and a bit cache lines. Copy-by-value in playouts is a single `memcpy` of 72 bytes — cheaper than any undo-move machinery. **Do not implement make/unmake for Fish; copy the state.**

### 3.4 Incremental Zobrist hashing

Precompute `Z[player][card]` (6 × 54 random `uint64_t`, from splitmix64), `Zturn[6]`, `Zdecl[9][3]` (unowned/team0/team1).

```
H ^= Z[from][c] ^ Z[to][c];        // a successful ask
H ^= Zturn[old] ^ Zturn[new];      // turn change
H ^= Zdecl[h][0] ^ Zdecl[h][team]; // declaration
```

This is the *perfect-information* (determinization) key. For an **information-set** key you must hash only public data: the declared-set vector, the hand-size vector, the turn, and a canonicalisation of the constraint set (see §4). The safest practical construction is:

```
H_info = Zturn[turn] ^ fold(Zdecl[h][owner_h]) ^ fold(Zsize[p][n_p]) ^ fold(Zcon[constraint_id])
```

where `Zcon` hashes each surviving belief constraint (§4.2). Constraint sets are order-independent under XOR, which is exactly what you want.

---

## 4. Belief state and incremental belief updates

### 4.1 Why Fish is unusually friendly

Because *every* ask, answer, transfer, declaration and hand count is public, the only hidden variable is the initial 9-card deal. Equivalently: at any moment there is a set `Ω` of assignments of the 54 − |declared| remaining cards to the 6 players consistent with all observations, and every observation is a *hard logical constraint* — there is no noise to model. This is the constraint-satisfaction (CSP) picture of belief used by Vasconcelos & Costa (arXiv:2507.19263) for Stratego/Goofspiel, but far cleaner here.

### 4.2 Representation

```cpp
struct Belief {
    uint64_t may[6];             // may[p] bit c set  ⇒  p may hold card c
    uint8_t  n[6];               // exact hand size (public)
    // cardinality constraints: "player p holds ≥1 card of half-suit h"
    uint16_t atLeastOne[6];      // bit h set ⇒ p is known to hold ≥1 card of HS h
};                               // 6*8 + 6 + 12 = 66 bytes
```

Update rules, all O(1) bit operations:

| Public event | Update |
|---|---|
| p asks q for card c | `may[p] &= ~(1<<c)` (asker provably lacks it); `atLeastOne[p] \|= 1<<(c/6)` |
| q answers **no** | `may[q] &= ~(1ull<<c)` |
| q answers **yes** | card transfers: `hand[p] \|= 1<<c`, `may[*] &= ~(1<<c)` for all except p; `may[p] \|= 1<<c`; `n[q]--`, `n[p]++` |
| p drops to 0 cards | `may[p] = 0`; clear `atLeastOne[p]` |
| half-suit h declared | `may[*] &= ~HS_MASK(h)` for all p |

Note the asymmetry that makes Fish inference rich: **the ask itself is informative in two directions** — it proves the asker holds ≥ 1 card of that half-suit and proves the asker does *not* hold that specific card.

### 4.3 Constraint propagation (arc consistency + Hall pruning)

Run to fixpoint after each event (typically 1–2 sweeps, ~100 ns):

1. **Singleton card:** if card `c` is possible for exactly one player p (`popcount over p of bit c == 1`), assign it: it becomes known-held.
2. **Saturated player:** if `popcount(may[p]) == n[p]`, all those cards are p's; clear them from every other `may[q]`.
3. **Exhausted player:** if `n[p] == 0`, `may[p] = 0`.
4. **`atLeastOne` propagation:** if `p` is known to hold ≥1 card of half-suit h and `popcount(may[p] & HS_MASK(h)) == 1`, that card is p's.
5. **Hall sets (optional, more expensive):** if a set S of cards has `|S| == Σ_{p∈P} n[p]` for the set P of players that can hold them, no player outside P can hold any card of S. A cheap approximation is to run rules 1–4 to fixpoint and skip general Hall reasoning; the full version is `O(2^6)` over player subsets = 64 checks, still only microseconds.

Rules 1–4 to fixpoint is a *pure bitwise* loop with no branching on card identity, and is the correct place to spend engineering effort. Rule 5 is worth it in Fish because the team structure creates many small Hall sets late in a hand.

### 4.4 Sampling determinizations

The determinization problem is: sample uniformly (or by an importance weight) from perfect matchings of the bipartite graph (cards × players) with degree constraints `n[p]`. Exact uniform sampling is #P-hard in general (permanent computation), so use the standard **scarcity-ordered sequential sampler with propagation and restart**:

```
loop:
  B ← copy of belief; assigned ← 0
  while cards remain:
      c ← card with fewest feasible owners (choose via popcount over 6 words; ties → lowest index)
      if no feasible owner: goto loop        // restart (rare after propagation)
      sample owner p ∝ remaining_capacity[p] · prior[p][c]
      assign; remaining_capacity[p]--; propagate rules 1–4
  return assignment
```

Costs on the target hardware: 54 iterations × (6-way popcount + propagate) ≈ **200–800 ns per determinization**, with a restart rate under ~2 % once rules 1–4 are running. That means a 1000-determinization ISMCTS root search spends well under 1 ms on sampling — negligible.

**Importance weighting.** The sequential sampler is *not* uniform. If you want unbiased expectations, accumulate the log-probability of the sampled path and weight by `w = 1/q(x)`; or, cheaper and usually sufficient, accept the bias and note it — Cowling, Powley & Whitehouse (2012) show ISMCTS is robust to non-uniform determinization as long as the support is correct. For a first version, take the bias.

**`prior[p][c]`** is where a learned opponent model plugs in (probability p held card c given the observed asking behaviour). Keep it as a `6 × 54` float table refreshed once per decision, not per sample.

---

## 5. MCTS data structures, memory layout and cache behaviour

### 5.1 Node/edge layout

Use **edge-centric SoA with contiguous child blocks in a bump-allocated arena.**

```cpp
struct NodeHdr {           // 16 bytes
    uint32_t firstEdge;    // index into edge arrays
    uint16_t nEdges;
    uint16_t flags;
    std::atomic<uint64_t> wn;   // W in high 32 bits (fixed-point), N in low 32
};

// Edge arrays (parallel, index-aligned):
std::vector<std::atomic<uint64_t>> eWN;   //  8 B  packed (W,N)
std::vector<float>                 eP;    //  4 B  prior
std::vector<uint32_t>              eChild;//  4 B  child node index, 0 = unexpanded
std::vector<uint16_t>              eAct;  //  2 B  encoded action
```

Rationale:

- The PUCT/UCT scan in the selection step touches `eWN`, `eP` for `nEdges` consecutive entries. With 40 edges that is `40×8 + 40×4 = 480 B` ≈ 8 cache lines, prefetched linearly by the hardware streamer. An AoS node with 24-byte edges would touch 15 lines and interleave cold fields.
- Contiguous child blocks are essential. Mirsoleimani et al. explicitly criticise Fuego's global node pool for this: *"the children of a node may not be assigned in consecutive memory locations which results in poor spatial locality"*, and note this matters specifically for `SELECT`.
- Allocate the whole arena up front (`reserve`) and hand out blocks with one `fetch_add(nEdges, std::memory_order_relaxed)` on a global bump cursor. No malloc in the hot loop, no per-node destructor.
- Align each node's edge block to 64 bytes if `nEdges ≥ 8` (pad the arrays) to avoid two nodes sharing a line and false-sharing their counters.

### 5.2 The packed `(W, N)` atomic

This is the key trick from Mirsoleimani et al. (ICAART 2018). Store `W` and `N` in one 64-bit atomic:

```
GET():   x = wn.load(std::memory_order_relaxed);
         W = (int32_t)(x >> 32);  N = (uint32_t)(x & 0xFFFFFFFF);
SET(Δ):  wn.fetch_add(((uint64_t)(uint32_t)Δ << 32) | 1ull, std::memory_order_relaxed);
```

Because the two statistics are read and written *together*, the "shared backup + selection" (SBS) race — reading a `W` from after an update and an `N` from before it — is impossible by construction, with zero locks and one instruction. Use fixed-point for `W` (e.g. Q16.16, or integer counts of {win=+1, loss=−1}) so `fetch_add` is exact and **deterministic irrespective of thread interleaving** — a float accumulator is not associative and destroys reproducibility.

Expansion is made race-free by two atomic flags per node:

```
if (!isParent.exchange(true))            { build child list; nUnexpanded.store(k);
                                           isExpandable.store(true, memory_order_release); }
if (isExpandable.load(memory_order_acquire)) {
    int idx = nUnexpanded.fetch_sub(1);
    if (idx == 0) isFullyExpanded.store(true);
    if (idx < 0) return current;  else return children[idx];
}
```

Measured effect (Hex, 24-core Xeon, 1 048 576 playouts fixed): coarse-grained lock peaked at **18×**; lock-free reached **23×**, and reached 17× at 32 tasks where the lock version needed 64. At `C_p = 0` the lock-free version reached **34×** — a genuinely super-linear artefact of parallel search producing shallower, more symmetric trees (average depth fell from 56 to ~25).

### 5.3 Transposition tables for imperfect information

Two distinct uses, do not conflate them:

**(a) Determinization-level TT (perfect-information subgame).** Keyed on the full hashed determinization state. Useful only if you run a solver/rollout that re-reaches states — in Fish, endgames after several half-suits are declared do transpose heavily. Use a **bucketed, lockless table**:

- Entry: `{ uint64_t key_xor_data; uint64_t data; }` — 16 bytes, 4 entries per cache line.
- **Hyatt/Mann lockless XOR trick:** store `key ^ data` and `data`; probe by testing `(stored_key ^ stored_data) == key`. A torn write from a racing thread almost always fails the test. Caveats: it is probabilistic, it requires both 64-bit words to be individually atomic (true on AArch64 for aligned 8-byte accesses), and you must still make entries cache-line-aligned to avoid false sharing.
- Replacement: two-tier (depth-preferred slot + always-replace slot) per bucket, as in most chess engines and in the two-layered TT used by ISMCTS practitioners.

**(b) Information-set-level TT / graph search.** Different ask orderings reaching the same public information set is *common* in Fish (asks that fail are commutative in their information content). Merging them into a DAG is exactly Monte-Carlo Graph Search.

MCGS (Czech, Korus & Kersting 2020) selection/backup, transcribed:

```
V*(s_t)          = −V(s_t)                                   (7)   [two-team zero-sum]
Q_δ(s_t,a)       = Q(s_t,a) − V*(s_{t+1})                     (8)
Q_φ(s_t,a)       = N(s_t,a)·Q_δ(s_t,a) + V*(s_{t+1})          (9)
Q'_φ(s_t,a)      = max(V_min, min(Q_φ(s_t,a), V_max))         (10)
```

During selection, if the next node is a transposition and `|Q_δ| > Q_ε` (they use `Q_ε = 0.01`), **stop the descent and back up `Q'_φ` instead of evaluating the network** — you already have enough signal. Store Q on *both* edges and nodes to prevent information leaking between parents. Reported: 30–70 % memory reduction, no measurable NPS loss (the extra CPU work hides behind evaluation), Elo gains in chess and crazyhouse.

Historical alternatives worth knowing: Childs, Brodeur & Kocsis (2008) `UCT1/UCT2/UCT3` variants for sharing statistics across transpositions; Saffidine, Cazenave & Méhat's `UCD` framework, which parameterises *which* mean and *which* parent/child visit counts you use in the exploration term of a DAG.

**Fish-specific warning.** Sharing statistics across an information-set DAG is only sound if the *belief* is identical, not merely the public counts. Two histories with the same hand sizes and declared sets can have very different `may[]` matrices. **Include a hash of the propagated constraint set in the key** (§3.4), or you will merge genuinely different belief states and get a silent strength regression that is very hard to debug.

---

## 6. Parallel MCTS: what the literature actually measured

### 6.1 The three classical schemes and their numbers

Chaslot, Winands & van den Herik, *Parallel Monte-Carlo Tree Search* (CG 2008), program **Mango**, 13×13 Go vs GNU Go 3.7.10 level 0, 1 s/move, 16-core POWER5 (single-thread baseline 3 400 playouts/s, 26.7 % win rate). Strength-speedup `S` defined by `E_t(S) = E_m` where `E_t(T) = A log₂T + B`, `A = 56.7`, `B = −175.2` (R² = 0.9922).

| Threads | Leaf (GPS / strength) | Root (GPS / strength) | Tree, global mutex | Tree, local mutex | Tree + virtual loss |
|---|---|---|---|---|---|
| 2  | 1.8 / 1.2 | 2 / **3.0** | 1.8 / 1.6 | 1.9 / 1.9 | 1.9 / 2.0 |
| 4  | 3.3 / 1.7 | 4 / **6.5** | 3.2 / 3.0 | 3.6 / 3.0 | 3.6 / 3.6 |
| 16 | 7.6 / 2.4 | 16 / **14.9** | 4.0 / 2.6 | 8.0 / 3.3 | 9.1 / **8.5** |

Two caveats the authors themselves raise, and that Soejima et al. later confirmed: root parallelisation's apparent super-linearity at 4 threads (6.5× from 4 threads) is partly an artefact of **UCT getting stuck in local optima**; four independent 1 s searches beat one 4 s search. On 9×9 with 4 threads, root and tree parallelisation were statistically indistinguishable (60.2/63.9 %, 78.7/79.3 %, 87.2/89.2 % at 0.25/2.5/10 s).

### 6.2 Virtual loss — formula and the negative result

Virtual loss (attributed to Coulom): when a thread selects node `s` during descent, immediately apply

```
N(s) ← N(s) + n_vl ,      W(s) ← W(s) + n_vl · r_vl      (r_vl = −1, a loss for the mover)
```

and undo it during backup. With the packed `(W,N)` atomic this is one `fetch_add` on the way down and the inverse (plus the real result) on the way up.

**Negative result.** Mirsoleimani, Plaat, van den Herik & Vermaseren, *An Analysis of Virtual Loss in Parallel MCTS* (ICAART 2017), define

```
SO  = playouts_parallel / playouts_sequential − 1                    (search overhead)
Eff = time_sequential / (workers · time_parallel)                    (time efficiency)
```

and sweep `C_p ∈ {0.0 … 1.0}` × `{2,4,…,64}` tokens on a Horner-scheme HEP problem (10 240 playouts, 20 runs/point, dual Xeon E5-2596v2). Their conclusion, verbatim in spirit: *using virtual loss for lock-free tree parallelisation still degrades performance* — the virtual-loss curve sits at higher SO across essentially the whole grid. Sephton et al. (2014) similarly found virtual loss made almost no difference in *Lords of War*.

The mechanism is exactly what Liu et al. (ICLR 2020) name **exploitation failure**: virtual loss is a *hard additive penalty* that persists even when the workers are certain a node is optimal, so they refuse to co-simulate it.

### 6.3 WU-UCT: the recommended shared-tree scheme

Liu, Chen, Yu, Zhai, Zhou & Liu, *Watch the Unobserved* (ICLR 2020). Add a third statistic `O_s` = number of initiated-but-incomplete simulations through `s`, and select

```
a_s = argmax_{s' ∈ C(s)}  { V_{s'} + β · sqrt( 2 log(N_s + O_s) / (N_{s'} + O_{s'}) ) }        (4)
```

with

```
incomplete update:   O_s ← O_s + 1                      (5)   [applied on the way down, before simulating]
complete update:     O_s ← O_s − 1 ;  N_s ← N_s + 1     (6)   [applied at backup, with V_s updated as usual]
```

The penalty vanishes automatically as `N_{s'}` grows — the property virtual loss lacks. System design: a **centralised master** owns selection and backpropagation (both cheap: measured 0.07 ms and 0.05 ms per rollout, 0.3 % and 0.2 % of the time), and worker pools do expansion (1.0–18 ms) and simulation (23–67 ms). Simulation-worker occupancy was 99.9 %.

Results: linear speedup to 16 workers with performance standard deviation of 0.67 and 1.22 game-steps against means of 12 and 30; on 15 Atari games with 128 simulations and 16 workers, WU-UCT beat TreeP (virtual loss), LeafP and RootP on 12/15, with statistically significant wins (Bonferroni-adjusted p < 0.0011) on 7, 9 and 7 games respectively.

For a C++ implementation you do not need the process-level master/worker split — implement `O` as a third field. Since `(W,N)` already uses 64 bits, either use a second `std::atomic<uint32_t> O` (accept a 2-word non-atomic read; `O` only affects exploration, so tearing is benign) or use a 128-bit `atomic<__int128>` (lock-free on AArch64 via `CASP`, but slower).

### 6.4 Concrete recommendation for Canadian Fish

**Two different parallel programs, do not merge them.**

- **Self-play / data generation and match play (99 % of your CPU-hours): game-level parallelism.** One worker per core, each playing complete games with a private tree, private arena, private RNG stream. Scaling is linear by construction, there is no synchronisation, and results are bit-reproducible. Aggregate finished games into a lock-free MPSC queue (or per-thread files merged later).
- **Interactive/analysis mode (one decision, all 15 cores): WU-UCT over a shared tree**, packed `(W,N)` atomics, contiguous child blocks, no locks, no virtual loss. Fall back to root parallelism (15 independent trees, merge root visit counts) if you want the simplest thing that provably works — Chaslot's 14.9×/16 threads is a strong baseline and the merge is 20 lines of code.
- **Extra structure Fish gives you for free:** ISMCTS already loops over determinizations. Shard the *determinization index* across threads (`det_id % nThreads`). Each thread's determinizations are independent, so this is root parallelism at the *belief* level, and it also parallelises the constraint-propagation and sampling work.

---

## 7. Deterministic, reproducible parallel RNG

### 7.1 Why counter-based

A counter-based RNG (CBRNG) is a keyed bijection `x = b_k(c)` on a counter, evaluated statelessly. Salmon, Moraes, Dror & Shaw (SC11) show these pass SmallCrush/Crush/BigCrush, run at a few cycles/byte, need no per-stream state, and are trivially parallel: *different threads use distinct counter ranges with the same key*.

For a self-play system this is decisive: address the stream by **semantic coordinates**, not by thread:

```
key     = (run_id, generation)
counter = (game_id, ply, purpose, sequence)     // purpose ∈ {DEAL, DETERMINIZE, ROLLOUT, NOISE, TEMP}
```

Now the deal for game 12345 is the same whether it ran on thread 0 of a 15-thread run or thread 3 of a 4-thread run, and a bug in game 12345 replays exactly. This is worth more than any raw-speed difference between generators.

### 7.2 The generators

**splitmix64** (state 64 bits, 0.63 ns/64 bits) — use for *seeding* and for Zobrist table generation:

```c
uint64_t splitmix64(uint64_t &x) {
    uint64_t z = (x += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}
```

**xoshiro256++** (state 256 bits, period 2²⁵⁶−1, 0.75 ns/64 bits, no BigCrush failures) — use as the per-stream workhorse, seeded from splitmix64:

```c
static inline uint64_t rotl(const uint64_t x, int k){ return (x<<k)|(x>>(64-k)); }
uint64_t next(void) {
    const uint64_t result = rotl(s[0] + s[3], 23) + s[0];
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;    s[3] = rotl(s[3], 45);
    return result;
}
```

(Vigna's shootout: xoshiro256+ 0.61 ns, xoshiro256++/** 0.75 ns, xoroshiro128+ 0.80 ns, splitmix64 0.63 ns, PCG-128-XSH-RS 1.70 ns, MT19937-64 1.36 ns with LinearComp failures. xoshiro256+ fails Hamming-weight tests after ~5 TB and should be used only for producing doubles.)

**Philox / Threefry** — use where you want *true* stateless addressability. Philox is a Feistel network built on integer multiply-high; `philox4x32-10` (10 rounds, 128-bit counter, 64-bit key) is the standard workhorse; Threefry (`threefry4x64-20`) is a reduced-round Threefish. The round for `philox2x32` is:

```
(hi, lo) = mulhilo32(M0, C0)
C0' = hi ^ K ^ C1 ;   C1' = lo ;   K += W0
```

with the Random123 constants `M(4x32,0) = 0xD2511F53`, `M(4x32,1) = 0xCD9E8D57`, `W32_0 = 0x9E3779B9`, `W32_1 = 0xBB67AE85`. *(Copy the exact 64-bit variants from the Random123 header rather than from memory — the `philox4x64` multipliers/Weyl constants are not verified here.)*

**Practical hybrid (recommended):** derive a 256-bit xoshiro seed from `splitmix64` applied to `hash(key, counter)` at the start of each *semantic unit* (a game, or a decision), then stream from xoshiro inside that unit. You get CBRNG addressability at unit granularity and xoshiro speed inside. Never call `rand()`, never share a `std::mt19937` across threads, and never let the number of random draws depend on thread scheduling.

---

## 8. Neural evaluation on CPU: NNUE-style incremental nets

### 8.1 Why NNUE and not a dense MLP

A Fish action changes very little of the state: one card moves, a few `may[]` bits clear, one turn index changes. That is the same structural property that makes NNUE work in chess: *only a handful of input features change per move, so the first (and by far largest) layer can be updated incrementally instead of recomputed.*

Canonical NNUE shape (Stockfish lineage): `FeatureSet[N] → M×2 → K → 1`, e.g. `HalfKP[40960] → 256×2 → 32 → 32 → 1`, later `1024×2 → 8 → 32 → 1`. The accumulator holds the first layer's output for both perspectives.

### 8.2 The accumulator and its update

```
A[persp] = b + Σ_{f ∈ ActiveFeatures(persp)} W[:, f]              (int16 vector of length M)
```

Incremental update for a change set:

```
for f in removed:  A[persp] -= W[:, f]
for f in added:    A[persp] += W[:, f]
```

Full refresh only when the perspective anchor changes (in chess, a king move; in Fish, when the *acting player* changes — so plan for a refresh every turn-pass and incremental updates within a run of successful asks, or index features relative to the root player and never refresh).

On NEON with `M = 256`: the accumulator is `256 × int16 = 512 B` = 32 `int16x8_t` registers' worth. AArch64 has 32 vector registers, so a full accumulator *just* fits; with `M = 128` it comfortably fits and updates are pure register arithmetic. Each `W[:,f]` column read is 512 B (8 cache lines) — sequential, prefetchable. Estimated cost: **~20–60 ns per feature delta**, so ~10 deltas ≈ 0.2–0.6 µs. If that is too slow, drop to `M = 128`.

### 8.3 Quantisation scheme (transcribed from the Stockfish `nnue.md` reference)

| Tensor | Type | Scale |
|---|---|---|
| Feature-transformer weights & biases | `int16` | ×127 |
| Accumulator | `int16` | ×127 (activation scale `s_A = 127`) |
| ClippedReLU output | `int8`, range `[0, 127]` | ×127 |
| Linear-layer weights | `int8` | `s_W = 64` |
| Linear-layer biases | `int32` | `s_A · s_W` |
| Output | `int32` → scaled by `s_O` | domain-specific |

Forward pass of a linear layer: accumulate `int8 × int8 → int32`, then

```
y_quantised = (Σ_j w_ij x_j + b_i) / s_W ,   then ClippedReLU: clamp(y, 0, 127)
```

NEON: `vdotq_s32(acc, a_int8x16, b_int8x16)` computes 16 int8 MACs into 4 int32 lanes per instruction (`sdot`, present on all Apple M-series). With 4-way unrolling over output neurons that is ~64 MACs/cycle. A `256→32→32→out` head is ≈ 9 200 MACs ≈ **0.1–0.3 µs** realistically.

Aggregate estimate for a Fish evaluation on one P-core: **0.3–1.0 µs** including the accumulator delta. That is ~1–3 M evaluations/s/core, ~15–45 M/s machine-wide — enough for AlphaZero-style search on CPU, which is the whole point.

### 8.4 Feature design for Fish (proposal)

Root-player-relative, one-hot per (relative player, card):

- `KNOWN_HELD[p][c]` — 6 × 54 = 324
- `KNOWN_NOT_HELD[p][c]` — 324 (this is the *negative-information* channel that the ask history generates, and it is where most of Fish's signal lives)
- `HAND_SIZE[p][n]` — 6 × 10 = 60
- `HS_STATUS[h][s]` — 9 × 3 = 27 (undeclared / ours / theirs)
- `HS_KNOWN_COUNT[p][h][k]` — optional, 6 × 9 × 7 = 378
- turn-to-move one-hot — 6

Total ≈ 750–1 130 binary features, with ~200–400 active at any time. Since only 4–12 flip per event, the incremental update is genuinely cheap. Encode the feature deltas *in the move application function* — do not diff two feature vectors.

Heads: a value head (2 outputs: expected final set difference, and win probability), a policy head over the encoded action space (target ~140 logits: `3 opponents × 45 askable cards` plus declaration actions), and — following KataGo — **auxiliary heads**: (a) predict each opponent's hidden hand (a 6×54 ownership head, exactly analogous to KataGo's ownership target, and the highest-value auxiliary target for Fish), (b) predict the opponent's next ask.

### 8.5 Train in Python, infer in C++

Do **not** write your own backprop unless you must. The pragmatic split, which is what the Stockfish project does with `nnue-pytorch`:

- C++ engine emits training samples to a compact binary format (packed state + policy target + value target + auxiliary targets), one file per worker.
- PyTorch (CPU or MPS on the same Mac) trains a float model, then a quantisation/export step writes the int8/int16 tensors in the layout the C++ inference kernel expects.
- C++ loads the blob with `mmap` and runs NEON kernels.

If you do want in-process training (useful for fast, tight tuning loops on tiny nets), Adam is 12 lines. Kingma & Ba (ICLR 2015):

```
g_t   = ∇_θ L_t(θ_{t-1})
m_t   = β₁ m_{t-1} + (1 − β₁) g_t
v_t   = β₂ v_{t-1} + (1 − β₂) g_t²
m̂_t   = m_t / (1 − β₁^t)
v̂_t   = v_t / (1 − β₂^t)
θ_t   = θ_{t-1} − α · m̂_t / ( √v̂_t + ε )
```

Defaults `α = 1e-3, β₁ = 0.9, β₂ = 0.999, ε = 1e-8`. Use AdamW (decoupled decay: `θ_t ← θ_t − α λ θ_{t-1}`) rather than adding `λ‖θ‖²` to the loss. Note KataGo used plain **SGD with momentum 0.9** and a per-sample learning rate of `6e-5` (warmup `2e-5` for the first 5 M samples), batch size 256, plus stochastic weight averaging (snapshot every ~250 k samples, new net every 4 snapshots, decay 0.75) — for small nets on CPU, SGD+momentum+SWA is often more stable than Adam and cheaper per step (no second moment array).

---

## 9. Self-play data generation sized for a CPU-only budget

### 9.1 The AlphaZero loss and targets

```
L(θ) = (z − v_θ(s))²  −  π^T log p_θ(s)  +  c‖θ‖²
```

with `z` the game outcome from the current player's perspective, `π_a = N(s,a)^{1/τ} / Σ_b N(s,b)^{1/τ}` the search visit distribution. In Fish, replace the scalar `z ∈ {−1,+1}` with the **set differential** `(our sets − their sets)/9 ∈ [−1,1]`; it is a far denser learning signal than win/loss over a 9-set game, and it is exactly the analogue of KataGo's score target.

Root exploration noise:

```
P(s,a) ← (1 − ε) P(s,a) + ε · η_a ,      η ~ Dir(α · 1)
```

AlphaZero used `ε = 0.25` with `α = 0.03` (Go, ~362 moves), `0.3` (chess, ~35), `0.15` (shogi). The working heuristic is `α ≈ 10/|A|`; for Fish's ~40–140 legal asks, **`α ≈ 0.1–0.25`**. KataGo instead uses `α = 0.03 · 19²/N` scaled to board size and a root softmax temperature of 1.03.

Temperature: sample `a ~ π^{1/τ}` with `τ = 1` for the first `T` plies and `τ → 0` after. AlphaZero used `T = 30` plies. KataGo decays `τ` from 0.8 to 0.2 with a halflife equal to the board width. For Fish (60–120 decisions/game), a sensible schedule is `τ = 1.0` for the first ~15 decisions, then `τ = 0.3` decaying to 0.1.

### 9.2 KataGo's cheap wins (all non-domain-specific; ablation speedups in parentheses)

- **Playout cap randomisation (1.37×).** With probability `p = 0.25` do a full search (`N = 600`, annealed to 1000); otherwise a fast search (`n = 100`, annealed to 200). **Only full-search turns produce training rows.** This decouples "how many games do I need" from "how good must the policy target be" — the single best value-per-line-of-code change for a CPU budget.
- **Forced playouts + policy target pruning.** Force each child to at least
  ```
  n_forced(c) = sqrt( k · P(c) · Σ_{c'} N(c') ) ,     k = 2
  ```
  playouts, guaranteeing exploration proportional to the prior; then, before writing the policy target, find the max-visit child `c*` and subtract up to `n_forced(c)` visits from every other child, stopping before its PUCT value would exceed `c*`'s. This removes the exploration artefacts from the target so the policy is not trained to imitate its own noise.
- **Auxiliary targets (1.30–1.65×).** Opponent-next-move head at weight 0.15; ownership head at weight `1.5/b²`; score-distribution heads at 0.02. The Fish analogue — a per-card ownership head — is a *huge* auxiliary signal because it directly supervises the belief model.
- **Global pooling (1.60×).** Concatenate (mean, scaled mean, max) per channel into later layers. In an MLP-shaped Fish net this becomes: pool over the 6 players / 9 half-suits and feed the pooled summary into the head.
- **Sublinear data window.** With `N_total` total training rows generated so far,
  ```
  N_window = c · ( 1 + β · ( (N_total/c)^α − 1 ) / α ) ,     c = 250 000, α = 0.75, β = 0.4
  ```
  chosen so `f(c) = c` and `f'(c) = β`. Train on a uniform sample from the most recent `N_window` rows. KataGo's window grew from 250 k to ~22 M samples over the run.

Aggregate: KataGo reports a **~50× reduction in computation** versus comparable AlphaZero-style methods, surpassing ELF's final model in ~1.4 GPU-years versus ELF's 74.

### 9.3 Gumbel AlphaZero — the key finding for a machine with no GPU

Danihelka, Guez, Schrittwieser & Silver, *Policy Improvement by Planning with Gumbel* (ICLR 2022). AlphaZero's PUCT gives no policy-improvement guarantee at small `n`; Gumbel planning does, at `n = 2`.

**Algorithm (root):**

```
Sample g ∈ R^k ~ Gumbel(0)
A_topm = argtop( g + logits , m )                       # Gumbel-Top-k: sampling m actions w/o replacement
Run Sequential Halving with n simulations over A_topm,
    comparing  g(a) + logits(a) + σ( q̂(a) )
A_{n+1} = argmax_{a ∈ Remaining}  g(a) + logits(a) + σ( q̂(a) )
```

Sequential Halving divides the budget `n` equally across `log₂(m)` phases; within a phase every surviving action is visited equally often; after each phase half are eliminated. It has **no problem-dependent hyper-parameters** (the authors found it easier to tune than UCB-E and UCB√·).

**The σ transformation:**

```
σ( q̂(a) ) = ( c_visit + max_b N(b) ) · c_scale · q̂(a) ,       c_visit = 50, c_scale = 1.0
```

The same constants worked across all simulation budgets in their experiments.

**Completed Q-values and the training target:**

```
completedQ(a) = q(a)                         if N(a) > 0
              = v_π = Σ_a π(a) q(a)          otherwise   (use the value-net estimate v̂_π)

π' = softmax( logits + σ( completedQ ) )
L_completed(π) = KL( π' , π )
```

**Non-root action selection** (deterministic, minimises MSE between `π'` and normalised visit counts):

```
argmax_a  [ π'(a) − N(a) / (1 + Σ_b N(b)) ]
```

**Results.** On 9×9 Go with training budgets `n ∈ {2,4,16,32,200}` (all evaluated at 800 simulations), Gumbel MuZero substantially outperformed MuZero, TRPO-MuZero and MPO-MuZero, with the gap largest at small `n`. Sensitivity to `m ∈ {4,8,16,32}` was mild.

**Why this matters for Fish:** with ~40–140 legal actions and a CPU-only budget, `m = 16, n = 16–32` is a *complete* search that still guarantees improvement. Compared to a 400-simulation PUCT search, that is a **12–25× throughput multiplier on self-play games** at similar or better policy quality — the largest single lever available.

### 9.4 Concrete budget for a 15-core Apple Silicon machine

Assume 12 usable P-core-equivalents after OS overhead.

| Quantity | Estimate |
|---|---|
| Random bitboard playout (60–120 decisions) | 1.5–3 µs |
| PUCT iteration (select depth ~8 + NNUE eval + backup) | 5–15 µs |
| Gumbel search, `m=16, n=32`, per decision | 0.2–0.5 ms |
| PUCT search, `n=400`, per decision | 2–6 ms |
| Self-play game, Gumbel n=32, ~100 decisions | 20–50 ms |
| Self-play game, PUCT n=400 | 200–600 ms |
| **Games/hour, 12 workers, Gumbel n=32** | **0.9–2.2 M** |
| **Games/hour, 12 workers, PUCT n=400** | **72 k – 216 k** |
| Training rows/game (with playout-cap randomisation, p=0.25) | ~25 |
| Rows/hour, Gumbel | 22–55 M |

**Recommended loop:** generation = 1–2 hours of self-play (≈ 2–20 M rows), window per §9.2, batch 256–1024, ~1–3 epochs' worth of gradient steps over the window sampled uniformly, SWA every ~250 k samples, then a 2 000-game gated arena match (accept if win rate > 55 %, per AlphaGo Zero's threshold) or ungated continuous training (AlphaZero style — simpler and usually fine). Replay buffer: keep raw rows on disk (`mmap`-ed, ~64–128 B/row packed), never in RAM as Python objects.

---

## 10. Tuning a handful of continuous policy weights

You will have 3–12 continuous knobs: `c_puct`, `c_visit`/`c_scale`, determinization count, declaration-risk threshold, belief-prior temperature, virtual-loss/`O` weight `β`, ask-value blending weights. Four families of methods apply.

### 10.1 CMA-ES — full equations (Hansen, *The CMA Evolution Strategy: A Tutorial*, arXiv:1604.00772)

Sample, for `k = 1..λ`:

```
z_k = N(0, I)
y_k = B D z_k  ~ N(0, C)                                   (C = B D² Bᵀ, eigendecomposition)
x_k = m + σ y_k  ~ N(m, σ² C)
```

Selection & recombination (`x_{i:λ}` = i-th best):

```
⟨y⟩_w = Σ_{i=1}^{μ} w_i y_{i:λ} ,   Σ_{i=1}^{μ} w_i = 1
m ← m + c_m σ ⟨y⟩_w
```

Step-size control (cumulative step-size adaptation):

```
p_σ ← (1 − c_σ) p_σ + sqrt( c_σ (2 − c_σ) μ_eff ) · C^{-1/2} ⟨y⟩_w
σ   ← σ · exp( (c_σ / d_σ) ( ‖p_σ‖ / E‖N(0,I)‖ − 1 ) )
```

Covariance adaptation (rank-one + rank-μ, with active/negative weights):

```
p_c  ← (1 − c_c) p_c + h_σ sqrt( c_c (2 − c_c) μ_eff ) ⟨y⟩_w
w°_i = w_i · ( 1 if w_i ≥ 0 else n / ‖C^{-1/2} y_{i:λ}‖² )
C    ← (1 + c₁ δ(h_σ) − c₁ − c_μ Σ w_j) C  +  c₁ p_c p_cᵀ  +  c_μ Σ_{i=1}^{λ} w°_i y_{i:λ} y_{i:λ}ᵀ
```

with

```
E‖N(0,I)‖ = sqrt(2) Γ((n+1)/2)/Γ(n/2) ≈ sqrt(n) (1 − 1/(4n) + 1/(21n²))
h_σ = 1 if ‖p_σ‖ / sqrt(1 − (1−c_σ)^{2(g+1)}) < (1.4 + 2/(n+1)) E‖N(0,I)‖ , else 0
δ(h_σ) = (1 − h_σ) c_c (2 − c_c)
μ_eff = ( Σ_{i=1}^{μ} w_i² )^{-1}
```

**Default parameters (Hansen 2016, Table 1):**

```
λ  = 4 + ⌊3 ln n⌋                      (increase freely)
w'_i = ln( (λ+1)/2 ) − ln i ,  i = 1..λ
μ  = ⌊λ/2⌋ = |{w_i > 0}|
c_m = 1

c_σ = (μ_eff + 2) / (n + μ_eff + 5)
d_σ = 1 + 2 max(0, sqrt((μ_eff − 1)/(n+1)) − 1) + c_σ

c_c = (4 + μ_eff/n) / (n + 4 + 2 μ_eff/n)
c₁  = α_cov / ((n + 1.3)² + μ_eff) ,                       α_cov = 2
c_μ = min( 1 − c₁ , α_cov (1/4 + μ_eff + 1/μ_eff − 2) / ((n+2)² + α_cov μ_eff/2) )
```

Negative weights are scaled by `min(α⁻_μ, α⁻_{μeff}, α⁻_{posdef})` where `α⁻_μ = 1 + c₁/c_μ`, `α⁻_{μeff} = 1 + 2μ⁻_eff/(μ_eff + 2)`, `α⁻_{posdef} = (1 − c₁ − c_μ)/(n c_μ)`. Positive weights are normalised to sum to 1.

Initialisation: `p_σ = p_c = 0`, `C = I`, `σ` ≈ 0.3 × the parameter range; the optimum should lie inside `m ± 3σ`.

**Applicability to Fish.** CMA-ES is the right tool if your objective is *low-noise* — e.g. optimising a differentiable-free surrogate, or optimising against a large fixed evaluation set with thousands of games per evaluation. It is the **wrong** tool for raw win-rate feedback: Coulom tested UH-CMA-ES (the uncertainty-handling variant explicitly designed for noise) on exactly this problem class and reported it *"was clearly not designed for that kind of problem, and it does not work well"* — it sometimes escaped the `[−1,1]` box even when started at 0 with small variance.

### 10.2 CLOP — the game-tuning specialist (Coulom, ACG 2011)

Problem: find `x* ∈ [−1,1]^n` maximising `f(x) ∈ [0,1]`, observed only through Bernoulli trials, minimising **simple regret** `f(x*) − f(x̃)`.

```
procedure QuadraticCLOP(H, (x₁,y₁) … (x_N,y_N)):
    w₀ ← λx.1 ;  W₀ ← N ;  k ← 0
    repeat
        w  ← λx. min_{i=0..k} w_i(x)
        k  ← k + 1
        q_k ← WeightedQuadraticLogisticRegression(w, data)     # f ≈ 1/(1 + e^{−q_k(x)})
        μ_k ← LogisticMean(w, data)                            # logistic regression by a constant
        σ_k ← ConfidenceDeviation(w, data)                     # posterior sd of that constant
        w_k ← λx. exp( (q_k(x) − μ_k) / (H σ_k) )
        W_k ← Σ_i min( w(x_i), w_k(x_i) )
    until W_k > 0.99 · W_{k−1}
    x_{N+1} ← Random(w)                                        # next sample ~ w, via Gibbs sampling
    x̃      ← Σ_i w(x_i) x_i / Σ_i w(x_i)                       # recommendation = weighted mean
```

Both regressions take the MAP under a Gaussian prior of variance 100. The single meta-parameter `H` trades bias against variance; the asymptotically optimal setting is `H = O(N^{1/6})`, giving simple regret `O(N^{-2/3})` — which is **provably optimal** for functions with bounded third derivatives (Chen). Coulom shows `H = 3` constant works well across a wide range.

**Empirical comparison** (1 000 replications, up to 10⁷ samples): CLOP beat RSPSA, hand-tuned SPSA, both cross-entropy variants, and UCT-over-binary-partitions on all smooth problems (Log, Log2, Log5, Flat, Rosenbrock, Correlated, Power); it tied on Angle and lost on Step (a discontinuity). Cost: for 10⁷ samples on a 24-core PC, CEM took 1'20", CLOP 9'57", UCT 21'19" — *negligible compared with playing the games*. Speed tricks: after fitting on N samples, collect `1 + N/10` more before refitting; take several replications at the same location.

**Verdict for Fish: CLOP is the default recommendation** for 1–8 continuous parameters tuned against actual game outcomes. It handles indefinite Hessians (a parameter with no effect) and extreme noise, which is exactly the regime you are in.

### 10.3 Bayesian optimisation (what AlphaGo actually used)

Chen, Huang, Wang, Antonoglou, Schrittwieser, Silver & de Freitas, *Bayesian Optimization in AlphaGo* (arXiv:1812.06855). GP surrogate + Expected Improvement:

```
EI(θ) = E_f[ max( f(θ) − f*, 0 ) ] = (μ(θ) − f*) Φ(z) + s(θ) φ(z) ,   z = (μ(θ) − f*)/s(θ)
```

Crucially they exploited the Bernoulli structure rather than fitting an unknown noise level:

```
ε̂ = sqrt( p̄ (1 − p̄) / N )
```

and used only **N = 50 games per evaluation** (versus 1 000 games / 6.7 hours per setting for the grid search it replaced). They tuned 3–10 hyper-parameters per task across five tasks (MCTS/UCT exploration constants, node-expansion thresholds, distributed-MCTS parameters, softmax annealing temperatures, rollout/value-net mixing ratios, time-control parameters). Reported gains: **50 % → 66.5 %** self-play win rate for the version used against Lee Sedol; individual MCTS-tuning iterations of **+94 and +103 Elo** (63.2 % / 64.4 % win rates); fast-player iterations of **+300, +285, +145, +129 Elo**. Manual grid search over 6 parameters × 5 values would have taken 8.3 days.

**Verdict:** use BO when evaluations are *expensive and few* (≤ a few hundred), and when you want to tune 5–10 knobs jointly. Use CLOP when evaluations are cheap and numerous.

### 10.4 Population-Based Training

Wu, Wei & Wu, *Accelerating and Improving AlphaZero Using Population Based Training* (AAAI 2020, arXiv:2003.06212). Instead of a separate tuning run per configuration, run a population of workers sharing one self-play stream; periodically **exploit** (copy weights+hyper-parameters from a better member) and **explore** (perturb hyper-parameters). Since self-play record generation is shared, the extra cost is only in the optimisation step. Results: higher win rate than baselines on 9×9 Go; on 19×19 the PBT agent reached **74 %** vs ELF OpenGo where the saturated non-PBT agent reached **47 %**.

**Verdict for Fish:** attractive because your self-play generation dominates the cost and is shared. With 12 workers you can run a population of 4–6 configurations differing in `c_puct`/`c_visit`/temperature and let PBT anneal them, at essentially no extra self-play cost.

### 10.5 The statistics that actually gate you

Win-rate standard error over `N` games: `s = sqrt(p(1−p)/N) ≈ 0.5/√N`. Elo: `ΔElo = −400 log₁₀(1/p − 1)`, so near p = 0.5, `Δp ≈ ΔElo/1386`.

| Difference to resolve | Δp | N for 95 % significance | N for 80 % power |
|---|---|---|---|
| 50 Elo | 0.0361 | ≈ 740 | ≈ 1 510 |
| 20 Elo | 0.0144 | ≈ 4 630 | ≈ 9 450 |
| 10 Elo | 0.0072 | ≈ 18 500 | ≈ 37 800 |

**Variance reduction is worth more than more cores.** In a card game you can use *duplicate deals*: play each deal twice with the two teams swapped, and hold the determinization/rollout RNG counters fixed across the two arms (§7.1). Score the pair difference. This removes deal luck — historically the single largest variance component in trick-taking-game evaluation — and typically cuts the required `N` by 2–5×. Implement this before you implement anything else in the tuning harness.

---

## 11. Pitfalls, negative results and known failure modes

1. **Virtual loss degrades lock-free tree parallel MCTS** (Mirsoleimani et al. 2017, across all `C_p` and 2–64 threads). Prefer WU-UCT's `O_s` statistic. If you must use virtual loss, tune `n_vl` — it is not a free constant.
2. **Root parallelism's super-linear speedups are partly an artefact of UCT escaping local optima** (Chaslot et al. 2008; Soejima et al. 2010). Do not conclude your parallel implementation is magic; test against a sequential run with `nThreads ×` the time.
3. **Transposition sharing leaks information across parents.** Without the MCGS `Q_φ` correction, the phenomenon *"occurs more frequently as the number of simulations increases and makes this approach unstable."* Also: sharing across an *information-set* DAG when beliefs differ is silently wrong.
4. **Determinization causes strategy fusion.** SO-ISMCTS treats all opponent moves as fully observable and therefore assumes it can respond differently to indistinguishable opponent actions. In Fish this matters most around declarations. MO-ISMCTS (one tree per player, moves from each player's viewpoint) is the principled fix; SO-ISMCTS+POM fixes fusion but reduces the opponent to random over indistinguishable moves.
5. **Float accumulation across threads is not reproducible.** `W += Δ` in float has scheduling-dependent rounding. Use fixed-point integers in the packed `(W,N)` atomic. Similarly, never let the *number* of RNG draws depend on thread timing.
6. **False sharing on visit counters** is the classic parallel-MCTS performance cliff: two hot nodes in one 64-byte line ping-pong the line between cores. Pad/align hot node headers; keep the huge `eP` prior array separate from the hot `eWN` array.
7. **`std::mutex` per node is slower than it looks on macOS.** Chaslot measured global-mutex tree parallelism *losing* strength from 4 to 16 threads (3.0 → 2.6). Go lock-free or go root-parallel; do not go fine-grained-lock.
8. **Heterogeneous cores + barriers = wasted cycles.** Never put an E-core in a fork-join barrier with P-cores. Set QoS explicitly; the default may confine you to E-cores entirely (a documented failure mode in clangd and Firefox on Apple Silicon).
9. **Oversubscription hurts on Apple Silicon.** Reported cases of 16 threads being slower than 12 on M4 Max. Measure; do not assume `hardware_concurrency()` is optimal.
10. **`Q_ε`, `c_visit`, `c_puct` interact with the simulation budget.** Mirsoleimani's data shows tree depth collapsing from 56 to 5 as `C_p` goes 0 → 1, with sequential runtime dropping 60 s → 21 s purely from shallower `SELECT`/`BACKUP` loops. Your "nodes per second" is a function of your exploration constant; never compare NPS across different `C_p`.
11. **CMA-ES on raw win rates fails** (Coulom's UH-CMA-ES result). SPSA works only with hand-tuned meta-parameters that do not transfer (`SPSA*` was excellent on `Log` and useless elsewhere).
12. **Tuning against a single fixed opponent overfits.** Especially in a team game where your teammate is also your bot: co-adaptation to a frozen partner produces conventions that collapse against anything else. Evaluate against a *pool* (previous generations + rule-based baselines) and hold out one opponent.
13. **Node-count explosion.** A 140-branching-factor game with 100 decisions will eat memory fast. Cap the arena, and when it fills, either stop growing (search within the existing tree) or reuse the subtree under the played move and discard the rest. Do not `shrink_to_fit` mid-search.
14. **Python is not an option in the hot loop.** OpenSpiel's Python-wrapped C++ measured roughly 10²–10⁴ steps/s for chess and 10³–10⁴ for 19×19 Go, versus 10⁵–10⁶ for a vectorised JAX implementation on an A100 (Koyamada et al., Pgx, NeurIPS 2023 D&B). Your C++ bitboard engine should be in the 10⁵–10⁶ decisions/s/core range; if it is not, profile before adding a neural net.
15. **PDFs of key papers hide their tables from naive scraping** — verify every number you copy from a secondary source against the primary. (Several of the numbers in this report were only recoverable by extracting the PDF text directly.)

---

## 12. Recommended concrete build

| Component | Choice | Numbers |
|---|---|---|
| State | Single `uint64_t` per hand, `c = 6h+i` | 72-byte state, copy not undo |
| Belief | `6 × uint64_t` `may[]` + hand sizes + `atLeastOne` bitmask | 66 bytes; propagation < 100 ns |
| Determinization | Scarcity-ordered sampler + arc consistency | 200–800 ns/sample |
| Hashing | Zobrist over (player,card) + separate info-set key over public constraints | incremental XOR |
| Node memory | SoA edge arrays, contiguous child blocks, bump arena | 8 B/edge hot, 64-B aligned blocks |
| Stats | Packed `(W:int32, N:uint32)` in one `std::atomic<uint64_t>`, `memory_order_relaxed` | 1 instruction per backup |
| TT | Bucketed, Hyatt XOR-lockless, two-tier replacement, 64-B buckets | 4 entries/line |
| Graph search | MCGS with `Q_ε = 0.01` and `Q_φ` clipping | 30–70 % memory saving |
| Search (self-play) | Gumbel: `m = 16`, `n = 16–32`, `c_visit = 50`, `c_scale = 1.0` | 0.2–0.5 ms/decision |
| Search (match) | WU-UCT with `O_s`, or root-parallel 12 trees | 14.9×/16 threads (root, measured) |
| Parallelism (self-play) | One game per thread, no shared state | linear |
| RNG | splitmix64 seeding + xoshiro256++ streams, keyed by `(run,gen,game,ply,purpose)` | 0.63 / 0.75 ns per 64 bits |
| NN | NNUE-style: ~900 sparse features → `256×2` int16 accumulator → 32 → 32 → {value, policy, ownership} int8 | 0.3–1.0 µs/eval |
| Training | PyTorch (float) → quantised export → NEON `sdot` inference | SGD+mom 0.9, per-sample LR 6e-5, batch 256, SWA |
| Pipeline | Playout cap randomisation `p=0.25`, forced playouts `k=2` + target pruning, window `c=250k, α=0.75, β=0.4` | ~25 rows/game |
| Throughput | 12 workers × Gumbel n=32 | ~1–2 M games/hour |
| Tuning | CLOP (`H = 3`) for ≤ 8 knobs; BO/EI for 5–10 expensive knobs; PBT riding the self-play stream | 50 games/eval (BO), duplicate deals always |

---

## 13. Bibliography

**Parallel MCTS**

1. Chaslot, G.M.J-B.; Winands, M.H.M.; van den Herik, H.J. *Parallel Monte-Carlo Tree Search.* Computers and Games (CG 2008), LNCS 5131, pp. 60–71, Springer, 2008. https://dke.maastrichtuniversity.nl/m.winands/documents/multithreadedMCTS2.pdf (also https://link.springer.com/chapter/10.1007/978-3-540-87608-3_6)
2. Mirsoleimani, S.A.; van den Herik, H.J.; Plaat, A.; Vermaseren, J. *A Lock-free Algorithm for Parallel MCTS.* ICAART 2018, SciTePress. https://liacs.leidenuniv.nl/~plaata1/papers/paper_ICAART18.pdf
3. Mirsoleimani, S.A.; Plaat, A.; van den Herik, H.J.; Vermaseren, J. *An Analysis of Virtual Loss in Parallel MCTS.* ICAART 2017, pp. 648–652, SciTePress. DOI 10.5220/0006205806480652. https://www.scitepress.org/papers/2017/62058/62058.pdf
4. Mirsoleimani, S.A.; Plaat, A.; Vermaseren, J.; van den Herik, H.J. *Structured Parallel Programming for Monte Carlo Tree Search.* arXiv:1704.00325, 2017. https://arxiv.org/abs/1704.00325 (3PMCTS pipeline pattern; playout-speedup ≈ 21 on 24 cores — figure quoted from the search abstract, **UNVERIFIED against the PDF**)
5. Liu, A.; Chen, J.; Yu, M.; Zhai, Y.; Zhou, X.; Liu, J. *Watch the Unobserved: A Simple Approach to Parallelizing Monte Carlo Tree Search.* ICLR 2020. arXiv:1810.11755. https://arxiv.org/abs/1810.11755 ; code https://github.com/liuanji/WU-UCT
6. Enzenberger, M.; Müller, M. *A Lock-free Multithreaded Monte-Carlo Tree Search Algorithm.* ACG 12, 2009. https://webdocs.cs.ualberta.ca/~mmueller/ps/enzenberger-mueller-acg12.pdf (**UNVERIFIED** — cited but not fetched in this review)
7. Soejima, Y.; Kishimoto, A.; Watanabe, O. *Evaluating Root Parallelization in Go.* IEEE TCIAIG 2(4), 2010. (Cited via Liu et al. and Mirsoleimani et al.; **UNVERIFIED** — not fetched directly.)

**Imperfect information search**

8. Cowling, P.I.; Powley, E.J.; Whitehouse, D. *Information Set Monte Carlo Tree Search.* IEEE Transactions on Computational Intelligence and AI in Games 4(2), pp. 120–143, 2012. DOI 10.1109/TCIAIG.2012.2200894. https://eprints.whiterose.ac.uk/id/eprint/75048/1/CowlingPowleyWhitehouse2012.pdf
9. Powley, E.J.; Cowling, P.I.; Whitehouse, D. *Information capture and reuse strategies in Monte Carlo Tree Search, with applications to games of hidden information.* Artificial Intelligence 217, pp. 92–116, 2014. https://www.sciencedirect.com/science/article/pii/S0004370214001052 (**UNVERIFIED** — publisher returned HTTP 403; content described from the search abstract only.)
10. Czech, J.; Korus, P.; Kersting, K. *Monte-Carlo Graph Search for AlphaZero.* arXiv:2012.11045, 2020. https://arxiv.org/abs/2012.11045
11. Childs, B.E.; Brodeur, J.H.; Kocsis, L. *Transpositions and Move Groups in Monte Carlo Tree Search.* IEEE CIG 2008, pp. 389–395. https://ieeexplore.ieee.org/document/5035667/ (**UNVERIFIED** — abstract only.)
12. Saffidine, A.; Cazenave, T.; Méhat, J. *UCD: Upper Confidence bound for rooted Directed acyclic graphs.* Knowledge-Based Systems 34, 2012 / IEEE TAAI 2010. https://www.sciencedirect.com/science/article/abs/pii/S0950705111002504 (**UNVERIFIED** — abstract only.)
13. Vasconcelos, N. et al. *Modeling Uncertainty: Constraint-Based Belief States in Imperfect-Information Games.* arXiv:2507.19263v2, 2025. https://arxiv.org/html/2507.19263v2
14. Ginsberg, M.L. *GIB: Imperfect Information in a Computationally Challenging Game.* JAIR 14, 2001. arXiv:1106.0669. https://arxiv.org/pdf/1106.0669 (constrained deal generation for bridge)

**Self-play learning and search-budget efficiency**

15. Wu, D.J. *Accelerating Self-Play Learning in Go.* arXiv:1902.10565, 2019 (KataGo). https://arxiv.org/abs/1902.10565 ; HTML v5 https://arxiv.org/html/1902.10565v5
16. Danihelka, I.; Guez, A.; Schrittwieser, J.; Silver, D. *Policy Improvement by Planning with Gumbel.* ICLR 2022. https://openreview.net/forum?id=bERaNdoegnO ; algorithm and results transcribed from Danihelka, I. *Planning and Policy Improvement* (PhD thesis, UCL, 2023), Chapter 5: https://discovery.ucl.ac.uk/id/eprint/10167022/2/ivo_danihelka_thesis.pdf
17. Zha, D.; Xie, J.; Ma, W.; Zhang, S.; Lian, X.; Hu, X.; Liu, J. *DouZero: Mastering DouDizhu with Self-Play Deep Reinforcement Learning.* ICML 2021, PMLR 139. arXiv:2106.06135. https://arxiv.org/abs/2106.06135 ; https://proceedings.mlr.press/v139/zha21a/zha21a.pdf (45 parallel actors, 48 CPU cores + 4×1080Ti; beats DeltaDou in 10 days)
18. Tian, Y. et al. *ELF OpenGo: An Analysis and Open Reimplementation of AlphaZero.* arXiv:1902.04522, 2019. https://arxiv.org/pdf/1902.04522 (**UNVERIFIED** — referenced via KataGo and Pgx, not fetched.)
19. Koyamada, S. et al. *Pgx: Hardware-Accelerated Parallel Game Simulators for Reinforcement Learning.* NeurIPS 2023 Datasets & Benchmarks. arXiv:2303.17503. https://arxiv.org/pdf/2303.17503 (OpenSpiel vs JAX simulation-throughput comparison)
20. Laurent, J. *AlphaZero.jl parameter reference.* https://jonathan-laurent.github.io/AlphaZero.jl/v0.5/reference/params/ (documents AlphaGo Zero's 1600 sims/move, τ=1 for 30 moves, ε=0.25, α=0.03, batch 2048, L2 1e-4, 500 k-game buffer, 55 % arena threshold, 400 arena games)

**Neural network engineering**

21. Stockfish contributors. *NNUE architecture and quantisation reference.* https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md ; rendered: https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html
22. Chess Programming Wiki. *NNUE* / *Stockfish NNUE.* https://www.chessprogramming.org/NNUE , https://www.chessprogramming.org/Stockfish_NNUE
23. Kingma, D.P.; Ba, J. *Adam: A Method for Stochastic Optimization.* ICLR 2015. arXiv:1412.6980. https://arxiv.org/abs/1412.6980
24. Kiessling, T. et al. *Apple vs. Oranges: Evaluating the Apple Silicon M-Series SoCs for HPC Performance and Efficiency.* arXiv:2502.05317, 2025. https://arxiv.org/pdf/2502.05317
25. *Above the Inner Loop: Exceeding Accelerate at LLM Prefill GEMM on the M1 AMX.* arXiv:2606.25426. https://arxiv.org/html/2606.25426 (vDSP SGEMM 0.90/1.09/1.38/1.49 TFLOPS on M1/M2/M3/M4; AMX on M1–M3, SME on M4+)
26. *Litespark Inference For CPUs: Ultra-Fast SIMD Framework for Ternary Language Models.* arXiv:2605.06485. https://arxiv.org/html/2605.06485 (NEON `sdot` int8 kernels on Apple Silicon; specific throughput multipliers **UNVERIFIED** — from search snippet only)
27. Apple. *Energy Efficiency Guide for Mac Apps: Prioritize Work at the Task Level* (QoS classes). https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/PrioritizeWorkAtTheTaskLevel.html

**Random number generation**

28. Salmon, J.K.; Moraes, M.A.; Dror, R.O.; Shaw, D.E. *Parallel Random Numbers: As Easy as 1, 2, 3.* SC11, ACM, 2011. Library docs: https://www.thesalmons.org/john/random123/releases/latest/docs/index.html
29. Blackman, D.; Vigna, S. *Scrambled Linear Pseudorandom Number Generators.* arXiv:1805.01407 (ACM TOMS 2021). https://arxiv.org/pdf/1805.01407 ; shootout table and reference C: https://prng.di.unimi.it/ , https://prng.di.unimi.it/xoshiro256plusplus.c , https://prng.di.unimi.it/splitmix64.c
30. Lemire, D. *Testing non-cryptographic random number generators: my results.* 2017. https://lemire.me/blog/2017/08/22/testing-non-cryptographic-random-number-generators-my-results/

**Parameter tuning**

31. Hansen, N. *The CMA Evolution Strategy: A Tutorial.* arXiv:1604.00772 (2016, rev. 2023). https://arxiv.org/abs/1604.00772 (Figure 6 algorithm summary and Table 1 defaults transcribed above)
32. Coulom, R. *CLOP: Confident Local Optimization for Noisy Black-Box Parameter Tuning.* Advances in Computer Games (ACG 13), LNCS 7168, pp. 146–157, Springer, 2012. https://www.remi-coulom.fr/CLOP/CLOP.pdf ; project page https://www.remi-coulom.fr/CLOP/
33. Chen, Y.; Huang, A.; Wang, Z.; Antonoglou, I.; Schrittwieser, J.; Silver, D.; de Freitas, N. *Bayesian Optimization in AlphaGo.* arXiv:1812.06855, 2018. https://arxiv.org/abs/1812.06855 ; https://ar5iv.labs.arxiv.org/html/1812.06855
34. Wu, T.-R.; Wei, T.-H.; Wu, I-C. *Accelerating and Improving AlphaZero Using Population Based Training.* AAAI 2020, pp. 1046–1053. arXiv:2003.06212. https://arxiv.org/abs/2003.06212 ; https://ojs.aaai.org/index.php/AAAI/article/view/5454
35. Spall, J.C. *Multivariate stochastic approximation using a simultaneous perturbation gradient approximation.* IEEE TAC 37, pp. 332–341, 1992. (SPSA; cited via Coulom, **UNVERIFIED** — not fetched.)
36. Kocsis, L.; Szepesvári, C. *Universal parameter optimisation in games based on SPSA.* Machine Learning 63(3), pp. 249–286, 2006. (RSPSA; cited via Coulom, **UNVERIFIED** — not fetched.)
37. Jaderberg, M. et al. *Population Based Training of Neural Networks.* arXiv:1711.09846, 2017. (**UNVERIFIED** — cited as the origin of PBT via Wu et al., not fetched directly.)

**Foundations / background**

38. Kocsis, L.; Szepesvári, C. *Bandit Based Monte-Carlo Planning.* ECML 2006, LNAI 4212, pp. 282–293. (UCT; source of the `UCT(j) = X̄_j + 2C_p sqrt(2 ln N(v) / N(v_j))` form used above, quoted via Mirsoleimani et al. 2018.)
39. Browne, C. et al. *A Survey of Monte Carlo Tree Search Methods.* IEEE TCIAIG 4(1), pp. 1–43, 2012. (**UNVERIFIED** — cited via other papers.)
40. Świechowski, M.; Godlewski, K.; Sawicki, B.; Mańdziuk, J. *Monte Carlo Tree Search: A Review of Recent Modifications and Applications.* arXiv:2103.04931. https://arxiv.org/pdf/2103.04931
41. Chess Programming Wiki. *Shared Hash Table* (lockless XOR trick, Hyatt & Mann) and *Zobrist Hashing.* https://www.chessprogramming.org/Shared_Hash_Table , https://chessprogramming.org/Zobrist_Hashing
42. Hyatt, R.; Cozzie, A. *The Effect of Hash Collisions in a Computer Chess Program.* https://craftychess.com/hyatt/collisions.html
43. Haglund, B. et al. *DDS — Double Dummy Solver for Bridge.* https://github.com/dds-bridge/dds (reference implementation of a high-performance C++ card-game engine with per-thread transposition tables reused within a batch and dynamic memory management)
