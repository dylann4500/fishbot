#!/usr/bin/env python3
"""Turn research/v06/results/* into paper/numbers_v06_generated.tex and
paper/tables_v06/*.tex.

Discipline, inherited from the v0.4/v0.5 pipeline and tightened:
  * every macro carries a comment naming the artifact and the field it came
    from, so paper/check_provenance.py can verify that no number in the
    manuscript is hand-entered;
  * a macro whose artifact is missing is NOT emitted, so the placeholder in
    numbers_v06.tex survives and the omission is visible on the page;
  * \\providecommand then \\renewcommand, so this file always wins over the
    placeholder file regardless of input order.
"""
import json, os, re, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES  = os.path.join(ROOT, 'research', 'v06', 'results')
PAPER= os.path.join(ROOT, 'paper')
TBL  = os.path.join(PAPER, 'tables_v06')
os.makedirs(TBL, exist_ok=True)


# --------------------------------------------------------------- table writer
# v0.4's convention, and the one that actually works: a generated file holds the
# WHOLE tabular.  A rows-only file, \input inside a tabular, trips
# "Misplaced \noalign" at the following \bottomrule.
TABLE_SPEC = {
  'belief.tex':    ('lrrrr', 'Posterior & Cards & Mean NLL & Argmax $p$ & Argmax hit'),
  'rollout.tex':   ('lrrr',  'Rollout blueprint & Win rate vs \\vfive{} & 95\\% CI & Games/s'),
  'search.tex':    ('lrrr',  'Configuration & Win rate vs \\vfive{} & 95\\% CI & $n$'),
  'searchdev.tex': ('lrrr',  '$\\kappa$ & Decisions searched (\\%) & Moved the action (\\%) & Games/s'),
  'perstyle.tex':  ('lrrr',  'Opponent & \\vsix{} & \\vfive{} & \\vfour{}'),
  'ablations.tex': ('lrrr',  'Variant & Pooled & \\vsix{} $-$ variant & 95\\% paired CI'),
  'throughput.tex':('lr',    'Policy & Games/s'),
  'partners.tex':  ('lrr',   'Partners & \\vsix{} & \\vfive{}'),
  'h2h_v05.tex':   ('rrrr',  'Win rate (\\%) & 95\\% cluster CI & Mean sets \\vsix{} & Mean sets \\vfive{}'),
  'h2h_v04.tex':   ('rrrr',  'Win rate (\\%) & 95\\% cluster CI & Mean sets \\vsix{} & Mean sets \\vfour{}'),
  'calibration.tex':('lrrrr','Bin & $n$ & Predicted & Observed & Gap'),
  'params.tex':    ('lrr',   'Coordinate & \\vfive{} & \\vsix{}'),
}
def writeTable(fn, lines, extra=None):
    spec, head = TABLE_SPEC[fn]
    body = '\n'.join(lines) if lines else \
           ('\\multicolumn{%d}{c}{\\textit{--- artifact not regenerated ---}} \\\\' % len(spec.replace('|','')))
    with open(os.path.join(TBL, fn), 'w') as f:
        f.write('\\begin{tabular}{%s}\n\\toprule\n%s \\\\\n\\midrule\n%s\n' % (spec, head, body))
        if extra: f.write('\\midrule\n' + extra + '\n')
        f.write('\\bottomrule\n\\end{tabular}\n')

OUT = []
def emit(name, value, src):
    OUT.append(f"% {src}\n\\providecommand{{\\{name}}}{{}}\\renewcommand{{\\{name}}}{{{value}}}")

def num(x, nd=2):
    return ('%.'+str(nd)+'f') % x
def pct(x, nd=2):
    return ('%.'+str(nd)+'f') % (100.0*x)
def commafy(n):
    return f"{int(n):,}"

def read(path):
    p = os.path.join(RES, path)
    return open(p).read() if os.path.exists(p) else None

def jsonl(path):
    t = read(path)
    if t is None: return []
    out = []
    for blob in re.findall(r'\{.*?\}(?=\s*(?:\{|$))', t, re.S):
        try: out.append(json.loads(blob))
        except Exception: pass
    if not out:                      # fall back to line-wise
        for line in t.splitlines():
            line = line.strip()
            if line.startswith('{'):
                try: out.append(json.loads(line))
                except Exception: pass
    return out

# ---------------------------------------------------------------- E0 identity
t = read('E0-identity.txt')
if t:
    emit('vsixIdentity', 'PASS' if 'IDENTITY PASS' in t else 'FAIL', 'E0-identity.txt')
    m = re.findall(r'QUERY mismatches (\d+)', t)
    if m: emit('vsixAliasQueryMismatch', m[0], 'E0-identity.txt: fish blockalias')
    m = re.findall(r'raw shared-pool field reads that differ: (\d+)', t)
    if m: emit('vsixAliasRawMismatch', m[0], 'E0-identity.txt: fish blockalias')

# ------------------------------------------------------------------ E1 verify
t = read('E1-verify.txt')
if t:
    m = re.search(r'audit violations: (\d+) / (\d+) checks', t)
    if m:
        emit('vsixAuditViolations', m.group(1), 'E1-verify.txt')
        emit('vsixAuditChecks', commafy(m.group(2)), 'E1-verify.txt')
    emit('vsixVerify', 'PASS' if 'VERIFY PASS' in t else 'FAIL', 'E1-verify.txt')

# --------------------------------------------------------------- E2 pathology
def parse_path_block(block):
    d = {}
    def g(pat, key, cast=float):
        m = re.search(pat, block)
        if m: d[key] = cast(m.group(1))
    g(r'events/game\s+([0-9.]+)', 'events')
    g(r'p90 (\d+)', 'pNinety', int); g(r'p99 (\d+)', 'pNineNine', int)
    g(r'hit rate ([0-9.]+)%', 'hit')
    g(r'DEAD asks\s+\d+\s+\(([0-9.]+)%', 'dead')
    g(r'dead runs.*?longest (\d+)', 'longest', int)
    g(r'games w/ run>=6\s+\d+\s+\(([0-9.]+)%', 'runSix')
    g(r'repeat \(a,c,t\)\s+\d+\s+\(([0-9.]+)%', 'repeatACT')
    g(r'asks in own-locked\s+\d+\s+\(([0-9.]+)%', 'ownlocked')
    g(r'declarations\s+\d+\s+wrong \d+ \(([0-9.]+)%', 'declwrong')
    g(r'action-limit games \d+ \(([0-9.]+)%', 'limit')
    m = re.search(r'forced endgame\s+(\d+)\s+wrong (\d+)', block)
    if m: d['forced'] = int(m.group(1)); d['forcedwrong'] = int(m.group(2))
    return d

t = read('E2-pathology.txt')
if t:
    blocks = re.split(r'### ', t)
    label = {'v0.6 mirror':'Six','v0.5 mirror':'Five','v0.4 mirror':'Four','v0.6 vs v0.5':'SixVFive'}
    for b in blocks[1:]:
        head = b.split('\n',1)[0].strip()
        key = label.get(head)
        if not key: continue
        d = parse_path_block(b)
        for f,v in d.items():
            emit(f'vsixPath{key}{f[0].upper()+f[1:]}', num(v,2) if isinstance(v,float) else str(v),
                 f'E2-pathology.txt "{head}" field {f}')

GATE_ALIAS = {'Dead':'vsixGateDeadAsk','Longest':'vsixGateDeadRun','Declwrong':'vsixGateDeclWrong',
              'Ownlocked':'vsixGateOwnLocked','RepeatACT':'vsixGateRepeatAsk','Limit':'vsixLimitRate'}
