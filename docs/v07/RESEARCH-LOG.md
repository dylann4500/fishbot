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

---

## Phase 3 — Candidate architectures

Started from `0c021a3` ("phase 2 v7: open-ended adversary generation"), working tree clean. Inputs
read: `docs/v07/THREAT-MODEL.md`, `docs/v07/INSTRUMENT.md`, `docs/v07/ADVERSARIES.md`,
`docs/v07/SUBOPTIMALITY-LEDGER.md`, `docs/v07/PHASE-PROMPTS.md`, and `engine/src/`. The deliverable
is `docs/v07/CANDIDATES.md`; this section is the working record, including the batteries that did
not finish and the two places one agent's inference outran its evidence.

**Machine.** Apple M5 Pro, 15 logical cores, `clang++ -O3 -march=native`, macOS 25.5.0 — the same
machine phases 1 and 2 used. **Every throughput figure produced during the parallel development
phase is unusable as a headline** and is labelled as such: six workstreams shared the machine and
load averages ran between 40 and 121 on 15 cores. Two repetitions of a *single* spec spread 18.86
against 20.87 games/s, and for another 32.42 against 21.39 — a within-spec spread larger than any
between-spec difference in the phase. Throughput claims in `CANDIDATES.md` are either from dedicated
back-to-back runs on a momentarily quiet machine, or they are stated as ratios measured within one
cell, and they say which.

**Structure.** Six workstreams, each in an isolated git worktree branched from the gate commit, each
required to (i) run the cheapest decisive probe first and stop if it killed the premise, (ii) pass an
identity control proving the new switch off is bit-identical to the reference binary, (iii) pass the
commit gate before quoting any strength number, and (iv) pass the mechanical side-channel gate. The
first attempt at the five candidates was killed 18–20 minutes in by a network outage; the worktrees
survived with uncommitted work and the second attempt resumed into them rather than retyping. That
is why every candidate's log begins with a commit of someone else's code.

### 3.0 K0 — the mechanical side-channel gate, and the calibration that makes it a test

THREAT-MODEL.md §6.4 specifies S1–S6 and states that none of them reads only existing artifacts.
Nothing had been built. `fish7 v7side` (`engine/src/v07_side.hpp`, committed `d1ec24f`) implements
S3, S4, S5 and S6 and is the first mechanical check of the homogeneity constraint in this corpus.

The design decision worth recording is **S6 by reconstruction rather than by IPC**. THREAT-MODEL
§6.4 proposes separate processes communicating a serialised event stream; that needs an event format
and an IPC path, neither of which exists. Instead every decision of every certified seat is rebuilt
from (own hand, public event stream, reset seed) alone, by a fresh agent, **on a freshly spawned
thread** (clean thread-locals) and **after the game** (time-shifted, so a process-global static is
read at the wrong moment), with the seat's own past decisions interleaved into the replay so that a
stochastic policy's private RNG advances exactly as the live run did. It covers all four decision
types of §6.2 **plus `bestGuess`** — no prior work in this corpus looked past asks.

**Three planted cheats, each failing exactly one test**, is what makes the gate a test rather than a
ritual. `v07x:cheat=seed` inverts its own reset seed (splitmix64 finaliser inverted step by step plus
a modular inverse of `0x9E3779B97F4A7C15`, 2,000 round trips verified before the gate will run),
re-deals, identifies the post-deal rotation from its own hand, and plays only guaranteed hits: S4's
ask hit rate 87.62% → 53.41%, S5's gap +68.79 against a +6.16 floor — and it **passes S6, which is
correct and is the point**, because a seed is something the seat legitimately receives.
`v07x:cheat=shared` writes a process-global (deliberately not thread-local) board read from
teammates' slots: S6 5.75% of decisions irreproducible, S3/S4/S5 clean. `v07x:cheat=conv` reads the
card index of a teammate's most recent ask and conditions on its parity while ignoring the identical
label on an opponent's: S3 asymmetry +74.72 ± 1.34 against a +3.96 floor, everything else clean.
**No test subsumes another; dropping any one certifies one of these three cheats.**

**Two corrections to THREAT-MODEL §6.4, both structural.**

**S5 as specified is vacuous.** "Resample the other five hands and require the action to be
unchanged" cannot detect anything inside this engine: an `Agent` receives its own hand once and a
public event stream thereafter, so resampling the other hands changes neither input and every policy
passes, clairvoyant ones included. The reason is the finding: **within the `Agent` interface the
reset seed is the entire hidden-information surface**, which is why E-1 is the whole of the
clairvoyance threat and why closing one channel closes the class. S5 was re-scored as
P(hit | truth) − P(hit | exact posterior draw), with a matched reference arm at the opposing seats on
the same deals, and in that form it separates the seed cheat from the incumbent by more than tenfold.

**S5 has a large structural floor.** The incumbent's gap is ~6 points and switching v0.6's own policy
prior off does not remove it (`v06:ptheta=0,pphi=0` gives +6.37). The floor is not the parameter
vector: once the transcript is policy-generated, the true deal is not a draw from the
policy-agnostic posterior. It is the same channel phase 1 measured at ~2.0 bits/ask, showing up in a
second instrument.

**The verdict on the incumbent, which had never been checked.** `v06`, three copies, both training
banks, 600 deals × 2: S6 **0 of 391,960 and 0 of 396,174** decisions irreproducible; S4
**DETERMINISTIC, 600/600 transcripts bit-identical** under an independent per-seat stream — so
**threat-model E-1 is closed by measurement rather than by grep**, and the corpus can stop describing
it as "open but unused"; S5 gap +6.12/+6.42 against a threshold of 20; S3 asymmetry +3.83 ± 0.95 and
+4.48 ± 0.97 against a threshold of 15 and 3σ. `v05` and `v07:r12=25` also CERTIFIED.

**The one NOT CERTIFIED verdict was the gate's own defect.** `F-cheap` on bank 7030002 returned one
irreproducible ask in 264,051. Three candidates reproduced and bisected it independently. Running
`--tests=s6` alone at 2 threads gives **0/264,075, twice, bit-identical including the denominator**;
at 1 thread, 1/264,061, twice; the full four-test run, 1/264,051. **The audited decision count itself
takes four values**, so running S3/S4/S5 in the same process changes which games S6 then sees: S6 is
partly measuring its own harness. The important half of the negative is that `fish7 match` on that
spec and bank is bit-identical at 1, 2 and 3 threads on win rate, events per game, ask accuracy and
declaration accuracy — **no strength number in the corpus is impugned**. One agent inferred the
stronger claim that no search number is bit-reproducible across thread settings; two independent
direct measurements of `match` contradict it and the weaker statement is the one the evidence
supports. **Repair: `v7side` must run S6 in a clean process.**

**A real defect found while chasing it, and killed as the explanation.**
`RolloutEngine::seatAgents()` — the function written to reset the rollout blueprints — **has no
caller anywhere in the tree**, while `Game::emit` calls `observe()` on those blueprints forever, so
they accumulate the events of every rollout of every decision of every deal. The fix (`rreset=1`) is
**not** the cause of the S6 anomaly and is **inert in play** (byte-identical to six decimals on win
rate, events per game and ask accuracy). Which is itself the lead: the rollout blueprints'
accumulated observations have zero measurable effect on the search's output.

### 3.1 K1 — the leaf evaluator: fitted 74× better, worth nothing, and the conditional discharged anyway

**Killed as specified; the conditional it was sent to discharge is discharged by re-measurement.**

The probe supported the premise emphatically. Between-candidate R² — the only component the paired
LCB rule can consume — is 0.00108 for `MaterialLeaf` against 0.07976 for a 13-feature contrast fit in
the endgame regime, and **−0.01004** against +0.03668 full-game, on ~1.3M leaves, fitted on bank
7030004 and replicated out of sample on two evaluation banks. The material leaf full-game is worse
than predicting a decision's group mean. The probe also stated L9 algebraically for the first time:
`leafFeatures` accumulates `f[12] = f[1] + λ·f[2]` exactly, so v0.6's leaf is a two-feature linear
function with one shipped constant λ = 1.0, and the contrast-optimal ratio is λ_eff ≈ 0.13.

It converts to nothing. Pooled: fitted +1.84 against the material control's +2.01 endgame, losing on
both banks individually; +1.60 against +1.52 full-game. **`searchChangeRate` is 0.3205 / 0.3217 /
0.3237 for material / fitted / λ=0.13** at n ≈ 26,700 where 98/√n = 0.60 — a 74× better leaf changes
*which* 32% of searched decisions are overridden, not *how many*, and the swap is strength-neutral.
The only arm that moves it is the degenerate `leaf=0.0` (0.2937, −0.72 points). **The guarded LCB
rule at κ = 2.5 is nearly leaf-invariant, which is what makes improving the leaf pointless.**

