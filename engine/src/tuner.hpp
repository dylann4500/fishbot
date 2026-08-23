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

// v0.6 E4.  Four defects in the v0.4/v0.5 fitting harness, each measured:
//
//  (1) ONE sigma for every coordinate.  sigma0 = 0.6 was applied to all 34
//      coordinates whose ranges span 0.1 to 40, so at generation 0 93.4% of
//      `declareMargin` proposals landed on a clamp bound while `valueWeight`
//      moved 1.5% of its range: bang-bang on the bounded knobs, frozen on the
//      unbounded ones (research/v06/notes/R4).  `--sigmaparams` was parsed at
//      main.cpp:224 and never referenced.  Fixed: per-coordinate sigma,
//      defaulting to sigmaRel * (hi - lo).
//
//  (2) UNPAIRED scoring.  Candidates were compared on absolute win rate against
//      a common seed bank.  Common random numbers pair the DEAL but not the
//      comparison, and the measured width of an unpaired one-arm interval is
//      1.042 pt against 5.66 pt paired-vs-unpaired on identical compute.  Fixed:
//      score every candidate as its paired per-deal margin over the incumbent,
//      which is evaluated once per generation and reused.
//
//  (3) The objective was documented as a soft MINIMUM and is a weighted mean:
//      at beta = 10 over v0.4's shipped profile the max/min gradient weight ratio
//      is 1.88 and the "minimum" sits 11.2 points above min(r).  Fixed: an
//      explicit dispatch over {softmin, min, mean, regret, minimaxregret}.
//
//  (4) The whole v0.5 fit was indistinguishable from sampling noise (OLS slope
//      +0.00049/generation, se 0.00031, t = 1.60 over 40 generations).  The
//      per-generation diagnostics below exist so that this is visible while a
//      fit is running rather than two studies later.
enum class TuneObjective { SoftMin, Min, Mean, Regret, MinimaxRegret };

struct TuneSpec {
  std::vector<std::string> panel;
  int gamesPerOpponent = 250;
  int rotations = 2;
  int population = 24;
  int elite = 6;
  int generations = 40;
  double beta = 10.0;
  double sigma0 = 0.6;          // legacy scalar; used only when sigmaRel <= 0
  double sigmaRel = 0.15;       // per-coordinate sigma as a fraction of (hi - lo)
  std::vector<double> sigmaVec; // explicit per-coordinate sigma, overrides both
  double sigmaFloor = 0.03;
  double smoothing = 0.6;
  uint64_t seed = 424242;
  Rules rules;
  int threads = 0;
  std::string baseSpec = "v04";
  std::string pairSpec = "";    // incumbent for the paired objective; "" = the CEM mean
  TuneObjective objective = TuneObjective::SoftMin;
  bool paired = false;
  std::vector<double> lo, hi;
};

inline std::string weightSpec(const std::string& base, const std::vector<double>& w) {
  // Derive the switch from NFEAT.  Hard-coding 18 here is the same class of
  // defect that cost v0.4 a whole fitting round when NFEAT grew to 20.
  std::string s = base + (w.size() > size_t(NFEAT) ? ":allparams=" : ":weights=");
  char buf[32];
  for (size_t i = 0; i < w.size(); i++) {
    snprintf(buf, sizeof(buf), "%.5f", w[i]);
    s += buf;
    if (i + 1 < w.size()) s += "|";
  }
  return s;
}

struct Evaluation {
  double score = 0;
  std::vector<double> winRates;              // absolute, or paired margin when sp.paired
  std::vector<std::vector<uint8_t>> paired;  // per-deal A-wins, per panel member
};

