// KV's sampled-world search policy, ported into the FishLab engine.
//
// Source: github.com/kv1514/fish-researchp12 @ 0676737 -- `fish/agents.py`
// (SearchAgent, on top of ProbabilisticAgent and BasicHeuristicAgent) and
// `fish/belief.py` (ParticleBelief).  SearchAgent is the top of that
// repository's policy ladder and the `team_a` entry of its configs/ismcts.yaml.
//
// The two projects model the same game -- six seats, alternating teams, nine
// six-card half-suits including eights-and-jokers, ask legality requiring
// another card of the asked half-suit, claims on your own turn only -- and they
// number cards identically (half-suit h, position i -> card 6h+i), so this is a
// port of the POLICY, not a re-implementation of the rules.
//
// Faithfulness.  Every formula, constant, ordering rule and tie-break policy is
// reproduced.  What cannot be reproduced is CPython's Mersenne Twister, so the
// random tie-breaks and the particle draw differ.  Everything deterministic was
// checked against the Python original decision by decision through the
// TypeScript port of the same class: see scripts/kv_parity_ref.py.
//
// Two of KV's house rules differ from this engine's and are honoured here as
// POLICY, since they are what his agent was written against:
//   * claims are legal on your own turn only (ClaimTiming.TURN_ONLY), so this
//     agent never proposes an out-of-turn declaration even though the engine
//     permits one;
//   * a cardless player may not claim, so it stays silent when empty.
// The engine's own rule differences -- above all that a misdistributed claim
// awards the half-suit to the opponents here, where KV's default voids it --
// are rules, not policy, and are left alone.
#pragma once
#include "fish.hpp"
#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace fish {
namespace kvsearch {

struct Config {
  int particles          = 96;    // runner.make_agent
  int determinizations   = 48;    // runner.make_agent
  int depth              = 2;     // runner.make_agent
  double successReward   = 1.0;   // SearchAgent.__init__
  double failurePenalty  = 0.2;   // SearchAgent.__init__
  int maxRootActions     = 12;    // SearchAgent.__init__
  int maxRolloutActions  = 4;     // SearchAgent.__init__
  double informationWeight = 0.08;  // ProbabilisticAgent.__init__
  double claimSupport    = 0.75;  // ProbabilisticAgent.candidate_actions
  int stalemateClaimAfter = 256;  // BasicHeuristicAgent.__init__
};

// An ask or a claim, the two action shapes KV's candidate set contains.
struct Action {
  bool claim = false;
  uint8_t card = 0, target = 0;               // ask
  uint8_t set = 0;
  uint8_t owner[SETSZ] = {0,0,0,0,0,0};       // claim allocation
  double support = 0;                         // particle support for a claim
};

inline double binEntropy(double p) {
  if (p <= 0 || p >= 1) return 0;
  return -p * std::log2(p) - (1 - p) * std::log2(1 - p);
}

// CPython's math.isclose defaults: rel_tol 1e-9, abs_tol 0.
inline bool isClose(double a, double b) {
  return std::fabs(a - b) <= 1e-9 * std::max(std::fabs(a), std::fabs(b));
}

// ---------------------------------------------------------------------------
// fish/belief.py :: ParticleBelief
//
// Domains carry exactly the inferences the original makes and no others: the
// observer's own hand, its own non-hand exclusions, successful transfers (the
// card is now the asker's), failed asks (the target does not hold it), and
// resolved half-suits.  Note what is absent -- the original never infers that
// an asker cannot hold the card it just asked for, though its own rules
// guarantee it, and this engine's Knowledge does.  Deriving domains from the
// public history rather than from Knowledge is what keeps the port honest.
// ---------------------------------------------------------------------------
struct Belief {
  uint8_t domain[NCARD] = {0};
  bool resolvedCard[NCARD] = {false};
  int nParticles = 0;
  std::vector<int8_t> flat;        // nParticles * NCARD, -1 == out of play
  Rng rng;

