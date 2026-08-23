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

namespace fish { namespace p4 {

// ---------------------------------------------------------------- probe stats
// P4 = instrumented copy of v04.hpp for the P4 adversarial correctness review.
// Nothing here changes the shipped policy: `cfg.fix == 0` reproduces v0.4
// bit-for-bit (asserted by `fish p4match` against `v04`).
struct P4Stats {
  // expectedRun
  long long runCalls = 0, runSkipFired = 0, runSelfCounted = 0;
  double runAbsErr = 0, runMaxErr = 0;
  long long askDecisions = 0, runChangedPick = 0;
  // threatOf / exposureOf
  long long threatCalls = 0, threatActivity0 = 0, threatActivityFull = 0;
  double threatActivitySum = 0;
  long long expCalls = 0, expSat = 0; double expSum = 0;
  // evaluateSet Fast
  long long evalFast = 0, evalAllocGtCheap = 0; double evalAllocCheapExcess = 0, evalMaxExcess = 0;
  long long evalFloorSavedByMax = 0;
  // exact comparison (filled by the blockcmp probe)
  long long cmpN = 0; double cmpPTeamErr = 0, cmpPAllocErr = 0, cmpCheapErr = 0;
  long long cmpPTeamOver = 0, cmpAllocOverTeamExact = 0;
  // stale aggregates
  long long declOpps = 0, staleOpps = 0, staleDeclFlip = 0; double staleEHMax = 0, staleEHSum = 0;
  // declareByValue
  long long dbvCalls = 0, dbvTrue = 0; double dbvThreshSum = 0, dbvThreshMin = 1e9, dbvThreshMax = -1e9;
  long long dbvBelow60 = 0, dbvBelow70 = 0;
  // pressure
  long long press1 = 0, press2 = 0, press0 = 0;
  // per-feature decision influence (spread of w_j f_j across the candidate list)
  double featRange[20] = {0}; double evRange = 0, linRange = 0, twoPlyRange = 0;
  long long featDecisions = 0;
  // locked misclaim
  long long lockClaim = 0, lockClaimWrong = 0;
  double f8f16Range = 0;
  long long brPress2=0, brPress1=0, brUrgThr=0, brUrgLocked=0, brValue=0, brOther=0;
  long long gateSeen = 0, gateWouldReject = 0, bypassOpps = 0, bypassLoadBearing = 0;
  void merge(const P4Stats& o) {
    runCalls+=o.runCalls; runSkipFired+=o.runSkipFired; runSelfCounted+=o.runSelfCounted;
    runAbsErr+=o.runAbsErr; runMaxErr=std::max(runMaxErr,o.runMaxErr);
    askDecisions+=o.askDecisions; runChangedPick+=o.runChangedPick;
    threatCalls+=o.threatCalls; threatActivity0+=o.threatActivity0; threatActivityFull+=o.threatActivityFull;
    threatActivitySum+=o.threatActivitySum; expCalls+=o.expCalls; expSat+=o.expSat; expSum+=o.expSum;
    evalFast+=o.evalFast; evalAllocGtCheap+=o.evalAllocGtCheap; evalAllocCheapExcess+=o.evalAllocCheapExcess;
    evalMaxExcess=std::max(evalMaxExcess,o.evalMaxExcess); evalFloorSavedByMax+=o.evalFloorSavedByMax;
    cmpN+=o.cmpN; cmpPTeamErr+=o.cmpPTeamErr; cmpPAllocErr+=o.cmpPAllocErr; cmpCheapErr+=o.cmpCheapErr;
    cmpPTeamOver+=o.cmpPTeamOver; cmpAllocOverTeamExact+=o.cmpAllocOverTeamExact;
    declOpps+=o.declOpps; staleOpps+=o.staleOpps; staleDeclFlip+=o.staleDeclFlip;
    staleEHMax=std::max(staleEHMax,o.staleEHMax); staleEHSum+=o.staleEHSum;
    dbvCalls+=o.dbvCalls; dbvTrue+=o.dbvTrue; dbvThreshSum+=o.dbvThreshSum;
    dbvThreshMin=std::min(dbvThreshMin,o.dbvThreshMin); dbvThreshMax=std::max(dbvThreshMax,o.dbvThreshMax);
    dbvBelow60+=o.dbvBelow60; dbvBelow70+=o.dbvBelow70;
    press0+=o.press0; press1+=o.press1; press2+=o.press2;
    for (int i=0;i<20;i++) featRange[i]+=o.featRange[i];
    evRange+=o.evRange; linRange+=o.linRange; twoPlyRange+=o.twoPlyRange; featDecisions+=o.featDecisions;
    lockClaim+=o.lockClaim; lockClaimWrong+=o.lockClaimWrong; f8f16Range+=o.f8f16Range;
    brPress2+=o.brPress2; brPress1+=o.brPress1; brUrgThr+=o.brUrgThr; brUrgLocked+=o.brUrgLocked;
    brValue+=o.brValue; brOther+=o.brOther;
    gateSeen+=o.gateSeen; gateWouldReject+=o.gateWouldReject; bypassOpps+=o.bypassOpps;
    bypassLoadBearing+=o.bypassLoadBearing;
  }
};
inline P4Stats& pstats() { static P4Stats s; return s; }

// fix bits
enum : int {
  FIX_RUNWAY   = 1<<0,   // expectedRun: exclude the candidate's own entry properly
  FIX_STALEAGG = 1<<1,   // proposeDeclaration: refresh() before computeAggregates
  FIX_PTEAMMAX = 1<<2,   // evaluateSet Fast: drop pTeam = max(cheap, pAlloc)
  FIX_DECLCARD = 1<<3,   // declareByValue: real card deltas on the wrong branch
  FIX_NEARCOMP = 1<<4,   // value(): perturb f[12]/f[13] with the hypothetical
  FIX_NOPRESS  = 1<<5,   // pressure(): never escalate (diagnostic)
  FIX_SOFT2    = 1<<6,   // press>=2 still requires a better-than-coin-flip allocation
};

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

struct P4Config {
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
  int    fix                = 0;      // probe: bitmask of correctness fixes
  bool   instrument         = false;  // probe: accumulate P4Stats
};

inline double binEnt(double p) {
  if (p <= 0 || p >= 1) return 0;
  return -p * std::log2(p) - (1 - p) * std::log2(1 - p);
}

struct P4Agent : Agent {
  P4Config cfg;
  mutable P4Stats st;
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
      if (cfg.instrument) {
        st.threatCalls++; st.threatActivitySum += activity;
        if (activity <= 0) st.threatActivity0++;
        if (activity >= 1) st.threatActivityFull++;
      }
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
    if (cfg.instrument) { st.expCalls++; st.expSum += std::min(1.0, e / 12.0); if (e >= 12.0) st.expSat++; }
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

