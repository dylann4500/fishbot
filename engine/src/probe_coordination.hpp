// SCRATCH PROBE (P8): the two legal coordination channels.
//   (1) turn transfer  -- rules allow the team to exchange willingness bits;
//                         v0.4 has the cardless player decide unilaterally.
//   (2) forced endgame -- the willingness ladder already exists (Rules::forcedTh).
// Nothing here is on the shipped path; see probe_coordination_game.hpp for the
// patched driver.
#pragma once
#include "probe_coordination_game.hpp"
#include "factory.hpp"
#include "arena.hpp"
#include <thread>

namespace fish {
namespace probecoord {

struct CoordConfig {
  std::string specA = "v04", specB = "v04";
  int games = 300;
  int rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 0;
  CoordCfg cc;
  std::vector<int>* perGameA = nullptr;   // A-team sets, indexed games*rotations
};

inline CoordStats runCoord(const CoordConfig& cc) {
  int nThreads = cc.threads > 0 ? cc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, cc.games));
  std::vector<CoordStats> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(cc.specA); B[i] = makeAgent(cc.specB); }
      CoordStats& st = local[t];
      Game game;
      game.cst = &st;
      game.ccfg = cc.cc;
      for (int i = t; i < cc.games; i += nThreads) {
        uint64_t s = mixSeed(cc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < cc.rotations; rot++) {
          int orient = (cc.rotations == 2) ? rot : (rot / 3);
          int shift  = (cc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          // The coordination policy under test is used by team A only, unless
          // policyTeam was explicitly set to -1 meaning "both".
          game.ccfg.policyTeam = (cc.cc.policyTeam == -2) ? -1 : orient;
          game.postPassTeam = -1; game.postPassAsks = 0; game.postPassHits = 0;
          long long p0 = st.passEvents, m0 = st.passMulti;
          GameResult r = game.run(s, cc.rules, ag);
          if (game.postPassTeam >= 0) {
            st.postPassRuns++; st.postPassAsks += game.postPassAsks; st.postPassHits += game.postPassHits;
            if (!game.postPassAsks) st.postPassZero++;
          }
          st.games++;
          st.events += r.events;
          if (st.passEvents > p0) st.gamesWithPass++;
          if (st.passMulti > m0) st.gamesWithMultiPass++;
          int teamA = orient, teamB = 1 - orient;
          st.sets[0] += r.score[teamA]; st.sets[1] += r.score[teamB];
          if (r.winner == teamA) st.wins[0]++; else st.wins[1]++;
          if (cc.perGameA) (*cc.perGameA)[size_t(i) * cc.rotations + rot] = r.score[teamA];
          if (r.hitLimit) st.limitHits++;
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  CoordStats total;
  for (int t = 0; t < nThreads; t++) total.merge(local[t]);
  return total;
}

inline double binEnt2(double p) {
  if (p <= 0 || p >= 1) return 0;
  return -p * std::log2(p) - (1 - p) * std::log2(1 - p);
}

inline void printCoord(const CoordStats& s, const CoordConfig& cc, std::ostream& os) {
  auto pct = [](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  long long G = s.games;
  os << "games                " << G << "   events/game " << (G ? double(s.events) / G : 0) << "\n";
  os << "sets  A " << s.sets[0] << "  B " << s.sets[1]
     << "   A sets/game " << (G ? double(s.sets[0]) / G : 0)
     << "   A win rate " << pct(s.wins[0], G) << "%\n";
  os << "limit-hit games      " << s.limitHits << " (" << pct(s.limitHits, G) << "%)\n";
  os << "\n-- turn transfer --\n";
  os << "pass events          " << s.passEvents << "   per game " << (G ? double(s.passEvents) / G : 0) << "\n";
  os << "games with >=1 pass  " << s.gamesWithPass << " (" << pct(s.gamesWithPass, G) << "%)\n";
  os << "pass decisions       " << s.passDecisions
     << "   with 2+ candidates " << s.passMulti << " (" << pct(s.passMulti, s.passDecisions) << "%)\n";
  os << "games with a real (2+ candidate) choice " << s.gamesWithMultiPass
     << " (" << pct(s.gamesWithMultiPass, G) << "%)\n";
  os << "post-transfer runs   " << s.postPassRuns
     << "   asks/run " << (s.postPassRuns ? double(s.postPassAsks) / s.postPassRuns : 0)
     << "   hits/run " << (s.postPassRuns ? double(s.postPassHits) / s.postPassRuns : 0)
     << "   hit rate " << pct(s.postPassHits, s.postPassAsks) << "%\n";
  os << "  transfers with 0 asks by the receiver " << s.postPassZero
     << " (" << pct(s.postPassZero, s.postPassRuns) << "%)\n";
  if (s.passMulti) {
    os << "\n-- selection quality at the " << s.passMulti << " decisions with 2+ candidates --\n";
    os << "   (sureRun = number of cards the receiver could pull with CERTAINTY)\n";
    os << "   at least one candidate had sureRun>0: " << s.multiAnyLive
       << " (" << pct(s.multiAnyLive, s.passMulti) << "%)\n";
    auto row = [&](const char* n, long long live, double sure, long long agree, bool showAgree) {
      os << "   " << n << "  live " << pct(live, s.passMulti) << "%"
         << "   mean sureRun " << sure / double(s.passMulti);
      if (showAgree) os << "   agrees with oracle " << pct(agree, s.passMulti) << "%";
      os << "\n";
    };
    row("lowest seat  ", s.lowLive, s.lowSure, s.lowAgree, true);
    row("most cards   ", s.cardLive, s.cardSure, s.cardAgree, true);
    row("v0.4 unilat. ", s.uniLive, s.uniSure, s.uniAgree, true);
    row("willing ladd.", s.ladLive, s.ladSure, s.ladAgree, true);
    row("ORACLE       ", s.oraLive, s.oraSure, 0, false);
    double gapU = double(s.oraLive - s.uniLive), gapL = double(s.ladLive - s.uniLive);
    os << "   ladder recovers " << (gapU > 0 ? 100.0 * gapL / gapU : 0.0) << "% of the oracle gap on 'live'\n";
    double gsU = (s.oraSure - s.uniSure), gsL = (s.ladSure - s.uniSure);
    os << "   ladder recovers " << (gsU > 0 ? 100.0 * gsL / gsU : 0.0) << "% of the oracle gap on mean sureRun\n";
    os << "   ladder found nobody willing at any rung: " << s.ladderNoWill << "\n";
    os << "   decisions where the candidates DIFFER in sureRun: " << s.multiDiffer
       << " (" << pct(s.multiDiffer, s.passMulti) << "% of multi, "
       << (G ? double(s.multiDiffer) / G : 0) << "/game)\n";
    os << "   decisions that are DECISIVE (some candidate has no certain hit at all, another does): "
       << s.multiDecisive << " (" << pct(s.multiDecisive, s.passMulti) << "%)\n";
    os << "   among the differing decisions, argmax picked by: v0.4 " << pct(s.uniBestDiffer, s.multiDiffer)
       << "%   ladder " << pct(s.ladBestDiffer, s.multiDiffer) << "%\n";
    os << "   shortfall vs oracle (cards):  d  v0.4   ladder\n";
    for (int i = 0; i < 16; i++) if (s.uniLossHist[i] || s.ladLossHist[i])
      os << "                                 " << i << "  " << s.uniLossHist[i] << "     " << s.ladLossHist[i] << "\n";
  }
  os << "\n-- forced endgame ladder --\n";
  os << "forced sweeps        " << s.forcedSweeps << "   forced declarations " << s.forcedDecls
     << "   wrong " << s.forcedWrong << " (" << pct(s.forcedWrong, s.forcedDecls) << "%)\n";
  os << "residue (nobody declared) " << s.forcedResidue << "\n";
  os << "rung   threshold    fired     wrong      mean conf\n";
  for (int i = 0; i <= cc.rules.nForcedTh && i <= MAXRUNG; i++) {
    if (i == cc.rules.nForcedTh && !s.rungFired[i]) continue;
    char buf[64];
    if (i < cc.rules.nForcedTh && cc.rules.forcedTh[i] >= 0) snprintf(buf, sizeof(buf), "%8.4f", cc.rules.forcedTh[i]);
    else snprintf(buf, sizeof(buf), "bestGuess");
    os << "  " << i << "   " << buf << "  " << s.rungFired[i]
       << " (" << pct(s.rungFired[i], s.forcedDecls) << "%)   "
       << s.rungWrong[i] << " (" << pct(s.rungWrong[i], s.rungFired[i]) << "%)   "
       << (s.rungFired[i] ? s.rungConf[i] / s.rungFired[i] : 0.0) << "\n";
  }
  if (s.confN) {
    os << "\nconfidence every declaring-team player attaches to its OWN best allocation,\n"
       << "surveyed over all (player, live half-suit) pairs at the moment the forced endgame opens:\n";
    os << "   n=" << s.confN << "  mean " << s.confSum / s.confN << "  max " << s.confMax << "\n";
    static const char* lbl[12] = {"exactly 0","<1e-6","<1e-4","<1e-3","<1e-2","<0.05","<0.10","<0.25","<0.50","<0.80","<0.95",">=0.95"};
    for (int i = 0; i < 12; i++) if (s.confHist[i])
      os << "     " << lbl[i] << "  " << s.confHist[i] << " (" << pct(s.confHist[i], s.confN) << "%)\n";
    os << "   allocation named vs the declarer's OWN Knowledge: contradicts a known owner "
       << pct(s.violOwner, s.confN) << "%, names a card at a seat its own mask excludes "
       << pct(s.violMask, s.confN) << "%, exceeds a teammate's capacity "
       << pct(s.violCap, s.confN) << "%, self-consistent " << pct(s.violNone, s.confN) << "%\n";
    os << "   picking the ARGMAX-confidence player for each half-suit would be right "
       << pct(s.confBestTrue, s.confBestN) << "% of the time (n=" << s.confBestN << ")\n";
  }
  long long leakTot = 0; for (int i = 0; i <= MAXRUNG; i++) leakTot += s.leakN[i];
  if (leakTot) {
    os << "\n-- information leakage of the turn-transfer ladder --\n";
    os << "  bucketed over (pass decision, candidate, live half-suit) triples.\n";
    os << "  prior = the observing opponent's own posterior P(candidate holds >=1 card of the half-suit)\n";
    long long rTot = 0; for (int i = 0; i <= MAXRUNG; i++) rTot += s.rungHist[i];
    double HR = 0;
    for (int i = 0; i <= MAXRUNG; i++) if (s.rungHist[i]) {
      double p = double(s.rungHist[i]) / rTot; HR -= p * std::log2(p);
    }
    os << "  rung distribution over candidates (n=" << rTot << "):";
    for (int i = 0; i <= cc.cc.nRung; i++) os << " r" << i << "=" << pct(s.rungHist[i], rTot) << "%";
    os << "\n  H(rung) = " << HR << " bits  <-- an EXACT upper bound on the per-candidate leak,\n"
       << "     because the transcript is a deterministic function of the private state\n";
    os << "  rung   n        mean prior    realised     lift      dH(bits)\n";
    double dHsum = 0; long long dHn = 0;
    for (int i = 0; i <= cc.cc.nRung; i++) {
      if (!s.leakN[i]) continue;
      double pr = s.leakPrior[i] / s.leakN[i];
      double re = double(s.leakTruth[i]) / s.leakN[i];
      double dH = binEnt2(pr) - binEnt2(re);
      dHsum += dH * s.leakN[i]; dHn += s.leakN[i];
      char buf[160];
      snprintf(buf, sizeof(buf), "   r%d  %8lld     %8.4f    %8.4f  %+8.4f    %+7.4f\n",
               i, s.leakN[i], pr, re, re - pr, dH);
      os << buf;
    }
    os << "  mean entropy change of the observer's per-(candidate,half-suit) belief: "
       << (dHn ? dHsum / dHn : 0.0) << " bits\n";
  }
}

} // namespace probecoord
} // namespace fish
