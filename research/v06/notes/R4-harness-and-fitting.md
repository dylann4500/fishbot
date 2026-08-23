# R4 — Anatomy of the FishLab experiment + fitting harness, and what v0.6 must add

Recon only. No file under `engine/src/`, `paper/` or `docs/` was modified.
Machine: Apple M5 Pro, 15 logical cores, 24 GiB. Binary: `engine/fish`, built by
`engine/Makefile:1-8` (`clang++ -std=c++20 -O3 -march=native -funroll-loops
-fno-math-errno`, `-pthread`). `make` reported *nothing to be done* — the committed
binary is current with `src/`. Repo at commit `bd812fe` ("v0.5"), tree clean at start.

---

## 1. Every `./fish` subcommand

Dispatch is a flat `if (cmd == "...")` chain in `engine/src/main.cpp:111-1149`.
Two argument helpers only: `argVal` (`main.cpp:28-33`, `--key=value`) and `argFlag`
(`main.cpp:34-38`, bare `--key`). **There is no unknown-flag detection** — a typo'd
flag is silently ignored and the default is used. `--help`/anything unmatched falls
through to a two-line usage string (`main.cpp:1147-1149`) that documents only
`match|verify|matrix|bench|serve`; the other 30 commands are undocumented by the binary.

### Global flags (accepted by every command)

| flag | where | effect |
|---|---|---|
| `--threads=N` | `main.cpp:112` | `0` ⇒ `std::thread::hardware_concurrency()` (15 here), `arena.hpp:58` |
| `--arb=low\|high\|turn` | `main.cpp:41-42` | `Rules::declArbitration` 0/1/2 |
| `--legacy` | `main.cpp:43-49` | v0.3 dialect: no out-of-turn declare, no cardless declare, `maxAsks=360`, 2-rung forced ladder |
| `--sets=N` | `main.cpp:50` | `Rules::deckSets`, default 9 |
| `--maxasks=N` | `main.cpp:51` | safety valve, default 400 (`fish.hpp:110`) |
| `--no-out-of-turn`, `--no-cardless-declare` | `main.cpp:52-53` | individual dialect switches |

### The commands

Defaults shown as parsed. `(E…)` = the battery step that uses it.

| cmd | `main.cpp` | flags (with defaults) | output | used by |
|---|---|---|---|---|
| `match` | 114 | `--a=v04 --b=v03 --games=1000 --seed=20260821 --rotations=2 --audit --json` | `printMatch` (`main.cpp:60-110`): human block, or one-line JSON with `deals, games, winRateA, ci, wilsonCI, meanSets{A,B}, askAcc*, declAcc*, declPerGame*, forcedAcc*, forcedPerGame*, outOfTurn*, lockHold*, eventsPerGame, limitHitRate, auditViolations, auditChecks, seconds` | v0.5 E3/E4/E5/E7/E8; v0.4 E3/E7/E8/E12/E13/E17; `exploitability.sh:27` |
| `verify` | 134 | `--games=200` | audit-violation count, set-conservation failures, action-limit games, determinism PASS/FAIL, `VERIFY PASS/FAIL`; **exit code 0/1** | v0.5 E1, v0.4 E1/E1L |
| `matrix` | 163 | `--policies=<9> --games=300 --seed=5150 --rotations=2` | `{"policies":[…],"cells":[<printMatch json>…]}` | v0.4 E4 |
| `tune` | 189 | `--panel=v03,lockout,detective,diversifier --games=250 --pop=24 --elite=6 --gens=40 --beta=10 --sigma=0.6 --seed=424242 --base=v04 --init= --sigmaparams= --out= --full` | JSONL, one `{"gen",…,"bestScore","incumbentScore","winRates":[…],"mu":[…]}` per generation, then `{"final":"mu"}`, `{"final":"best"}`, `{"weights":"a\|b\|…"}`; also `weights=…` on stdout | v0.5 F1; v0.4 rounds 1-5 + `exploitability.sh:23` |
| `selftest` | 238 | `--games=60` | belief-engine cross-validation vs exact rejection sampling; `SELFTEST PASS/CHECK` | v0.4 E2 |
| `oracle` | 327 | `--games=200 --maxdeals=200000 --samples=3000 --seed=20260822 --a=v04 --b=v03` | brute-force posterior validation; **exit 0/1** on `ORACLE PASS/FAIL` | v0.4 E15 |
| `gateaudit` | 386 | `--a=v04:mgate=0.008,gateaudit=1 --games=300 --seed=90210 --rotations=6 --panel=<8>` | declaration pre-gate false-negative counts | v0.4 E16 |
| `calibrate` | 423 | `--a=v04 --b=v03 --games=300 --seed=8181` | Brier / logloss / ECE + 10-bin reliability for ask and declaration forecasts | v0.5 E6, v0.4 E6/E6b |
| `ablate` | 441 | `--ref=v04 --variants=<`;`-sep> --panel=v03,lockout,detective --games=500 --rotations=2 --seed=606060` | JSON: reference profile then per-variant `{"spec","winRate","deltaFromRef","ci","perOpponent"}` — **the only command that reports a paired interval** | v0.4 E5 |
| `fitvalue` | 488 | `--a=v04:value=0 --b=v03 --games=400 --lambda=1e-3 --seed=31415` | `vweights=…` on stdout, `rows= R2= rmse=` on stderr | v0.4 E14 |
| `bench` | 559 | `--a=v04 --b=v04 --games=200` | `<X> games/s over <N> games` | v0.5 E9, v0.4 E9 |
| `pathology` | 568 | `--a=v04 --b=v04 --games=200 --rotations=2 --seed=31` | `printPathology` (`diag.hpp:190-216`) — the KPI block | v0.5 E2 |
| `blockalias` | 583 | `--a=v04 --games=20 --seed=31` | BlockDP aliasing check | P-report only |
| `forcedprobe` | 594 | `--a --b --games=300 --rotations=2 --seed=31 --csv=` | forced-endgame decision dump (31-column CSV) | P2 |
| `coord` | 631 | `--a --b --games=300 --rotations=2 --seed=31 --pass=unilateral\|oracle\|ladder\|low\|cards --rungs= --forcedth= --dump= --both-teams --no-measure --leak` | turn-transfer / coordination stats | P8 (24 invocations across v0.5 reports) |
| `deadlock` | 668 | `--spec=v04 --games=60 --rotations=2 --seed=31 --minev=300 --dump=4 --stride=40 --states=3 --h2h=0` | deadlock forensics | P1 |
| `serve` | 684 | `--port=8173 --web=` | HTTP table server | — |
| `dumpvalue` | 689 | `--a --b --games=400 --rotations=2 --seed=31415 --out=rows.csv` | value-feature CSV | P7 |
| `deceit` | 707 | `--m=v04 --d=silent --ctrl=v04 --games=200 --seed=4242 --stride=1 --dseats=1` | deception panel; **`--dseats` is the only per-seat heterogeneity in the harness** | v0.5 E10, P3 |
| `humanchan` | 732 | `--a --b --games=200 --rotations=2 --seed=31` | human-strategy channel | P5 |
| `p4probe` / `p4match` / `p4horizon` / `p4blockcmp` / `declcard` | 749 / 828 / 856 / 919 / 889 | see table above | policy-correctness instrumentation on a private `p4` agent (`probe_policy_v04.hpp`), **not** `makeAgent` | P4 |
| `shadow` | 948 | `--base=v04 --variants=<`;`> --games=60 --seed=99001` | decision-level divergence counts, no games scored | P-reports |
| `litpath` / `lith2h` / `litdecl` | 968 / 988 / 1003 | see table | Literature-dialect probes | P-lit |
| `passverify` / `vforced` / `vdeadlock` / `vturnxfer` / `polreview` / `vhorizon` | 1018 / 1032 / 1050 / 1066 / 1093 / 1141 | see table | adversarial re-verification of P1/P2/P4/P8 | P*-verify reports |

