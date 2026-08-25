#!/usr/bin/env python3
"""Assemble the phase-2 ADVERSARY BANK: every adversary that will be evaluated.

Sources, in order:
  1. the fitted exploiter searches (research/v07/runs/p2-*.spec, one per row of
     p2_rows.tsv),
  2. the best arms of the unfitted screens (P1-screen.jsonl, P1b-screen.jsonl),
     selected by a rule fixed here rather than by eyeballing the table,
  3. CONSENSUS vectors -- the coordinate-wise median of the independent in-class
     fits.  Fifteen runs of one search are one run; but fifteen runs of one
     search do carry one thing a single run does not, which is the direction they
     agree on.  Each individual fit is a maximum over a noisy population and
     carries the winner's curse; the median across independent fits on disjoint
     banks does not.  If the median vector beats every individual fit, the
     mechanism is the agreement and not any one search,
  4. CROSSED arms -- the best in-class vector carried by the white-box class and
     by the search class.  This is the only way to ask whether two clusters
     compose or merely overlap, which is what "how much of the frontier's
     exploitability does each cluster account for" requires.
  5. the reference rungs, which are not adversaries but are what every severity
     number is read against.

Cluster labels are assigned from the row's declared hypothesis, not from its
score.  The phase brief: "Cluster by mechanism, not by score."
"""
import glob, json, os, re, sys
from statistics import median

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, '..', 'research', 'v07', 'results')
RUNS = os.path.join(HERE, '..', 'research', 'v07', 'runs')

CLUSTER_BY_KPI = {
    'win': None, 'setdiff': None,
    'declerr': 'urgency-declaration', 'declsupp': 'urgency-declaration',
    'asksupp': 'gate-ask-suppression', 'forced': 'forced-endgame',
    'events': 'stall-lengthen', 'limit': 'harness-actioncap',
}
CLUSTER_BY_CLASS = {'C1': 'inclass-linear', 'C2': 'extended-features'}

def cluster_for(row):
    """Mechanism cluster.  Structural switches in the base win over the objective,
    because a base carrying dead7 or corr is a different mechanism whatever it is
    scored on."""
    base = row.get('base', '')
    if 'dead7' in base and 'corr' in base: return 'deadask-x-correlated'
    if 'dead7' in base: return 'deadask-turnrouting'
    if 'corr' in base: return 'correlated-roles'
    if row.get('extra', '') and 'partners' in row['extra']: return 'oneseat-k1'
    c = CLUSTER_BY_KPI.get(row.get('kpi', 'win'))
    if c: return c
    if row.get('target') in ('Fcheap', 'Fmid', 'Fsearch'): return 'anti-search'
    if row.get('target') == 'FRONT': return 'frontier-wide'
    return CLUSTER_BY_CLASS.get(row.get('class', ''), 'inclass-linear')

def load_jsonl(name):
    p = os.path.join(RES, name)
    if not os.path.exists(p): return []
    out = []
    for l in open(p):
        l = l.strip()
        if l.startswith('{'):
            try: out.append(json.loads(l))
            except json.JSONDecodeError: pass
    return out

def vec(path):
    try: return [float(x) for x in open(path).read().strip().split('|')]
    except Exception: return None

def main():
    rows, seen = [], set()
    def add(i, cl, spec):
        if spec in seen: return
        seen.add(spec); rows.append((i, cl, spec))

    # ---- 1. the fitted searches --------------------------------------------
    fits = {f['id']: f for f in load_jsonl('P2-fits.jsonl') if f.get('battery') == 'P2fit'}
    for i in sorted(fits):
        p = os.path.join(RUNS, f'p2-{i}.spec')
        if os.path.exists(p): add(i, cluster_for(fits[i]), open(p).read().strip())

    # ---- 3. consensus vectors ----------------------------------------------
    for cls, base in (('C1', 'v06'), ('C2', 'v07')):
        vs = []
        for i, f in sorted(fits.items()):
            if f.get('class') != cls or f.get('target') != 'Ffast': continue
            v = vec(os.path.join(RUNS, f'p2-{i}.txt'))
            if v: vs.append(v)
        if len(vs) >= 3:
            L = min(len(v) for v in vs)
            med = [median(v[d] for v in vs) for d in range(L)]
            add(f'M{cls}', 'consensus-median',
                base + ':allparams=' + '|'.join(f'{x:.5f}' for x in med))

    # ---- 4. crossed arms ----------------------------------------------------
    # The best in-class vector by fitted score, carried by the other two classes.
    best = None
    for i, f in sorted(fits.items()):
        if f.get('class') == 'C1' and f.get('target') == 'Ffast' and f.get('kpi') == 'win':
            v = os.path.join(RUNS, f'p2-{i}.txt')
            if os.path.exists(v): best = (i, open(v).read().strip()); break
    if best:
        i, w = best
        add(f'{i}xC5', 'cross-inclass-whitebox', f'v07i:allparams={w},idet=48,imodel=v06')
        add(f'{i}xC3', 'cross-inclass-search',
            f'v06:allparams={w},s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26,roppo=v06')
        add(f'{i}xC3f', 'cross-inclass-search',
            f'v06:allparams={w},s1=1,det=16,cand=6,kappa=2.0,maxq=26,roppo=v06')

    # ---- 2. the best unfitted screen arms -----------------------------------
    # Selection rule, fixed in advance: within each screen cluster, keep every arm
    # whose measured edge on the screening bank is positive, plus the best arm of
    # the cluster whatever its sign, so that a cluster which found nothing is
    # still represented in the evaluation by its best attempt rather than being
    # silently dropped.
    scr = [r for r in load_jsonl('P1-screen.jsonl') + load_jsonl('P1b-screen.jsonl')
           if 'winRateA' in r]
    bycl = {}
    for r in scr:
        e = 100 * (r['winRateA'] - 0.5)
        bycl.setdefault(r['cluster'], []).append((e, r['a']))
    for cl, arms in sorted(bycl.items()):
        arms.sort(reverse=True)
        keep = [a for a in arms if a[0] > 0][:6] or arms[:1]
        if arms[0] not in keep: keep = [arms[0]] + keep
        for n, (e, spec) in enumerate(keep):
            add(f'S-{cl}-{n}', f'screen-{cl}', spec)

    # ---- 5. reference rungs -------------------------------------------------
    for i, cl, spec in [
        ('R-v05', 'reference', 'v05'),
        ('R-v04', 'reference', 'v04'),
        ('R-C5', 'whitebox-unfitted', 'v07i:idet=48,imodel=v06'),
        ('R-Fcheap', 'reference-frontier', 'v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26'),
        ('R-Fmid', 'reference-frontier', 'v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26'),
    ]: add(i, cl, spec)

    out = os.path.join(RES, 'p2-specs.tsv')
    with open(out, 'w') as f:
        f.write("# FishBot v0.7 phase-2 adversary bank.  id, mechanism cluster, spec.\n")
        for i, cl, spec in rows: f.write(f"{i}\t{cl}\t{spec}\n")
    print(f"wrote {out}: {len(rows)} adversaries")
    from collections import Counter
    for cl, n in sorted(Counter(r[1] for r in rows).items()): print(f"  {cl:<28} {n}")

if __name__ == '__main__': main()
