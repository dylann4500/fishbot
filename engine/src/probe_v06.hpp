// FishBot v0.6 diagnostics.
//
// Three questions the v0.6 study turns on, each answered by a reproducible
// subcommand rather than a scratch probe:
//
//   ties    Is the "55% of ask decisions end in a bit-for-bit tie" channel a
//           DEFECT or the correct behaviour of a calibrated posterior under
//           exchangeability?  Reports the tie rate, the composition of the tie
//           set, who actually breaks it in the shipped policy, whether the
//           EXACT count law separates the tied candidates at all, and the
//           realised hit rate of every tie-break rule including hindsight.
//
//   belief  Is the posterior's residual error APPROXIMATION error or
//           POLICY-MODEL error?  Scores several posteriors as predictors on the
//           same states: predictive log loss on unresolved cards and the
//           realised hit rate of each posterior's own argmax ask.
//
//   locked  How much of the ask budget goes into half-suits the actor's own team
//           already owns outright, and what the posterior says about them.
#pragma once
#include "factory.hpp"
#include "blockdp.hpp"
#include "game.hpp"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>

namespace fish {

struct V6TieStats {
  long long decisions = 0, contested = 0, ties = 0;
  long long tieSameCard = 0, tieSameSet = 0, tieOther = 0;
  long long exactSeparates = 0, exactBuilt = 0, exactFailed = 0;
  long long hitArray = 0, hitFast = 0, hitShipped = 0, hitExact = 0, hitBest = 0, nEval = 0;
  long long fullFastHit = 0, fullExactHit = 0, fullBestHit = 0, fullN = 0, fullDisagree = 0;
  double fullAbsDiff = 0, sumTop2Gap = 0, sumSpread = 0;
  double exactBuildUs = 0;
  long long lockedAsks = 0, asksSeen = 0;
  long long shipMoved = 0;      // the shipped chain/threat pass moved the pick off the array-first tie member
  long long shipOutside = 0;    // ... and moved it OUTSIDE the tie group
  void merge(const V6TieStats& o) {
    decisions+=o.decisions; contested+=o.contested; ties+=o.ties;
    tieSameCard+=o.tieSameCard; tieSameSet+=o.tieSameSet; tieOther+=o.tieOther;
    exactSeparates+=o.exactSeparates; exactBuilt+=o.exactBuilt; exactFailed+=o.exactFailed;
    hitArray+=o.hitArray; hitFast+=o.hitFast; hitShipped+=o.hitShipped;
    hitExact+=o.hitExact; hitBest+=o.hitBest; nEval+=o.nEval;
    fullFastHit+=o.fullFastHit; fullExactHit+=o.fullExactHit; fullBestHit+=o.fullBestHit;
    fullN+=o.fullN; fullDisagree+=o.fullDisagree;
    fullAbsDiff+=o.fullAbsDiff; sumTop2Gap+=o.sumTop2Gap; sumSpread+=o.sumSpread;
    exactBuildUs+=o.exactBuildUs;
    lockedAsks+=o.lockedAsks; asksSeen+=o.asksSeen;
    shipMoved+=o.shipMoved; shipOutside+=o.shipOutside;
  }
};

// Score the candidate set exactly as V05Agent::chooseAsk does before its top-K
// chain/threat pass: linearWeight * w.f + valueWeight * askExpectedValue.
inline int v6ScoreCandidates(V05Agent& v, const PublicState& pub, AskMove* buf,
                             std::vector<double>& u, std::vector<double>& pp) {
  int n = v.cfg.liveAskGate ? v.enumerateLive(pub, buf)
                            : enumerateAsks(pub, v.k.myHand, v.seat, buf);
  if (n < 1) return n;
  if (v.cfg.useValue) v.computeAggregates(pub);
  v.prepareRunway(pub);
  u.assign(size_t(n), 0.0); pp.assign(size_t(n), 0.0);
  double f[NFEAT];
  for (int i = 0; i < n; i++) {
    v.features(pub, buf[i].card, buf[i].target, f);
    double s = 0;
    for (int j = 0; j < NFEAT; j++) s += v.cfg.w[j] * f[j];
    s *= v.cfg.linearWeight;
    if (v.cfg.useValue) s += v.cfg.valueWeight * v.askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
    u[size_t(i)] = s; pp[size_t(i)] = f[0];
  }
  return n;
}

struct V6ProbeConfig {
  std::string specA = "v05", specB = "v05";
  int games = 120, threads = 0;
  uint64_t seed = 31;
  Rules rules;
  double tieEps = 1e-9;
};

inline V6TieStats runV6Ties(const V6ProbeConfig& pc) {
  int nT = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nT < 1) nT = 1;
  nT = std::min(nT, std::max(1, pc.games));
  std::vector<V6TieStats> local;
  local.resize(size_t(nT));
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      V6TieStats& st = local[size_t(t)];
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(pc.specA); B[i] = makeAgent(pc.specB); }
      Agent* ag[NPLAY];
      for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == 0) ? A[p / 2].get() : B[p / 2].get();
      Game game;
      static thread_local double mu[NCARD][NPLAY];
      game.observer = [&](const Game& gg) {
        int who = gg.g.pub.turn;
        if (teamOf(who) != 0) return;                 // only the specA seats
        if (!gg.g.pub.handCount[who] || !gg.g.pub.activeSets()) return;
        auto* v = dynamic_cast<V05Agent*>(gg.agents[who]);
        if (!v) return;
        v->refresh();
        AskMove buf[NSET * SETSZ * 3];
        std::vector<double> u, pp;
        int n = v6ScoreCandidates(*v, gg.g.pub, buf, u, pp);
        if (n < 1) return;
        st.decisions++;
        if (n < 2) return;
        st.contested++;
        std::vector<int> ord;
        ord.resize(size_t(n));
        for (int i = 0; i < n; i++) ord[size_t(i)] = i;
        std::sort(ord.begin(), ord.end(), [&](int a, int b) { return u[size_t(a)] > u[size_t(b)]; });
        st.sumTop2Gap += u[size_t(ord[0])] - u[size_t(ord[1])];
        st.sumSpread  += u[size_t(ord[0])] - u[size_t(ord[size_t(n - 1)])];
        auto hits = [&](AskMove m) { return (gg.g.hand[m.target] & bit(m.card)) != 0; };

        // exact posterior for this information set
        BlockDP b;
        auto t0 = std::chrono::steady_clock::now();
        bool okb = b.build(v->k);
        auto t1 = std::chrono::steady_clock::now();
        if (okb) {
          st.exactBuilt++;
          st.exactBuildUs += std::chrono::duration<double>(t1 - t0).count() * 1e6;
          for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) mu[c][q] = 0.0;
          for (int c = 0; c < NCARD; c++) if (v->k.owner[c] < NPLAY) mu[c][v->k.owner[c]] = 1.0;
          b.marginals(mu);
          // full-candidate-set comparison, fast vs exact
          int aF = 0, aE = 0; double bF = -1, bE = -1, sumAbs = 0;
          for (int i = 0; i < n; i++) {
            double pf = v->bel.marg[buf[i].card][buf[i].target];
            double pe = mu[buf[i].card][buf[i].target];
            sumAbs += std::fabs(pe - pf);
            if (pf > bF + 1e-12) { bF = pf; aF = i; }
            if (pe > bE + 1e-12) { bE = pe; aE = i; }
          }
          st.fullN++; st.fullAbsDiff += sumAbs / n;
          if (aF != aE) st.fullDisagree++;
          if (hits(buf[aF])) st.fullFastHit++;
          if (hits(buf[aE])) st.fullExactHit++;
          bool any = false; for (int i = 0; i < n; i++) if (hits(buf[i])) any = true;
          if (any) st.fullBestHit++;
        } else st.exactFailed++;

        int tie = 1;
        while (tie < n && u[size_t(ord[size_t(tie)])] >= u[size_t(ord[0])] - pc.tieEps) tie++;
        if (tie < 2) return;
        st.ties++;
        bool sameCard = true, sameSet = true;
        for (int i = 1; i < tie; i++) {
          if (buf[ord[size_t(i)]].card != buf[ord[0]].card) sameCard = false;
          if (setOf(buf[ord[size_t(i)]].card) != setOf(buf[ord[0]].card)) sameSet = false;
        }
        if (sameCard) st.tieSameCard++; else if (sameSet) st.tieSameSet++; else st.tieOther++;
        if (!okb) return;

        double lo = 1e9, hi = -1e9; int argEx = ord[0];
        for (int i = 0; i < tie; i++) {
          double q = mu[buf[ord[size_t(i)]].card][buf[ord[size_t(i)]].target];
          if (q > hi + 1e-12) { hi = q; argEx = ord[size_t(i)]; }
          if (q < lo) lo = q;
        }
        if (hi - lo > 1e-9) st.exactSeparates++;
        int argF = ord[0]; double bf = -1;
        for (int i = 0; i < tie; i++) {
          double q = v->bel.marg[buf[ord[size_t(i)]].card][buf[ord[size_t(i)]].target];
          if (q > bf + 1e-12) { bf = q; argF = ord[size_t(i)]; }
        }
        AskMove pick = v->chooseAsk(gg.g.pub);
        st.nEval++;
        if (!(pick.card == buf[ord[0]].card && pick.target == buf[ord[0]].target)) {
          st.shipMoved++;
          bool inTie = false;
          for (int i = 0; i < tie; i++)
            if (buf[ord[size_t(i)]].card == pick.card && buf[ord[size_t(i)]].target == pick.target) inTie = true;
          if (!inTie) st.shipOutside++;
        }
        if (hits(buf[ord[0]])) st.hitArray++;
        if (hits(buf[argF]))   st.hitFast++;
        if (hits(pick))        st.hitShipped++;
        if (hits(buf[argEx]))  st.hitExact++;
        bool anyTie = false;
        for (int i = 0; i < tie; i++) if (hits(buf[ord[size_t(i)]])) anyTie = true;
        if (anyTie) st.hitBest++;
      };
      for (int i = t; i < pc.games; i += nT)
        game.run(mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1), pc.rules, ag);
    });
  }
  for (auto& th : pool) th.join();
  V6TieStats total;
  for (auto& l : local) total.merge(l);
  return total;
}

