# FishBot v0.7 — research log

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`.
Append-only. Each phase adds its own section; nothing above a section's heading is edited by a
later phase.

---

## Phase 1 — Rebuild the measuring instrument

Started from `db6066c` ("phase 0 v7"), working tree clean at the start of the session.
Inputs read: `docs/v07/THREAT-MODEL.md`, `docs/v07/SUBOPTIMALITY-LEDGER.md`,
`docs/v07/PHASE-PROMPTS.md`, and `engine/src/` (`arena.hpp`, `tuner.hpp`, `game.hpp`, `fish.hpp`,
`belief.hpp`, `factory.hpp`, `v05.hpp`, `v06.hpp`, `v06_rollout.hpp`, `main.cpp`).

**Machine.** Apple M5 Pro, 15 logical cores, `clang++ -O3 -march=native`, macOS 25.5.0. Every
throughput number in this cycle is from this machine and is not comparable to `E9-throughput.txt`,
which was measured elsewhere: `v06` mirror measures **364.7 games/s** here against E9's 303.4.
All v0.7 ratios are computed within this machine and within one basis.

### 1.0 A concurrent workstream in the same tree

Partway through the session a second workstream began editing `engine/src/serve.hpp`,
`engine/src/table.hpp`, `engine/src/httpd.hpp`, `engine/web/index.html`, `README.md` and
`docs/PLAY.md`, and added `engine/src/lobby.hpp` and `engine/src/tunnel.hpp` — a networked table with
invite codes, seat tokens and a tunnel. That work is unrelated to the instrument and none of its
files were touched here. It did, twice, leave `make` failing on a partially-written file
(`lobby.hpp:57` is a most-vexing-parse: `std::vector<unsigned char> v(size_t)` declares a function).

Rather than edit another workstream's files or wait on them, `main.cpp` gained a
`#ifndef FISH_NO_SERVE` guard around the `serve.hpp` include and the `serve` subcommand, and the
phase-1 battery builds its own binary:

```
clang++ -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -DFISH_NO_SERVE \
        src/main.cpp -o fish7 -pthread
```

Default behaviour of `make fish` is unchanged. Every v0.7 artifact in this cycle was produced by
`fish7`. This also means the throughput numbers were measured while another process was
intermittently compiling; the batteries were run when the machine was otherwise quiet, and the
single-thread column is the one to trust if a row looks anomalous.

### 1.1 What was built

| # | Thing | Where |
|---|---|---|
| B1 | Power arithmetic (98/√N) computed by the harness and emitted with every cell | `engine/src/v07_power.hpp`; wired into `printMatch`, `v7through`, `v7decide` |
| B2 | The reserved-seed registry the v0.6 paper claims exists | `engine/src/v07_seeds.hpp`; `fish7 seeds` |
| B3 | Deterministic paired-vector indexing and a work-stealing scheduler | `engine/src/arena.hpp` |
| B4 | Seed-bank sharding (`--shard=s/n`) | `engine/src/arena.hpp`, `main.cpp` |
| B5 | The per-decision evaluation channel | `engine/src/game.hpp` (`DecisionInfo`, `DecisionSink`), `v05.hpp`, `v06.hpp`, `fish7 v7decide` |
| B6 | Leaf-evaluator interface, truncation, batch evaluation | `engine/src/v07_leaf.hpp`, `v06_rollout.hpp`, `v06.hpp` |
| B7 | Per-seat rollout blueprints — the search pointed at exploitation | `RolloutConfig::oppSpec`, spec option `roppo=` |
| B8 | Planted-weakness targets | `V05Config::plantKind/plantStr`, spec options `hcap=`/`hstr=` |
| B9 | C2, the extended-feature responder | `engine/src/v07_responder.hpp` (`V07Responder`, spec `v07`) |
| B10 | C5, the white-box transcript-inversion responder and its bit probe | `engine/src/v07_invert.hpp`, `V07InvertAgent` (spec `v07i`), `fish7 v7bits` |
| B11 | The A2 ex-ante correlation device for the adversary | `correlationSignal()`, `--correlated`, spec option `corr=` |
| B12 | The detection-floor battery and the gate-first phase-1 battery | `engine/exploitability_v07.sh`, `engine/experiments_v07.sh` |

### 1.2 Defects found in the incumbent while building

**A-1 — the published bootstrap intervals are a function of the core count.**
`clusterBootstrap` resamples positions in `MatchStats::paired`, and `paired` was built by
concatenating per-thread strided runs, so its ORDER depended on
`std::thread::hardware_concurrency()`. Measured on E3's own cell (`v06` vs `v05`, seed 90210,
300 deals × 6): `--threads=1` gives `ci [0.491667, 0.538333]`, `--threads=4/8/15` give
`[0.491111, 0.537778]`. The win rate was thread-invariant throughout; the interval was not. No
published half-width in the corpus is reproducible on a machine with a different core count.
Fixed by indexing `paired` by deal, which makes the interval identical at every thread count and
equal to the single-threaded value. **Consequence for the record:** v0.4–v0.6 intervals are correct
to about ±0.06 points of the canonical value on the one cell measured, so no published conclusion
moves; but the reproducibility claim needed the fix.

**A-2 — the reserved-seed registry did not exist.** `paper/sections_v06/10-protocol.tex`
(`sec:protocol-seeds`) states that seed-bank disjointness "is enforced by a registry of the
\vsixReservedSeeds{} reserved seeds that the battery checks, rather than by discipline."
`\vsixReservedSeeds` is a two-element literal emitted by `engine/build_tables_v06.py:469` and
nothing checks anything. `engine/src/v07_seeds.hpp` is now that registry, back-filled with every
v0.4–v0.6 bank, and `fish7 seeds --require=...` fails a battery that names an unregistered or
still-sealed bank. On first run it found:

> `R1 VIOLATION seed 515253: 'v06/E4 per-style panel' (eval) and 'v06/X1 exploitability responder
> fit (exploitability_v06.sh FITSEED)' (fit)`

The v0.6 exploitability responder was fitted on the same deal bank the per-style panel is evaluated
on. The exploitability figure itself is not contaminated — its evaluation half is the disjoint bank
`6543210` (`exploitability_v06.sh:27`) — but the collision is real, it is the first thing anyone has
said about it, and under the registry's rules a v0.7 battery cannot repeat it.

**A-3 — a float leaf feature row breaks the search's tie behaviour.** The first version of the
leaf-evaluator refactor stored the feature row in `float`. That loses about 1e-8 relative precision
on the control term, which is enough to flip the search's lower-confidence-bound comparison at a few
decisions per hundred games: the depth-24 configuration moved from `winRate 0.55` to `0.50` on a
30-deal control against the pre-refactor binary. Switching the row to `double` was not sufficient
either, because floating-point addition is not associative and
`score + λ·(c₁+c₂+…)` is not bit-for-bit `((score + λ·c₁) + λ·c₂) + …`. The row now carries a
thirteenth slot accumulated in v0.6's own order, and `MaterialLeaf` reads that slot. With it, three
search configurations reproduce the pre-refactor binary exactly (§1.3). **This is a general warning
for phase 3: a learned leaf evaluator will want float, and float alone changes which move this
search plays.**

### 1.3 Identity controls

All run against a binary built from `db6066c` (`git archive HEAD`) as the reference, on the same
machine, same seeds, same n.

