// ADVERSARIAL VERIFICATION of the P2 sub-claim:
//   "The willingness rungs never fire because willingForced's own pAlloc is
//    EXACTLY 0.0 at every one of those states -- not merely low."
//
// Method, deliberately different from probe_forcedendgame.hpp:
// instead of REPLAYING the forced endgame from a reconstructed state, this file
// wraps every agent in a pass-through proxy and intercepts the calls that
// Game::forcedEndgame (game.hpp:246) actually makes, in situ.  At each
// intercepted call it
//   (a) re-asks the inner agent with threshold -1 so the rung's own pAlloc is
//       readable whatever it is,
//   (b) recomputes that same pAlloc with a LOCAL, instrumented copy of
//       Belief::jointSequential (belief.hpp:535-553) that reports WHICH return
//       statement produced the value, and whether the mask bit that killed it
//       was present in the agent's Knowledge before capacity propagation,
//   (c) computes the capacity-feasible alternative (jointSequentialMAP) and
//       checks it against every rung of Rules::forcedTh.
// Nothing in v04.hpp / belief.hpp / game.hpp / fish.hpp / blockdp.hpp is edited.
#pragma once
#include "factory.hpp"
#include "game.hpp"
#include <thread>
#include <cmath>
#include <vector>
#include <algorithm>

namespace fish {
namespace vwill {

// ---------------------------------------------------------------- instrumented
// Byte-for-byte copy of Belief::jointSequential (belief.hpp:535-553) with the
// return sites labelled.  exitCode: 0 = ran to completion, 1 = owner mismatch
// (line 542), 2 = mask violation (line 543), 3 = zero conditional marginal
// (line 546), 4 = underflow early-out (line 548, returns a NON-zero pr).
inline double jsInstr(const Knowledge& kk, const int* cards, const int* players, int n,
                      int outer, int inner, double theta, double phi,
                      int& exitCode, int& exitStep, bool& maskWasThereBefore) {
  Knowledge tmp = kk;
  double pr = 1.0;
  Belief scratchB;
  exitCode = 0; exitStep = -1; maskWasThereBefore = false;
  for (int i = 0; i < n; i++) {
    int c = cards[i], p = players[i];
    if (tmp.owner[c] < NPLAY) {
      if (tmp.owner[c] != p) {
        exitCode = 1; exitStep = i;
        // Was c already resolved in the agent's ORIGINAL knowledge?  If NOT,
        // the only thing that can have resolved it is setOwner/propagateCapacity
        // inside this loop -- i.e. the capacity-free argmax overfilled a
        // teammate and the leftovers collapsed onto the other one.
        maskWasThereBefore = (kk.owner[c] >= NPLAY);
        return 0.0;
      }
      continue;
    }
    if (!(tmp.mask[c] & (1u << p))) {
      exitCode = 2; exitStep = i;
      // Was p a legal owner of c in the agent's ORIGINAL knowledge?  If yes,
      // the bit can only have been cleared by setOwner/propagateCapacity
      // inside this loop, i.e. the candidate overfilled that teammate.
      maskWasThereBefore = (kk.owner[c] >= NPLAY) && ((kk.mask[c] & (1u << p)) != 0);
      return 0.0;
    }
    scratchB.sinkhornDisj(tmp, outer, inner, theta, phi);
    double v = scratchB.marg[c][p];
    if (v <= 0) { exitCode = 3; exitStep = i; return 0.0; }
    pr *= v;
    if (pr < 1e-9) { exitCode = 4; exitStep = i; return pr; }
    tmp.setOwner(c, p);
    tmp.propagateCapacity();
  }
  return pr;
}

struct Rec {
  double th = 0;
  bool ok = false;          // evaluateSet reported v.ok
  double pAlloc = 0;        // what willingForced compared against the rung
  bool exactlyZero = false; // bitwise == 0.0
  double pMap = 0;          // capacity-feasible greedy MAP alternative
  int exitCode = -1, exitStep = -1;
  bool maskWasThereBefore = false;
  int nUnresolved = 0;
  int distinctSeats = 0;    // how many teammates the argmax candidate uses
  int firstRung = 0;
};

struct WStats {
  long long games = 0;
  long long calls = 0;            // every willingForced call the engine made
  long long firstRungCalls = 0;   // calls at forcedTh[0]
  long long okCalls = 0;
  long long frOk = 0, frZero = 0, frPos = 0;   // first-rung calls == distinct (set,seat) states
  double frSumMap = 0; long long frMapGe50 = 0;
  long long zeroExact = 0;        // pAlloc bitwise 0.0
  long long posAlloc = 0;
  double sumAlloc = 0, maxAlloc = 0;
  double minPos = 1e9;
  long long exitHist[6] = {0,0,0,0,0,0};
  long long maskFromCapacity = 0; // zero exit caused by in-loop capacity propagation
  long long capOverfill = 0;      // the argmax hands a teammate more cards than they can hold
  long long mismatchOwnRecompute = 0;
  long long oneSeatCandidate = 0;
  // feasible MAP alternative
  double sumMap = 0;
  long long mapPos = 0;
  long long mapClears[8] = {0,0,0,0,0,0,0,0};  // >= forcedTh[i] for i<7
  long long rungFired = 0;        // the real engine accepted a rung (th >= 0)
  long long bestGuessUsed = 0;
  std::vector<double> allocVals, mapVals;
  void merge(const WStats& o) {
    games += o.games; calls += o.calls; firstRungCalls += o.firstRungCalls;
    okCalls += o.okCalls; zeroExact += o.zeroExact; posAlloc += o.posAlloc;
    frOk += o.frOk; frZero += o.frZero; frPos += o.frPos; frSumMap += o.frSumMap; frMapGe50 += o.frMapGe50;
    sumAlloc += o.sumAlloc; maxAlloc = std::max(maxAlloc, o.maxAlloc);
    minPos = std::min(minPos, o.minPos);
    for (int i = 0; i < 6; i++) exitHist[i] += o.exitHist[i];
    maskFromCapacity += o.maskFromCapacity; capOverfill += o.capOverfill;
    mismatchOwnRecompute += o.mismatchOwnRecompute;
    oneSeatCandidate += o.oneSeatCandidate;
    sumMap += o.sumMap; mapPos += o.mapPos;
    for (int i = 0; i < 8; i++) mapClears[i] += o.mapClears[i];
    rungFired += o.rungFired; bestGuessUsed += o.bestGuessUsed;
    allocVals.insert(allocVals.end(), o.allocVals.begin(), o.allocVals.end());
    mapVals.insert(mapVals.end(), o.mapVals.begin(), o.mapVals.end());
  }
};

// ------------------------------------------------------------------- the proxy
struct Proxy : Agent {
  std::unique_ptr<Agent> in;
  WStats* st = nullptr;
  Rules rules;

