# R1 — Complete anatomy of the deployed v0.5 policy

Recon only. Repository `fish optimization` at commit `bd812fe` ("v0.5"). Every code claim below is a
`file:line` citation into `engine/src/v05.hpp` (970 lines), `engine/src/v04.hpp` (822),
`engine/src/baselines.hpp` (397), `engine/src/fish.hpp`, `engine/src/game.hpp`, `engine/src/belief.hpp`.

**Method.** Everything numeric was measured against the shipped code. Ablations use the existing binary
(`engine/fish`, built from `bd812fe`) via `factory.hpp`'s spec parser. Structural statistics come from
probes that *subclass* `V05Agent`, measure, then delegate to the base method, so the played trajectory
is byte-identical to the shipped policy (the one probe that had to perturb state saves and restores
`bel`). Probe sources are checked in at `research/v06/notes/probes/` (`r1_anatomy.cpp`, `r1_decl.cpp`,
`r1_stale.cpp`, `r1_target.cpp`, `r1_split.cpp`, `r1_time.cpp`, `r1_misc.cpp`); nothing under
`engine/src/`, `paper/` or `docs/` was modified. Confidence intervals are the Wilson
intervals `fish match` reports.

**Note.** During this session a parallel agent began writing `engine/src/v06.hpp`,
`engine/src/v06_rollout.hpp` and editing `engine/src/factory.hpp`. `v05.hpp` and `V05Agent` are
untouched, so all measurements below are of the v0.5 policy as committed.

---

## 1. What is actually deployed

`V05Agent : Agent` (`v05.hpp:130`) — it does **not** inherit from `V04Agent`. `v05.hpp` is a full copy
of the v0.4 agent with three mechanisms edited in; `#include "v04.hpp"` (`v05.hpp:22`) is only there for
the shared declarations `BeliefMode`, `NFEAT=20`, `NVFEAT=16`, `ValueAggregates`, `binEnt`,
`GateAuditCounters` (`v04.hpp:39-52`, `v04.hpp:133-136`). `V04Agent` is compiled in as the reference
opponent and shares no code path with v0.5.

Of the ten mechanisms in `research/v05/DESIGN.md`, **three shipped**: M1 (live-ask gate), M2
(capacity-feasible allocation), M8 (delete `pressure()` stage 2). M3–M7, M9, M10 are unbuilt.
`engine/src/v05_oppmodel.hpp` (657 lines, M7) and `engine/src/v05_target.hpp` (524 lines, M4/M5) exist
but are included by **nothing in the shipped binary** — only by `research/v05/runs/M7/*.cpp` and
`engine/src/probe_m45_test.cpp` respectively. They are not part of v0.5.

`baselines.hpp` is not on any v0.5 code path. It holds `BaselineAgent` (`baselines.hpp:131`), the
faithful ports of the v0.2/v0.3 population plus the six hand-styled opponents, used only as evaluation
opponents. Its independent-marginal memory (`LegacyMemory`, `baselines.hpp:38`), its 12-iteration
Sinkhorn (`baselines.hpp:107`) and its `predictionForSet` per-card argmax with the
`pow(teamConf*allocConf, 1/12)` confidence (`baselines.hpp:283-301`) share no structure with v0.5's
`Knowledge`/`Belief`. Its `choosePassTarget` is "teammate with the most cards" (`baselines.hpp:390`).

---

## 2. `chooseAsk` (`v05.hpp:502-591`)

### 2.1 Candidate enumeration

`refresh()` (`503`) rebuilds the posterior if `dirty`. At the default `BeliefMode::Fast` this is one
`bel.sinkhornDisj(k, 4, 8, 0.44458, 0.12198)` (`v05.hpp:188`, `belief.hpp:478`): 4 outer sweeps of
8 row/column IPF iterations over the unresolved cards, interleaved with conditioning on the ask-legality
disjunctions (C5).

Candidates come from `enumerateLive` (`v05.hpp:482-500`), which calls `enumerateAsks`
(`fish.hpp:181-198`) — all (card, target) pairs where the actor holds another card of an active
half-suit, does not hold the card, and the target is an opponent with cards — then drops every ask that
is `provablyDead` (`v05.hpp:474-478`):

```
k.owner[card] < NPLAY ? k.owner[card] != target : !(k.mask[card] & (1u << target))
```

Measured over 26,417 shipped ask decisions (300 games, seed 4242): **47.34 legal candidates → 43.51
live**; the gate removes at least one candidate at **79.23%** of turns; the full fallback (nothing live,
`v05.hpp:497-498`) fires at **0.019%** of decisions (5 of 26,417).

### 2.2 The M1 live gate — what it is and is not

M1 as shipped is **only** the hard filter. The design spec's second half — "multiply f[3], f[5], f[7],
f[15] by p" — is `cfg.ownershipByP`, default **false** (`v05.hpp:112`), so `og = 1.0` at `v05.hpp:314`
and the four ownership features are ungated by `p` exactly as in v0.4. At v0.5's own fitted weights a
`p = 0` candidate can still collect `f[3] ≤ 2.507·(5/6)`, `f[5] ≤ 4.046`, `f[7] ≤ 1.219`,
`f[15] ≤ −0.958` = **+6.396 raw / +4.822 after `linearWeight`**, against `f[0]`'s `8.777·p`.

The gate is therefore load-bearing and the fitted weights do not encode it. Direct evidence
(`fish pathology`, 500 games, seed 31):

| | v0.5 shipped | v0.5 with `m1=0` |
|---|---:|---:|
| provably dead asks | **0 (0%)** | 33,808 (**46.28%**) |
| longest dead run | 0 | 374 |
| games with a dead run ≥ 6 | 0% | 27.2% |
| games hitting the action cap | 0% | **14.4%** |
| events/game (mean / p99 / max) | 96.0 / 124 / 131 | 154.5 / 406 / 406 |
| ask hit rate | 55.61% | 29.80% |
| declarations wrong | 2.18% | 4.38% |

v0.5's fitted vector makes the *latent* pathology worse than v0.4's (46.3% dead asks vs the 39.0%
reported for v0.4) — the CEM optimised entirely behind the gate and never saw the behaviour the gate
suppresses. Yet head-to-head `v05` vs `v05:m1=0` is **50.75% [49.20, 52.30]** (n=4,000): the gated and
ungated policies are equally strong because a mutual deadlock is resolved by neutral adjudication.
**M1 buys the pathology fix, not points.**

