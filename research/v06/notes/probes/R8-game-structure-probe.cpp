// R8 recon probe: game-structure measurement.  Read-only w.r.t. engine/src.
// Build:  clang++ -std=c++20 -O3 -march=native -I<engine/src> r8.cpp -o r8 -pthread
#include "factory.hpp"
#include "game.hpp"
#include "oracle.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

using namespace fish;
using clk = std::chrono::steady_clock;
static double secs(clk::time_point a, clk::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}

// ---------------------------------------------------------------- collectors
struct Row {
  int ev;            // pub.nEvents at the decision
  int active;        // active half-suits
  int unresolved;    // |U| for the deciding seat
  double log10Z;     // exact information-set mass (BlockDP)
  int nLegal;        // enumerateAsks
  int nLive;         // after M1 provably-dead filter
  int nLiveCards;    // distinct cards among live asks
  int myHand;
  int cardsInPlay;
  double dpMs;       // BlockDP build time
  int game;          // game index
  int evLeft;        // public events remaining after this decision (filled post-hoc)
  int asksLeft;      // ask events remaining
};

struct Bag {
  std::vector<Row> rows;
};

static bool provablyDead(const Knowledge& k, int card, int target) {
  return k.owner[card] < NPLAY ? k.owner[card] != target
                               : !(k.mask[card] & (1u << target));
}

struct ProbeAgent : V05Agent {
  Bag* bag = nullptr;
  bool doZ = true;
  int gameIdx = 0;
  void record(const PublicState& pub) {
    Row r{};
    r.ev = pub.nEvents;
    r.active = pub.activeSets();
    r.unresolved = __builtin_popcountll(k.unresolved);
    r.myHand = pub.handCount[seat];
    r.cardsInPlay = 0;
    for (int p = 0; p < NPLAY; p++) r.cardsInPlay += pub.handCount[p];
    AskMove buf[NSET * SETSZ * 3];
    int n = enumerateAsks(pub, k.myHand, seat, buf);
    r.nLegal = n;
    int m = 0; uint64_t cards = 0;
    for (int i = 0; i < n; i++)
      if (!::provablyDead(k, buf[i].card, buf[i].target)) { m++; cards |= bit(buf[i].card); }
    r.nLive = m;
    r.nLiveCards = __builtin_popcountll(cards);
    r.log10Z = -1; r.dpMs = -1;
    if (doZ) {
      BlockDP bd;
      auto t0 = clk::now();
      bool ok = bd.build(k);
      auto t1 = clk::now();
      r.dpMs = secs(t0, t1) * 1e3;
      r.log10Z = ok ? std::log10(bd.Z) : -2;
    }
    r.game = gameIdx;
    bag->rows.push_back(r);
  }
  AskMove chooseAsk(const PublicState& pub) override {
    if (bag) record(pub);
    return V05Agent::chooseAsk(pub);
  }
};

// ------------------------------------------------------------------ helpers
static double quant(std::vector<double> v, double q) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  double x = q * (v.size() - 1);
  size_t i = size_t(x);
  double fr = x - i;
  return i + 1 < v.size() ? v[i] * (1 - fr) + v[i + 1] * fr : v[i];
}

