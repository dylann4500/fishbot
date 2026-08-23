// Interactive table: one live game, played by any mix of humans and bots.
//
// The game runs on its own thread inside the ordinary Game driver; the HTTP
// thread only reads a public snapshot and writes into the human seats' inboxes.
// Everything shared is guarded by HumanIO::mu.
#pragma once
#include "factory.hpp"
#include "human.hpp"
#include "httpd.hpp"
#include <chrono>
#include <thread>
#include <sstream>

namespace fish {

struct SeatCfg {
  std::string spec = "v04";
  std::string name;          // display name; distinct names are what make a
  bool human = false;        // table of six bots readable at all
};

inline bool knownPolicy(const std::string& spec) {
  std::string base;
  parseOpts(spec, base);
  static const char* ok[] = {"v04", "fishbot_v04", "v03", "fishbot_v03", "v02", "fishbot_v02",
                             "random", "hunter", "diversifier", "detective", "lockout", "bluffer"};
  for (const char* n : ok) if (base == n) return true;
  return false;
}

inline std::string policyLabel(const std::string& spec) {
  std::string base;
  auto o = parseOpts(spec, base);
  std::string name;
  if (base == "v04" || base == "fishbot_v04") {
    auto it = o.find("belief");
    name = "FishBot v0.4";
    if (it != o.end() && it->second == "block") name = "FishBot v0.4-Block";
    else if (it != o.end() && it->second != "fast") name = "FishBot v0.4-" + it->second;
  }
  else if (base == "v03" || base == "fishbot_v03") name = "FishBot v0.3";
  else if (base == "v02" || base == "fishbot_v02") name = "FishBot v0.2";
  else if (base.empty()) name = "Detective";
  else { name = base; name[0] = char(toupper((unsigned char)name[0])); }
  return name;
}

// ------------------------------------------------------------------ JSON out
inline std::string jesc(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default:
        if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += c;
    }
  }
  return o + "\"";
}

struct Snap {
  bool running = false, finished = false, hitLimit = false, everStarted = false;
  int turn = 0, dealer = 0;
  int score[2] = {0, 0};
  int handCount[NPLAY] = {0,0,0,0,0,0};
  bool setActive[NSET] = {false,false,false,false,false,false,false,false,false};
  int setWinner[NSET] = {2,2,2,2,2,2,2,2,2};
  int nEvents = 0, nAsks = 0;
  std::vector<Event> hist;
  uint64_t hand[NPLAY] = {0,0,0,0,0,0};
  uint64_t dealt[NPLAY] = {0,0,0,0,0,0};
  bool reveal = false;
  std::string label[NPLAY];   // engine label, e.g. "FishBot v0.4"
  std::string name[NPLAY];    // display name, e.g. "Nova"
  std::string spec[NPLAY];
  bool isHuman[NPLAY] = {false,false,false,false,false,false};
  uint64_t seed = 0;
  int deckSets = 9;
};

struct Table {
  HumanIO io;
  Snap snap;
  std::thread th;
  SeatCfg seats[NPLAY];
  Rules rules;
  uint64_t seed = 0;

  static const char* defaultName(int p) {
    static const char* n[NPLAY] = {"Ari", "Vega", "Nova", "Orion", "Lyra", "Rigel"};
    return n[p];
  }
  // "You" is only ever a default for a seat somebody is actually playing.
  std::string resolvedName(int p) const {
    if (!seats[p].name.empty()) return seats[p].name;
    return seats[p].human ? std::string("You") : std::string(defaultName(p));
  }

  Table() {
    for (int p = 0; p < NPLAY; p++) { seats[p].spec = "v04"; seats[p].name = defaultName(p); }
    seats[0].human = true;
    seats[0].name = "You";
    publishConfig();
  }

