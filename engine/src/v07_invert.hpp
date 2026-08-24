// FishBot v0.7 -- white-box transcript inversion (threat-model class C5).
//
// THE HYPOTHESIS.  Every FishBot to date is deterministic: the deployed belief
// is a Sinkhorn fit that consumes no randomness, `chooseAsk` is an argmax, and
// v0.6's one stochastic switch is seeded from a hash of the PUBLIC event stream
// (THREAT-MODEL.md section 4.2, verified in code).  So for the shipped policy
// a_t = f(own hand, public transcript) exactly, and under the white-box grant
// (T3) the map f is public.  With a stochastic policy P(a | deal) is a soft
// likelihood and an observer's posterior stays diffuse; with a deterministic one
// P(a | deal) is in {0,1} and every observed action EXACTLY PARTITIONS the deal
// space.  Whether that partition shrinks the posterior below what the engine's
// certificates already imply, and whether the shrinkage converts into win rate,
// is unmeasured -- "no prior study has tested this", and either answer is a
// result (SUBOPTIMALITY-LEDGER.md L4).
//
// WHAT THIS FILE COMPUTES.  For an observer seat, at the moment a target seat j
// plays ask a_t:
//
//   1. Draw D deals from the observer's EXACT posterior over the hidden state.
//      DealDP is an exact sampler for constraints C1-C4 and C5 is enforced by
//      rejection (belief.hpp), so accepted draws are exact posterior samples.
//   2. For each draw, reconstruct j's information set -- the public deduction
//      state refined with the hand the draw gives j -- and ask the KNOWN policy
//      what it would have played.
//   3. The fraction q that reproduces a_t is the posterior mass the action
//      leaves alive.  -log2(q) is the contraction in bits, and it is a
//      contraction BEYOND the certificates, because the sample already
//      satisfies every certificate by construction.
//   4. Split the same sample by "does this draw put card c with seat p" and the
//      per-cell log-likelihood ratio falls out.  Accumulated over the game that
//      is a table of evidence in exactly the shape `Knowledge::priorWeight`
//      already consumes.
//
// WHY THAT SHAPE.  v0.5's theta/phi prior -- "a player who asked in this
// half-suit probably holds cards of it" -- is a hand-fitted, two-parameter
// approximation to precisely this quantity.  Ledger entry C2 records that the
// policy prior is the ENTIRE difference between the exact posterior and the
// deployed approximation as predictors (1.38218 nats against 1.42246, argmax
// 51.49% against 47.94%).  So C5 is not a new channel bolted onto the belief;
// it is the exact version of a channel the corpus already ships an approximation
// of, and the corpus has already measured that channel to be the one that
// matters.
//
// THE HONEST LIMITATION, STATED HERE RATHER THAN IN A FOOTNOTE.  Step 4
// marginalises: it accumulates per-(card, seat) evidence rather than carrying a
// joint particle set across the game.  A joint filter over a static hidden state
// degenerates -- the deal never changes, so particle weights collapse and
// rejuvenation loses exactly the evidence it is trying to keep.  The marginal
// accumulation is a Rao-Blackwellised approximation that treats the evidence
// from different actions as conditionally independent given the certificates.
// It is an UNDER-estimate of what a perfect inverter would extract, so a
// positive result here is a lower bound and a null result does not close the
// class.  The bit measurement in `InversionProbe` is NOT approximate in this
// way: it measures the exact one-step contraction.
#pragma once
#include "v06_rollout.hpp"
#include "belief.hpp"
#include <memory>
#include <string>

namespace fish {
namespace v07 {

// ---------------------------------------------------------------- the oracle
// "What would the target have played, at seat j, holding this hand, here?"
struct PolicyOracle {
  std::string spec;
  std::unique_ptr<Agent> ag;
  long long calls = 0;

  void ensure() { if (!ag) ag = makeAgent(spec); }

