// Why the column-scaling equivalence is not exact in the shipped code.
// Hypothesis: Sinkhorn's limit absorbs a column scaling only when the kernel has
// TOTAL SUPPORT (every mask-legal cell lies on some capacity-feasible allocation).
// Split the residual by whether the state's kernel has total support, using the
// exact DP (belief.hpp DealDP) to identify mask-legal but infeasible cells.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
#include <functional>
using namespace fish;

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
  }
}

int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 20;
  uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 4242;
  int inner = argc > 3 ? atoi(argv[3]) : 200;
  double ph = 0.13280;
  long long nTS = 0, nNoTS = 0, deadCells = 0, liveCells = 0;
  double mxTS = 0, sumTS = 0; long long cTS = 0;
  double mxNo = 0, sumNo = 0; long long cNo = 0;

  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
    for (int i = 0; i < NPLAY; i++) { ag[i] = makeAgent("v04"); ap[i] = ag[i].get(); }
    Game gm; Rules r;
    gm.observer = [&](const Game& gg) {
      if (gg.g.pub.nEvents % 5) return;
      for (int p = 0; p < NPLAY; p += 2) {
        const Knowledge& kk = gg.agents[p]->k;
        if (!kk.unresolved) return;
        DealDP dp; if (!dp.build(kk)) return;
        static double ex[NCARD][NPLAY];
        for (int c = 0; c < NCARD; c++) for (int q2 = 0; q2 < NPLAY; q2++) ex[c][q2] = 0;
        dp.marginals(ex);
        bool total = true;
        uint64_t u0 = kk.unresolved;
        while (u0) { int c = __builtin_ctzll(u0); u0 &= u0 - 1;
          for (int q2 = 0; q2 < NPLAY; q2++) if (kk.mask[c] & (1u << q2)) {
            if (ex[c][q2] <= 1e-12) { total = false; deadCells++; } else liveCells++; } }
        if (total) nTS++; else nNoTS++;
        double A[NCARD][NPLAY], B[NCARD][NPLAY];
        for (int c = 0; c < NCARD; c++) for (int q2 = 0; q2 < NPLAY; q2++) A[c][q2] = B[c][q2] = 0;
        for (int c = 0; c < NCARD; c++) if (kk.owner[c] < NPLAY) { A[c][kk.owner[c]] = 1; B[c][kk.owner[c]] = 1; }
        const double th = 0.26380;
        fitW(A, kk, 1, inner, [&kk, th, ph](int c, int q2) {
          int S = setOf(c); double a = kk.askCount[q2][S];
          return std::exp(th * a - ph * (double(kk.totalAsks[q2]) - a)); });
        fitW(B, kk, 1, inner, [&kk, th, ph](int c, int q2) {
          int S = setOf(c); double a = kk.askCount[q2][S];
          return std::exp((th + ph) * a); });
        uint64_t u = kk.unresolved;
        while (u) { int c = __builtin_ctzll(u); u &= u - 1;
          for (int q2 = 0; q2 < NPLAY; q2++) {
            double d = std::fabs(A[c][q2] - B[c][q2]);
            if (total) { if (d > mxTS) mxTS = d; sumTS += d; cTS++; }
            else       { if (d > mxNo) mxNo = d; sumNo += d; cNo++; } } }
      }
    };
    gm.run(mixSeed(seed, gi), r, ap);
  }
  printf("states with TOTAL SUPPORT   : %lld   residual max %.3e  mean %.3e  (cells %lld)\n",
         nTS, mxTS, cTS ? sumTS / cTS : 0.0, cTS);
  printf("states WITHOUT total support: %lld   residual max %.3e  mean %.3e  (cells %lld)\n",
         nNoTS, mxNo, cNo ? sumNo / cNo : 0.0, cNo);
  printf("mask-legal cells: %lld live, %lld capacity-infeasible (%.2f%%)\n",
         liveCells, deadCells, 100.0 * deadCells / double(liveCells + deadCells));
  return 0;
}
