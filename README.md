# FishLab

**A research workbench for six-player Canadian Fish (Literature), and the agents built in it.**

Every action in Fish is public, so the entire hidden state is the initial deal and the posterior over
that deal is exactly computable. That makes the game an unusually clean testbed for
imperfect-information team play: exact inference is available as an oracle, the domain is cheap
enough that one desktop plays hundreds of complete six-seat games a second, and a claim can be
checked rather than argued.

This repository holds the C++ engine, seven agent releases — the FishBot lineage from v0.2 to v0.6
and its successor **SESTINA v1.0** — the measurement apparatus, every artifact the reported numbers
were computed from, and the technical reports that describe them.

**Start here**

- **Read it** — SESTINA v1.0's technical report, 72 pages: [`output/pdf/sestina_v10.pdf`](output/pdf/sestina_v10.pdf)
- **Play it** — `cd engine && make && ./fish serve`, then open `http://127.0.0.1:8173`
- **Check it** — `cd engine && make && ./fish verify --games=600`
- **Bring your own bot** — any language, one JSON line per decision: [`docs/BOT_PACKAGE.md`](docs/BOT_PACKAGE.md)

---

## SESTINA v1.0

The frozen configuration is a single spec string, recorded with its 55-coordinate parameter vector in
[`engine/fishbot_v07.json`](engine/fishbot_v07.json):

```
v07:r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12,
    s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26
```

**On the name.** SESTINA v1.0 was developed and evaluated as **FishBot v0.7**, the seventh cycle of
the lineage, and was renamed at release; its predecessors keep the names they were published under.
Every seed, deal bank, digest, directory and configuration string still carries a `v07` identifier —
including `engine/fishbot_v07.json` and the spec string above — because that is what they were
committed and sealed as, and renaming them would break the provenance chain the results rest on.
Throughout, *SESTINA v1.0* names the agent and *the v0.7 cycle* names the programme that produced it.

It combines an approximate Sinkhorn fit to the deal posterior started from a fitted policy prior, a
linear ask and declaration policy, a public-history tie-break that preserves common knowledge among
teammates, a half-suit contestation weighting, a deduction-state stall detector in place of an
event-count termination rule, and a guarded determinized test-time search.

### What was measured

All figures below are on **sealed holdout material**, under a protocol registered before any of it
was played, with deal-clustered bootstrap intervals and replication across two disjoint deal banks
required in advance. The registered target was `F-cheap` rather than the deployed v0.6 policy on
purpose: the deployed policy ships its search off, so beating it is the easier claim.

| comparison | edge | 95% CI | games |
|---|---:|---|---:|
| **vs `F-cheap`** — the registered target: the v0.6 policy with its endgame-truncated search switched on, the cheapest configuration genuinely on the v0.6 frontier | **+3.33 pp** | [+2.88, +3.78] | 48,000 |
| vs the deployed v0.6 policy | +4.63 pp | [+4.19, +5.06] | 48,000 |
| vs `F-mid` | +2.89 pp | [+2.00, +3.78] | 12,000 |
| vs v0.5 | +5.18 pp | [+4.56, +5.81] | 24,000 |
| **vs the phase-2 composite** | **+0.15 pp** | **[−0.29, +0.59]** | 48,000 |

The registered decision rule for the primary comparison was three things at once: a pooled lower
bound above **1.53 pp** — a detection floor calibrated with planted edges, which the protocol states
does not buy down with games, so a larger cell does not lower it — a sign that replicates on both
banks, and a pass of the soundness gate. All three were satisfied; the banks read +3.67 and +2.99.

The last row is the paper's central qualification and is stated at the same weight as the first:
**SESTINA v1.0 does not measurably outperform a composite configuration assembled earlier in the same
programme**, so the architecture work that followed added no measurable strength. That was one of
seven conditions of non-confirmation named in the protocol before any holdout was played, and it is
recorded as having been met.

