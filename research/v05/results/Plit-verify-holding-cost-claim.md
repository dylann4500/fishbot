# Plit-verify — adversarial check of the "V(wait) ≡ V(now)" deadlock claim

Verifying the claim in `research/v05/lit/v05-refresh.md` §1.3 and recommendation R1:

> v0.4's deadlock is not caused by "no further information can arrive" nor merely by an ask
> policy that ignores information — it is caused by the value function having no
> time/discount/holding-cost feature at all, making V(wait) == V(now) an identity of the
> implementation.

**Verdict: the code facts are correct; the causal attribution is wrong, and it is wrong in the
direction opposite to what the claim asserts.**

All runs below are v0.4 mirror self-play, 200 games × 2 rotations = 400 traces, scored with the
`diag.hpp` definitions. Seeds 777001 and 24680 — both different from the P0 baseline seed 31.
Instrumented copy: `engine/src/probe_literature_v04.hpp` (a copy of `v04.hpp`, in
`namespace fish::litp`); drivers `engine/src/probe_literature.hpp`; commands `fish litdecl`,
`fish litpath`, `fish lith2h` appended to `main.cpp`. No protected header was modified.

---

## 1. The code facts: all confirmed

| cited fact | status |
|---|---|
| `v04.hpp:669` `double vWait = value(pub, 0,0,0,0, scoreDiff, turnSign, 0,0,0,0);` | exact, verbatim |
| `value()` signature at `v04.hpp:370`, body through `:401` | correct |
| 16 coefficients at `v04.hpp:111–127`, features as listed | correct |
| `pub.nEvents` never appears in `value()` | correct — `grep -n nEvents v04.hpp` returns **only** `:583`, `:584`, `:710` |
| `forceDeclareEvents = 220` at `v04.hpp:99`, used at `:583–584` and `:710` | correct |

So V(wait) is literally the current-state evaluation, and outside the two `nEvents >= …` tests
the whole v0.4 policy is time-invariant. That much holds up exactly as written.

**One correction to the code claim itself.** "No holding-cost feature at all" is not accurate.
A *constant* holding cost is already implemented: `declareByValue` returns
`vDeclare > vWait + cfg.declareMargin` (`v04.hpp:670`) with `declareMargin = -0.0342`
(`v04.hpp:106`) — a strictly negative required edge, i.e. a constant subsidy for declaring
already in the shipped config. What is missing is a *time-varying* cost, not a cost.

## 2. The counterfactual: the claim's own fix does not fix the deadlock

R1 asks for exactly `nEvents / 220` on the wait branch. Implemented verbatim in the copy
(`probe_literature_v04.hpp`, `vWait -= cfg.litTimeCost * pub.nEvents / cfg.forceDeclareEvents`)
and swept. `timecost=20` is saturating: it exceeds the entire dynamic range of the value
function, so it is the limiting case of *any* monotone time cost.

`./fish litpath --games=200 --seed=777001 [--timecost=X]`

| | shipped | tc=0.5 | tc=2.0 | tc=20 | **liveonly=1** |
|---|---|---|---|---|---|
| events/game | 146.2 | 127.7 | 121.0 | 121.4 | **96.8** |
| p90 events | 313 | 312 | 311 | 311 | **111** |
| p99 events | 321 | 321 | 321 | 321 | **123** |
| max events | 322 | 322 | 322 | 322 | **126** |
| dead asks | 41.4% | 33.3% | 29.2% | 29.5% | **0.034%** |
| **longest dead run** | **289** | **289** | **289** | **289** | **1** |
| games w/ dead run ≥6 | 32% | 22% | 18.5% | 19% | **0%** |
| declarations wrong | 10.9% | 12.6% | 13.3% | 13.4% | **1.9%** |
| declarations at/after ev 220 | 474 | 310 | 268 | 276 | **0** |

The same sweep is reproduced by the shipped binary through the existing `vmargin` knob (a
constant holding cost is *identical* to shifting `declareMargin`):
`./fish pathology --a="v04:vmargin=-5.0" --b="v04:vmargin=-5.0" --games=200 --seed=777001`
→ events/game 121.4, p90 311, longest dead run 289, 19% of games with a run ≥ 6.
Adding `askfloor=0` on top: p90 302, longest dead run 289.

Seed 24680 reproduces the pattern (shipped p90 311 / longest run 286 / 9.1% wrong;
tc=2.0 p90 313 / longest run 286 / 14.6% wrong; liveonly p90 112 / longest run 1 / 2.1% wrong).

**A saturating time cost moves the mean and the median and does not move the tail at all.**
p90, p99, max events and the longest dead run are unchanged to the unit. Roughly 10% of games
still run past 300 events with an unboundedly strong holding cost applied at every step.
Meanwhile declaration error rises from 10.9% to 13.4%.

