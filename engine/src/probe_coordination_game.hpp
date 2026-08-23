// SCRATCH PROBE (P8, coordination channels).  A copy of fish::Game's driver in a
// nested namespace so that game.hpp is left untouched for other agents.  The
// only behavioural differences from fish::Game are:
//   * choosePassTarget may be overridden by a pass-selection POLICY (unilateral
//     = v0.4 shipped behaviour, oracle = ground truth, willingness ladder);
//   * every turn-transfer decision and every forced-endgame rung is recorded.
// Everything else is byte-identical to engine/src/game.hpp @ fe21e19.
#pragma once
#include "fish.hpp"
#include "belief.hpp"
#include "game.hpp"
#include "v04.hpp"

namespace fish {
namespace probecoord {

enum class PassPolicy : int { Unilateral = 0, Oracle = 1, Ladder = 2, LowSeat = 3, MostCards = 4 };

static constexpr int MAXRUNG = 12;

struct CoordStats {
  long long games = 0, events = 0;
  long long passEvents = 0;          // Pass events emitted
  long long passMulti = 0;           // pass decisions with >= 2 live candidates
  long long passDecisions = 0;       // pass decisions (n >= 1)
  long long gamesWithPass = 0, gamesWithMultiPass = 0;
  // consequence of a transfer: asks made by the receiving team before the turn
  // leaves the team, and hits among them
  long long postPassAsks = 0, postPassHits = 0, postPassRuns = 0;
  long long postPassZero = 0;        // transfers after which the receiver missed immediately

  // per-decision comparison at MULTI decisions (observational; recorded whatever
  // policy is actually in force)
  long long multiAnyLive = 0;        // >=1 candidate had a guaranteed hit available
  long long uniLive = 0, oraLive = 0, ladLive = 0, lowLive = 0, cardLive = 0;
  double uniSure = 0, oraSure = 0, ladSure = 0, lowSure = 0, cardSure = 0;
  long long uniAgree = 0, ladAgree = 0, lowAgree = 0, cardAgree = 0;   // agrees with oracle pick
  long long ladderNoWill = 0;        // no candidate cleared any rung
  long long multiDiffer = 0;         // candidates did NOT all have the same sureRun
  long long multiDecisive = 0;       // some candidate had sureRun 0 while another had > 0
  long long uniBestDiffer = 0, ladBestDiffer = 0;   // picked an argmax among the differing ones
  long long uniLossHist[16] = {0};   // oracle sureRun - chosen sureRun, clamped
  long long ladLossHist[16] = {0};

  // forced-endgame ladder
  long long forcedDecls = 0, forcedWrong = 0;
  long long rungFired[MAXRUNG + 1] = {0};      // index nForcedTh == "bestGuess" rung
  long long rungWrong[MAXRUNG + 1] = {0};
  double    rungConf[MAXRUNG + 1] = {0};
  long long forcedSweeps = 0;                  // forcedEndgame invocations
  long long forcedResidue = 0;                 // sets nobody would declare
  // At the moment the forced endgame opens: the confidence every declaring-team
  // player attaches to its OWN best allocation of every still-live half-suit.
  // This is exactly the quantity the willingness ladder thresholds.
  long long confHist[12] = {0};                // 0, (0,1e-6], .. by decade, then 0.1..1
  long long confN = 0; double confSum = 0, confMax = 0;
  long long confBestTrue = 0, confBestN = 0;   // was the highest-confidence (player,set) allocation right?
  // why is the confidence zero?  checked against the declarer's OWN Knowledge
  long long violOwner = 0, violMask = 0, violCap = 0, violNone = 0;

  // leakage: bucketed by the rung a candidate reports, over (decision, candidate,
  // active set) triples.  prior = the observing opponent's own posterior that the
  // candidate holds >=1 card of that set; truth = ground truth.
  long long leakN[MAXRUNG + 1] = {0};
  double    leakPrior[MAXRUNG + 1] = {0};
  long long leakTruth[MAXRUNG + 1] = {0};
  long long rungHist[MAXRUNG + 1] = {0};       // marginal distribution of reported rungs

