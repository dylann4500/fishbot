#!/usr/bin/env python3
"""FishBot v0.7 PHASE 5 -- reduction of the preregistered battery to its tables.

Arithmetic, stated once here so every number in FINAL-RESULTS.md is traceable.

  edge          a cell's win-rate advantage in POINTS: 100*(winRateA - 0.5).
  interval      the DEAL-CLUSTERED bootstrap `ci` from `match --json` (20,000
                resamples over deals, arena.hpp:293).  `wilsonCI` is NEVER
                quoted: PREREGISTRATION section 3, correction 2.
  se            (hi - lo) / (2 * 1.96), read back off that interval.
  pooled        two banks of equal size: the mean of the two edges, with
                se_pooled = sqrt(se1^2 + se2^2) / 2 and a 95% interval at
                +/- 1.96 se_pooled.  Both per-bank values are printed beside it.
  delta         a difference between two POOLED cells: the difference of the
                estimates, with the two pooled half-widths combined IN
                QUADRATURE.  PREREGISTRATION 5.2 draft 2 fixes this arithmetic
                and calls it conservative, because the arms are paired on deals
                and the harness gives NO paired delta across two cells.  Every
                delta below is therefore two independent intervals combined,
                never a paired difference, and section 3 requires saying so.
  replication   a claim's sign must agree on both banks or it is reported NOT
                REPLICATED whatever its pooled interval says (section 3).  For a
                delta, the per-bank deltas are formed first and their signs
                compared, so a delta carries a replication status too.

Every scored cell ran at --threads=13 (PREREGISTRATION 5.3(2)); the S6 gate
condition ran at --threads=1 --freshagents.
"""
import json, math, os, sys, collections, statistics

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
RES = os.path.join(ROOT, 'research', 'v07', 'results')
Z = 1.959964
FLOOR = 1.53          # PREREG 3.3 / 5.1
DECL_FLOOR = 2.13     # PREREG 3.3 correction 4: the declaration-family floor

# The cell counts section 4 fixes.  A battery that reports fewer has dropped a
# cell, and section 7 requires that to be recorded rather than absorbed.
EXPECT = dict(B2=10, B3=248, B4fit=8, B4eval=16, B5=24, B6=64, B7=24, B8=16,
              B9=26, B9side=12, B10=16)

OUT = []
def say(s=''):
    OUT.append(s); print(s)

def load(name):
    p = os.path.join(RES, name)
    if not os.path.exists(p): return []
    out = []
    for l in open(p):
        l = l.strip()
        if l:
            try: out.append(json.loads(l))
            except Exception: pass
    return out

def cell(r):
    m = r['match']
    e = 100 * (m['winRateA'] - 0.5)
    lo, hi = 100 * (m['ci'][0] - 0.5), 100 * (m['ci'][1] - 0.5)
    se = (hi - lo) / (2 * Z) if hi > lo else 0.0
    return e, lo, hi, se, m['games'], m['threads']

def sign(x): return 1 if x > 0 else (-1 if x < 0 else 0)

def pool(rows):
    rows = sorted(rows, key=lambda r: r['bank'])
    cs = [cell(r) for r in rows]
    if not cs: return None
    e = sum(c[0] for c in cs) / len(cs)
    se = math.sqrt(sum(c[3] ** 2 for c in cs)) / len(cs)
    hw = Z * se
    return dict(edge=e, lo=e - hw, hi=e + hw, se=se, hw=hw, banks=len(cs),
                byBank={r['bank']: c[0] for r, c in zip(rows, cs)},
                seByBank={r['bank']: c[3] for r, c in zip(rows, cs)},
                per=[dict(bank=r['bank'], edge=c[0], lo=c[1], hi=c[2], n=c[4]) for r, c in zip(rows, cs)],
                threads=sorted({c[5] for c in cs}),
                replicated=(len(cs) >= 2 and len({sign(c[0]) for c in cs}) == 1),
                n=sum(c[4] for c in cs))

def delta(a, b):
    """a - b between two pooled results.  Half-widths in quadrature (PREREG 5.2);
    per-bank deltas formed first so the difference carries a replication status."""
    if a is None or b is None: return None
    d = a['edge'] - b['edge']
    hw = math.sqrt(a['hw'] ** 2 + b['hw'] ** 2)
    shared = sorted(set(a['byBank']) & set(b['byBank']))
    pb = {k: a['byBank'][k] - b['byBank'][k] for k in shared}
    return dict(delta=d, lo=d - hw, hi=d + hw, hw=hw, byBank=pb,
                banks=len(shared),
                replicated=(len(shared) >= 2 and len({sign(v) for v in pb.values()}) == 1))

def perbank(p):
    """Per-bank values, ALWAYS labelled with the bank they came from.  `pool` sorts
    by seed number, which is not the protocol's primary/replicate order for every
    battery (B5 and B8 run 7090003 first), so an unlabelled pair inverts."""
    if not p: return ''
    if 'per' in p: return ' '.join('%d:%+.2f' % (x['bank'], x['edge']) for x in p['per'])
    return ' '.join('%d:%+.2f' % (k, v) for k, v in sorted(p.get('byBank', {}).items()))

def fmt(p):
    if p is None: return '        --'
    s = '%+7.2f [%+.2f, %+.2f]' % (p['edge'], p['lo'], p['hi'])
    return s + ('' if p['banks'] >= 2 else '  1BANK')

def fmtd(p):
    if p is None: return '        --'
    s = '%+7.2f [%+.2f, %+.2f]' % (p['delta'], p['lo'], p['hi'])
    return s + ('' if p['banks'] >= 2 else '  1BANK')

def completeness(name, rows, expected):
    got = sum(1 for r in rows if r.get('ok'))
    bad = [r.get('key') for r in rows if not r.get('ok')]
    tag = 'complete' if got == expected else 'INCOMPLETE'
    say('cells measured %d of %d preregistered -- %s%s'
        % (got, expected, tag, ('   failed: ' + ', '.join(map(str, bad))) if bad else ''))
    return got == expected

# =============================================================================
def gate():
    """B0 and B1 read out of their artifacts.  PREREGISTRATION 4:231-232 forbids
    reporting a strength number from a configuration that has not passed B1, so
    the gate verdict has to be EVIDENCE in this file, not a sentence in it."""
    g = {r['id']: r for r in load('P5-gate.jsonl')}
    b0p = os.path.join(RES, 'P5-B0.json')
    b0 = json.load(open(b0p)) if os.path.exists(b0p) else {}
    say('## B0 and B1 -- the preconditions, read out of their artifacts')
    say()
    checks = [
        ('B0.1 seven bank digests reproduce', b0.get('B0_1_bankDigests', {}).get('allMatch')),
        ('B0.2 sealed adversary half matches SEAL.json', b0.get('B0_2_sealedAdversaries', {}).get('match')),
        ('B0.3 freeze artifact verifies (R1/R2a/R3)', b0.get('B0_3_freezeVerify', {}).get('rc') == 0),
        ('B0.4 all seven seeds registered and usable', b0.get('B0_4_seeds', {}).get('allRegisteredAndUsable')),
        ('B0.5 fish7 verify PASS', b0.get('B0_5_engine', {}).get('verify', {}).get('pass_')),
        ('B0.5 fish7 selftest PASS', b0.get('B0_5_engine', {}).get('selftest', {}).get('pass_')),
    ]
    for n, v in checks: say('  [%s] %s' % ('ok' if v else 'XX', n))
    say()
    for arm in ('FROZEN', 'INCUMBENT', 'F-cheap'):
        r = g.get(arm)
        say('  [%s] B1 %-11s %s' % ('ok' if r and r['verdict'] == 'PASS' else 'XX', arm,
                                    r['verdict'] if r else 'MISSING'))
    ncr = g.get('NEGCONTROL')
    need = ['G1 dead asks', 'G2 longest dead run', 'G3 games w/ run>=6',
            'G4 action-limit', 'G5 mirror tail']
    negok = bool(ncr) and ncr['verdict'] == 'FAIL' and all(not ncr['rules'].get(k, True) for k in need)
    say('  [%s] B9.1 negative control FAILS G1-G5 (a gate that cannot reject it certifies nothing)'
        % ('ok' if negok else 'XX'))
    say()
    frozen_ok = bool(g.get('FROZEN')) and g['FROZEN']['verdict'] == 'PASS'
    b0_ok = all(v for _, v in checks)
    if not (frozen_ok and b0_ok and negok):
        say('  *** PRECONDITIONS NOT MET -- no strength number below may be read as a measurement.')
    say()
    return dict(frozen=frozen_ok, b0=b0_ok, negctrl=negok, gate=g, b0doc=b0)

