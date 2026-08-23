// Adversarial verification of the P5 "turn routing" claim.
// Re-derives the (C) histogram independently, and adds the conditioning the
// original probe omits: how many opponents were actually LIVE at the decision.
// A "choice of recipient" among k>=2 reachable opponents is only interesting
// relative to how many recipients existed at all.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include <thread>

namespace fish {

struct RouteVerify {
  long long games=0, decisions=0;
  // [liveOpponents][reachableByDeadAsk]
  long long hist[4][4] = {};
  long long chosenDead=0;
  // among decisions with >=2 reachable, was the chosen ask dead?
  long long choice2=0, choice2Dead=0;
  // "full routing": every live opponent reachable AND >=2 live opponents
  long long fullRoute=0;
  // control: how often the actor could reach >=2 opponents with a dead ask
  // in a half-suit it had ALREADY publicly asked in (no new half-suit leaked)
  long long quietRoute2=0;
  void merge(const RouteVerify& o){
    games+=o.games; decisions+=o.decisions; chosenDead+=o.chosenDead;
    choice2+=o.choice2; choice2Dead+=o.choice2Dead; fullRoute+=o.fullRoute;
    quietRoute2+=o.quietRoute2;
    for(int a=0;a<4;a++) for(int b=0;b<4;b++) hist[a][b]+=o.hist[a][b];
  }
};

inline bool vrDead(const Knowledge& kk,int c,int t){
  return (kk.owner[c]<NPLAY)?(kk.owner[c]!=t):!(kk.mask[c]&(1u<<t));
}

inline void vrAnalyse(const std::vector<Event>& ev,const uint64_t dealt[NPLAY],
                      const Rules& rules,RouteVerify& st){
  Knowledge k[NPLAY];
  for(int p=0;p<NPLAY;p++) k[p].init(p,dealt[p],rules.deckSets);
  PublicState pub{}; pub.rules=rules;
  for(int p=0;p<NPLAY;p++) pub.handCount[p]=uint8_t(popcount64(dealt[p]));
  for(int s=0;s<NSET;s++) pub.setActive[s]=(s<rules.deckSets);
  st.games++;
  for(const Event& e:ev){
    if(e.kind==Kind::Ask){
      const Knowledge& kk=k[e.actor];
      int live=0;
      for(int t=0;t<NPLAY;t++) if(teamOf(t)!=teamOf(e.actor)&&pub.handCount[t]) live++;
      AskMove buf[NSET*SETSZ*3];
      int n=enumerateAsks(pub,kk.myHand,e.actor,buf);
      st.decisions++;
      bool reach[NPLAY]={false,false,false,false,false,false};
      bool quiet[NPLAY]={false,false,false,false,false,false};
      for(int i=0;i<n;i++) if(vrDead(kk,buf[i].card,buf[i].target)){
        reach[buf[i].target]=true;
        if(kk.askCount[e.actor][setOf(buf[i].card)]>0) quiet[buf[i].target]=true;
      }
      int nr=0,nq=0;
      for(int t=0;t<NPLAY;t++){ if(reach[t]) nr++; if(quiet[t]) nq++; }
      st.hist[std::min(3,live)][std::min(3,nr)]++;
      if(nq>=2) st.quietRoute2++;
      if(live>=2&&nr==live) st.fullRoute++;
      bool cd=vrDead(kk,e.card,e.target);
      if(cd) st.chosenDead++;
      if(nr>=2){ st.choice2++; if(cd) st.choice2Dead++; }
    }
    for(int p=0;p<NPLAY;p++) k[p].onEvent(e);
    if(e.kind==Kind::Declare||e.kind==Kind::ForcedDeclare) pub.setActive[e.set]=false;
    for(int p=0;p<NPLAY;p++) pub.handCount[p]=e.handCount[p];
  }
}

struct RVConfig{ std::string specA="v04",specB="v04"; int games=600,rotations=2;
                 uint64_t seed=31; Rules rules; int threads=0; };

inline RouteVerify runRouteVerify(const RVConfig& pc){
  int nT=pc.threads>0?pc.threads:int(std::thread::hardware_concurrency());
  if(nT<1) nT=1; nT=std::min(nT,std::max(1,pc.games));
  std::vector<RouteVerify> local(nT);
  std::vector<std::thread> pool;
  for(int t=0;t<nT;t++) pool.emplace_back([&,t](){
    std::unique_ptr<Agent> A[3],B[3];
    for(int i=0;i<3;i++){A[i]=makeAgent(pc.specA);B[i]=makeAgent(pc.specB);}
    Game game; game.trace.on=true;
    for(int i=t;i<pc.games;i+=nT){
      uint64_t s=mixSeed(pc.seed,uint64_t(i)*2654435761ull+1);
      for(int rot=0;rot<pc.rotations;rot++){
        int orient=(pc.rotations==2)?rot:(rot/3);
        int shift=(pc.rotations==2)?0:(rot%3);
        Agent* ag[NPLAY];
        for(int p=0;p<NPLAY;p++) ag[p]=(teamOf(p)==orient)?A[p/2].get():B[p/2].get();
        game.rotation=shift; game.trace.events.clear();
        game.run(s,pc.rules,ag);
        vrAnalyse(game.trace.events,game.g.dealt,pc.rules,local[t]);
      }
    }
  });
  for(auto& th:pool) th.join();
  RouteVerify st; for(auto& l:local) st.merge(l); return st;
}

inline void printRouteVerify(const RouteVerify& s,std::ostream& o){
  auto pct=[&](long long a,long long b){return b?100.0*double(a)/double(b):0.0;};
  o<<"games "<<s.games<<"  ask decisions "<<s.decisions<<"\n";
  o<<"liveOpp x reachable-by-dead-ask (row %, and % of all decisions)\n";
  for(int L=0;L<4;L++){
    long long row=0; for(int r=0;r<4;r++) row+=s.hist[L][r];
    if(!row) continue;
    o<<"  live="<<L<<"  n="<<row<<" ("<<pct(row,s.decisions)<<"% of decisions):";
    for(int r=0;r<4;r++) o<<"  r"<<r<<"="<<pct(s.hist[L][r],row)<<"%";
    o<<"\n";
  }
  long long ge2=0;
  for(int L=0;L<4;L++) for(int r=2;r<4;r++) ge2+=s.hist[L][r];
  o<<">=2 reachable (all decisions)          "<<ge2<<" ("<<pct(ge2,s.decisions)<<"%)\n";
  long long ge2live3=s.hist[3][2]+s.hist[3][3];
  o<<">=2 reachable AND 3 live opponents     "<<ge2live3<<" ("<<pct(ge2live3,s.decisions)<<"%)\n";
  o<<"all live opponents reachable, live>=2  "<<s.fullRoute<<" ("<<pct(s.fullRoute,s.decisions)<<"%)\n";
  o<<">=2 reachable WITHOUT opening a new half-suit "<<s.quietRoute2
   <<" ("<<pct(s.quietRoute2,s.decisions)<<"%)\n";
  o<<"chosen ask provably dead               "<<s.chosenDead<<" ("<<pct(s.chosenDead,s.decisions)<<"%)\n";
  o<<"  when >=2 reachable, chosen ask dead  "<<s.choice2Dead<<" ("<<pct(s.choice2Dead,s.choice2)<<"% of those)\n";
}

} // namespace fish
