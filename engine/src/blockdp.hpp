// Exact posterior over the initial deal, including the disjunctive ask-legality
// certificates.
//
// Structure.  Every certificate "player A held another card of half-suit S when
// they asked in S" constrains only cards of S, so no certificate couples two
// half-suits.  We therefore group the unresolved cards by half-suit and give
// each group one of two exact treatments:
//
//   * CARD group  (no live certificate in that half-suit): the cards are
//     expanded one at a time, branching factor |mask| <= 5.
//   * BLOCK group (at least one live certificate): the group's assignments are
//     enumerated exhaustively, filtered against its certificates exactly, and
//     aggregated into a count-vector table g(t).
//
// Either way the group consumes a known number of cards, so the "remaining
// capacity sum determines the layer" identity holds and the forward/backward
// tables stay single arrays of size prod_p (q_p + 1) <= 10^5.
//
// Exposed exactly, with no sampling and no independence assumption:
//   Z                      information-set mass
//   mu[c][p]               per-card ownership marginals
//   P(allocation A)        = S_{t(A)} / Z
//   P(team owns half-suit) = sum_{t on team} g(t) S_t / Z
//
// Under the uniform-deal prior every surviving allocation sharing a count vector
// is equally likely, so the MAP allocation is decided by the count vector alone.
#pragma once
#include "fish.hpp"
#include "belief.hpp"

namespace fish {

static constexpr int MAXENT = 512;
static constexpr int BLOCK_STATE_CAP = 200000;
static constexpr uint64_t SWAR_H = 0x8080808080808080ull;

struct Unit {                    // one DP layer transition
  int nCards = 0;
  int cards[SETSZ];
  uint8_t cmask[SETSZ];
  int group = -1;
  int nEnt = 0;
  uint64_t tpack[MAXENT];
  int toff[MAXENT];
  float g[MAXENT];
  bool isBlock = false;
};

struct Group {                   // one half-suit
  int set = -1;
  int firstUnit = 0, nUnits = 0;
  int nCards = 0;
  int cards[SETSZ];
  uint8_t cmask[SETSZ];
  bool isBlock = false;
  bool sReady = false;
  int startSum = 0;              // remaining-capacity sum entering the group
  int nEnt = 0;                  // count-vector table for the whole group
  uint64_t tpack[MAXENT];
  int toff[MAXENT];
  float g[MAXENT];
  double S[MAXENT];
  float gcard[MAXENT][SETSZ][NPLAY];
};

struct BlockDP {
  int nUnits = 0, nGroups = 0;
  Unit* units = nullptr;
  Group* groups = nullptr;
  uint8_t cap[NPLAY];
  int stride[NPLAY];
  int total = 0, Q = 0;
  uint64_t capPack = 0;
  bool ok = false;
  double Z = 0;
  double *F = nullptr, *B = nullptr;
  uint64_t* dpack = nullptr;
  int32_t* stateBySum = nullptr;
  int32_t bucket[64];            // start index per remaining-capacity sum
  int32_t bucketN[64];
  int8_t groupOfSum[64];

  struct Buffers {
    std::vector<Unit> units;
    std::vector<Group> groups;
    std::vector<double> F, B;
    std::vector<uint64_t> dpack;
    std::vector<int32_t> stateBySum;
    std::vector<int32_t> hash;
    void ensure(int n) {
      if (units.size() < NCARD + NSET) units.resize(NCARD + NSET);
      if (groups.size() < NSET) groups.resize(NSET);
      if ((int)F.size() < n) { F.resize(n); B.resize(n); dpack.resize(n); stateBySum.resize(n); }
      if (hash.size() < 4096) hash.resize(4096);
    }
  };
  static Buffers& buffers() { static thread_local Buffers b; return b; }