  // Scratch, reused across samples.
  std::vector<int> undecided;
  std::vector<double> tieBreak;
  std::vector<int> suffix;
  std::vector<int8_t> draft;
  int remaining[NPLAY] = {0,0,0,0,0,0};
  long long budget = 0;

  explicit Belief(uint64_t seed = 0) : rng(seed ? seed : 0x9E3779B97F4A7C15ull) {}

  inline const int8_t* particle(int i) const { return &flat[size_t(i) * NCARD]; }

  bool rebuild(int me, uint64_t myHand, const uint8_t* handCount,
               const bool* setActive, const std::vector<Event>& history,
               int deckSets, int want) {
    const uint8_t all = uint8_t((1u << NPLAY) - 1);
    for (int c = 0; c < NCARD; c++) {
      if (setOf(c) >= deckSets) { domain[c] = 0; resolvedCard[c] = true; continue; }
      resolvedCard[c] = false;
      domain[c] = (myHand & bit(c)) ? uint8_t(1u << me) : uint8_t(all & ~(1u << me));
    }
    for (const Event& e : history) {
      if (e.kind != Kind::Ask) continue;            // claims arrive as setActive
      if (e.success) domain[e.card] = uint8_t(1u << e.actor);
      else domain[e.card] = uint8_t(domain[e.card] & ~(1u << e.target));
    }
    for (int c = 0; c < NCARD; c++) if (myHand & bit(c)) domain[c] = uint8_t(1u << me);
    for (int s = 0; s < NSET; s++) {
      if (setActive[s] && s < deckSets) continue;
      for (int i = 0; i < SETSZ; i++) { int c = cardOf(s, i); domain[c] = 0; resolvedCard[c] = true; }
    }

    int active = 0;
    for (int p = 0; p < NPLAY; p++) active += handCount[p];
    int resolved = 0;
    for (int c = 0; c < NCARD; c++) resolved += resolvedCard[c];
    if (resolved != NCARD - active) return false;          // InconsistentObservation
    for (int c = 0; c < NCARD; c++) if (!resolvedCard[c] && !domain[c]) return false;

    flat.assign(size_t(want) * NCARD, -1);
    nParticles = 0;
    draft.assign(NCARD, -1);
    int attempts = std::max(64, want * 12);
    for (int a = 0; a < attempts && nParticles < want; a++) {
      if (!sampleAssignment(handCount)) continue;
      std::copy(draft.begin(), draft.end(), flat.begin() + size_t(nParticles) * NCARD);
      nParticles++;
    }
    return nParticles > 0;
  }

  bool sampleAssignment(const uint8_t* handCount) {
    for (int p = 0; p < NPLAY; p++) remaining[p] = handCount[p];
    std::fill(draft.begin(), draft.end(), int8_t(-1));
    undecided.clear();
    for (int c = 0; c < NCARD; c++) {
      if (resolvedCard[c]) continue;
      uint8_t d = domain[c];
      if (__builtin_popcount(d) == 1) {
        int owner = __builtin_ctz(d);
        draft[c] = int8_t(owner);
        remaining[owner] -= 1;
      } else undecided.push_back(c);
    }
    for (int p = 0; p < NPLAY; p++) if (remaining[p] < 0) return false;

    // Minimum remaining values, random tie-break -- the original's ordering.
    int total = int(undecided.size());
    tieBreak.assign(NCARD, 0.0);
    for (int c : undecided) tieBreak[c] = rng.uni();
    std::stable_sort(undecided.begin(), undecided.end(), [&](int a, int b) {
      int pa = __builtin_popcount(domain[a]), pb = __builtin_popcount(domain[b]);
      if (pa != pb) return pa < pb;
      return tieBreak[a] < tieBreak[b];
    });

    // `sum(player in domains[item] for item in undecided[index:])`, precomputed.
    // Domains are fixed during the search, so this is the same predicate the
    // original evaluates inline.
    suffix.assign(size_t(total + 1) * NPLAY, 0);
    for (int i = total - 1; i >= 0; i--) {
      uint8_t d = domain[undecided[i]];
      for (int p = 0; p < NPLAY; p++)
        suffix[size_t(i) * NPLAY + p] = suffix[size_t(i + 1) * NPLAY + p] + ((d >> p) & 1);
    }
    budget = 400000;                       // guard against a pathological hang
    return assign(0, total);
  }

