// P2 probe: anatomy of the forced endgame.
//
// Hooks Game::observer (the only non-invasive instrumentation point) and, at
// every public event that leaves one team cardless with half-suits still live,
// replays the exact ladder of Game::forcedEndgame against the LIVE agents to
// learn which rung fires, who declares, what they name, and how that compares
// with the truth and with the exact BlockDP posterior.
//
// Nothing here is on a decision path: every query is a pure function of an
// agent's Knowledge, so re-asking the question does not perturb the game.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include "blockdp.hpp"
#include <thread>
#include <mutex>
#include <algorithm>

namespace fish {

struct FEDecl {
  int gameId = 0, rot = 0;
  int ordinal = 0;            // which forced declaration within this game
  int nActiveAtDecl = 0;
  int set = 0, declarer = 0, declHand = 0, declTeam = 0;
  int rung = -1;              // index into Rules::forcedTh
  double th = 0, conf = 0;
  int named[SETSZ] = {0,0,0,0,0,0};
  int truth[SETSZ] = {0,0,0,0,0,0};
  bool correct = false;
  bool predictedMatched = false;   // probe's replay picked the same (set, seat)
  // Why the named allocation is (in)consistent with the declarer's own knowledge
  bool violMask = false;      // names a card at a seat its own Knowledge excludes
  bool violCap  = false;      // names more unresolved cards at a seat than capacity
  bool violDisj = false;      // breaks one of its own ask-legality certificates
  bool zeroPost = false;      // exact posterior probability of the named alloc == 0
  double pNamed = -1, pBest = -1;
  bool bestIsTruth = false;
  int nUnknown = 0;           // unresolved cards of this set for the declarer
  bool blockBuilt = false;
  int maxOver = 0;            // worst per-seat capacity overshoot
  double pTrue = -1;          // exact posterior mass on the TRUE allocation
  int nCapSeats = 0;          // teammates with spare capacity for this half-suit
  int nNamedSeats = 0;        // distinct seats used for the UNRESOLVED cards
  int nTrueSeats = 0;         // distinct seats the truth uses for those cards
  double margSpread = -1;     // max-minus-second marginal, averaged over unresolved cards
  double wAlloc = -1;         // pAlloc that willingForced() would report (threshold 0)
  bool   wOk = false;         // evaluateSet returned ok at all
  bool   wFeasible = false;   // that allocation has non-zero exact posterior
  bool   wIsTruth = false;
};

struct FEGame {
  int gameId = 0, rot = 0;
  int declaringTeam = -1;
  int setsAtEntry = 0;
  int unresolvedAtEntry = 0;
  int handAtEntry[3] = {0,0,0};
  // counterfactual policies, all using only the declaring team's knowledge
  int engineCorrect = 0, engineDecls = 0;
  int feasIdxCorrect = 0;        // set order 0..8, lowest-seat declarer, feasible argmax
  int feasBestSeatCorrect = 0;   // set order 0..8, best-declarer, feasible argmax
  int feasGreedyCorrect = 0;     // globally greedy (set,seat) by confidence
  int feasGreedyNoReveal = 0;    // greedy but without re-deriving after each reveal
  int nSets = 0;
};

struct FEStats {
  std::vector<FEDecl> decls;
  std::vector<FEGame> games;
  long long gamesRun = 0, gamesWithForced = 0, gamesTouched = 0;
  long long entrySets[10] = {0}, entrySetsForced[10] = {0};
  void merge(const FEStats& o) {
    decls.insert(decls.end(), o.decls.begin(), o.decls.end());
    games.insert(games.end(), o.games.begin(), o.games.end());
    gamesRun += o.gamesRun; gamesWithForced += o.gamesWithForced; gamesTouched += o.gamesTouched;
    for (int i = 0; i < 10; i++) { entrySets[i] += o.entrySets[i]; entrySetsForced[i] += o.entrySetsForced[i]; }
  }
};

// ---------------------------------------------------------------- helpers

inline int truthHolderOf(const uint64_t* hand, int c) {
  for (int p = 0; p < NPLAY; p++) if (hand[p] & bit(c)) return p;
  return -1;
}

// exact probability of a fully named allocation of half-suit s under k
inline double exactNamedProb(BlockDP& bd, const Knowledge& k, int s, const uint8_t* owner) {
  // resolved cards must be named correctly or the probability is zero
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (k.owner[c] < NPLAY && k.owner[c] != owner[i]) return 0.0;
  }
  int seats[SETSZ], n = 0;
  uint64_t un = k.unresolved & setMask(s);
  while (un) { int c = __builtin_ctzll(un); un &= un - 1; seats[n++] = owner[idxIn(c)]; }
  if (!n) return 1.0;
  return bd.allocationProbability(k, s, seats);
}

