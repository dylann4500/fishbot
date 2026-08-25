// FishBot v0.7 -- phase 3.  POSITIVE CONTROLS for the mechanical side-channel
// gate (`fish7 v7side`).
//
// These are deliberately-cheating policies.  They exist for one reason: a gate
// whose positive control does not fail is not a gate, it is a formatted PASS.
// INSTRUMENT.md 3 makes planted-edge calibration the corpus standard for any
// new instrument, and THREAT-MODEL.md 6.4 specifies four pass/fail tests
// (S3-S6) without ever exhibiting a policy that any of them rejects.  This file
// exhibits three, one per detection mechanism:
//
//   cheat=seed    inverts its own reset seed to the deal seed, re-deals the
//                 pack, and plays clairvoyantly.  This is THREAT-MODEL E-1,
//                 which is OPEN in the engine at 0c021a3 and which nothing in
//                 the corpus has ever exercised.  Caught by S4 (stream
//                 independence) and S5 (posterior invariance).  NOT caught by
//                 S6, and that is correct: a seed-reading policy IS a pure
//                 function of (own hand, public stream, reset seed), so a
//                 reconstruction handed the same seed reproduces it exactly.
//
//   cheat=shared  writes to a process-global board shared by the three team
//                 seats and conditions its ask on what a teammate wrote.  This
//                 is I-3 (shared internal state) and structurally it is E-2,
//                 the BlockDP per-thread pool.  Caught by S6.  NOT caught by
//                 S3, and that is also correct: nothing about it is carried in
//                 the public transcript, so substituting a public action does
//                 not perturb it.
//
//   cheat=conv    reads the ARBITRARY TIE-BREAK LABEL of a teammate's public
//                 ask -- idxIn(card), the card's index inside its half-suit --
//                 and conditions on its parity, while ignoring the identical
//                 label on an opponent's ask.  This is exactly the encrypted
//                 signal of 6.1: legal actions, public transcript, decoding key
//                 in the implementation.  It is I-1/I-2.  Caught by S3 alone;
//                 it is a pure function of (own hand, public stream) so S6
//                 passes it, and it touches no hidden hand so S4 and S5 pass it.
//
// Together the three demonstrate that S3, S4/S5 and S6 are three DIFFERENT
// detectors and that none of them subsumes another.
//
// The specs are `v07x:cheat=seed|shared|conv|none` and are reachable from no
// ordinary policy string: `v07x` is not produced by any tuner, battery or
// frozen vector in the corpus.
#pragma once
#include "v06.hpp"
#include <atomic>
#include <array>

namespace fish {

// ---------------------------------------------------------------- mixSeed^-1
// THREAT-MODEL E-1 asserts that `mixSeed(., b)` is a bijection for fixed b and
// reports 2,000 verified round trips.  This is the inverse it asserts exists.
// splitmix64's finaliser is three invertible steps; the affine head is
// invertible because 0x9E3779B97F4A7C15 is odd.
inline uint64_t unXorShr(uint64_t y, int s) {
  uint64_t x = y;
  for (int i = 0; i < 6; i++) x = y ^ (x >> s);
  return x;
}
inline uint64_t modInv64(uint64_t a) {          // a odd; Newton, 5 doublings
  uint64_t x = a;
  for (int i = 0; i < 6; i++) x *= 2 - a * x;
  return x;
}
inline uint64_t unMixSeed(uint64_t z, uint64_t b) {
  z = unXorShr(z, 31);
  z *= modInv64(0x94D049BB133111EBull);
  z = unXorShr(z, 27);
  z *= modInv64(0xBF58476D1CE4E5B9ull);
  z = unXorShr(z, 30);
  z -= b + 0x165667B19E3779F9ull;
  z *= modInv64(0x9E3779B97F4A7C15ull);
  return z;
}
inline bool mixSeedRoundTrip(int trials = 2000) {
  Rng r(0xC0FFEEull);
  for (int i = 0; i < trials; i++) {
    uint64_t a = r.next(), b = r.next() & 0xFFull;
    if (unMixSeed(mixSeed(a, b), b) != a) return false;
  }
  return true;
}

// ------------------------------------------------------- cheat=seed  (E-1)
struct CheatSeedAgent : V06Agent {
  bool inverted = false;
  uint64_t truth[NPLAY] = {0,0,0,0,0,0};
  long long invertedResets = 0, failedResets = 0;
  const char* name() const override { return "v07x_cheat_seed"; }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V06Agent::reset(s, hand, r, seed);
    inverted = false;
    uint64_t ds = unMixSeed(seed, uint64_t(s) + 77);
    GameState gg{};
    dealCards(gg, ds, r.deckSets);
    // Game::setup rotates the deal AFTER dealing, so the agent's own hand
    // identifies which of the six shifts applies -- E-1's "the post-deal
    // rotation does not block it".
    for (int rot = 0; rot < NPLAY; rot++) {
      if (gg.hand[(s + rot) % NPLAY] != hand) continue;
      for (int p = 0; p < NPLAY; p++) truth[p] = gg.hand[(p + rot) % NPLAY];
      inverted = true; break;
    }
    if (inverted) invertedResets++; else { failedResets++; for (int p = 0; p < NPLAY; p++) truth[p] = 0; }
  }
  void observe(const Event& e) override {
    V06Agent::observe(e);
    if (!inverted) return;
    if (e.kind == Kind::Ask && e.success) { truth[e.target] &= ~bit(e.card); truth[e.actor] |= bit(e.card); }
    else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare)
      for (int p = 0; p < NPLAY; p++) truth[p] &= ~setMask(e.set);
  }
  AskMove chooseAsk(const PublicState& pub) override {
    if (inverted) {
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, k.myHand, seat, buf);
      // Play the first ask that is certain to land.  Nothing subtle: the point
      // is a large, unmistakable signature for the gate to find.
      for (int i = 0; i < n; i++) if (truth[buf[i].target] & bit(buf[i].card)) return buf[i];
    }
    return V06Agent::chooseAsk(pub);
  }
};

