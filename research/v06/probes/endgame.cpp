// v0.6 probe 2: when does the game become exactly solvable, and how much is
// still at stake at that moment?
#include "factory.hpp"
#include "game.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace fish;

int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 200;
  uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 31;
  const char* spec = argc > 3 ? argv[3] : "v05";
  Rules rules;
  std::vector<std::unique_ptr<Agent>> ags;
  for (int i = 0; i < NPLAY; i++) ags.push_back(makeAgent(spec));
  Agent* ag[NPLAY]; for (int i = 0; i < NPLAY; i++) ag[i] = ags[i].get();

  const double THR[4] = {1e9, 1e6, 1e3, 1e1};
  std::vector<int> crossEv[4], crossActive[4], crossSetsLeft[4];
  std::vector<int> totalEv;
  std::vector<double> fracSetsAfter[4];
  long long nDeclAfter[4] = {0,0,0,0}, nDeclTotal = 0;

  Game game;
  for (int gi = 0; gi < games; gi++) {
    uint64_t s = mixSeed(seed, uint64_t(gi) * 2654435761ull + 1);
    int crossed[4] = {-1,-1,-1,-1};
    int activeAt[4] = {0,0,0,0};
    int declaredBefore[4] = {0,0,0,0};
    int nDecl = 0;
    game.observer = [&](const Game& gg) {
      // count declarations so far
      // (recomputed cheaply from score)
      int scored = gg.g.pub.score[0] + gg.g.pub.score[1];
      nDecl = scored;
      int who = gg.g.pub.turn;
      const Knowledge& kk = gg.agents[who]->k;
      DealDP dp;
      if (!dp.build(kk)) return;
      for (int t = 0; t < 4; t++) {
        if (crossed[t] < 0 && dp.N < THR[t]) {
          crossed[t] = gg.g.pub.nEvents;
          activeAt[t] = gg.g.pub.activeSets();
          declaredBefore[t] = scored;
        }
      }
    };
    GameResult r = game.run(s, rules, ag);
    totalEv.push_back(r.events);
    int totalSets = r.score[0] + r.score[1];
    nDeclTotal += totalSets;
    for (int t = 0; t < 4; t++) if (crossed[t] >= 0) {
      crossEv[t].push_back(crossed[t]);
      crossActive[t].push_back(activeAt[t]);
      nDeclAfter[t] += totalSets - declaredBefore[t];
      fracSetsAfter[t].push_back(double(totalSets - declaredBefore[t]) / std::max(1, totalSets));
    }
    (void)nDecl;
  }
  game.observer = nullptr;

  auto med = [](std::vector<int> v){ if(v.empty()) return -1; std::sort(v.begin(),v.end()); return v[v.size()/2]; };
  auto meanD = [](const std::vector<double>& v){ if(v.empty()) return 0.0; double s=0; for(double x:v)s+=x; return s/v.size(); };
  printf("policy=%s games=%d  median events/game=%d\n", spec, games, med(totalEv));
  printf("%-12s %8s %12s %14s %18s\n", "dp.N below", "reached%", "median ev", "median active", "mean %sets after");
  for (int t = 0; t < 4; t++) {
    printf("%-12.0e %7.1f%% %12d %14d %17.1f%%\n", THR[t],
      100.0*crossEv[t].size()/games, med(crossEv[t]), med(crossActive[t]), 100.0*meanD(fracSetsAfter[t]));
  }
  return 0;
}
