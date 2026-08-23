# R3 — Audit of the unshipped mechanisms M4, M5, M7

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, HEAD `bd812fe` ("v0.5"), working tree clean.
Recon only. Nothing under `engine/src/`, `paper/` or `docs/` was modified. All builds and runs were done
on a copy of the tree in the session scratchpad
(`…/scratchpad/work/{engine,m45sand,m7sand,bothsand}`).

---

## 0. Verdict in one table

| question | answer |
|---|---|
| Do the two headers compile? | Yes, both, `-O3 -Wall -Wextra`, zero warnings. |
| Do the patches apply to HEAD? | **No.** `git apply --check` fails on both, same hunk, `engine/src/v05.hpp:114`. `patch -p1 -F 3` recovers each one individually. |
| Do the two patches compose? | **No.** M7 applied after M4/M5 loses 4/14 hunks in `v05.hpp` and 1/1 in `factory.hpp`. |
| Off-switch equivalence to shipped v0.5? | **Yes, exact**, both patches, every printed digit, 600 games. |
| Is the M4 soundness claim (direction i) true? | **Yes**, 0 violations in 2.53 M certificate checks across two matchups. |
| Is model ignorance used as evidence anywhere? | **Yes, in two places** — `postMissLockout` (on by default) and `codebookValue` (off by default). Quantified in §4. |
| Does M4/M5 make v0.5 stronger at shipped weights? | **No.** 49.0 % [45.3, 52.8] over 600 paired games; 49.6 % [46.8, 52.4] over 1 200. |
| Does M7 make v0.5 stronger at shipped weights? | **Not measurably**, and the sign is *negative* against `withholder`, its design target. |
| Is M7's declaration firewall complete? | **No.** One residual call site: `declareByValue` reads `eH[]`, built from the *tilted* belief. |
| Can `fish tune` fit the new weights? | **No.** `main.cpp` still builds a 34-entry vector; M4/M5's reader at `K+14…K+18` is never reached, and M7 has no reader at all. |

---

## 1. `engine/src/v05_target.hpp` — M4 + M5

524 lines, header-only, `namespace fish::m45`. Depends only on `fish.hpp` and `belief.hpp`;
nothing in it names `V05Agent`.

### 1.1 What it implements

**M4 — one public deduction state, six lazy per-seat refinements.**
The design note's "6× the bookkeeping" is reduced to 1× by the observation that
`Knowledge::onEvent` is a pure function of the event except for two lines
(`belief.hpp:207-208`, the only writes conditioned on `me`; verified by reading
`belief.hpp:153-210`). So M4 maintains a single `Knowledge pub` seeded to
"nobody's hand is known" (`v05_target.hpp:122-140`) and derives

```
model(j) = pub, refined by every card OUR OWN k proves seat j holds     (v05_target.hpp:168-185)
```

**M5 — five features on the target index** (`v05_target.hpp:193-205`):
`lockout`, `void`, `emptyLastSafe`, `emptyLastRisk`, `codebook`.

### 1.2 Public API

| symbol | line | meaning |
|---|---|---|
| `initPublicKnowledge(Knowledge&, int deckSets)` | 122 | seeds the common-knowledge start state, `me = NPLAY` so the two `me`-lines can never fire |
| `struct SeatModels` | 143 | `pub`, `model[6]`, `built[6]`, counters `events/refits/contradictions` |
| `SeatModels::reset/observe/invalidate/build/of` | 155-189 | `of(j, mine)` is the lazy accessor |
| `enum {M45_LOCKOUT…M45_CODEBOOK}`, `NM45 = 5` | 193-200 | feature indices |
| `featureName(int)` | 202 | |
| `struct M45Config` | 207-244 | `m4, m5a, m5b, m5c, m5d, conventions, m5aHardOnly, m5aPostMiss, m5aReplaceF8, mw[5]` |
| `struct M45Ctx` | 248-253 | `{const Knowledge* k; const Belief* bel; const PublicState* pub; int seat, teamMask, oppMask;}` |
| `pCanAsk(ctx, t, S, skip)` | 259 | P(t holds ≥1 card of S other than `skip`), independence across cards |
| `targetBeliefTeam(kt, q, c, teamMask)` | 273 | the target's own capacity-weighted P(c is on our team) |
| `struct M45Module` | 285-521 | `cfg`, `models`, `reset`, `observe`, `buildAddressing`, `computeLockout`, `postMissLockout`, `beginDecision`, `voidProgress`, `codebookValue`, `features`, `score`, `emptiesNonLast` |