| Control | Result |
|---|---|
| `v06:legacy=1` mirror ≡ `v05` mirror, md5 of 60-game pathology dump | **PASS** (unchanged from v0.6) |
| `v07` with all twelve responder coordinates at zero ≡ `v06` | **PASS** |
| `v07i:inv=0` ≡ `v06` | **PASS** |
| `v06:s1=1,det=12,cand=4,kappa=2.5` vs reference binary, 14 deals × 2 | **IDENTICAL** on win rate, mean sets, ask accuracy, declaration accuracy, events/game |
| `v06:s1=1,det=8,cand=6,kappa=0` vs reference, 30 deals × 2 | **IDENTICAL** |
| `v06:s1=1,det=12,cand=4,kappa=2.5,depth=24` vs reference, 30 deals × 2 | **IDENTICAL** (after A-3's fix; DIFFERED before it) |
| `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=16,maxq=26` vs reference, 30 deals × 2 | **IDENTICAL** (after A-3's fix) |

So the leaf-evaluator interface, the truncation path, the batch-evaluation path, the
`resetWithKnowledge` fast reset, the work-stealing scheduler and the deterministic paired vector are
all behaviour-neutral on the incumbent, and every v0.6 search number remains valid under the new
engine.

### 1.4 The planted-edge ladder

Measured before any responder was fitted, on bank `7022001` at 6,000 deals × 2 = 12,000 games per
rung (98/√N = ±0.89 points). `dTrue` is the win rate the **unhandicapped incumbent** — a known
exploiter — reaches against the handicapped target, minus 50. Full table:
`research/v07/results/C0-ladder.txt`.

| handicap | `dTrue` (pts) | 95% CI | target's own decl. acc. | target's own ask acc. |
|---|---:|---|---:|---:|
| none | 0.00 | [0.00, 0.00] (mirror) | 0.9797 | 0.5402 |
| `hit` 0.02 | +0.02 | [−0.88, +0.92] | 0.9795 | 0.5403 |
| `hit` 0.05 | −0.35 | [−1.35, +0.65] | 0.9789 | 0.5391 |
| `hit` 0.10 | −0.10 | [−1.20, +0.98] | 0.9788 | 0.5371 |
| `hit` 0.20 | +0.07 | [−1.10, +1.20] | 0.9791 | 0.5330 |
| `hit` 0.40 | +0.37 | [−0.85, +1.60] | 0.9787 | 0.5240 |
| **`hit` 1.00** | **+4.15** | [+2.90, +5.38] | 0.9768 | 0.4806 |
| **`decl` 0.05** | **+0.85** | [+0.07, +1.63] | 0.9824 | 0.5335 |
| **`decl` 0.15** | **+2.70** | [+1.60, +3.80] | 0.9788 | 0.5182 |
| `decl` 0.40 | +2.77 | [+1.68, +3.85] | 0.9787 | 0.5181 |
| `decl` 1.00 | +2.77 | [+1.68, +3.85] | 0.9787 | 0.5181 |
| `prior` 0.25 | +0.05 | [−1.10, +1.18] | 0.9786 | 0.5352 |
| **`prior` 0.60** | **+2.23** | [+1.05, +3.40] | 0.9771 | 0.5300 |
| `prior` 1.00 | +5.97 | [+4.73, +7.18] | 0.9730 | 0.5251 |
| `gate` 0.02 | −0.03 | [−0.30, +0.25] | 0.9796 | 0.5402 |
| `gate` 0.05 | +0.03 | [−0.73, +0.78] | 0.9794 | 0.5399 |
| `gate` 0.15 | +0.40 | [−0.58, +1.38] | 0.9792 | 0.5402 |
| `gate` 0.40 | −0.92 | [−2.05, +0.23] | 0.9782 | 0.5405 |
| `leak` 0.15 | −0.15 | [−0.83, +0.55] | 0.9793 | 0.5392 |
| `leak` 0.50 | −0.42 | [−1.38, +0.55] | 0.9791 | 0.5383 |

Four things in that table are results in their own right.

1. **The ask-score handicap is almost entirely flat until it is total.** Scaling the
   hit-probability weight down by 40% costs 0.37 ± 1.2 points and moves ask accuracy 1.6 points
   (0.5402 → 0.5240); only zeroing it outright produces a measurable edge. The one calibration point
   the corpus has is therefore at the far end of a very nonlinear curve, and it is not evidence that
   the instrument can see anything smaller.
2. **The declaration channel is the sensitive one**, which is exactly what the ledger's arithmetic
   says it should be (§0.2: one point of declaration accuracy ≈ 1.2 points of win rate). Delaying
   declarations by `declareMargin += 0.025` is already worth 0.85 points. It is the only knob in the
   ladder that produces a *graded* sub-3-point edge, so it is what the floor is measured on.
3. **Disabling the live-ask gate M1 on up to 40% of decisions costs nothing measurable.** `gate 0.40`
   is −0.92 [−2.05, +0.23]. That is not what the v0.5 record implies — with `m1=0` at v0.5's weights,
   dead asks reach 46.28% and 14.4% of games hit the action cap. The reconciliation is in the
   per-decision channel: at v0.6's fitted weights the gate binds on what would have been the argmax
   at only **0.95%** of ask decisions (§1.5), because `wTeamHas` is fitted negative and the ownership
   incentive that caused v0.4's two-question cycle is priced out. **M1 is load-bearing for v0.5 and
   very nearly inert for v0.6.** This is directly relevant to ledger entry L10, which treats M1 as an
   untested exploitability hazard: the hazard is real in principle but the surface is small.
4. **`leak` is a readability handicap that costs nothing in play.** Biasing the ask score by the
   number of cards the asker holds in the half-suit — which makes the choice of half-suit a monotone
   read-out of the asker's holding — measures −0.15 and −0.42 points. That is what makes it the right
   probe for class C5: a strength-seeking responder has nothing to find there, and a transcript
   reader does.

### 1.5 The per-decision channel, first output

`fish7 v7decide --a=v06 --b=v06 --games=150 --seed=7011001`, 14,104 decision records from 300 games,
each rate with a deal-clustered bootstrap interval:

| metric | rate | 95% CI | decisions | 98/√n |
|---|---:|---|---:|---:|
| `askHitRate` | 0.53701 | [0.5248, 0.5499] | 12,754 | 0.868 |
| `ownLockedAskRate` | 0.10373 | [0.0901, 0.1179] | 12,754 | 0.868 |
| `tieShare` (of contested decisions) | 0.60574 | [0.5980, 0.6138] | 12,672 | 0.871 |
| `gateBindRate` | 0.00949 | [0.0064, 0.0130] | 8,538 | 1.061 |
| `deadAskRate` | 0.00000 | [0, 0] | 12,754 | 0.868 |
| `declAccuracy` | 0.97402 | [0.9651, 0.9822] | 1,347 | 2.670 |
| `declAllocErrorShare` | 0.91429 | [0.8158, 1.0000] | 35 | 16.565 |

Three notes. `ownLockedAskRate` 10.37% is the same channel `E2-pathology.txt` reports at 11.69% on a
different bank, so ledger entry L3 replicates. `tieShare` is measured here at the top group within
1e-12 over decisions with two or more candidates, which is not identical to `E8-ties.txt`'s
bit-for-bit definition; the two are not interchangeable and the definition is printed with the
number. `declAllocErrorShare` — of the wrong declarations, the share where the team physically held
all six cards and named the wrong teammate — comes out at 0.914 against the ledger's 72–75%, but on
**35** declarations, half-width 16.6 points; it is consistent with the ledger and settles nothing.

That last row is the whole argument for the channel: a per-game battery would need roughly
`98²/16.6² ≈ 35` times the games to say anything about it, and per declaration it is one command.

### 1.6 The white-box class, C5: what transcript inversion actually found

This is the ledger's L4 and the threat model's C5 — "never built anywhere; no external precedent
found". The measurement is `fish7 v7bits`, and it is the ledger's "cheapest experiment": no games
beyond the transcripts themselves.

**The construction.** At each ask by a target seat, from one observer's seat: draw D deals from the
observer's exact posterior (`DealDP` is an exact sampler for constraints C1–C4; C5 is enforced by
rejection, so accepted draws are exact posterior samples), reconstruct the actor's information set
under each draw, and ask the *known* policy what it would have played. The fraction q that
reproduces the observed ask is the posterior mass the action leaves alive; −log₂ q is the
contraction, and it is a contraction **beyond the certificates**, because every sampled deal already
satisfies all of them.

**Result 1 — the contraction is large.** `v06` observing `v06`, bank 7012001, D = 64:

> **1.78 bits per ask** (SE 0.067), mean surviving fraction **0.473**.

By event index: 2.66 bits over events 0–19, 2.09 over 20–39, 1.86 over 40–59, 1.40 over 60–79,
0.69 over 80–99, 0.28 beyond. Each observed ask by a deterministic FishBot eliminates about half of
the deals its own certificates leave standing, and it does most of that early, when the posterior is
widest. The ledger set the kill threshold at "under ~1 bit per ask the hypothesis dies for free";
**it does not die.**

**Result 2 — converting bits into a better posterior is much harder than measuring them.** The
first accumulator — per-(card, seat) log-likelihood ratios summed across the game — made the
observer's marginals **worse**, not better: 2.199 nats against a 1.416-nat baseline, argmax 0.3023
against 0.3346. Two causes, both real and both worth recording:

* **Estimator variance.** With D = 64 draws over six possible owners a (card, seat) cell sees about
  ten Bernoulli samples; an unshrunk ratio from ten samples is mostly noise, and summing 25 of them
  across a game builds a confident, wrong prior.
* **Naive-Bayes over-counting.** Actions by the same seat carry overlapping information; summing
  their log-likelihood ratios multiplies a confidence that should have been shared.

The repairs are the two the statistics of the situation dictate: shrink each cell's conditional
toward the overall rate by κ pseudo-draws, and temper the sum by a coefficient. **The tempering
coefficient is not a fudge factor: it is `priorTheta`/`priorPhi`.** v0.5's policy prior is a
hand-fitted two-parameter approximation to exactly this quantity, and ledger entry C2 already
records that the policy prior is the *entire* difference between the exact posterior and the
deployed approximation as predictors. C5 is that channel with the heuristic replaced by a
measurement.

The third repair is the one that mattered most, and it is structural rather than statistical. Given
the public state, the observed action is a function of the **actor's** hand and of nothing else, so
P(a | owner(c) = p) differs from P(a) at first order only for p = the actor. Updating all six
columns spends most of the sample estimating second-order effects the sample cannot resolve. The
sweep (bank 7012001, 20 games, ≈35,000 scored unresolved cards):

| update rule | gain | nats (base → inverted) | argmax (base → inverted) |
|---|---:|---|---|
| all six columns, all cards | 0.2 | 1.4160 → 1.4778 | 0.3346 → 0.3367 |
| all six columns, all cards | 1.0 | 1.4160 → 2.5784 | 0.3346 → 0.3009 |
| actor's column only | 0.2 | 1.4160 → 1.4190 | 0.3346 → 0.3294 |
| actor's column only | 1.0 | 1.4160 → 1.6330 | 0.3346 → 0.3134 |
| actor's column, half-suits the actor has asked in | 0.2 | 1.4160 → 1.4104 | 0.3346 → 0.3384 |
| **actor's column, half-suits the actor has asked in** | **1.0** | **1.4042 → 1.3940** | **0.3434 → 0.3503** |
| actor's column, half-suits asked in, heuristic prior removed | 1.0 | 1.4042 → 1.4025 | 0.3434 → 0.3447 |

(The last three rows are at 10 games rather than 8, which is why their baselines differ; the
comparison is always within a row.)

**Result 3 — the conversion is real but small.** At the swept optimum the measured inversion
improves the observer's marginals on both criteria at once — the only setting in the sweep that does
— by **0.0102 nats** and **+0.69 points of argmax accuracy**. Replacing the fitted heuristic prior
with the measurement is *worse* than adding the measurement to it, which says the two are not
substitutes: the heuristic is carrying something the one-step inversion does not.

**The honest limitation, stated here rather than in a footnote.** The accumulator marginalises. The
exact object is the intersection of the certificate posterior with every policy constraint, and its
acceptance rate is about 0.47^25 ≈ 10⁻⁸, so it cannot be sampled by rejection; a joint particle
filter over a *static* hidden state degenerates and its rejuvenation step discards exactly the
evidence it is trying to keep. The marginal accumulation is a Rao-Blackwellised approximation that
treats different actions as conditionally independent given the certificates. It is an
**under-estimate** of what a perfect inverter would extract. So: **a positive result here is a lower
bound, and a null result does not close the class.** The 1.78-bit contraction is not approximate in
this way — it is an exact one-step measurement.

**What this means for phase 2 and 3.** The determinism hypothesis survives its first real test: the
transcript of a deterministic FishBot is readable, at 1.78 bits an ask. What phase 1 has *not* shown
is that the readability is worth much: the best marginal conversion is 0.7 argmax points, and at the
ledger's own conversion (1 point of declaration accuracy ≈ 1.2 points of win rate) that is not
obviously a point of win rate. Ledger entry L4's own kill condition was "the bit measurement coming
back small"; it did not. Its second kill condition — "the follow-on responder failing to convert a
large contraction into win rate" — is what phase 2 has to settle, and it is now settle-able, because
the responder exists.

### 1.7 The responder classes, and what each one is

The corpus's single exploitability figure is a **C1 / A1 / k = 3** number: one 34-coordinate linear
responder family, three identical copies, fitted with the frozen target as its entire panel. Phase 1
adds three classes and repairs three defects in the first.

**C1 — in-class linear, repaired.** Three changes, each keyed to a ledger defect:

* *P-3a, the responder was not in the target's class.* `exploitability_v06.sh:21` defaults
  `BASE=v05` and the v0.6 run used it, so a `V05Agent` running the chain/threat pass was fitted
  against a `V06Agent` that does not run it. Here `BASE` is the target's own class, and `--fromv6`
  seeds the search at **the incumbent's own vector** rather than at v0.5's defaults. An exploiter of
  v0.6 should start from v0.6.
* *P-3b, the fitted responder was itself degraded* (declaration accuracy 0.9550 against 0.9826, and
  roughly seven times the forced-endgame rate). Every row of the floor artifact carries the
  responder's own KPIs — `declAccA`, `forcedPerGameA`, `limitHitRate` — so a broken exploiter cannot
  be read as a hard target.
* *P-3c, the budget could not resolve the effect.* The budget is a parameter, it is printed into the
  artifact, and §1.9 reports the curve rather than a single point.

A fourth defect was found while wiring the battery and is new here: **`tune --panel` splits on `,`,
and a spec with options contains commas**, so a one-target exploitability panel of
`v06:hcap=decl,hstr=0.15` silently became a panel of *two* members, the second of which is not a
policy. The v0.6 probe never hit this because its targets (`v04`, `v05`, `v06`) carry no options.
`--panel` now takes `;` as the separator when one is present and restores `+` to `,` inside a
member.

**C2 — extended features (`v07`).** v0.6's score plus twelve coordinates it does not have:
`targetKnownStrength`, `targetSetMass`, `oppTeamSetMass`, `turnDonationCost`, `targetThreat`,
`roleHit0`, `roleHit1`, `roleClaim`, `targetAskedHere`, `targetMissedHere`, `phaseHit`,
`deadDonation`. Four groups, each answering something the threat model names:

* *per-target and opponent-hand terms* — v0.6 prices the single card asked for; nothing in its
  feature set prices what else the target is holding, or what it will do with the turn a miss hands
  it;
* *seat-role terms* — `roleClaim` is an agreed division of half-suits among the three responder
  seats, which is precisely the role-differentiated attack the A1 regime "structurally cannot
  express" (THREAT-MODEL.md §4.3). It is legal for the adversary and illegal for the v0.7 team (H2);
* *deviation timing* — `phaseHit` lets the fit choose *when* to deviate, which is the one
  LBR-specific procedure the threat model says must be imported: Lisý and Bowling report that a
  greedy responder allowed to act too early understated exploitability by an order of magnitude;
* *`deadDonation`* — the coordinate that lets a **linear** score price a deliberate miss. v0.6 can
  only hand dead candidates to a rollout, because "the linear score cannot price a deliberate miss:
  the hit-probability term is zero on such an ask by definition and every remaining term is a
  penalty, so sweeping the admission margin gave bit-identical output at every setting" (ledger L14).
  With `dead7=1` the dead candidates enter the scored set and this coordinate is what can pay for
  them.

With all twelve at zero the class is v0.6 bit for bit, which is the identity control in §1.3.

**C3 — search-based (`roppo=`).** The v0.6 rollout seats six copies of one blueprint, so the search
maximises value **against a mirror of itself**. That is the right choice for self-improvement and
the wrong one for exploitation. `RolloutConfig::oppSpec` lets the three opposing seats of the
rollout carry the *target's* policy, which the threat model's white-box grant (T3) makes legitimate.
One string is the difference between a search that improves a policy and a search that
best-responds to one, and the corpus has only ever run the first. Cost: modelling the target's
scoring rule *and* its posterior exactly costs **8.3×** (26.9 → 3.25 games/s, measured); the battery
carries the scoring rule exactly and the posterior cheaply, and says so.

**C5 — white-box inversion (`v07i`).** §1.6.

**A2, the adversary's correlation device.** `correlationSignal()` is drawn per game by the arena
from a stream keyed by a constant of its own, handed to no policy through any argument. A responder
reading it with `corr=K` selects one of K role plans; one that does not read it is A1 by
construction, so the two regimes differ by one option and run on the same code path. The device is
built and available; phase 1 measures A1 with seat-conditioned features and leaves the A2 fit to
phase 2. **This is the fallback THREAT-MODEL.md §10 item 4 asks to be declared, and it is declared:
phase 1's headline regime is A1, not A2.**

### 1.8 Throughput: where the search's cost actually is

Before any engineering, the four knobs the engine already had were swept against the shipped search
configuration (`v06:s1=1,det=12,cand=4,kappa=2.5` against `v06`, 30 deals × 2, all threads):

| configuration | games/s | ratio |
|---|---:|---:|
| baseline search | 1.505 | 1.00× |
| `+ rbelief=indep` (the cheap blueprint) | 7.964 | 5.29× |
| `+ depth=24` (truncation) | 2.546 | 1.69× |
| `+ rbelief=indep,depth=24` | 19.888 | 13.2× |
| `+ rbelief=indep,depth=24,maxq=26` (endgame-restricted) | 64.453 | 42.8× |

Two things follow. First, **the throughput was almost entirely available in knobs the corpus already
shipped and never combined** — the cheap-blueprint frontier is measured in `E13-rollout.jsonl` and
the depth cut has been in `RolloutConfig` since v0.6. What was missing was a reason to combine them
and a leaf evaluator to make the depth cut mean something, which is what §1.3's interface is for.
Second, these are **different operating points, not a faster version of the same search**, so the
throughput claim is worth nothing without a strength measurement at the same point; that is battery
T2.

### 1.9 The gates, and two gate failures that were the gate's fault

The phase-1 battery is ordered gate-first, as the v0.6 battery is and for the same reason. Five
gates now run before any strength number: the seed registry, three identity controls, the pathology
KPIs for every class that will be measured, a truncation-determinism check and a sharding check.

`research/v07/results/G1-identity.txt`, first run:

```
V05/V06 IDENTITY PASS
V07 IDENTITY PASS
V07I IDENTITY PASS
TRUNCATION DETERMINISM FAIL
SHARD PARTITION FAIL
```

Both failures were defects **in the gate**, not in the engine, and both are worth recording because
each is a way of writing a test that reports a false alarm:

* *Truncation determinism* hashed the raw `match --json` output, which contains `seconds` and
  `gamesPerSec`. Those are wall-clock and differ run to run by construction. With the timing fields
  stripped the two digests are identical (`e9a6245893ed5af279ad22ccc2e2fb4b` twice).
* *Shard partition* reconstructed the whole run's win count from `winRateA`, which is printed to six
  significant figures, and then compared it to the shards' integer sum with a tolerance of 1e-6:
  |128 − 127.99992| = 8 × 10⁻⁵ and the gate failed an exact partition. Checked by hand, the whole run
  is **128 wins over 120 deals** and the three shards are **40 + 44 + 44 = 128 wins over
  40 + 40 + 40 = 120 deals**. The partition is exact.

Both gates are fixed and the fix is commented at the site. The engine passed both from the start.

**G2, pathology, for every class that will be measured** (`research/v07/results/G2-pathology.txt`,
200 deals × 2, seed 31):

| mirror | dead runs | mean length | longest | action-limit games |
|---|---:|---:|---:|---:|
| `v06` | 6 | 1 | 1 | 0 (0%) |
| `v07` | 6 | 1 | 1 | 0 (0%) |
| `v07i:idet=48` | 0 | 0 | 0 | 0 (0%) |

The commit gate runs before any strength number, and all three classes clear it.

### 1.10 The per-decision channel at battery scale

`fish7 v7decide --a=v06 --b=v06 --games=400 --seed=7011001`, 37,835 records from 800 games:

| metric | rate | 95% CI (deal-clustered) | decisions | 98/√n |
|---|---:|---|---:|---:|
| `askHitRate` | 0.54032 | [0.5327, 0.5482] | 34,235 | 0.530 |
| `ownLockedAskRate` | 0.10644 | [0.0982, 0.1150] | 34,235 | 0.530 |
| `tieShare` (of contested) | 0.60371 | [0.5990, 0.6081] | 34,018 | 0.531 |
| `gateBindRate` | 0.00938 | [0.0079, 0.0110] | 34,235 | 0.530 |
| `deadAskRate` | 0.00000 | [0, 0] | 34,235 | 0.530 |
| `declAccuracy` | 0.97663 | [0.9716, 0.9816] | 3,594 | 1.635 |
| `declAllocErrorShare` | **0.88095** | **[0.8101, 0.9444]** | 84 | 10.693 |

**A new number for ledger entry L1.** The ledger sizes the pure-allocation share of remaining
misdeclarations — the team held all six cards and named the wrong teammate — at **72–75%**, from
counts of **46/64** for v0.5 and **40/53** for v0.4 (`R0` V6-M9, via R6). There has never been a
figure for **v0.6**. Here it is: **88.1% [81.0, 94.4]** over 84 wrong declarations, and the interval
excludes 75%. The channel L1 identifies is *larger* in the incumbent than in either predecessor, and
the mechanism L1 names — a feasibility-constrained per-card argmax over a product of independent
marginals — is what is producing it.

That is also the argument for the channel in one line. This number has a half-width of 10.7 points
at 800 games because there are 84 wrong declarations in 800 games; getting the same resolution from
a per-game statistic is not a matter of a bigger battery, it is a matter of a battery about 35 times
bigger.

### 1.11 Transcript inversion at battery scale

`fish7 v7bits`, 120 deals × 2 per cell, D = 64 draws per inverted action, ~10,300 inverted asks per
cell (`research/v07/results/W1-inversion.jsonl`):

| observer | target | bank | bits/ask | SE | surviving fraction | nats base → inverted | argmax base → inverted |
|---|---|---:|---:|---:|---:|---|---|
| `v06` | `v06` | 7012001 | **1.9454** | 0.0180 | 0.4441 | 1.40094 → 1.39194 | 0.3515 → 0.3573 |
| `v06` | `v05` | 7012001 | **2.0583** | 0.0183 | 0.4214 | 1.39014 → 1.38685 | 0.3555 → 0.3612 |
| `v06` | `v06` | 7012002 | **1.9933** | 0.0181 | 0.4345 | 1.38691 → 1.37969 | 0.3595 → 0.3649 |

The contraction replicates across banks and across targets at about **2 bits per observed ask**, and
it is larger against v0.5 than against v0.6 — v0.5's score is the one with the chain/threat pass, so
its choices are a sharper function of its hand. Every observed ask by a deterministic FishBot
eliminates **56–58% of the deals its own certificates leave standing**.

The conversion into a better posterior replicates too, and stays small: **+0.58, +0.57 and +0.54
points of argmax accuracy** on the three cells, and 0.009, 0.003 and 0.007 nats. Consistent, real,
and an order of magnitude smaller than the bits would suggest if the evidence were independent —
which is precisely what §1.6 says the marginal accumulation costs.

### 1.12 The detection floor

`engine/exploitability_v07.sh`, artifact `research/v07/results/C1-floor.jsonl`. Fitting budget 12
generations × population 16 × 250 deals × 2 rotations = 96,000 games per fit, on bank 7020001;
evaluation on two fresh disjoint banks, 7021001 and 7021002, at 12,000 games each (98/√N = ±0.89)
for C1, C2 and C5, and 4,000 games each for C3.

**The control rung is not zero, and that is the first result.** A properly specified in-class
responder reaches **+0.76 points [+0.15, +1.37]** against the *unhandicapped* incumbent, pooled over
24,000 games and replicated in sign on both banks. The corpus's own probe reached **−1.64**
(48.36%, "did not reach parity") against the same target. Three repairs account for the difference,
all of them predicted by ledger entry P-3: the responder is now in the target's own class rather
than v0.5's, it is seeded at the incumbent's own vector rather than at v0.5's defaults, and the
`--panel` comma defect (§1.7) is fixed. **v0.6's in-class exploitability is at least 0.76 points, not
"below the probe's floor".**

The four classes order exactly as class power predicts:

| class | pooled edge vs unhandicapped `v06` | 95% CI | n | replicated in sign |
|---|---:|---|---:|:--:|
| C1 in-class linear (fitted) | +0.76 | [+0.15, +1.37] | 24,000 | yes |
| C2 extended features (fitted) | +1.05 | [+0.43, +1.66] | 24,000 | yes |
| C5 white-box inversion (**unfitted**) | +1.52 | [+0.92, +2.13] | 24,000 | yes |
| C3 search-based (fitted blueprint + rollout) | +1.86 | [+0.78, +2.94] | 8,000 | yes |

**The floors.** A class detects a rung when its **excess over the control** excludes zero on **both**
banks. The excess, not the raw edge, because the control is not zero; both banks, because the
project's replication rule says so.

| class | detection floor | rungs detected |
|---|---:|---|
| C1 | **2.45 pts** | `decl 0.15` (+2.45), `hit 1.0` (+3.85) |
| C2 | **2.31 pts** | `prior 0.6` (+2.31), `decl 0.15` (+2.45), `hit 1.0` (+3.85) |
| C3 | **2.45 pts** | `decl 0.15` (+2.45), `hit 1.0` (+3.85) |
| C5 | **1.68 pts** | `leak 1.5` (+1.68), `prior 0.6`, `decl 0.15`, `hit 1.0`, `leak 4.0` (+5.57) |

Nobody detects the +0.62-point rung. So **the floor is bracketed in (0.62, 2.31]** and the finer
rungs at `decl 0.08` and `decl 0.11` are running to narrow it.

**The `excess − dTrue` column is the interpretive key, and it separates two things the raw numbers
conflate.** `dTrue` is what the *unhandicapped incumbent* — a known but not target-specific
exploiter — gains from the handicap, so it measures the strength the target LOST. A class whose
excess exceeds `dTrue` is exploiting the planted weakness specifically; a class whose excess merely
tracks `dTrue` is collecting the lost strength that any stronger opponent would collect.

| class | rung | `excess − dTrue` |
|---|---|---:|
| C1 | `decl 0.15` / `hit 1.0` | **+1.36 / +3.62** |
| C2 | `decl 0.15` / `hit 1.0` | −0.16 / **+2.84** |
| C3 | `decl 0.15` / `hit 1.0` | **+2.49 / +4.06** |
| C5 | `decl 0.05` / `decl 0.15` / `hit 1.0` / `prior 0.6` / `leak 1.5` / `leak 4.0` | −0.06 / +0.10 / +0.03 / −0.40 / +0.28 / +0.17 |

**The fitted classes overshoot; C5 does not, on any of six rungs.** C1 finds 3.62 points more than
the reference exploiter on `hit 1.0`; C3 finds 4.06 more. C5's excess sits within ±0.4 of `dTrue`
everywhere, including on **both readability rungs**, which are the rungs it was built for.

That is a clean negative and it is the phase's most important qualification. **The +1.52 points C5
scores against the unhandicapped incumbent is a general strength gain from a sharper belief, not an
exploitation-specific advantage.** Inverting the transcript makes the responder play better against
whatever it faces; it does not make it better *at finding a specific weakness*, and it does not gain
extra ground when the target is deliberately made more readable. The hypothesis that a white-box
inverter is a sharper exploitability instrument than a fitted in-class responder is, on six rungs,
**not supported** — even though the same responder is the strongest single arm in the table against
the unhandicapped incumbent.

Two things follow for later phases. For **phase 2**, C5 is a strong *opponent* and a weak
*instrument*: it belongs in the panel, not in the detection apparatus. For **phase 3**, the finding
inverts into a candidate: a +1.5-point general strength gain, unfitted, from replacing a two-
parameter hand-fitted policy prior with a measured one, is a larger effect than anything v0.6
achieved, and it arrives with the ledger's own C2 entry as its prior evidence.

**The responders are not degraded.** Ledger P-3b records that the v0.6-targeting responder's own
declaration accuracy fell 2.76 points to 0.9550 with roughly sevenfold the forced-endgame rate. Every
fitted responder here runs at 0.9766–0.9785 declaration accuracy against the target's 0.9797, with
forced-endgame rates of 0.0059–0.0067 against the target's 0.0051 and no action-limit games. The
instrument is measuring a target, not a broken exploiter.

### 1.13 Fast-search strength: the throughput claim is not vacuous

`research/v07/results/T2-searchstrength.jsonl`, each configuration against `v06` on two disjoint
banks (7010001, 7010002). Timing in this table is contaminated by concurrent fits (the same
configuration shows 70 and 28 games/s on the two banks); the clean 400-deal re-timing is T1b and
strength is unaffected.

| configuration | bank A | bank B | pooled | n/bank |
|---|---:|---:|---:|---:|
| `rbelief=indep,depth=24` (no maxq) | 50.08 | — | +0.08 | 4,000 |
| `det=16,cand=6,kappa=2.0,maxq=26,rbelief=indep,depth=24` | 50.29 | 50.42 | +0.35 | 12,000 |
| `det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26` | 51.94 | 51.20 | **+1.57** | 12,000 |
| `det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` | 52.12 | 52.26 | **+2.19** | 12,000 |
| **F-mid** `det=16,cand=6,kappa=2.0,maxq=26` (shipped vector, full rollouts) | 52.77 | 52.27 | **+2.52** | 3,000 |

Three results.

**R1 — F-mid finally has its shipped-vector head-to-head, and ledger L2 was right.** The corpus's
state of evidence was "exactly one measurement on the shipped vector, on one bank, and no
head-to-head number at all" (L2), with a paired-panel point estimate of +1.35 that the published
table printed with its sign inverted. Measured directly: **+2.52 points over `v06`, replicated on
both banks** (52.77 [51.27, 54.27] and 52.27 [50.80, 53.73], n = 3,000 each). The strongest
configuration the corpus ever measured is confirmed stronger than the deployed policy at ~40× its
cost rather than 300×.

**R2 — the truncated fast path holds most of that strength at a fraction of the cost, and this
contradicts the inherited conditional.** `depth=12,maxq=26` on the cheap blueprint scores **+2.19
pooled, replicated** (52.12 / 52.26, n = 12,000 per bank — better powered than R1). The v0.6
conclusion says search "should be re-opened only after the leaf evaluator is rebuilt, because the
present one is algebraically close to a rescaling of the hit probability and cannot support a
depth-limited search" (ledger L9). The evaluator in this configuration IS that leaf (`MaterialLeaf`
≡ v0.6's `leafValue`, bit for bit), the truncation is aggressive (12 events), and the configuration
beats the deployed policy by two points, replicated. Whatever a *better* evaluator buys, the claim
that the present one *cannot support* a depth-limited search is *refuted at maxq=26* — in the
endgame regime, where the belief is sharp and 12 events reach the end of most games anyway. The
conditional may still bind for a full-game search; `depth=24` *without* maxq measures +0.08, so it
does bind there.

**R3 — the gain is not monotone in budget.** `det=16,cand=6` with the same cheap-blueprint+truncation
treatment measures +0.35 unseparated, while `det=12,cand=4` measures +1.57 — fewer determinizations
and fewer candidates, more strength. And shallower truncation (12 vs 24) is *better*. Both echo the
corpus's own optimizer's-curse finding: more candidates and deeper rollouts add variance to the LCB
comparison faster than they add signal. Attribution of exactly where the gain sits is phase 2/3
work; what phase 1 needed was a fast configuration whose strength is measurable, and there are now
two, replicated, at 12,000 games per bank.

### 1.14 W2: why C5 gained nothing on the readability rungs

`research/v07/results/W2-leakbits.jsonl` — the bit probe pointed at the handicapped targets, with
the oracle modelling the handicapped policy exactly (the white-box grant extends to the handicap;
it is part of the published policy). 100 deals × 2, D = 64, ~8,600 inverted asks per cell.

| target | bits/ask | SE | argmax base → inverted |
|---|---:|---:|---|
| `v06` | 1.9455 | 0.0198 | 0.3537 → 0.3594 |
| `leak 0.5` | 1.9365 | 0.0198 | 0.3515 → 0.3576 |
| `leak 1.5` | 1.9676 | 0.0202 | 0.3527 → 0.3583 |
| `leak 4.0` | 2.0147 | 0.0203 | 0.3624 → 0.3683 |
| `tell 1.5` | 2.0705 | 0.0203 | 0.3458 → 0.3500 |

**The planted "readability" handicap adds almost no inversion-accessible information: +0.02 to
+0.07 bits per ask on a baseline of 1.95, and the argmax conversion is flat across the ladder
(+0.56 to +0.61 points everywhere).** The explanation for §1.12's negative is therefore structural,
not a failure of the responder: **the unhandicapped deterministic policy is already close to fully
readable in the one-step sense.** Every ask is a pure function of hand and transcript, and it
already contracts the posterior by ~1.95 bits; biasing the score toward held half-suits changes
*which* deals survive the inversion, not *how many*. There was almost nothing extra for C5 to
collect on the leak rungs — which is also why `leak`'s dTrue at higher strengths (+1.68 at 1.5,
+5.57 at 4.0, from the floor battery's own ground-truth rows; near zero at 0.15–0.5) is a
*strength* cost (the bias overrides good ask choice), not a readability cost.

Consequence for the ledger: L4's planted-readability calibration idea has a measured ceiling. A
linear score handicap cannot make a deterministic FishBot meaningfully *more* readable than it
already is, so the white-box class cannot be calibrated by planted readability within this family —
only by planted strength, where it behaves like every other class. The determinism itself is the
readability, and it is uniform across the family.

Housekeeping: the first W2 run died on a spec bug of this session's own making — the battery
converted `,` to `+` in `--model`, which is only needed for specs nested *inside* another spec
(`imodel=`, `roppo=`); a top-level CLI argument takes commas as-is, and `makeAgent` exits on the
mangled form. Fixed in the battery; the artifact above is the correct run.

### 1.15 The budget curve, and the gates on re-run

**Gates.** With the two defective gate tests repaired (§1.9), the full battery reports:

```
V05/V06 IDENTITY PASS · V07 IDENTITY PASS · V07I IDENTITY PASS
TRUNCATION DETERMINISM PASS · SHARD PARTITION PASS
```

**The budget curve** (`research/v07/results/C2-budget.jsonl`), C1 against `decl 0.15`
(dTrue +2.45), fit seed 7020002, evaluated at 12,000 games per bank on 7021001/7021002:

| fitting budget (games) | bank A | bank B | pooled dFound |
|---:|---:|---:|---:|
| 21,600 (6 × 12 × 150) | +3.02 | +3.38 | **+3.20** |
| 96,000 (12 × 16 × 250) | +3.67 | +4.10 | **+3.88** |
| 280,000 (20 × 20 × 350) | +4.64 | +3.53 | **+4.08** |

The curve rises and flattens. Two readings matter. First, the v0.6 probe's defect was not
primarily budget: even a 21,600-game fit — a quarter of the v0.6 probe's ~155,000 — finds +3.2
points once the responder is in the right class and seeded at the incumbent, so **P-3a (class
mis-specification), not P-3c (budget), was the binding defect.** Second, the marginal point from
tripling the budget past 96,000 games is ~0.2 and the two banks disagree by 1.1 points at the top
rung, so the standard budget for phase 2 stays at 12 × 16 × 250 and the curve, not the scalar, is
what gets reported — as the threat model requires (T6, "report the curve, not the scalar").

### 1.16 The finer rungs: the floor, finished

Two rungs at `decl 0.08` (dTrue +0.86 [+0.29, +1.42]) and `decl 0.11` (+1.81 [+1.07, +2.55]), plus
the comparison §1.12 still owed: the **fitted** C1 on the readability rung C5 had detected.

| class | rung | dTrue | pooled excess over control | excess − dTrue | detected |
|---|---|---:|---:|---:|:--:|
| C1 | `decl 0.08` | +0.86 | +0.69 | −0.17 | no |
| C2 | `decl 0.08` | +0.86 | +0.23 | −0.63 | no |
| C5 | `decl 0.08` | +0.86 | +0.98 | +0.13 | no |
| C1 | `decl 0.11` | +1.81 | +2.86 | +1.05 | **yes** |
| C2 | `decl 0.11` | +1.81 | +1.18 | −0.63 | no |
| C5 | `decl 0.11` | +1.81 | +2.06 | +0.25 | **yes** |
| **C1 (fitted)** | `leak 1.5` | +1.68 | **+2.06** | +0.39 | **yes** |
| C5 | `leak 1.5` | +1.68 | +1.95 | +0.28 | **yes** |

**Final floors: C1 1.68 pts, C5 1.68 pts, C2 2.31 pts, C3 2.45 pts (coarse rungs only). No class
detects +0.86.** The instrument's floor sits in **(0.86, 1.68]**.

Two closing observations.

* **The `leak 1.5` asymmetry dissolves.** §1.12's summary showed C5 detecting the readability rung
  and C1 not — but C1 had simply not been run there. Fitted C1 detects it with excess +2.06 against
  C5's +1.95. Combined with W2 (§1.14: the rung adds ≤0.07 bits of inversion-accessible signal), the
  conclusion is airtight: `leak 1.5` is a ~1.7-point *strength* handicap that every competent class
  recovers, and nothing in this family is a readability handicap a white-box responder can uniquely
  see.
* **Below ~1.7 points the floor is evaluation-power-limited, not search-limited.** At `decl 0.08`
  the pooled excesses are positive for C1 and C5 (+0.69, +0.98) — the responders are finding
  something — but the excess estimator's quadrature CI at 12,000 games per cell is ±≈1.2 points and
  cannot exclude zero. The floor therefore scales roughly as (evaluation games)^−1/2 from here:
  phase 2 can buy a ~0.9-point floor with 4× the evaluation games per cell, without touching the
  responders. That is the practical trade the exit criterion's verdict (§1.17) turns on.

### 1.17 The clean throughput table, and the exit-criterion verdict

`research/v07/results/T1b-throughput-400.jsonl` — the search block re-timed at 400 deals against
`v06` on an otherwise quiet machine, because the 50-deal timing disagreed with the (contended)
6,000-deal strength runs by up to 39× on one row: per-deal search cost is heavy-tailed (a small
fraction of deals run long games in which `maxq` admits the search at almost every decision) and 50
deals does not sample the tail. Key rows, both bases:

| configuration | g/s (15 thr) | g/s (1 thr) | × search baseline | strength vs `v06` (T2, replicated) |
|---|---:|---:|---:|---|
| `v06` deployed | 382.2 | 32.8 | 242× | — |
| `s1=1,det=12,cand=4,kappa=2.5` (baseline search) | 1.58 | 0.161 | 1.0× | +2.08 at n=720 (±3.65) |
| `+ rbelief=indep,depth=24` | 22.8 | 2.01 | 14.5× | +0.08 (n=4,000, one bank) |
| `+ rbelief=indep,depth=24,maxq=26` | 78.5 | 6.62 | **49.7×** | **+1.57** (n=24,000) |
| `+ rbelief=indep,depth=12,maxq=26` | 119.0 | 10.36 | **75.3×** | **+2.19** (n=24,000) |
| F-mid `det=16,cand=6,kappa=2.0,maxq=26` | 7.62 | 0.71 | 4.8× | **+2.52** (n=6,000) |
| C3 responder (`roppo=` cheap opponent model) | 81.5 | 6.63 | 51.6× | — |

The two bases (all-threads and single-thread) now agree on every ratio to within 10%, which is what
"one basis" was for. The deployed policy is 242× the baseline search — between the corpus's "three
orders of magnitude" (mixed-basis) and the ledger's 300–420× (measured against `v05` on other
hardware); on this machine, against `v06`, it is 242× all-threads and 204× single-thread.