// ---------------------------------------------------- MODE 1: state + actions
static void modeState(int games, uint64_t seed, bool doZ) {
  Bag bag;
  std::vector<ProbeAgent> ag(NPLAY);
  Agent* ptr[NPLAY];
  for (int p = 0; p < NPLAY; p++) { ag[p].bag = &bag; ag[p].doZ = doZ; ptr[p] = &ag[p]; }
  Rules r;
  Game game;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    game.rotation = i % NPLAY;
    for (int p = 0; p < NPLAY; p++) ag[p].gameIdx = i;
    size_t from = bag.rows.size();
    GameResult gr = game.run(s, r, ptr);
    int totalEv = gr.events, totalAsk = gr.asks;
    (void)totalAsk;
    for (size_t j = from; j < bag.rows.size(); j++) {
      bag.rows[j].evLeft = totalEv - bag.rows[j].ev;
      bag.rows[j].asksLeft = 0;
    }
  }
  printf("# mode=state games=%d seed=%llu rows=%zu\n", games, (unsigned long long)seed, bag.rows.size());
  // --- bucketed by event index
  printf("evbucket n meanLog10Z medLog10Z meanUnres meanLegal medLegal p10Legal p90Legal meanLive medLive fracLiveEq1 fracDeadFrac meanActive\n");
  const int B[] = {0, 1, 5, 10, 20, 30, 40, 60, 80, 120, 1000000};
  for (int b = 0; b + 1 < int(sizeof(B) / sizeof(B[0])); b++) {
    std::vector<double> z, leg, liv, un, act;
    long long deadPairs = 0, totPairs = 0, liveEq1 = 0;
    for (const auto& x : bag.rows) {
      if (x.ev < B[b] || x.ev >= B[b + 1]) continue;
      if (x.log10Z >= 0) z.push_back(x.log10Z);
      leg.push_back(x.nLegal); liv.push_back(x.nLive); un.push_back(x.unresolved); act.push_back(x.active);
      deadPairs += x.nLegal - x.nLive; totPairs += x.nLegal;
      if (x.nLive == 1) liveEq1++;
    }
    if (leg.empty()) continue;
    double mz = 0; for (double v : z) mz += v; if (!z.empty()) mz /= z.size();
    double ml = 0; for (double v : leg) ml += v; ml /= leg.size();
    double mv = 0; for (double v : liv) mv += v; mv /= liv.size();
    double mu = 0; for (double v : un) mu += v; mu /= un.size();
    double ma = 0; for (double v : act) ma += v; ma /= act.size();
    printf("%d-%d %zu %.3f %.3f %.2f %.2f %.0f %.0f %.0f %.2f %.0f %.4f %.4f %.2f\n",
           B[b], B[b + 1] - 1, leg.size(), mz, quant(z, .5), mu,
           ml, quant(leg, .5), quant(leg, .1), quant(leg, .9),
           mv, quant(liv, .5), double(liveEq1) / leg.size(),
           totPairs ? double(deadPairs) / totPairs : 0.0, ma);
  }
  // --- bucketed by unresolved count (the endgame axis)
  printf("\nunres n meanLog10Z medLog10Z p90Log10Z minLog10Z fracZle1 fracZle10 fracZle1e3 fracZle1e6 meanLegal meanLive meanDpMs p99DpMs medEvLeft meanEvLeft medEv\n");
  std::map<int, std::vector<Row>> byU;
  for (const auto& x : bag.rows) byU[x.unresolved].push_back(x);
  for (auto& kv : byU) {
    std::vector<double> z, dp, leg, liv, evl, evi;
    long long le1 = 0, le10 = 0, le3 = 0, le6 = 0;
    for (auto& x : kv.second) {
      evl.push_back(x.evLeft); evi.push_back(x.ev);
      if (x.log10Z >= -1e-9) { z.push_back(x.log10Z);
        if (x.log10Z < 1e-9) le1++;
        if (x.log10Z <= 1.0 + 1e-9) le10++;
        if (x.log10Z <= 3.0) le3++;
        if (x.log10Z <= 6.0) le6++; }
      if (x.dpMs >= 0) dp.push_back(x.dpMs);
      leg.push_back(x.nLegal); liv.push_back(x.nLive);
    }
    double mz = 0; for (double v : z) mz += v; if (!z.empty()) mz /= z.size();
    double ml = 0; for (double v : leg) ml += v; ml /= leg.size();
    double mv = 0; for (double v : liv) mv += v; mv /= liv.size();
    double md = 0; for (double v : dp) md += v; if (!dp.empty()) md /= dp.size();
    size_t nz = z.size() ? z.size() : 1;
    double me = 0; for (double v : evl) me += v; me /= evl.size();
    printf("%d %zu %.3f %.3f %.3f %.3f %.4f %.4f %.4f %.4f %.2f %.2f %.4f %.4f %.1f %.1f %.1f\n",
           kv.first, kv.second.size(), mz, quant(z, .5), quant(z, .9),
           z.empty() ? 0.0 : *std::min_element(z.begin(), z.end()),
           double(le1) / nz, double(le10) / nz, double(le3) / nz, double(le6) / nz,
           ml, mv, md, quant(dp, .99), quant(evl, .5), me, quant(evi, .5));
  }
  // --- overall action-set distribution
  {
    std::vector<double> leg, liv;
    long long histL[64] = {0}; long long histV[64] = {0};
    for (const auto& x : bag.rows) {
      leg.push_back(x.nLegal); liv.push_back(x.nLive);
      histL[std::min(63, x.nLegal)]++; histV[std::min(63, x.nLive)]++;
    }
    double ml = 0, mv = 0;
    for (double v : leg) ml += v; ml /= leg.size();
    for (double v : liv) mv += v; mv /= liv.size();
    printf("\nOVERALL decisions=%zu meanLegal=%.3f medLegal=%.0f p05=%.0f p95=%.0f maxLegal=%.0f"
           " meanLive=%.3f medLive=%.0f p05Live=%.0f p95Live=%.0f\n",
           leg.size(), ml, quant(leg, .5), quant(leg, .05), quant(leg, .95),
           *std::max_element(leg.begin(), leg.end()),
           mv, quant(liv, .5), quant(liv, .05), quant(liv, .95));
    printf("HISTLEGAL");
    for (int i = 0; i < 64; i++) if (histL[i]) printf(" %d:%lld", i, histL[i]);
    printf("\nHISTLIVE");
    for (int i = 0; i < 64; i++) if (histV[i]) printf(" %d:%lld", i, histV[i]);
    printf("\n");
  }
  {
    printf("\nSOLVABILITY: fraction of ask decisions at or below an unresolved-card budget\n");
    printf("Qmax fracDecisions medEvLeft meanLive medLog10Z\n");
    for (int qm : {0, 2, 4, 6, 8, 10, 12, 13, 14, 16, 18, 20}) {
      long long c = 0; std::vector<double> evl, liv, z;
      for (const auto& x : bag.rows) if (x.unresolved <= qm) {
        c++; evl.push_back(x.evLeft); liv.push_back(x.nLive);
        if (x.log10Z >= 0) z.push_back(x.log10Z);
      }
      if (!c) continue;
      double mv = 0; for (double v : liv) mv += v; mv /= liv.size();
      printf("%d %.4f %.1f %.2f %.3f\n", qm, double(c) / bag.rows.size(), quant(evl, .5), mv, quant(z, .5));
    }
  }
}

// -------------------------------------------- MODE 2: endgame enumeration cost
// Collect real game states, and for those small enough, exhaustively enumerate
// the consistent deals and time it.
struct EndRow { int unres; long long Z; double ms; bool complete; int nLive; long long nodes; };

