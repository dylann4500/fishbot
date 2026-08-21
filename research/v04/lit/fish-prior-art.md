# Prior Art: Computational Agents and Human Strategy Literature for Canadian Fish / Literature

**Research area:** Existing Fish / Literature / Go-Fish computational agents, and the human strategy literature.
**Date of survey:** 2026-08-21.
**Target game:** 6 players, 2 teams of 3, alternating seats, 54 cards, 9 half-suits of 6, 9 cards dealt each, public asks/transfers/declarations, declare-at-any-time, any error forfeits the set to the opponents.

---

## 1. Executive Summary

1. **There is no published academic work on Literature / Canadian Fish.** Exhaustive arXiv API queries (`all:"Canadian Fish"`, `all:"Literature card game"`, `abs:"Go Fish"`, `all:"half-suit"`) returned **zero** entries. No AAAI/IJCAI/NeurIPS/ACM paper, no thesis, no course project was found. Any competent agent built for this project is, as far as this survey can determine, state of the art by default.
2. **The only real computational agent is `neelsomani/literature`** (Python, MIT licence, 10 GitHub stars). It is a complete rules engine plus a trained `sklearn.neural_network.MLPRegressor`. I cloned it and read every line of source plus disassembled the shipped pickle.
3. Its network is tiny and vanilla: **input 1149 → hidden 100 (ReLU) → 1 (identity)**, Adam, `alpha=1e-4`, `lr=1e-3`, 115,101 parameters. Trained via `partial_fit` for `n_iter_ = 1,030,945` steps over `t_ = 2,001,821` samples (read directly out of the pickle opcodes).
4. **It is not Q-learning despite the README's claim.** There is no bootstrapping, no discount, no `max_a' Q(s',a')`. It regresses a hand-written score directly onto `(state, action)` features. It is Monte-Carlo reward regression at best.
5. **It contains a fatal reward bug.** `_team_for_move` stores `int` team ids; `game.winner` returns a `Team(Enum)` member. `0 == Team.EVEN` is `False` in Python for a plain `Enum`. Therefore *every* move in *every* game receives the terminal reward `-100`. I verified this by reproducing the enum comparison. The published `model_10000.out` has learned only the immediate ±20 ask-success signal plus a constant −100 offset.
6. **It never learns to declare.** `commit_claim` carries the comment that incorrect claims are not penalised "since the bots will never make a claim with uncertainty." Declarations are emitted by a hard-coded rule (`Player.evaluate_claims`) that fires the instant all six cards of a half-suit are *provably* on your team, for every player, greedily. All of the interesting declaration strategy in the human literature — delayed declares, stalemate-breakers, coin-flip declares to transfer control — is structurally impossible in that codebase.
7. Its per-move reward `+20` on a successful ask / `−20` on a failed ask **actively punishes** the single most-recommended human tactic (deliberately failing an ask to inform your partner). It also only trains on 4-player games and it caps games at 200 moves, discarding the tail.
8. **Its deduction engine is genuinely good and worth stealing.** `Player._memorize` runs a fixed-point constraint propagation with four sound inference rules (suit-lower-bound, complete-half-suit closure, hand-count closure, unique-holder closure), plus **first-order theory of mind** via `dummy_players`: player *i* maintains a full simulated `Player` object for every *j*, giving "what I know that *j* knows."
9. Its deduction engine is nonetheless **incomplete**: the local rules do not capture Hall's-condition / max-flow feasibility. A degree-constrained bipartite flow with lower bounds subsumes all four of their rules and strictly dominates them. This is cheap (<1 ms) and is my top recommendation.
10. **All other repos are UIs with no agent.** `cjquines/cfish` (TypeScript, MIT-adjacent community, no AI files), `Dynosol/playfish.io`, `zairza-cetb/literature`, `play-litaf.onrender.com`, `whaatt/Literature`, `liamcolangelo/fish` — verified via the GitHub trees API; none contains a bot. `doubleiis02/CanadianFish` has a `Bot.java` that is a stub: it allocates four uniform probability vectors at `1/41` or `1/40` per card and never updates them.
11. **The human strategy literature is rich, specific, and almost entirely untested.** Three primary sources: John McLeod's pagat.com page (the canonical rules + a substantial Tactics section), Mike Develin's *Canadian Fish* chapter (the deepest strategic writing found, and it describes exactly the 6-player, declare-anytime, error-forfeits variant this project uses), and Wikipedia/derivatives (blackballing, stalemate-breaker).
12. Two heuristics dominate every source: **lockout / blackballing** (never grant the turn to a dangerous opponent) and **declare only at certainty** (a set your team fully holds cannot be stolen, because opponents have no base card to ask from).
13. Develin supplies the only **worked quantitative example** in the whole literature: a 2-suits-remaining endgame where declaring at 50% confidence to transfer control strictly dominates asking to improve information. His arithmetic implies the alternative would need >150% success probability to be preferred — which reduces exactly to the rule `declare iff q > 1/2` under ±1 set scoring.
14. Signalling conventions exist in the wild but the sources **disagree on legality**: pagat documents Ali Salahuddin's convention as an accepted partner-signalling agreement, whereas Develin states that pre-agreed conventions are explicitly forbidden in Canadian Fish. This is a design decision your project must make explicitly, because self-play will invent conventions whether you want it to or not.
15. **The Fish-specific structure nobody has exploited:** because every ask, answer, transfer, declaration and hand count is public, the *only* hidden variable is the initial deal. That makes the belief state a **transportation polytope over a bipartite graph**, exactly enumerable-by-counting, and it makes the game a *public-belief-state* game in the Brown-Bakhtin sense with a 10^28-sized but *structurally trivial* information set. No prior Fish agent exploits this.

---

## 2. The Only Real Agent: `neelsomani/literature`

Repository: https://github.com/neelsomani/literature (MIT licence, Python 3.6+, ~1,940 LOC).
Companion server: https://github.com/neelsomani/literature-server (React + Flask + gevent; no agent code).
Playable at https://literature.neelsomani.com/ (linked from pagat.com).

I cloned at commit `3d2c75c` ("Update to latest Python version") and read all sources. Git history is 11 commits; the entire agent landed in one commit, `5ff47f3` "Add neural net training, bots, and interactive gameplay".

### 2.1 Game variant implemented

48 cards (8s removed), 8 half-suits (`MINOR = {A,2,3,4,5,6}` as ranks 1–6, `MAJOR = {8,...,13}`), *n* players where `n | 48`. Teams are **even vs odd `unique_id`**, i.e. alternating seats, matching the real game. Notably the code encodes rank 1 = Ace as a MINOR card and rank 7 is excluded, i.e. the "remove the 7s" variant listed on pagat. Training is hard-coded to `get_game(4)`.

This is a **material mismatch with your target game**: 4 players means 1 teammate (Develin explicitly says a 4-player game "would be too easy — as soon as your team collected a set it would be easy to make a claim, knowing that your one partner must have all the missing cards"). Declaration allocation is trivial with one partner (2^6 = 64 allocations, and usually forced); with two teammates it is 3^6 = 729 allocations per half-suit. **The hardest sub-problem of your game is absent from the only prior agent.**

### 2.2 State encoding (exact)

Per `Player.serialize()`, for a game with `n` players, `C` cards and `H` half-suits, the block for one player is

```
1                                     # own unique_id
+ n * ( C                             # knowledge[p][card] ∈ {0,1,2}
      + H                             # suit_knowledge[p][halfsuit] ∈ ℕ (a LOWER BOUND)
      + 1 )                           # n_cards[p]