**The exit criterion, clause by clause.**

1. *"At least one responder recovers planted edges down to a stated size at or below the effect
   sizes v0.7 will claim."* **Met at ≥1.68 points; not met below it — and the sizes now on the
   table are above it.** The best floors are C1 = C5 = 1.68 points, replicated on two banks with the
   excess-over-control criterion; nothing detects +0.86. The live v0.7 candidates this phase
   surfaced carry effects of +1.5 to +2.5 points (F-mid +2.52, truncated fast search +2.19, the
   measured policy prior +1.52), all at or above the floor. What the instrument cannot certify is a
   sub-point claim — including a claim the size of v0.6's own +0.89 margin — and below ~1.7 the
   limitation is evaluation power, not search power (§1.16): the floor buys down as
   (evaluation games)^−1/2, to ≈0.9 points at 4× the games. Phase 2 must size its claims or its
   banks accordingly.
2. *"The search configuration is measurable at 10× or more of v0.6's games/s."* **Met, 5–7× over.**
   Two configurations hold replicated strength over the deployed policy at 50× and 75× the baseline
   search's throughput; the strongest configuration ever measured (F-mid, +2.52) is measurable at
   4.8× and its own cell above took 27 minutes rather than the v0.6 study's projected hours.
3. *"The 98/√N power arithmetic is wired into harness output."* **Met.** Emitted with every cell in
   `match`, `v7through`, `v7decide`, and the floor battery, in text and JSON, with the deal-level
   floor and the mirror-cell caveat.

