// R6 recon probe: decision-margin and set-attribution instrumentation for v0.5.
// Lives outside engine/src; includes the engine headers read-only.
#include "factory.hpp"
#include "arena.hpp"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace fish;

// ------------------------------------------------------------ decision probe
struct DecRec {
  int nLegal, nLive;
  double u1, u2, sdAll, rangeAll;      // refined top-2, spread of pre-refine scores
  double p1, p2;                        // posterior hit prob of pick / runner-up
  bool truth1, truth2;                  // ground-truth hit for pick / runner-up
  int nHitAvail;                        // legal asks that would hit
  int rankOfBestHit;                    // rank (by refined score, 0-based) of best hitting cand within top-K; -1 if none in K
  bool lin1EqPick;                      // linear argmax == final pick
  bool sameSetSameTarget;               // top-2 are two cards of one half-suit at one target
  bool exactTie;                        // u1 == u2 bit-for-bit
};

static std::vector<DecRec>* g_sink = nullptr;

struct ProbeV05 : V05Agent {
  const Game* gm = nullptr;
  std::vector<DecRec>* sink = nullptr;

  AskMove chooseAsk(const PublicState& pub) override {
    refresh();
    AskMove legal[NSET * SETSZ * 3];
    int nLegal = enumerateAsks(pub, k.myHand, seat, legal);
    AskMove buf[NSET * SETSZ * 3];
    int n = cfg.liveAskGate ? enumerateLive(pub, buf)
                            : enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return AskMove{0, 0};
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);

    double f[NFEAT];
    std::vector<double> u(n);
    for (int i = 0; i < n; i++) {
      features(pub, buf[i].card, buf[i].target, f);
      double v = 0;
      for (int j = 0; j < NFEAT; j++) v += cfg.w[j] * f[j];
      v *= cfg.linearWeight;
      if (cfg.useValue) v += cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
      u[i] = v;
    }
    int linBest = 0;
    for (int i = 1; i < n; i++) if (u[i] > u[linBest]) linBest = i;
    double mean = 0; for (int i = 0; i < n; i++) mean += u[i]; mean /= n;
    double var = 0; for (int i = 0; i < n; i++) var += (u[i] - mean) * (u[i] - mean);
    double sd = n > 1 ? std::sqrt(var / (n - 1)) : 0.0;
    double lo = u[0], hi = u[0];
    for (int i = 1; i < n; i++) { lo = std::min(lo, u[i]); hi = std::max(hi, u[i]); }

    // ground truth
    int nHitAvail = 0;
    if (gm) for (int i = 0; i < nLegal; i++)
      if (gm->g.hand[legal[i].target] & bit(legal[i].card)) nHitAvail++;

    struct Cand { double u; int idx; };
    std::vector<Cand> cs(n);
    for (int i = 0; i < n; i++) cs[i] = Cand{u[i], i};
    int K = std::min(cfg.searchTopK, n);
    std::partial_sort(cs.begin(), cs.begin() + K, cs.end(),
                      [](const Cand& x, const Cand& y) { return x.u > y.u; });

