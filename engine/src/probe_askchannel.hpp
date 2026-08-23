// Adversarial verification probe for the P5 "target dimension is a free channel"
// claim.  Unlike probe_human.hpp (which replays a trace and reconstructs a
// capacity-normalised marginal), this probe instruments the LIVE v0.4 agent and
// reads v0.4's own posterior (bel.marg) and v0.4's own ask score, so it measures
// what using the target dimension as a code would actually cost the policy.
//
// At every ask decision by an instrumented v0.4 seat we:
//   - reproduce chooseAsk's stage-1 score for all n legal asks,
//     u = linearWeight * w.f  +  valueWeight * askExpectedValue
//   - take the card c* the agent finally asks for
//   - form the hard-indistinguishable target class for c* (probe_human.hpp's
//     definition: live opponents with the same provably-dead status as the
//     target actually chosen)
//   - report, inside that class, the spread of v0.4's TRUE hit probability,
//     of the linear part, of the value part, and of the total score, split by
//     whether the chosen ask was provably dead.
#pragma once
#include "factory.hpp"
#include "v04.hpp"
#include "game.hpp"
#include <thread>
#include <cmath>
#include <algorithm>

namespace fish {

struct ChanAcc {
  long long n = 0;                 // decisions
  long long cls2 = 0;              // decisions with class >= 2
  double bits = 0;                 // sum log2(class size)
  double sumClass = 0;
  // sums over class>=2 decisions
  double sPTrue = 0;               // spread of v0.4's own bel.marg inside class
  double sPCap = 0;                // spread of the capacity-normalised marginal (probe_human's stat)
  double sLin = 0;                 // spread of linearWeight * w.f
  double sVal = 0;                 // spread of valueWeight * askExpectedValue
  double sTot = 0;                 // spread of the total stage-1 score
  double sTotFull = 0;             // spread of the total score over ALL n candidates
  double ratio = 0;                // sTot / sTotFull, per decision
  long *dummy = nullptr;
  // how often the intra-class score range exceeds the top1-top2 gap over all asks
  long long flips = 0;
  // decomposition of who varies with the target
  double sValOverFull = 0;         // range of value part over all candidates
  double sLinOverFull = 0;
  void add(const ChanAcc& o) {
    n += o.n; cls2 += o.cls2; bits += o.bits; sumClass += o.sumClass;
    sPTrue += o.sPTrue; sPCap += o.sPCap; sLin += o.sLin; sVal += o.sVal;
    sTot += o.sTot; sTotFull += o.sTotFull; ratio += o.ratio; flips += o.flips;
    sValOverFull += o.sValOverFull; sLinOverFull += o.sLinOverFull;
  }
};

struct ChanStats {
  ChanAcc all, dead, live;         // split by whether the chosen ask was provably dead
  // "how much of the ask score actually moves with the target seat"
  long long dvN = 0;
  double dvSameCardValRange = 0;   // range of value part across targets of the same card
  double dvSameCardLinRange = 0;
  double dvSameCardTotRange = 0;
  double dvSameCardPRange = 0;
  void merge(const ChanStats& o) {
    all.add(o.all); dead.add(o.dead); live.add(o.live);
    dvN += o.dvN;
    dvSameCardValRange += o.dvSameCardValRange;
    dvSameCardLinRange += o.dvSameCardLinRange;
    dvSameCardTotRange += o.dvSameCardTotRange;
    dvSameCardPRange += o.dvSameCardPRange;
  }
};

inline bool pDead(const Knowledge& kk, int c, int t) {
  return (kk.owner[c] < NPLAY) ? (kk.owner[c] != t) : !(kk.mask[c] & (1u << t));
}

struct ChanAgent : V04Agent {
  ChanStats* st = nullptr;