// best feasible allocation for `team` under k; writes full 6-card owner vector
inline double exactBestAlloc(BlockDP& bd, const Knowledge& k, int s, int teamMask, uint8_t* owner) {
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    owner[i] = (k.owner[c] < NPLAY) ? k.owner[c] : uint8_t(__builtin_ctz(teamMask));
  }
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (k.owner[c] < NPLAY && !(teamMask & (1u << k.owner[c]))) return 0.0;
  }
  int outC[SETSZ], outS[SETSZ], n2 = 0;
  double p = bd.bestTeamAllocation(s, teamMask, outC, outS, n2);
  for (int i = 0; i < n2; i++) owner[idxIn(outC[i])] = uint8_t(outS[i]);
  return p;
}

// -------------------------------------------------------------- the probe

struct FEProbe {
  const Rules* rules = nullptr;
  FEStats* out = nullptr;
  int gameId = 0, rot = 0;
  int declaringTeam = -1;
  int ordinal = 0;
  bool pending = false;
  FEDecl cur{};
  FEGame gm{};
  bool entered = false;
  // snapshot of the endgame entry, for the counterfactual replay
  Knowledge entryK[NPLAY];
  uint64_t entryHand[NPLAY];
  bool entrySetActive[NSET];

  void reset(int gid, int r, const Rules& rl, FEStats* o) {
    rules = &rl; out = o; gameId = gid; rot = r;
    declaringTeam = -1; ordinal = 0; pending = false; entered = false;
    gm = FEGame{}; gm.gameId = gid; gm.rot = r;
  }

  // Replay Game::forcedEndgame's ladder for ONE declaration against live agents.
  bool predictNext(const Game& G, int fteam, int& outSet, int& outSeat,
                   int& outRung, double& outTh, Declaration& outD, double& outConf) {
    for (int ti = 0; ti < G.rules.nForcedTh; ti++) {
      double th = G.rules.forcedTh[ti];
      for (int s = 0; s < NSET; s++) {
        if (!G.g.pub.setActive[s]) continue;
        for (int p = fteam; p < NPLAY; p += 2) {
          if (!G.rules.cardlessMayDeclare && !G.g.pub.handCount[p]) continue;
          Agent* a = const_cast<Agent*>(G.agents[p]);
          Declaration d{}; double conf = 0;
          if (th < 0) a->bestGuess(G.g.pub, s, d, conf);
          else if (!a->willingForced(G.g.pub, s, d, conf, th)) continue;
          d.set = uint8_t(s);
          bool ok = true;
          for (int i = 0; i < SETSZ; i++) if (teamOf(d.owner[i]) != fteam) ok = false;
          if (!ok) continue;
          outSet = s; outSeat = p; outRung = ti; outTh = th; outD = d; outConf = conf;
          return true;
        }
      }
    }
    return false;
  }

