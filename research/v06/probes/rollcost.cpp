// v0.6 probe 3: end-to-end cost of ONE determinized rollout from a real
// mid-game decision point, including reconstructing all six seats' knowledge.
#include "factory.hpp"
#include "game.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace fish;
using clk = std::chrono::steady_clock;
static double sec(clk::time_point a, clk::time_point b){return std::chrono::duration<double>(b-a).count();}

// Reconstruct seat j's information state from the PUBLIC deduction state plus
// the hand j holds in the determinization.  The public state is the common
// deduction with no seat's own hand folded in; refining it with j's hand yields
// exactly j's knowledge (belief.hpp Knowledge::onEvent never reads `me` except
// to maintain myHand).
static void publicKnowledge(Knowledge& k, int deckSets) {
  k = Knowledge{};
  k.me = NPLAY;                       // out of range for every seat
  k.myHand = 0;
  for (int c = 0; c < NCARD; c++) {
    if (setOf(c) >= deckSets) { k.owner[c] = OUT_OF_PLAY; k.mask[c] = 0; continue; }
    k.owner[c] = UNKNOWN; k.mask[c] = 0x3F;
  }
  k.unresolved = 0;
  for (int c = 0; c < NCARD; c++) if (setOf(c) < deckSets) k.unresolved |= bit(c);
  for (int p = 0; p < NPLAY; p++) k.handCount[p] = uint8_t(deckSets * SETSZ / NPLAY);
  k.publicKnown = 0;
}

static void refineWithHand(Knowledge& k, int j, uint64_t hand) {
  k.me = uint8_t(j);
  k.myHand = hand;
  for (int c = 0; c < NCARD; c++) {
    if (k.owner[c] == OUT_OF_PLAY) continue;
    if (hand & bit(c)) { if (k.owner[c] != j) k.setOwner(c, j); }
    else if (k.owner[c] == UNKNOWN && (k.mask[c] & (1u << j))) k.exclude(c, j);
  }
  k.propagateCapacity();
}

