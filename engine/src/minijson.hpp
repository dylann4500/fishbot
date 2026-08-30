// A small JSON reader, for the two places that need to read JSON somebody else
// wrote: a bot package's manifest, and a bot's replies over the protocol in
// extbot.hpp.
//
// The engine already writes JSON by hand (table.hpp) and reads KV's replies with
// a key scanner (kv6.hpp).  Neither is enough here: a manifest has nested
// objects and arrays, and the input comes from outside the project, so "find the
// key and hope" is the wrong shape.  This is a real parser -- bounded depth,
// bounded size, an error message rather than a guess -- and still small enough
// to read in one sitting.
//
// Deliberately not general: numbers are doubles, duplicate keys keep the last,
// and there is no writer beyond escape().  Nothing measured depends on it.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace fish {
namespace mj {

struct Value {
  enum Type { Null, Bool, Num, Str, Arr, Obj };
  Type type = Null;
  bool b = false;
  double num = 0;
  std::string str;
  std::vector<Value> arr;
  std::vector<std::pair<std::string, Value>> obj;

  bool isNull() const { return type == Null; }
  bool isObj()  const { return type == Obj; }
  bool isArr()  const { return type == Arr; }
  bool isStr()  const { return type == Str; }
  bool isNum()  const { return type == Num; }
  bool isBool() const { return type == Bool; }

  const Value* find(const char* k) const {
    if (type != Obj) return nullptr;
    for (const auto& kv : obj) if (kv.first == k) return &kv.second;
    return nullptr;
  }
  bool has(const char* k) const { return find(k) != nullptr; }
  std::string s(const char* k, const char* dflt = "") const {
    const Value* v = find(k);
    return (v && v->type == Str) ? v->str : std::string(dflt);
  }
  double n(const char* k, double dflt) const {
    const Value* v = find(k);
    return (v && v->type == Num) ? v->num : dflt;
  }
  bool bo(const char* k, bool dflt) const {
    const Value* v = find(k);
    return (v && v->type == Bool) ? v->b : dflt;
  }
};

struct Parser {
  const char* p = nullptr;
  const char* end = nullptr;
  std::string err;
  int depth = 0;
  static constexpr int MAXDEPTH = 40;

  void ws() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  }
  bool fail(const char* what) {
    if (err.empty()) err = what;
    return false;
  }
  bool lit(const char* s, size_t n) {
    if (size_t(end - p) < n || memcmp(p, s, n) != 0) return false;
    p += n;
    return true;
  }

  bool str(std::string& out) {
    if (p >= end || *p != '"') return fail("expected a string");
    p++;
    out.clear();
    while (p < end && *p != '"') {
      unsigned char c = (unsigned char)*p;
      if (c < 0x20) return fail("a control character inside a string");
      if (c != '\\') { out.push_back(char(c)); p++; continue; }
      p++;
      if (p >= end) return fail("a string ends inside an escape");
      char e = *p++;
      switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          if (end - p < 4) return fail("a truncated \\u escape");
          auto hex4 = [&](const char* h, unsigned& v) {
            v = 0;
            for (int i = 0; i < 4; i++) {
              char d = h[i];
              int x = (d >= '0' && d <= '9') ? d - '0'
                    : (d >= 'a' && d <= 'f') ? d - 'a' + 10
                    : (d >= 'A' && d <= 'F') ? d - 'A' + 10 : -1;
              if (x < 0) return false;
              v = v * 16 + unsigned(x);
            }
            return true;
          };
          unsigned cp = 0;
          if (!hex4(p, cp)) return fail("a malformed \\u escape");
          p += 4;
          if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
            unsigned lo = 0;
            if (hex4(p + 2, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
              p += 6;
            }
          }
          // Lone surrogates become U+FFFD rather than an invalid encoding.
          if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
          if (cp < 0x80) out.push_back(char(cp));
          else if (cp < 0x800) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
          } else if (cp < 0x10000) {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
          } else {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
          }
          break;
        }
        default: return fail("an unknown escape in a string");
      }
    }
    if (p >= end) return fail("a string is never closed");
    p++;
    return true;
  }

  bool value(Value& v) {
    if (++depth > MAXDEPTH) return fail("the document nests too deeply");
    struct Pop { int& d; ~Pop() { d--; } } pop{depth};
    ws();
    if (p >= end) return fail("the document ends where a value was expected");
    switch (*p) {
      case '{': {
        p++;
        v.type = Value::Obj;
        ws();
        if (p < end && *p == '}') { p++; return true; }
        for (;;) {
          ws();
          std::string k;
          if (!str(k)) return false;
          ws();
          if (p >= end || *p != ':') return fail("expected ':' after a key");
          p++;
          Value child;
          if (!value(child)) return false;
          bool replaced = false;
          for (auto& kv : v.obj) if (kv.first == k) { kv.second = child; replaced = true; break; }
          if (!replaced) v.obj.emplace_back(k, std::move(child));
          ws();
          if (p < end && *p == ',') { p++; continue; }
          if (p < end && *p == '}') { p++; return true; }
          return fail("expected ',' or '}' in an object");
        }
      }
      case '[': {
        p++;
        v.type = Value::Arr;
        ws();
        if (p < end && *p == ']') { p++; return true; }
        for (;;) {
          Value child;
          if (!value(child)) return false;
          v.arr.push_back(std::move(child));
          ws();
          if (p < end && *p == ',') { p++; continue; }
          if (p < end && *p == ']') { p++; return true; }
          return fail("expected ',' or ']' in an array");
        }
      }
      case '"':
        v.type = Value::Str;
        return str(v.str);
      case 't':
        if (!lit("true", 4)) return fail("expected a value");
        v.type = Value::Bool; v.b = true;
        return true;
      case 'f':
        if (!lit("false", 5)) return fail("expected a value");
        v.type = Value::Bool; v.b = false;
        return true;
      case 'n':
        if (!lit("null", 4)) return fail("expected a value");
        v.type = Value::Null;
        return true;
      default: {
        char* stop = nullptr;
        double d = strtod(p, &stop);
        if (stop == p) return fail("expected a value");
        // std::string guarantees a NUL at data()+size(), so strtod stops at the
        // end of the document rather than reading past it; this catches the
        // case where it stopped somewhere it should not have anyway.
        if (stop > end) return fail("a number runs past the end of the document");
        if (!std::isfinite(d)) return fail("a number is not finite");
        p = stop;
        v.type = Value::Num;
        v.num = d;
        return true;
      }
    }
  }
};

inline bool parse(const std::string& text, Value& out, std::string& err) {
  if (text.size() > 8u * 1024 * 1024) { err = "that JSON document is implausibly large"; return false; }
  Parser ps;
  ps.p = text.data();
  ps.end = text.data() + text.size();
  out = Value{};
  if (!ps.value(out)) { err = ps.err.empty() ? "malformed JSON" : ps.err; return false; }
  ps.ws();
  if (ps.p != ps.end) { err = "trailing text after the JSON document"; return false; }
  return true;
}

// Same escaping rules as table.hpp's jesc, kept here so this header stands
// alone; both quote the same set and neither emits a raw control byte.
inline std::string escape(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    switch (c) {
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default:
        if ((unsigned char)c < 0x20) {
          char b[8];
          snprintf(b, sizeof(b), "\\u%04x", c);
          o += b;
        } else o += c;
    }
  }
  return o + "\"";
}

}  // namespace mj
}  // namespace fish
