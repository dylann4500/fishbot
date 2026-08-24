#!/usr/bin/env python3
"""FishBot v0.7 -- generate every table in docs/v07/INSTRUMENT.md from the artifacts.

The project convention is that no number in a report is hand-typed: `build_tables_v06.py`
emits the v0.6 paper's macros from `research/v06/results/`, and the v0.6 audit found four
macro mis-bindings and one sign inversion in exactly that layer (SUBOPTIMALITY-LEDGER.md
P-1, P-6).  So this script prints markdown tables ready to paste, AND a JSON block of the
key scalars, from the artifacts alone.

Usage:  python3 engine/build_tables_v07.py [--dir research/v07/results] [--json]
"""
import json, os, sys, math, argparse

def load_jsonl(path):
    rows = []
    if not os.path.exists(path):
        return rows
    for line in open(path):
        line = line.strip()
        if not line.startswith('{'):
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    return rows

def hw(n):
    """The project's power rule: 95% half-width in points at p ~ 1/2."""
    return 98.0 / math.sqrt(n) if n > 0 else float('nan')

def pts(w):
    return 100.0 * (w - 0.5)

def fmt_ci(row):
    lo, hi = row['ci']
    return f"[{pts(lo):+.2f}, {pts(hi):+.2f}]"

# ---------------------------------------------------------------- throughput
def throughput(d):
    rows = load_jsonl(os.path.join(d, 'T1-throughput.jsonl'))
    # T1b supersedes T1's search block: the 50-deal timing under-samples the
    # heavy tail of per-deal search cost (RESEARCH-LOG.md 1.17), so where a
    # spec appears in both, T1b's 400-deal row is the one rendered.
    t1b = load_jsonl(os.path.join(d, 'T1b-throughput-400.jsonl'))
    if t1b:
        seen = {(r['spec'], r['mirror']) for r in t1b}
        rows = [r for r in rows if r['mirror'] or (r['spec'], r['mirror']) not in seen] + t1b
    if not rows:
        return "*(T1-throughput.jsonl absent)*\n", {}
    out = []
    blocks, cur, mirror = [], [], None
    for r in rows:
        if mirror is None or r['mirror'] != mirror:
            if cur: blocks.append((mirror, cur))
            cur, mirror = [], r['mirror']
        cur.append(r)
    if cur: blocks.append((mirror, cur))
    scal = {}
    for mir, blk in blocks:
        basis = ("mirror (each policy against itself, E9's basis)" if mir
                 else f"against `{blk[0]['opp']}` (the ledger's F-search basis)")
        out.append(f"\n**Basis: {basis}.** Reference row is the first; ratios are within the block "
                   f"and within one basis.\n")
        out.append("| configuration | games/s, all threads | games/s, 1 thread | × ref (all) | × ref (1 thr) | deals timed |")
        out.append("|---|---:|---:|---:|---:|---:|")
        for r in blk:
            out.append(f"| `{r['spec']}` | {r['gamesPerSecAll']:.2f} | {r['gamesPerSecOne']:.3f} | "
                       f"{r['relAll']:.3f} | {r['relOne']:.3f} | {r['deals']} / {r['deals1']} |")
        for r in blk:
            scal[('mirror' if mir else 'vs') + ':' + r['spec']] = r['gamesPerSecAll']
    return "\n".join(out) + "\n", scal

