// Exact public-information belief engine for Canadian Fish.
//
// KEY STRUCTURAL FACT.  Every card movement in Fish is public: a card changes
// hands only through a successful ask, which everyone observes.  Therefore a
// card whose location has never been publicly revealed is still with the player
// who was dealt it, and the ENTIRE hidden state is the initial deal.  An
// observer's posterior is the uniform distribution over deals consistent with:
//
//   (C1) own hand                    - exact
//   (C2) publicly transferred cards  - exact owner
//   (C3) exclusions                  - a miss proves the target lacks the card;
//                                      an ask proves the asker lacks it
//   (C4) capacities                  - player p holds exactly
//                                      handCount[p] - |known cards of p|
//                                      of the unresolved cards
//   (C5) ask legality                - an ask in half-suit S by A proves A held
//                                      at least one other card of S; because
//                                      unresolved cards never move, this is the
//                                      time-independent disjunction
//                                      OR_{d in D} [owner(d) = A]
//
// (C1)-(C4) define a degree-constrained bipartite counting problem whose exact
// marginals we obtain with a forward/backward dynamic program over capacity
// vectors.  Crucially the state space is bounded: sum_p q_p = |U| <= 45 with
// five unresolved owners, so prod_p (q_p+1) <= 10^5 by AM-GM, and the total DP
// work is O(prod_p (q_p+1) * 6) -- a few hundred thousand operations.  This
// replaces v0.3's Sinkhorn approximation with the exact posterior.
//
// (C5) is handled by exact rejection sampling from the (C1)-(C4) posterior,
// for which the same DP is an exact sampler.
#pragma once
#include "fish.hpp"

namespace fish {

static constexpr uint8_t OUT_OF_PLAY = 254;
static constexpr uint8_t UNKNOWN = 255;
static constexpr int DP_STATE_CAP = 400000;

struct Disjunction {          // "player holds at least one of these cards"
  uint8_t player;
  uint64_t cards;
};

// ---------------------------------------------------------------- knowledge
struct Knowledge {
  int me = 0;
  uint64_t myHand = 0;
  uint8_t owner[NCARD];             // seat, OUT_OF_PLAY, or UNKNOWN
  uint8_t mask[NCARD];              // possible owners for UNKNOWN cards
  uint64_t unresolved = 0;          // active-set cards with UNKNOWN owner
  uint8_t handCount[NPLAY];
  bool setActive[NSET];
  uint8_t askCount[NPLAY][NSET];    // public ask tallies (v0.2/v0.3 ports use these)
  uint8_t missCount[NPLAY][NSET];
  uint16_t totalAsks[NPLAY] = {0,0,0,0,0,0};
  uint64_t publicKnown = 0;   // cards whose location every player has seen
  std::vector<Disjunction> disj;
  int nSets = NSET;

  void init(int seat, uint64_t hand, int deckSets) {
    me = seat; myHand = hand; nSets = deckSets;
    disj.clear();
    unresolved = 0;
    memset(askCount, 0, sizeof(askCount));
    memset(missCount, 0, sizeof(missCount));
    memset(totalAsks, 0, sizeof(totalAsks));
    publicKnown = 0;
    for (int s = 0; s < NSET; s++) setActive[s] = (s < deckSets);
    for (int p = 0; p < NPLAY; p++) handCount[p] = uint8_t(deckSets * SETSZ / NPLAY);
    for (int c = 0; c < NCARD; c++) {
      if (setOf(c) >= deckSets) { owner[c] = OUT_OF_PLAY; mask[c] = 0; continue; }
      if (hand & bit(c)) { owner[c] = uint8_t(seat); mask[c] = uint8_t(1u << seat); }
      else { owner[c] = UNKNOWN; mask[c] = uint8_t(0x3F & ~(1u << seat)); unresolved |= bit(c); }
    }
  }