How often the gate binds on the *decision* rather than the candidate list: scoring all legal asks with
the shipped score, the ungated argmax is provably dead at **5.78%** of decisions (n=13,250).

### 2.3 The 20 ask features (`features`, `v05.hpp:285-334`)

`S = setOf(card)`; `p = bel.marg[card][target]`; `myHave` = cards of `S` in my hand; `teamExp` = Σ over
`S` of (1 if mine else `pTeamCard`); `pTeamOther` = Π over `S \ {card}`; `pTeamAll` = Π over all of `S`;
`og = 1.0` (since `ownershipByP=false`); `lead` = my team's score minus theirs.

Measured columns are from 26,417 shipped decisions. `const%` = share of decisions where the feature
takes one value across the whole candidate set (so it cannot affect the argmax at all).
`contribSpread` = `|w| × mean(max−min) × linearWeight`, i.e. the feature's mean discriminating power in
score units.

| # | name | formula (`v05.hpp` line) | reads | w (v0.5) | w (v0.4) | const% | contribSpread | sign |
|---|---|---|---|---:|---:|---:|---:|---|
| 0 | hit probability | `p` (306) | card,target | **+11.64227** | +11.5060 | 5.8 | **4.138** | ok |
| 1 | squared hit | `p²` (307) | card,target | +3.30409 | +3.2948 | 5.8 | 0.988 | ok (convex in p) |
| 2 | certain hit | `p > .9995` (308) | card,target | +3.47852 | +3.1978 | 74.6 | 0.667 | ok |
| 3 | own set progress | `og·myHave/6` (312) | **S only** | +2.50714 | +1.6881 | 24.8 | 0.432 | ok, but ungated by p |
| 4 | team control | `teamExp/6` (313) | **S only** | +1.68817 | +2.1333 | 16.5 | 0.297 | questionable — rewards asking where we already dominate |
| 5 | lock completion | `og·pTeamOther` (314) | S,card | +4.04617 | +4.0705 | 11.8 | 0.939 | ok, but ungated by p |
| 6 | continuation | `max_{c≠card, not mine} max_{opp} marg` (315) | S,card | +1.60518 | +1.4679 | 22.1 | 0.408 | ok |
| 7 | completion bonus | `og·(myHave≥4 ? 1 : myHave==3 ? .35 : 0)` (316) | **S only** | +1.21892 | +1.4281 | 56.9 | 0.245 | ok, ungated by p |
| 8 | reply threat | `(1−p)·threatOf(pub,target)` (317) | target,p | **−2.90583** | −3.0978 | 5.6 | 0.829 | ok |
| 9 | information leak | `teamRevealedSet(S) ? 0 : 1` (318) | **S only** | −1.22009 | −0.8536 | 31.3 | 0.632 | ok (hide fresh interest) |
| 10 | target hand size | `handCount[target]/9` (319) | target | −2.24823 | −2.0219 | 13.2 | 0.506 | ok |
| 11 | empties target | `handCount[target]==1 ? p : 0` (320) | target,p | +1.15962 | +1.1660 | 85.9 | 0.039 | **suspect** — does not distinguish the *last live* opponent; walking a team to cardless enters the forced endgame (defect F, unfixed) |
| 12 | repeats set | `lastMySet == S` (321) | **S only** | +1.38026 | +1.2697 | 38.6 | 0.639 | **suspect** — an explicit bonus for re-asking in the half-suit you last asked in; went **up** from v0.4 (defect E, unfixed). 50.24% of v0.5 asks repeat an (actor, half-suit, target) triple |
| 13 | known team cards | `teamKnown/6` (322) | **S only** | +0.83838 | +0.9142 | 79.1 | 0.035 | near-inert |
| 14 | location entropy | `binEnt(p)` (323) | p | **−2.42663** | −2.6534 | 6.1 | **1.068** | **wrong sign** — a *negative* value-of-information term; the 2nd-largest discriminator in the whole score pays the agent to avoid uncertainty-reducing asks (defect D, unfixed) |
| 15 | team owns set | `og·pTeamAll` (324) | **S only** | −0.95833 | −0.8045 | 17.4 | 0.121 | ok (flipped negative in v0.5, partially substituting for M1) |
| 16 | exposure on miss | `(1−p)·exposureOf(pub,target)` (325) | target,p | **+2.53330** | +1.9040 | 9.4 | 0.501 | **wrong sign** — `exposureOf` (`v05.hpp:228-238`) measures *our* team's card mass in half-suits the target has already asked in, i.e. danger; a positive weight rewards handing the turn to a well-placed opponent |
| 17 | trailing pressure | `(lead ≤ −2)·p` (326) | p,score | **−0.22422** | +0.0473 | 84.9 | 0.014 | **sign flipped vs v0.4**, near-inert; now asks *less* when behind |
| 18 | runway | `expectedRun(card, p)` (327) | card,p | +1.24554 | +1.4108 | 5.8 | 0.259 | **redundant** — corr(f[0], f[18]) across candidates = **0.9944** |
| 19 | leak magnitude | `(teamRevealedSet(S)?0:1)·myHave/6` (328) | **S only** | −1.40315 | −0.9990 | 22.0 | 0.233 | ok |

Structural facts that fall out of the formulas:

* **8 of 20 features depend only on the half-suit `S`** (f3, f4, f7, f9, f12, f13, f15, f19). They
  cannot discriminate between candidates inside a half-suit; they only shift half-suit against
  half-suit. The mean candidate set spans **3.70 distinct half-suits** out of 43.51 candidates.
* Only **four** features are genuine target-dimension terms: f8, f10, f11, f16. Everything else sees the
  target only through `p`. (The design doc's defect C is about the *value* side, `v05.hpp:438`; on the
  linear side the target dimension does exist.)
* `f[16]` and `f[8]` are two competing "danger on miss" measures with opposite signs; their measured
  correlation across candidates is only **0.298**, so they are not cancelling cleanly — they are
  fighting.
* `threatOf(pub, target)` (`v05.hpp:209`) and `exposureOf(pub, target)` (`v05.hpp:228`) are recomputed
  **once per candidate** although they depend only on `target` (≤ 3 distinct values), and
  `teamRevealedSet(S)` (`v05.hpp:240`) twice per candidate for ≤ 9 distinct values. With `searchTopK>1`
  `features()` runs twice per candidate (§2.6), so `threatOf` runs ~87 times per decision for ≤ 3
  distinct answers.