  // Copies the seat configuration into the snapshot so that stateJson never
  // has to touch `seats` -- the game thread owns those, the HTTP thread reads
  // only the snapshot.  Callers must not hold io.mu.
  void publishConfig() {
    std::lock_guard<std::mutex> lk(io.mu);
    for (int p = 0; p < NPLAY; p++) {
      snap.spec[p] = seats[p].spec;
      snap.isHuman[p] = seats[p].human;
      snap.label[p] = seats[p].human ? std::string("Human") : policyLabel(seats[p].spec);
      snap.name[p] = resolvedName(p);
    }
    io.bump();
  }

  // ------------------------------------------------------------- lifecycle
  void stop() {
    {
      std::lock_guard<std::mutex> lk(io.mu);
      io.abandon = true;
      io.paused = false;
      io.cv.notify_all();
    }
    if (th.joinable()) th.join();
    std::lock_guard<std::mutex> lk(io.mu);
    io.abandon = false;
    snap.running = false;
    io.bump();
  }

  void start() {
    stop();
    {
      std::lock_guard<std::mutex> lk(io.mu);
      snap = Snap{};
      snap.running = true;
      snap.everStarted = true;
      snap.seed = seed;
      snap.deckSets = rules.deckSets;
      for (int p = 0; p < NPLAY; p++) {
        snap.spec[p] = seats[p].spec;
        snap.isHuman[p] = seats[p].human;
        snap.label[p] = seats[p].human ? std::string("Human") : policyLabel(seats[p].spec);
        snap.name[p] = resolvedName(p);
        snap.handCount[p] = rules.deckSets;
      }
      snap.hist.clear();
      for (int s = 0; s < NSET; s++) { snap.setActive[s] = (s < rules.deckSets); snap.setWinner[s] = 2; }
      for (int p = 0; p < NPLAY; p++) io.slot[p] = HumanSlot{};
      io.bump();
    }
    th = std::thread([this] { gameLoop(); });
  }

  // ------------------------------------------------------------ game thread

  // Who acts next.  Game::run moves the turn *after* the event is emitted, so
  // pub.turn still names the previous mover while an observer is running; this
  // applies the transition the driver is about to apply.
  static int nextMover(const Game& g) {
    int t = g.g.pub.turn;
    if (!g.g.pub.history.empty()) {
      const Event& e = g.g.pub.history.back();
      if ((e.kind == Kind::Ask && !e.success) || e.kind == Kind::Pass) t = e.target;
    }
    return (t >= 0 && t < NPLAY) ? t : g.g.pub.turn;
  }

  void capture(const Game& g) {
    std::lock_guard<std::mutex> lk(io.mu);
    // Display correction only: agents still observe pub.turn exactly as before.
    snap.turn = nextMover(g);
    snap.dealer = g.g.dealer;
    snap.score[0] = g.g.pub.score[0];
    snap.score[1] = g.g.pub.score[1];
    for (int p = 0; p < NPLAY; p++) {
      snap.handCount[p] = g.g.pub.handCount[p];
      snap.hand[p] = seats[p].human ? g.agents[p]->k.myHand : 0;
    }
    for (int s = 0; s < NSET; s++) { snap.setActive[s] = g.g.pub.setActive[s]; snap.setWinner[s] = g.g.setWinner[s]; }
    snap.nEvents = g.g.pub.nEvents;
    snap.nAsks = g.res.asks;
    snap.hist = g.g.pub.history;
    io.bump();
  }

