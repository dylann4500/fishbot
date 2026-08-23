// Adversarial verification of the "two-question cycle" deadlock claim.
// Standalone; does NOT include probe_deadlock.hpp — the replay and the run
// detection are written from scratch here so the numbers are independent.
//   clang++ -std=c++20 -O3 -march=native src/probe_vdeadlock_main.cpp -o vdead -pthread
#include "factory.hpp"
#include "game.hpp"
#include "belief.hpp"
#include <iostream>
#include <cstdio>
#include <map>
#include <set>
#include <algorithm>

using namespace fish;

static std::string argVal(int argc, char** argv, const char* key, const char* dflt) {
  std::string k = std::string("--") + key + "=";
  for (int i = 1; i < argc; i++) { std::string a = argv[i];
    if (a.rfind(k, 0) == 0) return a.substr(k.size()); }
  return dflt;
}

struct G {
  uint64_t seed = 0; int rot = 0; int events = 0;
  std::vector<Event> ev;
  uint64_t dealt[NPLAY] = {0,0,0,0,0,0};
  int cyc2len = 0, cyc2start = -1; int cyc2distinct = 0;
  uint32_t cycA = 0, cycB = 0;
  int deadRun = 0, deadStart = 0, deadDistinct = 0, nonAskInDead = 0;
  int lockedAtCyc[2] = {0,0}; int liveAtCyc = 0;
  int cycCardLockedToAsker = 0;   // of the 2 cycle cards, how many sit in a half-suit
                                  // ground-truth locked to the ASKER's own team
  int cycCardLockedEither = 0;    // ... locked to either team
  int cycOppositeParity = 0;      // the two cycle seats are opponents
  int declares = 0, forced = 0;
  int asks = 0, deadAsks = 0;
};

static uint32_t trip(const Event& e) {
  return (uint32_t(e.actor) << 16) | (uint32_t(e.card) << 8) | e.target;
}
static std::string tstr(uint32_t t) {
  char b[64]; snprintf(b, sizeof b, "s%u asks %s of s%u", (t >> 16) & 0xFF,
                       cardName((t >> 8) & 0xFF).c_str(), t & 0xFF);
  return b;
}

