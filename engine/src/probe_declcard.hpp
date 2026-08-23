// Adversarial verification of the policy-review claim:
//   "declareByValue's wrong-declaration branch passes dOur=-SETSZ, dTheir=0,
//    but most wrong declarations leave cards on the opponents' side too."
//
// Independent of probe_policy_v04.hpp.  Subclasses the shipped V04Agent and
// replicates v04.hpp:689-749 (proposeDeclaration, minus the gateAudit branch
// which is off by default) so we can call declareByValue twice per candidate
// half-suit: once as shipped, once with the corrected card deltas.
#pragma once
#include "v04.hpp"
#include "game.hpp"
#include "arena.hpp"
#include <cmath>
#include <cstdio>

namespace fish {
namespace declcard {

struct DCStats {
  long long dbvCalls = 0;         // declareByValue invocations (non-urgent value branch)
  long long shipDeclare = 0;      // shipped verdict = declare
  long long fixDeclare = 0;       // corrected verdict = declare
  long long flipW2D = 0;          // shipped said wait, corrected says declare
  long long flipD2W = 0;          // shipped said declare, corrected says wait
  double sumBranchErr = 0;        // |vWrongFix - vWrongShip| per call
  double maxBranchErr = 0;
  double sumEvErr = 0;            // (1-pAlloc)*|dvWrong|  == |vDeclareFix - vDeclareShip|
  double maxEvErr = 0;
  double sumSlack = 0;            // vDeclareShip - (vWait + margin)
  long long pallocBucketN[10] = {0};
  long long pallocBucketFlip[10] = {0};
  double sumOurW = 0; long long nOurW = 0;
  long long setsA = 0, setsB = 0, evts = 0;
  void merge(const DCStats& o) {
    dbvCalls += o.dbvCalls; shipDeclare += o.shipDeclare; fixDeclare += o.fixDeclare;
    flipW2D += o.flipW2D; flipD2W += o.flipD2W;
    sumBranchErr += o.sumBranchErr; maxBranchErr = std::max(maxBranchErr, o.maxBranchErr);
    sumEvErr += o.sumEvErr; maxEvErr = std::max(maxEvErr, o.maxEvErr);
    sumSlack += o.sumSlack; sumOurW += o.sumOurW; nOurW += o.nOurW;
    for (int i = 0; i < 10; i++) { pallocBucketN[i] += o.pallocBucketN[i];
                                   pallocBucketFlip[i] += o.pallocBucketFlip[i]; }
  }
};

// mode 0 = shipped (both branches dOur=-6,dTheir=0)
// mode 1 = corrected: wrong branch uses the posterior expected split
// mode 2 = corrected: wrong branch uses the empirical (-4.21,-1.79) split
struct DCAgent : V04Agent {
  int mode = 0;
  DCStats* st = nullptr;

  // Replica of v04.hpp:653-671 with a switchable wrong-branch card delta.
  bool dbv(const PublicState& pub, const SetVerdict& v, int useMode, double* outSlack,
           double* outVWrong) {
    int S = v.decl.set;
    int scoreDiff = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    int turnSign = (teamOf(pub.turn) == teamOf(seat)) ? 1 : -1;
    double eOld = eH[S];
    double dC = -(2 * eOld - 1), dS = -sharp(eOld);
    double dL = -(eOld > .995 ? 1.0 : eOld < .005 ? -1.0 : 0.0);
    double dK = -eOld * (1 - eOld);
    uint64_t u = k.unresolved & setMask(S);
    int dUnres = -__builtin_popcountll(u);
    double ourW = SETSZ, theirW = 0;
    if (useMode == 1) {
      // expected cards of the set on our side under the current posterior
      double e = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(S, i);
        if (k.myHand & bit(c)) { e += 1.0; continue; }
        double p = 0;
        for (int q = 0; q < NPLAY; q++) if (teamMask & (1u << q)) p += bel.marg[c][q];
        e += p;
      }
      ourW = e; theirW = SETSZ - e;
    } else if (useMode == 2) {
      ourW = 4.21; theirW = 1.79;
    } else if (useMode == 3) {
      // E[our count | allocation WRONG].  A correct allocation implies all six
      // are ours, so E[count] = pAlloc*6 + (1-pAlloc)*E[count | wrong].
      double e = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(S, i);
        if (k.myHand & bit(c)) { e += 1.0; continue; }
        double p = 0;
        for (int q = 0; q < NPLAY; q++) if (teamMask & (1u << q)) p += bel.marg[c][q];
        e += p;
      }
      double denom = 1.0 - v.pAlloc;
      double cw = denom > 1e-6 ? (e - SETSZ * v.pAlloc) / denom : double(SETSZ);
      if (cw < 0) cw = 0; if (cw > SETSZ) cw = SETSZ;
      ourW = cw; theirW = SETSZ - cw;
    }
    if (st && useMode) { st->sumOurW += ourW; st->nOurW++; }
    double vRight = value(pub, dC, dS, dL, dK, scoreDiff + 1, turnSign, -SETSZ, 0, dUnres, -1);
    double vWrong = value(pub, dC, dS, dL, dK, scoreDiff - 1, turnSign, 0, 0, dUnres, -1)
                  + cfg.vw[6] * (-ourW + theirW) / 54.0;
    double vDeclare = v.pAlloc * vRight + (1 - v.pAlloc) * vWrong;
    double vWait = value(pub, 0, 0, 0, 0, scoreDiff, turnSign, 0, 0, 0, 0);
    if (outSlack) *outSlack = vDeclare - (vWait + cfg.declareMargin);
    if (outVWrong) *outVWrong = vWrong;
    return vDeclare > vWait + cfg.declareMargin;
  }

