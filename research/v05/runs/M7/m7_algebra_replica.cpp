// M7 algebra check, part 2.  Replicate sinkhornDisj with a pluggable weight so
// the +-2.6 clip can be switched off, and separate the algebraic claim from the
// clip artefact.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
#include <functional>

using namespace fish;

// verbatim structural copy of Belief::sinkhornDisj (belief.hpp:478-529) with the
// initialiser generalised from k.priorWeight(...) to w(card, player).
static void fitW(double marg[NCARD][NPLAY], const Knowledge& kk, int outer, int inner,
                 const std::function<double(int,int)>& w) {
  uint8_t q[NPLAY]; kk.capacities(q);
  int idx[NCARD], Q = 0;
  uint64_t u = kk.unresolved;
  while (u) { int c = __builtin_ctzll(u); u &= u - 1; idx[Q++] = c; }
  if (!Q) return;
  for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++)
    marg[idx[i]][p] = (kk.mask[idx[i]] & (1u << p)) ? w(idx[i], p) : 0.0;
  for (int o = 0; o < outer; o++) {
    for (int it = 0; it < inner; it++) {
      for (int i = 0; i < Q; i++) { double t = 0; int c = idx[i];
        for (int p = 0; p < NPLAY; p++) t += marg[c][p];
        if (t > 0) for (int p = 0; p < NPLAY; p++) marg[c][p] /= t; }
      double col[NPLAY] = {0,0,0,0,0,0};
      for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++) col[p] += marg[idx[i]][p];
      for (int p = 0; p < NPLAY; p++) { double sc = col[p] > 0 ? q[p] / col[p] : 0;
        for (int i = 0; i < Q; i++) marg[idx[i]][p] *= sc; }
    }
    for (int i = 0; i < Q; i++) { double t = 0; int c = idx[i];
      for (int p = 0; p < NPLAY; p++) t += marg[c][p];
      if (t > 0) for (int p = 0; p < NPLAY; p++) marg[c][p] /= t; }
    if (o + 1 == outer) break;
    for (const auto& d : kk.disj) {
      int A = d.player;
      double pNone = 1.0;
      uint64_t cc = d.cards;
      int list[SETSZ]; int m = 0;
      while (cc) { int x = __builtin_ctzll(cc); cc &= cc - 1;
        if (!(kk.unresolved & bit(x))) continue;
        list[m++] = x; pNone *= (1.0 - marg[x][A]); }
      if (!m) continue;
      double pAny = 1.0 - pNone;
      if (pAny < 1e-9) {
        for (int j = 0; j < m; j++) { for (int p = 0; p < NPLAY; p++) marg[list[j]][p] *= .001;
          marg[list[j]][A] += 1.0 / m; }
        continue;
      }
      if (pNone < 1e-12) continue;
      for (int j = 0; j < m; j++) {
        int c = list[j];
        double denom = 1.0 - marg[c][A];
        double pNoneOther = denom > 1e-12 ? pNone / denom : 0.0;
        double newA = marg[c][A] / pAny;
        double scale = (1.0 - pNoneOther) / pAny;
        for (int p = 0; p < NPLAY; p++) if (p != A) marg[c][p] *= scale;
        marg[c][A] = newA;
        double t = 0; for (int p = 0; p < NPLAY; p++) t += marg[c][p];
        if (t > 0) for (int p = 0; p < NPLAY; p++) marg[c][p] /= t;
      }
    }
  }
}

struct Acc { double mx = 0, sum = 0; long long n = 0;
  void add(double d) { if (d > mx) mx = d; sum += d; n++; } };