  // as shipped (skip only when the candidate's own entry is runway[0])
  double expectedRunShipped(int card, double p) const {
    double q[4]; int m = 0;
    for (int i = 0; i < nRunway && m < 3; i++) {
      if (bestPerCard[card] >= 0 && std::fabs(runway[i] - bestPerCard[card]) < 1e-12 && m == 0 && i == 0) continue;
      q[m++] = runway[i];
    }
    double tail = 0;
    for (int i = m - 1; i >= 0; i--) tail = q[i] * (1.0 + tail);
    return p * (1.0 + tail) / 4.0;
  }
  // intended: drop exactly one copy of the candidate's own entry, wherever it sits
  double expectedRunFixed(int card, double p) const {
    double q[4]; int m = 0; bool skipped = false;
    for (int i = 0; i < nRunway && m < 3; i++) {
      if (!skipped && bestPerCard[card] >= 0 && std::fabs(runway[i] - bestPerCard[card]) < 1e-12) { skipped = true; continue; }
      q[m++] = runway[i];
    }
    double tail = 0;
    for (int i = m - 1; i >= 0; i--) tail = q[i] * (1.0 + tail);
    return p * (1.0 + tail) / 4.0;
  }
  double expectedRun(int card, double p) const {
    if (cfg.instrument) {
      double a = expectedRunShipped(card, p), b = expectedRunFixed(card, p);
      st.runCalls++;
      st.runAbsErr += std::fabs(a - b);
      st.runMaxErr = std::max(st.runMaxErr, std::fabs(a - b));
      // does the shipped loop actually drop the candidate's own entry?
      bool dropped = (nRunway > 0 && bestPerCard[card] >= 0 &&
                      std::fabs(runway[0] - bestPerCard[card]) < 1e-12);
      if (dropped) st.runSkipFired++;
      // is the candidate's own entry present in the shipped continuation list?
      if (!dropped) {
        for (int i = 0; i < std::min(3, nRunway); i++)
          if (bestPerCard[card] >= 0 && std::fabs(runway[i] - bestPerCard[card]) < 1e-12) { st.runSelfCounted++; break; }
      }
    }
    return (cfg.fix & FIX_RUNWAY) ? expectedRunFixed(card, p) : expectedRunShipped(card, p);
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
               int dUnresolved, int dActive, int pertSet = -1, double pertE = -1) const {
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
    bool pert = (cfg.fix & FIX_NEARCOMP) && pertSet >= 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      double e = eH[s];
      if (pert && s == pertSet) { if (pertE < 0) continue; e = pertE; }   // pertE<0 == set leaves play
      if (e >= .5 && e <= .995) f[12] += 1.0 / 9.0;
      if (e <= .5 && e >= .005) f[13] += 1.0 / 9.0;
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
    double vHit = value(pub, dh[0], dh[1], dh[2], dh[3], scoreDiff, +1, +1, -1, -1, 0, S, eHit);
    double vMiss = value(pub, dm[0], dm[1], dm[2], dm[3], scoreDiff, -1, 0, 0, 0, 0, S, eMiss);
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
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    double bestScore = -1e18; AskMove best = buf[0]; double bestP = 0;
    double f[NFEAT];
    double fmin[NFEAT], fmax[NFEAT], evmin = 1e18, evmax = -1e18, lmin = 1e18, lmax = -1e18;
    double cmin = 1e18, cmax = -1e18;
    for (int j = 0; j < NFEAT; j++) { fmin[j] = 1e18; fmax[j] = -1e18; }
    for (int i = 0; i < n; i++) {
      features(pub, buf[i].card, buf[i].target, f);
      double u = 0;
      for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
      u *= cfg.linearWeight;
      double lin = u;
      double ev = cfg.useValue ? cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]) : 0.0;
      if (cfg.useValue) u += ev;
      if (cfg.instrument) {
        for (int j = 0; j < NFEAT; j++) { double x = cfg.linearWeight * cfg.w[j] * f[j];
          fmin[j] = std::min(fmin[j], x); fmax[j] = std::max(fmax[j], x); }
        evmin = std::min(evmin, ev); evmax = std::max(evmax, ev);
        lmin = std::min(lmin, lin); lmax = std::max(lmax, lin);
        double comb = cfg.linearWeight * (cfg.w[8] * f[8] + cfg.w[16] * f[16]);
        cmin = std::min(cmin, comb); cmax = std::max(cmax, comb);
      }
      if (u > bestScore) { bestScore = u; best = buf[i]; bestP = f[0]; }
    }
    if (cfg.instrument && n > 0) {
      st.featDecisions++;
      for (int j = 0; j < NFEAT; j++) st.featRange[j] += (fmax[j] - fmin[j]);
      st.evRange += (evmax - evmin); st.linRange += (lmax - lmin); st.f8f16Range += (cmax - cmin);
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
      double tpmin = 1e18, tpmax = -1e18;
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
        if (cfg.instrument) { double tp = u - cs[r].u; tpmin = std::min(tpmin, tp); tpmax = std::max(tpmax, tp); }
        if (u > best2) { best2 = u; pick = buf[i]; pickP = p; }
      }
      if (cfg.instrument) {
        st.twoPlyRange += (tpmax - tpmin);
        st.askDecisions++;
        if (pick.card != best.card || pick.target != best.target) st.runChangedPick++;
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
    if (cfg.fix & FIX_NOPRESS) return 0;
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
      if (cfg.instrument) {
        st.evalFast++;
        if (v.pAlloc > v.pTeam + 1e-12) {
          st.evalAllocGtCheap++;
          double ex = v.pAlloc - v.pTeam;
          st.evalAllocCheapExcess += ex; st.evalMaxExcess = std::max(st.evalMaxExcess, ex);
          if (v.pTeam < teamFloor && v.pAlloc >= teamFloor) st.evalFloorSavedByMax++;
        }
      }
      if (!(cfg.fix & FIX_PTEAMMAX)) v.pTeam = std::max(v.pTeam, v.pAlloc);
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
    // Expected cards actually leaving each side when this half-suit is cashed.
    // Correct declaration => all six were ours.  Wrong declaration => either the
    // allocation was misnamed (still six of ours) or an opponent held some.
    int dOurRight = -SETSZ, dTheirRight = 0;
    int dOurWrong = -SETSZ, dTheirWrong = 0;
    if (cfg.fix & FIX_DECLCARD) {
      double ourExp = 0;
      for (int i = 0; i < SETSZ; i++) { int c = cardOf(S, i);
        ourExp += (k.myHand & bit(c)) ? 1.0 : pTeamCard(c); }
      dOurWrong = -int(std::lround(ourExp));
      dTheirWrong = -(SETSZ + dOurWrong);
    }
    double vRight = value(pub, dC, dS, dL, dK, scoreDiff + 1, turnSign, dOurRight, dTheirRight, dUnres, -1, S, -1.0);
    double vWrong = value(pub, dC, dS, dL, dK, scoreDiff - 1, turnSign, dOurWrong, dTheirWrong, dUnres, -1, S, -1.0);
    (void)mine;
    double vDeclare = v.pAlloc * vRight + (1 - v.pAlloc) * vWrong;
    double vWait = value(pub, 0, 0, 0, 0, scoreDiff, turnSign, 0, 0, 0, 0);
    if (cfg.instrument) {
      // breakeven pAlloc: vWrong + p*(vRight-vWrong) == vWait + margin
      double d = vRight - vWrong;
      st.dbvCalls++;
      if (d > 1e-12) {
        double thr = (vWait + cfg.declareMargin - vWrong) / d;
        st.dbvThreshSum += thr;
        st.dbvThreshMin = std::min(st.dbvThreshMin, thr);
        st.dbvThreshMax = std::max(st.dbvThreshMax, thr);
        if (thr < 0.60) st.dbvBelow60++;
        if (thr < 0.70) st.dbvBelow70++;
      }
      if (vDeclare > vWait + cfg.declareMargin) st.dbvTrue++;
    }
    return vDeclare > vWait + cfg.declareMargin;
  }

