// Pathology diagnostics for FishBot.
//
// Replays a traced game against the ground-truth deal and reconstructs each
// seat's Knowledge object at every decision point, so that questions like "did
// the actor already know this ask was a guaranteed miss?" can be answered from
// public information only, without instrumenting the policy.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include "v07_stall.hpp"
#include <thread>
#include <mutex>

namespace fish {

struct PathologyStats {
  long long games = 0, events = 0, asks = 0, hits = 0;
  long long deadAsks = 0;          // actor could prove the target lacks the card
  long long nearDeadAsks = 0;      // hard-consistent, but the card is very unlikely there
  long long repeatAsks = 0;        // exact (actor,target,card) tuple seen before
  long long repeatPairAsks = 0;    // (actor,target,half-suit) seen before this game
  long long starvedTurns = 0;      // actor had NO ask with a live possibility
  long long deadRunTotal = 0;      // sum of maximal consecutive-dead-ask run lengths
  long long deadRuns = 0;
  int longestDeadRun = 0;
  long long decls = 0, declWrong = 0;
  long long declsLate = 0, declWrongLate = 0;    // at/after the forcing horizon
  long long declsForced = 0, declWrongForced = 0;
  long long limitHits = 0;
  long long lockedInfoAsks = 0;    // legal asks inside a half-suit our team already owns outright
  long long gamesWithDeadRun = 0;  // games containing a dead run of >= 6
  // ---- v0.7 phase 3 (K3): the stall detector, RECONSTRUCTED from the trace ---
  // These are computed by the replay from (each seat's opening hand + the public
  // event stream), independently of the agent, which is a mechanical check that
  // the detector is a function of exactly that and of nothing else.
  long long stallDecls = 0, stallDeclWrong = 0;   // declarations made while the
                                                  // declarer was stalled >= K
  long long stall2Decls = 0, stall2DeclWrong = 0; // ... stalled >= K2
  long long stallEvents = 0;                      // (seat, event) pairs at press>=1
  int       longestStall = 0;                     // longest no-progress run, any seat
  std::vector<int> stallHist;                     // per game: longest stall run
  std::vector<int> eventHist;      // events per game
  std::vector<int> deadRunHist;

  void merge(const PathologyStats& o) {
    games += o.games; events += o.events; asks += o.asks; hits += o.hits;
    deadAsks += o.deadAsks; nearDeadAsks += o.nearDeadAsks;
    repeatAsks += o.repeatAsks; repeatPairAsks += o.repeatPairAsks;
    starvedTurns += o.starvedTurns;
    deadRunTotal += o.deadRunTotal; deadRuns += o.deadRuns;
    longestDeadRun = std::max(longestDeadRun, o.longestDeadRun);
    decls += o.decls; declWrong += o.declWrong;
    declsLate += o.declsLate; declWrongLate += o.declWrongLate;
    declsForced += o.declsForced; declWrongForced += o.declWrongForced;
    limitHits += o.limitHits;
    lockedInfoAsks += o.lockedInfoAsks;
    gamesWithDeadRun += o.gamesWithDeadRun;
    stallDecls += o.stallDecls; stallDeclWrong += o.stallDeclWrong;
    stall2Decls += o.stall2Decls; stall2DeclWrong += o.stall2DeclWrong;
    stallEvents += o.stallEvents;
    longestStall = std::max(longestStall, o.longestStall);
    stallHist.insert(stallHist.end(), o.stallHist.begin(), o.stallHist.end());
    eventHist.insert(eventHist.end(), o.eventHist.begin(), o.eventHist.end());
    deadRunHist.insert(deadRunHist.end(), o.deadRunHist.begin(), o.deadRunHist.end());
  }
};

// Replay one traced game and score it.  `dealt` is the true opening deal.
inline void analyseTrace(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                         const Rules& rules, bool hitLimit, PathologyStats& st) {
  Knowledge k[NPLAY];
  for (int p = 0; p < NPLAY; p++) k[p].init(p, dealt[p], rules.deckSets);
  PublicState pub{};
  pub.rules = rules;
  for (int p = 0; p < NPLAY; p++) pub.handCount[p] = uint8_t(popcount64(dealt[p]));
  for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < rules.deckSets);
  uint64_t hand[NPLAY];
  for (int p = 0; p < NPLAY; p++) hand[p] = dealt[p];

  bool seenAsk[NPLAY][NCARD][NPLAY];
  memset(seenAsk, 0, sizeof(seenAsk));
  bool seenPair[NPLAY][NSET][NPLAY];
  memset(seenPair, 0, sizeof(seenPair));

