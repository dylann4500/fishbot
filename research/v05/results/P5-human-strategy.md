# P5 — What strong human Fish players do that v0.4 cannot express

Task P5, FishBot v0.5 investigation. Repository `/Users/dylan/Documents/GitHub/fish optimization`.
All engine claims cite `file:line` at the working-tree state of 2026-08-22. All measurements are
reproducible with the commands in §8.

---

## 0. Summary

Three results, in order of how much they should change v0.5.

1. **The target dimension is a large, nearly-free, completely unused channel.** Measured over
   154,318 v0.4-mirror ask decisions: at **46.6%** of decisions at least two opponents are
   *hard-indistinguishable* holders of the card actually asked for, mean **0.639 free bits per
   ask**, at a mean capacity-marginal hit-probability spread of only **0.081**. In v0.4-vs-v0.3
   (111,085 decisions) it is **66.2%** and **0.919 bits**. v0.4 cannot use one bit of it: the
   dominant half of its ask score, `askExpectedValue`, opens with `(void)target;`
   (`engine/src/v04.hpp:435`) and is a function of the card and the hit probability only.

2. **The "free information channel" the brief is built on is real by the rules but is almost
   never *provable* at the table.** The v0.4 pathology run reports 16.5% of asks land inside a
   half-suit the actor's team already owns outright — but that is a ground-truth statistic. When
   I require the actor to be able to *prove* team ownership from its own certificate knowledge
   (the only basis on which it could deliberately choose such an ask), the condition holds at
   only **0.34%** of mirror decisions and **2.0%** of v04-vs-v03 decisions, and in the mirror
   **31.3%** of those are forced — no other legal ask existed. **The D13 free channel alone will
   not break the deadlock.** A v0.5 built on it must either widen provability (a flow/Hall
   oracle over the transportation polytope) or accept a *priced*, not free, channel.

3. **Turn routing is available and unmodelled.** At **45.5%** of mirror decisions (**57.6%**
   vs v0.3) the actor could hand the turn to **two or more chosen opponents** by a provably-dead
   ask, and at 32.4% (43.4%) to any of all three. That is the mechanism behind blackballing, the
   Spence-style "safe ask", and Develin's declare-to-transfer-control line. v0.4 has one soft
   penalty for donating the turn to a dangerous seat (`f[8]`, weight −3.098) and no concept of
   choosing *whom* to donate it to.

A fourth, structural: **v0.4 never constructs a knowledge model of any other player.** The only
`Knowledge` objects it builds are clones of its own (`v04.hpp:507`, `v04.hpp:524`, both `Knowledge
kh = k;`). Every technique on the list below that turns on "what does *he* know" — blackballing,
decoding a teammate's signal, choosing the best-informed declarer, baiting — is out of reach not
because a feature is missing but because the state it would read does not exist.

---

## 1. Sources read this session

Read in full or fetched this session (not from memory):

