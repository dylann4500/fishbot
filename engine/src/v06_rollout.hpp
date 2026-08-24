// FishBot v0.6 -- determinized information-set rollouts.
//
// WHAT THIS IS NOT.  It is not perfect-information Monte Carlo.  PIMC is
// degenerate in Fish and the project eliminated it in the v0.4 study
// (research/v04/lit/pimc.md, paper/sections/03-related.tex:39-60): a clairvoyant
// player never fails an ask and never misdeclares, so a double-dummy evaluator
// collapses toward argmax P(target holds card) and prices none of the
// information the game is actually about.
//
// WHAT IT IS.  The deal is determinized -- because Proposition 1 makes the
// initial deal the entire hidden state -- but the six players who then play the
// continuation are NOT clairvoyant.  Each is reconstructed at its own
// information set: the public deduction state (which is common knowledge, see
// below) refined by the hand that determinization gives it.  So the rollout
// contains failed asks, missed certificates and wrong declarations in the same
// proportions the real game does.  This is the cooperative single-agent search
// of Lerer et al. (SPARTA, AAAI 2020) with the belief taken from an exactly
// enumerable posterior rather than a learned one.
//
// WHY THE RECONSTRUCTION IS CHEAP.  Knowledge::onEvent (belief.hpp:153-210) is a
// function of the event alone except for the two lines that maintain `myHand`,
// so six Knowledge objects fed the same event stream from an all-possible start
// agree bit for bit outside `me`/`myHand`.  The public deduction state is
// therefore ONE object, replayed once per decision (8.7 us measured for the
// replay plus all six refinements, research/v06/notes/R10), not six.  The same
// observation underlies M4 in research/v05/patches/M4-M5.patch.
//
// SOUNDNESS OF THE REFINEMENT.  model(j) = public state + "j holds exactly the
// cards determinization d gives it".  Inside a determinization that is not an
// under-approximation but j's exact information set, because d fixes j's hand,
// and j's knowledge is precisely the public record plus its own hand.
#pragma once
#include "game.hpp"
#include "v07_leaf.hpp"
#include <memory>
#include <string>

