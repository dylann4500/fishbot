// `fish serve` -- HTTP front end for the interactive table.
#pragma once
#include "table.hpp"
#include <fstream>
#include <sstream>
#include <libgen.h>
#include <sys/stat.h>

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

struct Server {
  Table table;
  std::string webDir;
  std::mutex ctl;            // serialises new/abandon against each other

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

  HttpResponse handle(const HttpRequest& req) {
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

    // ------------------------------------------------------------- read
    if (req.path == "/api/state") return json(table.stateJson(req.geti("seat", -1)));

    // ------------------------------------------------------------- setup
    if (req.path == "/api/new") {
      SeatCfg cfg[NPLAY];
      int nHuman = 0;
      for (int p = 0; p < NPLAY; p++) {
        char key[4]; snprintf(key, sizeof(key), "s%d", p);
        std::string spec = req.get(key, "v04");
        if (spec.empty()) spec = "v04";
        if (!knownPolicy(spec)) return err("unknown policy '" + spec + "' at seat " + std::to_string(p));
        cfg[p].spec = spec;
        char hk[4]; snprintf(hk, sizeof(hk), "h%d", p);
        cfg[p].human = req.getb(hk, false);
        if (cfg[p].human) nHuman++;
      }
      (void)nHuman;
      Rules r;
      r.deckSets = 9;
      r.maxAsks = std::max(40, std::min(2000, req.geti("maxasks", 400)));
      uint64_t sd = strtoull(req.get("seed", "0").c_str(), nullptr, 10);
      if (!sd) {
        sd = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
        sd = mixSeed(sd, 0xC0FFEEull);
        if (!sd) sd = 1;
      }
      int pace = std::max(0, std::min(6000, req.geti("pace", 800)));
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

    if (req.path == "/api/abandon") { std::lock_guard<std::mutex> ck(ctl); table.stop(); return okj(); }

    if (req.path == "/api/pause") {
      std::lock_guard<std::mutex> lk(table.io.mu);
      table.io.paused = req.getb("paused", true);
      table.io.stepBudget = 0;
      table.io.bump();
      return okj();
    }
    if (req.path == "/api/step") {
      std::lock_guard<std::mutex> lk(table.io.mu);
      table.io.paused = true;
      table.io.stepBudget += std::max(1, std::min(50, req.geti("n", 1)));
      table.io.bump();
      return okj();
    }
    if (req.path == "/api/pace") {
      std::lock_guard<std::mutex> lk(table.io.mu);
      table.io.paceMs = std::max(0, std::min(6000, req.geti("ms", 800)));
      table.io.bump();
      return okj();
    }

    // -------------------------------------------------------- human moves
    int seat = req.geti("seat", -1);
    if (seat < 0 || seat >= NPLAY) return err("bad seat");
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

inline int runServe(int port, const std::string& webDirArg, const char* argv0) {
  Server srv;
  srv.webDir = webDirArg;
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
  int bound = 0;
  int fd = httpListen(port, bound);
  if (fd < 0) { fprintf(stderr, "fish serve: could not bind a port at or above %d\n", port); return 1; }
  printf("FishLab table -- open http://127.0.0.1:%d in a browser\n", bound);
  printf("  web assets: %s\n", srv.webDir.c_str());
  printf("  Ctrl-C to stop.\n");
  fflush(stdout);
  httpAcceptLoop(fd, [&srv](const HttpRequest& r) { return srv.handle(r); });
  return 0;
}

} // namespace fish