  // score
  long long sets[2] = {0, 0};
  long long wins[2] = {0, 0};
  long long limitHits = 0;

  void merge(const CoordStats& o) {
    games += o.games; events += o.events;
    passEvents += o.passEvents; passMulti += o.passMulti; passDecisions += o.passDecisions;
    gamesWithPass += o.gamesWithPass; gamesWithMultiPass += o.gamesWithMultiPass;
    postPassAsks += o.postPassAsks; postPassHits += o.postPassHits; postPassRuns += o.postPassRuns;
    postPassZero += o.postPassZero;
    multiAnyLive += o.multiAnyLive;
    uniLive += o.uniLive; oraLive += o.oraLive; ladLive += o.ladLive; lowLive += o.lowLive; cardLive += o.cardLive;
    uniSure += o.uniSure; oraSure += o.oraSure; ladSure += o.ladSure; lowSure += o.lowSure; cardSure += o.cardSure;
    uniAgree += o.uniAgree; ladAgree += o.ladAgree; lowAgree += o.lowAgree; cardAgree += o.cardAgree;
    ladderNoWill += o.ladderNoWill;
    multiDiffer += o.multiDiffer; multiDecisive += o.multiDecisive;
    uniBestDiffer += o.uniBestDiffer; ladBestDiffer += o.ladBestDiffer;
    for (int i = 0; i < 16; i++) { uniLossHist[i] += o.uniLossHist[i]; ladLossHist[i] += o.ladLossHist[i]; }
    forcedDecls += o.forcedDecls; forcedWrong += o.forcedWrong;
    forcedSweeps += o.forcedSweeps; forcedResidue += o.forcedResidue;
    for (int i = 0; i < 12; i++) confHist[i] += o.confHist[i];
    confN += o.confN; confSum += o.confSum; confMax = std::max(confMax, o.confMax);
    confBestTrue += o.confBestTrue; confBestN += o.confBestN;
    violOwner += o.violOwner; violMask += o.violMask; violCap += o.violCap; violNone += o.violNone;
    for (int i = 0; i <= MAXRUNG; i++) {
      rungFired[i] += o.rungFired[i]; rungWrong[i] += o.rungWrong[i]; rungConf[i] += o.rungConf[i];
      leakN[i] += o.leakN[i]; leakPrior[i] += o.leakPrior[i]; leakTruth[i] += o.leakTruth[i];
      rungHist[i] += o.rungHist[i];
    }
    sets[0] += o.sets[0]; sets[1] += o.sets[1];
    wins[0] += o.wins[0]; wins[1] += o.wins[1];
    limitHits += o.limitHits;
  }
};

struct CoordCfg {
  PassPolicy policy = PassPolicy::Unilateral;
  int policyTeam = -1;              // -1 = both teams; else only this team uses `policy`
  int nRung = 8;
  double rung[MAXRUNG] = {0.98, 0.90, 0.80, 0.65, 0.50, 0.35, 0.20, 0.05};
  bool measure = false;             // record the per-decision comparison
  bool leak = false;                // record the leakage buckets
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
  int lockedAt[NSET];

  int rotation = 0;   // duplicate-block rotation: seat i receives hand (i+rotation)%6

  // ---- P8 probe additions -------------------------------------------------
  CoordCfg ccfg;
  CoordStats* cst = nullptr;
  int postPassTeam = -1;            // team currently inside a post-transfer run
  int postPassAsks = 0, postPassHits = 0;

  // Ground truth: how many cards the candidate could pull with CERTAINTY, one
  // after another.  A card is certainly pullable iff it sits with a live
  // opponent and the candidate already holds another card of that half-suit;
  // pulling never opens a new half-suit, so this count is exact (modulo an
  // opponent emptying mid-run).
  int sureRun(int u) const {
    int n = 0;
    for (int s = 0; s < NSET; s++) {
      if (!g.pub.setActive[s]) continue;
      if (!(g.hand[u] & setMask(s))) continue;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        if (g.hand[u] & bit(c)) continue;
        for (int t = 0; t < NPLAY; t++)
          if (teamOf(t) != teamOf(u) && g.pub.handCount[t] && (g.hand[t] & bit(c))) n++;
      }
    }
    return n;
  }

