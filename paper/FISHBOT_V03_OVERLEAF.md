# FishBot v0.3 paper - Overleaf-ready LaTeX

Copy the complete fenced block into `main.tex` in a blank Overleaf project. It is self-contained and uses only standard TeX Live packages.

```latex
\documentclass[11pt]{article}

\usepackage[margin=0.82in]{geometry}
\usepackage[T1]{fontenc}
\usepackage{lmodern}
\usepackage{microtype}
\usepackage{amsmath,amssymb}
\usepackage{booktabs}
\usepackage{array}
\usepackage{longtable}
\usepackage[dvipsnames]{xcolor}
\usepackage{hyperref}
\usepackage{enumitem}
\usepackage{fancyhdr}

\definecolor{fishgreen}{HTML}{0B5D46}
\definecolor{fishlight}{HTML}{EAF4EF}
\definecolor{fishgray}{HTML}{4B5954}
\hypersetup{colorlinks=true, linkcolor=fishgreen, citecolor=fishgreen, urlcolor=fishgreen}
\setlist{nosep,leftmargin=*}
\setlength{\parindent}{0pt}
\setlength{\parskip}{5pt}
\pagestyle{fancy}
\fancyhf{}
\lhead{FishBot v0.3}
\rhead{Reproducible policy optimization}
\cfoot{\thepage}

\title{\textbf{FishBot v0.3: A Count-Conditioned, Empirically Tuned Policy for Six-Player Canadian Fish}\\[4pt]
\large Mechanisms, Evaluation, and a Human Playbook}
\author{FishLab Research Project}
\date{August 20, 2026}

\begin{document}
\maketitle

\begin{abstract}
We develop FishBot v0.3 for the six-player, 54-card Canadian Fish variant with nine six-card half-suits and exact teammate-allocation declarations. The policy uses only its private hand and public information. It maintains card-location beliefs, conditions those beliefs on public hand counts, and evaluates every legal card-target ask with a transparent utility model. Development used 126,600 staged simulation games across optimization, validation, adversarial evaluation, mechanism ablation, and a full policy matrix. On an untouched test bank with team orientation swapped, v0.3 won 57.85\% against a literature-derived turn-starvation policy, 57.20\% against the previously strongest posterior-greedy detective, and 56.50\% against the sanitized v0.2 predecessor; all three 95\% Wilson intervals excluded 50\%. Paired ablations identify ask-history inference and count conditioning as the decisive mechanisms. Removing ask history cost 38.75 percentage points (95\% CI 36.01--41.49), while removing count conditioning cost 5.20 points (2.18--8.22). Smaller utility terms were not individually distinguishable from zero at the present sample size. We translate the validated mechanisms into a practical human strategy centered on exact ask memory, card-count reconciliation, high-conversion questions, cautious declarations, and evidence-based turn denial. These results establish the strongest policy in the tested simulator population, not a proof that Canadian Fish is solved.
\end{abstract}

\section{Research question and claims}

Canadian Fish is a partially observable, cooperative, adversarial game. A move transfers a card when correct, transfers the turn when wrong, reveals information to both teams, and can change the safety of a later declaration. The research question is therefore not merely ``Which ask is most likely to hit?'' It is:

\begin{quote}
Which inspectable policy maximizes robust win rate across diverse opponents when it is restricted to information available to a real player, and which parts of that policy can a human realistically adopt?
\end{quote}

This paper supports three bounded claims:

\begin{enumerate}
  \item FishBot v0.3 is the strongest policy in the evaluated simulator population.
  \item Its advantage is robust to team orientation and survives an untouched seed bank.
  \item Ask-history inference and public card-count conditioning are causally important within this simulator.
\end{enumerate}

It does \emph{not} claim a Nash equilibrium, low exploitability against every possible policy, or unbeatable human play. ``Stockfish for Fish'' remains a design target rather than a solved status.

\section{Rules and experimental scope}

The engine models six alternating seats, with seats 0, 2, and 4 on Team A and seats 1, 3, and 5 on Team B. It uses a 54-card deck: the ordinary eight low/high half-suits plus a ninth half-suit containing the four eights and two jokers. Each half-suit contains six cards, and each player begins with nine cards.

On a turn, a player asks one opponent for one specific card. The asker must hold another card in the same half-suit and cannot already hold the requested card. A hit transfers the card and retains the turn; a miss gives the turn to the target. A declaration assigns all six cards of a half-suit to exact teammates. A correct declaration scores the set; any wrong card or teammate assignment awards it to the opposing team. All asks, answers, transfers, declarations, and hand counts are public. Hidden hands are never exposed to the acting policy.

The 48-card, eight-book Literature rules are more common in published descriptions \cite{pagat,develin}. The 54-card ninth-book form is a documented variant \cite{pagat} and is the form already implemented by FishLab v0.2. Conclusions in this paper apply directly to that form. Strategy mechanisms such as memory, count reconciliation, and turn denial are likely transferable, but the reported win rates are not automatically transferable across rule dialects.

\section{External strategy review and prior computational work}

We reviewed the strategy synthesis supplied by the project owner \cite{strategy-site}, the rules and tactics collected by McLeod \cite{pagat}, Develin's Canadian Fish chapter \cite{develin}, Dorsa's practical notes \cite{dorsa}, and Somani's open-source Literature learner \cite{somani}. Four recurring ideas were relevant enough to operationalize or test:

\begin{description}[style=nextline]
  \item[Perfect or triaged memory.] Public questions are the main information channel. Published human advice often recommends exact memory for a few live half-suits rather than vague memory for all of them.
  \item[Ask the asker and continue a teammate's miss.] An ask proves that the asker owns another card in that half-suit and lacks the named card. A miss also excludes the target. These deductions should alter later card-location probabilities.
  \item[Lock out dangerous opponents.] Because a miss transfers control, a player who is close to completing a known half-suit may be worth avoiding even when that player is a plausible card target.
  \item[Claim conservatively.] Exact team ownership is insufficient: the declarer must know the allocation among teammates. Published advice consistently treats an uncertain declaration as a high-cost error.
\end{description}

Somani's implementation is valuable prior art but not a suitable six-player benchmark. Its published learner trains four-player games, uses an MLP over serialized knowledge and moves, rewards hits directly, makes only certain declarations, and suppresses known misses except as a fallback \cite{somani}. FishBot instead evaluates six-player games, penalizes wrong declarations, includes a fixed opponent population, and separates train, validation, and held-out test seeds.

Prearranged signaling conventions are a rules-dependent issue. Some Literature traditions document them, while US student Fish traditions forbid them \cite{pagat,develin,strategy-site}. FishBot learns from ordinary public actions but has no secret convention channel.

\section{Simulator audit and information safety}

The engine is deterministic under a seed and configuration. Every player owns a separate memory containing known card owners, player-card exclusions, and public player/half-suit ask counts. The verification suite checks deck integrity, legality, exact nine-set resolution, deterministic replay, action traces, and the absence of action-limit adjudication in normal verification games.

The v0.2 audit found an important information leak. Its reply-risk feature estimated a target's next legal actions by calling the legal-action generator with that target as actor. The legal-action generator correctly used the target's private hand to determine which half-suits the target could ask from, but the current actor was not entitled to that information. FishBot v0.3 replaces this with a public-history threat estimate. The in-simulator v0.2 comparison policy is also sanitized at this boundary; its original utility is retained, but its reply estimate no longer peeks at a hidden hand.

This correction matters methodologically even though the new bot beats the older one. A high win rate obtained by hidden-state access would be unusable as a human strategy and invalid as evidence of strong partially observable play.

\section{FishBot v0.3}

\subsection{Information state}

For observer $i$, card $c$, and possible owner $p$, FishBot maintains either an exact owner, an exclusion, or an unresolved weight. Public asks provide soft evidence:

\begin{equation}
w_{c,p} = \exp\left(\min\left(2.4,\;0.453\,s_{p,h(c)}\right)\right),
\end{equation}

where $s_{p,h(c)}$ is the number of public asks player $p$ has made in card $c$'s half-suit. Exact transfers become one-hot assignments; a miss excludes both the asker, who cannot hold the requested card, and the target.

Independent normalization is insufficient because public hand counts couple card locations. For each unresolved card, row sums must equal one. For each player, the sum of unresolved ownership mass must equal

\begin{equation}
q_p = |H_p| - k_p,
\end{equation}

where $|H_p|$ is the public current hand count and $k_p$ is the number of active cards already known to be owned by $p$. FishBot performs 12 alternating row and column scaling iterations. This is a maximum-entropy-style approximation to exact deal enumeration: it enforces the strongest public cardinality constraints without traversing every consistent deal.

\subsection{Legal ask search}

FishBot enumerates every legal pair $a=(c,t)$ of requested card and opposing target. The selected configuration scores it as

\begin{align}
U(a) ={}& 22.0\,P(\mathrm{hit})
 + 2.5\,\mathrm{progress}
 + 4.0\,\mathrm{teamControl} \\
&+ 0.5\,\mathrm{targetEvidence}
 + 4.0\,P(\mathrm{hit})\,\mathrm{continuation} \\
&+ 4.0\,P(\mathrm{hit})\,\mathrm{completion}
 + 0.5\,\mathrm{repeat} \\
&- 1.0\,P(\mathrm{miss})\,\mathrm{replyThreat}.
\end{align}

The information-entropy coefficient is zero in the selected model. This is intentional: v0.2 overpaid for uncertainty resolution, while held-out wins favored conversion and set control. Entropy remains a reported metric rather than a direct reward.

The features are interpretable:

\begin{itemize}
  \item $P(\mathrm{hit})$ is the count-conditioned posterior that target $t$ owns card $c$.
  \item \emph{Progress} is the actor's fraction of the six-card half-suit.
  \item \emph{Team control} is the expected fraction held across the actor's team.
  \item \emph{Target evidence} is capped public ask activity by the target in that half-suit.
  \item \emph{Continuation} is the best posterior target for another missing card after a hit.
  \item \emph{Completion} is 1 after four actor-held cards, 0.35 after three, and 0 otherwise.
  \item \emph{Reply threat} combines the target's expected concentration, ask activity, team control, and the friendly cards exposed if the turn is transferred.
\end{itemize}

Tie-breaking is deterministic. Decision traces retain the selected features, utility, and top alternatives for replay.

\subsection{Declarations}

For each unresolved half-suit, the bot computes the geometric mean probability that all cards are on its team and the geometric mean probability of the most likely exact teammate allocation. The combined confidence is the twelfth root of their product. The base declaration threshold is 0.963; the bot lowers it by 0.016 when trailing by at least two sets and raises it by 0.005 when leading by at least two. Exact-allocation confidence receives only 0.008 slack. An endgame escape threshold prevents infinite questioning when information is exhausted.

This is cautious but not infallible. The held-out declaration accuracy of v0.3 was approximately 95--96\% against the strongest challengers. Approximate beliefs are sometimes overconfident, and a declaration can fail through teammate misallocation even when team ownership is correct.

\subsection{Pseudocode}

\begin{verbatim}
observe(public event)
update exact owners, exclusions, hand counts, and ask signals

on turn:
    condition card beliefs on public hand capacities
    if a declaration clears the score-aware threshold:
        declare the highest-confidence half-suit
    else:
        legal = every card-target pair allowed by my actual hand
        score each pair with the fixed v0.3 utility
        ask the deterministic maximum
\end{verbatim}

\section{Experimental design}

\subsection{Opponent population}

The final population contains eight policies:

\begin{itemize}
  \item FishBot v0.3;
  \item sanitized FishBot v0.2;
  \item a literature-derived lockout specialist;
  \item a posterior-greedy detective;
  \item a focused half-suit hunter;
  \item an adaptive diversifier;
  \item a deliberate misdirection policy; and
  \item a uniform random legal control.
\end{itemize}

The lockout specialist was added after the external review. It begins with the detective's high-posterior ask rule and subtracts a public-history penalty for missing into a dangerous target. This guards against selecting a policy that only beats the original in-house archetypes.

\subsection{Seed separation and orientation}

Every matchup uses deterministic derived seeds. Team A and Team B orientations are both evaluated, and policy win rate is the average number of wins across the two orientations. Training, validation, final test, matrix, and paired-ablation banks use disjoint base seeds. The selected configuration was frozen before the final test. A later local refinement sweep did not produce a material improvement, so the frozen configuration remained unchanged.

\begin{table}[ht]
\centering
\caption{Current-study simulation budget.}
\begin{tabular}{lrr}
\toprule
Stage & Purpose & Games \\
\midrule
Initial coarse search and validation & Learn v0.3 weight region & 24,000 \\
Lockout-aware search and validation & Add external challenger & 24,600 \\
Held-out tests, ablations, and matrix & Estimate generalization & 54,000 \\
Post-test local stability sweep & Check nearby alternatives; no change & 24,000 \\
\midrule
Total & & 126,600 \\
\bottomrule
\end{tabular}
\end{table}

The final head-to-head test uses 1,000 games per team orientation for each opponent. We report a two-sided 95\% Wilson interval for the pooled 2,000-game win rate. The full ordered matrix uses 250 games per cell and is descriptive. Mechanism ablations use matched deals against the detective and lockout policies; paired confidence intervals use the observed per-deal win difference.

\subsection{Metrics}

Win rate is primary, but a mechanistic explanation requires supporting outcomes:

\begin{description}[style=nextline]
  \item[Mean score.] Average half-suits won out of nine.
  \item[Ask accuracy.] Fraction of asks that transfer the requested card.
  \item[Declaration accuracy.] Fraction of exact declarations scored correctly.
  \item[Information per ask.] Binary entropy of the modeled hit/miss outcome.
  \item[Reply threat on a miss.] Mean public-history danger assigned to the target when the ask fails.
  \item[Reaction accuracy.] Hit rate on the first ask after an opponent misses into the actor.
  \item[Decision regret.] Difference between the chosen ask and the v0.3 utility maximum in the same information state. It explains policy disagreement but is not an independent ground-truth value function.
\end{description}

\section{Results}

\subsection{Held-out head-to-head performance}

\begin{table}[ht]
\centering
\caption{FishBot v0.3 on the untouched test bank, pooled over both team orientations. Each row contains 2,000 games.}
\small
\begin{tabular}{lrrrrr}
\toprule
Opponent & Win rate & 95\% CI & Mean score & Ask acc. & Decl. acc. \\
\midrule
Lockout specialist & 57.85\% & 55.67--60.00 & 4.801 & 52.21\% & 96.11\% \\
Posterior detective & 57.20\% & 55.02--59.35 & 4.767 & 53.60\% & 95.01\% \\
FishBot v0.2 & 56.50\% & 54.32--58.66 & 4.770 & 52.29\% & 96.23\% \\
Adaptive diversifier & 86.15\% & 84.57--87.59 & 5.978 & 63.70\% & 93.95\% \\
Focused hunter & 95.15\% & 94.12--96.01 & 6.747 & 55.30\% & 91.56\% \\
Misdirection artist & 99.20\% & 98.70--99.51 & 7.438 & 57.33\% & 90.24\% \\
Random legal control & 100.00\% & 99.81--100.00 & 8.400 & 52.38\% & 93.91\% \\
\bottomrule
\end{tabular}
\end{table}

The strongest result is not the near-perfect performance against weak policies. It is the 6.5--7.85 point margin against the three credible challengers. Each confidence interval excludes a tie after orientation pooling.

Ask accuracy alone does not explain the result. Against the detective, v0.3's held-out ask accuracy was 53.60\%; against lockout it was 52.21\%. The policy converts enough high-quality asks while combining those asks with more coherent card-count beliefs and conservative exact declarations. Likewise, high information entropy is not inherently valuable: the selected model places no direct reward on it.

In the descriptive 8-by-8 ordered matrix, v0.3 had a 74.35\% row average. The next policies were lockout at 70.40\%, detective at 68.80\%, and v0.2 at 68.00\%. These averages include easy opponents and are not treated as the primary estimate.

\subsection{Mechanism ablations}

\begin{table}[ht]
\centering
\caption{Paired mechanism ablations against detective and lockout. Positive values mean the full v0.3 policy won more often than the ablated policy on the same deals.}
\small
\begin{tabular}{lrr}
\toprule
Removed or changed mechanism & Full minus ablated & 95\% paired CI \\
\midrule
Ask-history signal & 38.75 points & 36.01--41.49 \\
Public count conditioning & 5.20 points & 2.18--8.22 \\
All terms except immediate transfer & 3.20 points & 0.25--6.15 \\
Completion bonus & 1.15 points & $-0.52$--2.82 \\
Continuation value & 0.35 points & $-1.17$--1.87 \\
Reply-risk penalty & 0.30 points & $-1.06$--1.66 \\
Team-control term & $-0.80$ points & $-3.29$--1.69 \\
Restore a 4.5 entropy premium & $-0.50$ points & $-2.70$--1.70 \\
\bottomrule
\end{tabular}
\end{table}

Two mechanisms are individually supported: remembering public ask history and reconciling ownership probabilities with public hand counts. The immediate-transfer-only policy is also significantly worse, which shows that the auxiliary terms are useful collectively. The experiment does not identify continuation, completion, team control, entropy, or reply-risk weights as individually necessary. Their confidence intervals include zero, likely because the features are correlated and can substitute for one another.

This distinction is important for the human playbook. We should strongly recommend ask memory and count reconciliation. We should describe chaining and lockout as sensible secondary tools, but not claim that their isolated coefficients are proven at the present sample size.

\subsection{Psychological response rules}

The ``reactive heuristics'' toggle changes deterministic same-half-suit or diversion bonuses for baseline opponents. With it enabled, v0.3's win rate changed by $-0.8$, $-0.5$, and $-0.5$ points against detective, lockout, and bluffer, respectively, relative to matched disabled runs. These small differences were not used to select the final model. FishBot v0.3 itself does not apply a hardwired emotional or same-suit response. It treats every public ask as evidence and chooses the current utility maximum.

\section{How a human can play like FishBot}

The bot can maintain 54-by-6 belief tables; a human cannot. The goal is therefore to preserve the high-value logic in a compact routine.

\subsection{The practical priority order}

\begin{enumerate}
  \item \textbf{Remember every ask in live half-suits.} Record mentally: asker, target, named card, and hit or miss. An ask proves the asker has another card in that half-suit and lacks the named card. A miss also proves the target lacks it. A hit fixes the new owner exactly. This was the dominant ablation result.
  \item \textbf{Reconcile with public card counts.} When a player has few unexplained slots left, redistribute probability away from them. If all of a player's remaining cards are already located, they cannot own any unresolved card. Count constraints turned a merely plausible belief model into a significantly stronger one.
  \item \textbf{Ask for conversion first.} Prefer a card-player pair supported by a hit, a miss-based exclusion chain, repeated half-suit activity, or forced remaining capacity. Do not choose a question merely because its answer would be interesting.
  \item \textbf{Use a four-part tie-break.} When two asks seem similarly likely, favor the one that (a) advances a half-suit in which your team has control, (b) leaves another high-probability follow-up if it hits, (c) approaches four or five cards in one hand, and (d) avoids donating the turn to an obviously dangerous player.
  \item \textbf{Declare from a six-card ledger, not a feeling.} Before declaring, name all six cards and an exact teammate for each. Separate ``our team has it'' from ``I know which teammate has it.'' If either statement is uncertain, continue asking unless the endgame forces risk.
  \item \textbf{Let score change risk only slightly.} When far behind, accept a little more declaration risk; when comfortably ahead, demand more certainty. Do not let score pressure override a missing card location.
  \item \textbf{Drop unsupported theater.} The simulator found no reliable benefit from deterministic diversion or psychological response rules. Bluff only when it has a concrete information or turn-control purpose, not to appear unpredictable.
\end{enumerate}

\subsection{A table-side mental ledger}

For each half-suit you hold or that is strategically dangerous, imagine six slots. Use four labels:

\begin{center}
\fbox{\parbox{0.91\linewidth}{
\textbf{MINE} -- in your hand. \quad
\textbf{KNOWN} -- publicly transferred to a player. \quad
\textbf{NOT X} -- excluded from player X by a miss or ask legality. \quad
\textbf{LIKELY X} -- X has repeatedly asked in that half-suit and still has unexplained card capacity.
}}
\end{center}

After every action, update only the affected half-suit and the two players' remaining counts. When a half-suit is declared, erase it from memory. This follows the human literature's recommendation to keep a few ledgers exact while preserving the bot's most valuable evidence.

\subsection{A human ask score}

Exact arithmetic is unnecessary. Assign each candidate ask a qualitative score:

\begin{center}
\begin{tabular}{>{\raggedright\arraybackslash}p{0.38\linewidth}>{\raggedright\arraybackslash}p{0.48\linewidth}}
\toprule
Question & Human rule \\
\midrule
Do I have direct location evidence? & Strongest positive factor. A prior hit or forced exclusion beats a vague tell. \\
Does the target have capacity? & Downgrade targets whose unexplained card slots are already consumed. \\
Does this build a controlled half-suit? & Prefer team-controlled and near-complete sets when hit odds are comparable. \\
Is there a follow-up after a hit? & Favor chains only after establishing the first hit probability. \\
How dangerous is a miss into this player? & Avoid a visibly loaded, well-informed opponent when a comparable target exists. \\
Am I choosing uncertainty for its own sake? & If yes, stop. Information without conversion was overpriced by v0.2. \\
\bottomrule
\end{tabular}
\end{center}

\subsection{Worked deduction pattern}

Suppose teammate A asks opponent D for the low-club 7 and misses. Publicly, A has another low club, A lacks the 7, and D lacks the 7. If you also lack the 7, only three seats remain possible. If later transfers and card counts exhaust two of those seats, the 7 is forced to the last player. The optimal human action is not to remember merely ``someone wanted low clubs.'' It is to preserve the exact exclusions until counts finish the deduction.

Now suppose D has publicly accumulated five low clubs and everyone can locate the seventh card on your team. D is dangerous even if D is a likely target for an unrelated card. If you miss into D, D can immediately take the last low club. Prefer another opponent when the hit odds are close. This is the blackball principle. The ablation does not prove a precise lockout coefficient, but v0.3's win over the explicit lockout challenger shows that the policy handles this opponent style without surrendering its broader inference discipline.

\subsection{What ``unbeatable'' should mean in practice}

No fixed checklist makes a player literally unbeatable. Deals are random, teammates may forget, rule dialects vary, and a human opponent can adapt. A defensible target is \emph{error-resistant expert play}: remember all strategically live asks, make count-consistent deductions, donate fewer turns, and avoid uncertain declarations. Those practices are directly supported by the experiments and should produce a much larger gain than stylistic bluffing.

\section{Limitations and threats to validity}

\begin{itemize}
  \item \textbf{Population dependence.} A policy can be best against the tested opponents without being minimally exploitable against all policies.
  \item \textbf{Approximate beliefs.} Alternating scaling matches marginal hand capacities but does not enumerate all legal deals or model every higher-order dependency.
  \item \textbf{No learned conventions.} Teammates share a policy but do not develop population-specific signaling protocols.
  \item \textbf{Rule dialect.} The experiment uses nine books and opponent-awarded wrong declarations. Results may change under 48-card, voided-declaration, out-of-turn-claim, or challenge rules.
  \item \textbf{Hand-built opponent set.} The lockout challenger improves diversity, but expert human logs and independently authored bots would be stronger external tests.
  \item \textbf{Ablation interactions.} A feature can look individually unnecessary when a correlated feature substitutes for it. The significant transfer-only gap shows collective value without uniquely allocating credit.
  \item \textbf{No formal exploitability bound.} Canadian Fish has imperfect information, teams, and signaling. A Stockfish-like claim requires equilibrium-oriented training and evaluation, not only head-to-head wins.
\end{itemize}

\section{Next steps toward a Stockfish-like engine}

The next revision should preserve v0.3 as a fixed benchmark and add:

\begin{enumerate}
  \item exact or sampled posterior conditioning over complete legal deals;
  \item outcome-sampling Monte Carlo CFR over information sets;
  \item population-based self-play so several conventions and counter-conventions survive;
  \item value estimates for declaration timing and intentional known misses;
  \item strategic selection of a teammate after claiming out;
  \item expert human game logs and independently implemented external bots; and
  \item exploitability proxies rather than only fixed-population win rate.
\end{enumerate}

Intentional known misses and delayed certain declarations are especially important open questions from the external literature. The current engine permits known-miss asks but v0.3 almost never values them; it also tends to cash sufficiently confident declarations immediately. Both mechanisms require teammate-aware counterfactual values rather than another local coefficient.

\section{Reproducibility}

The implementation, optimization scripts, JSON result artifacts, and this paper are stored in the FishLab repository. The principal commands are:

\begin{verbatim}
npm run verify:engine
npm run optimize:fishbot
npm run evaluate:fishbot
npm run refine:fishbot
npm run ablations:fishbot
npm test
\end{verbatim}

The simulator engine is \path{lib/fish-engine.ts}. The final coefficients are exported as \path{DEFAULT_FISHBOT_CONFIG}. Evaluation artifacts include base seeds, sample sizes, both orientations, per-opponent results, matrices, and paired-ablation intervals. The browser simulator exposes v0.3, the lockout challenger, and the sanitized v0.2 baseline for direct comparison and replay.

\section{Conclusion}

FishBot v0.3 advances the project from a plausible one-ply formula to a held-out, adversarially tested policy. Its strongest lesson is simple and unusually stable: remember the asks, then make the card counts agree. Those two mechanisms dominate the ablations and translate directly to human play. High-probability acquisition, cautious exact declarations, and evidence-based turn denial complete the practical strategy. The resulting bot is decisively stronger than the previous best policies in this simulator, while the remaining limitations define a credible path toward equilibrium-oriented Fish research.

\begin{thebibliography}{9}

\bibitem{strategy-site}
Canadian Fish Project, ``Strategy notes,'' 2026.\\
\url{https://canadian-fish.vercel.app/strategy}

\bibitem{pagat}
J. McLeod, ``Literature,'' Pagat, updated July 1, 2026.\\
\url{https://www.pagat.com/quartet/literature.html}

\bibitem{develin}
M. Develin, ``Canadian Fish,'' in \emph{The Ten Best Card Games You've Never Heard Of}, Chapter 9.\\
\url{https://www.bantha.org/~develin/cardgames.html}

\bibitem{dorsa}
D. Dorsa, ``Literature Game Strategy,'' Deposit Genius, June 25, 2018.\\
\url{https://depositgenius.com/literature-strategy-canadian-fish/}

\bibitem{somani}
N. Somani, ``Literature: card game implementation and learning bot,'' GitHub repository.\\
\url{https://github.com/neelsomani/literature}

\end{thebibliography}

\end{document}
```