Also measured, and also reported:

- Over a shared **31-member opponent panel**, SESTINA's worst cell is **−0.04 pp** [−1.41, +1.33], which
  does not replicate in sign — +1.62 on one bank and −1.75 on the other, at a different panel member
  each time — and it is **3rd of four** on minimax regret, at 4.53 against `F-cheap`'s best-of-four
  4.00.
- The **attribution location test does not replicate**: one bank locates the gain in a single
  component, the other does not.
- **Eight independently constructed adversarial searches** were fitted against it; none found a
  positive edge at the tested budgets. That is a lower bound produced by a search, not a bound on
  what a search could find.
- Under partner substitution against a v0.5 opponent, 6 of 7 changed-partner rows stay positive; the
  worst is −0.19, which that battery does not resolve.
- The advantage persists under cross-play between independently trained runs and across eight rule
  dialects.
- SESTINA v1.0 costs **at least 4.52×** the non-searching blueprint. Every cost figure in the report is a
  lower bound, because the harness measures whole-match throughput.
- Four candidate mechanisms failed to produce measurable improvement at this resolution.

The phase-5 evaluation is **428 scored cells over 4,322,400 games**.

### What it does not claim

The report keeps three senses of "strongest" apart, and claims only the first:

| sense of "strongest" | status |
|---|---|
| **Lineage strength** — strongest configuration this project has produced | **established** |
| **Robustness** — does that strength survive panels, partners, dialects and fitted adversaries | evaluated extensively; **mixed results**, reported above |
| **Global standing** — strongest Canadian Fish agent anyone has built | **not established, not claimed** |

SESTINA v1.0 is not shown to be near-optimal and is not shown to be unexploitable. The comparison
class contains only agents written here.

### How the evaluation was run

The methodological difference between this cycle and the earlier ones is the point of the cycle:

- **Sealed holdout.** Seven deal banks of 24,000 deals were committed by digest, and the adversary
  half encrypted, before the configuration was frozen. Deals are generated from their index and never
  stored, so sealing is a public commitment rather than secrecy: `fish bankdigest` folds the six
  hands and the dealer of every deal into a rolling hash without constructing a policy or playing a
  game. The seal is enforced by the binary rather than by the battery script — every match goes
  through `runMatch`, which refuses a sealed seed and exits unless an explicit environment variable is
  set, which was set once and recorded. The protocol states the seal's two limits in advance.
- **Preregistration.** [`docs/v07/PREREGISTRATION.md`](docs/v07/PREREGISTRATION.md), 728 lines, fixes
  the battery, every cell and its sample size, every threshold, the replication rule, and the seven
  results that would mean the cycle is not an advancement — committed before any holdout bank had been
  played. The evaluation phase read only that document. It also fixes an amendment rule: if that
  phase finds a genuine flaw in the protocol it stops and reports it, because an amended protocol is
  a training run. Two of the seven conditions arose and are reported as such.
- **A freeze artifact.** [`engine/fishbot_v07.json`](engine/fishbot_v07.json) round-trips through the
  engine's own factory: verifying it re-executes the configuration rather than comparing strings.
- **Calibrated detection floors.** The battery's resolution was measured with planted edges of known
  size, including a sub-floor control that must *not* be recovered — and is not.
- **Mechanical side-channel controls**, a soundness gate applied before any strength number is
  computed, and identity controls certifying that the extended policy class with every new
  coordinate at zero is v0.6 bit for bit.
- **Provenance.** Every number in the report is a macro, not a typed digit: 274 are generated
  directly from artifacts and 146 are transcribed under a comment header naming the source document.
  [`paper/check_provenance.py --version v07`](paper/check_provenance.py) fails the build if any
  number lacks an attributed artifact that exists on disk.

### Where to read it

