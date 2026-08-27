// Test-only: drive the ported KV agent through observations captured elsewhere,
// so its scoring layers can be compared against the Python original.
//
// The TypeScript port in lib/kv-search-agent.ts is checked decision by decision
// against fish/agents.py by scripts/kv_parity_ref.py.  This probe puts the C++
// port on the same observations with the same particle set, so a transcription
// error between the two ports cannot hide.  Nothing here is reachable from a
// measured path: it is a separate subcommand that constructs no game.
#pragma once
#include "kv.hpp"
#include <cstdio>
#include <cstring>

namespace fish {
namespace kvsearch {

inline int parityMain(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) { fprintf(stderr, "kvparity: cannot open %s\n", path); return 2; }
  char tag[32];
  int decisions = 0;
  while (fscanf(f, "%31s", tag) == 1) {
    if (strcmp(tag, "DEC")) { fprintf(stderr, "kvparity: expected DEC, got %s\n", tag); return 2; }
    int player = 0, ply = 0, deckSets = 9;
    if (fscanf(f, "%d %d %d", &player, &ply, &deckSets) != 3) return 2;

    KVAgent agent;
    PublicState pub;
    pub.turn = player;
    pub.nEvents = ply;
    for (int s = 0; s < NSET; s++) pub.setActive[s] = (s < deckSets);
    pub.score[0] = pub.score[1] = 0;

    uint64_t hand = 0;
    int n = 0;
    if (fscanf(f, "%31s %d", tag, &n) != 2) return 2;          // HAND
    for (int i = 0; i < n; i++) { int c; if (fscanf(f, "%d", &c) != 1) return 2; hand |= bit(c); }
    if (fscanf(f, "%31s", tag) != 1) return 2;                 // COUNTS
    for (int p = 0; p < NPLAY; p++) { int v; if (fscanf(f, "%d", &v) != 1) return 2; pub.handCount[p] = uint8_t(v); }
    if (fscanf(f, "%31s", tag) != 1) return 2;                 // RESOLVED
    for (int s = 0; s < NSET; s++) { int v; if (fscanf(f, "%d", &v) != 1) return 2; if (v) pub.setActive[s] = false; }
    if (fscanf(f, "%31s %d", tag, &n) != 2) return 2;          // HIST
    for (int i = 0; i < n; i++) {
      int actor, target, card, success;
      if (fscanf(f, "%31s %d %d %d %d", tag, &actor, &target, &card, &success) != 5) return 2;
      Event e; e.kind = Kind::Ask;
      e.actor = uint8_t(actor); e.target = uint8_t(target); e.card = uint8_t(card);
      e.set = uint8_t(setOf(card)); e.success = success != 0;
      pub.history.push_back(e);
    }

    Rules rules; rules.deckSets = deckSets;
    agent.reset(player, hand, rules, 12345);
    agent.deckSets = deckSets;

    // Inject the captured particle set in place of a fresh draw, so the
    // comparison isolates the scoring layers from the sampler.
    int np = 0;
    if (fscanf(f, "%31s %d", tag, &np) != 2) return 2;         // PART
    agent.bel.flat.assign(size_t(np) * NCARD, -1);
    for (int i = 0; i < np; i++) {
      if (fscanf(f, "%31s", tag) != 1) return 2;               // P
      for (int c = 0; c < NCARD; c++) {
        int owner; if (fscanf(f, "%d", &owner) != 1) return 2;
        agent.bel.flat[size_t(i) * NCARD + size_t(c)] = int8_t(owner);
      }
    }
    agent.bel.nParticles = np;

    if (fscanf(f, "%31s %d", tag, &n) != 2) return 2;          // ASKS
    printf("DEC %d\n", decisions);
    for (int i = 0; i < n; i++) {
      int card, target;
      if (fscanf(f, "%31s %d %d", tag, &card, &target) != 3) return 2;
      Action a; a.card = uint8_t(card); a.target = uint8_t(target);
      printf("SCORE %d %d %.17g %.17g\n", card, target,
             agent.basicScore(a, pub.handCount), agent.probScore(a, pub.handCount));
    }
    // The claim candidates the belief supports, and their scores.
    std::vector<Action> claims = agent.candidateActions(pub, true);
    for (const Action& a : claims) {
      if (!a.claim) continue;
      printf("CLAIM %d", int(a.set));
      for (int j = 0; j < SETSZ; j++) printf(" %d", int(a.owner[j]));
      printf(" %.17g %.17g\n", a.support, agent.probScore(a, pub.handCount));
    }
    if (fscanf(f, "%31s", tag) != 1 || strcmp(tag, "END")) return 2;
    decisions++;
  }
  fclose(f);
  fprintf(stderr, "kvparity: %d decisions\n", decisions);
  return 0;
}

}  // namespace kvsearch
}  // namespace fish
