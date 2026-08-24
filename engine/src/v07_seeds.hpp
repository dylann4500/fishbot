// FishBot v0.7 -- the reserved-seed registry.
//
// paper/sections_v06/10-protocol.tex (sec:protocol-seeds) states that seed-bank
// disjointness "is enforced by a registry of the \vsixReservedSeeds{} reserved
// seeds that the battery checks, rather than by discipline."  No such registry
// exists at commit 60fee17: `\vsixReservedSeeds` is a two-element literal
// emitted by engine/build_tables_v06.py:469 and nothing checks anything.  This
// file is the registry the paper describes, built for v0.7 and back-filled with
// every seed the v0.4-v0.6 batteries used, so the claim becomes true.
//
// The rules the checker enforces:
//
//   R1  A seed may not carry both a FIT role and an EVAL/HOLDOUT role.  Fitting
//       deals must not reappear as evaluation deals.
//   R2  A HOLDOUT seed may carry no other role at all.
//   R3  A SEALED seed may not be read before the phase that unseals it; the
//       binary refuses to construct a bank from one unless
//       FISH_UNSEAL_PHASE >= its phase.
//   R4  Every seed a v0.7 battery uses must appear here.  `fish seeds --check`
//       fails on an unregistered seed passed via --require.
//
// Roles are recorded as they were USED, not as they were intended, which is why
// R1 fires on 515253: engine/exploitability_v06.sh:26 fits the v0.6
// exploitability responder on bank 515253 and engine/experiments_v06.sh:56
// evaluates the per-style panel on the same bank.  The exploitability figure
// itself is unaffected -- its own evaluation half is the disjoint bank 6543210
// (exploitability_v06.sh:27) -- but the collision is real and the registry is
// the first thing in the corpus that says so.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace fish {

enum class SeedRole { Fit, Eval, Holdout, Sealed, Diagnostic };

inline const char* roleName(SeedRole r) {
  switch (r) {
    case SeedRole::Fit: return "fit";
    case SeedRole::Eval: return "eval";
    case SeedRole::Holdout: return "holdout";
    case SeedRole::Sealed: return "sealed";
    default: return "diagnostic";
  }
}

struct SeedEntry {
  uint64_t seed;
  SeedRole role;
  const char* study;      // v04 | v05 | v06 | v07
  const char* battery;    // the battery or script that consumes it
  int unsealPhase;        // 0 = open; >0 = readable only from that v0.7 phase on
  const char* note;
};

