// FishBot v0.5 -- M4 (per-seat knowledge model) and M5 (target-dimension
// selection), as free functions plus a self-contained mixin module.
//
// Nothing in this header depends on V05Agent.  It is included by v05.hpp and
// driven through an explicit context struct so that it can also be exercised
// standalone by engine/src/probe_m45_test.cpp.
//
// ---------------------------------------------------------------------------
// THE DEFECT THIS FIXES
//
// `askExpectedValue` opens with `(void)target;` (v04.hpp:435, inherited verbatim
// by v05.hpp:427), so the value half of the ask score cannot see who is being
// asked.  Every target-dependent quantity anywhere in the ask score is material:
// hit probability, the target's reply threat, the target's hand size, the
// target's exposure, the follow-up hit probability (P5-verify-target-channel.md
// section 1).  Measured over 154,318 v0.4-mirror ask decisions, at 46.6% of
// decisions (43.9% at seed 777001; 66.2% against v0.3) at least two opponents
// are hard-indistinguishable holders of the card actually asked for, worth a
// mean 0.639 bits (0.607 at seed 777001; 0.919 vs v0.3) -- about 41 bits per
// team per game that no term in the policy consumes
// (P5-human-strategy.md section 3A, P5-verify-target-channel.md section 2).
//
// The verification report corrects the price: inside that class the spread of
// v0.4's OWN posterior is 0.25-0.29, not the 0.081 of the capacity-marginal
// yardstick, and the intra-class spread of the total ask score is 3.0-3.4,
// which exceeds v0.4's top1-top2 margin at 86-88% of such decisions
// (P5-verify-target-channel.md section 3).  So the target dimension is a PRICED
// channel, and a v0.5 that moves on it is overriding the policy's leading
// preference most of the time it fires.  That is the argument for scoring the
// target dimension on its own explicit terms rather than perturbing it.
//
// ---------------------------------------------------------------------------
// M4 -- A KNOWLEDGE MODEL OF THE OTHER FIVE SEATS
//
// v0.4/v0.5 build `Knowledge` only for themselves; the only two clones in the
// policy are clones of `k` itself (v04.hpp:507,524 / v05.hpp:532,549).  Every
// technique that turns on "what does *he* know" -- blackballing, teammate-
// directed signalling, best-informed-declarer selection, baiting -- is out of
// reach because the state it would read does not exist (P5 section 0, item 4).
//
// The transcript is public, so seat j's deduction state is computable by anyone.
// One structural observation makes this much cheaper than the 6x the design note
// budgets for:
//
//   *** The public deduction state is COMMON KNOWLEDGE: it is a single object,
//       not six.  ***
//
// Read `Knowledge::onEvent` (belief.hpp:153-210): every write to `owner`,
// `mask`, `unresolved`, `disj`, `askCount`, `missCount`, `totalAsks`,
// `handCount` and `publicKnown` is a function of the event alone.  The only two
// lines that consult `me` are belief.hpp:207-208, which maintain `myHand`.  Six
// `Knowledge` objects fed the same event stream from an all-possible start
// therefore agree bit for bit outside `me`/`myHand`.  probe_m45_test.cpp checks
// this empirically (check D) rather than resting on the code reading.
//
// So M4 maintains ONE incremental public object, `SeatModels::pub`, and derives
// a per-seat model lazily:
//
//   model(j) = pub  refined by every card OUR OWN `k` can prove seat j holds.
//
// That refinement is sound as a model of j's knowledge because j knows its own
// hand: if we can prove j holds c, then j knows it holds c.  It is what makes
// the models differ between seats at all -- `pub` alone cannot rank opponents by
// knowledge, only by our belief about their cards.
//
// LIMITATION, and it matters.  model(j) omits constraint C1 for seat j: every
// deduction j makes from the part of j's hand we cannot see.  A seat that holds
// four cards of a half-suit can exclude four cards from everyone else; our model
// of that seat cannot.  model(j) is therefore a STRICT UNDER-APPROXIMATION of
// what seat j actually knows.  Two consequences:
//
//   (i)  every fact in model(j) is genuinely known to j, so the model is a sound
//        LOWER BOUND on opponent knowledge.  `owner[c] == u` in model(t) is a
//        certificate: "opponent t provably knows our seat u holds card c".  That
//        is exactly the direction blackballing needs, and it is why the hard
//        lockout term below is a proof and not an estimate.
//   (ii) it can never be used the other way.  model(t) not knowing something is
//        NOT evidence that t does not know it, so this machinery must never be
//        used to argue an ask is safe.  The soft lockout term is graded, not a
//        safety proof, and no term here rewards an ask for being unreadable.
//
// ---------------------------------------------------------------------------
// M5 -- TARGET-DIMENSION SELECTION
//
//   (a) lockout / blackball value.  v0.5's f[8] = (1-p)*threatOf(target)
//       estimates the target's CARDS and fudges its knowledge with
//       `activity = askCount[t][s]/3` (v05.hpp:211).  Develin's blackball turns
//       on the opponent KNOWING where a card is.  With M4 we score that
//       directly, including the certificate our own ask is about to hand the
//       target (a miss emits C5: "the asker holds another card of this
//       half-suit"), which is the mechanism in Develin's worked example.
//   (b) void progress.  Taking an opponent's last card of a half-suit
//       permanently removes their right to ask in it: `legalAsk` requires
//       `g.hand[actor] & setMask(s)` (fish.hpp:164).  v0.5's f[11] fires only at
//       handCount[target]==1, which is emptying the whole hand, not voiding a
//       half-suit (P5 technique 9).
//   (c) the last-live-opponent trap.  f[11] rewards taking an opponent's last
//       card without distinguishing "one of three" from "the last live
//       opponent".  Emptying the last one triggers the forced endgame, in which
//       v0.4 declared 100% wrong (28/28, P0-v04-pathology.md).  Split it: the
//       upside (with the opposing team cardless, every remaining card is ours)
//       and the downside (we must allocate every remaining half-suit blind)
//       are separate features so the fit can price them separately.
//   (d) codebook value on the target index -- Construction B of
//       research/v04/lit/signalling.md section 7.9.  OFF unless
//       cfg.conventions, per owner decision D1.
//
// Every part is separately switchable (cfg.m4, m5a, m5b, m5c, m5d).
#pragma once
#include "fish.hpp"
#include "belief.hpp"

