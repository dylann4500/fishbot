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

The setup screen lays the six seats out as the two teams: **0, 2, 4 are Team A**
and **1, 3, 5 are Team B**, so a human at seat 0 has teammates at 2 and 4 and
opponents at 1, 3, 5. Each seat takes two things:

* **A name.** Six seats labelled "FishBot v0.4" are unreadable — you cannot tell
  who asked what. Names default to distinct call signs and "Shuffle names"
  redraws them; whatever you type is what appears on the table, in the log, and
  in every declaration. Duplicates are numbered rather than silently merged.
* **An engine**, or *You (play this seat)*. The engine list is the same one
  `fish match --a=` accepts: `v04` (the deployed v0.4-Fast configuration — every
  performance number in the paper), `v04-Block` (the exact reference belief),
  `v03`, `v02`, and the baseline population.

Three presets cover the common cases: you plus two v0.4 teammates against three
v0.4s, you plus two v0.3 teammates against three v0.4s, and six bots to watch.

Leaving the seed blank draws one from the clock; setting it replays the same deal
and the same dealer, which is what you want when comparing lines with a teammate.

More than one seat may be human. Each browser then picks which seat it is looking
at, so two people on two machines can share a table; a seat's hand is only ever
sent to a client that asks for that seat.

## Playing

The table shows every seat with its name, its engine, its card count, and a ring
on whoever is to play. Your hand is fanned along the bottom.

* **Asking.** On your turn an **Ask** chip appears on each opponent who still has
  cards. Click one and you get that opponent's askable cards — half-suits you
  already hold a card of, minus the cards in your hand. Nothing illegal is
  offered, so the grid *is* the rule. Click a card to ask for it.
* **Declaring.** The **Declare** button is live at all times, including during an
  opponent's turn, because the rules allow a declaration at any moment. Pick a
  half-suit, then name the teammate holding each of its six cards; cards in your
  hand are pre-assigned to you. Half-suits you hold nothing in are dimmed but
  still selectable — the rules explicitly permit declaring one, so nothing is
  blocked on that. Off turn the declaration is *queued* and announced at the
  engine's next declaration poll, which happens before every ask — exactly the
  granularity the bots get, so neither side is favoured. On your turn a
  declaration replaces your ask.
* **Running out of cards.** If you are cardless and hold the turn, a dialog asks
  which live teammate takes over. You can still declare while cardless.
* **Forced endgame.** When the other team is out of cards, your team must declare
  every remaining half-suit with no more asking. You are offered each half-suit
  in turn: take it with an allocation, step aside for a teammate, or hand the
  rest to your teammates. If nobody steps forward, somebody must still name an
  allocation, and that prompt cannot be declined.
* **Game over** reveals the opening hands of all six seats, so you can see what
  everyone was actually holding while you were guessing.

### Pace

The pace is *a bot's move time*, so it is charged by who acts **next**, not by
who just moved. You never wait to make your own move:

| What happened | Who is next | Wait |
|---|---|---|
| You ask and miss | the bot you asked | it replies after the delay |
| You ask and hit | you again | none — ask straight away |
| A bot asks you and you have nothing | you | none — you are prompted at once |
| A bot asks you and you hand the card over | that bot | its next ask is paced |

Once the turn is yours there is nothing to wait for: you set your own tempo and
can read the board for as long as you like before acting. The one exception is
the forced endgame, where nobody is "to move" in the asking sense and every event
is a declaration worth watching, so the delay stands there.

The **Pace** slider sits in the sidebar and takes effect immediately — including mid-delay, so dragging it down
from twenty seconds does not leave you waiting out the old value. It runs from
*instant* to twenty seconds per move; the track is squared, so the first half of
the travel covers nought to five seconds where the useful settings live, and the
top end is there for studying a position. The default is two seconds. The same
slider is on the setup screen, and the last value you chose is remembered.

**Pause** freezes the table before the next event and **Step** releases one event
at a time. Pause applies to every seat including yours — pausing means the table
is frozen, whoever moved last. Pause is the reliable way to compose a declaration in the middle of a
long bot run — at *instant* pace the bots can chain many asks before you finish.

## What you can and cannot see

You receive your own hand and the public event stream, and nothing else — the
same information the engine hands each bot.

* Card counts are shown for every seat, which the rules let you ask for.
* The status banner carries the most recent action, and **the previous two asks**
  are one click below it — again exactly what the rules entitle you to.
* The opening deal is revealed only after the game ends.

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

## Where the code lives

| File | Contents |
|---|---|
| `engine/web/index.html` | The whole browser client — markup, styles, card rendering, modals. No build step; edit and reload. |
| `engine/src/human.hpp` | The human `Agent`: the blocking decision points and how they map to the rules. |
| `engine/src/table.hpp` | Session lifecycle, the public snapshot, and the JSON the browser reads. |
| `engine/src/serve.hpp` | HTTP routes and request validation. |
| `engine/src/httpd.hpp` | Minimal loopback socket server. |