  void score(const Game& G, int fteam) {
    int s = cur.set, p = cur.declarer;
    const Knowledge& k = G.agents[p]->k;
    int teamMask = 0;
    for (int q = 0; q < NPLAY; q++) if (teamOf(q) == fteam) teamMask |= 1 << q;
    cur.nUnknown = __builtin_popcountll(k.unresolved & setMask(s));
    for (int i = 0; i < SETSZ; i++) cur.truth[i] = truthHolderOf(G.g.hand, cardOf(s, i));

    // 1. consistency of the named allocation with the declarer's own Knowledge
    uint8_t capv[NPLAY]; k.capacities(capv);
    int used[NPLAY] = {0,0,0,0,0,0};
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i), q = cur.named[i];
      if (k.owner[c] < NPLAY) { if (k.owner[c] != q) cur.violMask = true; }
      else if (!(k.mask[c] & (1u << q))) cur.violMask = true;
      else used[q]++;
    }
    for (int q = 0; q < NPLAY; q++) if (used[q] > capv[q]) {
      cur.violCap = true; cur.maxOver = std::max(cur.maxOver, used[q] - capv[q]);
    }
    for (const auto& dj : k.disj) {
      uint64_t inB = dj.cards & setMask(s) & k.unresolved;
      if (!inB) continue;
      bool sat = false;
      uint64_t u = inB;
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        if (cur.named[idxIn(c)] == dj.player) sat = true; }
      if (!sat) cur.violDisj = true;
    }

    // 2. exact posterior: probability of what was named, and the ceiling
    BlockDP bd;
    cur.blockBuilt = bd.build(k);
    if (cur.blockBuilt) {
      uint8_t nm[SETSZ], bst[SETSZ];
      for (int i = 0; i < SETSZ; i++) nm[i] = uint8_t(cur.named[i]);
      cur.pNamed = exactNamedProb(bd, k, s, nm);
      cur.zeroPost = (cur.pNamed <= 0);
      cur.pBest = exactBestAlloc(bd, k, s, teamMask, bst);
      bool same = true;
      for (int i = 0; i < SETSZ; i++) if (bst[i] != cur.truth[i]) same = false;
      cur.bestIsTruth = same;
      uint8_t tr[SETSZ];
      for (int i = 0; i < SETSZ; i++) tr[i] = uint8_t(cur.truth[i]);
      cur.pTrue = exactNamedProb(bd, k, s, tr);
    }
    // what the WILLINGNESS rungs would have seen: evaluateSet's own pAlloc
    {
      Agent* a = const_cast<Agent*>(G.agents[p]);
      Declaration wd{}; double wc = -1;
      if (a->willingForced(G.g.pub, s, wd, wc, 0.0)) {
        cur.wOk = true; cur.wAlloc = wc;
        bool same = true;
        for (int i = 0; i < SETSZ; i++) if (wd.owner[i] != cur.truth[i]) same = false;
        cur.wIsTruth = same;
        if (cur.blockBuilt) {
          uint8_t wn[SETSZ];
          for (int i = 0; i < SETSZ; i++) wn[i] = wd.owner[i];
          cur.wFeasible = exactNamedProb(bd, k, s, wn) > 0;
        }
      }
    }
    // structural signature: how many seats the guess spreads the unknowns over
    {
      int nm = 0, tsm = 0, capm = 0;
      double spread = 0; int nsp = 0;
      uint64_t u = k.unresolved & setMask(s);
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        nm |= 1 << cur.named[idxIn(c)];
        tsm |= 1 << cur.truth[idxIn(c)];
        double m1 = -1, m2 = -1;
        for (int q = 0; q < NPLAY; q++) if (teamMask & (1 << q)) {
          double v = G.agents[p]->k.mask[c] & (1u << q) ? 1.0 : 0.0; (void)v;
        }
        // marginal spread under the agent's own belief
        const V04Agent* va = dynamic_cast<const V04Agent*>(G.agents[p]);
        if (va) {
          for (int q = 0; q < NPLAY; q++) if (teamMask & (1 << q)) {
            double v = va->bel.marg[c][q];
            if (v > m1) { m2 = m1; m1 = v; } else if (v > m2) m2 = v;
          }
          if (m1 >= 0 && m2 >= 0) { spread += (m1 - m2); nsp++; }
        }
      }
      for (int q = 0; q < NPLAY; q++) if ((teamMask & (1 << q)) && capv[q] > 0) capm |= 1 << q;
      cur.nNamedSeats = __builtin_popcount(nm);
      cur.nTrueSeats = __builtin_popcount(tsm);
      cur.nCapSeats = __builtin_popcount(capm);
      if (nsp) cur.margSpread = spread / nsp;
    }
  }

  void onEvent(const Game& G) {
    const Event& e = G.g.pub.history.back();
    if (pending && (e.kind == Kind::ForcedDeclare)) {
      cur.correct = e.success;
      cur.predictedMatched = (int(e.set) == cur.set && int(e.actor) == cur.declarer);
      if (!cur.predictedMatched) {   // fall back to what actually happened
        cur.set = e.set; cur.declarer = e.actor;
        for (int i = 0; i < SETSZ; i++) cur.named[i] = e.decl.owner[i];
        cur.conf = e.confidence;
      }
      gm.engineDecls++; if (e.success) gm.engineCorrect++;
      out->decls.push_back(cur);
      pending = false;
      ordinal++;
    }
    bool alive0 = G.g.pub.teamAlive(0), alive1 = G.g.pub.teamAlive(1);
    if (!G.g.pub.activeSets()) return;
    if (alive0 && alive1) return;
    if (ordinal == 0) {   // re-snapshot until the first forced declaration fires
      declaringTeam = alive0 ? 0 : 1;
      gm.declaringTeam = declaringTeam;
      gm.setsAtEntry = G.g.pub.activeSets();
      gm.unresolvedAtEntry = __builtin_popcountll(G.agents[declaringTeam]->k.unresolved);
      int j = 0;
      for (int p = declaringTeam; p < NPLAY; p += 2) gm.handAtEntry[j++] = G.g.pub.handCount[p];
      for (int p = 0; p < NPLAY; p++) { entryK[p] = G.agents[p]->k; entryHand[p] = G.g.hand[p]; }
      for (int s = 0; s < NSET; s++) entrySetActive[s] = G.g.pub.setActive[s];
      entered = true;
    }
    int fteam = declaringTeam;
    int s, seat, rung; double th, conf; Declaration d{};
    if (!predictNext(G, fteam, s, seat, rung, th, d, conf)) return;
    cur = FEDecl{};
    cur.gameId = gameId; cur.rot = rot; cur.ordinal = ordinal;
    cur.nActiveAtDecl = G.g.pub.activeSets();
    cur.set = s; cur.declarer = seat; cur.declTeam = fteam;
    cur.declHand = G.g.pub.handCount[seat];
    cur.rung = rung; cur.th = th; cur.conf = conf;
    for (int i = 0; i < SETSZ; i++) cur.named[i] = d.owner[i];
    score(G, fteam);
    pending = true;
  }

  // ------------------------------------------------- counterfactual replay
  // mode 0: set index order, lowest live seat declares
  // mode 1: set index order, most confident seat declares
  // mode 2: globally greedy over (set, seat) by exact confidence
  // mode 3: as 2 but the whole schedule is fixed up front (no re-derivation)
  int replay(int mode) {
    if (!entered) return 0;
    Knowledge k[NPLAY];
    uint64_t hand[NPLAY];
    bool active[NSET];
    uint8_t hc[NPLAY];
    for (int p = 0; p < NPLAY; p++) { k[p] = entryK[p]; hand[p] = entryHand[p]; }
    for (int s = 0; s < NSET; s++) active[s] = entrySetActive[s];
    for (int p = 0; p < NPLAY; p++) hc[p] = uint8_t(popcount64(hand[p]));
    int fteam = declaringTeam, teamMask = 0;
    for (int q = 0; q < NPLAY; q++) if (teamOf(q) == fteam) teamMask |= 1 << q;
    int correct = 0;
    // mode 3: compute the whole schedule up front from the entry knowledge
    std::vector<std::pair<double,std::pair<int,int>>> sched;
    if (mode == 3) {
      for (int s = 0; s < NSET; s++) {
        if (!active[s]) continue;
        double best = -1; int bs = fteam;
        for (int p = fteam; p < NPLAY; p += 2) {
          BlockDP bd; if (!bd.build(k[p])) continue;
          uint8_t ow[SETSZ]; double pv = exactBestAlloc(bd, k[p], s, teamMask, ow);
          if (pv > best) { best = pv; bs = p; }
        }
        sched.push_back({best, {s, bs}});
      }
      std::sort(sched.begin(), sched.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
    }
    for (int step = 0; step < NSET + 1; step++) {
      int nAct = 0; for (int s = 0; s < NSET; s++) nAct += active[s];
      if (!nAct) break;
      int pickS = -1, pickP = -1; double pickV = -1; uint8_t pickOw[SETSZ] = {0,0,0,0,0,0};
      if (mode == 3) {
        for (auto& it : sched) if (active[it.second.first]) { pickS = it.second.first; pickP = it.second.second; break; }
        if (pickS < 0) break;
        BlockDP bd; if (!bd.build(k[pickP])) { // fall back to any teammate
          for (int p = fteam; p < NPLAY && !bd.ok; p += 2) { pickP = p; bd.build(k[p]); } }
        pickV = exactBestAlloc(bd, k[pickP], pickS, teamMask, pickOw);
      } else {
        for (int s = 0; s < NSET; s++) {
          if (!active[s]) continue;
          for (int p = fteam; p < NPLAY; p += 2) {
            BlockDP bd; if (!bd.build(k[p])) continue;
            uint8_t ow[SETSZ]; double pv = exactBestAlloc(bd, k[p], s, teamMask, ow);
            bool take = false;
            if (mode == 0) take = (pickS < 0);                       // first set, first seat
            else if (mode == 1) take = (pickS < 0 || (s == pickS && pv > pickV));
            else take = (pv > pickV);
            if (take) { pickS = s; pickP = p; pickV = pv; for (int i = 0; i < SETSZ; i++) pickOw[i] = ow[i]; }
          }
          if (mode <= 1 && pickS >= 0) break;                        // index order
        }
      }
      if (pickS < 0) break;
      bool ok = true;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(pickS, i);
        if (!(hand[pickOw[i]] & bit(c))) { ok = false; break; }
      }
      if (ok) correct++;
      Event e{}; e.kind = Kind::ForcedDeclare; e.actor = uint8_t(pickP);
      e.set = uint8_t(pickS); e.success = ok; e.decl.set = uint8_t(pickS);
      for (int i = 0; i < SETSZ; i++) e.decl.owner[i] = pickOw[i];
      for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(pickS);
      for (int p = 0; p < NPLAY; p++) hc[p] = uint8_t(popcount64(hand[p]));
      for (int p = 0; p < NPLAY; p++) e.handCount[p] = hc[p];
      active[pickS] = false;
      for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
    }
    return correct;
  }

  void finish() {
    if (!entered) return;
    gm.nSets = gm.setsAtEntry;
    gm.feasIdxCorrect      = replay(0);
    gm.feasBestSeatCorrect = replay(1);
    gm.feasGreedyCorrect   = replay(2);
    gm.feasGreedyNoReveal  = replay(3);
    int b = std::min(9, gm.setsAtEntry);
    out->entrySets[b]++;
    if (gm.engineDecls > 0) out->entrySetsForced[b]++;
    if (gm.engineDecls > 0) { out->games.push_back(gm); out->gamesWithForced++; }
    else out->gamesTouched++;
  }
};


