"use client";

import { useEffect, useMemo, useRef, useState } from "react";
import {
  BatchResult, CARDS, GameAction, GameSummary, PIVOTAL_THRESHOLD, PLAYER_NAMES,
  POLICY_TECHNICAL, replayGame, runBatch, STRATEGIES, StrategyId,
} from "@/lib/fish-engine";

const GAME_COUNTS = [100, 250, 500, 1000, 2500, 5000];
const POLICY_ORDER: StrategyId[] = ["fishbot", "kv_search", "lockout", "detective", "fishbot_v02", "diversifier", "hunter", "bluffer", "random"];
const SPEEDS = [.25, .5, 1, 2, 4];

const pct = (value: number, digits = 1) => `${(value * 100).toFixed(digits)}%`;
const fixed = (value: number, digits = 2) => Number.isFinite(value) ? value.toFixed(digits) : "—";
const teamName = (team: 0 | 1) => team ? "Team B" : "Team A";
const isRed = (card: number) => ["Hearts", "Diamonds"].includes(CARDS[card].suit);

function Toggle({ checked, onChange, label, detail }: { checked: boolean; onChange: (value: boolean) => void; label: string; detail: string }) {
  return <button className="switchRow" role="switch" aria-checked={checked} onClick={() => onChange(!checked)}>
    <span><b>{label}</b><small>{detail}</small></span><i className={checked ? "on" : ""} />
  </button>;
}

function PolicySelect({ value, onChange, label }: { value: StrategyId; onChange: (value: StrategyId) => void; label: string }) {
  return <label className="selectField"><span>{label}</span><select value={value} onChange={event => onChange(event.target.value as StrategyId)}>
    {POLICY_ORDER.map(id => <option value={id} key={id}>{STRATEGIES[id].name}</option>)}
  </select><small>{STRATEGIES[value].description}</small></label>;
}

function CardChip({ card, highlight = false }: { card: number; highlight?: boolean }) {
  return <span className={`cardChip ${isRed(card) ? "red" : ""} ${highlight ? "highlight" : ""}`} title={CARDS[card].label}>{CARDS[card].compact}</span>;
}

function HandPanel({ player, hand, actor, target, askedCard, success }: { player: number; hand: number[]; actor?: number; target?: number; askedCard?: number; success?: boolean }) {
  return <article className={`handPanel team${player % 2 ? "B" : "A"} ${actor === player ? "actor" : ""} ${target === player ? "target" : ""}`}>
    <header><span className="playerToken">{PLAYER_NAMES[player][0]}</span><div><b>{PLAYER_NAMES[player]}</b><small>{teamName((player % 2) as 0 | 1)} · {hand.length} cards</small></div>{actor === player && <em>ASKING</em>}{target === player && <em>ASKED</em>}</header>
    <div className="cards">{hand.length ? hand.map(card => <CardChip key={card} card={card} highlight={success && actor === player && askedCard === card} />) : <span className="emptyHand">No cards</span>}</div>
  </article>;
}

function DecisionReadout({ action }: { action: GameAction }) {
  if (!action.decision) return <div className="decisionEmpty">This action has no ask evaluation.</div>;
  const d = action.decision;
  return <div className="decisionReadout">
    <div className="decisionSummary"><b>Why this ask?</b><p>{STRATEGIES[d.policy].name} selected it from all legal card–target pairs. FishBot v0.3 estimated a <strong>{pct(d.features.hitProbability, 0)}</strong> hit probability, <strong>{pct(d.features.teamControl, 0)}</strong> team control, and <strong>{pct(d.features.replyThreat, 0)}</strong> opponent reply threat after a miss.</p></div>
    <div className="featureGrid">
      <span><small>P(hit)</small><b>{pct(d.features.hitProbability, 0)}</b></span>
      <span><small>Info gain</small><b>{fixed(d.features.informationGain)} bits</b></span>
      <span><small>Set progress</small><b>{pct(d.features.setProgress, 0)}</b></span>
      <span><small>Team control</small><b>{pct(d.features.teamControl, 0)}</b></span>
      <span><small>Continuation</small><b>{pct(d.features.continuationValue, 0)}</b></span>
      <span><small>Reply threat</small><b>{pct(d.features.replyThreat, 0)}</b></span>
      <span><small>v0.3 utility</small><b>{fixed(d.fishbotScore)}</b></span>
      <span><small>Decision regret</small><b>{fixed(d.regret)}</b></span>
    </div>
    <div className="alternatives"><small>FishBot v0.3 top candidates</small>{d.alternatives.map((alt, index) => <span key={`${alt.card}-${alt.target}`}><i>{index + 1}</i><b>{CARDS[alt.card].compact} from {PLAYER_NAMES[alt.target]}</b><em>{fixed(alt.score)}</em></span>)}</div>
  </div>;
}

