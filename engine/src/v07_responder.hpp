// FishBot v0.7 -- the responder classes.
//
// The corpus's only exploitability instrument fits a responder from the same
// 34-coordinate linear family with the frozen target as its entire panel
// (engine/exploitability_v06.sh).  Its detection floor has never been measured,
// it is blind to out-of-class exploits by construction, and against v0.6 the
// responder it produced was itself DEGRADED -- declaration accuracy 0.9550
// against 0.9826 for the v0.5-targeting one, and roughly sevenfold the
// forced-endgame rate (SUBOPTIMALITY-LEDGER.md P-3).  So the 48.36% is at least
// partly a measurement of a broken exploiter.
//
// This file adds the two classes the threat model names and the corpus has
// never built:
//
//   C2  V07Responder    -- the extended-feature class.  v0.6's score plus
//                          twelve coordinates it does not have: per-target
//                          terms, opponent-hand modelling beyond the single
//                          card, seat-role terms, deviation timing, and a
//                          linear price for the deliberate miss.
//   C5  V07InvertAgent  -- the white-box class.  v0.6's policy with the deal
//                          posterior sharpened by inverting the target's
//                          observed transcript against its known deterministic
//                          policy (v07_invert.hpp).
//
// C1 is `v06`/`v05` refit at higher budget and needs no new code; C3 is
// `v06:s1=1,...,roppo=<target>`, which is the existing rollout machinery with
// the opponent seats modelled as the target instead of as a mirror of the
// searcher (v06_rollout.hpp RolloutConfig::oppSpec).
//
// IDENTITY CONTROL.  Both classes reduce to v0.6 exactly: `v07` with its twelve
// responder weights at zero is `v06` bit for bit, and `v07i` with inversion off
// is `v06` bit for bit.  That is the project's standing convention -- the
// ablation table is exact rather than approximate -- and it is checked by
// `fish v7identity` rather than asserted.
#pragma once
#include "v06.hpp"
#include "v07_invert.hpp"

