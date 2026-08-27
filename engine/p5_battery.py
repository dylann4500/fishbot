#!/usr/bin/env python3
"""FishBot v0.7 PHASE 5 -- the preregistered battery.

Every cell below is written from docs/v07/PREREGISTRATION.md and nothing else.
The protocol is the only input; CANDIDATES.md, ADVERSARIES.md and the phase-4
training logs are not read.

The driver is RESUMABLE: a cell whose key already appears in its artifact is
skipped, so a battery that takes twelve hours survives an interruption without
replaying what it has already measured.  Every row carries the literal argv it
was produced by, so a reader can re-run any single cell by hand.

usage:  python3 p5_battery.py <battery> [<battery> ...]
        batteries: B2 B3 B4fit B4eval B5 B6 B7 B8 B9 B10
"""
import json, os, subprocess, sys, time, hashlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ENG  = os.path.join(ROOT, 'engine')
FISH = os.path.join(ENG, 'fish7')
RES  = os.path.join(ROOT, 'research', 'v07', 'results')
ENV  = dict(os.environ); ENV['FISH_UNSEAL_PHASE'] = '5'
THREADS = 13                      # PREREGISTRATION 5.3(2): the batteries fix it at 13

# ---- the material -----------------------------------------------------------
_frz = json.load(open(os.path.join(ENG, 'fishbot_v07.json')))
# PREREGISTRATION 1: "Phase 5 reconstructs the spec from that file rather than typing it".
FROZEN = _frz['base'] + ':' + ','.join('%s=%s' % (k, v) for k, v in _frz['options'].items())
assert FROZEN == _frz['spec'], 'reconstructed spec differs from the frozen artifact'

SEARCH    = 's1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26'
URGOFF    = 'pool=-1,oppfloor=-1,force=1000000,askfloor=-1'
INCUMBENT = 'v06'                                             # PREREG 2.2
FCHEAP    = 'v06:' + SEARCH
FMID      = 'v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26'
COMPOSITE = 'v07:m2=0,r12=25,' + SEARCH
NEGCTRL   = 'v06:rtie=1,m1=0,' + URGOFF

BANK1, BANK2, BANK3 = 7090001, 7090002, 7090003
BANK_ADV, BANK_CTRL = 7090004, 7090005
BANK_ADVEVAL, BANK_ADVFIT = 7091001, 7091002

ARCHETYPES = ['v05', 'v04', 'v03', 'v02', 'lockout', 'detective', 'feint',
              'hunter', 'diversifier', 'bluffer', 'silent', 'withholder', 'random']

def sealed_adversaries():
    """PREREG 2.2 / B0.2: decode the sealed holdout half and verify its SHA-256."""
    import base64
    p = os.path.join(ROOT, 'research', 'v07', 'banks', 'holdout', 'adversaries-holdout.sealed')
    body = ''.join(l for l in open(p).read().splitlines() if l and not l.startswith('#'))
    pt = base64.b64decode(body)
    seal = json.load(open(os.path.join(ROOT, 'research', 'v07', 'banks', 'SEAL.json')))
    got = hashlib.sha256(pt).hexdigest()
    assert got == seal['adversariesHoldoutSha256'], 'sealed adversary digest mismatch'
    rows = []
    for l in pt.decode().splitlines():
        if l.strip() and not l.startswith('#'):
            i, c, s = l.split('\t'); rows.append((i, c, s))
    assert len(rows) == 14
    return rows

SEALED = sealed_adversaries()

XP = [open(os.path.join(ROOT, 'research', 'v07', 'runs', 'p4-xp%d.spec' % i)).read().strip()
      for i in (1, 2, 3)]

# ---- the runner -------------------------------------------------------------
def done_keys(path):
    """Keys already MEASURED.  A row with ok=false is a cell that failed, not a
    cell that is finished: harvesting its key too would mark it done forever and
    the battery would silently report one bank where the protocol requires two."""
    if not os.path.exists(path): return set()
    ks = set()
    for l in open(path):
        l = l.strip()
        if not l: continue
        try:
            r = json.loads(l)
            if r.get('ok'): ks.add(r['key'])
        except Exception: pass
    return ks

def emit(path, row):
    with open(path, 'a') as f:
        f.write(json.dumps(row) + '\n')

