# P6 verification — cost of the information-safety constraint on declaration arbitration

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, at commit `fe21e19`.

**Claim under test (from `research/v05/results/P6-declaration.md` §1.4/§1.6):** lowest-seat
arbitration costs only +0.30 pp of win rate against an information-unsafe confidence-ranked upper
bound; the constraint at `engine/src/game.hpp:205-209` is essentially free.

**Verdict: HOLDS UP.** Direction, mechanism and order of magnitude all reproduce. The one
correction is that +0.30 pp is the *low* draw of the seed distribution, not a typical one: pooled
over five seeds the cost is **+0.37 pp**, range +0.28 to +0.45 pp across seeds.

---

## 1. Code claims — verified by reading

* `engine/src/game.hpp:223` is exactly
  `if (bestSeat < 0) { bestConf = conf; bestSeat = p; bestDecl = d; }`.
  `conf` is written into `bestConf` and `bestConf` is thereafter only passed to
  `applyDeclaration(...)` (`game.hpp:226`) as a reported diagnostic. It is never compared against
  another seat's `conf`. The claim's characterisation is exact.
* `engine/src/game.hpp:211-214` fixes the scan order from `rules.declArbitration`
  (0 lowest seat, 1 highest, 2 from the turn-holder); the first proposer in that order wins.