namespace fish {

// Defined in factory.hpp.  Declared rather than included here so that
// factory.hpp can include v06.hpp (which includes this file) without a cycle.
std::unique_ptr<Agent> makeAgent(const std::string& spec);

namespace v06 {

// The common-knowledge start state: nobody's hand is known to anybody.
// Knowledge::init() would exclude the named seat from every card it does not
// hold, which is the private constraint we are deliberately dropping here.
inline void initPublicKnowledge(Knowledge& kk, int deckSets) {
  kk.me = NPLAY;                       // out of range for every seat
  kk.myHand = 0;
  kk.nSets = deckSets;
  kk.disj.clear();
  kk.unresolved = 0;
  memset(kk.askCount, 0, sizeof(kk.askCount));
  memset(kk.missCount, 0, sizeof(kk.missCount));
  memset(kk.totalAsks, 0, sizeof(kk.totalAsks));
  kk.publicKnown = 0;
  for (int s = 0; s < NSET; s++) kk.setActive[s] = (s < deckSets);
  for (int p = 0; p < NPLAY; p++) kk.handCount[p] = uint8_t(deckSets * SETSZ / NPLAY);
  for (int c = 0; c < NCARD; c++) {
    if (setOf(c) >= deckSets) { kk.owner[c] = OUT_OF_PLAY; kk.mask[c] = 0; continue; }
    kk.owner[c] = UNKNOWN;
    kk.mask[c] = 0x3F;
    kk.unresolved |= bit(c);
  }
}

// Refine the public state with seat j's hand under a determinization.
inline void refineWithHand(Knowledge& kk, int j, uint64_t hand) {
  kk.me = uint8_t(j);
  kk.myHand = hand;
  for (int c = 0; c < NCARD; c++) {
    if (kk.owner[c] == OUT_OF_PLAY) continue;
    if (hand & bit(c)) { if (kk.owner[c] != j) kk.setOwner(c, j); }
    else if (kk.owner[c] == UNKNOWN && (kk.mask[c] & (1u << j))) kk.exclude(c, j);
  }
  kk.propagateCapacity();
}

struct RolloutConfig {
  std::string rolloutSpec = "v05:belief=indep,topk=0";
  // v0.7 C3.  The blueprint used for the OPPOSING seats of the rollout.  Empty
  // means "the same as ours", which is what v0.6 did and is the right choice for
  // self-improvement: the search then assumes its opponents play as it does.
  // It is the wrong choice for EXPLOITATION.  A responder that knows the target
  // exactly -- which the threat model grants it, T3 -- should roll the target's
  // seats out with the TARGET's policy, so that the value it maximises is the
  // value against the opponent it actually faces rather than against a mirror of
  // itself.  This one string is the difference between a search that improves a
  // policy and a search that best-responds to one, and the corpus has only ever
  // run the first.
  std::string oppSpec  = "";
  int   myTeam     = -1;     // which team the searching seat belongs to
  int   maxDepth   = 0;      // 0 = play to the end
  double leafLambda = 1.0;   // weight on expected half-suit control at a depth cut
  std::string leafSpec = "material";
  int   askCap     = 260;    // per-rollout safety valve
};

// Six blueprint agents plus the scratch a rollout needs.  One per thread.
struct RolloutEngine {
  RolloutConfig cfg;
  std::unique_ptr<Agent> ag[NPLAY];
  Knowledge pubK;              // public deduction state at the decision point
  PublicState basePub;         // pub with the history stripped: rollouts start empty
  bool pubReady = false;
  Game sim;
  int deckSets = 9;
  long long rollouts = 0, rolloutEvents = 0, truncations = 0;
  std::unique_ptr<LeafEvaluator> leafEval;
  std::string builtSig;

  std::string agentSig() const {
    return cfg.rolloutSpec + "||" + cfg.oppSpec + "||" + std::to_string(cfg.myTeam);
  }
  void ensureAgents() {
    std::string sig = agentSig();
    if (ag[0] && sig == builtSig) return;
    for (int p = 0; p < NPLAY; p++) {
      bool mine = (cfg.myTeam < 0) || (teamOf(p) == cfg.myTeam) || cfg.oppSpec.empty();
      ag[p] = makeAgent(mine ? cfg.rolloutSpec : cfg.oppSpec);
    }
    builtSig = sig;
  }
  LeafEvaluator& evaluator() {
    if (!leafEval) leafEval = makeLeafEvaluator(cfg.leafSpec, cfg.leafLambda);
    return *leafEval;
  }
  void rebuildEvaluator() { leafEval.reset(); }

  // Replay the public record once.  Everything downstream copies this.
  void beginDecision(const PublicState& pub) {
    ensureAgents();
    deckSets = pub.rules.deckSets;
    initPublicKnowledge(pubK, deckSets);
    for (const Event& e : pub.history) pubK.onEvent(e);
    basePub = pub;
    basePub.history.clear();
    pubReady = true;
  }

  // Materialise the hands implied by a determinization: cards the observer has
  // already resolved go where the observer knows they are; the rest go where
  // `owners` puts them.
  static void handsFrom(const PublicState& pub, const Knowledge& mine,
                        const uint8_t* owners, uint64_t* hand) {
    for (int p = 0; p < NPLAY; p++) hand[p] = 0;
    for (int c = 0; c < NCARD; c++) {
      if (!pub.setActive[setOf(c)]) continue;
      int o;
      if (mine.owner[c] < NPLAY) o = mine.owner[c];
      else if (mine.owner[c] == OUT_OF_PLAY) continue;
      else o = owners[c];
      if (o >= 0 && o < NPLAY) hand[o] |= bit(c);
    }
  }

