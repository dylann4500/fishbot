# The FishLab table — playing FishBot v0.4 yourself

`fish serve` opens a browser table where any mix of humans and bots occupies the
six seats. It exists to measure a human against the engine, so it does not
reimplement anything: a human seat is an ordinary `Agent` handed to the ordinary
`Game` driver in `engine/src/game.hpp`, the same driver every number in the paper
was produced with. Every rule — ask legality, out-of-turn declaration,
misdeclaration, cardless turn passing, the forced endgame, the ask cap — is
enforced by that driver, not by the browser.

```bash
cd engine && make
./fish serve
```

Then open <http://127.0.0.1:8173>. `--port=N` picks a different port (the first
free port at or above it is used) and `--web=DIR` points at the assets if you run
the binary from somewhere other than `engine/`.

## Setting up a table

"New game" opens the seat sheet. Seats alternate: **0, 2, 4 are Team A** and
**1, 3, 5 are Team B**, so a human at seat 0 has teammates at 2 and 4 and
opponents at 1, 3, 5. Each seat gets a policy, and any seat can be ticked
*Human*. Three presets cover the common cases:

| Preset | Seats 0 / 2 / 4 (Team A) | Seats 1 / 3 / 5 (Team B) |
|---|---|---|
| Me + 2× v0.4 vs 3× v0.4 | you, v0.4, v0.4 | v0.4, v0.4, v0.4 |
| Me + 2× v0.3 vs 3× v0.4 | you, v0.3, v0.3 | v0.4, v0.4, v0.4 |
| Watch 6 bots | v0.4 ×3 | v0.4 ×3 |

The policy list is the same one `fish match --a=` accepts: `v04` (the deployed
v0.4-Fast configuration, i.e. every performance number in the paper), `v04-Block`
(the exact reference belief), `v03`, `v02`, and the baseline population.

Leaving the seed blank draws one from the clock; setting it replays the same deal
and the same dealer, which is what you want when comparing lines with a teammate.

More than one seat may be human. Each browser then picks which seat it is looking
at, so two people on two machines can share a table; a seat's hand is only ever
sent to a client that asks for that seat.

## Playing

* **Your turn.** Pick an opponent, then pick a card. The grid only offers legal
  asks: half-suits you already hold a card of, minus the cards in your hand, and
  only opponents who still have cards. If you hold nothing you could legally ask
  for, the panel says so — declaring is then your only legal move, which is what
  the engine expects too.
* **Declaring.** The composer is open at all times, including during an
  opponent's turn, because the rules allow a declaration at any moment. Name the
  teammate holding each of the six cards. Off turn the declaration is *queued*
  and announced at the engine's next declaration poll, which happens before every
  ask — exactly the granularity the bots get, so neither side is favoured. On
  your turn a declaration replaces your ask.
* **Running out of cards.** If you are cardless and hold the turn you choose
  which live teammate takes over. You can still declare while cardless.
* **Forced endgame.** When the other team is out of cards, your team must declare
  every remaining half-suit with no more asking. You are offered each half-suit
  in turn: take it with an allocation, step aside for a teammate, or hand the
  rest to your teammates. If nobody steps forward, somebody must still name an
  allocation, and that prompt cannot be declined.

Bot moves are paced so you can follow them; `Pause` freezes the table before the
next event and `Step` releases one event at a time. Pause is the reliable way to
compose a declaration in the middle of a long bot run — at *instant* pace the
bots can chain many asks before you finish typing.

## What you can and cannot see

You receive your own hand and the public event stream, and nothing else — the
same information the engine hands each bot. Card counts are shown for every seat
(the rules let you ask), and the opening deal is revealed only after the game
ends.

The log has two modes. Unticking **full history** shows what the rules actually
entitle you to: declarations, turn passes, and the previous two asks. Ticked — the
default — it shows everything. The bots have perfect recall of every public
event, so the full log is the setting that puts you on even memory terms with
them; the rule view is the setting that reproduces a real table. Which one is
"fair" depends on what you are trying to measure, so both are one click apart.

## Deliberate simplifications

* The UI prevents illegal asks rather than penalising them. Under the rules a
  procedural mistake hands the half-suit to the other team; that is a
  table-manners rule, not a strategic one, and enforcing it against a mouse would
  measure misclicks rather than play.
* Misdeclaration is *not* prevented — you can name any allocation you like, and a
  wrong one gives the half-suit away exactly as the rules say.
* Table talk beyond the rules' allowances (asking a card, declaring, transferring
  a turn, and willingness bits in the forced endgame) has nowhere to happen, so
  the leak the rules forbid cannot occur.
