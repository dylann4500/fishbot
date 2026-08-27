import {
  KVAction, KVDecisionTrace, KVObservation, KVSearchAgent, KVSearchConfig, KVRuleset, KV_SEARCH_DEFAULTS,
} from "@/lib/kv-search-agent";

export type Team = 0 | 1;
export type StrategyId = "fishbot" | "fishbot_v02" | "kv_search" | "lockout" | "hunter" | "diversifier" | "detective" | "bluffer" | "random";

export type Strategy = {
  id: StrategyId;
  name: string;
  short: string;
  description: string;
  icon: string;
  declarationThreshold: number;
  risk: number;
};

export const STRATEGIES: Record<StrategyId, Strategy> = {
  fishbot: { id: "fishbot", name: "FishBot v0.3", short: "Fish v0.3", description: "Count-conditioned belief search with empirically tuned transfer, continuation, control, and reply values.", icon: "ƒ", declarationThreshold: .963, risk: .02 },
  fishbot_v02: { id: "fishbot_v02", name: "FishBot v0.2 (legacy)", short: "Fish v0.2", description: "Original one-ply utility policy retained as a frozen development baseline.", icon: "ƒ", declarationThreshold: .96, risk: .02 },
  kv_search: { id: "kv_search", name: "KV Search (fish-researchp12 v0.1)", short: "KV Search", description: "External engine: KV's sampled-world determinization search over a correlated feasible-world particle belief. Ported from fish/agents.py::SearchAgent at its shipped 96-particle / 48-determinization settings.", icon: "◆", declarationThreshold: .75, risk: .05 },
  lockout: { id: "lockout", name: "Turn-starvation specialist", short: "Lockout", description: "Posterior-greedy asking with an explicit penalty for missing into a dangerous opponent.", icon: "⊘", declarationThreshold: .94, risk: .04 },
  hunter: { id: "hunter", name: "Focused hunter", short: "Focus", description: "Builds momentum in one half-suit and keeps pressing it.", icon: "◎", declarationThreshold: .82, risk: .16 },
  diversifier: { id: "diversifier", name: "Adaptive diversifier", short: "Diversify", description: "Switches half-suits to preserve options and spread information.", icon: "↝", declarationThreshold: .88, risk: .12 },
  detective: { id: "detective", name: "Bayesian detective", short: "Infer", description: "Targets the card-location hypothesis with the best posterior odds.", icon: "⌁", declarationThreshold: .92, risk: .06 },
  bluffer: { id: "bluffer", name: "Misdirection artist", short: "Bluff", description: "Uses deliberate diversions after revealing moments to distort tells.", icon: "◇", declarationThreshold: .76, risk: .28 },
  random: { id: "random", name: "Unpredictable novice", short: "Random", description: "Chooses legal asks with little memory—the experimental control.", icon: "✦", declarationThreshold: .68, risk: .42 },
};

export type PolicyTechnical = {
  class: string;
  objective: string;
  askFormula: string;
  reactsToAsk: string;
  declarationRule: string;
  limitations: string;
};

export const POLICY_TECHNICAL: Record<StrategyId, PolicyTechnical> = {
  fishbot: {
    class: "Tuned count-conditioned belief search",
    objective: "Maximize held-out win performance using only the private hand and public history, balancing immediate transfer, continuation, team control, information, and public reply risk.",
    askFormula: "22·P(hit) + 2.5·progress + 4·team control + .5·target evidence + 4·P(hit)·continuation + 4·P(hit)·completion + .5·repeat − P(miss)·reply risk",
    reactsToAsk: "Conditions card-location beliefs on exclusions, public transfers, current hand counts, and ask history. It never inspects another player's hidden cards when estimating a reply.",
    declarationRule: "Uses separately tuned thresholds for exact allocation and team ownership, with score-aware caution and an endgame escape rule.",
    limitations: "Empirically robust in the tested simulator population, not a proof of equilibrium: beliefs are approximate and conventions, collusion, and expert human adaptations are not modeled.",
  },
  fishbot_v02: {
    class: "Legacy deterministic one-ply search",
    objective: "Maximize the original hand-built expected utility over each legal card–target pair.",
    askFormula: "15.2·P(hit) + 4.5·binary entropy + 3.2·set progress + 1.6·target evidence − 3.8·P(miss)·reply threat",
    reactsToAsk: "Adds public asker/half-suit evidence to an approximate card posterior.",
    declarationRule: "Uses the original dynamic 94–97% combined-confidence threshold.",
    limitations: "Frozen comparison baseline; its original reply-risk feature was not information-safe and is replaced by a public-history estimate in this implementation.",
  },
  kv_search: {
    class: "External engine \u2014 sampled-world determinization search",
    objective: "Maximize the mean value of an ask or claim across sampled feasible worlds, where a hit is worth 1, a miss -0.2, and a retained turn is worth another discounted ask.",
    askFormula: "mean over 48 determinizations of [hit ? 1 + .85\u00b7max(next hit chain) : \u2212.2] + 1e-4\u00b7(2\u00b7cards held in half-suit + 1/target hand size); root asks pruned to the top 12 by 20\u00b7P(hit) + .08\u00b7H(P) + base",
    reactsToAsk: "No explicit reaction term. Public asks enter only as belief constraints: a successful ask fixes the card to the asker, a failed ask removes it from the target, and resolved half-suits leave the deal.",
    declarationRule: "Claims the modal team-consistent allocation among the particles once it holds at least 75% of them, then only if its determinized value (+6 exact, \u22122 misallocated, \u22126 opponent-held) beats every ask.",
    limitations: "Ported to TypeScript from the Python original: formulas, constants and orderings are reproduced, but the random stream differs, so tie-breaks and the particle sample are not bit-identical. The belief also does not infer that an asker lacks the card it asked for \u2014 the original omits that inference too.",
  },
  lockout: {
    class: "External-strategy challenger",
    objective: "Convert likely asks while blackballing opponents whose public card concentration and ask history make control unusually dangerous.",
    askFormula: "21·P(hit) + target evidence + set progress − 8·P(miss)·dangerous-target score",
    reactsToAsk: "Uses public transfers, hand counts, exclusions, and half-suit activity to identify opponents who should not receive the turn.",
    declarationRule: "Declares conservatively above 94% modeled team/allocation confidence.",
    limitations: "Operationalizes blackballing from published human strategy notes, but cannot reproduce tacit team coordination or deeper multi-turn lockout plans.",
  },
  hunter: {
    class: "Weighted heuristic",
    objective: "Finish one selected half-suit before spreading attention elsewhere.",
    askFormula: "9·P(hit) + 1.97·cards held in set + 6.2 if focused set − 1.1 if not + small seeded tie-break",
    reactsToAsk: "After receiving the turn on a miss, adds 1.1 to asks in the same half-suit; public asks also alter ownership weights.",
    declarationRule: "Declare above 82% team/allocation confidence with moderate risk tolerance.",
    limitations: "Can reveal a stable target and overcommit when missing cards are probably held by teammates.",
  },
  diversifier: {
    class: "Weighted heuristic",
    objective: "Preserve several live half-suits and make consecutive intentions harder to read.",
    askFormula: "9·P(hit) + set progress + 2.1 for switching half-suits − 3.6 for repeating the last personal target",
    reactsToAsk: "Uses public asks as ownership evidence but weakly penalizes an immediate same-suit response.",
    declarationRule: "Declare above 88% confidence with low risk tolerance.",
    limitations: "May abandon a nearly complete set and create more public information than it converts.",
  },
  detective: {
    class: "Posterior-greedy heuristic",
    objective: "Ask the card–player pair with the strongest inferred location.",
    askFormula: "21·P(hit) + 0.85·target asks in this half-suit + set progress + seeded tie-break",
    reactsToAsk: "Each ask raises unresolved-card weight for that asker in that half-suit by 0.38, capped at +3.0.",
    declarationRule: "Declare above 92% confidence; this is the most cautious heuristic baseline.",
    limitations: "Greedy inference ignores the strategic value of a miss and only approximates card-count constraints.",
  },
  bluffer: {
    class: "Behavioral heuristic",
    objective: "Trade some card-acquisition value for ambiguity after a revealing interaction.",
    askFormula: "11·P(hit) + set progress + 4.8 for a diversion after being asked − 6.8 for same-suit response + noise",
    reactsToAsk: "When a miss transfers the turn to it, strongly prefers a different half-suit if psychological tells are enabled.",
    declarationRule: "Declare above 76% confidence with high risk tolerance.",
    limitations: "Bundles bluffing with declaration aggression, so ablation is required before attributing outcomes to misdirection.",
  },
  random: {
    class: "Control policy",
    objective: "Provide a low-information baseline by sampling uniformly from legal asks.",
    askFormula: "Uniform random legal card–target pair",
    reactsToAsk: "No intentional reaction; public information only constrains which options remain plausible.",
    declarationRule: "Declare above 68% confidence with very high risk tolerance.",
    limitations: "Not intended as credible play; useful for measuring how much value a policy adds over legality alone.",
  },
};

