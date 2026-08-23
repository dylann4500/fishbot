// M7 validation harness (out-of-tree; touches no engine file).
//   (a) divergence guard: m7::fitTilted(model off) vs Belief::sinkhornDisj
//   (b) episode census: measure the type-library responsiveness rates from
//       self-play, conditioned on GROUND TRUTH "does the seat hold a card of S"
//   (c) does the opportunity weight actually vary within a half-suit?
#include "factory.hpp"
#include "game.hpp"
#include "v05_oppmodel.hpp"
#include <cstdio>
#include <string>
using namespace fish;

struct Census : m7::Trace {
  // [kind][holdsTruth][outcome]
  long long c[2][2][3] = {};
  long long censored = 0;
  const GameState* gs = nullptr;
  int curSeat = -1, curSet = -1;
  void onEpisode(int seat, int set, int kind, int outcome, double q) override {
    (void)q;
    bool holds = (gs->hand[seat] & setMask(set)) != 0;
    c[kind][holds ? 1 : 0][outcome]++;
  }
};

int main(int argc, char** argv) {
  std::string spec = argc > 1 ? argv[1] : "v04";
  int games = argc > 2 ? atoi(argv[2]) : 30;
  uint64_t seed = argc > 3 ? strtoull(argv[3], nullptr, 10) : 20260822;

  Census cen;
  m7::OppModel mdl;
  m7::M7Config mc;
  double worstSelfTest = 0; long long nSelf = 0;
  long long oppCells = 0, oppVaries = 0;

  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
    for (int i = 0; i < NPLAY; i++) { ag[i] = makeAgent(spec); ap[i] = ag[i].get(); }
    Game gm; Rules r;
    mdl.reset(0, mc);
    mdl.trace = &cen;
    gm.observer = [&](const Game& gg) {
      cen.gs = &gg.g;
      const Event& e = gg.g.pub.history.back();
      mdl.onEvent(e, gg.agents[0]->k);
      const Knowledge& kk = gg.agents[0]->k;
      if (gg.g.pub.nEvents % 7 == 0 && kk.unresolved) {
        double d = m7::selfTest(kk, 4, 8, 0.26380, 0.13280);
        if (d > worstSelfTest) worstSelfTest = d;
        d = m7::selfTest(kk, 4, 8, 0.0, 0.0);
        if (d > worstSelfTest) worstSelfTest = d;
        nSelf += 2;
        // opportunity-weight variation within a half-suit
        for (int p = 0; p < NPLAY; p++) {
          if (p == 0) continue;
          for (int s = 0; s < NSET; s++) {
            if (!kk.setActive[s]) continue;
            int yes = 0, no = 0;
            for (int i = 0; i < SETSZ; i++) {
              int c = cardOf(s, i);
              if (kk.owner[c] != UNKNOWN || !(kk.mask[c] & (1u << p))) continue;
              (m7::OppModel::askable(kk, p, c) ? yes : no)++;
            }
            if (yes + no >= 2) { oppCells++; if (yes && no) oppVaries++; }
          }
        }
      }
    };
    gm.run(mixSeed(seed, gi), r, ap);
  }

  printf("== %s, %d games, seed %llu\n", spec.c_str(), games, (unsigned long long)seed);
  printf("divergence guard: %lld states, max |fitTilted(off) - sinkhornDisj| = %.3e\n",
         nSelf, worstSelfTest);
  printf("opportunity weight varies within a (seat,half-suit): %lld / %lld cells (%.1f%%)\n",
         oppVaries, oppCells, oppCells ? 100.0 * oppVaries / oppCells : 0.0);
  const char* kn[2] = {"reply", "probe"};
  for (int kd = 0; kd < 2; kd++) {
    long long h = cen.c[kd][1][0] + cen.c[kd][1][1] + cen.c[kd][1][2];
    long long v = cen.c[kd][0][0] + cen.c[kd][0][1] + cen.c[kd][0][2];
    printf("%-5s | holds S  n=%6lld  fast %.3f  slow %.3f  never %.3f\n", kn[kd], h,
           h ? double(cen.c[kd][1][0]) / h : 0.0, h ? double(cen.c[kd][1][1]) / h : 0.0,
           h ? double(cen.c[kd][1][2]) / h : 0.0);
    printf("%-5s | no card  n=%6lld  fast %.3f  slow %.3f  never %.3f   <-- fast/slow MUST be 0\n", kn[kd], v,
           v ? double(cen.c[kd][0][0]) / v : 0.0, v ? double(cen.c[kd][0][1]) / v : 0.0,
           v ? double(cen.c[kd][0][2]) / v : 0.0);
  }
  return 0;
}
