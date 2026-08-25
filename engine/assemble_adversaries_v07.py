#!/usr/bin/env python3
"""Render docs/v07/ADVERSARIES.md from docs/v07/ADVERSARIES.md.in.

The project's standing convention is that no number in a deliverable is hand-typed
(engine/build_tables_v06.py; engine/build_tables_v07.py).  For a markdown
deliverable that means two substitutions:

  <!-- TABLE: name -->   is replaced by research/v07/results/tables/name.md
  {{macro}}              is replaced by the value of `macro` in tables/numbers.json

Anything left unsubstituted is reported and fails the build, so a stale macro
cannot be shipped as literal braces.
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
TAB = os.path.join(ROOT, 'research', 'v07', 'results', 'tables')
SRC = os.path.join(ROOT, 'docs', 'v07', 'ADVERSARIES.md.in')
DST = os.path.join(ROOT, 'docs', 'v07', 'ADVERSARIES.md')

def main():
    text = open(SRC).read()
    nums = {}
    p = os.path.join(TAB, 'numbers.json')
    if os.path.exists(p): nums = json.load(open(p))

    missing_tables, missing_macros = [], []

    def table(m):
        name = m.group(1).strip()
        fp = os.path.join(TAB, name + '.md')
        if not os.path.exists(fp):
            missing_tables.append(name)
            return (f"> **`{name}` — battery incomplete at the time of writing.** Its artifact does "
                    f"not exist, so no number from it appears anywhere in this document. Re-running "
                    f"`engine/build_adversaries_v07.py` and `engine/assemble_adversaries_v07.py` "
                    f"after the battery finishes will splice it in without any other change.")
        return open(fp).read().rstrip('\n')

    text = re.sub(r'<!--\s*TABLE:\s*([A-Za-z0-9_.\-]+)\s*-->', table, text)

    def macro(m):
        k = m.group(1)
        if k not in nums:
            missing_macros.append(k); return '{{' + k + '}}'
        v = nums[k]
        return v if isinstance(v, str) else f"{v}"

    text = re.sub(r'\{\{([A-Za-z0-9_]+)\}\}', macro, text)

    open(DST, 'w').write(text)
    print(f"wrote {DST}  ({len(text.splitlines())} lines)")
    if missing_tables: print("MISSING TABLES :", sorted(set(missing_tables)))
    if missing_macros: print("MISSING MACROS :", sorted(set(missing_macros)))
    return 1 if (missing_tables or missing_macros) else 0

if __name__ == '__main__': sys.exit(main())
