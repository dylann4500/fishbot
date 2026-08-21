import { readFileSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { deriveGameSeed, FishBotConfig, simulateGame, StrategyId } from "../lib/fish-engine.ts";

type EvaluationArtifact = {
  ablations: { name: string; config: FishBotConfig }[];
};

const source = resolve(process.argv.find(arg => arg.startsWith("--source="))?.split("=")[1] ?? "research/results/final-evaluation.json");
const output = resolve(process.argv.find(arg => arg.startsWith("--output="))?.split("=")[1] ?? "research/results/paired-ablations.json");
const games = Math.max(100, Number(process.argv.find(arg => arg.startsWith("--games="))?.split("=")[1] ?? 500));
const baseSeed = 0x6d2b79f5;
const opponents = ["detective", "lockout"] as StrategyId[];
const artifact = JSON.parse(readFileSync(source, "utf8")) as EvaluationArtifact;
const variants = artifact.ablations;
const full = variants.find(variant => variant.name === "full")!;

function outcome(config: FishBotConfig, opponent: StrategyId, orientation: "A" | "B", seed: number) {
  const strategies: [StrategyId, StrategyId] = orientation === "A" ? ["fishbot", opponent] : [opponent, "fishbot"];
  const game = simulateGame({
    seed, strategies, psychologicalTells: true, declarations: true,
    maxActions: 360, fishbotConfig: config,
  });
  return game.winner === (orientation === "A" ? 0 : 1) ? 1 : 0;
}

const strata = opponents.flatMap(opponent => (["A", "B"] as const).map(orientation => ({ opponent, orientation })));
console.log(`Paired ablations: ${variants.length} variants × ${strata.length} strata × ${games} matched games`);
const fullOutcomes = new Map<string, number[]>();
for (const { opponent, orientation } of strata) {
  const key = `${opponent}:${orientation}`;
  fullOutcomes.set(key, Array.from({ length: games }, (_, index) => outcome(full.config, opponent, orientation, deriveGameSeed(baseSeed, index))));
}

const results = variants.map((variant, variantIndex) => {
  const differences: number[] = [];
  const byOpponent: Record<string, number[]> = {};
  for (const { opponent, orientation } of strata) {
    const key = `${opponent}:${orientation}`;
    const baseline = fullOutcomes.get(key)!;
    for (let index = 0; index < games; index++) {
      const ablated = variant.name === "full"
        ? baseline[index]
        : outcome(variant.config, opponent, orientation, deriveGameSeed(baseSeed, index));
      const difference = baseline[index] - ablated;
      differences.push(difference);
      (byOpponent[opponent] ??= []).push(difference);
    }
  }
  const mean = differences.reduce((sum, value) => sum + value, 0) / differences.length;
  const variance = differences.reduce((sum, value) => sum + (value - mean) ** 2, 0) / Math.max(1, differences.length - 1);
  const margin = 1.96 * Math.sqrt(variance / differences.length);
  const result = {
    name: variant.name,
    fullMinusAblatedWinRate: mean,
    confidenceInterval: [mean - margin, mean + margin],
    discordantGames: differences.filter(Boolean).length,
    byOpponent: Object.fromEntries(Object.entries(byOpponent).map(([opponent, values]) => [opponent, values.reduce((sum, value) => sum + value, 0) / values.length])),
  };
  console.log(`[${variantIndex + 1}/${variants.length}] ${variant.name.padEnd(25)} ${(mean * 100).toFixed(2)} pp [${((mean - margin) * 100).toFixed(2)}, ${((mean + margin) * 100).toFixed(2)}]`);
  return result;
});

writeFileSync(output, `${JSON.stringify({ generatedAt: new Date().toISOString(), source, baseSeed, gamesPerStratum: games, opponents, results }, null, 2)}\n`);
console.log(`Wrote ${output}`);