**Verdict: the instrument is fit for phase 2, with a stated domain of validity — claims of
1.7 points and up at current bank sizes, scaling down with evaluation games.** The battery does not
proceed on a weak instrument; it proceeds on a calibrated one whose floor is measured, stated, and
purchasable.

---

---

---

---

---

## Phase 2 — Open-ended adversary generation

Started from `f4581da` ("web play: networked lobby, seat credentials, and public tunnels"), working
tree clean at the start of the session. Inputs read: `docs/v07/THREAT-MODEL.md`,
`docs/v07/INSTRUMENT.md`, `docs/v07/RESEARCH-LOG.md` (phase 1), `docs/v07/SUBOPTIMALITY-LEDGER.md`,
`docs/v07/PHASE-PROMPTS.md`, and `engine/src/`.

**Machine.** Apple M5 Pro, 15 logical cores, `clang++ -O3 -march=native`, macOS 25.5.0 — the same
machine phase 1 used, so its throughput table transfers. `v06` mirror measures 364.2 games/s here
against phase 1's 364.7. Frontier throughputs re-confirmed at the start of the session:

| frontier point | spec | games/s (14 threads) |
|---|---|---:|
| **F-fast** | `v06` | 364.2 |
| **F-cheap** | `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` | 96.3 |
| **F-mid** | `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26` | 6.44 |
| **F-search** | `v06:s1=1,det=12,cand=4,kappa=2.5` | 1.71 |

