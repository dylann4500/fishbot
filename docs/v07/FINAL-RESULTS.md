# FishBot v0.7 — PHASE 5: the frozen final evaluation

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`.

**The run is identified by its binary, not by a commit.** Five commits landed in the repository
while the battery was running, so no single commit describes the tree for the whole of it. Every
one of the 428 scored cells was played by `engine/fish7`, SHA-256
`cf6d5ea2c1f0e9e3896b…`, 1,242,312 bytes, built 2026-08-25 13:19 from the tree at `d920f5b`, and
that binary was never rebuilt between the first bank digest and the last cell. The five later
commits (`b8cb227`, `2e829c2`, `46f3515`, `72fa936`, `e0da8fd`) are all in the interactive
browser-table surface. §15 D20 records this, and §1 records the executed check that the frozen
policy is unmoved by them.
Protocol: `docs/v07/PREREGISTRATION.md`, committed before any holdout bank had been played and
before the sealed adversary half had been decoded.

**This document is a recording.** Every cell the preregistration names is reported, including the
ones that failed and the ones that came out flat, with an interval on every number. The prespecified
thresholds are applied as written. Nothing here is an interpretation of what the numbers mean for the
next version of the policy; that is phase 6's job and this document deliberately does not do it.

Phase 5 read `docs/v07/PREREGISTRATION.md` and nothing else from the v0.7 document set. It did not
read `CANDIDATES.md`, `ADVERSARIES.md`, `RESEARCH-LOG.md`, or any phase-4 training log. Where a
phase-4 measurement is quoted below for comparison it is quoted *from the preregistration itself*,
which states its training expectations in advance for exactly this purpose.

Raw output: `research/v07/results/P5-*`, digested in `research/v07/results/MANIFEST-P5.json`.
Reduction: `engine/p5_analyse.py` → `research/v07/results/P5-TABLES.txt`.

---

## 0. How to read every number in this document

| quantity | how it is computed |
|---|---|
| **edge** | a cell's win-rate advantage in points: `100 × (winRateA − 0.5)`. A cell is a paired duplicate: the same deal bank, the same deal indices, `--rotations=2` so each deal is played with the teams swapped, so the deal's intrinsic luck cancels. |
| **interval** | the **deal-clustered bootstrap** `ci` emitted by `match --json` (20,000 resamples over deals, `arena.hpp:293`). `wilsonCI` is never quoted: the rotations of one deal are one correlated unit, and the preregistration's §3 correction 2 requires the clustered interval. |
| **se** | read back off that interval as `(hi − lo) / (2 × 1.96)`. |
| **pooled** | two banks of equal size: the mean of the two edges, with `se_pooled = √(se₁² + se₂²)/2` and a 95% interval at ±1.96·se_pooled. Both per-bank values are printed beside every pooled figure. |
| **delta** | a difference between two *pooled* cells: the difference of the estimates with the two pooled half-widths combined **in quadrature**. §5.2 of the preregistration fixes this arithmetic and calls it conservative, because the arms are paired on deals and the harness gives no paired delta across two separate cells. |
| **replication** | a claim whose **sign** does not agree on both banks is reported as NOT REPLICATED whatever its pooled interval says (§3). |
| **threads** | every scored cell ran at `--threads=13`, which §5.3(2) fixes for the preregistered batteries. The S6 gate condition runs at `--threads=1 --freshagents`. Both are stated with every cell that carries them. |

Two floors, and they are different objects, as §3 note 3 insists:

* **1.53** is the phase-2 C1-class detection floor. Applied to an *exploitability* number (B4, S3, B9.2, B9.3) it is a detection floor in its original sense. Applied in §5.1 to a *directly measured paired duplicate win-rate difference* it is a **fixed pre-committed reference bar** and nothing more — no responder, no fit and no maximisation are involved in that quantity, and the preregistration does not claim a responder-recovery floor bounds it.
* **2.13** is the declaration-family floor, and it applies to exactly two things: B4's `Z03` and the declaration-accuracy column of B2.

Neither buys down with games.

---

## 1. B0 — verification, before anything was measured

`FISH_UNSEAL_PHASE=5` was set once, at the top of the battery, and used for nothing else.
Binary `engine/fish7`, SHA-256 `cf6d5ea2c1f0e9e3…`, 1,242,312 bytes.

### B0.1 — the seven bank digests

`fish7 bankdigest --seed=S --deals=24000`, which folds the six dealt hands and the dealer of every
deal into a 64-bit rolling hash without constructing a policy or playing a game.

| bank | deals | digest reproduced | committed | role |
|---|---:|---|---|---|
| 7090001 | 24,000 | `896dbc89be124d85` | ✓ | primary holdout |
| 7090002 | 24,000 | `0b6e40d834ac0ca1` | ✓ | second disjoint bank |
| 7090003 | 24,000 | `863bea69baf6e73c` | ✓ | ablation lattice and dialect table |
| 7090004 | 24,000 | `54f257c3f8ae9fab` | ✓ | fresh adversary search |
| 7090005 | 24,000 | `268a1dae71a31713` | ✓ | negative controls |
| 7091001 | 24,000 | `958ada042cc26900` | ✓ | sealed adversary evaluation bank |
| 7091002 | 24,000 | `5c39af3b5e0bd9a0` | ✓ | sealed adversary fitting bank |

**All seven match.** This is also a cross-binary check that was not asked for and is worth one
sentence: `engine/seal_banks_v07.py` computed the committed digests with `engine/fish7b`, a
different build from the `engine/fish7` used here, so the agreement is between two binaries as well
as between two dates.

### B0.2 — the sealed adversary half

`research/v07/banks/holdout/adversaries-holdout.sealed` decodes to **14 rows**, plaintext SHA-256
`1ca0346a332586c70a750f1523b105485322af34ff31aab3b9e77a2f0a3b6c52`, **matching `SEAL.json`**, which
records the seal at commit `f4581da` (phase 2). The 14 are 14 distinct spec strings:

`R-v04`, `S-archetype-0`, `S-ask-1`, `S-ask-3`, `S-decl-0`, `S-invert-0`, `S-reference-0`,
`S-search-1`, `X01`, `X01xC3f`, `X02`, `X13`, `X18`, `X20`.

Three of them are policies the rest of the panel already carries (`R-v04` is `v04`,
`S-archetype-0` is `feint`, `S-reference-0` is `v06`). They are run anyway, because the
preregistration fixes the panel by name, and their agreement with the corresponding scripted or
frontier cell is used below as a free determinism check.

### B0.3 — the freeze artifact

`engine/freeze_config_v07.py --verify-only` → **VERIFY PASS**, exit 0.

* spec rebuilt from the JSON's base and ordered option map is character-identical to the artifact's
  own `spec` field. Phase 5 used the reconstructed string everywhere and never typed the spec:
  `v07:r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26`
* R2a blueprint round-trip PASS; 55 coordinates.
* R3 mirror digest `5f81f440fc9c272a87e87c05fecc7b74`, matching §1's table.
* The R2a comparison **with the search on** — the one the preregistration says is measured and
  printed rather than asserted — is recorded in the artifact as identical over 800 games
  (`setsA 4.5 / setsB 4.5`, `askAcc 0.504843 / 0.504843`).
* **No NOTE printed — and that is a WEAKENING, not a clean result.** The preregistration expected
  the verify step to list three engine sources changed since the freeze (`arena.hpp`, `main.cpp`,
  `v07_side.hpp`). Zero were listed. The reason is not that nothing changed. `freeze_config_v07.py`
  reads its drift baseline out of `engine/fishbot_v07.json`'s own `provenance.srcSha256_16`, and
  that artifact was **rewritten twice after the freeze commit `0fa4a5f`** — at `d1c6b35` and again
  at `d8c554b` — each rewrite refreshing the baseline to the then-current tree. B0.3 therefore
  compared the tree against a baseline taken from the tree. Measured against the actual freeze
  commit, **five of the 78 sources differ**. The intended check could not fire.

  What is **not** weakened is the policy identity. R1, R2a and the R3 mirror digest are computed by
  *running* the engine, not by comparing file hashes, and all three round-trip. Because the
  hash-based drift check was vacuous, it was replaced with an executed one, **recorded as an
  artifact in `research/v07/results/P5-drift.json`**: the engine was rebuilt from the tree as it
  stands at the end of the battery (fresh binary SHA-256 `3f2a48cc395b5b2f…`) and the mirror digest
  recomputed with it. Both binaries return `5f81f440fc9c272a87e87c05fecc7b74`, identical to the
  frozen artifact. **The source drift does not move the frozen policy, and that is established by
  executing it rather than by hashing it.** The same artifact records the drift measured properly:
  **nine of the 78 sources differ from the freeze commit `0fa4a5f`** — `arena.hpp`, `factory.hpp`,
  `fish.hpp`, `game.hpp`, `human.hpp`, `main.cpp`, `table.hpp`, `tuner.hpp`, `v07_side.hpp` — of
  which four arrived in the mid-battery commits of D20.

### B0.4 — the seed registry

`fish7 seeds --require=7090001,…,7091002` → exit 0, i.e. all seven registered and all seven usable
under `FISH_UNSEAL_PHASE=5`. `--require` prints nothing on success and returns 3 on any unregistered
or still-sealed seed.

One registry violation is reported by the same command and is recorded here because B0.4 says to
record it: **R1 violation on seed 515253**, a v0.6-era collision between `v06/E4 per-style panel`
(eval) and `v06/X1 exploitability responder fit` (fit). It involves no phase-5 bank and predates
this phase.

### B0.5 — the engine self-audit

* `fish7 verify` → **VERIFY PASS**: 0 audit violations in 6,737,436 checks, 0 set-conservation
  failures, 0 action-limit games, determinism PASS.
* `fish7 selftest` → **SELFTEST PASS**: 424,986 checks, 0 block-build failures, block vs card DP
  max |Δ| 3.414e-15, block vs exact sampling max |Δ| 4.066e-02.

### An extra check, run before the material was spent

The preregistration fixes the thread count at 13 and §5.3 records that a searching configuration's
play depends on the deal partition. `runMatch` schedules deals by **work-stealing over a shared
atomic counter**, so the deal-to-thread assignment is not fixed run to run even at a fixed thread
count. Whether a scored cell is therefore reproducible at all was not established anywhere phase 5
could read, and twelve hours of sealed material was about to be spent on the assumption that it is.
It was checked directly on a training cell (FROZEN vs `v06`, 400 deals × 2 rotations, seed 31):

| condition | winRateA | events/game | ask accuracy |
|---|---|---|---|
| `--threads=13`, run 1 | 0.560000 | 97.305 | 0.531488 |
| `--threads=13`, run 2 | 0.560000 | 97.305 | 0.531488 |
| `--threads=13`, run 3 | 0.560000 | 97.305 | 0.531488 |
| `--threads=1` | 0.560000 | 97.305 | 0.531488 |
| `--threads=2` | 0.560000 | 97.305 | 0.531488 |

Identical, including the bootstrap interval.

**That check was too small, and B3 corrected it.** The panel contains three policies twice, so 24
pairs of byte-identical commands were run twice each. One pair disagreed: `F-cheap` against `feint`
on 7090002, by **one game in 12,000** — 0.0083 points. That is the §5.3 cross-deal agent residue
reaching the *scored* mode, which 400 deals was not large enough to expose. The magnitude is ~76×
below a cell half-width and no verdict in this document turns on it, but the B0 table above should
be read as "reproducible at 400 deals", not "reproducible". §15 D8 and D18 carry both.

---

## 2. B1 — the commit gate, before any strength number

`fish7 pathology --a=SPEC --b=SPEC --games=400 --rotations=2 --seed=31 --threads=13`, plus
`fish7 v7side` on **both training banks** (7030001, 7030002) with S3/S4/S5 at 13 threads and S6 in
its own process at `--threads=1 --freshagents`. The gate runs on the *training* seed by design —
§2.1(b) — because it is a soundness check on self-play behaviour, not a strength claim.

| rule | FROZEN | INCUMBENT | F-cheap | negative control |
|---|---|---|---|---|
| G1 provably-dead asks ≤ 0.10% | **0.05820%** ok | 0.01178% ok | 0.00589% ok | **2.59217% XX** |
| G2 longest dead run ≤ 5 | **1** ok | 1 ok | 1 ok | **326 XX** |
| G3 games with a run ≥ 6 = 0 | **0** ok | 0 ok | 0 ok | **2 XX** |
| G4 action-limit games = 0 | **0** ok | 0 ok | 0 ok | **2 XX** |
| G5 mirror tail max < 220 and p99 ≤ 150 | **max 134, p99 130** ok | max 131, p99 120 ok | max 131, p99 124 ok | **max 405 XX** |
| G6 declarations at/after event 220 = 0 | **0** ok | 0 ok | 0 ok | 0 ok |
| G7a S3/S4/S5 CERTIFIED, zero tolerance | **ok** both banks | ok | ok | ok |
| G7b S6 = 0 at `--threads=1 --freshagents` | **0 / 542,483** ok | 0 / 524,371 ok | 0 / 526,995 ok | 0 / 528,199 ok |
| **verdict** | **PASS** | **PASS** | **PASS** | **FAIL** |

**FROZEN passes all eight rules.** Reported and not gated for FROZEN: events/game 99.805, ask hit
rate 50.716%, misdeclaration 2.55556%.

**B9.1 is satisfied.** The configuration the corpus built to be rejected —
`v06:rtie=1,m1=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1` — fails G1, G2, G3, G4 and G5,
exactly the five the preregistration names. A gate that cannot reject it would certify nothing about
the one it accepts, and it rejects it.

G7b carries the clause §5.3 requires and it is repeated here rather than buried: **the rule tests a
mode no scored cell uses.** `--freshagents` changes play. What G7b certifies is "with the cross-deal
residue removed, nothing else depends on anything it should not". The residue itself is **not**
removed from the shipped mode; it is a named, quantified engine defect that every strength number in
this document is measured under. B10 measures it again on holdout material.

---

## 3. THE HEADLINE — B3: worst case and minimax regret over the shared panel

248 of 248 cells. Four arms — `FROZEN`, `INCUMBENT`, `F-cheap`, the phase-2 composite — each scored
against the **same** 31-member panel on the **same** two banks under the same protocol: the 14
sealed adversaries, the 13 scripted archetypes and the four frontier points. All 31 members carry a
cell for all four arms, which is what makes minimax regret defined at all.

### 3.1 Worst cell per arm

| arm | worst cell | edge (points) | per bank |
|---|---|---|---|
| **`FROZEN`** | composite | **−0.04 [−1.41, +1.33]** | 7090001:+1.67  7090002:−1.75 |
| `INCUMBENT` | `SEALED:X01xC3f` | **−4.24 [−4.86, −3.62]** | 7090001:−3.86  7090002:−4.63 |
| `F-cheap` | composite | **−2.08 [−3.48, −0.69]** | 7090001:−2.21  7090002:−1.96 |
| composite | itself | +0.00 | — |

**S6 passes.** `FROZEN`'s worst cell is −0.04 against the incumbent's −4.24.

Three properties of that cell belong beside the verdict. Its **sign does not replicate**: +1.67 on
7090001 and −1.75 on 7090002, so by §0's own rule the cell is NOT REPLICATED and the −0.04 is a
pooled point estimate straddling zero. It is an **expensive-class** cell — 1,200 deals a bank, 4,800
games, half-width ±1.37 — while `INCUMBENT`'s worst cell is a 6,000-deal near cell resolved to ±0.63,
so the two sides of the comparison are not equally resolved. And B2.4 measures the same pairing at
24,000 games and gets +0.15 [−0.29, +0.59] with both banks positive. The honest reading is that
`FROZEN` is indistinguishable from the composite on this cell, not that it loses to it.

The comparison needs one caveat stated rather than buried: `INCUMBENT`, `F-cheap` and the composite
each meet **themselves** in the panel and so carry a free 0.00 cell, which can only lower their worst
case; `FROZEN` is not a panel member and carries none. Excluding self-cells the composite's worst
becomes +1.23 against `SEALED:X01xC3f` and the other three are unchanged, so S6 passes on that
reading too.

### 3.2 Minimax regret per arm — and `FROZEN` is third of four

| arm | minimax regret | attained against | runner-up |
|---|---:|---|---|
| `F-cheap` | **4.00** | `v03` | |
| composite | 4.08 | `withholder` | |
| **`FROZEN`** | **4.53** | `withholder` | |
| `INCUMBENT` | 5.67 | `SEALED:X01xC3f` | `SEALED:S-invert-0`, 0.004 behind |

The protocol sets no threshold on minimax regret; it requires it to be reported and to lead. It is
reported and it does not favour the frozen configuration. The regret is concentrated in the far
archetypes, where the blueprints beat `FROZEN` outright:

| far archetype | `FROZEN` | `INCUMBENT` | `F-cheap` | composite |
|---|---:|---:|---:|---:|
| `withholder` | **+25.35** | +29.88 | +29.55 | +25.80 |
| `hunter` | **+46.93** | +48.58 | +48.65 | +47.10 |
| `diversifier` | **+43.40** | +44.90 | +45.18 | +43.32 |
| `silent` | **+32.75** | +34.23 | +34.05 | +30.92 |

Against the hardest sealed adversary the ordering reverses: on `SEALED:X01xC3f`, `FROZEN` is
**+1.43 [+0.80, +2.06]** and the composite **+1.23 [+0.60, +1.86]**, where `INCUMBENT` is −4.24 and
`F-cheap` is −1.25. Two of the four arms beat it; `FROZEN` by the larger margin.

`INCUMBENT`'s attaining member is decided by 0.004 points over `SEALED:S-invert-0` against a
half-width of 1.24, and it flips between banks; the regret *value* 5.67 is stable, the name attached
to it is not.

### 3.3 The panel mean, printed last and as a diagnostic

`FROZEN` +14.94, composite +14.35, `F-cheap` +13.28, `INCUMBENT` +11.99, over the 31 shared cells.
The protocol forbids leading with this and it is not led with. It points the opposite way from
minimax regret, and both are in this document because neither on its own is the whole of what the
panel measured.

The full 31 × 4 cell table is in `research/v07/results/P5-TABLES.txt`.

---

## 4. B2 — headline strength against the frontier, and §5.1

10 of 10 cells, paired duplicate, both primary banks.

| cell | opponent | pooled edge | per bank | n |
|---|---|---|---|---:|
| B2.1 | `INCUMBENT` (`v06`) | **+4.63 [+4.19, +5.06]** | 7090001:+5.00  7090002:+4.25 | 48,000 |
| B2.2 | **`F-cheap`** | **+3.33 [+2.88, +3.78]** | 7090001:+3.67  7090002:+2.99 | 48,000 |
| B2.3 | `F-mid` | **+2.89 [+2.00, +3.78]** | 7090001:+3.02  7090002:+2.77 | 12,000 |
| B2.4 | phase-2 composite | **+0.15 [−0.29, +0.59]** | 7090001:+0.13  7090002:+0.16 | 48,000 |
| B2.5 | `v05` | **+5.18 [+4.56, +5.81]** | 7090001:+4.70  7090002:+5.67 | 24,000 |

### 4.1 The primary claim

| condition | result |
|---|---|
| pooled edge over `F-cheap` | +3.33 [+2.88, +3.78] over 48,000 games |
| lower bound above the 1.53 reference bar | **yes** (+2.88) |
| sign replicates on both banks | **yes** |
| `FROZEN` passes B1 | **yes**, read from `P5-gate.jsonl` |
| B0 preconditions | **all pass**, read from `P5-B0.json` |

**VERDICT: CERTIFIED advancement.**

B2.3 **grazes** the restriction the protocol placed on `F-mid` in advance — no claim below 2 points.
Its pooled edge is +2.8917 with a lower bound of **+2.0019**, two thousandths of a point above the
restriction, and the cell's half-width is 0.89, matching §3's `98/√N` table exactly. A claim about
`F-mid` is therefore admissible by the narrowest possible margin, and is stated as such rather than
as a clean clearance.

### 4.2 B2.4 is a failure, and it is one the protocol named in advance

**`FROZEN` does not beat the phase-2 composite: +0.15 [−0.29, +0.59].** Both banks are positive so
the sign replicates, but the interval contains zero. §6 item 2 states the consequence in advance:
what phase 3 called "K3's four keys on top of phase 2's composite, +1.42 [+0.18, +2.68] on one bank"
**did not replicate**, and on this cell v0.7 reduces to the phase-2 composite.

The protocol's own arithmetic makes this cell interpretable: `FROZEN` minus the composite is exactly
the group `rtie=1` + urgency-off + `stall=12` added and `m2=0` removed, and §1 records `m2=0`'s
leave-one-out drop as exactly zero. §1 states that group's training value in advance as **+0.78
[+0.33, +1.22]**. See §12 for what B11 does with that.

**§6 item 2 attaches a mandatory consequence to this outcome, and it is recorded here in the
protocol's own terms.** On this cell **v0.7 reduces to the phase-2 composite — which is a phase-2
result, not a v0.7 one — and the v0.7 contribution is the measurement programme and the four closed
ledger entries, not the policy.** The protocol requires the report to say that, and it says it.
That is not the whole of what this battery measured: `FROZEN` does beat the v0.6 frontier at B2.2
and B2.3, which is a claim the composite cell does not touch. Both stand.

### 4.3 Declaration accuracy — reported, not claimed

§3 correction 4 places "the declaration-accuracy column of B2" under the 2.13 declaration-family
floor. `match --json` emits declaration accuracy as a bare rate with **no interval of any kind**, so
no interval can be quoted for it. It is printed as a diagnostic — `FROZEN` 0.971–0.981 across the
five cells, opponents 0.976–0.985 — and **phase 5 makes no claim about the declaration channel.**
That is what discharges the rule; it is not discharged by clearing a bar.

---

## 5. B4 — a fresh adversary search against the frozen configuration, and S3

Eight independent searches, 8 of 8 fitted, 16 of 16 evaluation cells. Every arm evaluated on banks
disjoint from the stream it was fitted on.

| id | class | base | objective | edge over `FROZEN` | per bank | replicated |
|---|---|---|---|---|---|---|
| Z01 | C1 in-class | `v06` | `win` | −2.91 [−3.55, −2.27] | −2.76 / −3.06 | yes |
| Z02 | C2 extended | `v07` | `win` | −3.90 [−4.52, −3.28] | −3.80 / −4.00 | yes |
| Z03 | C1 | `v06` | `declerr` | −10.70 [−11.32, −10.09] | −10.65 / −10.76 | yes |
| Z04 | C1 | `v06` | `events` | −9.64 [−10.27, −9.01] | −9.83 / −9.46 | yes |
| Z05 | C1 | `v06` | `forced` | −7.14 [−7.76, −6.52] | −7.17 / −7.11 | yes |
| Z06 | C2, wide σ | `v07` | `win` | −5.25 [−5.88, −4.62] | −4.93 / −5.57 | yes |
| Z07 | C1, v0.5 basin | `v06` | `win` | −5.21 [−5.84, −4.58] | −4.85 / −5.58 | yes |
| **Z08** | **C5 white-box** | `v07i` | `win` | **−3.56 [−4.19, −2.92]** | −3.95 / −3.17 | yes |

**Not one arm beats the frozen configuration.** Every one loses, every sign replicates, and the
largest upper bound anywhere is Z01's **−2.27**, which is 3.8 points below the floor.

Z03 is read against the **declaration-family floor 2.13**, not 1.53, because its whole objective is
`--kpi=declerr`. Its upper bound is −10.09, so the distinction changes no verdict here.

**S3: PASS.** The protocol's pass condition ("every arm's upper bound below its floor") and its fail
condition ("any arm clears its floor with a replicated sign") are **not complements**, and there is a
band in which the protocol determines no verdict. Both were computed. The band is **empty** — every
upper bound is below its floor — so the pass condition is met outright and no reading had to be
chosen. Had the band been occupied, phase 5's rule was to report the arm as UNDETERMINED rather than
resolve it by choosing a reading (§15 D10). Whether the non-complementarity is itself a §7 flaw is
adjudicated in §15.1, not here.

Two limits of Z08, stated because it is the cell the phase brief singles out. `v07i` derives from
`V06Agent` and reads only the 37 v0.6 coordinates, while the CEM extends the vector to 55 for any
`v07*` base, so **18 of Z08's fitted coordinates are inert** (§15 D16). And the budget is 8
generations × 12 population, which is a small search. Z08 is evidence that this white-box inverter,
at this budget, does not exploit the frozen configuration — not that no white-box attack can.

The fitting-to-holdout gap is large and in the expected direction: Z01 reached +6.67 points of paired
margin on its own fitting material and lands at −2.91 on disjoint banks. That is the winner's-curse
arithmetic the protocol's disjointness rule exists to expose.

---

## 6. B5 — the attribution lattice, S4, and §6 item 3

24 of 24 cells. Reference opponent `INCUMBENT` throughout, banks 7090003 (primary) and 7090001
(replicate). Reference cell: `FROZEN` vs `INCUMBENT` = **+4.73 [+4.10, +5.36]**.

| component | add-one-in from `v06` | leave-one-out drop from `FROZEN` |
|---|---|---|
| `r12=25` | **+2.18 [+1.55, +2.81]** | **+1.69 [+0.80, +2.57]** |
| the search | +1.91 [+1.38, +2.44] | +0.57 [−0.33, +1.46] |
| `rtie=1` | +0.71 [+0.08, +1.34] | +1.08 [+0.20, +1.97] |
| urgency-off | +1.37 [+1.12, +1.63] | +0.64 [−0.25, +1.53] |
| `stall=12` | **+0.00 [+0.00, +0.00]** | **+0.00 [−0.89, +0.89]** |
| `m2=0` *(not in the freeze)* | +0.77 [+0.66, +0.88] | — |

Naive sum of the **five** components the freeze carries: **+6.18**. Measured whole: **+4.73**.
Sub-additivity ratio **0.766 [0.566, 1.049]** — phase 2's own 0.83 lies inside that interval, so the
two phases' composition is not distinguishable here. **The interval also contains 1.0**, so this
battery does not resolve sub-additivity: the point estimates are sub-additive, which is what S4
tests, and the ratio is not distinguishable from exact additivity at this sample size. Quoting the sum would overstate the
configuration by 1.45 points.

**S4: PASS** — the components are sub-additive and no single leave-one-out drop exceeds the whole.

### 6.1 The three advance predictions of §1

1. **`r12=25` carries the gain.** §1 says the holdout leave-one-out drop must be "roughly 2 points"
   or the attribution is wrong. Measured: +1.69 [+0.80, +2.57] with an add-one-in of +2.18. The
   training figure +2.10 lies inside the interval. **Reproduced.**
2. **`stall=12` fires zero times.** `v06:stall=12` plays **exactly** 50.000% against `v06` on both
   banks, and `FROZEN` minus `stall` matches `FROZEN` to ten decimal places in win rate *and* in
   events per game (98.052100 / 98.052100 and 97.966400 / 97.966400). **Bit-identical, reproduced on
   holdout.** The rung did not fire.
3. **`m2=0` contributes nothing inside the freeze.** It is worth +0.77 alone against `v06` and was
   dropped from the freeze for a zero leave-one-out drop. Both remain true.

### 6.2 §6 item 3 — the location test, which DOES NOT REPLICATE

§6 item 3 makes it a named failure condition if removing every single component in turn costs less
than a third of the whole. Under §3's replication rule this must be read per bank:

| bank | whole | one third | largest single drop | verdict |
|---|---:|---:|---|---|
| 7090001 | 4.883 | 1.628 | `L-r12` +2.350 | **LOCATED** |
| 7090003 *(lattice primary)* | 4.575 | 1.525 | `L-r12` +1.025 | **NOT LOCATED** |
| pooled | 4.729 | 1.576 | `L-r12` +1.687 | LOCATED, by +0.111 |

The two banks' largest drops differ by 1.325 points against a half-width on that difference of
**1.767** — each bank's drop is itself a difference of two single-bank cells, so the difference of
the two drops combines four per-bank errors:
the split is real on the point estimates and is **not itself statistically resolved**. Under §3 —
"a claim whose sign does not replicate across the two banks is reported as not replicated, whatever
its pooled interval says" — **the pooled LOCATED verdict is NOT REPLICATED**, and the honest reading
is that this battery does not settle whether the gain is located by the protocol's one-third rule.
S4 itself passes on both banks; it is §6 item 3 specifically that splits.

---

## 7. B6 — the partner-regime table, and S1

64 of 64 cells. Team A is [ARM, P, P]; team B is three copies of the opponent.

`FROZEN − INCUMBENT`, opponent `v05`:

| partners | `FROZEN − INCUMBENT` | per bank | replicates |
|---|---|---|---|
| itself | +4.49 [+3.61, +5.38] | 7090001:+4.18  7090002:+4.81 | yes |
| `v06` | +1.93 [+1.04, +2.82] | +2.07 / +1.79 | yes |
| `v05` | +0.78 [−0.09, +1.66] | +0.95 / +0.62 | yes |
| `v04` | +1.34 [+0.46, +2.22] | +1.30 / +1.38 | yes |
| `v03` | +1.40 [+0.56, +2.25] | +1.08 / +1.72 | yes |
| `detective` | +1.83 [+0.97, +2.68] | +1.35 / +2.30 | yes |
| **`withholder`** | **−0.19 [−1.04, +0.67]** | +0.00 / −0.38 | **NO** |
| `lockout` | +1.89 [+1.04, +2.74] | +1.39 / +2.39 | yes |

Seven of eight rows positive; six of seven changed-partner rows positive; minimum **−0.19**, above
the −1.0 collapse threshold. The single negative row, `withholder`, is +0.00 on one bank and −0.38
on the other and is reported as **not replicated**.

**S1's minimum passes on the point estimate and is not resolved against the threshold.** S1 fails if
any row is below −1.0. The minimum row's interval is [−1.04, +0.67] — its lower bound is *below*
−1.0. The point estimate clears the threshold, the interval does not exclude a value that would
fail it, and this battery does not separate the two.

**S1: PASS on both readings of the row count.** §5.2 says "five of the eight changed-partner rows";
B6 has seven changed settings plus self-play, and the protocol's own worked table counts eight
including self-play. Both counts are given; the verdict uses the eight-row reading the protocol
demonstrates, and the seven-row reading passes as well.

Opponent `v06`, where §1 predicts far more stability: +4.55 (itself), +2.75 (`v06`), +1.46 (`v03`),
+2.34 (`detective`) — all four rows positive and all four clear zero.

**This is where the measurement disagrees with the protocol's advance statement, and §1 says a
disagreement is a finding.** §1 item 4 states in advance "+4.82 self and +2.50 to +2.88 under
partner change, all three clearing zero". All three changed rows do clear zero, but **two of the
three fall below the stated range** — `v03` at +1.46 [+0.63, +2.29], a point below the bottom of it,
and `detective` at +2.34 [+1.49, +3.19] — and the self row is +4.55 against a predicted +4.82. The
direction of the prediction holds: the `v06` regime is more stable than the `v05` regime. Its
magnitude is smaller than phase 4 measured on training material.

§1's one-seat prediction: a single-seat upgrade among `v06` partners is worth **+1.93** against a
`v05` opponent (predicted +1.26) and **+2.75** against a `v06` opponent (predicted +2.88).

### 7.1 The sentence the protocol pre-committed to, and it applies

§5.2 states in advance that if the incumbent-baseline ordering survives the fuller measurement, the
honest sentence is *"v0.7's advantage transfers across partners about as well as v0.6's did, and on
the median-to-self-play ratio slightly less well"* — not "it transfers". The ordering survived:

| comparison | min | median | self | median/self |
|---|---:|---:|---:|---:|
| v0.7 over v0.6 — **holdout** | −0.19 | +1.40 | +4.49 | **0.313** |
| v0.6 over v0.5 — **holdout** | −0.40 | +0.26 | +0.69 | **0.380** |
| v0.7 over v0.6 — training, stated in advance | −0.15 | +1.26 | +2.94 | 0.428 |
| v0.6 over v0.5 — training, stated in advance | −0.61 | +0.64 | +1.35 | 0.472 |

So that sentence stands as the report's. Beside it belongs the fact it omits: v0.7's absolute
changed-partner median is +1.40 against v0.6's +0.26, and the ratio is smaller mainly because the
self-play row it divides by is more than six times larger. Both are true and neither is the headline
alone. This comparison is *reported and not gated*, as §5.2 requires.

---

## 8. B7 — cross-play between independently-trained runs, and S2

24 of 24 cells. Three runs of the frozen architecture, independently trained on disjoint fitting
banks with different CEM trajectories and, for one, a different starting basin.

Pairwise parameter distance over 55 coordinates: **xp1–xp2 L2 9.638**, **xp1–xp3 6.341**,
**xp2–xp3 9.554** — reproducing the protocol's committed "6.3–9.6".

| a \ partners | xp1 | xp2 | xp3 |
|---|---|---|---|
| **xp1** | +4.54 [+3.91, +5.17] | +3.80 [+3.17, +4.42] | +4.55 [+3.92, +5.19] |
| **xp2** | +4.93 [+4.29, +5.56] | +4.91 [+4.29, +5.54] | +4.83 [+4.20, +5.46] |
| **xp3** | +5.15 [+4.52, +5.78] | +4.06 [+3.44, +4.69] | +4.20 [+3.56, +4.83] |

Diagonal mean **+4.55**; off-diagonal mean **+4.55**; **gap −0.01** against a per-cell half-width of
0.63. Phase 4 measured +4.51 / +4.48 with a gap of 0.02 on training material.

**S2: PASS.** The worst off-diagonal cell (+3.80) is 0.40 below the weakest diagonal cell (xp3–xp3, +4.20) and
0.75 below the diagonal mean, and the
best off-diagonal (+5.15) exceeds every diagonal cell. For scale, the protocol cites the Hanabi
line's collapses as the thing to look for — SAD 23.97 → 2.52, IPPO 24.04 → 0.12. Nothing of that
kind is present.

Head to head, so that "these are different policies" is measured rather than assumed: xp1 vs xp2
+0.20 [−0.43, +0.83]; xp1 vs xp3 +0.70 [+0.08, +1.32]; xp2 vs xp3 +1.05 [+0.43, +1.68]. Two of the
three pairs separate; the third does not.

---

## 9. B8 — the rule-dialect table, and S5

16 of 16 cells. Both arms play the same dialect in every row.

| dialect | edge | per bank | vs default |
|---|---|---|---:|
| default | +4.73 [+4.10, +5.36] | 7090003:+4.57  7090001:+4.88 | — |
| `--no-out-of-turn` | +5.25 [+4.63, +5.87] | +4.72 / +5.78 | +0.52 |
| `--no-cardless-declare` | +4.90 [+4.27, +5.52] | +4.71 / +5.08 | +0.17 |
| `--maxasks=360` | +4.73 [+4.10, +5.36] | +4.57 / +4.88 | **+0.00** |
| `--arb=high` | +4.85 [+4.21, +5.49] | +5.03 / +4.67 | +0.12 |
| `--arb=turn` | +5.51 [+4.88, +6.14] | +5.26 / +5.77 | +0.78 |
| `--sets=8` | +4.13 [+3.52, +4.73] | +3.77 / +4.48 | −0.60 |
| `--legacy` | +5.12 [+4.50, +5.75] | +4.62 / +5.63 | +0.40 |

**S5: PASS.** Every row keeps its sign, every row replicates, and the largest excursion from default
is +0.78 — inside the 2-point tolerance. The edge survives 48-card Literature, all three arbitration
orders, both declaration-permission changes and the full v0.3 legacy dialect.

`--maxasks=360` is **bit-identical to default**: the 400-ask cap is never reached in this population.

The **legacy residual** — legacy minus the isolable components, which the protocol assigns to the
forced-endgame willingness ladder because it has no CLI flag of its own — is **−0.292 [−1.95,
+1.37]**. The interval is 5.7× the size of the residual, so the data are equally consistent with the
ladder contributing nothing. Note also that since `--maxasks=360` is inert, the residual is
arithmetically legacy minus *two* effective components, not three.

---

## 10. B9 — the negative controls, and S7

### 10.1 B9.1 — the gate must reject

Run inside B1. The configuration the corpus built to be rejected fails **G1, G2, G3, G4 and G5** —
exactly the five the protocol names. See §2.

### 10.2 B9.2 / B9.3 — planted-edge recovery

26 of 26 cells. The **recovered excess** is the responder's edge against the handicapped target minus
its edge against the unhandicapped one; both the planted cost and the unhandicapped cell are
additions (§15 D2), because neither "tracks the planted size" nor "resolving *to* zero" is computable
without them.

| planted `hstr` | planted cost | responder vs handicapped | responder vs `FROZEN` | recovered excess |
|---|---|---|---|---|
| **0.05** *(sub-floor)* | +0.78 [+0.25, +1.32] | −3.62 [−4.26, −2.99] | −4.32 [−4.94, −3.69] | **+0.70 [−0.20, +1.59]** |
| 0.08 | +1.59 [+1.05, +2.13] | −2.29 [−2.92, −1.67] | −4.67 [−5.30, −4.05] | **+2.38 [+1.50, +3.26]** |
| 0.11 | +8.52 [+7.93, +9.12] | +2.52 [+1.89, +3.16] | −6.90 [−7.52, −6.28] | **+9.42 [+8.54, +10.31]** |
| 0.15 | +11.44 [+10.85, +12.03] | +7.30 [+6.68, +7.93] | −5.34 [−5.97, −4.71] | **+12.64 [+11.75, +13.53]** |

**B9.2 passes on both clauses**: all three supra-floor rungs are recovered above 1.53, and the
recovered size tracks the planted size monotonically. The ladder is sharply non-linear, with most of
the damage arriving between 0.08 and 0.11.

**B9.3 passes**: the sub-floor rung is **not** recovered — +0.70, below 1.53, with an interval
containing zero. Two qualifications belong beside it. Phase 2 measured this excess at −0.01 [−0.45,
+0.42] and phase 5 gets +0.70: the same conclusion from a point estimate 0.71 higher. And its upper
bound, +1.59, *grazes* the 1.53 floor. The verdict is unchanged — the test is whether the edge is
recovered, and it is not — but the margin is thinner than phase 2's. The directly measured planted
cost, +0.78, reproduces phase 2's +0.88 rung closely.

The fitted responders lose to *unhandicapped* `FROZEN` in all four rows (−4.32 to −6.90),
independently reproducing B4's finding from a different fitting bank and a different objective.

### 10.3 B9.4 — the identity control

`FROZEN` against itself, both banks: `winRateA 0.500000`, `ci [0.500000, 0.500000]`, `power.mirror
true`, n = 12,000 each. **Exactly 50.000% with genuinely zero variance**, including the zero-variance
clause. The §5.3 residue does not disturb it, because the duplicate design makes a true mirror
exactly 50% by construction.

### 10.4 B9.5 — the side-channel positive controls

12 of 12 cells, identical on both banks.

| cheat | S3 | S4 | S5 | S6 | required |
|---|---|---|---|---|---|
| `v07x:cheat=seed` | PASS | **FAIL** | **FAIL** | PASS | fails S4 and S5, passes S3 and S6 ✓ |
| `v07x:cheat=shared` | PASS | PASS | PASS | **FAIL** | fails S6 only ✓ |
| `v07x:cheat=conv` | **FAIL** | PASS | PASS | PASS | fails S3 only ✓ |

**Each planted channel is caught by exactly the test built to catch it and by no other.**

### 10.5 S7

| leg | result |
|---|---|
| B9.2 recovery at or above 1.53 | ok |
| B9.2 recovered size tracks the planted size | ok |
| B9.3 the sub-floor rung is NOT recovered | ok |
| B9.5 the three cheats fail exactly the named tests | ok |
| B9.4 identity control exactly 50.000% with zero variance | ok |

**S7: PASS.** The instrument is the one phases 1 and 2 characterised, so the rest of this battery is
interpretable as measurement.

---

## 11. B10 — the S6 residual on holdout material

16 of 16 cells, 1,200 deals each.

| arm | bank | `--threads=1` | `--threads=1 --freshagents` |
|---|---|---|---|
| `FROZEN` | 7090001 | 0 / 813,512 | **0** / 813,512 |
| `FROZEN` | 7090002 | **3** / 816,049 | **0** / 816,016 |
| `INCUMBENT` | 7090001 | 0 / 791,530 | **0** / 791,530 |
| `INCUMBENT` | 7090002 | 0 / 792,734 | **0** / 792,734 |
| `F-cheap` | 7090001 | **1** / 792,140 | **0** / 792,140 |
| `F-cheap` | 7090002 | **2** / 795,218 | **0** / 795,224 |
| composite | 7090001 | **2** / 814,702 | **0** / 814,726 |
| composite | 7090002 | 0 / 816,225 | **0** / 816,225 |

**The gate column is zero for every arm on both banks.** There is no new S6 finding, and the
headline the protocol reserved for one does not apply.

The first column reproduces phase 4's defect on holdout material and reproduces its shape: every
nonzero count belongs to a **searching** arm, and the blueprint `INCUMBENT` is zero on both banks.
`FROZEN` is zero on 7090001 and 3 on 7090002 — the protocol anticipated exactly this, noting that a
clean run "was not clean by luck; it was a deal partition under which the residue happened not to
bite".

The **denominators move** between the two columns — 816,049 → 816,016 for `FROZEN`, 795,218 →
795,224 for `F-cheap`, 814,702 → 814,726 for the composite. That is the protocol's own point
restated by measurement: `--freshagents` **changes play**, so G7b certifies a mode no scored cell
uses. The residue is not removed from the shipped mode. It is a named, quantified engine defect that
every strength number in this document is measured under, and §15 D18 records where it surfaced.

---

## 12. B11 — the selection-bias check

*K* = 15 configurations were scored against `v06` before the freeze. At the 24,000-game lattice cell
size, σ = 98/2/√24000 = **0.3163** points, and **σ·√(2 ln 15) = 0.74 points** is the expected maximum
under the null that none of the *K* differs from the incumbent.

**How much of the measured gain would be expected from selection alone: 0.00 points.** The holdout
banks were never available for selection — not one of the *K* choices could have been made using any
deal of 7090001–7091002 — so the selection term on holdout is zero by construction, not by
measurement.

The check's third clause asks the other direction: whether the holdout estimate is systematically
smaller than the training estimate by about that amount. The protocol supplies no training figure for
B2.1 or B2.2, but it does state three others in advance:

| quantity | training | holdout | shortfall |
|---|---:|---:|---:|
| **B2.4 — `rtie` + urgency-off + `stall`, as a group** | **+0.78** | **+0.15** | **−0.63** |
| B6 self-play row, opponent `v05` | +2.94 | +4.49 | +1.55 |
| B7 diagonal (self-play) | +4.51 | +4.55 | +0.04 |
| B7 off-diagonal (cross-play) | +4.48 | +4.55 | +0.07 |

Three of the four reproduce or exceed their training values. The one that falls short is **B2.4**,
by **0.63** against an expected-maximum-under-the-null of **0.74** — and B2.4 is the cell that
measures the group of keys the freeze was *selected over*. That is the signature B11 was written to
look for, and it appears where selection happened and not elsewhere. The training interval for that
group was [+0.33, +1.22]; the holdout interval is [−0.29, +0.59].

---

## 13. Throughput — the cost claim, checked

§1 records the configuration as running at "roughly **3.2×** the blueprint" and asks phase 5 to check
rather than repeat it. Every B3 cell already carries `gamesPerSec` at a fixed 13 threads, so the
multiple is read off the panel without spending a game on a separate benchmark.

`gamesPerSec` is **whole-match** throughput and carries the opponent's cost on both sides of the
ratio, so `INCUMBENT_gps / ARM_gps = (t_arm + t_opp)/(t_inc + t_opp)`, strictly below `t_arm/t_inc`.
Every figure below is a **lower bound**, tightest against the cheapest opponents.

| arm | vs `random` (tightest) | far archetypes (median) | whole panel (median) |
|---|---|---|---|
| `FROZEN` | **4.52×** | **4.68×** | 2.76× |
| `F-cheap` | 4.10× | 4.37× | 2.66× |
| composite | 4.20× | 4.52× | 2.72× |

**The recorded 3.2× understates the cost.** `FROZEN` costs at least 4.52× the blueprint, and both
tight bounds exceed 3.2×. The whole-panel column is lower only because the expensive panel members
put a large common term on both sides of the ratio; it is the most diluted of the three.

---

## 14. §6 — what would mean v0.7 is not an advancement, answered condition by condition

| # | condition | result |
|---|---|---|
| 1 | the frontier cell: interval over `F-cheap` contains zero, or the sign does not replicate | **NOT MET.** +3.33 [+2.88, +3.78], sign replicates on both banks |
| 2 | the composite cell: `FROZEN` does not beat the phase-2 composite | **MET.** +0.15 [−0.29, +0.59]. The +1.42 figure phase 3 reported on one bank did not replicate |
| 3 | attribution: no single component's removal costs a third of the whole | **DOES NOT REPLICATE.** LOCATED on 7090001, NOT LOCATED on 7090003, pooled LOCATED by +0.111 |
| 4 | brittleness: present in self-play, absent under changed partners or off the diagonal | **NOT MET.** S1 passes with minimum −0.19; S2's cross-play gap is −0.01 |
| 5 | a fresh exploit: any B4 arm clears 1.53 with a replicated sign | **NOT MET.** Every arm loses; largest upper bound −2.27 |
| 6 | the gate: `FROZEN` fails B1 | **NOT MET.** All eight rules pass |
| 7 | the controls misbehave | **NOT MET.** S7 passes on all five legs |

**Two of the seven conditions are met or unresolved: item 2 outright, and item 3 by non-replication.**
The other five are not met.

### The prespecified verdicts, together

| claim | verdict |
|---|---|
| §5.1 primary claim vs `F-cheap` | **CERTIFIED advancement** |
| S1 no collapse under partner change | **PASS** (both row readings) |
| S2 not a private convention | **PASS** |
| S3 no fresh adversary exploits it | **PASS** |
| S4 the gain is attributable | **PASS** |
| S5 survives the rule dialect | **PASS** |
| S6 worst case not catastrophic | **PASS** |
| S7 the instrument is intact | **PASS** |
| §6 item 2 the composite cell | **FAILED** |
| §6 item 3 the location test | **NOT REPLICATED** |

---

## 15. Deviations, additions and protocol notes  (§7)

§7 requires a deviation for anything phase 5 did that the protocol does not specify, recorded next
to the affected result rather than in a footnote. Every entry below also appears in
`research/v07/results/P5-TABLES.txt` beside its table.

### 15.1 Whether any of this is "a genuine flaw in this protocol" under §7

§7 says that if phase 5 finds a genuine flaw — *a cell that cannot answer the question it is posed,
a threshold that is incoherent, a control that does not control* — it **stops and reports the flaw**
rather than amending and continuing. Four candidates arose. Phase 5 judged none of them a stop, and
records the reasoning so a reader can disagree with it. **No threshold, cell or sample size was
changed at any point; every judgement below is a recorded reading, not an amendment.**

| candidate | §7 category it might fall under | adjudication |
|---|---|---|
| **D19** — B0.3's source-drift NOTE could not fire, because its baseline lives inside the artifact it checks and was refreshed after the freeze | "a control that does not control" | **Not a stop.** The protocol itself calls the changed-source NOTE "a report and not a failure"; the *control* is the R3 digest round-trip, which is computed by running the engine, which fired and passed. A report that cannot fire is not a control that does not control. Phase 5 nonetheless replaced it with an executed check rather than leaving the gap, and recorded that check as an artifact. |
| **D10** — S3's pass and fail conditions are not complements, leaving a band undetermined | "a threshold that is incoherent" | **Not a stop, on this data.** The threshold determines a verdict everywhere except the band, and the band came out **empty**: every arm's upper bound is below its floor, so the pass condition was met by the protocol's own words with no choice by phase 5. A stricter reader could hold that *discovering* the gap is itself the trigger regardless of whether it bites; that reading is recorded here so it can be applied by someone else. |
| **D5** — §4/B4's description of what `tune --seed=<bank> --shard=s/4` plays is factually wrong | "a cell that cannot answer the question it is posed" | **Not a stop.** The cell answers its question: eight mutually disjoint fitting sets, fitting disjoint from the evaluation banks, evaluation at the preregistered size on holdout. The protocol's account of the mechanism is wrong; the cell is not. |
| **D26** — B11's third clause asks for a comparison the protocol supplies no figure for | "a cell that cannot answer the question it is posed" | **Not a stop, but the closest of the four.** B11's first two clauses — the 0.74 expected maximum, and how much of the gain selection would explain — are fully computable and are the substance of the check; both are discharged. The third clause is not computable as written, and phase 5 substituted four quantities the protocol *does* state in advance. That substitution is registered as D26 rather than performed silently. |

The reason none of these forces a stop is the same in each case: §7 exists to stop phase 5 using
holdout knowledge to choose a protocol. Nothing above changed what was measured, how large a cell
was, or what threshold it was read against. Each is a defect in the protocol's *description* or in a
*report*, not in a cell, a threshold that decided anything, or a control that was relied on.

**D1**  ADDED CELL -- B5 REF-FROZEN.  The eleven cells B5 names cannot yield a leave-one-out DROP:
    the drop is FROZEN's edge minus the variant's, and FROZEN's own edge on the lattice banks is
    not one of the eleven.  A twelfth cell was added at the lattice size on both lattice banks.
    B5 therefore cost 288,000 games rather than the preregistered 264,000.

**D2**  ADDED CELLS -- B9 planted cost and responder-vs-unhandicapped.  B9.2 requires that "the
    recovered size tracks the planted size", which needs the planted size; B9.3 speaks of the
    planted excess "resolving TO zero", which is the responder's edge against the handicapped
    target MINUS its edge against the unhandicapped one.  Four rungs x 2 banks x 6,000 deals were
    added for each, 192,000 games in total.

**D3**  UNSPECIFIED BANK -- B8.  Section 2.1 assigns 7090003 the role "the ablation lattice and the
    dialect table" but names no replicate for the dialect rows.  7090001 was used, by symmetry
    with B5, which the protocol does specify as 7090003 primary and 7090001 replicate.

**D4**  UNSPECIFIED BANK -- B3 sealed rows.  Section 2.1 gives 7091001 the role "evaluation bank for
    the sealed adversary half", but section 4/B3 puts every panel member on the same two banks as
    every other arm, and section 3 makes 7090001/7090002 the primary and replicate for every
    claim.  The sealed rows were run on 7090001/7090002 with the rest of the panel, so the whole
    panel is scored on one shared pair of banks, as minimax regret over a shared panel requires.
    7091001 was digest-verified in B0.1 and then not played.

**D5**  PROTOCOL NOTE -- what `tune --seed=<bank> --shard=s/4` actually plays.  Section 4/B4 describes
    the eight searches as drawing "480 deal indices of the bank".  They do not.  `tune` uses
    --seed only as the CEM root: it derives a fresh per-generation seed (tuner.hpp:310) and from
    that a per-opponent match seed (tuner.hpp:258), so the deals are a derived stream and the
    named bank's own deals are never constructed.  Banks 7090004, 7090005 and 7091002 are
    therefore digest-verified and never played as deal banks.  What the protocol RELIES on
    survives intact: --shard still partitions by index congruence, so the eight fitting sets are
    mutually disjoint, and the fitting stream is disjoint from the evaluation banks, which is what
    makes B4 and S3 interpretable.  Because the deals rotate per generation the fitted adversary
    is if anything LESS overfitted to its fitting material than the protocol's account assumes.
    The same applies to B9.2/B9.3's responder fits on 7090005.

**D6**  PROTOCOL NOTE -- the seal is not enforced on every command.  Section 2.1(b) records that
    `pathology`, `v7bits` and `v6probe` bypass `seedUsable`.  `tune` and `ablate` also bypass it,
    for the same reason as D5: the seed `runMatch` sees is derived, not the registered one.
    Phase 5 set FISH_UNSEAL_PHASE=5 once and used the seven registered seeds and no others.

**D7**  OBSERVATION, **SUPERSEDED BY D19** -- B0.3 printed no changed-source NOTE.  The protocol
    expected three engine sources to be listed as changed since the freeze (arena.hpp, main.cpp,
    v07_side.hpp).  Zero differ from the baseline the check actually uses.  D19 explains why that
    baseline is not the freeze's and gives the correct figure -- nine of 78 sources differ from the
    freeze commit.  This entry is left standing rather than deleted, because the first reading of
    B0.3 was reported before it was corrected.

**D8**  ADDED CHECK -- reproducibility, before the material was spent.  `runMatch` schedules deals by
    work-stealing, so the deal-to-thread assignment is not fixed run to run even at a fixed thread
    count, and the frozen configuration carries the cross-deal agent residue of section 5.3.  A
    scored cell was checked to be bit-identical over three runs at 13 threads and across 1, 2 and
    13 threads, on training material, before any holdout bank was played.

**D9**  READING -- S1's row count.  Section 5.2 says "five of the eight changed-partner rows"; B6 has
    seven changed settings plus self-play, and the protocol's own worked table counts eight
    including self-play.  Both counts are printed and the verdict states which reading it used.

**D10** READING -- S3's pass and fail conditions are not complements.  "Every arm's upper bound below
    1.53" and "any arm clears 1.53 with a replicated sign" leave a band in which neither holds.
    Both are computed and reported; if the band is occupied the verdict is reported as
    UNDETERMINED rather than resolved by choosing a reading.

**D11** READING -- the 2.13 declaration-family floor.  Section 3 correction 4 applies it to "B4's Z03,
    and the declaration-accuracy column of B2".  Z03 is read against 2.13.  B2 has no
    declaration-accuracy column with an interval -- `match --json` emits declaration accuracy as a
    bare rate -- so the accuracy is printed as a diagnostic and PHASE 5 MAKES NO CLAIM ABOUT THE
    DECLARATION CHANNEL, which is what discharges the rule.

**D12** ARITHMETIC -- S4's naive sum.  The freeze carries five components; `m2=0` was dropped from it.
    The sum S4 tests is over those five.  The six-term sum including A-m2 is printed beside it and
    labelled, because B5 preregisters the A-m2 cell even though the freeze does not carry the key.

**D13** NOTE -- every delta in this document is two independent intervals combined in quadrature, not
    a paired difference.  The harness gives a paired delta only within one `ablate` invocation
    over a shared panel, and no cell of this battery is of that form.  Section 5.2 fixes this
    arithmetic and calls it conservative.

**D14** NOTE -- `ablate` was not used anywhere.  It derives its own per-opponent seeds by
    `mixSeed(seed, i*7919+3)` (main.cpp:589), so `ablate --seed=7090003` would not play bank
    7090003.  Every lattice cell is a `match` cell on the named bank instead.

**D15** NOTE -- the panel of 31 NAMED members contains three policies twice: R-v04 is v04,
    S-archetype-0 is feint, S-reference-0 is v06.  The duplicated cells are run as named and used
    as a determinism check.

**D16** NOTE -- Z08 fits 55 coordinates of which 18 are inert.  The protocol specifies --base=v07i for
    the white-box class; `v07i` derives from V06Agent and reads only the 37 v0.6 coordinates
    (factory.hpp), while the CEM extends the vector to 55 for any v07* base.  The search is over
    37 live coordinates plus the inverter the base fixes.

**D17** NOTE -- thread counts.  Every scored cell ran at --threads=13, which section 5.3(2) fixes for
    the preregistered batteries.  The S6 gate condition ran at --threads=1 --freshagents, and the
    S6 residual column at --threads=1 alone.

**D18** CORRECTION TO D8 -- a scored cell is NOT bit-reproducible for a searching arm at panel
    sizes.  The B0 check found five runs of a 400-deal cell bit-identical.  The panel-duplicate
    check, on 1,500- and 6,000-deal cells, found one pair of identical commands disagreeing by
    one game in 12,000 -- the cross-deal agent residue of section 5.3 reaching the scored mode.
    The magnitude is 0.008 points against a 0.63-point half-width.  Reported in full under
    "Panel duplicates" above.

**D19** CORRECTION TO D7 -- B0.3's changed-source NOTE could not fire, and reporting zero as
    "conservative" was wrong.  `freeze_config_v07.py --verify-only` reads its drift baseline out
    of engine/fishbot_v07.json's own `provenance.srcSha256_16`.  That artifact was REWRITTEN
    after the freeze commit 0fa4a5f -- at d1c6b35 and again at d8c554b -- and each rewrite
    refreshes the baseline to the then-current tree.  B0.3 therefore compared the tree against a
    baseline taken from the tree, and the three sources the protocol predicted (arena.hpp,
    main.cpp, v07_side.hpp) no longer differ from it.  Against the actual freeze commit, five of
    the 78 sources differ.  What is NOT weakened is the policy identity itself: R1, R2a and the
    R3 mirror digest 5f81f440fc9c272a87e87c05fecc7b74 all round-trip, and those are computed by
    RUNNING the engine, not by comparing hashes.  Because the hash-based drift check could not
    fire, it was replaced by an EXECUTED one: the engine was rebuilt from the tree as it stands
    at the end of the battery and the R3 mirror digest recomputed with the fresh binary.  It is
    5f81f440fc9c272a87e87c05fecc7b74 -- identical to the frozen artifact and to the binary that
    played every cell.  The source drift does not move the frozen policy, and that is now
    established by running it rather than by comparing file hashes.

**D20** DISCLOSURE -- five commits landed in the repository DURING the battery, and four engine
    sources now differ even from the refreshed baseline: fish.hpp, game.hpp, human.hpp,
    table.hpp, from the "web play" commits b8cb227, 2e829c2, 46f3515, 72fa936 and e0da8fd.  They
    touch the interactive browser table, not the policy.  The decisive fact is that engine/fish7
    was NEVER REBUILT: sha256 cf6d5ea2c1f0e9e3896b..., mtime 2026-08-25 13:19, identical at the
    start of B0 and at the end of the battery.  ONE BINARY PLAYED EVERY CELL.  The results are
    therefore identified in this document by that binary hash and not by a commit, because no
    single commit describes the tree for the whole run.

**D21** ORDERING -- B0 was performed before anything else, but its ARTIFACT was written later.  The
    five B0 checks were run interactively at the top of the session, before the commit gate and
    before any holdout deal was played.  engine/p5_b0.py, which re-runs all five and records them
    as P5-B0.json, was written and run about fifteen minutes into B2.  Every B0 check is
    deterministic and reproduced identically, so the artifact records what was checked first --
    but its file timestamps do not show that ordering and this note is what does.

**D22** OBSERVATION -- one action-limit hit in 428 scored cells: the phase-2 composite against
    SEALED:S-ask-1 on bank 7090001, one game in 12,000 (limitHitRate 8.33e-05).  G4 gates the mirror
    pathology run and not scored cells, so this is not a gate violation, but the corpus standard is
    zero and it is recorded.  NOTE that scored cells carry NO audit evidence: `auditChecks` is 0 in
    all 428 of them, because `match` runs the audit only under `--audit` and no preregistered cell
    uses it.  "Zero audit violations" over a scored cell is zero out of zero and means nothing; the
    engine-wide figure of 0 violations in 6,737,436 checks comes from B0.5's `verify` run.

**D23** CORRECTION -- the throughput columns were printed with median_high (`v[len(v)//2]` on an
    even-length list) rather than the median.  Corrected to the true median; every figure moves by
    at most 0.11x and every one still exceeds the 3.2x the protocol records.

