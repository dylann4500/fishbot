# V2 — final correctness audit of the FishBot v0.6 deliverables

Scope: `README.md`, `docs/FISHBOT_V06.md`, `research/v06/RESULTS-SUMMARY.md`, `paper/fishbot_v06.tex`
+ `paper/sections_v06/` + `paper/tables_v06/` + `paper/numbers_v06*.tex`, `research/v06/results/`,
`research/v06/runs/`, `engine/src/v06.hpp`, `engine/*.sh`, `engine/build_tables_v06.py`.

Method: every quantitative statement in the four documents was traced to a file in
`research/v06/results/` or `research/v06/runs/`; the pooled per-style table, the head-to-head pool,
the three search pools and the behavioural table were recomputed from the JSONL artifacts; the paper
was compiled with `tectonic --keep-logs` and the log grepped rather than eyeballed; all 35 MANIFEST
digests and all 34 rows of `tables_v06/manifest.tex` were recomputed; the engine was rebuilt from
scratch and the four control runs re-executed.

**The tree was being edited during this pass.** All findings below were re-verified against the
state at **2026-08-23 11:20**: `README.md`, `docs/FISHBOT_V06.md` and `RESULTS-SUMMARY.md` at
11:00:28, `paper/sections_v06/{04-engine,11-results,G-reproducibility}.tex` at 11:17–11:18,
`paper/numbers_v06_generated.tex` at 11:17:53. Two defects found earlier in the pass were fixed
mid-audit and are recorded at the end as CLOSED.

**Counts: 14 BLOCKER, 24 SHOULD-FIX, 6 NOTE.**

---

## What is correct (stated once, briefly)

* `paper/fishbot_v06.tex` compiles clean: **0 undefined references, 0 undefined citations, 0
  multiply-defined labels** (`grep -icE "undefined|multiply" output/pdf/fishbot_v06.log` → 0).
* **Every `\vsix…` macro used in a section or table is renewed in `numbers_v06_generated.tex`.** The
  only `\vsix…`-prefixed token not generated is `\vsixsearch`, the policy-name command from the
  preamble. 284 macros checked.
* `research/v06/results/MANIFEST.json`: 33 result files + `runs/fitA.jsonl` + `runs/fitC.jsonl`,
  **all 35 sha256 digests and byte counts recomputed and correct**; nothing present-but-unlisted.
  `tables_v06/manifest.tex`'s 34 rows also all verify. `fitC.jsonl`'s digest prefix
  `be4a4c31d9153208` matches `V6FIT_PROVENANCE` in `engine/src/v06.hpp:183`.
* Engine controls, re-run here:
  * `./fish pathology --a=v06:legacy=1 --b=v06:legacy=1 --games=40 --seed=31 --threads=3` is
    md5-identical to the same for `v05` (`7d2865b9a6614ce59cd0516f84e83b76`), both before and after
    a clean rebuild.
  * `./fish blockalias --a=v05 --games=20 --seed=31` → **0 QUERY mismatches** (95 raw shared-pool
    field reads differ, as documented).
  * `./fish verify --games=200` → `VERIFY PASS`, 0 violations / 6,737,436 checks.
  * `rm -f fish && make` → clean build, **zero warnings**, 6.8 s.
* Every command referenced by `experiments_v06.sh`, `followups_v06.sh` and `exploitability_v06.sh`
  (`match`, `verify`, `pathology`, `blockalias`, `v6probe`, `ablate`, `calibrate`, `gateaudit`,
  `bench`, `tune`, `selftest`, `oracle`) exists in `engine/src/main.cpp`'s dispatch chain.
* Recomputed from artifacts and confirmed exact: the E3 five-bank means (51.03 / 50.86) and every
  row of "what the refit actually changed" (sets 4.539/4.461, ask acc 55.00/55.12, decl acc
  98.50/97.47, decl/game 4.483/4.503, lock hold 4.578/4.816); the pooled per-style table cell by
  cell; minimax regret 3.06 / 8.60 / 11.65; the withholder gain +8.60 and its three per-bank
  replications +9.06 / +8.33 / +8.34; the head-to-head pool 50.53% over 11,300 games with z = 1.13;
  the search pool 52.64% over 2,160 games (53.75 / 52.08 / 52.08); the v0.6-vs-v0.6 pool 52.08% over
  2,880 games; the exploitability table (50.69 / 50.31 / 48.36, all n = 3,600); the partner table;
  E7 dialects; E8 ties; E9 throughput; the belief table (140,661 cards); E14 deviation rates; F6
  reconstruction; F7 tie-guard rates; the OLS slope +0.00049/gen at t = 1.60; the 285/294 alias
  figure (traces to `research/v05/results/P2-forced-endgame.md:271`).

---

## BLOCKER

### B-1. The paper's title block still prints scaffolding

`paper/fishbot_v06.tex:95-96` typesets, on page 1:

