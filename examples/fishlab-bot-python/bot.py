#!/usr/bin/env python3
"""Sherlock -- a reference FishLab bot, in about two hundred lines of Python.

This is the worked example for docs/BOT_PACKAGE.md.  It is not a strong bot; it
is a *complete* one, in the sense that it answers every request the protocol can
make and never returns an illegal move, which is the part that is fiddly the
first time.  Copy it, keep the plumbing, replace `pick_ask` and friends.

It has no dependencies and keeps no state between requests: every request from
the engine carries the whole public record, so the bot rebuilds what it knows
from scratch each time.  That is slower than tracking incrementally and much
harder to get wrong.

What it knows how to prove, from the public record alone:

  * a successful ask moves a named card between two named seats, so afterwards
    everybody knows exactly where it is;
  * a failed ask proves the target does NOT hold that card, and also that the
    asker does not (you may not ask for a card you hold) but DOES hold some
    other card of that half-suit (you may not ask into a half-suit you are out
    of);
  * a resolved half-suit leaves play entirely.

It declares a half-suit when it can prove every one of the six cards is on its
own team, asks for cards it can prove an opponent holds when it can, and
otherwise asks into the half-suit where its team has the most to gain.
"""

import json
import sys

SET_SIZE = 6
NUM_SEATS = 6


class Table:
    """Everything the public record proves, rebuilt from one state message."""

    def __init__(self, cards, state):
        self.cards = cards                        # index -> name, engine order
        self.index = {n: i for i, n in enumerate(cards)}
        self.seat = state["seat"]
        self.turn = state["turn"]
        self.hand = set(self.index[c] for c in state["hand"])
        self.counts = state["hand_counts"]
        self.active = state["set_active"]
        self.n_sets = len(self.active)

        # holder[card] = seat known to hold it, or None
        self.holder = {}
        # denied[card] = set of seats known NOT to hold it
        self.denied = {c: set() for c in range(len(cards))}
        for c in self.hand:
            self.holder[c] = self.seat
        for c in range(len(cards)):
            if c not in self.hand:
                self.denied[c].add(self.seat)

        for ev in state["history"]:
            self.apply(ev)

        # A half-suit that has been won is gone; forget everything about it.
        for s in range(self.n_sets):
            if not self.active[s]:
                for i in range(SET_SIZE):
                    self.holder.pop(s * SET_SIZE + i, None)

    def apply(self, ev):
        kind = ev["t"]
        if kind == "ask":
            card = self.index[ev["card"]]
            asker, target = ev["actor"], ev["target"]
            self.denied[card].add(asker)          # you never ask for what you hold
            if ev["success"]:
                self.holder[card] = asker
                self.denied[card] = set(p for p in range(NUM_SEATS) if p != asker)
            else:
                self.denied[card].add(target)
                if self.holder.get(card) in (asker, target):
                    self.holder.pop(card, None)
        elif kind == "declare":
            for i in range(SET_SIZE):
                self.holder.pop(ev["set"] * SET_SIZE + i, None)
        # A pass moves the turn and no cards, so there is nothing to learn.

    # ------------------------------------------------------------- helpers
    def team(self, seat):
        return seat & 1

    def teammates(self):
        return [p for p in range(NUM_SEATS) if self.team(p) == self.team(self.seat)]

    def opponents(self):
        return [p for p in range(NUM_SEATS)
                if self.team(p) != self.team(self.seat) and self.counts[p] > 0]

    def my_sets(self):
        """Half-suits still in play that I hold at least one card of -- the only
        ones I am allowed to ask into."""
        out = []
        for s in range(self.n_sets):
            if not self.active[s]:
                continue
            if any((s * SET_SIZE + i) in self.hand for i in range(SET_SIZE)):
                out.append(s)
        return out

    def proven_allocation(self, s):
        """The six holders of half-suit `s` if every one of them is proven and
        on my team; otherwise None."""
        owners = []
        mine = self.teammates()
        for i in range(SET_SIZE):
            who = self.holder.get(s * SET_SIZE + i)
            if who is None or who not in mine:
                return None
            owners.append(who)
        return owners

    def best_guess_allocation(self, s):
        """Six seats on my team, using what is proven and guessing the rest.
        Never returns None: the engine's last resort has to have an answer."""
        mine = [p for p in self.teammates() if self.counts[p] > 0] or self.teammates()
        owners = []
        for i in range(SET_SIZE):
            card = s * SET_SIZE + i
            who = self.holder.get(card)
            if who is not None and self.team(who) == self.team(self.seat):
                owners.append(who)
                continue
            # Nobody proven: give it to the teammate least often denied it, and
            # break ties towards the fullest hand.
            best = max(mine, key=lambda p: (p not in self.denied[card], self.counts[p]))
            owners.append(best)
        return owners

    def confidence(self, s, owners):
        known = sum(1 for i in range(SET_SIZE)
                    if self.holder.get(s * SET_SIZE + i) == owners[i])
        return known / float(SET_SIZE)