**D25** READING -- section 6 item 3 read per bank.  The protocol states the one-third location rule
    without a per-bank clause, and section 3's replication rule is written for "a claim whose SIGN
    does not replicate".  Here the sign of the drop does not change between banks; the VERDICT LABEL
    does -- LOCATED on 7090001, NOT LOCATED on 7090003.  Applying section 3's rule to a threshold
    crossing rather than to a sign is an extension, and section 7 requires a deviation for "a
    threshold it read differently".  The pooled figure alone would have read LOCATED; both readings
    are printed so either can be applied.

**D26** SUBSTITUTION -- B11's third clause is not computable as written.  It asks phase 5 to confirm
    "that the holdout estimate is not systematically smaller than the training estimate by about
    that amount", where the estimate B11 is about is the measured holdout gain -- but the protocol
    supplies no training figure for B2.1 or B2.2.  Phase 5 substituted the three quantities the
    protocol DOES state in advance (the B2.4 group at +0.78, the B6 self-play row at +2.94, and B7's
    +4.51 / +4.48) and labelled the substitution in place.  B11's other two clauses are computable
    and are discharged exactly as written.  See 15.1.

**D27** ADDED ARTIFACT -- research/v07/results/P5-drift.json.  D19's executed replacement for the
    vacuous hash-based drift check was performed but not initially recorded, which made it an
    assertion rather than evidence.  It is now an artifact: the build command, both binaries' SHA-256
    and byte counts, the pathology argv, the two recomputed digests, and the source sets differing
    from the artifact's own baseline and from the freeze commit.

