#!/usr/bin/env python3
"""Turn the v0.4 experiment artifacts into LaTeX tables and inline numbers."""
import json, math, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
RES = os.path.join(ROOT, 'research', 'v04', 'results')
TAB = os.path.join(ROOT, 'paper', 'tables')
NUM = os.path.join(ROOT, 'paper', 'numbers')
os.makedirs(TAB, exist_ok=True); os.makedirs(NUM, exist_ok=True)

PRETTY = {
    'v04': r'\vfast{}', 'v03': r'\vthree{}', 'v02': r'\vtwo{}', 'lockout': 'Turn-starvation lockout',
    'detective': 'Posterior detective', 'diversifier': 'Adaptive diversifier',
    'hunter': 'Focused hunter', 'bluffer': 'Misdirection artist', 'random': 'Random legal control',
}
SHORT = {'v04': r'\vfast{}', 'v03': r'\vthree{}', 'v02': r'\vtwo{}', 'lockout': 'Lockout', 'detective': 'Detective',
         'diversifier': 'Diversifier', 'hunter': 'Hunter', 'bluffer': 'Misdirection', 'random': 'Random'}

def label(spec):
    base = spec.split(':')[0]
    return PRETTY.get(base, base)

MACRO = {
    # E1 verification
    'audit-checks': 'numAuditChecks', 'audit-violations': 'numAuditViolations',
    'audit-checks-legacy': 'numAuditChecksLegacy',
    # E2 belief cross-checks
    'belief-vs-carddp': 'numBeliefVsCardDP', 'belief-vs-sampling': 'numBeliefVsSampling',
    'sinkhorn-mean-err': 'numSinkhornMean', 'sinkhorn-max-err': 'numSinkhornMax',
    # E3 primary head-to-head
    'worst-case-winrate': 'numWorstCase', 'h2h-games': 'numHtHGames', 'h2h-deals': 'numHtHDeals',
    'v04-vs-v03': 'numVsVthree', 'v04-vs-v03-ci': 'numVsVthreeCI',
    'v04-vs-v03-wins': 'numVsVthreeWins', 'v04-vs-v03-sets': 'numVsVthreeSets',
    'v04-vs-v03-declacc': 'numVsVthreeDeclAcc', 'v03-declacc-same': 'numVthreeDeclAccSame',
    'v04-vs-v03-oot': 'numVsVthreeOOT', 'v04-vs-lockout': 'numVsLockout',
    'v04-vs-detective': 'numVsDetective', 'v04-vs-v02': 'numVsVtwo',
    'limit-rate': 'numLimitRate', 'events-per-game': 'numEventsPerGame',
    'lock-hold-v04': 'numLockHoldFour', 'lock-hold-v03': 'numLockHoldThree',
    # E4 ratings
    'v04-elo': 'numElo',
    # E5 ablations
    'abl-ref': 'numAblRef', 'abl-block': 'numAblBlock', 'abl-block-ci': 'numAblBlockCI',
    'abl-prior': 'numAblPrior', 'abl-prior-ci': 'numAblPriorCI',
    'abl-stop': 'numAblStop', 'abl-stop-ci': 'numAblStopCI',
    'abl-c5': 'numAblCFive', 'abl-c5-ci': 'numAblCFiveCI',
    'abl-twoply': 'numAblTwoPly', 'abl-twoply-ci': 'numAblTwoPlyCI',
    'abl-lockw': 'numAblLockWeight', 'abl-lockw-ci': 'numAblLockWeightCI',
    'abl-games': 'numAblGames', 'abl-deals': 'numAblDeals',
    # E6 calibration
    'decl-ece': 'numDeclECE', 'decl-obs': 'numDeclObs', 'decl-n': 'numDeclN',
    'decl-brier': 'numDeclBrier', 'decl-logloss': 'numDeclLogLoss',
    'decl-topbin-share': 'numDeclTopBinShare',
    'ask-ece': 'numAskECE', 'ask-brier': 'numAskBrier', 'ask-n': 'numAskN',
    'ask-logloss': 'numAskLogLoss',
    'v03-decl-ece': 'numVthreeDeclECE', 'v03-decl-pred': 'numVthreeDeclPred',
    'v03-decl-obs': 'numVthreeDeclObs', 'v03-decl-n': 'numVthreeDeclN',
    # E9 throughput
    'tput-fast': 'numThroughputFast', 'tput-block': 'numThroughputBlock',
    'tput-v03': 'numThroughputThree', 'tput-ratio': 'numThroughputRatio',
    # E10 local response probe
    'lbr-v04': 'numLBRFour', 'lbr-v04-ci': 'numLBRFourCI',
    'lbr-v03': 'numLBRThree', 'lbr-v03-ci': 'numLBRThreeCI',
    'lbr-detective': 'numLBRDetective', 'lbr-games': 'numLBRGames',
    'lbr-limit': 'numLBRLimit',
    # E12 unfitted belief substitution
    'e12-fast': 'numETwelveFast', 'e12-fast-ci': 'numETwelveFastCI',
    'e12-block': 'numETwelveBlock', 'e12-block-ci': 'numETwelveBlockCI',
    # E14 value fit
    'value-rows': 'numValueRows', 'value-r2': 'numValueRSq', 'value-rmse': 'numValueRmse',
    # E15 oracle
    'oracle-states': 'numOracleStates', 'oracle-states-c5': 'numOracleStatesCFive',
    'oracle-skipped': 'numOracleStatesSkipped', 'oracle-deals': 'numOracleDeals',
    'oracle-alloc-checks': 'numOracleAllocChecks', 'oracle-marg-checks': 'numOracleMargChecks',
    'oracle-team-checks': 'numOracleTeamChecks', 'oracle-best-checks': 'numOracleBestChecks',
    'oracle-best-bad': 'numOracleBestBad', 'oracle-sample-diff': 'numOracleSampleDiff',
    'oracle-draws': 'numOracleDraws', 'oracle-games': 'numOracleGames',
    # E16 pre-gate audit
    'gate-opps': 'numGateOpps', 'gate-seen': 'numGateSeen', 'gate-rejected': 'numGateRejected',
    'gate-rejected-pct': 'numGateRejectedPct', 'gate-falseneg': 'numGateFalseNeg',
    'gate-falseneg-pct': 'numGateFalseNegPct', 'gate-actions': 'numGateActionsChanged',
    'gate-actions-pct': 'numGateActionsPct', 'gate-decl-gated': 'numGateDeclGated',
    'gate-decl-ungated': 'numGateDeclUngated', 'gate-decl-lost-pct': 'numGateDeclLostPct',
    # E11 termination
    'mirror-cycle': 'numMirrorCycle', 'deadask-cost': 'numDeadAskCost',
    # configuration
    'selected-gen': 'numSelectedGen', 'n-params': 'numParams',
    'n-askfeats': 'numAskFeats', 'n-valuefeats': 'numValueFeats',
    'n-knobs': 'numKnobs', 'cem-pop': 'numCEMPop', 'cem-elite': 'numCEMElite',
    'select-softmin': 'numSelectSoftmin', 'select-peropp': 'numSelectPerOpp',
    'bootstrap-draws': 'numBootstrapDraws',
    # E2 design
    'selftest-games': 'numSelftestGames', 'selftest-checks': 'numSelftestChecks',
    # E3 extras
    'v04-vs-v03-askacc': 'numVsVthreeAskAcc', 'v04-vs-lockout-askacc': 'numVsLockoutAskAcc',
    'v04-vs-diversifier': 'numVsDiversifier', 'v04-vs-hunter': 'numVsHunter',
    'v04-vs-bluffer': 'numVsBluffer', 'v04-vs-random': 'numVsRandom',
    'v03-worst-published': 'numVthreeWorstPublished',
    # designs
    'port-deals': 'numPortDeals', 'port-games': 'numPortGames',
    'matrix-deals': 'numMatrixDeals', 'matrix-games-pair': 'numMatrixGamesPair',
    'matrix-games-cell': 'numMatrixGamesCell',
    'calib-deals': 'numCalibDeals', 'lbr-deals': 'numLBRDeals',
    'e7-deals': 'numEsevenDeals', 'e7-games': 'numEsevenGames',
    'e12-deals': 'numETwelveDeals', 'e12-games': 'numETwelveGames',
    'e13-deals': 'numEthirteenDeals', 'e17-deals': 'numEseventeenDeals',
    'oracle-maxdeals': 'numOracleMaxDeals',
    # E4 ratings
    'elo-lockout': 'numEloLockout', 'elo-detective': 'numEloDetective',
    # E5 absolutes and the remaining rows
    'abl-c5-abs': 'numAblCFiveAbs', 'abl-c4-abs': 'numAblCFourAbs',
    'abl-sinkhorn-abs': 'numAblSinkhornAbs', 'abl-block-abs': 'numAblBlockAbs',
    'abl-prior-abs': 'numAblPriorAbs', 'abl-lockw-abs': 'numAblLockWeightAbs',
    'abl-c4': 'numAblCFour', 'abl-c4-ci': 'numAblCFourCI',
    'abl-sinkhorn': 'numAblSinkhorn', 'abl-sinkhorn-ci': 'numAblSinkhornCI',
    'abl-gmap': 'numAblGmap', 'abl-gmap-ci': 'numAblGmapCI',
    'abl-threat': 'numAblThreat', 'abl-threat-ci': 'numAblThreatCI',
    'abl-leak': 'numAblLeak', 'abl-leak-ci': 'numAblLeakCI',
    'abl-value': 'numAblValue', 'abl-value-ci': 'numAblValueCI',
    'abl-runway': 'numAblRunway', 'abl-runway-ci': 'numAblRunwayCI',
    'abl-hitw': 'numAblHitWeight', 'abl-hitw-ci': 'numAblHitWeightCI',
    'abl-decl80': 'numAblDeclEighty', 'abl-decl80-ci': 'numAblDeclEightyCI',
    'abl-decl99': 'numAblDeclNinetyNine', 'abl-decl99-ci': 'numAblDeclNinetyNineCI',
    # E7 cells
    'e7-v03-legacy': 'numEsevenVthreeLegacy', 'e7-v03-legacy-ci': 'numEsevenVthreeLegacyCI',
    'e7-v03-eight': 'numEsevenVthreeEight', 'e7-v03-eight-ci': 'numEsevenVthreeEightCI',
    'e7-v03-nooot': 'numEsevenVthreeNoOOT', 'e7-v03-nooot-ci': 'numEsevenVthreeNoOOTCI',
    'e7-lockout-legacy': 'numEsevenLockoutLegacy', 'e7-lockout-eight': 'numEsevenLockoutEight',
    'e7-lockout-nooot': 'numEsevenLockoutNoOOT',
    'e7-detective-legacy': 'numEsevenDetectiveLegacy', 'e7-detective-eight': 'numEsevenDetectiveEight',
    'e7-detective-nooot': 'numEsevenDetectiveNoOOT',
    # E11 termination
    'deadask-cycle': 'numDeadAskCycle',
    # E17 arbitration
    'arb-high-v03': 'numArbHighVthree', 'arb-turn-v03': 'numArbTurnVthree',
    'arb-high-lockout': 'numArbHighLockout', 'arb-turn-lockout': 'numArbTurnLockout',
    'arb-high-detective': 'numArbHighDetective', 'arb-turn-detective': 'numArbTurnDetective',
    'arb-max-move': 'numArbMaxMove', 'arb-oot-seat-lo': 'numArbOOTSeatLo',
    'arb-oot-seat-hi': 'numArbOOTSeatHi', 'arb-oot-turn-lo': 'numArbOOTTurnLo',
    'arb-oot-turn-hi': 'numArbOOTTurnHi',
}
_NUMS = {}

