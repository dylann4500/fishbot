// FishBot v0.7 -- phase 3.  The mechanical side-channel certification gate.
//
// THREAT-MODEL.md 6.4 specifies six tests and says of all six: "None of the six
// reads only existing artifacts: each needs a small amount of harness plumbing."
// No phase built any of them, and phase 3's brief requires the homogeneity
// constraint (T7 H1/H2) to be "checked mechanically ... not by inspection".
// This file is that check.  It implements S6, S4, S5 and S3 -- the four pass/fail
// gates -- and it is calibrated by three planted cheats (v07_cheat.hpp), one per
// detection mechanism, because a gate whose positive control does not fail is a
// formatted PASS rather than a test.
//
// WHERE THE IMPLEMENTATION DEPARTS FROM THE SPEC, AND WHY
//
//  * S6 is specified as separate PROCESSES exchanging a serialised event stream.
//    It is implemented here as RECONSTRUCTION: a fresh agent built from the same
//    spec string, handed reset(seat, ownHand, rules, seed) and then a replay of
//    the public event stream INTERLEAVED WITH ITS OWN PAST DECISIONS, must choose
//    the same action as the live agent at every decision.  The interleaving is
//    what makes the test sound for a stochastic policy: a seat's own past
//    decisions are a function of its own past information, so replaying them is
//    legal, and it advances the policy's private RNG exactly as the live run did.
//    The reconstruction runs on a FRESHLY SPAWNED THREAD, which gives it clean
//    thread_locals, and it runs AFTER the game, which time-shifts it relative to
//    the live decision -- that is what lets it catch a process-global static too.
//
//  * S4's T10 (a per-seat stream drawn independently of the deal) is implemented
//    in the harness's own agent WRAPPER rather than in Game::setup.  The wrapper
//    substitutes the reset seed before the inner policy ever sees it, which is
//    exactly equivalent and leaves the shipped driver byte-for-byte untouched.
//
//  * S5 is implemented as specified -- exact posterior resampling of the other
//    five hands via DealDP + Belief::satisfies -- but SCORED as a clairvoyance
//    statistic rather than as an action-identity check, because within the
//    `Agent` interface the action-identity form is provably vacuous.  The proof
//    is at runS5Node.
//
// WHAT THE GATE CANNOT SEE is documented per test below and in
// research/v07/notes/K0-gate.md.  6.5's four failure modes all still apply.
#pragma once
#include "arena.hpp"
#include "v07_cheat.hpp"
#include "v07_seeds.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fish {
namespace v07side {

// --------------------------------------------------------------- utilities
inline uint64_t hashMix(uint64_t h, uint64_t v) { h ^= v; h *= 0x100000001b3ull; return h ^ (h >> 29); }
inline uint64_t eventHash(uint64_t h, const Event& e) {
  h = hashMix(h, uint64_t(e.kind));
  h = hashMix(h, uint64_t(e.actor) | (uint64_t(e.target) << 8) | (uint64_t(e.card) << 16)
                 | (uint64_t(e.set) << 24) | (uint64_t(e.success ? 1 : 0) << 32));
  for (int i = 0; i < SETSZ; i++) h = hashMix(h, uint64_t(e.decl.owner[i]));
  for (int p = 0; p < NPLAY; p++) h = hashMix(h, uint64_t(e.handCount[p]));
  return h;
}

// Ground truth as it evolves over a transcript.  Driver-side only; no policy
// ever touches it.
struct TruthTrack {
  uint64_t h[NPLAY] = {0,0,0,0,0,0};
  void init(const uint64_t* dealt) { for (int p = 0; p < NPLAY; p++) h[p] = dealt[p]; }
  void apply(const Event& e) {
    if (e.kind == Kind::Ask && e.success) { h[e.target] &= ~bit(e.card); h[e.actor] |= bit(e.card); }
    else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare)
      for (int p = 0; p < NPLAY; p++) h[p] &= ~setMask(e.set);
  }
};

// The PublicState an observer holds immediately after events[0..n).  Everything
// here is a function of the event stream alone; `turn` is not (the dealer is not
// an event), so callers that need it set it themselves.
inline PublicState rebuildPub(const Rules& r, const std::vector<Event>& ev, int n) {
  PublicState pub;
  pub.rules = r;
  for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < r.deckSets);
  for (int p = 0; p < NPLAY; p++) pub.handCount[p] = uint8_t(r.deckSets * SETSZ / NPLAY);
  pub.score[0] = pub.score[1] = 0;
  pub.nAsks = 0; pub.nEvents = n; pub.turn = 0;
  pub.history.assign(ev.begin(), ev.begin() + n);
  for (int i = 0; i < n; i++) {
    const Event& e = ev[size_t(i)];
    for (int p = 0; p < NPLAY; p++) pub.handCount[p] = e.handCount[p];
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      pub.setActive[e.set] = false;
      pub.score[e.success ? teamOf(e.actor) : 1 - teamOf(e.actor)]++;
    }
  }
  return pub;
}

// ------------------------------------------------------------ decision log
// One row per call the driver made into a certified seat, across ALL FOUR
// decision types of THREAT-MODEL 6.2.  No prior work in this corpus has looked
// past asks; D2/D3/D4 are recorded here on the same footing.
struct DecRec {
  uint8_t kind = 0;          // 0 chooseAsk, 1 proposeDeclaration, 2 choosePassTarget,
                             // 3 willingForced, 4 bestGuess -- the terminal (-1.0) rung of the
                             // forced ladder and the holds-only-complete-sets path, which is a
                             // fifth entry point the Agent interface exposes and 6.2's table
                             // folds into D2/D4 without naming.
  int32_t nObs = 0;          // events observed before the call
  PublicState pub;           // exactly the argument the live agent received
  int8_t cand[3] = {0,0,0}, nCand = 0;   // D3 argument
  int8_t set = 0; double threshold = 0;  // D4 argument
  uint8_t card = 0, target = 0;          // outputs
  bool ret = false;
  Declaration decl{};
  double conf = 0;
  bool haveK = false;        // S5 only, asks only
  Knowledge kk;