**D24** CORRECTION -- per-bank columns were printed unlabelled and sorted by seed NUMBER, which
    inverts the attribution for B5 and B8, whose primary bank is 7090003 and whose replicate is
    7090001.  Every per-bank column in P5-TABLES.txt is now printed as `seed:value`.  In THIS
    document the bank order is given in each table's first row and the pairs that follow keep it:
    section 5's B4 table is (7090001, 7090002) and section 9's B8 table is (7090003, 7090001).  A
    reader comparing the two must not assume a common order.

---

## 16. What the battery cost, and where the raw output is

| battery | preregistered | added | measured | games |
|---|---:|---:|---:|---:|
| B1 gate + B9.1 | 4 configurations | — | 4 | 3,200 mirror + 8 `v7side` runs |
| B2 frontier | 10 | — | 10 | 180,000 |
| B3 panel | 248 | — | 248 | 2,102,400 |
| B4 fresh adversary search | 8 fits + 16 | — | 8 + 16 | 188,160 fitting + 192,000 |
| B5 attribution lattice | 22 | **2** (D1) | 24 | 288,000 |
| B6 partner regime | 64 | — | 64 | 768,000 |
| B7 cross-play | 24 | — | 24 | 288,000 |
| B8 rule dialects | 16 | — | 16 | 192,000 |
| B9 controls | 10 + 12 | **16** (D2) | 26 + 12 | 312,000 + 4 fits |
| B10 S6 residual | 16 | — | 16 | 19,200 deals / 38,400 games audited |

