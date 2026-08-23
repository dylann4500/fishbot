// ADVERSARIAL VERIFICATION of the P1 "E11 is false" finding.
//
// The P1 report measures information arrival with BlockDP::bestTeamAllocation
// (`pAlloc`).  This file re-measures the SAME question with two quantities that
// do not touch bestTeamAllocation at all:
//
//   1. support size  -- sum over the unresolved cards of the half-suit of the
//      number of seats still possible for that card in the observer's
//      Knowledge.  Pure combinatorics, no DP, no probability.
//   2. allocation entropy -- Shannon entropy of BlockDP::marginals restricted to
//      the cards of the half-suit.  Uses `marginals`, a different function from
//      `bestTeamAllocation`; both are validated against exhaustive enumeration
//      by `fish oracle`.
//
// If either strictly falls after a legal ask inside a half-suit the asker's team
// provably owns, then information about that half-suit's allocation arrived, and
// E11's "no further information about its allocation can ever arrive" is false
// independently of how pAlloc is normalised.
#pragma once
#include "probe_deadlock.hpp"

namespace fish {

struct VDStat {
  long long states = 0;                 // (state x locked-half-suit x observer)
  long long asksLocked = 0, asksOther = 0;
  long long lockDropSupport = 0, lockDropH = 0;
  long long otherDropSupport = 0, otherDropH = 0;
  double sumDHLock = 0, maxDHLock = 0, sumDHOther = 0, maxDHOther = 0;
  double sumDSupLock = 0, sumDSupOther = 0;
  // per (state, locked set, observer): best over all own-locked asks
  std::vector<double> bestDH;
  long long anyLockedAskInformative = 0;
  // v0.4's own played ask, on the same metric
  long long moverStates = 0, playedDropH = 0, bestDropH = 0;
};

// support size of the half-suit's allocation in this observer's knowledge
inline int vdSupport(const Knowledge& k, int s) {
  int acc = 0;
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (k.owner[c] < NPLAY) { acc += 1; continue; }
    acc += __builtin_popcount(k.mask[c]);
  }
  return acc;
}

// Shannon entropy (nats) of the exact per-card marginals over the half-suit.
inline double vdEntropy(const Knowledge& k, int s) {
  BlockDP b;
  if (!b.build(k)) return -1;
  static thread_local double mu[NCARD][NPLAY];
  for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) mu[c][p] = 0;
  b.marginals(mu);
  double H = 0;
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (!(k.unresolved & bit(c))) continue;
    for (int p = 0; p < NPLAY; p++) {
      double q = mu[c][p];
      if (q > 1e-15) H -= q * std::log(q);
    }
  }
  return H;
}

