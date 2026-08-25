#!/usr/bin/env python3
"""FishBot v0.7 phase 2 -- seal the evaluation material.

The phase brief: "seal the evaluation material, physically, before anything can be
tuned against it".  A bank in this corpus is a SEED plus a SIZE -- the deals are
generated from the deal index and never stored -- so a physical seal has three
parts, and this script writes all three:

  1. A COMMITMENT.  `fish7 bankdigest` folds the six dealt hands and the dealer of
     every deal the arena would play into a 64-bit rolling hash.  It plays no
     game and builds no policy, so the digest of a sealed bank can be computed
     now without learning anything about how any policy performs on it.  Phase 5
     recomputes the digest and compares: if it matches, the bank it is evaluating
     on is the bank that was sealed here, and nobody has quietly changed a seed
     or a size.
  2. An ENFORCED SEAL.  engine/src/v07_seeds.hpp marks the holdout seeds
     `Sealed` with unsealPhase 5, and engine/src/arena.hpp now refuses to run any
     match on such a seed unless FISH_UNSEAL_PHASE >= 5.  That check lives in
     runMatch, so it covers every battery, not only the ones that remember to
     call `fish seeds --require`.
  3. A SPLIT OF THE ADVERSARY BANK.  The exploiters phase 2 produced are split in
     half by a rule fixed BEFORE the results were known -- lexicographic row id,
     alternating -- so the split cannot be chosen to flatter either half.  The
     train half is plaintext.  The holdout half is written base64-encoded with the
     plaintext SHA-256 recorded beside it, so it is not readable by eye or by a
     careless grep during phases 3-4 while remaining exactly verifiable in phase 5.

Nothing here is cryptography and it does not pretend to be: a determined reader
can decode the holdout half.  What it is is a COMMITMENT -- after this script has
run and the result is committed, the sealed material cannot be silently changed,
and any phase that reads it has to do something deliberate that shows up in the
record.
"""
import base64, hashlib, json, os, subprocess, sys, datetime

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
BANKS = os.path.join(ROOT, 'research', 'v07', 'banks')
BIN = os.environ.get('FISH_BIN', os.path.join(HERE, 'fish7b'))

# --- the split, fixed here and not chosen after the fact ---------------------
TRAIN = [
    (7030001, 24000, "phase-2 adversary evaluation bank A; phases 3-4 training"),
    (7030002, 24000, "phase-2 adversary evaluation bank B; phases 3-4 training"),
    (7030003, 24000, "phase-2 replication / transfer; phases 3-4 training"),
    (7030004, 24000, "phases 3-4 reserve"),
]
HOLDOUT = [
    (7090001, 24000, "phase-5 holdout bank 1"),
    (7090002, 24000, "phase-5 holdout bank 2"),
    (7090003, 24000, "phase-5 holdout bank 3"),
    (7090004, 24000, "phase-5 fresh adversary search against the frozen v0.7"),
    (7090005, 24000, "phase-5 negative controls / planted-edge recovery"),
    (7091001, 24000, "sealed adversary half: evaluation bank"),
    (7091002, 24000, "sealed adversary half: fitting bank for the phase-5 fresh search"),
]

def digest(seed, deals, unseal=False):
    env = dict(os.environ)
    if unseal: env['FISH_UNSEAL_PHASE'] = '5'
    out = subprocess.run([BIN, 'bankdigest', f'--seed={seed}', f'--deals={deals}'],
                         capture_output=True, text=True, env=env)
    if out.returncode != 0:
        raise SystemExit(f"bankdigest failed for {seed}: {out.stderr.strip()}")
    return json.loads(out.stdout)

def sha256(b): return hashlib.sha256(b).hexdigest()