    std::vector<std::pair<double,int>> refined;   // (refined utility, buf idx)
    for (int r = 0; r < K; r++) {
      int i = cs[r].idx;
      int card = buf[i].card, target = buf[i].target;
      double p = bel.marg[card][target];
      double follow = 0, threat = 0;
      if (cfg.chainWeight != 0 && p > 0.02) {
        Knowledge kh = k;
        kh.setOwner(card, target);
        kh.owner[card] = uint8_t(seat); kh.mask[card] = uint8_t(1u << seat);
        kh.myHand |= bit(card);
        kh.handCount[seat]++; kh.handCount[target]--;
        kh.propagateCapacity();
        Belief bh;
        for (int c2 = 0; c2 < NCARD; c2++) for (int q = 0; q < NPLAY; q++) bh.marg[c2][q] = 0;
        for (int c2 = 0; c2 < NCARD; c2++) if (kh.owner[c2] < NPLAY) bh.marg[c2][kh.owner[c2]] = 1;
        bh.sinkhornDisj(kh, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
        PublicState ph = pub;
        ph.handCount[seat]++; ph.handCount[target]--;
        AskMove b2[NSET * SETSZ * 3];
        int n2 = enumerateAsks(ph, kh.myHand, seat, b2);
        for (int j = 0; j < n2; j++) follow = std::max(follow, bh.marg[b2[j].card][b2[j].target]);
      }
      if (cfg.threatWeight != 0 && p < 0.98) {
        Knowledge km = k;
        km.exclude(card, target);
        km.propagateCapacity();
        Belief bm;
        for (int c2 = 0; c2 < NCARD; c2++) for (int q = 0; q < NPLAY; q++) bm.marg[c2][q] = 0;
        for (int c2 = 0; c2 < NCARD; c2++) if (km.owner[c2] < NPLAY) bm.marg[c2][km.owner[c2]] = 1;
        bm.sinkhornDisj(km, cfg.sinkOuter, cfg.sinkInner, cfg.priorTheta, cfg.priorPhi);
        for (int st = 0; st < NSET; st++) {
          if (!pub.setActive[st]) continue;
          double none = 1, bestCard = 0;
          for (int j = 0; j < SETSZ; j++) {
            int c2 = cardOf(st, j);
            double pt = bm.marg[c2][target];
            none *= (1 - pt);
            double fr = 0;
            for (int q = 0; q < NPLAY; q++) if (teamMask & (1 << q)) fr += bm.marg[c2][q];
            bestCard = std::max(bestCard, fr);
          }
          threat = std::max(threat, (1 - none) * bestCard);
        }
      }
      double v = cs[r].u + cfg.chainWeight * p * follow - cfg.threatWeight * (1 - p) * threat;
      refined.push_back({v, i});
    }
    std::sort(refined.begin(), refined.end(),
              [](const std::pair<double,int>& a, const std::pair<double,int>& b) { return a.first > b.first; });

    AskMove pick = buf[refined[0].second];
    double pickP = bel.marg[pick.card][pick.target];

    if (sink) {
      DecRec d{};
      d.nLegal = nLegal; d.nLive = n;
      d.u1 = refined[0].first;
      d.u2 = refined.size() > 1 ? refined[1].first : refined[0].first - 1e9;
      d.sdAll = sd; d.rangeAll = hi - lo;
      d.p1 = pickP;
      d.p2 = refined.size() > 1 ? bel.marg[buf[refined[1].second].card][buf[refined[1].second].target] : 0;
      d.truth1 = gm && (gm->g.hand[pick.target] & bit(pick.card));
      AskMove alt = refined.size() > 1 ? buf[refined[1].second] : pick;
      d.truth2 = gm && (gm->g.hand[alt.target] & bit(alt.card));
      d.nHitAvail = nHitAvail;
      d.rankOfBestHit = -1;
      for (size_t r = 0; r < refined.size(); r++) {
        AskMove m = buf[refined[r].second];
        if (gm && (gm->g.hand[m.target] & bit(m.card))) { d.rankOfBestHit = int(r); break; }
      }
      d.lin1EqPick = (refined[0].second == linBest);
      d.sameSetSameTarget = (alt.target == pick.target) && (setOf(alt.card) == setOf(pick.card));
      d.exactTie = refined.size() > 1 && (refined[0].first == refined[1].first);
      sink->push_back(d);
    }

    lastMySet = setOf(pick.card);
    lastAskP = pickP;
    return pick;
  }
};

// -------------------------------------------------------- ask-oracle probe
// Identical to v0.5 in every respect except the final tie-break: among the
// candidates it already ranked, it takes one that truly hits when one exists.
// `scope` 0 = top-K refined only, 1 = every live candidate.
struct AskOracleV05 : V05Agent {
  const Game* gm = nullptr;
  int scope = 0;
  AskMove chooseAsk(const PublicState& pub) override {
    AskMove pick = V05Agent::chooseAsk(pub);
    if (gm && (gm->g.hand[pick.target] & bit(pick.card))) return pick;
    // rebuild the candidate list the same way and take a true hit if available
    AskMove buf[NSET * SETSZ * 3];
    int n = cfg.liveAskGate ? enumerateLive(pub, buf)
                            : enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return pick;
    if (scope == 1) {
      for (int i = 0; i < n; i++)
        if (gm->g.hand[buf[i].target] & bit(buf[i].card)) return buf[i];
      return pick;
    }
    // top-K by the linear score, exactly as chooseAsk ranks before refinement
    double f[NFEAT];
    struct C { double u; int i; };
    std::vector<C> cs(n);
    if (cfg.useValue) computeAggregates(pub);
    prepareRunway(pub);
    for (int i = 0; i < n; i++) {
      features(pub, buf[i].card, buf[i].target, f);
      double u = 0; for (int j = 0; j < NFEAT; j++) u += cfg.w[j] * f[j];
      u *= cfg.linearWeight;
      if (cfg.useValue) u += cfg.valueWeight * askExpectedValue(pub, buf[i].card, buf[i].target, f[0]);
      cs[i] = C{u, i};
    }
    int K = std::min(cfg.searchTopK, n);
    std::partial_sort(cs.begin(), cs.begin() + K, cs.end(), [](const C& a, const C& b) { return a.u > b.u; });
    for (int r = 0; r < K; r++) {
      AskMove m = buf[cs[r].i];
      if (gm->g.hand[m.target] & bit(m.card)) return m;
    }
    return pick;
  }
};