int main(int argc, char** argv) {
  std::string spec = argVal(argc, argv, "spec", "v04");
  int games = atoi(argVal(argc, argv, "games", "60").c_str());
  int rots  = atoi(argVal(argc, argv, "rotations", "2").c_str());
  uint64_t seed = strtoull(argVal(argc, argv, "seed", "31").c_str(), nullptr, 10);
  int minev = atoi(argVal(argc, argv, "minev", "300").c_str());
  int show  = atoi(argVal(argc, argv, "show", "6").c_str());
  Rules rules;

  std::vector<G> all;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < rots; rot++) {
      std::unique_ptr<Agent> ag[NPLAY]; Agent* ap[NPLAY];
      for (int p = 0; p < NPLAY; p++) { ag[p] = makeAgent(spec); ap[p] = ag[p].get(); }
      Game game; game.trace.on = true; game.rotation = rot;
      game.run(s, rules, ap);
      G g; g.seed = s; g.rot = rot; g.ev = game.trace.events; g.events = int(g.ev.size());
      for (int p = 0; p < NPLAY; p++) g.dealt[p] = game.g.dealt[p];
      for (auto& e : g.ev) { if (e.kind == Kind::Declare) g.declares++;
                             if (e.kind == Kind::ForcedDeclare) g.forced++;
                             if (e.kind == Kind::Ask) g.asks++; }

      // --- metric 1: longest STRICTLY ALTERNATING ask window, purely structural.
      // ev[a..b) all Asks, ev[k] == ev[k-2] for k >= a+2.  No belief involved.
      {
        int n = g.events;
        for (int a = 0; a < n; a++) {
          if (g.ev[a].kind != Kind::Ask) continue;
          int b = a + 1;
          while (b < n && g.ev[b].kind == Kind::Ask &&
                 (b - a < 2 || trip(g.ev[b]) == trip(g.ev[b - 2]))) b++;
          int len = b - a;
          if (len > g.cyc2len) {
            std::set<uint32_t> d; for (int k = a; k < b; k++) d.insert(trip(g.ev[k]));
            g.cyc2len = len; g.cyc2start = a; g.cyc2distinct = int(d.size());
            g.cycA = trip(g.ev[a]); g.cycB = (len > 1) ? trip(g.ev[a + 1]) : g.cycA;
          }
        }
      }

      // --- metric 2: independent reimplementation of "provably dead" runs
      {
        Knowledge k[NPLAY];
        for (int p = 0; p < NPLAY; p++) k[p].init(p, g.dealt[p], rules.deckSets);
        int run = 0, from = -1;
        for (size_t i2 = 0; i2 < g.ev.size(); i2++) {
          const Event& e = g.ev[i2];
          if (e.kind == Kind::Ask) {
            const Knowledge& kk = k[e.actor];
            bool d = (kk.owner[e.card] < NPLAY) ? (kk.owner[e.card] != e.target)
                                                : !(kk.mask[e.card] & (1u << e.target));
            if (d) { g.deadAsks++;
                     if (!run) from = int(i2); run++;
                     if (run > g.deadRun) { g.deadRun = run; g.deadStart = from; } }
            else run = 0;
          }
          for (int p = 0; p < NPLAY; p++) k[p].onEvent(e);
        }
        std::map<uint32_t,int> t;
        for (int j = g.deadStart; j < std::min(g.events, g.deadStart + g.deadRun); j++) {
          if (g.ev[j].kind != Kind::Ask) { g.nonAskInDead++; continue; }
          t[trip(g.ev[j])]++;
        }
        g.deadDistinct = int(t.size());
      }

      // --- ground-truth locks at the 2-cycle onset
      if (g.cyc2start >= 0) {
        uint64_t hand[NPLAY]; bool act[NSET];
        for (int p = 0; p < NPLAY; p++) hand[p] = g.dealt[p];
        for (int j = 0; j < NSET; j++) act[j] = (j < rules.deckSets);
        for (int j = 0; j < g.events; j++) {
          if (j == g.cyc2start) {
            uint64_t t0 = hand[0]|hand[2]|hand[4], t1 = hand[1]|hand[3]|hand[5];
            for (int st = 0; st < NSET; st++) { if (!act[st]) continue; g.liveAtCyc++;
              uint64_t m = setMask(st);
              if ((t0&m)==m) g.lockedAtCyc[0]++; else if ((t1&m)==m) g.lockedAtCyc[1]++; }
            uint32_t cc[2] = {g.cycA, g.cycB};
            for (int q = 0; q < 2; q++) {
              int card = (cc[q] >> 8) & 0xFF, asker = (cc[q] >> 16) & 0xFF;
              uint64_t m = setMask(setOf(card));
              int own = ((t0 & m) == m) ? 0 : (((t1 & m) == m) ? 1 : -1);
              if (own >= 0) { g.cycCardLockedEither++;
                              if (own == teamOf(asker)) g.cycCardLockedToAsker++; }
            }
            g.cycOppositeParity = (teamOf((g.cycA >> 16) & 0xFF) != teamOf((g.cycB >> 16) & 0xFF));
          }
          const Event& e = g.ev[j];
          if (e.kind == Kind::Ask && e.success) { hand[e.target] &= ~bit(e.card); hand[e.actor] |= bit(e.card); }
          if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
            for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
            act[e.set] = false;
          }
        }
      }
      all.push_back(std::move(g));
    }
  }

  // ---------------------------------------------------------------- report
  printf("VDEADLOCK independent check: %d games (%s mirror, seed %llu), minev=%d\n",
         int(all.size()), spec.c_str(), (unsigned long long)seed, minev);
  std::vector<int> lg;
  for (int i = 0; i < int(all.size()); i++) if (all[i].events > minev) lg.push_back(i);
  std::sort(lg.begin(), lg.end(), [&](int a, int b){ return all[a].deadRun > all[b].deadRun; });
  printf("games with > %d events: %d / %d\n\n", minev, int(lg.size()), int(all.size()));

  long long dr = 0, dd = 0, c2 = 0; int pure2 = 0, exact2 = 0, one = 0, anyLock = 0, alt2 = 0;
  int covers = 0;
  for (int i : lg) {
    const G& g = all[i];
    dr += g.deadRun; dd += g.deadDistinct; c2 += g.cyc2len;
    if (g.deadDistinct <= 2) pure2++;
    if (g.deadDistinct == 2) exact2++;
    if (g.deadDistinct == 1) one++;
    if (g.lockedAtCyc[0] + g.lockedAtCyc[1] > 0) anyLock++;
    if (g.cyc2len >= 10 && g.cyc2distinct == 2) alt2++;
    // does the structural alternating window cover the dead run?
    if (g.cyc2len >= g.deadRun - 2) covers++;
  }
  int n = int(lg.size());
  if (n) {
    printf("LONG-GAME CENSUS (independent):\n");
    printf("  mean longest provably-dead run                 %.4f asks\n", double(dr)/n);
    printf("  mean distinct (actor,card,target) in that run  %.4f\n", double(dd)/n);
    printf("  runs with <=2 distinct triples                 %d / %d\n", pure2, n);
    printf("  runs with EXACTLY 2 distinct triples           %d / %d\n", exact2, n);
    printf("  runs with exactly 1 distinct triple            %d / %d\n", one, n);
    printf("  mean longest STRICTLY ALTERNATING ask window   %.4f events\n", double(c2)/n);
    printf("  alternating window >=10 and exactly 2 triples  %d / %d\n", alt2, n);
    printf("  alternating window covers the dead run (+-2)   %d / %d\n", covers, n);
    printf("  any half-suit ground-truth locked at cycle onset %d / %d\n", anyLock, n);
    int lk = 0, lke = 0, op = 0;
    for (int i : lg) { lk += all[i].cycCardLockedToAsker; lke += all[i].cycCardLockedEither;
                       op += all[i].cycOppositeParity; }
    printf("  of the 2*%d cycle cards: in a half-suit locked to the ASKER's team %d ; locked to either team %d\n",
           n, lk, lke);
    printf("  cycle pairs whose two seats are OPPONENTS: %d / %d\n\n", op, n);
  }

  // whole-population view
  {
    long long ev = 0, asks = 0, dead = 0; int longest = 0, longestCyc = 0;
    int anyCyc10 = 0, anyDead6 = 0;
    for (auto& g : all) { ev += g.events; asks += g.asks; dead += g.deadAsks;
      longest = std::max(longest, g.deadRun); longestCyc = std::max(longestCyc, g.cyc2len);
      if (g.cyc2len >= 10) anyCyc10++; if (g.deadRun >= 6) anyDead6++; }
    printf("WHOLE POPULATION: events/game %.2f  dead asks %.2f%%  longest dead run %d  longest alternating window %d\n",
           double(ev)/all.size(), 100.0*double(dead)/double(asks), longest, longestCyc);
    printf("  games with a dead run >=6: %d / %d ; games with an alternating window >=10: %d / %d\n\n",
           anyDead6, int(all.size()), anyCyc10, int(all.size()));
  }

  int nd = std::min<int>(show, n);
  for (int i = 0; i < nd; i++) {
    const G& g = all[lg[i]];
    printf("trace %c  seed %llu rot %d  events %d  decl %d forced %d\n",
           'A'+i, (unsigned long long)g.seed, g.rot, g.events, g.declares, g.forced);
    printf("   dead run len %d at ev %d, %d distinct triples, non-ask events inside: %d\n",
           g.deadRun, g.deadStart, g.deadDistinct, g.nonAskInDead);
    std::map<uint32_t,int> t;
    for (int j = g.deadStart; j < std::min(g.events, g.deadStart+g.deadRun); j++)
      if (g.ev[j].kind == Kind::Ask) t[trip(g.ev[j])]++;
    for (auto& kv : t)
      printf("      %s  [set %d %s]  x%d\n", tstr(kv.first).c_str(),
             setOf((kv.first>>8)&0xFF), setName(setOf((kv.first>>8)&0xFF)), kv.second);
    printf("   strictly-alternating window len %d at ev %d (%d distinct)\n",
           g.cyc2len, g.cyc2start, g.cyc2distinct);
    printf("   ground truth at cycle onset: live %d  locked-t0 %d  locked-t1 %d\n",
           g.liveAtCyc, g.lockedAtCyc[0], g.lockedAtCyc[1]);
    // were the two cycle cards in the same half-suit?
    if (t.size() == 2) {
      auto it = t.begin(); int s1 = setOf((it->first>>8)&0xFF); ++it;
      int s2 = setOf((it->first>>8)&0xFF);
      printf("   the two questions are in %s half-suits (%d, %d)\n",
             s1 == s2 ? "the SAME" : "DIFFERENT", s1, s2);
    }
    printf("\n");
  }
  return 0;
}
