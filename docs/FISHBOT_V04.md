# FishBot v0.4: technical specification

FishBot v0.4 is a deterministic, fully inspectable policy for six-player
Canadian Fish. It sees only its own hand and the public record — asks, answers,
transfers, declarations and card counts — and it has no private channel to its
teammates. It is implemented in `engine/src/v04.hpp`; the belief engines are in
`engine/src/belief.hpp` and `engine/src/blockdp.hpp`.

## What changed from v0.3

| | v0.3 | v0.4 |
|---|---|---|
| Rules | declarations only on your own turn; adjudication after an action cap | declarations at any moment; chosen successor on a turn gift; willingness-only endgame negotiation |
| Belief | Sinkhorn scaling over soft ask-count weights | exact posterior over the initial deal, including ask-legality certificates |
| Declaration | fixed confidence threshold on a geometric mean of per-card marginals | optimal stopping against a fitted value function, on the exact joint probability of the named allocation |
| Ask rule | 8 hand-set terms | 20 normalised features plus a one-ply expectimax, all fitted |
| Evaluation | two-orientation swap, Wilson intervals | six-rotation duplicate blocks, cluster bootstrap over deals |

## The structural fact

Cards move only through publicly observed transfers. A card whose location has
never been revealed is therefore still with whoever was dealt it, and the entire
hidden state of the game is the initial deal. Every deduction below follows.

## The constraint system

For an observer, with `U` the set of cards whose holder is not publicly known:

- **C1** own hand — exact, and excludes the observer from every card in `U`;
- **C2** public transfers and correct declarations — exact owner;
- **C3** exclusions — an ask proves the asker lacks the named card, a miss proves
  the target lacks it, and both are permanent;
- **C4** capacities — `q_p = handCount[p] − knownHeld[p]` unresolved cards belong
  to player `p`, and a declaration is absorbed by this automatically;
- **C5** ask legality — an ask in half-suit `S` proves the asker held another card
  of `S`, i.e. `OR_{d in D} [owner(d) = asker]`.

The posterior is uniform over assignments satisfying C1–C5. This is the
*grounded* posterior: exact with respect to the rules, and independent of how
opponents choose their moves.

## Exact inference

C1–C4 form a degree-constrained bipartite counting problem. The forward/backward
dynamic program over capacity vectors exploits the fact that the layer index is
determined by the state (`sum(n) = |U| − j`), so both tables are single arrays of
size `prod_p (q_p + 1) ≤ 10^5` and the whole pass costs about `6 × 10^5`
multiply-adds.

C5 is disjunctive and breaks the product form, but every certificate is confined
to one half-suit. Half-suits with a live certificate are enumerated exhaustively
(`≤ 5^6` assignments), filtered exactly, and aggregated into a count-vector table
`g_S(t)`; the dynamic program then runs over blocks instead of cards. This
yields, with no sampling:

- `mu[c][p]` — exact per-card ownership;
- `P(allocation A) = S_{t(A)} / Z` — the declaration query, which is a joint
  probability, not a product of marginals;
- `P(team owns S)` — exact;
- exact rejection-free deal sampling.

A consequence: under the uniform prior every surviving allocation sharing a
count vector has identical probability, so the MAP allocation is fixed by the
count vector alone.

A fast approximation (Sinkhorn fitting of C4 on the exact support of C1–C3,
interleaved with an independence-conditioning step for C5) is used for
large-scale fitting; its measured marginal error is small on average with a
heavy tail, and the two are compared as an ablation.

## The locked half-suit theorem

If every card of a half-suit is held by one team, no opponent can legally ask in
it — asking requires holding another card of that half-suit, and they hold none.
The half-suit is frozen until claimed. Therefore:

- the "steal" risk of waiting is exactly zero for a locked half-suit;
- the allocation probability can only improve;
- the only costs of holding are positional — the six cards license no productive
  ask, and a hand of locked cards makes your turn worthless;
- and the only cost of claiming is information: the announced split resolves six
  cards that were absorbing probability mass in the opponents' counting.

Declaration is therefore an optimal-stopping problem, decided against a fitted
value function, with cashing valves that guarantee termination.

## Ask rule

`U(a) = λ_lin · Σ w_k φ_k(a) + λ_val · [ p·V(s⁺) + (1−p)·V(s⁻) ]`

over 20 normalised features (hit probability and its square, certainty, own
progress, team control, lock completion, continuation, completion, reply threat,
information leak, target hand size, emptying the target, repeated half-suit,
known team cards, entropy, team-owns, exposure, trailing pressure, runway, leak
magnitude). The leading candidates are optionally re-scored with an exact
posterior recomputation on each branch.

`V` is a linear value function over 16 public-belief-state features, ridge-fitted
on self-play decision points and labelled with the final half-suit differential.
Features are collected from all six seats, not only the mover, or the
side-to-move coefficient is unidentified.

## Fitting

Cross-entropy method over a 34-coordinate vector, with common random numbers
within a generation and fresh banks between generations. The objective is a
**soft minimum** over the opponent panel rather than the mean, because the
question is whether one policy can be best across playstyles, not on average.
The misdirection artist, the random control and every rule variant are held out.

## Reproducing

```bash
cd engine && make
./fish verify --games=600
./fish selftest --games=40
./experiments.sh
```