  bool assign(int index, int total) {
    if (index == total) {
      for (int p = 0; p < NPLAY; p++) if (remaining[p] != 0) return false;
      return true;
    }
    if (--budget < 0) return false;
    int card = undecided[index];
    uint8_t d = domain[card];
    int cand[NPLAY]; int n = 0;
    for (int p = 0; p < NPLAY; p++) if (((d >> p) & 1) && remaining[p] > 0) cand[n++] = p;
    for (int i = n - 1; i > 0; i--) { int j = int(rng.u32(uint32_t(i + 1))); std::swap(cand[i], cand[j]); }
    std::stable_sort(cand, cand + n, [&](int a, int b) { return remaining[a] > remaining[b]; });
    const int base = (index + 1) * NPLAY;
    for (int i = 0; i < n; i++) {
      int owner = cand[i];
      draft[card] = int8_t(owner);
      remaining[owner] -= 1;
      bool feasible = true;
      for (int p = 0; p < NPLAY; p++) if (remaining[p] > suffix[size_t(base) + p]) { feasible = false; break; }
      if (feasible && assign(index + 1, total)) return true;
      remaining[owner] += 1;
      draft[card] = -1;
    }
    return false;
  }

  double probability(int card, int owner) const {
    if (!nParticles) return 0;
    int hits = 0;
    for (int i = 0; i < nParticles; i++) if (particle(i)[card] == owner) hits++;
    return double(hits) / double(nParticles);
  }
};

// ---------------------------------------------------------------------------
// fish/agents.py :: SearchAgent
// ---------------------------------------------------------------------------
struct KVAgent : Agent {
  Config cfg;
  Belief bel;
  Rng rng;
  uint64_t myHand = 0;
  int deckSets = NSET;
  std::string label = "kv-search";

  // BasicHeuristicAgent._stalemate_claim_due
  int progressSignature = -1, progressPly = 0;

  // One decision per (state, turn): the declaration poll and the ask both read it.
  int cachedEvents = -1;
  uint64_t cachedHands = 0;
  bool cachedIsClaim = false;
  Action cachedAction{};

  KVAgent() : bel(0), rng(0) {}
  const char* name() const override { return label.c_str(); }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    myHand = hand;
    deckSets = r.deckSets;
    // runner.play_game seeds each seat separately; the belief gets the same
    // seed value on its own stream, as ProbabilisticAgent.__init__ does.
    rng = Rng(mixSeed(seed, uint64_t(s) * 97 + 1));
    bel = Belief(mixSeed(seed, uint64_t(s) * 97 + 2));
    progressSignature = -1; progressPly = 0;
    cachedEvents = -1; cachedHands = 0;
  }

  void observe(const Event& e) override {
    Agent::observe(e);
    if (e.kind == Kind::Ask && e.success) {
      if (e.actor == seat) myHand |= bit(e.card);
      else if (e.target == seat) myHand &= ~bit(e.card);
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      myHand &= ~setMask(e.decl.set);
    }
  }

  // -- BasicHeuristicAgent.score_action -------------------------------------
  double basicScore(const Action& a, const uint8_t* handCount) const {
    if (a.claim) {
      bool complete = true;
      for (int i = 0; i < SETSZ; i++) {
        if (!(myHand & bit(cardOf(a.set, i))) || a.owner[i] != seat) { complete = false; break; }
      }
      return complete ? 1000.0 : -1000.0;
    }
    int s = setOf(a.card);
    int suitCount = __builtin_popcountll(myHand & setMask(s));
    int targetCount = handCount[a.target];
    return suitCount * 2.0 + 1.0 / double(std::max(1, targetCount));
  }

