// FishBot v0.7 phase 3, candidate K3 -- a termination rule that is not a cliff.
//
// WHAT THIS REPLACES.  v0.5's `pressure()` is the corpus's termination
// guarantee.  Theorem 1 makes patience correct, so two patient policies can
// freeze the position permanently: once no productive ask remains, no further
// information can arrive, and a half-suit whose allocation is unresolved stays
// unresolved forever.  v0.5 breaks that with a CLOCK -- `pub.nEvents >= 220`
// drops the declaration rule's team-ownership floor from 0.849 to 0.25, and
// `forceStage2` at 308 cashes the best candidate whatever it is.  Phase 2
// measured what those two rungs are worth to an opponent who can reach them:
// +15.18 and +39.75, with declaration accuracy collapsing to 0.8115 and 0.4893.
// The clock is a cliff, and the escalation that guards the cliff is also the
// only thing that bounds the game.
//
// THE OBSERVATION.  The event counter is a PROXY for a condition -- "nothing is
// happening any more".  The condition itself is observable from the seat's own
// deduction state, and observing it directly is both tighter and safe: an
// ordinary position produces a new certificate almost every event, so a rule
// that fires only on the absence of certificates never fires in ordinary play,
// and therefore costs no declaration accuracy there.
//
// THE RULE.  Each seat hashes its own HARD deduction state -- `owner[]`,
// `mask[]`, `unresolved`, `handCount[]`, `setActive[]`.  That is exactly the set
// of C1-C5 certificates it holds.  Every public event that changes the hash is
// PROGRESS and resets a per-seat counter.  `stall=K` escalates to pressure
// stage 1 after K consecutive public events with no progress, and to stage 2
// after `stall2` (default 2K).  The clock rungs are left in place underneath and
// are normally disabled alongside (`force=1000000`).
//
// WHY IT TERMINATES.  Write P = (total popcount of mask[] over live cards)
// + (number of live half-suits) + (number of unresolved cards).  Certificates
// only accumulate: a mask bit, once cleared by a certificate, is never set
// again; a resolved card is never unresolved again (an unresolved card cannot
// move, because a card only moves on a successful ask and a successful ask
// reveals it publicly); a half-suit, once declared, never returns.  Each such
// event strictly decreases P, P is a non-negative integer bounded by
// 54*5 + 9 + 54, and there are therefore at most that many of them in a game.
//
// AND WHAT THAT ARGUMENT DOES NOT COVER -- stated because the first draft of
// this comment overclaimed it.  The hash also contains `handCount[]`, so a
// SUCCESSFUL ask is always scored as progress even though a card moving between
// two seats is not a monotone gain: nothing above forbids a card being won back
// later.  So the rule does not by itself bound the number of successful asks,
// and "the game is therefore bounded" is proved only for the mode of
// non-termination that actually occurs -- a run of asks that produce no new
// certificate, which is exactly v0.4's dead-ask loop and exactly what the
// urgency-off configurations regress into.  Excluding `handCount[]` would make
// the argument total but would also make a genuine transfer invisible to the
// detector, which is worse.  The bound that carries is therefore EMPIRICAL, and
// it is measured in MIRROR in the pathology gate: on the worst freezer in the
// corpus (`m1=0` + urgency-off) the self-play tail goes from 405 events and a
// 326-ask dead run to 141 events and a 12-ask dead run, with the action-limit
// game count going 2 -> 0.  Report the distribution, never the mean.
//
// WHAT THIS IS NOT.  It is not a new information channel.  The counter is a
// function of (own hand, public event stream) alone -- the same argument that
// makes `fish7 v7side`'s S6 seat-isolation test pass -- and it is recomputed
// independently by `diag.hpp`'s replay from the public trace, which is a
// mechanical check of exactly that.
#pragma once
#include "belief.hpp"
#include <atomic>

namespace fish {

// The hard deduction state, hashed.  Deliberately EXCLUDES `disj`: a repeated
// ask appends a duplicate disjunction carrying no information (Knowledge's own
// `dedupDisj` comment), so counting `disj.size()` as progress would make the
// detector unable to see the one loop it exists to break.  A genuinely NEW
// distinct disjunction is information and is handled separately, behind
// `stallsoft=1`.
inline uint64_t k3HardSig(const Knowledge& k) {
  uint64_t h = 1469598103934665603ull;
  auto mix = [&](uint64_t v) { h ^= v; h *= 1099511628211ull; };
  for (int c = 0; c < NCARD; c++)
    mix(uint64_t(k.owner[c]) | (uint64_t(k.mask[c]) << 8) | (uint64_t(unsigned(c)) << 16));
  mix(k.unresolved);
  for (int p = 0; p < NPLAY; p++) mix(uint64_t(k.handCount[p]) | (uint64_t(unsigned(p)) << 8));
  uint64_t sa = 0;
  for (int s = 0; s < NSET; s++) if (k.setActive[s]) sa |= 1ull << s;
  mix(sa);
  return h;
}

// A single disjunction, hashed, so "have I seen this certificate before?" is a
// 64-bit comparison.  Used only when `stallsoft=1`.
inline uint64_t k3DisjSig(const Disjunction& d) {
  uint64_t h = 1099511628211ull;
  auto mix = [&](uint64_t v) { h ^= v; h *= 1469598103934665603ull; };
  mix(uint64_t(d.player));
  mix(d.cards);
  return h;
}

// Diagnostics.  Off unless a stall key was parsed, so `v06` prints exactly what
// it printed before -- the identity control compares pathology output byte for
// byte.
struct K3StallStats {
  std::atomic<bool>      on{false};
  std::atomic<int>       K{0}, K2{0}, soft{0};   // the parsed configuration
  std::atomic<long long> declOpps{0};    // proposeDeclaration calls with stall armed
  std::atomic<long long> stage1{0};      // ... at which the stall rule reached press 1
  std::atomic<long long> stage2{0};      // ... press 2
  std::atomic<long long> clockStage1{0}; // ... at which the CLOCK reached press 1
  std::atomic<long long> maxStall{0};    // longest observed no-progress run
  void reset() { declOpps = 0; stage1 = 0; stage2 = 0; clockStage1 = 0; maxStall = 0; }
};
inline K3StallStats& k3stall() { static K3StallStats s; return s; }

} // namespace fish
