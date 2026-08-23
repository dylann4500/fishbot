# P7 — adversarial verification: compiled `vw` vs the E14-fitted `vw`

Verifier: independent re-run at seeds disjoint from the original report.
Target claim (from `research/v05/results/P7-valuefn.md` §1.2):

> The hypothesis that the shipped coefficients cost strength did NOT hold: the compiled
> vector beats the E14-fitted vector against every opponent, **worst in mirror play**.

**Verdict: the main claim HOLDS UP. The "worst in mirror play" rider does NOT.**

---

## 1. Code checks (done before any run)

- Compiled vector, `engine/src/v04.hpp:111-127` — matches the report's "compiled `vw`"
  column digit for digit (bias 0.001242 … turn×unresolved −0.021409).
- E14 vector, `research/v04/results/E14-valuefit.txt` — single line, matches the report's
  "E14 fit" column digit for digit.
- The override path is real and does what the report assumes:
  `engine/src/factory.hpp:79-83` splits `vweights=` on `|` and writes `a->cfg.vw[i]`;
  `parseOpts` (`factory.hpp:20`) splits options on `,`, so a `|`-delimited value is not
  mangled. `V04Agent::value` (`v04.hpp:401`) is the only consumer.
- `engine/freeze_config.py` — confirmed it rewrites `double w[NFEAT]` and 14 named scalars
  and never `vw`, so the documented gap is genuine.
- The mirror row is a legitimate matched comparison, not an artefact: `ablate`
  (`engine/src/main.cpp:434-479`) runs the reference arm as `v04` vs `v04` (exactly 0.5 by
  seat symmetry) and the variant arm as `E14-v04` vs `compiled-v04`, i.e. a direct
  head-to-head; the paired bootstrap resamples deals with `rot` games per cluster.

## 2. Independent re-run, seed 424242 (original used 7788991)

`./fish ablate --ref=v04 --variants="v04:vweights=<E14>" --panel=<opp> --games=500
 --rotations=6 --seed=424242`, one run per opponent. Δ = compiled − E14.

| opponent | Δ, orig (seed 7788991) | Δ, mine (seed 424242) | 95 % CI, mine |
|---|---|---|---|
| v04 (mirror) | +0.0290 | **+0.0133** | **[−0.0010, +0.0280]** — includes 0 |
| v03 | +0.0203 | +0.0360 | [+0.0200, +0.0520] |
| lockout | +0.0180 | +0.0183 | [+0.0037, +0.0333] |
| detective | +0.0223 | +0.0220 | [+0.0053, +0.0387] |
| v02 | +0.0187 | +0.0207 | [+0.0057, …] |
| diversifier | +0.0323 | +0.0480 | [+0.0360, …] |
| hunter | +0.0167 | +0.0197 | [+0.0117, …] |
| bluffer | +0.0010 | +0.0020 | [+0.0007, …] |
| random | +0.0033 | +0.0023 | [+0.0007, …] |

9/9 opponents favour the compiled vector at the new seed; 8/9 CIs exclude zero. The
sign never flips. **The negative result replicates.**

Third mirror seed, larger n: `--panel=v04 --games=600 --rotations=6 --seed=13579246`
→ Δ = **+0.0253**, CI [+0.0119, …], excludes zero. Mirror across three independent
banks: +0.0290 / +0.0133 / +0.0253.

## 3. Where the report overreaches

"The largest loss is in mirror play" is wrong on the report's **own** table — diversifier
is +0.0323 vs mirror +0.0290 — and it reverses at my seed, where the mirror (+0.0133) is
the *smallest* effect among the eight non-degenerate opponents and the only CI that
includes zero, while diversifier is +0.0480 and v03 +0.0360.

Also worth flagging for anyone quoting the per-opponent CIs: v03 was +0.0203 [+0.0050,
+0.0360] at one seed and +0.0360 [+0.0200, +0.0520] at another — the intervals barely
overlap. The deal-cluster bootstrap under-covers seed-to-seed variation, so the honest
per-opponent statement is "1–5 win-rate points, sign stable", not the reported "1.7–3.2".

## 4. The report's mechanism reproduces cleanly

The report attributes the loss to the collapsed control block (`w2 + w6` = 0.8435 compiled
vs 0.2297 in E14) de-scaling the expectimax term relative to `linearWeight`. I tested that
directly with a hybrid vector — E14 everywhere except `v2` and `v6` restored to the
compiled values — against v03, 300 deals × 6 rotations, seed 424242:

| arm | win rate | Δ from compiled | 95 % CI |
|---|---|---|---|
| E14 as fitted | 0.7178 | +0.0417 | [+0.0206, +0.0628] |
| E14 with compiled `v2`, `v6` | 0.7528 | **+0.0067** | **[−0.0089, +0.0217]** |

Two of sixteen coefficients account for ~84 % of the gap, and restoring them makes the
difference statistically indistinguishable from zero. This supports the report's
conclusion that "paste the fitted numbers in" is the wrong repair — the 34 CEM-fitted
policy parameters are co-adapted to the compiled vector's scale, not to its direction.

## 5. Bottom line

- Compiled beats E14 against every opponent tested, at two independent seed banks: **holds**.
- "Worst in mirror play": **does not hold**. Mirror is mid-pack at one seed and the weakest
  effect at another; diversifier is the largest loss at both.
- Corrected magnitude: mirror Δ ≈ +0.022 (range +0.013 … +0.029 over three banks);
  worst opponent is diversifier at +0.032 … +0.048.