# --------------------------------------------------------------- floor table
def floor(d):
    rows = load_jsonl(os.path.join(d, 'C1-floor.jsonl'))
    if not rows:
        return "*(C1-floor.jsonl absent)*\n", {}
    dtrue, resp, meta = {}, {}, {}
    for r in rows:
        if 'budget' in r and 'tag' in r and 'target' in r:
            meta[r['tag']] = r
        elif r.get('row') == 'dTrue':
            dtrue[r['tag']] = r
        elif r.get('row') in ('C1', 'C2', 'C3', 'C5'):
            resp.setdefault(r['tag'], {}).setdefault(r['row'], []).append(r)
    order = [t for t in meta] or list(dtrue)
    out = ["| target | dTrue (pts) | 95% CI | class | dFound bank A | dFound bank B | 95% CI (A) | "
           "responder decl. acc. | forced/game | limit hits |",
           "|---|---:|---|---|---:|---:|---|---:|---:|---:|"]
    scal = {}
    for tag in order:
        dt = dtrue.get(tag)
        dtv = pts(dt['winRateA']) if dt else float('nan')
        dtc = fmt_ci(dt) if dt else ''
        scal[f'dTrue:{tag}'] = dtv
        for cls in ('C1', 'C2', 'C3', 'C5'):
            rs = resp.get(tag, {}).get(cls, [])
            if not rs:
                continue
            ra = rs[0]
            rb = rs[1] if len(rs) > 1 else None
            out.append(
                f"| `{tag}` | {dtv:+.2f} | {dtc} | {cls} | {pts(ra['winRateA']):+.2f} | "
                f"{(('%+.2f' % pts(rb['winRateA'])) if rb else '—')} | {fmt_ci(ra)} | "
                f"{ra['declAccA']:.4f} | {ra['forcedPerGameA']:.4f} | {ra['limitHitRate']:.4f} |")
            scal[f'dFound:{tag}:{cls}:A'] = pts(ra['winRateA'])
            if rb:
                scal[f'dFound:{tag}:{cls}:B'] = pts(rb['winRateA'])
    return "\n".join(out) + "\n", scal

def _se_from_ci(r):
    """Approximate SE in points from a percentile-bootstrap interval."""
    return (pts(r['ci'][1]) - pts(r['ci'][0])) / (2 * 1.959963985)

def excess(d):
    """The edge a responder finds ABOVE what it finds against the unhandicapped target.

    The `none` rung is not only a false-positive control: if the incumbent is
    genuinely exploitable in a class, that class reaches a positive edge there,
    and the quantity attributable to the PLANTED weakness is the excess over it.
    The two cells are run on the same deal bank but against different opponents,
    so they are not paired and the intervals are combined in quadrature -- which
    is conservative relative to a paired design and is stated as such.
    """
    rows = load_jsonl(os.path.join(d, 'C1-floor.jsonl'))
    if not rows:
        return "*(C1-floor.jsonl absent)*\n", {}
    dtrue, resp = {}, {}
    for r in rows:
        if r.get('row') == 'dTrue':
            dtrue[r['tag']] = pts(r['winRateA'])
        elif r.get('row') in ('C1', 'C2', 'C3', 'C5'):
            resp.setdefault(r['row'], {}).setdefault(r['tag'], []).append(r)
    out = ["| class | rung | dTrue | dFound | dFound − dFound(`none`) | 95% (quadrature) | excludes 0 |",
           "|---|---|---:|---:|---:|---|:--:|"]
    scal = {}
    for cls in ('C1', 'C2', 'C3', 'C5'):
        base = resp.get(cls, {}).get('none', [])
        for tag, rs in resp.get(cls, {}).items():
            for i, r in enumerate(rs):
                bank = r.get('bank')
                b0 = None
                for x in base:
                    if x.get('bank') == bank:
                        b0 = x
                if tag == 'none' or b0 is None:
                    out.append(f"| {cls} | `{tag}` | {dtrue.get(tag, float('nan')):+.2f} | "
                               f"{pts(r['winRateA']):+.2f} | — | — | — |")
                    continue
                ex = pts(r['winRateA']) - pts(b0['winRateA'])
                se = math.hypot(_se_from_ci(r), _se_from_ci(b0))
                lo, hi = ex - 1.959963985 * se, ex + 1.959963985 * se
                out.append(f"| {cls} | `{tag}` | {dtrue.get(tag, float('nan')):+.2f} | "
                           f"{pts(r['winRateA']):+.2f} | {ex:+.2f} | [{lo:+.2f}, {hi:+.2f}] | "
                           f"{'yes' if lo > 0 else 'no'} |")
                scal[f'excess:{cls}:{tag}:{bank}'] = ex
    return "\n".join(out) + "\n", scal

