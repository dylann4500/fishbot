// Adversarial verification of the P5 "D13 free channel is rarely provable" claim.
//
// For every v0.4 ask decision, replayed from the trace, we ask four versions of
// the same question -- "does a LIVE half-suit exist that my team owns outright
// and in which I have a legal ask?" -- differing only in what the actor is
// allowed to know:
//
//   GT     ground truth (the deal itself)                       -- upper bound
//   POSS   not yet refuted by the hard certificate masks         -- loose bound
//   EXACT  exact posterior over consistent deals == 1            -- blockdp.hpp
//   P99    exact posterior >= 0.99   (a policy does not need proof)
//   P90    exact posterior >= 0.90
//   HARD   probe_human.hpp provablyTeamOwned (the P5 measurement)
//
// HARD is what P5 measured.  EXACT is what the acting policy could actually
// prove, since v0.4 already builds the same BlockDP object every ply
// (v04.hpp:170).  The gap between HARD and EXACT is the thing P5 flagged as an
// open caveat in its own section 6.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include "blockdp.hpp"
#include "probe_human.hpp"
#include <thread>

namespace fish {
namespace vd13 {

struct Stats {
  long long games = 0, decisions = 0;
  long long availGT = 0, availPOSS = 0, availEXACT = 0, availP99 = 0, availP90 = 0, availP75 = 0, availP50 = 0, availHARD = 0;
  long long forcedGT = 0, forcedEXACT = 0, forcedHARD = 0;
  long long asks = 0, askInGT = 0, askInEXACT = 0, askInHARD = 0;
  long long deadAsks = 0, deadInGT = 0, deadInEXACT = 0, deadInHARD = 0;
  long long blockFail = 0, blockCalls = 0;
  // how many live half-suits are truly team-owned but not provable, by reason
  long long gtOwnedSets = 0, gtOwnedSetsExact = 0, gtOwnedSetsHard = 0;
  // clustering: how many distinct games contribute at all
  long long unsoundExact = 0, unsoundHard = 0;
  long long gamesAvailHard = 0, gamesForcedHard = 0, gamesAvailExact = 0, maxForcedInGame = 0;

  void merge(const Stats& o) {
    games += o.games; decisions += o.decisions;
    availGT += o.availGT; availPOSS += o.availPOSS; availEXACT += o.availEXACT;
    availP99 += o.availP99; availP90 += o.availP90; availP75 += o.availP75; availP50 += o.availP50; availHARD += o.availHARD;
    forcedGT += o.forcedGT; forcedEXACT += o.forcedEXACT; forcedHARD += o.forcedHARD;
    asks += o.asks; askInGT += o.askInGT; askInEXACT += o.askInEXACT; askInHARD += o.askInHARD;
    deadAsks += o.deadAsks; deadInGT += o.deadInGT; deadInEXACT += o.deadInEXACT;
    deadInHARD += o.deadInHARD;
    blockFail += o.blockFail; blockCalls += o.blockCalls;
    gtOwnedSets += o.gtOwnedSets; gtOwnedSetsExact += o.gtOwnedSetsExact;
    gtOwnedSetsHard += o.gtOwnedSetsHard;
    unsoundExact += o.unsoundExact; unsoundHard += o.unsoundHard;
    gamesAvailHard += o.gamesAvailHard; gamesForcedHard += o.gamesForcedHard;
    gamesAvailExact += o.gamesAvailExact;
    maxForcedInGame = std::max(maxForcedInGame, o.maxForcedInGame);
  }
};

// Can P(team owns every card of s) be non-zero given the hard masks alone?
inline bool possiblyTeamOwned(const Knowledge& kk, int s, int teamMask) {
  if (!kk.setActive[s]) return false;
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (kk.owner[c] == OUT_OF_PLAY) return false;
    if (kk.owner[c] < NPLAY) { if (!(teamMask & (1u << kk.owner[c]))) return false; continue; }
    if (!(kk.mask[c] & uint8_t(teamMask))) return false;      // no team seat can hold it
  }
  return true;
}

inline void analyse(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                    const Rules& rules, Stats& st) {
  Knowledge k[NPLAY];
  uint64_t hand[NPLAY];
  for (int p = 0; p < NPLAY; p++) { k[p].init(p, dealt[p], rules.deckSets); hand[p] = dealt[p]; }
  PublicState pub{};
  pub.rules = rules;
  for (int p = 0; p < NPLAY; p++) pub.handCount[p] = uint8_t(popcount64(dealt[p]));
  for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < rules.deckSets);
  st.games++;
  long long gAvailH = 0, gForcedH = 0, gAvailE = 0;
  BlockDP block;

  for (const Event& e : ev) {
    if (e.kind == Kind::Ask) {
      const Knowledge& kk = k[e.actor];
      int teamMask = 0;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == teamOf(e.actor)) teamMask |= 1 << p;

      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, kk.myHand, e.actor, buf);
      st.decisions++;
      st.asks++;

