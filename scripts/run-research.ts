import { runBatch, STRATEGIES, StrategyId } from "../lib/fish-engine.ts";

const strategies = Object.keys(STRATEGIES) as StrategyId[];
const gamesFlag = process.argv.find(arg => arg.startsWith("--games="));
const games = Math.max(100, Number(gamesFlag?.split("=")[1] ?? 1000));
const cells: Record<string, number> = {};

console.log(`FishLab pairwise study · ${games.toLocaleString()} games per matchup`);
console.log("Rows play as Team A; columns play as Team B. Cells are Team A win rates.\n");

for (const a of strategies) {
  for (const b of strategies) {
    const result = runBatch(games, {
      strategies: [a, b],
      psychologicalTells: true,
      declarations: true,
      maxActions: 360,
      seed: 20260820,
    });
    cells[`${a}:${b}`] = result.winRateA;
  }
}

const label = (id: StrategyId) => STRATEGIES[id].short.padEnd(9);
console.log(`${"A \\ B".padEnd(10)}${strategies.map(label).join(" ")}`);
for (const a of strategies) {
  console.log(`${label(a)} ${strategies.map(b => `${(cells[`${a}:${b}`] * 100).toFixed(1)}%`.padEnd(9)).join(" ")}`);
}

console.log("\nAverage win rate by row strategy:");
for (const a of strategies) {
  const average = strategies.reduce((sum, b) => sum + cells[`${a}:${b}`], 0) / strategies.length;
  console.log(`  ${STRATEGIES[a].name.padEnd(24)} ${(average * 100).toFixed(1)}%`);
}
