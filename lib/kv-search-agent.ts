/**
 * KV's sampled-world search policy, ported to TypeScript.
 *
 * Source: github.com/kv1514/fish-researchp12 @ 0676737, `fish/agents.py`.
 * The ported class is `SearchAgent`, the top of that repository's policy
 * ladder (random < heuristic < memory < probabilistic < search) and the
 * `team_a` entry of its `configs/ismcts.yaml`. `SearchAgent` inherits from
 * `ProbabilisticAgent`, which inherits from `BasicHeuristicAgent`, so the port
 * carries all three scoring layers plus `fish/belief.py::ParticleBelief`.
 *
 * The port is structural rather than bit-identical: every formula, constant,
 * ordering rule and tie-break policy is reproduced, but the pseudo-random
 * stream is a JavaScript PRNG rather than CPython's Mersenne Twister, so the
 * random tie-breaks and the particle sample differ. Deterministic quantities
 * (belief domains, scores, world values) are checked numerically against the
 * Python original by `scripts/kv-parity-check.ts` and `scripts/kv_parity_ref.py`.
 *
 * Defaults are KV's shipped defaults: `fish/runner.py::make_agent` builds
 * `search` with particle_count=96, determinizations=48, depth=2, and the
 * remaining values come from `SearchAgent.__init__` / `ProbabilisticAgent.__init__`.
 */

export type KVRuleset = {
  deckSize: number;
  numPlayers: number;
  /** card id -> half-suit id */
  halfSuitOf: Int8Array;
  /** half-suit id -> its card ids, in claim-allocation order */
  halfSuitCards: number[][];
};

export type KVSearchConfig = {
  particleCount: number;
  determinizations: number;
  depth: number;
  successReward: number;
  failurePenalty: number;
  maxRootActions: number;
  maxRolloutActions: number;
  informationWeight: number;
  claimSupportThreshold: number;
  stalemateClaimAfter: number;
};

export const KV_SEARCH_DEFAULTS: KVSearchConfig = {
  particleCount: 96,        // runner.make_agent
  determinizations: 48,     // runner.make_agent
  depth: 2,                 // runner.make_agent
  successReward: 1,         // SearchAgent.__init__
  failurePenalty: .2,       // SearchAgent.__init__
  maxRootActions: 12,       // SearchAgent.__init__
  maxRolloutActions: 4,     // SearchAgent.__init__
  informationWeight: .08,   // ProbabilisticAgent.__init__
  claimSupportThreshold: .75,   // ProbabilisticAgent.candidate_actions
  stalemateClaimAfter: 256,     // BasicHeuristicAgent.__init__
};

export type KVObservation = {
  player: number;
  hand: number[];
  cardCounts: number[];
  /** half-suit id -> true when the half-suit has been resolved (claimed) */
  resolved: boolean[];
  history: { actor: number; target: number; card: number; success: boolean }[];
  legalAsks: { card: number; target: number }[];
  legalClaimHalfSuits: number[];
  ply: number;
};

export type KVAsk = { kind: "ask"; card: number; target: number };
export type KVClaim = { kind: "claim"; halfSuit: number; allocation: number[]; support: number };
export type KVAction = KVAsk | KVClaim;

export type KVEstimate = { action: KVAction; value: number; samples: number };

/** Captured only when `traceDecisions` is on; used by the parity harness. */
export type KVDecisionTrace = {
  observation: KVObservation;
  particles: number[][];
  candidates: KVAction[];
  rootAsks: KVAsk[];
  determinizations: number[];
  estimates: { action: KVAction; value: number }[];
  askScores: Record<string, { basic: number; prob: number }>;
  chosen: KVAction;
};

const RESOLVED = -1;

/** `math.isclose` with CPython's defaults (rel_tol=1e-9, abs_tol=0). */
function isClose(a: number, b: number) {
  return Math.abs(a - b) <= 1e-9 * Math.max(Math.abs(a), Math.abs(b));
}

function binaryEntropy(p: number) {
  if (p <= 0 || p >= 1) return 0;
  return -p * Math.log2(p) - (1 - p) * Math.log2(1 - p);
}