      int liveOpp = 0;
      for (int t = 0; t < NPLAY; t++)
        if (teamOf(t) != teamOf(e.actor) && pub.handCount[t]) liveOpp++;

      // ------- per-set classification -------------------------------------
      bool gtOwn[NSET] = {}, hardOwn[NSET] = {}, exactOwn[NSET] = {},
           p99Own[NSET] = {}, p90Own[NSET] = {}, p75Own[NSET] = {}, p50Own[NSET] = {}, possOwn[NSET] = {};
      bool anyCandidate = false;
      for (int s = 0; s < rules.deckSets; s++) {
        if (!pub.setActive[s]) continue;
        // ground truth: every card of s sits with a seat on the actor's team
        bool g = true;
        for (int i = 0; i < SETSZ && g; i++) {
          int c = cardOf(s, i);
          int holder = -1;
          for (int p = 0; p < NPLAY; p++) if (hand[p] & bit(c)) { holder = p; break; }
          if (holder < 0 || !(teamMask & (1 << holder))) g = false;
        }
        gtOwn[s] = g;
        hardOwn[s] = provablyTeamOwned(kk, s, teamMask);
        possOwn[s] = possiblyTeamOwned(kk, s, teamMask);
        if (possOwn[s]) anyCandidate = true;
      }

      if (anyCandidate) {
        st.blockCalls++;
        bool ok = block.build(kk);
        if (!ok) st.blockFail++;
        for (int s = 0; s < rules.deckSets; s++) {
          if (!possOwn[s]) continue;
          double pT;
          if (ok) pT = block.teamOwnsProbability(s, teamMask);
          else    pT = hardOwn[s] ? 1.0 : 0.0;      // conservative fallback
          if (pT >= 1.0 - 1e-9) exactOwn[s] = true;
          if (pT >= 0.99) p99Own[s] = true;
          if (pT >= 0.90) p90Own[s] = true;
          if (pT >= 0.75) p75Own[s] = true;
          if (pT >= 0.50) p50Own[s] = true;
        }
      }

      // ------- availability of a legal ask inside such a set ---------------
      auto opts = [&](int s) {
        if (!(kk.myHand & setMask(s))) return 0;
        int miss = 0;
        for (int i = 0; i < SETSZ; i++) if (!(kk.myHand & bit(cardOf(s, i)))) miss++;
        return miss * liveOpp;
      };
      int oGT = 0, oHARD = 0, oEXACT = 0, oP99 = 0, oP90 = 0, oP75 = 0, oP50 = 0, oPOSS = 0;
      for (int s = 0; s < rules.deckSets; s++) {
        if (!pub.setActive[s]) continue;
        int o = opts(s);
        if (!o) continue;
        if (gtOwn[s]) oGT += o;
        if (hardOwn[s]) oHARD += o;
        if (exactOwn[s]) oEXACT += o;
        if (p99Own[s]) oP99 += o;
        if (p90Own[s]) oP90 += o;
        if (p75Own[s]) oP75 += o;
        if (p50Own[s]) oP50 += o;
        if (possOwn[s]) oPOSS += o;
      }
      if (oGT) { st.availGT++; if (oGT >= n) st.forcedGT++; }
      if (oHARD) { st.availHARD++; gAvailH++; if (oHARD >= n) { st.forcedHARD++; gForcedH++; } }
      if (oEXACT) { st.availEXACT++; gAvailE++; if (oEXACT >= n) st.forcedEXACT++; }
      if (oP99) st.availP99++;
      if (oP90) st.availP90++;
      if (oP75) st.availP75++;
      if (oP50) st.availP50++;
      if (oPOSS) st.availPOSS++;

      // soundness: a certificate must never fire on a set the team does not own
      for (int s = 0; s < rules.deckSets; s++) {
        if (!pub.setActive[s] || gtOwn[s]) continue;
        if (exactOwn[s]) st.unsoundExact++;
        if (hardOwn[s]) st.unsoundHard++;
      }
      // count truly-owned live half-suits and how many are certifiable
      for (int s = 0; s < rules.deckSets; s++) {
        if (!pub.setActive[s] || !gtOwn[s]) continue;
        st.gtOwnedSets++;
        if (exactOwn[s]) st.gtOwnedSetsExact++;
        if (hardOwn[s]) st.gtOwnedSetsHard++;
      }

      // ------- what v0.4 actually asked ------------------------------------
      int S = int(e.set);
      if (gtOwn[S]) st.askInGT++;
      if (exactOwn[S]) st.askInEXACT++;
      if (hardOwn[S]) st.askInHARD++;
      if (provablyDead(kk, e.card, e.target)) {
        st.deadAsks++;
        if (gtOwn[S]) st.deadInGT++;
        if (exactOwn[S]) st.deadInEXACT++;
        if (hardOwn[S]) st.deadInHARD++;
      }
    }

    // advance ground truth
    if (e.kind == Kind::Ask && e.success) {
      hand[e.target] &= ~bit(e.card);
      hand[e.actor] |= bit(e.card);
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
    }
    for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) pub.setActive[e.set] = false;
    for (int p = 0; p < NPLAY; p++) pub.handCount[p] = e.handCount[p];
  }
  if (gAvailH) st.gamesAvailHard++;
  if (gForcedH) st.gamesForcedHard++;
  if (gAvailE) st.gamesAvailExact++;
  st.maxForcedInGame = std::max(st.maxForcedInGame, gForcedH);
}