* `expectedRun` (`v05.hpp:274-283`) has an off-by-rank bug: the "exclude this card from its own
  continuation" test only fires at `i == 0 && m == 0` (`v05.hpp:277`), so a card tied with — but not
  equal to — the top entry is double-counted, and a card that is not the top entry never excludes
  itself. In practice the feature is 99.4% collinear with `p` anyway.

### 2.4 The one-ply expectimax (`askExpectedValue`, `v05.hpp:437-461`)

The static evaluator is `value(...)` (`v05.hpp:373-407`), 16 features over cached aggregates
(`computeAggregates`, `v05.hpp:345-372`): `eH[s]` = expected fraction of half-suit `s` held by my team,
`sumControl = Σ(2e−1)`, `sharpControl = Σ sharp(e)` with `sharp` a clipped ramp on `[0.35, 0.65]`
(`v05.hpp:339-343`), `locked`, `contested = Σ e(1−e)`, `active`, plus card counts.

For a candidate `(card, target)` with hit probability `p`, `pt = pTeamCard(card)`, `S = setOf(card)`:

* hit: `eHit = eOld + (1−pt)/6`; branch evaluated with `turnSign = +1`, `dOur = +1`, `dTheir = −1`,
  `dUnresolved = −1`, `dActive = 0`.
* miss: `ptMiss = min(1, pt/(1−p))`, `eMiss = eOld + (ptMiss−pt)/6`; `turnSign = −1`, all deltas 0.
* `EV = p·vHit + (1−p)·vMiss`.

`target` is explicitly discarded: `(void)target;` at **`v05.hpp:438`**. The miss branch models neither
which opponent receives the turn, nor the certificate the ask emits (that I hold another card of `S`),
nor any posterior change outside `eH[S]`.

### 2.5 How the pieces combine

```
u_i = linearWeight · Σ_j w[j]·f[j](i)                 (v05.hpp:515-516)
    + valueWeight  · askExpectedValue(i)              (v05.hpp:517)
```
`linearWeight = 0.75393`, `valueWeight = 6.47680`. Then, if `searchTopK > 1`:
```
u_i ← u_i + chainWeight·p_i·follow_i − threatWeight·(1−p_i)·threat_i     (v05.hpp:581)
```
`chainWeight = 3.58301`, `threatWeight = 2.70470`, `searchTopK = 6` (all fitted).

Measured relative magnitudes across the candidate set (26,417 decisions):

* mean spread of the linear part = **8.2699**; mean spread of the value part = **0.1479**; ratio
  **1.79%**.
* the value term flips the pre-search argmax at **0.94%** of decisions.
* the top-K rescoring flips the argmax of `L+V` at **43.37%** of decisions — the chain/threat term is
  by far the most decision-changing component after `f[0]`.
* the final move differs with vs without the value term (both with the search) at **2.98%** of
  decisions, because the value term reshuffles which six candidates enter the search.

### 2.6 The top-K search (`v05.hpp:520-587`)

1. **Lines 512-519 are dead when `searchTopK > 1`.** The first loop computes `best`/`bestScore`/`bestP`
   over all `n` candidates; the `searchTopK > 1` branch recomputes the identical quantity at
   `526-533` and returns at `586`. `best` is only read at `588-590`. At the shipped `searchTopK = 6`,
   **every candidate's `features()` and `askExpectedValue()` are computed exactly twice**, and the first
   pass is discarded.
2. `std::partial_sort` on `u` descending (`535`), keep `K = min(6, n)`.
3. For each of the K, if `p > 0.02`, build a hypothetical `Knowledge` in which the target held the card
   and I now hold it (`543-549` — note `setOwner(card,target)` first, so the disjunctions and capacities
   are discharged against the *pre-transfer* holder, which is correct), re-run `sinkhornDisj`, and take
   `follow = max` marginal over all legal asks in the hypothetical (`550-558`). This is a 1.5-ply
   continuation estimate, and it duplicates what `f[18] runway` already approximates.
4. For each of the K, if `p < 0.98`, build the miss `Knowledge` (`exclude(card,target)`), re-run
   `sinkhornDisj`, and compute a *second, differently defined* threat: `max_S (1−Π(1−marg[c][target]))
   · max_c Σ_team marg[c][q]` (`559-580`). This drops the `(1−pt)` discount and the
   `(0.7+0.3·activity)` factor that `threatOf` uses for `f[8]`. So miss-danger is priced twice, under
   two different definitions, with a combined coefficient of about `−2.19 (f[8]) − 2.70 (search)`.

**Cost and value of the search.** Single-threaded, 200 games:

| | µs per `chooseAsk` | share of total runtime |
|---|---:|---:|
| shipped (`topk=6`) | **59** | 12.5% |
| `topk=1` | 6 | 1.4% |
| `topk=6, chain=threat=0` | 10 | 2.6% |
| `topk=24` | 194 | 32.4% |

Head-to-head against v0.5 (pooled over 16,000 games at seeds 770077/313131/2024):

| ablation | v0.5's win rate | verdict |
|---|---:|---|
| `topk=1` (whole search off) | **50.81%** | +0.8 pt, ~2σ |
| `chain=0` | **50.08%** | **zero** |
| `threat=0` | **49.55%** | zero to slightly negative |
| `value=0` | **51.91%** | +1.9 pt |
| `m2=0` | 49.33% (n=600) | zero on win rate |
| `vdecl=0` | 50.33% (n=600) | zero |
| `stage2=1` | **exactly 50.00%** (n=600) | provably inert (§3.2) |
| `m1p=1` | 51.00% (n=600) | consistent with the −5.6 pt the header claims |
| `norepeat=1` | 55.50% (n=600) | consistent with the −6.0 pt the header claims |

So the 12 Sinkhorn refits per decision that make up 90% of `chooseAsk` buy **+0.8 ± 0.5 points**, and
`chainWeight` — the more expensive half — buys nothing measurable.

For scale, replacing the whole 20-feature score with pure `argmax p` (same declaration machinery) loses
**60.33% [58.36, 62.27]** to v0.5 (n=2,400): the feature set as a whole is worth ~10 points; the value
function and the 2-ply search are worth ~2.

### 2.7 Tie-breaking and determinism