def run_match(key, path, meta, a, b, deals, seed, partners='', partnersb='',
              extra=(), rotations=2, threads=THREADS):
    argv = [FISH, 'match', '--a=' + a, '--b=' + b, '--games=%d' % deals,
            '--rotations=%d' % rotations, '--seed=%d' % seed,
            '--threads=%d' % threads, '--json']
    if partners:  argv.insert(4, '--partners=' + partners)
    if partnersb: argv.insert(4, '--partnersb=' + partnersb)
    argv += list(extra)
    t0 = time.time()
    r = subprocess.run(argv, capture_output=True, text=True, env=ENV, cwd=ENG)
    if r.returncode != 0 or not r.stdout.strip().startswith('{'):
        row = dict(meta); row.update(key=key, argv=argv, ok=False,
                                     rc=r.returncode, err=(r.stderr or r.stdout)[:800])
        emit(path, row)
        print('  FAIL %s rc=%d %s' % (key, r.returncode, (r.stderr or r.stdout)[:200]), flush=True)
        return None
    m = json.loads(r.stdout)
    row = dict(meta)
    row.update(key=key, argv=argv, ok=True, wall=round(time.time() - t0, 2), match=m)
    emit(path, row)
    edge = 100 * (m['winRateA'] - 0.5)
    lo, hi = 100 * (m['ci'][0] - 0.5), 100 * (m['ci'][1] - 0.5)
    print('  %-46s %+7.3f [%+7.3f,%+7.3f]  n=%d  %.0fs' %
          (key, edge, lo, hi, m['games'], time.time() - t0), flush=True)
    return m

# =============================================================================
# B2 -- headline strength against the frontier            PREREGISTRATION 4/B2
# =============================================================================
def B2():
    path = os.path.join(RES, 'P5-B2.jsonl'); have = done_keys(path)
    cells = [('B2.1', INCUMBENT, 12000), ('B2.2', FCHEAP, 12000),
             ('B2.3', FMID, 3000),       ('B2.4', COMPOSITE, 12000),
             ('B2.5', 'v05', 6000)]
    for cell, opp, deals in cells:
        for bank in (BANK1, BANK2):
            key = '%s|%d' % (cell, bank)
            if key in have: continue
            run_match(key, path, dict(battery='B2', cell=cell, arm='FROZEN', armSpec=FROZEN,
                                      opp=opp, bank=bank, deals=deals),
                      FROZEN, opp, deals, bank)

# =============================================================================
# B3 -- the panel: worst case and minimax regret          PREREGISTRATION 4/B3
# =============================================================================
def b3_panel():
    """31 members with the near/far/expensive assignment FIXED BY NAME in the
    preregistration, not decided from any holdout margin."""
    near = [('F-fast', 'v06'), ('F-cheap', FCHEAP), ('v05', 'v05'), ('v04', 'v04'),
            ('feint', 'feint')] + [('SEALED:' + i, s) for i, c, s in SEALED]
    far  = [(n, n) for n in ['v03', 'v02', 'lockout', 'detective', 'hunter',
                             'diversifier', 'bluffer', 'silent', 'withholder', 'random']]
    exp  = [('F-mid', FMID), ('composite', COMPOSITE)]
    out = []
    for n, s in near: out.append((n, s, 'near', 6000))
    for n, s in far:  out.append((n, s, 'far', 1500))
    for n, s in exp:  out.append((n, s, 'expensive', 1200))
    assert len(out) == 31, len(out)
    return out

def B3():
    path = os.path.join(RES, 'P5-B3.jsonl'); have = done_keys(path)
    arms = [('FROZEN', FROZEN), ('INCUMBENT', INCUMBENT),
            ('F-cheap', FCHEAP), ('composite', COMPOSITE)]
    panel = b3_panel()
    for armName, armSpec in arms:
        for pName, pSpec, cls, deals in panel:
            for bank in (BANK1, BANK2):
                key = 'B3|%s|%s|%d' % (armName, pName, bank)
                if key in have: continue
                run_match(key, path, dict(battery='B3', arm=armName, armSpec=armSpec,
                                          opp=pName, oppSpec=pSpec, cls=cls,
                                          bank=bank, deals=deals),
                          armSpec, pSpec, deals, bank)

