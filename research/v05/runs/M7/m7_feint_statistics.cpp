// Is there a PUBLIC statistic that separates the Feint?  Candidate: ask breadth
// -- distinct half-suits asked in per ask.  A feinter asks in half-suits it holds
// exactly one card of, so it should spread wider than an honest player.
// Ground-truth control: cards actually held in S at the moment of the ask.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <string>
using namespace fish;

int main(int argc, char** argv) {
  std::string spec = argc > 1 ? argv[1] : "v04";
  int games = argc > 2 ? atoi(argv[2]) : 40;
  uint64_t seed = argc > 3 ? strtoull(argv[3], nullptr, 10) : 20260822;
  double sumBreadth = 0; int nSeatGames = 0;
  double sumHeld = 0; long long nAsk = 0;
  long long holdOne = 0;
  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
    for (int i = 0; i < NPLAY; i++) { ag[i] = makeAgent(spec); ap[i] = ag[i].get(); }
    Game gm; Rules r;
    int asks[NPLAY] = {0,0,0,0,0,0};
    uint16_t setsSeen[NPLAY] = {0,0,0,0,0,0};
    gm.observer = [&](const Game& gg) {
      const Event& e = gg.g.pub.history.back();
      if (e.kind != Kind::Ask) return;
      int S = setOf(e.card);
      asks[e.actor]++; setsSeen[e.actor] |= uint16_t(1u << S);
      // ground truth: cards of S the actor held when it asked (post-event hand,
      // plus the asked card back out if the ask hit)
      uint64_t h = gg.g.hand[e.actor];
      if (e.success) h &= ~bit(e.card);
      int held = __builtin_popcountll(h & setMask(S));
      sumHeld += held; nAsk++; if (held == 1) holdOne++;
    };
    gm.run(mixSeed(seed, gi), r, ap);
    for (int p = 0; p < NPLAY; p++) if (asks[p] >= 4) {
      sumBreadth += double(__builtin_popcount(setsSeen[p])) / asks[p]; nSeatGames++; }
  }
  printf("%-18s breadth (distinct sets / ask) %.4f   mean cards held in S at ask %.3f"
         "   asks with exactly 1 card of S %.1f%%\n",
         spec.c_str(), sumBreadth / nSeatGames, sumHeld / nAsk, 100.0 * holdOne / nAsk);
  return 0;
}