  // Paces the bots so a human can follow them, and honours pause / single step.
  //
  // The delay is the time a bot takes to move, so it is charged by *who acts
  // next*, not by who just moved:
  //
  //   * you ask and miss  -> a bot is next, so it replies after the delay;
  //   * you ask and hit   -> you are next, so you may ask again at once;
  //   * a bot asks you and misses -> you are next, so you are prompted at once;
  //   * a bot asks you and hits   -> it keeps the turn, so its next ask is paced.
  //
  // Once the turn is yours there is nothing to wait for: you set your own tempo
  // and can read the board for as long as you like before acting.
  //
  // The exception is the forced endgame, where nobody is "to move" in the asking
  // sense and every event is a declaration worth watching, so the delay stands.
  //
  // Pause and single-step still apply to every seat -- pausing means the table
  // is frozen, whoever moved last.
  void paceGate(const Game& g) {
    bool humanNext = false;
    if (g.g.pub.teamAlive(0) && g.g.pub.teamAlive(1))
      humanNext = seats[nextMover(g)].human;
    std::unique_lock<std::mutex> lk(io.mu);
    for (;;) {
      if (io.abandon) throw Abandoned{};
      if (!io.paused) break;
      if (io.stepBudget > 0) { io.stepBudget--; return; }
      io.cv.wait(lk);
    }
    if (humanNext) return;
    // The delay is measured against paceMs as it stands *now*, re-read on every
    // wakeup, so dragging the pace slider down from 20s to 1s takes effect at
    // once instead of after the deadline the old value had already fixed.
    auto start = std::chrono::steady_clock::now();
    for (;;) {
      if (io.abandon) throw Abandoned{};
      if (io.paused) return;                 // pausing supersedes the delay
      long long ms = io.paceMs;
      long long el = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start).count();
      if (el >= ms) return;
      io.cv.wait_for(lk, std::chrono::milliseconds(ms - el));
    }
  }

  void gameLoop() {
    try {
      std::unique_ptr<Agent> owned[NPLAY];
      Agent* ptr[NPLAY];
      for (int p = 0; p < NPLAY; p++) {
        if (seats[p].human) {
          auto h = std::make_unique<HumanAgent>();
          h->io = &io;
          h->label = resolvedName(p);
          owned[p] = std::move(h);
        } else {
          owned[p] = makeAgent(seats[p].spec);
        }
        ptr[p] = owned[p].get();
      }
      Game game;
      game.observer = [this](const Game& g) { capture(g); paceGate(g); };
      // setup() is idempotent for a fixed seed; running it here just gives the
      // browser the opening position before run() blocks on the first decision.
      game.setup(seed, rules, ptr);
      capture(game);
      GameResult r = game.run(seed, rules, ptr);
      std::lock_guard<std::mutex> lk(io.mu);
      snap.finished = true;
      snap.running = false;
      snap.reveal = true;
      snap.hitLimit = r.hitLimit;
      snap.score[0] = r.score[0];
      snap.score[1] = r.score[1];
      snap.nAsks = r.asks;
      snap.nEvents = game.g.pub.nEvents;
      snap.hist = game.g.pub.history;
      for (int p = 0; p < NPLAY; p++) { snap.dealt[p] = game.g.dealt[p]; snap.handCount[p] = game.g.pub.handCount[p]; }
      for (int s = 0; s < NSET; s++) { snap.setActive[s] = false; snap.setWinner[s] = game.g.setWinner[s]; }
      for (int p = 0; p < NPLAY; p++) io.slot[p].need = Need::None;
      io.bump();
    } catch (const Abandoned&) {
      std::lock_guard<std::mutex> lk(io.mu);
      snap.running = false;
      io.bump();
    }
  }

  // ------------------------------------------------------------------- JSON
  static void jarr(std::ostringstream& os, const uint64_t mask) {
    os << "[";
    bool first = true;
    for (int c = 0; c < NCARD; c++) if (mask & bit(c)) { if (!first) os << ","; os << c; first = false; }
    os << "]";
  }

  std::string stateJson(int viewSeat) {
    std::lock_guard<std::mutex> lk(io.mu);
    std::ostringstream os;
    os << "{\"rev\":" << io.rev
       << ",\"running\":" << (snap.running ? "true" : "false")
       << ",\"finished\":" << (snap.finished ? "true" : "false")
       << ",\"everStarted\":" << (snap.everStarted ? "true" : "false")
       << ",\"hitLimit\":" << (snap.hitLimit ? "true" : "false")
       << ",\"paused\":" << (io.paused ? "true" : "false")
       << ",\"pace\":" << io.paceMs
       << ",\"seed\":" << snap.seed
       << ",\"deckSets\":" << snap.deckSets
       << ",\"turn\":" << snap.turn
       << ",\"dealer\":" << snap.dealer
       << ",\"nAsks\":" << snap.nAsks
       << ",\"nEvents\":" << snap.nEvents
       << ",\"score\":[" << snap.score[0] << "," << snap.score[1] << "]";

    os << ",\"handCount\":[";
    for (int p = 0; p < NPLAY; p++) { if (p) os << ","; os << snap.handCount[p]; }
    os << "],\"setActive\":[";
    for (int s = 0; s < NSET; s++) { if (s) os << ","; os << (snap.setActive[s] ? "true" : "false"); }
    os << "],\"setWinner\":[";
    for (int s = 0; s < NSET; s++) { if (s) os << ","; os << snap.setWinner[s]; }
    os << "]";

    os << ",\"seats\":[";
    for (int p = 0; p < NPLAY; p++) {
      if (p) os << ",";
      os << "{\"i\":" << p << ",\"team\":" << teamOf(p)
         << ",\"name\":" << jesc(snap.name[p])
         << ",\"label\":" << jesc(snap.label[p])
         << ",\"spec\":" << jesc(snap.spec[p])
         << ",\"human\":" << (snap.isHuman[p] ? "true" : "false") << "}";
    }
    os << "]";

    os << ",\"history\":[";
    for (size_t i = 0; i < snap.hist.size(); i++) {
      const Event& e = snap.hist[i];
      if (i) os << ",";
      const char* kind = e.kind == Kind::Ask ? "ask"
                       : e.kind == Kind::Declare ? "declare"
                       : e.kind == Kind::ForcedDeclare ? "forced"
                       : e.kind == Kind::Pass ? "pass" : "end";
      os << "{\"k\":\"" << kind << "\",\"a\":" << int(e.actor)
         << ",\"t\":" << int(e.target) << ",\"c\":" << int(e.card)
         << ",\"s\":" << int(e.set) << ",\"ok\":" << (e.success ? "true" : "false");
      if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
        os << ",\"o\":[";
        for (int j = 0; j < SETSZ; j++) { if (j) os << ","; os << int(e.decl.owner[j]); }
        os << "]";
      }
      os << ",\"n\":[";
      for (int p = 0; p < NPLAY; p++) { if (p) os << ","; os << int(e.handCount[p]); }
      os << "]}";
    }
    os << "]";

    if (snap.reveal) {
      os << ",\"deal\":[";
      for (int p = 0; p < NPLAY; p++) { if (p) os << ","; jarr(os, snap.dealt[p]); }
      os << "]";
    }

    if (viewSeat >= 0 && viewSeat < NPLAY && snap.isHuman[viewSeat]) {
      const HumanSlot& sl = io.slot[viewSeat];
      os << ",\"you\":{\"seat\":" << viewSeat << ",\"hand\":";
      jarr(os, snap.hand[viewSeat]);
      os << ",\"need\":\"" << needName(sl.need) << "\""
         << ",\"needSet\":" << sl.needSet
         << ",\"deferAll\":" << (sl.forcedDeferAll ? "true" : "false")
         << ",\"note\":" << jesc(sl.note)
         << ",\"cands\":[";
      for (int i = 0; i < sl.nCands; i++) { if (i) os << ","; os << sl.cands[i]; }
      os << "]";
      if (sl.haveDecl) {
        os << ",\"queued\":{\"set\":" << int(sl.decl.set) << ",\"owner\":[";
        for (int j = 0; j < SETSZ; j++) { if (j) os << ","; os << int(sl.decl.owner[j]); }
        os << "]}";
      } else os << ",\"queued\":null";
      os << "}";
    } else os << ",\"you\":null";

    os << "}";
    return os.str();
  }
};

} // namespace fish
