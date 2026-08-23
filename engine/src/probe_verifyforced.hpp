// Adversarial verification of the "every forced declaration comes from the
// bestGuess rung" claim.
//
// Independent of probe_forcedendgame.hpp: instead of REPLAYING the ladder from
// an observer hook, this wraps every agent and counts the calls the engine
// itself makes inside Game::forcedEndgame (game.hpp:238-252).  A rung "fires"
// exactly when willingForced() returns true for that rung's threshold.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include <atomic>
#include <thread>

namespace fish { namespace vfe {

struct Counters {
  std::atomic<long long> wfCalls[16];
  std::atomic<long long> wfTrue[16];
  std::atomic<long long> wfOk[16];        // evaluateSet ok (probed at threshold -1e9)
  std::atomic<long long> bgCalls{0};
  std::atomic<long long> forcedEvents{0}, forcedWrong{0};
  std::atomic<long long> voluntaryEvents{0}, voluntaryWrong{0};
  // pAlloc histogram at the rung sweep, bucketed by the highest rung it clears
  std::atomic<long long> pallocBucket[16];
  std::atomic<long long> pallocNotOk{0};
  std::atomic<long long> pallocZero{0};
  std::atomic<long long> unresHist[16];
  Counters() {
    for (int i = 0; i < 16; i++) { wfCalls[i] = 0; wfTrue[i] = 0; wfOk[i] = 0; pallocBucket[i] = 0; unresHist[i] = 0; }
  }
};

inline Counters& C() { static Counters c; return c; }

struct WrapAgent : Agent {
  std::unique_ptr<Agent> in;
  Rules rules;
  bool probeAlloc = true;
  explicit WrapAgent(const std::string& spec) : in(makeAgent(spec)) {}
  const char* name() const override { return in->name(); }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed); in->reset(s, hand, r, seed); rules = r;
  }
  void observe(const Event& e) override { Agent::observe(e); in->observe(e); }
  AskMove chooseAsk(const PublicState& p) override { return in->chooseAsk(p); }
  double lastAskForecast() const override { return in->lastAskForecast(); }
  int valueFeatures(const PublicState& p, double* f) override { return in->valueFeatures(p, f); }
  bool proposeDeclaration(const PublicState& p, Declaration& d, double& c) override {
    return in->proposeDeclaration(p, d, c);
  }
  int choosePassTarget(const PublicState& p, const int* cand, int n) override {
    return in->choosePassTarget(p, cand, n);
  }
  int rungOf(double th) const {
    for (int i = 0; i < rules.nForcedTh; i++) if (rules.forcedTh[i] == th) return i;
    return 15;
  }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf,
                     double threshold) override {
    int r = rungOf(threshold);
    C().wfCalls[r].fetch_add(1, std::memory_order_relaxed);
    bool got = in->willingForced(pub, set, d, conf, threshold);
    if (got) C().wfTrue[r].fetch_add(1, std::memory_order_relaxed);
    if (r == 0 && probeAlloc) {
      // Same call with an unreachable threshold: reports whether evaluateSet
      // succeeded at all and what pAlloc it produced.  evaluateSet is a pure
      // function of the agent's own Knowledge, so this cannot perturb play.
      Declaration d2{}; double p2 = -1;
      bool ok = in->willingForced(pub, set, d2, p2, -1e18);
      if (!ok) C().pallocNotOk.fetch_add(1, std::memory_order_relaxed);
      else {
        C().wfOk[0].fetch_add(1, std::memory_order_relaxed);
        if (p2 <= 0) C().pallocZero.fetch_add(1, std::memory_order_relaxed);
        int b = rules.nForcedTh;                      // "clears no willingness rung"
        for (int i = 0; i < rules.nForcedTh; i++)
          if (rules.forcedTh[i] >= 0 && p2 >= rules.forcedTh[i]) { b = i; break; }
        C().pallocBucket[b].fetch_add(1, std::memory_order_relaxed);
      }
      int un = __builtin_popcountll(in->k.unresolved & setMask(set));
      C().unresHist[un < 15 ? un : 15].fetch_add(1, std::memory_order_relaxed);
    }
    return got;
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    C().bgCalls.fetch_add(1, std::memory_order_relaxed);
    in->bestGuess(pub, set, d, conf);
  }
};

