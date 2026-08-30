// KV's FishBot v0.6, played inside this engine over their JSON bridge.
//
// Source of the opponent: github.com/kv1514/fish-researchp12, branch
// claude/fishnbot-work-access-g7ciey, directory fishbot_v06/ (README.md +
// decide.py).  That package exposes their deployed v0.6 configuration
// (opponent_gamma 0.35, n_draws 480, lookahead 0.25 / depth 3 / beam 4,
// endgame_m 0) as a line-oriented JSON service on stdin/stdout.  Nothing of
// their policy is reimplemented here: this file is a translator and a pipe.
//
// NOTHING IN THIS ENGINE'S CORE IS TOUCHED BY THIS FILE.  It is additive --
// a new Agent subclass, reached only through a new `kv6` spec.  fish.hpp,
// game.hpp, belief.hpp and every v0.4-v0.7 policy header are untouched, so
// every published number this repository has measured is reproduced bit for
// bit by a build that includes this header.
//
// THE THREE THINGS THEIR README SAYS A HOST MUST GET RIGHT, and where each is
// handled:
//
//  1. Poll every seat off-turn.  This engine's Rules default
//     outOfTurnDeclare = true and Game::declarationRound already polls all six
//     seats through proposeDeclaration.  When it is not our turn we forward the
//     poll as {"op":"offturn"}; their bot answers only when the public record
//     alone pins the whole half-suit.  See proposeDeclaration below.
//
//  2. Cards are names, never integers.  Nothing here sends a card index.  More
//     than that: the half-suit INDICES also differ between the two projects,
//     which their protocol does not warn about because `half_suit` and
//     `assignment` are still exchanged as integers.  Their ordering is
//     Low C/D/H/S then High C/D/H/S; ours is Low/High per suit.  So our set 0
//     (Low Spades) is their set 3, our 6 (Low Clubs) is their 0, and inside the
//     specials our 8S,8H,8D,8C is their 8C,8D,8H,8S -- a within-set
//     permutation of [3,2,1,0,4,5].  Getting that wrong would misallocate every
//     declaration of the eights-and-jokers half-suit, legally and silently.
//     CardMap below derives the whole correspondence at startup from their own
//     {"op":"cards"} table and refuses to run if the decks disagree.
//
//  3. Never substitute a move for an error.  Every failure here -- a dead
//     process, an {"error":...} reply, an unparseable reply, a move that is
//     illegal in this engine -- calls fatal(), which prints the offending
//     request and reply and stops the run.  It never falls back to a legal
//     move, because a host that does that "will lose every game and show you
//     nothing".
#pragma once
#include "fish.hpp"
#include "game.hpp"
#include "botfault.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace fish {
namespace kv6 {

// ------------------------------------------------------------------ config
struct Config {
  std::string python;      // interpreter with numpy
  std::string dir;         // directory containing the package
  // Which decision service to launch. KV has shipped two that speak this exact
  // protocol -- fishbot_v06.decide and kraken.decide -- so the module is a
  // parameter rather than a constant.
  std::string module = "fishbot_v06.decide";
  std::string logPath;     // optional: JSONL transcript of every exchange
  bool offTurn = true;     // README requirement 1; off only to measure its cost
  // What to send as a claim event's `revealed` when a declaration was WRONG.
  // Their schema wants the true holders at resolution; this engine reveals
  // nothing on a failed declaration, so there is no truthful value available
  // to a policy.  "stop" refuses to guess (default).  "declared" knowingly
  // sends a falsehood and is for measuring what that costs, nothing else.
  std::string onFail = "stop";
  // A reply deadline, in milliseconds.  ZERO -- the default -- means "wait
  // forever", which is exactly what this bridge has always done and what every
  // measured run with `kv6`/`kraken` did, so those are unchanged bit for bit.
  //
  // It is set only for an UPLOADED package (factory.hpp's `kvbot:` branch, from
  // the manifest's timeout_ms), because that is the new situation: the bot on
  // the other end of the pipe is now somebody else's, arriving over HTTP, and a
  // hang there parks the game thread of `fish serve` forever -- with Table::stop
  // then blocking in join() while the control mutex is held, so the table
  // cannot even be abandoned.
  int timeoutMs = 0;
  // Where to send the child's stderr.  Empty keeps the old behaviour: it is
  // inherited, and lands in the host's terminal.
  std::string stderrPath;
};

[[noreturn]] inline void fatal(const std::string& what, const std::string& req,
                               const std::string& rep) {
  // An interactive table sets this (botfault.hpp): one guest bot's failure ends
  // that game with a message rather than ending the server under five other
  // people.  Off everywhere else, so every measured path still stops dead.
  if (botFaultsThrow()) throw BotFault("KV bridge", what, req, rep);
  fprintf(stderr,
          "\nfish: kv6 bridge FAILED -- refusing to substitute a move.\n"
          "  %s\n"
          "  request : %s\n"
          "  reply   : %s\n"
          "This is the request/response log KV asked for; send it to them.\n\n",
          what.c_str(), req.c_str(), rep.c_str());
  // _exit, not exit: a match runs this on a worker thread, and running static
  // and thread_local destructors from there deadlocks -- one of them waitpid()s
  // a child that is itself blocked reading a request that will never come.
  fflush(stderr);
  _exit(3);
}

// --------------------------------------------------------- tiny JSON reader
// The replies are small and come from one known producer, so a scanner is
// safer than a dependency.  Every accessor reports failure rather than
// guessing a value.
inline bool jsonHasNull(const std::string& s, const char* key) {
  std::string k = std::string("\"") + key + "\"";
  size_t p = s.find(k);
  if (p == std::string::npos) return false;
  p = s.find(':', p + k.size());
  if (p == std::string::npos) return false;
  p = s.find_first_not_of(" \t", p + 1);
  return p != std::string::npos && s.compare(p, 4, "null") == 0;
}
inline bool jsonString(const std::string& s, const char* key, std::string& out, size_t from = 0) {
  std::string k = std::string("\"") + key + "\"";
  size_t p = s.find(k, from);
  if (p == std::string::npos) return false;
  p = s.find(':', p + k.size());
  if (p == std::string::npos) return false;
  p = s.find('"', p);
  if (p == std::string::npos) return false;
  size_t q = s.find('"', p + 1);
  if (q == std::string::npos) return false;
  out = s.substr(p + 1, q - p - 1);
  return true;
}
inline bool jsonInt(const std::string& s, const char* key, long& out, size_t from = 0) {
  std::string k = std::string("\"") + key + "\"";
  size_t p = s.find(k, from);
  if (p == std::string::npos) return false;
  p = s.find(':', p + k.size());
  if (p == std::string::npos) return false;
  char* end = nullptr;
  out = strtol(s.c_str() + p + 1, &end, 10);
  return end != s.c_str() + p + 1;
}
inline bool jsonIntArray(const std::string& s, const char* key, std::vector<long>& out, size_t from = 0) {
  std::string k = std::string("\"") + key + "\"";
  size_t p = s.find(k, from);
  if (p == std::string::npos) return false;
  p = s.find('[', p + k.size());
  if (p == std::string::npos) return false;
  size_t q = s.find(']', p);
  if (q == std::string::npos) return false;
  out.clear();
  const char* c = s.c_str() + p + 1;
  const char* stop = s.c_str() + q;
  while (c < stop) {
    while (c < stop && (*c == ' ' || *c == ',')) c++;
    if (c >= stop) break;
    char* end = nullptr;
    long v = strtol(c, &end, 10);
    if (end == c) return false;
    out.push_back(v);
    c = end;
  }
  return true;
}
inline bool jsonStringArray(const std::string& s, const char* key, std::vector<std::string>& out) {
  std::string k = std::string("\"") + key + "\"";
  size_t p = s.find(k);
  if (p == std::string::npos) return false;
  p = s.find('[', p + k.size());
  if (p == std::string::npos) return false;
  size_t q = s.find(']', p);
  if (q == std::string::npos) return false;
  out.clear();
  size_t c = p + 1;
  while (c < q) {
    size_t a = s.find('"', c);
    if (a == std::string::npos || a > q) break;
    size_t b = s.find('"', a + 1);
    if (b == std::string::npos || b > q) break;
    out.push_back(s.substr(a + 1, b - a - 1));
    c = b + 1;
  }
  return true;
}

// ------------------------------------------------------------------- names
// Our deck, written in the rank-then-suit spelling their protocol uses.  Note
// "T" for the ten: fish.hpp's own cardName() spells it "10", which is not a
// name their card table contains.
inline std::string ourCardName(int c) {
  static const char* low[6]  = {"2", "3", "4", "5", "6", "7"};
  static const char* high[6] = {"9", "T", "J", "Q", "K", "A"};
  static const char* suit    = "SSHHDDCC";
  static const char* spec[6] = {"8S", "8H", "8D", "8C", "RJ", "BJ"};
  int s = setOf(c), i = c % SETSZ;
  if (s == 8) return spec[i];
  return std::string(s % 2 == 0 ? low[i] : high[i]) + suit[s];
}

// ------------------------------------------------------------------ process
struct Bridge {
  pid_t pid = -1;
  int wfd = -1;
  FILE* rf = nullptr;
  FILE* log = nullptr;
  int timeoutMs = 0;          // 0 = wait forever, as this bridge always has