// Combine a per-opponent profile into one scalar.  `ref` is the per-opponent best
// achieved anywhere in this generation; only the regret objectives read it.
inline double combine(const TuneSpec& sp, const std::vector<double>& r,
                      const std::vector<double>* ref) {
  if (r.empty()) return 0;
  switch (sp.objective) {
    case TuneObjective::Min: {
      double m = r[0]; for (double v : r) m = std::min(m, v); return m; }
    case TuneObjective::Mean: {
      double s2 = 0; for (double v : r) s2 += v; return s2 / double(r.size()); }
    case TuneObjective::Regret: {
      double s2 = 0;
      for (size_t i = 0; i < r.size(); i++) s2 += ((ref && i < ref->size()) ? (*ref)[i] : r[i]) - r[i];
      return -s2 / double(r.size()); }
    case TuneObjective::MinimaxRegret: {
      double worst = 0;
      for (size_t i = 0; i < r.size(); i++)
        worst = std::max(worst, ((ref && i < ref->size()) ? (*ref)[i] : r[i]) - r[i]);
      return -worst; }
    case TuneObjective::SoftMin:
    default: {
      double acc = 0; for (double v : r) acc += std::exp(-sp.beta * v);
      return -std::log(acc) / sp.beta; }
  }
}

inline Evaluation evaluateCandidate(const TuneSpec& sp, const std::vector<double>& w, uint64_t genSeed) {
  Evaluation e;
  std::string spec = weightSpec(sp.baseSpec, w);
  for (size_t i = 0; i < sp.panel.size(); i++) {
    MatchConfig mc;
    mc.specA = spec; mc.specB = sp.panel[i];
    mc.games = sp.gamesPerOpponent;
    mc.rotations = sp.rotations;
    mc.seed = mixSeed(genSeed, i * 7919 + 13);   // common random numbers
    mc.rules = sp.rules;
    mc.threads = sp.threads;
    MatchStats st = runMatch(mc);
    // games * rotations, not games * 2: fitting at --rotations=6 silently
    // reported a third of the true win rate.
    double wr = double(st.winsA) / double(std::max(1, st.games * std::max(1, mc.rotations)));
    e.winRates.push_back(wr);
    if (sp.paired) e.paired.push_back(st.paired);
  }
  e.score = combine(sp, e.winRates, nullptr);
  return e;
}