  inline int knownHeld(int p) const {
    int n = 0;
    for (int c = 0; c < NCARD; c++) if (owner[c] == p) n++;
    return n;
  }
  inline void capacities(uint8_t* q) const {
    int known[NPLAY] = {0,0,0,0,0,0};
    for (int c = 0; c < NCARD; c++) if (owner[c] < NPLAY) known[owner[c]]++;
    for (int p = 0; p < NPLAY; p++) {
      int v = int(handCount[p]) - known[p];
      q[p] = uint8_t(v > 0 ? v : 0);
    }
  }

  // Soft policy prior.  The hard certificates say a player who asked in a
  // half-suit holds at least one of its cards; they say nothing about the
  // player who has taken many turns and never asked there.  Under any sensible
  // policy that silence is evidence of a void, so we weight the uniform-deal
  // prior by
  //     w(c, p) = exp( theta * asks[p][suit(c)] - phi * (asks[p] - asks[p][suit(c)]) ),
  // clipped for stability.  Setting theta = phi = 0 recovers the policy-agnostic
  // posterior, which is what the ablation compares against.
  double priorWeight(int card, int p, double theta, double phi) const {
    if (theta == 0 && phi == 0) return 1.0;
    int S = setOf(card);
    double a = double(askCount[p][S]);
    double other = double(totalAsks[p]) - a;
    double z = theta * a - phi * other;
    if (z > 2.6) z = 2.6; else if (z < -2.6) z = -2.6;
    return std::exp(z);
  }

  // Capacity-only estimate that a half-suit is wholly owned by one team.  No
  // dynamic program: used to gate the expensive exact evaluation.
  double cheapTeamProb(int set, int teamMask) const {
    uint8_t q[NPLAY]; capacities(q);
    double prod = 1;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(set, i);
      if (owner[c] < NPLAY) { if (!(teamMask & (1u << owner[c]))) return 0.0; continue; }
      if (owner[c] == OUT_OF_PLAY) continue;
      double num = 0, den = 0;
      for (int p = 0; p < NPLAY; p++) if (mask[c] & (1u << p)) { den += q[p]; if (teamMask & (1u << p)) num += q[p]; }
      if (den <= 0) return 0.0;
      prod *= num / den;
      if (prod < 1e-6) return prod;
    }
    return prod;
  }

  // A resolved card lets us discharge or shrink every disjunction mentioning it.
  void resolveDisjunctions(int card) {
    uint8_t o = owner[card];
    for (size_t i = 0; i < disj.size();) {
      if (!(disj[i].cards & bit(card))) { i++; continue; }
      if (o == disj[i].player) { disj[i] = disj.back(); disj.pop_back(); continue; }
      disj[i].cards &= ~bit(card);
      if (!disj[i].cards) { disj[i] = disj.back(); disj.pop_back(); continue; }  // unreachable if consistent
      i++;
    }
  }

  void setOwner(int card, int p) {
    if (owner[card] == p) return;
    owner[card] = uint8_t(p);
    mask[card] = uint8_t(1u << p);
    unresolved &= ~bit(card);
    resolveDisjunctions(card);
  }
  void exclude(int card, int p) {
    if (owner[card] != UNKNOWN) return;
    mask[card] = uint8_t(mask[card] & ~(1u << p));
    if (__builtin_popcount(mask[card]) == 1) setOwner(card, __builtin_ctz(mask[card]));
  }