export type Card = { id: number; set: number; rank: string; suit: string; label: string; compact: string };
export type HalfSuit = { id: number; name: string; short: string; cards: number[]; symbol: string };

const SUITS = [
  ["Spades", "♠"], ["Hearts", "♥"], ["Diamonds", "♦"], ["Clubs", "♣"],
] as const;
const LOW = ["2", "3", "4", "5", "6", "7"];
const HIGH = ["9", "10", "J", "Q", "K", "A"];

export const HALF_SUITS: HalfSuit[] = [];
export const CARDS: Card[] = [];

for (let s = 0; s < 4; s++) {
  for (const band of [0, 1]) {
    const id = HALF_SUITS.length;
    const ranks = band === 0 ? LOW : HIGH;
    const cards: number[] = [];
    for (const rank of ranks) {
      const cardId = CARDS.length;
      cards.push(cardId);
      CARDS.push({ id: cardId, set: id, rank, suit: SUITS[s][0], label: `${rank} of ${SUITS[s][0]}`, compact: `${rank}${SUITS[s][1]}` });
    }
    HALF_SUITS.push({ id, name: `${band === 0 ? "Low" : "High"} ${SUITS[s][0]}`, short: `${band === 0 ? "Low" : "High"} ${SUITS[s][1]}`, cards, symbol: SUITS[s][1] });
  }
}
{
  const id = HALF_SUITS.length;
  const specs = [["8", "Spades", "♠"], ["8", "Hearts", "♥"], ["8", "Diamonds", "♦"], ["8", "Clubs", "♣"], ["Red Joker", "Joker", "RJ"], ["Black Joker", "Joker", "BJ"]];
  const cards = specs.map(([rank, suit, compact]) => {
    const cardId = CARDS.length;
    CARDS.push({ id: cardId, set: id, rank, suit, label: suit === "Joker" ? rank : `${rank} of ${suit}`, compact });
    return cardId;
  });
  HALF_SUITS.push({ id, name: "Eights & Jokers", short: "8s + Jokers", cards, symbol: "★" });
}

export type GameOptions = {
  seed: number;
  strategies: [StrategyId, StrategyId];
  psychologicalTells: boolean;
  declarations: boolean;
  detailed?: boolean;
  maxActions?: number;
  fishbotConfig?: Partial<FishBotConfig>;
  kvSearchConfig?: Partial<KVSearchConfig>;
};

export type FishBotConfig = {
  useCountConditioning: boolean;
  signalStrength: number;
  hitWeight: number;
  informationWeight: number;
  setProgressWeight: number;
  teamControlWeight: number;
  targetEvidenceWeight: number;
  continuationWeight: number;
  completionWeight: number;
  replyThreatWeight: number;
  repeatSetWeight: number;
  declarationThreshold: number;
  trailingDeclarationDelta: number;
  leadingDeclarationDelta: number;
  allocationSlack: number;
};

export const DEFAULT_FISHBOT_CONFIG: FishBotConfig = {
  useCountConditioning: true,
  signalStrength: .453,
  hitWeight: 22,
  informationWeight: 0,
  setProgressWeight: 2.5,
  teamControlWeight: 4,
  targetEvidenceWeight: .5,
  continuationWeight: 4,
  completionWeight: 4,
  replyThreatWeight: 1,
  repeatSetWeight: .5,
  declarationThreshold: .963,
  trailingDeclarationDelta: -.016,
  leadingDeclarationDelta: .005,
  allocationSlack: .008,
};

export type GameAction = {
  index: number;
  type: "ask" | "declare" | "pass" | "adjudicate";
  actor: number;
  target?: number;
  card?: number;
  set?: number;
  success?: boolean;
  team: Team;
  text: string;
  annotation: string;
  score: [number, number];
  cardCounts: number[];
  hands?: number[][];
  confidence?: number;
  pivotalScore?: number;
  pivotalReasons?: string[];
  decision?: DecisionTrace;
};

export type DecisionFeatures = {
  hitProbability: number;
  informationGain: number;
  setProgress: number;
  teamControl: number;
  targetEvidence: number;
  continuationValue: number;
  completionValue: number;
  replyThreat: number;
  expectedUtility: number;
};

export type DecisionTrace = {
  policy: StrategyId;
  policyScore: number;
  fishbotScore: number;
  regret: number;
  reactingToPreviousAsk: boolean;
  features: DecisionFeatures;
  alternatives: { card: number; target: number; score: number }[];
};

export type GameSummary = {
  seed: number;
  strategies: [StrategyId, StrategyId];
  psychologicalTells: boolean;
  declarationsEnabled: boolean;
  winner: Team;
  score: [number, number];
  asks: number;
  successfulAsks: number;
  declarations: number;
  failedDeclarations: number;
  diversions: number;
  leadChanges: number;
  maxComeback: number;
  actions: number;
  interest: number;
  tag: string;
  pivotalCount: number;
  peakPivotal: number;
  teamAsks: [number, number];
  teamHits: [number, number];
  teamDeclarations: [number, number];
  teamCorrectDeclarations: [number, number];
  informationGain: [number, number];
  decisionRegret: [number, number];
  reactionAsks: number;
  reactionHits: number;
  teamReactionAsks: [number, number];
  teamReactionHits: [number, number];
  teamDiversions: [number, number];
  replyThreatOnMiss: [number, number];
  continuationValue: [number, number];
  completionValue: [number, number];
  maxHitStreak: number;
  firstDeclarationAction: number;
  initialHands?: number[][];
  log?: GameAction[];
};

export type BatchResult = {
  games: number;
  seed: number;
  psychologicalTells: boolean;
  declarationsEnabled: boolean;
  strategies: [StrategyId, StrategyId];
  teamAWins: number;
  teamBWins: number;
  winRateA: number;
  avgScore: [number, number];
  avgActions: number;
  askAccuracy: number;
  declarationAccuracy: number;
  diversionRate: number;
  avgLeadChanges: number;
  winRateCI: [number, number];
  teamAskAccuracy: [number, number];
  teamDeclarationAccuracy: [number, number];
  avgInformationGain: [number, number];
  avgDecisionRegret: [number, number];
  reactionAccuracy: number;
  teamReactionAccuracy: [number, number];
  teamDiversionRate: [number, number];
  avgReplyThreatOnMiss: [number, number];
  avgContinuationValue: [number, number];
  avgCompletionValue: [number, number];
  avgPivotalMoments: number;
  avgMaxHitStreak: number;
  medianActions: number;
  p90Actions: number;
  scoreDistribution: number[];
  outliers: GameSummary[];
  elapsedMs: number;
};

