// FishBot v0.6 policy.
//
// v0.6 is v0.5 plus mechanisms, and it derives from V05Agent so that the
// all-switches-off configuration is v0.5 *by construction* rather than by
// agreement: with every v0.6 switch off, every override below defers to the
// base class and the two policies are bit-identical.  That makes the ablation
// table exact instead of approximate.
//
// The headline mechanism is S1, test-time search.  See v06_rollout.hpp for why
// this is not the perfect-information Monte Carlo the v0.4 study eliminated.
#pragma once
#include "v05.hpp"
#include "blockdp.hpp"
#include "v06_rollout.hpp"

namespace fish {

struct V06Extra {
  // ---- A1/A2: exact-posterior resolution ----------------------------------
  // The deployed Fast posterior is a Sinkhorn fit, and Sinkhorn makes
  // exchangeable cards numerically IDENTICAL by construction (belief.hpp:478).
  // That is why 55% of v0.5's ask decisions end in a bit-for-bit tie broken by
  // enumeration order (research/v06/notes/R0 loss channel 1, R10 section 4).
  // MEASURED AND REFUTED.  `fish v6probe --mode=ties` re-scores every tied
  // candidate under the exact count law and finds it separates 0.00% of them:
  // the tied candidates are two cards of one half-suit at one target that the
  // EXACT posterior also assigns identical probability, and enumeration order,
  // the deployed marginal, v0.5's chain/threat pass and the exact posterior all
  // realise the same hit rate to three significant figures.  The ties are
  // exchangeable and irreducible.  The switches below are therefore an
  // INSTRUMENT, not a mechanism: `exactTie` is not read by the ask rule, and
  // the shipped policy does not build the exact posterior at all.
  bool   exactTie    = true;    // A1: resolve ties on the exact marginal
  bool   exactP      = false;   // A1b: use the exact marginal as f[0] throughout
  double exactFloor  = 0.0;     // A2: drop candidates whose EXACT marginal is below this
  double exactMix    = 0.0;     // A2b: blend w*exact into the score as an extra term
  int    exactMaxQ   = 60;      // do not build the exact DP above this many unresolved cards
  // ---- D2: exact joint allocation on the declaration path ------------------
  bool   exactDecl   = false;   // score declarations with the exact count law
  double declThresh  = -1;      // <0 = leave v0.5's rule alone; else an exact-pAlloc threshold
  // ---- S1: determinized information-set search -----------------------------
  bool   search      = false;   // frozen OFF until it clears its gate; see section S1
  int    nDet        = 24;      // determinizations per searched decision
  int    topK        = 8;       // blueprint candidates carried into the search
  int    maxCand     = 20;      // hard cap after the tie extension
  double tieEps      = 1e-9;    // extend topK to cover exact ties at the cut
  int    maxDepth    = 0;       // 0 = roll to the end of the game
  double leafLambda  = 1.0;     // half-suit control weight at a depth cut
  double blend       = 0.0;     // blueprint score added to the rollout mean
  // Deviation rule.  The search deviates from the blueprint's own choice only
  // when the PAIRED improvement over it clears kappa standard errors.  Without
  // this the argmax of D noisy rollout means is the optimizer's curse: the
  // per-determinization value has sd ~2.5 half-suits while genuine differences
  // between the leading candidates are an order of magnitude smaller, so an
  // unguarded argmax selects on residual noise and lands on a candidate the
  // blueprint had ranked below the top.  Measured at 49.31% (blueprint-forced
  // control) against 13.61% (unguarded argmax) at identical budget, seed and
  // sample size -- a 35.70-point gap (research/v06/results/E12-search.jsonl).
  double kappa       = 1.0;
  // Inside the blueprint's own tie group the deviation penalty is waived: the
  // blueprint expresses NO preference there (its score is bit-identical), so
  // "deviating" from whichever candidate enumerateAsks emitted first is not a
  // deviation at all.  Loss channel #1 (research/v06/notes/R0) is exactly this
  // set, at 55% of decisions.
  double kappaTie    = 0.0;
  // Search only when the blueprint is blind, i.e. when its top group is tied.
  bool   tieOnly     = false;
  int    searchFrom  = 0;       // do not search before this public event index
  bool   policyPrior = true;    // importance-weight determinizations by theta/phi
  int    minGap      = -1;      // -1 = always search; else search only when the
                                // blueprint's top-2 gap is below `gapEps`
  double gapEps      = 0.0;
  // The rollout blueprint.  Built from parts rather than taken as a spec string
  // because the spec grammar separates options with commas.
  std::string rollBase   = "v05";
  std::string rollBelief = "";      // "" = leave the base default (Fast/Sinkhorn)
  int    rollOuter   = 1;
  int    rollInner   = 1;
  bool   rollValue   = true;
  // v0.7 C3.  The blueprint the OPPOSING seats play inside the rollout.  Empty
  // reproduces v0.6 exactly (everyone rolls out as us).  Set to the target's
  // spec and the same machinery becomes a best-response search: see
  // v06_rollout.hpp RolloutConfig::oppSpec.
  std::string rollOpp    = "";
  // v0.7: which leaf evaluator prices a depth cut.  "material" is the v0.6
  // formula and is bit-identical to it.
  std::string leafSpec   = "material";
  std::string rolloutSpec() const {
    std::string s = rollBase + ":topk=0";
    if (!rollBelief.empty()) s += ",belief=" + rollBelief;
    s += ",souter=" + std::to_string(rollOuter) + ",sinner=" + std::to_string(rollInner);
    if (!rollValue) s += ",value=0";
    return s;
  }

