// Probe 4: non-perturbing measurement of the computeAggregates-before-refresh defect,
// and whether it flips the declaration verdict.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace fish;
struct P4 : V05Agent {
  long long opps = 0, changed = 0, verdictOpps = 0, verdictFlips = 0, neverBuilt = 0;
  double sumD = 0, maxD = 0;
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (cfg.declareEnabled && (pub.rules.cardlessMayDeclare || pub.handCount[seat])) {
      int uc = __builtin_popcountll(k.unresolved);
      int press = pressure(pub);
      bool bypass = uc <= 8 || press >= 1;
      bool cand = bypass;
      for (int s = 0; s < NSET && !cand; s++) { if (!pub.setActive[s]) continue;
        if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) cand = true; }
      if (cand) {
        Belief bsave = bel; bool dsave = dirty; bool bok = blockOk;
        if (dirty) neverBuilt += 0;
        computeAggregates(pub);                       // shipped line 824 (pre-refresh)
        double before[NSET]; for (int s = 0; s < NSET; s++) before[s] = eH[s];
        ValueAggregates aggBefore = agg;
        int mcB = myCards, ocB = ourCards, tcB = theirCards, mfB = minFriendly, unB = unresolvedN;
        // fresh
        refresh();
        computeAggregates(pub);
        double dmax = 0; for (int s = 0; s < NSET; s++) dmax = std::max(dmax, std::fabs(eH[s] - before[s]));
        opps++; if (dmax > 1e-9) changed++;
        sumD += dmax; if (dmax > maxD) maxD = dmax;
        // verdict comparison on every half-suit that reaches declareByValue
        int oppCards = 0;
        for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
        bool urgent = uc <= cfg.patiencePool || oppCards <= cfg.oppCardFloor
                   || pub.nEvents >= cfg.forceDeclareEvents || bestAskProbability(pub) < cfg.askFloor;
        if (!urgent && cfg.useValue && cfg.valueDeclare) {
          for (int s = 0; s < NSET; s++) {
            if (!pub.setActive[s]) continue;
            if (!(bypass || k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb)) continue;
            SetVerdict v = evaluateSet(pub, s, press);
            if (!v.ok) continue;
            bool fresh = declareByValue(pub, v);      // with FRESH aggregates
            ValueAggregates keep = agg; double keepE[NSET];
            for (int t = 0; t < NSET; t++) keepE[t] = eH[t];
            agg = aggBefore; for (int t = 0; t < NSET; t++) eH[t] = before[t];
            myCards = mcB; ourCards = ocB; theirCards = tcB; minFriendly = mfB; unresolvedN = unB;
            bool shipped = declareByValue(pub, v);    // with the SHIPPED (stale) aggregates
            agg = keep; for (int t = 0; t < NSET; t++) eH[t] = keepE[t];
            verdictOpps++; if (shipped != fresh) verdictFlips++;
          }
        }
        bel = bsave; dirty = dsave; blockOk = bok;    // restore: play stays byte-identical
      }
    }
    return V05Agent::proposeDeclaration(pub, d, conf);
  }
};
int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 150;
  Rules r; Game g; Agent* ag[NPLAY];
  std::vector<std::unique_ptr<P4>> own;
  for (int p = 0; p < NPLAY; p++) { own.push_back(std::make_unique<P4>()); ag[p] = own[p].get(); }
  for (int i = 0; i < games; i++) g.run(mixSeed(4242, i), r, ag);
  P4& A = *own[0];
  for (int p = 1; p < NPLAY; p++) { P4& B = *own[p];
    A.opps += B.opps; A.changed += B.changed; A.sumD += B.sumD; A.maxD = std::max(A.maxD, B.maxD);
    A.verdictOpps += B.verdictOpps; A.verdictFlips += B.verdictFlips; }
  printf("games %d\n", games);
  printf("gated declaration opportunities %lld; eH differs pre- vs post-refresh at %.2f%%; "
         "mean max|dEH| %.5f  max %.5f\n", A.opps, 100.0*A.changed/A.opps, A.sumD/A.opps, A.maxD);
  printf("declareByValue verdict flips when the aggregates are refreshed first: %lld / %lld = %.2f%%\n",
         A.verdictFlips, A.verdictOpps, 100.0*A.verdictFlips/std::max(1LL,A.verdictOpps));
  return 0;
}