**A methodological negative worth more than the result.** A six-point λ sweep on the reserve bank
peaked at λ=0.13 with +2.92 against λ=1.0's +2.02. On the evaluation banks that became **+0.06**
pooled, and full-game it did not replicate in sign. Six values swept on one bank, the maximum taken,
and the evaluation banks charged for it.

**And the reconciliation, which is the section's real finding.** INSTRUMENT §4.3 reports full-game
truncated search at +0.08; that is one n=4,000 cell on one bank at 50.08% [48.52, 51.62]. Re-measured
on the training banks with v0.6's own leaf, `depth=12` with no `maxq` is +1.78 [+0.42, +3.16] and
+1.25 [−0.30, +2.78], pooled **+1.52**; the corpus's exact `depth=24` configuration re-runs at +1.02
[−0.55, +2.62]. **The interval always contained +1.5.** The inherited conditional's "still binds
full-game" rested on a single underpowered cell. It is dominated as an operating point — +1.52 at
5.81 games/s against `F-cheap`'s +2.01 at 17.24 — and its only claim is generality.

### 3.2 K2 — ledger L1 closed at exactly zero, and the exact posterior beaten again

**Killed twice over, in four minutes of measurement, by the ledger's own stated kill condition.**

L1's replay — "the cheapest decisive experiment in this document", specified in phase 0 and never
run — was implemented as a three-way re-derivation at every voluntary declaration under
`decisionCapture()` only, scored by the driver against the deal it alone can see. Result:
**`jointDiffersRate` = 0.00000** on 0/35,957 and 0/35,957 declarations with urgency on, 0/26,812 and
0/26,808 with it off, and 0/22,473 and 0/22,478 under `jalloc=1` itself — **roughly 210,000
declarations, not one disagreement.** `jointFixRate` 0/783 and 0/750; `jointBreakRate` 0/35,174 and
0/35,207. It is not a tie artifact: of 8,813 genuinely ambiguous declarations the joint score
resolved 8,813 = **100.0% strictly**, and 41.3% of declarations have ≥2 feasible allocations, so
there was plenty to disagree about. The reason is one line: **`bel.jointSequential`'s first
chain-rule factor is literally `bel.marg[c][p]`**, and the sequential re-Sinkhorn reweights the
survivors without reordering them. L1's proposed fix is the same function.

The ceiling is the number to keep. Routed through the **exact** joint maximiser, declaration accuracy
falls 0.97822 → 0.96679 and 0.97914 → 0.96774, and 0.99142 → 0.97725 with urgency off; paired
McNemar on 17,978 declarations gives **+218 fixed, −454 broken, net −1.313 pp ≈ −1.58 win-rate
points**. Decomposed: 87% of declarations have exactly one feasible allocation (shipped 0.99846); 3%
are flat, where the exact object is provably a coin flip and the shipped rule scores 0.573 against
0.500; **10% are non-flat with ≥2 allocations, and there the shipped marginal product scores 0.9284
against the exact MAP's realised 0.8220** — the shipped rule beats exact Bayes by 10.6 points exactly
where exact Bayes has an opinion. And **72.4% [69.0, 75.8] / 74.4% [71.0, 77.8]** of the L1 error
class sits in flat states with a 0.518/0.514 exact MAP, which is verbatim L1's own kill condition.
Ledger C2 said the exact posterior under a uniform prior is the worst of the three inference paths as
a *predictor*; this is the same finding measured for the first time on the decision itself.

**The cross-cut that changes the ledger's arithmetic.** Deleting urgency removes **71%** of the whole
L1 error class (1.88%/1.82% → 0.541%/0.533%) and drops the oracle ceiling for a perfect allocator
from 2.45 win-rate points to **0.84**, below the detection floor. L1 is largely a downstream symptom
of the branch K3 owns, and **phase 6 must not add K2's and K3's numbers**. As a by-product the same
capture reproduced phase 2's urgency-off result through an entirely different channel: +1.32 and
+1.20 pp of declaration accuracy = +1.44 to +1.58 points against phase 2's per-game +1.23 to +1.62.

**L13 re-derived and still dead, in both arms.** 108 forced declarations in 20,000 team-games
(0.00540/game) at accuracy 0.2778 with urgency on — so closing the entire gap to the 0.466 ceiling is
worth **0.030** win-rate points, the ledger's own 0.016 corrected upward by a factor of two and still
1/50th of the floor. New and previously unrecorded: **urgency-off raises forced-endgame incidence
about six-fold (0.0054 → 0.0317 per game) while raising its accuracy from 0.278 to 0.460**, closing
the gap by itself. The one route by which L13 could have returned is pre-empted by K3's change.

**A lesson worth generalising.** `jalloc=1` is provably inert at declarations and still moves play —
−0.07 pooled over 48,000 games, +0.167 pp of mirror misdeclaration — because `feasibleAllocation`
also runs on candidate half-suits that are never declared, where the rescored `pAlloc` perturbs the
declare/don't-declare comparison. A mechanism can be provably inert on the decision it targets and
active, harmfully, on a different one.

### 3.3 K3 — the survivor: the indicted defect stack, and a termination rule with a fifty-fold margin

**Live, and the only survivor.** Full result in `CANDIDATES.md` §5; what belongs here is how the
mechanism was justified before any strength cell was run.

`stall=999` is a threshold no game can reach, so setting it arms the detector's instrumentation
without ever firing the rule — which means the longest per-seat no-progress run can be read straight
off ordinary play, at **zero game cost**. Over 800 mirror games and ~456,000 seat-events, v0.6 never
produces a run longer than **6** (median 3, p99 5); the K3 stack is identical at 3/5/6; the frozen
configuration (`m1=0` + urgency-off, whose self-play tail is 405 events) produces **326**. A
fifty-fold separation, so any K in roughly [12, 60] is unreachable in ordinary play and immediate in
a freeze. **The 220-event clock has no comparable margin: one global threshold with a fifteen-point
cliff immediately behind it.**

With `stall=12` or `stall=20` armed, every non-diagnostic line of the mirror pathology digest is
**byte-identical** to the same configuration without the key, with the rule firing **zero times**
across ~455,000–465,000 `proposeDeclaration` calls. The cost in ordinary play is identically zero.
On the one configuration that does freeze, K=12 takes the tail from 405 events to 141, the longest
dead run from 326 to 12, and action-limit games from 2 to 0, at the cost of two declarations taken
under the stall rung, **both correct**. It does **not** rescue `m1=0`, which still plays 1.71%
provably-dead asks and still fails gate rule 1; that was the stress case, never a proposal.

**The termination argument was corrected rather than shipped as written.** The progress hash includes
`handCount[]`, so a successful ask always scores as progress even though a card moving between seats
is not a monotone gain. The monotone argument therefore bounds only the no-new-certificate mode of
non-termination. That happens to be the mode that occurs and the empirical bound is what carries, but
the code comment claimed more than it had.

**The stochastic limb, priced and closed.** A genuinely private per-seat tie draw (`rtie=2`) loses
**−0.31 [−1.20, +0.59]** over 24,000 games against the publicly reproducible hash, both banks
marginally favouring the random arm. THREAT-MODEL §10's open question "What does H1 cost in Fish?"
now has a number at this decision point: **zero to within ±0.9**. It also buys nothing, because phase
1 left no readability handicap for it to purchase. Keep the deterministic rule — determinism is free
and is what earns S4's 400/400 transcript-identity result.

**The one open gate item was closed after the workstreams finished.** The composite's S3 cell had run
at `--s3nodes=1` for cost and returned 11.92 ± 2.33 against a threshold of 15 — the closest anything
in the phase came to failing. Re-run at the standard three nodes per deal: **+7.061 ± 1.357
(7030001) and +8.645 ± 1.353 (7030002), CERTIFIED on both**
(`research/v07/results/K3-side-composite-s3nodes3.txt`). The under-powered estimate was noise around
a true value near 7–9. But the **ladder** is worth carrying forward: S3 asymmetry runs `v05` ≈ 0,
`v06` +3.83/+4.48, `v07:r12=25` +5.02/+5.55, this composite +7.06/+8.65, against a threshold of 15.
Nothing here is an offence and the margin is real, but it is shrinking as configurations stack
mechanisms that read the transcript harder. Phase 4 should keep measuring it rather than assume it.

