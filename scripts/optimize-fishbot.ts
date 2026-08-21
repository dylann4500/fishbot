import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import {
  DEFAULT_FISHBOT_CONFIG,
  FishBotConfig,
  runBatch,
  StrategyId,
} from "../lib/fish-engine.ts";

type MatchResult = {
  opponent: StrategyId;
  winRate: number;
  avgScore: number;
  askAccuracy: number;
  declarationAccuracy: number;
  informationPerAsk: number;
  avgActions: number;
};

type CandidateResult = {
  name: string;
  config: FishBotConfig;
  score: number;
  weightedWinRate: number;
  worstWinRate: number;
  matches: MatchResult[];
};

const opponents: { id: StrategyId; weight: number }[] = [
  { id: "detective", weight: .35 },
  { id: "lockout", weight: .25 },
  { id: "fishbot_v02", weight: .20 },
  { id: "diversifier", weight: .12 },
  { id: "hunter", weight: .08 },
];

const readNumberFlag = (name: string, fallback: number) => {
  const flag = process.argv.find(arg => arg.startsWith(`--${name}=`));
  const value = Number(flag?.split("=")[1]);
  return Number.isFinite(value) && value > 0 ? Math.floor(value) : fallback;
};

const trainGames = readNumberFlag("train-games", 60);
const validationGames = readNumberFlag("validation-games", 350);
const randomCandidates = readNumberFlag("candidates", 20);
const finalists = readNumberFlag("finalists", 5);
const outputFlag = process.argv.find(arg => arg.startsWith("--output="))?.split("=")[1];
const outputPath = resolve(outputFlag ?? "research/results/optimization.json");

let randomState = 0x5f3759df;
const random = () => {
  let x = randomState;
  x ^= x << 13; x ^= x >>> 17; x ^= x << 5;
  randomState = x >>> 0;
  return randomState / 4294967296;
};
const range = (low: number, high: number) => low + random() * (high - low);
const rounded = (value: number, digits = 3) => Number(value.toFixed(digits));

const namedCandidates: { name: string; config: FishBotConfig }[] = [
  { name: "v03-default", config: { ...DEFAULT_FISHBOT_CONFIG } },
  {
    name: "posterior-greedy",
    config: {
      ...DEFAULT_FISHBOT_CONFIG,
      hitWeight: 28, informationWeight: 0, setProgressWeight: 1,
      teamControlWeight: 1, targetEvidenceWeight: 1.4,
      continuationWeight: 1, completionWeight: 2.5,
      replyThreatWeight: .5, repeatSetWeight: .2,
    },
  },
  {
    name: "control-and-chain",
    config: {
      ...DEFAULT_FISHBOT_CONFIG,
      hitWeight: 22, informationWeight: 0, setProgressWeight: 2.5,
      teamControlWeight: 4, targetEvidenceWeight: .5,
      continuationWeight: 4, completionWeight: 4,
      replyThreatWeight: 1, repeatSetWeight: .5,
    },
  },
  {
    name: "cautious-declarer",
    config: {
      ...DEFAULT_FISHBOT_CONFIG,
      declarationThreshold: .97, trailingDeclarationDelta: -.025,
      leadingDeclarationDelta: .015, allocationSlack: .01,
    },
  },
];

for (let index = 0; index < randomCandidates; index++) {
  namedCandidates.push({
    name: `sample-${String(index + 1).padStart(2, "0")}`,
    config: {
      useCountConditioning: true,
      signalStrength: rounded(range(.16, .62)),
      hitWeight: rounded(range(17, 31), 2),
      informationWeight: rounded(range(-1, 3.5), 2),
      setProgressWeight: rounded(range(0, 5), 2),
      teamControlWeight: rounded(range(0, 5), 2),
      targetEvidenceWeight: rounded(range(0, 2.2), 2),
      continuationWeight: rounded(range(0, 5), 2),
      completionWeight: rounded(range(0, 5), 2),
      replyThreatWeight: rounded(range(0, 4), 2),
      repeatSetWeight: rounded(range(-.75, 1.75), 2),
      declarationThreshold: rounded(range(.925, .975)),
      trailingDeclarationDelta: rounded(range(-.03, 0)),
      leadingDeclarationDelta: rounded(range(0, .035)),
      allocationSlack: rounded(range(.005, .06)),
    },
  });
}

