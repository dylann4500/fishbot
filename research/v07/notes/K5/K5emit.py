"""Turn the fitted ridge into the engine's sparse `lw=` weight string.

Engine score:  a = bias + sum_j w_raw[j] * (row_r[j] - row_0[j])
Fit score:     pred_r = sum_j w[j]*(Xdiff_r[j]-mu[j])/sd[j] + w_intercept

so w_raw[j] = w[j]/sd[j] and bias = w_intercept - sum_j w[j]*mu[j]/sd[j].
"""
import json, sys
J = json.load(open(sys.argv[1]))
IDX = {'r':0,'isTie':1,'du':2,'p':3,
       'mOwn':24,'mMate1':25,'mMate2':26,'mTgt':27,'mOpp1':28,'mOpp2':29,
       'targetRel':30,'tgtHand':31,'setUnres':32,'myInSet':33,
       'unres':36,'nEvents':37,'myCards':38,'ourCards':39,'theirCards':40,'lead':41,
       'n':42,'tie':43,'K':44,'KC':45,'D':46,
       'isR1':51,'isR2':52,'isR3':53,'isR4':54,'sameTgt':55}
for i in range(20): IDX['f%d'%i] = 4+i

feat, w, mu, sd = J['feat'], J['w'], J['mu'], J['sd']
bias = 0.0; parts = []
skipped = []
for i, nm in enumerate(feat):
    if nm == '_intercept':
        bias += w[i]; continue
    if i >= len(sd):            # quadratic terms have no engine counterpart
        if abs(w[i]) > 1e-12: skipped.append(nm)
        continue
    wr = w[i]/sd[i]
    bias -= w[i]*mu[i]/sd[i]
    if nm == 'notRef':
        # identical to the appended intercept (both are 1 on every non-reference
        # candidate, 0 on the reference); ridge splits the weight between them.
        bias += wr; continue
    if nm not in IDX:
        if abs(wr) > 1e-12: skipped.append(nm)
        continue
    if abs(wr) < 1e-9: continue
    parts.append("%d:%.10g" % (IDX[nm], wr))
if skipped:
    print("WARNING, weights with no engine column: %s" % skipped, file=sys.stderr)
print("%.10g|%s" % (bias, "|".join(parts)))
