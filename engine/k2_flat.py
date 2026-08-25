#!/usr/bin/env python3
"""K2 -- the ceiling table: where the residual declaration error actually lives."""
import csv, math, sys

P = sys.argv[1]
rows = [r for r in csv.DictReader(open(P))]
d = [r for r in rows if int(r['l1have']) & 2]


def wil(k, n):
    if n == 0:
        return (0, 0, 0)
    p = k / n
    z = 1.96
    den = 1 + z * z / n
    c = (p + z * z / (2 * n)) / den
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / den
    return p, c - h, c + h


N = len(d)
print("file:", P)
print("declarations with exact shape:", N)
print()
print("A. SHIPPED accuracy inside FLAT exact-posterior states, by support size")
tot_flat = 0
tot_flat_hit = 0
for na in sorted({float(r['l1nAlloc']) for r in d if r['l1flat'] == '1'}):
    sub = [r for r in d if r['l1flat'] == '1' and abs(float(r['l1nAlloc']) - na) < 1e-6]
    if na < 2 or len(sub) < 5:
        continue
    k = sum(1 for r in sub if r['hit'] == '1')
    p, lo, hi = wil(k, len(sub))
    tot_flat += len(sub)
    tot_flat_hit += k
    print("   nAlloc=%-3d n=%5d (%.3f%% of decls)  shipped %.4f [%.4f,%.4f]  exact coin 1/n=%.4f"
          % (na, len(sub), 100 * len(sub) / N, p, lo, hi, 1.0 / na))
p, lo, hi = wil(tot_flat_hit, tot_flat)
print("   ALL FLAT>=2  n=%5d (%.3f%% of decls)  shipped %.4f [%.4f,%.4f]"
      % (tot_flat, 100 * tot_flat / N, p, lo, hi))
print("   -> headroom if a POLICY-PRIOR rule made flat states perfect: %.4f pp of decl accuracy = %.3f win pts"
      % (100 * (tot_flat - tot_flat_hit) / N, 1.2 * 100 * (tot_flat - tot_flat_hit) / N))
print()
print("B. NON-FLAT states with >=2 allocations")
sub = [r for r in d if r['l1flat'] == '0' and float(r['l1nAlloc']) >= 2]
k = sum(1 for r in sub if r['hit'] == '1')
p, lo, hi = wil(k, len(sub))
ke = sum(1 for r in sub if r['exactHit'] == '1')
p2, lo2, hi2 = wil(ke, len(sub))
mp = sum(float(r['l1pMap']) for r in sub) / len(sub)
print("   n=%5d (%.3f%% of decls)  shipped %.4f [%.4f,%.4f]" % (len(sub), 100 * len(sub) / N, p, lo, hi))
print("   exact MAP realized %.4f [%.4f,%.4f];  exact posterior's OWN optimum E[pMap]=%.4f"
      % (p2, lo2, hi2, mp))
print("   -> headroom if a rule made NON-FLAT states perfect: %.4f pp = %.3f win pts"
      % (100 * (len(sub) - k) / N, 1.2 * 100 * (len(sub) - k) / N))
print()
sub = [r for r in d if float(r['l1nAlloc']) < 2]
k = sum(1 for r in sub if r['hit'] == '1')
print("C. UNAMBIGUOUS (exactly one feasible allocation) n=%d (%.2f%%)  shipped %.5f"
      % (len(sub), 100 * len(sub) / N, k / len(sub)))
print()
print("D. exact-MAP paired McNemar over all declarations")
fx = sum(1 for r in d if r['hit'] == '0' and r['exactHit'] == '1')
bk = sum(1 for r in d if r['hit'] == '1' and r['exactHit'] == '0')
print("   +%d fixed  -%d broken  net %+d on %d = %+.4f pp = %+.3f win pts"
      % (fx, bk, fx - bk, N, 100 * (fx - bk) / N, 1.2 * 100 * (fx - bk) / N))
print()
print("E. the joint rule")
amb = [r for r in d if int(r['l1n']) >= 2]
strict = sum(1 for r in amb if float(r['l1jTop']) > float(r['l1jSecond']))
print("   ambiguous by the shipped enumerator: %d (%.2f%% of decls)" % (len(amb), 100 * len(amb) / N))
print("   joint argmax differs from marginal-product argmax: %d"
      % sum(1 for r in amb if r['l1jSame'] == '0'))
print("   of those, slot-0 won STRICTLY on joint score (not by the tie rule): %d (%.1f%%)"
      % (strict, 100 * strict / len(amb)))
