#!/bin/zsh
# K3 measurement driver.  All artifacts land in research/v07/results/K3-*.
set -u
E="/Users/dylan/Documents/GitHub/fish optimization/.claude/worktrees/wf_88eccd0d-4c3-4/engine"
R="/Users/dylan/Documents/GitHub/fish optimization/research/v07/results"
F="$E/fish7"
cd "$E"
"$@"