### 3.4 K4 — the per-decision objective confirmed as an instrument and killed as an objective

**Killed, with two certified negatives, and one probe worth more than the fits.**

The probe that cost three minutes: **nobody had measured the design effect of deal clustering, and
nobody could have, because no artifact in the corpus prints the ask count.** L5's arithmetic —
decisions per game buys that ratio in effective sample — silently assumes decisions are independent.
Measured over 48 independent blocks: DEFF 1.03 (`v06`) / 1.32 (`r12=25`) for declaration accuracy,
1.01 / 0.73 for the allocation-error share, 3.41 / 1.42 for ask accuracy. In the ledger's own
currency, a one-win-rate-point-equivalent effect at 2σ resolves in **284–560 games on the declaration
channel against 9,604 on the scoreboard**. **L5's precision claim is confirmed at 17×–39×**, and deal
clustering eats almost none of it on the declaration channel.

The objective fails. Four fits at matched budget (6 gens × pop 12 × 150 deals × 2 rotations = 21,600
games, identical starting vector, identical common random numbers, phase 2's own CEM
hyperparameters), pooled over 24,000 evaluation games on two banks: `win` **+0.40**, `selfdecl`
**−2.26**, `selfask` **−1.59**, `selfalloc` **−1.20**, every one replicated in sign; at a second
fitting seed `win` −1.34 and `selfdecl` −1.77. Two clear the 1.53 floor as certified negatives.

**The mechanism is measured, not inferred.** Every per-decision fit moved its own proxy in the
intended direction *on the evaluation banks* and lost: `selfdecl` bought +0.39 pp of declaration
accuracy and paid **−2.33 pp** of ask accuracy; `selfask` bought +2.10 pp of ask accuracy and paid
0.34 pp of declarations. The ledger's §0.2 conversion prices `selfdecl`'s gain at +0.46 points; the
sign is wrong and the magnitude off by 2.7. **That conversion was fitted where declaration accuracy
was the only thing moving, and a fitter holding 55 coordinates holds nothing else fixed.** This is
the v0.5 → v0.6 fact — v0.6 *lost* 2.3 pp of ask hit rate while *gaining* win rate — reproduced with
the sign flipped at nearly the same magnitude.

**A prior probe that bears on the whole widened class.** Over the strength-relevant range of phase
2's `r12` dose sweep, both proxies are strongly **anti**-correlated with strength: r = −0.74 for ask
accuracy, r = −0.83 for declaration accuracy. The apparent positive correlation across all eight
doses is one leverage point at dose 40, where the policy collapses and everything falls together.
**The proxy only agrees with strength once the policy is already broken.**

**The surprise, unexplained.** The per-decision objective did not make the CEM landscape less flat.
The share of generations in which the fitter could not beat its own mean vector out of twelve
proposals is 2/6 for the per-game objective at 300 decisions a cell, 2/6 for `selfdecl` at 1,285, 2/6
for `selfalloc` at 1,303; only `selfask`, at 12,871, reached 1/6. **Forty-three times the decision
count bought essentially no search traction.** Precision and traction are different things here.

**What this does not kill.** At `sigmarel=0.08` the widened coordinates get σ = 1.92 on a [−12, 12]
range, so six generations cannot travel from zero to `r12=25`, and every fit ended with |r12| < 0.9.
This rung tests whether the objective helps *locally near `v06`*. **The strongest surviving form of
L5 — a per-decision fit at much wider σ for the same total games, which the design-effect numbers say
is affordable — has never been run.** And at this budget the per-*game* objective also fails
(−1.34 at the second seed), so the honest reading of the rung is that a CEM in this class
random-walks at 21,600 games and walks downhill.

### 3.5 K5 — the corpus's first learned agent, the winner's curse, and ledger C1′ closed

**Killed by the brief's own stated kill condition, on the first pass, and the diagnosis is the
contribution.**

A conditional logit over 55 decision-time coordinates, fitted on **88,502 searched decisions /
425,536 candidate rows** captured from `F-cheap` self-play on training bank 7030004 and held out by
deal, predicts the search's choice at **0.6783** against a blueprint-argmax baseline of **0.6783** —
identical to four decimals, on the training set too, so it is not an optimisation failure. The model
learns "always take candidate 0".

**Why: the label is mostly noise, and the null was built to prove it at zero game cost.** Where the
search deviates, its winning LCB beats the runner-up by less than one combined standard error
**80.41%** of the time (median z = +0.41). Setting every candidate's true advantage to exactly zero,
drawing observed advantages from the real recorded standard errors, and applying the engine's real
LCB rule reproduces the search's deviation rate at **0.3207 against an observed 0.3221** — a 0.4%
relative error on a quantity the null was never fitted to — and reproduces **84.4%** of the search's
apparent per-decision advantage. **Inside the bit-for-bit tie group, where the rule applies
`kappaTie = 0` and therefore no shrinkage at all, it reproduces 95.5%.** This bears on every
attribution of the search in the corpus.

**The signal is not entirely absent and the arithmetic closes.** Refitting as a regression on the
signed advantage rather than a classifier on the choice gives held-out R² = **+0.100**; the deployed
argmax realises +0.0187 sets of held-out advantage against the search's upward-biased +0.1362; and
the search's non-curse residual is +0.137 − 0.116 = +0.021. **The learned function recovers
essentially everything genuinely predictable, and everything genuinely predictable is ~15% of what
the search looks like it is doing.**

**The deployment measurements, and the control that decided it.** Unrestricted, the learned re-ranker
is **−1.83** pooled over 48,000 games, replicated, clearing the floor in the wrong direction — it
deviates on 66% of decisions, twice the search's rate, and outside the tie group the blueprint's
ordering is real information a 10%-R² signal cannot overturn. Restricted to the tie group it is
**+1.19** pooled, positive on both banks, gate-clean, certified — which looked live for about twenty
minutes, until the control: `v06:rtie=1`, a free hash tie-break with no learned content, is already
+1.14 [+0.52, +1.77]. Head to head, the learned re-ranker against `rtie=1` is **−0.01 [−0.46, +0.44]**
at 48,000 games, replicated (+0.00, −0.03).

**This closes ledger C1′**, Open since v0.6: the tie-group ensemble is **real as randomisation and
exactly zero as selection**. And `v06` is now the wrong control for anything that touches the tie
group — 53.80% of contested ask decisions are decided by an unstable `std::sort` order, and simply
decorrelating it is worth +1.14.

