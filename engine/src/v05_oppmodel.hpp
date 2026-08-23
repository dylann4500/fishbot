// M7 -- online per-seat opponent model for FishBot v0.5.
//
// WHAT THIS REPLACES.  v0.4 models the other five seats with two global scalars,
// `priorTheta` and `priorPhi`, consumed by `Knowledge::priorWeight`
// (belief.hpp:100-108) as
//
//     w(c, p) = exp( theta * a[p][S(c)] - phi * (T[p] - a[p][S(c)]) ),   clipped to +-2.6
//
// with a = asks by p in the half-suit of c, T = p's asks all game.  One pair for
// all five seats, never updated in-game.  The project owner's manoeuvre -- hold
// two cards of a half-suit you were asked for, then deliberately never ask back
// in it -- is invisible to that model by construction.
//
// ---------------------------------------------------------------------------
// WHY priorPhi CANNOT BE THE SILENCE CHANNEL (the algebra, verified)
// ---------------------------------------------------------------------------
// Rearrange the exponent:
//
//     theta*a - phi*(T - a)  =  (theta + phi) * a[p][S(c)]  -  phi * T[p]
//                               \______ card-dependent ____/   \___ seat-only ___/
//
// so, before the clip,
//
//     w(c, p)  =  A(c, p) * g(p),      A(c,p) = exp((theta+phi) * a[p][S(c)]),
//                                      g(p)   = exp(-phi * T[p]).
//
// The posterior is produced by `Belief::sinkhornDisj` (belief.hpp:478-529), which
// is iterative proportional fitting of w to row sums 1 (each card has one owner)
// and column sums q_p (each seat's spare capacity).  IPF's limit for a kernel K
// with total support is the unique matrix diag(u) K diag(v) carrying those
// margins.  Replacing K by K*diag(g) replaces the limit's diag(v) by diag(v/g)
// and leaves the matrix itself IDENTICAL.  A factor that depends on the SEAT
// ALONE is therefore erased exactly.  `phi` is not a second channel: the pair
// (theta, phi) is the single effective certificate weight
//
//     theta_eff = theta + phi,
//
// and v0.4's shipped (0.26380, 0.13280) is exactly theta_eff = 0.39660.
//
// Measured, on 1,852 real v0.4 mirror belief states (research/v05/results/
// M7-design.md §1): with the +-2.6 clip removed and the fit run to convergence,
// (theta, phi) and (theta+phi, 0) agree to mean 1.9e-7 / max 4.3e-4 per marginal.
// The residual in the shipped code -- mean 2.6e-3, max 0.444 -- comes entirely
// from two artefacts: the +-2.6 clip, which is not separable and binds low on
// 12.1% of (seat, half-suit) cells, and `sinkInner = 8`, which stops short of the
// IPF fixed point.  Neither is a modelled signal.
//
// THE CONDITION A NEW STATISTIC MUST MEET is therefore exactly this: its
// log-weight must not be a function of the seat alone.  A (seat, half-suit)
// statistic is sufficient -- that is what `theta` is, and `theta` demonstrably
// moves the posterior (control measurement: a (seat, half-suit) tilt of -0.35
// per unit moves marginals by mean 4.9e-3 / max 0.41 at converged settings).
// M7's statistics are indexed by (seat, half-suit) and, through the opportunity
// weight below, by card as well.  They survive.
//
// ---------------------------------------------------------------------------
// WHAT M7 ADDS
// ---------------------------------------------------------------------------
// 1. Two TIME-LOCAL episode statistics, both driven by a public act so that a
//    seat which never acts generates no evidence at all:
//
//    REPLY episode  -- opened when seat p is the TARGET of an ask in half-suit S.
//      Outcome: did p ask in S within its next W1 own turns (fast), within W2
//      (slow), or not at all (never)?  `never` is the owner's manoeuvre.
//    PROBE episode  -- opened when seat p ASKS in S (emitting a C5 certificate).
//      Outcome: did p ask in S again within W1 / W2 / not at all?  `never` is the
//      Feint's signature (P3 §1: ask in a half-suit you hold exactly one card of).
//
//    The likelihood is exact on one side.  Ask legality (fish.hpp:183-184) requires
//    the asker to hold another card of the half-suit, so
//
//        P(p asks in S | p holds no card of S) = 0     exactly,
//
//    which makes a `fast`/`slow` outcome a hard C5 certificate the belief engine
//    already applies, and makes `never` the exact soft complement of C5.  Only
//    `never` outcomes tilt the belief; the other two feed the type posterior only.
//    Nothing is double-counted.
//
// 2. A PER-SEAT POSTERIOR over a small type library, updated online from that
//    seat's public record (Bayesian Policy Reuse, Rosman, Hawasly & Ramamoorthy,
//    Machine Learning 104:99-127, 2016).  A ~100-observation episode is far too
//    short to fit a per-seat policy; it is long enough to place mass over a
//    handful of pre-characterised types.  Each type carries its own
//    responsiveness rates AND its own certificate-weight multiplier, because P3
//    measured the two deceptive archetypes as OPPOSITE miscalibrations: against
//    the Withholder v0.4 is under-confident by -0.090 in the "asked once" cell,
//    against the Feint over-confident by +0.096.  A model that collapsed them
//    would cancel the correction to zero.
//
// 3. DATA-BIASED SHRINKAGE (Johanson & Bowling, AISTATS 2009).  Every tilt is
//    scaled by n / (n + n0), n = resolved uncensored episodes in that information
//    set, n0 = `dataBias`, the explicit exploitation/robustness dial.  n0 -> inf
//    reproduces v0.4 exactly; n0 -> 0 is full exploitation of the estimated type.
//    A deliberately silent seat that is never asked and never asks produces no
//    episodes, so n = 0, the tilt is 0, and it receives the population prior
//    rather than being misread.
//
// 4. SELF-CORRECTION AGAINST THE OWNER'S MANOEUVRE.  The silence tilt is
//    log(pNever) under the seat's CURRENT type mixture.  For an honest type
//    pNever is small, so silence is strong evidence of a void.  For the
//    Withholder type pNever is near 1, so log(pNever) ~ 0 and silence carries
//    almost no evidence.  As the type posterior moves toward Withholder the tilt
//    switches itself off -- v0.5 stops being fooled by the manoeuvre without ever
//    asserting the opposite.  Separately, the Withholder type raises that seat's
//    certificate weight, which is the direction P3 §2 measured as correct.
//
// SAFETY PROPERTIES, each keyed to a measured constraint:
//   * TILT ONLY, NEVER A CONSTRAINT.  M7 reweights deals that satisfy C1-C5; it
//     can never exclude one.  A deceiver can withhold evidence but cannot make
//     v0.5 believe something provably false (P3 §6.3.1).
//   * BOUNDED.  |z_M7| <= tiltCap (0.90) and the per-seat certificate multiplier
//     is clipped to [1/1.5, 1.5].  P3 §5 measured the cliff: 2x theta_eff costs
//     5.25 points [2.45, 8.05] paired and 4x loses the match outright.  1.5x is
//     inside the measured-safe radius on the dangerous side.
//   * TYPE FLOOR.  Every type keeps at least `typeFloor` posterior mass, so a
//     data-poisoning opponent cannot drive the mixture to a corner (P3 §6.3.4).
//   * DECLARATIONS ARE NOT UNLOCKED BY THE TILT.  `declWeight` defaults to 0:
//     the tilt buys ask quality by accepting more over-confident marginals, which
//     is a good trade for asking and a bad one for the irreversible action
//     (P3 §6.3.5).
//   * THE PRIOR IS NOT DELETED.  P3 §4 measured the policy-agnostic posterior as
//     4.40 points [1.60, 7.15] WORSE.  M7 shrinks toward v0.4's prior, never
//     toward none.
//
// This header touches no protected file.  It is included by v05.hpp through
// research/v05/patches/M7.patch.
#pragma once
#include "fish.hpp"
#include "belief.hpp"