```

and then the same block is appended once for each of the `n` **dummy players** (each dummy has no dummies of its own). So

$$d_{\text{player}}(n, C, H) = \big(1 + n(C+H+1)\big)\cdot(n+1)$$

For their case $n=4, C=48, H=8$: $d = (1 + 4\cdot 57)\cdot 5 = 229\cdot 5 = 1145$. The action is appended as 4 raw integers `[interrogator_id, respondent_id, suit, rank]`, giving

$$\texttt{MOVE\_LENGTH} = 1145 + 4 = 1149$$

which matches the constant in `learning.py` exactly. For your game ($n=6, C=54, H=9$) the analogous encoding would be $(1 + 6\cdot 64)\cdot 7 + 4 = 385\cdot 7 + 4 = \mathbf{2699}$ dimensions.

Three encoding sins worth noting before you copy anything:

- `knowledge` is encoded as a **raw ordinal 0/1/2** rather than one-hot. `MIGHT_POSSESS = 2 > DOES_NOT_POSSESS = 1 > DOES_POSSESS = 0`, so the network sees an ordering that is semantically backwards.
- The action is encoded as **raw integer ids**, so `Suit.SPADES` (4) is "twice" `Suit.DIAMONDS` (2) to the first layer. There are no embeddings and no permutation-equivariance over suits, which are exchangeable in this game.
- Player identity is absolute, not relative to the actor, so the network cannot share weights across seats.

### 2.3 The deduction engine (the good part)

`Player._memorize(ConcreteKnowledge)` runs a recursive fixed-point over four sound rules. Written as inference rules over the certainty relation, with $x_{p,c}\in\{\text{YES},\text{NO},\text{MAYBE}\}$, $\ell_{p,H}\in\mathbb{N}$ the *minimum* number of cards player $p$ holds in half-suit $H$, and $n_p$ the public hand size:

**R1 — suit lower bound from certainty** (`_update_suit_knowledge`):
$$\ell_{p,H} \leftarrow \max\Big(\ell_{p,H},\ \big|\{c\in H : x_{p,c}=\text{YES}\}\big|\Big)$$

**R2 — half-suit closure** (`_deduce_holds_remaining`): if the number of $H$-cards $p$ certainly lacks, plus the lower bound, saturates the half-suit,
$$\big|\{c\in H: x_{p,c}=\text{NO}\}\big| + \ell_{p,H} = |H| = 6 \ \Longrightarrow\ x_{p,c}=\text{YES}\ \forall c\in H \text{ with } x_{p,c}\neq \text{NO}$$

**R3a — hand-count closure by half-suit** (`_identify_complete_info`): if the lower bounds already account for the whole hand,
$$\sum_{H} \ell_{p,H} = n_p \ \Longrightarrow\ x_{p,c}=\text{NO}\ \ \forall c \text{ in any suit where } p \text{ has no known card}$$

**R3b — hand-count closure by card**:
$$\big|\{c: x_{p,c}=\text{YES}\}\big| = n_p \ \Longrightarrow\ x_{p,c'}=\text{NO}\ \forall c' \text{ with } x_{p,c'}\ne\text{YES}$$

**R4 — unique holder / exclusivity** (`_infer_about_others`):
$$\big|\{p' : x_{p',c}=\text{NO}\}\big| = m-1 \ \Longrightarrow\ x_{p,c}=\text{YES} \text{ for the remaining } p$$
$$x_{p,c}=\text{YES} \ \Longrightarrow\ x_{p',c}=\text{NO}\ \ \forall p'\ne p$$

**Move-observation update** (`memorize_move`), for `Move(i asks j for c)`:
$$\ell_{i,H(c)} \leftarrow \max(\ell_{i,H(c)},1) \quad\text{(legality: asker must hold a base card)}$$
- on success: $\ell_{i,H}\!+\!\!=\!1$, $\ell_{j,H}\leftarrow\max(0,\ell_{j,H}-1)$, $n_i\!+\!\!=\!1$, $n_j\!-\!\!=\!1$, $x_{i,c}=\text{YES}$, $x_{j,c}=\text{NO}$
- on failure: $x_{i,c}=\text{NO}$ (asker cannot ask for a card they hold), $x_{j,c}=\text{NO}$

**Theory of mind** (`_inform_dummy_players`): every `Player i` owns `dummy_players[j] : Player` for all `j`, each running the same propagation on the public transcript. This gives first-order beliefs, "what I know that *j* knows". The README explicitly stops there, arguing higher orders explode dimensionality and are not worth much. Note that because *everything* is public in this game, the dummies actually only differ from the true public state by **the dummy's own private hand**, which they never populate (`hand=[]`). So the dummies as implemented are really just a *public-knowledge* replica, replicated `n` times — 4/5 of the 1149-dim input is redundant. That is a large, free saving for your encoder.

### 2.4 Action selection and "learning"

Move enumeration (`GameHandler._get_valid_moves`) builds the Cartesian product over all opponents × all 48 cards, filtered by `Player.valid_ask`, with a two-stage fallback:

1. First pass with `use_all_knowledge=True`: exclude any ask where `knowledge[respondent][card] == DOES_NOT_POSSESS`.
2. Only if that set is empty, re-enumerate with `use_all_knowledge=False`.

This is the "Limitations" bullet in the README and it is a **hard-coded prohibition on the deliberate-failure tactic** that pagat and Develin both recommend. Legal ask-space upper bound is $|\text{opponents}|\times C$; for their 4-player game 96, for your 6-player 54-card game $3\times 54=162$; after legality filtering typical branching is on the order of 15–40.

Selection is greedy-with-Gaussian-jitter:
$$a^\star = \arg\max_{a\in\mathcal{A}} \Big[ f_\theta\big(\phi(s)\,\|\,\psi(a)\big) + \eta \Big],\quad \eta\sim\mathcal{N}(0,1)$$

Since rewards are on a ±20/±100 scale, $\mathcal{N}(0,1)$ jitter is negligible: this is **effectively pure greedy**, with essentially no exploration. The README states the noise exists to break infinite loops, not to explore.

Training targets (`run_full_game`):
$$y_t^{\text{immediate}} = \begin{cases} +20 & \text{ask succeeded}\\ -20 & \text{ask failed}\end{cases}$$
applied as an online `partial_fit` on the single sample $(\phi(s_t)\|\psi(a_t))$, and then, at game end, a second `partial_fit` over the whole stored batch with
$$y_t^{\text{terminal}} = \begin{cases} +100 & \text{team}(t) = \text{winner}\\ -100 & \text{otherwise}\end{cases}$$

### 2.5 The reward bug (verified)

```python
_team_for_move.append(self.game.turn.unique_id % 2)     # int
...
move_scores = [GAME_MAGNITUDE if t == self.game.winner  # Team(Enum)
               else -GAME_MAGNITUDE
               for t in _team_for_move]