* The rationale is at `engine/src/game.hpp:205-209` as cited, and is repeated at
  `engine/src/fish.hpp:115-119` ("Ranking by stated confidence is deliberately absent: it would
  compare private confidences across seats"). It is also flagged in `docs/FISHBOT_V04.md`
  ("Known gaps") and in `docs/V04_FINDINGS.md:52`. So this is a documented deliberate choice, not
  an undiscovered bug — which is how the claim frames it.

## 2. Design audit of the paired mirror duplicate

`probe_declaration.hpp:36-84` plays each deal twice with the treatment on team 0 then on team 1 and
records `xw ∈ {0,1,2}` per deal (`:76`), bootstrapped by deal cluster (`arena.hpp:151`).
The zero-variance control claim is structurally sound (when treatment == control the two
orientations are the same game, so `xw = 1` always) and I reproduced it in my own driver:

```
{"x":0,"y":0,"seed":424242,"deals":1000,"games":2000,"xWinRate":0.500000,"ci":[0.500000,0.500000]}
```

Two scope caveats, neither of which falsifies the claim:

* `PGame` resolves *cross-team* races with a deterministic coin
  (`probe_declaration_game.hpp:270-273`), whereas `Game` resolves them by global lowest seat. This
  is applied identically in both arms, so it isolates the within-team effect; it does mean the
  measured number is the within-team component only.
* Mode 3 (`probe_declaration_game.hpp:190-195`) breaks confidence ties by lowest seat
  (`conf[p] > bc + 1e-12`), so it is strictly "lowest seat, reordered by confidence". That is the
  right operationalisation of the claim.

## 3. Reproduction of the cited number, and four fresh seeds

`clang++ -std=c++20 -O3 -march=native src/probe_declaration_main.cpp -o probe_decl -pthread`
then `./probe_decl arb --games=3000 --x=3 --y=0 --seed=<S>` (3,000 deals / 6,000 games each).

| seed | X = confidence-ranked win rate | 95% CI | Δ vs lowest seat |
|---|---|---|---|
| 31 (the cited run) | **0.503000** | [0.501333, 0.504833] | +0.30 pp |
| 777001 | 0.503833 | [0.501833, 0.505833] | +0.38 pp |
| 20260822 | 0.504333 | [0.502500, 0.506333] | +0.43 pp |
| 90210 | 0.504500 | [0.502500, 0.506500] | +0.45 pp |
| 424242 | 0.502833 | [0.500833, 0.505000] | +0.28 pp |
| **pooled (15,000 deals / 30,000 games)** | **0.503700** | ≈ ±0.0008 | **+0.37 pp** |

Seed 31 reproduces bit-for-bit against `research/v05/runs/P6/A_conf.json` (including every race
count: 29,750 races, 4,961 contested, 1,218 / 1,415). An independently written driver
(`scratchpad/vdeclarb_main.cpp`, not sharing `probe_declaration.hpp`'s `runArb`) also returns
`0.503000` at seed 31, so the number is not an artefact of the reporting path.

The sign is the same at every seed and the pooled interval clears 50% comfortably: the effect is
real, positive, and small. The claim's headline is a **slight understatement**, not an overstatement.

## 4. A stronger control the original study did not run: oracle arbitration

Confidence ranking is only *one* unsafe rule. To bound what *any* within-team arbitration rule
could buy, I added mode 5 to a copy of the probe driver
(`engine/src/probe_vdeclarb_game.hpp:200-211`, namespace `fish::probe2`): pick a proposing seat
whose named allocation is **actually true** against the hidden hands, else the lowest seat. Same
paired mirror duplicate, 3,000 deals / 6,000 games:

| seed | oracle arbitration | 95% CI | Δ | confidence ranking at the same seed |
|---|---|---|---|---|
| 31 | 0.502667 | [0.501500, 0.504000] | +0.27 pp | +0.30 pp |
| 90210 | 0.504500 | [0.502833, 0.506333] | +0.45 pp | +0.45 pp |
| 424242 | 0.503333 | [0.501833, 0.505000] | +0.33 pp | +0.28 pp |
| pooled | 0.503500 | — | **+0.35 pp** | +0.34 pp on the same three seeds |

**A clairvoyant arbitrator is worth the same +0.35 pp as confidence ranking.** Arbitration is
saturated: confidence ranking already extracts essentially everything the arbitration channel
contains, and the total value of the channel — safe or unsafe, informed or omniscient — is about a
third of a percentage point. This is a strictly stronger form of the claim than the one made, and
it is consistent with the original study's ground-truth counts (`A_conf.json`: an oracle would win
`bothRight + confOnly + lowOnly` = 1,173 + 242 + 45 = 1,460 of 4,961 contested races vs 1,415 for
confidence and 1,218 for lowest seat — 70.6% of contested races are lost by both candidates).

## 5. Failure modes ruled out

| failure mode | checked |
|---|---|
| statistic means something else | No. `xWinRate` is the treated team's share of 2·deals games, treatment alternated across both orientations of every deal; control returns exactly 0.5 with zero width. |
| effect vanishes at another seed | No. Positive at 5/5 seeds, 0.28–0.45 pp. |
| magnitude overstated | No — mildly *understated*. Pooled +0.37 pp vs the cited +0.30 pp. |
| the "defect" is deliberate and documented | Yes, and the claim says so. `game.hpp:205-209`, `fish.hpp:115-118`, `docs/FISHBOT_V04.md`, `docs/V04_FINDINGS.md:52`. |
| confidence ranking is a weak upper bound, so the constraint might really be expensive | Tested and rejected: a ground-truth oracle arbitrator gains the same +0.35 pp. |
| a probe-specific artefact | No. Reproduced in a second, independently written driver. |

Not tested here (out of scope of the claim, which is explicitly scoped to mirror play): whether the
arbitration cost is larger against non-mirror opponents. `research/v04/results/E17-arbitration.jsonl`
is the only evidence there and it covers the safe orders only.

## Files

* `engine/src/probe_vdeclarb_game.hpp` — copy of `probe_declaration_game.hpp` in namespace
  `fish::probe2`, plus arbitration mode 5 (ground-truth oracle) and a `declNow` member
  (`:75`, `:244`) so `pickWithin` can see the round's proposals.
* `/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/22257a04-aa58-47f0-b0e0-e93f08cd9260/scratchpad/vdeclarb_main.cpp`
  — independent paired-duplicate driver.
  Build: `clang++ -std=c++20 -O3 -march=native -I src <that file> -o vdeclarb -pthread` from `engine/`.

## Reproduction

```
cd engine
clang++ -std=c++20 -O3 -march=native src/probe_declaration_main.cpp -o probe_decl -pthread
for s in 31 777001 20260822 90210 424242; do ./probe_decl arb --games=3000 --x=3 --y=0 --seed=$s; done
clang++ -std=c++20 -O3 -march=native -I src <scratchpad>/vdeclarb_main.cpp -o vdeclarb -pthread
./vdeclarb --games=1000 --x=0 --y=0 --seed=424242      # control, exactly 0.5
for s in 31 90210 424242; do ./vdeclarb --games=3000 --x=5 --y=0 --seed=$s; done   # oracle bound
```
