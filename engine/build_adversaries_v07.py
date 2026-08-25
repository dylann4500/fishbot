#!/usr/bin/env python3
"""FishBot v0.7 phase 2 -- generate every table in docs/v07/ADVERSARIES.md from the artifacts.

The project's standing convention is that no number in a deliverable is hand-typed
(engine/build_tables_v06.py, engine/build_tables_v07.py).  This script is that
convention applied to the adversary taxonomy: it reads research/v07/results/*.jsonl
and writes markdown fragments to research/v07/results/tables/, which ADVERSARIES.md
quotes verbatim.

Estimators
----------
Every cell is a win rate for the ADVERSARY seated as A against a frontier point
seated as B, over `deals` deals played at `rotations` orientations.  `edge` is
100*(winRate - 50%) in win-rate points.

`ci` in the artifact is the arena's deal-clustered percentile bootstrap: the
rotations of one deal are one correlated unit, so resampling deals rather than
games is the right cluster.  Pooling two banks combines the point estimates by
games and the half-widths in quadrature -- the two banks are independent draws of
the deal distribution, and this is the same estimator phase 1 used for its
excess-over-control column (RESEARCH-LOG.md 1.16).

`98/sqrt(N)` is the unpaired one-arm 95% half-width at p = 1/2, printed with every
cell because the phase brief requires the resolution to travel with the number.
"""
import json, math, os, re, sys, glob
from collections import defaultdict

RES = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'research', 'v07', 'results')
TAB = os.path.join(RES, 'tables')

def load(name):
    p = os.path.join(RES, name)
    if not os.path.exists(p): return []
    out = []
    for line in open(p):
        line = line.strip()
        if not line or not line.startswith('{'): continue
        try: out.append(json.loads(line))
        except json.JSONDecodeError: pass
    return out

def hw98(n):  return 98.0 / math.sqrt(n) if n else float('nan')

def cell(d):
    """One measured cell -> (edge_pts, halfwidth_pts, n_games)."""
    n = d['deals'] * d.get('rotations', 2)
    e = 100.0 * (d['winRateA'] - 0.5)
    lo, hi = d['ci']
    h = 100.0 * (hi - lo) / 2.0
    if not (h > 0):   # mirror cell: the bootstrap is degenerate, fall back on 98/sqrt(N)
        h = hw98(n)
    return e, h, n

def pool(cells):
    """Pool independent bank cells: games-weighted mean, half-widths in quadrature."""
    if not cells: return None
    N = sum(c[2] for c in cells)
    e = sum(c[0] * c[2] for c in cells) / N
    h = math.sqrt(sum((c[1] * c[2]) ** 2 for c in cells)) / N
    return e, h, N, len(cells)

def sgn(x): return 1 if x > 0 else (-1 if x < 0 else 0)

def fmt(e, h): return f"{e:+.2f} [{e-h:+.2f}, {e+h:+.2f}]"

def w(path, text):
    os.makedirs(TAB, exist_ok=True)
    with open(os.path.join(TAB, path), 'w') as f: f.write(text)
    print(f"  wrote tables/{path}")

# ------------------------------------------------------------------ P1 screen
def table_screen():
    rows = [r for r in load('P1-screen.jsonl') if r.get('battery') == 'P1screen']
    if not rows: return
    by = defaultdict(list)
    for r in rows: by[(r['cluster'], r['a'], r['targetTag'])].append(r)
    recs = []
    for (cl, spec, tt), rs in by.items():
        p = pool([cell(r) for r in rs])
        mirror = (spec == 'v06' and tt == 'Ffast')
        recs.append(dict(cluster=cl, spec=spec, target=tt, edge=p[0], hw=p[1], n=p[2],
                         banks=p[3], mirror=mirror,
                         declAccB=sum(r['declAccB'] for r in rs)/len(rs),
                         askAccB=sum(r['askAccB'] for r in rs)/len(rs),
                         declB=sum(r['declPerGameB'] for r in rs)/len(rs),
                         forcedB=sum(r.get('forcedPerGameA',0) for r in rs)/len(rs),
                         limit=sum(r['limitHitRate'] for r in rs)/len(rs),
                         events=sum(r['eventsPerGame'] for r in rs)/len(rs)))
    recs.sort(key=lambda r: -r['edge'])
    out = ["| adversary | cluster | edge vs `v06` (pts) | 95% CI | n | 98/&radic;N | target decl. acc. | target ask acc. | target decl./game | limit hits | events/game |",
           "|---|---|---:|---|---:|---:|---:|---:|---:|---:|---:|"]
    for r in recs:
        ci = "mirror &mdash; no interval" if r['mirror'] else f"[{r['edge']-r['hw']:+.2f}, {r['edge']+r['hw']:+.2f}]"
        out.append(f"| `{r['spec']}` | {r['cluster']} | {r['edge']:+.2f} | {ci} | {r['n']:,} | {hw98(r['n']):.2f} | "
                   f"{r['declAccB']:.4f} | {r['askAccB']:.4f} | {r['declB']:.2f} | {r['limit']:.4f} | {r['events']:.1f} |")
    w('P1-screen.md', "\n".join(out) + "\n")
    return recs

# ------------------------------------------------------------------ P4 floor
def table_floor():
    rows = [r for r in load('P4-floor.jsonl') if r.get('battery') == 'P4floor']
    if not rows: return
    dtrue, byclass = {}, defaultdict(lambda: defaultdict(list))
    for r in rows:
        if r['row'] == 'dTrue': dtrue[r['tag']] = cell(r)
        else: byclass[r['row']][r['tag']].append(cell(r))
    out = ["| class | rung | dTrue (pts) | dFound pooled | excess over control | excess &minus; dTrue | detected |",
           "|---|---|---:|---|---|---:|:--:|"]
    floors = {}
    for cls in sorted(byclass):
        ctrl = pool(byclass[cls].get('none', []))
        for tag in sorted(byclass[cls], key=lambda t: dtrue.get(t, (0,))[0]):
            p = pool(byclass[cls][tag])
            dt = dtrue.get(tag, (0.0, 0.0, 0))
            if tag == 'none':
                out.append(f"| {cls} | `none` (control) | 0.00 &mdash; mirror | {fmt(p[0], p[1])} | &mdash; | &mdash; | &mdash; |")
                continue
            ex = p[0] - ctrl[0]
            exh = math.hypot(p[1], ctrl[1])
            det = (ex - exh) > 0
            if det: floors.setdefault(cls, []).append(dt[0])
            out.append(f"| {cls} | `{tag}` | {dt[0]:+.2f} [{dt[0]-dt[1]:+.2f}, {dt[0]+dt[1]:+.2f}] | {fmt(p[0], p[1])} | "
                       f"{fmt(ex, exh)} | {ex-dt[0]:+.2f} | {'**yes**' if det else 'no'} |")
    out.append("")
    out.append("| class | detection floor at this sample size | evaluation games per bank |")
    out.append("|---|---:|---:|")
    for cls in sorted(byclass):
        n = pool(byclass[cls]['none'])[2] // 2 if byclass[cls].get('none') else 0
        if cls in floors:
            out.append(f"| {cls} | {min(floors[cls]):.2f} pts | {n:,} |")
        else:
            out.append(f"| {cls} | not established on the rungs run | {n:,} |")
    out.append("")
    rungs = sorted({t for c in byclass for t in byclass[c] if t != 'none'})
    out.append("Rungs run at this power: the false-positive control plus " + str(len(rungs)) +
               " planted rungs (" + ", ".join('`' + r + '`' for r in rungs) + "), spanning the "
               "declaration family and the readability rung phase 1 quoted its 1.68-point floor from.")
    w('P4-floor.md', "\n".join(out) + "\n")