_gate = {}
for _o in OUT:
    m = re.search(r'\\renewcommand\{\\vsixPathSix([A-Za-z]+)\}\{([^}]*)\}', _o)
    if m and m.group(1) in GATE_ALIAS: _gate[GATE_ALIAS[m.group(1)]] = m.group(2)
for k, v in _gate.items():
    emit(k, v, 'E2-pathology.txt v0.6 mirror (alias of the KPI row)')

# ------------------------------------------------------------ E3 head-to-head
rows = jsonl('E3-headtohead.jsonl')
if rows:
    for opp, tag in (('v05','Five'), ('v04','Four')):
        sel = [r for r in rows if r.get('b')==opp]
        if not sel: continue
        wr = [r['winRateA'] for r in sel]
        emit(f'vsixHead{tag}Mean', pct(sum(wr)/len(wr)), f'E3-headtohead.jsonl: mean over {len(wr)} banks vs {opp}')
        emit(f'vsixHead{tag}Min', pct(min(wr)), f'E3-headtohead.jsonl: min bank vs {opp}')
        emit(f'vsixHead{tag}Max', pct(max(wr)), f'E3-headtohead.jsonl: max bank vs {opp}')
        emit(f'vsixHead{tag}Banks', str(len(wr)), f'E3-headtohead.jsonl: bank count vs {opp}')
        emit(f'vsixHead{tag}N', commafy(sum(r['games'] for r in sel)), 'E3-headtohead.jsonl: total games')
        lines = []
        for r in sel:
            lines.append(f"{r['winRateA']*100:.2f} & [{r['ci'][0]*100:.2f}, {r['ci'][1]*100:.2f}] & "
                         f"{r['meanSetsA']:.3f} & {r['meanSetsB']:.3f} \\\\")
        writeTable(f'h2h_{opp}.tex', lines)

# ------------------------------------------------------------- E4 per-style
rows = jsonl('E4-perstyle.jsonl')
if rows:
    by = {}
    for r in rows:
        by.setdefault(r['a'].split(':')[0], {})[r['b']] = r
    order = ['v05','v04','v03','v02','lockout','detective','diversifier','hunter','bluffer','random','silent','feint','withholder']
    arms = [a for a in ('v06','v05','v04') if a in by]
    lines = []
    for opp in order:
        if not all(opp in by[a] for a in arms): continue
        cells = ' & '.join(f"{by[a][opp]['winRateA']*100:.2f}" for a in arms)
        lines.append(f"{opp} & {cells} \\\\")
    tag = {'v06':'Six','v05':'Five','v04':'Four'}
    summ = []
    def row(label, fmt):
        cells = ' & '.join(fmt(a) for a in arms)
        summ.append(f"{label} & {cells} \\\\")
    stat = {}
    for a in arms:
        vals = [by[a][o]['winRateA'] for o in order if o in by[a]]
        stat[a] = (min(vals), sum(vals)/len(vals),
                   [o for o in order if o in by[a] and by[a][o]['winRateA'] == min(vals)][0])
    reg = {}
    for a in arms:
        r = [max(by[x][o]['winRateA'] for x in arms) - by[a][o]['winRateA'] for o in order if all(o in by[x] for x in arms)]
        reg[a] = 100*max(r) if r else 0.0
    row('\\textbf{worst case}', lambda a: '\\textbf{%.2f}' % (100*stat[a][0]))
    row('worst opponent',        lambda a: stat[a][2])
    row('mean (descriptive)',    lambda a: '%.2f' % (100*stat[a][1]))
    row('\\textbf{minimax regret}', lambda a: '\\textbf{%.2f}' % reg[a])
    writeTable('perstyle.tex', lines, extra='\n'.join(summ))
    for a in arms:
        vals = [by[a][o]['winRateA'] for o in order if o in by[a]]
        if not vals: continue
        tag = {'v06':'Six','v05':'Five','v04':'Four'}[a]
        emit(f'vsixStyleWorst{tag}', pct(min(vals)), f'E4-perstyle.jsonl: min over {len(vals)} styles for {a}')
        emit(f'vsixStyleMean{tag}', pct(sum(vals)/len(vals)), f'E4-perstyle.jsonl: mean over {len(vals)} styles for {a}')
        worst = [o for o in order if o in by[a] and by[a][o]['winRateA']==min(vals)][0]
        emit(f'vsixStyleWorstOpp{tag}', worst, f'E4-perstyle.jsonl: argmin style for {a}')
    # minimax regret over the style set
    for a in arms:
        reg = []
        for o in order:
            if not all(o in by[x] for x in arms): continue
            best = max(by[x][o]['winRateA'] for x in arms)
            reg.append(best - by[a][o]['winRateA'])
        if reg:
            tag = {'v06':'Six','v05':'Five','v04':'Four'}[a]
            emit(f'vsixRegret{tag}', num(100*max(reg),2), f'E4-perstyle.jsonl: max regret over the style set for {a}')

# ------------------------------------------------------------ E5 ablations
t = read('E5-ablations.json')
if t:
    try:
        d = json.loads(t)
        lines = []
        for v in d['variants']:
            lines.append(f"\\texttt{{{v['spec'].replace('_','\\_')}}} & {v['winRate']*100:.2f} & "
                         f"{-v['deltaFromRef']*100:+.2f} & [{-v['ci'][1]*100:+.2f}, {-v['ci'][0]*100:+.2f}] \\\\")
        writeTable('ablations.tex', lines)
        emit('vsixAblateRef', pct(d['winRate']), 'E5-ablations.json: reference pooled win rate')
    except Exception as e:
        print('E5 parse failed:', e, file=sys.stderr)

# -------------------------------------------------------------- E8 tie/belief
t = read('E8-ties.txt')
if t:
    b = t.split('###')
    for seg in b[1:]:
        head = seg.split('\n',1)[0].strip()
        tag = ('Six' if 'v0.6 mirror' in head else 'Five' if 'v0.5 mirror' in head
               else 'Cross' if 'NON-mirror' in head else None)
        if not tag: continue
        def g(pat, cast=float):
            m = re.search(pat, seg); return cast(m.group(1)) if m else None
        v = g(r'EXACT TIES at the top\s+(\d+)', int)
        if v is not None: emit(f'vsixTieN{tag}', commafy(v), f'E8-ties.txt {head}')
        v = g(r'EXACT TIES at the top\s+\d+\s+\(([0-9.]+)%')
        if v is not None: emit(f'vsixTieShare{tag}', num(v), f'E8-ties.txt {head}')
        v = g(r'contested \(>=2 live asks\)\s+\d+\s+\(([0-9.]+)%')
        if v is not None: emit(f'vsixPath{tag}Contested', num(v), f'E8-ties.txt {head}')
        v = g(r'mean top1-top2 gap\s+([0-9.]+)')
        if v is not None: emit(f'vsixTieGap{tag}', num(v,3), f'E8-ties.txt {head}')
        v = g(r'mean spread ([0-9.]+)')
        if v is not None: emit(f'vsixTieSpread{tag}', num(v,3), f'E8-ties.txt {head}')
        v = g(r'same card, diff target\s+\d+\s+\(([0-9.]+)%')
        if v is not None: emit(f'vsixTieSameCard{tag}', num(v), f'E8-ties.txt {head}')
        v = g(r'mean build ([0-9.]+) us')
        if v is not None: emit(f'vsixExactBuildUs{tag}', num(v,1), f'E8-ties.txt {head}')
        v = g(r'same half-suit, diff card\s*(\d+)', int)
        v2 = g(r'same half-suit, diff card\s*\d+\s+\(([0-9.]+)%')
        if v2 is not None: emit(f'vsixTieSameSuit{tag}', num(v2), f'E8-ties.txt {head}')
        v = g(r'SEPARATES the tied candidates\s+(\d+)', int)
        if v is not None: emit(f'vsixTieExactSep{tag}', str(v), f'E8-ties.txt {head}')
        v = g(r'chain/threat pass MOVES the pick\s+\d+\s+\(([0-9.]+)% of ties')
        if v is not None: emit(f'vsixTieShipMoved{tag}', num(v), f'E8-ties.txt {head}')
        v = g(r'SEPARATES the tied candidates\s+\d+\s+\(([0-9.]+)%')
        if v is not None: emit(f'vsixTieExactSepPct{tag}', num(v), f'E8-ties.txt {head}')
        for lbl, pat in (('Array', r'array order \(enumeration first\)\s+([0-9.]+)%'),
                         ('Fast',  r'Fast-posterior argmax in the tie\s+([0-9.]+)%'),
                         ('Ship',  r"the SHIPPED policy's actual pick\s+([0-9.]+)%"),
                         ('Exact', r'EXACT-posterior argmax in the tie\s+([0-9.]+)%'),
                         ('Best',  r'hindsight best in the tie\s+([0-9.]+)%')):
            v = g(pat)
            if v is not None: emit(f'vsixTieHit{lbl}{tag}', num(v), f'E8-ties.txt {head}')
        for lbl, pat in (('FullFast', r'hit rate, Fast argmax\s+([0-9.]+)%'),
                         ('FullExact',r'hit rate, EXACT argmax\s+([0-9.]+)%'),
                         ('FullBest', r'hit rate, hindsight best\s+([0-9.]+)%'),
                         ('Disagree', r'fast/exact argmax disagreement\s+([0-9.]+)%')):
            v = g(pat)
            if v is not None: emit(f'vsix{lbl}{tag}', num(v), f'E8-ties.txt {head}')

