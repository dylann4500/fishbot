// Deterministic brute-force oracle for the exact block-allocation engine.
//
// The block engine of blockdp.hpp claims to compute, exactly and in closed
// form, four quantities over the observer-conditioned deal posterior:
//
//   Z                        the number of deals consistent with (C1)-(C5)
//   mu[c][p]                 per-card ownership marginals
//   P(team T owns half-suit) the declaration feasibility query
//   P(allocation A)          the declaration query itself
//
// This file validates all four against exhaustive enumeration on small
// reachable states, and additionally checks the sampler's empirical frequencies
// against the exact marginals.  Enumeration is over EVERY assignment of the
// unresolved cards to seats, filtered by support (C1)-(C3), capacities (C4) and
// the ask-legality certificates (C5) -- no dynamic programming, no factorisation
// and no sampling, so an agreement is evidence about the DP rather than about a
// shared derivation.
//
// The state is skipped when the enumeration would exceed `maxNodes`; the number
// of states skipped is reported alongside the number checked, so the coverage of
// the test is visible rather than implied.
#pragma once
#include "blockdp.hpp"
#include <map>

namespace fish {

struct OracleStats {
  long long statesChecked = 0;      // states fully enumerated and compared
  long long statesSkipped = 0;      // states too large for exhaustive enumeration
  long long statesWithC5 = 0;       // of those checked, how many had a live certificate
  long long deals = 0;              // total consistent assignments enumerated
  long long buildFailures = 0;

  double maxZRelDiff = 0;           // |Z_dp - Z_bf| / Z_bf
  double maxMarginalDiff = 0;       // per-card ownership
  double maxTeamDiff = 0;           // P(team owns half-suit)
  double maxAllocDiff = 0;          // P(named allocation), over every allocation
  double maxCountVectorSpread = 0;  // corollary: equal probability within a class
  double maxSampleDiff = 0;         // sampler frequency vs exact marginal
  long long sampleDraws = 0;

  long long allocChecks = 0;
  long long teamChecks = 0;
  long long marginalChecks = 0;

  // Named-allocation soundness of the reference declarer: does
  // bestTeamAllocation ever return an allocation of probability zero, and is it
  // ever not the argmax?
  long long bestAllocChecks = 0;
  long long bestAllocInconsistent = 0;
  long long bestAllocNotArgmax = 0;
  double maxBestAllocDiff = 0;

  bool pass() const {
    return buildFailures == 0
        && maxZRelDiff      < 1e-9
        && maxMarginalDiff  < 1e-9
        && maxTeamDiff      < 1e-9
        && maxAllocDiff     < 1e-9
        && maxCountVectorSpread < 1e-9
        && bestAllocInconsistent == 0
        && bestAllocNotArgmax == 0;
  }
};

// Exhaustive enumeration of the posterior for one observer.
struct BruteForce {
  int nU = 0;
  int cards[NCARD];
  uint8_t cmask[NCARD];
  uint8_t cap[NPLAY];
  long long Z = 0;
  double marg[NCARD][NPLAY];
  // per half-suit: assignment of that half-suit's unresolved cards (base 8,
  // low card first) -> number of consistent completions
  std::map<uint64_t, long long> alloc[NSET];
  int setCards[NSET][SETSZ], nSetCards[NSET];

  bool enumerate(const Knowledge& k, long long maxNodes) {
    nU = 0;
    uint64_t u = k.unresolved;
    while (u) { int c = __builtin_ctzll(u); u &= u - 1; cards[nU] = c; cmask[nU] = k.mask[c]; nU++; }
    k.capacities(cap);
    int sum = 0; for (int p = 0; p < NPLAY; p++) sum += cap[p];
    if (sum != nU) return false;
    for (int s = 0; s < NSET; s++) { nSetCards[s] = 0; alloc[s].clear(); }
    for (int i = 0; i < nU; i++) { int s = setOf(cards[i]); setCards[s][nSetCards[s]++] = i; }
    for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) marg[c][p] = 0;
    Z = 0;

    // A cheap upper bound on the search tree; refuse states that would blow up.
    double bound = 1;
    for (int i = 0; i < nU; i++) { bound *= double(__builtin_popcount(cmask[i])); if (bound > 4e9) break; }
    if (bound > double(maxNodes)) return false;

