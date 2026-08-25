#!/usr/bin/env python3
"""Assemble RESEARCH-LOG.md section 4 from the prose parts and the GENERATED tables.

Every `{{GEN:part}}` marker in the prose is replaced by the corresponding block of
engine/build_p4_v07.py, so no table in section 4 is hand-typed.  Idempotent: the
whole of section 4 is replaced each time.

  usage: ./assemble_p4_v07.py --parts=DIR [--log=docs/v07/RESEARCH-LOG.md]
"""
import os, re, subprocess, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG = os.path.join(ROOT, "docs/v07/RESEARCH-LOG.md")
PARTS = None
for a in sys.argv[1:]:
    if a.startswith("--parts="): PARTS = a.split("=", 1)[1]
    elif a.startswith("--log="): LOG = a.split("=", 1)[1]
if not PARTS: sys.exit("assemble_p4_v07.py: --parts=DIR is required")

def gen(part):
    return subprocess.run([sys.executable, os.path.join(ROOT, "engine/build_p4_v07.py"),
                           "--dir=" + os.path.join(ROOT, "research/v07/results"),
                           "--part=" + part],
                          capture_output=True, text=True, check=True).stdout.rstrip()

body = "".join(open(f).read() for f in sorted(glob.glob(os.path.join(PARTS, "*.md"))))
body = re.sub(r"\{\{GEN:([a-z]+)\}\}", lambda m: gen(m.group(1)), body)

src = open(LOG).read()
marker = "\n---\n\n# 4. Phase 4 — bake-off, iteration, and freeze"
i = src.find(marker)
if i >= 0:
    src = src[:i]
open(LOG, "w").write(src.rstrip() + "\n" + body)
print("section 4 assembled: %d lines" % len(body.splitlines()))