Those four numbers set the whole shape of the phase's budget. A fitted search against F-fast costs
minutes; the same search against F-mid costs hours and against F-search is not affordable at all. So
the fitted searches are run against F-fast and F-cheap, and their transfer to F-mid and F-search is
**measured rather than assumed** — which turns out to be the more informative experiment anyway
(§2.9).
### 2.0 What phase 2 had to build, and why

Phase 1 delivered four responder classes and a calibrated floor. It did not deliver the ability to
run *different* searches: every exploiter search in the corpus, phase 1's included, is a
cross-entropy fit of a linear vector maximising win rate from one seed against one target. The phase
brief is explicit that fifteen runs of that are one run. Six axes of variation were needed and five
of them did not exist.

| # | Built | Where | Why |
|---|---|---|---|
| **P1** | **Mechanism objectives** — the CEM can climb a per-decision failure mode of the *target* instead of a scoreboard: `declerr`, `forced`, `asksupp`, `declsupp`, `setdiff`, `limit`, `events` | `engine/src/tuner.hpp` (`TuneKpi`, `kpiValue`), `--kpi=` | Every one of these reads a `MatchStats` field the arena already accumulates for the B arm, so the objective axis cost no new plumbing and was simply never used |
| **P2** | **Target-arm decision capture** — `--capture=a\|b\|both` | `engine/src/arena.hpp` (`MatchConfig::captureArm`), `fish7 v7decide` | "Characterise what v0.6 does wrong against this exploiter" is not answerable while the channel can only record the arm under study |
| **P3** | **Declaration-urgency instrumentation** — `urgent`, `pressure`, and a four-bit `urgWhy` recording *which* clause of the urgency predicate fired | `engine/src/game.hpp` (`DecisionInfo`, `DecisionRecord`), `engine/src/v05.hpp`, `engine/src/v07_probe.hpp` | §2.2: the single most attackable structure found in the target |
| **P4** | **Information-denial and tally-inflation coordinates** — `NR7` 12 → 18 on the C2 responder | `engine/src/v07_responder.hpp` | §2.3: the two properties of the target's belief that no feature in the lineage prices |
| **P5** | **`--partners`/`--correlated` in the fitter, `--partnersb` in the arena** | `engine/src/tuner.hpp`, `engine/src/arena.hpp`, `engine/src/main.cpp` | The A2 regime could be measured but not *fitted*, and the one-seat column could not be fitted at all. `--partnersb` is the complement of the pre-existing `--partners`: a mixed **B**-arm team, which a one-seat deviation column needs when the adversary occupies the A arm. (`--partners` itself dates from v0.6's E5 battery, so ledger entry L7's "cheapest decisive experiment" was expressible all along and had simply never been run.) |
| **P6** | **The seal, enforced in `runMatch`** and `fish7 bankdigest` | `engine/src/arena.hpp`, `engine/src/main.cpp`, `engine/src/v07_seeds.hpp` | §2.11 |

Every one of these is behaviour-neutral on the incumbent, which is checked rather than asserted:
`v06:legacy=1` ≡ `v05`, `v07` (all responder coordinates zero) ≡ `v06`, and `v07i:inv=0` ≡ `v06`
are bit-for-bit identical before and after, on the same 60-game pathology digest phase 1 used
(`02772c5891dd3281b73f8d1881d1949d`), and phase 1's own D1 and W1 cells reproduce to the last digit
(`askHitRate` 0.54032, `ownLockedAskRate` 0.10644, `declAllocErrorShare` 0.88095; 1.9454 bits/ask).
### 2.1 Defects found in phase 1's own instrument

**D-1 — five ask decisions in a hundred were recorded with the previous decision's diagnostics, and
the bias was in the worst possible direction.** `V06Agent::chooseAsk` has five early returns
(`engine/src/v06.hpp`: `n <= 0`, `n == 1`, the `tieOnly` short-circuit, the deliberate-miss return,
and the `minGap` short-circuit) and none of them wrote `lastDec`. `Game::run` reads
`Agent::lastDecision()` at *every* ask, so each of those decisions entered the per-decision channel
carrying another decision's `nCand`, `nTie`, `margin` and `gateBound`. The `gateBound` bit is the one
that matters: it is the quantity ledger entry **L10** is measured on, and the `n == 1` path is
reached exactly when the live-ask gate has pruned the candidate set down to a single survivor — the
state in which the gate is *most* likely to have removed the ungated argmax. The decisions whose gate
bit was stale were disproportionately the decisions where the gate bound.

Fixed at all five sites. **Measured consequence: almost none, at v0.6's weights.** On phase 1's own
D1 cell (`v06` mirror, bank 7011001, 400 deals × 2, 34,235 ask decisions) `gateBindRate` is
**0.00938 before and 0.00938 after**; `tieShare` moves 0.60371 → 0.60455, because 49 decisions
(0.14%) now correctly report a single candidate and leave the contested denominator. No phase-1
number moves outside its interval. The fix is nevertheless load-bearing for phase 2, because the
adversaries in §2.6 are *designed* to raise gate pressure and would have been measured on a stale
bit.

**D-2 — `DealDP` never received the repair `BlockDP` was given, and the defect is latent rather than
live.** Threat-model E-2 records that `BlockDP` parks every instance's tables in a shared per-thread
pool and that v0.6 repaired it with a generation stamp (`engine/src/blockdp.hpp:97-118`). `DealDP`
has the identical construction — `static Buffers& buffers() { static thread_local Buffers b; }` at
`engine/src/belief.hpp:301`, with `F`, `B`, `scr`, `nz`, `nf` and `sumOf` parked into it at `:322-326`
— and **no stamp**. It is the sampler the v0.6 search's determinization and the phase-1 inverter both
use.

Whether that matters is a question with a decisive one-line answer, and it was run: a test build with
`DealDP::buffers()` made a per-instance member rather than a thread_local produces **bit-identical**
60-game pathology digests to the shipped build on `v06`, `v05`, `v07i:idet=48`, the truncated search
configuration and `v06:belief=block`. Every `DealDP` use in the current engine is build-then-read
with no interleaving, so the aliasing is unreachable. It is filed as a **latent** harness defect with
its reachability test attached: anything phase 3 builds that holds a `DealDP` result across a nested
build — a search inside an inversion, or a leaf evaluator that samples — makes it live, and the
repair is to port the `BlockDP` stamp.

**D-3 — the measurement flag is visible to the thing it measures.** `decisionCapture()`
(`engine/src/game.hpp:29`) is a free function returning a reference to a `static thread_local bool`,
in the same namespace as every policy. Any agent can read it and branch on it. No policy in the
corpus does, and phase 2's adversaries do not — but this is a *measurement-integrity* defect rather
than a curiosity: from phase 3 onward, candidate architectures are developed by the same programme
that measures them, and a per-decision channel that a policy can detect is not a channel that can
adjudicate between candidates. The repair is to move the flag out of the shared namespace and pass it
through `DecisionSink`. Recorded, not repaired here, because repairing it touches every capture site
and phase 2 changes no policy.
### 2.2 The declaration-urgency channel: a real structure, and a measured ceiling that kills it

The largest single structure phase 2 found in the target is in the declaration rule, and it is worth
setting out in full because the arithmetic that makes it look enormous is the arithmetic that turns
out to be wrong.

`V05Agent::declareNow` (`engine/src/v05.hpp`) is:

```
if (press >= 2) return true;
if (press >= 1 && v.pAlloc >= 0.5) return true;
if (cfg.useValue && cfg.valueDeclare) {
  if (urgent) return v.pAlloc >= cfg.declThreshold || (locked && v.pAlloc >= 0.5);
  return declareByValue(pub, v);
}
```

v0.6 has `useValue` and `valueDeclare` both true, so the shipped rule is the middle branch:
**urgency replaces an expected-value comparison with a threshold, and on a half-suit the team
provably owns (`locked`, `pTeam > 0.9995`) that threshold is a coin flip on the allocation.** That is
precisely the error class ledger entry **L1** sizes — the team held all six cards and named the wrong
teammate, 88.1% [81.0, 94.4] of v0.6's remaining misdeclarations (INSTRUMENT.md §1.10).

`urgent` is a disjunction of four clauses, and three of the four are public:

```
urgent = unresolvedCount <= patiencePool(5)
      || oppCards        <= oppCardFloor(2.99879)     <- the OPPOSING team's own total hand count
      || pub.nEvents     >= forceDeclareEvents(220)   <- the length of the game
      || bestAskProbability(pub) < askFloor(0.26573)
```

The thresholds are frozen constants of a published policy, so a white-box adversary knows exactly
where they are; and the second clause is a quantity the adversary controls directly, because
`oppCards` from the target's point of view is the number of cards in the *adversary's* three hands.

**The per-decision channel says the structure is large.** `fish7 v7decide --capture=b`, v0.6 mirror,
bank 7050001, 3,000 deals x 2 = 6,000 games, 26,962 voluntary declarations:

| metric | rate | 95% CI (deal-clustered) | n |
|---|---:|---|---:|
| `declUrgentShare` | 0.50193 | [0.4963, 0.5076] | 26,962 |
| `declAccUrgent` | 0.96837 | [0.9654, 0.9712] | 13,533 |
| `declAccCalm` | 0.98987 | [0.9882, 0.9916] | 13,429 |
| `urgWhyPatience` | 0.17421 | [0.1720, 0.1765] | 26,962 |
| `urgWhyOppCards` | 0.14628 | [0.1442, 0.1483] | 26,962 |
| `urgWhyEvents` | 0.00000 | [0.0000, 0.0000] | 26,962 |
| `urgWhyAskFloor` | 0.46925 | [0.4636, 0.4750] | 26,962 |

