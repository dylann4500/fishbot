// FishBot v0.7 -- phase-1 instrument drivers.
#pragma once
#include "arena.hpp"
#include "v07_responder.hpp"
#include "v07_seeds.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace fish {
namespace v07 {

// ---------------------------------------------------------------------------
// W1.  The bit measurement -- ledger L4's "cheapest experiment", which needs no
// games beyond the transcripts themselves.
//
// At each ask by a target seat, from one observer's seat: how much of the
// observer's EXACT posterior does the observed action leave alive, given that
// the observer knows the target's policy?  The sample already satisfies every
// certificate, so the contraction is entirely beyond what the rules force.
// "If the contraction is under ~1 bit per ask the hypothesis dies for free, and
// it dies before anyone builds a class-C5 responder."
//
// The second half of the probe is the part that decides whether bits matter:
// the observer's marginals are rebuilt with and without the accumulated
// evidence and scored against ground truth on the SAME states, in nats and
// argmax-hit -- the two numbers ledger entry C2 already reports for the three
// inference paths (1.38218 nats / 51.49% for the deployed approximation).
struct BitResult {
  long long asks = 0, inverted = 0, skipped = 0;
  double bitsSum = 0, bitsSq = 0;
  double qSum = 0;
  // by event bucket (0-19, 20-39, ... 100+)
  double bucketBits[6] = {0,0,0,0,0,0};
  long long bucketN[6] = {0,0,0,0,0,0};
  // predictive quality of the observer's marginals over UNRESOLVED cards
  long long predN = 0;
  double natBase = 0, natInv = 0;
  long long hitBase = 0, hitInv = 0;
  double oracleCalls = 0;
  double seconds = 0;
};

inline void scoreMarginals(const Knowledge& kk, const uint64_t* truth,
                           const double marg[NCARD][NPLAY],
                           long long& n, double& nats, long long& hits) {
  uint64_t u = kk.unresolved;
  while (u) {
    int c = __builtin_ctzll(u); u &= u - 1;
    int t = -1;
    for (int q = 0; q < NPLAY; q++) if (truth[q] & bit(c)) t = q;
    if (t < 0) continue;
    double s = 0;
    for (int q = 0; q < NPLAY; q++) s += marg[c][q];
    if (!(s > 0)) continue;
    double p = std::max(1e-12, marg[c][t] / s);
    nats += -std::log(p);
    int best = 0; double bp = -1;
    for (int q = 0; q < NPLAY; q++) if (marg[c][q] > bp) { bp = marg[c][q]; best = q; }
    if (best == t) hits++;
    n++;
  }
}

struct BitProbeConfig {
  std::string specA = "v06", specB = "v06";
  std::string model = "";        // "" = the target's own spec (a true white-box grant)
  int games = 100, rotations = 2, nDet = 64, fromEvent = 0, maxQ = 0, everyK = 1;
  uint64_t seed = 7012001;
  Rules rules;
  double gain = 1.0, alpha = 0.5, kappa = 3.0, stepClip = 1.25, clip = 5.0;
  int mode = 0, focus = 1;
  double theta = 0.44458, phi = 0.12198, thetaInv = -1, phiInv = -1;
  int threads = 0;
};

inline BitResult runBitProbe(const BitProbeConfig& bc) {
  int nThreads = bc.threads > 0 ? bc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, bc.games));
  std::vector<BitResult> local(nThreads);
  std::atomic<int> next{0};
  auto t0 = std::chrono::steady_clock::now();
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      BitResult& R = local[t];
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(bc.specA); B[i] = makeAgent(bc.specB); }
      Inverter inv;
      inv.oracle.spec = bc.model.empty() ? bc.specB : bc.model;
      inv.nDet = bc.nDet; inv.fromEvent = bc.fromEvent; inv.maxQ = bc.maxQ;
      inv.gain = bc.gain; inv.alpha = bc.alpha; inv.kappa = bc.kappa;
      inv.stepClip = bc.stepClip; inv.mode = bc.mode; inv.focus = bc.focus;
      Game game;
      for (;;) {
        int i = next.fetch_add(1, std::memory_order_relaxed);
        if (i >= bc.games) break;
        uint64_t s = mixSeed(bc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < bc.rotations; rot++) {
          int orient = (bc.rotations == 2) ? rot : (rot / 3);
          int shift  = (bc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          game.trace.on = true; game.trace.events.clear();
          game.run(s, bc.rules, ag);
          // The observer is a seat of team A; the targets are team B's seats,
          // which carry `specB`, the policy the oracle models.
          const int obsSeat = orient;              // lowest seat of team A
          uint64_t dealt[NPLAY];
          for (int p = 0; p < NPLAY; p++) dealt[p] = game.g.dealt[p];
          Knowledge obs; obs.init(obsSeat, dealt[obsSeat], bc.rules.deckSets);
          inv.reset(bc.rules, bc.rules.deckSets, mixSeed(s, 0xB175ull));
          long long seen = 0;
          for (const Event& e : game.trace.events) {
            if (e.kind == Kind::Ask && teamOf(e.actor) != teamOf(obsSeat)) {
              R.asks++;
              if ((seen++ % std::max(1, bc.everyK)) == 0) {
                double bits = 0;
                double q = inv.invertAsk(obs, e, &bits);
                if (q < 0) R.skipped++;
                else {
                  R.inverted++; R.bitsSum += bits; R.bitsSq += bits * bits; R.qSum += q;
                  // Bucket by public event index: the contraction is largest
                  // when the posterior is widest, and the decay is the shape
                  // that says whether inversion is an opening-game effect.
                  int b = std::min(5, std::max(0, inv.mirror.nEvents / 20));
                  R.bucketBits[b] += bits; R.bucketN[b]++;
                  // Predictive quality on this state, with and without evidence.
                  Belief bb;
                  Knowledge k0 = obs; k0.policyLL = nullptr;
                  bb.sinkhornDisj(k0, 4, 8, bc.theta, bc.phi);
                  long long n1 = 0, h1 = 0; double s1 = 0;
                  scoreMarginals(obs, dealt, bb.marg, n1, s1, h1);
                  Belief b2;
                  Knowledge k1 = obs; k1.policyLL = inv.ll; k1.policyClip = bc.clip;
                  b2.sinkhornDisj(k1, 4, 8, bc.thetaInv >= 0 ? bc.thetaInv : bc.theta, bc.phiInv >= 0 ? bc.phiInv : bc.phi);
                  long long n2 = 0, h2 = 0; double s2 = 0;
                  scoreMarginals(obs, dealt, b2.marg, n2, s2, h2);
                  if (n1 == n2 && n1 > 0) {
                    R.predN += n1; R.natBase += s1; R.natInv += s2;
                    R.hitBase += h1; R.hitInv += h2;
                  }
                }
              }
            }
            obs.onEvent(e);
            inv.advance(e);
          }
          R.oracleCalls += double(inv.oracle.calls);
          inv.oracle.calls = 0;
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  BitResult T;
  for (auto& R : local) {
    T.asks += R.asks; T.inverted += R.inverted; T.skipped += R.skipped;
    T.bitsSum += R.bitsSum; T.bitsSq += R.bitsSq; T.qSum += R.qSum;
    for (int b = 0; b < 6; b++) { T.bucketBits[b] += R.bucketBits[b]; T.bucketN[b] += R.bucketN[b]; }
    T.predN += R.predN; T.natBase += R.natBase; T.natInv += R.natInv;
    T.hitBase += R.hitBase; T.hitInv += R.hitInv; T.oracleCalls += R.oracleCalls;
  }
  T.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return T;
}

} // namespace v07
} // namespace fish

