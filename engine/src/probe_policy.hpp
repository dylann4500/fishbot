// P4 adversarial-correctness probe harness.
//
// Scratch tooling for research/v05/results/P4-policy-review.md.  Nothing here is
// on the shipped decision path: it drives `probe_policy_v04.hpp` (an instrumented
// copy of v04.hpp whose `fix` bitmask is 0 by default, reproducing v0.4 exactly).
#pragma once
#include "factory.hpp"
#include "probe_policy_v04.hpp"
#include "arena.hpp"
#include "diag.hpp"
#include <thread>

namespace fish {
namespace p4 {

inline std::unique_ptr<Agent> makeP4(const std::string& spec) {
  std::string base;
  auto o = parseOpts(spec, base);
  if (base != "p4") return makeAgent(spec);
  auto a = std::make_unique<P4Agent>();
  auto it = o.find("belief");
  if (it != o.end()) {
    if (it->second == "block") a->cfg.belief = BeliefMode::Block;
    else if (it->second == "fast") a->cfg.belief = BeliefMode::Fast;
    else if (it->second == "exact") a->cfg.belief = BeliefMode::Exact;
    else if (it->second == "indep") a->cfg.belief = BeliefMode::Independent;
  }
  a->cfg.priorTheta   = optD(o, "ptheta", a->cfg.priorTheta);
  a->cfg.priorPhi     = optD(o, "pphi", a->cfg.priorPhi);
  a->cfg.useValue     = optI(o, "value", a->cfg.useValue ? 1 : 0) != 0;
  a->cfg.declThreshold= optD(o, "decl", a->cfg.declThreshold);
  a->cfg.askFloor     = optD(o, "askfloor", a->cfg.askFloor);
  a->cfg.patiencePool = optI(o, "pool", a->cfg.patiencePool);
  a->cfg.oppCardFloor = optD(o, "oppfloor", a->cfg.oppCardFloor);
  a->cfg.fix          = optI(o, "fix", 0);
  a->cfg.instrument   = optI(o, "instr", 0) != 0;
  a->cfg.searchTopK   = optI(o, "topk", a->cfg.searchTopK);
  a->cfg.chainWeight  = optD(o, "chain", a->cfg.chainWeight);
  a->cfg.threatWeight = optD(o, "threat", a->cfg.threatWeight);
  a->cfg.forceDeclareEvents = optI(o, "force", a->cfg.forceDeclareEvents);
  a->cfg.declareMargin= optD(o, "vmargin", a->cfg.declareMargin);
  a->cfg.minTeamProb  = optD(o, "minteam", a->cfg.minTeamProb);
  a->cfg.valueWeight  = optD(o, "vweight", a->cfg.valueWeight);
  a->cfg.linearWeight = optD(o, "lweight", a->cfg.linearWeight);
  a->cfg.valueDeclare = optI(o, "vdecl", a->cfg.valueDeclare ? 1 : 0) != 0;
  for (int i = 0; i < NFEAT; i++) { char key[8]; snprintf(key, sizeof(key), "w%d", i);
    a->cfg.w[i] = optD(o, key, a->cfg.w[i]); }
  auto vv = o.find("vweights");
  if (vv != o.end()) { std::stringstream vs(vv->second); std::string tok; int i = 0;
    while (std::getline(vs, tok, '|') && i < NVFEAT) a->cfg.vw[i++] = atof(tok.c_str()); }
  a->label = "p4";
  return a;
}

// Mirror self-play with instrumentation on every seat.  Single-threaded so the
// per-agent P4Stats can simply be summed.
inline P4Stats runProbe(const std::string& spec, int games, uint64_t seed,
                        const Rules& rules, MatchStats* ms = nullptr) {
  std::unique_ptr<Agent> ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = makeP4(spec);
  Agent* raw[NPLAY];
  for (int p = 0; p < NPLAY; p++) raw[p] = ag[p].get();
  Game game;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    GameResult r = game.run(s, rules, raw);
    if (ms) { ms->games++; ms->events += r.events; if (r.hitLimit) ms->limitHits++;
      for (int t = 0; t < 2; t++) { ms->decl[t] += r.decls[t]; ms->declCorrect[t] += r.correctDecls[t];
        ms->fdecl[t] += r.forcedDecls[t]; ms->fdeclCorrect[t] += r.forcedCorrect[t];
        ms->asks[t] += r.teamAsks[t]; ms->hits[t] += r.teamHits[t]; } }
  }
  P4Stats tot;
  for (int p = 0; p < NPLAY; p++) tot.merge(static_cast<P4Agent*>(raw[p])->st);
  return tot;
}

// A/B match runner that understands "p4:..." specs.
inline MatchStats runMatchP4(const MatchConfig& mc) {
  int nThreads = mc.threads > 0 ? mc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, mc.games));
  std::vector<MatchStats> local(nThreads);
  std::vector<std::vector<uint8_t>> pairedLocal(nThreads);
  auto t0 = std::chrono::steady_clock::now();
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeP4(mc.specA); B[i] = makeP4(mc.specB); }
      MatchStats& st = local[t];
      auto& pr = pairedLocal[t];
      Game game;
      for (int i = t; i < mc.games; i += nThreads) {
        uint64_t s = mixSeed(mc.seed, uint64_t(i) * 2654435761ull + 1);
        int aWins = 0;
        for (int rot = 0; rot < mc.rotations; rot++) {
          int orient = (mc.rotations == 2) ? rot : (rot / 3);
          int shift  = (mc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          GameResult r = game.run(s, mc.rules, ag);
          int teamA = orient, teamB = 1 - orient;
          if (r.winner == teamA) { st.winsA++; aWins++; }
          st.asks[0] += r.teamAsks[teamA]; st.hits[0] += r.teamHits[teamA];
          st.asks[1] += r.teamAsks[teamB]; st.hits[1] += r.teamHits[teamB];
          st.decl[0] += r.decls[teamA]; st.declCorrect[0] += r.correctDecls[teamA];
          st.decl[1] += r.decls[teamB]; st.declCorrect[1] += r.correctDecls[teamB];
          st.fdecl[0] += r.forcedDecls[teamA]; st.fdeclCorrect[0] += r.forcedCorrect[teamA];
          st.fdecl[1] += r.forcedDecls[teamB]; st.fdeclCorrect[1] += r.forcedCorrect[teamB];
          st.sets[0] += r.score[teamA]; st.sets[1] += r.score[teamB];
          st.events += r.events;
          if (r.hitLimit) st.limitHits++;
        }
        st.games++;
        pr.push_back(uint8_t(aWins));
      }
    });
  }
  for (auto& th : pool) th.join();
  MatchStats total;
  for (int t = 0; t < nThreads; t++) { total.merge(local[t]);
    total.paired.insert(total.paired.end(), pairedLocal[t].begin(), pairedLocal[t].end()); }
  total.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return total;
}

