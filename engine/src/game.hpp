// Rules-exact game driver.  Agents never receive hidden state: they are handed
// their own hand once and a stream of public events thereafter.
#pragma once
#include "fish.hpp"
#include "belief.hpp"
#include <functional>

namespace fish {

struct DeclProposal { Declaration decl; double conf = 0; bool want = false; };

// ---------------------------------------------------------------- v0.7 D1
// The per-decision evaluation channel.
//
// "A study that wants to find a quarter-point mechanism will need a
// per-decision objective rather than a per-game one"
// (paper/sections_v06/14-conclusion.tex).  The arithmetic behind that sentence
// is 98/sqrt(N): at a thousand deals a cell this policy class looks flat, and
// the corpus documents six mechanisms that resolved at a small budget and
// vanished at a large one.  A declaration fires 4.48 times a game and its
// errors are individually labelled by ground truth, so scoring the mechanism on
// DECLARATIONS rather than on GAMES buys roughly the ratio of decisions to
// games in effective sample.
//
// Capture is opt-in through a thread-local flag rather than a member, because
// the agent that fills the record cannot see the Game that owns the sink and
// the shipped hot path must not pay for a feature it does not use.  With the
// flag off the cost is one thread-local bool load per decision.
inline bool& decisionCapture() { static thread_local bool b = false; return b; }
// v0.7 phase 3 (K5).  The deal/rotation the current thread is playing, so a
// per-CANDIDATE capture hook can label its rows with the same clustering unit
// the bootstrap uses.  Written only on the capture path.
inline int32_t& captureDeal() { static thread_local int32_t d = 0; return d; }
inline int16_t& captureRot()  { static thread_local int16_t r = 0; return r; }

// ---------------------------------------------------------------- v0.7 A2
// The adversary's ex-ante correlation device.
//
// THREAT-MODEL.md T4 names three adversary correlation regimes and makes A2 --
// one vector plus a shared pre-play signal secret from the target -- the
// headline, because it is the TMECor class and the only regime under which a
// low score certifies anything.  The asymmetry is deliberate: "the adversary is
// a measuring instrument, not a competitor bound by the same charter" (section
// 4.3), and the v0.7 team itself is denied this by H1.
//
// The signal is drawn per game by the arena from a stream that no policy is
// handed and that no policy can observe.  It is common to the three adversary
// seats and unavailable to the target, which is exactly what a correlation
// device is.  A responder that does not read it is A1 by construction, so the
// two regimes differ by one option and are measured on the same code path.
inline uint64_t& correlationSignal() { static thread_local uint64_t s = 0; return s; }

// What the POLICY knows about the decision it just made.  Filled by the policy;
// everything ground-truth-dependent is filled by the driver.
struct DecisionInfo {
  int    nCand    = 0;      // candidates the policy actually ranked
  int    nTie     = 0;      // how many tied the leader bit for bit
  double p        = -1;     // the policy's own forecast for the chosen action
  double margin   = 0;      // score gap, leader to runner-up (0 inside a tie)
  double score    = 0;      // the leader's blueprint score
  bool   dead     = false;  // the chosen ask is provably dead from the actor's view
  bool   gateBound= false;  // the live-ask gate removed what would have been the argmax
  int    nFeasible= 0;      // feasible allocations enumerated (declaration path)
  double pAlloc   = -1;     // the policy's own P(named allocation correct)
  bool   searched = false;  // a test-time search ran at this decision
  bool   changed  = false;  // ... and it moved the choice off the blueprint's
  // v0.7 phase 2.  The declaration path's URGENCY predicate (v05.hpp:939-944):
  //   unresolvedCount <= patiencePool | oppCards <= oppCardFloor
  //   | pub.nEvents >= forceDeclareEvents | bestAskProbability < askFloor.
  // Three of those four inputs are PUBLIC and two are directly under an
  // opponent's control -- oppCards is the opposing team's own total hand count,
  // and pub.nEvents is the length of the game.  The thresholds are frozen
  // constants of a white-box target.  So "declare before you are ready" is a
  // state an adversary can drive the target into, and this flag is what turns
  // that from an argument into a measurement.
  bool   urgent   = false;  // the declaration decision was taken under urgency
  int    pressure = 0;      // the escalation rung (v05.hpp pressure())
  int    urgWhy   = 0;      // which urgency clauses fired: bit0 patiencePool,
                            // bit1 oppCardFloor, bit2 forceDeclareEvents,
                            // bit3 askFloor
  void clear() { *this = DecisionInfo{}; }
};

// One row per decision.  Ground truth fields are computed by the driver and are
// never visible to a policy.
struct DecisionRecord {
  int32_t deal = 0;
  int16_t rot = 0;
  int16_t event = 0;
  int8_t  seat = 0, team = 0, arm = 0;
  int8_t  kind = 0;         // 0 ask, 1 voluntary declaration, 2 forced declaration
  int8_t  card = 0, target = 0, set = 0;
  int8_t  hit = 0;          // ask: did it land.  declaration: was it correct
  int8_t  ownLocked = 0;    // GROUND TRUTH: this team already holds all six of `set`
  int8_t  oppLocked = 0;    // GROUND TRUTH: the opposing team already holds all six
  int8_t  truthHolder = -1; // GROUND TRUTH: who actually holds `card`
  int8_t  dead = 0, gateBound = 0, searched = 0, changed = 0;
  int16_t nCand = 0, nTie = 0, nFeasible = 0;
  float   p = -1, margin = 0, score = 0, pAlloc = -1;
  int16_t unresolved = 0;   // |unresolved| at the decision, from the actor's view
  int8_t  urgent = 0;       // declaration only: taken under the urgency predicate
  int8_t  pressure = 0;     // declaration only: the escalation rung
  int8_t  urgWhy = 0;       // declaration only: which urgency clauses fired
};

struct DecisionSink {
  std::vector<DecisionRecord> rows;
  int32_t deal = 0; int16_t rot = 0; int8_t arm = 0;
  bool teamAOnly = false; int teamA = 0;     // restrict to one arm's seats
  void reserveFor(int deals) { rows.reserve(size_t(deals) * 120); }
};

struct Agent {
  int seat = 0;
  Knowledge k;
  virtual ~Agent() = default;
  virtual const char* name() const = 0;
  virtual void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) {
    seat = s; k.init(s, hand, r.deckSets); (void)seed;
  }
  virtual void observe(const Event& e) { k.onEvent(e); }
  // v0.7 T1 (c).  A rollout reconstructs six agents at their information sets
  // KC x D times per searched decision and then OVERWRITES `k` on every one of
  // them, so the `k.init` inside reset() is pure waste -- 54 cards of setup
  // thrown away immediately.  This entry point does the same job without it and
  // is exactly equivalent to `reset(...); k = k0;`.
  virtual void resetWithKnowledge(int s, uint64_t hand, const Rules& r, uint64_t seed,
                                  const Knowledge& k0) { reset(s, hand, r, seed); k = k0; }
  virtual AskMove chooseAsk(const PublicState& pub) = 0;
  // Forecast attached to the most recent ask (negative when not modelled).
  virtual double lastAskForecast() const { return -1; }
  // Public-belief-state features for value-function fitting.
  virtual int valueFeatures(const PublicState& pub, double* f) { (void)pub; (void)f; return 0; }
  // Voluntary declaration, legal at any moment under the real rules.
  virtual bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) { (void)pub; (void)d; (void)conf; return false; }
  // Which live teammate receives the turn when this agent is cardless.
  virtual int choosePassTarget(const PublicState& pub, const int* cand, int n) { (void)pub; return cand[n - 1]; }
  // Forced endgame: only a willingness bit may be shared with teammates.
  virtual bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) {
    (void)pub; (void)set; (void)d; (void)conf; (void)threshold; return false;
  }
  // v0.7 D1: what the policy knows about the decision it just made.  Null when
  // the policy does not implement the channel.
  virtual const DecisionInfo* lastDecision() const { return nullptr; }
  virtual void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) {
    (void)pub; d.set = uint8_t(set); conf = 0;
    for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(seat);
  }
};