  void onEvent(const Event& e) {
    if (e.kind == Kind::Ask) {
      int S = setOf(e.card);
      askCount[e.actor][S] = uint8_t(std::min(255, askCount[e.actor][S] + 1));
      totalAsks[e.actor] = uint16_t(std::min(65535, totalAsks[e.actor] + 1));
      // (C5): the asker held another card of S.  Vacuous if we already know one.
      bool vacuous = false;
      uint64_t D = 0;
      for (int i = 0; i < SETSZ; i++) {
        int d = cardOf(S, i);
        if (d == e.card) continue;
        if (owner[d] == e.actor) { vacuous = true; break; }
        if (owner[d] == UNKNOWN && (mask[d] & (1u << e.actor))) D |= bit(d);
      }
      exclude(e.card, e.actor);                          // (C3) asker lacks the card
      if (!vacuous && D) {
        if (__builtin_popcountll(D) == 1) setOwner(__builtin_ctzll(D), e.actor);
        else disj.push_back(Disjunction{uint8_t(e.actor), D});
      }
      if (e.success) {
        // Reveal the PRE-transfer holder first: an unresolved card has never
        // moved, so its holder at every earlier time was the ask target, which
        // is what any outstanding legality disjunction refers to.
        setOwner(e.card, e.target);
        owner[e.card] = uint8_t(e.actor);
        mask[e.card] = uint8_t(1u << e.actor);
        publicKnown |= bit(e.card);
      } else {
        missCount[e.target][S] = uint8_t(std::min(255, missCount[e.target][S] + 1));
        exclude(e.card, e.target);
      }
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      int S = e.decl.set;
      if (e.success) {                                   // exact holders revealed
        for (int i = 0; i < SETSZ; i++) {
          int c = cardOf(S, i);
          if (owner[c] == UNKNOWN) setOwner(c, e.decl.owner[i]);
        }
      }
      // The set leaves play.  Residual count information is absorbed exactly by
      // the public hand counts, which drive the capacity constraint (C4).
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(S, i);
        owner[c] = OUT_OF_PLAY; mask[c] = 0;
      }
      unresolved &= ~setMask(S);
      setActive[S] = false;
      for (size_t i = 0; i < disj.size();) {
        if (disj[i].cards & setMask(S)) { disj[i] = disj.back(); disj.pop_back(); }
        else i++;
      }
      myHand &= ~setMask(S);
    }
    for (int p = 0; p < NPLAY; p++) handCount[p] = e.handCount[p];
    if (e.kind == Kind::Ask && e.success && e.actor == me) myHand |= bit(e.card);
    if (e.kind == Kind::Ask && e.success && e.target == me) myHand &= ~bit(e.card);
    propagateCapacity();
  }

  // Players with no spare capacity cannot own any unresolved card, and a card
  // whose mask collapses to one owner is resolved.  Iterate to a fixed point.
  void propagateCapacity() {
    for (int iter = 0; iter < 8; iter++) {
      uint8_t q[NPLAY]; capacities(q);
      bool changed = false;
      for (int p = 0; p < NPLAY; p++) {
        if (q[p]) continue;
        uint64_t u = unresolved;
        while (u) { int c = __builtin_ctzll(u); u &= u - 1;
          if (mask[c] & (1u << p)) { exclude(c, p); changed = true; } }
      }
      if (!changed) break;
    }
  }
};

// --------------------------------------------------------------------- DP
// Forward/backward count DP over capacity vectors.  F[v] counts assignments of
// the first (Q - |v|) unresolved cards leaving capacity v; B[v] counts
// completions of the remaining cards from capacity v.  Exact marginals are
// P(card i -> p) = sum_{|v| = Q-i} F[v] * B[v - e_p] / F[0], and the same
// tables give an exact sampler for whole deals.
struct DealDP {
  int Q = 0;
  int card[NCARD];
  uint8_t cmask[NCARD];
  uint8_t cap[NPLAY];
  int stride[NPLAY];
  int total = 0;
  bool ok = false;
  bool uniform = false;             // every unresolved card shares one mask
  double N = 0;
  // Reusable per-thread scratch; the state space is bounded by
  // prod_p (q_p + 1) <= 10^5 because sum_p q_p <= 45 over five owners.
  double *F = nullptr, *B = nullptr, *scr = nullptr;
  uint8_t *nz = nullptr, *nf = nullptr;
  int16_t *sumOf = nullptr;

  struct Buffers {
    std::vector<double> F, B, scr;
    std::vector<uint8_t> nz, nf;
    std::vector<int16_t> sumOf;
    void ensure(int n) {
      if ((int)F.size() >= n) return;
      F.resize(n); B.resize(n); scr.resize(n); nz.resize(n); nf.resize(n); sumOf.resize(n);
    }
  };
  static Buffers& buffers() { static thread_local Buffers b; return b; }