// Fast-posterior (cheap, jointSequential) versus the exact BlockDP, sampled at
// every public event for seat 0, on every live half-suit that the Fast path
// would even consider (no opponent provably holds a card of it).
struct BlockCmp {
  long long n = 0, allocOverTeamFast = 0, allocOverTeamExact = 0, maxUsed = 0;
  double sumCheapErr = 0, sumAllocErr = 0, maxCheapErr = 0, maxAllocErr = 0;
  double sumMaxErr = 0, maxMaxErr = 0;
  double sumCheapSigned = 0, sumAllocSigned = 0;
  long long cheapOverExact = 0;
  long long gateFlipUp = 0, gateFlipDown = 0;
  long long declFlip = 0;
  long long lockClaim = 0, lockClaimWrong = 0;
};

inline BlockCmp runBlockCmp(int games, uint64_t seed, const Rules& rules, double teamFloor,
                            const std::string& spec = "p4") {
  BlockCmp bc;
  std::unique_ptr<Agent> ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = makeP4(spec);
  Agent* raw[NPLAY];
  for (int p = 0; p < NPLAY; p++) raw[p] = ag[p].get();
  Game game;
  BlockDP bd;
  game.observer = [&](const Game& g) {
    P4Agent* a = static_cast<P4Agent*>(raw[0]);
    if (!g.g.pub.handCount[0] && !g.g.pub.rules.cardlessMayDeclare) return;
    a->refresh();                                  // Fast posterior
    if (!bd.build(a->k)) return;
    for (int s = 0; s < NSET; s++) {
      if (!g.g.pub.setActive[s]) continue;
      bool possible = true;
      for (int i = 0; i < SETSZ && possible; i++) {
        int c = cardOf(s, i);
        if (a->k.owner[c] < NPLAY && !(a->teamMask & (1u << a->k.owner[c]))) possible = false;
        if (!(a->k.mask[c] & a->teamMask) && a->k.owner[c] >= NPLAY) possible = false;
      }
      if (!possible) continue;
      P4Agent::SetVerdict v = a->evaluateSet(g.g.pub, s, 0, true);   // gates off
      double cheap = 1;
      for (int i = 0; i < SETSZ; i++) { int c = cardOf(s, i);
        cheap *= (a->k.myHand & bit(c)) ? 1.0 : a->pTeamCard(c); }
      double exTeam = bd.teamOwnsProbability(s, a->teamMask);
      int oc[SETSZ], os[SETSZ], n2 = 0;
      double exAlloc = bd.bestTeamAllocation(s, a->teamMask, oc, os, n2);
      double used = std::max(cheap, v.pAlloc);
      bc.n++;
      if (v.pAlloc > cheap + 1e-12) bc.allocOverTeamFast++;
      if (v.pAlloc > exTeam + 1e-9) bc.allocOverTeamExact++;
      bc.sumCheapSigned += (cheap - exTeam); bc.sumAllocSigned += (v.pAlloc - exAlloc);
      if (cheap > exTeam + 1e-9) bc.cheapOverExact++;
      bc.sumCheapErr += std::fabs(cheap - exTeam); bc.maxCheapErr = std::max(bc.maxCheapErr, std::fabs(cheap - exTeam));
      bc.sumAllocErr += std::fabs(v.pAlloc - exAlloc); bc.maxAllocErr = std::max(bc.maxAllocErr, std::fabs(v.pAlloc - exAlloc));
      bc.sumMaxErr += std::fabs(used - exTeam); bc.maxMaxErr = std::max(bc.maxMaxErr, std::fabs(used - exTeam));
      if (used > .9995) { bc.lockClaim++; if (exTeam <= .9995) bc.lockClaimWrong++; }
      bool passUsed = used >= teamFloor, passExact = exTeam >= teamFloor;
      if (passUsed && !passExact) bc.gateFlipUp++;
      if (!passUsed && passExact) bc.gateFlipDown++;
      if (std::fabs(v.pAlloc - exAlloc) > 0.10) bc.declFlip++;
    }
  };
  for (int i = 0; i < games; i++) game.run(mixSeed(seed, uint64_t(i) * 2654435761ull + 1), rules, raw);
  return bc;
}


