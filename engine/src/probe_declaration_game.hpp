// P6 scratch: a COPY of the Game driver from game.hpp with a wider set of
// declaration-arbitration rules.  game.hpp itself is left untouched.
//
// Differences from fish::Game (everything else is copied verbatim):
//   * lives in namespace fish::probe as PGame and reuses fish::Agent,
//     fish::GameResult, fish::Trace, fish::ValueSink, fish::CalibSink;
//   * declarationRound() supports per-team arbitration modes
//        0 lowest seat      (v0.4 default, Rules::declArbitration = 0)
//        1 highest seat
//        2 scan from the turn holder
//        3 highest stated confidence  -- INFORMATION-UNSAFE upper bound
//        4 public willingness ladder  -- information-safe: each seat reveals
//          only the bit [conf >= th] for a public threshold th, exactly the
//          channel Rules::forcedTh uses in the forced endgame
//   * the cross-team tiebreak (both teams want to cash something in the same
//     round) is a deterministic coin rather than "lowest seat", so that the
//     per-team mode under test cannot win by also winning cross-team races;
//   * race instrumentation: how often two seats of one team propose in the same
//     round at all, and how often confidence ranking would pick a different one;
//   * optional suppression of one (set) for a window of events, used to build
//     the counterfactual "what if this declaration had been delayed" corpus.
#pragma once
#include "game.hpp"

namespace fish {
namespace probe {

struct ArbStats {
  long long rounds = 0;          // declaration rounds that produced a declaration
  long long races = 0;           // rounds where >=2 seats of ONE team proposed
  long long racesDiffSet = 0;    // ... and they named different half-suits
  long long racesDiffConf = 0;   // ... and their confidences differ by >1e-9
  long long racesConfDiffers = 0;// ... and argmax-confidence != lowest seat
  long long declsSuppressed = 0;
  // Ground-truth adjudication of the contested races (arbitration actually
  // matters only where the confidence argmax is not the lowest seat).
  long long contested = 0;       // races where argmax-confidence != lowest seat
  long long lowRight = 0;        // ... the lowest seat's named allocation is true
  long long confRight = 0;       // ... the confidence argmax's is true
  long long ladRight = 0;        // ... the willingness ladder's pick is true
  long long bothRight = 0, bothWrong = 0, confOnly = 0, lowOnly = 0;
  // Recovery curve: correctness of the seat an R-rung public willingness ladder
  // would pick, on the contested races, for R = 2,3,5,9,17,33,65.
  static constexpr int NR = 7;
  long long ladRightR[NR] = {0,0,0,0,0,0,0};
  double confGapSum = 0;         // mean |conf(argmax) - conf(lowest)| on contested races
  void merge(const ArbStats& o) {
    rounds += o.rounds; races += o.races; racesDiffSet += o.racesDiffSet;
    racesDiffConf += o.racesDiffConf; racesConfDiffers += o.racesConfDiffers;
    declsSuppressed += o.declsSuppressed;
    contested += o.contested; lowRight += o.lowRight; confRight += o.confRight;
    ladRight += o.ladRight; bothRight += o.bothRight; bothWrong += o.bothWrong;
    confOnly += o.confOnly; lowOnly += o.lowOnly; confGapSum += o.confGapSum;
    for (int i = 0; i < NR; i++) ladRightR[i] += o.ladRightR[i];
  }
};

class PGame {
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

  int rotation = 0;

  // ---- P6 additions -------------------------------------------------------
  int arbTeam[2] = {0, 0};             // per-team arbitration mode (see header)
  bool teamOOT[2] = {true, true};      // per-team permission to declare out of turn
  double ootTh[2] = {0.0, 0.0};        // per-team confidence gate on out-of-turn declarations
  double declFloor[2] = {0.0, 0.0};    // per-team public confidence floor on ANY voluntary declaration
  int nLadder = 8;
  double ladder[128] = {0.995, 0.98, 0.95, 0.90, 0.80, 0.65, 0.50, 0.0};
  // Public willingness ladder with R evenly spaced rungs: seat p reveals only
  // the bits [conf_p >= th_r].  R = 2 degenerates to lowest-seat; large R
  // approaches a full confidence ranking.  ceil(log2(R)) bits per seat.
  void setRungs(int R) {
    R = std::max(2, std::min(128, R));
    nLadder = R;
    for (int i = 0; i < R; i++) ladder[i] = 1.0 - double(i) / double(R - 1);
  }
  ArbStats arb;
  int blockSet = -1;                   // suppress voluntary declarations of this set
  int blockFromEvent = 0;              // ... while blockFrom <= pub.nEvents
  int blockUntilEvent = -1;            // ... and pub.nEvents < blockUntil
  // -------------------------------------------------------------------------

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
    if (observer) observer(*this);
  }

  std::function<void(const PGame&)> observer;

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

  // Ground truth: would this named allocation score?  Diagnostic only -- never
  // read by any policy, only by the race instrumentation.
  bool declTrue(int actor, const Declaration& d) const {
    int team = teamOf(actor);
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(d.set, i), o = d.owner[i];
      if (teamOf(o) != team || !(g.hand[o] & bit(c))) return false;
    }
    return true;
  }

  // Lowest seat clearing the highest rung of an R-rung evenly spaced public
  // ladder.  Each seat reveals only the bits [conf >= th_r].
  int pickLadderR(const bool* want, const double* conf, int team, int R) const {
    for (int r = 0; r < R; r++) {
      double th = 1.0 - double(r) / double(R - 1);
      for (int p = team; p < NPLAY; p += 2) if (want[p] && conf[p] >= th) return p;
    }
    return -1;
  }