// Independent exhaustive enumerator.  Same semantics as BruteForce::enumerate
// (assign every unresolved card, prune on capacity, test C5 at the leaf) but
// without oracle.hpp's crude `prod |mask| <= maxNodes` pre-filter, which refuses
// exactly the states we want to time, and with stack[0] always initialised
// (oracle.hpp:97-117 reads it uninitialised when nU == 0).
struct Enum {
  long long Z = 0, nodes = 0;
  bool enumerate(const Knowledge& k, long long maxNodes) {
    Z = 0; nodes = 0;
    int nU = 0, cards[NCARD]; uint8_t cmask[NCARD], cap[NPLAY];
    uint64_t u = k.unresolved;
    while (u) { int c = __builtin_ctzll(u); u &= u - 1; cards[nU] = c; cmask[nU] = k.mask[c]; nU++; }
    k.capacities(cap);
    int sum = 0; for (int p = 0; p < NPLAY; p++) sum += cap[p];
    if (sum != nU || nU == 0) return false;
    int assign[NCARD], cnt[NPLAY] = {0,0,0,0,0,0}, stack[NCARD];
    for (int i = 0; i < NCARD; i++) stack[i] = -1;
    uint8_t owners[NCARD];
    for (int c = 0; c < NCARD; c++) owners[c] = k.owner[c];
    int depth = 0;
    while (depth >= 0) {
      if (++nodes > maxNodes) return false;
      int nxt = stack[depth] + 1;
      if (stack[depth] >= 0) cnt[stack[depth]]--;
      bool placed = false;
      for (; nxt < NPLAY; nxt++) {
        if (!(cmask[depth] & (1u << nxt))) continue;
        if (cnt[nxt] + 1 > cap[nxt]) continue;
        stack[depth] = nxt; assign[depth] = nxt; cnt[nxt]++; placed = true; break;
      }
      if (!placed) { stack[depth] = -1; depth--; continue; }
      if (depth + 1 < nU) { depth++; stack[depth] = -1; continue; }
      for (int i = 0; i < nU; i++) owners[cards[i]] = uint8_t(assign[i]);
      if (Belief::satisfies(k, owners)) Z++;
    }
    return true;
  }
};

struct EndAgent : V05Agent {
  std::vector<EndRow>* out = nullptr;
  long long maxNodes = 0;
  int cap = 20;
  int* budget = nullptr;               // shared per-Q sampling budget
  AskMove chooseAsk(const PublicState& pub) override {
    int Q = __builtin_popcountll(k.unresolved);
    if (out && Q >= 2 && Q <= cap && budget && budget[Q] > 0) {
      budget[Q]--;
      Enum bf;
      auto t0 = clk::now();
      bool ok = bf.enumerate(k, maxNodes);
      auto t1 = clk::now();
      EndRow r{Q, ok ? bf.Z : -1, secs(t0, t1) * 1e3, ok, 0, bf.nodes};
      AskMove buf[NSET * SETSZ * 3];
      int n = enumerateAsks(pub, k.myHand, seat, buf);
      int m = 0;
      for (int i = 0; i < n; i++) if (!::provablyDead(k, buf[i].card, buf[i].target)) m++;
      r.nLive = m;
      out->push_back(r);
    }
    return V05Agent::chooseAsk(pub);
  }
};

static void modeEndgame(int games, uint64_t seed, long long maxNodes) {
  std::vector<EndRow> rows;
  static int budget[64];
  for (int q = 0; q < 64; q++) budget[q] = q <= 12 ? 400 : (q <= 15 ? 120 : (q <= 17 ? 40 : 12));
  std::vector<EndAgent> ag(NPLAY);
  Agent* ptr[NPLAY];
  for (int p = 0; p < NPLAY; p++) { ag[p].out = &rows; ag[p].maxNodes = maxNodes; ag[p].budget = budget; ptr[p] = &ag[p]; }
  Rules r; Game game;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    game.rotation = i % NPLAY;
    game.run(s, r, ptr);
  }
  printf("# mode=endgame games=%d seed=%llu maxNodes=%lld rows=%zu\n",
         games, (unsigned long long)seed, maxNodes, rows.size());
  printf("unres n complete medZ meanZ p90Z maxZ medMs meanMs p90Ms p99Ms dealsPerSec meanLive nodesPerDeal\n");
  std::map<int, std::vector<EndRow>> byU;
  for (auto& x : rows) byU[x.unres].push_back(x);
  for (auto& kv : byU) {
    std::vector<double> zs, ms, lv;
    long long comp = 0; double totZ = 0, totMs = 0, totNodes = 0;
    for (auto& x : kv.second) {
      lv.push_back(x.nLive);
      if (!x.complete) continue;
      comp++; zs.push_back(double(x.Z)); ms.push_back(x.ms);
      totZ += double(x.Z); totMs += x.ms; totNodes += double(x.nodes);
    }
    if (zs.empty()) { printf("%d %zu 0 - - - - - - - - -\n", kv.first, kv.second.size()); continue; }
    double meanZ = 0; for (double v : zs) meanZ += v; meanZ /= zs.size();
    double meanMs = 0; for (double v : ms) meanMs += v; meanMs /= ms.size();
    double mlv = 0; for (double v : lv) mlv += v; mlv /= lv.size();
    printf("%d %zu %lld %.1f %.1f %.1f %.0f %.5f %.5f %.5f %.5f %.0f %.2f %.2f\n",
           kv.first, kv.second.size(), comp, quant(zs, .5), meanZ, quant(zs, .9),
           *std::max_element(zs.begin(), zs.end()),
           quant(ms, .5), meanMs, quant(ms, .9), quant(ms, .99),
           totMs > 0 ? totZ / (totMs / 1e3) : 0.0, mlv,
           totZ > 0 ? totNodes / totZ : 0.0);
  }
}

