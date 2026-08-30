# The FishLab table — playing FishBot v0.6 yourself

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

* **A name.** Six seats labelled "FishBot v0.6" are unreadable — you cannot tell
  who asked what. Names default to distinct call signs and "Shuffle names"
  redraws them; whatever you type is what appears on the table, in the log, and
  in every declaration. Duplicates are numbered rather than silently merged.
* **An engine**, or *You (play this seat)*. The engine list is the same one
  `fish match --a=` accepts, so anything you can measure on the command line you
  can also sit down opposite:

  | group | entries |
  |---|---|
  | FishBot | `v06` — the deployed v0.6 policy and the default at every bot seat; **v0.6-Search** (`v06:s1=1,det=12,cand=4,kappa=2.5,roll=v06`) — v0.6 plus determinized information-set search, the strongest configuration measured; `v05`; `v04` (the v0.4-Fast configuration behind the v0.4 paper); `v04:belief=block` (the exact reference belief); `v03`; `v02` |
  | Deceptive archetypes | `withholder`, `feint`, `silent` — the opponent styles v0.6 was fitted against |
  | Baselines | `lockout`, `detective`, `hunter`, `diversifier`, `bluffer`, `random` |
  | Uploaded bots | `bot:<id>` — a package somebody dropped on this table; see below |

  A seat's label on the table is derived from its spec, so a v0.6 seat with
  search on reads "FishBot v0.6-Search" rather than being indistinguishable from
  a plain v0.6.

Four presets cover the common cases: you plus two v0.6 teammates against three
v0.6s, the same against three v0.6-Searches, a v0.3 partnership against three
v0.6s, and six v0.6s to watch.

**v0.6-Search is fast enough to play against.** It costs roughly a third of a
second a decision — a complete game against three of them, with the pace slider
at *instant*, plays out in about seven seconds — so at any ordinary pace setting
its thinking time is invisible.

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
  blocked on that. Off turn the declaration is announced **immediately**: it cuts
  short a bot's pace delay and interjects even while another player is still
  choosing an ask — the rules allow a declaration at any moment, and the table
  honours that literally. (The player who was interrupted is re-prompted against
  the new board; an ask they had half-chosen is marked stale rather than played.)
  On your turn a declaration replaces your ask.
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

## Bringing a bot that is not ours

The **Bots** panel takes a `.zip` and puts somebody else's engine in every seat's
engine list. It is the standardised replacement for what this used to cost: a
GitHub link, a day of reading, and a hand-written bridge in `engine/src/` for
each new opponent. The package format and the protocol are
[`docs/BOT_PACKAGE.md`](BOT_PACKAGE.md); the worked example is
`examples/fishlab-bot-python`.

The host sees the panel on the setup screen and everybody else sees it on the
join screen, which is deliberate — a guest waiting for the deal is exactly the
person with a bot to contribute — and so is the split in what the two can do:

* **Uploading executes nothing.** A package is unzipped and sits inert, so any
  invited player may add one. `--lock-bots` narrows that to the host.
* **Seating, checking and installing dependencies run somebody's code on the
  host's machine**, with the host's permissions and no sandbox. Those stay with
  the host. It is the same trust as `git clone && python bot.py`, which is what
  everyone was doing by hand before; the difference is that it is now one drag,
  so it is worth saying rather than leaving implied.

**Check** plays complete games against a baseline and reports what the bot
actually answered — how many asks, how many declaration polls, whether the
forced endgame was ever reached — or the exact request and reply that broke.
Nothing is ever substituted for a bad move: a bot that answers illegally ends
its game with the diagnostic on the setup screen, which is a bug report its
author can act on. At the table that ends the game; in `fish match` it ends the
run, because a measured number must never come from a game a bot could not play.

Everything the panel does is also on the command line — `fish bots list | add |
check | prepare | remove` — and an installed package is a policy spec like any
other, so `fish match --a=bot:<id> --b=v07` measures it the way every number in
the papers was measured.

## Playing with other people

The table has always allowed more than one human seat; what `--lan` and
`--public` add is a way for those humans to be on different machines, and the
credentials that make that safe. **Three people against three FishBot v0.6s is
the "3 players vs 3× v0.6" preset**, which seats the humans at 0, 2 and 4.

