#!/usr/bin/env python3
"""pt32a2m -- un module PT3 (ZX Spectrum, Vortex Tracker II) -> A2M, profil R.

    pt32a2m.py morceau.pt3 -o SORTIE.A2M [--seconds 60] [--report]

Profil R, comme ym2a2m.py, et pour la meme raison : le renderer PT3 (pt3.py)
INTERPRETE deja le module (notes, ornements) a l'hote -- ce que produit
to_apple() est un flux de registres, pas des evenements a rejouer sur
6502. Voir la docstring de pt3.py pour ce qui est couvert (etape 1+2 de
spec.md §5.7) et ce qui ne l'est pas encore (les effets, les echantillons).

Le morceau occupe UN AY (PT3 = trois voies natif), l'AY #2 reste libre pour
les bruitages -- comme pour le YM.
"""

import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a2m, pt3


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--title", default=""); ap.add_argument("--author", default="")
    ap.add_argument("--seconds", type=float, default=0.0,
                     help="tronquer (0 = tout le morceau)")
    ap.add_argument("--hz", type=int, default=50)
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    mod = pt3.read(args.source)
    maxf = int(args.seconds * args.hz) if args.seconds else None
    frames, loop_frame = pt3.to_apple(mod, maxf)

    blob = a2m.encode(frames,
                       title=args.title or mod["name"],
                       author=args.author or mod["author"],
                       hz=args.hz, loop_frame=loop_frame, looping=True, n_ay=1)
    open(args.output, "wb").write(blob)

    secs = len(frames) / float(args.hz)
    print("%-14s %6d o  %5d trames  %5.1f s  %4.0f o/s  profil R  %s" % (
        os.path.basename(args.output), len(blob), len(frames), secs,
        (len(blob) - a2m.HEADER_SIZE) / max(secs, .001),
        args.title or mod["name"]))

    if args.report:
        print("   source  : PT3 v%d, table de frequence %d, vitesse %d, "
              "%d motifs joues" % (mod["version"], mod["freq_table"],
                                    mod["speed"], len(mod["order"])))
        print("   rapport d'horloge applique : %.4f (periodes ton/enveloppe)"
              % (pt3.APPLE_CLOCK / pt3.ZX_CLOCK))
        if mod["n_effects_ignored"]:
            print("   /!\\ %d effet(s) rencontre(s) et IGNORES (glissando, "
                  "portamento, vibrato...) -- consommes sans etre appliques,"
                  % mod["n_effects_ignored"])
            print("       cf. pt3.py, etape 3 non faite. Un morceau qui en "
                  "dépend sonnera juste mais statique.")
        print("   /!\\ echantillons (enveloppes d'instrument) et bruit : "
              "NON reproduits en v1 -- cf. docstring de pt3.py.")

        # Aller-retour : comme ym2a2m.py, le decodeur doit relire EXACTEMENT
        # ce qu'on a ecrit.
        st = [0] * 14
        expect = []
        for fr in frames:
            st = list(st)
            for r, v in fr.items():
                st[r] = v
            expect.append(list(st))
        assert a2m.decode(blob) == expect, "ALLER-RETOUR ROMPU"
        print("   aller-retour : %d trames identiques" % len(expect))

        # Justesse : notes distinctes effectivement utilisees.
        notes_vus = set()
        for pat in mod["patterns"].values():
            for c in "ABC":
                for ev in pat[c]:
                    if "note" in ev:
                        notes_vus.add(ev["note"])
        if notes_vus:
            pire = max(notes_vus, key=lambda i: abs(pt3.cents_error(i)))
            print("   justesse : pire ecart %.1f cents (note d'indice %d)"
                  % (pt3.cents_error(pire), pire))


if __name__ == "__main__":
    main()
