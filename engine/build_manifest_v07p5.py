#!/usr/bin/env python3
"""Emit a single manifest of every artifact the FishBot v0.7 PHASE-5 report reads from.

Same pattern as engine/build_manifest.py: the manifest is the source of truth for
sample sizes, seeds, modes, commit hash and artifact checksums, so that prose and
tables cannot drift from the runs that produced them.  Written as JSON beside the
results, and as a LaTeX longtable under paper/tables/.

Phase 5 differs from the earlier studies in one way that the manifest records
explicitly: every bank is SEALED material, so the manifest carries the bank
digests and the sealed-adversary SHA-256 that B0 reproduced, not only the file
checksums.

Usage:  python3 build_manifest_v07p5.py [--results=DIR]
"""
import hashlib, json, os, subprocess, sys, datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TAB = os.path.join(ROOT, 'paper', 'tables')
RESULTS = os.path.join('research', 'v07', 'results')
TEX = 'manifest_v07p5.tex'

FROZEN = ('v07:r12=25,rtie=1,pool=-1,oppfloor=-1,force=1000000,askfloor=-1,stall=12,'
          's1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26')

# id -> (artifact file, command, design description)
RUNS = [
 ('B0',  'P5-B0.json',
  'fish7 bankdigest --seed=S --deals=24000 (x7); sha256 of the decoded sealed half; '
  'freeze_config_v07.py --verify-only; fish7 seeds --require=...; fish7 verify; fish7 selftest',
  'the verification block, run before anything is measured: seven bank digests against the '
  'phase-2 commitment, the sealed adversary half against SEAL.json, the freeze artifact\'s '
  'R1/R2a/R3 round trips, the seed registry, and the engine self-audit'),
 ('B1',  'P5-gate.jsonl',
  'engine/gate_v07.sh --spec=SPEC --id=NAME  (pathology --games=400 --rotations=2 --seed=31 '
  '--threads=13, plus v7side on both TRAINING banks with S6 at --threads=1 --freshagents)',
  'the commit gate, before any strength number: G1-G6, G7a, G7b for FROZEN, INCUMBENT, '
  'F-cheap and the negative control the gate must reject (B9.1)'),
 ('B1t', 'P5-gate.txt', 'see P5-gate.jsonl', 'the human-readable gate digest with each threshold\'s provenance'),
 ('B2',  'P5-B2.jsonl',
  'fish7 match --a=FROZEN --b=OPP --games=D --rotations=2 --seed=BANK --threads=13 --json',
  'headline strength against the frontier; paired duplicate, both primary banks 7090001 and '
  '7090002; 12,000 deals a bank against v06 / F-cheap / the phase-2 composite, 3,000 against '
  'F-mid, 6,000 against v05'),
 ('B3',  'P5-B3.jsonl',
  'fish7 match --a=ARM --b=PANEL_MEMBER --games=D --rotations=2 --seed=BANK --threads=13 --json',
  'the shared panel: four arms (FROZEN, INCUMBENT, F-cheap, the phase-2 composite) against 31 '
  'panel members (14 sealed adversaries, 13 scripted archetypes, 4 frontier points) on both '
  'primary banks; near 6,000 deals a bank, far 1,500, expensive 1,200'),
 ('B4f', 'P5-B4fits.jsonl',
  'fish7 tune --panel=<FROZEN, commas as +> --base=B --full [--fromv6] --kpi=K --obj=min '
  '--paired --beta=1 --sigmarel=S --games=120 --pop=12 --elite=5 --gens=8 --seed=BANK '
  '--shard=s/4 --threads=13 --out=P5-Z<NN>.jsonl',
  'eight independent fresh adversary searches against the frozen configuration, sharded across '
  'the two sealed fitting banks; 8 x 12 x 120 deals x 2 rotations = 23,040 games each'),
 ('B4e', 'P5-B4eval.jsonl',
  'fish7 match --a=<fitted adversary> --b=FROZEN --games=6000 --rotations=2 --seed=BANK '
  '--threads=13 --json',
  'each fitted adversary re-measured against the frozen configuration on the two EVALUATION '
  'banks, disjoint from the banks it was fitted on; 24,000 games an arm'),
 ('B5',  'P5-B5.jsonl',
  'fish7 match --a=VARIANT --b=v06 --games=6000 --rotations=2 --seed=BANK --threads=13 --json',
  'the attribution lattice: six add-one-in cells from v06, five leave-one-out cells from FROZEN, '
  'and the FROZEN reference the drops are taken from; banks 7090003 and 7090001'),
 ('B6',  'P5-B6.jsonl',
  'fish7 match --a=ARM --partners=P --b=v05|v06 --games=6000 --rotations=2 --seed=BANK '
  '--threads=13 --json',
  'the partner-regime table: three arms at all eight partner settings against v05, and two arms '
  'at four settings against v06; 24,000 games a cell'),
 ('B7',  'P5-B7.jsonl',
  'fish7 match --a=run_i --partners=run_j --b=v05 ... ; and each unordered pair head to head',
  'cross-play between three independently-trained runs of the frozen architecture (disjoint '
  'fitting banks, different CEM trajectories, one different starting basin)'),
 ('B8',  'P5-B8.jsonl',
  'fish7 match --a=FROZEN --b=v06 --games=6000 --rotations=2 --seed=BANK --threads=13 --json '
  '[--no-out-of-turn|--no-cardless-declare|--maxasks=360|--arb=high|--arb=turn|--sets=8|--legacy]',
  'the rule-dialect table, unbundled: both arms play the same dialect, eight rows'),
 ('B9f', 'P5-B9fits.jsonl',
  'fish7 tune --panel=<FROZEN,hcap=decl,hstr=H as +> --base=v06 --full --fromv6 --kpi=win '
  '--obj=min --paired --beta=1 --sigmarel=0.08 --games=120 --pop=12 --elite=5 --gens=8 '
  '--seed=7090005 --threads=13',
  'C1 responders fitted against the planted-weakness rungs H in {0.08, 0.11, 0.15} and against '
  'the sub-floor rung H=0.05'),
 ('B9',  'P5-B9.jsonl',
  'fish7 match ... --games=6000 --rotations=2 --seed=7090001|7090002',
  'planted-edge recovery (B9.2), the sub-floor rung that must NOT be recovered (B9.3), and the '
  'identity control (B9.4)'),
 ('B9s', 'P5-B9side.jsonl',
  'fish7 v7side --a=v07x:cheat=seed|shared|conv --b=v06 --games=400 --seed=BANK '
  '--tests=s3,s4,s5 --threads=13 ; and --tests=s6 --threads=1 --freshagents',
  'the three calibrated side-channel positive controls (B9.5)'),
 ('B10', 'P5-B10.jsonl',
  'fish7 v7side --a=ARM --b=v06 --games=1200 --seed=BANK --tests=s6 --threads=1 [--freshagents]',
  'the S6 residual on holdout material, in both conditions: one thread alone, and one thread '
  'with agents rebuilt per deal'),
 ('T',   'P5-TABLES.txt', 'python3 engine/p5_analyse.py',
  'the reduction of every artifact above to the tables the report prints'),
]

