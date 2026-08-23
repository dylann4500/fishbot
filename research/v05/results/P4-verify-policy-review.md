# P4-verify — adversarial check of D3 ("declaration aggregates from a stale posterior")

Dylan Nguyen, FishLab Research Project.
Repository `/Users/dylan/Documents/GitHub/fish optimization`, branch `main`, at commit `fe21e19`.

**Verdict: the claim HOLDS UP.** Every cited number reproduces exactly at the original seed and
survives at two fresh seeds. Two corrections, both of which make the defect *worse* than
reported, and one addition (a systematic direction to the error) that P4 did not report.

## Method — independent of the P4 harness

P4 measured this with `engine/src/probe_policy_v04.hpp`, a full mechanical copy of `v04.hpp`.
I did not reuse it. `engine/src/probe_polreview.hpp` instead **subclasses the shipped
`V04Agent`** and overrides only `proposeDeclaration`, replicating `v04.hpp:689-749` verbatim
(minus the `cfg.gateAudit` branch, off by default) with instrumentation, plus a `fixOrder=1`
variant that moves `refresh()` ahead of `computeAggregates()`. New CLI block `polreview`
appended to `main.cpp`. Nothing on the shipped path was modified.

Behavioural identity of the subclass against the shipped agent (single-threaded, 100 deals,
seed 31):

```
$ ./fish polreview --games=100 --seed=31 --measure=0
  A win 50%  sets 900-900  events/game 144.05  A decl 894 wrong 9.61969%
$ ./fish match --a=v04 --b=v04 --games=100 --seed=31 --threads=1
  mean sets 4.5-4.5  events/game 144.05  declarations 4.47/game at 90.3803%
```
(894 = 4.47×200; 9.61969 = 100 − 90.3803.)

## 1. The code claim — confirmed by reading

* `v04.hpp:698` `if (cfg.useValue) computeAggregates(pub);`
* `v04.hpp:705` `refresh();`
* `computeAggregates` reads the posterior through `pTeamCard` (`v04.hpp:358` → `v04.hpp:201-205`
  → `bel.marg`), and `bel.marg` is only ever rebuilt inside `refresh()` (`v04.hpp:163-198`).
* `observe()` sets `dirty = true` on every public event (`v04.hpp:161`); `refresh()` early-returns
  when `!dirty` (`v04.hpp:164`).
* The stale `eH[]`/`agg` are consumed by `declareByValue` (`v04.hpp:657` `eH[S]`, and `value()` at
  `v04.hpp:373-397` reads `agg` and `eH[]`), which is the optimal-stopping rule
  (`v04.hpp:679`).
* `chooseAsk` gets the order right: `refresh()` at `468`, `computeAggregates` at `472`.

No comment in `v04.hpp`, `docs/FISHBOT_V04.md`, `research/v04/`, or `paper/` describes this
ordering as deliberate (`grep -rn "computeAggregates|stale"` over docs/paper/research: no hits).
There is also no cost rationale available: the expensive call is `refresh()`, and it is *already*
deferred past the gate at `704`; `computeAggregates` sits **before** the gate, so the shipped
order wastes the aggregate computation on gate-rejected opportunities rather than saving anything.

## 2. The measurements — reproduced