| what | where |
|---|---|
| Technical report (built) | [`output/pdf/sestina_v10.pdf`](output/pdf/sestina_v10.pdf) — 72 pages |
| Report source | [`paper/sestina_v10.tex`](paper/sestina_v10.tex), sections in `paper/sections_v07/` |
| Single-file copy for Overleaf | `paper/sestina_v10_standalone.tex` |
| Registered protocol | [`docs/v07/PREREGISTRATION.md`](docs/v07/PREREGISTRATION.md) |
| Results of record | [`docs/v07/FINAL-RESULTS.md`](docs/v07/FINAL-RESULTS.md) |
| Phase reports | `docs/v07/` — `INSTRUMENT.md` (what the instrument can see), `ADVERSARIES.md` (what beats the v0.6 frontier), `CANDIDATES.md` (five architectures, what survived), `THREAT-MODEL.md`, `SUBOPTIMALITY-LEDGER.md` |
| Pre-submission audit and its disposition | `docs/v07/AUDIT-REPORT-PRESUBMISSION.md` and `docs/v07/AUDIT-DISPOSITION.md` |
| Raw artifacts | `research/v07/` — seed banks, run logs and result files; the 31 `P5-*` files are the phase-5 evaluation of record, digested in `research/v07/results/MANIFEST-P5.json` |

There is no `docs/FISHBOT_V07.md`; for this cycle the technical report *is* the specification.

---

## Quick start

```bash
cd engine && make          # clang++ -std=c++20 -O3, produces ./fish
```

```bash
./fish verify --games=600                        # rules, information safety, belief soundness
./fish match --a=v06 --b=v05 --games=300 --rotations=6 --seed=90210
./fish serve                                     # play, at http://127.0.0.1:8173
```

`--games` counts **deals**, not games. `--rotations` is the duplicate-block size — each deal is
replayed that many times — so `--games=300` is 600 games and `--games=300 --rotations=6` is 1,800.
Use 2 or 6 and nothing else: 2 swaps which team holds A's policy, and 6 plays every cyclic seat
rotation so each policy holds every hand-triple exactly three times, cancelling the deal's intrinsic
luck. Every deal and every decision is reproducible from `--seed`.

The usage line advertises five subcommands; `engine/src/main.cpp` dispatches 46. Most of the rest are
probes from earlier cycles, still compiled in and still runnable. The ones worth knowing:

| command | what it does |
|---|---|
| `match` | one A team against one B team, with a deal-clustered bootstrap interval and a power line |
| `verify` | rules, information safety and belief soundness across a round-robin of baselines |
| `pathology` | the commit-gate KPIs: dead asks, dead runs, action-limit games, declaration accuracy |
| `selftest`, `oracle` | cross-check the exact belief engines against each other and against exhaustive enumeration |
| `gateaudit` | re-run the full evaluation on every half-suit the cheap declaration pre-gate rejects, and count false negatives |
| `seeds` | the reserved-seed registry, which is what makes a sealed bank refuse to play |
| `bankdigest` | a bank's commitment digest, computed without constructing a policy or playing a game |
| `v7through`, `v7decide`, `v7side` | throughput on both bases; per-decision metrics; the mechanical side-channel gate |
| `bots`, `serve` | third-party bot packages, and the browser table |

`--audit` turns on a per-event check that every agent's deduced knowledge still contains the truth;
`verify` forces it on. `--legacy` is a rule dialect rather than a policy switch, and changes four
things at once: out-of-turn declaration off, cardless declaration off, `maxAsks` 360, and the
forced-declaration ladder collapsed to a single threshold.

### Playing the frozen SESTINA policy

```bash
./fish match \
  --a='v07:r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26' \
  --b=v06 --games=400 --rotations=6 --seed=90210 --json
```

The published evaluation was run with a separate build of the same source (`-DFISH_NO_SERVE`), whose
SHA-256 and byte count the report records. Binaries are not committed. A fresh `make` reproduces that
build's play on the frozen spec: across six cells against v0.6 and v0.5 at three seeds, all 34
reported fields agree exactly. So a clone reproduces the *policy* even though it cannot reproduce the
*binary*.

