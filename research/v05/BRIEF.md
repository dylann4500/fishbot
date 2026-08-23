# FishBot v0.5 — investigation brief

Repository root: `/Users/dylan/Documents/GitHub/fish optimization`
Engine: `engine/` (C++20, `cd engine && make` → `./fish`).  All commands below run from `engine/`.

## What v0.4 is

`engine/src/v04.hpp` — deterministic policy. Exact/approx posterior over the initial
deal (`belief.hpp`, `blockdp.hpp`), 20-feature linear ask score + one-ply expectimax over a
16-feature linear value function, declaration by an optimal-stopping rule.
Specification: `docs/FISHBOT_V04.md`. Study: `docs/V04_FINDINGS.md`, `paper/fishbot_v04.tex`.
Prior literature review (excellent, still current): `research/v04/lit/` — especially
`signalling.md` §2.4, §2.6, §6.4, §7.9, §8 and `00-SYNTHESIS.md`.

Rules driver: `engine/src/game.hpp`. Rules/primitives: `engine/src/fish.hpp`.
Human table: `engine/src/human.hpp`, `table.hpp`, `serve.hpp`, `web/index.html`.

## The user's report (live play against v0.4)

1. Bots get into an **infinite loop**, repeatedly asking the same bot the same question,
   going back and forth "hoping their teammate will declare".
2. At instant pace the game eventually ends with **many teams misdeclaring**.
   The user's judgement: *it is always better to try asking someone else, even if a little
   risky, than to misdeclare at the end because of incomplete information.*
3. **Deception works against it.** The user held two cards of a half-suit they were asked
   for and deliberately did not ask back in that half-suit; this measurably confused the bot.
4. A well-trained human still outperforms v0.4 on some reasoning.
5. "There are more strategies within Fish possible in terms of multi-person interactions and
   nuances that are not yet accounted for. Fish has raw computation and memory, but some
   advanced strategy pipelines are not yet developed."

## Reproduced baseline (new `fish pathology` command, `engine/src/diag.hpp`)

Full output: `research/v05/results/P0-v04-pathology.md`.

**v0.4 mirror (600 games, seed 31):**

| metric | value |
|---|---|
| events/game | 143.6 (median 106, **p90 312, p99 321**) |
| ask hit rate | 34.2% |
| **provably dead asks** (actor could PROVE the target lacks the card) | **39.0% of all asks** |
| **exact repeat asks** (same actor, card, target, same game) | **40.0% of all asks** |
| dead runs | 2610, mean length 12.0, **longest 286** |
| games containing a dead run ≥ 6 | **34.3%** |
| declarations wrong | 10.4% |
| **declarations made at/after event 220 (the forcing horizon) that were wrong** | **58.6%** |
| forced-endgame declarations wrong | **100% (28/28)** |
| asks inside a half-suit the actor's own team already owns outright | 16.5% |

**v0.4 vs v0.3 (600 games, seed 90210):** dead asks only 2.8%, longest dead run 5, no game
past event 220. The pathology is a *strong-opponent / mirror* phenomenon, which is exactly why
the published head-to-head number did not expose it — and a strong human is closer to the
mirror case than v0.3 is.

Switching to the exact belief (`v04:belief=block`) reduces dead asks 39% → 28% but leaves
the deadlock intact (longest run 280). **This is a policy defect, not an inference defect.**

## The central suspected error in the v0.4 theory

`research/v04/results/E11-termination.md` and the v0.4 paper claim:

> "Theorem 1 ... implies that such a half-suit is frozen — and, for the same reason, that no
> further information about its allocation can ever arrive. Two policies that both correctly
> decline to cash an uncertain half-suit therefore deadlock."

`docs/FISHBOT_V04.md` already contradicts this in a note: asks *inside* a locked half-suit
remain legal for the owning team and still carry C5 ask-legality certificates, so allocation
information **can** continue to arrive. v0.4's ask rule never chooses such asks deliberately
because its score is dominated by hit probability, and a guaranteed miss donates the turn.

If this is right, the deadlock is an artefact of an ask policy that values only *material*
(hits) and never *information* — and the fix is an ask rule that prices the certificate an
ask emits to teammates against what it leaks to opponents (the wiretap/Farrell–Gibbons
trade-off already formalised in `research/v04/lit/signalling.md` §2.4/§2.6, and never built).

## Other known v0.4 gaps (from `docs/FISHBOT_V04.md` "Known gaps")

- Declaration pre-gates are heuristics, not proved bounds (1,017 false negatives / 24.1M).
- **The 16 value-function coefficients compiled into `V04Config::vw` are NOT the fitted ones
  in `research/v04/results/E14-valuefit.txt`**; `freeze_config.py` writes only the 34 policy
  parameters.
