# FishBot v0.7 phase 4 -- the specs, in one place, sourced by every driver.
# Training banks only.  7030001 / 7030002 evaluate, 7030003 transfers,
# 7030004 fits.  709xxxx is sealed and runMatch refuses it.

BANK1=7030001
BANK2=7030002
BANKS=("$BANK1" "$BANK2")
FITBANK=7030004
XFERBANK=7030003

# The frontier, as phase 2 and phase 3 measured it.
V06='v06'
SEARCH='s1=1,det=12,cand=4,kappa=2.5,rbelief=indep,depth=12,maxq=26'
FCHEAP="v06:${SEARCH}"
FMID='v06:s1=1,det=16,cand=6,kappa=2.0,maxq=26'

# The phase-2 composite: the bar.  ADVERSARIES.md fact 1.
P2COMP="v07:m2=0,r12=25,${SEARCH}"

# Phase 3's survivor, and its parts.
URGOFF='pool=-1,oppfloor=-1,force=1000000,askfloor=-1'   # the urgency escalation, off
STALL='stall=12'                                          # the termination rule that replaces the cliff
RTIE='rtie=1'                                             # the public-hash tie-break
K3KEYS="${RTIE},${URGOFF},${STALL}"

K3STACK="v06:${K3KEYS}"
K3SEARCH="v06:${K3KEYS},${SEARCH}"

# THE FROZEN v0.7 CONFIGURATION.  Phase 4's leading candidate going in was
# `v07:m2=0,r12=25,${K3KEYS},${SEARCH}` -- K3's keys on top of the phase-2
# composite.  `m2=0` is dropped from the freeze because its leave-one-out drop is
# +0.00 with win rates identical to six decimals on both banks at 24,000 games:
# ADVERSARIES section 4C's "M2 is the same defect and is bit-identical inert once
# urgency is off", confirmed at thirty times the sample.  A key that provably does
# nothing is surface without benefit.  See engine/fishbot_v07.json.
V07CAND="v07:r12=25,${K3KEYS},${SEARCH}"
# The pre-freeze candidate, kept because the replication cell and the attribution
# lattice were run on it and the cross-play architecture was fitted from it.
V07CAND_M2="v07:m2=0,r12=25,${K3KEYS},${SEARCH}"

# The configuration the commit gate must reject -- a negative control, not a
# proposal.  ADVERSARIES.md section 4H: +2.68 over v06 and unsound.
GATEFAIL="v06:rtie=1,m1=0,${URGOFF}"