struct Config {
  std::string specA = "v04", specB = "v04";
  int games = 200, rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 0;
};

inline Stats run(const Config& pc) {
  int nThreads = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, pc.games));
  std::vector<Stats> local(nThreads);
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
          game.run(s, pc.rules, ag);
          analyse(game.trace.events, game.g.dealt, pc.rules, local[t]);
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  Stats st;
  for (auto& l : local) st.merge(l);
  return st;
}

inline void print(const Stats& s, std::ostream& o) {
  auto pct = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  o << "games                       " << s.games << "\n";
  o << "ask decisions               " << s.decisions << "\n";
  o << "blockdp builds              " << s.blockCalls << "  failures " << s.blockFail << "\n";
  o << "-- a legal ask exists inside a team-owned live half-suit --\n";
  o << "  GT    (ground truth)      " << s.availGT   << " (" << pct(s.availGT, s.decisions)   << "%)  forced " << s.forcedGT << " (" << pct(s.forcedGT, s.availGT) << "%)\n";
  o << "  POSS  (not refuted)       " << s.availPOSS << " (" << pct(s.availPOSS, s.decisions) << "%)\n";
  o << "  P50   (exact >= 0.50)     " << s.availP50  << " (" << pct(s.availP50, s.decisions)  << "%)\n";
  o << "  P75   (exact >= 0.75)     " << s.availP75  << " (" << pct(s.availP75, s.decisions)  << "%)\n";
  o << "  P90   (exact >= 0.90)     " << s.availP90  << " (" << pct(s.availP90, s.decisions)  << "%)\n";
  o << "  P99   (exact >= 0.99)     " << s.availP99  << " (" << pct(s.availP99, s.decisions)  << "%)\n";
  o << "  EXACT (exact == 1)        " << s.availEXACT<< " (" << pct(s.availEXACT, s.decisions)<< "%)  forced " << s.forcedEXACT << " (" << pct(s.forcedEXACT, s.availEXACT) << "%)\n";
  o << "  HARD  (P5 measurement)    " << s.availHARD << " (" << pct(s.availHARD, s.decisions) << "%)  forced " << s.forcedHARD << " (" << pct(s.forcedHARD, s.availHARD) << "%)\n";
  o << "  distinct games with HARD available " << s.gamesAvailHard << " / " << s.games
    << "   with a forced HARD decision " << s.gamesForcedHard
    << "   max forced decisions in one game " << s.maxForcedInGame << "\n";
  o << "  distinct games with EXACT available " << s.gamesAvailExact << " / " << s.games << "\n";
  o << "  UNSOUND certificates (fired on a set the team does NOT own): EXACT "
    << s.unsoundExact << "  HARD " << s.unsoundHard << "\n";
  o << "-- live half-suits truly owned by the actor's team, at ask decisions --\n";
  o << "  count                     " << s.gtOwnedSets << "\n";
  o << "  certified by EXACT        " << s.gtOwnedSetsExact << " (" << pct(s.gtOwnedSetsExact, s.gtOwnedSets) << "%)\n";
  o << "  certified by HARD         " << s.gtOwnedSetsHard  << " (" << pct(s.gtOwnedSetsHard, s.gtOwnedSets)  << "%)\n";
  o << "-- the ask v0.4 actually made --\n";
  o << "  asks                      " << s.asks << "\n";
  o << "  in a GT team-owned set    " << s.askInGT    << " (" << pct(s.askInGT, s.asks)    << "%)\n";
  o << "  in an EXACT-owned set     " << s.askInEXACT << " (" << pct(s.askInEXACT, s.asks) << "%)\n";
  o << "  in a HARD-owned set       " << s.askInHARD  << " (" << pct(s.askInHARD, s.asks)  << "%)\n";
  o << "-- provably dead asks --\n";
  o << "  dead asks                 " << s.deadAsks << " (" << pct(s.deadAsks, s.asks) << "% of asks)\n";
  o << "  in a GT team-owned set    " << s.deadInGT    << " (" << pct(s.deadInGT, s.deadAsks)    << "%)\n";
  o << "  in an EXACT-owned set     " << s.deadInEXACT << " (" << pct(s.deadInEXACT, s.deadAsks) << "%)\n";
  o << "  in a HARD-owned set       " << s.deadInHARD  << " (" << pct(s.deadInHARD, s.deadAsks)  << "%)\n";
}

} // namespace vd13
} // namespace fish