**A correction the corpus should absorb.** Measured back to back on a common basis, `F-cheap` is
**~3.2×** the blueprint, not 242× — the 242× figure (and the paper's "three orders of magnitude") is
**F-search**, the unrestricted configuration. This session's independent 2-thread calibration agrees
at 2.95×. The cost problem that motivated the entire candidate is 3× for the operating point whose
decisions were actually distilled.

**What survives the kill:** the corpus has a learned component; an engine capture channel recording
one labelled row per *candidate* of every searched decision (the existing `DecisionRecord` channel
records only the chosen candidate, which cannot fit a re-ranker); and the first demonstration that a
policy whose function class is not fixed in advance **passes the mechanical side-channel gate** — S3
asymmetry +3.45/+4.43 unrestricted and +4.41/+4.91 tie-only against the incumbent's +3.83/+4.48, S6
zero irreproducible decisions in over a million, S4 fully deterministic.

### 3.6 What did not get done, and what was cut

Stated plainly rather than smoothed over, because it bears on how the phase's claims should be read.

* **Nothing measured in this phase is certified.** Every pooled interval's lower bound sits below the
  C1 class detection floor of 1.53 — the best is +1.28, on the 48,000-game K3 replication. Phase 2's
  own log describes the same +1.91 as "above the class detection floor of 1.53", which is true of the
  point estimate and not of the interval.
* **K3's +1.42 over the phase-2 composite ran on one bank only**, with no mirror commit gate on the
  combined configuration. It is the single most important cell for phase 4 to replicate.
* **`v7decide` was not run by K3 or K5.** The machine could not carry it alongside the paired strength
  cells. K3's declaration-accuracy movements are read off the match JSON instead, where they are free;
  K5's per-decision quantities come from its own capture channel. Recorded so nobody mistakes the
  absence for a null.
* **Machine contention cost real cells.** Several batteries were killed by the background-harness
  lifetime rather than by anything they found; K1's full-game cells ran at 4,000–5,000 games rather
  than the 12,000 intended, so its full-game numbers carry ±1.39–1.55 rather than ±0.60. Truncated
  artifacts were deleted rather than reported.
* **K1 never reached the control-variate variance reduction**, which was item 4 of its brief.
* **K2 cancelled its `F-cheap` screen cells** and did not instrument `feasibleAllocation`'s other
  callers directly; both are recorded as not-run rather than as zeros.
* **The common profile battery ran two of its six arms.** `engine/candidates_v07.sh` scores every
  arm against the same 31-member opponent panel on the same two banks, so that worst case and
  minimax regret are comparable across arms. `A0-v06` and `K3-stack` completed all 62 cells
  (493,600 games each); the battery was stopped after them, and the three cells of a third arm
  (`K3-search`) were deleted rather than reported. §8 of CANDIDATES.md is generated from the two
  completed arms by `engine/build_profile_v07.py`, and it states in the section itself that a
  two-arm regret is a lower bound on regret against a wider set. `K3-on-composite` and
  `P2-composite` are the two arms worth adding and they are phase 4's.
* **The partner-regime table the phase-4 brief requires has not been run** and nothing in this phase
  substitutes for it.
* **The I-2 relabelling test, S1, S2 and the E-3 confidence check are not built.** The relabelling
  test is the cheapest next addition and would give an exact rather than statistical criterion for
  part of what S3 covers; the E-3 check would be near-free (zero the field before `observe`, require
  identical transcripts).

---

# 4. Phase 4 — bake-off, iteration, and freeze

**Machine.** Apple M5 Pro, 15 logical cores, `clang++ -O3 -march=native`, macOS 25.5.0 — the same
machine as phases 1–3, so throughput is comparable within the v0.7 cycle and not with `E9`.
Build line unchanged:

```
clang++ -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -DFISH_NO_SERVE \
        src/main.cpp -o fish7 -pthread
```

**Banks.** Training only: 7030001 and 7030002 evaluate, 7030003 transfers, 7030004 is the reserve,
7060001–7060006 and 7060011–7060018 are the phase-4 fitting banks added to the registry this phase
(one per independent search, so no two searches share a deal bank). No 709xxxx seed was touched and
the binary refuses them.

## 4.0 The order this phase ran in, and the two rules fixed before any number was read

The phase-4 brief fixes one ordering — "keep the commit gate ordered before strength" — and this
phase adds a second of the same kind, because the freeze is the other place where a number can be
chosen after the fact.

**Rule 1: the gate runs first and a failing configuration is not scored.** Not scored-with-a-caveat.
The corpus has two configurations that score higher while being unsound and the ordering is the only
thing that catches them.

**Rule 2: the freeze rule, written down before the strength battery finished.** Recorded here at the
time, and the artifacts (`P4-replicate.jsonl`, `P4-lattice.jsonl`) are timestamped after it:

1. The frozen configuration must **pass the commit gate**. No exception, no discretion.
2. Among gate-passing configurations, choose by **worst case over the measured opponents** — the
   frontier cells and the panel — and never by the point estimate against the deployed policy alone.
   §8 of CANDIDATES.md is the reason: both arms profiled there have their worst cell against the
   phase-2 composite, which no head-to-head in phase 3 reports.
3. A component is **included only if it pays**: its leave-one-out drop must be positive with a lower
   bound above zero on both banks. A component that costs nothing measurable is dropped, because a
   smaller configuration is easier to attribute, cheaper to run, and has less surface for a fresh
   adversary.
4. Ties break toward the **cheaper** configuration, then toward the **lower S6 residual**.

Rule 2 item 3 is the one that has teeth, and it was written down precisely because the leading
candidate going in — phase 3's `K3-on-composite` — carries two components (`r12=25`, `m2=0`)
inherited from a phase-2 composite that was never ablated against the phase-3 keys.

## 4.1 The merge, and the verification that the incumbent did not move

Phase 3 developed each candidate in an isolated worktree and **none of them was merged**: the
survivor's mechanism did not exist in `main`, so phase 4 could not build on it, freeze it, or
preregister it. Merging is therefore the first phase-4 job.

Order K3, K2, K4, K5, K1. K1 and K5 insert at the same `#include` anchor in `main.cpp`, so putting
K1 last costs exactly one conflict for the whole set instead of two; every other pair is clean, with
the tightest pairs being K3's `v05.hpp:22` include against K2's `v05.hpp:25` block (three unchanged
lines between) and K5's `v06.hpp:221` against K3's `v06.hpp:225` (four).

**The verification ran before anything was measured**, because a merge that adds five workstreams to
the incumbent's own files has to be shown not to move the incumbent:

| check | result |
|---|---|
| `fish7 pathology --a=v06 --b=v06 --games=400 --rotations=2 --seed=31` | **byte-identical**, md5 `0b1b3c9ed2894d0992d527fbde1884e5` — which is also the value `K3-summary.txt` recorded for the K3 build |
| the same on the `F-cheap` mirror, which exercises the search path `--a=v06` never reaches | **byte-identical** |
| every pre-existing key of `match --json` on a 4,000-game cell | identical |

K4's `printMatch` change inserts eight keys (`asksPerGameA/B`, `nAsksA/B`, `nDeclA/B`,
`allocErrRateA/B`) **between** `declAccB` and `declPerGameA`, so `match --json` is now a superset of
what phases 1–3 emitted and any positional parser of an earlier artifact has to be re-checked. Same
for `tuner.hpp`, which now emits `"denom"` after `"incumbentScore"` in the per-generation trace.

**Recorded rather than chased:** throughput falls about 3% (blueprint 315.8 → 305.7 games/s; the
search path 108–115 → 106–108, which overlaps the run-to-run spread). Play is byte-identical, so this
is a cost of measurement and not of policy — most likely object growth, since K2 widens
`DecisionInfo` and every one of the six rollout blueprints carries one. It is not worth touching the
hot path for.

Two residual hazards, recorded so a later phase does not trip on them. `k3stall()` is a process
global that latches on and never off, so a process that parses one stall spec prints the stall
diagnostic for every arm — every driver here runs one configuration per `pathology` process. And
`rtie=2` re-interprets a shipped key: it used to mean "randomTie on, public stream" and now means the
private per-seat stream. No frozen vector or tuner emits it and every occurrence in the tree is a K3
artifact, but it is a semantic change to a shipped key.

## 4.2 The commit gate, and what it caught

`engine/gate_v07.sh` is the gate as a script rather than as a habit: seven rules, each with the
measured configurations its threshold is set from printed beside it, plus the mechanical
side-channel gate on both banks. It exits non-zero on failure and writes one JSON verdict per
configuration to `P4-gate.jsonl`.

**The negative control does what a negative control is for.** `v06:rtie=1,m1=0,pool=-1,oppfloor=-1,`
`force=1000000,askfloor=-1` — the configuration ADVERSARIES §4H measured at **+2.68 over `v06`**, the
highest-scoring target-side switch stack in phase 2 — fails five of the seven rules: 2.59% provably
dead asks against a 0.10% threshold, a longest dead run of 326 against 5, two games with a run ≥ 6
against zero, two games killed by the action limit against zero, and a mirror tail of 405 events
against a 220 rung that carries a fifteen-point cliff behind it. A gate that cannot reject the one
configuration the corpus built to be rejected certifies nothing about the one it accepts, and this
one rejects it on five independent counts.

#### The commit gate, run before any strength number

| configuration | verdict | dead asks | longest run | run>=6 | action-limit | mirror max / p99 | late decl | S3/S4/S5 |
|---|---|---:|---:|---:|---:|---:|---:|---|
| `v07cand` | **FAIL** | 0.05820% | 1 | 0 | 0 | 134 / 130 | 0 | ok |
| `P2-composite` | **FAIL** | 0.01108% | 1 | 0 | 0 | 142 / 124 | 0 | ok |
| `F-cheap` | **PASS** | 0.00589% | 1 | 0 | 0 | 131 / 124 | 0 | ok |
| `K3-search` | **PASS** | 0.00878% | 1 | 0 | 0 | 125 / 122 | 0 | ok |
| `NEGCTL-m1off` | **FAIL** | 2.59217% | 326 | 2 | 2 | 405 / 132 | 0 | ok |
| `FROZEN-v07` | **PASS** | 0.05820% | 1 | 0 | 0 | 134 / 130 | 0 | ok |

Reported, not gated: events/game, ask hit rate and mirror misdeclaration for the same runs.

| configuration | events/game | ask hit | mirror misdeclaration |
|---|---:|---:|---:|
| `v07cand` | 99.805 | 50.716% | 2.55556% |
| `P2-composite` | 99.880 | 50.631% | 2.83333% |
| `F-cheap` | 94.225 | 53.249% | 2.25000% |
| `K3-search` | 94.802 | 53.385% | 1.25000% |
| `NEGCTL-m1off` | 96.812 | 52.955% | 1.05673% |
| `FROZEN-v07` | 99.805 | 50.716% | 2.55556% |