  // ---- table-aliasing guard (v0.6 E2) --------------------------------------
  // `buffers()` is one thread_local pool, and `build()` parks this instance's
  // Unit/Group/F/B tables in it.  A second BlockDP that builds on the same
  // thread therefore silently repoints the first instance's pointers, so a
  // query issued afterwards reads the other instance's tables.  Measured at
  // 285 mismatches in 294 checks (research/v05/results/P2-forced-endgame.md
  // section 6).  Harmless under the deployed Fast belief, which never queries a
  // BlockDP; fatal for anything exact, which is precisely how it survived.
  //
  // Fix: stamp every build with a per-thread generation counter and re-build
  // lazily from a stored copy of the source Knowledge whenever a query finds
  // the stamp stale.  O(1) memory, and it cannot be forgotten at a call site
  // because the check lives inside the queries.
  static long long& generation() { static thread_local long long g = 0; return g; }
  long long stamp = -1;
  Knowledge srcK;
  bool haveSrc = false;
  bool current() const { return ok && haveSrc && stamp == generation(); }
  bool ensureCurrent() {
    if (stamp == generation()) return ok;
    if (!haveSrc) return false;
    Knowledge tmp = srcK;
    return build(tmp);
  }

  static inline uint64_t packCnt(const int* v) {
    uint64_t r = 0;
    for (int p = 0; p < NPLAY; p++) r |= uint64_t(uint8_t(v[p])) << (8 * p);
    return r;
  }
  static inline bool geLanes(uint64_t a, uint64_t b) {
    return ((((a | SWAR_H) - b) & SWAR_H) == SWAR_H);
  }
  inline int offsetOf(const int* cnt) const {
    int o = 0; for (int p = 0; p < NPLAY; p++) o += cnt[p] * stride[p]; return o;
  }

  // Enumerate a group's assignments, filter, aggregate into (t, g, gcard).
  bool enumerateGroup(Group& gr, int nD, const uint8_t* dPlayer, const uint64_t* dCards) {
    gr.nEnt = 0;
    auto& H = buffers().hash;
    std::fill(H.begin(), H.begin() + 4096, -1);
    int assign[SETSZ], cnt[NPLAY] = {0,0,0,0,0,0}, stack[SETSZ];
    for (int i = 0; i < gr.nCards; i++) stack[i] = -1;
    int depth = 0;
    long long guard = 0;
    while (depth >= 0) {
      if (++guard > 2000000) return false;
      int nxt = stack[depth] + 1;
      if (stack[depth] >= 0) cnt[stack[depth]]--;
      bool placed = false;
      for (; nxt < NPLAY; nxt++) {
        if (!(gr.cmask[depth] & (1u << nxt))) continue;
        if (cnt[nxt] + 1 > cap[nxt]) continue;
        stack[depth] = nxt; assign[depth] = nxt; cnt[nxt]++; placed = true; break;
      }
      if (!placed) { stack[depth] = -1; depth--; continue; }
      if (depth + 1 < gr.nCards) { depth++; stack[depth] = -1; continue; }
      bool okAll = true;
      for (int j = 0; j < nD && okAll; j++) {
        bool sat = false;
        for (int i = 0; i < gr.nCards; i++)
          if (((dCards[j] >> gr.cards[i]) & 1ull) && assign[i] == dPlayer[j]) { sat = true; break; }
        if (!sat) okAll = false;
      }
      if (okAll) {
        uint64_t tp = packCnt(cnt);
        uint32_t h = uint32_t((tp * 0x9E3779B97F4A7C15ull) >> 52) & 4095u;
        int idx = -1;
        while (true) { int e = H[h]; if (e < 0) break; if (gr.tpack[e] == tp) { idx = e; break; } h = (h + 1) & 4095u; }
        if (idx < 0) {
          if (gr.nEnt >= MAXENT) return false;
          idx = gr.nEnt++; H[h] = idx;
          gr.tpack[idx] = tp; gr.toff[idx] = offsetOf(cnt); gr.g[idx] = 0;
          memset(gr.gcard[idx], 0, sizeof(gr.gcard[idx]));
        }
        gr.g[idx] += 1.0f;
        for (int i = 0; i < gr.nCards; i++) gr.gcard[idx][i][assign[i]] += 1.0f;
      }
    }
    return gr.nEnt > 0;
  }

