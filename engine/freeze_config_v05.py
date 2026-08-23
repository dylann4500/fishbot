#!/usr/bin/env python3
"""Bake the fitted v0.5 parameter vector into V05Config's defaults.

The v0.4 pipeline had a defect worth not repeating: the flat vector was parsed
with a hard-coded offset, so when the ask-feature count grew from 18 to 20 two
ask weights were aliased onto the first two decision knobs and the vector the
optimiser scored was not the vector that shipped.  Here the offset is derived
from NFEAT in both the parser (engine/src/factory.hpp) and this script, and the
script prints the round-trip command that asserts they agree.

Usage:  python3 engine/freeze_config_v05.py research/v05/runs/v05-fitted.txt
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NFEAT = 20
SRC = os.path.join(HERE, 'src', 'v05.hpp')

# (config field, clamp lo, clamp hi, integer?) in flat-vector order after NFEAT.
KNOBS = [
    ('declThreshold',      0.5,  0.9999,  False),
    ('lockedAllocThresh',  0.5,  0.99999, False),
    ('askFloor',           0.0,  0.9,     False),
    ('patiencePool',       0,    45,      True),
    ('oppCardFloor',       0.0,  20.0,    False),
    ('valueWeight',        0.0,  1e9,     False),
    ('linearWeight',       0.0,  1e9,     False),
    ('minTeamProb',        0.05, 0.99,    False),
    ('declareMargin',     -1e9,  1e9,     False),
    ('priorTheta',         0.0,  2.0,     False),
    ('priorPhi',           0.0,  1.0,     False),
    ('searchTopK',         0,    24,      True),
    ('chainWeight',        0.0,  1e9,     False),
    ('threatWeight',       0.0,  1e9,     False),
]


def clamp(v, lo, hi, is_int):
    if is_int:
        return max(int(lo), min(int(hi), int(round(v))))
    return max(lo, min(hi, v))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(HERE), 'research', 'v05', 'runs', 'v05-fitted.txt')
    vec = [float(x) for x in open(path).read().strip().split('|')]
    if len(vec) < NFEAT + len(KNOBS):
        sys.exit('vector has %d entries, need %d' % (len(vec), NFEAT + len(KNOBS)))

    src = open(SRC).read()

    # Ask weights: rewrite the numeric column of the w[] initialiser in place,
    # preserving the per-feature comments that name each weight.
    idx = [0]

    def repl(m):
        i = idx[0]
        idx[0] += 1
        # %.5f, not %.4f: the optimiser writes the flat vector with five
        # decimals (weightSpec in engine/src/tuner.hpp), so rounding to four
        # here makes the shipped defaults a DIFFERENT policy from the vector
        # that was scored, and the round-trip assertion below stops being an
        # assertion.  This is the same class of defect that invalidated a v0.4
        # fitting round; it must fail loudly, not silently.
        return '%s%.5f,' % (m.group(1), vec[i]) if i < NFEAT else m.group(0)

    block_start = src.index('double w[NFEAT] = {')
    block_end = src.index('};', block_start)
    block = src[block_start:block_end]
    block = re.sub(r'(/\*[^*]*\*/\s*)-?[\d.]+,', repl, block)
    if idx[0] != NFEAT:
        sys.exit('rewrote %d ask weights, expected %d' % (idx[0], NFEAT))
    src = src[:block_start] + block + src[block_end:]

    # Decision knobs.
    for j, (field, lo, hi, is_int) in enumerate(KNOBS):
        v = clamp(vec[NFEAT + j], lo, hi, is_int)
        lit = str(v) if is_int else ('%.5f' % v)
        pat = re.compile(r'(\b%s\s*=\s*)(-?[\d.]+)' % re.escape(field))
        src, n = pat.subn(lambda m: m.group(1) + lit, src, count=1)
        if n != 1:
            sys.exit('could not rewrite %s' % field)

    open(SRC, 'w').write(src)
    print('froze %d parameters into %s' % (NFEAT + len(KNOBS),
                                           os.path.relpath(SRC, os.path.dirname(HERE))))
    print()
    print('round-trip assertion -- these two must play identically:')
    print('  ./fish match --a=v05 --b="v05:allparams=%s" --games=50 --seed=1'
          % '|'.join('%.5f' % v for v in vec[:NFEAT + len(KNOBS)]))
    print('  (a mirror of the same policy returns exactly 50%)')


if __name__ == '__main__':
    main()