  bool sameAction(const DecRec& o) const {
    if (kind != o.kind) return false;
    if (kind == 0) return card == o.card && target == o.target;
    if (kind == 2) return target == o.target;
    if (kind == 4) {
      if (decl.set != o.decl.set) return false;
      for (int i = 0; i < SETSZ; i++) if (decl.owner[i] != o.decl.owner[i]) return false;
      return true;
    }
    if (ret != o.ret) return false;
    if (!ret) return true;
    if (decl.set != o.decl.set) return false;
    for (int i = 0; i < SETSZ; i++) if (decl.owner[i] != o.decl.owner[i]) return false;
    return true;
  }
};

// ------------------------------------------------------------- the wrapper
// Seats an inner policy, records every decision, and optionally substitutes the
// reset seed (T10).  It mirrors `k` so that anything driver-side which reads
// agents[p]->k (the soundness audit, the D1 decision sink) still sees the real
// belief state.  The inner agent is NOT owned: the driver constructs it once and
// reuses it across deals, exactly as arena.hpp does, because 6.3's fourth item
// (anything `reset` fails to clear is cross-game memory) is one of the things S6
// is here to test and rebuilding per deal would hide it.
struct SideAgent : Agent {
  Agent* inner = nullptr;
  bool record = false, captureK = false, substituteSeed = false;
  uint64_t subSeed = 0, ownHand = 0, usedSeed = 0;
  Rules rules;
  std::vector<Event> events;
  std::vector<DecRec> recs;

  const char* name() const override { return inner ? inner->name() : "side"; }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    seat = s; ownHand = hand; rules = r;
    usedSeed = substituteSeed ? subSeed : seed;
    events.clear(); recs.clear();
    inner->reset(s, hand, r, usedSeed);
    k = inner->k;
  }
  void observe(const Event& e) override { events.push_back(e); inner->observe(e); k = inner->k; }
  double lastAskForecast() const override { return inner->lastAskForecast(); }
  int valueFeatures(const PublicState& pub, double* f) override { return inner->valueFeatures(pub, f); }
  const DecisionInfo* lastDecision() const override { return inner->lastDecision(); }

  AskMove chooseAsk(const PublicState& pub) override {
    AskMove m = inner->chooseAsk(pub);
    if (record) {
      DecRec r; r.kind = 0; r.nObs = int32_t(events.size()); r.pub = pub;
      r.card = m.card; r.target = m.target;
      if (captureK) { r.haveK = true; r.kk = inner->k; }
      recs.push_back(std::move(r));
    }
    return m;
  }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    Declaration dd{}; double cc = 0;
    bool ok = inner->proposeDeclaration(pub, dd, cc);
    if (record) {
      DecRec r; r.kind = 1; r.nObs = int32_t(events.size()); r.pub = pub;
      r.ret = ok; r.decl = dd; r.conf = cc; recs.push_back(std::move(r));
    }
    if (ok) { d = dd; conf = cc; }
    return ok;
  }
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    int t = inner->choosePassTarget(pub, cand, n);
    if (record) {
      DecRec r; r.kind = 2; r.nObs = int32_t(events.size()); r.pub = pub;
      r.nCand = int8_t(n); for (int i = 0; i < n && i < 3; i++) r.cand[i] = int8_t(cand[i]);
      r.target = uint8_t(t); recs.push_back(std::move(r));
    }
    return t;
  }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) override {
    Declaration dd{}; double cc = 0;
    bool ok = inner->willingForced(pub, set, dd, cc, threshold);
    if (record) {
      DecRec r; r.kind = 3; r.nObs = int32_t(events.size()); r.pub = pub;
      r.set = int8_t(set); r.threshold = threshold;
      r.ret = ok; r.decl = dd; r.conf = cc; recs.push_back(std::move(r));
    }
    if (ok) { d = dd; conf = cc; }
    return ok;
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    Declaration dd{}; double cc = 0;
    inner->bestGuess(pub, set, dd, cc);
    if (record) {
      DecRec r; r.kind = 4; r.nObs = int32_t(events.size()); r.pub = pub;
      r.set = int8_t(set); r.decl = dd; r.conf = cc; r.ret = true;
      recs.push_back(std::move(r));
    }
    d = dd; conf = cc;
  }
};

// ------------------------------------------------------------------- config
struct SideConfig {
  std::string specA = "v06";     // the team under certification (three copies)
  std::string specB = "v06";     // the fixed opponent
  int games = 200;               // deals
  int rotations = 2;
  int threads = 0;
  uint64_t seed = 7030001;
  Rules rules;
  bool s3 = true, s4 = true, s5 = true, s6 = true;
  int s3nodes = 3;               // substitution nodes sampled per deal
  int s5nodes = 6;               // posterior-resampling nodes sampled per deal per seat
  int s5draws = 8;               // posterior draws per node
  uint64_t key = 0x51DE0CA11Bull;  // the harness's own stream constant (T10)
  // Diagnostic only.  Runs S6's reconstruction ON THE WORKER THREAD instead of
  // on a fresh one.  A mismatch that survives the fresh thread but vanishes here
  // is thread-local state that `reset` does not clear -- structurally E-2, the
  // BlockDP shared per-thread pool.  A mismatch present in both is cross-seat or
  // cross-game state on the same thread.  Never used for certification.
  bool reconInline = false;
  // v0.7 phase 4, DIAGNOSTIC.  Rebuild the twelve agents for every deal instead
  // of reusing one set per thread.  If the S6 residual is cross-deal state that
  // `reset()` does not clear, this removes it by construction.
  bool freshAgents = false;
};

struct SideStats {
  long long s6Nodes = 0, s6Mismatch = 0;
  long long s6ByKind[5] = {0,0,0,0,0}, s6MisByKind[5] = {0,0,0,0,0};
  long long s6Games = 0, s6GamesMismatch = 0;

  long long s4Deals = 0, s4Same = 0;                 // transcript-identical deals
  long long s4AskDeal = 0, s4HitDeal = 0, s4AskInd = 0, s4HitInd = 0;
  std::vector<double> s4PairDeal, s4PairInd;         // per-deal team-A wins (0..rotations)