Notes worth carrying into v0.6:

- **`./fish m7check` is cited three times in `docs/FISHBOT_V05.md` / `research/v05/DESIGN.md` and does not exist** in `main.cpp`. M7 is specified, not built; the command name is aspirational.
- Only `verify` and `oracle` return a non-zero exit code. **Nothing else is a gate.**
- `pathology`, the command that carries DESIGN.md §M10's three commit-gate KPIs, has **no `--json` and no exit code** (`main.cpp:568-582`).
- The `p4` spec (`--a=p4`) is not a `makeAgent` policy — `factory.hpp:232-245` would `exit(2)` on it; the p4 commands construct their own instrumented agent.

---

## 2. The fitting loop (`tune`)

### Optimiser: cross-entropy method, diagonal Gaussian

`engine/src/tuner.hpp:67-116`. Per generation `g`:

```cpp
uint64_t genSeed = mixSeed(sp.seed, uint64_t(g) * 104729 + 5);        // :76
for (int i = 0; i < sp.population; i++) {
  for (size_t d = 0; d < D; d++) {
    double u1 = std::max(1e-12, rng.uni()), u2 = rng.uni();
    double z = std::sqrt(-2 * std::log(u1)) * std::cos(2 * M_PI * u2);
    double v = mu[d] + sigma[d] * z;
    if (!sp.lo.empty()) v = std::min(sp.hi[d], std::max(sp.lo[d], v));   // :84  CLAMP
    cand[i][d] = v;
  }
  if (i == 0) cand[i] = mu;                   // always evaluate the incumbent   :87
  evals[i] = evaluateCandidate(sp, cand[i], genSeed);
}
```
Elite update (`tuner.hpp:94-102`), elite mean and elite *variance about that mean*,
then exponential smoothing with `smoothing = 0.6` and a hard `sigmaFloor = 0.03`:

```cpp
mu[d]    = sp.smoothing * newMu[d] + (1 - sp.smoothing) * mu[d];
sigma[d] = std::max(sp.sigmaFloor, sp.smoothing*std::sqrt(newSigma[d]) + (1-sp.smoothing)*sigma[d]);
```

### Objective

`tuner.hpp:47-65`:

```cpp
for (size_t i = 0; i < sp.panel.size(); i++) {
  MatchConfig mc;
  mc.specA = spec; mc.specB = sp.panel[i];
  mc.games = sp.gamesPerOpponent;
  mc.seed  = mixSeed(genSeed, i * 7919 + 13);   // common random numbers   :55
  ...
  double wr = double(st.winsA) / double(std::max(1, st.games * 2));       // :59
  acc += std::exp(-sp.beta * wr);                                          // :61
}
e.score = -std::log(acc) / sp.beta;                                        // :63
```

`mc.rotations` is **never set**, so it stays at `MatchConfig`'s default `2`
(`arena.hpp:50`) — the tuner uses a 2-rotation team-swap block while the evaluation
battery uses 6. `wr`'s denominator hard-codes that 2 at `tuner.hpp:59`.

### Panel, seeds, sizes — the v0.5 run of record

Command (`docs/FISHBOT_V05.md` §9; artifact `research/v05/runs/fit-round1.jsonl`):

```
./fish tune --base=v05 --panel=v04,v03,lockout,detective,diversifier,hunter \
  --full --games=200 --pop=24 --elite=6 --gens=40 --beta=25 --seed=505101
```

- **Dimension** D = 34 = `NFEAT` (20, `v04.hpp:42`) ask weights + 14 decision knobs
  appended by `--full` (`main.cpp:207-216`).
- **Bounds** `main.cpp:217-223`: all coordinates `[-12, 20]`, then `w[0] ∈ [0,30]`,
  then the 14 knobs get their own `plo[]/phi[]`.
- **Seed discipline.** Root `505101`. Per generation `mixSeed(root, 104729g+5)`; per
  panel member `mixSeed(genSeed, 7919i+13)`; per deal `mixSeed(mc.seed, 2654435761·i+1)`
  (`arena.hpp:73`). **Common random numbers within a generation** (all 24 candidates
  share `genSeed`), fresh bank between generations. Final guard bank
  `mixSeed(root, 999983)` (`tuner.hpp:112-113`) — disjoint from every generation bank
  but derived from the same root, and **not an independent validation bank**.
- **Games per candidate** = 6 panel × 200 deals × 2 rotations = **2,400 games**.
- **Games per generation** = 24 × 2,400 = 57,600. **Total** = 40 × 57,600 + 2 × 2,400 =
  **2,308,800 games** (`build_tables_v05.py:571-576` derives 2,304,000, excluding the
  two guard evaluations).