inline void vdAnalyseState(const Game& G, int actor, const Event& actual, VDStat& st,
                           std::ostream& os, bool dump) {
  PublicState P = G.g.pub; P.history.clear(); P.turn = actor;
  uint64_t t0 = G.g.hand[0] | G.g.hand[2] | G.g.hand[4];
  uint64_t t1 = G.g.hand[1] | G.g.hand[3] | G.g.hand[5];

  int lockOwner[NSET];
  for (int s = 0; s < NSET; s++) {
    lockOwner[s] = -1;
    if (!P.setActive[s]) continue;
    uint64_t m = setMask(s);
    if ((t0 & m) == m) lockOwner[s] = 0; else if ((t1 & m) == m) lockOwner[s] = 1;
  }

  // ---- the headline re-measurement -------------------------------------
  for (int s = 0; s < NSET; s++) {
    if (!P.setActive[s] || lockOwner[s] < 0) continue;
    int team = lockOwner[s];
    for (int obs = 0; obs < NPLAY; obs++) {
      if (teamOf(obs) != team) continue;
      double H0 = vdEntropy(G.agents[obs]->k, s);
      if (H0 < 0) continue;
      int S0 = vdSupport(G.agents[obs]->k, s);
      st.states++;
      double bestDH = 0; bool anyInfo = false;
      int bestA = -1, bestC = -1, bestT = -1;
      for (int a = 0; a < NPLAY; a++) {
        if (teamOf(a) != team || a == obs) continue;   // only a TEAMMATE's ask
        AskMove buf[NSET * SETSZ * 3];
        int n = enumerateAsks(P, G.agents[a]->k.myHand, a, buf);
        for (int i = 0; i < n; i++) {
          bool inLocked = (setOf(buf[i].card) == s);
          Knowledge kh = dlHypoMiss(G.agents[obs]->k, P.handCount, a, buf[i].card, buf[i].target);
          double H1 = vdEntropy(kh, s);
          if (H1 < 0) continue;
          int S1 = vdSupport(kh, s);
          double dH = H0 - H1;            // information gained, nats
          double dS = double(S0 - S1);
          if (inLocked) {
            st.asksLocked++; st.sumDHLock += dH; st.sumDSupLock += dS;
            if (dS > 0) st.lockDropSupport++;
            if (dH > 1e-9) { st.lockDropH++; anyInfo = true; }
            st.maxDHLock = std::max(st.maxDHLock, dH);
            if (dH > bestDH) { bestDH = dH; bestA = a; bestC = buf[i].card; bestT = buf[i].target; }
          } else {
            st.asksOther++; st.sumDHOther += dH; st.sumDSupOther += dS;
            if (dS > 0) st.otherDropSupport++;
            if (dH > 1e-9) st.otherDropH++;
            st.maxDHOther = std::max(st.maxDHOther, dH);
          }
        }
      }
      st.bestDH.push_back(bestDH);
      if (anyInfo) st.anyLockedAskInformative++;
      if (dump && bestA >= 0) {
        os << "    locked set " << s << " (" << setName(s) << ") owned by team " << team
           << ", observer s" << obs << ": H=" << std::setprecision(6) << H0
           << " support=" << S0 << ";  best teammate ask s" << bestA << " asks "
           << cardName(bestC) << " of s" << bestT << " -> dH=" << bestDH << " nats\n";
      }
    }
  }

  // ---- what the mover actually played, on the same metric ---------------
  {
    int myTeam = teamOf(actor);
    // pick the team's locked half-suit with the most entropy to a teammate
    int cand = -1; double bestH = -1;
    for (int s = 0; s < NSET; s++) {
      if (!P.setActive[s] || lockOwner[s] != myTeam) continue;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == myTeam && p != actor) {
        double h = vdEntropy(G.agents[p]->k, s);
        if (h > bestH) { bestH = h; cand = s; }
      }
    }
    if (cand >= 0) {
      st.moverStates++;
      double base[NPLAY]; for (int p = 0; p < NPLAY; p++) base[p] = -1;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == myTeam && p != actor) base[p] = vdEntropy(G.agents[p]->k, cand);
      auto gain = [&](int c, int t) {
        double g = 0;
        for (int p = 0; p < NPLAY; p++) {
          if (base[p] < 0) continue;
          Knowledge kh = dlHypoMiss(G.agents[p]->k, P.handCount, actor, c, t);
          double h = vdEntropy(kh, cand);
          if (h >= 0) g = std::max(g, base[p] - h);
        }
        return g;
      };
      double gPlayed = gain(actual.card, actual.target), gBest = 0;
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(P, G.agents[actor]->k.myHand, actor, buf);
      for (int i = 0; i < n; i++) gBest = std::max(gBest, gain(buf[i].card, buf[i].target));
      if (gPlayed > 1e-9) st.playedDropH++;
      if (gBest > 1e-9) st.bestDropH++;
      if (dump)
        os << "    mover s" << actor << " played " << cardName(actual.card) << "@s"
           << int(actual.target) << ": dH to a teammate on set " << cand << " = " << gPlayed
           << ";  best legal ask would give " << gBest << "\n";
    }
  }
}