```bash
cd engine && make
./fish serve --lan       # players on the same wifi
./fish serve --public    # players anywhere: publishes an https address
```

Either flag prints three things:

```
  HOST  http://127.0.0.1:8173/?h=4bc4b97a09508eec43054b171a2f62c5
  INVITE CODE  K7M2QW9D
  PLAYERS (same network)  http://10.61.66.228:8173/?j=K7M2QW9D
```

Open the **HOST** link yourself and send the **PLAYERS** link to everybody else.
The page banks whichever credential is in the link and strips it from the address
bar, so a screen-share or a browser history does not leak it.

### Why there are credentials at all

Fish is a hidden-information game, and before this the table would hand
`/api/state?seat=2` to anyone who asked. On loopback that did not matter — the
only client was the person who started the process — but on a shared address a
player who can read the other five hands is not playing Fish, and neither is
anybody sitting opposite them. So off loopback there are exactly three secrets:

| secret | who holds it | what it buys |
|---|---|---|
| **invite code** | everybody at the table | the public view — score, card counts, the log, the lobby — and the right to try to claim a seat |
| **seat token** | one browser, minted on claim | the only thing that will make the server disclose that seat's hand or accept a move for it |
| **host token** | the console of the machine running the server | the seat layout, the deal, pause, step and pace — and **no card visibility at all** |

Hosting a game and playing in one are separate powers held by separate secrets,
so the host sees no more of the deal than any spectator does. Every rejected
credential costs the caller a fifth of a second, which is the difference between
a feasible online guess at an eight-symbol invite and an infeasible one.

### The lobby

The host chooses the engines and leaves the human seats open; everyone else opens
the invite link, types a name and clicks a seat. Claims appear on the host's
screen as they happen, each with a **connected / away / open** dot, and **Deal
stays disabled until every human seat is filled** — an unclaimed one is a seat
the table would wait on forever. A player who reloads keeps their seat, because
the token is in their browser; a player who closes the tab and loses it needs the
host to press **free it** on that seat.

Claims survive a re-deal, so the same six seats play the next hand without
anybody rejoining.

### What each person sees

Everything the rules allow and nothing else. The state a browser receives
contains its own hand and the public event stream; another seat's cards are not
withheld in the interface, they are never sent. Asking for a hand you do not hold
the token for makes you a spectator of that seat, silently.

The table controls — pause, step, pace, new game — are simply absent for guests,
because they are properties of the table rather than of a viewer.

### Latency

A move reaches the other five browsers in about the time one round trip takes.
Rather than polling, each client holds a request open until something happens
(`/api/state?since=REV&wait=N`), so an idle table costs six requests a minute per
player instead of three a second, and a move is delivered the instant it is made
rather than up to 300 ms later. A dropped connection shows as a **Connection
lost** banner and retries on its own.

### `--public`: one link, sent to everybody

`--public` borrows a public `https://` address for the loopback port and prints
it with the invite code already in the query string, so **the host copies one
link and sends it to everyone**. No account, no inbound port opened on the
router, TLS for free, and the address dies with the process. The server stays
bound to **loopback** while a tunnel runs — only the tunnel client talks to it —
so `--public` is a narrower exposure than `--lan` even though it reaches
further. Combine them (`--lan --public`) if you want both.

The host's own browser cannot work this address out — it is sitting on
`127.0.0.1` — so the server hands it to the setup screen, and the **Copy** button
under *Send this to the other players* gives you the link that actually works.
It is sent only to the host.

Two providers are tried in order, because they need different outbound ports and
plenty of networks block one but not the other:

| provider | needs | account |
|---|---|---|
| `cloudflared` | outbound **7844**, and nothing else will do | none (`brew install cloudflared`) |
| `localhost.run` | outbound **22**, over ssh | none |

`--tunnel=cloudflared` or `--tunnel=ssh` pins one; the default is `auto`.

Both services hand out an address *before* the tunnel behind it is established,
so `--public` waits for each provider's "connected" line and **will not print an
address it has not seen come up** — a link that answers 530 to five people you
have just messaged is worse than a failure you can read. If every provider fails
it says which and why, and the table stays up on whatever addresses it has.

Round trips through cloudflared measure around 100 ms, and around 400 ms through
localhost.run; either is comfortably inside a turn-based card game.