  // Pick this team's declarer from the seats that want to declare.
  int pickWithin(int mode, const bool* want, const double* conf, int team) const {
    int order[3], n = 0;
    if (mode == 1)      { for (int p = 4 + team; p >= 0; p -= 2) order[n++] = p; }
    else if (mode == 2) { for (int o = 0; o < NPLAY; o++) { int p = (g.turn + o) % NPLAY;
                            if (teamOf(p) == team) order[n++] = p; } }
    else                { for (int p = team; p < NPLAY; p += 2) order[n++] = p; }
    if (mode == 3) {
      int best = -1; double bc = -1e18;
      for (int i = 0; i < n; i++) { int p = order[i];
        if (want[p] && conf[p] > bc + 1e-12) { bc = conf[p]; best = p; } }
      return best;
    }
    if (mode == 4) {
      for (int L = 0; L < nLadder; L++) {
        for (int i = 0; i < n; i++) { int p = order[i];
          if (want[p] && conf[p] >= ladder[L]) return p; }
      }
      return -1;
    }
    for (int i = 0; i < n; i++) if (want[order[i]]) return order[i];
    return -1;
  }

  bool declarationRound() {
    bool any = false;
    for (int round = 0; round < NSET + 2; round++) {
      bool want[NPLAY] = {false,false,false,false,false,false};
      double conf[NPLAY] = {0,0,0,0,0,0};
      Declaration decl[NPLAY];
      for (int p = 0; p < NPLAY; p++) {
        if ((!rules.outOfTurnDeclare || !teamOOT[teamOf(p)]) && p != g.turn) continue;
        if (!rules.cardlessMayDeclare && !g.pub.handCount[p]) continue;
        Declaration d{}; double c = 0;
        if (!agents[p]->proposeDeclaration(g.pub, d, c)) continue;
        if (!g.pub.setActive[d.set]) continue;
        bool ok = true;
        for (int i = 0; i < SETSZ; i++) if (teamOf(d.owner[i]) != teamOf(p)) ok = false;
        if (!ok) continue;
        if (c < declFloor[teamOf(p)]) continue;
        if (p != g.turn && c < ootTh[teamOf(p)]) continue;
        if (blockSet >= 0 && int(d.set) == blockSet
            && g.pub.nEvents >= blockFromEvent && g.pub.nEvents < blockUntilEvent) {
          arb.declsSuppressed++;
          continue;
        }
        want[p] = true; conf[p] = c; decl[p] = d;
      }
      int win[2] = {-1, -1};
      for (int t = 0; t < 2; t++) {
        int nw = 0, lowest = -1, cbest = -1; double bc = -1e18;
        bool diffSet = false, diffConf = false;
        for (int p = t; p < NPLAY; p += 2) if (want[p]) {
          nw++;
          if (lowest < 0) lowest = p; else if (decl[p].set != decl[lowest].set) diffSet = true;
          if (conf[p] > bc + 1e-12) { bc = conf[p]; cbest = p; }
        }
        if (nw >= 2) {
          arb.races++;
          if (diffSet) arb.racesDiffSet++;
          double mn = 1e18, mx = -1e18;
          for (int p = t; p < NPLAY; p += 2) if (want[p]) { mn = std::min(mn, conf[p]); mx = std::max(mx, conf[p]); }
          diffConf = (mx - mn) > 1e-9;
          if (diffConf) arb.racesDiffConf++;
          if (cbest != lowest) {
            arb.racesConfDiffers++;
            arb.contested++;
            arb.confGapSum += conf[cbest] - conf[lowest];
            bool lr = declTrue(lowest, decl[lowest]);
            bool cr = declTrue(cbest, decl[cbest]);
            int lad = pickWithin(4, want, conf, t);
            static const int RS[ArbStats::NR] = {2, 3, 5, 9, 17, 33, 65};
            for (int ri = 0; ri < ArbStats::NR; ri++) {
              int pk = pickLadderR(want, conf, t, RS[ri]);
              if (pk >= 0 && declTrue(pk, decl[pk])) arb.ladRightR[ri]++;
            }
            if (lr) arb.lowRight++;
            if (cr) arb.confRight++;
            if (lad >= 0 && declTrue(lad, decl[lad])) arb.ladRight++;
            if (lr && cr) arb.bothRight++;
            else if (!lr && !cr) arb.bothWrong++;
            else if (cr) arb.confOnly++;
            else arb.lowOnly++;
          }
        }
        win[t] = pickWithin(arbTeam[t], want, conf, t);
      }
      int chosen = -1;
      if (win[0] >= 0 && win[1] >= 0) {
        // Neutral cross-team coin: the per-team mode under test must not be
        // able to win cross-team races as a side effect.
        chosen = (mixSeed(seed, uint64_t(g.pub.nEvents) * 1315423911ull) & 1) ? win[1] : win[0];
      } else chosen = win[0] >= 0 ? win[0] : win[1];
      if (chosen < 0) break;
      arb.rounds++;
      applyDeclaration(chosen, decl[chosen], false, conf[chosen]);
      any = true;
      if (!g.pub.activeSets()) break;
    }
    return any;
  }

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
      AskMove mv = agents[g.turn]->chooseAsk(g.pub);
      if (!legalAsk(g, g.turn, mv.card, mv.target)) {
        AskMove buf[NSET * SETSZ * 3]; int n = enumerateAsks(g.pub, g.hand[g.turn], g.turn, buf);
        if (!n) {
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
      if (asks >= rules.maxAsks) { res.hitLimit = true; adjudicateRemaining(); break; }
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

} // namespace probe
} // namespace fish