No randomisation anywhere. `Rng rng` (`v05.hpp:135`) is read only at `v05.hpp:195` and `764`, both
inside non-`Fast` belief branches, so at the shipped configuration it is never used. Ties are broken by:
strict `>` scanning `i` ascending in the first loop (`518`), and after `std::partial_sort` — which is
**not stable** — strict `>` scanning rank `r` ascending in the search loop (`582`). Candidate order out
of `enumerateAsks` is (half-suit ascending, index-in-suit ascending, seat ascending), so the untied
policy is a deterministic function of the public state and the agent's hand.

`chooseAsk` returns `AskMove{0,0}` on an empty candidate set (`507`); `Game::run` catches the illegal
move and either substitutes `buf[0]` or forces a declaration (`game.hpp:326-339`).

Empirically the policy chooses `argmax p` at **42.26%** of decisions and stays in the greedy half-suit
at **79.51%**; it gives up **0.0364** of hit probability per ask on average (0.5478 chosen vs 0.5842
best available). Among decisions where the chosen card has more than one live target (**69.21%**), the
chosen target is the max-`p` one only **66.55%** of the time — so the target dimension is decided by
something other than `p` at **23.2%** of all asks.

---

## 3. The declaration path

### 3.1 `proposeDeclaration` (`v05.hpp:815-875`)

Called by `Game::declarationRound` (`game.hpp:202-231`) for **all six seats after every public event**,
in a loop that repeats while anybody wants to declare. Arbitration is lowest-seat (`rules.declArbitration = 0`).

Order of operations, exactly as written:

```
815  if (!declareEnabled) return false
817  if (!rules.cardlessMayDeclare && !handCount[seat]) return false
821  unresolvedCount = popcount(k.unresolved)
822  press   = pressure(pub)
823  bypass  = unresolvedCount <= 8 || press >= 1
824  if (useValue) computeAggregates(pub)          // <-- BEFORE refresh()
825-829  candidate = bypass || any active s with k.cheapTeamProb(s, teamMask) >= gateTeamProb (.008)
830  if (!candidate) return false
831  refresh()                                     // <-- posterior rebuilt HERE
832-837  urgent = unresolvedCount <= patiencePool(6)
              || oppCards <= oppCardFloor(2.61651)
              || nEvents >= forceDeclareEvents(220)
              || bestAskProbability(pub) < askFloor(0.25742)
839-861 for each active s passing the gate: v = evaluateSet(...); if (v.ok && declareNow(...)) keep argmax pAlloc
874  conf = bestConf; return found
```

**Defect H (stale aggregates) is NOT fixed in v0.5.** `computeAggregates` at `824` runs before
`refresh()` at `831`, so `eH[]`, `agg`, `ourCards`, `unresolvedN` are computed from the posterior as of
the previous refresh, while `evaluateSet`/`pAlloc` use the fresh one. Measured non-perturbingly over
81,694 gated opportunities (150 games): `eH` differs pre- vs post-refresh at **92.52%** of them, mean
max|Δe_H| = **0.10057**, max **0.96406**. It nonetheless flips the `declareByValue` verdict at only
**0.30%** of evaluations — because of §3.4.

Related latent bug: `Belief::marg` (`belief.hpp:454`) has no initialiser and `V05Agent::reset`
(`v05.hpp:148`) never refreshes, so the very first `proposeDeclaration` of a game (the game loop calls
`declarationRound()` before any ask, `game.hpp:294`) reads `bel.marg` before it has ever been written.
Production is saved by `std::make_unique<V05Agent>()` in `factory.hpp:40`, which value-initialises and
therefore zeroes it; a stack-allocated `V05Agent` reads indeterminate memory (I hit this in a probe).

Measured frequencies (70,236 opportunities, 120 games): `bypass` 14.70%, `urgent` **31.25%**
(`askFloor` clause 25.29%, `patiencePool` 10.65%, `oppCardFloor` 3.38%, `nEvents` **0.0000%**).

### 3.2 `pressure()` (`v05.hpp:684-702`) — inert

```
698  if (forceStage2 && nEvents >= (7·220)/5 = 308) return 2;   // forceStage2 = false → never
699  if (nEvents >= forceDeclareEvents = 220)        return 1;
700  return 0;
```
With M1 in place, v0.5 mirror games have max `nEvents = 131` (`fish pathology`, 500 games) and 127 in my
probe run. `press >= 1` fired at **0.0000%** of 70,236 declaration opportunities. Consequently:

* `pressure()` always returns 0 in self-play;
* the relaxed floors `press>=2 → 0.0`, `press>=1 → 0.25` (`v05.hpp:722`), the
  `press>=2 → marginalGate 0` (`723`), and the shortcuts `declareNow` `801-802` are all dead on this
  path (`evaluateSet(pub, set, 2)` in `willingForced`'s fallback at `v05.hpp:889` is the only live
  `press=2` call site);
* `forceDeclareEvents` is read at `698`, `699` and `836` and never binds anywhere;
* `v05:stage2=1` returns **exactly 50.00%** head-to-head, confirming M8's stage-2 deletion changes
  nothing at the current game lengths.

### 3.3 `evaluateSet` (`v05.hpp:703-770`) at the shipped configuration

```
707-721  for each card of s: reject if an opponent holds it or no teammate may;
         cheap  *= (mine ? 1 : pTeamCard(c));
         players[i] / v.decl.owner[i] = per-card team argmax          <-- DEAD (overwritten at 742)
722      teamFloor = minTeamProb = 0.85876          (press is always 0)
723      if (cheap < marginalGate = .008) return not-ok
736-747  Fast + feasibleDecl:
           feasibleAllocation(s, owners, pr)   (M2)   else return not-ok
           v.decl.owner ← owners;  v.pAlloc = pr;  v.pTeam = max(cheap, pAlloc)
           if (v.pTeam < 0.85876) return not-ok
           v.ok = true
```
`allow[]` (`706`, `712`) is only read at `767`, and `players[]` only at `755`/`768`; both sites are
unreachable at `belief=Fast, feasibleDecl=true`. The per-card argmax loop at `714-720` is pure waste on
the shipped path.

### 3.4 The stopping rule — `declareNow` (`v05.hpp:799-813`) and `declareByValue` (`779-797`)

At the shipped configuration (`useValue && valueDeclare` both true, `press == 0`) the rule reduces to:

```
if (urgent)  return pAlloc >= declThreshold(0.81991) || (pTeam > .9995 && pAlloc >= 0.5)
else         return declareByValue(pub, v)
```
`lockedAllocThresh (0.73250)` and `patientLocked` are read only at `v05.hpp:808`, inside the
`if (locked)` block that is reached only when `!(useValue && valueDeclare)` — **dead at defaults**.

