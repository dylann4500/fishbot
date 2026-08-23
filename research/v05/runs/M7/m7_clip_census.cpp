// M7 §1.2 clip census.  How often does belief.hpp's +-2.6 clip on
// Knowledge::priorWeight actually bind?  The exponent
//     z(p, S) = theta * askCount[p][S] - phi * (totalAsks[p] - askCount[p][S])
// depends on (seat, half-suit) only, so the unit of account is a
// (seat, half-suit) cell.  Same state selection as m7_algebra_replica.cpp:
// v0.4 mirror, every 5th event, observers 0/2/4.
//
//   clang++ -std=c++20 -O2 -I engine/src research/v05/runs/M7/m7_clip_census.cpp -o /tmp/h -pthread
//   /tmp/h 20 4242
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
using namespace fish;

int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 20;
  uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 4242;
  const double th = 0.26380, ph = 0.13280;
  long long nStates = 0, cells = 0, hi = 0, lo = 0;

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
        for (int q = 0; q < NPLAY; q++) for (int S = 0; S < NSET; S++) {
          if (!kk.setActive[S]) continue;
          double a = kk.askCount[q][S];
          double z = th * a - ph * (double(kk.totalAsks[q]) - a);
          cells++;
          if (z > 2.6) hi++; else if (z < -2.6) lo++;
        }
      }
    };
    gm.run(mixSeed(seed, gi), r, ap);
  }
  printf("states %lld   (seat,half-suit) cells %lld\n", nStates, cells);
  printf("clip binds HIGH (z > +2.6): %lld  (%.2f%%)\n", hi, 100.0 * hi / double(cells));
  printf("clip binds LOW  (z < -2.6): %lld  (%.2f%%)\n", lo, 100.0 * lo / double(cells));
  return 0;
}