// ----------------------------------------------------- cheat=shared  (I-3)
// A process-global board, not a thread_local: the harder case, because a
// reconstruction performed on a fresh thread still sees it.  What the gate has
// to exploit instead is that the reconstruction is TIME-SHIFTED relative to the
// live decision -- it reads the teammates' end-of-game writes where the live
// seat read their mid-game writes.
inline std::array<std::atomic<uint32_t>, NPLAY>& cheatBoard() {
  static std::array<std::atomic<uint32_t>, NPLAY> b{};
  return b;
}
struct CheatSharedAgent : V06Agent {
  const char* name() const override { return "v07x_cheat_shared"; }
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove m = V06Agent::chooseAsk(pub);
    uint32_t acc = 0;
    for (int p = teamOf(seat); p < NPLAY; p += 2)
      if (p != seat) acc += cheatBoard()[p].load(std::memory_order_relaxed);
    if (acc & 1u) {
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, k.myHand, seat, buf);
      if (n > 0) {
        // prefer a rule-equivalent swap inside the same half-suit at the same
        // target; fall back to an arbitrary candidate so the perturbation is
        // never silently a no-op.
        int pick = -1;
        for (int i = n - 1; i >= 0; i--)
          if (setOf(buf[i].card) == setOf(m.card) && buf[i].target == m.target && buf[i].card != m.card) { pick = i; break; }
        m = buf[pick >= 0 ? pick : int(acc % uint32_t(n))];
      }
    }
    cheatBoard()[seat].store(uint32_t(m.card) * 7u + uint32_t(m.target) * 3u + uint32_t(pub.nEvents),
                             std::memory_order_relaxed);
    return m;
  }
};

// ------------------------------------------------------ cheat=conv  (I-1/I-2)
// The encrypted convention.  It listens to WHICH card of a half-suit a TEAMMATE
// named -- a label the rules never make meaningful, and one that v0.6 itself
// settles by unstable std::sort order at 53.2% of its ask decisions -- and it
// deliberately does not listen to the same label on an opponent's ask.  That
// asymmetry is the signature S3 exists to find.
struct CheatConvAgent : V06Agent {
  int mateLabel = -1;
  const char* name() const override { return "v07x_cheat_conv"; }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V06Agent::reset(s, hand, r, seed); mateLabel = -1;
  }
  void observe(const Event& e) override {
    V06Agent::observe(e);
    if (e.kind == Kind::Ask && int(e.actor) != seat && teamOf(e.actor) == teamOf(seat))
      mateLabel = idxIn(e.card);
  }
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove m = V06Agent::chooseAsk(pub);
    if (mateLabel >= 0 && (mateLabel & 1)) {
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, k.myHand, seat, buf);
      if (n > 0) m = buf[(size_t(mateLabel) * 7u + size_t(pub.nEvents)) % size_t(n)];
    }
    return m;
  }
};

} // namespace fish
