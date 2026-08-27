"""Recompute FishLab's ported KV decisions with KV's original Python code.

Usage: python3 scripts/kv_parity_ref.py <kv-repo-path> <decisions.json>

FishLab and KV's engine number cards identically (half-suit h, position i ->
card 6h+i), so card identities map one to one; only the suit *names* attached to
each half-suit differ, and no policy code reads them.

Every deterministic layer is compared exactly: the belief's feasible domains,
BasicHeuristicAgent.score_action, ProbabilisticAgent._probabilistic_score, the
root-ask pruning order, ProbabilisticAgent.candidate_actions' claim set, and the
full SearchAgent.choose_action estimate vector. The particle set and the
determinization draw are taken from the TypeScript run so the only remaining
difference -- the pseudo-random stream -- is held fixed.
"""

from __future__ import annotations

import json
import sys
from types import SimpleNamespace

repo, dump = sys.argv[1], sys.argv[2]
sys.path.insert(0, repo)

from fish.agents import SearchAgent  # noqa: E402
from fish.belief import OwnershipParticle, ParticleBelief  # noqa: E402
from fish.core import AskAction, ClaimAction, GamePhase, Ruleset  # noqa: E402

RULES = Ruleset.joker_54()
HALF_SUIT_OF = lambda card: RULES.cards[int(card)].half_suit  # noqa: E731


def key(action):
    if isinstance(action, ClaimAction):
        return f"claim:{action.half_suit}:{','.join(str(o) for o in action.allocation)}"
    if isinstance(action, AskAction):
        return f"ask:{action.card}:{action.target}"
    if action["kind"] == "claim":
        return f"claim:{action['halfSuit']}:{','.join(str(o) for o in action['allocation'])}"
    return f"ask:{action['card']}:{action['target']}"


def build_observation(record):
    obs = record["observation"]
    history = tuple(
        SimpleNamespace(asker=e["actor"], target=e["target"], card=e["card"], success=e["success"])
        for e in obs["history"]
    )
    return SimpleNamespace(
        ruleset=RULES,
        player=obs["player"],
        team=obs["player"] & 1,
        own_hand=tuple(obs["hand"]),
        turn=obs["player"],
        phase=GamePhase.PLAY,
        pending_selector=None,
        forced_claimer=None,
        card_counts=tuple(obs["cardCounts"]),
        scores=(0, 0),
        resolved_owners=tuple(0 if resolved else -1 for resolved in obs["resolved"]),
        history=history,
        legal_asks=tuple(AskAction(obs["player"], a["target"], a["card"]) for a in obs["legalAsks"]),
        legal_claim_half_suits=tuple(obs["legalClaimHalfSuits"]),
        legal_admin_actions=(),
        done=False,
        ply=obs["ply"],
    )


def to_particle(owners):
    return OwnershipParticle({card: (None if owner < 0 else owner) for card, owner in enumerate(owners)})


def close(a, b, tol=1e-9):
    return abs(a - b) <= tol * max(1.0, abs(a), abs(b))


records = json.load(open(dump))
stats = {
    "decisions": 0, "domain_violations": 0, "count_violations": 0,
    "basic": 0, "basic_bad": 0, "prob": 0, "prob_bad": 0,
    "root_mismatch": 0, "candidate_mismatch": 0,
    "estimates": 0, "estimate_bad": 0, "argmax_mismatch": 0,
    "max_abs_diff": 0.0,
}

for record in records:
    obs = build_observation(record)
    particles = [to_particle(owners) for owners in record["particles"]]
    stats["decisions"] += 1

    # 1. Feasible domains: rebuild them with the original and check that every
    #    particle the port produced is legal under KV's own constraints.
    reference = ParticleBelief(range(54), particle_count=1, seed=0, half_suit_of=HALF_SUIT_OF)
    reference.update(obs)
    for particle in particles:
        for card in range(54):
            if particle.owner(card) not in reference.domains[card]:
                stats["domain_violations"] += 1
        counts = [0] * 6
        for card in range(54):
            owner = particle.owner(card)
            if owner is not None:
                counts[owner] += 1
        if tuple(counts) != obs.card_counts:
            stats["count_violations"] += 1

    agent = SearchAgent(
        range(54), particle_count=96, determinizations=48, depth=2,
        half_suit_of=HALF_SUIT_OF, seed=0,
    )
    agent.belief.particles = tuple(particles)
    agent.belief.update = lambda observation: agent.belief  # already fixed

    ported = {key(item["action"]): item["value"] for item in record["estimates"]}

    # 2/3. Base and probabilistic scores for every legal ask.
    ts_scores = record.get("askScores", {})
    for action in obs.legal_asks:
        name = key(action)
        if name in ts_scores:
            stats["basic"] += 1
            if not close(agent.score_action(obs, action), ts_scores[name]["basic"]):
                stats["basic_bad"] += 1
            stats["prob"] += 1
            diff = abs(agent._probabilistic_score(obs, action) - ts_scores[name]["prob"])
            stats["max_abs_diff"] = max(stats["max_abs_diff"], diff)
            if not close(agent._probabilistic_score(obs, action), ts_scores[name]["prob"]):
                stats["prob_bad"] += 1

    # 4. Root-ask pruning: same 12 actions in the same order.
    root = sorted(obs.legal_asks, key=lambda a: agent._probabilistic_score(obs, a), reverse=True)[:12]
    if [key(a) for a in root] != [key(a) for a in record["rootAsks"]]:
        stats["root_mismatch"] += 1

    # 5. Candidate action set, claims included.
    candidates = agent.candidate_actions(obs)
    if sorted(key(a) for a in candidates) != sorted(key(a) for a in record["candidates"]):
        stats["candidate_mismatch"] += 1

    # 6. Full search estimates against the same determinization draw.
    draw = iter([particles[index] for index in record["determinizations"]])
    agent.belief.sample = lambda rng=None: next(draw)
    agent.choose_action(obs, [a for a in candidates])
    for estimate in agent.last_estimates:
        name = key(estimate.action)
        if name not in ported:
            continue
        stats["estimates"] += 1
        diff = abs(estimate.value - ported[name])
        stats["max_abs_diff"] = max(stats["max_abs_diff"], diff)
        if not close(estimate.value, ported[name]):
            stats["estimate_bad"] += 1
    best = max(agent.last_estimates, key=lambda item: item.value)
    ts_best = max(record["estimates"], key=lambda item: item["value"])
    if not close(best.value, ts_best["value"]):
        stats["argmax_mismatch"] += 1

print(json.dumps(stats, indent=2))
