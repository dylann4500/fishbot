// R7 recon probe: branching factor of the ask move set over a game's lifetime.
// Read-only w.r.t. engine/src.
#include "game.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace fish;

int main() {
  std::vector<int> all;
  long long tot = 0, cnt = 0; int mx = 0;
  long long tot0 = 0, cnt0 = 0;
  for (uint64_t seed = 1; seed <= 400; seed++) {
    GameState g{}; dealCards(g, seed, NSET);
    for (int s = 0; s < NSET; s++) { g.pub.setActive[s] = true; g.setWinner[s] = 2; }
    for (int p = 0; p < NPLAY; p++) g.pub.handCount[p] = uint8_t(__builtin_popcountll(g.hand[p]));
    g.pub.score[0] = g.pub.score[1] = 0;
    Rng rng(mixSeed(seed, 0xBEEFull));
    int turn = g.turn;
    for (int step = 0; step < 120; step++) {
      if (!g.pub.handCount[turn]) { // pass to a teammate with cards
        int c[3], n = 0;
        for (int p = teamOf(turn); p < NPLAY; p += 2) if (g.pub.handCount[p]) c[n++] = p;
        if (!n) break;
        turn = c[rng.u32(uint32_t(n))]; continue;
      }
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(g.pub, g.hand[turn], turn, buf);
      if (!n) break;
      tot += n; cnt++; all.push_back(n); if (n > mx) mx = n;
      if (step == 0) { tot0 += n; cnt0++; }
      AskMove mv = buf[rng.u32(uint32_t(n))];
      bool hit = (g.hand[mv.target] & bit(mv.card)) != 0;
      if (hit) {
        g.hand[mv.target] &= ~bit(mv.card); g.hand[turn] |= bit(mv.card);
        g.pub.handCount[mv.target]--; g.pub.handCount[turn]++;
      } else {
        turn = mv.target;
      }
    }
  }
  std::sort(all.begin(), all.end());
  printf("legal asks per decision: mean %.2f  median %d  p90 %d  max %d   (n=%lld decisions, 400 games)\n",
         double(tot)/double(cnt), all[all.size()/2], all[all.size()*9/10], mx, cnt);
  printf("at the opening decision: mean %.2f over %lld games\n", double(tot0)/double(cnt0), cnt0);
  return 0;
}