### Reproducing the study

```bash
cd engine
BIN=./fish ./experiments_v07.sh                  # phase-1 instrument battery, 1-2 min
python3 build_tables_v07.py --paper              # artifacts -> paper/numbers_v07_generated.tex
python3 ../paper/check_provenance.py --version v07
cd ../paper && tectonic -X compile sestina_v10.tex --outdir ../output/pdf
```

The batteries default to `BIN=./fish7`, the binary of the original run, which a clone does not have;
point them at your own build as above. `experiments_v07.sh` opens with gates rather than
measurements — the reserved-seed registry, then identity controls that must reproduce v0.5 and v0.6
*bit for bit*, then the pathology KPIs — because a battery ordered by win rate first selects broken
policies: an earlier cycle's ablation table contains configurations scoring six points higher than
the shipped policy while carrying a 373-ask dead run and killing 14% of games at the action limit.

The full phase-5 evaluation is `python3 p5_battery.py <B2|B3|…|B10>`, which reads the protocol and
nothing else, is resumable, and takes on the order of twelve hours; `p5_analyse.py` reduces it to the
results tables. **Every recorded row carries the literal `argv` it was produced by**, so any single
cell in the study can be re-run by hand from the artifact that reports it.

To put a configuration of your own through the same commit gate:

```bash
FISH=./fish ./gate_v07.sh --spec='<your spec>' --id=mine   # exit 0 = PASS, 1 = FAIL
```

---

## Play it yourself

`fish serve` opens a browser table where any mix of humans and bots takes the six seats. It is the
same `Game` driver every published number came from — a human seat is just another `Agent` — so what
you are playing is the deployed policy, not a reimplementation of it. Each seat picks its engine from
a list spanning FishBot v0.2 through the frozen SESTINA v1.0 spec, the three deceptive archetypes, the
baseline population, and any outside engine that has been ported or installed as a package. Give the
seats names before you deal: six seats all labelled "SESTINA v1.0" are impossible to track.

```bash
./fish serve                                     # loopback
./fish serve --lan                               # players on the same wifi
./fish serve --public                            # players anywhere: prints one link to send
```

Off loopback the table is credentialed, because Fish is a hidden-information game and a shared
address without per-seat secrets would hand every player the other five hands. The host token governs
the table and confers no card visibility; a seat token is the only thing that discloses that seat's
cards. **Speak** reads every ask, declaration, turn pass and prompt aloud, which helps when you are
reading your hand rather than the log. See [`docs/PLAY.md`](docs/PLAY.md).

### Bring your own bot

Somebody else's engine plays here without a fork, a patch or a pull request. A bot is any program
that reads a game state and writes a move as one line of JSON.

```bash
./fish bots add mybot.zip
./fish bots check mybot                          # plays complete games, reports what it answered
./fish match --a=bot:mybot --b=v06 --games=400 --rotations=6
```

The manifest format is `fishlab-bot/1` and the wire protocol is `fishlab-json-v1`, both specified in
[`docs/BOT_PACKAGE.md`](docs/BOT_PACKAGE.md). The reference implementation is 249 lines of
dependency-free Python in [`examples/fishlab-bot-python`](examples/fishlab-bot-python). An installed
package is a policy spec like any other, so the whole measurement apparatus — duplicate blocks,
rotations, confidence intervals — points at it unchanged. Uploading runs nothing: only the host can
seat a bot or install its dependencies, because running one is running somebody's code on the host's
machine.

---

## The browser lab