def pooled(d):
    """Both banks together, and the replication verdict.

    The project's rule is that a claim is replicated only if it holds in sign
    and size on both banks; the pooled estimate is reported ALONGSIDE the two
    banks, never instead of them, because pooling hides a disagreement and the
    per-bank rows are what show one.
    """
    rows = load_jsonl(os.path.join(d, 'C1-floor.jsonl'))
    if not rows:
        return "*(C1-floor.jsonl absent)*\n", {}
    dtrue, resp = {}, {}
    for r in rows:
        if r.get('row') == 'dTrue':
            dtrue[r['tag']] = pts(r['winRateA'])
        elif r.get('row') in ('C1', 'C2', 'C3', 'C5'):
            resp.setdefault(r['row'], {}).setdefault(r['tag'], []).append(r)
    out = ["| class | rung | dTrue | bank A | bank B | pooled | 95% (pooled) | n (games) | same sign |",
           "|---|---|---:|---:|---:|---:|---|---:|:--:|"]
    scal = {}
    for cls in ('C1', 'C2', 'C3', 'C5'):
        for tag, rs in resp.get(cls, {}).items():
            if len(rs) < 2:
                if rs:
                    r = rs[0]
                    out.append(f"| {cls} | `{tag}` | {dtrue.get(tag, float('nan')):+.2f} | "
                               f"{pts(r['winRateA']):+.2f} | — | — | — | {r['games']} | — |")
                continue
            a2, b2 = rs[0], rs[1]
            na, nb = a2['games'], b2['games']
            p = (pts(a2['winRateA']) * na + pts(b2['winRateA']) * nb) / (na + nb)
            sa, sb = _se_from_ci(a2), _se_from_ci(b2)
            se = math.sqrt((sa * na) ** 2 + (sb * nb) ** 2) / (na + nb)
            lo, hi = p - 1.959963985 * se, p + 1.959963985 * se
            same = (pts(a2['winRateA']) > 0) == (pts(b2['winRateA']) > 0)
            out.append(f"| {cls} | `{tag}` | {dtrue.get(tag, float('nan')):+.2f} | "
                       f"{pts(a2['winRateA']):+.2f} | {pts(b2['winRateA']):+.2f} | {p:+.2f} | "
                       f"[{lo:+.2f}, {hi:+.2f}] | {na + nb} | {'yes' if same else '**no**'} |")
            scal[f'pooled:{cls}:{tag}'] = p
            scal[f'pooledLo:{cls}:{tag}'] = lo
    return "\n".join(out) + "\n", scal

