// P-lit scratch probe (adversarial verification of the "value function has no
// time feature" claim).  Runs v0.4 mirror self-play with an instrumented COPY of
// the v0.4 policy (probe_literature_v04.hpp) and reports, at every declaration
// opportunity, which branch of declareNow decided and what actually blocked a
// declaration.  Nothing here is on any shipped decision path.
#pragma once
#include "arena.hpp"
#include "diag.hpp"
#include "probe_literature_v04.hpp"
#include <thread>
#include <cstdio>

namespace fish {

inline void runLitProbe(int games, int rotations, uint64_t seed, const Rules& rules,
                        int nThreads, litp::LitStats& out, bool valueDeclareOff,
                        double vmargin, bool haveMargin) {
  if (nThreads < 1) nThreads = int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      litp::litRegister();
      std::unique_ptr<litp::V04Agent> ags[NPLAY];
      Agent* raw[NPLAY];
      for (int p = 0; p < NPLAY; p++) {
        ags[p] = std::make_unique<litp::V04Agent>();
        if (valueDeclareOff) ags[p]->cfg.valueDeclare = false;
        if (haveMargin) ags[p]->cfg.declareMargin = vmargin;
        raw[p] = ags[p].get();
      }
      Game game;
      for (int i = t; i < games; i += nThreads)
        for (int rot = 0; rot < rotations; rot++) {
          game.rotation = (rotations == 2) ? 0 : (rot % 3);
          game.run(mixSeed(seed, uint64_t(i) * 2654435761ull + 1), rules, raw);
        }
    });
  }
  for (auto& th : pool) th.join();
  std::lock_guard<std::mutex> g(litp::litMutex());
  for (auto* s : litp::litRegistry()) out.merge(*s);
}

inline void printLitProbe(const litp::LitStats& s, std::ostream& os) {
  auto pct = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  os << "declaration opportunities (all calls)   " << s.oppsAll << "\n";
  os << "  cut by the cheap capacity gate        " << s.earlyReturnGate
     << "  (" << pct(s.earlyReturnGate, s.oppsAll) << "%)\n";
  os << "  fully evaluated                       " << s.opps << "\n";
  os << "    press=0 / 1 / 2                     " << s.pressOpps[0] << " / "
     << s.pressOpps[1] << " / " << s.pressOpps[2] << "\n";
  os << "    urgent                              " << s.urgentOpps
     << "  (" << pct(s.urgentOpps, s.opps) << "%)"
     << "   of which askFloor-only " << s.urgentByAskFloor << "\n";
  os << "    >=1 half-suit certifiable (v.ok)    " << s.anyOk
     << "  (" << pct(s.anyOk, s.opps) << "%)\n";
  os << "    NOTHING certifiable                 " << s.noneOk
     << "  (" << pct(s.noneOk, s.opps) << "%)\n";
  os << "    declared                            " << s.declared
     << "  (" << pct(s.declared, s.opps) << "%)\n";
  os << "    certifiable but held back           " << (s.blockedByValue + s.blockedByThresh) << "\n";
  os << "      blocked by declareByValue (V)     " << s.blockedByValue << "\n";
  os << "      blocked by threshold/urgent path  " << s.blockedByThresh << "\n";
  os << "      mean best pAlloc when held back   "
     << (s.nBestPAllocBlocked ? s.sumBestPAllocBlocked / s.nBestPAllocBlocked : 0.0) << "\n";
  os << "LATE slice (nEvents >= 150)\n";
  os << "  opportunities                         " << s.lateOpps << "\n";
  os << "  fully evaluated & certifiable         " << s.lateAnyOk << "\n";
  os << "  fully evaluated & NOTHING certifiable " << s.lateNoneOk
     << "  (" << pct(s.lateNoneOk, s.lateAnyOk + s.lateNoneOk) << "% of evaluated)\n";
  os << "  urgent                                " << s.lateUrgent << "\n";
  os << "  declared                              " << s.lateDeclared << "\n";
  os << "  held back by declareByValue (V)       " << s.lateBlockedByValue << "\n";
  os << "  held back by threshold/urgent path    " << s.lateBlockedByThresh << "\n";
}