// Per-game cross-tab of the forcing horizon against actual deadlock.
struct HorizonStats {
  long long games = 0, reached = 0, reachedDeadlocked = 0, reachedHealthy = 0;
  long long declPost = 0, declPostWrong = 0;
  long long declPostHealthy = 0, declPostHealthyWrong = 0;
  long long declPre = 0, declPreWrong = 0;
  long long forced = 0, forcedWrong = 0;
  long long setsLostPostHealthy = 0;
  long long wrongTeamOwnedAll = 0, wrongOppHeldSome = 0;
  long long ourHeldOnWrong = 0;
  long long confN[10] = {0}, confW[10] = {0};
};

inline HorizonStats runHorizon(const std::string& spec, int games, uint64_t seed,
                               const Rules& rules, int horizon, int deadRunCut) {
  HorizonStats hs;
  std::unique_ptr<Agent> ag[NPLAY];
  for (int p = 0; p < NPLAY; p++) ag[p] = makeP4(spec);
  Agent* raw[NPLAY];
  for (int p = 0; p < NPLAY; p++) raw[p] = ag[p].get();
  Game game; game.trace.on = true;
  for (int i = 0; i < games; i++) {
    game.trace.events.clear();
    uint64_t sd = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    GameResult r = game.run(sd, rules, raw);
    // rebuild each seat's knowledge to score dead asks
    Knowledge k[NPLAY];
    for (int p = 0; p < NPLAY; p++) k[p].init(p, game.g.dealt[p], rules.deckSets);
    uint64_t th[NPLAY];
    for (int p = 0; p < NPLAY; p++) th[p] = game.g.dealt[p];
    int run = 0, longest = 0, nEv = 0;
    long long dp = 0, dpw = 0, dpre = 0, dprew = 0, fd = 0, fdw = 0;
    for (const Event& e : game.trace.events) {
      nEv++;
      if (e.kind == Kind::Ask) {
        const Knowledge& kk = k[e.actor];
        bool dead = (kk.owner[e.card] < NPLAY) ? (kk.owner[e.card] != e.target)
                                               : !(kk.mask[e.card] & (1u << e.target));
        if (dead) { run++; longest = std::max(longest, run); } else run = 0;
      }
      if (e.kind == Kind::Declare) {
        if (nEv >= horizon) { dp++; if (!e.success) dpw++; }
        else { dpre++; if (!e.success) dprew++; }
        { int b = std::min(9, std::max(0, int(e.confidence * 10))); hs.confN[b]++; if (!e.success) hs.confW[b]++; }
        if (!e.success) {   // did the declaring team in fact hold all six?
          int tm = 0; for (int q = 0; q < NPLAY; q++) if (teamOf(q) == teamOf(e.actor)) tm |= 1 << q;
          bool all = true;
          for (int i = 0; i < SETSZ; i++) { int c = cardOf(e.decl.set, i); int truth = -1;
            for (int q = 0; q < NPLAY; q++) if (th[q] & bit(c)) truth = q;
            if (truth < 0 || !(tm & (1 << truth))) { all = false; break; } }
          int held = 0;
          for (int i = 0; i < SETSZ; i++) { int c = cardOf(e.decl.set, i);
            for (int q = 0; q < NPLAY; q++) if ((th[q] & bit(c)) && (tm & (1 << q))) held++; }
          hs.ourHeldOnWrong += held;
          if (all) hs.wrongTeamOwnedAll++; else hs.wrongOppHeldSome++;
        }
      }
      if (e.kind == Kind::Ask && e.success) { th[e.target] &= ~bit(e.card); th[e.actor] |= bit(e.card); }
      if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare)
        for (int q = 0; q < NPLAY; q++) th[q] &= ~setMask(e.decl.set);
      if (e.kind == Kind::ForcedDeclare) { fd++; if (!e.success) fdw++; }
      for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
    }
    hs.games++;
    hs.declPost += dp; hs.declPostWrong += dpw;
    hs.declPre += dpre; hs.declPreWrong += dprew;
    hs.forced += fd; hs.forcedWrong += fdw;
    if (r.events >= horizon) {
      hs.reached++;
      if (longest >= deadRunCut) hs.reachedDeadlocked++;
      else { hs.reachedHealthy++; hs.declPostHealthy += dp; hs.declPostHealthyWrong += dpw; }
    }
  }
  return hs;
}

} // namespace p4
} // namespace fish