  long long s5Nodes = 0, s5Draws = 0, s5TrueHit = 0;
  double s5SampHit = 0;
  long long s5Unres = 0, s5DpFail = 0;
  // The matched REFERENCE arm.  The same statistic computed at the OPPOSING
  // seats, which run --b.  When --b is the incumbent this is a live measurement
  // of the test's own confound in the same deals, rather than an assertion about
  // it.  In a MIRROR the two arms are exchangeable and the difference carries no
  // information -- the same caveat the corpus applies to a mirror win rate.
  long long s5bNodes = 0, s5bTrueHit = 0, s5bUnres = 0;
  double s5bSampHit = 0;

  long long s3Nodes = 0, s3NoAlt = 0;
  long long s3MateQ = 0, s3MateChg = 0, s3OppQ = 0, s3OppChg = 0;

  double seconds = 0;
  int threadsUsed = 0;

  void merge(const SideStats& o) {
    s6Nodes += o.s6Nodes; s6Mismatch += o.s6Mismatch;
    for (int i = 0; i < 5; i++) { s6ByKind[i] += o.s6ByKind[i]; s6MisByKind[i] += o.s6MisByKind[i]; }
    s6Games += o.s6Games; s6GamesMismatch += o.s6GamesMismatch;
    s4Deals += o.s4Deals; s4Same += o.s4Same;
    s4AskDeal += o.s4AskDeal; s4HitDeal += o.s4HitDeal;
    s4AskInd += o.s4AskInd; s4HitInd += o.s4HitInd;
    s4PairDeal.insert(s4PairDeal.end(), o.s4PairDeal.begin(), o.s4PairDeal.end());
    s4PairInd.insert(s4PairInd.end(), o.s4PairInd.begin(), o.s4PairInd.end());
    s5Nodes += o.s5Nodes; s5Draws += o.s5Draws; s5TrueHit += o.s5TrueHit;
    s5SampHit += o.s5SampHit; s5Unres += o.s5Unres; s5DpFail += o.s5DpFail;
    s5bNodes += o.s5bNodes; s5bTrueHit += o.s5bTrueHit; s5bUnres += o.s5bUnres;
    s5bSampHit += o.s5bSampHit;
    s3Nodes += o.s3Nodes; s3NoAlt += o.s3NoAlt;
    s3MateQ += o.s3MateQ; s3MateChg += o.s3MateChg;
    s3OppQ += o.s3OppQ; s3OppChg += o.s3OppChg;
  }
};

// ------------------------------------------------------------------- S6
struct ReconResult { long long nodes = 0, mismatch = 0; long long byKind[5] = {0,0,0,0,0}, misByKind[5] = {0,0,0,0,0}; };

inline ReconResult reconstructSeat(const std::string& spec, int seat, uint64_t hand,
                                   const Rules& rules, uint64_t seed,
                                   const std::vector<Event>& events,
                                   const std::vector<DecRec>& recs) {
  ReconResult R;
  std::unique_ptr<Agent> a = makeAgent(spec);
  a->reset(seat, hand, rules, seed);
  size_t idx = 0;
  for (const DecRec& r : recs) {
    while (int32_t(idx) < r.nObs && idx < events.size()) a->observe(events[idx++]);
    DecRec got; got.kind = r.kind;
    switch (r.kind) {
      case 0: { AskMove m = a->chooseAsk(r.pub); got.card = m.card; got.target = m.target; break; }
      case 1: { Declaration d{}; double c = 0; got.ret = a->proposeDeclaration(r.pub, d, c); got.decl = d; break; }
      case 2: { int cd[3] = {0,0,0}; for (int i = 0; i < r.nCand && i < 3; i++) cd[i] = r.cand[i];
                got.target = uint8_t(a->choosePassTarget(r.pub, cd, r.nCand)); break; }
      case 3: { Declaration d{}; double c = 0;
                got.ret = a->willingForced(r.pub, r.set, d, c, r.threshold); got.decl = d; break; }
      default: { Declaration d{}; double c = 0;
                 a->bestGuess(r.pub, r.set, d, c); got.ret = true; got.decl = d; break; }
    }
    R.nodes++; R.byKind[r.kind < 5 ? r.kind : 4]++;
    if (!r.sameAction(got)) { R.mismatch++; R.misByKind[r.kind < 5 ? r.kind : 4]++; }
  }
  return R;
}

// Fresh thread: clean thread_locals, and no teammate agent has ever executed on
// it.  Combined with running after the game (a time shift relative to the live
// decision) this is what makes the reconstruction sensitive to shared state that
// is not thread_local as well.
inline ReconResult reconstructSeatIsolated(const std::string& spec, int seat, uint64_t hand,
                                           const Rules& rules, uint64_t seed,
                                           const std::vector<Event>& events,
                                           const std::vector<DecRec>& recs) {
  ReconResult R;
  std::thread th([&] { R = reconstructSeat(spec, seat, hand, rules, seed, events, recs); });
  th.join();
  return R;
}

