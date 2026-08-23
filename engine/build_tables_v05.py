#!/usr/bin/env python3
"""Generate paper/numbers_v05_generated.tex from the v0.5 experiment artifacts.

Every macro this emits is derived from a file under research/v05/, never typed.
paper/numbers_v05.tex supplies \\providecommand defaults for the whole macro set
(so the document compiles before the battery has run); this file is \\input
afterwards and \\renewcommand's everything the artifacts can settle, so a stale
default can never silently survive into the PDF.

Run from anywhere:  python3 engine/build_tables_v05.py
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RES = os.path.join(ROOT, 'research', 'v05', 'results')
RUNS = os.path.join(ROOT, 'research', 'v05', 'runs')
OUT = os.path.join(ROOT, 'paper', 'numbers_v05_generated.tex')

macros = {}
sources = {}
missing = []


def put(name, value, src):
    macros[name] = value
    sources[name] = src


def group(n):
    """LaTeX thousands separator matching the v0.4 paper's convention."""
    s = str(int(round(n)))
    out, c = '', 0
    for ch in reversed(s):
        if c and c % 3 == 0:
            out = '{,}' + out
        out = ch + out
        c += 1
    return out


def read(path):
    try:
        with open(path) as f:
            return f.read()
    except OSError:
        missing.append(os.path.relpath(path, ROOT))
        return None


# ---------------------------------------------------------------- pathology
PATHO_FIELDS = [
    (r'events/game\s+([\d.]+)', 'Events'),
    (r'asks/game\s+[\d.]+\s+hit rate ([\d.]+)%', 'Hit'),
    (r'DEAD asks\s+\d+\s+\(([\d.]+)%', 'DeadAsk'),
    (r'dead runs\s+\d+\s+mean length [\d.]+\s+longest (\d+)', 'DeadRunLongest'),
    (r'games w/ run>=6\s+\d+\s+\(([\d.]+)%', 'DeadRunGames'),
    (r'starved turns\s+\d+\s+\(([\d.]+)%', 'Starved'),
    (r'repeat \(a,c,t\)\s+\d+\s+\(([\d.]+)%', 'RepeatAsk'),
    (r'asks in own-locked\s+\d+\s+\(([\d.]+)%', 'OwnLocked'),
    (r'declarations\s+\d+\s+wrong \d+ \(([\d.]+)%', 'DeclWrong'),
    (r'at/after ev>=220\s+(\d+)\s+wrong', 'PostHorizonDecl'),
    (r'action-limit games \d+ \(([\d.]+)%', 'CapRate'),
]


def sig(v):
    """Two decimals for a rate, but never rounding a non-zero rate to 0.00 --
    the difference between 'no dead asks' and 'a few' is the whole point."""
    x = float(v)
    if x == 0:
        return '0'
    if abs(x) < 0.01:
        return '%.4f' % x
    if abs(x) >= 100:
        return '%.0f' % x
    return ('%.2f' % x).rstrip('0').rstrip('.')


def parse_pathology(block):
    d = {}
    for pat, key in PATHO_FIELDS:
        m = re.search(pat, block)
        if m:
            d[key] = m.group(1) if key in ('DeadRunLongest', 'PostHorizonDecl') else sig(m.group(1))
    m = re.search(r'games\s+(\d+)\n', block)
    if m:
        d['Games'] = m.group(1)
    # -- fields the results section needs that the abstract and diagnosis did
    #    not.  The event quantiles, the dead-run count, the *rate* at which
    #    post-horizon and forced declarations are wrong (as opposed to how many
    #    there were), and the ask volume.
    m = re.search(r'events/game\s+([\d.]+)\s+median (\d+)\s+p90 (\d+)\s+p99 (\d+)', block)
    if m:
        d['EventsMean'] = '%.1f' % float(m.group(1))
        d['EventsMedian'] = m.group(2)
        d['EventsPninety'] = m.group(3)
        d['EventsPninetynine'] = m.group(4)
    m = re.search(r'asks/game\s+([\d.]+)', block)
    if m:
        d['Asks'] = '%.1f' % float(m.group(1))
    m = re.search(r'dead runs\s+(\d+)', block)
    if m:
        d['DeadRuns'] = group(int(m.group(1)))
    m = re.search(r'declarations\s+(\d+)\s+wrong (\d+)', block)
    if m:
        d['DeclN'] = group(int(m.group(1)))
    m = re.search(r'at/after ev>=220\s+(\d+)\s+wrong (\d+) \(([\d.]+)%', block)
    if m:
        d['PostHorizonWrongN'] = m.group(2)
        d['PostHorizonWrong'] = sig(m.group(3))
    m = re.search(r'forced endgame\s+(\d+)\s+wrong (\d+) \(([\d.]+)%', block)
    if m:
        d['Forced'] = m.group(1)
        d['ForcedWrongN'] = m.group(2)
        d['ForcedWrong'] = sig(m.group(3))
    return d


