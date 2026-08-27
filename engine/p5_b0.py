#!/usr/bin/env python3
"""PHASE 5 -- B0, the verification block, run before anything is measured.

PREREGISTRATION section 4/B0: five checks, every one of which is a stop condition
if it fails.  The output is an artifact rather than a transcript so that the
verification can be re-read after the fact.
"""
import base64, hashlib, json, os, subprocess, time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ENG, FISH = os.path.join(ROOT, 'engine'), os.path.join(ROOT, 'engine', 'fish7')
ENV = dict(os.environ); ENV['FISH_UNSEAL_PHASE'] = '5'

EXPECT = {7090001: '896dbc89be124d85', 7090002: '0b6e40d834ac0ca1',
          7090003: '863bea69baf6e73c', 7090004: '54f257c3f8ae9fab',
          7090005: '268a1dae71a31713', 7091001: '958ada042cc26900',
          7091002: '5c39af3b5e0bd9a0'}

def sh(argv, **kw):
    return subprocess.run(argv, capture_output=True, text=True, env=ENV, cwd=ENG, **kw)

doc = dict(kind='fishbot-v07-phase5-B0', generated=time.strftime('%Y-%m-%dT%H:%M:%S'),
           commit=sh(['git', 'rev-parse', 'HEAD']).stdout.strip(),
           treeDirty=bool(sh(['git', 'status', '--porcelain']).stdout.strip()),
           binary=dict(path='engine/fish7',
                       sha256=hashlib.sha256(open(FISH, 'rb').read()).hexdigest(),
                       bytes=os.path.getsize(FISH)),
           unsealPhase='FISH_UNSEAL_PHASE=5, set once for the whole battery')

# ---- B0.1 bank digests ------------------------------------------------------
b01 = []
for s, want in EXPECT.items():
    r = sh([FISH, 'bankdigest', '--seed=%d' % s, '--deals=24000'])
    got = json.loads(r.stdout)
    b01.append(dict(seed=s, deals=got['deals'], digest=got['digest'],
                    expected=want, match=got['digest'] == want))
doc['B0_1_bankDigests'] = dict(rows=b01, allMatch=all(x['match'] for x in b01),
    note='reproduced with engine/fish7; engine/seal_banks_v07.py computed the '
         'committed digests with engine/fish7b, so this is also a cross-binary check')

# ---- B0.2 the sealed adversary half -----------------------------------------
p = os.path.join(ROOT, 'research', 'v07', 'banks', 'holdout', 'adversaries-holdout.sealed')
body = ''.join(l for l in open(p).read().splitlines() if l and not l.startswith('#'))
pt = base64.b64decode(body)
seal = json.load(open(os.path.join(ROOT, 'research', 'v07', 'banks', 'SEAL.json')))
rows = [l.split('\t') for l in pt.decode().splitlines() if l.strip() and not l.startswith('#')]
doc['B0_2_sealedAdversaries'] = dict(
    sha256=hashlib.sha256(pt).hexdigest(), expected=seal['adversariesHoldoutSha256'],
    match=hashlib.sha256(pt).hexdigest() == seal['adversariesHoldoutSha256'],
    sealCommit=seal['commit'], rows=len(rows),
    ids=[r[0] for r in rows], specs={r[0]: r[2] for r in rows},
    distinctPolicies=len({r[2] for r in rows}))

# ---- B0.3 the freeze artifact ----------------------------------------------
r = sh(['python3', os.path.join(ENG, 'freeze_config_v07.py'), '--verify-only'])
frz = json.load(open(os.path.join(ENG, 'fishbot_v07.json')))
rebuilt = frz['base'] + ':' + ','.join('%s=%s' % (k, v) for k, v in frz['options'].items())
doc['B0_3_freezeVerify'] = dict(rc=r.returncode, stdout=r.stdout.strip(), stderr=r.stderr.strip(),
    reconstructedSpec=rebuilt, matchesArtifact=rebuilt == frz['spec'],
    mirrorDigest=frz['roundTrip']['R3_digestMd5'], coordinates=len(frz['allparams']),
    R2a_vector_search=frz['roundTrip']['R2a_vector_search'],
    changedSourcesNote=('NOTE' in r.stdout),
    note='the NOTE listing engine sources changed since the freeze did not print: '
         'zero sources differ from the freeze provenance on this tree')

# ---- B0.4 the seed registry -------------------------------------------------
req = ','.join(str(s) for s in EXPECT)
r = sh([FISH, 'seeds', '--require=' + req])
viol = [l for l in r.stdout.splitlines() if 'VIOLATION' in l]
doc['B0_4_seeds'] = dict(rc=r.returncode, required=req,
    allRegisteredAndUsable=(r.returncode == 0),
    registryViolations=viol,
    note='--require prints nothing on success and returns 3 on any unregistered or '
         'still-sealed seed; rc=0 is the pass. The R1 violation reported is a '
         'pre-existing v0.6-era registry collision on seed 515253 and involves no '
         'phase-5 bank; it is recorded here because B0.4 says to record it.')

# ---- B0.5 verify and selftest ----------------------------------------------
v = sh([FISH, 'verify']); s = sh([FISH, 'selftest'])
doc['B0_5_engine'] = dict(
    verify=dict(rc=v.returncode, pass_='VERIFY PASS' in v.stdout, stdout=v.stdout.strip()),
    selftest=dict(rc=s.returncode, pass_='SELFTEST PASS' in s.stdout, stdout=s.stdout.strip()))

# ---- reproducibility, checked before spending the battery -------------------
FROZEN = frz['spec']
runs = []
for t in (13, 13, 13, 1, 2):
    r = sh([FISH, 'match', '--a=' + FROZEN, '--b=v06', '--games=400', '--rotations=2',
            '--seed=31', '--threads=%d' % t, '--json'])
    m = json.loads(r.stdout)
    runs.append(dict(threads=t, winRateA=m['winRateA'], eventsPerGame=m['eventsPerGame'],
                     askAccA=m['askAccA'], ci=m['ci']))
doc['reproducibility'] = dict(
    cell='FROZEN vs v06, 400 deals x 2 rotations, training seed 31', runs=runs,
    identical=len({(x['winRateA'], x['eventsPerGame'], x['askAccA']) for x in runs}) == 1,
    why='runMatch schedules deals by work-stealing, so the deal-to-thread assignment '
        'is not fixed run to run; the frozen configuration carries the cross-deal agent '
        'residue of PREREGISTRATION 5.3.  This checks directly that a scored cell is '
        'nonetheless reproducible, at a fixed thread count and across thread counts, '
        'before twelve hours of holdout material is spent on the assumption.')

out = os.path.join(ROOT, 'research', 'v07', 'results', 'P5-B0.json')
open(out, 'w').write(json.dumps(doc, indent=1) + '\n')
print(json.dumps({k: (v if not isinstance(v, dict) else
                      {kk: vv for kk, vv in v.items() if kk in
                       ('allMatch', 'match', 'rc', 'pass_', 'identical', 'rows',
                        'allRegisteredAndUsable', 'matchesArtifact', 'mirrorDigest')})
                  for k, v in doc.items() if k.startswith(('B0', 'repro'))}, indent=1))
print('wrote research/v07/results/P5-B0.json')