  AskMove chooseAsk(const PublicState& pub) override {
    int savedLastMySet = lastMySet;
    // ---- stage-1 score landscape, exactly as v04.hpp:474-483 ----
    refresh();
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return V04Agent::chooseAsk(pub);
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    std::vector<double> uu(n), ll(n), vv(n), pp(n);
    double f[NFEAT];
    for (int i = 0; i < n; i++) {
      features(pub, buf[i].card, buf[i].target, f);
      double lin = 0;
      for (int j = 0; j < NFEAT; j++) lin += cfg.w[j] * f[j];
      lin *= cfg.linearWeight;
      double val = cfg.useValue ? cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]) : 0.0;
      ll[i] = lin; vv[i] = val; uu[i] = lin + val; pp[i] = f[0];
    }
    // full-candidate range and top1/top2 gap
    double uMax = -1e18, uMax2 = -1e18, uMin = 1e18;
    double vMax = -1e18, vMin = 1e18, lMax = -1e18, lMin = 1e18;
    for (int i = 0; i < n; i++) {
      if (uu[i] > uMax) { uMax2 = uMax; uMax = uu[i]; } else if (uu[i] > uMax2) uMax2 = uu[i];
      uMin = std::min(uMin, uu[i]);
      vMax = std::max(vMax, vv[i]); vMin = std::min(vMin, vv[i]);
      lMax = std::max(lMax, ll[i]); lMin = std::min(lMin, ll[i]);
    }
    double gap12 = (n >= 2) ? (uMax - uMax2) : 0.0;

    // ---- the real decision ----
    lastMySet = savedLastMySet;
    AskMove pick = V04Agent::chooseAsk(pub);

    // ---- class for the card actually asked ----
    bool chosenDead = pDead(k, pick.card, pick.target);
    uint8_t cap[NPLAY]; k.capacities(cap);
    double den = 0;
    for (int p = 0; p < NPLAY; p++) if (k.mask[pick.card] & (1u << p)) den += cap[p];
    int cls = 0;
    double pTLo = 1e9, pTHi = -1e9, pCLo = 1e9, pCHi = -1e9;
    double lLo = 1e18, lHi = -1e18, vLo = 1e18, vHi = -1e18, tLo = 1e18, tHi = -1e18;
    for (int t = 0; t < NPLAY; t++) {
      if (teamOf(t) == teamOf(seat)) continue;
      if (!pub.handCount[t]) continue;
      if (pDead(k, pick.card, t) != chosenDead) continue;
      cls++;
      double pt = bel.marg[pick.card][t];
      pTLo = std::min(pTLo, pt); pTHi = std::max(pTHi, pt);
      double pc = (k.owner[pick.card] < NPLAY) ? (k.owner[pick.card] == t ? 1.0 : 0.0)
                : (den > 0 && (k.mask[pick.card] & (1u << t)) ? cap[t] / den : 0.0);
      pCLo = std::min(pCLo, pc); pCHi = std::max(pCHi, pc);
      for (int i = 0; i < n; i++) if (buf[i].card == pick.card && buf[i].target == t) {
        lLo = std::min(lLo, ll[i]); lHi = std::max(lHi, ll[i]);
        vLo = std::min(vLo, vv[i]); vHi = std::max(vHi, vv[i]);
        tLo = std::min(tLo, uu[i]); tHi = std::max(tHi, uu[i]);
      }
    }
    if (cls < 1) cls = 1;

    ChanAcc rec;
    rec.n = 1; rec.sumClass = cls; rec.bits = std::log2(double(cls));
    rec.sTotFull = uMax - uMin;
    rec.sValOverFull = vMax - vMin;
    rec.sLinOverFull = lMax - lMin;
    if (cls >= 2 && tHi > -1e17) {
      rec.cls2 = 1;
      rec.sPTrue = pTHi - pTLo; rec.sPCap = pCHi - pCLo;
      rec.sLin = lHi - lLo; rec.sVal = vHi - vLo; rec.sTot = tHi - tLo;
      rec.ratio = (uMax - uMin) > 1e-12 ? (tHi - tLo) / (uMax - uMin) : 0.0;
      if ((tHi - tLo) > gap12) rec.flips = 1;
    }
    st->all.add(rec);
    if (chosenDead) st->dead.add(rec); else st->live.add(rec);

