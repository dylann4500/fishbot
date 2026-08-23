// SCRATCH PROBE (adversarial verification of the turn-transfer claim).
// Independent of probe_coordination*.hpp: this runs the UNMODIFIED fish::Game
// driver (engine/src/game.hpp) and instruments the agent, not the driver.
// A V04Agent subclass intercepts choosePassTarget and records the candidate
// count the driver actually offered.
#pragma once
#include "fish.hpp"
#include "game.hpp"
#include "v04.hpp"
#include "factory.hpp"
#include <thread>
#include <vector>
#include <cstdio>

namespace fish {
namespace passverify {

struct PVStats {
  long long games = 0, events = 0;
  long long passEvents = 0;      // == choosePassTarget calls
  long long nCand[4] = {0,0,0,0};// how many live teammates were offered (0..3)
  long long gamesWithPass = 0, gamesWithMulti = 0;
  long long limitHits = 0;
  // number of Pass events observed in the emitted event stream (a second,
  // fully independent count taken from Trace, not from the agent hook)
  long long tracePass = 0;
  void merge(const PVStats& o) {
    games += o.games; events += o.events; passEvents += o.passEvents;
    for (int i = 0; i < 4; i++) nCand[i] += o.nCand[i];
    gamesWithPass += o.gamesWithPass; gamesWithMulti += o.gamesWithMulti;
    limitHits += o.limitHits; tracePass += o.tracePass;
  }
};

struct PVAgent : V04Agent {
  PVStats* st = nullptr;
  int gPass = 0, gMulti = 0;     // per-game tallies, reset by the runner
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    if (st) {
      st->passEvents++;
      st->nCand[n < 0 ? 0 : (n > 3 ? 3 : n)]++;
      gPass++;
      if (n >= 2) gMulti++;
    }
    return V04Agent::choosePassTarget(pub, cand, n);
  }
};

struct PVConfig {
  int games = 1500;
  int rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 0;
};

inline PVStats runPassVerify(const PVConfig& pc) {
  int nThreads = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, pc.games));
  std::vector<PVStats> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      PVStats& st = local[t];
      std::unique_ptr<PVAgent> A[6];
      for (int i = 0; i < 6; i++) { A[i].reset(new PVAgent()); A[i]->st = &st; }
      Game game;
      game.trace.on = true;
      for (int i = t; i < pc.games; i += nThreads) {
        uint64_t s = mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < pc.rotations; rot++) {
          int shift = (pc.rotations == 2) ? 0 : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = A[p].get();
          game.rotation = shift;
          for (int p = 0; p < 6; p++) { A[p]->gPass = 0; A[p]->gMulti = 0; }
          game.trace.events.clear();
          GameResult r = game.run(s, pc.rules, ag);
          st.games++;
          st.events += r.events;
          if (r.hitLimit) st.limitHits++;
          int gp = 0, gm = 0;
          for (int p = 0; p < 6; p++) { gp += A[p]->gPass; gm += A[p]->gMulti; }
          if (gp) st.gamesWithPass++;
          if (gm) st.gamesWithMulti++;
          for (const auto& e : game.trace.events) if (e.kind == Kind::Pass) st.tracePass++;
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  PVStats total;
  for (int t = 0; t < nThreads; t++) total.merge(local[t]);
  return total;
}

inline void printPassVerify(const PVStats& s, std::ostream& os) {
  auto pct = [](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  long long G = s.games;
  long long dec = s.passEvents;
  os << "games                " << G << "   events/game " << (G ? double(s.events) / G : 0) << "\n";
  os << "limit-hit games      " << s.limitHits << " (" << pct(s.limitHits, G) << "%)\n";
  os << "pass hook calls      " << dec << "   per game " << (G ? double(dec) / G : 0) << "\n";
  os << "trace Pass events    " << s.tracePass << "   per game " << (G ? double(s.tracePass) / G : 0) << "\n";
  os << "candidate count n=1  " << s.nCand[1] << " (" << pct(s.nCand[1], dec) << "%)\n";
  os << "candidate count n=2  " << s.nCand[2] << " (" << pct(s.nCand[2], dec) << "%)\n";
  os << "candidate count n=3  " << s.nCand[3] << " (" << pct(s.nCand[3], dec) << "%)\n";
  os << "multi (n>=2) per game " << (G ? double(s.nCand[2] + s.nCand[3]) / G : 0) << "\n";
  os << "games with >=1 pass  " << s.gamesWithPass << " (" << pct(s.gamesWithPass, G) << "%)\n";
  os << "games with >=1 multi " << s.gamesWithMulti << " (" << pct(s.gamesWithMulti, G) << "%)\n";
}

}  // namespace passverify
}  // namespace fish