// ------------------------------------------- MODE 3: turn value / lead effect
struct GameRec {
  uint64_t seed; int rot; int leadGroupEven; int scoreEven; int scoreOdd;
  int turnsEven, turnsOdd; int events;
};

static void modeTurn(int games, uint64_t seed, const std::string& spec) {
  std::vector<std::unique_ptr<Agent>> ags;
  for (int p = 0; p < NPLAY; p++) ags.push_back(makeAgent(spec));
  Agent* ptr[NPLAY];
  for (int p = 0; p < NPLAY; p++) ptr[p] = ags[p].get();
  Rules r; Game game;
  // opening-lead natural experiment: with rotation rot, dealt hand h sits at
  // seat (h-rot) mod 6, so hand-group {0,2,4} is on team (rot mod 2).  The
  // leading seat is fixed by the seed, so across rot the SAME hand partition
  // leads half the time.
  double sumLead = 0, sumNoLead = 0; long long nLead = 0, nNo = 0;
  double sumLead2 = 0, sumNo2 = 0;
  // turnsRetained regression
  double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0; long long n = 0;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < NPLAY; rot++) {
      game.rotation = rot;
      GameResult res = game.run(s, r, ptr);
      int evenTeam = rot % 2;                 // hands {0,2,4} sit on this team
      int leadTeam = teamOf(game.g.turn);     // NOTE: turn at end; recompute below
      (void)leadTeam;
      // recover the opening seat from the deal (dealCards is deterministic)
      GameState tmp{}; dealCards(tmp, s, r.deckSets);
      int openSeat = tmp.turn;
      int openTeam = teamOf(openSeat);
      int scoreEven = res.score[evenTeam], scoreOdd = res.score[1 - evenTeam];
      bool evenLeads = (openTeam == evenTeam);
      double d = double(scoreEven) - double(scoreOdd);
      if (evenLeads) { sumLead += d; sumLead2 += d * d; nLead++; }
      else           { sumNoLead += d; sumNo2 += d * d; nNo++; }
      double x = double(res.turnsRetained[0]) - double(res.turnsRetained[1]);
      double y = double(res.score[0]) - double(res.score[1]);
      sx += x; sy += y; sxx += x * x; syy += y * y; sxy += x * y; n++;
    }
  }
  double mL = sumLead / nLead, mN = sumNoLead / nNo;
  double vL = sumLead2 / nLead - mL * mL, vN = sumNo2 / nNo - mN * mN;
  double se = std::sqrt(vL / nLead + vN / nNo);
  printf("# mode=turn spec=%s games=%d (x6 rotations) seed=%llu\n", spec.c_str(), games, (unsigned long long)seed);
  printf("OPENLEAD nLead=%lld meanDiff=%.4f | nNoLead=%lld meanDiff=%.4f | effect=%.4f halfsuits SE=%.4f z=%.2f\n",
         nLead, mL, nNo, mN, mL - mN, se, (mL - mN) / se);
  double cov = sxy / n - (sx / n) * (sy / n);
  double vx = sxx / n - (sx / n) * (sx / n), vy = syy / n - (sy / n) * (sy / n);
  printf("TURNSRET n=%lld r=%.4f slope=%.5f (halfsuits per net retained turn) sd(x)=%.2f sd(y)=%.2f\n",
         n, cov / std::sqrt(vx * vy), cov / vx, std::sqrt(vx), std::sqrt(vy));
}

// --------------------------------------- MODE 4: deal determinism / attribution
struct DealAgent : V05Agent {};

