import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import {
  BatchResult,
  DEFAULT_FISHBOT_CONFIG,
  FishBotConfig,
  runBatch,
  STRATEGIES,
  StrategyId,
} from "../lib/fish-engine.ts";

const numberFlag = (name: string, fallback: number) => {
  const raw = process.argv.find(arg => arg.startsWith(`--${name}=`))?.split("=")[1];
  const value = Number(raw);
  return Number.isFinite(value) && value > 0 ? Math.floor(value) : fallback;
};

const games = numberFlag("games", 1_000);
const ablationGames = numberFlag("ablation-games", 500);
const matrixGames = numberFlag("matrix-games", 250);
const output = resolve(process.argv.find(arg => arg.startsWith("--output="))?.split("=")[1] ?? "research/results/final-evaluation.json");
const testSeed = 0x7f4a7c15;
const ablationSeed = 0x6d2b79f5;
const matrixSeed = 0x4c957f2d;

function wilson(successes: number, total: number): [number, number] {
  const z = 1.96;
  const p = successes / total;
  const denominator = 1 + z * z / total;
  const center = (p + z * z / (2 * total)) / denominator;
  const margin = z * Math.sqrt((p * (1 - p) + z * z / (4 * total)) / total) / denominator;
  return [center - margin, center + margin];
}

function policyMetric(a: BatchResult, b: BatchResult, field: keyof Pick<BatchResult,
  "teamAskAccuracy" | "teamDeclarationAccuracy" | "avgInformationGain" | "avgDecisionRegret" |
  "teamReactionAccuracy" | "teamDiversionRate" | "avgReplyThreatOnMiss" | "avgContinuationValue" | "avgCompletionValue"
>) {
  const first = a[field] as [number, number];
  const second = b[field] as [number, number];
  return (first[0] + second[1]) / 2;
}

function balancedMatch(
  opponent: StrategyId,
  perOrientation: number,
  seed: number,
  config: FishBotConfig = DEFAULT_FISHBOT_CONFIG,
  psychologicalTells = true,
) {
  const common = { psychologicalTells, declarations: true, maxActions: 360, seed, fishbotConfig: config };
  const asA = runBatch(perOrientation, { ...common, strategies: ["fishbot", opponent] });
  const asB = runBatch(perOrientation, { ...common, strategies: [opponent, "fishbot"] });
  const wins = asA.teamAWins + (perOrientation - asB.teamAWins);
  return {
    opponent,
    games: perOrientation * 2,
    wins,
    winRate: wins / (perOrientation * 2),
    winRateCI: wilson(wins, perOrientation * 2),
    orientationWinRates: { asTeamA: asA.winRateA, asTeamB: 1 - asB.winRateA },
    avgScore: (asA.avgScore[0] + asB.avgScore[1]) / 2,
    askAccuracy: policyMetric(asA, asB, "teamAskAccuracy"),
    declarationAccuracy: policyMetric(asA, asB, "teamDeclarationAccuracy"),
    informationPerAsk: policyMetric(asA, asB, "avgInformationGain"),
    decisionRegret: policyMetric(asA, asB, "avgDecisionRegret"),
    reactionAccuracy: policyMetric(asA, asB, "teamReactionAccuracy"),
    diversionRate: policyMetric(asA, asB, "teamDiversionRate"),
    replyThreatOnMiss: policyMetric(asA, asB, "avgReplyThreatOnMiss"),
    continuationValue: policyMetric(asA, asB, "avgContinuationValue"),
    completionValue: policyMetric(asA, asB, "avgCompletionValue"),
    avgActions: (asA.avgActions + asB.avgActions) / 2,
    medianActions: (asA.medianActions + asB.medianActions) / 2,
    p90Actions: (asA.p90Actions + asB.p90Actions) / 2,
  };
}

const opponents = ["lockout", "detective", "fishbot_v02", "diversifier", "hunter", "bluffer", "random"] as StrategyId[];
console.log(`Held-out evaluation: ${games} games per orientation against ${opponents.length} opponents`);
const headToHead = opponents.map((opponent, index) => {
  const result = balancedMatch(opponent, games, testSeed);
  console.log(`[${index + 1}/${opponents.length}] ${STRATEGIES[opponent].short.padEnd(10)} ${(result.winRate * 100).toFixed(2)}% [${(result.winRateCI[0] * 100).toFixed(2)}, ${(result.winRateCI[1] * 100).toFixed(2)}]`);
  return result;
});

