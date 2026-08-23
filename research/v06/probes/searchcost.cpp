// Feasibility probe for FishBot v0.6 test-time search.
// Measures, at real mid-game decision points reached by v0.5 self-play:
//   (a) cost of DealDP::build on the actor's Knowledge
//   (b) cost of one exact posterior deal sample
//   (c) number of consistent deals (dp.N) as the game progresses
//   (d) cost of a full blueprint continuation from that point with a cheap policy
//   (e) size of the live candidate ask set
#include "factory.hpp"
#include "game.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>

using namespace fish;
using clk = std::chrono::steady_clock;
static double secs(clk::time_point a, clk::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

struct Sample { int ev; double dpN; double tBuild; double tSample; int nLive; int unresolved; };

int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 20;
  uint64_t seed = argc > 2 ? strtoull(argv[2], nullptr, 10) : 31;
  const char* rollSpec = argc > 3 ? argv[3] : "v05:belief=indep,value=0,topk=0";

  Rules rules;
  std::vector<std::unique_ptr<Agent>> ags;
  for (int i = 0; i < NPLAY; i++) ags.push_back(makeAgent("v05"));
  Agent* ag[NPLAY]; for (int i = 0; i < NPLAY; i++) ag[i] = ags[i].get();

  std::vector<Sample> S;
  Game game;
  // Instrument via the observer hook: at each event, snapshot seat 0's knowledge.
  for (int gi = 0; gi < games; gi++) {
    uint64_t s = mixSeed(seed, uint64_t(gi) * 2654435761ull + 1);
    game.observer = [&](const Game& gg) {
      int ev = gg.g.pub.nEvents;
      if (ev % 8 != 0) return;
      int who = gg.g.pub.turn;
      const Knowledge& kk = gg.agents[who]->k;
      DealDP dp;
      auto t0 = clk::now();
      bool ok = dp.build(kk);
      auto t1 = clk::now();
      if (!ok) return;
      Rng r(12345 + ev);
      uint8_t owners[NCARD];
      auto t2 = clk::now();
      for (int i = 0; i < 32; i++) dp.sample(r, owners);
      auto t3 = clk::now();
      AskMove buf[NSET*SETSZ*3];
      int n = enumerateAsks(gg.g.pub, kk.myHand, who, buf);
      int nLive = 0;
      for (int i = 0; i < n; i++) {
        int c = buf[i].card, t = buf[i].target;
        bool dead = kk.owner[c] < NPLAY ? kk.owner[c] != t : !(kk.mask[c] & (1u<<t));
        if (!dead) nLive++;
      }
      S.push_back(Sample{ev, dp.N, secs(t0,t1), secs(t2,t3)/32.0, nLive,
                         __builtin_popcountll(kk.unresolved)});
    };
    game.run(s, rules, ag);
  }
  game.observer = nullptr;

  // (d) cost of a cheap-blueprint FULL game, as the rollout-cost yardstick.
  {
    std::vector<std::unique_ptr<Agent>> rs;
    for (int i = 0; i < NPLAY; i++) rs.push_back(makeAgent(rollSpec));
    Agent* rg[NPLAY]; for (int i = 0; i < NPLAY; i++) rg[i] = rs[i].get();
    Game g2;
    auto t0 = clk::now();
    int N = 400;
    long long evs = 0;
    for (int i = 0; i < N; i++) evs += g2.run(mixSeed(seed, i*7919+3), rules, rg).events;
    auto t1 = clk::now();
    printf("rollout policy \"%s\": %.4f ms/full-game, %.1f events/game, %.2f us/event\n",
           rollSpec, secs(t0,t1)/N*1000.0, double(evs)/N, secs(t0,t1)/double(evs)*1e6);
  }

  // Bucket by event index.
  printf("\n%-8s %8s %14s %10s %10s %8s %8s\n", "event", "n", "median dp.N", "build ms", "samp us", "nLive", "unres");
  int buckets[] = {0, 16, 32, 48, 64, 80, 96, 128, 1000};
  for (int b = 0; b + 1 < int(sizeof(buckets)/sizeof(int)); b++) {
    std::vector<double> dn, tb, ts; std::vector<int> nl, ur;
    for (auto& x : S) if (x.ev >= buckets[b] && x.ev < buckets[b+1]) {
      dn.push_back(x.dpN); tb.push_back(x.tBuild); ts.push_back(x.tSample);
      nl.push_back(x.nLive); ur.push_back(x.unresolved);
    }
    if (dn.empty()) continue;
    auto med = [](std::vector<double> v){ std::sort(v.begin(), v.end()); return v[v.size()/2]; };
    auto medi = [](std::vector<int> v){ std::sort(v.begin(), v.end()); return v[v.size()/2]; };
    printf("%-8d %8zu %14.4g %10.4f %10.2f %8d %8d\n", buckets[b], dn.size(), med(dn),
           med(tb)*1000.0, med(ts)*1e6, medi(nl), medi(ur));
  }
  return 0;
}
