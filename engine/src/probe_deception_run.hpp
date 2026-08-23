// P3 -- measurement harness for the deception archetypes (see probe_deception.hpp).
// Separate from probe_deception.hpp because factory.hpp includes the agents and
// this file includes factory.hpp.
#pragma once
#include "factory.hpp"
#include "arena.hpp"
#include "probe_deception.hpp"
#include <thread>
#include <iostream>

namespace fish {

// ------------------------------------------------------------ measurement
//
// Team 0 (seats 0,2,4) is always plain v0.4 and is the MEASURED team.  Team 1
// (seats 1,3,5) is the opponent: `dseatMask` picks which of its seats run the
// archetype; the rest run the control spec.  Belief error is read straight out
// of the measured agents' own posteriors at every public event and scored
// against the true hands.
struct DeceitStats {
  long long games = 0, events = 0, asks = 0, hits = 0;
  long long setsA = 0, setsB = 0, winsA = 0;
  // belief error over (unresolved card, seat) pairs
  long long pairs = 0;  double sumAbs = 0, maxAbs = 0;
  long long cards = 0;  double sumTruthMass = 0;  long long confWrong = 0;  // marg[c][truth] < 0.10
  // mass placed on the deceptive seats
  long long nDecTrue = 0;  double sumDecMassTrue = 0;   // cards the deceivers really hold
  long long nDecFalse = 0; double sumDecMassFalse = 0;  // cards they do not hold
  // channel-resolved: the two soft-prior channels of Knowledge::priorWeight
  long long nSilentTrue = 0;  double sumSilentTrue = 0;   // marked seat holds c, has NEVER asked in its half-suit
  long long nAskedTrue = 0;   double sumAskedTrue = 0;    // marked seat holds c, HAS asked in its half-suit
  long long nSilentFalse = 0; double sumSilentFalse = 0;  // marked seat lacks c, never asked there
  long long nAskedFalse = 0;  double sumAskedFalse = 0;   // marked seat lacks c, has asked there (certificate)
  // measured team's declarations
  long long decls = 0, wrong = 0;
  long long wrongCards = 0, wcDec = 0, wcOtherOpp = 0, wcMate = 0;
  long long wrongWithDec = 0;   // wrong declarations containing >=1 card truly at a deceptive seat
  std::vector<uint8_t> dealWins; // per deal: measured-team wins (cluster bootstrap)
  // Empirical calibration of the two soft-prior statistics.  For every
  // (observer on the measured team, opponent seat p, active half-suit S,
  // card c of S still unresolved for that observer, p still possible), bucket
  // by askCount[p][S] and by asks-elsewhere, and record whether p really holds
  // c.  This is exactly the quantity priorWeight (belief.hpp:100) reweights.
  long long priN[4][4] = {{0}}, priT[4][4] = {{0}};      // marked (deceptive) seats
  double priM[4][4] = {{0}};                             // summed model marginal, same cells
  long long priNc[4][4] = {{0}}, priTc[4][4] = {{0}};    // unmarked opponent seats

  void merge(const DeceitStats& o) {
    games += o.games; events += o.events; asks += o.asks; hits += o.hits;
    setsA += o.setsA; setsB += o.setsB; winsA += o.winsA;
    pairs += o.pairs; sumAbs += o.sumAbs; maxAbs = std::max(maxAbs, o.maxAbs);
    cards += o.cards; sumTruthMass += o.sumTruthMass; confWrong += o.confWrong;
    nDecTrue += o.nDecTrue; sumDecMassTrue += o.sumDecMassTrue;
    nDecFalse += o.nDecFalse; sumDecMassFalse += o.sumDecMassFalse;
    nSilentTrue += o.nSilentTrue; sumSilentTrue += o.sumSilentTrue;
    nAskedTrue += o.nAskedTrue; sumAskedTrue += o.sumAskedTrue;
    nSilentFalse += o.nSilentFalse; sumSilentFalse += o.sumSilentFalse;
    nAskedFalse += o.nAskedFalse; sumAskedFalse += o.sumAskedFalse;
    decls += o.decls; wrong += o.wrong;
    wrongCards += o.wrongCards; wcDec += o.wcDec; wcOtherOpp += o.wcOtherOpp; wcMate += o.wcMate;
    wrongWithDec += o.wrongWithDec;
    dealWins.insert(dealWins.end(), o.dealWins.begin(), o.dealWins.end());
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
      priN[i][j] += o.priN[i][j]; priT[i][j] += o.priT[i][j]; priM[i][j] += o.priM[i][j];
      priNc[i][j] += o.priNc[i][j]; priTc[i][j] += o.priTc[i][j];
    }
  }
};