    // ---- how much of the score moves with the target seat, all cards ----
    // For every distinct card with >=2 legal targets, the range of each part.
    {
      int seen[NCARD]; for (int c = 0; c < NCARD; c++) seen[c] = 0;
      for (int i = 0; i < n; i++) {
        int c = buf[i].card;
        if (seen[c]) continue;
        seen[c] = 1;
        double a = 1e18, b = -1e18, c1 = 1e18, d1 = -1e18, e1 = 1e18, g1 = -1e18, h1 = 1e18, i1 = -1e18;
        int m = 0;
        for (int j = 0; j < n; j++) if (buf[j].card == c) {
          m++;
          a = std::min(a, vv[j]); b = std::max(b, vv[j]);
          c1 = std::min(c1, ll[j]); d1 = std::max(d1, ll[j]);
          e1 = std::min(e1, uu[j]); g1 = std::max(g1, uu[j]);
          h1 = std::min(h1, pp[j]); i1 = std::max(i1, pp[j]);
        }
        if (m >= 2) {
          st->dvN++;
          st->dvSameCardValRange += b - a;
          st->dvSameCardLinRange += d1 - c1;
          st->dvSameCardTotRange += g1 - e1;
          st->dvSameCardPRange += i1 - h1;
        }
      }
    }
    return pick;
  }
};

struct ChanConfig {
  std::string specB = "v04";
  int games = 200, rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 0;
};

inline ChanStats runAskChannel(const ChanConfig& pc) {
  int nThreads = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, pc.games));
  std::vector<ChanStats> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<ChanAgent> A[3];
      std::unique_ptr<Agent> B[3];
      for (int i = 0; i < 3; i++) {
        A[i] = std::make_unique<ChanAgent>();
        A[i]->st = &local[t];
        B[i] = makeAgent(pc.specB);
      }
      Game game;
      for (int i = t; i < pc.games; i += nThreads) {
        uint64_t s = mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < pc.rotations; rot++) {
          int orient = (pc.rotations == 2) ? rot : (rot / 3);
          int shift  = (pc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? (Agent*)A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          game.run(s, pc.rules, ag);
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  ChanStats st;
  for (auto& l : local) st.merge(l);
  return st;
}

inline void printAskChannel(const ChanStats& s, std::ostream& o) {
  auto row = [&](const char* nm, const ChanAcc& a) {
    o << nm << "\n";
    o << "  decisions                        " << a.n << "\n";
    if (!a.n) return;
    o << "  mean class size                  " << a.sumClass / a.n << "\n";
    o << "  mean free bits (log2 class)      " << a.bits / a.n << "\n";
    o << "  decisions with class >= 2        " << a.cls2 << " (" << 100.0 * a.cls2 / a.n << "%)\n";
    if (!a.cls2) return;
    o << "  --- inside the class, per class>=2 decision ---\n";
    o << "  spread of capacity marginal      " << a.sPCap / a.cls2 << "\n";
    o << "  spread of v0.4's OWN bel.marg    " << a.sPTrue / a.cls2 << "\n";
    o << "  spread of linear part            " << a.sLin / a.cls2 << "\n";
    o << "  spread of VALUE part             " << a.sVal / a.cls2 << "\n";
    o << "  spread of TOTAL ask score        " << a.sTot / a.cls2 << "\n";
    o << "  full-candidate total score range " << a.sTotFull / a.n << "\n";
    o << "  intra-class range / full range   " << a.ratio / a.cls2 << "\n";
    o << "  intra-class range > top1-top2 gap " << a.flips << " (" << 100.0 * a.flips / a.cls2 << "%)\n";
    o << "  --- over ALL candidates, per decision ---\n";
    o << "  range of VALUE part              " << a.sValOverFull / a.n << "\n";
    o << "  range of LINEAR part             " << a.sLinOverFull / a.n << "\n";
  };
  row("ALL ask decisions", s.all);
  row("chosen ask PROVABLY DEAD", s.dead);
  row("chosen ask still live", s.live);
  o << "\n-- target-sensitivity of each score term (all cards with >=2 legal targets) --\n";
  o << "  (card, decision) pairs           " << s.dvN << "\n";
  if (s.dvN) {
    o << "  mean range of hit prob p         " << s.dvSameCardPRange / s.dvN << "\n";
    o << "  mean range of VALUE part         " << s.dvSameCardValRange / s.dvN << "\n";
    o << "  mean range of LINEAR part        " << s.dvSameCardLinRange / s.dvN << "\n";
    o << "  mean range of TOTAL              " << s.dvSameCardTotRange / s.dvN << "\n";
  }
}

} // namespace fish
