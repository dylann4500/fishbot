# P3 — adversarial verification of the deception finding

Dylan Nguyen, FishLab Research Project
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19` (working tree).
Engine rebuilt from clean: `cd engine && make` → `./fish`. No protected header was modified;
no new file was added by this verification.

**Claim under test.** "The brief's candidate headline is refused: the policy-agnostic posterior
(`v04:ptheta=0,pphi=0`) is NOT more robust against deceptive opponents — it is uniformly worse."

**Verdict: HOLDS UP, with two corrections.** The direction is real and survives three new seeds.
"Uniformly" is overstated at the per-cell level, and the whole effect is confined to the default
Sinkhorn (`belief=fast`) belief — under the exact combinatorial belief (`belief=block`) it
disappears and the point estimate flips.

---

## 1. Mechanism check (code, not numbers)

`Knowledge::priorWeight`, `engine/src/belief.hpp:104`:

```cpp
if (theta == 0 && phi == 0) return 1.0;
```

`BeliefState::sinkhornDisj` (`engine/src/belief.hpp:478-485`) seeds every unresolved
(card, player) cell with `kk.priorWeight(...)`, so `ptheta=0,pphi=0` really does initialise the
Sinkhorn iteration uniformly over the consistency mask — it is the policy-agnostic posterior, as
claimed. Spec parsing is correct: `parseOpts` splits on `:` then `,` (`engine/src/factory.hpp:12-25`)
and `factory.hpp:69-70` writes both scalars. The default belief mode is `Fast`
(`v04.hpp:52`), which routes through `sinkhornDisj` at `v04.hpp:189`.

Important scoping fact the original report does not state: in `BeliefMode::Block`
(`v04.hpp:167-176`) the priors are consulted **only on the fallback path** when `block.build(k)`
fails. The exact belief is already policy-agnostic. §4 exploits this.

## 2. The cited numbers are genuine

Re-ran four of the cited head-to-head cells at the cited seed (`--seed=20260822 --games=400
--rotations=2`). Bit-for-bit reproduction of `research/v05/runs/P3-winrate.txt`:

| cell | reported | reproduced |
|---|---|---|
| `v04` vs `feint` | 0.54000 | 0.54000 |
| `v04:ptheta=0,pphi=0` vs `feint` | 0.47625 | 0.47625 |
| `v04` vs `withholder:k=6` | 0.67750 | 0.67750 |
| `v04:ptheta=0,pphi=0` vs `withholder:k=6` | 0.62875 | 0.62875 |

## 3. Independent replication at new seeds

### 3a. Same mixed panel, new seed (`--seed=777333 --games=300 --rotations=2`, 2400 games/arm)

```
ref v04                     0.55792   per-opponent [0.50000, 0.52833, 0.52333, 0.68000]
v04:ptheta=0,pphi=0         0.52667   delta 0.03125  ci [0.00625, 0.05625]
                                      per-opponent [0.41333, 0.52833, 0.54000, 0.62500]
v04:pphi=0                  0.56625   delta -0.00833 ci [-0.03333, 0.01667]
v04:ptheta=0                0.54458   delta  0.01333 ci [-0.01125, 0.03750]
```

Aggregate direction holds (worse by **3.13** points, CI excludes 0), smaller than the reported
4.40. `pphi=0` is again free — that secondary finding replicates cleanly.

**But the per-cell picture does not.** Against `feint` the ablation was **better** (54.00 vs
52.33) and against `silent:tol=0.10` it was **exactly tied** (52.83 vs 52.83). The aggregate at
this seed is carried by the honest mirror cell (−8.67) and the Withholder (−5.50).

### 3b. Per-opponent paired ablations (`--seed=20260823 --games=500 --rotations=2`, 1000 games/cell/arm)

| opponent | `v04` | `ptheta=0,pphi=0` | ablation worse by | paired CI |
|---|---|---|---|---|
| `v04` (honest mirror) | 0.50000 | 0.44700 | **5.30** | [2.30, 8.30] |
| `silent:tol=0.10` | 0.55400 | 0.51400 | 4.00 | [−0.30, 8.30] |
| `feint` | 0.51900 | 0.48200 | 3.70 | [−0.50, 7.90] |
| `withholder:k=6` | 0.66400 | 0.63100 | 3.30 | [−0.60, 7.30] |
| `silent` (uncapped) | 0.77700 | 0.75500 | 2.20 | [−1.50, 5.80] |

All five point estimates negative for the ablation, but **not one deceptive cell is individually
significant** at 1000 games. The only significant cell is the *honest* mirror. Note the ordering:
the prior's value is largest against the honest opponent (5.30) and smallest against the most
deceptive one (2.20) — the prior loses roughly 40% of its value under deception but never inverts,
which is exactly what the original report's §0 point 2 says.

### 3c. Deception-only panel, large N (`--seed=31415926 --games=500 --rotations=2`, 4000 games/arm)

Panel = `silent, silent:tol=0.10, feint, withholder:k=6` — the honest mirror removed, so this is
the cleanest test of the claim as worded.

```
ref v04                0.63725   per-opponent [0.79600, 0.57500, 0.51900, 0.65900]
v04:ptheta=0,pphi=0    0.59125   delta 0.04600  ci [0.02625, 0.06575]
                                 per-opponent [0.74900, 0.53200, 0.47500, 0.60900]
