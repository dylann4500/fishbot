# P6-verify — adversarial check of the declaration-race claim

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, at commit `fe21e19`.

**Claim under test** (from `research/v05/results/P6-declaration.md` §1.2–1.3, backed by
`research/v05/runs/P6/A_conf.json`):

> Declaration races are common (55.4% of declarations) but arbitration is decisive in only 16.7%
> of them, and in 70.6% of those decisive races BOTH candidate allocations are false.

**Verdict: the defect holds up; two of the three numbers are mislabelled.**
The raw counters reproduce bit-for-bit and survive three fresh seeds. But 55.4% divides two
incommensurate counters (correct value **48.1%**), and 16.7% is not a decisiveness rate at all
(arbitration changes *what is declared* in **27.1%** of races and changes *whether the team scores*
in **2.2%**). 70.6% reproduces but compares only two of what are usually three candidates.

## Method

* Exact reproduction of the cited run: `./probe_decl arb --games=3000 --x=3 --y=0 --seed=31`
  reproduced `A_conf.json` field-for-field (same `declRounds` 53713, `races` 29750,
  `contested` 4961, `bothWrong` 3501).
* Fresh seeds 777001, 20260822, 424242, same command.
* A new instrumented copy of the probe driver, so nothing protected was touched:
  `engine/src/probe_vdeclrace_game.hpp` (copy of `probe_declaration_game.hpp` plus counters),
  `engine/src/probe_vdeclrace.hpp`, `engine/src/probe_vdeclrace_main.cpp`.
  Build: `clang++ -std=c++20 -O3 -march=native src/probe_vdeclrace_main.cpp -o vrace -pthread`.
  Run: `./vrace --games=3000 --x=0 --y=0 --seed=<s>` (the **control** arm, both teams on
  lowest-seat arbitration, so the statistics are not contaminated by the confidence-ranked
  treatment the original run measured them under).

## 1. The three numbers reproduce

`./probe_decl arb --games=3000 --x=3 --y=0 --seed=<s>`:

| seed | declRounds | races | races/decls | contested/races | bothWrong/contested |
|---|---|---|---|---|---|
| 31 (cited) | 53,713 | 29,750 | **55.4%** | **16.7%** | **70.6%** |
| 777001 | 53,705 | 29,630 | 55.2% | 16.3% | 71.8% |
| 20260822 | 53,713 | 29,933 | 55.7% | 17.1% | 72.6% |

Internal arithmetic of `A_conf.json` is consistent: `lowRight 1218 = bothRight 1173 + lowOnly 45`,
`confRight 1415 = 1173 + confOnly 242`, `bothWrong 3501 = 4961 − (1173+242+45)`.
`declRounds` is genuinely the voluntary-declaration count: `xDeclPerGame + yDeclPerGame`
= 4.4753 + 4.4768 times 6,000 = 53,713 exactly.

So nothing is fabricated. The problems are in what the ratios mean.

## 2. "55.4% of declarations" — wrong denominator pairing. True value 48.1%.

`ArbStats::races` (`engine/src/probe_declaration_game.hpp:30`) increments **once per (team,
poll-iteration)** at `probe_declaration_game.hpp:241`, while `ArbStats::rounds`
(`:29`, incremented at `:277`) counts **executed declarations**. These are not in
one-to-one correspondence, in two ways visible in the code:

* the polling loop `for (int round = 0; round < NSET + 2; round++)`
  (`probe_declaration_game.hpp:209`) executes **one** declaration per iteration
  (`:277-278`) but scores a race for **each** team that had ≥2 proposers;
* when both teams want to declare, a neutral coin
  (`probe_declaration_game.hpp:274`) picks one; the loser is re-polled on the next iteration
  and its race is counted again.

Counting the aligned quantity directly — executed voluntary declarations where the **declaring**
team had ≥2 simultaneous proposers:

| seed | voluntary declarations | of which the declaring team was racing | share |
|---|---|---|---|
| 31 | 53,714 | 25,826 | **48.1%** |
| 20260822 | 53,712 | 26,026 | **48.5%** |
| 424242 | 53,712 | 25,856 | **48.1%** |

`races/declarations` = 0.553–0.557 in the same runs, i.e. the published figure over-states the
aligned one by ~7 pp. "More than half of all declarations are made in a round where a teammate
also wanted to declare" (P6 §1.2) is therefore **false as written**; the correct statement is
"just under half".

## 3. "arbitration is decisive in only 16.7% of them" — 16.7% is not a decisiveness rate

`contested` is defined at `probe_declaration_game.hpp:37` and set at `:247` as
`cbest != lowest`: *the confidence argmax is not the lowest seat*. That is a comparison of two
particular arbitration rules, not a measure of whether arbitration matters. Measured properly on
the same races (control arm, both teams lowest-seat):

| quantity | seed 31 | 20260822 | 424242 |
|---|---|---|---|
| races (team, poll-iteration) | 29,712 | 29,898 | 29,848 |
| ... candidate declarations are **not all identical** (arbitration changes *what* is declared) | **27.1%** | 27.1% | 27.9% |
| ... `contested` (conf argmax ≠ lowest seat) | 17.6% | 17.9% | 18.3% |
| ... ground-truth correctness **differs** across candidates (arbitration changes *whether the team scores*) | **2.2%** | 2.2% | 2.5% |