// ------------------------------------------ perfect-information reference
// Sees the true hands.  Asks only cards the target actually holds (preferring the
// half-suit its team is closest to completing); declares a half-suit the moment
// its team truly holds all six (eager) or when it can no longer ask productively
// (patient).  This is a cheating upper reference, not a solved-game optimum.
struct CheatAgent : Agent {
  const Game* gm = nullptr;
  bool patient = false;
  const char* name() const override { return "cheat"; }

  int teamCount(int s) const {
    int n = 0;
    for (int i = 0; i < SETSZ; i++) { int c = cardOf(s, i);
      for (int p = teamOf(seat); p < NPLAY; p += 2) if (gm->g.hand[p] & bit(c)) n++; }
    return n;
  }
  bool locked(int s) const { return teamCount(s) == SETSZ; }

  AskMove chooseAsk(const PublicState& pub) override {
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    if (!n) return AskMove{0, 0};
    int best = -1; double bestScore = -1e18;
    for (int i = 0; i < n; i++) {
      bool hit = (gm->g.hand[buf[i].target] & bit(buf[i].card)) != 0;
      double sc = hit ? 1000.0 + teamCount(setOf(buf[i].card)) * 10.0 - pub.handCount[buf[i].target]
                      : -double(pub.handCount[buf[i].target]);
      if (sc > bestScore) { bestScore = sc; best = i; }
    }
    return buf[best];
  }
  void fill(int s, Declaration& d) const {
    d.set = uint8_t(s);
    for (int i = 0; i < SETSZ; i++) { int c = cardOf(s, i);
      for (int p = 0; p < NPLAY; p++) if (gm->g.hand[p] & bit(c)) d.owner[i] = uint8_t(p); }
  }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s] || !locked(s)) continue;
      if (patient) {
        // Hold the lock while a true hit is still available to us outside it.
        AskMove buf[NSET * SETSZ * 3];
        int n = enumerateAsks(pub, k.myHand, seat, buf);
        bool productive = false;
        for (int i = 0; i < n && !productive; i++)
          if (setOf(buf[i].card) != s && (gm->g.hand[buf[i].target] & bit(buf[i].card))) productive = true;
        if (productive && pub.handCount[seat] > popcount64(k.myHand & setMask(s))) continue;
      }
      fill(s, d); conf = 1.0; return true;
    }
    return false;
  }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double th) override {
    if (!pub.setActive[set]) return false;
    if (!locked(set)) return false;
    fill(set, d); conf = 1.0; return true;
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    fill(set, d); conf = 1.0;
  }
};

// ------------------------------------------------------- set attribution
struct SetStat {
  long long sets = 0;
  long long byCorrectDecl = 0, byMisdecl = 0, byForced = 0, byForcedWrong = 0, byAdjudication = 0;
  // for the team that did NOT get the set:
  long long lostToMisdecl = 0;        // we held all six and named it wrong
  long long lostOppCorrect = 0;       // opponents legitimately collected and cashed
  long long lostAfterHavingLock = 0;  // subset: we once held all six, then lost a card
  long long lostNeverLock = 0;
  long long lostForced = 0, lostAdjud = 0;
  // lateness of correct declarations
  long long correctDecls = 0, lateSum = 0, late5 = 0, late10 = 0, late20 = 0, lateNoHorizon = 0;
  std::vector<int> lateHist;
  // lock economics
  long long everLocked = 0, lockBroken = 0, lockHoldSum = 0;
  // declaration evidence class, judged at the DECLARING SEAT
  long long declProof = 0, declProofWrong = 0, declBelief = 0, declBeliefWrong = 0;
  long long misdeclHeldSum = 0;   // cards of the set the declaring team truly held
  long long misdeclHeld6 = 0;     // ... and it held ALL six: a pure allocation error
  void merge(const SetStat& o) {
    declProof += o.declProof; declProofWrong += o.declProofWrong;
    declBelief += o.declBelief; declBeliefWrong += o.declBeliefWrong;
    misdeclHeldSum += o.misdeclHeldSum; misdeclHeld6 += o.misdeclHeld6;
    sets += o.sets; byCorrectDecl += o.byCorrectDecl; byMisdecl += o.byMisdecl;
    byForced += o.byForced; byForcedWrong += o.byForcedWrong; byAdjudication += o.byAdjudication;
    lostToMisdecl += o.lostToMisdecl; lostOppCorrect += o.lostOppCorrect;
    lostAfterHavingLock += o.lostAfterHavingLock; lostNeverLock += o.lostNeverLock;
    lostForced += o.lostForced; lostAdjud += o.lostAdjud;
    correctDecls += o.correctDecls; lateSum += o.lateSum; late5 += o.late5; late10 += o.late10;
    late20 += o.late20; lateNoHorizon += o.lateNoHorizon;
    lateHist.insert(lateHist.end(), o.lateHist.begin(), o.lateHist.end());
    everLocked += o.everLocked; lockBroken += o.lockBroken; lockHoldSum += o.lockHoldSum;
  }
};