# ------------------------------------------------------------------ P3 eval
def eval_records():
    rows = [r for r in load('P3-eval.jsonl') if r.get('battery') == 'P3eval']
    by = defaultdict(list)
    for r in rows: by[(r['id'], r['cluster'], r['targetTag'], r['k'])].append(r)
    recs = []
    for (i, cl, tt, k), rs in by.items():
        cs = [cell(r) for r in rs]
        p = pool(cs)
        recs.append(dict(id=i, cluster=cl, target=tt, k=k, edge=p[0], hw=p[1], n=p[2], banks=p[3],
                         perbank=[c[0] for c in cs], sameSign=len(set(sgn(c[0]) for c in cs)) == 1,
                         declAccA=sum(r['declAccA'] for r in rs)/len(rs),
                         forcedA=sum(r['forcedPerGameA'] for r in rs)/len(rs),
                         limit=sum(r['limitHitRate'] for r in rs)/len(rs),
                         declAccB=sum(r['declAccB'] for r in rs)/len(rs),
                         askAccB=sum(r['askAccB'] for r in rs)/len(rs),
                         spec=rs[0]['a']))
    return recs

def table_fitresults(N, floors):
    """What the fitted searches reached, joined to what each was fitted for."""
    fits = {f['id']: f for f in load('P2-fits.jsonl') if f.get('battery') == 'P2fit'}
    rows = [r for r in load('P3-eval.jsonl') if r.get('battery') == 'P3eval']
    if not rows: return
    by = defaultdict(list)
    for r in rows: by[(r['id'], r['cluster'])].append(r)
    out = ["| id | class | fitted against | objective | budget (games) | edge vs `v06` | 95% CI | per bank | n | clears its class floor |",
           "|---|---|---|---|---:|---:|---|---|---:|:--:|"]
    recs = []
    for (i, cl), rs in by.items():
        if i not in fits: continue        # only the FITTED searches belong in this table
        p_ = pool([cell(r) for r in rs])
        recs.append((p_[0], i, cl, fits[i], p_, [cell(r)[0] for r in rs]))
    recs.sort(reverse=True)
    for e, i, cl, f, p_, pb in recs:
        klass = f.get('class', '?')
        fl = floors.get(klass, float('nan'))
        clears = (p_[0] - p_[1]) > fl if fl == fl else False
        out.append(f"| {i} | {klass} | `{f.get('target','?')}` | `{f.get('kpi','?')}` | "
                   f"{int(f.get('fitGames', 0)):,} | {p_[0]:+.2f} | [{p_[0]-p_[1]:+.2f}, {p_[0]+p_[1]:+.2f}] | "
                   + " / ".join(f"{x:+.2f}" for x in pb) + f" | {p_[2]:,} | "
                   + ('**yes**' if clears else f'no ({fl:.2f})') + " |")
    w('P3-fitresults.md', "\n".join(out) + "\n")
    if recs:
        best = recs[0]
        N['fitBestId'] = best[1]
        N['fitBestEdge'] = f"{best[0]:+.2f}"
        N['fitBestCI'] = f"[{best[4][0]-best[4][1]:+.2f}, {best[4][0]+best[4][1]:+.2f}]"
        N['nFitsEvaluated'] = str(len(recs))
        N['nFitsClearing'] = str(sum(1 for e, i, cl, f, p_, pb in recs
                                     if (p_[0] - p_[1]) > floors.get(f.get('class', '?'), 1.53)))

def table_eval(floors):
    recs = eval_records()
    if not recs: return
    fits = {f['id']: f for f in load('P2-fits.jsonl') if f.get('battery') == 'P2fit'}
    for tt in sorted(set(r['target'] for r in recs)):
        sub = sorted([r for r in recs if r['target'] == tt and r['k'] == 'k3'], key=lambda r: -r['edge'])
        if not sub: continue
        out = [f"| id | cluster | hypothesis | edge vs `{tt}` | per bank | n | 98/&radic;N | floor | clears | responder decl. acc. | forced/game |",
               "|---|---|---|---|---|---:|---:|---:|:--:|---:|---:|"]
        for r in sub:
            f = fits.get(r['id'], {})
            fl = floors.get(r['cluster'], float('nan'))
            clears = (r['edge'] - r['hw']) > fl if fl == fl else False
            pb = " / ".join(f"{x:+.2f}" for x in r['perbank'])
            out.append(f"| {r['id']} | {r['cluster']} | {f.get('hypothesis','')} | {fmt(r['edge'], r['hw'])} | {pb} | "
                       f"{r['n']:,} | {hw98(r['n']):.2f} | {fl:.2f} | {'**yes**' if clears else 'no'} | "
                       f"{r['declAccA']:.4f} | {r['forcedA']:.4f} |")
        w(f'P3-eval-{tt}.md', "\n".join(out) + "\n")
    # the transfer matrix: every adversary against every frontier point it was measured on
    ids = sorted(set(r['id'] for r in recs))
    tts = [t for t in ['Ffast', 'Fcheap', 'Fmid', 'Fsearch'] if any(r['target'] == t for r in recs)]
    out = ["| id | cluster | " + " | ".join(f"vs `{t}`" for t in tts) + " |",
           "|---|---|" + "---|" * len(tts)]
    for i in ids:
        rs = {r['target']: r for r in recs if r['id'] == i and r['k'] == 'k3'}
        if not rs: continue
        cl = next(iter(rs.values()))['cluster']
        cells = []
        for t in tts:
            r = rs.get(t)
            cells.append(f"{r['edge']:+.2f} &plusmn;{r['hw']:.2f}" if r else "&mdash;")
        out.append(f"| {i} | {cl} | " + " | ".join(cells) + " |")
    w('P3-transfer.md', "\n".join(out) + "\n")
    return recs

# ------------------------------------------------------------------ mechanism
def table_mechanism():
    rows = [r for r in load('P5-mech.jsonl') if r.get('probe') == 'v7decide']
    if not rows: return
    keys = ['askHitRate', 'ownLockedAskRate', 'gateBindRate', 'deadAskRate', 'searchRate',
            'searchChangeRate', 'declAccuracy', 'declAllocErrorShare', 'forcedDeclAccuracy', 'tieShare']
    out = ["| adversary (arm A) | target (arm B, measured) | " + " | ".join(f"`{k}`" for k in keys) + " |",
           "|---|---|" + "---:|" * len(keys)]
    for r in rows:
        m = r['metrics']
        vals = []
        for k in keys:
            v = m.get(k)
            vals.append("&mdash;" if v is None else f"{v['rate']:.5f}")
        out.append(f"| `{r['a']}` | `{r['b']}` | " + " | ".join(vals) + " |")
    out.append("")
    out.append("Deal-clustered 95% intervals and decision counts, per cell:")
    out.append("")
    out.append("| adversary | metric | rate | 95% CI | decisions |")
    out.append("|---|---|---:|---|---:|")
    for r in rows:
        for k in keys:
            v = r['metrics'].get(k)
            if not v or not v['n']: continue
            out.append(f"| `{r['a']}` | `{k}` | {v['rate']:.5f} | [{v['ci'][0]:.4f}, {v['ci'][1]:.4f}] | {v['n']:.0f} |")
    w('P5-mech.md', "\n".join(out) + "\n")