namespace fish {
namespace v07 {

// ---------------------------------------------------------------------------
// D1.  Per-decision scoring.
//
// Every metric below is a RATE OVER DECISIONS with a deal-clustered bootstrap
// interval, which is the point: a declaration fires 4.48 times a game and its
// errors are individually labelled, so the effective sample for a declaration
// mechanism is the number of declarations, not the number of games.  The
// clustering is by DEAL because the rotations of one deal are one correlated
// unit -- the same correction the win-rate estimator already makes.
struct DecMetric {
  const char* name;
  double num = 0, den = 0;
  std::vector<double> perDealNum, perDealDen;
  double rate() const { return den > 0 ? num / den : 0.0; }
};

struct DecSummary {
  std::vector<DecMetric> m;
  long long rows = 0, deals = 0;
  double seconds = 0, gamesPerSec = 0;
};

inline void clusterRatioCI(const std::vector<double>& num, const std::vector<double>& den,
                           double& mean, double& lo, double& hi, uint64_t seed = 4242, int B = 4000) {
  size_t n = num.size();
  double sn = 0, sd = 0;
  for (size_t i = 0; i < n; i++) { sn += num[i]; sd += den[i]; }
  mean = sd > 0 ? sn / sd : 0.0;
  if (!n) { lo = hi = 0; return; }
  std::vector<double> draws(B);
  Rng rng(seed);
  for (int b = 0; b < B; b++) {
    double a = 0, c = 0;
    for (size_t i = 0; i < n; i++) { size_t j = rng.u32(uint32_t(n)); a += num[j]; c += den[j]; }
    draws[b] = c > 0 ? a / c : 0.0;
  }
  std::sort(draws.begin(), draws.end());
  lo = draws[size_t(0.025 * B)]; hi = draws[size_t(0.975 * B)];
}

inline DecSummary summariseDecisions(const std::vector<DecisionRecord>& rows, int deals) {
  DecSummary S;
  S.rows = (long long)rows.size();
  S.deals = deals;
  const int NM = 37;
  static const char* names[NM] = {
    "askHitRate",            // asks that landed
    "ownLockedAskRate",      // asks into a half-suit this team ALREADY owns outright (ledger L3)
    "tieShare",              // ask decisions whose top group ties bit for bit (E8: 53.2%)
    "gateBindRate",          // the live-ask gate removed what would have been the argmax (L10)
    "deadAskRate",           // asks the actor could prove would miss
    "searchRate",            // ask decisions at which a test-time search ran
    "searchChangeRate",      // ... and at which it moved the choice (E14's deviation rate)
    "declAccuracy",          // voluntary declarations that were correct
    "declAllocErrorShare",   // wrong declarations where the TEAM held all six (ledger L1)
    "forcedDeclAccuracy",
    // v0.7 phase 2.  The urgency predicate is a public, frozen threshold in a
    // white-box target, and two of its four inputs are under an opponent's
    // control.  Splitting declaration accuracy by it is what separates "the
    // adversary made the target declare badly" from "the adversary is stronger".
    "declUrgentShare",       // voluntary declarations taken under urgency
    "declAccUrgent",         // ... and how many of those were right
    "declAccCalm",           // ... against the ones taken with time in hand
    // which urgency clause fired, as a share of all voluntary declarations.
    // They are not exclusive -- a declaration can fire two clauses at once.
    "urgWhyPatience",        // unresolvedCount <= patiencePool
    "urgWhyOppCards",        // oppCards <= oppCardFloor      <- the adversary's own hand count
    "urgWhyEvents",          // pub.nEvents >= forceDeclareEvents  <- the length of the game
    "urgWhyAskFloor",        // bestAskProbability < askFloor  <- a starved posterior
    // ---- v0.7 phase 3 (K2): ledger L1's replay -----------------------------
    // Every row below is a rate over VOLUNTARY DECLARATIONS with the replay
    // valid, or over the subset named.  Nothing here changed a decision.
    "l1Coverage",            // declarations at which the joint replay ran
    "l1ExactCoverage",       // ... and at which the exact shape was built
    "declJointAccuracy",     // the JOINT rule's allocation, scored on truth
    "declExactAccuracy",     // the EXACT MAP allocation, scored on truth
    "declShipAccuracy",      // the shipped rule on the SAME rows (the comparator)
    "jointFixRate",          // shipped wrong -> joint right
    "jointBreakRate",        // shipped right -> joint wrong
    "exactFixRate",          // shipped wrong -> exact right
    "exactBreakRate",        // shipped right -> exact wrong
    "jointDiffersRate",      // joint argmax != marginal-product argmax
    // The ceiling itself.  `l1FlatShare` is the share of decisions whose exact
    // posterior is uniform over every feasible allocation -- states in which NO
    // belief-based rule under the uniform-deal prior can prefer one allocation
    // to another.  `l1FlatShareErr` restricts to the wrong declarations, which
    // is the number ledger L1 asks for: if it is high the entry closes as an
    // information limit rather than as a mechanism defect.
    "l1FlatShare",
    "l1FlatShareErr",
    "l1CeilingMean",         // mean exact P(MAP | team owns), over declarations
    "l1CeilingMeanErr",      // ... over the wrong ones
    // Most declarations are not ambiguous at all -- one allocation survives and
    // there is nothing to choose.  The entry is only about the AMBIGUOUS ones,
    // so every ceiling number is repeated on the subset with >= 2 surviving
    // assignments, and again on the L1 error class itself (wrong AND the team
    // physically held all six).
    "l1AmbigShare",          // declarations with >= 2 surviving allocations
    "l1FlatShareAmbig",      // ... of those, exact posterior uniform over all
    "l1CeilingMeanAmbig",    // ... mean exact P(MAP | team owns)
    "l1AllocErrShare",       // declarations that are L1 allocation errors
    "l1AllocErrFlatShare",   // ... of those, exact posterior flat
    "l1AllocErrCeiling"      // ... mean exact P(MAP | team owns)
  };
  S.m.resize(NM);
  for (int i = 0; i < NM; i++) { S.m[i].name = names[i];
    S.m[i].perDealNum.assign(size_t(deals), 0.0); S.m[i].perDealDen.assign(size_t(deals), 0.0); }
  auto add = [&](int mi, int deal, double num, double den) {
    if (deal < 0 || deal >= deals) return;
    S.m[mi].num += num; S.m[mi].den += den;
    S.m[mi].perDealNum[size_t(deal)] += num; S.m[mi].perDealDen[size_t(deal)] += den;
  };
  for (const auto& r : rows) {
    if (r.kind == 0) {
      add(0, r.deal, r.hit ? 1 : 0, 1);
      add(1, r.deal, r.ownLocked ? 1 : 0, 1);
      add(2, r.deal, r.nTie >= 2 ? 1 : 0, r.nCand >= 2 ? 1 : 0);
      add(3, r.deal, r.gateBound ? 1 : 0, 1);
      add(4, r.deal, r.dead ? 1 : 0, 1);
      add(5, r.deal, r.searched ? 1 : 0, 1);
      add(6, r.deal, r.changed ? 1 : 0, r.searched ? 1 : 0);
    } else if (r.kind == 1) {
      add(7, r.deal, r.hit ? 1 : 0, 1);
      // A wrong declaration in a half-suit the team physically held outright is
      // an ALLOCATION error -- the team had all six and named the wrong
      // teammate.  72-75% of v0.6's remaining misdeclarations are of this kind
      // (SUBOPTIMALITY-LEDGER.md L1), and the share is only visible per
      // declaration.
      add(8, r.deal, (!r.hit && r.ownLocked) ? 1 : 0, r.hit ? 0 : 1);
      add(10, r.deal, r.urgent ? 1 : 0, 1);
      add(11, r.deal, (r.urgent && r.hit) ? 1 : 0, r.urgent ? 1 : 0);
      add(12, r.deal, (!r.urgent && r.hit) ? 1 : 0, r.urgent ? 0 : 1);
      for (int b = 0; b < 4; b++) add(13 + b, r.deal, (r.urgWhy >> b) & 1, 1);
      // ---- K2 -------------------------------------------------------------
      add(17, r.deal, (r.l1have & 1) ? 1 : 0, 1);
      add(18, r.deal, (r.l1have & 2) ? 1 : 0, 1);
      if (r.l1have & 1) {
        int sh = r.hit ? 1 : 0, jt = r.jointHit > 0 ? 1 : 0;
        add(19, r.deal, jt, 1);
        add(21, r.deal, sh, 1);
        add(22, r.deal, (!sh && jt) ? 1 : 0, sh ? 0 : 1);
        add(23, r.deal, (sh && !jt) ? 1 : 0, sh ? 1 : 0);
        add(26, r.deal, r.l1jSame == 0 ? 1 : 0, 1);
      }
      if (r.l1have & 2) {
        int sh = r.hit ? 1 : 0, ex = r.exactHit > 0 ? 1 : 0;
        add(20, r.deal, ex, 1);
        add(24, r.deal, (!sh && ex) ? 1 : 0, sh ? 0 : 1);
        add(25, r.deal, (sh && !ex) ? 1 : 0, sh ? 1 : 0);
        add(27, r.deal, r.l1flat > 0 ? 1 : 0, 1);
        add(28, r.deal, (!r.hit && r.l1flat > 0) ? 1 : 0, r.hit ? 0 : 1);
        add(29, r.deal, r.l1pMap, 1);
        add(30, r.deal, r.hit ? 0 : r.l1pMap, r.hit ? 0 : 1);
        bool amb = r.l1nAlloc >= 2;
        bool ae  = (!r.hit && r.ownLocked);
        add(31, r.deal, amb ? 1 : 0, 1);
        add(32, r.deal, (amb && r.l1flat > 0) ? 1 : 0, amb ? 1 : 0);
        add(33, r.deal, amb ? r.l1pMap : 0, amb ? 1 : 0);
        add(34, r.deal, ae ? 1 : 0, 1);
        add(35, r.deal, (ae && r.l1flat > 0) ? 1 : 0, ae ? 1 : 0);
        add(36, r.deal, ae ? r.l1pMap : 0, ae ? 1 : 0);
      }
    } else {
      add(9, r.deal, r.hit ? 1 : 0, 1);
    }
  }
  return S;
}

} // namespace v07
} // namespace fish