  bool build(const Knowledge& k) {
    Q = 0;
    uint64_t u = k.unresolved;
    uint8_t andMask = 0x3F, orMask = 0;
    while (u) { int c = __builtin_ctzll(u); u &= u - 1;
      card[Q] = c; cmask[Q] = k.mask[c]; andMask &= cmask[Q]; orMask |= cmask[Q]; Q++; }
    k.capacities(cap);
    int sum = 0; for (int p = 0; p < NPLAY; p++) sum += cap[p];
    if (sum != Q) { ok = false; return false; }
    uniform = (Q > 0 && andMask == orMask);
    if (!Q) { ok = true; uniform = true; N = 1; total = 0; return true; }
    total = 1;
    for (int p = 0; p < NPLAY; p++) {
      stride[p] = total;
      long long t = (long long)total * (cap[p] + 1);
      if (t > DP_STATE_CAP) { ok = false; return false; }
      total = int(t);
    }
    if (uniform) { ok = true; N = 1; return true; }   // marginals are q_p / Q
    Buffers& bf = buffers();
    bf.ensure(total);
    F = bf.F.data(); B = bf.B.data(); scr = bf.scr.data();
    nz = bf.nz.data(); nf = bf.nf.data(); sumOf = bf.sumOf.data();
    { int digit[NPLAY] = {0,0,0,0,0,0}; int s2 = 0;
      uint8_t mz = 0, mf = 0;
      for (int p = 0; p < NPLAY; p++) if (cap[p] > 0) mf |= uint8_t(1u << p);
      for (int st = 0; st < total; st++) {
        sumOf[st] = int16_t(s2); nz[st] = mz; nf[st] = mf;
        for (int p = 0; p < NPLAY; p++) {
          if (digit[p] + 1 <= cap[p]) { digit[p]++; s2++;
            mz |= uint8_t(1u << p);
            if (digit[p] == cap[p]) mf &= uint8_t(~(1u << p));
            break; }
          s2 -= digit[p]; digit[p] = 0;
          mz &= uint8_t(~(1u << p));
          if (cap[p] > 0) mf |= uint8_t(1u << p);
        }
      }
    }
    // forward, pull form: F[v] = sum_p [p in mask_{i-1}] F[v + e_p], i = Q - |v|
    F[total - 1] = 1.0;
    for (int st = total - 2; st >= 0; st--) {
      int i = Q - sumOf[st] - 1;                       // card that landed us here
      double acc = 0.0;
      uint8_t m = uint8_t(nf[st] & (i >= 0 && i < Q ? cmask[i] : 0));
      while (m) { int p = __builtin_ctz(m); m &= uint8_t(m - 1); acc += F[st + stride[p]]; }
      F[st] = acc;
    }
    N = F[0];
    if (!(N > 0)) { ok = false; return false; }
    // backward: B[v] = sum_p [p in mask_i] B[v - e_p], i = Q - |v|
    B[0] = 1.0;
    for (int st = 1; st < total; st++) {
      int i = Q - sumOf[st];
      double acc = 0.0;
      uint8_t m = uint8_t(nz[st] & (i >= 0 && i < Q ? cmask[i] : 0));
      while (m) { int p = __builtin_ctz(m); m &= uint8_t(m - 1); acc += B[st - stride[p]]; }
      B[st] = acc;
    }
    ok = true;
    return true;
  }