type Memory = { knownOwner: Int8Array; excluded: Uint8Array[]; signals: number[][] };
type State = {
  rng: RNG;
  opts: GameOptions;
  hands: Set<number>[];
  memories: Memory[];
  activeSets: boolean[];
  score: [number, number];
  turn: number;
  log: GameAction[];
  asks: number;
  successes: number;
  declarations: number;
  failedDeclarations: number;
  diversions: number;
  leadChanges: number;
  maxDeficit: [number, number];
  lastLead: number;
  focus: (number | null)[];
  lastAsk: { actor: number; target: number; set: number; success: boolean } | null;
  eventCount: number;
  pivotalCount: number;
  peakPivotal: number;
  teamAsks: [number, number];
  teamHits: [number, number];
  teamDeclarations: [number, number];
  teamCorrectDeclarations: [number, number];
  informationGain: [number, number];
  decisionRegret: [number, number];
  reactionAsks: number;
  reactionHits: number;
  teamReactionAsks: [number, number];
  teamReactionHits: [number, number];
  teamDiversions: [number, number];
  replyThreatOnMiss: [number, number];
  continuationValue: [number, number];
  completionValue: [number, number];
  hitStreak: number;
  maxHitStreak: number;
  streakActor: number;
  firstDeclarationAction: number;
  beliefVersion: number;
  beliefCache: ({ version: number; signalStrength: number; beliefs: number[][] } | null)[];
  publicHistory: { actor: number; target: number; card: number; success: boolean }[];
  kvAgents: (KVSearchAgent | null)[];
};

const TEAM: Team[] = [0, 1, 0, 1, 0, 1];
export const PLAYER_NAMES = ["Mara", "Theo", "Iris", "Jonah", "Sage", "Nico"];
export const PIVOTAL_THRESHOLD = 4.5;

class RNG {
  private x: number;
  constructor(seed: number) { this.x = seed >>> 0 || 0x9e3779b9; }
  next() { let x = this.x; x ^= x << 13; x ^= x >>> 17; x ^= x << 5; this.x = x >>> 0; return this.x / 4294967296; }
  int(n: number) { return Math.floor(this.next() * n); }
  shuffle<T>(a: T[]) { for (let i = a.length - 1; i > 0; i--) { const j = this.int(i + 1); [a[i], a[j]] = [a[j], a[i]]; } return a; }
}

function makeMemory(player: number, hands: Set<number>[]): Memory {
  const knownOwner = new Int8Array(54).fill(-1);
  const excluded = Array.from({ length: 54 }, () => new Uint8Array(6));
  for (let c = 0; c < 54; c++) {
    if (hands[player].has(c)) knownOwner[c] = player;
    else excluded[c][player] = 1;
  }
  return { knownOwner, excluded, signals: Array.from({ length: 6 }, () => Array(9).fill(0)) };
}

function ownerProbability(memory: Memory, observer: number, card: number, owner: number): number {
  if (memory.knownOwner[card] >= 0) return memory.knownOwner[card] === owner ? 1 : 0;
  if (memory.excluded[card][owner]) return 0;
  const set = CARDS[card].set;
  let total = 0;
  let mine = 0;
  for (let p = 0; p < 6; p++) {
    if (memory.excluded[card][p]) continue;
    const weight = 1 + Math.min(3, memory.signals[p][set] * .38);
    total += weight;
    if (p === owner) mine = weight;
  }
  return total ? mine / total : (observer === owner ? 0 : .2);
}

type AskOption = { card: number; target: number; set: number; probability: number };
type ScoredAsk = AskOption & { decision: DecisionTrace };

function binaryEntropy(p: number) {
  if (p <= 0 || p >= 1) return 0;
  return -p * Math.log2(p) - (1 - p) * Math.log2(1 - p);
}

function record(state: State, action: Omit<GameAction, "index" | "score" | "cardCounts" | "hands">) {
  state.eventCount++;
  if ((action.pivotalScore ?? 0) >= PIVOTAL_THRESHOLD) state.pivotalCount++;
  state.peakPivotal = Math.max(state.peakPivotal, action.pivotalScore ?? 0);
  if (!state.opts.detailed) return;
  state.log.push({
    ...action,
    index: state.log.length,
    score: [...state.score],
    cardCounts: state.hands.map(h => h.size),
    hands: state.hands.map(h => [...h].sort((a, b) => a - b)),
  });
}

function legalCandidates(state: State, actor: number, probabilities?: number[][]): AskOption[] {
  const memory = state.memories[actor];
  const candidates: { card: number; target: number; set: number; probability: number }[] = [];
  const heldSets = new Set([...state.hands[actor]].map(c => CARDS[c].set).filter(s => state.activeSets[s]));
  for (const set of heldSets) {
    for (const card of HALF_SUITS[set].cards) {
      if (state.hands[actor].has(card)) continue;
      for (let target = 0; target < 6; target++) {
        if (TEAM[target] === TEAM[actor] || state.hands[target].size === 0) continue;
        const probability = probabilities?.[card]?.[target] ?? ownerProbability(memory, actor, card, target);
        // A known miss is still a legal (and sometimes strategically necessary)
        // ask. Keeping it in the action space prevents knowledge from creating
        // an artificial deadlock when every missing card appears to be friendly.
        candidates.push({ card, target, set, probability });
      }
    }
  }
  return candidates;
}

function resolvedFishbotConfig(state: State): FishBotConfig {
  return { ...DEFAULT_FISHBOT_CONFIG, ...state.opts.fishbotConfig };
}

/**
 * Approximate the posterior over unresolved card owners while enforcing the
 * publicly known hand counts. Alternating row/column scaling is a compact
 * maximum-entropy approximation to enumerating every deal consistent with the
 * observer's exclusions. Crucially, it never reads another player's hand to
 * decide which owners are plausible; hand sizes are public consequences of the
 * deal, transfers, and declarations.
 */
function conditionedBeliefs(state: State, observer: number, signalStrength: number): number[][] {
  const cached = state.beliefCache[observer];
  if (cached && cached.version === state.beliefVersion && cached.signalStrength === signalStrength) return cached.beliefs;
  const memory = state.memories[observer];
  const beliefs = Array.from({ length: 54 }, () => Array(6).fill(0));
  const knownCounts = Array(6).fill(0);
  const unresolved: number[] = [];

  for (let card = 0; card < 54; card++) {
    if (!state.activeSets[CARDS[card].set]) continue;
    const known = memory.knownOwner[card];
    if (known >= 0) {
      beliefs[card][known] = 1;
      knownCounts[known]++;
    } else {
      unresolved.push(card);
      for (let owner = 0; owner < 6; owner++) {
        if (memory.excluded[card][owner]) continue;
        beliefs[card][owner] = Math.exp(Math.min(2.4, memory.signals[owner][CARDS[card].set] * signalStrength));
      }
    }
  }

  const capacities = state.hands.map((hand, player) => Math.max(0, hand.size - knownCounts[player]));
  for (let iteration = 0; iteration < 12; iteration++) {
    for (const card of unresolved) {
      let total = beliefs[card].reduce((sum, value) => sum + value, 0);
      if (!total) {
        for (let owner = 0; owner < 6; owner++) {
          if (!memory.excluded[card][owner] && capacities[owner] > 0) beliefs[card][owner] = 1;
        }
        total = beliefs[card].reduce((sum, value) => sum + value, 0);
      }
      if (total) for (let owner = 0; owner < 6; owner++) beliefs[card][owner] /= total;
    }
    const columnTotals = Array(6).fill(0);
    for (const card of unresolved) for (let owner = 0; owner < 6; owner++) columnTotals[owner] += beliefs[card][owner];
    for (let owner = 0; owner < 6; owner++) {
      const scale = columnTotals[owner] ? capacities[owner] / columnTotals[owner] : 0;
      for (const card of unresolved) beliefs[card][owner] *= scale;
    }
  }
  for (const card of unresolved) {
    const total = beliefs[card].reduce((sum, value) => sum + value, 0);
    if (total) for (let owner = 0; owner < 6; owner++) beliefs[card][owner] /= total;
  }
  state.beliefCache[observer] = { version: state.beliefVersion, signalStrength, beliefs };
  return beliefs;
}

function independentBeliefs(state: State, observer: number): number[][] {
  const memory = state.memories[observer];
  return Array.from({ length: 54 }, (_, card) => Array.from(
    { length: 6 },
    (_, owner) => ownerProbability(memory, observer, card, owner),
  ));
}

