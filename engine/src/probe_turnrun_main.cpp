// Scratch driver for the turn-transfer verification.  Not part of `fish`:
//   clang++ -std=c++20 -O3 -march=native src/probe_turnrun_main.cpp -o probe_turn -pthread
#include "probe_turnrun.hpp"
#include <iostream>
#include <cstdio>

using namespace fish;
using namespace fish::probeturn;

static std::string argVal(int argc, char** argv, const char* key, const char* dflt) {
  std::string k = std::string("--") + key + "=";
  for (int i = 1; i < argc; i++) { std::string a = argv[i];
    if (a.rfind(k, 0) == 0) return a.substr(k.size()); }
  return dflt;
}

static double pct(long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; }
static const char* acqName(int i) {
  const char* n[4] = {"LEAD", "MISS-IN", "PASS", "MISSCLES"};
  return n[i];
}
static int CTRL = ACQ_MISS;   // control arm, overridable with --ctrl=misscl

int main(int argc, char** argv) {
  TurnRunConfig pc;
  pc.games   = atoi(argVal(argc, argv, "games", "600").c_str());
  pc.seed    = strtoull(argVal(argc, argv, "seed", "31").c_str(), nullptr, 10);
  pc.specA   = argVal(argc, argv, "a", "v04");
  pc.specB   = argVal(argc, argv, "b", "v04");
  pc.threads = atoi(argVal(argc, argv, "threads", "0").c_str());
  if (std::string(argVal(argc, argv, "ctrl", "miss")) == "misscl") CTRL = ACQ_MISS_CL;

  TurnRunStats s = run(pc);
  printf("games %lld  events %lld  asks %lld  hits %lld  GLOBAL hit rate %.4f%%\n",
         s.games, s.events, s.asks, s.hits, pct(s.hits, s.asks));
  printf("pass events %lld  (%.4f/game)\n", s.passEvents, double(s.passEvents) / double(s.games));
  printf("\npossessions by how the turn was acquired\n");
  printf("%-8s %8s %10s %10s %10s %10s %10s\n",
         "acq", "runs", "asks/run", "hits/run", "hitrate%", "0-ask%", "endmiss%");
  for (int i = 0; i < ACQ_N; i++) {
    const Bucket& b = s.by[i];
    printf("%-8s %8lld %10.5f %10.5f %10.4f %10.3f %10.3f\n", acqName(i), b.runs,
           b.runs ? double(b.asks) / b.runs : 0.0,
           b.runs ? double(b.hits) / b.runs : 0.0,
           pct(b.hits, b.asks), pct(b.zeroAsk, b.runs), pct(b.endedByMiss, b.runs));
  }

  // ---- phase-matched control: PASS vs MISS-IN at the same cards-in-play count
  printf("\nPHASE-MATCHED (cards still in play at possession start)\n");
  printf("%6s | %8s %9s %9s | %8s %9s %9s\n", "cards",
         "passRuns", "asks/run", "hitrate%", "missRuns", "asks/run", "hitrate%");
  long long mA = 0, mH = 0, mR = 0;   // MISS-IN reweighted to the PASS phase profile
  double wAsks = 0, wHits = 0, wDen = 0;
  for (auto& kv : s.phase[ACQ_PASS]) {
    int c = kv.first;
    const Bucket& P = kv.second;
    Bucket M;
    auto it = s.phase[CTRL].find(c);
    if (it != s.phase[CTRL].end()) M = it->second;
    if (P.runs < 5 && M.runs < 5) continue;
    printf("%6d | %8lld %9.4f %9.4f | %8lld %9.4f %9.4f\n", c,
           P.runs, P.runs ? double(P.asks) / P.runs : 0.0, pct(P.hits, P.asks),
           M.runs, M.runs ? double(M.asks) / M.runs : 0.0, pct(M.hits, M.asks));
    if (M.runs) { wAsks += double(P.runs) * double(M.asks) / M.runs;
                  wHits += double(P.runs) * double(M.hits) / M.runs;
                  wDen  += double(P.runs); }
    mR += M.runs; mA += M.asks; mH += M.hits;
  }
  printf("\nCONTROL(%s) possessions reweighted to the PASS phase profile:\n", acqName(CTRL));
  printf("  asks/run %.5f   hits/run %.5f   hit rate %.4f%%   (weight %.0f)\n",
         wDen ? wAsks / wDen : 0.0, wDen ? wHits / wDen : 0.0,
         wAsks > 0 ? 100.0 * wHits / wAsks : 0.0, wDen);
  const Bucket& P = s.by[ACQ_PASS];
  printf("  PASS actual         asks/run %.5f   hits/run %.5f   hit rate %.4f%%\n",
         P.runs ? double(P.asks) / P.runs : 0.0, P.runs ? double(P.hits) / P.runs : 0.0,
         pct(P.hits, P.asks));
  printf("  share of all asks inside PASS possessions: %.4f%%\n", pct(P.asks, s.asks));
  printf("  asks inside a PASS possession NOT made by the receiver: %lld\n", s.passAsksByOther);
  return 0;
}