# ------------------------------------------------------------------ inventory
def table_inventory(N):
    """The search inventory.  The phase brief's constraint is that the searches
    must not share a bias -- "fifteen runs of one search are one run" -- so the
    deliverable has to show the AXES, not the count."""
    rows = []
    tsv = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'p2_rows.tsv')
    if os.path.exists(tsv):
        for line in open(tsv):
            if line.startswith('#') or not line.strip(): continue
            f = line.rstrip('\n').split('\t')
            if len(f) < 11: continue
            rows.append(dict(id=f[0], cls=f[1], base=f[2], target=f[3], kpi=f[4], seed=f[5],
                             budget=f"{f[6]}x{f[7]}x{f[8]}",
                             extra=('' if f[9] == '-' else f[9]), hyp=f[10]))
    fits = {r['id']: r for r in load('P2-fits.jsonl') if r.get('battery') == 'P2fit'}
    out = ["| id | class | responder base | target | objective | fitting bank | budget (gens x pop x deals) | games spent | regime / seats | hypothesis |",
           "|---|---|---|---|---|---:|---|---:|---|---|"]
    for r in rows:
        f = fits.get(r['id'], {})
        g = f.get('fitGames', '')
        reg = 'A2 correlated' if 'correlated' in r['extra'] else ('k = 1' if 'partners' in r['extra'] else 'A1, k = 3')
        if 'NOFROMV6' in r['extra']: reg += ', v0.5 basin'
        if 'INITFROM' in r['extra']: reg += ', seeded at the denial direction'
        if 'sigmarel' in r['extra']: reg += ', wide sigma'
        out.append(f"| {r['id']} | {r['cls']} | `{r['base']}` | {r['target']} | `{r['kpi']}` | {r['seed']} | "
                   f"{r['budget']} | {g if g else '&mdash;'} | {reg} | {r['hyp']} |")
    w('P2-inventory.md', "\n".join(out) + "\n")
    N['nFitRows'] = str(len(rows))
    N['nFitsDone'] = str(len(fits))
    N['nFitObjectives'] = str(len(set(r['kpi'] for r in rows)))
    N['nFitBanks'] = str(len(set(r['seed'] for r in rows)))
    N['nFitTargets'] = str(len(set(r['target'] for r in rows)))
    tot = sum(int(f.get('fitGames', 0)) for f in fits.values())
    N['fitGamesTotal'] = f"{tot:,}"

# ------------------------------------------------------------------ ceilings
def table_ceilings(N):
    rows = [r for r in load('P7-ceilings.jsonl') if r.get('battery') == 'P7ceiling']
    if not rows: return
    ctrl = next((r for r in rows if r['tag'] == 'control'), None)
    out = ["| arm | target the incumbent plays | `v06` edge (pts) | 95% CI | target decl. acc. | target decl./game | target lock hold | n |",
           "|---|---|---:|---|---:|---:|---:|---:|"]
    for r in rows:
        e, h, n = cell(r)
        ci = "mirror &mdash; no interval" if r['a'] == r['b'] else f"[{e-h:+.2f}, {e+h:+.2f}]"
        out.append(f"| `{r['tag']}` | `{r['b']}` | {e:+.2f} | {ci} | {r['declAccB']:.4f} | "
                   f"{r['declPerGameB']:.2f} | {r['lockHoldB']:.2f} | {n:,} |")
        key = 'ceil_' + re.sub(r'[^A-Za-z0-9]', '', r['tag'])
        N[key] = f"{e:+.2f}"
        N[key + 'CI'] = f"[{e-h:+.2f}, {e+h:+.2f}]"
        N[key + 'DeclAccB'] = f"{r['declAccB']:.4f}"
    w('P7-ceilings.md', "\n".join(out) + "\n")
    if ctrl is not None:
        N['ceilControlDeclAccB'] = f"{ctrl['declAccB']:.4f}"

# ------------------------------------------------------------------ harness
def table_harness():
    rows = [r for r in load('P6-harness.jsonl')]
    if not rows: return
    out = ["| probe | A | B | win rate A | 95% CI | n | limit-hit rate | events/game | mean sets A | mean sets B |",
           "|---|---|---|---:|---|---:|---:|---:|---:|---:|"]
    for r in rows:
        if 'winRateA' not in r: continue
        n = r['deals'] * r.get('rotations', 2)
        out.append(f"| {r.get('probe','')} | `{r['a']}` | `{r['b']}` | {100*r['winRateA']:.2f}% | "
                   f"[{100*r['ci'][0]:.2f}, {100*r['ci'][1]:.2f}] | {n:,} | {r['limitHitRate']:.4f} | "
                   f"{r['eventsPerGame']:.1f} | {r['meanSetsA']:.3f} | {r['meanSetsB']:.3f} |")
    w('P6-harness.md', "\n".join(out) + "\n")

def numbers_from_mech(N):
    rows = [r for r in load('P5-mech.jsonl') if r.get('probe') == 'v7decide']
    for r in rows:
        tag = re.sub(r'[^A-Za-z0-9]', '', r.get('tag', ''))
        for k, v in r['metrics'].items():
            if not v.get('n'): continue
            N[f'mech_{tag}_{k}'] = f"{v['rate']:.5f}"
            N[f'mech_{tag}_{k}CI'] = f"[{v['ci'][0]:.4f}, {v['ci'][1]:.4f}]"
            N[f'mech_{tag}_{k}N'] = f"{v['n']:.0f}"

def numbers_from_harness(N):
    rows = [r for r in load('P6-harness.jsonl') if 'winRateA' in r]
    for r in rows:
        tag = re.sub(r'[^A-Za-z0-9]', '', r.get('probe', ''))
        N[f'harn_{tag}_win'] = f"{100*r['winRateA']:.2f}"
        N[f'harn_{tag}_limit'] = f"{r['limitHitRate']:.4f}"
        N[f'harn_{tag}_events'] = f"{r['eventsPerGame']:.1f}"
    f = next((r for r in rows if r.get('probe') == 'H1-forward'), None)
    g = next((r for r in rows if r.get('probe') == 'H1-reverse'), None)
    if f and g: N['armSwapSum'] = f"{100*(f['winRateA'] + g['winRateA']):.4f}"
    f = next((r for r in rows if r.get('probe') == 'H1-forward-k1'), None)
    g = next((r for r in rows if r.get('probe') == 'H1-reverse-k1'), None)
    if f and g: N['armSwapSumK1'] = f"{100*(f['winRateA'] + g['winRateA']):.4f}"
    # The limit-hit rate has to be read at the SHIPPED cap.  Rows that lower
    # `--maxasks` are the demonstration that the mechanism exists, not evidence
    # that it is reachable, and pooling the two would report the demonstration as
    # the finding.
    def shipped(r):
        f = r.get('flags', '')
        if '--maxasks=' not in f: return True
        try: return int(f.split('--maxasks=')[1].split()[0]) >= 400
        except (ValueError, IndexError): return True
    lim = [r['limitHitRate'] for r in rows if r.get('probe', '').startswith('H2') and shipped(r)]
    if lim: N['maxLimitHitRate'] = f"{max(lim):.4f}"
    ev = [r['eventsPerGame'] for r in rows if r.get('probe', '').startswith('H2')]
    if ev: N['maxEventsPerGame'] = f"{max(ev):.1f}"
    low = [r for r in rows if not shipped(r)]
    if low:
        b = max(low, key=lambda r: r['limitHitRate'])
        N['capLoweredTo'] = b['flags'].split('--maxasks=')[1].split()[0]
        N['capLoweredLimitRate'] = f"{b['limitHitRate']:.4f}"
        N['capLoweredWin'] = f"{100*b['winRateA']:.2f}"