  bool build(const Knowledge& k) {
    ok = false;
    if (&k != &srcK) { srcK = k; haveSrc = true; }
    stamp = ++generation();
    k.capacities(cap);
    Q = __builtin_popcountll(k.unresolved);
    int sum = 0; for (int p = 0; p < NPLAY; p++) sum += cap[p];
    if (sum != Q) return false;
    if (!Q) { ok = true; Z = 1; nUnits = nGroups = 0; return true; }
    total = 1;
    for (int p = 0; p < NPLAY; p++) {
      stride[p] = total;
      long long t = (long long)total * (cap[p] + 1);
      if (t > BLOCK_STATE_CAP) return false;
      total = int(t);
    }
    capPack = 0;
    for (int p = 0; p < NPLAY; p++) capPack |= uint64_t(cap[p]) << (8 * p);
    auto& bf = buffers();
    bf.ensure(total);
    units = bf.units.data(); groups = bf.groups.data();
    F = bf.F.data(); B = bf.B.data(); dpack = bf.dpack.data(); stateBySum = bf.stateBySum.data();
    // enumerate the state space once: packed digits, and states bucketed by sum
    for (int s = 0; s <= sum + 1 && s < 64; s++) bucketN[s] = 0;
    { int digit[NPLAY] = {0,0,0,0,0,0}; int s2 = 0; uint64_t dp = 0;
      for (int st = 0; st < total; st++) {
        dpack[st] = dp;
        if (s2 < 64) bucketN[s2]++;
        for (int p = 0; p < NPLAY; p++) {
          if (digit[p] + 1 <= cap[p]) { digit[p]++; s2++; dp += uint64_t(1) << (8 * p); break; }
          s2 -= digit[p]; dp -= uint64_t(digit[p]) << (8 * p); digit[p] = 0;
        }
      }
      int acc = 0;
      for (int s = 0; s <= sum && s < 64; s++) { bucket[s] = acc; acc += bucketN[s]; }
      int pos[64];
      for (int s = 0; s <= sum && s < 64; s++) pos[s] = bucket[s];
      int d2[NPLAY] = {0,0,0,0,0,0}; int s3 = 0;
      for (int st = 0; st < total; st++) {
        if (s3 < 64) stateBySum[pos[s3]++] = st;
        for (int p = 0; p < NPLAY; p++) {
          if (d2[p] + 1 <= cap[p]) { d2[p]++; s3++; break; }
          s3 -= d2[p]; d2[p] = 0;
        }
      }
    }
    // groups and units
    nGroups = 0; nUnits = 0;
    int cum = 0;
    for (int s = 0; s < NSET; s++) {
      uint64_t un = k.unresolved & setMask(s);
      if (!un) continue;
      Group& gr = groups[nGroups];
      gr.set = s; gr.nCards = 0; gr.nEnt = 0;
      while (un) { int c = __builtin_ctzll(un); un &= un - 1;
        gr.cards[gr.nCards] = c; gr.cmask[gr.nCards] = k.mask[c]; gr.nCards++; }
      int nD = 0; uint8_t dPlayer[16]; uint64_t dCards[16];
      for (const auto& d : k.disj) {
        uint64_t inBlock = d.cards & setMask(s) & k.unresolved;
        if (!inBlock) continue;
        if (nD < 16) { dPlayer[nD] = d.player; dCards[nD] = inBlock; nD++; }
      }
      gr.isBlock = nD > 0;
      gr.startSum = Q - cum;
      gr.firstUnit = nUnits;
      if (gr.isBlock) {
        if (!enumerateGroup(gr, nD, dPlayer, dCards)) return false;
        Unit& u = units[nUnits++];
        u.nCards = gr.nCards; u.isBlock = true; u.group = nGroups; u.nEnt = gr.nEnt;
        for (int i = 0; i < gr.nCards; i++) { u.cards[i] = gr.cards[i]; u.cmask[i] = gr.cmask[i]; }
        for (int e = 0; e < gr.nEnt; e++) { u.tpack[e] = gr.tpack[e]; u.toff[e] = gr.toff[e]; u.g[e] = gr.g[e]; }
      } else {
        for (int i = 0; i < gr.nCards; i++) {
          Unit& u = units[nUnits++];
          u.nCards = 1; u.isBlock = false; u.group = nGroups;
          u.cards[0] = gr.cards[i]; u.cmask[0] = gr.cmask[i];
          u.nEnt = 0;
          for (int p = 0; p < NPLAY; p++) if ((gr.cmask[i] & (1u << p)) && cap[p] > 0) {
            u.tpack[u.nEnt] = uint64_t(1) << (8 * p);
            u.toff[u.nEnt] = stride[p];
            u.g[u.nEnt] = 1.0f;
            u.nEnt++;
          }
          if (!u.nEnt) return false;
        }
      }
      gr.nUnits = nUnits - gr.firstUnit;
      gr.sReady = false;
      cum += gr.nCards;
      nGroups++;
    }
    for (int i = 0; i < 64; i++) groupOfSum[i] = -1;
    { int c2 = 0;
      for (int b = 0; b < nUnits; b++) { if (Q - c2 < 64) groupOfSum[Q - c2] = int8_t(b); c2 += units[b].nCards; }
      if (Q - c2 < 64 && Q - c2 >= 0) groupOfSum[Q - c2] = int8_t(nUnits);
    }
    // forward (pull): a state at unit-layer b reads layer b-1 at state + t
    F[total - 1] = 1.0;
    { int c2 = 0;
      for (int b = 0; b < nUnits; b++) {
        const Unit& u = units[b];
        c2 += u.nCards;
        int s2 = Q - c2;
        if (s2 < 0 || s2 >= 64) return false;
        const int32_t* list = stateBySum + bucket[s2];
        int n = bucketN[s2];
        for (int i = 0; i < n; i++) {
          int st = list[i];
          uint64_t slack = capPack - dpack[st];
          double acc = 0;
          for (int e = 0; e < u.nEnt; e++)
            if (geLanes(slack, u.tpack[e])) acc += double(u.g[e]) * F[st + u.toff[e]];
          F[st] = acc;
        }
      }
    }
    Z = F[0];
    if (!(Z > 0)) return false;
    // backward (pull): a state at unit-layer b reads layer b+1 at state - t
    B[0] = 1.0;
    { int cumTo[NCARD + NSET + 1];
      int c2 = 0;
      for (int b = 0; b < nUnits; b++) { cumTo[b] = c2; c2 += units[b].nCards; }
      for (int b = nUnits - 1; b >= 0; b--) {
        const Unit& u = units[b];
        int s2 = Q - cumTo[b];                        // remaining sum entering unit b
        if (s2 < 0 || s2 >= 64) return false;
        const int32_t* list = stateBySum + bucket[s2];
        int n = bucketN[s2];
        for (int i = 0; i < n; i++) {
          int st = list[i];
          uint64_t d = dpack[st];
          double acc = 0;
          for (int e = 0; e < u.nEnt; e++)
            if (geLanes(d, u.tpack[e])) acc += double(u.g[e]) * B[st - u.toff[e]];
          B[st] = acc;
        }
      }
    }
    ok = true;
    return true;
  }

