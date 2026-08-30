// The FishLab Bot Protocol, and the agent that speaks it.
//
// An uploaded package (botpkg.hpp) is launched as its own process and asked for
// one decision at a time over line-delimited JSON on stdin/stdout.  That is the
// whole interface: a bot is a program that reads a game state and writes a move,
// in whatever language its author likes, and nothing about it is compiled into
// this engine.  docs/BOT_PACKAGE.md is the author-facing specification and this
// file is its implementation; if the two disagree, the document is the contract
// and this is the bug.
//
// Two decisions worth knowing about before reading the rest:
//
//  * EVERY REQUEST CARRIES THE WHOLE PUBLIC RECORD.  A bot never has to track
//    the game to answer a question about it, which removes the entire class of
//    bug where the host and the guest disagree about what has happened.  A bot
//    that wants incremental state may keep it -- it gets its own process for the
//    life of the seat -- but it must never require it.
//
//  * A BAD REPLY IS NEVER REPLACED BY A LEGAL MOVE.  An unparseable answer, an
//    illegal ask, an allocation naming the wrong team: each one stops the game
//    and reports the request and the reply.  The alternative -- quietly
//    substituting something legal -- produces a bot that loses every game for
//    reasons its author cannot see, which is the single most useless thing a
//    host can do to a guest.  (kv6.hpp says the same thing at more length; the
//    difference here is that an interactive table raises instead of exiting, so
//    one broken bot ends one game rather than the server.)
//
// Nothing in this file is included by a measured path: factory.hpp reaches it
// only through the `bot:` spec, which no experiment, script or frozen
// configuration in this repository uses.
#pragma once
#include "fish.hpp"
#include "game.hpp"
#include "botfault.hpp"
#include "botpkg.hpp"
#include "minijson.hpp"
#include <string>
#include <vector>
#include <exception>
#include <mutex>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace fish {
namespace extbot {

static constexpr const char* PROTOCOL = "fishlab-json-v1";

// Everything that can go wrong with a foreign bot is reported with the request
// and the reply attached, because the person who has to fix it is usually not
// the person running the table.  The type is in botfault.hpp because kv6.hpp
// raises the same one.
using fish::BotFault;

// ------------------------------------------------------------- card naming
// The protocol spells cards, never indexes them -- the one lesson from bridging
// KV's engine that generalises (kv6.hpp, requirement 2): two projects that
// exchange integers will eventually disagree about what the integers mean, and
// the disagreement is silent.  "T" for the ten because fish.hpp's cardName()
// spells it "10", which is two characters and invites a parser bug.
inline std::string cardCode(int c) {
  static const char* low[6]  = {"2", "3", "4", "5", "6", "7"};
  static const char* high[6] = {"9", "T", "J", "Q", "K", "A"};
  static const char* suit    = "SSHHDDCC";
  static const char* spec[6] = {"8S", "8H", "8D", "8C", "RJ", "BJ"};
  int s = setOf(c), i = c % SETSZ;
  if (s == 8) return spec[i];
  return std::string(s % 2 == 0 ? low[i] : high[i]) + suit[s];
}
inline int cardFromCode(const std::string& n) {
  for (int c = 0; c < NCARD; c++) if (cardCode(c) == n) return c;
  return -1;
}

// ------------------------------------------------------------------ process
// One child per agent, spawned on first use.  Per agent rather than pooled by
// package: the arena builds its agents once per worker thread and reuses them
// across games, so the process count is bounded either way, and a bot that
// keeps state between decisions then keeps the RIGHT state -- its own seat's.
struct Proc {
  pid_t pid = -1;
  int wfd = -1, rfd = -1;
  std::string buf;              // bytes read past the end of the last reply
  bool dead = false;

  Proc() = default;
  // It owns two descriptors and a child; a copy would close both twice.
  Proc(const Proc&) = delete;
  Proc& operator=(const Proc&) = delete;

