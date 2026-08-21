# External Canadian Fish strategy review

Accessed 2026-08-20. The review was used to propose adversarial tests and interpretable mechanisms; no web claim was treated as empirical evidence until tested in FishLab.

## Sources inspected

- Canadian Fish Project strategy synthesis: <https://canadian-fish.vercel.app/strategy>
- John McLeod, *Literature*, Pagat: <https://www.pagat.com/quartet/literature.html>
- Mike Develin, *Canadian Fish*: <https://www.bantha.org/~develin/cardgames.html>
- Donna Dorsa, *Literature Game Strategy*: <https://depositgenius.com/literature-strategy-canadian-fish/>
- Neel Somani, open-source Literature learner: <https://github.com/neelsomani/literature>

## Mechanisms routed into experiments

### Lockout / blackballing

Published strategy repeatedly recommends avoiding a dangerous opponent because a miss grants that player the turn. We created a fixed `lockout` challenger that combines posterior-greedy asking with a public-history penalty for missing into a concentrated, active opponent. v0.3 beat it at 57.85% on 2,000 held-out orientation-balanced games.

### Ask-history deductions

The external sources emphasize asking the asker, continuing a teammate's missed card, and remembering exact exclusions. FishBot already represented public ask counts, but v0.3 conditions them jointly with card capacities. Removing ask history caused the largest measured collapse.

### Intentional known misses and signaling

Some traditions permit an intentional miss to signal a half-suit, while opponents learn the same fact. The engine keeps known misses legal. v0.3 almost never selects them because it lacks a counterfactual teammate-communication value. This remains future work and is also rules-sensitive because some groups prohibit prearranged conventions.

### Delayed claims and claim-out passes

Human sources describe holding a fully controlled set to preserve hand size or create a strategic pass after claiming out. Current v0.3 normally declares once confidence clears its threshold; the simulator chooses a live teammate mainly by public hand count. This mechanism requires a longer-horizon value model and is not claimed as optimized.

## Computational prior art

Somani's learner encodes detailed knowledge and trains an MLP with immediate hit rewards plus game rewards. Its published limitations include four-player training, certain-only declarations, known misses only as fallback, and 200-move termination. It is useful evidence that learning over public knowledge is feasible, but its rule and evaluation scope do not support a direct six-player performance comparison.
