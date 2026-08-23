// R1 recon probe: instrument the SHIPPED v0.5 decision procedure without
// modifying engine/src.  Subclasses V05Agent, measures, then delegates so that
// play is byte-identical to the shipped policy.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace fish;

struct Acc {
  double n = 0, s = 0, s2 = 0, mn = 1e18, mx = -1e18;
  void add(double x) { n++; s += x; s2 += x * x; mn = std::min(mn, x); mx = std::max(mx, x); }
  double mean() const { return n ? s / n : 0; }
  double sd() const { return n > 1 ? std::sqrt(std::max(0.0, s2 / n - mean() * mean())) : 0; }
};

struct Probe : V05Agent {
  // ---- ask-side counters
  long long decisions = 0, candSum = 0;
  long long allLegalSum = 0, gateRemovedDecisions = 0, starved = 0;
  long long flipValue = 0;     // argmax(L) != argmax(L+V)
  long long flipSearch = 0;    // final != argmax(L+V)
  long long flipValueFinal = 0;// final(with V) != final(without V), both with topK search
  Acc spreadL, spreadV, r2VonP, sdV, sdVres, nSets;
  Acc corrLV;
  // per ask feature: how often constant across the candidate set; spread
  long long featConst[NFEAT] = {};
  Acc featSpread[NFEAT];
  // per value feature (EV-blended): constant across candidate set?
  long long vfeatConst[NVFEAT] = {};
  Acc vfeatSpread[NVFEAT];
  // ---- declaration-side counters
  long long declOpp = 0, urgentOpp = 0, press1 = 0, press2 = 0, bypassOpp = 0;
  long long urgPool = 0, urgOpp2 = 0, urgEvents = 0, urgAskFloor = 0;
  long long dbvCalls = 0, dbvTrue = 0, urgentPathCalls = 0;
  Acc dbvThresh, dbvPAlloc;
  long long feasCalls = 0, feasEnum = 0;   // feasibleAllocation invocations / assignments scanned
  Acc feasFree;
  int maxEvents = 0;

  // Replicate value()'s feature vector for a hypothetical perturbation.
  void vfeat(const PublicState& pub, double dC, double dS, double dL, double dK,
             int scoreDiff, int turnSign, int dOur, int dTheir, int dUnres, int dActive,
             double* f) const {
    double control = agg.sumControl + dC, sharpc = agg.sharpControl + dS;
    double locked = agg.locked + dL, contested = agg.contested + dK;
    int active = agg.active + dActive;
    f[0] = 1.0;
    f[1] = scoreDiff / 9.0;
    f[2] = control / 9.0;
    f[3] = sharpc / 9.0;
    f[4] = locked / 9.0;
    f[5] = double(turnSign);
    f[6] = double(ourCards + dOur - theirCards - dTheir) / 54.0;
    f[7] = double(unresolvedN + dUnres) / 45.0;
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
    f[15] = double(turnSign) * double(unresolvedN + dUnres) / 45.0;
  }

  // EV-blended value features for one candidate ask (mirrors askExpectedValue).
  void askVFeat(const PublicState& pub, int card, double p, double* g) const {
    int S = setOf(card);
    int scoreDiff = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    double pt = pTeamCard(card);
    double eOld = eH[S];
    double eHit = eOld + (1.0 - pt) / SETSZ;
    double denom = std::max(1e-6, 1.0 - p);
    double ptMiss = std::min(1.0, pt / denom);
    double eMiss = eOld + (ptMiss - pt) / SETSZ;
    auto del = [&](double eNew, double* d) {
      d[0] = (2 * eNew - 1) - (2 * eOld - 1);
      d[1] = sharp(eNew) - sharp(eOld);
      d[2] = (eNew > .995 ? 1.0 : eNew < .005 ? -1.0 : 0.0) - (eOld > .995 ? 1.0 : eOld < .005 ? -1.0 : 0.0);
      d[3] = eNew * (1 - eNew) - eOld * (1 - eOld);
    };
    double dh[4], dm[4];
    del(eHit, dh); del(eMiss, dm);
    double fh[NVFEAT], fm[NVFEAT];
    vfeat(pub, dh[0], dh[1], dh[2], dh[3], scoreDiff, +1, +1, -1, -1, 0, fh);
    vfeat(pub, dm[0], dm[1], dm[2], dm[3], scoreDiff, -1, 0, 0, 0, 0, fm);
    for (int j = 0; j < NVFEAT; j++) g[j] = p * fh[j] + (1 - p) * fm[j];
  }