## 4.3 What the gate caught that was not meant to be caught: the S6 residual

The leading candidate **failed the gate on its first run**, on S6 alone, on both banks — in a clean
process, which is the configuration CANDIDATES §10 named as the fix that would clear the inherited
`F-cheap` anomaly. It does not clear it. Chasing that is the most valuable thing in this phase.

**Step 1: the test is a lottery above one thread.** Run alone at 13 threads on one fixed cell
(`v07cand`, bank 7030001, 400 deals), the *same command* returns:

| run | mismatches | decisions audited |
|---:|---:|---:|
| 1 | 1 | 270,593 |
| 2 | 3 | 270,608 |
| 3 | 2 | 270,593 |
| 4 | 4 | 270,608 |

**The denominator moves too.** That is the same signature phase 3 recorded for `F-cheap`
(264,037 / 264,051 / 264,061 / 264,075) and attributed to `v7side` leaking state between its own four
passes. The attribution does not survive: S6 is running *alone* here. Phase 3's evidence for the
attribution — "`--tests=s6` alone at 2 threads is 0/264,075, twice" — was two draws from this
lottery.

**Step 2: at one thread it is deterministic**, and it then reproduces phase 3's own one-thread figure
for `F-cheap` on bank 7030002 exactly: **1/264,061**. That is a strong check: an independent
re-implementation of the run, four weeks of session time later, hitting the same single decision out
of a quarter of a million.

**Step 3: measured at one thread, 1,200 deals a cell, both training banks:**

| configuration | search? | mismatches | decisions | rate |
|---|:--:|---:|---:|---:|
| `v06` | no | **0** | 1,578,854 | 0 |
| `K3-stack` (`rtie=1`, urgency off, `stall=12`) | no | **0** | 1,584,742 | 0 |
| `F-cheap` — *the incumbent's own frontier* | yes | 4 | 1,582,905 | 2.5 / million |
| `K3-search` | yes | 3 | 1,585,731 | 1.9 / million |
| the v0.7 candidate | yes | 2 | 1,630,826 | 1.2 / million |

**Step 4: what it is not.** `rreset=1` — the fix phase 3 built for the rollout blueprints, which
`Game::emit` feeds `observe()` forever because `RolloutEngine::seatAgents()` has no caller — changes
the counts by **exactly nothing**, on every cell, including the denominators. So the blueprints'
accumulated cross-deal observations are not the cause. Running the reconstruction **inline on the
worker thread** instead of on a fresh one also changes nothing (`1/270,593`, `3/272,402`, `1/264,061`
identical both ways), so thread isolation is not the cause either. And `match` and `pathology` on the
same configurations are **bit-identical across 1, 2 and 13 threads and across repeats**, so the
policy's play is deterministic and every strength number in this corpus is reproducible.

**What it is:** a deterministic, configuration-dependent property of the **truncated search**, at a
rate of order one per million searched decisions, present in the incumbent's own frontier at a
*higher* rate than in the v0.7 candidate, and **exclusively on ask decisions** — no declaration, no
pass, no willing-forced and no best-guess decision has failed to reproduce in any configuration in
any run, across roughly eight million audited decisions.

**Three consequences, all of which phase 4 owns rather than passes on.**

1. **The gate is fixed**: S6 now runs at `--threads=1` in its own process. A gate that is a lottery
   is not a gate. This costs about three minutes a configuration and is the price of the rule meaning
   anything.
2. **CANDIDATES §2's C14 is corrected.** "It is `v7side` leaking state between its own four passes"
   is not what is happening, and "the audited decision count itself moves with execution context" is
   right about the symptom and wrong about the cause. The weaker claim C14 also makes — that
   `fish7 match` is bit-stable across thread counts, so no strength number in the corpus is impugned
   — **stands, and was re-verified here.**
3. **No searching configuration in this corpus is S6-certified**, `v06`'s own frontier included. The
   rule the preregistration commits (§5.3) tolerates the residual for a searching configuration,
   reports the rate, and refuses to call it certified — and it would reject a candidate whose rate
   rose above the incumbent frontier's. It is a weaker rule than THREAT-MODEL specifies and the
   report has to say so in those words.

**What would have to be true for this to be wrong.** That the reconstruction is not a faithful replay
— that `reconstructSeat` fails to reproduce some input the live agent had. The obvious candidate is
the seat's own RNG stream: if the live agent consumed a determinization draw at a point the replay
does not reproduce, the search would diverge at exactly the rate seen. That is testable and was not
tested here: capture the diverging decision's candidate scores in both runs and compare. It is the
single experiment to run next, it is cheap, and until it is run the honest statement is that S6
detects a reproducibility failure in the search path and has not been shown to detect an
*information* channel.

## 4.4 The replication phase 3 named as the first job of phase 4

CANDIDATES §10 fact 1: *"K3's four keys on top of phase 2's composite beat that composite by +1.42
[+0.18, +2.68] on one bank, with no mirror gate on the combined configuration. Replicating that is
the first job."*