namespace fish {
namespace m7 {

// --------------------------------------------------------------- type library
// The paper's four-way shorthand is ordinary / silent / v0.3-like / deceptive.
// "Deceptive" is instantiated as its two measured archetypes because P3 shows
// they miscalibrate v0.4 in opposite directions.
enum OppType : int { T_ORDINARY = 0, T_V03LIKE = 1, T_SILENT = 2, T_WITHHOLD = 3, T_FEINT = 4 };
static constexpr int NTYPE = 5;

struct TypeProfile {
  const char* name;
  // P(outcome | the seat holds at least one card of S), per episode kind.
  // pNever is the residual and is what the belief tilt reads.
  double replyFast, replySlow;
  double probeFast, probeSlow;
  double thetaMul;    // multiplier on the effective certificate weight for this seat
  double prior;       // population prior mass
};

// Defaults.  The responsiveness rates are MEASURED, not guessed: a self-play
// episode census over 30 games per policy, conditioned on ground-truth "does this
// seat hold a card of S", seed 20260822 (research/v05/results/M7-design.md §3).
// Reply-episode n = 159-690 per policy, probe-episode n = 1,781-3,963.
//
//   policy            reply fast/slow/never       probe fast/slow/never
//   v0.5 (M1,M2,M8)   .946 / .046 / .008          .946 / .034 / .021
//   v0.4              .941 / .042 / .017          .978 / .016 / .006
//   v0.3              .890 / .084 / .027          .931 / .040 / .030
//   silent:tol=0.10   .915 / .073 / .012          .932 / .054 / .014
//   feint             .912 / .068 / .020          .910 / .068 / .022
//   withholder:k=6    .333 / .164 / .503          .974 / .015 / .012
//
// Two readings, both of which shaped this table.
//  (i) The REPLY channel separates the owner's manoeuvre by a factor of ~30 in
//      the likelihood of a single episode (never: 0.503 vs 0.008-0.027).  This is
//      the channel M7 is built on.
// (ii) The PROBE channel separates nothing on this panel (never: 0.006-0.030 for
//      every policy including `feint`).  The Feint archetype keeps a card of the
//      half-suit it feints in, so it remains legally able to ask there and does.
//      The probe channel is retained because it is free and because a
//      ask-once-and-abandon opponent is a real strategy, but on today's evidence
//      it is close to inert.  See M7-design.md §3.2 for the negative result and
//      for the two further statistics (ask breadth, cards-held-at-ask) that were
//      also measured and also fail to identify the Feint.
//
// theta multipliers come from P3 §2's calibration table and are cap-bound by
// design:
//   Withholder  truth 0.4367 vs model 0.3464 at a=1  ->  +0.380 logit  -> mul 1.96
//   Feint       truth 0.2815 vs model 0.3776 at a=1  ->  -0.437 logit  -> mul < 0
// Both are clipped to [1/1.5, 1.5]; the shipped values sit at the clip, which is
// the deliberate robustness choice given the P3 §5 cliff.
//
// `feint` ships with prior 0.0 -- present so M10 can refit it the moment a
// statistic that identifies it exists, inert until then.  Shipping it with mass
// would apply a constant across-the-board discount to every seat's certificate
// weight and dress it up as per-seat modelling, which the evidence does not
// support.
inline const TypeProfile* types() {
  static const TypeProfile T[NTYPE] = {
    // name          replyFast replySlow probeFast probeSlow thetaMul prior
    { "ordinary",      0.941,    0.042,    0.978,    0.016,    1.00,  0.52 },
    { "v03like",       0.890,    0.084,    0.931,    0.040,    1.10,  0.16 },
    { "silent",        0.915,    0.073,    0.932,    0.054,    1.05,  0.16 },
    { "withholder",    0.333,    0.164,    0.974,    0.015,    1.50,  0.16 },
    { "feint",         0.912,    0.068,    0.910,    0.068,    0.67,  0.00 },
  };
  return T;
}
inline double pNeverReply(const TypeProfile& t) {
  double v = 1.0 - t.replyFast - t.replySlow; return v < 1e-3 ? 1e-3 : v;
}
inline double pNeverProbe(const TypeProfile& t) {
  double v = 1.0 - t.probeFast - t.probeSlow; return v < 1e-3 ? 1e-3 : v;
}

// -------------------------------------------------------------------- config
struct M7Config {
  bool   enabled        = true;
  int    windowFast     = 2;      // W1, in the seat's OWN turns
  int    windowSlow     = 6;      // W2; `withholder:k=6` is silent for exactly 6
  // THE EXPLOITATION / ROBUSTNESS DIAL.  Johanson & Bowling's data-biased
  // response parameter: the tilt applied in an information set is scaled by
  // n / (n + dataBias).  Report the frontier over {inf, 8, 4, 2, 1, 0.5, 0}.
  double dataBias       = 2.0;
  double typeFloor      = 0.02;   // minimum posterior mass per type
  double tiltCap        = 0.90;   // |z_M7| clip, well inside belief.hpp's +-2.6
  double thetaMulCap    = 1.50;   // per-seat certificate multiplier in [1/cap, cap]
  // Tilt strength.  NOT 1.0, and the reason is not tuning -- it is the same
  // double-counting P3 §6.2(ii) identified for theta.  The responsiveness rates
  // below are RAW conditional rates; the belief they tilt has already applied
  // C1-C5, which explains much of the same variation, so a tilt fitted to the raw
  // conditional double-counts the hard evidence.  P3 measured the size of that
  // gap for theta: v0.4's fitted value is 0.264 against an empirical raw
  // coefficient of 0.907 for honest play, a residual fraction of 0.29.  The
  // default carries that measured fraction across to the silence channel.
  // It is a DERIVED placeholder, not a fit: M10 must fit it on the residual after
  // C1-C5, using the truth-vs-marginal calibration table of P3 §2 bucketed by
  // silence-episode count.  See M7-design.md §5 for why the 50-game sanity runs
  // cannot be used to choose it.
  double askWeight      = 0.30;   // tilt strength on the ask path
  double declWeight     = 0.00;   // tilt strength on the declaration path (P3 §6.3.5)
  double qLo            = 0.03;   // episodes are censored outside this band: a cell
  double qHi            = 0.97;   // whose latent "holds S" is already settled is mute
  bool   silenceChannel = true;   // ablate the negative (silence) channel alone
  bool   thetaChannel   = true;   // ablate the per-seat certificate weight alone
  bool   opportunity    = true;   // ablate the card-level opportunity weight alone
  // BPR is defined ACROSS episodes of a task, and one deal supplies only ~2-5
  // uncensored observations per seat (measured; M7-design.md §4).  Within a
  // single deal the type posterior barely moves, which is the honest identifi-
  // ability budget P3 §6.2 predicted.  Carrying it across the deals of a match
  // against the same table is what makes the mechanism work -- and is what a
  // human does.  `carryWeight` is the between-deal retention: 0 = independent
  // deals (v0.4 behaviour), 1 = never forget.
  bool   persistTypes   = true;
  double carryWeight    = 0.70;
};

// Optional census hook for offline profile fitting; null in ordinary play.
struct Trace {
  // kind: 0 reply, 1 probe.  outcome: 0 fast, 1 slow, 2 never.
  virtual ~Trace() = default;
  virtual void onEpisode(int seat, int set, int kind, int outcome, double q) = 0;
};

// ---------------------------------------------------------------- the model
struct OppModel {
  M7Config cfg;
  int me = 0;
  Trace* trace = nullptr;

