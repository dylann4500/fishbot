#!/usr/bin/env python3
"""Bake a fitted parameter vector into V04Config so that the bare spec `v04`
constructs the shipping configuration."""
import re, sys, pathlib

NAMES = [
 'hit probability', 'squared hit', 'certain hit', 'own set progress', 'team control',
 'lock completion', 'continuation', 'completion bonus', 'reply threat', 'information leak',
 'target hand size', 'empties target', 'repeats set', 'known team cards', 'location entropy',
 'team owns set', 'exposure on miss', 'trailing pressure', 'runway', 'leak magnitude']

def main():
    vec = [float(x) for x in sys.argv[1].split('|')]
    assert len(vec) == 34, f'expected 34 params, got {len(vec)}'
    p = pathlib.Path(__file__).resolve().parent / 'src' / 'v04.hpp'
    s = p.read_text()
    start = s.index('  double w[NFEAT] = {')
    end = s.index('  };', start) + len('  };\n')
    lines = ['  double w[NFEAT] = {']
    for i, name in enumerate(NAMES):
        lines.append(f'    /*{i:<2d} {name:<18s}*/ {vec[i]:>9.4f},')
    lines.append('  };')
    s = s[:start] + '\n'.join(lines) + '\n' + s[end:]

    def setval(field, value, fmt='%.5f'):
        nonlocal s
        s = re.sub(r'(\b' + field + r'\s*=\s*)[-\w.]+(\s*;)', lambda m: m.group(1) + (fmt % value) + m.group(2), s, count=1)

    setval('declThreshold',      min(0.9999, max(0.5, vec[20])))
    setval('lockedAllocThresh',  min(0.99999, max(0.5, vec[21])))
    setval('askFloor',           min(0.9, max(0.0, vec[22])))
    setval('patiencePool',       int(round(min(45, max(0, vec[23])))), '%d')
    setval('oppCardFloor',       min(20.0, max(0.0, vec[24])))
    setval('valueWeight',        max(0.0, vec[25]))
    setval('linearWeight',       max(0.0, vec[26]))
    setval('minTeamProb',        min(0.99, max(0.05, vec[27])))
    setval('declareMargin',      vec[28])
    setval('priorTheta',         min(2.0, max(0.0, vec[29])))
    setval('priorPhi',           min(1.0, max(0.0, vec[30])))
    setval('searchTopK',         int(round(min(24, max(0, vec[31])))), '%d')
    setval('chainWeight',        max(0.0, vec[32]))
    setval('threatWeight',       max(0.0, vec[33]))
    p.write_text(s)
    print('frozen into', p)

if __name__ == '__main__':
    main()