inline void printV6Ties(const V6TieStats& s, std::ostream& os) {
  auto pc = [](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  os << "ask decisions              " << s.decisions << "\n";
  os << "contested (>=2 live asks)  " << s.contested << "  (" << pc(s.contested, s.decisions) << "%)\n";
  os << "EXACT TIES at the top      " << s.ties << "  (" << pc(s.ties, s.contested) << "% of contested)\n";
  os << "  same card, diff target   " << s.tieSameCard << "  (" << pc(s.tieSameCard, s.ties) << "%)\n";
  os << "  same half-suit, diff card" << s.tieSameSet << "  (" << pc(s.tieSameSet, s.ties) << "%)\n";
  os << "  other                    " << s.tieOther << "  (" << pc(s.tieOther, s.ties) << "%)\n";
  os << "mean top1-top2 gap         " << (s.contested ? s.sumTop2Gap / s.contested : 0)
     << "   mean spread " << (s.contested ? s.sumSpread / s.contested : 0) << "\n";
  os << "exact DP built             " << s.exactBuilt << " (failed " << s.exactFailed
     << "), mean build " << (s.exactBuilt ? s.exactBuildUs / s.exactBuilt : 0) << " us\n";
  os << "EXACT posterior SEPARATES the tied candidates  " << s.exactSeparates
     << "  (" << pc(s.exactSeparates, s.ties) << "% of ties)\n";
  os << "the shipped chain/threat pass MOVES the pick  " << s.shipMoved
     << "  (" << pc(s.shipMoved, s.nEval) << "% of ties; "
     << s.shipOutside << " of them to a candidate outside the tie group)\n";
  os << "\nrealised hit rate of each tie-break rule (n=" << s.nEval << "):\n";
  os << "  array order (enumeration first)   " << pc(s.hitArray, s.nEval) << "%\n";
  os << "  Fast-posterior argmax in the tie   " << pc(s.hitFast, s.nEval) << "%\n";
  os << "  the SHIPPED policy's actual pick   " << pc(s.hitShipped, s.nEval) << "%\n";
  os << "  EXACT-posterior argmax in the tie  " << pc(s.hitExact, s.nEval) << "%\n";
  os << "  hindsight best in the tie          " << pc(s.hitBest, s.nEval) << "%\n";
  os << "\nfull live candidate set at the same decisions (n=" << s.fullN << "):\n";
  os << "  mean |exact - fast| marginal       " << (s.fullN ? s.fullAbsDiff / s.fullN : 0) << "\n";
  os << "  fast/exact argmax disagreement     " << pc(s.fullDisagree, s.fullN) << "%\n";
  os << "  hit rate, Fast argmax              " << pc(s.fullFastHit, s.fullN) << "%\n";
  os << "  hit rate, EXACT argmax             " << pc(s.fullExactHit, s.fullN) << "%\n";
  os << "  hit rate, hindsight best           " << pc(s.fullBestHit, s.fullN) << "%\n";
}

// ---------------------------------------------------------------- belief mode
struct V6BeliefRow { std::string name; double nll = 0; long long n = 0; long long argHit = 0, argN = 0; double sumP = 0; };

inline std::vector<V6BeliefRow> runV6Belief(const V6ProbeConfig& pc,
                                            const std::vector<std::pair<double,double>>& thetaPhi) {
  struct Cfg { std::string name; int kind; double th, ph; };
  std::vector<Cfg> cfgs;
  cfgs.push_back({"exact (uniform prior)", 0, 0, 0});
  for (auto& tp : thetaPhi) {
    char b[64]; snprintf(b, sizeof(b), "sinkhorn th=%.3f ph=%.3f", tp.first, tp.second);
    cfgs.push_back({b, 1, tp.first, tp.second});
  }
  std::vector<V6BeliefRow> rows(cfgs.size());
  for (size_t i = 0; i < cfgs.size(); i++) rows[i].name = cfgs[i].name;

  std::unique_ptr<Agent> A[3], B[3];
  for (int i = 0; i < 3; i++) { A[i] = makeAgent(pc.specA); B[i] = makeAgent(pc.specB); }
  Agent* ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == 0) ? A[p / 2].get() : B[p / 2].get();
  Game game;
  static double mu[NCARD][NPLAY];
  game.observer = [&](const Game& gg) {
    int who = gg.g.pub.turn;
    if (teamOf(who) != 0) return;
    if (!gg.g.pub.handCount[who] || !gg.g.pub.activeSets()) return;
    const Knowledge& k = gg.agents[who]->k;
    if (!k.unresolved) return;
    for (size_t ci = 0; ci < cfgs.size(); ci++) {
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) mu[c][p] = 0.0;
      for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) mu[c][k.owner[c]] = 1.0;
      if (cfgs[ci].kind == 0) { BlockDP b; if (!b.build(k)) continue; b.marginals(mu); }
      else {
        Belief bl;
        for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) bl.marg[c][p] = 0.0;
        for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) bl.marg[c][k.owner[c]] = 1.0;
        bl.sinkhornDisj(k, 4, 8, cfgs[ci].th, cfgs[ci].ph);
        for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) mu[c][p] = bl.marg[c][p];
      }
      V6BeliefRow& r = rows[ci];
      uint64_t u = k.unresolved;
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        int truth = -1; for (int p = 0; p < NPLAY; p++) if (gg.g.hand[p] & bit(c)) truth = p;
        if (truth < 0) continue;
        r.nll += -std::log(std::max(1e-9, mu[c][truth])); r.n++; }
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(gg.g.pub, k.myHand, who, buf);
      int best = -1; double bp = -1;
      for (int i = 0; i < n; i++) {
        int c = buf[i].card, t = buf[i].target;
        bool dead = k.owner[c] < NPLAY ? k.owner[c] != t : !(k.mask[c] & (1u << t));
        if (dead) continue;
        if (mu[c][t] > bp + 1e-12) { bp = mu[c][t]; best = i; }
      }
      if (best >= 0) { r.argN++; r.sumP += bp;
        if (gg.g.hand[buf[best].target] & bit(buf[best].card)) r.argHit++; }
    }
  };
  for (int i = 0; i < pc.games; i++)
    game.run(mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1), pc.rules, ag);
  return rows;
}