  struct Cell {
    uint8_t replyOpen = 0, replyTurns = 0;
    uint8_t probeOpen = 0, probeTurns = 0;
    uint16_t n = 0;        // resolved, uncensored episodes in this (seat, half-suit)
    float llr = 0.0f;      // sum of log P(never | current mixture);  <= 0
  };
  Cell cell[NPLAY][NSET];
  double post[NPLAY][NTYPE];
  uint16_t nSeat[NPLAY];

  void reset(int seat, const M7Config& c) {
    me = seat; cfg = c; trace = nullptr;
    for (int p = 0; p < NPLAY; p++) {
      nSeat[p] = 0;
      for (int s = 0; s < NSET; s++) cell[p][s] = Cell{};
      for (int t = 0; t < NTYPE; t++) post[p][t] = types()[t].prior;
      renorm(p);
    }
  }

  // Start a new deal against the SAME table.  Per-deal evidence (episodes, the
  // silence log-likelihoods, the shrinkage counts) is discarded because it is
  // about this deal's cards; the type posterior is retained, blended back toward
  // the population prior by (1 - carryWeight).  Call this instead of reset()
  // between the deals of a match; call reset() when the table changes.
  void newDeal() {
    const TypeProfile* T = types();
    for (int p = 0; p < NPLAY; p++) {
      nSeat[p] = 0;
      for (int s = 0; s < NSET; s++) cell[p][s] = Cell{};
      if (!cfg.persistTypes) { for (int t = 0; t < NTYPE; t++) post[p][t] = T[t].prior; }
      else { double w = cfg.carryWeight;
             for (int t = 0; t < NTYPE; t++) post[p][t] = w * post[p][t] + (1.0 - w) * T[t].prior; }
      renorm(p);
    }
  }