  bool start(const botpkg::Installed& pkg, std::string& err) {
    std::vector<std::string> argv = pkg.argv();
    if (argv.empty()) { err = "the manifest has an empty \"run\""; return false; }

    // Without this a write to a dead bot kills the whole engine before the
    // EPIPE branch below can report it.  See botfault.hpp.
    ignoreSigpipeOnce();
    // Same hazard kv6.hpp documents: a match forks these from several threads at
    // once, and without O_CLOEXEC a child inherits every other thread's pipe
    // ends, so nobody ever sees EOF and the whole run deadlocks with every
    // process idle.  macOS has no pipe2(), so the flag is set between pipe()
    // and fork() under a lock that no other thread can fork inside -- and the
    // lock is shared with every other fork site in this process (botfault.hpp),
    // because a lock per site excludes nobody.
    std::lock_guard<std::mutex> guard(botSpawnLock());
    int toChild[2], fromChild[2];
    if (pipe(toChild)) { err = "could not create a pipe"; return false; }
    if (pipe(fromChild)) { close(toChild[0]); close(toChild[1]); err = "could not create a pipe"; return false; }
    for (int fd : {toChild[0], toChild[1], fromChild[0], fromChild[1]}) fcntl(fd, F_SETFD, FD_CLOEXEC);

    pid = fork();
    if (pid < 0) {
      close(toChild[0]); close(toChild[1]); close(fromChild[0]); close(fromChild[1]);
      err = "could not fork the bot process";
      return false;
    }
    if (pid == 0) {
      dup2(toChild[0], STDIN_FILENO);
      dup2(fromChild[1], STDOUT_FILENO);
      // The bot's own diagnostics go to a file it can be shown afterwards,
      // rather than into the host's terminal in the middle of a game.
      int logFd = ::open(pkg.logPath().c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (logFd >= 0) { dup2(logFd, STDERR_FILENO); if (logFd > 2) close(logFd); }
      close(toChild[0]); close(toChild[1]); close(fromChild[0]); close(fromChild[1]);
      // Line buffering on a pipe is the classic hang: a Python bot that prints
      // its reply and waits would block-buffer it and both sides would sit
      // there.  The manifest may override this, but it has to do so knowingly.
      setenv("PYTHONUNBUFFERED", "1", 1);
      setenv("FISHLAB_PROTOCOL", PROTOCOL, 1);
      for (const auto& kv : pkg.man.env) setenv(kv.first.c_str(), kv.second.c_str(), 1);
      if (chdir(pkg.dir.c_str()) != 0) _exit(127);
      std::vector<char*> cargv;
      for (auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
      cargv.push_back(nullptr);
      execvp(cargv[0], cargv.data());
      _exit(127);
    }
    close(toChild[0]);
    close(fromChild[1]);
    wfd = toChild[1];
    rfd = fromChild[0];
    return true;
  }

  bool write(const std::string& line, std::string& err) {
    if (wfd < 0 || dead) { err = "the bot process is not running"; return false; }
    std::string out = line + "\n";
    size_t off = 0;
    while (off < out.size()) {
      ssize_t n = ::write(wfd, out.data() + off, out.size() - off);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) { dead = true; err = "the bot process closed its input (did it crash?)"; return false; }
      off += size_t(n);
    }
    return true;
  }

  // One reply, or a timeout.  A bot that never answers must not be able to
  // freeze a table full of people, so this is a deadline and not a blocking
  // read -- the one place this bridge is stricter than kv6.hpp.
  bool readLine(int timeoutMs, std::string& out, std::string& err) {
    out.clear();
    if (rfd < 0 || dead) { err = "the bot process is not running"; return false; }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;) {
      size_t nl = buf.find('\n');
      if (nl != std::string::npos) {
        out = buf.substr(0, nl);
        buf.erase(0, nl + 1);
        while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) out.pop_back();
        return true;
      }
      if (buf.size() > 8u * 1024 * 1024) { dead = true; err = "the bot sent a reply with no end to it"; return false; }
      long long left = std::chrono::duration_cast<std::chrono::milliseconds>(
                         deadline - std::chrono::steady_clock::now()).count();
      if (left <= 0) {
        err = "the bot did not answer within " + std::to_string(timeoutMs) + " ms";
        dead = true;
        return false;
      }
      pollfd pf{rfd, POLLIN, 0};
      int pr = ::poll(&pf, 1, int(left > 2000 ? 2000 : left));
      if (pr < 0) {
        if (errno == EINTR) continue;
        dead = true;
        err = "lost the pipe to the bot process";
        return false;
      }
      if (pr == 0) continue;                       // nothing yet; the deadline decides
      char tmp[8192];
      ssize_t n = ::read(rfd, tmp, sizeof(tmp));
      if (n == 0) { dead = true; err = "the bot process exited without answering"; return false; }
      if (n < 0) {
        if (errno == EINTR) continue;
        dead = true;
        err = "could not read from the bot process";
        return false;
      }
      buf.append(tmp, size_t(n));
    }
  }

