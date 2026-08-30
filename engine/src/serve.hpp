// `fish serve` -- HTTP front end for the interactive table.
#pragma once
#include "table.hpp"
#include "lobby.hpp"
#include "tunnel.hpp"
#include "botpkg.hpp"
#include "botcheck.hpp"
#include <set>
#include <fstream>
#include <sstream>
#include <libgen.h>
#include <sys/stat.h>
#include <ifaddrs.h>
#include <net/if.h>

namespace fish {

inline bool fileExists(const std::string& p) {
  struct stat st;
  return ::stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFREG);
}

inline std::string readFile(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

inline std::string mimeFor(const std::string& p) {
  auto ends = [&](const char* e) {
    size_t n = strlen(e);
    return p.size() >= n && p.compare(p.size() - n, n, e) == 0;
  };
  if (ends(".html")) return "text/html; charset=utf-8";
  if (ends(".js"))   return "application/javascript; charset=utf-8";
  if (ends(".css"))  return "text/css; charset=utf-8";
  if (ends(".svg"))  return "image/svg+xml";
  if (ends(".png"))  return "image/png";
  if (ends(".ico"))  return "image/x-icon";
  return "application/octet-stream";
}

// Every non-loopback IPv4 address the host answers on, so the console can print
// a link a phone on the same wifi can actually open.
inline std::vector<std::string> localAddresses() {
  std::vector<std::string> out;
  ifaddrs* ifa = nullptr;
  if (getifaddrs(&ifa) != 0) return out;
  for (ifaddrs* p = ifa; p; p = p->ifa_next) {
    if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
    if (!(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
    char ip[INET_ADDRSTRLEN] = {0};
    auto* sin = (sockaddr_in*)p->ifa_addr;
    if (inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip))) out.push_back(ip);
  }
  freeifaddrs(ifa);
  return out;
}

// Upper bound on the per-event delay.  A human watching six seats and trying to
// track 54 cards wants far more thinking time than a plausible animation delay,
// so this is generous rather than cosmetic.
static constexpr int PACE_MAX = 20000;
// Longest a long-poll may block before it must answer anyway.  Short enough that
// an intermediary will not time the connection out on us, long enough that an
// idle table costs one request a minute per player instead of three a second.
static constexpr int WAIT_MAX = 25000;

struct Server {
  Table table;
  Lobby lobby;
  std::string webDir;
  // The address other people should use, which is exactly the one the host's
  // own browser cannot work out: a host on loopback sees location.origin =
  // 127.0.0.1, and a Copy button that hands that to three friends is worse than
  // no Copy button.  Set once the listener (and any tunnel) is up.
  std::string shareUrl;
  std::mutex ctl;            // serialises new/abandon against each other

  // Who may put a bot package on this machine.  Uploading is inert -- nothing
  // in a package is executed by installing it -- so an invited player may do
  // it, which is the whole point: everybody at the table brings their own bot.
  // RUNNING one is a different power and stays with the host, who decides what
  // to seat, what to check and what to install dependencies for.  --lock-bots
  // narrows uploading to the host as well.
  bool lockBots = false;
  std::mutex jobMu;                    // guards `jobs`
  std::set<std::string> jobs;          // bot ids with a background job running

  bool jobBegin(const std::string& id) {
    std::lock_guard<std::mutex> lk(jobMu);
    return jobs.insert(id).second;
  }
  void jobEnd(const std::string& id) {
    std::lock_guard<std::mutex> lk(jobMu);
    jobs.erase(id);
  }

  static HttpResponse json(const std::string& body, int status = 200) {
    HttpResponse r;
    r.status = status;
    r.type = "application/json; charset=utf-8";
    r.body = body;
    return r;
  }
  static HttpResponse err(const std::string& msg, int status = 400) {
    return json("{\"error\":" + jesc(msg) + "}", status);
  }
  static HttpResponse okj() { return json("{\"ok\":true}"); }
  // Every rejected credential costs the caller a fifth of a second.  Against an
  // eight-symbol invite that is the difference between a feasible online guess
  // and an infeasible one, and no honest client ever pays it twice.
  static HttpResponse deny(const char* msg, int status = 401) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return err(msg, status);
  }

  // The credential travels in a header so that it stays out of proxy logs,
  // browser history and the Referer of anything the page links to.  The query
  // parameter is accepted only as a fallback for hand-driven calls (curl, a
  // pasted URL), never emitted by the browser client.
  static std::string tokenOf(const HttpRequest& req) {
    std::string t = req.header("x-fish-token");
    return t.empty() ? req.get("t") : t;
  }
  // Running the table and sitting at it are separate powers held by separate
  // secrets, so they travel separately too: a host who is also a player sends
  // both, and neither one can be mistaken for the other.
  static std::string hostTokenOf(const HttpRequest& req) {
    std::string t = req.header("x-fish-host");
    return t.empty() ? req.get("h") : t;
  }

  // Reads owner=a,b,c,d,e,f and checks the allocation is well formed and names
  // only the declaring seat's own team, which is what game.hpp requires of a
  // declaration before it will execute it.
  static bool parseOwners(const HttpRequest& req, int seat, Declaration& d, std::string& why) {
    std::string s = req.get("owner");
    std::vector<int> v;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) v.push_back(atoi(tok.c_str()));
    if (v.size() != SETSZ) { why = "an allocation needs all six cards"; return false; }
    for (int i = 0; i < SETSZ; i++) {
      if (v[i] < 0 || v[i] >= NPLAY) { why = "bad seat in allocation"; return false; }
      if (teamOf(v[i]) != teamOf(seat)) { why = "you may only name your own team"; return false; }
      d.owner[i] = uint8_t(v[i]);
    }
    return true;
  }

  // Display names are free text from the setup screen.  They are trimmed, kept
  // printable, length-capped, and de-duplicated, because the whole point of
  // naming six seats is that "which bot was that?" has an answer.
  static std::string cleanName(std::string s) {
    std::string o;
    for (char c : s) if ((unsigned char)c >= 0x20 && c != 0x7f) o.push_back(c);
    size_t a = o.find_first_not_of(" \t");
    size_t b = o.find_last_not_of(" \t");
    o = (a == std::string::npos) ? std::string() : o.substr(a, b - a + 1);
    if (o.size() > 20) o = o.substr(0, 20);
    return o;
  }

  // The seat configuration, as the host's setup screen states it.  Shared by
  // /api/table (publish it so the lobby can be joined) and /api/new (publish it
  // and deal), so the two can never drift.
  bool readSeats(const HttpRequest& req, SeatCfg out[NPLAY], std::string& why) {
    bool human[NPLAY];
    for (int p = 0; p < NPLAY; p++) {
      char key[4]; snprintf(key, sizeof(key), "s%d", p);
      std::string spec = req.get(key, "v06");
      if (spec.empty()) spec = "v06";
      if (!knownPolicy(spec)) { why = "unknown policy '" + spec + "' at seat " + std::to_string(p); return false; }
      out[p].spec = spec;
      char hk[4]; snprintf(hk, sizeof(hk), "h%d", p);
      out[p].human = req.getb(hk, false);
      human[p] = out[p].human;
      char nk[4]; snprintf(nk, sizeof(nk), "n%d", p);
      out[p].name = cleanName(req.get(nk, ""));
    }
    // The host owns the shape of the table, so a seat turned into a bot loses
    // its claim rather than overriding the host.  A seat that stays human and is
    // claimed is named by whoever is sitting in it, not by the host.
    lobby.releaseNonHuman(human);
    for (int p = 0; p < NPLAY; p++) {
      if (lobby.claimed(p)) { out[p].human = true; out[p].name = lobby.nameOf(p); }
      if (out[p].name.empty()) out[p].name = defaultSeatName(p, out[p].human);
    }
    // De-duplication is a display concern and belongs to Table::displayNames,
    // which recomputes it from these base names on every publish.
    return true;
  }

  // An unclaimed player seat at a shared table belongs to nobody yet, so
  // calling it "You" would be a lie to five of the six people looking at it.
  std::string defaultSeatName(int p, bool human) const {
    if (!human) return Table::defaultName(p);
    return lobby.guard ? ("Seat " + std::to_string(p)) : std::string("You");
  }

  // A claim has to reach the seat itself, not just the lobby, or the table goes
  // on showing the placeholder until the host next touches something.
  void seatNameFromLobby(int p) {
    table.seats[p].name = lobby.claimed(p) ? lobby.nameOf(p)
                                           : defaultSeatName(p, table.seats[p].human);
  }

  // In a guarded game a human seat nobody has claimed is a seat the table would
  // wait on forever, so dealing is refused until every one of them is filled.
  bool seatsReady(const SeatCfg cfg[NPLAY], std::string& why) {
    if (!lobby.guard) return true;
    for (int p = 0; p < NPLAY; p++)
      if (cfg[p].human && !lobby.claimed(p)) {
        why = "seat " + std::to_string(p) + " is still waiting for a player";
        return false;
      }
    return true;
  }

  // The fields the browser needs that are about *who is asking* rather than
  // about the game: appended to the game snapshot rather than mixed into it.
  std::string authJson(const std::string& tok, bool host, bool authed) {
    // The bot library rides along with the game state rather than sitting on a
    // route of its own, so that an upload appears in five other browsers the
    // moment it lands -- the long poll is already watching.
    std::string o = ",\"bots\":" + botpkg::registry().json()
                  + ",\"canUpload\":" + std::string((host || (authed && !lockBots)) ? "true" : "false")
                  + ",\"maxUpload\":" + std::to_string(botpkg::MAX_ZIP_BYTES)
                  + ",\"guard\":" + std::string(lobby.guard ? "true" : "false")
                  + ",\"auth\":" + std::string(authed ? "true" : "false")
                  + ",\"host\":" + std::string(host ? "true" : "false")
                  + ",\"lobby\":" + lobby.json(tok)
                  + ",\"mySeats\":" + lobby.mineJson(tok);
    // The invite is a secret the host distributes; it is never sent to anybody
    // else, so a player cannot re-share the table behind the host's back.
    if (host && lobby.guard) {
      o += ",\"invite\":" + jesc(lobby.invite);
      if (!shareUrl.empty()) o += ",\"shareUrl\":" + jesc(shareUrl);
    }
    return o;
  }


  // ---------------------------------------------------------------- bots
  // The bot library.  Uploading installs a package and runs nothing; checking,
  // preparing and seating run the package's own code and are the host's calls.
  // docs/BOT_PACKAGE.md is what to send somebody who wants to bring a bot.
  std::string uploaderName(const std::string& tok, const HttpRequest& req) {
    for (int p = 0; p < NPLAY; p++)
      if (lobby.holdsSeat(tok, p) && !lobby.nameOf(p).empty()) return lobby.nameOf(p);
    std::string given = cleanName(req.get("who"));
    return given.empty() ? std::string("a player") : given;
  }

  HttpResponse handleBots(const HttpRequest& req, const std::string& tok, bool host, bool authed) {
    auto& reg = botpkg::registry();
    if (!authed) return deny("this table needs an invite code");

    if (req.path == "/api/bots") return json("{\"bots\":" + reg.json() + "}");

    if (req.path == "/api/bots/upload") {
      if (!host && lockBots) return deny("only the host may add bots to this table", 403);
      // Adding a NEW bot is open; REPLACING one is not.  A seat holds the spec
      // `bot:<id>` and re-resolves it against the registry on every deal, so
      // overwriting an id the host has already checked and seated would swap
      // the code under a seat they approved for something else -- which is the
      // one thing the "installing never executes" boundary exists to prevent.
      if (!host && req.getb("replace", false))
        return deny("only the host may replace a bot that is already installed; "
                    "give yours a different name or id", 403);
      // A content type that forces a CORS preflight, so a page on another site
      // cannot post a package here as a "simple" request.  The Origin check
      // above already covers this; requiring it as well means neither has to be
      // the only thing standing between a visited web page and an install.
      {
        std::string ct = req.header("content-type");
        if (ct.rfind("application/octet-stream", 0) != 0 && ct.rfind("application/zip", 0) != 0)
          return err("send the package as application/octet-stream", 415);
      }
      if (req.body.empty()) return err("no package arrived -- send the .zip as the request body");
      {
        // Replacing a package whose process is mid-game would pull the files out
        // from under a running bot, so the library is only rearranged between
        // games -- the same rule /api/bots/remove follows.
        std::lock_guard<std::mutex> lk(table.io.mu);
        if (table.snap.running && req.getb("replace", false))
          return err("not while a game is running", 409);
      }
      // A bounded library, so that a rude guest costs a bounded amount of disk.
      std::vector<botpkg::Installed> have = reg.all();
      long long total = 0;
      for (const auto& b : have) total += b.zipBytes;
      const bool replace = req.getb("replace", false);
      if (have.size() >= 32 && !replace)
        return err("this table already holds 32 bots; remove one first", 409);
      if (total + (long long)req.body.size() > 1024LL * 1024 * 1024)
        return err("the bot library is full; remove one first", 409);

      std::string id, why;
      if (!reg.install(req.body, uploaderName(tok, req), replace, id, why)) {
        // Distinguishable, because the page offers to replace rather than
        // making somebody rename their bot to get past it.
        bool clash = why.rfind("a bot called", 0) == 0;
        return json("{\"error\":" + jesc(why) + (clash ? ",\"clash\":true" : "") + "}", clash ? 409 : 400);
      }
      botpkg::Installed b;
      reg.get(id, b);
      { std::lock_guard<std::mutex> lk(table.io.mu); table.io.bump(); }
      printf("fish serve: %s uploaded the bot '%s' (%zu bytes)\n",
             uploaderName(tok, req).c_str(), id.c_str(), req.body.size());
      fflush(stdout);
      return json("{\"ok\":true,\"id\":" + jesc(id) + ",\"name\":" + jesc(b.man.name) +
                  ",\"spec\":" + jesc(botpkg::specFor(b)) +
                  ",\"needsVenv\":" + std::string(b.man.venv ? "true" : "false") +
                  ",\"prepared\":" + std::string(b.prepared ? "true" : "false") + "}");
    }

    // Everything below either runs the package or changes the library, so it is
    // the host's.
    const std::string id = req.get("id");
    if (!botpkg::validId(id)) return err("bad bot id");
    botpkg::Installed bot;
    if (!reg.get(id, bot)) return err("no bot called '" + id + "' is installed", 404);

    if (req.path == "/api/bots/log") {
      std::string which = req.get("which", "bot");
      std::string path = which == "prepare" ? bot.preparePath()
                       : which == "check"   ? bot.checkPath()
                                            : bot.logPath();
      std::string body = botpkg::readWhole(path);
      // The tail, because a bot that prints per decision produces megabytes and
      // the interesting part is always the end.
      if (body.size() > 64 * 1024) body = "(earlier output omitted)\n" + body.substr(body.size() - 64 * 1024);
      HttpResponse r;
      r.type = "text/plain; charset=utf-8";
      r.body = body.empty() ? "(nothing yet)\n" : body;
      return r;
    }

    if (req.path == "/api/bots/package") {
      std::string body = botpkg::readWhole(bot.zipPath());
      if (body.empty()) return err("that package's zip is no longer on disk", 404);
      HttpResponse r;
      r.type = "application/zip";
      r.extra = "Content-Disposition: attachment; filename=\"" + id + ".zip\"\r\n";
      r.body = body;
      return r;
    }

    if (!host) return deny("only the host may run, prepare or remove a bot", 403);

    if (req.path == "/api/bots/remove") {
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        if (table.snap.running) return err("not while a game is running", 409);
      }
      std::string why;
      if (!reg.erase(id, why)) return err(why);
      // A seat still pointing at the bot that just left would fail the policy
      // check on the next deal, so it is put back to the house engine now.
      { std::lock_guard<std::mutex> ck(ctl);
        for (int p = 0; p < NPLAY; p++)
          if (botpkg::idFromSpec(table.seats[p].spec) == id) table.seats[p].spec = "v06";
        table.publishConfig(); }
      return okj();
    }

    if (req.path == "/api/bots/prepare" || req.path == "/api/bots/check") {
      const bool checking = req.path == "/api/bots/check";
      // Neither is safe mid-game: check seats the bot in a game of its own, and
      // prepare deletes and rebuilds the virtualenv that a seated bot may be
      // running out of right now.
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        if (table.snap.running) return err("not while a game is running", 409);
      }
      if (!checking && !bot.man.venv)
        return err("this package does not ask for a virtualenv, so there is nothing to install");
      if (!jobBegin(id)) return err("that bot is already busy", 409);
      reg.setStatus(id, checking ? "checking" : "preparing", checking ? "playing a game" : "installing");
      { std::lock_guard<std::mutex> lk(table.io.mu); table.io.bump(); }
      // Detached, because pip takes minutes and a game takes seconds-to-a-minute
      // and neither should be holding an HTTP connection open.  Progress is read
      // back through the status in the bot list and the logs above.
      std::thread([this, id, checking] {
        auto& r = botpkg::registry();
        std::string log, why;
        if (checking) {
          botpkg::Installed b;
          std::string report;
          bool ok = r.get(id, b) && botcheck::run(b, report, why);
          botpkg::writeWhole(b.checkPath(), report);
          r.setStatus(id, ok ? "ok" : "failed",
                      ok ? botcheck::oneLine(report, 200) : botcheck::oneLine(why, 200));
        } else {
          bool ok = r.prepare(id, log, why);
          if (!ok) r.setStatus(id, "failed", why);
        }
        jobEnd(id);
        std::lock_guard<std::mutex> lk(table.io.mu);
        table.io.bump();
      }).detach();
      return okj();
    }

    return err("not found", 404);
  }

  // A request the browser made on behalf of ANOTHER site.
  //
  // On a loopback table there are no credentials -- that has always been fine,
  // because the only thing a stranger's web page could make the table do was
  // deal a hand.  That stopped being true the moment /api/bots/upload existed:
  // a page the host happens to visit can POST a zip to 127.0.0.1 and then POST
  // /api/new to seat it, and the host's machine runs the stranger's code
  // without a click.  Browsers volunteer the Origin header on exactly these
  // requests, so refusing a mismatched one costs the real client nothing (its
  // Origin is this table) and closes the whole class.
  //
  // Absent Origin is allowed on purpose: curl sends none, and every
  // hand-driven call in docs/PLAY.md is a curl call.
  static bool crossOrigin(const HttpRequest& req) {
    std::string origin = req.header("origin");
    if (origin.empty() || origin == "null") return false;
    size_t scheme = origin.find("://");
    std::string oh = scheme == std::string::npos ? origin : origin.substr(scheme + 3);
    std::string host = req.header("host");
    if (host.empty()) return true;
    return oh != host;
  }

  HttpResponse handle(const HttpRequest& req) {
    if (req.path.rfind("/api/", 0) == 0 && crossOrigin(req))
      return err("that request came from another site", 403);
    if (req.path == "/" || req.path == "/index.html") {
      std::string p = webDir + "/index.html";
      if (!fileExists(p)) {
        HttpResponse r;
        r.status = 404;
        r.type = "text/plain; charset=utf-8";
        r.body = "index.html not found under " + webDir + "\nRun fish serve from engine/, or pass --web=DIR.\n";
        return r;
      }
      HttpResponse r;
      r.type = "text/html; charset=utf-8";
      r.body = readFile(p);
      return r;
    }

    if (req.path.rfind("/api/", 0) != 0) {
      if (req.path.find("..") != std::string::npos) return err("bad path", 400);
      std::string p = webDir + req.path;
      if (!fileExists(p)) return err("not found", 404);
      HttpResponse r;
      r.type = mimeFor(p);
      r.body = readFile(p);
      return r;
    }

    const std::string tok = tokenOf(req);
    const bool host = lobby.isHost(hostTokenOf(req));
    const bool authed = host || lobby.inviteOk(req.get("j"), tok);
    lobby.touch(tok);

    // ------------------------------------------------------------- read
    if (req.path == "/api/state") {
      if (!authed) return deny("this table needs an invite code");
      // Long poll.  The client passes the revision it already has and how long
      // it is prepared to wait; the server answers the moment anything changes,
      // so a table on the far side of a tunnel costs one request per event
      // rather than three a second per player.
      long long since = atoll(req.get("since", "-1").c_str());
      int waitMs = std::max(0, std::min(WAIT_MAX, req.geti("wait", 0)));
      if (waitMs > 0 && since >= 0) {
        std::unique_lock<std::mutex> lk(table.io.mu);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
        while ((long long)table.io.rev <= since) {
          if (table.io.cv.wait_until(lk, deadline) == std::cv_status::timeout) break;
        }
      }
      // The single line that keeps the game a hidden information game: a hand is
      // disclosed only to the credential that claimed its seat.  Ask for
      // somebody else's and you are silently a spectator.
      int want = req.geti("seat", -1);
      int viewSeat = lobby.holdsSeat(tok, want) ? want : -1;
      // The x-ray for watching bots: full hands are emitted only to the HOST,
      // and only when no seat belongs to a person -- a bot has no privacy, a
      // player always does, and an invitee is never shown anything extra.
      bool allBots = true;
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        for (int p = 0; p < NPLAY; p++) allBots = allBots && !table.snap.isHuman[p];
      }
      return json(table.stateJson(viewSeat, authJson(tok, host, true), host && allBots));
    }

    if (req.path.rfind("/api/bots", 0) == 0) return handleBots(req, tok, host, authed);

    // ------------------------------------------------------------- lobby
    if (req.path == "/api/claim") {
      if (!authed) return deny("wrong invite code");
      int p = req.geti("seat", -1);
      if (p < 0 || p >= NPLAY) return err("bad seat");
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        if (!table.snap.isHuman[p]) return err("that seat is not a player's seat", 409);
        if (table.snap.running) return err("that table has already been dealt", 409);
      }
      std::string name = cleanName(req.get("name"));
      if (name.empty()) name = "Player " + std::to_string(p);
      std::string out, why;
      if (!lobby.claim(p, name, tok, out, why)) return err(why, 409);
      // publishConfig reads `seats` (through displayNames) without a lock of its
      // own, so the write and the publish share one critical section or they
      // race a concurrent /api/table on a std::string.
      { std::lock_guard<std::mutex> ck(ctl);
        table.seats[p].human = true; seatNameFromLobby(p); table.publishConfig(); }
      return json("{\"ok\":true,\"seat\":" + std::to_string(p) + ",\"token\":" + jesc(out) + "}");
    }

    if (req.path == "/api/release") {
      int p = req.geti("seat", -1);
      if (!lobby.holdsSeat(tok, p)) return deny("that is not your seat", 403);
      lobby.release(p);
      { std::lock_guard<std::mutex> ck(ctl); seatNameFromLobby(p); table.publishConfig(); }
      return okj();
    }

    // A player who closed the tab has taken their token with them; without this
    // the seat would be unclaimable for the life of the process.
    if (req.path == "/api/kick") {
      if (!host) return deny("only the host may free a seat", 403);
      int p = req.geti("seat", -1);
      if (p < 0 || p >= NPLAY) return err("bad seat");
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        if (table.snap.running) return err("not while a game is running", 409);
      }
      lobby.release(p);
      { std::lock_guard<std::mutex> ck(ctl); seatNameFromLobby(p); table.publishConfig(); }
      return okj();
    }

    // ------------------------------------------------------------- setup
    // Publish the seat configuration without dealing, so that players can see
    // which seats are open and claim one while the host is still setting up.
    if (req.path == "/api/table") {
      if (!host) return deny("only the host may configure the table", 403);
      SeatCfg cfg[NPLAY];
      std::string why;
      if (!readSeats(req, cfg, why)) return err(why);
      std::lock_guard<std::mutex> ck(ctl);
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        if (table.snap.running) return err("a game is already running", 409);
      }
      for (int p = 0; p < NPLAY; p++) table.seats[p] = cfg[p];
      table.publishConfig();
      return okj();
    }   // ctl held across the write and the publish, as above

    if (req.path == "/api/new") {
      if (!host) return deny("only the host may deal", 403);
      SeatCfg cfg[NPLAY];
      std::string why;
      if (!readSeats(req, cfg, why)) return err(why);
      if (!seatsReady(cfg, why)) return err(why, 409);
      Rules r;
      r.deckSets = 9;
      r.maxAsks = std::max(40, std::min(2000, req.geti("maxasks", 400)));
      uint64_t sd = strtoull(req.get("seed", "0").c_str(), nullptr, 10);
      if (!sd) {
        sd = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
        sd = mixSeed(sd, 0xC0FFEEull);
        if (!sd) sd = 1;
      }
      int pace = std::max(0, std::min(PACE_MAX, req.geti("pace", 2000)));
      std::lock_guard<std::mutex> ck(ctl);
      table.stop();
      for (int p = 0; p < NPLAY; p++) table.seats[p] = cfg[p];
      table.rules = r;
      table.seed = sd;
      table.publishConfig();
      {
        std::lock_guard<std::mutex> lk(table.io.mu);
        table.io.paceMs = pace;
        table.io.paused = false;
        table.io.stepBudget = 0;
      }
      table.start();
      return okj();
    }

    if (req.path == "/api/abandon") {
      if (!host) return deny("only the host may end the game", 403);
      std::lock_guard<std::mutex> ck(ctl);
      table.stop();
      return okj();
    }

    // Pause, step and pace are properties of the table rather than of a viewer,
    // so they belong to whoever is running it.
    if (req.path == "/api/pause") {
      if (!host) return deny("only the host may pause the table", 403);
      std::lock_guard<std::mutex> lk(table.io.mu);
      table.io.paused = req.getb("paused", true);
      table.io.stepBudget = 0;
      table.io.bump();
      return okj();
    }
    if (req.path == "/api/step") {
      if (!host) return deny("only the host may step the table", 403);
      std::lock_guard<std::mutex> lk(table.io.mu);
      table.io.paused = true;
      table.io.stepBudget += std::max(1, std::min(50, req.geti("n", 1)));
      table.io.bump();
      return okj();
    }
    if (req.path == "/api/pace") {
      if (!host) return deny("only the host may set the pace", 403);
      std::lock_guard<std::mutex> lk(table.io.mu);
      table.io.paceMs = std::max(0, std::min(PACE_MAX, req.geti("ms", 2000)));
      table.io.bump();
      return okj();
    }

    // -------------------------------------------------------- human moves
    int seat = req.geti("seat", -1);
    if (seat < 0 || seat >= NPLAY) return err("bad seat");
    // A move is accepted only from the credential that claimed the seat, which
    // is the same check that governs seeing its cards.
    if (!lobby.holdsSeat(tok, seat)) return deny("that is not your seat", 403);
    {
      std::lock_guard<std::mutex> lk(table.io.mu);
      if (!table.snap.running) return err("no game in progress", 409);
      if (!table.snap.isHuman[seat]) return err("seat " + std::to_string(seat) + " is not a human seat", 409);
    }
    HumanSlot& sl = table.io.slot[seat];

    if (req.path == "/api/ask") {
      int card = req.geti("card", -1), target = req.geti("target", -1);
      std::lock_guard<std::mutex> lk(table.io.mu);
      if (card < 0 || card >= NCARD || target < 0 || target >= NPLAY) return err("bad ask");
      int s = setOf(card);
      if (!table.snap.setActive[s]) return err("that half-suit is out of play");
      if (teamOf(target) == teamOf(seat)) return err("you may only ask an opponent");
      if (!table.snap.handCount[target]) return err("that player has no cards");
      if (table.snap.hand[seat] & bit(card)) return err("you already hold that card");
      if (!(table.snap.hand[seat] & setMask(s))) return err("you hold no card of that half-suit");
      sl.ask = AskMove{uint8_t(card), uint8_t(target)};
      sl.haveAsk = true;
      sl.note.clear();
      table.io.bump();
      return okj();
    }

    if (req.path == "/api/declare") {
      int set = req.geti("set", -1);
      Declaration d{};
      std::string why;
      if (set < 0 || set >= NSET) return err("bad half-suit");
      if (!parseOwners(req, seat, d, why)) return err(why);
      d.set = uint8_t(set);
      std::lock_guard<std::mutex> lk(table.io.mu);
      if (!table.snap.setActive[set]) return err("that half-suit is out of play");
      sl.decl = d;
      sl.haveDecl = true;
      sl.note.clear();
      table.io.bump();
      return okj();
    }

    if (req.path == "/api/declare/cancel") {
      std::lock_guard<std::mutex> lk(table.io.mu);
      sl.haveDecl = false;
      sl.note.clear();
      table.io.bump();
      return okj();
    }

    if (req.path == "/api/forced") {
      std::lock_guard<std::mutex> lk(table.io.mu);
      if (req.getb("deferall", false)) { sl.forcedDeferAll = true; table.io.bump(); return okj(); }
      if (req.getb("skip", false)) { sl.forcedSkip = true; table.io.bump(); return okj(); }
      return err("forced declaration needs an allocation");
    }

    if (req.path == "/api/forced/declare") {
      int set = req.geti("set", -1);
      Declaration d{};
      std::string why;
      if (set < 0 || set >= NSET) return err("bad half-suit");
      if (!parseOwners(req, seat, d, why)) return err(why);
      d.set = uint8_t(set);
      std::lock_guard<std::mutex> lk(table.io.mu);
      if (sl.needSet != set) return err("that is not the half-suit you were asked about");
      sl.forced = d;
      sl.haveForced = true;
      sl.note.clear();
      table.io.bump();
      return okj();
    }

    if (req.path == "/api/pass") {
      int target = req.geti("target", -1);
      std::lock_guard<std::mutex> lk(table.io.mu);
      bool ok = false;
      for (int i = 0; i < sl.nCands; i++) if (sl.cands[i] == target) ok = true;
      if (!ok) return err("that teammate cannot receive the turn");
      sl.passTo = target;
      sl.havePass = true;
      sl.note.clear();
      table.io.bump();
      return okj();
    }

    return err("not found", 404);
  }
};