> Experiments `E1--E??` were produced at commit `XXXXXXX`

`\commitplaceholder` is `providecommand`-ed to `XXXXXXX` in `paper/numbers_v06.tex:55` and is never
renewed; the `% TODO(v0.6)` note above it is still in the source.

**Fix.** Replace lines 89–97 with the real range and SHA, e.g. `Experiments \texttt{E0--E15},
\texttt{F0--F9} and \texttt{X1} were produced at commit \texttt{<sha>}`; delete the TODO comment and
`\providecommand{\commitplaceholder}`. Same edit at `fishbot_v06_standalone.tex:89-96,155`.

### B-2. Two unresolved `\num…` placeholders reach the PDF

`paper/sections_v06/09-fitting.tex:35` and `:37` typeset:

> the maximum-to-minimum gradient weight ratio is **X.XXXX** against the 1 a true mean would give …
> and the "minimum" sits **XX.X** points away from the actual minimum

`\numSoftMinRatioFour` = `X.XXXX` and `\numSoftMinBelowMin` = `XX.X` in `numbers_v06.tex`, and
`build_tables_v06.py` never renews them. Re-checked against the 11:17 regeneration — still there.
These are the **only** two used-and-unresolved placeholders in the manuscript; the other 82
placeholder `\num…` macros all carry real values. The ratio is stated as **1.88** in `README.md:59`
and `docs/FISHBOT_V06.md:51`.

**Fix.** Set `\numSoftMinRatioFour` to `1.88` in `numbers_v06.tex` and supply the measured
soft-minimum-to-true-minimum gap for `\numSoftMinBelowMin`, or delete the clause.

### B-3. Three macros used as counts carry strings, producing nonsense sentences

`build_tables_v06.py` emits provenance *strings* into macros the prose uses as numbers:

| macro | generator | current value | renders as |
|---|---|---|---|
| `\vsixWallClock` | `build_tables_v06.py:827` — `time.strftime('%Y-%m-%d')` | `2026-08-23` | `G-reproducibility.tex:199` — "The battery ran in **2026-08-23** hours on 15 cores." |
| `\vsixProvGenerated` | `:481` — the v06.hpp provenance stamp | `fitC.jsonl [final weights record]` | `G-reproducibility.tex:298` — "…**fitC.jsonl [final weights record]** are generated straight from an artifact…" |
| `\vsixProvTranscribed` | `:626` | `research/v05/results` | `G-reproducibility.tex:299` — "…and **research/v05/results** are transcribed from a report that already existed" |

**Fix.** Rename the generator's three emits (e.g. `\vsixFitProvenance`, `\vsixProvSource`) and emit
real quantities for the prose: wall-clock hours of the battery, and the counts of generated vs
transcribed figures — the generator already knows both (`len(emitted)` and the `\num…` count).

### B-4. "ahead of v0.5 on nine of the thirteen styles and behind on four" is false

Recomputed from `E4-perstyle.jsonl` + `F4-perstyle-banks.jsonl`, the pooled table gives, for
v0.6 − v0.5: **7 ahead** (v0.5 +1.00, v0.3 +2.35, detective +0.83, diversifier +1.33, hunter +0.78,
silent +1.92, withholder +8.60), **5 behind** (v0.4 −0.94, v0.2 −1.72, lockout −1.25, bluffer −0.06,
feint −0.06) and **1 tie** (random, 100.00 vs 100.00). Not 9 and 4. The table printed directly above
each of these sentences already shows this; only the sentence is wrong.

`README.md:22-23`, `research/v06/RESULTS-SUMMARY.md:70`,
`paper/sections_v06/11-results.tex:129-130`, `fishbot_v06_standalone.tex:3084`.

**Fix.** "v0.6 is ahead of v0.5 on seven of the thirteen styles, behind on five — none by more than
1.72 points — and tied on random." The "none by more than two points" half is correct.

### B-5. The paper states the search verdict as NEGATIVE in eight places

The abstract, §8.4 "What is established", the shipped `v0.6-Search` configuration and the README all
say the guarded search is the one positive mechanism (52.64% over three banks against v0.5; 52.08%
over four cells against v0.6). These eight passages say the opposite and are leftovers of the
pre-F0 draft:

| file:line | text |
|---|---|
| `01-introduction.tex:40` | "We report the deviation-rate falsifier that says why the guarded version still **has nothing to add**." |
| `08-search.tex:7` | "measures it honestly, and **reports that in this game it does not pay**" |
| `08-search.tex:192-193` | "It is **the best search configuration measured** and it is **still not separated from the blueprint**." |
| `11-results.tex:164-166` | "once the statistic is corrected the search **has nothing left to add**." |
| `13-limitations.tex:26-27` | "**Test-time search is reported as not paying**, and the conclusion rests on…" |
| `14-conclusion.tex:12-13` | "The search is dominated by the variance of its own return … and once that is corrected **it has nothing left to say**." |
| `E-search-detail.tex:73-84` | subsection titled "**Why the guarded search still converges to the blueprint**"; "**There is no setting at which the search deviates rarely and usefully.**" |
| `D-ablations.tex:52-54` | "Off; the deviation rate is either an order of magnitude above the healthy band **or zero**" |

