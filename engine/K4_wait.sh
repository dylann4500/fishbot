#!/bin/zsh
# Block until a results file reaches N lines.  $1 = file, $2 = lines.
F="$1"; N="$2"
while [ "$(grep -c . "$F")" -lt "$N" ]; do sleep 5; done
echo "READY $F $(grep -c . "$F")"