`declareByValue` evaluates `value()` with the half-suit removed from the control aggregates, `dOur = −6`,
`dActive = −1`, `dUnres = −|unresolved ∩ S|`, once with `scoreDiff+1` and once with `scoreDiff−1`, mixes
them by `pAlloc`, and compares to `value()` of the unchanged state plus `declareMargin = −0.03044`.

**Because `value()` is linear and `scoreDiff` enters only through `vw[1]·scoreDiff/9`, this is exactly a
threshold on `pAlloc`:**

```
declare  ⟺  pAlloc > 0.5 + (declareMargin − κ)·9/(2·vw[1])

κ = vw[2]·dC/9 + vw[3]·dS/9 + vw[4]·dL/9 − vw[6]·6/54 + vw[7]·dU/45
  − vw[8]/9 + vw[9]·turnSign·dC/9 + vw[14]·dK/9 + vw[15]·turnSign·dU/45
```
with `dC = −(2e_S−1)`, `dS = −sharp(e_S)`, `dL = −[e_S>.995] + [e_S<.005]`, `dK = −e_S(1−e_S)`,
`dU = −|unresolved ∩ S|`.

Verified by construction against the shipped function: **100.0000% verdict agreement over 4,226
evaluations** (250 games). The implied threshold has mean **0.8090**, min **0.7247**, max **0.8789** —
i.e. the "optimal stopping via a learned value function" is a fixed threshold of ≈0.81 with a
state-dependent jitter of ±0.07, and the hand-set/fitted urgent threshold it replaces is
`declThreshold = 0.81991`. This is why `vdecl=0` measures at 50.33% and why the stale-aggregate defect
flips only 0.30% of verdicts: `e_S` enters only through `κ`, a small additive constant.

Note also that `declareByValue`'s `dActive = −1` removes the half-suit from `agg.active`, but the
`f[12]/f[13]` "near-complete half-suits" loop inside `value()` (`v05.hpp:391-395`) still iterates
`pub.setActive[]` and still counts the half-suit being declared. And `int mine = popcount(myHand &
setMask(S))` is computed at `v05.hpp:789` and discarded at `v05.hpp:792` (`(void)mine;`) — the
declaration's effect on my own hand size is computed and thrown away (defect G, unfixed).

Behaviourally: of 2,047 verdicts on half-suits with `pTeam > .9995` (provably locked, unstealable by
Theorem 1), the rule chooses to **wait at 6.74%**.

---

## 4. `bestGuess`, `willingForced`, and M2 (`feasibleAllocation`)

### `feasibleAllocation` (`v05.hpp:619-676`)

The search it actually runs:

1. Partition the six cards of the half-suit: mine → `seat`; publicly located on a teammate → that
   teammate; publicly on an opponent, out of play, or with no teammate in `mask` → **return false**;
   everything else is `free_`, `nFree ≤ 6`.
2. If `nFree == 0`, score the fixed allocation with `bel.jointSequential` and return.
3. Otherwise enumerate **all `3^nFree` assignments of the free cards to the three teammates**
   (`645-666`), rejecting on the certificate mask (`658`) and on capacity `cnt[p] > q[p]` (`660`),
   where `q = k.capacities()` = `handCount[p] − knownHeld(p)`.
4. Score each survivor by the **product of unconditioned marginals** `Π bel.marg[c][p]` (`661`), keep
   the argmax (ties → lowest `code`).
5. Re-score only the winner with `bel.jointSequential` (`671-673`), which conditions card-by-card and
   re-runs `sinkhornDisj` after each fix — up to 6 Sinkhorn refits.

Cost: mean **164.8 assignments enumerated per call** (max 729), over 321,822 calls in 250 games;
`nFree` histogram (0..6) = 1168 / 8745 / 23731 / 41794 / 100489 / 129254 / 16641. Infeasible (returns
false) at **0.00%** of calls on the voluntary path. The enumeration itself is trivial; the cost is
`jointSequential`'s Sinkhorn refits.

`int used[NPLAY] = {0,...}` (`v05.hpp:625`) is read at `660` and **never written** — dead. (Harmlessly:
cards already located on a teammate are resolved, hence already inside `knownHeld`, hence already
excluded from `q`.)

**Approximation worth naming:** step 4 selects the MAP allocation by an *independence* score, and only
the winner gets a joint evaluation. `belief.hpp:557-563` states explicitly that per-card marginals are
not the same as maximising the joint because the six cards are capacity-coupled. Exactly maximising the
joint over the ≤729 feasible assignments is affordable and is not done.

### `willingForced` (`v05.hpp:877-895`)

`refresh()`, then `feasibleAllocation`; if it succeeds, accept iff `pr >= threshold`, else fall through
to `evaluateSet(pub, set, 2)`. The ladder that calls it is `Rules::forcedTh = {0.995, 0.98, 0.95, 0.90,
0.80, 0.65, 0.50, −1.0}` (`fish.hpp:127`), swept in `Game::forcedEndgame` (`game.hpp:235-258`). The
design spec's "re-shape to ~9 evenly spaced rungs over [0,1]" was **not done** — the eight rungs still
all sit ≥ 0.5.

### `bestGuess` (`v05.hpp:899-943`)

`refresh()`, `feasibleAllocation`; on success return immediately. Only if no feasible allocation exists
does it fall through to the v0.4 per-card argmax (`913-929`), the `BlockDP` branch (`930-937`, dead at
`belief=Fast`) and `jointSequential` (`939`). Since `feasibleAllocation` never failed in 250 games,
**lines 913-943 are effectively dead**.

Volume: `choosePassTarget` 0.372 calls/game, `willingForced` 126 calls / 250 games, `bestGuess` 6 calls
/ 250 games. `fish pathology` reports 6 forced-endgame declarations in 500 games (v0.4: 28).
`research/v05/RESULTS-SUMMARY.md` puts forced-endgame accuracy at 24.35% against a measured feasible
ceiling of ~40.6%.

---

## 5. `choosePassTarget` (`v05.hpp:945-968`)