(`research/v07/results/P5-mech.jsonl`, 3,000 deals x 2. An earlier 150-deal smoke run put the
urgent/calm gap at 3.36 points; at two hundred times the sample it is 2.15, which is the ordinary
lesson about quoting a difference of two rates from a small sample.)

Half of v0.6's declarations are taken under urgency, and those are **2.15 points less accurate**.
At the ledger's own conversion — one point of declaration accuracy ≈ 1.2 points of win rate — moving
the remaining half of the declarations from calm to urgent reads as ≈1.3 points of win rate. That is
the arithmetic that made this the phase's leading hypothesis.

**The ceiling battery says the arithmetic is wrong, and it says so decisively.** Rather than fitting
an adversary to raise the urgent share and then arguing about whether it succeeded, the battery hands
the adversary omnipotence over the predicate: the *target* is modified so that a clause is
permanently true, and the unmodified incumbent plays it. That is the same construction phase 1 used
for `dTrue`, and the resulting edge is the ceiling on what any adversary attacking the mechanism
could ever collect.

| target | what it does | `v06` edge | 95% CI | target decl. acc. |
|---|---|---:|---|---:|
| `v06` | control | +0.00 | mirror | 0.9777 |
| `v06:pool=45` | clause 1 permanently true | +0.38 | [−0.06, +0.82] | 0.9768 |
| `v06:oppfloor=54` | clause 2 permanently true | +0.38 | [−0.06, +0.82] | 0.9768 |
| `v06:askfloor=1.1` | clause 4 permanently true | +0.38 | [−0.06, +0.82] | 0.9768 |

n = 12,000 games a cell on bank 7030002, 98/√N = ±0.89.

**A permanently urgent v0.6 loses 0.38 points, and its declaration accuracy falls by nine
hundredths of a point — not by 3.36.** The three cells agree to the last digit, which is the internal
consistency check that the instrumentation is right: all three set the same boolean, so the target's
behaviour is identical and only the route differs.

So the per-decision gap is **confounded**, and the confound is obvious once it is stated: urgency
fires in hard positions. Forcing it in easy ones costs almost nothing, because in an easy position
`pAlloc` is already above `declThreshold` and the branch that fires would have declared anyway. The
3.36-point gap measures the difficulty of the positions in which urgency fires, not the damage
urgency does.

**This is the single most important negative of phase 2**, and it is worth stating why the ceiling
battery earns its place: without it, an adversary search that raised the urgent share and gained half
a point would have been read as a confirmed mechanism at its predicted size, and the whole of phase 3
lead (b) would have been pointed at a channel worth 0.38 points — below every detection floor the
instrument has.

**The C6 class, built to attack it, confirms the ceiling from the other side.** `engine/src/v07_adapt.hpp`
is the corpus's first scripted-adaptive adversary: it carries an online model of whether the target is
currently holding a half-suit it owns outright but cannot allocate, estimated every decision from the
adversary's own posterior, and times its own declarations against that estimate. Both polarities were
run — hold your declarations while the target is safe, and accelerate them when it is vulnerable —
and both lose:

| arm | edge vs `v06` | 95% CI | target's decl. acc. |
|---|---:|---|---:|
| `v07c:mode=1,holdmax=30` (hold) | −2.06 | [−4.13, +0.06] | 0.9799 |
| `v07c:mode=1,holdmax=60` (hold) | −3.38 | [−5.56, −1.19] | 0.9788 |
| `v07c:mode=2` (accelerate) | −2.31 | [−3.44, −1.19] | 0.9781 |
| `v07c:mode=3` (both) | −5.31 | [−7.56, −3.06] | 0.9780 |

n = 1,600 games a cell, bank 7030004. **These are an uncommitted smoke measurement**: the C6 arms sit
in the deception battery whose artifact was still outstanding when this section was written, so they
are recorded here as what they are.  The adversary's own declaration accuracy falls to 0.9591 under `mode=2` and to
0.9552 under `mode=3`, against 0.9788 for the unmodified incumbent. The target's declaration accuracy does not move at all; the
adversary's own falls by two to three points. **The structural reason is worth recording, because it
generalises: the only lever an adversary has on `oppCards` is its own hand count, the only way to
lower its own hand count is to declare, and declaring earlier is exactly the `decl` handicap family
phase 1 measured at +0.62 to +2.45 points of cost to the holder. The adversary pays the cost it is
trying to impose.** `v07c:mode=0` is `v06` bit for bit, which is the class's identity control.
### 2.3 The pressure cliff: fifteen points behind an unreachable counter

The ceiling battery separated two mechanisms that share one threshold and are not the same thing.
`pub.nEvents >= cfg.forceDeclareEvents` appears both as the third clause of `urgent` **and** as the
trigger of `pressure()`, and they do very different damage. At `press >= 1`, `evaluateSet` drops
`teamFloor` from `minTeamProb = 0.849` to 0.25, bypasses the capacity gate, and `declareNow` cashes
any half-suit at `pAlloc >= 0.5`:

| target | `v06` edge | 95% CI | target decl. acc. | target lock hold |
|---|---:|---|---:|---:|
| `v06:force=1` | **+15.18** | [+14.41, +15.94] | **0.8115** | 2.62 |

**v0.6 carries a fifteen-point cliff in its own declaration rule, and it sits behind an event
counter set to 220.** The question that decides whether that is a vulnerability or a curiosity is how
long a game an adversary can produce. Measured across every arm phase 2 ran, including the ones built
specifically to stall: the v0.6 mirror runs at 95.1 events a game, the longest any adversary produced
is 105.5, and the pathology probe's maximum over 400 games of the strongest denial adversary is 130.
The cliff needs 220. Nothing in the spec grammar gets within a factor of 1.7 of it, and the reason is
structural rather than incidental: the game ends when the half-suits are gone, and every ask either
transfers a card or publishes a certificate, so the transcript cannot be padded without also
resolving the position that ends it.

Filed as a **standing hazard rather than an exploit**: it is not reachable by any adversary phase 2
could build, and it is the single largest number in the phase. Any v0.7 candidate that lengthens
games — and a search that reasons about a longer horizon is a candidate that might — moves toward it.
Phase 3 should report `eventsPerGame` for every candidate against the 220 rung, and phase 4's commit
gate should refuse a configuration that crosses it.
### 2.4 The one arm that clears the bar, and the story it does not support

Phase 2's strongest adversary was not found by any of the thirty-one fitted searches. It was found by
the search of a different kind — the unfitted coordinate sweep — and it is a single hand-set
coefficient.

**How it was found, and the provenance caveat that goes with it.** The extended responder class was
widened from twelve coordinates to eighteen at the start of this session, on the strength of a reading
of `Knowledge::priorWeight` and of `enumerateAsks`: no feature anywhere in the v0.4–v0.6 lineage
prices the negative certificate a *miss* publishes about a target seat, and that certificate is the
only primitive that can separate two seats of one team, because a team can never manufacture one about
itself. Four coordinates were added to express **information denial**. A reconnaissance pass over the
engine then measured one of them, `oppCertDonate`, at +1.2 to +3.6 points on **unregistered scratch
banks** with a source tree being edited underneath it. **No number from that pass is used anywhere in
this document.** Everything below was re-measured from scratch on registered evaluation banks, with
the commit gate run first.

**The dose response, on two registered banks at 24,000 games a dose.**

| dose | edge vs `v06` | per bank | adversary decl. acc. | adversary lock hold | target lock hold | target decl./game |
|---|---:|---|---:|---:|---:|---:|
| control (`v06`) | +0.00 | mirror | 0.9788 | 4.60 | 4.60 | 4.49 |
| `r12=5` | +0.65 | +0.19 / +1.11 | 0.9793 | 4.61 | 4.61 | 4.49 |
| `r12=10` | +0.62 | +0.80 / +0.45 | 0.9746 | 4.27 | 4.86 | 4.43 |
| `r12=15` | +2.23 | +2.32 / +2.14 | 0.9651 | 3.48 | 5.26 | 4.33 |
| `r12=20` | +2.53 | +2.69 / +2.38 | 0.9647 | 3.37 | 5.30 | 4.30 |
| **`r12=25`** | **+2.71** | +2.85 / +2.58 | 0.9643 | 3.41 | 5.29 | 4.30 |
| `r12=30` | +1.65 | (one bank) | 0.9645 | 3.44 | 5.33 | 4.33 |

Monotone from 5 to 25, peaking at 25, falling away above it, with the two banks agreeing to within
0.3 points at every dose. The KPI profile moves together with the score and in the same direction at
every dose, which is what separates a mechanism from a lucky vector.

**The sign is the opposite of the one the coordinate was designed for, and that matters.** The feature
is (1 − p) · oppFrac · (uS/6) and the design intent was a *negative* coefficient: refuse to publish
the certificate. Measured, the negative branch is worth −0.77 at −25 and +0.70 at −10; the whole
effect lives on the positive branch. So whatever this is, **it is not information denial**, and the
document must not call it that.

**What it is, as far as the evidence supports.** The KPI signature is unambiguous and it is not a
strength signature: the adversary's own declaration accuracy falls from 0.9788 to 0.9643 and its own
ask accuracy falls from 0.5426 to 0.5336, while it claims *more* half-suits (4.67 a game against 4.49)
and cashes its own locked half-suits 1.2 events sooner (lock hold 4.60 → 3.41). The target's mirror
image: it claims fewer (4.30), holds its locks 0.7 events longer (5.29), and its ask accuracy falls
from 0.5426 to 0.5199. **The adversary plays a worse game by two of its own KPIs and wins by nearly
three points**, which is the discriminator the phase brief asks for.

**Three controls, each at 4,000 games on bank 7030004, rule out the obvious reductions.**

| control | edge | what it rules out |
|---|---:|---|
| `v07:r2=25` | −1.30 [−2.88, +0.25] | "prefer half-suits the opposing team dominates" — the plain opponent-mass term, without the (1 − p) and ambiguity factors |
| `v07:r2=10` | −0.77 [−2.22, +0.68] | the same at a lower dose |
| `v06:w0=8` | +0.40 [−1.02, +1.82] | "weight hit probability less" |
| `v06:w0=6` | −0.62 [−2.10, +0.82] | the same, further |
| `v06:w0=4` | −1.97 [−3.48, −0.50] | the same, further still |
| `v07:r12=25,r2=-25` | −11.50 [−13.00, −10.00] | the coordinate with the plain opponent-mass term cancelled |

Neither factor reproduces the effect alone and neither does a blunt reduction in the leading ask
weight. The product is load-bearing.

**What phase 2 will not claim.** It is characterised, not attributed. Which of the two lock-hold
movements is cause and which is consequence is not settled by these measurements, and the feature is a
product of three quantities whose separate contributions were not resolved. That is phase 3/4 work,
and it is precisely the work the v0.6 record says it could not afford. What is settled: real,
replicated on two banks at every dose, dose-responsive with a peak, commit-gate clean (0.022% dead
asks, longest dead run 1, no games at the action limit), and outside every class the corpus had.
### 2.5 Harness findings

The phase brief: "An exploiter that works only through a harness defect is a finding about the
harness — file it as such." Phase 2 found five, refuted two the threat model or its own
reconnaissance had proposed, and closed one question the corpus had left open. Every one of them was
checked in code and, where a check was possible, tested rather than argued.

**H-1 — the deal-seed inversion channel (threat-model E-1) is still open.** `Game::setup` hands seat
*p* the value `mixSeed(s, p + 77)` where the same `s` generates the deal, and `mixSeed` is a
bijection, so any policy can recover the deal in closed form and read all six hands. Phase 0 verified
the invertibility numerically. Phase 2 did **not** build the clairvoyant agent that would demonstrate
it: doing so would produce a number (an adversary at or near 100%) that is not an exploitability
number under T3 and could only ever be quoted as an upper anchor, and the channel's existence is
already established by construction. Threat-model **T10** — hand each seat a stream drawn
independently of the deal — remains unimplemented and remains the single most important harness
repair outstanding. No policy in the corpus reads it, which is what makes the omission survivable
rather than disqualifying.