struct DeceitConfig {
  std::string measured = "v04";     // team 0
  std::string deceptive = "silent"; // archetype at the marked seats
  std::string control = "v04";      // the other opponent seats
  int dseatMask = 0x2;              // bitmask over seats; default seat 1 only
  int games = 200;
  uint64_t seed = 4242;
  Rules rules;
  int threads = 0;
  int stride = 1;                   // sample belief error every `stride` events
};

// Declaration attribution, replayed against the true deal.
inline void attributeDeclarations(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                                  const Rules& rules, int dseatMask, DeceitStats& st) {
  uint64_t hand[NPLAY];
  for (int p = 0; p < NPLAY; p++) hand[p] = dealt[p];
  for (const Event& e : ev) {
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      if (teamOf(e.actor) == 0) {
        st.decls++;
        if (!e.success) {
          st.wrong++;
          bool anyDec = false;
          for (int i = 0; i < SETSZ; i++) {
            int c = cardOf(e.set, i);
            int truth = -1;
            for (int q = 0; q < NPLAY; q++) if (hand[q] & bit(c)) truth = q;
            if (truth < 0) continue;
            if (truth == e.decl.owner[i]) continue;
            st.wrongCards++;
            if (dseatMask & (1 << truth)) { st.wcDec++; anyDec = true; }
            else if (teamOf(truth) != 0) st.wcOtherOpp++;
            else st.wcMate++;
          }
          if (anyDec) st.wrongWithDec++;
        }
      }
      for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
    } else if (e.kind == Kind::Ask && e.success) {
      hand[e.target] &= ~bit(e.card);
      hand[e.actor] |= bit(e.card);
    }
  }
  (void)rules;
}

