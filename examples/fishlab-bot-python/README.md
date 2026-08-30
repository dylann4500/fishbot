# Sherlock — the reference FishLab bot

A complete, dependency-free bot in one file. It is not a strong player; it is a
*correct* one, which is the part that is fiddly the first time. Copy it and
replace the four decision functions at the bottom.

```bash
cd examples
zip -r sherlock.zip fishlab-bot-python
cd ../engine && make
./fish bots add ../examples/sherlock.zip
./fish bots check sherlock
```

Then seat it: `./fish serve` and pick **Sherlock** at any seat, or measure it
against the house engine:

```bash
./fish match --a=bot:sherlock --b=v06 --games=200 --rotations=6
```

It loses badly, which is the point of having something to beat.

## What it does

Everything it knows, it proves from the public record, which every request
carries in full:

* a successful ask moves a named card between two named seats, so afterwards
  everybody knows exactly where it is;
* a failed ask proves the target does **not** hold that card — and also that the
  asker does not (you may not ask for a card you hold);
* a resolved half-suit leaves play entirely.

From that it declares a half-suit only when it can prove all six cards are on its
own team, asks for cards it can prove an opponent holds, and otherwise asks into
the half-suit where its team has most to gain. In the forced endgame it answers
with its best allocation and an honest confidence, and always answers when the
engine says `last_resort`.

What it does not do, and what you would add: any belief over cards it cannot
prove, any model of what an opponent's ask reveals about their hand, any search,
and any notion that its own asks tell the table something too.

The format and the protocol are specified in [`docs/BOT_PACKAGE.md`](../../docs/BOT_PACKAGE.md).