`08-search.tex:192` is also numerically false: `\vsixSearchDead` = **50.28**, which is the *worst*
of the six guarded ladder rows, not the best (κ = 4 is 53.89, κ = 2.5 is 53.75).

**Fix.** Restate each as measured, matching `08-search.tex:129-139`. `E-search-detail`'s subsection
should be retitled ("Why the deviation rate is not the falsifier it looks like") and its "no
setting" sentence replaced by the tie-group-churn decomposition already at `08-search.tex:113-118`.
At `08-search.tex:192-193`, say that the deliberate-miss-in-the-candidate-set variant scores
`\vsixSearchDead`% and is not separated from the blueprint — dropping "best configuration measured".

### B-6. The same defect in the other two documents

* `research/v06/RESULTS-SUMMARY.md:193-199` lists **test-time search under "What did NOT
  contribute"**, as "−27 points unguarded; converges to the blueprint guarded". Both halves are
  wrong: the same file's §"Test-time search" measures −35.70 unguarded and 52.64% guarded, and calls
  it "the one place in this study where a multi-step method beats a static rule" (`:86`).
* `docs/FISHBOT_V06.md:143-146`, inside §4.3 whose Result 2 is the positive one, still reads: "A
  paired lower-confidence-bound deviation rule recovers **all 27** monotonically. Guarded, the
  search **converges to the blueprint** and moves the action on ~34% of searched decisions". The
  "27" is a dangling fragment of the superseded figure; "converges to the blueprint" is refuted.

**Fix.** Remove test-time search from the RESULTS-SUMMARY "did NOT contribute" list (leaving
exact-posterior tie resolution, the three extra ask terms and the deliberate miss) and cross-refer to
its own §"Test-time search". Delete the two stale sentences from docs §4.3's "Not resolved"
paragraph, keeping the tie-group / unrestricted / guarded comparison and the cost.

### B-7. The deliberate-miss numbers are stale in three files and contradicted by E15

`research/v06/results/E15-deliberate-miss.txt` is the artifact of record (200 deals × 6 = 1,200
games for the match, 200 × 2 = 400 for the pathology block):

| quantity | quoted | E15 artifact | paper macro (correct) |
|---|---:|---:|---|
| win rate vs v0.5 | 52.33% | **51.1667%** [48.3393, 53.9866] | `\vsixDeadWin` = 51.1667 |
| dead asks | 27.1% | **35.8476%** | `\vsixDeadPathDead` = 35.85 |
| longest dead run | 364 | **365** | `\vsixDeadPathLongest` = 365 |
| games killed by the action limit | 7.5% | **10%** | `\vsixDeadPathLimit` = 10.00 |

Stale at `README.md:65`, `docs/FISHBOT_V06.md:152-155` and `engine/src/v06.hpp:126-128`. The 52.33%
traces only to `research/v06/notes/R12-mechanism-trials.md:38`, a smaller superseded run.

**Fix.** Replace all four figures in all three files with the E15 values and cite
`research/v06/results/E15-deliberate-miss.txt` instead of R12.

### B-8. The gate-audit figure does not trace and is off by a factor of 22

`README.md:76-77` and `docs/FISHBOT_V06.md:37`: "**0 false negatives in 666,689 rejections**".
`research/v06/results/E10-gateaudit.txt` reports **5,472,906** declaration opportunities,
**31,321,233** (opportunity, half-suit) pairs and **14,449,770** gate rejections, 0 false negatives.
`666,689` appears nowhere in `research/v06/results/` or `research/v06/runs/` — only in
`PAPER_PLAN.md` and the V1 note, which flagged it as pending E10 before E10 existed. The paper is
correct (`\vsixGateRejected` = 14,449,770, `\vsixGateOpportunities` = 5,472,906).

**Fix.** "0 false negatives in 14,449,770 gate rejections over 5,472,906 declaration opportunities"
in both files.

### B-9. "52.92% against v0.6 itself" is one cell of four

`F0-search-confirm.jsonl` measures the search against v0.6 on **four** cells, all n = 720:

| config | bank | win |
|---|---:|---:|
| `s1=1,det=12,cand=4,kappa=2.5,roll=v06` | 90210 | **52.92** |
| `s1=1,det=12,cand=4,kappa=2.5,roll=v06` | 31337 | 50.83 |
| `s1=1,det=12,cand=4,kappa=2.5` | 90210 | 51.81 |
| `s1=1,det=12,cand=4,kappa=2.5` | 31337 | 52.78 |

