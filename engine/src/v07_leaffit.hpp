// FishBot v0.7 phase 3, candidate K1 -- fitting and diagnosing the search leaf.
//
// THE QUESTION THIS FILE EXISTS TO ANSWER.  The v0.6 conclusion made re-opening
// test-time search conditional on rebuilding the leaf evaluator, on the grounds
// that the present one "is algebraically close to a rescaling of the hit
// probability and cannot support a depth-limited search"
// (SUBOPTIMALITY-LEDGER.md L9).  Phase 1 refuted that in the endgame regime
// (depth=12,maxq=26 holds +2.19 over v06 with v0.6's OWN leaf) and confirmed it
// full-game (+0.08).  So the conditional binds somewhere, and the cheap way to
// find out where is to look at what the leaf is actually being asked to do.
//
// WHAT THE PAIRED LCB RULE ACTUALLY CONSUMES.  V06Agent::chooseAsk compares
// candidate r against candidate 0 on the SAME determinization d and takes the
// mean of val[r][d] - val[0][d].  Any part of the leaf value that is constant
// across candidates at a fixed determinization cancels exactly.  So a leaf that
// predicts the LEVEL of the continuation value beautifully and is flat across a
// decision's candidates contributes nothing at all -- which is the leaf-side
// restatement of L9's "7 of 16 value features are exactly constant across the
// candidate set at 100.00% of decisions".  This file therefore reports the
// between-candidate R^2, grouped by (decision, determinization), SEPARATELY
// from the overall R^2, and treats the former as the one that decides.
//
// One place the level does still matter: with `depth` set, a rollout that ends
// naturally before the cut returns the true terminal differential while a
// truncated one returns the leaf, so the two are mixed inside one paired
// difference.  A leaf on the wrong SCALE corrupts those comparisons even if its
// direction is right.  Hence both a level-calibrated fit and a difference-optimal
// fit are produced, and both are measured in play.
#pragma once
#include "v07_leaf.hpp"
#include "arena.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>