  // The floor bounds the worst case a data-poisoning opponent can reach (P3
  // §6.3.4): no type can ever be driven out of the mixture.  It applies only to
  // types the population prior actually admits -- a type shipped with prior 0 is
  // switched off, not floored back in.
  void renorm(int p) {
    const TypeProfile* T = types();
    double s = 0;
    for (int t = 0; t < NTYPE; t++) {
      if (T[t].prior <= 0) { post[p][t] = 0; continue; }
      if (post[p][t] < cfg.typeFloor) post[p][t] = cfg.typeFloor;
      s += post[p][t];
    }
    if (s <= 0) { for (int t = 0; t < NTYPE; t++) post[p][t] = T[t].prior > 0 ? 1.0 : 0.0; s = 0;
      for (int t = 0; t < NTYPE; t++) s += post[p][t]; }
    for (int t = 0; t < NTYPE; t++) post[p][t] /= s;
  }

  // ---- cheap, Belief-free estimate of P(seat p holds >= 1 card of half-suit S).
  // Capacity-proportional, exactly the shape of Knowledge::cheapTeamProb.  Using
  // a self-contained estimate rather than the agent's Belief avoids the staleness
  // hazard the diagnosis found in v0.4 (defect H: computeAggregates before
  // refresh) and keeps the model independent of which belief mode is running.
  static double holdsSetProb(const Knowledge& k, int p, int S) {
    if (!k.setActive[S]) return -1.0;
    uint8_t q[NPLAY]; k.capacities(q);
    double none = 1.0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(S, i);
      if (k.owner[c] == uint8_t(p)) return 1.0;
      if (k.owner[c] != UNKNOWN) continue;
      if (!(k.mask[c] & (1u << p))) continue;
      double den = 0;
      for (int r = 0; r < NPLAY; r++) if (k.mask[c] & (1u << r)) den += q[r];
      if (den <= 0) continue;
      none *= (1.0 - double(q[p]) / den);
    }
    return 1.0 - none;
  }