`score()` (504) is `Σ cfg.mw[i] * g[i]` over the five features.

### 1.3 How `M4-M5.patch` wires it into v0.5

Nine hunks in `v05.hpp`, two in `factory.hpp`:

1. `#include "v05_target.hpp"` after `#include "v04.hpp"`.
2. `m45::M45Config m45;` appended to `V05Config`.
3. `m45::M45Module m45; m45::M45Ctx m45ctx;` appended to `V05Agent`; `reset()` does
   `m45.cfg = cfg.m45; m45.reset(r.deckSets); m45ctx = {&k,&bel,nullptr,s,teamMask,oppMask};`
   (correctly *after* `teamMask`/`oppMask` are computed); `observe()` adds `m45.observe(e)`.
4. `f[8]` (real tree `v05.hpp:320`) becomes conditional on `m5aReplaceF8`.
5. `f[11]` (`v05.hpp:323`) becomes `m45.emptiesNonLast(...)` when `m5c`.
6. `askExpectedValue`'s `(void)target;` (`v05.hpp:438`) is **kept**, with a comment giving the
   measured reason (the value part supplies 1.9 % of the selecting variation).
7. `chooseAsk` gains `m45ctx.pub = &pub; m45.beginDecision(m45ctx);` after `prepareRunway`, and
   `u += m45.score(...)` in both the main loop and the top-K rescoring loop.
8. `factory.hpp`: `allparams` reader for `mw[0..4]` at `K+14…K+18`; nine `optI/optD` switches
   (`m4, m5a, m5b, m5c, m5d, conventions, m5ahard, m5apost, m5af8`) and `mw0…mw4`.

Ordering is safe: `features()` is called only from `chooseAsk` (`v05.hpp:513, 527`), which calls
`refresh()` at 503 and `prepareRunway` at 509, so `m45ctx.pub` and `lastLive[]` are always current.

### 1.4 A scale defect in the wiring, not in the header

`m45.score()` is added **after** `u *= cfg.linearWeight`, so the five M5 weights are *not* scaled by
`linearWeight`, while every `w[i]` is. `cfg.linearWeight = 0.75393` (`v05.hpp:72`). The header
(`v05_target.hpp:232-243`) claims the placeholders are "scale-matched to the existing fitted
vector — lockout takes f[8]'s sign and magnitude (-3.0978), the last-live upside keeps f[11]'s
(+1.1660)". Two problems:

* those anchor values are v0.4-era. The shipped v0.5 vector is `w[8] = -2.90583`,
  `w[11] = 1.15962` (`v05.hpp`, `double w[NFEAT]` block).
* the *effective* magnitude of `w[8]` inside the score is `0.75393 × -2.90583 = -2.191`, against
  `mw[0] = -3.000` unscaled. The lockout placeholder is therefore **1.37× the weight it claims to
  match**, on a feature that also lives in [0, 1]. Same for `emptyLastSafe`: `1.166` against an
  effective `0.874`.

Measured (see §3): correcting for this moves the arm from 49.6 % to 51.6 %, both inside noise at
n = 1200. It is a hypothesis for the refit, not a result — but the "scale-matched" claim in the
header is false as written and should not be repeated in the paper.

---

## 2. `engine/src/v05_oppmodel.hpp` — M7

657 lines, header-only, `namespace fish::m7`.

### 2.1 What it implements

The header opens with the load-bearing algebra: `priorWeight`'s exponent
`θa − φ(T − a)` rearranges to `(θ+φ)a − φT`, and the second term depends on the **seat alone**, so
Sinkhorn's column normalisation erases it exactly. Hence v0.4's `(θ, φ)` is a single effective
`θ_eff = θ + φ`, and any new statistic must be at least (seat, half-suit)-indexed to survive.
I did not re-derive this numerically; the IPF argument is correct as stated (the initialiser is
`w = A(c,p)·g(p)`, and `diag(u) K diag(g) diag(v)` = `diag(u) K diag(gv)`).

