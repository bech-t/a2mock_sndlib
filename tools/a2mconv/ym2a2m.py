#!/usr/bin/env python3
"""ym2a2m -- un dump YM (Atari ST, ZX...) -> module A2M, profil R.

    ym2a2m.py morceau.ym -o SORTIE.A2M [--secondes 60] [--rapport]

Profil R obligatoirement, et c'est le point : un YM ne dit rien de ses
instruments, seulement l'etat des registres a chaque trame. Le profil T, qui
fabrique les enveloppes, aurait besoin de les DEVINER -- c'est un probleme
dur, et une erreur y serait inaudible a la conversion et fausse a l'ecoute.
Le profil R ne suppose rien, donc il ne peut pas se tromper.

Le morceau occupe UN seul AY (le YM2149 a trois voies), ce qui laisse l'AY #2
libre pour les bruitages.
"""

import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a2m, ym


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--title", default=""); ap.add_argument("--author", default="")
    ap.add_argument("--seconds", type=float, default=0.0,
                    help="tronquer (0 = tout le morceau)")
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    y = ym.read(args.source)
    maxf = int(args.seconds * y["hz"]) if args.seconds else None
    frames = ym.to_apple(y, maxf)

    blob = a2m.encode(frames,
                      title=args.title or y["name"],
                      author=args.author or y["author"],
                      hz=y["hz"], looping=True, n_ay=1)
    open(args.output, "wb").write(blob)

    secs = len(frames) / float(y["hz"])
    print("%-14s %6d o  %5d trames  %5.1f s  %4.0f o/s  profil R  %s" % (
        os.path.basename(args.output), len(blob), len(frames), secs,
        (len(blob) - a2m.HEADER_SIZE) / max(secs, .001),
        args.title or y["name"]))

    if args.report:
        print("   source  : %s, %d trames, %d Hz, horloge %d Hz"
              % (y["format"], y["frames"], y["hz"], y["clock"]))
        print("   rapport d'horloge applique : %.4f (periodes ton/bruit/enveloppe)"
              % (ym.APPLE_CLOCK / y["clock"]))
        if y["digidrums"]:
            print("   /!\\ %d digidrums IGNORES : ils demandent des ecritures"
                  " d'amplitude a plusieurs kHz," % y["digidrums"])
            print("       quand notre tick en fait 50. Le morceau perdra sa"
                  " percussion.")
        # Aller-retour : le decodeur doit relire exactement ce qu'on a ecrit.
        st = [0] * 14
        expect = []
        for fr in frames:
            st = list(st)
            for r, v in fr.items():
                st[r] = v
            expect.append(list(st))
        assert a2m.decode(blob) == expect, "ALLER-RETOUR ROMPU"
        print("   aller-retour : %d trames identiques" % len(expect))

        # Combien de notes tombent dans la zone ou l'AY est faux ?
        haut = 0
        for fr in frames:
            for ch in range(3):
                if ch * 2 in fr:
                    pass
        used_per = set()
        st = [0] * 14
        for fr in frames:
            for r, v in fr.items():
                st[r] = v
            for ch in range(3):
                p = ((st[ch * 2 + 1] & 0x0F) << 8) | st[ch * 2]
                if p and st[8 + ch] & 0x0F:
                    used_per.add(p)
        if used_per:
            print("   periodes utilisees : %d a %d  (petite = aigue ;"
                  " sous 60, l'AY commence a sonner faux)"
                  % (min(used_per), max(used_per)))


if __name__ == "__main__":
    main()
