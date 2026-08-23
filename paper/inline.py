#!/usr/bin/env python3
"""Flatten the modular paper into one self-contained .tex for Overleaf."""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))

def expand(path, depth=0):
    out = []
    with open(path) as f:
        for line in f:
            # \InputIfFileExists{f}{true}{false} must be flattened too, or the
            # Overleaf copy loses the generated numbers file and prints
            # placeholders.  The file exists here by construction (we are
            # flattening a build that just succeeded), so the true branch wins.
            mi = re.match(r'\s*\\InputIfFileExists\{([^}]+)\}\{([^}]*)\}\{[^}]*\}\s*$', line)
            if mi and depth < 6:
                target = os.path.join(HERE, mi.group(1))
                if not target.endswith('.tex'): target += '.tex'
                if os.path.exists(target):
                    out.append('%% --- begin ' + mi.group(1) + ' (InputIfFileExists) ---\n')
                    out.append(expand(target, depth + 1))
                    if mi.group(2).strip(): out.append(mi.group(2) + '\n')
                    out.append('%% --- end ' + mi.group(1) + ' ---\n')
                    continue
            m = re.match(r'\s*\\input\{([^}]+)\}\s*$', line)
            if m and depth < 6:
                target = os.path.join(HERE, m.group(1))
                if not target.endswith('.tex'): target += '.tex'
                if os.path.exists(target):
                    out.append('%% --- begin ' + m.group(1) + ' ---\n')
                    out.append(expand(target, depth + 1))
                    out.append('%% --- end ' + m.group(1) + ' ---\n')
                    continue
            # inline \input inside a line (e.g. within a sentence)
            def sub(mm):
                target = os.path.join(HERE, mm.group(1))
                if not target.endswith('.tex'): target += '.tex'
                if os.path.exists(target) and depth < 6:
                    return expand(target, depth + 1).strip()
                return mm.group(0)
            out.append(re.sub(r'\\input\{([^}]+)\}', sub, line))
    return ''.join(out)

if __name__ == '__main__':
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'fishbot_v04.tex')
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'fishbot_v04_standalone.tex')
    open(dst, 'w').write(expand(src))
    print('wrote', dst)