# =============================================================================
# B4 -- a fresh adversary search against the frozen configuration   PREREG 4/B4
# =============================================================================
# The eight searches, with the two rows that deviate from the template written
# into the row exactly as the preregistration writes them.
Z = [
 ('Z01', 'C1 in-class',  'v06',                 'win',     BANK_ADVFIT, '0/4', True,  0.08),
 ('Z02', 'C2 extended',  'v07',                 'win',     BANK_ADVFIT, '1/4', True,  0.08),
 ('Z03', 'C1',           'v06',                 'declerr', BANK_ADVFIT, '2/4', True,  0.08),
 ('Z04', 'C1',           'v06',                 'events',  BANK_ADVFIT, '3/4', True,  0.08),
 ('Z05', 'C1',           'v06',                 'forced',  BANK_ADV,    '0/4', True,  0.08),
 ('Z06', 'C2 wide sigma','v07',                 'win',     BANK_ADV,    '1/4', True,  0.30),
 ('Z07', 'C1 v05 basin', 'v06',                 'win',     BANK_ADV,    '2/4', False, 0.08),
 ('Z08', 'C5 white-box', 'v07i:idet=48,imodel=v06', 'win', BANK_ADV,    '3/4', True,  0.08),
]

def B4fit():
    log = os.path.join(RES, 'P5-B4fits.jsonl'); have = done_keys(log)
    panel = FROZEN.replace(',', '+')     # PREREG: "FROZEN spec with commas rewritten to +"
    for zid, cls, base, kpi, bank, shard, fromv6, sigrel in Z:
        key = 'B4fit|%s' % zid
        if key in have: continue
        out = os.path.join(RES, 'P5-%s.jsonl' % zid)
        argv = [FISH, 'tune', '--panel=' + panel, '--base=' + base, '--full']
        if fromv6: argv.append('--fromv6')
        argv += ['--kpi=' + kpi, '--obj=min', '--paired', '--beta=1',
                 '--sigmarel=%g' % sigrel, '--games=120', '--pop=12', '--elite=5',
                 '--gens=8', '--seed=%d' % bank, '--shard=' + shard,
                 '--threads=%d' % THREADS, '--out=' + out]
        print('=== %s  %s  base=%s kpi=%s bank=%d shard=%s sigmarel=%g fromv6=%s' %
              (zid, cls, base, kpi, bank, shard, sigrel, fromv6), flush=True)
        t0 = time.time()
        r = subprocess.run(argv, capture_output=True, text=True, env=ENV, cwd=ENG)
        w = ''
        for l in r.stdout.splitlines():
            if l.startswith('weights='): w = l[len('weights='):].strip()
        ok = bool(w)
        sep = ',' if ':' in base else ':'
        spec = base + sep + 'allparams=' + w if ok else ''
        emit(log, dict(battery='B4fit', key=key, id=zid, cls=cls, base=base, kpi=kpi,
                       fitBank=bank, shard=shard, sigmaRel=sigrel, fromv6=fromv6,
                       argv=argv, ok=ok, seconds=round(time.time() - t0, 1),
                       spec=spec, rc=r.returncode, err=('' if ok else (r.stderr or '')[:600])))
        print('    %s %s in %.0fs' % (zid, 'fitted' if ok else 'FAILED', time.time() - t0), flush=True)

def B4eval():
    fits = {json.loads(l)['id']: json.loads(l) for l in open(os.path.join(RES, 'P5-B4fits.jsonl'))
            if l.strip() and json.loads(l).get('battery') == 'B4fit'}
    path = os.path.join(RES, 'P5-B4eval.jsonl'); have = done_keys(path)
    for zid, cls, base, kpi, bank, shard, fromv6, sigrel in Z:
        f = fits.get(zid)
        if not f or not f['ok']:
            print('  skip %s (no fit)' % zid, flush=True); continue
        for evbank in (BANK1, BANK2):
            key = 'B4|%s|%d' % (zid, evbank)
            if key in have: continue
            run_match(key, path, dict(battery='B4', cell=zid, cls=cls, kpi=kpi,
                                      advSpec=f['spec'], fitBank=bank, shard=shard,
                                      bank=evbank, deals=6000),
                      f['spec'], FROZEN, 6000, evbank)

