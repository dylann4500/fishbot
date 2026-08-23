// Adversarial verification of the P8 turn-transfer claim.
//
// CLAIM (research/v05/results/P8-coordination.md §1.2):
//   "A transfer is individually consequential: the receiver averages 3.36 asks
//    at an 85.1% hit rate, pulling 2.86 cards, against the 34.2% global hit rate."
//
// This file re-derives the statistic from the *event trace* only -- it does not
// reuse probe_coordination_game.hpp's counters -- and adds the controls that the
// original comparison lacks:
//
//   * ALL team possessions (turn-runs), not only post-transfer ones.
//   * Possessions split by how the turn was acquired: Pass (a cardless teammate
//     handed it over) vs Miss (an opponent missed into us) vs Lead.
//   * Both bucketed by PHASE (cards still in play at possession start), so the
//     post-transfer number can be compared against same-phase possessions
//     instead of against the whole-game average.
//
// A possession = a maximal sequence of asks by one team beginning when that team
// acquires the turn.  A miss always crosses teams (asks must target a live
// opponent), so a possession contains k hits then at most one miss.  A
// possession may also be cut short by a declaration that empties the holder, or
// by the game ending.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include <thread>
#include <map>

namespace fish {
namespace probeturn {

struct Bucket {
  long long runs = 0, asks = 0, hits = 0, zeroAsk = 0, endedByMiss = 0;
  void add(int a, int h, bool miss) {
    runs++; asks += a; hits += h; if (!a) zeroAsk++; if (miss) endedByMiss++;
  }
  void merge(const Bucket& o) {
    runs += o.runs; asks += o.asks; hits += o.hits; zeroAsk += o.zeroAsk;
    endedByMiss += o.endedByMiss;
  }
};

// How a possession started.  ACQ_MISS_CL = an ordinary miss-in possession that
// began while at least one player was ALREADY cardless -- the structural
// condition under which a transfer can happen at all.
enum Acq { ACQ_LEAD = 0, ACQ_MISS = 1, ACQ_PASS = 2, ACQ_MISS_CL = 3, ACQ_N = 4 };

struct TurnRunStats {
  long long games = 0, events = 0, asks = 0, hits = 0;
  long long passEvents = 0;
  long long passAsksByOther = 0;   // asks inside a PASS possession NOT made by the receiver
  Bucket by[ACQ_N];
  // phase buckets: cards in play at possession start, bucketed by 6
  std::map<int, Bucket> phase[ACQ_N];
  // matched pairing: for every cards-remaining value, PASS vs MISS
  void merge(const TurnRunStats& o) {
    games += o.games; events += o.events; asks += o.asks; hits += o.hits;
    passEvents += o.passEvents; passAsksByOther += o.passAsksByOther;
    for (int i = 0; i < ACQ_N; i++) {
      by[i].merge(o.by[i]);
      for (auto& kv : o.phase[i]) phase[i][kv.first].merge(kv.second);
    }
  }
};

inline void analyse(const std::vector<Event>& ev, TurnRunStats& st) {
  st.games++;
  int curTeam = -1, curAsks = 0, curHits = 0, curAcq = ACQ_LEAD, curCards = 0;
  bool open = false;

  // cards in play, tracked from the public handCount carried on each event.
  int cards = 0;   // running count; before the first event we do not know it, so
                   // the first possession's phase is filled in from ev[0].
  bool haveCards = false;
  bool anyCardless = false;   // has some seat hit handCount 0 yet?
  int curRecv = -1;

  bool curCardless = false;   // was some seat already cardless when this possession opened?
  auto close = [&](bool miss) {
    if (!open) return;
    st.by[curAcq].add(curAsks, curHits, miss);
    st.phase[curAcq][curCards].add(curAsks, curHits, miss);
    if (curAcq == ACQ_MISS && curCardless) {          // subset, double-counted on purpose
      st.by[ACQ_MISS_CL].add(curAsks, curHits, miss);
      st.phase[ACQ_MISS_CL][curCards].add(curAsks, curHits, miss);
    }
    open = false; curAsks = 0; curHits = 0;
  };

  for (const Event& e : ev) {
    if (e.kind == Kind::Ask) {
      int t = teamOf(e.actor);
      if (!open || curTeam != t) {
        // A team acquired the turn without an explicit marker: either the game's
        // opening lead, or a possession we already closed on a miss.
        close(false);
        curTeam = t;
        curAcq = haveCards ? ACQ_MISS : ACQ_LEAD;
        curCardless = anyCardless;
        curCards = cards; open = true; curAsks = 0; curHits = 0; curRecv = -1;
      }
      if (curAcq == ACQ_PASS && curRecv >= 0 && e.actor != curRecv) st.passAsksByOther++;
      st.asks++; curAsks++;
      if (e.success) { st.hits++; curHits++; }
      if (!e.success) { close(true); curTeam = -1; }
    } else if (e.kind == Kind::Pass) {
      // The cardless holder hands the turn to a live teammate.  Same team keeps
      // the turn; a fresh possession starts for the receiver.
      st.passEvents++;
      close(false);
      curTeam = teamOf(e.target); curAcq = ACQ_PASS; curCards = cards;
      curCardless = true; curRecv = e.target;
      open = true; curAsks = 0; curHits = 0;
    }
    // update cards in play AFTER the event
    int c = 0; for (int p = 0; p < NPLAY; p++) c += e.handCount[p];
    cards = c; haveCards = true;
    for (int p = 0; p < NPLAY; p++) if (!e.handCount[p]) anyCardless = true;
    st.events++;
  }
  close(false);
}

struct TurnRunConfig {
  std::string specA = "v04", specB = "v04";
  int games = 600, rotations = 2, threads = 0;
  uint64_t seed = 31;
  Rules rules;
};

inline TurnRunStats run(const TurnRunConfig& pc) {
  int nT = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nT < 1) nT = 1; nT = std::min(nT, std::max(1, pc.games));
  std::vector<TurnRunStats> local(nT);
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(pc.specA); B[i] = makeAgent(pc.specB); }
      Game game; game.trace.on = true;
      for (int i = t; i < pc.games; i += nT) {
        uint64_t s = mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < pc.rotations; rot++) {
          int orient = (pc.rotations == 2) ? rot : (rot / 3);
          int shift  = (pc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          game.trace.events.clear();
          game.run(s, pc.rules, ag);
          analyse(game.trace.events, local[t]);
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  TurnRunStats tot;
  for (int t = 0; t < nT; t++) tot.merge(local[t]);
  return tot;
}

} // namespace probeturn
} // namespace fish
