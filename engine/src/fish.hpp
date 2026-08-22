// FishLab v0.4 — rules-exact core for six-player Canadian Fish (Literature).
//
// Rule source: the authoritative rules supplied by the project owner, which the
// v0.3 simulator only partially implemented. Differences that matter and are
// implemented here:
//   * declarations are legal AT ANY TIME, including during an opponent's turn;
//     the turn returns to whoever held it before the declaration;
//   * a player needs no cards of a set to declare it, and (by the literal
//     reading of "drops out ... i.e. cannot ask and cannot be asked") a
//     cardless player may still declare;  toggled by rules.cardlessMayDeclare;
//   * a cardless player whose turn it is *chooses* which live teammate receives
//     the turn (open, information-free negotiation);
//   * when a whole team is cardless the other team must declare every remaining
//     set with no further asking, choosing among willing declarers -- and may
//     misdeclare, handing sets to the cardless team.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <cmath>

namespace fish {

static constexpr int NCARD = 54;
static constexpr int NSET  = 9;
static constexpr int NPLAY = 6;
static constexpr int SETSZ = 6;

inline constexpr int setOf(int card) { return card / SETSZ; }
inline constexpr int idxIn(int card) { return card % SETSZ; }
inline constexpr int cardOf(int set, int i) { return set * SETSZ + i; }

inline constexpr uint64_t setMask(int s) { return uint64_t(0x3F) << (SETSZ * s); }
inline constexpr uint64_t bit(int c) { return uint64_t(1) << c; }

// Seats alternate; 0,2,4 = team 0, 1,3,5 = team 1.
inline constexpr int teamOf(int seat) { return seat & 1; }

inline int popcount64(uint64_t x) { return __builtin_popcountll(x); }

// ---------------------------------------------------------------- card names
// Ordering matches the v0.3 TypeScript engine exactly so that ported policies
// and cross-engine validation line up card-for-card.
//   set 0 Low Spades, 1 High Spades, 2 Low Hearts, 3 High Hearts,
//   set 4 Low Diamonds, 5 High Diamonds, 6 Low Clubs, 7 High Clubs,
//   set 8 Eights + Jokers  (8S,8H,8D,8C,RedJoker,BlackJoker)
inline const char* setName(int s) {
  static const char* n[9] = {"Low Spades","High Spades","Low Hearts","High Hearts",
                             "Low Diamonds","High Diamonds","Low Clubs","High Clubs",
                             "Eights & Jokers"};
  return n[s];
}
inline std::string cardName(int c) {
  static const char* lowR[6] = {"2","3","4","5","6","7"};
  static const char* hiR[6]  = {"9","10","J","Q","K","A"};
  static const char* suit[4] = {"S","H","D","C"};
  int s = setOf(c), i = idxIn(c);
  if (s == 8) { static const char* e[6] = {"8S","8H","8D","8C","RJ","BJ"}; return e[i]; }
  return std::string(s % 2 == 0 ? lowR[i] : hiR[i]) + suit[s / 2];
}

// ------------------------------------------------------------------- RNG
struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
  inline uint64_t next() {                       // splitmix64
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  inline uint32_t u32(uint32_t n) { return uint32_t((next() >> 32) * uint64_t(n) >> 32); }
  inline double uni() { return double(next() >> 11) * 0x1.0p-53; }
};
inline uint64_t mixSeed(uint64_t a, uint64_t b) {
  uint64_t z = a * 0x9E3779B97F4A7C15ull + b + 0x165667B19E3779F9ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// ------------------------------------------------------------------ actions
enum class Kind : uint8_t { Ask, Declare, Pass, ForcedDeclare, End };

struct Declaration {
  uint8_t set = 0;
  uint8_t owner[SETSZ] = {0,0,0,0,0,0};   // seat claimed to hold card set*6+i
};

struct Event {
  Kind kind;
  uint8_t actor = 0;
  uint8_t target = 0;       // ask target / pass receiver
  uint8_t card = 0;
  uint8_t set = 0;
  bool success = false;     // ask hit / declaration correct
  Declaration decl;
  uint8_t handCount[NPLAY] = {0,0,0,0,0,0};   // public counts AFTER the event
  double confidence = 0;    // declarer's own stated confidence (diagnostic only)
};

// ------------------------------------------------------------------- rules
struct Rules {
  bool outOfTurnDeclare   = true;   // real rule: declare at any moment
  bool cardlessMayDeclare = true;   // literal reading of "drops out"
  int  maxAsks            = 400;    // safety valve; incidence is reported
  bool adjudicateOnLimit  = true;   // resolve leftovers by majority if hit
  int  deckSets           = 9;      // 9 = 54-card Canadian; 8 = 48-card Literature
  // Arbitration between simultaneous voluntary declarations is a modelling
  // choice, not a rule: the rules say a player may declare at any moment but do
  // not say who wins a tie.  0 = lowest seat, 1 = highest seat, 2 = scan from the
  // current turn-holder.  Ranking by stated confidence is deliberately absent:
  // it would compare private confidences across seats, which is the leak the
  // forced endgame was corrected for.
  int  declArbitration    = 0;
  // Forced-endgame willingness ladder.  Only a willingness bit may be shared
  // with teammates, so the team sweeps thresholds; a negative entry means
  // "somebody must declare, use your best guess".
  // A finer ladder recovers most of "most confident declares first" while still
  // exchanging nothing but willingness bits, and it lets the earlier, safer
  // declarations reveal allocations that sharpen the later ones.
  int    nForcedTh = 8;
  double forcedTh[8] = {0.995, 0.98, 0.95, 0.90, 0.80, 0.65, 0.50, -1.0};
};

struct PublicState {
  Rules rules;
  uint8_t handCount[NPLAY];
  bool setActive[NSET];
  uint8_t score[2];
  int nAsks = 0;
  int nEvents = 0;
  int turn = 0;
  std::vector<Event> history;
  inline int activeSets() const { int n = 0; for (int s = 0; s < NSET; s++) n += setActive[s]; return n; }
  inline bool teamAlive(int t) const {
    for (int p = t; p < NPLAY; p += 2) if (handCount[p]) return true;
    return false;
  }
};

struct GameState {
  uint64_t hand[NPLAY];
  uint64_t dealt[NPLAY];     // the initial deal (ground truth, never shown to a policy)
  PublicState pub;
  int turn = 0;
  int dealer = 0;
  bool finished = false;
  uint8_t setWinner[NSET];   // 2 while active
};

// A legal ask: actor holds another card of the set, does not hold the card,
// the target is an opponent, and the target still has cards.
inline bool legalAsk(const GameState& g, int actor, int card, int target) {
  int s = setOf(card);
  if (!g.pub.setActive[s]) return false;
  if (teamOf(target) == teamOf(actor)) return false;
  if (!g.pub.handCount[target]) return false;
  if (g.hand[actor] & bit(card)) return false;
  return (g.hand[actor] & setMask(s)) != 0;
}

// The card-side legality test as an observer sees it (no hidden information).
inline bool legalAskShape(const PublicState& pub, uint64_t myHand, int actor, int card, int target) {
  int s = setOf(card);
  if (!pub.setActive[s]) return false;
  if (teamOf(target) == teamOf(actor)) return false;
  if (!pub.handCount[target]) return false;
  if (myHand & bit(card)) return false;
  return (myHand & setMask(s)) != 0;
}

struct AskMove { uint8_t card, target; };

inline int enumerateAsks(const PublicState& pub, uint64_t myHand, int actor, AskMove* out) {
  int n = 0;
  for (int s = 0; s < NSET; s++) {
    if (!pub.setActive[s]) continue;
    uint64_t mine = myHand & setMask(s);
    if (!mine) continue;
    for (int i = 0; i < SETSZ; i++) {
      int c = cardOf(s, i);
      if (myHand & bit(c)) continue;
      for (int t = 0; t < NPLAY; t++) {
        if (teamOf(t) == teamOf(actor)) continue;
        if (!pub.handCount[t]) continue;
        out[n++] = AskMove{uint8_t(c), uint8_t(t)};
      }
    }
  }
  return n;
}

inline void dealCards(GameState& g, uint64_t seed, int deckSets) {
  Rng rng(mixSeed(seed, 0xF15Aull));
  int n = deckSets * SETSZ;
  std::array<int, NCARD> deck{};
  for (int i = 0; i < n; i++) deck[i] = i;
  for (int i = n - 1; i > 0; i--) { int j = int(rng.u32(uint32_t(i + 1))); std::swap(deck[i], deck[j]); }
  for (int p = 0; p < NPLAY; p++) g.hand[p] = 0;
  int per = n / NPLAY;
  for (int i = 0; i < n; i++) g.hand[i / per] |= bit(deck[i]);
  for (int p = 0; p < NPLAY; p++) g.dealt[p] = g.hand[p];
  g.dealer = int(rng.u32(NPLAY));
  g.turn = (g.dealer + 1) % NPLAY;
}

} // namespace fish
