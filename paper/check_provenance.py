#!/usr/bin/env python3
"""Provenance audit for a manuscript's numbers.

Pass ``--version v05 | v06 | v07`` (default ``v05``, so the invocation the v0.5
study used is unchanged). Each version names its placeholder file, its generated
file, its sections directory, the macro prefix its generator emits, and the
prefixes that mean "inherited from an earlier study".

Two classes of number appear in a manuscript:

  * GENERATED -- emitted by the version's ``build_tables`` script straight from
    an experiment artifact and ``\\renewcommand``'d over the placeholder, so it
    cannot drift from the artifact.
  * TRANSCRIBED -- read by a human (or an agent) out of a diagnosis report and
    typed in. These are the ones that can silently go stale, so each must sit
    under a comment header naming the artifact it came from, and that artifact
    must exist on disk.

A macro counts as a *number* only if some numbers file declares it. Commands
declared in the master document -- the policy names ``\\vsix``, ``\\vseven`` and
friends -- are prose, not measurements, and are excluded by that rule rather
than by a hand-maintained skip list. A macro used in a section but declared
nowhere is reported separately and loudly, because it typesets as empty.

Exit status is non-zero if any number is unattributed, names an artifact that is
absent, or is undeclared.
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Per version: the prefix the generator emits, and the prefixes that mean the
# number was carried in from an earlier study. v0.7 inherits v0.5's and v0.6's
# macros as transcribed, because a v0.7 section quoting an earlier study's
# number is exactly the case this audit exists for.
VERSIONS = {
    'v05': dict(generated='num', inherited=(), master='fishbot_v05.tex'),
    'v06': dict(generated='vsix', inherited=('num',), master='fishbot_v06.tex'),
    'v07': dict(generated='vseven', inherited=('num', 'vsix'), master='fishbot_v07.tex'),
}

SRC_RE = re.compile(r'(research/[^\s,;]+|engine/[^\s,;]+|docs/[^\s,;]+|paper/[^\s,;]+)')


def used_macros(sections, prefixes):
    """Every macro of an audited prefix that a section actually uses."""
    used = set()
    pat = re.compile(r'\\((?:%s)[A-Za-z]+)' % '|'.join(prefixes))
    for path in sorted(glob.glob(os.path.join(sections, '*.tex'))):
        used |= set(pat.findall(open(path).read()))
    return used


def generated_macros(generated, prefix):
    if not os.path.exists(generated):
        return set()
    return set(re.findall(r'renewcommand\{\\(%s[A-Za-z]+)\}' % prefix,
                          open(generated).read()))


def prose_commands(master, prefixes):
    """Commands the master document declares -- policy names like \\vsix and
    \\vsevensearch. These are prose, not measurements, so they are excluded from
    the audit rather than reported as undeclared numbers."""
    if not os.path.exists(master):
        return set()
    pat = re.compile(r'(?:DeclareRobustCommand|newcommand|providecommand)\{\\((?:%s)[A-Za-z]*)\}'
                     % '|'.join(prefixes))
    return set(pat.findall(open(master).read()))


def declared_in(path, prefixes):
    """Macros a numbers file declares, whatever the declaring command."""
    if not os.path.exists(path):
        return set()
    pat = re.compile(r'(?:provide|new|renew)command\{\\((?:%s)[A-Za-z]+)\}' % '|'.join(prefixes))
    return set(pat.findall(open(path).read()))


def placeholder_attribution(placeholder, prefixes):
    """Map macro -> the artifact named by the nearest preceding comment header."""
    attrib, current = {}, None
    if not os.path.exists(placeholder):
        return attrib
    decl = re.compile(r'providecommand\{\\((?:%s)[A-Za-z]+)\}' % '|'.join(prefixes))
    for line in open(placeholder):
        if line.startswith('%'):
            m = SRC_RE.search(line)
            if m:
                # Comments name paths inside prose, so strip trailing punctuation
                # and a possessive that belong to the sentence, not to the path.
                p = m.group(1).rstrip(').,;:')
                if p.endswith("'s"):
                    p = p[:-2]
                current = p
            continue
        m = decl.search(line)
        if m:
            attrib[m.group(1)] = current
    return attrib


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--version', default='v05', choices=sorted(VERSIONS))
    args = ap.parse_args()
    cfg = VERSIONS[args.version]
    v = args.version

    placeholder = os.path.join(HERE, 'numbers_%s.tex' % v)
    generated = os.path.join(HERE, 'numbers_%s_generated.tex' % v)
    sections = os.path.join(HERE, 'sections_%s' % v)
    prefixes = (cfg['generated'],) + tuple(cfg['inherited'])

    used = used_macros(sections, prefixes)
    gen = generated_macros(generated, cfg['generated'])
    attrib = placeholder_attribution(placeholder, prefixes)

    # A macro is a NUMBER only if a numbers file declares it. Everything else a
    # section writes with an audited prefix is a prose command from the master
    # document, and prose is not a measurement.
    declared = set(attrib) | gen
    for extra in ('numbers.tex', 'numbers_v05.tex', 'numbers_v06.tex'):
        declared |= declared_in(os.path.join(HERE, extra), prefixes)

    prose = prose_commands(os.path.join(HERE, cfg['master']), prefixes)
    numbers = (used - prose) & declared
    undeclared = sorted(used - prose - declared)
    transcribed = sorted(numbers - gen)

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

    for m in undeclared:
        problems.append('undeclared: \\%s is used in sections_%s/ but no numbers '
                        'file declares it -- it typesets as empty' % (m, v))

    print('numbers used in sections_%s/ : %d' % (v, len(numbers)))
    print('  generated from artifacts   : %d' % len(numbers & gen))
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
