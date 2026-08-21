// Population-robust weight fitting.
//
// The objective is deliberately NOT mean win rate against a fixed pool: a policy
// that averages well but collapses against one playstyle is not what we want.
// We optimise a soft minimum over the opponent panel,
//     score(w) = -(1/beta) * log sum_o exp(-beta * winRate_o(w)),
// which converges to min_o winRate_o as beta grows, and report the full profile.
// Common random numbers (identical seed banks for every candidate within a
// generation) make the comparisons paired and cut the sampling noise sharply.
#pragma once
#include "arena.hpp"
#include <numeric>
#include <cstdio>

namespace fish {

struct TuneSpec {
  std::vector<std::string> panel;
  int gamesPerOpponent = 250;
  int population = 24;
  int elite = 6;
  int generations = 40;
  double beta = 10.0;
  double sigma0 = 0.6;
  double sigmaFloor = 0.03;
  double smoothing = 0.6;
  uint64_t seed = 424242;
  Rules rules;
  int threads = 0;
  std::string baseSpec = "v04";
  std::vector<double> lo, hi;
};

inline std::string weightSpec(const std::string& base, const std::vector<double>& w) {
  std::string s = base + (w.size() > 18 ? ":allparams=" : ":weights=");
  char buf[32];
  for (size_t i = 0; i < w.size(); i++) {
    snprintf(buf, sizeof(buf), "%.5f", w[i]);
    s += buf;
    if (i + 1 < w.size()) s += "|";
  }
  return s;
}

struct Evaluation { double score = 0; std::vector<double> winRates; };

inline Evaluation evaluateCandidate(const TuneSpec& sp, const std::vector<double>& w, uint64_t genSeed) {
  Evaluation e;
  std::string spec = weightSpec(sp.baseSpec, w);
  double acc = 0;
  for (size_t i = 0; i < sp.panel.size(); i++) {
    MatchConfig mc;
    mc.specA = spec; mc.specB = sp.panel[i];
    mc.games = sp.gamesPerOpponent;
    mc.seed = mixSeed(genSeed, i * 7919 + 13);   // common random numbers
    mc.rules = sp.rules;
    mc.threads = sp.threads;
    MatchStats st = runMatch(mc);
    double wr = double(st.winsA) / double(std::max(1, st.games * 2));
    e.winRates.push_back(wr);
    acc += std::exp(-sp.beta * wr);
  }
  e.score = -std::log(acc) / sp.beta;
  return e;
}

// Cross-entropy method with a diagonal Gaussian: robust under evaluation noise,
// no gradient, and it degrades gracefully when the objective is flat.
inline std::vector<double> tune(TuneSpec sp, std::vector<double> mu, FILE* out) {
  size_t D = mu.size();
  std::vector<double> sigma(D, sp.sigma0);
  Rng rng(sp.seed);
  std::vector<double> best = mu;
  double bestScore = -1e9;
  for (int g = 0; g < sp.generations; g++) {
    uint64_t genSeed = mixSeed(sp.seed, uint64_t(g) * 104729 + 5);
    std::vector<std::vector<double>> cand(sp.population, std::vector<double>(D));
    std::vector<Evaluation> evals(sp.population);
    for (int i = 0; i < sp.population; i++) {
      for (size_t d = 0; d < D; d++) {
        double u1 = std::max(1e-12, rng.uni()), u2 = rng.uni();
        double z = std::sqrt(-2 * std::log(u1)) * std::cos(2 * M_PI * u2);
        double v = mu[d] + sigma[d] * z;
        if (!sp.lo.empty()) v = std::min(sp.hi[d], std::max(sp.lo[d], v));
        cand[i][d] = v;
      }
      if (i == 0) cand[i] = mu;                   // always evaluate the incumbent
      evals[i] = evaluateCandidate(sp, cand[i], genSeed);
    }
    std::vector<int> order(sp.population);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return evals[a].score > evals[b].score; });
    std::vector<double> newMu(D, 0), newSigma(D, 0);
    for (int e = 0; e < sp.elite; e++) for (size_t d = 0; d < D; d++) newMu[d] += cand[order[e]][d] / sp.elite;
    for (int e = 0; e < sp.elite; e++) for (size_t d = 0; d < D; d++) {
      double diff = cand[order[e]][d] - newMu[d];
      newSigma[d] += diff * diff / sp.elite;
    }
    for (size_t d = 0; d < D; d++) {
      mu[d] = sp.smoothing * newMu[d] + (1 - sp.smoothing) * mu[d];
      sigma[d] = std::max(sp.sigmaFloor, sp.smoothing * std::sqrt(newSigma[d]) + (1 - sp.smoothing) * sigma[d]);
    }
    if (evals[order[0]].score > bestScore) { bestScore = evals[order[0]].score; best = cand[order[0]]; }
    fprintf(out, "{\"gen\":%d,\"bestScore\":%.4f,\"incumbentScore\":%.4f,\"winRates\":[", g, evals[order[0]].score, evals[0].score);
    for (size_t i = 0; i < evals[order[0]].winRates.size(); i++)
      fprintf(out, "%s%.4f", i ? "," : "", evals[order[0]].winRates[i]);
    fprintf(out, "],\"mu\":[");
    for (size_t d = 0; d < D; d++) fprintf(out, "%s%.4f", d ? "," : "", mu[d]);
    fprintf(out, "]}\n");
    fflush(out);
  }
  Evaluation fin = evaluateCandidate(sp, mu, mixSeed(sp.seed, 999983));
  Evaluation fb = evaluateCandidate(sp, best, mixSeed(sp.seed, 999983));
  fprintf(out, "{\"final\":\"mu\",\"score\":%.4f}\n{\"final\":\"best\",\"score\":%.4f}\n", fin.score, fb.score);
  return fin.score >= fb.score ? mu : best;
}

} // namespace fish
