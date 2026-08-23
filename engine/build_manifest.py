#!/usr/bin/env python3
"""Emit a single manifest of every artifact the paper reports from.

The manifest is the source of truth for sample sizes, seeds, modes, commit hash
and artifact checksums, so that prose and tables cannot drift from the runs that
produced them.  Written as JSON and as a LaTeX longtable.

Usage:
    python3 build_manifest.py                     # v0.4 (default, unchanged)
    python3 build_manifest.py v05                 # v0.5
    python3 build_manifest.py --study=v05
    python3 build_manifest.py --results=research/v05/results   # explicit path

A study selects the run table, the results directory and the output LaTeX table;
``--results`` overrides only the directory, so a manifest can be taken of a copy
of the artifacts (a scratch re-run, say) without touching the committed one.
"""
import hashlib, json, os, subprocess, sys, datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TAB = os.path.join(ROOT, 'paper', 'tables')

# id -> (artifact file, command, design description)
RUNS_V04 = [
    ('E1',  'E1-verify.txt',            'fish verify --games=600',
     'engine and information-safety audit, $9\\times9$ policy cross-product, full dialect'),
    ('E1L', 'E1-verify-legacy.txt',     'fish verify --games=300 --legacy',
     'the same audit under the v0.3 rules dialect'),
    ('E2',  'E2-belief-selftest.txt',   'fish selftest --games=40',
     'reference engine vs card-level DP vs exact rejection sampling'),
    ('E3',  'E3-headtohead.jsonl',      'fish match --a=v04:mgate=0.008 --b=OPP --games=700 --rotations=6 --seed=90210',
     'primary head-to-head; 700 deals $\\times$ 6 rotations = 4{,}200 games per opponent'),
    ('E4',  'E4-matrix.json',           'fish matrix --policies=... --games=200 --rotations=6 --seed=515151',
     'round-robin over nine policies; 200 deals $\\times$ 6 rotations per ordered pair'),
    ('E5',  'E5-ablations.json',        'fish ablate --ref=v04:mgate=0.008 --games=500 --rotations=2 --seed=606060',
     'frozen-policy ablations; 500 deals $\\times$ 2 rotations against a four-policy panel'),
    ('E6',  'E6-calibration-v04.txt',   'fish calibrate --a=v04:mgate=0.008 --b=v03 --games=600 --seed=717171',
     'forecast reliability, v0.4-Fast'),
    ('E6b', 'E6-calibration-v03.txt',   'fish calibrate --a=v03 --b=v04:mgate=0.008 --games=600 --seed=717171',
     'forecast reliability, FishBot v0.3'),
    ('E7',  'E7-rules.jsonl',           'fish match ... --seed=828282|838383|848484 (--legacy | --sets=8 | --no-out-of-turn)',
     'rules-dialect sensitivity; 400 deals $\\times$ 6 rotations per cell'),
    ('E8',  'E8-v03-port.jsonl',        'fish match --a=v03 --b=OPP --games=1000 --rotations=2 --seed=20260820 --legacy',
     'v0.3 port validation against the published v0.3 figures'),
    ('E9',  'E9-throughput.txt',        'fish bench --a=... --b=...',
     'whole-game throughput, v0.4-Fast vs v0.4-Block vs v0.3'),
    ('E10', 'E10-lbr.jsonl',            'fish tune --panel=TARGET --seed=515253 ; fish match ... --seed=6543210',
     'local response probe; response fitted, then re-measured on 600 deals $\\times$ 6 rotations'),
    ('E11', 'E11-termination.md',       'see the file',
     'termination study of four forcing rules in mirror self-play'),
    ('E12', 'E12-exactness.txt',        'fish match --a=v04:<greedy>,belief=fast|block --b=v03 --games=400 --rotations=6 --seed=959595',
     'belief substitution under a deliberately unfitted policy'),
    ('E13', 'E13-termination.jsonl',    'fish match --a=v04:mgate=0.008 --b=OPP --games=300 --rotations=6 --seed=464646',
     'action-cap incidence across the population; 300 deals $\\times$ 6 rotations, including the mirror match'),
    ('E14', 'E14-valuefit-stats.txt',   'fish fitvalue --a=v04:mgate=0.008 --b=v04:mgate=0.008 --games=250 --seed=31415',
     'ridge fit of the value function on self-play decision points'),
    ('E15', 'E15-oracle.txt',           'fish oracle --games=150 --maxdeals=200000 --samples=3000',
     'brute-force oracle for the exact reference engine'),
    ('E16', 'E16-gateaudit.txt',        'fish gateaudit --games=700 --rotations=6 --seed=90210',
     'false-negative audit of the declaration pre-gates over the primary bank'),
    ('E17', 'E17-arbitration.jsonl',    'fish match ... --seed=90210 --arb=low|high|turn',
     'sensitivity of the results to the simultaneous-declaration arbitration order'),
]