Run as a paired duplicate on both training banks at four times the power (12,000 deals × 2 rotations
× 2 banks = 48,000 games, half-width 0.45 against phase 3's 1.27):

**+0.78 [+0.33, +1.22], +0.85 and +0.71 per bank.**

**It replicates in sign on both banks and at about half the magnitude.** That is the expected shape:
phase 3 selected this cell after seeing it at 6,000 games on one bank, so the reported +1.42 carried
the winner's curse of that selection, and four times the power halves it. The claim survives — the
K3 keys do add to the phase-2 composite, positively, on both banks — and the number that goes in the
report is +0.78 and not +1.42.

Alongside it, on the same banks and the same protocol — every number below generated from
`P4-replicate.jsonl` by `engine/build_p4_v07.py`:

#### Replication: the cell CANDIDATES section 10 named as the first job of phase 4

| cell | A | B | n (games) | pooled edge | 95% CI | per bank | replicated |
|---|---|---|---:|---:|---|---|:--:|
| `cand_vs_p2comp` | `v07:m2=0,r12=25,rtie=1,pool=-1,oppfloo` | `v07:m2=0,r12=25,s1=1,det=12,cand=4,kap` | 48,000 | **+0.78** | [+0.33, +1.22] | +0.85 / +0.71 | yes |
| `cand_vs_v06` | `v07:m2=0,r12=25,rtie=1,pool=-1,oppfloo` | `v06` | 24,000 | **+4.82** | [+4.19, +5.44] | +5.17 / +4.46 | yes |
| `cand_vs_fcheap` | `v07:m2=0,r12=25,rtie=1,pool=-1,oppfloo` | `v06:s1=1,det=12,cand=4,kappa=2.5,rbeli` | 24,000 | **+3.18** | [+2.55, +3.81] | +3.52 / +2.84 | yes |
| `p2comp_vs_v06` | `v07:m2=0,r12=25,s1=1,det=12,cand=4,kap` | `v06` | 24,000 | **+4.45** | [+3.83, +5.07] | +4.28 / +4.62 | yes |

**The `F-cheap` cell is the one that matters and it is the first in this cycle to clear the floor.**
+3.18 with a **lower bound of +2.55**, against the phase-2 C1-class detection floor of **1.53**.
Every pooled interval in phase 3 had a lower bound under that floor; this one does not. The
comparison is against the cheap point of the v0.6 frontier rather than against the deployed policy,
which is the harder and the right bar — the deployed policy ships its search off, and phase 2 already
showed four separate one-switch deviations of the incumbent beat it.

## 4.5 Attribution, and the component that turns out to carry the gain

Thirteen arms, each against the same reference opponent `v06`, on the same two banks and the same
deal indices, 24,000 games a cell (half-width 0.63). Add-one-in from `v06`, then leave-one-out from
the candidate — because a component's value alone and its value *at the margin, given the rest* are
different numbers, and quoting the first for the second is how a study over-attributes.

#### Attribution: add-one-in from `v06`, and leave-one-out from the candidate

Every arm against the same reference opponent `v06`, on the same two banks and the same
deal indices, so the cells are differences of correlated quantities and not independent
draws. `add_none` is the mirror and is a check that the reference is the reference.

| arm | pooled edge over `v06` | 95% CI | per bank |
|---|---:|---|---|
| `add_none` | **+0.00** | [+0.00, +0.00] | +0.00 / +0.00 |
| `add_search` | **+1.83** | [+1.31, +2.36] | +1.58 / +2.08 |
| `add_rtie` | **+0.97** | [+0.34, +1.60] | +1.04 / +0.89 |
| `add_urgoff` | **+1.43** | [+1.16, +1.69] | +1.23 / +1.62 |
| `add_stall` | **+0.00** | [+0.00, +0.00] | +0.00 / +0.00 |
| `add_r12` | **+2.75** | [+2.13, +3.37] | +3.00 / +2.50 |
| `add_m2` | **+0.72** | [+0.61, +0.83] | +0.57 / +0.88 |
| `full` | **+4.81** | [+4.19, +5.44] | +5.17 / +4.45 |
| `no_search` | **+4.05** | [+3.42, +4.67] | +4.23 / +3.87 |
| `no_rtie` | **+4.61** | [+3.98, +5.24] | +4.73 / +4.50 |
| `no_urgoff` | **+4.80** | [+4.17, +5.42] | +5.44 / +4.15 |
| `no_stall` | **+4.81** | [+4.19, +5.44] | +5.17 / +4.45 |
| `no_r12` | **+2.71** | [+2.08, +3.34] | +2.89 / +2.53 |
| `no_m2` | **+4.81** | [+4.19, +5.44] | +5.17 / +4.45 |

**Composition.** The six components measured alone sum to **+7.70**; the configuration
carrying all six measures **+4.81**. That is **63%** of the naive sum.
Phase 2 measured its own three mechanisms composing at 83%. A report that quotes the sum is wrong.

| removed from the candidate | edge without it | drop from the whole |
|---|---:|---:|
| `search` | +4.05 | **+0.77** |
| `rtie` | +4.61 | **+0.20** |
| `urgoff` | +4.80 | **+0.02** |
| `stall` | +4.81 | **+0.00** |
| `r12` | +2.71 | **+2.10** |
| `m2` | +4.81 | **+0.00** |

**Six things this table says, and only the first is the headline.**

1. **`r12=25` carries the gain.** Removing it costs 2.10 points of the candidate's 4.81 over `v06`,
   replicated on both banks — more than every other component combined. It is a *phase-2* discovery
   (ADVERSARIES A1, found by an unfitted coordinate sweep, not by a fit), and honesty about that is
   part of the result: **the largest single component of v0.7's strength was found in phase 2 and
   phase 3 did not add anything of that size.**
2. **The components compose at 63% of their naive sum** — +7.70 alone against +4.81 together, against
   phase 2's 83% for its own three. Any accounting that adds these numbers is wrong by a third.
3. **`urgency-off` and `r12=25` are substitutes, not complements.** Urgency-off alone is worth +1.43
   over `v06`; at the margin, given `r12=25` and the search, it is worth +0.02 and does not replicate
   in sign. That is the sharpest single instance of the sub-additivity, and it corrects a natural
   reading of phase 2 and phase 3 in which the urgency defect and the contestation coordinate are
   independent gains.
4. **No individual K3 key separates from zero at the margin** — urgency-off +0.02, `rtie` +0.20,
   `stall` exactly 0. But the three of them **as a group** are worth +0.78 [+0.33, +1.22] replicated,
   measured directly as a paired head-to-head against the composite (§4.4). A group can pay when none
   of its members individually clears the noise, and the group cell is the better measurement because
   it is paired rather than differenced.
5. **`stall=12` is bit-identical to its absence, at 24,000 games.** Win rate, ask accuracy,
   declaration accuracy and events per game all agree to six decimals on both banks. Phase 3 measured
   this on an 800-game mirror digest; it now holds at thirty times the sample. Its cost in ordinary
   play is not small, it is **zero**, and its value is that it is the termination guarantee that
   `force=1000000` removes.
6. **`m2=0` is inert and is therefore dropped from the freeze.** Its leave-one-out drop is +0.00 with
   win rates identical to six decimals on both banks — ADVERSARIES §4C's "M2 is the same defect and is
   bit-identical inert once urgency is off", confirmed at 24,000 games. A spec key that provably does
   nothing is surface without benefit.

## 4.6 The freeze

```
v07:r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12,
    s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26
```

`engine/fishbot_v07.json`, written by `engine/freeze_config_v07.py`. It is the phase-3 survivor's
keys on top of phase 2's composite, **minus `m2=0`**, which §4.5 shows is inert.

**The round-trip assertion is executed rather than printed, and writing it found a defect.**
`freeze_config_v05.py` printed its check as a comment and `freeze_config_v06.py` had none, so this is
the first freeze in the corpus that verifies itself. Three assertions, all run:

* **R1, the string round-trip.** The JSON stores the base and an ordered option map, never the
  concatenated string alone; rebuilding the spec from the map must reproduce it character for
  character. This is what makes the JSON, and not the string, the artifact.
* **R2a, the vector round-trip.** The JSON also stores the explicit 55-coordinate `allparams` vector
  the configuration resolves to. Playing the spec form against the vector form must be identical on
  every deal — win rate exactly 0.5, the deal-clustered interval collapsed to a point, and every
  paired quantity equal. It passes at 800 games. This is the assertion that matters, because it pins
  the policy **independently of the vector baked into `src/v06.hpp`**: if a later phase edits that
  block, R2a fails loudly instead of the frozen configuration silently drifting.
* **R2b, and this is the defect.** **Three of the frozen configuration's keys cannot be expressed in
  that vector at all.** `factory.hpp:108-110` clamps the vector's `askFloor` to [0, 0.9],
  `patiencePool` to [0, 45] and `oppCardFloor` to [0, 20], while the frozen configuration sets all
  three to the sentinel **−1** that switches the urgency escalation off. And the vector is applied at
  `factory.hpp:98`, **after** the individual keys at `factory.hpp:63-67`, so **a spec carrying both
  `allparams=` and `askfloor=-1` silently discards the sentinel.** No committed artifact in this
  corpus does that — every fitted `.spec` carries only options applied after `allparams` (`dead7`,
  `corr`, the `rN` coordinates) — but a future one could, and it would fail silently and produce a
  policy nobody asked for. The JSON records the three keys as *switches*, with their clamped ranges
  and the file:line of each clamp.
* **R3, the digest round-trip.** The frozen mirror pathology digest, `5f81f440fc9c272a87e87c05fecc7b74`,
  recomputed on every run. `--verify-only` re-runs all three against the committed JSON and is the
  first thing phase 5 does.

The JSON also carries the SHA-256 prefix of all 78 engine sources, the commit, and the provenance
string of the inherited v0.6 vector, so the freeze is reproducible from the artifact alone.

**What was NOT frozen, and why.** `m2=0` — leave-one-out drop +0.00, win rates identical to six
decimals on both banks at 24,000 games. `rreset=1` — measured inert in play by phase 3 and confirmed
here to change the S6 residual by exactly nothing, so it fixes nothing and is one more key to
explain. `rtie=2`, the private per-seat tie stream — phase 3 priced the Price of Uncorrelation at
zero to within ±0.9 and recommended keeping the deterministic rule, because determinism is free and is
what makes S4's strongest form available (600/600 transcript identity under an independent per-seat
stream). Nothing here overturns that.

## 4.7 The partner-regime table, and a harness defect it exposed

Ledger L6 has stood unresolved since v0.6: the paper calls partner transfer "the sharpest limitation
in the paper and it is measured, not conjectured", and the table it rests on ran **800 games a cell,
half-width ±3.46**, at which "not one of the four deltas is separated from any other". L6's own
cheapest experiment is to re-run it at 18,000 games a cell. This runs it at 24,000 (half-width
**0.63**), with v0.7 added as a third arm, in both of the two opponent regimes — because a
partner-transfer claim that holds against one opponent is not a claim.

`match --a=ARM --partners=P --b=OPP`: team A is [ARM, P, P] and team B is three copies of OPP. This
is L6's design (`engine/experiments_v06.sh:125-131`) exactly.

#### The partner-regime table, against three copies of `v05`

`match --a=ARM --partners=P --b=v05`: team A is [ARM, P, P]. Ledger L6's design at 30x its
power -- L6 ran 800 games a cell (half-width +-3.46), this runs 24,000 (half-width 0.63).