Separately from the C++ engine, `app/` is a Next.js research console over a TypeScript engine
(`lib/fish-engine.ts`). It runs experiments, ranks games by how much they turn on a single decision,
and replays them action by action. Its policy population is FishBot v0.2 and v0.3, the archetype
opponents (lockout, hunter, diversifier, detective, bluffer, random), and a port of an independently
written external engine — KV's sampled-world determinization search (`lib/kv-search-agent.ts`). The
port reproduces every formula, constant and tie-break rule rather than the random stream, and the
repository carries the harness for checking its deterministic quantities against the Python original:
`fish kvparity`, `scripts/kv-parity-dump.ts` and `scripts/kv_parity_ref.py`.

```bash
npm install
npm run dev                                      # http://localhost:3000
npm run research -- --games=1000                 # headless pairwise strategy matrix
```

This is a different engine from `engine/`. Published FishBot numbers from v0.4 onward come from the
C++ engine; the browser lab is where the v0.2/v0.3 work was done and where replays are inspected.

---

## Release history

Each release is specified by its own document and evaluated in its own report. Numbers below are the
single most defensible headline for each, and each is a *comparative* claim within this project's own
lineage.

| release | what it established | headline |
|---|---|---|
| **SESTINA v1.0** | Strongest configuration in the lineage, on sealed material under a registered protocol — and no measurable gain over a composite the same cycle had already assembled | +3.33 pp [+2.88, +3.78] over `F-cheap`, 48,000 games |
| **v0.6** | A repaired optimiser. The strength gain is *entirely parametric*: exact-posterior tie resolution, three extra ask terms and the deliberate miss all measured null, and guarded test-time search was the single positive mechanism | beats v0.5 50.89% [50.61, 51.16] over 126,000 games, 7 of 7 banks above parity |
| **v0.5** | Not meaningfully stronger than v0.4. What it delivered was the elimination of a failure mode, and reporting that honestly is the result | provably dead asks in mirror play fall from **39.04%** of v0.4's asks to **0.011%**; head to head, pooled 50.77% with every bank's interval containing 50% |
| **v0.4** | The move to C++, the exact posterior over the initial deal, the locked-half-suit theorem, and the evaluation apparatus still in use: duplicate rotation blocks with a cluster bootstrap over deals | beats v0.3 75.07% [73.71, 76.40] over 4,200 games |
| **v0.3** | Count-conditioned beliefs by Sinkhorn scaling, held-out weight tuning, and an information-safety fix — v0.2's reply-risk term had indirectly consulted the target's hidden hand | beats the posterior detective 57.20% [55.02, 59.35] over 2,000 held-out games |
| **v0.2** | The first search policy, and a negative result: it does not beat the posterior-greedy detective baseline | 47.58% [46.20, 48.97] against the detective |

| release | specification | report |
|---|---|---|
| SESTINA v1.0 | *the report itself* | [`paper/sestina_v10.tex`](paper/sestina_v10.tex) → [PDF](output/pdf/sestina_v10.pdf) |
| v0.6 | [`docs/FISHBOT_V06.md`](docs/FISHBOT_V06.md) | [`paper/fishbot_v06.tex`](paper/fishbot_v06.tex) → [PDF](output/pdf/fishbot_v06.pdf), results of record [`research/v06/RESULTS-SUMMARY.md`](research/v06/RESULTS-SUMMARY.md) |
| v0.5 | [`docs/FISHBOT_V05.md`](docs/FISHBOT_V05.md) | [`paper/fishbot_v05.tex`](paper/fishbot_v05.tex) → [PDF](output/pdf/fishbot_v05.pdf), study design [`docs/V05_FINDINGS.md`](docs/V05_FINDINGS.md) |
| v0.4 | [`docs/FISHBOT_V04.md`](docs/FISHBOT_V04.md) | [`paper/fishbot_v04.tex`](paper/fishbot_v04.tex) → [PDF](output/pdf/fishbot_v04.pdf), results [`docs/V04_RESULTS.md`](docs/V04_RESULTS.md) |
| v0.3 | [`docs/FISHBOT_V03.md`](docs/FISHBOT_V03.md) | [`paper/fishbot_v03.tex`](paper/fishbot_v03.tex) → [PDF](output/pdf/fishbot_v03.pdf), findings [`docs/V03_FINDINGS.md`](docs/V03_FINDINGS.md) |
| v0.2 | [`docs/FISHBOT_V02.md`](docs/FISHBOT_V02.md) | findings [`docs/V02_FINDINGS.md`](docs/V02_FINDINGS.md) |