struct GameResult {
  int winner = 0;
  int score[2] = {0, 0};
  int asks = 0, hits = 0;
  int teamAsks[2] = {0, 0}, teamHits[2] = {0, 0};
  int decls[2] = {0, 0}, correctDecls[2] = {0, 0};
  int forcedDecls[2] = {0, 0}, forcedCorrect[2] = {0, 0};
  int events = 0;
  bool hitLimit = false;
  int leadChanges = 0;
  int turnsRetained[2] = {0, 0};
  double predSumHit[2] = {0, 0};      // calibration: summed P(hit) forecasts
  int predNHit[2] = {0, 0};
  double predSumDecl[2] = {0, 0};
  int predNDecl[2] = {0, 0};
  int outOfTurnDecls[2] = {0, 0};
  // Observational only, computed from ground truth and never shown to a policy:
  // how long a half-suit sat provably locked (all six cards on one team, so by
  // Theorem 1 unstealable) before that team cashed it.
  long long lockHeldEvents[2] = {0, 0};
  int lockedDeclarations[2] = {0, 0};
};

struct Trace { bool on = false; std::vector<Event> events; };

static constexpr int VFEAT_MAX = 24;

// Value-function training rows: state features at a decision point, labelled
// with the final set differential of the team that was deciding.
struct ValueSink {
  std::vector<std::array<float, VFEAT_MAX>> X;
  std::vector<float> y;
  std::vector<int> team;
};

