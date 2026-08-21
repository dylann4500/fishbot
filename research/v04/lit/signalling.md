# Emergent Signalling and Convention Formation Through Legal Actions
### Literature review for the Canadian Fish / Literature agent — v0.4
### Scope: Hanabi conventions & zero-shot coordination, bridge bidding as a learned language, signalling-game theory, information-theoretic measures of action-borne information, public-belief-state (PBS) formalisms, and the formal value of a convention when *every* action is observed by all six players.

---

## 1. Executive summary (15 bullets)

1. **Fish is the cleanest possible PBS game.** All asks, answers, transfers, declarations and hand counts are public; the *only* hidden variable is the initial 9-card deal. Therefore every player's information state factorises exactly as $s_i^t = (x_i, h_t^{\text{pub}})$ where $x_i$ is player $i$'s dealt hand. This is strictly simpler than poker (where private cards arrive over time) and makes the ReBeL / Player-of-Games / BAD public-belief-state machinery *directly* applicable in structure, though not in its two-player-zero-sum guarantees.
2. **The public belief is a distribution over $\approx 1.011\times 10^{38}$ deals** ($54!/(9!)^6$, i.e. **126.2 bits** of total uncertainty; **93.9 bits** from a single player's viewpoint, $45!/(9!)^5 \approx 1.90\times 10^{28}$). Beliefs must be represented by a neural / factorised model plus a constraint projection, never enumerated.
3. **The belief factorises into a "grounded" part and a "conventional" part**: $\beta_t(d) \propto \mathbb{1}[d \in \mathcal{C}_t]\cdot L_t(d)$, where $\mathcal{C}_t$ is the set of deals not excluded by the *hard rules* (you must hold a card of the asked half-suit; you must not hold the asked card; answers are truthful) and $L_t(d)=\prod_{\tau<t}\pi_{i_\tau}(a_\tau\mid x_{i_\tau}(d),h_\tau^{\text{pub}})$ is the *policy likelihood*. Conventions live entirely in $L_t$. BAD measured that in Hanabi roughly 40% of the recovered information came from the conventional part, not the grounded part.
4. **Raw signalling bandwidth is not the bottleneck in Fish.** An opening hand admits on average **84 legal asks** (median 81, range 27–135) $\Rightarrow$ up to **≈6.4 bits per ask** plus ≤1 bit per answer. Over 60–120 decisions that is 400–800 raw bits against 94 bits of per-player uncertainty. The binding constraint is *incentive compatibility under eavesdropping*, not channel capacity.
5. **The right economic model is Farrell & Gibbons (1989), "Cheap Talk with Two Audiences."** A separating (informative) equilibrium exists in *private* with audience Q iff $v_1,v_2\ge 0$, in private with R iff $w_1,w_2\ge 0$, and **in public iff $v_1+w_1\ge 0$ and $v_2+w_2\ge 0$**. In Fish the friendly audience is the two teammates and the hostile audience is the three opponents, so a convention survives only when the teammate-side gain outweighs the opponent-side loss, state by state. Their Proposition 1: private credibility with each audience implies public credibility, but *not* conversely — public speech can be both disciplined and subverted.
6. **The right information-theoretic model is Wyner's wiretap channel**, secrecy capacity $C_s=\max_{p(x)}[I(X;Y)-I(X;Z)]$ (Csiszár–Körner: $\max_{p(v)p(x|v)}[I(V;Y)-I(V;Z)]$). Applied per-decision, a Fish convention is worth adopting only when its *secrecy rate* $\sum_{j\in\text{team}} I(X;A\mid h,x_j)-\sum_{k\in\text{opp}} I(X;A\mid h,x_k)$ is positive **in value terms**, not merely in bits.
7. **Critical negative result specific to Fish: symmetric-decodability means near-zero information-theoretic secrecy.** In Hanabi every player *sees everyone else's hand*, which is what makes Cox et al.'s modular hat-guessing code work. In Fish nobody sees anyone's hand, so a naive "encode fact $F$ in the choice of card" convention is decoded *equally well* by all five listeners. Copying Hanabi conventions verbatim into Fish is a category error.
8. **The escape hatch is receiver-relative (hand-indexed) codebooks**, i.e. a channel whose state is known at the decoder but not the encoder. If the *meaning* of an ask is defined relative to the receiver's own holdings ("the $t$-th card of half-suit $h'$ counting up from *your* lowest card in $h'$"), the teammate resolves it uniquely while opponents must marginalise over the teammate's unknown hand. Secrecy rate $R_s=\max_{p(x)}[I(X;Y\mid S_j)-I(X;Z)]$ can be strictly positive. Cost: encoder-side ambiguity — the sender cannot verify the frame.
9. **Signal in the dimensions the rules already leak.** Choosing to ask in half-suit $h$ already publicly proves you hold a card of $h$ (grounded, zero marginal leak). The *free* channels are (a) **which opponent you target** ($\log_2 3\approx1.58$ bits, essentially no direct hand leak) and (b) **which card within $h$** ($\le\log_2 5\approx2.32$ bits, leaking only "I do not hold $c$"). Conventions should be built on those two, and the "which half-suit" dimension should be treated as material, not communicative.
10. **The eavesdropping tax in Fish is near-deterministic and immediate**, unlike in bridge. Once it is common knowledge that you hold card $c\in h$, *any* opponent holding a card of $h$ can take $c$ on their turn with probability 1 *and retain the turn*. Contrast with Hanabi (no adversary) and bridge (leaked info only shifts opponents' play probabilities). This single asymmetry should dominate the bot's signalling policy.
11. **Self-play conventions do not transfer** and can be arbitrary "handshakes." Other-Play (Hu et al. 2020) raised Hanabi cross-play from $2.52\pm0.34$ (SAD) to $22.07\pm0.11$ (SAD+AUX+OP) with essentially unchanged self-play score; Off-Belief Learning (Hu et al. 2021) reached $24.10\pm0.01$ self-play with $23.76\pm0.06$ cross-play at level 4. For a Fish bot that will face *human* teammates or independently-trained agents, OP/OBL-style grounding is mandatory; for a bot with a fixed, jointly-trained partner it is optional and costly.
12. **OBL is the sharpest available knob for "how much convention do I want?"** OBL level 1 is a purely *grounded* policy (all conventional information zeroed out); higher levels reintroduce $k$ steps of convention. This maps exactly onto the Fish grounded/conventional belief decomposition and gives a principled ablation ladder.
13. **Search on top of a blueprint is worth several points.** SPARTA multi-agent search took Hanabi 2P from $24.08$ (56.1% perfect) to $24.61$ (75.5%) but at $\sim1.8\times10^8$ rollouts/game; Learned Belief Search recovered 76–90% of the gain at 4.6–42× less compute using an autoregressive belief net. For Fish, LBS-style learned beliefs over deals are the practical choice.
14. **Bridge bidding is the only published "learned language under adversarial observation."** Rong, Qin & An (AAMAS 2019) split the problem into an *estimation* net (partner's cards) and a *policy* net; Lockhart et al. (DeepMind 2020) got human-compatible bidding via imitation + Borel particle search + policy iteration (+0.97 IMPs/deal vs humans). Note that NukkAI's NooK win over eight world champions (2022) was **card play only, with bidding removed** — it is *not* evidence about learned signalling.
15. **Beware metric theatre.** Lowe et al. (AAMAS 2019) show agents can score high on "positive signalling" metrics (speaker consistency, message–action mutual information) while messages have *zero* causal effect; the same scrambling test must be run on any Fish convention diagnostic. Use bits as a diagnostic, use value as the objective.

---

## 2. Formal foundations

### 2.1 The signalling game and separating vs. pooling equilibria

A signalling game (Spence 1973/1978; Crawford & Sobel 1982 for the cheap-talk variant) has:

- a sender with private type $t\in T$ drawn from common prior $p(t)$,
- a message $m\in M$ chosen by sender strategy $\mu(m\mid t)$,
- a receiver action $a\in A$ chosen by $\alpha(a\mid m)$,
- payoffs $u_S(t,m,a)$ and $u_R(t,m,a)$.

A **Perfect Bayesian Equilibrium** is $(\mu,\alpha,\beta)$ with

$$
\beta(t\mid m)=\frac{p(t)\mu(m\mid t)}{\sum_{t'}p(t')\mu(m\mid t')}\quad\text{whenever the denominator is}>0,
$$
$$
\alpha(\cdot\mid m)\in\arg\max_{a}\sum_t \beta(t\mid m)\,u_R(t,m,a),\qquad
\mu(\cdot\mid t)\in\arg\max_m \sum_a \alpha(a\mid m)u_S(t,m,a).
$$

- **Separating**: $\mu$ is injective on types — $\beta(\cdot\mid m)$ is a point mass. Full revelation.
- **Pooling (babbling)**: $\mu(m\mid t)=\mu(m)$ independent of $t$ — $\beta(\cdot\mid m)=p(\cdot)$, the message is ignored.
- **Semi-separating / hybrid**: partial partitioning of $T$; Crawford–Sobel show that with misaligned preferences and a continuum of types, all equilibria are *interval partitions* (a finite number of "bins"), and the number of bins is bounded by the preference bias.

**Cheap talk** is the special case where $m$ enters no payoff function: costless, non-binding, unverifiable. **Costly signalling** (Spence) is where $u_S$ depends directly on $m$, so separation is sustained by differential cost.

**Mapping to Fish.** An ask is *almost* cheap talk — the message is the (target, card) pair and the direct payoff effect is only via the resulting transfer and turn. But it is not free: a miss costs the turn, and the ask itself proves half-suit membership. So a Fish ask is **costly cheap talk / money-burning**, which is *good news*: costly messages support separating equilibria that pure cheap talk cannot. A **declaration** is the textbook costly signal: you pay real risk (any error hands the set to the opponents) to convert belief into score. This is the natural point at which conventional information must be "cashed in."

### 2.2 Cheap talk with two audiences — the exact model for Fish (Farrell & Gibbons, AER 1989)

Sender $S$ observes $s\in\{s_1,s_2\}$ with prior $\pi$ on $s_1$. Two receivers $Q$ and $R$ each choose a binary action. Normalise so that if the state were known, $Q$ would choose $q_i$ and $R$ would choose $r_i$ in state $s_i$. The sender's payoff is additively separable:

$$
u_S(s,q,r)=u_S^Q(s,q)+u_S^R(s,r),\qquad
u_S^Q(s_i,q_i)=v_i,\quad u_S^R(s_i,r_i)=w_i,
$$

with $u_S^Q(s_i,q_j)=u_S^R(s_i,r_j)=0$ for $j\ne i$. Then (their conditions (1)–(2)):

$$
\textbf{private to }Q:\ \text{separating iff } v_1\ge 0 \text{ and } v_2\ge 0,
$$
$$
\textbf{private to }R:\ \text{separating iff } w_1\ge 0 \text{ and } w_2\ge 0,
$$
$$
\boxed{\textbf{public to both:}\ \text{separating iff } v_1+w_1\ge 0 \text{ and } v_2+w_2\ge 0.}
$$

**Proposition 1 (Farrell–Gibbons).** Incentives for honesty in each relationship in private imply incentives for honesty in public; the converse is false.

Three named regimes follow:
- **One-sided discipline**: $v_i \gg 0$, $w_i$ slightly negative — the friendly audience's presence sustains credibility that would fail with the hostile audience alone.
- **Subversion**: private talk to $Q$ works ($v_i\ge0$) but $v_i+w_i<0$ for some $i$ — the lurking hostile audience destroys communication that would otherwise be feasible.
- **Mutual discipline**: $v_1,w_2\gg0$ and $v_2,w_1<0$ — neither private channel supports separation, but the *opposing* temptations cancel and public speech is credible.

**Why this matters for Fish.** Fish is exactly this game with one friendly audience (2 teammates) and one hostile audience (3 opponents), and *only* the public channel exists. The design question "should the bot adopt convention $\kappa$?" is literally the question "is $v_i^\kappa + w_i^\kappa \ge 0$ in every state?" The regime the bot will mostly live in is **subversion** — because $w_i$ (the harm from opponents decoding) is large and near-deterministic in Fish (§2.6). Expect mostly-pooling equilibria on card identity, with separation only where $w_i\approx 0$ (the free channels of §1.9), or where mutual-discipline structure arises naturally (e.g. an ask that is simultaneously an information request and a material grab, so the sender cannot cheaply lie).

### 2.3 Value of information in zero-sum games

Classical results (Gossner & Mertens 2001; Bassan, Gossner, Scarsini & Zamir 2003; going back to Hirshleifer 1971) establish:

- In a **two-player zero-sum** game, giving a player *more* information (in the Blackwell garbling order) weakly increases that player's value, provided the information structure is common knowledge.
- In **non-zero-sum** or multi-player settings, more information can strictly *hurt* the informed player; positive value of information holds only under conditions (Bassan et al. give sufficient conditions).

**Consequence for Fish.** If we abstract each team into a single ex-ante-correlating decision maker (the TMECor abstraction of §2.5), the game *is* two-player zero-sum and therefore **garbling the opposing team's information weakly helps us**. That justifies the sign of the "eavesdropping tax" $\Delta_{\text{eaves}}\ge0$ defined below. **Caveat:** the opposing *team* is not literally a single agent — three humans/bots with imperfect coordination — so in principle extra information could hurt them (Hirshleifer effect). Do not rely on this; it is a second-order effect and a fragile source of edge.

> Verification note: I located Gossner's papers and Bassan et al. by title/venue but could not extract the PDF text (see bibliography); the statements above are the standard summary as reported in the sources I did read. Marked **PARTIALLY VERIFIED**.

### 2.4 The wiretap channel: informing a friend while an enemy listens

Wyner (1975) degraded wiretap channel: sender $X$, legitimate receiver $Y$, eavesdropper $Z$. Secrecy capacity

$$
C_s=\max_{p(x)}\big[\,I(X;Y)-I(X;Z)\,\big].
$$

Csiszár & Körner (1978) generalised to arbitrary (non-degraded) broadcast channels with confidential messages, with an auxiliary variable $V$:

$$
C_s=\max_{p(v)\,p(x\mid v)}\big[\,I(V;Y)-I(V;Z)\,\big].
$$

**Fish version.** Let $X$ be a sufficient statistic of the asker's hand that the convention wants to transmit, $A$ the chosen ask, $h$ the public history, $x_j$ a teammate's hand, $x_k$ an opponent's hand. Define the **per-decision secrecy rate**

$$
\rho(\pi;h)\;=\;\sum_{j\in \mathcal{T}\setminus\{i\}} I\!\left(X;A \,\middle|\, H=h,\;X_j=x_j\right)\;-\;\sum_{k\in \mathcal{O}} I\!\left(X;A \,\middle|\, H=h,\;X_k=x_k\right).
$$

**Key structural result for Fish (negative).** A priori, teammates' and opponents' hands are exchangeable given the public history — nothing in the deal distinguishes them. Therefore for a convention whose codebook is *absolute* (meaning fixed by public information only), $I(X;A\mid h,x_j)$ and $I(X;A\mid h,x_k)$ have the same distribution and $\mathbb{E}[\rho]\le 0$ once the $2$-vs-$3$ receiver counts are accounted for. **Absolute-codebook conventions in Fish have non-positive secrecy rate.** This is why the Hanabi hat-guessing construction (§4.1) does not port.

**Positive construction: state-at-the-decoder channels.** If the codebook is *receiver-relative*, i.e. the semantics of $A$ are a function $\phi(A, x_j)$ of the message and the receiver's own hand, then the relevant capacity is that of a channel with state known only at the decoder:

$$
C = \max_{p(x)} I(X;Y\mid S), \qquad R_s = \max_{p(x)}\big[\,I(X;Y\mid S_j)-I(X;Z)\,\big] \;>\;0 \text{ is achievable.}
$$

The eavesdropper must marginalise over $S_j$ (the teammate's unknown cards), the teammate does not. This is the one clean, provable source of asymmetry available in Fish. §7.9 gives a concrete construction.

### 2.5 Team solution concepts: TME, TMECor, TMECom, and the price of uncorrelation

Celli & Gatti (AAAI 2018) define three regimes by communication capability, and Team-PSRO (McAleer et al., NeurIPS 2023) gives the modern scalable form.

Let the team be $\mathcal{T}=\{T_1,\dots\}$, the opponent team $\mathcal{O}$, $\Pi_i$ the reduced-normal-form plans of player $i$, $\hat u_{\mathcal T}(z)=\big(\sum_{i\in\mathcal T} u_i(z)\big)p_C(z)$. Then

$$
u_{\mathcal T}(\mu_{\mathcal T},\mu_{\mathcal O})=\sum_{z\in Z}\hat u_{\mathcal T}(z)\Big(\!\!\sum_{\substack{\pi_{T_1}\in\Pi_{T_1}(z)\\ \pi_{T_2}\in\Pi_{T_2}(z)}}\!\!\mu_{\mathcal T}(\pi_{T_1},\pi_{T_2})\Big)\Big(\!\!\sum_{\substack{\pi_{O_1}\in\Pi_{O_1}(z)\\ \pi_{O_2}\in\Pi_{O_2}(z)}}\!\!\mu_{\mathcal O}(\pi_{O_1},\pi_{O_2})\Big).
$$

- **TMECom** — teammates may communicate before *and during* play (a mediator that receives truthful reports). Computable in polynomial time by reduction to a 2-player game (Celli & Gatti, Thm 2).
- **TMECor** — teammates correlate **only before** play; during play they must signal through public actions. Solve
$$
\max_{\mu_{\mathcal T}\in\Delta(\Pi_{T_1}\times\Pi_{T_2})}\ \min_{\mu_{\mathcal O}\in\Delta(\Pi_{O_1}\times\Pi_{O_2})}\ u_{\mathcal T}(\mu_{\mathcal T},\mu_{\mathcal O}).
$$
At a TMECor, $u_{\mathcal T}(\mu_{\mathcal T},\mu_{\mathcal O})=u_{\mathcal T}(\mathrm{BR}_{\mathcal T}(\mu_{\mathcal O}),\mu_{\mathcal O})=u_{\mathcal T}(\mu_{\mathcal T},\mathrm{BR}_{\mathcal O}(\mu_{\mathcal T}))$. Finding TMECor is **FNP-hard** already for two teammates (Celli & Gatti, Thm 3); the best-response oracle is **APX-hard** (Thm 4).
- **TME** — no correlation at all: $\arg\max_{r_1,\dots,r_{n-1}}\min_{r_n} U_{\mathcal T}\prod_i r_i$; non-convex objective, and $v_{\text{Cor}}\ge v_{\text{No}}$ always.

**Price of uncorrelation** (their inefficiency indices):
$$
\mathrm{PoU}_{\text{Com/No}}=\frac{v_{\text{Com}}}{v_{\text{No}}},\quad
\mathrm{PoU}_{\text{Cor/No}}=\frac{v_{\text{Cor}}}{v_{\text{No}}},\quad
\mathrm{PoU}_{\text{Com/Cor}}=\frac{v_{\text{Com}}}{v_{\text{Cor}}},
$$
shown to be **unbounded in the size of the game tree** (worst-case examples of order $|L|/2$, $|L|/4$, $\sqrt{|L|}$ in the number of leaves $L$).

**Fish is exactly a TMECor problem.** Team-PSRO's own framing is the best one-sentence statement of the problem: two-team zero-sum games are harder than 2p0s "because each player on a team has different information and must use their public actions to signal to other members of the team."

$\varepsilon$-approximate TMECor is measured by exploitability
$$
e(\mu_{\mathcal T},\mu_{\mathcal O})=u_{\mathcal T}(\mathrm{BR}_{\mathcal T}(\mu_{\mathcal O}),\mu_{\mathcal O})+u_{\mathcal O}(\mu_{\mathcal T},\mathrm{BR}_{\mathcal O}(\mu_{\mathcal T})),
$$
which Team-DO/Team-PSRO produce for free each iteration — a genuinely useful progress metric for a Fish bot.

### 2.6 **The value of a convention in a fully public game — formalisation**

This is the question the brief asks to formalise. Here is a complete, implementable answer.

**Setup.** Players $N=\{1,\dots,6\}$, teams $\mathcal{A}=\{1,3,5\}$, $\mathcal{B}=\{2,4,6\}$ (alternating seats). Chance deals $d\in\mathcal{D}$, $|\mathcal{D}|=54!/(9!)^6\approx1.011\times10^{38}$. Player $i$'s private information is $x_i=\mathrm{hand}_i(d)$ and *nothing else ever becomes private*. Public history $h_t^{\text{pub}}$ records every $(\text{asker},\text{target},\text{card},\text{yes/no})$, every declaration and its full allocation, and all hand counts.

**Exact public belief.**
$$
\beta_t(d)\;=\;\frac{\mathbb{1}\!\left[d\in\mathcal{C}_t\right]\;\prod_{\tau<t}\pi_{i_\tau}\!\big(a_\tau\mid x_{i_\tau}(d),\,h_\tau^{\text{pub}}\big)}{\sum_{d'\in\mathcal{D}}\mathbb{1}\!\left[d'\in\mathcal{C}_t\right]\prod_{\tau<t}\pi_{i_\tau}\!\big(a_\tau\mid x_{i_\tau}(d'),\,h_\tau^{\text{pub}}\big)}
\;=\;\frac{\mathbb{1}[d\in\mathcal{C}_t]\,L_t(d)}{Z_t},
$$
with $\mathcal{C}_t$ the *rule-consistent* deals (hard constraints only) and $L_t$ the policy likelihood. Because $\beta_t$ is computed from public data alone, **all six players hold the identical $\beta_t$** — the defining property of a public belief state and the reason PBS methods fit Fish so well.

**Grounded vs. conventional information.** Define the *grounded* belief $\tilde\beta_t(d)\propto\mathbb{1}[d\in\mathcal{C}_t]$ (uniform on rule-consistent deals). Then

$$
\underbrace{D_{\mathrm{KL}}\!\left(\beta_t \,\|\, \tilde\beta_t\right)}_{\text{conventional information (bits)}}
$$

is precisely the number of bits that exist *only because of the policy*, i.e. the size of the convention system in operation at time $t$. This is directly measurable in simulation and is the Fish analogue of BAD's reported "≈40% of information comes from conventions."

**Strategy classes.** Let $\mathcal{A}(x_i,h)$ be the legal ask set. Define the **grounded (babbling) class**
$$
\Pi^{\text{gr}}_{\mathcal T}=\Big\{\pi:\ \pi_i(a\mid x_i,h)=\frac{\nu_i(a\mid h)\,\mathbb{1}[a\in\mathcal{A}(x_i,h)]}{\sum_{a'\in\mathcal{A}(x_i,h)}\nu_i(a'\mid h)}\ \ \forall i\in\mathcal T\Big\},
$$
i.e. the action distribution is a fixed *public* reference measure $\nu_i$ restricted and renormalised to whatever happens to be legal. Under $\Pi^{\text{gr}}$, $L_t(d)$ depends on $d$ only through the legality indicators, so $\beta_t\approx\tilde\beta_t$ up to the renormalisers $Z(x_i,h)$ — i.e. essentially no conventional information. (Exact pooling is impossible in Fish because *legality itself leaks*: asking in half-suit $h$ proves membership. This is the formal statement of "some information is forced.")

**Values.**
$$
V^{\text{conv}}_{\text{pub}}=\max_{\pi_{\mathcal T}\in\Pi_{\mathcal T}}\min_{\pi_{\mathcal O}}u_{\mathcal T},\qquad
V^{\text{gr}}_{\text{pub}}=\max_{\pi_{\mathcal T}\in\Pi^{\text{gr}}_{\mathcal T}}\min_{\pi_{\mathcal O}}u_{\mathcal T}.
$$

$$
\boxed{\ \Delta_{\text{conv}}:=V^{\text{conv}}_{\text{pub}}-V^{\text{gr}}_{\text{pub}}\ \ge 0\ }
$$
is the **value of having a convention system at all**, measured in expected sets (or win probability). This is directly estimable: train/evaluate a policy with a communication-suppressing regulariser (force $\pi_i(\cdot\mid x_i,h)$ toward the legality-renormalised public reference) and compare head-to-head against the unconstrained policy, both against a common strong opponent.

**The eavesdropping tax.** Construct the auxiliary game $G_{\text{priv}}$ identical to Fish except that opponents observe a coarsening $\varphi(a_\tau)$ of each ask (e.g. they learn only that a transfer of *some* card in the half-suit occurred, or they learn the outcome but not which of the legal asks was selected), while teammates still observe $a_\tau$ in full. Let $V^{\text{conv}}_{\text{priv}}$ be its value. Under the TMECor abstraction (each team = one ex-ante-correlating maximiser/minimiser) the game is 2p0s, so garbling the minimiser's signal weakly raises the maximiser's value:
$$
\boxed{\ \Delta_{\text{eaves}}:=V^{\text{conv}}_{\text{priv}}-V^{\text{conv}}_{\text{pub}}\ \ge 0.\ }
$$
$\Delta_{\text{eaves}}$ is **the price of the opponents listening** and is the single most important quantity to measure empirically for the Fish bot. It is a 20-line environment change and one training run.

**Decomposition.** Trivially,
$$
\Delta_{\text{conv}}=\underbrace{\big(V^{\text{conv}}_{\text{priv}}-V^{\text{gr}}_{\text{priv}}\big)}_{\text{gross signalling value}}\;-\;\underbrace{\big(\Delta_{\text{eaves}}(\text{conv})-\Delta_{\text{eaves}}(\text{gr})\big)}_{\text{incremental eavesdropping tax}} .
$$
A convention is worth adopting iff gross signalling value exceeds its incremental tax — **the multi-agent, sequential restatement of Farrell–Gibbons' $v_i+w_i\ge0$.**

**Local (per-action) version usable inside search.** Maintain two filters in parallel: the full filter $\beta$ and the grounded filter $\tilde\beta$. With a PBS value function $V_{\mathcal T}(\cdot)$ (§3.5), the *realised signalling value* of playing $a$ at $\beta$ is
$$
\Delta_{\text{sig}}(a)\;=\;V_{\mathcal T}\big(\beta^{+a}\big)\;-\;V_{\mathcal T}\big(\tilde\beta^{+a}\big),
$$
the value difference between "everyone updates on my policy" and "everyone updates on the rules only." This is the exact quantity a Fish bot should be trading off; it is signed, so a convention that helps opponents more than teammates shows up as negative. It is also precisely the contrast that Off-Belief Learning zeroes out by construction (§4.5), which makes OBL the natural training-time control knob.

**Why the harm term is unusually large in Fish.** Suppose the public belief assigns probability $q$ that player $i$ holds card $c\in h$. If $q\to1$ and *any* opponent holds a card of $h$, that opponent can ask for $c$ and is guaranteed a hit — which by the rules means they *keep the turn*. So the marginal harm of revealing a card location is approximately

$$
\text{Harm}(c)\ \approx\ \Pr\big[\exists k\in\mathcal{O}: x_k\cap h\ne\emptyset\big]\times\Big(\text{value of }c\ +\ \text{value of a free turn continuation}\Big),
$$

with the first factor typically large (a 6-card half-suit split across 6 players is very likely to touch the opposing team). Contrast:
- **Hanabi**: no adversary, so $w_i\equiv0$ and every bit of signal is pure profit. This is why Hanabi conventions are so elaborate.
- **Bridge bidding**: leaked information changes opponents' *probabilistic* play choices and defensive leads; the harm is real but diffuse.
- **Fish**: leaked information is *directly executable* by opponents as a guaranteed card transfer plus tempo.

**Design corollary.** In Fish the equilibrium sits far toward pooling on card-identity information, with conventional content concentrated in (i) rule-forced dimensions (half-suit membership), (ii) low-harm dimensions (target choice), and (iii) receiver-relative codebooks with positive secrecy rate (§2.4, §7.9). A Fish bot that imports Hanabi-style convention density will bleed value to opponents.

---

## 3. Public belief states: definitions and how to train a PBS-conditioned policy

### 3.1 PBS in ReBeL (Brown, Bakhtin, Lerer & Gong, NeurIPS 2020)

A public belief state is a joint distribution over each agent's possible information states given the public state:
$$
\beta=\big(\Delta \mathcal S_1(s_{\text{pub}}),\ \dots,\ \Delta \mathcal S_N(s_{\text{pub}})\big),
$$
where $\mathcal S_i(s_{\text{pub}})$ is the set of infostates agent $i$ may occupy. The PBS value under policy profile $\pi$:
$$
V^\pi_i(\beta)=\sum_{h\in\mathcal H(s_{\text{pub}}(\beta))}p(h\mid\beta)\,v^\pi_i(h),
$$
and in 2p0s every PBS has a unique value with $V_1(\beta)=-V_2(\beta)$. The infostate value against an equilibrium opponent:
$$
v^{\pi^*}_i(s_i\mid\beta)=\max_{\pi_i}\sum_{h\in\mathcal H(s_i)}p(h\mid s_i,\beta_{-i})\,v_i^{\langle\pi_i,\pi^*_{-i}\rangle}(h).
$$
ReBeL runs CFR-D over subgames rooted at $\beta$, setting leaf values from a learned network $\hat v(s_i(z)\mid\beta_z^{\pi^t},\theta^v)$, averaging over $T$ iterations, and training the value net with **pointwise Huber loss** and the policy net with **MSE over probabilities**. Search is described as gradient-ascent-like on supergradients of the PBS value function at leaf nodes. Result: superhuman heads-up no-limit hold'em with far less domain knowledge than prior bots.

> **Caveat for Fish:** ReBeL's convergence guarantee is *two-player zero-sum*. Fish is 6-player, 2-team. The PBS *representation* transfers; the guarantee does not. See §7.1.

### 3.2 PBS in Player of Games / Student of Games (Schmid et al.)

$s_{\text{pub}}(h)$ is the sequence of public observations along $h$; $\beta=(s_{\text{pub}},r)$ with $r\in\Delta(\mathcal S_1(s_{\text{pub}}))\times\Delta(\mathcal S_2(s_{\text{pub}}))$ ("ranges"). The **counterfactual value-and-policy network** implements $f_\theta(\beta)=(\mathbf v,\mathbf p)$: one counterfactual value per infostate per player, one prior policy per infostate for the acting player. **GT-CFR** alternates (i) public-tree CFR updates on the current tree $\mathcal L_t$ and (ii) simulation-driven expansion, querying $f_\theta(\beta')$ at frontier belief states. Theorem 2 bounds exploitability after $D$ continual re-solving steps as $(5D+2)\big(F\varepsilon+NU\sqrt{A/T}\big)$. Results: chess $\approx+420$ Elo over the Stockfish baseline used, Go $+1970$ Elo vs GnuGo, poker $7\pm3$ mbb/h vs Slumbot, Scotland Yard 55% vs PimBot.

**Scotland Yard is the closest published analogue to Fish**: a many-vs-one hidden-position game with public moves — evidence that PBS+search handles non-poker public-observation structures.

### 3.3 The public belief MDP and *prescriptions* (BAD; CAPI; Team Belief DAG)

**Bayesian Action Decoder** (Foerster, Song, Hughes, Burch, Dunning, Whiteson, Botvinick & Bowling, ICML 2019). Public belief
$$
\mathcal B_t=P\!\left(f^{\text{pri}}_t \mid f^{\text{pub}}_{\le t}\right),
$$
updated on observing action $u^a_t$ produced by a **deterministic partial policy** $\pi_\Delta$:
$$
P\!\left(f^a_t \mid u^a_t,\mathcal B_t,f^{\text{pub}}_t,\pi_\Delta\right)\ \propto\ \mathbb{1}\big(\pi_\Delta(f^a_t)=u^a_t\big)\,P\!\left(f^a_t\mid \mathcal B_t,f^{\text{pub}}_t\right).
$$
The **PuB-MDP** has state $s_{\text{BAD}}=\{\mathcal B,f^{\text{pub}}\}$, actions the deterministic partial policies $\pi_\Delta:\{f^a\}\to\mathcal U$, and reward
$$
r_{\text{BAD}}(s_{\text{BAD}},\pi_\Delta)=\sum_{f^{\text{pri}}}\mathcal B(f^{\text{pri}})\,r\big(s,\pi_\Delta(f^{\text{pri}})\big).
$$
Factorised belief approximation: $P(f^{\text{pri}}_t\mid f^{\text{pub}}_{\le t})\approx\prod_i P(f^{\text{pri}}_t[i]\mid f^{\text{pub}}_{\le t})$. Results: 24.174 ± 0.004 in 2P Hanabi, 58.6% perfect games, beating FireFlower (23.37, 52.6%).

**CAPI** (Sokota, Lockhart, Timbers, Davoodi, D'Orazio, Burch, Schmid, Bowling & Lanctot, 2021). Converts a factored-observation stochastic game into a PuB-MDP where a coordinator issues **prescription vectors** $\Gamma$ mapping each player's private info state to an action, and does approximate policy iteration in that MDP. Factorised prescription policy
$$
P(\Gamma\mid b)=\prod_i\prod_{s_i}\pi\big(\Gamma(s_i)\mid b\big),
$$
which reduces the prescription space from $|\mathcal A_i|^{N\cdot|\mathcal S_i|}$ to $|\mathcal A_i|\cdot N\cdot|\mathcal S_i|$ parameters. Solved Trade Comm 30/32 runs (vs 0/32 for IQL, HQL, IA2C2, VDN, SAD) and abstracted Tiny Bridge 18/32 (vs 0/32).

**Team Belief DAG** (Zhang, Farina, Celli & Sandholm, 2022). Generalises the sequence form to team games: nodes are *beliefs* $B$ (sets of game nodes jointly possible for the team), edges are *prescriptions* (one action per infoset intersecting $B$), and children are the connected components of the connectivity graph (nodes $h,h'$ in the same layer joined when some team infoset contains prefixes of both). Theorem 4.2: the DAG and the original team decision problem are strategically equivalent. Theorem 4.3: CFR on a DAG with $N$ nodes and $E$ edges has regret $O(N\sqrt T)$ with $O(E)$ per iteration. Theorem B.3 ($k$-private case): the DAG has $O^*\big((b+1)^k\big)$ edges where $b$ is the branching factor and $k$ bounds the number of distinct last-infosets per public state.

> **Fish sizing:** the $k$-private bound is the relevant one. In Fish, at a given public state the team's three members each have a distinct private hand, so $k$ is effectively the number of *distinct hand-consistent private states per public state* — astronomically large. **The exact TB-DAG is not buildable for Fish.** Use it as the conceptual target (prescriptions over public states) and approximate with neural prescriptions in the CAPI style.

### 3.4 Concretely: the Fish public belief state

Represent $\beta_t$ by the **marginal location matrix**
$$
B_t\in\mathbb R^{54\times 7},\qquad B_t[c,i]=\Pr\big(\text{card }c\text{ is held by player }i \mid h_t^{\text{pub}}\big),
$$
with column 7 = "already declared / out of play." Constraints that must hold exactly:
$$
\sum_{i} B_t[c,i]=1\ \ \forall c,\qquad \sum_{c} B_t[c,i]=n_i(t)\ \ \forall i\in N,
$$
where $n_i(t)$ is $i$'s publicly known hand size. Plus hard zeros: $B_t[c,i]=0$ whenever $i$ has publicly denied $c$, or $i$ asked for $c$ (proving they lacked it at that time and did not receive it since), etc. Plus positivity constraints of the form "player $i$ held at least one card of half-suit $h$ at time $\tau$" which are *not* expressible as marginals — these are the genuinely hard ones.

**Recipe (implementable):**
1. Bayesian update the marginals from the ask/answer likelihood under the current policy: for an ask $a_\tau$ by $i$, multiply $B[\cdot,i]$ entrywise by $\pi_i(a_\tau\mid\cdot)$-derived evidence, then
2. **Sinkhorn-project** $B$ onto the transportation polytope $\{B\ge0,\ \sum_i B[c,i]=1,\ \sum_c B[c,i]=n_i\}$ (alternating row/column normalisation) to restore card conservation and hand sizes;
3. for the *joint* constraints (half-suit membership at past times, declaration consistency), maintain a **particle filter**: $M$ weighted candidate deals $d^{(m)}$ with weights $w^{(m)}\propto L_t(d^{(m)})$; resample with rejuvenation moves that swap unknown cards between players while preserving hand sizes. Use $M\sim10^3$–$10^4$.
4. Alternatively/additionally learn an **autoregressive belief net** in the LBS style:
$$
P_\phi(f^{-i}\mid \tau^i)=\prod_j P\big(f_j^{-i}\mid f_{<j}^{-i},\tau^i\big),
$$
trained by supervised learning on self-play deals — this is what made SPARTA-style search affordable at scale.

### 3.5 Training a policy conditioned on a PBS

Standard recipe distilled from ReBeL + BAD + CAPI + Player of Games, adapted to a 6-player two-team game:

**Networks.** $V_\theta(\beta)\in\mathbb R$ (team-A value; use two heads or antisymmetrise), and $\pi_\psi(a\mid \beta, x_i)$ — condition on the *public* belief and the *private* hand. Architecturally, use the LBS "public–private" split: an LSTM/transformer over $h^{\text{pub}}$ producing $h(\tau^{\text{pub}}_t)$, then a small MLP combining it with $x_i$:
$$
\pi(\tau^i_t)\ \to\ \pi\big(h(\tau^{\text{pub}}_t),\,f^i_t\big).
$$
This lets you re-evaluate the policy for *many candidate hands* at one PBS with a single public-trunk forward pass — essential for belief-conditioned search (LBS reported this as the key efficiency trick).

**Data generation loop.**
1. Self-play from the root; at each public state form $\beta_t$.
2. Run a bounded subgame solve rooted at $\beta_t$ (CFR-D-style if you use the 2p0s abstraction; or CAPI-style prescription policy iteration if you treat the team as a coordinator), with leaf values from $V_\theta$.
3. Emit targets $(\beta_t,\hat v_t,\hat\pi_t)$; ReBeL's unbiasedness trick is to *sample a random leaf PBS* to continue from, so the value targets are unbiased.
4. Fit $V_\theta$ with Huber loss, $\pi_\psi$ with cross-entropy/MSE to $\hat\pi$.

**Fish-specific modifications.**
- Two teams and three opponents mean no unique value; use the **Team-PSRO** outer loop (populations $\Pi_{\mathcal T},\Pi_{\mathcal O}$, restricted-game meta-Nash, joint best response by cooperative RL — MAPPO with a centralised critic is the reference choice) so you retain an *exploitability* estimate $e(\mu_{\mathcal T},\mu_{\mathcal O})$ as a progress metric.
- Because the team must correlate ex ante, sample a **shared correlation seed** $\omega\sim\mathcal U$ at the start of each episode and give it to all three teammates as an input. This literally implements the TMECor correlation device and is nearly free; it lets the team randomise *jointly* over convention systems, which is a real and often-missed source of unexploitability (an opponent cannot know which convention book is in force this deal).

---

## 4. Learned conventions in cooperative games (the Hanabi line of work)

### 4.1 Hand-designed information-theoretic conventions: the hat-guessing family

Cox, De Silva, DeOrsey, Kenter, Retter & Tobin (*Mathematics Magazine* 88(5):323–336, 2015) is the canonical construction. In the $8$-colour, $5$-player hat game, player $P_1$ announces
$$
r_1:=\sum_{i\ne1}c_i \ (\mathrm{mod}\ 8),
$$
and every other player $P_i$, $i>1$, recovers their own colour as
$$
r_i:=r_1-\sum_{j\ne1,i}c_j\ (\mathrm{mod}\ 8)\ \equiv\ \sum_{j\ne1}c_j-\sum_{j\ne1,i}c_j\ \equiv\ c_i \ (\mathrm{mod}\ 8).
$$
One public message conveys a *different* private fact to each of four receivers — a "check digit"/network-coding scheme. Applied to Hanabi by mapping each player's hand to a colour $0..7$ encoding a recommended action (0–3: play $C_1..C_4$; 4–7: discard $C_1..C_4$), a single hint carries a *custom recommendation to every other player*. Their information strategy achieved a perfect score over 75% of the time; later hat-based bots (Bouzy; Wu's WTFWThat) reach ~24.9 average and ~90%+ perfect in 4–5 player games. The Hanabi Challenge reports WTFWThat at 24.20 (3P), 24.83 (4P), 24.89 (5P).

**Why it does not port to Fish (important).** The construction requires each receiver to *see the other receivers' hidden state* — in Hanabi you see all hands but your own. In Fish nobody sees anyone's hand, so there is no $\sum_{j\ne1,i}c_j$ term for a receiver to subtract. A modular code broadcast in Fish is decoded identically by teammates and opponents: **zero secrecy, full leak.** See §7.9 for the correct Fish analogue (receiver-relative frames).

### 4.2 Conventions in the Hanabi Challenge (Bard, Foerster, Chandar, Burch, Lanctot, Song, Parisotto, Dumoulin, Moitra, Hughes, Dunning, Mourad, Larochelle, Bellemare & Bowling, *AIJ* 280, 2020)

Conventions are formalised as *constraints on the policy to enact the convention*. The canonical example is the **finesse**: a hint that appears locally unplayable, whose interpretation requires the receiver to reason about *why* it was given, implying a third player holds a rescuing card. The paper's central empirical warning: **different independent ACHA runs learn mutually incompatible conventions and, when cross-played, "performance drops off sharply, with some agents scoring essentially zero."** Their benchmark table (2P/3P/4P/5P):

| Agent | 2P | 3P | 4P | 5P | Perfect % (2P) |
|---|---|---|---|---|---|
| SmartBot (rule-based) | 22.99 | 23.12 | 22.19 | 20.25 | 29.6% |
| FireFlower (human-style) | 22.56 | 21.05 | 21.78 | — | 52.6% |
| WTFWThat (hat/info-encoding) | 19.45 | 24.20 | 24.83 | 24.89 | 0.28% |
| Rainbow (RL, 100M steps) | 20.64 | 18.71 | 18.00 | 15.26 | 2.5% |
| ACHA (RL, 20B steps) | 22.73 | 20.24 | 21.57 | 16.80 | 15.1% |
| BAD (belief-based, 2P) | 23.92 | — | — | — | 58.6% |

Also note: exploration is *holistically* damaging to conventions — $\varepsilon$-greedy and entropy regularisation perturb single actions but conventions are global objects, so single-action exploration "ignores their holistic impact."

### 4.3 SAD: the exploration-vs-communication dilemma (Hu & Foerster, ICLR 2020)

Under $\varepsilon$-greedy, a teammate's Bayesian update is blurred:
$$
\pi^{a'}\!\left(u^{a'}_t\mid O(a',\tau_t)\right)=(1-\varepsilon)\,\mathbb{1}\big(u^*(\tau_t)=u^{a'}_t\big)+\frac{\varepsilon}{|U|}.
$$
SAD's fix: during centralised training each agent emits **two** actions — the exploratory $u^a$ actually executed and the greedy $u^*$ passed to teammates as a side input — so partners get the clean update
$$
P(\tau_t\mid \tau^a_t,u^*)=\frac{\mathbb{1}\big(u^*(\tau_t)=u^*\big)\,\mathcal B(\tau_t)}{\sum_{\tau'}\mathbb{1}\big(u^*(\tau')=u^*\big)\,\mathcal B(\tau')}.
$$
At test time $\varepsilon=0$ and the executed action *is* the greedy action, so nothing is lost. Results: 24.01 / 23.93 / 23.81 / 23.01 for 2/3/4/5 players (52.39% / 48.05% / 41.45% / 13.93% perfect).

**This is a free win for Fish.** Any self-play training of a Fish bot must decouple "the ask I explore with" from "the ask my teammates should decode," or exploration noise will destroy every emerging convention.

### 4.4 Search over beliefs: SPARTA and Learned Belief Search

**SPARTA** (Lerer, Hu, Foerster & Brown, AAAI 2020). Common-knowledge belief update:
$$
B^i(\tau_t)=\frac{B^i(\tau_{t-1})\,\pi^j\!\left(a^j_t\mid\tau_{t-1}\right)P\!\left(o^i_t\mid\tau_{t-1},a^j_t\right)}{\sum_{\tau'_{t-1}}B^i(\tau'_{t-1})\,\pi^j\!\left(a^j_t\mid\tau'_{t-1}\right)P\!\left(o^i_t\mid\tau'_{t-1},a^j_t\right)}.
$$
Theorem 1 (no-regret w.r.t. blueprint):
$$
V_{\pi_s}-V_{\pi_b}\ \ge\ -2T\Delta|\mathcal A|N^{-1/2},
$$
with $N$ rollouts/step, $\Delta$ the reward range, $T$ the horizon. Results on SAD blueprint: 24.08 (56.1%) $\to$ 24.53 (71.1%) single-agent $\to$ 24.61 (75.5%) multi-agent — at $\sim1.8\times10^8$ rollouts/game for multi-agent vs $1.5\times10^5$ for single-agent, with a `max_range` fallback (search ran at 86% of timesteps with range 2000).

**Learned Belief Search** (Hu, Lerer, Brown & Foerster, 2021) replaces the exact belief with the autoregressive $P_\phi$, uses $N$-step rollouts plus a bootstrap:
$$
Q(a^i\mid\tau^i)=\mathbb E_{\tau\sim P(\tau\mid\tau^i)}\,Q(a^i\mid\tau),
$$
$$
R^t(\tau')\simeq\sum r_{t'}+\sum_i Q^i_{\text{BP}}\big(a^i_{\text{BP}}\mid f^i_{t+N},\tau^{\text{pub}}_{t+N}\big).
$$
Speedups vs SPARTA: 4.6× (5-card, 90% of the gain), 42× (6-card, 76% of the gain); on the 7-card variant SPARTA OOMs and LBS still works.

### 4.5 Making conventions transferable: OP, OBL, k-level, TrajeDi

**Other-Play** (Hu, Lerer, Peysakhovich & Foerster, ICML 2020). Let $\Phi$ be the group of payoff-irrelevant symmetries of the Dec-POMDP:
$$
\phi\in\Phi \iff P\big(\phi(s')\mid\phi(s),\phi(\mathbf a)\big)=P(s'\mid s,\mathbf a)\ \wedge\ R\big(\phi(s'),\phi(\mathbf a),\phi(s)\big)=R(s',\mathbf a,s)\ \wedge\ O\big(\phi(o)\mid\phi(s),\phi(\mathbf a),i\big)=O(o\mid s,\mathbf a,i).
$$
Self-play maximises $J(\pi^1,\pi^2)$; other-play maximises
$$
\boxed{\ \boldsymbol\pi^*=\arg\max_{\boldsymbol\pi}\ \mathbb E_{\phi\sim\Phi}\ J\big(\pi^1,\phi(\pi^2)\big).\ }
$$
Lever game intuition: with ten symmetric levers paying $1.0$ and one paying $0.9$, SP randomises among the symmetric levers (expected $0.1$) while OP reliably picks the asymmetric $0.9$ lever. Hanabi results:

| Method | Cross-Play | Cross-Play(*) | Self-Play |
|---|---|---|---|
| SAD | 2.52 ± 0.34 | 3.02 ± 0.39 | 23.97 ± 0.04 |
| SAD + OP | 15.32 ± 0.65 | 18.28 ± 0.36 | 23.93 ± 0.02 |
| SAD + AUX | 17.65 ± 0.69 | 21.09 ± 0.18 | 24.09 ± 0.03 |
| SAD + AUX + OP | 22.07 ± 0.11 | 22.49 ± 0.18 | 24.06 ± 0.02 |

With humans: OP 15.75 avg (45% bomb rate) vs SP 9.15 (85% bomb rate), $p=0.00411$.

**Off-Belief Learning** (Hu, Lerer, Cui, Pineda, Brown & Foerster, ICML 2021). Optimise assuming *past* actions came from a fixed $\pi_0$ but *future* actions come from $\pi_1$:
$$
V^{\pi_0\to\pi_1}(\tau^i)=\mathbb E_{\tau\sim\mathcal B_{\pi_0}(\tau^i)}\big[V^{\pi_1}(\tau)\big],\qquad \mathcal B_{\pi_0}(\tau^i)=P(\tau\mid\tau^i,\pi_0),
$$
$$
Q^{\pi_0\to\pi_1}\!\left(a\mid\tau^i_t\right)=\sum_{\tau_t,\tau_{t+1}}\mathcal B_{\pi_0}\!\left(\tau_t\mid\tau^i_t\right)\Big[R(s_t,a)+\mathcal T(\tau_{t+1}\mid\tau_t)V^{\pi_1}(\tau_{t+1})\Big],
$$
$$
\pi_1(a\mid\tau^i)=\frac{\exp\!\big(Q^{\pi_0\to\pi_1}(a\mid\tau^i)/T\big)}{\sum_{a'}\exp\!\big(Q^{\pi_0\to\pi_1}(a'\mid\tau^i)/T\big)}.
$$
Grounded belief (uniform-random $\pi_0$):
$$
\mathcal B_G(\tau\mid\tau^i)=\frac{P(\tau)\prod_t P(o^i_t\mid\tau)}{\sum_{\tau'}P(\tau')\prod_t P(o^i_t\mid\tau')}.
$$
Theorems: (1) OBL converges to a unique $\pi_1$ for any $T>0$; (2) $J(\pi_1)\ge J(\pi_0)-t_{\max}T/e$, enabling an improvement ladder; (4) constant $\pi_0$ with $T\to0$ yields the optimal *grounded* policy. Hanabi 2P:

| Method | Self-Play | Cross-Play | w/ Clone Bot |
|---|---|---|---|
| OBL Level 1 | 20.92 ± 0.07 | 20.85 ± 0.03 | 13.56 ± 0.15 |
| OBL Level 4 | 24.10 ± 0.01 | 23.76 ± 0.06 | 16.76 ± 0.16 |
| Other-Play | 24.14 ± 0.03 | 21.77 ± 0.68 | 8.55 ± 0.48 |

**OBL level $k$ is literally a dial on convention depth**, and it maps 1-to-1 onto the Fish grounded/conventional decomposition of §2.6. Level 1 $=$ $\tilde\beta$-only policy. This is the single most useful algorithmic import for Fish.

**k-level reasoning for ZSC** (Cui, Hu, Pineda & Foerster, NeurIPS 2021/2022, arXiv:2207.07166) builds cognitive-hierarchy agents on top of OP; **TrajeDi** (Lupu, Cui, Hu & Foerster, ICML 2021) generates *diverse* partner populations so the trained agent doesn't lock onto one convention; **Noisy ZSC** (Anwar, Pandian, Wan, Krueger & Foerster, arXiv:2411.04976) drops the assumption that the Dec-POMDP itself is common knowledge — relevant if your Fish bot will meet humans with slightly different rule interpretations (e.g. house rules on the joker half-suit or on cardless-player turn passing).

**piKL / human-regularised search** (Jacob, Wu, Farina, Lerer, Hu, Bakhtin, Andreas & Brown, ICML 2022; Hu, Wu, Lerer, Foerster & Brown, arXiv:2210.05125). Maximise reward minus $\lambda\,D_{\mathrm{KL}}$ from a behaviour-cloned anchor. piKL-Hedge update at iteration $t$:
$$
\pi^{(t)}\ \propto\ \exp\!\left(\frac{\eta\,\bar Q^{(t)}+t\lambda\eta\,\log\tau_{\text{anchor}}}{1+t\lambda\eta}\right),
$$
with Theorem 1 bounding KL from the anchor as $O(1/\lambda)$ and Theorem 2 giving an $O(\lambda)$-approximate Nash in 2p0s. Empirically: chess top-1 human-move accuracy 53.2%→54.3%, Go 57.8%→58.5%, Diplomacy piKL-HedgeBot 32.9% average score.

**Benchmark reality check** (AH2AC2, arXiv:2506.21490, 2025). Human proxies trained on 101,096 2P / 46,525 3P hanab.live games (human averages 23.37 / 23.25). Agent scores against proxies: OBL(L4) **21.04** (2P), BR-BC 19.41 (2P) / 11.89 (3P), HDR-IPPO 12.76 / 14.03, human proxies with each other 22.76 / 20.86. The headline: **existing methods cannot effectively integrate small human datasets to improve coordination — OBL wins without using human data at all.**

---

## 5. Bridge bidding: the only published "learned language under adversarial observation"

### 5.1 Rong, Qin & An, "Competitive Bridge Bidding with Deep Neural Networks" (AAMAS 2019, arXiv:1903.00900)

Two networks:
- **ENN** (estimation): maps own cards + vulnerability + bidding history $\to$ probability distribution over partner's 52 cards. 8 fully-connected layers × 1500 units, skip connections every 2 layers, **52 sigmoid outputs**, loss = sum of per-neuron cross-entropies (multi-label).
- **PNN** (policy): own cards + vulnerability + bidding history + ENN output $\to$ 38-way softmax over bids/pass/double/redouble. 10 layers × 1200 units, skip connections.

Feature encoding: bidding history as a **318-dim binary vector** ($3+9\times35$ slots) indicating which bids occurred; cards as 52-dim binary; vulnerability 2-dim.

RL update (REINFORCE with duplicate-bridge score $r$ over $M$ bids):
$$
\theta\leftarrow\theta+\alpha\,r\,\frac{1}{M}\sum_{i=1}^{M}\nabla_\theta\log\sigma_\theta(b_i\mid s_i).
$$
Training: supervised on 12M instances from 1M expert games, then self-play RL over 2M random deals with a historical opponent pool and DDA scoring on 100-game mini-batches. Result: **+0.25 IMP over Wbridge5** on 64 random boards (the authors note 0.1 IMP is already a meaningful margin).

**The relevant lesson for Fish:** the ENN/PNN split is exactly the belief-net / policy-net split we want, and it is trained under *competitive* bidding, i.e. with opponents listening and interfering. That is the closest published precedent to Fish's public-signal tension.

### 5.2 Lockhart, Burch, Bard, Borgeaud, Eccles, Smaira & Smith, "Human-Agent Cooperation in Bridge Bidding" (DeepMind, arXiv:2011.14124)

Pipeline: (1) imitate WBridge5 to 93.9% action accuracy; (2) **Borel search with a non-deterministic model** — jointly sample private card distributions consistent with the auction, filter particles by policy probability, roll out; posterior policy
$$
\pi_{\text{post}}\ \propto\ \pi_{\text{prior}}\times\exp\!\big(V(a)/(t\sqrt R)\big);
$$
(3) policy iteration: distil search results back into the network with **soft** updates so the agent does not drift to an incompatible equilibrium. Results: **+0.48 IMPs/deal** with a WBridge5 partner, **+0.85** with a copy of itself, **+0.97 IMPs/deal** with human experts over 32 deals. Qualitatively the agent stays inside Standard American Yellow Card and "prefers simpler, more direct auctions, which are more robust to slight differences in interpretation."

**Two directly transferable lessons for Fish:** (i) *anchor the convention system to a hand-written reference policy and use soft updates*, so the learned language stays human-legible and robust; (ii) *prefer robust, low-variance signals over maximally informative ones* — a signal that survives partner misinterpretation is worth more than a fragile one.

### 5.3 NooK / NukkAI (2022) — read the fine print

NooK (NukkAI, Véronique Ventos et al.) won 67 of 80 ten-deal sets (83%) against eight world champions in a Paris challenge, and is a neurosymbolic system that explains its plays. **However, the challenge removed bidding entirely and both humans and NooK played declarer against the same Wbridge5 defenders.** As reported, "skipping the bidding process and playing only the declarer role removed challenging and nuanced parts of the game in which partners must communicate with each other and deceive their opponents." So NooK is *not* evidence about learned signalling; it is evidence that neurosymbolic + explanation is viable for card-play search. Cited here to prevent a common misreading.

> Verification note: NooK details come from press coverage (CBC, SingularityHub, Slashdot) — I found no peer-reviewed primary source. **UNVERIFIED as to technical details.**

---

## 6. Information-theoretic measures of how much an action conveys

### 6.1 Positive signalling and positive listening (Lowe, Foerster, Boureau, Pineau & Dauphin, AAMAS 2019)

- **Positive signalling** (Def. 3.1): the message sequence is statistically dependent on observations or actions, $\bar m\not\perp\bar o$ or $\bar m\not\perp\bar a$.
- **Positive listening** (Def. 3.2): there exists a message such that $\|\pi(o,\mathbf 0)-\pi(o,m)\|>0$.
- **Speaker consistency**: $\mathrm{SC}=\sum_{a,m}p(a,m)\log\frac{p(a,m)}{p(a)p(m)}$.
- **Instantaneous coordination**: $\mathrm{IC}=I(m^k_t;a^j_{t+1})$ (symbol/action) or $I(a^k_t;a^j_{t+1})$ (action/action).
- **Causal influence of communication (CIC)**: intervene on the message distribution rather than averaging over episodes.

**The central negative result:** agents can score high SC while messages have *zero causal effect*; scrambling the messages before delivery leaves SC unchanged. **Any Fish convention metric must be validated with a scramble/ablation test.**

### 6.2 Inductive biases that create signalling (Eccles, Bachrach, Lever, Lazaridou & Graepel, NeurIPS 2019)

Positive-signalling loss (target-entropy form, avoiding direct MI maximisation):
$$
\mathcal I\!\left(m^i_t,x^i_t\right)=\mathcal H(m^i_t)-\mathcal H(m^i_t\mid x^i_t),
$$
$$
L_{ps}\big(\pi^i_M,s_i\big)=-\mathbb E\Big[\lambda\,\mathcal H\big(\overline{\pi^i_M}\big)-\big(\mathcal H(m^i_t\mid x_t)-\mathcal H_{\text{target}}\big)^2\Big],\qquad \mathcal H_{\text{target}}\approx\tfrac{1}{2}\log|A|.
$$
Positive-listening loss ($L_1$ surrogate for CIC):
$$
\mathrm{CIC}(x_t)=\mathcal H(a_t\mid x'_t)-\mathcal H(a_t\mid x_t)=D_{\mathrm{KL}}\big((a_t\mid x_t)\,\|\,(a_t\mid x'_t)\big),
$$
$$
L_{pl}(x_t)=-\sum_{a\in\mathcal A^i}\big|\pi^i_A(a\mid x_t)-\overline\pi^i_A(a\mid x'_t)\big|.
$$
Results: near-optimal (~0.98–0.99) on MNIST-summing with 98–100% consistency over 50 runs; Treasure Hunt 94% success with both biases vs 28% with neither.

### 6.3 Social influence as intrinsic motivation (Jaques, Lazaridou, Hughes, Gulcehre, Ortega, Strouse, Leibo & de Freitas, ICML 2019)

$$
c^k_t=\sum_{j\ne k}D_{\mathrm{KL}}\Big[p\big(a^j_t\mid a^k_t,s^j_t\big)\ \Big\|\ p\big(a^j_t\mid s^j_t\big)\Big],
$$
which in expectation is the mutual information between agents' actions,
$$
I\!\left(A^k;A^j\mid z\right)=\sum_{a^k}p(a^k\mid z)\,D_{\mathrm{KL}}\Big[p(a^j\mid a^k,z)\ \Big\|\ p(a^j\mid z)\Big].
$$
Results: influence agents ~1073 collective reward in Harvest (vs 750 prior), 951 in the communication variant; decentralised MOA version 588.

### 6.4 The Fish-specific measures you actually want

Given the theory above, instrument the Fish bot with **four** numbers per decision:

1. **Total leak (bits):** $I(X;A\mid H=h)$ estimated by comparing the entropy of the belief over the actor's hand before/after — equivalently $\mathbb E\big[D_{\mathrm{KL}}(\beta^{+a}\|\tilde\beta^{+a})\big]$.
2. **Teammate-usable leak:** $\frac{1}{2}\sum_{j\in\mathcal T\setminus i} I(X;A\mid h,x_j)$.
3. **Opponent-usable leak:** $\frac{1}{3}\sum_{k\in\mathcal O} I(X;A\mid h,x_k)$.
4. **Realised value differential** $\Delta_{\text{sig}}(a)=V_{\mathcal T}(\beta^{+a})-V_{\mathcal T}(\tilde\beta^{+a})$ — the only one that is an objective; (1)–(3) are diagnostics subject to Lowe et al.'s warning.

A convention is *real* iff (4) is positive and survives a scramble ablation; it is *safe* iff (2) $>$ (3) weighted by actionability.

---

## 7. Applicability to Canadian Fish — technique by technique

### 7.1 ReBeL-style PBS + CFR-D subgame solving
- **Would it help?** The representation, yes, strongly — Fish's information structure is the ideal case (public actions, one-shot private info). The *algorithm's guarantee*, no: ReBeL is proved only for 2p0s. Fish is 6-player/2-team; a TMECor abstraction restores 2p0s at the *team* level only if you can solve joint best responses, which is APX-hard.
- **Compute.** A single subgame at a mid-game PBS in Fish still has an astronomically large belief support ($\le10^{28}$ from one viewpoint). Full CFR-D is not viable. Depth-limited CFR over a *particle-approximated* belief ($10^3$–$10^4$ particles) with a learned leaf value net is viable on a single GPU.
- **Pitfalls.** Value network extrapolation off the belief manifold; the "range" concept requires each player's reach probabilities under a *known common policy*, which breaks if teammates deviate.
- **Adaptation.** Treat each team as a coordinator issuing prescriptions (CAPI style); run the outer loop as Team-PSRO so you get exploitability for free.

### 7.2 BAD / PuB-MDP with prescriptions
- **Would it help?** Yes, as the *training* formulation. Acting on a PBS with prescriptions is exactly what allows a policy to be simultaneously informative and optimal.
- **Cost.** Prescription space is $|\mathcal A|^{|\mathcal S_i|}$; use CAPI's factorisation $P(\Gamma\mid b)=\prod_i\prod_{s_i}\pi(\Gamma(s_i)\mid b)$ to reduce to $|\mathcal A|\cdot N\cdot|\mathcal S_i|$ parameters. Even then $|\mathcal S_i|$ (hands consistent with public info) is huge — sample a minibatch of consistent hands per PBS and prescribe only for those.
- **Pitfall.** BAD's factorised belief is a *product* approximation; in Fish the hand-size and card-conservation constraints make independence badly wrong. Fix with the Sinkhorn projection of §3.4.

### 7.3 Off-Belief Learning
- **Would it help?** This is the highest-value import. It gives (a) a policy with *zero* arbitrary handshakes at level 1, (b) a principled ladder to level $k$, and (c) robustness to teammates who are humans or other bots.
- **Cost.** Requires a belief model $\mathcal B_{\pi_0}$ over deals to sample counterfactual trajectories — the same particle filter / autoregressive net you need anyway. Roughly 2–3× the cost of plain self-play per step.
- **Pitfall.** Level-1 OBL in Fish will underperform a convention-rich policy *against a jointly trained teammate*. Use OBL level as an explicit hyperparameter and pick it by the deployment scenario (fixed partner ⇒ high $k$ or plain SP; unknown partner ⇒ low $k$).
- **Adaptation.** Fish's grounded belief $\tilde\beta$ is far more informative than Hanabi's, because legality constraints are strong (asking in $h$ *proves* membership). Expect OBL level 1 in Fish to be much stronger relative to level 4 than it is in Hanabi.

### 7.4 Other-Play
- **Would it help?** Only if Fish has non-trivial payoff-irrelevant symmetries. It does: **suit relabelling** among $\{\clubsuit,\diamondsuit,\heartsuit,\spadesuit\}$ is an exact symmetry of the deal distribution and the rules (it permutes the eight rank-based half-suits in pairs of low/high and leaves the 8s+jokers half-suit fixed). Also **swapping the two jokers**, and **rank-reversal is *not* a symmetry** (low half-suit 2–7 vs high 9–A are distinguishable only by label — actually relabelling low↔high within a suit *is* a symmetry too, since neither confers payoff advantage). And **relabelling the three opponents' seat identities is not** a symmetry (seat order determines turn flow).
- **Concrete OP group.** $\Phi \cong S_4 \text{ (suits)} \times C_2 \text{ (low}\leftrightarrow\text{high within suit)}^{?} \times C_2 \text{ (joker swap)}$ — verify the low/high factor against your exact rules before using it. Implement OP by sampling $\phi\sim\Phi$ per episode and applying it to one team's observation/action encoding.
- **Cost.** Nearly free (a permutation of the card encoding).
- **Pitfall.** OP only removes *symmetry-based* arbitrariness. Hanabi results show OP alone got cross-play only to 15.32 and needed the AUX task to reach 22.07 — expect the same in Fish. Also OP hurt "w/ Clone Bot" scores in the OBL table (8.55 vs OBL-L4's 16.76), i.e. OP can be *worse* than OBL for human-like partners.

### 7.5 SAD (decoupled exploratory/greedy action)
- **Would it help?** Yes, essentially for free, and it is a prerequisite for *any* convention to emerge under $\varepsilon$-greedy or entropy-regularised training.
- **Cost.** One extra input channel of size $|\mathcal A|$ per teammate; zero test-time cost.
- **Adaptation.** In Fish there are two teammates; feed both teammates' greedy asks. Careful: also feed the *opponents'* greedy asks or not? Feed them too during training (they are public at test time anyway) — but note that at test time the executed action *is* the greedy action, so consistency holds for all six.

### 7.6 SPARTA / LBS search at test time
- **Would it help?** SPARTA-style single-agent search on top of a blueprint gave +0.45 Hanabi points and has a no-regret guarantee; in Fish the analogous gain is likely larger because Fish decisions are more sharply value-differentiated (a wrong ask hands over the turn).
- **Cost.** Multi-agent SPARTA at $1.8\times10^8$ rollouts/game is out of the question for Fish. Use **LBS**: autoregressive belief net over deals + $N$-step rollouts + bootstrap from the blueprint $Q$. Budget $10^3$–$10^4$ sampled deals per decision.
- **Pitfall.** SPARTA/LBS assume teammates follow the *known* blueprint. Against humans this fails; use piKL-style anchoring instead (§7.7).
- **Adaptation.** Implement the public/private network split so one public-trunk pass serves all sampled deals — this is where the 4.6–42× speedups came from.

### 7.7 piKL / human-regularised search
- **Would it help?** Only if the Fish bot must partner humans. Then it is the state of the art: it makes the policy simultaneously stronger *and* more human-like.
- **Cost.** Requires a behaviour-cloned anchor, i.e. a human Fish dataset. If none exists, hand-write a strong rule-based reference policy (the AH2AC2 result — that OBL beats human-data methods when data is small — suggests you should not over-invest in a tiny dataset).
- **Formula to implement:** the piKL-Hedge update of §4.5 with $\lambda\in[10^{-3},10^{-1}]$ swept.

### 7.8 Team-PSRO outer loop
- **Would it help?** Yes — it is the only published method that both scales to large team games *and* targets TMECor with a convergence statement. It also yields an exploitability estimate each iteration, which no self-play Fish bot otherwise has.
- **Cost.** Each iteration is a full cooperative-RL best-response training run (MAPPO with centralised critic). Expect 5–20 iterations; each is a full training job.
- **Adaptation.** Use *Mix-and-Match* (allow population policies to be recombined across best responses) — it empirically converged faster. Give all three teammates a shared per-episode correlation seed $\omega$ to realise the ex-ante correlation device.
- **Pitfall.** TMECor best response is APX-hard; RL best responses are approximate, so "convergence to TMECor" degrades to "convergence to an approximate TMECor with error proportional to BR suboptimality."

### 7.9 Conventions: what to actually build for Fish

Combining §2.4 and §2.6, here is the concrete design.

**Channel inventory per ask $a=(\text{target }k,\ \text{card }c\in h)$:**

| Dimension | Bits | Marginal leak to opponents | Verdict |
|---|---|---|---|
| Choice of half-suit $h$ | ~$\log_2 6.16$ | **Forced by rules** (you must hold $h$) | Material decision; free grounded info |
| Choice of target $k$ | $\log_2 3\approx1.58$ | Almost none about your hand | **Prime signalling channel** |
| Choice of card $c$ within $h$ | $\le\log_2 5\approx2.32$ | "I lack $c$"; on a hit, $c$'s location becomes public anyway | **Secondary signalling channel** |
| Declaration timing/content | large | Terminal | Costly, self-verifying signal |

Mean legal asks at deal time $\approx 84$ (median 81, min 27, max 135) $\Rightarrow$ raw channel $\approx 6.39$ bits/ask. Mean askable cards (before ×3 targets) $\approx 28$; mean distinct half-suits per hand $\approx 6.16$.

**Construction A — receiver-relative card code (positive secrecy rate).** Fix a public ordering of the six cards of every half-suit. Let $\mathcal U_h$ be the cards of $h$ whose location is not yet common knowledge. When asking within $h$ for the $t$-th element of $\mathcal U_h$ (mod $|\mathcal U_h|$), the *meaning* is defined relative to the designated teammate $j$'s own holdings:

> "$t$ indexes the $t$-th card of half-suit $h'=\sigma(h)$ **counting up from your own lowest card in $h'$**."

Teammate $j$ resolves this to a unique card. An opponent $k$ must marginalise over $j$'s unknown $h'$ holdings and gets a distribution. Secrecy rate $R_s=I(X;Y\mid S_j)-I(X;Z)>0$. **Cost:** the sender cannot verify $j$'s frame, so the code is lossy from the encoder's side — sender ambiguity is the price of receiver secrecy. Use it for *robust, coarse* facts ("I have length in $h'$"), not exact card identities.

**Construction B — teammate selection by target seat.** With three opponents and two teammates, use the *asked opponent's seat index* to select which teammate's frame applies: $j=\mathcal T[\,k \bmod 2\,]$, reserving the third opponent as "no frame / material ask only." This costs nothing (you were choosing a target anyway) and gives a clean 2-teammate addressing scheme.

**Construction C — deliberate pooling on high-harm dimensions.** For any $c$ whose revelation would let a specific opponent execute an immediate guaranteed take, the policy should be *maximally uninformative*: sample $c$ from the legality-renormalised public reference $\nu$. This is the $\Pi^{\text{gr}}$ class of §2.6 applied selectively — pooling where $w_i \ll 0$, separating where $w_i\approx0$.

**Construction D — declaration as the cash-out.** Model declaration as optimal stopping on the PBS. With $p_t=\Pr[\text{our named allocation is exactly correct}\mid\beta_t]$ and unit set value,
$$
\text{EV}(\text{declare now})=2p_t-1,\qquad
\text{EV}(\text{wait})=\mathbb E\big[\max\big(2p_{t+1}-1,\ \text{EV}(\text{wait at }t+1)\big)\big]-\text{risk}_t,
$$
where $\text{risk}_t$ covers opponents stripping cards from the half-suit and the endgame forced-declaration rule. Declaring is the only way conventional information turns into score, so the *entire* value of a Fish convention should be measured through its effect on $p_t$.

**Construction E — the "safe ask" as a costly signal.** Asking for a card you are near-certain the target lacks deliberately passes the turn. That is a pure Spence-style costly signal: you burn tempo to separate types. It is credible precisely *because* it is costly. Reserve a small set of such asks for the highest-value messages (e.g. "I hold ≥4 of half-suit $h'$; teammate, do not ask in $h'$, prepare to declare").

---

## 8. Pitfalls, negative results and known failure modes

1. **Self-play conventions are arbitrary and non-transferable.** SAD cross-play 2.52 vs self-play 23.97 in Hanabi; ACHA runs cross-playing to near zero. If two independently trained Fish bots must team up, expect catastrophic failure without OP/OBL.
2. **Hanabi conventions do not port to Fish.** The hat-guessing construction depends on players seeing others' hands. In Fish, absolute codebooks have non-positive secrecy rate (§2.4). This is the single biggest trap.
3. **Exploration destroys conventions.** $\varepsilon$-greedy blurs the Bayesian update; single-action exploration cannot evaluate a convention's holistic effect (Hanabi Challenge). SAD's decoupling is the mitigation, but even SAD is a *training-time* fix requiring centralised training.
4. **Signalling metrics lie.** Lowe et al.: high speaker consistency with zero causal effect; scrambled messages give identical scores. Never ship a convention validated only by a mutual-information number.
5. **Positive value of information is not guaranteed off the 2p0s abstraction.** With three imperfectly-coordinated opponents, extra information could in principle hurt them (Hirshleifer / Bassan et al.), so $\Delta_{\text{eaves}}\ge0$ is only guaranteed under the team-as-single-agent abstraction.
6. **TMECor is FNP-hard; the best-response oracle is APX-hard** (Celli & Gatti). Do not expect exact solutions; instrument exploitability instead.
7. **The price of uncorrelation is unbounded** in game size — so a Fish bot without an explicit ex-ante correlation device can be arbitrarily worse than one with. Cheap fix: shared per-episode seed.
8. **Team Belief DAG is not buildable for Fish** — the $k$-private bound $O^*((b+1)^k)$ blows up because each of three teammates has an unrestricted 9-card hand.
9. **Multi-agent SPARTA is compute-prohibitive** at $1.8\times10^8$ rollouts/game; and both SPARTA and LBS assume partners follow the known blueprint, which breaks against humans.
10. **Belief factorisation is badly wrong under hard combinatorial constraints.** BAD's independent-marginals approximation must be repaired (Sinkhorn / particle filter) for Fish's fixed hand sizes and card conservation.
11. **Human-data methods underperform on small datasets.** AH2AC2: OBL(L4) scores 21.04 with human proxies *using no human data*, beating BR-BC at 19.41. Do not build the Fish bot around a small human corpus.
12. **OP can hurt human compatibility.** In the OBL table, Other-Play scored 8.55 with the clone bot vs OBL-L4's 16.76. Symmetry-breaking robustness ≠ human-likeness.
13. **NooK's bridge result is card-play only.** Do not cite it as evidence about learned signalling systems.
14. **Common knowledge of the game itself may fail** (Noisy ZSC). Differences in house rules (joker handling, cardless-player pass rules, whether "I'm willing to receive" may be said) will break conventions silently.
15. **Beware over-signalling in the endgame.** When a team is cardless, the other team must declare everything with no asking — conventional information accumulated earlier becomes the *only* resource. Conversely, late-game asks are near-maximally informative to opponents with few cards left. The optimal signalling rate is non-stationary and should decrease as opponents' hands shrink and their half-suit coverage becomes public.

---

## 9. Bibliography

**Public belief states, search, and equilibrium computation**

1. Brown, N., Bakhtin, A., Lerer, A., Gong, Q. — *Combining Deep Reinforcement Learning and Search for Imperfect-Information Games* (ReBeL). NeurIPS 2020. arXiv:2007.13544. https://arxiv.org/abs/2007.13544 · full text read via https://ar5iv.labs.arxiv.org/html/2007.13544 · NeurIPS PDF: https://proceedings.neurips.cc/paper/2020/file/c61f571dbd2fb949d3fe5ae1608dd48b-Paper.pdf **VERIFIED**
2. Schmid, M. et al. — *Player of Games* / *Student of Games: A unified learning algorithm for both perfect and imperfect information games*. arXiv:2112.03178; Science Advances (2023). https://arxiv.org/pdf/2112.03178 · read via https://ar5iv.labs.arxiv.org/html/2112.03178 · https://www.science.org/doi/10.1126/sciadv.adg3256 **VERIFIED**
3. Foerster, J., Song, F., Hughes, E., Burch, N., Dunning, I., Whiteson, S., Botvinick, M., Bowling, M. — *Bayesian Action Decoder for Deep Multi-Agent Reinforcement Learning*. ICML 2019. arXiv:1811.01458. https://arxiv.org/abs/1811.01458 · read via https://ar5iv.labs.arxiv.org/html/1811.01458 · http://proceedings.mlr.press/v97/foerster19a.html **VERIFIED**
4. Sokota, S., Lockhart, E., Timbers, F., Davoodi, E., D'Orazio, R., Burch, N., Schmid, M., Bowling, M., Lanctot, M. — *Solving Common-Payoff Games with Approximate Policy Iteration* (CAPI). arXiv:2101.04237. https://arxiv.org/pdf/2101.04237 · read via ar5iv **VERIFIED**
5. Lerer, A., Hu, H., Foerster, J., Brown, N. — *Improving Policies via Search in Cooperative Partially Observable Games* (SPARTA). AAAI 2020, 34(05):7187–7194. arXiv:1912.02318. https://arxiv.org/abs/1912.02318 · https://ojs.aaai.org/index.php/AAAI/article/view/6208 · code: https://github.com/facebookresearch/Hanabi_SPARTA **VERIFIED**
6. Hu, H., Lerer, A., Brown, N., Foerster, J. — *Learned Belief Search: Efficiently Improving Policies in Partially Observable Settings*. arXiv:2106.09086. https://arxiv.org/abs/2106.09086 · read via ar5iv **VERIFIED**
7. Moravčík, M. et al. — *DeepStack: Expert-Level Artificial Intelligence in No-Limit Poker*. arXiv:1701.01724. https://arxiv.org/pdf/1701.01724 **PARTIALLY VERIFIED** (located, not read in full this session)

**Adversarial team games / TMECor**

8. Celli, A., Gatti, N. — *Computational Results for Extensive-Form Adversarial Team Games*. AAAI 2018. arXiv:1711.06930. https://arxiv.org/abs/1711.06930 · read via ar5iv **VERIFIED**
9. Farina, G., Celli, A., Gatti, N., Sandholm, T. — *Connecting Optimal Ex-Ante Collusion in Teams to Extensive-Form Correlation: Faster Algorithms and Positive Complexity Results*. ICML 2021, PMLR 139:3164–3173. https://proceedings.mlr.press/v139/farina21a.html **PARTIALLY VERIFIED** (abstract/venue confirmed; full text not read)
10. Zhang, B. H., Farina, G., Celli, A., Sandholm, T. — *Team Belief DAG: Generalizing the Sequence Form to Team Games for Fast Computation of Correlated Team Max-Min Equilibria via Regret Minimization*. arXiv:2202.00789. https://arxiv.org/pdf/2202.00789 · read via ar5iv **VERIFIED**
11. McAleer, S., Farina, G., Zhou, G., Wang, M., Yang, Y., Sandholm, T. — *Team-PSRO for Learning Approximate TMECor in Large Team Games via Cooperative Reinforcement Learning*. NeurIPS 2023. https://proceedings.neurips.cc/paper_files/paper/2023/file/8e4ccc9ca6ae2225c4cbb7782ab48daf-Paper-Conference.pdf (PDF text extracted) **VERIFIED**
12. Carminati, L., Cacciamani, F., Ciccone, M., Gatti, N. — *A Marriage between Adversarial Team Games and 2-player Games: Enabling Abstractions, No-regret Learning, and Subgame Solving*. ICML 2022, PMLR 162. https://proceedings.mlr.press/v162/carminati22a/carminati22a.pdf **PARTIALLY VERIFIED** (located, not read)

**Hanabi: conventions, zero-shot coordination, human coordination**

13. Bard, N., Foerster, J. N., Chandar, S., Burch, N., Lanctot, M., Song, H. F., Parisotto, E., Dumoulin, V., Moitra, S., Hughes, E., Dunning, I., Mourad, S., Larochelle, H., Bellemare, M. G., Bowling, M. — *The Hanabi Challenge: A New Frontier for AI Research*. Artificial Intelligence 280 (2020). arXiv:1902.00506. https://arxiv.org/abs/1902.00506 · read via ar5iv **VERIFIED**
14. Cox, C., De Silva, J., DeOrsey, P., Kenter, F. H. J., Retter, T., Tobin, J. — *How to Make the Perfect Fireworks Display: Two Strategies for Hanabi*. Mathematics Magazine 88(5):323–336, 2015. https://www.tandfonline.com/doi/abs/10.4169/math.mag.88.5.323 · text read from https://gwern.net/doc/reinforcement-learning/imperfect-information/hanabi/2015-cox.pdf **VERIFIED**
15. Hu, H., Foerster, J. N. — *Simplified Action Decoder for Deep Multi-Agent Reinforcement Learning*. ICLR 2020. arXiv:1912.02288. https://arxiv.org/abs/1912.02288 · read via ar5iv · code https://github.com/facebookresearch/hanabi_SAD **VERIFIED**
16. Hu, H., Lerer, A., Peysakhovich, A., Foerster, J. — *"Other-Play" for Zero-Shot Coordination*. ICML 2020. arXiv:2003.02979. https://arxiv.org/abs/2003.02979 · read via ar5iv **VERIFIED**
17. Hu, H., Lerer, A., Cui, B., Pineda, L., Brown, N., Foerster, J. — *Off-Belief Learning*. ICML 2021, PMLR 139:4369–4379. arXiv:2103.04000. https://arxiv.org/abs/2103.04000 · read via ar5iv · https://proceedings.mlr.press/v139/hu21c.html **VERIFIED**
18. Cui, B., Hu, H., Pineda, L., Foerster, J. — *K-level Reasoning for Zero-Shot Coordination in Hanabi*. NeurIPS 2021/2022. arXiv:2207.07166. https://arxiv.org/pdf/2207.07166 · https://proceedings.neurips.cc/paper/2021/hash/4547dff5fd7604f18c8ee32cf3da41d7-Abstract.html **PARTIALLY VERIFIED** (abstract/venue confirmed)
19. Lupu, A., Cui, B., Hu, H., Foerster, J. — *Trajectory Diversity for Zero-Shot Coordination*. ICML 2021, PMLR 139. https://proceedings.mlr.press/v139/lupu21a/lupu21a.pdf · extended abstract https://www.ifaamas.org/Proceedings/aamas2021/pdfs/p1593.pdf **PARTIALLY VERIFIED**
20. Anwar, U., Pandian, A., Wan, J., Krueger, D., Foerster, J. — *Noisy Zero-Shot Coordination: Breaking The Common Knowledge Assumption In Zero-Shot Coordination Games*. arXiv:2411.04976 (2024). https://arxiv.org/abs/2411.04976 **PARTIALLY VERIFIED** (abstract confirmed)
21. Jacob, A. P., Wu, D. J., Farina, G., Lerer, A., Hu, H., Bakhtin, A., Andreas, J., Brown, N. — *Modeling Strong and Human-Like Gameplay with KL-Regularized Search* (piKL). ICML 2022. arXiv:2112.07544. https://arxiv.org/pdf/2112.07544 · read via ar5iv **VERIFIED**
22. Hu, H., Wu, D. J., Lerer, A., Foerster, J., Brown, N. — *Human-AI Coordination via Human-Regularized Search and Learning*. arXiv:2210.05125 (2022). https://arxiv.org/abs/2210.05125 **PARTIALLY VERIFIED** (abstract confirmed)
23. *Ad-Hoc Human-AI Coordination Challenge (AH2AC2)*. arXiv:2506.21490 (2025). https://arxiv.org/html/2506.21490v2 **VERIFIED** (HTML read)
24. Cui, B., Hu, H., et al. — *Off-Team Learning*. OpenReview https://openreview.net/pdf?id=uOdTKkg2FtP **UNVERIFIED** (located only)
25. Bouzy, B. — *Playing Hanabi Near-Optimally*. ACG 2017, in *Advances in Computer Games*, Springer. https://link.springer.com/chapter/10.1007/978-3-319-67468-1_7 **PARTIALLY VERIFIED**
26. Wu, J. — *WTFWThat* Hanabi bot / hat-player documentation. https://github.com/chikinn/hanabi/blob/master/doc_hat_player.md ; O'Dwyer, A. — *Hat-game strategies in Hanabi*, https://quuxplusone.github.io/blog/2018/03/29/hat-guessing-in-hanabi/ **PARTIALLY VERIFIED** (secondary sources)

**Bridge bidding as a learned language**

27. Rong, J., Qin, T., An, B. — *Competitive Bridge Bidding with Deep Neural Networks*. AAMAS 2019. arXiv:1903.00900. https://arxiv.org/abs/1903.00900 · read via ar5iv · https://aamas.csc.liv.ac.uk/Proceedings/aamas2019/pdfs/p16.pdf **VERIFIED**
28. Lockhart, E., Burch, N., Bard, N., Borgeaud, S., Eccles, T., Smaira, L., Smith, R. — *Human-Agent Cooperation in Bridge Bidding*. arXiv:2011.14124 (2020, DeepMind). https://arxiv.org/pdf/2011.14124 · read via ar5iv **VERIFIED**
29. NukkAI / NooK — press coverage of the March 2022 Paris challenge: CBC *As It Happens* https://www.cbc.ca/radio/asithappens/as-it-happens-the-wednesday-edition-1.6402751/an-artificial-intelligence-just-beat-8-world-champions-at-bridge-1.6402861 ; SingularityHub https://singularityhub.com/2022/04/03/a-hybrid-ai-just-beat-eight-world-champions-at-bridge-and-explained-how-it-did-it/ **UNVERIFIED** (no primary technical paper found; note the challenge excluded bidding)

**Signalling-game and information-design theory**

30. Farrell, J., Gibbons, R. — *Cheap Talk with Two Audiences*. American Economic Review 79(5):1214–1223, Dec. 1989. http://web.mit.edu/rgibbons/www/Gibbons_Farrell_Cheap%20Talk%20with%20Two%20Audiences.pdf (PDF text extracted) **VERIFIED**
31. Crawford, V., Sobel, J. — *Strategic Information Transmission*. Econometrica 50(6):1431–1451, 1982. **PARTIALLY VERIFIED** (referenced within source 30 and secondary sources; original not read)
32. Sobel, J. — *Signaling Games* (encyclopedia entry). UCSD. https://econweb.ucsd.edu/~jsobel/Paris_Lectures/20070527_Signal_encyc_Sobel.pdf **UNVERIFIED** (PDF located, text extraction failed)
33. Spence, M. — job-market signalling; origin of the separating/pooling terminology (1973/1978). **UNVERIFIED** (secondary attribution only)
34. Gossner, O., Mertens, J.-F. — *The Value of Information in Zero-Sum Games* (2001). http://gossner.me/wp-content/uploads/2020/07/Value.pdf **PARTIALLY VERIFIED** (located; PDF fetch failed)
35. Bassan, B., Gossner, O., Scarsini, M., Zamir, S. — *Positive Value of Information in Games*. International Journal of Game Theory, 2003. **PARTIALLY VERIFIED** (cited in the sources I read)
36. Lehrer, E., Rosenberg, D. — *Evaluating Information in Zero-Sum Games with Incomplete Information on Both Sides*. Mathematics of Operations Research 35(4):851–863, 2010. https://www.cs.tau.ac.il/~lehrer/Papers/voif-both-sides-web.pdf **PARTIALLY VERIFIED**
37. Kamenica, E., Gentzkow, M. — *Bayesian Persuasion*. American Economic Review 101(6), 2011. https://web.stanford.edu/~gentzkow/research/BayesianPersuasion.pdf **PARTIALLY VERIFIED** (located; not read in full)
38. Wyner, A. D. — *The Wire-Tap Channel*. Bell System Technical Journal, 1975. Secrecy capacity $C_s=\max_{p(x)}[I(X;Y)-I(X;Z)]$. **PARTIALLY VERIFIED** (formula confirmed via multiple secondary technical sources listed below)
39. Csiszár, I., Körner, J. — *Broadcast Channels with Confidential Messages*. IEEE Trans. Inf. Theory, 1978. $C_s=\max_{p(v)p(x|v)}[I(V;Y)-I(V;Z)]$. Confirmed via https://arxiv.org/pdf/1410.3422 and https://arxiv.org/pdf/1106.4286 **PARTIALLY VERIFIED**

**Emergent communication: measures and biases**

40. Lowe, R., Foerster, J., Boureau, Y.-L., Pineau, J., Dauphin, Y. — *On the Pitfalls of Measuring Emergent Communication*. AAMAS 2019. arXiv:1903.05168. https://arxiv.org/abs/1903.05168 · read via ar5iv · code https://github.com/facebookresearch/measuring-emergent-comm **VERIFIED**
41. Eccles, T., Bachrach, Y., Lever, G., Lazaridou, A., Graepel, T. — *Biases for Emergent Communication in Multi-agent Reinforcement Learning*. NeurIPS 2019. arXiv:1912.05676. https://arxiv.org/abs/1912.05676 · read via ar5iv **VERIFIED**
42. Jaques, N., Lazaridou, A., Hughes, E., Gulcehre, C., Ortega, P., Strouse, D., Leibo, J. Z., de Freitas, N. — *Social Influence as Intrinsic Motivation for Multi-Agent Deep Reinforcement Learning*. ICML 2019. arXiv:1810.08647. https://arxiv.org/abs/1810.08647 · read via ar5iv · https://proceedings.mlr.press/v97/jaques19a.html **VERIFIED**
43. Köster, R., McKee, K. R., Everett, R., Weidinger, L., Isaac, W. S., Hughes, E., Duéñez-Guzmán, E. A., Graepel, T., Botvinick, M., Leibo, J. Z. — *Model-free conventions in multi-agent reinforcement learning with heterogeneous preferences*. arXiv:2010.09054 (2020). https://arxiv.org/abs/2010.09054 **PARTIALLY VERIFIED** (abstract confirmed)
44. Abadi, M., Andersen, D. G. — *Learning to Protect Communications with Adversarial Neural Cryptography*. arXiv:1610.06918 (2016). https://arxiv.org/abs/1610.06918 **PARTIALLY VERIFIED** (abstract confirmed; the Alice/Bob/Eve setup is the ML archetype for the wiretap construction)

**Other**

45. Bakhtin, A. et al. (Meta FAIR Diplomacy Team) — *Human-level play in the game of Diplomacy by combining language models with strategic reasoning* (CICERO). Science 378, 2022. https://noambrown.github.io/papers/22-Science-Diplomacy-TR.pdf · https://ai.meta.com/research/cicero/ **PARTIALLY VERIFIED**
46. Lanctot, M., Lockhart, E. et al. — *OpenSpiel: A Framework for Reinforcement Learning in Games*. arXiv:1908.09453. https://arxiv.org/pdf/1908.09453 · https://github.com/google-deepmind/open_spiel — contains `tiny_bridge` and `trade_comm`, the two standard *miniature signalling* benchmarks. Recommended as unit tests for the Fish belief/convention machinery before scaling. **PARTIALLY VERIFIED**
47. Perolat, J. et al. — *Mastering the Game of Stratego with Model-Free Multiagent Reinforcement Learning* (DeepNash, R-NaD). arXiv:2206.15378. https://arxiv.org/pdf/2206.15378 **UNVERIFIED** (located only; relevant as a large imperfect-information game solved without explicit belief modelling)
48. Wikipedia — *Literature (card game)*. https://en.wikipedia.org/wiki/Literature_(card_game) — confirms the "Fish / Canadian Fish / Russian Fish" naming. No published AI work on the game was found; the only artefact located was a hobby repo, https://github.com/iuruoy-shao/fish. **VERIFIED (as a negative result: no prior academic art on Fish)**

---

### Appendix: numbers computed for this report (reproducible)

| Quantity | Value |
|---|---|
| Number of deals $54!/(9!)^6$ | $1.011\times10^{38}$ (126.25 bits) |
| Deals consistent with one player's own hand, $45!/(9!)^5$ | $1.901\times10^{28}$ (93.94 bits) |
| Uniform per-unknown-card location entropy | $\log_2 5=2.322$ bits ($\times45=104.5$ bits, an upper bound ignoring hand-size constraints) |
| Distinct half-suits in a random 9-card hand | mean 6.16 (min 3, max 9) |
| Askable cards (legal card targets) per hand | mean 27.98 |
| Legal asks at deal time (×3 opponents) | mean 83.95, median 81, min 27, max 135 |
| Raw action-channel capacity per ask | $\log_2 84\approx6.39$ bits |
| Answer channel | $\le 1$ bit |

Computed by Monte Carlo over $2\times10^5$ random 9-card hands from the 54-card deck with the 9 half-suits as specified.