const ablations: { name: string; config: FishBotConfig }[] = [
  { name: "full", config: { ...DEFAULT_FISHBOT_CONFIG } },
  { name: "no-count-conditioning", config: { ...DEFAULT_FISHBOT_CONFIG, useCountConditioning: false } },
  { name: "no-ask-history", config: { ...DEFAULT_FISHBOT_CONFIG, signalStrength: 0 } },
  { name: "no-continuation", config: { ...DEFAULT_FISHBOT_CONFIG, continuationWeight: 0 } },
  { name: "no-team-control", config: { ...DEFAULT_FISHBOT_CONFIG, teamControlWeight: 0 } },
  { name: "no-completion", config: { ...DEFAULT_FISHBOT_CONFIG, completionWeight: 0 } },
  { name: "no-reply-risk", config: { ...DEFAULT_FISHBOT_CONFIG, replyThreatWeight: 0 } },
  { name: "entropy-premium", config: { ...DEFAULT_FISHBOT_CONFIG, informationWeight: 4.5 } },
  {
    name: "immediate-transfer-only",
    config: {
      ...DEFAULT_FISHBOT_CONFIG,
      informationWeight: 0, setProgressWeight: 0, teamControlWeight: 0,
      targetEvidenceWeight: 0, continuationWeight: 0, completionWeight: 0,
      replyThreatWeight: 0, repeatSetWeight: 0,
    },
  },
];

console.log(`\nAblations: ${ablations.length} variants against detective and lockout`);
const ablationResults = ablations.map((ablation, index) => {
  const matches = (["detective", "lockout"] as StrategyId[]).map(opponent => balancedMatch(opponent, ablationGames, ablationSeed, ablation.config));
  const meanWinRate = matches.reduce((sum, match) => sum + match.winRate, 0) / matches.length;
  console.log(`[${index + 1}/${ablations.length}] ${ablation.name.padEnd(25)} ${(meanWinRate * 100).toFixed(2)}%`);
  return { ...ablation, meanWinRate, matches };
});

console.log(`\nAsk-response behavior ablation against the strongest challengers`);
const askResponseAblation = (["detective", "lockout", "bluffer"] as StrategyId[]).map(opponent => ({
  opponent,
  enabled: balancedMatch(opponent, ablationGames, ablationSeed ^ 0x11111111, DEFAULT_FISHBOT_CONFIG, true),
  disabled: balancedMatch(opponent, ablationGames, ablationSeed ^ 0x11111111, DEFAULT_FISHBOT_CONFIG, false),
}));

const matrixStrategies = Object.keys(STRATEGIES) as StrategyId[];
console.log(`\nOrdered ${matrixStrategies.length}×${matrixStrategies.length} policy matrix: ${matrixGames} games/cell`);
const matrix: Record<string, number> = {};
for (const row of matrixStrategies) {
  for (const column of matrixStrategies) {
    matrix[`${row}:${column}`] = runBatch(matrixGames, {
      strategies: [row, column], psychologicalTells: true, declarations: true,
      maxActions: 360, seed: matrixSeed,
    }).winRateA;
  }
  console.log(`${STRATEGIES[row].short.padEnd(10)} ${matrixStrategies.map(column => `${(matrix[`${row}:${column}`] * 100).toFixed(1)}%`).join(" ")}`);
}

const artifact = {
  generatedAt: new Date().toISOString(),
  scope: "54-card, nine-half-suit, six-player Fish; wrong declarations award the set to the opponent",
  selectedConfig: DEFAULT_FISHBOT_CONFIG,
  seeds: { testSeed, ablationSeed, matrixSeed },
  games: { headToHeadPerOrientation: games, ablationPerOrientation: ablationGames, matrixPerCell: matrixGames },
  headToHead,
  ablations: ablationResults,
  askResponseAblation,
  matrix: { strategies: matrixStrategies, cells: matrix },
};
mkdirSync(dirname(output), { recursive: true });
writeFileSync(output, `${JSON.stringify(artifact, null, 2)}\n`);
console.log(`\nWrote ${output}`);