  // Search only when the residual hidden state is small enough that the
  // continuation is both SHORT and well-resolved.  By median event 76 the whole
  // hidden state is one of at most 10^5 possibilities while a third of the
  // half-suits are still undecided (research/v06/notes/R10 section 2), so the
  // endgame is where a rollout is simultaneously cheapest and most faithful.
  // <=0 disables the restriction.
  int    searchMaxQ  = 0;

  // ---- A2/A3: extra ask terms, weight 0 by default so v0.6 with the switches
  // off is v0.5 bit for bit.  Each is fitted, never hand-set.
  double wVoid       = 0.0;   // P(this hit takes the target's LAST card of the half-suit)
  double wTeamHas    = 0.0;   // P(our own team already holds this card)
  double wLastLive   = 0.0;   // the target is the last live opponent
  bool   extraFeats  = false;

  // ---- A6: stochastic resolution of the blueprint's tie group ---------------
  // The measured fact this exists for, and the negative control it provides:
  // the determinized search's advantage disappears when the deviation penalty
  // is applied inside the tie group as well (49.86%, F7-winrate.txt), so the
  // tie group is where it is doing its work.  If that advantage were merely
  // that a deterministic argmax broken by array order is readable, then varying
  // the choice would capture it for free.  It does not: this switch measures
  // EXACTLY 50.00% [47.18, 52.82], mean half-suits 4.500 to 4.500
  // (F8-tiesearch.txt row D).  That is the control that makes the search result
  // a claim about joint information rather than about predictability.
  //
  // The draw is seeded from a rolling hash of the PUBLIC event stream, not from
  // private state, so it is common knowledge in the sense the cooperative-search
  // literature requires: a teammate replaying the same public history can
  // reproduce the draw exactly.  A policy that randomised on private state would
  // break that property and with it the blueprint assumption a partner relies on.
  bool   randomTie   = false;

  // ---- A5: the rationed deliberate miss ------------------------------------
  // M1 removed v0.4's deadlock by DELETING every ask the actor can prove will
  // miss.  That also deleted the move class every multi-step human tactic is
  // built out of: blackballing, choosing which opponent receives the turn, the
  // costly safe-ask signal, and voluntary turn donation are all implemented as
  // a guaranteed miss at a CHOSEN seat.  Such an ask exists at 79.2% of v0.5's
  // decisions and at two or more distinct chosen opponents at 50.1%
  // (research/v06/notes/R9 section 0).
  //
  // Unbanning it outright is measured and rejected: `v05:m1=0,m1p=1` scores
  // 51.17% [48.34, 53.99] against v0.5 but brings back 35.85% dead asks, a
  // 365-ask dead run and 10% of games killed by the action limit
  // (research/v06/results/E15-deliberate-miss.txt).  So the move is RATIONED:
  //
  //   (a) the four ownership features are zeroed on a dead candidate, so the
  //       +8.76-at-p=0 incentive that caused v0.4's two-question cycle cannot
  //       fire (v05.hpp header);
  //   (b) a dead candidate must beat the best LIVE candidate by deadMargin, so
  //       it is taken only when the turn-donation and signalling terms clearly
  //       dominate a real chance at a card;
  //   (c) at most deadBudget of them per game per seat;
  //   (d) and an exact-repeat guard that is provably free: a dead (card,target)
  //       pair STAYS dead unless the card publicly moves to that target, and
  //       that movement is observable, so forbidding the repeat while the card
  //       has not moved forbids nothing that could have become live.  This is
  //       the distinction the blanket repetition guard missed -- that guard cost
  //       6.13 points precisely because it also forbade LIVE repeats, which are
  //       often the best ask on the board once a card has moved.
  // v0.5 re-scores its leading candidates with a one-step chain/threat pass that
  // rebuilds the approximate posterior twice per candidate.  It is ~60% of the
  // policy's runtime and was measured at +0.8 +/- 0.5 points.  The v0.6 scoring
  // path does not run it, which is why v0.6 is faster; the switch restores it so
  // that the omission is an ablation rather than an accident, and so that
  // `searchTopK`, `chainWeight` and `threatWeight` -- inert in the shipped
  // configuration -- can be shown to be inert.
  bool   chainPass   = false;
  bool   deadAsk     = false;
  double deadMargin  = 1.0;
  int    deadBudget  = 3;
  // How many dead candidates to hand to the search.  The linear score can never
  // price a deliberate miss -- f[0]'s 11.64*p is zero on it by definition and
  // every remaining term is a penalty -- so under the fitted weights a dead ask
  // is never competitive and the deadMargin gate above never fires (measured:
  // identical results at margins 0.0 to 2.0).  The rollout CAN price it, because
  // it plays out the position the donated turn actually produces.  This is the
  // clearest case in the study of a move class that exists only under search.
  int    deadInSearch = 0;

