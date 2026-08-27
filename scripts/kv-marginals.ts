/** Dump high-sample ownership marginals from the ported particle sampler. */
import { KVParticleBelief, KVObservation, KVRuleset } from "../lib/kv-search-agent.ts";
import { CARDS, HALF_SUITS } from "../lib/fish-engine.ts";
import { readFileSync, writeFileSync } from "node:fs";

const rules: KVRuleset = {
  deckSize: 54, numPlayers: 6,
  halfSuitOf: Int8Array.from(CARDS.map(card => card.set)),
  halfSuitCards: HALF_SUITS.map(half => [...half.cards]),
};
const records = JSON.parse(readFileSync(process.argv[2], "utf8")) as { observation: KVObservation }[];
const count = Number(process.argv[4] ?? 2000);
const picks = [0, 40, 90, 150, 220, 300].filter(index => index < records.length);
const out = picks.map(index => {
  const observation = records[index].observation;
  const belief = new KVParticleBelief(rules, count, 12345);
  belief.update(observation);
  const marginals: number[][] = [];
  for (let card = 0; card < 54; card++) {
    marginals.push(Array.from({ length: 6 }, (_, owner) => belief.probability(card, owner)));
  }
  return { index, particles: belief.particles.length, marginals };
});
writeFileSync(process.argv[3], JSON.stringify(out));
console.log(`marginals for ${out.length} observations at ${count} particles -> ${process.argv[3]}`);
