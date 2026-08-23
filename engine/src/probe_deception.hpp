// P3 -- deception and opponent-model misspecification.
//
// Three deceptive archetypes, each a V04Agent whose ONLY departure is the set
// of half-suits it is willing to ask in.  Everything else -- belief, ask
// scoring, one-ply expectimax, declaration stopping rule, forced endgame -- is
// the shipped v0.4 policy, so a measured difference is deception and not
// weakness.
//
// The restriction is implemented by temporarily hiding the agent's own cards of
// the forbidden half-suits from `k.myHand` and re-entering V04Agent::chooseAsk.
// This is exact rather than approximate because of a structural fact of
// v04.hpp: every belief quantity is read from `k.owner` / `bel.marg`, and every
// feature that consults `k.myHand` does so in the form
//     mine ? 1.0 : pTeamCard(c)
// (v04.hpp:294, v04.hpp:305, v04.hpp:352), and pTeamCard(c) is already exactly
// 1.0 for a card the agent holds because k.owner[c] == seat resolves the
// marginal.  Hiding a card therefore changes nothing except
//   (i)  ask legality in enumerateAsks (fish.hpp:186) -- the intended effect,
//   (ii) the `myHave` counts f[3], f[7], f[19] (v04.hpp:305-...) of the hidden
//        half-suits, whose asks are excluded anyway.
// The un-restricted branch calls V04Agent::chooseAsk unmodified, so an
// archetype that never deviates is bit-identical to v0.4.
#pragma once
#include "v04.hpp"
#include <atomic>

namespace fish {

// Process-wide instrumentation: how much material each archetype gives up.
struct DeceptionCost {
  std::atomic<long long> asks{0};        // ask decisions taken by an archetype
  std::atomic<long long> deviations{0};  // decisions where the archetype moved off the v0.4 pick
  std::atomic<long long> forced{0};      // constraint had to be dropped (no legal ask left)
  std::atomic<long long> pHonestMilli{0};// sum of 1000*P(hit) of the v0.4 pick, over deviations
  std::atomic<long long> pChosenMilli{0};// sum of 1000*P(hit) of the archetype pick, over deviations
  std::atomic<long long> pHonestAllMilli{0};  // ... over all decisions
  std::atomic<long long> pChosenAllMilli{0};
  void reset() {
    asks = 0; deviations = 0; forced = 0;
    pHonestMilli = 0; pChosenMilli = 0; pHonestAllMilli = 0; pChosenAllMilli = 0;
  }
};
inline DeceptionCost& decCost() { static DeceptionCost d; return d; }

enum class DeceitStyle : int { Silent = 0, Feint = 1, Withholder = 2 };

struct DeceptiveAgent : V04Agent {
  DeceitStyle style = DeceitStyle::Silent;
  int cooldownK = 6;          // Withholder: own-ask turns of silence after being asked
  double feintTol = 0.10;     // Feint: P(hit) it will give up to plant a certificate
  double silentTol = 1.0;     // Silent/Withholder: P(hit) cap on the sacrifice (1.0 = unconditional)
  int cool[NSET] = {0,0,0,0,0,0,0,0,0};
  const char* labelStr = "silent";

  const char* name() const override { return labelStr; }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V04Agent::reset(s, hand, r, seed);
    for (int i = 0; i < NSET; i++) cool[i] = 0;
  }

  void observe(const Event& e) override {
    V04Agent::observe(e);
    if (style == DeceitStyle::Withholder && e.kind == Kind::Ask && e.target == seat) {
      // The owner's manoeuvre: I was asked in S and I still hold cards of S.
      if (k.myHand & setMask(e.set)) cool[e.set] = cooldownK;
    }
  }

  // Best v0.4 ask, restricted to half-suits outside `hide`.  Returns false when
  // the restriction leaves no legal ask at all.
  bool restrictedAsk(const PublicState& pub, uint64_t hide, AskMove& out, double& p) {
    uint64_t save = k.myHand;
    k.myHand = save & ~hide;
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    bool ok = false;
    if (n > 0) { out = V04Agent::chooseAsk(pub); p = lastAskP; ok = true; }
    k.myHand = save;
    return ok;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    AskMove honest = V04Agent::chooseAsk(pub);
    double pH = lastAskP;
    int honestSet = setOf(honest.card);
    AskMove pick = honest; double pP = pH;
    bool deviated = false, forced = false;

    if (style == DeceitStyle::Silent) {
      // Never ask in the half-suit I hold most cards of, until forced.
      int bestS = -1, bestN = 0;
      for (int s = 0; s < NSET; s++) {
        if (!pub.setActive[s]) continue;
        int c = __builtin_popcountll(k.myHand & setMask(s));
        if (c > bestN) { bestN = c; bestS = s; }
      }
      if (bestS >= 0 && bestN >= 1 && honestSet == bestS) {
        AskMove m; double p2;
        // `feintTol` doubles as a material-cost cap: at the default 1.0 the
        // silence is unconditional, at 0.10 the agent breaks silence rather
        // than sacrifice more than 0.10 of hit probability.
        if (restrictedAsk(pub, setMask(bestS), m, p2)) {
          if (p2 >= pH - silentTol) { pick = m; pP = p2; deviated = true; }
        } else forced = true;
      }
    } else if (style == DeceitStyle::Withholder) {
      uint64_t hide = 0;
      for (int s = 0; s < NSET; s++) if (cool[s] > 0) hide |= setMask(s);
      if (hide && (hide & setMask(honestSet))) {
        AskMove m; double p2;
        if (restrictedAsk(pub, hide, m, p2)) {
          if (p2 >= pH - silentTol) { pick = m; pP = p2; deviated = true; }
        } else forced = true;
      }
      for (int s = 0; s < NSET; s++) if (cool[s] > 0) cool[s]--;
    } else {  // Feint
      // Prefer a half-suit I hold exactly ONE card of: the ask emits a true C5
      // certificate ("holds another card of S") that overstates my holding.
      int mine = __builtin_popcountll(k.myHand & setMask(honestSet));
      if (mine != 1) {
        uint64_t keep = 0;
        for (int s = 0; s < NSET; s++) {
          if (!pub.setActive[s]) continue;
          if (__builtin_popcountll(k.myHand & setMask(s)) == 1) keep |= setMask(s);
        }
        if (keep) {
          uint64_t hide = ~keep;
          AskMove m; double p2;
          if (restrictedAsk(pub, hide, m, p2) && p2 >= pH - feintTol) { pick = m; pP = p2; deviated = true; }
        }
      }
    }

    auto& C = decCost();
    C.asks.fetch_add(1, std::memory_order_relaxed);
    C.pHonestAllMilli.fetch_add((long long)std::lround(1000 * pH), std::memory_order_relaxed);
    C.pChosenAllMilli.fetch_add((long long)std::lround(1000 * pP), std::memory_order_relaxed);
    if (forced) C.forced.fetch_add(1, std::memory_order_relaxed);
    if (deviated) {
      C.deviations.fetch_add(1, std::memory_order_relaxed);
      C.pHonestMilli.fetch_add((long long)std::lround(1000 * pH), std::memory_order_relaxed);
      C.pChosenMilli.fetch_add((long long)std::lround(1000 * pP), std::memory_order_relaxed);
    }
    lastMySet = setOf(pick.card);
    lastAskP = pP;
    return pick;
  }
};

} // namespace fish
