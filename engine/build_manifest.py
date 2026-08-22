#!/usr/bin/env python3
"""Emit a single manifest of every artifact the paper reports from.

The manifest is the source of truth for sample sizes, seeds, modes, commit hash
and artifact checksums, so that prose and tables cannot drift from the runs that
produced them.  Written as JSON and as a LaTeX longtable.
"""
import hashlib, json, os, subprocess, sys, datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
RES = os.path.join(ROOT, 'research', 'v04', 'results')
TAB = os.path.join(ROOT, 'paper', 'tables')

# id -> (artifact file, command, design description)
RUNS = [
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

def main():
    entries = []
    for eid, fname, cmd, design in RUNS:
        path = os.path.join(RES, fname)
        if not os.path.exists(path):
            entries.append(dict(id=eid, artifact=fname, command=cmd, design=design,
                                present=False))
            continue
        entries.append(dict(id=eid, artifact=fname, command=cmd, design=design,
                            present=True, bytes=os.path.getsize(path),
                            sha256=sha(path)))
    man = dict(project='FishBot v0.4',
               generated=datetime.datetime.now().isoformat(timespec='seconds'),
               commit=commit(), working_tree_dirty=dirty(),
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
               runs=entries)
    os.makedirs(RES, exist_ok=True)
    with open(os.path.join(RES, 'MANIFEST.json'), 'w') as f:
        json.dump(man, f, indent=2)
        f.write('\n')

    lines = [r'\begin{longtable}{@{}llp{0.40\linewidth}l@{}}', r'\toprule',
             r'ID & Artifact & Design & SHA-256 (first 12) \\', r'\midrule', r'\endhead']
    for e in entries:
        h = e.get('sha256', '')[:12] if e.get('present') else 'missing'
        art = e['artifact'].replace('_', r'\_')
        lines.append(f"{e['id']} & \\filepath{{{e["artifact"]}}} & {e['design']} & \\texttt{{{h}}} \\\\")
    lines += [r'\bottomrule', r'\end{longtable}']
    os.makedirs(TAB, exist_ok=True)
    open(os.path.join(TAB, 'manifest.tex'), 'w').write('\n'.join(lines) + '\n')
    print('wrote MANIFEST.json and paper/tables/manifest.tex;',
          sum(1 for e in entries if e.get('present')), 'of', len(entries), 'artifacts present')

if __name__ == '__main__':
    main()