**428 scored `match` cells and 28 `v7side` cells; every preregistered cell measured, none dropped.**
One action-limit hit, in one game of 12,000 (D22).

**Scored cells carry no audit evidence.** `auditChecks` is 0 in all 428 of them — `match` runs the
audit only under `--audit`, which no preregistered cell uses. The engine-wide audit figure this
corpus quotes, 0 violations in 6,737,436 checks, comes from B0.5's `verify` run and is cited there
and nowhere else.

Raw output is under `research/v07/results/`:

| artifact | contents |
|---|---|
| `P5-B0.json` | the verification block: seven bank digests, the sealed SHA-256, the freeze round-trips, the seed registry, `verify`/`selftest`, the reproducibility runs |
| `P5-gate.jsonl`, `P5-gate.txt` | the commit gate for all four configurations, per rule |
| `P5-B2.jsonl` … `P5-B10.jsonl` | one JSON row per cell, each carrying the literal `argv` it was produced by and the complete `match --json` object |
| `P5-Z01.jsonl` … `P5-Z08.jsonl`, `P5-B9-h*.jsonl` | the CEM fitting traces: header, one record per generation, and the final weight vector each adversary spec was rebuilt from |
| `P5-TABLES.txt` | the reduction, produced by `engine/p5_analyse.py` |
| `P5-drift.json` | the executed source-drift check of D19/D27: both binaries' SHA-256, the build command, the recomputed mirror digests, and the sources differing from the artifact baseline and from the freeze commit |
| `MANIFEST-P5.json` | SHA-256, byte count and row count of every artifact above, with the commands and designs that produced them; also `paper/tables/manifest_v07p5.tex` |