def main():
    os.makedirs(os.path.join(BANKS, 'train'), exist_ok=True)
    os.makedirs(os.path.join(BANKS, 'holdout'), exist_ok=True)
    stamp = os.popen('git -C "%s" rev-parse HEAD' % ROOT).read().strip()

    def entry(s, d, n, role, unseal=False, extra=None):
        r = digest(s, d, unseal=unseal)
        r.pop('probe', None)
        r.update(rotations=2, role=role, note=n)
        if extra: r.update(extra)
        return r
    train = [entry(s, d, n, 'train') for s, d, n in TRAIN]
    hold  = [entry(s, d, n, 'holdout', unseal=True, extra={'unsealPhase': 5}) for s, d, n in HOLDOUT]

    tj = json.dumps(dict(kind='fishbot-v07-bank-manifest', half='train', commit=stamp,
                         written='phase 2', banks=train), indent=1) + "\n"
    open(os.path.join(BANKS, 'train', 'BANKS.json'), 'w').write(tj)

    hj = json.dumps(dict(kind='fishbot-v07-bank-manifest', half='holdout', commit=stamp,
                         written='phase 2', unsealPhase=5, banks=hold), indent=1) + "\n"
    open(os.path.join(BANKS, 'holdout', 'BANKS.json'), 'w').write(hj)

    # --- the adversary bank, split by a rule fixed in advance -----------------
    specs = os.path.join(ROOT, 'research', 'v07', 'results', 'p2-specs.tsv')
    manifest_extra = {}
    if os.path.exists(specs):
        rows = [l.rstrip('\n') for l in open(specs) if l.strip() and not l.startswith('#')]
        rows.sort(key=lambda l: l.split('\t')[0])
        tr = [l for i, l in enumerate(rows) if i % 2 == 0]
        ho = [l for i, l in enumerate(rows) if i % 2 == 1]
        hdr = ("# FishBot v0.7 -- the phase-2 ADVERSARY BANK, %s half.\n"
               "# Split rule, fixed before any result was known: rows sorted by id, alternating;\n"
               "# even positions train, odd positions holdout.  Columns: id, cluster, spec.\n")
        open(os.path.join(BANKS, 'train', 'adversaries-train.tsv'), 'w').write(hdr % 'TRAIN' + "\n".join(tr) + "\n")
        pt = (hdr % 'HOLDOUT' + "\n".join(ho) + "\n").encode()
        sealed = ("# FishBot v0.7 -- the phase-2 adversary bank, SEALED HOLDOUT half.\n"
                  "# Do not decode before phase 5.  Registered as seed 7091001 in\n"
                  "# engine/src/v07_seeds.hpp (Sealed, unsealPhase 5).\n"
                  "# sha256(plaintext) = %s\n"
                  "# decode: base64 -d < this file | tail -n +2\n%s\n" % (sha256(pt), base64.b64encode(pt).decode()))
        open(os.path.join(BANKS, 'holdout', 'adversaries-holdout.sealed'), 'w').write(sealed)
        manifest_extra = dict(adversariesTrain=len(tr), adversariesHoldout=len(ho),
                              adversariesHoldoutSha256=sha256(pt))

    # --- one checksum file over everything ------------------------------------
    lines = []
    for sub in ('train', 'holdout'):
        d = os.path.join(BANKS, sub)
        for fn in sorted(os.listdir(d)):
            p = os.path.join(d, fn)
            if not os.path.isfile(p) or fn == 'MANIFEST.sha256': continue
            lines.append(f"{sha256(open(p,'rb').read())}  {sub}/{fn}")
    open(os.path.join(BANKS, 'MANIFEST.sha256'), 'w').write("\n".join(lines) + "\n")

    summary = dict(kind='fishbot-v07-seal', commit=stamp, phase=2,
                   trainBanks=[b['seed'] for b in train], holdoutBanks=[b['seed'] for b in hold],
                   **manifest_extra)
    open(os.path.join(BANKS, 'SEAL.json'), 'w').write(json.dumps(summary, indent=1) + "\n")
    print(json.dumps(summary, indent=1))
    print("\ntrain digests:"); [print(f"  {b['seed']}  {b['deals']:>6} deals  {b['digest']}") for b in train]
    print("holdout digests (committed, not played):")
    [print(f"  {b['seed']}  {b['deals']:>6} deals  {b['digest']}") for b in hold]

if __name__ == '__main__': main()