def numbers_from_screen(N, recs):
    if not recs: return
    N['nScreenArms'] = str(len(recs))
    live = [r for r in recs if not r['mirror']]
    best = max(live, key=lambda r: r['edge']) if live else None
    if best:
        N['screenBestSpec'] = '`' + best['spec'] + '`'
        N['screenBestEdge'] = f"{best['edge']:+.2f}"
        N['screenBestCI'] = f"[{best['edge']-best['hw']:+.2f}, {best['edge']+best['hw']:+.2f}]"
        N['screenBestCluster'] = best['cluster']
    N['nScreenPositive'] = str(sum(1 for r in live if r['edge'] > 0))
    N['nScreenSeparated'] = str(sum(1 for r in live if r['edge'] - r['hw'] > 0))

# ------------------------------------------------------------ denial + severity
# Cluster labels name the MEASURED mechanism, not the one the coordinate was
# designed for.  The `oppCertDonate` group was built to express information
# denial and does not: the winning sign is the opposite of the design intent and
# the KPI profile is a tempo effect in the race to claim half-suits.  The label
# follows the measurement.
CLUSTER_OF = {
    'control': 'reference', 'search': 'search-strength', 'm2off': 'm2-defect',
    'm2off-r12': 'contestation-x-m2', 'search-r12': 'contestation-x-search',
    'search-m2': 'm2-x-search', 'composite': 'contestation-x-m2-x-search',
    'denial-all': 'contestation-group', 'r12-vs-Fcheap': 'contestation',
    'composite-vs-Fcheap': 'contestation-x-m2-x-search',
}
def cluster_of(tag):
    if tag in CLUSTER_OF: return CLUSTER_OF[tag]
    if tag[:3] in ('r12', 'r13', 'r14', 'r15'):
        return 'contestation'
    if tag.startswith('r16') or tag.startswith('r17'): return 'tally-inflation'
    return 'other'

