// `fish serve --public`: borrow a public https address for the loopback table.
//
// Playing with people who are not on your wifi needs an address they can reach,
// and the two ways to get one are to open a port on the router or to have
// something outside dial in and forward.  The second is strictly better here:
// no inbound hole, TLS for free, and an address that disappears when the
// process does.
//
// There is more than one way to make that outbound call and they do not use the
// same port, which matters because plenty of networks block one and not the
// other:
//
//   * **cloudflared** needs outbound **7844**, and nothing else will do.  It is
//     the first choice where it works: no account, no shelling out to ssh, and a
//     quick tunnel is a supported product rather than a favour.
//   * **localhost.run** is a reverse SSH forward over **22**, which is open on a
//     great many networks that drop 7844.  No account either.
//
// So --public tries them in order and reports the first that actually comes up.
// "Actually" is the load-bearing word: both services hand out an address before
// the tunnel behind it is established, and publishing an address that answers
// 502 to everybody the host has just messaged is worse than admitting failure.
// Each provider therefore names a marker line that means *connected*, and no
// address is reported until that line appears.
//
// The server stays bound to loopback while a tunnel runs -- only the tunnel
// client talks to it -- so --public is a narrower exposure than --lan even
// though it reaches further.  Credentials stay on regardless: the address is
// unguessable, and an unguessable URL is not an authorisation scheme.
//
// One thing to know before using either: both services terminate TLS at their
// edge, so the operator can see the traffic, seat tokens included.  That is the
// standard bargain for a free tunnel and it is fine for a card game among
// friends; it would not be fine for anything else.
#pragma once
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>
#include <cctype>

namespace fish {

// The child has to die with us: an orphaned tunnel would keep publishing a
// public address for a port nothing is listening on.  A file-scope pid plus a
// signal handler is the only thing that survives the process being killed with
// Ctrl-C, which is how this is always going to end.
inline pid_t& tunnelPid() { static pid_t p = -1; return p; }

inline void tunnelKill() {
  pid_t p = tunnelPid();
  if (p > 0) { ::kill(p, SIGTERM); tunnelPid() = -1; }
}

inline void tunnelSignal(int sig) {
  tunnelKill();
  ::signal(sig, SIG_DFL);
  ::raise(sig);
}

// The last "ERR ..." the client logged, which is the only part of a very chatty
// log worth putting in front of somebody.
inline std::string lastErrorLine(const std::string& log) {
  size_t at = log.rfind(" ERR ");
  if (at == std::string::npos) return std::string();
  size_t b = log.find_first_of("\r\n", at);
  std::string line = log.substr(at + 5, (b == std::string::npos ? log.size() : b) - at - 5);
  if (line.size() > 160) line = line.substr(0, 160) + "...";
  return line;
}

// cloudflared failing to reach its edge is worth naming rather than retrying:
// it is a property of the network, and the fix is a different provider.
inline bool blockedEdge(const std::string& log) {
  return log.find(":7844") != std::string::npos
      || log.find("failed to dial to edge") != std::string::npos
      || log.find("TLS handshake with edge error") != std::string::npos;
}

// Pulls the first https://<host><suffix> out of a chunk of log.
inline std::string scanForUrl(const std::string& s, const std::string& suffix) {
  size_t at = 0;
  for (;;) {
    size_t i = s.find("https://", at);
    if (i == std::string::npos) return std::string();
    size_t j = i + 8;
    while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '-')) j++;
    std::string u = s.substr(i, j - i);
    if (u.size() > suffix.size() && u.compare(u.size() - suffix.size(), suffix.size(), suffix) == 0)
      return u;
    at = i + 8;
  }
}

struct Provider {
  const char* key;                     // what --tunnel= accepts
  const char* label;                   // what the console calls it
  std::vector<std::string> argv;       // %P is replaced by the bound port
  const char* suffix;                  // the address family it hands out
  const char* upMarker;                // the line that means "connected"
  const char* missingHint;             // what to say if the binary is absent
};

inline std::vector<Provider> providers() {
  return {
    { "cloudflared", "cloudflared",
      {"cloudflared", "tunnel", "--no-autoupdate", "--url", "http://127.0.0.1:%P"},
      ".trycloudflare.com", "Registered tunnel connection",
      "not installed (brew install cloudflared)" },
    // -R 80:... asks for an http forward; localhost.run terminates TLS in front
    // of it.  BatchMode stops ssh waiting on a password prompt nobody will ever
    // see, and accept-new pins the host key on first use rather than either
    // prompting or trusting it blindly on every run.
    { "ssh", "localhost.run over ssh",
      {"ssh", "-o", "StrictHostKeyChecking=accept-new", "-o", "BatchMode=yes",
       "-o", "ExitOnForwardFailure=yes", "-o", "ServerAliveInterval=30",
       "-o", "ServerAliveCountMax=3", "-R", "80:localhost:%P", "nokey@localhost.run"},
      ".lhr.life", "tunneled with tls termination",
      "not installed (ssh is normally already present)" },
  };
}