  int run = 0, longest = 0;
  int nEvents = 0;
  // K3: per-seat stall reconstruction.  Mirrors V05Agent::stallStep exactly.
  const int K3K  = k3stall().K.load(std::memory_order_relaxed);
  const int K3K2 = k3stall().K2.load(std::memory_order_relaxed);
  int      k3last[NPLAY];
  uint64_t k3sig[NPLAY];
  int      k3longest = 0;
  for (int p = 0; p < NPLAY; p++) { k3last[p] = 0; k3sig[p] = k3HardSig(k[p]); }
  st.games++;
  if (hitLimit) st.limitHits++;

  for (const Event& e : ev) {
    if (e.kind == Kind::Ask) {
      const Knowledge& kk = k[e.actor];
      int c = e.card, t = e.target;
      bool dead = (kk.owner[c] < NPLAY) ? (kk.owner[c] != t)
                                        : !(kk.mask[c] & (1u << t));
      st.asks++;
      if (e.success) st.hits++;
      if (dead) {
        st.deadAsks++;
        run++;
        longest = std::max(longest, run);
      } else {
        if (run) { st.deadRuns++; st.deadRunTotal += run; st.deadRunHist.push_back(run); }
        run = 0;
      }
      if (seenAsk[e.actor][c][t]) st.repeatAsks++;
      seenAsk[e.actor][c][t] = true;
      if (seenPair[e.actor][e.set][t]) st.repeatPairAsks++;
      seenPair[e.actor][e.set][t] = true;

      // Did the actor have any ask at all with a live possibility?
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, kk.myHand, e.actor, buf);
      bool anyLive = false;
      for (int i = 0; i < n && !anyLive; i++) {
        int cc = buf[i].card, tt = buf[i].target;
        bool d2 = (kk.owner[cc] < NPLAY) ? (kk.owner[cc] != tt) : !(kk.mask[cc] & (1u << tt));
        if (!d2) anyLive = true;
      }
      if (!anyLive) st.starvedTurns++;

      // Asks inside a half-suit this actor's team provably owns outright: they
      // are guaranteed misses, but they still emit an ask-legality certificate,
      // so they are the information-bearing move a deadlocked team could make.
      int tm = 0;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == teamOf(e.actor)) tm |= 1 << p;
      bool teamOwnsAll = true;
      for (int i = 0; i < SETSZ; i++) {
        int cc = cardOf(e.set, i);
        int truth = -1;
        for (int q = 0; q < NPLAY; q++) if (hand[q] & bit(cc)) truth = q;
        if (truth < 0 || !(tm & (1 << truth))) { teamOwnsAll = false; break; }
      }
      if (teamOwnsAll) st.lockedInfoAsks++;
    }
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      st.decls++;
      if (!e.success) st.declWrong++;
      if (e.kind == Kind::ForcedDeclare) { st.declsForced++; if (!e.success) st.declWrongForced++; }
      if (nEvents >= 220) { st.declsLate++; if (!e.success) st.declWrongLate++; }
      if (K3K > 0) {
        int stalled = nEvents - k3last[e.actor];
        if (stalled >= K3K2) { st.stall2Decls++; if (!e.success) st.stall2DeclWrong++; }
        if (stalled >= K3K)  { st.stallDecls++;  if (!e.success) st.stallDeclWrong++; }
      }
    }

    // Apply the event to the true hands and to every observer.
    if (e.kind == Kind::Ask && e.success) {
      hand[e.target] &= ~bit(e.card);
      hand[e.actor] |= bit(e.card);
    }
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
      pub.setActive[e.set] = false;
    }
    for (int p = 0; p < NPLAY; p++) pub.handCount[p] = e.handCount[p];
    for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
    nEvents++;
    if (K3K > 0) {
      for (int p = 0; p < NPLAY; p++) {
        uint64_t sg = k3HardSig(k[p]);
        if (sg != k3sig[p]) { k3sig[p] = sg; k3last[p] = nEvents; }
        else {
          int stalled = nEvents - k3last[p];
          if (stalled >= K3K) st.stallEvents++;
          k3longest = std::max(k3longest, stalled);
        }
      }
    }
  }
  if (K3K > 0) { st.longestStall = std::max(st.longestStall, k3longest);
                 st.stallHist.push_back(k3longest); }
  if (run) { st.deadRuns++; st.deadRunTotal += run; st.deadRunHist.push_back(run); }
  st.longestDeadRun = std::max(st.longestDeadRun, longest);
  if (longest >= 6) st.gamesWithDeadRun++;
  st.events += nEvents;
  st.eventHist.push_back(nEvents);
}

struct PathologyConfig {
  std::string specA = "v04", specB = "v04";
  int games = 200;
  int rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 0;
};

