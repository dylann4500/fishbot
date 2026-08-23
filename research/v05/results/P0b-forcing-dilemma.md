# The forcing dilemma in v0.4 (run by the lead session, seed 4242 / 31)

v0.4 terminates deadlocks with a graduated forcing rule keyed on `pub.nEvents`
(`V04Agent::pressure`, forceDeclareEvents = 220). Raising that horizon so the rule
never fires is worth **+6.2 win-rate points against the shipped configuration**:

```
$ ./fish match --a=v04:force=2000 --b=v04 --games=250 --rotations=6 --seed=4242
  win rate      56.2%  [53.6765, 58.6918]  n=1500
  declarations  4.16267/game at 98.3985%   opp 4.79667/game at 85.6567%
  lock hold     7.67513 / 6.72984 events before cashing
  events/game   133.697   limit hits 0%
  cluster boot  56.2% [54.9333, 57.4667]
```

Identical numbers at force=400, 800 and 2000: no mirror game reaches event 400, so
force=400 already disables the rule. The patient side declares at 98.4% accuracy against
the forcing side's 85.7%.

## But pure patience deadlocks

```
$ ./fish pathology --a=v04:force=2000 --b=v04:force=2000 --games=120 --rotations=2 --seed=31
v04:force=2000 vs v04:force=2000
games              240
events/game        168.583   median 103  p90 404  p99 406  max 406
asks/game          160.567   hit rate 27.4289%
DEAD asks          19716  (51.1625% of asks)   -- actor could PROVE the target lacks the card
dead runs          930  mean length 21.2  longest 379
games w/ run>=6    78  (32.5%)
starved turns      346  (0.897862% of asks)   -- NO legal ask had a live possibility
repeat (a,c,t)     20044  (52.0137%)
repeat (a,suit,t)  29238  (75.8719%)
asks in own-locked 5236  (13.5873% of asks)   -- guaranteed miss, but emits a certificate
declarations       1864   wrong 40 (2.14592%)
  at/after ev>=220 0   wrong 0 (0%)
  forced endgame   16   wrong 16 (100%)
action-limit games 54 (22.5%)
```

## The dichotomy

| | forcing (shipped) | patient (force=2000) |
|---|---|---|
| declarations wrong | 10.4% | **2.1%** |
| games ending by action-cap adjudication | 0% | **22.5%** |
| provably dead asks | 39.0% | **51.2%** |
| longest dead run | 286 | 379 |

v0.4 has only two settings, and both are bad: misdeclare, or never finish. The project
owner named the third option — keep asking, but ask productively. Note that in the
patient mirror **51% of asks are already provably dead**: the turns are being burned
regardless. They are simply not being spent on asks that emit useful certificates.
