// Soundness and cost test for M4 (per-seat knowledge models) and M5
// (target-dimension selection).  Standalone translation unit: it does not touch
// any protected header and it does not need `fish` to be rebuilt.
//
//   c++ -std=c++20 -O3 -march=native -Isrc src/probe_m45_test.cpp -o m45test -pthread
//   ./m45test --games=40 --seed=31 --a=v05 --b=v05
//
// WHAT IT CHECKS
//
//   A  SOUNDNESS.  For every observer i, every seat j and every card c of a live
//      half-suit, observer i's model of seat j must never exclude the card's
//      TRUE holder.  This is the audit idea of Game::runAudit (game.hpp:120-146)
//      applied to the derived models instead of the agents' own knowledge.
//      A single violation invalidates M5a: the lockout term would then be
//      asserting an opponent knows something false.
//   B  CAPACITY CONSISTENCY.  Each model's spare capacity vector must equal the
//      true number of that model's unresolved cards held by each seat.
//   C  DISJUNCTIONS.  Every C5 certificate carried by a model must be satisfied
//      by the ground truth.
//   D  COMMON KNOWLEDGE.  Six public models seeded with me = 0..5 and fed the
//      same event stream must agree bit for bit in owner/mask/unresolved/disj/
//      handCount/askCount.  This is the claim that makes M4 cost 1x rather than
//      6x, and it is checked rather than argued.
//   E  NO FABRICATED KNOWLEDGE.  Observer i's model of ITSELF must be no
//      stronger than its own Knowledge: k_i.mask[c] must be a subset of
//      model(i).mask[c] for every card.  If the model were ever stronger, the
//      "strict under-approximation" claim in v05_target.hpp would be false.
//   F  CONTRADICTIONS.  SeatModels::contradictions must stay at 0.
//
// WHAT IT MEASURES
//
//   * the cost of M4, in nanoseconds per public event and per lazy per-seat
//     build, against the cost of a real agent's own Knowledge::onEvent -- a
//     shadow copy of the live agent's `k`, resynced every event, so the control
//     carries constraint C1 the way the agent's own knowledge does;
//   * how often the hard blackball certificate fires -- decisions at which we
//     can PROVE some opponent knows where one of our cards is;
//   * whether M5 actually resolves the measured target channel: among decisions
//     offering two or more hard-indistinguishable targets for the best card, the
//     fraction at which the M5 score strictly separates them.
#include "fish.hpp"
#include "belief.hpp"
#include "game.hpp"
#include "factory.hpp"
#include "v05_target.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

using namespace fish;

static int trueHolder(const GameState& g, int c) {
  for (int p = 0; p < NPLAY; p++) if (g.hand[p] & bit(c)) return p;
  return -1;
}

struct Stats {
  long long checksA = 0, violA = 0;
  long long checksB = 0, violB = 0;
  long long checksC = 0, violC = 0;
  long long checksD = 0, violD = 0;
  long long checksE = 0, violE = 0;
  long long contradictions = 0;

  long long events = 0, ownEvents = 0;
  double nsPublic = 0, nsBuild = 0, nsOwn = 0;
  long long nBuild = 0;
  // Per-ask-decision cost of M4+M5, against one Sinkhorn posterior solve -- the
  // unit v0.5 already pays 13 times per decision (refresh + 2 per top-K branch,
  // searchTopK = 6; v05.hpp:531-556).
  double nsBegin = 0, nsScore = 0, nsSink = 0;
  long long nBegin = 0, nScored = 0;

  long long decisions = 0, decCert = 0, decCert50 = 0, decCert90 = 0;
  long long decLastLive = 0, decVoid = 0;
  long long classDecisions = 0, classSeparated = 0;
  double classSizeSum = 0;
  // Per-(decision, card) form of the same question, which is the population the
  // verification report used (all cards with >= 2 legal targets).
  long long pairCards = 0, pairMulti = 0, pairSeparated = 0;

  // information content: cards resolved by an agent's own knowledge vs by the
  // public model of that same seat -- the size of the C1 gap M4 cannot see.
  long long resolvedOwn = 0, resolvedModel = 0, resolvedCmp = 0;
};