    int assign[NCARD];
    int cnt[NPLAY] = {0,0,0,0,0,0};
    long long nodes = 0;
    // iterative DFS with capacity pruning
    int stack[NCARD];
    for (int i = 0; i < nU; i++) stack[i] = -1;
    int depth = 0;
    uint8_t owners[NCARD];
    for (int c = 0; c < NCARD; c++) owners[c] = k.owner[c];
    while (depth >= 0) {
      if (++nodes > maxNodes) return false;
      int nxt = stack[depth] + 1;
      if (stack[depth] >= 0) cnt[stack[depth]]--;
      bool placed = false;
      for (; nxt < NPLAY; nxt++) {
        if (!(cmask[depth] & (1u << nxt))) continue;
        if (cnt[nxt] + 1 > cap[nxt]) continue;
        stack[depth] = nxt; assign[depth] = nxt; cnt[nxt]++; placed = true; break;
      }
      if (!placed) { stack[depth] = -1; depth--; continue; }
      if (depth + 1 < nU) { depth++; stack[depth] = -1; continue; }
      // leaf: a complete capacity-feasible assignment; test (C5) directly
      for (int i = 0; i < nU; i++) owners[cards[i]] = uint8_t(assign[i]);
      if (Belief::satisfies(k, owners)) {
        Z++;
        for (int i = 0; i < nU; i++) marg[cards[i]][assign[i]] += 1.0;
        for (int s = 0; s < NSET; s++) {
          if (!nSetCards[s]) continue;
          uint64_t key = 0;
          for (int j = 0; j < nSetCards[s]; j++) key |= uint64_t(assign[setCards[s][j]]) << (3 * j);
          alloc[s][key] += 1;
        }
      }
    }
    if (!Z) return false;
    for (int i = 0; i < nU; i++)
      for (int p = 0; p < NPLAY; p++) marg[cards[i]][p] /= double(Z);
    return true;
  }
};