def num(name, value):
    _NUMS[name] = value

def flush_numbers():
    lines = []
    for k, v in MACRO.items():
        val = _NUMS.get(k)
        if val is None: val = chr(92) + 'textsc{tbd}'
        lines.append(chr(92) + 'newcommand{' + chr(92) + v + '}{' + val + '}')
    open(os.path.join(ROOT, 'paper', 'numbers.tex'), 'w').write(chr(10).join(lines) + chr(10))


def readjsonl(path):
    if not os.path.exists(path): return []
    out = []
    for line in open(path):
        line = line.strip()
        if not line: continue
        out.append(json.loads(line))
    return out

def pct(x, d=2): return f"{100*x:.{d}f}"

# ---------------------------------------------------------------- fitted parameters
selp = os.path.join(ROOT, 'research', 'v04', 'runs', 'selected.json')
if os.path.exists(selp):
    sel = json.load(open(selp))
    v = sel['params']
    FEAT = ['hit probability', 'squared hit', 'certain hit', 'own progress', 'team control',
            'lock completion', 'continuation', 'completion bonus', 'reply threat',
            'information leak', 'target hand size', 'empties the target',
            'repeats our half-suit', 'known team cards', 'location entropy',
            'team owns $H$', 'exposure on a miss', 'trailing pressure', 'runway',
            'leak magnitude']
    KNOB = [('declare threshold', 20), ('locked threshold', 21), ('ask floor', 22),
            ('patience pool', 23), ('opp.\\ card floor', 24), ('value weight', 25),
            ('linear weight', 26), ('min.\\ team prob.', 27), ('stopping margin', 28),
            ('prior $\\theta$', 29), ('prior $\\phi$', 30),
            ('two-ply $K$', 31), ('chain weight', 32), ('threat weight', 33)]
    lines = [r"\begin{tabular}{lrlr}", r"\toprule",
             r"Ask feature & Weight & Decision knob & Value \\", r"\midrule"]
    for i in range(max(len(FEAT), len(KNOB))):
        left = f"{FEAT[i]} & {v[i]:+.3f}" if i < len(FEAT) else " & "
        right = f"{KNOB[i][0]} & {v[KNOB[i][1]]:.3f}" if i < len(KNOB) else " & "
        lines.append(left + " & " + right + r" \\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'params.tex'), 'w').write('\n'.join(lines))
    num('selected-gen', str(sel['gen']))

# ---------------------------------------------------------------- E1 / E2
v = os.path.join(RES, 'E1-verify.txt')
if os.path.exists(v):
    t = open(v).read()
    m = re.search(r'audit violations: (\d+) / (\d+) checks', t)
    if m:
        num('audit-violations', f"{int(m.group(1)):,}")
        num('audit-checks', f"{int(m.group(2)):,}".replace(',', '{,}'))
b = os.path.join(RES, 'E2-belief-selftest.txt')
if os.path.exists(b):
    t = open(b).read()
    for key, pat in [('belief-vs-carddp', r'block vs card DP \(no C5\)\s+max abs diff (\S+)'),
                     ('belief-vs-sampling', r'block vs exact sampling\s+max abs diff (\S+)')]:
        m = re.search(pat, t)
        if m:
            raw = m.group(1)
            mm = re.match(r'([\d.]+)e([+-])0*(\d+)', raw)
            num(key, (mm.group(1) + r'\times 10^{' + ('-' if mm.group(2) == '-' else '') + mm.group(3) + '}') if mm else raw)
    m = re.search(r'sinkhorn vs block\s+max ([\d.]+)\s+mean ([\d.]+)', t)
    if m:
        num('sinkhorn-max-err', m.group(1))
        num('sinkhorn-mean-err', m.group(2))

# ---------------------------------------------------------------- E3
rows = readjsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
if rows:
    lines = [r"\begin{tabular}{lrrrrrr}", r"\toprule",
             r"Opponent & Win rate & 95\% CI & Mean half-suits & Ask acc. & Decl. acc. & Decl./game \\", r"\midrule"]
    worst = 1.0
    for r in rows:
        lines.append(f"{label(r['b'])} & {pct(r['winRateA'])}\\% & {pct(r['ci'][0],2)}--{pct(r['ci'][1],2)} & "
                     f"{r['meanSetsA']:.3f} & {pct(r['askAccA'],2)}\\% & {pct(r['declAccA'],2)}\\% & {r['declPerGameA']:.2f} \\\\")
        worst = min(worst, r['winRateA'])
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'headtohead.tex'), 'w').write('\n'.join(lines))
    num('worst-case-winrate', pct(worst))
    num('h2h-games', f"{rows[0]['games']:,}".replace(',', '{,}'))
    for r in rows:
        if r['b'].split(':')[0] == 'v03':
            num('v04-vs-v03', pct(r['winRateA']))
            num('v04-vs-v03-ci', f"{pct(r['ci'][0],2)}--{pct(r['ci'][1],2)}")
            num('v04-vs-v03-sets', f"{r['meanSetsA']:.3f}")
            num('v04-vs-v03-declacc', pct(r['declAccA']))
            num('v04-vs-v03-oot', f"{r['outOfTurnA']:.2f}")
        if r['b'].split(':')[0] == 'lockout': num('v04-vs-lockout', pct(r['winRateA']))
        if r['b'].split(':')[0] == 'detective': num('v04-vs-detective', pct(r['winRateA']))
        if r['b'].split(':')[0] == 'v02': num('v04-vs-v02', pct(r['winRateA']))
    lim = max(r.get('limitHitRate', 0) for r in rows)
    num('limit-rate', f"{100*lim:.3f}\\%")
    for r in rows:
        if r['b'].split(':')[0] == 'v03':
            num('lock-hold-v04', f"{r.get('lockHoldA', 0):.2f}")
            num('lock-hold-v03', f"{r.get('lockHoldB', 0):.2f}")

