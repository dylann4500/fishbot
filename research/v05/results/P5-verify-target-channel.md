# Adversarial verification — P5 claim: "target choice is a large *free* signalling channel that v0.4's dominant scoring term does not even look at"

Verifier task, FishBot v0.5 investigation. Repository `/Users/dylan/Documents/GitHub/fish optimization`,
working tree of 2026-08-22. Every number below is from a run made in this session.

**Verdict: the defect is real, both quantified halves of the headline are wrong.**
The target dimension carries the claimed bits, and v0.4 prices none of them as information —
that part survives. But (i) the channel is not free, it is *expensive*, and the 0.081 cost figure
is an artefact of the yardstick chosen; and (ii) `askExpectedValue` — the term the finding names
as "dominant" and cites for `(void)target;` — is the *least* discriminative half of the ask
score, contributing ~2% of the score variation that actually decides an ask. The term that
decides the target is the linear score, and it does look at the target.

---

## 1. Code claims — checked line by line

| Cited claim | Status |
|---|---|
| `askExpectedValue` begins `(void)target;` | **True.** `engine/src/v04.hpp:434-435` |
| "the only target-dependent terms in the linear score are material (`v04.hpp:317-320`)" | **Half true.** "All material" is right; "only 317-320" is wrong. Target-dependent features are `f[8]` (`v04.hpp:317`, `threatOf`), `f[10]` (`:319`), `f[11]` (`:320`), `f[16]` (`:325`, `exposureOf`) **and** `f[0] f[1] f[2] f[14] f[17] f[18]` (`:309,310,311,323,326,327`) through `p = bel.marg[card][target]` (`v04.hpp:287`). `f[9]` at `:318` is *not* target-dependent. |
| implied: the value term is blind to the target | **False as stated.** `askExpectedValue` is called with `f[0]`, i.e. `p = bel.marg[card][target]` (`v04.hpp:481`, `:495`), so it does vary with the target. `(void)target;` discards only the *seat identity*, not the target's effect. |
| not mentioned by the finding | The top-K refinement adds **two more** target-dependent terms, `chainWeight * p * follow` and `-threatWeight * (1-p) * threat`, built from a `Knowledge` clone that conditions on this target (`v04.hpp:520-545`), with `searchTopK = 6`, `chainWeight = 3.358`, `threatWeight = 2.613` (`v04.hpp:64-66`). Also material. |

The **substantive** part of the finding is confirmed and unaffected by the above: across all of
`features()` (`v04.hpp:285-329`) and the refinement (`v04.hpp:499-546`), **every** target-dependent
quantity is material — hit probability, the target's reply threat, the target's hand size, the
target's exposure, the follow-up hit probability. Nothing prices what the ask tells a teammate.

## 2. Reproduction of the cited measurement

`./fish humanchan` rebuilt from a clean `make` and re-run.

| | reported (seed 31) | reproduced (seed 31) | **new seed 777001** |
|---|---|---|---|
| ask decisions | 154,318 | 154,318 | 160,440 |
| decisions with ≥2 hard-indistinguishable targets | 46.6% | 46.630% | **43.92%** |
| mean free bits/ask | 0.639 | 0.63948 | **0.6071** |
| mean capacity-marginal spread in class | 0.081 | 0.08145 | **0.0823** |

v04-vs-v03: reported 66.2% / 0.919 bits (seed 90210); at **seed 555013** I measure 64.50% /
0.899 bits. **The capacity numbers reproduce exactly and are seed-robust.**

## 3. Where it breaks — the cost was measured with the wrong yardstick

`probe_human.hpp:116-127` prices the channel with a **capacity-normalised marginal**
`cap[t] / Σ cap`, which deliberately "drops disjunctions and half-suit lower bounds"
(its own comment, `probe_human.hpp:112-115`). Those are exactly the constraints v0.4's belief
uses. So the cost was measured against a proxy that is flatter than the policy's real posterior.

New probe `engine/src/probe_askchannel.hpp` instruments the **live** v0.4 agent
(`ChanAgent : V04Agent`, rng stream and pick unchanged — `refresh()` is called once either way,
`v04.hpp:196` is the only rng use on this path) and reads v0.4's *own* `bel.marg` and *own* ask
score inside the same hard-indistinguishable class.

```
instrumented v04, per decision with class >= 2
                                    v04 vs v04 (seed 31)  v04 vs v04 (777001)  v04 vs v03 (555013)
decisions with class >= 2                45.17%               42.34%              64.50%
spread of capacity marginal  (P5's stat)  0.0797               0.0819              0.0928
spread of v0.4's OWN bel.marg             0.2525               0.2518              0.2886
spread of v0.4's TOTAL ask score          3.038                3.006               3.408
  full score range over all ~41 asks      6.421                6.222               8.908
  intra-class range / full range          48.9%                46.8%               47.0%
  intra-class range > v0.4's top1-top2 gap 88.2%               87.9%               86.4%
```