function publicReplyThreat(state: State, actor: number, target: number, askedSet: number, beliefs: number[][]) {
  const memory = state.memories[actor];
  let best = 0;
  for (let set = 0; set < 9; set++) {
    if (!state.activeSets[set]) continue;
    let noneProbability = 1;
    let expectedTargetCards = 0;
    let expectedTargetTeamCards = 0;
    let expectedFriendlyCards = 0;
    for (const card of HALF_SUITS[set].cards) {
      const targetProbability = beliefs[card]?.[target] ?? ownerProbability(memory, actor, card, target);
      expectedTargetCards += targetProbability;
      noneProbability *= 1 - targetProbability;
      for (let player = 0; player < 6; player++) {
        const probability = beliefs[card]?.[player] ?? ownerProbability(memory, actor, card, player);
        if (TEAM[player] === TEAM[target]) expectedTargetTeamCards += probability;
        else expectedFriendlyCards += probability;
      }
    }
    const targetHasSet = 1 - noneProbability;
    const publicActivity = Math.min(1, memory.signals[target][set] / 3);
    const concentration = Math.min(1, expectedTargetCards / 4);
    const opponentControl = expectedTargetTeamCards / 6;
    const friendlyExposure = expectedFriendlyCards / 6;
    const newTell = set === askedSet ? .12 : 0;
    const danger = targetHasSet * friendlyExposure
      * Math.min(1, .15 + .35 * concentration + .25 * publicActivity + .25 * opponentControl + newTell);
    best = Math.max(best, danger);
  }
  return best;
}

function legacyFishbotFeatures(state: State, actor: number, option: AskOption, replyThreat: number): DecisionFeatures {
  const memory = state.memories[actor];
  let held = 0;
  let knownFriendly = 0;
  for (const card of HALF_SUITS[option.set].cards) {
    if (state.hands[actor].has(card)) held++;
    const known = memory.knownOwner[card];
    if (known >= 0 && TEAM[known] === TEAM[actor]) knownFriendly++;
  }
  const hitProbability = option.probability;
  const informationGain = binaryEntropy(hitProbability);
  const setProgress = Math.min(1, (held + knownFriendly * .45) / 6);
  const teamControl = setProgress;
  const targetEvidence = Math.min(1, memory.signals[option.target][option.set] / 4);
  const continuationValue = 0;
  const completionValue = held >= 4 ? 1 : held === 3 ? .35 : 0;
  const expectedUtility = 13 * hitProbability
    + 4.5 * informationGain
    + 3.2 * setProgress
    + 1.6 * targetEvidence
    + 2.2 * hitProbability
    - 3.8 * (1 - hitProbability) * replyThreat;
  return { hitProbability, informationGain, setProgress, teamControl, targetEvidence, continuationValue, completionValue, replyThreat, expectedUtility };
}

function optimizedFishbotFeatures(state: State, actor: number, option: AskOption, beliefs: number[][], config: FishBotConfig): DecisionFeatures {
  const memory = state.memories[actor];
  let held = 0;
  let expectedTeamCards = 0;
  let continuationValue = 0;
  for (const card of HALF_SUITS[option.set].cards) {
    if (state.hands[actor].has(card)) held++;
    for (let player = 0; player < 6; player++) if (TEAM[player] === TEAM[actor]) expectedTeamCards += beliefs[card][player];
    if (card === option.card || state.hands[actor].has(card)) continue;
    for (let target = 0; target < 6; target++) {
      if (TEAM[target] !== TEAM[actor]) continuationValue = Math.max(continuationValue, beliefs[card][target]);
    }
  }
  const hitProbability = option.probability;
  const informationGain = binaryEntropy(hitProbability);
  const setProgress = held / 6;
  const teamControl = expectedTeamCards / 6;
  const targetEvidence = Math.min(1, memory.signals[option.target][option.set] / 4);
  const completionValue = held >= 4 ? 1 : held === 3 ? .35 : 0;
  const replyThreat = publicReplyThreat(state, actor, option.target, option.set, beliefs);
  const repeatsSet = state.lastAsk?.actor === actor && state.lastAsk.set === option.set ? 1 : 0;
  const expectedUtility = config.hitWeight * hitProbability
    + config.informationWeight * informationGain
    + config.setProgressWeight * setProgress
    + config.teamControlWeight * teamControl
    + config.targetEvidenceWeight * targetEvidence
    + config.continuationWeight * hitProbability * continuationValue
    + config.completionWeight * hitProbability * completionValue
    + config.repeatSetWeight * repeatsSet
    - config.replyThreatWeight * (1 - hitProbability) * replyThreat;
  return { hitProbability, informationGain, setProgress, teamControl, targetEvidence, continuationValue, completionValue, replyThreat, expectedUtility };
}

function chooseAsk(state: State, actor: number): ScoredAsk | null {
  const strategy = STRATEGIES[state.opts.strategies[TEAM[actor]]];
  const config = resolvedFishbotConfig(state);
  const beliefs = config.useCountConditioning
    ? conditionedBeliefs(state, actor, config.signalStrength)
    : independentBeliefs(state, actor);
  const legacyBeliefs = strategy.id === "fishbot_v02"
    ? conditionedBeliefs(state, actor, DEFAULT_FISHBOT_CONFIG.signalStrength)
    : beliefs;
  const challengerBeliefs = strategy.id === "lockout"
    ? conditionedBeliefs(state, actor, DEFAULT_FISHBOT_CONFIG.signalStrength)
    : beliefs;
  const legal = legalCandidates(state, actor, strategy.id === "fishbot" ? beliefs : undefined);
  if (!legal.length) return null;
  const heldBySet = Array(9).fill(0);
  const teamKnown = Array(9).fill(0);
  for (const c of state.hands[actor]) heldBySet[CARDS[c].set]++;
  for (let c = 0; c < 54; c++) {
    const owner = state.memories[actor].knownOwner[c];
    if (owner >= 0 && TEAM[owner] === TEAM[actor]) teamKnown[CARDS[c].set]++;
  }
  if (state.focus[actor] === null || !state.activeSets[state.focus[actor]!] || heldBySet[state.focus[actor]!] === 0) {
    state.focus[actor] = [...new Set(legal.map(x => x.set))].sort((a, b) => heldBySet[b] - heldBySet[a])[0];
  }
  const responding = state.lastAsk && state.lastAsk.target === actor && !state.lastAsk.success;
  const evaluate = (option: AskOption) => optimizedFishbotFeatures(state, actor, {
    ...option,
    probability: beliefs[option.card][option.target],
  }, beliefs, config);
  const evaluateLegacy = (option: AskOption) => legacyFishbotFeatures(
    state,
    actor,
    option,
    publicReplyThreat(state, actor, option.target, option.set, legacyBeliefs),
  );
  const fishRanked = legal.map(option => ({ option, features: evaluate(option) }))
    .sort((a, b) => b.features.expectedUtility - a.features.expectedUtility || a.option.card - b.option.card || a.option.target - b.option.target);
  let best = strategy.id === "random" ? legal[state.rng.int(legal.length)] : legal[0];
  let bestScore = -Infinity;
  for (const option of legal) {
    const features = evaluate(option);
    let score = option.probability * 9 + heldBySet[option.set] * .72 + teamKnown[option.set] * .25 + (strategy.id === "fishbot" || strategy.id === "fishbot_v02" ? 0 : state.rng.next() * .7);
    if (strategy.id === "fishbot") score = features.expectedUtility;
    if (strategy.id === "fishbot_v02") score = evaluateLegacy(option).expectedUtility;
    if (strategy.id === "hunter") {
      score += option.set === state.focus[actor] ? 6.2 : -1.1;
      score += heldBySet[option.set] * 1.25;
    }
    if (strategy.id === "detective") {
      score += option.probability * 12;
      score += state.memories[actor].signals[option.target][option.set] * .85;
    }
    if (strategy.id === "lockout") {
      score += option.probability * 12;
      score += state.memories[actor].signals[option.target][option.set] * .85;
      score -= 8 * (1 - option.probability) * publicReplyThreat(state, actor, option.target, option.set, challengerBeliefs);
    }
    if (strategy.id === "diversifier") {
      const recentSame = state.lastAsk?.actor === actor && state.lastAsk.set === option.set;
      score += recentSame ? -3.6 : 2.1;
      score += (6 - heldBySet[option.set]) * .25;
    }
    if (strategy.id === "bluffer") {
      if (responding && state.opts.psychologicalTells) score += option.set === state.lastAsk!.set ? -6.8 : 4.8;
      score += option.probability * 2 + state.rng.next() * 2.8;
    }
    if (responding && state.opts.psychologicalTells && strategy.id !== "bluffer" && strategy.id !== "fishbot" && strategy.id !== "fishbot_v02") {
      score += option.set === state.lastAsk!.set ? 1.1 : -.25;
    }
    if (strategy.id !== "random" && score > bestScore) { bestScore = score; best = option; }
  }
  if (strategy.id === "random") bestScore = 0;
  const selectedFish = evaluate(best);
  const topFish = fishRanked[0].features.expectedUtility;
  return {
    ...best,
    decision: {
      policy: strategy.id,
      policyScore: bestScore,
      fishbotScore: selectedFish.expectedUtility,
      regret: Math.max(0, topFish - selectedFish.expectedUtility),
      reactingToPreviousAsk: Boolean(responding),
      features: selectedFish,
      alternatives: fishRanked.slice(0, 3).map(item => ({ card: item.option.card, target: item.option.target, score: item.features.expectedUtility })),
    },
  };
}

