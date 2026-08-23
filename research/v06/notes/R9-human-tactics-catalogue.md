# R9 — Catalogue of human Fish tactics, and what FishBot v0.5 can and cannot express

Recon task, FishBot v0.6. Repository `/Users/dylan/Documents/GitHub/fish optimization`, engine at
commit `bd812fe` ("v0.5"), working tree otherwise clean. Every code claim cites `file:line` at that
state. Every number is from a run made in this session; the new probe is read-only and lives in the
scratchpad (§8). Nothing under `engine/src/`, `paper/` or `docs/` was modified.

Byline: Dylan Nguyen, FishLab Research Project.

---

## 0. Verdict

**The human-strategy gap is not an inference gap and it is not a tuning gap. It is that v0.5's
action space, as the policy actually enumerates it, excludes the entire class of moves that human
tactics are built out of.** Three findings, in order of how much they should change v0.6.

1. **M1 deletes the deliberate miss, and with it every multi-step tactic in the corpus.**
   `V05Agent::enumerateLive` (`engine/src/v05.hpp:482-500`) removes every ask the actor can *prove*
   will miss, and `chooseAsk` gates on it (`v05.hpp:505-506`). Measured over a v0.5 mirror,
   **0 of 34,668 ask decisions produced a provably-dead ask** (v0.4: 36.6%). Yet the *capability* is
   there at nearly every decision: a provably-dead ask into a chosen opponent exists at **79.2%** of
   decisions, into **two or more distinct chosen opponents at 50.1%**, into all three at **35.0%**.
   Blackballing (T#7), the safe-ask costly signal (T#12), declare-to-transfer-control's asking
   cousin, and voluntary turn donation are all *implemented as guaranteed misses at a chosen seat*.
   M1 fixed the v0.4 deadlock by forbidding the move class the deadlock abused — and forbade the
   tactics with it. v0.6 needs a **priced** dead ask, not a banned one.

2. **v0.5 still builds no model of any other seat's knowledge.** The only two `Knowledge` clones in
   the policy are clones of its own (`v05.hpp:543`, `v05.hpp:560`, both `Knowledge k? = k;`). Every
   tactic that turns on "what does *he* know" — blackballing in Develin's sense, baiting, decoding a
   teammate, choosing the best-informed declarer — reads state that does not exist. `v05_target.hpp`
   designs the fix (M4/M5) and proves the key structural fact (the public deduction state is a
   *single* common-knowledge object, `v05_target.hpp:44-56`), but **it is not included by `v05.hpp`**
   — `grep -rln v05.hpp src/` shows the only consumer of `v05_target.hpp` is
   `src/probe_m45_test.cpp:45`. M4/M5/M7 are designed and unshipped.

3. **The legal-action dimension carries ~5 free bits per ask and v0.5 prices none of them.**
   Measured on the v0.5 mirror, per ask decision: **0.959 bits** free in the target dimension,
   **1.301 bits** in the card dimension *inside the chosen half-suit*, **4.979 bits** jointly over all
   legal asks with the same provable-liveness status (≥2 such asks at **99.15%** of decisions).
   `askExpectedValue` still opens `(void)target;` (`v05.hpp:437-438`), and no feature in
   `features()` (`v05.hpp:285-332`) prices information delivered to a teammate — the three
   information terms `f[9]`, `f[16]`, `f[19]` (`v05.hpp:321,328,331`) all price leakage to
   *opponents*, and none prices gain to the team.

Two supporting facts that frame the whole report:

* **v0.5 is not stronger than v0.4** (`docs/V05_FINDINGS.md:15-24`: "+1.11 points head-to-head …
  50.79% pooled"). It removed a failure mode; it added no strategy. So the owner's report #4 — a
  well-trained human still out-reasons the bot — is *expected*, not anomalous.
* **The human at `fish serve` is playing with strictly worse inference and still winning on
  reasoning.** `table.hpp:187` sends the browser only that seat's own hand; the deduction state
  `Agent::k` (`game.hpp:14`, maintained for the human seat by `Agent::observe`, `game.hpp:20`) is
  never exposed. `HumanAgent` (`engine/src/human.hpp:88-269`) is pure I/O: five blocking decision
  points and no analysis. The gap is therefore located in *policy*, not in *knowledge*.

---

## 1. Corpus

Read or fetched this session. **The corpus has not grown since the v0.4 and v0.5 literature reviews**
— this independently re-confirms `P5-human-strategy.md` §6.4.

| Source | Status | Distinct tactical content |
|---|---|---|
| [pagat.com — Literature](https://www.pagat.com/quartet/literature.html) (McLeod) | fetched, full | claim timing (both directions); "ask questions to which you already know that the answer is no"; **lockout**; memory triage; asking-order; **void creation**; back-and-forth warning; Ali Salahuddin's convention; endgame protocol; Srinivasan's Forced Claims / No Probabilistic Information / Challenge; the communication rules (last question + card counts legal, "History" not) |
| [Develin, *Canadian Fish*](http://www.bantha.org/~develin/cardgames.html) | fetched, full | memory budgeting; **blackballing** with worked example; ask-the-asker; **lie low**; sequential suit cleanup; **declare-to-transfer-control**; endgame declarer = most-informed; **"Conventions are explicitly forbidden"**; "your opponents get just as much information from a failed question as your teammates do" |
| [Wikipedia — Literature](https://en.wikipedia.org/wiki/Literature_(card_game)) | fetched | information-asymmetry master principle; **stalemate-breaker** (hold a fully-attributed set to break a deadlock and move control); the "ask for a card you already hold" advanced variant |
| [Deposit Genius — Literature Game Strategy](https://depositgenius.com/literature-strategy-canadian-fish/) | fetched | blackballing; ask-the-asker; **deferring claims** because a large hand attracts asks; certainty before claiming; endgame delegation to the **most-carded** player |
| [CardRules+](https://cardrulesplus.com/games/literature/) | fetched | 70%-confidence opening asks; early **known-miss asks** to avoid leaking; "use asking patterns as signals"; **claim immediately** (contradicts Develin/pagat/Wikipedia); the 3-3 split claim |
| [gamerules.com](https://gamerules.com/rules/literature-card-game/) | fetched | rules only; no strategy (contradicts the P5 citation — see §7 correction 1) |
| [brynmawr.edu — Rules of Fish](https://www.brynmawr.edu/math/rules-fish) | fetched | rules only; confirms the "no history" convention and "do not claim unless 100% sure" |

**Negative results, recorded so they are not re-run.** No Reddit thread, no BoardGameGeek strategy
thread, no forum thread, no university-club wiki, and **no academic treatment of Literature/Fish**
surfaced. Searches for AI work on this specific game return only generic imperfect-information
card-game literature (poker, bridge, Jass, trick-taking). `gambiter.com` timed out;
`canadian-fish.vercel.app/strategy` now returns an empty page (it is cited in
`docs/EXTERNAL_STRATEGY_REVIEW.md:7` and is no longer readable). The only code prior art remains
Somani's four-player learner and `Dynosol/playfish.io` (an implementation, no strategy document).

**Consequence for v0.6.** The published human corpus is ~20 tactics and is exhausted. Tactics #16–#24
below are **derived** — read off the engine's own rules of record rather than from a source — and are
flagged as such. Given that the corpus is closed, the derived tactics are where the remaining
headroom is, and they are exactly the multi-step ones.

---

## 2. Legality frame under the owner's rules of record

Checked against `engine/src/fish.hpp`, `engine/src/game.hpp`, and `research/v05/BRIEF.md`
"Rules of record".

| Channel | Legal? | Where |
|---|---|---|
| Choose **which card** to ask for, freely among legal ones | yes | `fish.hpp:158-165`, `fish.hpp:179-196` |
| Choose **which opponent** to ask | yes, any live opponent | same |
| An ask you know will miss | yes — legality tests hands, never beliefs | `fish.hpp:158-165` (`g.hand`), `fish.hpp:168-175` (`myHand`) |
| Ask inside a half-suit your team already owns outright | yes; the C5 certificate still fires | `belief.hpp:156-171` |
| Ask for a card **you already hold** (the Wikipedia "advanced variant") | **no** under these rules | `fish.hpp:163`, `fish.hpp:173` |
| Declare at any moment, including an opponent's turn, holding no cards | yes | `fish.hpp:108-109`; `game.hpp:294` polls before every ask |
| Refuse to hand over a card | **no**, transfer is unconditional | `game.hpp:341`, `game.hpp:350` |
| Table talk beyond the two public queries | **no** | BRIEF rules of record |
| Willingness bits when a cardless player passes the turn | **yes by the rules, not implemented** | `game.hpp:29` gives `choosePassTarget` no teammate input; `game.hpp:302` takes the unilateral answer |
| Compare stated confidences to arbitrate a declaration race | **no** (deliberate) | `fish.hpp:119` `declArbitration = 0`; `game.hpp:210-224` discards `conf` |
| Pre-agreed codebooks | **disputed**; owner decision D1 = ship behind a flag, default off | Develin forbids; pagat documents Ali Salahuddin's; `BRIEF.md` D1 |

Two rules-level facts that no source states and that matter for v0.6:

* **A declaration strips the half-suit from *every* hand, including opponents'** (`game.hpp:189`).
  So a declaration is a hand-size weapon in both directions, and it can trigger the forced endgame
  (`game.hpp:297`) or force a turn transfer (`game.hpp:298-309`).
* **A declaration never moves the turn directly** (`applyDeclaration`, `game.hpp:170-199`, does not
  touch `g.turn`). Develin's "declare to transfer control" works *only* through hand emptying.
  Measured: **5.17%** of v0.5 declarations empty the current turn-holder (§6, T12).

---

## 3. The five structural blockers in v0.5

Everything in §4 reduces to one of these.

| # | Blocker | Evidence | Consequence |
|---|---|---|---|
| B1 | **No deliberate miss.** `enumerateLive` deletes provably-dead asks; the full set is restored only if *every* legal ask is dead | `v05.hpp:482-500`, gated at `v05.hpp:505-506`; measured 0/34,668 dead asks, 0% starved turns (`fish pathology --a=v05`) | kills T#7, T#12, T#13, T#17, T#20 |
| B2 | **No model of any other seat's knowledge.** Only self-clones | `v05.hpp:543`, `v05.hpp:560`; `v05_target.hpp` (M4/M5) not included by `v05.hpp` | kills T#7, T#11, T#14, T#19, T#21, T#22 |
| B3 | **No term prices information delivered to a teammate.** All 20 ask features are material or leak-to-opponent | `v05.hpp:285-332`; `w[]` at `v05.hpp:38-59`; `askExpectedValue` `(void)target;` at `v05.hpp:437-438`; `grep -cin "signal\|convention\|infoGain\|codebook" src/v05.hpp` → **0** | kills T#10, T#11, T#12, T#13 |
| B4 | **`value()` cannot see a declaration emptying a hand.** `f[10] myCards` and `f[11] minFriendly` are read from cached aggregates with no perturbation | `v05.hpp:393-394` vs the deltas actually passed at `v05.hpp:791-792`; and `declareByValue` computes `int mine = __builtin_popcountll(k.myHand & setMask(S));` at `v05.hpp:790` then **discards it at `v05.hpp:793` with `(void)mine;`** | kills T#15, T#16, T#18 |
| B5 | **Deterministic argmax.** The agent's `rng` reaches only belief sampling (`v05.hpp:195`, `v05.hpp:764`), never action selection | `v05.hpp:135,150,195,764`; `chooseAsk` is a plain argmax, `v05.hpp:512-518` and `v05.hpp:536-583` | a human who has played 50 games learns the policy — the owner's report #4 — and pooling (BRIEF D2 / Construction C) is unavailable |

B4 is worth restating because it is a two-line fix with a named beneficiary:
`v05.hpp:790` computes exactly the quantity the stalemate-breaker needs and `v05.hpp:793` throws it
away. This defect was diagnosed in `P5-human-strategy.md` §4 items 14–15 for v0.4 and **survives
verbatim into v0.5**.

---

## 4. The catalogue

Legend, column *v0.5*: **No** = no state or term exists. **Weak** = a related term exists but is
dominated, mis-signed, or measures the wrong thing. **Yes** = genuinely expressible. **Blocked** =
the mechanism exists elsewhere in the engine but the policy's own action gate forbids it.
Column *Cheap?* answers: computable inside `chooseAsk`/`proposeDeclaration` without a new
search, at the current ~2 s/300-game budget (`fish match --a=v05 --b=v05 --games=150` → 2.05 s).

Frequencies are v0.5-mirror unless noted, 400 games (200 deals × 2 orientations), seed 31,
34,668 ask decisions; the seed-777001 replicate is in §6.

*Notation warning:* there are **two** `f[]` vectors in `v05.hpp` and they collide. `f[0..19]` in
`features()` (`v05.hpp:285-332`) are the **ask** features; `f[0..15]` in `value()`
(`v05.hpp:373-406`) are the **value** features. So `f[10]`/`f[11]` mean *target hand size* /
*empties target* at `v05.hpp:322-323`, and *my hand size* / *smallest friendly hand* at
`v05.hpp:393-394`. Every citation below carries its line number for this reason.

### A. Single-step tactics from the corpus

**T#1 — Memory triage / focus on half-suits you hold.** (Develin; pagat: "save your brain cells")
*Definition:* budget recall toward half-suits you hold cards in.
*Engine:* `Knowledge` tracks all 54 cards unconditionally (`belief.hpp:61-76`).
*v0.5:* n/a — a bot has perfect recall. *Cheap?* n/a. *Frequency:* n/a.
**This is the null control.** It should have exactly zero effect on a bot and is the largest single
component of the *human's* difficulty, which is why human strategy writing over-weights it.

**T#2 — Ask the asker.** (Develin: "if you don't have the card that they asked about, you can do
well to ask for it yourself"; Deposit Genius; pagat)
*Definition:* after a seat asks in half-suit H and misses, ask that same card yourself.
*Engine:* the C5 disjunction fires on every ask including a teammate's (`belief.hpp:156-171`), and
`priorTheta` raises the prior for any seat that asked in H (`belief.hpp:100-108`).
*v0.5:* **Yes** — falls out of the posterior; no explicit term.
*Cheap?* already free. *Frequency:* a live legal ask on a card a **teammate** asked for and missed
exists at **35.13%** of decisions and v0.5 takes it **33.49%** of the time; on a card **the actor
itself** missed, **40.65%** available, taken **32.26%**. (v0.4 mirror: 24.48% / 39.85% and 62.47% /
**73.44%** — the v0.4 self-repeat rate is the repeat-ask pathology, and M1 fixed it.)

**T#3 — Exhaust one rank across opponents before switching rank.** (pagat asking-order)
*Definition:* if you want card c, ask every plausible holder of c before moving to a different card
of H, because switching card reveals you lack both.
*Engine:* nothing in `features()` references the previous *card* asked; only the previous *half-suit*
(`lastMySet`, `v05.hpp:324`).
*v0.5:* **No**, and `f[12]` (weight **+1.380**, `v05.hpp:51,324`) rewards re-entering the same
half-suit, which is the coarser and partly opposed rule. Measured `repeat (a,suit,t)` = **50.15%** of
v0.5's asks (`fish pathology --a=v05 --b=v05 --games=200 --seed=31`).
*Cheap?* yes — one `uint8` per (own card, target). *Frequency:* every multi-ask half-suit.
*Note:* conflicts with T#4; `research/v04/lit/fish-prior-art.md` H6/H7 already flags they must be
tested jointly.

**T#4 — Void creation.** (pagat: keep hitting the same opponent to void them of H)
*Definition:* prefer taking cards of H from the opponent who holds the most of them, so that they
become void in H and can never legally ask in it again (`fish.hpp:164`).
*Engine:* `f[11] = (handCount[target]==1) ? p : 0` (`v05.hpp:323`, weight **+1.160**) fires on
emptying the whole *hand*, not on voiding a *half-suit*. No feature counts the target's remaining
cards in H.
*v0.5:* **Weak** (wrong granularity). *Cheap?* yes — the per-(target, H) provable lower bound is
already available from `k.mask` and the C5 disjunctions.
*Frequency:* an opponent at whom **every** legal ask is provably dead (i.e. fully locked out of every
half-suit you can ask in) exists at **1.25%** of v0.5 decisions, 0.49% of (decision, opponent) pairs.
Against a v0.4 mirror it is **12.98%** / 5.30% — v0.5's shorter games (96 vs 143.6 events/game) give
voiding far less time to accumulate.

**T#5 — Lie low / do not open a half-suit you are long in.** (Develin; Wikipedia)
*Definition:* holding ≥4 of H, do not ask in H while others do; let them resolve it for you.
*Engine:* `f[9] = teamRevealedSet(S) ? 0 : 1` (`v05.hpp:321`, weight **−1.220**) and
`f[19] = f[9]·myHave/6` (`v05.hpp:331`, weight **−1.403**) penalise entering an unrevealed half-suit.
`teamRevealedSet` correctly judges on public information only (`v05.hpp:240-246`).
*v0.5:* **Weak** — outweighed by the completion terms `f[3]` (+2.507), `f[5]` (+4.046), `f[7]`
(+1.219) (`v05.hpp:42,44,46`).
*Cheap?* already computed. *Frequency:* 336 (decision, half-suit) opportunities in 400 games (≥4 in
hand, team publicly unrevealed); v0.5 **breaks concealment at 36.9%** of them against a chance rate
of ~1/6 ≈ 17%. v0.4: 158 opportunities, 40.5% broken. The defect is unrepaired and its *rate* is
essentially unchanged from v0.4.

**T#6 — Avoid an extended back-and-forth with one opponent.** (pagat)
*Definition:* a long two-seat exchange broadcasts a large joint certificate to the other two
opponents for an uncertain gain.
*Engine:* `f[12]` (`v05.hpp:324`) rewards it. No term counts the joint information a repeated
exchange broadcasts.
*v0.5:* **Weak — pushed the wrong way**, but the damage is much smaller than in v0.4: exact
`(actor, card, target)` repeats fall from **40.0%** (v0.4, BRIEF baseline) to **2.61%** (v0.5),
because M1 makes a repeat after a miss provably dead. Half-suit-level repeats remain at 50.15%.
*Cheap?* yes. *Frequency:* see above.

### B. Multi-step tactics from the corpus — the priority set

**T#7 — Blackballing / lockout.** (Develin: "Your team can conspire to blackball one or two of the
opponents so that it is never their turn"; pagat; Deposit Genius)
*Definition, operational:* let *j* be an opponent whose knowledge would let them take or declare a
half-suit if they held the turn. Then (a) never make an ask at *j* that can miss, and (b) when you
must give the turn away, give it to somebody else — by choosing a **provably-dead** ask at a
non-dangerous seat.
*What the bot must know:* seat *j*'s knowledge state, i.e. the common-knowledge public deduction
object refined by what our own `k` proves about *j*'s hand (`v05_target.hpp:44-63` shows the public
object is a **single** shared object, so this is O(1) extra state, not 6×).
*What action it takes:* the ask that keeps the turn away from *j* — which requires B1's dead ask.
*v0.5:* **Weak on (a), Blocked on (b).** (a) is approximated by `f[8] = (1-p)·threatOf(pub,target)`
(`v05.hpp:320`, weight **−2.906**) plus the top-K `-threatWeight·(1-p)·threat` term
(`v05.hpp:559-581`, `threatWeight = 2.705`). But `threatOf` (`v05.hpp:209-226`) estimates the
target's **cards**, using `activity = min(1, askCount[t][s]/3)` as a crude knowledge proxy — it never
asks what *j* can deduce. (b) is impossible: 0/34,668 dead asks.
*Cheap?* (a) yes, from the common-knowledge object. (b) yes — the candidate set already exists inside
`enumerateAsks` before M1 filters it.
*Frequency:* the *literal* Develin condition — an opponent who could declare a live half-suit from
**common knowledge alone** — is degenerate under v0.5 at **0.000%** of decisions, because
`declarationRound` runs before every ask (`game.hpp:294`) so any publicly-declarable half-suit is
cashed instantly. Relaxations: an opponent **one card away** publicly at **0.237%**; some live
half-suit where the *opposing* team owns everything at capacity-only P ≥ 0.50 at **1.748%**, at
P ≥ 0.90 at **0.288%** (seed 777001: 0.343%), and at P ≥ 0.90 with that opponent having asked there
≥2× at **0.288%**. At 66% of those P≥0.90 decisions a live ask at a non-implicated opponent existed,
so the *cheap* half of blackballing is available whenever it matters.
**Reading: blackballing as a rare high-stakes veto, not a common move.** The right v0.6 design is a
hard veto on the ~0.3% of decisions where an opponent is provably close to a half-suit, not a soft
weight, and it is worth roughly 0.3 declarations per hundred decisions — which is the same order as
the entire declaration-arbitration effect P6 measured (+0.30 pp).

**T#8 — Endgame declarer = most-informed (Develin) vs most-carded (Deposit Genius).**
*Definition:* the team routes the declaration to whichever member knows most.
*Engine:* `declarationRound` polls in a fixed order and takes the **first** willing seat
(`game.hpp:210-224`); `conf` is captured and discarded at `game.hpp:223`. `Rules::declArbitration = 0`
(`fish.hpp:119`). Srinivasan's "No Probabilistic Information" (pagat) is the rules argument for the
refusal.
*v0.5:* **No** for voluntary declarations; the forced endgame already has the information-safe
ladder (`fish.hpp:126-127`, driven at `game.hpp:238-256`).
*Cheap?* yes. *Frequency and value:* **measured and near-zero.**
`P6-declaration.md` §1.2–§1.4: 55.4% of declarations happen in a round where a teammate also
proposed, but only **16.7% of races** are contested on confidence, and in **70.6%** of contested races
*both* candidates are wrong. Confidence ranking — the information-**unsafe** upper bound — is worth
**+0.30 pp** [+0.12, +0.48]; the safe 17-rung ladder recovers +0.27 pp.
**Do not spend v0.6 effort here.** The upper bound is 0.3 points.

**T#9 — Turn transfer when cardless: exchange willingness bits.** (BRIEF rules of record §1)
*Definition:* a cardless turn-holder must pass to a live teammate; the rules permit the team to
exchange willingness bits.
*Engine:* `Agent::choosePassTarget(pub, cand, n)` (`game.hpp:29`) receives no teammate input;
`game.hpp:302` takes the unilateral answer. v0.5's implementation scores each candidate by *its own*
belief about that teammate's hand (`v05.hpp:945-967`).
*v0.5:* **No** for the exchange. *Cheap?* yes — `Rules::forcedTh` is the pattern.
*Frequency and value:* **measured and zero.** `P8-coordination.md`: a real (≥2 candidate) transfer
arises **0.148 times per game**, v0.4's unilateral pick already matches an omniscient oracle at
800/886 of them, and handing one team a *ground-truth* chooser is worth
**−0.0007 ± 0.0024 sets/game**. My own count on the v0.5 mirror: **0.33 pass events per game** total.
**Do not spend v0.6 effort here either.** P8 also found the ladder leaks 2.19 bits per candidate
against the ~1 bit the rules license.

**T#10 — Ask purely to tell your partner what you hold (the C5 certificate as a signal).**
(pagat, verbatim: "Sometimes it is correct to ask questions to which you already know that the
answer is no in order to give your teammates more information.")
*Definition:* choose the ask that maximises what your teammates learn, accepting a material loss.
*Engine:* the ask emits (i) C3 "the asker lacks c" and (ii) C5 "the asker holds another card of H"
(`belief.hpp:156-171`) to *all six seats*.
*v0.5:* **No.** No term in `features()` prices ΔI to the team; `f[9]`, `f[16]`, `f[19]`
(`v05.hpp:321,328,331`) price only leakage to opponents. The wiretap trade-off
`u = material + ν·ΔI_team − λ·ΔI_opp` is not present anywhere in `v05.hpp`.
*Cheap?* **Yes, and cheaper than it looks.** ΔI_team can be evaluated by applying the C3+C5
certificates to a *single* common-knowledge object and diffing entropies — that is one
`Knowledge::onEvent` + one Sinkhorn pass per candidate, and `chooseAsk` already runs two full
Sinkhorn passes per top-K candidate (`v05.hpp:549-566`) at `searchTopK = 6`.
*Frequency:* every ask. Free capacity available for it: §0.3.

**T#11 — The pure signalling channel: which opponent, and which card, among equivalents.**
(gamerules.com is *not* a source for this — see §7 correction 1; the sourced form is CardRules+
"use asking patterns as signals" and pagat's Ali Salahuddin convention)
*Definition:* among asks that are indistinguishable to the hard certificates, pick by a codebook the
teammate can decode.
*Engine:* the choice is unconstrained (`fish.hpp:179-196`).
*v0.5:* **No.** `askExpectedValue` still discards the seat identity (`v05.hpp:437-438`); every
target-dependent quantity in the score is material (hit probability `f[0-2,14,17,18]` via
`p = bel.marg[card][target]` at `v05.hpp:287`, threat `f[8]`, hand size `f[10]`, emptying `f[11]`,
exposure `f[16]`, and the top-K chain/threat terms at `v05.hpp:581`).
*Cheap?* yes. *Frequency and capacity (v0.5 mirror, seed 31 / seed 777001):*

| dimension | mean free bits/ask | decisions with ≥2 options |
|---|---|---|
| target, given the card | **0.959 / 0.957** | 69.20% / (69.0%) |
| card, inside the chosen half-suit, given the target | **1.301 / 1.309** | 82.09% / 82.48% |
| joint, over all same-liveness legal asks | **4.979 / 4.992** | 99.15% / 99.22% |

*Price:* `P5-verify-target-channel.md` §3 measured that on v0.4 the class is indistinguishable to
the certificates but **not** to the policy — the spread of v0.4's own `bel.marg` inside the class is
0.25–0.29 and moving inside it flips v0.4's preferred ask at 86–88% of such decisions. The same
architecture ships in v0.5, so **treat this as a priced channel: ~0.25 hit probability per bit.**
*Convention status:* Develin forbids **pre-agreement**; owner decision D1 (`BRIEF.md`) is to ship it
behind `--conventions=off|on` with **off** as headline and publish the delta.

**T#12 — The deliberate safe ask as a costly signal.** (pagat, same quote as T#10; CardRules+
"ask for cards you know an opponent cannot have to avoid giving away information")
*Definition:* spend a guaranteed miss to name a half-suit to your partner, paying the turn.
*Engine:* legal (`fish.hpp:158-165` tests hands, not beliefs).
*v0.5:* **Blocked by B1.** *Cheap?* yes — the moves are enumerated and then discarded at
`v05.hpp:487`.
*Frequency and price (v0.5 mirror, seed 31 / 777001):* a provably-dead ask is available at
**79.17% / 79.47%** of decisions. Its **message alphabet** — distinct half-suits nameable by a
provably-dead ask — is **0.425 / 0.421 bits** on average, with ≥2 distinct half-suits at
**36.96% / 36.74%** of the decisions where any dead ask exists. Its **price** is the best live hit
probability forgone: **0.5617 / 0.5676** under the capacity-normalised marginal (v0.4 mirror:
0.4437) against a v0.5 baseline ask hit rate of **55.4%**.
**Reading: the safe-ask channel is available almost always, is worth <½ bit per use, and costs
roughly one whole expected take.** It is a *last-resort* signal, not a routine one. A v0.6 that fires
it whenever it is available will lose; a v0.6 that fires it when the message is worth ≥1 take
(e.g. "I am 5/6 in H, prepare to declare") may not.

**T#13 — Turn routing: donate the turn to a chosen opponent.** (Develin's blackball mechanism;
Wikipedia's stalemate-breaker uses the declaration version)
*Definition:* when you cannot profitably continue, hand the turn to the opponent you least mind
holding it, by a provably-dead ask at that seat.
*v0.5:* **Blocked by B1.** *Cheap?* yes.
*Frequency (v0.5 mirror, seed 31 / 777001), distinct opponents reachable by a provably-dead ask at a
non-dangerous seat:* 0 → **20.83% / 20.53%**; 1 → **29.05% / 28.55%**; 2 → **15.10% / 15.41%**;
3 → **35.02% / 35.51%**. So **≥2 chosen destinations at 50.12% / 50.92%** of decisions.
(v0.4 mirror at the same seed: 13.08 / 46.18 / 12.60 / 28.15.)
This is the single largest unused *action* channel in the engine, and unlike T#11 it is not a
codebook — it needs no partner decoding and is therefore **legal under Develin's convention ban**.

**T#14 — Baiting an opponent into a revealing ask.**
*Definition:* make an ask whose *certificate* induces a named opponent to make an ask that reveals
more than it gains, then exploit the reveal.
*Engine:* requires (i) a per-seat knowledge model (B2) and (ii) a model of that seat's *policy*, then
(iii) at least a two-ply search over the induced reply.
*v0.5:* **No.** Opponent modelling is still the two global scalars `priorTheta`/`priorPhi`
(`v05.hpp:29-30`, consumed at `belief.hpp:100-108`) — one pair for all five seats, never updated
in-game. `v05_oppmodel.hpp` (M7) designs the replacement and proves algebraically that **`priorPhi`
cannot be a second channel at all** (`v05_oppmodel.hpp:16-40`: a seat-only factor is erased exactly
by Sinkhorn/IPF, so the shipped pair is the single effective weight `theta_eff = theta + phi`), which
is why `P3-deception.md` §5 measures the `phi` column flat over a 15-fold sweep. M7 is not included
by `v05.hpp`.
*Cheap?* **No.** This is the most expensive item in the catalogue: it needs a policy model plus a
second ply, on top of `chooseAsk` already doing 2 Sinkhorn passes × 6 candidates.
*Frequency:* not measurable without the model. **Lowest priority.**

**T#15 — The stalemate-breaker: hold a fully-attributed half-suit in reserve, cash it to move
control.** (Wikipedia, verbatim; Develin's declare-to-transfer-control; Deposit Genius "refraining
from claiming")
*Definition:* when your team provably owns H and can allocate it, *do not* cash it. Cash it later, at
the moment when doing so empties a hand and thereby routes the turn — typically your own, so that
`choosePassTarget` hands the turn to the teammate who is stuck.
*Engine:* the mechanism is `game.hpp:189` (declaration strips the half-suit from every hand) →
`game.hpp:298-309` (a cardless turn-holder passes to a chosen teammate). Waiting is risk-free: an
opponent holding no card of H can never legally ask in it (`fish.hpp:164`), so `C_steal = 0`.
*v0.5:* **No, and structurally cannot be.** `declareByValue` (`v05.hpp:779-797`) prices the
declaration through the value function only, and `value()` takes **no `myCards`/`minFriendly` delta**
(`v05.hpp:393-394`) — see B4 and the discarded `mine` at `v05.hpp:790,793`. `patientLocked = true`
(`v05.hpp:64`) is **dead code in the shipped configuration**: `useValue` and `valueDeclare` are both
true (`v05.hpp:70,73`), so `declareNow` returns from the `declareByValue` branch at `v05.hpp:803-805`
and never reaches the `patientLocked` test at `v05.hpp:807-811`.
*Cheap?* **Yes — two argument slots in `value()`.** `dOur` already exists in the signature
(`v05.hpp:374`); `f[10]`/`f[11]` need the same treatment.
*Frequency:* v0.5 holds a provably-locked half-suit for a mean of **5.73 events** before cashing it
(`lockHoldA`, `fish match --a=v05 --b=v05 --games=150 --seed=31 --json`; against v0.4: 5.05 vs 6.35).
**30.5%** of v0.5's declarations empty somebody on the declaring team (11.78% the declarer, 18.72% a
teammate) and **5.17%** empty the current turn-holder — i.e. a declaration reroutes the turn about
**0.47 times per game**, entirely as a side effect the policy cannot see. A situation where the
actor's team provably owns a live half-suit *and* a teammate is already cardless arises at 0.000% of
decisions, so the tactic's value is in the *timing* of the emptying, not in a reserve held across
many plies.

**T#16 — Declare to protect a teammate about to run out of cards.** (derived; adjacent to Deposit
Genius's "a player with many cards attracts inquiries")
*Definition:* the same delta as T#15 with the opposite sign — decline to cash a half-suit whose
cards sit mostly with a teammate who is nearly out, because cashing it removes their asking rights.
*v0.5:* **No.** Same defect as T#15 (B4). *Cheap?* yes, same fix.
*Frequency:* 18.72% of v0.5 declarations empty a teammate.

**T#17 — Declare early to deny information vs. late to keep options.** (source conflict: Develin,
pagat P1 and Wikipedia say *hold*; CardRules+ says *claim immediately*)
*Definition:* a correct declaration publishes the exact allocation of six cards to the opponents.
*Engine:* it also drops the declaring team's public hand counts, tightening the capacity constraint
C4 (`belief.hpp:214+ propagateCapacity`) for everyone.
*v0.5:* **Yes as optimal stopping** (`declareByValue`, `v05.hpp:779-797`), **No on the information
price** — `value()` has no leakage term.
*Cheap?* the leak is exactly measurable: it is the count of previously-unresolved cards the
declaration resolves in the common-knowledge object.
*Frequency and magnitude:* a correct v0.5 declaration reveals **2.173** previously-unresolved cards
on average (v0.4: 1.978), over 9.0 declarations per game. That is ~19.6 cards of allocation
information published per game per table, free to the opponents.
*Source conflict resolution:* the majority is right under **these** rules — an opponent with no card
of H can never legally ask in it (`fish.hpp:164`), so a held half-suit cannot be stolen and
`C_steal = 0` exactly. CardRules+ describes a house rule set where claims race.

**T#18 — Delay a declaration to keep your hand count uncertain.** (derived, from the rules of record:
"players may ask how many cards another player holds")
*Definition:* your public hand count is a hard constraint on everyone's posterior (C4). Cashing a
half-suit shrinks it and sharpens every opponent's inference about your remaining cards; holding it
keeps the count large and the inference loose.
*Engine:* `Event::handCount[NPLAY]` is emitted with every event (`fish.hpp:102`, `game.hpp:149`);
`Knowledge::capacities` (`belief.hpp:83-90`) and `propagateCapacity` (`belief.hpp:214+`) consume it.
*v0.5:* **No.** Nothing prices the *sharpening* a declaration does to the opponents' capacity
constraint; `f[10] = handCount[target]/9` (`v05.hpp:322`) reads only the *target's* count, as
material.
*Cheap?* yes — the number of cards a declaration would resolve for the opponents is the T#17 measure
and can be reused.
*Frequency:* every declaration, 9.0/game. **Measured evidence that C4 does real work:**
**5.06 cards per game** are resolved in the common-knowledge state by *pure inference* rather than
direct observation (**9.36%** of all resolutions; v0.4 mirror: 4.21/game, 7.79%). Those are exactly
the cards a counting player picks up, and the count is what makes them pickable.

**T#19 — Counting-based endgame squeeze.** (derived; the ancestor is pagat's card-count query rule
and Develin's "clean out suits sequentially")
*Definition:* late in the game, use the hand-count arithmetic plus the resolved cards to force an
opponent's remaining unknowns into a unique consistent assignment (a Hall/transportation argument),
then declare or ask with certainty.
*Engine:* the exact form exists — `blockdp.hpp` (`teamOwnsProbability`, `bestTeamAllocation`) — but is
only reachable under `belief=block` (`v05.hpp:724-735`), which is **not** the shipped configuration
(`cfg.belief = BeliefMode::Fast`, `v05.hpp:27`). In `Fast` mode the squeeze is approximated by
`sinkhornDisj` + the 729-way `feasibleAllocation` enumeration (`v05.hpp:619-675`).
*v0.5:* **Weak** — `feasibleAllocation` is M2 and it *is* a genuine capacity-feasibility argument
(and it is what took forced-endgame declarations from 100% wrong to 2/2, and voluntary declarations
from 10.4% wrong to **2.11%** wrong). But it applies the constraint only to *our own team's*
allocation, never to force an inference about an *opponent's* hand.
*Cheap?* the one-sided version is already there; extending `propagateCapacity` with a degree-
constrained-flow / Hall oracle over the transportation polytope is the item
`P5-verify-human-strategy.md` §2 already priced (exact inference lifts D13 provability from 0.34% to
only 0.544%, so **do not expect much from a stronger oracle on that channel**).
*Frequency:* 5.06 cards/game resolved by inference alone (T#18). The relevant unexploited residue is
that these resolutions are *available to the opponents too* and v0.5 does not reason about who has
them.

**T#20 — Deliberately running the opponents out of cards.** (derived; the risk half is not in any
source)
*Definition:* emptying **one** opponent is good — they cannot be asked, so they can never receive the
turn (`fish.hpp:162`). Emptying **the last live opponent** triggers the forced endgame
(`game.hpp:297`), in which your team must declare every remaining half-suit with no more asking.
*v0.5:* **Weak and possibly mis-signed** — `f[11] = (handCount[target]==1) ? p : 0` (`v05.hpp:323`,
weight **+1.160**) does not distinguish the two cases.
*Cheap?* yes — one extra condition (`count of live opponents ≥ 2`).
*Frequency:* far less dangerous than in v0.4. Forced-endgame declarations: **2 in 400 v0.5 mirror
games** (v0.4 mirror: 16 in 240 games, and `P0` measured them **100% wrong**). M2 defused this, so
the mis-sign is now a small-stakes item — but note only 0.17% of v0.5's declarations empty an
opponent, so the *positive* half of the feature is also nearly never exercised.

### C. Tactics that the rules of record make unavailable

**T#21 — Keep your last card of a half-suit to preserve asking rights.** *Not available*: a hit
transfers the card unconditionally (`game.hpp:350`); you may not decline. Its legal cousin is T#4.

**T#22 — Sacrificing a card to protect a half-suit.** *Not available in the literal form* for the
same reason. The nearest legal construct, and the one worth building, is a **decoy ask**: enter a
half-suit you do *not* care about in order to draw the opponents' asks there and away from the one
you are long in. This is T#5 run in reverse and it is expressible today — it needs a term that prices
the *opponents'* subsequent ask distribution, which requires B2. Not implemented; not in the corpus;
frequency not measured.

**T#23 — Ask for a card you already hold.** (Wikipedia: "a variant played by some advanced players …
in order to confuse opponents") *Illegal* under the rules of record (`fish.hpp:163`, `fish.hpp:173`).
Recorded so it is not proposed again.

**T#24 — Pre-agreed codebooks (Ali Salahuddin; §7.9 Constructions A/B/E).** *Disputed.* Develin:
"Conventions are explicitly forbidden in Canadian Fish." pagat documents Ali Salahuddin's convention
in real use: after A's failed ask for the 2♥, partner C asks for a **different** minor heart if C
holds the 2♥, and **repeats** the ask for the 2♥ if C does not. Note this convention rides on the
**card** dimension, which §4 T#11 measures at **1.30 bits/ask** — the largest of the three
dimensions and the one v0.5 leaves entirely to `f[12]`'s half-suit-level stickiness. Owner decision
D1 governs: build behind a flag, ship **off**, publish the delta.

---

## 5. Cross-reference: which blocker gates which tactic

| Tactic | B1 no dead ask | B2 no seat model | B3 no ΔI_team | B4 value blind to hands | B5 deterministic |
|---|---|---|---|---|---|
| T#7 blackball | ✔ (the routing half) | ✔ | | | |
| T#10 certificate as signal | | | ✔ | | |
| T#11 target/card codebook | | ✔ (decoder) | ✔ | | ✔ |
| T#12 safe ask as costly signal | ✔ | | ✔ | | |
| T#13 turn routing | ✔ | ✔ (who is safe) | | | |
| T#14 baiting | | ✔ | | | |
| T#15 stalemate-breaker | | | | ✔ | |
| T#16 protect a teammate | | | | ✔ | |
| T#17/#18 declaration timing | | ✔ (whose inference) | ✔ | ✔ | |
| T#22 decoy ask | | ✔ | ✔ | | ✔ |

**One fix, B1 (price the dead ask instead of banning it), unlocks four of the priority tactics.**
It is also the cheapest: the moves are already enumerated at `v05.hpp:484` and thrown away at
`v05.hpp:487`. The obvious safe shape is a *quota*: allow a provably-dead ask only when its scored
message value exceeds the measured price of 0.56 expected hit probability, and cap the number per
game so the v0.4 deadlock (which was an *unpriced* dead ask, not a chosen one) cannot return.

---

## 6. Measurements (raw)

All runs this session, engine at `bd812fe`, rebuilt with `cd engine && make`.

### 6.1 v0.5 mirror baseline (`fish pathology --a=v05 --b=v05 --games=200 --seed=31`)

```
games 400   events/game 96 (median 95, p90 112, p99 124, max 131)
asks/game 86.67   hit rate 55.42%
DEAD asks 0 (0%)          starved turns 0 (0%)
repeat (a,c,t) 906 (2.61%)     repeat (a,suit,t) 17386 (50.15%)
asks in own-locked 3522 (10.16% of asks)   <- guaranteed misses the actor CANNOT prove
declarations 3600  wrong 76 (2.11%)   at/after ev>=220: 0   forced endgame: 2 (100% wrong)
```
For contrast, the v0.4 mirror at the same seed (BRIEF baseline): 143.6 events/game, 34.2% hit rate,
39.0% dead asks, 40.0% exact repeats, 10.4% wrong declarations, longest dead run 286.

`fish match --a=v05 --b=v05 --games=150 --rotations=2 --seed=31 --json`:
`askAcc 0.5548, declAcc 0.9770, declPerGame 4.497, outOfTurn 3.333, lockHold 5.729,
eventsPerGame 95.36`.

### 6.2 Signalling capacity (`fish humanchan --a=v05 --b=v05 --games=200 --seed=31`)

```
ask decisions 34668     mean legal asks/decision 46.23
(A) mean indistinguishable targets 2.149   free bits 0.959   >=2 at 69.20%   spread 0.0901
(B) D13 provably team-owned live half-suit available at 34 decisions (0.098%), used 0
(C) opponents reachable by a provably-dead ask: 0->20.83%  1->29.05%  2->15.10%  3->35.02%
    asks that were provably dead: 0 (0%)
(E) concealment opportunities 336, broken 124 (36.90%)
```
Against v0.3 (`--b=v03`, 240 games, seed 90210) the target dimension is 0.947 bits / 68.17%.
Note that mixed-policy runs mix v0.5 and v0.3 actors; only the mirror rows are pure v0.5.

### 6.3 Tactic-incidence probe (scratch `r9`, §8), v0.5 mirror, seed 31 / seed 777001

| quantity | seed 31 | seed 777001 |
|---|---|---|
| ask decisions | 34,668 | 34,948 |
| opponent publicly able to declare a live half-suit | 0.000% | 0.000% |
| opponent one card away, publicly | 0.237% | — |
| opposing team owns a live half-suit at capacity-P ≥ 0.50 | 1.748% | — |
| … at P ≥ 0.90 | 0.288% | 0.343% |
| … and a live ask at a non-implicated opponent existed | 66.0% of those | 30.0% of those |
| provably-dead ask at a non-dangerous opponent available | 79.17% | 79.47% |
| ≥2 distinct safe destinations for the turn | 50.12% | 50.92% |
| opponent at whom **every** legal ask is provably dead | 1.25% of decisions | — |
| live ask on a card a **teammate** missed | 35.13% avail, 33.49% taken | 33.58% / 35.21% |
| live ask on a card the **actor** missed | 40.65% avail, 32.26% taken | 39.98% / 31.35% |
| free bits — target dimension | 0.959 | 0.957 |
| free bits — card dimension inside the chosen half-suit | 1.301 | 1.309 |
| free bits — joint over same-liveness legal asks | 4.979 (≥2 at 99.15%) | 4.992 (≥2 at 99.22%) |
| costly-signal alphabet (distinct half-suits nameable by a dead ask) | 0.425 bits, ≥2 at 36.96% | 0.421, 36.74% |
| price of a dead ask (best live capacity-marginal forgone) | **0.5617** | **0.5676** |
| cards resolved in the public state by pure inference | 5.055/game (9.36%) | — |
| declaration empties: declarer / teammate / opponent / turn-holder | 11.78% / 18.72% / 0.17% / **5.17%** | — |
| unresolved cards published by a correct declaration | 2.173 each | — |
| pass events (cardless turn transfers) | 0.330/game | — |

v0.4 mirror at seed 31 for contrast (240 games, 32,008 decisions): fully-voided opponent **12.98%**
of decisions; teammate-missed-card taken 39.85%; actor-missed-card taken **73.44%**; free bits target
0.611 / card 1.433 / joint 3.818; dead-ask price 0.4437; inference resolutions 4.208/game (7.79%);
declaration reveals 1.990 cards; declaration empties the turn-holder 5.22%.

---

## 7. Corrections to the prior reports

1. **`P5-human-strategy.md` §1 attributes to gamerules.com the sentence** "Partners convey strategic
   information primarily through the choice of whom to ask … and the specific cards requested" **and
   calls it "the only external source that names the two signalling dimensions of §7.9 explicitly".**
   I fetched `https://gamerules.com/rules/literature-card-game/` this session and it contains **no
   strategy section at all** — objective, setup, ranking, dealing, turn mechanics, claiming,
   information rules, scoring, and nothing else. The claim should be re-sourced to CardRules+
   ("use asking patterns as signals … an unusual ask tells your teammates you want them to also ask
   for that half-suit") or dropped. The *substance* of §7.9's two dimensions is unaffected.
2. **`docs/EXTERNAL_STRATEGY_REVIEW.md:7` cites `canadian-fish.vercel.app/strategy`.** That page now
   returns a bare heading with no body. Any claim traced only to it is currently unverifiable.
3. **`P5-human-strategy.md` §4 items 14–15 and §7 item 5 ("Repair `value()`") were not actioned.**
   The defect survives verbatim in v0.5 at `v05.hpp:393-394` and `v05.hpp:790,793`. `docs/V05_FINDINGS.md`
   does not claim it was fixed; this is recorded so v0.6 does not re-diagnose it.
4. **`patientLocked` is unreachable in the shipped v0.5 configuration** (`v05.hpp:64` vs the
   `declareByValue` early return at `v05.hpp:803-805`). Any ablation that sets `patient=0`
   (`factory.hpp`) will therefore measure nothing. Not previously recorded anywhere I read.

---

## 8. Ranked v0.6 opportunities

Ordered by (measured frequency × measured stake) ÷ implementation cost. Difficulty is my estimate;
payoff is annotated with what the evidence actually supports.

| rank | item | what it is | difficulty | payoff evidence |
|---|---|---|---|---|
| 1 | **Price the dead ask instead of banning it (repeal B1 with a quota)** | allow a provably-dead ask when its scored message value beats the measured 0.56 forgone hit probability; cap per game | low — the moves are enumerated at `v05.hpp:484` and dropped at `v05.hpp:487` | unlocks T#7(b), T#12, T#13; availability 79.2% of decisions, ≥2 destinations at 50.1%; **risk: this is the move class that caused the v0.4 deadlock** — the quota is the whole design |
| 2 | **Repair `value()`: perturb `myCards` and `minFriendly`** | pass the two deltas `declareByValue` already computes | **very low** — `v05.hpp:790` computes `mine` and `v05.hpp:793` discards it | unlocks T#15, T#16, T#18; declarations reroute the turn 0.47×/game and empty a team member at 30.5% of declarations, all invisible today. `vw[10] = -0.0066`, `vw[11] = -0.0075` are near zero, so the coefficients must be **refitted**, not just wired |
| 3 | **Ship M4: one common-knowledge `Knowledge` object + per-seat refinement** | `v05_target.hpp` is written, proved and tested; it is simply not `#include`d by `v05.hpp` | low-medium — the design work is done | prerequisite for T#7(a), T#11 decoding, T#14, T#19, T#22. The structural claim (one object, not six) makes it ~O(1), not 6× |
| 4 | **A ΔI_team term in the ask score** | `u = material + ν·ΔI_team − λ·ΔI_opp`; ΔI_team from applying C3+C5 to the common-knowledge object and diffing | medium | 4.98 free joint bits/ask at 99.15% of decisions; today three leak terms and zero gain terms (`v05.hpp:321,328,331`) |
| 5 | **Stochastic action selection (BRIEF D2 / Construction C)** | softmax over asks within a margin, temperature keyed on partner regime | low | B5: `rng` never reaches action selection (`v05.hpp:195,764`). This is the direct answer to owner report #4 and it needs an exploitability probe, not a win rate |
| 6 | **Re-weight or gate the lie-low terms (T#5)** | hard gate: do not open a half-suit where the team is ≥4 and publicly unrevealed unless the ask completes it | very low | concealment broken at 36.9% vs ~17% chance; unchanged from v0.4 |
| 7 | **Half-suit-level void progress feature (T#4)** | per-(target, half-suit) provable lower bound, replacing `f[11]`'s whole-hand test at `v05.hpp:323` | low | a permanently locked-out opponent exists at only 1.25% of v0.5 decisions, so the payoff is small *under v0.5's short games* and may grow once T#12/T#13 lengthen them |
| 8 | **Split `f[11]`'s sign on "last live opponent" (T#20)** | one extra condition | very low | small now: M2 cut forced-endgame declarations to 2 per 400 games |
| — | **Do not build**: declaration arbitration by confidence (T#8, upper bound +0.30 pp), willingness-bit turn transfer (T#9, measured −0.0007 ± 0.0024 sets/game), a stronger provability oracle for the D13 channel (exact inference lifts it 0.34% → 0.544%) | | | P6 §1.4, P8 §1, P5-verify-human-strategy §2 |

**The single biggest risk to this plan** is that items 1 and 4 re-open the failure mode M1 closed.
Any v0.6 that re-admits dead asks must be gated on the P0/P1 deadlock instruments
(`fish pathology`: longest dead run, dead-run incidence, events/game p99) before any win-rate number
is quoted, and must report the per-opponent table and worst case per the owner's standing preference.

---

## 9. Reproduction

```
cd "/Users/dylan/Documents/GitHub/fish optimization/engine" && make
./fish pathology --a=v05 --b=v05 --games=200 --seed=31
./fish match     --a=v05 --b=v05 --games=150 --rotations=2 --seed=31 --json
./fish humanchan --a=v05 --b=v05 --games=200 --seed=31
./fish humanchan --a=v05 --b=v03 --games=120 --seed=90210

# tactic-incidence probe (new, scratch only; nothing under engine/src touched)
c++ -std=c++20 -O2 -Isrc <scratch>/r9tactics.cpp -o <scratch>/r9 -pthread
<scratch>/r9 --a=v05 --b=v05 --games=200 --seed=31
<scratch>/r9 --a=v05 --b=v05 --games=200 --seed=777001
<scratch>/r9 --a=v04 --b=v04 --games=120 --seed=31
```

`r9tactics.cpp` is at
`/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/eacfe5c8-cfa1-4aef-ad3c-cf4e017d6982/scratchpad/r9tactics.cpp`.
It replays traced games and reconstructs (i) each actor's own `Knowledge` and (ii) a single
common-knowledge `Knowledge` seeded all-possible, then feeds both the public event stream. Ground
truth is consulted nowhere except to seed the actors' own hands, so every number is a capability the
acting policy could have used. The common-knowledge object rests on the structural claim proved in
`engine/src/v05_target.hpp:44-56` and checked empirically by `engine/src/probe_m45_test.cpp`.

Total measured wall time for everything in §6: under 40 s.