// ------------------------------------------------------------------- S5
// Posterior invariance / clairvoyance.
//
// WHY NOT THE ACTION-IDENTITY FORM.  6.4 phrases S5 as "resample the other five
// hands ... and require the seat's chosen action to be unchanged".  Inside this
// engine that form is VACUOUS, and provably so: an `Agent` is handed its own hand
// once, at reset, and a stream of public events thereafter, and nothing else.
// Resampling the other five hands changes neither input, so EVERY policy passes
// by construction -- including a clairvoyant one, whose clairvoyance is anchored
// to the reset SEED and not to the driver's hand array.  That is itself worth
// recording: within the Agent interface the reset seed is the ENTIRE hidden-
// information surface, which is exactly why E-1 is the whole of the clairvoyance
// threat and why the corpus can close it by closing one channel.
//
// So S5 is scored the other way round, using the same exact posterior.  At a
// sampled ask, draw J worlds from the exact policy-agnostic posterior over deals
// consistent with (public transcript, own hand) -- DealDP for C1-C4,
// Belief::satisfies for the C5 certificates by rejection, the identical
// construction v0.6's own determinizer uses -- and compare
//     P(the chosen ask lands | truth)  against  P(it lands | posterior draw).
// A policy conditioning only on what it may see has these equal in expectation,
// because the truth IS a draw from that posterior.  A clairvoyant policy beats
// its own posterior, and by a lot.
//
// KNOWN CONFOUND, stated because it bounds the resolution.  The sampler is
// policy-AGNOSTIC; v0.6 carries a nonzero policy prior (priorTheta 0.37062,
// priorPhi 0.14525, V6PARAMS) and the true deal is generated BY policies, so a
// legitimate policy-aware seat can beat the policy-agnostic posterior by a real
// margin -- phase 2 measured that channel at ~2.0 bits/ask of transcript
// inversion.  The gate therefore uses a clairvoyance-scale threshold, not zero.
inline void runS5Node(const DecRec& r, const TruthTrack& tr, Rng& rng, int draws,
                      SideStats& S, bool refArm) {
  if (!r.haveK) return;
  int c = int(r.card), t = int(r.target);
  long long& nodes = refArm ? S.s5bNodes : S.s5Nodes;
  long long& trueHit_ = refArm ? S.s5bTrueHit : S.s5TrueHit;
  long long& unres = refArm ? S.s5bUnres : S.s5Unres;
  double& samp = refArm ? S.s5bSampHit : S.s5SampHit;
  nodes++;
  bool trueHit = (tr.h[t] & bit(c)) != 0;
  trueHit_ += trueHit ? 1 : 0;
  if (!(r.kk.unresolved & bit(c))) {
    // Resolved in the seat's own deduction: the posterior is a point mass and
    // carries no information either way.  Scored at its certain value.
    samp += trueHit ? 1.0 : 0.0; S.s5Draws += draws; return;
  }
  unres++;
  DealDP dp;
  if (!dp.build(r.kk)) { S.s5DpFail++; samp += trueHit ? 1.0 : 0.0; S.s5Draws += draws; return; }
  std::array<uint8_t, NCARD> base{};
  for (int i = 0; i < NCARD; i++) base[i] = r.kk.owner[i];
  int got = 0, tries = 0; double hits = 0;
  while (got < draws && tries < draws * 40 + 100) {
    tries++;
    std::array<uint8_t, NCARD> o = base;
    dp.sample(rng, o.data());
    if (!Belief::satisfies(r.kk, o.data())) continue;
    got++;
    if (o[size_t(c)] == uint8_t(t)) hits += 1.0;
  }
  if (!got) { S.s5DpFail++; samp += trueHit ? 1.0 : 0.0; S.s5Draws += draws; return; }
  samp += hits / double(got);
  S.s5Draws += got;
}

// ------------------------------------------------------------------- S3
// Listening substitution.
//
// RULE-EQUIVALENCE, made concrete.  At a node where a certified seat j asked
// card c of half-suit s from target t, a substitute c' is admissible iff
//   (a) c' lies in half-suit s and j does not hold c'  -- the ask stays legal
//       for j, and by 6.2's D1 row the SAME rule-forced certificates fire;
//   (b) (t holds c') == (t holds c)                    -- the public success bit
//       is unchanged and the turn moves the same way, so G is identical;
//   (c) neither c nor c' is held by the observer       -- a cheap sufficient
//       condition for both cards being unresolved from the observer's side, so
//       the observer's exact posterior does not separate them either.
// This is research/v06/results/E8-ties.txt's bit-for-bit tie group made
// operational: 53.2% of v0.6's ask decisions have two or more candidates
// numerically identical at the top, 93.1% of those are two cards of one
// half-suit at one target, and the exact posterior separates 0.00% of them.  A
// policy that responds to this substitution is responding to an ARBITRARY
// TIE-BREAK LABEL -- I-1 when it came from sort or enumeration order (v0.6
// resolves these by unstable std::sort, v06.hpp:403-405), I-2 when from the
// card-index encoding.  Either way it is on the closed list.
//
// THE CONTROL that makes the number readable.  The same substituted public event
// is shown to a TEAMMATE observer and to an OPPOSING observer, and BOTH observers
// run the policy under certification, at their own real information sets.  Both
// see the identical public action; the only difference is which side of the table
// they sit on.  A teammate that responds materially more than an opponent does is
// the encrypted convention this test exists to find.
inline bool s3AskQuery(const std::string& spec, int obs, uint64_t obsHand0, const Rules& rules,
                       uint64_t obsSeed, const std::vector<Event>& ev, int upto,
                       const Event& lastEv, uint64_t obsHandNow, AskMove& out) {
  PublicState pub = rebuildPub(rules, ev, upto);
  pub.history.push_back(lastEv);
  pub.nEvents = upto + 1;
  for (int p = 0; p < NPLAY; p++) pub.handCount[p] = lastEv.handCount[p];
  if (lastEv.kind == Kind::Declare || lastEv.kind == Kind::ForcedDeclare) {
    pub.setActive[lastEv.set] = false;
    pub.score[lastEv.success ? teamOf(lastEv.actor) : 1 - teamOf(lastEv.actor)]++;
  }
  pub.turn = obs;
  AskMove buf[NSET * SETSZ * 3];
  if (enumerateAsks(pub, obsHandNow, obs, buf) <= 0) return false;
  std::unique_ptr<Agent> a = makeAgent(spec);
  a->reset(obs, obsHand0, rules, obsSeed);
  for (int i = 0; i < upto; i++) a->observe(ev[size_t(i)]);
  a->observe(lastEv);
  out = a->chooseAsk(pub);
  return true;
}