def table_denial(N):
    rows = [r for r in load('P8-denial.jsonl') if r.get('battery') == 'P8denial']
    if not rows: return []
    by = defaultdict(list)
    for r in rows: by[(r['tag'], r['b'])].append(r)
    recs = []
    for (tag, tgt), rs in sorted(by.items()):
        cs = [cell(r) for r in rs]
        p = pool(cs)
        tt = 'Ffast' if tgt == 'v06' else ('Fcheap' if 'depth=12' in tgt else tgt)
        # The transfer rows carry their own tag ("r12-vs-Fcheap"); strip the
        # suffix so an arm's Ffast and Fcheap cells join into one frontier
        # profile instead of appearing as two unrelated adversaries.
        aid = tag[:-len('-vs-Fcheap')] if tag.endswith('-vs-Fcheap') else tag
        recs.append(dict(id=aid, cluster=cluster_of(aid), target=tt, k='k3',
                         edge=p[0], hw=p[1], n=p[2], banks=p[3], spec=rs[0]['a'],
                         perbank=[c[0] for c in cs],
                         sameSign=len(set(sgn(c[0]) for c in cs)) == 1,
                         declAccA=sum(r['declAccA'] for r in rs)/len(rs),
                         declAccB=sum(r['declAccB'] for r in rs)/len(rs),
                         lockHoldB=sum(r['lockHoldB'] for r in rs)/len(rs),
                         declPerGameB=sum(r['declPerGameB'] for r in rs)/len(rs),
                         askAccB=sum(r['askAccB'] for r in rs)/len(rs),
                         limit=sum(r['limitHitRate'] for r in rs)/len(rs),
                         forcedA=sum(r['forcedPerGameA'] for r in rs)/len(rs)))
    for r in recs:
        if r['id'] == 'r12': r['id'] = 'r12=25'
    ff = [r for r in recs if r['target'] == 'Ffast']
    ff.sort(key=lambda r: -r['edge'])
    out = ["| arm | cluster | edge vs `v06` | per bank | n | adversary decl. acc. | target decl. acc. | target lock hold | target decl./game | target ask acc. | limit hits |",
           "|---|---|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for r in ff:
        pb = " / ".join(f"{x:+.2f}" for x in r['perbank'])
        cell_ci = ("+0.00 mirror &mdash; no interval" if r['spec'] == 'v06' and r['id'] == 'control'
                   else fmt(r['edge'], r['hw']))
        out.append(f"| `{r['id']}` | {r['cluster']} | {cell_ci} | {pb} | {r['n']:,} | "
                   f"{r['declAccA']:.4f} | {r['declAccB']:.4f} | {r['lockHoldB']:.2f} | "
                   f"{r['declPerGameB']:.2f} | {r['askAccB']:.4f} | {r['limit']:.4f} |")
    w('P8-denial.md', "\n".join(out) + "\n")
    ctrl = next((r for r in ff if r['id'] == 'control'), None)
    if ctrl: N['denialControlDeclAccA'] = f"{ctrl['declAccA']:.4f}"
    best = max((r for r in ff if r['cluster'] == 'contestation'), key=lambda r: r['edge'], default=None)
    if best:
        N['denialBestArm'] = best['spec']
        N['denialBestTag'] = best['id']
        N['denialBestEdge'] = f"{best['edge']:+.2f}"
        N['denialBestCI'] = f"[{best['edge']-best['hw']:+.2f}, {best['edge']+best['hw']:+.2f}]"
        N['denialBestN'] = f"{best['n']:,}"
        N['denialBestDeclAccA'] = f"{best['declAccA']:.4f}"
        N['denialBestLockHoldB'] = f"{best['lockHoldB']:.2f}"
        N['denialBestDeclB'] = f"{best['declPerGameB']:.2f}"
    comp = next((r for r in ff if r['id'] == 'composite'), None)
    if comp:
        N['compositeEdge'] = f"{comp['edge']:+.2f}"
        N['compositeCI'] = f"[{comp['edge']-comp['hw']:+.2f}, {comp['edge']+comp['hw']:+.2f}]"
        N['compositeN'] = f"{comp['n']:,}"
    srch = next((r for r in ff if r['id'] == 'search'), None)
    if srch:
        N['searchBaselineEdge'] = f"{srch['edge']:+.2f}"
        N['searchBaselineCI'] = f"[{srch['edge']-srch['hw']:+.2f}, {srch['edge']+srch['hw']:+.2f}]"
    return recs

def table_severity(N, extra):
    """Severity per mechanism cluster, read against the WORST frontier point."""
    recs = list(extra)
    for r in load('P9-frontier.jsonl'):
        if r.get('battery') != 'P9frontier': continue
        recs.append(dict(id=r['id'], cluster=r['cluster'], target=r['targetTag'], k='k3',
                         _raw=r))
    # collapse P9 rows by (id,target)
    # The frontier battery names its arms independently of the dose battery; the
    # same adversary must not appear twice in a severity table.
    ALIAS = {'D-r12-25': 'r12=25', 'D-composite': 'composite',
             'D-r12-20': 'r12=20', 'M2-off': 'm2off', 'S-Fcheap': 'search'}
    byid = defaultdict(list)
    for r in load('P9-frontier.jsonl'):
        if r.get('battery') != 'P9frontier': continue
        byid[(ALIAS.get(r['id'], r['id']), r['cluster'], r['targetTag'])].append(r)
    recs = [r for r in recs if '_raw' not in r]
    have = {(r['id'], r['target']) for r in recs}
    for (i, cl, tt), rs in byid.items():
        if (i, tt) in have: continue          # the dose battery already measured this cell at higher n
        p = pool([cell(r) for r in rs])
        recs.append(dict(id=i, cluster=cl, target=tt, k='k3', edge=p[0], hw=p[1], n=p[2],
                         banks=p[3], spec=rs[0]['a'], perbank=[cell(r)[0] for r in rs],
                         declAccA=sum(r['declAccA'] for r in rs)/len(rs),
                         declAccB=sum(r['declAccB'] for r in rs)/len(rs),
                         lockHoldB=sum(r['lockHoldB'] for r in rs)/len(rs),
                         limit=sum(r['limitHitRate'] for r in rs)/len(rs)))
    if not recs: return
    tts = [t for t in ('Ffast', 'Fcheap', 'Fmid', 'Fsearch') if any(r['target'] == t for r in recs)]
    byarm = defaultdict(dict)
    meta = {}
    for r in recs:
        byarm[r['id']][r['target']] = r
        meta[r['id']] = r['cluster']
    # per-arm frontier profile
    out = ["| arm | cluster | " + " | ".join(f"vs `{t}`" for t in tts) + " | worst cell over the frontier |",
           "|---|---|" + "---|" * (len(tts) + 1)]
    prof = []
    # An arm earns a row here if its FRONTIER profile was measured (more than one
    # point), or if it is the best arm of its cluster against the deployed policy.
    # The dose-response ladder belongs in the dose table, not in the severity one.
    bestOf = {}
    for i, cells in byarm.items():
        r = cells.get('Ffast')
        if not r: continue
        c = meta[i]
        if c not in bestOf or r['edge'] > byarm[bestOf[c]]['Ffast']['edge']: bestOf[c] = i
    keep = {i for i, cells in byarm.items() if len(cells) > 1} | set(bestOf.values())
    rows_ = []
    for i, cells in ((k, v) for k, v in byarm.items() if k in keep):
        vals, have = [], []
        for t in tts:
            r = cells.get(t)
            if not r: vals.append("&mdash;"); continue
            mirror = (r.get('spec') == 'v06' and t == 'Ffast' and i == 'control')
            vals.append("mirror" if mirror else f"{r['edge']:+.2f} &plusmn;{r['hw']:.2f}")
            if not mirror: have.append(r['edge'])
        worst = min(have) if len(have) >= 2 else None
        key = worst if worst is not None else (cells.get('Ffast', {}).get('edge', -1e9) - 1e6)
        rows_.append((key, i, meta[i], vals, worst))
        prof.append((i, meta[i], worst, cells))
    # Sorted by the WORST cell over the frontier, which is what the section says.
    # Arms measured against only one frontier point sort below every arm with a
    # full profile, because a single cell is not a frontier severity.
    rows_.sort(key=lambda x: -x[0])
    for _, i, cl, vals, worst in rows_:
        out.append(f"| `{i}` | {cl} | " + " | ".join(vals) + " | " +
                   (f"**{worst:+.2f}**" if worst is not None else "&mdash; (one point only)") + " |")
    w('SEVERITY.md', "\n".join(out) + "\n")
    dom = [(w_, i, c) for i, c, w_, _ in prof if w_ is not None]
    if dom:
        w_, i, c = max(dom)
        N['frontierWorstBest'] = f"{w_:+.2f}"
        N['frontierWorstBestId'] = '`' + i + '`'
        N['frontierWorstBestCluster'] = c

# ------------------------------------------------------------------ P1b / P7b
def table_screen2():
    rows = [r for r in load('P1b-screen.jsonl') if r.get('battery') == 'P1bscreen']
    if not rows: return
    by = defaultdict(list)
    for r in rows: by[(r['cluster'], r['a'])].append(r)
    recs = []
    for (cl, spec), rs in by.items():
        p = pool([cell(r) for r in rs])
        recs.append((p[0], p[1], p[2], cl, spec, rs[0]))
    recs.sort(reverse=True)
    out = ["| adversary | cluster | edge vs `v06` | 95% CI | n | 98/&radic;N | target decl. acc. | target lock hold | events/game |",
           "|---|---|---:|---|---:|---:|---:|---:|---:|"]
    for e, h, n, cl, spec, r in recs:
        out.append(f"| `{spec}` | {cl} | {e:+.2f} | [{e-h:+.2f}, {e+h:+.2f}] | {n:,} | {hw98(n):.2f} | "
                   f"{r['declAccB']:.4f} | {r['lockHoldB']:.2f} | {r['eventsPerGame']:.1f} |")
    # The battery designs ~60 arms; if fewer are present it is still running and
    # the table must say so rather than let a short table read as a finished sweep.
    # The battery's designed arm count is read from its own script rather than
    # hard-coded, so this note cannot go stale the way a literal did.
    sc = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'adversaries2_v07.sh')
    designed = 0
    if os.path.exists(sc):
        body = open(sc).read()
        designed = len(re.findall(r'^\s*run [a-z0-9-]+ ', body, re.M))
        for m in re.finditer(r'^for \w+ in ([^;]+); do((?:.|\n)*?)^done', body, re.M):
            n = len(m.group(1).split())
            designed += (n - 1) * len(re.findall(r'run [a-z0-9-]+ ', m.group(2)))
    # Whether the battery is still producing is a fact about the machine, not
    # about the artifact, so it is read from the process table at build time
    # rather than inferred from a row count against a hard-coded design.
    live = os.system("pgrep -f 'bash ./adversaries2_v07.sh' >/dev/null 2>&1") == 0
    out.insert(0, f"> **{len(recs)} arms landed.**" + (
        "  The battery was **still running** when this was built; re-running the generator "
        "after it finishes splices in the rest." if live else "  The battery has finished."))
    out.insert(1, "")
    w('P1b-screen.md', "\n".join(out) + "\n")

def table_pressure(N):
    rows = [r for r in load('P7b-pressure.jsonl') if r.get('battery') == 'P7bpressure']
    if not rows: return
    out = ["| target | `v06` edge | 95% CI | target decl. acc. | target decl./game | target lock hold | events/game | n |",
           "|---|---:|---|---:|---:|---:|---:|---:|"]
    for r in rows:
        e, h, n = cell(r)
        ci = "mirror &mdash; no interval" if r['a'] == r['b'] else f"[{e-h:+.2f}, {e+h:+.2f}]"
        out.append(f"| `{r['b']}` | {e:+.2f} | {ci} | {r['declAccB']:.4f} | "
                   f"{r['declPerGameB']:.2f} | {r['lockHoldB']:.2f} | {r['eventsPerGame']:.1f} | {n:,} |")
    w('P7b-pressure.md', "\n".join(out) + "\n")
    for r in rows:
        e, h, n = cell(r)
        N['press_' + re.sub(r'[^A-Za-z0-9]', '', r['tag'])] = f"{e:+.2f}"