## 3. The mechanism the claim dismisses is the actual one

`liveonly=1` is a one-line ask-side filter using **only the actor's own public-information
`Knowledge`** — exactly `diag.hpp`'s definition of a dead ask — dropping every ask the actor can
*prove* is a miss, keeping the full list only when all asks are dead:

```cpp
bool dead = (k.owner[c] < NPLAY) ? (k.owner[c] != t) : !(k.mask[c] & (1u << t));
```

It eliminates the deadlock outright: longest dead run 289 → **1**, games with a run ≥ 6
32% → **0%**, declarations past the forcing horizon 474 → **0**, misdeclarations 10.9% → **1.9%**.
The 12 remaining dead asks equal the 12 starved turns exactly — i.e. every one is forced.

The complementary baseline number from `fish pathology`: **starved turns are 0.29% of asks**
while dead asks are 41%. **99.3% of v0.4's provably-dead asks are voluntary** — a live-possibility
ask was available and the ask score preferred a guaranteed miss.

And `--liveonly=1 --timecost=2.0` is byte-identical to `--liveonly=1` alone: once the ask side is
fixed, the time feature changes nothing whatsoever.

## 4. `declareByValue` is not even on the path in the deadlock

`./fish litdecl --games=200 --seed=777001` (350,892 declaration opportunities):

| | count | share |
|---|---|---|
| fully evaluated (past the cheap gate) | 331,418 | |
| **nothing certifiable — every live half-suit failed `evaluateSet`** | 284,812 | **85.9%** |
| ≥1 half-suit certifiable | 46,606 | 14.1% |
| `urgent` true | 168,694 | 50.9% (63.8% of those via `bestAskProbability < askFloor` alone) |
| certifiable but held back | 39,138 | |
|  · blocked by `declareByValue` | 10,428 | 26.6% |
|  · blocked by the urgent/threshold path | 28,710 | **73.4%** |
| mean best `pAlloc` when held back | 0.327 | |

Late slice (`nEvents ≥ 150`, the deadlock regime): **68.4%** of evaluated opportunities have
nothing certifiable at all, and of those held back, **90.6% (21,184) are blocked by the
urgent/threshold path versus 9.4% (2,194) by `declareByValue`**.

`urgent` is set by `v04.hpp:708–711`, and `bestAskProbability(pub) < cfg.askFloor` is precisely
the "no productive ask remains" condition — so in the deadlock the policy *bypasses*
`declareByValue` and falls to `pAlloc >= declThreshold || (locked && pAlloc >= 0.5)`
(`v04.hpp:678–679`). The line at `v04.hpp:669` that the claim identifies as the cause is the
binding constraint in **under 10%** of deadlocked blocked opportunities.

The mean best `pAlloc` of 0.327 when held back is the real story: the team usually cannot *name*
the allocation. That is an information deficit, which is the E11 / ask-policy framing the claim
tried to displace.

## 5. Play strength

`./fish lith2h --games=400 --seed=777001` (800 colour-balanced games, variant vs shipped copy):

| variant | sets for / against | win rate |
|---|---|---|
| control (no knob) | 3600 / 3600 | 50.00% (symmetry sanity check) |
| **R1 holding cost, `timecost=2.0`** | 3483 / 3717 | **46.13%** |
| **`liveonly=1` ask filter** | 3577 / 3623 | **49.75%** |

The recommended holding cost *loses* 3.9 points against shipped v0.4. The ask-side filter is
win-rate neutral and removes the deadlock entirely.

## 6. What survives

- §1.3's code reading is accurate and worth keeping, with the `declareMargin` correction.
- The R1 *design* observation — an undiscounted stopping rule is ill-posed — is a real
  well-posedness point, and the sweep does confirm the (deadlock, misdeclaration) frontier §1.3
  predicted. But R1's closing sentence, "it removes the structural cause of the deadlock rather
  than capping it at event 220", is false: it is a mean-shifter that leaves the tail exactly where
  it was and buys the mean shift with a worse declaration error rate and a worse win rate.
- The claim's dismissal of the ask policy ("nor merely by an ask policy that ignores
  information") is backwards. R2 — the information terms in the ask score — is the load-bearing
  recommendation, not R1.

## Caveat

`liveonly=1` is a *diagnostic* intervention, not a proposed v0.5 policy: it is a hard filter with
no notion of the certificate an intentional dead ask emits to teammates. Its role here is to
settle the causal attribution — an ask-side intervention alone is sufficient to remove the
deadlock, and a declaration-side one is not, at any strength.

---
Dylan Nguyen, FishLab Research Project — repository `fish optimization`, engine at `fe21e19`.