// ------------------------------------------------------------- the driver
inline SideStats runSide(const SideConfig& cfg) {
  { std::string why;
    if (!seedUsable(cfg.seed, why)) { fprintf(stderr, "fish: %s\n", why.c_str()); std::exit(5); } }
  int nT = cfg.threads > 0 ? cfg.threads : int(std::thread::hardware_concurrency());
  if (nT < 1) nT = 1;
  nT = std::min(nT, std::max(1, cfg.games));
  std::vector<SideStats> local{}; local.resize(size_t(nT));
  // Indexed BY DEAL, not concatenated in thread order, for the same reason
  // arena.hpp's `pairedAll` is: clusterBootstrap-style resampling over a vector
  // whose ORDER depends on the core count is not reproducible.
  std::vector<double> pairDealAll(size_t(std::max(0, cfg.games)), -1.0);
  std::vector<double> pairIndAll(size_t(std::max(0, cfg.games)), -1.0);
  std::atomic<int> next{0};
  auto t0 = std::chrono::steady_clock::now();

  std::vector<std::thread> pool;
  for (int th = 0; th < nT; th++) {
    pool.emplace_back([&, th] {
      SideStats& S = local[size_t(th)];
      // Constructed ONCE and reused across deals, as arena.hpp does.
      std::vector<std::unique_ptr<Agent>> aA(NPLAY), aB(NPLAY);
      std::vector<std::unique_ptr<SideAgent>> W(NPLAY);
      auto buildAgents = [&] {
        for (int p = 0; p < NPLAY; p++) {
          aA[p] = makeAgent(cfg.specA);
          aB[p] = makeAgent(cfg.specB);
          if (!W[p]) W[p] = std::make_unique<SideAgent>();
        }
      };
      buildAgents();
      Game game;
      while (true) {
        int i = next.fetch_add(1);
        if (i >= cfg.games) break;
        if (cfg.freshAgents) buildAgents();   // diagnostic; see SideConfig::freshAgents
        uint64_t s = mixSeed(cfg.seed, uint64_t(i) * 2654435761ull + 1);
        // v0.7 T1 (a), applied to this instrument.  The node sampler is keyed off
        // the DEAL INDEX, not off the thread, so which nodes S3 and S5 visit is a
        // function of the bank and the deal alone.  Keying it off the thread made
        // the reported asymmetry a function of std::thread::hardware_concurrency()
        // and of work-stealing order -- the same defect T1 (a) removed from the
        // bootstrap interval, and it moved S3's incumbent number by 0.9 points
        // between two runs of the identical command.
        Rng rng(mixSeed(cfg.key, uint64_t(i) * 2654435761ull + 7ull));

        // mode 0: the shipped deal-seeded stream.  mode 1: T10 -- a per-seat
        // stream keyed off a constant of the harness and the deal INDEX, so it
        // is independent of the deal.
        uint64_t transcript[2] = {0, 0};
        int winsA[2] = {0, 0};
        long long asksA[2] = {0, 0}, hitsA[2] = {0, 0};
        int nModes = cfg.s4 ? 2 : 1;
        for (int mode = 0; mode < nModes; mode++) {
          for (int rot = 0; rot < cfg.rotations; rot++) {
            int orient = (cfg.rotations == 2) ? rot : (rot / 3);
            int shift  = (cfg.rotations == 2) ? 0   : (rot % 3);
            Agent* ag[NPLAY];
            for (int p = 0; p < NPLAY; p++) {
              bool tA = (teamOf(p) == orient);
              W[p]->inner = tA ? aA[p].get() : aB[p].get();
              W[p]->record = (mode == 0) && ((tA && (cfg.s6 || cfg.s5)) || (!tA && cfg.s5));
              W[p]->captureK = (mode == 0) && cfg.s5;
              // T10 is applied to the CERTIFIED seats only.  Substituting the
              // opponent's stream as well would make the transcript-identity
              // arm useless whenever --b is stochastic: a deterministic --a
              // would be reported "stochastic" because the OPPONENT diverged.
              // Certifying team A means decoupling team A's stream and nothing
              // else, and it leaves the opponent's contribution paired.
              W[p]->substituteSeed = (mode == 1) && tA;
              W[p]->subSeed = mixSeed(cfg.key ^ 0xA5A5A5A5A5A5A5A5ull,
                                      uint64_t(i) * 6151ull + uint64_t(rot) * 131ull + uint64_t(p) + 77ull);
              ag[p] = W[p].get();
            }
            game.rotation = shift;
            game.trace.on = true; game.trace.events.clear();
            GameResult gr = game.run(s, cfg.rules, ag);
            int tA = orient;
            if (gr.winner == tA) winsA[mode]++;
            asksA[mode] += gr.teamAsks[tA];
            hitsA[mode] += gr.teamHits[tA];
            uint64_t h = 0xcbf29ce484222325ull;
            for (const Event& e : game.trace.events) h = eventHash(h, e);
            transcript[mode] = hashMix(transcript[mode], h);
            if (mode != 0) continue;

            uint64_t dealt[NPLAY];
            for (int p = 0; p < NPLAY; p++) dealt[p] = game.g.dealt[p];
            const std::vector<Event>& ev = game.trace.events;

            // ----------------------------------------------------------- S6
            if (cfg.s6) {
              bool bad = false;
              for (int p = 0; p < NPLAY; p++) {
                if (teamOf(p) != tA) continue;
                ReconResult R = cfg.reconInline
                    ? reconstructSeat(cfg.specA, p, dealt[p], cfg.rules,
                                      W[p]->usedSeed, W[p]->events, W[p]->recs)
                    : reconstructSeatIsolated(cfg.specA, p, dealt[p], cfg.rules,
                                              W[p]->usedSeed, W[p]->events, W[p]->recs);
                S.s6Nodes += R.nodes; S.s6Mismatch += R.mismatch;
                for (int q = 0; q < 5; q++) { S.s6ByKind[q] += R.byKind[q]; S.s6MisByKind[q] += R.misByKind[q]; }
                if (R.mismatch) bad = true;
              }
              S.s6Games++; if (bad) S.s6GamesMismatch++;
            }

            // ----------------------------------------------------------- S5
            if (cfg.s5) {
              for (int p = 0; p < NPLAY; p++) {
                bool refArm = (teamOf(p) != tA);
                TruthTrack tr; tr.init(dealt);
                size_t ri = 0; int taken = 0;
                for (size_t e = 0; e <= ev.size(); e++) {
                  while (ri < W[p]->recs.size() && size_t(W[p]->recs[ri].nObs) == e) {
                    const DecRec& r = W[p]->recs[ri];
                    if (r.kind == 0 && taken < cfg.s5nodes && rng.u32(3u) == 0u) {
                      runS5Node(r, tr, rng, cfg.s5draws, S, refArm); taken++;
                    }
                    ri++;
                  }
                  if (e < ev.size()) tr.apply(ev[e]);
                }
              }
            }

            // ----------------------------------------------------------- S3
            if (cfg.s3) {
              std::vector<int> askIdx;
              for (size_t e = 0; e < ev.size(); e++)
                if (ev[e].kind == Kind::Ask && teamOf(ev[e].actor) == tA) askIdx.push_back(int(e));
              std::vector<int> pick;
              int want = std::min<int>(cfg.s3nodes, int(askIdx.size()));
              for (int q = 0; q < want; q++) pick.push_back(askIdx[rng.u32(uint32_t(askIdx.size()))]);
              std::sort(pick.begin(), pick.end());
              pick.erase(std::unique(pick.begin(), pick.end()), pick.end());
              size_t pi = 0;
              TruthTrack cur; cur.init(dealt);
              for (size_t e = 0; e < ev.size() && pi < pick.size(); e++) {
                while (pi < pick.size() && size_t(pick[pi]) == e) {
                  const Event& evk = ev[e];
                  int j = evk.actor, c = evk.card, t = evk.target, st = setOf(c);
                  std::vector<int> alts;
                  for (int idx = 0; idx < SETSZ; idx++) {
                    int c2 = cardOf(st, idx);
                    if (c2 == c) continue;
                    if (cur.h[j] & bit(c2)) continue;                        // (a)
                    if (bool(cur.h[t] & bit(c2)) != bool(evk.success)) continue;   // (b)
                    alts.push_back(c2);
                  }
                  if (alts.empty()) { S.s3NoAlt++; pi++; continue; }
                  int c2 = alts[rng.u32(uint32_t(alts.size()))];
                  Event alt = evk; alt.card = uint8_t(c2);
                  S.s3Nodes++;
                  for (int obs = 0; obs < NPLAY; obs++) {
                    if (obs == j) continue;
                    if (cur.h[obs] & (bit(c) | bit(c2))) continue;           // (c)
                    uint64_t oseed = mixSeed(s, uint64_t(obs) + 77ull);
                    AskMove a0{}, a1{};
                    if (!s3AskQuery(cfg.specA, obs, dealt[obs], cfg.rules, oseed, ev, int(e), evk, cur.h[obs], a0)) continue;
                    if (!s3AskQuery(cfg.specA, obs, dealt[obs], cfg.rules, oseed, ev, int(e), alt,  cur.h[obs], a1)) continue;
                    bool chg = (a0.card != a1.card) || (a0.target != a1.target);
                    if (teamOf(obs) == teamOf(j)) { S.s3MateQ++; S.s3MateChg += chg ? 1 : 0; }
                    else                          { S.s3OppQ++;  S.s3OppChg += chg ? 1 : 0; }
                  }
                  pi++;
                }
                cur.apply(ev[e]);
              }
            }
          }
        }
        pairDealAll[size_t(i)] = double(winsA[0]);
        S.s4AskDeal += asksA[0]; S.s4HitDeal += hitsA[0];
        if (nModes > 1) {
          pairIndAll[size_t(i)] = double(winsA[1]);
          S.s4AskInd += asksA[1]; S.s4HitInd += hitsA[1];
          S.s4Deals++;
          if (transcript[0] == transcript[1]) S.s4Same++;
        }
      }
    });
  }
  for (auto& t : pool) t.join();
  SideStats T;
  for (auto& L : local) T.merge(L);
  T.s4PairDeal.clear(); T.s4PairInd.clear();
  for (size_t q = 0; q < pairDealAll.size(); q++)
    if (pairDealAll[q] >= 0 && pairIndAll[q] >= 0) {
      T.s4PairDeal.push_back(pairDealAll[q]); T.s4PairInd.push_back(pairIndAll[q]);
    }
  T.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  T.threadsUsed = nT;
  return T;
}

