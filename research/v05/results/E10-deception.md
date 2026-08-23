# E10 — Deception panel: v0.5 against adaptive opponents

Two independent seed banks, 400 deals x 6 rotations per cell (n = 2,400 games).
Archetypes are in `engine/src/probe_deception.hpp`, registered in `engine/src/factory.hpp`:

- **silent** — never asks in the half-suit it holds most cards of, until forced.
- **feint** — preferentially asks in a half-suit it holds exactly one card of, manufacturing a
  misleading ask-legality certificate, when the material cost is small.
- **withholder** — the project owner's own manoeuvre: after being asked in half-suit *S* while
  holding other cards of *S*, avoids asking in *S* for the next *K* turns.

| opponent | v0.5 (seed 31415926) | v0.4 (seed 31415926) | v0.5 (seed 8675309) | v0.4 (seed 8675309) | mean delta |
|---|---:|---:|---:|---:|---:|
| silent | 80.42% | 79.96% | 83.17% | 79.00% | **+2.3** |
| feint | 50.96% | 54.13% | 52.08% | 53.29% | **−2.2** |
| withholder | 73.63% | 66.25% | 71.42% | 64.46% | **+7.2** |

## Reading

**v0.5 is markedly more robust to the manoeuvre that motivated this work.** The withholder is a
direct model of what the project owner did at the table — hold cards of a half-suit you were
asked for, then decline to ask back in it — and v0.5 gains **+7.2 points** on it, replicated at
both seeds. It also gains on the silent archetype. The mechanism is not a new opponent model:
M3–M7 are unbuilt. It is that v0.5 no longer spends turns on asks it can prove will miss, so a
misleading *absence* of asks has far less leverage over the position.

**v0.5 is worse against the feint, by 2.2 points, and this replicates.** The feint manufactures
a *false positive* certificate rather than withholding a true one, and the fit raised
`priorTheta` from 0.264 to 0.445 — i.e. v0.5 weights "this player asked here" more heavily than
v0.4 did. The v0.5 diagnosis predicted precisely this exposure: over-weighting the policy prior,
not weighting it at all, is the vulnerability (P3, and the rejected finding that deleting the
prior is *worse* by 4.60 points [2.63, 6.58]).

**The exposure is not fixable by retuning `priorTheta` alone.** A sweep over
{0.20, 0.26, 0.35, 0.445, 0.60} at two seeds does not identify the parameter: the ordering is
not stable across seeds and every pairwise difference sits inside the cluster-bootstrap interval.
The fitted value is retained rather than tuned on noise. The principled fix is M7 — a per-seat
online type posterior with data-biased shrinkage, so a manufactured certificate from a seat whose
type posterior has drifted toward "deceptive" is discounted — and M7 is specified but not built.

## Consequence for the worst-case table

Across the full twelve-style set (nine standard + three deceptive), v0.5's worst case is the
**feint at 50.96%**, marginally below its mirror worst case of 51.11%. v0.4's worst case remains
its own mirror at 50.00%. Neither policy is worse than a coin flip against anything in the set,
and no aggregate figure should be quoted without this table beside it.