  void marginals(double out[NCARD][NPLAY]) const {
    if (uniform) {
      if (!Q) return;
      for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++)
        out[card[i]][p] = (cmask[i] & (1u << p)) ? double(cap[p]) / double(Q) : 0.0;
      return;
    }
    double acc[NCARD][NPLAY];
    for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++) acc[i][p] = 0.0;
    for (int st = 0; st < total; st++) {
      double f = F[st];
      if (f == 0.0) continue;
      int i = Q - sumOf[st];
      if (i < 0 || i >= Q) continue;
      uint8_t m = uint8_t(nz[st] & cmask[i]);
      while (m) { int p = __builtin_ctz(m); m &= uint8_t(m - 1); acc[i][p] += f * B[st - stride[p]]; }
    }
    for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++) out[card[i]][p] = acc[i][p] / N;
  }

  void sample(Rng& rng, uint8_t* owners) const {
    if (uniform) {                                     // sequential without replacement
      int rem[NPLAY];
      for (int p = 0; p < NPLAY; p++) rem[p] = cap[p];
      for (int i = 0; i < Q; i++) {
        int tot = 0;
        for (int p = 0; p < NPLAY; p++) if (cmask[i] & (1u << p)) tot += rem[p];
        int r = int(rng.u32(uint32_t(tot > 0 ? tot : 1))), chosen = -1;
        for (int p = 0; p < NPLAY; p++) if (cmask[i] & (1u << p)) { r -= rem[p]; chosen = p; if (r < 0) break; }
        owners[card[i]] = uint8_t(chosen);
        if (chosen >= 0) rem[chosen]--;
      }
      return;
    }
    int st = total - 1;
    for (int i = 0; i < Q; i++) {
      uint8_t m = uint8_t(nz[st] & cmask[i]);
      double tot = 0, w[NPLAY] = {0,0,0,0,0,0};
      uint8_t mm = m;
      while (mm) { int p = __builtin_ctz(mm); mm &= uint8_t(mm - 1); w[p] = B[st - stride[p]]; tot += w[p]; }
      double r = rng.uni() * tot;
      int chosen = -1;
      mm = m;
      while (mm) { int p = __builtin_ctz(mm); mm &= uint8_t(mm - 1); chosen = p; r -= w[p]; if (r <= 0) break; }
      owners[card[i]] = uint8_t(chosen);
      st -= stride[chosen];
    }
  }

  double countMasked(const uint8_t* m2) {
    scr[total - 1] = 1.0;
    for (int st = total - 2; st >= 0; st--) {
      int i = Q - sumOf[st] - 1;
      double acc = 0.0;
      uint8_t m = uint8_t(nf[st] & (i >= 0 && i < Q ? m2[i] : 0));
      while (m) { int p = __builtin_ctz(m); m &= uint8_t(m - 1); acc += scr[st + stride[p]]; }
      scr[st] = acc;
    }
    return scr[0];
  }

  // Exact count of deals in which each listed card's owner lies in the mask.
  // In the uniform case every unresolved card shares one mask, so the deal is a
  // uniform partition of Q exchangeable cards into capacity slots and the answer
  // is a falling-factorial (multivariate hypergeometric) probability, returned
  // directly with N == 1.
  double countWithMasks(const int* cards, const uint8_t* allow, int n) {
    if (uniform) {
      bool allEqual = true;
      for (int j = 1; j < n; j++) if (allow[j] != allow[0]) allEqual = false;
      int idx[SETSZ * 2], m = 0;
      for (int j = 0; j < n; j++) {
        int id = -1;
        for (int i = 0; i < Q; i++) if (card[i] == cards[j]) { id = i; break; }
        if (id >= 0) idx[m++] = j;
      }
      if (!m) return 1.0;
      if (allEqual) {
        int qT = 0;
        for (int p = 0; p < NPLAY; p++) if ((cmask[0] & allow[0]) & (1u << p)) qT += cap[p];
        double pr = 1.0;
        for (int i = 0; i < m; i++) {
          double num = double(qT - i), den = double(Q - i);
          if (num <= 0 || den <= 0) return 0.0;
          pr *= num / den;
        }
        return pr;
      }
      int used[NPLAY] = {0,0,0,0,0,0};
      double pr = 1.0;
      for (int i = 0; i < m; i++) {
        int j = idx[i];
        int single = __builtin_popcount(allow[j] & cmask[0]) == 1 ? __builtin_ctz(allow[j] & cmask[0]) : -1;
        int num = 0;
        for (int p = 0; p < NPLAY; p++) if ((allow[j] & cmask[0]) & (1u << p)) num += cap[p] - used[p];
        int den = Q - i;
        if (num <= 0 || den <= 0) return 0.0;
        pr *= double(num) / double(den);
        if (single >= 0) used[single]++;
        else { int best = -1;
          for (int p = 0; p < NPLAY; p++) if (((allow[j] & cmask[0]) & (1u << p)) && (best < 0 || cap[p] - used[p] > cap[best] - used[best])) best = p;
          if (best >= 0) used[best]++; }
      }
      return pr;
    }
    uint8_t m2[NCARD];
    for (int i = 0; i < Q; i++) m2[i] = cmask[i];
    for (int j = 0; j < n; j++)
      for (int i = 0; i < Q; i++) if (card[i] == cards[j]) {
        m2[i] = uint8_t(cmask[i] & allow[j]);
        if (!m2[i]) return 0.0;
      }
    return countMasked(m2);
  }

  double countWith(const int* forceCard, const int* forcePlayer, int nForce) {
    uint8_t allow[SETSZ * 2];
    for (int j = 0; j < nForce; j++) allow[j] = uint8_t(1u << forcePlayer[j]);
    return countWithMasks(forceCard, allow, nForce);
  }
};

