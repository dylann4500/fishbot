// Faithful C++ ports of the FishLab v0.3 policy population.  These reproduce
// lib/fish-engine.ts: the same memory model (known owners, exclusions, public
// ask signals), the same Sinkhorn count conditioning, the same utility
// coefficients and the same declaration thresholds.  They exist so that v0.4 is
// measured against the published benchmark rather than a straw man.
#pragma once
#include "fish.hpp"
#include "game.hpp"

namespace fish {

enum class Baseline { Random, Hunter, Diversifier, Detective, Lockout, Bluffer, FishV02, FishV03 };

inline const char* baselineName(Baseline b) {
  switch (b) {
    case Baseline::Random: return "random";
    case Baseline::Hunter: return "hunter";
    case Baseline::Diversifier: return "diversifier";
    case Baseline::Detective: return "detective";
    case Baseline::Lockout: return "lockout";
    case Baseline::Bluffer: return "bluffer";
    case Baseline::FishV02: return "fishbot_v02";
    case Baseline::FishV03: return "fishbot_v03";
  }
  return "?";
}

struct V03Config {
  bool useCountConditioning = true;
  double signalStrength = .453;
  double hitWeight = 22, informationWeight = 0, setProgressWeight = 2.5, teamControlWeight = 4;
  double targetEvidenceWeight = .5, continuationWeight = 4, completionWeight = 4;
  double replyThreatWeight = 1, repeatSetWeight = .5;
  double declarationThreshold = .963, trailingDelta = -.016, leadingDelta = .005, allocationSlack = .008;
};

// The v0.3 memory: exclusions + known owners + public ask tallies only.
struct LegacyMemory {
  int8_t knownOwner[NCARD];
  uint8_t excluded[NCARD][NPLAY];
  uint8_t signals[NPLAY][NSET];
  bool active[NSET];
  uint8_t handCount[NPLAY];
  uint64_t myHand = 0;
  int me = 0;

  void init(int seat, uint64_t hand, int deckSets) {
    me = seat; myHand = hand;
    memset(signals, 0, sizeof(signals));
    memset(excluded, 0, sizeof(excluded));
    for (int s = 0; s < NSET; s++) active[s] = (s < deckSets);
    for (int p = 0; p < NPLAY; p++) handCount[p] = uint8_t(deckSets);
    for (int c = 0; c < NCARD; c++) {
      if (hand & bit(c)) knownOwner[c] = int8_t(seat);
      else { knownOwner[c] = -1; excluded[c][seat] = 1; }
    }
  }
  void onEvent(const Event& e) {
    if (e.kind == Kind::Ask) {
      excluded[e.card][e.actor] = 1;
      signals[e.actor][setOf(e.card)] = uint8_t(std::min(255, signals[e.actor][setOf(e.card)] + 1));
      if (e.success) {
        knownOwner[e.card] = int8_t(e.actor);
        for (int p = 0; p < NPLAY; p++) excluded[e.card][p] = (p == e.actor) ? 0 : 1;
        if (e.actor == me) myHand |= bit(e.card);
        if (e.target == me) myHand &= ~bit(e.card);
      } else excluded[e.card][e.target] = 1;
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      active[e.set] = false;
      myHand &= ~setMask(e.set);
    }
    for (int p = 0; p < NPLAY; p++) handCount[p] = e.handCount[p];
  }
  // v0.3 independent posterior
  double ownerProbability(int card, int owner) const {
    if (knownOwner[card] >= 0) return knownOwner[card] == owner ? 1.0 : 0.0;
    if (excluded[card][owner]) return 0.0;
    int s = setOf(card);
    double total = 0, mine = 0;
    for (int p = 0; p < NPLAY; p++) {
      if (excluded[card][p]) continue;
      double w = 1 + std::min(3.0, signals[p][s] * .38);
      total += w;
      if (p == owner) mine = w;
    }
    return total ? mine / total : (me == owner ? 0.0 : .2);
  }
  void independentBeliefs(double out[NCARD][NPLAY]) const {
    for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) out[c][p] = ownerProbability(c, p);
  }
  // v0.3 count-conditioned posterior: 12 alternating row/column scalings.
  void conditionedBeliefs(double out[NCARD][NPLAY], double signalStrength) const {
    int knownCounts[NPLAY] = {0,0,0,0,0,0};
    int unresolved[NCARD], nU = 0;
    for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) out[c][p] = 0;
    for (int c = 0; c < NCARD; c++) {
      if (!active[setOf(c)]) continue;
      if (knownOwner[c] >= 0) { out[c][knownOwner[c]] = 1; knownCounts[knownOwner[c]]++; }
      else {
        unresolved[nU++] = c;
        for (int p = 0; p < NPLAY; p++) if (!excluded[c][p])
          out[c][p] = std::exp(std::min(2.4, signals[p][setOf(c)] * signalStrength));
      }
    }
    double cap[NPLAY];
    for (int p = 0; p < NPLAY; p++) { double v = double(handCount[p]) - knownCounts[p]; cap[p] = v > 0 ? v : 0; }
    for (int it = 0; it < 12; it++) {
      for (int i = 0; i < nU; i++) {
        int c = unresolved[i];
        double t = 0; for (int p = 0; p < NPLAY; p++) t += out[c][p];
        if (t == 0) { for (int p = 0; p < NPLAY; p++) if (!excluded[c][p] && cap[p] > 0) out[c][p] = 1;
                      t = 0; for (int p = 0; p < NPLAY; p++) t += out[c][p]; }
        if (t) for (int p = 0; p < NPLAY; p++) out[c][p] /= t;
      }
      double col[NPLAY] = {0,0,0,0,0,0};
      for (int i = 0; i < nU; i++) for (int p = 0; p < NPLAY; p++) col[p] += out[unresolved[i]][p];
      for (int p = 0; p < NPLAY; p++) { double s = col[p] ? cap[p] / col[p] : 0;
        for (int i = 0; i < nU; i++) out[unresolved[i]][p] *= s; }
    }
    for (int i = 0; i < nU; i++) { int c = unresolved[i];
      double t = 0; for (int p = 0; p < NPLAY; p++) t += out[c][p];
      if (t) for (int p = 0; p < NPLAY; p++) out[c][p] /= t; }
  }
};