  // Returns true when there is something to read, false when `ms` elapsed
  // first.  With ms == 0 it answers true immediately and the blocking fgets
  // below behaves exactly as it always did.  Used only before the FIRST read of
  // a reply, where nothing can be sitting in stdio's buffer yet.
  bool waitReadable(int ms) const {
    if (ms <= 0 || !rf) return true;
    int fd = fileno(rf);
    if (fd < 0) return true;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
      long long left = std::chrono::duration_cast<std::chrono::milliseconds>(
                         deadline - std::chrono::steady_clock::now()).count();
      if (left <= 0) return false;
      pollfd pf{fd, POLLIN, 0};
      int r = ::poll(&pf, 1, int(left > 2000 ? 2000 : left));
      if (r > 0) return true;
      if (r < 0 && errno == EINTR) continue;
      if (r < 0) return true;                  // let fgets report the real error
    }
  }

  void start(const Config& cfg) {
    // A match forks these from several worker threads at once. Without care a
    // child inherits every OTHER thread's pipe ends, so nobody ever sees EOF
    // and the whole match deadlocks with every process idle -- which is exactly
    // what happened before this was serialised. macOS has no pipe2(O_CLOEXEC),
    // so hold a lock across pipe+fork+exec and mark the ends close-on-exec;
    // dup2 clears the flag on the two we actually want the child to keep.
    ignoreSigpipeOnce();
    // Shared with every other fork site in this process; see botfault.hpp.
    std::lock_guard<std::mutex> guard(botSpawnLock());
    int toChild[2], fromChild[2];
    if (pipe(toChild) || pipe(fromChild)) fatal("could not create pipes", "", "");
    for (int fd : {toChild[0], toChild[1], fromChild[0], fromChild[1]})
      fcntl(fd, F_SETFD, FD_CLOEXEC);
    pid = fork();
    if (pid < 0) fatal("could not fork the bot process", "", "");
    if (pid == 0) {
      dup2(toChild[0], STDIN_FILENO);
      dup2(fromChild[1], STDOUT_FILENO);
      if (!cfg.stderrPath.empty()) {
        int logFd = ::open(cfg.stderrPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (logFd >= 0) { dup2(logFd, STDERR_FILENO); if (logFd > 2) close(logFd); }
      }
      close(toChild[0]); close(toChild[1]);
      close(fromChild[0]); close(fromChild[1]);
      if (chdir(cfg.dir.c_str()) != 0) _exit(127);
      execl(cfg.python.c_str(), cfg.python.c_str(), "-m", cfg.module.c_str(), (char*)nullptr);
      _exit(127);
    }
    close(toChild[0]); close(fromChild[1]);
    wfd = toChild[1];
    rf = fdopen(fromChild[0], "r");
    if (!rf) fatal("could not attach to the bot's stdout", "", "");
    if (!cfg.logPath.empty()) log = fopen(cfg.logPath.c_str(), "a");
    timeoutMs = cfg.timeoutMs;
  }

  std::string request(const std::string& line) {
    if (wfd < 0 || !rf) fatal("bridge is not running", line, "");
    std::string out = line + "\n";
    size_t off = 0;
    while (off < out.size()) {
      ssize_t n = write(wfd, out.data() + off, out.size() - off);
      if (n <= 0) fatal("the bot process closed its input (did it crash?)", line, "");
      off += size_t(n);
    }
    std::string reply;
    char buf[8192];
    if (!waitReadable(timeoutMs))
      fatal("the bot did not answer within " + std::to_string(timeoutMs) + " ms", line, "");
    if (!fgets(buf, sizeof(buf), rf)) fatal("the bot process gave no reply (did it crash?)", line, "");
    reply = buf;
    while (!reply.empty() && (reply.back() == '\n' || reply.back() == '\r')) reply.pop_back();
    // A reply longer than the buffer arrives in pieces; keep reading until the
    // braces balance rather than truncating silently.
    int depth = 0; bool inStr = false, esc = false;
    auto scan = [&](const std::string& s) {
      for (char ch : s) {
        if (esc) { esc = false; continue; }
        if (inStr) { if (ch == '\\') esc = true; else if (ch == '"') inStr = false; continue; }
        if (ch == '"') inStr = true;
        else if (ch == '{' || ch == '[') depth++;
        else if (ch == '}' || ch == ']') depth--;
      }
    };
    scan(reply);
    while (depth > 0) {
      // No deadline on the continuation reads, deliberately: fgets reads through
      // stdio's buffer, and a reply that is already sitting in that buffer would
      // make poll() on the descriptor wait for bytes that have already arrived.
      // The hang worth guarding against is "it never answered at all", which the
      // first read above covers.
      if (!fgets(buf, sizeof(buf), rf)) fatal("truncated reply from the bot process", line, reply);
      std::string more = buf;
      while (!more.empty() && (more.back() == '\n' || more.back() == '\r')) more.pop_back();
      scan(more);
      reply += more;
    }
    if (log) { fprintf(log, "%s\n%s\n", line.c_str(), reply.c_str()); fflush(log); }
    std::string err;
    if (jsonString(reply, "error", err)) fatal("the bot rejected the request: " + err, line, reply);
    return reply;
  }

  void stop() {
    if (wfd >= 0) { close(wfd); wfd = -1; }
    if (rf) { fclose(rf); rf = nullptr; }
    if (log) { fclose(log); log = nullptr; }
    if (pid > 0) {
      // With a deadline in force the child is somebody else's uploaded package,
      // and a blocking wait for one that ignores EOF holds the table's game
      // thread open forever.  Without one, this is the wait it has always been.
      if (timeoutMs > 0) {
        for (int i = 0; i < 30; i++) {
          int st = 0;
          pid_t r = ::waitpid(pid, &st, WNOHANG);
          if (r == pid || r < 0) { pid = -1; break; }
          struct timespec ts{0, 10 * 1000 * 1000};
          nanosleep(&ts, nullptr);
        }
        if (pid > 0) { ::kill(pid, SIGKILL); int st = 0; ::waitpid(pid, &st, 0); pid = -1; }
      } else {
        int st = 0; waitpid(pid, &st, 0); pid = -1;
      }
    }
  }
  ~Bridge() { stop(); }
};

// ------------------------------------------------------------------ mapping
// Built once from their own {"op":"cards"} reply, so the correspondence is
// discovered rather than assumed and a future reordering on their side cannot
// silently scramble a game.
struct CardMap {
  int ourToTheirSet[NSET];
  int theirToOurSet[NSET];
  int ourToTheirIdx[NSET][SETSZ];   // position within the half-suit
  int theirToOurIdx[NSET][SETSZ];
  bool ready = false;

