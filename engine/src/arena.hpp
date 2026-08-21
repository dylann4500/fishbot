// Threaded, seed-paired, orientation-balanced match runner.
#pragma once
#include "factory.hpp"
#include <thread>
#include <atomic>
#include <mutex>

namespace fish {

struct MatchStats {
  int games = 0;                 // deals; each is played in both orientations
  int winsA = 0;                 // A wins out of 2*games
  long long asks[2] = {0, 0}, hits[2] = {0, 0};
  long long decl[2] = {0, 0}, declCorrect[2] = {0, 0};
  long long fdecl[2] = {0, 0}, fdeclCorrect[2] = {0, 0};
  long long sets[2] = {0, 0};
  long long outOfTurn[2] = {0, 0};
  long long lockHeld[2] = {0, 0};
  long long lockedDecls[2] = {0, 0};
  long long events = 0;
  int limitHits = 0;
  long long auditViolations = 0, auditChecks = 0;
  std::vector<uint8_t> paired;   // per deal: A wins across the two orientations (0..2)
  double seconds = 0;
  void merge(const MatchStats& o) {
    games += o.games; winsA += o.winsA; events += o.events; limitHits += o.limitHits;
    auditViolations += o.auditViolations; auditChecks += o.auditChecks;
    for (int i = 0; i < 2; i++) {
      asks[i] += o.asks[i]; hits[i] += o.hits[i];
      decl[i] += o.decl[i]; declCorrect[i] += o.declCorrect[i];
      fdecl[i] += o.fdecl[i]; fdeclCorrect[i] += o.fdeclCorrect[i];
      sets[i] += o.sets[i]; outOfTurn[i] += o.outOfTurn[i];
      lockHeld[i] += o.lockHeld[i]; lockedDecls[i] += o.lockedDecls[i];
    }
  }
};

inline void wilson(int k, int n, double& lo, double& hi) {
  if (!n) { lo = hi = 0; return; }
  const double z = 1.959963985;
  double p = double(k) / n, d = 1 + z * z / n;
  double c = (p + z * z / (2.0 * n)) / d;
  double m = z * std::sqrt((p * (1 - p) + z * z / (4.0 * n)) / n) / d;
  lo = std::max(0.0, c - m); hi = std::min(1.0, c + m);
}

struct MatchConfig {
  std::string specA, specB;
  int games = 1000;              // deals
  int rotations = 2;             // 2 = team swap; 6 = full duplicate block
  uint64_t seed = 20260821;
  Rules rules;
  bool audit = false;
  int threads = 0;
};

inline MatchStats runMatch(const MatchConfig& mc) {
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
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(mc.specA); B[i] = makeAgent(mc.specB); }
      MatchStats& st = local[t];
      auto& pr = pairedLocal[t];
      Game game;
      for (int i = t; i < mc.games; i += nThreads) {
        uint64_t s = mixSeed(mc.seed, uint64_t(i) * 2654435761ull + 1);
        int aWins = 0;
        // Duplicate blocks.  With 2 rotations we simply swap which team A holds.
        // With 6, every cyclic seat rotation of the same deal is played: odd
        // rotations exchange the two teams' hands, even rotations permute hands
        // within a team, so A and B each hold every hand-triple exactly once and
        // the deal's intrinsic luck cancels out of the comparison.
        for (int rot = 0; rot < mc.rotations; rot++) {
          // With six rotations we vary the team label (orient) and the hand
          // shift independently.  Coupling them -- orient = rot & 1 with a
          // shift of rot -- leaves policy A holding the even-indexed dealt
          // hands in every rotation, which is the opposite of a duplicate
          // block.  Here A holds each hand-triple in exactly three of the six.
          int orient = (mc.rotations == 2) ? rot : (rot / 3);
          int shift  = (mc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) {
            bool aSeat = (teamOf(p) == orient) ? true : false;
            ag[p] = aSeat ? A[p / 2].get() : B[p / 2].get();
          }
          game.audit = mc.audit; game.auditViolations = 0; game.auditChecks = 0;
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
          st.outOfTurn[0] += r.outOfTurnDecls[teamA]; st.outOfTurn[1] += r.outOfTurnDecls[teamB];
          st.lockHeld[0] += r.lockHeldEvents[teamA]; st.lockHeld[1] += r.lockHeldEvents[teamB];
          st.lockedDecls[0] += r.lockedDeclarations[teamA]; st.lockedDecls[1] += r.lockedDeclarations[teamB];
          st.events += r.events;
          if (r.hitLimit) st.limitHits++;
          st.auditViolations += game.auditViolations; st.auditChecks += game.auditChecks;
        }
        st.games++;
        pr.push_back(uint8_t(aWins));
      }
    });
  }
  for (auto& th : pool) th.join();
  MatchStats total;
  for (int t = 0; t < nThreads; t++) {
    total.merge(local[t]);
    total.paired.insert(total.paired.end(), pairedLocal[t].begin(), pairedLocal[t].end());
  }
  total.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return total;
}

