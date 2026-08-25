// FishBot v0.7 -- C6, the scripted-adaptive class.
//
// THREAT-MODEL.md T5 defines C6 as "hand-built manoeuvres with an ONLINE MODEL
// OF THE TARGET'S POLICY that updates within a match", and records that the
// class is empty: the scripted baselines carry a within-match model of their
// opponents' ASK TALLY (baselines.hpp:61, :262) but none of them models the
// target's POLICY, best-responds to it, or carries anything across matches.
// INSTRUMENT.md 1 defers the class to phase 2.  This is the class.
//
// THE MANOEUVRE, and why this one.
//
// v0.6's declaration rule (v05.hpp, declareNow) is:
//
//     if (urgent) return v.pAlloc >= declThreshold || (locked && v.pAlloc >= 0.5);
//     return declareByValue(pub, v);
//
// so URGENCY replaces an expected-value comparison with a bare threshold, and on
// a LOCKED half-suit -- one the team provably owns outright -- that threshold is
// a COIN FLIP on the allocation.  That is exactly the error class ledger entry
// L1 sizes: the team held all six cards and named the wrong teammate, which is
// 88.1% [81.0, 94.4] of v0.6's remaining misdeclarations (INSTRUMENT.md 1.10).
// Measured per decision in the v0.6 mirror, declarations taken under urgency are
// right 95.7% of the time against 99.1% for declarations taken with time in
// hand.
//
// `urgent` is a disjunction of four clauses (v05.hpp):
//     unresolvedCount <= patiencePool(5)
//     oppCards        <= oppCardFloor(2.99879)
//     pub.nEvents     >= forceDeclareEvents(220)
//     bestAskProbability < askFloor(0.26573)
// and the second is the OPPOSING TEAM'S OWN TOTAL HAND COUNT, which is public
// (PublicState::handCount) and which the adversary controls directly: its hand
// count falls by six every time it declares a half-suit.  The first clause is
// also pushed by the same act, because a declared half-suit leaves every seat's
// unresolved set.  The thresholds are frozen constants of a published policy, so
// a white-box adversary knows exactly where they are.
//
// So the manoeuvre is a TIMING one, and it is adaptive rather than scripted-blind:
// hold your own declarations while the target has nothing to lose by declaring,
// and release them -- crossing its urgency thresholds -- at the moment the target
// is holding a half-suit it owns outright but cannot allocate.  The online model
// is the second half: "is the target in that state right now" is estimated every
// decision from the adversary's own posterior, which is a model of what the
// target knows, and the estimate updates on every public event.
//
// The manoeuvre is not free: holding a declaration is the `decl` handicap family
// applied to oneself, and phase 1 measured that family at +0.62 to +2.45 points
// of cost to the holder.  Whether the trade is positive is the experiment.
#pragma once
#include "v06.hpp"

namespace fish {

struct V07AdaptAgent : V06Agent {
  // --- knobs -------------------------------------------------------------
  int    holdOn    = 1;      // 0 = the class reduces to v0.6 exactly (identity control)
  int    holdMax   = 60;     // never hold a declaration past this many public events
  double lockProb  = 0.90;   // "the target owns this half-suit outright" threshold
  int    ambigMin  = 2;      // ... and this many of its cards have >1 possible owner
  // The manoeuvre has two polarities and the first smoke test settled which one
  // is worth running.  HOLDING declarations while the target is safe (mode 1)
  // is the `decl` handicap family applied to oneself and it measured -2.1 to
  // -3.4 points against `v06` at n = 1,600 without moving the target's
  // declaration accuracy at all: the cost of an unclaimed half-suit dominates
  // whatever the timing buys.  ACCELERATING them while the target is vulnerable
  // (mode 2) is the other polarity: it crosses two of the target's four urgency
  // clauses at once, because a declared half-suit takes six cards out of the
  // adversary's hands (oppCards) AND out of every seat's unresolved set
  // (patiencePool), and it costs only the option value of waiting.
  int    mode      = 1;      // 0 off (identity) | 1 hold-while-safe | 2 accelerate-when-vulnerable | 3 both
  double aggrMargin = -0.50; // the declareMargin used while the target is vulnerable
  int    dumpOn    = 1;      // reserved
  // --- diagnostics, read-only ---------------------------------------------
  long long heldDecls = 0, releasedDecls = 0, vulnSeen = 0, declOpportunities = 0;

  V07AdaptAgent() { label = "fishbot_v07c"; }

  // The online model: does the OPPOSING team hold a half-suit outright that the
  // public record cannot allocate?  Both halves are computed from the
  // adversary's own Knowledge and Belief, which are functions of the public
  // transcript and of the adversary's own hand -- never of the target's hand.
  bool targetVulnerable(const PublicState& pub) {
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      // (a) does the opposing team own it outright?
      double p = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        double m = 0;
        if (k.owner[c] < NPLAY) m = (oppMask & (1u << k.owner[c])) ? 1.0 : 0.0;
        else for (int q = 0; q < NPLAY; q++) if (oppMask & (1 << q)) m += bel.marg[c][q];
        p += m;
      }
      if (p < lockProb * double(SETSZ)) continue;
      // (b) is the allocation among their three seats still open?  A card is
      // ambiguous when the public certificates leave more than one of their
      // seats able to hold it.  `k.mask` is exactly that certificate structure.
      int ambig = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        if (k.owner[c] < NPLAY) continue;
        int n = 0;
        for (int q = 0; q < NPLAY; q++) if ((oppMask & (1 << q)) && (k.mask[c] & (1u << q))) n++;
        if (n >= 2) ambig++;
      }
      if (ambig >= ambigMin) return true;
    }
    return false;
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (!holdOn || mode == 0) return V06Agent::proposeDeclaration(pub, d, conf);
    const bool vuln = targetVulnerable(pub);
    if (vuln) vulnSeen++;
    // Mode 2/3: while the target is vulnerable, declare on a materially easier
    // bar.  `declareMargin` is the only knob in the shipped rule that trades
    // the option value of waiting against cashing now, and it is restored
    // immediately, so the manoeuvre is a per-decision deviation and not a
    // different policy.
    const double saved = cfg.declareMargin;
    if ((mode == 2 || mode == 3) && vuln) cfg.declareMargin = aggrMargin;
    bool want = V06Agent::proposeDeclaration(pub, d, conf);
    cfg.declareMargin = saved;
    if (!want) return false;
    declOpportunities++;
    if (mode == 2) { releasedDecls++; return true; }
    // Mode 1/3: hold while the target has nothing to lose.  Past the hold
    // horizon the manoeuvre is abandoned rather than allowed to cost the whole
    // game -- an unclaimed half-suit scores nothing, which is the same reason
    // v0.5's own stopping rule escalates on the event count.
    if (int(pub.nEvents) >= holdMax) { releasedDecls++; return true; }
    if (vuln) { releasedDecls++; return true; }
    heldDecls++;
    return false;
  }

  // Once the target IS vulnerable, the point is to cross its thresholds fast,
  // so the willingness bit in the forced endgame is left alone (it is the one
  // intra-team channel the engine sanctions) and the acceleration is done where
  // it is legal and public: the voluntary declaration above.
};

} // namespace fish