inline PathologyStats runPathology(const PathologyConfig& pc) {
  int nThreads = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, pc.games));
  std::vector<PathologyStats> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(pc.specA); B[i] = makeAgent(pc.specB); }
      Game game;
      game.trace.on = true;
      for (int i = t; i < pc.games; i += nThreads) {
        uint64_t s = mixSeed(pc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < pc.rotations; rot++) {
          int orient = (pc.rotations == 2) ? rot : (rot / 3);
          int shift  = (pc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          game.trace.events.clear();
          GameResult r = game.run(s, pc.rules, ag);
          analyseTrace(game.trace.events, game.g.dealt, pc.rules, r.hitLimit, local[t]);
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  PathologyStats total;
  for (int t = 0; t < nThreads; t++) total.merge(local[t]);
  return total;
}

inline void printPathology(const PathologyStats& s, std::ostream& os) {
  auto pct = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  std::vector<int> ev = s.eventHist;
  std::sort(ev.begin(), ev.end());
  auto q = [&](double f) { return ev.empty() ? 0 : ev[std::min(ev.size() - 1, size_t(f * ev.size()))]; };
  os << "games              " << s.games << "\n";
  os << "events/game        " << (s.games ? double(s.events) / s.games : 0)
     << "   median " << q(0.5) << "  p90 " << q(0.90) << "  p99 " << q(0.99) << "  max " << (ev.empty() ? 0 : ev.back()) << "\n";
  os << "asks/game          " << (s.games ? double(s.asks) / s.games : 0)
     << "   hit rate " << pct(s.hits, s.asks) << "%\n";
  os << "DEAD asks          " << s.deadAsks << "  (" << pct(s.deadAsks, s.asks)
     << "% of asks)   -- actor could PROVE the target lacks the card\n";
  os << "dead runs          " << s.deadRuns << "  mean length "
     << (s.deadRuns ? double(s.deadRunTotal) / s.deadRuns : 0)
     << "  longest " << s.longestDeadRun << "\n";
  os << "games w/ run>=6    " << s.gamesWithDeadRun << "  (" << pct(s.gamesWithDeadRun, s.games) << "%)\n";
  os << "starved turns      " << s.starvedTurns << "  (" << pct(s.starvedTurns, s.asks)
     << "% of asks)   -- NO legal ask had a live possibility\n";
  os << "repeat (a,c,t)     " << s.repeatAsks << "  (" << pct(s.repeatAsks, s.asks) << "%)\n";
  os << "repeat (a,suit,t)  " << s.repeatPairAsks << "  (" << pct(s.repeatPairAsks, s.asks) << "%)\n";
  os << "asks in own-locked " << s.lockedInfoAsks << "  (" << pct(s.lockedInfoAsks, s.asks)
     << "% of asks)   -- guaranteed miss, but emits a certificate\n";
  os << "declarations       " << s.decls << "   wrong " << s.declWrong << " (" << pct(s.declWrong, s.decls) << "%)\n";
  os << "  at/after ev>=220 " << s.declsLate << "   wrong " << s.declWrongLate << " (" << pct(s.declWrongLate, s.declsLate) << "%)\n";
  os << "  forced endgame   " << s.declsForced << "   wrong " << s.declWrongForced << " (" << pct(s.declWrongForced, s.declsForced) << "%)\n";
  os << "action-limit games " << s.limitHits << " (" << pct(s.limitHits, s.games) << "%)\n";
  // K3.  Printed only when a stall key was parsed, so the reference output of
  // `pathology --a=v06 --b=v06` is byte-identical to the unmodified binary.
  if (k3stall().on.load(std::memory_order_relaxed)) {
    auto& S = k3stall();
    std::vector<int> sh = s.stallHist;
    std::sort(sh.begin(), sh.end());
    auto sq = [&](double f) { return sh.empty() ? 0 : sh[std::min(sh.size() - 1, size_t(f * sh.size()))]; };
    os << "-- K3 stall detector (K=" << S.K.load() << ", K2=" << S.K2.load()
       << ", soft=" << S.soft.load() << ") --\n";
    os << "longest no-progress run per game   median " << sq(0.5) << "  p99 " << sq(0.99)
       << "  max " << s.longestStall << "\n";
    os << "(seat,event) pairs at press>=1     " << s.stallEvents
       << "  (" << pct(s.stallEvents, s.events * NPLAY) << "% of seat-events)\n";
    os << "declarations while stalled>=K      " << s.stallDecls << "   wrong "
       << s.stallDeclWrong << " (" << pct(s.stallDeclWrong, s.stallDecls) << "%)\n";
    os << "declarations while stalled>=K2     " << s.stall2Decls << "   wrong "
       << s.stall2DeclWrong << " (" << pct(s.stall2DeclWrong, s.stall2Decls) << "%)\n";
    os << "agent-side: declOpps " << S.declOpps.load() << "  stage1 " << S.stage1.load()
       << "  stage2 " << S.stage2.load() << "  clockStage1 " << S.clockStage1.load()
       << "  maxStall " << S.maxStall.load() << "\n";
  }
}

} // namespace fish
