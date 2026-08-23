# P4 adversarial verification — `declareByValue` wrong-branch card delta

Verifier: independent re-derivation and re-run. Engine at working tree, `cd engine && make`.
New scratch code: `engine/src/probe_declcard.hpp`, CLI `fish declcard` (appended block in
`engine/src/main.cpp`). Nothing in the protected headers was modified.

## Claim under review

> `declareByValue`'s wrong-declaration branch passes `dOur=-SETSZ, dTheir=0`, but 83.9% of wrong
> declarations are cases where an opponent held one or more of the six cards, so fewer than six
> leave our hands and the rest leave the opponents'. True deltas average (-4.21, -1.79); the
> resulting value error is 0.0280 = 82% of `|declareMargin| = 0.0342`.

## Verdict: HOLDS UP, magnitude overstated ~2.2x

## 1. The code defect is real

`engine/src/v04.hpp:664-667`

```cpp
int mine = __builtin_popcountll(k.myHand & setMask(S));
double vRight = value(pub, dC, dS, dL, dK, scoreDiff + 1, turnSign, -SETSZ + 0, 0, dUnres, -1);
double vWrong = value(pub, dC, dS, dL, dK, scoreDiff - 1, turnSign, -SETSZ + 0, 0, dUnres, -1);
(void)mine;
```

Argument 8 and 9 of `value` are `int dOur, int dTheir` (`v04.hpp:370-372`), and they enter only
through the card-differential feature `f[6] = (ourCards + dOur - theirCards - dTheir) / 54.0`
(`v04.hpp:386`), where `ourCards`/`theirCards` are **team** hand totals (`v04.hpp:345-348`). So
both branches model "six cards leave our team, zero leave theirs". That is correct for `vRight`
(a correct declaration implies the team held all six) and wrong for `vWrong` whenever an opponent
held any of the six. `mine` is computed and discarded — a leftover, not a documented choice:
`grep -rn "declareByValue\|dOur\|card differential" docs/*.md research/v04/results/*.md` returns
nothing, and `docs/FISHBOT_V04.md` "Known gaps" does not list it.

## 2. The ground-truth statistic reproduces, and survives seed changes

`fish p4horizon` (`engine/src/probe_policy.hpp:230-250`) replays true hands forward through the
event trace and counts, per wrong **voluntary** declaration (`Kind::Declare`; forced declarations
are `Kind::ForcedDeclare` and excluded — correct scope, since `declareByValue` only governs
voluntary ones), how many of the six the declaring team actually held.

| seed | games | wrong decls | opp held >=1 | share | mean team-held |
|---|---|---|---|---|---|
| 31 (cited) | 200 | 199 | 167 | **83.9%** | **4.21** |
| 90210 | 200 | 166 | 140 | 84.3% | 4.25 |
| 777 | 150 | 144 | 125 | 86.8% | 4.02 |
| 4242 | 150 | 99 | 85 | 85.9% | 4.03 |
| 12345 | 150 | 127 | 103 | 81.1% | 4.19 |

Cited numbers reproduce exactly at seed 31, and the effect is stable at 81–87% / 4.0–4.3 across
four other seeds. The arithmetic also checks: `(6 - 2.42)/54 * vw[6]=0.422207 = 0.02799`,
confirmed as the measured mean `|dvWrong|` below.

## 3. Where the claim overstates: the error enters `vDeclare` weighted by `(1 - pAlloc)`

`vDeclare = pAlloc*vRight + (1-pAlloc)*vWrong` (`v04.hpp:668`). The decision compares `vDeclare`
against `vWait + declareMargin`, so the decision-relevant error is `(1-pAlloc)*|dvWrong|`, not
`|dvWrong|`. Comparing 0.0280 directly against `|declareMargin|` skips that factor.

`engine/src/probe_declcard.hpp` subclasses the shipped `V04Agent` and replays
`v04.hpp:689-749` verbatim (minus the default-off `gateAudit` branch), evaluating each candidate
half-suit twice. Faithfulness check at `--games=150 --seed=31 --mode=0`: 1341 voluntary
declarations, 1209 correct — identical to `fish p4horizon --games=150 --seed=31`
(1160 pre + 181 post = 1341; 19 + 113 = 132 wrong). The replica is bit-faithful.

`./fish declcard --games=150 --seed=31 --mode=2` (wrong branch given the empirical (4.21, 1.79)):

```
declareByValue calls 3582
shipped declare 823 (22.98%)   corrected declare 888 (24.79%)
flips wait->declare 65 (1.815%)   declare->wait 0 (0.000%)
|dvWrong|  mean 0.02799  max 0.02799     (|declareMargin| = 0.03420)  -> 82% of margin
|dvDeclare| = (1-pAlloc)*|dvWrong|  mean 0.01275  max 0.02799         -> 37% of margin
```

Seed 90210, 120 games: mean `|dvDeclare|` 0.01121, flips 1.667%, again all one-directional.

**Corrected magnitude: the wrong branch is mis-valued by 0.0280 (82% of the margin), but the
resulting error in the quantity actually compared is 0.0128 on average — 37% of
`|declareMargin|`, reaching the full 0.0280 only as `pAlloc -> 0`.** It flips 1.8% of
value-branch declaration verdicts.

## 4. A second caveat the original finding does not state: 4.21 is an oracle number

The bot cannot compute (4.21, 1.79) at decision time. The best in-engine correction is
`E[our count | allocation wrong] = (E[count] - 6*pAlloc) / (1 - pAlloc)` — an identity, since a
correct allocation implies all six are ours — evaluated on the agent's own posterior
(`bel.marg`). `--mode=3` does exactly that:

| seed | games | mean E[our|wrong] from the bot's posterior | mean \|dvWrong\| | mean \|dvDeclare\| | flips |
|---|---|---|---|---|---|
| 31 | 150 | 5.766 | 0.00366 | 0.00164 (4.8% of margin) | 0.384% |
| 90210 | 120 | 5.726 | 0.00428 | 0.00193 (5.6% of margin) | 0.592% |

The bot's posterior says wrong declarations still leave ~5.75 of the six on our side; ground
truth says 4.21. **The 1.5-card gap is a posterior-calibration failure, not something the card-
delta fix can reach.** A correct implementation of the card delta recovers only ~5% of the
margin, not 82%.

## 5. Direction of the fix

Across every seed and both correction modes, **all** flips are `wait -> declare`, never
`declare -> wait`; they concentrate in the pAlloc 0.6–0.8 buckets, where `fish p4horizon`
measures wrong rates of 10.7%–53.9%. Fixing this defect therefore makes v0.4 declare *more*
often, in exactly the confidence band where it is already unreliable. It is a correctness fix,
but on its own it is not a fix for the misdeclaration pathology in `P0-v04-pathology.md`, and
should not be shipped without measuring the misdeclaration rate after the change.

## Commands

```
cd engine && make
./fish p4horizon --games=200 --seed=31
./fish p4horizon --games=200 --seed=90210
./fish p4horizon --games=150 --seed=777      # also 4242, 12345
./fish declcard  --games=150 --seed=31 --mode=0   # posterior mean split
./fish declcard  --games=150 --seed=31 --mode=2   # empirical (4.21, 1.79)
./fish declcard  --games=150 --seed=31 --mode=3   # E[count | wrong] from own posterior
./fish declcard  --games=120 --seed=90210 --mode=2
```