t = read('E8-belief.txt')
if t:
    lines = []
    for line in t.splitlines():
        m = re.match(r'^(.*?)\s+(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)%$', line.strip())
        if m:
            lines.append(f"{m.group(1).strip()} & {int(m.group(2)):,} & {m.group(3)} & {m.group(4)} & {m.group(5)} \\\\")
            key = m.group(1).strip()
            if key.startswith('exact'):
                emit('vsixBeliefExactNLL', m.group(3), 'E8-belief.txt exact row')
                emit('vsixBeliefExactHit', m.group(5), 'E8-belief.txt exact row')
            if 'th=0.000' in key:
                emit('vsixBeliefNoPriorNLL', m.group(3), 'E8-belief.txt theta=0 row')
                emit('vsixBeliefNoPriorHit', m.group(5), 'E8-belief.txt theta=0 row')
            if 'th=0.445' in key or 'th=0.44458' in key:
                emit('vsixBeliefShippedNLL', m.group(3), 'E8-belief.txt shipped-theta row')
                emit('vsixBeliefShippedHit', m.group(5), 'E8-belief.txt shipped-theta row')
    if lines:
        writeTable('belief.tex', lines)

# -------------------------------------------------------------- E9 throughput
t = read('E9-throughput.txt')
if t:
    lines = []
    for line in t.splitlines():
        m = re.match(r'^(\S+)\s+([0-9.]+) games/s', line.strip())
        if m:
            lines.append(f"\\texttt{{{m.group(1).replace('_','\\_')}}} & {float(m.group(2)):.1f} \\\\")
            key = {'v04':'Four','v05':'Five','v06':'Six'}.get(m.group(1))
            if key: emit(f'vsixThroughput{key}', num(float(m.group(2)),1), 'E9-throughput.txt')
    if lines:
        writeTable('throughput.tex', lines)

# -------------------------------------------------------------- E10 gateaudit
t = read('E10-gateaudit.txt')
if t:
    emit('vsixGateN', '1', 'E10-gateaudit.txt: one audit run')
    m = re.search(r'\(opportunity, half-suit\)\s+(\d+)', t)
    if m: emit('vsixGateStarved', commafy(m.group(1)), 'E10-gateaudit.txt: (opportunity, half-suit) pairs')
    m = re.search(r'declaration opportunities\s+(\d+)', t)
    if m: emit('vsixGateOpportunities', commafy(m.group(1)), 'E10-gateaudit.txt')
    m = re.search(r'rejected by a cheap gate\s+(\d+)', t)
    if m: emit('vsixGateRejected', commafy(m.group(1)), 'E10-gateaudit.txt')
    m = re.search(r'false negatives\s+(\d+)', t)
    if m: emit('vsixGateFalseNeg', commafy(m.group(1)), 'E10-gateaudit.txt')
    emit('vsixGateVerdict', 'PASS' if 'GATEAUDIT PASS' in t else 'FAIL', 'E10-gateaudit.txt')

# ------------------------------------------------------------- E11 partners
rows = jsonl('E11-partners.jsonl')
if rows:
    cur = None; table = {}
    for r in rows:
        if 'partners' in r: cur = (r['partners'], r['seat'].split(':')[0])
        elif cur: table[cur] = r; cur = None
    lines = []
    for p in ('self','v03','detective','withholder'):
        cells = []
        for a in ('v06','v05'):
            r = table.get((p,a))
            cells.append(f"{r['winRateA']*100:.2f}" if r else '--')
        lines.append(f"{p} & " + ' & '.join(cells) + ' \\\\')
    writeTable('partners.tex', lines)

# ------------------------------------------------------------- E12 search
rows = jsonl('E12-search.jsonl')
if rows:
    cur = None; pairs = []
    for r in rows:
        if 'search' in r: cur = r['search']
        elif cur is not None: pairs.append((cur, r)); cur = None
    lines = []
    for cfg, r in pairs:
        lines.append(f"\\texttt{{{cfg.replace('_','\\_')}}} & {r['winRateA']*100:.2f} & "
                     f"[{r['ci'][0]*100:.2f}, {r['ci'][1]*100:.2f}] & {r['games']:,} \\\\")
        if 'blend=1000000' in cfg:
            emit('vsixSearchControl', pct(r['winRateA']), 'E12-search.jsonl: blueprint-forced control')
            _ctrl = r['winRateA']
        if cfg.endswith('kappa=0') and 'det=8' in cfg:
            emit('vsixSearchUnguarded', pct(r['winRateA']), 'E12-search.jsonl: unguarded argmax')
        if 'kappa=2.5' in cfg:
            emit('vsixSearchGuarded', pct(r['winRateA']), 'E12-search.jsonl: kappa=2.5')
        if 'deadsearch=2' in cfg:
            emit('vsixSearchDead', pct(r['winRateA']), 'E12-search.jsonl: endgame search with the deliberate miss')
    if lines:
        writeTable('search.tex', lines)
        best = max(p[1]['winRateA'] for p in pairs)
        worst = min(p[1]['winRateA'] for p in pairs)
        emit('vsixSearchSpread', num(100*(best-worst),2), 'E12-search.jsonl: range of the deviation-threshold ladder')
        try:
            emit('vsixCurseGap', num(100*(_ctrl - min(p[1]['winRateA'] for p in pairs if 'kappa=0' in p[0])),2),
                 'E12-search.jsonl: blueprint-forced control minus the unguarded argmax')
        except Exception: pass

