import assert from "node:assert/strict";
import { CARDS, HALF_SUITS, simulateGame, StrategyId } from "../lib/fish-engine.ts";

assert.equal(CARDS.length, 54, "deck must contain 54 cards");
assert.equal(new Set(CARDS.map(card => card.id)).size, 54, "card ids must be unique");
assert.equal(HALF_SUITS.length, 9, "deck must contain nine half-suits");
assert.ok(HALF_SUITS.every(set => set.cards.length === 6), "each half-suit must contain six cards");

const strategies: StrategyId[] = ["fishbot", "fishbot_v02", "lockout", "hunter", "diversifier", "detective", "bluffer", "random"];
let checkedAsks = 0;
for (let i = 0; i < 250; i++) {
  const config = {
    seed: 1000 + i,
    strategies: [strategies[i % strategies.length], strategies[(i * 3 + 1) % strategies.length]] as [StrategyId, StrategyId],
    psychologicalTells: i % 2 === 0,
    declarations: true,
    detailed: true,
    maxActions: 360,
  };
  const game = simulateGame(config);
  assert.equal(game.score[0] + game.score[1], 9, "all nine sets must be awarded");
  assert.ok(game.winner === 0 || game.winner === 1, "winner must be a team");
  assert.equal(game.winner, game.score[1] > game.score[0] ? 1 : 0, "winner must match score");
  assert.ok(game.log?.length, "detailed games must retain an action log");
  assert.ok(!game.log!.some(action => action.type === "adjudicate"), "normal games must finish without safety adjudication");
  for (const action of game.log!) {
    assert.equal(action.cardCounts.length, 6);
    assert.ok(action.cardCounts.every(count => count >= 0 && count <= 54));
    if (action.type !== "ask") continue;
    checkedAsks++;
    assert.notEqual(action.actor % 2, action.target! % 2, "asks must target the opposing team");
    assert.ok(action.card !== undefined && action.set !== undefined);
    assert.ok(action.decision, "every ask must retain an explainable decision trace");
    assert.ok(Number.isFinite(action.decision!.features.expectedUtility));
    assert.ok(Object.values(action.decision!.features).every(Number.isFinite), "all decision features must be finite");
    assert.ok(action.decision!.features.hitProbability >= 0 && action.decision!.features.hitProbability <= 1);
    assert.ok(action.decision!.features.teamControl >= 0 && action.decision!.features.teamControl <= 1);
    assert.ok(action.decision!.features.replyThreat >= 0 && action.decision!.features.replyThreat <= 1);
    assert.ok(action.decision!.alternatives.length >= 1);
    assert.equal(CARDS[action.card!].set, action.set);
    const actorHand = action.hands![action.actor];
    assert.ok(actorHand.some(card => CARDS[card].set === action.set && card !== action.card), "asker must hold another card in the half-suit");
    if (action.success) assert.ok(actorHand.includes(action.card!), "a successful ask must transfer the card to the asker");
    else assert.ok(!actorHand.includes(action.card!), "a missed card cannot appear in the asker's hand");
  }
  const duplicate = simulateGame(config);
  assert.deepEqual(duplicate, game, "same seed and policy must reproduce exactly");
}

assert.ok(checkedAsks > 10_000, "verification should exercise a large ask sample");
console.log(`Engine verified: 250 reproducible games, ${checkedAsks.toLocaleString()} legal asks, zero safety adjudications.`);
