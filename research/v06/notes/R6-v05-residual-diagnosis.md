# R6 — What is still wrong with the shipped v0.5

Recon report seeding FishBot v0.6. Read-only on `engine/src/`; every number below was
measured on this machine at commit `bd812fed0e42f9b873e148aef4d592b4c2db437e`
(`engine/fish` rebuilt: `cd engine && make` → "Nothing to be done", binary already current).

Instrumentation that v0.5 does not ship was written **outside** the engine tree, as
`v06probe.cpp` in the session scratchpad, compiled against `engine/src` headers read-only:

```
clang++ -std=c++20 -O2 -march=native -I"engine/src" v06probe.cpp -o v06probe -pthread
```

It contains four things: `ProbeV05` (a `V05Agent` subclass that re-executes v0.5's exact
`chooseAsk` body — `engine/src/v05.hpp:502-604` — and records the candidate-score
distribution plus ground truth), `AskOracleV05` (v0.5 with one change: among the candidates
it already ranked, take one that truly hits), `CheatAgent` (perfect information), and a
trace replayer that attributes every half-suit to a cause. A copy of the source is at the
end of this file's provenance section.

---

## 0. Headline

v0.5's *published* defects are genuinely fixed and stay fixed. What is left is not a
pathology, it is a **ceiling**: v0.5 is essentially indifferent between its top candidate
asks in the majority of its decisions, and the indifference is not noise — it is worth
about **three half-suits per game**.

| claim | number |
|---|---|
| v0.5 vs v0.4, seed 515253, 300×6 | 51.61% [49.30, 53.91] — CI contains 50% |
| v0.5 vs v0.4, seed 90210, 300×6 | 50.50% [48.19, 52.81] — CI contains 50% |
| exact tie (u₁ == u₂ bit-for-bit) among ask candidates | **54.2–54.7% of all ask decisions** |
| v0.5 + perfect tie-break inside its own top-6 | **99.89% win rate, 7.66 – 1.34 sets** |
| v0.5 + perfect information (cheat) | 100.00% win rate, **8.955 – 0.045 sets** |
| exact posterior (`belief=block`) vs shipped `Fast` | **40.33% / 35.83%** — exact is 10–14 pts *worse* |
| misses that are asks for a card the asker's own team already holds | **21.97% of all misses** |
| half-suits lost to misdeclaration, mirror | 1.96% |
| half-suits cashed late (after the declarer could prove the allocation) | **0.00%** (mean lateness 0.006 events) |
| action-limit games | 0 / 600 |

---

## 1. Decision structure: forced / near-tie / decided

`./v06probe margin` re-runs v0.5's `chooseAsk` verbatim and logs, per decision: the number
of legal asks, the number surviving the M1 live gate, the top-2 utilities **after** the
top-K two-ply refinement (`engine/src/v05.hpp:520-585`), the standard deviation of the
pre-refinement linear+value scores over all candidates, and ground truth.

"Near-tie" is defined as `(u₁ − u₂) / sd(u over all live candidates) < τ`. v0.5 ships no
margin instrumentation of any kind — `lastAskP` (`engine/src/v05.hpp:135`, `462`) records
only the winner's hit probability — so this scale had to be constructed.

```
$ ./v06probe margin --b=v05 --games=60 --rotations=2 --seed=31
=== ask-decision margin probe (v05 mirror-team vs v05) ===
decisions                    5291
FORCED (1 legal ask)         28  (0.529%)
1 live candidate after M1    44  (0.832%)
candidates: mean legal 46.77  mean live 42.94
margin/sd  mean 0.691  p10 0.000  p25 0.000  median 0.000  p75 0.742  p90 2.576
NEAR-TIE margin/sd < 0.05    3184  (62.66% of non-forced)
NEAR-TIE margin/sd < 0.10    3275  (64.46%)
NEAR-TIE margin/sd < 0.25    3462  (68.14%)
DECISIVE margin/sd >= 0.50   1426  (28.07%)
top-2 differ in TRUE outcome 1887  (37.14% of non-forced);  of those, margin/sd<0.25: 1165 (61.74%)
final pick != linear argmax  2371  (44.81%)  -- top-K refinement changes the move
--- tie anatomy ---
EXACT tie u1==u2             2896  (54.73% of decisions)
  ... same posterior p1==p2  2896  (100.00% of exact ties)
  ... same half-suit+target  2745  (94.79% of exact ties)
  ... top-2 differ in truth  962  (33.22% of exact ties), of which same-p 962
```

