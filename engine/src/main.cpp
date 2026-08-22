#include "arena.hpp"
#include "tuner.hpp"
#include "blockdp.hpp"
#include "oracle.hpp"
#include "serve.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

using namespace fish;

static std::string argVal(int argc, char** argv, const char* key, const char* dflt) {
  std::string k = std::string("--") + key + "=";
  for (int i = 1; i < argc; i++) { std::string a = argv[i];
    if (a.rfind(k, 0) == 0) return a.substr(k.size()); }
  return dflt;
}
static bool argFlag(int argc, char** argv, const char* key) {
  std::string k = std::string("--") + key;
  for (int i = 1; i < argc; i++) if (std::string(argv[i]) == k) return true;
  return false;
}

static Rules rulesFrom(int argc, char** argv) {
  Rules r;
  { std::string arb = argVal(argc, argv, "arb", "low");
    r.declArbitration = arb == "high" ? 1 : (arb == "turn" ? 2 : 0); }
  if (argFlag(argc, argv, "legacy")) {
    r.outOfTurnDeclare = false;
    r.cardlessMayDeclare = false;
    r.maxAsks = 360;
    r.nForcedTh = 2; r.forcedTh[0] = 0.38; r.forcedTh[1] = -1.0;
    for (int i = 2; i < 8; i++) r.forcedTh[i] = -1.0;
  }
  r.deckSets = atoi(argVal(argc, argv, "sets", "9").c_str());
  r.maxAsks = atoi(argVal(argc, argv, "maxasks", std::to_string(r.maxAsks).c_str()).c_str());
  if (argFlag(argc, argv, "no-out-of-turn")) r.outOfTurnDeclare = false;
  if (argFlag(argc, argv, "no-cardless-declare")) r.cardlessMayDeclare = false;
  return r;
}

static void printMatch(const MatchStats& st, const std::string& a, const std::string& b, bool json, std::ostream& os,
                       int rotations = 2) {
  int n = st.games * rotations;
  double lo, hi; wilson(st.winsA, n, lo, hi);
  double wr = n ? double(st.winsA) / n : 0;
  // The rotations of one deal are a single correlated cluster, so the interval
  // that is reported has to resample deals, not games.
  double bm, blo, bhi;
  clusterBootstrap(st.paired, rotations, bm, blo, bhi);
  auto acc = [](long long c, long long t) { return t ? double(c) / double(t) : 0.0; };
  if (json) {
    os << "{\"a\":\"" << a << "\",\"b\":\"" << b << "\",\"deals\":" << st.games
       << ",\"games\":" << n << ",\"winRateA\":" << wr
       << ",\"ci\":[" << blo << "," << bhi << "]"
       << ",\"wilsonCI\":[" << lo << "," << hi << "]"
       << ",\"meanSetsA\":" << (st.games ? double(st.sets[0]) / n : 0)
       << ",\"meanSetsB\":" << (st.games ? double(st.sets[1]) / n : 0)
       << ",\"askAccA\":" << acc(st.hits[0], st.asks[0])
       << ",\"askAccB\":" << acc(st.hits[1], st.asks[1])
       << ",\"declAccA\":" << acc(st.declCorrect[0], st.decl[0])
       << ",\"declAccB\":" << acc(st.declCorrect[1], st.decl[1])
       << ",\"declPerGameA\":" << (n ? double(st.decl[0]) / n : 0)
       << ",\"declPerGameB\":" << (n ? double(st.decl[1]) / n : 0)
       << ",\"forcedAccA\":" << acc(st.fdeclCorrect[0], st.fdecl[0])
       << ",\"forcedAccB\":" << acc(st.fdeclCorrect[1], st.fdecl[1])
       << ",\"forcedPerGameA\":" << (n ? double(st.fdecl[0]) / n : 0)
       << ",\"outOfTurnA\":" << (n ? double(st.outOfTurn[0]) / n : 0)
       << ",\"outOfTurnB\":" << (n ? double(st.outOfTurn[1]) / n : 0)
       << ",\"lockHoldA\":" << (st.lockedDecls[0] ? double(st.lockHeld[0]) / st.lockedDecls[0] : 0)
       << ",\"lockHoldB\":" << (st.lockedDecls[1] ? double(st.lockHeld[1]) / st.lockedDecls[1] : 0)
       << ",\"eventsPerGame\":" << (n ? double(st.events) / n : 0)
       << ",\"limitHitRate\":" << (n ? double(st.limitHits) / n : 0)
       << ",\"auditViolations\":" << st.auditViolations
       << ",\"auditChecks\":" << st.auditChecks
       << ",\"seconds\":" << st.seconds << "}";
  } else {
    os << a << " vs " << b << "\n";
    os << "  win rate      " << 100 * wr << "%  [" << 100 * lo << ", " << 100 * hi << "]  n=" << n << "\n";
    os << "  mean sets     " << double(st.sets[0]) / n << " - " << double(st.sets[1]) / n << "\n";
    os << "  ask accuracy  " << 100 * acc(st.hits[0], st.asks[0]) << "% / " << 100 * acc(st.hits[1], st.asks[1]) << "%\n";
    os << "  declarations  " << double(st.decl[0]) / n << "/game at " << 100 * acc(st.declCorrect[0], st.decl[0]) << "%"
       << "   opp " << double(st.decl[1]) / n << "/game at " << 100 * acc(st.declCorrect[1], st.decl[1]) << "%\n";
    os << "  forced decls  " << double(st.fdecl[0]) / n << "/game at " << 100 * acc(st.fdeclCorrect[0], st.fdecl[0]) << "%\n";
    os << "  out-of-turn   " << double(st.outOfTurn[0]) / n << " / " << double(st.outOfTurn[1]) / n << " per game\n";
    os << "  lock hold     " << (st.lockedDecls[0] ? double(st.lockHeld[0]) / st.lockedDecls[0] : 0)
       << " / " << (st.lockedDecls[1] ? double(st.lockHeld[1]) / st.lockedDecls[1] : 0) << " events before cashing\n";
    os << "  events/game   " << double(st.events) / n << "   limit hits " << 100.0 * st.limitHits / n << "%\n";
    if (st.auditChecks) os << "  audit         " << st.auditViolations << " violations in " << st.auditChecks << " checks\n";
    os << "  elapsed       " << st.seconds << "s  (" << (n / std::max(1e-9, st.seconds)) << " games/s)\n";
  }
}