int main(int argc, char** argv) {
  int games = 40; uint64_t seed = 31;
  std::string aSpec = "v05", bSpec = "v05";
  bool verbose = false;
  for (int i = 1; i < argc; i++) {
    std::string s = argv[i];
    auto val = [&](const char* k) { return s.rfind(k, 0) == 0 ? s.substr(strlen(k)) : std::string(); };
    if (!val("--games=").empty()) games = atoi(val("--games=").c_str());
    else if (!val("--seed=").empty()) seed = strtoull(val("--seed=").c_str(), nullptr, 10);
    else if (!val("--a=").empty()) aSpec = val("--a=");
    else if (!val("--b=").empty()) bSpec = val("--b=");
    else if (s == "--verbose") verbose = true;
  }

  Rules rules;
  Stats st;

  for (int gi = 0; gi < games; gi++) {
    std::unique_ptr<Agent> ag[NPLAY];
    Agent* ptr[NPLAY];
    for (int p = 0; p < NPLAY; p++) {
      ag[p] = makeAgent(teamOf(p) == 0 ? aSpec : bSpec);
      ptr[p] = ag[p].get();
    }
    Game gm;
    gm.audit = true;                     // also re-run the existing soundness audit

    // One SeatModels per observer seat: the public half is identical (that is
    // check D) but the per-seat refinement reads that observer's own Knowledge.
    m45::SeatModels sm[NPLAY];
    for (int i = 0; i < NPLAY; i++) sm[i].reset(rules.deckSets);
    // Six public models seeded with different `me`, for check D.
    Knowledge six[NPLAY];
    for (int j = 0; j < NPLAY; j++) { m45::initPublicKnowledge(six[j], rules.deckSets); six[j].me = j; }
    // The cost CONTROL for nsOwn: a real agent's own Knowledge -- one that
    // carries constraint C1, its own hand -- consuming the same event.  It is
    // resynced from the live agents at the end of every observer call, so on
    // entry it holds the state just before the new event, and timing
    // shadow[j].onEvent(e) measures exactly the work Agent::observe does
    // (game.hpp:20 is `k.onEvent(e)`).  Timing `six[]` here instead would
    // compare a public model against another public model and would say
    // nothing about what the agent already pays.
    Knowledge shadow[NPLAY];
    bool shadowReady = false;

    size_t seen = 0;
    gm.observer = [&](const Game& G) {
      const auto& hist = G.g.pub.history;
      while (seen < hist.size()) {
        const Event& e = hist[seen++];
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < NPLAY; i++) sm[i].observe(e);
        auto t1 = std::chrono::steady_clock::now();
        if (shadowReady) for (int j = 0; j < NPLAY; j++) shadow[j].onEvent(e);
        auto t2 = std::chrono::steady_clock::now();
        for (int j = 0; j < NPLAY; j++) six[j].onEvent(e);
        st.nsPublic += std::chrono::duration<double, std::nano>(t1 - t0).count() / NPLAY;
        if (shadowReady) {
          st.nsOwn += std::chrono::duration<double, std::nano>(t2 - t1).count() / NPLAY;
          st.ownEvents++;
        }
        st.events++;
      }
      for (int j = 0; j < NPLAY; j++) shadow[j] = G.agents[j]->k;
      shadowReady = true;

      // ---- D: the public deduction state is common knowledge ----
      for (int j = 1; j < NPLAY; j++) {
        st.checksD++;
        bool same = six[0].unresolved == six[j].unresolved
                 && six[0].publicKnown == six[j].publicKnown
                 && six[0].disj.size() == six[j].disj.size()
                 && memcmp(six[0].owner, six[j].owner, sizeof(six[0].owner)) == 0
                 && memcmp(six[0].mask, six[j].mask, sizeof(six[0].mask)) == 0
                 && memcmp(six[0].handCount, six[j].handCount, sizeof(six[0].handCount)) == 0
                 && memcmp(six[0].askCount, six[j].askCount, sizeof(six[0].askCount)) == 0
                 && memcmp(six[0].missCount, six[j].missCount, sizeof(six[0].missCount)) == 0;
        for (size_t d = 0; same && d < six[0].disj.size(); d++)
          if (six[0].disj[d].player != six[j].disj[d].player
              || six[0].disj[d].cards != six[j].disj[d].cards) same = false;
        if (!same) { st.violD++; if (verbose) printf("D violation game %d seat %d\n", gi, j); }
      }

      // ---- A/B/C/E/F: audit every observer's model of every seat ----
      for (int i = 0; i < NPLAY; i++) {
        for (int j = 0; j < NPLAY; j++) {
          auto b0 = std::chrono::steady_clock::now();
          const Knowledge& m = sm[i].of(j, G.agents[i]->k);
          auto b1 = std::chrono::steady_clock::now();
          st.nsBuild += std::chrono::duration<double, std::nano>(b1 - b0).count();
          st.nBuild++;

          int cap[NPLAY] = {0,0,0,0,0,0};
          for (int c = 0; c < NCARD; c++) {
            if (!G.g.pub.setActive[setOf(c)]) continue;
            int truth = trueHolder(G.g, c);
            if (truth < 0) continue;
            st.checksA++;
            if (m.owner[c] < NPLAY) {
              if (m.owner[c] != truth) { st.violA++;
                if (verbose) printf("A: g%d obs%d model%d card%d owner=%d truth=%d\n",
                                    gi, i, j, c, m.owner[c], truth); }
            } else if (m.owner[c] == OUT_OF_PLAY) {
              st.violA++;
            } else {
              if (!(m.mask[c] & (1u << truth))) { st.violA++;
                if (verbose) printf("A: g%d obs%d model%d card%d mask=%02x truth=%d\n",
                                    gi, i, j, c, m.mask[c], truth); }
              cap[truth]++;
            }
          }
          uint8_t q[NPLAY]; m.capacities(q);
          for (int r = 0; r < NPLAY; r++) { st.checksB++; if (q[r] != cap[r]) st.violB++; }
          for (const auto& d : m.disj) {
            st.checksC++;
            bool sat = false;
            uint64_t cc = d.cards;
            while (cc) { int x = __builtin_ctzll(cc); cc &= cc - 1;
              if (trueHolder(G.g, x) == d.player) sat = true; }
            if (!sat) st.violC++;
          }
          if (i == j) {
            // ---- E: the model of myself is never stronger than my own k ----
            const Knowledge& mine = G.agents[i]->k;
            for (int c = 0; c < NCARD; c++) {
              if (!G.g.pub.setActive[setOf(c)]) continue;
              st.checksE++;
              uint8_t mm = m.owner[c] < NPLAY ? uint8_t(1u << m.owner[c]) : m.mask[c];
              uint8_t kk = mine.owner[c] < NPLAY ? uint8_t(1u << mine.owner[c]) : mine.mask[c];
              if (kk & ~mm) st.violE++;      // k allows an owner the model forbids
            }
          } else {
            for (int c = 0; c < NCARD; c++) {
              if (!G.g.pub.setActive[setOf(c)]) continue;
              st.resolvedCmp++;
              if (G.agents[i]->k.owner[c] < NPLAY) st.resolvedOwn++;
              if (m.owner[c] < NPLAY) st.resolvedModel++;
            }
          }
        }
        st.contradictions += sm[i].contradictions;
        sm[i].contradictions = 0;
      }

      // ---- M5 channel measurement for the seat about to move ----
      int mover = G.g.pub.turn;
      if (G.g.pub.handCount[mover]) {
        const Knowledge& mk = G.agents[mover]->k;
        AskMove buf[NSET * SETSZ * 3];
        int n = enumerateAsks(G.g.pub, mk.myHand, mover, buf);
        if (n) {
          // Build a posterior externally so nothing about the live agents is
          // touched.  Same solver and same parameters as v0.5's Fast mode
          // (v05.hpp:174-178).
          Belief bel;
          for (int c = 0; c < NCARD; c++) for (int p = 0; p < NPLAY; p++) bel.marg[c][p] = 0;
          for (int c = 0; c < NCARD; c++) if (mk.owner[c] < NPLAY) bel.marg[c][mk.owner[c]] = 1;
          auto s0 = std::chrono::steady_clock::now();
          bel.sinkhornDisj(mk, 4, 8, 0.26380, 0.13280);
          auto s1 = std::chrono::steady_clock::now();
          st.nsSink += std::chrono::duration<double, std::nano>(s1 - s0).count();

          m45::M45Module mod;
          mod.cfg.conventions = true;      // exercise the D1-gated path too
          mod.cfg.m5d = true;
          mod.models = sm[mover];          // reuse the models already built
          m45::M45Ctx ctx;
          ctx.k = &mk; ctx.bel = &bel; ctx.pub = &G.g.pub; ctx.seat = mover;
          ctx.teamMask = ctx.oppMask = 0;
          for (int p = 0; p < NPLAY; p++) {
            if (teamOf(p) == teamOf(mover)) ctx.teamMask |= 1 << p; else ctx.oppMask |= 1 << p;
          }
          // Cold-cache timing: drop the per-seat models the audit loop above
          // already built, so beginDecision pays for them like the live agent
          // would on its first M5 call after an event.
          mod.models.invalidate();
          auto d0 = std::chrono::steady_clock::now();
          mod.beginDecision(ctx);
          auto d1 = std::chrono::steady_clock::now();
          double sink = 0;
          for (int i2 = 0; i2 < n; i2++)
            sink += mod.score(ctx, buf[i2].card, buf[i2].target,
                              bel.marg[buf[i2].card][buf[i2].target]);
          auto d2 = std::chrono::steady_clock::now();
          if (sink == 1e300) printf(" ");            // keep the loop alive
          st.nsBegin += std::chrono::duration<double, std::nano>(d1 - d0).count();
          st.nsScore += std::chrono::duration<double, std::nano>(d2 - d1).count();
          st.nBegin++; st.nScored += n;
          st.decisions++;
          for (int t = 0; t < NPLAY; t++) if (mod.lastLive[t]) { st.decLastLive++; break; }
          { double bv = 0; double gg[m45::NM45];
            for (int i2 = 0; i2 < n; i2++) {
              mod.features(ctx, buf[i2].card, buf[i2].target,
                           bel.marg[buf[i2].card][buf[i2].target], gg);
              bv = std::max(bv, gg[m45::M45_VOID]);
            }
            if (bv > 0.25) st.decVoid++; }
          double cert = 0;
          for (int t = 0; t < NPLAY; t++) cert = std::max(cert, mod.hardLock[t]);
          if (cert > 0) st.decCert++;
          if (cert > 0.5) st.decCert50++;
          if (cert > 0.9) st.decCert90++;

          // Per-(decision, card): every card with >= 2 targets the hard
          // certificates cannot separate.
          for (int S = 0; S < NSET; S++) {
            if (!G.g.pub.setActive[S]) continue;
            if (!(mk.myHand & setMask(S))) continue;
            for (int ii = 0; ii < SETSZ; ii++) {
              int cc = cardOf(S, ii);
              if (mk.myHand & bit(cc)) continue;
              int cls2[NPLAY], nc2 = 0;
              for (int t = 0; t < NPLAY; t++) {
                if (teamOf(t) == teamOf(mover) || !G.g.pub.handCount[t]) continue;
                bool live = mk.owner[cc] < NPLAY ? mk.owner[cc] == t
                                                 : (mk.mask[cc] & (1u << t)) != 0;
                if (live) cls2[nc2++] = t;
              }
              st.pairCards++;
              if (nc2 < 2) continue;
              st.pairMulti++;
              double lo2 = 1e18, hi2 = -1e18;
              for (int z = 0; z < nc2; z++) {
                double u = mod.score(ctx, cc, cls2[z], bel.marg[cc][cls2[z]]);
                lo2 = std::min(lo2, u); hi2 = std::max(hi2, u);
              }
              if (hi2 - lo2 > 1e-9) st.pairSeparated++;
            }
          }

          // Best card by hit probability, then the class of targets the hard
          // certificates cannot tell apart -- the population P5 measured.
          int bestCard = -1; double bestP = -1;
          for (int i2 = 0; i2 < n; i2++) {
            double p = bel.marg[buf[i2].card][buf[i2].target];
            if (p > bestP) { bestP = p; bestCard = buf[i2].card; }
          }
          if (bestCard >= 0) {
            int cls[NPLAY], nc = 0;
            for (int i2 = 0; i2 < n; i2++) {
              if (buf[i2].card != bestCard) continue;
              int t = buf[i2].target;
              bool live = mk.owner[bestCard] < NPLAY ? mk.owner[bestCard] == t
                                                     : (mk.mask[bestCard] & (1u << t)) != 0;
              if (live) { bool dup = false;
                for (int z = 0; z < nc; z++) if (cls[z] == t) dup = true;
                if (!dup) cls[nc++] = t; }
            }
            if (nc >= 2) {
              st.classDecisions++;
              st.classSizeSum += nc;
              double lo = 1e18, hi = -1e18;
              for (int z = 0; z < nc; z++) {
                double u = mod.score(ctx, bestCard, cls[z], bel.marg[bestCard][cls[z]]);
                lo = std::min(lo, u); hi = std::max(hi, u);
              }
              if (hi - lo > 1e-9) st.classSeparated++;
            }
          }
        }
      }
    };

    gm.run(mixSeed(seed, gi), rules, ptr);
    if (gm.auditViolations) printf("BASELINE runAudit violations in game %d: %lld\n",
                                  gi, gm.auditViolations);
  }

  printf("M4/M5 soundness and cost test -- %d games, seed %llu, %s vs %s\n",
         games, (unsigned long long)seed, aSpec.c_str(), bSpec.c_str());
  printf("public events fed                     %lld\n", st.events);
  printf("A  model excludes true holder         %lld / %lld\n", st.violA, st.checksA);
  printf("B  capacity mismatches                %lld / %lld\n", st.violB, st.checksB);
  printf("C  unsatisfied C5 certificates        %lld / %lld\n", st.violC, st.checksC);
  printf("D  common-knowledge mismatches        %lld / %lld\n", st.violD, st.checksD);
  printf("E  model stronger than own knowledge  %lld / %lld\n", st.violE, st.checksE);
  printf("F  build contradictions               %lld\n", st.contradictions);
  printf("--- cost ---\n");
  printf("public model onEvent      %.0f ns/event   (a real agent's own Knowledge::onEvent, "
         "same event stream: %.0f ns over %lld events)\n",
         st.events ? st.nsPublic / st.events : 0.0,
         st.ownEvents ? st.nsOwn / st.ownEvents : 0.0, st.ownEvents);
  printf("lazy per-seat build       %.0f ns  (%lld builds)\n",
         st.nBuild ? st.nsBuild / st.nBuild : 0.0, st.nBuild);
  printf("per ask decision: beginDecision %.0f ns + %.0f ns scoring %.1f candidates\n",
         st.nBegin ? st.nsBegin / st.nBegin : 0.0,
         st.nBegin ? st.nsScore / st.nBegin : 0.0,
         st.nBegin ? double(st.nScored) / st.nBegin : 0.0);
  printf("  vs ONE Sinkhorn posterior solve %.0f ns  -- v0.5 pays 13 of those per decision\n",
         st.nBegin ? st.nsSink / st.nBegin : 0.0);
  printf("  M4+M5 overhead as a fraction of the 13 solves  %.2f%%\n",
         st.nsSink > 0 ? 100.0 * (st.nsBegin + st.nsScore) / (13.0 * st.nsSink) : 0.0);
  printf("--- what M4/M5 see ---\n");
  printf("cards resolved, own knowledge   %.3f per (obs,seat,card)\n",
         st.resolvedCmp ? double(st.resolvedOwn) / st.resolvedCmp : 0.0);
  printf("cards resolved, M4 model        %.3f  (the C1 gap)\n",
         st.resolvedCmp ? double(st.resolvedModel) / st.resolvedCmp : 0.0);
  printf("decisions                                  %lld\n", st.decisions);
  printf("  with a hard blackball certificate        %.2f%%  (>0.5: %.2f%%, >0.9: %.2f%%)\n",
         st.decisions ? 100.0 * st.decCert / st.decisions : 0.0,
         st.decisions ? 100.0 * st.decCert50 / st.decisions : 0.0,
         st.decisions ? 100.0 * st.decCert90 / st.decisions : 0.0);
  printf("  with a last-live-opponent ask available   %.2f%%   (M5c)\n",
         st.decisions ? 100.0 * st.decLastLive / st.decisions : 0.0);
  printf("  with a void-progress ask worth >0.25      %.2f%%   (M5b)\n",
         st.decisions ? 100.0 * st.decVoid / st.decisions : 0.0);
  printf("  (decision,card) pairs with >=2 live targets %.2f%% of %lld\n",
         st.pairCards ? 100.0 * st.pairMulti / st.pairCards : 0.0, st.pairCards);
  printf("  of those, M5 strictly separates the targets %.2f%%\n",
         st.pairMulti ? 100.0 * st.pairSeparated / st.pairMulti : 0.0);
  printf("  with >=2 hard-indistinguishable targets  %.2f%%  (mean class %.3f)\n",
         st.decisions ? 100.0 * st.classDecisions / st.decisions : 0.0,
         st.classDecisions ? st.classSizeSum / st.classDecisions : 0.0);
  printf("  of those, M5 strictly separates the class %.2f%%\n",
         st.classDecisions ? 100.0 * st.classSeparated / st.classDecisions : 0.0);

  long long fatal = st.violA + st.violB + st.violC + st.violD + st.violE + st.contradictions;
  printf("\n%s\n", fatal ? "FAIL" : "PASS: no seat model ever excluded the true holder of a card.");
  return fatal ? 1 : 0;
}
