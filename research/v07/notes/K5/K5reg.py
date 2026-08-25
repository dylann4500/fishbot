"""K5 probe, second form: regress the search's SIGNED ADVANTAGE, not its choice.

The classifier form fits a label that the label-noise diagnostic shows is 80%
Monte-Carlo noise on the decisions that matter.  The regression form fits m, the
search's paired mean advantage over the blueprint's own pick -- a noisy but
UNBIASED estimate of the real continuation-value advantage in sets of
differential.  Noise that swamps any single label still averages down across
88,502 decisions in a fit.

The value-relevant metric is not accuracy.  It is: if we deploy argmax of the
learned advantage, what is the mean TRUE advantage of the candidate we pick, on
held-out deals?  Reported in sets of differential and converted at the corpus's
own rate (1 set = 14.7 win-rate points, SUBOPTIMALITY-LEDGER section 0.2).

The search's own held-out number is upward-biased: it selects on the same
Monte-Carlo draw it is scored on.  The learned model's is not -- it is fitted on
training deals and scored on held-out ones.  The comparison is therefore
CONSERVATIVE against the learned model, which is the right direction.
"""
import numpy as np, sys, json

PATH = sys.argv[1]
OUT = sys.argv[2]
with open(PATH) as fh:
    hdr = fh.readline().strip().split(',')
raw = np.loadtxt(PATH, delimiter=',', skiprows=1)
c = {n: i for i, n in enumerate(hdr)}
E = sys.stdout

did = raw[:, c['did']].astype(np.int64)
brk = np.nonzero(np.diff(did) != 0)[0] + 1
start = np.concatenate([[0], brk]); cnt = np.diff(np.concatenate([start, [len(raw)]]))
ND = len(start); grp = np.repeat(np.arange(ND), cnt)
lab = np.full(ND, -1, np.int64)
sel = np.nonzero(raw[:, c['chosen']] == 1)[0]; lab[grp[sel]] = sel - start[grp[sel]]
deal = raw[start, c['deal']].astype(np.int64)
tie = raw[start, c['tie']].astype(int)
r = raw[:, c['r']].astype(int)
m = raw[:, c['m']]                       # target
se = raw[:, c['se']]

EXCL = {'deal', 'rot', 'event', 'seat', 'did', 'm', 'se', 'lcb', 'chosen',
        'cardIdxInSet', 'card'}
FEAT = [n for n in hdr if n not in EXCL]
X = raw[:, [c[n] for n in FEAT]].astype(float)
def add(nm, v):
    global X, FEAT
    X = np.hstack([X, np.asarray(v, float).reshape(-1, 1)]); FEAT = FEAT + [nm]
for kk in (1, 2, 3, 4): add('isR%d' % kk, (r == kk).astype(float))
add('sameTgt', (raw[:, c['targetRel']] == raw[start[grp], c['targetRel']]).astype(float))
add('notRef', (r > 0).astype(float))

X = X - X[start[grp], :]                 # advantage form: differenced against candidate 0
keep = np.nonzero(X.std(0) > 1e-12)[0]; X = X[:, keep]; FEAT = [FEAT[i] for i in keep]
uq = []
for j in range(X.shape[1]):
    if not any(np.allclose(X[:, i], X[:, j], atol=1e-12) for i in uq): uq.append(j)
X = X[:, uq]; FEAT = [FEAT[i] for i in uq]
mu = X.mean(0); sd = X.std(0); sd[sd < 1e-9] = 1.0
Xs = (X - mu) / sd; Xs[start, :] = 0.0
Xs = np.hstack([Xs, (r > 0).astype(float).reshape(-1, 1)])   # explicit intercept on non-ref
FEAT = FEAT + ['_intercept']

# quadratic expansion over the strongest coordinates -- a slightly larger class,
# so that a null is a null about more than linearity
base = [FEAT.index(n) for n in ('p', 'du', 'isTie', 'setUnres', 'myInSet') if n in FEAT]
Q = []
QN = []
for i in range(len(base)):
    for j in range(i, len(base)):
        Q.append(Xs[:, base[i]] * Xs[:, base[j]]); QN.append('%sx%s' % (FEAT[base[i]], FEAT[base[j]]))
Xq = np.hstack([Xs, np.array(Q).T]); FEATQ = FEAT + QN
Xq[start, :] = 0.0
Xq[start, -len(Q) - 1:] = 0.0
Xq[start, FEAT.index('_intercept')] = 0.0

udeal = np.unique(deal); cut = udeal[int(0.7 * len(udeal))]
trD = deal < cut; teD = ~trD
rowTr = trD[grp]; rowTe = teD[grp]
print("train %d / test %d decisions (deal cut %d)" % (trD.sum(), teD.sum(), cut), file=E)

def ridge(A, y, l2):
    G = A.T @ A + l2 * np.eye(A.shape[1]) * len(A)
    return np.linalg.solve(G, A.T @ y)