function Replay({ game, onClose }: { game: GameSummary; onClose: () => void }) {
  const detailed = useMemo(() => replayGame(game), [game]);
  const log = useMemo(() => detailed.log ?? [], [detailed.log]);
  const [step, setStep] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(1);
  const [pivotalOnly, setPivotalOnly] = useState(false);
  const action = step === 0 ? undefined : log[step - 1];
  const hands = action?.hands ?? detailed.initialHands ?? [];
  const moments = useMemo(() => log.map((entry, index) => ({ entry, step: index + 1 })).filter(item => (item.entry.pivotalScore ?? 0) >= PIVOTAL_THRESHOLD), [log]);

  const move = (direction: 1 | -1) => {
    setStep(current => {
      if (!pivotalOnly) return Math.max(0, Math.min(log.length, current + direction));
      const stops = [0, ...moments.map(moment => moment.step), log.length].filter((value, index, values) => values.indexOf(value) === index).sort((a, b) => a - b);
      if (direction > 0) return stops.find(value => value > current) ?? current;
      return [...stops].reverse().find(value => value < current) ?? current;
    });
  };

  useEffect(() => {
    if (!playing) return;
    const timer = window.setTimeout(() => {
      setStep(current => {
        const stops = pivotalOnly ? [0, ...moments.map(moment => moment.step), log.length].filter((value, index, values) => values.indexOf(value) === index).sort((a, b) => a - b) : Array.from({ length: log.length + 1 }, (_, index) => index);
        const next = stops.find(value => value > current);
        if (next === undefined) { setPlaying(false); return current; }
        return next;
      });
    }, 1400 / speed);
    return () => window.clearTimeout(timer);
  }, [playing, step, speed, pivotalOnly, moments, log.length]);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key === "Escape") onClose();
      if (event.key === "ArrowRight") move(1);
      if (event.key === "ArrowLeft") move(-1);
      if (event.key === " ") { event.preventDefault(); setPlaying(value => !value); }
    };
    window.addEventListener("keydown", onKey); return () => window.removeEventListener("keydown", onKey);
  });

  const eventWindow = log.slice(Math.max(0, step - 4), Math.min(log.length, step + 3));
  const card = action?.card === undefined ? undefined : CARDS[action.card];

  return <div className="replayOverlay" role="dialog" aria-modal="true" aria-label="Game replay">
    <div className="replayConsole">
      <header className="replayTopbar"><div><b>Game #{game.seed}</b><span>{STRATEGIES[game.strategies[0]].short} vs {STRATEGIES[game.strategies[1]].short}</span></div><div className="replayFinal"><small>FINAL</small><b>{game.score[0]}–{game.score[1]}</b></div><button onClick={onClose} aria-label="Close replay">×</button></header>
      <div className="replayToolbar">
        <div className="transport"><button onClick={() => setStep(0)} aria-label="Start">↤</button><button onClick={() => move(-1)} aria-label="Previous">←</button><button className="primary" onClick={() => setPlaying(value => !value)}>{playing ? "Pause" : "Play"}</button><button onClick={() => move(1)} aria-label="Next">→</button><button onClick={() => setStep(log.length)} aria-label="End">↦</button></div>
        <label>Speed <select value={speed} onChange={event => setSpeed(Number(event.target.value))}>{SPEEDS.map(value => <option value={value} key={value}>{value}×</option>)}</select></label>
        <button className={`pivotalToggle ${pivotalOnly ? "active" : ""}`} onClick={() => setPivotalOnly(value => !value)}>Pivotal only {pivotalOnly ? "on" : "off"}</button>
        <span>Step {step} / {log.length}</span>
      </div>
      <div className={`transferBanner ${action?.success === false ? "miss" : ""}`}>
        {!action ? <><b>Initial deal</b><span>All 54 cards shown in the omniscient replay view.</span></> : action.type === "ask" ? <><b>{PLAYER_NAMES[action.actor]} → {PLAYER_NAMES[action.target!]}</b><strong>{card?.compact}</strong><span>{action.success ? `HIT · ${PLAYER_NAMES[action.target!]} transfers ${card?.compact} to ${PLAYER_NAMES[action.actor]}; turn retained.` : `MISS · ${PLAYER_NAMES[action.target!]} takes the turn.`}</span></> : <><b>{action.text}</b><span>{action.annotation}</span></>}
        {action && (action.pivotalScore ?? 0) >= PIVOTAL_THRESHOLD && <em>PIVOTAL {fixed(action.pivotalScore, 1)}</em>}
      </div>
      <div className="replayMain">
        <section className="handsStage">
          <div className="teamHeader"><b>Team A</b><span>players 1 · 3 · 5</span><strong>{action?.score[0] ?? 0} sets</strong></div>
          <div className="teamHands">{[0, 2, 4].map(player => <HandPanel key={player} player={player} hand={hands[player] ?? []} actor={action?.actor} target={action?.target} askedCard={action?.card} success={action?.success} />)}</div>
          <div className="teamHeader teamB"><b>Team B</b><span>players 2 · 4 · 6</span><strong>{action?.score[1] ?? 0} sets</strong></div>
          <div className="teamHands">{[1, 3, 5].map(player => <HandPanel key={player} player={player} hand={hands[player] ?? []} actor={action?.actor} target={action?.target} askedCard={action?.card} success={action?.success} />)}</div>
          <div className="timelineWrap"><input aria-label="Replay position" type="range" min="0" max={log.length} value={step} onChange={event => setStep(Number(event.target.value))} /><div className="timelineMarks">{moments.map(moment => <button key={moment.step} style={{ left: `${moment.step / Math.max(1, log.length) * 100}%` }} onClick={() => setStep(moment.step)} aria-label={`Pivotal action ${moment.step}`} />)}</div></div>
          {action && <DecisionReadout action={action} />}
        </section>
        <aside className="replayRail">
          <section><header><b>Pivotal moments</b><span>{moments.length}</span></header><div className="momentList">{moments.length ? moments.map(moment => <button className={moment.step === step ? "active" : ""} key={moment.step} onClick={() => setStep(moment.step)}><i>{moment.step}</i><span><b>{moment.entry.text}</b><small>{moment.entry.pivotalReasons?.join(" · ")}</small></span><em>{fixed(moment.entry.pivotalScore ?? 0, 1)}</em></button>) : <p>No actions crossed the pivotal threshold.</p>}</div></section>
          <section><header><b>Nearby actions</b><span>{step}</span></header><div className="eventList">{eventWindow.map(entry => <button className={entry.index + 1 === step ? "active" : ""} key={entry.index} onClick={() => setStep(entry.index + 1)}><i>{entry.index + 1}</i><span>{entry.text}</span><em>{entry.type === "ask" ? entry.success ? "hit" : "miss" : entry.type}</em></button>)}</div></section>
        </aside>
      </div>
    </div>
  </div>;
}