// Pathology metrics for the instrumented copy, so ask-side and declaration-side
// counterfactuals can be compared on exactly the diag.hpp definitions.
struct LitPathCfg {
  int games = 200, rotations = 2, threads = 0;
  uint64_t seed = 777001;
  Rules rules;
  int liveOnly = 0;
  double timeCost = 0.0;
  bool haveMargin = false; double vmargin = 0.0;
  double askFloor = -1;
};

inline PathologyStats runLitPathology(const LitPathCfg& c) {
  int nThreads = c.threads > 0 ? c.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, c.games));
  std::vector<PathologyStats> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<litp::V04Agent> ags[NPLAY];
      Agent* raw[NPLAY];
      for (int p = 0; p < NPLAY; p++) {
        ags[p] = std::make_unique<litp::V04Agent>();
        ags[p]->cfg.litLiveOnly = c.liveOnly;
        ags[p]->cfg.litTimeCost = c.timeCost;
        if (c.haveMargin) ags[p]->cfg.declareMargin = c.vmargin;
        if (c.askFloor >= 0) ags[p]->cfg.askFloor = c.askFloor;
        raw[p] = ags[p].get();
      }
      Game game;
      game.trace.on = true;
      for (int i = t; i < c.games; i += nThreads)
        for (int rot = 0; rot < c.rotations; rot++) {
          game.rotation = (c.rotations == 2) ? 0 : (rot % 3);
          game.trace.events.clear();
          GameResult r = game.run(mixSeed(c.seed, uint64_t(i) * 2654435761ull + 1), c.rules, raw);
          analyseTrace(game.trace.events, game.g.dealt, c.rules, r.hitLimit, local[t]);
        }
    });
  }
  for (auto& th : pool) th.join();
  PathologyStats total;
  for (int t = 0; t < nThreads; t++) total.merge(local[t]);
  return total;
}


// Head-to-head: team A = instrumented copy with a knob set, team B = the copy
// with shipped settings.  Colour-balanced by running both orientations.
struct LitH2H { long long setsA = 0, setsB = 0, winA = 0, winB = 0, draws = 0, games = 0; };

inline LitH2H runLitH2H(const LitPathCfg& c) {
  int nThreads = c.threads > 0 ? c.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, c.games));
  std::vector<LitH2H> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<litp::V04Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) {
        A[i] = std::make_unique<litp::V04Agent>();
        A[i]->cfg.litLiveOnly = c.liveOnly;
        A[i]->cfg.litTimeCost = c.timeCost;
        if (c.haveMargin) A[i]->cfg.declareMargin = c.vmargin;
        B[i] = std::make_unique<litp::V04Agent>();
      }
      Game game;
      for (int i = t; i < c.games; i += nThreads)
        for (int orient = 0; orient < 2; orient++) {
          Agent* raw[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            raw[p] = (teamOf(p) == orient) ? (Agent*)A[p / 2].get() : (Agent*)B[p / 2].get();
          GameResult r = game.run(mixSeed(c.seed, uint64_t(i) * 2654435761ull + 1), c.rules, raw);
          int sa = r.score[orient], sb = r.score[1 - orient];
          local[t].setsA += sa; local[t].setsB += sb; local[t].games++;
          if (sa > sb) local[t].winA++; else if (sb > sa) local[t].winB++; else local[t].draws++;
        }
    });
  }
  for (auto& th : pool) th.join();
  LitH2H tot;
  for (auto& x : local) { tot.setsA += x.setsA; tot.setsB += x.setsB; tot.winA += x.winA;
                          tot.winB += x.winB; tot.draws += x.draws; tot.games += x.games; }
  return tot;
}

} // namespace fish