function evaluate(name: string, config: FishBotConfig, games: number, seed: number): CandidateResult {
  const matches = opponents.map(({ id }) => {
    const asA = runBatch(games, {
      strategies: ["fishbot", id], psychologicalTells: true, declarations: true,
      maxActions: 360, seed, fishbotConfig: config,
    });
    const asB = runBatch(games, {
      strategies: [id, "fishbot"], psychologicalTells: true, declarations: true,
      maxActions: 360, seed, fishbotConfig: config,
    });
    return {
      opponent: id,
      winRate: (asA.winRateA + 1 - asB.winRateA) / 2,
      avgScore: (asA.avgScore[0] + asB.avgScore[1]) / 2,
      askAccuracy: (asA.teamAskAccuracy[0] + asB.teamAskAccuracy[1]) / 2,
      declarationAccuracy: (asA.teamDeclarationAccuracy[0] + asB.teamDeclarationAccuracy[1]) / 2,
      informationPerAsk: (asA.avgInformationGain[0] + asB.avgInformationGain[1]) / 2,
      avgActions: (asA.avgActions + asB.avgActions) / 2,
    };
  });
  const weightedWinRate = matches.reduce((sum, match) => {
    const weight = opponents.find(opponent => opponent.id === match.opponent)!.weight;
    return sum + match.winRate * weight;
  }, 0);
  const worstWinRate = Math.min(...matches.map(match => match.winRate));
  const robustnessPenalty = Math.max(0, .5 - worstWinRate) * .2;
  return { name, config, score: weightedWinRate - robustnessPenalty, weightedWinRate, worstWinRate, matches };
}

console.log(`FishBot optimizer: ${namedCandidates.length} candidates × ${trainGames} games/orientation/matchup`);
const training = namedCandidates.map((candidate, index) => {
  const result = evaluate(candidate.name, candidate.config, trainGames, 0x13579bdf);
  console.log(`[${index + 1}/${namedCandidates.length}] ${candidate.name.padEnd(20)} score ${(result.score * 100).toFixed(2)}% · detective ${(result.matches[0].winRate * 100).toFixed(1)}% · worst ${(result.worstWinRate * 100).toFixed(1)}%`);
  return result;
}).sort((a, b) => b.score - a.score);

console.log(`\nValidating top ${Math.min(finalists, training.length)} on an independent seed bank...`);
const validation = training.slice(0, finalists).map((candidate, index) => {
  const result = evaluate(candidate.name, candidate.config, validationGames, 0x2468ace0);
  console.log(`[${index + 1}/${Math.min(finalists, training.length)}] ${candidate.name.padEnd(20)} score ${(result.score * 100).toFixed(2)}% · detective ${(result.matches[0].winRate * 100).toFixed(1)}% · worst ${(result.worstWinRate * 100).toFixed(1)}%`);
  return result;
}).sort((a, b) => b.score - a.score);

const artifact = {
  generatedAt: new Date().toISOString(),
  methodology: {
    trainGamesPerOrientationMatchup: trainGames,
    validationGamesPerOrientationMatchup: validationGames,
    trainSeed: 0x13579bdf,
    validationSeed: 0x2468ace0,
    opponents,
    objective: "weighted orientation-balanced win rate minus a worst-matchup penalty",
  },
  selected: validation[0],
  validation,
  training,
};
mkdirSync(dirname(outputPath), { recursive: true });
writeFileSync(outputPath, `${JSON.stringify(artifact, null, 2)}\n`);
console.log(`\nSelected ${validation[0].name}: ${(validation[0].weightedWinRate * 100).toFixed(2)}% weighted validation win rate.`);
console.log(`Wrote ${outputPath}`);