  const char* name() const override { return in->name(); }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    rules = r;
    in->reset(s, hand, r, seed);
  }
  void observe(const Event& e) override { Agent::observe(e); in->observe(e); }
  AskMove chooseAsk(const PublicState& pub) override { return in->chooseAsk(pub); }
  double lastAskForecast() const override { return in->lastAskForecast(); }
  int valueFeatures(const PublicState& pub, double* f) override { return in->valueFeatures(pub, f); }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    return in->proposeDeclaration(pub, d, conf);
  }
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    return in->choosePassTarget(pub, cand, n);
  }

  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf,
                     double threshold) override {
    probe(pub, set, threshold);
    bool r = in->willingForced(pub, set, d, conf, threshold);
    if (r && threshold >= 0) st->rungFired++;
    return r;
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    st->bestGuessUsed++;
    in->bestGuess(pub, set, d, conf);
  }

  void probe(const PublicState& pub, int set, double th) {
    V04Agent* va = dynamic_cast<V04Agent*>(in.get());
    if (!va || !st) return;
    st->calls++;
    bool first = (th > 0.99);
    if (first) st->firstRungCalls++;

    // (a) read the rung's own pAlloc through the real code path
    Declaration dd{}; double cc = -1;
    bool ok = va->willingForced(pub, set, dd, cc, -1.0);   // -1 => accept anything ok
    if (!ok) return;
    st->okCalls++;
    double pAlloc = cc;
    if (pAlloc == 0.0) st->zeroExact++;
    else { st->posAlloc++; st->minPos = std::min(st->minPos, pAlloc); }
    if (first) { st->frOk++; if (pAlloc == 0.0) st->frZero++; else st->frPos++; }
    st->sumAlloc += pAlloc;
    st->maxAlloc = std::max(st->maxAlloc, pAlloc);
    st->allocVals.push_back(pAlloc);

    // (b) rebuild the candidate exactly as evaluateSet does (v04.hpp:594-609)
    //     and rescore it with the instrumented copy.
    va->refresh();
    int cards[SETSZ], players[SETSZ];
    bool bail = false;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(set, i);
      if (va->k.owner[c] < NPLAY && !(va->teamMask & (1u << va->k.owner[c]))) { bail = true; break; }
      if (!(va->k.mask[c] & va->teamMask) && va->k.owner[c] >= NPLAY) { bail = true; break; }
      cards[i] = c;
      int bestQ = va->seat; double bestP = -1;
      for (int q = 0; q < NPLAY; q++) if (va->teamMask & (1 << q)) {
        double pr = (va->k.myHand & bit(c)) ? (q == va->seat ? 1.0 : 0.0) : va->bel.marg[c][q];
        if (pr > bestP) { bestP = pr; bestQ = q; }
      }
      players[i] = bestQ;
    }
    if (bail) return;

    int seatsUsed = 0;
    for (int q = 0; q < NPLAY; q++) {
      bool used = false;
      for (int i = 0; i < SETSZ; i++) if (players[i] == q) used = true;
      if (used) seatsUsed++;
    }
    if (seatsUsed == 1) st->oneSeatCandidate++;

    int ec = -1, es = -1; bool mb = false;
    double mine = jsInstr(va->k, cards, players, SETSZ, va->cfg.sinkOuter, va->cfg.sinkInner,
                          va->cfg.priorTheta, va->cfg.priorPhi, ec, es, mb);
    if (ec >= 0 && ec < 6) st->exitHist[ec]++;
    if ((ec == 2 || ec == 1) && mb) st->maskFromCapacity++;
    if (ec == 1 || ec == 2) {
      // Did the candidate ask a teammate to hold more cards than they can?
      // Count, over the six cards, how many the argmax hands to each seat and
      // compare with that seat's spare capacity in the agent's own Knowledge.
      uint8_t cap[NPLAY]; va->k.capacities(cap);
      int want[NPLAY] = {0,0,0,0,0,0};
      for (int i = 0; i < SETSZ; i++)
        if (va->k.owner[cards[i]] >= NPLAY) want[players[i]]++;
      bool over = false;
      for (int q = 0; q < NPLAY; q++) if (want[q] > int(cap[q])) over = true;
      if (over) st->capOverfill++;
    }
    if (std::fabs(mine - pAlloc) > 1e-12) st->mismatchOwnRecompute++;

    // (c) the capacity-feasible alternative the rung COULD have been given
    int chosen[SETSZ];
    double pmap = va->bel.jointSequentialMAP(va->k, cards, SETSZ, va->teamMask, chosen,
                                             va->cfg.sinkOuter, va->cfg.sinkInner,
                                             va->cfg.priorTheta, va->cfg.priorPhi);
    st->sumMap += pmap;
    if (first) { st->frSumMap += pmap; if (pmap >= 0.5) st->frMapGe50++; }
    st->mapVals.push_back(pmap);
    if (pmap > 0) st->mapPos++;
    for (int i = 0; i < rules.nForcedTh && i < 8; i++)
      if (rules.forcedTh[i] >= 0 && pmap >= rules.forcedTh[i]) st->mapClears[i]++;

    int nun = 0;
    for (int i = 0; i < SETSZ; i++) if (va->k.owner[cardOf(set, i)] >= NPLAY) nun++;
    (void)nun;
  }
};

