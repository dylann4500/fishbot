// Minimal HTTP/1.1 server for the interactive table.  Loopback only, one
// detached thread per connection, Connection: close.  It exists so that
// `fish serve` needs no runtime beyond the engine binary itself.
#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <thread>
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

namespace fish {

struct HttpRequest {
  std::string method, path, body;
  std::map<std::string, std::string> params;   // query string and form body merged
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
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 500: return "Internal Server Error";
  }
  return "OK";
}

inline void httpServeConn(int fd, const HttpHandler* handler) {
  std::string buf;
  char tmp[8192];
  size_t headEnd = std::string::npos;
  while (headEnd == std::string::npos) {
    ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
    if (r <= 0) { ::close(fd); return; }
    buf.append(tmp, size_t(r));
    headEnd = buf.find("\r\n\r\n");
    if (buf.size() > (1u << 20)) { ::close(fd); return; }
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
  size_t contentLength = 0;
  {
    std::string lower = head;
    for (auto& c : lower) c = char(tolower((unsigned char)c));
    size_t p = lower.find("content-length:");
    if (p != std::string::npos) contentLength = size_t(atoll(head.c_str() + p + 15));
  }
  while (body.size() < contentLength) {
    ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
    if (r <= 0) break;
    body.append(tmp, size_t(r));
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
// and writes the port actually bound; -1 on failure.
inline int httpListen(int port, int& boundPort, int tries = 20) {
  for (int i = 0; i < tries; i++) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(port + i));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::bind(fd, (sockaddr*)&addr, sizeof(addr)) == 0 && ::listen(fd, 32) == 0) {
      boundPort = port + i;
      return fd;
    }
    ::close(fd);
  }
  return -1;
}

inline void httpAcceptLoop(int listenFd, HttpHandler handler) {
  ::signal(SIGPIPE, SIG_IGN);
  static HttpHandler h;
  h = std::move(handler);
  for (;;) {
    int fd = ::accept(listenFd, nullptr, nullptr);
    if (fd < 0) { if (errno == EINTR) continue; break; }
    int on = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    std::thread(httpServeConn, fd, &h).detach();
  }
}

} // namespace fish