namespace fish {

// v0.7 phase 2 raises this from twelve to sixteen.  The four new coordinates are
// the INFORMATION-DENIAL group and they exist because a grep across v04.hpp,
// v05.hpp, v06.hpp and v07_responder.hpp finds no term anywhere in the lineage
// that prices what an ask hands to the OPPOSING TEAM.  Every information feature
// in the family (f[9] leak, f[16] exposure on miss, f[19] leak magnitude) prices
// what the actor reveals about ITS OWN hand.  Nothing prices the fact that a
// miss at seat q publishes "q does not hold c" -- a certificate whose only
// beneficiaries are q's two teammates, who are trying to work out which of them
// holds it.  That is the exact information the target needs to allocate a
// declaration correctly, and 88.1% of v0.6's wrong declarations are pure
// allocation errors (INSTRUMENT.md 1.10).  A vector with these four at zero is
// the phase-1 twelve-coordinate class, and a vector with all sixteen at zero is
// `v06` bit for bit, so both identity controls survive.
// Two further coordinates, added when phase-2 reconnaissance established a
// property of the target's prior that the corpus has never written down:
// `Knowledge::priorWeight` (belief.hpp:123-138) reads `askCount[p][S]` and
// `totalAsks[p]` and NOTHING ELSE.  `missCount` exists (belief.hpp:55, written
// at :222) and no belief quantity reads it.  The prior is therefore
// OUTCOME-BLIND: an ask that misses is exactly as much evidence that the asker
// holds cards of that half-suit as an ask that hits.  Since the rules only
// require the asker to hold ONE card of the half-suit (fish.hpp:158-165), an
// adversary holding a single card of S can ask in S repeatedly and drive the
// target's marginal for its own team's other five cards of S up by as much as
// exp(2.6) = 13.5x -- the clip at belief.hpp:131 -- at a price of one lost turn
// per repetition.  That is the channel the `feint` archetype gropes at by
// restricting which half-suits it asks in, priced linearly for the first time.
static constexpr int NR7 = 18;                     // the responder's extra ask terms
static constexpr int NV7PARAM = NFEAT + 14 + 3 + NR7;

inline const char* r7Name(int i) {
  static const char* n[NR7] = {
    "targetKnownStrength",  // per-target: how much of this target is already resolved
    "targetSetMass",        // opponent-hand modelling: expected cards of this half-suit at the target
    "oppTeamSetMass",       // ... and across the whole opposing team
    "turnDonationCost",     // (1-p) x what the target can do with the turn we would hand it
    "targetThreat",         // the target's expected holding in half-suits WE lead
    "roleHit0",             // seat-role: p, on the first seat of the responder team
    "roleHit1",             // seat-role: p, on the second
    "roleClaim",            // an agreed division of half-suits among the three seats
    "targetAskedHere",      // within-match model of the TARGET's public ask tally
    "targetMissedHere",     // ... and its miss tally
    "phaseHit",             // p x game phase: lets the fit choose WHEN to deviate
    "deadDonation",         // the deliberate miss, priced linearly for the first time
    // ---- information denial: what this ask hands to the OPPOSING team -------
    "oppCertDonate",        // (1-p) x opp mass x publicly-ambiguous mass: the negative
                            //   certificate a MISS publishes, which resolves the target
                            //   team's own internal allocation for them
    "oppCertHit",           // p x opp mass: what a HIT resolves for them (the card leaves
                            //   a named seat in public view)
    "oppNearLock",          // step: the opposing team is within one card of owning this
                            //   half-suit outright -- the state in which a declaration is
                            //   imminent and a certificate is worth most to them
    "oppDenyLate",          // opp mass x game phase: denial is worth more late, when the
                            //   target is about to declare, than early
    // ---- tally inflation: lying to an outcome-blind prior ------------------
    "selfTally",            // my own public ask tally in this half-suit, which is the
                            //   only thing the target's theta term reads
    "tallyLie"              // ... times how little of the half-suit I actually hold: the
                            //   SIZE of the lie the next ask here would tell
  };
  return n[i];
}

struct V07Responder : V06Agent {
  double rw[NR7] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  bool   admitDead = false;      // put provably-dead candidates in the scored set
  int    deadCap   = 3;          // per game per seat, as v0.6 rations them
  int    corrPlans = 0;          // 0 = A1 (no device); K>0 = A2 over K role plans
  int    roleOffset = 0;         // the drawn plan
  // per-decision scratch
  double tgtBest[NPLAY] = {0,0,0,0,0,0};   // target's best expected half-suit mass
  double leadMask = 0;
  double setLead[NSET] = {0,0,0,0,0,0,0,0,0};
  int    myRole = 0;
  int    dead7Used = 0;

  V07Responder() { label = "fishbot_v07r"; x.extraFeats = true; }

  void resetR7(int s) {
    myRole = s / 2;
    dead7Used = 0;
    // A2: read the harness's per-game correlation signal.  Zero when the regime
    // is off, in which case every seat picks plan 0 and the team is A1.
    roleOffset = corrPlans > 0 ? int(correlationSignal() % uint64_t(corrPlans)) : 0;
  }
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V06Agent::reset(s, hand, r, seed);
    resetR7(s);
  }
  // Without this, a v0.7 responder used as a ROLLOUT BLUEPRINT would keep the
  // seat role and the dead-ask budget of whatever seat it was last reset for --
  // the rollout path resets through resetWithKnowledge, not reset.
  void resetWithKnowledge(int s, uint64_t hand, const Rules& r, uint64_t seed,
                          const Knowledge& k0) override {
    V06Agent::resetWithKnowledge(s, hand, r, seed, k0);
    resetR7(s);
  }

