// P6 scratch analysis: declaration timing and arbitration.
#pragma once
#include "probe_declaration_game.hpp"
#include "factory.hpp"
#include "arena.hpp"
#include <thread>
#include <mutex>

namespace fish {
namespace probe {

// ---------------------------------------------------------------- arbitration
struct ArbMatch {
  int deals = 0;
  int xWins = 0;                 // X-arm wins out of 2*deals
  long long xSets = 0, ySets = 0;
  long long xDecl = 0, xDeclOk = 0, yDecl = 0, yDeclOk = 0;
  long long xForced = 0, xForcedOk = 0, yForced = 0, yForcedOk = 0;
  long long events = 0;
  long long limitHits = 0;
  std::vector<uint8_t> paired;   // per deal: X wins across the two orientations
  ArbStats arb;
  void merge(const ArbMatch& o) {
    deals += o.deals; xWins += o.xWins; xSets += o.xSets; ySets += o.ySets;
    xDecl += o.xDecl; xDeclOk += o.xDeclOk; yDecl += o.yDecl; yDeclOk += o.yDeclOk;
    xForced += o.xForced; xForcedOk += o.xForcedOk; yForced += o.yForced; yForcedOk += o.yForcedOk;
    events += o.events; limitHits += o.limitHits;
    paired.insert(paired.end(), o.paired.begin(), o.paired.end());
    arb.merge(o.arb);
  }
};

// Mirror duplicate pair: the same deal is played twice, with the X treatment on
// team 0 and then on team 1, so the deal's luck cancels and the only asymmetry
// between the arms is the treatment itself.
inline ArbMatch runArb(const std::string& spec, int games, uint64_t seed, const Rules& rules,
                       int modeX, int modeY, bool ootX, bool ootY, int threads,
                       double ladderFloor = 0.0, int rungs = 0,
                       double ootThX = 0.0, double ootThY = 0.0,
                       double floorX = 0.0, double floorY = 0.0) {
  int nT = threads > 0 ? threads : int(std::thread::hardware_concurrency());
  nT = std::max(1, std::min(nT, std::max(1, games)));
  std::vector<ArbMatch> local(nT);
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[NPLAY];
      for (int i = 0; i < NPLAY; i++) A[i] = makeAgent(spec);
      PGame game;
      if (rungs > 0) game.setRungs(rungs);
      if (ladderFloor > 0) game.ladder[game.nLadder - 1] = ladderFloor;
      ArbMatch& st = local[t];
      for (int i = t; i < games; i += nT) {
        uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
        int xw = 0;
        for (int rot = 0; rot < 2; rot++) {
          int xTeam = rot;              // rot 0: X on team 0; rot 1: X on team 1
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = A[p].get();
          game.arbTeam[xTeam] = modeX;  game.arbTeam[1 - xTeam] = modeY;
          game.teamOOT[xTeam] = ootX;   game.teamOOT[1 - xTeam] = ootY;
          game.ootTh[xTeam] = ootThX;   game.ootTh[1 - xTeam] = ootThY;
          game.declFloor[xTeam] = floorX; game.declFloor[1 - xTeam] = floorY;
          game.arb = ArbStats{};
          GameResult r = game.run(s, rules, ag);
          if (r.winner == xTeam) { st.xWins++; xw++; }
          st.xSets += r.score[xTeam]; st.ySets += r.score[1 - xTeam];
          st.xDecl += r.decls[xTeam]; st.xDeclOk += r.correctDecls[xTeam];
          st.yDecl += r.decls[1 - xTeam]; st.yDeclOk += r.correctDecls[1 - xTeam];
          st.xForced += r.forcedDecls[xTeam]; st.xForcedOk += r.forcedCorrect[xTeam];
          st.yForced += r.forcedDecls[1 - xTeam]; st.yForcedOk += r.forcedCorrect[1 - xTeam];
          st.events += r.events; if (r.hitLimit) st.limitHits++;
          st.arb.merge(game.arb);
        }
        st.deals++;
        st.paired.push_back(uint8_t(xw));
      }
    });
  }
  for (auto& th : pool) th.join();
  ArbMatch total;
  for (int t = 0; t < nT; t++) total.merge(local[t]);
  return total;
}

// ---------------------------------------------------------------- replay core
// Rebuild every seat's Knowledge and the true hands along a traced game, keeping
// a window of past snapshots so that a declaration can be scored at earlier
// moments with the exact BlockDP posterior.
struct Snap {
  Knowledge k[NPLAY];
  uint64_t hand[NPLAY];
  bool active[NSET];
};

struct AllocProbe {
  bool ok = false;
  double pAlloc = 0, pTeam = 0;
  bool correct = false;      // does the exact argmax allocation match the truth?
  bool lockedOwn = false;    // truth: all six cards on the declarer's team
  bool lockedOpp = false;
};

// Exact BlockDP evaluation of half-suit s from seat p's knowledge at a snapshot.
inline AllocProbe probeSet(const Snap& sn, int p, int s) {
  AllocProbe r;
  if (!sn.active[s]) return r;
  int team = teamOf(p), tm = 0;
  for (int q = 0; q < NPLAY; q++) if (teamOf(q) == team) tm |= 1 << q;
  int cnt = 0;
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    for (int q = 0; q < NPLAY; q++) if (sn.hand[q] & bit(c)) { if (teamOf(q) == team) cnt++; }
  }
  r.lockedOwn = (cnt == SETSZ);
  r.lockedOpp = (cnt == 0);
  BlockDP dp;
  if (!dp.build(sn.k[p])) return r;
  r.pTeam = dp.teamOwnsProbability(s, tm);
  int oc[SETSZ], os[SETSZ], n = 0;
  double pa = dp.bestTeamAllocation(s, tm, oc, os, n);
  if (pa <= 0) { r.ok = true; r.pAlloc = 0; r.correct = false; return r; }
  int owner[SETSZ];
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    owner[i] = sn.k[p].owner[c] < NPLAY ? sn.k[p].owner[c] : -1;
  }
  for (int i = 0; i < n; i++) owner[idxIn(oc[i])] = os[i];
  bool good = true;
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (owner[i] < 0 || teamOf(owner[i]) != team || !(sn.hand[owner[i]] & bit(c))) { good = false; break; }
  }
  r.ok = true; r.pAlloc = pa; r.correct = good;
  return r;
}

