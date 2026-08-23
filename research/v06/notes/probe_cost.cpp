// R7 recon probe: cost of one belief re-derivation (sinkhornDisj), the atomic
// unit of v0.5's two-ply chain term, and of one exact block-DP build.
#include "game.hpp"
#include "v05.hpp"
#include "blockdp.hpp"
#include <chrono>
#include <cstdio>
using namespace fish;
using clk = std::chrono::steady_clock;

int main() {
  GameState g{}; dealCards(g, 7, NSET);
  for (int s = 0; s < NSET; s++) { g.pub.setActive[s] = true; g.setWinner[s] = 2; }
  for (int p = 0; p < NPLAY; p++) g.pub.handCount[p] = uint8_t(__builtin_popcountll(g.hand[p]));
  Knowledge k; k.init(0, g.hand[0], NSET);
  // Sinkhorn cost at the opening state.
  const int REPS = 20000;
  Belief b;
  auto t0 = clk::now();
  for (int i = 0; i < REPS; i++) {
    for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) b.marg[c][q] = 0;
    for (int c = 0; c < NCARD; c++) if (k.owner[c] < NPLAY) b.marg[c][k.owner[c]] = 1;
    b.sinkhornDisj(k, 4, 8, 0.44458, 0.12198);
  }
  auto t1 = clk::now();
  double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / REPS;
  printf("sinkhornDisj(outer=4,inner=8) at opening: %.1f us/call\n", us);
  return 0;
}