// --------------------------------------------------------- aliasing check
// BlockDP::build() parks all of its tables in a thread_local Buffers pool
// (blockdp.hpp:88-95, 172-176), so two BlockDP objects alive in the same thread
// share storage.  V04Agent owns one `block` member each and only rebuilds when
// `dirty` (v04.hpp:163), and Game::forcedEndgame / Game::declarationRound poll
// several agents back to back between events -- so the second agent's build
// silently repoints the first agent's tables.  This probes for that directly.
struct AliasReport { long long checks = 0, mismatches = 0, rawMismatches = 0; double worst = 0; };

inline AliasReport blockAliasCheck(const std::string& spec, int games, uint64_t seed, const Rules& rules) {
  AliasReport rep;
  std::unique_ptr<Agent> A[3], B[3];
  for (int i = 0; i < 3; i++) { A[i] = makeAgent(spec); B[i] = makeAgent(spec); }
  Game game;
  Agent* ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == 0) ? A[p / 2].get() : B[p / 2].get();
  game.observer = [&](const Game& G) {
    if (G.g.pub.nEvents % 17) return;
    int s = -1;
    for (int t = 0; t < NSET; t++) if (G.g.pub.setActive[t]) { s = t; break; }
    if (s < 0) return;
    BlockDP b0, b1;
    if (!b0.build(G.agents[0]->k)) return;
    if (b0.nGroups < 1) return;
    // Read-only fingerprint of b0's own tables, taken twice, with an unrelated
    // build in between.  No query is issued, so nothing can fault.
    double x = b0.Z; int gs = b0.groups[0].set, gn = b0.groups[0].nCards;
    uint8_t gm = b0.groups[0].cmask[0];
    if (!b1.build(G.agents[2]->k)) return;
    double y = b0.Z; int gs2 = b0.groups[0].set, gn2 = b0.groups[0].nCards;
    uint8_t gm2 = b0.groups[0].cmask[0];
    rep.checks++;
    if (gs != gs2 || gn != gn2 || gm != gm2) {
      rep.rawMismatches++;
    }
    // The check that matters for behaviour: issue the SAME QUERY on b0 before
    // and after the unrelated build.  v0.6's E2 guard re-derives b0's tables
    // lazily when it finds its generation stamp stale, so the answers must
    // agree exactly even though the raw pointers still address the shared pool.
    BlockDP b2;
    if (!b2.build(G.agents[0]->k)) return;
    double q0 = b2.teamOwnsProbability(s, 0x15);
    BlockDP b3;
    if (!b3.build(G.agents[2]->k)) return;
    double q1 = b2.teamOwnsProbability(s, 0x15);
    if (q0 != q1) { rep.mismatches++; rep.worst = std::max(rep.worst, std::fabs(q0 - q1)); }
    (void)x; (void)y;
  };
  for (int i = 0; i < games; i++) game.run(mixSeed(seed, uint64_t(i) + 5), rules, ag);
  return rep;
}

