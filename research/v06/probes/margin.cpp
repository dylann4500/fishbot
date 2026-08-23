// v0.6 probe 4: how DECIDED is each v0.5 ask decision?
// At every v0.5 ask decision in self-play, recompute the full candidate score
// vector the policy itself uses (linear score * linearWeight + valueWeight *
// one-ply expectimax) and report the structure of the choice.
#include "factory.hpp"
#include "game.hpp"
#include <cstdio>
#include <vector>
#include <algorithm>
using namespace fish;

int main(int argc,char**argv){
  int games = argc>1?atoi(argv[1]):200;
  uint64_t seed = argc>2?strtoull(argv[2],nullptr,10):31;
  const char* spec = argc>3?argv[3]:"v05";
  Rules rules;
  std::vector<std::unique_ptr<Agent>> a;
  for(int i=0;i<NPLAY;i++) a.push_back(makeAgent(spec));
  Agent* ag[NPLAY]; for(int i=0;i<NPLAY;i++) ag[i]=a[i].get();

  long long nDec=0, nForced=0, nCert=0;
  std::vector<double> gapAbs, gapRel, spread;
  std::vector<int> nCand, nWithin1, nWithin5, nWithin10;
  long long topIsMaxP=0;
  std::vector<double> pOfTop, pOfMax;

  Game g;
  g.observer=[&](const Game& gg){
    if (gg.g.pub.history.empty()) return;
    const Event& e = gg.g.pub.history.back();
    if (e.kind != Kind::Ask) return;
    // reconstruct the decision the actor faced: state BEFORE this ask is not
    // available here, so instead we instrument the NEXT decision point below.
    (void)e;
  };
  g.observer=nullptr;

  // Instrument directly: wrap a V05Agent and score every decision.
  for(int gi=0; gi<games; gi++){
    uint64_t s=mixSeed(seed, uint64_t(gi)*2654435761ull+1);
    // Re-run with an observer that scores the CURRENT turn-holder's options.
    g.observer=[&](const Game& gg){
      int who = gg.g.pub.turn;
      if (!gg.g.pub.handCount[who]) return;
      if (!gg.g.pub.activeSets()) return;
      auto* v = dynamic_cast<V05Agent*>(gg.agents[who]);
      if (!v) return;
      v->refresh();
      AskMove buf[NSET*SETSZ*3];
      int n = v->cfg.liveAskGate ? v->enumerateLive(gg.g.pub, buf)
                                 : enumerateAsks(gg.g.pub, v->k.myHand, who, buf);
      if (n<=0) return;
      nDec++;
      if (n==1){ nForced++; return; }
      if (v->cfg.useValue) v->computeAggregates(gg.g.pub);
      v->prepareRunway(gg.g.pub);
      std::vector<double> u(n); std::vector<double> pp(n);
      double f[NFEAT];
      for(int i=0;i<n;i++){
        v->features(gg.g.pub, buf[i].card, buf[i].target, f);
        double x=0; for(int j=0;j<NFEAT;j++) x += v->cfg.w[j]*f[j];
        x *= v->cfg.linearWeight;
        if (v->cfg.useValue) x += v->cfg.valueWeight * v->askExpectedValue(gg.g.pub, buf[i].card, buf[i].target, f[0]);
        u[i]=x; pp[i]=f[0];
      }
      std::vector<int> ord(n); for(int i=0;i<n;i++) ord[i]=i;
      std::sort(ord.begin(),ord.end(),[&](int x,int y){return u[x]>u[y];});
      double top=u[ord[0]], second=u[ord[1]];
      double lo=u[ord[n-1]];
      double sp = top-lo;
      gapAbs.push_back(top-second);
      gapRel.push_back(sp>1e-12 ? (top-second)/sp : 0.0);
      spread.push_back(sp);
      int w1=0,w5=0,w10=0;
      for(int i=0;i<n;i++){ double d=(top-u[i]); if (sp>1e-12){ double rr=d/sp; if(rr<=0.01)w1++; if(rr<=0.05)w5++; if(rr<=0.10)w10++; } }
      nCand.push_back(n); nWithin1.push_back(w1); nWithin5.push_back(w5); nWithin10.push_back(w10);
      int argmaxP=0; for(int i=1;i<n;i++) if(pp[i]>pp[argmaxP]) argmaxP=i;
      if (ord[0]==argmaxP) topIsMaxP++;
      pOfTop.push_back(pp[ord[0]]); pOfMax.push_back(pp[argmaxP]);
      if (pp[ord[0]] > 0.9995) nCert++;
    };
    g.run(s, rules, ag);
  }
  g.observer=nullptr;

  auto pct=[&](std::vector<double> v,double q){ if(v.empty())return 0.0; std::sort(v.begin(),v.end()); return v[std::min(v.size()-1,size_t(q*v.size()))]; };
  auto meani=[&](const std::vector<int>&v){ double s=0; for(int x:v)s+=x; return v.empty()?0.0:s/v.size(); };
  auto meand=[&](const std::vector<double>&v){ double s=0; for(double x:v)s+=x; return v.empty()?0.0:s/v.size(); };
  printf("games=%d  ask decisions=%lld\n", games, nDec);
  printf("forced (exactly one live ask) : %.2f%%\n", 100.0*nForced/std::max(1LL,nDec));
  printf("chosen ask is a certain hit   : %.2f%% of contested decisions\n", 100.0*nCert/std::max<size_t>(1,gapAbs.size()));
  printf("mean live candidates          : %.1f\n", meani(nCand));
  printf("candidates within 1%%/5%%/10%% of the score range of the top: %.2f / %.2f / %.2f\n",
         meani(nWithin1), meani(nWithin5), meani(nWithin10));
  printf("top1-top2 gap as a fraction of the full score range: p10=%.3f  p25=%.3f  median=%.3f  p75=%.3f\n",
         pct(gapRel,0.10), pct(gapRel,0.25), pct(gapRel,0.50), pct(gapRel,0.75));
  printf("fraction of decisions where gap < 5%% of range: %.2f%%\n",
         100.0*std::count_if(gapRel.begin(),gapRel.end(),[](double x){return x<0.05;})/std::max<size_t>(1,gapRel.size()));
  printf("EXACT ties at the top (gap < 1e-9)     : %.2f%%\n",
         100.0*std::count_if(gapAbs.begin(),gapAbs.end(),[](double x){return x<1e-9;})/std::max<size_t>(1,gapAbs.size()));
  printf("degenerate score range (spread < 1e-9): %.2f%%\n",
         100.0*std::count_if(spread.begin(),spread.end(),[](double x){return x<1e-9;})/std::max<size_t>(1,spread.size()));
  printf("median score spread across candidates : %.4f ; median top1-top2 gap %.6f\n", pct(spread,0.5), pct(gapAbs,0.5));
  printf("policy's pick is the max-P(hit) ask: %.2f%%\n", 100.0*topIsMaxP/std::max<size_t>(1,gapAbs.size()));
  printf("mean P(hit) of pick = %.4f ; of the max-P ask = %.4f  (gap %.4f)\n",
         meand(pOfTop), meand(pOfMax), meand(pOfMax)-meand(pOfTop));
  return 0;
}