# The v0.5 battery.  Commands are exactly those in engine/experiments_v05.sh,
# except E10, which is a standalone deception panel run at two seed banks.
RUNS_V05 = [
    ('E1',  'E1-verify.txt',        'fish verify --games=600',
     'engine and information-safety audit over the policy cross-product, full dialect'),
    ('E2',  'E2-pathology.txt',     'fish pathology --a=v05|v04 --b=v05|v04 --games=300 --rotations=2 --seed=31 (and --seed=90210 for the cross match)',
     'the commit-gate KPIs: dead asks, dead runs, repeats, post-horizon and forced declarations'),
    ('E3',  'E3-headtohead.jsonl',  'fish match --a=v05 --b=v04 --games=300 --rotations=6 --seed=90210|31337|515151|777001|424242',
     'held-out head-to-head at five disjoint seed banks; 300 deals $\\times$ 6 rotations each'),
    ('E4',  'E4-perstyle.jsonl',    'fish match --a=v05|v04 --b=OPP --games=300 --rotations=6 --seed=515253',
     'per-opponent profile over nine styles for both arms; the worst case, not the mean, is the headline'),
    ('E5',  'E5-ablations.jsonl',   'fish match --a=v05:<switches> --b=v04 --games=250 --rotations=6 --seed=606060',
     'mechanism ablations: the factorial core of M1/M2/M8 plus the two rejected defaults'),
    ('E6',  'E6-calibration-v05.txt', 'fish calibrate --a=v05 --b=v04 --games=400 --seed=717171',
     'forecast reliability of the declaration estimator'),
    ('E7',  'E7-rules.jsonl',       'fish match --a=v05 --b=v04 --games=250 --rotations=6 --seed=828282 (--no-out-of-turn | --no-cardless-declare | --legacy)',
     'rules-dialect sensitivity across four dialects'),
    ('E8',  'E8-forced-endgame.txt', 'fish match --a=v05 --b=v04 --games=4000 --rotations=6 --seed=909090 (and the swapped arm)',
     'forced-endgame rate and accuracy per declaring team at volume; 24{,}000 games per arm'),
    ('E9',  'E9-throughput.txt',    'fish bench --a=v05 --b=v05 --games=300',
     'whole-game throughput of the shipped configuration'),
    ('E10', 'E10-deception.md',     'fish match --a=v05|v04 --b=silent|feint|withholder --games=400 --rotations=6 --seed=31415926|8675309',
     'deception panel at two independent seed banks; 400 deals $\\times$ 6 rotations per cell'),
    ('C1',  'C1-v04-corrections.md', 'see the file',
     'corrections register of record: every v0.4 claim the v0.5 diagnosis overturns, and every one it fails to overturn'),
]

# Fitting artifacts live under research/<study>/runs/ rather than results/.
RUNS_V05_EXTRA = [
    ('F1', os.path.join('..', 'runs', 'fit-round1.jsonl'),
     'fish tune --base=v05 --panel=v04,v03,lockout,detective,diversifier,hunter --full --games=200 --pop=24 --elite=6 --beta=25 --seed=505101',
     'the CEM fitting trace that produced the shipped vector'),
    ('F2', os.path.join('..', 'runs', 'v05-fitted.txt'),
     'python3 engine/freeze_config_v05.py research/v05/runs/v05-fitted.txt',
     'the frozen 34-coordinate parameter vector baked into V05Config'),
]

STUDIES = {
    'v04': dict(project='FishBot v0.4', results=os.path.join('research', 'v04', 'results'),
                runs=RUNS_V04, extra=[], tex='manifest.tex', diagnosis_glob=None),
    'v05': dict(project='FishBot v0.5', results=os.path.join('research', 'v05', 'results'),
                runs=RUNS_V05, extra=RUNS_V05_EXTRA, tex='manifest_v05.tex',
                # Every diagnosis report is digested too, without a hand-kept
                # list, so a report cannot be quoted from a file the manifest
                # does not cover.
                diagnosis_glob=('P', '.md')),
}

def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 16), b''):
            h.update(chunk)
    return h.hexdigest()

def commit():
    try:
        return subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT,
                              capture_output=True, text=True).stdout.strip()
    except Exception:
        return 'unknown'

def dirty():
    try:
        out = subprocess.run(['git', 'status', '--porcelain'], cwd=ROOT,
                             capture_output=True, text=True).stdout.strip()
        return bool(out)
    except Exception:
        return True

V04_META = dict(
    policy_spec='v04:mgate=0.008',
    belief_mode='Fast (BeliefMode::Fast)',
    fitted_parameters=34, ask_features=20, value_features=16,
    fitting_seeds_recorded=[20260821, 770077, 313131, 888111],
    fitting_round5_seed=None,
    selection_seed=1357911, selected_generation=8,
    evaluation_seeds=dict(headtohead=90210, matrix=515151, ablations=606060,
                          calibration=717171, dialects=[828282, 838383, 848484],
                          port=20260820, unfitted_belief=959595,
                          termination=464646, valuefit=31415, oracle=20260822,
                          exploiter_fit=515253, exploiter_eval=6543210,
                          gateaudit=90210, arbitration=90210),
)