inline double binaryEntropy(double p) {
  if (p <= 0 || p >= 1) return 0;
  return -p * std::log2(p) - (1 - p) * std::log2(1 - p);
}

struct BaselineAgent : Agent {
  Baseline kind;
  V03Config cfg;
  LegacyMemory mem;
  Rng rng;
  int focus = -1;
  int lastAskActor = -1, lastAskSet = -1, lastAskTarget = -1; bool lastAskSuccess = false;
  bool psychTells = false;
  double declThresholdOverride = -1;

  explicit BaselineAgent(Baseline b) : kind(b) {}
  const char* name() const override { return baselineName(kind); }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    mem.init(s, hand, r.deckSets);
    rng = Rng(seed);
    focus = -1; lastAskActor = -1; lastAskSet = -1; lastAskSuccess = false;
  }
  void observe(const Event& e) override {
    Agent::observe(e);
    mem.onEvent(e);
    if (e.kind == Kind::Ask) { lastAskActor = e.actor; lastAskSet = e.set;
                               lastAskSuccess = e.success; lastAskTarget = e.target; }
  }

  static double publicReplyThreat(const LegacyMemory& m, int actor, int target, int askedSet,
                                  const double bel[NCARD][NPLAY]) {
    double best = 0;
    for (int s = 0; s < NSET; s++) {
      if (!m.active[s]) continue;
      double none = 1, eTarget = 0, eTargetTeam = 0, eFriendly = 0;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        double pt = bel[c][target];
        eTarget += pt; none *= 1 - pt;
        for (int p = 0; p < NPLAY; p++) {
          double pr = bel[c][p];
          if (teamOf(p) == teamOf(target)) eTargetTeam += pr; else eFriendly += pr;
        }
      }
      double targetHasSet = 1 - none;
      double publicActivity = std::min(1.0, m.signals[target][s] / 3.0);
      double concentration = std::min(1.0, eTarget / 4.0);
      double opponentControl = eTargetTeam / 6.0;
      double friendlyExposure = eFriendly / 6.0;
      double newTell = (s == askedSet) ? .12 : 0;
      double danger = targetHasSet * friendlyExposure *
        std::min(1.0, .15 + .35 * concentration + .25 * publicActivity + .25 * opponentControl + newTell);
      best = std::max(best, danger);
      (void)actor;
    }
    return best;
  }

  double v03Utility(int card, int target, const double bel[NCARD][NPLAY], double p) const {
    int s = setOf(card);
    int held = 0; double expTeam = 0, continuation = 0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i);
      if (mem.myHand & bit(c)) held++;
      for (int q = 0; q < NPLAY; q++) if (teamOf(q) == teamOf(seat)) expTeam += bel[c][q];
      if (c == card || (mem.myHand & bit(c))) continue;
      for (int t = 0; t < NPLAY; t++) if (teamOf(t) != teamOf(seat)) continuation = std::max(continuation, bel[c][t]);
    }
    double setProgress = held / 6.0;
    double teamControl = expTeam / 6.0;
    double targetEvidence = std::min(1.0, mem.signals[target][s] / 4.0);
    double completion = held >= 4 ? 1.0 : (held == 3 ? .35 : 0.0);
    double replyThreat = publicReplyThreat(mem, seat, target, s, bel);
    double repeats = (lastAskActor == seat && lastAskSet == s) ? 1.0 : 0.0;
    return cfg.hitWeight * p + cfg.informationWeight * binaryEntropy(p)
         + cfg.setProgressWeight * setProgress + cfg.teamControlWeight * teamControl
         + cfg.targetEvidenceWeight * targetEvidence
         + cfg.continuationWeight * p * continuation
         + cfg.completionWeight * p * completion
         + cfg.repeatSetWeight * repeats
         - cfg.replyThreatWeight * (1 - p) * replyThreat;
  }

  double v02Utility(int card, int target, const double legacyBel[NCARD][NPLAY], double p) const {
    int s = setOf(card);
    int held = 0, knownFriendly = 0;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i);
      if (mem.myHand & bit(c)) held++;
      int k = mem.knownOwner[c];
      if (k >= 0 && teamOf(k) == teamOf(seat)) knownFriendly++;
    }
    double setProgress = std::min(1.0, (held + knownFriendly * .45) / 6.0);
    double targetEvidence = std::min(1.0, mem.signals[target][s] / 4.0);
    double replyThreat = publicReplyThreat(mem, seat, target, s, legacyBel);
    return 13 * p + 4.5 * binaryEntropy(p) + 3.2 * setProgress + 1.6 * targetEvidence
         + 2.2 * p - 3.8 * (1 - p) * replyThreat;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    static thread_local double bel[NCARD][NPLAY];
    static thread_local double indep[NCARD][NPLAY];
    bool useCond = cfg.useCountConditioning;
    if (useCond) mem.conditionedBeliefs(bel, cfg.signalStrength); else mem.independentBeliefs(bel);
    mem.independentBeliefs(indep);
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, mem.myHand, seat, buf);
    if (!n) return AskMove{0, 0};
    int heldBySet[NSET] = {0,0,0,0,0,0,0,0,0}, teamKnown[NSET] = {0,0,0,0,0,0,0,0,0};
    for (int c = 0; c < NCARD; c++) {
      if (mem.myHand & bit(c)) heldBySet[setOf(c)]++;
      int o = mem.knownOwner[c];
      if (o >= 0 && teamOf(o) == teamOf(seat)) teamKnown[setOf(c)]++;
    }
    if (focus < 0 || !pub.setActive[focus] || heldBySet[focus] == 0) {
      int best = -1;
      for (int i = 0; i < n; i++) { int s = setOf(buf[i].card);
        if (best < 0 || heldBySet[s] > heldBySet[best]) best = s; }
      focus = best;
    }
    bool responding = (lastAskActor >= 0 && !lastAskSuccess && lastAskTarget == seat);
    if (kind == Baseline::Random) return buf[rng.u32(uint32_t(n))];
    double bestScore = -1e18; AskMove best = buf[0];
    for (int i = 0; i < n; i++) {
      int card = buf[i].card, target = buf[i].target, s = setOf(card);
      double pIndep = indep[card][target];
      double pCond = bel[card][target];
      double score;
      if (kind == Baseline::FishV03) score = v03Utility(card, target, bel, pCond);
      else if (kind == Baseline::FishV02) score = v02Utility(card, target, bel, pIndep);
      else {
        score = pIndep * 9 + heldBySet[s] * .72 + teamKnown[s] * .25 + rng.uni() * .7;
        if (kind == Baseline::Hunter) { score += (s == focus) ? 6.2 : -1.1; score += heldBySet[s] * 1.25; }
        if (kind == Baseline::Detective || kind == Baseline::Lockout) {
          score += pIndep * 12; score += mem.signals[target][s] * .85;
        }
        if (kind == Baseline::Lockout) score -= 8 * (1 - pIndep) * publicReplyThreat(mem, seat, target, s, bel);
        if (kind == Baseline::Diversifier) {
          bool recentSame = (lastAskActor == seat && lastAskSet == s);
          score += recentSame ? -3.6 : 2.1;
          score += (6 - heldBySet[s]) * .25;
        }
        if (kind == Baseline::Bluffer) {
          if (responding && psychTells) score += (s == lastAskSet) ? -6.8 : 4.8;
          score += pIndep * 2 + rng.uni() * 2.8;
        }
        if (responding && psychTells && kind != Baseline::Bluffer)
          score += (s == lastAskSet) ? 1.1 : -.25;
      }
      if (score > bestScore) { bestScore = score; best = buf[i]; }
    }
    return best;
  }

  struct Pred { int owners[SETSZ]; double confidence; double teamConfidence; };
  Pred predictionForSet(int s, const double* belFlat) const {
    Pred r{}; double teamConf = 1, allocConf = 1;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i);
      double bestP = -1; int bestQ = seat; double teamP = 0;
      for (int q = 0; q < NPLAY; q++) {
        if (teamOf(q) != teamOf(seat)) continue;
        double pr = belFlat ? belFlat[c * NPLAY + q] : mem.ownerProbability(c, q);
        teamP += pr;
        if (pr > bestP) { bestP = pr; bestQ = q; }
      }
      r.owners[i] = bestQ;
      teamConf *= std::max(.001, teamP);
      allocConf *= std::max(.001, bestP);
    }
    r.confidence = std::pow(teamConf * allocConf, 1.0 / 12.0);
    r.teamConfidence = std::pow(teamConf, 1.0 / 6.0);
    return r;
  }

  bool bestDeclarationInternal(const PublicState& pub, Declaration& out, double& conf, bool forced) {
    static thread_local double bel[NCARD][NPLAY];
    const double* flat = nullptr;
    if (kind == Baseline::FishV03) {
      if (cfg.useCountConditioning) mem.conditionedBeliefs(bel, cfg.signalStrength);
      else mem.independentBeliefs(bel);
      flat = &bel[0][0];
    }
    bool informationExhausted = false;
    if (kind == Baseline::FishV03 || kind == Baseline::FishV02) {
      static thread_local double b2[NCARD][NPLAY];
      const double* f2;
      if (kind == Baseline::FishV03) f2 = flat;
      else { mem.independentBeliefs(b2); f2 = &b2[0][0]; }
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, mem.myHand, seat, buf);
      informationExhausted = true;
      for (int i = 0; i < n; i++) if (f2[buf[i].card * NPLAY + buf[i].target] > .001) { informationExhausted = false; break; }
      if (!n) informationExhausted = true;
    }
    int lead = int(pub.score[teamOf(seat)]) - int(pub.score[1 - teamOf(seat)]);
    double bestConf = -1; bool found = false;
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      Pred pr = predictionForSet(s, flat);
      double optimized = cfg.declarationThreshold + (lead >= 2 ? cfg.leadingDelta : (lead <= -2 ? cfg.trailingDelta : 0));
      double legacyThreshold = lead >= 2 ? .97 : (lead <= -2 ? .94 : .96);
      double base;
      switch (kind) {
        case Baseline::Hunter: base = .82; break;
        case Baseline::Diversifier: base = .88; break;
        case Baseline::Detective: base = .92; break;
        case Baseline::Lockout: base = .94; break;
        case Baseline::Bluffer: base = .76; break;
        case Baseline::Random: base = .68; break;
        default: base = .96; break;
      }
      double risk;
      switch (kind) {
        case Baseline::Hunter: risk = .16; break;
        case Baseline::Diversifier: risk = .12; break;
        case Baseline::Detective: risk = .06; break;
        case Baseline::Lockout: risk = .04; break;
        case Baseline::Bluffer: risk = .28; break;
        case Baseline::Random: risk = .42; break;
        default: risk = .02; break;
      }
      double threshold;
      if (forced) threshold = .38;
      else if (kind == Baseline::FishV03)
        threshold = (pub.nEvents >= 280 || informationExhausted) ? .78 : optimized;
      else if (pub.nEvents >= 280) threshold = .5;
      else if (informationExhausted) threshold = .72;
      else threshold = (kind == Baseline::FishV02) ? legacyThreshold : base;
      double slack = (kind == Baseline::FishV03) ? cfg.allocationSlack : risk * .22;
      if (declThresholdOverride >= 0) threshold = declThresholdOverride;
      if (pr.teamConfidence < threshold || pr.confidence < threshold - slack) continue;
      if (pr.confidence > bestConf) {
        bestConf = pr.confidence; found = true;
        out.set = uint8_t(s);
        for (int i = 0; i < SETSZ; i++) out.owner[i] = uint8_t(pr.owners[i]);
      }
    }
    conf = bestConf;
    return found;
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    if (!mem.handCount[seat] && !pub.rules.cardlessMayDeclare) return false;
    return bestDeclarationInternal(pub, d, conf, false);
  }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) override {
    Pred pr = predictionForSet(set, nullptr);
    if (pr.confidence < threshold) return false;
    d.set = uint8_t(set);
    for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(pr.owners[i]);
    conf = pr.confidence;
    (void)pub;
    return true;
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    Pred pr = predictionForSet(set, nullptr);
    d.set = uint8_t(set);
    for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(pr.owners[i]);
    conf = pr.confidence;
    (void)pub;
  }
  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    int best = cand[0];
    for (int i = 1; i < n; i++) if (pub.handCount[cand[i]] > pub.handCount[best]) best = cand[i];
    return best;
  }
};

} // namespace fish