def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 16), b''): h.update(chunk)
    return h.hexdigest()

def git(*a):
    try: return subprocess.run(['git'] + list(a), cwd=ROOT, capture_output=True,
                               text=True).stdout.strip()
    except Exception: return 'unknown'

def digest(res, eid, fname, cmd, design):
    p = os.path.join(res, fname)
    rec = dict(id=eid, artifact=os.path.normpath(fname), command=cmd, design=design)
    if not os.path.exists(p):
        rec['present'] = False; return rec
    rec.update(present=True, bytes=os.path.getsize(p), sha256=sha(p))
    if fname.endswith('.jsonl'):
        rec['rows'] = sum(1 for l in open(p) if l.strip())
    return rec

META = dict(
    project='FishBot v0.7 -- phase 5, frozen final evaluation',
    protocol='docs/v07/PREREGISTRATION.md',
    frozen_spec=FROZEN,
    frozen_artifact='engine/fishbot_v07.json',
    mirror_pathology_digest='5f81f440fc9c272a87e87c05fecc7b74',
    coordinates=55,
    threads_scored_cells=13,
    threads_s6_gate=1,
    rotations=2,
    unseal='FISH_UNSEAL_PHASE=5, set once for the whole battery',
    banks=dict(primary=7090001, replicate=7090002, lattice_and_dialects=7090003,
               fresh_search=7090004, negative_controls=7090005,
               sealed_adversary_eval=7091001, sealed_adversary_fit=7091002),
    bank_digests={'7090001': '896dbc89be124d85', '7090002': '0b6e40d834ac0ca1',
                  '7090003': '863bea69baf6e73c', '7090004': '54f257c3f8ae9fab',
                  '7090005': '268a1dae71a31713', '7091001': '958ada042cc26900',
                  '7091002': '5c39af3b5e0bd9a0'},
    sealed_adversaries_sha256=('1ca0346a332586c70a750f1523b105485322af34ff31aab3b9e77a2f0a3b6c52'),
    seal_commit='f4581da58ff0a4408cd222d3313e26a43ad0101a',
    frontier=dict(INCUMBENT='v06',
                  F_cheap='v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26',
                  F_mid='v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26',
                  composite=('v07:m2=0,r12=25,s1=1,det=12,cand=4,kappa=2.5,'
                             'rbelief=indep,depth=12,maxq=26')),
    thresholds=dict(certified_advancement_bar=1.53, exploitability_detection_floor=1.53,
                    declaration_family_floor=2.13, S1_min_not_below=-1.0,
                    S1_positive_rows_required=5, S2_offdiagonal_within=1.5,
                    S5_row_within_of_default=2.0),
    interval='deal-clustered bootstrap `ci` from match --json; never `wilsonCI`',
)