namespace fish {
namespace m45 {

// ---------------------------------------------------------------------- M4
// A Knowledge initialised to "nobody's hand is known": the common-knowledge
// start state.  Knowledge::init() would exclude the named seat from every card
// it does not hold, which is exactly the private constraint C1 we are dropping.
// `me = NPLAY` is out of range for every seat, so the two me-conditioned lines
// in Knowledge::onEvent (belief.hpp:207-208) can never fire on this object.
inline void initPublicKnowledge(Knowledge& kk, int deckSets) {
  kk.me = NPLAY;
  kk.myHand = 0;
  kk.nSets = deckSets;
  kk.disj.clear();
  kk.unresolved = 0;
  memset(kk.askCount, 0, sizeof(kk.askCount));
  memset(kk.missCount, 0, sizeof(kk.missCount));
  memset(kk.totalAsks, 0, sizeof(kk.totalAsks));
  kk.publicKnown = 0;
  for (int s = 0; s < NSET; s++) kk.setActive[s] = (s < deckSets);
  for (int p = 0; p < NPLAY; p++) kk.handCount[p] = uint8_t(deckSets * SETSZ / NPLAY);
  for (int c = 0; c < NCARD; c++) {
    if (setOf(c) >= deckSets) { kk.owner[c] = OUT_OF_PLAY; kk.mask[c] = 0; continue; }
    kk.owner[c] = UNKNOWN;
    kk.mask[c] = 0x3F;                      // every seat still possible
    kk.unresolved |= bit(c);
  }
}

// The public deduction state, plus lazily-derived per-seat refinements.
struct SeatModels {
  Knowledge pub;                 // common knowledge; maintained incrementally
  Knowledge model[NPLAY];        // pub + "cards WE can prove seat j holds"
  bool built[NPLAY] = {false, false, false, false, false, false};
  // Cost counters, so the paper can state the price of M4 instead of asserting
  // it is cheap.  `refits` counts lazy per-seat builds, `events` public updates.
  long long events = 0, refits = 0;
  // Defensive: a card our own k assigns to seat j that the public model has
  // already excluded from j would mean one of the two is unsound.  It must stay
  // at zero; probe_m45_test.cpp asserts that it does.
  long long contradictions = 0;

  void reset(int deckSets) {
    initPublicKnowledge(pub, deckSets);
    for (int j = 0; j < NPLAY; j++) built[j] = false;
    events = refits = contradictions = 0;
  }
  void observe(const Event& e) {
    pub.onEvent(e);
    for (int j = 0; j < NPLAY; j++) built[j] = false;
    events++;
  }
  void invalidate() { for (int j = 0; j < NPLAY; j++) built[j] = false; }