inline DeceitStats runDeceit(const DeceitConfig& dc) {
  int nThreads = dc.threads > 0 ? dc.threads : int(std::thread::hardware_concurrency());
  if (nThreads < 1) nThreads = 1;
  nThreads = std::min(nThreads, std::max(1, dc.games));
  std::vector<DeceitStats> local(nThreads);
  std::vector<std::thread> pool;
  for (int t = 0; t < nThreads; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) {
        A[i] = makeAgent(dc.measured);
        int seatB = 2 * i + 1;
        B[i] = makeAgent((dc.dseatMask & (1 << seatB)) ? dc.deceptive : dc.control);
      }
      DeceitStats& st = local[t];
      Game game;
      game.trace.on = true;
      for (int i = t; i < dc.games; i += nThreads) {
        uint64_t s = mixSeed(dc.seed, uint64_t(i) * 2654435761ull + 1);
        int wins = 0;
        for (int rot = 0; rot < 2; rot++) {
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == 0) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = rot;      // vary which hand-triple the measured team gets
          game.trace.events.clear();
          long long evc = 0;
          game.observer = [&](const Game& G) {
            if (dc.stride > 1 && (evc++ % dc.stride)) return;
            for (int m = 0; m < NPLAY; m += 2) {
              V04Agent* a = static_cast<V04Agent*>(G.agents[m]);
              a->refresh();
              for (int c = 0; c < NCARD; c++) {
                if (!G.g.pub.setActive[setOf(c)]) continue;
                if (a->k.owner[c] != UNKNOWN) continue;      // already resolved: error is 0 by soundness
                int truth = -1;
                for (int q = 0; q < NPLAY; q++) if (G.g.hand[q] & bit(c)) truth = q;
                if (truth < 0) continue;
                double dmass = 0;
                for (int p = 0; p < NPLAY; p++) {
                  double err = std::fabs(a->bel.marg[c][p] - (p == truth ? 1.0 : 0.0));
                  st.sumAbs += err; st.pairs++;
                  if (err > st.maxAbs) st.maxAbs = err;
                  if (dc.dseatMask & (1 << p)) dmass += a->bel.marg[c][p];
                }
                st.cards++;
                st.sumTruthMass += a->bel.marg[c][truth];
                if (a->bel.marg[c][truth] < 0.10) st.confWrong++;
                if (dc.dseatMask & (1 << truth)) { st.nDecTrue++; st.sumDecMassTrue += dmass; }
                else { st.nDecFalse++; st.sumDecMassFalse += dmass; }
                // Channel decomposition.  priorWeight (belief.hpp:100) reweights
                // by askCount[p][S] (theta) and by turns-without-asking-here
                // (phi), so score the marginal separately in the four cells of
                // {holder / non-holder} x {has asked in S / has never asked}.
                int S = setOf(c);
                for (int p = 0; p < NPLAY; p++) {
                  if (teamOf(p) == teamOf(m)) continue;
                  if (!(a->k.mask[c] & (1u << p))) continue;
                  int aa = a->k.askCount[p][S];
                  int ob = int(a->k.totalAsks[p]) - aa;
                  int ai = aa >= 3 ? 3 : aa;
                  int oj = ob <= 3 ? 0 : (ob <= 7 ? 1 : (ob <= 11 ? 2 : 3));
                  bool marked = (dc.dseatMask & (1 << p)) != 0;
                  if (marked) { st.priN[ai][oj]++; st.priM[ai][oj] += a->bel.marg[c][p]; if (p == truth) st.priT[ai][oj]++; }
                  else        { st.priNc[ai][oj]++; if (p == truth) st.priTc[ai][oj]++; }
                }
                for (int p = 0; p < NPLAY; p++) {
                  if (!(dc.dseatMask & (1 << p))) continue;
                  if (!(a->k.mask[c] & (1u << p))) continue;   // hard-excluded: no prior effect
                  bool asked = a->k.askCount[p][S] > 0;
                  bool holds = (p == truth);
                  double mm = a->bel.marg[c][p];
                  if (holds && asked)   { st.nAskedTrue++;   st.sumAskedTrue += mm; }
                  if (holds && !asked)  { st.nSilentTrue++;  st.sumSilentTrue += mm; }
                  if (!holds && asked)  { st.nAskedFalse++;  st.sumAskedFalse += mm; }
                  if (!holds && !asked) { st.nSilentFalse++; st.sumSilentFalse += mm; }
                }
              }
            }
          };
          GameResult r = game.run(s, dc.rules, ag);
          game.observer = nullptr;
          st.games++; st.events += r.events; st.asks += r.teamAsks[0]; st.hits += r.teamHits[0];
          st.setsA += r.score[0]; st.setsB += r.score[1];
          if (r.winner == 0) { st.winsA++; wins++; }
          attributeDeclarations(game.trace.events, game.g.dealt, dc.rules, dc.dseatMask, st);
        }
        st.dealWins.push_back(uint8_t(wins));
      }
    });
  }
  for (auto& th : pool) th.join();
  DeceitStats total;
  for (int t = 0; t < nThreads; t++) total.merge(local[t]);
  return total;
}