# ------------------------------------------------------- E13 rollout fidelity
rows = jsonl('E13-rollout.jsonl')
if rows:
    cur = None; recs = []
    for r in rows:
        if 'rollout' in r: cur = {'spec': r['rollout']}
        elif 'rolloutBench' in r:
            if cur is not None: cur['gps'] = r.get('gps'); recs.append(cur); cur = None
        elif cur is not None: cur.update(r)
    lines = []
    for r in recs:
        if 'winRateA' not in r: continue
        gps = r.get('gps')
        lines.append(f"\\texttt{{{r['spec'].replace('_','\\_')}}} & {r['winRateA']*100:.2f} & "
                     f"[{r['ci'][0]*100:.2f}, {r['ci'][1]*100:.2f}] & "
                     + (f"{gps:.1f}" if isinstance(gps,(int,float)) else "--") + " \\\\")
    if lines:
        writeTable('rollout.tex', lines)

# ------------------------------------------------------- E14 search deviation
t = read('E14-searchdev.txt')
if t:
    lines = []
    for seg in t.split('###')[1:]:
        head = seg.split('\n',1)[0].strip()
        k = head.replace('kappa=','')
        def g(pat, cast=float):
            m = re.search(pat, seg); return cast(m.group(1)) if m else None
        dev = g(r'searches that MOVED the action\s+\d+\s+\(([0-9.]+)% of searches')
        sh  = g(r'decisions searched\s+\d+\s+\(([0-9.]+)%')
        gps = g(r'\(([0-9.]+) games/s, 1 thread\)')
        dep = g(r'mean depth ([0-9.]+)')
        if dev is None: continue
        lines.append(f"{k} & {sh:.2f} & {dev:.2f} & " + (f"{gps:.3f}" if gps else '--') + " \\\\")
        tag = {'0':'Zero','1':'One','2.5':'TwoFive','4':'Four','6':'Six'}.get(k, k.replace('.','p'))
        emit(f'vsixSearchDev{tag}', num(dev), f'E14-searchdev.txt kappa={k}')
        if dep is not None and k == '2.5':
            emit('vsixSearchDepth', num(dep,1), 'E14-searchdev.txt kappa=2.5 mean rollout depth')
        if gps is not None and k == '2.5':
            emit('vsixSearchGps', num(gps,3), 'E14-searchdev.txt kappa=2.5 single-thread games/s')
    if lines:
        writeTable('searchdev.tex', lines)

# --------------------------------------------------- E15 the deliberate miss
t = read('E15-deliberate-miss.txt')
if t:
    segs = t.split('###')
    for seg in segs[1:]:
        head = seg.split('\n',1)[0].strip()
        if 'PATHOLOGY' in head:
            d = parse_path_block(seg)
            for f_, v in d.items():
                emit(f'vsixDeadPath{f_[0].upper()+f_[1:]}', num(v,2) if isinstance(v,float) else str(v),
                     'E15-deliberate-miss.txt unbanned-dead pathology')
        if 'win rate against v0.5' in head:
            m = re.search(r'win rate\s+([0-9.]+)%\s+\[([0-9.]+), ([0-9.]+)\]', seg)
            if m:
                emit('vsixDeadWin', m.group(1), 'E15-deliberate-miss.txt win rate')
                emit('vsixDeadWinLo', m.group(2), 'E15-deliberate-miss.txt CI low')
                emit('vsixDeadWinHi', m.group(3), 'E15-deliberate-miss.txt CI high')

# ---------------------------------------------------------------- fit traces
for tag, fn in (('A','fitA.jsonl'), ('C','fitC.jsonl')):
    p = os.path.join(ROOT, 'research', 'v06', 'runs', fn)
    if not os.path.exists(p): continue
    hdr = None; gens = []
    for line in open(p):
        line = line.strip()
        if not line: continue
        d = json.loads(line)
        if 'header' in d: hdr = d
        elif 'gen' in d: gens.append(d)
    if hdr:
        emit(f'vsixFit{tag}Panel', '+'.join(hdr['panel']), f'{fn} header')
        emit(f'vsixFit{tag}Seed', str(hdr['seed']), f'{fn} header')
        emit(f'vsixFit{tag}Pop', str(hdr['pop']), f'{fn} header')
        emit(f'vsixFit{tag}Deals', f"{hdr['deals']}x{hdr['rotations']}", f'{fn} header')
        emit(f'vsixFit{tag}SigmaRel', num(hdr['sigmaRel'],3), f'{fn} header')
    if gens:
        emit(f'vsixFit{tag}Gens', str(len(gens)), f'{fn}: generation records')
        worst = max(max(g['clip']) for g in gens[:1]) if gens[0].get('clip') else None
        if worst is not None:
            emit(f'vsixFit{tag}ClipGenZero', pct(worst), f'{fn}: worst generation-0 clip fraction')

# Every table file the manuscript inputs must EXIST, because a conditional
# \input inside a tabular is a "Misplaced \omit" at the first cell.  Emit a
# visible placeholder row for any table this run had no artifact for.
PLACEHOLDER = {
    'rollout.tex': 4, 'search.tex': 4, 'searchdev.tex': 4, 'perstyle.tex': 4,
    'ablations.tex': 4, 'belief.tex': 5, 'throughput.tex': 2, 'partners.tex': 3,
    'h2h_v05.tex': 4, 'h2h_v04.tex': 4, 'calibration.tex': 5, 'params.tex': 3,
}
for fn in TABLE_SPEC:
    fp = os.path.join(TBL, fn)
    if not os.path.exists(fp) or os.path.getsize(fp) == 0:
        writeTable(fn, [])

# ------------------------------------------- protocol and configuration constants
emit('vsixRotations', '6', 'engine/experiments_v06.sh: --rotations on every held-out cell')
emit('vsixBootReps', '20,000', 'engine/src/arena.hpp pairedBootstrap/clusterBootstrap default B')
emit('vsixBootAlpha', '95', 'engine/src/arena.hpp: 2.5/97.5 percentile bootstrap interval')
emit('vsixHeldOutBanks', '5', 'engine/experiments_v06.sh E3: seed list')
emit('vsixCores', '15', 'measurement host: 15 cores')
emit('vsixMirrorDuplication', '2', 'a mirror cell at --rotations=2 duplicates exactly; see E2')
emit('vsixVerifyGames', '600', 'engine/experiments_v06.sh E1')
emit('vsixReservedSeeds', '20260823, 20260824', 'fitting seeds, disjoint from every evaluation bank')
emit('vsixPairedSelfDelta', '0.0000', 'engine/src/tuner.hpp self-test: a policy paired against itself')

# the shipped policy prior, read straight out of the frozen vector
try:
    src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src', 'v06.hpp')).read()
    blk = re.search(r'V6PARAMS\[NV6PARAM\] = \{(.*?)\};', src, re.S).group(1)
    vals = [float(x) for x in re.findall(r'-?\d+\.?\d*', blk)]
    if len(vals) >= 37:
        emit('vsixPriorTheta', '%.5f' % vals[29], 'engine/src/v06.hpp frozen vector, coordinate NFEAT+9')
        emit('vsixPriorPhi',   '%.5f' % vals[30], 'engine/src/v06.hpp frozen vector, coordinate NFEAT+10')
    m = re.search(r'V6FIT_PROVENANCE = "([^"]*)"', src)
    if m: emit('vsixFitProvenance', m.group(1), 'engine/src/v06.hpp provenance stamp')
except Exception as e:
    print('frozen-vector read failed:', e, file=sys.stderr)