// --------------------------------------------------------------- reporting
// Paired difference of two per-deal win vectors, with a deal-clustered
// bootstrap interval -- the same estimator arena.hpp uses for a match.
inline void pairedDiff(const std::vector<double>& a, const std::vector<double>& b, int rotations,
                       double& mean, double& lo, double& hi, int B = 4000) {
  size_t n = std::min(a.size(), b.size());
  mean = lo = hi = 0;
  if (!n) return;
  std::vector<double> d(n);
  for (size_t i = 0; i < n; i++) d[i] = (a[i] - b[i]) / double(rotations);
  double sm = 0; for (double v : d) sm += v;
  mean = sm / double(n);
  std::vector<double> draws; draws.resize(size_t(B));
  Rng rng(0x5A1Dull);
  for (int r = 0; r < B; r++) {
    double acc = 0;
    for (size_t i = 0; i < n; i++) acc += d[rng.u32(uint32_t(n))];
    draws[size_t(r)] = acc / double(n);
  }
  std::sort(draws.begin(), draws.end());
  lo = draws[size_t(0.025 * B)]; hi = draws[size_t(0.975 * B)];
}

struct Verdict { const char* test; const char* status; std::string quantity; };

inline std::string fmt(double v, int prec = 4) {
  char b[64]; snprintf(b, sizeof(b), "%.*f", prec, v); return b;
}

