#include "factory.hpp"
#include "game.hpp"
#include <chrono>
#include <cstdio>
using namespace fish;
using clk=std::chrono::steady_clock;
int main(int argc,char**argv){
  int N = argc>1?atoi(argv[1]):200;
  Rules rules; Game g;
  const char* specs[] = {
    "v05","v05:topk=0","v05:topk=0,value=0","v05:belief=indep","v05:belief=indep,topk=0",
    "v05:belief=indep,value=0,topk=0","v04","v03","v02","detective","lockout","hunter"
  };
  printf("%-34s %10s %10s %12s %10s\n","spec","games/s","ev/game","us/event","setsA");
  for (auto sp : specs) {
    std::vector<std::unique_ptr<Agent>> a;
    for(int i=0;i<NPLAY;i++) a.push_back(makeAgent(sp));
    Agent* ag[NPLAY]; for(int i=0;i<NPLAY;i++) ag[i]=a[i].get();
    auto t0=clk::now(); long long ev=0; long long sc=0;
    for(int i=0;i<N;i++){ GameResult r=g.run(mixSeed(4242,i*7919+3),rules,ag); ev+=r.events; sc+=r.score[0]; }
    double s=std::chrono::duration<double>(clk::now()-t0).count();
    printf("%-34s %10.1f %10.1f %12.2f %10.2f\n", sp, N/s, double(ev)/N, s/double(ev)*1e6, double(sc)/N);
  }
  return 0;
}
