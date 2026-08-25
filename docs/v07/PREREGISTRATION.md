# FishBot v0.7 — PREREGISTRATION: the phase-5 battery, committed before any holdout result is known

Dylan Nguyen, FishLab Research Project
Repository `https://github.com/dylann4500/fishbot.git`, built on the commit this file is committed at.
Phase 4 of the v0.7 programme (`docs/v07/PHASE-PROMPTS.md`).

**This document is the entire protocol for phase 5.** Phase 5 starts from a cleared context and is
instructed not to read `CANDIDATES.md`, `ADVERSARIES.md` or any training log. Everything it needs is
here: what is frozen, what it may touch, every cell with its sample size and the arithmetic behind
it, every threshold, and — stated in advance, which is the only time it can honestly be stated —
**what result would mean v0.7 is not an advancement.**

It is committed before any holdout bank has been played and before the sealed adversary half has been
decoded. `research/v07/banks/SEAL.json` records the commit at which the material was sealed
(`f4581da`, phase 2) and the SHA-256 of the sealed plaintext; §2 below records the digests phase 5
must recompute. If phase 5 finds a genuine flaw in this protocol it **stops and reports it** — an
amended protocol is a training run, and any amendment requires fresh holdout banks.

---

## 0. The one-paragraph summary of what is being tested

v0.7 is **a configuration of the v0.6 policy family, not a new architecture**: v0.6's frozen
37-coordinate vector, one hand-set extended coordinate, three switches that turn off machinery phase
2 measured as defective, one new mechanism (a termination rule that replaces an event-count cliff
with a stall detector), and the truncated test-time search that v0.6 shipped switched off. The claim
phase 5 is testing is that this configuration **beats the v0.6 frontier — not merely the deployed
policy — on material chosen and sealed before it existed**, that the advantage survives a fresh
adversary search aimed at it, that it is not a self-play convention, and that it is attributable to
named components rather than to a selection over many candidates.

---

## 1. What is frozen

```
v07:r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12,
    s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26
```

`engine/fishbot_v07.json`, written by `engine/freeze_config_v07.py`. **Phase 5 reconstructs the spec
from that file rather than typing it**, and runs `freeze_config_v07.py --verify-only` first (B0.3).

| field | value |
|---|---|
| mirror pathology digest | `5f81f440fc9c272a87e87c05fecc7b74` |
| inherited vector | v0.6's `V6PARAMS`, `fitC.jsonl … obj=minimaxregret paired=1 panel=v05+v03+withholder+feint seed=20260824` |
| resolved vector | 55 coordinates, recorded in the JSON: 20 ask weights, 14 v0.5 knobs, 3 v0.6 ask terms, 18 v0.7 responder coordinates |
| commit gate | **PASS**, all eight rules G1–G6, G7a, G7b (`P4-gate.jsonl`, id `FROZEN-v07`) |

**What each key is, so phase 5 can read a failure.**

| key | what it does | phase-4 leave-one-out drop |
|---|---|---:|
| `r12=25` | the half-suit contestation / information-denial coordinate `oppCertDonate`, found by phase 2's unfitted coordinate sweep | **+2.10**, replicated |
| `s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` | the endgame-truncated test-time search v0.6 shipped **off** | +0.77, replicated |
| `rtie=1` | replaces an unstable `std::sort` order that decides 53.80% of contested ask decisions with a hash of the public event stream | +0.20 |
| `pool=-1,oppfloor=-1,force=1000000,askfloor=-1` | switches the `urgent` disjunction off, and with it v0.5's 220-event clock and its fifteen-point cliff | +0.02 |
| `stall=12` | the replacement termination guarantee: escalate after 12 consecutive public events in which this seat's hard-certificate hash does not change | **+0.00, bit-identical** |

The last three are individually indistinguishable from zero at the margin and **as a group** are worth
**+0.78 [+0.33, +1.22]**, replicated on both training banks, measured as a paired head-to-head against
the phase-2 composite. Phase 5 must not attribute the group's value to any one of them.

**Five things about this configuration that phase 5 should expect to see, because phase 4 measured
them on training material and they are stated here so that agreement is a check and disagreement is a
finding.**

1. **`r12=25` carries the gain.** It is a phase-2 discovery, not a phase-3 one. If the holdout
   ablation does not reproduce a leave-one-out drop of roughly 2 points for `r12`, the attribution is
   wrong.
2. **`stall=12` fires zero times in ordinary play.** Its mirror digest is byte-identical to the same
   configuration without it, at 24,000 games on two banks. If phase 5 sees the stall rung firing on
   holdout material, that is a new phenomenon and belongs in the report.
3. **`m2=0` was dropped from the freeze** because its leave-one-out drop is exactly zero. Phase 5 does
   not need to re-test it, and should not add it back.
4. **The partner-regime table will shrink and should not collapse.** On training banks the `FROZEN` −
   `INCUMBENT` delta is **+2.94 in self-play against a `v05` opponent and −0.15 to +2.08 with a
   foreign partner** (median +1.26; four of the seven changed-partner rows clear zero under the
   conservative interval of §5.2). Against a `v06` opponent it is far more stable: +4.82 self and
   +2.50 to +2.88 under partner change, all three clearing zero. A **one-seat** upgrade is worth
   **+1.26** against `v05` and **+2.88** against `v06`, i.e. 43% and 60% of the three-seat gain,
   which is the single strongest piece of evidence that the gain is individual rather than
   conventional.
5. **Cross-play should not collapse.** Three independently-trained runs of the architecture
   (disjoint fitting banks, different CEM trajectories, one different starting basin, pairwise L2
   distance 7.1–11.2 over 55 coordinates) score **+4.48 self-play and +4.22 cross-play — a gap of
   0.26 against a per-cell half-width of 0.63.** If phase 5's B7 shows a gap of more than 1.5, that
   is a finding and it contradicts phase 4.