  // Could seat p plausibly have been asking for card c, on public information?
  // A card of S whose possible owners are all p's own teammates or dead seats is
  // one p would never ask for, so p's silence in S says nothing about it.  This
  // is what makes the tilt vary WITHIN a half-suit rather than only across them.
  static bool askable(const Knowledge& k, int p, int c) {
    if (!k.setActive[setOf(c)]) return false;
    for (int t = 0; t < NPLAY; t++) {
      if (teamOf(t) == teamOf(p)) continue;
      if (!k.handCount[t]) continue;
      if (k.owner[c] == uint8_t(t)) return true;
      if (k.owner[c] == UNKNOWN && (k.mask[c] & (1u << t))) return true;
    }
    return false;
  }

  // Is there anything left in S for p to ask about at all?  If not, an open
  // episode is censored rather than scored as silence.
  static bool couldAskIn(const Knowledge& k, int p, int S) {
    if (!k.setActive[S] || !k.handCount[p]) return false;
    for (int i = 0; i < SETSZ; i++) if (askable(k, p, cardOf(S, i))) return true;
    return false;
  }

  // ---- episode resolution ------------------------------------------------
  // kind 0 = reply, 1 = probe.  outcome 0 = fast, 1 = slow, 2 = never.
  void resolve(int p, int S, int kind, int outcome, const Knowledge& k) {
    double q = holdsSetProb(k, p, S);
    bool censored = (q < cfg.qLo || q > cfg.qHi);
    if (outcome == 2 && !couldAskIn(k, p, S)) censored = true;
    if (trace) trace->onEpisode(p, S, kind, outcome, q);
    if (censored) return;

    const TypeProfile* T = types();
    // Belief tilt: ONLY a `never` outcome carries soft evidence.  `fast` and
    // `slow` mean p asked in S, which C5 has already applied as a hard
    // certificate; scoring them again would double-count.  Use the PREDICTIVE
    // mixture (the posterior before this observation), which is the proper
    // one-step filtering quantity.
    if (outcome == 2 && cfg.silenceChannel) {
      double mix = 0;
      for (int t = 0; t < NTYPE; t++)
        mix += post[p][t] * (kind == 0 ? pNeverReply(T[t]) : pNeverProbe(T[t]));
      if (mix < 1e-3) mix = 1e-3;
      if (mix > 1.0) mix = 1.0;
      cell[p][S].llr += float(std::log(mix));
    }

    // Type posterior.  Marginalise the latent "p holds >= 1 card of S" with q:
    //   P(fast | tau) = q * fastRate      P(slow | tau) = q * slowRate
    //   P(never | tau) = 1 - q * (fastRate + slowRate)
    // because P(ask in S | holds nothing in S) = 0 exactly, by ask legality.
    for (int t = 0; t < NTYPE; t++) {
      double f = kind == 0 ? T[t].replyFast : T[t].probeFast;
      double s = kind == 0 ? T[t].replySlow : T[t].probeSlow;
      double lik = outcome == 0 ? q * f : (outcome == 1 ? q * s : 1.0 - q * (f + s));
      if (lik < 1e-4) lik = 1e-4;
      post[p][t] *= lik;
    }
    renorm(p);

    cell[p][S].n = uint16_t(std::min(65535, cell[p][S].n + 1));
    nSeat[p] = uint16_t(std::min(65535, nSeat[p] + 1));
  }