  bool dnow(const PublicState& pub, const SetVerdict& v, bool urgent, int press, int useMode,
            bool* wentValue, double* slack, double* vw) {
    *wentValue = false;
    bool locked = v.pTeam > .9995;
    if (press >= 2) return true;
    if (press >= 1 && v.pAlloc >= 0.5) return true;
    if (cfg.useValue && cfg.valueDeclare) {
      if (urgent) return v.pAlloc >= cfg.declThreshold || (locked && v.pAlloc >= 0.5);
      *wentValue = true;
      return dbv(pub, v, useMode, slack, vw);
    }
    if (locked) {
      if (v.pAlloc >= cfg.lockedAllocThresh && (!cfg.patientLocked || urgent)) return true;
      if (urgent && v.pAlloc >= cfg.declThreshold) return true;
      return false;
    }
    return v.pAlloc >= cfg.declThreshold;
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (!cfg.declareEnabled) return false;
    if (!pub.rules.cardlessMayDeclare && !pub.handCount[seat]) return false;
    int unresolvedCount = __builtin_popcountll(k.unresolved);
    int press = pressure(pub);
    bool bypass = unresolvedCount <= 8 || press >= 1;
    if (cfg.useValue) computeAggregates(pub);
    bool candidate = bypass;
    for (int s = 0; s < NSET && !candidate; s++) {
      if (!pub.setActive[s]) continue;
      if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) candidate = true;
    }
    if (!candidate) return false;
    refresh();
    int oppCards = 0;
    for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
    bool urgent = unresolvedCount <= cfg.patiencePool
               || oppCards <= cfg.oppCardFloor
               || pub.nEvents >= cfg.forceDeclareEvents
               || bestAskProbability(pub) < cfg.askFloor;
    double bestConf = -1; bool found = false;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      if (!(bypass || k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb)) continue;
      SetVerdict v = evaluateSet(pub, s, press);
      if (!v.ok) continue;
      bool wv0 = false, wv1 = false;
      double sl0 = 0, sl1 = 0, vw0 = 0, vw1 = 0;
      bool ship = dnow(pub, v, urgent, press, 0, &wv0, &sl0, &vw0);
      bool fix  = dnow(pub, v, urgent, press, mode ? mode : 1, &wv1, &sl1, &vw1);
      if (st && wv0 && wv1) {
        st->dbvCalls++;
        if (ship) st->shipDeclare++;
        if (fix) st->fixDeclare++;
        double be = std::fabs(vw1 - vw0);
        st->sumBranchErr += be; st->maxBranchErr = std::max(st->maxBranchErr, be);
        double ee = (1.0 - v.pAlloc) * be;
        st->sumEvErr += ee; st->maxEvErr = std::max(st->maxEvErr, ee);
        st->sumSlack += sl0;
        int b = std::min(9, std::max(0, int(v.pAlloc * 10)));
        st->pallocBucketN[b]++;
        if (ship != fix) st->pallocBucketFlip[b]++;
        if (!ship && fix) st->flipW2D++;
        if (ship && !fix) st->flipD2W++;
      }
      bool use = mode ? fix : ship;
      if (!use) continue;
      if (v.pAlloc > bestConf) { bestConf = v.pAlloc; d = v.decl; found = true; }
    }
    conf = bestConf;
    return found;
  }
};

inline DCStats runDeclCard(int games, uint64_t seed, const Rules& rules, int mode) {
  DCStats st;
  std::unique_ptr<DCAgent> ag[NPLAY];
  Agent* raw[NPLAY];
  for (int p = 0; p < NPLAY; p++) {
    ag[p].reset(new DCAgent());
    ag[p]->mode = mode; ag[p]->st = &st;
    raw[p] = ag[p].get();
  }
  Game game;
  for (int i = 0; i < games; i++) {
    uint64_t sd = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    GameResult gr = game.run(sd, rules, raw);
    st.setsA += gr.decls[0] + gr.decls[1]; st.setsB += gr.correctDecls[0] + gr.correctDecls[1]; st.evts += gr.events;
  }
  return st;
}

}  // namespace declcard
}  // namespace fish