  // The candidate's OWN willingness statistic: the highest posterior hit
  // probability among the asks that are legal for it.  Only the resulting bit is
  // ever published.
  double willingness(int u) {
    auto* a = dynamic_cast<V04Agent*>(agents[u]);
    if (!a) return 0.0;
    a->refresh();
    return a->bestAskProbability(g.pub);
  }

  // Descending public thresholds; the first rung at which somebody is willing
  // decides.  Ties inside a rung go to the cardless player's own (unilateral)
  // preference, which is information-free.
  int ladderPick(const int* cand, int n, int uniPick, int* rungOut) {
    double w[3];
    for (int i = 0; i < n; i++) w[i] = willingness(cand[i]);
    for (int r = 0; r < ccfg.nRung; r++) {
      int pick = -1;
      for (int i = 0; i < n; i++) if (w[i] >= ccfg.rung[r]) {
        if (cand[i] == uniPick) { pick = cand[i]; break; }
        if (pick < 0) pick = cand[i];
      }
      if (pick >= 0) { if (rungOut) *rungOut = r; return pick; }
    }
    if (rungOut) *rungOut = ccfg.nRung;
    if (cst) cst->ladderNoWill++;
    return uniPick;
  }

  int rungOf(double w) const {
    for (int r = 0; r < ccfg.nRung; r++) if (w >= ccfg.rung[r]) return r;
    return ccfg.nRung;
  }

  int selectPassTarget(const int* cand, int n) {
    int uni = agents[g.turn]->choosePassTarget(g.pub, cand, n);
    bool valid = false; for (int i = 0; i < n; i++) if (cand[i] == uni) valid = true;
    if (!valid) uni = cand[0];
    // oracle
    int ora = uni; int bestSure = -1;
    for (int i = 0; i < n; i++) { int q = sureRun(cand[i]);
      if (q > bestSure || (q == bestSure && cand[i] == uni)) { bestSure = q; ora = cand[i]; } }
    int low = cand[0];
    int most = cand[0]; for (int i = 0; i < n; i++) if (g.pub.handCount[cand[i]] > g.pub.handCount[most]) most = cand[i];
    int lad = uni, ladRung = ccfg.nRung;
    bool needLadder = (ccfg.policy == PassPolicy::Ladder) || (ccfg.measure && n >= 2);
    if (needLadder) lad = ladderPick(cand, n, uni, &ladRung);

    if (cst) {
      cst->passDecisions++;
      if (n >= 2) {
        cst->passMulti++;
        if (bestSure > 0) cst->multiAnyLive++;
        auto rec = [&](int pick, long long& live, double& sure, long long& agree) {
          int q = sureRun(pick);
          if (q > 0) live++;
          sure += q;
          if (pick == ora) agree++;
        };
        long long dummy = 0;
        int mn = 99, mx = -1;
        for (int i = 0; i < n; i++) { int q = sureRun(cand[i]); mn = std::min(mn, q); mx = std::max(mx, q); }
        if (mn != mx) {
          cst->multiDiffer++;
          if (sureRun(uni) == mx) cst->uniBestDiffer++;
          if (sureRun(lad) == mx) cst->ladBestDiffer++;
        }
        if (mn == 0 && mx > 0) cst->multiDecisive++;
        cst->uniLossHist[std::min(15, mx - sureRun(uni))]++;
        cst->ladLossHist[std::min(15, mx - sureRun(lad))]++;
        rec(uni, cst->uniLive, cst->uniSure, cst->uniAgree);
        rec(ora, cst->oraLive, cst->oraSure, dummy);
        rec(lad, cst->ladLive, cst->ladSure, cst->ladAgree);
        rec(low, cst->lowLive, cst->lowSure, cst->lowAgree);
        rec(most, cst->cardLive, cst->cardSure, cst->cardAgree);
        if (ccfg.leak) collectLeak(cand, n);
      }
    }
    switch (ccfg.policy) {
      case PassPolicy::Oracle:    return (ccfg.policyTeam < 0 || teamOf(g.turn) == ccfg.policyTeam) ? ora : uni;
      case PassPolicy::Ladder:    return (ccfg.policyTeam < 0 || teamOf(g.turn) == ccfg.policyTeam) ? lad : uni;
      case PassPolicy::LowSeat:   return (ccfg.policyTeam < 0 || teamOf(g.turn) == ccfg.policyTeam) ? low : uni;
      case PassPolicy::MostCards: return (ccfg.policyTeam < 0 || teamOf(g.turn) == ccfg.policyTeam) ? most : uni;
      default:                    return uni;
    }
  }

