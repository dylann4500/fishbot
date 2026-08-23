# P4-verify — adversarial verification of D1 (the forcing horizon's second stage)

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, at commit `fe21e19`.

**Claim under test (P4/D1).** *"The forcing horizon's second stage (`press>=2` at `nEvents>=308`)
cashes half-suits unconditionally: it zeroes `teamFloor`, zeroes `marginalGate`, and returns true
from `declareNow` before ever inspecting `pAlloc`. It is the single most expensive defect in the
policy."*

**Verdict: HOLDS UP.** Reproduced on a different seed, by a different route, and confirmed by a
constructed counterexample run against the shipped header. Magnitude was not overstated — at my
seed it is marginally larger than reported.

New tooling for this verification (nothing on the shipped decision path touched):
`engine/src/probe_vpolicy.hpp` + one appended `if (cmd == "vhorizon")` block in
`engine/src/main.cpp`. It `#include`s `v04.hpp` and instantiates the **shipped** `V04Agent`; it
does not use `probe_policy_v04.hpp`.

---

## 1. The code reads as claimed

All five citations verified verbatim in `engine/src/v04.hpp`:

| line | text |
|---|---|
| 583 | `if (pub.nEvents >= (7 * cfg.forceDeclareEvents) / 5) return 2;` — with `forceDeclareEvents = 220` (`v04.hpp:99`) this is `nEvents >= 308` |
| 607 | `double teamFloor = press >= 2 ? 0.0 : (press >= 1 ? 0.25 : cfg.minTeamProb);` |
| 608 | `if (!ignoreGates && cheap < (press >= 2 ? 0.0 : cfg.marginalGate)) return v;` |
| 675 | `if (press >= 2) return true;` — the **second** statement of `declareNow`, before any `pAlloc` read |
| 734 | `if (v.pAlloc > bestConf) { bestConf = v.pAlloc; d = v.decl; found = true; }` |

`cheap < 0.0` is unsatisfiable and `v.pTeam < 0.0` is unsatisfiable, so both gates are not merely
lowered but *disabled*. `bypass` is also forced on at `press >= 1` (`v04.hpp:697`), so the
cheap pre-gate does not save it either.

**One refinement to "unconditionally":** `evaluateSet` still returns `!ok` for a half-suit where an
opponent is *provably* located on a card (`v04.hpp:595-596`), and only the argmax set is cashed per
opportunity. Subject to that screen, the branch is unconditional in `pAlloc`.

---

## 2. Constructed misfire, shipped header, `nEvents` the only variable

`./fish vhorizon --seed=4242 --deals=200`. For each deal, a **fresh** shipped `V04Agent` is reset
with seat 0's nine cards and given a synthetic `PublicState` with no history at all — the agent
knows nothing beyond its own hand. The only thing varied is `pub.nEvents`:

```
deal   nEvents=219 (press 0)   nEvents=307 (press 1)   nEvents=308 (press 2)
0      no declaration          no declaration          DECLARE set=1 pAlloc=8.457e-04   <- WRONG
1      no declaration          no declaration          DECLARE set=2 pAlloc=8.457e-04   <- WRONG
2      no declaration          no declaration          DECLARE set=7 pAlloc=5.920e-03   <- WRONG
...
over 200 fresh deals:
  declares at nEvents=219 (press 0): 0
  declares at nEvents=307 (press 1): 0
  declares at nEvents=308 (press 2): 200   of which WRONG: 196 (98.00%)
  smallest stated pAlloc it was willing to cash at press 2: 8.457e-04
```

A one-event change in a public counter flips the shipped policy from "no declaration" to
"declare a half-suit I have zero evidence about, at a self-assessed 1-in-1200". This is the
defect in isolation, with no game dynamics and no probe copy in the loop.

Gate attribution on the same 200 states: all 200 would have been stopped by `marginalGate`
(0.008) alone at `press 0` (it returns first, at `v04.hpp:608`, before `pTeam` is even computed).

---

## 3. In-game numbers reproduced at a different seed

Their `p4horizon --games=200 --seed=31` vs mine at **seed 777001**:

| quantity | claimed (seed 31) | verified (seed 777001) |
|---|---|---|
| declarations at stated `pAlloc` in [0.0,0.1) | 130, 100.00% wrong | **136, 99.26% wrong** |
| voluntary declarations pre-horizon, wrong | 1.83% | **1.86%** |
| voluntary declarations at/after horizon, wrong | 66.54% | **68.35%** |
| games reaching the horizon that are NOT deadlocked | 0 | **0 of 48** |
| forced-endgame declarations wrong | 100% | **100% (6/6)** |