// Probability forecasts paired with realised outcomes, for reliability diagrams.
struct CalibSink {
  std::vector<std::pair<float, uint8_t>> ask;    // P(hit)          , hit?
  std::vector<std::pair<float, uint8_t>> decl;   // P(alloc correct), correct?
};

class Game {
public:
  Rules rules;
  GameState g;
  Agent* agents[NPLAY];
  Trace trace;
  GameResult res;
  uint64_t seed = 0;
  CalibSink* calib = nullptr;
  int calibTeam = -1;
  ValueSink* vsink = nullptr;
  int vsinkStart = 0;
  DecisionSink* dsink = nullptr;      // v0.7 D1: per-decision records
  int lockedAt[NSET];

  int rotation = 0;   // duplicate-block rotation: seat i receives hand (i+rotation)%6

  void setup(uint64_t s, const Rules& r, Agent** ag) {
    seed = s; rules = r; g = GameState{};
    g.pub.rules = r;
    dealCards(g, s, r.deckSets);
    if (rotation % NPLAY) {
      uint64_t tmp[NPLAY];
      for (int p = 0; p < NPLAY; p++) tmp[p] = g.hand[(p + rotation) % NPLAY];
      for (int p = 0; p < NPLAY; p++) { g.hand[p] = tmp[p]; g.dealt[p] = tmp[p]; }
    }
    for (int i = 0; i < NSET; i++) { g.pub.setActive[i] = (i < r.deckSets); g.setWinner[i] = 2; }
    for (int p = 0; p < NPLAY; p++) { g.pub.handCount[p] = uint8_t(popcount64(g.hand[p])); agents[p] = ag[p]; }
    g.pub.score[0] = g.pub.score[1] = 0;
    g.pub.turn = g.turn;
    res = GameResult{};
    for (int i = 0; i < NSET; i++) lockedAt[i] = -1;
    for (int p = 0; p < NPLAY; p++) agents[p]->reset(p, g.hand[p], r, mixSeed(s, uint64_t(p) + 77));
  }

  bool audit = false;
  long long auditViolations = 0;
  long long auditChecks = 0;