// The quantity strings are written for a human first and carry newlines and
// quotes; a JSON line that embeds them raw is not parseable, which is the sort
// of defect that only shows up when someone tries to read the artifact.
inline std::string jsonEsc(const std::string& in) {
  std::string o; o.reserve(in.size() + 8);
  for (char c : in) {
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (uint8_t(c) < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", int(uint8_t(c))); o += b; }
    else o += c;
  }
  return o;
}

// The gate's decision rules, stated once and applied identically to every
// configuration.  Thresholds are absolute and chosen a priori; the calibration
// that justifies them is the planted-cheat panel in K0-sidechannel.txt.
struct GateReport {
  std::vector<Verdict> v;
  bool allPass = true;
  double s4WinDiff = 0, s4Lo = 0, s4Hi = 0;
  double s4HitDeal = 0, s4HitInd = 0;
  double s5True = 0, s5Samp = 0, s5RefTrue = 0, s5RefSamp = 0;
  double s3Mate = 0, s3Opp = 0, s3SE = 0;
};

// The gate's thresholds, in one place so that phase 6 can quote them and so that
// no configuration is judged by a rule fitted to it.  Each is an ABSOLUTE number
// chosen a priori at the scale of the offence, not at the scale of the noise;
// the planted-cheat panel in research/v07/results/K0-sidechannel.txt is what
// shows each one separates a cheat from the incumbent by an order of magnitude.
struct GateThresholds {
  // S3.  The raw response rate is NOT the offence -- substituting the named card
  // changes a genuine public certificate ("j lacks c" becomes "j lacks c'") and
  // every seat's posterior moves, teammate and opponent alike.  The offence is
  // the ASYMMETRY between a teammate's response and an opposing seat's response
  // to the identical public action.  It is still not a perfect control: the game
  // is not symmetric between a teammate and an opponent (a teammate cannot ask
  // the asker at all, and the substituted certificate bears on its own team's
  // half-suit completion), so a legitimate policy carries a nonzero floor here.
  // MEASURED FLOOR: the incumbent sits at +4.42 +/- 1.68 pts (bank 7030001, 200
  // deals, 1047/1837 queries).  The conv cheat sits at +73.71 +/- 1.33.  The
  // threshold is set an order of magnitude below the cheat and three times above
  // the floor; an asymmetry BETWEEN the floor and the threshold is a measured
  // signal to be reported, not a certified pass.
  static constexpr double s3Asym = 0.15;          // 15 points
  static constexpr double s3Sigma = 3.0;          // and at least 3 standard errors
  // S4.  A deterministic policy must give a transcript-identical run under T10.
  // A stochastic one must move neither the paired win rate nor the ask hit rate.
  // Measured: seed cheat 87.3% -> 53.5% hit, 50-point win-rate collapse.
  static constexpr double s4Hit = 0.03;           // 3 points of ask hit rate
  // S5.  Clairvoyance scale.  The test has a large measured FLOOR -- see the
  // note at runS5Node: the policy-agnostic posterior is not the measure the true
  // deal is drawn from once the transcript is policy-generated, and the incumbent
  // sits at 5.7-8.3 points of gap with its own policy prior switched OFF.  So the
  // threshold is set at clairvoyance scale and anything below it is reported, not
  // certified.  Measured: incumbent 5.7-8.3 pts, seed cheat 69.5 pts.
  static constexpr double s5Gap = 0.20;           // 20 points
};

inline GateReport judge(const SideConfig& cfg, const SideStats& T) {
  GateReport G;
  auto add = [&](const char* t, bool pass, bool skipped, const std::string& q) {
    G.v.push_back(Verdict{t, skipped ? "SKIP" : (pass ? "PASS" : "FAIL"), q});
    if (!skipped && !pass) G.allPass = false;
  };
  // ---- S3 ----------------------------------------------------------------
  if (cfg.s3) {
    G.s3Mate = T.s3MateQ ? double(T.s3MateChg) / double(T.s3MateQ) : 0.0;
    G.s3Opp  = T.s3OppQ  ? double(T.s3OppChg)  / double(T.s3OppQ)  : 0.0;
    // A response that is symmetric between a teammate and an opponent is not a
    // partnership understanding; it is the policy reacting to a public fact, and
    // the opposing team reads it identically.  The offence is the ASYMMETRY.
    double asym = G.s3Mate - G.s3Opp;
    G.s3SE = std::sqrt((T.s3MateQ ? G.s3Mate * (1 - G.s3Mate) / double(T.s3MateQ) : 0.0)
                     + (T.s3OppQ  ? G.s3Opp  * (1 - G.s3Opp)  / double(T.s3OppQ)  : 0.0));
    bool pass = !(std::fabs(asym) > GateThresholds::s3Asym
                  && std::fabs(asym) > GateThresholds::s3Sigma * G.s3SE);
    add("S3 listening-substitution", pass, T.s3MateQ == 0,
        "teammate-change " + fmt(100 * G.s3Mate, 3) + "%  opponent-change " + fmt(100 * G.s3Opp, 3)
        + "%  asymmetry " + fmt(100 * asym, 3) + " +/- " + fmt(100 * G.s3SE, 3)
        + " pts  (n_mate=" + std::to_string(T.s3MateQ)
        + ", n_opp=" + std::to_string(T.s3OppQ) + ", nodes=" + std::to_string(T.s3Nodes)
        + ", no rule-equivalent alternative at " + std::to_string(T.s3NoAlt) + ")");
  }
  // ---- S4 ----------------------------------------------------------------
  if (cfg.s4) {
    pairedDiff(T.s4PairDeal, T.s4PairInd, cfg.rotations, G.s4WinDiff, G.s4Lo, G.s4Hi);
    G.s4HitDeal = T.s4AskDeal ? double(T.s4HitDeal) / double(T.s4AskDeal) : 0.0;
    G.s4HitInd  = T.s4AskInd  ? double(T.s4HitInd)  / double(T.s4AskInd)  : 0.0;
    bool det = (T.s4Deals > 0 && T.s4Same == T.s4Deals);
    // Deterministic form: transcript-identical under an independent stream is a
    // proof the policy consumed nothing from the seed.  Stochastic form: the
    // paired win-rate difference must contain zero AND the ask hit rate must not
    // move -- a seed-reading policy loses its clairvoyance when the stream is
    // decoupled and both statistics collapse.
    bool pass = det || (G.s4Lo <= 0 && G.s4Hi >= 0
                        && std::fabs(G.s4HitDeal - G.s4HitInd) <= GateThresholds::s4Hit);
    add("S4 stream-independence", pass, T.s4Deals == 0,
        std::string(det ? "DETERMINISTIC: " : "stochastic: ")
        + std::to_string(T.s4Same) + "/" + std::to_string(T.s4Deals)
        + " transcripts identical under T10;  paired win-rate diff "
        + fmt(100 * G.s4WinDiff, 3) + " pts [" + fmt(100 * G.s4Lo, 3) + ", " + fmt(100 * G.s4Hi, 3)
        + "];  ask hit " + fmt(100 * G.s4HitDeal, 3) + "% -> " + fmt(100 * G.s4HitInd, 3) + "%");
  }
  // ---- S5 ----------------------------------------------------------------
  if (cfg.s5) {
    G.s5True = T.s5Nodes ? double(T.s5TrueHit) / double(T.s5Nodes) : 0.0;
    G.s5Samp = T.s5Nodes ? T.s5SampHit / double(T.s5Nodes) : 0.0;
    G.s5RefTrue = T.s5bNodes ? double(T.s5bTrueHit) / double(T.s5bNodes) : 0.0;
    G.s5RefSamp = T.s5bNodes ? T.s5bSampHit / double(T.s5bNodes) : 0.0;
    double gap = G.s5True - G.s5Samp;
    double refGap = G.s5RefTrue - G.s5RefSamp;
    bool pass = gap <= GateThresholds::s5Gap;    // clairvoyance-scale: see runS5Node
    add("S5 posterior-invariance", pass, T.s5Nodes == 0,
        "P(hit|truth) " + fmt(100 * G.s5True, 3) + "%  vs  P(hit|exact posterior) "
        + fmt(100 * G.s5Samp, 3) + "%   gap " + fmt(100 * gap, 3) + " pts  (n="
        + std::to_string(T.s5Nodes) + " asks, " + std::to_string(T.s5Unres) + " with the card unresolved)"
        + "\n        reference arm (the --b seats, same deals): gap " + fmt(100 * refGap, 3)
        + " pts over " + std::to_string(T.s5bNodes) + " asks;  A-minus-reference "
        + fmt(100 * (gap - refGap), 3) + " pts"
        + (cfg.specA == cfg.specB ? "  [MIRROR: the two arms are exchangeable, this difference carries no information]" : ""));
  }
  // ---- S6 ----------------------------------------------------------------
  if (cfg.s6) {
    double rate = T.s6Nodes ? double(T.s6Mismatch) / double(T.s6Nodes) : 0.0;
    bool pass = (T.s6Mismatch == 0);
    add("S6 seat-isolation", pass, T.s6Nodes == 0,
        std::to_string(T.s6Mismatch) + "/" + std::to_string(T.s6Nodes) + " decisions irreproducible ("
        + fmt(100 * rate, 4) + "%);  by kind ask " + std::to_string(T.s6MisByKind[0]) + "/" + std::to_string(T.s6ByKind[0])
        + ", declare " + std::to_string(T.s6MisByKind[1]) + "/" + std::to_string(T.s6ByKind[1])
        + ", pass " + std::to_string(T.s6MisByKind[2]) + "/" + std::to_string(T.s6ByKind[2])
        + ", willing " + std::to_string(T.s6MisByKind[3]) + "/" + std::to_string(T.s6ByKind[3])
        + ", bestGuess " + std::to_string(T.s6MisByKind[4]) + "/" + std::to_string(T.s6ByKind[4])
        + ";  games with any " + std::to_string(T.s6GamesMismatch) + "/" + std::to_string(T.s6Games));
  }
  return G;
}

inline void printSide(const SideConfig& cfg, const SideStats& T, const GateReport& G, std::ostream& os) {
  os << "v7side  a=" << cfg.specA << "  b=" << cfg.specB
     << "  seed=" << cfg.seed << "  deals=" << cfg.games << "x" << cfg.rotations << "\n";
  for (const auto& v : G.v)
    os << "  " << v.status << "  " << v.test << "\n        " << v.quantity << "\n";
  os << "  VERDICT  " << (G.allPass ? "CERTIFIED (S3,S4,S5,S6 all pass)" : "NOT CERTIFIED")
     << "   " << T.seconds << "s on " << T.threadsUsed << " threads\n";
}

inline void jsonSide(const SideConfig& cfg, const SideStats& T, const GateReport& G, std::ostream& os) {
  os << "{\"probe\":\"v7side\",\"a\":\"" << jsonEsc(cfg.specA) << "\",\"b\":\"" << jsonEsc(cfg.specB)
     << "\",\"seed\":" << cfg.seed << ",\"deals\":" << cfg.games << ",\"rotations\":" << cfg.rotations
     << ",\"verdict\":\"" << (G.allPass ? "CERTIFIED" : "NOT_CERTIFIED") << "\",\"tests\":{";
  bool first = true;
  for (const auto& v : G.v) {
    if (!first) os << ",";
    first = false;
    os << "\"" << v.test << "\":{\"status\":\"" << v.status << "\",\"quantity\":\"" << jsonEsc(v.quantity) << "\"}";
  }
  os << "},\"s3\":{\"nodes\":" << T.s3Nodes << ",\"noAlt\":" << T.s3NoAlt
     << ",\"mateQ\":" << T.s3MateQ << ",\"mateChg\":" << T.s3MateChg
     << ",\"oppQ\":" << T.s3OppQ << ",\"oppChg\":" << T.s3OppChg
     << ",\"mateRate\":" << G.s3Mate << ",\"oppRate\":" << G.s3Opp
     << ",\"asymmetry\":" << (G.s3Mate - G.s3Opp) << ",\"asymmetrySE\":" << G.s3SE << "}"
     << ",\"s4\":{\"deals\":" << T.s4Deals << ",\"identicalTranscripts\":" << T.s4Same
     << ",\"winDiff\":" << G.s4WinDiff << ",\"ci\":[" << G.s4Lo << "," << G.s4Hi << "]"
     << ",\"hitDealSeeded\":" << G.s4HitDeal << ",\"hitIndependent\":" << G.s4HitInd << "}"
     << ",\"s5\":{\"nodes\":" << T.s5Nodes << ",\"unresolved\":" << T.s5Unres
     << ",\"dpFail\":" << T.s5DpFail << ",\"draws\":" << T.s5Draws
     << ",\"pHitTruth\":" << G.s5True << ",\"pHitPosterior\":" << G.s5Samp
     << ",\"gap\":" << (G.s5True - G.s5Samp)
     << ",\"refNodes\":" << T.s5bNodes << ",\"refPHitTruth\":" << G.s5RefTrue
     << ",\"refPHitPosterior\":" << G.s5RefSamp << ",\"refGap\":" << (G.s5RefTrue - G.s5RefSamp)
     << ",\"mirror\":" << (cfg.specA == cfg.specB ? 1 : 0) << "}"
     << ",\"s6\":{\"nodes\":" << T.s6Nodes << ",\"mismatch\":" << T.s6Mismatch
     << ",\"byKind\":[" << T.s6ByKind[0] << "," << T.s6ByKind[1] << "," << T.s6ByKind[2] << "," << T.s6ByKind[3] << "," << T.s6ByKind[4] << "]"
     << ",\"misByKind\":[" << T.s6MisByKind[0] << "," << T.s6MisByKind[1] << "," << T.s6MisByKind[2] << "," << T.s6MisByKind[3] << "," << T.s6MisByKind[4] << "]"
     << ",\"games\":" << T.s6Games << ",\"gamesWithMismatch\":" << T.s6GamesMismatch << "}"
     << ",\"seconds\":" << T.seconds << ",\"threads\":" << T.threadsUsed << "}";
}

} // namespace v07side
} // namespace fish