# ------------------------------------------------------------------ stack / mech
def table_stack(N):
    rows = [r for r in load('P10-stack.jsonl') if r.get('battery') == 'P10stack']
    if not rows: return
    by = defaultdict(list)
    for r in rows: by[r['a']].append(r)
    recs = []
    for spec, rs in by.items():
        p = pool([cell(r) for r in rs])
        recs.append((p[0], p[1], p[2], spec, [cell(r)[0] for r in rs], rs[0]))
    recs.sort(reverse=True)
    out = ["| configuration | edge over `v06` | 95% CI | per bank | n | its own decl. acc. | its own ask acc. |",
           "|---|---:|---|---|---:|---:|---:|"]
    for e, h, n, spec, pb, r in recs:
        ci = "mirror &mdash; no interval" if spec == r['b'] else f"[{e-h:+.2f}, {e+h:+.2f}]"
        out.append(f"| `{spec}` | {e:+.2f} | {ci} | " +
                   " / ".join(f"{x:+.2f}" for x in pb) + f" | {n:,} | {r['declAccA']:.4f} | {r['askAccA']:.4f} |")
    w('P10-stack.md', "\n".join(out) + "\n")
    if recs:
        e, h, n, spec, pb, r = recs[0]
        N['stackBestSpec'] = '`' + spec + '`'
        N['stackBestEdge'] = f"{e:+.2f}"
        N['stackBestCI'] = f"[{e-h:+.2f}, {e+h:+.2f}]"
        N['stackBestN'] = f"{n:,}"

def table_mechcontrols():
    rows = [r for r in load('P8b-mechanism.jsonl') if r.get('battery') == 'P8bmech']
    if not rows: return
    out = ["| control | edge vs `v06` | 95% CI | n | adversary ask acc. | adversary decl./game | adversary decl. acc. | adversary lock hold | target lock hold |",
           "|---|---:|---|---:|---:|---:|---:|---:|---:|"]
    for r in rows:
        e, h, n = cell(r)
        out.append(f"| `{r['a']}` | {e:+.2f} | [{e-h:+.2f}, {e+h:+.2f}] | {n:,} | {r['askAccA']:.4f} | "
                   f"{r['declPerGameA']:.2f} | {r['declAccA']:.4f} | {r['lockHoldA']:.2f} | {r['lockHoldB']:.2f} |")
    w('P8b-mechanism.md', "\n".join(out) + "\n")

# ------------------------------------------------------- cross-opponent profile
def table_crossopp(N):
    rows = [r for r in load('P8c-crossopp.jsonl') if r.get('battery') == 'P8ccross']
    if not rows: return
    opps, by = [], {}
    for r in rows:
        if r['b'] not in opps: opps.append(r['b'])
        by[(r['a'], r['b'])] = r
    arms = []
    for r in rows:
        if r['a'] not in arms: arms.append(r['a'])
    out = ["| opponent | " + " | ".join(f"`{a}`" for a in arms) + " | difference |",
           "|---|" + "---|" * (len(arms) + 1)]
    for o in opps:
        cells, vals = [], []
        for a in arms:
            r = by.get((a, o))
            if not r: cells.append("&mdash;"); continue
            e, h, n = cell(r)
            mirror = (a == o)
            cells.append("mirror" if mirror else f"{e:+.2f} &plusmn;{h:.2f}")
            vals.append(None if mirror else e)
        d = (vals[-1] - vals[0]) if len(vals) == 2 and None not in vals else None
        out.append(f"| `{o}` | " + " | ".join(cells) + " | " + (f"**{d:+.2f}**" if d is not None else "&mdash;") + " |")
    w('P8c-crossopp.md', "\n".join(out) + "\n")

def table_crossopp2(N):
    rows = [r for r in load('P8d-crossopp2.jsonl') if r.get('battery') == 'P8dcross2']
    if not rows: return
    by, by2 = defaultdict(list), defaultdict(list)
    for r in rows:
        by[(r['a'], r['b'])].append(cell(r))
        by2[(r['a'], r['b'])].append(r['meanSetsA'] - r['meanSetsB'])
    opps = []
    for r in rows:
        if r['b'] not in opps: opps.append(r['b'])
    out = ["| opponent | `v06` | `v07:r12=25` | increment | n a cell |",
           "|---|---:|---:|---:|---:|"]
    incs = []
    for o in opps:
        a = by.get(('v06', o)); b = by.get(('v07:r12=25', o))
        if not b: continue
        pb = pool(b)
        if o == 'v06':
            out.append(f"| `{o}` | mirror | {pb[0]:+.2f} &plusmn;{pb[1]:.2f} | {pb[0]:+.2f} | {pb[2]:,} |")
            incs.append((o, pb[0])); continue
        pa = pool(a)
        inc = pb[0] - pa[0]
        incs.append((o, inc))
        out.append(f"| `{o}` | {pa[0]:+.2f} &plusmn;{pa[1]:.2f} | {pb[0]:+.2f} &plusmn;{pb[1]:.2f} | "
                   f"**{inc:+.2f}** | {pb[2]:,} |")
    # The same table on the LINEAR scale.  A win rate is compressed near its
    # extremes: the same strength difference buys fewer points at 77.5% than at
    # 50%, so comparing an increment measured against a near-parity opponent with
    # one measured against a weak opponent is confounded by the operating point.
    # The mean half-suit differential is the corpus's own linear unit (the ledger
    # fits 1 unit = 14.7 win-rate points), and it is not compressed.
    out2 = ["| opponent | `v06` differential | `v07:r12=25` differential | increment (sets) | x 14.7 |",
            "|---|---:|---:|---:|---:|"]
    # The +/- columns are half the bank-to-bank range, not a bootstrap interval:
    # enough to show that the gradient across opponents is not resolved.
    for o in opps:
        a = by2.get(('v06', o)); b = by2.get(('v07:r12=25', o))
        if not b: continue
        sb = sum(b) / len(b)
        # A crude but honest spread: half the range across the two banks, so the
        # reader can see that the gradient is not resolved bank to bank.
        hb = (max(b) - min(b)) / 2.0 if len(b) > 1 else float('nan')
        if o == 'v06':
            out2.append(f"| `{o}` | mirror | {sb:+.3f} &plusmn;{hb:.3f} | {sb:+.3f} | {sb*14.7:+.2f} |")
            N['setIncr_' + re.sub(r'[^A-Za-z0-9]', '', o)] = f"{sb*14.7:+.2f}"
            continue
        sa = sum(a) / len(a)
        ha = (max(a) - min(a)) / 2.0 if len(a) > 1 else float('nan')
        out2.append(f"| `{o}` | {sa:+.3f} &plusmn;{ha:.3f} | {sb:+.3f} &plusmn;{hb:.3f} | "
                    f"**{sb-sa:+.3f}** | {(sb-sa)*14.7:+.2f} |")
        N['setIncr_' + re.sub(r'[^A-Za-z0-9]', '', o)] = f"{(sb-sa)*14.7:+.2f}"
    w('P8d-crossopp2-sets.md', "\n".join(out2) + "\n")
    w('P8d-crossopp2.md', "\n".join(out) + "\n")
    if incs:
        N['incrVsIncumbent'] = f"{dict(incs).get('v06', float('nan')):+.2f}"
        others = [v for k, v in incs if k != 'v06']
        if others:
            N['incrMinOther'] = f"{min(others):+.2f}"
            N['incrMaxOther'] = f"{max(others):+.2f}"
            N['incrMeanOther'] = f"{sum(others)/len(others):+.2f}"