  AskMove chooseAsk(const PublicState& pub) override {
    refresh();
    AskMove all[NSET * SETSZ * 3];
    int nAll = enumerateAsks(pub, k.myHand, seat, all);
    AskMove buf[NSET * SETSZ * 3];
    int n = cfg.liveAskGate ? enumerateLive(pub, buf) : nAll;
    if (!cfg.liveAskGate) memcpy(buf, all, sizeof(AskMove) * nAll);
    if (n) {
      decisions++; candSum += n; allLegalSum += nAll;
      if (n < nAll) gateRemovedDecisions++;
      bool anyLive = false;
      for (int i = 0; i < nAll; i++) if (!provablyDead(all[i].card, all[i].target)) anyLive = true;
      if (!anyLive) starved++;

      if (cfg.useValue) computeAggregates(pub);
      prepareRunway(pub);
      std::vector<double> L(n), V(n), P(n);
      std::vector<std::array<double, NFEAT>> F(n);
      std::vector<std::array<double, NVFEAT>> G(n);
      int setsSeen = 0; bool setSeen[NSET] = {};
      for (int i = 0; i < n; i++) {
        double f[NFEAT];
        features(pub, buf[i].card, buf[i].target, f);
        double u = 0;
        for (int j = 0; j < NFEAT; j++) { u += cfg.w[j] * f[j]; F[i][j] = f[j]; }
        L[i] = u * cfg.linearWeight;
        P[i] = f[0];
        V[i] = cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
        askVFeat(pub, buf[i].card, f[0], G[i].data());
        int S = setOf(buf[i].card);
        if (!setSeen[S]) { setSeen[S] = true; setsSeen++; }
      }
      nSets.add(setsSeen);
      int aL = 0, aLV = 0;
      for (int i = 1; i < n; i++) { if (L[i] > L[aL]) aL = i; if (L[i] + V[i] > L[aLV] + V[aLV]) aLV = i; }
      if (buf[aL].card != buf[aLV].card || buf[aL].target != buf[aLV].target) flipValue++;

      double lo = *std::min_element(L.begin(), L.end()), hi = *std::max_element(L.begin(), L.end());
      double vlo = *std::min_element(V.begin(), V.end()), vhi = *std::max_element(V.begin(), V.end());
      spreadL.add(hi - lo); spreadV.add(vhi - vlo);
      // OLS of V on p
      if (n >= 3) {
        double sp = 0, sv = 0, spp = 0, spv = 0, svv = 0;
        for (int i = 0; i < n; i++) { sp += P[i]; sv += V[i]; spp += P[i] * P[i]; spv += P[i] * V[i]; svv += V[i] * V[i]; }
        double dn = n * spp - sp * sp;
        double varV = svv / n - (sv / n) * (sv / n);
        if (dn > 1e-12 && varV > 1e-18) {
          double b = (n * spv - sp * sv) / dn, a = (sv - b * sp) / n;
          double ss = 0;
          for (int i = 0; i < n; i++) { double r = V[i] - (a + b * P[i]); ss += r * r; }
          r2VonP.add(1.0 - (ss / n) / varV);
          sdV.add(std::sqrt(varV));
          sdVres.add(std::sqrt(ss / n));
        }
        // correlation of L and V
        double varL = 0, mL = 0, mV = sv / n, cov = 0;
        for (int i = 0; i < n; i++) mL += L[i]; mL /= n;
        for (int i = 0; i < n; i++) { varL += (L[i] - mL) * (L[i] - mL); cov += (L[i] - mL) * (V[i] - mV); }
        if (varL > 1e-18 && varV > 1e-18) corrLV.add(cov / n / (std::sqrt(varL / n) * std::sqrt(varV)));
      }
      for (int j = 0; j < NFEAT; j++) {
        double a = F[0][j], b = F[0][j];
        for (int i = 1; i < n; i++) { a = std::min(a, F[i][j]); b = std::max(b, F[i][j]); }
        if (b - a < 1e-12) featConst[j]++;
        featSpread[j].add(b - a);
      }
      for (int j = 0; j < NVFEAT; j++) {
        double a = G[0][j], b = G[0][j];
        for (int i = 1; i < n; i++) { a = std::min(a, G[i][j]); b = std::max(b, G[i][j]); }
        if (b - a < 1e-12) vfeatConst[j]++;
        vfeatSpread[j].add(b - a);
      }
      // top-K rescoring: replicate, and also replicate with valueWeight = 0
      auto runSearch = [&](const std::vector<double>& base) {
        struct C { double u; int i; };
        std::vector<C> cs(n);
        for (int i = 0; i < n; i++) cs[i] = C{base[i], i};
        int K = std::min(cfg.searchTopK, n);
        std::partial_sort(cs.begin(), cs.begin() + K, cs.end(), [](const C& x, const C& y) { return x.u > y.u; });
        double best2 = -1e18; int pick = cs[0].i;
        for (int r = 0; r < K; r++) {
          int i = cs[r].i, card = buf[i].card, target = buf[i].target;
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
          if (u > best2) { best2 = u; pick = i; }
        }
        return pick;
      };
      std::vector<double> LV(n);
      for (int i = 0; i < n; i++) LV[i] = L[i] + V[i];
      int finalWith = runSearch(LV);
      int finalWithout = runSearch(L);
      if (buf[finalWith].card != buf[aLV].card || buf[finalWith].target != buf[aLV].target) flipSearch++;
      if (buf[finalWith].card != buf[finalWithout].card || buf[finalWith].target != buf[finalWithout].target) flipValueFinal++;
    }
    return V05Agent::chooseAsk(pub);
  }

};