// --------------------------------------------------- disjunction-aware belief
// Exact rejection sampling: draw deals from the (C1)-(C4) posterior with the DP
// sampler and keep those satisfying every (C5) disjunction.  Accepted draws are
// exact samples of the full posterior.
struct Belief {
  const Knowledge* k = nullptr;
  DealDP dp;
  double marg[NCARD][NPLAY];
  bool dpOk = false;
  int nParticles = 0;
  double acceptRate = 1.0;
  std::vector<std::array<uint8_t, NCARD>> particles;

  static bool satisfies(const Knowledge& kk, const uint8_t* owners) {
    for (const auto& d : kk.disj) {
      uint64_t c = d.cards; bool sat = false;
      while (c) { int x = __builtin_ctzll(c); c &= c - 1; if (owners[x] == d.player) { sat = true; break; } }
      if (!sat) return false;
    }
    return true;
  }

  // Fast posterior: Sinkhorn (iterative proportional fitting) enforces the exact
  // capacity constraints (C4) on the exact support (C1-C3), interleaved with a
  // conditioning step for the ask-legality disjunctions (C5).  For a constraint
  // "player A holds at least one of D", independence conditioning gives
  //     P(s(d)=A | E) = p_{d,A} / (1 - prod_{e in D}(1 - p_{e,A})),
  //     P(s(d)=p | E) = p_{d,p} * (1 - prod_{e in D\{d}}(1 - p_{e,A})) / (1 - P(no A)),
  // after which capacity fitting is re-run.  Four outer sweeps converge in
  // practice.  This is O(iters * |U| * 6) rather than the O(prod (q_p+1))
  // exact dynamic program, and is measured against it in the calibration study.
  void sinkhornDisj(const Knowledge& kk, int outer, int inner, double theta = 0, double phi = 0) {
    uint8_t q[NPLAY]; kk.capacities(q);
    int idx[NCARD], Q = 0;
    uint64_t u = kk.unresolved;
    while (u) { int c = __builtin_ctzll(u); u &= u - 1; idx[Q++] = c; }
    if (!Q) return;
    for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++)
      marg[idx[i]][p] = (kk.mask[idx[i]] & (1u << p)) ? kk.priorWeight(idx[i], p, theta, phi) : 0.0;
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
        int list[SETSZ]; int m = 0;
        while (cc) { int x = __builtin_ctzll(cc); cc &= cc - 1;
          if (!(kk.unresolved & bit(x))) continue;
          list[m++] = x; pNone *= (1.0 - marg[x][A]); }
        if (!m) continue;
        double pAny = 1.0 - pNone;
        if (pAny < 1e-9) {                       // degenerate: force the mass in
          for (int j = 0; j < m; j++) { for (int p = 0; p < NPLAY; p++) marg[list[j]][p] *= .001;
            marg[list[j]][A] += 1.0 / m; }
          continue;
        }
        if (pNone < 1e-12) continue;             // already implied
        for (int j = 0; j < m; j++) {
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

  // Joint probability of a full allocation under the fast posterior, computed by
  // sequential conditioning: fix one card at a time, decrement that owner's
  // capacity, and refit.  Exact in structure, approximate only in the marginal
  // solver, and validated against the exact dynamic program.
  double jointSequential(const Knowledge& kk, const int* cards, const int* players, int n,
                         int outer, int inner, double theta = 0, double phi = 0) {
    Knowledge tmp = kk;
    double pr = 1.0;
    Belief scratchB;
    for (int i = 0; i < n; i++) {
      int c = cards[i], p = players[i];
      if (tmp.owner[c] < NPLAY) { if (tmp.owner[c] != p) return 0.0; continue; }
      if (!(tmp.mask[c] & (1u << p))) return 0.0;
      scratchB.sinkhornDisj(tmp, outer, inner, theta, phi);
      double v = scratchB.marg[c][p];
      if (v <= 0) return 0.0;
      pr *= v;
      if (pr < 1e-9) return pr;
      tmp.setOwner(c, p);
      tmp.propagateCapacity();
    }
    return pr;
  }

  // Greedy sequential MAP allocation under the fast posterior.  Choosing each
  // card's owner by its *unconditioned* marginal is not the same as maximising
  // the joint, because the six cards of a half-suit are coupled through the
  // capacity constraint; conditioning as we go is strictly better at the same
  // cost, and it is what the exact engine does by construction.
  double jointSequentialMAP(const Knowledge& kk, const int* cards, int n, int teamMask,
                            int* outPlayers, int outerIt, int innerIt, double theta, double phi) {
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
      scratchB.sinkhornDisj(tmp, outerIt, innerIt, theta, phi);
      int best = -1; double bestP = -1;
      for (int q = 0; q < NPLAY; q++) {
        if (!(teamMask & (1u << q))) continue;
        double v = scratchB.marg[c][q];
        if (v > bestP) { bestP = v; best = q; }
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

  // Fast fallback when the DP is unavailable: Sinkhorn scaling on the exact
  // support, which is the maximum-entropy approximation to the same posterior.
  void sinkhorn(const Knowledge& kk) {
    uint8_t q[NPLAY]; kk.capacities(q);
    int idx[NCARD], Q = 0;
    uint64_t u = kk.unresolved;
    while (u) { int c = __builtin_ctzll(u); u &= u - 1; idx[Q++] = c; }
    for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++)
      marg[idx[i]][p] = (kk.mask[idx[i]] & (1u << p)) ? 1.0 : 0.0;
    for (int it = 0; it < 40; it++) {
      for (int i = 0; i < Q; i++) { double t = 0; for (int p = 0; p < NPLAY; p++) t += marg[idx[i]][p];
        if (t > 0) for (int p = 0; p < NPLAY; p++) marg[idx[i]][p] /= t; }
      double col[NPLAY] = {0,0,0,0,0,0};
      for (int i = 0; i < Q; i++) for (int p = 0; p < NPLAY; p++) col[p] += marg[idx[i]][p];
      for (int p = 0; p < NPLAY; p++) { double s = col[p] > 0 ? q[p] / col[p] : 0;
        for (int i = 0; i < Q; i++) marg[idx[i]][p] *= s; }
    }
    for (int i = 0; i < Q; i++) { double t = 0; for (int p = 0; p < NPLAY; p++) t += marg[idx[i]][p];
      if (t > 0) for (int p = 0; p < NPLAY; p++) marg[idx[i]][p] /= t; }
  }

  // Build the exact DP on demand (declaration decisions only, under hybrid).
  bool ensureDP(const Knowledge& kk, Rng& rng, int wantParticles, bool useDisjunctions) {
    if (dpBuilt) return dpOk;
    dpBuilt = true;
    k = &kk;
    if (!kk.unresolved) { dpOk = true; return true; }
    dpOk = dp.build(kk);
    if (!dpOk) return false;
    if (useDisjunctions && !kk.disj.empty()) {
      particles.clear(); nParticles = 0;
      std::array<uint8_t, NCARD> buf{};
      for (int c = 0; c < NCARD; c++) buf[c] = kk.owner[c];
      int want = std::max(wantParticles, 64);
      int tries = 0, maxTries = want * 30 + 200;
      while (nParticles < want && tries < maxTries) {
        tries++;
        dp.sample(rng, buf.data());
        if (!satisfies(kk, buf.data())) continue;
        particles.push_back(buf); nParticles++;
      }
      acceptRate = tries ? double(nParticles) / double(tries) : 0.0;
    }
    return dpOk;
  }

  bool dpBuilt = false;

  void compute(const Knowledge& kk, Rng& rng, int wantParticles, bool useDisjunctions) {
    dpBuilt = true;
    k = &kk;
    for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) marg[c][p] = 0.0;
    for (int c = 0; c < NCARD; c++) if (kk.owner[c] < NPLAY) marg[c][kk.owner[c]] = 1.0;
    particles.clear(); nParticles = 0; acceptRate = 1.0;
    if (!kk.unresolved) { dpOk = true; return; }
    dpOk = dp.build(kk);
    if (!dpOk) { sinkhorn(kk); return; }
    dp.marginals(marg);
    bool needParticles = wantParticles > 0;
    bool needDisj = useDisjunctions && !kk.disj.empty();
    if (!needParticles && !needDisj) return;
    int want = std::max(wantParticles, needDisj ? 64 : 0);
    std::array<uint8_t, NCARD> buf{};
    for (int c = 0; c < NCARD; c++) buf[c] = kk.owner[c];
    int tries = 0, maxTries = want * 40 + 200;
    while (nParticles < want && tries < maxTries) {
      tries++;
      dp.sample(rng, buf.data());
      if (needDisj && !satisfies(kk, buf.data())) continue;
      particles.push_back(buf);
      nParticles++;
    }
    acceptRate = tries ? double(nParticles) / double(tries) : 0.0;
    if (needDisj && nParticles >= 8) {                    // exact posterior marginals
      double acc[NCARD][NPLAY];
      for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) acc[c][p] = 0.0;
      for (const auto& pt : particles) {
        uint64_t u = kk.unresolved;
        while (u) { int c = __builtin_ctzll(u); u &= u - 1; acc[c][pt[c]] += 1.0; }
      }
      uint64_t u = kk.unresolved;
      while (u) { int c = __builtin_ctzll(u); u &= u - 1;
        for (int p = 0; p < NPLAY; p++) marg[c][p] = acc[c][p] / nParticles; }
    }
  }