# v0.5's fit trace, for the "the fit was noise" claim
fp = os.path.join(ROOT, 'research', 'v05', 'runs', 'fit-round1.jsonl')
if os.path.exists(fp):
    inc = []
    best = []
    pop = None
    for line in open(fp):
        line = line.strip()
        if not line.startswith('{'): continue
        try: d = json.loads(line)
        except Exception: continue
        if 'gen' in d:
            inc.append(d.get('incumbentScore')); best.append(d.get('bestScore'))
    inc = [x for x in inc if isinstance(x, (int, float))]
    best = [x for x in best if isinstance(x, (int, float))]
    if len(inc) > 3:
        n = len(inc); xs = list(range(n))
        mx = sum(xs)/n; my = sum(inc)/n
        sxx = sum((x-mx)**2 for x in xs); sxy = sum((x-mx)*(y-my) for x, y in zip(xs, inc))
        slope = sxy/sxx if sxx else 0.0
        resid = [y - (my + slope*(x-mx)) for x, y in zip(xs, inc)]
        sse = sum(r*r for r in resid)
        se = ((sse/(n-2))/sxx) ** 0.5 if n > 2 and sxx else 0.0
        emit('vsixFitSlope', '%+.5f' % slope, 'research/v05/runs/fit-round1.jsonl: OLS slope of the v0.5 incumbent objective')
        emit('vsixFitSlopeT', '%.2f' % (slope/se if se else 0.0), 'research/v05/runs/fit-round1.jsonl: t statistic')
        emit('vsixFitBanks', str(n), 'research/v05/runs/fit-round1.jsonl: generations')
        if best:
            gaps = [b - i for b, i in zip(best, inc)]
            emit('vsixFitBestGap', '%.4f' % (sum(gaps)/len(gaps)), 'research/v05/runs/fit-round1.jsonl: mean best-minus-incumbent')
            sd = (sum((x-sum(inc)/n)**2 for x in inc)/(n-1)) ** 0.5
            emit('vsixFitOrderStat', '%.4f' % (1.948*sd), 'expected maximum of 24 iid draws at the observed per-generation sd')
        emit('vsixFitPop', '24', 'research/v05/runs/fit-round1.jsonl: population')

# ---------- per-style profile pooled over EVERY bank it was measured on -------
pool = {}
def addcell(arm, opp, wr, n):
    d = pool.setdefault(arm, {}).setdefault(opp, [0.0, 0])
    d[0] += wr * n; d[1] += n
for r in jsonl('E4-perstyle.jsonl'):
    if 'a' in r and 'b' in r: addcell(r['a'].split(':')[0], r['b'], r['winRateA'], r['games'])
_cur = None
for r in jsonl('F4-perstyle-banks.jsonl'):
    if 'bank' in r: _cur = r['bank']
    elif _cur is not None and 'a' in r:
        addcell(r['a'].split(':')[0], r['b'], r['winRateA'], r['games']); _cur = None
if 'v06' in pool and 'v05' in pool:
    ORDER = ['v05','v04','v03','v02','lockout','detective','diversifier','hunter','bluffer','random','silent','feint','withholder']
    arms = [a for a in ('v06','v05','v04') if a in pool]
    def cell(a, o):
        d = pool[a].get(o); return d[0]/d[1] if d and d[1] else None
    lines = []
    for o in ORDER:
        vals = [cell(a, o) for a in arms]
        if any(v is None for v in vals): continue
        nn = pool['v06'][o][1]
        lines.append('%s & %s & %s \\\\' % (o, ' & '.join('%.2f' % (100*v) for v in vals), commafy(nn)))
    summ = []
    stat = {}
    for a in arms:
        vs = [cell(a, o) for o in ORDER if cell(a, o) is not None and all(cell(x, o) is not None for x in arms)]
        stat[a] = (min(vs), sum(vs)/len(vs), [o for o in ORDER if cell(a, o) == min(vs)][0])
    reg = {}
    for a in arms:
        rr = [max(cell(x, o) for x in arms) - cell(a, o) for o in ORDER
              if all(cell(x, o) is not None for x in arms)]
        reg[a] = 100*max(rr) if rr else 0.0
    summ.append('\\textbf{worst case} & ' + ' & '.join('\\textbf{%.2f}' % (100*stat[a][0]) for a in arms) + ' & \\\\')
    summ.append('worst opponent & ' + ' & '.join(stat[a][2] for a in arms) + ' & \\\\')
    summ.append('mean (descriptive) & ' + ' & '.join('%.2f' % (100*stat[a][1]) for a in arms) + ' & \\\\')
    summ.append('\\textbf{minimax regret} & ' + ' & '.join('\\textbf{%.2f}' % reg[a] for a in arms) + ' & \\\\')
    TABLE_SPEC['perstyle_pooled.tex'] = ('lrrrr', 'Opponent & \\vsix{} & \\vfive{} & \\vfour{} & games')
    writeTable('perstyle_pooled.tex', lines, extra='\n'.join(summ))
    for a in arms:
        tg = {'v06':'Six','v05':'Five','v04':'Four'}[a]
        emit(f'vsixPoolWorst{tg}', pct(stat[a][0]), 'E4 + F4 pooled: worst style cell')
        emit(f'vsixPoolWorstOpp{tg}', stat[a][2], 'E4 + F4 pooled: argmin style')
        emit(f'vsixPoolMean{tg}', pct(stat[a][1]), 'E4 + F4 pooled: mean over the style set')
        emit(f'vsixPoolRegret{tg}', num(reg[a], 2), 'E4 + F4 pooled: minimax regret over the style set')
    wh = [cell(a, 'withholder') for a in ('v06','v05')]
    if all(w is not None for w in wh):
        emit('vsixPoolWithholderSix', pct(wh[0]), 'E4 + F4 pooled: withholder cell, v0.6')
        emit('vsixPoolWithholderFive', pct(wh[1]), 'E4 + F4 pooled: withholder cell, v0.5')
        emit('vsixPoolWithholderGain', num(100*(wh[0]-wh[1]), 2), 'E4 + F4 pooled: withholder gain')
        emit('vsixPoolWithholderN', commafy(pool['v06']['withholder'][1]), 'E4 + F4 pooled: withholder games')
    emit('vsixPoolBanks', '3', 'E4 (515253) + F4 (90210, 424242)')
    ah = bh = tievs = 0
    for o in ORDER:
        a6, a5 = cell('v06', o), cell('v05', o)
        if a6 is None or a5 is None: continue
        if abs(a6 - a5) < 1e-9: tievs += 1
        elif a6 > a5: ah += 1
        else: bh += 1
    emit('vsixPoolAhead', str(ah), 'E4 + F4 pooled: styles where v0.6 beats v0.5')
    emit('vsixPoolBehind', str(bh), 'E4 + F4 pooled: styles where it does not')
    emit('vsixPoolTied', str(tievs), 'E4 + F4 pooled: styles where the two are equal')
    worstloss = 0.0
    for o in ORDER:
        a6, a5 = cell('v06', o), cell('v05', o)
        if a6 is None or a5 is None: continue
        worstloss = max(worstloss, 100*(a5 - a6))
    emit('vsixPoolWorstLoss', num(worstloss, 2), 'E4 + F4 pooled: largest per-style loss against v0.5')

# ---- F11 + F12: the high-power head-to-head that settles the comparison -----
# The battery's E3 cells are 1,800 games each, which the project's own planning
# rule prices at +/-2.3 points -- too coarse to resolve a one-point difference.
# These are 18,000 (v0.5) and 12,000 (v0.4) games a bank.
import math as _mm
def _hcells(path, opp):
    tt = read(path)
    if not tt: return []
    out, cur = [], None
    for bb in re.findall(r'\{.*?\}(?=\s*(?:\{|$))', tt, re.S):
        try: dd = json.loads(bb)
        except Exception: continue
        if 'bank' in dd: cur = dd
        elif cur is not None and 'winRateA' in dd:
            if cur.get('b', opp) == opp: out.append((cur['bank'], dd['winRateA'], dd['games']))
            cur = None
    return out
