// P1 -- deadlock and dead-ask forensics.
//
// Scratch diagnostic for the v0.5 investigation.  Nothing here is on any
// decision path; it replays v0.4 mirror games, locates the dead-ask run, and
// asks the load-bearing question directly: at a deadlocked state, does a legal
// ask change a TEAMMATE's exact posterior over the allocation of a half-suit the
// team provably owns?
//
// The certificate an ask emits is applied through the production Knowledge
// machinery (belief.hpp Knowledge::onEvent) so that what is measured is exactly
// what a real ask would publish:
//   C5  the asker holds another card of the half-suit  (disjunction)
//   C3a the asker does not hold the asked card         (hard exclusion)
//   C3b on a miss, the target does not hold it         (hard exclusion)
// and the posterior is the exact block DP (blockdp.hpp), not an approximation.
#pragma once
#include "factory.hpp"
#include "diag.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>

namespace fish {

// ------------------------------------------------------------------ queries
struct AllocQ {
  bool ok = false;
  double pTeam = 0;     // exact P(team owns every card of the half-suit)
  double pAlloc = 0;    // exact P(the MAP all-team allocation is exactly right)
  int seats[SETSZ];
};

// Exact posterior query.  BlockDP borrows thread-local scratch, so only one may
// be live at a time; build/query/discard here keeps that invariant.
inline AllocQ dlQuery(const Knowledge& k, int s, int teamMask) {
  AllocQ q;
  for (int i = 0; i < SETSZ; i++) q.seats[i] = -1;
  BlockDP b;
  if (!b.build(k)) return q;
  q.pTeam = b.teamOwnsProbability(s, teamMask);
  int oc[SETSZ], os[SETSZ], n = 0;
  q.pAlloc = b.bestTeamAllocation(s, teamMask, oc, os, n);
  for (int i = 0; i < n; i++) q.seats[idxIn(oc[i])] = os[i];
  for (int i = 0; i < SETSZ; i++) {
    int c = cardOf(s, i);
    if (q.seats[i] < 0 && k.owner[c] < NPLAY) q.seats[i] = k.owner[c];
  }
  q.ok = true;
  return q;
}

// Apply the certificate a *missed* ask emits, exactly as the engine would.
inline Knowledge dlHypoMiss(const Knowledge& k0, const uint8_t* hc, int actor, int card, int target) {
  Knowledge k = k0;
  Event e{};
  e.kind = Kind::Ask; e.actor = uint8_t(actor); e.target = uint8_t(target);
  e.card = uint8_t(card); e.set = uint8_t(setOf(card)); e.success = false;
  for (int p = 0; p < NPLAY; p++) e.handCount[p] = hc[p];
  k.onEvent(e);
  return k;
}

// Fingerprint of an observer's deduced public-information state.  Two states
// with the same fingerprint carry literally the same knowledge.
inline uint64_t dlFingerprint(const Knowledge& k) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
  for (int c = 0; c < NCARD; c++) { mix(k.owner[c]); mix(k.mask[c]); }
  mix(k.unresolved);
  for (const auto& d : k.disj) { mix(d.player); mix(d.cards); }
  for (int p = 0; p < NPLAY; p++) mix(k.handCount[p]);
  return h;
}

// ------------------------------------------------- causal isolation variant
// v0.4 with ONE change: it never repeats an exact (card, target) question it has
// already asked in this game.  When its own pick is a repeat it falls back to
// the highest-scoring question it has not yet asked, ranked by the same score
// (the pre-refinement linear + one-ply-EV score; the shipped topK re-ranking is
// applied only to the primary pick).  Nothing about information is priced --
// this isolates repetition as a cause.
struct NoRepeatV04 : V04Agent {
  bool used[NCARD][NPLAY];
  const char* name() const override { return "v04_norepeat"; }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V04Agent::reset(s, hand, r, seed);
    memset(used, 0, sizeof(used));
  }
  void observe(const Event& e) override {
    V04Agent::observe(e);
    if (e.kind == Kind::Ask && e.actor == seat) used[e.card][e.target] = true;
  }
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove mv = V04Agent::chooseAsk(pub);
    if (!used[mv.card][mv.target]) return mv;
    refresh();
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    double bestU = -1e18; bool found = false; AskMove best = mv;
    double f[NFEAT];
    for (int i = 0; i < n; i++) {
      if (used[buf[i].card][buf[i].target]) continue;
      features(pub, buf[i].card, buf[i].target, f);
      double u = 0; for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
      u *= cfg.linearWeight;
      if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
      if (u > bestU) { bestU = u; best = buf[i]; found = true; }
    }
    if (found) { lastMySet = setOf(best.card); return best; }
    return mv;
  }
};

// ------------------------------------------------------------- game replay
struct DLGameInfo {
  uint64_t seed = 0;
  int rot = 0;
  int events = 0;
  int runStart = -1, runLen = 0;      // longest maximal dead-ask run
  int distinctTriples = 0;            // distinct (actor,card,target) in that run
  int lockedAtStart[2] = {0, 0}, lockedAtEnd[2] = {0, 0};
  int liveAtStart = 0;
  std::vector<Event> ev;
  std::vector<char> dead;             // per event: 1 = provably-dead ask
  uint64_t dealt[NPLAY] = {0,0,0,0,0,0};
};

