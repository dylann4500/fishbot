// ADVERSARIAL VERIFICATION probe for the P2 claim "100% of forced-endgame
// declarations are wrong, and the figure is not a counting artefact".
//
// This file deliberately shares NO code with diag.hpp or probe_forcedendgame.hpp.
// It reruns games, and from (a) the opening deal g.dealt and (b) the public event
// trace, it reconstructs the true hands itself and re-scores every ForcedDeclare
// event from scratch, ignoring Event::success and ignoring GameResult entirely.
//
// It also classifies WHERE each ForcedDeclare came from, because
// Game::applyDeclaration(..., forced=true, ...) has TWO call sites:
//   game.hpp:251  -- inside Game::forcedEndgame (a team is cardless)
//   game.hpp:335  -- the "holds only complete sets, must declare" branch
// diag.hpp lumps both into declsForced.
#pragma once
#include "factory.hpp"
#include <thread>

namespace fish {

struct VFStats {
  long long games = 0;
  long long forcedEvents = 0;      // Kind::ForcedDeclare seen in the trace
  long long forcedWrongMine = 0;   // wrong per MY OWN recomputation
  long long forcedWrongFlag = 0;   // wrong per the engine's own success flag
  long long flagDisagree = 0;      // my recomputation != engine flag
  long long fromEndgame = 0;       // opposing team was cardless before the event
  long long fromEndgameWrong = 0;
  long long fromCompleteSet = 0;   // the other call site
  long long fromCompleteSetWrong = 0;
  long long voluntary = 0, voluntaryWrong = 0;
  // structural facts about the endgame declarations
  long long capViolation = 0;      // named more cards at a seat than it can hold
                                   // (checked against TRUE hand sizes at the time)
  long long nMisnamed[8] = {0};    // how many of the six cards were named wrong
  long long onlyOneTeammateUsed = 0;
  long long truthSplitsAcrossSeats = 0;
  long long confZero = 0;
  long long activeAtDecl[10] = {0};
  long long declaredByLowestLiveSeat = 0;
  long long unresolvableTruth = 0; // named a seat on the WRONG team (would be illegal)
  long long forcedA = 0, forcedAWrong = 0;   // declaring team was spec A
  long long forcedB = 0, forcedBWrong = 0;   // declaring team was spec B
  void merge(const VFStats& o) {
    games += o.games; forcedEvents += o.forcedEvents;
    forcedWrongMine += o.forcedWrongMine; forcedWrongFlag += o.forcedWrongFlag;
    flagDisagree += o.flagDisagree;
    fromEndgame += o.fromEndgame; fromEndgameWrong += o.fromEndgameWrong;
    fromCompleteSet += o.fromCompleteSet; fromCompleteSetWrong += o.fromCompleteSetWrong;
    voluntary += o.voluntary; voluntaryWrong += o.voluntaryWrong;
    capViolation += o.capViolation;
    for (int i = 0; i < 8; i++) nMisnamed[i] += o.nMisnamed[i];
    onlyOneTeammateUsed += o.onlyOneTeammateUsed;
    truthSplitsAcrossSeats += o.truthSplitsAcrossSeats;
    confZero += o.confZero;
    for (int i = 0; i < 10; i++) activeAtDecl[i] += o.activeAtDecl[i];
    declaredByLowestLiveSeat += o.declaredByLowestLiveSeat;
    unresolvableTruth += o.unresolvableTruth;
    forcedA += o.forcedA; forcedAWrong += o.forcedAWrong;
    forcedB += o.forcedB; forcedBWrong += o.forcedBWrong;
  }
};

// Replay one trace against the true opening deal, re-deriving everything.
inline void vfAnalyse(const std::vector<Event>& ev, const uint64_t dealt[NPLAY],
                      const Rules& rules, VFStats& st, int teamA) {
  uint64_t hand[NPLAY];
  for (int p = 0; p < NPLAY; p++) hand[p] = dealt[p];
  bool active[NSET];
  for (int s = 0; s < NSET; s++) active[s] = (s < rules.deckSets);
  st.games++;

  for (const Event& e : ev) {
    if (e.kind == Kind::Ask) {
      if (e.success) { hand[e.target] &= ~bit(e.card); hand[e.actor] |= bit(e.card); }
      continue;
    }
    if (e.kind != Kind::Declare && e.kind != Kind::ForcedDeclare) continue;

    int team = teamOf(e.actor);
    // ---- recompute correctness from ground truth, independent of e.success
    bool mineCorrect = true;
    int misnamed = 0; bool wrongTeamNamed = false;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(e.set, i);
      int claimed = e.decl.owner[i];
      if (teamOf(claimed) != team) { wrongTeamNamed = true; mineCorrect = false; misnamed++; continue; }
      if (!(hand[claimed] & bit(c))) { mineCorrect = false; misnamed++; }
    }
    if (e.kind == Kind::Declare) {
      st.voluntary++; if (!mineCorrect) st.voluntaryWrong++;
    } else {
      st.forcedEvents++;
      if (!mineCorrect) st.forcedWrongMine++;
      if (!e.success) st.forcedWrongFlag++;
      if (mineCorrect == !e.success) st.flagDisagree++;
      if (wrongTeamNamed) st.unresolvableTruth++;
      st.nMisnamed[std::min(7, misnamed)]++;
      if (team == teamA) { st.forcedA++; if (!mineCorrect) st.forcedAWrong++; }
      else { st.forcedB++; if (!mineCorrect) st.forcedBWrong++; }
      if (e.confidence == 0.0) st.confZero++;

      // which call site?  forcedEndgame requires the OPPOSING team cardless.
      int oppCards = 0, myLive = 0;
      for (int p = 0; p < NPLAY; p++)
        if (teamOf(p) != team) oppCards += popcount64(hand[p]);
      for (int p = team; p < NPLAY; p += 2) if (popcount64(hand[p])) myLive++;
      bool endgame = (oppCards == 0);
      if (endgame) { st.fromEndgame++; if (!mineCorrect) st.fromEndgameWrong++; }
      else { st.fromCompleteSet++; if (!mineCorrect) st.fromCompleteSetWrong++; }

      int nAct = 0; for (int s = 0; s < NSET; s++) nAct += active[s];
      st.activeAtDecl[std::min(9, nAct)]++;

      // lowest-seated teammate with cards?
      int lowest = -1;
      for (int p = team; p < NPLAY; p += 2) if (popcount64(hand[p])) { lowest = p; break; }
      if (lowest == int(e.actor)) st.declaredByLowestLiveSeat++;

      // capacity feasibility of the NAMED allocation against true hand sizes:
      // how many cards of this half-suit each named seat is credited with vs.
      // how many cards it actually holds in total.
      int used[NPLAY] = {0,0,0,0,0,0};
      for (int i = 0; i < SETSZ; i++) used[e.decl.owner[i]]++;
      bool cap = false;
      for (int p = 0; p < NPLAY; p++) if (used[p] > int(popcount64(hand[p]))) cap = true;
      if (cap) st.capViolation++;

      int namedSeats = 0, trueSeats = 0;
      { int nm = 0, tm = 0;
        for (int i = 0; i < SETSZ; i++) {
          nm |= 1 << e.decl.owner[i];
          int c = cardOf(e.set, i);
          for (int p = 0; p < NPLAY; p++) if (hand[p] & bit(c)) tm |= 1 << p;
        }
        namedSeats = __builtin_popcount(nm); trueSeats = __builtin_popcount(tm);
      }
      if (namedSeats == 1) st.onlyOneTeammateUsed++;
      if (trueSeats > 1) st.truthSplitsAcrossSeats++;
    }
    for (int p = 0; p < NPLAY; p++) hand[p] &= ~setMask(e.set);
    active[e.set] = false;
  }
}

