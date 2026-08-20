export type Team = 0 | 1;
export type StrategyId = "hunter" | "diversifier" | "detective" | "bluffer" | "random";

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
  hunter: { id: "hunter", name: "Focused hunter", short: "Focus", description: "Builds momentum in one half-suit and keeps pressing it.", icon: "◎", declarationThreshold: .82, risk: .16 },
  diversifier: { id: "diversifier", name: "Adaptive diversifier", short: "Diversify", description: "Switches half-suits to preserve options and spread information.", icon: "↝", declarationThreshold: .88, risk: .12 },
  detective: { id: "detective", name: "Bayesian detective", short: "Infer", description: "Targets the card-location hypothesis with the best posterior odds.", icon: "⌁", declarationThreshold: .92, risk: .06 },
  bluffer: { id: "bluffer", name: "Misdirection artist", short: "Bluff", description: "Uses deliberate diversions after revealing moments to distort tells.", icon: "◇", declarationThreshold: .76, risk: .28 },
  random: { id: "random", name: "Unpredictable novice", short: "Random", description: "Chooses legal asks with little memory—the experimental control.", icon: "✦", declarationThreshold: .68, risk: .42 },
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
};

export type GameSummary = {
  seed: number;
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
  log?: GameAction[];
};

export type BatchResult = {
  games: number;
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
};

const TEAM: Team[] = [0, 1, 0, 1, 0, 1];
export const PLAYER_NAMES = ["Mara", "Theo", "Iris", "Jonah", "Sage", "Nico"];

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

function record(state: State, action: Omit<GameAction, "index" | "score" | "cardCounts" | "hands">) {
  if (!state.opts.detailed) return;
  state.log.push({
    ...action,
    index: state.log.length,
    score: [...state.score],
    cardCounts: state.hands.map(h => h.size),
    hands: state.hands.map(h => [...h].sort((a, b) => a - b)),
  });
}

function legalCandidates(state: State, actor: number) {
  const memory = state.memories[actor];
  const candidates: { card: number; target: number; set: number; probability: number }[] = [];
  const heldSets = new Set([...state.hands[actor]].map(c => CARDS[c].set).filter(s => state.activeSets[s]));
  for (const set of heldSets) {
    for (const card of HALF_SUITS[set].cards) {
      if (state.hands[actor].has(card)) continue;
      for (let target = 0; target < 6; target++) {
        if (TEAM[target] === TEAM[actor] || state.hands[target].size === 0) continue;
        const probability = ownerProbability(memory, actor, card, target);
        // A known miss is still a legal (and sometimes strategically necessary)
        // ask. Keeping it in the action space prevents knowledge from creating
        // an artificial deadlock when every missing card appears to be friendly.
        candidates.push({ card, target, set, probability });
      }
    }
  }
  return candidates;
}