Every row carries its own `argv`, so any single cell can be re-run by hand from the artifact without
reference to this document or to the driver.

---

## 17. The result, stated plainly

The frozen v0.7 configuration **passes the commit gate on all eight rules**, and the gate still
rejects the configuration built to be rejected.

It is a **certified advancement over the v0.6 frontier**: +3.33 [+2.88, +3.78] over `F-cheap` on
48,000 games of sealed material, replicating on both banks, with a lower bound above the
pre-committed 1.53 bar. It also beats the deployed policy by +4.63 [+4.19, +5.06], and `F-mid` by +2.89 [+2.00, +3.78] —
the latter grazing the protocol's own 2-point restriction on `F-mid` claims rather than clearing it.

Its **worst cell over a 31-member shared panel is −0.04**, far above the incumbent's −4.24, which is
what S6 tests, and it passes. It does **not** have the best worst case of the four arms: the phase-2
composite's is higher, at +0.00 with its free self-cell and +1.23 without one. On the hardest sealed
adversary, `X01xC3f`, `FROZEN` is one of **two** arms that beat it — +1.43 against the composite's
+1.23 — and it does so by the larger margin. It does **not**
have the best minimax regret: at 4.53 it is third of four, behind `F-cheap` at 4.00 and the composite
at 4.08, and the difference is concentrated in the far archetypes.

