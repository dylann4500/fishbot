// FishBot v0.7 -- the leaf-evaluator interface.
//
// THE INHERITED CONDITIONAL.  The v0.6 conclusion says test-time search "should
// be re-opened only after the leaf evaluator is rebuilt, because the present one
// is algebraically close to a rescaling of the hit probability and cannot
// support a depth-limited search" (paper/sections_v06/14-conclusion.tex).  The
// measured basis is precise: over 26,417 shipped ask decisions seven of sixteen
// value features are exactly constant across the candidate set at 100.00% of
// decisions, the mean R^2 of the value term on p is 0.84034, and `declareByValue`
// is algebraically a threshold on pAlloc at 100.0000% verdict agreement
// (SUBOPTIMALITY-LEDGER.md L9).
//
// Phase 1 builds the INTERFACE and the TRUNCATION; the evaluator itself is
// phase 3's problem.  Two properties are what make this file worth having:
//
//   (1) The default evaluator is bit-for-bit the v0.6 `leafValue` it replaces,
//       so every existing search number stays valid and the refactor is
//       falsifiable by an identity control rather than by inspection.
//   (2) The interface is FEATURE-FIRST and BATCHED.  A rollout that hits a
//       depth cut writes a fixed-width feature row and returns; the caller
//       evaluates all of a decision's leaves in one call.  A scalar evaluator
//       loses nothing by this, and it is the only shape in which a learned
//       evaluator is affordable at all -- the alternative, one forward pass per
//       leaf, is what makes depth-limited search slower than playing the game
//       out.
//
// A NOTE ON WHAT THE LEAF SEES.  Inside a determinization the deal is fixed, so
// the leaf state is fully observed and the features below may read hands.  This
// is not clairvoyance leaking into the policy: the determinization is drawn from
// the searcher's own posterior, and the value is averaged over the draw.  It is
// the same footing the v0.6 `leafValue` already stood on.
#pragma once
#include "game.hpp"
#include <memory>
#include <string>
#include <sstream>
#include <fstream>