Called only when a cardless seat holds the turn (`game.hpp:298-308`). For each candidate teammate `u`,
score
```
v(u) = max over active half-suits S of  (1 − Π_{c∈S}(1 − P(u holds c))) · max_{c∈S} max_{opp o} P(o holds c)
```
and take the argmax (`v05.hpp:966`, ties → first candidate scanned). `P(u holds c)` uses `bel.marg`
except for `u == seat`, which uses the exact own hand. This is "hand the turn to the teammate most
likely to have a legal ask in the half-suit with the juiciest opponent-held card". Note it maximises the
*product form* over half-suits, not the teammate's expected hit probability, and it never consults the
teammate's own preferences (there is no willingness channel — the v0.5 diagnosis measured a
turn-transfer ladder at ~0.05 cards/game and rejected it).

---

## 6. Every knob in `V05Config` (`v05.hpp:26-128`)

**Fitted** = a coordinate of the 34-dimensional CEM vector in
`research/v05/runs/v05-fitted.txt`, baked in by `engine/freeze_config_v05.py` (panel: v04, v03, lockout,
detective, diversifier, hunter; β = 25; 40 generations, population 24, elite 6).

| line | knob | default | meaning | provenance | live? |
|---|---|---:|---|---|---|
| 27 | `belief` | `Fast` | posterior engine: one `sinkhornDisj(4,8)` | hand-set | live; all other modes dead |
| 28 | `sinkOuter`,`sinkInner` | 4, 8 | IPF sweeps / iterations | hand-set | live |
| 29 | `priorTheta` | 0.44458 | weight on "asked in this half-suit" | **fitted** (K+9) | live |
| 30 | `priorPhi` | 0.12198 | weight on "took turns, never asked here" | **fitted** (K+10) | live but **not an independent channel** (§7.4) |
| 33 | `greedyMAP` | false | condition while choosing the allocation | hand-set | **unreachable** — `v05.hpp:749` sits after `feasibleDecl`'s unconditional return at `746` |
| 34 | `searchTopK` | 6 | candidates re-scored with chain/threat | **fitted** (K+11, raw 5.73502) | live; worth +0.8 pt |
| 35 | `chainWeight` | 3.58301 | coefficient on `p·follow` | **fitted** (K+12) | live; worth **0.0 pt** |
| 36 | `threatWeight` | 2.70470 | coefficient on `(1−p)·threat` | **fitted** (K+13) | live; worth ~0 pt |
| 37 | `particles` | 96 | particle count | hand-set | **dead** (only `v05.hpp:195`, `764`) |
| 38-58 | `w[20]` | see §2.3 | ask-score weights | **fitted** (0..19) | live |
| 61 | `declThreshold` | 0.81991 | `pAlloc` bar | **fitted** (K+0) | live **only on the `urgent` path** (`804`) |
| 62 | `lockedAllocThresh` | 0.73250 | locked-half-suit bar | **fitted** (K+1) | **dead** (`808`, unreachable) |
| 63 | `minTeamProb` | 0.85876 | `teamFloor` in `evaluateSet` | **fitted** (K+7) | live (`722`) |
| 64 | `patientLocked` | true | delay locked half-suits | hand-set | **dead** (`808`) |
| 65 | `askFloor` | 0.25742 | "no productive ask left" → urgent | **fitted** (K+2) | live; the dominant urgency clause (25.3%) |
| 66 | `patiencePool` | 6 | unresolved-card floor → urgent | **fitted** (K+3, raw 5.64045) | live (10.7%) |
| 67 | `forceDeclareEvents` | 220 | forcing horizon | hand-set | live-but-never-binds (max `nEvents` = 131) |
| 68 | `oppCardFloor` | 2.61651 | opponents nearly out → urgent | **fitted** (K+4) | live (3.4%) |
| 70 | `useValue` | true | one-ply expectimax on | hand-set | live |
| 71 | `valueWeight` | 6.47680 | scale of EV vs linear score | **fitted** (K+5) | live; 1.8% of score spread |
| 72 | `linearWeight` | 0.75393 | scale of the linear score | **fitted** (K+6) | live |
| 73 | `valueDeclare` | true | declare by EV, not threshold | hand-set | live but algebraically ≡ a threshold (§3.4) |
| 74 | `declareMargin` | −0.03044 | EV edge to cash | **fitted** (K+8) | live |
| 79-96 | `vw[16]` | see §7.1 | value-function coefficients | ridge fit (`fish fitvalue`), **byte-identical to v0.4's** — the value function was never refit for v0.5 | live but near-inert |
| 97 | `gateTeamProb` | .008 | capacity-only pre-gate | hand-set | live (`828`, `843`) |
| 98 | `marginalGate` | .008 | marginal pre-gate | hand-set | live (`723`) |
| 99 | `declareEnabled` | true | master switch | hand-set | live |
| 100 | `gateAudit` | false | `fish gateaudit` instrumentation | hand-set | **dead** (`844-861`, `862-872`) |
| 107 | `liveAskGate` | **true** | M1 hard filter | hand-set | live; the load-bearing mechanism |
| 112 | `ownershipByP` | **false** | M1's "scale ownership by p" half | hand-set | **inert** — `og = 1.0` at `314` |
| 118 | `forceStage2` | **false** | M8: `pressure()` stage 2 | hand-set | inert either way (§3.2) |
| 126 | `repeatGuard` | **false** | M8's repetition backstop | hand-set | **inert**; makes `asked[54][6]` (324 B, written at `158`) dead storage |
| 127 | `feasibleDecl` | **true** | M2 on the voluntary path | hand-set | live |

Note `patiencePool = 6` and `searchTopK = 6` were *fitted* to values that coincide with v0.4's hand-set
defaults (raw 5.64045 and 5.73502, both rounded to 6 by `freeze_config_v05.py`).

---

## 7. Dead code and inert terms

### 7.1 Defect (I): "the fitted value-function signal is constant across candidate asks" — **refined, mostly confirmed**

Literally false; effectively true. Of the 16 value features, measured over 26,417 shipped decisions:

**7 are exactly constant across the candidate set at 100.00% of decisions:**

| # | feature | `vw` | why constant |
|---|---|---:|---|
| 0 | bias | +0.001242 | constant by definition |
| 1 | **score differential** | **+0.888965** | `scoreDiff` is passed unchanged in both branches (`v05.hpp:453-454`) |
| 8 | active half-suits | +0.005904 | `dActive = 0` in both branches |
| 10 | my hand size | −0.006601 | `myCards` never perturbed, even though a hit adds a card to my hand |
| 11 | smallest friendly hand | −0.007472 | never perturbed |
| 12 | our near-complete half-suits | −0.022484 | computed from the **unperturbed** cached `eH[]` inside `value()` (`v05.hpp:391-395`) |
| 13 | their near-complete half-suits | −0.025189 | same |