  void stop() {
    if (wfd >= 0) { close(wfd); wfd = -1; }         // EOF on stdin: the polite exit
    if (pid > 0) {
      // A bot that does not take the hint gets 300 ms and then a signal; a
      // table must not be held open by somebody else's infinite loop.
      for (int i = 0; i < 30; i++) {
        int st = 0;
        pid_t r = ::waitpid(pid, &st, WNOHANG);
        if (r == pid) { pid = -1; break; }
        if (r < 0) { pid = -1; break; }
        struct timespec ts{0, 10 * 1000 * 1000};
        nanosleep(&ts, nullptr);
      }
      if (pid > 0) {
        ::kill(pid, SIGKILL);
        int st = 0;
        ::waitpid(pid, &st, 0);
        pid = -1;
      }
    }
    if (rfd >= 0) { close(rfd); rfd = -1; }
    dead = true;
  }
  ~Proc() { stop(); }
};

// -------------------------------------------------------------------- agent
struct ExternAgent : Agent {
  botpkg::Installed pkg;
  std::string label = "bot";

  // Set when the spec named a package that is not installed.  Reported through
  // the same fault path as everything else rather than at construction, because
  // makeAgent has no way to refuse: a missing bot must read the same as a broken
  // one, and must not take down a table full of people on the way.
  std::string missing;

  Proc proc;
  bool started = false;
  uint64_t myHand = 0;
  int deckSets = NSET;
  int setWinner[NSET];
  std::vector<std::string> events;      // the public record, pre-rendered
  int nAsks = 0;                        // counted here: see stateJson
  Rules rules;

  // What the bot was actually asked, and how long it took.  Counted for the
  // self-check report: "your bot answered 34 asks and 3 forced declarations" is
  // the difference between an author knowing their forced-endgame branch ran
  // and an author assuming it did.
  struct Counts {
    long hello = 0, newGame = 0, ask = 0, poll = 0, pass = 0, forced = 0;
    long declared = 0, fallbacks = 0;
    long long micros = 0;
    long calls() const { return hello + newGame + ask + poll + pass + forced; }
  } counts;

  const char* name() const override { return label.c_str(); }

  [[noreturn]] void fault(const std::string& what, const std::string& req, const std::string& rep) {
    std::string log = pkg.logPath();
    // botFaultsThrow() is set by `fish serve` and by nothing else: a fault ends
    // the game with a message at an interactive table, and ends the process in
    // a batch run, where a measured number must never come from a bot whose
    // move was substituted.
    if (botFaultsThrow()) {
      BotFault f(label, what + "  (its own output is in " + log + ")", req, rep);
      proc.stop();
      started = false;
      throw f;
    }
    fprintf(stderr,
            "\nfish: bot '%s' FAILED -- refusing to substitute a move.\n"
            "  %s\n"
            "  request : %s\n"
            "  reply   : %s\n"
            "  the bot's own stderr is in %s\n\n",
            label.c_str(), what.c_str(), req.c_str(), rep.c_str(), log.c_str());
    fflush(stderr);
    // _exit rather than exit, for the reason kv6.hpp gives: a match runs this on
    // a worker thread, and running static destructors from there deadlocks on a
    // waitpid for a child that is itself blocked on a request never coming.
    _exit(3);
  }

  void ensureStarted() {
    if (!missing.empty()) fault(missing, "", "");
    if (started && !proc.dead) return;
    if (started) fault("the bot process has died", "", "");
    std::string err;
    if (!proc.start(pkg, err)) fault(err, "", "");
    started = true;
    handshake();
  }