  // ---- diagnostics ----------------------------------------------------------
  bool   trace       = false;
};

// ---- the frozen v0.6 parameter vector ------------------------------------
// Written by engine/freeze_config_v06.py from the selected fitting run, so the
// shipped numbers and the fit artifact cannot drift apart.  Layout is the flat
// `allparams` vector: NFEAT ask weights, then the fourteen v0.5 knobs, then the
// three v0.6 ask terms.  `v06:legacy=1` restores v0.5's vector exactly, which is
// what the identity control in experiments_v06.sh compares against.
// FIT-VECTOR-BEGIN
static constexpr int NV6PARAM = NFEAT + 14 + 3;
static constexpr double V6PARAMS[NV6PARAM] = {
  11.26561, 4.18288, 3.05733, 3.80008, 4.35318, 6.42340, 1.36441, -0.38185,
  -0.81545, -6.24532, -2.62967, 1.49745, 3.12582, 3.96890, 1.06205, -1.62528,
  4.04349, 1.01429, 1.21342, -0.79873, 0.84218, 0.76534, 0.26573, 4.77343,
  2.99879, 7.12192, 0.78874, 0.84929, -0.02068, 0.37062, 0.14525, 5.89200,
  3.27319, 3.76581, 0.17133, -0.47667, -0.77680
};
static constexpr const char* V6FIT_PROVENANCE = "fitC.jsonl [final weights record] sha256:be4a4c31d9153208 gens=14 obj=minimaxregret paired=1 panel=v05+v03+withholder+feint seed=20260824 deals=200x2 sigmaRel=0.04";
// FIT-VECTOR-END

inline void applyV6Params(V05Config& c, V06Extra& x, const double* v) {
  for (int i = 0; i < NFEAT; i++) c.w[i] = v[i];
  const int K = NFEAT;
  c.declThreshold     = std::min(0.9999,  std::max(0.5,  v[K + 0]));
  c.lockedAllocThresh = std::min(0.99999, std::max(0.5,  v[K + 1]));
  c.askFloor          = std::min(0.9,     std::max(0.0,  v[K + 2]));
  c.patiencePool      = std::max(0, std::min(45, int(std::lround(v[K + 3]))));
  c.oppCardFloor      = std::max(0.0, std::min(20.0, v[K + 4]));
  c.valueWeight       = std::max(0.0, v[K + 5]);
  c.linearWeight      = std::max(0.0, v[K + 6]);
  c.minTeamProb       = std::min(0.99, std::max(0.05, v[K + 7]));
  c.declareMargin     = v[K + 8];
  c.priorTheta        = std::max(0.0, std::min(2.0, v[K + 9]));
  c.priorPhi          = std::max(0.0, std::min(1.0, v[K + 10]));
  c.searchTopK        = std::max(0, std::min(24, int(std::lround(v[K + 11]))));
  c.chainWeight       = std::max(0.0, v[K + 12]);
  c.threatWeight      = std::max(0.0, v[K + 13]);
  x.wVoid    = v[K + 14];
  x.wTeamHas = v[K + 15];
  x.wLastLive= v[K + 16];
  x.extraFeats = (x.wVoid != 0.0 || x.wTeamHas != 0.0 || x.wLastLive != 0.0);
}

struct V06Agent : V05Agent {
  V06Extra x;
  v06::RolloutEngine roll;
  Rng srng;
  BlockDP xb;                 // exact posterior, built on demand
  bool    xbOk = false;
  double  xmu[NCARD][NPLAY];  // exact marginals for the current decision
  bool    xmuOk = false;
  long long xbBuilds = 0, xbFails = 0, tieSeen = 0, tieBroken = 0;
  // A5 bookkeeping: (card, target) pairs this seat has already asked and that
  // were provably dead when asked, cleared when the card publicly moves to that
  // target (at which point the pair can become live again).
  uint64_t publicHash = 0x9E3779B97F4A7C15ull;   // rolling hash of the public stream
  bool deadTried[NCARD][NPLAY] = {};
  int  deadUsed = 0;
  long long deadPlayed = 0, deadOffered = 0;
  // instrumentation, read by `fish v6probe`
  long long searched = 0, decisions = 0, changed = 0, dpFail = 0, rejected = 0;

