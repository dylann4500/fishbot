#include "factory.hpp"
#include "game.hpp"
#include <chrono>
#include <cstdio>
using namespace fish;
int main(int argc, char** argv) {
  std::string spec = argc > 1 ? argv[1] : "v05";
  int games = argc > 2 ? atoi(argv[2]) : 200;
  Rules r; Game g;
  std::vector<std::unique_ptr<Agent>> owned;
  Agent* ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) { owned.push_back(makeAgent(spec)); ag[p] = owned.back().get(); }
  long long ev = 0, asks = 0;
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < games; i++) { auto res = g.run(mixSeed(1234567, i), r, ag); ev += res.events; asks += res.asks; }
  double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  printf("%-24s games=%d events=%lld asks=%lld  total=%.3fs  us/ask=%.1f  us/game=%.0f\n",
         spec.c_str(), games, ev, asks, dt, 1e6 * dt / double(asks), 1e6 * dt / games);
  return 0;
}
