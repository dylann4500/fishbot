// Adversarial verification of the P3 claim that `priorPhi` "carries no information".
// Standalone.  Replays real v0.4 self-play games into a fresh Knowledge and
// compares the sinkhornDisj posterior under several (theta, phi) settings.
//   clang++ -std=c++20 -O3 -march=native src/probe_vdeception_main.cpp -o vdec -pthread
#include "factory.hpp"
#include "game.hpp"
#include "belief.hpp"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace fish;

static std::string argVal(int argc, char** argv, const char* key, const char* dflt) {
  std::string k = std::string("--") + key + "=";
  for (int i = 1; i < argc; i++) { std::string a = argv[i];
    if (a.rfind(k, 0) == 0) return a.substr(k.size()); }
  return dflt;
}

struct Acc {
  const char* label;
  double theta, phi;
  double sumL1 = 0, maxL1 = 0;      // vs reference (shipped)
  double sumMaxCell = 0, maxCell = 0;
  long long n = 0;
  long long flips = 0, cards = 0;   // argmax-owner changes
  double silAbs = 0; long long silAbsN = 0;
};

int main(int argc, char** argv) {
  std::string spec = argVal(argc, argv, "spec", "v04");
  int games = atoi(argVal(argc, argv, "games", "40").c_str());
  uint64_t seed = strtoull(argVal(argc, argv, "seed", "777001").c_str(), nullptr, 10);
  int stride = atoi(argVal(argc, argv, "stride", "3").c_str());
  int outer = atoi(argVal(argc, argv, "outer", "4").c_str());
  int inner = atoi(argVal(argc, argv, "inner", "8").c_str());
  Rules rules;

  const double TH = atof(argVal(argc, argv, "th", "0.26380").c_str());
  const double PH = atof(argVal(argc, argv, "ph", "0.13280").c_str());

  std::vector<Acc> accs = {
    {"phi=0    theta=theta+phi (degeneracy prediction)", TH + PH, 0.0},
    {"identical control (theta,phi) unchanged",         TH,      PH},
    {"phi=0    theta unchanged (the P3 ablation)",       TH,      0.0},
    {"theta=0  phi unchanged",                           0.0,     PH},
    {"theta=0  phi=0 (policy-agnostic)",                 0.0,     0.0},
    {"PURE COLUMN FACTOR  theta=-phi=-0.1328",          -0.1328,  0.1328},
    {"PURE COLUMN FACTOR  theta=-phi=-0.01328",         -0.01328, 0.01328},
    {"PURE COLUMN FACTOR  theta=-phi=-0.001328",        -0.001328,0.001328},
  };

  // clipping census
  long long cellsTot = 0, cellsClipLo = 0, cellsClipHi = 0;
  long long statesTot = 0, statesAnyClip = 0;
  // "silence cell" census: cards in a set S where player p has askCount[p][S]==0
  // and totalAsks[p] >= 4.  These are exactly the cells phi is supposed to damp.
  double silShip = 0, silNoPhi = 0, silDeg = 0; long long silN = 0;

  Belief bRef, bVar;

  for (int gi = 0; gi < games; gi++) {
    uint64_t s = mixSeed(seed, uint64_t(gi) * 2654435761ull + 11);
    Agent* ag[NPLAY]; std::unique_ptr<Agent> own[NPLAY];
    for (int p = 0; p < NPLAY; p++) { own[p] = makeAgent(spec); ag[p] = own[p].get(); }
    Game gm; gm.trace.on = true; gm.run(s, rules, ag);

    // replay from seat 0's view
    Knowledge k; k.init(0, gm.g.dealt[0], rules.deckSets);
    int idx = 0;
    for (const Event& e : gm.trace.events) {
      k.onEvent(e);
      idx++;
      if (idx % stride) continue;
      int Q = __builtin_popcountll(k.unresolved);
      if (Q < 3) continue;

      // clipping census on the cells that actually enter the prior
      bool anyClip = false;
      uint64_t u = k.unresolved;
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        int S = setOf(c);
        for (int p = 0; p < NPLAY; p++) {
          if (!(k.mask[c] & (1u << p))) continue;
          double a = k.askCount[p][S];
          double z = TH * a - PH * (double(k.totalAsks[p]) - a);
          cellsTot++;
          if (z > 2.6) { cellsClipHi++; anyClip = true; }
          else if (z < -2.6) { cellsClipLo++; anyClip = true; }
        }
      }
      statesTot++; if (anyClip) statesAnyClip++;

      bRef.sinkhornDisj(k, outer, inner, TH, PH);
      double ref[NCARD][NPLAY];
      u = k.unresolved;
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        for (int p = 0; p < NPLAY; p++) ref[c][p] = bRef.marg[c][p]; }

      for (auto& A : accs) {
        bVar.sinkhornDisj(k, outer, inner, A.theta, A.phi);
        double l1 = 0, mx = 0;
        uint64_t v = k.unresolved;
        while (v) { int c = __builtin_ctzll(v); v &= v - 1;
          for (int p = 0; p < NPLAY; p++) {
            double d = std::fabs(bVar.marg[c][p] - ref[c][p]);
            l1 += d; if (d > mx) mx = d; } }
        // argmax-owner flip rate
        { uint64_t z = k.unresolved;
          while (z) { int c = __builtin_ctzll(z); z &= z - 1;
            int ba = 0, bb = 0;
            for (int p = 1; p < NPLAY; p++) { if (ref[c][p] > ref[c][ba]) ba = p;
                                              if (bVar.marg[c][p] > bVar.marg[c][bb]) bb = p; }
            A.cards++; if (ba != bb) A.flips++;
            int S = setOf(c);
            for (int p = 0; p < NPLAY; p++) {
              if (!(k.mask[c] & (1u << p))) continue;
              if (k.askCount[p][S] != 0) continue;
              if (k.totalAsks[p] < 4) continue;
              A.silAbs += std::fabs(bVar.marg[c][p] - ref[c][p]); A.silAbsN++; } } }
        l1 /= double(Q);                 // mean total-variation-ish per card
        A.sumL1 += l1; if (l1 > A.maxL1) A.maxL1 = l1;
        A.sumMaxCell += mx; if (mx > A.maxCell) A.maxCell = mx;
        A.n++;

        if (A.theta == TH && A.phi == 0.0) {   // the P3 ablation arm
          uint64_t w = k.unresolved;
          while (w) { int c = __builtin_ctzll(w); w &= w - 1;
            int S = setOf(c);
            for (int p = 0; p < NPLAY; p++) {
              if (!(k.mask[c] & (1u << p))) continue;
              if (k.askCount[p][S] != 0) continue;
              if (k.totalAsks[p] < 4) continue;
              silShip += ref[c][p]; silNoPhi += bVar.marg[c][p]; silN++; } }
        }
        if (A.theta == TH + PH && A.phi == 0.0) {
          uint64_t w = k.unresolved;
          while (w) { int c = __builtin_ctzll(w); w &= w - 1;
            int S = setOf(c);
            for (int p = 0; p < NPLAY; p++) {
              if (!(k.mask[c] & (1u << p))) continue;
              if (k.askCount[p][S] != 0) continue;
              if (k.totalAsks[p] < 4) continue;
              silDeg += bVar.marg[c][p]; } }
        }
      }
    }
  }

  printf("spec=%s games=%d seed=%llu stride=%d outer=%d inner=%d\n",
         spec.c_str(), games, (unsigned long long)seed, stride, outer, inner);
  printf("belief states sampled: %lld   prior cells: %lld\n", statesTot, cellsTot);
  printf("clip census (shipped theta=%.5f phi=%.5f): hi %.3f%%  lo %.3f%%  states with any clip %.2f%%\n",
         TH, PH, 100.0 * double(cellsClipHi) / std::max(1LL, cellsTot),
         100.0 * double(cellsClipLo) / std::max(1LL, cellsTot),
         100.0 * double(statesAnyClip) / std::max(1LL, statesTot));
  printf("\n%-52s  %10s %10s %10s %10s %9s %11s\n", "variant vs shipped posterior",
         "meanL1/card", "maxL1/card", "mean|dcell|", "max|dcell|", "argmaxflip", "silence|d|");
  for (auto& A : accs)
    printf("%-52s  %10.6f %10.6f %10.6f %10.6f %8.3f%% %11.6f\n", A.label,
           A.sumL1 / std::max(1LL, A.n), A.maxL1,
           A.sumMaxCell / std::max(1LL, A.n), A.maxCell,
           100.0 * double(A.flips) / std::max(1LL, A.cards),
           A.silAbs / std::max(1LL, A.silAbsN));

  printf("\nsilence cells (askCount[p][S]==0 and totalAsks[p]>=4), n=%lld\n", silN);
  printf("  shipped                 mean P = %.6f\n", silShip  / std::max(1LL, silN));
  printf("  phi=0, theta unchanged  mean P = %.6f   (delta %+.6f)\n",
         silNoPhi / std::max(1LL, silN), (silNoPhi - silShip) / std::max(1LL, silN));
  printf("  phi=0, theta=theta+phi  mean P = %.6f   (delta %+.6f)\n",
         silDeg / std::max(1LL, silN), (silDeg - silShip) / std::max(1LL, silN));
  return 0;
}