function ScoreDistribution({ result }: { result: BatchResult }) {
  const max = Math.max(...result.scoreDistribution, 1);
  return <div className="scoreChart">{result.scoreDistribution.map((count, scoreA) => count ? <div key={scoreA}><span style={{ height: `${Math.max(3, count / max * 100)}%` }} /><small>{scoreA}–{9 - scoreA}</small><em>{pct(count / result.games, 0)}</em></div> : null)}</div>;
}

function MetricRow({ label, a, b, format = fixed, note }: { label: string; a: number; b: number; format?: (value: number) => string; note: string }) {
  return <div className="comparisonRow"><span><b>{label}</b><small>{note}</small></span><strong>{format(a)}</strong><strong>{format(b)}</strong></div>;
}

export function FishLab() {
  const [teamA, setTeamA] = useState<StrategyId>("fishbot");
  const [teamB, setTeamB] = useState<StrategyId>("lockout");
  const [countIndex, setCountIndex] = useState(2);
  const [seed, setSeed] = useState(20260820);
  const [tells, setTells] = useState(true);
  const [declarations, setDeclarations] = useState(true);
  const [result, setResult] = useState<BatchResult | null>(null);
  const [running, setRunning] = useState(false);
  const [replay, setReplay] = useState<GameSummary | null>(null);
  const workerRef = useRef<Worker | null>(null);

  const execute = (initial = false) => {
    setRunning(true);
    window.setTimeout(() => {
      const games = initial ? 200 : GAME_COUNTS[countIndex];
      const options = { strategies: [teamA, teamB] as [StrategyId, StrategyId], psychologicalTells: tells, declarations, maxActions: 360, seed };
      const finish = (next: BatchResult) => { setResult(next); setRunning(false); workerRef.current?.terminate(); workerRef.current = null; };
      try {
        workerRef.current?.terminate();
        const worker = new Worker(new URL("../lib/fish-worker.ts", import.meta.url), { type: "module" });
        workerRef.current = worker;
        worker.onmessage = event => finish(event.data as BatchResult);
        worker.onerror = () => finish(runBatch(games, options));
        worker.postMessage({ games, options });
      } catch {
        finish(runBatch(games, options));
      }
    }, 20);
  };
  useEffect(() => { const timer = window.setTimeout(() => execute(true), 0); return () => { window.clearTimeout(timer); workerRef.current?.terminate(); }; }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const resultA = result ? STRATEGIES[result.strategies[0]] : STRATEGIES[teamA];
  const resultB = result ? STRATEGIES[result.strategies[1]] : STRATEGIES[teamB];
  const ciCrossesTie = result ? result.winRateCI[0] <= .5 && result.winRateCI[1] >= .5 : true;

  return <main className="shell">
    <header className="topbar"><a className="brand" href="#lab"><span className="brandmark">F</span><span>FishLab</span><small>research console</small></a><nav aria-label="Primary"><a className="active" href="#lab">Experiment</a><a href="#outliers">Games</a><a href="#models">Models</a><a href="#statistics">Statistics</a></nav><span className="buildLabel">MODEL v0.3</span></header>
    <section className="researchHeader" id="lab"><div><p className="eyebrow"><span /> Canadian Fish analysis</p><h1>Simulation laboratory</h1><p className="lede">Compare decision policies, measure information quality, and jump directly to pivotal actions in reproducible games.</p></div><button className="runButton" onClick={() => execute()} disabled={running}>{running ? "Simulating…" : "Run experiment"}<b>→</b></button></section>

    <section className="labGrid">
      <aside className="configuration"><header><b>Experiment configuration</b><span>01</span></header><PolicySelect label="Team A policy" value={teamA} onChange={setTeamA} /><PolicySelect label="Team B policy" value={teamB} onChange={setTeamB} />
        <label className="rangeField"><span>Games <b>{GAME_COUNTS[countIndex].toLocaleString()}</b></span><input type="range" min="0" max={GAME_COUNTS.length - 1} value={countIndex} onChange={event => setCountIndex(Number(event.target.value))} /><small>100 <em>larger samples reduce uncertainty</em> 5,000</small></label>
        <label className="seedField"><span>Base seed</span><input type="number" value={seed} onChange={event => setSeed(Number(event.target.value) || 1)} /></label>
        <Toggle checked={tells} onChange={setTells} label="Reactive heuristics" detail="Baseline opponents favor direct ask responses" /><Toggle checked={declarations} onChange={setDeclarations} label="Strategic declarations" detail="Policies declare from confidence, not hidden cards" />
        <button className="executeButton" onClick={() => execute()} disabled={running}>{running ? "Running deterministic games…" : `Run ${GAME_COUNTS[countIndex].toLocaleString()} games`}</button>
        <p className="configNote">One configuration is varied at a time. The seed makes every deal reproducible. Swap team positions in a second run to check for orientation effects.</p>
      </aside>

      <section className="resultsPanel" aria-busy={running}>
        <header className="panelHeader"><div><small>LATEST RUN</small><h2>{result ? `${result.games.toLocaleString()} completed games` : "Initializing experiment"}</h2></div>{result && <span>seed {result.seed} · {fixed(result.elapsedMs, 0)} ms</span>}</header>
        {result ? <>
          <div className="winSummary"><div><span>Team A win rate</span><b>{pct(result.winRateA)}</b><small>{resultA.name}</small></div><div className="confidence"><b>95% interval {pct(result.winRateCI[0])}–{pct(result.winRateCI[1])}</b><p>{ciCrossesTie ? "This sample does not yet distinguish the policies from a 50/50 result." : `${result.winRateA > .5 ? resultA.name : resultB.name} has a statistically separated lead in this run.`}</p></div><div><span>Team B win rate</span><b>{pct(1 - result.winRateA)}</b><small>{resultB.name}</small></div></div>
          <div className="winBar"><span style={{ width: `${result.winRateA * 100}%` }}>A</span><span>B</span></div>
          <div className="keyMetrics"><article><small>Median length</small><b>{result.medianActions}</b><span>actions · p90 {result.p90Actions}</span></article><article><small>Pivotal moments</small><b>{fixed(result.avgPivotalMoments, 1)}</b><span>per game</span></article><article><small>Reaction conversion</small><b>{pct(result.reactionAccuracy)}</b><span>hit after receiving turn</span></article><article><small>Mean score</small><b>{fixed(result.avgScore[0])}–{fixed(result.avgScore[1])}</b><span>sets</span></article></div>
          <div className="resultColumns"><section className="comparison"><header><b>Policy comparison</b><span>{resultA.short}</span><span>{resultB.short}</span></header><MetricRow label="Ask accuracy" note="requested cards transferred" a={result.teamAskAccuracy[0]} b={result.teamAskAccuracy[1]} format={value => pct(value)} /><MetricRow label="Declaration accuracy" note="exact set allocations" a={result.teamDeclarationAccuracy[0]} b={result.teamDeclarationAccuracy[1]} format={value => pct(value)} /><MetricRow label="Information / ask" note="binary entropy resolved" a={result.avgInformationGain[0]} b={result.avgInformationGain[1]} /><MetricRow label="Reply risk / miss" note="danger of handing over the turn" a={result.avgReplyThreatOnMiss[0]} b={result.avgReplyThreatOnMiss[1]} /><MetricRow label="v0.3 regret" note="utility below best v0.3 ask" a={result.avgDecisionRegret[0]} b={result.avgDecisionRegret[1]} /></section><section className="scoreDistribution"><header><b>Final score distribution</b><span>Team A sets won</span></header><ScoreDistribution result={result} /></section></div>
        </> : <div className="loadingState">Creating belief states and dealing 54 cards…</div>}
      </section>
    </section>

    {result && <section className="gamesSection" id="outliers"><header className="sectionHeader"><div><small>02 · GAME DISCOVERY</small><h2>Outliers and replay candidates</h2></div><p>Games are ranked by pivotal decisions, declaration failures, lead changes, comebacks, and peak moment impact. Open one and use “Pivotal only” to skip routine asks.</p></header><div className="gamesTable"><div className="gamesHead"><span>Game</span><span>Classification</span><span>Final</span><span>Pivotal</span><span>Peak</span><span>Length</span><span /></div>{result.outliers.map(game => <button key={game.seed} onClick={() => setReplay(game)}><span><b>#{game.seed}</b><small>{STRATEGIES[game.strategies[0]].short} vs {STRATEGIES[game.strategies[1]].short}</small></span><span><em>{game.tag}</em><small>{game.maxComeback}-set comeback · {game.failedDeclarations} bad declarations</small></span><span>{game.score[0]}–{game.score[1]}</span><span>{game.pivotalCount}</span><span>{fixed(game.peakPivotal, 1)}</span><span>{game.actions}</span><span>Replay →</span></button>)}</div></section>}

    <section className="modelsSection" id="models"><header className="sectionHeader"><div><small>03 · DECISION MODELS</small><h2>What each computer actually does</h2></div><p>These policies are numeric and inspectable. “Reacting to an ask” means updating a defined belief state or applying a defined response weight—not sending prose to an LLM.</p></header><div className="fishbotNote"><b>FishBot v0.3 is the strongest tested policy—not solved Fish.</b><p>It conditions card beliefs on public hand counts, remembers every ask, prioritizes high-probability transfers, and prices continuation, half-suit completion, and the danger of missing into a strong opponent. Its fixed v0.2 predecessor and the literature-derived lockout challenger remain available as adversarial baselines.</p></div><div className="policyList">{POLICY_ORDER.map((id, index) => { const policy = STRATEGIES[id], technical = POLICY_TECHNICAL[id]; return <details key={id} open={id === "fishbot"}><summary><span>{String(index + 1).padStart(2, "0")}</span><div><b>{policy.name}</b><small>{technical.class}</small></div><em>declaration ≥ {pct(policy.declarationThreshold, 0)}</em></summary><div className="policyBody"><section><small>OBJECTIVE</small><p>{technical.objective}</p></section><section><small>ASK SCORE</small><code>{technical.askFormula}</code></section><section><small>REACTION TO PUBLIC ASKS</small><p>{technical.reactsToAsk}</p></section><section><small>DECLARATION</small><p>{technical.declarationRule}</p></section><section><small>LIMITATION</small><p>{technical.limitations}</p></section></div></details>; })}</div></section>

    <section className="statisticsSection" id="statistics"><header className="sectionHeader"><div><small>04 · RESEARCH GUIDE</small><h2>Statistics that answer strategic questions</h2></div><p>Win rate is an outcome, not an explanation. The supporting metrics below identify how a policy creates—or destroys—value.</p></header><div className="definitionGrid"><article><b>Win-rate interval</b><p>Wilson 95% interval around observed wins. If it crosses 50%, treat the current head-to-head result as inconclusive.</p></article><article><b>FishBot regret</b><p>Difference between the selected ask’s utility and the highest FishBot utility available in the same information state. Zero means agreement.</p></article><article><b>Information / ask</b><p>Binary entropy of whether the target owns the requested card. High values mean the question sharply resolves uncertainty.</p></article><article><b>Reaction conversion</b><p>Hit rate on the first ask after an opponent misses against that player. Compare tells on/off to test response tactics.</p></article><article><b>Pivotal score</b><p>Combines surprise, half-suit progress, reply danger, decision regret, declaration failure, and lead change. Replay threshold: {PIVOTAL_THRESHOLD}.</p></article><article><b>Length distribution</b><p>Median gives a typical game; p90 exposes slow endgames. Both are more informative than a mean distorted by rare stalls.</p></article></div><div className="studyProtocol"><b>Recommended empirical sequence</b><ol><li>Run at least 1,000 paired seeds for one matchup.</li><li>Swap Team A and Team B and rerun the same seeds.</li><li>Change one mechanism—tells, declaration behavior, or policy—at a time.</li><li>Inspect high-regret and high-pivotal replays to explain the aggregate difference.</li><li>Only call a policy stronger when the effect survives new held-out seeds.</li></ol></div></section>

    <footer><b>FishLab v0.3</b><span>Deterministic simulation · count-conditioned hidden-information beliefs · reproducible seeds</span><a href="#lab">Back to experiment ↑</a></footer>
    {replay && <Replay game={replay} onClose={() => setReplay(null)} />}
  </main>;
}