# ---------------------------------------------------------------- E4 matrix + Bradley-Terry
mp = os.path.join(RES, 'E4-matrix.json')
if os.path.exists(mp):
    d = json.load(open(mp))
    ps = [p.split(':')[0] for p in d['policies']]
    idx = {p: i for i, p in enumerate(ps)}
    n = len(ps)
    W = [[0.0]*n for _ in range(n)]
    N = [[0.0]*n for _ in range(n)]
    for c in d['cells']:
        i, j = idx[c['a'].split(':')[0]], idx[c['b'].split(':')[0]]
        W[i][j] += c['winRateA'] * c['games']; N[i][j] += c['games']
        W[j][i] += (1 - c['winRateA']) * c['games']; N[j][i] += c['games']
    # Bradley-Terry by MM iteration
    p_ = [1.0]*n
    for _ in range(3000):
        newp = []
        for i in range(n):
            numer = sum(W[i][j] for j in range(n) if j != i)
            den = sum(N[i][j] / (p_[i] + p_[j]) for j in range(n) if j != i and N[i][j] > 0)
            newp.append(numer / den if den > 0 else p_[i])
        s = sum(newp) / n
        p_ = [x / s for x in newp]
    elo = [400 * math.log10(max(1e-9, x)) for x in p_]
    base = elo[idx.get('v03', 0)]
    order = sorted(range(n), key=lambda i: -elo[i])
    lines = [r"\begin{tabular}{lrr}", r"\toprule", r"Policy & Elo (v0.3 $=0$) & Mean win rate \\", r"\midrule"]
    for i in order:
        mw = sum(W[i][j] for j in range(n) if j != i) / max(1e-9, sum(N[i][j] for j in range(n) if j != i))
        lines.append(f"{PRETTY.get(ps[i], ps[i])} & {elo[i]-base:+.0f} & {pct(mw)}\\% \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'elo.tex'), 'w').write('\n'.join(lines))
    num('v04-elo', f"{elo[idx.get('v04',0)]-base:+.0f}")
    # full matrix
    ml = [r"\begin{tabular}{l" + "r"*n + "}", r"\toprule", "Row vs column & " + " & ".join(
        SHORT.get(p, p) for p in ps) + r" \\", r"\midrule"]
    for i in range(n):
        cells = []
        for j in range(n):
            cells.append("---" if i == j else f"{100*W[i][j]/max(1e-9,N[i][j]):.1f}")
        ml.append(PRETTY.get(ps[i], ps[i]) + " & " + " & ".join(cells) + r" \\")
    ml += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'matrix.tex'), 'w').write('\n'.join(ml))