Σ|vw| of the constant seven = **0.957857 = 41.5%** of Σ|vw| = 2.30609, and it contains the single
largest coefficient (score differential, 38.5% of the mass) — so the design doc's "effectively bias +
score differential" is exactly right about which part of the fit dominates and exactly right that that
part cannot rank anything.

**9 vary, but 6 of them vary only through `p`.** The algebra: for any candidate,
```
E[e_S after the ask] = p·eHit + (1−p)·eMiss = e_S + p/6
```
*independently of the card, the half-suit and `pt`* (the miss branch's renormalisation exactly cancels
`pt`). Therefore
* `v[2]` expControl contributes exactly `vw[2]·p/27`;
* `v[3]` sharpControl contributes exactly `vw[3]·1.1111·p/9` whenever `sharp` is unclipped;
* `v[5]` sideToMove contributes `2·vw[5]·p` (turnSign +1/−1);
* `v[6]` cardDiff contributes `2·vw[6]·p/54`;
* `v[7]` and `v[15]` contribute `−vw[7]·p/45` and `vw[15]·(2U−1)·p/45`;
* `v[9]` contributes `≈ 2·vw[9]·sumControl·p/9`, |·| ≤ 0.002.

Summing at `U ≈ 20` gives `dEV/dp ≈ 0.040–0.058`, i.e. `dV/dp = valueWeight·that ≈ 0.26–0.38`.
**Measured: 0.1479/0.47146 = 0.3137.** Only `v[3]` (via clipping of `sharp` at the band edges),
`v[4]` lockedDiff (via a `.995`/`.005` threshold crossing — constant at **90.34%** of decisions) and
`v[14]` contestedMass (a second-order `e(1−e)` term) carry any candidate-specific information beyond `p`.

**Consequence, measured:** across the candidate set, `V` is **84.03%** explained by a simple linear
regression on `p` alone (mean R² over 26,417 decisions); the residual sd is **0.00749** against
`V`'s own sd of **0.04047** and against the linear score's spread of **8.2699**. So the entire
16-feature value function amounts to:

1. a **+1.79% rescaling of the hit-probability coefficient**, plus
2. a genuinely new signal worth **0.09% of the score spread**, plus
3. a discrete `+vw[4]·p/9·valueWeight ≈ +0.16·p` bonus for asks that would push a half-suit over the
   `e > .995` lock threshold, at 9.66% of decisions.

Yet ablating it costs **1.9 points** — because at 43 candidates a 1.8% perturbation reshuffles which six
enter the chain/threat search (final move changes at 2.98% of decisions vs an argmax flip at 0.94%).
The value function is functioning as a tie-breaker on the search's input set, not as an evaluator.

### 7.2 Dead code

| what | where | why dead |
|---|---|---|
| first full scoring loop | `v05.hpp:512-519` | its output is only read at `588-590`, unreachable when `searchTopK > 1` (default 6). **Every candidate is scored twice.** |
| `greedyMAP` branch | `v05.hpp:749-753` | preceded by `feasibleDecl`'s unconditional return at `746` |
| plain `jointSequential(players)` branch | `v05.hpp:754-757` | same |
| `allow[]` | `v05.hpp:706, 712` | read only at `767` (non-`Fast` belief) |
| `players[]` + `v.decl.owner[i]` argmax loop | `v05.hpp:714-720` | overwritten at `742`; read only at `755`/`768` |
| `int used[NPLAY]` | `v05.hpp:625, 660` | declared, read, never written |
| `mine` | `v05.hpp:789, 792` | computed then `(void)mine;` |
| `asked[54][6]` | `v05.hpp:143, 158, 488, 493` | written on every own ask; read only under `repeatGuard = false` |
| `Rng rng` | `v05.hpp:135, 150, 195, 764` | both read sites are non-`Fast` belief modes |
| `BlockDP block`, `blockOk` | `v05.hpp:133-134, 166-174, 724-735, 930-937` | `belief != Block` |
| `cfg.particles` | `v05.hpp:37, 195, 764` | non-`Fast` only |
| `lockedAllocThresh`, `patientLocked` | `v05.hpp:62, 64, 808` | inside `if (locked)`, reached only when `!(useValue && valueDeclare)` |
| `gateAudit` machinery | `v05.hpp:100, 844-861, 862-872` | diagnostic switch, off |
| `pressure()` stages | `v05.hpp:698-699`, `722-723`, `801-802` | `nEvents` never reaches 220 |
| `bestGuess` fallback | `v05.hpp:913-943` | `feasibleAllocation` never failed in 250 games |
| `V05Agent::valueFeatures` | `v05.hpp:464-468` | only `fish fitvalue` calls it (`game.hpp:316`) |
| `v05_oppmodel.hpp`, `v05_target.hpp` | 1,181 lines | not included by the shipped binary |

### 7.3 Inert terms (present, live, but cannot move the argmax)

* **8 of 20 ask features depend only on the half-suit** (f3, f4, f7, f9, f12, f13, f15, f19). Between
  candidates sharing a half-suit — 43.51 candidates over 3.70 half-suits — they are constant.
* `f[13]` (contribSpread 0.035), `f[11]` (0.039) and `f[17]` (0.014) are below the value function's own
  0.148 spread: they rank **below** the entire 16-feature value function in discriminating power.
* `f[18] runway` is 99.4% collinear with `f[0]`.
* `ownershipByP` = false makes M1's second half a no-op.
* `f[12]`'s `lastMySet` is set at `v05.hpp:584/588`, i.e. *after* the pick, so it is genuinely the
  previous decision's half-suit; the term is a live +1.38 bonus for perseveration.

### 7.4 `priorPhi` is not an independent channel

`Knowledge::priorWeight` (`belief.hpp:100-108`) computes
`z = θ·a − φ·(totalAsks[p] − a) = (θ+φ)·a − φ·totalAsks[p]`. The second term is card-independent for a
fixed player, i.e. a pure column scaling, which `sinkhornDisj`'s column normalisation
(`belief.hpp:492-495`) removes exactly. So `φ` acts only as an increment to `θ`, up to the `±2.6` clip
at `belief.hpp:106`. Confirmed head-to-head: `v05` vs `v05:ptheta=0.56656,pphi=0` (= θ+φ) is **50.375%
[46.92, 53.83]** (n=800), while `v05` vs `v05:ptheta=0.44458,pphi=0` is 52.125%. The policy prior as a
whole is worth a lot — `v05` vs `v05:ptheta=0,pphi=0` is **59.375% [55.93, 62.73]** — but it is a
one-parameter channel, not two. (Defect K, unfixed. `research/v05/RESULTS-SUMMARY.md` flags that the fit
*raised* θ from 0.264 to 0.445 and that robustness against the deceptive archetypes is unmeasured.)

