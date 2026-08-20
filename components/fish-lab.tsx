"use client";

import { useEffect, useMemo, useState } from "react";
import {
  BatchResult, CARDS, GameSummary, PLAYER_NAMES, replayGame, runBatch,
  STRATEGIES, StrategyId,
} from "@/lib/fish-engine";

const GAME_COUNTS = [100, 250, 500, 1000, 2500, 5000];
const strategyOrder: StrategyId[] = ["hunter", "diversifier", "detective", "bluffer", "random"];
const players = PLAYER_NAMES.map((name, i) => ({ name, team: i % 2 ? "B" : "A", tone: i % 2 ? "blue" : "coral" }));

function pct(v: number) { return `${(v * 100).toFixed(1)}%`; }
function signed(v: number) { return `${v >= 0 ? "+" : ""}${v.toFixed(1)}`; }

function StrategySelect({ value, onChange, team }: { value: StrategyId; onChange: (v: StrategyId) => void; team: "A" | "B" }) {
  const strategy = STRATEGIES[value];
  return (
    <div className={`strategySelect ${team === "A" ? "coral" : "blue"}`}>
      <span className="strategyIcon">{strategy.icon}</span>
      <span><strong>{strategy.name}</strong><small>{strategy.description}</small></span>
      <select value={value} onChange={e => onChange(e.target.value as StrategyId)} aria-label={`Team ${team} strategy`}>
        {strategyOrder.map(id => <option key={id} value={id}>{STRATEGIES[id].name}</option>)}
      </select>
      <span className="chevron">⌄</span>
    </div>
  );
}

function Toggle({ checked, onChange, label, detail }: { checked: boolean; onChange: (v: boolean) => void; label: string; detail: string }) {
  return (
    <button className="toggleRow" role="switch" aria-checked={checked} onClick={() => onChange(!checked)}>
      <span><strong>{label}</strong><small>{detail}</small></span><i className={`toggle ${checked ? "on" : ""}`} />
    </button>
  );
}

function MiniTable({ counts = [9, 9, 9, 9, 9, 9], actor, target }: { counts?: number[]; actor?: number; target?: number }) {
  return (
    <div className="tableWrap compactTable">
      <div className="felt"><div className="feltCenter"><span>F</span><small>9 half-suits<br />54 cards</small></div></div>
      {players.map((p, i) => (
        <div key={p.name} className={`seat seat${i + 1} ${actor === i ? "acting" : ""} ${target === i ? "targeted" : ""}`}>
          <div className={`avatar ${p.tone}`}>{p.name[0]}</div>
          <span><strong>{p.name}</strong><small>Team {p.team} · {counts[i]} card{counts[i] === 1 ? "" : "s"}</small></span>
        </div>
      ))}
    </div>
  );
}