// Replay and mark every ask the actor could have PROVED was a miss, using the
// same test as diag.hpp so the numbers line up with the P0 baseline.
inline void dlScan(const std::string& spec, uint64_t s, int rot, const Rules& rules, DLGameInfo& gi,
                   bool noRepeat = false) {
  std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
  for (int p = 0; p < NPLAY; p++) {
    if (noRepeat) ag[p] = std::make_unique<NoRepeatV04>(); else ag[p] = makeAgent(spec);
    ap[p] = ag[p].get();
  }
  Game game; game.trace.on = true; game.rotation = rot;
  game.run(s, rules, ap);
  gi.seed = s; gi.rot = rot;
  gi.ev = game.trace.events;
  gi.events = int(gi.ev.size());
  for (int p = 0; p < NPLAY; p++) gi.dealt[p] = game.g.dealt[p];

  Knowledge k[NPLAY];
  for (int p = 0; p < NPLAY; p++) k[p].init(p, gi.dealt[p], rules.deckSets);
  gi.dead.assign(gi.ev.size(), 0);
  int run = 0, runFrom = -1;
  for (size_t i = 0; i < gi.ev.size(); i++) {
    const Event& e = gi.ev[i];
    if (e.kind == Kind::Ask) {
      const Knowledge& kk = k[e.actor];
      bool d = (kk.owner[e.card] < NPLAY) ? (kk.owner[e.card] != e.target)
                                          : !(kk.mask[e.card] & (1u << e.target));
      gi.dead[i] = d ? 1 : 0;
      if (d) { if (!run) runFrom = int(i); run++;
               if (run > gi.runLen) { gi.runLen = run; gi.runStart = runFrom; } }
      else run = 0;
    }
    for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
  }
  // structure of the longest run and the ground-truth lock census around it
  if (gi.runStart >= 0) {
    std::map<uint32_t, int> tally;
    int end = std::min<int>(gi.events, gi.runStart + gi.runLen);
    for (int j = gi.runStart; j < end; j++) {
      const Event& e = gi.ev[j];
      if (e.kind != Kind::Ask) continue;
      tally[(uint32_t(e.actor) << 16) | (uint32_t(e.card) << 8) | e.target]++;
    }
    gi.distinctTriples = int(tally.size());
    uint64_t hand[NPLAY]; bool act[NSET];
    for (int p = 0; p < NPLAY; p++) hand[p] = gi.dealt[p];
    for (int j = 0; j < NSET; j++) act[j] = (j < rules.deckSets);
    for (int j = 0; j < gi.events; j++) {
      if (j == gi.runStart || j == end) {
        int* dst = (j == gi.runStart) ? gi.lockedAtStart : gi.lockedAtEnd;
        uint64_t t0 = hand[0] | hand[2] | hand[4], t1 = hand[1] | hand[3] | hand[5];
        for (int st = 0; st < NSET; st++) {
          if (!act[st]) continue;
          if (j == gi.runStart) gi.liveAtStart++;
          uint64_t m = setMask(st);
          if ((t0 & m) == m) dst[0]++; else if ((t1 & m) == m) dst[1]++;
        }
      }
      const Event& e = gi.ev[j];
      if (e.kind == Kind::Ask && e.success) { hand[e.target] &= ~bit(e.card); hand[e.actor] |= bit(e.card); }
      if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
        for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
        act[e.set] = false;
      }
    }
  }
}

// -------------------------------------------------------- part 2 aggregate
struct InfoStat {
  long long asks = 0;            // legal asks examined
  long long asksLocked = 0;      // inside a half-suit the actor's team provably owns
  long long posLocked = 0, posOther = 0;   // strictly positive dP for a teammate
  double sumLocked = 0, sumOther = 0;
  double maxLocked = 0, maxOther = 0;
  std::vector<double> dLocked, dOther;
  long long states = 0;
  double sumBestBefore = 0, sumBestAfter = 0;   // team-max pAlloc on the candidate set
  long long mapFlipToTruth = 0, mapFlipFromTruth = 0;
  // generalised information: change in a teammate's mean per-card certainty
  long long moverStates = 0, playedVacuous = 0, someAskInformative = 0;
  double sumPlayedDC = 0, sumMaxDC = 0;
  std::vector<double> dcPlayed, dcMax;
  long long statesLocked = 0, statesNoLock = 0;
  long long rankN = 0, sumInfoRank = 0;
  double sumInfoDeficit = 0;
};

inline void pctl(std::vector<double>& v, std::ostream& os, const char* tag) {
  if (v.empty()) { os << tag << ": (none)\n"; return; }
  std::sort(v.begin(), v.end());
  auto q = [&](double f) { return v[std::min(v.size() - 1, size_t(f * v.size()))]; };
  double sum = 0; for (double x : v) sum += x;
  os << tag << ": n=" << v.size() << "  mean " << (sum / v.size())
     << "  p50 " << q(0.5) << "  p90 " << q(0.90) << "  p99 " << q(0.99)
     << "  max " << v.back() << "\n";
}

// Mean posterior certainty about where the unresolved cards are, from the
// exact block DP.  The generalised information / leakage metric.
inline double dlCertainty(const Knowledge& k) {
  BlockDP b;
  if (!b.build(k)) return -1;
  static thread_local double mu[NCARD][NPLAY];
  for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) mu[c][p] = 0;
  b.marginals(mu);
  double acc = 0; int n = 0;
  uint64_t u = k.unresolved;
  while (u) { int c = __builtin_ctzll(u); u &= u - 1;
    double m = 0; for (int p = 0; p < NPLAY; p++) m = std::max(m, mu[c][p]);
    acc += m; n++; }
  return n ? acc / n : 1.0;
}

// ------------------------------------------------------------ state report
struct SeqStat;
inline void dlSequence(const Game& G, int team, int s, int maxSteps, SeqStat& st, std::ostream* os);

struct DLAnalysisCfg {
  bool dump = true;          // print the full characterisation of this state
  InfoStat* info = nullptr;  // accumulate the part-2 distribution
  std::ostream* os = nullptr;
  double* turnCostHalfSuits = nullptr;
  SeqStat* seq = nullptr;
  int seqSteps = 8;
};