  // ---- event ingestion ---------------------------------------------------
  // MUST be called AFTER Knowledge::onEvent, so `k` is the post-event state.
  void onEvent(const Event& e, const Knowledge& k) {
    if (!cfg.enabled) return;
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      int S = e.decl.set;                       // the half-suit leaves play: censor
      for (int p = 0; p < NPLAY; p++) { cell[p][S].replyOpen = 0; cell[p][S].probeOpen = 0; }
      return;
    }
    if (e.kind != Kind::Ask && e.kind != Kind::Pass) return;

    int p = e.actor;
    int S = (e.kind == Kind::Ask) ? setOf(e.card) : -1;

    // (a) this is one of p's OWN turns: age every episode open against p.
    for (int s = 0; s < NSET; s++) {
      Cell& C = cell[p][s];
      bool acted = (s == S);
      if (C.replyOpen) {
        int el = C.replyTurns + 1;
        if (acted)                       { C.replyOpen = 0; resolve(p, s, 0, el <= cfg.windowFast ? 0 : 1, k); }
        else if (el >= cfg.windowSlow)   { C.replyOpen = 0; resolve(p, s, 0, 2, k); }
        else                             { C.replyTurns = uint8_t(el); }
      }
      if (C.probeOpen) {
        int el = C.probeTurns + 1;
        if (acted)                       { C.probeOpen = 0; resolve(p, s, 1, el <= cfg.windowFast ? 0 : 1, k); }
        else if (el >= cfg.windowSlow)   { C.probeOpen = 0; resolve(p, s, 1, 2, k); }
        else                             { C.probeTurns = uint8_t(el); }
      }
    }

    // (b) open the episodes this event creates.
    if (e.kind == Kind::Ask) {
      if (k.setActive[S] && k.handCount[p]) { cell[p][S].probeOpen = 1; cell[p][S].probeTurns = 0; }
      int t = e.target;
      if (k.setActive[S] && k.handCount[t]) { cell[t][S].replyOpen = 1; cell[t][S].replyTurns = 0; }
    }
  }

  // ---- the two outputs ---------------------------------------------------
  // z_M7(c, p): additive log-weight on the deal prior for "card c sits with p".
  // Indexed by (seat, half-suit) through llr and by CARD through `askable`, so it
  // is not a function of the seat alone and survives the capacity normalisation.
  double logTilt(const Knowledge& k, int c, int p, double weight) const {
    if (!cfg.enabled || !cfg.silenceChannel || weight == 0.0) return 0.0;
    int S = setOf(c);
    const Cell& C = cell[p][S];
    if (C.n == 0 || C.llr >= 0.0f) return 0.0;
    if (cfg.opportunity && !askable(k, p, c)) return 0.0;
    // MEAN-FIELD ALLOCATION.  `llr` is a log-odds shift on the DISJUNCTION
    // "p holds at least one card of S" -- the same event C5 asserts from the
    // other side.  Spreading it undivided over every card of S would move
    // P(p holds nothing in S) by roughly exp(m*llr) instead of exp(llr), so it is
    // divided by the number of cards that carry it.  (v0.4's theta makes exactly
    // this approximation without the division, which is one reason its fitted
    // value sits far below every empirically identified coefficient -- P3 §6.2.)
    int m = 0;
    for (int i = 0; i < SETSZ; i++) {
      int d = cardOf(S, i);
      if (k.owner[d] != UNKNOWN || !(k.mask[d] & (1u << p))) continue;
      if (cfg.opportunity && !askable(k, p, d)) continue;
      m++;
    }
    if (m < 1) m = 1;
    double shrink = double(C.n) / (double(C.n) + cfg.dataBias);   // Johanson & Bowling
    double z = weight * shrink * double(C.llr) / double(m);
    if (z < -cfg.tiltCap) z = -cfg.tiltCap;
    if (z > 0) z = 0;
    return z;
  }

  // Per-seat effective certificate weight, shrunk toward the population value.
  double thetaFor(int p, double baseTheta) const {
    if (!cfg.enabled || !cfg.thetaChannel) return baseTheta;
    const TypeProfile* T = types();
    double mul = 0;
    for (int t = 0; t < NTYPE; t++) mul += post[p][t] * T[t].thetaMul;
    double shrink = double(nSeat[p]) / (double(nSeat[p]) + cfg.dataBias);
    double m = 1.0 + shrink * (mul - 1.0);
    double hi = cfg.thetaMulCap, lo = 1.0 / cfg.thetaMulCap;
    if (m > hi) m = hi; else if (m < lo) m = lo;
    return baseTheta * m;
  }