  bool declareNow(const PublicState& pub, const SetVerdict& v, bool urgent, int press) {
    bool locked = v.pTeam > .9995;
    if (cfg.instrument && locked) st.lockClaim++;
    if (press >= 2) {
      if (!(cfg.fix & FIX_SOFT2) || v.pAlloc >= 0.5) { if (cfg.instrument) st.brPress2++; return true; }
    }
    if (press >= 1 && v.pAlloc >= 0.5) { if (cfg.instrument) st.brPress1++; return true; }
    if (cfg.useValue && cfg.valueDeclare) {
      if (urgent) {
        bool a = v.pAlloc >= cfg.declThreshold, b = locked && v.pAlloc >= 0.5;
        if (cfg.instrument) { if (a) st.brUrgThr++; else if (b) st.brUrgLocked++; }
        return a || b;
      }
      bool r = declareByValue(pub, v);
      if (cfg.instrument && r) st.brValue++;
      return r;
    }
    if (cfg.instrument) st.brOther++;
    if (locked) {
      if (v.pAlloc >= cfg.lockedAllocThresh && (!cfg.patientLocked || urgent)) return true;
      if (urgent && v.pAlloc >= cfg.declThreshold) return true;
      return false;
    }
    return v.pAlloc >= cfg.declThreshold;
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (!cfg.declareEnabled) return false;
    if (!pub.rules.cardlessMayDeclare && !pub.handCount[seat]) return false;
    // Capacity-only gate first: the exact posterior is far too costly to build
    // for six observers after every public event, and a half-suit that cannot
    // plausibly be wholly ours can never clear the declaration bar.
    int unresolvedCount = __builtin_popcountll(k.unresolved);
    int press = pressure(pub);
    bool bypass = unresolvedCount <= 8 || press >= 1;
    bool wasDirty = dirty;
    if (cfg.instrument) { st.declOpps++; if (press >= 2) st.press2++; else if (press >= 1) st.press1++; else st.press0++; }
    // SHIPPED ORDER: computeAggregates() runs on the belief left over from the
    // previous public event, because refresh() is only called ~7 lines later.
    if (cfg.fix & FIX_STALEAGG) refresh();
    if (cfg.useValue) computeAggregates(pub);
    ValueAggregates aggStale = agg; double ehStale[NSET];
    for (int s = 0; s < NSET; s++) ehStale[s] = eH[s];
    bool candidate = bypass;
    for (int s = 0; s < NSET && !candidate; s++) {
      if (!pub.setActive[s]) continue;
      if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) candidate = true;
    }
    if (!candidate && !cfg.gateAudit) return false;
    refresh();
    ValueAggregates aggFresh = aggStale; double ehFresh[NSET];
    for (int s = 0; s < NSET; s++) ehFresh[s] = ehStale[s];
    bool measureStale = false;
    if (cfg.instrument && cfg.useValue && wasDirty && !(cfg.fix & FIX_STALEAGG)) {
      computeAggregates(pub);
      aggFresh = agg;
      double mx = 0;
      for (int s = 0; s < NSET; s++) { ehFresh[s] = eH[s]; mx = std::max(mx, std::fabs(eH[s] - ehStale[s])); }
      st.staleOpps++; st.staleEHSum += mx; st.staleEHMax = std::max(st.staleEHMax, mx);
      measureStale = true;
      agg = aggStale; for (int s = 0; s < NSET; s++) eH[s] = ehStale[s];   // restore shipped state
    }
    int oppCards = 0;
    for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
    bool urgent = unresolvedCount <= cfg.patiencePool
               || oppCards <= cfg.oppCardFloor
               || pub.nEvents >= cfg.forceDeclareEvents
               || bestAskProbability(pub) < cfg.askFloor;
    long long nBypassSaved = 0;
    double bestConf = -1; bool found = false;
    double auditBestConf = -1; bool auditFound = false; int auditBestSet = -1;
    long long localSeen = 0, localRejected = 0, localFalseNeg = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      bool passesGate = bypass || k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb;
      if (cfg.instrument) { st.gateSeen++;
        if (k.cheapTeamProb(s, teamMask) < cfg.gateTeamProb) { st.gateWouldReject++; if (bypass) nBypassSaved++; } }
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
      bool wantShipped = declareNow(pub, v, urgent, press);
      if (measureStale) {
        agg = aggFresh; for (int t = 0; t < NSET; t++) eH[t] = ehFresh[t];
        bool wantFresh = declareNow(pub, v, urgent, press);
        agg = aggStale; for (int t = 0; t < NSET; t++) eH[t] = ehStale[t];
        if (wantFresh != wantShipped) st.staleDeclFlip++;
      }
      if (!wantShipped) continue;
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
    if (cfg.instrument && bypass) { st.bypassOpps++; if (nBypassSaved) st.bypassLoadBearing++; }
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

} } // namespace fish::p4