// Everything that happens at one decision state.  `actor` is the seat about to
// move (taken from the trace, because Game::g.turn is not yet updated when the
// post-event observer fires).
inline void dlAnalyseState(const Game& G, int actor, const Event& actual, const DLAnalysisCfg& ac) {
  std::ostream& os = ac.os ? *ac.os : std::cout;
  PublicState P = G.g.pub; P.history.clear(); P.turn = actor;

  int tm[2] = {0, 0};
  for (int p = 0; p < NPLAY; p++) tm[teamOf(p)] |= 1 << p;

  // ground-truth lock status of every live half-suit
  int lockOwner[NSET];
  for (int s = 0; s < NSET; s++) {
    lockOwner[s] = -1;
    if (!P.setActive[s]) continue;
    uint64_t t0 = G.g.hand[0] | G.g.hand[2] | G.g.hand[4];
    uint64_t t1 = G.g.hand[1] | G.g.hand[3] | G.g.hand[5];
    uint64_t m = setMask(s);
    if ((t0 & m) == m) lockOwner[s] = 0;
    else if ((t1 & m) == m) lockOwner[s] = 1;
  }

  if (ac.dump) {
    os << "  event " << P.nEvents << "  turn=seat" << actor << " (team " << teamOf(actor) << ")"
       << "  score " << int(P.score[0]) << "-" << int(P.score[1]) << "\n";
    os << "  hand counts";
    for (int p = 0; p < NPLAY; p++) os << " s" << p << "=" << int(P.handCount[p]);
    os << "\n";
    int live = P.activeSets(), l0 = 0, l1 = 0, split = 0;
    for (int s = 0; s < NSET; s++) if (P.setActive[s]) {
      if (lockOwner[s] == 0) l0++; else if (lockOwner[s] == 1) l1++; else split++;
    }
    os << "  live half-suits " << live << "  locked-to-team0 " << l0
       << "  locked-to-team1 " << l1 << "  genuinely split " << split << "\n";
    os << "  unresolved cards per observer:";
    for (int p = 0; p < NPLAY; p++) os << " s" << p << "=" << __builtin_popcountll(G.agents[p]->k.unresolved);
    os << "\n  live ask-legality certificates held per observer:";
    for (int p = 0; p < NPLAY; p++) os << " s" << p << "=" << G.agents[p]->k.disj.size();
    os << "\n  exact mean per-card certainty per observer:" << std::setprecision(6);
    for (int p = 0; p < NPLAY; p++) os << " " << dlCertainty(G.agents[p]->k);
    os << "\n";
    for (int s = 0; s < NSET; s++) {
      if (!P.setActive[s] || lockOwner[s] < 0) continue;
      os << "  LOCKED set " << s << " (" << setName(s) << ") to team " << lockOwner[s] << ":  truth";
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        for (int p = 0; p < NPLAY; p++) if (G.g.hand[p] & bit(c)) os << " " << cardName(c) << "@s" << p;
      }
      os << "\n";
      for (int p = 0; p < NPLAY; p++) {
        if (teamOf(p) != lockOwner[s]) continue;
        AllocQ q = dlQuery(G.agents[p]->k, s, tm[lockOwner[s]]);
        bool right = true;
        for (int i = 0; i < SETSZ; i++) {
          int c = cardOf(s, i);
          if (q.seats[i] < 0 || !(G.g.hand[q.seats[i]] & bit(c))) right = false;
        }
        os << "      observer s" << p << "  pTeam " << std::setprecision(6) << q.pTeam
           << "  pAlloc(MAP) " << q.pAlloc << "  MAP-is-truth " << (right ? "yes" : "NO") << "\n";
      }
    }
  }

  // --- the v0.4 ask score at this state -------------------------------------
  V04Agent* A = dynamic_cast<V04Agent*>(G.agents[actor]);
  if (A && ac.dump) {
    int saveSet = A->lastMySet; double saveP = A->lastAskP;
    A->refresh();
    if (A->cfg.useValue) A->computeAggregates(P);
    A->prepareRunway(P);
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(P, A->k.myHand, actor, buf);
    struct Row { double u, lin, val, p; int card, target; bool locked; };
    std::vector<Row> rows;
    double f[NFEAT];
    for (int i = 0; i < n; i++) {
      A->features(P, buf[i].card, buf[i].target, f);
      double lin = 0; for (int j = 0; j < NFEAT; j++) lin += A->cfg.w[j] * f[j];
      lin *= A->cfg.linearWeight;
      double val = A->cfg.useValue
                 ? A->cfg.valueWeight * A->askExpectedValue(P, buf[i].card, buf[i].target, f[0]) : 0.0;
      int s = setOf(buf[i].card);
      rows.push_back(Row{lin + val, lin, val, f[0], buf[i].card, buf[i].target,
                         lockOwner[s] == teamOf(actor)});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.u > b.u; });
    // provably-dead share of the legal menu, and where the best live ask ranks
    int nDead = 0, rankLive = -1;
    for (int i = 0; i < int(rows.size()); i++) {
      const Knowledge& kk = A->k;
      bool d = (kk.owner[rows[i].card] < NPLAY) ? (kk.owner[rows[i].card] != rows[i].target)
                                                : !(kk.mask[rows[i].card] & (1u << rows[i].target));
      if (d) nDead++; else if (rankLive < 0) rankLive = i + 1;
    }
    os << "  provably-dead legal asks " << nDead << " of " << n
       << ";  best rank of an ask that is NOT provably dead: "
       << (rankLive < 0 ? std::string("none") : std::to_string(rankLive)) << "\n";
    os << "  legal asks " << n << ";  v0.4 ranking (score = " << A->cfg.linearWeight
       << "*linear + " << A->cfg.valueWeight << "*EV):\n";
    for (int i = 0; i < int(rows.size()) && i < 6; i++) {
      const Row& r = rows[i];
      os << "     #" << (i + 1) << "  ask " << cardName(r.card) << " (set " << setOf(r.card)
         << ") from s" << r.target << "   p(hit)=" << std::setprecision(4) << r.p
         << "   u=" << r.u << " (lin " << r.lin << ", ev " << r.val << ")"
         << (r.locked ? "   [inside our own locked half-suit -> guaranteed miss]" : "") << "\n";
    }
    // why did #1 beat the best live ask?  per-feature weighted contributions.
    {
      static const char* fn[NFEAT] = {
        "p(hit)","p^2","certain","myHave","teamExp","pTeamOther","continuation","completion",
        "replyThreat","notRevealed","targetHand","emptiesTarget","repeatSet","teamKnown",
        "entropy","teamOwnsAll","exposureOnMiss","trailing","runway","leakMag"};
      int iBest = 0, iLive = -1;
      for (int i = 0; i < int(rows.size()); i++) {
        const Knowledge& kk = A->k;
        bool d = (kk.owner[rows[i].card] < NPLAY) ? (kk.owner[rows[i].card] != rows[i].target)
                                                  : !(kk.mask[rows[i].card] & (1u << rows[i].target));
        if (!d) { iLive = i; break; }
      }
      if (iLive > iBest) {
        double fa[NFEAT], fb[NFEAT];
        A->features(P, rows[iBest].card, rows[iBest].target, fa);
        A->features(P, rows[iLive].card, rows[iLive].target, fb);
        os << "     WHY: #1 (" << cardName(rows[iBest].card) << "@s" << rows[iBest].target
           << ", dead) vs best live #" << (iLive + 1) << " (" << cardName(rows[iLive].card)
           << "@s" << rows[iLive].target << ", p=" << rows[iLive].p << ")"
           << "   linear gap " << (rows[iBest].lin - rows[iLive].lin)
           << ", EV gap " << (rows[iBest].val - rows[iLive].val) << "\n";
        struct C { double d; int j; };
        std::vector<C> cs;
        for (int j = 0; j < NFEAT; j++)
          cs.push_back(C{A->cfg.linearWeight * A->cfg.w[j] * (fa[j] - fb[j]), j});
        std::sort(cs.begin(), cs.end(), [](const C& x, const C& y) { return x.d > y.d; });
        os << "       top contributions to the gap:";
        for (int j = 0; j < 4; j++)
          os << "  " << fn[cs[j].j] << " " << std::showpos << cs[j].d << std::noshowpos;
        os << "\n       worst:";
        for (int j = 0; j < 3; j++) {
          const C& c = cs[cs.size() - 1 - j];
          os << "  " << fn[c.j] << " " << std::showpos << c.d << std::noshowpos;
        }
        os << "\n";
      }
    }
    // where do the certificate-bearing asks rank?
    int bestLockedRank = -1;
    for (int i = 0; i < int(rows.size()); i++) if (rows[i].locked) { bestLockedRank = i + 1; break; }
    os << "     best rank of an own-locked (certificate-only) ask: "
       << (bestLockedRank < 0 ? std::string("none legal") : std::to_string(bestLockedRank))
       << " of " << n << "\n";
    os << "     played: " << cardName(actual.card) << " from s" << int(actual.target)
       << "  -> " << (actual.success ? "HIT" : "miss") << "\n";
    // turn-donation cost in the shipped value function, in half-suits
    int sd = int(P.score[teamOf(actor)]) - int(P.score[1 - teamOf(actor)]);
    double vKeep = A->value(P, 0, 0, 0, 0, sd, +1, 0, 0, 0, 0);
    double vGive = A->value(P, 0, 0, 0, 0, sd, -1, 0, 0, 0, 0);
    os << "     turn-donation cost in v0.4's value function: " << std::setprecision(6)
       << (vKeep - vGive) << " scaled units = " << ((vKeep - vGive) * 9.0) << " half-suits\n";
    if (ac.turnCostHalfSuits) *ac.turnCostHalfSuits = (vKeep - vGive) * 9.0;
    A->lastMySet = saveSet; A->lastAskP = saveP; A->dirty = true;
  }

  // --- the certificate ladder: how far can a team get by asking on purpose? --
  if (ac.seq) {
    for (int team = 0; team < 2; team++) {
      int best = -1; double bestv = -1;
      int mask = 0; for (int p = 0; p < NPLAY; p++) if (teamOf(p) == team) mask |= 1 << p;
      for (int s = 0; s < NSET; s++) {
        if (!P.setActive[s] || lockOwner[s] != team) continue;
        double b = 0;
        for (int p = 0; p < NPLAY; p++) if (teamOf(p) == team) { AllocQ q = dlQuery(G.agents[p]->k, s, mask); b = std::max(b, q.pAlloc); }
        if (b > bestv) { bestv = b; best = s; }
      }
      if (best >= 0) dlSequence(G, team, best, ac.seqSteps, *ac.seq, ac.dump ? &os : nullptr);
    }
  }

  if (!ac.info) return;
  {
    // --- generalised information: does ANY legal ask move a teammate's exact
    // posterior at all, and does the one v0.4 actually plays?
    InfoStat& st = *ac.info;
    int myTeam = teamOf(actor);
    bool anyLock = false;
    for (int s = 0; s < NSET; s++) if (P.setActive[s] && lockOwner[s] >= 0) anyLock = true;
    if (anyLock) st.statesLocked++; else st.statesNoLock++;
    double base[NPLAY];
    for (int p = 0; p < NPLAY; p++) base[p] = (teamOf(p) == myTeam && p != actor) ? dlCertainty(G.agents[p]->k) : -1;
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(P, G.agents[actor]->k.myHand, actor, buf);
    double maxDC = 0, playedDC = 0; int argMax = -1;
    std::vector<double> dcs(n, 0.0);
    for (int i = 0; i < n; i++) {
      double d = 0;
      for (int p = 0; p < NPLAY; p++) {
        if (base[p] < 0) continue;
        Knowledge kh = dlHypoMiss(G.agents[p]->k, P.handCount, actor, buf[i].card, buf[i].target);
        double c = dlCertainty(kh);
        if (c >= 0) d = std::max(d, c - base[p]);
      }
      dcs[i] = d;
      if (d > maxDC) { maxDC = d; argMax = i; }
      if (buf[i].card == actual.card && buf[i].target == actual.target) playedDC = d;
    }
    // where does the most informative ask sit in v0.4's own ranking?
    if (A && argMax >= 0) {
      int saveSet = A->lastMySet; double saveP = A->lastAskP;
      A->refresh(); if (A->cfg.useValue) A->computeAggregates(P); A->prepareRunway(P);
      double f2[NFEAT];
      auto score = [&](int i) {
        A->features(P, buf[i].card, buf[i].target, f2);
        double u = 0; for (int j = 0; j < NFEAT; j++) u += A->cfg.w[j] * f2[j];
        u *= A->cfg.linearWeight;
        if (A->cfg.useValue) u += A->cfg.valueWeight * A->askExpectedValue(P, buf[i].card, buf[i].target, f2[0]);
        return u;
      };
      double uBest = -1e18, uInfo = score(argMax);
      int rank = 1;
      for (int i = 0; i < n; i++) { double u = score(i); if (u > uBest) uBest = u; if (u > uInfo) rank++; }
      st.sumInfoRank += rank; st.sumInfoDeficit += (uBest - uInfo); st.rankN++;
      if (ac.dump)
        os << "     the most informative legal ask (" << cardName(buf[argMax].card) << "@s"
           << int(buf[argMax].target) << ") ranks #" << rank << " of " << n
           << " in v0.4's own score, " << (uBest - uInfo) << " below the leader\n";
      A->lastMySet = saveSet; A->lastAskP = saveP; A->dirty = true;
    }
    st.moverStates++;
    st.sumPlayedDC += playedDC; st.sumMaxDC += maxDC;
    st.dcPlayed.push_back(playedDC); st.dcMax.push_back(maxDC);
    if (playedDC <= 1e-12) st.playedVacuous++;
    if (maxDC > 1e-12) st.someAskInformative++;
    if (ac.dump)
      os << "     information audit: the ask v0.4 played raises a teammate's mean per-card certainty by "
         << std::setprecision(6) << playedDC << "; the best available legal ask raises it by " << maxDC << "\n";
  }

  // --- part 2: does an ask move a TEAMMATE's exact posterior? ---------------
  InfoStat& st = *ac.info;
  for (int a = 0; a < NPLAY; a++) {
    int myTeam = teamOf(a), mask = tm[myTeam];
    // the team's best candidate: a live half-suit the team provably owns, ranked
    // by the best pAlloc any team member currently has for it.
    int cand = -1; double bestBefore = -1;
    double before[NSET][NPLAY];
    for (int s = 0; s < NSET; s++) for (int p = 0; p < NPLAY; p++) before[s][p] = -1;
    for (int s = 0; s < NSET; s++) {
      if (!P.setActive[s] || lockOwner[s] != myTeam) continue;
      double bm = -1;
      for (int p = 0; p < NPLAY; p++) {
        if (teamOf(p) != myTeam) continue;
        AllocQ q = dlQuery(G.agents[p]->k, s, mask);
        before[s][p] = q.ok ? q.pAlloc : -1;
        bm = std::max(bm, before[s][p]);
      }
      if (bm > bestBefore) { bestBefore = bm; cand = s; }
    }
    if (cand < 0) continue;                       // team owns no locked half-suit here
    st.states++;
    st.sumBestBefore += bestBefore;

    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(P, G.agents[a]->k.myHand, a, buf);
    double bestAfterAnyAsk = bestBefore;
    for (int i = 0; i < n; i++) {
      int c = buf[i].card, t = buf[i].target;
      bool inLocked = (lockOwner[setOf(c)] == myTeam);
      st.asks++; if (inLocked) st.asksLocked++;
      double dBest = 0;
      for (int p = 0; p < NPLAY; p++) {
        if (teamOf(p) != myTeam || p == a) continue;   // only teammates learn
        if (before[cand][p] < 0) continue;
        Knowledge kh = dlHypoMiss(G.agents[p]->k, P.handCount, a, c, t);
        AllocQ q2 = dlQuery(kh, cand, mask);
        if (!q2.ok) continue;
        double d = q2.pAlloc - before[cand][p];
        if (d > dBest) dBest = d;
        if (q2.pAlloc > bestAfterAnyAsk) bestAfterAnyAsk = q2.pAlloc;
        // did the MAP allocation move toward or away from the truth?
        bool r1 = true, r2 = true;
        AllocQ q1 = dlQuery(G.agents[p]->k, cand, mask);
        for (int j = 0; j < SETSZ; j++) {
          int cc = cardOf(cand, j);
          if (q1.seats[j] < 0 || !(G.g.hand[q1.seats[j]] & bit(cc))) r1 = false;
          if (q2.seats[j] < 0 || !(G.g.hand[q2.seats[j]] & bit(cc))) r2 = false;
        }
        if (!r1 && r2) st.mapFlipToTruth++;
        if (r1 && !r2) st.mapFlipFromTruth++;
      }
      if (inLocked) {
        st.sumLocked += dBest; st.dLocked.push_back(dBest);
        if (dBest > 1e-9) st.posLocked++;
        st.maxLocked = std::max(st.maxLocked, dBest);
      } else {
        st.sumOther += dBest; st.dOther.push_back(dBest);
        if (dBest > 1e-9) st.posOther++;
        st.maxOther = std::max(st.maxOther, dBest);
      }
    }
    st.sumBestAfter += bestAfterAnyAsk;
  }
}