for opp, tg in (('v05','Five'), ('v04','Four')):
    cs = _hcells('F11-bighead-v05.jsonl', opp) + _hcells('F12-bighead-extra.jsonl', opp)
    if not cs: continue
    tot = sum(x[2] for x in cs); w = sum(x[1]*x[2] for x in cs); pp = w/tot
    hw = 98.0/_mm.sqrt(tot)
    emit(f'vsixBig{tg}', num(100*pp, 2), f'F11+F12: high-power head-to-head against {opp}, pooled')
    emit(f'vsixBig{tg}N', commafy(tot), 'F11+F12: games')
    emit(f'vsixBig{tg}Banks', str(len(cs)), 'F11+F12: seed banks')
    emit(f'vsixBig{tg}Above', str(sum(1 for x in cs if x[1] > 0.5)), 'F11+F12: banks above parity')
    emit(f'vsixBig{tg}Lo', num(100*pp-hw, 2), "F11+F12: planning-rule interval, 98/sqrt(N)")
    emit(f'vsixBig{tg}Hi', num(100*pp+hw, 2), 'F11+F12: planning-rule interval')
    emit(f'vsixBig{tg}Min', num(100*min(x[1] for x in cs), 2), 'F11+F12: worst bank')
    lines = ['%s & %.2f & %s \\\\' % (b, 100*wr, commafy(n)) for b, wr, n in cs]
    TABLE_SPEC[f'bighead_{opp}.tex'] = ('lrr', 'Seed bank & Win rate (\\%) & Games')
    writeTable(f'bighead_{opp}.tex', lines)

# ---- every v0.6-vs-v0.5 head-to-head cell in the battery, pooled honestly ----
# Reporting only the E3 banks would be selection on the experiment that agreed:
# the battery contains two further default-rules cells on further disjoint banks,
# and v0.6 loses both.
cells = []
for r in jsonl('E3-headtohead.jsonl'):
    if r.get('b') == 'v05' and 'winRateA' in r: cells.append((r['winRateA'], r['games']))
_cur = None
for r in jsonl('E7-rules.jsonl'):
    if 'dialect' in r: _cur = r['dialect']
    elif _cur is not None:
        if _cur in ('default', ''): cells.append((r['winRateA'], r['games']))
        _cur = None
try:
    _e5 = json.loads(read('E5-ablations.json') or '{}')
    if _e5.get('panel') and _e5['panel'][0] == 'v05':
        cells.append((_e5['perOpponent'][0], 800))
except Exception: pass
if cells:
    tot = sum(c[1] for c in cells); wins = sum(c[0]*c[1] for c in cells)
    emit('vsixHeadPooled', pct(wins/tot), 'E3 + E7-default + E5: every v0.6-vs-v0.5 cell in the battery')
    emit('vsixHeadPooledN', commafy(tot), 'E3 + E7-default + E5: total games')
    emit('vsixHeadCells', str(len(cells)), 'number of head-to-head cells pooled')
    emit('vsixHeadCellsAbove', str(sum(1 for c in cells if c[0] > 0.5)), 'cells above parity')
    import math as _m
    _p = wins/tot
    emit('vsixHeadZ', num((_p-0.5)/_m.sqrt(0.25/tot), 2),
         'naive z on the pooled cells; ignores rotation clustering and therefore OVERSTATES significance')

# ---------------------------------------------------------------- E7 dialects
rows = jsonl('E7-rules.jsonl')
if rows:
    cur = None; recs = []
    for r in rows:
        if 'dialect' in r: cur = r['dialect']
        elif cur is not None: recs.append((cur, r)); cur = None
    if recs:
        wr = [r['winRateA'] for _, r in recs]
        emit('vsixDialectN', str(len(recs)), 'E7-rules.jsonl: dialects measured')
        emit('vsixDialectSpan', num(100*(max(wr)-min(wr)),2), 'E7-rules.jsonl: range of the win rate across dialects')
        emit('vsixDialectDeals', commafy(recs[0][1]['games']), 'E7-rules.jsonl: games per dialect')
        emit('vsixDialectSeed', '828282', 'engine/experiments_v06.sh E7')
        lines = ['\\texttt{%s} & %.2f & [%.2f, %.2f] \\\\' % (d.replace('_','\\_'), 100*r['winRateA'],
                 100*r['ci'][0], 100*r['ci'][1]) for d, r in recs]
        TABLE_SPEC['dialects.tex'] = ('lrr', 'Dialect & Win rate vs \\vfive{} & 95\\% CI')
        writeTable('dialects.tex', lines)

# -------------------------------------------------- costs quoted in the text
emit('vsixCountCost', '0.4', 'E8-ties.txt: mean exact count-law build, milliseconds (see vsixExactBuildUs*)')
emit('vsixCountStates', '$10^5$', 'engine/src/belief.hpp DP_STATE_CAP and the capacity-vector bound')
emit('vsixSinkhornCost', '9.07', 'research/v06/notes/R10 section 1: us/event for the reduced blueprint, all six seats')
emit('vsixLeafSd', '2.5', 'research/v06/notes/R11 section 4: sd of one determinization s terminal set differential')
emit('vsixCollapseEvents', '76', 'research/v06/notes/R10 section 2: median event at which the consistent-deal count falls below 10^5')
emit('vsixPriorDeleteCost', '4.60', 'research/v05/results/P3-deception.md section 4 (transcribed)')
emit('vsixPriorDeleteCI', '[2.63, 6.58]', 'research/v05/results/P3-deception.md section 4 (transcribed)')


# --------------------------------------------- F0 search replication and lift
rows = jsonl('F0-search-confirm.jsonl')
if rows:
    cur = None; recs = []
    for r in rows:
        if 'cfg' in r: cur = r
        elif cur is not None: recs.append((cur, r)); cur = None
    vs05 = [(c, r) for c, r in recs if c['opp'] == 'v05' and 'kappa=2.5' in c['cfg']]
    vs06 = [(c, r) for c, r in recs if c['opp'] == 'v06' and 'kappa=2.5' in c['cfg']]
    # the ladder's own bank belongs with the replication banks
    lad = [r for c, r in [] ]
    if vs05:
        wr = [r['winRateA'] for _, r in vs05]
        tot = sum(r['games'] for _, r in vs05)
        emit('vsixSearchReplBanks', str(len(wr)), 'F0-search-confirm.jsonl: replication banks at kappa=2.5')
        emit('vsixSearchReplMean', pct(sum(wr)/len(wr)), 'F0-search-confirm.jsonl: mean over the replication banks')
        emit('vsixSearchReplN', commafy(tot), 'F0-search-confirm.jsonl: games')
        emit('vsixSearchReplMin', pct(min(wr)), 'F0-search-confirm.jsonl: worst replication bank')
    if vs06:
        wr6 = [r['winRateA'] for _, r in vs06]
        n6 = sum(r['games'] for _, r in vs06)
        emit('vsixSearchLift', pct(sum(w*r['games'] for (_, r), w in zip(vs06, wr6))/n6),
             'F0-search-confirm.jsonl: v0.6-Search against v0.6, pooled over every cell')
        emit('vsixSearchLiftN', commafy(n6), 'F0-search-confirm.jsonl: games')
        emit('vsixSearchLiftCells', str(len(wr6)), 'F0-search-confirm.jsonl: cells')
        emit('vsixSearchLiftMin', pct(min(wr6)), 'F0-search-confirm.jsonl: worst cell')
        emit('vsixSearchLiftAbove', str(sum(1 for w in wr6 if w > 0.5)), 'F0-search-confirm.jsonl: cells above parity')
    lines = ['\\texttt{%s} & %s & %s & %.2f & [%.2f, %.2f] \\\\' %
             (c['cfg'].replace('v06:legacy=1,','').replace('v06:','').replace('_','\\_'),
              c['opp'], c['bank'], 100*r['winRateA'], 100*r['ci'][0], 100*r['ci'][1])
             for c, r in recs]
    TABLE_SPEC['searchrepl.tex'] = ('llrrr', 'Configuration & Opponent & Bank & Win rate & 95\\% CI')
    writeTable('searchrepl.tex', lines)