struct WCfg {
  std::string specA = "v04", specB = "v04";
  int games = 300, rotations = 2;
  uint64_t seed = 31;
  Rules rules;
  int threads = 1;
};

inline WStats runVWill(const WCfg& c) {
  int nT = std::max(1, c.threads);
  std::vector<WStats> local(nT);
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Proxy> P[NPLAY];
      Game game;
      for (int i = t; i < c.games; i += nT) {
        uint64_t s = mixSeed(c.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < c.rotations; rot++) {
          int orient = (c.rotations == 2) ? rot : (rot / 3);
          int shift  = (c.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) {
            P[p] = std::make_unique<Proxy>();
            P[p]->in = makeAgent(teamOf(p) == orient ? c.specA : c.specB);
            P[p]->st = &local[t];
            ag[p] = P[p].get();
          }
          game.rotation = shift;
          game.run(s, c.rules, ag);
          local[t].games++;
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  WStats tot;
  for (int t = 0; t < nT; t++) tot.merge(local[t]);
  return tot;
}

inline void printVWill(const WCfg& c, WStats& s, std::ostream& os) {
  auto pc = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  os << "games                              " << s.games << "\n";
  os << "willingForced calls intercepted    " << s.calls
     << "   (first rung " << s.firstRungCalls << ")\n";
  os << "  evaluateSet ok                   " << s.okCalls
     << " (" << pc(s.okCalls, s.calls) << "%)\n";
  os << "  pAlloc bitwise == 0.0            " << s.zeroExact
     << " (" << pc(s.zeroExact, s.okCalls) << "% of ok)\n";
  os << "  pAlloc > 0                       " << s.posAlloc
     << " (" << pc(s.posAlloc, s.okCalls) << "%)";
  if (s.posAlloc) os << "   min " << s.minPos << "  max " << s.maxAlloc;
  os << "\n";
  os << "  mean pAlloc                      "
     << (s.okCalls ? s.sumAlloc / double(s.okCalls) : 0.0) << "\n";
  os << "  my independent recompute differs " << s.mismatchOwnRecompute << "\n";
  static const char* names[6] = {"ran to completion (non-zero)",
                                 "owner mismatch (belief.hpp:542)",
                                 "mask violation (belief.hpp:543)",
                                 "zero conditional marginal (belief.hpp:546)",
                                 "underflow early-out (belief.hpp:548, NON-zero)",
                                 "-"};
  os << "where the value came from\n";
  for (int i = 0; i < 6; i++) if (s.exitHist[i])
    os << "  [" << i << "] " << names[i] << "  " << s.exitHist[i] << "\n";
  os << "  of those zero-exits, the card was UNRESOLVED before the loop, i.e. the\n"
        "  zero was created by in-loop setOwner/propagateCapacity:        "
     << s.maskFromCapacity << " (" << pc(s.maskFromCapacity, s.okCalls) << "%)\n";
  os << "  argmax candidate overfills a teammate's own capacity:          "
     << s.capOverfill << " (" << pc(s.capOverfill, s.okCalls) << "%)\n";
  os << "  argmax candidate names all cards at ONE seat  " << s.oneSeatCandidate
     << " (" << pc(s.oneSeatCandidate, s.okCalls) << "%)\n";
  os << "counterfactual: feasible greedy MAP at the same states\n";
  os << "  mean pMAP                        "
     << (s.okCalls ? s.sumMap / double(s.okCalls) : 0.0) << "\n";
  os << "  pMAP > 0                         " << s.mapPos
     << " (" << pc(s.mapPos, s.okCalls) << "%)\n";
  for (int i = 0; i < c.rules.nForcedTh && i < 8; i++) if (c.rules.forcedTh[i] >= 0)
    os << "  pMAP >= rung " << i << " (" << c.rules.forcedTh[i] << ")   "
       << s.mapClears[i] << " (" << pc(s.mapClears[i], s.okCalls) << "%)\n";
  if (!s.mapVals.empty()) {
    std::sort(s.mapVals.begin(), s.mapVals.end());
    size_t n = s.mapVals.size();
    os << "  pMAP quantiles  p10 " << s.mapVals[n / 10]
       << "  p50 " << s.mapVals[n / 2]
       << "  p90 " << s.mapVals[(9 * n) / 10]
       << "  max " << s.mapVals[n - 1] << "\n";
  }
  os << "PER-STATE (first rung only = one row per distinct (half-suit, seat) poll)\n";
  os << "  states                           " << s.frOk << "\n";
  os << "  pAlloc exactly 0.0               " << s.frZero
     << " (" << pc(s.frZero, s.frOk) << "%)\n";
  os << "  pAlloc > 0                       " << s.frPos
     << " (" << pc(s.frPos, s.frOk) << "%)\n";
  os << "  mean feasible pMAP               " << (s.frOk ? s.frSumMap / double(s.frOk) : 0.0)
     << "   >= 0.50: " << s.frMapGe50 << " (" << pc(s.frMapGe50, s.frOk) << "%)\n";
  os << "engine outcome\n";
  os << "  a willingness rung was ACCEPTED  " << s.rungFired << "\n";
  os << "  bestGuess (rung 7) invoked       " << s.bestGuessUsed << "\n";
}

} // namespace vwill
} // namespace fish