txt = read(os.path.join(RES, 'E2-pathology.txt'))
if txt:
    blocks = {}
    cur = None
    for line in txt.splitlines():
        if line.startswith('### '):
            cur = line[4:].strip()
            blocks[cur] = []
        elif cur:
            blocks[cur].append(line)
    # `fish pathology --rotations=2` plays each deal at both team orientations.
    # In a MIRROR the two orientations run identical policies and produce
    # byte-identical games, so the run's raw counts are each game counted twice
    # -- verified by re-running the same bank at --rotations=1, which returns
    # exactly half of every count and every rate to the printed digit.  The
    # cross match is not affected: there the orientations differ in which
    # policy sits on which team, and the rates differ accordingly.  This is the
    # same double-counting the corrections register halves for the forced-
    # endgame figure (\numForcedN); counts are reported per distinct game and
    # rates are untouched.
    COUNTS = ('Games', 'DeadRuns', 'DeclN', 'PostHorizonDecl',
              'PostHorizonWrongN', 'Forced', 'ForcedWrongN')
    for label, prefix in (('v0.5 mirror', 'numFive'),
                          ('v0.4 mirror (reference)', 'numFour'),
                          ('v0.5 vs v0.4', 'numCross')):
        if label not in blocks:
            continue
        d = parse_pathology('\n'.join(blocks[label]))
        if 'mirror' in label:
            for k in COUNTS:
                if k not in d:
                    continue
                raw = int(d[k].replace('{,}', ''))
                assert raw % 2 == 0, 'odd mirror count for %s: %d' % (k, raw)
                d[k] = group(raw // 2) if raw >= 1000 else str(raw // 2)
        for k, v in d.items():
            put(prefix + k, v, 'research/v05/results/E2-pathology.txt')
    # The abstract and diagnosis section address the v0.4 mirror directly.
    alias = {'numFourDeadAsk': 'numDeadAsk', 'numFourRepeatAsk': 'numRepeatAsk',
             'numFourDeadRunLongest': 'numDeadRunLongest',
             'numFourDeadRunGames': 'numDeadRunGames',
             'numFourDeclWrong': 'numDeclWrong', 'numFourEvents': 'numMirrorEvents',
             'numFourHit': 'numMirrorHit', 'numFourStarved': 'numStarved',
             'numFourOwnLocked': 'numOwnLocked',
             'numFourPostHorizonWrong': 'numPostHorizonWrong',
             'numFourPostHorizonDecl': 'numPostHorizonDecl',
             'numFourForcedWrong': 'numForcedWrongRate'}
    for src_name, dst in alias.items():
        if src_name in macros:
            put(dst, macros[src_name], sources[src_name])
    if 'numFourGames' in macros:
        put('numMirrorGames', group(int(macros['numFourGames'])),
            'research/v05/results/E2-pathology.txt')


# ------------------------------------------------------------- match JSONL
def load_jsonl(path):
    txt = read(path)
    if not txt:
        return []
    out = []
    for chunk in txt.split('\n'):
        chunk = chunk.strip()
        if not chunk:
            continue
        try:
            out.append(json.loads(chunk))
        except json.JSONDecodeError:
            pass
    return out


# The match harness writes the A-side win rate as "winRateA".
def winrate(r):
    for k in ('winRateA', 'winRate'):
        if k in r:
            return 100.0 * r[k]
    return None


# A LaTeX control sequence may not contain a digit, so every opponent name that
# reaches a macro name is spelled out.  Anything not in the map is title-cased,
# which is safe for the alphabetic style names.
STYLE_TAG = {'v05': 'Vfive', 'v04': 'Vfour', 'v03': 'Vthree', 'v02': 'Vtwo'}


def style_tag(name):
    return STYLE_TAG.get(name, name.capitalize())


# How a style name is spelled when it reaches the reader rather than a macro
# name.  "v02" is a legal fragment of a control sequence; "v0.2" is not.
STYLE_NAME = {'v05': 'v0.5', 'v04': 'v0.4', 'v03': 'v0.3', 'v02': 'v0.2'}


def style_name(name):
    return STYLE_NAME.get(name, name)


def signed(x):
    """A signed difference, with a plain zero rather than a signed one."""
    return '0.00' if abs(x) < 5e-3 else '%+.2f' % x


# The multi-seed head-to-head.  Distinct from the single-seed v0.4 cell of the
# per-style panel below, and given its own macro names so neither overwrites the
# other.
rows = [r for r in load_jsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
        if winrate(r) is not None]
if rows:
    wrs = [winrate(r) for r in rows]
    n = sum(r.get('games', 0) for r in rows)
    d = sum(r.get('deals', 0) for r in rows)
    put('numHeadWin', '%.2f' % (sum(wrs) / len(wrs)),
        'research/v05/results/E3-headtohead.jsonl')
    put('numHeadRange', '%.2f--%.2f' % (min(wrs), max(wrs)),
        'research/v05/results/E3-headtohead.jsonl')
    put('numHeadSeeds', str(len(rows)),
        'research/v05/results/E3-headtohead.jsonl')
    put('numHeadDeals', group(d),
        'research/v05/results/E3-headtohead.jsonl')
    put('numHeadGames', group(n),
        'research/v05/results/E3-headtohead.jsonl')
    put('numHeadBankDeals', group(rows[0].get('deals', 0)),
        'research/v05/results/E3-headtohead.jsonl')
    put('numHeadBankGames', group(rows[0].get('games', 0)),
        'research/v05/results/E3-headtohead.jsonl')

# -------------------------------------------------- per-style worst case
rows = load_jsonl(os.path.join(RES, 'E4-perstyle.jsonl'))
prof = {'v05': {}, 'v04': {}}
cell = {}
for r in rows:
    if winrate(r) is None or 'a' not in r or 'b' not in r:
        continue
    a = r['a'].split(':')[0]
    if a in prof:
        prof[a][r['b'].split(':')[0]] = winrate(r)
        cell = r
if cell:
    put('numStyleDeals', group(cell.get('deals', 0)),
        'research/v05/results/E4-perstyle.jsonl')
    put('numStyleGames', group(cell.get('games', 0)),
        'research/v05/results/E4-perstyle.jsonl')
# The cluster-bootstrap interval on the mirror-strength cell, quoted where the
# manuscript says the head-to-head difference is level.
for r in rows:
    if r.get('a', '').split(':')[0] == 'v05' and r.get('b') == 'v04' and 'ci' in r:
        put('numStyleCIVfour', '%.2f--%.2f' % (100 * r['ci'][0], 100 * r['ci'][1]),
            'research/v05/results/E4-perstyle.jsonl')
for arm, tag in (('v05', 'Vfive'), ('v04', 'Vfour')):
    p = prof[arm]
    if not p:
        continue
    put('num%sWorst' % tag, '%.2f' % min(p.values()),
        'research/v05/results/E4-perstyle.jsonl')
    put('num%sWorstOpp' % tag, style_name(min(p, key=p.get)),
        'research/v05/results/E4-perstyle.jsonl')
    put('num%sMean' % tag, '%.2f' % (sum(p.values()) / len(p)),
        'research/v05/results/E4-perstyle.jsonl')
    put('num%sStyleN' % tag, str(len(p)),
        'research/v05/results/E4-perstyle.jsonl')
    for opp, wr in p.items():
        # "Panel" prefix, not "Vs": \numVfourVsVthree is already the v0.4
        # study's PUBLISHED head-to-head figure, quoted in sec:corrections, and
        # must not be overwritten by this panel's own measurement of the same
        # pairing.
        put('numPanel%s%s' % (tag, style_tag(opp)), '%.2f' % wr,
            'research/v05/results/E4-perstyle.jsonl')
if prof['v05'] and prof['v04'] and len(prof['v05']) == len(prof['v04']):
    ma = sum(prof['v05'].values()) / len(prof['v05'])
    mb = sum(prof['v04'].values()) / len(prof['v04'])
    put('numStyleMeanGap', '%.2f' % abs(ma - mb),
        'research/v05/results/E4-perstyle.jsonl (derived)')
# Per-style difference, v0.5 minus v0.4, signed.
if prof['v05'] and prof['v04']:
    common = set(prof['v05']) & set(prof['v04'])
    for s in common:
        put('numDelta%s' % style_tag(s),
            signed(prof['v05'][s] - prof['v04'][s]),
            'research/v05/results/E4-perstyle.jsonl (derived)')
    # Minimax regret over the style set: for each style, the shortfall of this
    # arm against the best arm we measured on that style.
    if common:
        for arm, tag in (('v05', 'Vfive'), ('v04', 'Vfour')):
            reg = {s: max(prof['v05'][s], prof['v04'][s]) - prof[arm][s]
                   for s in common}
            worst = max(reg, key=reg.get)
            put('num%sRegret' % tag, '%.2f' % reg[worst],
                'research/v05/results/E4-perstyle.jsonl (derived)')
            put('num%sRegretOpp' % tag, style_name(worst),
                'research/v05/results/E4-perstyle.jsonl (derived)')

# -------------------------------------------------------------- ablations
rows = load_jsonl(os.path.join(RES, 'E5-ablations.jsonl'))
spec = None
ABL = {
    'v05:m1=0,m2=0,stage2=1': 'numAblControl',
    'v05:m1=1,m2=0,stage2=1': 'numAblMone',
    'v05:m1=0,m2=1,stage2=1': 'numAblMtwo',
    'v05:m1=0,m2=0,stage2=0': 'numAblMeight',
    'v05:m1=1,m2=1,stage2=1': 'numAblMoneMtwo',
    'v05:m1=1,m2=0,stage2=0': 'numAblMoneMeight',
    'v05:m1=0,m2=1,stage2=0': 'numAblMtwoMeight',
    'v05': 'numAblFull',
    'v05:m1p=1': 'numAblOwnershipP',
    'v05:norepeat=1': 'numAblRepeatGuard',
}
for r in rows:
    if 'spec' in r:
        spec = r['spec']
    elif winrate(r) is not None and spec in ABL:
        put(ABL[spec], '%.2f' % winrate(r),
            'research/v05/results/E5-ablations.jsonl')
        spec = None

# --------------------------------------------------------- forced endgame
txt = read(os.path.join(RES, 'E8-forced-endgame.txt'))
if txt:
    cur = None
    forced = {}
    for line in txt.splitlines():
        if line.startswith('###'):
            cur = 'Vfive' if 'v0.5' in line else 'Vfour'
        m = re.search(r'forced decls\s+([\d.]+)/game at ([\d.]+)%', line)
        if m and cur:
            forced[cur] = (float(m.group(1)), float(m.group(2)))
            # Rates and accuracies are quoted in prose, so they are rounded
            # here rather than carrying the harness's full print precision.
            put('num%sForcedRate' % cur, '%.4f' % float(m.group(1)),
                'research/v05/results/E8-forced-endgame.txt')
            put('num%sForcedAcc' % cur, '%.2f' % float(m.group(2)),
                'research/v05/results/E8-forced-endgame.txt')
    if 'Vfive' in forced and 'Vfour' in forced and forced['Vfive'][0] > 0:
        put('numForcedRateRatio', '%.1f' % (forced['Vfour'][0] / forced['Vfive'][0]),
            'research/v05/results/E8-forced-endgame.txt (derived)')

# ------------------------------------------------------- deception panel
# research/v05/results/E10-deception.md carries a two-seed table whose columns
# are v0.5 and v0.4 at each seed.  The mean delta is recomputed here from the
# four cells rather than read off the artifact's own summary column.
txt = read(os.path.join(RES, 'E10-deception.md'))
if txt:
    m = re.search(r'(\d+) deals x (\d+) rotations per cell \(n = ([\d,]+) games\)', txt)
    if m:
        put('numDecDeals', group(int(m.group(1))),
            'research/v05/results/E10-deception.md')
        put('numDecGames', m.group(3).replace(',', '{,}'),
            'research/v05/results/E10-deception.md')
    seeds = re.findall(r'v0\.5 \(seed (\d+)\)', txt)
    if seeds:
        put('numDecSeeds', str(len(seeds)), 'research/v05/results/E10-deception.md')
    decfive = {}
    for line in txt.splitlines():
        m = re.match(r'\|\s*\*{0,2}(silent|feint|withholder)\*{0,2}\s*\|(.*)\|\s*$',
                     line.strip())
        if not m:
            continue
        style = m.group(1)
        vals = [float(x) for x in re.findall(r'(-?[\d.]+)%', m.group(2))]
        if len(vals) < 4:
            continue
        five = vals[0::2]
        four = vals[1::2]
        tag = style.capitalize()
        decfive[style] = five
        put('numDec%sFive' % tag, '%.2f' % (sum(five) / len(five)),
            'research/v05/results/E10-deception.md')
        put('numDec%sFour' % tag, '%.2f' % (sum(four) / len(four)),
            'research/v05/results/E10-deception.md')
        put('numDec%sDelta' % tag,
            '%+.1f' % (sum(five) / len(five) - sum(four) / len(four)),
            'research/v05/results/E10-deception.md (derived)')
        put('numDec%sWorst' % tag, '%.2f' % min(five),
            'research/v05/results/E10-deception.md (derived)')
    if decfive:
        allcells = [(v, s) for s, vs in decfive.items() for v in vs]
        lo = min(allcells)
        put('numDecWorst', '%.2f' % lo[0],
            'research/v05/results/E10-deception.md (derived)')
        put('numDecWorstOpp', lo[1],
            'research/v05/results/E10-deception.md (derived)')
    m = re.search(r'A sweep over\s*\n?\{([^}]*)\}', txt)
    if m:
        put('numThetaSweep', m.group(1).strip(),
            'research/v05/results/E10-deception.md')
        put('numThetaSweepN', str(len(m.group(1).split(','))),
            'research/v05/results/E10-deception.md')

# ------------------------------------------------- the policy prior weight
# priorTheta is the tenth knob after the twenty ask features; the offset is the
# one engine/freeze_config_v05.py bakes in, and is derived here the same way.
txt = read(os.path.join(RUNS, 'v05-fitted.txt'))
if txt:
    vec = [float(x) for x in txt.strip().split('|')]
    if len(vec) >= 31:
        put('numPriorThetaFive', '%.3f' % vec[20 + 9],
            'research/v05/runs/v05-fitted.txt')
        put('numPriorPhiFive', '%.3f' % vec[20 + 10],
            'research/v05/runs/v05-fitted.txt')
txt = read(os.path.join(HERE, 'src', 'v04.hpp'))
if txt:
    m = re.search(r'priorTheta\s*=\s*([\d.]+)', txt)
    if m:
        put('numPriorThetaFour', '%.3f' % float(m.group(1)), 'engine/src/v04.hpp')

# --------------------------------------------- the unused target dimension
txt = read(os.path.join(RES, 'P5-human-strategy.md'))
if txt:
    m = re.search(r'~(\d+) bits of\s*\n?\s*unused private channel per team per game',
                  txt)
    if m:
        put('numTargetBitsGame', m.group(1),
            'research/v05/results/P5-human-strategy.md')

# ------------------------------------------------------ fitting rounds run
v05rounds = [f for f in os.listdir(RUNS)
             if re.match(r'fit-round\d+\.jsonl$', f)] if os.path.isdir(RUNS) else []
if v05rounds:
    put('numFitRounds', str(len(v05rounds)), 'research/v05/runs/')
V04RUNS = os.path.join(ROOT, 'research', 'v04', 'runs')
if os.path.isdir(V04RUNS):
    n = len(set(re.match(r'tune-round(\d+)', f).group(1)
                for f in os.listdir(V04RUNS)
                if re.match(r'tune-round(\d+).*\.jsonl$', f)))
    if n:
        put('numVfourFitRounds', str(n), 'research/v04/runs/')

# ------------------------------------------- seeds and sizes of the battery
# The seeds are properties of the run, and the run is the committed script.
txt = read(os.path.join(HERE, 'experiments_v05.sh'))
if txt:
    m = re.search(r'E4-perstyle.*?--seed=(\d+)', txt, re.S)
    if m:
        put('numStyleSeed', m.group(1), 'engine/experiments_v05.sh')
    m = re.search(r'E8.*?--games=(\d+) --rotations=(\d+)', txt, re.S)
    if m:
        put('numForcedEightDeals', group(int(m.group(1))),
            'engine/experiments_v05.sh')
        put('numForcedEightGames', group(int(m.group(1)) * int(m.group(2))),
            'engine/experiments_v05.sh')

# ----------------------------------------------------------- verification
txt = read(os.path.join(RES, 'E1-verify.txt'))
if txt:
    m = re.search(r'audit violations:\s*(\d+)\s*/\s*(\d+) checks', txt)
    if m:
        put('numVerifyViolations', m.group(1), 'research/v05/results/E1-verify.txt')
        put('numVerifyChecks', group(int(m.group(2))),
            'research/v05/results/E1-verify.txt')

# ----------------------------------------------------------------- the fit
import math

FIT = 'research/v05/runs/fit-round1.jsonl'
rows = load_jsonl(os.path.join(RUNS, 'fit-round1.jsonl'))
gens = [r for r in rows if 'gen' in r]
finals = {r['final']: r['score'] for r in rows if 'final' in r and 'score' in r}

# The command of record for the fit, read first because the temperature and the
# panel it names are needed to interpret the trace.  Neither is recorded inside
# fit-round1.jsonl, so both have to come from the command, and the generation
# count the command names is cross-checked against the trace rather than trusted.
# v0.4's shipping round had no committed script carrying any of this -- a
# reproducibility gap the v0.4 paper states -- so v0.5's lives in
# docs/FISHBOT_V05.md section 9.
DOC9 = 'docs/FISHBOT_V05.md section 9'
doc = read(os.path.join(ROOT, 'docs', 'FISHBOT_V05.md'))
m = re.search(r'\./fish tune --base=v05(.*?)```', doc, re.S) if doc else None
cmd = m.group(1) if m else ''
# beta: the tuner's compiled default unless the command overrides it, which is
# also how v0.4's beta is known (its command recorded no --beta).
tun = read(os.path.join(HERE, 'src', 'tuner.hpp'))
m = re.search(r'double beta = ([\d.]+)', tun) if tun else None
default_beta = float(m.group(1)) if m else 10.0
put('numSoftMinBeta', '%g' % default_beta, 'engine/src/tuner.hpp (compiled default)')
m = re.search(r'--beta=([\d.]+)', cmd)
fit_beta = float(m.group(1)) if m else default_beta
put('numFitBeta', '%g' % fit_beta, DOC9)
m = re.search(r'--panel=([\w,:=.]+)', cmd)
fit_panel = m.group(1).split(',') if m else []
if fit_panel:
    pretty = {'v04': 'v0.4', 'v03': 'v0.3', 'v02': 'v0.2'}
    put('numFitPanel', ', '.join(pretty.get(p, p) for p in fit_panel), DOC9)

if gens:
    # "Best generation" means the best per-generation score the trace records.
    # It is a maximum over a population evaluated on shared seeds, so it is
    # biased upwards; the winner's-curse guard below is what settles the vector.
    best = max(gens, key=lambda r: r['bestScore'])
    put('numFitGens', str(len(gens)), FIT)
    put('numFitScore', '%.4f' % best['bestScore'], FIT)
    put('numFitBestGen', str(best['gen']), FIT)
    put('numFitWorst', '%.2f' % (100 * min(best['winRates'])), FIT)
    put('numFitPanelN', str(len(best['winRates'])), FIT)
    if fit_panel and len(fit_panel) != len(best['winRates']):
        print('  WARNING: %s names %d panel members but %s scores %d'
              % (DOC9, len(fit_panel), FIT, len(best['winRates'])))
    wr = best['winRates']
    put('numFitProfile', ' / '.join('%.1f' % (100 * x) for x in wr), FIT)
    # The point of putting the mirror in the panel, and of the raised beta: a
    # soft minimum is only a minimum if the gradient weights on the panel
    # separate.  The weight on opponent o is proportional to exp(-beta * wr_o),
    # so the max/min ratio over the panel is exactly exp(beta * span).
    span = max(wr) - min(wr)
    ws = [math.exp(-fit_beta * x) for x in wr]
    put('numFitSpan', '%.3f' % span, FIT + ' (derived)')
    put('numFitRatio', '%.1f' % (max(ws) / min(ws)), FIT + ' (derived)')
    # v0.4's temperature over v0.5's panel spread, and v0.5's temperature over
    # v0.4's: the two counterfactuals the fitting section quotes.
    put('numFitRatioAtTen', '%.2f' % math.exp(default_beta * span),
        FIT + ' (derived)')
    # The mirror image: v0.5's temperature over v0.4's panel spread.  Emitted
    # after the v0.4 block below has settled numSoftMinSpan, so it is deferred.
    fit_span = span
if finals:
    # The winner's-curse guard.  tune() re-evaluates the distribution mean and
    # the best single generation at one common held-out seed and returns
    # whichever scores higher (engine/src/tuner.hpp).
    if 'mu' in finals:
        put('numFitGuardMu', '%.4f' % finals['mu'], FIT)
    if 'best' in finals:
        put('numFitGuardBest', '%.4f' % finals['best'], FIT)
    # How much the best generation's own in-generation score overstated the same
    # vector when it was re-scored on a bank it had not been selected on.
    if 'best' in finals and gens:
        put('numFitCurse',
            '%.4f' % (max(g['bestScore'] for g in gens) - finals['best']),
            FIT + ' (derived)')
    if 'mu' in finals and 'best' in finals:
        put('numFitGuardWinner', 'mean' if finals['mu'] >= finals['best'] else 'best',
            FIT + ' (derived)')
        put('numFitGuardGap', '%.4f' % abs(finals['mu'] - finals['best']),
            FIT + ' (derived)')

# The rest of the schedule, from the same command of record.
if cmd:
    for flag, name, fmt in (('seed', 'numFitSeed', str),
                            ('games', 'numFitDeals', group),
                            ('pop', 'numFitPop', str),
                            ('elite', 'numFitElite', str)):
        mm = re.search(r'--%s=(\d+)' % flag, cmd)
        if mm:
            put(name, fmt(int(mm.group(1))), DOC9)
    # A tuner cell is one candidate against one panel member: --games counts
    # deals, and MatchStats plays each deal in both team orientations.
    if 'numFitDeals' in macros:
        deals = int(macros['numFitDeals'].replace('{,}', ''))
        put('numFitCellGames', group(2 * deals), DOC9)
        if gens and 'numFitPop' in macros and 'numFitPanelN' in macros:
            put('numFitTotalGames',
                group(2 * deals * int(macros['numFitPop'])
                      * int(macros['numFitPanelN']) * len(gens)),
                DOC9 + ' (derived)')
    mm = re.search(r'--gens=(\d+)', cmd)
    if mm and gens and int(mm.group(1)) != len(gens):
        print('  WARNING: docs/FISHBOT_V05.md records --gens=%s but %s has %d '
              'generations' % (mm.group(1), FIT, len(gens)))

# ------------------------------------- v0.4's objective, for the comparison
# The soft minimum's gradient weight on opponent o is proportional to
# exp(-beta * wr_o), so the max/min ratio over a panel is exactly
# exp(beta * span).  v0.4's selected generation is the profile to read it on.
txt = read(os.path.join(ROOT, 'research', 'v04', 'runs', 'selected.json'))
if txt:
    try:
        sel = json.loads(txt)
    except json.JSONDecodeError:
        sel = None
    if sel and sel.get('winRates'):
        wr4 = sel['winRates']
        span4 = max(wr4) - min(wr4)
        put('numSoftMinSpan', '%.3f' % span4, 'research/v04/runs/selected.json (derived)')
        put('numSoftMinPanelN', str(len(wr4)), 'research/v04/runs/selected.json')
        # Quoted at one decimal so it agrees with the diagnosis section, which
        # takes the same figure from the corrections register.
        put('numSoftMinRatio', '%.1f' % math.exp(default_beta * span4),
            'research/v04/runs/selected.json (derived, beta = %g)' % default_beta)
        # v0.5's temperature applied to v0.4's panel spread: the counterfactual
        # that shows the temperature alone does not make the objective a minimum.
        put('numFitRatioCross', '%.2f' % math.exp(fit_beta * span4),
            'research/v04/runs/selected.json (derived, beta = %g)' % fit_beta)

# The in-panel-to-held-out regression: the best generation's own mirror cell
# against the multi-seed held-out mean of the frozen configuration.
if 'numFitWorst' in macros and 'numHeadWin' in macros:
    put('numFitRegression',
        '%.2f' % (float(macros['numFitWorst']) - float(macros['numHeadWin'])),
        'research/v05/runs/fit-round1.jsonl + '
        'research/v05/results/E3-headtohead.jsonl (derived)')

# ------------------------------------------- the freeze/parse round trip
# The flat vector is parsed at an offset DERIVED from NFEAT in both
# engine/src/factory.hpp and engine/freeze_config_v05.py.  v0.4 hard-coded 18;
# when NFEAT grew to 20 two ask weights were aliased onto the first two decision
# knobs, so the vector the optimiser scored was not the vector that shipped.
# research/v05/runs/roundtrip-assert.txt is the mechanical check, with a
# negative control that re-encodes the same vector at the v0.4 offset.
txt = read(os.path.join(RUNS, 'roundtrip-assert.txt'))
if txt:
    RT = 'research/v05/runs/roundtrip-assert.txt'
    for key, name in (('NFEAT', 'numFitAskFeats'), ('KNOBS', 'numFitKnobs'),
                      ('DIMS', 'numFitDims'), ('ALIASED_OFFSET', 'numAliasOffset')):
        m = re.search(r'^%s (\d+)$' % key, txt, re.M)
        if m:
            put(name, m.group(1), RT)
    section, runs = None, {}
    for line in txt.splitlines():
        if line.startswith('## '):
            # A heading may run over several comment lines; only a line that
            # names a section switches, so continuations do not clear it.
            low = line.lower()
            for key, tag in (('roundtrip', 'roundtrip'),
                             ('negative control', 'alias'),
                             ('quantisation', 'quant')):
                if key in low:
                    section = tag
                    break
        elif line.startswith('{') and section:
            try:
                r = json.loads(line)
            except json.JSONDecodeError:
                continue
            runs.setdefault(section, []).append(r)
    if runs.get('roundtrip'):
        rt = runs['roundtrip']
        put('numRoundTrip', '%.4f' % (100 * max(r['winRateA'] for r in rt)), RT)
        put('numRoundTripSeeds', str(len(rt)), RT)
        put('numRoundTripGames', group(sum(r['games'] for r in rt)), RT)
        put('numRoundTripPass',
            'passes' if all(r['winRateA'] == 0.5 for r in rt) else 'FAILS',
            RT + ' (derived)')
    for section, prefix in (('alias', 'numAlias'), ('quant', 'numQuant')):
        for r in runs.get(section, [])[:1]:
            # The artifact plays the shipped configuration as arm A, so the
            # counterfactual arm's win rate is the complement.
            put(prefix + 'Win', '%.2f' % (100 * (1 - r['winRateA'])), RT)
            put(prefix + 'Loss', '%.2f' % (100 * (r['winRateA'] - 0.5)),
                RT + ' (derived)')
            put(prefix + 'CI', '%.2f--%.2f' % (100 * (1 - r['ci'][1]),
                                               100 * (1 - r['ci'][0])), RT)
            put(prefix + 'Games', group(r['games']), RT)

# The frozen vector is the one freeze_config_v05.py baked in, which is the
# distribution mean the guard preferred -- NOT the mean at the best-scoring
# generation.  Read it from the frozen artifact so the two cannot diverge.
txt = read(os.path.join(RUNS, 'v05-fitted.txt'))
if txt:
    put('numFitVector', txt.strip(), 'research/v05/runs/v05-fitted.txt')

# ------- the pre-refit figure, and what the refit was actually worth
# research/v05/runs/v04vector-in-v05.txt runs the built mechanisms on v0.4's
# frozen 34-coordinate vector.  It carries two different configurations and they
# must not be pooled:
#
#   "design-time"  -- the assembly as it stood when sec:mechanisms' table was
#                     taken, with the ownership-by-p scaling and the repetition
#                     guard ON.  Both were later measured, rejected and ship
#                     OFF.  This is the source of the pre-refit figure that
#                     sec:mechanisms and sec:fitting quote.
#   "shipped"      -- the assembly that actually ships, same vector.  Its large
#                     banks and its mirror pathology row are what separate the
#                     mechanical repair from the parametric one.
txt = read(os.path.join(RUNS, 'v04vector-in-v05.txt'))
if txt:
    VP = 'research/v05/runs/v04vector-in-v05.txt'
    section, runs, patho = None, {}, []
    for line in txt.splitlines():
        if line.startswith('## '):
            low = line.lower()
            if './fish' in low:      # the reproduction commands, not a section
                section = None
                continue
            for key, tag in (('design-time', 'design'),
                             ('same bank', 'sameBank'),
                             ('two larger banks', 'shipped'),
                             ('pathology', 'patho')):
                if key in low:
                    section = tag
                    break
        elif line.startswith('{') and section:
            try:
                runs.setdefault(section, []).append(json.loads(line))
            except json.JSONDecodeError:
                pass
        elif section == 'patho':
            patho.append(line)

    def one(tag, key):
        rs = runs.get(tag) or []
        return rs[0][key] if rs else None

    # (1) The pre-refit figure, at the bank and configuration it came from.
    if runs.get('design'):
        r = runs['design'][0]
        put('numVfourParamWin', '%.1f' % (100 * r['winRateA']), VP)
        put('numVfourParamCI', '%.2f--%.2f' % (100 * r['ci'][0], 100 * r['ci'][1]), VP)
        put('numVfourParamDecl', '%.2f' % r['declPerGameA'], VP)
        put('numVfourDecl', '%.2f' % r['declPerGameB'], VP)
    # The shipped assembly on the SAME bank, which is what isolates how much of
    # the pre-refit shortfall belonged to the two rejected mechanisms.
    if runs.get('sameBank'):
        r = runs['sameBank'][0]
        put('numShipParamSameBank', '%.2f' % (100 * r['winRateA']), VP)
        put('numShipParamSameCI',
            '%.2f--%.2f' % (100 * r['ci'][0], 100 * r['ci'][1]), VP)
        if runs.get('design'):
            put('numRejectedCost',
                '%.1f' % (100 * (r['winRateA'] - runs['design'][0]['winRateA'])),
                VP + ' (derived)')

    # (2) What the refit was worth: the shipped assembly on larger banks.
    rs = runs.get('shipped') or []
    if rs:
        wrs = [100 * r['winRateA'] for r in rs]
        put('numShipParamWin', '%.2f' % (sum(wrs) / len(wrs)), VP + ' (derived)')
        put('numShipParamRange', '%.2f--%.2f' % (min(wrs), max(wrs)), VP)
        put('numShipParamSeeds', str(len(rs)), VP)
        put('numShipParamGames', group(sum(r['games'] for r in rs)), VP)
        # The union of the deal-clustered intervals, so the figure quoted in
        # prose is never narrower than either bank supports.
        put('numShipParamCI',
            '%.2f--%.2f' % (100 * min(r['ci'][0] for r in rs),
                            100 * max(r['ci'][1] for r in rs)),
            VP + ' (derived)')
        for key, name in (('declPerGameA', 'numShipParamDecl'),
                          ('declPerGameB', 'numShipParamVfourDecl')):
            put(name, '%.2f' % (sum(r[key] for r in rs) / len(rs)),
                VP + ' (derived)')
    # (3) The mirror pathology row of the shipped assembly, on E2's instrument.
    block = '\n'.join(patho)
    for pat, name in (
            (r'DEAD asks\s+\d+\s+\(([\d.]+)%', 'numShipParamDeadAsk'),
            (r'dead runs\s+\d+\s+mean length [\d.]+\s+longest (\d+)',
             'numShipParamDeadRun'),
            (r'games w/ run>=6\s+\d+\s+\(([\d.]+)%', 'numShipParamDeadRunGames'),
            (r'repeat \(a,c,t\)\s+\d+\s+\(([\d.]+)%', 'numShipParamRepeat'),
            (r'declarations\s+\d+\s+wrong \d+ \(([\d.]+)%',
             'numShipParamDeclWrong'),
            (r'at/after ev>=220\s+(\d+)\s+wrong', 'numShipParamPostHorizon'),
            (r'forced endgame\s+(\d+)\s+wrong', 'numShipParamForcedN')):
        m = re.search(pat, block)
        if m:
            put(name, sig(m.group(1)), VP)

# ================= results-section coverage: E1, E3, E5-E10 ================
# Everything below is consumed by paper/sections_v05/10-results.tex.  Each block
# reads one artifact of the battery in engine/experiments_v05.sh.

SH = read(os.path.join(HERE, 'experiments_v05.sh')) or ''


def sh_seed(marker):
    """The seed used by the numbered experiment whose banner contains MARKER."""
    m = re.search(re.escape(marker) + r'.*?--seed=(\d+)', SH, re.S)
    return m.group(1) if m else None


# ------------------------------------ E2, the seeds the pathology bank used
m = re.search(r'pathology --a=v05 --b=v05 --games=(\d+) --rotations=(\d+) --seed=(\d+)', SH)
if m:
    put('numPathologyDeals', group(int(m.group(1))), 'engine/experiments_v05.sh')
    put('numPathologySeed', m.group(3), 'engine/experiments_v05.sh')
m = re.search(r'pathology --a=v05 --b=v04 .*?--seed=(\d+)', SH, re.S)
if m:
    put('numPathologyCrossSeed', m.group(1), 'engine/experiments_v05.sh')


# --------- the policy pool `fish verify` actually sweeps (it excludes v0.5)
txt = read(os.path.join(HERE, 'src', 'main.cpp'))
if txt:
    m = re.search(r'cmd == "verify".*?int np = (\d+);', txt, re.S)
    if m:
        put('numVerifyPool', m.group(1), 'engine/src/main.cpp')
        put('numVerifyPairs', str(int(m.group(1)) ** 2), 'engine/src/main.cpp (derived)')


# ------- the action-limit rate across every match row of the whole battery
lim_rows, lim_bad = 0, 0
for f in ('E3-headtohead.jsonl', 'E4-perstyle.jsonl', 'E5-ablations.jsonl',
          'E7-rules.jsonl'):
    for r in load_jsonl(os.path.join(RES, f)):
        if 'limitHitRate' in r:
            lim_rows += 1
            if r['limitHitRate'] != 0:
                lim_bad += 1
if lim_rows:
    put('numLimitRows', str(lim_rows), 'research/v05/results/E3,E4,E5,E7 (derived)')
    put('numLimitNonzero', str(lim_bad), 'research/v05/results/E3,E4,E5,E7 (derived)')


# ----------------------------------------------------- E1, the whole report
txt = read(os.path.join(RES, 'E1-verify.txt'))
if txt:
    E1 = 'research/v05/results/E1-verify.txt'
    m = re.search(r'set-conservation failures:\s*(\d+)', txt)
    if m:
        put('numVerifySetFail', m.group(1), E1)
    m = re.search(r'action-limit games:\s*(\d+)', txt)
    if m:
        put('numVerifyLimitGames', m.group(1), E1)
    m = re.search(r'determinism:\s*(\w+)', txt)
    if m:
        put('numVerifyDeterminism', m.group(1), E1)
    m = re.search(r'\./fish verify --games=(\d+)', SH)
    if m:
        put('numVerifyGames', group(int(m.group(1))), 'engine/experiments_v05.sh')

# ------------------------------------- E3, seed by seed and its aggregates
rows = [r for r in load_jsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
        if winrate(r) is not None]
if rows:
    E3 = 'research/v05/results/E3-headtohead.jsonl'
    ORD = ['One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight']
    m = re.search(r'for S in ([\d ]+); do', SH)
    seeds = m.group(1).split() if m else []
    for i, r in enumerate(rows):
        if i >= len(ORD):
            break
        put('numHeadWin' + ORD[i], '%.2f' % winrate(r), E3)
        if 'ci' in r:
            put('numHeadCI' + ORD[i],
                '%.2f--%.2f' % (100 * r['ci'][0], 100 * r['ci'][1]), E3)
        if i < len(seeds):
            put('numHeadSeed' + ORD[i], seeds[i], 'engine/experiments_v05.sh')

    def avg(key):
        vals = [r[key] for r in rows if key in r]
        return sum(vals) / len(vals) if vals else None

    for key, name, scale, fmt in (('declAccA', 'numHeadDeclAccFive', 100, '%.2f'),
                                  ('declAccB', 'numHeadDeclAccFour', 100, '%.2f'),
                                  ('declPerGameA', 'numHeadDeclRateFive', 1, '%.2f'),
                                  ('declPerGameB', 'numHeadDeclRateFour', 1, '%.2f'),
                                  ('meanSetsA', 'numHeadSetsFive', 1, '%.2f'),
                                  ('meanSetsB', 'numHeadSetsFour', 1, '%.2f'),
                                  ('eventsPerGame', 'numHeadEvents', 1, '%.1f')):
        v = avg(key)
        if v is not None:
            put(name, fmt % (scale * v), E3 + ' (derived)')

# ------------------------------------ E5, the population and the per-arm detail
rows = load_jsonl(os.path.join(RES, 'E5-ablations.jsonl'))
spec, arms = None, {}
for r in rows:
    if 'spec' in r:
        spec = r['spec']
    elif winrate(r) is not None and spec is not None:
        arms[spec] = r
        spec = None
if arms:
    E5 = 'research/v05/results/E5-ablations.jsonl'
    any_row = next(iter(arms.values()))
    put('numAblFiveDeals', group(any_row.get('deals', 0)), E5)
    put('numAblFiveGames', group(any_row.get('games', 0)), E5)
    s = sh_seed('E5 mechanism ablations')
    if s:
        put('numAblFiveSeed', s, 'engine/experiments_v05.sh')
    for key, tag in (('v05:m1=0,m2=0,stage2=1', 'Control'),
                     ('v05:m1=1,m2=0,stage2=1', 'Mone'),
                     ('v05:m1=0,m2=1,stage2=1', 'Mtwo'),
                     ('v05:m1=0,m2=0,stage2=0', 'Meight'),
                     ('v05:m1=1,m2=1,stage2=1', 'MoneMtwo'),
                     ('v05:m1=1,m2=0,stage2=0', 'MoneMeight'),
                     ('v05:m1=0,m2=1,stage2=0', 'MtwoMeight'),
                     ('v05', 'Full'),
                     ('v05:m1p=1', 'OwnershipP'),
                     ('v05:norepeat=1', 'RepeatGuard')):
        r = arms.get(key)
        if not r:
            continue
        if 'ci' in r:
            put('numAbl%sCI' % tag,
                '%.2f--%.2f' % (100 * r['ci'][0], 100 * r['ci'][1]), E5)
        if 'eventsPerGame' in r:
            put('numAbl%sEvents' % tag, '%.1f' % r['eventsPerGame'], E5)
        if 'declAccA' in r:
            put('numAbl%sDeclAcc' % tag, '%.2f' % (100 * r['declAccA']), E5)
        if 'declAccB' in r:
            put('numAbl%sOppDeclAcc' % tag, '%.2f' % (100 * r['declAccB']), E5)
        if 'askAccA' in r:
            put('numAbl%sHit' % tag, '%.2f' % (100 * r['askAccA']), E5)
    # The two mechanisms that were designed and rejected are measured on top of
    # the shipped configuration, so their cost is the difference from it.
    full = arms.get('v05')
    if full:
        for key, tag in (('v05:m1p=1', 'OwnershipP'),
                         ('v05:norepeat=1', 'RepeatGuard')):
            r = arms.get(key)
            if r:
                put('numAbl%sDelta' % tag,
                    signed(winrate(r) - winrate(full)), E5 + ' (derived)')
    # Throughput is a by-product: the pathological arms play longer games.
    ctl, full = arms.get('v05:m1=0,m2=0,stage2=1'), arms.get('v05')
    if ctl and full and ctl.get('seconds') and full.get('seconds'):
        a = ctl['games'] / ctl['seconds']
        b = full['games'] / full['seconds']
        put('numThroughputPatho', '%.0f' % a, E5 + ' (derived)')
        put('numThroughputFixed', '%.0f' % b, E5 + ' (derived)')
        put('numThroughputRatioFive', '%.1f' % (b / a), E5 + ' (derived)')

# -------------------------------------------- E6, calibration of both forecasts
txt = read(os.path.join(RES, 'E6-calibration-v05.txt'))
if txt:
    E6 = 'research/v05/results/E6-calibration-v05.txt'
    m = re.search(r'\./fish calibrate.*?--games=(\d+) --seed=(\d+)', SH, re.S)
    if m:
        put('numCalibFiveDeals', group(int(m.group(1))), 'engine/experiments_v05.sh')
        put('numCalibFiveSeed', m.group(2), 'engine/experiments_v05.sh')
    cur, bins = None, {}
    for line in txt.splitlines():
        m = re.match(r'\s*(ask|decl)\s+n=(\d+) brier=([\d.]+) logloss=([\d.]+) '
                     r'ece=([\d.]+) meanPred=([\d.]+) meanObs=([\d.]+)', line)
        if m:
            cur = 'Ask' if m.group(1) == 'ask' else 'Decl'
            bins[cur] = []
            put('numCalib%sN' % cur, group(int(m.group(2))), E6)
            put('numCalib%sBrier' % cur, m.group(3), E6)
            put('numCalib%sLogloss' % cur, m.group(4), E6)
            put('numCalib%sECE' % cur, m.group(5), E6)
            put('numCalib%sPred' % cur, '%.1f' % (100 * float(m.group(6))), E6)
            put('numCalib%sObs' % cur, '%.1f' % (100 * float(m.group(7))), E6)
            continue
        m = re.match(r'\s*\[([\d.]+),([\d.]+)\)\s+n=\s*(\d+) pred=([\d.]+) obs=([\d.]+)',
                     line)
        if m and cur:
            bins[cur].append((float(m.group(1)), int(m.group(3)),
                              float(m.group(4)), float(m.group(5))))
    for cur, lo_want, tag in (('Ask', 0.0, 'Low'), ('Ask', 0.4, 'Mid'),
                              ('Decl', 0.7, 'Mid'), ('Decl', 0.9, 'Top')):
        for lo, n, pred, obs in bins.get(cur, []):
            if abs(lo - lo_want) < 1e-9:
                put('numCalib%s%sN' % (cur, tag), group(n), E6)
                put('numCalib%s%sPred' % (cur, tag), '%.1f' % (100 * pred), E6)
                put('numCalib%s%sObs' % (cur, tag), '%.1f' % (100 * obs), E6)
                total = sum(b[1] for b in bins[cur])
                if total:
                    put('numCalib%s%sShare' % (cur, tag),
                        '%.1f' % (100.0 * n / total), E6 + ' (derived)')
    # How much of each aggregate is one bin: the reason an ECE is not a summary.
    for cur in ('Ask', 'Decl'):
        b = bins.get(cur)
        if b:
            put('numCalib%sBins' % cur, str(len(b)), E6)

# ------------------------------------------------------- E7, the rule dialects
rows = load_jsonl(os.path.join(RES, 'E7-rules.jsonl'))
dial, cur = {}, None
DIALECT = {'default': 'Default', '--no-out-of-turn': 'NoOOT',
           '--no-cardless-declare': 'NoCardless', '--legacy': 'Legacy'}
for r in rows:
    if 'dialect' in r:
        cur = DIALECT.get(r['dialect'])
    elif winrate(r) is not None and cur:
        dial[cur] = r
        cur = None
if dial:
    E7 = 'research/v05/results/E7-rules.jsonl'
    any_row = next(iter(dial.values()))
    put('numDialectDeals', group(any_row.get('deals', 0)), E7)
    put('numDialectGames', group(any_row.get('games', 0)), E7)
    s = sh_seed('E7 rule dialects')
    if s:
        put('numDialectSeed', s, 'engine/experiments_v05.sh')
    for tag, r in dial.items():
        put('numDialect%s' % tag, '%.2f' % winrate(r), E7)
        if 'ci' in r:
            put('numDialect%sCI' % tag,
                '%.2f--%.2f' % (100 * r['ci'][0], 100 * r['ci'][1]), E7)
        for key, suffix, scale, fmt in (('forcedPerGameA', 'ForcedRate', 1, '%.4f'),
                                        ('forcedAccA', 'ForcedFive', 100, '%.2f'),
                                        ('forcedAccB', 'ForcedFour', 100, '%.2f'),
                                        ('declAccA', 'DeclFive', 100, '%.2f'),
                                        ('declAccB', 'DeclFour', 100, '%.2f'),
                                        ('askAccA', 'Hit', 100, '%.2f')):
            if key in r:
                put('numDialect%s%s' % (tag, suffix), fmt % (scale * r[key]), E7)
    if 'Default' in dial and 'NoOOT' in dial:
        a = dial['Default'].get('forcedPerGameA') or 0
        b = dial['NoOOT'].get('forcedPerGameA') or 0
        if a:
            put('numDialectForcedRatio', '%.0f' % (b / a), E7 + ' (derived)')

# ------------------------------ E8, the declaration totals behind the probe
txt = read(os.path.join(RES, 'E8-forced-endgame.txt'))
if txt:
    E8 = 'research/v05/results/E8-forced-endgame.txt'
    cur = None
    for line in txt.splitlines():
        if line.startswith('###'):
            cur = 'Vfive' if 'v0.5' in line else 'Vfour'
        m = re.search(r'declarations\s+([\d.]+)/game at ([\d.]+)%', line)
        if m and cur:
            put('num%sDeclRateEight' % cur, '%.2f' % float(m.group(1)), E8)
            put('num%sDeclAccEight' % cur, '%.2f' % float(m.group(2)), E8)
    s = sh_seed('E8 forced-endgame')
    if s:
        put('numForcedEightSeed', s, 'engine/experiments_v05.sh')

# ------------------------------------------------------------- E9 throughput
txt = read(os.path.join(RES, 'E9-throughput.txt'))
if txt:
    m = re.search(r'([\d.]+) games/s over (\d+) games', txt)
    if m:
        put('numThroughputFive', '%.0f' % float(m.group(1)),
            'research/v05/results/E9-throughput.txt')
        put('numThroughputGamesFive', group(int(m.group(2))),
            'research/v05/results/E9-throughput.txt')

# --------------------------------------------- E10, cell by cell and by seed
txt = read(os.path.join(RES, 'E10-deception.md'))
if txt:
    E10 = 'research/v05/results/E10-deception.md'
    seeds = re.findall(r'v0\.5 \(seed (\d+)\)', txt)
    for i, s in enumerate(seeds[:2]):
        put('numDecSeed' + ('A' if i == 0 else 'B'), s, E10)
    for line in txt.splitlines():
        m = re.match(r'\|\s*\*{0,2}(silent|feint|withholder)\*{0,2}\s*\|(.*)\|\s*$',
                     line.strip())
        if not m:
            continue
        tag = m.group(1).capitalize()
        vals = [float(x) for x in re.findall(r'(-?[\d.]+)%', m.group(2))]
        if len(vals) < 4:
            continue
        for i, name in enumerate(('FiveA', 'FourA', 'FiveB', 'FourB')):
            put('numDec%s%s' % (tag, name), '%.2f' % vals[i], E10)
    m = re.search(r'twelve-style set \((\w+) standard \+ (\w+) deceptive\)', txt)
    if m:
        put('numTwelveStyles', '12', E10)

# ---------------------------- the deceiver-only cost of deleting the prior
txt = read(os.path.join(RES, 'P3-verify-deception.md'))
if txt:
    P3V = 'research/v05/results/P3-verify-deception.md'
    m = re.search(r'Worse by \*\*([\d.]+)\*\* points \[([\d.]+), ([\d.]+)\]', txt)
    if m:
        put('numPriorDeleteCost', m.group(1), P3V)
        put('numPriorDeleteCI', '%s, %s' % (m.group(2), m.group(3)), P3V)
    # The per-cell sign census.  The same verification refuses the word
    # "uniformly" for this ablation at the resolution the per-cell banks have,
    # so the paper reports the census instead of claiming every cell.
    m = re.search(r'(\d+) \(seed .{0,40}?\) cells.{0,200}?'
                  r'\*\*(\d+) worse, (\d+) tie, (\d+) reversed', txt, re.S)
    if m:
        for i, name in enumerate(('numPriorCells', 'numPriorCellsWorse',
                                  'numPriorCellsTie', 'numPriorCellsReversed')):
            put(name, m.group(i + 1), P3V)
    m = re.search(r'Sign test on the \d+ non-ties: p . ([\d.]+)', txt)
    if m:
        put('numPriorSignP', m.group(1).rstrip('.'), P3V)

# ------- the design-time cost that put the two rejected mechanisms off by default
txt = read(os.path.join(HERE, 'src', 'v05.hpp'))
if txt:
    V05 = 'engine/src/v05.hpp'
    m = re.search(r'ownershipByP.*?', txt)
    for pat, name in ((r'distorts the calibration.*?-([\d.]+) points', 'numMonePReject'),
                      (r'provably dead|Measured at -([\d.]+) points against v0\.4 on top of M1\+M2\+M8\.\s*M1', 'numGuardReject')):
        mm = re.search(pat, txt, re.S)
        if mm and mm.lastindex:
            put(name, mm.group(1), V05)


# ================= audit pass: denominators, spans and per-bank deltas =====
# Added by the results audit.  Each of these exists because a figure in
# sec:results could otherwise be read as stronger than the bank behind it.

# ---- E8: the counts under the two forced-endgame accuracy rates.  "24.35%"
#      without its denominator is the kind of figure that travels further than
#      the evidence; the paper quotes n beside it.
txt = read(os.path.join(RES, 'E8-forced-endgame.txt'))
if txt and 'numForcedEightGames' in macros:
    E8 = 'research/v05/results/E8-forced-endgame.txt'
    ngames = int(macros['numForcedEightGames'].replace('{,}', ''))
    cur = None
    for line in txt.splitlines():
        if line.startswith('###'):
            cur = 'Vfive' if 'v0.5' in line else 'Vfour'
        m = re.search(r'forced decls\s+([\d.]+)/game at ([\d.]+)%', line)
        if m and cur:
            n = int(round(float(m.group(1)) * ngames))
            put('num%sForcedN' % cur, group(n), E8 + ' (derived)')
            put('num%sForcedRightN' % cur,
                str(int(round(n * float(m.group(2)) / 100.0))), E8 + ' (derived)')

# ---- E3: v0.4's declaration ERROR in these games, so the manuscript can put it
#      beside the mirror's error rate instead of beside its accuracy.
rows = [r for r in load_jsonl(os.path.join(RES, 'E3-headtohead.jsonl'))
        if winrate(r) is not None]
if rows:
    E3 = 'research/v05/results/E3-headtohead.jsonl'
    for key, name in (('declAccB', 'numHeadDeclErrFour'),):
        vals = [r[key] for r in rows if key in r]
        if vals:
            put(name, '%.2f' % (100 * (1 - sum(vals) / len(vals))),
                E3 + ' (derived)')

# ---- every independent v0.5-against-v0.4 bank in the report, at the frozen
#      configuration: E3's five, E4's per-style cell, E5's shipped row and E7's
#      default dialect.  The span is the honest statement of the head-to-head.
banks = [winrate(r) for r in rows]
for r in load_jsonl(os.path.join(RES, 'E4-perstyle.jsonl')):
    if r.get('a') == 'v05' and r.get('b') == 'v04' and winrate(r) is not None:
        banks.append(winrate(r))
spec = None
for r in load_jsonl(os.path.join(RES, 'E5-ablations.jsonl')):
    if 'spec' in r:
        spec = r['spec']
    elif winrate(r) is not None:
        if spec == 'v05':
            banks.append(winrate(r))
        spec = None
cur = None
for r in load_jsonl(os.path.join(RES, 'E7-rules.jsonl')):
    if 'dialect' in r:
        cur = r['dialect']
    elif winrate(r) is not None:
        if cur == 'default':
            banks.append(winrate(r))
        cur = None
if len(banks) > 1:
    SRC = 'research/v05/results/E3,E4,E5,E7 (derived)'
    put('numHeadBanks', str(len(banks)), SRC)
    put('numHeadBankSpan', '%.2f--%.2f' % (min(banks), max(banks)), SRC)
    put('numHeadBankLow', '%.2f' % min(banks), SRC)

# ---- E10: the per-bank deltas.  The mean delta alone hides that the silent
#      archetype's two banks disagree by an order of magnitude in size.
txt = read(os.path.join(RES, 'E10-deception.md'))
if txt:
    E10 = 'research/v05/results/E10-deception.md'
    for line in txt.splitlines():
        m = re.match(r'\|\s*\*{0,2}(silent|feint|withholder)\*{0,2}\s*\|(.*)\|\s*$',
                     line.strip())
        if not m:
            continue
        tag = m.group(1).capitalize()
        vals = [float(x) for x in re.findall(r'(-?[\d.]+)%', m.group(2))]
        if len(vals) < 4:
            continue
        put('numDec%sDeltaA' % tag, '%+.2f' % (vals[0] - vals[1]),
            E10 + ' (derived)')
        put('numDec%sDeltaB' % tag, '%+.2f' % (vals[2] - vals[3]),
            E10 + ' (derived)')


# ------------------------------------------------- provenance of the numbers
# The manuscript quotes two classes of figure: those this script derives from an
# experiment artifact, and those transcribed out of a diagnosis report.  The
# split is itself reported in the paper, so it is computed here rather than
# stated, and paper/check_provenance.py fails the build if any transcribed
# figure lacks a source-artifact attribution.
try:
    import glob as _glob
    used = set()
    for _f in _glob.glob(os.path.join(ROOT, 'paper', 'sections_v05', '*.tex')):
        with open(_f) as _h:
            used |= set(re.findall(r'\\(num[A-Za-z]+)', _h.read()))
    if used:
        gen_n = len(used & set(macros))
        put('numProvUsed', str(len(used)), 'derived: paper/sections_v05/*.tex')
        put('numProvGenerated', str(gen_n), 'derived: paper/sections_v05/*.tex')
        put('numProvTranscribed', str(len(used) - gen_n), 'derived: paper/sections_v05/*.tex')
except Exception:
    pass

# ---------------------------------------------------------------- emit
lines = ['% GENERATED by engine/build_tables_v05.py -- do not edit.',
         '% Every macro below is derived from an artifact under research/v05/.',
         '% paper/numbers_v05.tex supplies \\providecommand defaults for the full',
         '% macro set; this file is \\input after it and overrides what the',
         '% artifacts settle, so a stale default cannot survive into the PDF.',
         '']
for name in sorted(macros):
    lines.append('%% %s' % sources[name])
    # \providecommand first: a macro the generator introduces need not already
    # exist in the hand-written placeholder file, and \renewcommand alone would
    # then be a hard LaTeX error.
    lines.append('\\providecommand{\\%s}{}\\renewcommand{\\%s}{%s}'
                 % (name, name, macros[name]))
with open(OUT, 'w') as f:
    f.write('\n'.join(lines) + '\n')

print('wrote %s' % os.path.relpath(OUT, ROOT))
print('  %d macros generated' % len(macros))
if missing:
    print('  MISSING artifacts (macros keep their placeholder defaults):')
    for m in sorted(set(missing)):
        print('    %s' % m)
    print('  run engine/experiments_v05.sh to produce them')
sys.exit(0)