inline void runVDeadlock(const DeadlockCfg& dc, std::ostream& os) {
  os << std::fixed;
  std::vector<DLGameInfo> all;
  for (int i = 0; i < dc.games; i++) {
    uint64_t s = mixSeed(dc.seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < dc.rotations; rot++) {
      DLGameInfo gi; dlScan(dc.spec, s, rot, dc.rules, gi);
      all.push_back(std::move(gi));
    }
  }
  std::vector<int> lg;
  for (int i = 0; i < int(all.size()); i++) if (all[i].events > dc.minEvents) lg.push_back(i);
  std::sort(lg.begin(), lg.end(), [&](int a, int b) { return all[a].runLen > all[b].runLen; });
  os << "VERIFY: scanned " << all.size() << " games (" << dc.spec << " mirror, seed " << dc.seed
     << ")  long games: " << lg.size() << "\n";

  VDStat st;
  int nd = std::min<int>(dc.dump, int(lg.size()));
  for (int gi_i = 0; gi_i < int(lg.size()); gi_i++) {
    const DLGameInfo& gi = all[lg[gi_i]];
    if (gi.runStart < 0) continue;
    std::vector<int> idxs;
    for (int j = 0; j < dc.maxStates; j++) {
      int t = gi.runStart + j * dc.stride;
      if (t < gi.events && gi.ev[t].kind == Kind::Ask) idxs.push_back(t);
    }
    bool dump = gi_i < nd;
    std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
    for (int p = 0; p < NPLAY; p++) { ag[p] = makeAgent(dc.spec); ap[p] = ag[p].get(); }
    Game game; game.rotation = gi.rot;
    std::vector<char> want(gi.ev.size() + 2, 0);
    for (int i : idxs) want[i] = 1;
    game.observer = [&](const Game& G) {
      int t = G.g.pub.nEvents;
      if (t >= int(gi.ev.size()) || !want[t]) return;
      if (gi.ev[t].kind != Kind::Ask) return;
      if (dump) os << "  [game seed " << gi.seed << " rot " << gi.rot << "] event " << t << "\n";
      vdAnalyseState(G, gi.ev[t].actor, gi.ev[t], st, os, dump);
    };
    game.run(gi.seed, dc.rules, ap);
  }

  os << "\n--- INDEPENDENT RE-MEASUREMENT (support size + marginal entropy) ---\n";
  os << "(state x locked-half-suit x owning-team observer) triples: " << st.states << "\n";
  os << "  legal teammate asks INSIDE that locked half-suit: " << st.asksLocked << "\n";
  if (st.asksLocked) {
    os << "     strictly shrink the observer's support set: " << st.lockDropSupport << " ("
       << (100.0 * st.lockDropSupport / st.asksLocked) << "%)\n";
    os << "     strictly lower the observer's allocation entropy: " << st.lockDropH << " ("
       << (100.0 * st.lockDropH / st.asksLocked) << "%)\n";
    os << "     mean dH " << (st.sumDHLock / st.asksLocked) << " nats,  max " << st.maxDHLock
       << ",  mean support removed " << (st.sumDSupLock / st.asksLocked) << " seat-slots\n";
  }
  os << "  legal teammate asks OUTSIDE it: " << st.asksOther << "\n";
  if (st.asksOther) {
    os << "     strictly shrink support: " << st.otherDropSupport << " ("
       << (100.0 * st.otherDropSupport / st.asksOther) << "%)\n";
    os << "     strictly lower entropy: " << st.otherDropH << " ("
       << (100.0 * st.otherDropH / st.asksOther) << "%)\n";
    os << "     mean dH " << (st.sumDHOther / st.asksOther) << " nats,  max " << st.maxDHOther << "\n";
  }
  os << "  triples where SOME legal own-locked ask was informative: " << st.anyLockedAskInformative
     << " / " << st.states << "\n";
  pctl(st.bestDH, os, "  best available dH per triple");
  os << "  mover states with a locked half-suit: " << st.moverStates
     << ";  ask v0.4 played was informative in " << st.playedDropH
     << ";  some legal ask was informative in " << st.bestDropH << "\n";
}

} // namespace fish
