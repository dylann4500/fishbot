import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { DEFAULT_FISHBOT_CONFIG, FishBotConfig, runBatch, StrategyId } from "../lib/fish-engine.ts";

const games = Math.max(50, Number(process.argv.find(arg => arg.startsWith("--games="))?.split("=")[1] ?? 250));
const output = resolve(process.argv.find(arg => arg.startsWith("--output="))?.split("=")[1] ?? "research/results/refinement.json");
const opponents: { id: StrategyId; weight: number }[] = [
  { id: "detective", weight: .4 },
  { id: "lockout", weight: .35 },
  { id: "fishbot_v02", weight: .25 },
];
const seedBanks = [0x10293847, 0x56473829];

const candidates: { name: string; config: FishBotConfig }[] = [
  { name: "selected-full", config: { ...DEFAULT_FISHBOT_CONFIG } },
  { name: "no-team-control", config: { ...DEFAULT_FISHBOT_CONFIG, teamControlWeight: 0 } },
  { name: "entropy-1.5", config: { ...DEFAULT_FISHBOT_CONFIG, informationWeight: 1.5 } },
  { name: "entropy-3", config: { ...DEFAULT_FISHBOT_CONFIG, informationWeight: 3 } },
  { name: "entropy-4.5", config: { ...DEFAULT_FISHBOT_CONFIG, informationWeight: 4.5 } },
  { name: "no-control-entropy-2", config: { ...DEFAULT_FISHBOT_CONFIG, teamControlWeight: 0, informationWeight: 2 } },
  { name: "no-control-entropy-4.5", config: { ...DEFAULT_FISHBOT_CONFIG, teamControlWeight: 0, informationWeight: 4.5 } },
  { name: "control-2-entropy-2", config: { ...DEFAULT_FISHBOT_CONFIG, teamControlWeight: 2, informationWeight: 2 } },
  { name: "no-continuation", config: { ...DEFAULT_FISHBOT_CONFIG, continuationWeight: 0 } },
  { name: "cautious-97", config: { ...DEFAULT_FISHBOT_CONFIG, declarationThreshold: .97 } },
];

function balancedWinRate(config: FishBotConfig, opponent: StrategyId, seed: number) {
  const common = { psychologicalTells: true, declarations: true, maxActions: 360, seed, fishbotConfig: config };
  const asA = runBatch(games, { ...common, strategies: ["fishbot", opponent] });
  const asB = runBatch(games, { ...common, strategies: [opponent, "fishbot"] });
  return (asA.winRateA + 1 - asB.winRateA) / 2;
}

console.log(`Focused refinement: ${candidates.length} candidates × ${opponents.length} opponents × ${seedBanks.length} banks × ${games} games/orientation`);
const results = candidates.map((candidate, index) => {
  const matches = opponents.map(opponent => {
    const bankWinRates = seedBanks.map(seed => balancedWinRate(candidate.config, opponent.id, seed));
    return { opponent: opponent.id, bankWinRates, winRate: bankWinRates.reduce((sum, value) => sum + value, 0) / bankWinRates.length };
  });
  const score = matches.reduce((sum, match) => sum + match.winRate * opponents.find(opponent => opponent.id === match.opponent)!.weight, 0);
  console.log(`[${index + 1}/${candidates.length}] ${candidate.name.padEnd(25)} ${(score * 100).toFixed(2)}% · ${matches.map(match => `${match.opponent} ${(match.winRate * 100).toFixed(1)}%`).join(" · ")}`);
  return { ...candidate, score, matches };
}).sort((a, b) => b.score - a.score);

const artifact = {
  generatedAt: new Date().toISOString(),
  gamesPerOrientationBank: games,
  seedBanks,
  opponents,
  selected: results[0],
  results,
};
mkdirSync(dirname(output), { recursive: true });
writeFileSync(output, `${JSON.stringify(artifact, null, 2)}\n`);
console.log(`Selected ${results[0].name}: ${(results[0].score * 100).toFixed(2)}%. Wrote ${output}`);