struct FEConfig {
  std::string specA = "v04", specB = "v04";
  int games = 300, rotations = 2, threads = 0;
  uint64_t seed = 31;
  Rules rules;
};

inline FEStats runForcedProbe(const FEConfig& fc) {
  int nT = fc.threads > 0 ? fc.threads : int(std::thread::hardware_concurrency());
  if (nT < 1) nT = 1;
  nT = std::min(nT, std::max(1, fc.games));
  std::vector<FEStats> local(nT);
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(fc.specA); B[i] = makeAgent(fc.specB); }
      Game game;
      game.trace.on = true;
      FEProbe probe;
      game.observer = [&](const Game& G) { probe.onEvent(G); };
      for (int i = t; i < fc.games; i += nT) {
        uint64_t s = mixSeed(fc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < fc.rotations; rot++) {
          int orient = (fc.rotations == 2) ? rot : (rot / 3);
          int shift  = (fc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          game.trace.events.clear();
          probe.reset(i, rot, fc.rules, &local[t]);
          local[t].gamesRun++;
          game.run(s, fc.rules, ag);
          probe.finish();
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  FEStats tot;
  for (int t = 0; t < nT; t++) tot.merge(local[t]);
  return tot;
}

inline void printForcedProbe(const FEStats& st, std::ostream& os) {
  auto pc = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  long long n = (long long)st.decls.size();
  long long wrong = 0, matched = 0, vm = 0, vc = 0, vd = 0, zp = 0, bt = 0, anyViol = 0;
  long long byRung[9] = {0}, wrongByRung[9] = {0};
  double sumBest = 0, sumNamed = 0;
  long long nBlock = 0;
  long long unkHist[8] = {0};
  for (const auto& d : st.decls) {
    if (!d.correct) wrong++;
    if (d.predictedMatched) matched++;
    if (d.violMask) vm++;
    if (d.violCap) vc++;
    if (d.violDisj) vd++;
    if (d.zeroPost) zp++;
    if (d.violMask || d.violCap || d.violDisj) anyViol++;
    if (d.bestIsTruth) bt++;
    int r = d.rung < 0 ? 8 : d.rung; byRung[r]++; if (!d.correct) wrongByRung[r]++;
    if (d.blockBuilt) { nBlock++; sumBest += d.pBest; sumNamed += std::max(0.0, d.pNamed); }
    unkHist[std::min(7, d.nUnknown)]++;
  }
  os << "games                 " << st.gamesRun << "\n";
  os << "games w/ forced end   " << st.gamesWithForced << " (" << pc(st.gamesWithForced, st.gamesRun) << "%)\n";
  os << "games that PASSED THROUGH a cardless-team state but finished voluntarily  " << st.gamesTouched << "\n";
  os << "forced declarations   " << n << "   wrong " << wrong << " (" << pc(wrong, n) << "%)\n";
  os << "probe replay matched  " << matched << " / " << n << "\n";
  double rungTh[9]; for (int r = 0; r < 9; r++) rungTh[r] = -99;
  for (const auto& d : st.decls) { int r = d.rung < 0 ? 8 : d.rung; rungTh[r] = d.th; }
  os << "\nby ladder rung\n";
  for (int r = 0; r < 9; r++) if (byRung[r])
    os << "  rung " << r << "  th=" << rungTh[r] << "   n=" << byRung[r]
       << "   wrong=" << wrongByRung[r] << " (" << pc(wrongByRung[r], byRung[r]) << "%)\n";
  os << "\n";
  os << "named alloc inconsistent with the DECLARER'S OWN knowledge\n";
  os << "  mask violation      " << vm << " (" << pc(vm, n) << "%)\n";
  os << "  capacity violation  " << vc << " (" << pc(vc, n) << "%)\n";
  os << "  certificate broken  " << vd << " (" << pc(vd, n) << "%)\n";
  os << "  ANY of the three    " << anyViol << " (" << pc(anyViol, n) << "%)\n";
  os << "  exact posterior = 0 " << zp << " (" << pc(zp, n) << "%)\n";
  os << "\nceiling (exact BlockDP, best FEASIBLE allocation)\n";
  os << "  mean P(named)       " << (nBlock ? sumNamed / nBlock : 0) << "\n";
  os << "  mean P(best feas)   " << (nBlock ? sumBest / nBlock : 0) << "\n";
  os << "  best feas == truth  " << bt << " (" << pc(bt, n) << "%)\n";
  long long collapse = 0, splitTruth = 0; double sumTrue = 0, sumSpread = 0; long long nSp = 0;
  for (const auto& d : st.decls) {
    if (d.nNamedSeats < d.nTrueSeats) collapse++;
    if (d.nTrueSeats > 1) splitTruth++;
    if (d.pTrue >= 0) sumTrue += d.pTrue;
    if (d.margSpread >= 0) { sumSpread += d.margSpread; nSp++; }
  }
  os << "  mean P(TRUE alloc)  " << (nBlock ? sumTrue / nBlock : 0) << "\n";
  os << "  guess uses FEWER seats than the truth  " << collapse << " (" << pc(collapse, n) << "%)\n";
  os << "  truth splits unknowns over >1 seat     " << splitTruth << " (" << pc(splitTruth, n) << "%)\n";
  os << "  mean (top marginal - 2nd marginal)     " << (nSp ? sumSpread / nSp : 0) << "\n";
  long long wok = 0, wpos = 0, wfeas = 0, wtruth = 0; double wsum = 0;
  for (const auto& d : st.decls) { if (d.wOk) { wok++; wsum += d.wAlloc; if (d.wAlloc > 0) wpos++; }
    if (d.wFeasible) wfeas++; if (d.wIsTruth) wtruth++; }
  os << "\nwhat the WILLINGNESS rungs saw (evaluateSet at press=2)\n";
  os << "  evaluateSet ok        " << wok << " / " << n << "\n";
  os << "  its pAlloc > 0        " << wpos << " (" << pc(wpos, n) << "%)   mean pAlloc " << (wok ? wsum / wok : 0) << "\n";
  os << "  its alloc feasible    " << wfeas << " (" << pc(wfeas, n) << "%)\n";
  os << "  its alloc == truth    " << wtruth << " (" << pc(wtruth, n) << "%)\n";
  os << "\nlive half-suits at the moment a team went cardless (all entries)\n";
  for (int i = 0; i < 10; i++) if (st.entrySets[i])
    os << "  " << i << " sets: " << st.entrySets[i] << " entries, of which " << st.entrySetsForced[i]
       << " actually reached a forced declaration\n";
  os << "\nunresolved cards in the declared half-suit\n";
  for (int u = 0; u < 8; u++) if (unkHist[u]) os << "  " << u << (u == 7 ? "+" : " ") << "  " << unkHist[u] << "\n";
  long long ge = 0, gd = 0, f0 = 0, f1 = 0, f2 = 0, f3 = 0, tot = 0;
  for (const auto& g : st.games) { ge += g.engineCorrect; gd += g.engineDecls;
    f0 += g.feasIdxCorrect; f1 += g.feasBestSeatCorrect; f2 += g.feasGreedyCorrect;
    f3 += g.feasGreedyNoReveal; tot += g.nSets; }
  os << "\ncounterfactual policies over " << st.games.size() << " forced endgames, "
     << tot << " half-suits\n";
  os << "  v0.4 engine                       " << ge << " / " << gd << "  (" << pc(ge, gd) << "%)\n";
  os << "  feasible argmax, index order      " << f0 << " / " << tot << "  (" << pc(f0, tot) << "%)\n";
  os << "  + best declarer per set           " << f1 << " / " << tot << "  (" << pc(f1, tot) << "%)\n";
  os << "  + confidence-greedy ORDER         " << f2 << " / " << tot << "  (" << pc(f2, tot) << "%)\n";
  os << "  greedy order fixed at entry       " << f3 << " / " << tot << "  (" << pc(f3, tot) << "%)\n";
}

} // namespace fish