function popcount(mask: number) {
  let count = 0;
  for (let bit = mask; bit; bit &= bit - 1) count++;
  return count;
}

/** Deterministic PRNG standing in for `random.Random`. */
export class KVRandom {
  private state: number;
  constructor(seed: number) { this.state = (seed >>> 0) || 0x2545f491; }
  random() {
    this.state = (this.state + 0x6d2b79f5) | 0;
    let t = this.state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  }
  below(n: number) { return Math.floor(this.random() * n); }
  choice<T>(items: readonly T[]): T { return items[this.below(items.length)]; }
  /** Fisher-Yates in `random.shuffle`'s direction. */
  shuffle<T>(items: T[]) {
    for (let i = items.length - 1; i > 0; i--) {
      const j = this.below(i + 1);
      const swap = items[i]; items[i] = items[j]; items[j] = swap;
    }
    return items;
  }
}

export class KVInconsistentObservation extends Error {}

/**
 * Port of `fish/belief.py::ParticleBelief`.
 *
 * Ownership domains are rebuilt from the public event stream with exactly the
 * inferences the original makes — own hand, own non-hand exclusions, successful
 * transfers, failed-ask exclusions and resolved half-suits — and nothing more.
 * In particular the original does not infer that an asker cannot hold the card
 * it asked for, and neither does this port.
 *
 * Particles come from the same randomized backtracking search: minimum
 * remaining values ordering with a random tie-break, candidate owners shuffled
 * then stably sorted by remaining capacity, and a capacity-feasibility cut.
 */
export class KVParticleBelief {
  readonly particles: Int8Array[] = [];
  readonly domains: Uint8Array;
  private resolvedCard: Uint8Array;
  private signature = "";
  private allPlayersMask: number;
  private rng: KVRandom;
  /** Backtracking node budget; a guard against a pathological browser hang. */
  nodeBudget = 400_000;
  exhausted = 0;

  constructor(private rules: KVRuleset, private particleCount: number, seed: number) {
    this.domains = new Uint8Array(rules.deckSize);
    this.resolvedCard = new Uint8Array(rules.deckSize);
    this.allPlayersMask = (1 << rules.numPlayers) - 1;
    this.rng = new KVRandom(seed);
  }

  update(observation: KVObservation): this {
    const { deckSize, numPlayers, halfSuitCards } = this.rules;
    const player = observation.player;
    const signature = [
      player,
      observation.hand.join(","),
      observation.cardCounts.join(","),
      observation.resolved.map(value => value ? 1 : 0).join(""),
      observation.history.length,
    ].join("|");
    if (signature === this.signature && this.particles.length) return this;

    const counts = observation.cardCounts;
    if (counts.length !== numPlayers) throw new KVInconsistentObservation("public card count vector has wrong length");
    if (counts[player] !== observation.hand.length) throw new KVInconsistentObservation("observer hand disagrees with public card count");

    const domains = this.domains;
    const resolvedCard = this.resolvedCard;
    domains.fill(this.allPlayersMask);
    resolvedCard.fill(0);
    const inHand = new Uint8Array(deckSize);
    for (const card of observation.hand) inHand[card] = 1;
    for (let card = 0; card < deckSize; card++) {
      if (inHand[card]) domains[card] = 1 << player;
      else domains[card] &= ~(1 << player);
    }
    for (const event of observation.history) {
      if (event.success) domains[event.card] = 1 << event.actor;
      else domains[event.card] &= ~(1 << event.target);
    }
    for (const card of observation.hand) domains[card] = 1 << player;
    for (let halfSuit = 0; halfSuit < observation.resolved.length; halfSuit++) {
      if (!observation.resolved[halfSuit]) continue;
      for (const card of halfSuitCards[halfSuit]) { domains[card] = 0; resolvedCard[card] = 1; }
    }

    let activeTotal = 0;
    for (const count of counts) activeTotal += count;
    let resolvedCount = 0;
    for (let card = 0; card < deckSize; card++) resolvedCount += resolvedCard[card];
    if (resolvedCount !== deckSize - activeTotal) {
      throw new KVInconsistentObservation("resolved card identities are missing from the public observation");
    }
    for (let card = 0; card < deckSize; card++) {
      if (!resolvedCard[card] && !domains[card]) throw new KVInconsistentObservation("public facts eliminate every owner for a card");
    }

    this.particles.length = 0;
    const attempts = Math.max(64, this.particleCount * 12);
    for (let attempt = 0; attempt < attempts; attempt++) {
      const assignment = this.sampleAssignment(counts);
      if (assignment) {
        this.particles.push(assignment);
        if (this.particles.length >= this.particleCount) break;
      }
    }
    if (!this.particles.length) throw new KVInconsistentObservation("no ownership assignment satisfies the observation");
    this.signature = signature;
    return this;
  }

