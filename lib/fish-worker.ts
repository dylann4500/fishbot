/// <reference lib="webworker" />
import { runBatch, type GameOptions } from "./fish-engine";

type BatchRequest = {
  games: number;
  options: Omit<GameOptions, "seed" | "detailed"> & { seed?: number };
};

self.onmessage = (event: MessageEvent<BatchRequest>) => {
  const { games, options } = event.data;
  self.postMessage(runBatch(games, options));
};