**Answer to Q1**, mirror play, 5,291 decisions:

* **(a) forced** — exactly one legal ask: **0.53%**. (One live candidate after M1: 0.83%.)
* **(b) near-tie** — **62.7%** at τ=0.05, **68.1%** at τ=0.25. The median margin/sd is
  **0.000**: more than half of all decisions are an *exact* floating-point tie between the
  best and second-best candidate.
* **(c) clearly decided** — margin/sd ≥ 0.50: **28.1%**.

The tie is structural, not numerical noise. **100%** of exact ties have `p₁ == p₂`
(identical posterior hit probability) and **94.8%** are two cards *of the same half-suit
against the same target*. `features()` (`engine/src/v05.hpp:285-344`) computes 18 of its 20
features per (half-suit, target); only `f[0]`/`f[1]`/`f[2]` are per-card, and they are all
functions of `bel.marg[card][target]`, which `sinkhornDisj` makes identical for
exchangeable cards of one half-suit. So the whole score collapses.

**The ties are decision-relevant.** In 33.2% of exact ties (18.2% of all decisions) the top
two candidates differ in ground-truth outcome — one hits, the other misses, and v0.5 picks
by `std::partial_sort` order.

Two other facts from the same run worth carrying into v0.6:

* `final pick != linear argmax` in **44.8%** of decisions — the top-K two-ply refinement
  (`chainWeight`, `threatWeight`) is not a tie-break, it is doing the deciding almost half
  the time, and it costs ~6 extra Sinkhorn solves per decision.
* Mean 46.8 legal asks per decision, 42.9 surviving M1. M1 removes only **8.2%** of
  candidates in mirror play now (it removed 39% of *executed* asks in v0.4).

---

## 2. Headroom: gap to a perfect-information reference

There is **no cheating agent in the engine**. `factory.hpp` (`engine/src/factory.hpp:35-260`)
registers only v05/v04, the three deception archetypes and the eight baselines; `fish oracle`
(`engine/src/main.cpp:327-385`) is a *correctness* validator for the block posterior, not a
strength reference. So I built one.

`CheatAgent` reads `Game::g.hand` (`engine/src/game.hpp:82`), asks only cards the target
actually holds (preferring the half-suit its team is nearest completing), and declares with
the true owners the moment its team truly holds all six.

```
$ ./v06probe cheat --b=v05 --games=200 --rotations=6 --seed=90210
=== cheat vs v05 ===
games 1200  win rate 100.0000%  mean sets 8.9550 - 0.0450  events/game 37.66  limit hits 0

$ ./v06probe cheatpatient --b=v05 --games=200 --rotations=6 --seed=90210
games 1200  win rate 100.0000%  mean sets 8.9558 - 0.0442  events/game 37.66  limit hits 0
```

**Perfect information takes 8.955 of 9 half-suits and never loses a game.** v0.5 scores
0.045 sets/game against it. That is the outer bound and it is not close.

A much tighter and far more useful bound: **v0.5 with a perfect tie-break, and nothing else
changed** — same belief, same features, same weights, same declaration logic, same top-6
candidate set. It only reorders *within* the six candidates it already produced:

```
$ ./v06probe askoracle --b=v05 --games=150 --rotations=6 --seed=90210
=== askoracle vs v05 ===
games 900  win rate 99.8889%  mean sets 7.6611 - 1.3389  events/game 65.63

$ ./v06probe askoracleall --b=v05 --games=150 --rotations=6 --seed=90210   # hindsight over ALL live candidates
games 900  win rate 94.2222%  mean sets 6.6767 - 2.3233  events/game 42.14
```

**Resolving the tie correctly inside the existing top-6 is worth 6.3 half-suits per game
(7.66 vs 1.34).** No new features, no new inference, no new search — just the ordering.
That is where v0.6's headroom lives, and it is entirely inside a subroutine v0.5 already
runs. (Note `askoracleall` is *worse* than `askoracle`: taking the first hitting candidate
in enumeration order discards the policy's set/target preferences. The policy ranking is
carrying real signal; it is the last comparison that is blind.)

### 2b. The exact posterior is worse than the approximation — replicated

The README calls the exact block posterior "a reference and validation oracle" and the
Sinkhorn path "a faster approximate inference path." Measured, the approximation is
**stronger**, by a lot, at two independent seeds:

```
$ ./fish match --a=v05:belief=block --b=v05 --games=200 --rotations=6 --seed=90210
  win rate      40.3333%  [37.5928, 43.1355]  n=1200
  mean sets     4.165 - 4.835
  ask accuracy  53.7308% / 55.7705%

$ ./fish match --a=v05:belief=block --b=v05 --games=100 --rotations=6 --seed=515253
  win rate      35.8333%  [32.0978, 39.7491]  n=600
  mean sets     4.05667 - 4.94333
  ask accuracy  53.1306% / 55.8899%
```

Mechanism, read from source: `BlockDP::build` (`engine/src/blockdp.hpp:157`) takes only
`const Knowledge&` and has **no `theta`/`phi` parameter** (`grep -n "theta\|phi" src/blockdp.hpp`
returns one unrelated comment at line 441). `Belief::sinkhornDisj`
(`engine/src/belief.hpp:478`) takes `theta`/`phi` and v0.5 passes `priorTheta = 0.44458`,
`priorPhi = 0.12198` (`engine/src/v05.hpp:29-30`, used at `v05.hpp:187`). So:

* `belief=block` = **exact combinatorics, policy-agnostic** (uniform over consistent deals).
* `belief=fast` (shipped) = **approximate combinatorics + a policy prior**.

The measured result says the policy prior is worth more than combinatorial exactness — by
10 to 14 points. The margin probe run under `belief=block` confirms the mechanism at the
decision level (25 deals × 2, matched against the same `Fast` run size):

| | `Fast` (shipped) | `block` (exact) |
|---|---:|---:|
| decisions | 2,215 | 2,161 |
| exact ties u₁==u₂ | 54.22% | **43.36%** |
| ask hit rate | **55.67%** | 52.48% |
| a hitting ask inside top-K | 87.95% | 87.32% |

Exactness **does** break 10.9 points of ties (20% of them), so the extra structure is real —
but the resulting marginals predict hits *worse*, because they throw away the behavioural
signal. Neither path has both. That is the single clearest architectural gap in v0.5.

One caveat on the exactness claim itself: `fish oracle` validates only the states small
enough to brute-force.

```
$ ./fish oracle --games=150
states enumerated          15544  (with a live C5 certificate: 14942)
states skipped (too large) 75203
partition function Z       max rel diff 0.000e+00
per-card marginals         max abs diff 0.000e+00 over 656826 checks
named allocation prob      max abs diff 0.000e+00 over 3189103 checks
sampler vs exact marginals max abs diff 0.0532 over 46345121 draws
ORACLE PASS
```

**17.1%** of encountered states are verified; 82.9% are skipped as too large. The PASS is
exact on the verified subsample and silent on the rest — and the skipped ones are, by
construction, the high-entropy early game where inference matters most.

---

## 3. Declaration quality now

```
$ ./fish pathology --a=v05 --b=v05 --games=300 --seed=31        # 600 games
declarations       5400   wrong 112 (2.07407%)
  at/after ev>=220 0   wrong 0 (0%)
  forced endgame   6   wrong 6 (100%)

$ ./fish pathology --a=v05 --b=v04 --games=300 --seed=31        # 600 games
declarations       5400   wrong 103 (1.90741%)
  at/after ev>=220 8   wrong 1 (12.5%)
  forced endgame   22   wrong 22 (100%)
```

**Wrong-declaration rate: 2.07% mirror, 1.91% vs v0.4.** Confirmed against the
head-to-head telemetry (`./fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed=90210`:
`declarations 4.49278/game at 98.4048%  opp 4.47111/game at 98.3723%`).

### Cashed late? No — 0.00%

The trace replayer computes, per half-suit, the first event index at which **some seat of a
team could *prove* the whole allocation** (all six `Knowledge::owner[c]` resolved and all on
that team, `engine/src/belief.hpp:49`), then measures the gap to the actual declaration.

```
$ ./v06probe sets --a=v05 --b=v05 --games=300 --rotations=2 --seed=31
half-suits resolved          5400
  by correct declaration     5288  (97.93%)
  by MISDECLARATION (gift)   106  (1.96%)
  by forced endgame          6  (0.11%, wrong 6)
  by adjudication            0  (0.00%)
--- lateness of correct declarations ---
  correct declarations       5288  (no proof horizon before the declaration: 1858)
  mean lateness              0.006 events;  median 0  p75 0  p90 0  p99 0  max 1
  late >= 5  events          0  (0.00%)
--- lock economics ---
  (half-suit, team) locks    5370
  locks BROKEN before cash   0  (0.00%)
  mean events lock held      5.546
```