/**
 * The bridge to KV's external engine (`lib/kv-search-agent.ts`).
 *
 * FishLab and KV's `fish` package model the same game — six seats, alternating
 * teams, nine six-card half-suits including eights-and-jokers, ask legality
 * requiring a card of the asked half-suit, claims on your own turn only — so
 * the agent is driven by a translated observation rather than a re-implemented
 * rule set. Card and half-suit numbering differ between the two projects; the
 * agent is instantiated in FishLab's numbering, which changes only the order in
 * which exactly-tied actions are enumerated.
 */
const KV_RULESET: KVRuleset = {
  deckSize: 54,
  numPlayers: 6,
  halfSuitOf: Int8Array.from(CARDS.map(card => card.set)),
  halfSuitCards: HALF_SUITS.map(half => [...half.cards]),
};

export function kvAgentSeed(seed: number, seat: number) {
  // runner.play_game seeds each seat with `seed * 1_000_003 + seat * 97`,
  // reduced here to 32 bits because JavaScript integers are not 63-bit.
  return (Math.imul(seed >>> 0, 1000003) + seat * 97) >>> 0;
}

function kvObservation(state: State, actor: number, claimsAllowed: boolean): KVObservation {
  const legalAsks: { card: number; target: number }[] = [];
  for (let set = 0; set < 9; set++) {
    if (!state.activeSets[set]) continue;
    let holds = false;
    for (const card of HALF_SUITS[set].cards) if (state.hands[actor].has(card)) { holds = true; break; }
    if (!holds) continue;
    for (const card of HALF_SUITS[set].cards) {
      if (state.hands[actor].has(card)) continue;
      for (let target = 0; target < 6; target++) {
        if (TEAM[target] === TEAM[actor] || state.hands[target].size === 0) continue;
        legalAsks.push({ card, target });
      }
    }
  }
  const legalClaimHalfSuits: number[] = [];
  if (claimsAllowed && state.hands[actor].size > 0) {
    for (let set = 0; set < 9; set++) if (state.activeSets[set]) legalClaimHalfSuits.push(set);
  }
  return {
    player: actor,
    hand: [...state.hands[actor]].sort((a, b) => a - b),
    cardCounts: state.hands.map(hand => hand.size),
    resolved: state.activeSets.map(active => !active),
    history: state.publicHistory,
    legalAsks,
    legalClaimHalfSuits,
    ply: state.eventCount,
  };
}

/**
 * Build the decision trace the replay and the batch metrics expect for an ask
 * that an external policy chose. The feature frame is FishLab's, exactly as it
 * is for every other non-FishBot policy; only `policyScore` is the external
 * policy's own number.
 */
function traceForOption(state: State, actor: number, chosen: { card: number; target: number; set: number }, policy: StrategyId, policyScore: number): ScoredAsk {
  const config = resolvedFishbotConfig(state);
  const beliefs = config.useCountConditioning
    ? conditionedBeliefs(state, actor, config.signalStrength)
    : independentBeliefs(state, actor);
  const evaluate = (option: AskOption) => optimizedFishbotFeatures(state, actor, { ...option, probability: beliefs[option.card][option.target] }, beliefs, config);
  const fishRanked = legalCandidates(state, actor, beliefs).map(option => ({ option, features: evaluate(option) }))
    .sort((a, b) => b.features.expectedUtility - a.features.expectedUtility || a.option.card - b.option.card || a.option.target - b.option.target);
  const option: AskOption = { ...chosen, probability: beliefs[chosen.card][chosen.target] };
  const features = evaluate(option);
  const topFish = fishRanked.length ? fishRanked[0].features.expectedUtility : features.expectedUtility;
  return {
    ...option,
    decision: {
      policy,
      policyScore,
      fishbotScore: features.expectedUtility,
      regret: Math.max(0, topFish - features.expectedUtility),
      reactingToPreviousAsk: Boolean(state.lastAsk && state.lastAsk.target === actor && !state.lastAsk.success),
      features,
      alternatives: fishRanked.slice(0, 3).map(item => ({ card: item.option.card, target: item.option.target, score: item.features.expectedUtility })),
    },
  };
}

type KVTurn = { kind: "claim"; set: number; owners: number[]; confidence: number } | { kind: "ask"; ask: ScoredAsk } | null;

let kvTraceSink: ((trace: KVDecisionTrace) => void) | null = null;

/** Opt-in decision tracing, used by `scripts/kv-parity-dump.ts`. Off by default. */
export function setKvTraceSink(sink: ((trace: KVDecisionTrace) => void) | null) { kvTraceSink = sink; }

function kvTakeTurn(state: State, actor: number, agent: KVSearchAgent): KVTurn {
  agent.traceDecisions = Boolean(kvTraceSink);
  const observation = kvObservation(state, actor, state.opts.declarations);
  let action: KVAction;
  try {
    const candidates = agent.candidateActions(observation);
    if (!candidates.length) return null;
    action = agent.chooseAction(observation, candidates);
  } catch {
    // A belief that cannot be reconciled with the public facts is a bug, not a
    // legal position; fall back to FishLab's own policy rather than crash a run.
    const fallback = chooseAsk(state, actor);
    return fallback ? { kind: "ask", ask: fallback } : null;
  }
  if (kvTraceSink && agent.lastTrace) kvTraceSink(agent.lastTrace);
  if (action.kind === "claim") {
    return { kind: "claim", set: action.halfSuit, owners: action.allocation, confidence: action.support };
  }
  const value = agent.lastEstimates.length ? agent.lastEstimates[0].value : 0;
  return { kind: "ask", ask: traceForOption(state, actor, { card: action.card, target: action.target, set: CARDS[action.card].set }, "kv_search", value) };
}

function predictionForSet(state: State, actor: number, set: number, probabilities?: number[][]) {
  const memory = state.memories[actor];
  const team = TEAM[actor];
  const teammates = [0, 1, 2, 3, 4, 5].filter(p => TEAM[p] === team);
  const owners: number[] = [];
  let teamConfidence = 1;
  let allocationConfidence = 1;
  for (const card of HALF_SUITS[set].cards) {
    const probs = teammates.map(p => [p, probabilities?.[card]?.[p] ?? ownerProbability(memory, actor, card, p)] as const).sort((a, b) => b[1] - a[1]);
    const teamProb = probs.reduce((sum, x) => sum + x[1], 0);
    owners.push(probs[0][0]);
    teamConfidence *= Math.max(.001, teamProb);
    allocationConfidence *= Math.max(.001, probs[0][1]);
  }
  const confidence = Math.pow(teamConfidence * allocationConfidence, 1 / 12);
  return { owners, confidence, teamConfidence: Math.pow(teamConfidence, 1 / 6) };
}

