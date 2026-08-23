// Does "public deduction replay + own hand" reproduce a seat's real Knowledge?
// If it does not, every rollout in v06 is seated at the wrong information set.
#include "factory.hpp"
#include "v06_rollout.hpp"
#include <cstdio>
using namespace fish;
int main(int argc,char**argv){
  int games=argc>1?atoi(argv[1]):40; uint64_t seed=argc>2?strtoull(argv[2],nullptr,10):31;
  Rules rules; std::vector<std::unique_ptr<Agent>> a;
  for(int i=0;i<NPLAY;i++) a.push_back(makeAgent("v05"));
  Agent* ag[NPLAY]; for(int i=0;i<NPLAY;i++) ag[i]=a[i].get();
  long long checks=0, ownerDiff=0, maskDiff=0, maskWider=0, maskNarrower=0, disjDiff=0, capDiff=0, states=0;
  Game g;
  for(int gi=0;gi<games;gi++){
    g.observer=[&](const Game& gg){
      if (gg.g.pub.nEvents % 7) return;
      Knowledge pub; v06::initPublicKnowledge(pub, rules.deckSets);
      for (const Event& e : gg.g.pub.history) pub.onEvent(e);
      for (int j=0;j<NPLAY;j++){
        states++;
        Knowledge kj = pub;
        v06::refineWithHand(kj, j, gg.g.hand[j]);
        const Knowledge& real = gg.agents[j]->k;
        for (int c=0;c<NCARD;c++){
          if (!gg.g.pub.setActive[setOf(c)]) continue;
          checks++;
          if (kj.owner[c] != real.owner[c]) ownerDiff++;
          if (kj.mask[c] != real.mask[c]) { maskDiff++;
            if ((kj.mask[c] | real.mask[c]) == kj.mask[c]) maskWider++; else maskNarrower++; }
        }
        if (kj.disj.size() != real.disj.size()) disjDiff++;
        uint8_t q1[NPLAY], q2[NPLAY]; kj.capacities(q1); real.capacities(q2);
        for (int p=0;p<NPLAY;p++) if (q1[p]!=q2[p]) { capDiff++; break; }
      }
    };
    g.run(mixSeed(seed,uint64_t(gi)*2654435761ull+1), rules, ag);
  }
  g.observer=nullptr;
  printf("states %lld  card checks %lld\n", states, checks);
  printf("owner mismatches   %lld (%.4f%%)\n", ownerDiff, 100.0*ownerDiff/checks);
  printf("mask  mismatches   %lld (%.4f%%)   reconstruction WIDER %lld  NARROWER %lld\n",
         maskDiff, 100.0*maskDiff/checks, maskWider, maskNarrower);
  printf("disj size differs  %lld states (%.2f%%)\n", disjDiff, 100.0*disjDiff/states);
  printf("capacity differs   %lld states (%.2f%%)\n", capDiff, 100.0*capDiff/states);
  return 0;
}
