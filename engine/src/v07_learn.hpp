// FishBot v0.7 phase 3, candidate K5 -- the corpus's first learned component.
//
// The bet.  Test-time search is worth +1.89 to +2.52 points over the deployed
// policy (INSTRUMENT.md section 4.3) and the deployed policy ships it OFF because it
// costs 242x (section 4.1).  That is a pure cost problem, so the affordable learned
// object is an AMORTISATION: a function of the information available at
// decision time, trained to reproduce what the search decides, deployed at
// blueprint cost.
//
// This file holds three things:
//   1. the capture sink that turns searched decisions into labelled
//      per-candidate rows (the hook itself is in v06.hpp, null by default);
//   2. `V07LAgent`, a V06Agent whose search is replaced by a fixed linear
//      re-ranker over the same candidate set -- the amortised policy;
//   3. the drivers `fish7 v7learn` runs.
//
// Determinism.  The learned score is a dot product of fixed double weights with
// a feature row built in a fixed order from integer and double state that the
// blueprint already computes.  No RNG, no thread-dependent accumulation, no
// float storage.  The identity control (`lw` unset => V07LAgent is never
// constructed) and the twice-run reproducibility check are both in the driver.
#pragma once
#include "v06.hpp"
#include <mutex>
#include <fstream>
#include <sstream>

namespace fish {
namespace v07learn {

// ---- capture ---------------------------------------------------------------

struct CapSink {
  std::mutex mu;
  std::vector<double> rows;      // flat, V7L_FEATW + 5 doubles per row
  long long decisions = 0;
};
inline CapSink& capSink() { static CapSink s; return s; }

// Emitted layout = 5 identifying columns then the V7L_FEATW payload.
static const int V7L_IDW = 5;    // deal, rot, event, seat, decisionId

inline void capHandler(const SearchCapture& sc) {
  CapSink& s = capSink();
  std::lock_guard<std::mutex> g(s.mu);
  long long did = s.decisions++;
  for (int r = 0; r < sc.rows; r++) {
    const double* f = sc.f + size_t(r) * size_t(V7L_FEATW);
    s.rows.push_back(double(sc.deal));
    s.rows.push_back(double(sc.rot));
    s.rows.push_back(double(sc.event));
    s.rows.push_back(double(sc.seat));
    s.rows.push_back(double(did));
    for (int j = 0; j < V7L_FEATW; j++) s.rows.push_back(f[j]);
  }
}

inline const char* colName(int j) {
  static const char* n[V7L_IDW + V7L_FEATW] = {
    "deal","rot","event","seat","did",
    "r","isTie","du","p",
    "f0","f1","f2","f3","f4","f5","f6","f7","f8","f9",
    "f10","f11","f12","f13","f14","f15","f16","f17","f18","f19",
    "mOwn","mMate1","mMate2","mTgt","mOpp1","mOpp2",
    "targetRel","tgtHand","setUnres","myInSet","cardIdxInSet","card",
    "unres","nEvents","myCards","ourCards","theirCards","lead",
    "n","tie","K","KC","D",
    "m","se","lcb","chosen" };
  return n[j];
}

// ---- the learned re-ranker -------------------------------------------------
//
// Deployment shape.  The blueprint enumerates and scores the candidates exactly
// as v0.6 does; the learned function then scores the same top-K set and the
// argmax of the learned score is played.  Nothing else in the policy moves, so
// with an all-zero weight vector the agent is v0.6 by construction (the learned
// score is then constant and the argmax is candidate 0).
//
// The weight vector is applied to a REDUCED row: the deployed model may not
// read a payoff-irrelevant label (THREAT-MODEL.md I-2), so `cardIdxInSet` and
// `card` are hard-excluded here rather than merely left unfitted.
struct LearnModel {
  bool   on = false;
  double bias = 0.0;                     // added to every non-blueprint candidate
  double w[V7L_FEATW] = {};              // indices 34, 35, 47..50 are never used
  double margin = 0.0;                   // deviate only if learned score beats
                                         // candidate 0 by this much
  bool   tieOnly = false;                // restrict deviations to the tie group
  int    maxQ = 0;                       // act only when |unresolved| <= maxQ
};

inline bool excludedColumn(int j) {
  return j == 34 || j == 35 || j >= 47;   // I-2 labels, and the search's own verdict
}

inline bool parseModel(const std::string& s, LearnModel& m) {
  // "bias|idx:weight|idx:weight|..." -- sparse, because the fitted models this
  // phase deploys touch a handful of coordinates.
  std::stringstream ss(s); std::string tok; bool first = true;
  while (std::getline(ss, tok, '|')) {
    if (tok.empty()) continue;
    if (first) { m.bias = atof(tok.c_str()); first = false; continue; }
    size_t c = tok.find(':');
    if (c == std::string::npos) return false;
    int idx = atoi(tok.substr(0, c).c_str());
    if (idx < 0 || idx >= V7L_FEATW || excludedColumn(idx)) return false;
    m.w[idx] = atof(tok.substr(c + 1).c_str());
  }
  m.on = true;
  return true;
}

} // namespace v07learn

// The amortised policy.  It derives from V06Agent so that every switch v0.6
// ships keeps its meaning; `x.search` stays OFF, so no rollout ever runs and
// the cost is the blueprint's.
struct V07LAgent : V06Agent {
  v07learn::LearnModel lm;
  long long lDecisions = 0, lChanged = 0;
  const char* name() const override { return "v07l"; }
  bool wantV6Path() const override { return true; }