### Later cycles correct earlier ones, in writing

Corrections are recorded rather than quietly applied, and the SESTINA report's Appendix E lists this
cycle's. Three that a reader of the older documents should know about:

- **The cost of the frontier's search was wrong in both directions.** The "three orders of magnitude"
  figure divided an all-threads timing by a single-thread one, and it describes the *unrestricted*
  search, which is not the operating point anything was measured at. On a common basis the frontier
  point costs ~4.1× the non-searching blueprint, and every such figure is a lower bound.
- **v0.6's exploitability probe was mis-specified.** It reported that a fitted best response fails to
  reach parity against v0.6. Re-run with the responder in the target's own class and seeded at the
  incumbent's own vector, the exploiter reaches **+0.79 pp [+0.48, +1.10]** over 96,000 games, and an
  earlier, weaker run of the same repair had already found +0.76 [+0.15, +1.37] over 24,000. v0.6's
  in-class exploitability is *at least* 0.79 points. Every exploitability number in this project is a
  lower bound produced by a search, not a bound on what a search could find.
- **v0.6's "the tie group is worth exactly nothing" was measured against itself.** That cell was a
  self-mirror rather than a contrast. Run as a contrast, a free public-history tie-break is worth
  **+1.14 pp [+0.52, +1.77]** over v0.6.

If an older document and a newer one disagree, the newer one and its correction note are the record.

---

## Repository layout

```
engine/          C++ engine: the policies, the match harness, the browser table server,
                 the experiment drivers and the table generators.  src/ is the engine
                 proper; factory.hpp is the authority on how a spec string is applied.
paper/           LaTeX sources for every technical report, the generated number macros,
                 and check_provenance.py.
docs/            Per-release specifications, findings documents, PLAY.md, BOT_PACKAGE.md,
                 METHODOLOGY.md.  docs/v07/ holds the current cycle's phase reports.
research/        Every artifact the reported numbers were computed from: seed banks, run
                 logs, result files and digest manifests, one directory per release
                 (v04-v07), plus the v0.3-era browser-lab runs in research/results.
app/  lib/       The browser lab: a Next.js research console over a separate TypeScript
components/      engine, deployed as a Cloudflare Worker (worker/).
scripts/         TypeScript drivers for the browser-lab experiments, and the KV
                 parity-check harness.
examples/        Reference third-party bot package (Python).
output/pdf/      Built technical reports.
```

## Design principles

- Hidden information stays hidden from acting agents.
- Every deal and decision is reproducible from a seed.
- Strategy is expressed as inspectable numeric policies, not opaque calls in the hot loop.
- Soundness is gated before strength is measured; a policy that scores well while making provably
  dead asks is a broken policy, not a strong one.
- Final claims use held-out, orientation-balanced seeds, and a claim replicates on two disjoint banks
  or it is reported as not replicating.
- Negative results are results. A mechanism that was built, measured and found worthless is reported
  at the same weight as one that worked.

## Citing

The current work is the SESTINA v1.0 technical report:

> Dylan Nguyen. *SESTINA v1.0: Exact Inference, Robust Evaluation, and Adversarial Testing in
> Six-Player Canadian Fish.* FishLab, 2026.
> [`output/pdf/sestina_v10.pdf`](output/pdf/sestina_v10.pdf)

The report's reproducibility appendix carries the repository and the commit each battery ran at.

## Status

Research code, actively developed, single author. There is currently **no license file**, so default
copyright applies and the code is not open source; if you want to use any of it, open an issue.
