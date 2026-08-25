"""Is the search's apparent per-decision selection value a winner's curse?

Null model: every candidate of every decision has TRUE advantage exactly zero.
Each candidate's observed advantage is drawn from N(0, se_r) with the se the
capture actually recorded, and the LCB rule the engine actually runs is applied:

    LCB_0 = 0                                   (the blueprint's own pick)
    LCB_r = m_r - kappa(r) * se_r,   kappa = kappaTie (0.0) inside the tie group,
                                             x.kappa (2.5) outside it

Under this null the search deviates whenever noise pushes some candidate's LCB
above zero, and the mean drawn m of whatever it picks is pure selection bias.
Compare that number with the +0.1362 sets the capture actually shows.  If they
agree, the search's per-decision advantage carries no information at all and the
whole quantity the amortisation was going to learn is an artifact.
"""
import numpy as np, sys

PATH = sys.argv[1]
KAPPA, KAPPATIE = 2.5, 0.0
rng = np.random.default_rng(20260824)

with open(PATH) as fh:
    hdr = fh.readline().strip().split(',')
raw = np.loadtxt(PATH, delimiter=',', skiprows=1)
c = {n: i for i, n in enumerate(hdr)}
did = raw[:, c['did']].astype(np.int64)
brk = np.nonzero(np.diff(did) != 0)[0] + 1
start = np.concatenate([[0], brk]); cnt = np.diff(np.concatenate([start, [len(raw)]]))
ND = len(start); grp = np.repeat(np.arange(ND), cnt)
lab = np.full(ND, -1, np.int64)
sel = np.nonzero(raw[:, c['chosen']] == 1)[0]; lab[grp[sel]] = sel - start[grp[sel]]

se = raw[:, c['se']].copy()
m = raw[:, c['m']]
r = raw[:, c['r']].astype(int)
tie = raw[start, c['tie']].astype(int)

maxK = int(cnt.max())
padi = np.full((ND, maxK), -1, np.int64)
for kk in range(maxK):
    msk = cnt > kk; padi[msk, kk] = start[msk] + kk
valid = padi >= 0
SE = np.zeros((ND, maxK)); SE[valid] = se[padi[valid]]
R = np.zeros((ND, maxK), int); R[valid] = r[padi[valid]]
M = np.zeros((ND, maxK)); M[valid] = m[padi[valid]]

# the LCB penalty each candidate actually carries
KAP = np.where(R < tie[:, None], KAPPATIE, KAPPA)
usable = valid & (SE < 1e8)
usable[:, 0] = True                 # the reference is exact by construction

print("observed: mean m of the search's pick   %+.5f sets  (devrate %.4f)"
      % (M[np.arange(ND), lab].mean(), (lab > 0).mean()))

NREP = 40
tot, totdev = 0.0, 0.0
for _ in range(NREP):
    draw = rng.normal(0.0, np.where(usable, SE, 0.0))
    draw[:, 0] = 0.0
    L = np.where(usable, draw - KAP * SE, -1e30)
    L[:, 0] = 0.0
    pick = L.argmax(1)
    tot += draw[np.arange(ND), pick].mean()
    totdev += (pick != 0).mean()
print("NULL (true advantage identically zero, real se, real LCB rule):")
print("   mean drawn m of the pick             %+.5f sets  (devrate %.4f)"
      % (tot / NREP, totdev / NREP))
print("   -> the observed value is %.1f%% of what pure selection-on-noise produces"
      % (100 * M[np.arange(ND), lab].mean() / (tot / NREP)))

# restricted to the bit-for-bit tie group, where the LCB rule applies NO shrinkage
tg = tie >= 2
print("")
print("inside the bit-for-bit tie group (kappaTie = 0, no shrinkage at all):")
print("   observed mean m of pick %+.5f   devrate %.4f"
      % (M[tg][np.arange(tg.sum()), lab[tg]].mean(), (lab[tg] > 0).mean()))
tot2, dev2 = 0.0, 0.0
for _ in range(NREP):
    draw = rng.normal(0.0, np.where(usable, SE, 0.0)); draw[:, 0] = 0.0
    L = np.where(usable, draw - KAP * SE, -1e30); L[:, 0] = 0.0
    pick = L.argmax(1)
    tot2 += draw[tg, :][np.arange(tg.sum()), pick[tg]].mean()
    dev2 += (pick[tg] != 0).mean()
print("   null     mean m of pick %+.5f   devrate %.4f" % (tot2 / NREP, dev2 / NREP))