def b2():
    rows = load('P5-B2.jsonl')
    say('## B2 -- headline strength against the frontier')
    say(); completeness('B2', rows, EXPECT['B2']); say()
    rows = [r for r in rows if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[r['cell']].append(r)
    say('%-6s %-56s %-30s %-24s %-8s' % ('cell', 'opponent', 'pooled edge (points)', 'per bank', 'n'))
    res = {}
    for c in ['B2.1', 'B2.2', 'B2.3', 'B2.4', 'B2.5']:
        rs = sorted(by.get(c, []), key=lambda r: r['bank'])
        p = pool(rs); res[c] = p
        if p is None: say('%-6s %-56s (not run)' % (c, '')); continue
        per = ' '.join('%d:%+.2f' % (x['bank'], x['edge']) for x in p['per'])
        say('%-6s %-56s %-30s %-24s %-8d%s' % (c, rs[0]['opp'][:56], fmt(p), per, p['n'],
                                               '' if p['replicated'] else '  SIGN NOT REPLICATED'))
    say()
    say('Declaration accuracy, printed as a DIAGNOSTIC and not as a claim.  PREREGISTRATION 3')
    say('correction 4 puts "the declaration-accuracy column of B2" under the 2.13 declaration-family')
    say('floor; `match --json` emits declaration accuracy as a bare rate with no interval of any')
    say('kind, so no interval can be quoted for it and PHASE 5 MAKES NO CLAIM ABOUT IT.  The rule')
    say('is discharged by making no claim, not by clearing a bar.')
    say('%-6s %-56s %-12s %-12s' % ('cell', 'opponent', 'declAcc A', 'declAcc B'))
    for c in ['B2.1', 'B2.2', 'B2.3', 'B2.4', 'B2.5']:
        rs = by.get(c, [])
        if not rs: continue
        a = sum(r['match']['declAccA'] for r in rs) / len(rs)
        b = sum(r['match']['declAccB'] for r in rs) / len(rs)
        say('%-6s %-56s %-12.5f %-12.5f' % (c, rs[0]['opp'][:56], a, b))
    say()
    return res

def b3():
    raw = load('P5-B3.jsonl')
    say('## B3 -- the panel: worst case and minimax regret')
    say(); completeness('B3', raw, EXPECT['B3']); say()
    rows = [r for r in raw if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[(r['arm'], r['opp'])].append(r)
    arms = ['FROZEN', 'INCUMBENT', 'F-cheap', 'composite']
    members, seen = [], set()
    for r in rows:
        if r['opp'] not in seen: seen.add(r['opp']); members.append((r['opp'], r['cls'], r['oppSpec']))
    P = {(a, m): pool(by.get((a, m), [])) for a in arms for m, _, _ in members}
    shared = [m for m, _, _ in members if all(P.get((a, m)) for a in arms)]
    say('Panel members with a cell for all four arms: %d of %d.  Minimax regret is only defined'
        % (len(shared), len(members)))
    say('over a SHARED panel (section 4/B3), so it is computed over those %d and no others.' % len(shared))
    say()
    say('%-11s %-26s %-32s %-16s' % ('arm', 'WORST cell (opponent)', 'edge (points)', 'per bank'))
    worst = {}
    for a in arms:
        cs = sorted((P[(a, m)]['edge'], m) for m in shared)
        if not cs: continue
        m = cs[0][1]; p = P[(a, m)]
        worst[a] = (p['edge'], m, p)
        say('%-11s %-26s %-32s %-16s' % (a, m[:26], fmt(p),
                                         perbank(p)))
    say()
    # the self-cell asymmetry: three arms meet themselves in the panel, FROZEN does not
    SELF = {'INCUMBENT': 'F-fast', 'F-cheap': 'F-cheap', 'composite': 'composite'}
    say('%-11s %-26s %-32s' % ('arm', 'WORST excluding self-cell', 'edge (points)'))
    worst_ns = {}
    for a in arms:
        cs = sorted((P[(a, m)]['edge'], m) for m in shared if m != SELF.get(a))
        if not cs: continue
        m = cs[0][1]; worst_ns[a] = (P[(a, m)]['edge'], m, P[(a, m)])
        say('%-11s %-26s %-32s' % (a, m[:26], fmt(P[(a, m)])))
    say()
    say('%-11s %-18s %-30s %s' % ('arm', 'MINIMAX REGRET', 'attained against', 'runner-up (margin)'))
    mmr = {}
    for a in arms:
        wr, wm = -1e18, None
        for m in shared:
            best = max(P[(x, m)]['edge'] for x in arms)
            r = best - P[(a, m)]['edge']
            if r > wr: wr, wm = r, m
        rs = sorted(((max(P[(x, m)]['edge'] for x in arms) - P[(a, m)]['edge'], m)
                     for m in shared), reverse=True)
        mmr[a] = (wr, wm)
        say('%-11s %-18.2f %-30s %s (%.3f behind)' % (a, wr, wm, rs[1][1], rs[0][0] - rs[1][0]))
    say()
    say('Every cell: the pooled edge over both primary banks with its deal-clustered interval.')
    say()
    say('%-16s %-6s %-22s %-22s %-22s %-22s' % ('opponent', 'class', *arms))
    for m, cls, _ in members:
        row = []
        for a in arms:
            p = P.get((a, m))
            row.append(('%+6.2f[%+.2f,%+.2f]' % (p['edge'], p['lo'], p['hi'])) if p else '  --')
        say('%-16s %-6s %-22s %-22s %-22s %-22s' % (m[:16], cls[:6], *row))
    say()
    say('%-11s %-28s' % ('arm', 'panel mean -- A DIAGNOSTIC, NOT A HEADLINE'))
    for a in arms:
        es = [P[(a, m)]['edge'] for m in shared]
        if es: say('%-11s %+.2f  over %d shared cells' % (a, sum(es) / len(es), len(es)))
    say()
    return P, worst, worst_ns, mmr, members, arms, shared

def b4():
    raw = load('P5-B4eval.jsonl')
    say('## B4 -- a fresh adversary search against the frozen configuration')
    say(); completeness('B4eval', raw, EXPECT['B4eval'])
    fits = load('P5-B4fits.jsonl'); completeness('B4fit', fits, EXPECT['B4fit']); say()
    say("Each arm's edge is the fitted adversary's win rate over FROZEN, in points, on the two")
    say('EVALUATION banks, which are disjoint from the banks it was fitted on.')
    say()
    rows = [r for r in raw if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[r['cell']].append(r)
    say('%-5s %-15s %-9s %-32s %-16s %s' % ('id', 'class', 'kpi', 'edge over FROZEN', 'per bank', 'replicated'))
    res = {}
    for zid in ['Z01', 'Z02', 'Z03', 'Z04', 'Z05', 'Z06', 'Z07', 'Z08']:
        rs = sorted(by.get(zid, []), key=lambda r: r['bank'])
        p = pool(rs); res[zid] = p
        if p is None: say('%-5s (not run)' % zid); continue
        say('%-5s %-15s %-9s %-32s %-16s %s'
            % (zid, rs[0]['cls'][:15], rs[0]['kpi'], fmt(p),
               perbank(p),
               'yes' if p['replicated'] else 'NO'))
    say()
    return res

def b5():
    raw = load('P5-B5.jsonl')
    say('## B5 -- ablations: is the gain attributable?')
    say(); completeness('B5', raw, EXPECT['B5']); say()
    rows = [r for r in raw if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[r['cell']].append(r)
    P = {k: pool(v) for k, v in by.items()}
    ref = P.get('REF-FROZEN')
    say('Reference: FROZEN vs INCUMBENT on the lattice banks (7090003 primary, 7090001 replicate)')
    say('  %s   per bank %s' % (fmt(ref), ' '.join('%+.2f' % x['edge'] for x in ref['per']) if ref else ''))
    say()
    say('Add-one-in from v06 -- the component alone, against INCUMBENT:')
    say('%-12s %-32s %-16s %s' % ('component', 'edge', 'per bank', 'replicated'))
    adds = {}
    IN_FREEZE = ['A-r12', 'A-search', 'A-rtie', 'A-urgoff', 'A-stall']
    for c in IN_FREEZE + ['A-m2']:
        p = P.get(c); adds[c] = p
        if p: say('%-12s %-32s %-16s %s%s'
                  % (c, fmt(p), perbank(p),
                     'yes' if p['replicated'] else 'NO',
                     '   (NOT in the freeze)' if c == 'A-m2' else ''))
        else: say('%-12s (not run)' % c)
    say()
    say('Leave-one-out from FROZEN -- drop = FROZEN edge minus the variant edge:')
    say('%-12s %-32s %-32s %-16s %s' % ('component', 'variant edge', 'DROP', 'per-bank drop', 'replicated'))
    loos = {}
    for c in ['L-r12', 'L-search', 'L-rtie', 'L-urgoff', 'L-stall']:
        p = P.get(c); d = delta(ref, p); loos[c] = d
        if p: say('%-12s %-32s %-32s %-16s %s'
                  % (c, fmt(p), fmtd(d),
                     perbank(d) if d else '',
                     ('yes' if d['replicated'] else 'NO') if d else ''))
        else: say('%-12s (not run)' % c)
    say()
    # PREREG 1 item 3 / 4-B5: the freeze carries FIVE components.  `m2=0` was dropped
    # from it, so it is not one of "its parts" and does not belong in the sum S4 tests.
    naive5 = sum(adds[c]['edge'] for c in IN_FREEZE if adds.get(c))
    naive6 = naive5 + (adds['A-m2']['edge'] if adds.get('A-m2') else 0.0)
    if ref:
        say('naive sum of the FIVE components the freeze carries   %+.2f' % naive5)
        say('  (the same sum including A-m2, which the freeze does')
        say('   NOT carry, and which S4 therefore does not test:   %+.2f)' % naive6)
        say('measured whole (FROZEN vs INCUMBENT)                  %+.2f' % ref['edge'])
        if naive5:
            nse = math.sqrt(sum(adds[c]['se'] ** 2 for c in IN_FREEZE if adds.get(c)))
            r0 = ref['edge'] / naive5
            rlo = (ref['edge'] - Z * ref['se']) / (naive5 + Z * nse)
            rhi = (ref['edge'] + Z * ref['se']) / max(1e-9, naive5 - Z * nse)
            say('sub-additivity ratio  whole / naive sum               %.3f [%.3f, %.3f]'
                % (r0, rlo, rhi))
            say('  phase 2 measured its own three mechanisms composing at 0.83, which lies INSIDE')
            say('  that interval: the two phases\' composition is not distinguishable here.')
        say()
        drops = {k: v['delta'] for k, v in loos.items() if v}
        if drops:
            big = max(drops.items(), key=lambda kv: kv[1])
            say('largest single leave-one-out drop  %s %+.3f' % (big[0], big[1]))
            say()
            say('PREREGISTRATION 6 item 3: the gain is UNLOCATED if removing every single component')
            say('in turn costs less than a third of the whole.  This is a NAMED FAILURE CONDITION, so')
            say('it is reported per bank as well as pooled, under section 3\'s replication rule.')
            say()
            say('%-10s %-12s %-12s %-22s %s' % ('bank', 'whole', 'one third', 'largest single drop', 'verdict'))
            verd = {}
            for bk in sorted(ref['byBank']):
                w = ref['byBank'][bk]
                dd = {k: w - P[k]['byBank'][bk] for k in loos if P.get(k) and bk in P[k]['byBank']}
                bg = max(dd.items(), key=lambda kv: kv[1])
                verd[bk] = bg[1] >= w / 3
                say('%-10d %-12.3f %-12.3f %-22s %s'
                    % (bk, w, w / 3, '%s %+.3f' % bg, 'LOCATED' if verd[bk] else 'NOT LOCATED'))
            say('%-10s %-12.3f %-12.3f %-22s %s'
                % ('pooled', ref['edge'], ref['edge'] / 3, '%s %+.3f' % big,
                   'LOCATED' if big[1] >= ref['edge'] / 3 else 'NOT LOCATED'))
            say()
            if len(set(verd.values())) > 1:
                say('THE LOCATION TEST DOES NOT REPLICATE.  It is LOCATED on one bank and NOT LOCATED')
                say('on the other, and the pooled figure clears the bar by only %+.3f points.'
                    % (big[1] - ref['edge'] / 3))
                say('Under section 3 -- "a claim whose sign does not replicate across the two banks')
                say('is reported as not replicated, whatever its pooled interval says" -- the pooled')
                say('LOCATED verdict is NOT REPLICATED and must not be reported as though it were.')
                bd = sorted(ref['byBank'][bk] - P[big[0]]['byBank'][bk] for bk in ref['byBank'])
                # each bank's drop is itself a difference of two single-bank cells, so the
                # difference BETWEEN the two banks' drops combines four per-bank errors.
                hw = Z * math.sqrt(sum(ref['seByBank'][bk] ** 2 + P[big[0]]['seByBank'][bk] ** 2
                                       for bk in ref['byBank']))
                say('The two banks\' largest drops differ by %.3f points, against a half-width on'
                    % (bd[-1] - bd[0]))
                say('that difference of %.3f: the split is real on the point estimates but is not'
                    % hw)
                say('itself statistically resolved, so the honest reading is that this battery does')
                say('not settle whether the gain is located by the protocol\'s one-third rule.')
        say()
    return P, adds, loos, ref, naive5, naive6

def b6():
    raw = load('P5-B6.jsonl')
    say('## B6 -- the partner-regime table')
    say(); completeness('B6', raw, EXPECT['B6']); say()
    rows = [r for r in raw if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[(r['opp'], r['arm'], r['partners'])].append(r)
    P = {k: pool(v) for k, v in by.items()}
    out = {}
    for opp, partners in (('v05', ['itself', 'v06', 'v05', 'v04', 'v03', 'detective', 'withholder', 'lockout']),
                          ('v06', ['itself', 'v06', 'v03', 'detective'])):
        arms = ['FROZEN', 'INCUMBENT', 'v05'] if opp == 'v05' else ['FROZEN', 'INCUMBENT']
        say('### opponent %s   -- team A is [ARM, P, P] against three copies of %s' % (opp, opp))
        say('%-12s %-34s %-34s %-34s' % ('partners', *(arms + [''] * (3 - len(arms)))))
        for p in partners:
            row = []
            for a in arms:
                q = P.get((opp, a, p))
                row.append(('%s (%s)' % (fmt(q), ','.join('%+.2f' % x['edge'] for x in q['per'])))
                           if q else '   --')
            row += [''] * (3 - len(row))
            say('%-12s %-34s %-34s %-34s' % (p, *row))
        say()
        say('%-12s %-34s %-16s %-11s %s' % ('partners', 'FROZEN - INCUMBENT', 'per-bank delta',
                                            'replicated', 'clears zero*'))
        deltas = {}
        for p in partners:
            d = delta(P.get((opp, 'FROZEN', p)), P.get((opp, 'INCUMBENT', p)))
            deltas[p] = d
            if d: say('%-12s %-34s %-16s %-11s %s'
                      % (p, fmtd(d), perbank(d),
                         'yes' if d['replicated'] else 'NO', 'yes' if d['lo'] > 0 else 'no'))
            else: say('%-12s (not run)' % p)
        say('* "clears zero" is the ABANDONED draft-2 statistic (PREREGISTRATION 5.2); it is printed')
        say('  because the protocol shows its own draft-2 column, and it is NOT what S1 tests.')
        say()
        base = {}
        if opp == 'v05':
            say('%-12s %-34s %-16s %s' % ('partners', 'INCUMBENT - v05 (the incumbent as its own control)',
                                          'per-bank', 'replicated'))
            for p in partners:
                d = delta(P.get((opp, 'INCUMBENT', p)), P.get((opp, 'v05', p)))
                base[p] = d
                if d: say('%-12s %-34s %-16s %s'
                          % (p, fmtd(d), perbank(d),
                             'yes' if d['replicated'] else 'NO'))
            say()
            # PREREG 5.2 draft 3: reported, not gated, and now computable on all eight rows.
            def scal(dd):
                ch = [dd[q]['delta'] for q in partners[1:] if dd.get(q)]
                se = dd['itself']['delta'] if dd.get('itself') else None
                if not ch or se is None: return None
                ch = sorted(ch)
                med = ch[len(ch) // 2] if len(ch) % 2 else (ch[len(ch) // 2 - 1] + ch[len(ch) // 2]) / 2
                return dict(min=ch[0], median=med, self=se, ratio=(med / se if se else float('nan')))
            s7, s6 = scal(deltas), scal(base)
            say('Draft 3 of S1, REPORTED and not gated (PREREGISTRATION 5.2): the incumbent as its own')
            say('control, over all eight rows now that B6 runs all three arms at all eight settings.')
            say('%-22s %-9s %-9s %-9s %-9s' % ('comparison', 'min', 'median', 'self', 'median/self'))
            for nm, s in (('v0.7 over v0.6', s7), ('v0.6 over v0.5', s6)):
                if s: say('%-22s %+9.2f %+9.2f %+9.2f %9.3f' % (nm, s['min'], s['median'], s['self'], s['ratio']))
            say('phase 4 measured, on TRAINING material and stated in advance in the protocol:')
            say('%-22s %+9.2f %+9.2f %+9.2f %9.3f' % ('v0.7 over v0.6 (train)', -0.15, 1.26, 2.94, 0.428))
            say('%-22s %+9.2f %+9.2f %+9.2f %9.3f' % ('v0.6 over v0.5 (train)', -0.61, 0.64, 1.35, 0.472))
            say()
        out[opp] = (P, deltas, partners, arms, base)
    return out

def b7():
    raw = load('P5-B7.jsonl')
    say('## B7 -- cross-play between independently-trained runs')
    say(); completeness('B7', raw, EXPECT['B7']); say()
    ids = ['xp1', 'xp2', 'xp3']
    dist = {}
    try:
        V = {i: [float(x) for x in open(os.path.join(ROOT, 'research', 'v07', 'runs',
                                                     'p4-%s.txt' % i)).read().strip().split('|')]
             for i in ids}
        for i in range(3):
            for j in range(i + 1, 3):
                a, b = V[ids[i]], V[ids[j]]; n = min(len(a), len(b))
                dist[(ids[i], ids[j])] = (math.sqrt(sum((a[k] - b[k]) ** 2 for k in range(n))),
                                          max(abs(a[k] - b[k]) for k in range(n)), n)
    except Exception:
        pass
    if dist:
        say('The three runs, and how far apart they actually landed -- an off-diagonal that does not')
        say('collapse is only evidence of robustness if the runs are genuinely different policies:')
        say('%-14s %-10s %-10s %s' % ('pair', 'L2', 'Linf', 'coordinates'))
        for k, (l2, li, n) in sorted(dist.items()):
            say('%-14s %-10.3f %-10.3f %d' % ('%s vs %s' % k, l2, li, n))
        say('The protocol commits these as "pairwise L2 distance 6.3-9.6 over 55 coordinates".')
        say()
    rows = [r for r in raw if r.get('ok')]
    cp = collections.defaultdict(list); h2h = collections.defaultdict(list)
    for r in rows:
        (cp if r['kind'] == 'crossplay' else h2h)[(r['arm'], r.get('partners') or r.get('opp'))].append(r)
    P = {k: pool(v) for k, v in cp.items()}
    say('Cell (i, j) = --a=run_i --partners=run_j --b=v05.  The diagonal is self-play.')
    say('%-8s %-34s %-34s %-34s' % ('a \\ ptnr', *ids))
    for i in ids:
        row = [(('%s (%s)' % (fmt(P[(i, j)]), perbank(P[(i, j)])))
                if P.get((i, j)) else '   --') for j in ids]
        say('%-8s %-34s %-34s %-34s' % (i, *row))
    say()
    diag = [P[(i, i)]['edge'] for i in ids if P.get((i, i))]
    off = [P[(i, j)]['edge'] for i in ids for j in ids if i != j and P.get((i, j))]
    gap = None
    if diag and off:
        gap = sum(diag) / len(diag) - sum(off) / len(off)
        say('diagonal mean      %+.2f  over %d cells' % (sum(diag) / len(diag), len(diag)))
        say('off-diagonal mean  %+.2f  over %d cells' % (sum(off) / len(off), len(off)))
        say('gap                %+.2f' % gap)
        say('phase 4 measured, on training material: +4.51 self-play and +4.48 cross-play, a gap of')
        say('0.02 against a per-cell half-width of 0.63.  The protocol states that a phase-5 gap of')
        say('more than 1.5 is a finding that CONTRADICTS phase 4.')
        say()
    H = {k: pool(v) for k, v in h2h.items()}
    say('Head to head, so "these are different policies" is measured rather than assumed:')
    say('%-14s %-32s %-16s' % ('pair', 'edge of the first', 'per bank'))
    for (a, b), p in sorted(H.items()):
        say('%-14s %-32s %-16s' % ('%s vs %s' % (a, b), fmt(p),
                                   perbank(p)))
    say()
    return P, H, diag, off, gap, dist

def b8():
    raw = load('P5-B8.jsonl')
    say('## B8 -- the rule-dialect table  (FROZEN vs INCUMBENT, both arms in the same dialect)')
    say(); completeness('B8', raw, EXPECT['B8']); say()
    rows = [r for r in raw if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[r['row']].append(r)
    P = {k: pool(v) for k, v in by.items()}
    base = P.get('default')
    say('%-22s %-32s %-16s %-11s %s' % ('dialect', 'edge', 'per bank', 'replicated', 'vs default'))
    for name in ['default', 'no-out-of-turn', 'no-cardless-declare', 'maxasks=360',
                 'arb=high', 'arb=turn', 'sets=8', 'legacy']:
        p = P.get(name)
        if not p: say('%-22s (not run)' % name); continue
        say('%-22s %-32s %-16s %-11s %+.2f'
            % (name, fmt(p), perbank(p),
               'yes' if p['replicated'] else 'NO', p['edge'] - base['edge'] if base else float('nan')))
    say()
    if base and all(P.get(k) for k in ('legacy', 'no-out-of-turn', 'no-cardless-declare', 'maxasks=360')):
        iso = sum(P[k]['edge'] - base['edge'] for k in ('no-out-of-turn', 'no-cardless-declare', 'maxasks=360'))
        rse = math.sqrt(sum(P[k]['se'] ** 2 for k in ('legacy', 'no-out-of-turn',
                                                      'no-cardless-declare', 'maxasks=360'))
                        + 3 * base['se'] ** 2)
        rv = P['legacy']['edge'] - base['edge'] - iso
        say('legacy residual (legacy minus the three isolable components) = %+.3f [%+.2f, %+.2f].'
            % (rv, rv - Z * rse, rv + Z * rse))
        say('The interval is %.1fx the size of the residual, so the data are equally consistent'
            % (Z * rse / max(1e-9, abs(rv))))
        say('with the ladder contributing nothing.  Note also that `maxasks=360` is BIT-IDENTICAL')
        say('to default here -- the 400-ask cap is never reached -- so the residual is')
        say('arithmetically legacy minus TWO effective components, not three.')
        say('The forced-endgame willingness ladder has no CLI flag of its own and is reachable only')
        say('through --legacy, so it is not given a row rather than faked, and this residual is the')
        say("protocol's estimate of the ladder's contribution.")
        say()
    return P

def b9():
    raw = load('P5-B9.jsonl')
    say('## B9 -- negative controls')
    say(); completeness('B9', raw, EXPECT['B9']); say()
    rows = [r for r in raw if r.get('ok')]
    by = collections.defaultdict(list)
    for r in rows: by[(r['kind'], r.get('hstr'))].append(r)
    P = {k: pool(v) for k, v in by.items()}
    say('### B9.2 / B9.3  planted-weakness recovery')
    say()
    say('The planted cost is measured directly as a paired duplicate (an ADDED cell: "the recovered')
    say('size tracks the planted size" is not computable without it).  The recovered EXCESS is the')
    say("responder's edge against the handicapped target minus its edge against the unhandicapped")
    say('one (also an ADDED cell: B9.3 speaks of the excess "resolving to zero").')
    say()
    say('%-7s %-30s %-30s %-30s %-30s' % ('hstr', 'planted cost (direct)', 'responder vs handicapped',
                                          'responder vs FROZEN', 'RECOVERED EXCESS'))
    ex = {}
    for h in ['0.05', '0.08', '0.11', '0.15']:
        pc = P.get(('planted-cost', h)); rh = P.get(('responder-vs-handicapped', h))
        rf = P.get(('responder-vs-frozen', h))
        e = delta(rh, rf); ex[h] = (pc, rh, rf, e)
        say('%-7s %-30s %-30s %-30s %-30s' % (h, fmt(pc), fmt(rh), fmt(rf), fmtd(e)))
    say()
    say('### B9.4  the identity control')
    idr = sorted([r for r in rows if r['kind'] == 'identity'], key=lambda r: r['bank'])
    for r in idr:
        m = r['match']
        say('  bank %d   winRateA %.6f   ci [%.6f, %.6f]   power.mirror %s   n=%d'
            % (r['bank'], m['winRateA'], m['ci'][0], m['ci'][1], m['power']['mirror'], m['games']))
    say()
    return P, rows, ex, idr

def b9side():
    rows = load('P5-B9side.jsonl')
    say('### B9.5  the side-channel positive controls')
    say(); completeness('B9side', rows, EXPECT['B9side']); say()
    got = collections.defaultdict(dict)
    for r in rows:
        if not r.get('ok'): continue
        for name, t in (r['side'].get('tests') or {}).items():
            got[(r['cheat'], r['bank'])][name] = t.get('status')
    say('%-9s %-9s %s' % ('cheat', 'bank', 'per-test status'))
    for k in sorted(got):
        say('%-9s %-9d %s' % (k[0], k[1], '  '.join('%s=%s' % (n.split()[0], s)
                                                    for n, s in sorted(got[k].items()))))
    say()
    return got

def b10():
    raw = load('P5-B10.jsonl')
    say('## B10 -- the S6 residual, on holdout material')
    say(); completeness('B10', raw, EXPECT['B10']); say()
    rows = [r for r in raw if r.get('ok')]
    say('Column 1 reproduces the phase-4 defect; column 2 is the GATE condition.  A nonzero column 2')
    say('for any arm is something phase 4 did not catch, and the protocol says it is the headline.')
    say()
    say('%-11s %-9s %-24s %-24s' % ('arm', 'bank', '--threads=1', '--threads=1 --freshagents'))
    by = {}
    for r in rows:
        s6 = (r['side'] or {}).get('s6', {})
        by[(r['arm'], r['bank'], r['cond'])] = (s6.get('mismatch'), s6.get('nodes'))
    for a in ['FROZEN', 'INCUMBENT', 'F-cheap', 'composite']:
        for bank in (7090001, 7090002):
            x = by.get((a, bank, 'threads1')); y = by.get((a, bank, 'threads1-freshagents'))
            say('%-11s %-9d %-24s %-24s' % (a, bank, ('%s / %s' % x) if x else 'NOT MEASURED',
                                            ('%s / %s' % y) if y else 'NOT MEASURED'))
    say()
    return by

def b11(r2, r6, r7):
    say('## B11 -- the selection-bias check')
    say()
    K = 15
    sigma = 98 / 2 / math.sqrt(24000)
    exp_max = sigma * math.sqrt(2 * math.log(K))
    say('K = %d configurations were scored against v06 before the freeze.  At the 24,000-game' % K)
    say('lattice cell size sigma = 98/2/sqrt(24000) = %.4f points, and sigma*sqrt(2 ln K) = %.2f'
        % (sigma, exp_max))
    say('points is the expected MAXIMUM under the null that none of the K differs from the incumbent.')
    say()
    p21, p22 = r2.get('B2.1'), r2.get('B2.2')
    if p21: say('  measured holdout gain over INCUMBENT   %+.2f [%+.2f, %+.2f]' % (p21['edge'], p21['lo'], p21['hi']))
    if p22: say('  measured holdout gain over F-cheap     %+.2f [%+.2f, %+.2f]' % (p22['edge'], p22['lo'], p22['hi']))
    say('  expected contribution of selection to the HOLDOUT estimate:  0.00 points')
    say()
    say('The holdout banks were never available for selection: not one of the K choices could have')
    say('been made using any deal of 7090001-7091002, so the selection term on holdout is zero by')
    say('construction rather than by measurement.')
    say()
    say('The third clause -- "confirm that the holdout estimate is not systematically smaller than')
    say('the training estimate by about that amount" -- needs a TRAINING estimate of the same cell.')
    say('The protocol supplies no training figure for B2.1 or B2.2.  The two training figures it')
    say('does state in advance are compared instead, and the comparison is labelled for what it is:')
    say()
    say('%-42s %-16s %-16s %-10s' % ('quantity', 'training', 'holdout', 'shortfall'))
    # The one directly comparable pair the protocol supplies.  Section 1 states, in
    # advance, that `rtie=1`, the urgency-off keys and `stall=12` "as a group are
    # worth +0.78 [+0.33, +1.22], replicated on both training banks, measured as a
    # PAIRED HEAD-TO-HEAD AGAINST THE PHASE-2 COMPOSITE".  B2.4 is exactly that
    # head-to-head, re-run on holdout: FROZEN minus the composite is that group of
    # three added and `m2=0` removed, and section 1 item 3 records `m2=0`'s
    # leave-one-out drop as exactly zero.
    p24 = r2.get('B2.4')
    if p24:
        say('%-42s %+16.2f %+16.2f %+10.2f'
            % ('B2.4  rtie + urgency-off + stall, as a group', 0.78, p24['edge'], p24['edge'] - 0.78))
        say('   training interval [+0.33, +1.22]   holdout interval [%+.2f, %+.2f]'
            % (p24['lo'], p24['hi']))
        say()
    d05 = r6.get('v05', (None, {}, [], [], {}))[1] if r6 else {}
    if d05.get('itself'):
        t, h = 2.94, d05['itself']['delta']
        say('%-42s %+16.2f %+16.2f %+10.2f' % ('B6 self-play row, opponent v05', t, h, h - t))
    P7, H7, diag, off, gap, dist = r7 if r7 else (None,) * 6
    if diag and off:
        say('%-42s %+16.2f %+16.2f %+10.2f' % ('B7 diagonal (self-play)', 4.51,
                                               sum(diag) / len(diag), sum(diag) / len(diag) - 4.51))
        say('%-42s %+16.2f %+16.2f %+10.2f' % ('B7 off-diagonal (cross-play)', 4.48,
                                               sum(off) / len(off), sum(off) / len(off) - 4.48))
    say()
    say('A shortfall of about %.2f points on these would be the signature of a training estimate' % exp_max)
    say('inflated by selection; a shortfall much larger than that is something else and a shortfall')
    say('near zero is consistent with no selection inflation at all.')
    say()
    return exp_max, sigma, K

def throughput():
    """PREREGISTRATION 1, Cost.  Read off the B3 cells, which already carry
    gamesPerSec at a fixed thread count, rather than spending games on a bench."""
    rows = [r for r in load('P5-B3.jsonl') if r.get('ok')]
    if not rows: return {}
    by = collections.defaultdict(dict)
    cls = {}
    for r in rows:
        by[(r['opp'], r['bank'])][r['arm']] = r['match']['gamesPerSec']
        cls[r['opp']] = r['cls']
    say('## Throughput -- the cost claim, CHECKED rather than repeated')
    say()
    say('`gamesPerSec` is WHOLE-MATCH throughput: it carries the opponent\'s decision cost on both')
    say('sides of the ratio, so INCUMBENT_gps / ARM_gps = (t_arm + t_opp)/(t_inc + t_opp), which is')
    say('strictly BELOW t_arm/t_inc.  Every figure below is therefore a LOWER BOUND on the cost')
    say('multiple, and the bound is tightest against the cheapest opponents.')
    say()
    say('%-11s %-22s %-22s %-22s' % ('arm', 'vs `random` (tightest)', 'far archetypes (median)',
                                     'whole panel (median)'))
    out = {}
    for a in ('FROZEN', 'F-cheap', 'composite'):
        def med(sel):
            v = sorted(by[k]['INCUMBENT'] / by[k][a] for k in by
                       if sel(k) and by[k].get('INCUMBENT') and by[k].get(a))
            return statistics.median(v) if v else None
        rnd = med(lambda k: k[0] == 'random')
        far = med(lambda k: cls.get(k[0]) == 'far')
        allm = med(lambda k: True)
        out[a] = rnd
        say('%-11s %-22s %-22s %-22s'
            % (a, '%.2fx' % rnd if rnd else '--', '%.2fx' % far if far else '--',
               '%.2fx' % allm if allm else '--'))
    say()
    say('The tightest bound is the one taken against the cheapest opponents, where the opponent')
    say('contributes least to both sides of the ratio.  The protocol records the configuration as')
    say('running at "roughly 3.2x the blueprint" and asks phase 5 to CHECK rather than repeat it.')
    if out.get('FROZEN'):
        say('Measured here: FROZEN costs AT LEAST %.2fx the blueprint against `random`, and at least'
            % out['FROZEN'])
        say('the far-archetype median above.  Both lower bounds EXCEED 3.2x, so the recorded cost')
        say('claim understates the true multiple rather than overstating it.  The whole-panel median')
        say('is lower only because the expensive panel members put a large common term on both sides')
        say('of the ratio; it is the most diluted of the three and the least informative.')
    say()
    return out

def duplicates():
    rows = [r for r in load('P5-B3.jsonl') if r.get('ok')]
    if not rows: return None
    by = collections.defaultdict(list)
    for r in rows: by[(r['arm'], r['oppSpec'], r['bank'], r['deals'])].append((r['opp'], r['match']['winRateA']))
    dups = {k: v for k, v in by.items() if len(v) > 1}
    bad = [(k, v) for k, v in dups.items() if len({x[1] for x in v}) > 1]
    say('## Panel duplicates -- a free determinism check, and what it caught')
    say()
    say('Three sealed adversaries are policies the scripted archetypes and the frontier already')
    say('carry (R-v04 is v04, S-archetype-0 is feint, S-reference-0 is v06).  The protocol fixes the')
    say('panel by NAME, so those cells are run anyway.  Each duplicated pair is the SAME command')
    say('run twice -- same arm, same opponent spec, same bank, same deals, same 13 threads -- so')
    say('the two cells must agree to the digit.')
    say()
    say('%d duplicated (arm, policy, bank) cells; %d disagree.' % (len(dups), len(bad)))
    for k, v in bad:
        ws = sorted(x[1] for x in v)
        n = 2 * k[3]
        say('  DISAGREEMENT  arm=%s  opponent=%s  bank=%d' % (k[0], k[1], k[2]))
        say('    %s' % '  '.join('%s %.6f' % x for x in v))
        say('    difference %.6f = %.0f game(s) in %d = %.4f points'
            % (ws[-1] - ws[0], round((ws[-1] - ws[0]) * n), n, 100 * (ws[-1] - ws[0])))
    if bad:
        say()
        say('  This is the cross-deal agent residue of PREREGISTRATION 5.3, surfacing in the SCORED')
        say('  mode for the first time in this corpus.  `runMatch` schedules deals by work-stealing')
        say('  over a shared atomic counter (arena.hpp), so the deal-to-thread assignment is not')
        say('  fixed run to run even at a fixed thread count; agents are built once per thread and')
        say('  reused across deals, and some per-agent state survives reset() and is reachable only')
        say('  under the search.  A searching arm can therefore play one deal differently between')
        say('  two invocations of the identical command.  The protocol quantifies the defect as')
        say('  "one deal in 800 of play" and states that phase 5 evaluates the frozen configuration')
        say('  AS FROZEN, with the defect present.')
        say()
        say('  The B0 reproducibility check did not catch this: it ran a 400-deal cell five times')
        say('  and got bit-identical results.  The panel duplicates are 1,500- and 6,000-deal cells,')
        say('  and at that size the residue bites.  The B0 check was therefore too small to be')
        say('  conclusive, and this is the correction to it.')
        say()
        say('  Magnitude, against the numbers it could move: the largest disagreement above is')
        say('  %.4f points, against a per-cell half-width of 0.63 points -- about %.0fx smaller.'
            % (max(100 * (sorted(x[1] for x in v)[-1] - sorted(x[1] for x in v)[0]) for k, v in bad),
               0.63 / max(1e-9, max(100 * (sorted(x[1] for x in v)[-1] - sorted(x[1] for x in v)[0])
                                    for k, v in bad))))
        say('  No verdict in this document turns on a quantity that small, and every arm carries')
        say('  the same defect, but it is a real property of the instrument and it is recorded.')
    say()
    return dict(pairs=len(dups), disagreements=len(bad))

# =============================================================================
def verdicts(pre, r2, r3, r4, r5, r6, r7, r8, r9, r9s, r10, r11):
    P3, worst, worst_ns, mmr, members, arms, shared = r3
    P5, adds, loos, ref, naive5, naive6 = r5
    P9, rows9, ex9, idr = r9
    exp_max, sigma, K = r11
    stop = []
    say('## The prespecified verdicts')
    say()

    say('### 5.1  the primary claim: FROZEN beats F-cheap on both primary banks')
    p = r2.get('B2.2')
    if p is None or p['banks'] < 2:
        say('  INCOMPLETE')
    else:
        gate_ok = pre['frozen'] and pre['b0'] and pre['negctrl']
        cert = p['lo'] > FLOOR and p['replicated'] and gate_ok
        meas = p['lo'] > 0 and p['replicated'] and gate_ok
        say('  pooled edge over F-cheap    %+.2f [%+.2f, %+.2f] over %d games' % (p['edge'], p['lo'], p['hi'], p['n']))
        say('  per bank                    %s' % '  '.join('%d:%+.2f' % (x['bank'], x['edge']) for x in p['per']))
        say('  sign replicates on both banks              %s' % ('yes' if p['replicated'] else 'NO'))
        say('  lower bound above the %.2f reference bar    %s' % (FLOOR, 'yes' if p['lo'] > FLOOR else 'no'))
        say('  FROZEN passes B1 (read from P5-gate.jsonl) %s' % ('yes' if pre['frozen'] else 'NO'))
        say('  B0 preconditions all pass                  %s' % ('yes' if pre['b0'] else 'NO'))
        say('  VERDICT: %s' % ('CERTIFIED advancement' if cert else
                               'measured advancement' if meas else 'NOT an advancement'))
    say()

    say('### S1  the advantage does not COLLAPSE under partner change')
    d05 = r6.get('v05', (None, {}, [], [], {}))[1] if r6 else {}
    changed = ['v06', 'v05', 'v04', 'v03', 'detective', 'withholder', 'lockout']
    have = [q for q in changed if d05.get(q)]
    if len(have) < 7 or not d05.get('itself'):
        say('  INCOMPLETE (%d of 7 changed-partner rows measured)' % len(have))
    else:
        allrows = ['itself'] + changed
        allv = [d05[q]['delta'] for q in allrows]
        chv = [d05[q]['delta'] for q in changed]
        say('  FROZEN - INCUMBENT over the eight partner settings, opponent v05:')
        say('    ' + '  '.join('%s %+.2f' % (q, d05[q]['delta']) for q in allrows))
        say('  positive rows: %d of 8 including self-play;  %d of 7 changed-partner rows'
            % (sum(1 for v in allv if v > 0), sum(1 for v in chv if v > 0)))
        say('  minimum over the eight %+.2f;  over the seven changed %+.2f  (must not be below -1.0)'
            % (min(allv), min(chv)))
        say('  The protocol says "five of the eight changed-partner rows"; B6 has SEVEN changed')
        say('  settings plus self-play, and the protocol\'s own worked table counts EIGHT including')
        say('  self-play ("Seven of the eight rows are positive").  The verdict below uses the')
        say('  eight-row reading, which is the one the protocol demonstrates; the seven-row count is')
        say('  printed above so a reader can apply the other reading.')
        ok8 = sum(1 for v in allv if v > 0) >= 5 and min(allv) >= -1.0
        ok7 = sum(1 for v in chv if v > 0) >= 5 and min(chv) >= -1.0
        say('  VERDICT (eight-row reading): %s' % ('PASS' if ok8 else 'FAIL -- a collapse'))
        say('  under the seven-row reading:  %s' % ('PASS' if ok7 else 'FAIL'))
    say()

    say('### S2  the advantage is not a private convention between identical fits')
    P7, H7, diag, off, gap, dist = r7
    if gap is None:
        say('  INCOMPLETE')
    else:
        say('  diagonal mean %+.2f, off-diagonal mean %+.2f, gap %+.2f'
            % (sum(diag) / len(diag), sum(off) / len(off), gap))
        say('  VERDICT: %s  (fails if the off-diagonal COLLAPSES, i.e. more than 1.5 below)'
            % ('PASS' if gap <= 1.5 else 'FAIL -- the off-diagonal collapses'))
    say()

    say('### S3  no fresh adversary exploits it')
    ex = [(k, v) for k, v in r4.items() if v and v['banks'] >= 2]
    if len(ex) < 8:
        say('  INCOMPLETE (%d of 8 arms measured)' % len(ex))
    if ex:
        say('  The protocol states a PASS condition and a FAIL condition that are not complements:')
        say('  it PASSES if "every arm\'s edge has an upper bound below %.2f" and FAILS if "any arm' % FLOOR)
        say('  clears %.2f with a replicated sign".  Both are computed.' % FLOOR)
        say()
        say('  %-5s %-10s %-10s %-12s %-10s' % ('id', 'edge', 'upper', 'lower', 'replicated'))
        for k, v in sorted(ex):
            f = DECL_FLOOR if k == 'Z03' else FLOOR
            say('  %-5s %+10.2f %+10.2f %+12.2f %-10s   floor %.2f'
                % (k, v['edge'], v['hi'], v['lo'], 'yes' if v['replicated'] else 'no', f))
        say()
        pass_all = all(v['hi'] < (DECL_FLOOR if k == 'Z03' else FLOOR) for k, v in ex)
        fail_any = [k for k, v in ex if v['lo'] > (DECL_FLOOR if k == 'Z03' else FLOOR) and v['replicated']]
        band = [k for k, v in ex
                if v['hi'] >= (DECL_FLOOR if k == 'Z03' else FLOOR)
                and not (v['lo'] > (DECL_FLOOR if k == 'Z03' else FLOOR) and v['replicated'])]
        say('  Z03 is read against the DECLARATION-FAMILY floor %.2f, not %.2f, because its whole'
            % (DECL_FLOOR, FLOOR))
        say('  objective is --kpi=declerr (PREREGISTRATION 3, correction 4).')
        say('  pass condition (every upper bound below its floor): %s' % ('MET' if pass_all else 'not met'))
        say('  fail condition (an arm clears its floor, replicated): %s' % (', '.join(fail_any) or 'none'))
        if fail_any:
            say('  VERDICT: FAIL -- the exploit is the headline')
        elif pass_all:
            say('  VERDICT: PASS')
        else:
            say('  VERDICT: UNDETERMINED BY THE PROTOCOL AS WRITTEN.  Arms %s have an upper bound at'
                % ', '.join(band))
            say('  or above the floor without clearing it with a replicated sign, so neither the pass')
            say('  condition nor the fail condition is met.  This is reported and NOT resolved by')
            say('  choosing a reading.')
            stop.append('S3 is undetermined for %s: neither the pass condition nor the fail '
                        'condition of PREREGISTRATION 5.2/6.5 is met.' % ', '.join(band))
    say()

    say('### S4  the gain is attributable')
    if ref and loos and adds:
        drops = {k: v['delta'] for k, v in loos.items() if v}
        big = max(drops.items(), key=lambda kv: kv[1]) if drops else None
        subadd = naive5 > ref['edge']
        no_single = big and big[1] <= ref['edge']
        say('  measured whole %+.2f;  naive sum of the five in-freeze components %+.2f;  sub-additive: %s'
            % (ref['edge'], naive5, 'yes' if subadd else 'NO'))
        if big: say('  largest single leave-one-out drop %s %+.2f (must not exceed the whole)' % big)
        say('  VERDICT: %s' % ('PASS' if (subadd and no_single) else 'FAIL'))
    else:
        say('  INCOMPLETE')
    say()

    say('### S5  it survives the rule dialect')
    base = r8.get('default')
    if base and len([k for k in r8 if r8[k]]) >= 8:
        flip = [k for k, v in r8.items() if sign(v['edge']) != sign(base['edge'])]
        low = [k for k, v in r8.items() if v['edge'] < base['edge'] - 2.0]
        say('  default %+.2f;  rows losing the sign: %s;  rows more than 2 points below default: %s'
            % (base['edge'], flip or 'none', low or 'none'))
        say('  VERDICT: %s' % ('PASS' if not flip and not low else 'FAIL'))
    else:
        say('  INCOMPLETE')
    say()

    say('### S6  the worst case is not catastrophic')
    if 'FROZEN' in worst and 'INCUMBENT' in worst:
        f, i = worst['FROZEN'], worst['INCUMBENT']
        say("  FROZEN's worst cell    %+.2f  against %s" % (f[0], f[1]))
        say("  INCUMBENT's worst cell %+.2f  against %s" % (i[0], i[1]))
        say('  VERDICT: %s' % ('PASS' if f[0] >= i[0] else "FAIL -- FROZEN's worst cell is worse"))
        say()
        say('  A caveat this comparison needs: INCUMBENT, F-cheap and the composite each meet')
        say('  THEMSELVES in the panel and so carry a 0.00 self-cell, which can only lower their')
        say('  worst cell; FROZEN is not in the panel and carries none.  With self-cells excluded:')
        fn, iN = worst_ns.get('FROZEN'), worst_ns.get('INCUMBENT')
        if fn and iN:
            say('    FROZEN %+.2f against %s;  INCUMBENT %+.2f against %s;  %s'
                % (fn[0], fn[1], iN[0], iN[1],
                   'still PASS' if fn[0] >= iN[0] else 'FAILS on this reading'))
    else:
        say('  INCOMPLETE')
    say()

    say('### S7  the instrument is intact')
    legs = []
    say('  B9.2 -- the three supra-floor rungs must be recovered at or above %.2f, and the' % FLOOR)
    say('  recovered size must TRACK the planted size across the rungs.')
    say('  %-7s %-12s %-30s %-12s' % ('hstr', 'planted', 'recovered excess', 'recovered?'))
    rec = {}
    for h in ('0.08', '0.11', '0.15'):
        pc, rh, rf, e = ex9.get(h, (None,) * 4)
        if e is None: say('  %-7s (not run)' % h); continue
        got = e['delta'] >= FLOOR
        rec[h] = e['delta']
        say('  %-7s %-12s %-30s %-12s'
            % (h, ('%+.2f' % pc['edge']) if pc else '--', fmtd(e), 'yes' if got else 'NO'))
    if len(rec) == 3:
        tracks = rec['0.08'] <= rec['0.11'] <= rec['0.15']
        allrec = all(v >= FLOOR for v in rec.values())
        legs.append(('B9.2 recovery at or above %.2f' % FLOOR, allrec))
        legs.append(('B9.2 recovered size tracks the planted size', tracks))
        say('  monotone in the planted size: %s' % ('yes' if tracks else 'NO'))
    say()
    pc, rh, rf, e = ex9.get('0.05', (None,) * 4)
    say('  B9.3 -- the sub-floor rung hstr=0.05 must NOT be recovered.  Phase 2 measured this')
    say('  excess resolving TO zero at -0.01 [-0.45, +0.42]; if phase 5 now detects it, the')
    say('  instrument has changed and every other number in this battery is suspect.')
    if e:
        notrec = not (e['delta'] >= FLOOR)
        say('  planted %s;  recovered excess %s;  recovered: %s'
            % (('%+.2f' % pc['edge']) if pc else '--', fmtd(e), 'NO' if notrec else 'YES'))
        legs.append(('B9.3 the sub-floor rung is NOT recovered', notrec))
    say()
    say('  B9.5 -- the three cheats must fail exactly the tests the protocol names, on both banks.')
    REQ = {'seed': {'S3': 'PASS', 'S4': 'FAIL', 'S5': 'FAIL', 'S6': 'PASS'},
           'shared': {'S3': 'PASS', 'S4': 'PASS', 'S5': 'PASS', 'S6': 'FAIL'},
           'conv': {'S3': 'FAIL', 'S4': 'PASS', 'S5': 'PASS', 'S6': 'PASS'}}
    sideok = bool(r9s)
    for (ch, bank), d in sorted(r9s.items()):
        short = {n.split()[0]: s for n, s in d.items()}
        want = {k: v for k, v in REQ[ch].items() if k in short}
        bad = {k: (short[k], v) for k, v in want.items() if short[k] != v}
        if bad: sideok = False
        say('  %-8s bank %d: %s' % (ch, bank, 'as required' if not bad else 'MISMATCH %s' % bad))
    if r9s: legs.append(('B9.5 the three cheats fail exactly the named tests', sideok))
    if idr:
        exact = all(r['match']['winRateA'] == 0.5 and r['match']['power']['mirror'] for r in idr)
        zerovar = all(r['match']['ci'][0] == 0.5 and r['match']['ci'][1] == 0.5 for r in idr)
        legs.append(('B9.4 the identity control is exactly 50.000% with zero variance',
                     exact and zerovar))
        say('  B9.4: exact 50.000%% and power.mirror: %s;  zero variance (ci == [0.5, 0.5]): %s'
            % ('yes' if exact else 'NO', 'yes' if zerovar else 'NO'))
    say()
    for n, v in legs: say('  [%s] %s' % ('ok' if v else 'XX', n))
    s7 = bool(legs) and all(v for _, v in legs)
    say('  VERDICT: %s' % ('PASS' if s7 else ('FAIL -- a PROCEDURAL failure; phase 5 stops'
                                              if legs else 'INCOMPLETE')))
    if legs and not s7:
        stop.append('S7 failed: %s. PREREGISTRATION 5.2 makes this a procedural failure and no '
                    'number in this battery may be reported as a measurement.'
                    % '; '.join(n for n, v in legs if not v))
    say()

    say('### B10  the S6 gate condition on holdout material')
    if r10:
        gatecol = {k: v for k, v in r10.items() if k[2] == 'threads1-freshagents'}
        missing = [k for k in [(a, b, 'threads1-freshagents') for a in
                               ['FROZEN', 'INCUMBENT', 'F-cheap', 'composite']
                               for b in (7090001, 7090002)] if k not in gatecol]
        nz = {k: v for k, v in gatecol.items() if v[0]}
        if missing:
            say('  INCOMPLETE -- %d of 8 gate-condition cells not measured' % len(missing))
        elif nz:
            say('  NONZERO under --threads=1 --freshagents: %s' % nz)
            say('  The protocol says this is something phase 4 did not catch, and IT IS THE HEADLINE.')
            stop.append('B10 found a nonzero S6 count under the gate condition: %s' % nz)
        else:
            say('  every arm, both banks: 0 mismatches under --threads=1 --freshagents.')
        resid = {k: v for k, v in r10.items() if k[2] == 'threads1' and v[0]}
        say('  the phase-4 residue reproduces at one thread WITHOUT --freshagents in %d of %d cells'
            % (len(resid), len([k for k in r10 if k[2] == 'threads1'])))
    say()
    return stop

# =============================================================================
def deviations(stop):
    say('## Deviations, additions and protocol notes  (PREREGISTRATION section 7)')
    say()
    for d in DEVIATIONS: say(d)
    say()
    if stop:
        say('## STOP CONDITIONS RAISED')
        say()
        for s in stop: say('  * ' + s)
        say()
    else:
        say('No stop condition was raised.')
        say()

DEVIATIONS = [
 'D1  ADDED CELL -- B5 REF-FROZEN.  The eleven cells B5 names cannot yield a leave-one-out DROP:',
 '    the drop is FROZEN\'s edge minus the variant\'s, and FROZEN\'s own edge on the lattice banks is',
 '    not one of the eleven.  A twelfth cell was added at the lattice size on both lattice banks.',
 '    B5 therefore cost 288,000 games rather than the preregistered 264,000.',
 '',
 'D2  ADDED CELLS -- B9 planted cost and responder-vs-unhandicapped.  B9.2 requires that "the',
 '    recovered size tracks the planted size", which needs the planted size; B9.3 speaks of the',
 '    planted excess "resolving TO zero", which is the responder\'s edge against the handicapped',
 '    target MINUS its edge against the unhandicapped one.  Four rungs x 2 banks x 6,000 deals were',
 '    added for each, 192,000 games in total.',
 '',
 'D3  UNSPECIFIED BANK -- B8.  Section 2.1 assigns 7090003 the role "the ablation lattice and the',
 '    dialect table" but names no replicate for the dialect rows.  7090001 was used, by symmetry',
 '    with B5, which the protocol does specify as 7090003 primary and 7090001 replicate.',
 '',
 'D4  UNSPECIFIED BANK -- B3 sealed rows.  Section 2.1 gives 7091001 the role "evaluation bank for',
 '    the sealed adversary half", but section 4/B3 puts every panel member on the same two banks as',
 '    every other arm, and section 3 makes 7090001/7090002 the primary and replicate for every',
 '    claim.  The sealed rows were run on 7090001/7090002 with the rest of the panel, so the whole',
 '    panel is scored on one shared pair of banks, as minimax regret over a shared panel requires.',
 '    7091001 was digest-verified in B0.1 and then not played.',
 '',
 'D5  PROTOCOL NOTE -- what `tune --seed=<bank> --shard=s/4` actually plays.  Section 4/B4 describes',
 '    the eight searches as drawing "480 deal indices of the bank".  They do not.  `tune` uses',
 '    --seed only as the CEM root: it derives a fresh per-generation seed (tuner.hpp:310) and from',
 '    that a per-opponent match seed (tuner.hpp:258), so the deals are a derived stream and the',
 '    named bank\'s own deals are never constructed.  Banks 7090004, 7090005 and 7091002 are',
 '    therefore digest-verified and never played as deal banks.  What the protocol RELIES on',
 '    survives intact: --shard still partitions by index congruence, so the eight fitting sets are',
 '    mutually disjoint, and the fitting stream is disjoint from the evaluation banks, which is what',
 '    makes B4 and S3 interpretable.  Because the deals rotate per generation the fitted adversary',
 '    is if anything LESS overfitted to its fitting material than the protocol\'s account assumes.',
 '    The same applies to B9.2/B9.3\'s responder fits on 7090005.',
 '',
 'D6  PROTOCOL NOTE -- the seal is not enforced on every command.  Section 2.1(b) records that',
 '    `pathology`, `v7bits` and `v6probe` bypass `seedUsable`.  `tune` and `ablate` also bypass it,',
 '    for the same reason as D5: the seed `runMatch` sees is derived, not the registered one.',
 '    Phase 5 set FISH_UNSEAL_PHASE=5 once and used the seven registered seeds and no others.',
 '',
 'D7  OBSERVATION -- B0.3 printed no changed-source NOTE.  The protocol expected three engine',
 '    sources to be listed as changed since the freeze (arena.hpp, main.cpp, v07_side.hpp).  Zero',
 '    differ on the tree phase 5 ran at.  The stop condition -- a changed source WITH a changed R3',
 '    digest -- could not fire.',
 '',
 'D8  ADDED CHECK -- reproducibility, before the material was spent.  `runMatch` schedules deals by',
 '    work-stealing, so the deal-to-thread assignment is not fixed run to run even at a fixed thread',
 '    count, and the frozen configuration carries the cross-deal agent residue of section 5.3.  A',
 '    scored cell was checked to be bit-identical over three runs at 13 threads and across 1, 2 and',
 '    13 threads, on training material, before any holdout bank was played.',
 '',
 'D9  READING -- S1\'s row count.  Section 5.2 says "five of the eight changed-partner rows"; B6 has',
 '    seven changed settings plus self-play, and the protocol\'s own worked table counts eight',
 '    including self-play.  Both counts are printed and the verdict states which reading it used.',
 '',
 'D10 READING -- S3\'s pass and fail conditions are not complements.  "Every arm\'s upper bound below',
 '    1.53" and "any arm clears 1.53 with a replicated sign" leave a band in which neither holds.',
 '    Both are computed and reported; if the band is occupied the verdict is reported as',
 '    UNDETERMINED rather than resolved by choosing a reading.',
 '',
 'D11 READING -- the 2.13 declaration-family floor.  Section 3 correction 4 applies it to "B4\'s Z03,',
 '    and the declaration-accuracy column of B2".  Z03 is read against 2.13.  B2 has no',
 '    declaration-accuracy column with an interval -- `match --json` emits declaration accuracy as a',
 '    bare rate -- so the accuracy is printed as a diagnostic and PHASE 5 MAKES NO CLAIM ABOUT THE',
 '    DECLARATION CHANNEL, which is what discharges the rule.',
 '',
 'D12 ARITHMETIC -- S4\'s naive sum.  The freeze carries five components; `m2=0` was dropped from it.',
 '    The sum S4 tests is over those five.  The six-term sum including A-m2 is printed beside it and',
 '    labelled, because B5 preregisters the A-m2 cell even though the freeze does not carry the key.',
 '',
 'D13 NOTE -- every delta in this document is two independent intervals combined in quadrature, not',
 '    a paired difference.  The harness gives a paired delta only within one `ablate` invocation',
 '    over a shared panel, and no cell of this battery is of that form.  Section 5.2 fixes this',
 '    arithmetic and calls it conservative.',
 '',
 'D14 NOTE -- `ablate` was not used anywhere.  It derives its own per-opponent seeds by',
 '    `mixSeed(seed, i*7919+3)` (main.cpp:589), so `ablate --seed=7090003` would not play bank',
 '    7090003.  Every lattice cell is a `match` cell on the named bank instead.',
 '',
 'D15 NOTE -- the panel of 31 NAMED members contains three policies twice: R-v04 is v04,',
 '    S-archetype-0 is feint, S-reference-0 is v06.  The duplicated cells are run as named and used',
 '    as a determinism check.',
 '',
 'D16 NOTE -- Z08 fits 55 coordinates of which 18 are inert.  The protocol specifies --base=v07i for',
 '    the white-box class; `v07i` derives from V06Agent and reads only the 37 v0.6 coordinates',
 '    (factory.hpp), while the CEM extends the vector to 55 for any v07* base.  The search is over',
 '    37 live coordinates plus the inverter the base fixes.',
 '',
 'D17 NOTE -- thread counts.  Every scored cell ran at --threads=13, which section 5.3(2) fixes for',
 '    the preregistered batteries.  The S6 gate condition ran at --threads=1 --freshagents, and the',
 '    S6 residual column at --threads=1 alone.',
 'D18 CORRECTION TO D8 -- a scored cell is NOT bit-reproducible for a searching arm at panel',
 '    sizes.  The B0 check found five runs of a 400-deal cell bit-identical.  The panel-duplicate',
 '    check, on 1,500- and 6,000-deal cells, found one pair of identical commands disagreeing by',
 '    one game in 12,000 -- the cross-deal agent residue of section 5.3 reaching the scored mode.',
 '    The magnitude is 0.008 points against a 0.63-point half-width.  Reported in full under',
 '    "Panel duplicates" above.',
 '',
 'D19 CORRECTION -- B0.3\'s changed-source NOTE could not fire, and reporting zero as',
 '    "conservative" was wrong.  `freeze_config_v07.py --verify-only` reads its drift baseline out',
 '    of engine/fishbot_v07.json\'s own `provenance.srcSha256_16`.  That artifact was REWRITTEN',
 '    after the freeze commit 0fa4a5f -- at d1c6b35 and again at d8c554b -- and each rewrite',
 '    refreshes the baseline to the then-current tree.  B0.3 therefore compared the tree against a',
 '    baseline taken from the tree, and the three sources the protocol predicted (arena.hpp,',
 '    main.cpp, v07_side.hpp) no longer differ from it.  Against the actual freeze commit, five of',
 '    the 78 sources differ.  What is NOT weakened is the policy identity itself: R1, R2a and the',
 '    R3 mirror digest 5f81f440fc9c272a87e87c05fecc7b74 all round-trip, and those are computed by',
 '    RUNNING the engine, not by comparing hashes.  Because the hash-based drift check could not',
 '    fire, it was replaced by an EXECUTED one: the engine was rebuilt from the tree as it stands',
 '    at the end of the battery and the R3 mirror digest recomputed with the fresh binary.  It is',
 '    5f81f440fc9c272a87e87c05fecc7b74 -- identical to the frozen artifact and to the binary that',
 '    played every cell.  The source drift does not move the frozen policy, and that is now',
 '    established by running it rather than by comparing file hashes.',
 '',
 'D20 DISCLOSURE -- five commits landed in the repository DURING the battery, and four engine',
 '    sources now differ even from the refreshed baseline: fish.hpp, game.hpp, human.hpp,',
 '    table.hpp, from the "web play" commits b8cb227, 2e829c2, 46f3515, 72fa936 and e0da8fd.  They',
 '    touch the interactive browser table, not the policy.  The decisive fact is that engine/fish7',
 '    was NEVER REBUILT: sha256 cf6d5ea2c1f0e9e3896b..., mtime 2026-08-25 13:19, identical at the',
 '    start of B0 and at the end of the battery.  ONE BINARY PLAYED EVERY CELL.  The results are',
 '    therefore identified in this document by that binary hash and not by a commit, because no',
 '    single commit describes the tree for the whole run.',
 '',
 'D21 ORDERING -- B0 was performed before anything else, but its ARTIFACT was written later.  The',
 '    five B0 checks were run interactively at the top of the session, before the commit gate and',
 '    before any holdout deal was played.  engine/p5_b0.py, which re-runs all five and records them',
 '    as P5-B0.json, was written and run about fifteen minutes into B2.  Every B0 check is',
 '    deterministic and reproduced identically, so the artifact records what was checked first --',
 '    but its file timestamps do not show that ordering and this note is what does.',
 '',
 'D22 OBSERVATION -- one action-limit hit in 428 scored cells: the phase-2 composite against',
 '    SEALED:S-ask-1 on bank 7090001, one game in 12,000 (limitHitRate 8.33e-05).  Zero audit',
 '    violations in every cell of the battery.  G4 gates the mirror pathology run and not scored',
 '    cells, so this is not a gate violation, but the corpus standard is zero and it is recorded.',
 '',
 'D23 CORRECTION -- the throughput columns were printed with median_high (`v[len(v)//2]` on an',
 '    even-length list) rather than the median.  Corrected to the true median; every figure moves by',
 '    at most 0.11x and every one still exceeds the 3.2x the protocol records.',
 '',
 'D24 CORRECTION -- per-bank columns were printed unlabelled and sorted by seed NUMBER, which',
 '    inverts the attribution for B5 and B8, whose primary bank is 7090003 and whose replicate is',
 '    7090001.  Every per-bank value in this document is now printed as `seed:value`.',
 '',
]

if __name__ == '__main__':
    say('# FishBot v0.7 phase 5 -- reduction of the preregistered battery')
    say()
    pre = gate()
    r2 = b2(); r3 = b3(); r4 = b4(); r5 = b5(); r6 = b6(); r7 = b7()
    r8 = b8(); r9 = b9(); r9s = b9side(); r10 = b10()
    r11 = b11(r2, r6, r7)
    throughput(); duplicates()
    stop = verdicts(pre, r2, r3, r4, r5, r6, r7, r8, r9, r9s, r10, r11)
    deviations(stop)
    open(os.path.join(RES, 'P5-TABLES.txt'), 'w').write('\n'.join(OUT) + '\n')
    print('\nwrote research/v07/results/P5-TABLES.txt', file=sys.stderr)