// ---------------------------------------------------------------- the table
inline const std::vector<SeedEntry>& seedRegistry() {
  static const std::vector<SeedEntry> R = {
    // ---- v0.6 fitting -----------------------------------------------------
    {20260823, SeedRole::Fit, "v06", "fitA (tune)", 0, "fitting base seed, run A"},
    {20260824, SeedRole::Fit, "v06", "fitC (tune)", 0, "fitting base seed, run C; V6PARAMS provenance"},
    // ---- v0.6 evaluation --------------------------------------------------
    {90210,  SeedRole::Eval, "v06", "E3 head-to-head; E8 ties(non-mirror); E10 gateaudit; E12 search; E13 rollout; E14 searchdev; E2 pathology(vs v05)", 0, "most heavily reused evaluation bank in the corpus"},
    {31337,  SeedRole::Eval, "v06", "E3 head-to-head; F0 search-confirm", 0, ""},
    {515151, SeedRole::Eval, "v06", "E3 head-to-head; F0 search-confirm", 0, ""},
    {777001, SeedRole::Eval, "v06", "E3 head-to-head", 0, ""},
    {424242, SeedRole::Eval, "v06", "E3 head-to-head", 0, "also the tuner's DEFAULT seed (tuner.hpp TuneSpec::seed)"},
    {515253, SeedRole::Eval, "v06", "E4 per-style panel", 0, "COLLIDES with the exploitability responder's fitting bank"},
    {515253, SeedRole::Fit,  "v06", "X1 exploitability responder fit (exploitability_v06.sh FITSEED)", 0, "COLLIDES with E4"},
    {6543210, SeedRole::Eval, "v06", "X1 exploitability responder evaluation (EVALSEED)", 0, "the fresh half; disjoint from the fit"},
    {606060, SeedRole::Eval, "v06", "E5 ablations; F1 chain2x2", 0, ""},
    {717171, SeedRole::Eval, "v06", "E6 calibration", 0, ""},
    {828282, SeedRole::Eval, "v06", "E7 rule dialects", 0, ""},
    {313131, SeedRole::Eval, "v06", "E11 partner regimes", 0, ""},
    {31,     SeedRole::Diagnostic, "v06", "E0 identity; E2 pathology; E8 ties/belief; E15 deliberate miss", 0, "KPI and identity controls, not a strength bank"},
    // ---- v0.5 / v0.4 banks carried forward ---------------------------------
    {20260821, SeedRole::Eval, "v05", "arena default (MatchConfig::seed)", 0, "the engine's built-in default"},
    {20260822, SeedRole::Diagnostic, "v05", "P1 deadlock verification", 0, ""},
    {777,    SeedRole::Diagnostic, "v05", "P2 forced-endgame replay", 0, ""},
    {424243, SeedRole::Fit, "v05", "v0.5 fitting", 0, ""},

    // ================= v0.7 =================================================
    // Phase 1 instrument banks.  Disjoint from every seed above by construction
    // (the 7'0xx'xxx block is reserved for v0.7 and is used nowhere else).
    {7010001, SeedRole::Diagnostic, "v07", "T1 throughput table", 0, "games/s only; no strength claim rides on it"},
    {7010002, SeedRole::Diagnostic, "v07", "T2 rollout-fidelity ladder", 0, ""},
    {7011001, SeedRole::Diagnostic, "v07", "D1 per-decision channel", 0, ""},
    {7011002, SeedRole::Diagnostic, "v07", "D2 per-decision channel, replicate", 0, ""},
    {7012001, SeedRole::Diagnostic, "v07", "W1 transcript-inversion bit measurement", 0, ""},
    {7012002, SeedRole::Diagnostic, "v07", "W2 transcript-inversion bit measurement, replicate", 0, ""},
    // Planted-edge calibration: the fitting half and the sealed evaluation half.
    {7020001, SeedRole::Fit,  "v07", "C1 responder fitting bank A (planted-edge calibration)", 0, ""},
    {7020002, SeedRole::Fit,  "v07", "C1 responder fitting bank B (planted-edge calibration)", 0, ""},
    {7021001, SeedRole::Eval, "v07", "C1 responder evaluation bank A (fresh; planted-edge calibration)", 0, ""},
    {7021002, SeedRole::Eval, "v07", "C1 responder evaluation bank B (fresh; replicate)", 0, ""},
    {7022001, SeedRole::Eval, "v07", "C0 planted-edge ground truth (reference exploiter vs handicapped target)", 0, ""},
    {7022002, SeedRole::Eval, "v07", "C0 planted-edge ground truth, replicate", 0, ""},
    // Phase 2-4 training banks.
    {7030001, SeedRole::Eval, "v07", "train bank 1", 0, "research/v07/banks/train"},
    {7030002, SeedRole::Eval, "v07", "train bank 2", 0, "research/v07/banks/train"},
    {7030003, SeedRole::Eval, "v07", "train bank 3", 0, "research/v07/banks/train"},
    {7030004, SeedRole::Eval, "v07", "train bank 4", 0, "research/v07/banks/train"},
    // Phase 5 holdout.  SEALED: unsealPhase 5.
    {7090001, SeedRole::Sealed, "v07", "holdout bank 1", 5, "sealed until phase 5"},
    {7090002, SeedRole::Sealed, "v07", "holdout bank 2", 5, "sealed until phase 5"},
    {7090003, SeedRole::Sealed, "v07", "holdout bank 3", 5, "sealed until phase 5"},
    {7091001, SeedRole::Sealed, "v07", "sealed adversary half", 5, "sealed until phase 5"},
  };
  return R;
}

// The phase this binary is permitted to unseal, from FISH_UNSEAL_PHASE.
inline int unsealPhase() {
  const char* e = getenv("FISH_UNSEAL_PHASE");
  return e ? atoi(e) : 0;
}

struct SeedCheck { int violations = 0; std::string report; };

inline SeedCheck checkSeeds() {
  SeedCheck c;
  const auto& R = seedRegistry();
  char buf[512];
  for (size_t i = 0; i < R.size(); i++) {
    for (size_t j = i + 1; j < R.size(); j++) {
      if (R[i].seed != R[j].seed) continue;
      bool fitI = R[i].role == SeedRole::Fit, fitJ = R[j].role == SeedRole::Fit;
      bool holdI = R[i].role == SeedRole::Holdout || R[i].role == SeedRole::Sealed;
      bool holdJ = R[j].role == SeedRole::Holdout || R[j].role == SeedRole::Sealed;
      if (fitI != fitJ) {   // R1
        snprintf(buf, sizeof(buf), "R1 VIOLATION seed %llu: '%s/%s' (%s) and '%s/%s' (%s)\n",
                 (unsigned long long)R[i].seed, R[i].study, R[i].battery, roleName(R[i].role),
                 R[j].study, R[j].battery, roleName(R[j].role));
        c.report += buf; c.violations++;
      }
      if (holdI || holdJ) { // R2
        snprintf(buf, sizeof(buf), "R2 VIOLATION seed %llu: holdout seed also used as '%s'\n",
                 (unsigned long long)R[i].seed, roleName(holdI ? R[j].role : R[i].role));
        c.report += buf; c.violations++;
      }
    }
  }
  return c;
}

// R3.  Returns true if the seed may be used now.
inline bool seedUsable(uint64_t s, std::string& why) {
  for (const auto& e : seedRegistry()) {
    if (e.seed != s) continue;
    if (e.role == SeedRole::Sealed && unsealPhase() < e.unsealPhase) {
      why = "seed " + std::to_string(s) + " is SEALED until phase " +
            std::to_string(e.unsealPhase) + " (set FISH_UNSEAL_PHASE to unseal)";
      return false;
    }
  }
  return true;
}

// R4.  Is the seed registered at all?
inline bool seedRegistered(uint64_t s) {
  for (const auto& e : seedRegistry()) if (e.seed == s) return true;
  return false;
}

} // namespace fish