  // Build model(j) = pub refined by every card `mine` proves seat j holds.
  void build(int j, const Knowledge& mine) {
    model[j] = pub;
    model[j].me = j;
    for (int c = 0; c < NCARD; c++) {
      if (mine.owner[c] != j) continue;
      if (model[j].owner[c] == j) continue;
      if (model[j].owner[c] < NPLAY) { contradictions++; continue; }
      if (model[j].owner[c] == OUT_OF_PLAY) continue;
      if (!(model[j].mask[c] & (1u << j))) { contradictions++; continue; }
      model[j].setOwner(c, j);
    }
    uint64_t hj = 0;
    for (int c = 0; c < NCARD; c++) if (model[j].owner[c] == j) hj |= bit(c);
    model[j].myHand = hj;                   // the publicly-provable part of j's hand
    model[j].propagateCapacity();
    built[j] = true;
    refits++;
  }
  const Knowledge& of(int j, const Knowledge& mine) {
    if (!built[j]) build(j, mine);
    return model[j];
  }
};

// ---------------------------------------------------------------------- M5
static constexpr int NM45 = 5;
enum {
  M45_LOCKOUT          = 0,   // (a) hand the turn to someone who knows less
  M45_VOID             = 1,   // (b) void the target in this half-suit
  M45_EMPTY_LAST_SAFE  = 2,   // (c) forced endgame we can allocate
  M45_EMPTY_LAST_RISK  = 3,   // (c) forced endgame we cannot
  M45_CODEBOOK         = 4,   // (d) Construction B, conventions only
};

inline const char* featureName(int i) {
  static const char* n[NM45] = {"lockout", "void", "emptyLastSafe", "emptyLastRisk", "codebook"};
  return (i >= 0 && i < NM45) ? n[i] : "?";
}

struct M45Config {
  // --- mechanism switches (ablation) ---
  bool m4   = true;    // build the per-seat knowledge models at all
  bool m5a  = true;    // lockout / blackball value
  bool m5b  = true;    // per-(target, half-suit) void progress
  bool m5c  = true;    // split the last-live-opponent trap out of f[11]
  bool m5d  = false;   // codebook on the target index
  bool conventions = false;   // owner decision D1: m5d is inert without this

  // --- variants within M5a ---
  // hardOnly: score only what we can PROVE the target knows (owner[c] resolved
  // to a teammate in the target's model).  Off by default: the certificate fires
  // rarely, and the graded form degrades to it continuously.
  bool m5aHardOnly = false;
  // postMiss: include the certificate this very ask would emit.  A miss tells
  // the table "the asker holds another card of this half-suit" (C5,
  // belief.hpp:158-171); if that disjunction has exactly one survivor in the
  // target's model, the target learns precisely which card we hold and can take
  // it on the turn we just handed over.  This is Develin's blackball example.
  bool m5aPostMiss = true;
  // replaceF8: zero v0.5's card-based threat term so the knowledge-based one is
  // not double counting.  Off by default -- f[8]'s weight is already fitted and
  // the two quantities are different (can they take vs do they know).
  bool m5aReplaceF8 = false;