Note the drift: the header and `fish m7check` both use v0.4's `(0.26380, 0.13280)`, but the shipped
v0.5 is `priorTheta = 0.44458, priorPhi = 0.12198` (`v05.hpp:29-30`), i.e. `θ_eff = 0.56656`, not
`0.39660`. Every θ-related number quoted in `M7-design.md` §1 and in `v05_oppmodel.hpp:38` is about a
parameter point v0.5 no longer occupies.

### 2.2 Public API

| symbol | line | meaning |
|---|---|---|
| `enum OppType`, `NTYPE = 5` | 138-139 | ordinary / v03like / silent / withholder / feint |
| `struct TypeProfile`, `types()` | 141-199 | rates measured by self-play census; `feint` ships with prior **0.0** |
| `pNeverReply/pNeverProbe` | 200-205 | residual, floored at 1e-3 |
| `struct M7Config` | 208-247 | `enabled, windowFast=2, windowSlow=6, dataBias=2.0, typeFloor=0.02, tiltCap=0.90, thetaMulCap=1.50, askWeight=0.30, declWeight=0.00, qLo/qHi, silenceChannel, thetaChannel, opportunity, persistTypes, carryWeight=0.70` |
| `struct Trace` | 250-254 | census hook — **no consumer anywhere in the tree** |
| `struct OppModel` | 257-491 | `reset, newDeal, renorm, holdsSetProb, askable, couldAskIn, resolve, onEvent, logTilt, thetaFor, mapType` |
| `fitTilted(Belief&, …, const OppModel*, double weight)` | 503 | structural copy of `Belief::sinkhornDisj` (`belief.hpp:478-529`) with a generalised initialiser |
| `jointSequentialTilted` | 586 | copy of `belief.hpp:535` |
| `jointSequentialMAPTilted` | 607 | copy of `belief.hpp:560` |
| `selfTest(kk, outer, inner, θ, φ)` | 642 | divergence guard; must return exactly 0.0 |

Two outputs: `logTilt` (a negative, capped, mean-field-divided log-weight on "card c sits with p")
and `thetaFor` (a per-seat multiplier on the certificate weight, clipped to [1/1.5, 1.5]).

### 2.3 How `M7.patch` wires it in

14 hunks in `v05.hpp`, 1 in `factory.hpp`, 1 in `main.cpp`.

* nine `V05Config` knobs (`m7, m7AskWeight, m7DeclWeight, m7DataBias, m7TiltCap, m7ThetaCap,
  m7Carry, m7Silence, m7Theta`) and their `optI/optD` parsers.
* `m7::OppModel opp; int lastSeat = -1;` on `V05Agent`; `reset()` full-resets on a seat change,
  otherwise calls `newDeal()`.
* `observe()` calls `opp.onEvent(e, k)` **after** `Agent::observe(e)` — post-event `Knowledge`,
  which is the ordering the diagnosis flagged as defect H.
* all five `sinkhornDisj` / `jointSequential*` call sites replaced with the tilted forms.
* a second posterior `Belief belDecl` + `dbel()` + `dPTeamCard()`, built only when
  `m7DeclWeight != m7AskWeight`.
* `main.cpp`: new command `fish m7check`.

---

## 3. Do they compile, and what do they do?

### 3.1 Patch application against HEAD — both fail

Verbatim, from the real tree (read-only `--check`):

```
$ git apply --check --verbose research/v05/patches/M4-M5.patch
Checking patch engine/src/v05.hpp...
error: while searching for:
  bool   forceStage2        = false;
  bool   repeatGuard        = true;
  bool   feasibleDecl       = true;   // M2 on the voluntary path too
};

struct V05Agent : Agent {

error: patch failed: engine/src/v05.hpp:114
error: engine/src/v05.hpp: patch does not apply
Checking patch engine/src/factory.hpp...
```