Their harness is faithful: `p4` mirror and shipped `v04` mirror agree exactly at seed 777001
(`p4match --a=p4 --b=p4 --games=150 --seed=777001`: events/game 146.0, 1345 declarations,
10.93% wrong; `match --a=v04 --b=v04 --games=150 --seed=777001`: events/game 146.02,
4.48333/game × 300 = 1345 declarations at 89.0706% correct).

---

## 4. Win-rate cost reproduced without their probe

The shipped `v04` exposes `force=` (`engine/src/factory.hpp:58`), so `v04:force=1000000` disables
both press stages on the **shipped** policy — a stricter removal than their `fix=32` (it also
disables the `urgent` trigger at `v04.hpp:710`).

| A | B | deals | A win rate | A decl wrong | B decl wrong | limit hits |
|---|---|---|---|---|---|---|
| `v04:force=1000000` | `v04` (stock) | 400, seed 777001 | **58.75 % [57.00, 60.62]** | 2.06 % | 17.91 % | 0 % |
| `p4:fix=64` (keep relaxations, require `pAlloc>=0.5`) | `p4` | 400, seed 777001 | **57.25 % [55.62, 59.00]** | 2.77 % | 15.93 % | 0/800 |
| `v04:force=1000000` | `v03` | 300, seed 777001 | 73.67 % [70.00, 77.03] | 1.38 % | — | 0 % |
| `v04` | `v03` | 300, seed 777001 | 73.67 % [70.00, 77.03] | 1.38 % | — | 0 % |
| `v04:force=1000000` | `detective` | 300, seed 777001 | 75.33 % [71.73, 78.61] | 1.58 % | — | 0 % |
| `v04` | `detective` | 300, seed 777001 | 75.33 % [71.73, 78.61] | 1.62 % | — | 0 % |

Claimed +8.13 pp; measured **+8.75 pp** at a different seed via a different mechanism. Against
`v03` the two configurations are *identical to the digit* (no game reaches event 220), and against
`detective` they differ by 0.00 pp. The effect is strictly a strong-opponent/mirror phenomenon —
which is what the reviewer said.

The `fix=64` row is the important isolation: keeping every `press>=2` relaxation but adding back
`pAlloc >= 0.5` recovers 7.25 of the 8.75 points. The *unconditional* second stage, not the
horizon as a concept, is where the cost lives.

## 5. It is deliberate and documented — and its stated justification is wrong in this harness

The escalation is not an oversight. `v04.hpp:565-575` and `paper/sections/06-locked.tex:186-190`
both describe it: *"past a second horizon its best candidate whatever the estimate, an undeclared
half-suit scoring nothing."* The paper also already concedes both horizons are hand-set constants
with no sensitivity study (`06-locked.tex:194-196`).

But "an undeclared half-suit scor[es] nothing" is **false inside the evaluation harness**. At the
action cap, `game.hpp:358-366` calls `adjudicateRemaining()`, and `game.hpp:271-289` awards each
unresolved half-suit to *the team physically holding the majority*. Their own `p4horizon` reports
the declaring team holds a mean of 4.08 of the six cards on a wrong declaration — i.e. the majority.
So in the harness, cashing at `pAlloc ≈ 10⁻³` converts a set the team would have been *awarded*
into a set handed to the opponents. Deliberate design, but the arithmetic behind it does not hold.

## 6. The one thing the branch buys: termination

Confirmed, and it is real. Both sides at `v04:force=1000000`, 300 deals, seed 777001:
**23 % of games hit `Rules::maxAsks = 400`** (events/game 171.3) versus 0 % for stock. Their
reported figure was 18.0 %. `p4:fix=64` mirror was 12.3 % in their run. So the second stage is
doing two jobs — terminate, and cash — and only the cashing rule is wrong. Any v0.5 replacement
must supply its own termination device.

---

## Reproduction

```
cd engine && make
./fish vhorizon --seed=4242 --deals=200
./fish p4horizon --games=200 --seed=777001 --deadcut=6
./fish match  --a=v04:force=1000000 --b=v04 --games=400 --seed=777001 --threads=8
./fish match  --a=v04:force=1000000 --b=v04:force=1000000 --games=300 --seed=777001 --threads=8
./fish p4match --a=p4:fix=64 --b=p4 --games=400 --seed=777001 --threads=8
./fish p4match --a=p4 --b=p4 --games=150 --seed=777001 --threads=8
./fish match  --a=v04  --b=v04 --games=150 --seed=777001 --threads=8
```
