// FishBot v0.5 policy.
//
// v0.5 is v0.4 with the defects the v0.5 diagnosis measured, fixed.  The
// diagnosis is in research/v05/DESIGN.md; every mechanism here is keyed to a
// measured defect and is individually switchable so the paper can ablate it.
//
// The load-bearing change is M1.  v0.4 spends 41.4% of its asks on cards it can
// PROVE the target does not hold, and 99.3% of those are voluntary -- starved
// turns, where no live ask existed at all, are 0.29%.  It does this because the
// ownership features are not gated by hit probability: f[3] own-set progress,
// f[5] lock completion, f[7] completion bonus and f[15] pTeamAll together pay up
// to +8.76 at p = 0, against f[0]'s 11.506*p.  A card the whole team already
// owns therefore outscores a genuine chance at a card it does not, and two such
// policies facing each other settle into a deterministic two-question cycle:
// across 1,200 mirror games at six seeds, all 140 games past 300 events contain
// a strictly 2-periodic ask cycle averaging 235.7 events.
//
// Restricting the candidate set to asks with a strictly positive hard-consistent
// probability collapses the longest dead run from 289 to 1 and misdeclarations
// from 10.9% to 1.9% at no cost in win rate.
#pragma once
#include "v04.hpp"
#include "v07_stall.hpp"

namespace fish {

// ---- v0.7 phase 3 (K2) ----------------------------------------------------
// A fixed-size descending top-M over the feasible allocations of one half-suit,
// ties going to whatever arrived first.  That tie rule is load-bearing: it is
// what makes slot 0 the allocation the shipped marginal-product rule names, so
// `jalloc=0` and `jalloc=1` differ only where the joint score actually differs.
static constexpr int K2_JMAX = 64;
inline void k2InsertTop(double* score, int (*picks)[SETSZ], int& n, int cap,
                        double s, const int* pick, int nFree) {
  if (n == cap && !(s > score[n - 1])) return;
  int at = n < cap ? n : cap - 1;
  while (at > 0 && s > score[at - 1]) {
    score[at] = score[at - 1];
    for (int i = 0; i < nFree; i++) picks[at][i] = picks[at - 1][i];
    at--;
  }
  score[at] = s;
  for (int i = 0; i < nFree; i++) picks[at][i] = pick[i];
  if (n < cap) n++;
}

struct V05Config {
  BeliefMode belief = BeliefMode::Fast;
  int sinkOuter = 4, sinkInner = 8;
  double priorTheta = 0.44458;      // weight on "asked in this half-suit"
  double priorPhi   = 0.12198;      // weight on "took turns but never asked here"
  // Exact two-ply refinement: re-derive the posterior after the hypothetical hit
  // and miss for the leading candidates, instead of approximating the follow-up.
  bool   greedyMAP    = false;   // condition while choosing the allocation
  int    searchTopK   = 6;
  double chainWeight  = 3.58301;
  double threatWeight = 2.70470;
  int particles = 96;
  double w[NFEAT] = {
    /*0  hit probability   */   11.64227,
    /*1  squared hit       */    3.30409,
    /*2  certain hit       */    3.47852,
    /*3  own set progress  */    2.50714,
    /*4  team control      */    1.68817,
    /*5  lock completion   */    4.04617,
    /*6  continuation      */    1.60518,
    /*7  completion bonus  */    1.21892,
    /*8  reply threat      */   -2.90583,
    /*9  information leak  */   -1.22009,
    /*10 target hand size  */   -2.24823,
    /*11 empties target    */    1.15962,
    /*12 repeats set       */    1.38026,
    /*13 known team cards  */    0.83838,
    /*14 location entropy  */   -2.42663,
    /*15 team owns set     */   -0.95833,
    /*16 exposure on miss  */    2.53330,
    /*17 trailing pressure */    -0.22422,
    /*18 runway            */    1.24554,
    /*19 leak magnitude    */   -1.40315,
  };
  // declaration
  double declThreshold      = 0.81991;   // unlocked half-suits: P(allocation correct)
  double lockedAllocThresh  = 0.73250;  // locked half-suits: only near-certainty
  double minTeamProb        = 0.85876;   // exact P(team owns the half-suit) floor
  bool   patientLocked      = true;  // delay locked half-suits (information hiding)
  double askFloor           = 0.25742;   // no productive ask left -> cash everything
  int    patiencePool       = 6;     // unresolved cards below which we stop waiting
  int    forceDeclareEvents = 220;   // hard termination guarantee
  double oppCardFloor       = 2.61651;     // opponents nearly out -> cash
  // one-ply expectimax over the learned value function
  bool   useValue           = true;
  double valueWeight        = 6.47680;   // scale of EV against the linear ask score
  double linearWeight       = 0.75393;
  bool   valueDeclare       = true;  // decide declarations by EV, not a threshold
  double declareMargin      = -0.03044;   // EV edge required to cash a half-suit
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