**Cost.** The configuration runs at roughly **3.2× the blueprint** — phase 3's correction to the
corpus's own record, which had it at 242× because that figure was `F-search`, the unrestricted
configuration, and not the endgame-restricted operating point. Phase 5 reports measured throughput
with the frontier table so the cost claim is checked rather than repeated.
---

## 2. The material, by name, with the commitments phase 5 must verify

### 2.1 Deal banks

Every bank is a **seed plus a size**; the deals are generated from the deal index
(`arena.hpp:165`, `mixSeed(S, i·2654435761 + 1)`) and are never stored, so a bank is cheap to
reproduce and impossible to hide. Sealing therefore means a *commitment*, not secrecy, and phase 5
verifies it: `fish7 bankdigest --seed=S --deals=N` folds the six dealt hands and the dealer of every
deal into a 64-bit rolling hash without constructing a policy or playing a game.

**B0 is the first command phase 5 runs, before anything else.** Every digest below must match. A
mismatch proves a seed or a size changed between the commitment and the evaluation and is a stop
condition, not a note.

| bank | deals | digest to reproduce | role in phase 5 |
|---|---:|---|---|
| `7090001` | 24,000 | `896dbc89be124d85` | **primary holdout.** Every headline cell. |
| `7090002` | 24,000 | `0b6e40d834ac0ca1` | **second disjoint bank.** Every claim re-run here. |
| `7090003` | 24,000 | `863bea69baf6e73c` | third bank: the ablation lattice and the dialect table |
| `7090004` | 24,000 | `54f257c3f8ae9fab` | the fresh adversary search against the frozen configuration |
| `7090005` | 24,000 | `268a1dae71a31713` | negative controls and planted-edge recovery |
| `7091001` | 24,000 | `958ada042cc26900` | evaluation bank for the sealed adversary half |
| `7091002` | 24,000 | `5c39af3b5e0bd9a0` | fitting bank for the phase-5 fresh search |

`runMatch` (`arena.hpp:111-116`) refuses all seven unless `FISH_UNSEAL_PHASE>=5` is in the
environment, and exits 5. Phase 5 sets it once, at the top of its battery, and records that it did.

Two limits of the seal, stated here so phase 5 does not over-read it. **(a) The refusal is
per-registered-seed, not per-prefix**: `--seed=7090006` is not refused because it is not in the
registry. Phase 5 must use the seven seeds above and no others, and must run `fish7 seeds --require=`
on each. **(b) `pathology`, `v7bits` and `v6probe` build a `Game` directly and never call
`seedUsable`** — they would happily play a sealed bank. The commit gate (B1) runs on the *training*
seed 31 exactly as it did in phase 4, and that is deliberate: the gate is a soundness check on
self-play behaviour, not a strength claim, and running it on holdout material would spend a bank for
nothing.

### 2.2 The adversary population

`research/v07/banks/holdout/adversaries-holdout.sealed`, **14 rows**, base64-encoded, plaintext
SHA-256 `1ca0346a332586c70a750f1523b105485322af34ff31aab3b9e77a2f0a3b6c52` (`SEAL.json`). The split
rule was fixed in `engine/seal_banks_v07.py` before any phase-2 result was known: rows sorted by id,
alternating, **even positions train, odd positions holdout**. Phase 5 decodes the file, verifies the
SHA-256 against `SEAL.json`, and reports the verification before any result.

The 14 training-half adversaries are named in `research/v07/banks/train/adversaries-train.tsv` and
are **not** part of the phase-5 population except where §3 names them explicitly as a
training-to-holdout transfer check.

The scripted archetype panel — `v05`, `v04`, `v03`, `v02`, `lockout`, `detective`, `feint`, `hunter`,
`diversifier`, `bluffer`, `silent`, `withholder`, `random` — is a fixed part of the engine and is not
sealed; it is included because minimax regret is only defined over a shared panel and because the far
archetypes are where a configuration that has quietly broken shows up first.

The frontier — `v06`, `F-cheap`, `F-mid`, and the phase-2 composite — is included as the *bar*. The
v0.7 case rests on beating the frontier, not on beating the deployed policy. **These are not engine
policy names and `makeAgent` will exit 2 on them**, so the four spec strings are written out here and
phase 5 uses these and not the labels:

| label | spec |
|---|---|
| `INCUMBENT` | `v06` |
| `F-cheap` | `v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` |
| `F-mid` | `v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26` |
| the phase-2 composite | `v07:m2=0,r12=25,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26` |

The scripted archetypes and the sealed adversaries are engine policy names and spec strings
respectively and need no translation.
---

## 3. Sample sizes, with the arithmetic shown

The corpus's resolution rule is that a win-rate cell of *N* games carries a 95% half-width of
approximately **98/√N points**, because for a rate near 0.5 the standard error is 50/√N points and
two of them is 100/√N. `v07_power.hpp` computes it and `match --json` emits it with every cell as
`power.halfWidth98Games`, so no number below has to be trusted on assertion.

| games *N* | 98/√N | what it resolves |
|---:|---:|---|
| 2,400 | 2.00 | nothing this study claims |
| 4,800 | 1.41 | a 3-point archetype gap |
| 12,000 | 0.89 | a 2-point effect |
| 24,000 | 0.63 | a 1.5-point effect at the class detection floor |
| 48,000 | 0.45 | a 1-point component of an ablation |
| 96,000 | 0.32 | a 0.7-point component |
| 192,000 | 0.22 | a 0.5-point component |

**Three corrections to that arithmetic, all of which phase 5 must apply.**

1. **`--games` counts DEALS, not games.** The arena plays each deal at `--rotations` orientations, so
   a cell of *D* deals is *2D* games. Every *n* in §4 is stated in deals **and** in games.
2. **Deals are the clustering unit, not games.** The rotations of one deal are one correlated unit.
   `match --json`'s `"ci"` is the deal-clustered bootstrap and `"wilsonCI"` is the naive per-game
   interval; **phase 5 quotes `ci` and never `wilsonCI`.** Phase 3 measured the design effect of deal
   clustering for the first time: 1.03 on declaration accuracy, 1.01 on the allocation-error share,
   3.41 on the ask hit rate. The win-rate channel is already scored per deal by the duplicate design,
   so 98/√(games) is the right figure there and the design effect matters only for the per-decision
   channels.