// Cross-entropy method with a diagonal Gaussian: robust under evaluation noise,
// no gradient, and it degrades gracefully when the objective is flat.
inline std::vector<double> tune(TuneSpec sp, std::vector<double> mu, FILE* out) {
  size_t D = mu.size();
  std::vector<double> sigma(D, sp.sigma0);
  if (!sp.sigmaVec.empty()) {
    for (size_t d = 0; d < D; d++) sigma[d] = d < sp.sigmaVec.size() ? sp.sigmaVec[d] : sp.sigma0;
  } else if (sp.sigmaRel > 0 && sp.lo.size() == D) {
    for (size_t d = 0; d < D; d++) sigma[d] = std::max(1e-6, sp.sigmaRel * (sp.hi[d] - sp.lo[d]));
  }
  // Header record.  build_tables reads the fit's configuration from this line
  // instead of from prose, which is how the v0.5 study ended up sourcing beta
  // from a markdown file.
  fprintf(out, "{\"header\":1,\"base\":\"%s\",\"objective\":%d,\"beta\":%.4f,\"paired\":%d,"
               "\"pop\":%d,\"elite\":%d,\"gens\":%d,\"deals\":%d,\"rotations\":%d,\"seed\":%llu,"
               "\"sigmaRel\":%.4f,\"panel\":[",
          sp.baseSpec.c_str(), int(sp.objective), sp.beta, sp.paired ? 1 : 0,
          sp.population, sp.elite, sp.generations, sp.gamesPerOpponent, sp.rotations,
          (unsigned long long)sp.seed, sp.sigmaRel);
  for (size_t i = 0; i < sp.panel.size(); i++) fprintf(out, "%s\"%s\"", i ? "," : "", sp.panel[i].c_str());
  fprintf(out, "],\"sigma\":[");
  for (size_t d = 0; d < D; d++) fprintf(out, "%s%.5f", d ? "," : "", sigma[d]);
  fprintf(out, "]}\n");
  fflush(out);
  Rng rng(sp.seed);
  std::vector<double> best = mu;
  double bestScore = -1e9;
  for (int g = 0; g < sp.generations; g++) {
    uint64_t genSeed = mixSeed(sp.seed, uint64_t(g) * 104729 + 5);
    std::vector<std::vector<double>> cand(sp.population, std::vector<double>(D));
    std::vector<Evaluation> evals(sp.population);
    std::vector<int> clipped(D, 0);
    for (int i = 0; i < sp.population; i++) {
      for (size_t d = 0; d < D; d++) {
        double u1 = std::max(1e-12, rng.uni()), u2 = rng.uni();
        double z = std::sqrt(-2 * std::log(u1)) * std::cos(2 * M_PI * u2);
        double v = mu[d] + sigma[d] * z;
        if (!sp.lo.empty()) {
          double c = std::min(sp.hi[d], std::max(sp.lo[d], v));
          if (c != v) clipped[d]++;
          v = c;
        }
        cand[i][d] = v;
      }
      if (i == 0) cand[i] = mu;                   // always evaluate the incumbent
      evals[i] = evaluateCandidate(sp, cand[i], genSeed);
    }
    // Paired rescoring.  The incumbent is candidate 0 and was played on the very
    // same deals, so the per-deal margin over it is available for free and
    // removes the deal-level variance that dominates an absolute win rate.
    if (sp.paired) {
      for (int i = 0; i < sp.population; i++) {
        std::vector<double> rel(sp.panel.size(), 0.0);
        for (size_t o = 0; o < sp.panel.size(); o++) {
          if (o >= evals[i].paired.size() || o >= evals[0].paired.size()) continue;
          const auto& a = evals[i].paired[o];
          const auto& b = evals[0].paired[o];
          size_t nn = std::min(a.size(), b.size());
          double acc2 = 0;
          for (size_t j = 0; j < nn; j++) acc2 += (double(a[j]) - double(b[j])) / double(std::max(1, sp.rotations));
          rel[o] = nn ? acc2 / double(nn) : 0.0;
        }
        evals[i].winRates = rel;                  // now a paired MARGIN profile
      }
    }
    // Regret objectives need the per-opponent best achieved this generation.
    std::vector<double> ref;
    if (sp.objective == TuneObjective::Regret || sp.objective == TuneObjective::MinimaxRegret) {
      ref.assign(sp.panel.size(), -1e9);
      for (int i = 0; i < sp.population; i++)
        for (size_t o = 0; o < evals[i].winRates.size(); o++)
          ref[o] = std::max(ref[o], evals[i].winRates[o]);
    }
    for (int i = 0; i < sp.population; i++)
      evals[i].score = combine(sp, evals[i].winRates, ref.empty() ? nullptr : &ref);
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
    fprintf(out, "],\"clip\":[");
    for (size_t d = 0; d < D; d++) fprintf(out, "%s%.3f", d ? "," : "", double(clipped[d]) / double(sp.population));
    fprintf(out, "],\"spread\":%.5f}\n", [&]{ double mn=1e18,mx=-1e18; for(auto&e:evals){mn=std::min(mn,e.score);mx=std::max(mx,e.score);} return mx-mn; }());
    fflush(out);
  }
  TuneSpec fs = sp; fs.paired = false; fs.objective = TuneObjective::SoftMin;
  Evaluation fin = evaluateCandidate(fs, mu, mixSeed(sp.seed, 999983));
  Evaluation fb = evaluateCandidate(fs, best, mixSeed(sp.seed, 999983));
  fprintf(out, "{\"final\":\"mu\",\"score\":%.4f}\n{\"final\":\"best\",\"score\":%.4f}\n", fin.score, fb.score);
  return fin.score >= fb.score ? mu : best;
}

} // namespace fish