namespace fish {
namespace leaffit {

struct Fit {
  double w[NLEAF] = {0};
  std::string tag;
};

inline std::string specOf(const double* w) {
  char buf[64]; std::string s;
  for (int i = 0; i < NLEAF; i++) {
    snprintf(buf, sizeof buf, "%.10g", w[i]);
    if (i) s += "|";
    s += buf;
  }
  return s;
}

// 13x13 symmetric solve, Gaussian elimination with partial pivoting.  Small and
// explicit on purpose: the corpus has no linear-algebra dependency and this is
// the only place that needs one.
inline bool solveSym(double A[NLEAF][NLEAF], double b[NLEAF], double* out) {
  int n = NLEAF;
  for (int c = 0; c < n; c++) {
    int piv = c; double best = std::fabs(A[c][c]);
    for (int r = c + 1; r < n; r++) if (std::fabs(A[r][c]) > best) { best = std::fabs(A[r][c]); piv = r; }
    if (best < 1e-14) return false;
    if (piv != c) { for (int j = 0; j < n; j++) std::swap(A[c][j], A[piv][j]); std::swap(b[c], b[piv]); }
    for (int r = c + 1; r < n; r++) {
      double f = A[r][c] / A[c][c];
      if (f == 0) continue;
      for (int j = c; j < n; j++) A[r][j] -= f * A[c][j];
      b[r] -= f * b[c];
    }
  }
  for (int r = n - 1; r >= 0; r--) {
    double s = b[r];
    for (int j = r + 1; j < n; j++) s -= A[r][j] * out[j];
    out[r] = s / A[r][r];
  }
  return true;
}

// Ridge on standardised columns, unstandardised on the way out.  `useCols`
// selects the features admitted to the fit; a column with zero variance is
// dropped whatever the caller says.
inline Fit ridgeFit(const std::vector<const double*>& X, const std::vector<double>& y,
                    double lam, bool intercept, const bool* useCols) {
  Fit F;
  const size_t n = X.size();
  if (n < NLEAF + 2) return F;
  double mu[NLEAF] = {0}, sd[NLEAF] = {0};
  for (size_t i = 0; i < n; i++) for (int j = 0; j < NLEAF; j++) mu[j] += X[i][j];
  for (int j = 0; j < NLEAF; j++) mu[j] /= double(n);
  for (size_t i = 0; i < n; i++) for (int j = 0; j < NLEAF; j++) { double d = X[i][j] - mu[j]; sd[j] += d * d; }
  for (int j = 0; j < NLEAF; j++) sd[j] = std::sqrt(sd[j] / double(n));
  bool on[NLEAF];
  for (int j = 0; j < NLEAF; j++) on[j] = (useCols ? useCols[j] : true) && sd[j] > 1e-12 && j != 0;
  if (!intercept) { for (int j = 0; j < NLEAF; j++) mu[j] = 0.0; }
  double ybar = 0; if (intercept) { for (double v : y) ybar += v; ybar /= double(n); }

  double A[NLEAF][NLEAF] = {{0}}, b[NLEAF] = {0}, w[NLEAF] = {0};
  for (size_t i = 0; i < n; i++) {
    double z[NLEAF];
    for (int j = 0; j < NLEAF; j++) z[j] = on[j] ? (X[i][j] - mu[j]) / sd[j] : 0.0;
    double yc = y[i] - ybar;
    for (int j = 0; j < NLEAF; j++) if (on[j]) {
      b[j] += z[j] * yc;
      for (int k2 = 0; k2 < NLEAF; k2++) if (on[k2]) A[j][k2] += z[j] * z[k2];
    }
  }
  for (int j = 0; j < NLEAF; j++) A[j][j] += (on[j] ? lam * double(n) : 1.0);
  if (!solveSym(A, b, w)) return F;
  for (int j = 0; j < NLEAF; j++) F.w[j] = on[j] ? w[j] / sd[j] : 0.0;
  if (intercept) { double c = ybar; for (int j = 1; j < NLEAF; j++) c -= F.w[j] * mu[j]; F.w[0] = c; }
  else F.w[0] = 0.0;
  return F;
}

inline double predict(const double* w, const double* f) {
  double v = 0; for (int j = 0; j < NLEAF; j++) v += w[j] * f[j]; return v;
}

struct R2 {
  double r2 = 0;       // 1 - SSres/SStot, the predictor taken at its own scale
  double corr2 = 0;    // squared correlation, i.e. the best affine rescaling
  double slope = 1;    // regression of y on the prediction
  size_t n = 0;
};

inline R2 scoreLevel(const std::vector<const double*>& X, const std::vector<double>& y, const double* w) {
  R2 r; r.n = X.size(); if (r.n < 3) return r;
  double sy = 0, sp = 0;
  std::vector<double> p(r.n);
  for (size_t i = 0; i < r.n; i++) { p[i] = predict(w, X[i]); sp += p[i]; sy += y[i]; }
  double my = sy / double(r.n), mp = sp / double(r.n);
  double sst = 0, ssr = 0, cpy = 0, cpp = 0;
  for (size_t i = 0; i < r.n; i++) {
    double dy = y[i] - my, dp = p[i] - mp;
    sst += dy * dy; ssr += (y[i] - p[i]) * (y[i] - p[i]);
    cpy += dp * dy; cpp += dp * dp;
  }
  r.r2 = sst > 0 ? 1.0 - ssr / sst : 0.0;
  r.corr2 = (sst > 0 && cpp > 0) ? (cpy * cpy) / (sst * cpp) : 0.0;
  r.slope = cpp > 0 ? cpy / cpp : 0.0;
  return r;
}

// The between-candidate score.  Rows are grouped by (decision, determinization);
// within a group, both the prediction and the outcome are demeaned across the
// candidates.  That is exactly the quantity `val[r][d] - val[0][d]` is built
// from, up to a choice of baseline.  Groups of size 1 carry no information and
// are dropped.
struct Grouped {
  std::vector<std::pair<size_t, size_t>> spans;   // [begin,end) into a sorted index
  std::vector<size_t> idx;
};
inline Grouped groupByDecisionDet(const std::vector<LeafSample>& S) {
  Grouped g;
  g.idx.resize(S.size());
  for (size_t i = 0; i < S.size(); i++) g.idx[i] = i;
  std::sort(g.idx.begin(), g.idx.end(), [&](size_t a, size_t b) {
    if (S[a].did != S[b].did) return S[a].did < S[b].did;
    if (S[a].det != S[b].det) return S[a].det < S[b].det;
    return S[a].cand < S[b].cand;
  });
  size_t i = 0;
  while (i < g.idx.size()) {
    size_t j = i + 1;
    while (j < g.idx.size() && S[g.idx[j]].did == S[g.idx[i]].did && S[g.idx[j]].det == S[g.idx[i]].det) j++;
    if (j - i >= 2) g.spans.push_back({i, j});
    i = j;
  }
  return g;
}

inline R2 scoreBetween(const std::vector<LeafSample>& S, const Grouped& g, const double* w) {
  R2 r;
  double sst = 0, ssr = 0, cpy = 0, cpp = 0; size_t n = 0;
  for (auto& sp : g.spans) {
    size_t m = sp.second - sp.first;
    double my = 0, mp = 0;
    for (size_t t = sp.first; t < sp.second; t++) { const LeafSample& s = S[g.idx[t]]; my += s.y; mp += predict(w, s.f); }
    my /= double(m); mp /= double(m);
    for (size_t t = sp.first; t < sp.second; t++) {
      const LeafSample& s = S[g.idx[t]];
      double dy = s.y - my, dp = predict(w, s.f) - mp;
      sst += dy * dy; ssr += (dy - dp) * (dy - dp);
      cpy += dp * dy; cpp += dp * dp;
      n++;
    }
  }
  r.n = n;
  r.r2 = sst > 0 ? 1.0 - ssr / sst : 0.0;
  r.corr2 = (sst > 0 && cpp > 0) ? (cpy * cpy) / (sst * cpp) : 0.0;
  r.slope = cpp > 0 ? cpy / cpp : 0.0;
  return r;
}

// The direct analogue of ledger L9's headline statistic, computed at the LEAF
// rather than at the ask-score: in what fraction of (decision, determinization)
// groups is each feature exactly constant across the candidates?
inline void constancy(const std::vector<LeafSample>& S, const Grouped& g, double* frac) {
  long long tot = 0; long long cnt[NLEAF] = {0};
  for (auto& sp : g.spans) {
    tot++;
    for (int j = 0; j < NLEAF; j++) {
      double v0 = S[g.idx[sp.first]].f[j]; bool same = true;
      for (size_t t = sp.first + 1; t < sp.second; t++) if (S[g.idx[t]].f[j] != v0) { same = false; break; }
      if (same) cnt[j]++;
    }
  }
  for (int j = 0; j < NLEAF; j++) frac[j] = tot ? double(cnt[j]) / double(tot) : 0.0;
}

// Fit on the between-candidate contrasts, then rescale the resulting direction so
// its LEVEL regression on the outcome has slope 1.  Direction from the contrasts
// (the only part the LCB rule sees), scale from the level (so it composes with
// the untruncated rollouts inside the same paired difference).
inline Fit diffFit(const std::vector<LeafSample>& S, const Grouped& g, double lam, const bool* useCols) {
  std::vector<std::array<double, NLEAF>> rows;
  std::vector<double> ys;
  rows.reserve(g.idx.size()); ys.reserve(g.idx.size());
  for (auto& sp : g.spans) {
    size_t m = sp.second - sp.first;
    double mf[NLEAF] = {0}, my = 0;
    for (size_t t = sp.first; t < sp.second; t++) {
      const LeafSample& s = S[g.idx[t]];
      for (int j = 0; j < NLEAF; j++) mf[j] += s.f[j];
      my += s.y;
    }
    for (int j = 0; j < NLEAF; j++) mf[j] /= double(m);
    my /= double(m);
    for (size_t t = sp.first; t < sp.second; t++) {
      const LeafSample& s = S[g.idx[t]];
      std::array<double, NLEAF> r{};
      for (int j = 0; j < NLEAF; j++) r[j] = s.f[j] - mf[j];
      rows.push_back(r); ys.push_back(s.y - my);
    }
  }
  std::vector<const double*> X; X.reserve(rows.size());
  for (auto& r : rows) X.push_back(r.data());
  Fit F = ridgeFit(X, ys, lam, false, useCols);
  F.tag = "diff";
  return F;
}

} // namespace leaffit
} // namespace fish