static void modeDeal(int games, uint64_t seed) {
  std::vector<DealAgent> ag(NPLAY);
  Agent* ptr[NPLAY];
  for (int p = 0; p < NPLAY; p++) ptr[p] = &ag[p];
  Rules r; Game game;
  // Per half-suit: dealt split -> which team won it.
  long long splitN[7] = {0}, splitWin[7] = {0};   // index = team0 count at deal
  long long majorityCorrect = 0, majorityTot = 0;
  // regression of final diff on a deal-only score
  std::vector<double> X, Y;
  std::vector<std::array<double, 7>> NK;   // counts of half-suits by team-0 share
  std::vector<int> C0;                      // per-set team-0 share, for the p-map
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    for (int rot = 0; rot < NPLAY; rot++) {
      game.rotation = rot;
      GameResult res = game.run(s, r, ptr);
      uint64_t t0 = game.g.dealt[0] | game.g.dealt[2] | game.g.dealt[4];
      double dealScore = 0;
      std::array<double, 7> nk{};
      for (int st = 0; st < NSET; st++) {
        int c0 = popcount64(t0 & setMask(st));
        splitN[c0]++;
        nk[c0] += 1.0;
        int w = game.g.setWinner[st];
        if (w == 0) splitWin[c0]++;
        if (c0 != 3) { majorityTot++; if ((c0 > 3) == (w == 0)) majorityCorrect++; }
        dealScore += (c0 >= 4 ? 1.0 : (c0 == 3 ? 0.5 : 0.0));
      }
      NK.push_back(nk);
      X.push_back(dealScore);
      Y.push_back(double(res.score[0]) - double(res.score[1]));
    }
  }
  printf("# mode=deal games=%d (x6 rotations) seed=%llu\n", games, (unsigned long long)seed);
  printf("split(team0 cards at deal) n P(team0 wins set)\n");
  for (int c = 0; c <= 6; c++) if (splitN[c])
    printf("%d %lld %.4f\n", c, splitN[c], double(splitWin[c]) / splitN[c]);
  printf("MAJORITY-AT-DEAL predicts winner: %lld/%lld = %.4f\n",
         majorityCorrect, majorityTot, double(majorityCorrect) / majorityTot);
  double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0; size_t n = X.size();
  for (size_t i = 0; i < n; i++) { sx += X[i]; sy += Y[i]; sxx += X[i] * X[i]; syy += Y[i] * Y[i]; sxy += X[i] * Y[i]; }
  double cov = sxy / n - (sx / n) * (sy / n);
  double vx = sxx / n - (sx / n) * (sx / n), vy = syy / n - (sy / n) * (sy / n);
  double rr = cov / std::sqrt(vx * vy);
  printf("DEALSCORE->finaldiff n=%zu r=%.4f R2=%.4f slope=%.4f sd(final)=%.4f residSD=%.4f sd(dealscore)=%.4f\n",
         n, rr, rr * rr, cov / vx, std::sqrt(vy), std::sqrt(vy * (1 - rr * rr)), std::sqrt(vx));
  // conservation check: the deal-only expected score, using the empirical
  // P(team 0 wins | team-0 share) map above.
  {
    double pmap[7];
    for (int c = 0; c <= 6; c++) pmap[c] = splitN[c] ? double(splitWin[c]) / splitN[c] : 0.5;
    std::vector<double> P;
    double sp = 0, sp2 = 0, spy = 0;
    for (size_t i = 0; i < n; i++) {
      double e = 0;
      for (int c = 0; c <= 6; c++) e += NK[i][c] * pmap[c];
      double pred = 2 * e - 9;              // expected (score0 - score1)
      P.push_back(pred); sp += pred; sp2 += pred * pred; spy += pred * Y[i];
    }
    double mp = sp / n, vp = sp2 / n - mp * mp;
    double cvy = spy / n - mp * (sy / n);
    double rp = cvy / std::sqrt(vp * vy);
    printf("DEALPRIOR (sum of empirical P(win|split)) mean=%.4f sd=%.4f  r=%.4f R2=%.4f\n",
           mp, std::sqrt(vp), rp, rp * rp);
  }
  // full 7-feature ridge regression of the final differential on the deal's
  // half-suit split profile: an upper bound on the deal's explanatory power.
  {
    const int D = 8;                        // 7 counts + intercept
    double A[8][8] = {}, b[8] = {};
    for (size_t i = 0; i < n; i++) {
      double x[8];
      for (int c = 0; c < 7; c++) x[c] = NK[i][c];
      x[7] = 1.0;
      for (int a = 0; a < D; a++) { for (int c = 0; c < D; c++) A[a][c] += x[a] * x[c]; b[a] += x[a] * Y[i]; }
    }
    for (int a = 0; a < D; a++) A[a][a] += 1e-6 * n;
    // Gaussian elimination
    double M[8][9];
    for (int a = 0; a < D; a++) { for (int c = 0; c < D; c++) M[a][c] = A[a][c]; M[a][D] = b[a]; }
    for (int col = 0; col < D; col++) {
      int piv = col;
      for (int a = col; a < D; a++) if (std::fabs(M[a][col]) > std::fabs(M[piv][col])) piv = a;
      for (int c = 0; c <= D; c++) std::swap(M[col][c], M[piv][c]);
      double d = M[col][col];
      if (std::fabs(d) < 1e-12) continue;
      for (int c = 0; c <= D; c++) M[col][c] /= d;
      for (int a = 0; a < D; a++) if (a != col) {
        double f = M[a][col];
        for (int c = 0; c <= D; c++) M[a][c] -= f * M[col][c];
      }
    }
    double w[8]; for (int a = 0; a < D; a++) w[a] = M[a][D];
    double sse = 0, my = sy / n, sst = 0;
    for (size_t i = 0; i < n; i++) {
      double x[8];
      for (int c = 0; c < 7; c++) x[c] = NK[i][c];
      x[7] = 1.0;
      double yh = 0; for (int a = 0; a < D; a++) yh += w[a] * x[a];
      sse += (Y[i] - yh) * (Y[i] - yh); sst += (Y[i] - my) * (Y[i] - my);
    }
    printf("DEALPROFILE ridge on split counts n0..n6: in-sample R2=%.4f residSD=%.4f (sd(final)=%.4f)\n",
           1 - sse / sst, std::sqrt(sse / n), std::sqrt(vy));
  }
}

// -------------------------------------- MODE 5: coin-flip ask matched contrast
struct FlipAgent : V05Agent {
  struct Obs { double p; int actorTeam; int hit; int evIdx; };
  std::vector<Obs>* out = nullptr;
  int pendingIdx = -1;
};