  // ---- v0.5 mechanisms, each individually switchable for ablation ----------
  // M1 live-ask gating.  Restrict candidates to asks with a strictly positive
  // hard-consistent probability, and scale the ownership features by p so that a
  // card nobody can hand over cannot outscore a live one.  This is the fix for
  // the two-question deadlock; see the file header.
  bool   liveAskGate        = true;
  // Scaling the ownership features by p as well as gating on it is redundant
  // once liveAskGate removes the p = 0 case, and it distorts the calibration of
  // the rest of the linear score: measured at -5.6 points against v0.4 on top of
  // M1+M2+M8.  Retained as a switch for the ablation table, off by default.
  bool   ownershipByP       = false;
  // M8 termination.  v0.4's pressure() stage 2 returns true from declareNow
  // before inspecting pAlloc at all, which costs 8.13 points against a mirror
  // opponent and exactly nothing against a weak one.  With M1 in place the
  // deadlock is gone and the guillotine has almost nothing left to do, so the
  // structural backstop is a repetition guard instead of an event count.
  bool   forceStage2        = false;
  // ---- v0.7 phase 3, candidate K3: termination by stall, not by clock ------
  // See v07_stall.hpp for the rule and the termination argument.  Default 0 is
  // OFF and the binary is bit-identical to the reference with it off.
  int    stallEvents        = 0;      // 0 = off; else escalate to press 1 after
                                      // K public events with no new certificate
  int    stallStage2        = 0;      // 0 = 2*stallEvents
  bool   stallSoft          = false;  // count a NEW DISTINCT disjunction as
                                      // progress as well as a hard certificate
  // Designed and rejected.  Forbidding a (card, target) pair this seat has
  // already asked looks like a free structural backstop, but cards MOVE: if the
  // target has since won the card from somebody else in a public transfer, the
  // repeat is the single most valuable ask on the board, and the guard forbids
  // exactly it.  Measured at -6.0 points against v0.4 on top of M1+M2+M8.  M1
  // already subsumes the case the guard was meant to catch, because a repeat
  // after a miss is provably dead.  Off by default; kept for the ablation.
  bool   repeatGuard        = false;
  bool   feasibleDecl       = true;   // M2 on the voluntary path too

  // ---- v0.7: calibration by planted weakness --------------------------------
  // A responder that cannot recover a planted two-point edge contributes
  // nothing to a claim about a one-point difference, and the phase-1 report has
  // to be able to say so in those words (THREAT-MODEL.md T5).  The corpus's
  // only calibration point is a single ~4-point handicap built by zeroing the
  // hit-probability weight (paper/sections_v06/09-fitting.tex, sec:fit-objective:
  // 45.89% recovered to 48.33%).  These knobs turn that one point into a graded
  // ladder, and add a family the strength ladder cannot express.
  //
  //   plantKind 1 LEAK.  Add plantStr * (cards of this half-suit I hold) to the
  //     ask score.  The bias is nearly free in strength -- it correlates with
  //     f[3] own-set progress, which the policy already rewards -- but it makes
  //     the CHOICE OF HALF-SUIT a monotone read-out of the asker's holding in
  //     it.  This is a READABILITY handicap, and it is the only one in the
  //     ladder that a white-box inversion responder should find at a smaller
  //     size than a strength-seeking one.  If C5 does not beat C1 here, C5 has
  //     no case anywhere.
  //   plantKind 2 TELL.  Add plantStr to any ask whose target is the
  //     lowest-indexed live opponent: a fixed, public, per-target bias that the
  //     v0.6 feature set contains no coordinate for.  Separates C1 from C2.
  //   plantKind 3 GATE.  Disable M1's live-ask filter on a deterministic
  //     plantStr fraction of decisions (by public event index, so it is
  //     reproducible and carries no hidden randomness).  Reintroduces the
  //     guaranteed-miss asks M1 exists to delete, at a controlled rate.
  int    plantKind          = 0;
  double plantStr           = 0.0;