// ----------------------------------- how many certificate asks resolve a lock
// Mean posterior certainty about where the unresolved cards are, from the exact
// block DP.  Used as the leakage metric for the opponents.

struct SeqStat {
  long long seqs = 0, solved = 0;
  long long stepsToSolve = 0;
  double sumStart = 0, sumEnd = 0;
  double sumLeakStart = 0, sumLeakEnd = 0;
  double sumOppAllocStart = 0, sumOppAllocEnd = 0;
  long long oppSeqs = 0;
  std::vector<int> steps;
};

// Greedily play certificate-only asks inside one team-locked half-suit and watch
// the team's exact posterior over its allocation.  Every certificate is public,
// so it is applied to all six observers.
inline void dlSequence(const Game& G, int team, int s, int maxSteps, SeqStat& st, std::ostream* os) {
  int mask = 0, omask = 0;
  for (int p = 0; p < NPLAY; p++) { if (teamOf(p) == team) mask |= 1 << p; else omask |= 1 << p; }
  Knowledge k[NPLAY];
  for (int p = 0; p < NPLAY; p++) k[p] = G.agents[p]->k;
  PublicState P = G.g.pub; P.history.clear();

  auto teamBest = [&]() {
    double b = 0;
    for (int p = 0; p < NPLAY; p++) if (mask & (1 << p)) { AllocQ q = dlQuery(k[p], s, mask); b = std::max(b, q.pAlloc); }
    return b;
  };
  auto oppBest = [&]() {                     // opponents' own best locked half-suit
    double b = -1;
    for (int s2 = 0; s2 < NSET; s2++) {
      if (!P.setActive[s2]) continue;
      uint64_t m = setMask(s2), oh = 0;
      for (int p = 0; p < NPLAY; p++) if (omask & (1 << p)) oh |= G.g.hand[p];
      if ((oh & m) != m) continue;
      for (int p = 0; p < NPLAY; p++) if (omask & (1 << p)) { AllocQ q = dlQuery(k[p], s2, omask); b = std::max(b, q.pAlloc); }
    }
    return b;
  };
  auto oppCert = [&]() {
    double acc = 0; int n = 0;
    for (int p = 0; p < NPLAY; p++) if (omask & (1 << p)) { double c = dlCertainty(k[p]); if (c >= 0) { acc += c; n++; } }
    return n ? acc / n : -1;
  };

  double start = teamBest();
  double leak0 = oppCert(), oa0 = oppBest();
  st.seqs++; st.sumStart += start;
  if (leak0 >= 0) st.sumLeakStart += leak0;
  if (oa0 >= 0) { st.sumOppAllocStart += oa0; st.oppSeqs++; }
  if (os) *os << "      certificate ladder in set " << s << " for team " << team
              << ": pAlloc(team best) " << std::setprecision(4) << start;
  int used = 0;
  for (int step = 0; step < maxSteps; step++) {
    double bestVal = teamBest(); int bc = -1, bt = -1, ba = -1;
    for (int a = 0; a < NPLAY; a++) {
      if (!(mask & (1 << a))) continue;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        if (!(G.g.hand[a] & setMask(s))) break;
        if (G.g.hand[a] & bit(c)) continue;
        for (int t = 0; t < NPLAY; t++) {
          if (teamOf(t) == team || !P.handCount[t]) continue;
          Knowledge tmp[NPLAY];
          double v = 0;
          for (int p = 0; p < NPLAY; p++) {
            if (!(mask & (1 << p))) continue;
            tmp[p] = dlHypoMiss(k[p], P.handCount, a, c, t);
            AllocQ q = dlQuery(tmp[p], s, mask);
            v = std::max(v, q.pAlloc);
          }
          if (v > bestVal + 1e-9) { bestVal = v; bc = c; bt = t; ba = a; }
        }
      }
    }
    if (bc < 0) break;
    for (int p = 0; p < NPLAY; p++) k[p] = dlHypoMiss(k[p], P.handCount, ba, bc, bt);
    used++;
    if (os) *os << " -> " << teamBest();
    if (teamBest() > 0.9995) break;
  }
  double end = teamBest();
  st.sumEnd += end;
  st.steps.push_back(used);
  if (end > 0.9995) { st.solved++; st.stepsToSolve += used; }
  double leak1 = oppCert(), oa1 = oppBest();
  if (leak1 >= 0) st.sumLeakEnd += leak1;
  if (oa1 >= 0) st.sumOppAllocEnd += oa1;
  if (os) *os << "   (" << used << " certificate asks; opponents' mean card certainty "
              << leak0 << " -> " << leak1;
  if (os && oa0 >= 0) *os << ", opponents' own best pAlloc " << oa0 << " -> " << oa1;
  if (os) *os << ")\n";
}

