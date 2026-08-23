# P1 verification — "E11's *no further information can ever arrive* is false"

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, commit `fe21e19`.
Adversarial re-check of the P1 deadlock finding (`P1-deadlock-forensics.md` §2).

**Verdict: the claim HOLDS UP.** E11's statement is false, and the defect is real.
Three of the offered numbers are seed-31 high-water marks and should be restated
with a pooled figure; one word in the claim ("provably") is wrong and matters for
how v0.5 can act on it.

Commands (from `engine/`):

```
make
./fish deadlock  --games=60 --dump=5 --states=3 --stride=40 --seed=31          # original
./fish deadlock  --games=60 --dump=0 --states=3 --stride=40 --seed=777001      # fresh
./fish deadlock  --games=60 --dump=0 --states=3 --stride=40 --seed=20260822    # fresh
./fish vdeadlock --games=60 --dump=1 --states=3 --stride=40 --seed={31,777001,20260822}
./fish oracle    --games=25 --maxdeals=40000 --samples=500 --seed=555
```

`vdeadlock` is a new independent probe, `engine/src/probe_vdeadlock.hpp`,
registered as an appended block in `engine/src/main.cpp`. Raw output:
`P1-verify-e11-independent-seed{31,777001,20260822}.txt`,
`P1-verify-e11-deadlock-seed{777001,20260822}.txt`.

---

## 1. The E11 text says what P1 quotes

`research/v04/results/E11-termination.md:3-7`:

> "Theorem 1 of the paper … implies that such a half-suit is frozen — and, for the
> same reason, that no further information about its allocation can ever arrive."

and again at `E11-termination.md:46-47`: "Theorem 1 freezes information as well as
ownership". Not a strawman; not hedged.

## 2. It is false at the level of the rules, before any measurement

`legalAsk` (`engine/src/fish.hpp:158-165`) requires only: the set is active, the
target is a live **opponent**, the actor does not hold the asked card, and the
actor holds some other card of the set. A half-suit locked to team T places all
six cards with T's three seats, so a member of T holds another card of the set and
lacks at least four of them — every one of those questions is legal. It is a
guaranteed miss, and `Knowledge::onEvent` publishes the exclusion regardless of
outcome: `exclude(e.card, e.actor)` at `engine/src/belief.hpp:167`, executed
before the `if (e.success)` branch. That single line is the whole refutation:
"the asker does not hold this card" is public, permanent information about the
allocation of a locked half-suit.

## 3. The cited measurement reproduces exactly, and survives two fresh seeds

Part 2 of `fish deadlock`, `dP` = increase in a teammate's exact
`BlockDP::bestTeamAllocation` on the team's candidate locked half-suit.

| seed | own-locked asks | with `dP>0` | mean `dP` | max `dP` | other asks with `dP>0` | mean `dP` |
|---|---:|---:|---:|---:|---:|---:|
| **31 (as published)** | 918 | **648 (70.59%)** | **+0.1261** | +0.6581 | 550/2061 (26.69%) | +0.00139 |
| 777001 | 1242 | 855 (68.84%) | +0.0794 | +0.5912 | 1329/4050 (32.81%) | +0.00101 |
| 20260822 | 720 | 469 (65.14%) | +0.0849 | +0.6640 | 497/2481 (20.03%) | +0.00125 |
| **pooled** | **2880** | **1972 (68.5%)** | **+0.0957** | **+0.6640** | 2376/8592 (27.7%) | **+0.00117** |

Seed 31 reproduced to the last digit (`648 / 918 (70.588235%) mean dP 0.126104
max dP 0.658119`). The rate is stable; the **mean is not** — 0.126 at seed 31 vs
0.079/0.085 elsewhere. Pooled mean is **+0.096**, about 24% below the headline.

## 4. `pAlloc` means what the claim says it means

The worry was that `bestTeamAllocation` returns `S[e]/Z` for a *count vector*
(`engine/src/blockdp.hpp:437-448`), which would overstate the probability of a
named card→seat allocation. `fish oracle` settles it against exhaustive
enumeration — no DP, no factorisation:

```
named allocation prob      max abs diff 0.000e+00 over 227062 checks
bestTeamAllocation         1679 checks, 0 inconsistent, 0 not argmax, max diff 0.000e+00
ORACLE PASS
```

So `pAlloc` is genuinely P(the MAP allocation is the true one) under the
observer's posterior.

## 5. Independent re-measurement, not using `bestTeamAllocation` at all

