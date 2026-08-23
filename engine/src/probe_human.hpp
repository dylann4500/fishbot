// P5 probe: how much *signalling channel* a v0.4 game actually offers, measured
// from each actor's own public Knowledge at the moment it moves.
//
// Everything here is computed from the reconstructed Knowledge object (public
// information + the actor's own hand), never from ground truth, so every number
// is a capability the acting policy could in principle have used.
//
// Channels measured, one row per ask decision:
//   (A) target freedom  -- among the legal targets for the card actually asked,
//       how many are HARD-INDISTINGUISHABLE from the chosen one (both still
//       possible holders, or both provably excluded).  log2 of that class size
//       is a lower bound on the bits available in the target dimension at zero
//       material cost, which is signalling.md 7.9 "Construction B".
//   (B) D13 free channel -- does a live half-suit exist that the actor can PROVE
//       its own team owns outright, in which the actor has a legal ask?  Every
//       ask there is a guaranteed miss that leaks nothing the opponents can use
//       (they hold no card of the half-suit and can never legally ask in it),
//       but it still emits a C5 ask-legality certificate to teammates.
//   (C) turn routing -- for how many DISTINCT opponents does a provably-dead
//       legal ask exist?  k >= 2 means the actor can hand the turn to a chosen
//       opponent at will, which is what blackball/lockout play requires.
//   (D) what v0.4 actually did with each of these.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include <thread>
#include <mutex>

namespace fish {

struct HumanChanStats {
  long long games = 0, decisions = 0;
  long long legalAsks = 0;

  // (A) target dimension
  long long tgtClassSum = 0;        // sum of |indistinguishable class| for the chosen card
  double    tgtBits = 0;            // sum of log2(class size)
  long long tgtFree = 0;            // decisions with class size >= 2
  double    tgtSpread = 0;          // sum over free decisions of (max-min) capacity-marginal
  long long d13Forced = 0;          // D13 available AND every legal ask lay in an owned set

  // (B) D13 free channel
  long long d13Available = 0;       // decisions where a provably team-owned live set had a legal ask
  long long d13Used = 0;            // ... and v0.4's ask was in such a set
  long long d13AskOptions = 0;      // total legal asks living in such sets

  // (C) turn routing via a deliberate miss
  long long routeHist[4] = {0,0,0,0};   // # distinct opponents reachable by a provably-dead ask
  long long routeUsed = 0;              // v0.4's ask was provably dead (turn deliberately donated?)

  // (D) contrast: dead asks split by whether they were in a provably-team-owned
  // set (a defensible information move) or a contested set (pure waste).
  long long deadAsks = 0, deadInOwned = 0, deadInContested = 0;

  // (E) concealment: decisions at which the actor held >=4 of a live half-suit
  // that its team had never publicly touched, and asked in it anyway.
  long long concealOpp = 0, concealBroken = 0;

  void merge(const HumanChanStats& o) {
    games += o.games; decisions += o.decisions; legalAsks += o.legalAsks;
    tgtClassSum += o.tgtClassSum; tgtBits += o.tgtBits; tgtFree += o.tgtFree;
    tgtSpread += o.tgtSpread; d13Forced += o.d13Forced;
    d13Available += o.d13Available; d13Used += o.d13Used; d13AskOptions += o.d13AskOptions;
    for (int i = 0; i < 4; i++) routeHist[i] += o.routeHist[i];
    routeUsed += o.routeUsed;
    deadAsks += o.deadAsks; deadInOwned += o.deadInOwned; deadInContested += o.deadInContested;
    concealOpp += o.concealOpp; concealBroken += o.concealBroken;
  }
};

// Can the actor PROVE, from public information plus its own hand, that its team
// holds every card of live half-suit s?
inline bool provablyTeamOwned(const Knowledge& kk, int s, int teamMask) {
  if (!kk.setActive[s]) return false;
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (kk.owner[c] == OUT_OF_PLAY) return false;
    if (kk.owner[c] < NPLAY) { if (!(teamMask & (1u << kk.owner[c]))) return false; continue; }
    if (kk.mask[c] & ~uint8_t(teamMask)) return false;   // an opponent is still possible
  }
  return true;
}

