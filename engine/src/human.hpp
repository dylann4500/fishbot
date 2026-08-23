// Human-in-the-loop agent for the interactive table (`fish serve`).
//
// The point of the interactive table is to measure a human against the *same*
// FishBot the study measures, so nothing here reimplements the rules: a
// HumanAgent is an ordinary `Agent` handed to the ordinary `Game` driver, and
// every rule -- ask legality, out-of-turn declaration, misdeclaration, cardless
// turn passing, the forced endgame -- is enforced by game.hpp exactly as it is
// in a headless match.
//
// The one structural difference is that a human decides slowly.  Game::run is a
// blocking loop, so the table runs it on its own thread and the HumanAgent's
// decision points block on a condition variable until the HTTP thread delivers
// an answer.  Two consequences are worth stating:
//
//   * A voluntary declaration is *queued*, not immediate.  Game::run polls every
//     agent for a declaration once per loop iteration, i.e. before every ask, so
//     a queued declaration is announced at the next such poll.  That is exactly
//     the granularity the bots get, so neither side is favoured.
//   * On the human's own turn the block sits in proposeDeclaration rather than
//     chooseAsk, because that poll happens first; the UI offers "ask" and
//     "declare" together and whichever arrives resolves the block.  Answering
//     "ask" stores the move, returns false, and chooseAsk consumes it without
//     blocking again.
//
// Hidden information stays hidden: a HumanAgent never sees GameState, only its
// own hand (tracked by the shared Knowledge object) and the public event stream.
#pragma once
#include "game.hpp"
#include <mutex>
#include <condition_variable>
#include <string>

namespace fish {

// Thrown out of a blocked decision point to unwind the game thread when the
// table is torn down (new game, or shutdown).
struct Abandoned {};

enum class Need : int {
  None   = 0,
  Turn   = 1,   // your turn: ask, or declare instead of asking
  Pass   = 2,   // you are cardless and hold the turn: choose a live teammate
  Forced = 3,   // forced endgame: declare this half-suit, or step aside
  Guess  = 4    // forced endgame, last resort: you must declare this half-suit
};

inline const char* needName(Need n) {
  switch (n) {
    case Need::None:   return "none";
    case Need::Turn:   return "turn";
    case Need::Pass:   return "pass";
    case Need::Forced: return "forced";
    case Need::Guess:  return "guess";
  }
  return "none";
}

// Everything the HTTP thread may read or write for one seat.  Guarded by
// HumanIO::mu; the seat's agent is the only writer on the game-thread side.
struct HumanSlot {
  bool human = false;
  Need need = Need::None;
  int  needSet = -1;
  int  cands[NPLAY] = {0,0,0,0,0,0};
  int  nCands = 0;

  bool haveAsk = false;      AskMove ask{};
  bool haveDecl = false;     Declaration decl{};        // queued voluntary declaration
  bool haveForced = false;   Declaration forced{};      // answer to a forced-endgame prompt
  bool forcedSkip = false;
  bool forcedDeferAll = false;
  bool havePass = false;     int passTo = -1;

  bool declAnswered = false;                            // "I'll ask" since the last event
  int8_t forcedAnswer[NSET] = {-1,-1,-1,-1,-1,-1,-1,-1,-1};
  Declaration forcedStore[NSET];
  std::string note;
};

struct HumanIO {
  std::mutex mu;
  std::condition_variable cv;
  bool abandon = false;
  bool paused  = false;
  int  stepBudget = 0;       // while paused, release this many further events
  int  paceMs = 2000;
  HumanSlot slot[NPLAY];
  uint64_t rev = 0;
  void bump() { rev++; cv.notify_all(); }   // call with mu held
};

struct HumanAgent : Agent {
  HumanIO* io = nullptr;
  std::string label = "You";

  const char* name() const override { return label.c_str(); }

  template <class P>
  void await(std::unique_lock<std::mutex>& lk, P pred) {
    io->cv.wait(lk, [&] { return io->abandon || pred(); });
    if (io->abandon) throw Abandoned{};
  }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    std::lock_guard<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[s];
    sl = HumanSlot{};
    sl.human = true;
    io->bump();
  }

