# FishBot v0.5 — paper deliverable plan

The v0.5 study ships a paper matched to the v0.4 one in structure, rigour and build pipeline.

## What the v0.4 paper is, and therefore the bar

- `paper/fishbot_v04.tex`, 11pt article, 13 body sections + 9 appendices in `paper/sections/`.
- Every number in the prose comes from a `\num...` macro in `paper/numbers.tex` (182 of them),
  which is **generated** from the raw artifacts by `engine/build_tables.py`. No number is typed
  by hand. Generated tables live in `paper/tables/*.tex`.
- `paper/check.py` is a consistency sweep with three classes of check, each of which has caught
  a real defect: BANNED phrases (claims the v0.4 review ruled out — e.g. calling the declaration
  rule "optimal", calling the two-ply refinement "exact", asserting an upper bound on
  exploitability), MACROS (every referenced macro is defined; no doubled percent signs), and
  STRUCTURE (every ref/cite/input resolves; every float has a caption and a label).
- `paper/inline.py` flattens the sectioned source into `paper/fishbot_v04_standalone.tex` for
  Overleaf. Build: `npm run paper:v04` → `tectonic -X compile ... --outdir ../output/pdf`.
  `tectonic` is installed at /opt/homebrew/bin/tectonic; there is no pdflatex/latexmk.
- Byline: `Dylan Nguyen\\\small FishLab Research Project`, with the repository URL and the exact
  commit the experiments ran at, stated per experiment group when they differ.
- Artifact digests in `research/v04/results/MANIFEST.json` via `engine/build_manifest.py`.

## v0.5 deliverable

- `paper/fishbot_v05.tex` + `paper/sections_v05/` + `paper/numbers_v05.tex` (generated).
- Extend, do not fork, the tooling: `engine/build_tables.py` gains a v0.5 mode;
  `paper/check.py` gains the v0.5 banned list; `paper/inline.py` handles the new main file.
- `npm run paper:v05` builds it to `output/pdf/fishbot_v05.pdf`.
- The v0.4 paper stays buildable and unchanged except where v0.5 **corrects** it — corrections
  are stated as corrections, in their own subsection, not silently patched.

## The narrative arc the evidence is pointing at

Provisional; the workflow findings decide it. Current state of the evidence:

1. **v0.4's headline evaluation hid a failure mode.** Against v0.3 it is fine (2.8% dead asks).
   In mirror play 39% of asks are provably dead, 40% are exact repeats, and 34% of games contain
   a run of ≥6 consecutive provably-dead asks. Strong opponents — and strong humans — trigger it.
2. **The termination theory in the v0.4 study is wrong in a specific, checkable way.**
   `research/v04/results/E11-termination.md` claims a locked half-suit is informationally frozen
   as well as ownership-frozen. It is not: the owning team may still legally ask inside it and
   those asks emit ask-legality certificates. Task P1 is measuring exactly how many bits.
3. **The forcing rule is a net loss.** A v0.4 that never force-declares beats shipped v0.4 by
   +6.2 points (56.2%, cluster CI 54.9–57.5, n=1500), declaring at 98.4% against 85.7%. But pure
   patience deadlocks 22.5% of mirror games. Both settings are bad; the missing capability is an
   ask rule that values information, not just material.
4. **The opponent model is a single fixed pair of scalars** shared across all five other seats
   and never updated in-game, so deliberate silence is misread by construction.
5. **Two rules-legal coordination channels are unused**: willingness bits on turn transfer, and
   an information-safe arbitration ladder for simultaneous declarations.

## Standing constraints on the writing

- Never headline an aggregate win rate alone. Report the per-opponent breakdown, an explicit
  worst-case across playstyles, and an exploitability probe.
- "Exact Bayesian inference" is an assumption to test, not a claim: the uniform-over-consistent-
  deals posterior is policy-agnostic, and v0.5 will make the policy-aware part explicit.
- Where a claim is unsupported and the evidence is cheap to produce in the engine, produce the
  evidence rather than hedging the sentence.
- Anything that cannot be settled becomes a named limitation with the experiment that would
  resolve it.
