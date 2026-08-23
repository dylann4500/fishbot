# P3 verification — "over-weighting the prior is the real exploit surface"

Dylan Nguyen, FishLab Research Project
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`, engine rebuilt from
unmodified sources (`cd engine && make`). No file was modified for this verification.

**Claim under test** (from `research/v05/results/P3-deception.md` §0 and §5):
> Over-weighting the prior is the real exploit surface: doubling it costs 5.25 points [2.45, 8.05]
> paired, and quadrupling it loses the match outright to an archetype that is itself 17 points
> weaker than v0.4.

**Verdict: the numbers reproduce; the causal framing does not.** Over-weighting `priorTheta`
damages v0.4 by the same amount against an honest weak opponent (`v03`) as against the deceptive
`withholder:k=6`. It is generic self-inflicted mis-calibration of the declaration rule, not a lever
a deceiver pulls.

---

## 1. What reproduced

All runs below use **seed 555777999** for `match` and **seed 987654321** for `ablate` — different
from the originals (20260823 and 515151).

### 1a. Doubling the prior, paired

```
./fish ablate --ref=v04 --variants="v04:ptheta=0.5276,pphi=0.2656;v04:ptheta=0,pphi=0" \
              --panel="v04,silent:tol=0.10,feint,withholder:k=6" --games=250 --seed=987654321
```

| variant | orig (seed 515151) | replication (seed 987654321) |
|---|---|---|
| `ptheta=0.5276,pphi=0.2656` (2×) | +5.25 [2.45, 8.05] | **+6.15 [3.35, 8.90]** |
| `ptheta=0,pphi=0` (0×) | +4.40 [1.60, 7.15] | **+7.35 [4.60, 10.05]** |

The 2× loss reproduces and is if anything larger. **But the relative ordering flips**: at the new
seed *deleting* the prior costs more than doubling it. Both intervals overlap heavily in both runs.
"Over-weighting is the *real* exploit surface" — i.e. that up is worse than down — is not
supported at 2×/0×. P3's own body text ("doubling the prior is as costly as deleting it") is the
accurate statement; the headline sentence is not.

### 1b. Quadrupling, and the 17-point gap

| A | vs `withholder:k=6`, orig seed 20260823 | replication, seed 555777999 |
|---|---|---|
| shipped v0.4 | 69.00 [65.3, 72.7] | 65.17 [61.3, 69.0] |
| s=4 `ptheta=1.0552,pphi=0.5312` | **45.67 [41.5, 49.8]** | **44.50 [40.3, 48.7]** |
| s=0 `ptheta=0,pphi=0` | 60.67 [56.5, 64.7] | 61.00 [57.0, 65.0] |
| theta-only `ptheta=1.0` | 51.67 [47.5, 55.8] | 50.67 [46.5, 54.8] |

s=4 loses outright at both seeds (CI excludes 50). The theta-only cell is a coin flip at both
seeds, not a loss — the claim quoted it correctly as 51.67% but it is not evidence of losing.

The "17 points weaker" gap reproduces exactly: at seed 555777999, `v04` vs `v03` = 77.00%
[73.3, 80.5]; `withholder:k=6` vs `v03` = 59.67% [55.7, 63.5]. Gap **17.3 points**.

---

## 2. What did not hold: the effect is not deception-specific

The claim's rhetorical force comes from pairing the collapse with a *deceptive* opponent. Running
the same over-weighted arm against honest opponents at the same seed removes that pairing.

Seed 555777999, 300 deals = 600 games per cell. Δ is in log-odds, which is the scale on which a
fixed handicap should be constant regardless of how strong the opponent is.

| opponent | shipped v0.4 | s=2 | Δ logit | s=4 | Δ logit |
|---|---|---|---|---|---|
| `v03` (honest, weak) | 77.00 | 72.67 | −0.230 | 58.33 | **−0.872** |
| `detective` (honest) | 73.33 | 70.83 | −0.124 | 59.83 | −0.613 |
| `lockout` (honest) | 80.50 | — | — | 66.33 | −0.740 |
| `v04` (honest mirror) | 50.00 | 44.33 | −0.228 | 39.17 | −0.440 |
| `withholder:k=6` (deceptive) | 65.17 | 59.00 | −0.263 | 44.50 | **−0.847** |

At s=4 the handicap against the **honest** `v03` (−0.872) is *larger* than against the deceptive
`withholder` (−0.847). At s=2 the four cells span −0.124 to −0.263 with no deception ordering. The
withholder cell is not special.

**Why it nevertheless "loses the match outright" only to the withholder:** arithmetic, not
deception. Shipped v0.4 starts at 65% against the withholder and 77% against v0.3, so an equal
generic handicap crosses 50% in the withholder cell first. The withholder is simply the strongest
opponent in the panel from v0.4's point of view (65% head-to-head vs 77% against a bot that is 17
points weaker on the common v0.3 yardstick — a plain non-transitivity, and the reason quoting the
17-point gap makes the result sound more surprising than it is).

## 3. The actual mechanism: the declaration rule, not the opponent model

`V04Agent::bestGuess` (`engine/src/v04.hpp:790-791`) sets its confidence with
`bel.jointSequential(k, cards, players, SETSZ, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta,
cfg.priorPhi)`, and the stopping rule at `engine/src/v04.hpp:686` fires on
`v.pAlloc >= cfg.declThreshold` (0.81770, `v04.hpp:93`). `Knowledge::priorWeight`
(`engine/src/belief.hpp:100-108`) multiplies the deal weight by `exp(theta*a - phi*other)` clipped
at ±2.6, so scaling theta inflates `pAlloc` on unchanged evidence. The threshold then fires on
allocations that are wrong. Measured, at the same seed:

| A | vs `v03` declAcc | vs `withholder:k=6` declAcc |
|---|---|---|
| shipped v0.4 | 0.987 | 0.898 |
| s=2 | 0.949 | 0.831 |
| s=4 | **0.836** | **0.709** |

Declarations per game rise at the same time (5.24 → 5.63 against the withholder). This is a
confidence-calibration failure that is present against every opponent; the deceiver contributes
nothing to it beyond already being a harder opponent.

## 4. Bottom line

* +5.25 [2.45, 8.05] paired for the 2× prior: **reproduced** (+6.15 [3.35, 8.90] at a new seed).
* s=4 loses to `withholder:k=6`: **reproduced** (44.50% [40.3, 48.7]).
* 17-point gap on the v0.3 yardstick: **reproduced** (17.3).
* "The real exploit surface" / attribution to deception: **does not hold.** The same over-weighting
  costs the same in log-odds against honest `v03`, and the asymmetry against deleting the prior
  does not survive a seed change.
* Correct statement: v0.4's declaration confidence is not robust to the scale of its policy prior —
  a 4× prior costs ≈0.6–0.9 log-odds against *every* opponent tested and drops declaration accuracy
  from 0.99 to 0.84 — and the shipped scale sits inside a broad flat optimum. Nothing here is an
  exploit an opponent can trigger, because no opponent action changes `priorTheta`.