  // -- ProbabilisticAgent._probabilistic_score ------------------------------
  double probScore(const Action& a, const uint8_t* handCount) const {
    double base = basicScore(a, handCount);
    if (a.claim) {
      int exact = 0, opponentHeld = 0;
      for (int i = 0; i < bel.nParticles; i++) {
        const int8_t* w = bel.particle(i);
        bool matches = true, opponent = false;
        for (int j = 0; j < SETSZ; j++) {
          int8_t o = w[cardOf(a.set, j)];
          if (o != int8_t(a.owner[j])) matches = false;
          if (o >= 0 && teamOf(o) != teamOf(seat)) opponent = true;
        }
        exact += matches; opponentHeld += opponent;
      }
      double n = double(bel.nParticles);
      return 100.0 * (exact / n) - 120.0 * (opponentHeld / n) - 20.0;
    }
    double p = bel.probability(a.card, a.target);
    return base + 20.0 * p + cfg.informationWeight * binEntropy(p);
  }

  // -- SearchAgent._world_action_value ---------------------------------------
  double worldValue(const int8_t* w, const Action& a, const std::vector<Action>& roots,
                    const uint8_t* handCount, int depth, uint32_t used) const {
    if (a.claim) {
      bool matches = true, opponent = false;
      for (int j = 0; j < SETSZ; j++) {
        int8_t o = w[cardOf(a.set, j)];
        if (o != int8_t(a.owner[j])) matches = false;
        if (o >= 0 && teamOf(o) != teamOf(seat)) opponent = true;
      }
      if (matches) return 6.0;
      if (opponent) return -6.0;
      return -2.0;
    }
    if (w[a.card] != int8_t(a.target)) return -cfg.failurePenalty;
    double value = cfg.successReward;
    if (depth <= 1) return value;
    int next[NSET * SETSZ * 3]; int n = 0;
    for (size_t i = 0; i < roots.size(); i++) {
      if (used & (1u << i)) continue;
      const Action& c = roots[i];
      if (c.card == a.card && c.target == a.target) continue;
      if (w[c.card] == int8_t(c.target)) next[n++] = int(i);
    }
    if (!n) return value;
    std::stable_sort(next, next + n, [&](int x, int y) {
      return probScore(roots[x], handCount) > probScore(roots[y], handCount);
    });
    if (n > cfg.maxRolloutActions) n = cfg.maxRolloutActions;
    int self = -1;
    for (size_t i = 0; i < roots.size(); i++)
      if (!roots[i].claim && roots[i].card == a.card && roots[i].target == a.target) { self = int(i); break; }
    uint32_t deeper = used | (self >= 0 ? (1u << self) : 0u);
    double best = -1e300;
    for (int i = 0; i < n; i++)
      best = std::max(best, worldValue(w, roots[next[i]], roots, handCount, depth - 1, deeper));
    return value + 0.85 * best;
  }

  // -- ProbabilisticAgent.candidate_actions ---------------------------------
  std::vector<Action> candidateActions(const PublicState& pub, bool claimsAllowed) {
    std::vector<Action> actions;
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, myHand, seat, buf);
    for (int i = 0; i < n; i++) {
      Action a; a.card = buf[i].card; a.target = buf[i].target;
      actions.push_back(a);
    }
    if (!claimsAllowed) return actions;

    // BasicHeuristicAgent._stalemate_claim_due
    int signature = 0;
    for (int s = 0; s < NSET; s++) if (!pub.setActive[s]) signature |= (1 << s);
    bool stalled = false;
    if (signature != progressSignature) { progressSignature = signature; progressPly = pub.nEvents; }
    else if (pub.nEvents - progressPly >= cfg.stalemateClaimAfter) { progressPly = pub.nEvents; stalled = true; }