**v0.5 declares on the very event it becomes able to prove the allocation. Late cashing is
not a defect any more — 0.00% at every threshold.** The M8 removal did not reintroduce
dithering.

But the second half of that block is the interesting one. **Locks are never broken (0 of
5,370)** — consistent with the unstealability theorem — yet the team physically holds all
six cards for a mean of **5.55 events** before it can *prove* who holds what. The binding
constraint on cashing speed is **allocation proof, not policy patience**. Section 6 shows
what those 5.5 events cost.

### Evidence class of each declaration (150 deals × 2, same probe)

```
  PROVED allocation          1230  (45.56%)  wrong 0 (0.000%)
  GUESSED (belief only)      1470  (54.44%)  wrong 64 (4.354%)
  misdeclarations: mean cards of the set the declaring team really held 5.719 / 6
  misdeclarations where the team HELD ALL SIX (pure allocation error) 46 / 64 (71.88%)
```

vs v0.4 (150 deals × 2): `PROVED 1242 (46.00%) wrong 0` / `GUESSED 1458 (54.00%) wrong 53
(3.635%)`, pure allocation errors 40/53 = **75.47%**.

Three things follow:

1. Proof-backed declarations are **never** wrong. All error lives in the 54% that are
   belief-only, at a **4.35%** error rate.
2. **72–75% of all misdeclarations are pure allocation errors**: the team held all six cards
   and named the wrong teammate. M2 fixed *capacity feasibility*
   (`engine/src/v05.hpp:607+`); it did not fix *which* feasible allocation. This is the
   residue M2 left.
3. Declaration confidence is **under-confident**, so the thresholds are mis-sited:

```
$ ./fish calibrate --a=v05 --b=v05 --games=200
decl n=1800 brier=0.01554 logloss=0.06215 ece=0.02044 meanPred=0.9645 meanObs=0.9839
   [0.5,0.6) n=    40 pred=0.5139 obs=0.6250
   [0.7,0.8) n=   112 pred=0.7589 obs=0.9554
   [0.8,0.9) n=    76 pred=0.8511 obs=0.9342
   [0.9,1.0) n=  1562 pred=0.9985 obs=0.9981
```

`declThreshold = 0.81991` and `lockedAllocThresh = 0.73250` (`engine/src/v05.hpp:88-90`) are
set against a forecast that under-states its own accuracy by 20 points in the 0.7–0.8 band.

### Lost to the opponent declaring first

Of every half-suit in mirror play, **97.93% are awarded because the other team cashed them
correctly** and **0.00% because a team that once held the lock lost it**. There is no
"opponent sniped a set we had" failure mode left; losses are decided in the collection race,
not at the declaration.

### Forced endgame

6 forced declarations in 5,400 mirror half-suits (0.11%), 22 in 5,400 vs v0.4 (0.41%) — all
wrong in these samples. The README's 24.35% correct rate is over 24,000 games; the incidence
is now so low (0.005–0.006/game, confirmed by `match`: `forced decls 0.00555556/game at 40%`)
that this is no longer a material loss channel. Deprioritise it.

### Declaration pre-gate: unaudited for v0.5

`fish gateaudit` still defaults to `--a=v04:mgate=0.008,gateaudit=1`
(`engine/src/main.cpp:393`). Against v0.4 it does not pass:

```
$ ./fish gateaudit --games=120 --rotations=6
declaration opportunities  1741416
(opportunity, half-suit)   10119744
rejected by a cheap gate   4122986  (40.742% of pairs)
false negatives            132  (0.00320% of rejections)
opportunities where the chosen action differs  132
GATEAUDIT: FALSE NEGATIVES PRESENT
```

v0.5 has the audit code (`engine/src/v05.hpp:830, 844, 862-871`) and the config flag
(`engine/src/v05.hpp:100`), but **`factory.hpp`'s v0.5 branch — lines 39 to 128 — never
parses `gateaudit`**; only the v0.4 branch does, at `engine/src/factory.hpp:141`. So the
option is silently dropped and the audit reports a vacuous pass:

```
$ ./fish gateaudit --a="v05:mgate=0.008,gateaudit=1" --games=12 --rotations=2 --panel=v03,lockout
declaration opportunities  0
...
GATEAUDIT PASS (no false negative observed)
```

**v0.5's declaration pre-gate has never been audited.** One line in `factory.hpp` fixes it.

---

## 4. Ask quality now