  // Path mass for one group's count vector t, evaluated at the group's entry
  // layer:  S_t = sum_{|n| = startSum} F(n) * B(n - t).
  double pathMass(const Group& gr, uint64_t tpack_, int toff_) const {
    int s2 = gr.startSum;
    if (s2 < 0 || s2 >= 64) return 0;
    const int32_t* list = stateBySum + bucket[s2];
    int n = bucketN[s2];
    double acc = 0;
    for (int i = 0; i < n; i++) {
      int st = list[i];
      double f = F[st];
      if (f == 0) continue;
      if (!geLanes(dpack[st], tpack_)) continue;
      acc += f * B[st - toff_];
    }
    return acc;
  }

  void marginals(double out[NCARD][NPLAY]) {
    if (!ensureCurrent()) return;
    for (int b = 0; b < nUnits; b++) {
      const Unit& u = units[b];
      if (u.isBlock) continue;
      // single card: S for each candidate owner
      const Group& gr = groups[u.group];
      int s2 = 0;
      { int c2 = 0; for (int j = 0; j < b; j++) c2 += units[j].nCards; s2 = Q - c2; }
      (void)gr;
      double tot = 0;
      double vals[NPLAY] = {0,0,0,0,0,0};
      const int32_t* list = stateBySum + bucket[s2];
      int n = bucketN[s2];
      for (int e = 0; e < u.nEnt; e++) {
        double acc = 0;
        for (int i = 0; i < n; i++) {
          int st = list[i];
          double f = F[st];
          if (f == 0) continue;
          if (!geLanes(dpack[st], u.tpack[e])) continue;
          acc += f * B[st - u.toff[e]];
        }
        int p = __builtin_ctzll(u.tpack[e]) / 8;
        vals[p] = acc; tot += acc;
      }
      for (int p = 0; p < NPLAY; p++) out[u.cards[0]][p] = tot > 0 ? vals[p] / tot : 0.0;
    }
    for (int gi = 0; gi < nGroups; gi++) {
      Group& gr = groups[gi];
      if (!gr.isBlock) continue;
      for (int e = 0; e < gr.nEnt; e++) gr.S[e] = pathMass(gr, gr.tpack[e], gr.toff[e]);
      gr.sReady = true;
      for (int i = 0; i < gr.nCards; i++) {
        double tot = 0, vals[NPLAY] = {0,0,0,0,0,0};
        for (int p = 0; p < NPLAY; p++) {
          double acc = 0;
          for (int e = 0; e < gr.nEnt; e++) acc += double(gr.gcard[e][i][p]) * gr.S[e];
          vals[p] = acc; tot += acc;
        }
        for (int p = 0; p < NPLAY; p++) out[gr.cards[i]][p] = tot > 0 ? vals[p] / tot : 0.0;
      }
    }
  }