`M7.patch` fails identically at the same location. The cause is drift the patches themselves warned
about: `repeatGuard` changed `true → false` and gained a nine-line comment, and `ownershipByP` was
added, so `V05Config`'s tail no longer matches. Everything else still lines up — `patch -p1 -F 3`
applies each patch with one fuzz-2 hunk and a uniform offset of 11 lines:

```
M4-M5:  Hunk #2 succeeded at 126 with fuzz 2 (offset 11 lines);  8 others clean.
M7:     Hunk #2 succeeded at 126 with fuzz 2 (offset 11 lines); 13 others clean.
```

Placement was verified by eye in both sandboxes; the fuzzed hunk lands correctly inside
`V05Config`.

### 3.2 They do not compose

Applying `M4-M5.patch` first and `M7.patch` second:

```
4 out of 14 hunks failed--saving rejects to 'engine/src/v05.hpp.rej'
1 out of 1 hunks failed--saving rejects to 'engine/src/factory.hpp.rej'
```

The four rejected `v05.hpp` hunks are `@@ -20 @@` (the include), `@@ -114 @@` (the `V05Config` tail),
`@@ -130 @@` (the `V05Agent` member block) and `@@ -139 @@` (`reset`/`observe`); the `factory.hpp`
reject is the switch block. Both patches insert at exactly the same five anchors. **Shipping M4/M5
and M7 together requires a hand-merge, not two `git apply`s.**

### 3.3 Compilation — clean

```
c++ -std=c++20 -O3 -march=native -funroll-loops -fno-math-errno -Wall -Wextra -Wno-unused-parameter src/main.cpp -o fish -pthread
```

M4/M5 sandbox: exit 0, no diagnostics. M7 sandbox: exit 0, no diagnostics.
`c++ -std=c++20 -O2 -Isrc src/probe_m45_test.cpp -o m45test -pthread` builds in both the patched and
the **pristine** tree (it is self-standing), exit 0, no diagnostics. It is not referenced by
`engine/Makefile`, which builds only `fish`.

`fish m7check` passes in the M7 sandbox:

```
m7check: 1122 states, max |fitTilted(off) - sinkhornDisj| = 0   PASS   (--games=30 --seed=20260822 --a=v05:m7=0)
m7check:  728 states, max |fitTilted(off) - sinkhornDisj| = 0   PASS   (--games=20 --seed=777 --a=v05)
```

`./m45test --games=40 --seed=31 --a=v05 --b=v05` passes all six checks (A 0/4 586 976,
B 0/831 816, C 0/698 871, D 0/19 255, E 0/764 496, F 0), and reports M4+M5 at **4.00 %** of the
13 Sinkhorn solves the decision already pays for.

### 3.4 Off-switch equivalence — exact

`--games=100 --rotations=6 --seed=90210`, mirror, all three binaries:

| build | ask acc | decl/game | decl acc | forced | events/game |
|---|---|---|---|---|---|
| unpatched `v05` | 55.8189 % | 4.49 | 97.7357 % | 0.01 @ 33.3333 % | 96.1533 |
| M4/M5-patched `v05:m4=0,m5a=0,m5b=0,m5c=0,m5d=0` | 55.8189 % | 4.49 | 97.7357 % | 0.01 @ 33.3333 % | 96.1533 |
| M7-patched `v05:m7=0` | 55.8189 % | 4.49 | 97.7357 % | 0.01 @ 33.3333 % | 96.1533 |

Identical to every printed digit, including `lock hold 4.70474`. Both patches' off-switch claims hold
against the *current* tree, not just against the tree they were written for.

### 3.5 Head-to-head, M4/M5

`./fish match --a=<arm> --b=v05:m4=0,m5a=0,m5b=0,m5c=0,m5d=0 --games=100 --rotations=6 --seed=90210`
(n = 600 paired games; interval is the deal-clustered bootstrap):

| arm | win rate | mean sets |
|---|---|---|
| all M4/M5 on (shipped defaults) | **49.0 %** [45.33, 52.83] | 4.463 – 4.537 |
| `m5a` only (`m5b=0,m5c=0`) | 48.83 % [45.33, 52.50] | 4.430 – 4.570 |
| `m5b` only (`m5a=0,m5c=0`) | 50.67 % [47.33, 54.00] | 4.493 – 4.507 |
| `m5c` only (`m5a=0,m5b=0`) | 50.00 % [50.00, 50.00] | 4.500 – 4.500 |