  void build(Bridge& br) {
    std::string reply = br.request("{\"op\":\"cards\"}");
    std::vector<std::string> theirs;
    if (!jsonStringArray(reply, "cards", theirs) || theirs.size() != NCARD)
      fatal("their card table is not 54 names", "{\"op\":\"cards\"}", reply);
    for (int s = 0; s < NSET; s++) { ourToTheirSet[s] = -1; theirToOurSet[s] = -1; }
    for (int c = 0; c < NCARD; c++) {
      std::string name = ourCardName(c);
      int found = -1;
      for (int k = 0; k < NCARD; k++) if (theirs[size_t(k)] == name) { found = k; break; }
      if (found < 0) fatal("our card " + name + " is not in their deck", "{\"op\":\"cards\"}", reply);
      int os = setOf(c), oi = c % SETSZ, ts = found / SETSZ, ti = found % SETSZ;
      if (ourToTheirSet[os] == -1) ourToTheirSet[os] = ts;
      else if (ourToTheirSet[os] != ts)
        fatal("our half-suit " + std::to_string(os) + " straddles two of theirs",
              "{\"op\":\"cards\"}", reply);
      theirToOurSet[ts] = os;
      ourToTheirIdx[os][oi] = ti;
      theirToOurIdx[ts][ti] = oi;
    }
    for (int s = 0; s < NSET; s++)
      if (ourToTheirSet[s] < 0 || theirToOurSet[s] < 0)
        fatal("half-suit correspondence is not a bijection", "{\"op\":\"cards\"}", reply);
    ready = true;
  }
};

// One process per thread PER SERVICE.  These bots are stateless -- every request
// carries the whole public record -- so the three seats of a team share one
// interpreter safely, and a thread pays the ~0.4s numpy startup once.  Keyed by
// directory and module so a table that seats two different KV bots at once gets
// two processes rather than sending one of them the other's requests.
inline Bridge& threadBridge(const Config& cfg, CardMap*& map) {
  struct Entry { Bridge br; CardMap cm; };
  static thread_local std::map<std::string, std::unique_ptr<Entry>> pool;
  const std::string key = cfg.dir + "|" + cfg.module;
  auto it = pool.find(key);
  if (it == pool.end()) {
    auto e = std::make_unique<Entry>();
    e->br.start(cfg);
    e->cm.build(e->br);
    it = pool.emplace(key, std::move(e)).first;
  }
  map = &it->second->cm;
  return it->second->br;
}

// -------------------------------------------------------------------- agent
struct KV6Agent : Agent {
  Config cfg;
  std::string label = "kv6";
  uint64_t myHand = 0;
  int deckSets = NSET;
  int setWinner[NSET];          // -1 unresolved, else the team that took it
  int revealedSet = -1;         // the half-suit Game::observeResolution just reported
  uint8_t revealedOwner[SETSZ] = {0,0,0,0,0,0};
  std::vector<std::string> events;   // our public record, in their spelling