struct Tunnel {
  std::string error;
  std::string via;          // the provider that actually worked

  bool start(int port, std::string& url, const std::string& want = "auto", int waitSeconds = 30) {
    std::string tried;
    bool matched = false;
    for (const auto& p : providers()) {
      if (want != "auto" && want != p.key) continue;
      matched = true;
      std::string why;
      if (tryOne(p, port, url, waitSeconds, why)) { via = p.label; return true; }
      if (!tried.empty()) tried += "\n            ";
      tried += std::string(p.label) + ": " + why;
    }
    error = matched ? tried : ("no tunnel provider named '" + want + "'");
    return false;
  }

private:
  bool tryOne(const Provider& prov, int port, std::string& url, int waitSeconds, std::string& why) {
    url.clear();
    int fds[2];
    if (::pipe(fds) != 0) { why = "could not open a pipe"; return false; }
    pid_t pid = ::fork();
    if (pid < 0) { ::close(fds[0]); ::close(fds[1]); why = "could not fork"; return false; }
    if (pid == 0) {
      // Child: both streams into the pipe, because which one carries the
      // address differs between providers and between releases of each.
      ::close(fds[0]);
      ::dup2(fds[1], STDOUT_FILENO);
      ::dup2(fds[1], STDERR_FILENO);
      ::close(fds[1]);
      // stdin must not be a terminal, or ssh tries to allocate one.
      int devnull = ::open("/dev/null", O_RDONLY);
      if (devnull >= 0) { ::dup2(devnull, STDIN_FILENO); ::close(devnull); }
      std::vector<std::string> args = prov.argv;
      std::vector<char*> cargv;
      char portBuf[16];
      snprintf(portBuf, sizeof(portBuf), "%d", port);
      for (auto& a : args) {
        size_t at = a.find("%P");
        if (at != std::string::npos) a.replace(at, 2, portBuf);
        cargv.push_back(const_cast<char*>(a.c_str()));
      }
      cargv.push_back(nullptr);
      ::execvp(cargv[0], cargv.data());
      ::_exit(127);                       // execvp only returns on failure
    }
    ::close(fds[1]);
    tunnelPid() = pid;
    ::signal(SIGINT, tunnelSignal);
    ::signal(SIGTERM, tunnelSignal);
    ::signal(SIGHUP, tunnelSignal);
    static bool once = false;
    if (!once) { once = true; ::atexit(tunnelKill); }

    ::fcntl(fds[0], F_SETFL, O_NONBLOCK);
    std::string acc;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(waitSeconds);
    bool childGone = false, up = false;
    int edgeFailures = 0;
    while (std::chrono::steady_clock::now() < deadline) {
      char buf[4096];
      ssize_t n = ::read(fds[0], buf, sizeof(buf));
      if (n > 0) {
        acc.append(buf, size_t(n));
        if (url.empty()) url = scanForUrl(acc, prov.suffix);
        if (!up && acc.find(prov.upMarker) != std::string::npos) up = true;
        if (!url.empty() && up) break;
        // Retrying the edge forever is the client's business, but the host is
        // standing at a console waiting to send a link, so two failures is
        // enough to stop waiting and try the next provider.
        if (blockedEdge(acc) && ++edgeFailures >= 2) break;
        if (acc.size() > (1u << 20)) acc.erase(0, acc.size() - 65536);
      } else if (n == 0) {
        childGone = true;
        break;
      } else {
        int st = 0;
        if (::waitpid(pid, &st, WNOHANG) == pid) { childGone = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
      }
    }

    if (!url.empty() && up) {
      // The client keeps logging for the life of the tunnel, and a pipe nobody
      // drains eventually blocks the writer -- which would freeze the tunnel
      // rather than the table, and be all the more confusing for it.
      int rfd = fds[0];
      std::thread([rfd] {
        char buf[4096];
        for (;;) {
          ssize_t n = ::read(rfd, buf, sizeof(buf));
          if (n > 0) continue;
          if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
          }
          break;
        }
        ::close(rfd);
      }).detach();
      return true;
    }

    const std::string got = url;
    url.clear();
    tunnelKill();
    int st = 0;
    ::waitpid(pid, &st, WNOHANG);
    if (got.empty() && childGone && acc.empty()) why = prov.missingHint;
    else if (got.empty() && blockedEdge(acc))
      why = "this network blocks outbound port 7844, the only port it can use";
    else if (got.empty() && childGone)          why = "exited before publishing an address";
    else if (got.empty())                       why = "published no address within "
                                                    + std::to_string(waitSeconds) + "s";
    else if (blockedEdge(acc))
      why = "reached " + got + " but this network blocks outbound port 7844";
    else {
      why = "published " + got + " but never reported the tunnel as connected";
      std::string last = lastErrorLine(acc);
      if (!last.empty()) why += " (" + last + ")";
    }
    ::close(fds[0]);
    return false;
  }
};

} // namespace fish