// Compare the block engine against the brute force on one observer's state.
inline void oracleCheckState(const Knowledge& k, BlockDP& bd, BruteForce& bf,
                             OracleStats& st, Rng& rng, int sampleDraws) {
  if (!bd.ok) { st.buildFailures++; return; }
  st.statesChecked++;
  st.deals += bf.Z;
  bool anyC5 = false;
  for (const auto& d : k.disj) if (d.cards & k.unresolved) anyC5 = true;
  if (anyC5) st.statesWithC5++;

  // (1) partition function
  st.maxZRelDiff = std::max(st.maxZRelDiff, std::fabs(bd.Z - double(bf.Z)) / double(bf.Z));

  // (2) per-card marginals
  static double mb[NCARD][NPLAY];
  for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) mb[c][p] = 0;
  bd.marginals(mb);
  for (int i = 0; i < bf.nU; i++) {
    int c = bf.cards[i];
    for (int p = 0; p < NPLAY; p++) {
      st.maxMarginalDiff = std::max(st.maxMarginalDiff, std::fabs(mb[c][p] - bf.marg[c][p]));
      st.marginalChecks++;
    }
  }

  // (3) team-ownership probability, both teams, every half-suit with an
  //     unresolved card and no resolved card on the other team
  for (int s = 0; s < NSET; s++) {
    if (!bf.nSetCards[s]) continue;
    for (int t = 0; t < 2; t++) {
      int teamMask = 0;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == t) teamMask |= 1 << p;
      bool feasible = true;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        if (k.owner[c] < NPLAY && !(teamMask & (1u << k.owner[c]))) feasible = false;
        if (k.owner[c] == OUT_OF_PLAY) feasible = false;
      }
      if (!feasible) continue;
      long long hits = 0;
      for (const auto& kv : bf.alloc[s]) {
        bool allTeam = true;
        for (int j = 0; j < bf.nSetCards[s]; j++) {
          int p = int((kv.first >> (3 * j)) & 7);
          if (!(teamMask & (1u << p))) { allTeam = false; break; }
        }
        if (allTeam) hits += kv.second;
      }
      double exact = double(hits) / double(bf.Z);
      double got = bd.teamOwnsProbability(s, teamMask);
      st.maxTeamDiff = std::max(st.maxTeamDiff, std::fabs(got - exact));
      st.teamChecks++;
    }
  }

  // (4) probability of every named allocation, and the equal-probability
  //     corollary within a count-vector class
  for (int s = 0; s < NSET; s++) {
    if (!bf.nSetCards[s]) continue;
    std::map<uint64_t, std::pair<double, double>> byCount;   // packed count -> (min,max) prob
    for (const auto& kv : bf.alloc[s]) {
      int seats[SETSZ];
      int cnt[NPLAY] = {0,0,0,0,0,0};
      for (int j = 0; j < bf.nSetCards[s]; j++) {
        seats[j] = int((kv.first >> (3 * j)) & 7);
        cnt[seats[j]]++;
      }
      double exact = double(kv.second) / double(bf.Z);
      double got = bd.allocationProbability(k, s, seats);
      st.maxAllocDiff = std::max(st.maxAllocDiff, std::fabs(got - exact));
      st.allocChecks++;
      uint64_t cp = BlockDP::packCnt(cnt);
      auto it = byCount.find(cp);
      if (it == byCount.end()) byCount[cp] = {exact, exact};
      else { it->second.first = std::min(it->second.first, exact);
             it->second.second = std::max(it->second.second, exact); }
    }
    for (const auto& kv : byCount)
      st.maxCountVectorSpread = std::max(st.maxCountVectorSpread, kv.second.second - kv.second.first);
  }

  // (5) the reference declarer's named allocation: consistent, and the argmax
  for (int s = 0; s < NSET; s++) {
    if (!bf.nSetCards[s]) continue;
    for (int t = 0; t < 2; t++) {
      int teamMask = 0;
      for (int p = 0; p < NPLAY; p++) if (teamOf(p) == t) teamMask |= 1 << p;
      bool feasible = true;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        if (k.owner[c] < NPLAY && !(teamMask & (1u << k.owner[c]))) feasible = false;
        if (k.owner[c] == OUT_OF_PLAY) feasible = false;
      }
      if (!feasible) continue;
      int outCards[SETSZ], outSeats[SETSZ], n2 = 0;
      double pa = bd.bestTeamAllocation(s, teamMask, outCards, outSeats, n2);
      if (n2 != bf.nSetCards[s]) continue;
      // best achievable under brute force, restricted to allocations on team t
      double bestExact = 0;
      for (const auto& kv : bf.alloc[s]) {
        bool allTeam = true;
        for (int j = 0; j < bf.nSetCards[s]; j++)
          if (!(teamMask & (1u << int((kv.first >> (3 * j)) & 7)))) { allTeam = false; break; }
        if (allTeam) bestExact = std::max(bestExact, double(kv.second) / double(bf.Z));
      }
      // probability the engine's own named allocation actually has
      uint64_t key = 0;
      for (int j = 0; j < n2; j++) {
        int idx = -1;
        for (int q = 0; q < bf.nSetCards[s]; q++)
          if (bf.cards[bf.setCards[s][q]] == outCards[j]) { idx = q; break; }
        if (idx < 0) { key = ~0ull; break; }
        key |= uint64_t(outSeats[j]) << (3 * idx);
      }
      st.bestAllocChecks++;
      if (key == ~0ull) { st.bestAllocInconsistent++; continue; }
      auto it = bf.alloc[s].find(key);
      double named = (it == bf.alloc[s].end()) ? 0.0 : double(it->second) / double(bf.Z);
      if (pa > 0 && named == 0.0) st.bestAllocInconsistent++;
      if (pa > 0 && named + 1e-12 < bestExact) st.bestAllocNotArgmax++;
      st.maxBestAllocDiff = std::max(st.maxBestAllocDiff, std::fabs(pa - named));
    }
  }

  // (6) sampler frequencies against the exact marginals
  if (sampleDraws > 0) {
    DealDP dd;
    if (dd.build(k)) {
      static double ms[NCARD][NPLAY];
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) ms[c][p] = 0;
      std::array<uint8_t, NCARD> buf{};
      for (int c = 0; c < NCARD; c++) buf[c] = k.owner[c];
      int drawn = 0;
      for (int t = 0; t < sampleDraws * 40 && drawn < sampleDraws; t++) {
        dd.sample(rng, buf.data());
        if (!Belief::satisfies(k, buf.data())) continue;
        for (int i = 0; i < bf.nU; i++) ms[bf.cards[i]][buf[bf.cards[i]]] += 1;
        drawn++;
      }
      if (drawn >= 500) {
        st.sampleDraws += drawn;
        for (int i = 0; i < bf.nU; i++) {
          int c = bf.cards[i];
          for (int p = 0; p < NPLAY; p++)
            st.maxSampleDiff = std::max(st.maxSampleDiff, std::fabs(ms[c][p] / drawn - bf.marg[c][p]));
        }
      }
    }
  }
}

} // namespace fish
