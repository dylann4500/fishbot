// P6 scratch driver.  Not part of `fish`; built separately:
//   clang++ -std=c++20 -O3 -march=native src/probe_declaration_main.cpp -o probe_decl -pthread
#include "probe_declaration.hpp"
#include <iostream>
#include <cstdio>

using namespace fish;
using namespace fish::probe;

static std::string argVal(int argc, char** argv, const char* key, const char* dflt) {
  std::string k = std::string("--") + key + "=";
  for (int i = 1; i < argc; i++) { std::string a = argv[i];
    if (a.rfind(k, 0) == 0) return a.substr(k.size()); }
  return dflt;
}

static const char* modeName(int m) {
  switch (m) { case 0: return "lowest-seat"; case 1: return "highest-seat";
               case 2: return "from-turn";   case 3: return "confidence(UNSAFE)";
               case 4: return "ladder(safe)"; }
  return "?";
}

int main(int argc, char** argv) {
  std::string cmd = argc > 1 ? argv[1] : "help";
  int threads = atoi(argVal(argc, argv, "threads", "0").c_str());
  int games   = atoi(argVal(argc, argv, "games", "600").c_str());
  uint64_t seed = strtoull(argVal(argc, argv, "seed", "31").c_str(), nullptr, 10);
  std::string spec = argVal(argc, argv, "spec", "v04");
  Rules rules;

  // ---------------------------------------------------------------- arb
  if (cmd == "veto") {
    // Information-safe public confidence floor: a seat proposes a voluntary
    // declaration only when its own confidence clears a PUBLIC constant.  No
    // private value is ever compared across seats.
    double fx = atof(argVal(argc, argv, "fx", "0.5").c_str());
    double fy = atof(argVal(argc, argv, "fy", "0").c_str());
    ArbMatch st = runArb(spec, games, seed, rules, 0, 0, true, true, threads,
                         0.0, 0, 0.0, 0.0, fx, fy);
    int n = st.deals * 2;
    double m, lo, hi; clusterBootstrap(st.paired, 2, m, lo, hi);
    printf("{\"cmd\":\"veto\",\"spec\":\"%s\",\"floorX\":%.4f,\"floorY\":%.4f,"
           "\"deals\":%d,\"games\":%d,\"xWinRate\":%.6f,\"ci\":[%.6f,%.6f],"
           "\"xSets\":%.4f,\"ySets\":%.4f,\"xDeclAcc\":%.5f,\"yDeclAcc\":%.5f,"
           "\"xDeclPerGame\":%.4f,\"yDeclPerGame\":%.4f,\"xForcedPerGame\":%.4f,"
           "\"yForcedPerGame\":%.4f,\"eventsPerGame\":%.2f,\"limitHitRate\":%.4f}\n",
           spec.c_str(), fx, fy, st.deals, n, double(st.xWins) / n, lo, hi,
           double(st.xSets) / n, double(st.ySets) / n,
           st.xDecl ? double(st.xDeclOk) / st.xDecl : 0.0,
           st.yDecl ? double(st.yDeclOk) / st.yDecl : 0.0,
           double(st.xDecl) / n, double(st.yDecl) / n,
           double(st.xForced) / n, double(st.yForced) / n, double(st.events) / n,
           double(st.limitHits) / n);
    return 0;
  }

  if (cmd == "arb") {
    int mx = atoi(argVal(argc, argv, "x", "3").c_str());
    int my = atoi(argVal(argc, argv, "y", "0").c_str());
    double floorTh = atof(argVal(argc, argv, "floor", "0").c_str());
    int rungs = atoi(argVal(argc, argv, "rungs", "0").c_str());
    ArbMatch st = runArb(spec, games, seed, rules, mx, my, true, true, threads, floorTh, rungs);
    int n = st.deals * 2;
    double m, lo, hi; clusterBootstrap(st.paired, 2, m, lo, hi);
    printf("{\"cmd\":\"arb\",\"spec\":\"%s\",\"x\":%d,\"xname\":\"%s\",\"y\":%d,\"yname\":\"%s\","
           "\"deals\":%d,\"games\":%d,\"xWinRate\":%.6f,\"ci\":[%.6f,%.6f],"
           "\"xSets\":%.4f,\"ySets\":%.4f,\"xDeclAcc\":%.5f,\"yDeclAcc\":%.5f,"
           "\"xDeclPerGame\":%.4f,\"yDeclPerGame\":%.4f,"
           "\"xForcedPerGame\":%.4f,\"yForcedPerGame\":%.4f,"
           "\"eventsPerGame\":%.2f,\"declRounds\":%lld,\"races\":%lld,\"racesDiffSet\":%lld,"
           "\"racesDiffConf\":%lld,\"racesConfWouldDiffer\":%lld,"
           "\"contested\":%lld,\"contestedLowRight\":%lld,\"contestedConfRight\":%lld,"
           "\"contestedLadderRight\":%lld,\"bothRight\":%lld,\"bothWrong\":%lld,"
           "\"confOnlyRight\":%lld,\"lowOnlyRight\":%lld,\"meanConfGap\":%.5f,"
           "\"floor\":%.4f,\"rungs\":%d,"
           "\"ladderRecovery\":{\"R2\":%lld,\"R3\":%lld,\"R5\":%lld,\"R9\":%lld,"
           "\"R17\":%lld,\"R33\":%lld,\"R65\":%lld}}\n",
           spec.c_str(), mx, modeName(mx), my, modeName(my), st.deals, n,
           double(st.xWins) / n, lo, hi,
           double(st.xSets) / n, double(st.ySets) / n,
           st.xDecl ? double(st.xDeclOk) / st.xDecl : 0.0,
           st.yDecl ? double(st.yDeclOk) / st.yDecl : 0.0,
           double(st.xDecl) / n, double(st.yDecl) / n,
           double(st.xForced) / n, double(st.yForced) / n,
           double(st.events) / n,
           st.arb.rounds, st.arb.races, st.arb.racesDiffSet,
           st.arb.racesDiffConf, st.arb.racesConfDiffers,
           st.arb.contested, st.arb.lowRight, st.arb.confRight, st.arb.ladRight,
           st.arb.bothRight, st.arb.bothWrong, st.arb.confOnly, st.arb.lowOnly,
           st.arb.contested ? st.arb.confGapSum / st.arb.contested : 0.0, floorTh, rungs,
           st.arb.ladRightR[0], st.arb.ladRightR[1], st.arb.ladRightR[2], st.arb.ladRightR[3],
           st.arb.ladRightR[4], st.arb.ladRightR[5], st.arb.ladRightR[6]);
    return 0;
  }

  // ---------------------------------------------------------------- oot
  if (cmd == "oot") {
    int on = atoi(argVal(argc, argv, "on", "1").c_str());
    double ootThX = atof(argVal(argc, argv, "ootth", "0").c_str());
    // --ootth>0: BOTH teams may declare out of turn, but the X team only when
    // its stated confidence clears the gate.
    bool bothOn = ootThX > 0;
    int yOff = atoi(argVal(argc, argv, "yoff", "0").c_str());
    ArbMatch st = runArb(spec, games, seed, rules, 0, 0,
                         bothOn ? true : (on != 0), bothOn ? (yOff == 0) : (on == 0),
                         threads, 0.0, 0, ootThX, 0.0);
    int n = st.deals * 2;
    double m, lo, hi; clusterBootstrap(st.paired, 2, m, lo, hi);
    printf("{\"cmd\":\"oot\",\"spec\":\"%s\",\"ootTh\":%.3f,\"xOOT\":%d,\"deals\":%d,\"games\":%d,"
           "\"xWinRate\":%.6f,\"ci\":[%.6f,%.6f],\"xSets\":%.4f,\"ySets\":%.4f,"
           "\"xDeclAcc\":%.5f,\"yDeclAcc\":%.5f,\"xDeclPerGame\":%.4f,\"yDeclPerGame\":%.4f,"
           "\"xForcedPerGame\":%.4f,\"yForcedPerGame\":%.4f,\"eventsPerGame\":%.2f}\n",
           spec.c_str(), ootThX, on, st.deals, n, double(st.xWins) / n, lo, hi,
           double(st.xSets) / n, double(st.ySets) / n,
           st.xDecl ? double(st.xDeclOk) / st.xDecl : 0.0,
           st.yDecl ? double(st.yDeclOk) / st.yDecl : 0.0,
           double(st.xDecl) / n, double(st.yDecl) / n,
           double(st.xForced) / n, double(st.yForced) / n,
           double(st.events) / n);
    return 0;
  }

  // ---------------------------------------------------------------- timing
  // Backward: exact BlockDP posterior at the declaration and 5/10/20 events
  // earlier.  Forward: the same deal replayed with that half-suit's voluntary
  // declarations suppressed for 21 events, scored at +5/+10/+20.
  if (cmd == "timing") {
    int doForward = atoi(argVal(argc, argv, "forward", "1").c_str());
    int pick = atoi(argVal(argc, argv, "pick", "0").c_str());  // 0 first, 1 last, 2 first wrong
    int nT = threads > 0 ? threads : int(std::thread::hardware_concurrency());
    nT = std::max(1, std::min(nT, std::max(1, games)));
    struct Acc {
      std::vector<DeclRecord> decls;
      std::vector<ForwardRecord> fwd;
      long long buildFail = 0;
      long long ootTotal = 0, ootCorrect = 0, onTotal = 0, onCorrect = 0;
      long long ootLocked = 0, onLocked = 0;
    };
    std::vector<Acc> local(nT);
    std::vector<std::thread> pool;
    for (int t = 0; t < nT; t++) {
      pool.emplace_back([&, t]() {
        std::unique_ptr<Agent> A[NPLAY];
        for (int i = 0; i < NPLAY; i++) A[i] = makeAgent(spec);
        PGame game; game.trace.on = true;
        Acc& acc = local[t];
        for (int i = t; i < games; i += nT) {
          uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++) ag[p] = A[p].get();
          game.arbTeam[0] = game.arbTeam[1] = 0;
          game.teamOOT[0] = game.teamOOT[1] = true;
          game.blockSet = -1; game.blockFromEvent = 0; game.blockUntilEvent = -1;
          game.trace.events.clear();
          game.run(s, rules, ag);
          std::vector<Event> ev = game.trace.events;
          uint64_t dealt[NPLAY];
          for (int p = 0; p < NPLAY; p++) dealt[p] = game.g.dealt[p];
          size_t before = acc.decls.size();
          backwardScan(ev, dealt, rules, s, acc.decls, acc.buildFail);
          for (size_t j = before; j < acc.decls.size(); j++) {
            const DeclRecord& d = acc.decls[j];
            if (d.onTurn) { acc.onTotal++; if (d.correct) acc.onCorrect++; if (d.lockedOwn) acc.onLocked++; }
            else          { acc.ootTotal++; if (d.correct) acc.ootCorrect++; if (d.lockedOwn) acc.ootLocked++; }
          }
          if (!doForward) continue;
          // counterfactual: delay one voluntary declaration by 21 events
          int t0 = -1, actor = -1, st2 = -1;
          for (size_t j = 0; j < ev.size(); j++) if (ev[j].kind == Kind::Declare) {
            if (pick == 2 && ev[j].success) continue;      // first WRONG one
            t0 = int(j); actor = ev[j].actor; st2 = ev[j].set;
            if (pick != 1) break;                          // pick==1: keep the last
          }
          if (pick == 2 && t0 < 0) {
            for (size_t j = 0; j < ev.size(); j++) if (ev[j].kind == Kind::Declare) {
              t0 = int(j); actor = ev[j].actor; st2 = ev[j].set; break; }
          }
          if (t0 < 0) continue;
          game.blockSet = st2; game.blockFromEvent = t0; game.blockUntilEvent = t0 + 21;
          game.trace.events.clear();
          game.run(s, rules, ag);
          ForwardRecord fr;
          forwardScan(game.trace.events, dealt, rules, s, t0, actor, st2, fr, acc.buildFail);
          acc.fwd.push_back(fr);
        }
      });
    }
    for (auto& th : pool) th.join();
    Acc all;
    for (int t = 0; t < nT; t++) {
      all.decls.insert(all.decls.end(), local[t].decls.begin(), local[t].decls.end());
      all.fwd.insert(all.fwd.end(), local[t].fwd.begin(), local[t].fwd.end());
      all.buildFail += local[t].buildFail;
      all.ootTotal += local[t].ootTotal; all.ootCorrect += local[t].ootCorrect;
      all.onTotal += local[t].onTotal; all.onCorrect += local[t].onCorrect;
      all.ootLocked += local[t].ootLocked; all.onLocked += local[t].onLocked;
    }
    const char* offName[4] = {"-20", "-10", "-5", "0"};
    struct Split { const char* name; int kind; };
    const Split splits[] = {
      {"ALL", 0}, {"LOCKED-to-own-team", 1}, {"NOT-locked", 2},
      {"declaration CORRECT", 3}, {"declaration WRONG", 4},
      {"late (event>=220)", 5}, {"early (event<220)", 6},
    };
    auto keep = [](const DeclRecord& d, int kind) {
      switch (kind) {
        case 1: return d.lockedOwn; case 2: return !d.lockedOwn;
        case 3: return d.correct;   case 4: return !d.correct;
        case 5: return d.event >= 220; case 6: return d.event < 220;
      }
      return true;
    };
    printf("=== backward (exact BlockDP posterior of the declarer)\n");
    printf("declarations scanned: %zu   BlockDP build failures: %lld\n", all.decls.size(), all.buildFail);
    for (const auto& sp : splits) {
      long long tot = 0; double conf = 0;
      for (const auto& d : all.decls) if (keep(d, sp.kind)) { tot++; conf += d.statedConf; }
      printf("-- %s   n=%lld  v0.4 stated conf=%.4f\n", sp.name, tot, tot ? conf / tot : 0.0);
      for (int j = 0; j < 4; j++) {
        long long n = 0, argmaxRight = 0; double sp2 = 0, spTeam = 0;
        for (const auto& d : all.decls) {
          if (!keep(d, sp.kind) || !d.have[j]) continue;
          n++; sp2 += d.at[j].pAlloc; spTeam += d.at[j].pTeam;
          if (d.at[j].correct) argmaxRight++;
        }
        if (!n) { printf("   %4s  n=0\n", offName[j]); continue; }
        printf("   %4s  n=%-6lld  exact P(alloc)=%.4f  P(team owns)=%.4f  exact-argmax-correct=%.4f\n",
               offName[j], n, sp2 / n, spTeam / n, double(argmaxRight) / n);
      }
    }
    printf("\n=== on-turn vs out-of-turn voluntary declarations\n");
    printf("on-turn      n=%lld  correct=%.4f  locked-to-own-team=%.4f\n",
           all.onTotal, all.onTotal ? double(all.onCorrect) / all.onTotal : 0.0,
           all.onTotal ? double(all.onLocked) / all.onTotal : 0.0);
    printf("out-of-turn  n=%lld  correct=%.4f  locked-to-own-team=%.4f\n",
           all.ootTotal, all.ootTotal ? double(all.ootCorrect) / all.ootTotal : 0.0,
           all.ootTotal ? double(all.ootLocked) / all.ootTotal : 0.0);
    {
      double sOn = 0, sOot = 0; long long nOn = 0, nOot = 0;
      double pOn = 0, pOot = 0; long long qOn = 0, qOot = 0;
      long long lateOn = 0, lateOot = 0;
      for (const auto& d : all.decls) {
        if (d.onTurn) { sOn += d.statedConf; nOn++; if (d.event >= 220) lateOn++; if (d.have[3]) { pOn += d.at[3].pAlloc; qOn++; } }
        else          { sOot += d.statedConf; nOot++; if (d.event >= 220) lateOot++; if (d.have[3]) { pOot += d.at[3].pAlloc; qOot++; } }
      }
      printf("stated conf  on-turn=%.4f  out-of-turn=%.4f\n", nOn ? sOn / nOn : 0.0, nOot ? sOot / nOot : 0.0);
      printf("exact P(alloc) on-turn=%.4f  out-of-turn=%.4f\n", qOn ? pOn / qOn : 0.0, qOot ? pOot / qOot : 0.0);
      printf("share late (event>=220)  on-turn=%.4f  out-of-turn=%.4f\n",
             nOn ? double(lateOn) / nOn : 0.0, nOot ? double(lateOot) / nOot : 0.0);
    }
    if (doForward) {
      printf("\n=== forward counterfactual (first voluntary declaration delayed 21 events)\n");
      printf("cases: %zu\n", all.fwd.size());
      const char* fn[3] = {"+5", "+10", "+20"};
      for (int split = 0; split < 3; split++) {
        long long base = 0; double sb = 0; long long bRight = 0;
        for (const auto& f : all.fwd) {
          if (split == 1 && !f.lockedOwn) continue;
          if (split == 2 && f.lockedOwn) continue;
          if (!f.at0.ok) continue;
          base++; sb += f.at0.pAlloc; if (f.at0.correct) bRight++;
        }
        printf("-- split %s   n=%lld  P(alloc)@0=%.4f  argmax-correct@0=%.4f\n",
               split == 0 ? "ALL" : split == 1 ? "LOCKED" : "NOT-locked",
               base, base ? sb / base : 0.0, base ? double(bRight) / base : 0.0);
        for (int j = 0; j < 3; j++) {
          long long n = 0, right = 0, trunc = 0; double sp = 0, dp = 0;
          for (const auto& f : all.fwd) {
            if (split == 1 && !f.lockedOwn) continue;
            if (split == 2 && f.lockedOwn) continue;
            if (f.truncated) trunc++;
            if (!f.have[j] || !f.at0.ok) continue;
            n++; sp += f.at[j].pAlloc; dp += f.at[j].pAlloc - f.at0.pAlloc;
            if (f.at[j].correct) right++;
          }
          printf("   %4s  n=%-6lld  meanP(alloc)=%.4f  delta=%+.4f  argmax-correct=%.4f  (truncated %lld)\n",
                 fn[j], n, n ? sp / n : 0.0, n ? dp / n : 0.0, n ? double(right) / n : 0.0, trunc);
        }
      }
    }
    return 0;
  }

  fprintf(stderr, "usage: probe_decl arb|oot|timing [--games=N] [--seed=S] [--spec=...] [--x=M] [--y=M]\n");
  return 2;
}