static void modeFlip(int games, uint64_t seed) {
  // We need to pair each ask's forecast with the realised hit and the final
  // score.  Use the Trace, plus lastAskForecast captured through an observer.
  struct Rec { double p; int team; int hit; int ev; };
  std::vector<Rec> recs;
  std::vector<double> finals;      // final diff (team0 - team1) per game
  std::vector<int> recGame;
  std::vector<std::unique_ptr<Agent>> ags;
  for (int p = 0; p < NPLAY; p++) ags.push_back(makeAgent("v05"));
  Agent* ptr[NPLAY];
  for (int p = 0; p < NPLAY; p++) ptr[p] = ags[p].get();
  Rules r; Game game;
  for (int i = 0; i < games; i++) {
    for (int rot = 0; rot < NPLAY; rot++) {
      uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
      game.rotation = rot;
      int gi = int(finals.size());
      game.observer = [&](const Game& gg) {
        const Event& e = gg.g.pub.history.back();
        if (e.kind != Kind::Ask) return;
        double p = gg.agents[e.actor]->lastAskForecast();
        if (p < 0) return;
        recs.push_back(Rec{p, teamOf(e.actor), e.success ? 1 : 0, gg.g.pub.nEvents});
        recGame.push_back(gi);
      };
      GameResult res = game.run(s, r, ptr);
      finals.push_back(double(res.score[0]) - double(res.score[1]));
    }
  }
  game.observer = nullptr;
  printf("# mode=flip games=%d (x6) seed=%llu asks=%zu\n", games, (unsigned long long)seed, recs.size());
  struct Bin { double lo, hi; };
  Bin bins[] = {{0.30, 0.70}, {0.40, 0.60}, {0.45, 0.55}, {0.20, 0.80}};
  for (auto& b : bins) {
    double sh = 0, sm = 0, sh2 = 0, sm2 = 0; long long nh = 0, nm = 0;
    for (size_t i = 0; i < recs.size(); i++) {
      if (recs[i].p < b.lo || recs[i].p > b.hi) continue;
      double y = finals[recGame[i]];
      if (recs[i].team == 1) y = -y;    // from the asker's team's perspective
      if (recs[i].hit) { sh += y; sh2 += y * y; nh++; } else { sm += y; sm2 += y * y; nm++; }
    }
    if (!nh || !nm) continue;
    double mh = sh / nh, mm = sm / nm;
    double vh = sh2 / nh - mh * mh, vm = sm2 / nm - mm * mm;
    double se = std::sqrt(vh / nh + vm / nm);
    printf("p in [%.2f,%.2f]  hit n=%lld mean=%.4f | miss n=%lld mean=%.4f | delta=%.4f SE=%.4f z=%.1f\n",
           b.lo, b.hi, nh, mh, nm, mm, mh - mm, se, (mh - mm) / se);
  }
  // phase split of the near-coin-flip contrast
  {
    const int PB[] = {0, 20, 40, 60, 80, 1000000};
    printf("PHASE (p in [0.40,0.60])\n");
    for (int q = 0; q + 1 < int(sizeof(PB) / sizeof(PB[0])); q++) {
      double sh = 0, sm = 0, sh2 = 0, sm2 = 0; long long nh = 0, nm = 0;
      for (size_t i = 0; i < recs.size(); i++) {
        if (recs[i].p < 0.40 || recs[i].p > 0.60) continue;
        if (recs[i].ev < PB[q] || recs[i].ev >= PB[q + 1]) continue;
        double y = finals[recGame[i]];
        if (recs[i].team == 1) y = -y;
        if (recs[i].hit) { sh += y; sh2 += y * y; nh++; } else { sm += y; sm2 += y * y; nm++; }
      }
      if (nh < 30 || nm < 30) continue;
      double mh = sh / nh, mm = sm / nm;
      double vh = sh2 / nh - mh * mh, vm = sm2 / nm - mm * mm;
      double se = std::sqrt(vh / nh + vm / nm);
      printf("  ev %d-%d hit n=%lld mean=%.4f | miss n=%lld mean=%.4f | delta=%.4f SE=%.4f z=%.1f\n",
             PB[q], PB[q + 1] - 1, nh, mh, nm, mm, mh - mm, se, (mh - mm) / se);
    }
  }
  // calibration sanity: realised hit rate vs forecast in each bin
  for (auto& b : bins) {
    long long n = 0, h = 0; double sp = 0;
    for (auto& rr : recs) if (rr.p >= b.lo && rr.p <= b.hi) { n++; h += rr.hit; sp += rr.p; }
    if (n) printf("CALIB [%.2f,%.2f] n=%lld meanP=%.4f realised=%.4f\n", b.lo, b.hi, n, sp / n, double(h) / n);
  }
}

// ------------------------------------- MODE 6: structural-fact census
// How often is a half-suit provably locked (all six on one team) and how long
// does it stay locked before being cashed?  How often is an opponent void in a
// half-suit in which they still have the right to ask?
struct CensusAgent : V05Agent {};