    bool haveFallback = false; Action fallback{};
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      // Counter over team-consistent allocations; first inserted wins a tie,
      // as Counter.most_common(1) does.
      std::vector<std::pair<std::array<int8_t, SETSZ>, int>> tally;
      for (int i = 0; i < bel.nParticles; i++) {
        const int8_t* w = bel.particle(i);
        std::array<int8_t, SETSZ> alloc{};
        bool teamOwned = true;
        for (int j = 0; j < SETSZ; j++) {
          int8_t o = w[cardOf(s, j)];
          if (o < 0 || teamOf(o) != teamOf(seat)) { teamOwned = false; break; }
          alloc[j] = o;
        }
        if (!teamOwned) continue;
        bool found = false;
        for (auto& entry : tally) if (entry.first == alloc) { entry.second++; found = true; break; }
        if (!found) tally.push_back({alloc, 1});
      }
      if (tally.empty()) continue;
      const std::pair<std::array<int8_t, SETSZ>, int>* top = &tally[0];
      for (const auto& entry : tally) if (entry.second > top->second) top = &entry;
      Action claim; claim.claim = true; claim.set = uint8_t(s);
      for (int j = 0; j < SETSZ; j++) claim.owner[j] = uint8_t(top->first[j]);
      claim.support = double(top->second) / double(bel.nParticles);
      if (!haveFallback || claim.support > fallback.support) { fallback = claim; haveFallback = true; }
      if (claim.support >= cfg.claimSupport) actions.push_back(claim);
    }

    if (stalled) {
      if (haveFallback) return {fallback};
      return {fallbackClaim(pub)};
    }
    if (actions.empty() && haveFallback) actions.push_back(fallback);
    return actions;
  }

  // -- BasicHeuristicAgent._fallback_claim -----------------------------------
  Action fallbackClaim(const PublicState& pub) {
    int best = -1, bestHeld = -1;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      int held = __builtin_popcountll(myHand & setMask(s));
      if (held > bestHeld) { bestHeld = held; best = s; }
    }
    Action a; a.claim = true; a.set = uint8_t(best < 0 ? 0 : best);
    int team[3]; int nt = 0;
    for (int p = 0; p < NPLAY; p++) if (teamOf(p) == teamOf(seat)) team[nt++] = p;
    for (int j = 0; j < SETSZ; j++) {
      int c = cardOf(a.set, j);
      a.owner[j] = (myHand & bit(c)) ? uint8_t(seat) : uint8_t(team[rng.u32(uint32_t(nt))]);
    }
    return a;
  }

  // -- SearchAgent.choose_action ---------------------------------------------
  Action chooseAction(const PublicState& pub, std::vector<Action>& legal) {
    std::vector<Action> asks, nonAsks;
    for (const Action& a : legal) (a.claim ? nonAsks : asks).push_back(a);
    std::stable_sort(asks.begin(), asks.end(), [&](const Action& a, const Action& b) {
      return probScore(a, pub.handCount) > probScore(b, pub.handCount);
    });
    if (int(asks.size()) > cfg.maxRootActions) asks.resize(size_t(cfg.maxRootActions));
    std::vector<Action> search = asks;
    search.insert(search.end(), nonAsks.begin(), nonAsks.end());

    std::vector<int> draw(size_t(cfg.determinizations));
    for (int i = 0; i < cfg.determinizations; i++) draw[size_t(i)] = int(rng.u32(uint32_t(bel.nParticles)));

    std::vector<double> values(search.size());
    for (size_t a = 0; a < search.size(); a++) {
      double total = 0;
      for (int i = 0; i < cfg.determinizations; i++)
        total += worldValue(bel.particle(draw[size_t(i)]), search[a], asks, pub.handCount, cfg.depth, 0);
      values[a] = total / double(cfg.determinizations) + 1e-4 * basicScore(search[a], pub.handCount);
    }
    std::vector<size_t> order(search.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return values[a] > values[b]; });
    double best = values[order[0]];
    std::vector<size_t> tied;
    for (size_t i : order) if (isClose(values[i], best)) tied.push_back(i);
    return search[tied[rng.u32(uint32_t(tied.size()))]];
  }

  // One search per (state, turn), shared by the declaration poll and the ask.
  void decide(const PublicState& pub) {
    uint64_t handsKey = 0;
    for (int p = 0; p < NPLAY; p++) handsKey = handsKey * 61 + pub.handCount[p];
    if (cachedEvents == pub.nEvents && cachedHands == handsKey) return;
    cachedEvents = pub.nEvents; cachedHands = handsKey;
    cachedIsClaim = false;
    if (!bel.rebuild(seat, myHand, pub.handCount, pub.setActive, pub.history, deckSets, cfg.particles)) {
      cachedAction = Action{};
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, myHand, seat, buf);
      if (n) { cachedAction.card = buf[0].card; cachedAction.target = buf[0].target; }
      return;
    }
    // KV's claim timing is TURN_ONLY and a cardless player may not claim.
    bool claimsAllowed = (pub.turn == seat) && pub.handCount[seat] > 0;
    std::vector<Action> candidates = candidateActions(pub, claimsAllowed);
    if (candidates.empty()) { cachedAction = Action{}; return; }
    cachedAction = chooseAction(pub, candidates);
    cachedIsClaim = cachedAction.claim;
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (pub.turn != seat || !pub.handCount[seat]) return false;   // TURN_ONLY
    decide(pub);
    if (!cachedIsClaim) return false;
    d.set = cachedAction.set;
    for (int i = 0; i < SETSZ; i++) d.owner[i] = cachedAction.owner[i];
    conf = cachedAction.support;
    return true;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    decide(pub);
    if (!cachedIsClaim) return AskMove{cachedAction.card, cachedAction.target};
    // The poll already had its chance to take the claim; ask instead.
    std::vector<Action> asks = candidateActions(pub, false);
    if (asks.empty()) return AskMove{0, uint8_t((seat + 1) % NPLAY)};
    Action best = chooseAction(pub, asks);
    return AskMove{best.card, best.target};
  }

  // Forced endgame: the best team-consistent allocation the particles support.
  Action forcedAllocation(const PublicState& pub, int set) {
    if (!bel.rebuild(seat, myHand, pub.handCount, pub.setActive, pub.history, deckSets, cfg.particles))
      return fallbackClaim(pub);
    std::vector<std::pair<std::array<int8_t, SETSZ>, int>> tally;
    for (int i = 0; i < bel.nParticles; i++) {
      const int8_t* w = bel.particle(i);
      std::array<int8_t, SETSZ> alloc{}; bool teamOwned = true;
      for (int j = 0; j < SETSZ; j++) {
        int8_t o = w[cardOf(set, j)];
        if (o < 0 || teamOf(o) != teamOf(seat)) { teamOwned = false; break; }
        alloc[j] = o;
      }
      if (!teamOwned) continue;
      bool found = false;
      for (auto& entry : tally) if (entry.first == alloc) { entry.second++; found = true; break; }
      if (!found) tally.push_back({alloc, 1});
    }
    Action a; a.claim = true; a.set = uint8_t(set);
    if (tally.empty()) {
      int team[3]; int nt = 0;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == teamOf(seat)) team[nt++] = p;
      for (int j = 0; j < SETSZ; j++) {
        int c = cardOf(set, j);
        a.owner[j] = (myHand & bit(c)) ? uint8_t(seat) : uint8_t(team[rng.u32(uint32_t(nt))]);
      }
      a.support = 0;
      return a;
    }
    const std::pair<std::array<int8_t, SETSZ>, int>* top = &tally[0];
    for (const auto& entry : tally) if (entry.second > top->second) top = &entry;
    for (int j = 0; j < SETSZ; j++) a.owner[j] = uint8_t(top->first[j]);
    a.support = double(top->second) / double(bel.nParticles);
    return a;
  }

  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) override {
    Action a = forcedAllocation(pub, set);
    if (a.support < threshold) return false;
    d.set = uint8_t(set);
    for (int i = 0; i < SETSZ; i++) d.owner[i] = a.owner[i];
    conf = a.support;
    return true;
  }

  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    Action a = forcedAllocation(pub, set);
    d.set = uint8_t(set);
    for (int i = 0; i < SETSZ; i++) d.owner[i] = a.owner[i];
    conf = a.support;
  }
};

}  // namespace kvsearch
}  // namespace fish
