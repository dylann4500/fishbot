// AUDIT PROBE (recon only, sandbox copy): does M4's under-approximation get used
// as EVIDENCE OF THE TARGET'S IGNORANCE anywhere that matters?
//
// Two directions are measured against ground truth (each opponent's own real
// Knowledge object, which is the thing model(t) is meant to under-approximate):
//
//   (1) HARD LOCKOUT CERTIFICATE.  model(t).owner[c] == u for u on the actor's
//       team is claimed in v05_target.hpp:73-76 to be a proof that t knows.
//       Soundness = never fires when the truth does not.  False negative =
//       truth knows and the model does not; the graded term degrades to a
//       smaller penalty there, so the ask looks safer than it is.
//
//   (2) postMissLockout's `if (nSurv > 1) return 0.0;` (v05_target.hpp:386).
//       nSurv counted in model(t) is an OVER-count of nSurv in t's real
//       knowledge, so `> 1` in the model can hide `== 1` in truth: the ask
//       really does hand t the card, and the term prices it at zero.
#include "fish.hpp"
#include "belief.hpp"
#include "game.hpp"
#include "factory.hpp"
#include "v05_target.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>

using namespace fish;

struct S {
  long long certChecks = 0, certModel = 0, certTruth = 0, certBoth = 0, certUnsound = 0;
  // decision-level: at how many (observer, opponent) pairs does the TRUTH carry a
  // hard certificate that the model misses entirely?
  long long pairChecks = 0, pairTruthCert = 0, pairModelCert = 0, pairMissed = 0;
  // postMiss
  long long pmCands = 0;
  long long pmModel1_truth1 = 0;   // correct fire
  long long pmModel1_truthV = 0;   // model fires, truth already knew (vacuous): over-fire
  long long pmModelN_truth1 = 0;   // SUPPRESSED REAL LEAK  <-- ignorance used as evidence
  long long pmModelN_truthN = 0;
  long long pmModel1_truth0 = 0;   // should be impossible
  long long pmVacuousModel  = 0;   // model saw it as vacuous
};