// Replay a traced game; per half-suit find:
//  - the first event index at which SOME seat could PROVE the whole allocation
//    (owner resolved for all six, all on that seat's team)  == declarable horizon
//  - whether a team ever held all six (lock), and whether the lock was broken
//  - how the set was finally awarded
static void attribute(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                      const Rules& rules, SetStat& st) {
  Knowledge k[NPLAY];
  for (int p = 0; p < NPLAY; p++) k[p].init(p, dealt[p], rules.deckSets);
  uint64_t hand[NPLAY];
  for (int p = 0; p < NPLAY; p++) hand[p] = dealt[p];

  int horizon[NSET][2];               // first event index a seat of team t could prove it
  int lockAt[NSET][2]; bool lockBroke[NSET][2];
  for (int s = 0; s < NSET; s++) for (int t = 0; t < 2; t++) {
    horizon[s][t] = -1; lockAt[s][t] = -1; lockBroke[s][t] = false;
  }
  bool alive[NSET];
  for (int s = 0; s < NSET; s++) alive[s] = (s < rules.deckSets);

  int idx = 0;
  auto snapshot = [&]() {
    for (int s = 0; s < NSET; s++) {
      if (!alive[s]) continue;
      // true lock
      for (int t = 0; t < 2; t++) {
        bool all = true;
        for (int i = 0; i < SETSZ && all; i++) {
          int c = cardOf(s, i); bool held = false;
          for (int p = t; p < NPLAY; p += 2) if (hand[p] & bit(c)) held = true;
          if (!held) all = false;
        }
        if (all) { if (lockAt[s][t] < 0) lockAt[s][t] = idx; }
        else if (lockAt[s][t] >= 0) lockBroke[s][t] = true;
      }
      // provable-allocation horizon
      for (int p = 0; p < NPLAY; p++) {
        int t = teamOf(p);
        if (horizon[s][t] >= 0) continue;
        bool ok = true;
        for (int i = 0; i < SETSZ && ok; i++) {
          int c = cardOf(s, i);
          if (k[p].owner[c] >= NPLAY) ok = false;
          else if (teamOf(k[p].owner[c]) != t) ok = false;
        }
        if (ok) horizon[s][t] = idx;
      }
    }
  };
  snapshot();

  for (const Event& e : ev) {
    if (e.kind == Kind::Ask && e.success) {
      hand[e.target] &= ~bit(e.card);
      hand[e.actor] |= bit(e.card);
    }
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      int dt = teamOf(e.actor);
      int aw = e.success ? dt : 1 - dt;
      int lose = 1 - aw;
      st.sets++;
      {
        // Could the declaring seat PROVE the whole allocation at this instant?
        bool proof = true;
        for (int i = 0; i < SETSZ && proof; i++) {
          int c = cardOf(e.set, i);
          if (k[e.actor].owner[c] >= NPLAY || teamOf(k[e.actor].owner[c]) != dt) proof = false;
        }
        if (proof) { st.declProof++; if (!e.success) st.declProofWrong++; }
        else       { st.declBelief++; if (!e.success) st.declBeliefWrong++; }
        if (!e.success) {
          int held = 0;
          for (int i = 0; i < SETSZ; i++) { int c = cardOf(e.set, i);
            for (int p = dt; p < NPLAY; p += 2) if (hand[p] & bit(c)) held++; }
          st.misdeclHeldSum += held;
          if (held == SETSZ) st.misdeclHeld6++;
        }
      }
      if (e.kind == Kind::ForcedDeclare) {
        st.byForced++;
        if (!e.success) st.byForcedWrong++;
        st.lostForced++;
      } else if (e.success) {
        st.byCorrectDecl++;
        st.correctDecls++;
        int h = horizon[e.set][dt];
        if (h < 0) st.lateNoHorizon++;
        else {
          int late = idx - h;
          st.lateSum += late; st.lateHist.push_back(late);
          if (late >= 5) st.late5++;
          if (late >= 10) st.late10++;
          if (late >= 20) st.late20++;
        }
        st.lostOppCorrect++;
        if (lockAt[e.set][lose] >= 0) st.lostAfterHavingLock++; else st.lostNeverLock++;
      } else {
        st.byMisdecl++;
        st.lostToMisdecl++;
      }
      for (int t = 0; t < 2; t++) if (lockAt[e.set][t] >= 0) {
        st.everLocked++;
        if (lockBroke[e.set][t]) st.lockBroken++;
        st.lockHoldSum += idx - lockAt[e.set][t];
      }
      alive[e.set] = false;
      for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
    }
    for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
    idx++;
    snapshot();
  }
  // anything still alive was adjudicated
  for (int s = 0; s < NSET; s++) if (alive[s]) { st.sets++; st.byAdjudication++; st.lostAdjud++; }
}