Pooled: `\vsixSearchLift` = **52.08%** over 2,880 games, 4 of 4 above parity, worst 50.83%. Even the
shipped `roll=v06` spec alone means 51.88%, not 52.92%.

State at 11:20: `README.md:31-34` (the bullet) and `RESULTS-SUMMARY.md:96` were **fixed** mid-audit
and now report 52.08% correctly. Still wrong: **`README.md:63`** (the verdict table row — "52.64%
against v0.5 over 2,160 games and **52.92% against v0.6**", now contradicting the bullet nineteen
lines above it) and **`docs/FISHBOT_V06.md:17-18`**. Quoting the best of four cells is the same
selection error the head-to-head section of every one of these documents exists to disavow.

**Fix.** Apply the README bullet's wording to `README.md:63` and `docs/FISHBOT_V06.md:18`: "52.08%
against v0.6 over 2,880 games and four cells, all four above parity, worst 50.83%".

### B-10. The 52.64% is attributed to the wrong configuration

`docs/FISHBOT_V06.md:15` defines **FishBot v0.6-Search** as
`v06:s1=1,det=12,cand=4,kappa=2.5,roll=v06`, and `:17` then says "**v0.6-Search** takes 52.64%
against v0.5". The 52.64% was measured on `v06:legacy=1,s1=1,det=12,cand=4,kappa=2.5` — v0.5's
parameter vector with the v0.5 rollout blueprint (`E12-search.jsonl` row 5;
`F0-search-confirm.jsonl` rows 1–2). No run behind that number used the v0.6 vector or the v0.6
rollout. Same conflation in the `README.md:63` verdict row. `README.md:31-33` (post-11:00) and
`08-search.tex:129-135` both state it correctly — "On v0.5's vector … On v0.6's own vector …".

**Fix.** Adopt that two-clause split in `docs/FISHBOT_V06.md:17-18` and `README.md:63`.

### B-11. The paired-panel-ablation claim is stated three different ways and none of them traces

| source | claim |
|---|---|
| `abstract.tex:46` | "better than v0.5 on the paired panel **at all three banks measured**" |
| `14-conclusion.tex:24` | "ahead of v0.5 on the paired panel comparison **at all three banks** it was measured on" |
| `RESULTS-SUMMARY.md:184-191` | "**two held-out banks**, 500 deals × 2 rotations per cell": 515253 → 0.6870, −0.0230 [−0.0364, −0.0099]; 90210 → 0.6823, −0.0134 [−0.0266, +0.0000] |

The only `ablate` artifacts in the study are `E5-ablations.json` (`experiments_v06.sh:60-62`) and
`F1-chain2x2.json` (`followups_v06.sh:31-33`), both at **one bank, seed 606060, 400 deals × 2
rotations**, giving v0.6 − v0.5 = **+0.02687 [+0.00979, +0.04354]**. The four numbers in the
RESULTS-SUMMARY table appear in no artifact, no run log and no note; `battery.log` and
`followups.log` contain no `ablate` invocation at 515253 or 90210, and no file in `results/` or
`runs/` contains `0.6870`, `0.6823` or `0.0134`.

**Fix.** Either run the two- or three-bank paired panel (`ablate --ref=v05 --variants=v06` at 515253
and 90210) and cite it, or reduce all three statements to what E5/F1 support: "ahead of v0.5 on the
paired six-opponent panel at the one bank it was measured on — +2.69 points [+0.98, +4.35], seed
606060, 800 games per cell". This is the largest untraced number in the deliverables, and it is
load-bearing: it is what the abstract, the conclusion and `11-results.tex:69-71` offer as the thing
that *is* separated when the head-to-head is not.

### B-12. `\vsixRegretFive` (9.06) contradicts `\vsixPoolRegretFive` (8.60) in the same paper

`13-limitations.tex:10-11`: "Minimax regret over the **thirteen-style set** is `\vsixRegretSix`
(3.06) for v0.6 against `\vsixRegretFive` for v0.5" → prints **9.06**. `11-results.tex:128` and
`perstyle_pooled.tex` print **8.60** for the same quantity, and 8.60 is the headline in README,
RESULTS-SUMMARY and the paper's own results table. `\vsixRegretFive` = 9.06 and `\vsixRegretFour` =
11.56 are the *single-bank* (E4, seed 515253) regrets.

**Fix.** Use `\vsixPoolRegretFive` / `\vsixPoolRegretFour` at `13-limitations.tex:11`, or name the
bank. Delete `\vsixRegretFive`/`Four`/`Six` from the generator if nothing else uses them.

### B-13. `paper/fishbot_v06_standalone.tex` does not work as a single file

