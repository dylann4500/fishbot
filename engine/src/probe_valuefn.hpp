// P7 scratch probe: dump value-function training rows with game-level metadata
// so the fit can be evaluated with a GAME-clustered held-out split, and so the
// label's dependence on adjudication / forced endgame can be measured.
//
// Nothing here is on any decision path; it is only reachable from the
// `fish dumpvalue` command appended at the end of main.cpp.
#pragma once
#include "arena.hpp"
#include <cstdio>
#include <thread>

namespace fish {

struct DumpConfig {
  std::string specA = "v04", specB = "v04";
  int games = 400;
  int rotations = 2;
  uint64_t seed = 31415;
  Rules rules;
  int threads = 0;
  std::string out = "rows.csv";
};

struct DumpRows {
  std::vector<std::array<float, VFEAT_MAX>> X;
  std::vector<float> y;
  std::vector<int> team;
  std::vector<int> gameId;
  std::vector<uint8_t> capped;     // game ended at the ask cap (adjudicated)
  std::vector<uint8_t> forced;     // game contained >=1 forced-endgame declaration
  std::vector<uint16_t> evIdx;     // event index of the game at which the row was taken
};

inline void runDump(const DumpConfig& dc, DumpRows& outRows, int nfeat) {
  int nThreads = dc.threads > 0 ? dc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  std::vector<DumpRows> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(dc.specA); B[i] = makeAgent(dc.specB); }
      Game game;
      DumpRows& dst = local[t];
      for (int i = t; i < dc.games; i += nThreads) {
        uint64_t s = mixSeed(dc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < dc.rotations; rot++) {
          int orient = (dc.rotations == 2) ? rot : (rot / 3);
          int shift  = (dc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          ValueSink sink;
          game.vsink = &sink;
          game.rotation = shift;
          GameResult r = game.run(s, dc.rules, ag);
          game.vsink = nullptr;
          int gid = i * 16 + rot;
          uint8_t cap = r.hitLimit ? 1 : 0;
          uint8_t fdc = (r.forcedDecls[0] + r.forcedDecls[1]) > 0 ? 1 : 0;
          for (size_t k = 0; k < sink.y.size(); k++) {
            dst.X.push_back(sink.X[k]);
            dst.y.push_back(sink.y[k]);
            dst.team.push_back(sink.team[k]);
            dst.gameId.push_back(gid);
            dst.capped.push_back(cap);
            dst.forced.push_back(fdc);
            dst.evIdx.push_back(uint16_t(k / NPLAY));
          }
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  for (int t = 0; t < nThreads; t++) {
    outRows.X.insert(outRows.X.end(), local[t].X.begin(), local[t].X.end());
    outRows.y.insert(outRows.y.end(), local[t].y.begin(), local[t].y.end());
    outRows.team.insert(outRows.team.end(), local[t].team.begin(), local[t].team.end());
    outRows.gameId.insert(outRows.gameId.end(), local[t].gameId.begin(), local[t].gameId.end());
    outRows.capped.insert(outRows.capped.end(), local[t].capped.begin(), local[t].capped.end());
    outRows.forced.insert(outRows.forced.end(), local[t].forced.begin(), local[t].forced.end());
    outRows.evIdx.insert(outRows.evIdx.end(), local[t].evIdx.begin(), local[t].evIdx.end());
  }
  (void)nfeat;
}

inline int writeDump(const DumpConfig& dc, const DumpRows& rows, int nfeat) {
  FILE* f = fopen(dc.out.c_str(), "w");
  if (!f) { fprintf(stderr, "cannot open %s\n", dc.out.c_str()); return 1; }
  fprintf(f, "gameid,team,capped,forced,evidx");
  for (int i = 0; i < nfeat; i++) fprintf(f, ",f%d", i);
  fprintf(f, ",y\n");
  for (size_t k = 0; k < rows.y.size(); k++) {
    fprintf(f, "%d,%d,%d,%d,%d", rows.gameId[k], rows.team[k], int(rows.capped[k]),
            int(rows.forced[k]), int(rows.evIdx[k]));
    for (int i = 0; i < nfeat; i++) fprintf(f, ",%.6g", double(rows.X[k][i]));
    fprintf(f, ",%.6g\n", double(rows.y[k]));
  }
  fclose(f);
  return 0;
}

} // namespace fish

// ---------------------------------------------------------------------------
// Shadow-agent probe: does perturbing a value-function coefficient change the
// action CHOSEN AT THE SAME STATE?  A whole-game win-rate comparison cannot
// answer this, because the trajectory is chaotic -- a 1e-6 change to a
// coefficient that provably cancels still reshuffles float ties and sends the
// game somewhere else.  Here the shadow policies see exactly the same public
// history as the primary and are asked for a move at the same states.
namespace fish {

struct ShadowStats {
  std::vector<long long> askTotal, askDiff, declTotal, declDiff;
  void init(size_t n) { askTotal.assign(n,0); askDiff.assign(n,0); declTotal.assign(n,0); declDiff.assign(n,0); }
  void merge(const ShadowStats& o) {
    for (size_t i = 0; i < askTotal.size(); i++) {
      askTotal[i]+=o.askTotal[i]; askDiff[i]+=o.askDiff[i];
      declTotal[i]+=o.declTotal[i]; declDiff[i]+=o.declDiff[i];
    }
  }
};

struct ShadowAgent : Agent {
  std::unique_ptr<Agent> prim;
  std::vector<std::unique_ptr<Agent>> shad;
  ShadowStats* st = nullptr;
  const char* name() const override { return "shadow"; }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    prim->reset(s, hand, r, seed);
    for (auto& a : shad) a->reset(s, hand, r, seed);
  }
  void observe(const Event& e) override {
    Agent::observe(e); prim->observe(e);
    for (auto& a : shad) a->observe(e);
  }
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove m = prim->chooseAsk(pub);
    for (size_t i = 0; i < shad.size(); i++) {
      AskMove m2 = shad[i]->chooseAsk(pub);
      st->askTotal[i]++;
      if (m2.card != m.card || m2.target != m.target) st->askDiff[i]++;
    }
    return m;
  }
  double lastAskForecast() const override { return prim->lastAskForecast(); }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    bool w = prim->proposeDeclaration(pub, d, conf);
    for (size_t i = 0; i < shad.size(); i++) {
      Declaration d2{}; double c2 = 0;
      bool w2 = shad[i]->proposeDeclaration(pub, d2, c2);
      st->declTotal[i]++;
      bool same = (w2 == w);
      if (same && w) {
        if (d2.set != d.set) same = false;
        else for (int j = 0; j < SETSZ; j++) if (d2.owner[j] != d.owner[j]) same = false;
      }
      if (!same) st->declDiff[i]++;
    }
    return w;
  }
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    return prim->choosePassTarget(pub, cand, n);
  }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double th) override {
    return prim->willingForced(pub, set, d, conf, th);
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    prim->bestGuess(pub, set, d, conf);
  }
};

inline void runShadow(const std::string& base, const std::vector<std::string>& variants,
                      int games, uint64_t seed, const Rules& rules, int nThreads, ShadowStats& out) {
  if (nThreads < 1) nThreads = int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  std::vector<ShadowStats> local(nThreads);
  for (auto& s : local) s.init(variants.size());
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::vector<std::unique_ptr<ShadowAgent>> ags(NPLAY);
      for (int p = 0; p < NPLAY; p++) {
        ags[p] = std::make_unique<ShadowAgent>();
        ags[p]->prim = makeAgent(base);
        for (auto& v : variants) ags[p]->shad.push_back(makeAgent(v));
        ags[p]->st = &local[t];
      }
      Agent* raw[NPLAY];
      for (int p = 0; p < NPLAY; p++) raw[p] = ags[p].get();
      Game game;
      for (int i = t; i < games; i += nThreads)
        game.run(mixSeed(seed, uint64_t(i) * 2654435761ull + 1), rules, raw);
    });
  }
  for (auto& th : pool) th.join();
  out.init(variants.size());
  for (auto& s : local) out.merge(s);
}

} // namespace fish