**H-2 — `Event::confidence` is not merely broadcast, it is in the public history.** The threat model
records that `Game::emit` hands the full event, confidence included, to every agent's `observe`.
Phase 2 adds that `emit` also pushes the event onto `g.pub.history`, so the declarer's private
`pAlloc` is a *persistent* field of the public state rather than a transient argument, and any policy
that walks the history can read every declaration confidence any seat has ever stated. The field is
documented as "diagnostic only" and no policy in the corpus reads it — verified again here by grep
across `v04.hpp`, `v05.hpp`, `v06.hpp`, `v07_responder.hpp`, `v07_invert.hpp`, `v07_adapt.hpp`,
`belief.hpp`, `baselines.hpp` and `human.hpp`. The repair is one line (strip it before `emit` and
carry it on the driver-side sink next to `DecisionSink`); the test is a build with the field poisoned
to NaN, under which any battery must be bit-identical.

**H-3 — `DealDP` never received the repair `BlockDP` was given, and the defect is latent.** See §2.1
D-2. `BlockDP` carries a generation stamp; `DealDP`, which is the sampler the v0.6 search's
determinization and the phase-1 inverter both use, carries none. Tested: a build with the pool made
per-instance is **bit-identical** on `v06`, `v05`, `v07i:idet=48`, the truncated search configuration
and `v06:belief=block`. Every use is build-then-read with no interleaving, so the aliasing is
unreachable today. It becomes live the moment anything holds a `DealDP` result across a nested build.

**H-4 — the measurement flag is visible to the thing it measures.** See §2.1 D-3. `decisionCapture()`
is a `static thread_local bool` behind a free function in the same namespace as every policy. From
phase 3 onward, candidates are developed by the same programme that measures them, and a per-decision
channel a policy can detect is not a channel that can adjudicate between candidates.

**H-5 — two residue rules disagree with each other, and neither is a rule of the game; both are
dead code.** `forcedEndgame` resolves an unclaimed half-suit by physical majority with **every 3–3
tie going to team 0 unconditionally**; `adjudicateRemaining`, thirty lines later, resolves the same
physical situation by physical majority with ties to the holder of the lowest card. The first is an
unconditional asymmetry between the two teams. Both were instrumented with a counter and neither
fired in 1,600 v0.6 mirror games or in 1,600 games against `v06:declare=0`, a configuration that
produces 4.24 forced declarations a game — because the willingness ladder's last rung is `-1.0`,
which forces `bestGuess`, so no half-suit ever survives to the residue. Filed as dead code with an
asymmetry in it, to be removed or unified rather than left for a future policy to reach.

**Refuted: the action-cap adjudication farm.** `game.hpp` breaks the game at `rules.maxAsks` (400)
and adjudicates the residue by physical majority with no declaration risk, which deletes the target's
largest edge from the payoff. It is unreachable. Every stalling configuration in the spec grammar was
tried, including `v06:dead=1,deadmargin=-1000,deadbudget=999,declare=0,patient=1,pool=45`, and every
one returns a **limit-hit rate of exactly 0.0000**, and the longest single game any adversary produced
against the incumbent runs **149 events** against a 400-**ask** cap. The mechanism exists and cannot be reached; it is retained as a
harness finding and as a reporting obligation — `limitHitRate` is printed with every phase-2 cell and
is zero in all of them.

**Not tested: arm asymmetry, and the test that looked like one is an identity.** Every exploitability
number in the corpus is measured with the adversary as the A arm. Measured on bank 7051001 at 3,000
games, `v06:vmargin=-0.02` as A scores 49.77% and `v06` as A against it scores 50.23%, summing to
exactly 100.00; with one-seat partners on either side, exactly 100.00 again. **That is a harness
identity, not evidence.** At `--rotations=2` the arena plays both orientations of every deal, so
exchanging the arm labels replays the same game multiset — the two cells report identical
`eventsPerGame` to four decimals, which is the giveaway. The identity holding is worth recording,
because it would break if the orientation loop or the seat construction were asymmetric; but a real
test needs a single fixed orientation, which the arena cannot express. Filed as **not tested**, with
the construction that would test it named.

**Refuted: `BlockDP` aliasing (threat-model E-2).** The threat model lists it as open. It is not: the
generation stamp, `current()` and `ensureCurrent()` are present and are checked at every query site.
The 175 raw shared-pool field mismatches the v0.6 record reports are reads of the raw pool, which the
query-level guard makes unobservable. **THREAT-MODEL.md §6.3 E-2 should be corrected in phase 6.**
### 2.6 A ledger entry closed, and a number in it corrected

**L13 — forced-endgame allocation — is closed, and for a better reason than incidence.** The ledger
demotes it to a priority of 0.014 on incidence grounds (0.0031 forced endgames a game) but keeps it
open on one condition: "incidence is an adversarial variable […] Phase 2 should measure
forced-endgame incidence under adversarial pressure before this entry is closed for good, and if an
adversary can raise it by an order of magnitude the entry returns at ~0.15 points."

Measured, and the answer is not the one the entry anticipated. `v06:declare=0` — a team that never
claims a half-suit voluntarily — raises the **game's** forced-endgame incidence to **4.20 declarations
a game** against a mirror baseline of 0.0056. But they are made by the `declare=0` team *itself*:
`forcedPerGameA` = 4.20167 with `forcedAccB` = 0.000000 over 12,000 games. A team that never claims
keeps its cards while the other team empties, so it is the *other* team that goes cardless and the
non-declaring team that must then declare everything. **The target's own incidence stays at exactly
zero**, and the construction that would raise it — the adversary going cardless itself — measures −17
points.

The second half of the measurement is the one that closes the entry:

> **v0.6's forced path, exercised 1,400x more often than normal, resolves correctly 99.3–99.5% of the
> time — not 28.6%.**

23 wrong in 3,396 over 800 games (99.32%), and 0.994863 over 12,000 games in the paired cell. The
corpus's 0.286 rests on two to eight observations per battery at an incidence of 0.003 a game; it is a
small-sample artifact, and the v0.5 study's ~40.6% "feasible ceiling" is a ceiling on a quantity
measured in a regime the forced endgame is almost never in. The adversary pays 7.5 to 7.8 points for
the construction.

**L13 is closed on both counts: the target's incidence is not raisable cheaply, and the mechanism is
not broken.** The
brief's lead (b), which names the forced endgame, should be read as naming the *voluntary*
declaration channel only — which is what the ledger already concluded on different grounds.
### 2.7 The detection floor at 4× the evaluation power, and a phase-1 claim refuted

Phase 1 measured its floors at 12,000 evaluation games a bank and then made a claim about what would
happen with more: *"Below ~1.7 points the floor is evaluation-power-limited, not search-limited […]
the floor therefore scales roughly as (evaluation games)^−1/2 from here: phase 2 can buy a ~0.9-point
floor with 4× the evaluation games per cell, without touching the responders."* The evidence was that
at the undetected `decl 0.08` rung the pooled excesses were positive (+0.69 C1, +0.98 C5) while the
estimator's ±1.2-point width could not exclude zero.

Phase 2 cannot rest a null on an extrapolated scaling law, so it was tested: the same phase-1
responders, the same rungs, the same two evaluation banks, at **48,000 games a cell instead of
12,000**. The banks are extended rather than replaced — a deal's seed is a function of its index
alone, so the first 6,000 deals of each are bit-identical to phase 1's — which makes the comparison
nested.

| class | rung | dTrue (re-measured at 24,000 games) | dFound pooled, 96,000 games | excess over control | detected |
|---|---|---:|---|---|:--:|
| C1 | `none` (control) | 0.00, mirror | +0.79 [+0.48, +1.10] | — | — |
| C1 | `decl,hstr=0.05` | +0.67 [+0.28, +1.06] | +1.04 [+0.73, +1.34] | +0.25 [−0.19, +0.68] | no |
| C1 | `decl,hstr=0.08` | +0.88 [+0.47, +1.28] | +0.78 [+0.47, +1.08] | **−0.01 [−0.45, +0.42]** | no |
| C1 | `decl,hstr=0.11` | +2.13 [+1.61, +2.65] | +3.46 [+3.02, +3.90] | +2.67 [+2.13, +3.20] | **yes** |

A fifth rung, `leak,hstr=1.5`, is the one phase 1 quoted its 1.68-point floor from. At four times the
power its `dTrue` re-measures at **+1.53 [+0.96, +2.11]** and its excess over the control is
**+1.74 [+1.30, +2.18]** — detected. So the class floor moved from 1.68 to 1.53: four times the games
bought **0.15 points of floor**, against the 0.78 the scaling law predicts.

**The scaling law does not hold, and the reason is the interesting part.** At four times the power the
`decl 0.08` excess does not sharpen to a small positive number — it collapses to **zero**, ±0.44. The
+0.69 phase 1 saw at that rung was noise, and buying more games buys a sharper zero rather than a
resolvable effect. Below about two points on the declaration family the C1 responder is not
*unresolved*, it is **empty**: the search finds nothing there, and the binding constraint is search
power after all.

Two consequences, and both raise the bar rather than lower it.

1. **Phase 2's claims are read against a class floor of 1.53 points, and 2.13 on the declaration
   family — not the 0.9 phase 1 projected.** Every severity in ADVERSARIES.md carries those numbers.
2. **INSTRUMENT.md §6's clause-1 verdict needs the qualification.** Its statement that the floor is
   "purchasable" — "≈0.9 points at 4× the games" — is measured here and is not. The exit criterion
   itself is unaffected: the effects phase 1 put on the table (+1.5 to +2.5) still sit at or above
   the floor, and the floor is still stated rather than assumed. What changes is that phase 3 cannot
   buy resolution below ~2 points with games alone, and a sub-two-point claim needs a better
   responder or a per-decision estimator, which is exactly what ledger entry L5 says.

**The control rung is the other result, and it replicates phase 1's I5 at four times the power.** A
properly specified in-class responder reaches **+0.79 [+0.48, +1.10]** against the *unhandicapped*
incumbent over 96,000 games — against phase 1's +0.76 [+0.15, +1.37] over 24,000, and against the
v0.6 probe's published −1.64. v0.6's in-class exploitability is at least 0.79 points, and the
interval is now a third of its former width.
### 2.8 What was searched, and the honest accounting of it

The phase brief's constraint is that the searches must not share a bias. Six axes were varied and five
of them did not exist at the end of phase 1 (§2.0). The inventory is in ADVERSARIES.md §1; the
accounting that matters here is which axis produced what.

* **The unfitted coordinate sweep produced the phase's strongest arm** and, separately, its most useful
  negative — that every axis-aligned deviation of the incumbent's belief and policy prior loses, which
  is a stronger statement about the shipped configuration than any fit produced.
* **The ceiling batteries produced the two results that reshape phase 3** — the urgency channel's
  0.38-point ceiling and the 1.4-point gain from switching it off — and neither is an adversary.
  Measuring a mechanism's worth before fitting anything against it is the single highest-value
  procedure phase 2 adopted, and it is the phase-1 `dTrue` construction applied to mechanisms rather
  than to handicaps.
* **The mechanism objectives were built so that a search could climb a per-decision failure mode of the
  target rather than a scoreboard.** Their value is diagnostic as much as competitive: an adversary
  that doubles the target's misdeclaration rate and gains nothing in win rate is telling us the
  channel is not worth what the ledger's conversion arithmetic says it is.
* **The fitted searches are the continuity column.** They are what makes "nothing else clears the bar"
  a measured statement rather than an absence.
### 2.9 The fitted searches

Thirty-one independent cross-entropy searches, one registered fitting bank each, listed in full in
ADVERSARIES.md §1. The budgets are not uniform and the reason is stated rather than hidden: the three
control rows carry phase 1's standard 12 × 16 × 250 (96,000 games), one row carries phase 1's
budget-curve top rung at 20 × 20 × 350 (280,000 games), and the exploratory rows carry 8 × 12 × 150
(28,800 games). Phase 1's own budget curve is what licenses that — a 21,600-game fit reached +3.20 of
the +4.08 the curve plateaus at, so the marginal point per game past ~30,000 is small, and the phase
brief's constraint is on the *number of distinct search biases*, not on the depth of each. Every row
prints its budget into the artifact, and the two rows that carry the full and the extended budget are
there precisely so that "the exploratory rows were too small" is a checkable claim rather than an
excuse.