int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 200;
  uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 4242;
  Rules r;
  Agent* ag[NPLAY];
  std::vector<std::unique_ptr<Probe>> own;
  for (int p = 0; p < NPLAY; p++) { own.push_back(std::make_unique<Probe>()); ag[p] = own[p].get(); }
  auto& probes = own;
  Game g;
  for (int i = 0; i < games; i++) g.run(mixSeed(seed, i), r, ag);

  Probe& A = *probes[0];
  // pool across seats
  for (int p = 1; p < NPLAY; p++) {
    Probe& B = *probes[p];
    A.decisions += B.decisions; A.candSum += B.candSum; A.allLegalSum += B.allLegalSum;
    A.gateRemovedDecisions += B.gateRemovedDecisions; A.starved += B.starved;
    A.flipValue += B.flipValue; A.flipSearch += B.flipSearch; A.flipValueFinal += B.flipValueFinal;
    for (int j = 0; j < NFEAT; j++) { A.featConst[j] += B.featConst[j];
      A.featSpread[j].n += B.featSpread[j].n; A.featSpread[j].s += B.featSpread[j].s;
      A.featSpread[j].s2 += B.featSpread[j].s2; A.featSpread[j].mx = std::max(A.featSpread[j].mx, B.featSpread[j].mx); }
    for (int j = 0; j < NVFEAT; j++) { A.vfeatConst[j] += B.vfeatConst[j];
      A.vfeatSpread[j].n += B.vfeatSpread[j].n; A.vfeatSpread[j].s += B.vfeatSpread[j].s;
      A.vfeatSpread[j].s2 += B.vfeatSpread[j].s2; A.vfeatSpread[j].mx = std::max(A.vfeatSpread[j].mx, B.vfeatSpread[j].mx); }
    auto merge = [](Acc& a, const Acc& b) { a.n += b.n; a.s += b.s; a.s2 += b.s2; a.mn = std::min(a.mn, b.mn); a.mx = std::max(a.mx, b.mx); };
    merge(A.spreadL, B.spreadL); merge(A.spreadV, B.spreadV); merge(A.r2VonP, B.r2VonP);
    merge(A.sdV, B.sdV); merge(A.sdVres, B.sdVres); merge(A.nSets, B.nSets); merge(A.corrLV, B.corrLV);
    merge(A.dbvThresh, B.dbvThresh); merge(A.dbvPAlloc, B.dbvPAlloc);
    A.declOpp += B.declOpp; A.urgentOpp += B.urgentOpp; A.press1 += B.press1; A.press2 += B.press2;
    A.bypassOpp += B.bypassOpp; A.urgPool += B.urgPool; A.urgOpp2 += B.urgOpp2;
    A.urgEvents += B.urgEvents; A.urgAskFloor += B.urgAskFloor;
    A.dbvCalls += B.dbvCalls; A.dbvTrue += B.dbvTrue; A.urgentPathCalls += B.urgentPathCalls;
    A.maxEvents = std::max(A.maxEvents, B.maxEvents);
  }
  printf("games %d  seed %llu\n", games, (unsigned long long)seed);
  printf("ASK decisions %lld  mean live cands %.2f  mean legal cands %.2f  gate removed on %.2f%% of turns  starved %lld (%.3f%%)\n",
         A.decisions, double(A.candSum) / A.decisions, double(A.allLegalSum) / A.decisions,
         100.0 * A.gateRemovedDecisions / A.decisions, A.starved, 100.0 * A.starved / A.decisions);
  printf("mean distinct half-suits in candidate set %.2f\n", A.nSets.mean());
  printf("value term flips pre-search argmax: %.3f%%\n", 100.0 * A.flipValue / A.decisions);
  printf("topK search flips argmax(L+V):      %.3f%%\n", 100.0 * A.flipSearch / A.decisions);
  printf("FINAL move differs with vs without value term: %.3f%%\n", 100.0 * A.flipValueFinal / A.decisions);
  printf("mean spread(L)=%.4f  mean spread(V)=%.4f  ratio %.4f\n",
         A.spreadL.mean(), A.spreadV.mean(), A.spreadV.mean() / A.spreadL.mean());
  printf("mean R^2 of V on p across candidates = %.5f   sd(V)=%.5f  sd(resid)=%.5f  resid/sd=%.4f\n",
         A.r2VonP.mean(), A.sdV.mean(), A.sdVres.mean(), A.sdVres.mean() / std::max(1e-12, A.sdV.mean()));
  printf("mean corr(L,V) across candidates = %.4f\n", A.corrLV.mean());
  printf("\nASK FEATURE VARIATION (share of decisions where the feature is constant across the candidate set)\n");
  const char* fn[NFEAT] = {"hitProb","hitProb^2","certainHit","ownSetProgress","teamControl","lockCompletion",
    "continuation","completionBonus","replyThreat","infoLeak","targetHandSize","emptiesTarget","repeatsSet",
    "knownTeamCards","locationEntropy","teamOwnsSet","exposureOnMiss","trailingPressure","runway","leakMagnitude"};
  for (int j = 0; j < NFEAT; j++)
    printf("  f[%2d] %-16s const %6.2f%%  meanSpread=%.5f  maxSpread=%.4f  w=%+9.5f  contribSpread=%.5f\n",
           j, fn[j], 100.0 * A.featConst[j] / A.decisions, A.featSpread[j].mean(), A.featSpread[j].mx,
           V05Config{}.w[j], std::fabs(V05Config{}.w[j]) * A.featSpread[j].mean() * V05Config{}.linearWeight);
  printf("\nVALUE FEATURE VARIATION across candidate asks (EV-blended)\n");
  const char* vn[NVFEAT] = {"bias","scoreDiff","expControl","sharpControl","lockedDiff","sideToMove",
    "cardDiff","unresolvedPool","activeSets","turnXcontrol","myHandSize","smallestFriendly",
    "ourNearComplete","theirNearComplete","contestedMass","turnXunresolved"};
  for (int j = 0; j < NVFEAT; j++)
    printf("  v[%2d] %-18s const %6.2f%%  meanSpread=%.6f  vw=%+9.6f  contribSpread(xvalueWeight)=%.6f\n",
           j, vn[j], 100.0 * A.vfeatConst[j] / A.decisions, A.vfeatSpread[j].mean(),
           V05Config{}.vw[j], std::fabs(V05Config{}.vw[j]) * A.vfeatSpread[j].mean() * V05Config{}.valueWeight);
  return 0;
}
