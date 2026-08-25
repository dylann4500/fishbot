#!/usr/bin/env python3
"""Splice the generated phase-3 profile section into docs/v07/CANDIDATES.md section 8.

Replaces everything between the "## 8." heading line and the next "## " heading,
so the section can be regenerated without hand-editing.  Idempotent.
"""
import subprocess, sys, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "docs/v07/CANDIDATES.md")
HEAD = "## 8. The common profile: worst case and minimax regret over the panel"

# The arm filter is deliberate.  Phase 4 added a FROZEN arm to the same panel and
# its cells sit in the same directory, but section 8 is PHASE 3's deliverable and
# reporting a phase-4 object in it would be anachronistic.  The three-arm panel is
# RESEARCH-LOG section 4.10.
body = subprocess.run([sys.executable, os.path.join(ROOT, "engine/build_profile_v07.py"),
                       "--dir=" + os.path.join(ROOT, "research/v07/results"),
                       "--arms=A0-v06,K3-stack"],
                      capture_output=True, text=True, check=True).stdout

src = open(DOC).read()
i = src.index(HEAD)
j = src.index("\n## ", i + len(HEAD))
src = src[:i] + HEAD + "\n\n" + body.rstrip() + "\n" + src[j:]
open(DOC, "w").write(src)
print("spliced %d lines into section 8" % len(body.splitlines()))
