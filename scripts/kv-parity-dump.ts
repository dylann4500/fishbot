/**
 * Dump KV-agent decisions from real FishLab games so `kv_parity_ref.py` can
 * recompute them with the original Python implementation.
 */
import { deriveGameSeed, setKvTraceSink, simulateGame } from "../lib/fish-engine.ts";
import { writeFileSync } from "node:fs";

const out = process.argv[2] ?? "/tmp/kv-parity.json";
const wanted = Number(process.argv[3] ?? 40);
const traces: unknown[] = [];
setKvTraceSink(trace => { if (traces.length < wanted) traces.push(trace); });
for (let i = 0; traces.length < wanted && i < 40; i++) {
  simulateGame({ seed: deriveGameSeed(20260820, i), strategies: ["kv_search", "fishbot"], psychologicalTells: true, declarations: true });
}
setKvTraceSink(null);
writeFileSync(out, JSON.stringify(traces));
console.log(`wrote ${traces.length} decisions to ${out}`);
