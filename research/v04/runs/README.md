# Fitting traces

| File | What it is |
|---|---|
| `tune-round1-partial.jsonl` | round 1: 20 ask weights only, fast posterior, 4-opponent panel, seed 20260821 |
| `tune-round2.jsonl` | round 2: joint vector, 6-opponent panel, seed 770077 (superseded; its parameter layout predates two added features) |
| `tune-round3-final.jsonl` | round 3: aligned 34-coordinate vector, seed 313131 |
| `tune-round4.jsonl` | round 4: refinement after the information-leak feature fix, seed 888111 |
| `selection.log`, `selected.json` | validation-bank selection (seed 1357911) of the shipping configuration |
| `finalize*.log` | pipeline logs for the freeze-and-evaluate runs |

The shipping configuration is generation 10 of round 4, baked into
`V04Config` in `engine/src/v04.hpp` by `engine/freeze_config.py`, so the bare
policy spec `v04` constructs it.

Two engine changes were made *after* the parameter vector was frozen and before
the reported experiments were run. Neither touches a fitted coefficient:

1. **Neutral adjudication at the action cap.** Previously the residue of a capped
   game was handed to one team, which was biased. It is now split by who
   physically holds the majority of each unresolved half-suit. In the reported
   experiments the cap is never reached, so this affects nothing in the tables.
2. **Graduated declaration pressure.** See
   `../results/E11-termination.md`. Without it, two copies of the fitted policy
   deadlock in 21% of self-play deals; with it, every game in every reported
   experiment terminates through play.
