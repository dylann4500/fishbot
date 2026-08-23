// Adversarial verification of the P1 claim:
//   "in 9 of 14 long games no half-suit is locked to any team at the dead run's
//    start or end, so the E11 frozen-locked-half-suit story cannot be the
//    mechanism for the majority of deadlocks."
//
// Independent re-implementation: replay v0.4 mirror games, mark provably-dead
// asks from each actor's own Knowledge, locate the longest dead run, and take a
// ground-truth lock census at EVERY event of the run (not just its two ends),
// plus at the game's final state.  Also reports the deadlocked pair's own
// half-suits and whether the two cycled questions live in locked half-suits.
#pragma once
#include "game.hpp"
#include "factory.hpp"
#include "belief.hpp"
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

namespace fish {
namespace vlock {

struct GameRec {
  uint64_t seed = 0;
  int rot = 0, events = 0;
  int runStart = -1, runLen = 0;
  int distinctTriples = 0;
  int lockedStart = 0, lockedEnd = 0;   // total (both teams), active sets only
  int lockedMaxInRun = 0;               // max over every event of the run
  int lockedMinInRun = 99;
  int liveStart = 0, liveEnd = 0;
  bool endMeasurable = false;           // was runStart+runLen a real event index?
  int cycleAskInLocked = 0;             // asks of the run inside a locked half-suit
  int runAsks = 0;
  int declaresInRun = 0;                // non-ask events swallowed by the run
  int lockedFinal = 0;                  // locks at the very last event of the game
};

// lock census over ground-truth hands
inline void census(const uint64_t* hand, const bool* act, int nsets, int& locked, int& live) {
  uint64_t t0 = hand[0] | hand[2] | hand[4], t1 = hand[1] | hand[3] | hand[5];
  locked = 0; live = 0;
  for (int s = 0; s < nsets; s++) {
    if (!act[s]) continue;
    live++;
    uint64_t m = setMask(s);
    if ((t0 & m) == m || (t1 & m) == m) locked++;
  }
}

inline void scanGame(const std::string& spec, uint64_t s, int rot, const Rules& rules, GameRec& r) {
  std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
  for (int p = 0; p < NPLAY; p++) { ag[p] = makeAgent(spec); ap[p] = ag[p].get(); }
  Game game; game.trace.on = true; game.rotation = rot;
  game.run(s, rules, ap);
  r.seed = s; r.rot = rot;
  const std::vector<Event>& ev = game.trace.events;
  r.events = int(ev.size());

  uint64_t dealt[NPLAY];
  for (int p = 0; p < NPLAY; p++) dealt[p] = game.g.dealt[p];

  // provably-dead marking, from each actor's own Knowledge
  Knowledge k[NPLAY];
  for (int p = 0; p < NPLAY; p++) k[p].init(p, dealt[p], rules.deckSets);
  std::vector<char> dead(ev.size(), 0);
  int run = 0, from = -1;
  for (size_t i = 0; i < ev.size(); i++) {
    const Event& e = ev[i];
    if (e.kind == Kind::Ask) {
      const Knowledge& kk = k[e.actor];
      bool d = (kk.owner[e.card] < NPLAY) ? (kk.owner[e.card] != e.target)
                                          : !(kk.mask[e.card] & (1u << e.target));
      dead[i] = d ? 1 : 0;
      if (d) { if (!run) from = int(i); run++;
               if (run > r.runLen) { r.runLen = run; r.runStart = from; } }
      else run = 0;
    }
    for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
  }
  if (r.runStart < 0) return;

  // NOTE: the run counter is only reset by a *non-dead ask*; declares do not
  // break it, so a "run" of runLen dead asks may span more than runLen events.
  // Walk events until we have consumed runLen dead asks.
  int endIdx = r.runStart;
  { int seen = 0;
    for (int j = r.runStart; j < r.events; j++) {
      if (ev[j].kind == Kind::Ask && dead[j]) { seen++; if (seen == r.runLen) { endIdx = j; break; } }
    } }

  // ground-truth forward simulation with a census at every event of the run
  uint64_t hand[NPLAY]; bool act[NSET];
  for (int p = 0; p < NPLAY; p++) hand[p] = dealt[p];
  for (int j = 0; j < NSET; j++) act[j] = (j < rules.deckSets);
  std::map<uint32_t,int> tally;
  int lockedNow = 0, liveNow = 0;
  for (int j = 0; j < r.events; j++) {
    if (j >= r.runStart && j <= endIdx) {
      census(hand, act, rules.deckSets, lockedNow, liveNow);
      if (j == r.runStart) { r.lockedStart = lockedNow; r.liveStart = liveNow; }
      r.lockedMaxInRun = std::max(r.lockedMaxInRun, lockedNow);
      r.lockedMinInRun = std::min(r.lockedMinInRun, lockedNow);
      const Event& e = ev[j];
      if (e.kind == Kind::Ask) {
        r.runAsks++;
        tally[(uint32_t(e.actor) << 16) | (uint32_t(e.card) << 8) | e.target]++;
        uint64_t t0 = hand[0] | hand[2] | hand[4], t1 = hand[1] | hand[3] | hand[5];
        uint64_t m = setMask(setOf(e.card));
        if ((t0 & m) == m || (t1 & m) == m) r.cycleAskInLocked++;
      } else r.declaresInRun++;
      if (j == endIdx) { r.lockedEnd = lockedNow; r.liveEnd = liveNow; r.endMeasurable = true; }
    }
    const Event& e = ev[j];
    if (e.kind == Kind::Ask && e.success) { hand[e.target] &= ~bit(e.card); hand[e.actor] |= bit(e.card); }
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
      act[e.set] = false;
    }
  }
  census(hand, act, rules.deckSets, r.lockedFinal, liveNow);
  r.distinctTriples = int(tally.size());
}

inline void run(const std::string& spec, int games, int rotations, uint64_t seed,
                int minEvents, const Rules& rules, std::ostream& os) {
  std::vector<GameRec> all;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < rotations; rot++) { GameRec r; scanGame(spec, s, rot, rules, r); all.push_back(r); }
  }
  std::vector<int> lg;
  for (int i = 0; i < int(all.size()); i++) if (all[i].events > minEvents) lg.push_back(i);
  std::sort(lg.begin(), lg.end(), [&](int a, int b){ return all[a].runLen > all[b].runLen; });
  os << "spec=" << spec << " seed=" << seed << " games=" << all.size()
     << "  long (>" << minEvents << " ev): " << lg.size() << "\n";
  int anyEnds = 0, anyAnywhere = 0, allLive = 0, endNotMeasurable = 0, anyFinal = 0;
  os << "  seed/rot                              ev   run  trip  live0 lockS lockE lockMax lockMin declInRun askInLocked lockFinal\n";
  for (int i : lg) {
    const GameRec& g = all[i];
    if (g.lockedStart + g.lockedEnd > 0) anyEnds++;
    if (g.lockedMaxInRun > 0) anyAnywhere++;
    if (g.lockedMaxInRun == 0) allLive++;
    if (!g.endMeasurable) endNotMeasurable++;
    if (g.lockedFinal > 0) anyFinal++;
    os << "  " << g.seed << "/" << g.rot << "  " << g.events << "  " << g.runLen << "  "
       << g.distinctTriples << "  " << g.liveStart << "  " << g.lockedStart << "  " << g.lockedEnd
       << "  " << g.lockedMaxInRun << "  " << g.lockedMinInRun << "  " << g.declaresInRun
       << "  " << g.cycleAskInLocked << "/" << g.runAsks << "  " << g.lockedFinal << "\n";
  }
  int n = int(lg.size());
  os << "\n  long games with a lock at run start OR end (P1's statistic): " << anyEnds << " / " << n
     << "   => no lock in " << (n - anyEnds) << " / " << n << "\n";
  os << "  long games with a lock ANYWHERE in the run:                  " << anyAnywhere << " / " << n
     << "   => no lock in " << (n - anyAnywhere) << " / " << n << "\n";
  os << "  long games whose run-end index was unmeasurable in P1's loop: " << endNotMeasurable << " / " << n << "\n";
  os << "  long games with a lock at the game's final state:            " << anyFinal << " / " << n << "\n";
  long long ce = 0, ca = 0;
  for (int i : lg) { ce += all[i].cycleAskInLocked; ca += all[i].runAsks; }
  os << "  asks inside the dead run that sit in a ground-truth locked half-suit: "
     << ce << " / " << ca << "\n";
}

} // namespace vlock
} // namespace fish