struct VFCfg {
  std::string specA = "v04", specB = "v04";
  int games = 300, rotations = 2;
  uint64_t seed = 31;
  Rules rules;
};

struct VFOut { long long games = 0, gamesWithForced = 0; };

inline VFOut runVerifyForced(const VFCfg& cfg) {
  VFOut o;
  int nT = std::max(1, int(std::thread::hardware_concurrency()));
  nT = std::min(nT, std::max(1, cfg.games));
  std::vector<VFOut> loc(nT);
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<WrapAgent> A[3], B[3];
      for (int i = 0; i < 3; i++) {
        A[i] = std::make_unique<WrapAgent>(cfg.specA);
        B[i] = std::make_unique<WrapAgent>(cfg.specB);
      }
      Game game;
      game.trace.on = true;
      for (int i = t; i < cfg.games; i += nT) {
        uint64_t s = mixSeed(cfg.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < cfg.rotations; rot++) {
          int orient = (cfg.rotations == 2) ? rot : (rot / 3);
          int shift  = (cfg.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? (Agent*)A[p / 2].get() : (Agent*)B[p / 2].get();
          game.rotation = shift;
          game.trace.events.clear();
          game.run(s, cfg.rules, ag);
          loc[t].games++;
          long long f = 0;
          for (const auto& e : game.trace.events) {
            if (e.kind == Kind::ForcedDeclare) {
              f++;
              C().forcedEvents.fetch_add(1, std::memory_order_relaxed);
              if (!e.success) C().forcedWrong.fetch_add(1, std::memory_order_relaxed);
            } else if (e.kind == Kind::Declare) {
              C().voluntaryEvents.fetch_add(1, std::memory_order_relaxed);
              if (!e.success) C().voluntaryWrong.fetch_add(1, std::memory_order_relaxed);
            }
          }
          if (f) loc[t].gamesWithForced++;
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  for (int t = 0; t < nT; t++) { o.games += loc[t].games; o.gamesWithForced += loc[t].gamesWithForced; }
  return o;
}

inline void printVerifyForced(const VFCfg& cfg, const VFOut& o, std::ostream& os) {
  auto& c = C();
  os << "games                    " << o.games << "\n";
  os << "games w/ ForcedDeclare   " << o.gamesWithForced << "\n";
  os << "ForcedDeclare events     " << c.forcedEvents.load()
     << "   wrong " << c.forcedWrong.load() << "\n";
  os << "Declare (voluntary)      " << c.voluntaryEvents.load()
     << "   wrong " << c.voluntaryWrong.load() << "\n";
  os << "bestGuess() calls        " << c.bgCalls.load() << "\n";
  os << "\nengine-side willingForced() calls, by ladder rung\n";
  for (int i = 0; i < cfg.rules.nForcedTh; i++) {
    if (cfg.rules.forcedTh[i] < 0) continue;
    os << "  rung " << i << "  th=" << cfg.rules.forcedTh[i]
       << "   calls=" << c.wfCalls[i].load()
       << "   returned TRUE=" << c.wfTrue[i].load() << "\n";
  }
  os << "  (rung " << (cfg.rules.nForcedTh - 1) << " th=-1 is bestGuess, never willingForced)\n";
  os << "\nat the FIRST rung of each sweep: what evaluateSet(press=2) reported\n";
  os << "  evaluateSet !ok        " << c.pallocNotOk.load() << "\n";
  os << "  evaluateSet ok         " << c.wfOk[0].load() << "\n";
  os << "  of those, pAlloc == 0  " << c.pallocZero.load() << "\n";
  os << "  highest rung its pAlloc would clear:\n";
  for (int b = 0; b <= cfg.rules.nForcedTh; b++) {
    long long v = c.pallocBucket[b].load();
    if (!v) continue;
    if (b < cfg.rules.nForcedTh) os << "    rung " << b << " (>= " << cfg.rules.forcedTh[b] << ")   " << v << "\n";
    else os << "    none (pAlloc < 0.50)   " << v << "\n";
  }
  os << "\nunresolved cards of the queried half-suit (declarer's own knowledge)\n";
  for (int u = 0; u < 16; u++) if (c.unresHist[u].load())
    os << "  " << u << "  " << c.unresHist[u].load() << "\n";
}

}} // namespace fish::vfe
