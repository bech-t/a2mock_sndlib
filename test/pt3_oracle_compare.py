#!/usr/bin/env python3
"""pt3_oracle_compare.py -- compare pt3.py a l'oracle PT3Play.cs (reel, C#,
Sergey Bulba -- cf. tools/pt3oracle/README.md). Les periodes de l'oracle
sont NATIVES (horloge ZX) ; on les reechelonne avec NOTRE PROPRE fonction
avant de comparer, pour valider l'interpretation ET le rescale en un seul
passage. C'est CE script qui a trouve les trois bugs listes dans
tools/pt3oracle/README.md et spec.md §5.7 -- pas une relecture de code.

    tools/pt3oracle/run.sh test/pt3_corpus/oldlove.pt3 800 /tmp/oracle.csv
    python3 test/pt3_oracle_compare.py test/pt3_corpus/oldlove.pt3 /tmp/oracle.csv
"""
import csv
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "a2mconv"))
import pt3


def replay(frames):
    st = [0] * 14
    out = []
    for fr in frames:
        st = list(st)
        for r, v in fr.items():
            st[r] = v
        out.append(st)
    return out


def main():
    pt3_path, csv_path = sys.argv[1], sys.argv[2]
    verbose = "--verbose" in sys.argv

    mod = pt3.read(pt3_path)
    my_frames, loop_frame = pt3.to_apple(mod)
    my_state = replay(my_frames)

    oracle = []
    with open(csv_path) as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            oracle.append([int(x) for x in row[1:]])

    n = min(len(my_state), len(oracle))
    print("%-14s v%d  ma sortie=%d trames  oracle=%d trames  compare=%d" % (
        pt3_path.split("/")[-1], mod["version"], len(my_state), len(oracle), n))

    # Comptes de divergence par registre, sur TOUTE la plage comparee --
    # un registre "toujours faux" (bug systematique) se distingue d'un
    # registre "parfois faux de 1" (bruit d'arrondi).
    mismatch = [0] * 14
    first_diff = None
    max_abs = [0] * 14
    first_at = [None] * 14

    for i in range(n):
        mine = my_state[i]
        theirs_zx = oracle[i]
        # Reechelonnement ZX -> Apple, meme fonction que to_apple() --
        # tons (0..5) et enveloppe (11,12) sont des PERIODES ; le reste
        # (volume, mixer, bruit, forme d'enveloppe) ne l'est pas -- sauf le
        # bruit (r6) qui EST une periode, cf. ym.py/pt3.py.
        exp = list(theirs_zx)
        for base in (0, 2, 4):  # tons A/B/C : (lo,hi) -> periode 12 bits
            per_zx = theirs_zx[base] | (theirs_zx[base + 1] << 8)
            per_ap = pt3._rescale(per_zx, bits=12) if per_zx else 0
            exp[base] = per_ap & 0xFF
            exp[base + 1] = (per_ap >> 8) & 0x0F
        if theirs_zx[6]:
            exp[6] = max(1, min(31, round(theirs_zx[6] * (pt3.APPLE_CLOCK / pt3.ZX_CLOCK))))
        per_zx = theirs_zx[11] | (theirs_zx[12] << 8)
        per_ap = pt3._rescale(per_zx, bits=16) if per_zx else 0
        exp[11] = per_ap & 0xFF
        exp[12] = (per_ap >> 8) & 0xFF

        for r in range(14):
            d = abs(mine[r] - exp[r])
            if d:
                mismatch[r] += 1
                max_abs[r] = max(max_abs[r], d)
                if first_at[r] is None:
                    first_at[r] = i
                if first_diff is None:
                    first_diff = (i, r, mine[r], exp[r])
                if verbose and i < 40:
                    print("  trame %4d  r%-2d  nous=%3d  oracle(reechelonne)=%3d" %
                          (i, r, mine[r], exp[r]))

    print("  divergences par registre (sur %d trames) :" % n)
    labels = ["tonA_lo", "tonA_hi", "tonB_lo", "tonB_hi", "tonC_lo", "tonC_hi",
              "bruit", "mixer", "ampA", "ampB", "ampC", "env_lo", "env_hi", "env_forme"]
    for r in range(14):
        if mismatch[r]:
            print("    r%-2d %-9s : %5d/%d trames differentes (ecart max %d, premiere a la trame %d)" %
                  (r, labels[r], mismatch[r], n, max_abs[r], first_at[r]))
    if not any(mismatch):
        print("  IDENTIQUE sur toute la plage comparee.")
    elif first_diff:
        i, r, m, e = first_diff
        print("  premiere divergence : trame %d, r%d, nous=%d oracle=%d" % (i, r, m, e))


if __name__ == "__main__":
    main()