// Paired bootstrap over deals for the difference between two matched runs.
// Bootstrap resamples DEALS, not games: the rotations of one deal are a single
// correlated cluster and resampling games would understate the variance.
inline void pairedBootstrap(const std::vector<uint8_t>& x, const std::vector<uint8_t>& y,
                            double& mean, double& lo, double& hi, uint64_t seed = 12345, int B = 20000,
                            double denom = 2.0) {
  size_t n = std::min(x.size(), y.size());
  if (!n) { mean = lo = hi = 0; return; }
  double sum = 0;
  for (size_t i = 0; i < n; i++) sum += (double(x[i]) - double(y[i])) / denom;
  mean = sum / double(n);
  std::vector<double> draws(B);
  Rng rng(seed);
  for (int b = 0; b < B; b++) {
    double s = 0;
    for (size_t i = 0; i < n; i++) { size_t j = rng.u32(uint32_t(n)); s += (double(x[j]) - double(y[j])) / denom; }
    draws[b] = s / double(n);
  }
  std::sort(draws.begin(), draws.end());
  lo = draws[size_t(0.025 * B)];
  hi = draws[size_t(0.975 * B)];
}

// Cluster bootstrap CI for one arm's win rate, resampling deals.
inline void clusterBootstrap(const std::vector<uint8_t>& wins, int perDeal,
                             double& mean, double& lo, double& hi, uint64_t seed = 999, int B = 20000) {
  size_t n = wins.size();
  if (!n) { mean = lo = hi = 0; return; }
  double sum = 0;
  for (size_t i = 0; i < n; i++) sum += double(wins[i]);
  mean = sum / (double(n) * perDeal);
  std::vector<double> draws(B);
  Rng rng(seed);
  for (int b = 0; b < B; b++) {
    double s2 = 0;
    for (size_t i = 0; i < n; i++) s2 += double(wins[rng.u32(uint32_t(n))]);
    draws[b] = s2 / (double(n) * perDeal);
  }
  std::sort(draws.begin(), draws.end());
  lo = draws[size_t(0.025 * B)];
  hi = draws[size_t(0.975 * B)];
}

} // namespace fish

namespace fish {

// Reliability of probability forecasts: Brier score, log loss, expected
// calibration error, and a ten-bin reliability table.
struct Reliability {
  int n = 0;
  double brier = 0, logloss = 0, ece = 0, meanPred = 0, meanObs = 0;
  int binN[10] = {0,0,0,0,0,0,0,0,0,0};
  double binPred[10] = {0,0,0,0,0,0,0,0,0,0}, binObs[10] = {0,0,0,0,0,0,0,0,0,0};
};

inline Reliability reliability(const std::vector<std::pair<float, uint8_t>>& d) {
  Reliability r;
  r.n = int(d.size());
  if (!r.n) return r;
  for (auto& [p0, y] : d) {
    double p = std::min(1.0 - 1e-9, std::max(1e-9, double(p0)));
    r.brier += (p - y) * (p - y);
    r.logloss += -(y ? std::log(p) : std::log(1 - p));
    r.meanPred += p; r.meanObs += y;
    int b = std::min(9, int(p * 10));
    r.binN[b]++; r.binPred[b] += p; r.binObs[b] += y;
  }
  r.brier /= r.n; r.logloss /= r.n; r.meanPred /= r.n; r.meanObs /= r.n;
  for (int b = 0; b < 10; b++) if (r.binN[b]) {
    r.binPred[b] /= r.binN[b]; r.binObs[b] /= r.binN[b];
    r.ece += double(r.binN[b]) / r.n * std::fabs(r.binPred[b] - r.binObs[b]);
  }
  return r;
}

inline CalibSink collectCalibration(const std::string& specA, const std::string& specB,
                                    int deals, uint64_t seed, const Rules& rules, int rotations = 2) {
  CalibSink sink;
  std::unique_ptr<Agent> A[3], B[3];
  for (int i = 0; i < 3; i++) { A[i] = makeAgent(specA); B[i] = makeAgent(specB); }
  Game game;
  for (int i = 0; i < deals; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < rotations; rot++) {
      int orient = (rotations == 2) ? rot : (rot & 1);
      Agent* ag[NPLAY];
      for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
      game.rotation = (rotations == 2) ? 0 : rot;
      game.calib = &sink; game.calibTeam = orient;
      game.run(s, rules, ag);
    }
  }
  return sink;
}

} // namespace fish