Copied to an empty directory, `tectonic -X compile fishbot_v06_standalone.tex` **fails with exit 1,
"! Undefined control sequence" at line 836**. Re-tested against the 11:18:18 regeneration — still
fails. Cause: `paper/inline.py` expands `\input{…}` but not `\InputIfFileExists{…}`, so line 829
`\InputIfFileExists{numbers_v06_generated}{}{}` is copied verbatim. The flattened file therefore
carries only the placeholder `numbers_v06.tex` — **302 macros still at `XX`/`XX.XX`**, including
`\vsixCurseGap` = `XX` (used five times), plus `\vsixHeadPooled`, `\vsixPoolRegretFive`,
`\vsixPoolWithholderGain` and others **not defined at all**. It only appears to work because it is
normally compiled from `paper/`, where the generated file happens to sit beside it. The README
advertises the v0.4 and v0.5 equivalents as "single-file Overleaf copy"; this one is not one.

**Fix.** Teach `paper/inline.py` to expand `\InputIfFileExists{f}{}{}` exactly as it expands
`\input{f}` (preferring `numbers_v06_generated.tex`, which must be inlined *after*
`numbers_v06.tex` so the `\renewcommand`s win), then re-run `npm run paper:v06`. Verify by
compiling the result in an empty directory and grepping its log for `Undefined`.

### B-14. The introduction prints the wrong number for the optimizer's curse

`01-introduction.tex:36-39` — "…is `\vsixSearchSpread` **points worse than its own blueprint** when
the action is chosen by an unguarded argmax" → prints **40.28**. The control-to-argmax gap is
`\vsixCurseGap` = **35.69** (49.31 → 13.61), which is what the abstract, `08-search.tex:63-71` and
`14-conclusion.tex:12` print for the same quantity. `\vsixSearchSpread` = 40.28 is the *range across
the whole ladder* (53.89 − 13.61) and is used correctly at `08-search.tex:96-97`.

**Fix.** `01-introduction.tex:38` → `\vsixCurseGap`.

---

## SHOULD-FIX

**SF-1. 35.69 vs 35.70.** The paper prints `\vsixCurseGap` = **35.69** (from the unrounded
49.3055 − 13.6111); `README.md:35` and `:63`, `docs/FISHBOT_V06.md:124` and
`RESULTS-SUMMARY.md:98` print **35.70** (rounded-then-subtracted). Adopt the generator's 35.69 in
the three prose files.

**SF-2. `08-search.tex` quotes the single best bank for the headline, and denies the replication.**
`:102-103` — "at κ = 2.5 it takes `\vsixSearchGuarded`% (**53.75**) against v0.5 with an interval
that excludes parity" — is E12's bank 90210 alone; the claim of record is the three-bank pool
52.64%. `:199-201` then says "it was measured on **one bank** at that setting", which
`F0-search-confirm.jsonl` falsifies (two further banks, both 52.08%). **Fix:** use
`\vsixSearchPooled` at `:103`; delete the "one bank" clause at `:200`.

