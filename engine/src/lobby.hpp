// Who is allowed to do what at a networked table.
//
// On loopback the table has always been wide open, and it stays that way: the
// only client is the person who started the process.  The moment it binds
// another interface that stops being true, and the game becomes unplayable
// without credentials -- not for tidiness, but because Fish is a hidden
// information game and `/api/state?seat=2` used to hand seat 2's cards to
// anybody who asked.  A player who can read the other five hands is not playing
// Fish, and neither is anybody sitting opposite them.
//
// So there are exactly three kinds of secret:
//
//   * the **invite code**, shared with everyone you want at the table.  It buys
//     the public view -- the score, the card counts, the event log, the lobby --
//     and the right to attempt a claim.  It is short enough to read down a phone.
//   * a **seat token**, minted when somebody claims a seat and known only to
//     that browser.  It is the only thing that will make the server disclose
//     that seat's hand or accept a move for it.
//   * the **host token**, printed on the console of the machine running the
//     server.  It governs the table -- the seat configuration, the deal, pause,
//     pace -- and deliberately confers no card visibility at all, so hosting a
//     game and playing in one are separate powers.
//
// Everything here is safe to call from any thread and holds only its own mutex,
// so it never participates in the game thread's locking order.
#pragma once
#include "fish.hpp"
#include <string>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace fish {

inline long long nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now().time_since_epoch()).count();
}

// /dev/urandom rather than the standard library, because the guarantee that
// matters is "an attacker cannot predict this", which <random> does not make.
inline void randomBytes(unsigned char* out, size_t n) {
  FILE* f = fopen("/dev/urandom", "rb");
  if (f) {
    size_t got = fread(out, 1, n, f);
    fclose(f);
    if (got == n) return;
  }
  // Refusing to start is better than a table whose secrets are guessable.
  fprintf(stderr, "fish serve: no entropy available (/dev/urandom unreadable)\n");
  std::exit(3);
}

inline std::string randomHex(int bytes) {
  std::vector<unsigned char> b(size_t(bytes), 0);
  randomBytes(b.data(), b.size());
  static const char* H = "0123456789abcdef";
  std::string s;
  s.reserve(size_t(bytes) * 2);
  for (unsigned char c : b) { s.push_back(H[c >> 4]); s.push_back(H[c & 15]); }
  return s;
}

// An invite is read aloud and typed, so the alphabet drops the characters that
// get misheard or mistyped: no 0/O, no 1/I/L.  Rejection sampling rather than a
// modulo, so every code is equally likely.
inline std::string randomCode(int n) {
  static const char* A = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";   // 31 symbols
  const size_t k = strlen(A);
  std::string s;
  while ((int)s.size() < n) {
    unsigned char b[16];
    randomBytes(b, sizeof(b));
    for (unsigned char c : b) {
      if (c >= (256 / k) * k) continue;         // would bias the tail
      s.push_back(A[c % k]);
      if ((int)s.size() == n) break;
    }
  }
  return s;
}

// Length-independent up to the compare itself: no early return on the first
// differing byte, so a token cannot be recovered a character at a time.
inline bool secretEqual(const std::string& a, const std::string& b) {
  if (a.empty() || b.empty() || a.size() != b.size()) return false;
  unsigned char d = 0;
  for (size_t i = 0; i < a.size(); i++) d = (unsigned char)(d | (a[i] ^ b[i]));
  return d == 0;
}

// Case-insensitive for invite codes only -- they are typed by hand, and the
// alphabet is upper case, so folding case costs nothing and saves a support
// question.  Still compared without an early exit.
inline bool codeEqual(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char d = 0;
  for (size_t i = 0; i < a.size(); i++)
    d = (unsigned char)(d | (toupper((unsigned char)a[i]) ^ toupper((unsigned char)b[i])));
  return d == 0;
}

struct SeatClaim {
  std::string token;   // empty means the seat is open
  std::string name;
  long long seen = 0;  // nowMs() of the last request that carried the token
};

// A seat is "away" rather than gone when its browser has been quiet for a
// while; the distinction is only ever displayed, never enforced, because a
// player thinking for two minutes is not a player who has left.
static constexpr long long AWAY_MS = 12000;

struct Lobby {
  mutable std::mutex mu;
  bool guard = false;               // false on loopback: no credentials at all
  std::string hostToken, invite;
  SeatClaim seat[NPLAY];

  void init(bool guarded, const std::string& inviteOverride) {
    std::lock_guard<std::mutex> lk(mu);
    guard = guarded;
    hostToken = randomHex(16);
    invite = inviteOverride.empty() ? randomCode(8) : inviteOverride;
  }