// A voluntary declaration, with the exact posterior recomputed at several
// earlier moments.
struct DeclRecord {
  int event = 0, actor = 0, set = 0;
  bool correct = false, onTurn = false, lockedOwn = false, lockedOpp = false;
  double statedConf = 0;
  AllocProbe at[4];       // offsets -20, -10, -5, 0
  bool have[4] = {false, false, false, false};
};

inline int initialTurn(uint64_t seed, int deckSets) {
  GameState tmp{};
  dealCards(tmp, seed, deckSets);
  return tmp.turn;
}

// Walk a trace, maintaining snapshots, and score every voluntary declaration.
inline void backwardScan(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                         const Rules& rules, uint64_t seed,
                         std::vector<DeclRecord>& out, long long& buildFail) {
  const int W = 21;
  std::vector<Snap> ring(W);
  Snap cur;
  for (int p = 0; p < NPLAY; p++) { cur.k[p].init(p, dealt[p], rules.deckSets); cur.hand[p] = dealt[p]; }
  for (int s = 0; s < NSET; s++) cur.active[s] = (s < rules.deckSets);
  int turn = initialTurn(seed, rules.deckSets);
  const int off[4] = {-20, -10, -5, 0};
  for (size_t t = 0; t < ev.size(); t++) {
    ring[t % W] = cur;                       // state BEFORE event t
    const Event& e = ev[t];
    if (e.kind == Kind::Declare) {
      DeclRecord d;
      d.event = int(t); d.actor = e.actor; d.set = e.set; d.correct = e.success;
      d.onTurn = (int(e.actor) == turn);
      d.statedConf = e.confidence;
      for (int j = 0; j < 4; j++) {
        long long idx = (long long)t + off[j];
        if (idx < 0 || (long long)t - idx >= W) continue;
        const Snap& sn = ring[size_t(idx) % W];
        AllocProbe ap = probeSet(sn, e.actor, e.set);
        if (!ap.ok) { buildFail++; continue; }
        d.at[j] = ap; d.have[j] = true;
        if (j == 3) { d.lockedOwn = ap.lockedOwn; d.lockedOpp = ap.lockedOpp; }
      }
      out.push_back(d);
    }
    // advance
    if (e.kind == Kind::Ask) {
      if (e.success) { cur.hand[e.target] &= ~bit(e.card); cur.hand[e.actor] |= bit(e.card); turn = e.actor; }
      else turn = e.target;
    } else if (e.kind == Kind::Pass) {
      turn = e.target;
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      for (int p = 0; p < NPLAY; p++) cur.hand[p] &= ~setMask(e.set);
      cur.active[e.set] = false;
    }
    for (int p = 0; p < NPLAY; p++) cur.k[p].onEvent(e);
  }
}

// Forward counterfactual: replay the same deal with one half-suit's voluntary
// declarations suppressed for a window, and score the declarer's exact
// posterior at +5, +10, +20 events.
struct ForwardRecord {
  int event = 0, actor = 0, set = 0;
  bool lockedOwn = false;
  AllocProbe at0;
  AllocProbe at[3];              // +5, +10, +20
  bool have[3] = {false, false, false};
  bool truncated = false;        // the set left play inside the window anyway
};

inline void forwardScan(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                        const Rules& rules, uint64_t seed, int t0, int actor, int set,
                        ForwardRecord& fr, long long& buildFail) {
  Snap cur;
  for (int p = 0; p < NPLAY; p++) { cur.k[p].init(p, dealt[p], rules.deckSets); cur.hand[p] = dealt[p]; }
  for (int s = 0; s < NSET; s++) cur.active[s] = (s < rules.deckSets);
  const int off[3] = {5, 10, 20};
  fr.event = t0; fr.actor = actor; fr.set = set;
  for (size_t t = 0; t <= ev.size(); t++) {
    int rel = int(t) - t0;
    if (rel == 0) {
      AllocProbe ap = probeSet(cur, actor, set);
      if (!ap.ok) buildFail++;
      fr.at0 = ap; fr.lockedOwn = ap.lockedOwn;
    }
    for (int j = 0; j < 3; j++) if (rel == off[j]) {
      if (!cur.active[set]) { fr.truncated = true; break; }
      AllocProbe ap = probeSet(cur, actor, set);
      if (!ap.ok) { buildFail++; break; }
      fr.at[j] = ap; fr.have[j] = true;
    }
    if (t == ev.size()) break;
    const Event& e = ev[t];
    if (e.kind == Kind::Ask) {
      if (e.success) { cur.hand[e.target] &= ~bit(e.card); cur.hand[e.actor] |= bit(e.card); }
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      for (int p = 0; p < NPLAY; p++) cur.hand[p] &= ~setMask(e.set);
      cur.active[e.set] = false;
    }
    for (int p = 0; p < NPLAY; p++) cur.k[p].onEvent(e);
  }
}

} // namespace probe
} // namespace fish