`m5c` alone is **bit-identical** to the all-off build (zero-width bootstrap = every deal split 3–3),
independently confirming the patch's own "honest negative" for the last-live-opponent split.

Weight-scale probe, n = 1200 (`--games=200 --rotations=6 --seed=90210`), same opponent:

| `mw` setting | win rate |
|---|---|
| as shipped (`-3.0, 1.5, 1.166, -3.0`) | 49.58 % [46.83, 52.42] |
| × `linearWeight` (`-2.191, 1.131, 0.874, -2.191`) | 51.58 % [48.58, 54.58] |
| half (`-1.5, 0.75, 0.583, -1.5`) | 51.67 % [49.08, 54.25] |
| quarter (`-0.75, 0.375, 0.29, -0.75`) | 50.58 % [48.25, 52.83] |

Monotone-ish in the right direction, no cell separated from 50 %. **At the shipped placeholder
weights M4/M5 is worth nothing, and possibly slightly negative** — consistent with the patch's own
40-game caution, now at 30× the sample.

### 3.6 Head-to-head, M7

`--games=100 --rotations=6 --seed=90210`, unpaired arms (each arm plays the named opponent):

| opponent | `v05` (m7 on) | `v05:m7=0` |
|---|---|---|
| `withholder:k=6` | 71.17 % [67.67, 74.67] | **72.33 %** [68.83, 75.83] |
| `v04` | 51.50 % [47.67, 55.50] | 50.67 % [46.83, 54.50] |
| `silent` | 83.17 % [80.17, 86.17] | 82.00 % [78.83, 85.00] |
| `feint` | 52.67 % [48.50, 56.83] | 54.00 % [50.00, 58.00] |
| `v03` | 76.50 % [72.83, 80.00] | 77.50 % [73.83, 81.17] |

Nothing separates. The sign is **negative against `withholder:k=6`, the archetype M7 was built for**.

At `--rotations=1` (where `m7carry` fully engages), 300 deals vs `withholder:k=6`, seed 90210:

| arm | win rate |
|---|---|
| `v05:m7=0` | 71.00 % [65.67, 76.00] |
| `v05` (defaults) | 72.00 % [67.00, 77.00] |
| `v05:m7carry=0` | 72.33 % [67.33, 77.33] |
| `v05:m7carry=0.95` | 76.33 % [71.33, 81.00] |
| `v05:m7bias=0.5` | 72.67 % [67.67, 77.67] |
| `v05:m7sil=0` (θ channel only) | **77.00 %** [72.00, 81.67] |
| `v05:m7th=0` (silence only) | 73.67 % [68.67, 78.67] |

Again nothing separates, but the two best cells both **remove or bypass the silence channel**, which
is M7's headline mechanism. Worth designing a paired experiment around.

Cost: `v05:m7=0` mirror 97.2 games/s vs `v05` 86.3 games/s on the same box — about **11 % throughput**,
driven mainly by `needDeclBelief()` being true at the defaults (`m7DeclWeight 0.00 ≠ m7AskWeight 0.30`),
which builds a second full Sinkhorn fit on every `refresh()`.

---

## 4. Is the M4 soundness argument sound as written?

The header states two things (`v05_target.hpp:66-80`):
(i) every fact in `model(j)` is genuinely known to `j` — a sound **lower bound**;
(ii) it may **never** be used the other way: "`model(t)` not knowing something is NOT evidence that
`t` does not know it… no term here rewards an ask for being unreadable."

I tested both directions against ground truth — each opponent's *real* `Knowledge` object — with a
purpose-built probe (`…/scratchpad/work/m45sand/engine/src/probe_ignorance.cpp`, sandbox only).

### 4.1 Direction (i) holds, exactly