  Group* groupForSet(int s) {
    for (int gi = 0; gi < nGroups; gi++) if (groups[gi].set == s) return &groups[gi];
    return nullptr;
  }

  // Materialise the count-vector table for a CARD group on demand (a small local
  // DP over the group's own cards), so declaration queries are exact for every
  // half-suit, not only the constrained ones.
  void ensureGroupTable(Group& gr) {
    if (gr.nEnt) {
      if (!gr.sReady) { for (int e = 0; e < gr.nEnt; e++) gr.S[e] = pathMass(gr, gr.tpack[e], gr.toff[e]); gr.sReady = true; }
      return;
    }
    int cnt[NPLAY] = {0,0,0,0,0,0};
    int assign[SETSZ], stack[SETSZ];
    for (int i = 0; i < gr.nCards; i++) stack[i] = -1;
    auto& H = buffers().hash;
    std::fill(H.begin(), H.begin() + 4096, -1);
    int depth = 0;
    long long guard = 0;
    while (depth >= 0) {
      if (++guard > 2000000) return;
      int nxt = stack[depth] + 1;
      if (stack[depth] >= 0) cnt[stack[depth]]--;
      bool placed = false;
      for (; nxt < NPLAY; nxt++) {
        if (!(gr.cmask[depth] & (1u << nxt))) continue;
        if (cnt[nxt] + 1 > cap[nxt]) continue;
        stack[depth] = nxt; assign[depth] = nxt; cnt[nxt]++; placed = true; break;
      }
      if (!placed) { stack[depth] = -1; depth--; continue; }
      if (depth + 1 < gr.nCards) { depth++; stack[depth] = -1; continue; }
      uint64_t tp = packCnt(cnt);
      uint32_t h = uint32_t((tp * 0x9E3779B97F4A7C15ull) >> 52) & 4095u;
      int idx = -1;
      while (true) { int e = H[h]; if (e < 0) break; if (gr.tpack[e] == tp) { idx = e; break; } h = (h + 1) & 4095u; }
      if (idx < 0) {
        if (gr.nEnt >= MAXENT) return;
        idx = gr.nEnt++; H[h] = idx;
        gr.tpack[idx] = tp; gr.toff[idx] = offsetOf(cnt); gr.g[idx] = 0;
        memset(gr.gcard[idx], 0, sizeof(gr.gcard[idx]));
      }
      gr.g[idx] += 1.0f;
      for (int i = 0; i < gr.nCards; i++) gr.gcard[idx][i][assign[i]] += 1.0f;
    }
    for (int e = 0; e < gr.nEnt; e++) gr.S[e] = pathMass(gr, gr.tpack[e], gr.toff[e]);
    gr.sReady = true;
  }