  // ---- v0.7 phase 3 (K2): joint-posterior declaration allocation ----------
  // Ledger L1.  The shipped voluntary declaration picks the six-card assignment
  // by maximising a PRODUCT OF INDEPENDENT MARGINALS subject to capacity and
  // certificate feasibility, and only afterwards prices the winner jointly.
  // `jalloc=1` selects by the joint score instead, restricted to the top
  // `jtopm` survivors by marginal product (0 = every survivor), with ties broken
  // in favour of the marginal-product winner so that `jalloc=0` is exactly
  // today's code and today's bits.  DEFAULT OFF.
  bool   jalloc             = false;
  int    jallocTopM         = 8;
  // Capture-only: how many survivors the L1 REPLAY rescores jointly.  Not a
  // policy knob -- it never runs unless decisionCapture() is set.
  int    l1ReplayTopM       = 32;
};

struct V05Agent : Agent {
  V05Config cfg;
  Belief bel;
  BlockDP block;
  bool blockOk = false;
  Rng rng;
  bool dirty = true;
  int lastMySet = -1;
  int teamMask = 0, oppMask = 0;
  double lastAskP = -1;      // calibration: forecast attached to the last ask
  // M8 repetition guard: (card, target) pairs this agent has already asked.
  // A structural backstop -- with M1 in place a repeat is almost always already
  // provably dead, so this only catches the residue.
  bool asked[NCARD][NPLAY] = {};
  // K3 stall detector.  `evSeen` tracks pub.nEvents (every seat observes every
  // event, game.hpp:260), `lastProgressEv` the last event at which THIS seat's
  // certificate set changed.  Untouched, and never even hashed, when
  // cfg.stallEvents == 0.
  int      evSeen = 0, lastProgressEv = 0;
  uint64_t stallSig = 0;
  std::vector<uint64_t> disjSeen;   // only when cfg.stallSoft
  size_t   disjScanned = 0;
  const char* label = "fishbot_v05";
  // v0.7 D1: the per-decision channel.  Filled only when decisionCapture() is
  // set by the harness, so the shipped hot path pays one thread-local load.
  DecisionInfo lastDec;

  const char* name() const override { return label; }
  const DecisionInfo* lastDecision() const override { return &lastDec; }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    rng = Rng(mixSeed(seed, 0x0404ull));
    dirty = true; lastMySet = -1; lastAskP = -1;
    memset(asked, 0, sizeof(asked));
    teamMask = 0; oppMask = 0;
    for (int p = 0; p < NPLAY; p++) { if (teamOf(p) == teamOf(s)) teamMask |= 1 << p; else oppMask |= 1 << p; }
    stallReset();
  }
  void resetWithKnowledge(int s, uint64_t hand, const Rules& r, uint64_t seed,
                          const Knowledge& k0) override {
    seat = s; k = k0;
    rng = Rng(mixSeed(seed, 0x0404ull));
    dirty = true; lastMySet = -1; lastAskP = -1;
    memset(asked, 0, sizeof(asked));
    teamMask = 0; oppMask = 0;
    for (int p = 0; p < NPLAY; p++) { if (teamOf(p) == teamOf(s)) teamMask |= 1 << p; else oppMask |= 1 << p; }
    stallReset();
  }
  void observe(const Event& e) override {
    Agent::observe(e);
    if (e.kind == Kind::Ask && int(e.actor) == seat) asked[e.card][e.target] = true;
    dirty = true;
    if (cfg.stallEvents > 0) stallStep();
  }

  // One step of the K3 stall detector.  Costs one 54-card hash per observed
  // event and nothing at all when the key is off.
  void stallStep() {
    evSeen++;
    bool progress = false;
    uint64_t s = k3HardSig(k);
    if (s != stallSig) { stallSig = s; progress = true; }
    if (cfg.stallSoft) {
      for (; disjScanned < k.disj.size(); disjScanned++) {
        uint64_t h = k3DisjSig(k.disj[disjScanned]);
        bool seen = false;
        for (uint64_t v : disjSeen) if (v == h) { seen = true; break; }
        if (!seen) { disjSeen.push_back(h); progress = true; }
      }
    }
    if (progress) lastProgressEv = evSeen;
    else {
      long long run = evSeen - lastProgressEv;
      auto& S = k3stall();
      long long cur = S.maxStall.load(std::memory_order_relaxed);
      while (run > cur && !S.maxStall.compare_exchange_weak(cur, run)) {}
    }
  }
  void stallReset() {
    evSeen = 0; lastProgressEv = 0; disjSeen.clear(); disjScanned = 0;
    stallSig = cfg.stallEvents > 0 ? k3HardSig(k) : 0;
  }

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
    // M1: the ownership features are scaled by the hit probability.  Ungated
    // they pay up to +8.76 at p = 0 (v0.4 f[3]+f[5]+f[7]+f[15]), which is what
    // makes a provably dead ask outscore a live one.
    double og = cfg.ownershipByP ? p : 1.0;
    f[3]  = og * myHave / 6.0;
    f[4]  = teamExp / 6.0;
    f[5]  = og * pTeamOther;
    f[6]  = continuation;
    f[7]  = og * (myHave >= 4 ? 1.0 : (myHave == 3 ? .35 : 0.0));
    f[8]  = (1 - p) * threatOf(pub, target);
    f[9]  = teamRevealedSet(S) ? 0.0 : 1.0;
    f[10] = pub.handCount[target] / 9.0;
    f[11] = (pub.handCount[target] == 1) ? p : 0.0;
    f[12] = (lastMySet == S) ? 1.0 : 0.0;
    f[13] = teamKnown / 6.0;
    f[14] = binEnt(p);
    f[15] = og * pTeamAll;
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