  // Score one candidate under the learned model, on the same reduced row the
  // fit saw.  Candidate 0 scores exactly 0 by construction, so the model is a
  // learned ADVANTAGE over the blueprint's own choice and an all-zero weight
  // vector reproduces v0.6 exactly.
  double learnedAdv(const PublicState& pub, const AskMove& mv, int r, int tie,
                    double du, double p, int n, int K, int myCards, int ourCards,
                    int theirCards, int lead) {
    if (r == 0) return 0.0;
    double f[NFEAT];
    features(pub, mv.card, mv.target, f);
    double row[V7L_FEATW] = {};
    row[0] = r; row[1] = (r < tie) ? 1 : 0; row[2] = du; row[3] = p;
    for (int j = 0; j < NFEAT; j++) row[4 + j] = f[j];
    { int slot = 0;
      row[24 + slot++] = bel.marg[mv.card][seat];
      for (int q = 0; q < NPLAY; q++) if (q != seat && (teamMask & (1 << q))) row[24 + slot++] = bel.marg[mv.card][q];
      row[24 + slot++] = bel.marg[mv.card][mv.target];
      for (int q = 0; q < NPLAY; q++) if ((oppMask & (1 << q)) && q != mv.target) row[24 + slot++] = bel.marg[mv.card][q];
    }
    const int S = setOf(mv.card);
    row[30] = double((int(mv.target) - seat + NPLAY) % NPLAY);
    row[31] = pub.handCount[mv.target] / 9.0;
    row[32] = double(__builtin_popcountll(k.unresolved & setMask(S)));
    row[33] = double(__builtin_popcountll(k.myHand & setMask(S)));
    row[36] = double(__builtin_popcountll(k.unresolved));
    row[37] = double(pub.nEvents);
    row[38] = double(myCards); row[39] = double(ourCards); row[40] = double(theirCards);
    row[41] = double(lead);
    row[42] = double(n); row[43] = double(tie); row[44] = double(K);
    row[45] = double(K); row[46] = 0.0;
    double a = lm.bias;
    for (int j = 0; j < V7L_FEATW; j++) if (!v07learn::excludedColumn(j)) a += lm.w[j] * row[j];
    return a;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    if (!lm.on) return V06Agent::chooseAsk(pub);
    refresh();
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateForScore(pub, buf);
    if (n <= 0) { if (decisionCapture()) { lastDec.clear(); lastDec.nCand = 0; captureGateBind(pub); }
                  return AskMove{0, 0}; }
    decisions++;
    if (n == 1) {
      lastMySet = setOf(buf[0].card); lastAskP = bel.marg[buf[0].card][buf[0].target];
      if (decisionCapture()) {
        lastDec.clear(); lastDec.nCand = 1; lastDec.nTie = 1; lastDec.margin = 0.0;
        lastDec.p = lastAskP; lastDec.dead = provablyDead(buf[0].card, buf[0].target);
        captureGateBind(pub);
      }
      return buf[0];
    }
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    prepareScore(pub);
    std::vector<double> u(n), pp(n);
    for (int i = 0; i < n; i++) { double p; u[i] = blueprintScore(pub, buf[i].card, buf[i].target, &p); pp[i] = p; }
    std::vector<int> ord(n);
    for (int i = 0; i < n; i++) ord[i] = i;
    std::sort(ord.begin(), ord.end(), [&](int a, int b) { return u[a] > u[b]; });

    int tie = 1;
    while (tie < n && u[ord[tie]] >= u[ord[0]] - x.tieEps) tie++;
    int K = std::min(n, std::max(2, x.topK));
    while (K < n && K < x.maxCand && u[ord[K]] >= u[ord[K - 1]] - x.tieEps) K++;

    const bool act = (lm.maxQ <= 0 || __builtin_popcountll(k.unresolved) <= lm.maxQ);
    int pick = 0;
    if (act) {
      lDecisions++;
      const int team = teamOf(seat);
      int myCards = pub.handCount[seat], ourCards = 0, theirCards = 0;
      for (int q = 0; q < NPLAY; q++) {
        if (teamMask & (1 << q)) ourCards += pub.handCount[q]; else theirCards += pub.handCount[q];
      }
      const int lead = int(pub.score[team]) - int(pub.score[1 - team]);
      int hi = lm.tieOnly ? std::min(tie, K) : K;
      double best = lm.margin;
      for (int r = 1; r < hi; r++) {
        double a = learnedAdv(pub, buf[ord[r]], r, tie, u[ord[r]] - u[ord[0]], pp[ord[r]],
                              n, K, myCards, ourCards, theirCards, lead);
        if (a > best) { best = a; pick = r; }
      }
      if (pick != 0) lChanged++;
    }
    lastMySet = setOf(buf[ord[pick]].card); lastAskP = pp[ord[pick]];
    if (decisionCapture()) {
      captureV6(n, u, ord, tie, buf[ord[pick]], pp[ord[pick]]);
      captureGateBind(pub);
      lastDec.searched = act; lastDec.changed = (pick != 0);
    }
    return buf[ord[pick]];
  }
};

} // namespace fish