  private sampleAssignment(counts: number[]): Int8Array | null {
    const { deckSize, numPlayers } = this.rules;
    const domains = this.domains;
    const remaining = counts.slice();
    const result = new Int8Array(deckSize).fill(RESOLVED);
    const undecided: number[] = [];

    for (let card = 0; card < deckSize; card++) {
      if (this.resolvedCard[card]) continue;
      const domain = domains[card];
      if (popcount(domain) === 1) {
        const owner = 31 - Math.clz32(domain);
        result[card] = owner;
        remaining[owner] -= 1;
      } else {
        undecided.push(card);
      }
    }
    for (const value of remaining) if (value < 0) return null;

    const tieBreakers = new Map<number, number>();
    for (const card of undecided) tieBreakers.set(card, this.rng.random());
    undecided.sort((a, b) => popcount(domains[a]) - popcount(domains[b]) || tieBreakers.get(a)! - tieBreakers.get(b)!);

    // `sum(player in domains[item] for item in undecided[index:])`, precomputed.
    // Domains never change during the search, so this is the same predicate the
    // original evaluates inline, only without the repeated scan.
    const total = undecided.length;
    const suffix = new Int32Array((total + 1) * numPlayers);
    for (let index = total - 1; index >= 0; index--) {
      const domain = domains[undecided[index]];
      for (let owner = 0; owner < numPlayers; owner++) {
        suffix[index * numPlayers + owner] = suffix[(index + 1) * numPlayers + owner] + ((domain >> owner) & 1);
      }
    }

    let budget = this.nodeBudget;
    const assign = (index: number): boolean => {
      if (index === total) return remaining.every(value => value === 0);
      if (--budget < 0) return false;
      const card = undecided[index];
      const domain = domains[card];
      const candidates: number[] = [];
      for (let owner = 0; owner < numPlayers; owner++) {
        if ((domain >> owner) & 1 && remaining[owner] > 0) candidates.push(owner);
      }
      this.rng.shuffle(candidates);
      candidates.sort((a, b) => remaining[b] - remaining[a]);
      const base = (index + 1) * numPlayers;
      for (const owner of candidates) {
        result[card] = owner;
        remaining[owner] -= 1;
        let feasible = true;
        for (let candidate = 0; candidate < numPlayers; candidate++) {
          if (remaining[candidate] > suffix[base + candidate]) { feasible = false; break; }
        }
        if (feasible && assign(index + 1)) return true;
        remaining[owner] += 1;
        result[card] = RESOLVED;
      }
      return false;
    };

    if (assign(0)) return result;
    if (budget < 0) this.exhausted++;
    return null;
  }

  probability(card: number, owner: number) {
    if (!this.particles.length) throw new Error("belief must be updated before it is queried");
    let hits = 0;
    for (const particle of this.particles) if (particle[card] === owner) hits++;
    return hits / this.particles.length;
  }

  sample(rng: KVRandom) {
    if (!this.particles.length) throw new Error("belief must be updated before sampling");
    return rng.choice(this.particles);
  }
}

/**
 * Port of `SearchAgent`, including the `ProbabilisticAgent` candidate/claim
 * layer and the `BasicHeuristicAgent` base score it is built on. One instance
 * per seat, matching `runner.make_agent`'s "fresh policy for one seat".
 */