  // Soundness check: no agent's deduced knowledge may ever exclude the truth.
  void runAudit() {
    for (int p = 0; p < NPLAY; p++) {
      const Knowledge& kk = agents[p]->k;
      int cap[NPLAY] = {0,0,0,0,0,0};
      for (int c = 0; c < NCARD; c++) {
        if (!g.pub.setActive[setOf(c)]) continue;
        int truth = -1;
        for (int q = 0; q < NPLAY; q++) if (g.hand[q] & bit(c)) truth = q;
        if (truth < 0) { auditViolations++; continue; }
        auditChecks++;
        if (kk.owner[c] < NPLAY) { if (kk.owner[c] != truth) auditViolations++; }
        else if (kk.owner[c] == OUT_OF_PLAY) auditViolations++;
        else { if (!(kk.mask[c] & (1u << truth))) auditViolations++; cap[truth]++; }
      }
      uint8_t q[NPLAY]; kk.capacities(q);
      for (int r = 0; r < NPLAY; r++) if (q[r] != cap[r]) auditViolations++;
      for (const auto& d : kk.disj) {
        bool sat = false;
        uint64_t cc = d.cards;
        while (cc) { int x = __builtin_ctzll(cc); cc &= cc - 1;
          int truth = -1;
          for (int qq = 0; qq < NPLAY; qq++) if (g.hand[qq] & bit(x)) truth = qq;
          if (truth == d.player) sat = true; }
        if (!sat) auditViolations++;
      }
    }
  }

  void emit(Event& e) {
    for (int p = 0; p < NPLAY; p++) e.handCount[p] = g.pub.handCount[p];
    g.pub.nEvents++; res.events++;
    if (trace.on) trace.events.push_back(e);
    g.pub.history.push_back(e);
    for (int p = 0; p < NPLAY; p++) agents[p]->observe(e);
    for (int st = 0; st < NSET; st++) {
      if (!g.pub.setActive[st] || lockedAt[st] >= 0) continue;
      uint64_t t0 = g.hand[0] | g.hand[2] | g.hand[4];
      uint64_t t1 = g.hand[1] | g.hand[3] | g.hand[5];
      uint64_t m = setMask(st);
      if ((t0 & m) == m || (t1 & m) == m) lockedAt[st] = g.pub.nEvents;
    }
    if (audit) runAudit();
    if (observer) observer(*this);
  }

  // Optional post-event hook.  Used by the brute-force oracle to inspect every
  // agent's belief state at every public event; unset in ordinary play, so it
  // costs one null check per event and changes nothing.
  std::function<void(const Game&)> observer;

  // v0.7 D1.  Ground-truth annotation of a decision record.  Reads g.hand, so it
  // exists only on the driver side and is never reachable from a policy.
  bool teamOwnsAll(int st, int team) const {
    uint64_t t = 0;
    for (int p = team; p < NPLAY; p += 2) t |= g.hand[p];
    return (t & setMask(st)) == setMask(st);
  }
  bool wantRecord(int seat) const {
    if (!dsink) return false;
    if (dsink->teamAOnly && teamOf(seat) != dsink->teamA) return false;
    return true;
  }
  void baseRecord(DecisionRecord& r, int seat) const {
    r.deal = dsink->deal; r.rot = dsink->rot; r.arm = dsink->arm;
    r.event = int16_t(g.pub.nEvents);
    r.seat = int8_t(seat); r.team = int8_t(teamOf(seat));
    r.unresolved = int16_t(__builtin_popcountll(agents[seat]->k.unresolved));
  }

