#!/usr/bin/env bash
# FishBot v0.7 -- phase 2, battery A: the UNFITTED single-knob adversary screen.
#
# Why this exists, and why it runs before any CEM fit.
#
# Every exploiter search the corpus has ever run is one search: a cross-entropy
# fit of a linear vector, maximising win rate, from one seed.  "Fifteen runs of
# one search are one run."  A coordinate sweep is a different search with a
# different bias -- it explores the axis-aligned neighbourhood of the incumbent
# exhaustively and at zero fitting cost, and it cannot be trapped by the CEM's
# covariance collapse.  It is also the only search that can find an exploiter
# which is a SIMPLIFICATION of the target rather than a refinement of it, and
# two of the corpus's own results (rtie=1 at -0.69, xf=0 at -0.46, both
# "variant ahead, unseparated") are exactly that shape.
#
# Every arm here is an ADVERSARY seated as three copies against the frozen
# target.  A1 screens on one bank at 12,000 games (98/sqrt(N) = +/-0.89 pts);
# survivors are re-measured on two banks at high power by battery A2.
set -uo pipefail
cd "$(dirname "$0")"
OUT=${OUT:-../research/v07/results}
BIN=${BIN:-./fish7}
THREADS=${THREADS:-14}
DEALS=${DEALS:-6000}
BANK=${BANK:-7030001}
TARGET=${TARGET:-v06}
TAG=${TAG:-Ffast}
ART=${ART:-$OUT/P1-screen.jsonl}
mkdir -p "$OUT"

$BIN seeds --require=$BANK >/dev/null || { echo "seed registry check FAILED"; exit 3; }

run () {  # run <cluster> <spec>
  local CL="$1"; shift
  local SPEC="$1"; shift
  $BIN match --a="$SPEC" --b="$TARGET" --games=$DEALS --rotations=2 --seed=$BANK \
       --threads=$THREADS --json 2>/dev/null \
    | sed "s|^{|{\"battery\":\"P1screen\",\"cluster\":\"$CL\",\"targetTag\":\"$TAG\",\"bank\":$BANK,|" \
    >> "$ART"
  echo "  done $CL  $SPEC"
}

echo "== P1 screen vs $TARGET ($TAG), bank $BANK, $DEALS deals x 2 =="

# ---- reference rungs ------------------------------------------------------
run reference   "v06"
run reference   "v05"
run reference   "v04"

# ---- the belief and its policy prior (ledger L4, C2; the feint's channel) --
run prior       "v06:ptheta=0"
run prior       "v06:ptheta=0.2"
run prior       "v06:ptheta=0.9"
run prior       "v06:ptheta=1.5"
run prior       "v06:pphi=0"
run prior       "v06:pphi=0.4"
run prior       "v06:ptheta=0,pphi=0"
run belief      "v06:belief=indep"
run belief      "v06:belief=sinkhorn"
run belief      "v06:belief=exactdisj"
run belief      "v06:belief=hybrid"
run belief      "v06:belief=block"

# ---- declaration timing and allocation (ledger L1) ------------------------
run decl        "v06:vmargin=-0.02"
run decl        "v06:vmargin=-0.01"
run decl        "v06:vmargin=0.01"
run decl        "v06:vmargin=0.03"
run decl        "v06:decl=0.90"
run decl        "v06:decl=0.99"
run decl        "v06:minteam=0.20"
run decl        "v06:minteam=0.70"
run decl        "v06:m2=0"
run decl        "v06:gmap=1"
run decl        "v06:declare=0"
run decl        "v06:vdecl=0"

# ---- ask selection, the live-ask gate M1, turn routing (L3, L10, L14) -----
run ask         "v06:m1=0"
run ask         "v06:m1p=0"
run ask         "v06:dead=1"
run ask         "v06:dead=1,deadbudget=3"
run ask         "v06:dead=1,deadbudget=1"
run ask         "v06:norepeat=1"
run ask         "v06:rtie=1"
run ask         "v06:topk=0"
run ask         "v06:chain2=1"
run ask         "v06:xf=0"
run ask         "v06:value=0"
run ask         "v06:askfloor=0.3"
run ask         "v06:oppfloor=0"

# ---- the scripted panel: what already beats the target, and by how much ---
run archetype   "detective"
run archetype   "lockout"
run archetype   "hunter"
run archetype   "diversifier"
run archetype   "bluffer"
run archetype   "v03"
run archetype   "v02"
run archetype   "silent"
run archetype   "feint"
run archetype   "withholder"

# ---- the frontier's own search, seated as the adversary -------------------
run search      "v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26"
run search      "v06:s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=24,maxq=26"
run search      "v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26"

# ---- the white-box class, unfitted (phase 1's strongest single arm) -------
run invert      "v07i:idet=48,imodel=v06"

echo "done -> $ART"