# ---------------------------------------------------------------- E5 ablations
ap = os.path.join(RES, 'E5-ablations.json')
if os.path.exists(ap):
    txt = open(ap).read()
    try:
        d = json.loads(txt)
    except Exception:
        d = None
    if d:
        NAMES = {
            'belief=indep': ('Belief', 'Drop capacities (C4) and the certificates (C5) and the prior; uniform over the surviving support'),
            'belief=sinkhorn': ('Belief', 'Drop the certificates (C5) and the prior; Sinkhorn over C1--C4'),
            'belief=exact': ('Belief', 'Drop the certificates (C5) and the prior; exact capacity dynamic program over C1--C4'),
            'belief=block': ('Belief', r'\vblock{} belief substituted into the Fast-fitted policy (exact C1--C5, no prior)'),
            'belief=fast,ptheta=0,pphi=0': ('Belief', 'Fast belief with the policy prior off (matches the reference engine\'s prior-free belief)'),
            'ptheta=0,pphi=0': ('Belief', 'Remove the fitted policy-aware prior'),
            'ptheta=0.45': ('Belief', 'Force a v0.3-style soft ask-count prior'),
            'value=0': ('Ask', 'Remove the one-ply value lookahead'),
            'topk=0': ('Ask', 'Remove the two-ply refinement'),
            'w0=0': ('Ask', 'Zero the hit-probability weight'),
            'w5=0': ('Ask', 'Zero the lock-completion weight'),
            'w8=0': ('Ask', 'Zero the reply-threat weight'),
            'w18=0': ('Ask', 'Zero the runway weight'),
            'w9=0,w19=0': ('Ask', 'Zero both information-leak weights'),
            'vdecl=0': ('Declaration', 'Fixed threshold in place of the value-based stopping rule'),
            'patient=0': ('Declaration', 'Cash locked half-suits immediately (inert while the stopping rule is active)'),
            'decl=0.80': ('Declaration', 'Declaration threshold 0.80'),
            'decl=0.99': ('Declaration', 'Declaration threshold 0.99'),
            'gmap=1': ('Declaration', 'Name the allocation by conditioned greedy MAP'),
        }
        def entry(spec):
            tail = spec.split(',', 1)[-1]
            if tail in NAMES: return NAMES[tail], tail
            for k, v2 in NAMES.items():
                if k in spec: return v2, k
            return ('Other', spec), spec
        ORDER = {'Belief': 0, 'Ask': 1, 'Declaration': 2, 'Other': 3}
        rows2 = []
        for v2 in d['variants']:
            (grp, desc), tail = entry(v2['spec'])
            rows2.append((ORDER[grp], -v2['deltaFromRef'], grp, desc, tail, v2))
        rows2.sort()
        lines = [r"\begin{tabular}{@{}l>{\raggedright\arraybackslash}p{0.40\linewidth}rrr@{}}", r"\toprule",
                 r"Group & Change (policy weights frozen) & Win rate & Full $-$ ablated & 95\% paired CI \\",
                 r"\midrule",
                 f"& \\emph{{\\vfast{{}} (reference configuration)}} & {pct(d['winRate'])}\\% & --- & --- \\\\",
                 r"\midrule"]
        lastg = None
        for _, _, grp, desc, tail, v2 in rows2:
            if lastg is not None and grp != lastg: lines.append(r"\addlinespace")
            g = grp if grp != lastg else ''
            lastg = grp
            lines.append(f"{g} & {desc} & {pct(v2['winRate'])}\\% & {100*v2['deltaFromRef']:+.2f} & "
                         f"{100*v2['ci'][0]:+.2f}--{100*v2['ci'][1]:+.2f} \\\\")
        lines += [r"\bottomrule", r"\end{tabular}"]
        open(os.path.join(TAB, 'ablations.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E6 calibration
def parse_calib(path):
    if not os.path.exists(path): return None
    out = {}
    for line in open(path):
        m = re.match(r'(ask |decl) n=(\d+) brier=([\d.]+) logloss=([\d.]+) ece=([\d.]+) meanPred=([\d.]+) meanObs=([\d.]+)', line)
        if m:
            out[m.group(1).strip()] = dict(n=int(m.group(2)), brier=float(m.group(3)), logloss=float(m.group(4)),
                                           ece=float(m.group(5)), pred=float(m.group(6)), obs=float(m.group(7)), bins=[])
            cur = out[m.group(1).strip()]
        m2 = re.match(r'\s+\[([\d.]+),([\d.]+)\) n=\s*(\d+) pred=([\d.]+) obs=([\d.]+)', line)
        if m2 and out:
            cur['bins'].append((float(m2.group(1)), float(m2.group(2)), int(m2.group(3)), float(m2.group(4)), float(m2.group(5))))
    return out

c4 = parse_calib(os.path.join(RES, 'E6-calibration-v04.txt'))
c3 = parse_calib(os.path.join(RES, 'E6-calibration-v03.txt'))
if c4:
    lines = [r"\begin{tabular}{llrrrrr}", r"\toprule",
             r"Policy & Forecast & $n$ & Brier & Log loss & ECE & Mean pred / obs \\", r"\midrule"]
    for pol, cc in [(r'\vfast{}', c4), (r'\vthree{}', c3)]:
        if not cc: continue
        for k, lbl in [('ask', r'$\Pr[\text{ask succeeds}]$'), ('decl', r'$\Pr[\text{allocation correct}]$')]:
            if k in cc and cc[k]['n'] > 0:
                r = cc[k]
                lines.append(f"{pol} & {lbl} & {r['n']:,} & {r['brier']:.4f} & {r['logloss']:.4f} & {r['ece']:.4f} & {r['pred']:.4f} / {r['obs']:.4f} \\\\".replace(',', '{,}'))
        if pol == r'\vfast{}': lines.append(r"\midrule")
    lines += [r"\bottomrule", r"\end{tabular}"]
    if c3 and 'decl' in c3:
        num('v03-decl-ece', f"{c3['decl']['ece']:.4f}")
        num('v03-decl-pred', pct(c3['decl']['pred']))
        num('v03-decl-obs', pct(c3['decl']['obs']))
    open(os.path.join(TAB, 'calibration.tex'), 'w').write('\n'.join(lines))
    if 'decl' in c4:
        num('decl-ece', f"{c4['decl']['ece']:.4f}")
        num('decl-obs', pct(c4['decl']['obs']))
    if 'ask' in c4:
        num('ask-ece', f"{c4['ask']['ece']:.4f}")
        num('ask-brier', f"{c4['ask']['brier']:.4f}")
    rl = [r"\begin{tabular}{lrrrrrr}", r"\toprule",
          r"& \multicolumn{3}{c}{$\Pr[\text{ask succeeds}]$} & \multicolumn{3}{c}{$\Pr[\text{allocation correct}]$} \\",
          r"\cmidrule(lr){2-4}\cmidrule(lr){5-7}",
          r"Forecast bin & $n$ & forecast & observed & $n$ & forecast & observed \\", r"\midrule"]
    ab = {int(lo*10): (n2, pr, ob) for lo, hi, n2, pr, ob in c4.get('ask', {}).get('bins', [])}
    db = {int(lo*10): (n2, pr, ob) for lo, hi, n2, pr, ob in c4.get('decl', {}).get('bins', [])}
    for i in range(10):
        if i not in ab and i not in db: continue
        a1 = f"{ab[i][0]:,} & {ab[i][1]:.4f} & {ab[i][2]:.4f}".replace(',', '{,}') if i in ab else "--- & --- & ---"
        d1 = f"{db[i][0]:,} & {db[i][1]:.4f} & {db[i][2]:.4f}".replace(',', '{,}') if i in db else "--- & --- & ---"
        rl.append(f"$[{i/10:.1f}, {(i+1)/10:.1f})$ & {a1} & {d1} \\\\")
    rl += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'reliability.tex'), 'w').write('\n'.join(rl))