- Round 5's fitting base seed was never captured in a committed script.
- Opponent modelling is a single fixed pair of scalars, `priorTheta` (weight on "asked in this
  half-suit") and `priorPhi` (weight on "took turns but never asked here"), identical for all
  five other players and never updated during a game. A deliberately silent opponent — exactly
  the user's manoeuvre — is misread by construction.
- Voluntary declaration races are arbitrated by **lowest seat**, not by confidence
  (`Rules::declArbitration`), deliberately, to avoid leaking private confidence.
- `V04Agent::bestGuess` picks a per-card argmax over teammates with **no capacity constraint**,
  so it can name an allocation giving a teammate more cards than they hold.

## Standing user preferences for this project

- Never headline an aggregate win rate alone: report the per-opponent breakdown, an explicit
  worst-case across styles, and an exploitability probe. Robustness across playstyles is the
  research question, not average win rate.
- Treat "exact Bayesian inference" as an assumption to test, not a claim: the uniform prior
  over consistent deals is the *policy-agnostic* posterior; the true posterior weights deals by
  opponent policy.
- When a claim is unsupported and the evidence is cheap to produce in the engine, **produce the
  evidence** rather than hedging the prose.

## Rules of record (supplied by the project owner, 2026-08-22)

The engine's `Rules` defaults match the owner's rules on every point checked:
six players in two alternating teams of three; 54 cards as nine six-card sets; random dealer,
nine cards each, player to the dealer's left leads; ask legality (must hold another card of the
set, must not hold the asked card, target must be a live opponent); a hit retains the turn and a
miss passes it to the target; **declaration legal at any moment**, including on an opponent's
turn, with the turn returning to whoever held it; a declarer needs no cards of the set; a
declarer may not declare for the opposing team; any inaccuracy in the named allocation hands the
set to the opponents; a cardless player drops out but may still declare; when one whole team is
cardless the other must declare every remaining set with no further asking.

Two places where the engine is **more restrictive than the rules allow**, and both are
unexploited coordination channels for v0.5:

1. **Turn transfer.** The rules say that when a cardless player must pass the turn and both
   teammates have cards, "the team may openly strategize on which teammate receives the turn,
   but cannot share any information other than their willingness to receive the turn."
   `Agent::choosePassTarget` (engine/src/game.hpp) instead has the cardless player choose
   unilaterally from its own belief; no willingness bit is ever solicited from the teammates.
   The forced endgame already implements exactly this kind of willingness ladder
   (`Rules::forcedTh`), so the machinery exists and is simply not used here.
2. **Declaration arbitration.** The rules do not say who wins a simultaneous declaration;
   `Rules::declArbitration = 0` resolves it by lowest seat to avoid comparing private
   confidences. A willingness ladder is the information-safe way to recover most of
   "the most confident declares first". See task P6.

Also note the rules permit two public queries the engine models implicitly: the previous two
asks, and any player's remaining card count. Both are already public in `PublicState`.

## Design decisions taken by the project owner (2026-08-22)

**D1 — Conventions ship behind a flag, and both configurations are reported.**
Develin states that pre-agreed conventions are explicitly forbidden in Canadian Fish; pagat
documents Ali Salahuddin's convention in real use; a self-play-trained pair has an implicit
pre-agreement by construction. v0.5 therefore builds the signalling machinery behind
`--conventions=off|on`, ships **off** as the headline configuration (legal under every reading),
and publishes the with/without delta as a result in its own right.

**D2 — Action selection becomes stochastic among near-optimal asks, and the policy is
PARTNER-AWARE.** The owner's answer was explicitly nuanced: a bot's playstyle "may be variable
depending on whether it is playing with each other (which is how training is done) vs when it
plays with a human". Two regimes must be designed and evaluated separately, not collapsed:

- *Bot teammates* — the self-play condition. Partners follow a known blueprint, so
  convention-rich, lower-temperature, higher-signalling play is appropriate: the receiver
  actually decodes what the sender encodes.
- *Human or unknown teammates* — the blueprint assumption fails. Play must be grounded (working
  without a shared codebook) and less predictable, because a human who has played many games
  against a deterministic argmax learns to read it. This is where the owner's own report — that
  a well-trained human still outperforms v0.4 — lives.

In the literature this is exactly the Off-Belief Learning / piKL distinction, and the OBL level
is the natural explicit knob. Concretely for v0.5: a softmax over asks within a margin of the
best, with the temperature *and* the convention flag both keyed on the partner regime, and the
temperature raised further where revealing a card would let a named opponent execute a
guaranteed take (signalling.md §7.9 Construction C).

Report both regimes. Never headline the self-play configuration as though it were the one a
human will meet.