function bestDeclaration(state: State, actor: number, forced = false) {
  const strategy = STRATEGIES[state.opts.strategies[TEAM[actor]]];
  const config = resolvedFishbotConfig(state);
  const beliefs = strategy.id === "fishbot"
    ? config.useCountConditioning ? conditionedBeliefs(state, actor, config.signalStrength) : independentBeliefs(state, actor)
    : undefined;
  const informationExhausted = (strategy.id === "fishbot" || strategy.id === "fishbot_v02")
    && legalCandidates(state, actor, beliefs).every(option => option.probability <= .001);
  let best: { set: number; owners: number[]; confidence: number; teamConfidence: number } | null = null;
  for (let set = 0; set < 9; set++) {
    if (!state.activeSets[set]) continue;
    const pred = predictionForSet(state, actor, set, beliefs);
    const lead = state.score[TEAM[actor]] - state.score[(1 - TEAM[actor]) as Team];
    const optimizedThreshold = config.declarationThreshold
      + (lead >= 2 ? config.leadingDeclarationDelta : lead <= -2 ? config.trailingDeclarationDelta : 0);
    const legacyThreshold = lead >= 2 ? .97 : lead <= -2 ? .94 : .96;
    const threshold = forced ? .38
      : strategy.id === "fishbot"
        ? state.eventCount >= 280 || informationExhausted ? .78 : optimizedThreshold
        : state.eventCount >= 280 ? .5
          : informationExhausted ? .72
            : strategy.id === "fishbot_v02" ? legacyThreshold : strategy.declarationThreshold;
    const allocationSlack = strategy.id === "fishbot" ? config.allocationSlack : strategy.risk * .22;
    if (pred.teamConfidence < threshold || pred.confidence < threshold - allocationSlack) continue;
    if (!best || pred.confidence > best.confidence) best = { set, ...pred };
  }
  return best;
}

function revealTransfer(state: State, card: number, owner: number) {
  for (const memory of state.memories) {
    memory.knownOwner[card] = owner;
    for (let p = 0; p < 6; p++) memory.excluded[card][p] = p === owner ? 0 : 1;
  }
}

function declareSet(state: State, actor: number, declaration: NonNullable<ReturnType<typeof bestDeclaration>>, forced = false) {
  const team = TEAM[actor];
  const cards = HALF_SUITS[declaration.set].cards;
  const correct = cards.every((card, i) => state.hands[declaration.owners[i]].has(card) && TEAM[declaration.owners[i]] === team);
  const awarded: Team = correct ? team : (1 - team) as Team;
  state.declarations++;
  state.teamDeclarations[team]++;
  if (correct) state.teamCorrectDeclarations[team]++;
  if (state.firstDeclarationAction < 0) state.firstDeclarationAction = state.eventCount;
  if (!correct) state.failedDeclarations++;
  state.score[awarded]++;
  const beforeLead = state.lastLead;
  const diff = state.score[0] - state.score[1];
  const lead = Math.sign(diff);
  const changedLead = Boolean(beforeLead && lead && beforeLead !== lead);
  if (changedLead) state.leadChanges++;
  if (lead) state.lastLead = lead;
  state.maxDeficit[0] = Math.max(state.maxDeficit[0], state.score[1] - state.score[0]);
  state.maxDeficit[1] = Math.max(state.maxDeficit[1], state.score[0] - state.score[1]);
  state.activeSets[declaration.set] = false;
  for (const card of cards) for (const hand of state.hands) hand.delete(card);
  state.beliefVersion++;
  const assignments = declaration.owners.map((p, i) => `${CARDS[cards[i]].compact}→${PLAYER_NAMES[p]}`).join(", ");
  const pivotalReasons = [
    !correct ? "incorrect allocation awards the set to the opponent" : "set changes the score",
    ...(changedLead ? ["lead changes teams"] : []),
    ...(declaration.confidence < .85 ? ["high-risk confidence threshold"] : []),
  ];
  record(state, {
    type: "declare", actor, set: declaration.set, team,
    success: correct, confidence: declaration.confidence,
    text: `${PLAYER_NAMES[actor]} ${correct ? "wins" : "misdeclares"} ${HALF_SUITS[declaration.set].name}`,
    annotation: `${forced ? "Forced endgame · " : ""}${Math.round(declaration.confidence * 100)}% belief confidence · ${assignments}`,
    pivotalScore: !correct ? 10 : 2 + (changedLead ? 3 : 0) + (declaration.confidence < .85 ? 2 : 0),
    pivotalReasons,
  });
}

function nextLiveTeammate(state: State, player: number) {
  const candidates = [0, 1, 2, 3, 4, 5].filter(p => TEAM[p] === TEAM[player] && state.hands[p].size > 0);
  if (!candidates.length) return null;
  return candidates.sort((a, b) => state.hands[b].size - state.hands[a].size)[0];
}

function classify(summary: Omit<GameSummary, "tag">) {
  if (summary.failedDeclarations >= 2) return "Declaration disaster";
  if (summary.maxComeback >= 3) return "Major comeback";
  if (summary.leadChanges >= 3) return "Lead-change thriller";
  if (summary.diversions >= 8) return "Misdirection duel";
  if (summary.actions >= 180) return "Marathon endgame";
  if (summary.score[summary.winner] >= 8) return "Strategic rout";
  return "Tactical finish";
}