def floor_summary(d):
    """The headline the phase-1 exit criterion turns on.

    A class's DETECTION FLOOR is the smallest planted edge at which its EXCESS
    OVER THE UNHANDICAPPED CONTROL excludes zero on BOTH evaluation banks.

    Two corrections are built into that sentence and both matter.  It is the
    EXCESS, not the raw edge, because the control rung is not zero: a properly
    specified responder reaches +0.76 to +1.86 points against the unhandicapped
    incumbent, so a class that clears zero on a handicapped rung may simply be
    showing the same exploitability it shows everywhere.  And it is BOTH BANKS,
    because the project's standing rule is that a claim is replicated only if it
    holds in sign and size on both; one bank is a result about one bank.

    `excessVsRef` is the excess minus dTrue -- what the class finds beyond what
    the reference exploiter (the unhandicapped incumbent) finds on the same
    rung.  A class whose excess merely tracks dTrue is recovering the target's
    lost STRENGTH, not exploiting the planted weakness specifically.
    """
    rows = load_jsonl(os.path.join(d, 'C1-floor.jsonl'))
    if not rows:
        return "*(C1-floor.jsonl absent)*\n", {}
    dtrue, resp = {}, {}
    for r in rows:
        if r.get('row') == 'dTrue':
            dtrue[r['tag']] = pts(r['winRateA'])
        elif r.get('row') in ('C1', 'C2', 'C3', 'C5'):
            resp.setdefault(r['row'], {}).setdefault(r['tag'], []).append(r)
    out = ["| class | rungs detected (excess over control excludes 0 on both banks) | detection floor | rungs measured |",
           "|---|---|---:|---:|"]
    scal = {}
    detail = ["", "Per rung, pooled over both banks:", "",
              "| class | rung | dTrue | pooled edge | excess over control | excess − dTrue | detected |",
              "|---|---|---:|---:|---:|---:|:--:|"]
    for cls in ('C1', 'C2', 'C3', 'C5'):
        base = resp.get(cls, {}).get('none', [])
        if not base:
            continue
        found, n = [], 0
        for tag, rs in resp[cls].items():
            if tag == 'none':
                continue
            n += 1
            ok, exs = True, []
            for r in rs:
                b0 = next((x for x in base if x.get('bank') == r.get('bank')), None)
                if b0 is None:
                    ok = False; continue
                ex = pts(r['winRateA']) - pts(b0['winRateA'])
                se = math.hypot(_se_from_ci(r), _se_from_ci(b0))
                exs.append(ex)
                if ex - 1.959963985 * se <= 0:
                    ok = False
            if len(rs) < 2:
                ok = False
            dt = dtrue.get(tag, float('nan'))
            pooled_edge = sum(pts(r['winRateA']) * r['games'] for r in rs) / sum(r['games'] for r in rs)
            pooled_ctrl = sum(pts(x['winRateA']) * x['games'] for x in base) / sum(x['games'] for x in base)
            ex_pool = pooled_edge - pooled_ctrl
            detail.append(f"| {cls} | `{tag}` | {dt:+.2f} | {pooled_edge:+.2f} | {ex_pool:+.2f} | "
                          f"{ex_pool - dt:+.2f} | {'**yes**' if ok else 'no'} |")
            scal[f'excessPooled:{cls}:{tag}'] = ex_pool
            if ok:
                found.append((dt, tag))
        found.sort()
        floor_v = found[0][0] if found else float('nan')
        names = ", ".join(f"`{t}` ({v:+.2f})" for v, t in found) or "**none**"
        out.append(f"| {cls} | {names} | "
                   f"{('%.2f pts' % floor_v) if found else '**not reached**'} | {n} |")
        scal[f'floor:{cls}'] = floor_v
    return "\n".join(out) + "\n" + "\n".join(detail) + "\n", scal

# ----------------------------------------------------------------- T2 / W1 / D1
def searchstrength(d):
    rows = load_jsonl(os.path.join(d, 'T2-searchstrength.jsonl'))
    if not rows:
        return "*(T2-searchstrength.jsonl absent)*\n", {}
    out = ["| search configuration | bank | win rate vs `v06` | 95% CI | n (games) | 98/√N | games/s |",
           "|---|---:|---:|---|---:|---:|---:|"]
    scal = {}
    for r in rows:
        n = r['games']
        out.append(f"| `{r['a']}` | {r['bank']} | {100*r['winRateA']:.2f}% | "
                   f"[{100*r['ci'][0]:.2f}, {100*r['ci'][1]:.2f}] | {n} | {hw(n):.2f} | "
                   f"{r.get('gamesPerSec', float('nan')):.2f} |")
        scal[f"T2:{r['a']}:{r['bank']}"] = 100 * r['winRateA']
    return "\n".join(out) + "\n", scal