V05_META = dict(
    policy_spec='v05',
    belief_mode='Fast (BeliefMode::Fast)',
    fitted_parameters=34, ask_features=20, value_features=16,
    mechanisms_built=['M1 live-ask gating', 'M2 capacity-feasible allocation',
                      'M8 stage-2 removal'],
    mechanisms_off_by_default=['ownershipByP (m1p)', 'repeatGuard (norepeat)'],
    fitting_seed=505101, fitting_beta=25,
    fitting_panel=['v04', 'v03', 'lockout', 'detective', 'diversifier', 'hunter'],
    evaluation_seeds=dict(pathology=31, pathology_cross=90210,
                          headtohead=[90210, 31337, 515151, 777001, 424242],
                          perstyle=515253, ablations=606060, calibration=717171,
                          dialects=828282, forced_endgame=909090,
                          deception=[31415926, 8675309]),
)

META = {'v04': V04_META, 'v05': V05_META}


def parse_argv(argv):
    """Return (study, results_dir).  Accepts a bare study name, --study=, and
    --results= (a path, absolute or relative to the repository root)."""
    study, results = 'v04', None
    for arg in argv:
        if arg.startswith('--study='):
            study = arg.split('=', 1)[1]
        elif arg.startswith('--results='):
            results = arg.split('=', 1)[1]
        elif arg.startswith('--'):
            sys.exit('unknown option %s; usage: build_manifest.py [v04|v05] '
                     '[--study=V] [--results=DIR]' % arg)
        else:
            study = arg
    if study not in STUDIES:
        sys.exit('unknown study %r; known studies: %s'
                 % (study, ', '.join(sorted(STUDIES))))
    if results is None:
        results = os.path.join(ROOT, STUDIES[study]['results'])
    elif not os.path.isabs(results):
        results = os.path.join(ROOT, results)
    return study, os.path.abspath(results)


def digest(res, eid, fname, cmd, design):
    path = os.path.join(res, fname)
    rec = dict(id=eid, artifact=os.path.normpath(fname), command=cmd, design=design)
    if not os.path.exists(path):
        rec['present'] = False
        return rec
    rec.update(present=True, bytes=os.path.getsize(path), sha256=sha(path))
    return rec


def main(argv=None):
    study, res = parse_argv(argv if argv is not None else sys.argv[1:])
    spec = STUDIES[study]
    # The LaTeX table is shared state under paper/tables/.  Only a manifest of
    # the study's canonical results directory may write it; pointing --results
    # at a copy produces the JSON beside that copy and nothing else, so a
    # scratch run cannot clobber the table the paper inputs.
    canonical = os.path.abspath(os.path.join(ROOT, spec['results'])) == res

    entries = [digest(res, *r) for r in spec['runs']]
    entries += [digest(res, *r) for r in spec['extra']]

    # Diagnosis corpus: every report whose name starts with the configured
    # prefix, digested automatically so the list cannot go stale.
    if spec['diagnosis_glob'] and os.path.isdir(res):
        prefix, suffix = spec['diagnosis_glob']
        named = {e['artifact'] for e in entries}
        for fname in sorted(os.listdir(res)):
            if (fname.startswith(prefix) and fname.endswith(suffix)
                    and fname not in named):
                entries.append(digest(res, 'D', fname, 'see the file',
                                      'diagnosis report'))

    man = dict(project=spec['project'],
               generated=datetime.datetime.now().isoformat(timespec='seconds'),
               commit=commit(), working_tree_dirty=dirty(),
               results_dir=os.path.relpath(res, ROOT),
               **META[study])
    man['runs'] = entries
    os.makedirs(res, exist_ok=True)
    with open(os.path.join(res, 'MANIFEST.json'), 'w') as f:
        json.dump(man, f, indent=2)
        f.write('\n')

    lines = [r'\begin{longtable}{@{}llp{0.40\linewidth}l@{}}', r'\toprule',
             r'ID & Artifact & Design & SHA-256 (first 12) \\', r'\midrule', r'\endhead']
    for e in entries:
        h = e.get('sha256', '')[:12] if e.get('present') else 'missing'
        lines.append(f"{e['id']} & \\filepath{{{e['artifact']}}} & {e['design']} & \\texttt{{{h}}} \\\\")
    lines += [r'\bottomrule', r'\end{longtable}']
    if canonical:
        os.makedirs(TAB, exist_ok=True)
        open(os.path.join(TAB, spec['tex']), 'w').write('\n'.join(lines) + '\n')
    where = os.path.join(res, 'MANIFEST.json')
    try:
        where = os.path.join(os.path.relpath(res, ROOT), 'MANIFEST.json')
    except ValueError:
        pass
    print('wrote %s%s; %d of %d artifacts present'
          % (where, ' and paper/tables/%s' % spec['tex'] if canonical else '',
             sum(1 for e in entries if e.get('present')), len(entries)))

if __name__ == '__main__':
    main()