  // --- weights, UNFITTED PLACEHOLDERS ---
  // These are scale-matched to the existing fitted vector, not measured:
  // lockout takes f[8]'s sign and magnitude (-3.0978), the last-live upside
  // keeps f[11]'s (+1.1660).  They MUST be refit -- see the patch notes, which
  // extend the `allparams` flat vector by NM45 entries so the CEM covers them.
  double mw[NM45] = {
    /* lockout        */ -3.0000,
    /* void           */  1.5000,
    /* emptyLastSafe  */  1.1660,
    /* emptyLastRisk  */ -3.0000,
    /* codebook       */  0.5000,
  };
};

// Everything the M5 terms need from the acting agent, passed explicitly so the
// module never reaches into V05Agent.
struct M45Ctx {
  const Knowledge*   k    = nullptr;   // the actor's own knowledge
  const Belief*      bel  = nullptr;   // the actor's posterior (marg only)
  const PublicState* pub  = nullptr;
  int seat = 0, teamMask = 0, oppMask = 0;
};

// P(seat t holds at least one card of half-suit S other than `skip`), under the
// actor's posterior.  Independence across the cards of S, which is the same
// approximation v0.4/v0.5 already make in threatOf (v05.hpp:198-215) and in
// choosePassTarget (v05.hpp:934-956).
inline double pCanAsk(const M45Ctx& c, int t, int S, int skip) {
  double none = 1.0;
  for (int i = 0; i < SETSZ; i++) {
    int d = cardOf(S, i);
    if (d == skip) continue;
    none *= (1.0 - c.bel->marg[d][t]);
  }
  return 1.0 - none;
}

// The target's own estimate that card c sits with OUR team, read off the
// target's knowledge model.  Capacity-weighted over that model's surviving
// owners -- the per-card term of Knowledge::cheapTeamProb (belief.hpp:112-126),
// which needs no dynamic program.  Returns -1 when c is out of play.
inline double targetBeliefTeam(const Knowledge& kt, const uint8_t* q, int c, int teamMask) {
  if (kt.owner[c] < NPLAY) return (teamMask & (1u << kt.owner[c])) ? 1.0 : 0.0;
  if (kt.owner[c] == OUT_OF_PLAY) return -1.0;
  double num = 0, den = 0;
  for (int p = 0; p < NPLAY; p++) if (kt.mask[c] & (1u << p)) {
    den += q[p];
    if (teamMask & (1u << p)) num += q[p];
  }
  return den > 0 ? num / den : 0.0;
}

// --------------------------------------------------------------- the module
struct M45Module {
  M45Config cfg;
  SeatModels models;

  // ---- per-decision cache (filled by beginDecision) ----
  double baseLock[NPLAY]  = {0,0,0,0,0,0};   // knowledge-based lockout value
  double hardLock[NPLAY]  = {0,0,0,0,0,0};   // certificate-only component
  uint8_t capOf[NPLAY][NPLAY] = {};          // capacities under each seat's model
  bool   lastLive[NPLAY]  = {false,false,false,false,false,false};
  int    addressee[NPLAY];                   // opponent -> teammate addressed, -1 = reserved
  double trapRisk = 0;
  bool   ready = false;

  // ---- diagnostics, for the paper ----
  long long decisions = 0, decisionsWithCertificate = 0;

  void reset(int deckSets) {
    models.reset(deckSets);
    for (int j = 0; j < NPLAY; j++) { baseLock[j] = hardLock[j] = 0; addressee[j] = -1; }
    decisions = decisionsWithCertificate = 0;
    ready = false;
  }
  void observe(const Event& e) { if (cfg.m4) models.observe(e); ready = false; }

  // Construction B addressing map (signalling.md 7.9).  Opponents in seat order
  // relative to the actor (+1, +3, +5) address teammates in the same relative
  // order (+2, +4); the third opponent is reserved as "no frame, material ask
  // only".  Stated relative to the actor's seat so it is rotation-invariant --
  // a codebook keyed on absolute seat numbers would mean different things in
  // different rotations of the same deal.
  void buildAddressing(const M45Ctx& c) {
    for (int j = 0; j < NPLAY; j++) addressee[j] = -1;
    int mates[2], nm = 0, opps[3], no = 0;
    for (int d = 1; d < NPLAY; d++) {
      int p = (c.seat + d) % NPLAY;
      if (teamOf(p) == teamOf(c.seat)) { if (nm < 2) mates[nm++] = p; }
      else { if (no < 3) opps[no++] = p; }
    }
    for (int i = 0; i < no && i < nm; i++) addressee[opps[i]] = mates[i];
  }

  // Knowledge-based lockout: if we miss, the turn goes to `t`.  What can `t`
  // then take from us, given what `t` PROVABLY knows?
  //
  //   value(t) = max over live half-suits S, over cards c of S,
  //                  P(t can legally ask in S) * P_t(c is on our team)
  //
  // with P_t read from t's model.  When t's model has c resolved to one of our
  // seats the second factor is exactly 1 and the whole term is a certificate;
  // that component is tracked separately in hardLock[].
  //
  // Contrast v0.5's f[8]: threatOf (v05.hpp:198-215) uses OUR posterior for the
  // target's cards and multiplies by `0.7 + 0.3*min(1, askCount[t][s]/3)` as a
  // stand-in for the target's knowledge.  This term replaces that stand-in with
  // the target's actual public deduction state.
  void computeLockout(const M45Ctx& c) {
    for (int t = 0; t < NPLAY; t++) {
      baseLock[t] = hardLock[t] = 0;
      if (!(c.oppMask & (1 << t))) continue;
      if (!c.pub->handCount[t]) continue;
      const Knowledge& kt = models.of(t, *c.k);
      kt.capacities(capOf[t]);
      for (int S = 0; S < NSET; S++) {
        if (!c.pub->setActive[S]) continue;
        for (int i = 0; i < SETSZ; i++) {
          int cc = cardOf(S, i);
          double pTeamT = targetBeliefTeam(kt, capOf[t], cc, c.teamMask);
          if (pTeamT <= 0) continue;
          double canAsk = pCanAsk(c, t, S, cc);
          double v = canAsk * pTeamT;
          if (v > baseLock[t]) baseLock[t] = v;
          if (pTeamT > 0.9995 && canAsk > hardLock[t]) hardLock[t] = canAsk;
        }
      }
      if (cfg.m5aHardOnly) baseLock[t] = hardLock[t];
    }
  }