def main(argv):
    res = os.path.join(ROOT, RESULTS)
    for a in argv:
        if a.startswith('--results='):
            r = a.split('=', 1)[1]
            res = r if os.path.isabs(r) else os.path.join(ROOT, r)
    res = os.path.abspath(res)
    canonical = res == os.path.abspath(os.path.join(ROOT, RESULTS))
    entries = [digest(res, *r) for r in RUNS]
    # the eight per-search fitting traces, digested without a hand-kept list
    named = {e['artifact'] for e in entries}
    for fn in sorted(os.listdir(res)) if os.path.isdir(res) else []:
        if fn.startswith('P5-Z') or fn.startswith('P5-B9-h'):
            if fn not in named:
                entries.append(digest(res, 'F', fn, 'see B4f / B9f',
                                      'CEM fitting trace: header, one record a generation, and the '
                                      'final weight vector the adversary spec is rebuilt from'))
    man = dict(generated=datetime.datetime.now().isoformat(timespec='seconds'),
               commit=git('rev-parse', 'HEAD'),
               working_tree_dirty=bool(git('status', '--porcelain')),
               results_dir=os.path.relpath(res, ROOT), **META)
    man['runs'] = entries
    os.makedirs(res, exist_ok=True)
    with open(os.path.join(res, 'MANIFEST-P5.json'), 'w') as f:
        json.dump(man, f, indent=2); f.write('\n')
    lines = [r'\begin{longtable}{@{}llp{0.40\linewidth}l@{}}', r'\toprule',
             r'ID & Artifact & Design & SHA-256 (first 12) \\', r'\midrule', r'\endhead']
    for e in entries:
        h = e.get('sha256', '')[:12] if e.get('present') else 'missing'
        lines.append(f"{e['id']} & \\filepath{{{e['artifact']}}} & {e['design']} & \\texttt{{{h}}} \\\\")
    lines += [r'\bottomrule', r'\end{longtable}']
    if canonical:
        os.makedirs(TAB, exist_ok=True)
        open(os.path.join(TAB, TEX), 'w').write('\n'.join(lines) + '\n')
    print('wrote %s/MANIFEST-P5.json%s; %d of %d artifacts present'
          % (os.path.relpath(res, ROOT), ' and paper/tables/%s' % TEX if canonical else '',
             sum(1 for e in entries if e.get('present')), len(entries)))

if __name__ == '__main__':
    main(sys.argv[1:])