| | v0.5 mirror, 20 games seed 31 | v0.4 mirror, 20 games seed 90210 |
|---|---|---|
| (obs, opp, card) checks | 1 107 810 | 1 424 826 |
| model resolves the card to the actor's team | 56 072 (5.06 %) | 90 507 (6.35 %) |
| truth resolves it to the actor's team | 60 018 (5.42 %) | 95 784 (6.72 %) |
| **model fires where truth does not** | **0** | **0** |

The hard blackball certificate never claims an opponent knows something it does not. `hardLock` is a
proof of the *knowledge* half. (It is not a proof end-to-end: its second factor
`pCanAsk` is read off **our** posterior, `v05_target.hpp:356`, so the product is
certificate × estimate. The header's "the hard lockout term below is a proof and not an estimate"
overstates by one factor.)

### 4.2 Direction (ii) is violated — by `postMissLockout`, on by default

`v05_target.hpp:386`:

```cpp
      survivor = d; nSurv++;
      if (nSurv > 1) return 0.0;
```

`nSurv` is counted in `model(target)`, which has **fewer** exclusions than the target's real
knowledge, so `nSurv_model ≥ nSurv_truth`. `> 1` in the model therefore routinely hides `== 1` in
truth: the ask really does hand the target the card, and the term prices the leak at **zero**.
Because `mw[M45_LOCKOUT] < 0` and the term enters as `max(baseLock, postMiss)`
(`v05_target.hpp:493`), a suppressed leak makes the ask score *higher* — the candidate is preferred
precisely because our under-approximation cannot read it. That is the sentence in the header's own
paragraph (ii).

Measured over every legal (actor, opponent, card) triple:

| | v0.5 mirror, seed 31 | v0.4 mirror, seed 90210 |
|---|---|---|
| candidates | 516 267 | 631 196 |
| model `nSurv == 1`, truth `nSurv == 1` (correct fire) | 2 013 | 3 887 |
| model `nSurv > 1`, truth `nSurv == 1` (**suppressed real leak**) | **3 511** | **5 608** |
| model `nSurv == 1`, truth `nSurv != 1` (unsound fire) | 0 | 0 |
| model `nSurv == 1`, truth vacuous (over-fire, target already knew) | 273 | 350 |
| **detection rate on real one-survivor leaks** | **36.4 %** | **40.9 %** |

So the term is directionally safe (0 unsound fires) but **misses ~60–64 % of the leaks it exists to
price**, and the misses are systematically the ones that make an ask look attractive.

The same gap shows in the graded term. `baseLock[t]` is a `max` over `targetBeliefTeam` read off
`model(t)`; the model misses **5.5–6.6 %** of the card-level certificates the target actually holds,
and at **1.3–2.0 % of (actor, opponent) pairs** the truth has *some* certificate while the model has
**none at all** — i.e. the policy is told "this opponent can be handed the turn safely" when it
cannot. This is not a hypothetical: it is the C1 gap the header itself measures (0.119 model vs
0.270 own-knowledge resolved cards per triple, reproduced by `m45test`) showing up in the decision.

### 4.3 Direction (ii) is violated a second time, by `codebookValue` (off by default)

`v05_target.hpp:479`:

```cpp
      if (kj.owner[d] == UNKNOWN) need++;
```

`need` is the *teammate's* ignorance, read off `model(j)`, and it multiplies the codebook reward. An
under-approximation over-counts `need`, so the term systematically over-values signalling to a
teammate that may already know. Same forbidden direction, applied to a friendly seat. It is inert at
the shipped defaults (`m5d = false`, `conventions = false`), and the header already concedes the
convention is sender-side-only, but the claim "nothing in this patch…" in `M4-M5.patch` §2(ii)
should be narrowed rather than repeated.

### 4.4 Summary

The *architecture* is sound: the under-approximation is real, one-directional, and empirically
never wrong in the direction it claims. The *usage* is not: two terms consume the model's ignorance,
one of them shipped on, and the shipped one has a measured 36–41 % recall on the exact event it was
built to detect. For v0.6 this is not a reason to drop M5a — it is a reason to (a) state the recall
in the paper rather than the "Develin's worked example" framing alone, and (b) make the missing
half explicit as a separate, differently-signed feature so the fit can decide what to do with
"model is blind here".