inline void printV6Belief(const std::vector<V6BeliefRow>& rows, std::ostream& os) {
  char line[256];
  snprintf(line, sizeof(line), "%-28s %10s %12s %12s %12s\n", "posterior", "cards", "mean NLL", "argmax p", "argmax HIT");
  os << line;
  for (const auto& r : rows) {
    snprintf(line, sizeof(line), "%-28s %10lld %12.5f %12.4f %11.2f%%\n", r.name.c_str(), r.n,
             r.n ? r.nll / double(r.n) : 0.0, r.argN ? r.sumP / double(r.argN) : 0.0,
             r.argN ? 100.0 * double(r.argHit) / double(r.argN) : 0.0);
    os << line;
  }
}

// ---------------------------------------------------------------- search mode
// How often does the guarded search actually deviate from the blueprint?  The
// falsifier the literature review supplies is explicit: a good search differs
// from its blueprint on 1-3% of decisions; one that differs on 40% is
// mis-scaled, not smart.
struct V6SearchStats {
  long long decisions = 0, searched = 0, changed = 0, dpFail = 0, rejected = 0;
  long long deadOffered = 0, deadPlayed = 0;
  long long rollouts = 0, rolloutEvents = 0;
  double seconds = 0;
};

inline V6SearchStats runV6Search(const V6ProbeConfig& pc) {
  V6SearchStats st;
  std::unique_ptr<Agent> A[3], B[3];
  for (int i = 0; i < 3; i++) { A[i] = makeAgent(pc.specA); B[i] = makeAgent(pc.specB); }
  Agent* ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == 0) ? A[p / 2].get() : B[p / 2].get();
  Game game;
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < pc.games; i++)
    game.run(mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1), pc.rules, ag);
  st.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  for (int i = 0; i < 3; i++) {
    auto* v = dynamic_cast<V06Agent*>(A[i].get());
    if (!v) continue;
    st.decisions += v->decisions; st.searched += v->searched; st.changed += v->changed;
    st.dpFail += v->dpFail; st.rejected += v->rejected;
    st.deadOffered += v->deadOffered; st.deadPlayed += v->deadPlayed;
    st.rollouts += v->roll.rollouts; st.rolloutEvents += v->roll.rolloutEvents;
  }
  return st;
}

inline void printV6Search(const V6SearchStats& s, int games, std::ostream& os) {
  auto pc = [](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  os << "ask decisions by the searching team " << s.decisions << "\n";
  os << "  decisions searched                " << s.searched << "  (" << pc(s.searched, s.decisions) << "%)\n";
  os << "  searches that MOVED the action    " << s.changed << "  (" << pc(s.changed, s.searched)
     << "% of searches, " << pc(s.changed, s.decisions) << "% of decisions)\n";
  os << "  exact DP unavailable              " << s.dpFail << "\n";
  os << "  determinizations rejected on C5   " << s.rejected << "\n";
  os << "deliberate misses offered/played    " << s.deadOffered << " / " << s.deadPlayed << "\n";
  os << "rollouts                            " << s.rollouts
     << "   events " << s.rolloutEvents
     << "   mean depth " << (s.rollouts ? double(s.rolloutEvents) / double(s.rollouts) : 0.0) << "\n";
  os << "wall clock                          " << s.seconds << " s over " << games << " games ("
     << (s.seconds > 0 ? games / s.seconds : 0) << " games/s, 1 thread)\n";
}

} // namespace fish