int main(int argc, char** argv) {
  std::string cmd = argc > 1 ? argv[1] : "help";
  int threads = atoi(argVal(argc, argv, "threads", "0").c_str());

  if (cmd == "match") {
    MatchConfig mc;
    mc.specA = argVal(argc, argv, "a", "v04");
    mc.specB = argVal(argc, argv, "b", "v03");
    mc.games = atoi(argVal(argc, argv, "games", "1000").c_str());
    mc.seed = strtoull(argVal(argc, argv, "seed", "20260821").c_str(), nullptr, 10);
    mc.rules = rulesFrom(argc, argv);
    mc.audit = argFlag(argc, argv, "audit");
    mc.threads = threads;
    mc.rotations = atoi(argVal(argc, argv, "rotations", "2").c_str());
    MatchStats st = runMatch(mc);
    bool json = argFlag(argc, argv, "json");
    printMatch(st, mc.specA, mc.specB, json, std::cout, mc.rotations);
    double m, lo, hi;
    clusterBootstrap(st.paired, mc.rotations, m, lo, hi);
    if (!json) std::cout << "  cluster boot  " << 100 * m << "% [" << 100 * lo << ", " << 100 * hi << "]\n";
    std::cout << std::endl;
    return 0;
  }

  if (cmd == "verify") {
    int games = atoi(argVal(argc, argv, "games", "200").c_str());
    const char* pool[] = {"v04", "v03", "v02", "lockout", "detective", "hunter",
                          "diversifier", "bluffer", "random"};
    int np = 9;
    long long viol = 0, checks = 0, limit = 0, bad = 0;
    for (int i = 0; i < np; i++) for (int j = 0; j < np; j++) {
      MatchConfig mc;
      mc.specA = pool[i]; mc.specB = pool[j];
      mc.games = std::max(1, games / (np * np));
      mc.seed = 991 + i * 31 + j;
      mc.rules = rulesFrom(argc, argv);
      mc.audit = true; mc.threads = threads;
      MatchStats st = runMatch(mc);
      viol += st.auditViolations; checks += st.auditChecks; limit += st.limitHits;
      if (st.sets[0] + st.sets[1] != (long long)st.games * 2 * mc.rules.deckSets) bad++;
    }
    std::cout << "audit violations: " << viol << " / " << checks << " checks\n";
    std::cout << "set-conservation failures: " << bad << "\n";
    std::cout << "action-limit games: " << limit << "\n";
    // determinism
    MatchConfig mc; mc.specA = "v04"; mc.specB = "v03"; mc.games = 50; mc.seed = 7; mc.threads = 1;
    mc.rules = rulesFrom(argc, argv);
    MatchStats s1 = runMatch(mc), s2 = runMatch(mc);
    std::cout << "determinism: " << ((s1.winsA == s2.winsA && s1.events == s2.events) ? "PASS" : "FAIL") << "\n";
    std::cout << ((viol == 0 && bad == 0) ? "VERIFY PASS" : "VERIFY FAIL") << std::endl;
    return viol == 0 && bad == 0 ? 0 : 1;
  }

  if (cmd == "matrix") {
    std::string list = argVal(argc, argv, "policies", "v04,v03,v02,lockout,detective,diversifier,hunter,bluffer,random");
    std::vector<std::string> ps;
    { std::stringstream ss(list); std::string it; while (std::getline(ss, it, ',')) ps.push_back(it); }
    int games = atoi(argVal(argc, argv, "games", "300").c_str());
    uint64_t seed = strtoull(argVal(argc, argv, "seed", "5150").c_str(), nullptr, 10);
    Rules r = rulesFrom(argc, argv);
    std::cout << "{\"policies\":[";
    for (size_t i = 0; i < ps.size(); i++) std::cout << (i ? "," : "") << "\"" << ps[i] << "\"";
    std::cout << "],\"cells\":[";
    bool first = true;
    for (size_t i = 0; i < ps.size(); i++) for (size_t j = 0; j < ps.size(); j++) {
      if (i == j) continue;
      MatchConfig mc; mc.specA = ps[i]; mc.specB = ps[j]; mc.games = games;
      mc.seed = seed + i * 1000 + j; mc.rules = r; mc.threads = threads;
      mc.rotations = atoi(argVal(argc, argv, "rotations", "2").c_str());
      MatchStats st = runMatch(mc);
      if (!first) std::cout << ",";
      first = false;
      printMatch(st, ps[i], ps[j], true, std::cout, mc.rotations);
      std::cout.flush();
    }
    std::cout << "]}" << std::endl;
    return 0;
  }

  if (cmd == "tune") {
    TuneSpec sp;
    std::string panel = argVal(argc, argv, "panel", "v03,lockout,detective,diversifier");
    { std::stringstream ss(panel); std::string it; while (std::getline(ss, it, ',')) sp.panel.push_back(it); }
    sp.gamesPerOpponent = atoi(argVal(argc, argv, "games", "250").c_str());
    sp.population = atoi(argVal(argc, argv, "pop", "24").c_str());
    sp.elite = atoi(argVal(argc, argv, "elite", "6").c_str());
    sp.generations = atoi(argVal(argc, argv, "gens", "40").c_str());
    sp.beta = atof(argVal(argc, argv, "beta", "10").c_str());
    sp.sigma0 = atof(argVal(argc, argv, "sigma", "0.6").c_str());
    sp.seed = strtoull(argVal(argc, argv, "seed", "424242").c_str(), nullptr, 10);
    sp.rules = rulesFrom(argc, argv);
    sp.threads = threads;
    sp.baseSpec = argVal(argc, argv, "base", "v04");
    std::vector<double> mu;
    std::string init = argVal(argc, argv, "init", "");
    if (!init.empty()) { std::stringstream ss(init); std::string t; while (std::getline(ss, t, '|')) mu.push_back(atof(t.c_str())); }
    else { V04Config d; for (int i = 0; i < NFEAT; i++) mu.push_back(d.w[i]); }
    if (argFlag(argc, argv, "full") && mu.size() == NFEAT) {
      V04Config d;
      mu.push_back(d.declThreshold); mu.push_back(d.lockedAllocThresh);
      mu.push_back(d.askFloor); mu.push_back(double(d.patiencePool));
      mu.push_back(d.oppCardFloor); mu.push_back(d.valueWeight);
      mu.push_back(d.linearWeight); mu.push_back(d.minTeamProb);
      mu.push_back(d.declareMargin);
      mu.push_back(d.priorTheta); mu.push_back(d.priorPhi);
      mu.push_back(double(d.searchTopK)); mu.push_back(d.chainWeight); mu.push_back(d.threatWeight);
    }
    sp.lo.assign(mu.size(), -12.0); sp.hi.assign(mu.size(), 20.0);
    sp.lo[0] = 0.0; sp.hi[0] = 30.0;
    if (mu.size() > NFEAT) {
      const double plo[14] = {0.55, 0.55, 0.0,  0.0, 0.0,  0.0, 0.0,  0.05, -0.05, 0.0, 0.0, 0.0, 0.0, 0.0};
      const double phi[14] = {0.999, 0.99999, 0.6, 20.0, 12.0, 40.0, 3.0, 0.95, 0.05, 1.5, 0.6, 14.0, 12.0, 12.0};
      for (int i = 0; i < 14 && NFEAT + i < (int)mu.size(); i++) { sp.lo[NFEAT + i] = plo[i]; sp.hi[NFEAT + i] = phi[i]; }
    }
    std::string sigPer = argVal(argc, argv, "sigmaparams", "");
    std::string outPath = argVal(argc, argv, "out", "");
    FILE* out = outPath.empty() ? stdout : fopen(outPath.c_str(), "w");
    std::vector<double> w = tune(sp, mu, out);
    fprintf(out, "{\"weights\":\"");
    for (size_t i = 0; i < w.size(); i++) fprintf(out, "%s%.5f", i ? "|" : "", w[i]);
    fprintf(out, "\"}\n");
    if (out != stdout) fclose(out);
    printf("weights=");
    for (size_t i = 0; i < w.size(); i++) printf("%s%.5f", i ? "|" : "", w[i]);
    printf("\n");
    return 0;
  }

  if (cmd == "selftest") {
    // Validate the exact belief engines against each other and against exact
    // rejection sampling from the constraint-satisfying posterior.
    int games = atoi(argVal(argc, argv, "games", "60").c_str());
    Rules r = rulesFrom(argc, argv);
    double maxCardDiff = 0, maxDisjDiff = 0, maxSinkDiff = 0, sumSink = 0; long long nSink = 0;
    long long checks = 0, blockFail = 0;
    Rng rng(4242);
    for (int gi = 0; gi < games; gi++) {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent("v04"); B[i] = makeAgent("v03"); }
      Agent* ag[NPLAY];
      for (int p = 0; p < NPLAY; p++) ag[p] = (p % 2 == 0) ? A[p / 2].get() : B[p / 2].get();
      Game game;
      game.setup(mixSeed(777, gi), r, ag);
      // replay the game, auditing beliefs at every event
      int guard = 0;
      while (game.g.pub.activeSets() && guard++ < 300) {
        for (int p = 0; p < NPLAY; p++) {
          const Knowledge& kk = ag[p]->k;
          if (!kk.unresolved) continue;
          BlockDP bd;
          if (!bd.build(kk)) { blockFail++; continue; }
          static double mb[NCARD][NPLAY], mc[NCARD][NPLAY];
          for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) { mb[c][q] = 0; mc[c][q] = 0; }
          bd.marginals(mb);
          DealDP dd;
          bool ddok = dd.build(kk);
          if (ddok) dd.marginals(mc);
          Belief sk; sk.sinkhornDisj(kk, 4, 8);
          // exact rejection sample as ground truth for the disjunctive posterior
          static double ms[NCARD][NPLAY];
          for (int c = 0; c < NCARD; c++) for (int q = 0; q < NPLAY; q++) ms[c][q] = 0;
          int drawn = 0;
          if (ddok) {
            std::array<uint8_t, NCARD> buf{};
            for (int c = 0; c < NCARD; c++) buf[c] = kk.owner[c];
            for (int t = 0; t < 40000 && drawn < 4000; t++) {
              dd.sample(rng, buf.data());
              if (!Belief::satisfies(kk, buf.data())) continue;
              uint64_t u = kk.unresolved;
              while (u) { int c = __builtin_ctzll(u); u &= u - 1; ms[c][buf[c]] += 1; }
              drawn++;
            }
          }
          uint64_t u = kk.unresolved;
          while (u) {
            int c = __builtin_ctzll(u); u &= u - 1;
            for (int q = 0; q < NPLAY; q++) {
              if (kk.disj.empty() && ddok) maxCardDiff = std::max(maxCardDiff, std::fabs(mb[c][q] - mc[c][q]));
              if (drawn >= 1500) {
                double gt = ms[c][q] / drawn;
                maxDisjDiff = std::max(maxDisjDiff, std::fabs(mb[c][q] - gt));
                double sd = std::fabs(sk.marg[c][q] - mb[c][q]);
                maxSinkDiff = std::max(maxSinkDiff, sd);
                sumSink += sd; nSink++;
              }
            }
            checks++;
          }
          break;   // one observer per event is enough
        }
        // advance one action
        if (!game.g.pub.teamAlive(0) || !game.g.pub.teamAlive(1)) break;
        if (!game.g.pub.handCount[game.g.turn]) break;
        AskMove mv = ag[game.g.turn]->chooseAsk(game.g.pub);
        if (!legalAsk(game.g, game.g.turn, mv.card, mv.target)) break;
        int actor = game.g.turn, target = mv.target, card = mv.card;
        bool success = (game.g.hand[target] & bit(card)) != 0;
        if (success) { game.g.hand[target] &= ~bit(card); game.g.hand[actor] |= bit(card); }
        game.g.pub.handCount[actor] = uint8_t(popcount64(game.g.hand[actor]));
        game.g.pub.handCount[target] = uint8_t(popcount64(game.g.hand[target]));
        Event e{}; e.kind = Kind::Ask; e.actor = uint8_t(actor); e.target = uint8_t(target);
        e.card = uint8_t(card); e.set = uint8_t(setOf(card)); e.success = success;
        game.emit(e);
        if (!success) { game.g.turn = target; game.g.pub.turn = target; }
      }
    }
    printf("checks                    %lld\n", checks);
    printf("block build failures      %lld\n", blockFail);
    printf("block vs card DP (no C5)  max abs diff %.3e\n", maxCardDiff);
    printf("block vs exact sampling   max abs diff %.3e\n", maxDisjDiff);
    printf("sinkhorn vs block  max %.4f  mean %.5f\n", maxSinkDiff, nSink ? sumSink / nSink : 0.0);
    printf("%s\n", (maxCardDiff < 1e-9 && maxDisjDiff < 0.05 && blockFail == 0) ? "SELFTEST PASS" : "SELFTEST CHECK");
    printf("note: allocation probabilities are validated by `fish oracle`, not here\n");
    return 0;
  }


  if (cmd == "oracle") {
    // Deterministic brute-force validation of the exact block engine.  Every
    // quantity the engine claims to compute exactly -- Z, per-card marginals,
    // team-ownership probabilities and full allocation probabilities -- is
    // compared against exhaustive enumeration of the posterior on small
    // reachable states, and the sampler's frequencies are compared against the
    // exact marginals.  Coverage is reported alongside the result: states whose
    // posterior is too large to enumerate are counted, not silently dropped.
    int games      = atoi(argVal(argc, argv, "games", "200").c_str());
    long long maxD = atoll(argVal(argc, argv, "maxdeals", "200000").c_str());
    int samples    = atoi(argVal(argc, argv, "samples", "3000").c_str());
    uint64_t seed  = strtoull(argVal(argc, argv, "seed", "20260822").c_str(), nullptr, 10);
    std::string aSpec = argVal(argc, argv, "a", "v04");
    std::string bSpec = argVal(argc, argv, "b", "v03");
    Rules r = rulesFrom(argc, argv);
    OracleStats st;
    Rng rng(seed ? seed : 1);
    static BruteForce bf;
    std::unique_ptr<Agent> A[3], B[3];
    for (int i = 0; i < 3; i++) { A[i] = makeAgent(aSpec); B[i] = makeAgent(bSpec); }
    Agent* ag[NPLAY];
    for (int p = 0; p < NPLAY; p++) ag[p] = (p % 2 == 0) ? A[p / 2].get() : B[p / 2].get();
    Game game;
    game.observer = [&](const Game& gm) {
      for (int p = 0; p < NPLAY; p++) {
        const Knowledge& kk = gm.agents[p]->k;
        if (!kk.unresolved) continue;
        DealDP dd;
        if (!dd.build(kk)) continue;
        if (!(dd.N > 0) || dd.N > double(maxD)) { st.statesSkipped++; continue; }
        if (!bf.enumerate(kk, maxD * 64)) { st.statesSkipped++; continue; }
        BlockDP bd;
        if (!bd.build(kk)) { st.buildFailures++; continue; }
        oracleCheckState(kk, bd, bf, st, rng, samples);
      }
    };
    for (int gi = 0; gi < games; gi++) game.run(mixSeed(seed, gi), r, ag);
    printf("games replayed             %d  (%s vs %s)\n", games, aSpec.c_str(), bSpec.c_str());
    printf("states enumerated          %lld  (with a live C5 certificate: %lld)\n",
           st.statesChecked, st.statesWithC5);
    printf("states skipped (too large) %lld\n", st.statesSkipped);
    printf("block build failures       %lld\n", st.buildFailures);
    printf("consistent deals counted   %lld\n", st.deals);
    printf("partition function Z       max rel diff %.3e\n", st.maxZRelDiff);
    printf("per-card marginals         max abs diff %.3e over %lld checks\n",
           st.maxMarginalDiff, st.marginalChecks);
    printf("team-ownership prob        max abs diff %.3e over %lld checks\n",
           st.maxTeamDiff, st.teamChecks);
    printf("named allocation prob      max abs diff %.3e over %lld checks\n",
           st.maxAllocDiff, st.allocChecks);
    printf("equal-prob corollary       max within-class spread %.3e\n", st.maxCountVectorSpread);
    printf("bestTeamAllocation         %lld checks, %lld inconsistent, %lld not argmax, max diff %.3e\n",
           st.bestAllocChecks, st.bestAllocInconsistent, st.bestAllocNotArgmax, st.maxBestAllocDiff);
    printf("sampler vs exact marginals max abs diff %.4f over %lld draws\n",
           st.maxSampleDiff, st.sampleDraws);
    printf("%s\n", st.pass() ? "ORACLE PASS" : "ORACLE FAIL");
    return st.pass() ? 0 : 1;
  }

  if (cmd == "gateaudit") {
    // Corpus-wide false-negative audit of the declaration pre-gates.  The
    // shipped policy screens half-suits with two cheap scores before running the
    // full posterior query; neither screen is proved to be an upper bound on the
    // full evaluation, so this re-runs the complete evaluation on every half-suit
    // the screens reject and counts how often it would have declared one.
    std::string a = argVal(argc, argv, "a", "v04:mgate=0.008,gateaudit=1");
    int deals = atoi(argVal(argc, argv, "games", "300").c_str());
    uint64_t seed = strtoull(argVal(argc, argv, "seed", "90210").c_str(), nullptr, 10);
    int rots = atoi(argVal(argc, argv, "rotations", "6").c_str());
    std::string panel = argVal(argc, argv, "panel", "v03,lockout,detective,v02,diversifier,hunter,bluffer,random");
    std::vector<std::string> opps;
    { std::stringstream ss(panel); std::string it; while (std::getline(ss, it, ',')) opps.push_back(it); }
    Rules r = rulesFrom(argc, argv);
    for (const auto& opp : opps) {
      MatchConfig mc;
      mc.specA = a; mc.specB = opp; mc.games = deals; mc.rotations = rots;
      mc.seed = seed; mc.rules = r; mc.threads = threads;
      runMatch(mc);
    }
    auto& G = gateAudit();
    long long seen = G.setsSeen.load(), rej = G.setsRejected.load(), fn = G.falseNegatives.load();
    printf("opponents                  %zu\n", opps.size());
    printf("declaration opportunities  %lld\n", G.opportunities.load());
    printf("(opportunity, half-suit)   %lld\n", seen);
    printf("rejected by a cheap gate   %lld  (%.3f%% of pairs)\n",
           rej, seen ? 100.0 * double(rej) / double(seen) : 0.0);
    printf("false negatives            %lld  (%.5f%% of rejections)\n",
           fn, rej ? 100.0 * double(fn) / double(rej) : 0.0);
    printf("declarations, gated        %lld\n", G.gatedDeclares.load());
    printf("declarations, ungated      %lld\n", G.ungatedDeclares.load());
    printf("opportunities where the chosen action differs  %lld\n", G.actionsChanged.load());
    printf("%s\n", (fn == 0 && G.actionsChanged.load() == 0) ? "GATEAUDIT PASS (no false negative observed)"
                                                             : "GATEAUDIT: FALSE NEGATIVES PRESENT");
    return 0;
  }

  if (cmd == "calibrate") {
    std::string a = argVal(argc, argv, "a", "v04");
    std::string b = argVal(argc, argv, "b", "v03");
    int deals = atoi(argVal(argc, argv, "games", "300").c_str());
    Rules r = rulesFrom(argc, argv);
    CalibSink sink = collectCalibration(a, b, deals, strtoull(argVal(argc, argv, "seed", "8181").c_str(), nullptr, 10), r);
    Reliability ra = reliability(sink.ask), rd = reliability(sink.decl);
    auto dump = [&](const char* label, const Reliability& x) {
      printf("%s n=%d brier=%.5f logloss=%.5f ece=%.5f meanPred=%.4f meanObs=%.4f\n",
             label, x.n, x.brier, x.logloss, x.ece, x.meanPred, x.meanObs);
      for (int i = 0; i < 10; i++) if (x.binN[i])
        printf("   [%.1f,%.1f) n=%6d pred=%.4f obs=%.4f\n", i / 10.0, (i + 1) / 10.0, x.binN[i], x.binPred[i], x.binObs[i]);
    };
    dump("ask ", ra);
    dump("decl", rd);
    return 0;
  }

  if (cmd == "ablate") {
    // Paired ablations: every variant plays the SAME deals against the SAME
    // panel, so the difference from the reference is a matched comparison.
    std::string ref = argVal(argc, argv, "ref", "v04");
    std::string variants = argVal(argc, argv, "variants", "");
    std::string panel = argVal(argc, argv, "panel", "v03,lockout,detective");
    int deals = atoi(argVal(argc, argv, "games", "500").c_str());
    int rot = atoi(argVal(argc, argv, "rotations", "2").c_str());
    uint64_t seed = strtoull(argVal(argc, argv, "seed", "606060").c_str(), nullptr, 10);
    Rules r = rulesFrom(argc, argv);
    std::vector<std::string> vs, ps;
    { std::stringstream ss(variants); std::string it; while (std::getline(ss, it, ';')) if (!it.empty()) vs.push_back(it); }
    { std::stringstream ss(panel); std::string it; while (std::getline(ss, it, ',')) ps.push_back(it); }
    auto runAll = [&](const std::string& spec) {
      std::vector<uint8_t> all;
      int wins = 0, tot = 0;
      std::vector<double> per;
      for (size_t i = 0; i < ps.size(); i++) {
        MatchConfig mc; mc.specA = spec; mc.specB = ps[i]; mc.games = deals;
        mc.seed = mixSeed(seed, i * 7919 + 3); mc.rules = r; mc.threads = threads; mc.rotations = rot;
        MatchStats st = runMatch(mc);
        all.insert(all.end(), st.paired.begin(), st.paired.end());
        wins += st.winsA; tot += st.games * rot;
        per.push_back(double(st.winsA) / double(st.games * rot));
      }
      return std::make_tuple(all, double(wins) / tot, per);
    };
    auto [refPaired, refWr, refPer] = runAll(ref);
    printf("{\"reference\":\"%s\",\"winRate\":%.5f,\"perOpponent\":[", ref.c_str(), refWr);
    for (size_t i = 0; i < refPer.size(); i++) printf("%s%.5f", i ? "," : "", refPer[i]);
    printf("],\"panel\":[");
    for (size_t i = 0; i < ps.size(); i++) printf("%s\"%s\"", i ? "," : "", ps[i].c_str());
    printf("],\"variants\":[\n");
    for (size_t v = 0; v < vs.size(); v++) {
      auto [vp, vwr, vper] = runAll(vs[v]);
      double m, lo, hi;
      pairedBootstrap(refPaired, vp, m, lo, hi, 31337, 20000, double(rot));
      printf("  {\"spec\":\"%s\",\"winRate\":%.5f,\"deltaFromRef\":%.5f,\"ci\":[%.5f,%.5f],\"perOpponent\":[",
             vs[v].c_str(), vwr, m, lo, hi);
      for (size_t i = 0; i < vper.size(); i++) printf("%s%.5f", i ? "," : "", vper[i]);
      printf("]}%s\n", v + 1 < vs.size() ? "," : "");
      fflush(stdout);
    }
    printf("]}\n");
    return 0;
  }

  if (cmd == "fitvalue") {
    // Ridge regression of the final set differential on public-belief-state
    // features, gathered from self-play decision points.
    std::string a = argVal(argc, argv, "a", "v04:value=0");
    std::string b = argVal(argc, argv, "b", "v03");
    int deals = atoi(argVal(argc, argv, "games", "400").c_str());
    double lambda = atof(argVal(argc, argv, "lambda", "1e-3").c_str());
    Rules r = rulesFrom(argc, argv);
    uint64_t seed = strtoull(argVal(argc, argv, "seed", "31415").c_str(), nullptr, 10);
    int nThreads = threads > 0 ? threads : int(std::thread::hardware_concurrency());
    std::vector<ValueSink> sinks(nThreads);
    std::vector<std::thread> pool;
    for (int t = 0; t < nThreads; t++) pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(a); B[i] = makeAgent(b); }
      Game game;
      for (int i = t; i < deals; i += nThreads) {
        uint64_t s2 = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
        for (int orient = 0; orient < 2; orient++) {
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.vsink = &sinks[t];
          game.run(s2, r, ag);
        }
      }
    });
    for (auto& th : pool) th.join();
    int D = NVFEAT;
    std::vector<double> XtX(D * D, 0.0), Xty(D, 0.0);
    long long rows = 0;
    for (auto& sk : sinks) for (size_t i = 0; i < sk.y.size(); i++) {
      const auto& x = sk.X[i];
      for (int u = 0; u < D; u++) {
        Xty[u] += double(x[u]) * sk.y[i];
        for (int v = 0; v < D; v++) XtX[u * D + v] += double(x[u]) * double(x[v]);
      }
      rows++;
    }
    for (int u = 0; u < D; u++) XtX[u * D + u] += lambda * rows;
    // Gaussian elimination
    std::vector<double> M(XtX), rhs(Xty), w(D, 0.0);
    for (int c = 0; c < D; c++) {
      int piv = c;
      for (int rr = c + 1; rr < D; rr++) if (std::fabs(M[rr * D + c]) > std::fabs(M[piv * D + c])) piv = rr;
      if (std::fabs(M[piv * D + c]) < 1e-12) continue;
      if (piv != c) { for (int j = 0; j < D; j++) std::swap(M[c * D + j], M[piv * D + j]); std::swap(rhs[c], rhs[piv]); }
      for (int rr = 0; rr < D; rr++) {
        if (rr == c) continue;
        double fac = M[rr * D + c] / M[c * D + c];
        if (fac == 0) continue;
        for (int j = 0; j < D; j++) M[rr * D + j] -= fac * M[c * D + j];
        rhs[rr] -= fac * rhs[c];
      }
    }
    for (int c = 0; c < D; c++) w[c] = std::fabs(M[c * D + c]) > 1e-12 ? rhs[c] / M[c * D + c] : 0.0;
    double sse = 0, sst = 0, ybar = 0; long long nAll = 0;
    for (auto& sk : sinks) for (size_t i = 0; i < sk.y.size(); i++) { ybar += sk.y[i]; nAll++; }
    ybar /= std::max(1LL, nAll);
    for (auto& sk : sinks) for (size_t i = 0; i < sk.y.size(); i++) {
      double pred = 0;
      for (int u = 0; u < D; u++) pred += w[u] * sk.X[i][u];
      sse += (pred - sk.y[i]) * (pred - sk.y[i]);
      sst += (sk.y[i] - ybar) * (sk.y[i] - ybar);
    }
    fprintf(stderr, "rows=%lld  R2=%.4f  rmse=%.4f\n", rows, 1.0 - sse / std::max(1e-9, sst), std::sqrt(sse / std::max(1LL, nAll)));
    printf("vweights=");
    for (int u = 0; u < D; u++) printf("%s%.6f", u ? "|" : "", w[u]);
    printf("\n");
    return 0;
  }

  if (cmd == "bench") {
    MatchConfig mc; mc.specA = argVal(argc, argv, "a", "v04"); mc.specB = argVal(argc, argv, "b", "v04");
    mc.games = atoi(argVal(argc, argv, "games", "200").c_str());
    mc.rules = rulesFrom(argc, argv); mc.threads = threads;
    MatchStats st = runMatch(mc);
    std::cout << (st.games * 2 / st.seconds) << " games/s over " << st.games * 2 << " games\n";
    return 0;
  }

  if (cmd == "serve") {
    int port = atoi(argVal(argc, argv, "port", "8173").c_str());
    return runServe(port, argVal(argc, argv, "web", ""), argv[0]);
  }

  std::cout << "usage: fish <match|verify|matrix|bench|serve> [--a=SPEC] [--b=SPEC] [--games=N] [--seed=S] [--legacy] [--json] [--audit]\n";
  std::cout << "       fish serve [--port=8173] [--web=DIR]   interactive table in a browser\n";
  return 0;
}
