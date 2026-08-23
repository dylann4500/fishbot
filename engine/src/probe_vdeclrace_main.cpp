// Adversarial verification of the P6 declaration-race claim.
#include "probe_vdeclrace.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fish; using namespace fish::probe;
static std::string av(int c,char**v,const char*k,const char*d){std::string kk=std::string("--")+k+"=";
  for(int i=1;i<c;i++){std::string a=v[i]; if(a.rfind(kk,0)==0) return a.substr(kk.size());} return d;}
int main(int argc,char**argv){
  int games=atoi(av(argc,argv,"games","3000").c_str());
  uint64_t seed=strtoull(av(argc,argv,"seed","31").c_str(),nullptr,10);
  int mx=atoi(av(argc,argv,"x","0").c_str()), my=atoi(av(argc,argv,"y","0").c_str());
  std::string spec=av(argc,argv,"spec","v04");
  Rules rules;
  ArbMatch st=runArb(spec,games,seed,rules,mx,my,true,true,0);
  const ArbStats&a=st.arb;
  printf("{\"seed\":%llu,\"x\":%d,\"y\":%d,\"deals\":%d,\"games\":%d,\n",
    (unsigned long long)seed,mx,my,st.deals,st.deals*2);
  printf(" \"declRounds\":%lld,\"vDeclTotal\":%lld,\n",a.rounds,a.vDeclTotal);
  printf(" \"races_teamRoundEvents\":%lld,\"races_over_decls\":%.4f,\n",a.races,double(a.races)/double(a.rounds));
  printf(" \"vDeclInRace\":%lld,\"pct_decls_that_were_races\":%.4f,\n",a.vDeclInRace,double(a.vDeclInRace)/double(a.vDeclTotal));
  printf(" \"vDeclInContested\":%lld,\"pct_decls_contested\":%.4f,\n",a.vDeclInContested,double(a.vDeclInContested)/double(a.vDeclTotal));
  printf(" \"vDeclInRaceWrong\":%lld,\"inRaceWrongRate\":%.4f,\n",a.vDeclInRaceWrong,a.vDeclInRace?double(a.vDeclInRaceWrong)/double(a.vDeclInRace):0.0);
  printf(" \"vRace3\":%lld,\n",a.vRace3);
  printf(" \"vRaceDiffDecl\":%lld,\"pct_races_candidates_differ\":%.4f,\n",a.vRaceDiffDecl,double(a.vRaceDiffDecl)/double(a.races));
  printf(" \"vRaceOutcomeDiffers\":%lld,\"pct_races_outcome_differs\":%.4f,\n",a.vRaceOutcomeDiffers,double(a.vRaceOutcomeDiffers)/double(a.races));
  printf(" \"vRaceAllWrong\":%lld,\"pct_races_all_candidates_wrong\":%.4f,\n",a.vRaceAllWrong,double(a.vRaceAllWrong)/double(a.races));
  printf(" \"contested\":%lld,\"pct_races_contested\":%.4f,\n",a.contested,double(a.contested)/double(a.races));
  printf(" \"vContestedDiffSet\":%lld,\n",a.vContestedDiffSet);
  printf(" \"racesDiffSet\":%lld,\"racesDiffConf\":%lld,\n",a.racesDiffSet,a.racesDiffConf);
  printf(" \"bothWrong\":%lld,\"pct_contested_bothWrong\":%.4f,\n",a.bothWrong,double(a.bothWrong)/double(a.contested));
  printf(" \"bothRight\":%lld,\"confOnly\":%lld,\"lowOnly\":%lld,\n",a.bothRight,a.confOnly,a.lowOnly);
  printf(" \"lowRight\":%lld,\"confRight\":%lld,\n",a.lowRight,a.confRight);
  printf(" \"vDeclInContestedWrong\":%lld,\"contestedExecWrongRate\":%.4f,\n",a.vDeclInContestedWrong,a.vDeclInContested?double(a.vDeclInContestedWrong)/double(a.vDeclInContested):0.0);
  printf(" \"vRace2\":%lld,\"vRace2AllWrong\":%lld,\"pct_2way_races_allwrong\":%.4f,\n",a.vRace2,a.vRace2AllWrong,a.vRace2?double(a.vRace2AllWrong)/double(a.vRace2):0.0);
  printf(" \"vContested2\":%lld,\"vContested2BothWrong\":%lld,\"pct_contested2_bothwrong\":%.4f,\n",a.vContested2,a.vContested2BothWrong,a.vContested2?double(a.vContested2BothWrong)/double(a.vContested2):0.0);
  printf(" \"xDeclAcc\":%.5f,\"yDeclAcc\":%.5f,\"xWinRate\":%.5f}\n",
    st.xDecl?double(st.xDeclOk)/st.xDecl:0.0, st.yDecl?double(st.yDeclOk)/st.yDecl:0.0,
    double(st.xWins)/double(st.deals*2));
  return 0;
}
