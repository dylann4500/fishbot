#!/usr/bin/env python3
"""Pick the shipping configuration on a validation bank that fitting never saw.

The per-generation 'best' reported by the optimiser is the maximum over a
population evaluated on shared seeds, and a maximum is upward biased.  We
therefore re-evaluate the last few generation means on a fresh bank and select
there, so the frozen configuration is chosen on data that had no influence on
the search.
"""
import json, math, subprocess, sys, os, argparse

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
BIN = os.environ.get('FISHBIN', os.path.join(os.path.dirname(__file__), 'fish2'))

def winrate(spec, opp, games, seed, rotations, threads):
    out = subprocess.run([BIN, 'match', f'--a={spec}', f'--b={opp}', f'--games={games}',
                          f'--seed={seed}', f'--rotations={rotations}', '--json',
                          f'--threads={threads}'], capture_output=True, text=True).stdout
    return json.loads(out)['winRateA']

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--trace', required=True)
    ap.add_argument('--last', type=int, default=6)
    ap.add_argument('--games', type=int, default=400)
    ap.add_argument('--seed', type=int, default=1357911)
    ap.add_argument('--rotations', type=int, default=2)
    ap.add_argument('--threads', type=int, default=0)
    ap.add_argument('--panel', default='v03,lockout,detective,v02')
    ap.add_argument('--beta', type=float, default=8.0)
    a = ap.parse_args()

    rows = [json.loads(l) for l in open(a.trace) if l.startswith('{"gen')]
    if not rows:
        print('no generations in trace'); sys.exit(1)
    cands = []
    seen = set()
    for r in rows[-a.last:]:
        key = tuple(round(x, 4) for x in r['mu'])
        if key in seen: continue
        seen.add(key)
        cands.append((r['gen'], r['mu']))
    panel = a.panel.split(',')
    best = None
    for gen, mu in cands:
        spec = 'v04:allparams=' + '|'.join(f'{x:.5f}' for x in mu)
        wrs = [winrate(spec, o, a.games, a.seed + 17 * i, a.rotations, a.threads) for i, o in enumerate(panel)]
        score = -math.log(sum(math.exp(-a.beta * w) for w in wrs)) / a.beta
        print(f'gen {gen:3d}  softmin {score:.4f}  min {min(wrs):.4f}  ' +
              '  '.join(f'{o}={w:.3f}' for o, w in zip(panel, wrs)), flush=True)
        if best is None or score > best[0]:
            best = (score, gen, mu, wrs)
    print()
    print(f'selected generation {best[1]} with validation soft-min {best[0]:.4f}')
    print('allparams=' + '|'.join(f'{x:.5f}' for x in best[2]))
    json.dump({'gen': best[1], 'softmin': best[0], 'winRates': best[3], 'params': best[2],
               'panel': panel, 'validationSeed': a.seed, 'games': a.games},
              open(os.path.join(ROOT, 'research', 'v04', 'runs', 'selected.json'), 'w'), indent=2)

if __name__ == '__main__':
    main()