  // The deliberate miss has to be RATIONED, for the reason v0.6 rations it:
  // unbanning it wholesale scores higher and brings back 35.85% dead asks, a
  // 365-ask dead run and 10% of games killed by the action limit
  // (research/v06/results/E15-deliberate-miss.txt).  The budget is spent here,
  // where the chosen ask is known, rather than in the scoring loop, where it is
  // not.  Without this the budget check in enumerateForScore never fires.
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove m = V06Agent::chooseAsk(pub);
    if (admitDead && provablyDead(m.card, m.target) && !deadTried[m.card][m.target]) {
      deadTried[m.card][m.target] = true;
      dead7Used++;
    }
    return m;
  }

  bool wantV6Path() const override { return true; }

  int enumerateForScore(const PublicState& pub, AskMove* buf) override {
    if (!admitDead || dead7Used >= deadCap)
      return cfg.liveAskGate ? enumerateLive(pub, buf) : enumerateAsks(pub, k.myHand, seat, buf);
    // The deliberate miss, admitted into the SCORED set rather than bolted on
    // after it.  v0.6 can only offer dead candidates to a rollout, because its
    // linear score cannot price one -- f[0]'s 11.64*p is zero on a dead ask by
    // definition and every remaining term is a penalty, which is why sweeping
    // the admission margin gave bit-identical output at every setting
    // (v06.hpp, A5).  `deadDonation` is the coordinate that fixes that, and it
    // exists only in this class.
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    int m = 0;
    for (int i = 0; i < n; i++) {
      bool d = provablyDead(buf[i].card, buf[i].target);
      if (d && deadTried[buf[i].card][buf[i].target]) continue;
      buf[m++] = buf[i];
    }
    return m ? m : n;
  }

  void prepareScore(const PublicState& pub) override {
    // What each opponent could do with the turn, and which half-suits we lead.
    for (int q = 0; q < NPLAY; q++) tgtBest[q] = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) { setLead[s] = 0; continue; }
      double mine = 0, theirs = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        for (int q = 0; q < NPLAY; q++) {
          double m = (k.owner[c] == q) ? 1.0 : ((k.owner[c] < NPLAY) ? 0.0 : bel.marg[c][q]);
          if (teamMask & (1 << q)) mine += m; else theirs += m;
          if (oppMask & (1 << q)) { /* per-target accumulation below */ }
        }
      }
      setLead[s] = (mine - theirs) / double(SETSZ);
      for (int q = 0; q < NPLAY; q++) {
        if (!(oppMask & (1 << q)) || !pub.handCount[q]) continue;
        double mass = 0;
        for (int i = 0; i < SETSZ; i++) {
          int c = cardOf(s, i);
          mass += (k.owner[c] == q) ? 1.0 : ((k.owner[c] < NPLAY) ? 0.0 : bel.marg[c][q]);
        }
        if (mass > tgtBest[q]) tgtBest[q] = mass;
      }
    }
  }

  void r7Terms(const PublicState& pub, int card, int target, double p, double* g) const {
    const int S = setOf(card);
    int known = 0;
    for (int c = 0; c < NCARD; c++) if (k.owner[c] == target) known++;
    double hc = std::max(1.0, double(pub.handCount[target]));
    double tMass = 0, oMass = 0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(S, i);
      if (k.owner[c] < NPLAY) {
        if (k.owner[c] == target) tMass += 1.0;
        if (oppMask & (1u << k.owner[c])) oMass += 1.0;
        continue;
      }
      tMass += bel.marg[c][target];
      for (int q = 0; q < NPLAY; q++) if (oppMask & (1 << q)) oMass += bel.marg[c][q];
    }
    double donate = tgtBest[target] / double(SETSZ);
    double threat = 0;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s] || setLead[s] <= 0) continue;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        threat += (k.owner[c] == target) ? 1.0 : ((k.owner[c] < NPLAY) ? 0.0 : bel.marg[c][target]);
      }
    }
    g[0]  = double(known) / hc;
    g[1]  = tMass / double(SETSZ);
    g[2]  = oMass / double(SETSZ);
    g[3]  = (1.0 - p) * donate;
    g[4]  = threat / double(std::max(1, NSET * SETSZ));
    g[5]  = (myRole == 0) ? p : 0.0;
    g[6]  = (myRole == 1) ? p : 0.0;
    g[7]  = (((S + roleOffset) % 3) == myRole) ? 1.0 : 0.0;
    g[8]  = double(k.askCount[target][S]) / 8.0;
    g[9]  = double(k.missCount[target][S]) / 8.0;
    g[10] = p * (double(pub.nEvents) / 100.0);
    g[11] = (p <= 0.0) ? (1.0 - donate) : 0.0;
    // ---- information denial -------------------------------------------------
    // `uS` is the number of cards of this half-suit whose holder we cannot
    // place.  It is a proxy for the opposing team's OWN residual ambiguity:
    // a card we can place is one whose location is public (or ours), and a
    // card whose location is public is not ambiguous to them either.  The
    // proxy over-counts -- our own cards are unknown to them but known to us,
    // and their cards are unknown to us but known to their holder -- and the
    // over-count is in a fixed direction, which is what a linear coefficient
    // can absorb.
    int uS = 0;
    for (int i = 0; i < SETSZ; i++) if (k.owner[cardOf(S, i)] >= NPLAY) uS++;
    const double oppFrac = oMass / double(SETSZ);
    g[12] = (1.0 - p) * oppFrac * (double(uS) / double(SETSZ));
    g[13] = p * oppFrac;
    g[14] = (oMass >= double(SETSZ) - 1.0) ? 1.0 : 0.0;
    g[15] = oppFrac * (double(pub.nEvents) / 100.0);
    // ---- tally inflation ----------------------------------------------------
    const double mine = double(__builtin_popcountll(k.myHand & setMask(S)));
    g[16] = double(k.askCount[seat][S]) / 8.0;
    g[17] = g[16] * (1.0 - mine / double(SETSZ));
  }

  double blueprintScore(const PublicState& pub, int card, int target, double* fOut) override {
    double u = V06Agent::blueprintScore(pub, card, target, fOut);
    double p = fOut ? *fOut : bel.marg[card][target];
    double g[NR7] = {0};
    r7Terms(pub, card, target, p, g);
    double add = 0;
    for (int i = 0; i < NR7; i++) add += rw[i] * g[i];
    return u + cfg.linearWeight * add;
  }
};

