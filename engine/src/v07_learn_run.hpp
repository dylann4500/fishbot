// Drivers for candidate K5.  Kept out of v07_learn.hpp because the capture
// driver needs the arena and factory.hpp needs v07_learn.hpp.
#pragma once
#include "arena.hpp"
#include "v07_learn.hpp"
#include <cstdio>

namespace fish {
namespace v07learn {

struct CapConfig {
  std::string spec = "v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26";
  std::string opp;                 // empty => self-play (mirror), the deployment shape
  uint64_t seed = 7030004;
  int games = 400, rotations = 2, threads = 2;
  std::string out;
};

// Runs the search in self-play on a TRAINING bank with the capture hook
// installed, and writes one CSV row per live candidate of every searched ask
// decision.  Self-play is the right generating distribution: the deployed
// configuration of v0.7 is three copies of itself.
inline int runCapture(const CapConfig& cc) {
  MatchConfig mc;
  mc.specA = cc.spec;
  mc.specB = cc.opp.empty() ? cc.spec : cc.opp;
  mc.games = cc.games; mc.rotations = cc.rotations; mc.seed = cc.seed;
  mc.threads = cc.threads;
  mc.captureDecisions = true;      // this is what makes captureDeal()/Rot() live
  mc.captureTeamAOnly = false;
  mc.captureArm = 2;
  capSink().rows.clear(); capSink().decisions = 0;
  searchCaptureHook() = &capHandler;
  MatchStats st = runMatch(mc);
  searchCaptureHook() = nullptr;
  const int W = V7L_IDW + V7L_FEATW;
  FILE* f = cc.out.empty() ? stdout : fopen(cc.out.c_str(), "w");
  if (!f) { fprintf(stderr, "fish: v7learn: cannot open %s\n", cc.out.c_str()); return 2; }
  for (int j = 0; j < W; j++) fprintf(f, "%s%s", j ? "," : "", colName(j));
  fprintf(f, "\n");
  const std::vector<double>& R = capSink().rows;
  for (size_t i = 0; i + size_t(W) <= R.size(); i += size_t(W)) {
    for (int j = 0; j < W; j++) fprintf(f, "%s%.10g", j ? "," : "", R[i + size_t(j)]);
    fprintf(f, "\n");
  }
  if (f != stdout) fclose(f);
  fprintf(stderr, "v7learn capture: %lld searched decisions, %zu rows, %.2f games/s\n",
          capSink().decisions, R.size() / size_t(W), st.gamesPerSec(cc.rotations));
  return 0;
}

} // namespace v07learn
} // namespace fish