**What the tunnel operator can see.** Both providers terminate TLS at their edge,
so the traffic — seat tokens included — is readable by them in transit. That is
the standard bargain for a free tunnel and it is fine for a card game among
friends. It would not be fine for anything else, and it is the reason to stop the
process when you are done rather than leaving a table up overnight.

### Exposure, honestly

`--lan` puts a hand-written C++ HTTP server on every interface of your machine.
It reads at most a 64 KB request, holds a socket for at most eight seconds, and
answers nothing but the table's own routes — but it is a small program on an open
port, so prefer `--public` (loopback bind, TLS, no inbound hole) when the players
are not in the room, and stop the process when you are done. There is no reason
to leave a table up overnight.

## Speaking the table

**Speak**, in the sidebar and on the setup screen, reads every ask, declaration,
turn pass and prompt aloud. It exists because a table is read with the eyes and a
hand is held in the head: you can study your own cards while the bots' asks
arrive through your ears instead of competing for the same attention. It is also
what makes *instant* pace usable at all — a run of six bot asks is over before
you have finished reading the first line of the log.

The narration is second-person for your own seat, so you hear "Lyra asks you for
the 7 of Spades. No." and then "Your turn." Prompts are spoken too: the cardless
turn pass, each half-suit offered in the forced endgame, and the final score.

* **Voice** picks any English voice your browser exposes and remembers the
  choice; changing it speaks a sample. **Speed** runs from 0.6× to 2×.
* **🔊**, or the **R** key when you are not typing, says the last event again
  together with whose turn it is — the spoken equivalent of the status banner.
* Speech is synthesised **by the browser, on this machine**. There is no API key,
  no account and no request: the table is a loopback server that has to work with
  the network unplugged, and a hosted voice could not keep up with a game that
  emits ninety-odd events anyway.

When the bots outrun the voice — which they do at *instant* pace — the narration
**skips ahead rather than falling behind**. Ordinary asks are dropped oldest
first, and once even declarations are stacking up those go the same way, so what
you hear is always roughly what is on screen. To follow a fast run event by
event, use **Pause** and **Step**, not the voice.

## What you can and cannot see

You receive your own hand and the public event stream, and nothing else — the
same information the engine hands each bot.

* Card counts are shown for every seat, which the rules let you ask for.
* The status banner carries the most recent action, and **the previous two asks**
  are one click below it — again exactly what the rules entitle you to.
* The opening deal is revealed only after the game ends.

The log shows only what the rules actually entitle a player to: declarations,
turn passes, and the previous two asks. There is deliberately no full-history
mode — real Fish is a memory game, and a scrollback is memory you did not do.
The bots keep perfect recall of every public event regardless; that asymmetry
is part of what playing them measures, and the Rules dialog says so.

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
| `engine/web/index.html` | The whole browser client — markup, styles, card rendering, modals, and the spoken narration. No build step; edit and reload. |
| `engine/src/human.hpp` | The human `Agent`: the blocking decision points and how they map to the rules. |
| `engine/src/table.hpp` | Session lifecycle, the public snapshot, the JSON the browser reads, and the allow-list of playable engines (`knownPolicy`) with their table labels (`policyLabel`). |
| `engine/src/lobby.hpp` | The three secrets, seat claims, presence, and the one function — `holdsSeat` — that decides whether a hand is disclosed. |
| `engine/src/tunnel.hpp` | `--public`: spawning cloudflared, reading its address out of the log, and refusing to report one until the tunnel is really connected. |
| `engine/src/serve.hpp` | HTTP routes and request validation, including the bot library's upload, check, prepare and remove. |
| `engine/src/botpkg.hpp` | The bot package: manifest, zip install, the on-disk library, and the virtualenv step. |
| `engine/src/extbot.hpp` | The FishLab bot protocol — the subprocess, the JSON, and every check that stops a bad answer from becoming a substituted move. |
| `engine/src/botcheck.hpp` | The self-check: complete games against a baseline, and the report an uploader gets back. |
| `engine/src/zip.hpp`, `minijson.hpp` | A zip reader and a JSON reader, so an upload needs no dependency the engine binary does not already have. |
| `engine/src/httpd.hpp` | Minimal socket server: loopback by default, every interface under `--lan`, with the request and deadline limits that go with being reachable. |
