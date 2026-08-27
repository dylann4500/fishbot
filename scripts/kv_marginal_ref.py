"""Compare the ported particle sampler's ownership marginals with the original's.

The port reproduces `ParticleBelief._sample_assignment`'s randomized
backtracking, but on a different pseudo-random stream, so the two samplers can
only be compared distributionally. Python is run twice with different seeds to
give a same-implementation noise floor to read the cross-implementation gap
against.

Usage: python3 scripts/kv_marginal_ref.py <kv-repo> <parity.json> <marginals.json> [particles]
"""

from __future__ import annotations

import json
import sys
from types import SimpleNamespace

repo, dump, ported_path = sys.argv[1], sys.argv[2], sys.argv[3]
count = int(sys.argv[4]) if len(sys.argv) > 4 else 2000
sys.path.insert(0, repo)

from fish.belief import ParticleBelief  # noqa: E402
from fish.core import GamePhase, Ruleset  # noqa: E402

RULES = Ruleset.joker_54()
HALF_SUIT_OF = lambda card: RULES.cards[int(card)].half_suit  # noqa: E731

records = json.load(open(dump))
ported = json.load(open(ported_path))


def observation_for(index):
    o = records[index]["observation"]
    return SimpleNamespace(
        ruleset=RULES, player=o["player"], own_hand=tuple(o["hand"]),
        card_counts=tuple(o["cardCounts"]),
        resolved_owners=tuple(0 if r else -1 for r in o["resolved"]),
        history=tuple(SimpleNamespace(asker=e["actor"], target=e["target"], card=e["card"], success=e["success"])
                      for e in o["history"]),
        legal_asks=(), legal_claim_half_suits=(), legal_admin_actions=(),
        ply=o["ply"], phase=GamePhase.PLAY, done=False,
    )


def marginals(observation, seed):
    belief = ParticleBelief(range(54), particle_count=count, seed=seed, half_suit_of=HALF_SUIT_OF)
    belief.update(observation)
    n = len(belief.particles)
    table = [[0.0] * 6 for _ in range(54)]
    for particle in belief.particles:
        for card in range(54):
            owner = particle.owner(card)
            if owner is not None:
                table[card][owner] += 1.0
    return [[value / n for value in row] for row in table], n


def gap(a, b):
    diffs = [abs(a[card][owner] - b[card][owner]) for card in range(54) for owner in range(6)]
    return max(diffs), sum(diffs) / len(diffs)


print(f"{'obs':>5} {'ply':>5} {'ts_n':>6} {'py_n':>6}  {'TS vs PY max':>13} {'mean':>7}   {'PY vs PY max':>13} {'mean':>7}")
for entry in ported:
    observation = observation_for(entry["index"])
    reference, n1 = marginals(observation, 11)
    control, _ = marginals(observation, 97)
    cross_max, cross_mean = gap(entry["marginals"], reference)
    self_max, self_mean = gap(reference, control)
    ply = records[entry["index"]]["observation"]["ply"]
    print(f"{entry['index']:>5} {ply:>5} {entry['particles']:>6} {n1:>6}  {cross_max:>13.4f} {cross_mean:>7.4f}   {self_max:>13.4f} {self_mean:>7.4f}")