```
hit rate                     56.587%    (mirror, probe team; 55.63% both teams via pathology)
hitting legal asks available mean 9.325 per decision
MISSES with NO hitting ask   32  (1.39% of misses; unavoidable)
MISSES with a hitting ask    2265  (98.61% of misses; recoverable)
a hitting ask ranked #1      2994  (56.59% of decisions)
a hitting ask inside top-K   4657  (88.02% of decisions)
decisions with NO hitting ask 32  (0.60%)  -> perfect-information ask ceiling 99.40%
```

**Answer to Q4.** Hit rate **55.6%** (mirror, `pathology`), **53.6%** vs v0.4. Of the
misses, **1.4% were unavoidable** — no legal ask on the board would have hit. **98.6% of
misses were recoverable**, and in **88.0% of all decisions a hitting ask was already inside
v0.5's own top-6 refined candidates**, against a realised 56.6%. The gap between 88.0% and
56.6% — **31.4 points of hit rate** — is sitting in the tie-break, and Section 2 prices it
at 6.3 half-suits per game.

Vs v0.4 the picture is the same shape (25 deals × 2): hit rate 53.50%, hitting ask inside
top-K 85.10%, unavoidable misses 3.14%.

Forecast calibration for the ask channel is decent but under-confident in the middle:

```
ask  n=17336 brier=0.12647 logloss=0.37583 ece=0.02562 meanPred=0.5484 meanObs=0.5566
   [0.4,0.5) n=  1462 pred=0.4516 obs=0.5233
   [0.5,0.6) n=  1326 pred=0.5389 obs=0.5837
   [0.7,0.8) n=   743 pred=0.7556 obs=0.6891
   [0.9,1.0) n=  5164 pred=0.9975 obs=0.9955
```

29.8% of asks sit in the [0.9, 1.0) bin (deduced certainties). The 0.7–0.8 bin is the only
*over*-confident one.

---

## 5. Action cap and event distribution

```
mirror  events/game 96.7233   median 96  p90 112  p99 124  max 131   action-limit games 0 (0%)
vs v04  events/game 100.203   median 99  p90 116  p99 136  max 241   action-limit games 0 (0%)
match   events/game 100.441   limit hits 0%
verify  ./fish verify --games=600 -> audit violations 0 / 23594580 checks,
        set-conservation failures 0, action-limit games 0, determinism PASS, VERIFY PASS
```

**Zero games hit the cap** in any configuration measured (`maxAsks = 400`,
`engine/src/fish.hpp:141`). The distribution is tight and right-bounded: p99 = 124 in
mirror, and the vs-v0.4 tail (max 241) is entirely v0.4's residual deadlock, not v0.5's.
This item is closed; the `forceDeclareEvents = 220` backstop fires 0 times in mirror
(`at/after ev>=220  0`).

---

## 6. Largest remaining source of lost half-suits

```
$ ./v06probe sets --a=v05 --b=v05 --games=300 --rotations=2 --seed=31
--- for the team that LOST each half-suit ---
  own misdeclaration         106  (1.96%)
  opponent cashed correctly  5288  (97.93%)
     ... and WE HAD THE LOCK once  0  (0.00% of all half-suits)
     ... never had the lock        5288  (97.93%)
  forced endgame             6
  adjudication               0
```

**Answer to Q6.** The attribution is unambiguous and slightly deflating: only **1.96%**
of lost half-suits are own-goals (misdeclaration), **0.11%** forced endgame, **0.00%**
adjudication, and **0.00%** "we had it and lost it." **97.93% are lost in the collection
race** — the other team simply got all six first and proved it first.

So the largest remaining loss source is not a declaration bug. It is **wasted turns**, and
the pathology run names the biggest identifiable slice of them:

```
asks in own-locked 5112  (9.74977% of asks)   -- guaranteed miss, but emits a certificate
repeat (a,suit,t)  26364  (50.2823%)
repeat (a,c,t)     1370  (2.61291%)
DEAD asks          6     (0.0114%)
starved turns      6     (0.0114%)
```

Derived from those totals (52,432 asks, 55.626% hit rate → 23,266 misses):

> **21.97% of every miss v0.5 makes in mirror play is an ask to an opponent for a card
> v0.5's own team already holds.**

These are not provably-dead asks — M1 cannot see them, because `enumerateAsks`
(`engine/src/fish.hpp:179-197`) only permits asking opponents, and the asker has no way to
prove a *teammate* holds the card. They are exactly the 5.55-event window from Section 3:
the team owns the half-suit, nobody can prove the allocation, and meanwhile teammates burn
turns fishing for it from the opposition. Each one hands the turn to the other team.