### Wall-clock per round — measured

```
$ cd engine && time ./fish tune --base=v05 \
    --panel=v04,v03,lockout,detective,diversifier,hunter --full \
    --games=200 --pop=6 --elite=3 --gens=1 --beta=25 --seed=505101 --out=<scratch>
… 612.85s user 2.29s system 1131% cpu  54.388 total
```
8 candidate evaluations (6 population + `mu` + `best`) × 2,400 games = 19,200 games in
54.39 s ⇒ **353 games/s aggregate, 6.80 s per candidate evaluation**.

| | per generation | 40 generations |
|---|---|---|
| candidate evaluations | 24 | 962 |
| games | 57,600 | 2,308,800 |
| **wall clock** | **163 s (2.7 min)** | **6,542 s = 1.82 h** |

### Winner's-curse guard

`tuner.hpp:112-115` re-scores `mu` and `best` on the single common bank
`mixSeed(seed,999983)` and returns the higher. In `fit-round1.jsonl` it returned
`{"final":"mu",0.5272}` vs `{"final":"best",0.4798}` ⇒ **the shipped vector is the
distribution mean**. The comparison is paired (same bank) but sits on 2,400 games per
arm and **no interval is reported**.

The `best` it guards against is itself broken: `tuner.hpp:103` tracks the arg-max of
`evals[order[0]].score` **across generations**, i.e. across *different* seed banks.
It is the maximum of 960 unpaired noisy draws.

### The fit made no measurable progress — measured from the trace

`research/v05/runs/fit-round1.jsonl`, 40 generations. `incumbentScore` is the objective
of `mu` itself (`cand[0] = mu`, `tuner.hpp:87`), re-evaluated on a fresh bank each generation.

| quantity | value |
|---|---|
| `incumbentScore` gen 0 → gen 39 | 0.4998 → 0.5096 |
| `incumbentScore` mean / sd over 40 gens | 0.5000 / **0.0227** |
| OLS slope of `incumbentScore` on generation | +0.00049/gen, se 0.00031, **t = 1.60** (total +1.93 pts over 40 gens) |
| `bestScore` mean − `incumbentScore` mean | **0.0404** |
| E[max of 24 iid] × sd(incumbent) = 1.948 × 0.0227 | **0.0442** |
| v0.4 cell of best-of-gen: first-10 mean vs last-10 mean | 0.5265 vs 0.5420 |

The best-minus-incumbent gap (4.04 pts) is within 4 % of the pure order-statistic
prediction for 24 draws of the observed noise (4.42 pts). The incumbent trend is not
significant at 40 observations. This is the quantitative form of `docs/FISHBOT_V05.md`
§9's "What the refit was worth: nothing measurable."

### Why: the step size is uncalibrated across coordinates

`sigma0 = 0.6` (`tuner.hpp:24`) is applied to **all 34 coordinates**, whose ranges span
0.1 (`declareMargin ∈ [-0.05, 0.05]`) to 40 (`valueWeight ∈ [0, 40]`).
Gen-0 clipping probability at `mu` from the trace (`tuner.hpp:84`):

| knob | range | range/σ | P(candidate lands on a clamp bound) |
|---|---|---|---|
| `declareMargin` | 0.10 | 0.17 | **93.4 %** |
| `declThreshold` | 0.449 | 0.75 | **70.9 %** |
| `lockedAllocThresh` | 0.450 | 0.75 | **70.9 %** |
| `priorPhi` | 0.60 | 1.00 | 63.9 % |
| `askFloor` | 0.60 | 1.00 | 62.2 % |
| `minTeamProb` | 0.90 | 1.50 | 52.6 % |
| `priorTheta` | 1.50 | 2.50 | 34.0 % |
| `linearWeight` | 3.00 | 5.00 | 12.1 % |
| `valueWeight` | 40.0 | 66.7 | 0 % (step = 1.5 % of range) |
| `searchTopK` | 14 | 23.3 | 0 % (step = 4.3 %) |
| `chainWeight`, `threatWeight`, `oppCardFloor` | 12 | 20 | 0 % (step = 5 %) |
| `patiencePool` | 20 | 33.3 | 0 % (step = 3 %) |

The optimiser is simultaneously **bang-bang on the seven bounded probability knobs**
and **frozen on the seven unbounded weight knobs**. `sigmaFloor = 0.03`
(`tuner.hpp:25`) is 30 % of `declareMargin`'s range and 0.075 % of `valueWeight`'s, so
neither ever converges usefully.

**`--sigmaparams` is parsed and thrown away.** `main.cpp:224`:
```cpp
std::string sigPer = argVal(argc, argv, "sigmaparams", "");
```
`sigPer` is never referenced again anywhere in the file (`grep -n sigPer src/main.cpp`
returns exactly one line). The per-coordinate step-size knob the flag advertises does
not exist.

### Latent parser hazard

`tuner.hpp:35`: `base + (w.size() > 18 ? ":allparams=" : ":weights=")`. A hard-coded
`18` in the same place the v0.4 aliasing defect lived. Benign today (`NFEAT = 20`, a
20-vector routes to `allparams=` and `get(K+j, default)` falls back to defaults), but it
is `NFEAT`-dependent logic that does not read `NFEAT`.

---

## 3. Defect J: the β = 10 "soft minimum" is a weighted mean, ratio 1.9

### Algebra

The objective is `score(w) = -(1/β) log Σ_o exp(-β·r_o)`, `r_o = winRate_o(w)`
(`tuner.hpp:61-63`). Differentiate:

```
∂score/∂r_o = -(1/β) · (1/Σ_k e^{-β r_k}) · (-β) e^{-β r_o}
            =  e^{-β r_o} / Σ_k e^{-β r_k}   =  π_o = softmax(-β r)_o
```

So the gradient is a **convex combination** of the panel's win rates — the objective is
exactly a weighted mean in its first order, with weights `π`. The weight ratio is

```
π_max / π_min = e^{-β r_min} / e^{-β r_max} = exp(β · Δ),   Δ = max r − min r
```

which depends on β **only through the product β·Δ**. A second-order expansion makes the
"mean" reading literal:

```
score = r̄ − log(n)/β − (β/2)·Var(r) + O(β³·μ₃)
```
`log(n)/β` is constant in `w`, so **ranking by `score` = ranking by
`r̄ − (β/2)Var(r)`** to that order.

### The numbers (`research/v04/runs/selected.json`)

v0.4's shipped generation-8 profile against `v03, lockout, detective, v02`:
`r = [0.7560, 0.8020, 0.7650, 0.8190]`, `r̄ = 0.78550`, `Δ = 0.06300`, `Var = 6.715e-4`.

| | β = 10 (tuner default, `tuner.hpp:23`) |
|---|---|
| gradient weights π | `[0.3249, 0.2051, 0.2969, 0.1730]` (uniform would be 0.2500) |
| **π_max/π_min** | **1.8776** = `exp(10 × 0.063)` — **the 1.9 of `docs/FISHBOT_V05.md` §9** |
| soft-min exact | 0.643578 |
| `r̄ − log 4/10 − 5·Var` | 0.643514 — **agreement to 6.4 × 10⁻⁵** |
| variance penalty `(β/2)Var` | 0.00336 = **0.336 win-rate points** |
| `min(r)` | 0.75600 — the "soft minimum" sits **11.2 points below** it |

So over v0.4's panel, β = 10 makes the objective arithmetically indistinguishable from
*"mean win rate, minus one third of a point of spread penalty"*. The claim is verified.
The two counterfactuals `docs/FISHBOT_V05.md` §9 quotes also reproduce exactly:

| panel | β | Δ | ratio `exp(βΔ)` | π on the hardest cell |
|---|---|---|---|---|
| v0.4's (4 styles) | 10 | 0.063 | **1.88** | 0.3249 |
| v0.4's | 25 | 0.063 | **4.83** | 0.4306 |
| v0.5's (6 styles, best gen 24) | 10 | 0.415 | **63.43** | 0.7112 |
| v0.5's | 25 | 0.415 | **32,048.3** | **0.9829** |

The last row is a v0.6 budgeting fact, not just a rhetorical one: **at β = 25 over
v0.5's panel, 5 of the 6 panel cells together carry 1.71 % of the objective's gradient
while consuming 74 % of the fitting compute.**

### Two different β's in the same pipeline

`select_final.py:30` defaults `--beta 8.0`; `tuner.hpp:23` compiles `beta = 10.0`;
v0.4's shipping command recorded no `--beta`. Recomputing the soft minimum of
`selected.json`'s profile at β = 8 reproduces its stored `"softmin":
0.6095652811544355` **to all 16 digits**; at β = 10 it is 0.643578. The fitting
objective and the selection objective are different functions, and neither β is
recorded in any trace file — `build_tables_v05.py:493-499` has to recover them by
regexing `tuner.hpp` and `docs/FISHBOT_V05.md`.

---

## 4. Parameters → shipped config, and what is not covered

### The path

```
fish tune  ──► 34-float "a|b|…" (tuner.hpp:34-43 weightSpec, main.cpp:228-234)
           ──► research/v05/runs/v05-fitted.txt
           ──► freeze_config_v05.py  ──► rewrites engine/src/v05.hpp in place
           ──► make                  ──► bare spec `v05` now IS the fitted policy
           ──► roundtrip assertion: `v05` vs `v05:allparams=<same 34>` must return 50.0000 %
