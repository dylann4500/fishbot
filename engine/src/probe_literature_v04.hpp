// FishBot v0.4 policy.
//
// Three departures from v0.3:
//  (1) beliefs come from the exact deal posterior (belief.hpp) rather than
//      Sinkhorn over soft ask weights;
//  (2) declarations are decided from the exact joint probability that a named
//      allocation is correct, together with the observation that a half-suit
//      wholly owned by one team can never be broken -- no opponent holds a card
//      of it, so no opponent may legally ask in it -- which makes waiting on a
//      locked half-suit risk-free;
//  (3) the ask rule scores a richer, normalised feature set whose weights are
//      fitted by self-play rather than hand-set.
#pragma once
#include "fish.hpp"
#include "belief.hpp"
#include "blockdp.hpp"
#include "game.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace fish {
namespace litp {

// ---- P-lit instrumentation -------------------------------------------------
// Records, at every declaration opportunity, which branch of declareNow decided
// and what the binding constraint was.  Nothing here is on the shipped path.
struct LitStats {
  long long opps = 0;              // proposeDeclaration calls that got past the cheap gate
  long long oppsAll = 0;           // all proposeDeclaration calls
  long long earlyReturnGate = 0;   // no candidate half-suit survived the cheap gate
  long long urgentOpps = 0;        // urgent flag true
  long long urgentByAskFloor = 0;  // urgent because bestAskProbability < askFloor
  long long urgentByOther = 0;
  long long pressOpps[3] = {0,0,0};
  long long anyOk = 0;             // >=1 half-suit passed evaluateSet (team provably owns it)
  long long noneOk = 0;            // NOT ONE half-suit could be certified -> nothing to declare
  long long declared = 0;
  // Among opportunities with >=1 ok set and NOT declared: why?
  long long blockedByValue = 0;    // declareByValue path ran and said no
  long long blockedByThresh = 0;   // urgent/threshold path said no
  // Would an infinite holding cost (vDeclare > vWait - inf) have changed it?
  long long valueBlockedButOk = 0;
  // late (nEvents>=150) slices
  long long lateOpps = 0, lateNoneOk = 0, lateAnyOk = 0, lateUrgent = 0, lateDeclared = 0;
  long long lateBlockedByValue = 0, lateBlockedByThresh = 0;
  // best pAlloc seen when >=1 ok and not declared
  double sumBestPAllocBlocked = 0; long long nBestPAllocBlocked = 0;
  void merge(const LitStats& o) {
    opps+=o.opps; oppsAll+=o.oppsAll; earlyReturnGate+=o.earlyReturnGate;
    urgentOpps+=o.urgentOpps; urgentByAskFloor+=o.urgentByAskFloor; urgentByOther+=o.urgentByOther;
    for (int i=0;i<3;i++) pressOpps[i]+=o.pressOpps[i];
    anyOk+=o.anyOk; noneOk+=o.noneOk; declared+=o.declared;
    blockedByValue+=o.blockedByValue; blockedByThresh+=o.blockedByThresh;
    valueBlockedButOk+=o.valueBlockedButOk;
    lateOpps+=o.lateOpps; lateNoneOk+=o.lateNoneOk; lateAnyOk+=o.lateAnyOk;
    lateUrgent+=o.lateUrgent; lateDeclared+=o.lateDeclared;
    lateBlockedByValue+=o.lateBlockedByValue; lateBlockedByThresh+=o.lateBlockedByThresh;
    sumBestPAllocBlocked+=o.sumBestPAllocBlocked; nBestPAllocBlocked+=o.nBestPAllocBlocked;
  }
};
inline LitStats& litStats() { static thread_local LitStats s; return s; }
inline std::vector<LitStats*>& litRegistry() { static std::vector<LitStats*> v; return v; }
inline std::mutex& litMutex() { static std::mutex m; return m; }
inline void litRegister() {
  std::lock_guard<std::mutex> g(litMutex());
  auto& r = litRegistry();
  for (auto* p : r) if (p == &litStats()) return;
  r.push_back(&litStats());
}

// Instrumentation for the declaration pre-gate audit.  The shipped policy skips
// a half-suit whose cheap capacity-only score falls below `gateTeamProb`, and
// `evaluateSet` applies a second cheap gate (`marginalGate`).  Neither is proved
// to be an upper bound on the full evaluation, so `fish gateaudit` re-runs the
// complete evaluation on every half-suit the gates reject and counts how often
// the full evaluation would have declared one.  Counters are process-wide and
// atomic because matches are threaded; they are inert unless cfg.gateAudit.
struct GateAuditCounters {
  std::atomic<long long> opportunities{0};   // declaration opportunities examined
  std::atomic<long long> setsSeen{0};        // (opportunity, live half-suit) pairs
  std::atomic<long long> setsRejected{0};    // rejected by one of the cheap gates
  std::atomic<long long> falseNegatives{0};  // rejected, but the full rule would declare
  std::atomic<long long> actionsChanged{0};  // opportunities whose chosen action differs
  std::atomic<long long> gatedDeclares{0};   // declarations the shipped path made
  std::atomic<long long> ungatedDeclares{0}; // declarations the ungated path would make
};
inline GateAuditCounters& gateAudit() { static GateAuditCounters g; return g; }

enum class BeliefMode : int { Exact = 0, ExactDisj = 1, Sinkhorn = 2, Independent = 3, Hybrid = 4, Fast = 5, Block = 6 };

static constexpr int NFEAT = 20;
static constexpr int NVFEAT = 16;

// Static evaluation of a public belief state, in units of final set differential
// scaled to [-1, 1].  Fitted by ridge regression on self-play outcomes; see the
// `fitvalue` command.  The same function drives the one-ply expectimax over asks
// and the declaration optimal-stopping test, so asking and declaring are scored
// on one consistent scale.
struct ValueAggregates {
  double sumControl = 0;      // sum_H (2 e_H - 1)
  double sharpControl = 0;
  double locked = 0;          // ours - theirs
  double contested = 0;
  int active = 0;
};

struct V04Config {
  BeliefMode belief = BeliefMode::Fast;
  int sinkOuter = 4, sinkInner = 8;
  double priorTheta = 0.26380;      // weight on "asked in this half-suit"
  double priorPhi   = 0.13280;      // weight on "took turns but never asked here"
  // Exact two-ply refinement: re-derive the posterior after the hypothetical hit
  // and miss for the leading candidates, instead of approximating the follow-up.
  bool   greedyMAP    = false;   // condition while choosing the allocation
  int    searchTopK   = 6;
  double chainWeight  = 3.35800;
  double threatWeight = 2.61270;
  int particles = 96;
  double w[NFEAT] = {
    /*0  hit probability   */   11.5060,
    /*1  squared hit       */    3.2948,
    /*2  certain hit       */    3.1978,
    /*3  own set progress  */    1.6881,
    /*4  team control      */    2.1333,
    /*5  lock completion   */    4.0705,
    /*6  continuation      */    1.4679,
    /*7  completion bonus  */    1.4281,
    /*8  reply threat      */   -3.0978,
    /*9  information leak  */   -0.8536,
    /*10 target hand size  */   -2.0219,
    /*11 empties target    */    1.1660,
    /*12 repeats set       */    1.2697,
    /*13 known team cards  */    0.9142,
    /*14 location entropy  */   -2.6534,
    /*15 team owns set     */   -0.8045,
    /*16 exposure on miss  */    1.9040,
    /*17 trailing pressure */    0.0473,
    /*18 runway            */    1.4108,
    /*19 leak magnitude    */   -0.9990,
  };
  // declaration
  double declThreshold      = 0.81770;   // unlocked half-suits: P(allocation correct)
  double lockedAllocThresh  = 0.79690;  // locked half-suits: only near-certainty
  double minTeamProb        = 0.79250;   // exact P(team owns the half-suit) floor
  bool   patientLocked      = true;  // delay locked half-suits (information hiding)
  double askFloor           = 0.33250;   // no productive ask left -> cash everything
  int    patiencePool       = 6;     // unresolved cards below which we stop waiting
  int    forceDeclareEvents = 220;   // hard termination guarantee
  double oppCardFloor       = 2.86760;     // opponents nearly out -> cash
  // one-ply expectimax over the learned value function
  bool   useValue           = true;
  double valueWeight        = 6.04320;   // scale of EV against the linear ask score
  double linearWeight       = 0.76670;
  bool   valueDeclare       = true;  // decide declarations by EV, not a threshold
  double declareMargin      = -0.03420;   // EV edge required to cash a half-suit
  // Ridge-fitted on self-play decision points (see fitvalue); the coefficients
  // say that a card-count lead and expected half-suit control are each worth
  // roughly half of a scored half-suit, and that holding the turn is worth about
  // four tenths of a half-suit.
  double vw[NVFEAT] = {
     0.001242,   // bias
     0.888965,   // score differential
     0.421266,   // expected control
    -0.145791,   // sharpened control
     0.225573,   // locked differential
     0.022896,   // side to move
     0.422207,   // card differential
     0.007678,   // unresolved pool
     0.005904,   // active half-suits
    -0.000997,   // turn x control
    -0.006601,   // my hand size
    -0.007472,   // smallest friendly hand
    -0.022484,   // our near-complete half-suits
    -0.025189,   // their near-complete half-suits
     0.080416,   // contested mass
    -0.021409,   // turn x unresolved
  };
  double gateTeamProb       = .008;  // loose capacity-only pre-filter
  double marginalGate       = .008;  // loose marginal pre-filter
  bool   declareEnabled     = true;
  bool   gateAudit          = false;  // diagnostic only; see `fish gateaudit`
  // P-lit scratch knobs (not in the shipped config)
  int    litLiveOnly        = 0;     // 1: never choose a PROVABLY dead ask when a live one exists
  double litTimeCost        = 0.0;   // holding cost per forcing-horizon unit added to declareByValue
};

inline double binEnt(double p) {
  if (p <= 0 || p >= 1) return 0;
  return -p * std::log2(p) - (1 - p) * std::log2(1 - p);
}

struct V04Agent : Agent {
  V04Config cfg;
  Belief bel;
  BlockDP block;
  bool blockOk = false;
  Rng rng;
  bool dirty = true;
  int lastMySet = -1;
  int teamMask = 0, oppMask = 0;
  double lastAskP = -1;      // calibration: forecast attached to the last ask
  const char* label = "fishbot_v04";

