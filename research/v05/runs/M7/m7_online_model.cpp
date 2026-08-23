// Does the online per-seat model identify the owner's manoeuvre, and is the tilt
// it applies bounded?  Seat 1 runs the marked policy, everyone else v0.5.
// Observer = seat 0 (an opponent of seat 1).
#include "factory.hpp"
#include "game.hpp"
#include "v05_oppmodel.hpp"
#include <cstdio>
#include <string>
using namespace fish;

int main(int argc, char** argv) {
  std::string marked = argc > 1 ? argv[1] : "withholder:k=6";
  int games = argc > 2 ? atoi(argv[2]) : 40;
  uint64_t seed = argc > 3 ? strtoull(argv[3], nullptr, 10) : 515151;
  double dataBias = argc > 4 ? atof(argv[4]) : 2.0;
  double carry = argc > 5 ? atof(argv[5]) : 0.70;
  m7::OppModel mdl;

  double postSum[NPLAY][m7::NTYPE] = {};
  long long mapCount[NPLAY][m7::NTYPE] = {};
  double tiltSum = 0, tiltMax = 0; long long tiltN = 0, tiltNz = 0;
  double thetaSum[NPLAY] = {}; long long thetaN = 0;
  long long episodes[NPLAY] = {};

  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
    for (int i = 0; i < NPLAY; i++) {
      ag[i] = makeAgent(i == 1 ? marked : "v05"); ap[i] = ag[i].get();
    }
    Game gm; Rules r;
    if (gi == 0) { m7::M7Config mc; mc.dataBias = dataBias; mc.carryWeight = carry;
                   mc.persistTypes = carry > 0; mdl.reset(0, mc); }
    else mdl.newDeal();
    gm.observer = [&](const Game& gg) {
      const Event& e = gg.g.pub.history.back();
      mdl.onEvent(e, gg.agents[0]->k);
      const Knowledge& kk = gg.agents[0]->k;
      if (gg.g.pub.nEvents % 11 || !kk.unresolved) return;
      uint64_t u = kk.unresolved;
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        for (int p = 0; p < NPLAY; p++) {
          if (p == 0 || !(kk.mask[c] & (1u << p))) continue;
          double z = mdl.logTilt(kk, c, p, 1.0);
          tiltN++; tiltSum += -z; if (z != 0) tiltNz++;
          if (-z > tiltMax) tiltMax = -z; } }
      for (int p = 1; p < NPLAY; p++) thetaSum[p] += mdl.thetaFor(p, 0.39660);
      thetaN++;
    };
    gm.run(mixSeed(seed, gi), r, ap);
    for (int p = 1; p < NPLAY; p++) {
      for (int t = 0; t < m7::NTYPE; t++) postSum[p][t] += mdl.post[p][t];
      mapCount[p][mdl.mapType(p)]++;
      for (int s = 0; s < NSET; s++) episodes[p] += mdl.cell[p][s].n;
    }
  }
  printf("marked seat 1 = %-16s  %d deals  dataBias=%.2f  carry=%.2f  (observer = seat 0)\n",
         marked.c_str(), games, dataBias, carry);
  printf("%-6s %-8s %-8s %-8s %-11s %-8s | %-8s %-9s %s\n",
         "seat", "ordinary", "v03like", "silent", "withholder", "feint", "MAP=with", "theta_eff", "episodes/game");
  for (int p = 1; p < NPLAY; p++) {
    printf("%-6d ", p);
    for (int t = 0; t < m7::NTYPE; t++) printf("%-8.3f ", postSum[p][t] / games);
    printf("  | %-8.2f %-9.4f %.1f\n",
           double(mapCount[p][m7::T_WITHHOLD]) / games, thetaSum[p] / thetaN,
           double(episodes[p]) / games);
  }
  printf("tilt: %.2f%% of (card,seat) cells non-zero, mean |z| over all cells %.4f, max %.4f\n",
         100.0 * tiltNz / tiltN, tiltSum / tiltN, tiltMax);
  return 0;
}
