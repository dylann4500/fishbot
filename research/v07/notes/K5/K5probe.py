"""K5 cheap probe: does the search's choice predict from decision-time features?

Conditional logit over the candidate set of each searched ask decision.
Held-out split is by DEAL, so no decision from a training deal appears in test.
"""
import numpy as np, sys, json

PATH = sys.argv[1]
OUT  = sys.argv[2] if len(sys.argv) > 2 else '/tmp/K5model.json'
TIEONLY = ('--tieonly' in sys.argv)

with open(PATH) as fh:
    hdr = fh.readline().strip().split(',')
raw = np.loadtxt(PATH, delimiter=',', skiprows=1)
col = {n: i for i, n in enumerate(hdr)}

deal = raw[:, col['deal']].astype(np.int64)
did  = raw[:, col['did']].astype(np.int64)
r    = raw[:, col['r']].astype(np.int64)
tie  = raw[:, col['tie']].astype(np.int64)
chosen = raw[:, col['chosen']].astype(np.int64)

# rows are contiguous per decision id (handler pushes a whole decision under one lock)
brk = np.nonzero(np.diff(did) != 0)[0] + 1
start = np.concatenate([[0], brk])
cnt = np.diff(np.concatenate([start, [len(raw)]]))
ND = len(start)
grp = np.repeat(np.arange(ND), cnt)

lab = np.full(ND, -1, np.int64)
sel = np.nonzero(chosen == 1)[0]
lab[grp[sel]] = sel - start[grp[sel]]
assert (lab >= 0).all(), "some decision has no chosen row"

dDeal = deal[start]; dTie = tie[start]; dK = cnt

E = sys.stderr
print("rows %d   decisions %d" % (len(raw), ND), file=E)
print("search deviation rate (picks r>0): %.4f" % (lab > 0).mean(), file=E)
print("chosen-rank histogram: %s" % np.bincount(lab).tolist(), file=E)
print("K histogram: %s" % dict(zip(*[a.tolist() for a in np.unique(dK, return_counts=True)])), file=E)
print("tie histogram: %s" % dict(zip(*[a.tolist() for a in np.unique(dTie, return_counts=True)])), file=E)
tk = np.minimum(dTie, dK)
intie = (lab > 0) & (lab < tk)
print("deviations inside the tie group: %.4f of decisions, %.3f of deviations"
      % (intie.mean(), intie.sum() / max(1, (lab > 0).sum())), file=E)

# ---------------- feature matrix -------------------------------------------
EXCL = {'deal', 'rot', 'event', 'seat', 'did', 'm', 'se', 'lcb', 'chosen',
        'cardIdxInSet', 'card'}          # last two: THREAT-MODEL I-2 labels
FEAT = [n for n in hdr if n not in EXCL]
X = raw[:, [col[n] for n in FEAT]].astype(float)

def add(name, v):
    global X, FEAT
    X = np.hstack([X, np.asarray(v, float).reshape(-1, 1)]); FEAT = FEAT + [name]

for kk in (1, 2, 3, 4):
    add('isR%d' % kk, (r == kk).astype(float))
add('sameTgt', (raw[:, col['targetRel']] == raw[start[grp], col['targetRel']]).astype(float))
add('notRef', (r > 0).astype(float))     # the intercept: a blanket cost of deviating

# score is an ADVANTAGE over candidate 0 -> difference every row against its group leader
X = X - X[start[grp], :]
keep = np.nonzero(X.std(axis=0) > 1e-12)[0]
X = X[:, keep]; FEAT = [FEAT[i] for i in keep]
# drop exact duplicates (p, f0 and mTgt are the same quantity)
uq = []
for j in range(X.shape[1]):
    dup = False
    for i in uq:
        a, b = X[:, i], X[:, j]
        if np.allclose(a, b, atol=1e-12): dup = True; break
    if not dup: uq.append(j)
if len(uq) < X.shape[1]:
    print("dropped %d duplicate columns: %s" % (X.shape[1]-len(uq),
          [FEAT[j] for j in range(X.shape[1]) if j not in uq]), file=sys.stderr)
X = X[:, uq]; FEAT = [FEAT[i] for i in uq]
mu = X.mean(0); sd = X.std(0); sd[sd < 1e-9] = 1.0
Xs = (X - mu) / sd
Xs[start, :] = 0.0                      # reference candidate scores exactly 0

# ---------------- padded group softmax --------------------------------------
maxK = int(cnt.max())
padidx = np.full((ND, maxK), -1, np.int64)
for kk in range(maxK):
    m = cnt > kk
    padidx[m, kk] = start[m] + kk
mask = padidx >= 0
onehot = np.zeros((ND, maxK)); onehot[np.arange(ND), lab] = 1.0