```

Worse by **4.60** points [2.63, 6.58] against deceivers only, all four cells worse. This is the
number the claim should have quoted; the reported 4.40 came from a panel one quarter of which was
the honest mirror.

### Sign census over all measured deception cells

14 (seed × archetype) cells across seeds 515151 (original file), 777333, 20260823, 31415926:
**12 worse, 1 tie, 1 reversed.** Sign test on the 13 non-ties: p ≈ 0.003. The aggregate claim is
supported; the word "uniformly" is not, at the resolution the per-cell runs actually have
(±4 points at 800–1200 games).

## 4. The correction that matters: the effect is Sinkhorn-specific

Because `BeliefMode::Block` ignores `priorTheta`/`priorPhi` except on build failure
(`v04.hpp:167-176`), the same ablation under the exact belief isolates whether the priors are
opponent modelling or approximation repair.

`--ref=v04:belief=block --variants=v04:belief=block,ptheta=0,pphi=0`, deception-only panel:

| run | panel | games/arm | delta (ablation worse by) | CI |
|---|---|---|---|---|
| seed 5150, 150 deals | 3 archetypes | 900 | **−2.22** (ablation better) | [−6.22, +1.78] |
| seed 606061, 350 deals | 4 archetypes | 2800 | **−1.04** (ablation better) | [−3.21, +1.18] |

Under the exact combinatorial posterior the policy prior is worth **nothing measurable against
deceivers**, and the point estimate is on the ablation's side in both runs. The 4.6-point cost
measured in §3c therefore does not show that the soft policy prior carries opponent-policy
information; it is consistent with the prior compensating for Sinkhorn approximation error in the
default `Fast` mode. Any v0.5 decision to keep `ptheta`/`pphi` should be scoped to the approximate
belief.

## 5. Secondary observations

- The ablation degrades the P0 deadlock, not just the win rate: at seed 20260822 vs `feint`,
  `lockHoldA` 8.80 → 18.72 events and `eventsPerGame` 138.2 → 159.5. The loss looks like a general
  policy degradation (slower, more locked-half-suit dithering), not a deception-specific failure.
- On a *relative* robustness metric — drop from the vs-`v03` baseline — the two policies are
  indistinguishable (seed 20260822): `v04` drops 21.0 / 18.9 / 7.3 points against
  feint / capped-silent / withholder; the ablation drops 21.4 / 17.5 / 6.1. So "not more robust"
  is a statement about absolute win rate; the ablation is not measurably *less* robust in the
  degradation sense. The claim is correct as written but should not be read as "the prior protects
  against deception".
- Untested confound, stated for the record: the other 34 v0.4 parameters were tuned jointly with
  `ptheta`/`pphi` (`main.cpp:210`, `factory.hpp:112-113`). Zeroing two tuned coefficients without
  retuning the rest is a lower bound on what a genuinely policy-agnostic policy could achieve.

## 6. Commands

```
cd engine && make
./fish match --a=v04 --b=feint --games=400 --rotations=2 --seed=20260822 --threads=14 --json
./fish ablate --ref=v04 "--variants=v04:ptheta=0,pphi=0;v04:pphi=0;v04:ptheta=0" \
  "--panel=v04,silent:tol=0.10,feint,withholder:k=6" --games=300 --rotations=2 --seed=777333 --threads=14
./fish ablate --ref=v04 "--variants=v04:ptheta=0,pphi=0" "--panel=<one opponent>" \
  --games=500 --rotations=2 --seed=20260823 --threads=14
./fish ablate --ref=v04 "--variants=v04:ptheta=0,pphi=0" \
  "--panel=silent,silent:tol=0.10,feint,withholder:k=6" --games=500 --rotations=2 --seed=31415926 --threads=14
./fish ablate "--ref=v04:belief=block" "--variants=v04:belief=block,ptheta=0,pphi=0" \
  "--panel=silent:tol=0.10,feint,withholder:k=6,silent" --games=350 --rotations=2 --seed=606061 --threads=14
```
