# FishLab

FishLab is a Canadian Fish simulation and research workbench. The current agent
is **FishBot v0.4**, which combines an exact observer-conditioned posterior over
the initial deal — used as a reference and validation oracle — with a faster
approximate inference path that the deployed policy runs. It lives in the C++
engine under `engine/`. The browser lab (`app/`, `lib/fish-engine.ts`) hosts the
earlier v0.3 population and the interactive replay.

## FishBot v0.4 (current)

Because every card movement in Fish is public, the entire hidden state is the
initial deal. That makes the posterior exactly computable, including the
certificate that an ask carries about the asker's own hand, and it yields a
theorem: a half-suit held entirely by one team can never be asked in by the
other, so its ownership can never change, so waiting to claim it carries no
ownership risk.

Two configurations are distinguished throughout:

- **v0.4-Fast** — the default, deployed and primarily evaluated policy
  (`BeliefMode::Fast`). Every reported performance number is this one.
- **v0.4-Block** — the same fitted policy with the exact reference belief
  substituted (`v04:belief=block`). It validates the probabilities and serves as
  an ablation; it does not run in the inner loop.

See `docs/FISHBOT_V04.md` for the specification and `paper/fishbot_v04.tex` for
the full study.

```bash
cd engine && make
./fish verify    --games=600                    # rules + belief soundness audit
./fish selftest  --games=40                     # reference engine vs card DP vs sampling
./fish oracle    --games=150                    # brute-force allocation oracle
./fish gateaudit --games=700 --rotations=6      # declaration pre-gate false-negative audit
./fish match --a=v04 --b=v03 --games=700 --rotations=6 --seed=90210
./experiments.sh                                # the full battery
python3 build_manifest.py                       # artifact checksums + MANIFEST.json
```

## Play it yourself

`fish serve` opens a browser table where any mix of humans and bots takes the six
seats. It is the same `Game` driver every published number came from — a human
seat is just another `Agent` — so what you are playing is the deployed policy,
not a reimplementation of it.

```bash
cd engine && make
./fish serve                                    # then open http://127.0.0.1:8173
```

Pick a policy per seat, tick the seats you want to play yourself, and deal.
Presets cover you plus two v0.4 teammates against three v0.4s, and you plus two
v0.3 teammates against three v0.4s. You are sent your own hand and the public
event stream and nothing else. See `docs/PLAY.md`.

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

FishBot v0.3 is specified in `docs/FISHBOT_V03.md`; its held-out results are in `docs/V03_FINDINGS.md`. The v0.3 paper is `paper/fishbot_v03.tex` with a verified PDF at `output/pdf/fishbot_v03.pdf`. FishBot v0.4 is specified in `docs/FISHBOT_V04.md`, its study design in `docs/V04_FINDINGS.md`, its generated result tables in `docs/V04_RESULTS.md`, and its paper in `paper/fishbot_v04.tex` (single-file Overleaf copy: `paper/fishbot_v04_standalone.tex`, built PDF: `output/pdf/fishbot_v04.pdf`).

## Reproduce v0.3

```bash
npm run verify:engine
npm run optimize:fishbot
npm run evaluate:fishbot
npm run refine:fishbot
npm run ablations:fishbot
npm run paper:markdown
```

## Design principles

- Hidden information stays hidden from acting agents.
- Every deal and decision is reproducible from a seed.
- Strategy is expressed through inspectable numeric policies, not opaque prose calls in the hot loop.
- LLMs are best used outside the loop for policy ideation and replay interpretation; deterministic simulation supplies the evidence.
- Final claims use held-out, orientation-balanced seeds; matrix row averages are descriptive, not substitutes for direct tests.