inline bool provablyDead(const Knowledge& kk, int c, int t) {
  return (kk.owner[c] < NPLAY) ? (kk.owner[c] != t) : !(kk.mask[c] & (1u << t));
}

inline void analyseChannels(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                            const Rules& rules, HumanChanStats& st) {
  Knowledge k[NPLAY];
  for (int p = 0; p < NPLAY; p++) k[p].init(p, dealt[p], rules.deckSets);
  PublicState pub{};
  pub.rules = rules;
  for (int p = 0; p < NPLAY; p++) pub.handCount[p] = uint8_t(popcount64(dealt[p]));
  for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < rules.deckSets);
  st.games++;

  for (const Event& e : ev) {
    if (e.kind == Kind::Ask) {
      const Knowledge& kk = k[e.actor];
      int teamMask = 0;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == teamOf(e.actor)) teamMask |= 1 << p;

      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, kk.myHand, e.actor, buf);
      st.decisions++;
      st.legalAsks += n;

      // (A) target freedom for the card actually asked
      bool chosenDead = provablyDead(kk, e.card, e.target);
      int cls = 0;
      // Capacity-normalised marginal for the asked card: the exact
      // uniform-over-consistent-deals marginal when disjunctions and half-suit
      // lower bounds are dropped.  Its spread across the indistinguishable
      // class bounds what using the target dimension as a code actually costs.
      uint8_t cap[NPLAY]; kk.capacities(cap);
      double den = 0;
      for (int p = 0; p < NPLAY; p++) if (kk.mask[e.card] & (1u << p)) den += cap[p];
      double lo = 1e9, hi = -1e9;
      for (int t = 0; t < NPLAY; t++) {
        if (teamOf(t) == teamOf(e.actor)) continue;
        if (!pub.handCount[t]) continue;
        if (provablyDead(kk, e.card, t) != chosenDead) continue;
        cls++;
        double p = (kk.owner[e.card] < NPLAY) ? (kk.owner[e.card] == t ? 1.0 : 0.0)
                 : (den > 0 && (kk.mask[e.card] & (1u << t)) ? cap[t] / den : 0.0);
        lo = std::min(lo, p); hi = std::max(hi, p);
      }
      if (cls < 1) cls = 1;
      st.tgtClassSum += cls;
      st.tgtBits += std::log2(double(cls));
      if (cls >= 2) { st.tgtFree++; st.tgtSpread += (hi - lo); }

      // (B) D13 free channel
      bool ownedAvail = false, ownedUsed = false;
      int ownedOpts = 0;
      for (int s = 0; s < NSET; s++) {
        if (!pub.setActive[s]) continue;
        if (!(kk.myHand & setMask(s))) continue;          // no legal ask without a base card
        if (!provablyTeamOwned(kk, s, teamMask)) continue;
        // at least one card of s not in my hand => a legal ask exists
        int opts = 0;
        for (int i = 0; i < SETSZ; i++) {
          int c = cardOf(s, i);
          if (kk.myHand & bit(c)) continue;
          for (int t = 0; t < NPLAY; t++) {
            if (teamOf(t) == teamOf(e.actor)) continue;
            if (!pub.handCount[t]) continue;
            opts++;
          }
        }
        if (opts) { ownedAvail = true; st.d13AskOptions += opts; ownedOpts += opts; }
        if (int(e.set) == s && opts) ownedUsed = true;
      }
      if (ownedAvail) st.d13Available++;
      if (ownedUsed) st.d13Used++;
      if (ownedAvail && ownedOpts >= n) st.d13Forced++;   // no non-owned ask existed

      // (C) turn routing
      bool reach[NPLAY] = {false,false,false,false,false,false};
      for (int i = 0; i < n; i++)
        if (provablyDead(kk, buf[i].card, buf[i].target)) reach[buf[i].target] = true;
      int nreach = 0;
      for (int t = 0; t < NPLAY; t++) if (reach[t]) nreach++;
      st.routeHist[std::min(3, nreach)]++;
      if (chosenDead) st.routeUsed++;

      // (D) dead-ask breakdown
      if (chosenDead) {
        st.deadAsks++;
        if (provablyTeamOwned(kk, e.set, teamMask)) st.deadInOwned++;
        else st.deadInContested++;
      }

      // (E) concealment opportunity: >=4 of a live half-suit in hand, team has
      // never asked in it and no team card of it is publicly located.
      for (int s = 0; s < NSET; s++) {
        if (!pub.setActive[s]) continue;
        if (popcount64(kk.myHand & setMask(s)) < 4) continue;
        bool revealed = false;
        for (int p = 0; p < NPLAY && !revealed; p++)
          if ((teamMask & (1 << p)) && kk.askCount[p][s]) revealed = true;
        uint64_t pk = kk.publicKnown & setMask(s);
        while (pk && !revealed) { int c = __builtin_ctzll(pk); pk &= pk - 1;
          if (kk.owner[c] < NPLAY && (teamMask & (1u << kk.owner[c]))) revealed = true; }
        if (revealed) continue;
        st.concealOpp++;
        if (int(e.set) == s) st.concealBroken++;
      }
    }

    // advance the world
    for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) pub.setActive[e.set] = false;
    for (int p = 0; p < NPLAY; p++) pub.handCount[p] = e.handCount[p];
  }
}