# =============================================================================
# B5 -- ablations: is the gain attributable?              PREREGISTRATION 4/B5
# =============================================================================
B5_CELLS = [
 # add-one-in from v06
 ('A-search',  'add',  FCHEAP),
 ('A-rtie',    'add',  'v06:rtie=1'),
 ('A-urgoff',  'add',  'v06:' + URGOFF),
 ('A-stall',   'add',  'v06:stall=12'),
 ('A-r12',     'add',  'v07:r12=25'),
 ('A-m2',      'add',  'v06:m2=0'),
 # leave-one-out from FROZEN
 ('L-search',  'loo',  'v07:r12=25,rtie=1,' + URGOFF + ',stall=12'),
 ('L-rtie',    'loo',  'v07:r12=25,' + URGOFF + ',stall=12,' + SEARCH),
 ('L-urgoff',  'loo',  'v07:r12=25,rtie=1,stall=12,' + SEARCH),
 ('L-stall',   'loo',  'v07:r12=25,rtie=1,' + URGOFF + ',' + SEARCH),
 ('L-r12',     'loo',  'v07:rtie=1,' + URGOFF + ',stall=12,' + SEARCH),
 # the reference the leave-one-out drops are taken from.  ADDED CELL: the eleven
 # cells the preregistration names cannot yield a leave-one-out DROP without it.
 ('REF-FROZEN','ref',  FROZEN),
]

def B5():
    path = os.path.join(RES, 'P5-B5.jsonl'); have = done_keys(path)
    for cid, kind, spec in B5_CELLS:
        for bank in (BANK3, BANK1):      # PREREG B5: 7090003 primary, 7090001 replicate
            key = 'B5|%s|%d' % (cid, bank)
            if key in have: continue
            run_match(key, path, dict(battery='B5', cell=cid, kind=kind, armSpec=spec,
                                      opp=INCUMBENT, bank=bank, deals=6000),
                      spec, INCUMBENT, 6000, bank)

# =============================================================================
# B6 -- the partner-regime table                          PREREGISTRATION 4/B6
# =============================================================================
def B6():
    path = os.path.join(RES, 'P5-B6.jsonl'); have = done_keys(path)
    arms_v05 = [('FROZEN', FROZEN), ('INCUMBENT', INCUMBENT), ('v05', 'v05')]
    partners_v05 = ['itself', 'v06', 'v05', 'v04', 'v03', 'detective', 'withholder', 'lockout']
    arms_v06 = [('FROZEN', FROZEN), ('INCUMBENT', INCUMBENT)]
    partners_v06 = ['itself', 'v06', 'v03', 'detective']
    for opp, arms, partners in (('v05', arms_v05, partners_v05),
                                ('v06', arms_v06, partners_v06)):
        for armName, armSpec in arms:
            for p in partners:
                pSpec = armSpec if p == 'itself' else p
                for bank in (BANK1, BANK2):
                    key = 'B6|%s|%s|%s|%d' % (opp, armName, p, bank)
                    if key in have: continue
                    run_match(key, path, dict(battery='B6', opp=opp, arm=armName,
                                              armSpec=armSpec, partners=p, partnersSpec=pSpec,
                                              bank=bank, deals=6000),
                              armSpec, opp, 6000, bank, partners=pSpec)

# =============================================================================
# B7 -- cross-play between independently-trained runs     PREREGISTRATION 4/B7
# =============================================================================
def B7():
    path = os.path.join(RES, 'P5-B7.jsonl'); have = done_keys(path)
    for i in range(3):
        for j in range(3):
            for bank in (BANK1, BANK2):
                key = 'B7|xp%d+xp%d|%d' % (i + 1, j + 1, bank)
                if key in have: continue
                run_match(key, path, dict(battery='B7', kind='crossplay',
                                          arm='xp%d' % (i + 1), partners='xp%d' % (j + 1),
                                          diagonal=(i == j), bank=bank, deals=6000,
                                          armSpec=XP[i], partnersSpec=XP[j]),
                          XP[i], 'v05', 6000, bank, partners=XP[j])
    for i in range(3):
        for j in range(i + 1, 3):
            for bank in (BANK1, BANK2):
                key = 'B7h2h|xp%d-xp%d|%d' % (i + 1, j + 1, bank)
                if key in have: continue
                run_match(key, path, dict(battery='B7', kind='h2h',
                                          arm='xp%d' % (i + 1), opp='xp%d' % (j + 1),
                                          bank=bank, deals=6000,
                                          armSpec=XP[i], oppSpec=XP[j]),
                          XP[i], XP[j], 6000, bank)