function Replay({ game, strategies, tells, declarations, onClose }: { game: GameSummary; strategies: [StrategyId, StrategyId]; tells: boolean; declarations: boolean; onClose: () => void }) {
  const detailed = useMemo(() => replayGame(game, strategies, tells, declarations), [game, strategies, tells, declarations]);
  const [step, setStep] = useState(0);
  const [playing, setPlaying] = useState(false);
  const action = detailed.log?.[step];
  useEffect(() => {
    if (!playing || !detailed.log) return;
    const timer = window.setInterval(() => setStep(s => s >= detailed.log!.length - 1 ? (setPlaying(false), s) : s + 1), 700);
    return () => window.clearInterval(timer);
  }, [playing, detailed.log]);
  useEffect(() => {
    const key = (e: KeyboardEvent) => { if (e.key === "Escape") onClose(); if (e.key === "ArrowRight") setStep(s => Math.min((detailed.log?.length ?? 1) - 1, s + 1)); if (e.key === "ArrowLeft") setStep(s => Math.max(0, s - 1)); };
    window.addEventListener("keydown", key); return () => window.removeEventListener("keydown", key);
  }, [detailed.log?.length, onClose]);
  if (!action) return null;
  const card = action.card === undefined ? null : CARDS[action.card];
  return (
    <div className="modalBackdrop" role="dialog" aria-modal="true" aria-label="Game replay">
      <div className="replayPanel">
        <header className="replayHead">
          <div><p className="eyebrow"><span /> Game #{String(game.seed).slice(-6)}</p><h2>{game.tag}</h2></div>
          <div className="replayScore"><small>FINAL</small><b>{game.score[0]}—{game.score[1]}</b></div>
          <button className="closeButton" onClick={onClose} aria-label="Close replay">×</button>
        </header>
        <div className="replayBody">
          <div className="replayStage">
            <div className="stepMeta"><span>Action {step + 1} / {detailed.log?.length}</span><span>Team A {action.score[0]} · {action.score[1]} Team B</span></div>
            <MiniTable counts={action.cardCounts} actor={action.actor} target={action.target} />
            <div className={`actionCard ${action.success === false ? "miss" : ""}`}>
              <span className="eventIndex">{String(step + 1).padStart(3, "0")}</span>
              {card && <b className={`playingCard ${["Hearts", "Diamonds"].includes(card.suit) ? "redCard" : ""}`}>{card.compact}</b>}
              <div><strong>{action.text}</strong><small>{action.annotation}</small></div>
              <span className="result">{action.type === "ask" ? (action.success ? "HIT" : "MISS") : action.type.toUpperCase()}</span>
            </div>
            <div className="replayControls">
              <button onClick={() => setStep(0)} aria-label="First action">↤</button>
              <button onClick={() => setStep(s => Math.max(0, s - 1))} aria-label="Previous action">←</button>
              <button className="playButton" onClick={() => setPlaying(!playing)} aria-label={playing ? "Pause" : "Play"}>{playing ? "Ⅱ" : "▶"}</button>
              <button onClick={() => setStep(s => Math.min(detailed.log!.length - 1, s + 1))} aria-label="Next action">→</button>
              <button onClick={() => setStep(detailed.log!.length - 1)} aria-label="Last action">↦</button>
            </div>
            <input className="timeline" aria-label="Replay position" type="range" min="0" max={(detailed.log?.length ?? 1) - 1} value={step} onChange={e => setStep(Number(e.target.value))} />
          </div>
          <aside className="omniscient">
            <div className="sectionTitle"><span>Research lens</span><small>OMNISCIENT</small></div>
            <p>Visible only in replay. This view exposes every current hand so you can inspect what each agent actually knew—and what it merely inferred.</p>
            {action.hands?.map((hand, player) => (
              <div className="handRow" key={player}><span className={`handDot ${player % 2 ? "blue" : "coral"}`} /> <b>{PLAYER_NAMES[player]}</b><div>{hand.length ? hand.map(c => <i key={c} className={["Hearts", "Diamonds"].includes(CARDS[c].suit) ? "redCard" : ""}>{CARDS[c].compact}</i>) : <em>out</em>}</div></div>
            ))}
          </aside>
        </div>
      </div>
    </div>
  );
}