// ------------------------------------------------------- part 3: turn cost
struct TakeStat {
  long long donations = 0;      // misses that handed the turn to the opponents
  long long cards = 0;          // cards the receiving team then took
  long long declares = 0;       // half-suits the receiving team then scored
  long long zero = 0;           // donations after which they took nothing
};

// After a miss the turn goes to the target.  Count what the receiving team takes
// before it hands the turn back (its own first miss), plus any half-suit it
// declares in that window.
inline void dlTake(const DLGameInfo& gi, TakeStat& inDead, TakeStat& outDead, int deadRunMin) {
  // mark asks belonging to a dead run of length >= deadRunMin
  std::vector<char> inRun(gi.ev.size(), 0);
  { int run = 0, from = -1;
    for (size_t i = 0; i < gi.ev.size(); i++) {
      if (gi.ev[i].kind != Kind::Ask) continue;
      if (gi.dead[i]) { if (!run) from = int(i); run++; }
      else { if (run >= deadRunMin) for (int j = from; j < int(i); j++) inRun[j] = 1; run = 0; }
    }
    if (run >= deadRunMin) for (int j = from; j < int(gi.ev.size()); j++) inRun[j] = 1;
  }
  for (size_t i = 0; i < gi.ev.size(); i++) {
    const Event& e = gi.ev[i];
    if (e.kind != Kind::Ask || e.success) continue;
    TakeStat& st = inRun[i] ? inDead : outDead;
    st.donations++;
    int recv = teamOf(e.target);
    long long c0 = 0, d0 = 0;
    for (size_t j = i + 1; j < gi.ev.size(); j++) {
      const Event& x = gi.ev[j];
      if (x.kind == Kind::Declare || x.kind == Kind::ForcedDeclare) {
        if (teamOf(x.actor) == recv && x.success) d0++;
        continue;
      }
      if (x.kind != Kind::Ask) continue;
      if (teamOf(x.actor) != recv) break;
      if (!x.success) break;
      c0++;
    }
    st.cards += c0; st.declares += d0;
    if (!c0 && !d0) st.zero++;
  }
}