### 7.5 Where the compute actually goes

Single-threaded, 200 games, shipped configuration:

| entry point | calls | µs/call | share |
|---|---:|---:|---:|
| `proposeDeclaration` | 114,486 (6/event) | 61 | **87.2%** |
| `chooseAsk` | 17,205 | 59 | 12.5% |
| `bestGuess` + `willingForced` | ~0 | — | 0.0% |

The dominant cost of the entire policy is polling six seats for a voluntary declaration after every
public event, each poll running `evaluateSet` on ~2.2 half-suits, each of those running one
`jointSequential` = up to 6 `sinkhornDisj` refits (~4.4 µs each). None of that work is cached across
events or across the six seats, and 5 of the 6 polls per event are discarded by lowest-seat arbitration
(`game.hpp:210-224`).

---

## 8. Where the policy is myopic

Ordered by my estimate of how much a deeper computation would change the chosen action.

1. **Declaration timing has no lookahead at all.** `declareByValue` compares "cash now" against
   `value(pub, 0,0,0,0, …)` — literally the *current* state (`v05.hpp:795`). There is no model of what
   arrives between now and later, no model of the opponents declaring first, no model of the
   information the declaration leaks. Since the rule collapses to a `pAlloc` threshold at 0.809
   (§3.4), the whole apparatus is a constant. A 2-ply "declare now vs. one more ask then declare"
   comparison is the obvious replacement, and 6.74% of locked-half-suit verdicts already choose to wait.
2. **`askExpectedValue` ignores the target** (`v05.hpp:438`). On a miss the turn goes to a *specific*
   opponent; the miss branch uses a uniform `turnSign = −1`. The linear score patches this with four
   target features whose weights include two suspicious signs (f[11], f[16]). A miss-branch that
   evaluated the recipient's actual position would change the target choice at up to the 23.2% of
   decisions where the target is currently decided by non-`p` terms.
3. **The hit branch models one card, not the run.** A hit keeps the turn, so the true value of an ask is
   the expected length of the run it starts. This is approximated twice and badly — once by `f[18]`
   (99.4% collinear with `p`) and once by the `chain` term over 6 candidates (worth 0.0 points). An
   explicit expectimax over the continuation (2–3 asks deep, with the posterior updated) is unexplored.
4. **Neither branch models the certificate the ask emits.** An ask in half-suit `S` publicly proves the
   asker holds another card of `S` (constraint C5, `belief.hpp:16-20`); a miss proves the target lacks
   the card. v0.5 prices the leak with two crude indicator features (`f[9]`, `f[19]`) and a
   *negative* information weight (`f[14]`, −2.43). It never computes the actual information delta, for
   either team, although the engine already has the machinery (`sinkhornDisj` on a perturbed
   `Knowledge`, exactly as the chain/threat branches do).
5. **No model of the other five seats' knowledge.** 9.77% of v0.5's asks are inside a half-suit its own
   team already owns outright in ground truth (`fish pathology`, `diag.hpp:110-118`) — guaranteed
   misses that `provablyDead` cannot catch from one seat's `Knowledge` but that a shared-team-knowledge
   model would catch. This is the largest identified block of provably-worthless asks remaining, and it
   is 20× the rate at which M1's gate binds on the argmax.
6. **`feasibleAllocation` maximises the wrong objective.** It picks the MAP allocation by a product of
   *unconditioned* marginals (`v05.hpp:661`) and only joint-scores the winner. Exactly maximising the
   joint over ≤729 candidates is affordable, and `belief.hpp:557-563` already says why the two differ.
   Forced-endgame accuracy is 24.35% against a measured ~40.6% ceiling.
7. **The forced endgame declares half-suits in index order.** `Game::forcedEndgame` (`game.hpp:238-256`)
   sweeps thresholds then scans `s = 0..8`. `fish.hpp:120-124` explicitly notes that earlier
   declarations reveal allocations that sharpen later ones — nobody chooses the order.
8. **`choosePassTarget` is one-ply and unilateral** (`v05.hpp:945-968`): it scores each teammate by a
   product-form "can they ask somewhere juicy", with no model of what happens after their ask, and no
   channel for the teammate's own assessment. Rare (0.372 calls/game) so the payoff is small; the v0.5
   diagnosis priced a ladder at ~0.05 cards/game.
9. **The score has no scoreboard awareness that can rank anything.** `scoreDiff` is the single largest
   value-function coefficient and is *exactly constant* across candidates; `f[17]` is the only ask
   feature that sees the score and it has the smallest contribSpread of all 20 (0.014) and a sign
   flipped from v0.4. A policy that plays differently at 4-3 down with two half-suits left than at 1-0
   up does not exist here.
10. **Top-K = 6 out of 43.5.** The deep term is applied to 14% of the candidate set, chosen by a score
    whose value component is a 1.8% perturbation. Widening K is cheap relative to
    `proposeDeclaration`'s 87% share (`topk=24` costs 194 µs/ask vs 59, and total runtime rises only
    from 8.06 s to 10.4 s per 200 games).

---

## 9. Reproduction

```
cd engine && make
./fish match     --a=v05 --b="v05:<knob>=<v>" --games=2000 --seed=770077
./fish pathology --a=v05 --b=v05 --games=250 --rotations=2 --seed=31
# structural probes (research/v06/notes/probes/, compile against engine/src, modify nothing):
clang++ -std=c++20 -O2 -Iengine/src r1_anatomy.cpp -o r1_anatomy -pthread && ./r1_anatomy 300 4242
clang++ -std=c++20 -O2 -Iengine/src r1_decl.cpp    -o r1_decl    -pthread && ./r1_decl 250
clang++ -std=c++20 -O2 -Iengine/src r1_stale.cpp   -o r1_stale   -pthread && ./r1_stale 150
clang++ -std=c++20 -O2 -Iengine/src r1_target.cpp  -o r1_target  -pthread && ./r1_target 200
clang++ -std=c++20 -O3 -Iengine/src r1_split.cpp   -o r1_split   -pthread && ./r1_split 200
```
