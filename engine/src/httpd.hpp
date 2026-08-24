// Minimal HTTP/1.1 server for the interactive table.  One detached thread per
// connection, Connection: close.  It exists so that `fish serve` needs no
// runtime beyond the engine binary itself.
//
// It binds loopback unless asked otherwise.  Off loopback it is reachable by
// anything that can route to the host, so the limits here are load-bearing
// rather than tidiness: a bounded request body, a socket deadline, and a
// bounded header count are what stop one rude connection from holding a thread
// or a gigabyte.  Everything above this layer authenticates (see lobby.hpp).
#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <cerrno>
#include <cctype>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace fish {

struct HttpRequest {
  std::string method, path, body;
  std::map<std::string, std::string> params;   // query string and form body merged
  std::map<std::string, std::string> headers;  // lower-cased names
  std::string peer;                            // client address, for the log
  std::string header(const char* k, const char* dflt = "") const {
    auto it = headers.find(k);
    return it == headers.end() ? std::string(dflt) : it->second;
  }
  std::string get(const char* k, const char* dflt = "") const {
    auto it = params.find(k);
    return it == params.end() ? std::string(dflt) : it->second;
  }
  int geti(const char* k, int dflt) const {
    auto it = params.find(k);
    return it == params.end() || it->second.empty() ? dflt : atoi(it->second.c_str());
  }
  bool getb(const char* k, bool dflt) const {
    auto it = params.find(k);
    if (it == params.end()) return dflt;
    return it->second == "1" || it->second == "true" || it->second == "yes";
  }
};

struct HttpResponse {
  int status = 200;
  std::string type = "text/plain; charset=utf-8";
  std::string body;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

inline std::string urlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') out.push_back(' ');
    else if (s[i] == '%' && i + 2 < s.size()) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
      if (hi >= 0 && lo >= 0) { out.push_back(char(hi * 16 + lo)); i += 2; }
      else out.push_back(s[i]);
    } else out.push_back(s[i]);
  }
  return out;
}

inline void parseForm(const std::string& q, std::map<std::string, std::string>& out) {
  size_t i = 0;
  while (i < q.size()) {
    size_t amp = q.find('&', i);
    if (amp == std::string::npos) amp = q.size();
    std::string item = q.substr(i, amp - i);
    size_t eq = item.find('=');
    if (eq != std::string::npos) out[urlDecode(item.substr(0, eq))] = urlDecode(item.substr(eq + 1));
    else if (!item.empty()) out[urlDecode(item)] = "";
    i = amp + 1;
  }
}

inline bool sendAll(int fd, const char* p, size_t n) {
  while (n) {
    ssize_t w = ::send(fd, p, n, 0);
    if (w <= 0) return false;
    p += w; n -= size_t(w);
  }
  return true;
}

inline const char* statusText(int s) {
  switch (s) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
  }
  return "OK";
}

// A request body here is a handful of form fields; anything larger is either a
// mistake or an attempt to make the server allocate on demand.
static constexpr size_t MAX_BODY = 64 * 1024;
static constexpr size_t MAX_HEAD = 32 * 1024;