**SF-3. The same configuration reports two different values on the same bank.**
`F8-tiesearch.txt` §A ("search ONLY inside the tie group, 12 determinizations") = **54.1667%**,
n = 720. `F9-tieonly.txt` bank 90210 ("tieonly search, det=12, on the v0.5 vector, vs v0.5") =
**51.9444%**, n = 720. Neither artifact records its full spec, so the 2.2-point gap cannot be
explained from the tree. Both feed the paper: `\vsixSearchTieOnlyTwelve` = 54.17
(`06-diagnosis.tex`'s tie table and `08-search.tex:126`) and `\vsixTieSearchBankList` =
"51.94, 50.69, 50.00" (used two paragraphs later, and in RESULTS-SUMMARY and docs §4.3).
**Fix:** print the exact spec in both artifacts and reconcile, or drop the 54.17 row and use F9
throughout.

**SF-4. `08-search.tex:120-127` asserts the attribution that `:141-146` retracts.** "That
decomposition also **locates the win** … takes it to `\vsixSearchTieOnlyTwelve`% — better than the
unrestricted search" is precisely the reading `:145` calls "an earlier draft of this section read a
single favourable bank as an attribution. It is not one." **Fix:** delete the "locates the win"
paragraph and keep the retraction.

**SF-5. `06-diagnosis.tex:107-111` pools the wrong configuration.** The tie-resolution table's
twelve-deal row is the tie-group-only search (54.17%); the sentence beneath then says "pooled over
every bank the configuration was measured on, `\vsixSearchPooled`% (52.64)" — but 52.64% is the
*unrestricted* search. The tie-only configuration's three-bank pool is `\vsixTieSearchMean` =
**50.88%**. **Fix:** pool with `\vsixTieSearchMean`, and move 52.64% into the following paragraph
where it is already correctly labelled.

**SF-6. "two orders of magnitude" vs "three orders of magnitude".** `08-search.tex:200` and
`D-ablations.tex:53` say two; `abstract.tex:49`, `README.md:17` and `README.md:63` say three.
303.4 games/s all-threads ÷ 0.144 games/s single-thread = 2,107× (three), but that ratio mixes
thread counts; `08-search.tex:147-148` is the only place that labels both sides. **Fix:** state one
ratio with both sides labelled and use it in all four places.

**SF-7. `14-conclusion.tex:23` says "thirty generations of the old one".** v0.5's refit under the
*old* optimiser ran **forty** generations (`\numFitTraceGensVfive` = 40; `abstract.tex:40`
"forty-generation refit"; `README.md:59`; `RESULTS-SUMMARY.md:202`). Thirty is fitA's generation
count under the *new* optimiser. **Fix:** "forty generations".

**SF-8. RESULTS-SUMMARY's chain/threat head-to-head numbers do not trace.** `:247-248` — "Restoring
it at the v0.6 vector (`v06:chain2=1`) … **45.89% [42.66, 49.16] at bank 90210 and 51.78%
[48.51, 55.03] at bank 31337** head to head" — no `chain2=1` match artifact exists in
`research/v06/results/` or `runs/`; grep finds neither figure. The paired figure on the same line
(−0.75 [−2.44, +0.92]) does trace, to `F1-chain2x2.json`. Note also that 45.89% is used for a
second, unrelated thing at `docs/FISHBOT_V06.md:64` (the handicapped-base sanity fit `v05:w0=0`),
which the V1 note flagged as a likely transcription error and which is still unresolved.
Similarly `RESULTS-SUMMARY.md:237-238`'s "`v05:ptheta=0.30` scores 48.75% against v0.5 over 1,200
games" traces to nothing in `results/` or `runs/`. **Fix:** produce the artifacts or mark both as
transcribed from a named note.

**SF-9. `\numVfourLbrWin` / `\numVfourLbrCI` exist, are still `XX.XX`, and are unused while the
value is hard-coded.** `11-results.tex:94` and `12-corrections.tex:32` both hard-code
`$51.19\%$ $[49.67, 52.72]$`. **Fix:** set both macros in `numbers_v06.tex` and use them.

**SF-10. Partner deltas hard-coded.** `11-results.tex:179-180` and `13-limitations.tex:45-46` both
hard-code `$2.25$`, `$1.4$`, `$-0.1$`, `$-0.8$`, which `tables_v06/partners.tex` already carries.
**Fix:** emit `\vsixPartner*Delta` from `E11-partners.jsonl` and use them in both places.

**SF-11. `13-limitations.tex:12` hard-codes "$3.06$ / $3.11$ / $4.06$".** The withholder-deleted
minimax regret appears in no artifact and no macro anywhere in the tree; it cannot be checked.
**Fix:** compute it in `build_tables_v06.py` from E4+F4 and emit
`\vsixPoolRegretNoWithholder{Six,Five,Four}`.

**SF-12. Other literal experimental numbers where a macro belongs.**
`08-search.tex:165-166` and `H-human-play.tex:35-36` — `$79.2\%$` / `$50.1\%$` (duplicated
literals); `07-mechanisms.tex:33` — `$14\%$`; `:62` — `$41.65\%$`;
`08-search.tex:51-52` and `E-search-detail.tex:56` — `$62\times$` and `$38$ points`;
`08-search.tex:108,116` — `$31\%$`;
`06-diagnosis.tex:108-109` — `$50.00\%$` and `$4.500$ to $4.500$`, duplicating
`\vsixSearchTieRandom` in the table row directly above;
`12-corrections.tex:38` — `$+0.12$ points`;
`D-ablations.tex:30-34` — the whole five-row null table restated as literals although
`\numResVoidSmall`, `\numResVoidSmallCI`, `\numResPerseverLargeA` … exist for exactly those cells.
Separately, "$62\times$ cheaper per event" is not derivable from `E13-rollout.jsonl`, whose only
throughput ratio is 996.2 / 30.7 = 32×.

**SF-13. `engine/src/v06.hpp` comments contradict the measured results.** Three sites:
* `:56` — "Measured at **-27 points** before the guard was added (research/v06/notes/R11)". The
  measured gap is 35.69 (49.31 → 13.61, `E12-search.jsonl`).
* `:103-108` — "The measured fact this exists for: the determinized search's **ENTIRE advantage
  comes from re-choosing inside the tie group, not from deviating outside it** (F8-tiesearch.txt)".
  The study's own conclusion is that this attribution is *not resolved*: guarding the tie group gives
  49.86% (`F7-winrate.txt`), restricting to it gives 51.94 / 50.69 / 50.00 (`F9-tieonly.txt`), and
  the unrestricted search gives 52.64%. This is the same class of defect as the one already fixed
  elsewhere in this header.
* `:126-128` — the stale deliberate-miss figures (B-7).
**Fix:** rewrite all three to the measured values and cite `E12`, `F7`/`F9` and `E15`.

**SF-14. F6–F9 are in the manifest and feed the paper, but no committed script produces them.**
`F6-reconstruction.txt`, `F7-tieguard.txt`, `F7-winrate.txt`, `F8-tiesearch.txt` and
`F9-tieonly.txt` supply ten paper macros — `\vsixRecon*`, `\vsixSearchTieRandom*`,
`\vsixSearchTieOnlyOne*`, `\vsixSearchTieOnlyTwelve*`, `\vsixSearchTieGuarded*`,
`\vsixSearchDevGuarded*`, `\vsixTieSearch*` — **including both negative controls that make the
search result a claim about information**. `experiments_v06.sh` stops at E15; `followups_v06.sh`
stops at F5. **Fix:** add an F6–F9 block to `followups_v06.sh` recording each exact spec, or list
the commands in `G-reproducibility.tex` and register the gap in `app:repro-gaps`.

**SF-15. `G-reproducibility.tex` never mentions `followups_v06.sh` and gives an exploitability
command that cannot reproduce its own table.** `:169` gives `BASE=v05 TARGETS="v04 v05"
./exploitability_v06.sh`, which omits the v0.6 row of `tab:lbr`; `docs/FISHBOT_V06.md:188` records
the right invocation (`TARGETS="v04 v05 v06" GENS=12 GAMES=180 POP=18 EVAL=600`). No command is
listed for F0–F5 at all. **Fix:** add `./followups_v06.sh` to the command block and correct the
exploitability line.

**SF-16. `F5-exploitability.log` is a superseded lower-budget run and is not labelled as one.** It
records the v0.4 probe at **48.71%** and v0.5 at **48.96%** (GENS=10, GAMES=150, POP=14, EVAL=400,
n = 2,400) — i.e. at that budget the positive control does *not* reproduce the published 51.19%. The
shipped table comes from the later GENS=12/EVAL=600 run in `X1-lbr.jsonl` (50.69 / 50.31 / 48.36).
`exploitability_v06.sh:30` truncates `X1-lbr.jsonl` on every invocation, so re-running F5 as written
would destroy the table of record. **Fix:** mark F5 superseded in the manifest and summary, and make
`exploitability_v06.sh` write to a run-specific filename.

**SF-17. "Pooling *every* v0.6-versus-v0.5 cell in the battery" overstates.** The pool is E3's five
banks + E7's default-dialect cell + E5's v0.5 panel cell. It excludes four further v0.6-vs-v0.5
head-to-head cells at default rules on disjoint banks: E4 51.22% (515253), F4 50.87% (90210), F4
50.87% (424242) and E11's self-partner cell 52.25% (313131). All four are **above** parity, so the
null conclusion survives — but the word "every" is false. `README.md:28`, `RESULTS-SUMMARY.md:38`,
`11-results.tex:63`, `01-introduction.tex:54`. **Fix:** "pooling every dedicated head-to-head cell —
E3's five banks, E7's default-rules row and E5's v0.5 panel cell —" plus a sentence naming the four
excluded cells and their direction.

**SF-18. Per-style deal counts are wrong in RESULTS-SUMMARY.** `:48` says "300 deals x 6 rotations
per cell at seeds 515253, 90210 and 424242". `F4-perstyle-banks.jsonl` records
`"deals":250,"games":1500` for both 90210 and 424242; only 515253 is 300 × 6. (1,800 + 1,500 + 1,500
= 4,800, so the totals are right.) **Fix:** "300 × 6 at 515253 and 250 × 6 at 90210 and 424242".

**SF-19. The pooled per-style table's `games` column does not apply to the v0.4 column.** `F4` has
no v0.4 arm, so every "4,800" row is 4,800 games for v0.6 and v0.5 but **1,800** for v0.4 — the
minimax-regret comparison, which is the headline, therefore compares two n = 4,800 arms against an
n = 1,800 arm. `perstyle_pooled.tex` and `RESULTS-SUMMARY.md:51-68` both carry one games column for
three arms. **Fix:** split the column, or footnote it in the table caption and the summary.

**SF-20. The `xf=0` ablation is reported three ways.** `README.md:64` "−0.13 points [−1.92, +1.67]"
(E5's interval); `docs/FISHBOT_V06.md:97` and `:109` "−0.13 [−1.96, +1.69]" (F1's interval with E5's
rounding); `tables_v06/fourway.tex` "−0.12 [−1.96, +1.69]" (F1). The artifact `F1-chain2x2.json`
gives −0.00125 [−0.01958, +0.01688]. **Fix:** use F1 everywhere with one rounding rule.

**SF-21. The in-panel/out-of-panel split is single-bank but sits under a three-bank table.**
`RESULTS-SUMMARY.md:163-182`'s +3.12 / −0.20 and "+1.15 excluding the withholder" reproduce exactly
from `E4-perstyle.jsonl` (bank 515253) alone; recomputed on the pooled three-bank table the
withholder-excluded in-panel mean is **+1.10**. Only the third bullet discloses the bank, in
passing. **Fix:** say "computed on E4 (seed 515253)" in the section heading.

**SF-22. `threatWeight` is presented as a fitted coordinate that moved and as inert in the same
file.** `RESULTS-SUMMARY.md:235` lists `threatWeight` 2.7047 → 3.7658 under "the coordinates that
moved most"; `:257-259` states that `searchTopK`, `chainWeight` and `threatWeight` are **inert
coordinates** in the shipped configuration, "flagged as inert … rather than presented as fitted
quantities that do something". **Fix:** drop the row, or mark it "(inert — v0.6 does not run the
chain/threat pass)".

**SF-23. Artifact counts disagree.** `\vsixManifestFiles` = **34** ("The manifest holds 34
artifacts", `G-reproducibility.tex:297`); `MANIFEST.json` lists **35** (33 result files +
`runs/fitA.jsonl` + `runs/fitC.jsonl`). `tables_v06/manifest.tex` has 34 rows: the 33 result files
plus `MANIFEST.json` itself, omitting the two fit runs MANIFEST does list. **Fix:** emit both counts
from one list in `build_tables_v06.py`, and add the fit runs to the printed table — they are the
provenance of the shipped vector.

**SF-24. `05-belief.tex:262-265` describes a row it does not use.** It calls the
`\vsixBeliefNoPrior` row "the approximate path with the policy prior deleted — so that it differs
from the exact object only in that its counts are computed approximately". That row is θ = 0 but
**φ = 0.122**. `F2-belief-noprior.txt`, which exists precisely to answer this, measures the genuine
θ = φ = 0 row at **1.39339 nats / 49.14%** against the quoted 1.39083 / 49.99. The conclusion is
unaffected — the approximation still beats the exact object at θ = φ = 0 — but the sentence
describes an experiment other than the one it cites. Same wording in `README.md:62` ("the rest
survives with the prior deleted"). **Fix:** use the F2 θ = φ = 0 row, or say "with the ask prior
deleted and the certificate prior retained".

---

## NOTE

**N-1.** `tables_v06/h2h_v05.tex` has no seed-bank column although `11-results.tex:74` captions it
"one row per seed bank". A reader cannot map 51.44 / 50.44 / 51.33 / 50.11 / 51.83 to
90210 / 31337 / 515151 / 777001 / 424242.

**N-2.** `engine/perstyle_extra.sh` writes `E4b-perstyle-banks.jsonl`, which does not exist; the
three-bank profile actually came from `followups_v06.sh` block F4 (250 deals, v0.6 and v0.5 only, no
v0.4 arm). Dead script — delete it or fold it into `followups_v06.sh`.

**N-3.** `research/v06/runs/searchtest.json`, `fitB.log` and `fitD.log` are **zero bytes**, and no
note records why `fitC` was selected over `fitB`/`fitD`. If the selection used bank 515253 or 90210
those banks are model-selection data rather than held-out data. The V1 audit raised this; it is
still unanswered.

**N-4.** `engine/src/probe_v06.hpp:310-313` states the literature falsifier ("a good search differs
from its blueprint on 1-3% of decisions; one that differs on 40% is mis-scaled, not smart") without
the tie-group-churn decomposition the paper now uses to answer it. Not wrong, but a reader of the
source alone draws the refuted conclusion.

**N-5.** `./fish` with no arguments prints only
`usage: fish <match|verify|matrix|bench|serve>`. Every command the v0.6 documents instruct a reader
to run — `pathology`, `v6probe`, `ablate`, `gateaudit`, `blockalias`, `tune`, `selftest`, `oracle` —
is dispatched in `main.cpp` but absent from that line.

**N-6.** `G-reproducibility.tex:44-46` says the battery "writes one artifact per block"; block E8
writes two (`E8-ties.txt` and `E8-belief.txt`).

---

## Closed during this pass

* **Label `tab:belief` was multiply defined** (`05-belief.tex:254` and `11-results.tex:146`), so
  three `\ref{tab:belief}` resolved to the wrong table. Fixed at 11:17 —
  `11-results.tex:146` is now `tab:belief-results`, and the compile log is clean.
* **`README.md` and `RESULTS-SUMMARY.md` quoted 52.92% / n = 720 for the search against v0.6.**
  Fixed at 11:00 in the README bullet (`:31-34`) and `RESULTS-SUMMARY.md:96`, both now 52.08% over
  2,880 games with all four cells above parity. `README.md:63` and `docs/FISHBOT_V06.md:17-18` were
  not updated — see B-9.
* **`fishbot_v06_standalone.tex` was six sections out of date**, missing the self-play limitation
  paragraph, the partner-regime paragraph, the "What is established" v0.6-vs-v0.6 paragraph, the
  whole four-way ablation subsection and table, and the `MANIFEST.json` artifact row. Regenerated at
  11:18:18; a marker-aware diff of all 25 section files against the flattened copy is now clean. Its
  single-file compile is still broken — see B-13.
