// What a foreign bot's failure looks like when it must not end the process.
//
// Both bridges in this engine follow the same rule -- a bot that answers badly
// stops the game and is reported, and is NEVER quietly replaced by a legal move
// -- and both implemented "stops" as _exit(3), which is right for a measured
// run: a number produced by a substituted move is worse than no number, and a
// half-dead match should not keep going.
//
// It is wrong for `fish serve`.  There, six people are sitting at a table, and
// one guest bot's mistake should end that game with a message on the felt, not
// take the server down under everybody.  So the exit becomes a throw when this
// flag says so; the flag is off by default, and every batch path -- match,
// arena, every experiment script -- runs exactly as it did before.
#pragma once
#include <string>
#include <exception>
#include <mutex>
#include <csignal>

namespace fish {

struct BotFault : std::exception {
  std::string bot, problem, request, reply, text;
  BotFault(std::string b, std::string p, std::string req, std::string rep)
      : bot(std::move(b)), problem(std::move(p)), request(std::move(req)), reply(std::move(rep)) {
    text = "bot '" + bot + "': " + problem;
    if (!request.empty()) text += "\n  request : " + request.substr(0, 900);
    if (!reply.empty())   text += "\n  reply   : " + reply.substr(0, 900);
  }
  const char* what() const noexcept override { return text.c_str(); }
};

// Set once, by runServe.  A global rather than a per-agent flag because it also
// governs kv6.hpp, whose fatal() is a free function reached from places that
// hold no agent.
inline bool& botFaultsThrow() {
  static bool b = false;
  return b;
}

// ONE lock across every place this process forks a child, shared by
// extbot.hpp, kv6.hpp and botpkg.hpp's dependency install.
//
// The hazard each of them documents is the same: between pipe() and
// fcntl(FD_CLOEXEC) the new descriptors are inheritable, so a fork on ANOTHER
// thread in that window hands them to an unrelated child, which then holds a
// pipe end nobody will ever close -- and the reader waits for an EOF that never
// comes.  A lock per fork site does not exclude the fork sites from each other,
// and now that a table can seat a native bot and a KV-dialect bot while pip
// runs in the background, all three are live at once.
inline std::mutex& botSpawnLock() {
  static std::mutex m;
  return m;
}

// A write to a pipe whose reader has gone raises SIGPIPE, and the default
// disposition is to kill the process.  Every bot write site handles the EPIPE
// return instead -- with the diagnostic the whole design turns on -- but only
// if the signal is ignored first.  `fish serve` happens to ignore it (the HTTP
// accept loop does), which is exactly why this was invisible until a bot broke
// its pipe under `fish match` and the run died with no output at all.
//
// Called once, from the first bot process spawn: nothing else in this engine
// writes to a pipe, so this is scoped to "a foreign bot exists" rather than
// changing the process globally at startup.
inline void ignoreSigpipeOnce() {
  static const bool done = [] {
    ::signal(SIGPIPE, SIG_IGN);
    return true;
  }();
  (void)done;
}

}  // namespace fish
