// Probe 3: (a) staleness of computeAggregates in proposeDeclaration,
//          (b) collinearity of f[8] and f[16], (c) how often M1 binds on the argmax.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace fish;
struct P3 : V05Agent {
  long long opps = 0, stale = 0, firstOpp = 0; double sumAbsD = 0, maxAbsD = 0;
  long long everRefreshed = 0; long long lagSum = 0, lagMax = 0; int lastRefreshEvent = -1;
  long long neverRefreshedOpps = 0;
  long long dec = 0, m1binds = 0, ungatedDead = 0;
  double cSum = 0; long long cN = 0;   // corr(f8,f16)
  double c0Sum = 0; long long c0N = 0; // corr(f0,f18)
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (cfg.declareEnabled && (pub.rules.cardlessMayDeclare || pub.handCount[seat])) {
      int uc = __builtin_popcountll(k.unresolved);
      bool bypass = uc <= 8 || pressure(pub) >= 1;
      bool cand = bypass;
      for (int s = 0; s < NSET && !cand; s++) { if (!pub.setActive[s]) continue;
        if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) cand = true; }
      if (cand) {
        opps++;
        if (lastRefreshEvent < 0) { neverRefreshedOpps++; }
        else {
          computeAggregates(pub);            // exactly what line 824 does (pre-refresh)
          double before[NSET]; for (int s = 0; s < NSET; s++) before[s] = eH[s];
          int lag = pub.nEvents - lastRefreshEvent;
          refresh();
          computeAggregates(pub);
          double dmax = 0; for (int s = 0; s < NSET; s++) dmax = std::max(dmax, std::fabs(eH[s] - before[s]));
          if (dmax > 1e-9) stale++;
          sumAbsD += dmax; maxAbsD = std::max(maxAbsD, dmax);
          lagSum += lag; if (lag > lagMax) lagMax = lag;
          everRefreshed++;
        }
        lastRefreshEvent = pub.nEvents;
        refresh();
      }
    }
    return V05Agent::proposeDeclaration(pub, d, conf);
  }
  AskMove chooseAsk(const PublicState& pub) override {
    refresh(); lastRefreshEvent = pub.nEvents;
    AskMove all[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, all);
    if (n) {
      dec++;
      computeAggregates(pub); prepareRunway(pub);
      double bestU = -1e18; int bi = 0;
      std::vector<double> a8, a16, a0, a18;
      for (int i = 0; i < n; i++) {
        double f[NFEAT];
        features(pub, all[i].card, all[i].target, f);
        double u = 0; for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
        u *= cfg.linearWeight;
        u += cfg.valueWeight * askExpectedValue(pub, all[i].card, all[i].target, f[0]);
        if (u > bestU) { bestU = u; bi = i; }
        a8.push_back(f[8]); a16.push_back(f[16]); a0.push_back(f[0]); a18.push_back(f[18]);
      }
      if (provablyDead(all[bi].card, all[bi].target)) ungatedDead++;
      auto corr = [&](std::vector<double>& x, std::vector<double>& y) {
        int m = x.size(); if (m < 3) return 2.0;
        double mx = 0, my = 0; for (int i = 0; i < m; i++) { mx += x[i]; my += y[i]; } mx /= m; my /= m;
        double sxx = 0, syy = 0, sxy = 0;
        for (int i = 0; i < m; i++) { sxx += (x[i]-mx)*(x[i]-mx); syy += (y[i]-my)*(y[i]-my); sxy += (x[i]-mx)*(y[i]-my); }
        if (sxx < 1e-18 || syy < 1e-18) return 2.0;
        return sxy / std::sqrt(sxx * syy); };
      double c = corr(a8, a16); if (c < 1.5) { cSum += c; cN++; }
      double c0 = corr(a0, a18); if (c0 < 1.5) { c0Sum += c0; c0N++; }
    }
    return V05Agent::chooseAsk(pub);
  }
};
int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 150;
  Rules r; Game g; Agent* ag[NPLAY];
  std::vector<std::unique_ptr<P3>> own;
  for (int p = 0; p < NPLAY; p++) { own.push_back(std::make_unique<P3>()); ag[p] = own[p].get(); }
  auto& ps = own;
  for (int i = 0; i < games; i++) g.run(mixSeed(4242, i), r, ag);
  P3& A = *ps[0];
  for (int p = 1; p < NPLAY; p++) { P3& B = *ps[p];
    A.opps += B.opps; A.stale += B.stale; A.sumAbsD += B.sumAbsD; A.maxAbsD = std::max(A.maxAbsD, B.maxAbsD);
    A.everRefreshed += B.everRefreshed; A.lagSum += B.lagSum; A.lagMax = std::max(A.lagMax, B.lagMax);
    A.neverRefreshedOpps += B.neverRefreshedOpps;
    A.dec += B.dec; A.ungatedDead += B.ungatedDead; A.cSum += B.cSum; A.cN += B.cN;
    A.c0Sum += B.c0Sum; A.c0N += B.c0N; }
  printf("games %d\n", games);
  printf("computeAggregates-before-refresh: %lld gated opportunities; %lld ran with a posterior that had NEVER been built\n", A.opps, A.neverRefreshedOpps);
  printf("  of the %lld comparable ones: eH changed after refresh at %.2f%%, mean max|dEH| %.5f, max %.5f; "
         "mean staleness %.2f events, max %lld\n", A.everRefreshed, 100.0*A.stale/A.everRefreshed,
         A.sumAbsD/A.everRefreshed, A.maxAbsD, double(A.lagSum)/A.everRefreshed, A.lagMax);
  printf("M1 binds on the argmax: ungated argmax is provably dead at %.2f%% of %lld ask decisions\n",
         100.0*A.ungatedDead/A.dec, A.dec);
  printf("mean corr(f[8] replyThreat, f[16] exposureOnMiss) across candidates = %.4f  (n=%lld)\n",
         A.cSum/A.cN, A.cN);
  printf("mean corr(f[0] p, f[18] runway) across candidates = %.4f  (n=%lld)\n", A.c0Sum/A.c0N, A.c0N);
  return 0;
}