export class KVSearchAgent {
  readonly belief: KVParticleBelief;
  readonly config: KVSearchConfig;
  private rng: KVRandom;
  private progressSignature: string | null = null;
  private progressPly = 0;
  lastEstimates: KVEstimate[] = [];
  traceDecisions = false;
  lastTrace: KVDecisionTrace | null = null;
  private lastCandidates: KVAction[] = [];

  constructor(private rules: KVRuleset, seed: number, config: Partial<KVSearchConfig> = {}) {
    this.config = { ...KV_SEARCH_DEFAULTS, ...config };
    this.rng = new KVRandom(seed);
    // ProbabilisticAgent gives the belief the same seed value on a separate stream.
    this.belief = new KVParticleBelief(rules, this.config.particleCount, seed);
  }

  private team(player: number) { return player & 1; }

  private teammates(player: number) {
    const team: number[] = [];
    for (let candidate = 0; candidate < this.rules.numPlayers; candidate++) {
      if ((candidate & 1) === (player & 1)) team.push(candidate);
    }
    return team;
  }

  /** `BasicHeuristicAgent.score_action`. */
  basicScore(observation: KVObservation, action: KVAction) {
    if (action.kind === "claim") {
      const cards = this.rules.halfSuitCards[action.halfSuit];
      const hand = new Set(observation.hand);
      const complete = cards.every(card => hand.has(card))
        && action.allocation.every(owner => owner === observation.player);
      return complete ? 1000 : -1000;
    }
    const suit = this.rules.halfSuitOf[action.card];
    let suitCount = 0;
    for (const held of observation.hand) if (this.rules.halfSuitOf[held] === suit) suitCount++;
    const targetCount = observation.cardCounts[action.target];
    return suitCount * 2 + 1 / Math.max(1, targetCount);
  }

  /** `ProbabilisticAgent._probabilistic_score`. */
  probabilisticScore(observation: KVObservation, action: KVAction) {
    const base = this.basicScore(observation, action);
    if (action.kind === "claim") {
      const cards = this.rules.halfSuitCards[action.halfSuit];
      const claimingTeam = this.team(observation.player);
      let exact = 0;
      let opponentHeld = 0;
      for (const particle of this.belief.particles) {
        let matches = true;
        let opponent = false;
        for (let index = 0; index < cards.length; index++) {
          const owner = particle[cards[index]];
          if (owner !== action.allocation[index]) matches = false;
          if (owner !== RESOLVED && this.team(owner) !== claimingTeam) opponent = true;
        }
        if (matches) exact++;
        if (opponent) opponentHeld++;
      }
      const sampleCount = this.belief.particles.length;
      return 100 * (exact / sampleCount) - 120 * (opponentHeld / sampleCount) - 20;
    }
    const probability = this.belief.probability(action.card, action.target);
    return base + 20 * probability + this.config.informationWeight * binaryEntropy(probability);
  }

  /** `SearchAgent._world_action_value`. */
  private worldActionValue(
    particle: Int8Array,
    observation: KVObservation,
    action: KVAction,
    askActions: KVAsk[],
    depth: number,
    used: ReadonlySet<KVAsk>,
  ): number {
    if (action.kind === "claim") {
      const cards = this.rules.halfSuitCards[action.halfSuit];
      const claimingTeam = this.team(observation.player);
      let matches = true;
      let opponent = false;
      for (let index = 0; index < cards.length; index++) {
        const owner = particle[cards[index]];
        if (owner !== action.allocation[index]) matches = false;
        if (owner !== RESOLVED && this.team(owner) !== claimingTeam) opponent = true;
      }
      if (matches) return 6;
      if (opponent) return -6;
      return -2;
    }
    if (particle[action.card] !== action.target) return -this.config.failurePenalty;
    let value = this.config.successReward;
    if (depth <= 1) return value;
    let next = askActions.filter(candidate =>
      !used.has(candidate) && candidate !== action && particle[candidate.card] === candidate.target);
    if (next.length) {
      next = next
        .map(candidate => ({ candidate, score: this.probabilisticScore(observation, candidate) }))
        .sort((a, b) => b.score - a.score)
        .slice(0, this.config.maxRolloutActions)
        .map(item => item.candidate);
      const deeper = new Set(used);
      deeper.add(action);
      let best = -Infinity;
      for (const candidate of next) {
        best = Math.max(best, this.worldActionValue(particle, observation, candidate, askActions, depth - 1, deeper));
      }
      value += .85 * best;
    }
    return value;
  }