  std::string request(const std::string& req) {
    ensureStarted();
    std::string err, reply;
    auto t0 = std::chrono::steady_clock::now();
    if (!proc.write(req, err)) fault(err, req, "");
    if (!proc.readLine(pkg.man.timeoutMs, reply, err)) fault(err, req, "");
    counts.micros += std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now() - t0).count();
    mj::Value v;
    std::string perr;
    if (!mj::parse(reply, v, perr)) fault("the reply is not valid JSON: " + perr, req, reply);
    if (!v.isObj()) fault("the reply is not a JSON object", req, reply);
    const mj::Value* e = v.find("error");
    if (e && e->isStr()) fault("the bot rejected the request: " + e->str, req, reply);
    return reply;
  }

  // The handshake states the protocol in both directions and gives the bot one
  // authoritative copy of the deck, so no author has to hardcode a card order.
  void handshake() {
    std::string req = "{\"op\":\"hello\",\"protocol\":\"";
    req += PROTOCOL;
    req += "\",\"engine\":\"fishlab\",\"seats\":6,\"set_size\":6,\"timeout_ms\":" +
           std::to_string(pkg.man.timeoutMs) + ",\"cards\":[";
    for (int c = 0; c < NCARD; c++) req += (c ? ",\"" : "\"") + cardCode(c) + "\"";
    req += "],\"sets\":[";
    for (int s = 0; s < NSET; s++) req += std::string(s ? "," : "") + mj::escape(setName(s));
    req += "]}";
    std::string err, reply;
    if (!proc.write(req, err)) fault(err, req, "");
    if (!proc.readLine(pkg.man.timeoutMs, reply, err))
      fault(std::string(err) + " -- it never answered the handshake, so it may not have started at all",
            req, "");
    mj::Value v;
    std::string perr;
    if (!mj::parse(reply, v, perr)) fault("the handshake reply is not valid JSON: " + perr, req, reply);
    if (!v.isObj()) fault("the handshake reply is not a JSON object", req, reply);
    // request() checks this for every other op; the handshake does not go
    // through request() (it must run before the process is marked started), so
    // the check is repeated here rather than left as a hole the protocol
    // document does not describe.
    { const mj::Value* e = v.find("error");
      if (e && e->isStr()) fault("it refused the handshake: " + e->str, req, reply); }
    counts.hello++;
    std::string got = v.s("protocol", PROTOCOL);
    if (got != PROTOCOL)
      fault("it speaks protocol '" + botpkg::clip(got, 40) + "'; this engine speaks '" + PROTOCOL + "'",
            req, reply);
  }

  // ------------------------------------------------------------ observation
  void reset(int s, uint64_t hand, const Rules& r, uint64_t seed) override {
    Agent::reset(s, hand, r, seed);
    myHand = hand;
    deckSets = r.deckSets;
    rules = r;
    for (int i = 0; i < NSET; i++) setWinner[i] = -1;
    events.clear();
    nAsks = 0;
    // A fresh deal is announced so a bot that caches per game knows to drop it.
    // Sent lazily with the first decision rather than here, because reset() runs
    // for six seats before the first move and a table that has not started yet
    // should not have six interpreters warming up.
    newGamePending = true;
  }
  bool newGamePending = false;

  void sendNewGameIfNeeded() {
    if (!newGamePending) return;
    newGamePending = false;
    std::string req = "{\"op\":\"new_game\",\"seat\":" + std::to_string(seat) +
                      ",\"deck_sets\":" + std::to_string(deckSets) +
                      ",\"hand\":[";
    bool first = true;
    for (int c = 0; c < NCARD; c++) {
      if (!(myHand & bit(c))) continue;
      req += (first ? "\"" : ",\"") + cardCode(c) + "\"";
      first = false;
    }
    req += "],\"rules\":" + rulesJson() + "}";
    counts.newGame++;
    request(req);
  }

  std::string rulesJson() const {
    return std::string("{\"out_of_turn_declare\":") + (rules.outOfTurnDeclare ? "true" : "false") +
           ",\"cardless_may_declare\":" + (rules.cardlessMayDeclare ? "true" : "false") +
           ",\"max_asks\":" + std::to_string(rules.maxAsks) +
           ",\"deck_sets\":" + std::to_string(deckSets) + "}";
  }

  void observe(const Event& e) override {
    Agent::observe(e);
    char buf[512];
    if (e.kind == Kind::Ask) {
      nAsks++;
      if (e.success) {
        if (e.actor == seat) myHand |= bit(e.card);
        else if (e.target == seat) myHand &= ~bit(e.card);
      }
      snprintf(buf, sizeof(buf),
               "{\"t\":\"ask\",\"actor\":%d,\"target\":%d,\"card\":\"%s\",\"success\":%s",
               int(e.actor), int(e.target), cardCode(e.card).c_str(), e.success ? "true" : "false");
      events.push_back(std::string(buf) + countsJson(e) + "}");
    } else if (e.kind == Kind::Declare || e.kind == Kind::ForcedDeclare) {
      myHand &= ~setMask(e.decl.set);
      int team = teamOf(e.actor);
      int winner = e.success ? team : 1 - team;
      setWinner[e.decl.set] = winner;
      // `owner` is what the declarer CLAIMED, which is public whether or not it
      // was right; `success` says whether it was.  A wrong declaration reveals
      // nothing else in this engine -- exactly what a person at the table sees
      // -- so unlike the KV dialect there is no true-holder field to fill in and
      // no reason for the engine core to hand one out.
      std::string own = "[";
      for (int j = 0; j < SETSZ; j++) own += (j ? "," : "") + std::to_string(int(e.decl.owner[j]));
      own += "]";
      snprintf(buf, sizeof(buf),
               "{\"t\":\"declare\",\"actor\":%d,\"set\":%d,\"forced\":%s,\"success\":%s,\"winner\":%d",
               int(e.actor), int(e.decl.set), e.kind == Kind::ForcedDeclare ? "true" : "false",
               e.success ? "true" : "false", winner);
      events.push_back(std::string(buf) + ",\"owner\":" + own + countsJson(e) + "}");
    } else if (e.kind == Kind::Pass) {
      snprintf(buf, sizeof(buf), "{\"t\":\"pass\",\"actor\":%d,\"target\":%d",
               int(e.actor), int(e.target));
      events.push_back(std::string(buf) + countsJson(e) + "}");
    }
  }

  static std::string countsJson(const Event& e) {
    std::string o = ",\"counts\":[";
    for (int p = 0; p < NPLAY; p++) o += (p ? "," : "") + std::to_string(int(e.handCount[p]));
    return o + "]";
  }

  // ------------------------------------------------------------------ state
  // `turnOverride` says whose move it is when the engine has handed this seat
  // the move without moving pub.turn, which is what the forced endgame does.
  std::string stateJson(const PublicState& pub, int turnOverride = -1) const {
    std::string r = "{\"seat\":" + std::to_string(seat) +
                    ",\"turn\":" + std::to_string(turnOverride >= 0 ? turnOverride : pub.turn) +
                    ",\"deck_sets\":" + std::to_string(deckSets) + ",\"hand\":[";
    bool first = true;
    for (int c = 0; c < NCARD; c++) {
      if (!(myHand & bit(c))) continue;
      r += (first ? "\"" : ",\"") + cardCode(c) + "\"";
      first = false;
    }
    r += "],\"hand_counts\":[";
    for (int p = 0; p < NPLAY; p++) r += (p ? "," : "") + std::to_string(int(pub.handCount[p]));
    r += "],\"score\":[" + std::to_string(int(pub.score[0])) + "," + std::to_string(int(pub.score[1])) + "]";
    r += ",\"set_active\":[";
    for (int s = 0; s < NSET; s++) r += std::string(s ? "," : "") + (pub.setActive[s] ? "true" : "false");
    r += "],\"set_winner\":[";
    for (int s = 0; s < NSET; s++)
      r += std::string(s ? "," : "") + (setWinner[s] >= 0 ? std::to_string(setWinner[s]) : "null");
    // Counted from the events this seat has observed, NOT from
    // PublicState::nAsks: that field exists in fish.hpp but Game never writes
    // it (the driver keeps its count in GameResult::asks), so reporting it sent
    // every bot a permanent zero next to a max_asks of 400.
    r += "],\"n_asks\":" + std::to_string(nAsks);
    r += ",\"rules\":" + rulesJson();
    r += ",\"history\":[";
    for (size_t i = 0; i < events.size(); i++) r += (i ? "," : "") + events[i];
    return r + "]}";
  }

  // ------------------------------------------------------------- decoding
  // A declaration has to be well formed before the engine sees it: six seats,
  // all on the declaring seat's own team, for a half-suit still in play.
  //
  // Checked HERE rather than left to the driver because the driver's answer is
  // silence: declarationRound() and forcedEndgame() both `continue` past an
  // allocation that names the wrong team, so a bot with its seats transposed
  // would present as a bot that has decided never to declare.  That is a far
  // harder thing for its author to debug than a message naming the seat.
  void decodeDeclaration(const mj::Value& v, const std::string& req, const std::string& reply,
                         Declaration& d, const PublicState& pub) {
    const mj::Value* setv = v.find("set");
    const mj::Value* own = v.find("owner");
    if (!setv || !setv->isNum()) fault("a declaration with no \"set\"", req, reply);
    int s = int(setv->num);
    if (s < 0 || s >= NSET) fault("a declaration names half-suit " + std::to_string(s), req, reply);
    if (!pub.setActive[s]) fault("a declaration names half-suit " + std::to_string(s) +
                                 ", which is already out of play", req, reply);
    if (!own || !own->isArr() || own->arr.size() != SETSZ)
      fault("a declaration needs an \"owner\" array of six seats", req, reply);
    d.set = uint8_t(s);
    for (int i = 0; i < SETSZ; i++) {
      const mj::Value& q = own->arr[size_t(i)];
      if (!q.isNum()) fault("an \"owner\" entry is not a seat number", req, reply);
      int p = int(q.num);
      if (p < 0 || p >= NPLAY) fault("an \"owner\" entry names seat " + std::to_string(p), req, reply);
      if (teamOf(p) != teamOf(seat))
        fault("an \"owner\" entry names seat " + std::to_string(p) +
              ", who is on the other team -- a declaration allocates the half-suit "
              "among your OWN three seats", req, reply);
      d.owner[i] = uint8_t(p);
    }
  }

  // Clamped, never promoted.  Rewriting an out-of-range confidence to 1.0 would
  // turn -1 -- the obvious spelling of "I have no idea", and this engine's own
  // internal sentinel for it -- into maximum confidence, which in the forced
  // ladder means declaring at the very first and strictest rung.
  static double clampConfidence(double c) {
    if (!(c == c)) return 0.0;                 // NaN
    return c < 0 ? 0.0 : (c > 1 ? 1.0 : c);
  }

  static bool actionIs(const mj::Value& v, const char* what) {
    const mj::Value* a = v.find("action");
    return a && a->isStr() && a->str == what;
  }
  static bool actionIsNone(const mj::Value& v) {
    const mj::Value* a = v.find("action");
    if (!a) return true;
    if (a->isNull()) return true;
    return a->isStr() && (a->str == "none" || a->str.empty());
  }

  // ------------------------------------------------------------- decisions
  bool proposeDeclaration(const PublicState& pub, Declaration& d, double& conf) override {
    const bool mine = (pub.turn == seat);
    if (!mine && !pkg.man.pollOffTurn) return false;
    sendNewGameIfNeeded();
    counts.poll++;
    std::string req = "{\"op\":\"declare_poll\",\"state\":" + stateJson(pub) + "}";
    std::string reply = request(req);
    mj::Value v;
    std::string perr;
    mj::parse(reply, v, perr);                   // request() already validated it
    if (actionIsNone(v)) return false;
    if (!actionIs(v, "declare"))
      fault("a declaration poll may only answer \"declare\" or nothing", req, reply);
    decodeDeclaration(v, req, reply, d, pub);
    counts.declared++;
    conf = clampConfidence(v.n("confidence", 1.0));
    return true;
  }

  AskMove chooseAsk(const PublicState& pub) override {
    sendNewGameIfNeeded();
    counts.ask++;
    std::string req = "{\"op\":\"ask\",\"state\":" + stateJson(pub) + "}";
    std::string reply = request(req);
    mj::Value v;
    std::string perr;
    mj::parse(reply, v, perr);
    if (!actionIs(v, "ask")) fault("this seat has to ask, and the reply is not an ask", req, reply);
    const mj::Value* cv = v.find("card");
    const mj::Value* tv = v.find("target");
    if (!cv || !cv->isStr()) fault("an ask with no \"card\"", req, reply);
    if (!tv || !tv->isNum()) fault("an ask with no \"target\"", req, reply);
    int c = cardFromCode(cv->str);
    if (c < 0) fault("an ask names a card that is not in the deck: " + botpkg::clip(cv->str, 20), req, reply);
    int t = int(tv->num);
    if (t < 0 || t >= NPLAY) fault("an ask names seat " + std::to_string(t), req, reply);
    // The engine's driver would silently replace an illegal ask with the first
    // legal one, so it is caught here, where the reply that caused it is still
    // in hand.  The message names the specific rule that was broken.
    if (!legalAskShape(pub, myHand, seat, c, t)) {
      std::string why = "that ask is not legal";
      int s = setOf(c);
      if (teamOf(t) == teamOf(seat)) why = "you may only ask an opponent, and seat " +
                                           std::to_string(t) + " is on your own team";
      else if (!pub.setActive[s]) why = "half-suit " + std::to_string(s) + " is already out of play";
      else if (!pub.handCount[t]) why = "seat " + std::to_string(t) + " has no cards left";
      else if (myHand & bit(c)) why = "you already hold " + cardCode(c);
      else if (!(myHand & setMask(s))) why = "you hold no card of half-suit " + std::to_string(s) +
                                             ", so you may not ask for " + cardCode(c);
      fault(why, req, reply);
    }
    return AskMove{uint8_t(c), uint8_t(t)};
  }

  int choosePassTarget(const PublicState& pub, const int* cand, int n) override {
    sendNewGameIfNeeded();
    std::string cands = "[";
    for (int i = 0; i < n; i++) cands += (i ? "," : "") + std::to_string(cand[i]);
    cands += "]";
    counts.pass++;
    std::string req = "{\"op\":\"pass\",\"candidates\":" + cands + ",\"state\":" + stateJson(pub) + "}";
    std::string reply = request(req);
    mj::Value v;
    std::string perr;
    mj::parse(reply, v, perr);
    if (actionIs(v, "pass")) {
      const mj::Value* to = v.find("to");
      if (to && to->isNum()) {
        int t = int(to->num);
        for (int i = 0; i < n; i++) if (cand[i] == t) return t;
      }
    }
    fault("this seat has no cards and must hand the turn to a teammate; the reply "
          "did not name one of the seats offered in \"candidates\"", req, reply);
  }

  // The forced endgame: the engine sweeps a ladder of confidence thresholds and
  // asks each seat whether it is willing to declare this half-suit at that
  // threshold.  Only a willingness bit crosses between teammates, so this stays
  // exactly as leak-free for a foreign bot as for one of ours.
  bool forcedDeclare(const PublicState& pub, int set, Declaration& d, double& conf,
                     double threshold, bool lastResort) {
    sendNewGameIfNeeded();
    counts.forced++;
    std::string req = "{\"op\":\"forced\",\"set\":" + std::to_string(set) +
                      ",\"threshold\":" + (threshold < 0 ? std::string("0") : std::to_string(threshold)) +
                      ",\"last_resort\":" + (lastResort ? "true" : "false") +
                      ",\"state\":" + stateJson(pub, seat) + "}";
    std::string reply = request(req);
    mj::Value v;
    std::string perr;
    mj::parse(reply, v, perr);
    if (actionIsNone(v)) return false;
    if (!actionIs(v, "declare")) fault("a forced-declaration request may only answer \"declare\" or nothing", req, reply);
    Declaration got{};
    decodeDeclaration(v, req, reply, got, pub);
    // The engine has already fixed which half-suit it is asking about, and
    // overwrites d.set with it, so an answer about a different one is declined
    // rather than reshaped -- the sweep comes back round for that one.
    if (int(got.set) != set) return false;
    d = got;
    conf = clampConfidence(v.n("confidence", 1.0));
    return true;
  }

  bool willingForced(const PublicState& pub, int set, Declaration& d, double& conf,
                     double threshold) override {
    if (!forcedDeclare(pub, set, d, conf, threshold, false)) return false;
    return conf >= threshold;
  }

  // The last resort: somebody on this team must name this half-suit now.  If the
  // bot still declines, the base class allocates it all to this seat -- which is
  // nearly always wrong -- so the fallback is counted and reported, because a
  // declaration the ENGINE invented must never be read as one of theirs.
  // Incremented from every worker thread of a match, so it is atomic; the
  // per-agent `counts.fallbacks` beside it is what the self-check reports.
  static std::atomic<long>& forcedFallbacks() { static std::atomic<long> n{0}; return n; }

  void bestGuess(const PublicState& pub, int set, Declaration& d, double& conf) override {
    if (forcedDeclare(pub, set, d, conf, -1, true)) return;
    forcedFallbacks()++;
    counts.fallbacks++;
    Agent::bestGuess(pub, set, d, conf);
  }
};

}  // namespace extbot
}  // namespace fish