  // A card the actor can PROVE the target does not hold.  This is a hard
  // deduction from the actor's own hand plus the public record -- no posterior
  // and no policy prior enter it -- so filtering on it cannot introduce a
  // modelling error, only remove guaranteed-zero moves.
  inline bool provablyDead(int card, int target) const {
    return k.owner[card] < NPLAY ? k.owner[card] != target
                                 : !(k.mask[card] & (1u << target));
  }

  // M1: legal asks that are not provably dead.  Falls back to the full legal set
  // when every ask is dead -- a genuinely starved turn, measured at 0.29% of
  // turns, where the rules still oblige the actor to move.
  // v0.7 planted weakness, kinds 1 and 2.  Zero unless a handicap is installed.
  double plantTerm(const PublicState& pub, int card, int target) const {
    if (cfg.plantKind == 1)
      return cfg.plantStr * double(popcount64(k.myHand & setMask(setOf(card))));
    if (cfg.plantKind == 2) {
      for (int q = 0; q < NPLAY; q++)
        if ((oppMask & (1 << q)) && pub.handCount[q]) return (target == q) ? cfg.plantStr : 0.0;
    }
    return 0.0;
  }
  bool plantGateOpen(const PublicState& pub) const {
    if (cfg.plantKind != 3) return false;
    int pct = int(std::lround(cfg.plantStr * 100.0));
    return pct > 0 && (pub.nEvents % 100) < pct;
  }