---

## 5. What is missing before either can ship

### Both

1. **Rebase.** Neither patch applies to HEAD; both need their `V05Config` hunk regenerated.
2. **A merge, not two patches.** M4/M5 and M7 collide at five anchors (§3.2).
3. **No CI/regression target.** `engine/Makefile` builds only `fish`. `probe_m45_test.cpp` and
   `fish m7check` are the two guards these mechanisms depend on and neither is wired into any
   build/test target, `experiments_v05.sh`, or a KPI gate.
4. **No experiment-script coverage.** `grep -rn "m5a\|m7ask\|m45\|oppmodel" engine/*.py engine/*.sh
   engine/src/main.cpp engine/src/diag.hpp engine/src/arena.hpp` returns **nothing**.
   `experiments_v05.sh:36-38` (E5 ablations) enumerates only `m1/m2/stage2/m1p/norepeat`.
5. **No diagnostics path.** `M45Module::decisions/decisionsWithCertificate` and
   `OppModel::mapType/post` are never surfaced by any `fish` subcommand; every number quoted in the
   patches comes from standalone binaries.

### M4 / M5 specifically

6. **The fit does not exist and cannot be run as documented.** The patch claims the factory hunk lets
   `fish tune` fit `mw[]`. It does not: `factory.hpp` gains a *reader* at `K+14…K+18`, but
   `main.cpp:207-222` still pushes exactly 14 knobs and its bounds tables are `plo[14]`/`phi[14]`,
   so `fish tune --full` produces a 34-entry vector and never reaches index 34–38. `mw[]` can only
   be fitted by hand-supplying a 39-entry `--init`, where the extra entries silently inherit the
   default bounds [-12, 20].
7. **`freeze_config_v05.py` cannot bake them.** `KNOBS` has 14 entries and the script asserts
   `len(vec) >= NFEAT + len(KNOBS)`; five more entries and clamps are needed.
8. **`w[11]` must be refit alongside.** `m5c` changes what `f[11]` *means*; the current fitted
   `w[11] = 1.15962` prices the old, undifferentiated feature.
9. **Fix or document the `linearWeight` scale** (§1.4) before any weight is quoted as
   "scale-matched".
10. **Ablation switch for the ignorance branch.** There is no way to run `postMissLockout` with the
    suppression removed, so its 36 % recall cannot be ablated.
11. **A soundness test in CI.** `m45test`'s six checks are exactly the right gate; they run nowhere.

### M7 specifically

12. **Declaration firewall is one call site short — again.** The amendment fixed `evaluateSet`'s
    `cheap` (→ `dPTeamCard`). It did not fix the value path:
    `computeAggregates` (`v05.hpp:345`) builds `eH[]` from `pTeamCard()` (`v05.hpp:361`), i.e. the
    **tilted** `bel`; `proposeDeclaration` calls it at `v05.hpp:824`; `declareByValue` reads
    `eOld = eH[S]` at `v05.hpp:783`; and `declareNow` dispatches to `declareByValue` whenever
    `cfg.useValue && cfg.valueDeclare` — both `true` by default (`v05.hpp:70, 73`). So at
    `m7DeclWeight = 0` the ask-path tilt still steers *when and which* half-suit is voluntarily
    declared. (A head-to-head cannot isolate this — changing the ask path changes the trajectory —
    but the call chain is unconditional.)
13. **No `allparams` plumbing at all.** Unlike M4/M5, M7 has no flat-vector reader; `m7AskWeight`
    and `dataBias` cannot be fitted by the CEM even in principle.
14. **`askWeight = 0.30` is derived, not fitted**, by the header's and `M7-design.md` §5's own
    admission; `M7-design.md` §5 names the instrument (P3 §2's calibration table bucketed by
    silence-episode count). Nothing has run it.
15. **Type profiles are compile-time constants** fitted on **mirror self-play**
    (`M7-design.md` §3.1); `feint` ships at prior 0.0. No way to load refit profiles.
