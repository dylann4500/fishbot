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
| Declaration | fixed confidence threshold on a geometric mean of per-card marginals | value-based stopping rule against a fitted value function, on the joint probability of the named allocation |
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

- `mu[c][p]` — exact per-card ownership marginals;
- `P(allocation A) = S_{t(A)} / Z` for a **surviving** allocation — the
  declaration query, which is a joint probability, not a product of marginals;
- `P(team owns S)` — exact;
- deal sampling that is rejection-free for C1–C4; with a live C5 certificate the
  same sampler is used with an exact accept/reject test, so the combined
  procedure is exact but not rejection-free.

A consequence: under the uniform prior every **surviving** allocation sharing a
count vector has identical probability, so the MAP allocation is fixed by the
count vector alone. An allocation whose count vector survives but which itself
violates a certificate has probability zero, so survival has to be checked.

All four quantities are validated against exhaustive enumeration by
`./fish oracle` (see `research/v04/results/E15-oracle.txt`), on small reachable
states; larger states are skipped and the number skipped is reported.

### The deployed path

`BeliefMode::Fast` is the **default**, and it is what every reported performance
number was produced with. It keeps the exact constraint bookkeeping of C1–C3 and
the exact capacities of C4, fits them by Sinkhorn scaling, and interleaves an
independence-conditioning step for C5. Measured marginal error against the
reference engine is 0.017 mean, 0.498 max. The exact reference engine
(`belief=block`) validates the probabilities and serves as an ablation; it runs
about 14× slower in whole-game throughput and is not in the inner loop.

## The locked half-suit theorem

If every card of a half-suit is held by one team, no opponent can legally ask in
it — asking requires holding another card of that half-suit, and they hold none.
The half-suit is frozen until claimed. Therefore:

- the "steal" risk of waiting is exactly zero for a locked half-suit;
- the *support* of surviving allocations is non-increasing, though the
  probability of the true allocation is not monotone;
- the only costs of holding are positional — the six cards license no productive
  ask, and a hand of locked cards makes your turn worth little;
- and the only cost of claiming is information: the announced split resolves six
  cards that were absorbing probability mass in the opponents' counting.

Note what the theorem does **not** say. Asks inside a locked half-suit remain
legal for the owning team and still carry C5 certificates, so allocation
information about it can continue to arrive even though ownership cannot change.
Ownership monotonicity alone therefore does not establish that such a position is
informationally frozen.

Declaration is decided by a **value-based stopping rule** against a fitted value
function, with cashing valves that force termination. The frozen-policy ablation
does not resolve a benefit for that rule over a fixed threshold (+0.12 points,
95% paired CI −1.23 to +1.47), so the formulation is not claimed to be optimal.

## Ask rule

`U(a) = λ_lin · Σ w_k φ_k(a) + λ_val · [ p·V(s⁺) + (1−p)·V(s⁻) ]`

over 20 normalised features (hit probability and its square, certainty, own
progress, team control, lock completion, continuation, completion, reply threat,
information leak, target hand size, emptying the target, repeated half-suit,
known team cards, entropy, team-owns, exposure, trailing pressure, runway, leak
magnitude). The leading candidates are optionally re-scored with a two-ply
lookahead that recomputes the belief on each branch — with the **same Sinkhorn
approximation** as the main path, so neither branch is exact.

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
./fish verify    --games=600                 # rules + information safety + belief soundness
./fish selftest  --games=40                  # reference engine vs card DP vs exact sampling
./fish oracle    --games=150                 # brute-force allocation oracle
./fish gateaudit --games=700 --rotations=6   # declaration pre-gate false-negative audit
./experiments.sh                             # the full battery, E1–E17
python3 build_manifest.py                    # artifact checksums + MANIFEST.json
```

### Known gaps

- The declaration pre-gates are pruning heuristics, not proved upper bounds.
  `./fish gateaudit` measures the loss: 1,017 false negatives over 24.1M gate
  rejections, changing the chosen action at 0.0101% of declaration opportunities.
- The 16 value-function coefficients compiled into `V04Config::vw` are **not**
  those in `research/v04/results/E14-valuefit.txt`; `freeze_config.py` writes only
  the 34 policy parameters.
- Round 5's fitting base seed was not captured in a committed script.
