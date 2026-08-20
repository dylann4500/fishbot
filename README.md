# FishLab

FishLab is an interactive Canadian Fish simulation and research workbench. It runs seeded batches of games between information-strategy archetypes, reports comparative metrics, surfaces unusual games, and reconstructs every action with an omniscient research replay.

## Run locally

```bash
npm install
npm run dev
```

Open `http://localhost:3000`.

## Headless research

Run a full pairwise strategy matrix:

```bash
npm run research -- --games=1000
```

The browser interface supports 100–5,000 games per experiment. The engine is in `lib/fish-engine.ts`; FishBot v0.2 is specified in `docs/FISHBOT_V02.md`, and the path toward equilibrium play is documented in `docs/METHODOLOGY.md`. Reproducible findings from the initial 85,000-game study and the 46,000-game v0.2 study are in `docs/BASELINE_FINDINGS.md` and `docs/V02_FINDINGS.md`.

## Design principles

- Hidden information stays hidden from acting agents.
- Every deal and decision is reproducible from a seed.
- Strategy is expressed through inspectable numeric policies, not opaque prose calls in the hot loop.
- LLMs are best used outside the loop for policy ideation and replay interpretation; deterministic simulation supplies the evidence.