**What the axes bought.** The searches vary in six dimensions (ADVERSARIES.md §1) and the honest
summary of what each dimension produced is:

* **Objective.** The five mechanism objectives are new to this corpus, and their value here is
  diagnostic rather than competitive. A search that climbs the target's misdeclaration rate and gains
  nothing in win rate is a measurement of the channel's conversion, which is exactly what the ceiling
  batteries measured directly and more cheaply. That is worth saying plainly: **the ceiling
  construction dominated the objective axis.** Measuring what a mechanism is worth by handing the
  adversary omnipotence over it costs one cell; fitting an adversary to earn it costs a battery, and
  answers a strictly weaker question.
* **Starting basin.** Whether the CEM is trapped near the incumbent is a real question — every fit in
  the corpus, phase 1's included, starts at the incumbent's own vector because of the P-3a repair —
  and it is answered by running the same objective from the v0.5 defaults and from a wide sigma.
* **Seat count.** The one-seat column is mandatory under threat-model T2 and had never been reported.
* **Target.** Fitting against the frontier's search end, and against the two-member frontier panel
  under a `min` objective, is the only way to ask for an adversary that dominates the frontier rather
  than its cheap point.

**What they reached.** Eight of the thirty-one designed searches completed inside the session's
simulator budget, spanning both classes, both structural switches (`dead7`, and `corr` with the A2
device live), two objectives, two frontier targets, four budgets from 23,040 to 280,000 games, and one
registered fitting bank each. Three have been evaluated on the training banks so far:

| id | class | fitted against | objective | fitting games | edge vs `v06` | 95% CI |
|---|---|---|---|---:|---:|---|
| X01 | C1 | `Ffast` | `win` | 96,000 | **+0.98** | [+0.54, +1.42] |
| X13 | C2 (`dead7=1`) | `Ffast` | `win` | 28,800 | +0.67 | [+0.06, +1.28] |
| X05 | C1 | `Ffast` | `declerr` | 28,800 | **−8.42** | [−8.85, −7.99] |

**Not one clears its class floor**, and the best of them lands where phase 1's C1 landed (+0.76) and
where the floor battery's control rung landed (+0.79) at four times the power. Three independent fits,
three banks, three batteries, one answer: a correctly-specified in-class responder gets about a point
out of `v06` and no more.

**X05 is the row worth reading twice.** It was fitted to maximise the *target's* misdeclaration rate,
not its own win rate, and it scores −8.42 in games. That is the mechanism objective working as a
diagnostic rather than failing as a competitor: it says the ledger's conversion — drive the target's
declarations wrong and the wins follow — does not survive being pursued single-mindedly. Read with the
0.38-point ceiling on the same channel (§2.2), the two independent measurements agree that **the
declaration channel is not where an adversary's points are**, which is the opposite of where this
phase started.
### 2.10 The train / holdout split, and what "sealed" was made to mean

The phase brief: *"seal the evaluation material, physically, before anything can be tuned against it"*,
and *"split the adversary bank the same way (train half / sealed half)"*.

A bank in this corpus is a **seed plus a size** — the deals are generated from the deal index and
never stored — so sealing one cannot mean secrecy. It was made to mean three things instead, and
`research/v07/banks/README.md` states them in the directory itself.

**1. A commitment.** `fish7 bankdigest` (new) folds the six dealt hands and the dealer of every deal a
bank would produce into a 64-bit rolling hash. It plays no game and constructs no policy, so the
digest of a *sealed* bank can be computed now without learning anything about how any policy performs
on it. **That computation is the only phase-2 contact with the holdout material, and this sentence is
the record of it.** Phase 5 recomputes each digest and compares.

**2. A seal the binary enforces.** INSTRUMENT.md §7 states that the phase-5 banks "refuse to unseal
below `FISH_UNSEAL_PHASE=5`". At the end of phase 1 that was true only of `fish7 seeds --require`,
which is a check a battery has to remember to run; nothing stopped `fish7 match --seed=7090001`. The
check now lives in `runMatch`, so it covers every command in the binary:

```
$ ./fish7 match --a=v06 --b=v06 --games=2 --seed=7090001
fish: seed 7090001 is SEALED until phase 5 (set FISH_UNSEAL_PHASE to unseal)   [exit 5]
```

**3. A split made by a rule rather than by a choice.** The adversary bank is sorted by row id and
assigned alternately, even positions train and odd positions holdout. The rule is in
`engine/seal_banks_v07.py` and was written before any phase-2 result was known, so the split cannot
have been chosen to flatter either half. The train half is plaintext; the holdout half is base64 with
its plaintext SHA-256 in the file header, so it is not readable by eye or by a careless `grep` during
phases 3–4 and is exactly verifiable in phase 5.

**The banks.**

| half | seeds | role |
|---|---|---|
| train | 7030001, 7030002, 7030003, 7030004 | phase-2 adversary evaluation; phases 3–4 training |
| sealed | 7090001–7090005, 7091001, 7091002 | phase-5 holdout, the phase-5 fresh adversary search, its fitting bank, and the phase-5 negative controls |

Phase 2 evaluated every adversary on 7030001–7030003, so those banks are burnt by the time phase 3
starts. That is deliberate — it is what makes them the training half. The fitting banks
7040001–7040031, one per independent search, carry role `Fit` in the registry, which is what makes the
registry's R1 rule (no seed may be both a fitting and an evaluation bank) enforceable rather than
aspirational; the one R1 violation in the corpus remains v0.6's 515253, reported by `fish7 seeds` on
every run.

None of this is cryptography and the README says so. It is a **commitment**: after this is committed
the sealed material cannot be silently changed, and any phase that reads it has to do something
deliberate that shows up in the record.
### 2.11 The verdict

**Nothing phase 2 built exploits the v0.6 frontier beyond the detection floor**, and the brief asks
for that to be said plainly rather than hedged.

The best measured exploitability of the deployed policy is the in-class figure: **+0.79
[+0.48, +1.10]** over 96,000 games, a correctly-specified C1 responder against the unhandicapped
incumbent, replicating phase 1's I5 at four times the power and confirming that the v0.6 probe's
published 48.36% was a mis-specified-exploiter artifact. It sits **below** the class's re-measured
detection floor of 1.53 points.

Two arms beat `v06` by more than that, and both were measured and found to be **better policies rather
than exploits**:

* **The target's own test-time search**, at +1.89 on the phase-2 training banks. It is not an exploit
  by construction — it is the configuration the deployed policy ships switched off for cost — and there
  is no KPI that separates it from being stronger. Naming that explicitly is what stops it being
  smuggled into a ranking, and it corrects phase 1's reading of its own class ladder: most of C3's
  +1.86 was the search improving the policy, not the `roppo=` opponent model attacking it.
* **A single out-of-class coordinate**, at +2.56 [+2.12, +3.00] over 48,000 games. Its KPI signature
  looked like an exploit — the adversary's own declaration accuracy falls 1.5 points while it wins —
  and the cross-opponent profile at 8,000 games a cell appeared to confirm it. Re-run at 48,000 games a
  cell and re-expressed in the corpus's own linear unit, the gain is +2.52 / +2.26 / +1.83 / +1.40
  win-rate-equivalent points against `v06` / `v05` / `v04` / `detective`: a mild gradient monotone in
  the opponent's strength, with no discontinuity at the boundary of the FishBot family. The apparent
  specificity was win-rate compression at a 77.5% operating point.

**So the phase-2 deliverable is a taxonomy of closed directions and a list of defects in the
incumbent, not a list of exploits.** That reshapes phase 3 in the way the brief's gate anticipates:
the v0.7 case rests on beating the frontier, and phase 2's contribution to it is that several of the
things worth beating it with are already measured.
### 2.12 What phase 2 hands to phase 3, and what it takes off the table

**Taken off the table, with numbers.** Each of these is closed in the sense that matters — measured at
or below zero with an interval, not merely "not found":

| direction | why it is closed | number |
|---|---|---|
| declaration-timing / urgency induction as an **attack** | ceiling measured by making the target permanently urgent | +0.38 [−0.06, +0.82] |
| the action-cap adjudication farm | unreachable by any spec in the grammar | limit-hit rate exactly 0.0000 everywhere; longest game 105.5 events against a 400-ask cap |
| stalling to the pressure rung at `nEvents >= 220` | the same barrier, from the other side | 105.5 measured against 220 needed |
| forced-endgame induction | the target's forced path is not broken, and inducing it is expensive | forced accuracy 0.9943 under pressure; adversary pays 6.5 to 7.8 points |
| the deception family, swept | its best arm is a loss, and no threat to `v06` | best is `feint:tol=0.02` at −1.21 [−2.32, −0.11]; the published defaults are three to twenty-eight points worse — **see the note below** |
| axis-aligned belief and prior perturbation | the shipped configuration is a local optimum in its own neighbourhood | best of twelve is −0.84 [−1.67, −0.02] |
| buying resolution below ~2 points with more evaluation games | the floor does not scale as asserted | the +0.88 rung's excess is −0.01 [−0.45, +0.42] at 4× power |
| C6 declaration-timing manoeuvres | the adversary pays the cost it is trying to impose | both polarities lose, −2.1 to −5.3 |

**Closed on size, but not for the reason the corpus gives.** The deception family's `tol` and `k`
parameters have never been set by any committed artifact, and the sweep that sets them changes the
reasoning even though it does not change the verdict. `tol` — how much hit probability the archetype
will sacrifice per deviation — spans **6.75 points for `feint` alone** (−1.21 at 0.02, −1.74 at 0.05,
−4.21 at the default 0.10, −7.96 at 0.20), with the same monotone shape for `withholder` and `silent`.
The published value is three points off the family's best, and at its best `feint` reaches **−1.21**,
which is *parity with* the unrestricted `v04` it restricts rather than below it. So the tidy claim in
the row above — every restriction costs more than the corruption it buys — is not right as stated;
what is right is that the family's best is still a loss, and still no threat to `v06`.

`k` is a **dead knob**: `feint:k=1`, `k=3`, `k=6` and `k=12` are bit-identical to each other and to
the default in every column of the artifact. It belongs on ledger entry C12's list alongside `patient`
and `lockthr`, and any future ablation on it measures nothing.

**Handed over, with numbers.**

1. **Switch off the urgency escalation.** +1.23 to +1.62 over `v06`, replicated on three banks, mirror
   misdeclaration rate 2.94% → 1.22%, commit gate clean. It is a configuration change, not an
   architecture, and it is the largest well-measured improvement over the incumbent anyone in this
   programme has produced. M2 rides along inside it.
2. **The one-switch defects stack, and the commit gate decides which stack.** `rtie=1` on top of
   urgency-off is **+1.91 [+1.29, +2.54]**, replicated on two banks, mirror misdeclaration rate
   2.37% → 1.11%, gate-clean (longest dead run 1, zero action-limit games), and above the class
   detection floor of 1.53. Adding `m1=0` reaches +2.68 and **fails the gate**: 2.91% provably-dead
   asks, a 326-ask dead run, 0.33% of games killed by the action limit, and a game-length tail to 405
   events. That is ledger entry C14 happening again, and the gate caught it before the strength number
   was quoted, which is what the gate is for.
3. **Half-suit contestation.** Real, replicated, dose-responsive, out of class, and unattributed.
   Phase 3 should either attribute it or price it as an opponent.
4. **The pressure cliff is a hazard to design against, not an opportunity.** Print `eventsPerGame`
   against the 220 rung for every candidate; a search that reasons over a longer horizon moves toward
   it.
5. **Three harness repairs, in priority order**: T10's independent per-seat stream (threat-model E-1
   is still open); stripping `Event::confidence` before `emit`; and moving `decisionCapture()` out of
   the shared namespace before phase 3 begins developing candidates against the channel that measures
   them.