// Re-run one game and analyse it at the given event indices.
inline void dlRunAt(const std::string& spec, const DLGameInfo& gi, const Rules& rules,
                    const std::vector<int>& idxs, bool dump, InfoStat* info, std::ostream& os,
                    std::vector<double>* turnCosts, SeqStat* seq = nullptr, int seqSteps = 8) {
  std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
  for (int p = 0; p < NPLAY; p++) { ag[p] = makeAgent(spec); ap[p] = ag[p].get(); }
  Game game; game.rotation = gi.rot;
  std::vector<char> want(gi.ev.size() + 2, 0);
  for (int i : idxs) if (i >= 0 && i < int(gi.ev.size())) want[i] = 1;
  game.observer = [&](const Game& G) {
    int t = G.g.pub.nEvents;                       // state right before event t
    if (t >= int(gi.ev.size()) || !want[t]) return;
    if (gi.ev[t].kind != Kind::Ask) return;
    DLAnalysisCfg ac; ac.dump = dump; ac.info = info; ac.os = &os;
    ac.seq = seq; ac.seqSteps = seqSteps;
    double tc = 0; ac.turnCostHalfSuits = &tc;
    dlAnalyseState(G, gi.ev[t].actor, gi.ev[t], ac);
    if (turnCosts && tc != 0) turnCosts->push_back(tc);
  };
  game.run(gi.seed, rules, ap);
}

// Head-to-head: does the no-repeat tie-break cost material against v0.4?
inline void dlHeadToHead(const std::string& spec, int deals, uint64_t seed, const Rules& rules, std::ostream& os) {
  long long setsA = 0, setsB = 0, winsA = 0, n = 0, ev = 0;
  long long declA = 0, wrongA = 0, declB = 0, wrongB = 0;
  for (int i = 0; i < deals; i++) {
    uint64_t sd = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    for (int orient = 0; orient < 2; orient++) {
      std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
      for (int p = 0; p < NPLAY; p++) {
        if (teamOf(p) == orient) ag[p] = std::make_unique<NoRepeatV04>();
        else ag[p] = makeAgent(spec);
        ap[p] = ag[p].get();
      }
      Game game;
      GameResult r = game.run(sd, rules, ap);
      int a = r.score[orient], b = r.score[1 - orient];
      setsA += a; setsB += b; if (a > b) winsA++;
      ev += r.events; n++;
      declA += r.decls[orient] + r.forcedDecls[orient];
      wrongA += r.decls[orient] - r.correctDecls[orient] + r.forcedDecls[orient] - r.forcedCorrect[orient];
      declB += r.decls[1 - orient] + r.forcedDecls[1 - orient];
      wrongB += r.decls[1 - orient] - r.correctDecls[1 - orient] + r.forcedDecls[1 - orient] - r.forcedCorrect[1 - orient];
    }
  }
  os << "  no-repeat v0.4 vs shipped v0.4: " << n << " games, win rate "
     << (100.0 * double(winsA) / n) << "%,  mean sets " << (double(setsA) / n)
     << " vs " << (double(setsB) / n) << ",  events/game " << (double(ev) / n) << "\n";
  os << "  misdeclaration rate: no-repeat " << (100.0 * double(wrongA) / std::max(1LL, declA))
     << "%  vs shipped " << (100.0 * double(wrongB) / std::max(1LL, declB)) << "%\n";
}

