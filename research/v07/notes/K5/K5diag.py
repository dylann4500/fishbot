"""K5 zero-cost diagnostics on the captured search decisions.

(a) Is the search's own choice noise-dominated?  The capture carries the LCB
    rule's inputs: the paired mean advantage m, its standard error se, and the
    LCB itself.  If the winning LCB beats the runner-up by much less than the
    combined standard error, the search is choosing inside its own Monte-Carlo
    noise and NO function of decision-time state can predict it, however large.

(b) How much of the decision mass is the tie group, and how separable is it?
"""
import numpy as np, sys

PATH = sys.argv[1]
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

lcb = raw[:, c['lcb']]; se = raw[:, c['se']]; m = raw[:, c['m']]
tie = raw[start, c['tie']].astype(int); K = cnt

maxK = int(cnt.max())
padi = np.full((ND, maxK), -1, np.int64)
for kk in range(maxK):
    msk = cnt > kk; padi[msk, kk] = start[msk] + kk
valid = padi >= 0
L = np.full((ND, maxK), -1e30); L[valid] = lcb[padi[valid]]
S = np.zeros((ND, maxK)); S[valid] = se[padi[valid]]

win = lab
Lw = L[np.arange(ND), win]; Sw = S[np.arange(ND), win]
L2 = L.copy(); L2[np.arange(ND), win] = -1e30
run = L2.argmax(1); Lr = L2[np.arange(ND), run]; Sr = S[np.arange(ND), run]

gap = Lw - Lr
comb = np.sqrt(Sw**2 + Sr**2)
ok = np.isfinite(gap) & (comb > 0) & (comb < 1e8)
z = gap[ok] / comb[ok]

E = sys.stdout
print("decisions %d, of which usable for the z statistic %d" % (ND, ok.sum()), file=E)
print("LCB margin over runner-up, in units of its own combined standard error:", file=E)
for q in (10, 25, 50, 75, 90):
    print("   p%-3d  z = %+.3f" % (q, np.percentile(z, q)), file=E)
print("   mean  z = %+.3f" % z.mean(), file=E)
for t in (0.25, 0.5, 1.0, 2.0):
    print("   share of decisions whose winning margin is under %.2f se: %.4f"
          % (t, (z < t).mean()), file=E)

print("", file=E)
print("Same statistic restricted to decisions the search DEVIATED on (r>0):", file=E)
dev = ok & (lab > 0)
zd = gap[dev] / comb[dev]
print("   n=%d  median z %+.3f   share under 1 se %.4f" % (dev.sum(), np.median(zd), (zd < 1).mean()), file=E)
print("Restricted to decisions with a bit-for-bit tie group (tie>=2):", file=E)
tg = ok & (tie >= 2)
zt = gap[tg] / comb[tg]
print("   n=%d  median z %+.3f   share under 1 se %.4f" % (tg.sum(), np.median(zt), (zt < 1).mean()), file=E)

# The paired-mean advantage of the chosen candidate: how big is the SIGNAL the
# search is acting on, in half-suits?
mw = np.zeros(ND); mw = m[padi[np.arange(ND), win]]
print("", file=E)
print("paired mean advantage m of the CHOSEN candidate (sets of differential):", file=E)
print("   mean %+.4f   median %+.4f   share > 0: %.4f" % (mw.mean(), np.median(mw), (mw > 0).mean()), file=E)
sew = S[np.arange(ND), win]
fin = sew < 1e8
print("   its own se: mean %.4f -> the search acts on a signal that is %.2f se in size"
      % (sew[fin].mean(), mw[fin].mean() / max(1e-9, sew[fin].mean())), file=E)