  /** `BasicHeuristicAgent._stalemate_claim_due`. */
  private stalemateClaimDue(observation: KVObservation) {
    const signature = observation.resolved.map(value => value ? 1 : 0).join("");
    const ply = observation.ply;
    if (signature !== this.progressSignature) {
      this.progressSignature = signature;
      this.progressPly = ply;
      return false;
    }
    if (ply - this.progressPly < this.config.stalemateClaimAfter) return false;
    this.progressPly = ply;
    return true;
  }

  /** `BasicHeuristicAgent._fallback_claim`. */
  private fallbackClaim(observation: KVObservation, halfSuit: number): KVClaim {
    const hand = new Set(observation.hand);
    const team = this.teammates(observation.player);
    const allocation = this.rules.halfSuitCards[halfSuit].map(card =>
      hand.has(card) ? observation.player : this.rng.choice(team));
    return { kind: "claim", halfSuit, allocation, support: 0 };
  }

  /** `ProbabilisticAgent.candidate_actions`. */
  candidateActions(observation: KVObservation): KVAction[] {
    const asks: KVAction[] = observation.legalAsks.map(ask => ({ kind: "ask", card: ask.card, target: ask.target }));
    const claimSuits = observation.legalClaimHalfSuits;
    if (!claimSuits.length) return asks;
    const stalled = this.stalemateClaimDue(observation);
    this.belief.update(observation);
    const actions = [...asks];
    const player = observation.player;
    let bestFallback: KVClaim | null = null;

    for (const halfSuit of claimSuits) {
      const cards = this.rules.halfSuitCards[halfSuit];
      const tally = new Map<string, { allocation: number[]; count: number }>();
      for (const particle of this.belief.particles) {
        const allocation: number[] = [];
        let teamOwned = true;
        for (const card of cards) {
          const owner = particle[card];
          if (owner === RESOLVED || this.team(owner) !== this.team(player)) { teamOwned = false; break; }
          allocation.push(owner);
        }
        if (!teamOwned) continue;
        const key = allocation.join(",");
        const entry = tally.get(key);
        if (entry) entry.count++;
        else tally.set(key, { allocation, count: 1 });
      }
      if (!tally.size) continue;
      // `Counter.most_common(1)`: highest count, first-inserted wins a tie.
      let top: { allocation: number[]; count: number } | null = null;
      for (const entry of tally.values()) if (!top || entry.count > top.count) top = entry;
      const support = top!.count / this.belief.particles.length;
      const claim: KVClaim = { kind: "claim", halfSuit, allocation: top!.allocation, support };
      if (!bestFallback || top!.count > bestFallback.support * this.belief.particles.length) bestFallback = claim;
      if (support >= this.config.claimSupportThreshold) actions.push(claim);
    }

    if (stalled) {
      if (bestFallback) return [bestFallback];
      const hand = new Set(observation.hand);
      let bestSuit = claimSuits[0];
      let bestHeld = -1;
      for (const halfSuit of claimSuits) {
        const held = this.rules.halfSuitCards[halfSuit].filter(card => hand.has(card)).length;
        if (held > bestHeld) { bestHeld = held; bestSuit = halfSuit; }
      }
      return [this.fallbackClaim(observation, bestSuit)];
    }
    if (!actions.length && bestFallback) actions.push(bestFallback);
    if (this.traceDecisions) this.lastCandidates = actions;
    return actions;
  }