  double teamOwnsProbability(int s, int teamMask) {
    if (!ensureCurrent()) return 0.0;
    Group* gr = groupForSet(s);
    if (!gr) return 1.0;
    ensureGroupTable(*gr);
    double acc = 0;
    for (int e = 0; e < gr->nEnt; e++) {
      uint64_t tp = gr->tpack[e];
      bool allTeam = true;
      for (int p = 0; p < NPLAY; p++)
        if (!(teamMask & (1 << p)) && ((tp >> (8 * p)) & 0xFF)) { allTeam = false; break; }
      if (allTeam) acc += double(gr->g[e]) * gr->S[e];
    }
    return acc / Z;
  }

  // Best allocation of a half-suit to one team, with its exact probability.
  double bestTeamAllocation(int s, int teamMask, int* outCards, int* outSeats, int& n) {
    n = 0;
    if (!ensureCurrent()) return 0.0;
    Group* gr = groupForSet(s);
    if (!gr) return 1.0;
    ensureGroupTable(*gr);
    int bestE = -1; double bestS = -1;
    for (int e = 0; e < gr->nEnt; e++) {
      uint64_t tp = gr->tpack[e];
      bool allTeam = true;
      for (int p = 0; p < NPLAY; p++)
        if (!(teamMask & (1 << p)) && ((tp >> (8 * p)) & 0xFF)) { allTeam = false; break; }
      if (!allTeam) continue;
      if (gr->S[e] > bestS) { bestS = gr->S[e]; bestE = e; }
    }
    if (bestE < 0) return 0.0;
    // any surviving assignment with this count vector is equally likely; take
    // the lexicographically first that gcard proves exists
    int want[NPLAY];
    for (int p = 0; p < NPLAY; p++) want[p] = int((gr->tpack[bestE] >> (8 * p)) & 0xFF);
    int assign[SETSZ];
    if (!pick(*gr, bestE, want, assign, 0)) return 0.0;
    n = gr->nCards;
    for (int i = 0; i < n; i++) { outCards[i] = gr->cards[i]; outSeats[i] = assign[i]; }
    return bestS / Z;
  }
  // Exact probability of a NAMED allocation of half-suit s.  Under the uniform
  // prior every SURVIVING assignment sharing a count vector is equally likely,
  // so P(A) = S_{t(A)} / Z -- but an assignment with a surviving count vector
  // need not itself satisfy the half-suit's certificates, so survival is checked
  // explicitly here rather than inferred from the count vector.  Used by the
  // brute-force oracle (`fish oracle`); not on any decision path.
  double allocationProbability(const Knowledge& k, int s, const int* seats) {
    if (!ensureCurrent()) return 0.0;
    Group* gr = groupForSet(s);
    if (!gr) return 1.0;
    ensureGroupTable(*gr);
    if (!gr->nEnt) return 0.0;
    int cnt[NPLAY] = {0,0,0,0,0,0};
    for (int i = 0; i < gr->nCards; i++) {
      int p = seats[i];
      if (p < 0 || p >= NPLAY || !(gr->cmask[i] & (1u << p))) return 0.0;
      cnt[p]++;
      if (cnt[p] > cap[p]) return 0.0;
    }
    // survival: every certificate confined to this half-suit must be satisfied
    for (const auto& d : k.disj) {
      uint64_t inBlock = d.cards & setMask(s) & k.unresolved;
      if (!inBlock) continue;
      bool sat = false;
      for (int i = 0; i < gr->nCards && !sat; i++)
        if ((inBlock & bit(gr->cards[i])) && seats[i] == d.player) sat = true;
      if (!sat) return 0.0;
    }
    uint64_t tp = packCnt(cnt);
    for (int e = 0; e < gr->nEnt; e++) if (gr->tpack[e] == tp) return gr->S[e] / Z;
    return 0.0;
  }