export function FishLab() {
  const [teamA, setTeamA] = useState<StrategyId>("hunter");
  const [teamB, setTeamB] = useState<StrategyId>("diversifier");
  const [countIndex, setCountIndex] = useState(3);
  const [tells, setTells] = useState(true);
  const [declarations, setDeclarations] = useState(true);
  const [result, setResult] = useState<BatchResult | null>(null);
  const [running, setRunning] = useState(false);
  const [replay, setReplay] = useState<GameSummary | null>(null);
  const [completedAt, setCompletedAt] = useState("");
  const [lastConfig, setLastConfig] = useState<{ strategies: [StrategyId, StrategyId]; tells: boolean; declarations: boolean }>({ strategies: ["hunter", "diversifier"], tells: true, declarations: true });

  const run = () => {
    setRunning(true);
    window.setTimeout(() => {
      const games = GAME_COUNTS[countIndex];
      const next = runBatch(games, { strategies: [teamA, teamB], psychologicalTells: tells, declarations, maxActions: 360 });
      setResult(next); setLastConfig({ strategies: [teamA, teamB], tells, declarations }); setRunning(false); setCompletedAt(new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }));
      document.getElementById("results")?.scrollIntoView({ behavior: "smooth", block: "center" });
    }, 40);
  };
  useEffect(() => {
    const timer = window.setTimeout(() => {
      setResult(runBatch(250, { strategies: ["hunter", "diversifier"], psychologicalTells: true, declarations: true, maxActions: 360 }));
    }, 0);
    return () => window.clearTimeout(timer);
  }, []);

  const strategyDelta = result ? (result.winRateA - .5) * 100 : 0;
  const winnerName = result ? STRATEGIES[result.winRateA >= .5 ? teamA : teamB].name : "—";
  const winnerTeam = result?.winRateA && result.winRateA >= .5 ? "A" : "B";

  return (
    <main className="shell">
      <header className="topbar">
        <a className="brand" href="#lab"><span className="brandmark">F</span><span>FishLab</span><small>strategy simulator</small></a>
        <nav aria-label="Primary"><a className="active" href="#lab">Simulation lab</a><a href="#findings">Findings</a><a href="#rules">Rules</a></nav>
        <a className="iconButton" href="#rules" aria-label="About FishLab">?</a>
      </header>

      <section className="hero" id="lab">
        <div><p className="eyebrow"><span /> Research workbench 01</p><h1>What does <em>optimal</em><br />Fish look like?</h1><p className="lede">Pit competing strategies against each other. Run thousands of games. Find the moments where information, misdirection, and timing decide everything.</p></div>
        <button className="runButton" onClick={run} disabled={running}><span>{running ? "Simulating…" : "Run experiment"}</span><b>{running ? "···" : "→"}</b></button>
      </section>

      <section className="workspace">
        <aside className="setup">
          <div className="sectionTitle"><span>Experiment setup</span><small>EDIT</small></div>
          <label htmlFor="game-count">Games per run <strong>{GAME_COUNTS[countIndex].toLocaleString()}</strong></label>
          <input id="game-count" type="range" min="0" max={GAME_COUNTS.length - 1} value={countIndex} onChange={e => setCountIndex(Number(e.target.value))} />
          <div className="rangeLabels"><span>100</span><span>5,000</span></div>
          <div className="divider" />
          <div className="fieldLabel">Team A strategy</div><StrategySelect team="A" value={teamA} onChange={setTeamA} />
          <div className="fieldLabel">Team B strategy</div><StrategySelect team="B" value={teamB} onChange={setTeamB} />
          <div className="divider" />
          <Toggle checked={declarations} onChange={setDeclarations} label="Strategic declarations" detail="Declare from probabilistic belief" />
          <Toggle checked={tells} onChange={setTells} label="Psychological tells" detail="React to the previous ask" />
          <button className="sideRun" onClick={run} disabled={running}>{running ? "Running seeded games…" : `Run ${GAME_COUNTS[countIndex].toLocaleString()} games`}<span>↗</span></button>
        </aside>

        <article className="arena resultsArena" id="results" aria-busy={running}>
          <div className="arenaHead"><div><p className="eyebrow"><span className={running ? "pulse" : ""} /> {running ? "Experiment in progress" : "Latest experiment"}</p><h2>{result ? `${result.games.toLocaleString()} games complete` : "Preparing the table"}</h2></div>{result && <div className="runMeta"><b>SEED 20260820</b><span>{completedAt || "baseline"} · {result.elapsedMs.toFixed(0)} ms</span></div>}</div>
          {result ? <>
            <div className="headlineResult">
              <div><small>LEADING STRATEGY</small><strong>{winnerName}</strong><span>Team {winnerTeam} · {signed(Math.abs(strategyDelta))} point win-rate edge</span></div>
              <div className="bigRate">{pct(Math.max(result.winRateA, 1 - result.winRateA))}<small>WIN RATE</small></div>
            </div>
            <div className="distribution"><div className="teamABar" style={{ width: `${result.winRateA * 100}%` }}><span>TEAM A · {pct(result.winRateA)}</span></div><div className="teamBBar"><span>{pct(1 - result.winRateA)} · TEAM B</span></div></div>
            <div className="metricGrid">
              <div><small>Ask accuracy</small><b>{pct(result.askAccuracy)}</b><span>successful transfers</span></div>
              <div><small>Declaration accuracy</small><b>{pct(result.declarationAccuracy)}</b><span>correct allocations</span></div>
              <div><small>Average game</small><b>{result.avgActions.toFixed(0)}</b><span>actions to finish</span></div>
              <div><small>Diversion rate</small><b>{pct(result.diversionRate)}</b><span>of all asks</span></div>
            </div>
            <div className="arenaSubhead"><div><span>Average final score</span><strong>{result.avgScore[0].toFixed(2)} <i>—</i> {result.avgScore[1].toFixed(2)}</strong></div><p>Each deal is reproducible. Select an unusual game below to reconstruct every ask, transfer, declaration, and hidden hand.</p></div>
            <MiniTable counts={[9, 9, 9, 9, 9, 9]} />
          </> : <div className="loadingResearch">Shuffling 54 cards and initializing private beliefs…</div>}
        </article>
      </section>

      {result && <section className="outliers" aria-labelledby="outlier-title">
        <div className="sectionIntro"><div><p className="eyebrow"><span /> Outlier library</p><h2 id="outlier-title">Games worth watching twice.</h2></div><p>Ranked by lead changes, comebacks, declaration risk, length, and misdirection. A high score means the game was strategically unusual—not necessarily well played.</p></div>
        <div className="outlierTable" role="table">
          <div className="outlierHeader" role="row"><span>Game</span><span>Why it surfaced</span><span>Score</span><span>Asks</span><span>Turns</span><span /></div>
          {result.outliers.slice(0, 7).map((game, i) => <button className="outlierRow" role="row" key={game.seed} onClick={() => setReplay(game)}>
            <span><i>{String(i + 1).padStart(2, "0")}</i><b>#{String(game.seed).slice(-6)}</b></span>
            <span><em className={`tag tag${i % 3}`}>{game.tag}</em><small>{game.maxComeback ? `${game.maxComeback}-set comeback · ` : ""}{game.failedDeclarations ? `${game.failedDeclarations} failed declaration${game.failedDeclarations > 1 ? "s" : ""}` : `${game.leadChanges} lead changes`}</small></span>
            <span><b className="tableScore">{game.score[0]}—{game.score[1]}</b></span><span>{game.successfulAsks}/{game.asks}</span><span>{game.actions}</span><span className="watch">Watch replay <b>→</b></span>
          </button>)}
        </div>
      </section>}

      <section className="findings" id="findings">
        <div className="sectionIntro"><div><p className="eyebrow"><span /> Live interpretation</p><h2>What this experiment suggests.</h2></div><p>These are descriptive findings from the current seeded run, not a proof of solved play. Change one assumption at a time, rerun, and look for effects that survive larger samples.</p></div>
        <div className="findingGrid">
          <article><span className="findingNo">01</span><h3>{Math.abs(strategyDelta) < 2 ? "No meaningful strategy edge—yet." : `${winnerName} has the clearest edge.`}</h3><p>{Math.abs(strategyDelta) < 2 ? "The matchup is inside a two-point practical tie. Increase the sample or change a single behavioral switch." : `Across this sample, Team ${winnerTeam} wins ${Math.abs(strategyDelta).toFixed(1)} percentage points more often than chance.`}</p><b>{result ? pct(Math.max(result.winRateA, 1 - result.winRateA)) : "—"}<small>observed win rate</small></b></article>
          <article><span className="findingNo">02</span><h3>Information quality beats ask volume.</h3><p>Ask accuracy measures whether location beliefs convert into actual transfers. The detective archetype should separate here before it necessarily separates in wins.</p><b>{result ? pct(result.askAccuracy) : "—"}<small>asks converted</small></b></article>
          <article><span className="findingNo">03</span><h3>Declarations are the risk frontier.</h3><p>A declaration is both a set win and an allocation test. Lower thresholds create dramatic failures; higher thresholds can surrender tempo.</p><b>{result ? pct(result.declarationAccuracy) : "—"}<small>allocation accuracy</small></b></article>
        </div>
        <div className="methodNote"><span>Method note</span><p>Agents never inspect opponents’ cards. They update card-location probabilities from their own hand, successful transfers, misses, and the half-suits implied by public asks. “Psychological tells” changes how a responder values an immediate same-suit answer versus a diversion. Randomized choices are generated from a fixed seed, so the same experiment is exactly reproducible.</p></div>
      </section>

      <section className="rules" id="rules">
        <div><p className="eyebrow"><span /> Model specification</p><h2>Canadian Fish,<br />encoded faithfully.</h2><p>The simulator uses six alternating players, two teams, 54 cards, and nine six-card half-suits. The interface keeps the research choices legible without hiding the mechanics.</p></div>
        <ol>
          <li><span>01</span><div><h3>Ask legally</h3><p>Name one card, ask an opponent with cards, and hold another card from that same half-suit.</p></div></li>
          <li><span>02</span><div><h3>Transfer the turn</h3><p>A hit moves the card and keeps the turn. A miss hands the turn to the player who was asked.</p></div></li>
          <li><span>03</span><div><h3>Declare precisely</h3><p>Name every teammate holding every card. Any wrong owner—or any opponent-held card—awards the set away.</p></div></li>
          <li><span>04</span><div><h3>Finish all nine sets</h3><p>Declared cards leave play. Cardless players drop out; if a whole team is out, the other team declares the remainder.</p></div></li>
        </ol>
      </section>
      <footer><div className="brand"><span className="brandmark">F</span><span>FishLab</span></div><p>A computational notebook for strategy, information, and misdirection.</p><a href="#lab">Back to the table ↑</a></footer>
      {replay && <Replay game={replay} strategies={lastConfig.strategies} tells={lastConfig.tells} declarations={lastConfig.declarations} onClose={() => setReplay(null)} />}
    </main>
  );
}