export function simulateGame(options: GameOptions): GameSummary {
  const rng = new RNG(options.seed);
  const deck = rng.shuffle(Array.from({ length: 54 }, (_, i) => i));
  const hands = Array.from({ length: 6 }, () => new Set<number>());
  deck.forEach((card, i) => hands[i % 6].add(card));
  const initialHands = hands.map(hand => [...hand].sort((a, b) => a - b));
  const dealer = rng.int(6);
  const state: State = {
    rng, opts: options, hands, memories: [], activeSets: Array(9).fill(true), score: [0, 0], turn: (dealer + 1) % 6,
    log: [], asks: 0, successes: 0, declarations: 0, failedDeclarations: 0, diversions: 0, leadChanges: 0,
    maxDeficit: [0, 0], lastLead: 0, focus: Array(6).fill(null), lastAsk: null,
    eventCount: 0, pivotalCount: 0, peakPivotal: 0, teamAsks: [0, 0], teamHits: [0, 0],
    teamDeclarations: [0, 0], teamCorrectDeclarations: [0, 0], informationGain: [0, 0], decisionRegret: [0, 0],
    reactionAsks: 0, reactionHits: 0, teamReactionAsks: [0, 0], teamReactionHits: [0, 0], teamDiversions: [0, 0],
    replyThreatOnMiss: [0, 0], continuationValue: [0, 0], completionValue: [0, 0],
    hitStreak: 0, maxHitStreak: 0, streakActor: -1, firstDeclarationAction: -1,
    beliefVersion: 0, beliefCache: Array(6).fill(null),
    publicHistory: [], kvAgents: Array(6).fill(null),
  };
  for (let player = 0; player < 6; player++) {
    if (STRATEGIES[options.strategies[TEAM[player]]].id !== "kv_search") continue;
    state.kvAgents[player] = new KVSearchAgent(KV_RULESET, kvAgentSeed(options.seed, player), options.kvSearchConfig);
  }
  state.memories = Array.from({ length: 6 }, (_, p) => makeMemory(p, hands));
  const maxActions = options.maxActions ?? 360;
  let loops = 0;
  while (state.activeSets.some(Boolean) && loops++ < maxActions) {
    const team = TEAM[state.turn];
    const opponentHasCards = state.hands.some((h, p) => TEAM[p] !== team && h.size > 0);
    const ownTeamHasCards = state.hands.some((h, p) => TEAM[p] === team && h.size > 0);

    if (!ownTeamHasCards || !opponentHasCards) {
      const declaringTeam: Team = ownTeamHasCards ? team : (1 - team) as Team;
      const possible = [0, 1, 2, 3, 4, 5].filter(p => TEAM[p] === declaringTeam);
      // KV's engine hands the forced-claims role to one publicly chosen player
      // who then makes every remaining claim, and keeps that role even after it
      // runs out of cards. Choose it publicly, by hand size.
      const kvClaimer = state.kvAgents[possible[0]]
        ? [...possible].sort((a, b) => state.hands[b].size - state.hands[a].size || a - b)[0]
        : -1;
      for (const set of state.activeSets.map((active, s) => active ? s : -1).filter(s => s >= 0)) {
        const kvAgent = kvClaimer >= 0 ? state.kvAgents[kvClaimer] : null;
        if (kvAgent) {
          const claim = kvAgent.forcedClaim(kvObservation(state, kvClaimer, true), set);
          declareSet(state, kvClaimer, { set, owners: claim.allocation, confidence: claim.support, teamConfidence: claim.support }, true);
          continue;
        }
        const choices = possible.map(p => ({ p, d: predictionForSet(state, p, set) })).sort((a, b) => b.d.confidence - a.d.confidence);
        declareSet(state, choices[0].p, { set, ...choices[0].d }, true);
      }
      break;
    }

    if (state.hands[state.turn].size === 0) {
      const receiver = nextLiveTeammate(state, state.turn);
      if (receiver === null) continue;
      record(state, { type: "pass", actor: state.turn, target: receiver, team, text: `${PLAYER_NAMES[state.turn]} passes the turn to ${PLAYER_NAMES[receiver]}`, annotation: "Player has no cards remaining" });
      state.turn = receiver;
      continue;
    }

    const kvAgent = state.kvAgents[state.turn];
    let kvAsk: ScoredAsk | null = null;
    if (kvAgent) {
      // KV's policy chooses between asking and claiming in a single search, so
      // it replaces both the declaration pass and the ask selection.
      const turn = kvTakeTurn(state, state.turn, kvAgent);
      if (turn && turn.kind === "claim") {
        declareSet(state, state.turn, { set: turn.set, owners: turn.owners, confidence: turn.confidence, teamConfidence: turn.confidence });
        if (!state.activeSets.some(Boolean)) break;
        continue;
      }
      kvAsk = turn && turn.kind === "ask" ? turn.ask : null;
    } else if (options.declarations) {
      let declaration = bestDeclaration(state, state.turn);
      let guard = 0;
      while (declaration && guard++ < 9) {
        declareSet(state, state.turn, declaration);
        if (!state.activeSets.some(Boolean)) break;
        declaration = bestDeclaration(state, state.turn);
      }
      if (!state.activeSets.some(Boolean)) break;
      if (state.hands[state.turn].size === 0) continue;
    }

    const ask = kvAgent ? kvAsk : chooseAsk(state, state.turn);
    if (!ask) {
      const receiver = nextLiveTeammate(state, state.turn);
      if (receiver !== null && receiver !== state.turn) {
        record(state, { type: "pass", actor: state.turn, target: receiver, team, text: `${PLAYER_NAMES[state.turn]} passes to ${PLAYER_NAMES[receiver]}`, annotation: "No informative legal ask remains" });
        state.turn = receiver;
        continue;
      }
      break;
    }

    const actor = state.turn;
    const previous = state.lastAsk;
    const success = state.hands[ask.target].has(ask.card);
    const heldBefore = [...state.hands[actor]].filter(card => CARDS[card].set === ask.set).length;
    const reacting = Boolean(previous && previous.target === actor && !previous.success);
    state.asks++;
    state.teamAsks[team]++;
    state.informationGain[team] += ask.decision.features.informationGain;
    state.decisionRegret[team] += ask.decision.regret;
    state.continuationValue[team] += ask.decision.features.continuationValue;
    state.completionValue[team] += ask.decision.features.completionValue;
    if (!success) state.replyThreatOnMiss[team] += ask.decision.features.replyThreat;
    if (reacting) {
      state.reactionAsks++; state.teamReactionAsks[team]++;
      if (success) { state.reactionHits++; state.teamReactionHits[team]++; }
    }
    if (previous && previous.target === actor && !previous.success && previous.set !== ask.set) {
      state.diversions++; state.teamDiversions[team]++;
    }
    for (const memory of state.memories) {
      memory.excluded[ask.card][actor] = 1;
      memory.signals[actor][ask.set]++;
    }
    if (success) {
      state.successes++;
      state.teamHits[team]++;
      state.hitStreak = state.streakActor === actor ? state.hitStreak + 1 : 1;
      state.streakActor = actor;
      state.maxHitStreak = Math.max(state.maxHitStreak, state.hitStreak);
      state.hands[ask.target].delete(ask.card);
      state.hands[actor].add(ask.card);
      revealTransfer(state, ask.card, actor);
    } else {
      state.hitStreak = 0;
      state.streakActor = -1;
      for (const memory of state.memories) memory.excluded[ask.card][ask.target] = 1;
      state.turn = ask.target;
    }
    state.beliefVersion++;
    const isDiversion = previous && previous.target === actor && !previous.success && previous.set !== ask.set;
    const surprise = success ? (1 - ask.probability) * 4 : ask.probability * 3;
    const progressImpact = success && heldBefore >= 3 ? 2.5 : success && heldBefore === 2 ? 1 : 0;
    const replyImpact = !success ? ask.decision.features.replyThreat * 2 : 0;
    const regretImpact = Math.min(3, ask.decision.regret * .45);
    const pivotalScore = surprise + progressImpact + replyImpact + regretImpact + (reacting ? .8 : 0);
    const pivotalReasons = [
      ...(surprise >= 1.8 ? [success ? "unexpected card location confirmed" : "high-probability ask misses"] : []),
      ...(progressImpact >= 2 ? ["transfer materially advances a half-suit"] : []),
      ...(replyImpact >= 1.2 ? ["miss gives a dangerous responder the turn"] : []),
      ...(regretImpact >= 1 ? ["large gap from FishBot's preferred ask"] : []),
      ...(reacting ? [isDiversion ? "response diverts to a different half-suit" : "direct response to the previous ask"] : []),
    ];
    record(state, {
      type: "ask", actor, target: ask.target, card: ask.card, set: ask.set, success, team,
      text: `${PLAYER_NAMES[actor]} asks ${PLAYER_NAMES[ask.target]} for ${CARDS[ask.card].compact}`,
      annotation: `${success ? "Card transferred; turn retained" : "Miss; turn changes hands"}${isDiversion ? " · diversion detected" : ""}`,
      pivotalScore,
      pivotalReasons,
      decision: ask.decision,
    });
    state.publicHistory.push({ actor, target: ask.target, card: ask.card, success });
    state.lastAsk = { actor, target: ask.target, set: ask.set, success };
  }

  if (state.activeSets.some(Boolean)) {
    for (let set = 0; set < 9; set++) if (state.activeSets[set]) {
      const counts: [number, number] = [0, 0];
      for (const card of HALF_SUITS[set].cards) {
        const owner = state.hands.findIndex(h => h.has(card));
        if (owner >= 0) counts[TEAM[owner]]++;
      }
      const awarded: Team = counts[1] > counts[0] ? 1 : 0;
      state.score[awarded]++;
      state.activeSets[set] = false;
      record(state, { type: "adjudicate", actor: state.turn, set, team: awarded, text: `${HALF_SUITS[set].name} is adjudicated to Team ${awarded ? "B" : "A"}`, annotation: "Safety resolution after the action limit" });
    }
  }

  const winner: Team = state.score[1] > state.score[0] ? 1 : 0;
  const maxComeback = state.maxDeficit[winner];
  const interest = state.pivotalCount * 3 + state.peakPivotal * 2 + state.leadChanges * 8 + maxComeback * 7 + state.failedDeclarations * 10 + state.diversions * .35 - Math.abs(4.5 - state.score[0]) * .3;
  const bare = {
    seed: options.seed, strategies: options.strategies, psychologicalTells: options.psychologicalTells,
    declarationsEnabled: options.declarations, winner, score: state.score, asks: state.asks, successfulAsks: state.successes,
    declarations: state.declarations, failedDeclarations: state.failedDeclarations, diversions: state.diversions,
    leadChanges: state.leadChanges, maxComeback, actions: state.eventCount || loops, interest,
    pivotalCount: state.pivotalCount, peakPivotal: state.peakPivotal,
    teamAsks: state.teamAsks, teamHits: state.teamHits, teamDeclarations: state.teamDeclarations,
    teamCorrectDeclarations: state.teamCorrectDeclarations, informationGain: state.informationGain,
    decisionRegret: state.decisionRegret, reactionAsks: state.reactionAsks, reactionHits: state.reactionHits,
    teamReactionAsks: state.teamReactionAsks, teamReactionHits: state.teamReactionHits, teamDiversions: state.teamDiversions,
    replyThreatOnMiss: state.replyThreatOnMiss, continuationValue: state.continuationValue, completionValue: state.completionValue,
    maxHitStreak: state.maxHitStreak, firstDeclarationAction: state.firstDeclarationAction < 0 ? state.eventCount : state.firstDeclarationAction,
    ...(options.detailed ? { log: state.log, initialHands } : {}),
  };
  return { ...bare, tag: classify(bare) };
}