```

`engine/freeze_config_v05.py`:
- `NFEAT = 20` (line 18) — **derived, not hard-coded at 18**, and the same derivation
  is used by the parser at `factory.hpp:86` (`const size_t K = size_t(NFEAT);`). This
  is the explicit fix for the v0.4 aliasing defect (`factory.hpp:188-191`).
- Ask weights: regex-rewrites the numeric column of `double w[NFEAT] = {…}` in place at
  `%.5f` (lines 59-76), matching `weightSpec`'s `snprintf("%.5f")` (`tuner.hpp:38`) so
  the round-trip is exact; a `%.4f` here would silently ship a different policy
  (comment, lines 61-67).
- 14 decision knobs: `KNOBS` table lines 22-37, clamped with the **same** bounds as
  `factory.hpp:87-100`, then `re.sub(r'(\bfield\s*=\s*)(-?[\d.]+)')` — `sys.exit` if any
  field fails to match exactly once (lines 79-85).
- Prints the round-trip command (lines 91-94). The executed artifact is
  `research/v05/runs/roundtrip-assert.txt`: 50.0000 % over 6,000 games at two seeds,
  with the negative control (re-encoded at the v0.4 offset 18) at 45.87 % — a 4.13-point
  shortfall, which is the size of the bug v0.4's pipeline could not see.

### What is NOT covered

**1. The 16 value-function coefficients.** `V05Config::vw[NVFEAT]` (`src/v05.hpp:79-95`,
`NVFEAT = 16`, `v04.hpp:43`) is written by **no** freezer. `freeze_config_v05.py`'s
`KNOBS` has 14 entries and the ask-weight rewrite stops at `NFEAT`; the CEM vector is
34 long and never reaches them. `docs/FISHBOT_V04.md:150-152` states the v0.4 form of
this ("the 16 value-function coefficients compiled into `V04Config::vw` are **not**
those in `research/v04/results/E14-valuefit.txt`; `freeze_config.py` writes only the 34
policy parameters"). Verified for v0.5 and worse:

```
$ sed -n '/double vw\[NVFEAT\] = {/,/};/p' src/v04.hpp | md5   786720428bfe8ac3e08e46967eb40161
$ sed -n '/double vw\[NVFEAT\] = {/,/};/p' src/v05.hpp | md5   786720428bfe8ac3e08e46967eb40161
```

**v0.5's value weights are byte-identical to v0.4's** — an unfitted, un-refit, un-frozen
16-dimensional block sitting inside the shipped policy. `fish fitvalue` prints
`vweights=…` (`main.cpp:552-556`) and `factory.hpp:121-125` accepts `vweights=` on the
spec line, so the plumbing exists in both directions; nothing connects them.

**2. `Rules::forcedTh` (the willingness ladder).** 8 numbers at `fish.hpp:126-127`,
settable per-run only via `coord --forcedth=` (`main.cpp:653-656`) or
`vforced --forcedlow=` (`main.cpp:1040-1041`). Not in the CEM vector, not in any
freezer. `docs/FISHBOT_V05.md` §12 calls it "live but badly calibrated".

**3. Mechanism switches.** `m1, m1p, m2, stage2, norepeat` (`factory.hpp:103-107`) are
compile-time defaults in `V05Config`, chosen by hand from E5; not fitted, not frozen by
script.

**4. `select_final.py` is v0.4-only and was not used for v0.5.** It hard-codes
`'v04:allparams='` (line 46) and writes `research/v04/runs/selected.json` (line 58);
`BIN` defaults to `./fish2` (line 13). There is **no `selection.log` under
`research/v05/runs/`** — v0.5 replaced an out-of-band selection on an independent
validation bank (seed 1357911, 4 opponents, 400-500 deals, last 6 generation means)
with `tune()`'s two-candidate internal guard on a bank derived from the fitting root.
That is a **regression in selection discipline**, not an improvement.

---

## 5. Evaluation discipline

### Duplicate / rotation design (`arena.hpp:57-125`)

Deal seed: `mixSeed(mc.seed, 2654435761·i + 1)` (`arena.hpp:73`). Then

```cpp
int orient = (mc.rotations == 2) ? rot : (rot / 3);   // :86  which team is "A"
int shift  = (mc.rotations == 2) ? 0   : (rot % 3);   // :87  game.rotation
```
`game.rotation` means "seat *i* receives hand `(i+rotation) % 6`" (`game.hpp:95,101-103`),
and `teamOf(seat) = seat & 1` (`fish.hpp:41`), so teams are `{0,2,4}` and `{1,3,5}`.
Working the six out: A holds hand-triple `{0,2,4}` at rot ∈ {0,2,4} and `{1,3,5}` at
rot ∈ {1,3,5} — **exactly three each**, as the comment at `arena.hpp:81-86` claims. The
comment also correctly flags the naive coupling (`orient = rot&1`, `shift = rot`) as the
*opposite* of a duplicate block.

**`collectCalibration` still has the wrong coupling** (`arena.hpp:212,215`:
`orient = rot & 1`, `game.rotation = rot`). Dormant, because `calibrate`
(`main.cpp:423-440`) never passes `rotations`, so it runs at the default 2. It will bite
the first time anyone calibrates at 6.

### Mirror runs are exactly 50 % duplicated work — measured

At `--rotations=2` with identical policies both orientations replay the same game.
Measured on the actual instrument:

```
$ ./fish pathology --a=v05 --b=v05 --games=100 --rotations=1 --seed=31
games 100 … repeat (a,c,t) 248 (2.84078%) … declarations 900 wrong 26 (2.88889%)
$ ./fish pathology --a=v05 --b=v05 --games=100 --rotations=2 --seed=31
games 200 … repeat (a,c,t) 496 (2.84078%) … declarations 1800 wrong 52 (2.88889%)
```
**Every count doubles; every rate is identical to the last printed digit.** E2's mirror
columns (`experiments_v05.sh:13-14`, `--games=300 --rotations=2`) therefore report 600
"games" over 300 distinct games. `build_tables_v05.py:130-152` knows this and halves
the counts in post-processing with an `assert raw % 2 == 0`. The engine still burns the
compute. The same identity makes a mirror `match --rotations=2` return **exactly 50 %
with a degenerate cluster interval** — which is what makes the round-trip assertion work,
and also what makes any mirror cell in a tuner panel carry zero gradient.

### Seed banks

| study | fitting roots | selection root | evaluation roots |
|---|---|---|---|
| v0.4 | 20260821, 770077, 313131, 888111, **round 5 not captured** | 1357911 (`select_final.py`, β = 8) | 90210, 515151, 606060, 717171, 828282/838383/848484, 20260820, 959595, 464646, 31415, 20260822, 515253→6543210, 90210 |
| v0.5 | 505101 (β = 25) | none (internal guard on `mixSeed(505101, 999983)`) | 31, 90210, 31337, 515151, 777001, 424242, 515253, 606060, 717171, 828282, 909090, 31415926, 8675309 |

Recorded by hand in `build_manifest.py:135-164` (`V04_META`, `V05_META`) and in the
comment header of `experiments.sh:4-9`. Disjointness is **asserted in prose, checked
nowhere**. Three hazards:

- `match`'s default seed is **20260821** (`main.cpp:119`) — a v0.4 *fitting* root. Any
  ad-hoc `./fish match` with no `--seed` replays a fitting bank.
- `tune`'s default seed is **424242** (`tuner.hpp:27`, `main.cpp:197`), which is also a
  v0.5 E3 *held-out* head-to-head bank (`experiments_v05.sh:20`).
- v0.5 reuses 515151 and 606060, which are v0.4 evaluation roots. Harmless across
  studies, but there is no registry that would catch it if it were not.

Because everything passes through `mixSeed` (splitmix64 finalizer, `fish.hpp:79-84`),
distinct roots give unrelated deals, so these are hygiene problems rather than live
contamination — with the exception that **v0.5's final selection bank is derived from
the fitting root**, so "held out" is a statement about the mixer, not about the design.

### Confidence intervals — exactly how

Three estimators, all with **fixed** bootstrap seeds so runs are reproducible:

1. **Wilson** (`arena.hpp:38-45`), z = 1.959964, on `winsA / (deals × rotations)`.
   Treats every game as independent. Emitted as `wilsonCI` (`main.cpp:73`) and as the
   bracket in the human block (`main.cpp:96`).
2. **Cluster bootstrap** (`arena.hpp:151-168`), B = 20,000, seed 999, **percentile**
   (2.5 % / 97.5 % order statistics, lines 166-167). Resamples **deals**, each deal
   contributing its 0…`rotations` A-wins. This is the `ci` field of every `match`/`matrix`
   row (`main.cpp:66-67, 72`). **It is a one-arm interval — not paired against the
   opponent, not paired against another candidate.**
3. **Paired bootstrap** (`arena.hpp:130-148`), B = 20,000, seed 31337, percentile,
   `(x[j] − y[j]) / rotations` over a *common* resampled deal index `j`. Used by
   **`ablate` only** (`main.cpp:479-481`). Alignment of `x` and `y` relies on
   `runMatch` concatenating per-thread `paired` vectors in thread order
   (`arena.hpp:121`), which is deterministic given identical `games` and `nThreads` —
   true here, but silently fragile.

**So: E3, E4, E5 (v0.5), E7, E8 and the exploitability probe are all reported with
unpaired intervals. Only v0.4's E5 uses the paired one.**

#### Measured: the duplicate design already cancels deal luck, and pairing is worth 2-5×

From `research/v05/results/E3-headtohead.jsonl` (300 deals × 6 rotations = 1,800 games
per row): cluster-bootstrap widths 4.33 / 4.83 / 4.94 / 4.44 / 4.78 points against a
Wilson width of 4.61. Independent-game theory gives
`2 × 1.96 × 0.5/√1800 = 4.62` points. **The intra-deal correlation of the A-win count is
≈ 0** — the 6-rotation block is doing its job, and effective n = games, not deals. So
for a *marginal* win rate the planning rule is simply

> half-width (points) ≈ 98 / √N_games  ⇒ ±1.0 pt needs 9,600 games; ±0.5 pt needs 38,400 games.

For a *difference*, pairing is far cheaper. Measured directly:

```
$ ./fish ablate --ref=v05 --variants="v05;v05:decl=0.87;v05:w0=11.9" \
    --panel=v04 --games=400 --rotations=6 --seed=606061     # 83.7 s wall