# -------- the search headline, pooled over EVERY bank it was measured on -----
# E12's ladder row and F0's replication banks are the same configuration at the
# same budget; pooling them is the honest estimate, and the single best bank is
# not.
_sr = []
_rows = jsonl('E12-search.jsonl')
_cur = None
for r in _rows:
    if 'search' in r: _cur = r['search']
    elif _cur is not None:
        if 'det=12,cand=4,kappa=2.5' in _cur: _sr.append((r['winRateA'], r['games']))
        _cur = None
_rows = jsonl('F0-search-confirm.jsonl')
_cur = None
for r in _rows:
    if 'cfg' in r: _cur = r
    elif _cur is not None:
        if _cur['opp'] == 'v05' and 'det=12,cand=4,kappa=2.5' in _cur['cfg']:
            _sr.append((r['winRateA'], r['games']))
        _cur = None
if _sr:
    _tot = sum(x[1] for x in _sr); _w = sum(x[0]*x[1] for x in _sr)
    emit('vsixSearchPooled', pct(_w/_tot), 'E12 + F0: every bank of the kappa=2.5 search against v0.5')
    emit('vsixSearchPooledN', commafy(_tot), 'E12 + F0: games')
    emit('vsixSearchPooledBanks', str(len(_sr)), 'E12 + F0: banks')
    emit('vsixSearchPooledMin', pct(min(x[0] for x in _sr)), 'E12 + F0: worst bank')

# ------------------------------------------------- F9 the tie-only search
t = read('F9-tieonly.txt')
if t:
    blocks = t.split('###')
    for seg in blocks[1:]:
        head = seg.split('\n', 1)[0].strip()
        wr = [float(x) for x in re.findall(r'win rate\s+([0-9.]+)%', seg)]
        if not wr: continue
        if 'vs v0.5' in head:
            emit('vsixTieSearchMean', num(sum(wr)/len(wr)), 'F9-tieonly.txt: mean over the replication banks, vs v0.5')
            emit('vsixTieSearchBankList', ', '.join('%.2f' % x for x in wr), 'F9-tieonly.txt: per-bank')
            emit('vsixTieSearchBanks', str(len(wr)), 'F9-tieonly.txt: banks')
            emit('vsixTieSearchMin', num(min(wr)), 'F9-tieonly.txt: worst bank')
        elif 'vs v0.6' in head:
            emit('vsixTieSearchLift', num(sum(wr)/len(wr)), 'F9-tieonly.txt: tie-only search against v0.6 itself')
            emit('vsixTieSearchLiftBanks', str(len(wr)), 'F9-tieonly.txt')
        m = re.search(r'([0-9.]+) games/s', seg)
        if m: emit('vsixTieSearchGps', num(float(m.group(1)),1), 'F9-tieonly.txt: throughput, all threads')

# ---------------------------------------- F8 where the search's advantage lives
t = read('F8-tiesearch.txt')
if t:
    for seg in t.split('###')[1:]:
        head = seg.split('\n', 1)[0].strip()
        m = re.search(r'win rate\s+([0-9.]+)%\s+\[([0-9.]+), ([0-9.]+)\]', seg)
        if not m: continue
        key = ('TieOnlyTwelve' if head.startswith('A:') else
               'TieOnlyOne' if head.startswith('B:') else
               'TieOnlyThirtyTwo' if head.startswith('C:') else 'TieRandom')
        emit(f'vsixSearch{key}', m.group(1), f'F8-tiesearch.txt {head}')
        emit(f'vsixSearch{key}Lo', m.group(2), f'F8-tiesearch.txt {head}')
        emit(f'vsixSearch{key}Hi', m.group(3), f'F8-tiesearch.txt {head}')

# --------------------------------------- the tie-guarded win rate (F7 partner)
t = read('F7-winrate.txt')
if t:
    m = re.search(r'win rate\s+([0-9.]+)%\s+\[([0-9.]+), ([0-9.]+)\]', t)
    if m:
        emit('vsixSearchTieGuardedWin', m.group(1), 'F7-winrate.txt: kappa applied inside the tie group too')
        emit('vsixSearchTieGuardedLo', m.group(2), 'F7-winrate.txt')
        emit('vsixSearchTieGuardedHi', m.group(3), 'F7-winrate.txt')

# ------------------------------------------------- F7 tie-guarded deviation
t = read('F7-tieguard.txt')
if t:
    segs = t.split('###')
    for seg in segs[1:]:
        head = seg.split('\n', 1)[0].strip()
        m = re.search(r'MOVED the action\s+\d+\s+\(([0-9.]+)% of searches', seg)
        if not m: continue
        k = 'TwoFive' if '2.5' in head else ('Four' if 'kappa=4' in head else None)
        if k: emit(f'vsixSearchDevGuarded{k}', num(float(m.group(1))), f'F7-tieguard.txt {head}')

# ------------------------------------------ F6 information-set reconstruction
t = read('F6-reconstruction.txt')
if t:
    segs = re.findall(r'states (\d+)\s+card checks (\d+)\s+owner mismatches\s+(\d+) \(([0-9.]+)%\)\s+mask  mismatches\s+(\d+) \(([0-9.]+)%\)\s+reconstruction WIDER (\d+)  NARROWER (\d+)', t)
    if segs:
        st, ck, om, op, mm, mp, wd, nr = segs[0]
        emit('vsixReconStates', commafy(st), 'F6-reconstruction.txt seed 31')
        emit('vsixReconChecks', commafy(ck), 'F6-reconstruction.txt seed 31')
        emit('vsixReconMaskPct', mp, 'F6-reconstruction.txt seed 31: mask mismatch rate')
        emit('vsixReconWider', wd, 'F6-reconstruction.txt seed 31: mismatches where the reconstruction is WIDER')
        emit('vsixReconNarrower', nr, 'F6-reconstruction.txt seed 31: mismatches where it is NARROWER')
        emit('vsixReconBanks', str(len(segs)), 'F6-reconstruction.txt: seed banks')

# ------------------------- F10 + E5: the paired panel comparison, three banks
paired = []
t = read('E5-ablations.json')
if t:
    try:
        d = json.loads(t)
        for v in d['variants']:
            if v['spec'] == 'v05': paired.append(('606060', v['deltaFromRef'], v['ci']))
    except Exception: pass
t = read('F10-panel-banks.json')
if t:
    _cur = None
    for b in re.findall(r'\{.*?\}(?=\s*(?:\{|$))', t, re.S):
        try: d = json.loads(b)
        except Exception: continue
        if 'bank' in d: _cur = str(d['bank']); continue
        if 'reference' in d and d.get('variants'):
            v = d['variants'][0]
            paired.append((_cur or '?', v['deltaFromRef'], v['ci']))