namespace fish {

static constexpr int NLEAF = 13;

inline const char* leafFeatureName(int i) {
  static const char* n[NLEAF] = {
    "bias",
    "scoreDiff",            // signed half-suits already scored, for `team`
    "control",              // sum over live half-suits of (2*frac_held - 1)
    "sharpControl",         // the same, cubed: rewards decided half-suits
    "lockedDiff",           // live half-suits held outright by us minus by them
    "activeSets",           // how many half-suits are still live
    "sideToMove",           // +1 if `team` holds the turn
    "cardDiff",             // (our cards - their cards) / deck size
    "turnXControl",         // interaction: the turn is worth more when contested
    "minFriendlyHand",      // the smallest live hand on our team, normalised
    "nearComplete",         // our live half-suits at >= 5/6, minus theirs
    "contestedMass",        // live half-suits with frac in (1/6, 5/6), normalised
    "materialV06"           // the v0.6 leafValue, accumulated in v0.6's own order
  };
  return n[i];
}

// Extract the leaf feature row for `team` from a fully-determinized state.
// The row is DOUBLE, not float.  A float row loses about 1e-8 relative
// precision on the control term, which is enough to flip the search's
// lower-confidence-bound comparison at a handful of decisions and therefore to
// break the identity control against v0.6's own `leafValue`.  A learned
// evaluator that wants float can down-convert at its own boundary; the harness
// will not pay for its convenience in reproducibility.
inline void leafFeatures(const GameState& g, int team, double lambda, double* f) {
  const int nSets = g.pub.rules.deckSets;
  double control = 0, sharp = 0, locked = 0, active = 0, nearC = 0, contested = 0;
  // f[12] is accumulated HERE, in v0.6's order -- score first, then one
  // `+= lambda * (2*frac - 1)` per live half-suit in half-suit order.  Floating
  // point addition is not associative, so `score + lambda*(c0+c1+...)` is not
  // bit-for-bit `((score + lambda*c0) + lambda*c1) + ...`, and the difference is
  // large enough to flip the search's LCB comparison at a few decisions per
  // hundred games.  Reproducing the order is what makes MaterialLeaf an exact
  // refactor of v0.6's leafValue rather than a numerically-close one; the
  // identity control that caught this is in `fish v7leafcheck`.
  double matV06 = double(g.pub.score[team]) - double(g.pub.score[1 - team]);
  for (int s = 0; s < NSET; s++) {
    if (!g.pub.setActive[s]) continue;
    active += 1;
    int a = 0;
    for (int p = team; p < NPLAY; p += 2) a += popcount64(g.hand[p] & setMask(s));
    double frac = double(a) / double(SETSZ);
    double c = 2.0 * frac - 1.0;
    matV06 += lambda * (2.0 * double(a) / double(SETSZ) - 1.0);
    control += c;
    sharp += c * c * c;
    if (a == SETSZ) locked += 1; else if (a == 0) locked -= 1;
    if (frac >= 5.0 / 6.0) nearC += 1; else if (frac <= 1.0 / 6.0) nearC -= 1;
    if (frac > 1.0 / 6.0 && frac < 5.0 / 6.0) contested += 1;
  }
  int mine = 0, theirs = 0, minFriendly = 99;
  for (int p = 0; p < NPLAY; p++) {
    int h = popcount64(g.hand[p]);
    if (teamOf(p) == team) { mine += h; if (h > 0 && h < minFriendly) minFriendly = h; }
    else theirs += h;
  }
  if (minFriendly == 99) minFriendly = 0;
  double side = (teamOf(g.pub.turn) == team) ? 1.0 : -1.0;
  double denom = double(std::max(1, nSets * SETSZ));
  f[0]  = 1.0;
  f[1]  = double(g.pub.score[team]) - double(g.pub.score[1 - team]);
  f[2]  = control;
  f[3]  = sharp;
  f[4]  = locked;
  f[5]  = active;
  f[6]  = side;
  f[7]  = (mine - theirs) / denom;
  f[8]  = side * contested;
  f[9]  = double(minFriendly) / double(SETSZ);
  f[10] = nearC;
  f[11] = contested;
  f[12] = matV06;
}

struct LeafEvaluator {
  virtual ~LeafEvaluator() = default;
  virtual const char* name() const = 0;
  virtual double value(const double* f) const = 0;
  // One call per decision rather than one per leaf.  A scalar evaluator loops;
  // a learned one batches.  `n` rows of NLEAF floats each, contiguous.
  virtual void valueBatch(const double* f, int n, double* out) const {
    for (int i = 0; i < n; i++) out[i] = value(f + size_t(i) * NLEAF);
  }
  // Reported in the throughput table so a configuration's cost is attributable.
  mutable long long calls = 0, batches = 0;
};

// The v0.6 evaluator, exactly.  It reads f[12], which the extractor accumulated
// in v0.6's own order using the engine's own `leafLambda`; it does NOT carry a
// lambda of its own, because two sources for one constant is how the corpus
// acquired four macro mis-bindings.
struct MaterialLeaf : LeafEvaluator {
  const char* name() const override { return "material"; }
  double value(const double* f) const override { calls++; return f[12]; }
};

// The degenerate control: half-suits already scored and nothing else.  Named
// because the ledger's warning about a depth-limited search with a
// one-dimensional leaf (Brown, Sandholm & Amos) applies most sharply here, and a
// configuration that measures the same under `score` as under `material` is one
// whose depth cut is not doing any work.
struct ScoreLeaf : LeafEvaluator {
  const char* name() const override { return "score"; }
  double value(const double* f) const override { calls++; return f[1]; }
};

// A linear evaluator over the whole row.  This is the slot phase 3 fills: fit
// the coefficients with the candidate-varying component orthogonalised against
// p, and the gate the v0.6 conclusion names is discharged by a number rather
// than by an argument.
struct LinearLeaf : LeafEvaluator {
  // Default: 1*scoreDiff + 1*control, i.e. MaterialLeaf at lambda 1 up to
  // summation order.  f[12] carries weight 0 -- a fitted evaluator that wanted
  // the v0.6 value as a feature would set it, but it is not a free input.
  double w[NLEAF] = {0,1,1,0,0,0,0,0,0,0,0,0,0};
  std::string tag = "linear";
  const char* name() const override { return tag.c_str(); }
  double value(const double* f) const override {
    calls++;
    double v = 0;
    for (int i = 0; i < NLEAF; i++) v += w[i] * f[i];
    return v;
  }
  void valueBatch(const double* f, int n, double* out) const override {
    batches++;
    for (int i = 0; i < n; i++) {
      const double* r = f + size_t(i) * NLEAF;
      double v = 0;
      for (int j = 0; j < NLEAF; j++) v += w[j] * r[j];
      out[i] = v;
    }
    calls += n;
  }
};

// spec grammar:  material            -> the v0.6 value at the engine's leafLambda
//                score
//                linear@w0|w1|...|w11
//                linear@file:<path>   (whitespace- or pipe-separated coefficients)
inline std::unique_ptr<LeafEvaluator> makeLeafEvaluator(const std::string& spec, double lambda) {
  auto at = spec.find('@');
  std::string base = at == std::string::npos ? spec : spec.substr(0, at);
  std::string arg = at == std::string::npos ? "" : spec.substr(at + 1);
  (void)lambda;
  if (base.empty() || base == "material") return std::make_unique<MaterialLeaf>();
  if (base == "score") return std::make_unique<ScoreLeaf>();
  if (base == "linear") {
    auto ll = std::make_unique<LinearLeaf>();
    std::string body = arg;
    if (body.rfind("file:", 0) == 0) {
      std::ifstream in(body.substr(5));
      body.clear();
      std::string tok;
      while (in >> tok) { if (!body.empty()) body += "|"; body += tok; }
      ll->tag = "linear@file";
    }
    if (!body.empty()) {
      std::stringstream ss(body); std::string t; int i = 0;
      for (int j = 0; j < NLEAF; j++) ll->w[j] = 0;
      while (std::getline(ss, t, '|') && i < NLEAF) ll->w[i++] = atof(t.c_str());
    }
    return ll;
  }
  fprintf(stderr, "fish: unknown leaf evaluator '%s'\n", base.c_str());
  std::exit(2);
}

} // namespace fish