  // ---- v0.7 phase 3 (K2): the SHAPE of the allocation posterior -----------
  //
  // Ledger L1 asks for a measured ceiling, not another rule.  Under the
  // uniform-deal prior P(assignment A) = S_{t(A)} / Z, constant within a count
  // vector -- so the whole exact posterior over the assignments in which
  // `teamMask` holds all six cards of half-suit `s` is described by the list of
  // surviving team-only count vectors, their assignment counts g[e] and their
  // path masses S[e].  Two consequences that this struct exists to measure:
  //
  //   * if exactly ONE count vector survives, the exact posterior is UNIFORM
  //     over every feasible allocation, no belief under this prior can prefer
  //     one of them, and the best any rule can do is 1 / nAlloc.  That is an
  //     information limit, not a mechanism defect.
  //   * otherwise the exact ceiling on allocation accuracy at this state is
  //     max_e S[e] / sum_e g[e] S[e], attained by naming any assignment inside
  //     the heaviest count vector.
  //
  // Read-only: it builds nothing the queries above do not already build.
  struct AllocShape {
    bool ok = false;
    int  nCV = 0;            // team-only count vectors surviving
    double nAlloc = 0;       // assignments summed over them
    double nTopAlloc = 0;    // assignments sharing the heaviest count vector
    double pMap = 0;         // exact P(MAP assignment | team owns all six)
    bool flat = false;       // every survivor equally likely
    int  nCards = 0;
    int  cards[SETSZ];
    int  seats[SETSZ];       // one assignment from the heaviest count vector
  };
  AllocShape allocShape(int s, int teamMask) {
    AllocShape R;
    if (!ensureCurrent()) return R;
    Group* gr = groupForSet(s);
    if (!gr) { R.ok = true; R.nCV = 1; R.nAlloc = 1; R.nTopAlloc = 1; R.pMap = 1; R.flat = true; return R; }
    ensureGroupTable(*gr);
    int bestE = -1; double bestS = -1, zt = 0, minS = 1e300, maxS = -1;
    for (int e = 0; e < gr->nEnt; e++) {
      uint64_t tp = gr->tpack[e];
      bool allTeam = true;
      for (int p = 0; p < NPLAY; p++)
        if (!(teamMask & (1 << p)) && ((tp >> (8 * p)) & 0xFF)) { allTeam = false; break; }
      if (!allTeam) continue;
      R.nCV++;
      R.nAlloc += double(gr->g[e]);
      zt += double(gr->g[e]) * gr->S[e];
      if (gr->S[e] < minS) minS = gr->S[e];
      if (gr->S[e] > maxS) maxS = gr->S[e];
      if (gr->S[e] > bestS) { bestS = gr->S[e]; bestE = e; R.nTopAlloc = double(gr->g[e]); }
    }
    if (bestE < 0 || !(zt > 0)) return R;
    R.pMap = bestS / zt;
    // "Flat" is a statement about the whole surviving set, so it is the spread
    // of S over count vectors, not the size of the top one.
    R.flat = (R.nCV <= 1) || (maxS - minS <= 1e-9 * std::max(1.0, maxS));
    int want[NPLAY];
    for (int p = 0; p < NPLAY; p++) want[p] = int((gr->tpack[bestE] >> (8 * p)) & 0xFF);
    int assign[SETSZ];
    if (pick(*gr, bestE, want, assign, 0)) {
      R.nCards = gr->nCards;
      for (int i = 0; i < R.nCards; i++) { R.cards[i] = gr->cards[i]; R.seats[i] = assign[i]; }
    }
    R.ok = true;
    return R;
  }

  bool pick(const Group& gr, int e, int* want, int* assign, int i) const {
    if (i == gr.nCards) return true;
    for (int p = 0; p < NPLAY; p++) {
      if (!(gr.cmask[i] & (1u << p)) || want[p] <= 0) continue;
      if (gr.gcard[e][i][p] <= 0) continue;
      want[p]--; assign[i] = p;
      if (pick(gr, e, want, assign, i + 1)) return true;
      want[p]++;
    }
    return false;
  }
};

} // namespace fish