int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 20;
  uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 4242;
  int outer = argc > 3 ? atoi(argv[3]) : 4;
  int inner = argc > 4 ? atoi(argv[4]) : 8;
  double th = 0.26380, ph = 0.13280;

  Acc replica, clipOnEq, clipOffEq, clipOffPhiOnly, clipOffThetaMove;
  long long nStates = 0;

  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
    for (int i = 0; i < NPLAY; i++) { ag[i] = makeAgent("v04"); ap[i] = ag[i].get(); }
    Game gm; Rules r;
    gm.observer = [&](const Game& gg) {
      if (gg.g.pub.nEvents % 5) return;
      for (int p = 0; p < NPLAY; p += 2) {
        const Knowledge& kk = gg.agents[p]->k;
        if (!kk.unresolved) continue;
        nStates++;
        auto clipped = [&](double t, double f) {
          return [&kk, t, f](int c, int q) {
            if (t == 0 && f == 0) return 1.0;
            int S = setOf(c);
            double a = kk.askCount[q][S], o = double(kk.totalAsks[q]) - a;
            double z = t * a - f * o;
            if (z > 2.6) z = 2.6; else if (z < -2.6) z = -2.6;
            return std::exp(z); }; };
        auto raw = [&](double t, double f) {
          return [&kk, t, f](int c, int q) {
            int S = setOf(c);
            double a = kk.askCount[q][S], o = double(kk.totalAsks[q]) - a;
            return std::exp(t * a - f * o); }; };

        Belief ref;
        for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) ref.marg[c][q] = 0;
        for (int c = 0; c < NCARD; c++) if (kk.owner[c] < NPLAY) ref.marg[c][kk.owner[c]] = 1;
        ref.sinkhornDisj(kk, outer, inner, th, ph);

        double M[6][NCARD][NPLAY];
        std::function<double(int,int)> ws[6] = {
          clipped(th, ph), clipped(th + ph, 0.0),
          raw(th, ph),     raw(th + ph, 0.0),
          raw(0.0, ph),    raw(0.0, 0.0) };
        for (int j = 0; j < 6; j++) {
          for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) M[j][c][q] = 0;
          for (int c = 0; c < NCARD; c++) if (kk.owner[c] < NPLAY) M[j][c][kk.owner[c]] = 1;
          fitW(M[j], kk, outer, inner, ws[j]);
        }
        // extra: a (seat,half-suit) statistic that is NOT a function of the seat
        // alone -- the shape M7 uses.  Must NOT be absorbed.
        auto m7like = [&kk](int c, int q) {
          int S = setOf(c);
          double sil = double(kk.missCount[q][S]);      // any (seat,set) statistic
          return std::exp(-0.35 * sil); };
        double M7[NCARD][NPLAY];
        for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) M7[c][q] = 0;
        for (int c = 0; c < NCARD; c++) if (kk.owner[c] < NPLAY) M7[c][kk.owner[c]] = 1;
        fitW(M7, kk, outer, inner, m7like);

        uint64_t u = kk.unresolved;
        while (u) { int c = __builtin_ctzll(u); u &= u - 1;
          for (int q = 0; q < NPLAY; q++) {
            replica.add(std::fabs(ref.marg[c][q] - M[0][c][q]));
            clipOnEq.add(std::fabs(M[0][c][q] - M[1][c][q]));
            clipOffEq.add(std::fabs(M[2][c][q] - M[3][c][q]));
            clipOffPhiOnly.add(std::fabs(M[4][c][q] - M[5][c][q]));
            clipOffThetaMove.add(std::fabs(M[5][c][q] - M7[c][q]));
          }
        }
      }
    };
    gm.run(mixSeed(seed, gi), r, ap);
  }
  auto rep = [](const char* nm, const Acc& a) {
    printf("%-52s max %.3e  mean %.3e\n", nm, a.mx, a.n ? a.sum / a.n : 0.0); };
  printf("outer=%d inner=%d   states %lld  cells %lld\n", outer, inner, nStates, replica.n);
  rep("replica vs belief.hpp sinkhornDisj (th,ph)", replica);
  rep("CLIP ON : (th,ph) vs (th+ph,0)", clipOnEq);
  rep("CLIP OFF: (th,ph) vs (th+ph,0)", clipOffEq);
  rep("CLIP OFF: (0,ph)  vs (0,0)   [pure silence]", clipOffPhiOnly);
  rep("(0,0) vs (seat,half-suit) statistic tilt", clipOffThetaMove);
  return 0;
}
