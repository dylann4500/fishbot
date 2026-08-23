// Adversarial verification of the P6 "willingness ladder" claim.
// Build:
//   clang++ -std=c++20 -O3 -march=native src/probe_vladder_main.cpp -o probe_vladder -pthread
// Same runArb harness as probe_declaration_main.cpp, but also reports the
// paired-outcome distribution (how many deals the treatment actually flipped),
// which is what determines the resolution of the win-rate metric.
#include "probe_declaration.hpp"
#include <iostream>
#include <cstdio>

using namespace fish;
using namespace fish::probe;

static std::string argVal(int argc, char** argv, const char* key, const char* dflt) {
  std::string k = std::string("--") + key + "=";
  for (int i = 1; i < argc; i++) { std::string a = argv[i];
    if (a.rfind(k, 0) == 0) return a.substr(k.size()); }
  return dflt;
}

int main(int argc, char** argv) {
  int threads = atoi(argVal(argc, argv, "threads", "0").c_str());
  int games   = atoi(argVal(argc, argv, "games", "3000").c_str());
  uint64_t seed = strtoull(argVal(argc, argv, "seed", "31").c_str(), nullptr, 10);
  std::string spec = argVal(argc, argv, "spec", "v04");
  int mx = atoi(argVal(argc, argv, "x", "3").c_str());
  int my = atoi(argVal(argc, argv, "y", "0").c_str());
  int rungs = atoi(argVal(argc, argv, "rungs", "0").c_str());
  const char* tag = argVal(argc, argv, "tag", "run").c_str();
  std::string tags = argVal(argc, argv, "tag", "run");
  Rules rules;
  ArbMatch st = runArb(spec, games, seed, rules, mx, my, true, true, threads, 0.0, rungs);
  int n = st.deals * 2;
  double m, lo, hi; clusterBootstrap(st.paired, 2, m, lo, hi);
  int p0 = 0, p1 = 0, p2 = 0;
  for (uint8_t v : st.paired) { if (v == 0) p0++; else if (v == 1) p1++; else p2++; }
  printf("{\"tag\":\"%s\",\"seed\":%llu,\"x\":%d,\"y\":%d,\"rungs\":%d,"
         "\"deals\":%d,\"games\":%d,\"xWinRate\":%.6f,\"ci\":[%.6f,%.6f],"
         "\"pairedSplit\":%d,\"pairedXBoth\":%d,\"pairedYBoth\":%d,"
         "\"netDeals\":%d,\"contested\":%lld,\"lowRight\":%lld,\"confRight\":%lld,"
         "\"ladderRight\":%lld,\"meanConfGap\":%.5f,"
         "\"R\":{\"2\":%lld,\"3\":%lld,\"5\":%lld,\"9\":%lld,\"17\":%lld,\"33\":%lld,\"65\":%lld}}\n",
         tags.c_str(), (unsigned long long)seed, mx, my, rungs, st.deals, n,
         double(st.xWins) / n, lo, hi, p1, p2, p0, p2 - p0,
         st.arb.contested, st.arb.lowRight, st.arb.confRight, st.arb.ladRight,
         st.arb.contested ? st.arb.confGapSum / st.arb.contested : 0.0,
         st.arb.ladRightR[0], st.arb.ladRightR[1], st.arb.ladRightR[2],
         st.arb.ladRightR[3], st.arb.ladRightR[4], st.arb.ladRightR[5], st.arb.ladRightR[6]);
  (void)tag;
  return 0;
}