  const char* name() const override { return label; }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    rng = Rng(mixSeed(seed, 0x0404ull));
    dirty = true; lastMySet = -1; lastAskP = -1;
    teamMask = 0; oppMask = 0;
    for (int p = 0; p < NPLAY; p++) { if (teamOf(p) == teamOf(s)) teamMask |= 1 << p; else oppMask |= 1 << p; }
  }
  void observe(const Event& e) override { Agent::observe(e); dirty = true; }

  void refresh() {
    if (!dirty) return;
    bel.dpBuilt = false;
    blockOk = false;
    if (cfg.belief == BeliefMode::Block) {
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) bel.marg[c][p] = 0;
      for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) bel.marg[c][k.owner[c]] = 1;
      blockOk = block.build(k);
      if (blockOk) block.marginals(bel.marg);
      else bel.sinkhornDisj(k, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
      bel.dpOk = false; bel.nParticles = 0; bel.k = &k;
      dirty = false;
      return;
    }
    if (cfg.belief == BeliefMode::Independent) {
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) bel.marg[c][p] = 0;
      for (int c = 0; c < NCARD; c++) {
        if (k.owner[c] < NPLAY) { bel.marg[c][k.owner[c]] = 1; continue; }
        if (k.owner[c] == OUT_OF_PLAY) continue;
        int n = __builtin_popcount(k.mask[c]);
        for (int p = 0; p < NPLAY; p++) if (k.mask[c] & (1u << p)) bel.marg[c][p] = 1.0 / n;
      }
      bel.dpOk = false; bel.nParticles = 0; bel.k = &k;
    } else if (cfg.belief == BeliefMode::Fast) {
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) bel.marg[c][p] = 0;
      for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) bel.marg[c][k.owner[c]] = 1;
      bel.sinkhornDisj(k, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
      bel.dpOk = false; bel.nParticles = 0; bel.k = &k;
    } else if (cfg.belief == BeliefMode::Sinkhorn || cfg.belief == BeliefMode::Hybrid) {
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) bel.marg[c][p] = 0;
      for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) bel.marg[c][k.owner[c]] = 1;
      bel.sinkhorn(k); bel.dpOk = false; bel.nParticles = 0; bel.k = &k;
    } else {
      bel.compute(k, rng, cfg.particles, cfg.belief == BeliefMode::ExactDisj);
    }
    dirty = false;
  }

  inline double pTeamCard(int c) const {
    double s = 0;
    for (int p = 0; p < NPLAY; p++) if (teamMask & (1 << p)) s += bel.marg[c][p];
    return s;
  }

  // One-ply threat this opponent poses if the turn is handed over: the best ask
  // it could make against our team, weighted by the chance it can legally make
  // it at all.
  double threatOf(const PublicState& pub, int t) const {
    double best = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      double none = 1, bestCard = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        double pt = bel.marg[c][t];
        none *= (1 - pt);
        double friendly = pTeamCard(c);
        bestCard = std::max(bestCard, friendly * (1 - pt));
      }
      double canAsk = 1 - none;
      double activity = std::min(1.0, k.askCount[t][s] / 3.0);
      best = std::max(best, canAsk * bestCard * (0.7 + 0.3 * activity));
    }
    return best;
  }

  double exposureOf(const PublicState& pub, int t) const {
    double e = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s] || !k.askCount[t][s]) continue;
      for (int i = 0; i < SETSZ; i++) e += pTeamCard(cardOf(s, i));
    }
    return std::min(1.0, e / 12.0);
  }

  // Has my team already told the table it is interested in this half-suit?  This
  // must be judged on PUBLIC information only: my own hand is not public, so a
  // card of the half-suit sitting in my hand reveals nothing to anybody.
  bool teamRevealedSet(int s) const {
    for (int p = 0; p < NPLAY; p++) if ((teamMask & (1 << p)) && k.askCount[p][s]) return true;
    uint64_t pub2 = k.publicKnown & setMask(s);
    while (pub2) { int c = __builtin_ctzll(pub2); pub2 &= pub2 - 1;
      if (k.owner[c] < NPLAY && (teamMask & (1u << k.owner[c]))) return true; }
    return false;
  }

  // Runway: how many consecutive takes this ask plausibly starts.  A hit keeps
  // the turn, so the value of an ask is not one card but the expected length of
  // the run it begins.  We approximate the continuation by the best remaining
  // per-card hit probabilities, which cannot open new half-suits (a legal ask
  // already requires a card of that half-suit in hand).
  double bestPerCard[NCARD];
  double runway[4];
  int nRunway = 0;

  void prepareRunway(const PublicState& pub) {
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    for (int i = 0; i < NCARD; i++) bestPerCard[i] = -1;
    for (int i = 0; i < n; i++) {
      double p = bel.marg[buf[i].card][buf[i].target];
      if (p > bestPerCard[buf[i].card]) bestPerCard[buf[i].card] = p;
    }
    double top[8]; int m = 0;
    for (int c = 0; c < NCARD; c++) if (bestPerCard[c] >= 0) {
      if (m < 8) { top[m++] = bestPerCard[c]; std::sort(top, top + m, std::greater<double>()); }
      else if (bestPerCard[c] > top[7]) { top[7] = bestPerCard[c]; std::sort(top, top + 8, std::greater<double>()); }
    }
    nRunway = std::min(4, m);
    for (int i = 0; i < 4; i++) runway[i] = i < nRunway ? top[i] : 0.0;
  }

  double expectedRun(int card, double p) const {
    double q[4]; int m = 0;
    for (int i = 0; i < nRunway && m < 3; i++) {
      if (bestPerCard[card] >= 0 && std::fabs(runway[i] - bestPerCard[card]) < 1e-12 && m == 0 && i == 0) continue;
      q[m++] = runway[i];
    }
    double tail = 0;
    for (int i = m - 1; i >= 0; i--) tail = q[i] * (1.0 + tail);
    return p * (1.0 + tail) / 4.0;
  }

  void features(const PublicState& pub, int card, int target, double* f) const {
    int S = setOf(card);
    double p = bel.marg[card][target];
    int myHave = 0, teamKnown = 0;
    double teamExp = 0, pTeamOther = 1, continuation = 0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(S, i);
      bool mine = (k.myHand & bit(c)) != 0;
      if (mine) myHave++;
      double pt = mine ? 1.0 : pTeamCard(c);
      teamExp += pt;
      if (c != card) pTeamOther *= pt;
      if (c != card && !mine) {
        double bestOpp = 0;
        for (int u = 0; u < NPLAY; u++) if (oppMask & (1 << u)) bestOpp = std::max(bestOpp, bel.marg[c][u]);
        continuation = std::max(continuation, bestOpp);
      }
      if (!mine && k.owner[c] < NPLAY && (teamMask & (1u << k.owner[c]))) teamKnown++;
    }
    double pTeamAll = 1;
    for (int i = 0; i < SETSZ; i++) { int c = cardOf(S, i);
      pTeamAll *= (k.myHand & bit(c)) ? 1.0 : pTeamCard(c); }
    int lead = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    f[0]  = p;
    f[1]  = p * p;
    f[2]  = p > .9995 ? 1 : 0;
    f[3]  = myHave / 6.0;
    f[4]  = teamExp / 6.0;
    f[5]  = pTeamOther;
    f[6]  = continuation;
    f[7]  = myHave >= 4 ? 1.0 : (myHave == 3 ? .35 : 0.0);
    f[8]  = (1 - p) * threatOf(pub, target);
    f[9]  = teamRevealedSet(S) ? 0.0 : 1.0;
    f[10] = pub.handCount[target] / 9.0;
    f[11] = (pub.handCount[target] == 1) ? p : 0.0;
    f[12] = (lastMySet == S) ? 1.0 : 0.0;
    f[13] = teamKnown / 6.0;
    f[14] = binEnt(p);
    f[15] = pTeamAll;
    f[16] = (1 - p) * exposureOf(pub, target);
    f[17] = (lead <= -2 ? 1.0 : 0.0) * p;
    f[18] = expectedRun(card, p);
    f[19] = (teamRevealedSet(S) ? 0.0 : 1.0) * (myHave / 6.0);
  }

  // ---- learned value function -------------------------------------------
  double eH[NSET];              // expected fraction of half-suit H held by my team
  ValueAggregates agg;
  int myCards = 0, ourCards = 0, theirCards = 0, minFriendly = 0, unresolvedN = 0;

  static inline double sharp(double e) {
    double x = (e - 0.35) / 0.30;
    if (x < 0) x = 0; else if (x > 1) x = 1;
    return 2 * x - 1;
  }

  void computeAggregates(const PublicState& pub) {
    agg = ValueAggregates{};
    unresolvedN = __builtin_popcountll(k.unresolved);
    ourCards = theirCards = 0; minFriendly = 99;
    for (int p = 0; p < NPLAY; p++) {
      if (teamMask & (1 << p)) { ourCards += pub.handCount[p]; minFriendly = std::min(minFriendly, int(pub.handCount[p])); }
      else theirCards += pub.handCount[p];
    }
    myCards = pub.handCount[seat];
    for (int s = 0; s < NSET; s++) {
      eH[s] = 0;
      if (!pub.setActive[s]) continue;
      agg.active++;
      double e = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        e += (k.myHand & bit(c)) ? 1.0 : pTeamCard(c);
      }
      e /= SETSZ;
      eH[s] = e;
      agg.sumControl += 2 * e - 1;
      agg.sharpControl += sharp(e);
      if (e > .995) agg.locked += 1; else if (e < .005) agg.locked -= 1;
      agg.contested += e * (1 - e);
    }
  }

  // Evaluate from cached aggregates, applying a hypothetical perturbation.
  double value(const PublicState& pub, double dControl, double dSharp, double dLocked,
               double dContested, int scoreDiff, int turnSign, int dOur, int dTheir,
               int dUnresolved, int dActive) const {
    const double* w = cfg.vw;
    double control = agg.sumControl + dControl;
    double sharpc = agg.sharpControl + dSharp;
    double locked = agg.locked + dLocked;
    double contested = agg.contested + dContested;
    int active = agg.active + dActive;
    double f[NVFEAT];
    f[0] = 1.0;
    f[1] = scoreDiff / 9.0;
    f[2] = control / 9.0;
    f[3] = sharpc / 9.0;
    f[4] = locked / 9.0;
    f[5] = double(turnSign);
    f[6] = double(ourCards + dOur - theirCards - dTheir) / 54.0;
    f[7] = double(unresolvedN + dUnresolved) / 45.0;
    f[8] = double(active) / 9.0;
    f[9] = double(turnSign) * control / 9.0;
    f[10] = double(myCards) / 9.0;
    f[11] = double(minFriendly) / 9.0;
    f[12] = 0; f[13] = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      if (eH[s] >= .5 && eH[s] <= .995) f[12] += 1.0 / 9.0;
      if (eH[s] <= .5 && eH[s] >= .005) f[13] += 1.0 / 9.0;
    }
    f[14] = contested / 9.0;
    f[15] = double(turnSign) * double(unresolvedN + dUnresolved) / 45.0;
    double v = 0;
    for (int i = 0; i < NVFEAT; i++) v += w[i] * f[i];
    return v;
  }

  void stateFeatures(const PublicState& pub, double* f) {
    computeAggregates(pub);
    int scoreDiff = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    int turnSign = (teamOf(pub.turn) == teamOf(seat)) ? 1 : -1;
    f[0] = 1.0;
    f[1] = scoreDiff / 9.0;
    f[2] = agg.sumControl / 9.0;
    f[3] = agg.sharpControl / 9.0;
    f[4] = agg.locked / 9.0;
    f[5] = double(turnSign);
    f[6] = double(ourCards - theirCards) / 54.0;
    f[7] = double(unresolvedN) / 45.0;
    f[8] = double(agg.active) / 9.0;
    f[9] = double(turnSign) * agg.sumControl / 9.0;
    f[10] = double(myCards) / 9.0;
    f[11] = double(minFriendly) / 9.0;
    f[12] = 0; f[13] = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      if (eH[s] >= .5 && eH[s] <= .995) f[12] += 1.0 / 9.0;
      if (eH[s] <= .5 && eH[s] >= .005) f[13] += 1.0 / 9.0;
    }
    f[14] = agg.contested / 9.0;
    f[15] = double(turnSign) * double(unresolvedN) / 45.0;
  }

  // One-ply expectimax for an ask.  A hit moves the card to my hand and keeps
  // the turn; a miss excludes the target (which re-normalises that card's
  // ownership among the remaining candidates) and hands the turn over.
  double askExpectedValue(const PublicState& pub, int card, int target, double p) const {
    (void)target;
    int S = setOf(card);
    int scoreDiff = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    double pt = pTeamCard(card);
    double eOld = eH[S];
    // hit: the card becomes certainly ours
    double eHit = eOld + (1.0 - pt) / SETSZ;
    // miss: renormalise away the target's mass
    double denom = std::max(1e-6, 1.0 - p);
    double ptMiss = std::min(1.0, pt / denom);
    double eMiss = eOld + (ptMiss - pt) / SETSZ;
    auto delta = [&](double eNew) {
      double dC = (2 * eNew - 1) - (2 * eOld - 1);
      double dS = sharp(eNew) - sharp(eOld);
      double dL = (eNew > .995 ? 1.0 : eNew < .005 ? -1.0 : 0.0) - (eOld > .995 ? 1.0 : eOld < .005 ? -1.0 : 0.0);
      double dK = eNew * (1 - eNew) - eOld * (1 - eOld);
      return std::array<double, 4>{dC, dS, dL, dK};
    };
    auto dh = delta(eHit), dm = delta(eMiss);
    double vHit = value(pub, dh[0], dh[1], dh[2], dh[3], scoreDiff, +1, +1, -1, -1, 0);
    double vMiss = value(pub, dm[0], dm[1], dm[2], dm[3], scoreDiff, -1, 0, 0, 0, 0);
    return p * vHit + (1 - p) * vMiss;
  }

  double lastAskForecast() const override { return lastAskP; }

  int valueFeatures(const PublicState& pub, double* f) override {
    refresh();
    stateFeatures(pub, f);
    return NVFEAT;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    refresh();
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return AskMove{0, 0};
    if (cfg.litLiveOnly) {
      // Drop every ask the actor can PROVE is a miss, using exactly diag.hpp's
      // definition of "dead".  If they are all dead the position is starved and
      // the full list is kept.
      AskMove live[NSET * SETSZ * 3]; int m = 0;
      for (int i = 0; i < n; i++) {
        int c = buf[i].card, t = buf[i].target;
        bool dead = (k.owner[c] < NPLAY) ? (k.owner[c] != t) : !(k.mask[c] & (1u << t));
        if (!dead) live[m++] = buf[i];
      }
      if (m) { for (int i = 0; i < m; i++) buf[i] = live[i]; n = m; }
    }
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    double bestScore = -1e18; AskMove best = buf[0]; double bestP = 0;
    double f[NFEAT];
    for (int i = 0; i < n; i++) {
      features(pub, buf[i].card, buf[i].target, f);
      double u = 0;
      for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
      u *= cfg.linearWeight;
      if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
      if (u > bestScore) { bestScore = u; best = buf[i]; bestP = f[0]; }
    }
    if (cfg.searchTopK > 1) {
      // Rank, keep the leaders, and re-score them with an exact posterior update
      // on each branch.  A hit resolves the card to us and keeps the turn; a
      // miss excludes the target, which redistributes that card's ownership.
      struct Cand { double u; int idx; };
      Cand cs[NSET * SETSZ * 3];
      for (int i = 0; i < n; i++) {
        features(pub, buf[i].card, buf[i].target, f);
        double u = 0;
        for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
        u *= cfg.linearWeight;
        if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
        cs[i] = Cand{u, i};
      }
      int K = std::min(cfg.searchTopK, n);
      std::partial_sort(cs, cs + K, cs + n, [](const Cand& x, const Cand& y) { return x.u > y.u; });
      double best2 = -1e18; AskMove pick = buf[cs[0].idx]; double pickP = 0;
      for (int r = 0; r < K; r++) {
        int i = cs[r].idx;
        int card = buf[i].card, target = buf[i].target;
        double p = bel.marg[card][target];
        double follow = 0, threat = 0;
        if (cfg.chainWeight != 0 && p > 0.02) {
          Knowledge kh = k;
          kh.setOwner(card, target);
          kh.owner[card] = uint8_t(seat); kh.mask[card] = uint8_t(1u << seat);
          kh.myHand |= bit(card);
          kh.handCount[seat]++; kh.handCount[target]--;
          kh.propagateCapacity();
          Belief bh;
          for (int c2 = 0; c2 < NCARD; c2++) for (int q = 0; q < NPLAY; q++) bh.marg[c2][q] = 0;
          for (int c2 = 0; c2 < NCARD; c2++) if (kh.owner[c2] < NPLAY) bh.marg[c2][kh.owner[c2]] = 1;
          bh.sinkhornDisj(kh, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
          PublicState ph = pub;
          ph.handCount[seat]++; ph.handCount[target]--;
          AskMove b2[NSET * SETSZ * 3];
          int n2 = enumerateAsks(ph, kh.myHand, seat, b2);
          for (int j = 0; j < n2; j++) follow = std::max(follow, bh.marg[b2[j].card][b2[j].target]);
        }
        if (cfg.threatWeight != 0 && p < 0.98) {
          Knowledge km = k;
          km.exclude(card, target);
          km.propagateCapacity();
          Belief bm;
          for (int c2 = 0; c2 < NCARD; c2++) for (int q = 0; q < NPLAY; q++) bm.marg[c2][q] = 0;
          for (int c2 = 0; c2 < NCARD; c2++) if (km.owner[c2] < NPLAY) bm.marg[c2][km.owner[c2]] = 1;
          bm.sinkhornDisj(km, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
          for (int st = 0; st < NSET; st++) {
            if (!pub.setActive[st]) continue;
            double none = 1, bestCard = 0;
            for (int j = 0; j < SETSZ; j++) {
              int c2 = cardOf(st, j);
              double pt = bm.marg[c2][target];
              none *= (1 - pt);
              double fr = 0;
              for (int q = 0; q < NPLAY; q++) if (teamMask & (1 << q)) fr += bm.marg[c2][q];
              bestCard = std::max(bestCard, fr);
            }
            threat = std::max(threat, (1 - none) * bestCard);
          }
        }
        double u = cs[r].u + cfg.chainWeight * p * follow - cfg.threatWeight * (1 - p) * threat;
        if (u > best2) { best2 = u; pick = buf[i]; pickP = p; }
      }
      lastMySet = setOf(pick.card);
      lastAskP = pickP;
      return pick;
    }
    lastMySet = setOf(best.card);
    lastAskP = bestP;
    return best;
  }

  // Highest posterior legal ask value available to me right now.
  double bestAskProbability(const PublicState& pub) {
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    double best = 0;
    for (int i = 0; i < n; i++) best = std::max(best, bel.marg[buf[i].card][buf[i].target]);
    return n ? best : 0.0;
  }

  struct SetVerdict { bool ok = false; double pTeam = 0, pAlloc = 0; Declaration decl; };

  // Theorem 1 makes patience correct, and two patient policies can therefore
  // freeze the position permanently: once no productive ask remains, no further
  // information can arrive, so a half-suit whose allocation is unresolved stays
  // unresolved forever.  Termination has to come from the policy.  We escalate:
  // at ordinary pressure the stopping rule decides; past the forcing horizon we
  // cash anything better than a coin flip; past twice that we cash the best
  // candidate whatever it is, because an unclaimed half-suit scores nothing.
  int pressure(const PublicState& pub) const {
    // A half-suit our team provably owns but cannot allocate is, by Theorem 1,
    // safe forever -- and for exactly the same reason no information about it
    // can ever arrive, since every ask in it is a guaranteed miss.  Waiting is
    // then not merely unprofitable but permanent, so the policy has to break the
    // deadlock itself.  We escalate on the public event count rather than on a
    // dead-ask test, because the dead-ask condition also fires in ordinary
    // early positions where waiting is still correct.
    if (pub.nEvents >= (7 * cfg.forceDeclareEvents) / 5) return 2;
    if (pub.nEvents >= cfg.forceDeclareEvents) return 1;
    return 0;
  }

  SetVerdict evaluateSet(const PublicState& pub, int s, int press = 0, bool ignoreGates = false) {
    SetVerdict v{};
    v.decl.set = uint8_t(s);
    uint8_t allow[SETSZ]; int cards[SETSZ], players[SETSZ];
    double cheap = 1;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i);
      if (k.owner[c] < NPLAY && !(teamMask & (1u << k.owner[c]))) return v;   // opponent holds it
      if (!(k.mask[c] & teamMask) && k.owner[c] >= NPLAY) return v;
      cards[i] = c; allow[i] = uint8_t(teamMask);
      cheap *= (k.myHand & bit(c)) ? 1.0 : pTeamCard(c);
      int bestQ = seat; double bestP = -1;
      for (int q = 0; q < NPLAY; q++) if (teamMask & (1 << q)) {
        double pr = (k.myHand & bit(c)) ? (q == seat ? 1.0 : 0.0) : bel.marg[c][q];
        if (pr > bestP) { bestP = pr; bestQ = q; }
      }
      players[i] = bestQ;
      v.decl.owner[i] = uint8_t(bestQ);
    }
    double teamFloor = press >= 2 ? 0.0 : (press >= 1 ? 0.25 : cfg.minTeamProb);
    if (!ignoreGates && cheap < (press >= 2 ? 0.0 : cfg.marginalGate)) return v;
    if (cfg.belief == BeliefMode::Block && blockOk) {
      // Exact: probability that every unresolved card of the half-suit sits with
      // my team, and the exact probability of the best legal allocation.
      v.pTeam = block.teamOwnsProbability(s, teamMask);
      if (v.pTeam < teamFloor) return v;
      int outCards[SETSZ], outSeats[SETSZ], n2 = 0;
      double pa = block.bestTeamAllocation(s, teamMask, outCards, outSeats, n2);
      for (int i = 0; i < n2; i++) v.decl.owner[idxIn(outCards[i])] = uint8_t(outSeats[i]);
      v.pAlloc = pa;
      v.ok = true;
      return v;
    }
    if (cfg.belief == BeliefMode::Fast) {
      v.pTeam = cheap;
      if (cfg.greedyMAP) {
        int chosen[SETSZ];
        v.pAlloc = bel.jointSequentialMAP(k, cards, SETSZ, teamMask, chosen,
                                          cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
        for (int i = 0; i < SETSZ; i++) v.decl.owner[i] = uint8_t(chosen[i]);
      } else {
        v.pAlloc = bel.jointSequential(k, cards, players, SETSZ, cfg.sinkOuter, cfg.sinkInner,
                                       cfg.priorTheta, cfg.priorPhi);
      }
      v.pTeam = std::max(v.pTeam, v.pAlloc);
      if (v.pTeam < teamFloor) return v;
      v.ok = true;
      return v;
    }
    bool exact = cfg.belief != BeliefMode::Sinkhorn && cfg.belief != BeliefMode::Independent;
    if (exact) bel.ensureDP(k, rng, cfg.particles, cfg.belief == BeliefMode::ExactDisj);
    if (!bel.dpOk) { v.pTeam = cheap; v.pAlloc = cheap; v.ok = true; return v; }
    v.pTeam = bel.dp.countWithMasks(cards, allow, SETSZ) / bel.dp.N;
    if (v.pTeam < teamFloor) return v;
    v.pAlloc = bel.jointProbability(k, cards, players, SETSZ, cfg.belief == BeliefMode::ExactDisj);
    v.ok = true;
    (void)pub;
    return v;
  }

  // Optimal stopping: cash the half-suit only when doing so beats the value of
  // holding it.  A half-suit provably held by one team cannot be taken back --
  // the opponents hold no card of it and therefore may never legally ask in it
  // -- so C_steal = 0 and waiting is risk-free; the value function is what
  // decides whether the information the declaration leaks is worth the point.
  bool declareByValue(const PublicState& pub, const SetVerdict& v) {
    int S = v.decl.set;
    int scoreDiff = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    int turnSign = (teamOf(pub.turn) == teamOf(seat)) ? 1 : -1;
    double eOld = eH[S];
    double dC = -(2 * eOld - 1), dS = -sharp(eOld);
    double dL = -(eOld > .995 ? 1.0 : eOld < .005 ? -1.0 : 0.0);
    double dK = -eOld * (1 - eOld);
    int dUnres = 0;
    uint64_t u = k.unresolved & setMask(S);
    dUnres = -__builtin_popcountll(u);
    int mine = __builtin_popcountll(k.myHand & setMask(S));
    double vRight = value(pub, dC, dS, dL, dK, scoreDiff + 1, turnSign, -SETSZ + 0, 0, dUnres, -1);
    double vWrong = value(pub, dC, dS, dL, dK, scoreDiff - 1, turnSign, -SETSZ + 0, 0, dUnres, -1);
    (void)mine;
    double vDeclare = v.pAlloc * vRight + (1 - v.pAlloc) * vWrong;
    double vWait = value(pub, 0, 0, 0, 0, scoreDiff, turnSign, 0, 0, 0, 0);
    // P-lit: the time/holding-cost feature the claim says is missing.
    vWait -= cfg.litTimeCost * double(pub.nEvents) / double(cfg.forceDeclareEvents);
    return vDeclare > vWait + cfg.declareMargin;
  }

  bool declareNow(const PublicState& pub, const SetVerdict& v, bool urgent, int press) {
    bool locked = v.pTeam > .9995;
    if (press >= 2) return true;
    if (press >= 1 && v.pAlloc >= 0.5) return true;
    if (cfg.useValue && cfg.valueDeclare) {
      if (urgent) return v.pAlloc >= cfg.declThreshold || (locked && v.pAlloc >= 0.5);
      return declareByValue(pub, v);
    }
    if (locked) {
      if (v.pAlloc >= cfg.lockedAllocThresh && (!cfg.patientLocked || urgent)) return true;
      if (urgent && v.pAlloc >= cfg.declThreshold) return true;
      return false;
    }
    return v.pAlloc >= cfg.declThreshold;
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    LitStats& LS = litStats(); litRegister();
    bool LATE = pub.nEvents >= 150;
    LS.oppsAll++; if (LATE) LS.lateOpps++;
    if (!cfg.declareEnabled) return false;
    if (!pub.rules.cardlessMayDeclare && !pub.handCount[seat]) return false;
    // Capacity-only gate first: the exact posterior is far too costly to build
    // for six observers after every public event, and a half-suit that cannot
    // plausibly be wholly ours can never clear the declaration bar.
    int unresolvedCount = __builtin_popcountll(k.unresolved);
    int press = pressure(pub);
    bool bypass = unresolvedCount <= 8 || press >= 1;
    if (cfg.useValue) computeAggregates(pub);
    bool candidate = bypass;
    for (int s = 0; s < NSET && !candidate; s++) {
      if (!pub.setActive[s]) continue;
      if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) candidate = true;
    }
    if (!candidate && !cfg.gateAudit) { LS.earlyReturnGate++; return false; }
    refresh();
    int oppCards = 0;
    for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
    double LbestAsk = bestAskProbability(pub);
    bool LurgOther = unresolvedCount <= cfg.patiencePool
               || oppCards <= cfg.oppCardFloor
               || pub.nEvents >= cfg.forceDeclareEvents;
    bool LurgFloor = LbestAsk < cfg.askFloor;
    bool urgent = LurgOther || LurgFloor;
    LS.opps++;
    LS.pressOpps[press < 3 ? press : 2]++;
    if (urgent) { LS.urgentOpps++; if (LATE) LS.lateUrgent++;
      if (LurgFloor && !LurgOther) LS.urgentByAskFloor++; else LS.urgentByOther++; }
    int LnOk = 0; double LbestOkAlloc = -1; bool LanyValueSaysYes = false; bool LanyOkAtAll = false;
    double bestConf = -1; bool found = false;
    double auditBestConf = -1; bool auditFound = false; int auditBestSet = -1;
    long long localSeen = 0, localRejected = 0, localFalseNeg = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      bool passesGate = bypass || k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb;
      if (cfg.gateAudit) {
        // The ungated evaluation: full posterior query on every live half-suit,
        // with both cheap gates disabled, followed by the same stopping rule.
        localSeen++;
        SetVerdict vu = evaluateSet(pub, s, press, true);
        bool wouldDeclare = vu.ok && declareNow(pub, vu, urgent, press);
        if (wouldDeclare && vu.pAlloc > auditBestConf) { auditBestConf = vu.pAlloc; auditBestSet = s; auditFound = true; }
        bool rejected = !candidate || !passesGate
                     || (!evaluateSet(pub, s, press).ok && vu.ok);
        if (rejected) { localRejected++; if (wouldDeclare) localFalseNeg++; }
      }
      if (!candidate) continue;
      if (!passesGate) continue;
      SetVerdict v = evaluateSet(pub, s, press);
      if (!v.ok) continue;
      LnOk++; LanyOkAtAll = true;
      if (v.pAlloc > LbestOkAlloc) LbestOkAlloc = v.pAlloc;
      if (cfg.useValue && cfg.valueDeclare && !urgent && press < 1 && declareByValue(pub, v))
        LanyValueSaysYes = true;
      if (!declareNow(pub, v, urgent, press)) continue;
      if (v.pAlloc > bestConf) { bestConf = v.pAlloc; d = v.decl; found = true; }
    }
    if (cfg.gateAudit) {
      auto& G = gateAudit();
      G.opportunities.fetch_add(1, std::memory_order_relaxed);
      G.setsSeen.fetch_add(localSeen, std::memory_order_relaxed);
      G.setsRejected.fetch_add(localRejected, std::memory_order_relaxed);
      G.falseNegatives.fetch_add(localFalseNeg, std::memory_order_relaxed);
      if (found) G.gatedDeclares.fetch_add(1, std::memory_order_relaxed);
      if (auditFound) G.ungatedDeclares.fetch_add(1, std::memory_order_relaxed);
      bool changed = (found != auditFound) || (found && auditFound && int(d.set) != auditBestSet);
      if (changed) G.actionsChanged.fetch_add(1, std::memory_order_relaxed);
    }
    if (LanyOkAtAll) { LS.anyOk++; if (LATE) LS.lateAnyOk++; }
    else { LS.noneOk++; if (LATE) LS.lateNoneOk++; }
    if (found) { LS.declared++; if (LATE) LS.lateDeclared++; }
    else if (LanyOkAtAll) {
      LS.sumBestPAllocBlocked += LbestOkAlloc; LS.nBestPAllocBlocked++;
      if (cfg.useValue && cfg.valueDeclare && !urgent && press < 1) {
        LS.blockedByValue++; if (LATE) LS.lateBlockedByValue++;
        if (!LanyValueSaysYes) LS.valueBlockedButOk++;
      } else { LS.blockedByThresh++; if (LATE) LS.lateBlockedByThresh++; }
    }
    (void)LnOk;
    conf = bestConf;
    return found;
  }

  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) override {
    refresh();
    SetVerdict v = evaluateSet(pub, set, 2);
    if (!v.ok || v.pAlloc < threshold) return false;
    d = v.decl; conf = v.pAlloc;
    return true;
  }
  // Forced declaration: somebody on this team must name an allocation, so we
  // always produce our best one rather than falling through a gate.  Cards we
  // hold are ours; cards publicly located on our team are that teammate's; the
  // rest go to the most likely teammate under the current posterior.
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    refresh();
    d.set = uint8_t(set);
    int cards[SETSZ], players[SETSZ];
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(set, i);
      cards[i] = c;
      int chosen = seat; double best = -1;
      if (k.myHand & bit(c)) chosen = seat;
      else if (k.owner[c] < NPLAY && (teamMask & (1u << k.owner[c]))) chosen = k.owner[c];
      else {
        for (int q = 0; q < NPLAY; q++) {
          if (!(teamMask & (1 << q))) continue;
          double pr = bel.marg[c][q];
          if (pr > best) { best = pr; chosen = q; }
        }
      }
      players[i] = chosen;
      d.owner[i] = uint8_t(chosen);
    }
    if (cfg.belief == BeliefMode::Block && blockOk) {
      int outCards[SETSZ], outSeats[SETSZ], n2 = 0;
      double pa = block.bestTeamAllocation(set, teamMask, outCards, outSeats, n2);
      if (pa > 0) {
        for (int i = 0; i < n2; i++) d.owner[idxIn(outCards[i])] = uint8_t(outSeats[i]);
        conf = pa;
        return;
      }
    }
    conf = bel.jointSequential(k, cards, players, SETSZ, cfg.sinkOuter, cfg.sinkInner,
                               cfg.priorTheta, cfg.priorPhi);
    (void)pub;
  }

  // Hand the turn to the teammate with the strongest publicly-estimable ask.
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    refresh();
    int best = cand[0]; double bestV = -1;
    for (int i = 0; i < n; i++) {
      int u = cand[i];
      double v = 0;
      for (int s = 0; s < NSET; s++) {
        if (!pub.setActive[s]) continue;
        double none = 1, bestCard = 0;
        for (int j = 0; j < SETSZ; j++) {
          int c = cardOf(s, j);
          double pu = (u == seat) ? ((k.myHand & bit(c)) ? 1.0 : 0.0) : bel.marg[c][u];
          none *= (1 - pu);
          double opp = 0;
          for (int o = 0; o < NPLAY; o++) if (oppMask & (1 << o)) opp = std::max(opp, bel.marg[c][o]);
          bestCard = std::max(bestCard, opp);
        }
        v = std::max(v, (1 - none) * bestCard);
      }
      if (v > bestV) { bestV = v; best = u; }
    }
    return best;
  }
};

} // namespace litp
} // namespace fish
