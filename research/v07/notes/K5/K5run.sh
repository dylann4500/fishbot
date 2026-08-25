#!/bin/sh
# K5 driver.  Any argument containing the token @SPEC / @SPECTIE has it replaced
# by the fitted learned spec, which is far too long to type on a command line.
ENG="/Users/dylan/Documents/GitHub/fish optimization/.claude/worktrees/wf_88eccd0d-4c3-6/engine"
SCR="/private/tmp/claude-501/-Users-dylan-Documents-GitHub-fish-optimization/f63a50cf-0555-4d3c-99d1-238332bf1a3d/scratchpad"
SPEC=`cat "$SCR/K5-spec.txt"`
SPECTIE=`cat "$SCR/K5-spec-tie.txt"`
cd "$ENG" || exit 1
set -f
n=0
for a in "$@"; do
  case "$a" in
    *@SPECTIE*) a=`printf '%s' "$a" | sed "s|@SPECTIE|$SPECTIE|"` ;;
    *@SPEC*)    a=`printf '%s' "$a" | sed "s|@SPEC|$SPEC|"` ;;
  esac
  n=$((n+1))
  eval "A$n=\$a"
done
case $n in
 4) exec ./fish7 "$A1" "$A2" "$A3" "$A4" ;;
 5) exec ./fish7 "$A1" "$A2" "$A3" "$A4" "$A5" ;;
 6) exec ./fish7 "$A1" "$A2" "$A3" "$A4" "$A5" "$A6" ;;
 7) exec ./fish7 "$A1" "$A2" "$A3" "$A4" "$A5" "$A6" "$A7" ;;
 8) exec ./fish7 "$A1" "$A2" "$A3" "$A4" "$A5" "$A6" "$A7" "$A8" ;;
 9) exec ./fish7 "$A1" "$A2" "$A3" "$A4" "$A5" "$A6" "$A7" "$A8" "$A9" ;;
 10) exec ./fish7 "$A1" "$A2" "$A3" "$A4" "$A5" "$A6" "$A7" "$A8" "$A9" "$A10" ;;
 *) echo "K5run: unsupported arg count $n" >&2; exit 2 ;;
esac