  // How much does a full ladder sweep sharpen an OPPONENT's posterior?  For the
  // lowest-seat live opponent we take its own belief marginals as the prior that
  // candidate u holds >= 1 card of half-suit s, bucket by the rung u reports,
  // and compare with ground truth.
  void collectLeak(const int* cand, int n) {
    int obs = -1;
    for (int p = 0; p < NPLAY; p++)
      if (teamOf(p) != teamOf(g.turn) && g.pub.handCount[p]) { obs = p; break; }
    if (obs < 0) return;
    auto* oa = dynamic_cast<V04Agent*>(agents[obs]);
    if (!oa) return;
    oa->refresh();
    for (int i = 0; i < n; i++) {
      int u = cand[i];
      int r = rungOf(willingness(u));
      cst->rungHist[r]++;
      for (int s = 0; s < NSET; s++) {
        if (!g.pub.setActive[s]) continue;
        double none = 1;
        for (int j = 0; j < SETSZ; j++) none *= (1 - oa->bel.marg[cardOf(s, j)][u]);
        cst->leakN[r]++;
        cst->leakPrior[r] += 1 - none;
        if (g.hand[u] & setMask(s)) cst->leakTruth[r]++;
      }
    }
  }

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

  void applyDeclaration(int actor, const Declaration& d, bool forced, double conf) {
    int team = teamOf(actor);
    bool correct = true;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(d.set, i);
      int claimed = d.owner[i];
      if (teamOf(claimed) != team || !(g.hand[claimed] & bit(c))) { correct = false; break; }
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
    if (cst) cst->forcedSweeps++;
    if (cst) surveyForcedConfidence(declaringTeam);
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
            if (cst) {
              // check correctness before the declaration mutates the hands
              bool corr = true;
              for (int i = 0; i < SETSZ; i++) {
                int c = cardOf(s, i);
                if (teamOf(d.owner[i]) != declaringTeam || !(g.hand[d.owner[i]] & bit(c))) { corr = false; break; }
              }
              int ri = std::min(ti, MAXRUNG);
              cst->rungFired[ri]++; cst->rungConf[ri] += conf;
              cst->forcedDecls++;
              if (!corr) { cst->rungWrong[ri]++; cst->forcedWrong++; }
            }
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
      if (cst) cst->forcedResidue++;
      int cnt[2] = {0, 0};
      for (int i = 0; i < SETSZ; i++) { int c = cardOf(s, i);
        for (int p = 0; p < NPLAY; p++) if (g.hand[p] & bit(c)) cnt[teamOf(p)]++; }
      int aw = cnt[1] > cnt[0] ? 1 : 0;
      g.pub.score[aw]++; g.setWinner[s] = uint8_t(aw); g.pub.setActive[s] = false;
      for (int p = 0; p < NPLAY; p++) { g.hand[p] &= ~setMask(s); g.pub.handCount[p] = uint8_t(popcount64(g.hand[p])); }
    }
  }

