// v0.6 probe 2b: TRUE consistent-deal count over the course of a game.
// DealDP::build sets N=1 as a sentinel when `uniform`, so the count must be
// recomputed as a multinomial there.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>
using namespace fish;

static double log10Deals(const Knowledge& k, bool& okOut) {
  DealDP dp;
  okOut = false;
  // recompute the pieces ourselves so we can handle the uniform shortcut
  int Q = 0; int card[NCARD]; uint8_t cmask[NCARD];
  uint8_t andM = 0x3F, orM = 0;
  uint64_t u = k.unresolved;
  while (u) { int c = __builtin_ctzll(u); u &= u - 1; card[Q]=c; cmask[Q]=k.mask[c]; andM&=cmask[Q]; orM|=cmask[Q]; Q++; }
  uint8_t cap[NPLAY]; k.capacities(cap);
  int sum=0; for (int p=0;p<NPLAY;p++) sum+=cap[p];
  if (sum != Q) return -1;
  okOut = true;
  if (!Q) return 0.0;
  if (andM == orM) {                     // uniform: multinomial
    for (int p=0;p<NPLAY;p++) if (cap[p] && !(andM & (1u<<p))) return -1;
    double lg = std::lgamma(Q+1.0);
    for (int p=0;p<NPLAY;p++) lg -= std::lgamma(cap[p]+1.0);
    return lg / std::log(10.0);
  }
  if (!dp.build(k)) { okOut=false; return -1; }
  return std::log10(dp.N);
}

int main(int argc, char** argv) {
  int games = argc>1?atoi(argv[1]):200;
  uint64_t seed = argc>2?strtoull(argv[2],nullptr,10):31;
  const char* spec = argc>3?argv[3]:"v05";
  Rules rules;
  std::vector<std::unique_ptr<Agent>> ags;
  for (int i=0;i<NPLAY;i++) ags.push_back(makeAgent(spec));
  Agent* ag[NPLAY]; for (int i=0;i<NPLAY;i++) ag[i]=ags[i].get();

  const double THR[5] = {9, 7, 5, 3, 1};   // log10 thresholds
  std::vector<int> crossEv[5]; std::vector<double> setsAfter[5]; std::vector<int> activeAt[5];
  std::vector<int> totalEv;
  // also: per-seat log10 at fixed event marks
  std::vector<double> atEv[9]; const int MARK[9]={0,16,32,48,64,80,96,112,128};

  Game game;
  for (int gi=0; gi<games; gi++) {
    uint64_t s = mixSeed(seed, uint64_t(gi)*2654435761ull+1);
    int crossed[5]; int declBefore[5]; int act[5];
    for (int t=0;t<5;t++){crossed[t]=-1;declBefore[t]=0;act[t]=0;}
    game.observer = [&](const Game& gg){
      int who = gg.g.pub.turn;
      bool ok; double L = log10Deals(gg.agents[who]->k, ok);
      if (!ok) return;
      int scored = gg.g.pub.score[0]+gg.g.pub.score[1];
      for (int t=0;t<5;t++) if (crossed[t]<0 && L < THR[t]) {
        crossed[t]=gg.g.pub.nEvents; declBefore[t]=scored; act[t]=gg.g.pub.activeSets();
      }
      for (int m=0;m<9;m++) if (gg.g.pub.nEvents==MARK[m]) atEv[m].push_back(L);
    };
    GameResult r = game.run(s, rules, ag);
    totalEv.push_back(r.events);
    int tot = r.score[0]+r.score[1];
    for (int t=0;t<5;t++) if (crossed[t]>=0) {
      crossEv[t].push_back(crossed[t]);
      activeAt[t].push_back(act[t]);
      setsAfter[t].push_back(double(tot-declBefore[t])/std::max(1,tot));
    }
  }
  game.observer=nullptr;
  auto med=[](std::vector<int> v){ if(v.empty())return -1; std::sort(v.begin(),v.end()); return v[v.size()/2]; };
  auto medd=[](std::vector<double> v){ if(v.empty())return 0.0; std::sort(v.begin(),v.end()); return v[v.size()/2]; };
  auto meand=[](const std::vector<double>&v){ if(v.empty())return 0.0; double s=0; for(double x:v)s+=x; return s/v.size(); };
  printf("policy=%s games=%d median events/game=%d\n\n", spec, games, med(totalEv));
  printf("consistent deals (log10) seen by the turn-holder, by event index\n");
  printf("%-8s %8s %10s %10s %10s\n","event","n","p10","median","p90");
  for (int m=0;m<9;m++){ auto v=atEv[m]; if(v.empty())continue; std::sort(v.begin(),v.end());
    printf("%-8d %8zu %10.2f %10.2f %10.2f\n",MARK[m],v.size(),v[v.size()/10],v[v.size()/2],v[v.size()*9/10]); }
  printf("\nfirst event at which the turn-holder's consistent-deal count drops below 10^X\n");
  printf("%-10s %9s %10s %14s %18s\n","10^X","reached%","median ev","median active","mean %sets after");
  for (int t=0;t<5;t++)
    printf("%-10.0f %8.1f%% %10d %14d %17.1f%%\n",THR[t],100.0*crossEv[t].size()/games,
           med(crossEv[t]), med(activeAt[t]), 100.0*meand(setsAfter[t]));
  (void)medd;
  return 0;
}