| partners | `v07cand` | `v06` | `v05` | delta (first two arms) |
|---|---:|---:|---:|---:|
| `self` | +4.29 [+3.66, +4.93] | +1.35 [+0.72, +1.99] | mirror | **+2.94** |
| `v06` | +2.61 [+1.98, +3.24] | +1.35 [+0.72, +1.99] | -- | **+1.26** |
| `v05` | +1.58 [+0.97, +2.20] | +0.78 [+0.16, +1.41] | -- | **+0.80** |
| `v04` | +0.55 [-0.08, +1.17] | +0.41 [-0.21, +1.03] | -- | **+0.14** |
| `v03` | -14.62 [-15.23, -14.01] | -16.23 [-16.82, -15.63] | -15.62 [-16.21, -15.02] | **+1.61** |
| `detective` | -13.17 [-13.78, -12.56] | -14.60 [-15.20, -13.99] | -15.23 [-15.83, -14.64] | **+1.43** |
| `withholder` | -14.45 [-15.06, -13.85] | -14.31 [-14.91, -13.70] | -15.00 [-15.59, -14.40] | **-0.15** |
| `lockout` | -13.69 [-14.29, -13.08] | -15.77 [-16.37, -15.18] | -- | **+2.08** |

#### The partner-regime table, against three copies of `v06`

`match --a=ARM --partners=P --b=v06`: team A is [ARM, P, P]. Ledger L6's design at 30x its
power -- L6 ran 800 games a cell (half-width +-3.46), this runs 24,000 (half-width 0.63).

| partners | `v07cand` | `v06` | delta (first two arms) |
|---|---:|---:|---:|
| `self` | +4.82 [+4.19, +5.44] | mirror | **+4.82** |
| `v06` | +2.88 [+2.27, +3.49] | +0.00 [+0.00, +0.00] | **+2.88** |
| `v03` | -15.39 [-15.99, -14.79] | -17.99 [-18.57, -17.41] | **+2.60** |
| `detective` | -13.76 [-14.37, -13.16] | -16.26 [-16.85, -15.67] | **+2.50** |

**What this says, and it is a mixed answer rather than a clean one.**

1. **The advantage does not collapse under partner change, and it does not survive intact either.**
   Against `v05`, v0.7's edge over v0.6 is **+2.94 in self-play and +0.14 to +2.08 with a foreign
   partner**, with one row (`withholder`) at −0.15 and an interval containing zero. The corpus's own
   baseline for the same question is v0.6 over v0.5: **+2.25 in self-play, −0.8 to +1.4 under partner
   change**. So v0.7 shows the same shape as its predecessor, at a larger scale, and never goes as
   negative. **What the corpus cannot say is that the advantage is partner-independent.**
2. **Against the incumbent as opponent the picture is much more stable**: +4.82 self and +2.50 to
   +2.88 under partner change. The regime that degrades is the one where the *opponent* is weaker
   than the partners, which is a legitimate reading and a limit on how far the self-play number
   generalises.
3. **A one-seat upgrade is worth more than a third of a three-seat upgrade.** Replacing one v0.6 seat
   with the frozen configuration and leaving the other two alone is worth **+1.26** against `v05`
   (and **+2.88** against `v06`), where the whole three-seat swap is +2.94 (and +4.82). If the gain
   were a coordination convention, one seat would buy far less than a third; it buys 43% and 60%.
   **That is direct evidence the gain is largely individual rather than conventional**, and it is the
   single most reassuring number in this table.

**The defect the table exposed.** `power.mirror` in `match --json` was computed as `specA == specB`
and **ignored the partner specs**, so `--a=v06 --partners=v03 --b=v06` — a one-seat deviation column
running at **31.7%** — was flagged a mirror and printed its deal-clustered interval as **[0, 0]**.
`v07_power.hpp`'s own comment says a mirror cell "carries NO information … the effective sample is
zero", so any consumer that skipped a mirror cell on that basis silently dropped a real measurement.
Fixed at `main.cpp:70,166`: a cell is a mirror only if `specA == specB` **and**
`partnersA == partnersB`. The v0.6 mirror digest is unchanged by the fix, so no play is affected, and
the artifacts in this section were written by the pre-fix binary and re-reduced with the corrected
predicate. Phase 2's `P12-partners.jsonl` carries the same mislabelling and should be re-read.

## 4.8 Cross-play between independently-trained runs

The frozen configuration is **not a fit**, so two independently-trained runs of it do not exist and
had to be produced. Three were: the same architecture — every structural key of the freeze held fixed
— with only the 55-coordinate vector free, fitted by v0.6's own recipe (`obj=minimaxregret`, paired,
panel `v05+v03+withholder+feint`), and made independent on three axes at once: **a different CEM
trajectory**, **a disjoint fitting bank** (7060001/2/3, registered one per search), and — for the
third run — **a different starting basin** (the v0.5 defaults rather than the incumbent). `sigmarel`
was set to 0.12 against v0.6's 0.04 *on purpose*: a cross-play test is only informative if the runs
actually separate, so the separation was bought and is then reported.

They did separate. Over 55 coordinates the pairwise distances are **L2 7.08 to 11.25, L∞ 2.54 to
5.70**, and head to head `xp1` against `xp2` is −0.53 [−1.41, +0.35] — different policies of
indistinguishable strength, which is exactly the condition under which a cross-play test means
something.

#### Cross-play between independently-trained runs of the same architecture

Row = the seat-0 run, column = the run its two partners come from, opponent = three copies
of `v05`. The diagonal is self-play. A convention private to one run shows up as the
off-diagonal collapsing relative to the diagonal.

| seat 0 \ partners | `p4-xp1` | `p4-xp2` | `p4-xp3` | diagonal - mean off-diagonal |
|---|---:|---:|---:|---:|
| `p4-xp1` | +4.28 [+3.66, +4.91] | +4.16 [+3.53, +4.79] | +4.09 [+3.46, +4.72] | **+0.16** |
| `p4-xp2` | +4.69 [+4.06, +5.31] | +5.27 [+4.64, +5.90] | +3.67 [+3.04, +4.29] | **+1.09** |
| `p4-xp3` | +3.85 [+3.23, +4.48] | +4.83 [+4.20, +5.46] | +3.88 [+3.25, +4.51] | **-0.46** |

**Self-play +4.48 over 3 diagonal cells; cross-play +4.22 over 6 off-diagonal cells; the gap is
+0.26 points** against a per-cell half-width of about 0.63. The Hanabi line reports
self-play-to-cross-play collapses of 23.97 to 2.52 (SAD) and 24.04 to 0.12 (IPPO); this is
not that, and the runs are genuinely different policies -- see the distances below.

Head to head, so "these are different policies" is measured rather than assumed:

| pair | edge | 95% CI |
|---|---:|---|
| `p4-xp1` vs `p4-xp2` | -0.63 | [-1.25, -0.01] |
| `p4-xp1` vs `p4-xp3` | +0.95 | [+0.32, +1.59] |
| `p4-xp2` vs `p4-xp3` | +1.40 | [+0.77, +2.03] |

Parameter distance between the runs, so the table above can be read:

| pair | L2 | L-inf | coordinates |
|---|---:|---:|---:|
| `xp1` vs `xp2` | 10.440 | 5.698 | 55 |
| `xp1` vs `xp3` | 7.082 | 2.536 | 55 |
| `xp2` vs `xp3` | 11.247 | 4.141 | 55 |

**This is the strongest single result in phase 4 and it is a negative one.** The Hanabi line the
threat model cites reports self-play-to-cross-play collapses that are not subtle — SAD 23.97 → 2.52
at 10,000 games per pair, IPPO 24.04 ± 0.02 → 0.12 ± 0.03 median. Nothing of that kind is present
here. Read with §4.7's one-seat result — a single upgraded seat buys 43–60% of the three-seat gain —
the two together say the v0.7 advantage is **an individual policy improvement and not a convention**,
which is what a 55-coordinate linear score has far less room to hide than a neural policy does.

**What would have to be true for this to be wrong.** That six generations at population 12 is too
small a fit for a convention to form in — plausible, and it is the honest limit of the result. The
runs are independent and they are *weak*; a convention that only appears at v0.6's own 14-generation
budget would not be visible here. The falsification is affordable and is not run: refit at the full
budget at two seeds and repeat the matrix.

## 4.9 The adversary re-search against the improved policy

The phase-4 brief: *"after each fix, re-run adversary search against the improved policy and check
whether the weakness closed or merely moved."* Eight independent searches, target the **frozen
configuration** and not `v06`, one registered fitting bank each (7060011–7060018, disjoint from the
evaluation banks and from each other), each evaluated on both evaluation banks at 24,000 games. The
axes are the ones phase 2 established are genuinely different searches — class, objective, starting
basin, step size — and the objectives are pointed at the **mechanisms** phase 2 named rather than at
reproducing its adversaries.

#### A fresh adversary search against the improved policy

The target is the v0.7 candidate, not `v06`, and the objective axis is aimed at the
MECHANISMS phase 2 named rather than at reproducing its adversaries. A positive edge is
an exploit; the class detection floor is **1.53** and nothing below it is an exploit.