  bool isHost(const std::string& tok) const {
    std::lock_guard<std::mutex> lk(mu);
    return !guard || secretEqual(tok, hostToken);
  }
  bool inviteOk(const std::string& code, const std::string& tok) const {
    std::lock_guard<std::mutex> lk(mu);
    if (!guard) return true;
    if (codeEqual(code, invite)) return true;
    // A player who has already claimed a seat stays admitted even if their
    // browser has since forgotten the code it arrived with.
    for (int p = 0; p < NPLAY; p++) if (secretEqual(tok, seat[p].token)) return true;
    return false;
  }
  // The one question that decides whether a hand is disclosed.
  bool holdsSeat(const std::string& tok, int p) const {
    if (p < 0 || p >= NPLAY) return false;
    std::lock_guard<std::mutex> lk(mu);
    if (!guard) return true;
    return secretEqual(tok, seat[p].token);
  }
  bool claimed(int p) const {
    std::lock_guard<std::mutex> lk(mu);
    return p >= 0 && p < NPLAY && !seat[p].token.empty();
  }
  std::string nameOf(int p) const {
    std::lock_guard<std::mutex> lk(mu);
    return (p >= 0 && p < NPLAY) ? seat[p].name : std::string();
  }

  // Marks every seat the token holds as alive.  Called on each authenticated
  // request, which is what makes "away" mean anything.
  void touch(const std::string& tok) {
    if (tok.empty()) return;
    std::lock_guard<std::mutex> lk(mu);
    long long t = nowMs();
    for (int p = 0; p < NPLAY; p++) if (secretEqual(tok, seat[p].token)) seat[p].seen = t;
  }

  // Re-claiming with the token you already hold is how a browser recovers from a
  // reload, so it succeeds and simply renames the seat.  Somebody else's live
  // claim is refused; there is a host-only kick for the case where a player has
  // gone and taken their localStorage with them.
  bool claim(int p, const std::string& name, const std::string& have, std::string& out, std::string& why) {
    if (p < 0 || p >= NPLAY) { why = "no such seat"; return false; }
    std::lock_guard<std::mutex> lk(mu);
    if (!seat[p].token.empty() && !secretEqual(have, seat[p].token)) {
      why = "that seat is taken";
      return false;
    }
    if (seat[p].token.empty()) {
      // The token identifies a browser, not a seat, so a client that already
      // holds one seat takes the next under the same credential.  That is what
      // lets one person play two seats at a local table -- and what lets the
      // seat switcher know which seats are theirs from a single header.
      std::string reuse;
      for (int q = 0; q < NPLAY; q++)
        if (!seat[q].token.empty() && secretEqual(have, seat[q].token)) { reuse = seat[q].token; break; }
      seat[p].token = reuse.empty() ? randomHex(16) : reuse;
    }
    seat[p].name = name;
    seat[p].seen = nowMs();
    out = seat[p].token;
    return true;
  }
  void release(int p) {
    std::lock_guard<std::mutex> lk(mu);
    if (p >= 0 && p < NPLAY) seat[p] = SeatClaim{};
  }
  // Frees every seat whose engine the host has just switched away from a human,
  // so a claim can never outlive the seat it was made for.
  void releaseNonHuman(const bool* human) {
    std::lock_guard<std::mutex> lk(mu);
    for (int p = 0; p < NPLAY; p++) if (!human[p]) seat[p] = SeatClaim{};
  }

  std::string json(const std::string& tok) const {
    std::lock_guard<std::mutex> lk(mu);
    long long t = nowMs();
    std::string o = "[";
    for (int p = 0; p < NPLAY; p++) {
      if (p) o += ",";
      bool taken = !seat[p].token.empty();
      bool mine  = taken && secretEqual(tok, seat[p].token);
      char b[256];
      snprintf(b, sizeof(b), "{\"i\":%d,\"taken\":%s,\"mine\":%s,\"idleMs\":%lld,\"away\":%s}",
               p, taken ? "true" : "false", mine ? "true" : "false",
               taken ? (t - seat[p].seen) : 0LL,
               (taken && t - seat[p].seen > AWAY_MS) ? "true" : "false");
      o += b;
    }
    return o + "]";
  }
  // Every seat this token may play, which is what the browser's seat switcher
  // is allowed to offer.
  std::string mineJson(const std::string& tok) const {
    std::lock_guard<std::mutex> lk(mu);
    std::string o = "[";
    bool first = true;
    for (int p = 0; p < NPLAY; p++) {
      if (!guard || (!seat[p].token.empty() && secretEqual(tok, seat[p].token))) {
        if (!first) o += ",";
        o += std::to_string(p);
        first = false;
      }
    }
    return o + "]";
  }
};

} // namespace fish