  V06Agent() { label = "fishbot_v06"; applyV6Params(cfg, x, V6PARAMS); }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V05Agent::reset(s, hand, r, seed);
    resetV6(seed);
  }
  void resetWithKnowledge(int s, uint64_t hand, const Rules& r, uint64_t seed,
                          const Knowledge& k0) override {
    V05Agent::resetWithKnowledge(s, hand, r, seed, k0);
    resetV6(seed);
  }
  void resetV6(uint64_t seed) {
    srng = Rng(mixSeed(seed, 0x0606ull));
    xbOk = xmuOk = false;
    memset(deadTried, 0, sizeof(deadTried));
    deadUsed = 0;
    publicHash = 0x9E3779B97F4A7C15ull;
  }
  void observe(const Event& e) override {
    V05Agent::observe(e);
    xbOk = xmuOk = false;
    publicHash = mixSeed(publicHash,
        (uint64_t(uint8_t(e.kind)) << 32) | (uint64_t(e.actor) << 24) |
        (uint64_t(e.target) << 16) | (uint64_t(e.card) << 8) | uint64_t(e.success));
    // A5(d): a successful ask moves the card to the ASKER, so any dead pair
    // naming that card at that seat may now be live.  Clear it.
    if (e.kind == Kind::Ask && e.success) for (int t = 0; t < NPLAY; t++) deadTried[e.card][t] = false;
  }