| id | class | objective | hypothesis | adversary edge | 95% CI | per bank | clears 1.53? |
|---|---|---|---|---:|---|---|:--:|
| `Y01` | C1-inclass | `win` | in-class control against the new target: does the C1 | **-4.41** | [-5.04, -3.77] | -4.35 / -4.47 | no |
| `Y02` | C2-extended | `win` | the extended class, the one that found phase 2's str | **-3.11** | [-3.74, -2.49] | -3.27 / -2.95 | no |
| `Y03` | C1-declerr | `declerr` | A3/L1: drive the target's misdeclaration now that ur | **-12.36** | [-12.97, -11.74] | -12.52 / -12.19 | no |
| `Y04` | C1-events | `events` | A4: lengthen the game -- the cliff is gone, is the s | **-18.22** | [-18.81, -17.64] | -18.28 / -18.16 | no |
| `Y05` | C1-forced | `forced` | K2 raised forced-endgame incidence six-fold by delet | **-7.40** | [-8.02, -6.78] | -7.38 / -7.42 | no |
| `Y06` | C2-wide | `win` | a wider step: is the CEM trapped near the incumbent  | **-3.87** | [-4.51, -3.24] | -3.94 / -3.80 | no |
| `Y07` | C1-basin | `win` | a different starting basin: the v0.5 defaults, not t | **-2.85** | [-3.48, -2.21] | -2.80 / -2.89 | no |
| `Y08` | C2-asksupp | `asksupp` | L10: suppress the target's ask hit rate through the  | **-21.17** | [-21.74, -20.60] | -21.55 / -20.78 | no |

**Nothing exploits it.** The best of the eight is −2.85 and the closest to parity is −3.11; every one
loses by more than 2.8 points, and the class detection floor is 1.53 in the other direction. That is
a stronger statement than phase 2 could make about `v06`, whose best in-class exploiter reached
**+0.79 [+0.48, +1.10]** over 96,000 games.

**And the two mechanism searches answer the brief's actual question, which is not about win rates.**

**A3, the declaration channel — the weakness moved and shrank.** `Y03` is fitted to drive the
target's misdeclaration rate and it does: the frozen configuration's declaration accuracy falls from
**0.970** (under the two `win`-objective adversaries) to **0.9335 / 0.9349**, a drop of 3.6 points of
accuracy. It pays **−12.4 win-rate points** to do it. The channel is real, it is still drivable, and
driving it is not an exploit — which is exactly the shape phase 2 measured against `v06`, where the
same cluster's whole ceiling was +0.38 [−0.06, +0.81].

**A4, the fifteen-point cliff — the weakness closed, and did not move.** This is the one that
mattered, because the freeze *disables v0.5's termination guarantee* (`force=1000000`) and replaces
it with a stall detector, so an adversary that can lengthen games is attacking a new mechanism rather
than an old one. `Y04` is fitted for exactly that and it works: it drives events per game from ~97.4
to **108.8**, an 11.7% increase, at a cost of −18.2 points. Measured on the tail rather than the
mean, over 800 games:

| | events/game | p90 | p99 | **max** | declarations at or after event 220 | action-limit games |
|---|---:|---:|---:|---:|---:|---:|
| frozen v0.7 vs `Y04` | 108.996 | 124 | 135 | **144** | **0** | 0 |
| `v06` vs `Y04` | 106.626 | 120 | 135 | **144** | **0** | 0 |

**The longest game the game-lengthening adversary can produce against the frozen configuration is 144
events, against a 220 rung** — and the frozen configuration does not have that rung, because
`force=1000000` removes it. Phase 2's figure for the same question against the incumbent was 149
events. So the adversary gains nothing from the removal of the clock, the stall rung at 12
consecutive no-progress events is not approached (ordinary play never exceeds 6), and the
fifteen-point cliff that ADVERSARIES A4 identified is **unreachable in the frozen configuration by
construction rather than by luck**.

One number from that table belongs in the record even though it is not a gate result: against `Y04`
the frozen configuration plays **0.112% provably-dead asks**, against 0.058% in its own mirror and a
gate threshold of 0.10%. The gate is specified on the mirror, so this is not a gate failure — but it
is the first configuration in this cycle whose dead-ask rate against an adversary exceeds the mirror
threshold, and phase 5 should report the adversarial dead-ask rate alongside the mirror one.

## 4.11 What phase 5 inherits

**Instruments built or repaired this phase.**

* `engine/gate_v07.sh` — the commit gate as a script rather than a habit. Seven rules, each printing
  the measured configurations its threshold is set from, plus `v7side` on both banks with **S6 in its
  own process at one thread**. Exits non-zero on failure; one JSON verdict per configuration.
* `engine/freeze_config_v07.py` — the first freeze in this corpus that **executes** its round-trip
  assertion instead of printing it, with `--verify-only` for phase 5's B0.3.
* `engine/build_p4_v07.py` — reduces every phase-4 artifact to markdown, so no number in §4 is
  hand-typed.
* `engine/p4_*.sh` — the strength lattice, the partner table, the cross-play fits, the adversary
  re-search and the rule-dialect table, each parameterised by `p4_specs.sh`.
* Two engine fixes: `power.mirror` now accounts for the partner specs (`main.cpp:70,166`), and the
  phase-4 fitting banks are in the seed registry (`v07_seeds.hpp`), one per independent search.
* The five phase-3 worktrees are merged, so `stall`, `jalloc`, the self-oriented KPIs, the search
  capture channel and `v7leaffit` all exist in `main` for the first time.

**Facts phase 5 should treat as established on training material.**

1. The frozen configuration beats `F-cheap` by **+3.18 [+2.55, +3.81]** — the first pooled interval
   in this cycle whose lower bound clears the 1.53 detection floor.
2. Phase 3's `+1.42` over the phase-2 composite replicates at **+0.78 [+0.33, +1.22]** at four times
   the power on two banks. The sign holds; the magnitude halves.
3. **`r12=25` carries the gain** (leave-one-out +2.10, replicated), and it is a phase-2 discovery.
   The search is worth +0.77 at the margin. The three K3 keys are worth +0.78 **as a group** and none
   of them individually.
4. `urgency-off` and `r12=25` are **substitutes**: +1.43 alone, +0.02 at the margin. The six
   components compose at **63%** of their naive sum.
5. `stall=12` is **bit-identical** to its absence at 24,000 games on two banks.
6. **No searching configuration in this corpus is S6-certified**, `v06`'s own frontier included; the
   rate is of order one per million ask decisions and blueprint play is exactly zero.
7. A **one-seat** upgrade to the frozen configuration is worth 43–60% of the three-seat upgrade,
   which is direct evidence the gain is largely individual rather than conventional.

## 4.12 What did not get done, and what was cut

Stated plainly rather than smoothed over, because it bears on how §4's claims should be read.

* **The rule-dialect table was written and not run.** `engine/p4_dialects_v07.sh` exists, unbundles
  `--legacy` into its three isolable components, and adds the three axes the corpus has never swept
  (`--arb=high`, `--arb=turn`, `--sets=8`). It is preregistered as phase-5 cell B8 and it has no
  training-bank counterpart, so phase 5's dialect result will be the first of its kind and has
  nothing to be checked against.
* **The panel was not re-run for the frozen configuration.** CANDIDATES §8 profiles two arms;
  §8.1 says the two that matter — `K3-on-composite` and `P2-composite` — are phase 4's. They were
  not run, because the machine went to the attribution lattice, the partner table and the adversary
  re-search instead, and those are what the phase-4 brief names. **Worst case and minimax regret for
  the frozen configuration are therefore a phase-5 number** (cell B3) and this phase has no estimate
  of them. That is the largest single gap in phase 4.
* **The S6 residual is localised and not explained.** Phase 4 established what it is not — not
  inter-test leakage, not thread isolation, not the rollout blueprints' accumulated observations, not
  the determinization count — and did not establish what it is. The next experiment is named in §4.3
  and was not run: capture the diverging decision's candidate scores in both the live run and the
  replay and compare them.
* **The one-seat deviation column was run only for the partner table**, not as a full
  `--partnersb` one-seat exploitability column in the threat model's T2 sense.
* **The v0.7 architecture was refitted only at a small budget** for the cross-play runs
  (6 generations × 12 population × 150 deals against a four-member panel). Those runs exist to be
  *independent*, not to be strong, and they should not be read as an attempt to improve the frozen
  vector. No fit of any kind was applied to the frozen configuration.