16. **`Trace` is dead code** — declared at `v05_oppmodel.hpp:250`, never instantiated.
17. **The carry gate resets every deal.** `newDeal()` (`v05_oppmodel.hpp:287-297`) blends `post`
    toward the prior with `carryWeight`, but sets `nSeat[p] = 0` and clears every `cell`. Both
    outputs are gated by those counters — `thetaFor`'s `shrink = nSeat/(nSeat+dataBias)`
    (`:480`) and `logTilt`'s `C.n == 0 → return 0` (`:450`). So at the start of every deal M7
    contributes exactly nothing regardless of `carryWeight`, and with the measured 2–5 uncensored
    episodes per seat per deal (`M7-design.md` §4) `shrink` peaks near 0.5–0.7. The carried
    posterior modulates only the *late* part of each deal.
18. **The rotation caveat is wrong at `--rotations=6`,** which is what `experiments_v05.sh` uses.
    `M7.patch` says carry "can only engage under `--rotations=1` or at a fixed table". Verified:
    `arena.hpp:86` sets `orient = rot / 3` at six rotations, so seats are stable across rotations
    0–2 and 3–5. Empirically, `--a=v05:m7carry=0` vs `m7carry=0.95` vs `v04`, seed 4242:
    `--rotations=2` gives **bit-identical** statistics (both arms 43.3333 %, ask acc
    55.6098 % / 56.2734 %) — carry inert, as documented. `--rotations=6` gives **different**
    statistics (`m7carry=0` 48.8889 %, ask 55.0494 % / 55.5175 %; `m7carry=0.95` 47.7778 %,
    ask 54.9026 % / 55.4041 %) — carry live, *across replays of the same
    deal*. That breaks the duplicate-block assumption that the rotations of one deal are
    exchangeable replicates, and it means any E5-style ablation at `--rotations=6` measures a
    carry effect that does not exist at a real table. Either force `m7carry = 0` in ablation runs or
    reset the model per rotation.
19. **`fitTilted` is an 78-line structural copy of a frozen header.** `m7check` guards it, but only
    at `(θ, φ) = (0.26380, 0.13280)` and `(0, 0)` — v0.4's point, not v0.5's `(0.44458, 0.12198)`.
20. **Block belief mode silently disables M7.** In `refresh()`'s Block branch the tilt is applied
    only on the `!blockOk` fallback; `v05:belief=block` therefore ablates M7 without saying so.
21. **Validation against a best-responder has not happened** (`M7-design.md` §5 item 4). An online
    per-seat model is a data-poisoning surface and none of the five archetypes observe v0.5's
    beliefs.

---

## Appendix — commands run

```
# sandboxes (copies; the real tree was never written to)
cp -R engine  <scratch>/work/{engine,m45sand/engine,m7sand/engine,bothsand/engine}
cd <scratch>/work/m45sand  && patch -p1 -F 3 < research/v05/patches/M4-M5.patch && (cd engine && make)
cd <scratch>/work/m7sand   && patch -p1 -F 3 < research/v05/patches/M7.patch    && (cd engine && make)
cd <scratch>/work/bothsand && patch -p1 -F 3 < …/M4-M5.patch && patch -p1 -F 3 < …/M7.patch   # 5 rejects

c++ -std=c++20 -O2 -Isrc src/probe_m45_test.cpp  -o m45test  -pthread
c++ -std=c++20 -O2 -Isrc src/probe_ignorance.cpp -o ignprobe -pthread     # written for this audit
./m45test  --games=40 --seed=31    --a=v05 --b=v05
./ignprobe --games=20 --seed=31    --a=v05 --b=v05
./ignprobe --games=20 --seed=90210 --a=v04 --b=v04
./fish m7check --games=30 --seed=20260822 --a=v05:m7=0
./fish match --a=v05 --b=v05:m4=0,m5a=0,m5b=0,m5c=0,m5d=0 --games=100 --rotations=6 --seed=90210
./fish match --a=v05 --b=v05:m7=0                         --games=100 --rotations=6 --seed=90210
```

Total engine runtime for this audit: under 3 minutes.
`probe_ignorance.cpp` lives only in the scratchpad sandbox; if v0.6 wants it, it should be added to
`engine/src/` as a sibling of `probe_m45_test.cpp` and wired into the same gate.