  const char* name() const override { return label.c_str(); }

  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    myHand = hand;
    deckSets = r.deckSets;
    for (int i = 0; i < NSET; i++) setWinner[i] = -1;
    events.clear();
    (void)seed;
  }

  // Game::applyDeclaration hands us the true holders out of band, immediately
  // before the resolving event.  This is the only thing in this integration
  // that the host engine had to grow, and it exists because their ClaimEvent
  // has no way to say "resolved, holders unrevealed".
  void observeResolution(int set, const uint8_t* trueOwner) override {
    revealedSet = set;
    for (int i = 0; i < SETSZ; i++) revealedOwner[i] = trueOwner[i];
  }

  void observe(const Event& e) override {
    Agent::observe(e);
    CardMap* map = nullptr;
    Bridge& br = threadBridge(cfg, map);
    (void)br;
    char buf[512];
    if (e.kind == Kind::Ask) {
      if (e.success) {
        if (e.actor == seat) myHand |= bit(e.card);
        else if (e.target == seat) myHand &= ~bit(e.card);
      }
      snprintf(buf, sizeof(buf),
               "{\"t\":\"ask\",\"asker\":%d,\"target\":%d,\"card\":\"%s\",\"success\":%s}",
               int(e.actor), int(e.target), ourCardName(e.card).c_str(),
               e.success ? "true" : "false");
      events.push_back(buf);
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      myHand &= ~setMask(e.decl.set);
      int team = teamOf(e.actor);
      int winner = e.success ? team : 1 - team;
      setWinner[e.decl.set] = winner;
      // Their ClaimEvent wants the holders ACTUALLY revealed at resolution.
      // When the declaration was correct those are the declared holders and we
      // can say so truthfully.  When it was wrong this engine reveals nothing
      // -- Knowledge::onEvent only learns from a correct declaration -- so no
      // policy here can know them, and there is no way to express "resolved,
      // holders unknown" in their schema.  We refuse to invent them.
      // Emit in THEIR within-set order: position j of their half-suit is our
      // position theirToOurIdx[theirSet][j].
      std::string declared = "[";
      int ts = map->ourToTheirSet[e.decl.set];
      for (int j = 0; j < SETSZ; j++) {
        int oi = map->theirToOurIdx[ts][j];
        declared += (j ? "," : "") + std::to_string(int(e.decl.owner[oi]));
      }
      declared += "]";
      // When the declaration was correct the declared holders ARE the true
      // ones.  When it was wrong they are not, and inventing them poisons the
      // bot's constraint store -- so use what the engine just told us.
      std::string revealed = declared;
      if (!e.success) {
        if (cfg.onFail == "declared") {
          // Knowingly lossy. Retained only to measure what the falsehood costs;
          // never quote a result produced with it.
        } else if (revealedSet == int(e.decl.set)) {
          revealed = "[";
          for (int j = 0; j < SETSZ; j++) {
            int oi = map->theirToOurIdx[ts][j];
            revealed += (j ? "," : "") + std::to_string(int(revealedOwner[oi]));
          }
          revealed += "]";
        } else {
          fatal("a declaration was WRONG and the engine did not report the true "
                "holders for it -- Game::observeResolution did not fire for this "
                "half-suit, so the bridge has nothing truthful to send.",
                "declare set " + std::to_string(int(e.decl.set)) + " by seat " +
                    std::to_string(int(e.actor)) + " (incorrect)",
                "declared=" + declared);
        }
      }
      snprintf(buf, sizeof(buf),
               "{\"t\":\"claim\",\"claimer\":%d,\"half_suit\":%d,\"declared\":%s,"
               "\"revealed\":%s,\"winner\":%d}",
               int(e.actor), ts, declared.c_str(), revealed.c_str(), winner);
      events.push_back(buf);
    } else if (e.kind == Kind::Pass) {
      snprintf(buf, sizeof(buf), "{\"t\":\"pass\",\"player\":%d,\"teammate\":%d}",
               int(e.actor), int(e.target));
      events.push_back(buf);
    }
  }