inline void printDeceit(const DeceitStats& s, std::ostream& os) {
  auto pct = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  double m, lo, hi;
  std::vector<uint8_t> w = s.dealWins;
  clusterBootstrap(w, 2, m, lo, hi);
  os << "games                 " << s.games << "   measured-team win rate " << 100 * m
     << "%  [" << 100 * lo << ", " << 100 * hi << "]\n";
  os << "events/game           " << (s.games ? double(s.events) / s.games : 0)
     << "   measured ask hit rate " << pct(s.hits, s.asks) << "%\n";
  os << "mean |marg - truth|   " << (s.pairs ? s.sumAbs / s.pairs : 0)
     << "   over " << s.pairs << " (unresolved card, seat) pairs;  max " << s.maxAbs << "\n";
  os << "mean P(true holder)   " << (s.cards ? s.sumTruthMass / s.cards : 0)
     << "   confidently wrong (<0.10 on truth) " << pct(s.confWrong, s.cards) << "%\n";
  os << "mass on marked seats  held-by-them " << (s.nDecTrue ? s.sumDecMassTrue / s.nDecTrue : 0)
     << "   not-held " << (s.nDecFalse ? s.sumDecMassFalse / s.nDecFalse : 0) << "\n";
  os << "channel  holder&silent  " << (s.nSilentTrue ? s.sumSilentTrue / s.nSilentTrue : 0)
     << " (n=" << s.nSilentTrue << ")   holder&asked " << (s.nAskedTrue ? s.sumAskedTrue / s.nAskedTrue : 0)
     << " (n=" << s.nAskedTrue << ")\n";
  os << "channel  nonhold&silent " << (s.nSilentFalse ? s.sumSilentFalse / s.nSilentFalse : 0)
     << " (n=" << s.nSilentFalse << ")   nonhold&asked " << (s.nAskedFalse ? s.sumAskedFalse / s.nAskedFalse : 0)
     << " (n=" << s.nAskedFalse << ")\n";
  os << "measured declarations " << s.decls << "   wrong " << s.wrong << " (" << pct(s.wrong, s.decls) << "%)\n";
  os << "  mis-assigned cards  " << s.wrongCards
     << "   truly at a MARKED seat " << s.wcDec << " (" << pct(s.wcDec, s.wrongCards) << "%)"
     << "   other opponent " << s.wcOtherOpp << " (" << pct(s.wcOtherOpp, s.wrongCards) << "%)"
     << "   own teammate " << s.wcMate << " (" << pct(s.wcMate, s.wrongCards) << "%)\n";
  os << "  wrong decls touching a marked seat " << s.wrongWithDec
     << " (" << pct(s.wrongWithDec, s.wrong) << "% of wrong)\n";
  {
    const char* lbl[2] = {"marked  ", "unmarked"};
    for (int t = 0; t < 2; t++) {
      const long long (*N)[4] = t ? s.priNc : s.priN;
      const long long (*T)[4] = t ? s.priTc : s.priT;
      os << "truth rate / v0.4 mean marginal, by askCount[p][S]=a and asks elsewhere=b   seats: " << lbl[t] << "\n";
      os << "        b:0-3      4-7      8-11     12+\n";
      for (int i = 0; i < 4; i++) {
        os << "  a=" << (i == 3 ? "3+" : std::to_string(i).c_str()) << "  ";
        for (int j = 0; j < 4; j++) {
          double v = N[i][j] ? double(T[i][j]) / double(N[i][j]) : 0;
          double mm = N[i][j] ? (t ? 0.0 : s.priM[i][j] / double(N[i][j])) : 0;
          char b[80]; snprintf(b, sizeof(b), "%.4f/%.4f(%lldk) ", v, mm, N[i][j] / 1000);
          os << b;
        }
        os << "\n";
      }
    }
  }
  const auto& C = decCost();
  long long na = C.asks.load(), nd = C.deviations.load();
  os << "archetype asks        " << na << "   deviations " << nd << " (" << pct(nd, na) << "%)"
     << "   forced " << C.forced.load() << "\n";
  os << "  P(hit) on deviations  v04 pick " << (nd ? C.pHonestMilli.load() / (1000.0 * nd) : 0)
     << " -> archetype pick " << (nd ? C.pChosenMilli.load() / (1000.0 * nd) : 0) << "\n";
  os << "  P(hit) over all asks  v04 pick " << (na ? C.pHonestAllMilli.load() / (1000.0 * na) : 0)
     << " -> archetype pick " << (na ? C.pChosenAllMilli.load() / (1000.0 * na) : 0) << "\n";
}

} // namespace fish
