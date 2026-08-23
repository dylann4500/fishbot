// Adversarial verification of P4/D3: "proposeDeclaration builds eH[]/agg from a
// stale posterior because computeAggregates() runs before refresh()".
//
// Independent of probe_policy_v04.hpp.  Instead of copying v04.hpp, this
// SUBCLASSES the shipped V04Agent and overrides the one virtual we care about,
// proposeDeclaration, replicating v04.hpp:689-749 verbatim (minus the
// cfg.gateAudit diagnostic branch, which is off by default) plus instrumentation.
#pragma once
#include "v04.hpp"
#include "game.hpp"
#include "arena.hpp"
#include <cstring>
#include <cmath>
#include <iostream>
#include <vector>

namespace fish {
namespace polreview {

struct RevStats {
  long long opps = 0;             // proposeDeclaration entries past the cheap guards
  long long oppsStale = 0;        // ... where dirty == true on entry
  long long oppsPress[3] = {0,0,0};
  long long refreshReached = 0;   // reached v04.hpp:705
  long long cmp = 0;              // opportunities where stale/fresh eH were compared
  long long cmpDiff = 0;          // ... and actually differed
  double sumMaxDiff = 0, maxMaxDiff = 0;
  long long declNowCalls = 0;     // declareNow invocations
  long long valueRuleCalls = 0;   // ... that reached declareByValue
  long long flips = 0;            // declareNow verdict changed under fresh aggregates
  long long valueFlips = 0;       // ... of which via the declareByValue branch
  long long actionChanged = 0;    // final (found, set) differed
  // staleness age: number of observe() calls since the last refresh
  long long ageSum = 0, ageMax = 0;
  void merge(const RevStats& o) {
    opps += o.opps; oppsStale += o.oppsStale; refreshReached += o.refreshReached;
    cmp += o.cmp; cmpDiff += o.cmpDiff; sumMaxDiff += o.sumMaxDiff;
    maxMaxDiff = std::max(maxMaxDiff, o.maxMaxDiff);
    declNowCalls += o.declNowCalls; valueRuleCalls += o.valueRuleCalls;
    flips += o.flips; valueFlips += o.valueFlips; actionChanged += o.actionChanged;
    ageSum += o.ageSum; ageMax = std::max(ageMax, o.ageMax);
    for (int i = 0; i < 3; i++) oppsPress[i] += o.oppsPress[i];
  }
};

// fixOrder: 0 = shipped order (aggregates from the stale belief)
//           1 = refresh() first, then computeAggregates()
struct RevAgent : V04Agent {
  int fixOrder = 0;
  bool measure = false;
  bool dump = false;
  double dumpThresh = 0.5;
  int gameIdx = 0;
  RevStats* st = nullptr;
  int age = 0;    // observe()s since the last refresh

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V04Agent::reset(s, hand, r, seed);
    age = 0;
  }
  void observe(const Event& e) override { V04Agent::observe(e); age++; }
  // every other refresh() site in the shipped agent, so `age` really measures
  // "public events observed since this agent last rebuilt bel.marg".
  AskMove chooseAsk(const PublicState& pub) override { AskMove m = V04Agent::chooseAsk(pub); age = 0; return m; }
  int valueFeatures(const PublicState& pub, double* f) override { int n = V04Agent::valueFeatures(pub, f); age = 0; return n; }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double th) override {
    bool r = V04Agent::willingForced(pub, set, d, conf, th); age = 0; return r; }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    V04Agent::bestGuess(pub, set, d, conf); age = 0; }
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    int r = V04Agent::choosePassTarget(pub, cand, n); age = 0; return r; }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (!cfg.declareEnabled) return false;
    if (!pub.rules.cardlessMayDeclare && !pub.handCount[seat]) return false;
    int unresolvedCount = __builtin_popcountll(k.unresolved);
    int press = pressure(pub);
    bool bypass = unresolvedCount <= 8 || press >= 1;

    bool wasDirty = dirty;
    int ageAtEntry = age;
    if (st) {
      st->opps++;
      if (wasDirty) st->oppsStale++;
      st->oppsPress[press < 3 ? press : 2]++;
      st->ageSum += ageAtEntry;
      st->ageMax = std::max(st->ageMax, (long long)ageAtEntry);
    }

    double eHstale[NSET]; ValueAggregates aggStale{};
    if (fixOrder == 0) {
      if (cfg.useValue) computeAggregates(pub);
      std::memcpy(eHstale, eH, sizeof(eH)); aggStale = agg;
    }

    bool candidate = bypass;
    for (int s = 0; s < NSET && !candidate; s++) {
      if (!pub.setActive[s]) continue;
      if (k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb) candidate = true;
    }
    if (!candidate) return false;
    refresh();
    if (st) st->refreshReached++;
    age = 0;

    double eHfresh[NSET]; ValueAggregates aggFresh{};
    if (fixOrder != 0) {
      if (cfg.useValue) computeAggregates(pub);
      std::memcpy(eHstale, eH, sizeof(eH)); aggStale = agg;   // "stale" slot holds the live one
      std::memcpy(eHfresh, eH, sizeof(eH)); aggFresh = agg;
    } else if (measure && cfg.useValue) {
      computeAggregates(pub);
      std::memcpy(eHfresh, eH, sizeof(eH)); aggFresh = agg;
      double mx = 0;
      for (int s = 0; s < NSET; s++) {
        if (!pub.setActive[s]) continue;
        mx = std::max(mx, std::fabs(eHstale[s] - eHfresh[s]));
      }
      if (dump && mx >= dumpThresh) {
        int ws = -1; double wd = 0;
        for (int s2 = 0; s2 < NSET; s2++) { if (!pub.setActive[s2]) continue;
          double dd = std::fabs(eHstale[s2] - eHfresh[s2]); if (dd > wd) { wd = dd; ws = s2; } }
        std::cout << "BIGDIFF game=" << gameIdx << " seat=" << seat << " ev=" << pub.nEvents
                  << " set=" << ws << " eH_stale=" << eHstale[ws] << " eH_fresh=" << eHfresh[ws]
                  << " diff=" << wd << " ageAtEntry=" << ageAtEntry
                  << " handCount=" << int(pub.handCount[seat]) << "\n";
      }
      if (st) {
        st->cmp++; st->sumMaxDiff += mx;
        if (mx > 1e-12) st->cmpDiff++;
        st->maxMaxDiff = std::max(st->maxMaxDiff, mx);
      }
      // restore the shipped (stale) aggregates
      std::memcpy(eH, eHstale, sizeof(eH)); agg = aggStale;
    } else {
      std::memcpy(eHfresh, eHstale, sizeof(eH)); aggFresh = aggStale;
    }

    int oppCards = 0;
    for (int p = 0; p < NPLAY; p++) if (oppMask & (1 << p)) oppCards += pub.handCount[p];
    bool urgent = unresolvedCount <= cfg.patiencePool
               || oppCards <= cfg.oppCardFloor
               || pub.nEvents >= cfg.forceDeclareEvents
               || bestAskProbability(pub) < cfg.askFloor;
    double bestConf = -1; bool found = false;
    double freshBestConf = -1; bool freshFound = false; int freshBestSet = -1;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      bool passesGate = bypass || k.cheapTeamProb(s, teamMask) >= cfg.gateTeamProb;
      if (!passesGate) continue;
      SetVerdict v = evaluateSet(pub, s, press);
      if (!v.ok) continue;
      bool dec = declareNow(pub, v, urgent, press);
      if (measure && st) {
        st->declNowCalls++;
        bool viaValue = !(press >= 2) && !(press >= 1 && v.pAlloc >= 0.5)
                        && cfg.useValue && cfg.valueDeclare && !urgent;
        if (viaValue) st->valueRuleCalls++;
        std::memcpy(eH, eHfresh, sizeof(eH)); agg = aggFresh;
        bool decF = declareNow(pub, v, urgent, press);
        std::memcpy(eH, eHstale, sizeof(eH)); agg = aggStale;
        if (decF != dec) {
          st->flips++; if (viaValue) st->valueFlips++;
          if (dump) {
            std::cout << "FLIP game=" << gameIdx << " seat=" << seat << " ev=" << pub.nEvents
                      << " set=" << s << " press=" << press << " urgent=" << urgent
                      << " pAlloc=" << v.pAlloc << " pTeam=" << v.pTeam
                      << " eH_stale=" << eHstale[s] << " eH_fresh=" << eHfresh[s]
                      << " agg.sumControl " << aggStale.sumControl << " -> " << aggFresh.sumControl
                      << " decision " << dec << " -> " << decF
                      << " ageAtEntry=" << ageAtEntry << "\n";
          }
        }
        if (decF && v.pAlloc > freshBestConf) { freshBestConf = v.pAlloc; freshBestSet = s; freshFound = true; }
      }
      if (!dec) continue;
      if (v.pAlloc > bestConf) { bestConf = v.pAlloc; d = v.decl; found = true; }
    }
    if (measure && st) {
      bool changed = (found != freshFound) || (found && freshFound && int(d.set) != freshBestSet);
      if (changed) st->actionChanged++;
    }
    conf = bestConf;
    return found;
  }
};