  // Reporting only.
  int mapType(int p) const {
    int b = 0; for (int t = 1; t < NTYPE; t++) if (post[p][t] > post[p][b]) b = t; return b;
  }
};

// ------------------------------------------------------- the tilted posterior
// STRUCTURAL COPY of Belief::sinkhornDisj (belief.hpp:478-529).  The ONLY change
// is the initialiser: `kk.priorWeight(c, p, theta, phi)` becomes the same
// expression with a per-seat theta and the additive M7 tilt.  Everything after
// the initialiser -- the IPF sweeps, the C5 conditioning, the degenerate-case
// handling -- is byte-identical to the protected header, and `selfTest` below
// asserts that with the model disabled this function reproduces
// Belief::sinkhornDisj bit for bit.  belief.hpp is frozen so that v0.4 stays the
// reference opponent; duplicating the loop is the sanctioned way to extend it
// (the P3 archetypes did the same).
inline void fitTilted(Belief& b, const Knowledge& kk, int outer, int inner,
                      double theta, double phi, const OppModel* m, double weight) {
  double (*marg)[NPLAY] = b.marg;
  uint8_t q[NPLAY]; kk.capacities(q);
  int idx[NCARD], Q = 0;
  uint64_t u = kk.unresolved;
  while (u) { int c = __builtin_ctzll(u); u &= u - 1; idx[Q++] = c; }
  if (!Q) return;
  for (int i = 0; i < Q; i++) {
    int c = idx[i];
    for (int p = 0; p < NPLAY; p++) {
      if (!(kk.mask[c] & (1u << p))) { marg[c][p] = 0.0; continue; }
      // BOTH M7 channels are gated by `weight`, so that weight = 0 reproduces
      // v0.4's global prior exactly.  Gating only the silence tilt and letting
      // the per-seat certificate weight through would put M7 on the declaration
      // path even at declWeight = 0, which is precisely what P3 §6.3.5 forbids:
      // the tilt buys ask quality by accepting more over-confident marginals,
      // a good trade for asking and a bad one for the irreversible action.
      double th = theta;
      if (m && weight != 0.0) th = theta + weight * (m->thetaFor(p, theta) - theta);
      double w;
      if (th == 0 && phi == 0) w = 1.0;
      else {
        int S = setOf(c);
        double a = double(kk.askCount[p][S]);
        double other = double(kk.totalAsks[p]) - a;
        double z = th * a - phi * other;
        if (z > 2.6) z = 2.6; else if (z < -2.6) z = -2.6;
        w = std::exp(z);
      }
      if (m) { double t = m->logTilt(kk, c, p, weight); if (t != 0.0) w *= std::exp(t); }
      marg[c][p] = w;
    }
  }
  for (int o = 0; o < outer; o++) {
    for (int it = 0; it < inner; it++) {
      for (int i = 0; i < Q; i++) { double t = 0; int c = idx[i];
        for (int p = 0; p < NPLAY; p++) t += marg[c][p];
        if (t > 0) for (int p = 0; p < NPLAY; p++) marg[c][p] /= t; }
      double col[NPLAY] = {0,0,0,0,0,0};
      for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++) col[p] += marg[idx[i]][p];
      for (int p = 0; p < NPLAY; p++) { double sc = col[p] > 0 ? q[p] / col[p] : 0;
        for (int i = 0; i < Q; i++) marg[idx[i]][p] *= sc; }
    }
    for (int i = 0; i < Q; i++) { double t = 0; int c = idx[i];
      for (int p = 0; p < NPLAY; p++) t += marg[c][p];
      if (t > 0) for (int p = 0; p < NPLAY; p++) marg[c][p] /= t; }
    if (o + 1 == outer) break;
    for (const auto& d : kk.disj) {
      int A = d.player;
      double pNone = 1.0;
      uint64_t cc = d.cards;
      int list[SETSZ]; int mm2 = 0;
      while (cc) { int x = __builtin_ctzll(cc); cc &= cc - 1;
        if (!(kk.unresolved & bit(x))) continue;
        list[mm2++] = x; pNone *= (1.0 - marg[x][A]); }
      if (!mm2) continue;
      double pAny = 1.0 - pNone;
      if (pAny < 1e-9) {
        for (int j = 0; j < mm2; j++) { for (int p = 0; p < NPLAY; p++) marg[list[j]][p] *= .001;
          marg[list[j]][A] += 1.0 / mm2; }
        continue;
      }
      if (pNone < 1e-12) continue;
      for (int j = 0; j < mm2; j++) {
        int c = list[j];
        double denom = 1.0 - marg[c][A];
        double pNoneOther = denom > 1e-12 ? pNone / denom : 0.0;
        double newA = marg[c][A] / pAny;
        double scale = (1.0 - pNoneOther) / pAny;
        for (int p = 0; p < NPLAY; p++) if (p != A) marg[c][p] *= scale;
        marg[c][A] = newA;
        double t = 0; for (int p = 0; p < NPLAY; p++) t += marg[c][p];
        if (t > 0) for (int p = 0; p < NPLAY; p++) marg[c][p] /= t;
      }
    }
  }
}