  // `pubK` is the PUBLIC deduction state at the decision (nobody's hand known);
  // `pub` is the public state at the decision, with `turn` already set to j.
  AskMove wouldAsk(const PublicState& pub, const Knowledge& pubK, int j, uint64_t hand, uint64_t seed) {
    ensure();
    calls++;
    Knowledge kj = pubK;
    v06::refineWithHand(kj, j, hand);
    ag->resetWithKnowledge(j, hand, pub.rules, seed, kj);
    return ag->chooseAsk(pub);
  }
};

// --------------------------------------------------------- inversion machine
// Maintains, for one observer, the public deduction state and the accumulated
// per-(card, seat) policy evidence.  Used both by the offline bit probe and by
// the C5 responder, so the two cannot drift apart.
struct Inverter {
  PolicyOracle oracle;
  Knowledge pubK;                       // public deduction state, nobody's hand
  PublicState mirror;                   // public state as of the last event
  double ll[NCARD][NPLAY];              // accumulated log-likelihood ratios
  Rng rng;
  int  nDet = 32;                       // draws per inverted action
  double alpha = 0.5;                   // Laplace smoothing on the split counts
  // TEMPERING.  The per-action evidence is summed across the game as if the
  // actions were conditionally independent given the certificates.  They are
  // not: two asks by the same seat in the same half-suit carry overlapping
  // information, and summing their log-likelihood ratios multiplies a
  // confidence that should have been shared.  Naive-Bayes over-counting is the
  // classic failure mode and the classic remedy is to temper -- which is also,
  // exactly, what v0.5's theta/phi already are: a hand-FITTED coefficient on the
  // same evidence.  So `gain` is not a fudge factor bolted on after a
  // disappointment; it is the coordinate the corpus has always had, with the
  // heuristic behind it replaced by a measurement.  Its value is chosen by
  // sweeping against ground truth in `fish v7bits`, not by taste.
  double gain = 1.0;   // swept in `fish v7bits`; 1.0 at focus 2 is the measured optimum
  // Shrinkage on the per-cell conditional.  With D draws spread over six
  // possible owners a cell sees ~D/6 samples, and an unshrunk ratio estimated
  // from ten Bernoulli draws is mostly noise.  kappa pseudo-draws AT THE
  // OVERALL RATE pull an unsupported cell to zero evidence.
  double kappa = 3.0;
  double stepClip = 1.25;               // maximum |LLR| contributed by one action
  // 0 = per (card, seat).  1 = per (half-suit, seat), which is the resolution
  // the theta/phi heuristic works at and has six times less variance.  2 = the
  // mean of the two.
  int  mode = 0;
  // WHICH CELLS THE ACTION SPEAKS TO.  Given the public state, the observed
  // action is a function of the ACTOR's hand and of nothing else.  So
  // P(a_t | owner(c) = p) differs from P(a_t) at first order only for p = the
  // actor; for every other seat it differs only through the induced shift in
  // the actor's hand distribution, which is a second-order effect a sample of
  // D draws cannot resolve and which the re-centring below already expresses.
  // Updating all six columns therefore spends most of the sample estimating
  // noise.  focus 1 updates the actor's column only and lets the re-centring
  // distribute the complement; focus 2 additionally restricts to half-suits the
  // actor has actually asked in.  focus 0 is the unrestricted form, kept
  // because it is the naive thing to do and the report should be able to say
  // what it costs.
  int  focus = 1;
  int  fromEvent = 0;                   // do not invert before this event index
  int  maxQ = 0;                        // 0 = no cap; else skip when |unresolved| > maxQ
  bool inited = false;
  // diagnostics
  long long inversions = 0, draws = 0, consistent = 0, dpFails = 0;
  double bitsSum = 0; long long bitsN = 0;

  void reset(const Rules& r, int deckSets, uint64_t seed) {
    v06::initPublicKnowledge(pubK, deckSets);
    mirror = PublicState{};
    mirror.rules = r;
    for (int s = 0; s < NSET; s++) mirror.setActive[s] = (s < deckSets);
    for (int p = 0; p < NPLAY; p++) mirror.handCount[p] = uint8_t(deckSets * SETSZ / NPLAY);
    mirror.score[0] = mirror.score[1] = 0;
    mirror.nAsks = 0; mirror.nEvents = 0; mirror.turn = 0;
    mirror.history.clear();
    memset(ll, 0, sizeof(ll));
    rng = Rng(seed ? seed : 0x5EEDull);
    inited = true;
  }

  // Advance the public mirror.  Called AFTER any inversion that used the
  // pre-event state.
  void advance(const Event& e) {
    mirror.history.push_back(e);
    pubK.onEvent(e);
    for (int p = 0; p < NPLAY; p++) mirror.handCount[p] = e.handCount[p];
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      mirror.setActive[e.decl.set] = false;
      int team = teamOf(e.actor);
      mirror.score[e.success ? team : 1 - team]++;
    }
    mirror.nEvents++;
    if (e.kind == Kind::Ask) { mirror.nAsks++; mirror.turn = e.success ? e.actor : e.target; }
    else if (e.kind == Kind::Pass) mirror.turn = e.target;
  }