`./fish polreview --games=100 --seed=<S>` (team A's three seats instrumented):

| statistic | seed 31 (P4's seed) | P4 reported | seed 777001 |
|---|---|---|---|
| declaration opportunities | 86 430 (press0 76 734 / press1 9 036 / press2 660) | 86 430 (76 734 / 9 036 / 660) | 85 878 |
| dirty belief on entry | **86 430 (100 %)** | — | 85 878 (100 %) |
| opportunities that consume the stale aggregates | 81 965 (94.83 %) | 81 965 (94.83 %) | 81 568 (94.98 %) |
| ... where `eH` numerically differs | 60 666 (74.01 %) | — | 60 618 (74.32 %) |
| mean max&#124;eH stale−fresh&#124; | **0.0636** | 0.0636 | 0.0610 |
| max max&#124;eH stale−fresh&#124; | **0.9091** | 0.9091 | 0.9621 |
| mean events since the agent last rebuilt `bel.marg` | 2.01 (max **171**) | "one event" | 1.76 (max 177) |
| `declareNow` verdicts flipped | **10** | 10 | 8 |
| final declaration action changed | 10 | — | 8 |

Every figure P4 cited is reproduced to the digit at seed 31, and every conclusion survives at
seed 777001.

## 3. Cost of the fix — reproduced, and null

`fixOrder=1` moves `refresh()` ahead of `computeAggregates()`:

| A | B | deals | A win rate (Wilson, n=2×deals) | sets |
|---|---|---|---|---|
| `fix` | stock | 600 | 49.83 % [47.01, 52.66] | **5405-5395** |
| `fix` | stock | 600 (seed 777001) | 50.25 % [47.42, 53.07] | 5403-5397 |
| `fix` | `fix` | 300 (seed 424242) | 50.00 % | 2700-2700 |

The first row matches P4's `fix=2` line exactly, point estimate *and* set split (5405-5395), from
a completely different implementation. P4's narrower interval [49.25, 50.42] is a paired/cluster
statistic, not Wilson; the conclusion — no measurable win-rate cost — is the same either way.

## 4. Correction 1 — "one-event-stale" understates the tail

Measured mean staleness is 2.0 public events, but the maximum is **171 events** (seed 31) /
**177** (seed 777001). The cause is the early return at `v04.hpp:704`
(`if (!candidate && !cfg.gateAudit) return false;`), which skips `refresh()`; 4 465/86 430 =
5.2 % of opportunities never reach it, so `dirty` stays set and the age keeps accumulating.
P4's attribution — "the last `refresh()` happened inside the previous `chooseAsk` at `:468`" — is
also wrong in the usual case: the last refresh is normally this agent's *own previous*
`proposeDeclaration` at `:705`, one event earlier.

## 5. Correction 2 — the staleness spans deals, not just events

`Belief::marg` has no initializer (`belief.hpp:454`) and `V04Agent::reset` (`v04.hpp:153-160`)
never clears it, while the match harness reuses one agent object across every deal
(`arena.hpp:67-71`). `Game::run` calls `declarationRound()` *before* the first ask
(`game.hpp:289-294`: `run()` opens its loop with `declarationRound()`). So at event 0 of every deal after the first, `computeAggregates` reads the
**previous deal's** posterior. Directly observed (`--dump=1 --dumpthresh=0.6`, 40 deals, seed 31),
in roughly half of all deals:

```
BIGDIFF game=22 seat=2 ev=0 set=2 eH_stale=1 eH_fresh=0.4 diff=0.6 ageAtEntry=0 handCount=9
BIGDIFF game=34 seat=0 ev=0 set=5 eH_stale=1 eH_fresh=0.4 diff=0.6 ageAtEntry=0 handCount=9
```
`eH_stale = 1` for a half-suit the seat does not hold outright (`eH_fresh = 0.4`) is only
reachable from leftover marginals. This appears inert — no flip was observed at `ev=0`; all 18
observed flips sit between event 37 and event 90 — but it is the same bug with a longer reach,
and it means the *reported* max of 0.909 is not driven by it (that one is mid-game).

## 6. Addition — the error has a systematic direction P4 did not report

Dumping every flip with its state: **18 out of 18 flips across both seeds are `1 → 0`** — the
stale posterior says *declare*, the current posterior says *wait*. All 18 are at `press=0`,
`urgent=0`, i.e. purely through `declareByValue`. Sample (seed 31):

```
FLIP game=23 seat=1 ev=57 set=1 press=0 urgent=0 pAlloc=0.675993 pTeam=0.811302
     eH_stale=0.828003 eH_fresh=0.966908  decision 1 -> 0  ageAtEntry=1
FLIP game=95 seat=5 ev=37 set=4 press=0 urgent=0 pAlloc=0.669697 pTeam=0.853554
     eH_stale=0.719178 eH_fresh=0.974627  decision 1 -> 0  ageAtEntry=1
```

Mechanism, and why the direction is not noise: `declareByValue` sets
`dC = -(2·eOld - 1)` with `eOld = eH[S]` (`v04.hpp:657-658`). A stale `eH` *understates* how much
of the half-suit the team already controls — because the event just absorbed is typically the one
that raised that control — so the control the declaration gives up looks smaller than it is, and
cashing looks cheaper than it is. The 18 flipped declarations were made at `pAlloc` 0.62–0.75,
i.e. roughly 30 % of them hand the half-suit to the opponents.

So the defect is not sign-neutral: **it is a small, systematic bias toward premature declaration
on uncertain allocations**, which is directionally the same failure the user reported from live
play. It is still too rare (10 per 100 deals, 0.32 % of the 3 170 `declareByValue` calls) to move
a 1 200-orientation win rate, so P4's "no measurable cost" verdict stands as stated.

## Failure modes ruled out

* *Statistic means something else* — partially. P4's "opportunities with a STALE belief 81965
  (94.83 %)" is really "opportunities that reached `refresh()`". The belief is dirty on **100 %**
  of entries; 94.83 % is the share that actually consumes the stale aggregates; 74.0 % is the
  share where the stale and fresh `eH` differ numerically. None of this changes the conclusion.
* *Deliberate and documented* — no. No mention anywhere, and no cost rationale exists (§1).
* *Vanishes at another seed* — no. Reproduced at 777001 and 424242.
* *Magnitude overstated* — no. If anything understated: staleness reaches 171 events and crosses
  deal boundaries, and the flips are 18/18 in the risky direction.

## Artefacts

* `engine/src/probe_polreview.hpp` — `RevAgent` (subclass of the shipped `V04Agent`), `runReview`.
* `engine/src/main.cpp` — appended `polreview` command block
  (`--games --seed --rotations --fixa --fixb --measure --dump --dumpthresh --belief`).