static void modeCensus(int games, uint64_t seed) {
  std::vector<CensusAgent> ag(NPLAY);
  Agent* ptr[NPLAY];
  for (int p = 0; p < NPLAY; p++) ptr[p] = &ag[p];
  Rules r; Game game;
  long long decisions = 0;
  long long oppRightSum = 0, oppRightMax = 0;      // # opponents with the right to ask in some set
  long long voidOpps = 0, oppSetPairs = 0, voidPairsProvable = 0;
  long long lockedAtDeal = 0, setsTot = 0;
  long long lockObserved = 0, lockEvents = 0;
  long long emptyLastCardAsks = 0, asksTot = 0, hitsTot = 0;
  long long declTot = 0, declCardless = 0, declNoCardOfSet = 0, declForced = 0, declCorrect = 0;
  long long stripLastOfSet = 0;                    // asks that would void the target of the set
  long long stripLastOfSetHit = 0;
  for (int i = 0; i < games; i++) {
    uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
    game.rotation = i % NPLAY;
    game.trace.on = true; game.trace.events.clear();
    GameResult res = game.run(s, r, ptr);
    (void)res;
    // deal-time locks
    uint64_t t0 = game.g.dealt[0] | game.g.dealt[2] | game.g.dealt[4];
    uint64_t t1 = game.g.dealt[1] | game.g.dealt[3] | game.g.dealt[5];
    for (int st = 0; st < NSET; st++) {
      setsTot++;
      uint64_t m = setMask(st);
      if ((t0 & m) == m || (t1 & m) == m) lockedAtDeal++;
    }
    // replay the trace against ground truth to count void/strip situations
    uint64_t hand[NPLAY];
    for (int p = 0; p < NPLAY; p++) hand[p] = game.g.dealt[p];
    bool active[NSET]; for (int st = 0; st < NSET; st++) active[st] = true;
    for (const auto& e : game.trace.events) {
      if (e.kind == Kind::Ask) {
        asksTot++;
        if (e.success) hitsTot++;
        int S = setOf(e.card);
        // does the target hold exactly one card of S (so a hit voids them)?
        if (popcount64(hand[e.target] & setMask(S)) == 1 && (hand[e.target] & bit(e.card))) {
          stripLastOfSet++;
          if (e.success) stripLastOfSetHit++;
        }
        if (popcount64(hand[e.target]) == 1) emptyLastCardAsks++;
        // census of ask rights
        int actor = e.actor;
        for (int q = 0; q < NPLAY; q++) {
          if (teamOf(q) == teamOf(actor)) continue;
          if (!popcount64(hand[q])) continue;
          for (int st = 0; st < NSET; st++) {
            if (!active[st]) continue;
            oppSetPairs++;
            if (!(hand[q] & setMask(st))) voidOpps++;
          }
        }
        decisions++;
        if (e.success) { hand[e.target] &= ~bit(e.card); hand[e.actor] |= bit(e.card); }
      } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
        declTot++;
        if (!popcount64(hand[e.actor])) declCardless++;
        if (!(hand[e.actor] & setMask(e.set))) declNoCardOfSet++;
        if (e.kind == Kind::ForcedDeclare) declForced++;
        if (e.success) declCorrect++;
        active[e.set] = false;
        for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
      }
    }
    lockObserved += res.lockedDeclarations[0] + res.lockedDeclarations[1];
    lockEvents += res.lockHeldEvents[0] + res.lockHeldEvents[1];
  }
  (void)oppRightSum; (void)oppRightMax; (void)voidPairsProvable;
  printf("# mode=census games=%d seed=%llu\n", games, (unsigned long long)seed);
  printf("halfsuits locked AT THE DEAL (all 6 on one team): %lld/%lld = %.4f\n",
         lockedAtDeal, setsTot, double(lockedAtDeal) / setsTot);
  printf("(opponent,active-set) pairs at ask time where the opponent is VOID (cannot ask there): %lld/%lld = %.4f\n",
         voidOpps, oppSetPairs, double(voidOpps) / oppSetPairs);
  printf("asks (all %lld, hits %lld = %.4f) that strip the target's LAST card of the asked half-suit: %lld = %.4f of asks, %.4f of hits\n",
         asksTot, hitsTot, double(hitsTot)/asksTot, stripLastOfSet, double(stripLastOfSet) / asksTot, double(stripLastOfSetHit)/hitsTot);
  printf("asks whose target holds exactly one card in total: %lld/%lld = %.4f\n",
         emptyLastCardAsks, asksTot, double(emptyLastCardAsks) / asksTot);
  printf("locked declarations %lld, total lock-held events %lld (mean %.2f events held)\n",
         lockObserved, lockEvents, lockObserved ? double(lockEvents) / lockObserved : 0.0);
  printf("declarations %lld (correct %.4f, forced %lld): by a CARDLESS seat %lld = %.4f ; by a seat holding NO card of the declared set %lld = %.4f\n",
         declTot, double(declCorrect)/declTot, declForced, declCardless, double(declCardless)/declTot,
         declNoCardOfSet, double(declNoCardOfSet)/declTot);
  // pure combinatorial check of the deal-time lock rate, no play involved
  {
    Rng rr2(0xBADC0FFEull);
    long long tot = 0, lk = 0;
    for (int i = 0; i < 200000; i++) {
      GameState gs{};
      dealCards(gs, rr2.next(), 9);
      uint64_t a = gs.hand[0] | gs.hand[2] | gs.hand[4];
      uint64_t b2 = gs.hand[1] | gs.hand[3] | gs.hand[5];
      for (int st = 0; st < NSET; st++) {
        uint64_t m = setMask(st); tot++;
        if ((a & m) == m || (b2 & m) == m) lk++;
      }
    }
    printf("COMBINATORIAL deal-time lock rate over %lld half-suits: %.5f (analytic 2*C(27,6)/C(54,6) = %.5f)\n",
           tot, double(lk) / tot, 2.0 * 296010.0 / 25827165.0);
  }
}