int main(int argc, char** argv) {
  std::string mode = argc > 1 ? argv[1] : "margin";
  std::string aSpec = "v05", bSpec = "v05", beliefOpt;
  int games = 100, rots = 2;
  uint64_t seed = 31;
  for (int i = 2; i < argc; i++) {
    std::string s = argv[i];
    auto eq = s.find('=');
    if (eq == std::string::npos) continue;
    std::string kk = s.substr(2, eq - 2), vv = s.substr(eq + 1);
    if (kk == "a") aSpec = vv; else if (kk == "b") bSpec = vv;
    else if (kk == "games") games = atoi(vv.c_str());
    else if (kk == "rotations") rots = atoi(vv.c_str());
    else if (kk == "seed") seed = strtoull(vv.c_str(), nullptr, 10);
    else if (kk == "belief") beliefOpt = vv;
  }
  Rules r;

  if (mode == "margin") {
    std::vector<DecRec> recs;
    ProbeV05 pr[3];
    std::unique_ptr<Agent> B[3];
    for (int i = 0; i < 3; i++) B[i] = makeAgent(bSpec);
    Game game;
    for (int i = 0; i < 3; i++) {
      pr[i].sink = &recs; pr[i].gm = &game;
      if (beliefOpt == "block") pr[i].cfg.belief = BeliefMode::Block;
      else if (beliefOpt == "exact") pr[i].cfg.belief = BeliefMode::Exact;
      else if (beliefOpt == "indep") pr[i].cfg.belief = BeliefMode::Independent;
    }
    Agent* ag[NPLAY];
    for (int gi = 0; gi < games; gi++) {
      for (int rot = 0; rot < rots; rot++) {
        int orient = (rots == 2) ? rot : (rot / 3);
        int shift  = (rots == 2) ? 0   : (rot % 3);
        for (int p = 0; p < NPLAY; p++)
          ag[p] = (teamOf(p) == orient) ? (Agent*)&pr[p / 2] : B[p / 2].get();
        game.rotation = shift;
        game.run(mixSeed(seed, uint64_t(gi) * 2654435761ull + 1), r, ag);
      }
    }
    long long N = (long long)recs.size();
    long long forced = 0, forcedLive = 0, oneLive = 0;
    long long tie05 = 0, tie10 = 0, tie25 = 0, decisive = 0;
    long long flipDiff = 0, flipDiffTie = 0;
    long long hits = 0, missNoHitAvail = 0, missHitAvail = 0, hitAvailTotal = 0;
    long long bestHitRank0 = 0, bestHitInK = 0;
    long long linNePick = 0;
    double sumMarginOverSd = 0; long long nSd = 0;
    std::vector<double> ratios;
    for (const DecRec& d : recs) {
      if (d.nLegal <= 1) forced++;
      if (d.nLive <= 1) oneLive++;
      if (d.nLegal <= 1 && d.nLive <= 1) forcedLive++;
      double m = d.u1 - d.u2;
      if (d.nLive > 1 && d.sdAll > 1e-12) {
        double ratio = m / d.sdAll;
        ratios.push_back(ratio);
        sumMarginOverSd += ratio; nSd++;
        if (ratio < 0.05) tie05++;
        if (ratio < 0.10) tie10++;
        if (ratio < 0.25) tie25++;
        if (ratio >= 0.50) decisive++;
        if (d.truth1 != d.truth2) { flipDiff++; if (ratio < 0.25) flipDiffTie++; }
      }
      if (d.truth1) hits++;
      else { if (d.nHitAvail == 0) missNoHitAvail++; else missHitAvail++; }
      hitAvailTotal += d.nHitAvail;
      if (d.rankOfBestHit == 0) bestHitRank0++;
      if (d.rankOfBestHit >= 0) bestHitInK++;
      if (!d.lin1EqPick) linNePick++;
    }
    std::sort(ratios.begin(), ratios.end());
    auto qq = [&](double f) { return ratios.empty() ? 0.0 : ratios[std::min(ratios.size() - 1, size_t(f * ratios.size()))]; };
    auto pc = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
    printf("=== ask-decision margin probe (%s mirror-team vs %s) ===\n", "v05", bSpec.c_str());
    printf("decisions                    %lld\n", N);
    printf("FORCED (1 legal ask)         %lld  (%.3f%%)\n", forced, pc(forced, N));
    printf("1 live candidate after M1    %lld  (%.3f%%)\n", oneLive, pc(oneLive, N));
    printf("candidates: mean legal %.2f  mean live %.2f\n",
           [&]{ double s=0; for (auto&d:recs) s+=d.nLegal; return N? s/N:0; }(),
           [&]{ double s=0; for (auto&d:recs) s+=d.nLive;  return N? s/N:0; }());
    printf("margin/sd  mean %.3f  p10 %.3f  p25 %.3f  median %.3f  p75 %.3f  p90 %.3f\n",
           nSd ? sumMarginOverSd / nSd : 0.0, qq(0.10), qq(0.25), qq(0.50), qq(0.75), qq(0.90));
    printf("NEAR-TIE margin/sd < 0.05    %lld  (%.2f%% of non-forced)\n", tie05, pc(tie05, (long long)ratios.size()));
    printf("NEAR-TIE margin/sd < 0.10    %lld  (%.2f%%)\n", tie10, pc(tie10, (long long)ratios.size()));
    printf("NEAR-TIE margin/sd < 0.25    %lld  (%.2f%%)\n", tie25, pc(tie25, (long long)ratios.size()));
    printf("DECISIVE margin/sd >= 0.50   %lld  (%.2f%%)\n", decisive, pc(decisive, (long long)ratios.size()));
    printf("top-2 differ in TRUE outcome %lld  (%.2f%% of non-forced);  of those, margin/sd<0.25: %lld (%.2f%%)\n",
           flipDiff, pc(flipDiff, (long long)ratios.size()), flipDiffTie, pc(flipDiffTie, flipDiff));
    printf("final pick != linear argmax  %lld  (%.2f%%)  -- top-K refinement changes the move\n",
           linNePick, pc(linNePick, N));
    printf("--- hindsight ---\n");
    printf("hit rate                     %.3f%%\n", pc(hits, N));
    printf("hitting legal asks available mean %.3f per decision\n", N ? double(hitAvailTotal) / N : 0.0);
    printf("MISSES with NO hitting ask   %lld  (%.2f%% of misses; unavoidable)\n",
           missNoHitAvail, pc(missNoHitAvail, N - hits));
    printf("MISSES with a hitting ask    %lld  (%.2f%% of misses; recoverable)\n",
           missHitAvail, pc(missHitAvail, N - hits));
    printf("a hitting ask ranked #1      %lld  (%.2f%% of decisions)\n", bestHitRank0, pc(bestHitRank0, N));
    printf("a hitting ask inside top-K   %lld  (%.2f%% of decisions)\n", bestHitInK, pc(bestHitInK, N));
    long long exact = 0, exactSameP = 0, exactSameSetTgt = 0, exactFlip = 0, exactSamePFlip = 0;
    long long noHitAvail = 0;
    for (const DecRec& d : recs) {
      if (d.nHitAvail == 0) noHitAvail++;
      if (!d.exactTie) continue;
      exact++;
      if (std::fabs(d.p1 - d.p2) < 1e-12) exactSameP++;
      if (d.sameSetSameTarget) exactSameSetTgt++;
      if (d.truth1 != d.truth2) { exactFlip++; if (std::fabs(d.p1 - d.p2) < 1e-12) exactSamePFlip++; }
    }
    printf("--- tie anatomy ---\n");
    printf("EXACT tie u1==u2             %lld  (%.2f%% of decisions)\n", exact, pc(exact, N));
    printf("  ... same posterior p1==p2  %lld  (%.2f%% of exact ties)  -- informationally symmetric, irreducible\n",
           exactSameP, pc(exactSameP, exact));
    printf("  ... same half-suit+target  %lld  (%.2f%% of exact ties)\n", exactSameSetTgt, pc(exactSameSetTgt, exact));
    printf("  ... top-2 differ in truth  %lld  (%.2f%% of exact ties), of which same-p %lld\n",
           exactFlip, pc(exactFlip, exact), exactSamePFlip);
    printf("decisions with NO hitting ask %lld  (%.2f%%)  -> perfect-information ask ceiling %.2f%%\n",
           noHitAvail, pc(noHitAvail, N), 100.0 - pc(noHitAvail, N));
    return 0;
  }

  if (mode == "sets") {
    SetStat st;
    std::unique_ptr<Agent> A[3], B[3];
    for (int i = 0; i < 3; i++) { A[i] = makeAgent(aSpec); B[i] = makeAgent(bSpec); }
    Game game; game.trace.on = true;
    Agent* ag[NPLAY];
    for (int gi = 0; gi < games; gi++) {
      for (int rot = 0; rot < rots; rot++) {
        int orient = (rots == 2) ? rot : (rot / 3);
        int shift  = (rots == 2) ? 0   : (rot % 3);
        for (int p = 0; p < NPLAY; p++)
          ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
        game.rotation = shift;
        game.trace.events.clear();
        game.run(mixSeed(seed, uint64_t(gi) * 2654435761ull + 1), r, ag);
        attribute(game.trace.events, game.g.dealt, r, st);
      }
    }
    auto pc = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
    std::sort(st.lateHist.begin(), st.lateHist.end());
    auto qq = [&](double f) { return st.lateHist.empty() ? 0 : st.lateHist[std::min(st.lateHist.size()-1, size_t(f*st.lateHist.size()))]; };
    printf("=== half-suit attribution (%s vs %s, %d deals x %d rot) ===\n", aSpec.c_str(), bSpec.c_str(), games, rots);
    printf("half-suits resolved          %lld\n", st.sets);
    printf("  by correct declaration     %lld  (%.2f%%)\n", st.byCorrectDecl, pc(st.byCorrectDecl, st.sets));
    printf("  by MISDECLARATION (gift)   %lld  (%.2f%%)\n", st.byMisdecl, pc(st.byMisdecl, st.sets));
    printf("  by forced endgame          %lld  (%.2f%%, wrong %lld)\n", st.byForced, pc(st.byForced, st.sets), st.byForcedWrong);
    printf("  by adjudication            %lld  (%.2f%%)\n", st.byAdjudication, pc(st.byAdjudication, st.sets));
    printf("--- for the team that LOST each half-suit ---\n");
    printf("  own misdeclaration         %lld  (%.2f%%)\n", st.lostToMisdecl, pc(st.lostToMisdecl, st.sets));
    printf("  opponent cashed correctly  %lld  (%.2f%%)\n", st.lostOppCorrect, pc(st.lostOppCorrect, st.sets));
    printf("     ... and WE HAD THE LOCK once  %lld  (%.2f%% of all half-suits)\n",
           st.lostAfterHavingLock, pc(st.lostAfterHavingLock, st.sets));
    printf("     ... never had the lock        %lld  (%.2f%%)\n", st.lostNeverLock, pc(st.lostNeverLock, st.sets));
    printf("  forced endgame             %lld\n", st.lostForced);
    printf("  adjudication               %lld\n", st.lostAdjud);
    printf("--- lateness of correct declarations (events after the declarer's team could PROVE the allocation) ---\n");
    printf("  correct declarations       %lld  (no proof horizon before the declaration: %lld)\n", st.correctDecls, st.lateNoHorizon);
    printf("  mean lateness              %.3f events;  median %d  p75 %d  p90 %d  p99 %d  max %d\n",
           st.lateHist.empty() ? 0.0 : double(st.lateSum) / st.lateHist.size(),
           qq(0.5), qq(0.75), qq(0.90), qq(0.99), st.lateHist.empty() ? 0 : st.lateHist.back());
    printf("  late >= 5  events          %lld  (%.2f%%)\n", st.late5, pc(st.late5, (long long)st.lateHist.size()));
    printf("  late >= 10 events          %lld  (%.2f%%)\n", st.late10, pc(st.late10, (long long)st.lateHist.size()));
    printf("  late >= 20 events          %lld  (%.2f%%)\n", st.late20, pc(st.late20, (long long)st.lateHist.size()));
    printf("--- lock economics ---\n");
    printf("  (half-suit, team) locks    %lld\n", st.everLocked);
    printf("  locks BROKEN before cash   %lld  (%.2f%%)\n", st.lockBroken, pc(st.lockBroken, st.everLocked));
    printf("  mean events lock held      %.3f\n", st.everLocked ? double(st.lockHoldSum) / st.everLocked : 0.0);
    printf("--- evidence class of each declaration, at the declaring seat ---\n");
    printf("  PROVED allocation          %lld  (%.2f%%)  wrong %lld (%.3f%%)\n",
           st.declProof, pc(st.declProof, st.declProof + st.declBelief), st.declProofWrong, pc(st.declProofWrong, st.declProof));
    printf("  GUESSED (belief only)      %lld  (%.2f%%)  wrong %lld (%.3f%%)\n",
           st.declBelief, pc(st.declBelief, st.declProof + st.declBelief), st.declBeliefWrong, pc(st.declBeliefWrong, st.declBelief));
    long long mw = st.byMisdecl + st.byForcedWrong;
    printf("  misdeclarations: mean cards of the set the declaring team really held %.3f / 6\n",
           mw ? double(st.misdeclHeldSum) / double(mw) : 0.0);
    printf("  misdeclarations where the team HELD ALL SIX (pure allocation error) %lld / %lld (%.2f%%)\n",
           st.misdeclHeld6, mw, pc(st.misdeclHeld6, mw));
    return 0;
  }
  if (mode == "askoracle" || mode == "askoracleall") {
    AskOracleV05 ao[3];
    for (int i = 0; i < 3; i++) ao[i].scope = (mode == "askoracleall") ? 1 : 0;
    std::unique_ptr<Agent> B[3];
    for (int i = 0; i < 3; i++) B[i] = makeAgent(bSpec);
    Game game;
    for (int i = 0; i < 3; i++) ao[i].gm = &game;
    Agent* ag[NPLAY];
    long long wins = 0, tiesG = 0, setsA = 0, setsB = 0, gtot = 0, evsum = 0;
    for (int gi = 0; gi < games; gi++) {
      for (int rot = 0; rot < rots; rot++) {
        int orient = (rots == 2) ? rot : (rot / 3);
        int shift  = (rots == 2) ? 0   : (rot % 3);
        for (int p = 0; p < NPLAY; p++)
          ag[p] = (teamOf(p) == orient) ? (Agent*)&ao[p / 2] : B[p / 2].get();
        game.rotation = shift;
        GameResult res = game.run(mixSeed(seed, uint64_t(gi) * 2654435761ull + 1), r, ag);
        int a = res.score[orient], b = res.score[1 - orient];
        setsA += a; setsB += b; gtot++;
        if (a > b) wins++; else if (a == b) tiesG++;
        evsum += res.events;
      }
    }
    printf("=== %s vs %s ===\n", mode.c_str(), bSpec.c_str());
    printf("games %lld  win rate %.4f%%  mean sets %.4f - %.4f  events/game %.2f\n",
           gtot, 100.0 * (double(wins) + 0.5 * double(tiesG)) / double(gtot),
           double(setsA) / gtot, double(setsB) / gtot, double(evsum) / gtot);
    return 0;
  }

  if (mode == "cheat" || mode == "cheatpatient") {
    CheatAgent ch[3];
    for (int i = 0; i < 3; i++) ch[i].patient = (mode == "cheatpatient");
    std::unique_ptr<Agent> B[3];
    for (int i = 0; i < 3; i++) B[i] = makeAgent(bSpec);
    Game game;
    for (int i = 0; i < 3; i++) ch[i].gm = &game;
    Agent* ag[NPLAY];
    long long wins = 0, ties = 0, setsA = 0, setsB = 0, gtot = 0, limit = 0, evsum = 0;
    for (int gi = 0; gi < games; gi++) {
      for (int rot = 0; rot < rots; rot++) {
        int orient = (rots == 2) ? rot : (rot / 3);
        int shift  = (rots == 2) ? 0   : (rot % 3);
        for (int p = 0; p < NPLAY; p++)
          ag[p] = (teamOf(p) == orient) ? (Agent*)&ch[p / 2] : B[p / 2].get();
        game.rotation = shift;
        GameResult res = game.run(mixSeed(seed, uint64_t(gi) * 2654435761ull + 1), r, ag);
        int a = res.score[orient], b = res.score[1 - orient];
        setsA += a; setsB += b; gtot++;
        if (a > b) wins++; else if (a == b) ties++;
        if (res.hitLimit) limit++;
        evsum += res.events;
      }
    }
    double wr = 100.0 * (double(wins) + 0.5 * double(ties)) / double(gtot);
    printf("=== %s vs %s ===\n", mode.c_str(), bSpec.c_str());
    printf("games %lld  win rate %.4f%%  mean sets %.4f - %.4f  events/game %.2f  limit hits %lld\n",
           gtot, wr, double(setsA) / gtot, double(setsB) / gtot, double(evsum) / gtot, limit);
    return 0;
  }
  fprintf(stderr, "usage: v06probe <margin|sets|cheat|cheatpatient> [--a=SPEC] [--b=SPEC] [--games=N] [--rotations=R] [--seed=S]\n");
  return 2;
}