# --------------------------------------------------------------- decisions
def pick_declaration(t):
    """Declare only what can be proven.  A wrong declaration hands the half-suit
    to the other team, so a guess is worse than waiting."""
    for s in range(t.n_sets):
        if not t.active[s]:
            continue
        owners = t.proven_allocation(s)
        if owners:
            return {"action": "declare", "set": s, "owner": owners, "confidence": 1.0}
    return {"action": "none"}


def pick_ask(t):
    """Ask for the card most likely to be there, in the half-suit worth most."""
    best, best_score = None, None
    for s in t.my_sets():
        # How much of this half-suit my team already has: finishing one I nearly
        # own is worth more than starting one I barely hold.
        held = sum(1 for i in range(SET_SIZE)
                   if t.holder.get(s * SET_SIZE + i) in t.teammates())
        for i in range(SET_SIZE):
            card = s * SET_SIZE + i
            if card in t.hand:
                continue
            for target in t.opponents():
                if target in t.denied[card]:
                    continue                       # proven not to have it
                score = held
                if t.holder.get(card) == target:
                    score += 100                   # proven to have it: free card
                score += t.counts[target] * 0.01   # a fuller hand is likelier
                if best_score is None or score > best_score:
                    best, best_score = (t.cards[card], target), score
    if best is None:
        # Every proven-safe ask is used up; fall back to any legal one at all.
        for s in t.my_sets():
            for i in range(SET_SIZE):
                card = s * SET_SIZE + i
                if card in t.hand:
                    continue
                for target in t.opponents():
                    return {"action": "ask", "card": t.cards[card], "target": target}
        # A seat with cards always has a legal ask, so this cannot be reached in
        # a well-formed position; answering with an error beats guessing.
        return {"error": "no legal ask from this position"}
    return {"action": "ask", "card": best[0], "target": best[1]}


def pick_pass(t, candidates):
    """I have no cards left; hand the turn to the teammate with the most."""
    return {"action": "pass", "to": max(candidates, key=lambda p: t.counts[p])}


def pick_forced(t, s, threshold, last_resort):
    """The endgame: one team is out of cards and the other must name every
    remaining half-suit.  Answer when confident enough, and ALWAYS answer when
    the engine says this is the last resort -- otherwise it allocates on your
    behalf and the guess it makes is a bad one."""
    owners = t.proven_allocation(s)
    if owners:
        return {"action": "declare", "set": s, "owner": owners, "confidence": 1.0}
    owners = t.best_guess_allocation(s)
    conf = t.confidence(s, owners)
    if last_resort or conf >= threshold:
        return {"action": "declare", "set": s, "owner": owners, "confidence": conf}
    return {"action": "none"}


# ------------------------------------------------------------------- main
def main():
    cards = None
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        req = json.loads(line)
        op = req.get("op")

        if op == "hello":
            cards = req["cards"]
            reply = {"ok": True, "name": "Sherlock", "version": "1.0",
                     "protocol": "fishlab-json-v1"}
        elif op == "new_game":
            reply = {"ok": True}
        elif op in ("ask", "declare_poll", "pass", "forced"):
            t = Table(cards, req["state"])
            if op == "ask":
                reply = pick_ask(t)
            elif op == "declare_poll":
                reply = pick_declaration(t)
            elif op == "pass":
                reply = pick_pass(t, req["candidates"])
            else:
                reply = pick_forced(t, req["set"], req.get("threshold", 0.0),
                                    req.get("last_resort", False))
        else:
            reply = {"error": "unknown op %r" % (op,)}

        # One line, flushed.  A reply that sits in a buffer looks exactly like a
        # bot that has hung.
        sys.stdout.write(json.dumps(reply) + "\n")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
