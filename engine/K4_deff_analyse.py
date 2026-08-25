#!/usr/bin/env python3
"""K4 -- design effect of the per-decision estimators.

Ledger L5 says scoring a mechanism on DECISIONS rather than GAMES "buys roughly
the ratio of decisions to games in effective sample".  That ratio is the truth
only if decisions are independent.  They are not: the ~85 asks and ~4.5
declarations of one deal share a deal.  The design effect

    DEFF = Var_empirical(rate across independent blocks)
         / Var_binomial(rate)          where Var_binomial = p(1-p)/nDecisions

is the factor by which the naive decision count over-states the effective
sample.  Effective decisions per game = decisionsPerGame / DEFF, and L5's
promise survives only if that is comfortably above 1.

Everything is then converted into ONE currency -- games needed to resolve an
effect worth 1.0 win-rate point -- using the ledger's own SS0.2 conversion
(1 percentage point of declaration accuracy ~ 1.2 points of win rate).
"""
import json, math, sys, statistics as st

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    "/Users/dylan/Documents/GitHub/fish optimization/research/v07/results/K4-designeffect.jsonl"

rows = [json.loads(l) for l in open(PATH)]
arms = {}
for r in rows:
    arms.setdefault(r["arm"], []).append(r["r"])

# (label, numerator-rate field, denominator-count field, decisions-per-game field)
METRICS = [
    ("askAcc",   "askAccA",       "nAsksA", "asksPerGameA"),
    ("declAcc",  "declAccA",      "nDeclA", "declPerGameA"),
    ("allocErr", "allocErrRateA", "nDeclA", "declPerGameA"),
]

print(f"{'arm':<14} {'metric':<9} {'p':>9} {'n/blk':>8} {'dec/game':>9} "
      f"{'SDemp':>9} {'SDbin':>9} {'DEFF':>7} {'effDec/game':>12}")
out = {}
for arm, rs in arms.items():
    K = len(rs)
    nblk = st.mean(r["games"] for r in rs)
    wr = [100 * r["winRateA"] - 50 for r in rs]
    sd_w = st.stdev(wr)
    # a per-game Bernoulli's binomial SD on nblk games, in points
    sdbin_w = 100 * math.sqrt(0.25 / nblk)
    deff_w = (sd_w / sdbin_w) ** 2
    out[(arm, "winRate")] = dict(p=st.mean(r["winRateA"] for r in rs), n=nblk,
                                 perGame=1.0, sdemp=sd_w / 100, sdbin=sdbin_w / 100,
                                 deff=deff_w)
    eff = (1.0 / deff_w) if deff_w > 0 else float("inf")
    tag = "  (MIRROR: exchangeable, no information)" if sd_w == 0 else ""
    print(f"{arm:<14} {'winRate':<9} {st.mean(r['winRateA'] for r in rs):9.5f} "
          f"{nblk:8.0f} {1.0:9.3f} {sd_w/100:9.5f} {sdbin_w/100:9.5f} "
          f"{deff_w:7.2f} {eff:12.3f}{tag}")
    for label, rf, nf, pgf in METRICS:
        vals = [r[rf] for r in rs]
        n = st.mean(r[nf] for r in rs)
        pg = st.mean(r[pgf] for r in rs)
        p = st.mean(vals)
        sde = st.stdev(vals)
        sdb = math.sqrt(max(p * (1 - p), 1e-12) / n)
        deff = (sde / sdb) ** 2
        out[(arm, label)] = dict(p=p, n=n, perGame=pg, sdemp=sde, sdbin=sdb, deff=deff)
        print(f"{arm:<14} {label:<9} {p:9.5f} {n:8.0f} {pg:9.3f} {sde:9.5f} "
              f"{sdb:9.5f} {deff:7.2f} {pg/deff:12.3f}")

print()
print("--- games needed to resolve a 1.0-win-rate-point-equivalent effect, 2 sigma ---")
print("(conversion, LEDGER SS0.2: 1 pp of declaration accuracy ~ 1.2 pts of win rate;")
print(" so a 1-point effect is 0.833 pp of declaration accuracy.)")
for arm in arms:
    d = out[(arm, "winRate")]
    # win rate: need SE = 0.5 pt -> N from 98/sqrt(N) = 1.0 pt (1.96 sigma half width)
    print(f"  {arm:<14} winRate   : {9604:>9,d} games   (98/sqrt(N) = 1.0 pt)")
    for label, delta_pp in (("declAcc", 0.00833),):
        e = out[(arm, label)]
        # SE(rate) at G games = sqrt(p(1-p) * DEFF / (perGame*G)); want 2*SE = delta
        need = e["p"] * (1 - e["p"]) * e["deff"] / (e["perGame"] * (delta_pp / 2) ** 2)
        print(f"  {arm:<14} {label:<10}: {need:>9,.0f} games   "
              f"(DEFF {e['deff']:.2f}, {e['perGame']:.2f} decisions/game)")
    e = out[(arm, "allocErr")]
    # the allocation-error CLASS.  LEDGER SS0.2 gives the conversion directly:
    # 1 point of win rate ~ 0.034 avoided misdeclarations PER GAME.  As a rate
    # per declaration that is 0.034 / declPerGame.
    delta_alloc = 0.034 / e["perGame"]
    need = e["p"] * (1 - e["p"]) * e["deff"] / (e["perGame"] * (delta_alloc / 2) ** 2)
    print(f"  {arm:<14} {'allocErr':<10}: {need:>9,.0f} games   "
          f"(DEFF {e['deff']:.2f}, rate {e['p']:.5f} per declaration)")
    e = out[(arm, "askAcc")]
    print(f"  {arm:<14} {'askAcc':<10}: no established conversion to win rate "
          f"(DEFF {e['deff']:.2f}, {e['perGame']:.1f} decisions/game, "
          f"eff {e['perGame']/e['deff']:.2f}/game)")