  // The extra lockout this particular ask would create by missing.  A miss emits
  // the C5 certificate "the asker holds another card of S" (belief.hpp:158-171).
  // If, in the target's model, exactly one card of S other than `card` can still
  // be ours, that disjunction collapses and the target learns exactly which card
  // we hold -- and takes it with the turn we just handed over.
  //
  // This is the first-order effect only: the mask collapses that the two
  // exclusions (asker lacks `card`, target lacks `card`) could cascade into are
  // not chased.  Chasing them would need a full Knowledge copy and onEvent per
  // candidate ask; the first-order term is O(SETSZ) and captures the mechanism
  // Develin describes.
  double postMissLockout(const M45Ctx& c, int card, int target) const {
    int S = setOf(card);
    if (!models.built[target]) return 0.0;    // beginDecision always builds it
    const Knowledge& kt = models.model[target];
    int survivor = -1, nSurv = 0;
    for (int i = 0; i < SETSZ; i++) {
      int d = cardOf(S, i);
      if (d == card) continue;
      if (kt.owner[d] == uint8_t(c.seat)) return 0.0;   // vacuous: already known
      if (kt.owner[d] != UNKNOWN) continue;
      if (!(kt.mask[d] & (1u << c.seat))) continue;
      survivor = d; nSurv++;
      if (nSurv > 1) return 0.0;
    }
    if (nSurv != 1) return 0.0;
    // The target now knows we hold `survivor`.  It can take it iff it holds
    // another card of S -- and after this miss it certainly does not hold
    // `card`, so exclude that from the legality estimate too.
    double none = 1.0;
    for (int i = 0; i < SETSZ; i++) {
      int d = cardOf(S, i);
      if (d == survivor || d == card) continue;
      none *= (1.0 - c.bel->marg[d][target]);
    }
    return 1.0 - none;
  }

  // Called once per ask decision, after refresh(); builds the per-seat models
  // for the three opponents and everything that does not depend on the card.
  void beginDecision(const M45Ctx& c) {
    ready = false;
    if (!cfg.m4) { for (int j = 0; j < NPLAY; j++) { baseLock[j] = hardLock[j] = 0; } }
    // (c) which opponents this ask could leave cardless, and how badly the
    // forced endgame would go if it emptied the LAST of them.  When the whole
    // opposing team is cardless every remaining card is on our team, so we win
    // every remaining half-suit we can allocate and lose every one we cannot:
    // the risk is exactly the fraction of live half-suits whose allocation is
    // still unresolved for us.
    int active = 0, unalloc = 0;
    for (int s = 0; s < NSET; s++) {
      if (!c.pub->setActive[s]) continue;
      active++;
      if (c.k->unresolved & setMask(s)) unalloc++;
    }
    trapRisk = active ? double(unalloc) / double(active) : 0.0;
    for (int t = 0; t < NPLAY; t++) {
      lastLive[t] = false;
      if (!(c.oppMask & (1 << t))) continue;
      if (c.pub->handCount[t] != 1) continue;
      bool other = false;
      for (int o = 0; o < NPLAY; o++)
        if ((c.oppMask & (1 << o)) && o != t && c.pub->handCount[o]) other = true;
      lastLive[t] = !other;
    }
    if (cfg.m5d && cfg.conventions) buildAddressing(c);
    if (cfg.m4 && (cfg.m5a || (cfg.m5d && cfg.conventions))) {
      // The teammate models are only needed by the codebook term.
      if (cfg.m5d && cfg.conventions)
        for (int j = 0; j < NPLAY; j++)
          if (j != c.seat && teamOf(j) == teamOf(c.seat)) models.of(j, *c.k);
      if (cfg.m5a) computeLockout(c);
    }
    decisions++;
    for (int t = 0; t < NPLAY; t++) if (hardLock[t] > 0) { decisionsWithCertificate++; break; }
    ready = true;
  }

