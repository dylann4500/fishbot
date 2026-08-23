// P4-verify: independent check of the press>=2 forcing-horizon branch.
// Uses the SHIPPED V04Agent (src/v04.hpp) unmodified.
#pragma once
#include "v04.hpp"
#include "game.hpp"
#include <cstdio>

namespace fish::vpol {

// Deal a real game, hand seat 0's cards to a fresh shipped V04Agent, and ask it
// to declare from a synthetic public state that differs ONLY in pub.nEvents.
inline void runHorizonUnit(uint64_t seed, int nDeals) {
  Rules r;
  printf("forceDeclareEvents=%d  press1 at nEvents>=%d  press2 at nEvents>=%d\n",
         V04Config().forceDeclareEvents, V04Config().forceDeclareEvents,
         (7 * V04Config().forceDeclareEvents) / 5);
  printf("%-6s %-28s %-28s %-28s\n", "deal", "nEvents=219 (press 0)", "nEvents=307 (press 1)",
         "nEvents=308 (press 2)");
  int declaredAt308 = 0, declaredAt307 = 0, declaredAt219 = 0;
  int wrongAt308 = 0;
  double worstAlloc = 1e9;
  for (int d = 0; d < nDeals; d++) {
    GameState g{};
    dealCards(g, mixSeed(seed, uint64_t(d)), r.deckSets);
    char line[512]; int off = 0;
    off += snprintf(line + off, sizeof(line) - off, "%-6d", d);
    bool bad = false; double badAlloc = 0; int badSet = -1;
    for (int ev : {219, 307, 308}) {
      V04Agent a;                                   // shipped policy, default config
      a.reset(0, g.hand[0], r, mixSeed(seed, 77));
      PublicState pub{};
      pub.rules = r;
      for (int p = 0; p < NPLAY; p++) pub.handCount[p] = uint8_t(popcount64(g.hand[p]));
      for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < r.deckSets);
      pub.score[0] = pub.score[1] = 0;
      pub.nEvents = ev; pub.nAsks = 0; pub.turn = 0;
      Declaration dec{}; double conf = -1;
      bool got = a.proposeDeclaration(pub, dec, conf);
      if (got) {
        if (ev == 219) declaredAt219++;
        if (ev == 307) declaredAt307++;
        if (ev == 308) {
          declaredAt308++;
          // ground truth: is the named allocation right?
          bool correct = true;
          for (int i = 0; i < SETSZ; i++) {
            int c = cardOf(dec.set, i);
            if (teamOf(dec.owner[i]) != 0 || !(g.hand[dec.owner[i]] & bit(c))) { correct = false; break; }
          }
          if (!correct) wrongAt308++;
          bad = !correct; badAlloc = conf; badSet = dec.set;
          if (conf < worstAlloc) worstAlloc = conf;
        }
        off += snprintf(line + off, sizeof(line) - off, " DECLARE set=%d pAlloc=%.3e ",
                        int(dec.set), conf);
      } else {
        off += snprintf(line + off, sizeof(line) - off, " %-27s", "no declaration");
      }
    }
    if (d < 12) printf("%s%s\n", line, bad ? "  <- WRONG" : "");
    (void)badAlloc; (void)badSet;
  }
  printf("\nover %d fresh deals (zero information beyond the agent's own nine cards):\n", nDeals);
  printf("  declares at nEvents=219 (press 0): %d\n", declaredAt219);
  printf("  declares at nEvents=307 (press 1): %d\n", declaredAt307);
  printf("  declares at nEvents=308 (press 2): %d   of which WRONG: %d (%.2f%%)\n",
         declaredAt308, wrongAt308, 100.0 * wrongAt308 / std::max(1, declaredAt308));
  printf("  smallest stated pAlloc it was willing to cash at press 2: %.3e\n", worstAlloc);
}

// Same three states, but reporting which of the three press>=2 relaxations is
// load-bearing, by re-running with the gates individually restored via cfg.
inline void runGateAttribution(uint64_t seed, int nDeals) {
  Rules r;
  struct Row { const char* what; int declares; };
  // cfg knobs cannot restore the press>=2 gates (they are hard-coded ternaries),
  // so instead measure what each gate WOULD have rejected, using evaluateSet at
  // press 0 with the same knowledge.
  long long n = 0, rejMarg = 0, rejFloor = 0, rejAlloc = 0;
  for (int d = 0; d < nDeals; d++) {
    GameState g{};
    dealCards(g, mixSeed(seed, uint64_t(d)), r.deckSets);
    V04Agent a;
    a.reset(0, g.hand[0], r, mixSeed(seed, 77));
    PublicState pub{};
    pub.rules = r;
    for (int p = 0; p < NPLAY; p++) pub.handCount[p] = uint8_t(popcount64(g.hand[p]));
    for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < r.deckSets);
    pub.nEvents = 308; pub.turn = 0;
    Declaration dec{}; double conf = -1;
    if (!a.proposeDeclaration(pub, dec, conf)) continue;
    n++;
    a.refresh();
    V04Agent::SetVerdict v0 = a.evaluateSet(pub, dec.set, 0);   // gates ON
    if (!v0.ok) {
      // find which gate killed it
      V04Agent::SetVerdict vi = a.evaluateSet(pub, dec.set, 0, true);  // ignoreGates
      if (vi.ok && vi.pTeam < a.cfg.minTeamProb) rejFloor++;
      else rejMarg++;
    } else if (conf < a.cfg.declThreshold) rejAlloc++;
  }
  printf("\npress>=2 declarations that the press-0 gates would have blocked (n=%lld):\n", n);
  printf("  blocked by marginalGate (%.3f):  %lld\n", V04Config().marginalGate, rejMarg);
  printf("  blocked by minTeamProb  (%.4f): %lld\n", V04Config().minTeamProb, rejFloor);
  printf("  passed both gates but below declThreshold (%.3f): %lld\n",
         V04Config().declThreshold, rejAlloc);
  (void)0;
}

}  // namespace fish::vpol
