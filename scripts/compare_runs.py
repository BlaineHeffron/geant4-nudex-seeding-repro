#!/usr/bin/env python3
"""Compare the capture records of two capture_repro runs.

Captures are grouped by compound nucleus and paired in order: the k-th capture
on a nucleus in run A against the k-th in run B. A pair is "identical" when the
emitted particle types match in order and every kinetic energy agrees within
TOL_KEV (the lab boost of a thermal capture shifts a 7 MeV gamma by about 1 eV).
Capture positions are compared too, to show the neutron histories differ.
"chance" is the number of identical pairs expected if the two runs drew cascades
independently from their own frequencies: sum over distinct cascades (energies
rounded to 0.1 keV) of n_A(c) * n_B(c) / pairs.
"""
import sys
from collections import Counter, defaultdict

TOL_KEV = 0.05


def load(path):
    caps = defaultdict(list)
    for line in open(path):
        f = line.split()
        nuc = f"Z={f[1]} A={f[2]}"
        pos = tuple(f[3:6])
        parts = [(p[0], float(p[2:])) for p in f[8:]]
        caps[nuc].append((pos, f[6], parts))
    return caps


def same(a, b):
    return len(a) == len(b) and all(x[0] == y[0] and abs(x[1] - y[1]) < TOL_KEV for x, y in zip(a, b))


def key(parts):
    return tuple((t, round(e, 1)) for t, e in parts)


def chance(xs, ys):
    ca, cb = Counter(key(x[2]) for x in xs), Counter(key(y[2]) for y in ys)
    return sum(ca[c] * cb[c] for c in ca) / len(xs)


def main(a_path, b_path):
    a, b = load(a_path), load(b_path)
    print(f"{'nucleus':<12} {'pairs':>6} {'same cascade':>13} {'chance':>7} {'same position':>14}  model")
    tot = tot_same = tot_chance = 0
    for nuc in sorted(set(a) & set(b)):
        pairs = list(zip(a[nuc], b[nuc]))
        n_same = sum(same(x[2], y[2]) for x, y in pairs)
        n_pos = sum(x[0] == y[0] for x, y in pairs)
        exp = chance([x for x, _ in pairs], [y for _, y in pairs])
        models = sorted({x[1] for x, _ in pairs} | {y[1] for _, y in pairs})
        print(f"{nuc:<12} {len(pairs):>6} {n_same:>13} {exp:>7.0f} {n_pos:>14}  {','.join(models)}")
        tot += len(pairs)
        tot_same += n_same
        tot_chance += exp
    print(f"{'all':<12} {tot:>6} {tot_same:>13} {tot_chance:>7.0f}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: compare_runs.py RUN_A.txt RUN_B.txt")
    main(*sys.argv[1:])