struct HumanChanConfig {
  std::string specA = "v04", specB = "v04";
  int games = 200, rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 0;
};

inline HumanChanStats runHumanChan(const HumanChanConfig& pc) {
  int nThreads = pc.threads > 0 ? pc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, pc.games));
  std::vector<HumanChanStats> local(nThreads);
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
          analyseChannels(game.trace.events, game.g.dealt, pc.rules, local[t]);
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  HumanChanStats st;
  for (auto& l : local) st.merge(l);
  return st;
}

inline void printHumanChan(const HumanChanStats& s, std::ostream& o) {
  auto pct = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  o << "games                       " << s.games << "\n";
  o << "ask decisions               " << s.decisions << "\n";
  o << "mean legal asks/decision    " << (s.decisions ? double(s.legalAsks) / s.decisions : 0) << "\n";
  o << "-- (A) target dimension --\n";
  o << "mean indistinguishable targets for the card asked  "
    << (s.decisions ? double(s.tgtClassSum) / s.decisions : 0) << "\n";
  o << "mean free bits in target choice                    "
    << (s.decisions ? s.tgtBits / s.decisions : 0) << "\n";
  o << "decisions with >=2 equivalent targets              "
    << s.tgtFree << " (" << pct(s.tgtFree, s.decisions) << "%)\n";
  o << "mean capacity-marginal spread inside that class    "
    << (s.tgtFree ? s.tgtSpread / s.tgtFree : 0) << "\n";
  o << "-- (B) D13 free channel (provably team-owned live half-suit) --\n";
  o << "decisions where it existed   " << s.d13Available << " (" << pct(s.d13Available, s.decisions) << "%)\n";
  o << "decisions where v0.4 used it " << s.d13Used << " (" << pct(s.d13Used, s.d13Available) << "% of available)\n";
  o << "mean free-channel asks when available " << (s.d13Available ? double(s.d13AskOptions) / s.d13Available : 0) << "\n";
  o << "  of those, decisions where NO other ask existed (forced, not chosen) " << s.d13Forced
    << " (" << pct(s.d13Forced, s.d13Available) << "%)\n";
  o << "-- (C) turn routing by deliberate miss --\n";
  for (int i = 0; i < 4; i++)
    o << "  distinct opponents reachable = " << i << (i == 3 ? "  " : "  ")
      << s.routeHist[i] << " (" << pct(s.routeHist[i], s.decisions) << "%)\n";
  o << "asks that were provably dead " << s.routeUsed << " (" << pct(s.routeUsed, s.decisions) << "%)\n";
  o << "-- (D) dead-ask breakdown --\n";
  o << "dead asks                    " << s.deadAsks << "\n";
  o << "  in a provably team-owned half-suit (defensible) " << s.deadInOwned
    << " (" << pct(s.deadInOwned, s.deadAsks) << "%)\n";
  o << "  in a contested half-suit (pure waste)           " << s.deadInContested
    << " (" << pct(s.deadInContested, s.deadAsks) << "%)\n";
  o << "-- (E) concealment --\n";
  o << "(decision, half-suit) pairs with >=4 in hand and team unrevealed " << s.concealOpp << "\n";
  o << "  broken by asking in that half-suit " << s.concealBroken
    << " (" << pct(s.concealBroken, s.concealOpp) << "%)\n";
}

} // namespace fish
