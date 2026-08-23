#!/usr/bin/env python3
"""Bake a fitted v0.6 parameter vector into engine/src/v06.hpp.

The v0.4 study shipped a configuration whose value-function coefficients were not
the ones any recorded fit produced, because freeze_config.py wrote only the 34
policy parameters.  This script writes the WHOLE flat vector -- ask weights, the
fourteen v0.5 knobs and the three v0.6 ask terms -- and stamps the provenance
(run file, generation, objective, panel, seed) into the header next to it, so a
reader can always recover which artifact the shipped numbers came from.

usage:  python3 freeze_config_v06.py ../research/v06/runs/fitB.jsonl [--dry]
"""
import json, re, sys, os, hashlib

def load(path):
    hdr, gens, weights = None, [], None
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        d = json.loads(line)
        if 'header' in d: hdr = d
        elif 'gen' in d: gens.append(d)
        elif 'weights' in d: weights = [float(x) for x in d['weights'].split('|')]
    return hdr, gens, weights

def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(2)
    path = sys.argv[1]
    dry = '--dry' in sys.argv
    hdr, gens, w = load(path)
    src_note = "final weights record"
    if w is None:
        # A fit that is still running has no final record.  Falling back to the
        # last generation's distribution mean is legitimate -- it is what the
        # v0.5 study shipped -- but it must be STAMPED as such, because the mean
        # and the best generation are different objects and the v0.5 provenance
        # gap was exactly this.
        if not gens:
            print(f"{path}: no generation records at all"); sys.exit(1)
        w = list(gens[-1]['mu'])
        src_note = f"generation {gens[-1]['gen']} distribution mean (fit unfinished)"
    
    digest = hashlib.sha256(open(path,'rb').read()).hexdigest()[:16]
    OBJ = {0:'softmin',1:'min',2:'mean',3:'regret',4:'minimaxregret'}
    prov = (f"{os.path.basename(path)} [{src_note}] sha256:{digest} gens={len(gens)} "
            f"obj={OBJ.get(hdr.get('objective',0),'?')} paired={hdr.get('paired')} "
            f"panel={'+'.join(hdr.get('panel',[]))} seed={hdr.get('seed')} "
            f"deals={hdr.get('deals')}x{hdr.get('rotations')} sigmaRel={hdr.get('sigmaRel')}")
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src', 'v06.hpp')
    text = open(src).read()
    need = None
    m = re.search(r'static constexpr int NV6PARAM = NFEAT \+ 14 \+ 3;', text)
    if not m: print('v06.hpp: parameter block not found'); sys.exit(1)
    NFEAT = 20
    need = NFEAT + 14 + 3
    if len(w) < need:
        w = w + [0.0] * (need - len(w))
    w = w[:need]
    rows = []
    for i in range(0, need, 8):
        rows.append('  ' + ', '.join('%.5f' % x for x in w[i:i+8]) + ',')
    body = '\n'.join(rows).rstrip(',')
    block = ('// FIT-VECTOR-BEGIN\n'
             'static constexpr int NV6PARAM = NFEAT + 14 + 3;\n'
             'static constexpr double V6PARAMS[NV6PARAM] = {\n' + body + '\n};\n'
             f'static constexpr const char* V6FIT_PROVENANCE = "{prov}";\n'
             '// FIT-VECTOR-END')
    new = re.sub(r'// FIT-VECTOR-BEGIN.*?// FIT-VECTOR-END', block, text, flags=re.S)
    if dry:
        print(block); return
    open(src, 'w').write(new)
    print(f"froze {need} coordinates into {src}")
    print(f"provenance: {prov}")

if __name__ == '__main__':
    main()
