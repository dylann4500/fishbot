// Probe 5: how much of chooseAsk is more than argmax p, and the target dimension.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
using namespace fish;
struct P5 : V05Agent {
  long long dec = 0, sameAsGreedy = 0, sameCardDiffTarget = 0, targetIsMaxP = 0, multiTarget = 0;
  double sumChosenP = 0, sumMaxP = 0;
  long long sameSetAsGreedy = 0;
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove pick = V05Agent::chooseAsk(pub);      // shipped decision, state already refreshed
    AskMove buf[NSET * SETSZ * 3];
    int n = cfg.liveAskGate ? enumerateLive(pub, buf) : enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return pick;
    dec++;
    int gi = 0; for (int i = 1; i < n; i++) if (bel.marg[buf[i].card][buf[i].target] > bel.marg[buf[gi].card][buf[gi].target]) gi = i;
    if (buf[gi].card == pick.card && buf[gi].target == pick.target) sameAsGreedy++;
    if (setOf(buf[gi].card) == setOf(pick.card)) sameSetAsGreedy++;
    sumChosenP += bel.marg[pick.card][pick.target];
    sumMaxP += bel.marg[buf[gi].card][buf[gi].target];
    // target dimension: among live candidates sharing the chosen card
    int nt = 0, bt = -1; double bp = -1;
    for (int i = 0; i < n; i++) if (buf[i].card == pick.card) { nt++;
      double p = bel.marg[buf[i].card][buf[i].target]; if (p > bp) { bp = p; bt = buf[i].target; } }
    if (nt > 1) { multiTarget++; if (bt == pick.target) targetIsMaxP++; else sameCardDiffTarget++; }
    return pick;
  }
};
int main(int argc, char** argv) {
  int games = argc > 1 ? atoi(argv[1]) : 200;
  Rules r; Game g; Agent* ag[NPLAY];
  std::vector<std::unique_ptr<P5>> own;
  for (int p = 0; p < NPLAY; p++) { own.push_back(std::make_unique<P5>()); ag[p] = own[p].get(); }
  for (int i = 0; i < games; i++) g.run(mixSeed(4242, i), r, ag);
  P5& A = *own[0];
  for (int p = 1; p < NPLAY; p++) { P5& B = *own[p];
    A.dec += B.dec; A.sameAsGreedy += B.sameAsGreedy; A.sameCardDiffTarget += B.sameCardDiffTarget;
    A.targetIsMaxP += B.targetIsMaxP; A.multiTarget += B.multiTarget;
    A.sumChosenP += B.sumChosenP; A.sumMaxP += B.sumMaxP; A.sameSetAsGreedy += B.sameSetAsGreedy; }
  printf("games %d  ask decisions %lld\n", games, A.dec);
  printf("final pick == argmax p (pure greedy): %.2f%%   same half-suit as greedy: %.2f%%\n",
         100.0*A.sameAsGreedy/A.dec, 100.0*A.sameSetAsGreedy/A.dec);
  printf("mean p of chosen ask %.4f vs mean max available p %.4f  (gap %.4f)\n",
         A.sumChosenP/A.dec, A.sumMaxP/A.dec, (A.sumMaxP-A.sumChosenP)/A.dec);
  printf("decisions with >1 live target for the chosen card: %.2f%%; of those the chosen target is the max-p one %.2f%%\n",
         100.0*A.multiTarget/A.dec, 100.0*A.targetIsMaxP/std::max(1LL,A.multiTarget));
  return 0;
}