function chooseAsk(state: State, actor: number) {
  const strategy = STRATEGIES[state.opts.strategies[TEAM[actor]]];
  const legal = legalCandidates(state, actor);
  if (!legal.length) return null;
  if (strategy.id === "random") return legal[state.rng.int(legal.length)];
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
  let best = legal[0];
  let bestScore = -Infinity;
  for (const option of legal) {
    let score = option.probability * 9 + heldBySet[option.set] * .72 + teamKnown[option.set] * .25 + state.rng.next() * .7;
    if (strategy.id === "hunter") {
      score += option.set === state.focus[actor] ? 6.2 : -1.1;
      score += heldBySet[option.set] * 1.25;
    }
    if (strategy.id === "detective") {
      score += option.probability * 12;
      score += state.memories[actor].signals[option.target][option.set] * .85;
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
    if (responding && state.opts.psychologicalTells && strategy.id !== "bluffer") {
      score += option.set === state.lastAsk!.set ? 1.1 : -.25;
    }
    if (score > bestScore) { bestScore = score; best = option; }
  }
  return best;
}

function predictionForSet(state: State, actor: number, set: number) {
  const memory = state.memories[actor];
  const team = TEAM[actor];
  const teammates = [0, 1, 2, 3, 4, 5].filter(p => TEAM[p] === team);
  const owners: number[] = [];
  let teamConfidence = 1;
  let allocationConfidence = 1;
  for (const card of HALF_SUITS[set].cards) {
    const probs = teammates.map(p => [p, ownerProbability(memory, actor, card, p)] as const).sort((a, b) => b[1] - a[1]);
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
  let best: { set: number; owners: number[]; confidence: number; teamConfidence: number } | null = null;
  for (let set = 0; set < 9; set++) {
    if (!state.activeSets[set]) continue;
    const pred = predictionForSet(state, actor, set);
    const threshold = forced ? .38 : strategy.declarationThreshold;
    if (pred.teamConfidence < threshold || pred.confidence < threshold - strategy.risk * .22) continue;
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
  if (!correct) state.failedDeclarations++;
  state.score[awarded]++;
  const beforeLead = state.lastLead;
  const diff = state.score[0] - state.score[1];
  const lead = Math.sign(diff);
  if (beforeLead && lead && beforeLead !== lead) state.leadChanges++;
  if (lead) state.lastLead = lead;
  state.maxDeficit[0] = Math.max(state.maxDeficit[0], state.score[1] - state.score[0]);
  state.maxDeficit[1] = Math.max(state.maxDeficit[1], state.score[0] - state.score[1]);
  state.activeSets[declaration.set] = false;
  for (const card of cards) for (const hand of state.hands) hand.delete(card);
  const assignments = declaration.owners.map((p, i) => `${CARDS[cards[i]].compact}→${PLAYER_NAMES[p]}`).join(", ");
  record(state, {
    type: "declare", actor, set: declaration.set, team,
    success: correct, confidence: declaration.confidence,
    text: `${PLAYER_NAMES[actor]} ${correct ? "wins" : "misdeclares"} ${HALF_SUITS[declaration.set].name}`,
    annotation: `${forced ? "Forced endgame · " : ""}${Math.round(declaration.confidence * 100)}% belief confidence · ${assignments}`,
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
  const dealer = rng.int(6);
  const state: State = {
    rng, opts: options, hands, memories: [], activeSets: Array(9).fill(true), score: [0, 0], turn: (dealer + 1) % 6,
    log: [], asks: 0, successes: 0, declarations: 0, failedDeclarations: 0, diversions: 0, leadChanges: 0,
    maxDeficit: [0, 0], lastLead: 0, focus: Array(6).fill(null), lastAsk: null,
  };
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
      for (const set of state.activeSets.map((active, s) => active ? s : -1).filter(s => s >= 0)) {
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

    if (options.declarations) {
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

    const ask = chooseAsk(state, state.turn);
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
    state.asks++;
    if (previous && previous.target === actor && !previous.success && previous.set !== ask.set) state.diversions++;
    for (const memory of state.memories) {
      memory.excluded[ask.card][actor] = 1;
      memory.signals[actor][ask.set]++;
    }
    if (success) {
      state.successes++;
      state.hands[ask.target].delete(ask.card);
      state.hands[actor].add(ask.card);
      revealTransfer(state, ask.card, actor);
    } else {
      for (const memory of state.memories) memory.excluded[ask.card][ask.target] = 1;
      state.turn = ask.target;
    }
    const isDiversion = previous && previous.target === actor && !previous.success && previous.set !== ask.set;
    record(state, {
      type: "ask", actor, target: ask.target, card: ask.card, set: ask.set, success, team,
      text: `${PLAYER_NAMES[actor]} asks ${PLAYER_NAMES[ask.target]} for ${CARDS[ask.card].compact}`,
      annotation: `${success ? "Card transferred; turn retained" : "Miss; turn changes hands"}${isDiversion ? " · diversion detected" : ""}`,
    });
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
  const interest = state.asks / 14 + state.leadChanges * 8 + maxComeback * 7 + state.failedDeclarations * 10 + state.diversions * .7 + Math.abs(4.5 - state.score[0]) * -.3;
  const bare = {
    seed: options.seed, winner, score: state.score, asks: state.asks, successfulAsks: state.successes,
    declarations: state.declarations, failedDeclarations: state.failedDeclarations, diversions: state.diversions,
    leadChanges: state.leadChanges, maxComeback, actions: state.log.length || loops, interest,
    ...(options.detailed ? { log: state.log } : {}),
  };
  return { ...bare, tag: classify(bare) };
}

function mixSeed(base: number, i: number) {
  let x = (base + Math.imul(i + 1, 0x9e3779b1)) >>> 0;
  x ^= x >>> 16; x = Math.imul(x, 0x85ebca6b); x ^= x >>> 13; x = Math.imul(x, 0xc2b2ae35); x ^= x >>> 16;
  return x >>> 0;
}

export function runBatch(games: number, options: Omit<GameOptions, "seed" | "detailed"> & { seed?: number }): BatchResult {
  const started = performance.now();
  const base = options.seed ?? 20260820;
  let teamAWins = 0, scoreA = 0, scoreB = 0, actions = 0, asks = 0, hits = 0, declarations = 0, failedDeclarations = 0, diversions = 0, leadChanges = 0;
  const outliers: GameSummary[] = [];
  for (let i = 0; i < games; i++) {
    const game = simulateGame({ ...options, seed: mixSeed(base, i), detailed: false });
    if (game.winner === 0) teamAWins++;
    scoreA += game.score[0]; scoreB += game.score[1]; actions += game.actions; asks += game.asks; hits += game.successfulAsks;
    declarations += game.declarations; failedDeclarations += game.failedDeclarations; diversions += game.diversions; leadChanges += game.leadChanges;
    outliers.push(game);
    outliers.sort((a, b) => b.interest - a.interest);
    if (outliers.length > 10) outliers.pop();
  }
  return {
    games, strategies: options.strategies, teamAWins, teamBWins: games - teamAWins, winRateA: teamAWins / games,
    avgScore: [scoreA / games, scoreB / games], avgActions: actions / games, askAccuracy: asks ? hits / asks : 0,
    declarationAccuracy: declarations ? (declarations - failedDeclarations) / declarations : 1,
    diversionRate: asks ? diversions / asks : 0, avgLeadChanges: leadChanges / games, outliers,
    elapsedMs: performance.now() - started,
  };
}

export function replayGame(summary: GameSummary, strategies: [StrategyId, StrategyId], psychologicalTells: boolean, declarations: boolean) {
  return simulateGame({ seed: summary.seed, strategies, psychologicalTells, declarations, detailed: true });
}