| Source | What it added beyond `research/v04/lit/fish-prior-art.md` |
|---|---|
| [pagat.com Literature](https://www.pagat.com/quartet/literature.html) (McLeod, page updated 1 Jul 2026) | Re-confirmed verbatim: "Sometimes it is correct to ask questions to which you already know that the answer is no in order to give your teammates more information."; lockout; void creation; asking-order; back-and-forth warning; endgame protocol; Ali Salahuddin's convention; Srinivasan's Forced Claims / No Probabilistic Information / Challenge |
| [Develin, *The Ten Best Card Games You've Never Heard Of*, ch. Canadian Fish](http://www.bantha.org/~develin/cardgames.html) (live site; the archive.org copy is not fetchable from this environment) | Re-confirmed: memory budgeting; blackball worked example ("you should never ask him a question"); ask-the-asker; lie low; "The only way your opponents can win the suit is if you declare it wrong"; the clubs/diamonds declare-to-transfer-control example; endgame declarer = most-informed; **"Conventions are explicitly forbidden in Canadian Fish."** Also a **negative**: Develin gives *no* advice on which opponent to ask or which card within a half-suit |
| [Wikipedia, Literature (card game)](https://en.wikipedia.org/wiki/Literature_(card_game)) | Information-asymmetry master principle; the **stalemate-breaker** (hold a fully-attributed set in reserve to break a deadlock and move control) — verbatim confirmation |
| [Deposit Genius, Literature Game Strategy](https://depositgenius.com/literature-strategy-canadian-fish/) | Blackballing; ask-the-asker; deferring claims; "Don't Claim without 100% Certainty"; endgame delegation **to the player with the most cards** |
| [CardRules+, How to Play Literature](https://cardrulesplus.com/games/literature/) | **New and contradictory**: "Claim half-suits AS SOON as you are certain … not after." Also: "Opening moves: start with high-confidence asks (70%+ certain) rather than pure fishing expeditions"; "Use unusual asks as coded messages" |
| Web search, Ali Salahuddin convention provenance | The convention's exact content re-confirmed independently of pagat: after A's failed ask for the 2♥, partner C **asks for a different minor heart** if C holds the 2♥, and **repeats the ask for the 2♥** if C does not |
| [gamerules.com](https://gamerules.com/rules/literature-card-game/) | "Partners convey strategic information primarily through the choice of **whom to ask** for cards and the **specific cards requested**" — the only external source that names the two signalling dimensions of §7.9 explicitly |

Negative search results, recorded so they are not re-run: no BoardGameGeek strategy thread, no
Reddit thread, no university-club wiki, and no Discord/competitive-scene document surfaced for
Literature/Canadian Fish beyond the sources above. The only competitive-scene trace found is an
MIT Splash (Fall 2025) class listing a Literature/Fish strategy session — no materials online.
`grokipedia.com` and the Quora thread returned HTTP 403 and could not be read.
The corpus is small and has not grown since the v0.4 literature review; §5 of
`research/v04/lit/fish-prior-art.md` remains the best extraction of it and is not superseded.

**One genuine source conflict, worth a decision.** On declaration timing, Develin, pagat (P1)
and Wikipedia (W3) all say *hold* a fully-owned half-suit; CardRules+ says *claim immediately*.
v0.4 sides with the majority (`patientLocked = true`, `v04.hpp:96`; optimal stopping in
`declareByValue`, `v04.hpp:653-671`) and that is right on the rules — an opponent with no card of
the half-suit can never legally ask in it (`fish.hpp:158-166`), so `C_steal = 0`. CardRules+ is
describing a house rule set where claims race.

---

## 2. Legality frame — what these rules actually permit

Checked against `engine/src/fish.hpp`, `engine/src/game.hpp`, and the rules of record in
`research/v05/BRIEF.md`.

| Channel | Legal? | Where |
|---|---|---|
| Encode meaning in **which card** you ask for | Yes — the choice is unconstrained beyond legality | `fish.hpp:158-166`, `fish.hpp:179-196` |
| Encode meaning in **which opponent** you ask | Yes — any live opponent | same |
| A deliberate ask you know will miss | Yes — legality tests hand membership, never belief | `fish.hpp:158-166` |
| Asking inside a half-suit your team owns outright | Yes — you still hold a base card, you still lack the asked card | same; the certificate is still emitted at `belief.hpp:154-158` |
| Declaring at any moment, incl. an opponent's turn, holding no cards | Yes | `fish.hpp:108-109` |
| Refusing to hand over a card you hold when asked | **No.** The transfer is unconditional | `game.hpp:350` |
| Verbal table talk beyond the two public queries | **No** | BRIEF rules of record |
| Willingness bits when a cardless player passes the turn | **Yes, and the engine does not implement it** | `game.hpp:302` passes only `pub` and the candidate list to `Agent::choosePassTarget` (`game.hpp:29`) |
| Pre-agreed codebooks (Ali Salahuddin, Constructions A/B/E) | **Disputed.** Develin: "Conventions are explicitly forbidden"; pagat documents Ali Salahuddin's convention as in use. The BRIEF's rules of record are silent | — |

The convention question is a decision the project owner has to make, and it is not a
detectability question: a code carried entirely in *which legal action you choose* is
indistinguishable from good judgement, so no rule can police it. What Develin forbids is the
*pre-agreement*. A self-play-trained v0.5 pair has an implicit pre-agreement by construction.
Recommendation: ship the convention behind a flag (`--conventions=off|on`) and report both, as
`fish-prior-art.md` §10 already proposed for H15 — the with/without delta is publishable.

---

## 3. Measured channel capacity (new evidence)

New probe: `engine/src/probe_human.hpp`, CLI `fish humanchan`
(block appended at the end of `engine/src/main.cpp`; include at `main.cpp:12`).
It replays traced games and reconstructs each actor's `Knowledge` at the moment it moves, so
every number below is a capability the acting policy *could* have used from public information
plus its own hand. Ground truth is never consulted.

```
v04 vs v04, 600 deals x 2 rotations, seed 31        v04 vs v03, 600 x 2, seed 90210
ask decisions                        154,318        111,085
mean legal asks / decision              41.4         49.2
(A) target dimension
  mean indistinguishable targets         1.76         2.10
  mean free bits in target choice       0.639        0.919
  decisions with >= 2 equivalents       46.6%        66.2%
  mean capacity-marginal spread         0.081        0.076
(B) D13 free channel (PROVABLY team-owned live half-suit with a legal ask)
  decisions where it existed             0.34%        2.02%
  v0.4 asked there                     88.6% of avail 17.8% of avail
  of those, no other legal ask existed  31.3%         1.4%
(C) turn routing by a provably-dead ask — distinct opponents reachable
  0 opponents                           13.4%        16.9%
  1 opponent                            41.0%        25.5%
  2 opponents                           13.1%        14.3%
  3 opponents                           32.4%        43.4%
(D) dead-ask breakdown
  dead asks                             36.6% of asks  2.7% of asks
    in a provably team-owned half-suit   0.8%         13.2%
    in a contested half-suit (waste)    99.2%         86.8%
(E) concealment
  (decision, half-suit) pairs, >=4 in hand, team unrevealed   902     681
    broken by asking in that half-suit  38.4%        43.2%
```

Readings.

- **(A)** is the headline. 0.64–0.92 bits per ask sit in the target dimension at a mean hit-cost
  of 0.08 probability. Over a v0.4-mirror game each team makes ~64 asks, i.e. **~41 bits of
  unused private channel per team per game**. §7.9's channel-inventory table estimated
  $\log_2 3 = 1.58$ bits for the target dimension as an upper bound; the *usable* figure once
  hard exclusions are respected is 0.64–0.92.
- **(B) is the negative result that matters.** The BRIEF's central mechanism — asks inside a
  locked half-suit still emit certificates, so information *can* keep arriving — is correct as a
  rules statement but nearly vacuous as a *policy* lever, because the actor can rarely prove the
  half-suit is his team's. The gap between the ground-truth rate (16.5%, `P0-v04-pathology.md`)
  and the provable rate (0.34%) is ~50×. In v0.4-vs-v0.3 the provable rate is 6× higher and v0.4
  uses it in only 17.8% of the cases where it exists, so its apparent 88.6% "usage" in the
  mirror is an artefact of having nothing else to ask (31.3% forced).
- **(C)** blackballing and Construction E need the actor to be able to choose *who* gets the
  turn. It can, at nearly half of all decisions.
- **(E)** v0.4 *does* have concealment terms — `f[9]` (weight −0.854) and `f[19]` (−0.999) at
  `v04.hpp:318,328` penalise asking in a half-suit the team has not publicly touched. They are
  outweighed: with ~6 live half-suits, chance would break concealment ~17% of the time; v0.4
  breaks it 38–43% of the time, because `f[3]` own-set-progress (+1.688), `f[5]` lock-completion
  (+4.071) and `f[7]` completion bonus (+1.428) all point the other way in exactly the position
  where you hold ≥4 of a half-suit.

---

## 4. Technique table

Legend for column 3: **No** = no state or term exists; **Weak** = a related term exists but is
dominated or measures the wrong thing; **Yes** = genuinely expressible.

| # | Technique (source) | Legal here? | Expressible in v0.4? | Evidence | Mechanism v0.5 needs |
|---|---|---|---|---|---|
| 1 | **Ask purely to tell your partner what you hold** — the C5 certificate as a signal (D13, P3, W1) | Yes | **No** | The 20 ask features `v04.hpp:309-328` contain no term for information delivered to teammates; three terms price leakage to opponents (`f[9]`, `f[16]`, `f[19]`) and none prices gain to the team | An ask score $u = \ldots + \nu\,\Delta I_{\text{team}} - \lambda\,\Delta I_{\text{opp}}$; $\Delta I_{\text{team}}$ needs a *teammate-side* belief object (public knowledge + posterior over the teammate's hand) |
| 2 | **Deliberate safe ask, known to miss, to pass the turn or signal** (P3; §7.9 Construction E) | Yes (`fish.hpp:158-166` tests hands, not beliefs) | **No** as a choice | Hit probability dominates the linear score (`w[0]=11.506`, `w[1]=3.295`, `v04.hpp:71-72`) and `askExpectedValue` reduces the ask to `p·v_hit + (1-p)·v_miss` (`v04.hpp:434-457`). v0.4 makes dead asks 36.6% of the time but as inference failure, not plan: `P0` shows switching to the exact belief cuts them 39%→28% with the deadlock intact | An explicit *costly-signal* action class scored on message value, plus a small codebook of high-value messages ("I hold ≥4 of $h'$; prepare to declare") |
| 3 | **Which opponent to ask, when several are equivalent** (gamerules.com; §7.9 Construction B) | Yes | **No** | `askExpectedValue(pub, card, target, p)` begins `(void)target;` (`v04.hpp:435`). The only target-dependence anywhere is through `p`, `threatOf`, `exposureOf`, `handCount[target]` (`v04.hpp:317-320`) — all material | 0.64–0.92 free bits/ask measured (§3A). Needs: a target-selection layer that, among targets within $\epsilon$ of the best hit probability, picks by codebook — and a teammate decoder that reads it |
| 4 | **Which card within a half-suit, as a secondary channel** (P9 Ali Salahuddin; P6 asking order; §7.9 Construction A) | Yes; the *pre-agreed* form is disputed (D16) | **Weak** | `f[12]` rewards repeating the same *half-suit* (`lastMySet == S`, `v04.hpp:321`, weight +1.270) — it does not encode anything, and it rewards the opposite of P6's asking-order rule, which says exhaust one *rank across opponents* before switching card | Card-index code over the not-yet-common-knowledge cards $\mathcal U_h$; plus an explicit P6/P7 ordering term (they conflict — test jointly, per H6/H7) |
| 5 | **Deliberately not asking in a half-suit you intend to declare** (D6 lie low, W1) | Yes | **Weak** | `f[9]` (−0.854) and `f[19]` (−0.999) do penalise entering an unrevealed half-suit (`v04.hpp:318,328`), but the completion terms `f[3]/f[5]/f[7]` (+1.688/+4.071/+1.428) dominate; measured concealment break rate 38–43% vs ~17% chance (§3E) | Re-weight, or gate: a hard "do not open a half-suit where my team is already ≥4 and unrevealed unless the ask completes it" rule |
| 6 | **Inferring from what an opponent did NOT ask** | Yes | **Weak** | It exists: `priorWeight` weights the deal prior by $\exp(\theta\,a_{p,H} - \phi(\text{asks}_p - a_{p,H}))$ (`belief.hpp:97-105`) with $\theta=0.264,\ \phi=0.133$ (`v04.hpp:61-62`). But it is a **single global pair of scalars, identical for all five other players, never updated in-game** — a deliberately silent opponent (the owner's manoeuvre, BRIEF §3) is misread by construction | Per-opponent, in-game-updated $(\theta_p,\phi_p)$; at minimum a two-component mixture over "ordinary" and "concealing" opponent types with online responsibility updates |
| 7 | **Turn-transfer choice when cardless, incl. exchanging willingness bits** | **Yes, and the engine is more restrictive than the rules** | **No** for the willingness exchange | `Agent::choosePassTarget(pub, cand, n)` (`game.hpp:29`) receives no teammate input; `game.hpp:302` calls it and takes the answer. v0.4's implementation (`v04.hpp:797-819`) scores each candidate teammate by *its own* belief `bel.marg[c][u]` about that teammate's hand | A willingness ladder exactly like `Rules::forcedTh` (`fish.hpp:126-127`) — sweep thresholds, each teammate answers a single bit "I can use the turn at confidence ≥ θ". The machinery already exists in `forcedEndgame` (`game.hpp:235-266`) and is simply not wired here |
| 8 | **Keeping your last card of a half-suit to preserve asking rights** | **Not available.** A hit transfers the card unconditionally (`game.hpp:350`); you cannot decline | n/a | — | The legal cousin is #9 |
| 9 | **Void creation — keep hitting the same opponent to make them void in a half-suit** (P7) | Yes | **No** | `f[11]` "empties target" fires only at `handCount[target] == 1` (`v04.hpp:320`) — that is emptying the whole *hand*, not voiding a *half-suit*. No feature counts the target's remaining cards in $H$ | A per-(target, half-suit) void-progress feature; a void opponent can never legally ask in $H$ again (`fish.hpp:158-166`), which is a permanent, provable gain |
| 10 | **Blackballing / lockout — never grant the turn to a dangerous opponent** (D3, P4, W2) | Yes | **Weak** | `f[8] = (1-p)·threatOf(target)`, weight −3.098 (`v04.hpp:317`, `v04.hpp:210-227`). But `threatOf` estimates the target's **cards**, with `activity = askCount/3` as a crude proxy for knowledge. Develin's blackball turns on the opponent **knowing** where the last card is. v0.4 builds no `Knowledge` for any seat but its own — the only two clones are of `k` itself (`v04.hpp:507,524`) | A per-opponent knowledge model. It is cheap: the transcript is public, so opponent $j$'s knowledge = the public deduction state + a posterior over $j$'s hand (`fish-prior-art.md` §9 pitfall 13) |
| 11 | **Ask the asker / teammate's ask is a beacon** (D5, D11) | Yes | **Yes** (inference side) | The C5 certificate is applied on every ask including teammates' (`belief.hpp:154-158`), and $\theta$ raises the prior for any player who asked in $H$ (`belief.hpp:97-105`) | Nothing new needed for the inference; the *action* side ("ask for exactly the card my teammate just missed on") falls out of a correct posterior |
| 12 | **Avoid extended back-and-forth with one opponent** (P8) | Yes | **No — v0.4 is pushed the wrong way** | `f[12]` rewards asking again in the same half-suit you last asked in (`v04.hpp:321`, weight **+1.270**). `P0` measures **40.0% exact repeat asks** (same actor, card, target, same game) in the mirror | A cap or penalty on consecutive same-pair exchanges, and a leak term that counts the *joint* information the exchange broadcasts to the other two opponents |
| 13 | **Declaring early to deny information vs. late to keep options** (D8/P1/W3 hold; CardRules+ claim now) | Yes (`fish.hpp:108`) | **Yes** | `patientLocked = true` (`v04.hpp:96`); `declareByValue` is an optimal-stopping test against the learned value function (`v04.hpp:653-671`); `declareNow` (`v04.hpp:673-687`) | This is §7.9 Construction D and it **was built**. Gap: it prices the declaration only through the aggregate value function; it has no term for the allocation information the declaration hands the opponents |
| 14 | **The stalemate-breaker — hold a fully-attributed set in reserve, cash it to move control** (W3, D8, D15) | Yes | **No** | `declareByValue` (`v04.hpp:653-671`) computes `vDeclare` with `dOur = -SETSZ`, but `value()` (`v04.hpp:370-403`) takes **no delta on `myCards` (`f[10]`) or `minFriendly` (`f[11]`)** — they are read from the cached aggregates at `v04.hpp:390-391`. So the stopping rule literally cannot see that this declaration empties a hand, which is the entire point of the manoeuvre | Perturb `myCards`/`minFriendly` in `value()`; then add the option value of *becoming cardless and choosing the receiver* — i.e. couple #7 and #14 |
| 15 | **Declaring to protect a teammate about to run out of cards** (P11-adjacent) | Yes | **No** | Same defect as #14: no per-teammate card-count delta in `value()` | Same fix |
| 16 | **Deliberately running the opponents out of cards** | Yes, but it is usually a **blunder under these rules** | **Yes — and plausibly mis-signed** | `f[11] = (handCount[target] == 1) ? p : 0` with weight **+1.166** (`v04.hpp:82,320`) rewards taking an opponent's last card. Emptying *one* opponent is good (they cannot be asked, so they cannot receive the turn). Emptying *all three* triggers the forced endgame, in which `P0` measures forced declarations **100% wrong (28/28)** | The feature must distinguish "empty this opponent" from "empty the last live opponent". Split it: reward when ≥2 opponents remain live, penalise hard when this ask would leave the opposing team cardless and our own allocation confidence is below the forced-endgame bar |
| 17 | **Coordinating so the best-informed teammate declares** (D14 most-informed vs W4 most-carded) | Yes; confidence may not be discussed (Srinivasan's "No Probabilistic Information", P13) | **No for voluntary declarations** | `declarationRound` polls seats in fixed order and takes the **first** willing declarer — `Rules::declArbitration = 0`, lowest seat (`fish.hpp:119`, `game.hpp:210-223`). The stated confidence is captured and then discarded as a diagnostic (`game.hpp:222-224`) | Extend the willingness ladder (`fish.hpp:126-127`, already used in `forcedEndgame`) to voluntary declarations: sweep thresholds descending, first seat willing at the current threshold declares. Recovers most of "most confident declares" while exchanging one bit. This is task **P6** |
| 18 | **Baiting an opponent into an ask that reveals their hand** | Yes | **No** | No source names it directly; P8 is the closest. v0.4 has no opponent policy model at all beyond the two scalars $\theta,\phi$ — it cannot predict any opponent's reply, let alone induce one | Requires the per-opponent policy model of #6 plus at least a two-ply search over the opponent's reply. Highest cost on this list; lowest priority |
| 19 | **Exhaust one rank across all opponents before switching rank** (P6) | Yes | **No** | Nothing in `features()` (`v04.hpp:286-329`) references the previous *card* asked; only the previous *set* (`lastMySet`, `v04.hpp:321`) | A leak term over the actor's own revealed-void pattern within $H$. Note it conflicts with #9 (P7 void creation); `fish-prior-art.md` H6/H7 already flags they must be tested jointly |
| 20 | **Memory triage / focus on half-suits you hold** (D1, D2, D7, P5) | Yes | n/a — a bot has perfect recall | `Knowledge` tracks all 54 cards unconditionally (`belief.hpp:46-76`) | Nothing. `fish-prior-art.md` H13 correctly calls this the null control: it should have **zero** effect on a bot |
| 21 | **Randomising / pooling on high-harm asks** (§7.9 Construction C) | Yes | **No** | `chooseAsk` is a deterministic argmax (`v04.hpp:474-483`, `v04.hpp:500-547`); the agent's `rng` is used **only** for belief particle sampling (`v04.hpp:196`, `v04.hpp:638`), never for action selection | A deterministic policy is also maximally exploitable by a human who has played 50 games against it — which is exactly the owner's report #3/#4. Sample from a softmax over near-optimal asks, with temperature raised specifically where revealing $c$ would let a named opponent execute a guaranteed take |

---

## 5. §7.9 Constructions A–E: did v0.4 build them?

| | Construction | Built in v0.4? | Evidence | Worth building now? |
|---|---|---|---|---|
| **A** | Receiver-relative card code (positive secrecy rate) | **No** | No codebook of any kind in `v04.hpp`; a case-insensitive grep for `signal\|convention\|infoGain\|mutual` over `v04.hpp` returns **0** hits | **Later.** It needs a per-teammate hand posterior (which #1/#10 will build anyway) and it is lossy from the sender's side. Build A *after* B works, and only for coarse facts ("I have length in $h'$") as §7.9 warns |
| **B** | Teammate selection by target seat | **No** | `askExpectedValue` discards `target` (`v04.hpp:435`); the linear score's only target terms are material (`v04.hpp:317-320`) | **Yes — build first.** Measured 0.64–0.92 free bits/ask at 0.08 hit-probability cost (§3A). Cheapest capability-per-line on the whole list, and it is exactly the "which opponent to ask" channel gamerules.com names |
| **C** | Deliberate pooling on high-harm dimensions | **No** | Deterministic argmax; `rng` never reaches action selection (`v04.hpp:196,638` are its only uses) | **Yes — build second, for a different reason than §7.9 gives.** Its stated motive is leak reduction; its bigger payoff here is de-exploitability. Per the owner's standing preference, robustness across playstyles is the research question, and a deterministic policy has none |
| **D** | Declaration as optimal stopping on the PBS | **Yes** | `declareByValue` (`v04.hpp:653-671`), `declareNow` (`v04.hpp:673-687`), `patientLocked` (`v04.hpp:96`), `pressure` escalation (`v04.hpp:575-586`) | **Repair, don't rebuild.** Two defects: (i) `value()` has no `myCards`/`minFriendly` delta, so it cannot see a declaration emptying a hand — killing techniques #14 and #15; (ii) per BRIEF, the 16 `vw` coefficients compiled at `v04.hpp:111-128` are not the fitted ones in `E14-valuefit.txt` |
| **E** | The safe ask as a costly signal | **No** | See #2 above | **Yes — build alongside B.** This is the direct deadlock fix the BRIEF hypothesises. But see §6: it must be priced, not free, because the D13 "free" precondition is rarely provable |

---

## 6. Hypotheses that did NOT hold

1. **"The D13 free channel is plentiful."** It is not, once you require the actor to be able to
   *prove* it. Ground truth 16.5% of asks (`P0-v04-pathology.md`) vs **0.34%** of decisions
   provable from the actor's certificates (§3B) — a ~50× gap. Caveat on my own measurement:
   `provablyTeamOwned` (`probe_human.hpp`) uses only the hard certificate masks; a
   degree-constrained-flow / Hall oracle over the transportation polytope (`fish-prior-art.md`
   §6.2) would prove strictly more, and closing that gap is itself a v0.5 work item. But the
   direction is unambiguous: **a v0.5 ask rule that only signals when the channel is provably
   free will almost never signal.** The channel has to be priced.
2. **"v0.4 has no concealment term."** False — it has two (`f[9]`, `f[19]`). They are simply
   outweighed by the completion features in exactly the position where concealment matters.
   The fix is re-weighting or a gate, not a new feature.
3. **"Target choice is at least a tie-break in v0.4's score."** Worse than expected: the
   value-function half of the score does not see the target at all (`v04.hpp:435`).
4. **"The human corpus has grown since the v0.4 review."** It has not. Every strategy claim
   available on the open web still traces to McLeod (pagat), Develin, or Wikipedia, plus
   derivative summaries. CardRules+ is the only new document, it is thin, and it contradicts the
   primary sources on claim timing.

---

## 7. Ranked recommendation for v0.5

1. **Give the agent a knowledge model of the other five seats.** Public transcript + a posterior
   over each player's hand. It is nearly free (the transcript is public) and it is the
   prerequisite for #1, #10, #17, #18 and for decoding any signal at all. Without it, every
   remaining item is a heuristic.
2. **Price the ask, don't just score the hit.** $u = \text{material} + \nu\,\Delta I_{\text{team}}
   - \lambda\,\Delta I_{\text{opp}}$, with the target dimension exposed to the optimiser
   (delete the `(void)target;`).
3. **Construction B + E together**: a codebook on the target dimension for cheap messages, and a
   costly safe-ask class for expensive ones. Measured capacity: ~41 bits/team/game unused.
4. **Wire the willingness ladder into `choosePassTarget`** (#7) and into voluntary declaration
   arbitration (#17). The rules permit both; `Rules::forcedTh` already implements the pattern.
5. **Repair `value()`** to perturb `myCards` and `minFriendly` (#14, #15), and recompile the
   fitted `vw`.
6. **Fix `f[11]`'s sign asymmetry** (#16): emptying one opponent is good, emptying the last one
   is a 100%-wrong forced endgame.
7. **Add stochasticity to action selection** (Construction C) and re-run the exploitability probe
   per the owner's standing preference.

---

## 8. Reproduction

```
cd "/Users/dylan/Documents/GitHub/fish optimization/engine" && make
./fish humanchan --a=v04 --b=v04 --games=600 --seed=31
./fish humanchan --a=v04 --b=v03 --games=600 --seed=90210
```

New files: `engine/src/probe_human.hpp`; `humanchan` command block appended at the end of
`engine/src/main.cpp` with its include at `main.cpp:12`. No protected header was modified.

Note at time of writing `make` fails on `src/probe_policy.hpp:153` (`no member named 'pub' in
'fish::Game'`), a concurrently-edited file belonging to another task. The numbers above were
produced from a standalone translation unit including only `probe_human.hpp`:

```
c++ -std=c++20 -O3 -march=native -Isrc <scratch>/p5main.cpp -o <scratch>/p5 -pthread
```

Byline: Dylan Nguyen, FishLab Research Project.
