import json, glob, os
R = '/Users/dylan/Documents/GitHub/fish optimization/research/v07/results/'
rows = []
for f in sorted(glob.glob(R + 'K5*.jsonl')):
    if os.path.getsize(f) == 0: continue
    d = json.load(open(f))
    rows.append((os.path.basename(f), d))

out = []
out.append("K5 -- amortising test-time search into a learned evaluator.  Measured cells.")
out.append("All paired duplicate blocks, --rotations=2, --threads=2.  Training bank 7030004,")
out.append("disjoint from both evaluation banks.  edge = 100*winRateA - 50.")
out.append("")
out.append("%-32s %7s %8s %18s %8s" % ("cell", "n", "edge", "95% CI", "hw98"))
for name, d in rows:
    out.append("%-32s %7d %+8.2f  [%+6.2f, %+6.2f] %8.2f" % (
        name.replace('.jsonl', ''), d['games'], 100 * d['winRateA'] - 50,
        100 * d['ci'][0] - 50, 100 * d['ci'][1] - 50, d['power']['halfWidth98Games']))

def pool(prefix):
    xs = [(100 * d['winRateA'] - 50, d['games']) for n, d in rows if n.startswith(prefix)]
    if len(xs) < 2: return None
    e = sum(a for a, _ in xs) / len(xs); n = sum(b for _, b in xs)
    return e, n, 98.0 / (n ** 0.5)

out.append("")
out.append("pooled (mean of the two bank edges, pooled n):")
for p, lbl in (('K5-vs-v06', 'learned re-ranker, unrestricted, vs v06'),
               ('K5tie-vs-v06', 'learned re-ranker, TIE GROUP ONLY, vs v06'),
               ('K5tie-vs-rtie', 'learned re-ranker, TIE GROUP ONLY, vs v06:rtie=1')):
    r = pool(p)
    if r: out.append("  %-46s %+6.2f  n=%d  +-%.2f" % (lbl, r[0], r[1], r[2]))

out.append("")
out.append("throughput, all three measured back to back on the same loaded machine")
out.append("(six agents, load ~70 on 15 cores), 800 games each, 2 threads.  The ABSOLUTE")
out.append("numbers are depressed by roughly 3.7x against the unloaded calibration; the")
out.append("RATIOS are the quantity of interest.")
try:
    S = '/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/f63a50cf-0555-4d3c-99d1-238332bf1a3d/scratchpad/'
    l = json.load(open(S + 'K5-tp-learned.json')); s = json.load(open(S + 'K5-tp-search.json'))
    v = json.load(open(S + 'K5-tp-v06.json'))
    out.append("  learned tie-only vs v06   %6.2f games/s" % l['gamesPerSec'])
    out.append("  F-cheap search   vs v06   %6.2f games/s" % s['gamesPerSec'])
    out.append("  v06              vs v06   %6.2f games/s" % v['gamesPerSec'])
    out.append("  learned / search = %.1fx    learned / v06 = %.3fx    v06 / search = %.1fx"
               % (l['gamesPerSec'] / s['gamesPerSec'], l['gamesPerSec'] / v['gamesPerSec'],
                  v['gamesPerSec'] / s['gamesPerSec']))
    out.append("  NOTE: the corpus's 242x/300x figure is F-SEARCH (unrestricted).  F-cheap,")
    out.append("  the configuration whose decisions were distilled, is only ~3.2x the")
    out.append("  blueprint on this basis -- consistent with the phase-3 brief's own 2-thread")
    out.append("  calibration (22.7 vs 67 games/s = 2.95x).  The cost problem the amortisation")
    out.append("  was built to solve is 3x, not 242x, for this operating point.")
except Exception as e:
    out.append("  (timings unavailable: %s)" % e)

txt = "\n".join(out) + "\n"
open(R + 'K5-screen.txt', 'w').write(txt)
print(txt)