// ---- runners -------------------------------------------------------------

struct RevCfg {
  int games = 100, rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int fixA = 0, fixB = 0;
  bool measure = true;
  bool dump = false;
  double dumpThresh = 0.5;
  BeliefMode belief = BeliefMode::Fast;
};

struct RevOut {
  RevStats st;
  MatchStats ms;
};

inline RevOut runReview(const RevCfg& c) {
  RevOut out;
  std::vector<RevAgent> A(3), B(3);
  for (int i = 0; i < 3; i++) {
    A[i].fixOrder = c.fixA; A[i].measure = c.measure; A[i].st = &out.st; A[i].cfg.belief = c.belief;
    A[i].dump = c.dump; A[i].dumpThresh = c.dumpThresh;
    B[i].fixOrder = c.fixB; B[i].measure = false;     B[i].st = nullptr;  B[i].cfg.belief = c.belief;
  }
  Game game;
  for (int i = 0; i < c.games; i++) {
    uint64_t s = mixSeed(c.seed, uint64_t(i) * 2654435761ull + 1);
    for (int j = 0; j < 3; j++) A[j].gameIdx = i;
    int aWins = 0;
    for (int rot = 0; rot < c.rotations; rot++) {
      int orient = (c.rotations == 2) ? rot : (rot / 3);
      int shift  = (c.rotations == 2) ? 0   : (rot % 3);
      Agent* ag[NPLAY];
      for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == orient) ? (Agent*)&A[p / 2] : (Agent*)&B[p / 2];
      game.rotation = shift;
      GameResult r = game.run(s, c.rules, ag);
      int teamA = orient, teamB = 1 - orient;
      if (r.winner == teamA) { out.ms.winsA++; aWins++; }
      out.ms.decl[0] += r.decls[teamA]; out.ms.declCorrect[0] += r.correctDecls[teamA];
      out.ms.decl[1] += r.decls[teamB]; out.ms.declCorrect[1] += r.correctDecls[teamB];
      out.ms.sets[0] += r.score[teamA]; out.ms.sets[1] += r.score[teamB];
      out.ms.events += r.events;
      if (r.hitLimit) out.ms.limitHits++;
    }
    out.ms.games++;
    out.ms.paired.push_back(uint8_t(aWins));
  }
  return out;
}

}  // namespace polreview
}  // namespace fish