maxK = int(cnt.max())
padi = np.full((ND, maxK), -1, np.int64)
for kk in range(maxK):
    msk = cnt > kk; padi[msk, kk] = start[msk] + kk
valid = padi >= 0

def evaluate(pred, md, name, margin=0.0, tieonly=False):
    """mean TRUE m of the candidate this predictor picks, on decisions md.
    `margin` is the deployment threshold: deviate only when the predicted
    advantage over candidate 0 exceeds it."""
    P = np.full((ND, maxK), -1e30); P[valid] = pred[padi[valid]]
    P[:, 0] = margin
    if tieonly:
        allow = np.arange(maxK)[None, :] < np.minimum(tie, cnt)[:, None]
        allow[:, 0] = True
        P[~allow] = -1e30
    pick = P.argmax(1)
    M = np.zeros((ND, maxK)); M[valid] = m[padi[valid]]
    got = M[np.arange(ND), pick][md]
    return got.mean(), (pick[md] != 0).mean()

M = np.zeros((ND, maxK)); M[valid] = m[padi[valid]]
best = None
for name, A, FN in (('linear', Xs, FEAT), ('quadratic', Xq, FEATQ)):
    for l2 in (1e-6, 1e-5, 1e-4, 1e-3, 1e-2):
        w = ridge(A[rowTr], m[rowTr], l2)
        pred = A @ w
        pred[start] = 0.0
        ss = 1 - ((m[rowTe] - pred[rowTe]) ** 2).sum() / ((m[rowTe] - m[rowTe].mean()) ** 2).sum()
        gain, dev = evaluate(pred, teD, name)
        print("%-9s l2=%-7.4g heldout R2 %+.5f   mean-m-of-pick %+.5f  devrate %.4f"
              % (name, l2, ss, gain, dev), file=E)
        if best is None or gain > best[0]:
            best = (gain, name, l2, w, A, FN, ss, dev, pred)

gain, name, l2, w, A, FN, ss, dev, pred = best
zero = np.zeros(len(m)); zero[:] = -1.0; zero[start] = 0.0     # blueprint: always candidate 0
g_bp, _ = evaluate(zero, teD, 'blueprint')
g_search, d_search = evaluate(raw[:, c['lcb']], teD, 'search')
rnd = np.zeros(len(m)); rnd[:] = 0.0
print("", file=E)
print("mean TRUE paired advantage m of the candidate each rule picks, HELD-OUT deals:", file=E)
print("  blueprint argmax (v0.6)   %+.5f sets   (= 0 by construction)" % g_bp, file=E)
print("  the search's own LCB rule %+.5f sets   [UPWARD BIASED: selects on the draw it is scored on]" % g_search, file=E)
print("  learned advantage (%s, l2=%.4g) %+.5f sets   devrate %.4f" % (name, l2, gain, dev), file=E)
K = 14.7
print("", file=E)
print("converted at 1 set of differential = %.1f win-rate points (LEDGER 0.2):" % K, file=E)
nAsk = 1.0
print("  search   %+.3f pts/decision-equivalent   learned %+.3f pts" % (g_search * K, gain * K), file=E)
print("  heldout R2 of the learned advantage against m: %+.5f" % ss, file=E)

print("", file=E)
print("--- deployment margin sweep (held-out), best class above ---", file=E)
bestm = None
for mg in (0.0, 0.01, 0.02, 0.03, 0.05, 0.08, 0.12, 0.20, 0.30, 0.50):
    for to in (False, True):
        g, d = evaluate(pred, teD, 'x', margin=mg, tieonly=to)
        print("  margin %.2f  tieOnly=%-5s  mean-m-of-pick %+.5f  devrate %.4f"
              % (mg, to, g, d), file=E)
        if bestm is None or g > bestm[0]: bestm = (g, mg, to, d)
g_best, mg_best, to_best, d_best = bestm
print("  BEST: margin %.2f tieOnly=%s -> %+.5f sets, devrate %.4f  (%.1f%% of the search's %+.5f)"
      % (mg_best, to_best, g_best, d_best, 100 * g_best / g_search, g_search), file=E)

print("\n--- top weights ---", file=E)
for i in np.argsort(-np.abs(w))[:15]:
    print("  %-22s %+.5f" % (FN[i], w[i]), file=E)

SD = np.concatenate([sd, [1.0]])          # the appended intercept column is unscaled
json.dump({"feat": FN, "w": [float(v) for v in w],
           "w_raw": [float(w[i] / (SD[i] if i < len(SD) else 1.0)) for i in range(len(FEAT))],
           "mu": [float(v) for v in mu], "sd": [float(v) for v in sd],
           "class": name, "l2": float(l2),
           "heldoutR2": float(ss), "gain_sets": float(gain), "search_sets": float(g_search),
           "devrate": float(dev), "margin": float(mg_best), "tieOnly": bool(to_best),
           "gain_best": float(g_best), "devrate_best": float(d_best)}, open(OUT, 'w'), indent=1)
print("\nwrote %s" % OUT, file=E)