ref v05 wr 50.7920
  v05             wr 50.7920  pairedDelta +0.0000  ci [+0.0000,+0.0000]  width 0.0000
  v05:decl=0.87   wr 51.1670  pairedDelta -0.3750  ci [-0.8750,+0.1670]  width 1.0420
  v05:w0=11.9     wr 51.5000  pairedDelta -0.7080  ci [-2.2500,+0.7920]  width 3.0420
```

- The identical variant returns **exactly zero with a zero-width interval** — proof that
  CRN pairing in this harness is exact, seed for seed. This is a free regression test.
- 2,400 games. Unpaired, the difference of two arms would have width
  `2 × 1.96 × √2 × 0.5/√2400 = 5.66` points. Paired: **1.04 points for a small knob
  change (5.4× tighter, 29× the variance reduction) and 3.04 points for an ask-weight
  change (1.9× tighter).**

---

## 6. Throughput on this machine

Command of record and steady-state numbers (first run after idle is cold — 157 games/s —
so all figures below are the 2nd+ run):

```
$ cd engine && ./fish bench --a=v05 --b=v05 --games=600
251.089 games/s over 1200 games          # also observed 288.9, 288.6, 275.9
```

**Planning figure: v0.5 mirror ≈ 275 games/s (range 251-289 over 4 runs).** The
committed artifact `research/v05/results/E9-throughput.txt` says 282.142 — reproduced.

| pairing | games/s | note |
|---|---|---|
| `v05` vs `v05` | **251-289** | E9 of record |
| `v05` vs `v04` | 235 | the fitting objective's dominant cell |
| **`v04` vs `v04`** | **83.4** | 3.1× slower — the v0.4 mirror deadlock is also a compute tax |
| `v05` vs `v03` | 293 | |
| `v05` vs `lockout` | 468 | |
| `v05` vs `detective` | 491 | |
| `v05` vs `diversifier` | 389 | |
| `v05` vs `hunter` | 437 | |
| `v05` vs `v05`, `--threads=1` | 21.7 | ⇒ 13.3× scaling on 15 cores |

Reference: `v04` block posterior is 11.9 games/s and `v03` is 6,954 games/s
(`research/v04/results/E9-throughput.txt`) — the exact-belief mode is ~23× slower than
Fast and is not a fitting option.

### v0.6 training budget

| job | games | wall clock at measured rates |
|---|---|---|
| one tuner generation (v0.5 schedule: pop 24, 6 opponents, 200 deals × 2 rot) | 57,600 | **163 s** |
| full 40-generation fit | 2,308,800 | **1.82 h** |
| one E3-class head-to-head cell (300 deals × 6 rot) | 1,800 | 6 s (from the artifact's own `seconds` field) |
| a ±0.5-point head-to-head cell (38,400 games) | 38,400 | ~140 s |
| full 9-style × 2-arm E4 at ±0.5 pt | 691,200 | ~42 min |
| E3+E4+E5+E7 of `experiments_v05.sh` (the `match`-based steps) | 62,400 | **183.1 s**, summed from the `seconds` field of every row in the four committed JSONL artifacts (303 / 461 / 214 / 456 games/s respectively) |
| E8 alone (`--games=4000 --rotations=6`, two arms) | 48,000 | ≈ 3 min (greps out `seconds`, so extrapolated at ~250 g/s) |

**The binding constraint is the tuner's per-cell noise, not the wall clock.** At 400
games/cell the marginal sd is 2.5 points; the CRN-paired sd for CEM-scale perturbations
is ≈ 1.9 points (extrapolated from the measured 0.78 pt sd at 2,400 games). To resolve a
1-point improvement the CEM would need ~5,800 games/cell (14.5×) ⇒ **26 h for a
40-generation fit**. Two cheap ways out, both measurable today:

- **Spend the panel budget where the gradient is.** At β = 25 over v0.5's profile the
  gradient weights are `[0.9829, 0.0103, 0.0028, 0.0040, 0.0001, 0.0000]` for
  `v04 / v03 / lockout / detective / diversifier / hunter`, while the measured cell
  *times* (400 games ÷ the rates in the table above) split
  `25.5 / 20.5 / 12.8 / 12.2 / 15.4 / 13.7 %`. Dropping `diversifier` and `hunter`
  (combined gradient weight 1.1e-4) frees **29 % of the cell time**; keeping only `v04`
  and `v03` (99.3 % of the gradient) frees **54 %** and buys **2.2× the games per
  informative cell** at constant wall clock; keeping only `v04` (98.3 %) frees **74 %**
  and buys **3.9×**.
- **Score the paired difference, not the raw rate** — the measured 5.4× / 1.9× width
  reductions above are variance reductions of 29× / 3.6× at zero extra compute.

---

## 7. CONCRETE LIST — what v0.6 must build into the harness

Ordered by (payoff / difficulty). Every item names the file and line where it goes.

### H1. Minimax-regret objective in the optimiser (not in post-processing)
Today regret is computed **after the fact, in Python, over exactly two arms**:
`build_tables_v05.py:304-314` (`reg = {s: max(prof['v05'][s], prof['v04'][s]) - prof[arm][s]}`).
The optimiser never sees it. `grep -rn regret engine/src/` returns nothing.
- **Where:** replace the fixed softmin at `tuner.hpp:61-63` with a dispatch over
  `TuneSpec::objective ∈ {SoftMin, Min, Regret, MinimaxRegret}`; add
  `std::vector<double> refProfile` to `TuneSpec` (`tuner.hpp:17-32`) holding the
  incumbent's per-opponent win rate, measured once before generation 0 on the same
  banks. Regret on opponent *o* is `max(refProfile[o], wr_o) − wr_o`; the objective is
  `−max_o regret_o` (optionally soft-maxed with the same β machinery for smoothness).
- **CLI:** `--objective=`, `--refprofile=a|b|…` in `main.cpp:189-236`.
- **Report both:** extend the per-generation JSONL at `tuner.hpp:104-109` with
  `"min"`, `"regret"`, `"argmaxRegret"` so `build_tables_*` reads them instead of
  re-deriving them. DESIGN.md §M10 asks for `min` *and* regret; today the trace has neither.

### H2. Per-coordinate step sizes, and the dead `--sigmaparams`
Measured above: 93 % of gen-0 `declareMargin` candidates land on a clamp bound while
`valueWeight` gets a 1.5 %-of-range step.
- **Where:** `main.cpp:224` currently parses `--sigmaparams` into an unused string.
  Parse it into `TuneSpec::sigma0Vec` (new field, `tuner.hpp:17-32`) and use it at
  `tuner.hpp:71` (`std::vector<double> sigma(D, sp.sigma0)`).
- **Better default:** derive σ from the bounds — `sigma[d] = frac * (hi[d]-lo[d])` with
  `frac ≈ 0.15`, and make `sigmaFloor` relative too (`tuner.hpp:25,101`). That makes the
  search scale-free and removes the need for the flag in the common case.
- **Diagnostic to add:** log the per-generation clip fraction per coordinate into the
  JSONL (`tuner.hpp:104-109`) so this failure mode is visible in the artifact.

### H3. Paired estimator for the headline, and a paired tuner objective
`ablate`/`pairedBootstrap` (`arena.hpp:130-148`) is already 2-5× tighter than the
`clusterBootstrap` (`arena.hpp:151-168`) that E3/E4 report, and returns exactly 0 with a
zero-width CI for identical policies.
- **Where:** add `--pair=SPEC` to the `match` block (`main.cpp:114-133`): run
  `runMatch` twice on identical `mc.seed`/`games`/`threads`, then emit
  `"pairedDelta"` and `"pairedCI"` alongside the existing fields in `printMatch`
  (`main.cpp:69-92`). Use it for E3 (`experiments_v06.sh`) so the head-to-head claim is a
  paired one.
- **Tuner:** in `evaluateCandidate` (`tuner.hpp:47-65`), keep `MatchStats::paired` and
  score `wr_candidate − wr_incumbent` on the same deals rather than `wr_candidate`.
  Zero extra games; the incumbent arm is already evaluated as `cand[0]` (`tuner.hpp:87`).
- **Guard both directions:** assert `pairedDelta == 0` for a self-variant as a startup
  self-test (the measured invariant), so a future thread-ordering change that breaks
  alignment fails loudly rather than silently widening the interval.

### H4. Pathology KPIs as commit gates
`diag.hpp` already computes all three DESIGN.md §M10 KPIs — `longestDeadRun`
(`diag.hpp:24,141`), `gamesWithDeadRun` (`diag.hpp:30,142`), `declWrongLate`
(`diag.hpp:26,124`) — and prints them (`diag.hpp:190-216`). Nothing gates on them; the
`pathology` block (`main.cpp:568-582`) always `return 0`.
- **Where:**
  1. `--json` in `main.cpp:568-582`, mirroring `printMatch`'s JSON branch, so the KPIs
     stop being regex-scraped out of prose (`build_tables_v05.py:55-66` currently parses
     eleven regexes out of the human block).
  2. `--gate` returning non-zero when any of `longestDeadRun >= K`, `gamesWithDeadRun > 0`,
     `declsLate > 0`, `limitHits > 0` — and thresholds moved from the two hard-coded
     literals (`diag.hpp:124` `nEvents >= 220`, `diag.hpp:142` `longest >= 6`) into
     `PathologyConfig` (`diag.hpp:147-154`).
  3. `set -e` plus `./fish pathology --gate` as **step 1** of `engine/experiments_v06.sh`,
     before any head-to-head runs.
  4. Fix the mirror double-count: in `runPathology` (`diag.hpp:156-188`) force
     `rotations = 1` when `pc.specA == pc.specB`. Measured above: it is exactly 50 %
     wasted compute, and `build_tables_v05.py:130-152` currently repairs it downstream
     with an `assert raw % 2 == 0`.

### H5. Exploitability / best-response probe for v0.6
`docs/FISHBOT_V05.md` §12: *"No exploitability probe has been run against v0.5 … the
single largest hole in the evaluation."* The machinery exists —
`engine/exploitability.sh:19-31` fits an exploiter with `tune --panel=<target>
--beta=1 --sigma=0.7 --seed=515253` and re-measures it on a fresh bank (6543210).
- **Where:** the script is hard-wired to v0.4 — `--base=v04` (line 23),
  `v04:allparams=` (line 27), `OUT=../research/v04/results` (line 9). Parameterise
  `BASE`/`OUT`/`SPECPREFIX`, add `probe v05 v05` and `probe v06 v06`, and register the
  artifact in `build_manifest.py` (a `RUNS_V06` list beside `RUNS_V05`, lines 67-90) so
  it is checksummed like everything else.
- **Objective note:** `--beta=1` there is deliberate (single-member panel ⇒ β is
  irrelevant to the ranking, `exp(βΔ)` with n = 1), so H1's rework must keep a
  single-opponent path that reduces to plain win-rate maximisation.
- **Budget:** at `GENS=12 POP=18 GAMES=180`, one probe = 12 × 18 × 360 = 77,760 games
  against one opponent; at 235-490 games/s that is **3-6 min per target**. Cheap.

### H6. Per-seat specs → partner-regime split
`runMatch` builds **one** policy for all three A seats: `A[i] = makeAgent(mc.specA)`
(`arena.hpp:68`), `ag[p] = aSeat ? A[p/2].get() : B[p/2].get()` (`arena.hpp:89-92`).
There is no way to express "FishBot with two bot partners" vs "FishBot with two
human-model partners", which decision D2 (`docs/FISHBOT_V05.md` §12: *"the partner-aware
regimes (decision D2) do not exist, so this document reports one configuration where the
brief asks for two reported separately"*) requires.
- **Where:** widen `MatchConfig` (`arena.hpp:47-55`) from `std::string specA` to
  `std::array<std::string,3> specA` (with a single-string constructor for back-compat),
  and construct per index at `arena.hpp:68`. CLI: `--a=`, `--a1=`, `--a2=` in
  `main.cpp:114-133`; same for B.
- **Reuse what exists:** `deceit`'s `--dseats` bitmask (`main.cpp:717-721`,
  `probe_deception_run.hpp`) is exactly this mechanism, implemented once, privately, for
  one probe. Lift it into `arena.hpp` and delete the duplicate.
- **Then:** `experiments_v06.sh` reports every headline cell twice — bot-partner regime
  and human-model-partner regime — and `build_tables_v06.py` emits two macro families.
  The tuner needs the same: `TuneSpec::panel` entries become `(opponentSpec,
  partnerSpec)` pairs so a candidate can be fitted under both regimes and the
  minimax-regret objective (H1) can range over regimes as well as styles.

### H7. Freeze the value weights (or take them out of the policy)
`V05Config::vw[16]` (`v05.hpp:79-95`) is byte-identical to `V04Config::vw`
(`v04.hpp:111-127`, md5 verified) and is written by no freezer.
- **Option A (cheap):** `engine/freeze_value_v06.py`, consuming `fish fitvalue`'s
  `vweights=` line (`main.cpp:552-556`) and rewriting the `vw[NVFEAT]` block the same way
  `freeze_config_v05.py:59-76` rewrites `w[NFEAT]`; add a round-trip assertion against
  `v06:vweights=…` (`factory.hpp:121-125`).
- **Option B (principled):** extend the CEM vector from 34 to 50 (`main.cpp:207-216`,
  `factory.hpp:79-101`, `freeze_config_v05.py:22-37`). Costs D = 50 in a search that
  already cannot resolve D = 34 (§2), so **A first, B only after H2/H3 make the search
  work.**

### H8. Kill the cross-bank `best`, and restore an independent selection bank
- `tuner.hpp:103` tracks the arg-max of per-generation scores **across different seed
  banks**. Either delete it (return `mu`) or re-score the top-k generation means on one
  common bank, as `select_final.py:36-52` already does.
- `select_final.py` is v0.4-only (line 46 `'v04:allparams='`, line 58 the hard-coded
  output path, line 13 `FISHBIN=fish2`, line 30 β = 8 ≠ the tuner's 10). Parameterise
  `--base` and `--out`, make `--beta` default to *the fitting β* rather than 8, and put
  it back in the v0.6 pipeline on a root that is not the fitting root.

### H9. Machine-checked seed registry
Seeds live in prose (`experiments.sh:4-9`) and in hand-maintained dicts
(`build_manifest.py:135-164`). Disjointness is never asserted.
- **Where:** `research/v06/seeds.json` with `{fitting: [...], selection: [...],
  evaluation: {...}}`; `build_manifest.py:203-253` asserts pairwise disjointness and
  that every `--seed=` appearing in `experiments_v06.sh` is declared. Also change
  `match`'s default seed away from **20260821** (`main.cpp:119`, a v0.4 fitting root)
  and `tune`'s away from **424242** (`tuner.hpp:27`, a v0.5 held-out E3 bank) — or make
  both required arguments.

### H10. Small correctness items found while reading
- `tuner.hpp:35` — `w.size() > 18` hard-codes 18 where `NFEAT` (=20) belongs. Same class
  as the v0.4 aliasing defect; currently benign.
- `tuner.hpp:59` — `st.games * 2` hard-codes the tuner's 2 rotations. If v0.6 fits at
  `--rotations=6` (H3 makes that attractive) this silently reports a third of the true
  win rate. Use `st.games * mc.rotations`.
- `arena.hpp:212,215` — `collectCalibration` uses the *wrong* rotation coupling
  (`orient = rot & 1`, `shift = rot`) that `arena.hpp:81-86` explicitly documents as not
  a duplicate block. Dormant only because `calibrate` never passes `rotations`.
- `main.cpp:28-38` — no unknown-flag detection anywhere. A mistyped `--rotation=6`
  silently runs at 2. One `argSeen` set checked at the end of `main` would close it.
- `main.cpp:163-188` — `matrix` gives cell (i,j) and cell (j,i) *different* seeds
  (`seed + i*1000 + j`), so the matrix is not paired across its transpose and
  `w_ij + w_ji ≠ 1` by more than rounding.

---

## 8. Reproduction commands used for this report

```bash
cd "/Users/dylan/Documents/GitHub/fish optimization/engine"
make                                                             # nothing to be done
./fish bench --a=v05 --b=v05 --games=600                         # 251-289 games/s (2nd+ run)
./fish bench --a=v04 --b=v04 --games=600                         # 83.4 games/s
./fish bench --a=v05 --b=v05 --games=600 --threads=1             # 21.7 games/s
./fish pathology --a=v05 --b=v05 --games=100 --rotations=1 --seed=31   # vs --rotations=2
./fish ablate --ref=v05 --variants="v05;v05:decl=0.87;v05:w0=11.9" \
      --panel=v04 --games=400 --rotations=6 --seed=606061        # 83.7 s
time ./fish tune --base=v05 --panel=v04,v03,lockout,detective,diversifier,hunter \
      --full --games=200 --pop=6 --elite=3 --gens=1 --beta=25 --seed=505101 --out=<scratch>
                                                                 # 54.388 s, 19,200 games
```
Soft-min algebra and the fit-trace regression were computed in Python from
`research/v04/runs/selected.json` and `research/v05/runs/fit-round1.jsonl`; no engine
source was touched.

— Dylan Nguyen, FishLab Research Project. Repository `fish optimization`, commit `bd812fe`.