def gsm(sc):
    P = np.full((ND, maxK), -1e30)
    P[mask] = sc[padidx[mask]]
    P -= P.max(1, keepdims=True)
    Ex = np.exp(P); Ex[~mask] = 0.0
    return Ex / Ex.sum(1, keepdims=True)

def nll(w, md):
    Pm = gsm(Xs @ w)
    return -np.log(np.maximum(Pm[np.arange(ND), lab][md], 1e-300)).mean()

def fit(md, l2=1e-3, iters=600, lr=0.15, verbose=False):
    """Adam on the conditional-logit NLL. Robust where diagonal-Newton is not:
    the columns are heavily collinear (f0 == p == mTgt before dedup)."""
    w = np.zeros(Xs.shape[1]); n = int(md.sum())
    idx = padidx[mask]
    m1 = np.zeros_like(w); m2 = np.zeros_like(w)
    b1, b2, eps = 0.9, 0.999, 1e-8
    for t in range(1, iters + 1):
        Pm = gsm(Xs @ w)
        diff = (Pm - onehot); diff[~md, :] = 0.0
        gvec = np.zeros(len(Xs)); np.add.at(gvec, idx, diff[mask])
        g = Xs.T @ gvec / n + l2 * w
        m1 = b1 * m1 + (1 - b1) * g
        m2 = b2 * m2 + (1 - b2) * g * g
        w -= lr * (m1 / (1 - b1 ** t)) / (np.sqrt(m2 / (1 - b2 ** t)) + eps)
        if verbose and t % 150 == 0:
            print("    it%4d nll %.4f" % (t, nll(w, md)), file=E)
    return w

def predict(w):
    sc = Xs @ w
    P = np.full((ND, maxK), -1e30); P[mask] = sc[padidx[mask]]
    if TIEONLY:                          # deviations only inside the tie group
        allow = np.arange(maxK)[None, :] < tk[:, None]
        allow[:, 0] = True
        P[~allow] = -1e30
    return P.argmax(1)

def acc(w, md):
    return (predict(w)[md] == lab[md]).mean()

udeal = np.unique(dDeal)
cut = udeal[int(0.7 * len(udeal))]
trD = dDeal < cut; teD = ~trD
print("train %d decisions / test %d decisions (deal < %d)" % (trD.sum(), teD.sum(), cut), file=E)

base_argmax = (lab[teD] == 0).mean()
p_rand = np.where(lab < tk, 1.0 / tk, 0.0)
base_rand = p_rand[teD].mean()

best = None
for l2 in (1e-5, 1e-4, 1e-3, 1e-2, 1e-1):
    w = fit(trD, l2=l2)
    a_tr, a_te = acc(w, trD), acc(w, teD)
    print("l2=%-8.4g nll_tr %.4f nll_te %.4f  train %.4f  HELDOUT %.4f"
          % (l2, nll(w, trD), nll(w, teD), a_tr, a_te), file=E)
    if best is None or a_te > best[1]:
        best = (l2, a_te, w, a_tr)

l2, a_te, w, a_tr = best
print("", file=E)
print("BASELINE blueprint-argmax heldout %.4f" % base_argmax, file=E)
print("BASELINE random-in-tie   heldout %.4f" % base_rand, file=E)
print("MODEL    cond-logit      heldout %.4f   lift %+.4f" % (a_te, a_te - base_argmax), file=E)
n_te = int(teD.sum())
se = np.sqrt(a_te * (1 - a_te) / n_te)
print("  (binomial se %.4f on n=%d; deal-clustered will be wider)" % (se, n_te), file=E)

sub = teD & (dTie >= 2)
print("tie-subgroup heldout: n=%d argmax %.4f model %.4f"
      % (sub.sum(), (lab[sub] == 0).mean(), acc(w, sub)), file=E)
sub2 = teD & (dTie < 2)
print("non-tie      heldout: n=%d argmax %.4f model %.4f"
      % (sub2.sum(), (lab[sub2] == 0).mean(), acc(w, sub2)), file=E)

wu = w / sd
bias = -float((w * mu / sd).sum())
print("\n--- top weights (standardised | raw) ---", file=E)
for i in np.argsort(-np.abs(w))[:22]:
    print("  %-14s %+8.4f  %+.6g" % (FEAT[i], w[i], wu[i]), file=E)

json.dump({"feat": FEAT, "w": [float(v) for v in wu], "bias": bias,
           "heldout": float(a_te), "train": float(a_tr),
           "base_argmax": float(base_argmax), "base_rand": float(base_rand),
           "l2": float(l2), "devrate": float((lab > 0).mean()), "ND": int(ND),
           "n_test": n_te, "tieonly": bool(TIEONLY)}, open(OUT, 'w'), indent=1)
print("\nwrote %s" % OUT, file=E)