# ------------------------------------------------------------- one-seat column
def table_oneseat(N, k3recs):
    rows = [r for r in load('P11-oneseat.jsonl') if r.get('battery') == 'P11oneseat']
    if not rows: return
    three = {}
    for r in (k3recs or []):
        if r.get('target') == 'Ffast': three[r['spec']] = r['edge']
    by = defaultdict(list)
    for r in rows: by[(r['id'], r['cluster'], r['a'])].append(r)
    out = ["| arm | cluster | k = 1 edge | 95% CI | n | k = 3 edge | k=1 share of k=3 |",
           "|---|---|---:|---|---:|---:|---:|"]
    for (i, cl, spec), rs in sorted(by.items()):
        p = pool([cell(r) for r in rs])
        e3 = three.get(spec)
        share = f"{100.0 * p[0] / e3:.0f}%" if (e3 and abs(e3) > 1e-9) else "&mdash;"
        out.append(f"| `{spec}` | {cl} | {p[0]:+.2f} | [{p[0]-p[1]:+.2f}, {p[0]+p[1]:+.2f}] | {p[2]:,} | "
                   + (f"{e3:+.2f}" if e3 is not None else "&mdash;") + f" | {share} |")
    w('P11-oneseat.md', "\n".join(out) + "\n")

# ------------------------------------------------------------- L7 / partners
def table_partners(N):
    rows = [r for r in load('P12-partners.jsonl') if r.get('battery') == 'P12partners']
    if not rows: return
    by = defaultdict(list)
    for r in rows: by[r['tag']].append(r)
    out = ["| arm | seats deviating | edge over `v06` | 95% CI | per bank | n | one-seat share of three |",
           "|---|---:|---:|---|---|---:|---:|"]
    vals = {}
    order = ['search3-cheap', 'search1-cheap', 'search3-mid', 'search1-mid',
             'contest3', 'contest1', 'noURG3', 'noURG1']
    for tag in [t for t in order if t in by] + [t for t in by if t not in order]:
        rs = by[tag]
        p = pool([cell(r) for r in rs])
        vals[tag] = p[0]
        k = 1 if tag.endswith('1') or 'search1' in tag else 3
        share = ''
        if k == 1:
            base = vals.get(tag.replace('1', '3'))
            if base and abs(base) > 1e-9: share = f"{100.0 * p[0] / base:.0f}%"
        out.append(f"| `{tag}` | {k} | {p[0]:+.2f} | [{p[0]-p[1]:+.2f}, {p[0]+p[1]:+.2f}] | " +
                   " / ".join(f"{cell(r)[0]:+.2f}" for r in rs) + f" | {p[2]:,} | {share or '&mdash;'} |")
    w('P12-partners.md', "\n".join(out) + "\n")
    for k, v in vals.items(): N['l7_' + re.sub(r'[^A-Za-z0-9]', '', k)] = f"{v:+.2f}"

# ------------------------------------------------------------- negative dose
def table_negdose(N):
    rows = [r for r in load('P8e-negdose.jsonl') if r.get('battery') == 'P8enegdose']
    if not rows: return
    by = defaultdict(list)
    for r in rows: by[r['tag']].append(cell(r))
    out = ["| dose | edge vs `v06` | 95% CI | per bank | n |", "|---|---:|---|---|---:|"]
    for tag in sorted(by, key=lambda t: float(t.split('=')[1])):
        p_ = pool(by[tag])
        out.append(f"| `{tag}` | {p_[0]:+.2f} | [{p_[0]-p_[1]:+.2f}, {p_[0]+p_[1]:+.2f}] | " +
                   " / ".join(f"{c[0]:+.2f}" for c in by[tag]) + f" | {p_[2]:,} |")
        N['neg_' + tag.replace('=', '').replace('-', 'm')] = f"{p_[0]:+.2f}"
    w('P8e-negdose.md', "\n".join(out) + "\n")

# ---------------------------------------------------- commit-gate digests
def numbers_from_gate2(N):
    """Parse research/v07/results/P0-gate2.txt -- the single-base commit-gate
    digests, 400 deals x 2 at seed 31.  The earlier P0-gate.txt blocks were
    tail-trimmed and lost the dead-ask lines, and quoting misdeclaration rates
    from runs at three different sample sizes made a 0.57pp run-to-run spread
    look like a finding."""
    fp = os.path.join(RES, 'P0-gate2.txt')
    if not os.path.exists(fp): return
    KEY = {'v06': 'v06',
           'v07:r12=25': 'r12',
           'v06:pool=-1,oppfloor=-1,force=1000000,askfloor=-1': 'noUrg',
           'v06:m2=0,pool=-1,oppfloor=-1,force=1000000,askfloor=-1': 'noUrgM2',
           'v06:rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1': 'rtieNoUrg',
           'v07:m2=0,r12=25,s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26': 'composite'}
    cur = None
    for line in open(fp):
        m = re.match(r'=== mirror: (.*) ===', line.strip())
        if m: cur = KEY.get(m.group(1)); continue
        if not cur: continue
        m = re.search(r'DEAD asks\s+\d+\s+\(([\d.eE+-]+)% of asks\)', line)
        if m: N[f'gate2_{cur}_dead'] = f"{float(m.group(1)):.4f}%"
        m = re.search(r'declarations\s+\d+\s+wrong \d+ \(([\d.]+)%\)', line)
        if m: N[f'gate2_{cur}_decl'] = f"{float(m.group(1)):.3f}%"
        m = re.search(r'events/game\s+([\d.]+).*max (\d+)', line)
        if m:
            N[f'gate2_{cur}_events'] = f"{float(m.group(1)):.1f}"
            N[f'gate2_{cur}_maxEvents'] = m.group(2)

# ------------------------------------------------------------------ seal
def table_seal(N):
    b = os.path.join(RES, '..', 'banks')
    sj = os.path.join(b, 'SEAL.json')
    if not os.path.exists(sj): return
    S = json.load(open(sj))
    out = []
    for half in ('train', 'holdout'):
        bj = os.path.join(b, half, 'BANKS.json')
        if not os.path.exists(bj): continue
        M = json.load(open(bj))
        out.append(f"**{half} half** — {len(M['banks'])} banks, written at commit `{M.get('commit','')[:12]}`.")
        out.append("")
        out.append("| seed | deals | role | unseal phase | commitment digest | note |")
        out.append("|---:|---:|---|---:|---|---|")
        for x in M['banks']:
            out.append(f"| {x['seed']} | {x['deals']:,} | {x['role']} | {x.get('unsealPhase', 0)} | "
                       f"`{x['digest']}` | {x['note']} |")
        out.append("")
    out.append(f"The adversary bank is split {S.get('adversariesTrain','?')} / "
               f"{S.get('adversariesHoldout','?')} by a rule fixed before any result was known — rows "
               f"sorted by id, alternating. The sealed half's plaintext SHA-256 is "
               f"`{S.get('adversariesHoldoutSha256','')[:32]}…`, recorded in the file header and in "
               f"`SEAL.json`, so phase 5 can verify it was not changed.")
    w('SEAL.md', "\n".join(out) + "\n")
    N['nTrainBanks'] = str(len(S.get('trainBanks', [])))
    N['nHoldoutBanks'] = str(len(S.get('holdoutBanks', [])))
    N['nAdvTrain'] = str(S.get('adversariesTrain', 0))
    N['nAdvHoldout'] = str(S.get('adversariesHoldout', 0))

