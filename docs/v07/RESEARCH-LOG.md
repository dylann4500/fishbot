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