# =============================================================================
# B8 -- the rule-dialect table                            PREREGISTRATION 4/B8
# =============================================================================
B8_ROWS = [('default', []), ('no-out-of-turn', ['--no-out-of-turn']),
           ('no-cardless-declare', ['--no-cardless-declare']),
           ('maxasks=360', ['--maxasks=360']), ('arb=high', ['--arb=high']),
           ('arb=turn', ['--arb=turn']), ('sets=8', ['--sets=8']),
           ('legacy', ['--legacy'])]

def B8():
    path = os.path.join(RES, 'P5-B8.jsonl'); have = done_keys(path)
    for name, flags in B8_ROWS:
        for bank in (BANK3, BANK1):     # the dialect table's bank is 7090003 (PREREG 2.1)
            key = 'B8|%s|%d' % (name, bank)
            if key in have: continue
            run_match(key, path, dict(battery='B8', row=name, flags=flags,
                                      arm='FROZEN', armSpec=FROZEN, opp=INCUMBENT,
                                      bank=bank, deals=6000),
                      FROZEN, INCUMBENT, 6000, bank, extra=flags)

# =============================================================================
# B9 -- negative controls                                 PREREGISTRATION 4/B9
# =============================================================================
HRUNGS = [('0.08', 'B9.2'), ('0.11', 'B9.2'), ('0.15', 'B9.2'), ('0.05', 'B9.3')]

def B9fit():
    log = os.path.join(RES, 'P5-B9fits.jsonl'); have = done_keys(log)
    for h, cell in HRUNGS:
        key = 'B9fit|%s' % h
        if key in have: continue
        target = (FROZEN + ',hcap=decl,hstr=' + h).replace(',', '+')
        out = os.path.join(RES, 'P5-B9-h%s.jsonl' % h.replace('.', ''))
        argv = [FISH, 'tune', '--panel=' + target, '--base=v06', '--full', '--fromv6',
                '--kpi=win', '--obj=min', '--paired', '--beta=1', '--sigmarel=0.08',
                '--games=120', '--pop=12', '--elite=5', '--gens=8',
                '--seed=%d' % BANK_CTRL, '--threads=%d' % THREADS, '--out=' + out]
        print('=== B9 responder fit, hstr=%s (%s)' % (h, cell), flush=True)
        t0 = time.time()
        r = subprocess.run(argv, capture_output=True, text=True, env=ENV, cwd=ENG)
        w = ''
        for l in r.stdout.splitlines():
            if l.startswith('weights='): w = l[len('weights='):].strip()
        ok = bool(w)
        emit(log, dict(battery='B9fit', key=key, hstr=h, cell=cell, argv=argv, ok=ok,
                       seconds=round(time.time() - t0, 1),
                       spec=('v06:allparams=' + w) if ok else '',
                       rc=r.returncode, err=('' if ok else (r.stderr or '')[:600])))
        print('    hstr=%s %s in %.0fs' % (h, 'fitted' if ok else 'FAILED', time.time() - t0), flush=True)

def B9():
    path = os.path.join(RES, 'P5-B9.jsonl'); have = done_keys(path)
    fits = {json.loads(l)['hstr']: json.loads(l)
            for l in open(os.path.join(RES, 'P5-B9fits.jsonl')) if l.strip()}
    for h, cell in HRUNGS:
        tgt = FROZEN + ',hcap=decl,hstr=' + h
        f = fits.get(h)
        for bank in (BANK1, BANK2):
            # the planted cost itself, measured directly as a paired duplicate.
            # ADDED CELL: "the recovered size tracks the planted size" is not
            # computable without the planted size.
            key = 'B9plant|%s|%d' % (h, bank)
            if key not in have:
                run_match(key, path, dict(battery=cell, kind='planted-cost', hstr=h,
                                          bank=bank, deals=6000),
                          FROZEN, tgt, 6000, bank)
            if not f or not f['ok']: continue
            key = 'B9resp-hcap|%s|%d' % (h, bank)
            if key not in have:
                run_match(key, path, dict(battery=cell, kind='responder-vs-handicapped',
                                          hstr=h, advSpec=f['spec'], bank=bank, deals=6000),
                          f['spec'], tgt, 6000, bank)
            # ADDED CELL: B9.3 speaks of the planted excess "resolving to zero",
            # which is the responder's edge against the HANDICAPPED target minus
            # its edge against the unhandicapped one.  The second term needs this.
            key = 'B9resp-clean|%s|%d' % (h, bank)
            if key not in have:
                run_match(key, path, dict(battery=cell, kind='responder-vs-frozen',
                                          hstr=h, advSpec=f['spec'], bank=bank, deals=6000),
                          f['spec'], FROZEN, 6000, bank)
    # B9.4 the identity control
    for bank in (BANK1, BANK2):
        key = 'B9.4|identity|%d' % bank
        if key in have: continue
        run_match(key, path, dict(battery='B9.4', kind='identity', bank=bank, deals=6000),
                  FROZEN, FROZEN, 6000, bank)