struct VFConfig {
  std::string specA = "v04", specB = "v04";
  int games = 300, rotations = 2, threads = 0;
  uint64_t seed = 31;
  Rules rules;
};

inline VFStats runVForced(const VFConfig& fc) {
  int nT = fc.threads > 0 ? fc.threads : int(std::thread::hardware_concurrency());
  if (nT < 1) nT = 1;
  nT = std::min(nT, std::max(1, fc.games));
  std::vector<VFStats> local(nT);
  std::vector<std::thread> pool;
  for (int t = 0; t < nT; t++) {
    pool.emplace_back([&, t]() {
      std::unique_ptr<Agent> A[3], B[3];
      for (int i = 0; i < 3; i++) { A[i] = makeAgent(fc.specA); B[i] = makeAgent(fc.specB); }
      Game game; game.trace.on = true;
      for (int i = t; i < fc.games; i += nT) {
        uint64_t s = mixSeed(fc.seed, uint64_t(i) * 2654435761ull + 1);
        for (int rot = 0; rot < fc.rotations; rot++) {
          int orient = (fc.rotations == 2) ? rot : (rot / 3);
          int shift  = (fc.rotations == 2) ? 0   : (rot % 3);
          Agent* ag[NPLAY];
          for (int p = 0; p < NPLAY; p++)
            ag[p] = (teamOf(p) == orient) ? A[p / 2].get() : B[p / 2].get();
          game.rotation = shift;
          game.trace.events.clear();
          game.run(s, fc.rules, ag);
          vfAnalyse(game.trace.events, game.g.dealt, fc.rules, local[t], orient);
        }
      }
    });
  }
  for (auto& th : pool) th.join();
  VFStats tot;
  for (int t = 0; t < nT; t++) tot.merge(local[t]);
  return tot;
}

