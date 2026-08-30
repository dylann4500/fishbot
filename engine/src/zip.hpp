// A ZIP reader, so that `fish serve` can accept an uploaded bot package without
// growing a dependency.
//
// The engine's whole premise is that the binary is the artifact -- `fish serve`
// "needs no runtime beyond the engine binary itself" (httpd.hpp) -- and shelling
// out to unzip(1) would break that on the one path where it matters most, the
// one that takes bytes from somebody else's machine.  So DEFLATE (RFC 1951) is
// decoded here, in about two hundred lines, and the container is walked from the
// central directory rather than by scanning for local headers.
//
// Nothing in this file is reachable from a measured code path: it is included
// only by botpkg.hpp, which is included only by factory.hpp's `bot:` branch and
// by serve.hpp.
//
// What it deliberately refuses, because the input is untrusted:
//   * absolute paths, `..` anywhere in a name, and backslashes;
//   * symbolic links (a link is how an archive escapes the directory it was
//     told to unpack into, and no bot package needs one);
//   * ZIP64, encrypted entries, and methods other than store and deflate --
//     each is reported by name rather than silently producing a short file;
//   * anything past the caller's entry-count and byte budgets.
// Every entry's CRC-32 is checked against the central directory, so a truncated
// or tampered upload fails at extraction rather than at first use.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace fish {
namespace zipf {

// ---------------------------------------------------------------- crc32
inline const uint32_t* crcTable() {
  static uint32_t t[256];
  static bool built = false;
  if (!built) {
    for (uint32_t i = 0; i < 256; i++) {
      uint32_t c = i;
      for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      t[i] = c;
    }
    built = true;
  }
  return t;
}
inline uint32_t crc32of(const void* data, size_t n, uint32_t seed = 0) {
  const uint32_t* t = crcTable();
  uint32_t c = seed ^ 0xFFFFFFFFu;
  const uint8_t* p = (const uint8_t*)data;
  for (size_t i = 0; i < n; i++) c = t[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

// ------------------------------------------------------------- bit reader
struct BitIn {
  const uint8_t* p = nullptr;
  size_t n = 0, at = 0;
  uint32_t buf = 0;
  int cnt = 0;
  bool bad = false;

  int get(int need) {                       // LSB-first, as DEFLATE stores them
    while (cnt < need) {
      if (at >= n) { bad = true; return 0; }
      buf |= uint32_t(p[at++]) << cnt;
      cnt += 8;
    }
    int v = int(buf & ((1u << need) - 1));
    buf >>= need;
    cnt -= need;
    return v;
  }
  void align() { buf = 0; cnt = 0; }        // drop to the next byte boundary
};

// -------------------------------------------------------- canonical huffman
// Held as "how many codes of each length" plus the symbols in canonical order,
// which is all the decoder below needs and cannot be built into an inconsistent
// state the way an explicit tree can.
struct Huff {
  uint16_t count[16] = {0};
  std::vector<uint16_t> symbol;

  // Returns false only for an OVER-subscribed code (more codes than the length
  // permits), which is corrupt.  An under-subscribed one is legal in exactly one
  // place -- a block with a single distance code -- and decode() rejects the
  // codes that were never assigned, so it is left to the decoder.
  bool build(const uint8_t* lengths, int n) {
    for (int i = 0; i < 16; i++) count[i] = 0;
    for (int i = 0; i < n; i++) count[lengths[i]]++;
    count[0] = 0;
    int left = 1;
    for (int len = 1; len < 16; len++) {
      left <<= 1;
      left -= count[len];
      if (left < 0) return false;
    }
    uint16_t offs[16];
    offs[0] = offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = uint16_t(offs[len] + count[len]);
    symbol.assign(size_t(n), 0);
    for (int i = 0; i < n; i++) if (lengths[i]) symbol[offs[lengths[i]]++] = uint16_t(i);
    return true;
  }

  int decode(BitIn& b) const {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
      code |= b.get(1);
      if (b.bad) return -1;
      int cnt = count[len];
      if (code - first < cnt) return symbol[size_t(index + (code - first))];
      index += cnt;
      first = (first + cnt) << 1;
      code <<= 1;
    }
    return -1;
  }
};

// ------------------------------------------------------------------ inflate
inline bool inflateRaw(const uint8_t* in, size_t n, size_t cap, std::string& out, std::string& err) {
  static const uint16_t lenBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
                                       67,83,99,115,131,163,195,227,258};
  static const uint8_t  lenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
  static const uint16_t distBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
                                        1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
  static const uint8_t  distExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,
                                         12,12,13,13};
  static const uint8_t  clOrder[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

  BitIn b;
  b.p = in; b.n = n;
  out.clear();

  Huff fixedLit, fixedDist;
  bool fixedReady = false;

  for (;;) {
    int last = b.get(1);
    int type = b.get(2);
    if (b.bad) { err = "the compressed stream ends in the middle of a block header"; return false; }

    if (type == 0) {                                  // stored
      b.align();
      if (b.at + 4 > b.n) { err = "a stored block has no length"; return false; }
      unsigned len  = unsigned(b.p[b.at]) | (unsigned(b.p[b.at + 1]) << 8);
      unsigned nlen = unsigned(b.p[b.at + 2]) | (unsigned(b.p[b.at + 3]) << 8);
      b.at += 4;
      if ((len ^ 0xFFFFu) != nlen) { err = "a stored block's length is self-inconsistent"; return false; }
      if (b.at + len > b.n) { err = "a stored block runs past the end of the data"; return false; }
      if (out.size() + len > cap) { err = "the archive expands past the size limit"; return false; }
      out.append((const char*)b.p + b.at, len);
      b.at += len;
    } else if (type == 1 || type == 2) {
      const Huff* lit;
      const Huff* dist;
      Huff dynLit, dynDist;
      if (type == 1) {
        if (!fixedReady) {
          uint8_t L[288], D[30];
          for (int i = 0;   i < 144; i++) L[i] = 8;
          for (int i = 144; i < 256; i++) L[i] = 9;
          for (int i = 256; i < 280; i++) L[i] = 7;
          for (int i = 280; i < 288; i++) L[i] = 8;
          for (int i = 0; i < 30; i++) D[i] = 5;
          fixedLit.build(L, 288);
          fixedDist.build(D, 30);
          fixedReady = true;
        }
        lit = &fixedLit; dist = &fixedDist;
      } else {
        int hlit  = b.get(5) + 257;
        int hdist = b.get(5) + 1;
        int hclen = b.get(4) + 4;
        if (b.bad) { err = "truncated dynamic block header"; return false; }
        if (hlit > 286 || hdist > 30) { err = "a dynamic block declares too many codes"; return false; }
        uint8_t cl[19] = {0};
        for (int i = 0; i < hclen; i++) cl[clOrder[i]] = uint8_t(b.get(3));
        Huff clh;
        if (!clh.build(cl, 19)) { err = "the code-length code is over-subscribed"; return false; }
        uint8_t lens[320] = {0};
        int i = 0;
        while (i < hlit + hdist) {
          int sym = clh.decode(b);
          if (sym < 0) { err = "a code length could not be decoded"; return false; }
          if (sym < 16) { lens[i++] = uint8_t(sym); continue; }
          int rep = 0;
          uint8_t val = 0;
          if (sym == 16) {
            if (i == 0) { err = "a length repeat with nothing to repeat"; return false; }
            val = lens[i - 1];
            rep = 3 + b.get(2);
          } else if (sym == 17) {
            rep = 3 + b.get(3);
          } else {
            rep = 11 + b.get(7);
          }
          if (i + rep > hlit + hdist) { err = "a length repeat overruns the code table"; return false; }
          while (rep--) lens[i++] = val;
        }
        if (b.bad) { err = "truncated code-length table"; return false; }
        if (!dynLit.build(lens, hlit)) { err = "the literal code is over-subscribed"; return false; }
        if (!dynDist.build(lens + hlit, hdist)) { err = "the distance code is over-subscribed"; return false; }
        lit = &dynLit; dist = &dynDist;
      }

      for (;;) {
        int sym = lit->decode(b);
        if (sym < 0) { err = "a symbol could not be decoded"; return false; }
        if (sym < 256) {
          if (out.size() + 1 > cap) { err = "the archive expands past the size limit"; return false; }
          out.push_back(char(uint8_t(sym)));
          continue;
        }
        if (sym == 256) break;
        sym -= 257;
        if (sym >= 29) { err = "a length symbol is out of range"; return false; }
        unsigned len = lenBase[sym] + unsigned(b.get(lenExtra[sym]));
        int dsym = dist->decode(b);
        if (dsym < 0 || dsym >= 30) { err = "a distance symbol is out of range"; return false; }
        unsigned d = distBase[dsym] + unsigned(b.get(distExtra[dsym]));
        if (b.bad) { err = "the compressed stream ends inside a match"; return false; }
        if (d > out.size()) { err = "a match points before the start of the data"; return false; }
        if (out.size() + len > cap) { err = "the archive expands past the size limit"; return false; }
        size_t from = out.size() - d;
        // Byte at a time deliberately: a match may overlap its own output, which
        // is how DEFLATE spells a run, and append(out, ...) would read a buffer
        // that reallocates underneath it.
        for (unsigned k = 0; k < len; k++) out.push_back(out[from + k]);
      }
    } else {
      err = "reserved block type";
      return false;
    }
    if (last) break;
  }
  return true;
}

// ---------------------------------------------------------------- container
struct Entry {
  std::string name;
  uint16_t method = 0;
  uint32_t crc = 0;
  uint64_t csize = 0, usize = 0, localOff = 0;
  bool symlink = false;
  bool dir = false;
  // The unix mode, when the archive carries one.  Only the execute bits are
  // ever acted on, and only to grant them: a package whose entry point is a
  // shell script or a compiled binary has to be runnable, and everything else
  // about the mode is the extracting side's business.
  bool executable = false;
};

inline uint16_t rd16(const std::string& s, size_t at) {
  return uint16_t(uint8_t(s[at]) | (uint8_t(s[at + 1]) << 8));
}
inline uint32_t rd32(const std::string& s, size_t at) {
  return uint32_t(uint8_t(s[at])) | (uint32_t(uint8_t(s[at + 1])) << 8) |
         (uint32_t(uint8_t(s[at + 2])) << 16) | (uint32_t(uint8_t(s[at + 3])) << 24);
}

// A member name is safe when it names a file inside the destination and nothing
// else.  Rejecting rather than sanitising: a package whose paths need repair is
// a package whose author should hear about it.
inline bool safeName(const std::string& n, std::string& why) {
  if (n.empty()) { why = "an entry has an empty name"; return false; }
  if (n.size() > 400) { why = "an entry name is absurdly long"; return false; }
  if (n[0] == '/' || (n.size() > 1 && n[1] == ':')) { why = "an entry has an absolute path: " + n; return false; }
  if (n.find('\\') != std::string::npos) { why = "an entry name contains a backslash: " + n; return false; }
  for (unsigned char c : n) if (c < 0x20 || c == 0x7f) { why = "an entry name contains a control character"; return false; }
  size_t at = 0;
  while (at <= n.size()) {
    size_t slash = n.find('/', at);
    std::string comp = n.substr(at, (slash == std::string::npos ? n.size() : slash) - at);
    if (comp == "..") { why = "an entry escapes the package directory: " + n; return false; }
    if (slash == std::string::npos) break;
    at = slash + 1;
  }
  return true;
}

// Entries the zip tools of the world add on their own and that no package
// means to ship.  Dropped rather than rejected, or every archive made by
// dragging a folder to "Compress" on a Mac would be refused.
inline bool junkName(const std::string& n) {
  if (n.rfind("__MACOSX/", 0) == 0) return true;
  size_t slash = n.find_last_of('/');
  std::string base = slash == std::string::npos ? n : n.substr(slash + 1);
  return base == ".DS_Store" || base == "Thumbs.db" || base.rfind("._", 0) == 0;
}

// Walks the central directory.  Reading the directory rather than scanning for
// local headers is what makes the entry list authoritative: a crafted archive
// can put anything between the local headers, but only what the directory
// names is ever extracted.
inline bool list(const std::string& z, std::vector<Entry>& out, std::string& err) {
  out.clear();
  if (z.size() < 22) { err = "that is too small to be a zip file"; return false; }
  // The end-of-central-directory record is last, but a trailing comment may
  // follow it, so it is found by scanning backwards over the comment's range.
  size_t eocd = std::string::npos;
  size_t lowest = z.size() > 66000 ? z.size() - 66000 : 0;
  for (size_t i = z.size() - 22 + 1; i-- > lowest;) {
    if (z[i] == 'P' && z[i + 1] == 'K' && uint8_t(z[i + 2]) == 5 && uint8_t(z[i + 3]) == 6) { eocd = i; break; }
  }
  if (eocd == std::string::npos) {
    err = "this file has no zip directory in it -- is it really a .zip?";
    return false;
  }
  uint32_t nEnt = rd16(z, eocd + 10);
  uint32_t cdSize = rd32(z, eocd + 12);
  uint32_t cdOff  = rd32(z, eocd + 16);
  if (cdOff == 0xFFFFFFFFu || cdSize == 0xFFFFFFFFu || rd16(z, eocd + 10) == 0xFFFF) {
    err = "ZIP64 archives are not supported; re-zip without the ZIP64 extension";
    return false;
  }
  if (size_t(cdOff) + cdSize > z.size()) { err = "the zip directory points outside the file"; return false; }

  size_t at = cdOff;
  for (uint32_t i = 0; i < nEnt; i++) {
    if (at + 46 > z.size()) { err = "the zip directory is truncated"; return false; }
    if (!(z[at] == 'P' && z[at + 1] == 'K' && uint8_t(z[at + 2]) == 1 && uint8_t(z[at + 3]) == 2)) {
      err = "the zip directory is malformed";
      return false;
    }
    uint16_t flags  = rd16(z, at + 8);
    uint16_t method = rd16(z, at + 10);
    uint32_t crc    = rd32(z, at + 16);
    uint32_t csize  = rd32(z, at + 20);
    uint32_t usize  = rd32(z, at + 24);
    uint16_t nlen   = rd16(z, at + 28);
    uint16_t elen   = rd16(z, at + 30);
    uint16_t clen   = rd16(z, at + 32);
    uint16_t madeBy = rd16(z, at + 4);
    uint32_t attrs  = rd32(z, at + 38);
    uint32_t lOff   = rd32(z, at + 42);
    if (at + 46 + nlen + elen + clen > z.size()) { err = "the zip directory is truncated"; return false; }
    Entry e;
    e.name = z.substr(at + 46, nlen);
    e.method = method;
    e.crc = crc;
    e.csize = csize;
    e.usize = usize;
    e.localOff = lOff;
    e.dir = !e.name.empty() && e.name.back() == '/';
    // Unix permissions live in the high half of the external attributes, and
    // only when the archive says it was made on a unix-like system.
    if ((madeBy >> 8) == 3) {
      const uint32_t mode = (attrs >> 16) & 0xFFFFu;
      e.symlink = (mode & 0xF000u) == 0xA000u;
      e.executable = (mode & 0111u) != 0;
    }
    if (flags & 0x0001) { err = "the archive is encrypted; upload an unencrypted zip"; return false; }
    at += 46u + nlen + elen + clen;
    if (junkName(e.name)) continue;
    out.push_back(e);
  }
  return true;
}

inline bool extract(const std::string& z, const Entry& e, size_t cap, std::string& out, std::string& err) {
  out.clear();
  if (e.dir) return true;
  size_t at = size_t(e.localOff);
  if (at + 30 > z.size()) { err = "entry " + e.name + " points outside the file"; return false; }
  if (!(z[at] == 'P' && z[at + 1] == 'K' && uint8_t(z[at + 2]) == 3 && uint8_t(z[at + 3]) == 4)) {
    err = "entry " + e.name + " has no local header where the directory says";
    return false;
  }
  uint16_t nlen = rd16(z, at + 26), elen = rd16(z, at + 28);
  size_t data = at + 30 + nlen + elen;
  if (data + e.csize > z.size()) { err = "entry " + e.name + " is truncated"; return false; }
  if (e.usize > cap) { err = "entry " + e.name + " is larger than the size limit"; return false; }
  if (e.method == 0) {
    out.assign(z, data, size_t(e.csize));
  } else if (e.method == 8) {
    // The ceiling is the entry's own declared size, not the caller's budget.
    // Passing `cap` here made the declared size something that was only audited
    // afterwards: an entry declaring nine bytes whose stream inflates to 256 MB
    // of zeros passed every pre-extraction budget (they are all computed from
    // the archive's own numbers) and then ballooned the process before the
    // size-mismatch check below rejected it.
    const size_t ceiling = std::min(cap, size_t(e.usize));
    if (!inflateRaw((const uint8_t*)z.data() + data, size_t(e.csize), ceiling, out, err)) {
      err = "entry " + e.name + ": " + err;
      return false;
    }
  } else {
    err = "entry " + e.name + " uses compression method " + std::to_string(e.method) +
          "; only store and deflate are supported";
    return false;
  }
  if (out.size() != e.usize) {
    err = "entry " + e.name + " decompressed to the wrong size";
    return false;
  }
  if (crc32of(out.data(), out.size()) != e.crc) {
    err = "entry " + e.name + " failed its checksum -- the upload is damaged";
    return false;
  }
  return true;
}

}  // namespace zipf
}  // namespace fish