Both natural readings of "decisive" give a different number than 16.7%: arbitration changes the
named declaration 1.6x more often than the claim says, and changes the *outcome* 8x less often.
The 2.2% figure is the one that matters, and it makes P6's own conclusion (§1.6, "arbitration is
not worth engineering effort") **stronger** than the report argued it.

## 4. "70.6% both wrong" — reproduces, but "both" is 2 of usually 3 candidates

`bothWrong/contested` in the control arm: 71.3% (seed 31), 73.0% (20260822), 71.2% (424242).
The number is robust. Two qualifications:

* **87% of races are three-way**, not two-way (`vRace3` 25,912 of 29,712 races at seed 31;
  26,038 of 29,898 at 20260822). The `bothWrong` counter at `probe_declaration_game.hpp:263`
  scores only `decl[lowest]` and `decl[cbest]` and never looks at the third proposer.
  Restricted to contested races that genuinely have **exactly two** candidates, both-wrong is
  **10.3%** (78/754, seed 20260822) and **12.1%** (98/810, seed 424242) — not 70%.
* Over **all** races, the share where *every* candidate is false is **26.0–26.6%**, not 70.6%.

## 5. What does hold up, and is understated

The substantive point behind the claim — arbitration bites precisely where the team is guessing —
is real, large, and seed-stable. The declaration actually executed out of a contested round is
false **65.7–67.5%** of the time, against a 10.2–10.7% baseline over all voluntary declarations:

| seed | declarations executed from a contested round | of those, false | baseline decl accuracy |
|---|---|---|---|
| 20260822 | 3,302 | 2,230 (**67.5%**) | 0.898 |
| 424242 | 3,364 | 2,210 (**65.7%**) | 0.893 |

And the selection is genuine, not an artefact of conditioning on any race: non-contested races
are all-candidates-wrong only 15.7% of the time (7,762 − 3,900 = 3,862 of 29,898 − 5,340 = 24,558,
seed 20260822) versus 73.0% for contested ones.

## 6. Failure modes ruled out / found

| failure mode | result |
|---|---|
| number cannot be reproduced | **Ruled out.** `A_conf.json` reproduced exactly. |
| effect vanishes at another seed | **Ruled out.** 3 fresh seeds, all three ratios within 1.5 pp. |
| artefact of the confidence-ranked treatment arm the stats were collected under | **Ruled out.** The pure control arm (x=0,y=0) gives 17.6%/71.3% instead of 16.7%/70.6%. |
| the statistic does not mean what was said | **CONFIRMED for two of three numbers** (§2, §3). |
| the behaviour is deliberate and documented | Partly. Lowest-seat arbitration *is* deliberate and documented (`engine/src/fish.hpp:115-119`, `engine/src/game.hpp:203-207`, and the brief lists it). The claim is not that it is a bug, so this does not defeat it. |
| effect is opponent-specific | **Found, and it is.** v0.3 mirror (1,500 deals, seed 90210): races are 28.5% of declarations, contested is 56.1% of races, and both-wrong is **4.0%** (174/4,384). The 70.6% figure is a property of v0.4-mirror play, not of declaration races in general. The claim was scoped to the mirror, so this is a scope note rather than a refutation. |

## 7. Scope note on the measurement vehicle

All of these numbers come from `PGame`, not the shipped `Game`. The shipped
`Game::declarationRound` (`engine/src/game.hpp:201-231`) scans all six seats in **one** order and
takes the first proposer overall (`game.hpp:223`), so its arbitration is cross-team and
systematically favours team 0; `PGame` splits arbitration per team and breaks cross-team ties with
a neutral coin (`probe_declaration_game.hpp:271-274`). That is the right design for the A/B, and it
is documented in P6 §"Experimental design", but it means the race counts describe the probe's
variant of the rule, not the binary a user plays against.

## Corrected statement

> In v0.4 mirror play, **48%** of voluntary declarations are made in a round where a teammate also
> wanted to declare. Arbitration changes which allocation is named in **27%** of those races but
> changes whether the team scores in only **2.2%** of them. The races where the confidence argmax
> differs from the lowest seat (**17%** of races) are the guessing states: the declaration actually
> executed there is wrong **66%** of the time, against a 10% baseline, and in **71%** of them both
> the lowest seat's and the confidence argmax's allocation are false. Choosing the better of two
> guesses is therefore worth almost nothing — which is what P6 §1.4 measures (+0.30 pp).

## Reproduction

```
cd engine
clang++ -std=c++20 -O3 -march=native src/probe_declaration_main.cpp -o probe_decl -pthread
./probe_decl arb --games=3000 --x=3 --y=0 --seed=31        # reproduces A_conf.json exactly
./probe_decl arb --games=3000 --x=3 --y=0 --seed=777001
./probe_decl arb --games=3000 --x=3 --y=0 --seed=20260822

clang++ -std=c++20 -O3 -march=native src/probe_vdeclrace_main.cpp -o vrace -pthread
./vrace --games=3000 --x=0 --y=0 --seed=31
./vrace --games=3000 --x=0 --y=0 --seed=20260822
./vrace --games=3000 --x=0 --y=0 --seed=424242
./vrace --games=1500 --x=0 --y=0 --seed=90210 --spec=v03
```
