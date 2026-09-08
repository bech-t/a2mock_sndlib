#!/usr/bin/env python3
"""a2mconv -- partition lisible -> module A2M.

    a2mconv.py partition.txt -o SORTIE.A2M [--profil T|R] [--rapport]

Profil T par defaut. Une partition DECLARE l'enveloppe de chacune de ses voix
(`voix A: pincee`) : l'information est deja la, il serait absurde d'epeler
ensuite l'amplitude trame par trame comme le fait le profil R.

Ces partitions n'occupent qu'UN AY -- trois voix. L'AY #2 reste donc libre
pour les bruitages, ce qui est la configuration d'un jeu (cf. la regle de
propriete des puces, spec §13).

Le rapport n'est pas un ornement : il chiffre la JUSTESSE. La periode de l'AY
tient sur 12 bits, et plus une note est aigue, plus le pas de quantification
est gros en proportion -- au-dessus de do6 l'ecart devient audible. Mieux vaut
le savoir avant de graver la disquette qu'apres l'avoir ecoutee.
"""

import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a2m, render


def score_events(sc):
    """Partition -> evenements du profil T.

    C'est direct : une partition est DEJA trois voix monophoniques, avec leur
    enveloppe declaree. Il n'y a ni accord a departager, ni instrument a
    deviner -- contrairement a une reduction d'orchestre."""
    instrs = []
    vinstr = []
    for v in "ABC":
        name = sc.env.get(v, "pluck")
        if name not in a2m.INSTRUMENTS:
            # Un repli SILENCIEUX serait un piege : une faute de frappe
            # ("pincée" avec l'accent) changerait le son sans rien dire.
            sys.stderr.write(
                "  /!\\ voix %s : enveloppe %r inconnue, repli sur 'pluck'.\n"
                "      connues : %s\n" % (v, name, ", ".join(a2m.INSTRUMENTS)))
            name = "pluck"
        if name not in instrs:
            instrs.append(name)
        vinstr.append(instrs.index(name))

    # Placer chaque note sur la ligne du temps, voix par voix.
    at = {}
    total = 0
    fins = []
    for vi, v in enumerate("ABC"):
        t = 0
        for idx, dur in sc.voix[v]:
            at.setdefault(t, []).append(("off", vi) if idx is None
                                        else ("on", vi, idx))
            t += dur
        if t and sc.voix[v] and sc.voix[v][-1][0] is not None:
            fins.append((t, vi))    # la voix finit sur une NOTE, pas un silence
        total = max(total, t)

    # Une voix plus courte que les autres doit se TAIRE a sa fin. Sans ce
    # note-off, sa derniere note continue de sonner jusqu'au rebouclage --
    # inaudible avec une enveloppe pincee (elle a decline), mais une voix
    # `soutenue` ou `douce` tiendrait indefiniment. Le piege ne se revele
    # alors qu'en changeant d'enveloppe.
    for t, vi in fins:
        if t < total:
            at.setdefault(t, []).append(("off", vi))

    events = [("instr", i, vinstr[i]) for i in range(3)]
    events.append(("loop",))
    prev = 0
    for t in sorted(at):
        if t > prev:
            events.append(("wait", t - prev))
        events.extend(at[t])
        prev = t
    if total > prev:
        events.append(("wait", total - prev))
    return events, instrs, total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--profile", default="T", choices=("T", "R"))
    ap.add_argument("--report", action="store_true")
    args = ap.parse_args()

    sc = render.Score.load(args.source)
    frames, used = render.render(sc)          # sert au rapport de justesse

    if args.profile == "T":
        events, instrs, nframes = score_events(sc)
        blob = a2m.encode_t(events, instrs, title=sc.titre, author=sc.auteur,
                            hz=sc.hz, looping=sc.boucle, n_ay=1,
                            n_frames=nframes)
        n_notes = sum(1 for e in events if e[0] == "on")
    else:
        nframes = len(frames)
        blob = a2m.encode(frames, title=sc.titre, author=sc.auteur,
                          hz=sc.hz, looping=sc.boucle, n_ay=1)
        n_notes = None

    with open(args.output, "wb") as f:
        f.write(blob)

    secs = nframes / float(sc.hz)
    print("%-14s %5d o  %5d trames  %5.1f s  %4.0f o/s  profil %s  %s" % (
        os.path.basename(args.output), len(blob), nframes, secs,
        (len(blob) - a2m.HEADER_SIZE) / max(secs, 0.001), args.profile, sc.titre))
    if n_notes is not None:
        print("   %d notes, %d instruments" % (n_notes, len(instrs)))

    if args.report and args.profile == "R":
        # Aller-retour : ce que le decodeur relit doit etre EXACTEMENT ce que
        # le rendu a produit. Une erreur d'ordre des valeurs ou de bit de
        # masque est invisible a l'oreille et fatale au lecteur 6502.
        st = [0] * 14
        expect = []
        for fr in frames:
            st = list(st)
            for r, v in fr.items():
                st[r] = v
            expect.append(list(st))
        got = a2m.decode(blob)
        assert got == expect, "ALLER-RETOUR ROMPU"
        print("   aller-retour : %d trames identiques" % len(got))

        worst = 0.0
        worst_n = 0
        for i in sorted(used):
            c = render.cents_error(i)
            if abs(c) > abs(worst):
                worst, worst_n = c, i
        print("   justesse : %+.1f cents au pire (octave %d), etendue %d..%d"
              % (worst, worst_n // 12, min(used) // 12, max(used) // 12))
        if abs(worst) > 10:
            print("   /!\\ au-dela de 10 cents : audible.")

        raw = len(frames) * 14
        print("   compression : %d o brut -> %d o (%.0f %%)"
              % (raw, len(blob) - a2m.HEADER_SIZE,
                 100.0 * (len(blob) - a2m.HEADER_SIZE) / max(raw, 1)))


if __name__ == "__main__":
    main()