// ------------------------------------------------------- C5, white-box class
struct V07InvertAgent : V06Agent {
  v07::Inverter inv;
  bool   invOn = true;
  bool   invStarted = false;
  double invClip = 5.0;
  long long invSkipped = 0;

  // Defaults are the swept optimum from `fish v7bits` (focus 2, gain 1.0,
  // kappa 3, step clip 1.25), not a guess: at those settings the observer's
  // marginals improve against ground truth on both criteria at once, which no
  // other setting in the sweep does.
  V07InvertAgent() { label = "fishbot_v07i"; inv.focus = 2; inv.gain = 1.0; inv.nDet = 48; }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    V06Agent::reset(s, hand, r, seed);
    if (invOn) {
      inv.reset(r, r.deckSets, mixSeed(seed, 0x1E7ull));
      k.policyLL = inv.ll;
      k.policyClip = invClip;
      invStarted = true;
    }
  }
  void observe(const Event& e) override {
    // The inversion reads the state BEFORE the event: `k` is still the observer's
    // pre-event knowledge and `inv.mirror` is still the pre-event public state.
    if (invOn && e.kind == Kind::Ask && teamOf(e.actor) != teamOf(seat)) {
      double bits = 0;
      if (inv.invertAsk(k, e, &bits) < 0) invSkipped++;
    }
    inv.advance(e);
    V06Agent::observe(e);
    if (invOn) { k.policyLL = inv.ll; k.policyClip = invClip; }
  }
};

} // namespace fish
