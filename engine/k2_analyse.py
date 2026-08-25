#!/usr/bin/env python3
"""K2 -- read the v7decide jsonl the ceiling battery wrote and print the L1 table."""
import sys, json

KEYS = ["declAccuracy","declAllocErrorShare","declJointAccuracy","declExactAccuracy",
        "declShipAccuracy","jointFixRate","jointBreakRate","exactFixRate","exactBreakRate",
        "jointDiffersRate","l1Coverage","l1ExactCoverage",
        "l1FlatShare","l1FlatShareErr","l1CeilingMean","l1CeilingMeanErr",
        "l1AmbigShare","l1FlatShareAmbig","l1CeilingMeanAmbig",
        "l1AllocErrShare","l1AllocErrFlatShare","l1AllocErrCeiling"]

for path in sys.argv[1:]:
    print("=" * 92)
    print(path)
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        d = json.loads(line)
        m = d["metrics"]
        print("-" * 92)
        print("%s  seed %s  %d deals x %d  %.1f g/s" %
              (d["a"], d["seed"], d["deals"], d["rotations"], d["gamesPerSec"]))
        print("  %-22s %10s  %-22s %10s" % ("metric", "rate", "95% CI", "n"))
        for k in KEYS:
            if k not in m:
                continue
            v = m[k]
            print("  %-22s %10.5f  [%.4f, %.4f]      %10.0f" %
                  (k, v["rate"], v["ci"][0], v["ci"][1], v["n"]))
