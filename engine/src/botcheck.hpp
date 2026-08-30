// The self-check an uploaded bot gets before anybody has to sit down opposite
// it: one complete game, against this engine's own driver, reported back to
// whoever uploaded it.
//
// It is deliberately a REAL game rather than a canned position.  A scripted
// handshake would prove the bot starts; a game proves it can be asked for an
// ask, polled for a declaration off turn, handed a turn it cannot use, and
// pressed through the forced endgame -- which are exactly the four places a
// first attempt at the protocol goes wrong, and the last two are the ones an
// author never reaches by hand.  What comes back is either "it played 47 asks
// and 3 forced declarations in 4.2 s" or the precise request and reply that
// broke, which is a bug report its author can act on without owning this repo.
//
// The check RUNS THE PACKAGE'S CODE.  That is why serve.hpp gates it on the
// host credential and not on the invite: uploading is inert, running is not.
#pragma once
#include "factory.hpp"
#include "botpkg.hpp"
#include "extbot.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace fish {
namespace botcheck {

// The opponents.  `hunter` is a cheap baseline with no search in it, so the
// wall-clock in the report is the uploaded bot's own time and not this engine's.
static constexpr const char* SPARRING = "hunter";

inline std::string oneLine(const std::string& s, size_t n) {
  std::string o;
  for (char c : s) o.push_back(c == '\n' ? ' ' : c);
  return o.size() > n ? o.substr(0, n) : o;
}

// Returns true when the bot played a complete game.  `report` is filled in
// either way; `err` only on failure.
inline bool run(const botpkg::Installed& pkg, std::string& report, std::string& err) {
  const std::string spec = botpkg::specFor(pkg);
  // A fault must come back as an exception even if this is somehow reached
  // outside `fish serve`, or a broken package would take the process with it.
  const bool wasThrowing = botFaultsThrow();
  botFaultsThrow() = true;
  struct Restore {
    bool was;
    ~Restore() { botFaultsThrow() = was; }
  } restore{wasThrowing};

  std::unique_ptr<Agent> owned[NPLAY];
  Agent* ptr[NPLAY];
  owned[0] = makeAgent(spec);
  for (int p = 1; p < NPLAY; p++) owned[p] = makeAgent(SPARRING);
  for (int p = 0; p < NPLAY; p++) ptr[p] = owned[p].get();
  auto* bot = dynamic_cast<extbot::ExternAgent*>(owned[0].get());

  Rules rules;
  Game game;
  auto t0 = std::chrono::steady_clock::now();
  GameResult res;
  int deals = 0;
  try {
    // Fixed seeds, so two runs of the check are comparable and an author
    // chasing a fault sees the same deal twice.  Up to four of them, because
    // the two rarest requests -- the forced endgame and handing on a turn from
    // a seat with no cards -- do not arise in every deal, and an author who is
    // told "your forced branch was never called" learns more than one who is
    // told nothing.  It stops as soon as both have been exercised.
    static const uint64_t seeds[4] = {0x51F15Bull, 0x9E3779B9ull, 0xC0FFEEull, 0x1234567ull};
    for (int d = 0; d < 4; d++) {
      GameResult r = game.run(seeds[d], rules, ptr);
      if (d == 0) res = r;
      deals++;
      if (!bot) break;
      if (bot->counts.forced > 0 && bot->counts.pass > 0) break;
    }
  } catch (const BotFault& f) {
    err = f.text;
    report = "The bot stopped the game.\n\n" + f.text +
             "\n\nIts own stderr is in " + pkg.logPath() + ".";
    return false;
  } catch (const std::exception& e) {
    err = e.what();
    report = std::string("The check could not finish: ") + e.what();
    return false;
  }
  double secs = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - t0).count() / 1000.0;

  char buf[1200];
  if (bot) {
    const auto& c = bot->counts;
    double meanMs = c.calls() ? double(c.micros) / 1000.0 / double(c.calls()) : 0.0;
    snprintf(buf, sizeof(buf),
             "Played %d complete game%s in %.2f s.\n"
             "  seat 0 (%s) against five %s\n"
             "  first deal: half-suits %d - %d (its team first), %d asks over %d events\n"
             "  answered: %ld ask, %ld declaration poll, %ld pass, %ld forced\n"
             "  it declared %ld half-suits of its own accord\n"
             "  %.1f ms per reply on average\n",
             deals, deals == 1 ? "" : "s", secs, pkg.man.name.c_str(), SPARRING,
             res.score[0], res.score[1], res.asks, res.events,
             c.ask, c.poll, c.pass, c.forced, c.declared, meanMs);
    report = buf;
    // Naming what was NOT exercised, because "it passed" over a branch that
    // never ran is the check telling an author something it does not know.
    if (c.forced == 0)
      report += "\nNote: none of these deals reached the forced endgame, so the \"forced\" op "
                "was never exercised. Seat it against a stronger opponent, or run "
                "`fish match --a=bot:" + pkg.id + " --b=v06 --games=40`, before trusting it.\n";
    if (c.pass == 0)
      report += "\nNote: this bot never ran out of cards while holding the turn, so the "
                "\"pass\" op was never exercised.\n";
    if (c.fallbacks) {
      snprintf(buf, sizeof(buf),
               "\nWarning: the forced endgame asked it to name a half-suit %ld time(s) and it "
               "declined, so THIS ENGINE guessed on its behalf -- an allocation naming every "
               "card to one seat, which is nearly always wrong. Answer the \"forced\" op with "
               "last_resort=true and your best allocation instead of nothing.\n",
               c.fallbacks);
      report += buf;
    }
    if (c.poll == 0 && !pkg.man.pollOffTurn)
      report += "\nNote: this package sets poll_off_turn=false, so it was never offered the "
                "chance to declare during another seat's turn.\n";
  } else {
    // A kv-dialect package plays through kv6.hpp, which keeps no counters.
    snprintf(buf, sizeof(buf),
             "Played a full game in %.2f s (KV dialect).\n"
             "  seat 0 (%s) against five %s\n"
             "  half-suits  %d - %d   (its team first)\n"
             "  asks %d over %d events\n",
             secs, pkg.man.name.c_str(), SPARRING, res.score[0], res.score[1], res.asks, res.events);
    report = buf;
  }
  if (res.hitLimit)
    report += "\nWarning: the game hit the ask limit rather than finishing, which usually means "
              "the bot repeats a move it can never win with.\n";
  return true;
}

}  // namespace botcheck
}  // namespace fish