- The real material cost of coding on the target dimension is **0.25–0.29 of hit probability**,
  not 0.081 — **3.1×** the reported figure, against a v0.4 baseline ask hit rate of 34.2%
  (`P0-v04-pathology.md`).
- In v0.4's own currency, moving the target inside the "indistinguishable" class moves the ask
  score by **3.0–3.4**, which is **~47% of the entire score range across every legal ask**, and
  **exceeds the margin between v0.4's top two candidate asks 86–88% of the time**.

**The class is indistinguishable to the hard certificate masks; it is not indistinguishable to
v0.4.** A v0.5 that codes on this dimension is not spending free capacity, it is overriding the
policy's leading preference in ~7 of every 8 such decisions.

## 4. Where it breaks again — "dominant scoring term" names the wrong term

`chooseAsk` scores `u = linearWeight·(w·f) + valueWeight·askExpectedValue` (`v04.hpp:476-481`).
Measured range of each part *across all candidate asks in a decision*:

```
                                 v04 vs v04 (seed 31)   v04 vs v03 (555013)
range of VALUE part per decision        0.121                 0.159
range of LINEAR part per decision       6.359                 8.783
```

The value term — the one containing `(void)target;` — supplies **1.9%** (mirror) / **1.8%**
(vs v03) of the score variation that selects the ask. Restricted to the target dimension alone
(all cards with ≥2 legal targets, 557,394 / 306,651 pairs):

```
mean range across targets of the same card:   p        VALUE part   LINEAR part   TOTAL
  v04 vs v04                                  0.1716    0.0428       1.5068       1.5192
  v04 vs v03                                  0.1747    0.0430       1.8156       1.8444
```

**99.2% of the target-driven score movement comes from the linear term**, which does look at the
target. `askExpectedValue` is a near-constant offset; whatever `valueWeight = 6.043` does to its
level, a term with a 0.12 range cannot dominate an argmax against a term with a 6.36 range.
Deleting `(void)target;` — recommendation 2 of `P5-human-strategy.md` §7 — would therefore expose
the target to the *weakest* half of the score.

## 5. The bits and the freeness live in disjoint populations

Splitting the same decisions by whether the ask v0.4 chose was provably dead (v04 vs v04, seed 31):

```
                                    chosen ask DEAD    chosen ask LIVE
decisions                                7,696             12,193
mean class size                          1.101             2.137
mean free bits                           0.0952            0.948
decisions with class >= 2                 8.7%             68.2%
spread of v0.4's own bel.marg in class    0.000             0.273
```

Where the target choice genuinely costs nothing (dead asks: every class member is at p = 0) the
class almost never has more than one member — **0.095 bits**. Where the bits are (live asks,
0.95 bits) the choice costs a quarter of a hit probability. The headline "0.639 free bits/ask"
is a mixture of a free-but-empty channel and a full-but-expensive one, and no decision offers both.

## 6. What survives

1. v0.4 prices **no** information on the target dimension. Confirmed in code across
   `features()`, `askExpectedValue`, and the top-K refinement. This is a genuine, exploitable gap
   and the finding is right to name it.
2. ~44–47% (mirror) / ~65% (vs v03) of decisions offer ≥2 hard-indistinguishable targets, worth
   0.61–0.90 bits. Reproduced at new seeds.
3. Corrected magnitude: the channel costs **0.25–0.29 hit probability** and **~47% of the
   decision's score range**, and overriding it flips v0.4's preferred ask in **86–88%** of the
   decisions where it exists. It is a **priced** channel, which is what §6 of
   `P5-human-strategy.md` already concluded for the *D13* channel and did not apply to its own
   headline result. A v0.5 wiretap trade-off must pay 0.25 hit probability per ~1 bit, not 0.08.

## 7. Reproduction

```
cd "/Users/dylan/Documents/GitHub/fish optimization/engine" && make
./fish humanchan --a=v04 --b=v04 --games=600 --seed=31
./fish humanchan --a=v04 --b=v04 --games=600 --seed=777001

c++ -std=c++20 -O3 -march=native -Isrc <scratch>/chanmain.cpp -o <scratch>/chan -pthread
<scratch>/chan --b=v04 --games=150 --seed=31
<scratch>/chan --b=v04 --games=300 --seed=777001
<scratch>/chan --b=v03 --games=200 --seed=555013
```

New file this session: `engine/src/probe_askchannel.hpp`. No protected header modified; `main.cpp`
untouched (the probe was driven from a standalone translation unit).

Byline: Dylan Nguyen, FishLab Research Project.