```

`Team` is a plain `Enum`, not `IntEnum`. I reproduced the comparison: `0 == Team.EVEN` → `False`, `1 == Team.ODD` → `False`. Therefore `move_scores` is the constant vector $-100$ for every game.

Consequences:
- The published `model_10000.out` has **never received a win/loss gradient**. It is a supervised model of "will this ask succeed", offset by a constant.
- The regression is minimising $\mathbb{E}[(f_\theta - y)^2]$ against a bimodal target with the two phases interleaved, so the fitted value is roughly $f_\theta \approx \tfrac12(\pm20) + (-100)$ — dominated by the constant and by the ask-success signal.
- This also means the "self-play trained Literature bot" that a naive comparison would treat as a baseline is, functionally, **a greedy maximum-likelihood asker**. That is still a non-trivial baseline (greedy asking is decent in Fish), but it is not an RL result.

### 2.6 Other structural weaknesses

| Issue | Where | Effect |
|---|---|---|
| Declarations are greedy & unconditional | `GameHandler.make_claims` loops all players every step | No delayed declare, no stalemate-breaker, no control-transfer declare, no risky declare |
| No penalty modelled for wrong claims | `commit_claim` docstring | The bot can never be trained to reason about declaration risk |
| Turn-passing when cardless is **random** | `_switch_turn_if_no_cards`, `switch_turn` use `random.random()` | Discards a real decision that the human literature says is important |
| 200-move truncation returns without training | `run_full_game` early-return | Long, hard games contribute nothing |
| Threads share one `MLPRegressor` behind a `Lock` | `Model.train` | Serialised training; the `--cores` flag mostly buys nothing |
| Online `partial_fit` on single correlated samples | everywhere | Catastrophic forgetting; Adam moments dominated by the last trajectory |
| No opponent/teammate policy model in the value function | — | Cannot reason about signalling at all |

### 2.7 Reported empirical results

**There are none.** `compete_n_times(n_games, first, second)` exists and returns `{first_wins, second_wins, ties}`, and `compete_models` returns `Team.NEITHER` for games killed at 200 moves — but no numbers are published in the README, in the commit messages, or in any linked write-up. I searched for a companion blog post by the author (neelsomani.ai, neelsomaniblog.com) and found none on this project. **Treat "the Somani bot's strength" as UNVERIFIED — it has never been measured in public.**

---

## 3. Other Implementations Found (all verified via GitHub trees API)

| Repo | Lang | Stars | Agent? | Notes |
|---|---|---|---|---|
| `neelsomani/literature` | Python | 10 | **Yes** | Analysed above |
| `neelsomani/literature-server` | JS/Python | 4 | No | Web front-end for the above |
| `zairza-cetb/literature` | Dart | 30 | No | Flutter multiplayer client, 4–12 players |
| `cjquines/cfish` | TypeScript | 4 | No | 35 files, none AI-related; README wishlist mentions only "predict actions client-side" |
| `Dynosol/playfish.io` | TypeScript | — | No | React/Vite/Firestore; linked from pagat |
| `doubleiis02/CanadianFish` | Java | 0 | **Stub** | `Bot.java` holds four `ArrayList<Double>` probability vectors initialised to $1/41$ or $1/40$ per unseen card and never updated. Uniform-prior scaffolding only. |
| `david-amirault/fish` | Java | 0 | Marginal | `AIDummyController.java`, `AIDummyGame.java`; the other 64 java files are AP-CS starter code |
| `Ryan1729/canadian-fish` | Rust | 1 | Unclear | Single-player terminal version; I grepped `main.rs` and found only terminal-plumbing functions. Opponent logic **UNVERIFIED** |
| `acavet/web-fish`, `liamcolangelo/fish`, `whaatt/Literature`, `Raghav-Sao/literature`, `onlymx13/fish`, `gyash24x7/littplay` | various | 0–2 | No | UI/servers only |
| `play-litaf.onrender.com` (Pattabiraman & Suri) | web | — | No | Human multiplayer only |
| "Literature: Fish Card Game" (Google Play, `com.cards.game.literature`) | Android | — | **Claimed** | Store listing advertises offline bots at 4 or 6 players, three difficulty levels, per-bot playing styles. Closed source. Algorithm **UNVERIFIED**. This is the only known 6-player Fish bot other than what you build. |

**Negative result of note:** GitHub search across `literature+card+game`, `canadian+fish+card+game`, `fish+card+game+ai`, `half+suit+card+game` surfaced nothing with meaningful search, planning, CFR, or RL for this game. The `Go Fish` repos that appear are the unrelated children's game.

---

## 4. Absence of Academic Literature (a Result in Itself)

arXiv API queries returning zero entries:
- `all:"Canadian Fish"` → 0
- `all:"Literature card game"` → 0
- `all:"half-suit"` → 0
- `abs:"Go Fish"` → 0

Web searches for Stanford CS221/CS229, Berkeley, MIT, Caltech course projects on Literature/Canadian Fish returned nothing specific. There is **no CFR result, no equilibrium analysis, no complexity result, and no benchmark** for this game.

The nearest *methodologically* relevant published work I verified while searching (full treatment belongs to other research areas, listed here only so you do not re-derive it):

- **Morenville & Piette, "Modeling Uncertainty: Constraint-Based Belief States in Imperfect-Information Games", arXiv:2507.19263 (2025).** Represents beliefs about hidden identities as a CSP and compares against a Belief-Propagation extension estimating marginals. Headline finding: constraint-based (purely logical) beliefs perform **comparably** to probabilistic inference, with minimal agent-performance difference. This is directly encouraging for Fish, where the constraint structure is unusually tight.
- **Zinkevich, Johanson, Bowling & Piccione, "Regret Minimization in Games with Incomplete Information", NeurIPS 2007** — the CFR foundation. Two-player zero-sum guarantees do not transfer to a 6-player 2-team game.
- **Zha et al., "RLCard: A Toolkit for Reinforcement Learning in Card Games", arXiv:1910.04376** — the standard harness; Literature is not among its environments.

---

## 5. The Human Strategy Literature — Full Extraction

Three primary sources, all read in full from the raw pages.

### 5.1 Mike Develin, *Canadian Fish* (chapter 9 of his card-games manual)

This is the single most valuable document found. **It describes exactly your variant**: exactly six players, two teams of three alternating, declare-at-any-time by any player regardless of turn, all six cards removed from play regardless of correctness, any error in *any way* gives the set to the opponents, declaration does not change whose turn it is except when it empties the declarer's hand (in which case *they alone* pick the teammate to receive control), players with no cards are out and may not declare, and when one team is cardless nothing more may be said and the other team must declare everything out.

Develin's tactical content, itemised:

- **D1 — Memory budgeting.** You cannot remember everything, so choose. Perfect information about a few half-suits usually beats imperfect information about all of them.
- **D2 — Focus on suits you hold.** You can never acquire cards in a half-suit you have none of, so spend memory on the half-suits where you hold cards.
- **D3 — Blackballing / lockout, with the canonical worked example.** An opponent visibly holds 2–6♣ and has asked both you and your right-hand teammate for the 7♣ without success. If any opponent held the 7♣ they would already have declared, so your left-hand teammate must hold it. If that opponent ever gets a turn, they take the 7♣ and win the set. So never ask them a question — then it is never their turn. He notes explicitly that you cannot *say* this; your teammates must infer it.
- **D4 — Don't fully ignore half-suits you have no cards in**, precisely because blackball reasoning requires tracking opponents' near-complete sets.
- **D5 — Ask the asker.** If someone asks you about a half-suit where you hold other cards, it is often good to ask them back in that half-suit — at the cost of revealing that you hold a card there.
- **D6 — Lie low.** With many cards in a half-suit, sometimes the best play is to let others ask about it and accumulate enough information to place the whole set.
- **D7 — Clean out half-suits sequentially** to reduce memory load, since a declared half-suit can be forgotten entirely.
- **D8 — Don't declare a set you hold entirely.** Holding it makes you look card-rich, attracting questions away from teammates, and lets you declare later at a moment of your choosing to transfer control. If you hold all six yourself there is **no risk at all** in holding.
- **D9 — But do declare a set that is split across your team once you know it**, because the cost is teammates wasting questions on it.
- **D10 — If everyone knows you hold all six, holding gains nothing.**
- **D11 — Teammate's ask is a beacon.** If your teammate asks in a half-suit you also hold, you learn they hold a base card there; team up. In particular, if you don't have the card they asked for, ask for it yourself — your prior is better than theirs.
- **D12 — Declare only at certainty.** If your team holds all six they are not going anywhere; the only way to lose the set is a wrong declaration.
- **D13 — Asking is worth less than you think**, because your opponents learn as much from a failed question as your teammates do. **Corollary:** if you ask about a half-suit your team already fully holds, your opponents learn *nothing useful*, so this is the safe channel for conveying information to teammates.
- **D14 — Endgame declarer choice.** When only one team has cards and must declare, no more information arrives, so the player with the most information (usually, not always, the one with the most cards in the set) should declare. Often it comes down to a 50-50 guess.
- **D15 — Declare-to-transfer-control, with the only quantitative example in the literature.** Two half-suits remain (low clubs, high diamonds). You hold 4,5,6♣; your left-hand teammate has the 3♣; the 2♣ and 7♣ are split between your two teammates but you don't know which way. Your right-hand teammate holds the last diamond, and everyone knows where it is. **Right play:** guess the clubs at 50%, and regardless of outcome you are now cardless and transfer control to the diamond-holding teammate, who then acquires all remaining diamonds. Expected: diamonds with certainty and clubs 50% of the time. The alternative (ask the opponent for the 3♣ so a teammate can declare) loses the diamonds for certain. Develin observes the alternative would need to be worth "150%" to be preferred.
- **D16 — Conventions are explicitly forbidden** in his ruleset. You may infer, but you may not pre-agree that (e.g.) asking for a Jack implies holding the Queen or King.
- **D17 — Legality accident is catastrophic.** Under Develin's rules, asking for a card you hold, or in a half-suit you have no card in, immediately forfeits that half-suit to the other team.

### 5.2 John McLeod, pagat.com — the Tactics section

- **P1 — A set your team holds cannot be stolen back**, because the opponents have no base card in it and therefore cannot legally ask. So do not rush a declaration you are not 100% sure of.
- **P2 — But once you are certain, declare promptly** so teammates stop spending turns and attention on it.
- **P3 — Deliberately failing asks is sometimes correct**, purely to give teammates information.
- **P4 — Lockout ("locking someone out")**: when an opponent has a dangerous mix of cards and knowledge that would let them clean you out and declare, do not ask them anything, because a failed ask hands them the turn.
- **P5 — Memory triage:** if you are dealt nothing in a half-suit, or lose your last card in it, you will never get another card in it — save your attention.
- **P6 — Exhaust one rank across opponents before switching ranks (the asking-order rule).** If you fail to get the 2♥ from D and next ask D for the 3♥, you have told everybody that you hold neither the 2♥ nor the 3♥ but do hold some minor heart. Better to ask *all* opponents for the 2♥ first. This is a genuine information-leak-minimisation argument (see §6.5 for the formalisation).
- **P7 — Void creation.** Conversely, once you *do* get the 2♥ from D, it can pay to keep asking **that same opponent** for other minor hearts, aiming to make them **void** in that half-suit — a void opponent can never legally ask in it again, permanently securing it.
- **P8 — The back-and-forth is dangerous.** A worked example: A takes the 9♥ from D and then fails on the J♥; D can now recover the 9♥ and knows A lacks the J♥ but holds another major heart. D takes the Q♥, fails on the K♥; A can recover three cards and void D. The winner of a back-and-forth is unpredictable at the outset and the exchange broadcasts a huge amount of information, very likely making one of the *other* opponents dangerous. Think before starting or continuing one.
- **P9 — Ali Salahuddin's convention (partner signalling).** Teams A,B,C vs D,E,F. A asks unsuccessfully for the 2♥ — everyone now knows A holds a minor heart but not the 2♥. Then, on B's next turn:
  - B holds the 2♥ → B asks for a **different** minor heart (signals possession of the 2♥);
  - B holds minor hearts but not the 2♥ → B **continues** asking for the 2♥;
  - B holds no minor hearts → B asks about some other set entirely.
  Pagat notes the trade-off explicitly: it also informs the opponents.
- **P10 — Public information rules that constrain any bot:** any player may ask what the last question, asker and answer were; anything older is "History" and may not be discussed; any player may ask any other player (including a teammate) how many cards they hold and must be answered truthfully; **written notes are forbidden**. (An agent obviously has perfect memory — a fairness consideration for evaluation, not for the agent.)
- **P11 — Endgame protocol.** A cardless player cannot be asked, so the turn cannot be given to them. If a declaration empties your hand while it is your turn, you choose which teammate with cards receives the turn. Once one team is entirely cardless, **no more questions may be asked**; if the turn is with the team that still has cards, that player must declare all remaining sets alone without consulting partners; **if the turn is with the cardless team, that player chooses which member of the opposing team (who must hold at least one card) makes all the final declarations.** That last clause is strategically enormous: your opponents will pick your *worst-informed* player.
- **P12 — Variants that matter for your rules.** (a) 54-card deck with two Jokers, nine cards each, and the four 8s plus two Jokers forming a ninth set — **this is exactly your deck**. (b) Some play that any error in the declaration awards the set to the opponents even if your team held all six (**your rule**). (c) Some play that a wrong-turn declaration cancels the set, or awards it to the opponents if they hold any of it. (d) Some play that the last opponent who asked you a question, not you, chooses which teammate receives the turn when you are emptied.
- **P13 — Guy Srinivasan's proposed rules**, aimed at fixing two real degeneracies: **Forced Claims** (in a forced-declaration endgame, players take turns declaring or passing, moving left, and if nobody declares after each player has had twice as many chances as there are forced suits, all are forfeit) — introduced because players were leaking information by *discussing who should declare what*; **No Probabilistic Information** (during a forced claim you may communicate your confidence only by declaring or passing); and **Challenge** (a player may force the opposing team to treat a suit as a forced claim; if everyone passes twice, the challenger must name the locations of all six cards in the opponents' hands, winning the suit on success and losing it on failure). The challenge rule was introduced to give players an incentive to track cards held *entirely* by the other team, and to allow suit-stealing by bluff or brazen probability.

### 5.3 Wikipedia / gamerules / depositgenius family (derivative, but adds two named ideas)

- **W1 — Information asymmetry as the master principle.** Good strategy emits as much information as possible to teammates while emitting as little as possible to opponents — including not prematurely revealing which half-suits you hold at all.
- **W2 — Blackballing** (same as D3/P4), stated as one of the most important strategies, with the explicit reasoning that a responder only gets to ask if they *had* the requested card.
- **W3 — The "stalemate-breaker".** If your team knows a whole set is theirs and can attribute it correctly, **do not declare it**. Keep it in reserve. Later, when a teammate is on the verge of finishing a set but cannot get a turn, declare the reserved set to break the deadlock and move control. This is D8 with a name and an explicit trigger condition.
- **W4 — Endgame delegation heuristic** (depositgenius): near the end, have the player holding the most cards make the final declarations, on the theory that they have the most deduction data. Note this contradicts D14 slightly — Develin says *most information*, "usually but not always" the player with the most cards.
- **W5 — Advanced bluffing variant** (non-standard): some groups allow asking for a card you already hold, which destroys the cleanest deduction rule in the game. Not your ruleset, but note that if you ever face humans playing this variant your $x_{i,c}=\text{NO}$ inference on a failed ask becomes unsound.

---

## 6. Concrete Algorithms and Mathematical Formulations

Nothing in the prior art exploits the structural fact that makes Fish tractable. I set that out here in implementable form.

### 6.1 The belief state is a transportation polytope

Let $P=\{1,\dots,6\}$ be players, $\mathcal{C}$ the live (undeclared) cards, $\mathcal{H}$ the live half-suits. Define indicator variables $x_{p,c}\in\{0,1\}$. The set of worlds consistent with the public transcript $\mathcal{I}_t$ is the integer points of

$$
\mathcal{W}(\mathcal{I}_t)=\left\{ x \;\middle|\;
\begin{array}{ll}
\sum_{p\in P} x_{p,c} = 1 & \forall c\in\mathcal{C}\\[2pt]
\sum_{c\in\mathcal{C}} x_{p,c} = n_p & \forall p\in P \quad(\text{hand counts are public})\\[2pt]
x_{p,c} = 0 & \forall (p,c)\in \mathcal{N}_t \quad(\text{public denials, transfers, own hand})\\[2pt]
x_{p,c} = 1 & \forall (p,c)\in \mathcal{Y}_t\\[2pt]
\sum_{c\in H} x_{p,c} \;\ge\; \ell_{p,H} & \forall (p,H) \quad(\text{legality of past asks})
\end{array}\right\}
$$

Sizes for your game. Total deals:
$$|\mathcal{D}| = \frac{54!}{(9!)^6} \approx 1.011\times10^{38}$$
Worlds consistent with *your own hand only* (start of game):
$$|\mathcal{W}(h_i)| = \frac{45!}{(9!)^5} \approx 1.901\times10^{28}$$
(For the classic 48-card game these are $10^{33.5}$ and $10^{24.9}$.) The public transcript collapses this fast: every failed ask kills two $x_{p,c}$, every transfer fixes one, and hand-count changes are public.

**Counting worlds exactly.** Without the half-suit lower bounds, the count is a coefficient extraction. Group the unknown cards into classes by their *allowed-holder set* $S\subseteq P$; let $m_S$ be the number of cards in class $S$, and $k_p$ the number of unassigned slots for player $p$. Then

$$
N(k_1,\dots,k_6) \;=\; \Big[\textstyle\prod_p z_p^{k_p}\Big] \prod_{S} \Big(\sum_{p\in S} z_p\Big)^{m_S}
$$

There are at most $2^{|P|-1}=32$ classes from your seat, so this is a DP over capacity vectors of size $\prod_p (k_p+1) \le 10^5$, processed once per class: **$O(32\cdot 10^5\cdot 6)\approx 2\times10^7$ integer ops, i.e. milliseconds.** Marginals come from a forward–backward pass over classes:
$$\mu_{p,c} = \frac{N\big(x_{p,c}=1\big)}{N} \quad\text{for } c \text{ in class } S \ni p$$
The half-suit lower bounds $\ell_{p,H}\ge1$ break the class structure. Handle them by inclusion–exclusion over the (small) set of *active* lower bounds,
$$N_{\text{sat}} = \sum_{T\subseteq \mathcal{L}} (-1)^{|T|} N_{\text{relax}}(T)$$
where $N_{\text{relax}}(T)$ forbids all $H$-cards to $p$ for each $(p,H)\in T$ — which is again the same class-DP. Because $|\mathcal{L}|$ can reach ~15 mid-game, fall back to a **particle filter** when $2^{|\mathcal{L}|}$ gets large.

### 6.2 Certainty oracle via degree-constrained flow (strict improvement over §2.3)

Rules R1–R4 are *sound but incomplete*. The complete polynomial-time certainty test is feasibility of the above system with lower bounds, i.e. a **circulation with lower bounds**:

- source $s \to$ card node $c$, capacity $[1,1]$;
- card $c \to$ bucket $(p, H(c))$ if $(p,c)$ allowed, capacity $[0,1]$;
- bucket $(p,H) \to$ player $p$, capacity $[\ell_{p,H},\ \min(|H|, u_{p,H})]$;
- player $p\to t$, capacity $[k_p,k_p]$.

Then, for each pair $(p,c)$ currently MAYBE:
$$x_{p,c}=\text{YES} \iff \text{infeasible after forcing } x_{p,c}=0, \qquad x_{p,c}=\text{NO}\iff \text{infeasible after forcing }x_{p,c}=1$$

The graph has $\le 54 + 54 + 6 + 2 \approx 120$ nodes; a max-flow is microseconds. $54\times 6 = 324$ probes per decision is still sub-millisecond with warm-started flows. **This subsumes R1–R4 exactly and catches Hall-condition deductions they miss** (e.g. "five specific cards can only live in four specific hands whose free capacity totals four ⟹ a sixth card elsewhere is forced"). Every Fish player who is good at the game does this implicitly; the Somani engine does not.

### 6.3 Policy-aware belief update (what nobody has done for Fish)

The uniform-over-consistent-worlds posterior is *wrong*, because opponents' asks depend on their hands. The correct filter, since transfers are public and the only latent variable is the deal $w$:

$$
b_t(w) \;=\; \frac{b_0(w)\displaystyle\prod_{k<t}\pi_{\alpha_k}\big(a_k \mid I_{\alpha_k}(w,\mathcal{I}_k)\big)\;\mathbb{1}\big[o_k \text{ consistent with } w\big]}{\displaystyle\sum_{w'} b_0(w')\prod_{k<t}\pi_{\alpha_k}\big(a_k \mid I_{\alpha_k}(w',\mathcal{I}_k)\big)\mathbb{1}[\cdot]}
$$

with $\alpha_k$ the actor at step $k$ and $I_\alpha(w,\mathcal{I})$ that actor's information set in world $w$. Implement as a particle filter:

$$\omega_t^{(i)} \;\propto\; \omega_{t-1}^{(i)}\cdot \pi_{\alpha_t}\big(a_t\mid I_{\alpha_t}(w^{(i)},\mathcal{I}_{t-1})\big),\qquad \mu_{p,c}=\sum_i \omega_t^{(i)} x^{(i)}_{p,c}$$

Resample when $\mathrm{ESS}=\big(\sum_i\omega^{(i)}\big)^2/\sum_i(\omega^{(i)})^2 < N/2$, regenerating particles by sampling uniformly from the flow polytope (randomised greedy assignment + repair by augmenting paths, or Gibbs over 2-swaps that preserve feasibility). $N=2000$ particles with an incremental per-action reweight is trivially affordable at ~60–120 decisions/game.

This is exactly what makes **P9 (Ali Salahuddin's convention) computable**: the convention is nothing but a particular $\pi_B$, and the filter automatically turns it into a posterior jump. It also makes **D13** computable: if $\pi$ is identical across all worlds an opponent considers possible, that action carries zero information to them.

### 6.4 Ask selection

For a candidate ask $a=(j,c)$ from actor $i$, with $\mu_{j,c}$ the current marginal:

$$
Q(a) \;=\; \mu_{j,c}\Big[\,r_{\text{gain}}(c) + \gamma\,V\big(s'_{\text{succ}}\big)\Big]
\;+\;(1-\mu_{j,c})\Big[\,\gamma\,V\big(s'_{\text{fail}}\big) - \underbrace{D_j}_{\text{control cost}}\Big]
\;+\;\nu\,\Delta I_{\text{team}}(a)\;-\;\lambda\,\Delta I_{\text{opp}}(a)
$$

**Information terms.** With a factored surrogate for belief entropy over live cards,
$$H(b) \;\approx\; -\sum_{c\in\mathcal{C}}\sum_{p\in P}\mu_{p,c}\log \mu_{p,c}$$
define, for each *audience* $A\in\{\text{team},\text{opp}\}$ and each audience member's belief $b^{(q)}$,
$$\Delta I_A(a) \;=\; \frac{1}{|A|}\sum_{q\in A} \Big( H\big(b^{(q)}\big) - \mathbb{E}_{o\sim \Pr(\cdot|a)}\big[H\big(b^{(q)}\mid a,o\big)\big]\Big)$$
and restrict the sum to cards in half-suits still contested, since information about a decided half-suit is worth nothing. This single equation *derives* D13, D3/P3, P6 and W1 rather than hard-coding them. In particular, **P3 (deliberate failure)** is chosen automatically whenever $\nu\,\Delta I_{\text{team}} > \mu_{j,c}\cdot(\text{material})$ — which the Somani agent structurally forbids.

**Lockout, formalised.** Define the danger of opponent $j$:
$$D_j \;=\; \sum_{H\in\mathcal{H}} v_H\cdot \Pr\big[\,j \text{ can, if given the turn, complete and correctly declare } H \text{ for their team}\,\big]$$
A cheap sound lower bound: $j$ is dangerous in $H$ if $j$ holds $\ge1$ card of $H$ and, for every $H$-card not on $j$'s team, $j$'s belief marginal exceeds a threshold. Then the safe action set is
$$\mathcal{A}_{\text{safe}}=\big\{(j,c)\;:\;(1-\mu_{j,c})\cdot D_j \;\le\; \tau\big\},\qquad a^\star=\arg\max_{a\in\mathcal{A}_{\text{safe}}} Q(a)$$
Note that this *unifies* P4/D3/W2 (lockout) with D5/D11 (ask-the-asker): asking the asker is safe precisely because $\mu_{j,c}$ is high, so the control cost $(1-\mu_{j,c})D_j$ is small.

**Ask-the-asker bound.** When $j$ asks for $c'\in H$, the transcript adds $\ell_{j,H}\ge1$ and $x_{j,c'}=0$. If $u_{j,H}=|\{c\in H : x_{j,c}=\text{MAYBE}\}|$ then immediately
$$\mu_{j,c}\;\ge\;\frac{1}{u_{j,H}}\quad\forall c\in H \text{ with } x_{j,c}=\text{MAYBE}$$
a provable floor typically far above the unconditional prior $n_j/|\mathcal{C}_{\text{unknown}}|\approx 9/45 = 0.2$. This is the formal content of D5/D11/W-"ask the asker".

**Void creation (P7), formalised.** Making $j$ void in $H$ ($\ell_{j,H}=0$ and $x_{j,c}=0\ \forall c\in H$) permanently removes $j$'s legal ability to ask in $H$. Since a half-suit fully held by one team can never be recovered by the other (P1/D12), voiding all three opponents in $H$ makes $H$ *unconditionally safe*. Value:
$$V_{\text{void}}(j,H)=\Pr\big[H \text{ ends ours}\mid j \text{ void}\big]-\Pr\big[H \text{ ends ours}\mid \text{not}\big]$$
Note the direct tension with P6: P6 says vary the respondent and hold the rank fixed; P7 says hold the respondent fixed and vary the rank *after a success*. These are not contradictory — P6 governs the pre-success search, P7 the post-success exploitation — but that is a hypothesis, not a theorem (see H6, H7 in §8).

### 6.5 The asking-order leak (P6) made precise

Compare two two-ask continuations from a position where you hold a minor heart but not the 2♥:

- **Same respondent, new rank:** ask $D$ for 2♥ (fail), then $D$ for 3♥ (fail). Public consequence: $x_{i,2♥}=0 \wedge x_{i,3♥}=0 \wedge \ell_{i,H}\ge1$.
- **Same rank, new respondent:** ask $D$ for 2♥ (fail), then $E$ for 2♥. Public consequence: $x_{i,2♥}=0 \wedge \ell_{i,H}\ge1$ only.

The second reveals **strictly less** about the asker (one fewer forced-zero in your own row) while revealing strictly more about the *respondents* (two rows constrained instead of one). Formally $\Delta I_{\text{opp}}^{\text{self}}(\text{new rank}) > \Delta I_{\text{opp}}^{\text{self}}(\text{new respondent})$, and pagat's argument is that the self-leak dominates because a respondent who knows more than you about that half-suit converts your leak into a declaration. This is directly testable (H6).

### 6.6 The declaration decision

Let $H$ be a half-suit, $\mathcal{T}$ your team, and $A: H \to \mathcal{T}$ a candidate allocation. Under your rules (any error ⇒ opponents score, correct ⇒ you score), and scoring set-differential in $\{+1,-1\}$:

$$q(A) \;=\; \Pr\big[\,x_{A(c),c}=1\ \ \forall c\in H \;\big|\; \mathcal{I}_t \,\big], \qquad
\mathbb{E}[\text{score}\mid \text{declare }A] = q(A)\cdot(+1) + (1-q(A))\cdot(-1) = 2q(A)-1$$

so the **immediate** declaration is +EV iff $q>\tfrac12$, and the best allocation is
$$A^\star=\arg\max_{A\in \mathcal{T}^{H}} q(A) = \arg\max_A \sum_i \omega^{(i)}\prod_{c\in H}\mathbb{1}\big[x^{(i)}_{A(c),c}=1\big]$$
with $|\mathcal{T}^H| = 3^6 = 729$ allocations per half-suit — brute-forceable, or read off directly from the particle set as the modal allocation (which is better: $q(A^\star)$ is just the weight of the modal allocation cluster).

But $2q-1$ is only the *exercise* value of an American option. The full rule is

$$
\text{Declare now} \iff (2q_t-1) \;>\; \underbrace{\mathbb{E}\big[\,2q_{t+\Delta}-1\,\big]}_{\text{info improves by waiting}} \;-\; \underbrace{C_{\text{waste}}}_{\text{teammates burn asks (D9)}} \;+\; \underbrace{B_{\text{control}}(t)}_{\text{control transfer (D15/W3)}} \;-\; \underbrace{C_{\text{steal}}}_{=0 \text{ if your team provably holds all six (P1)}}
$$

The $C_{\text{steal}}=0$ term is the theoretical justification for D12/P1 and it is *exact*: once the opposing team is void in $H$ they have no legal base card, so $q$ is a martingale that can only improve. **Therefore, if you provably hold all six yourself, waiting weakly dominates declaring — the stalemate-breaker (W3) is not folklore, it is optimal play under this model.**

Develin's D15 example drops straight out. Two sets remain. Option A: declare clubs at $q=\tfrac12$, become cardless, transfer control, then win diamonds with certainty:
$$\mathbb{E}[A] = (2\cdot\tfrac12-1) + 1 = 0 + 1 = 1$$
Option B: ask to improve club information, thereby losing diamonds for certain:
$$\mathbb{E}[B] = (2q'-1) - 1 \;\Rightarrow\; \mathbb{E}[B]>\mathbb{E}[A] \iff q' > \tfrac32$$
— impossible, which is exactly his "150%" remark. **The rule `declare a coin-flip set to buy a control transfer whenever the transfer secures a set` is provably right, and no existing agent implements it.**

### 6.7 Turn-passing and endgame declarer choice

When a declaration empties your hand on your turn, you choose which teammate with cards receives control (the team may openly say only who is *willing* to receive). Pick
$$t^\star=\arg\max_{t\in\mathcal{T}, n_t>0}\ \Big[\ \mathbb{E}\big[\text{sets won}\mid \text{control at } t\big]\ \Big]$$
which decomposes into: (a) $t$'s expected immediate ask-chain length $\sum_{k\ge1}\prod_{l\le k}\mu^{(t)}_{\cdot}$; (b) whether $t$ is holding cards in half-suits where an opponent is nearly void; (c) whether $t$ is the teammate whose beliefs place a contested set. Note this requires modelling $t$'s beliefs, i.e. the dummy-player machinery of §2.3 done properly.

For the forced-declaration endgame, under the pagat rule your **opponents** choose which of your players (holding $\ge1$ card) must declare everything, and they will choose your worst-informed player. So your objective becomes a **maximin over your own team**:
$$\text{maximise}\quad \min_{t\in\mathcal{T}: n_t>0}\ \prod_{H\in\mathcal{R}} \max_{A} q_t(A)$$
which gives a genuinely novel strategic implication that no source states explicitly: **before the endgame, either equalise information across your team, or deliberately empty your least-informed teammate's hand so they cannot be nominated.** Treat as hypothesis H12.

---

## 7. Empirical Results Known

- **`neelsomani/literature`:** no published win rates, no benchmark, no self-play curves, no head-to-head numbers. The only reproducible facts are the pickle's training counters ($n_{\text{iter}}=1{,}030{,}945$, $t=2{,}001{,}821$ samples, 34.7 MB dominated by an enormous `loss_curve_`) and the architecture. **Everything about its playing strength is UNVERIFIED.**
- **`doubleiis02/CanadianFish`:** no evaluation; the bot's probability vectors are never updated, so it is uniform-random within legality.
- **Google Play `com.cards.game.literature`:** claims three bot difficulty levels with distinct playing styles for 4 and 6 players. No published methodology, no strength data. **UNVERIFIED.**
- **Cross-domain, verified, and relevant:** Morenville & Piette (arXiv:2507.19263) report that purely constraint-based (logical) belief states perform **comparably** to Belief-Propagation marginals across two hidden-identity games, with minimal difference in agent performance. For Fish this suggests the flow-based certainty oracle (§6.2) may capture most of the value, with the probabilistic layer (§6.3) contributing mainly in the mid-game where many cards are MAYBE.

**What worked (in the prior art):** the constraint-propagation deduction engine; first-order theory-of-mind bookkeeping over the public transcript; treating the ask as a `(respondent, card)` pair and filtering by legality; capping episodes to avoid non-terminating self-play.

**What failed:** the RL itself (broken terminal reward, no bootstrapping, no exploration, myopic per-move shaping that contradicts the game's own strategy literature); the declaration policy (hard-coded, greedy, risk-free-only); the 4-player restriction (removes the game's central difficulty); the raw-ordinal, non-equivariant, 5×-redundant state encoding.

---

## 8. Human Heuristics as Testable Hypotheses

Every heuristic below is stated so that it can be run as a paired ablation in self-play (same deals, policy A vs policy B). "Testable" means you can measure it; "confounded" flags heuristics whose value depends on opponent modelling.

| # | Heuristic | Source | Formal handle | Testable? |
|---|---|---|---|---|
| H1 | Never grant the turn to a dangerous opponent (blackball/lockout) | D3, P4, W2 | $\mathcal{A}_{\text{safe}}$ threshold $\tau$ in §6.4 | **Yes** — sweep $\tau$; measure sets conceded |
| H2 | Never declare below certainty except to transfer control | D12, P1 | declare iff $q>\theta$; §6.6 | **Yes** — sweep $\theta\in[0.5,1]$ |
| H3 | Hold a fully-owned set as a stalemate-breaker | D8, W3 | option value; $C_{\text{steal}}=0$ | **Yes** — delay-declare vs greedy-declare |
| H4 | Declare a 50/50 set to buy a control transfer | D15 | $\mathbb{E}[A]$ vs $\mathbb{E}[B]$ in §6.6 | **Yes** — endgame-only ablation |
| H5 | Ask the asker | D5, D11, P-implicit | $\mu_{j,c}\ge 1/u_{j,H}$ bound | **Yes** — prior bonus on/off |
| H6 | Exhaust one rank across all opponents before switching rank | P6 | $\Delta I^{\text{self}}_{\text{opp}}$ ordering, §6.5 | **Yes** — direct ordering constraint |
| H7 | After a success, keep hitting the same opponent to void them | P7 | $V_{\text{void}}$, §6.4 | **Yes** — conflicts with H6; test jointly |
| H8 | Avoid extended back-and-forth with one opponent | P8 | cap on consecutive same-pair exchanges | **Yes** |
| H9 | Deliberately fail an ask to inform your partner | P3, D13 | $\nu\,\Delta I_{\text{team}}$ term | **Yes** — set $\nu=0$ vs $\nu>0$ |
| H10 | Ask inside a half-suit your team fully owns as a free information channel | D13 corollary | $\Delta I_{\text{opp}}=0$ exactly | **Yes** — clean, high-confidence test |
| H11 | Endgame declarer = most-informed, not most-carded | D14 vs W4 | $\arg\max_t \prod_H \max_A q_t(A)$ | **Yes** — the two rules disagree; measure |
| H12 | Equalise team information / empty your weakest teammate before the forced endgame | novel (from P11) | maximin in §6.7 | **Yes**, but needs the pagat nomination rule |
| H13 | Clean out half-suits sequentially | D7 | pure memory-load argument | **No** for a bot (perfect memory) — expect ~0 effect; a good null-hypothesis control |
| H14 | Focus attention only on half-suits you hold cards in | D2, P5 | attention/compute budget | **No** for correctness; **Yes** for compute-limited variants |
| H15 | Partner signalling (Ali Salahuddin convention) | P9 | $\pi_B$ conditioned on $\kappa$; §6.3 | **Yes**, but see §9 — legality is disputed |
| H16 | Lie low with many cards in a half-suit | D6 | delay-entry into a contested $H$ | **Yes** |
| H17 | Don't ignore half-suits you hold nothing in (needed for lockout) | D4 | belief must cover all $H$ | **Yes** — ablate opponent-only tracking |

H13 is the most interesting *null*: it is pure human memory economics and should have **zero** effect on a bot. If your ablation shows an effect, you have a bug or a hidden confound. Use it as a sanity control.

---

## 9. Pitfalls, Negative Results, and Failure Modes

1. **Copying the Somani reward shaping will make your bot worse.** The `±20` per-ask signal is a direct anti-incentive against H9, H10, and H16 — three of the strongest human heuristics. If you shape at all, shape on *set-differential* deltas, not ask success.
2. **The enum-comparison bug class.** Verify your terminal reward actually varies across games by asserting `len(set(returns)) > 1` in your training loop. This exact bug survived a public release, a PyPI publication, and a Travis CI setup.
3. **Uniform-over-consistent-worlds is a wrong prior and it is wrong in a *directional* way.** Opponents ask about half-suits they hold; assuming uniformity systematically underestimates $\mu_{j,c}$ for $c$ in half-suits $j$ has asked about. Every one-line-heuristic bot in the wild makes this error.
4. **Self-play will invent conventions, and Develin's rules forbid them.** Because all asks are public and the deal is the only hidden state, self-play in a 2-team game has enormous pressure to develop signalling codes (the same phenomenon that makes Hanabi self-play agents unusable with humans). Decide up front: (a) allow conventions (pagat-legal, per P9), and accept that your agent will not cooperate with human partners; or (b) forbid them, e.g. by training with randomly-paired partners from a diverse population, or by penalising policies whose action distribution is not invariant under relabelling of private-hand-consistent worlds. This is the single biggest architectural fork in the project.
5. **4-player results do not transfer.** With one teammate, declaration allocation is near-forced ($2^6$ and heavily constrained by your own hand); with two teammates it is $3^6=729$ and genuinely hard. Any baseline drawn from the Somani agent is measuring a different game.
6. **Declaration is not a single action.** It is (a) *when*, (b) *which half-suit*, (c) *which of 729 allocations*, and (d) *by whom* (any player may declare at any time, including during an opponent's turn). Any agent that models it as "declare when certain" throws away (a), (c) and (d) entirely.
7. **Infinite/near-infinite games are real.** Somani hit them and patched with jitter + a 200-move cap. Under a lockout-aware policy the risk *increases*: both teams may refuse to grant the turn to anyone dangerous, and the position stalls. You need either a move cap with a defined scoring rule, or Srinivasan's Forced Claims rule (P13) as a terminating device. Note Srinivasan invented that rule for exactly this degeneracy.
8. **Ties are frequent in the 48-card game** (4–4 splits); your 54-card, 9-half-suit deck removes them by construction, which is one reason the joker variant exists. Do not port evaluation code that assumes ties.
9. **`Half.MINOR` in the Somani code is $\{A,2,3,4,5,6\}$, not $\{2,\dots,7\}$.** If you benchmark against it, you are benchmarking a remove-the-7s variant.
10. **The "history may not be discussed" rule (P10)** means human players are memory-limited by design. A perfect-recall bot is not playing the same game a human is; if you evaluate against humans, this is a confound worth reporting, not hiding.
11. **The bluff variant (W5) invalidates a core inference.** If a group allows asking for a card you already hold, `failed ask ⟹ asker lacks card` becomes unsound. Guard the deduction engine behind a rules flag.
12. **Greedy claim-everything (as in `make_claims`) destroys H3, H4 and H10** and can hand your opponents information for free by resolving a half-suit whose ambiguity was protecting you.
13. **First-order theory of mind may be insufficient here, but second-order is nearly free.** Because the transcript is public, "what $j$ knows" differs from the public state only by $j$'s hand. So a *correct* opponent model is: public deduction state + a posterior over $j$'s hand. The Somani dummies never populate the hand and are therefore just five redundant copies of the public state. Fix this and you get most of the value of ToM for almost no extra dimensionality.

---

## 10. Applicability to Canadian Fish — Technique-by-Technique

| Technique | Would it help? | Compute cost | Pitfalls | Adaptation for 6-player / 2-team / public-transfer |
|---|---|---|---|---|
| **Somani constraint propagation (R1–R4)** | Yes — necessary floor | Negligible (<0.1 ms) | Incomplete; misses Hall deductions | Port directly; generalise `SETS` to 9 half-suits incl. `{8♠8♥8♦8♣, J1, J2}`; make the joker set a first-class `HalfSuit` |
| **Max-flow-with-lower-bounds certainty oracle (§6.2)** | **Yes — top recommendation** | ~0.3–1 ms/decision with warm starts | Must model $\ell_{p,H}\ge1$ correctly or you lose the strongest constraints | 120-node graph; probe all $(p,c)$ MAYBE pairs; recompute incrementally per public event |
| **Exact world counting via class-DP (§6.1)** | Yes, early/mid game | $\sim2\times10^7$ int ops; ms | Half-suit lower bounds force inclusion–exclusion; degrades as $|\mathcal{L}|$ grows | Use exact counting when $|\mathcal{L}|\le 12$, particle filter otherwise |
| **Policy-aware particle filter (§6.3)** | Yes — the biggest accuracy win over any existing bot | $N=2000$, $O(N)$ reweight per public action; ~1–5 ms | Particle depletion after surprising asks; needs a rejuvenation move that preserves feasibility | Reweight by $\pi_{\alpha}$ of *all five* other players; ESS-triggered resample; regenerate from the flow polytope |
| **Somani MLP value head** | No — do not reuse | — | Broken reward, ordinal encoding, no equivariance | If you want a learned value, re-encode: one-hot $\{YES,NO,MAYBE\}$ per (player, card) *relative to the actor's seat*, plus per-half-suit $(\ell_{p,H}, u_{p,H})$, plus $n_p$; enforce suit-permutation equivariance (the 4 non-joker suits are exchangeable) to cut sample complexity ~4× |
| **Somani greedy claim rule** | Partially — as a *floor*, never as the policy | Negligible | Destroys H3/H4/H10 | Replace with the option-value rule of §6.6; keep "declare immediately" only for the case where the team provably holds all six **and** a teammate has asked in that half-suit (D9) |
| **Explicit lockout constraint (§6.4)** | Yes — cheap and matches every human source | Negligible once $D_j$ is computed; $D_j$ is $O(|\mathcal{H}|)$ | Over-aggressive $\tau$ produces stalemates (pitfall 7) | Compute $D_j$ from your *belief about $j$'s belief*, i.e. the public deduction state; that is exactly what public transfers make easy |
| **Information-gain terms $\nu\Delta I_{\text{team}} - \lambda\Delta I_{\text{opp}}$** | Yes — this is where Fish skill lives | Factored-entropy surrogate is $O(|\mathcal{C}||P|)$; ~0.1 ms | Full entropy over worlds is intractable; the factored surrogate ignores correlations | Restrict to contested half-suits; use $\Delta I_{\text{opp}}=0$ exactly for team-owned half-suits (D13 corollary), which is a *provable* shortcut |
| **Ali Salahuddin convention as an explicit $\pi$** | Only if you allow conventions | Free | Illegal under Develin; will not transfer to human partners | Implement as a *toggle*; measure H15 with and without to quantify the convention's value, which is publishable in itself |
| **Declaration allocation search ($3^6$)** | Yes | 729 × particle-set lookup ≈ trivial; or read the modal allocation off the particle set in $O(N)$ | Do not confuse $\Pr[\text{team holds all six}]$ with $\Pr[\text{this exact allocation}]$ — under your rules only the latter matters | With 3 teammates the gap between the two is large; this is precisely the difficulty absent from the 4-player prior art |
| **Turn-transfer choice on being emptied (§6.7)** | Yes — free value, currently `random.random()` in the prior art | Small tree evaluation per candidate teammate | Requires teammate belief models | Evaluate $\mathbb{E}[\text{chain length}]$ + set-completion prospects per teammate |
| **Srinivasan Forced Claims / Challenge (P13)** | As an *evaluation* rule, yes | — | Changes the game; only adopt deliberately | Consider Forced Claims as your anti-stalemate terminator instead of an arbitrary move cap |

---

## 11. Bibliography

**Primary rules and human strategy**

1. John McLeod. *Literature — card game rules.* pagat.com, © 2006, 2007, 2024, 2025; page last updated 1 July 2026. https://www.pagat.com/quartet/literature.html — canonical rules, Tactics section, Public Information rules, Endgame protocol, Irregularities, Variations (incl. the 54-card two-Joker nine-set variant and the "any error awards the set to opponents" variant), Ali Salahuddin's convention, Guy Srinivasan's Forced Claims / No Probabilistic Information / Challenge proposals. Read in full from the raw page.
2. Mike Develin. *Canadian Fish*, chapter 9 of his card-games manual. Archived copy: https://web.archive.org/web/20170720075756/http://bantha.org/~develin/cardgames.html#ch9 — the deepest strategy writing found; exactly the 6-player declare-anytime error-forfeits variant; contains the blackball worked example, the "don't declare a set you own" argument, the D13 free-information-channel corollary, the D15 quantitative control-transfer example, and the explicit prohibition of conventions. Read in full from the archive.
3. Wikipedia contributors. *Literature (card game).* https://en.wikipedia.org/wiki/Literature_(card_game) — rules, information-asymmetry principle, the "stalemate breaker".
4. Deposit Genius. *Literature Game Strategy* (Canadian Fish). https://depositgenius.com/literature-strategy-canadian-fish/ — blackballing, ask-the-asker, defer-claiming, endgame delegation to the player with the most cards, memory aids. Derivative of 1–3 but states W4 explicitly.
5. Deposit Genius. *How to Play Literature (Canadian Fish).* https://depositgenius.com/literature-rules-canadian-fish/
6. gamerules.com. *Literature Card Game Rules.* https://gamerules.com/rules/literature-card-game/
7. R. M. Winslow. *Literature — Game Rules.* https://games.rmwinslow.com/rules/othercards-literature.html — rules only; no strategy.
8. gambiter.com. *Literature — card game.* https://gambiter.com/cards/Literature_card_game.html — a pagat mirror. **UNVERIFIED**: the site timed out on direct fetch; content known only through search snippets.
9. Grokipedia. *Literature (card game).* https://grokipedia.com/page/Literature_(card_game) — **UNVERIFIED**: returned HTTP 403 on fetch. Search snippets attribute the blackball framing ("the responder only gets to ask if they hold the requested card") to this page.
10. `canadian-fish.vercel.app/strategy` — **UNVERIFIED / empty**: fetched and found to contain no strategy content (title only).

**Computational agents and implementations (all repository facts verified against the GitHub API or a local clone)**

11. Neel Somani. *literature* — Literature card game implementation. GitHub, MIT licence, Python. https://github.com/neelsomani/literature — cloned at commit `3d2c75c`; all 1,940 LOC read; `model_10000.out` pickle disassembled for architecture and training counters.
12. Neel Somani. *literature-server.* GitHub. https://github.com/neelsomani/literature-server — React + Flask/gevent front-end; no agent code. Live at https://literature.neelsomani.com/
13. CJ Quines. *cfish* — web app for canadian fish / literature. GitHub, TypeScript. https://github.com/cjquines/cfish — 35 files, verified no AI code.
14. Sol Kim (`Dynosol`). *playfish.io.* GitHub / https://playfish.io/ — React/TypeScript/Vite/Firestore; no agent.
15. zairza-cetb. *literature.* GitHub, Dart/Flutter. https://github.com/zairza-cetb/literature
16. `doubleiis02`. *CanadianFish.* GitHub, Java. https://github.com/doubleiis02/CanadianFish — `Fish_2.0/Bot.java` read: uniform $1/41$ or $1/40$ per-card probability vectors, never updated.
17. David Amirault. *fish — Java interface and AI for the card game Fish.* GitHub. https://github.com/david-amirault/fish — `AIDummyController.java`, `AIDummyGame.java`; **UNVERIFIED** contents (repo is dominated by AP-CS starter code).
18. Ryan1729. *canadian-fish* — a single player version of the card game. GitHub, Rust. https://github.com/Ryan1729/canadian-fish — grepped `src/main.rs`; opponent logic **UNVERIFIED**.
19. Srinath Pattabiraman and Narayana Suri. *Literature — Free Multiplayer Card Game.* https://play-litaf.onrender.com/ — human multiplayer; no bot found.
20. *Literature: Fish Card Game* (`com.cards.game.literature`), Google Play. https://play.google.com/store/apps/details?id=com.cards.game.literature — advertises offline bots for 4 and 6 players at three difficulty levels with per-bot playing styles. Closed source; algorithm **UNVERIFIED**. The only known 6-player Fish bot besides yours.
21. Other UI-only implementations verified to contain no agent: `acavet/web-fish`, `liamcolangelo/fish`, `whaatt/Literature`, `Raghav-Sao/literature`, `onlymx13/fish`, `gyash24x7/littplay`, `nikhilmandlik/canadian-fish`, `cheedep/CanadianFishing`, `m-goulet/canadian-fish-dc`.

**Cross-domain methods actually consulted (full treatment belongs to other research areas)**

22. Achille Morenville and Éric Piette. *Modeling Uncertainty: Constraint-Based Belief States in Imperfect-Information Games.* arXiv:2507.19263, 2025. https://arxiv.org/abs/2507.19263 — CSP belief states vs Belief-Propagation marginals; finding that constraint-based beliefs perform comparably to probabilistic inference.
23. Martin Zinkevich, Michael Johanson, Michael Bowling, Carmelo Piccione. *Regret Minimization in Games with Incomplete Information.* NeurIPS 2007. https://proceedings.neurips.cc/paper/2007/file/08d98638c6fcd194a4b1e6992063e944-Paper.pdf
24. Daochen Zha, Kwei-Herng Lai, Songyi Huang, Yuanpu Cao, Keerthana Reddy, Juan Vargas, Alex Spirling, Xia Hu. *RLCard: A Toolkit for Reinforcement Learning in Card Games.* arXiv:1910.04376. https://arxiv.org/abs/1910.04376 — Literature is **not** among its environments.

**Negative search results (recorded so they are not re-run)**

25. arXiv API, 2026-08-21: `all:"Canadian Fish"` → 0 results; `all:"Literature card game"` → 0 results; `all:"half-suit"` → 0 results; `abs:"Go Fish"` → 0 results.
26. GitHub repository search, 2026-08-21: queries `literature+card+game`, `canadian+fish+card+game`, `fish+card+game+ai`, `literature+card+game+ai`, `half+suit+card+game` — no repository with search, planning, CFR, or functioning RL for this game other than item 11.
27. Web search for Stanford CS221/CS229, Berkeley, MIT, Caltech course projects on Literature / Canadian Fish — nothing specific found.
28. Web search for a Neel Somani blog post or paper describing the agent's results — nothing found on neelsomani.ai or neelsomaniblog.com.