  // Build the exact posterior for this decision.  Guarded by the unresolved
  // count: the DP state space is prod_p (q_p + 1) and the build is 92.8 us at
  // the opening against 11 us once the game has resolved (R10 section 3), so
  // the guard is a cost control, not a correctness one.
  bool ensureExact() {
    if (xbOk) return xb.ok;
    xbOk = true; xmuOk = false;
    if (__builtin_popcountll(k.unresolved) > x.exactMaxQ) { xbFails++; return false; }
    bool r = xb.build(k);
    xbBuilds++;
    if (!r) xbFails++;
    return r;
  }
  bool ensureExactMarginals() {
    if (xmuOk) return xb.ok;
    if (!ensureExact()) return false;
    for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) xmu[c][p] = 0.0;
    for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) xmu[c][k.owner[c]] = 1.0;
    xb.marginals(xmu);
    xmuOk = true;
    return true;
  }

  // ---- A2/A3 extra ask terms -----------------------------------------------
  //
  // g0 VOID.  legalAsk requires the actor to hold a card of the half-suit
  // (fish.hpp:164), so taking an opponent's last card of a half-suit
  // PERMANENTLY removes their right to ask in it.  41.65% of v0.5's successful
  // asks already create a void by accident and nothing in its 20-feature score
  // rewards doing it on purpose; the defensive dual (f[8] reply threat) is
  // implemented (research/v06/notes/R8, R0 V6-M11).
  //
  // g1 TEAM-HAS.  9.74% of v0.5's asks -- 21.97% of every miss it makes -- go
  // into a half-suit its own team already owns outright.  M1 structurally
  // cannot see them: enumerateAsks only permits asking opponents, and no seat
  // can PROVE a teammate holds a card.  Pricing the posterior mass our own team
  // already carries is the graded version of the proof M1 cannot have.
  //
  // g2 LAST-LIVE.  f[11] rewards emptying a target's hand without distinguishing
  // "one of three opponents" from "the last live opponent", and emptying the
  // last one triggers the forced endgame.
  void extraTerms(const PublicState& pub, int card, int target, double p, double* g) const {
    int S = setOf(card);
    double none = 1.0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(S, i);
      if (c == card) continue;
      none *= (1.0 - bel.marg[c][target]);
    }
    g[0] = p * none;
    g[1] = pTeamCard(card);
    int live = 0;
    for (int q = 0; q < NPLAY; q++) if ((oppMask & (1 << q)) && pub.handCount[q]) live++;
    g[2] = (live <= 1 && pub.handCount[target] == 1) ? p : 0.0;
  }

  // The blueprint score v0.5 maximises, without the top-K chain/threat pass.
  // Used only to rank candidates for the search; when the search is off the
  // base class runs unchanged, including that pass.
  virtual double blueprintScore(const PublicState& pub, int card, int target, double* fOut) {
    double f[NFEAT];
    features(pub, card, target, f);
    double u = 0;
    for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
    u *= cfg.linearWeight;
    if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, card, target, f[0]);
    if (x.extraFeats) {
      double g[3];
      extraTerms(pub, card, target, f[0], g);
      u += cfg.linearWeight * (x.wVoid * g[0] + x.wTeamHas * g[1] + x.wLastLive * g[2]);
    }
    if (cfg.plantKind) u += plantTerm(pub, card, target);
    if (fOut) *fOut = f[0];
    return u;
  }

  // v0.5's top-K chain/threat re-scoring, operating on the v0.6 score vector so
  // that the extra ask terms are not discarded by it.  Reproduces v05.hpp's
  // branch-belief construction exactly.
  int chainRescore(const PublicState& pub, const AskMove* buf, const std::vector<int>& ord,
                   int n, const std::vector<double>& u) {
    int K = std::min(cfg.searchTopK, n);
    int best = ord[0]; double bestU = -1e18;
    for (int r = 0; r < K; r++) {
      int i = ord[size_t(r)];
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
      double uu = u[size_t(i)] + cfg.chainWeight * p * follow - cfg.threatWeight * (1 - p) * threat;
      if (uu > bestU) { bestU = uu; best = i; }
    }
    return best;
  }

  // v0.7 D1 on the v0.6 scoring path.  The candidate scores are already in
  // hand here, so the tie group and the margin are free -- which matters,
  // because the tie group is 53.2% of this policy's ask decisions
  // (research/v06/results/E8-ties.txt) and is invisible in any per-game metric.
  void captureV6(int n, const std::vector<double>& u, const std::vector<int>& ord,
                 int tie, AskMove pick, double p) {
    lastDec.clear();
    lastDec.nCand = n;
    lastDec.nTie = tie;
    lastDec.score = u[size_t(ord[0])];
    lastDec.margin = (n >= 2 && tie < 2) ? (u[size_t(ord[0])] - u[size_t(ord[1])]) : 0.0;
    lastDec.p = p;
    lastDec.dead = provablyDead(pick.card, pick.target);
  }

  // The live-ask gate's binding rate, on the v0.6 scoring path.  Ledger entry
  // L10 is the hypothesis that M1 -- a naive knowledge-limited pruning rule, of
  // exactly the kind Zhang & Sandholm PROVE can increase exploitability -- is an
  // untested hazard, and the quantity an exploiter would try to raise is this
  // one: how often the gate removes what would otherwise have been the argmax.
  // The corpus measured it once at 5.78% of decisions (R1) and it is a property
  // of DECISIONS, so it is invisible in every per-game metric.
  void captureGateBind(const PublicState& pub) {
    if (!cfg.liveAskGate) { lastDec.gateBound = false; return; }
    AskMove all[NSET * SETSZ * 3];
    int na = enumerateAsks(pub, k.myHand, seat, all);
    double best = -1e18; int bi = -1;
    for (int i = 0; i < na; i++) {
      double u = blueprintScore(pub, all[i].card, all[i].target, nullptr);
      if (u > best) { best = u; bi = i; }
    }
    lastDec.gateBound = (bi >= 0) && provablyDead(all[bi].card, all[bi].target);
  }

  // A subclass that widens the score has to reach the v0.6 candidate path even
  // with every v0.6 switch off; and it may want dead candidates in the
  // enumeration, which the v0.6 path never does.
  virtual bool wantV6Path() const { return x.search || x.extraFeats || x.deadAsk; }
  virtual int enumerateForScore(const PublicState& pub, AskMove* buf) {
    return cfg.liveAskGate ? enumerateLive(pub, buf) : enumerateAsks(pub, k.myHand, seat, buf);
  }
  virtual void prepareScore(const PublicState& pub) { (void)pub; }

  AskMove chooseAsk(const PublicState& pub) override {
    if (!wantV6Path()) return V05Agent::chooseAsk(pub);
    refresh();
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateForScore(pub, buf);
    if (n <= 0) return AskMove{0, 0};
    decisions++;
    if (n == 1) { lastMySet = setOf(buf[0].card); lastAskP = bel.marg[buf[0].card][buf[0].target]; return buf[0]; }
    const bool doSearch = x.search
      && int(pub.nEvents) >= x.searchFrom
      && (x.searchMaxQ <= 0 || __builtin_popcountll(k.unresolved) <= x.searchMaxQ);

    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    prepareScore(pub);
    std::vector<double> u(n), pp(n);
    for (int i = 0; i < n; i++) { double p; u[i] = blueprintScore(pub, buf[i].card, buf[i].target, &p); pp[i] = p; }
    std::vector<int> ord(n);
    for (int i = 0; i < n; i++) ord[i] = i;
    std::sort(ord.begin(), ord.end(), [&](int a, int b) { return u[a] > u[b]; });

    // ---- A5: dead candidates offered to the search --------------------------
    AskMove deadCand[6]; int nDead = 0;
    if (x.deadInSearch > 0 && deadUsed < x.deadBudget) {
      AskMove all[NSET * SETSZ * 3];
      int na = enumerateAsks(pub, k.myHand, seat, all);
      struct DC { double u; int i; };
      std::vector<DC> dcs;
      double f[NFEAT];
      for (int i = 0; i < na; i++) {
        int c = all[i].card, t = all[i].target;
        if (!provablyDead(c, t)) continue;
        if (deadTried[c][t]) continue;
        features(pub, c, t, f);
        f[3] = f[5] = f[7] = f[15] = 0.0;
        double s2 = 0;
        for (int j = 0; j < NFEAT; j++) s2 += cfg.w[j] * f[j];
        dcs.push_back(DC{s2 * cfg.linearWeight, i});
      }
      std::sort(dcs.begin(), dcs.end(), [](const DC& a, const DC& b) { return a.u > b.u; });
      // Keep at most one per distinct target: the point of the move is choosing
      // WHO receives the turn, so two dead asks at the same seat are near
      // duplicates and would waste the rollout budget.
      bool used[NPLAY] = {false,false,false,false,false,false};
      for (const DC& d : dcs) {
        if (nDead >= x.deadInSearch || nDead >= 6) break;
        int t = all[d.i].target;
        if (used[t]) continue;
        used[t] = true;
        deadCand[nDead++] = all[d.i];
      }
    }

    // ---- A5: the linear-score gate on the deliberate miss --------------------
    // Scored on the same linear rule with the four ownership features zeroed,
    // which is what makes p = 0 unattractive rather than attractive.
    int deadBest = -1; double deadBestU = -1e18;
    if (x.deadAsk && deadUsed < x.deadBudget) {
      AskMove all[NSET * SETSZ * 3];
      int na = enumerateAsks(pub, k.myHand, seat, all);
      double f[NFEAT];
      for (int i = 0; i < na; i++) {
        int c = all[i].card, t = all[i].target;
        if (!provablyDead(c, t)) continue;
        if (deadTried[c][t]) continue;
        features(pub, c, t, f);
        f[3] = f[5] = f[7] = f[15] = 0.0;      // the p = 0 ownership incentive
        double s2 = 0;
        for (int j = 0; j < NFEAT; j++) s2 += cfg.w[j] * f[j];
        s2 *= cfg.linearWeight;
        if (cfg.useValue) s2 += cfg.valueWeight * askExpectedValue(pub, c, t, 0.0);
        if (s2 > deadBestU) { deadBestU = s2; deadBest = i; }
      }
      if (deadBest >= 0) {
        deadOffered++;
        if (deadBestU > u[ord[0]] + x.deadMargin) {
          AskMove m = all[deadBest];
          deadTried[m.card][m.target] = true;
          deadUsed++; deadPlayed++;
          lastMySet = setOf(m.card); lastAskP = 0.0;
          return m;
        }
      }
    }


    // Candidate set: the top K, extended through every candidate that ties the
    // K-th within tieEps.  The extension is load-bearing: at 55% of v0.5's ask
    // decisions two or more candidates are numerically identical at the top
    // (research/v06/notes/R10), and a plain top-K would cut the tie arbitrarily
    // -- reproducing exactly the defect the search exists to remove.
    // The tie group: candidates the blueprint cannot separate from its own top.
    int tie = 1;
    while (tie < n && u[ord[tie]] >= u[ord[0]] - x.tieEps) tie++;
    int K = std::min(n, std::max(2, x.topK));
    while (K < n && K < x.maxCand && u[ord[K]] >= u[ord[K - 1]] - x.tieEps) K++;
    if (x.tieOnly) {
      if (tie < 2) { lastMySet = setOf(buf[ord[0]].card); lastAskP = pp[ord[0]];
                     return buf[ord[0]]; }
      K = std::min(tie, x.maxCand);
    }
    if (!doSearch) {
      int pick = ord[0];
      if (x.randomTie && tie >= 2) {
        Rng r(mixSeed(publicHash, uint64_t(seat) * 1000003 + uint64_t(pub.nEvents)));
        pick = ord[size_t(r.u32(uint32_t(tie)))];
      }
      if (x.chainPass && cfg.searchTopK > 1) pick = chainRescore(pub, buf, ord, n, u);
      lastMySet = setOf(buf[pick].card); lastAskP = pp[pick];
      if (decisionCapture()) { captureV6(n, u, ord, tie, buf[pick], pp[pick]); captureGateBind(pub); }
      return buf[pick];
    }

    if (x.minGap >= 0 && (u[ord[0]] - u[ord[1]]) > x.gapEps) {
      lastMySet = setOf(buf[ord[0]].card); lastAskP = pp[ord[0]];
      return buf[ord[0]];
    }

    // Determinize.  DealDP is the exact uniform sampler over deals satisfying
    // C1-C4; C5 certificates are enforced by rejection, exactly as
    // Belief::ensureDP does, so accepted draws are exact samples of the
    // policy-agnostic posterior.  The policy prior (theta/phi) is then applied
    // as an importance weight rather than being baked into the sampler, which
    // keeps the sampler exact and the prior auditable.
    DealDP dp;
    if (!dp.build(k)) { dpFail++; return V05Agent::chooseAsk(pub); }

    const int wantDet = std::max(1, x.nDet);
    std::vector<std::array<uint8_t, NCARD>> det;
    std::vector<double> wt;
    det.reserve(size_t(wantDet)); wt.reserve(size_t(wantDet));
    std::array<uint8_t, NCARD> base{};
    for (int c = 0; c < NCARD; c++) base[c] = k.owner[c];
    int tries = 0, maxTries = wantDet * 40 + 200;
    while (int(det.size()) < wantDet && tries < maxTries) {
      tries++;
      std::array<uint8_t, NCARD> o = base;
      dp.sample(srng, o.data());
      if (!Belief::satisfies(k, o.data())) { rejected++; continue; }
      double w = 1.0;
      if (x.policyPrior && (cfg.priorTheta != 0 || cfg.priorPhi != 0)) {
        uint64_t un = k.unresolved;
        while (un) { int c = __builtin_ctzll(un); un &= un - 1;
          w *= k.priorWeight(c, o[c], cfg.priorTheta, cfg.priorPhi); }
      }
      det.push_back(o); wt.push_back(w);
    }
    if (det.empty()) { dpFail++; return V05Agent::chooseAsk(pub); }
    { double s = 0; for (double v : wt) s += v;
      if (!(s > 0)) { for (double& v : wt) v = 1.0; s = double(wt.size()); }
      for (double& v : wt) v /= s; }

    // Roll out.  Common random numbers: every candidate is evaluated on the
    // same determinization set, so the comparison is paired and the sampling
    // noise that matters is the noise in the DIFFERENCE, not in the level.
    roll.cfg.rolloutSpec = x.rolloutSpec();
    roll.cfg.oppSpec = x.rollOpp;
    roll.cfg.myTeam = teamOf(seat);
    roll.cfg.maxDepth = x.maxDepth;
    if (roll.cfg.leafSpec != x.leafSpec || roll.cfg.leafLambda != x.leafLambda) {
      roll.cfg.leafSpec = x.leafSpec; roll.cfg.leafLambda = x.leafLambda; roll.rebuildEvaluator();
    }
    roll.beginDecision(pub);
    const int team = teamOf(seat);
    // The search's candidate list: K live candidates in blueprint order,
    // followed by the dead candidates.  Index 0 stays the blueprint's own
    // choice, so the paired deviation rule keeps its meaning.
    std::vector<AskMove> cand;
    cand.reserve(size_t(K) + size_t(nDead));
    for (int r = 0; r < K; r++) cand.push_back(buf[ord[r]]);
    for (int r = 0; r < nDead; r++) cand.push_back(deadCand[r]);
    const int KC = int(cand.size());
    const int D = int(det.size());
    const size_t NV = size_t(KC) * size_t(D);
    std::vector<double> val(NV, 0.0);
    // v0.7 T1 (d): the batch leaf path.  A truncated rollout writes its leaf's
    // feature row here and returns without pricing it; every leaf of the
    // decision is then priced in ONE call.  With `maxDepth = 0` nothing is
    // truncated and the buffers are never allocated, so the unrestricted search
    // is untouched.
    std::vector<double> leafBuf;
    std::vector<uint8_t> wasTrunc;
    const bool batchLeaves = x.maxDepth > 0;
    if (batchLeaves) { leafBuf.assign(NV * size_t(NLEAF), 0.0); wasTrunc.assign(NV, 0); }
    uint64_t hand[NPLAY];
    Knowledge seatK[NPLAY];
    for (int d = 0; d < D; d++) {
      v06::RolloutEngine::handsFrom(pub, k, det[size_t(d)].data(), hand);
      for (int j = 0; j < NPLAY; j++) { seatK[j] = roll.pubK; v06::refineWithHand(seatK[j], j, hand[j]); }
      uint64_t rs = mixSeed(srng.next(), uint64_t(d) * 7919 + 11);
      for (int r = 0; r < KC; r++) {
        for (int j = 0; j < NPLAY; j++)
          roll.ag[j]->resetWithKnowledge(j, hand[j], pub.rules, mixSeed(rs, uint64_t(j) * 131 + 7), seatK[j]);
        size_t idx = size_t(r) * size_t(D) + size_t(d);
        if (batchLeaves) {
          bool tr = false;
          val[idx] = roll.playOut(hand, seat, team, &cand[size_t(r)],
                                  leafBuf.data() + idx * size_t(NLEAF), &tr);
          wasTrunc[idx] = uint8_t(tr ? 1 : 0);
        } else {
          val[idx] = roll.playOut(hand, seat, team, &cand[size_t(r)]);
        }
      }
    }
    if (batchLeaves) {
      std::vector<double> pack; std::vector<size_t> where;
      pack.reserve(NV * size_t(NLEAF)); where.reserve(NV);
      for (size_t i = 0; i < NV; i++) if (wasTrunc[i]) {
        where.push_back(i);
        pack.insert(pack.end(), leafBuf.begin() + long(i * NLEAF), leafBuf.begin() + long((i + 1) * NLEAF));
      }
      if (!where.empty()) {
        std::vector<double> out(where.size());
        roll.evaluator().valueBatch(pack.data(), int(where.size()), out.data());
        for (size_t j = 0; j < where.size(); j++) val[where[j]] = out[j];
      }
    }
    searched++;

    // Paired lower-confidence-bound deviation rule.  Candidate 0 is the
    // blueprint's own choice; every other candidate must beat it, on the SAME
    // determinizations, by kappa standard errors of the paired difference.
    double wsum2 = 0;
    for (int d = 0; d < D; d++) wsum2 += wt[size_t(d)] * wt[size_t(d)];
    const double ess = wsum2 > 0 ? 1.0 / wsum2 : 1.0;         // Kish effective sample size
    int bestR = 0; double bestLcb = 0.0;
    for (int r = 1; r < KC; r++) {
      double m = 0;
      for (int d = 0; d < D; d++)
        m += wt[size_t(d)] * (val[size_t(r) * size_t(D) + size_t(d)] - val[size_t(d)]);
      double var = 0;
      for (int d = 0; d < D; d++) {
        double e = (val[size_t(r) * size_t(D) + size_t(d)] - val[size_t(d)]) - m;
        var += wt[size_t(d)] * e * e;
      }
      double se = ess > 1.5 ? std::sqrt(std::max(0.0, var) * ess / (ess - 1.0) / ess) : 1e9;
      double kap = (r < tie) ? x.kappaTie : x.kappa;
      double lcb = m - kap * se + (r < K ? x.blend * (u[ord[r]] - u[ord[0]]) : 0.0);
      if (lcb > bestLcb) { bestLcb = lcb; bestR = r; }
    }
    if (bestR != 0) changed++;
    AskMove pick = cand[size_t(bestR)];
    if (bestR >= K) { deadTried[pick.card][pick.target] = true; deadUsed++; deadPlayed++; }
    lastMySet = setOf(pick.card);
    lastAskP = bestR < K ? pp[ord[bestR]] : 0.0;
    if (decisionCapture()) {
      captureV6(n, u, ord, tie, pick, lastAskP);
      captureGateBind(pub);
      lastDec.searched = true; lastDec.changed = (bestR != 0);
    }
    return pick;
  }
};

} // namespace fish