inline void httpServeConn(int fd, const HttpHandler* handler, std::string peer) {
  // A read deadline is what makes a stalled or malicious client cost one socket
  // for a few seconds rather than one thread forever.  The write deadline is
  // longer because a long-polled reply can legitimately be sent to a client on
  // a slow link.
  timeval rt{8, 0}, wt{30, 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof(rt));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &wt, sizeof(wt));
  std::string buf;
  char tmp[8192];
  size_t headEnd = std::string::npos;
  while (headEnd == std::string::npos) {
    ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
    if (r <= 0) { ::close(fd); return; }
    buf.append(tmp, size_t(r));
    headEnd = buf.find("\r\n\r\n");
    if (buf.size() > MAX_HEAD) { ::close(fd); return; }
  }
  std::string head = buf.substr(0, headEnd);
  std::string body = buf.substr(headEnd + 4);

  size_t eol = head.find("\r\n");
  std::string line = head.substr(0, eol == std::string::npos ? head.size() : eol);
  HttpRequest req;
  {
    size_t a = line.find(' ');
    size_t b = a == std::string::npos ? std::string::npos : line.find(' ', a + 1);
    if (a == std::string::npos || b == std::string::npos) { ::close(fd); return; }
    req.method = line.substr(0, a);
    std::string target = line.substr(a + 1, b - a - 1);
    size_t q = target.find('?');
    if (q == std::string::npos) req.path = urlDecode(target);
    else { req.path = urlDecode(target.substr(0, q)); parseForm(target.substr(q + 1), req.params); }
  }
  req.peer = std::move(peer);
  // Headers, lower-cased.  The seat and host credentials travel in one
  // (X-Fish-Token) rather than in the query string, so they stay out of proxy
  // logs and browser history on the way through a tunnel.
  {
    size_t at = eol == std::string::npos ? head.size() : eol + 2;
    while (at < head.size()) {
      size_t e = head.find("\r\n", at);
      if (e == std::string::npos) e = head.size();
      std::string h = head.substr(at, e - at);
      size_t c = h.find(':');
      if (c != std::string::npos && req.headers.size() < 64) {
        std::string k = h.substr(0, c);
        for (auto& ch : k) ch = char(tolower((unsigned char)ch));
        size_t v = h.find_first_not_of(" \t", c + 1);
        req.headers[k] = v == std::string::npos ? std::string() : h.substr(v);
      }
      at = e + 2;
    }
  }
  size_t contentLength = 0;
  {
    auto it = req.headers.find("content-length");
    if (it != req.headers.end()) {
      long long n = atoll(it->second.c_str());
      if (n < 0 || size_t(n) > MAX_BODY) {
        const char* over = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\n"
                           "Connection: close\r\n\r\n";
        sendAll(fd, over, strlen(over));
        ::close(fd);
        return;
      }
      contentLength = size_t(n);
    }
  }
  while (body.size() < contentLength) {
    ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
    if (r <= 0) break;
    body.append(tmp, size_t(r));
    if (body.size() > MAX_BODY) { ::close(fd); return; }
  }
  body.resize(std::min(body.size(), contentLength ? contentLength : body.size()));
  req.body = body;
  if (!body.empty()) parseForm(body, req.params);

  HttpResponse res;
  try {
    res = (*handler)(req);
  } catch (...) {
    res.status = 500;
    res.body = "internal error";
  }
  char hdr[512];
  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                   "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                   res.status, statusText(res.status), res.type.c_str(), res.body.size());
  if (sendAll(fd, hdr, size_t(n))) sendAll(fd, res.body.data(), res.body.size());
  ::shutdown(fd, SHUT_WR);
  ::close(fd);
}

// Binds the first free port at or above `port`.  Returns the listening socket
// and writes the port actually bound; -1 on failure.  `bindAll` binds every
// interface instead of loopback, which is what makes the table reachable from
// another machine -- and what turns on the lobby's authentication.
inline int httpListen(int port, int& boundPort, bool bindAll = false, int tries = 20) {
  for (int i = 0; i < tries; i++) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(port + i));
    addr.sin_addr.s_addr = htonl(bindAll ? INADDR_ANY : INADDR_LOOPBACK);
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) == 0 && ::listen(fd, 32) == 0) {
      // `fish serve --public` forks and execs cloudflared, and a child inherits
      // every descriptor it is not told to drop -- including this one, which
      // would leave the tunnel process holding the table's listening socket for
      // as long as it ran.
      ::fcntl(fd, F_SETFD, FD_CLOEXEC);
      boundPort = port + i;
      return fd;
    }
    ::close(fd);
  }
  return -1;
}

// A long poll deliberately holds its connection open, so "one thread per
// connection" stops being self-limiting the moment the port is reachable.  Six
// players need a handful; this is the backstop that keeps a rude client from
// turning the table into a thread bomb.
static constexpr int MAX_CONNS = 128;
inline std::atomic<int>& liveConns() { static std::atomic<int> n{0}; return n; }

struct ConnGuard {
  ConnGuard() { liveConns().fetch_add(1); }
  ~ConnGuard() { liveConns().fetch_sub(1); }
};

inline void httpAcceptLoop(int listenFd, HttpHandler handler) {
  ::signal(SIGPIPE, SIG_IGN);
  static HttpHandler h;
  h = std::move(handler);
  for (;;) {
    sockaddr_in who{};
    socklen_t wholen = sizeof(who);
    int fd = ::accept(listenFd, (sockaddr*)&who, &wholen);
    if (fd < 0) { if (errno == EINTR) continue; break; }
    int on = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    char ip[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &who.sin_addr, ip, sizeof(ip));
    if (liveConns().load() >= MAX_CONNS) {
      const char* busy = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\n"
                         "Connection: close\r\n\r\n";
      sendAll(fd, busy, strlen(busy));
      ::close(fd);
      continue;
    }
    std::thread([fd, ip = std::string(ip)] { ConnGuard g; httpServeConn(fd, &h, ip); }).detach();
  }
}

} // namespace fish
