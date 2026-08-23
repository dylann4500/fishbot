// Probe 2: the stopping rule and M2's cost.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
using namespace fish;
struct P2 : V05Agent {
  long long dbv = 0, agree = 0, tru = 0;
  double sumThr = 0, minThr = 9, maxThr = -9;
  long long feas = 0, feasTotal = 0, feasFail = 0; long long freeHist[7] = {};
  long long nPass = 0, nWilling = 0, nBestGuess = 0, nEvalSet = 0;
  long long declEmit = 0, declRight = 0;
  long long lockedNow = 0, lockedWait = 0;

  void countFeas(int set) {
    int nm = 3, nFree = 0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(set, i);
      if (k.myHand & bit(c)) continue;
      if (k.owner[c] < NPLAY) continue;
      if (k.owner[c] == OUT_OF_PLAY) { nFree = -1; break; }
      nFree++;
    }
    feas++;
    if (nFree >= 0) { int t = 1; for (int i = 0; i < nFree; i++) t *= nm; feasTotal += t;
                      freeHist[std::min(6, nFree)]++; }
  }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (cfg.declareEnabled && (pub.rules.cardlessMayDeclare || pub.handCount[seat])) {
      int unresolvedCount = __builtin_popcountll(k.unresolved);
      int press = pressure(pub);
      bool bypass = unresolvedCount <= 8 || press >= 1;
      refresh();
      if (cfg.useValue) computeAggregates(pub);
      int oppCards = 0;
      for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
      bool urgent = unresolvedCount <= cfg.patiencePool || oppCards <= cfg.oppCardFloor
                 || pub.nEvents >= cfg.forceDeclareEvents || bestAskProbability(pub) < cfg.askFloor;
      for (int s = 0; s < NSET && !urgent; s++) {
        if (!pub.setActive[s]) continue;
        if (!(bypass || k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb)) continue;
        countFeas(s);
        SetVerdict v = evaluateSet(pub, s, press);
        nEvalSet++;
        if (!v.ok) continue;
        if (v.pTeam > .9995) { lockedNow++; }
        dbv++;
        bool got = declareByValue(pub, v);
        if (got) tru++;
        if (v.pTeam > .9995 && !got) lockedWait++;
        int S = v.decl.set;
        int turnSign = (teamOf(pub.turn) == teamOf(seat)) ? 1 : -1;
        double eOld = eH[S];
        double dC = -(2 * eOld - 1), dS = -sharp(eOld);
        double dL = -(eOld > .995 ? 1.0 : eOld < .005 ? -1.0 : 0.0);
        double dK = -eOld * (1 - eOld);
        int dU = -__builtin_popcountll(k.unresolved & setMask(S));
        const double* w = cfg.vw;
        double konst = w[2]*dC/9 + w[3]*dS/9 + w[4]*dL/9 - w[6]*6.0/54.0 + w[7]*dU/45.0
                     - w[8]/9.0 + w[9]*turnSign*dC/9 + w[14]*dK/9 + w[15]*turnSign*dU/45.0;
        double thr = 0.5 + (cfg.declareMargin - konst) * 9.0 / (2.0 * w[1]);
        sumThr += thr; minThr = std::min(minThr, thr); maxThr = std::max(maxThr, thr);
        if (got == (v.pAlloc > thr)) agree++;
      }
    }
    return V05Agent::proposeDeclaration(pub, d, conf);
  }
  int choosePassTarget(const PublicState& pub, const int* c, int n) override {
    nPass++; return V05Agent::choosePassTarget(pub, c, n); }
  bool willingForced(const PublicState& pub, int s, Declaration& d, double& c, double th) override {
    nWilling++; return V05Agent::willingForced(pub, s, d, c, th); }
  void bestGuess(const PublicState& pub, int s, Declaration& d, double& c) override {
    nBestGuess++; V05Agent::bestGuess(pub, s, d, c); }
};
int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 300;
  Rules r; Game g; P2 ps[NPLAY]; Agent* ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = &ps[p];
  long long passEv = 0;
  for (int i = 0; i < games; i++) g.run(mixSeed(4242, i), r, ag);
  P2& A = ps[0];
  for (int p = 1; p < NPLAY; p++) { P2& B = ps[p];
    A.dbv += B.dbv; A.agree += B.agree; A.tru += B.tru; A.sumThr += B.sumThr;
    A.minThr = std::min(A.minThr, B.minThr); A.maxThr = std::max(A.maxThr, B.maxThr);
    A.feas += B.feas; A.feasTotal += B.feasTotal; A.feasFail += B.feasFail;
    for (int i = 0; i < 7; i++) A.freeHist[i] += B.freeHist[i];
    A.nPass += B.nPass; A.nWilling += B.nWilling; A.nBestGuess += B.nBestGuess; A.nEvalSet += B.nEvalSet;
    A.lockedNow += B.lockedNow; A.lockedWait += B.lockedWait; }
  printf("games %d\n", games);
  printf("declareByValue: n=%lld  true=%.2f%%  agreement with analytic pAlloc-threshold = %.4f%%\n",
         A.dbv, 100.0*A.tru/A.dbv, 100.0*A.agree/A.dbv);
  printf("  implied threshold mean %.4f  min %.4f  max %.4f   (declThreshold knob = %.5f)\n",
         A.sumThr/A.dbv, A.minThr, A.maxThr, V05Config{}.declThreshold);
  printf("  locked (pTeam>.9995) verdicts %lld of which WAIT %lld (%.2f%%)\n",
         A.lockedNow, A.lockedWait, 100.0*A.lockedWait/std::max(1LL,A.lockedNow));
  printf("feasibleAllocation: calls %lld  assignments enumerated %lld (mean %.2f)  infeasible %.2f%%\n",
         A.feas, A.feasTotal, double(A.feasTotal)/A.feas, 100.0*A.feasFail/A.feas);
  printf("  nFree histogram (0..6+): ");
  for (int i = 0; i < 7; i++) printf("%lld ", A.freeHist[i]);
  printf("\nchoosePassTarget calls %lld (%.3f/game)   willingForced %lld   bestGuess %lld\n",
         A.nPass, double(A.nPass)/games, A.nWilling, A.nBestGuess);
  (void)passEv;
  return 0;
}