**Nothing in this search budget exploits it.** Eight fresh adversary searches — including the
white-box inverter that reads the target's own deterministic policy — all lose, by between 2.9 and
10.7 points, with every sign replicating. Each search is 8 generations × 12 population × 120 deals,
and the white-box arm searches 37 live coordinates of the 55 it was given (§15 D16). Nothing here
bounds attacks outside that budget and those classes.

Its advantage is **not a self-play convention**: it survives partner substitution with a minimum of
−0.19 across eight partner settings, and cross-play between three independently-trained runs shows a
gap of −0.01. On the median-to-self-play ratio it transfers about as well as v0.6's advantage did,
and slightly less well.

Its advantage under partner change passes on the point estimate and is **not resolved against the
threshold**: the minimum row is −0.19 with an interval of [−1.04, +0.67], whose lower bound falls
below the −1.0 that S1 fails on.

Two prespecified conditions went against it. It **does not beat the phase-2 composite** — +0.15
[−0.29, +0.59] — so the increment phase 3 reported did not replicate. §6 item 2 attaches a mandatory
consequence to that outcome and it is recorded in the protocol's own terms: **on this cell v0.7
reduces to the phase-2 composite, which is a phase-2 result and not a v0.7 one, and the v0.7
contribution is the measurement programme and the four closed ledger entries, not the policy.** That
does not erase B2.2 and B2.3, which are claims about the v0.6 frontier that the composite cell does
not touch; both stand. And the protocol's own **location test does not replicate**: the gain is
attributable on one bank and not on the other, and the difference between the banks is itself
unresolved.

The **instrument is intact** on every control the protocol specified, including the sub-floor rung
that must not be recovered and the three planted side channels that must each fail exactly one test.

**Selection accounts for none of the measured gain** on holdout material, which was never available
to select on. Where a training figure exists for the same quantity, the one that falls short is the
one measuring the group of keys the freeze was selected over — by 0.63 points against an expected
maximum under the null of 0.74.

The configuration costs **at least 4.5× the blueprint**, not the 3.2× the record carried.