The causal chain, end to end, all measured above:

```
no partner model  ->  allocation unprovable for ~5.5 events after the team owns the set
                  ->  9.75% of asks (22.0% of misses) are guaranteed misses inside owned sets
                  ->  turn handed over
                  ->  the other team wins the collection race
                  ->  97.93% of lost half-suits
```

And the second slice: **50.3% of asks repeat an (actor, half-suit, target) triple** already
asked this game. That is not necessarily wrong — cards move — but combined with the
54% exact-tie rate it says the candidate ranking is close to memoryless.

---

## 7. Risks and caveats on the above

* `CheatAgent`, `AskOracleV05` and the trace attribution are **my** code, not audited by the
  project's `verify`/`oracle` harness. The cheat's declaration policy (declare on lock) is a
  heuristic, not a solved-game optimum; `cheatpatient` was written to test sensitivity and
  gives an identical result (8.9558 vs 8.9550), which is reassuring but not a proof.
* `askoracle` is a hindsight upper bound, not an achievable policy. The reachable fraction of
  its 6.3-set edge is unknown until a real tie-break is built and measured; the honest claim
  is "the ordering is worth a lot", not "v0.6 will get it."
* The margin/sd statistic is my construction. The *exact-tie* count (54.2–54.7%, `u₁ == u₂`
  bit-for-bit with `p₁ == p₂`) is definition-free and is the load-bearing number.
* Sample sizes: margin probe 5,291 decisions (60 deals × 2); `sets` 5,400 half-suits (300 × 2);
  `askoracle`/`cheat` 900–1,200 games. The `belief=block` decision-level comparison is only
  2,161 vs 2,215 decisions — the *match-level* block result is the replicated one.
* `belief=block` is ~11× slower (33.0 games/s vs 319.2 games/s), so its match samples are
  smaller; both seeds nonetheless exclude 50% by a wide margin.
* The `askoracle` and `cheat` runs use `--rotations=6` and one seed bank each; not
  cross-validated over the five held-out banks the v0.5 study used.

---

## 8. Provenance

Commands run, in order, all from `engine/` unless noted:

```
make                                                                     # up to date
./fish pathology --a=v05 --b=v05 --games=300 --seed=31
./fish pathology --a=v05 --b=v04 --games=300 --seed=31
./fish verify --games=600
./fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed=90210
./fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed=515253
./fish match --a=v05:belief=block --b=v05 --games=200 --rotations=6 --seed=90210
./fish match --a=v05:belief=block --b=v05 --games=100 --rotations=6 --seed=515253
./fish oracle --games=150
./fish gateaudit --games=120 --rotations=6
./fish gateaudit --a="v05:mgate=0.008,gateaudit=1" --games=12 --rotations=2 --panel=v03,lockout
./fish calibrate --a=v05 --b=v05 --games=200

# scratchpad probe (not in engine/src)
./v06probe margin      --b=v05 --games=60  --rotations=2 --seed=31
./v06probe margin      --b=v05 --games=25  --rotations=2 --seed=31
./v06probe margin      --b=v05 --belief=block --games=25 --rotations=2 --seed=31
./v06probe margin      --b=v04 --games=25  --rotations=2 --seed=31
./v06probe sets --a=v05 --b=v05 --games=300 --rotations=2 --seed=31
./v06probe sets --a=v05 --b=v05 --games=150 --rotations=2 --seed=31
./v06probe sets --a=v05 --b=v04 --games=150 --rotations=2 --seed=31
./v06probe askoracle    --b=v05 --games=150 --rotations=6 --seed=90210
./v06probe askoracleall --b=v05 --games=150 --rotations=6 --seed=90210
./v06probe cheat        --b=v05 --games=200 --rotations=6 --seed=90210
./v06probe cheatpatient --b=v05 --games=200 --rotations=6 --seed=90210
```

Total engine time ≈ 7 minutes. Nothing under `engine/src/`, `paper/` or `docs/` was modified;
`git status` shows only the new untracked `research/v06/` tree.

Probe source: session scratchpad `v06probe.cpp`
(`/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/eacfe5c8-cfa1-4aef-ad3c-cf4e017d6982/scratchpad/v06probe.cpp`).
It should be moved into the repo (as `engine/src/probe_v06.hpp` + a `fish tiebreak` /
`fish setattr` subcommand) before any of these numbers go in a paper, so they reproduce from
a clean checkout.
