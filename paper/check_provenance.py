#!/usr/bin/env python3
"""Provenance audit for the v0.5 manuscript's numbers.

Two classes of number appear in paper/numbers_v05.tex:

  * GENERATED -- emitted by engine/build_tables_v05.py straight from an
    experiment artifact and \\renewcommand'd over the placeholder, so it cannot
    drift from the artifact.
  * TRANSCRIBED -- read by a human (or an agent) out of a diagnosis report and
    typed in. These are the ones that can silently go stale, so each must sit
    under a comment header naming the artifact it came from, and that artifact
    must exist on disk.

This script fails if a transcribed macro has no source header, or names an
artifact that is missing. It also reports the transcribed/generated split, which
the manuscript's reproducibility appendix quotes.

Exit status is non-zero if any macro is unattributed or any named source is absent.
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PLACEHOLDER = os.path.join(HERE, 'numbers_v05.tex')
GENERATED = os.path.join(HERE, 'numbers_v05_generated.tex')
SECTIONS = os.path.join(HERE, 'sections_v05')

SRC_RE = re.compile(r'(research/[^\s,;]+|engine/[^\s,;]+|docs/[^\s,;]+|paper/[^\s,;]+)')


def used_macros():
    used = set()
    for path in glob.glob(os.path.join(SECTIONS, '*.tex')):
        used |= set(re.findall(r'\\(num[A-Za-z]+)', open(path).read()))
    return used


def generated_macros():
    if not os.path.exists(GENERATED):
        return set()
    return set(re.findall(r'renewcommand\{\\(num[A-Za-z]+)\}', open(GENERATED).read()))


def placeholder_attribution():
    """Map macro -> the artifact named by the nearest preceding comment header."""
    attrib, current = {}, None
    for line in open(PLACEHOLDER):
        if line.startswith('%'):
            m = SRC_RE.search(line)
            if m:
                # Comments name paths inside prose, so strip trailing punctuation
                # that belongs to the sentence rather than to the path.
                current = m.group(1).rstrip(').,;:')
            continue
        m = re.search(r'providecommand\{\\(num[A-Za-z]+)\}', line)
        if m:
            attrib[m.group(1)] = current
    return attrib


def main():
    used = used_macros()
    gen = generated_macros()
    attrib = placeholder_attribution()

    transcribed = sorted(used - gen)
    problems = []
    missing_src = {}

    for m in transcribed:
        src = attrib.get(m)
        if not src:
            problems.append('unattributed: \\%s has no source-artifact comment header' % m)
            continue
        if not os.path.exists(os.path.join(ROOT, src)):
            missing_src.setdefault(src, []).append(m)

    for src, ms in sorted(missing_src.items()):
        problems.append('missing artifact: %s (cited by %d macro%s, e.g. \\%s)'
                        % (src, len(ms), '' if len(ms) == 1 else 's', ms[0]))

    print('macros used in sections_v05/ : %d' % len(used))
    print('  generated from artifacts   : %d' % len(used & gen))
    print('  transcribed from reports   : %d' % len(transcribed))
    srcs = sorted({attrib.get(m) for m in transcribed if attrib.get(m)})
    print('  distinct source artifacts  : %d' % len(srcs))
    for s in srcs:
        n = sum(1 for m in transcribed if attrib.get(m) == s)
        mark = ' ' if os.path.exists(os.path.join(ROOT, s)) else ' MISSING'
        print('      %-58s %3d%s' % (s, n, mark))

    if problems:
        print()
        for p in problems:
            print('problem: %s' % p)
        print('\n%d problem(s).' % len(problems))
        return 1
    print('\nevery transcribed number is attributed to an artifact that exists.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