  // `turnOverride` states whose move it is. In the forced endgame this engine
  // has handed the move to this seat without moving pub.turn -- their protocol
  // requires turn == seat for a decide, and saying so is the truth about who is
  // being asked to act, not a fiction.
  std::string buildRequest(const PublicState& pub, const char* op, int turnOverride = -1) const {
    CardMap* map = nullptr;
    Config c = cfg;
    Bridge& br = threadBridge(c, map);
    (void)br;
    std::string r = "{\"op\":\"";
    r += op;
    r += "\",\"seat\":" + std::to_string(seat);
    r += ",\"turn\":" + std::to_string(turnOverride >= 0 ? turnOverride : pub.turn);
    r += ",\"hand\":[";
    bool first = true;
    for (int c2 = 0; c2 < NCARD; c2++) {
      if (!(myHand & bit(c2))) continue;
      r += (first ? "\"" : ",\"");
      r += ourCardName(c2);
      r += "\"";
      first = false;
    }
    r += "],\"hand_counts\":[";
    for (int p = 0; p < NPLAY; p++) r += (p ? "," : "") + std::to_string(int(pub.handCount[p]));
    // set_winner is indexed in THEIR half-suit numbering.
    r += "],\"set_winner\":[";
    for (int ts = 0; ts < NSET; ts++) {
      int os = map->theirToOurSet[ts];
      r += (ts ? "," : "");
      r += (os >= 0 && setWinner[os] >= 0) ? std::to_string(setWinner[os]) : "null";
    }
    r += "],\"history\":[";
    for (size_t i = 0; i < events.size(); i++) r += (i ? "," : "") + events[i];
    r += "],\"rules\":{\"wrong_distribution_outcome\":\"opponent\"}}";
    return r;
  }