inline void printVForced(const VFStats& s, std::ostream& os) {
  auto pc = [&](long long a, long long b) { return b ? 100.0 * double(a) / double(b) : 0.0; };
  os << "games                       " << s.games << "\n";
  os << "voluntary declarations      " << s.voluntary << "  wrong " << s.voluntaryWrong
     << " (" << pc(s.voluntaryWrong, s.voluntary) << "%)\n";
  os << "ForcedDeclare events        " << s.forcedEvents << "\n";
  os << "  wrong, MY recomputation   " << s.forcedWrongMine << " (" << pc(s.forcedWrongMine, s.forcedEvents) << "%)\n";
  os << "  wrong, engine flag        " << s.forcedWrongFlag << " (" << pc(s.forcedWrongFlag, s.forcedEvents) << "%)\n";
  os << "  DISAGREEMENTS             " << s.flagDisagree << "\n";
  os << "  by declaring team: A      " << s.forcedA << "  wrong " << s.forcedAWrong
     << " (" << pc(s.forcedAWrong, s.forcedA) << "%)\n";
  os << "  by declaring team: B      " << s.forcedB << "  wrong " << s.forcedBWrong
     << " (" << pc(s.forcedBWrong, s.forcedB) << "%)\n";
  os << "  from forcedEndgame path   " << s.fromEndgame << "  wrong " << s.fromEndgameWrong
     << " (" << pc(s.fromEndgameWrong, s.fromEndgame) << "%)\n";
  os << "  from complete-set branch  " << s.fromCompleteSet << "  wrong " << s.fromCompleteSetWrong
     << " (" << pc(s.fromCompleteSetWrong, s.fromCompleteSet) << "%)\n";
  os << "  named a WRONG-TEAM seat   " << s.unresolvableTruth << "\n";
  os << "  confidence exactly 0      " << s.confZero << " (" << pc(s.confZero, s.forcedEvents) << "%)\n";
  os << "  declarer == lowest live teammate " << s.declaredByLowestLiveSeat
     << " (" << pc(s.declaredByLowestLiveSeat, s.forcedEvents) << "%)\n";
  os << "  named alloc exceeds a seat's TRUE hand size " << s.capViolation
     << " (" << pc(s.capViolation, s.forcedEvents) << "%)\n";
  os << "  all six cards named at ONE seat  " << s.onlyOneTeammateUsed
     << " (" << pc(s.onlyOneTeammateUsed, s.forcedEvents) << "%)\n";
  os << "  truth spreads the half-suit over >1 seat " << s.truthSplitsAcrossSeats
     << " (" << pc(s.truthSplitsAcrossSeats, s.forcedEvents) << "%)\n";
  os << "misnamed cards per forced declaration\n";
  for (int i = 0; i < 8; i++) if (s.nMisnamed[i]) os << "  " << i << ": " << s.nMisnamed[i] << "\n";
  os << "live half-suits at the forced declaration\n";
  for (int i = 0; i < 10; i++) if (s.activeAtDecl[i]) os << "  " << i << ": " << s.activeAtDecl[i] << "\n";
}

} // namespace fish
