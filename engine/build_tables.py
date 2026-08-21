#!/usr/bin/env python3
"""Turn the v0.4 experiment artifacts into LaTeX tables and inline numbers."""
import json, math, os, re, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
RES = os.path.join(ROOT, 'research', 'v04', 'results')
TAB = os.path.join(ROOT, 'paper', 'tables')
NUM = os.path.join(ROOT, 'paper', 'numbers')
os.makedirs(TAB, exist_ok=True); os.makedirs(NUM, exist_ok=True)

PRETTY = {
    'v04': 'FishBot v0.4', 'v03': 'FishBot v0.3', 'v02': 'FishBot v0.2', 'lockout': 'Turn-starvation lockout',
    'detective': 'Posterior detective', 'diversifier': 'Adaptive diversifier',
    'hunter': 'Focused hunter', 'bluffer': 'Misdirection artist', 'random': 'Random legal control',
}
SHORT = {'v04': 'v0.4', 'v03': 'v0.3', 'v02': 'v0.2', 'lockout': 'Lock', 'detective': 'Infer',
         'diversifier': 'Divers', 'hunter': 'Focus', 'bluffer': 'Bluff', 'random': 'Rand'}

def label(spec):
    base = spec.split(':')[0]
    return PRETTY.get(base, base)

MACRO = {'audit-checks': 'numAuditChecks', 'audit-violations': 'numAuditViolations', 'belief-vs-carddp': 'numBeliefVsCardDP', 'belief-vs-sampling': 'numBeliefVsSampling', 'sinkhorn-mean-err': 'numSinkhornMean', 'sinkhorn-max-err': 'numSinkhornMax', 'worst-case-winrate': 'numWorstCase', 'h2h-games': 'numHtHGames', 'v04-vs-v03': 'numVsVthree', 'v04-vs-v03-ci': 'numVsVthreeCI', 'v04-vs-v03-sets': 'numVsVthreeSets', 'v04-vs-v03-declacc': 'numVsVthreeDeclAcc', 'v04-vs-v03-oot': 'numVsVthreeOOT', 'v04-vs-lockout': 'numVsLockout', 'v04-vs-detective': 'numVsDetective', 'v04-vs-v02': 'numVsVtwo', 'limit-rate': 'numLimitRate', 'v04-elo': 'numElo', 'decl-ece': 'numDeclECE', 'decl-obs': 'numDeclObs', 'ask-ece': 'numAskECE', 'ask-brier': 'numAskBrier', 'lock-hold-v04': 'numLockHoldFour', 'lock-hold-v03': 'numLockHoldThree', 'lbr-v04': 'numLBRFour', 'lbr-v03': 'numLBRThree', 'selected-gen': 'numSelectedGen', 'v03-decl-ece': 'numVthreeDeclECE', 'v03-decl-pred': 'numVthreeDeclPred', 'v03-decl-obs': 'numVthreeDeclObs'}
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
    FEAT = ['hit probability', 'squared hit', 'certain hit', 'set progress', 'team control',
            'lock completion', 'continuation', 'completion', 'reply threat',
            'information leak', 'target hand size', 'empties target', 'repeats set',
            'known team cards', 'entropy', 'team owns set', 'exposure on miss',
            'trailing pressure', 'runway', 'leak magnitude']
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
             r"Opponent & Win rate & 95\% CI & Mean sets & Ask acc. & Decl. acc. & Decl./game \\", r"\midrule"]
    worst = 1.0
    for r in rows:
        lines.append(f"{label(r['b'])} & {pct(r['winRateA'])}\\% & {pct(r['ci'][0],1)}--{pct(r['ci'][1],1)} & "
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
            num('lock-hold-v04', f"{r.get('lockHoldA', 0):.1f}")
            num('lock-hold-v03', f"{r.get('lockHoldB', 0):.1f}")

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
            'belief=block': 'Exact block posterior instead of the fast one',
            'belief=fast,ptheta=0,pphi=0': 'Fast posterior with the policy prior off (matches block)',
            'belief=exact': 'Drop ask-legality certificates (C5)',
            'belief=sinkhorn': 'Drop certificates and use plain Sinkhorn',
            'belief=indep': 'Drop capacity conditioning as well (C4)',
            'value=0': 'Remove the one-ply value lookahead',
            'vdecl=0': 'Threshold declarations instead of optimal stopping',
            'patient=0': 'Cash locked half-suits immediately',
            'ptheta=0.45': 'Force a v0.3-style soft ask-count prior',
            'w0=0': 'Zero the hit-probability weight',
            'w5=0': 'Zero the lock-completion weight',
            'w8=0': 'Zero the reply-threat weight',
            'w18=0': 'Zero the runway weight',
            'w9=0,w19=0': 'Zero both information-leak weights',
            'decl=0.80': 'Declaration threshold 0.80',
            'decl=0.99': 'Declaration threshold 0.99',
            'topk=0': 'Remove the exact two-ply refinement',
            'gmap=1': 'Choose the allocation by conditioned greedy MAP',
            'ptheta=0,pphi=0': 'Remove the fitted soft policy prior',
        }
        def name(spec):
            for k, v2 in NAMES.items():
                if k in spec: return v2
            return spec
        lines = [r"\begin{tabular}{lrrr}", r"\toprule",
                 r"Change & Win rate & Full $-$ ablated & 95\% paired CI \\", r"\midrule",
                 f"\\emph{{FishBot v0.4 (reference)}} & {pct(d['winRate'])}\\% & --- & --- \\\\"]
        for v2 in sorted(d['variants'], key=lambda x: -x['deltaFromRef']):
            lines.append(f"{name(v2['spec'])} & {pct(v2['winRate'])}\\% & {100*v2['deltaFromRef']:+.2f} & "
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
    for pol, cc in [('FishBot v0.4', c4), ('FishBot v0.3', c3)]:
        if not cc: continue
        for k, lbl in [('ask', r'$\Pr[\text{ask succeeds}]$'), ('decl', r'$\Pr[\text{allocation correct}]$')]:
            if k in cc and cc[k]['n'] > 0:
                r = cc[k]
                lines.append(f"{pol} & {lbl} & {r['n']:,} & {r['brier']:.4f} & {r['logloss']:.4f} & {r['ece']:.4f} & {r['pred']:.4f} / {r['obs']:.4f} \\\\".replace(',', '{,}'))
        if pol == 'FishBot v0.4': lines.append(r"\midrule")
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
        lines.append(f"{tags[i%3]} & {label(r['b'])} & {pct(r['winRateA'])}\\% & {pct(r['ci'][0],1)}--{pct(r['ci'][1],1)} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'rules.tex'), 'w').write('\n'.join(lines))

# ---------------------------------------------------------------- E10 LBR
rows = readjsonl(os.path.join(RES, 'E10-lbr.jsonl'))
if rows:
    NAME = {'v04': 'FishBot v0.4', 'v03': 'FishBot v0.3', 'detective': 'Posterior detective'}
    lines = [r"\begin{tabular}{lrrr}", r"\toprule",
             r"Frozen policy & Exploiter win rate & 95\% CI & Mean sets conceded \\", r"\midrule"]
    for r in rows:
        lines.append(f"{NAME.get(r.get('probe',''), r.get('probe',''))} & {pct(r['winRateA'])}\\% & "
                     f"{pct(r['ci'][0],1)}--{pct(r['ci'][1],1)} & {r['meanSetsA']:.3f} \\\\")
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
                     f"{pct(r['ci'][0],1)}--{pct(r['ci'][1],1)} \\\\")
    lines += [r"\bottomrule", r"\end{tabular}"]
    open(os.path.join(TAB, 'port.tex'), 'w').write('\n'.join(lines))

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