  inline double P(int card, int player) const { return marg[card][player]; }

  // Probability that every named card sits with the named seat.  Exact from the
  // DP when no disjunction is active, otherwise the particle estimate.
  double jointProbability(const Knowledge& kk, const int* cards, const int* players, int n,
                          bool useDisjunctions) {
    int fc[SETSZ * 2], fp[SETSZ * 2], nf = 0;
    for (int i = 0; i < n; i++) {
      if (kk.owner[cards[i]] < NPLAY) { if (kk.owner[cards[i]] != players[i]) return 0.0; continue; }
      if (!(kk.mask[cards[i]] & (1u << players[i]))) return 0.0;
      fc[nf] = cards[i]; fp[nf] = players[i]; nf++;
    }
    if (!nf) return 1.0;
    if (useDisjunctions && !kk.disj.empty() && nParticles >= 8) {
      int hit = 0;
      for (const auto& pt : particles) {
        bool all = true;
        for (int i = 0; i < nf; i++) if (pt[fc[i]] != fp[i]) { all = false; break; }
        if (all) hit++;
      }
      return double(hit) / double(nParticles);
    }
    if (!dpOk) { double p = 1.0; for (int i = 0; i < nf; i++) p *= marg[fc[i]][fp[i]]; return p; }
    return dp.countWith(fc, fp, nf) / dp.N;
  }
};

} // namespace fish
