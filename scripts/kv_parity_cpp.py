"""Check the engine's C++ port of KV's policy against KV's Python original.

The TypeScript port is verified against fish/agents.py by kv_parity_ref.py; this
puts engine/src/kv.hpp on the same captured observations with the same particle
set, so a transcription error between the two ports cannot hide.

Usage: python3 scripts/kv_parity_cpp.py <kv-repo> <decisions.json> [fish-binary]
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from types import SimpleNamespace

repo, dump = sys.argv[1], sys.argv[2]
binary = sys.argv[3] if len(sys.argv) > 3 else "engine/fish"
sys.path.insert(0, repo)

from fish.agents import SearchAgent  # noqa: E402
from fish.belief import OwnershipParticle  # noqa: E402
from fish.core import AskAction, ClaimAction, GamePhase, Ruleset  # noqa: E402

RULES = Ruleset.joker_54()
HALF_SUIT_OF = lambda card: RULES.cards[int(card)].half_suit  # noqa: E731
records = json.load(open(dump))

flat = tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False)
for record in records:
    o = record["observation"]
    flat.write(f"DEC {o['player']} {o['ply']} 9\n")
    flat.write(f"HAND {len(o['hand'])} " + " ".join(str(c) for c in o["hand"]) + "\n")
    flat.write("COUNTS " + " ".join(str(c) for c in o["cardCounts"]) + "\n")
    flat.write("RESOLVED " + " ".join("1" if r else "0" for r in o["resolved"]) + "\n")
    flat.write(f"HIST {len(o['history'])}\n")
    for e in o["history"]:
        flat.write(f"E {e['actor']} {e['target']} {e['card']} {1 if e['success'] else 0}\n")
    flat.write(f"PART {len(record['particles'])}\n")
    for particle in record["particles"]:
        flat.write("P " + " ".join(str(v) for v in particle) + "\n")
    flat.write(f"ASKS {len(o['legalAsks'])}\n")
    for ask in o["legalAsks"]:
        flat.write(f"A {ask['card']} {ask['target']}\n")
    flat.write("END\n")
flat.close()

proc = subprocess.run([binary, "kvparity", f"--file={flat.name}"], capture_output=True, text=True)
if proc.returncode != 0:
    sys.exit(f"fish kvparity failed: {proc.stderr}")

cpp = []
current = None
for line in proc.stdout.splitlines():
    parts = line.split()
    if parts[0] == "DEC":
        current = {"scores": {}, "claims": {}}
        cpp.append(current)
    elif parts[0] == "SCORE":
        current["scores"][f"{parts[1]}:{parts[2]}"] = (float(parts[3]), float(parts[4]))
    elif parts[0] == "CLAIM":
        key = f"{parts[1]}:{','.join(parts[2:8])}"
        current["claims"][key] = (float(parts[8]), float(parts[9]))

stats = {"decisions": 0, "basic": 0, "basic_bad": 0, "prob": 0, "prob_bad": 0,
         "claims_py": 0, "claims_cpp": 0, "claim_missing": 0, "claim_bad": 0,
         "max_abs_diff": 0.0}


def close(a, b):
    return abs(a - b) <= 1e-9 * max(1.0, abs(a), abs(b))


for record, out in zip(records, cpp):
    o = record["observation"]
    obs = SimpleNamespace(
        ruleset=RULES, player=o["player"], own_hand=tuple(o["hand"]),
        card_counts=tuple(o["cardCounts"]),
        resolved_owners=tuple(0 if r else -1 for r in o["resolved"]),
        history=tuple(SimpleNamespace(asker=e["actor"], target=e["target"], card=e["card"], success=e["success"])
                      for e in o["history"]),
        legal_asks=tuple(AskAction(o["player"], a["target"], a["card"]) for a in o["legalAsks"]),
        legal_claim_half_suits=tuple(o["legalClaimHalfSuits"]),
        legal_admin_actions=(), ply=o["ply"], phase=GamePhase.PLAY, done=False, team=o["player"] & 1,
    )
    agent = SearchAgent(range(54), particle_count=96, determinizations=48, depth=2,
                        half_suit_of=HALF_SUIT_OF, seed=0)
    agent.belief.particles = tuple(
        OwnershipParticle({c: (None if v < 0 else v) for c, v in enumerate(p)})
        for p in record["particles"])
    agent.belief.update = lambda observation: agent.belief
    stats["decisions"] += 1

    for action in obs.legal_asks:
        key = f"{action.card}:{action.target}"
        if key not in out["scores"]:
            continue
        basic, prob = out["scores"][key]
        stats["basic"] += 1
        if not close(agent.score_action(obs, action), basic):
            stats["basic_bad"] += 1
        stats["prob"] += 1
        reference = agent._probabilistic_score(obs, action)
        stats["max_abs_diff"] = max(stats["max_abs_diff"], abs(reference - prob))
        if not close(reference, prob):
            stats["prob_bad"] += 1

    py_claims = {}
    for action in agent.candidate_actions(obs):
        if not isinstance(action, ClaimAction):
            continue
        key = f"{action.half_suit}:{','.join(str(x) for x in action.allocation)}"
        py_claims[key] = agent._probabilistic_score(obs, action)
    stats["claims_py"] += len(py_claims)
    stats["claims_cpp"] += len(out["claims"])
    for key, score in py_claims.items():
        if key not in out["claims"]:
            stats["claim_missing"] += 1
            continue
        stats["max_abs_diff"] = max(stats["max_abs_diff"], abs(out["claims"][key][1] - score))
        if not close(out["claims"][key][1], score):
            stats["claim_bad"] += 1

print(json.dumps(stats, indent=2))