`fish vdeadlock` re-asks the same question with two quantities that avoid the
disputed function: **support size** (Σ over the half-suit's cards of the number of
seats still possible in the observer's `Knowledge` — pure combinatorics, no
probability) and **allocation entropy** (Shannon entropy of `BlockDP::marginals`
over the half-suit's cards). Same deadlock states; the unit is now
(state × locked half-suit × owning-team observer), and the ask is by a *teammate*
of that observer.

| seed | teammate asks inside the locked set | strictly shrink support | strictly lower entropy | mean ΔH | max ΔH |
|---|---:|---:|---:|---:|---:|
| 31 | 1836 | 786 (42.8%) | 1398 (76.1%) | 0.425 nats | 2.115 |
| 777001 | 2484 | 912 (36.7%) | 1437 (57.9%) | 0.301 | 1.678 |
| 20260822 | 1440 | 523 (36.3%) | 613 (42.6%) | 0.239 | 1.732 |
| **pooled** | **5760** | **2221 (38.6%)** | **3448 (59.9%)** | **0.325 nats** | **2.115** |

Asks **outside** the locked half-suit: **0 of 20 898** shrank its support, at any
seed. Triples where some legal own-locked ask was informative: **241 / 282**.
At the mover's own decision point, across all three seeds, the ask v0.4 actually
played was informative in **0 / 46** states while some legal ask was informative
in **42 / 46** — independently confirming P1 §2.5.

A concrete example from the seed-777001 dump (game 15190593729845910486 rot 0,
event 99), High Hearts locked to team 1:

```
observer s1: H=3.928 support=15;  best teammate ask s3 asks KH of s2 -> dH=1.102 nats
observer s3: H=5.772 support=19;  best teammate ask s5 asks JH of s2 -> dH=0.627 nats
observer s5: H=5.129 support=18;  best teammate ask s3 asks JH of s2 -> dH=1.220 nats
mover s1 played 9H@s0: dH to a teammate = 0.000; best legal ask would give 0.407
```

## 6. Where the claim overstates

### 6.1 "a half-suit the team **provably** owns" — wrong word, and it matters

The probe classifies by `lockOwner[]`, computed from `G.g.hand[]`, i.e. **ground
truth** (`engine/src/probe_deadlock.hpp:264-274`). It is not what any observer can
prove. At the 15 dumped seed-31 deadlock states, over all 36 (owning-team
observer × locked half-suit) pairs:

```
exact P(my team owns this half-suit):  mean 0.068  median 0.030  min 0.0048  max 0.342
pairs with pTeam > 0.999 (i.e. provable):  0 / 36
```

P1's own trace D says the same thing in prose ("four half-suits *are* locked and
**no owner knows it**") but §2.2 still writes "provably owns". The refutation of
E11 is unaffected — E11 quantifies over half-suits that are in fact locked — but
the *actionability* is weaker than the wording suggests: a v0.5 ask rule cannot
condition on this set. It would have to price expected information against
`pTeam`, which in these states is ≈0.07.

### 6.2 The ladder result is seed-31-specific

| seed | ladders | reached pAlloc > 0.9995 | median asks |
|---|---:|---:|---:|
| 31 | 21 | **21 (100%)** | **2** |
| 777001 | 36 | 28 (77.8%) | 4 |
| 20260822 | 25 | 19 (76.0%) | 4 |
| pooled | 82 | **68 (82.9%)** | **4** |

"0.220 → 1.000 in a median of 2 asks, 21/21" should be
"→ certainty in 83% of attempts, median 4 asks".

### 6.3 The direction ratio is seed-31-specific

MAP flips toward the truth vs away: 258:18 (14.3:1) at seed 31, but **216:171
(1.26:1)** at seed 777001 and 606:143 (4.2:1) at seed 20260822. Pooled
**1080:332 = 3.3:1**. Still the right sign, a quarter of the advertised strength.
(Note also that this counter is incremented over *all* enumerated asks, locked and
not — `probe_deadlock.hpp:534-543` — so it is not specific to the own-locked
class it is quoted next to.)

### 6.4 The "other asks" contrast is structural, not empirical

`dP` is measured on a *locked* candidate half-suit and C3a only ever touches the
asked card. An ask outside that half-suit can move it only through capacity
coupling. The +0.126 vs +0.0014 gap is therefore near-tautological — my support
metric makes it exact: 0/20 898. It is a correct number, but it is not evidence
that own-locked asks are unusually informative *relative to a policy's real
alternatives*; it is evidence that the certificate lands where it is aimed.

## 7. Failure modes checked and ruled out

- **Not a documented deliberate design.** `docs/FISHBOT_V04.md:97-101` contradicts
  E11 rather than defending it ("Asks inside a locked half-suit remain legal … so
  allocation information about it can continue to arrive"). P1 discloses this. The
  E11 text and `paper/fishbot_v04.tex` were never corrected.
- **Not a seed artefact.** Two fresh seeds, 3 600 own-locked asks beyond the
  original, same conclusion.
- **Not a broken statistic.** `bestTeamAllocation` is oracle-validated; the
  conclusion also survives replacement of that statistic by support size and by
  marginal entropy.
- **Not an inference bug masquerading as a policy bug.** The certificate path is
  the production `Knowledge::onEvent`, and the belief reports the miss correctly;
  it is the ask score that ignores the information.
- **Magnitude overstated** — yes, on three secondary figures (§6.1-6.3), corrected
  above. The headline rate (68.5% pooled vs 70.6% claimed) is not.