  void observe(const Event& e) override {
    Agent::observe(e);
    std::lock_guard<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[seat];
    // The board changed, so the "I'll ask rather than declare" answer and any
    // ask chosen against the old board are both stale.
    sl.declAnswered = false;
    if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      sl.haveAsk = false;
      // A queued declaration is cleared once its half-suit leaves play, whether
      // because we announced it or because somebody else claimed it first.
      if (sl.haveDecl && sl.decl.set == e.set) {
        sl.haveDecl = false;
        if (e.actor != seat) sl.note = std::string(setName(e.set)) + " was claimed before your declaration was announced.";
      }
    }
    io->bump();
  }

  // Voluntary declaration.  Off turn this is a non-blocking queue check, so the
  // bots are never held up; on turn it is the single blocking decision point.
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    std::unique_lock<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[seat];
    if (sl.haveDecl && !pub.setActive[sl.decl.set]) {
      sl.haveDecl = false;
      sl.note = std::string(setName(sl.decl.set)) + " is no longer in play.";
      io->bump();
    }
    bool myTurn = (pub.turn == seat) && pub.handCount[seat] > 0;
    if (myTurn && !sl.declAnswered && !sl.haveAsk && !sl.haveDecl) {
      sl.need = Need::Turn;
      sl.needSet = -1;
      io->bump();
      await(lk, [&] { return sl.haveAsk || sl.haveDecl; });
      sl.need = Need::None;
      io->bump();
    }
    if (sl.haveDecl && pub.setActive[sl.decl.set]) {
      // Left queued deliberately: declarationRound polls every seat and executes
      // only the arbitration winner, so we must not consume it here.  observe()
      // clears it when the half-suit actually leaves play.
      d = sl.decl;
      conf = 1.0;
      return true;
    }
    if (myTurn) sl.declAnswered = true;
    return false;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    {
      // Holding only complete half-suits leaves no legal ask.  Returning an
      // illegal move hands the case back to Game::run, which enumerates, finds
      // nothing, and routes us to bestGuess -- i.e. "you must declare".
      AskMove buf[NSET * SETSZ * 3];
      if (enumerateAsks(pub, k.myHand, seat, buf) == 0) return AskMove{0, 0};
    }
    std::unique_lock<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[seat];
    for (;;) {
      if (sl.haveAsk) {
        AskMove m = sl.ask;
        sl.haveAsk = false;
        if (legalAskShape(pub, k.myHand, seat, m.card, m.target)) {
          sl.need = Need::None;
          sl.note.clear();
          io->bump();
          return m;
        }
        sl.note = "That ask is not legal any more -- choose again.";
      }
      sl.need = Need::Turn;
      sl.needSet = -1;
      io->bump();
      await(lk, [&] { return sl.haveAsk; });
    }
  }

  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    (void)pub;
    std::unique_lock<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[seat];
    sl.nCands = n;
    for (int i = 0; i < n && i < NPLAY; i++) sl.cands[i] = cand[i];
    sl.havePass = false;
    sl.need = Need::Pass;
    io->bump();
    await(lk, [&] { return sl.havePass; });
    int t = sl.passTo;
    sl.havePass = false;
    sl.need = Need::None;
    sl.nCands = 0;
    io->bump();
    for (int i = 0; i < n; i++) if (cand[i] == t) return t;
    return cand[0];
  }

  // Forced endgame.  Thresholds mean nothing to a human, so the answer is per
  // half-suit and is remembered: declare it (with an allocation), or step aside
  // and let a teammate take it.  "Defer all" stops the prompting entirely.
  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf, double threshold) override {
    (void)pub; (void)threshold;
    std::unique_lock<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[seat];
    if (sl.forcedDeferAll) return false;
    if (sl.forcedAnswer[set] < 0) {
      sl.need = Need::Forced;
      sl.needSet = set;
      sl.haveForced = false;
      sl.forcedSkip = false;
      io->bump();
      await(lk, [&] { return sl.haveForced || sl.forcedSkip || sl.forcedDeferAll; });
      if (sl.haveForced) {
        sl.forcedAnswer[set] = 1;
        sl.forcedStore[set] = sl.forced;
        sl.forcedStore[set].set = uint8_t(set);
      } else {
        sl.forcedAnswer[set] = 0;
      }
      sl.haveForced = false;
      sl.forcedSkip = false;
      sl.need = Need::None;
      sl.needSet = -1;
      io->bump();
    }
    if (sl.forcedAnswer[set] == 1) { d = sl.forcedStore[set]; conf = 1.0; return true; }
    return false;
  }

  // Somebody must name an allocation, so this one cannot be declined.  It is
  // also reached in ordinary play when a player holds only complete half-suits
  // and therefore has no legal ask.
  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    (void)pub;
    std::unique_lock<std::mutex> lk(io->mu);
    HumanSlot& sl = io->slot[seat];
    if (sl.forcedAnswer[set] == 1) {
      d = sl.forcedStore[set];
      d.set = uint8_t(set);
      conf = 1.0;
      return;
    }
    sl.need = Need::Guess;
    sl.needSet = set;
    sl.haveForced = false;
    io->bump();
    await(lk, [&] { return sl.haveForced; });
    d = sl.forced;
    d.set = uint8_t(set);
    conf = 1.0;
    sl.haveForced = false;
    sl.need = Need::None;
    sl.needSet = -1;
    io->bump();
  }
};

} // namespace fish
