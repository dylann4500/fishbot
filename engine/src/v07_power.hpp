// FishBot v0.7 -- power arithmetic, printed with every cell.
//
// The project's standing rule (paper/sections_v06/10-protocol.tex,
// sec:protocol-metrics; docs/v07/SUBOPTIMALITY-LEDGER.md section 0.3) is that a
// 95% half-width of roughly 98/sqrt(N) points accompanies every win-rate cell,
// so that a null is read at the resolution it was measured at rather than as a
// zero.  Before v0.7 that arithmetic lived in prose and in two markdown tables;
// every study that quoted it had to do the division by hand, and the v0.6 audit
// found four macro mis-bindings in the neighbourhood of exactly this quantity
// (SUBOPTIMALITY-LEDGER.md P-6).  Here it is computed by the harness and
// emitted with the cell, so the number and its resolution cannot drift apart.
//
// Three quantities, and they are not the same quantity:
//
//   halfWidth98(n)     the unpaired one-arm half-width at p ~ 1/2 over n GAMES.
//                      This is the figure the ledger's table prints.
//   halfWidthDeals(d)  the same formula over d DEALS.  A duplicate block plays
//                      one deal at `rotations` orientations and the outcomes of
//                      one deal are a single correlated cluster, so the paired
//                      estimator's effective sample is the number of deals, not
//                      the number of games.  Reporting 98/sqrt(games) for a
//                      paired design overstates the resolution by
//                      sqrt(rotations).
//   mirror             a mirror win-rate cell carries NO information: the arms
//                      are exchangeable by construction and the per-deal outcome
//                      is deterministic, so the effective sample is zero and the
//                      artifact prints ci [0.5, 0.5].  Halving is the correct
//                      correction for RATE DENOMINATORS (asks, declarations),
//                      not for win rates.  We flag the cell rather than printing
//                      a half-width that does not exist.
#pragma once
#include <cmath>
#include <string>
#include <cstdio>

namespace fish {

inline double halfWidth98(double n) { return n > 0 ? 98.0 / std::sqrt(n) : 0.0; }

// Games required for a stated 95% half-width in percentage points.
inline double gamesFor(double pts) { return pts > 0 ? (98.0 / pts) * (98.0 / pts) : 0.0; }

struct PowerLine {
  long long games = 0;
  long long deals = 0;
  int rotations = 2;
  bool mirror = false;
  double hwGames = 0;     // 98/sqrt(games)
  double hwDeals = 0;     // 98/sqrt(deals)  -- the paired design's floor
  double nFor1pt = 0, nForHalfPt = 0, nForQuarterPt = 0;
};

inline PowerLine powerLine(long long games, long long deals, int rotations, bool mirror) {
  PowerLine p;
  p.games = games; p.deals = deals; p.rotations = rotations; p.mirror = mirror;
  p.hwGames = halfWidth98(double(games));
  p.hwDeals = halfWidth98(double(deals));
  p.nFor1pt = gamesFor(1.0);
  p.nForHalfPt = gamesFor(0.5);
  p.nForQuarterPt = gamesFor(0.25);
  return p;
}

// JSON fragment, without the enclosing braces, for splicing into a cell record.
inline std::string powerJson(const PowerLine& p) {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "\"power\":{\"games\":%lld,\"deals\":%lld,\"rotations\":%d,\"mirror\":%s,"
           "\"halfWidth98Games\":%.4f,\"halfWidth98Deals\":%.4f,"
           "\"gamesFor1pt\":%.0f,\"gamesFor0.5pt\":%.0f,\"gamesFor0.25pt\":%.0f}",
           p.games, p.deals, p.rotations, p.mirror ? "true" : "false",
           p.hwGames, p.hwDeals, p.nFor1pt, p.nForHalfPt, p.nForQuarterPt);
  return std::string(buf);
}

inline std::string powerText(const PowerLine& p) {
  char buf[512];
  if (p.mirror) {
    snprintf(buf, sizeof(buf),
             "  power         MIRROR CELL: win-rate effective sample is 0 (per-deal outcome is "
             "deterministic).  Rate denominators are halved.  n=%lld games / %lld deals",
             p.games, p.deals);
  } else {
    snprintf(buf, sizeof(buf),
             "  power         98/sqrt(N): +/-%.2f pts unpaired over %lld games; +/-%.2f pts over "
             "%lld deals (the paired floor).  1 pt needs %.0f games, 0.5 pt %.0f, 0.25 pt %.0f",
             p.hwGames, p.games, p.hwDeals, p.deals, p.nFor1pt, p.nForHalfPt, p.nForQuarterPt);
  }
  return std::string(buf);
}

} // namespace fish