def inversion(d):
    rows = load_jsonl(os.path.join(d, 'W1-inversion.jsonl'))
    if not rows:
        return "*(W1-inversion.jsonl absent)*\n", {}
    out = ["| observer | target | model | bank | bits/ask | SE | surviving frac. | nats base → inv. | argmax base → inv. | inverted asks |",
           "|---|---|---|---:|---:|---:|---:|---|---|---:|"]
    scal = {}
    for r in rows:
        out.append(f"| `{r['a']}` | `{r['b']}` | `{r['model']}` | {r['seed']} | {r['bitsPerAsk']:.4f} | "
                   f"{r['bitsSE']:.4f} | {r['meanConsistentFrac']:.4f} | "
                   f"{r['natsBase']:.5f} → {r['natsInv']:.5f} | "
                   f"{r['argmaxBase']:.4f} → {r['argmaxInv']:.4f} | {r['inverted']} |")
        scal[f"W1:{r['b']}:{r['seed']}:bits"] = r['bitsPerAsk']
        scal[f"W1:{r['b']}:{r['seed']}:dArgmax"] = 100 * (r['argmaxInv'] - r['argmaxBase'])
    return "\n".join(out) + "\n", scal

def decisions(d):
    rows = load_jsonl(os.path.join(d, 'D1-decisions.jsonl'))
    if not rows:
        return "*(D1-decisions.jsonl absent)*\n", {}
    names = list(rows[0]['metrics'].keys())
    out = ["| arm | bank | " + " | ".join(f"`{n}`" for n in names) + " |",
           "|---|---:|" + "---:|" * len(names)]
    scal = {}
    for r in rows:
        cells = []
        for n in names:
            m = r['metrics'][n]
            cells.append(f"{m['rate']:.5f}" if m['n'] else "—")
            scal[f"D1:{r['a']}:{r['seed']}:{n}"] = m['rate']
        out.append(f"| `{r['a']}` | {r['seed']} | " + " | ".join(cells) + " |")
    out.append("")
    out.append("Decision counts and intervals, first row only:")
    out.append("")
    out.append("| metric | rate | 95% CI (deal-clustered) | decisions | 98/√n |")
    out.append("|---|---:|---|---:|---:|")
    for n in names:
        m = rows[0]['metrics'][n]
        out.append(f"| `{n}` | {m['rate']:.5f} | [{m['ci'][0]:.4f}, {m['ci'][1]:.4f}] | "
                   f"{m['n']:.0f} | {m['halfWidth98']:.3f} |")
    return "\n".join(out) + "\n", scal

def budget(d):
    rows = load_jsonl(os.path.join(d, 'C2-budget.jsonl'))
    if not rows:
        return "*(C2-budget.jsonl absent)*\n", {}
    specs = {(r['target'], r['gens'], r['pop'], r['deals']): r for r in rows if r.get('row') == 'budgetSpec'}
    out = ["| target | gens × pop × deals | games spent fitting | bank | dFound (pts) | 95% CI |",
           "|---|---|---:|---:|---:|---|"]
    scal = {}
    for r in rows:
        if r.get('row') != 'budget':
            continue
        key = (r['target'], r['gens'], r['pop'], r['fitDeals'])
        sp = specs.get(key)
        out.append(f"| `{r['target']}` | {r['gens']} × {r['pop']} × {r['fitDeals']} | "
                   f"{sp['gamesInFit'] if sp else '?'} | {r['bank']} | {pts(r['winRateA']):+.2f} | {fmt_ci(r)} |")
        scal[f"budget:{r['target']}:{r['gens']}x{r['pop']}x{r['fitDeals']}:{r['bank']}"] = pts(r['winRateA'])
    return "\n".join(out) + "\n", scal

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', default='research/v07/results')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args()
    scal = {}
    sections = [("Throughput (T1)", throughput),
                ("Detection floor summary", floor_summary),
                ("Detection floor (C1)", floor),
                ("Both banks pooled", pooled),
                ("Edge above the unhandicapped control", excess),
                ("Fast-search strength (T2)", searchstrength), ("Budget curve (C2)", budget),
                ("Transcript inversion (W1)", inversion), ("Per-decision channel (D1)", decisions)]
    body = []
    for title, fn in sections:
        txt, s = fn(a.dir)
        scal.update(s)
        body.append(f"\n## {title}\n\n{txt}")
    if a.json:
        print(json.dumps(scal, indent=1, sort_keys=True))
    else:
        print("".join(body))

if __name__ == '__main__':
    main()