  // Their `assignment` is in their within-set order; ours is not.
  void decodeDeclaration(const std::string& reply, const std::string& req, Declaration& d) const {
    CardMap* map = nullptr;
    Config c = cfg;
    threadBridge(c, map);
    long theirSet = 0;
    std::vector<long> assign;
    if (!jsonInt(reply, "half_suit", theirSet) || theirSet < 0 || theirSet >= NSET)
      fatal("declaration without a usable half_suit", req, reply);
    if (!jsonIntArray(reply, "assignment", assign) || assign.size() != SETSZ)
      fatal("declaration without six assigned holders", req, reply);
    int os = map->theirToOurSet[theirSet];
    if (os < 0) fatal("declaration names a half-suit we cannot place", req, reply);
    d.set = uint8_t(os);
    for (int j = 0; j < SETSZ; j++) {
      int oi = map->theirToOurIdx[theirSet][j];
      if (assign[size_t(j)] < 0 || assign[size_t(j)] >= NPLAY)
        fatal("declaration names a seat out of range", req, reply);
      d.owner[oi] = uint8_t(assign[size_t(j)]);
    }
  }

  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    CardMap* map = nullptr;
    Bridge& br = threadBridge(cfg, map);
    // README requirement 1.  On our turn the full policy runs and may return a
    // declaration; off-turn we forward the poll to their proof-only channel.
    const bool mine = (pub.turn == seat);
    if (!mine && !cfg.offTurn) return false;
    // Their bot does not implement cardlessMayDeclare for the on-turn loop.
    if (mine && !pub.handCount[seat]) return false;
    std::string req = buildRequest(pub, mine ? "decide" : "offturn");
    std::string reply = br.request(req);
    if (jsonHasNull(reply, "action")) return false;
    std::string type;
    if (!jsonString(reply, "type", type)) fatal("reply has no action type", req, reply);
    if (type != "declare") return false;          // an ask; chooseAsk will take it
    decodeDeclaration(reply, req, d);
    conf = 1.0;                                   // they expose no confidence
    return true;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    CardMap* map = nullptr;
    Bridge& br = threadBridge(cfg, map);
    std::string req = buildRequest(pub, "decide");
    std::string reply = br.request(req);
    std::string type;
    if (jsonHasNull(reply, "action") || !jsonString(reply, "type", type))
      fatal("no action for a seat that must move", req, reply);
    if (type != "ask") fatal("expected an ask, got a " + type, req, reply);
    std::string card;
    long target = -1;
    if (!jsonString(reply, "card", card) || !jsonInt(reply, "target", target))
      fatal("ask without a card and a target", req, reply);
    int c = -1;
    for (int i = 0; i < NCARD; i++) if (ourCardName(i) == card) { c = i; break; }
    if (c < 0) fatal("ask names a card we do not have: " + card, req, reply);
    if (target < 0 || target >= NPLAY) fatal("ask names a seat out of range", req, reply);
    // README requirement 3: an illegal move is a bridge defect, and the driver
    // would quietly replace it with buf[0].  Catch it here instead.
    if (!legalAskShape(pub, myHand, seat, c, int(target)))
      fatal("their ask is illegal in this engine -- the translation is wrong", req, reply);
    return AskMove{uint8_t(c), uint8_t(target)};
  }

  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    CardMap* map = nullptr;
    Bridge& br = threadBridge(cfg, map);
    std::string req = buildRequest(pub, "decide");
    std::string reply = br.request(req);
    std::string type;
    long teammate = -1;
    if (!jsonHasNull(reply, "action") && jsonString(reply, "type", type) && type == "pass" &&
        jsonInt(reply, "teammate", teammate)) {
      for (int i = 0; i < n; i++) if (cand[i] == int(teammate)) return int(teammate);
    }
    fatal("a pass was required and their reply did not name a legal teammate", req, reply);
  }

  // The forced endgame has no op of its own in their protocol, but it does not
  // need one: this engine has given the seat the move, so the honest request is
  // an ordinary `decide` with turn == seat, which is what their own engine runs
  // in its forced-claims phase.  Asking the proof-only off-turn channel instead
  // was wrong -- it declines whenever the public record does not pin the whole
  // half-suit, which in an endgame split across three teammates is most of the
  // time, and the engine then falls through to its own all-to-me default and
  // declares something guaranteed wrong in their name.
  //
  // The engine overwrites d.set with the half-suit it asked about, so a
  // declaration naming a DIFFERENT half-suit must be declined rather than
  // reshaped: the loop comes back round for the set they actually want.
  bool declareFor(const PublicState& pub, int set, Declaration& d) {
    CardMap* map = nullptr;
    Bridge& br = threadBridge(cfg, map);
    std::string req = buildRequest(pub, "decide", seat);
    std::string reply = br.request(req);
    if (jsonHasNull(reply, "action")) return false;
    std::string type;
    if (!jsonString(reply, "type", type) || type != "declare") return false;
    Declaration got{};
    decodeDeclaration(reply, req, got);
    if (int(got.set) != set) return false;
    d = got;
    return true;
  }

  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf,
                     double threshold) override {
    if (!declareFor(pub, set, d)) return false;
    conf = 1.0;                       // they expose no confidence of their own
    return conf >= threshold;
  }
  // The engine's last resort: somebody on this team must declare this set now.
  // Their protocol has no forced-declaration op, so we ask the proof-only
  // channel and use its answer when it has one.  When it does not, we leave the
  // base class's guess and count it, because inventing an allocation on their
  // behalf and reporting it as theirs would misattribute the error.
  static long& forcedFallbacks() { static long n = 0; return n; }

  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    if (declareFor(pub, set, d)) { conf = 1.0; return; }
    // Their bot will not name this half-suit even as a last resort. The engine
    // still needs six holders, and the base class names them all to this seat,
    // which is almost always wrong. Counted, because a declaration this engine
    // invented must not be read as one of theirs.
    forcedFallbacks()++;
    Agent::bestGuess(pub, set, d, conf);
  }
};

}  // namespace kv6
}  // namespace fish