  void applyDeclaration(int actor, const Declaration& d, bool forced, double conf) {
    int team = teamOf(actor);
    bool correct = true;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(d.set, i);
      int claimed = d.owner[i];
      if (teamOf(claimed) != team || !(g.hand[claimed] & bit(c))) { correct = false; break; }
    }
    if (wantRecord(actor)) {
      DecisionRecord r; baseRecord(r, actor);
      r.kind = forced ? 2 : 1;
      r.set = int8_t(d.set); r.card = -1; r.target = -1;
      r.hit = int8_t(correct ? 1 : 0);
      r.ownLocked = int8_t(teamOwnsAll(d.set, team) ? 1 : 0);
      r.oppLocked = int8_t(teamOwnsAll(d.set, 1 - team) ? 1 : 0);
      r.pAlloc = float(conf); r.p = float(conf);
      if (const DecisionInfo* di = agents[actor]->lastDecision()) {
        r.nFeasible = int16_t(di->nFeasible);
        r.nCand = int16_t(di->nCand);
        r.urgent = int8_t(di->urgent ? 1 : 0);
        r.pressure = int8_t(di->pressure);
        r.urgWhy = int8_t(di->urgWhy);
      }
      dsink->rows.push_back(r);
    }
    if (lockedAt[d.set] >= 0 && correct) {
      res.lockHeldEvents[team] += g.pub.nEvents - lockedAt[d.set];
      res.lockedDeclarations[team]++;
    }
    int awarded = correct ? team : 1 - team;
    int before = g.pub.score[0] - g.pub.score[1];
    g.pub.score[awarded]++;
    int after = g.pub.score[0] - g.pub.score[1];
    if (before != 0 && after != 0 && ((before > 0) != (after > 0))) res.leadChanges++;
    g.setWinner[d.set] = uint8_t(awarded);
    g.pub.setActive[d.set] = false;
    for (int p = 0; p < NPLAY; p++) { g.hand[p] &= ~setMask(d.set); g.pub.handCount[p] = uint8_t(popcount64(g.hand[p])); }
    if (calib && teamOf(actor) == calibTeam && conf >= 0)
      calib->decl.push_back({float(conf), uint8_t(correct ? 1 : 0)});
    if (forced) { res.forcedDecls[team]++; if (correct) res.forcedCorrect[team]++; }
    else { res.decls[team]++; if (correct) res.correctDecls[team]++;
           res.predSumDecl[team] += conf; res.predNDecl[team]++;
           if (actor != g.turn) res.outOfTurnDecls[team]++; }
    Event e{}; e.kind = forced ? Kind::ForcedDeclare : Kind::Declare;
    e.actor = uint8_t(actor); e.set = d.set; e.success = correct; e.decl = d; e.confidence = conf;
    emit(e);
  }

  // Poll every agent for a voluntary declaration and execute the strongest.
  bool declarationRound() {
    bool any = false;
    for (int round = 0; round < NSET + 2; round++) {
      // Arbitration between simultaneous declarations is a modelling choice, not
      // a rule.  Picking the most confident proposer would compare private
      // confidences across seats, which is exactly the leak the forced endgame
      // was corrected for, so we take the lowest seat that wants to declare and
      // let the confidence be a reported diagnostic only.
      int bestSeat = -1; double bestConf = -1; Declaration bestDecl{};
      for (int ord = 0; ord < NPLAY; ord++) {
        int p = rules.declArbitration == 1 ? (NPLAY - 1 - ord)
              : rules.declArbitration == 2 ? ((g.turn + ord) % NPLAY)
              : ord;
        if (!rules.outOfTurnDeclare && p != g.turn) continue;
        if (!rules.cardlessMayDeclare && !g.pub.handCount[p]) continue;
        Declaration d{}; double conf = 0;
        if (!agents[p]->proposeDeclaration(g.pub, d, conf)) continue;
        if (!g.pub.setActive[d.set]) continue;
        bool ok = true;
        for (int i = 0; i < SETSZ; i++) if (teamOf(d.owner[i]) != teamOf(p)) ok = false;
        if (!ok) continue;
        if (bestSeat < 0) { bestConf = conf; bestSeat = p; bestDecl = d; }
      }
      if (bestSeat < 0) break;
      applyDeclaration(bestSeat, bestDecl, false, bestConf);
      any = true;
      if (!g.pub.activeSets()) break;
    }
    return any;
  }

  // A whole team is cardless: the other team must declare every remaining set,
  // sharing nothing but willingness.
  void forcedEndgame(int declaringTeam) {
    for (int guard = 0; guard < 2 * NSET + 4 && g.pub.activeSets(); guard++) {
      bool did = false;
      for (int ti = 0; ti < rules.nForcedTh; ti++) {
        double th = rules.forcedTh[ti];
        for (int s = 0; s < NSET && !did; s++) {
          if (!g.pub.setActive[s]) continue;
          for (int p = declaringTeam; p < NPLAY; p += 2) {
            if (!rules.cardlessMayDeclare && !g.pub.handCount[p]) continue;
            Declaration d{}; double conf = 0;
            if (th < 0) { agents[p]->bestGuess(g.pub, s, d, conf); }
            else if (!agents[p]->willingForced(g.pub, s, d, conf, th)) continue;
            d.set = uint8_t(s);
            bool ok = true;
            for (int i = 0; i < SETSZ; i++) if (teamOf(d.owner[i]) != declaringTeam) ok = false;
            if (!ok) continue;
            applyDeclaration(p, d, true, conf);
            did = true; break;
          }
        }
        if (did) break;
      }
      if (!did) break;
    }
    // Anything still unresolved (only reachable if every agent refused) goes to
    // the team physically holding the majority.
    for (int s = 0; s < NSET; s++) if (g.pub.setActive[s]) {
      int cnt[2] = {0, 0};
      for (int i = 0; i < SETSZ; i++) { int c = cardOf(s, i);
        for (int p = 0; p < NPLAY; p++) if (g.hand[p] & bit(c)) cnt[teamOf(p)]++; }
      int aw = cnt[1] > cnt[0] ? 1 : 0;
      g.pub.score[aw]++; g.setWinner[s] = uint8_t(aw); g.pub.setActive[s] = false;
      for (int p = 0; p < NPLAY; p++) { g.hand[p] &= ~setMask(s); g.pub.handCount[p] = uint8_t(popcount64(g.hand[p])); }
    }
  }

  void adjudicateRemaining() {
    for (int s = 0; s < NSET; s++) {
      if (!g.pub.setActive[s]) continue;
      int cnt[2] = {0, 0};
      int lowTeam = 0; bool haveLow = false;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        for (int p = 0; p < NPLAY; p++) if (g.hand[p] & bit(c)) {
          cnt[teamOf(p)]++;
          if (!haveLow) { lowTeam = teamOf(p); haveLow = true; }
        }
      }
      int aw = cnt[1] > cnt[0] ? 1 : (cnt[0] > cnt[1] ? 0 : lowTeam);
      g.pub.score[aw]++; g.setWinner[s] = uint8_t(aw); g.pub.setActive[s] = false;
      for (int p = 0; p < NPLAY; p++) { g.hand[p] &= ~setMask(s); g.pub.handCount[p] = uint8_t(popcount64(g.hand[p])); }
    }
  }

  GameResult run(uint64_t s, const Rules& r, Agent** ag) {
    setup(s, r, ag);
    vsinkStart = vsink ? int(vsink->y.size()) : 0;
    int asks = 0;
    while (true) {
      declarationRound();
      if (!g.pub.activeSets()) break;
      bool alive0 = g.pub.teamAlive(0), alive1 = g.pub.teamAlive(1);
      if (!alive0 || !alive1) { forcedEndgame(alive0 ? 0 : 1); break; }
      if (!g.pub.handCount[g.turn]) {
        int cand[3], n = 0;
        for (int p = teamOf(g.turn); p < NPLAY; p += 2) if (g.pub.handCount[p]) cand[n++] = p;
        if (!n) { forcedEndgame(1 - teamOf(g.turn)); break; }
        int rcv = agents[g.turn]->choosePassTarget(g.pub, cand, n);
        bool valid = false; for (int i = 0; i < n; i++) if (cand[i] == rcv) valid = true;
        if (!valid) rcv = cand[0];
        Event e{}; e.kind = Kind::Pass; e.actor = uint8_t(g.turn); e.target = uint8_t(rcv);
        emit(e);
        g.turn = rcv; g.pub.turn = rcv;
        continue;
      }
      if (vsink) {
        // Sample every seat, not just the mover: features are relative to the
        // observing team, so collecting only the mover would leave the
        // side-to-move coefficient unidentified (it would always be +1).
        for (int p = 0; p < NPLAY; p++) {
          double f[VFEAT_MAX];
          int nf = agents[p]->valueFeatures(g.pub, f);
          if (nf <= 0) continue;
          std::array<float, VFEAT_MAX> row{};
          for (int i = 0; i < nf; i++) row[i] = float(f[i]);
          vsink->X.push_back(row);
          vsink->y.push_back(0);
          vsink->team.push_back(teamOf(p));
        }
      }
      AskMove mv = agents[g.turn]->chooseAsk(g.pub);
      if (!legalAsk(g, g.turn, mv.card, mv.target)) {
        AskMove buf[NSET * SETSZ * 3]; int n = enumerateAsks(g.pub, g.hand[g.turn], g.turn, buf);
        if (!n) {                        // holds only complete sets: must declare
          Declaration d{}; double conf = 0;
          int chosen = -1;
          for (int st = 0; st < NSET; st++) if (g.pub.setActive[st] && (g.hand[g.turn] & setMask(st))) { chosen = st; break; }
          if (chosen < 0) { res.hitLimit = true; break; }
          agents[g.turn]->bestGuess(g.pub, chosen, d, conf);
          d.set = uint8_t(chosen);
          applyDeclaration(g.turn, d, true, conf);
          continue;
        }
        mv = buf[0];
      }
      int actor = g.turn, target = mv.target, card = mv.card;
      bool success = (g.hand[target] & bit(card)) != 0;
      int team = teamOf(actor);
      if (wantRecord(actor)) {
        DecisionRecord r; baseRecord(r, actor);
        r.kind = 0;
        r.card = int8_t(card); r.target = int8_t(target); r.set = int8_t(setOf(card));
        r.hit = int8_t(success ? 1 : 0);
        r.ownLocked = int8_t(teamOwnsAll(setOf(card), team) ? 1 : 0);
        r.oppLocked = int8_t(teamOwnsAll(setOf(card), 1 - team) ? 1 : 0);
        for (int q = 0; q < NPLAY; q++) if (g.hand[q] & bit(card)) r.truthHolder = int8_t(q);
        if (const DecisionInfo* di = agents[actor]->lastDecision()) {
          r.nCand = int16_t(di->nCand); r.nTie = int16_t(di->nTie);
          r.p = float(di->p); r.margin = float(di->margin); r.score = float(di->score);
          r.dead = int8_t(di->dead ? 1 : 0); r.gateBound = int8_t(di->gateBound ? 1 : 0);
          r.searched = int8_t(di->searched ? 1 : 0); r.changed = int8_t(di->changed ? 1 : 0);
        }
        dsink->rows.push_back(r);
      }
      if (calib && team == calibTeam) {
        double f = agents[actor]->lastAskForecast();
        if (f >= 0) calib->ask.push_back({float(f), uint8_t(success ? 1 : 0)});
      }
      res.asks++; res.teamAsks[team]++; asks++;
      if (success) {
        res.hits++; res.teamHits[team]++; res.turnsRetained[team]++;
        g.hand[target] &= ~bit(card); g.hand[actor] |= bit(card);
      }
      g.pub.handCount[actor] = uint8_t(popcount64(g.hand[actor]));
      g.pub.handCount[target] = uint8_t(popcount64(g.hand[target]));
      Event e{}; e.kind = Kind::Ask; e.actor = uint8_t(actor); e.target = uint8_t(target);
      e.card = uint8_t(card); e.set = uint8_t(setOf(card)); e.success = success;
      emit(e);
      if (!success) { g.turn = target; g.pub.turn = target; }
      if (asks >= rules.maxAsks) {
        // Safety valve.  Two sufficiently patient policies can stall (Theorem 1
        // makes patience correct), so a cap is needed -- but handing the residue
        // to one team would bias the result, so unresolved half-suits are
        // adjudicated neutrally by who physically holds the majority, ties going
        // to whoever holds the lowest card of the half-suit.
        res.hitLimit = true;
        adjudicateRemaining();
        break;
      }
    }
    res.score[0] = g.pub.score[0]; res.score[1] = g.pub.score[1];
    res.winner = res.score[1] > res.score[0] ? 1 : 0;
    if (vsink) {
      double diff = (double(res.score[0]) - double(res.score[1])) / double(rules.deckSets);
      for (int i = vsinkStart; i < int(vsink->y.size()); i++)
        vsink->y[i] = float(vsink->team[i] == 0 ? diff : -diff);
    }
    return res;
  }
};

} // namespace fish