struct DeadlockCfg {
  std::string spec = "v04";
  int games = 60, rotations = 2;
  uint64_t seed = 31;
  int minEvents = 300;
  int dump = 4;
  int stride = 40, maxStates = 3, seqSteps = 8;
  int deadRunMin = 6, h2h = 0;
  Rules rules;
};

inline void runDeadlockProbe(const DeadlockCfg& dc, std::ostream& os) {
  os << std::fixed;
  std::vector<DLGameInfo> all;
  for (int i = 0; i < dc.games; i++) {
    uint64_t s = mixSeed(dc.seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < dc.rotations; rot++) {
      DLGameInfo gi; dlScan(dc.spec, s, rot, dc.rules, gi);
      all.push_back(std::move(gi));
    }
  }
  std::vector<int> longGames;
  for (int i = 0; i < int(all.size()); i++) if (all[i].events > dc.minEvents) longGames.push_back(i);
  std::sort(longGames.begin(), longGames.end(),
            [&](int a, int b) { return all[a].runLen > all[b].runLen; });
  os << "scanned " << all.size() << " games (" << dc.spec << " mirror, seed " << dc.seed
     << ")   games with >" << dc.minEvents << " events: " << longGames.size() << "\n\n";

  { // census of the long games
    int pure2 = 0, anyLock = 0; long long dt = 0, rl = 0;
    for (int i : longGames) {
      const DLGameInfo& g = all[i];
      dt += g.distinctTriples; rl += g.runLen;
      if (g.distinctTriples <= 2) pure2++;
      if (g.lockedAtStart[0] + g.lockedAtStart[1] + g.lockedAtEnd[0] + g.lockedAtEnd[1] > 0) anyLock++;
    }
    int n = int(longGames.size());
    if (n) {
      os << "long-game census: mean longest dead run " << (double(rl) / n)
         << " asks, mean distinct (actor,card,target) triples in it " << (double(dt) / n) << "\n";
      os << "  runs that are a pure two-question cycle: " << pure2 << " / " << n << "\n";
      os << "  runs where ANY half-suit was already locked to a team (ground truth) at the run's start or end: "
         << anyLock << " / " << n << "\n\n";
    }
  }

  // ---- part 1: concrete traces -------------------------------------------
  os << "=== PART 1: deadlock onset traces ===\n";
  InfoStat info;
  SeqStat seq;
  std::vector<double> turnCosts;
  int nd = std::min<int>(dc.dump, int(longGames.size()));
  for (int gi_ = 0; gi_ < nd; gi_++) {
    const DLGameInfo& gi = all[longGames[gi_]];
    os << "\n--- game seed=" << gi.seed << " rot=" << gi.rot << "  events=" << gi.events
       << "  longest dead run=" << gi.runLen << " starting at event " << gi.runStart << "\n";
    {   // structure of the run: how many distinct questions does it contain?
      std::map<uint32_t, int> tally;
      int end = std::min<int>(gi.events, gi.runStart + gi.runLen);
      for (int j = gi.runStart; j < end; j++) {
        const Event& e = gi.ev[j];
        if (e.kind != Kind::Ask) continue;
        tally[(uint32_t(e.actor) << 16) | (uint32_t(e.card) << 8) | e.target]++;
      }
      std::vector<std::pair<int, uint32_t>> v;
      for (auto& kv : tally) v.push_back({kv.second, kv.first});
      std::sort(v.rbegin(), v.rend());
      os << "  run structure: " << (end - gi.runStart) << " asks, " << tally.size()
         << " distinct (actor,card,target) triples";
      for (int j = 0; j < 3 && j < int(v.size()); j++) {
        int a = v[j].second >> 16, c = (v[j].second >> 8) & 0xFF, t = v[j].second & 0xFF;
        os << "; s" << a << " asks " << cardName(c) << " of s" << t << " x" << v[j].first;
      }
      os << "\n  first asks of the run:";
      for (int j = gi.runStart; j < std::min(gi.runStart + 10, end); j++) {
        const Event& e = gi.ev[j];
        if (e.kind != Kind::Ask) continue;
        os << " [s" << int(e.actor) << "->s" << int(e.target) << " " << cardName(e.card) << "]";
      }
      os << "\n";
    }
    std::vector<int> idxs; idxs.push_back(gi.runStart);
    for (int j = 1; j < dc.maxStates; j++) {
      int t = gi.runStart + j * dc.stride;
      if (t < gi.runStart + gi.runLen && t < gi.events) idxs.push_back(t);
    }
    for (size_t j = 0; j < idxs.size(); j++) {
      os << (j == 0 ? "  [onset]\n" : "  [later in the run]\n");
      std::vector<int> one{idxs[j]};
      dlRunAt(dc.spec, gi, dc.rules, one, true, &info, os, &turnCosts, &seq, dc.seqSteps);
    }
  }

  // ---- part 2 over more states -------------------------------------------
  os << "\n=== PART 2: is information obtainable in a deadlocked state? ===\n";
  int extra = std::min<int>(int(longGames.size()), 24);
  for (int gi_ = nd; gi_ < extra; gi_++) {
    const DLGameInfo& gi = all[longGames[gi_]];
    std::vector<int> idxs;
    for (int j = 0; j < dc.maxStates; j++) {
      int t = gi.runStart + j * dc.stride;
      if (t >= 0 && t < gi.runStart + gi.runLen && t < gi.events) idxs.push_back(t);
    }
    if (idxs.empty()) continue;
    for (int t : idxs) { std::vector<int> one{t};
      dlRunAt(dc.spec, gi, dc.rules, one, false, &info, os, nullptr, &seq, dc.seqSteps); }
  }
  os << "states examined (seat x state pairs with a team-locked half-suit): " << info.states << "\n";
  os << "team-best pAlloc on the candidate half-suit: mean before "
     << (info.states ? info.sumBestBefore / info.states : 0)
     << "   mean best-after-one-ask " << (info.states ? info.sumBestAfter / info.states : 0) << "\n";
  os << "legal asks examined " << info.asks << "  of which inside our own locked half-suit "
     << info.asksLocked << "\n";
  os << "  own-locked asks with dP > 0 for a teammate: " << info.posLocked << " / " << info.asksLocked
     << "  (" << (info.asksLocked ? 100.0 * double(info.posLocked) / double(info.asksLocked) : 0) << "%)"
     << "  mean dP " << (info.asksLocked ? info.sumLocked / info.asksLocked : 0)
     << "  max dP " << info.maxLocked << "\n";
  os << "  other asks with dP > 0 for a teammate:      " << info.posOther << " / "
     << (info.asks - info.asksLocked)
     << "  (" << ((info.asks - info.asksLocked) ? 100.0 * double(info.posOther) / double(info.asks - info.asksLocked) : 0) << "%)"
     << "  mean dP " << ((info.asks - info.asksLocked) ? info.sumOther / double(info.asks - info.asksLocked) : 0)
     << "  max dP " << info.maxOther << "\n";
  pctl(info.dLocked, os, "  dP distribution, own-locked asks");
  pctl(info.dOther, os, "  dP distribution, other asks     ");
  os << "  teammate MAP allocation flipped to the truth " << info.mapFlipToTruth
     << " times, away from the truth " << info.mapFlipFromTruth << " times\n";
  os << "\n  generalised information audit at the mover's decision point (n=" << info.moverStates
     << " states; " << info.statesLocked << " with a locked half-suit, " << info.statesNoLock << " without):\n";
  os << "    the ask v0.4 actually played was information-free for its teammates in "
     << info.playedVacuous << " / " << info.moverStates << " states ("
     << (info.moverStates ? 100.0 * double(info.playedVacuous) / info.moverStates : 0) << "%)\n";
  os << "    at least one legal ask WAS informative in " << info.someAskInformative << " / "
     << info.moverStates << " states ("
     << (info.moverStates ? 100.0 * double(info.someAskInformative) / info.moverStates : 0) << "%)\n";
  os << "    mean d(teammate certainty): played " << (info.moverStates ? info.sumPlayedDC / info.moverStates : 0)
     << "   best available " << (info.moverStates ? info.sumMaxDC / info.moverStates : 0) << "\n";
  pctl(info.dcMax, os, "    best-available d(certainty)");
  if (info.rankN)
    os << "    the most informative legal ask ranks on average #" << (double(info.sumInfoRank) / info.rankN)
       << " in v0.4's own ask score, a mean deficit of " << (info.sumInfoDeficit / info.rankN)
       << " score units below the leader\n";

  // ---- part 3: cost -------------------------------------------------------
  if (seq.seqs) {
    os << "\n--- certificate ladder (greedy information-only asks inside one locked half-suit) ---\n";
    std::sort(seq.steps.begin(), seq.steps.end());
    os << "  ladders run " << seq.seqs << "  mean team-best pAlloc " << (seq.sumStart / seq.seqs)
       << " -> " << (seq.sumEnd / seq.seqs) << "\n";
    os << "  reached pAlloc > 0.9995 (allocation fully resolved): " << seq.solved << " / " << seq.seqs
       << "  (" << (100.0 * double(seq.solved) / double(seq.seqs)) << "%)"
       << "  mean asks needed " << (seq.solved ? double(seq.stepsToSolve) / seq.solved : 0)
       << "  median asks used " << (seq.steps.empty() ? 0 : seq.steps[seq.steps.size() / 2]) << "\n";
    os << "  LEAK: opponents' mean per-card certainty " << (seq.sumLeakStart / seq.seqs)
       << " -> " << (seq.sumLeakEnd / seq.seqs) << "\n";
    if (seq.oppSeqs)
      os << "  LEAK: opponents' own best locked-half-suit pAlloc " << (seq.sumOppAllocStart / seq.oppSeqs)
         << " -> " << (seq.sumOppAllocEnd / seq.oppSeqs) << "  (n=" << seq.oppSeqs << ")\n";
  }

  os << "\n=== PART 3: what the donated turn costs ===\n";
  TakeStat inDead, outDead;
  for (const auto& gi : all) dlTake(gi, inDead, outDead, dc.deadRunMin);
  auto rep = [&](const char* tag, const TakeStat& s) {
    os << tag << ": donations " << s.donations
       << "   cards taken/donation " << (s.donations ? double(s.cards) / s.donations : 0)
       << "   half-suits scored/donation " << (s.donations ? double(s.declares) / s.donations : 0)
       << "   donations yielding nothing " << (s.donations ? 100.0 * double(s.zero) / s.donations : 0) << "%\n";
  };
  rep("  inside a dead run (>=6)", inDead);
  rep("  everywhere else        ", outDead);
  {
    os << "\n=== CAUSAL ISOLATION: v0.4 with exact repetition forbidden ===\n";
    long long ev = 0, asks = 0, dead = 0; int longest = 0, over300 = 0;
    std::vector<int> evs;
    for (int i = 0; i < dc.games; i++) {
      uint64_t sd = mixSeed(dc.seed, uint64_t(i) * 2654435761ull + 1);
      for (int rot = 0; rot < dc.rotations; rot++) {
        DLGameInfo gi; dlScan(dc.spec, sd, rot, dc.rules, gi, true);
        ev += gi.events; evs.push_back(gi.events);
        if (gi.events > dc.minEvents) over300++;
        longest = std::max(longest, gi.runLen);
        for (size_t j = 0; j < gi.ev.size(); j++) if (gi.ev[j].kind == Kind::Ask) { asks++; dead += gi.dead[j]; }
      }
    }
    std::sort(evs.begin(), evs.end());
    int n = int(evs.size());
    os << "  events/game " << (double(ev) / n) << "  median " << evs[n / 2]
       << "  p90 " << evs[std::min(n - 1, int(0.9 * n))] << "  max " << evs.back() << "\n";
    os << "  dead asks " << (100.0 * double(dead) / double(asks)) << "% of asks;  longest dead run "
       << longest << ";  games over " << dc.minEvents << " events " << over300 << " / " << n << "\n";
    long long ev0 = 0; int over0 = 0, long0 = 0;
    for (const auto& g : all) { ev0 += g.events; if (g.events > dc.minEvents) over0++; long0 = std::max(long0, g.runLen); }
    os << "  (baseline v0.4 on the same deals: events/game " << (double(ev0) / all.size())
       << ", longest dead run " << long0 << ", games over " << dc.minEvents << " events "
       << over0 << " / " << all.size() << ")\n";
  }

  if (dc.h2h) dlHeadToHead(dc.spec, dc.h2h, 90210ull, dc.rules, os);

  if (!turnCosts.empty()) {
    double sum = 0, mx = 0; for (double x : turnCosts) { sum += x; mx = std::max(mx, x); }
    os << "  v0.4 value function, turn-donation cost at the dumped deadlock states: mean "
       << (sum / turnCosts.size()) << " half-suits (max " << mx << ", n=" << turnCosts.size() << ")\n";
  }
}

} // namespace fish