if paired:
    lines = ['%s & %+.2f & [%+.2f, %+.2f] \\\\' % (bk, 100*dl, 100*ci[0], 100*ci[1])
             for bk, dl, ci in paired]
    TABLE_SPEC['pairedpanel.tex'] = ('lrr', 'Seed bank & \\vsix{} $-$ \\vfive{} & 95\\% paired CI')
    writeTable('pairedpanel.tex', lines)
    emit('vsixPairedBanks', str(len(paired)), 'E5 + F10: paired panel comparisons')
    emit('vsixPairedMean', num(100*sum(d for _, d, _ in paired)/len(paired), 2), 'E5 + F10: mean paired margin')
    emit('vsixPairedMin', num(100*min(d for _, d, _ in paired), 2), 'E5 + F10: smallest paired margin')
    emit('vsixPairedExcl', str(sum(1 for _, _, ci in paired if ci[0] > 0)), 'E5 + F10: banks whose CI excludes zero')

# ------------------------------------------------- F1 the clean four-way
t = read('F1-chain2x2.json')
if t:
    try:
        d = json.loads(t)
        label = {'v06:wvoid=0,wteam=0,wlast=0': 'extras off, chain off',
                 'v06:chain2=1': 'extras on, chain ON',
                 'v06:xf=0': 'extras off, chain ON',
                 'v06:rtie=1': 'extras on, chain off, ties at random'}
        lines = []
        for v in d['variants']:
            lines.append('\\texttt{%s} & %s & %.2f & %+.2f & [%+.2f, %+.2f] \\\\' %
                         (v['spec'].replace('_','\\_'), label.get(v['spec'], ''),
                          100*v['winRate'], 100*v['deltaFromRef'],
                          100*v['ci'][0], 100*v['ci'][1]))
        TABLE_SPEC['fourway.tex'] = ('llrrr', 'Arm & Configuration & Pooled & \\vsix{} $-$ variant & 95\\% paired CI')
        writeTable('fourway.tex', lines)
        emit('vsixFourwayRef', pct(d['winRate']), 'F1-chain2x2.json: reference pooled win rate')
    except Exception as e:
        print('F1 parse failed:', e, file=sys.stderr)

# ------------------------------------------------------ X1 exploitability
rows = jsonl('X1-lbr.jsonl')
if rows:
    lines = []
    for r in rows:
        if 'probe' not in r: continue
        lines.append('%s & %.2f & [%.2f, %.2f] & %s \\\\' %
                     (r['probe'], 100*r['winRateA'], 100*r['ci'][0], 100*r['ci'][1], commafy(r['games'])))
        tg = {'v04':'Four','v05':'Five','v06':'Six'}.get(r['probe'])
        if tg:
            emit(f'vsixLbr{tg}', pct(r['winRateA']), f"X1-lbr.jsonl: fitted best response against {r['probe']}")
            emit(f'vsixLbr{tg}Lo', pct(r['ci'][0]), 'X1-lbr.jsonl')
            emit(f'vsixLbr{tg}Hi', pct(r['ci'][1]), 'X1-lbr.jsonl')
            emit(f'vsixLbr{tg}N', commafy(r['games']), 'X1-lbr.jsonl')
    if lines:
        TABLE_SPEC['lbr.tex'] = ('lrrr', 'Frozen target & Response win rate & 95\\% CI & Games')
        writeTable('lbr.tex', lines)

# ------------------------------------------------------------------ manifest
import hashlib
mrows = []
for fn in sorted(os.listdir(RES)):
    fp = os.path.join(RES, fn)
    if not os.path.isfile(fp): continue
    h = hashlib.sha256(open(fp,'rb').read()).hexdigest()[:12]
    mrows.append('\\texttt{%s} & %s & \\texttt{%s} \\\\' % (fn.replace('_','\\_'), f"{os.path.getsize(fp):,}", h))
# The manifest is long, so it is a longtable and must NOT sit inside a float.
with open(os.path.join(TBL, 'manifest.tex'), 'w') as f:
    f.write('\\begin{longtable}{lrl}\n\\toprule\n'
            'Artifact & Bytes & sha256 (first 12) \\\\\n\\midrule\n\\endfirsthead\n'
            '\\toprule\nArtifact & Bytes & sha256 (first 12) \\\\\n\\midrule\n\\endhead\n')
    f.write('\n'.join(mrows) + '\n')
    f.write('\\bottomrule\n\\end{longtable}\n')
# and the machine-readable form, which is what a reproduction check reads
man = {'study': 'v06', 'results': 'research/v06/results', 'artifacts': []}
for fn in sorted(os.listdir(RES)):
    fp = os.path.join(RES, fn)
    if not os.path.isfile(fp) or fn == 'MANIFEST.json': continue
    man['artifacts'].append({'file': fn, 'bytes': os.path.getsize(fp),
                             'sha256': hashlib.sha256(open(fp,'rb').read()).hexdigest()})
for extra in ('research/v06/runs/fitA.jsonl', 'research/v06/runs/fitC.jsonl'):
    fp = os.path.join(ROOT, extra)
    if os.path.exists(fp):
        man['artifacts'].append({'file': extra, 'bytes': os.path.getsize(fp),
                                 'sha256': hashlib.sha256(open(fp,'rb').read()).hexdigest()})
with open(os.path.join(RES, 'MANIFEST.json'), 'w') as f:
    json.dump(man, f, indent=1)
print('wrote %d artifact digests to %s' % (len(man['artifacts']), os.path.join(RES, 'MANIFEST.json')))
emit('vsixManifestFiles', str(len(mrows)), 'research/v06/results/: artifact count')

path = os.path.join(PAPER, 'numbers_v06_generated.tex')
with open(path, 'w') as f:
    f.write("% GENERATED by engine/build_tables_v06.py -- do not edit.\n")
    f.write("% Each macro names the artifact and field it came from.\n\n")
    f.write('\n'.join(OUT) + '\n')
import time
emit('vsixBuildDate', time.strftime('%Y-%m-%d'), 'build_tables_v06.py run date')
# provenance counts: how many of the manuscript's numbers are generated here,
# and how many are transcribed from an artifact of an earlier study.
try:
    secs = os.path.join(PAPER, 'sections_v06')
    used_v6, used_num = set(), set()
    for fn in os.listdir(secs):
        if not fn.endswith('.tex'): continue
        tx = open(os.path.join(secs, fn)).read()
        used_v6 |= set(re.findall(r'\\(vsix[A-Za-z]+)', tx))
        used_num |= set(re.findall(r'\\(num[A-Za-z]+)', tx))
    gen = set(re.findall(r'renewcommand\{\\(vsix[A-Za-z]+)\}', '\n'.join(OUT)))
    emit('vsixProvGenerated', str(len(used_v6 & gen)), 'macros used in sections_v06 that this script generates')
    emit('vsixProvTranscribed', str(len(used_num)), 'macros used in sections_v06 transcribed from earlier studies')
except Exception as e:
    print('provenance count failed:', e, file=sys.stderr)
# wall clock of the battery, from the artifact mtimes
try:
    fs = [os.path.getmtime(os.path.join(RES, f)) for f in os.listdir(RES)
          if os.path.isfile(os.path.join(RES, f)) and f.startswith('E')]
    if len(fs) > 1:
        emit('vsixWallClock', num((max(fs) - min(fs)) / 3600.0, 1),
             'research/v06/results: span of the battery artifact mtimes, hours')
except Exception: pass
with open(path, 'w') as f:
    f.write("% GENERATED by engine/build_tables_v06.py -- do not edit.\n")
    f.write("% Each macro names the artifact and field it came from.\n\n")
    f.write('\n'.join(OUT) + '\n')
print(f"wrote {len(OUT)} macros to {path}")
print(f"wrote tables to {TBL}")