  // The core step.  `obs` is the observer's own knowledge (which already carries
  // its hand and every certificate).  Returns the fraction of the observer's
  // posterior under which the target would have played this exact ask, or -1 if
  // the step could not be taken.
  double invertAsk(const Knowledge& obs, const Event& e, double* outBits) {
    if (e.kind != Kind::Ask) return -1;
    if (mirror.nEvents < fromEvent) return -1;
    int j = e.actor;
    if (maxQ > 0 && __builtin_popcountll(obs.unresolved) > maxQ) return -1;
    DealDP dp;
    if (!dp.build(obs)) { dpFails++; return -1; }

    // The pre-event public state: hand counts and turn are as of before the ask,
    // which for an ask means the actor holds the turn.
    PublicState pre = mirror;
    pre.turn = j;

    std::array<uint8_t, NCARD> base{};
    for (int c = 0; c < NCARD; c++) base[c] = obs.owner[c];
    // Split counts: nAll[c][p] over accepted draws, nOk[c][p] over those the
    // policy reproduces.
    static thread_local std::vector<double> nAll, nOk;
    nAll.assign(size_t(NCARD) * NPLAY, 0.0);
    nOk.assign(size_t(NCARD) * NPLAY, 0.0);
    int got = 0, ok = 0;
    int tries = 0, maxTries = nDet * 40 + 200;
    uint64_t hand[NPLAY];
    std::array<uint8_t, NCARD> o;
    while (got < nDet && tries < maxTries) {
      tries++;
      o = base;
      dp.sample(rng, o.data());
      if (!Belief::satisfies(obs, o.data())) continue;
      got++; draws++;
      v06::RolloutEngine::handsFrom(pre, obs, o.data(), hand);
      AskMove mv = oracle.wouldAsk(pre, pubK, j, hand[j], mixSeed(rng.next(), 0x1Bull));
      bool match = (mv.card == e.card && mv.target == e.target);
      if (match) { ok++; consistent++; }
      uint64_t u = obs.unresolved;
      while (u) {
        int c = __builtin_ctzll(u); u &= u - 1;
        int p = o[c];
        if (p < 0 || p >= NPLAY) continue;
        nAll[size_t(c) * NPLAY + size_t(p)] += 1.0;
        if (match) nOk[size_t(c) * NPLAY + size_t(p)] += 1.0;
      }
    }
    if (!got) return -1;
    inversions++;
    double q = double(ok) / double(got);
    if (outBits) *outBits = (q > 0) ? -std::log2(std::max(q, 1.0 / (double(got) + 1.0))) : -std::log2(1.0 / (double(got) + 1.0));
    if (q > 0) { bitsSum += (outBits ? *outBits : 0.0); bitsN++; }
    // Per-cell evidence: log P(a_t | owner(c) = p) - log P(a_t), shrunk toward
    // zero by `kappa` pseudo-draws at the overall rate, tempered by `gain`, and
    // capped per action.
    if (ok > 0) {
      const double logQ = std::log(q);
      auto llrOf = [&](double a, double b) {
        double cond = (b + kappa * q) / (a + kappa);
        double z = std::log(std::max(1e-12, cond)) - logQ;
        if (z > stepClip) z = stepClip; else if (z < -stepClip) z = -stepClip;
        return z;
      };
      // Half-suit aggregate, used by modes 1 and 2.
      double setAll[NSET][NPLAY], setOk[NSET][NPLAY];
      if (mode >= 1) {
        memset(setAll, 0, sizeof(setAll)); memset(setOk, 0, sizeof(setOk));
        uint64_t u2 = obs.unresolved;
        while (u2) {
          int c = __builtin_ctzll(u2); u2 &= u2 - 1;
          for (int p = 0; p < NPLAY; p++) {
            setAll[setOf(c)][p] += nAll[size_t(c) * NPLAY + size_t(p)];
            setOk[setOf(c)][p]  += nOk[size_t(c) * NPLAY + size_t(p)];
          }
        }
      }
      uint64_t u = obs.unresolved;
      while (u) {
        int c = __builtin_ctzll(u); u &= u - 1;
        if (focus >= 2 && !obs.askCount[j][setOf(c)]) continue;
        for (int p = 0; p < NPLAY; p++) {
          if (focus >= 1 && p != j) continue;
          double a = nAll[size_t(c) * NPLAY + size_t(p)];
          double b = nOk[size_t(c) * NPLAY + size_t(p)];
          double z = 0;
          if (mode == 0) { if (a <= 0) continue; z = llrOf(a, b); }
          else if (mode == 1) {
            double sa = setAll[setOf(c)][p]; if (sa <= 0) continue;
            z = llrOf(sa, setOk[setOf(c)][p]);
          } else {
            double sa = setAll[setOf(c)][p];
            double z1 = (a > 0) ? llrOf(a, b) : 0.0;
            double z2 = (sa > 0) ? llrOf(sa, setOk[setOf(c)][p]) : 0.0;
            z = 0.5 * (z1 + z2);
          }
          ll[c][p] += gain * z;
        }
      }
      // Re-centre each card's row so the table carries only RELATIVE evidence;
      // an overall level shift is absorbed by the posterior's normalisation
      // anyway and would otherwise drift without bound over a long game.
      u = obs.unresolved;
      while (u) {
        int c = __builtin_ctzll(u); u &= u - 1;
        double m = 0; int n2 = 0;
        for (int p = 0; p < NPLAY; p++) if (obs.mask[c] & (1u << p)) { m += ll[c][p]; n2++; }
        if (n2) { m /= n2; for (int p = 0; p < NPLAY; p++) ll[c][p] -= m; }
      }
    }
    return q;
  }
};

} // namespace v07
} // namespace fish