  // Ask every declaring-team player, for every live half-suit, how confident it is
  // in its own best allocation.  Threshold 0 makes willingForced report rather
  // than gate, so this is the raw statistic the ladder is thresholding.
  void surveyForcedConfidence(int declaringTeam) {
    for (int s = 0; s < NSET; s++) {
      if (!g.pub.setActive[s]) continue;
      double bestC = -1; bool bestOk = false;
      for (int p = declaringTeam; p < NPLAY; p += 2) {
        Declaration d{}; double conf = -1;
        if (!agents[p]->willingForced(g.pub, s, d, conf, 0.0)) continue;
        d.set = uint8_t(s);
        cst->confN++; cst->confSum += conf; cst->confMax = std::max(cst->confMax, conf);
        int b;
        if (conf <= 0) b = 0;
        else if (conf < 1e-6) b = 1;
        else if (conf < 1e-4) b = 2;
        else if (conf < 1e-3) b = 3;
        else if (conf < 1e-2) b = 4;
        else if (conf < 0.05) b = 5;
        else if (conf < 0.10) b = 6;
        else if (conf < 0.25) b = 7;
        else if (conf < 0.50) b = 8;
        else if (conf < 0.80) b = 9;
        else if (conf < 0.95) b = 10;
        else b = 11;
        cst->confHist[b]++;
        {
          const Knowledge& kk = agents[p]->k;
          bool vo = false, vm = false;
          int extra[NPLAY] = {0,0,0,0,0,0};
          for (int i = 0; i < SETSZ; i++) {
            int c = cardOf(s, i), q = d.owner[i];
            if (kk.owner[c] < NPLAY) { if (kk.owner[c] != q) vo = true; }
            else if (kk.owner[c] == OUT_OF_PLAY) vo = true;
            else { if (!(kk.mask[c] & (1u << q))) vm = true; extra[q]++; }
          }
          uint8_t capq[NPLAY]; kk.capacities(capq);
          bool vc = false;
          for (int q = 0; q < NPLAY; q++) if (extra[q] > capq[q]) vc = true;
          if (vo) cst->violOwner++;
          if (vm) cst->violMask++;
          if (vc) cst->violCap++;
          if (!vo && !vm && !vc) cst->violNone++;
        }
        if (conf > bestC) {
          bestC = conf; bestOk = true;
          bool corr = true;
          for (int i = 0; i < SETSZ; i++) {
            int c = cardOf(s, i);
            if (teamOf(d.owner[i]) != declaringTeam || !(g.hand[d.owner[i]] & bit(c))) { corr = false; break; }
          }
          bestOk = corr;
        }
      }
      if (bestC >= 0) { cst->confBestN++; if (bestOk) cst->confBestTrue++; }
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
        int rcv = selectPassTarget(cand, n);
        bool valid = false; for (int i = 0; i < n; i++) if (cand[i] == rcv) valid = true;
        if (!valid) rcv = cand[0];
        if (cst) {
          cst->passEvents++;
          if (postPassTeam >= 0) { cst->postPassRuns++; cst->postPassAsks += postPassAsks;
                                   cst->postPassHits += postPassHits;
                                   if (!postPassAsks) cst->postPassZero++; }
          postPassTeam = teamOf(rcv); postPassAsks = 0; postPassHits = 0;
        }
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
      if (calib && team == calibTeam) {
        double f = agents[actor]->lastAskForecast();
        if (f >= 0) calib->ask.push_back({float(f), uint8_t(success ? 1 : 0)});
      }
      res.asks++; res.teamAsks[team]++; asks++;
      if (cst && postPassTeam == team) { postPassAsks++; if (success) postPassHits++; }
      if (cst && postPassTeam >= 0 && !success && teamOf(target) != postPassTeam) {
        cst->postPassRuns++; cst->postPassAsks += postPassAsks; cst->postPassHits += postPassHits;
        if (!postPassAsks) cst->postPassZero++;
        postPassTeam = -1; postPassAsks = 0; postPassHits = 0;
      }
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

} // namespace probecoord
} // namespace fish