int main(int argc, char** argv) {
  int games = argc>1?atoi(argv[1]):10;
  uint64_t seed = argc>2?strtoull(argv[2],nullptr,10):31;
  const char* rollSpec = argc>3?argv[3]:"v05:belief=indep,value=0,topk=0";
  Rules rules;
  std::vector<std::unique_ptr<Agent>> ags;
  for (int i=0;i<NPLAY;i++) ags.push_back(makeAgent("v05"));
  Agent* ag[NPLAY]; for (int i=0;i<NPLAY;i++) ag[i]=ags[i].get();

  // rollout agent pool
  std::vector<std::unique_ptr<Agent>> rl;
  for (int i=0;i<NPLAY;i++) rl.push_back(makeAgent(rollSpec));

  double tReplay=0, tRollout=0, tSample=0, tBuild=0;
  long long nReplay=0, nRollout=0, nEvents=0;
  Game game;

  for (int gi=0; gi<games; gi++) {
    uint64_t s = mixSeed(seed, uint64_t(gi)*2654435761ull+1);
    game.observer = [&](const Game& gg){
      int ev = gg.g.pub.nEvents;
      if (ev < 24 || ev % 16 != 0) return;
      int who = gg.g.pub.turn;
      if (!gg.g.pub.handCount[who]) return;
      const Knowledge& kk = gg.agents[who]->k;
      auto t0=clk::now();
      DealDP dp; if(!dp.build(kk)) return;
      auto t1=clk::now(); tBuild += sec(t0,t1);
      Rng r(999+ev);
      uint8_t owners[NCARD];
      // --- one determinization ------------------------------------------
      auto t2=clk::now();
      dp.sample(r, owners);
      auto t3=clk::now(); tSample += sec(t2,t3);
      uint64_t hand[NPLAY]={0,0,0,0,0,0};
      for (int c=0;c<NCARD;c++){
        if (!gg.g.pub.setActive[setOf(c)]) continue;
        int o = kk.owner[c] < NPLAY ? kk.owner[c] : (kk.owner[c]==OUT_OF_PLAY? -1 : owners[c]);
        if (o>=0) hand[o] |= bit(c);
      }
      // --- reconstruct all six information states ------------------------
      auto t4=clk::now();
      Knowledge base; publicKnowledge(base, rules.deckSets);
      Knowledge pk = base;
      for (const auto& e : gg.g.pub.history) pk.onEvent(e);
      for (int j=0;j<NPLAY;j++){ Knowledge kj = pk; refineWithHand(kj, j, hand[j]); rl[j]->k = kj; rl[j]->seat = j; }
      auto t5=clk::now(); tReplay += sec(t4,t5); nReplay++;
      // --- roll the rest of the game out ---------------------------------
      auto t6=clk::now();
      GameState gs = gg.g;
      for (int p=0;p<NPLAY;p++){ gs.hand[p]=hand[p]; gs.pub.handCount[p]=uint8_t(popcount64(hand[p])); }
      gs.pub.history.clear();
      Game sim; sim.rules = rules; sim.g = gs; sim.res = GameResult{};
      for (int i=0;i<NSET;i++) sim.lockedAt[i] = -1;
      Agent* rg[NPLAY]; for (int j=0;j<NPLAY;j++) rg[j]=rl[j].get();
      for (int j=0;j<NPLAY;j++) sim.agents[j]=rg[j];
      // continue the main loop by hand (mirrors Game::run without setup())
      int asks=0;
      int startEv = sim.g.pub.nEvents;
      while (true) {
        sim.declarationRound();
        if (!sim.g.pub.activeSets()) break;
        bool a0=sim.g.pub.teamAlive(0), a1=sim.g.pub.teamAlive(1);
        if (!a0||!a1){ sim.forcedEndgame(a0?0:1); break; }
        if (!sim.g.pub.handCount[sim.g.turn]) {
          int cand[3],n=0;
          for (int p=teamOf(sim.g.turn); p<NPLAY; p+=2) if (sim.g.pub.handCount[p]) cand[n++]=p;
          if(!n){ sim.forcedEndgame(1-teamOf(sim.g.turn)); break; }
          int rcv = sim.agents[sim.g.turn]->choosePassTarget(sim.g.pub,cand,n);
          bool ok=false; for(int i2=0;i2<n;i2++) if(cand[i2]==rcv) ok=true;
          if(!ok) rcv=cand[0];
          Event e{}; e.kind=Kind::Pass; e.actor=uint8_t(sim.g.turn); e.target=uint8_t(rcv);
          sim.emit(e); sim.g.turn=rcv; sim.g.pub.turn=rcv; continue;
        }
        AskMove mv = sim.agents[sim.g.turn]->chooseAsk(sim.g.pub);
        if (!legalAsk(sim.g, sim.g.turn, mv.card, mv.target)) {
          AskMove b[NSET*SETSZ*3]; int n2=enumerateAsks(sim.g.pub, sim.g.hand[sim.g.turn], sim.g.turn, b);
          if(!n2){ sim.res.hitLimit=true; break; }
          mv=b[0];
        }
        int actor=sim.g.turn, tgt=mv.target, card=mv.card;
        bool succ = (sim.g.hand[tgt]&bit(card))!=0;
        if (succ){ sim.g.hand[tgt]&=~bit(card); sim.g.hand[actor]|=bit(card); }
        sim.g.pub.handCount[actor]=uint8_t(popcount64(sim.g.hand[actor]));
        sim.g.pub.handCount[tgt]=uint8_t(popcount64(sim.g.hand[tgt]));
        Event e{}; e.kind=Kind::Ask; e.actor=uint8_t(actor); e.target=uint8_t(tgt);
        e.card=uint8_t(card); e.set=uint8_t(setOf(card)); e.success=succ;
        sim.emit(e);
        if(!succ){ sim.g.turn=tgt; sim.g.pub.turn=tgt; }
        if (++asks >= 200) { sim.res.hitLimit=true; sim.adjudicateRemaining(); break; }
      }
      auto t7=clk::now(); tRollout += sec(t6,t7); nRollout++;
      nEvents += sim.g.pub.nEvents - startEv;
    };
    game.run(s, rules, ag);
  }
  game.observer=nullptr;
  printf("decision points sampled : %lld\n", nRollout);
  printf("DealDP build            : %8.1f us\n", tBuild/std::max(1LL,nRollout)*1e6);
  printf("one deal sample         : %8.2f us\n", tSample/std::max(1LL,nRollout)*1e6);
  printf("6-seat state reconstruct: %8.1f us   (full history replay)\n", tReplay/std::max(1LL,nReplay)*1e6);
  printf("one rollout to game end : %8.1f us   (%.1f events)\n",
         tRollout/std::max(1LL,nRollout)*1e6, double(nEvents)/std::max(1LL,nRollout));
  printf("\nrollout policy = %s\n", rollSpec);
  return 0;
}