int main(int argc, char** argv) {
  int games = 20; uint64_t seed = 31; std::string aSpec = "v05", bSpec = "v05";
  for (int i = 1; i < argc; i++) {
    std::string s = argv[i];
    auto val = [&](const char* k) { return s.rfind(k, 0) == 0 ? s.substr(strlen(k)) : std::string(); };
    if (!val("--games=").empty()) games = atoi(val("--games=").c_str());
    else if (!val("--seed=").empty()) seed = strtoull(val("--seed=").c_str(), nullptr, 10);
    else if (!val("--a=").empty()) aSpec = val("--a=");
    else if (!val("--b=").empty()) bSpec = val("--b=");
  }
  Rules rules; S st;

  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ptr[NPLAY];
    for (int p = 0; p < NPLAY; p++) { ag[p] = makeAgent(teamOf(p) == 0 ? aSpec : bSpec); ptr[p] = ag[p].get(); }
    Game gm;
    m45::SeatModels sm[NPLAY];
    for (int i = 0; i < NPLAY; i++) sm[i].reset(rules.deckSets);
    size_t seen = 0;
    gm.observer = [&](const Game& G) {
      const auto& hist = G.g.pub.history;
      while (seen < hist.size()) { const Event& e = hist[seen++];
        for (int i = 0; i < NPLAY; i++) sm[i].observe(e); }

      for (int i = 0; i < NPLAY; i++) {
        int myTeam = 0; for (int p = 0; p < NPLAY; p++) if (teamOf(p) == teamOf(i)) myTeam |= 1 << p;
        for (int t = 0; t < NPLAY; t++) {
          if (teamOf(t) == teamOf(i)) continue;
          if (!G.g.pub.handCount[t]) continue;
          const Knowledge& mdl = sm[i].of(t, G.agents[i]->k);
          const Knowledge& tru = G.agents[t]->k;
          st.pairChecks++;
          bool mCert = false, tCert = false;
          for (int c = 0; c < NCARD; c++) {
            if (!G.g.pub.setActive[setOf(c)]) continue;
            st.certChecks++;
            bool mk = (mdl.owner[c] < NPLAY) && (myTeam & (1u << mdl.owner[c]));
            bool tk = (tru.owner[c] < NPLAY) && (myTeam & (1u << tru.owner[c]));
            if (mk) st.certModel++;
            if (tk) st.certTruth++;
            if (mk && tk) st.certBoth++;
            if (mk && !tk) st.certUnsound++;
            mCert |= mk; tCert |= tk;
          }
          if (mCert) st.pairModelCert++;
          if (tCert) st.pairTruthCert++;
          if (tCert && !mCert) st.pairMissed++;

          // ---- postMissLockout survivor count, model vs truth ----
          for (int S_ = 0; S_ < NSET; S_++) {
            if (!G.g.pub.setActive[S_]) continue;
            if (!(G.g.hand[i] & setMask(S_))) continue;      // i could not legally ask in S
            for (int q = 0; q < SETSZ; q++) {
              int card = cardOf(S_, q);
              if (G.g.hand[i] & bit(card)) continue;          // cannot ask for a card you hold
              st.pmCands++;
              auto count = [&](const Knowledge& kk, int& nS, bool& vac) {
                nS = 0; vac = false;
                for (int z = 0; z < SETSZ; z++) { int d = cardOf(S_, z);
                  if (d == card) continue;
                  if (kk.owner[d] == uint8_t(i)) { vac = true; return; }
                  if (kk.owner[d] != UNKNOWN) continue;
                  if (!(kk.mask[d] & (1u << i))) continue;
                  nS++; }
              };
              int nM, nT; bool vM, vT;
              count(mdl, nM, vM); count(tru, nT, vT);
              if (vM) { st.pmVacuousModel++; continue; }
              if (nM == 1) {
                if (vT) st.pmModel1_truthV++;
                else if (nT == 1) st.pmModel1_truth1++;
                else st.pmModel1_truth0++;
              } else {
                if (!vT && nT == 1) st.pmModelN_truth1++;
                else st.pmModelN_truthN++;
              }
            }
          }
        }
      }
    };
    gm.run(mixSeed(seed, gi), rules, ptr);
  }

  printf("games=%d seed=%llu a=%s b=%s\n", games, (unsigned long long)seed, aSpec.c_str(), bSpec.c_str());
  printf("\n(1) HARD BLACKBALL CERTIFICATE, model(t) vs seat t's real knowledge\n");
  printf("  (obs,opp,card) checks              %lld\n", st.certChecks);
  printf("  model resolves to actor's team     %lld  (%.3f%%)\n", st.certModel, 100.0*st.certModel/std::max(1LL,st.certChecks));
  printf("  TRUTH resolves to actor's team     %lld  (%.3f%%)\n", st.certTruth, 100.0*st.certTruth/std::max(1LL,st.certChecks));
  printf("  model fires and truth does NOT     %lld   <-- must be 0 (soundness)\n", st.certUnsound);
  printf("  truth fires and model does not     %lld  (%.3f%% of truth fires)\n",
         st.certTruth - st.certBoth, 100.0*(st.certTruth-st.certBoth)/std::max(1LL,st.certTruth));
  printf("  (obs,opp) pairs                    %lld\n", st.pairChecks);
  printf("    truth has some certificate       %lld  (%.2f%%)\n", st.pairTruthCert, 100.0*st.pairTruthCert/std::max(1LL,st.pairChecks));
  printf("    model has some certificate       %lld  (%.2f%%)\n", st.pairModelCert, 100.0*st.pairModelCert/std::max(1LL,st.pairChecks));
  printf("    truth yes / model no             %lld  (%.2f%% of pairs)\n", st.pairMissed, 100.0*st.pairMissed/std::max(1LL,st.pairChecks));
  printf("\n(2) postMissLockout survivor count (v05_target.hpp:379-388)\n");
  printf("  candidate (obs,opp,card)           %lld\n", st.pmCands);
  printf("  model vacuous (term = 0)           %lld\n", st.pmVacuousModel);
  printf("  model nSurv==1, truth nSurv==1     %lld   correct fire\n", st.pmModel1_truth1);
  printf("  model nSurv==1, truth vacuous      %lld   over-fire (t already knew)\n", st.pmModel1_truthV);
  printf("  model nSurv==1, truth nSurv!=1     %lld   (expected 0)\n", st.pmModel1_truth0);
  printf("  model nSurv>1,  truth nSurv==1     %lld   <-- SUPPRESSED REAL LEAK\n", st.pmModelN_truth1);
  printf("  model nSurv>1,  truth nSurv!=1     %lld\n", st.pmModelN_truthN);
  long long realLeaks = st.pmModel1_truth1 + st.pmModelN_truth1;
  printf("  detection rate on real leaks       %.2f%%  (%lld of %lld)\n",
         100.0*st.pmModel1_truth1/std::max(1LL,realLeaks), st.pmModel1_truth1, realLeaks);
  return 0;
}