  int enumerateLive(const PublicState& pub, AskMove* out) const {
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    if (plantGateOpen(pub)) { for (int i = 0; i < n; i++) out[i] = buf[i]; return n; }
    int m = 0;
    for (int i = 0; i < n; i++) {
      if (provablyDead(buf[i].card, buf[i].target)) continue;
      if (cfg.repeatGuard && asked[buf[i].card][buf[i].target]) continue;
      out[m++] = buf[i];
    }
    if (m) return m;
    // Nothing live: retry without the repetition guard before giving up on it.
    if (cfg.repeatGuard) {
      for (int i = 0; i < n; i++)
        if (!provablyDead(buf[i].card, buf[i].target)) out[m++] = buf[i];
      if (m) return m;
    }
    for (int i = 0; i < n; i++) out[m++] = buf[i];
    return m;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    refresh();
    AskMove buf[NSET * SETSZ * 3];
    int n = cfg.liveAskGate ? enumerateLive(pub, buf)
                            : enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return AskMove{0, 0};
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    double bestScore = -1e18; AskMove best = buf[0]; double bestP = 0;
    double f[NFEAT];
    const bool cap = decisionCapture();
    double us[NSET * SETSZ * 3];
    for (int i = 0; i < n; i++) {
      features(pub, buf[i].card, buf[i].target, f);
      double u = 0;
      for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
      u *= cfg.linearWeight;
      if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
      if (cfg.plantKind) u += plantTerm(pub, buf[i].card, buf[i].target);
      us[i] = u;
      if (u > bestScore) { bestScore = u; best = buf[i]; bestP = f[0]; }
    }
    if (cap) captureAsk(pub, buf, us, n, bestScore);
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
        if (cfg.plantKind) u += plantTerm(pub, buf[i].card, buf[i].target);
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
      if (cap) { lastDec.p = pickP; lastDec.dead = provablyDead(pick.card, pick.target); }
      return pick;
    }
    lastMySet = setOf(best.card);
    lastAskP = bestP;
    if (cap) { lastDec.p = bestP; lastDec.dead = provablyDead(best.card, best.target); }
    return best;
  }

  // v0.7 D1.  The candidate-set anatomy of one ask decision: how many
  // candidates the policy ranked, how many of them tied the leader BIT FOR BIT
  // (which is 53.2% of v0.6's ask decisions -- research/v06/results/E8-ties.txt),
  // the score gap to the runner-up, and whether the live-ask gate removed what
  // would otherwise have been the argmax.  The last of these is what makes
  // ledger entry L10 measurable at all: the gate's binding rate is a property of
  // decisions, not of games.
  void captureAsk(const PublicState& pub, const AskMove* buf, const double* us, int n, double bestScore) {
    lastDec.clear();
    lastDec.nCand = n;
    int tie = 0; double second = -1e18;
    for (int i = 0; i < n; i++) {
      if (us[i] >= bestScore - 1e-12) tie++;
      else if (us[i] > second) second = us[i];
    }
    lastDec.nTie = tie;
    lastDec.score = bestScore;
    lastDec.margin = (tie >= 2 || second <= -1e17) ? 0.0 : bestScore - second;
    // Would the ungated argmax have been a candidate the gate removed?
    if (cfg.liveAskGate) {
      AskMove all[NSET * SETSZ * 3];
      int na = enumerateAsks(pub, k.myHand, seat, all);
      double f2[NFEAT], bestAll = -1e18; int bestIdx = -1;
      for (int i = 0; i < na; i++) {
        features(pub, all[i].card, all[i].target, f2);
        double u = 0;
        for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f2[j];
        u *= cfg.linearWeight;
        if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, all[i].card, all[i].target, f2[0]);
        if (u > bestAll) { bestAll = u; bestIdx = i; }
      }
      if (bestIdx >= 0 && provablyDead(all[bestIdx].card, all[bestIdx].target))
        lastDec.gateBound = true;
    }
    (void)buf;
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

  // ---- M2: capacity-feasible joint allocation -----------------------------
  //
  // v0.4 named the allocation by taking, for each card independently, the
  // teammate with the largest marginal (`bestGuess`).  That ignores capacity, so
  // it routinely names an allocation in which one teammate holds more cards of
  // the half-suit than their whole hand contains.  Such an allocation has
  // posterior probability exactly zero, which is why 100% of v0.4's
  // forced-endgame declarations are wrong and why `willingForced` returns a
  // bitwise-zero confidence at 99.2% of forced-endgame states, leaving all seven
  // willingness rungs permanently inert.
  //
  // A half-suit has six cards and a team has three seats, so the feasible set is
  // at most 3^6 = 729 assignments.  We enumerate it exhaustively, reject on
  // capacity, and score the survivors with the same joint estimator the
  // declaration rule already uses.  Exact, and cheaper than the DP it replaces.
  int lastFeasibleCount = 0;    // v0.7 D1: feasible allocations surviving capacity+certificates
  bool feasibleAllocation(int set, int* owners, double& prob) {
    lastFeasibleCount = 0;
    int mates[3], nm = 0;
    for (int p = 0; p < NPLAY; p++) if (teamMask & (1 << p)) mates[nm++] = p;
    uint8_t q[NPLAY]; k.capacities(q);

    int free_[SETSZ], nFree = 0;
    int used[NPLAY] = {0,0,0,0,0,0};
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(set, i);
      if (k.myHand & bit(c)) { owners[i] = seat; continue; }
      if (k.owner[c] < NPLAY) {
        if (!(teamMask & (1u << k.owner[c]))) return false;   // an opponent holds it
        owners[i] = k.owner[c];
        continue;
      }
      if (k.owner[c] == OUT_OF_PLAY) return false;
      if (!(k.mask[c] & teamMask)) return false;              // no teammate may hold it
      free_[nFree++] = i;
      owners[i] = -1;
    }
    if (!nFree) {
      int cards[SETSZ];
      for (int i = 0; i < SETSZ; i++) cards[i] = cardOf(set, i);
      prob = bel.jointSequential(k, cards, owners, SETSZ, cfg.sinkOuter, cfg.sinkInner,
                                 cfg.priorTheta, cfg.priorPhi);
      lastFeasibleCount = 1;
      return true;
    }

    int best[SETSZ]; double bestScore = -1; bool found = false;
    int pick[SETSZ];
    // K2 (jalloc).  When the joint rule is armed we keep the top-M survivors by
    // marginal product in DESCENDING order, first-encountered winning ties, so
    // slot 0 is bit-for-bit the allocation the shipped rule names.  With
    // cfg.jalloc off, JM is 0 and not one instruction of this runs.
    const int JM = cfg.jalloc ? (cfg.jallocTopM > 0 ? std::min(cfg.jallocTopM, K2_JMAX) : K2_JMAX) : 0;
    double topScore[K2_JMAX]; int topPick[K2_JMAX][SETSZ]; int nTop = 0;
    int total = 1;
    for (int i = 0; i < nFree; i++) total *= nm;
    for (int code = 0; code < total; code++) {
      int t = code;
      for (int i = 0; i < nFree; i++) { pick[i] = mates[t % nm]; t /= nm; }
      int cnt[NPLAY] = {0,0,0,0,0,0};
      bool ok = true;
      double score = 1;
      for (int i = 0; i < nFree && ok; i++) {
        int idx = free_[i], c = cardOf(set, idx), p = pick[i];
        if (!(k.mask[c] & (1u << p))) { ok = false; break; }   // excluded by a certificate
        if (++cnt[p] > int(q[p]) + used[p]) { ok = false; break; }  // capacity
        score *= bel.marg[c][p];
      }
      if (!ok) continue;
      lastFeasibleCount++;
      if (JM) k2InsertTop(topScore, topPick, nTop, JM, score, pick, nFree);
      if (score <= bestScore) continue;
      bestScore = score; found = true;
      for (int i = 0; i < nFree; i++) best[free_[i]] = pick[i];
    }
    if (!found) return false;
    for (int i = 0; i < nFree; i++) owners[free_[i]] = best[free_[i]];

    int cards[SETSZ];
    for (int i = 0; i < SETSZ; i++) cards[i] = cardOf(set, i);
    if (JM && nTop > 1) {
      // The joint rescoring.  `jointSequential` conditions card by card, so it
      // prices the assignment as a joint object rather than as a product of
      // unconditioned marginals -- which is the whole of ledger L1's mechanism
      // claim.  Slot 0 is scored first and the comparison is STRICT, so a tie
      // keeps the marginal-product winner and jalloc is a minimal change.
      int cand[SETSZ]; double bestJ = -1; int bestT = -1;
      for (int t2 = 0; t2 < nTop; t2++) {
        for (int i = 0; i < SETSZ; i++) cand[i] = owners[i];
        for (int i = 0; i < nFree; i++) cand[free_[i]] = topPick[t2][i];
        double pj = bel.jointSequential(k, cards, cand, SETSZ, cfg.sinkOuter, cfg.sinkInner,
                                        cfg.priorTheta, cfg.priorPhi);
        if (pj > bestJ) { bestJ = pj; bestT = t2; }
      }
      if (bestT >= 0) {
        for (int i = 0; i < nFree; i++) owners[free_[i]] = topPick[bestT][i];
        prob = bestJ;
        return true;
      }
    }
    prob = bel.jointSequential(k, cards, owners, SETSZ, cfg.sinkOuter, cfg.sinkInner,
                               cfg.priorTheta, cfg.priorPhi);
    return true;
  }

  // ---- v0.7 phase 3 (K2): ledger L1's replay ------------------------------
  //
  // "The cheapest decisive experiment in this document ... a pure replay."  At
  // an actual voluntary declaration, and only when the harness has asked for
  // records, re-derive the same half-suit three ways and hand all three to the
  // driver, which is the only thing that can see the deal:
  //   (a) the allocation the shipped marginal-product rule named  -- `d` itself;
  //   (b) the allocation a JOINT argmax names;
  //   (c) the EXACT posterior's shape over the feasible set, from the block DP:
  //       how many assignments survive, how many count vectors they span, and
  //       -- decisively -- whether the exact posterior is FLAT over them, in
  //       which case no belief-based rule under the uniform-deal prior can do
  //       better than 1/nAlloc and the entry closes as an information limit.
  // This runs no games of any new policy and changes no decision.
  void l1Replay(int set, const Declaration& d) {
    lastDec.l1have = 0;
    int mates[3], nm = 0;
    for (int p = 0; p < NPLAY; p++) if (teamMask & (1 << p)) mates[nm++] = p;
    uint8_t q[NPLAY]; k.capacities(q);
    int free_[SETSZ], nFree = 0, owners[SETSZ];
    int used[NPLAY] = {0,0,0,0,0,0};
    bool feasible = true;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(set, i);
      if (k.myHand & bit(c)) { owners[i] = seat; continue; }
      if (k.owner[c] < NPLAY) {
        if (!(teamMask & (1u << k.owner[c]))) { feasible = false; break; }
        owners[i] = k.owner[c]; continue;
      }
      if (k.owner[c] == OUT_OF_PLAY) { feasible = false; break; }
      if (!(k.mask[c] & teamMask)) { feasible = false; break; }
      free_[nFree++] = i; owners[i] = -1;
    }
    if (!feasible) return;
    int cards[SETSZ];
    for (int i = 0; i < SETSZ; i++) cards[i] = cardOf(set, i);

    // (a)+(b): enumerate, keep the top-M by marginal product, rescore jointly.
    const int JM = std::min(std::max(1, cfg.l1ReplayTopM), K2_JMAX);
    double topScore[K2_JMAX]; int topPick[K2_JMAX][SETSZ]; int nTop = 0;
    int nFeas = 0, pick[SETSZ];
    int total = 1;
    for (int i = 0; i < nFree; i++) total *= nm;
    for (int code = 0; code < total; code++) {
      int t = code;
      for (int i = 0; i < nFree; i++) { pick[i] = mates[t % nm]; t /= nm; }
      int cnt[NPLAY] = {0,0,0,0,0,0};
      bool ok = true; double score = 1;
      for (int i = 0; i < nFree && ok; i++) {
        int idx = free_[i], c = cardOf(set, idx), p = pick[i];
        if (!(k.mask[c] & (1u << p))) { ok = false; break; }
        if (++cnt[p] > int(q[p]) + used[p]) { ok = false; break; }
        score *= bel.marg[c][p];
      }
      if (!ok) continue;
      nFeas++;
      k2InsertTop(topScore, topPick, nTop, JM, score, pick, nFree);
    }
    lastDec.l1n = nFeas;
    if (!nTop) return;
    int cand[SETSZ]; double bestJ = -1, secondJ = -1; int bestT = -1;
    for (int t2 = 0; t2 < nTop; t2++) {
      for (int i = 0; i < SETSZ; i++) cand[i] = owners[i];
      for (int i = 0; i < nFree; i++) cand[free_[i]] = topPick[t2][i];
      double pj = bel.jointSequential(k, cards, cand, SETSZ, cfg.sinkOuter, cfg.sinkInner,
                                      cfg.priorTheta, cfg.priorPhi);
      if (pj > bestJ) { secondJ = bestJ; bestJ = pj; bestT = t2; }
      else if (pj > secondJ) secondJ = pj;
    }
    for (int i = 0; i < SETSZ; i++) lastDec.l1jointOwner[i] = int8_t(owners[i]);
    for (int i = 0; i < nFree; i++) lastDec.l1jointOwner[free_[i]] = int8_t(topPick[bestT][i]);
    lastDec.l1jTop = bestJ; lastDec.l1jSecond = secondJ;
    lastDec.l1jRescored = nTop;
    bool same = true;
    for (int i = 0; i < SETSZ; i++) if (lastDec.l1jointOwner[i] != int8_t(d.owner[i])) { same = false; break; }
    lastDec.l1jSame = same ? 1 : 0;
    lastDec.l1have |= 1;

    // (c): the exact shape.  A scratch DP, not the agent's own -- the aliasing
    // guard (blockdp.hpp) makes a second instance on this thread safe.
    static thread_local BlockDP scratch;
    if (!scratch.build(k)) return;
    BlockDP::AllocShape sh = scratch.allocShape(set, teamMask);
    if (!sh.ok) return;
    lastDec.l1nCV = sh.nCV; lastDec.l1nAlloc = sh.nAlloc;
    lastDec.l1pMap = sh.pMap; lastDec.l1flat = sh.flat ? 1 : 0;
    for (int i = 0; i < SETSZ; i++) lastDec.l1exactOwner[i] = int8_t(owners[i]);
    for (int i = 0; i < sh.nCards; i++) {
      int idx = -1;
      for (int j = 0; j < SETSZ; j++) if (cards[j] == sh.cards[i]) { idx = j; break; }
      if (idx >= 0) lastDec.l1exactOwner[idx] = int8_t(sh.seats[i]);
    }
    for (int i = 0; i < SETSZ; i++) if (lastDec.l1exactOwner[i] < 0) return;
    lastDec.l1have |= 2;
  }

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
    // M8.  v0.4's stage 2 returned true from declareNow before ever inspecting
    // pAlloc, cashing whatever was best however hopeless.  Measured at 8.13
    // points against a mirror opponent and exactly 0 against every weak one -- a
    // pure strong-opponent tax.  With M1 the deadlock it existed to break is
    // gone, so stage 2 is off by default and stage 1 (better than a coin flip)
    // is the only escalation left.
    // v0.7 K3.  The stall detector replaces the clock with the condition the
    // clock is a proxy for.  It sits ABOVE the clock rungs rather than instead
    // of them, so that `stall=K` alone is a strictly-added backstop and
    // `stall=K,force=1000000` is the clock-free configuration.  See
    // v07_stall.hpp.
    if (cfg.stallEvents > 0) {
      const int stalled = evSeen - lastProgressEv;
      const int s2 = cfg.stallStage2 > 0 ? cfg.stallStage2 : 2 * cfg.stallEvents;
      if (stalled >= s2) return 2;
      if (stalled >= cfg.stallEvents) return 1;
    }
    if (cfg.forceStage2 && pub.nEvents >= (7 * cfg.forceDeclareEvents) / 5) return 2;
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
      if (cfg.feasibleDecl) {
        // M2: the voluntary declaration names a capacity-feasible allocation too.
        int owners[SETSZ]; double pr = 0;
        if (!feasibleAllocation(s, owners, pr)) return v;
        for (int i = 0; i < SETSZ; i++) v.decl.owner[i] = uint8_t(owners[i]);
        v.pAlloc = pr;
        v.pTeam = std::max(v.pTeam, v.pAlloc);
        if (v.pTeam < teamFloor) return v;
        v.ok = true;
        return v;
      }
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
    if (!cfg.declareEnabled) return false;
    if (!pub.rules.cardlessMayDeclare && !pub.handCount[seat]) return false;
    // Capacity-only gate first: the exact posterior is far too costly to build
    // for six observers after every public event, and a half-suit that cannot
    // plausibly be wholly ours can never clear the declaration bar.
    int unresolvedCount = __builtin_popcountll(k.unresolved);
    int press = pressure(pub);
    if (cfg.stallEvents > 0) {
      // Attribution.  Which rung fired, and was it the stall rule or the clock?
      auto& S = k3stall();
      S.declOpps.fetch_add(1, std::memory_order_relaxed);
      const int stalled = evSeen - lastProgressEv;
      const int s2 = cfg.stallStage2 > 0 ? cfg.stallStage2 : 2 * cfg.stallEvents;
      if (stalled >= s2) S.stage2.fetch_add(1, std::memory_order_relaxed);
      else if (stalled >= cfg.stallEvents) S.stage1.fetch_add(1, std::memory_order_relaxed);
      else if (press >= 1) S.clockStage1.fetch_add(1, std::memory_order_relaxed);
    }
    bool bypass = unresolvedCount <= 8 || press >= 1;
    if (cfg.useValue) computeAggregates(pub);
    bool candidate = bypass;
    for (int s = 0; s < NSET && !candidate; s++) {
      if (!pub.setActive[s]) continue;
      if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) candidate = true;
    }
    if (!candidate && !cfg.gateAudit) return false;
    refresh();
    int oppCards = 0;
    for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
    // v0.7 phase 2.  The four clauses are recorded separately, because they are
    // not equally attackable and an adversary that raises the urgent share is
    // only interesting if we can say WHICH lever it pulled.  Clause 2 is the
    // opposing team's own total hand count and clause 3 is the length of the
    // game -- both are public and both are things an opponent can move.  Clause
    // 1 is the target's own residual ambiguity and clause 4 is its own best
    // available ask, which an opponent moves only indirectly, by starving the
    // posterior.  The bit order is (1,2,3,4) -> bits 0..3.
    const bool u0 = unresolvedCount <= cfg.patiencePool;
    const bool u1 = oppCards <= cfg.oppCardFloor;
    const bool u2 = pub.nEvents >= cfg.forceDeclareEvents;
    const bool u3 = bestAskProbability(pub) < cfg.askFloor;
    bool urgent = u0 || u1 || u2 || u3;
    const int urgWhyBits = (u0 ? 1 : 0) | (u1 ? 2 : 0) | (u2 ? 4 : 0) | (u3 ? 8 : 0);
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
    conf = bestConf;
    if (decisionCapture()) {
      lastDec.clear();
      lastDec.pAlloc = bestConf;
      lastDec.nFeasible = lastFeasibleCount;
      lastDec.nCand = unresolvedCount;
      lastDec.urgent = urgent;
      lastDec.pressure = press;
      lastDec.urgWhy = urgWhyBits;
      // K2.  The replay runs on the half-suit actually declared, not on
      // whichever one evaluateSet happened to touch last.
      if (found) l1Replay(d.set, d);
    }
    return found;
  }

  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) override {
    refresh();
    // M2: the ladder is fed by the feasible allocator.  Under v0.4 this returned
    // a bitwise-zero confidence at 99.2% of forced-endgame states, so every rung
    // was inert and every declaration fell through to the bestGuess rung.
    int owners[SETSZ]; double pr = 0;
    if (feasibleAllocation(set, owners, pr)) {
      if (pr < threshold) return false;
      d.set = uint8_t(set);
      for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(owners[i]);
      conf = pr;
      return true;
    }
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
    // M2: name a capacity-feasible allocation.  Only if no feasible allocation
    // exists at all do we fall through to v0.4's per-card argmax, which is then
    // a formality -- the rules oblige somebody to name something.
    {
      int owners[SETSZ]; double pr = 0;
      if (feasibleAllocation(set, owners, pr)) {
        for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(owners[i]);
        conf = pr;
        return;
      }
    }
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

} // namespace fish
