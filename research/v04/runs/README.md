# Fitting traces

| File | What it is |
|---|---|
| `tune-round1-partial.jsonl`, `tune-round1.jsonl` | round 1: 20 ask weights only, fast posterior, four-opponent panel, base seed 20260821 |
| `tune-round2.jsonl` | round 2: joint vector, six-opponent panel, base seed 770077 (superseded; its parameter layout predates two added features) |
| `tune-round3.jsonl`, `tune-round3-final.jsonl` | round 3: aligned 34-coordinate vector, base seed 313131 |
| `tune-round4.jsonl` | round 4: refinement after the information-leak feature fix, base seed 888111 |
| `tune-round5.jsonl` | round 5: the trace the shipping configuration was selected from. **Its base seed was not captured in a committed script and is not recoverable from the artifacts.** This is a known reproducibility gap and is disclosed in the paper. |
| `selection.log`, `selected.json` | validation-bank selection (seed 1357911, 500 deals, 2 rotations, panel `v03,lockout,detective,v02`) of the shipping configuration |
| `finalize*.log` | pipeline logs for the freeze-and-evaluate runs |

## What is actually shipped

`selected.json` and `selection.log` are authoritative: the shipping configuration is
**generation 8 of the round-5 trace**, validation soft-min 0.6096, per-opponent win rates
0.756 / 0.802 / 0.765 / 0.819. It is baked into `V04Config` in `engine/src/v04.hpp` by
`engine/freeze_config.py`, so the bare policy spec `v04` constructs it.

`freeze_config.py` writes only the **34 policy parameters** (20 ask weights + 14 decision
knobs). It does **not** write the 16 value-function coefficients, so the `vw[]` array
compiled into `engine/src/v04.hpp` is not the vector in
`../results/E14-valuefit.txt`; the two were fitted at different times. This is disclosed
in the paper's reproducibility appendix.

## Engine changes made after the parameter vector was frozen

Neither touches a fitted coefficient, and both were in place before the reported
experiments were run.

1. **Neutral adjudication at the action cap.** Previously the residue of a capped game was
   handed to one team, which was biased. It is now split by who physically holds the
   majority of each unresolved half-suit, ties going to the holder of the lowest card. In
   every reported experiment the cap is never reached, so this affects nothing in the
   tables.
2. **Graduated declaration pressure.** See `../results/E11-termination.md`. Without it, two
   copies of the fitted policy fail to terminate within the cap in 21% of self-play deals;
   with it, every game in every reported experiment terminates through play.

Three further engine additions were made *after* the experiment battery was first run, for
validation only. None is on any decision path, and the head-to-head result reproduces
exactly (0.750714) with them present:

3. `fish oracle` (`engine/src/oracle.hpp`) — the brute-force allocation oracle (E15).
4. `fish gateaudit` — the declaration pre-gate false-negative audit (E16).
5. `Rules::declArbitration` and `--arb=` — alternative simultaneous-declaration arbitration
   orders for the sensitivity analysis (E17). The default, lowest seat index, is the
   behaviour every earlier experiment already had.
