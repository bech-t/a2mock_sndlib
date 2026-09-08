#!/usr/bin/env python3
"""midi2score -- un MIDI d'orchestre -> une partition TEXTE a trois voix.

Pourquoi passer par le texte alors que midi2a2m sait deja convertir : parce
qu'une partition se CORRIGE. Le MIDI donne les bonnes notes, le texte les rend
modifiables -- on change une hauteur, un `make music`, et c'est refait. Sans
compter qu'a trois voix le morceau ne prend qu'UN AY, ce qui laisse l'autre
aux bruitages (cf. la regle de propriete des puces).

C'est une SIMPLIFICATION, et il faut le dire : trois voix au lieu de six,
un seul tempo au lieu de la carte des tempos du MIDI, et les durees ramenees
a une grille. Ce qu'on y gagne, c'est un fichier qu'on peut lire et retoucher.

    midi2score.py source.mid -o partition.txt --tempo 104 \\
        --voix "8:0" --voix "8:1" --voix "12+13:0"
"""

import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import midi

NOMS = ['c', 'c#', 'd', 'd#', 'e', 'f', 'f#', 'g', 'g#', 'a', 'a#', 'b']

# Durees representables dans le format, en SEIZIEMES de ronde... c'est-a-dire
# en croches-doubles. 16 = une ronde.
DUREES = [(16, "1"), (12, "2."), (8, "2"), (6, "4."), (4, "4"),
          (3, "8."), (2, "8"), (1, "16")]


def nom_note(midi_pitch):
    """MIDI 60 = do4 ; notre echelle a do0 = index 0, donc -12."""
    i = midi_pitch - 12
    return "%s%d" % (NOMS[i % 12], i // 12)


def decoupe(n):
    """n croches-doubles -> suite de durees representables (la plus longue
    d'abord). Une note de 5 devient donc `/4` puis un silence `/16` : on garde
    l'ALIGNEMENT plutot que d'arrondir, quitte a raccourcir la note. Sur un
    instrument pince la difference ne s'entend pas -- la note a deja fini de
    decliner."""
    out = []
    while n > 0:
        for v, t in DUREES:
            if v <= n:
                out.append((v, t)); n -= v; break
        else:
            break
    return out


def extraire(m, tracks, rank, grid, max_ticks):
    """-> liste (debut, duree, hauteur) en unites de grille, monophonique.

    `tracks` est une liste par ORDRE DE PRIORITE, pas un simple ensemble. A
    chaque case, on prend la premiere piste qui a quelque chose a dire ; on ne
    descend a la suivante que si elle se tait.

    Fusionner les pistes et prendre la note la plus haute serait plus simple et
    donnerait un resultat faux : dans la Danse arabe, les flutes montent au
    dessus du cor anglais qui porte pourtant la melodie, et la ligne saute d'un
    instrument a l'autre. Mesure sur la Danse arabe : en fusionnant les vents,
    41 % seulement des notes de la melodie survivaient ; en donnant la priorite
    au cor anglais qui la porte, 100 %."""
    div = m["division"]
    step = div * 4 // grid            # ticks par unite de grille
    par_piste = []
    span = 0
    for ti in tracks:
        if ti < len(m["tracks"]):
            ns = [(n.t, n.dur, n.pitch) for n in m["tracks"][ti]
                  if n.t < max_ticks]
            par_piste.append(ns)
            if ns:
                span = max(span, max(t + d for t, d, _ in ns))
    if not any(par_piste):
        return []
    span = min(span, max_ticks)
    cells = span // step + 1

    ligne = [None] * cells
    for c in range(cells):
        t = c * step
        for ns in par_piste:              # dans l'ordre de priorite
            son = sorted((p for t0, d, p in ns if t0 <= t < t0 + d),
                         reverse=True)
            if len(son) > rank:
                ligne[c] = son[rank]
                break

    # Regrouper les cases identiques en notes ; une nouvelle attaque quand la
    # hauteur change.
    out = []
    c = 0
    while c < cells:
        p = ligne[c]
        k = c
        while k < cells and ligne[k] == p:
            k += 1
        out.append((c, k - c, p))
        c = k
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--title", default=""); ap.add_argument("--author", default="")
    ap.add_argument("--tempo", type=int, default=100)
    ap.add_argument("--grid", type=int, default=16, help="16 = croche-double")
    ap.add_argument("--bars", type=int, default=32)
    ap.add_argument("--transpose", type=int, default=0)
    ap.add_argument("--voice", action="append", required=True,
                    help="pistes:rang[:enveloppe], trois fois")
    args = ap.parse_args()

    m = midi.read(args.source)
    max_ticks = args.bars * 2 * m["division"]        # 2/4 : 2 noires/mesure

    lignes = {}
    envs = {}
    for vi, spec in enumerate("ABC"):
        if vi >= len(args.voice):
            lignes[spec] = []; envs[spec] = "pluck"; continue
        # Completer champ par champ : concatener des valeurs par defaut
        # DECALE les champs quand l'utilisateur en donne deja deux.
        p = args.voice[vi].split(":")
        trks = [int(x) for x in p[0].split("+") if x]
        rank = int(p[1]) if len(p) > 1 and p[1] else 0
        envs[spec] = p[2] if len(p) > 2 and p[2] else "pluck"
        lignes[spec] = extraire(m, trks, rank, args.grid, max_ticks)

    with open(args.output, "w", encoding="utf-8") as f:
        f.write("# %s\n" % (args.title or os.path.basename(args.source)))
        f.write("# Extrait d'un MIDI par tools/a2mconv/midi2score.py, puis\n")
        f.write("# SIMPLIFIE : trois voix (un seul AY, l'autre reste libre pour\n")
        f.write("# les bruitages), un seul tempo, durees ramenees a une grille\n")
        f.write("# de 1/%d. Corrigez-le a la main : c'est fait pour.\n" % args.grid)
        f.write("title: %s\nauthor: %s\ntempo: %d\nhz: 50\nloop: yes\n\n"
                % (args.title.upper(), args.author.upper(), args.tempo))
        for v in "ABC":
            f.write("voice %s: %s\n" % (v, envs[v]))
        f.write("\n")

        for v in "ABC":
            toks = []
            for _, dur, p in lignes[v]:
                unites = max(1, dur * 16 // args.grid)
                parts = decoupe(unites)
                if p is None:
                    for _, t in parts:
                        toks.append("r/%s" % t)
                else:
                    nom = nom_note(p + args.transpose)
                    for i, (_, t) in enumerate(parts):
                        toks.append(("%s/%s" % (nom, t)) if i == 0 else "r/%s" % t)
            for i in range(0, len(toks), 8):
                f.write("%s: %s\n" % (v, " ".join(toks[i:i+8])))
            f.write("\n")

    n = sum(len([x for x in lignes[v] if x[2] is not None]) for v in "ABC")
    print("%s : %d notes sur 3 voix, %d mesures" % (args.output, n, args.bars))


if __name__ == "__main__":
    main()