  /** `SearchAgent.choose_action`. */
  chooseAction(observation: KVObservation, legalActions: KVAction[]): KVAction {
    if (!legalActions.length) throw new Error("agent was called without a legal action");
    this.belief.update(observation);
    const allAsks = legalActions.filter((action): action is KVAsk => action.kind === "ask");
    const askActions = allAsks
      .map(action => ({ action, score: this.probabilisticScore(observation, action) }))
      .sort((a, b) => b.score - a.score)
      .slice(0, this.config.maxRootActions)
      .map(item => item.action);
    const nonAsks = legalActions.filter(action => action.kind !== "ask");
    const searchActions: KVAction[] = [...askActions, ...nonAsks];
    const particles: Int8Array[] = [];
    for (let index = 0; index < this.config.determinizations; index++) particles.push(this.belief.sample(this.rng));

    const determinizations = this.traceDecisions ? particles.map(particle => this.belief.particles.indexOf(particle)) : [];
    const empty: ReadonlySet<KVAsk> = new Set();
    const estimates: KVEstimate[] = searchActions.map(action => {
      let total = 0;
      for (const particle of particles) {
        total += this.worldActionValue(particle, observation, action, askActions, this.config.depth, empty);
      }
      const value = total / particles.length + 1e-4 * this.basicScore(observation, action);
      return { action, value, samples: particles.length };
    });
    estimates.sort((a, b) => b.value - a.value);
    this.lastEstimates = estimates;
    const best = estimates[0].value;
    const choices = estimates.filter(item => isClose(item.value, best)).map(item => item.action);
    const chosen = this.rng.choice(choices);
    if (this.traceDecisions) {
      this.lastTrace = {
        // Deep copy: the engine hands the agent a live view of the public
        // history, which keeps growing after this decision returns.
        observation: {
          ...observation,
          hand: [...observation.hand],
          cardCounts: [...observation.cardCounts],
          resolved: [...observation.resolved],
          history: observation.history.map(event => ({ ...event })),
          legalAsks: observation.legalAsks.map(ask => ({ ...ask })),
          legalClaimHalfSuits: [...observation.legalClaimHalfSuits],
        },
        particles: this.belief.particles.map(particle => Array.from(particle)),
        candidates: this.lastCandidates,
        rootAsks: askActions,
        determinizations,
        estimates: estimates.map(item => ({ action: item.action, value: item.value })),
        askScores: Object.fromEntries(observation.legalAsks.map(ask => {
          const action: KVAsk = { kind: "ask", card: ask.card, target: ask.target };
          return [`ask:${ask.card}:${ask.target}`, { basic: this.basicScore(observation, action), prob: this.probabilisticScore(observation, action) }];
        })),
        chosen,
      };
    }
    return chosen;
  }

  /** `BasicHeuristicAgent.select_action` with `ProbabilisticAgent`'s candidates. */
  selectAction(observation: KVObservation): KVAction {
    return this.chooseAction(observation, this.candidateActions(observation));
  }

  /**
   * The claim this agent would make for one half-suit when the rules force it
   * to claim (KV's FORCED_CLAIMS phase, where no ask is legal and
   * `candidate_actions` falls through to its best-supported allocation).
   */
  forcedClaim(observation: KVObservation, halfSuit: number): KVClaim {
    this.belief.update(observation);
    const cards = this.rules.halfSuitCards[halfSuit];
    const player = observation.player;
    const tally = new Map<string, { allocation: number[]; count: number }>();
    for (const particle of this.belief.particles) {
      const allocation: number[] = [];
      let teamOwned = true;
      for (const card of cards) {
        const owner = particle[card];
        if (owner === RESOLVED || this.team(owner) !== this.team(player)) { teamOwned = false; break; }
        allocation.push(owner);
      }
      if (!teamOwned) continue;
      const key = allocation.join(",");
      const entry = tally.get(key);
      if (entry) entry.count++;
      else tally.set(key, { allocation, count: 1 });
    }
    let top: { allocation: number[]; count: number } | null = null;
    for (const entry of tally.values()) if (!top || entry.count > top.count) top = entry;
    if (!top) return this.fallbackClaim(observation, halfSuit);
    return { kind: "claim", halfSuit, allocation: top.allocation, support: top.count / this.belief.particles.length };
  }
}