  // (b) void progress.  P(this ask takes the target's LAST card of S), scaled by
  // what our team has at stake in S.  A void opponent can never legally ask in S
  // again (fish.hpp:158-164) -- a permanent, provable gain, and the reason this
  // is not the same feature as f[11] "empties target".
  double voidProgress(const M45Ctx& c, int card, int target, double p) const {
    int S = setOf(card);
    double noOther = 1.0, teamExp = 0.0;
    for (int i = 0; i < SETSZ; i++) {
      int d = cardOf(S, i);
      if (d != card) noOther *= (1.0 - c.bel->marg[d][target]);
      double pt = 0;
      if (c.k->myHand & bit(d)) pt = 1.0;
      else for (int q = 0; q < NPLAY; q++) if (c.teamMask & (1 << q)) pt += c.bel->marg[d][q];
      teamExp += pt;
    }
    return p * noOther * (teamExp / double(SETSZ));
  }

  // (d) Construction B, sender side.  The target index selects which teammate
  // the ask addresses; prefer the target whose addressee most needs the
  // certificate this ask emits about S, and whose need we are actually able to
  // meet.  `need` is read from the addressee's M4 model, so it is what the
  // teammate publicly knows, not what we wish it knew.
  //
  // LIMITATION: this is the sender half only.  The receiver-side decode -- the
  // teammate reading "the ask was targeted at the opponent that addresses me,
  // so the C5 certificate in S is for me" -- belongs with M3 and is not built
  // here, so with M5d alone the convention is one-sided and its measured effect
  // should be near zero.  It is included so the machinery and the D1 flag are in
  // place, not because it is expected to score on its own.
  double codebookValue(const M45Ctx& c, int card, int target) const {
    if (!cfg.m5d || !cfg.conventions || !cfg.m4) return 0.0;
    int j = addressee[target];
    if (j < 0 || !models.built[j]) return 0.0;         // reserved third opponent
    const Knowledge& kj = models.model[j];
    int S = setOf(card);
    int need = 0, tell = 0;
    for (int i = 0; i < SETSZ; i++) {
      int d = cardOf(S, i);
      if (kj.owner[d] == UNKNOWN) need++;
      if ((c.k->myHand & bit(d)) ||
          (c.k->owner[d] < NPLAY && (c.teamMask & (1u << c.k->owner[d])))) tell++;
    }
    return (double(need) / SETSZ) * (double(tell) / SETSZ);
  }

  // The five M5 features for one candidate ask.  `p` is the hit probability the
  // caller already computed (bel.marg[card][target]).
  void features(const M45Ctx& c, int card, int target, double p, double* g) const {
    for (int i = 0; i < NM45; i++) g[i] = 0.0;
    if (cfg.m5a && cfg.m4) {
      double lk = baseLock[target];
      if (cfg.m5aPostMiss) lk = std::max(lk, postMissLockout(c, card, target));
      g[M45_LOCKOUT] = (1.0 - p) * lk;
    }
    if (cfg.m5b) g[M45_VOID] = voidProgress(c, card, target, p);
    if (cfg.m5c && lastLive[target]) {
      g[M45_EMPTY_LAST_SAFE] = p * (1.0 - trapRisk);
      g[M45_EMPTY_LAST_RISK] = p * trapRisk;
    }
    g[M45_CODEBOOK] = codebookValue(c, card, target);
  }

  double score(const M45Ctx& c, int card, int target, double p) const {
    double g[NM45];
    features(c, card, target, p, g);
    double u = 0;
    for (int i = 0; i < NM45; i++) u += cfg.mw[i] * g[i];
    return u;
  }

  // (c) the surviving half of v0.5's f[11]: emptying an opponent that is NOT the
  // last live one is still good -- they cannot be asked, so they cannot receive
  // the turn.  Used by the patched features() in place of the unconditional
  // `(pub.handCount[target] == 1) ? p : 0`.
  double emptiesNonLast(const M45Ctx& c, int target, double p) const {
    if (!c.pub || c.pub->handCount[target] != 1) return 0.0;
    if (cfg.m5c && lastLive[target]) return 0.0;
    return p;
  }
};

} // namespace m45
} // namespace fish