3. **The 1.53 floor is a reference bar here, not a detection floor, and that is stated because the
   two are different objects.** `ADVERSARIES.md` defines 1.53 as the smallest planted handicap a
   *fitted C1 responder* can recover in an exploitability search at 48,000 evaluation games a bank.
   §5.1 then uses it as the bar a **directly measured paired duplicate win-rate difference** must
   clear — a quantity that involves no responder, no fit and no maximisation. Nothing in the corpus
   shows that a responder-recovery floor bounds a directly measured difference, and this document
   does not claim it does. What 1.53 is being used for is a **fixed, pre-committed reference** at a
   size the corpus has already shown is meaningful, so that "certified" means the same thing in phase
   5 as it did in phases 2 and 3. Where it is applied to an *exploitability* number — B4, S3, B9.2,
   B9.3 — it is a detection floor in its original sense and no caveat is needed.
4. **The detection floor does not buy down with evaluation games.** Phase 2 measured this directly:
   four times the power moved the C1-class floor from 1.68 to **1.53**, not to the 0.78 the scaling
   law predicts, and the +0.88 planted rung resolves *to* zero (−0.01 [−0.45, +0.42]) rather than
   sharpening. So an interval half-width below 1.53 does **not** license a sub-1.53 exploitability
   claim. The **declaration-family** floor is **2.13**, and it applies to exactly one thing in this
   battery: any claim phase 5 makes about an adversary driving the target's *declaration* channel
   (B4's `Z03`, and the declaration-accuracy column of B2) is read against 2.13 and not against 1.53.
   Neither number is renegotiable by buying more games.

**The paired design.** Every comparison in §4 is run as a paired duplicate: the same deal bank, the
same deal indices, `--rotations=2` so each deal is played with the teams swapped. The two arms
therefore see identical cards and the deal's intrinsic luck cancels. Where a cell compares two
*variants* of v0.7 rather than v0.7 against an opponent, the variants are additionally run on the
same banks and the same indices, so the difference between two cells is better resolved than
0.63·√2 would suggest — positively correlated errors subtract. Phase 5 reports the paired difference
where the harness provides one (`fish7 ablate --ref/--variants` gives a paired `deltaFromRef` over a
shared panel) and the two independent intervals otherwise, and says which it did.

**Every claim is re-run on a second disjoint bank.** No result in phase 5 is reported from one bank.
The primary is `7090001`, the replicate is `7090002`, and the reported figure is the pooled estimate
with both per-bank values printed beside it. A claim whose sign does not replicate across the two
banks is reported as **not replicated**, whatever its pooled interval says.
---

## 4. The battery, cell by cell

Run in this order. **B1 completes and passes before any cell of B2 is run**, and a strength number
from a configuration that has not passed B1 is not reported at all — not reported-with-a-caveat.
The corpus contains two configurations that score higher while being unsound (`v06:rtie=1,m1=0,…`
at +2.68 with 2.91% provably-dead asks and a 326-ask dead run; M8-alone at 56.60% against v0.4 with
44.83% dead asks), and the ordering is what catches them.

Throughout, **`FROZEN`** means the configuration in `engine/fishbot_v07.json`, reconstructed from
that file rather than typed, and **`INCUMBENT`** means `v06`.

### B0 — verification, before anything is measured

| # | command | pass condition |
|---|---|---|
| B0.1 | `fish7 bankdigest --seed=S --deals=24000` on all seven sealed seeds (`--deals`, **not** `--games`, which `bankdigest` silently ignores) | all seven digests match §2.1 |
| B0.2 | decode `adversaries-holdout.sealed`, SHA-256 it | matches `SEAL.json` |
| B0.3 | `engine/freeze_config_v07.py --verify-only` | R1 string, R2a blueprint-vector and R3 digest round-trips all pass. It also **prints a NOTE listing engine sources that changed since the freeze** — that is a report and not a failure, and phase 4 already leaves three (`arena.hpp`, `main.cpp`, `v07_side.hpp`, all changed after the freeze with the frozen policy byte-identical). A changed source *with* a changed R3 digest is a stop |
| B0.4 | `fish7 seeds --require=<the seven seeds>` | all registered; the report of any R1/R2 registry violation is recorded |
| B0.5 | `fish7 verify` and `fish7 selftest` | PASS |

B0.3 is the assertion that the frozen artifact still names the same policy: it rebuilds the spec from
the JSON's base and option map, plays the spec form against the explicit 55-coordinate `allparams`
form **with the search off**, where the two must be identical on every deal, and recomputes the frozen
mirror digest. If the engine has moved the frozen policy, this fails before any holdout deal is
played. The same comparison **with the search on** is measured and printed rather than asserted —
§5.3 explains why it cannot be asserted to zero.

### B1 — the commit gate, before any strength number

For `FROZEN`, `INCUMBENT`, `F-cheap`, and the **negative control** below, on the *training* seed 31
(see §2.1(b) for why the gate does not spend holdout material):

`fish7 pathology --a=SPEC --b=SPEC --games=400 --rotations=2 --seed=31`, plus `fish7 v7side` on both
*training* banks with **S6 in its own process at `--threads=1 --freshagents`** — all three conditions,
and §5.3 says why each is needed.

| rule | threshold | set from |
|---|---|---|
| G1 provably-dead asks | ≤ 0.10% of asks | `v06` 0.0118%, K3 arms 0.0088–0.0203%, the rejected stack 2.91% |
| G2 longest dead run | ≤ 5 | `v06` 1, K3 arms 1, the rejected stack 326; 6 is the engine's own run cut |
| G3 games with a run ≥ 6 | 0 | zero for every configuration the corpus has shipped |
| G4 action-limit games | 0 | `v06` 0; the rejected stack 0.33% |
| G5 mirror tail | max < 220 **and** p99 ≤ 150 | 220 is v0.5's pressure rung and a 15.18-point cliff; `v06` max 131 |
| G6 declarations at or after event 220 | 0 | such declarations are taken under the collapsed 0.25 floor |
| G7a S3, S4, S5 | CERTIFIED, zero tolerance | every configuration in the corpus passes |
| G7b S6 | **0** mismatches, at `--threads=1 --freshagents` | zero for every configuration in the corpus once §5.3's confound is removed |

**The negative control is part of B1, not an appendix.** `v06:rtie=1,m1=0,pool=-1,oppfloor=-1,`
`force=1000000,askfloor=-1` **must FAIL** G1, G2, G3, G4 and G5. If it passes, the gate is broken and
phase 5 stops: a gate that cannot reject the one configuration the corpus built to be rejected
certifies nothing about the one it accepts.

### B2 — headline strength against the frontier

Paired duplicate, `--rotations=2`, both primary banks, every cell re-run on the second.

| cell | opponent | deals/bank | games/bank | games total | 98/√N |
|---|---|---:|---:|---:|---:|
| B2.1 | `INCUMBENT` (`v06`) | 12,000 | 24,000 | 48,000 | 0.45 |
| B2.2 | `F-cheap` | 12,000 | 24,000 | 48,000 | 0.45 |
| B2.3 | `F-mid` | 3,000 | 6,000 | 12,000 | 0.89 |
| B2.4 | the phase-2 composite | 12,000 | 24,000 | 48,000 | 0.45 |
| B2.5 | `v05` | 6,000 | 12,000 | 24,000 | 0.63 |

`F-mid` is allotted a quarter of the games of the other frontier cells and it is the one place in
this battery where that is a deliberate compromise rather than a calculation: `F-mid` runs at ~6.7
games/s against the ~100 of the cheap search, so a 48,000-game cell would cost two hours of simulator
time on its own. **The consequence is stated in advance: the `F-mid` cell resolves 2 points and not
1, and no claim about `F-mid` may be made below 2 points.**

### B3 — the panel: worst case and minimax regret

The statistic that leads the report. Every arm is scored against the **same** panel on the **same**
banks under the same protocol, because minimax regret is only defined over a shared panel — which is
why `INCUMBENT` is an *arm* here and not only an opponent.

Arms: `FROZEN`, `INCUMBENT`, `F-cheap`, the phase-2 composite.
Panel: the 14 **sealed** adversaries, the 13 scripted archetypes, and the four frontier points.

**The near/far assignment is fixed here, by name, from phase-2 and phase-4 material** — it cannot be
decided from the holdout margins, because those are the numbers the cells exist to measure, and 14 of
the 31 panel members are sealed adversaries for which no margin exists at all.

| class | members | deals/bank | games/bank | games/cell (2 banks) | 98/√N |
|---|---|---:|---:|---:|---:|
| **near** | `F-fast`(=`v06`), `F-cheap`, `v05`, `v04`, `feint`, and **all 14 sealed adversaries** | 6,000 | 12,000 | 24,000 | 0.63 |
| **far** | `v03`, `v02`, `lockout`, `detective`, `hunter`, `diversifier`, `bluffer`, `silent`, `withholder`, `random` | 1,500 | 3,000 | 6,000 | 1.26 |
| **expensive** | `F-mid`, the phase-2 composite | 1,200 | 2,400 | 4,800 | 1.41 |

The sealed adversaries are all assigned to **near** without looking at them: every member of the
training half sits within 15 points of the incumbent (phase 4's panel: −3.50 to +8.11 for `v06`), the
split rule was alternating by id, and giving them the larger size is the conservative choice.

Reported as: **worst cell per arm**, **minimax regret per arm**, then every cell. The mean over the
panel is printed last and labelled a diagnostic. It is never the headline, and phase 5 is not
permitted to lead with it even if it flatters the frozen configuration.

### B4 — a fresh adversary search against the frozen configuration

Eight independent searches. Only two sealed banks are available for fitting, so they are **sharded**,
and the mapping is fixed here rather than left to be chosen. `--shard=s/n` partitions a bank by deal
index exactly (`arena.hpp`), so eight disjoint fitting sets exist. **`tune` did not parse `--shard`
until phase 4 added it** (`tuner.hpp`, `main.cpp`): before that a sharded `tune` silently ignored the
flag and two searches written as disjoint shared every deal. The tuner scales its per-generation deal
count by `shards`, so `--games=120 --shard=s/4` still evaluates 120 deals per opponent per
generation, out of 480 indices.

| id | fitting bank | shard | id | fitting bank | shard |
|---|---|---|---|---|---|
| Z01 | 7091002 | `0/4` | Z05 | 7090004 | `0/4` |
| Z02 | 7091002 | `1/4` | Z06 | 7090004 | `1/4` |
| Z03 | 7091002 | `2/4` | Z07 | 7090004 | `2/4` |
| Z04 | 7091002 | `3/4` | Z08 | 7090004 | `3/4` |

The command template, literally, with the objective column below supplying `--kpi=` — **not
`--obj=`**, which takes `{softmin,min,mean,regret,minimaxregret}` and falls through to `softmin`
silently on an unknown value:

```
fish7 tune --panel="<FROZEN spec with commas rewritten to +>" --base=<v06|v07|v07i> --full --fromv6 \
           --kpi=<win|declerr|events|forced|asksupp> --obj=min --paired --beta=1 --sigmarel=0.08 \
           --games=120 --pop=12 --elite=5 --gens=8 --seed=<bank> --shard=<s/4> --threads=13 \
           --out=research/v07/results/P5-Z<NN>.jsonl
```

Two rows deviate from the template and the deviation is written into the row: **Z06** replaces
`--sigmarel=0.08` with `--sigmarel=0.30`, and **Z07** drops `--fromv6` so the CEM starts from the
v0.5 defaults instead of the incumbent. **Z08**, the white-box class, uses `--base=v07i` and additionally
`idet=48,imodel=v06` in the base, which is how phase 1's C5 responder is specified.

The axes are the ones phase 2 established are genuinely different searches — class, objective,
starting basin, step size — and the objectives are aimed at *mechanisms* rather than at reproducing
phase 2's adversaries.

| id | class | base | objective | why this one |
|---|---|---|---|---|
| Z01 | C1 in-class | `v06` | `win` | the class control; the only class that has ever beaten the incumbent |
| Z02 | C2 extended | `v07` | `win` | the class that produced phase 2's strongest arm |
| Z03 | C1 | `v06` | `declerr` | the declaration channel, with the urgency branch now switched off |
| Z04 | C1 | `v06` | `events` | lengthen the game: the 220-event cliff is gone, so is the stall rung reachable? |
| Z05 | C1 | `v06` | `forced` | forced-endgame incidence, which switching urgency off raises about six-fold |
| Z06 | C2 | `v07` | `win`, wide σ | is the CEM trapped near the incumbent against this target |
| Z07 | C1 | `v06` | `win`, v0.5 basin | a different starting basin: one more shared bias removed |
| **Z08** | **C5 white-box** | `v07i` | `win` | **the white-box inverter, required by the phase-5 brief.** It sharpens its deal posterior by inverting the target's transcript against the target's known deterministic policy; phase 1 measured that channel at ~2.0 bits per observed ask. |

Budget per search: 8 generations × 12 population × 120 deals × 2 rotations = **23,040 games**, drawn
from 480 deal indices of the bank under `--shard=s/4`; total fitting **184,320 games**. Evaluation of each fitted adversary against `FROZEN`: 6,000 deals ×
2 rotations × 2 banks = **24,000 games**, half-width 0.63.

**Nothing fitted here is evaluated on the bank it was fitted on.** The win rate reached during
fitting is a maximum over a population on shared seeds and is upward biased; that is why the fitting
banks (`7090004`, `7091002`) and the evaluation banks (`7090001`, `7090002`) are disjoint.

### B5 — ablations: is the gain attributable?

The phase-4 attribution lattice, re-run on holdout bank `7090003` with `7090001` as the replicate.
Reference opponent `INCUMBENT` throughout, so the pieces are comparable and checkable for additivity.

*Add-one-in from `v06`*: the search alone; `rtie=1` alone; the urgency-off keys alone; `stall=12`
alone; `r12=25` alone; `m2=0` alone.
*Leave-one-out from `FROZEN`*: minus the search; minus `rtie`; minus urgency-off; minus `stall`;
minus `r12`. (There is no "minus `m2`" cell: the freeze carries no `m2` key, so that cell would be
`FROZEN` itself and would measure zero by construction. Phase 4 measured it and it is +0.00, which is
why the key was dropped.)

6,000 deals × 2 rotations × 2 banks = 24,000 games a cell, half-width 0.63. Eleven cells,
**264,000 games**.

Reported as: each component's add-one-in edge, each leave-one-out drop, the naive sum of the
components against the measured whole, and the **sub-additivity ratio**. Phase 2 measured its own
three mechanisms composing at 83% of their naive sum; a v0.7 report that quotes a sum is wrong.

### B6 — the partner-regime table

`fish7 match --a=ARM --partners=P --b=v05`: team A is [ARM, P, P] and team B is three copies of v0.5.
This is ledger L6's design (`engine/experiments_v06.sh:125-131`) at 30× its power — L6 ran 800 games
a cell, half-width ±3.46, at which "not one of the four deltas is separated from any other".

Arms: `FROZEN`, `INCUMBENT`, `v05` — **all three at every partner setting**, because S1's reported
baseline is `INCUMBENT` − `v05` and it has to be taken over the same rows as `FROZEN` − `INCUMBENT`.
Partners *P*: **itself**, `v06`, `v05`, `v04`, `v03`, `detective`, `withholder`, `lockout`.
6,000 deals × 2 rotations × 2 banks = 24,000 games a cell, half-width **0.63**. 3 × 8 = **24 cells,
576,000 games.**

**And the same table against `--b=v06`**: arms `FROZEN` and `INCUMBENT`, partners itself, `v06`,
`v03`, `detective` — 8 cells, 192,000 games. §1 item 4 states expectations for that regime and
without these cells they cannot be checked, and §4.7 of the phase-4 log is explicit that a
partner-transfer claim holding against one opponent is not a claim.

The self-play row is reported but **never headlined**: the target configuration is three copies, so
self-play coordination is legitimate, and the question this table answers is whether the *advantage
over the incumbent* is the same size when the partners change.

### B7 — cross-play between independently-trained runs

The frozen configuration is not a fit, so two independently-trained runs of it do not exist and phase
4 produced them: the same architecture, the same panel and objective as v0.6's own fit
(`obj=minimaxregret`, `paired`, panel `v05+v03+withholder+feint`), differing in CEM trajectory, in a
disjoint fitting bank, and — for one run — in starting basin. The runs and their pairwise parameter
distances are committed with this file.

Every ordered pair (i, j): `--a=run_i --partners=run_j --b=v05`. The diagonal is self-play. 6,000
deals × 2 rotations × 2 banks = 24,000 games a cell, half-width 0.63. Plus each unordered pair head
to head at the same size, so "these are different policies" is measured rather than assumed, and the
parameter distance is reported beside it.

**A convention private to one run shows up as the off-diagonal collapsing relative to the diagonal.**
The Hanabi line reports self-play-to-cross-play collapses that are not subtle — SAD 23.97 → 2.52,
IPPO 24.04 → 0.12 — so the effect, if present, is not a subtle one to look for.

### B8 — the rule-dialect table

Both arms play the same dialect; the cell is "does the v0.7 edge over the incumbent survive this
reading of the rules". Ledger P-8's complaint is that `--legacy` changes four things at once, so it
is unbundled here.

Rows: `default`; `--no-out-of-turn`; `--no-cardless-declare`; `--maxasks=360`; `--arb=high`;
`--arb=turn`; `--sets=8` (48-card Literature rather than 54-card Canadian); `--legacy`.
6,000 deals × 2 rotations × 2 banks = 24,000 games a row, half-width 0.63. Eight rows, **192,000
games**.

The forced-endgame willingness ladder has **no CLI flag of its own** and is reachable only through
`--legacy`; it is therefore not given a row rather than faked, and the residual (legacy minus the
three isolable components) is reported as the ladder's contribution.

### B9 — negative controls

| # | control | what it must show |
|---|---|---|
| B9.1 | **a configuration the gate must reject**: `v06:rtie=1,m1=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1` | FAILS G1–G5 (run in B1) |
| B9.2 | **planted-weakness recovery**: `FROZEN,hcap=decl,hstr=H` for **H ∈ {0.08, 0.11, 0.15}** — phase 1's calibrated rungs — attacked by a C1 responder fitted on `7090005` at **8 × 12 × 120 deals = 23,040 games**, evaluated against the handicapped target on `7090001` and `7090002` at **6,000 deals × 2 rotations × 2 banks = 24,000 games** (half-width 0.63) | the responder recovers each planted edge at or above 1.53 points, and the recovered size tracks the planted size across the three rungs |
| B9.3 | **a planted edge below the floor**: `hstr=0.05`, the rung phase 2 measured at **+0.88** | is **not** recovered — same fit budget and the same 24,000-game evaluation. Phase 2 measured this excess resolving *to* zero at −0.01 [−0.45, +0.42]; if phase 5 now "detects" it, the instrument has changed and every other number in this battery is suspect |
| B9.4 | **the identity control**: `FROZEN` against itself | exactly 50.000%, `power.mirror` true, zero variance |
| B9.5 | **the side-channel positive controls**: `v07x:cheat=seed`, `cheat=shared`, `cheat=conv` | `cheat=seed` fails **S4 and S5** and passes S3 and S6; `cheat=shared` fails **S6 only**; `cheat=conv` fails **S3 only** — on both banks |

B9.3 is the control that makes the rest of the battery interpretable, and it is the one most likely
to be quietly skipped. It is not optional.

### B10 — the S6 residual

Phase 4 found — and this document records it in advance so that phase 5 cannot be accused of finding
it afterwards — that the apparent S6 failure of every searching configuration is **cross-deal agent
state, not a side channel**, and that removing it takes every cell to exactly zero. §5.3 gives the
measurement and the two conditions the gate now carries.

Phase 5 re-measures on holdout material, 1,200 deals per cell, both banks, for `FROZEN`,
`INCUMBENT`, `F-cheap` and the phase-2 composite, in **both** conditions:

| arm | `--threads=1` alone | `--threads=1 --freshagents` |
|---|---|---|
| what phase 4 measured on training banks | 1–3 mismatches per ~270,000 for searching arms, 0 for blueprint | **0 everywhere** |
| what phase 5 must find | a nonzero, ask-only residue for searching arms | **0**; anything else is a finding |

Reporting both is the point: the first column is the reproduction of phase 4's defect and the second
is the gate condition. If the second column is nonzero for any arm, S6 has caught something phase 4
did not, and it is the headline.

### B11 — the selection-bias check

Phase 4 evaluated a bounded number of candidate configurations and selected the best. Under the null
that none of them differs from the incumbent, the maximum of *K* independent cells each with standard
error σ has expectation approximately σ·√(2 ln K). *K* is fixed here rather than counted
later: **14** distinct configurations were scored against `v06` in phase 4, enumerable from
`P4-lattice.jsonl` and `P4-replicate.jsonl`, and one more (`K3-stack`) in phase 3's panel, so
**K = 15**. At the 24,000-game cell size used for the lattice, σ = 98/2/√24000 = 0.316 points, and
σ·√(2 ln 15) = **0.74 points** is the expected maximum under the null. Phase 5 reports that figure
beside its measured holdout gain and states **how much of the gain would be expected from selection
alone**. The holdout banks were never available for selection,
so the correct expectation is that this term is zero on holdout; the check exists to confirm that the
holdout estimate is not systematically smaller than the training estimate by about that amount.
---

## 5. Pass and fail, stated in advance

### 5.1 The primary claim and its threshold

**Primary claim.** `FROZEN` beats **`F-cheap`** — the cheapest point of the v0.6 frontier that is
actually on the frontier — on both primary holdout banks, pooled over 48,000 games.

| verdict | condition |
|---|---|
| **CERTIFIED advancement** | pooled edge over `F-cheap` has a **lower bound above 1.53**, and the sign replicates on both banks, and `FROZEN` passes B1 |
| **measured advancement** | pooled edge positive with a lower bound above 0, sign replicates on both banks, B1 passes — but the lower bound is under 1.53 |
| **not an advancement** | the pooled interval contains zero, or the sign does not replicate, or B1 fails |

The 1.53 is the phase-2 C1-class detection floor and it is **not** an interval width: it is the size
below which this instrument's responders are empty rather than unresolved. It does not buy down with
games (§3.3), so a 96,000-game cell does not lower it.

`F-cheap` is named as the bar rather than `v06` deliberately. The deployed policy ships its search
**off**; beating it is the easier claim and phase 2 already established that several unfitted
one-switch deviations of the incumbent do it. The v0.7 case has to be against the frontier.

### 5.2 The secondary claims, each with its own threshold

| # | claim | passes if | fails if |
|---|---|---|---|
| S1 | **the advantage does not COLLAPSE under partner change** | the `FROZEN` − `INCUMBENT` delta is **positive in at least five of the eight changed-partner rows** of B6, **and** its minimum is **not below −1.0** (about 1.5 cell half-widths) | a majority of rows negative, or any row below −1.0. Either is a collapse; neither is mere shrinkage |
| S2 | **the advantage is not a private convention between identical fits** | in B7, the off-diagonal (run *i* with run *j*'s partners) is within 1.5 points of the diagonal | the off-diagonal collapses |
| S3 | **no fresh adversary exploits it** | every B4 arm's edge over `FROZEN` has an upper bound below 1.53 | any arm clears 1.53 with a replicated sign |
| S4 | **the gain is attributable** | in B5, no single leave-one-out drop accounts for more than the whole, and the components' naive sum exceeds the measured whole (sub-additive, as phase 2's did at 83%) | the measured whole exceeds the naive sum of its parts, which would mean the attribution is wrong |
| S5 | **it survives the rule dialect** | every B8 row's edge keeps its sign, and no row's edge is more than 2 points below the `default` row | a dialect flips the sign |
| S6 | **the worst case is not catastrophic** | in B3, `FROZEN`'s worst cell over the panel is no worse than `INCUMBENT`'s worst cell | `FROZEN`'s worst cell is worse than the incumbent's |
| S7 | **the instrument is intact** | B9.2 recovers the planted edges, B9.3 does **not** recover the sub-floor rung, and B9.5's three cheats fail exactly the tests B9.5 names | any control misbehaves — in which case nothing else in the battery is interpretable |

A failure of S1, S2 or S6 is a **substantive** failure and the report says v0.7 is brittle in that
named way. A failure of S7 is a **procedural** failure and phase 5 stops.

**S1 took three drafts and all three are recorded, because the third is the one phase 5 applies and
the reader is entitled to know it was not the first thing that came to mind.**

* **Draft 1**, written before phase 4's partner table ran: "the changed-partner delta is within 1.5
  points of the self-play delta". Measured, **neither v0.7 nor v0.6 passes it** — a three-seat
  upgrade is simply worth more than a one-seat upgrade, which is arithmetic, not brittleness.
* **Draft 2**: "at least five of seven changed-partner deltas clear zero". Computing the delta
  intervals properly — each arm's pooled half-width combined in quadrature, which is *conservative*
  because the arms are paired on deals and the harness gives no paired delta across two cells — gives
  **four of seven**, so v0.7 would fail a test v0.6 had never been measured against.
* **Draft 3**: use the incumbent as its own control, comparing v0.7's `min` and median/self ratio
  against `INCUMBENT` − `v05`. **Abandoned not because v0.7 loses it** — although on the rows
  available it does — but because **the baseline is computable on only three of the seven rows**: B6
  as first written ran the `v05` arm at four partner settings and the other two arms at eight, so the
  medians would be taken over different row sets. **The fix is to make the comparison computable
  rather than to drop it: B6 now runs all three arms at all eight settings**, and the comparison is
  *reported* rather than gated, because three rows is not a baseline anyone should gate on.

So S1 as it stands tests **collapse**, which is what the phase brief asks — *"a convention that
collapses in cross-play is brittleness"*.

**Stated in advance so the direction cannot be chosen afterwards: on phase 4's training measurement
the incumbent-baseline comparison goes AGAINST v0.7.** v0.7 over v0.6 has min −0.15, median +1.26,
self +2.94, ratio **0.428**. v0.6 over v0.5, on the three rows where both are currently measurable,
has min −0.61, median +0.64, self +1.35, ratio **0.472**. If phase 5's fuller measurement keeps that
ordering, the honest sentence for the report is *"v0.7's advantage transfers across partners about as
well as v0.6's did, and on the median-to-self-play ratio slightly less well"* — not "it transfers".

Phase 4's training measurement of the v0.7 side (deltas of `FROZEN` − `INCUMBENT`, `v05` opponent):

| partners | itself | `v06` | `v05` | `v04` | `v03` | `detective` | `withholder` | `lockout` |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| delta | +2.94 | +1.26 | +0.80 | +0.14 | +1.61 | +1.43 | −0.15 | +2.08 |
| clears zero | yes | yes | no | no | yes | yes | no | yes |

Seven of the eight rows are positive and the minimum is −0.15, so the collapse test passes; only four
clear zero under the conservative interval, which is why the count-of-significant-rows version was
abandoned.

---

### 5.3 The S6 side-channel rule, and the confound phase 4 removed from it

THREAT-MODEL §6.4 specifies S6 as zero-tolerance: any decision the reconstruction cannot reproduce
from (own hand, public event stream, rules, reset seed) is an offence. **Phase 5 applies it at zero
tolerance.** Getting back to that took most of phase 4's diagnostic effort and the route is set out
here, because the two conditions the rule now carries are not obvious and a phase-5 reader who drops
either of them will measure something else.

**Condition 1: one thread.** Above one thread the test is a lottery. Run alone at 13 threads on one
fixed cell, the same command returns 1, 2, 3 and 4 mismatches on successive invocations and the
*denominator* moves too (270,593 / 270,608). Phase 3 attributed this to `v7side` leaking
state between its own four passes and recorded `--tests=s6` alone at 2 threads as `0/264,075`, twice.
That attribution does not survive — S6 was running alone. At `--threads=1` the test is deterministic.

**Condition 2: fresh agents per deal (`--freshagents`).** With condition 1 alone, every configuration
carrying the truncated search still shows 1–3 irreproducible **ask** decisions per ~270,000, and no
configuration without the search shows any. Phase 4 traced it: **agents are constructed once per
thread and reused across every deal that thread is handed** (`arena.hpp`, `v07_side.hpp`), and some
per-agent state survives `reset()` and is reachable only under the search. The live agent therefore
carries residue from its previous deals that the reconstruction's fresh agent does not.

Two independent demonstrations, neither of which involves the side-channel harness reading anything
about seats:

* **Rebuilding the agents per deal takes every mismatching cell to exactly zero**, on every cell
  tested: the frozen configuration 1/270,593 → **0**/270,628; `F-cheap` 1/264,061 → **0**/264,075;
  the phase-2 composite 3/272,402 → **0**/272,390. Note the middle one: **264,075 is exactly the
  denominator phase 3 recorded for the one `F-cheap` run it believed was clean.** That run was not
  clean by luck; it was a deal partition under which the residue happened not to bite.
* **The same residue makes a searching configuration's play depend on the deal partition, and hence
  on the thread count.** Two spec forms of one identical policy — the baked `V6PARAMS` and an
  `allparams=` string of the same numbers — play identically at 2, 3, 6, 8 and 13 threads and differ
  on one deal in 800 at 1 and 4 threads. With `--freshagents` they are identical at every thread
  count. With the search removed they are identical at every thread count without it.

**What this is and is not.** It is **not** a channel between seats: it is one agent instance carrying
its own past. It is **not** the rollout blueprints' accumulated `Knowledge` — `rreset=1`, phase 3's
fix for exactly that, changes the counts by nothing. It **is** cross-game memory in the sense S6
defines, and it is a real defect in the engine's reuse of agents, of a size that moves one deal in
800 for a searching configuration and nothing at all for a blueprint.

**Two consequences phase 5 must carry.**

1. **The gate rule is zero tolerance again**, and `engine/gate_v07.sh` measures S6 at `--threads=1
   --freshagents`. Under those conditions every configuration in this corpus — the incumbent, the
   incumbent's frontier, phase 3's survivor and the frozen configuration — measures **exactly zero**.
   A nonzero count in phase 5 is therefore a genuine finding and not the residue.

   **The rule tests a mode no scored cell uses, and that has to be said in the report.**
   `--freshagents` changes play — it is what takes `F-cheap`'s audited denominator from 264,061 to
   264,075 — so G7b certifies "with the residue removed, nothing else depends on anything it should
   not". The residue itself is **not** removed from the shipped mode; it is a named, quantified
   engine defect (1–3 ask decisions per ~270,000, one deal in 800 of play) that every strength number
   in this corpus is measured under. Phase 6 must not write "the frozen configuration passes S6"
   without that clause.
2. **It corrects `CANDIDATES.md` C14 on two counts.** "It is `v7side` leaking state between its own
   four passes" is not what is happening, and "`fish7 match` is bit-stable across thread counts for
   both search and non-search play" is **false in general** — it holds for the cells phase 3 tested
   and fails for a searching configuration played against itself. Phase 5 reports every searching
   cell's thread count with the cell, and the preregistered batteries fix it at 13.

**What is still unknown**, and phase 5 is not asked to find it: *which* state survives `reset()`. The
remaining suspects are the thread-local `BlockDP::buffers()` / `generation()` pool and
`Belief::buffers()`, both shared by every agent on a thread. The decisive experiment is to reset those
pools per deal and re-run the thread sweep; it is cheap and it is phase 6's or a maintenance task's,
not phase 5's.

---

## 6. What result would mean v0.7 is not an advancement

Stated now, in advance, because after the numbers are in every one of these has a reading that makes
it sound like something else.

1. **The frontier cell.** If `FROZEN`'s pooled edge over `F-cheap` has an interval containing zero,
   or the sign does not replicate across `7090001` and `7090002`, **v0.7 is not an advancement over
   the v0.6 frontier**, whatever it does against the deployed policy. Beating `v06` is not the claim:
   phase 2 already showed that four separate one-switch deviations of the incumbent beat it, and the
   deployed policy ships its search off.

2. **The composite cell.** If `FROZEN` does not beat the phase-2 composite, then what phase 3 called
   "K3's four keys on top of phase 2's composite, +1.42 [+0.18, +2.68] on one bank" did not replicate,
   and v0.7 reduces to the phase-2 composite — which is a phase-2 result, not a v0.7 one. The report
   must then say that the v0.7 contribution is the *measurement programme* and the four closed
   ledger entries, not the policy.

3. **Attribution.** If B5's leave-one-out lattice cannot locate the gain — if removing every single
   component in turn costs less than a third of the whole, so that the whole is not the sum of any
   identifiable parts — then v0.7 has a number it cannot explain, and the phase-4 brief's own
   standard ("v0.7 must not headline a gain it cannot attribute") says it must not headline it. That
   is a failure even if the headline number is large.

4. **Brittleness.** If the advantage over the incumbent is present in the self-play row of B6 and
   absent under changed partners, or present on the diagonal of B7 and absent off it, then what was
   measured is a self-play convention. The target configuration is three copies, so this does not
   make v0.7 useless — but the report must say it has a convention and not a strength gain, and it
   must lead with that.

5. **A fresh exploit.** If any B4 arm clears 1.53 points against `FROZEN` with a replicated sign,
   then v0.7 closed nothing and opened something, and the exploit is the headline.

6. **The gate.** If `FROZEN` fails B1, there is no v0.7. This is the one condition with no reading
   that softens it.

7. **The controls.** If B9.3 "detects" the sub-floor planted edge, or B9.2 fails to recover the
   supra-floor ones, or a positive-control cheat fails the wrong test, the instrument is not the
   instrument phase 1 and phase 2 characterised and **no number in this battery may be reported as a
   measurement.**

**And the honest all-negative path is a legitimate deliverable.** If phase 5 returns "no advancement
over the frontier", the v0.7 report is a barrier report: the four ledger entries phase 3 closed with
measurements (L1 at exactly zero; L5 killed as an objective and confirmed as an instrument; C1′
closed in the negative; the inherited leaf conditional discharged), the two corrections to the
corpus's own record (`F-cheap` costs ~3.2× the blueprint and not 242×; the search's per-decision
selection signal is ~84% winner's curse), the mechanical side-channel gate with its three calibrated
positive controls, and the S6 residual this document commits in §5.3. That is a real contribution and
the report should not apologise for it.

---

## 7. Deviations

Phase 5 records a deviation for anything it does that this document does not specify, including:
a cell it could not afford and dropped; a cell it added; a threshold it read differently; a command
whose flags differ from the ones written here; a bank digest that did not match; any use of
`FISH_UNSEAL_PHASE` beyond the single documented setting.

Deviations are reported **in the results document, next to the affected result**, not in a footnote
and not only in the log. A battery with three recorded deviations and honest numbers is worth more
than one that silently matched its own protocol.

If phase 5 finds a genuine flaw in this protocol — a cell that cannot answer the question it is
posed, a threshold that is incoherent, a control that does not control — it **stops and reports the
flaw**. It does not amend and continue. An amended protocol is a training run, and continuing under
one would mean the holdout material has been used to choose a protocol, which is the one thing the
seal exists to prevent.