# ------------------------------------------------------------------ class ladder
def table_classladder(N, den):
    """The responder classes against the unhandicapped incumbent, phase 1 beside phase 2."""
    rows = [r for r in load('P4-floor.jsonl')
            if r.get('battery') == 'P4floor' and r.get('tag') == 'none' and r['row'] != 'dTrue']
    out = ["| class | phase 1, 24,000 games | phase 2, this battery | n | what it is |",
           "|---|---|---|---:|---|"]
    P1 = {'C1': '+0.76 [+0.15, +1.37]', 'C2': '+1.05 [+0.43, +1.66]',
          'C5': '+1.52 [+0.92, +2.13]', 'C3': '+1.86 [+0.78, +2.94]'}
    WHAT = {'C1': 'the incumbent\'s own 37-coordinate family, refit and seeded at the incumbent',
            'C2': 'C1 plus the v0.7 responder coordinates',
            'C5': 'the incumbent with its posterior sharpened by inverting the target\'s transcript',
            'C3': 'the v0.6 rollout with the target\'s policy in the rollout\'s opposing seats'}
    by = defaultdict(list)
    for r in rows: by[r['row']].append(cell(r))
    for cls in ('C1', 'C2', 'C3', 'C5'):
        if cls in by:
            p = pool(by[cls])
            got = fmt(p[0], p[1]); n = f"{p[2]:,}"
            N['class' + cls] = f"{p[0]:+.2f}"
        else:
            got = '&mdash; (not re-run)'; n = '&mdash;'
        out.append(f"| {cls} | {P1[cls]} | {got} | {n} | {WHAT[cls]} |")
    w('CLASSLADDER.md', "\n".join(out) + "\n")

# ------------------------------------------------------------------ composition
def table_composition(N, den, ev):
    recs = list(den or []) + list(ev or [])
    ff = [r for r in recs if r.get('target') == 'Ffast' and r.get('k', 'k3') == 'k3']
    if not ff: return
    best = max(r['edge'] for r in ff)
    by = defaultdict(list)
    for r in ff: by[r['cluster']].append(r)
    out = ["| cluster | best arm | edge vs `v06` | 95% CI | share of the best measured | replicated in sign |",
           "|---|---|---:|---|---:|:--:|"]
    for cl, rs in sorted(by.items(), key=lambda kv: -max(r['edge'] for r in kv[1])):
        b = max(rs, key=lambda r: r['edge'])
        share = 100.0 * b['edge'] / best if best > 0 else float('nan')
        out.append(f"| {cl} | `{b.get('spec', b['id'])}` | {b['edge']:+.2f} | "
                   f"[{b['edge']-b['hw']:+.2f}, {b['edge']+b['hw']:+.2f}] | "
                   f"{share:.0f}% | {'yes' if b.get('sameSign') else 'n/a'} |")
    w('COMPOSITION.md', "\n".join(out) + "\n")
    N['bestMeasuredFfast'] = f"{best:+.2f}"

def main():
    os.makedirs(TAB, exist_ok=True)
    print("building phase-2 tables from", os.path.abspath(RES))
    N = {}
    recs = table_screen()
    numbers_from_screen(N, recs)
    table_inventory(N)
    table_ceilings(N)
    numbers_from_mech(N)
    numbers_from_harness(N)
    numbers_from_gate2(N)
    table_negdose(N)
    table_floor()
    # detection floors at phase-2 sample sizes, read back out of the floor artifact
    floors = {}
    p = os.path.join(TAB, 'P4-floor.md')
    if os.path.exists(p):
        for line in open(p):
            parts = [x.strip() for x in line.strip().strip('|').split('|')]
            if len(parts) == 3 and parts[1].endswith('pts'):
                try:
                    v = float(parts[1].split()[0])
                    if v == v: floors[parts[0]] = v      # NaN = no rung detected yet
                except ValueError: pass
    # Classes that were not re-floored at phase-2 power inherit phase 1's measured
    # floor, and the document must say which is which: only C1 was re-floored.
    P1 = {'C1': 1.68, 'C2': 2.31, 'C3': 2.45, 'C5': 1.68}
    N['refloored'] = ", ".join(sorted(floors)) if floors else "none"
    for k, v in P1.items():
        N['floorPhase1' + k] = f"{v:.2f}"
        floors.setdefault(k, v)
    json.dump(floors, open(os.path.join(TAB, 'floors.json'), 'w'), indent=1)
    for k, v in floors.items(): N['floor' + k] = f"{v:.2f}"
    den = table_denial(N)
    table_fitresults(N, floors)
    ev = table_eval(floors)
    table_severity(N, den or [])
    table_screen2()
    table_pressure(N)
    table_stack(N)
    table_mechcontrols()
    table_crossopp(N)
    table_crossopp2(N)
    table_partners(N)
    table_oneseat(N, den)
    table_seal(N)
    table_classladder(N, den)
    table_composition(N, den, ev)
    table_mechanism()
    table_harness()
    if ev:
        N['nAdversariesEvaluated'] = str(len(set(r['id'] for r in ev)))
        for tt in sorted(set(r['target'] for r in ev)):
            sub = [r for r in ev if r['target'] == tt and r['k'] == 'k3']
            if not sub: continue
            b = max(sub, key=lambda r: r['edge'])
            N[f'best_{tt}_id'] = b['id']
            N[f'best_{tt}_cluster'] = b['cluster']
            N[f'best_{tt}_edge'] = f"{b['edge']:+.2f}"
            N[f'best_{tt}_ci'] = f"[{b['edge']-b['hw']:+.2f}, {b['edge']+b['hw']:+.2f}]"
            N[f'best_{tt}_n'] = f"{b['n']:,}"
            N[f'n_{tt}_clearing'] = str(sum(
                1 for r in sub if (r['edge'] - r['hw']) > floors.get(r['cluster'], 1.68)))
            N[f'n_{tt}_positive'] = str(sum(1 for r in sub if r['edge'] > 0))
        # frontier-dominating severity: the worst cell over the frontier points
        fp = [t for t in ('Ffast', 'Fmid') if any(r['target'] == t for r in ev)]
        if len(fp) == 2:
            best_dom, best_id = -1e9, None
            for i in sorted(set(r['id'] for r in ev)):
                cells = {r['target']: r for r in ev if r['id'] == i and r['k'] == 'k3'}
                if not all(t in cells for t in fp): continue
                m = min(cells[t]['edge'] for t in fp)
                if m > best_dom: best_dom, best_id = m, i
            if best_id:
                N['frontierDomBest'] = f"{best_dom:+.2f}"
                N['frontierDomBestId'] = best_id
    json.dump(N, open(os.path.join(TAB, 'numbers.json'), 'w'), indent=1, sort_keys=True)
    print(f"wrote tables/numbers.json ({len(N)} macros)")
    print("floors:", floors)

if __name__ == '__main__':
    main()