  // Perfect-information leaf value, used only when a depth cut fires.  Signed
  // for `team`.  Half-suits still active are priced by the fraction of the
  // half-suit the team physically holds, which is the quantity the adjudication
  // rule (game.hpp:271-287) actually resolves on.
  double leafValue(const GameState& g, int team) {
    double f[NLEAF];
    leafFeatures(g, team, cfg.leafLambda, f);
    return evaluator().value(f);
  }
  // The v0.6 formula, kept verbatim so the refactor has an identity control
  // rather than an argument.  `fish v7leafcheck` asserts leafValue == this for
  // the default evaluator over a sample of real leaves.
  double leafValueV06(const GameState& g, int team) const {
    double v = double(g.pub.score[team]) - double(g.pub.score[1 - team]);
    for (int s = 0; s < NSET; s++) {
      if (!g.pub.setActive[s]) continue;
      int a = 0;
      for (int p = team; p < NPLAY; p += 2) a += popcount64(g.hand[p] & setMask(s));
      v += cfg.leafLambda * (2.0 * double(a) / double(SETSZ) - 1.0);
    }
    return v;
  }

  // Play the continuation out.  Mirrors Game::run's main loop exactly, minus
  // setup(), the value sink and the calibration sink; the driver's own methods
  // (declarationRound, forcedEndgame, emit, adjudicateRemaining) are reused so
  // the rules cannot drift between the search and the real game.
  // `leafFeat`/`truncated` are the batch path: when a depth cut fires and a
  // buffer is supplied, the leaf's feature row is written and the value is left
  // for the caller to compute in one batched call per decision.  With no buffer
  // the evaluator is called inline, which is what v0.6 did.
  double playOut(const uint64_t* hand, int turn, int team, const AskMove* first,
                 double* leafFeat = nullptr, bool* truncated = nullptr) {
    sim.rules = basePub.rules;
    sim.g.pub.history.clear();
    sim.g.pub = basePub;
    for (int p = 0; p < NPLAY; p++) {
      sim.g.hand[p] = hand[p];
      sim.g.dealt[p] = hand[p];
      sim.g.pub.handCount[p] = uint8_t(popcount64(hand[p]));
    }
    for (int s = 0; s < NSET; s++) sim.g.setWinner[s] = 2;
    sim.g.turn = turn; sim.g.pub.turn = turn;
    sim.res = GameResult{};
    sim.audit = false; sim.trace.on = false; sim.observer = nullptr;
    sim.calib = nullptr; sim.vsink = nullptr;
    for (int s = 0; s < NSET; s++) sim.lockedAt[s] = -1;
    for (int p = 0; p < NPLAY; p++) sim.agents[p] = ag[p].get();

    const int startEv = sim.g.pub.nEvents;
    int asks = 0;
    if (first) {
      int actor = turn, target = first->target, card = first->card;
      bool success = (sim.g.hand[target] & bit(card)) != 0;
      if (success) { sim.g.hand[target] &= ~bit(card); sim.g.hand[actor] |= bit(card); }
      sim.g.pub.handCount[actor] = uint8_t(popcount64(sim.g.hand[actor]));
      sim.g.pub.handCount[target] = uint8_t(popcount64(sim.g.hand[target]));
      Event e{}; e.kind = Kind::Ask; e.actor = uint8_t(actor); e.target = uint8_t(target);
      e.card = uint8_t(card); e.set = uint8_t(setOf(card)); e.success = success;
      sim.emit(e);
      if (!success) { sim.g.turn = target; sim.g.pub.turn = target; }
      asks++;
    }
    while (true) {
      if (cfg.maxDepth > 0 && sim.g.pub.nEvents - startEv >= cfg.maxDepth) {
        rollouts++; truncations++; rolloutEvents += sim.g.pub.nEvents - startEv;
        if (leafFeat) { leafFeatures(sim.g, team, cfg.leafLambda, leafFeat); if (truncated) *truncated = true; return 0.0; }
        return leafValue(sim.g, team);
      }
      sim.declarationRound();
      if (!sim.g.pub.activeSets()) break;
      bool a0 = sim.g.pub.teamAlive(0), a1 = sim.g.pub.teamAlive(1);
      if (!a0 || !a1) { sim.forcedEndgame(a0 ? 0 : 1); break; }
      if (!sim.g.pub.handCount[sim.g.turn]) {
        int cand[3], n = 0;
        for (int p = teamOf(sim.g.turn); p < NPLAY; p += 2)
          if (sim.g.pub.handCount[p]) cand[n++] = p;
        if (!n) { sim.forcedEndgame(1 - teamOf(sim.g.turn)); break; }
        int rcv = sim.agents[sim.g.turn]->choosePassTarget(sim.g.pub, cand, n);
        bool valid = false; for (int i = 0; i < n; i++) if (cand[i] == rcv) valid = true;
        if (!valid) rcv = cand[0];
        Event e{}; e.kind = Kind::Pass; e.actor = uint8_t(sim.g.turn); e.target = uint8_t(rcv);
        sim.emit(e);
        sim.g.turn = rcv; sim.g.pub.turn = rcv;
        continue;
      }
      AskMove mv = sim.agents[sim.g.turn]->chooseAsk(sim.g.pub);
      if (!legalAsk(sim.g, sim.g.turn, mv.card, mv.target)) {
        AskMove buf[NSET * SETSZ * 3];
        int n = enumerateAsks(sim.g.pub, sim.g.hand[sim.g.turn], sim.g.turn, buf);
        if (!n) {
          Declaration d{}; double conf = 0; int chosen = -1;
          for (int st = 0; st < NSET; st++)
            if (sim.g.pub.setActive[st] && (sim.g.hand[sim.g.turn] & setMask(st))) { chosen = st; break; }
          if (chosen < 0) break;
          sim.agents[sim.g.turn]->bestGuess(sim.g.pub, chosen, d, conf);
          d.set = uint8_t(chosen);
          sim.applyDeclaration(sim.g.turn, d, true, conf);
          continue;
        }
        mv = buf[0];
      }
      int actor = sim.g.turn, target = mv.target, card = mv.card;
      bool success = (sim.g.hand[target] & bit(card)) != 0;
      if (success) { sim.g.hand[target] &= ~bit(card); sim.g.hand[actor] |= bit(card); }
      sim.g.pub.handCount[actor] = uint8_t(popcount64(sim.g.hand[actor]));
      sim.g.pub.handCount[target] = uint8_t(popcount64(sim.g.hand[target]));
      Event e{}; e.kind = Kind::Ask; e.actor = uint8_t(actor); e.target = uint8_t(target);
      e.card = uint8_t(card); e.set = uint8_t(setOf(card)); e.success = success;
      sim.emit(e);
      if (!success) { sim.g.turn = target; sim.g.pub.turn = target; }
      if (++asks >= cfg.askCap) { sim.adjudicateRemaining(); break; }
    }
    rollouts++; rolloutEvents += sim.g.pub.nEvents - startEv;
    return double(sim.g.pub.score[team]) - double(sim.g.pub.score[1 - team]);
  }

  // Seat the six blueprint agents at their information sets under one
  // determinization, then apply `first` (an ask by `seat`) and play out.
  // `first.card == 0xFF` means "make no opening move, just play out from here",
  // which is how a declaration candidate is priced.
  void seatAgents(const uint64_t* hand, uint64_t seed) {
    for (int j = 0; j < NPLAY; j++) {
      ag[j]->reset(j, hand[j], basePub.rules, mixSeed(seed, uint64_t(j) * 131 + 7));
      Knowledge kj = pubK;
      refineWithHand(kj, j, hand[j]);
      ag[j]->k = kj;
    }
  }
};

} // namespace v06
} // namespace fish