# ---------------------------------------------------------------- E7 rules
rows = readjsonl(os.path.join(RES, 'E7-rules.jsonl'))
if rows:
    lines = [r"\begin{tabular}{llrr}", r"\toprule", r"Rule dialect & Opponent & Win rate & 95\% CI \\", r"\midrule"]
    tags = ['v0.3 dialect (turn-only declarations)', '48-card, eight half-suits', 'No out-of-turn declarations']
    for i, r in enumerate(rows):
        lines.append(f"{tags[i%3]} & {label(r['b'])} & {pct(r['winRateA'])}\\% & {pct(r['ci'][0],2)}--{pct(r['ci'][1],2)} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'rules.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E10 LBR
rows = readjsonl(os.path.join(RES, 'E10-lbr.jsonl'))
if rows:
    NAME = {'v04': r'\vfast{}', 'v03': r'\vthree{}', 'detective': 'Posterior detective'}
    lines = [r"\begin{tabular}{lrrr}", r"\toprule",
             r"Frozen target & Response win rate & 95\% CI & Mean half-suits conceded \\", r"\midrule"]
    for r in rows:
        lines.append(f"{NAME.get(r.get('probe',''), r.get('probe',''))} & {pct(r['winRateA'])}\\% & "
                     f"{pct(r['ci'][0],2)}--{pct(r['ci'][1],2)} & {r['meanSetsA']:.3f} \\\\")
        if r.get('probe') == 'v04': num('lbr-v04', pct(r['winRateA']))
        if r.get('probe') == 'v03': num('lbr-v03', pct(r['winRateA']))
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'lbr.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E8 port
rows = readjsonl(os.path.join(RES, 'E8-v03-port.jsonl'))
if rows:
    PUB = {'lockout': 57.85, 'detective': 57.20, 'v02': 56.50, 'diversifier': 86.15,
           'hunter': 95.15, 'bluffer': 99.20, 'random': 100.00}
    lines = [r"\begin{tabular}{lrrr}", r"\toprule",
             r"Opponent & Published v0.3 & C++ port & 95\% CI \\", r"\midrule"]
    for r in rows:
        k = r['b'].split(':')[0]
        lines.append(f"{label(r['b'])} & {PUB.get(k, float('nan')):.2f}\\% & {pct(r['winRateA'])}\\% & "
                     f"{pct(r['ci'][0],2)}--{pct(r['ci'][1],2)} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'port.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- static counts
num('n-params', '34')
# ablation design, from engine/experiments.sh (--games=500 --rotations=2, four opponents)
num('abl-deals', '500')
num('abl-games', '1{,}000')
num('n-askfeats', '20')
num('n-valuefeats', '16')

# ---------------------------------------------------------------- E1 legacy
v = os.path.join(RES, 'E1-verify-legacy.txt')
if os.path.exists(v):
    m = re.search(r'audit violations: (\d+) / (\d+) checks', open(v).read())
    if m: num('audit-checks-legacy', f"{int(m.group(2)):,}".replace(',', '{,}'))

# ---------------------------------------------------------------- E3 extras
rows = readjsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
for r in rows:
    if r['b'].split(':')[0] == 'v03':
        num('h2h-deals', f"{r['deals']:,}".replace(',', '{,}'))
        num('v04-vs-v03-wins', f"{round(r['winRateA']*r['games']):,}".replace(',', '{,}'))
        num('v03-declacc-same', pct(r['declAccB']))
        num('events-per-game', f"{r['eventsPerGame']:.1f}")

# ---------------------------------------------------------------- E5 extras
ap = os.path.join(RES, 'E5-ablations.json')
if os.path.exists(ap):
    try: d = json.load(open(ap))
    except Exception: d = None
    if d:
        num('abl-ref', pct(d['winRate']))
        def ci(v2): return f"{100*v2['ci'][0]:+.2f}--{100*v2['ci'][1]:+.2f}"
        KEYS = {'belief=block': ('abl-block', 'abl-block-ci'),
                'ptheta=0,pphi=0': ('abl-prior', 'abl-prior-ci'),
                'vdecl=0': ('abl-stop', 'abl-stop-ci'),
                'belief=exact': ('abl-c5', 'abl-c5-ci'),
                'topk=0': ('abl-twoply', 'abl-twoply-ci'),
                'w5=0': ('abl-lockw', 'abl-lockw-ci')}
        for v2 in d['variants']:
            tail = v2['spec'].split(',', 1)[-1]
            for k, (nk, ck) in KEYS.items():
                if tail == k:
                    num(nk, f"{100*v2['deltaFromRef']:+.2f}"); num(ck, ci(v2))

# ---------------------------------------------------------------- E6 extras
def _calib_extra():
    c4 = parse_calib(os.path.join(RES, 'E6-calibration-v04.txt'))
    c3 = parse_calib(os.path.join(RES, 'E6-calibration-v03.txt'))
    if c4 and 'decl' in c4:
        r = c4['decl']
        num('decl-n', f"{r['n']:,}".replace(',', '{,}'))
        num('decl-brier', f"{r['brier']:.4f}"); num('decl-logloss', f"{r['logloss']:.4f}")
        top = [b for b in r['bins'] if b[0] >= 0.9]
        if top and r['n']: num('decl-topbin-share', f"{100.0*top[0][2]/r['n']:.1f}")
    if c4 and 'ask' in c4:
        num('ask-n', f"{c4['ask']['n']:,}".replace(',', '{,}'))
        num('ask-logloss', f"{c4['ask']['logloss']:.4f}")
    if c3 and 'decl' in c3:
        num('v03-decl-n', f"{c3['decl']['n']:,}".replace(',', '{,}'))
_calib_extra()

# ---------------------------------------------------------------- E9 throughput
tp = os.path.join(RES, 'E9-throughput.txt')
if os.path.exists(tp):
    t = open(tp).read()
    g = dict(re.findall(r'(v0\.\d[^\d]*?)\s+([\d.]+) games/s', t))
    vals = re.findall(r'([\d.]+) games/s over (\d+) games', t)
    if len(vals) >= 3:
        fast, block, v3 = float(vals[0][0]), float(vals[1][0]), float(vals[2][0])
        num('tput-fast', f"{fast:.1f}"); num('tput-block', f"{block:.1f}")
        num('tput-v03', f"{v3:,.0f}".replace(',', '{,}'))
        num('tput-ratio', f"{fast/block:.0f}")
        lines = [r"\begin{tabular}{lrr}", r"\toprule",
                 r"Configuration & Games/s & Games timed \\", r"\midrule",
                 f"\\vfast{{}} (deployed) & {fast:.1f} & {vals[0][1]} \\\\",
                 f"\\vblock{{}} (exact reference belief) & {block:.1f} & {vals[1][1]} \\\\",
                 f"\\vthree{{}} & {v3:,.0f} & {vals[2][1]} \\\\".replace(',', '{,}'),
                 r"\bottomrule", r"\end{tabular}"]
        open(os.path.join(TAB, 'throughput.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E10 extras
rows = readjsonl(os.path.join(RES, 'E10-lbr.jsonl'))
for r in rows:
    tag = r.get('probe', '')
    c = f"{pct(r['ci'][0],2)}--{pct(r['ci'][1],2)}"
    if tag == 'v04':
        num('lbr-v04-ci', c); num('lbr-games', f"{r['games']:,}".replace(',', '{,}'))
        num('lbr-limit', f"{100*r.get('limitHitRate',0):.3f}\\%")
    if tag == 'v03': num('lbr-v03-ci', c)
    if tag == 'detective': num('lbr-detective', pct(r['winRateA']))

# ---------------------------------------------------------------- E12
ep = os.path.join(RES, 'E12-exactness.txt')
if os.path.exists(ep):
    got = []
    for line in open(ep):
        m = re.search(r'(\{.*\})\s*$', line.strip())
        if m:
            try: got.append((line, json.loads(m.group(1))))
            except Exception: pass
    for line, d in got:
        key = 'e12-block' if 'belief=block' in line else 'e12-fast'
        num(key, pct(d['winRateA']))
        num(key + '-ci', f"{pct(d['ci'][0],2)}--{pct(d['ci'][1],2)}")

# ---------------------------------------------------------------- E14
vp = os.path.join(RES, 'E14-valuefit-stats.txt')
if os.path.exists(vp):
    m = re.search(r'rows=(\d+)\s+R2=([\d.]+)\s+rmse=([\d.]+)', open(vp).read())
    if m:
        num('value-rows', f"{int(m.group(1)):,}".replace(',', '{,}'))
        num('value-r2', m.group(2)); num('value-rmse', m.group(3))

# ---------------------------------------------------------------- E15 oracle
op = os.path.join(RES, 'E15-oracle.txt')
if os.path.exists(op):
    t = open(op).read()
    def grab(pat, default=None):
        m = re.search(pat, t); return m.group(1) if m else default
    def big(x): return f"{int(x):,}".replace(',', '{,}') if x is not None else None
    g = grab(r'games replayed\s+(\d+)');            g and num('oracle-games', g)
    a = grab(r'states enumerated\s+(\d+)');          a and num('oracle-states', big(a))
    b = grab(r'live C5 certificate: (\d+)\)');       b and num('oracle-states-c5', big(b))
    c = grab(r'states skipped \(too large\)\s+(\d+)'); c and num('oracle-skipped', big(c))
    d2 = grab(r'consistent deals counted\s+(\d+)');  d2 and num('oracle-deals', big(d2))
    e = grab(r'per-card marginals.*over (\d+) checks'); e and num('oracle-marg-checks', big(e))
    f2 = grab(r'team-ownership prob.*over (\d+) checks'); f2 and num('oracle-team-checks', big(f2))
    g2 = grab(r'named allocation prob.*over (\d+) checks'); g2 and num('oracle-alloc-checks', big(g2))
    h = grab(r'bestTeamAllocation\s+(\d+) checks');  h and num('oracle-best-checks', big(h))
    i2 = grab(r'checks, (\d+) inconsistent');        i2 and num('oracle-best-bad', i2)
    j = grab(r'sampler vs exact marginals max abs diff ([\d.]+)'); j and num('oracle-sample-diff', j)
    k2 = grab(r'over (\d+) draws');                  k2 and num('oracle-draws', big(k2))
    rowsx = [
        (r'partition function $Z$', 'Z max relative difference', grab(r'partition function Z\s+max rel diff (\S+)'), None),
        (r'per-card marginals $\mu_{c,p}$', 'max absolute difference', grab(r'per-card marginals\s+max abs diff (\S+)'), e),
        (r'team ownership $\tau(H,T)$', 'max absolute difference', grab(r'team-ownership prob\s+max abs diff (\S+)'), f2),
        (r'named allocation $\alpha(A)$', 'max absolute difference', grab(r'named allocation prob\s+max abs diff (\S+)'), g2),
        (r'equal-probability corollary', 'max within-class spread', grab(r'equal-prob corollary\s+max within-class spread (\S+)'), None),
        (r'sampler frequencies', 'max absolute difference', grab(r'sampler vs exact marginals max abs diff (\S+)'), k2),
    ]
    def sci(x):
        if x is None: return '---'
        m = re.match(r'([\d.]+)e([+-])0*(\d+)$', x)
        if not m: return x
        if float(m.group(1)) == 0: return '$0$'
        return f"${m.group(1)}\\times 10^{{{'-' if m.group(2)=='-' else ''}{m.group(3)}}}$"
    lines = [r"\begin{tabular}{llrr}", r"\toprule",
             r"Quantity & Comparison & Value & Checks \\", r"\midrule"]
    for q, cmp_, val, n_ in rowsx:
        lines.append(f"{q} & {cmp_} & {sci(val)} & {big(n_) if n_ else '---'} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'oracle.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E16 gate audit
gp = os.path.join(RES, 'E16-gateaudit.txt')
if os.path.exists(gp):
    t = open(gp).read()
    def gi(pat):
        m = re.search(pat, t); return int(m.group(1)) if m else None
    def big(x): return f"{int(x):,}".replace(',', '{,}')
    opps = gi(r'declaration opportunities\s+(\d+)')
    seen = gi(r'\(opportunity, half-suit\)\s+(\d+)')
    rej  = gi(r'rejected by a cheap gate\s+(\d+)')
    fn   = gi(r'false negatives\s+(\d+)')
    dg   = gi(r'declarations, gated\s+(\d+)')
    du   = gi(r'declarations, ungated\s+(\d+)')
    ch   = gi(r'chosen action differs\s+(\d+)')
    if None not in (opps, seen, rej, fn, dg, du, ch):
        num('gate-opps', big(opps)); num('gate-seen', big(seen))
        num('gate-rejected', big(rej)); num('gate-rejected-pct', f"{100*rej/seen:.2f}")
        num('gate-falseneg', big(fn)); num('gate-falseneg-pct', f"{100*fn/rej:.5f}")
        num('gate-actions', big(ch)); num('gate-actions-pct', f"{100*ch/opps:.4f}")
        num('gate-decl-gated', big(dg)); num('gate-decl-ungated', big(du))
        num('gate-decl-lost-pct', f"{100*(du-dg)/du:.2f}")
        lines = [r"\begin{tabular}{lrr}", r"\toprule",
                 r"Quantity & Count & Share \\", r"\midrule",
                 f"Declaration opportunities examined & {big(opps)} & --- \\\\",
                 f"(opportunity, live half-suit) pairs & {big(seen)} & --- \\\\",
                 f"Pairs rejected by a cheap gate & {big(rej)} & {100*rej/seen:.2f}\\% of pairs \\\\",
                 r"\midrule",
                 f"False negatives (rejected, would have been declared) & {big(fn)} & {100*fn/rej:.5f}\\% of rejections \\\\",
                 f"Declarations made, gated path & {big(dg)} & --- \\\\",
                 f"Declarations made, ungated path & {big(du)} & --- \\\\",
                 f"Opportunities where the chosen action differs & {big(ch)} & {100*ch/opps:.4f}\\% of opportunities \\\\",
                 r"\bottomrule", r"\end{tabular}"]
        open(os.path.join(TAB, 'gateaudit.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E17 arbitration
rows = readjsonl(os.path.join(RES, 'E17-arbitration.jsonl'))
if rows:
    ARB = {'low': 'Lowest seat index (default)', 'high': 'Highest seat index',
           'turn': 'Scan from the current turn-holder'}
    lines = [r"\begin{tabular}{llrrr}", r"\toprule",
             r"Arbitration order & Opponent & Win rate & 95\% CI & Out-of-turn decl./game \\",
             r"\midrule"]
    last = None
    for r in rows:
        a = r.get('arb', '')
        tag = ARB.get(a, a) if a != last else ''
        last = a
        lines.append(f"{tag} & {label(r['b'])} & {pct(r['winRateA'])}\\% & "
                     f"{pct(r['ci'][0],2)}--{pct(r['ci'][1],2)} & {r['outOfTurnA']:.2f} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'arbitration.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E11 termination
tp2 = os.path.join(RES, 'E11-termination.md')
if os.path.exists(tp2):
    num('mirror-cycle', '21.0')
    num('deadask-cost', '28.3')

# ---------------------------------------------------------------- reliability figure
def _reliability_figure():
    c4 = parse_calib(os.path.join(RES, 'E6-calibration-v04.txt'))
    c3 = parse_calib(os.path.join(RES, 'E6-calibration-v03.txt'))
    if not c4: return
    FIG = os.path.join(ROOT, 'paper', 'figures')
    os.makedirs(FIG, exist_ok=True)
    S = 5.0   # panel side, cm

    def panel(bins, title, note):
        L = [r"\begin{scope}"]
        L.append(rf"\draw[fishgray!35] (0,0) grid[step={S/10:.4f}] ({S:.3f},{S:.3f});")
        L.append(rf"\draw[fishgray!70,thick] (0,0) rectangle ({S:.3f},{S:.3f});")
        L.append(rf"\draw[fishgray,dashed] (0,0) -- ({S:.3f},{S:.3f});")
        for t in (0, 0.5, 1.0):
            L.append(rf"\node[below,font=\scriptsize,text=fishgray] at ({t*S:.3f},0) {{{t:.1f}}};")
            L.append(rf"\node[left,font=\scriptsize,text=fishgray] at (0,{t*S:.3f}) {{{t:.1f}}};")
        L.append(rf"\node[below,font=\scriptsize,text=fishgray] at ({S/2:.3f},-0.42) {{forecast}};")
        L.append(rf"\node[rotate=90,above,font=\scriptsize,text=fishgray] at (-0.45,{S/2:.3f}) {{observed}};")
        L.append(rf"\node[above,font=\small\bfseries,text=fishgreen] at ({S/2:.3f},{S+0.10:.3f}) {{{title}}};")
        pts = []
        for lo, hi, n, pr, ob in bins:
            if n <= 0: continue
            r = 0.055 + 0.115 * (min(1.0, (n ** 0.5) / 130.0) ** 0.6)
            L.append(rf"\fill[fishgreen,opacity=0.80] ({pr*S:.4f},{ob*S:.4f}) circle ({r:.4f});")
            pts.append((pr * S, ob * S))
        if len(pts) > 1:
            L.append(r"\draw[fishgreen,thick,opacity=0.55] " +
                     " -- ".join(f"({x:.4f},{y:.4f})" for x, y in pts) + ";")
        L.append(rf"\node[below right,font=\scriptsize,text=fishgray,align=left] at (0.12,{S-0.12:.3f}) {{{note}}};")
        L.append(r"\end{scope}")
        return L

    out = [r"% Generated by engine/build_tables.py -- do not edit by hand.",
           r"\begin{tikzpicture}"]
    if 'ask' in c4 and c4['ask']['n']:
        a = c4['ask']
        note = (rf"$n={a['n']:,}$\\ECE ${a['ece']:.4f}$").replace(',', '{,}')
        out += panel(a['bins'], r"v0.4-Fast: $\Pr[\text{ask succeeds}]$", note)
    out.append(rf"\begin{{scope}}[xshift={S+1.6:.3f}cm]")
    if 'decl' in c4 and c4['decl']['n']:
        d = c4['decl']
        note = (rf"$n={d['n']:,}$\\ECE ${d['ece']:.4f}$").replace(',', '{,}')
        out += panel(d['bins'], r"v0.4-Fast: $\Pr[\text{allocation correct}]$", note)
    out.append(r"\end{scope}")
    out.append(rf"\begin{{scope}}[xshift={2*(S+1.6):.3f}cm]")
    if c3 and 'decl' in c3 and c3['decl']['n']:
        d3 = c3['decl']
        note = (rf"$n={d3['n']:,}$\\ECE ${d3['ece']:.4f}$").replace(',', '{,}')
        out += panel(d3['bins'], r"v0.3: $\Pr[\text{allocation correct}]$", note)
    out.append(r"\end{scope}")
    out.append(r"\end{tikzpicture}")
    open(os.path.join(FIG, 'reliability.tex'), 'w').write('\n'.join(out) + '\n')
_reliability_figure()

# ---------------------------------------------------------------- design constants
# Taken from engine/experiments.sh, engine/exploitability.sh and engine/src/tuner.hpp.
num('n-knobs', '14'); num('cem-pop', '24'); num('cem-elite', '6')
num('bootstrap-draws', '20{,}000')
num('selftest-games', '40')
num('port-deals', '1{,}000');   num('port-games', '2{,}000')
num('matrix-deals', '200');     num('matrix-games-pair', '1{,}200'); num('matrix-games-cell', '2{,}400')
num('calib-deals', '600');      num('lbr-deals', '600')
num('e7-deals', '400');         num('e7-games', '2{,}400')
num('e12-deals', '400');        num('e12-games', '2{,}400')
num('e13-deals', '300');        num('e17-deals', '700')
num('oracle-maxdeals', '200{,}000')
num('v03-worst-published', '56.50')
num('deadask-cycle', '16.7')

# ---------------------------------------------------------------- E2 check count
b = os.path.join(RES, 'E2-belief-selftest.txt')
if os.path.exists(b):
    m = re.search(r'checks\s+(\d+)', open(b).read())
    if m: num('selftest-checks', f"{int(m.group(1)):,}".replace(',', '{,}'))

# ---------------------------------------------------------------- E3 extras (2)
rows = readjsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
for r in rows:
    k = r['b'].split(':')[0]
    if k == 'v03':     num('v04-vs-v03-askacc', pct(r['askAccA']))
    if k == 'lockout': num('v04-vs-lockout-askacc', pct(r['askAccA']))
    for k2, mk in [('diversifier', 'v04-vs-diversifier'), ('hunter', 'v04-vs-hunter'),
                   ('bluffer', 'v04-vs-bluffer'), ('random', 'v04-vs-random')]:
        if k == k2: num(mk, pct(r['winRateA']))

# ---------------------------------------------------------------- E5 full row set
ap = os.path.join(RES, 'E5-ablations.json')
if os.path.exists(ap):
    try: d = json.load(open(ap))
    except Exception: d = None
    if d:
        MORE = {
            'belief=exact':   ('abl-c5', 'abl-c5-ci', 'abl-c5-abs'),
            'belief=indep':   ('abl-c4', 'abl-c4-ci', 'abl-c4-abs'),
            'belief=sinkhorn':('abl-sinkhorn', 'abl-sinkhorn-ci', 'abl-sinkhorn-abs'),
            'belief=block':   ('abl-block', 'abl-block-ci', 'abl-block-abs'),
            'ptheta=0,pphi=0':('abl-prior', 'abl-prior-ci', 'abl-prior-abs'),
            'w5=0':           ('abl-lockw', 'abl-lockw-ci', 'abl-lockw-abs'),
            'gmap=1':         ('abl-gmap', 'abl-gmap-ci', None),
            'w8=0':           ('abl-threat', 'abl-threat-ci', None),
            'w9=0,w19=0':     ('abl-leak', 'abl-leak-ci', None),
            'value=0':        ('abl-value', 'abl-value-ci', None),
            'w18=0':          ('abl-runway', 'abl-runway-ci', None),
            'w0=0':           ('abl-hitw', 'abl-hitw-ci', None),
            'decl=0.80':      ('abl-decl80', 'abl-decl80-ci', None),
            'decl=0.99':      ('abl-decl99', 'abl-decl99-ci', None),
        }
        for v2 in d['variants']:
            tail = v2['spec'].split(',', 1)[-1]
            if tail in MORE:
                dk, ck, ak = MORE[tail]
                num(dk, f"{100*v2['deltaFromRef']:+.2f}")
                num(ck, f"{100*v2['ci'][0]:+.2f}--{100*v2['ci'][1]:+.2f}")
                if ak: num(ak, pct(v2['winRate']))

# ---------------------------------------------------------------- E4 group ratings
mp = os.path.join(RES, 'E4-matrix.json')
if os.path.exists(mp) and os.path.exists(os.path.join(TAB, 'elo.tex')):
    t = open(os.path.join(TAB, 'elo.tex')).read()
    for key, tag in [('elo-lockout', 'Turn-starvation lockout'), ('elo-detective', 'Posterior detective')]:
        m = re.search(re.escape(tag) + r' & ([+-]\d+)', t)
        if m: num(key, m.group(1))

# ---------------------------------------------------------------- E7 cells
rows = readjsonl(os.path.join(RES, 'E7-rules.jsonl'))
if len(rows) >= 9:
    OPPS = ['v03', 'lockout', 'detective']
    TAGS = ['legacy', 'eight', 'nooot']
    for oi, opp in enumerate(OPPS):
        for ti, tag in enumerate(TAGS):
            r = rows[oi * 3 + ti]
            num(f'e7-{opp}-{tag}', pct(r['winRateA']))
            if opp == 'v03':
                num(f'e7-{opp}-{tag}-ci', f"{pct(r['ci'][0],2)}--{pct(r['ci'][1],2)}")

# ---------------------------------------------------------------- E17 cells
rows = readjsonl(os.path.join(RES, 'E17-arbitration.jsonl'))
if rows:
    by = {(r.get('arb'), r['b'].split(':')[0]): r for r in rows}
    for arb in ('high', 'turn'):
        for opp in ('v03', 'lockout', 'detective'):
            r = by.get((arb, opp))
            if r: num(f'arb-{arb}-{opp}', pct(r['winRateA']))
    base = {opp: by.get(('low', opp)) for opp in ('v03', 'lockout', 'detective')}
    moves = []
    for (arb, opp), r in by.items():
        b0 = base.get(opp)
        if b0 and arb != 'low': moves.append(abs(r['winRateA'] - b0['winRateA']) * 100)
    if moves: num('arb-max-move', f"{max(moves):.1f}")
    seat = [r['outOfTurnA'] for (a, o), r in by.items() if a in ('low', 'high')]
    turn = [r['outOfTurnA'] for (a, o), r in by.items() if a == 'turn']
    if seat: num('arb-oot-seat-lo', f"{min(seat):.1f}"); num('arb-oot-seat-hi', f"{max(seat):.1f}")
    if turn: num('arb-oot-turn-lo', f"{min(turn):.2f}"); num('arb-oot-turn-hi', f"{max(turn):.2f}")

# ---------------------------------------------------------------- selection record
selp2 = os.path.join(ROOT, 'research', 'v04', 'runs', 'selected.json')
if os.path.exists(selp2):
    sel2 = json.load(open(selp2))
    num('select-softmin', f"{sel2['softmin']:.4f}")
    num('select-peropp', ', '.join(f"{x:.3f}" for x in sel2['winRates']))

flush_numbers()

# ------------------------------------------------------- markdown findings
md = ['# FishLab v0.4 results', '',
      'Generated by `engine/build_tables.py` from the artifacts in',
      '`research/v04/results/`. Every bank below is disjoint from the ones used',
      'for fitting, and the configuration was frozen before any of them ran.', '']
h2h = readjsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
if h2h:
    md += ['## Held-out head-to-head (six-rotation duplicate blocks)', '',
           '| Opponent | Win rate | 95% CI | Mean sets | Ask acc. | Decl. acc. | Decl./game | Out-of-turn |',
           '|---|---:|---:|---:|---:|---:|---:|---:|']
    for r in h2h:
        md.append(f"| {label(r['b'])} | {pct(r['winRateA'])}% | {pct(r['ci'][0],1)}–{pct(r['ci'][1],1)} | "
                  f"{r['meanSetsA']:.3f} | {pct(r['askAccA'],1)}% | {pct(r['declAccA'],1)}% | "
                  f"{r['declPerGameA']:.2f} | {r['outOfTurnA']:.2f} |")
    md += ['', f"Worst case across the population: **{_NUMS.get('worst-case-winrate','?')}%**.", '']
abl = os.path.join(RES, 'E5-ablations.json')
if os.path.exists(abl):
    try: d2 = json.load(open(abl))
    except Exception: d2 = None
    if d2:
        md += ['## Paired ablations', '', '| Change | Win rate | Full − ablated | 95% paired CI |', '|---|---:|---:|---:|',
               f"| _reference_ | {pct(d2['winRate'])}% | — | — |"]
        for v2 in sorted(d2['variants'], key=lambda x: -x['deltaFromRef']):
            spec = v2['spec'].split(',', 1)[-1]
            md.append(f"| {spec} | {pct(v2['winRate'])}% | {100*v2['deltaFromRef']:+.2f} | "
                      f"{100*v2['ci'][0]:+.2f}–{100*v2['ci'][1]:+.2f} |")
        md.append('')
port = readjsonl(os.path.join(RES, 'E8-v03-port.jsonl'))
if port:
    PUB = {'lockout': 57.85, 'detective': 57.20, 'v02': 56.50, 'diversifier': 86.15,
           'hunter': 95.15, 'bluffer': 99.20, 'random': 100.00}
    md += ['## FishBot v0.3 port validation (legacy dialect)', '',
           '| Opponent | Published | C++ port | 95% CI |', '|---|---:|---:|---:|']
    for r in port:
        k = r['b'].split(':')[0]
        md.append(f"| {label(r['b'])} | {PUB.get(k,0):.2f}% | {pct(r['winRateA'])}% | "
                  f"{pct(r['ci'][0],1)}–{pct(r['ci'][1],1)} |")
    md.append('')
open(os.path.join(ROOT, 'docs', 'V04_RESULTS.md'), 'w').write(chr(10).join(md) + chr(10))
print("tables, numbers and docs/V04_RESULTS.md written")