// Tilted counterparts of Belief::jointSequential / jointSequentialMAP
// (belief.hpp:535-588).  With weight = 0 and m = nullptr they are exact
// reproductions; the declaration path calls them with cfg.m7DeclWeight, which
// defaults to 0 (P3 §6.3.5).
inline double jointSequentialTilted(const Knowledge& kk, const int* cards, const int* players, int n,
                                    int outer, int inner, double theta, double phi,
                                    const OppModel* m, double weight) {
  Knowledge tmp = kk;
  double pr = 1.0;
  Belief scratchB;
  for (int i = 0; i < n; i++) {
    int c = cards[i], p = players[i];
    if (tmp.owner[c] < NPLAY) { if (tmp.owner[c] != p) return 0.0; continue; }
    if (!(tmp.mask[c] & (1u << p))) return 0.0;
    fitTilted(scratchB, tmp, outer, inner, theta, phi, m, weight);
    double v = scratchB.marg[c][p];
    if (v <= 0) return 0.0;
    pr *= v;
    if (pr < 1e-9) return pr;
    tmp.setOwner(c, p);
    tmp.propagateCapacity();
  }
  return pr;
}

inline double jointSequentialMAPTilted(const Knowledge& kk, const int* cards, int n, int teamMask,
                                       int* outPlayers, int outerIt, int innerIt,
                                       double theta, double phi, const OppModel* m, double weight) {
  Knowledge tmp = kk;
  Belief scratchB;
  double pr = 1.0;
  for (int i = 0; i < n; i++) {
    int c = cards[i];
    if (tmp.owner[c] < NPLAY) {
      if (!(teamMask & (1u << tmp.owner[c]))) return 0.0;
      outPlayers[i] = tmp.owner[c];
      continue;
    }
    if (!(tmp.mask[c] & teamMask)) return 0.0;
    fitTilted(scratchB, tmp, outerIt, innerIt, theta, phi, m, weight);
    int best = -1; double bestP = -1;
    for (int qq = 0; qq < NPLAY; qq++) {
      if (!(teamMask & (1u << qq))) continue;
      double v = scratchB.marg[c][qq];
      if (v > bestP) { bestP = v; best = qq; }
    }
    if (best < 0 || bestP <= 0) return 0.0;
    outPlayers[i] = best;
    pr *= bestP;
    if (pr < 1e-9) return pr;
    tmp.setOwner(c, best);
    tmp.propagateCapacity();
  }
  return pr;
}

// Divergence guard.  belief.hpp is frozen; this asserts that the structural copy
// above has not drifted from it.  Returns the max absolute marginal difference
// between fitTilted(model = off) and Belief::sinkhornDisj on the given state; it
// must be exactly 0.0.  Wired to `fish m7check` by M7.patch.
inline double selfTest(const Knowledge& kk, int outer, int inner, double theta, double phi) {
  Belief a, bb;
  for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) { a.marg[c][p] = 0; bb.marg[c][p] = 0; }
  for (int c = 0; c < NCARD; c++) if (kk.owner[c] < NPLAY) { a.marg[c][kk.owner[c]] = 1; bb.marg[c][kk.owner[c]] = 1; }
  a.sinkhornDisj(kk, outer, inner, theta, phi);
  fitTilted(bb, kk, outer, inner, theta, phi, nullptr, 0.0);
  double mx = 0;
  for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) {
    double d = std::fabs(a.marg[c][p] - bb.marg[c][p]);
    if (d > mx) mx = d;
  }
  return mx;
}

} // namespace m7
} // namespace fish