def B9side():
    """B9.5 -- the side-channel positive controls."""
    path = os.path.join(RES, 'P5-B9side.jsonl'); have = done_keys(path)
    for cheat in ('seed', 'shared', 'conv'):
        spec = 'v07x:cheat=' + cheat
        for bank in (BANK1, BANK2):
            for tag, tests, thr, fresh in (('s345', 's3,s4,s5', THREADS, False),
                                           ('s6', 's6', 1, True)):
                key = 'B9.5|%s|%s|%d' % (cheat, tag, bank)
                if key in have: continue
                argv = [FISH, 'v7side', '--a=' + spec, '--b=v06', '--games=400',
                        '--seed=%d' % bank, '--tests=' + tests, '--threads=%d' % thr, '--json']
                if fresh: argv.append('--freshagents')
                t0 = time.time()
                r = subprocess.run(argv, capture_output=True, text=True, env=ENV, cwd=ENG)
                line = [l for l in r.stdout.splitlines() if l.strip().startswith('{')]
                ok = bool(line)
                emit(path, dict(battery='B9.5', key=key, cheat=cheat, tests=tests,
                                threads=thr, freshagents=fresh, bank=bank, argv=argv,
                                ok=ok, rc=r.returncode, seconds=round(time.time() - t0, 1),
                                side=(json.loads(line[-1]) if ok else None),
                                err=('' if ok else (r.stderr or '')[:400])))
                print('  %-28s rc=%d %.0fs' % (key, r.returncode, time.time() - t0), flush=True)

# =============================================================================
# B10 -- the S6 residual                                  PREREGISTRATION 4/B10
# =============================================================================
def B10():
    path = os.path.join(RES, 'P5-B10.jsonl'); have = done_keys(path)
    arms = [('FROZEN', FROZEN), ('INCUMBENT', INCUMBENT),
            ('F-cheap', FCHEAP), ('composite', COMPOSITE)]
    for armName, spec in arms:
        for bank in (BANK1, BANK2):
            for cond, fresh in (('threads1', False), ('threads1-freshagents', True)):
                key = 'B10|%s|%s|%d' % (armName, cond, bank)
                if key in have: continue
                argv = [FISH, 'v7side', '--a=' + spec, '--b=v06', '--games=1200',
                        '--seed=%d' % bank, '--tests=s6', '--threads=1', '--json']
                if fresh: argv.append('--freshagents')
                t0 = time.time()
                r = subprocess.run(argv, capture_output=True, text=True, env=ENV, cwd=ENG)
                line = [l for l in r.stdout.splitlines() if l.strip().startswith('{')]
                ok = bool(line)
                j = json.loads(line[-1]) if ok else None
                emit(path, dict(battery='B10', key=key, arm=armName, armSpec=spec,
                                cond=cond, bank=bank, argv=argv, ok=ok, rc=r.returncode,
                                seconds=round(time.time() - t0, 1), side=j,
                                err=('' if ok else (r.stderr or '')[:400])))
                s6 = (j or {}).get('s6', {})
                print('  %-40s mismatch=%s nodes=%s  %.0fs' %
                      (key, s6.get('mismatch'), s6.get('nodes'), time.time() - t0), flush=True)

BATTERIES = dict(B2=B2, B3=B3, B4fit=B4fit, B4eval=B4eval, B5=B5, B6=B6, B7=B7,
                 B8=B8, B9fit=B9fit, B9=B9, B9side=B9side, B10=B10)

if __name__ == '__main__':
    os.makedirs(RES, exist_ok=True)
    for name in sys.argv[1:]:
        if name not in BATTERIES: sys.exit('unknown battery %r' % name)
        print('##### %s  %s' % (name, time.strftime('%H:%M:%S')), flush=True)
        BATTERIES[name]()
        print('##### %s DONE %s' % (name, time.strftime('%H:%M:%S')), flush=True)