export function deriveGameSeed(base: number, i: number) {
  let x = (base + Math.imul(i + 1, 0x9e3779b1)) >>> 0;
  x ^= x >>> 16; x = Math.imul(x, 0x85ebca6b); x ^= x >>> 13; x = Math.imul(x, 0xc2b2ae35); x ^= x >>> 16;
  return x >>> 0;
}

function wilson(successes: number, total: number): [number, number] {
  if (!total) return [0, 0];
  const z = 1.96;
  const p = successes / total;
  const denominator = 1 + z * z / total;
  const center = (p + z * z / (2 * total)) / denominator;
  const margin = z * Math.sqrt((p * (1 - p) + z * z / (4 * total)) / total) / denominator;
  return [Math.max(0, center - margin), Math.min(1, center + margin)];
}

function percentile(sorted: number[], p: number) {
  if (!sorted.length) return 0;
  return sorted[Math.min(sorted.length - 1, Math.floor((sorted.length - 1) * p))];
}

export function runBatch(games: number, options: Omit<GameOptions, "seed" | "detailed"> & { seed?: number }): BatchResult {
  const started = performance.now();
  const base = options.seed ?? 20260820;
  let teamAWins = 0, scoreA = 0, scoreB = 0, actions = 0, asks = 0, hits = 0, declarations = 0, failedDeclarations = 0, diversions = 0, leadChanges = 0;
  let reactionAsks = 0, reactionHits = 0, pivotalMoments = 0, maxHitStreaks = 0;
  const teamAsks: [number, number] = [0, 0], teamHits: [number, number] = [0, 0];
  const teamDeclarations: [number, number] = [0, 0], teamCorrectDeclarations: [number, number] = [0, 0];
  const informationGain: [number, number] = [0, 0], decisionRegret: [number, number] = [0, 0];
  const teamReactionAsks: [number, number] = [0, 0], teamReactionHits: [number, number] = [0, 0], teamDiversions: [number, number] = [0, 0];
  const replyThreatOnMiss: [number, number] = [0, 0], continuationValue: [number, number] = [0, 0], completionValue: [number, number] = [0, 0];
  const gameLengths: number[] = [];
  const scoreDistribution = Array(10).fill(0);
  const outliers: GameSummary[] = [];
  for (let i = 0; i < games; i++) {
    const game = simulateGame({ ...options, seed: deriveGameSeed(base, i), detailed: false });
    if (game.winner === 0) teamAWins++;
    scoreA += game.score[0]; scoreB += game.score[1]; actions += game.actions; asks += game.asks; hits += game.successfulAsks;
    declarations += game.declarations; failedDeclarations += game.failedDeclarations; diversions += game.diversions; leadChanges += game.leadChanges;
    reactionAsks += game.reactionAsks; reactionHits += game.reactionHits; pivotalMoments += game.pivotalCount; maxHitStreaks += game.maxHitStreak;
    gameLengths.push(game.actions); scoreDistribution[game.score[0]]++;
    for (const team of [0, 1] as Team[]) {
      teamAsks[team] += game.teamAsks[team]; teamHits[team] += game.teamHits[team];
      teamDeclarations[team] += game.teamDeclarations[team]; teamCorrectDeclarations[team] += game.teamCorrectDeclarations[team];
      informationGain[team] += game.informationGain[team]; decisionRegret[team] += game.decisionRegret[team];
      teamReactionAsks[team] += game.teamReactionAsks[team]; teamReactionHits[team] += game.teamReactionHits[team];
      teamDiversions[team] += game.teamDiversions[team]; replyThreatOnMiss[team] += game.replyThreatOnMiss[team];
      continuationValue[team] += game.continuationValue[team]; completionValue[team] += game.completionValue[team];
    }
    outliers.push(game);
    outliers.sort((a, b) => b.interest - a.interest);
    if (outliers.length > 10) outliers.pop();
  }
  gameLengths.sort((a, b) => a - b);
  return {
    games, seed: base, psychologicalTells: options.psychologicalTells, declarationsEnabled: options.declarations,
    strategies: options.strategies, teamAWins, teamBWins: games - teamAWins, winRateA: teamAWins / games,
    avgScore: [scoreA / games, scoreB / games], avgActions: actions / games, askAccuracy: asks ? hits / asks : 0,
    declarationAccuracy: declarations ? (declarations - failedDeclarations) / declarations : 1,
    diversionRate: asks ? diversions / asks : 0, avgLeadChanges: leadChanges / games,
    winRateCI: wilson(teamAWins, games),
    teamAskAccuracy: [teamAsks[0] ? teamHits[0] / teamAsks[0] : 0, teamAsks[1] ? teamHits[1] / teamAsks[1] : 0],
    teamDeclarationAccuracy: [teamDeclarations[0] ? teamCorrectDeclarations[0] / teamDeclarations[0] : 1, teamDeclarations[1] ? teamCorrectDeclarations[1] / teamDeclarations[1] : 1],
    avgInformationGain: [teamAsks[0] ? informationGain[0] / teamAsks[0] : 0, teamAsks[1] ? informationGain[1] / teamAsks[1] : 0],
    avgDecisionRegret: [teamAsks[0] ? decisionRegret[0] / teamAsks[0] : 0, teamAsks[1] ? decisionRegret[1] / teamAsks[1] : 0],
    reactionAccuracy: reactionAsks ? reactionHits / reactionAsks : 0,
    teamReactionAccuracy: [teamReactionAsks[0] ? teamReactionHits[0] / teamReactionAsks[0] : 0, teamReactionAsks[1] ? teamReactionHits[1] / teamReactionAsks[1] : 0],
    teamDiversionRate: [teamAsks[0] ? teamDiversions[0] / teamAsks[0] : 0, teamAsks[1] ? teamDiversions[1] / teamAsks[1] : 0],
    avgReplyThreatOnMiss: [teamAsks[0] > teamHits[0] ? replyThreatOnMiss[0] / (teamAsks[0] - teamHits[0]) : 0, teamAsks[1] > teamHits[1] ? replyThreatOnMiss[1] / (teamAsks[1] - teamHits[1]) : 0],
    avgContinuationValue: [teamAsks[0] ? continuationValue[0] / teamAsks[0] : 0, teamAsks[1] ? continuationValue[1] / teamAsks[1] : 0],
    avgCompletionValue: [teamAsks[0] ? completionValue[0] / teamAsks[0] : 0, teamAsks[1] ? completionValue[1] / teamAsks[1] : 0],
    avgPivotalMoments: pivotalMoments / games, avgMaxHitStreak: maxHitStreaks / games,
    medianActions: percentile(gameLengths, .5), p90Actions: percentile(gameLengths, .9), scoreDistribution,
    outliers,
    elapsedMs: performance.now() - started,
  };
}

export function replayGame(summary: GameSummary) {
  return simulateGame({ seed: summary.seed, strategies: summary.strategies, psychologicalTells: summary.psychologicalTells, declarations: summary.declarationsEnabled, detailed: true });
}