struct ServeOptions {
  int port = 8173;
  std::string webDir;
  std::string botsDir;       // --bots=DIR: where uploaded packages live
  bool lockBots = false;     // --lock-bots: only the host may upload one
  bool bindAll = false;      // --lan / --public
  bool forceAuth = false;    // --auth: credentials even on loopback
  bool publicTunnel = false; // --public: borrow an https address from a tunnel
  std::string tunnel = "auto";// --tunnel=cloudflared|ssh|auto
  std::string invite;        // --invite=CODE, otherwise generated
};

inline int runServe(const ServeOptions& opt, const char* argv0) {
  Server srv;
  srv.webDir = opt.webDir;
  if (srv.webDir.empty()) {
    if (fileExists("web/index.html")) srv.webDir = "web";
    else {
      std::vector<char> buf(strlen(argv0) + 1);
      memcpy(buf.data(), argv0, buf.size());
      std::string dir = ::dirname(buf.data());
      if (fileExists(dir + "/web/index.html")) srv.webDir = dir + "/web";
      else srv.webDir = "web";
    }
  }
  // The bot library sits beside the web assets, so a server started from
  // anywhere finds the same one: engine/web -> engine/bots.
  std::string botsDir = opt.botsDir;
  if (botsDir.empty()) {
    const std::string tail = "/web";
    botsDir = (srv.webDir.size() > tail.size() &&
               srv.webDir.compare(srv.webDir.size() - tail.size(), tail.size(), tail) == 0)
                  ? srv.webDir.substr(0, srv.webDir.size() - tail.size()) + "/bots"
                  : "bots";
  }
  botpkg::registry().setRoot(botsDir);
  botpkg::registry().rescan();
  srv.lockBots = opt.lockBots;
  // At a table, a foreign bot that breaks the protocol ends its game and is
  // reported; it does not end the server.  Every batch path leaves this off and
  // still stops dead, because a measured number must never come from a game a
  // bot could not actually play.  (botfault.hpp.)
  botFaultsThrow() = true;
  // One route carries megabytes; the rest keep the 64 KB ceiling they had.  And
  // only for a caller who has already presented a credential: the invite is
  // cheap to check and the alternative is letting anyone who can reach the port
  // make this process allocate 64 MB before a handler runs.
  bodyLimit() = [&srv](const HttpRequest& r) -> size_t {
    if (r.method != "POST" || r.path != "/api/bots/upload") return MAX_BODY;
    if (Server::crossOrigin(r)) return MAX_BODY;
    const bool host = srv.lobby.isHost(Server::hostTokenOf(r));
    if (!host && !srv.lobby.inviteOk(r.get("j"), Server::tokenOf(r))) return MAX_BODY;
    // The body is read into memory before any handler runs, and the socket
    // server will serve MAX_CONNS of them at once, so the ceiling has to be
    // paired with a limit on how many can be climbing it together.  Two: a
    // table is six people taking turns, not a fleet.
    return bigBodiesInFlight() < 2 ? botpkg::MAX_ZIP_BYTES : MAX_BODY;
  };
  // Reachable from another machine means reachable by somebody who was not
  // invited, so the credentials come on automatically rather than by being
  // remembered.  A loopback table stays exactly as open as it always was.
  // --public reaches further than --lan even though it binds narrower, so it
  // turns the credentials on for the same reason.
  const bool guard = opt.bindAll || opt.forceAuth || opt.publicTunnel;
  srv.lobby.init(guard, opt.invite);

  int bound = 0;
  int fd = httpListen(opt.port, bound, opt.bindAll);
  if (fd < 0) { fprintf(stderr, "fish serve: could not bind a port at or above %d\n", opt.port); return 1; }

  printf("FishLab table\n");
  if (!guard) {
    printf("  open http://127.0.0.1:%d in a browser\n", bound);
    printf("  loopback only -- pass --lan or --public to let other people in\n");
  } else {
    printf("  HOST  http://127.0.0.1:%d/?h=%s\n", bound, srv.lobby.hostToken.c_str());
    printf("        ^ open this one yourself: it is what lets you configure and deal.\n");
    printf("  INVITE CODE  %s\n", srv.lobby.invite.c_str());
    if (opt.bindAll) {
      auto addrs = localAddresses();
      for (const auto& ip : addrs)
        printf("  PLAYERS (same network)  http://%s:%d/?j=%s\n", ip.c_str(), bound, srv.lobby.invite.c_str());
      if (!addrs.empty())
        srv.shareUrl = "http://" + addrs.front() + ":" + std::to_string(bound);
    }
    printf("  Players see only their own hand; the host token is not a card-visibility\n"
           "  credential and is never sent to anybody else.\n");
  }
  {
    size_t n = botpkg::registry().all().size();
    printf("\n  Bots        %s  (%zu installed)\n", botpkg::registry().rootDir().c_str(), n);
    if (!guard)
      printf("              you may upload a .zip package from the setup screen.\n");
    else if (opt.lockBots)
      printf("              --lock-bots: only you may upload. Guests can see and download\n"
             "              what you add, but not contribute one of their own.\n");
    else
      printf("              anyone with the invite code may UPLOAD a package; only you can\n"
             "              seat, check or install one, and nothing in a package runs until\n"
             "              you do.  --lock-bots to keep uploading to yourself.\n");
    printf("              format: docs/BOT_PACKAGE.md\n");
  }
  fflush(stdout);

  // The tunnel is started after the listener is up, so the first request it
  // forwards cannot beat the server to the port.
  Tunnel tun;
  if (opt.publicTunnel) {
    std::string url;
    printf("\n  opening a public address");
    if (opt.tunnel != "auto") printf(" via %s", opt.tunnel.c_str());
    printf("...\n");
    fflush(stdout);
    if (tun.start(bound, url, opt.tunnel)) {
      srv.shareUrl = url;                 // beats a LAN address: it works from both
      printf("\n  PLAYERS (anywhere)  %s/?j=%s\n", url.c_str(), srv.lobby.invite.c_str());
      printf("  Send that one link to everybody -- it carries the invite code. The tunnel\n"
             "  closes when this process does, and %s can see the traffic on the way\n"
             "  through, so it is for a card game and not for anything confidential.\n",
             tun.via.c_str());
    } else {
      printf("\n  --public: no tunnel came up.\n            %s\n", tun.error.c_str());
      // Without --lan the socket is loopback-only, so a failed tunnel leaves
      // nobody able to reach the table but the person sitting at it.  Say so
      // rather than implying the addresses above are of any use to a guest.
      if (opt.bindAll) printf("  The table is still reachable on the network addresses above.\n");
      else printf("  The table is reachable only from this machine. Re-run with --lan (or\n"
                  "  --lan --public) to let other people in.\n");
    }
    fflush(stdout);
  }
  printf("\n  Ctrl-C to stop.\n");
  fflush(stdout);

  httpAcceptLoop(fd, [&srv](const HttpRequest& r) { return srv.handle(r); });
  return 0;
}

} // namespace fish