// ------------------------------------------- MODE 7: perfect-information value
// An omniscient agent (reads ground truth, which no real policy may do).  Used
// only to measure the perfect-information value of the game, i.e. the size of
// the information gap and the discriminating power a determinized (PIMC) search
// could ever have.
struct OmniAgent : Agent {
  const GameState* truth = nullptr;
  const char* name() const override { return "omni"; }
  int myTeam() const { return teamOf(seat); }
  bool teamOwns(int s, int* owners) const {
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i), h = -1;
      for (int p = 0; p < NPLAY; p++) if (truth->hand[p] & bit(c)) h = p;
      if (h < 0 || teamOf(h) != myTeam()) return false;
      owners[i] = h;
    }
    return true;
  }
  AskMove chooseAsk(const PublicState& pub) override {
    uint64_t mine = truth->hand[seat];
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s] || !(mine & setMask(s))) continue;
      for (int i = 0; i < SETSZ; i++) {
        int c = cardOf(s, i);
        if (mine & bit(c)) continue;
        for (int p = 0; p < NPLAY; p++)
          if (teamOf(p) != myTeam() && pub.handCount[p] && (truth->hand[p] & bit(c)))
            return AskMove{uint8_t(c), uint8_t(p)};
      }
    }
    return AskMove{0, 0};   // no take available; engine falls back
  }
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    int owners[SETSZ];
    for (int s = 0; s < NSET; s++) {
      if (!pub.setActive[s]) continue;
      if (!teamOwns(s, owners)) continue;
      d.set = uint8_t(s);
      for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(owners[i]);
      conf = 1.0;
      return true;
    }
    return false;
  }
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double th) override {
    int owners[SETSZ];
    if (!teamOwns(set, owners)) return false;
    d.set = uint8_t(set);
    for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(owners[i]);
    conf = 1.0; return true;
  }
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    int owners[SETSZ];
    d.set = uint8_t(set);
    if (teamOwns(set, owners)) { for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(owners[i]); conf = 1; return; }
    for (int i = 0; i < SETSZ; i++) d.owner[i] = uint8_t(seat);
    conf = 0;
  }
};

static void modeOmni(int games, uint64_t seed, const std::string& opp) {
  Rules r; Game game;
  std::vector<OmniAgent> om(NPLAY);
  for (int p = 0; p < NPLAY; p++) om[p].truth = &game.g;
  std::vector<std::unique_ptr<Agent>> other;
  for (int p = 0; p < NPLAY; p++) other.push_back(makeAgent(opp.empty() ? "v05" : opp));
  // (a) omniscient on BOTH sides: the perfect-information value of the game.
  {
    Agent* ptr[NPLAY];
    for (int p = 0; p < NPLAY; p++) ptr[p] = &om[p];
    double sMove = 0, sOther = 0; long long n = 0, exact = 0;
    double sLockOpp = 0;
    for (int i = 0; i < games; i++) {
      uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
      game.rotation = i % NPLAY;
      GameResult res = game.run(s, r, ptr);
      GameState tmp{}; dealCards(tmp, s, r.deckSets);
      int mover = teamOf(tmp.turn);
      uint64_t hm = 0, ho = 0;
      for (int p = 0; p < NPLAY; p++) (teamOf(p) == mover ? hm : ho) |= game.g.dealt[p];
      int lockedOpp = 0;
      for (int st = 0; st < NSET; st++) { uint64_t m = setMask(st); if ((ho & m) == m) lockedOpp++; }
      (void)hm;
      sMove += res.score[mover]; sOther += res.score[1 - mover];
      sLockOpp += lockedOpp;
      if (res.score[1 - mover] == lockedOpp) exact++;
      n++;
    }
    printf("# mode=omni games=%d seed=%llu\n", games, (unsigned long long)seed);
    printf("PERFECT INFO (both teams omniscient): mover %.4f sets, other %.4f sets over %lld games\n",
           sMove / n, sOther / n, n);
    printf("  mean half-suits dealt LOCKED to the non-moving team: %.4f\n", sLockOpp / n);
    printf("  games where non-mover's score == its deal-time locked count: %lld/%lld = %.4f\n",
           exact, n, double(exact) / n);
  }
  // (b) omniscient team vs the reference policy: the value of the hidden state.
  {
    Agent* ptr[NPLAY];
    double sO = 0, sV = 0; long long n = 0, wins = 0;
    for (int i = 0; i < games; i++) {
      uint64_t s = mixSeed(seed, uint64_t(i) * 2654435761ull + 1);
      for (int orient = 0; orient < 2; orient++) {
        for (int p = 0; p < NPLAY; p++) ptr[p] = (teamOf(p) == orient) ? (Agent*)&om[p] : other[p].get();
        game.rotation = i % NPLAY;
        GameResult res = game.run(s, r, ptr);
        sO += res.score[orient]; sV += res.score[1 - orient];
        if (res.winner == orient) wins++;
        n++;
      }
    }
    printf("OMNISCIENT vs %s : %.4f - %.4f sets, omniscient win rate %.4f over %lld games\n",
           opp.empty() ? "v05" : opp.c_str(), sO / n, sV / n, double(wins) / n, n);
  }
}

int main(int argc, char** argv) {
  std::string mode = argc > 1 ? argv[1] : "state";
  int games = argc > 2 ? atoi(argv[2]) : 100;
  uint64_t seed = argc > 3 ? strtoull(argv[3], nullptr, 10) : 31;
  std::string extra = argc > 4 ? argv[4] : "";
  if (mode == "state") modeState(games, seed, extra != "noz");
  else if (mode == "endgame") modeEndgame(games, seed, extra.empty() ? 3000000LL : atoll(extra.c_str()));
  else if (mode == "turn") modeTurn(games, seed, extra.empty() ? "v05" : extra);
  else if (mode == "deal") modeDeal(games, seed);
  else if (mode == "flip") modeFlip(games, seed);
  else if (mode == "census") modeCensus(games, seed);
  else if (mode == "omni") modeOmni(games, seed, extra);
  else { fprintf(stderr, "unknown mode\n"); return 2; }
  return 0;
}
