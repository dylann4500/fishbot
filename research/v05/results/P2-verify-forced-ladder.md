# P2 verification — "every forced declaration comes from the bestGuess rung; the seven willingness rungs fire zero times"

Dylan Nguyen, FishLab Research Project
Repository: `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.
Adversarial re-check of the rung-histogram claim in `research/v05/results/P2-forced-endgame.md`.

**Verdict: HOLDS for the runs it was measured on (v0.4 mirror and v0.4-vs-v0.3), with one
correction to the cited sample size and one important scope limit: the rungs are not dead
code — against weaker opponents they carry the overwhelming majority of forced
declarations.**

---

## 1. Independent instrumentation

The original evidence comes from `probe_forcedendgame.hpp`, which **replays** the ladder
from an observer hook (`predictNext`, `engine/src/probe_forcedendgame.hpp:142-161`) rather
than watching the engine execute it. A replay can disagree with the real sweep.

New instrumentation, sharing no code with it: `engine/src/probe_verifyforced.hpp` wraps
every agent and counts the calls **the engine itself** makes inside `Game::forcedEndgame`
(`engine/src/game.hpp:238-252`). A rung "fires" exactly when `willingForced()` returns
`true` for that rung's threshold; the bestGuess rung is exactly a `bestGuess()` call.
New CLI command `fish vforced` (appended to `main.cpp`, no existing block touched).

**Positive control.** `--forcedlow=X` overrides `Rules::forcedTh[6]`. With
`--forcedlow=0.0`, seed 31 mirror: `rung 6 th=0 calls=28 returned TRUE=28`,
`bestGuess() calls 0`. The counter does detect a firing rung; the zeros below are real.

Citations in the original claim check out verbatim: ladder `engine/src/fish.hpp:126-127`,
swept at `engine/src/game.hpp:238`, `bestGuess` at `engine/src/game.hpp:245`.

## 2. Reproduction, including at seeds the original never used

`willingForced()` returned TRUE, by rung, over the whole run:

| configuration | fwd decls | wrong | bestGuess calls | rung 0..6 TRUE |
|---|---|---|---|---|
| v04 mirror, seed **31**, 300×2 | 28 | 28 | 28 | 0,0,0,0,0,0,0 |
| v04 mirror, seed **777333**, 150×2 | 10 | 10 | 10 | 0,0,0,0,0,0,0 |
| v04 mirror, seed **1234567**, 400×6 | 120 | 120 | 120 | 0,0,0,0,0,0,0 |
| v04 mirror, seed **90210**, 400×6 | 100 | 100 | 100 | 0,0,0,0,0,0,0 |
| v04 mirror `belief=block`, seed 31, 300×2 | 8 | 8 | 8 | 0,0,0,0,0,0,0 |
| v04 vs **v03**, seed 424242, 400×6 | 85 | 85 | 85 | 0,0,0,0,0,0,0 |

The rung histogram reproduces at four seeds the original did not use, under the exact
belief (`belief=block`), and against v0.3. 351 forced declarations in total (mirror
rotations replay each deal — see §4), 100% from the bestGuess rung, 100% wrong. The single-threaded and multi-threaded runners agree
byte-for-byte at seed 31.

**Accounting closes exactly.** At seed 31: 84 `willingForced` calls per rung = 28 sweeps ×
1 live half-suit × 3 teammates; 28 `bestGuess` calls = 28 `ForcedDeclare` events. At seed
777333: 36 = 3·(6 games with 1 live set) + 9·(2 games with 2 live sets), 10 declarations.
So no `ForcedDeclare` came from the unrelated "holds only complete sets" path at
`engine/src/game.hpp:333`; all of them came from the ladder.

## 3. Why the rungs never fire in mirror play

`willingForced` is `evaluateSet(pub, set, 2)` plus `if (!v.ok || v.pAlloc < threshold)
return false` (`engine/src/v04.hpp:751-756`). At `press = 2` the team floor and the
marginal gate are both disabled (`engine/src/v04.hpp:607-608`), so `v.ok` is **not** the
binding constraint — measured directly, `evaluateSet !ok` was 0 in every configuration
tested. The binding constraint is `pAlloc`:

| configuration | pAlloc queries | `pAlloc == 0` | highest rung any query cleared |
|---|---|---|---|
| mirror seed 31 (Fast belief) | 84 | 84 | none (< 0.50) |
| mirror seed 1234567 (Fast) | 366 | 356 | none (< 0.50) |
| mirror seed 31 (`belief=block`) | 24 | 0 | none (< 0.50) |
| v04 vs v03 (Fast) | 255 | 255 | none (< 0.50) |

Two distinct mechanisms, and only the first is the known `bestGuess` capacity bug:

1. **Fast belief (the shipped default, `engine/src/v04.hpp:59`).** `pAlloc` comes from
   `jointSequential` fed the per-card *marginal argmax* allocation
   (`engine/src/v04.hpp:600-606`). That allocation piles the unresolved cards onto one
   teammate, `jointSequential` hits the capacity propagation and returns exactly 0
   (`engine/src/belief.hpp:jointSequential`, `if (!(tmp.mask[c] & (1u << p))) return 0.0`).
   0 < 0.50, so every rung refuses.
2. **Block belief.** `pAlloc` is `block.bestTeamAllocation`, which *is* capacity-feasible
   and strictly positive (0 of 24 queries were zero) — and still never reached 0.50.

So the ladder's failure in mirror play is **not** reducible to the `bestGuess` capacity
gap listed in `BRIEF.md`. Fixing capacity feasibility alone would still leave every rung
refusing, because the half-suits that survive to the forced endgame are exactly the ones
the team is genuinely unsure about: measured unresolved-card counts of the queried
half-suit were 2–5, never 0 or 1 (a half-suit whose allocation is known gets cashed
voluntarily first).

## 4. Correction to the cited evidence: `n=562` is 281 counted twice

The claim cites `rung 7 th=-1 n=562`. In a **mirror** match `rotations=6` sets
`orient = rot/3`, so the two orientations run identical policies and every deal is played
twice. My own runner reproduces the doubling exactly: seed 1234567 gives **120** forced
declarations at `--rotations=6` and **60** at `--rotations=3`. The 562 is **281 distinct**.
(Independently found in `research/v05/results/P2-verify-forced-endgame.md` §5; confirmed
here with different code.) **This changes the sample size, not the rate.**

## 5. Scope limit the claim omits: the rungs are not dead code

The claim's evidence is three mirror runs, and its wording ("the seven willingness rungs
fire zero times") reads as a property of v0.4. It is not. Against weaker opponents the
willingness rungs carry almost every forced declaration:

| opponent (400×6, seed 424242) | fwd decls | wrong | from bestGuess | rung 3 / 4 / 5 / 6 TRUE |
|---|---|---|---|---|
| detective | 2217 | 578 (26%) | **66 (3%)** | 293 / 1311 / 504 / 43 |
| lockout | 2905 | 601 (21%) | **34 (1%)** | 1042 / 1291 / 495 / 43 |
| hunter | 273 | 218 (80%) | **150 (55%)** | 0 / 1 / 121 / 1 |
| v03 | 85 | 85 (100%) | **85 (100%)** | 0 / 0 / 0 / 0 |
| v04 (mirror) | 100 | 100 (100%) | **100 (100%)** | 0 / 0 / 0 / 0 |

Weak opponents leak enough that the declaring team reaches the endgame holding
*resolvable* half-suits, and then rungs 3–6 do the work and the error rate drops from
100% to ~21–26%. The pathology is the same strong-opponent / mirror phenomenon the rest
of `BRIEF.md` describes, and it should be stated with that scope.

**A sharper statement that survives every configuration tested:** rungs 0, 1 and 2
(θ = 0.995, 0.98, 0.95) returned TRUE **zero times in 24,888 engine-side queries across
every configuration in the tables above**. v0.4's forced-endgame confidence never once reaches 0.95.
The three top rungs are unreachable by construction; the next four are reachable but only
against opponents that leak.

This also makes the paper's stated benefit of the ladder inoperative in mirror play:
`paper/fishbot_v04_standalone.tex:3054-3076` argues that "early, safer declarations resolve
cards and thereby sharpen the allocations available for the later ones". In mirror play
there is no early safe declaration to provide that sharpening, and the mirror endgame
almost always has exactly one live half-suit anyway (28 of 28 declarations at seed 31; 118
of 120 at seed 1234567), so the restart has nothing to sharpen.

## Reproduction

```
cd engine && make
./fish vforced --games=300 --rotations=2 --seed=31
./fish vforced --games=150 --rotations=2 --seed=777333
./fish vforced --games=400 --rotations=6 --seed=1234567
./fish vforced --games=400 --rotations=3 --seed=1234567    # halves: double-count check
./fish vforced --games=400 --rotations=6 --seed=90210
./fish vforced --games=300 --rotations=2 --seed=31 --a=v04:belief=block --b=v04:belief=block
./fish vforced --games=400 --rotations=6 --seed=424242 --a=v04 --b=v03
./fish vforced --games=400 --rotations=6 --seed=424242 --a=v04 --b=detective
./fish vforced --games=400 --rotations=6 --seed=424242 --a=v04 --b=lockout
./fish vforced --games=400 --rotations=6 --seed=424242 --a=v04 --b=hunter
./fish vforced --games=300 --rotations=2 --seed=31 --forcedlow=0.0     # positive control
```
