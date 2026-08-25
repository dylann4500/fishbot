#!/bin/zsh
# Block until a file reaches N lines, or until a deadline.
# $1 = file, $2 = lines, $3 = max seconds (default 580).
F="$1"; N="$2"; MAX="${3:-580}"
E=0
while [ "$(grep -c . "$F")" -lt "$N" ]; do
  sleep 5; E=$((E+5))
  if [ "$E" -ge "$MAX" ]; then echo "TIMEOUT $F $(grep -c . "$F")/$N"; exit 0; fi
done
echo "READY $F $(grep -c . "$F")"
