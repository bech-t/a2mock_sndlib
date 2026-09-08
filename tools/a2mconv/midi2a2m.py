#!/usr/bin/env python3
"""midi2a2m -- un MIDI d'orchestre -> six voies d'AY.

Le probleme n'est pas technique, il est MUSICAL : un orchestre a quarante
portees, la Mockingboard en a six. Reduire, c'est choisir -- et le choix se
declare ici, par piece, plutot que d'etre subi par un algorithme generique.

Une voie = un ou plusieurs pistes MIDI, plus un RANG. Quand la piste joue un
accord, le rang 0 prend la note la plus haute, le rang 1 la suivante, etc.
C'est ainsi qu'un celesta qui joue a trois notes occupe trois voies.

Repartition sur les deux puces : voies 0-2 sur l'AY #1, voies 3-5 sur l'AY #2.
Comme chaque AY a sa propre sortie audio, ce n'est pas un detail de cablage --
c'est un placement STEREO. Le celesta a gauche, les cordes a droite.
"""

import argparse, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import a2m, midi, render

VIA_CLOCK = 1020500.0


class Voice:
    def __init__(self, tracks, rank=0, env="pluck", transpose=0, gain=0):
        self.tracks = tracks      # indices de pistes MIDI a fusionner
        self.rank = rank          # 0 = note la plus haute de l'accord
        self.env = env
        self.transpose = transpose
        self.gain = gain          # correction d'amplitude, en pas AY


def tick_to_frame(m, hz):
    """Table tick -> numero de trame, en suivant la carte des tempos.

    Un MIDI d'orchestre change de tempo (ici : 62, 47, 54 bpm...). Ignorer ces
    changements donnerait un rubato faux et une duree fausse."""
    div = m["division"]
    tempos = sorted(m["tempos"])
    def conv(tick):
        sec, prev_t, prev_us = 0.0, 0, tempos[0][1]
        for t, us in tempos:
            if t >= tick:
                break
            sec += (t - prev_t) / div * (prev_us / 1e6)
            prev_t, prev_us = t, us
        sec += (tick - prev_t) / div * (prev_us / 1e6)
        return sec * hz
    return conv


def build(m, voices, hz, max_frames, skip_frames=0):
    conv = tick_to_frame(m, hz)

    # 1. chaque voie : quelle note sonne a chaque trame ?
    total = 0
    lines = []
    for v in voices:
        notes = []
        for ti in v.tracks:
            if ti < len(m["tracks"]):
                for n in m["tracks"][ti]:
                    f0 = int(conv(n.t)) - skip_frames
                    f1 = int(conv(n.t + n.dur)) - skip_frames
                    if f1 > 0 and f0 < max_frames:
                        notes.append((max(0, f0), min(max_frames, max(f0 + 1, f1)),
                                      n.pitch, n.vel))
        if notes:
            total = max(total, max(f1 for _, f1, _, _ in notes))
        lines.append(notes)
    total = min(total, max_frames)

    # 2. rang dans l'accord, trame par trame
    seq = []
    for v, notes in zip(voices, lines):
        cur = [None] * total
        for f in range(total):
            sounding = sorted((p for f0, f1, p, _ in notes if f0 <= f < f1),
                              reverse=True)
            cur[f] = sounding[v.rank] if len(sounding) > v.rank else None
        seq.append(cur)

    # 3. hauteurs -> registres, avec enveloppe logicielle par voie
    frames = []
    prev = {0: {}, 1: {}}
    age = [0] * 6
    last = [None] * 6
    for f in range(total):
        cur = {0: {}, 1: {}}
        mixer = {0: 0x3F, 1: 0x3F}
        for vi in range(len(voices)):
            v = voices[vi]
            ay, ch = (0, vi) if vi < 3 else (1, vi - 3)
            p = seq[vi][f]
            if p != last[vi]:
                age[vi] = 0          # nouvelle note -> l'enveloppe repart
                last[vi] = p
            else:
                age[vi] += 1
            if p is None:
                amp = 0
            else:
                idx = p - 12 + v.transpose      # MIDI 60 = do4 = notre index 48
                if idx < 0 or idx >= 96:
                    amp = 0
                else:
                    envf = render.ENVELOPPES.get(v.env, render.env_pincee)
                    amp = max(0, min(15, envf(age[vi], 40) + v.gain))
                    if amp:
                        per = render.period(idx)
                        cur[ay][ch * 2]     = per & 0xFF
                        cur[ay][ch * 2 + 1] = (per >> 8) & 0x0F
                        mixer[ay] &= ~(1 << ch)
            cur[ay][8 + ch] = amp
        for ay in (0, 1):
            cur[ay][7] = mixer[ay]
        d0 = {r: x for r, x in cur[0].items() if prev[0].get(r) != x}
        d1 = {r: x for r, x in cur[1].items() if prev[1].get(r) != x}
        prev[0].update(cur[0]); prev[1].update(cur[1])
        frames.append((d0, d1))
    return frames


# ===========================================================================
# ALLOCATION DYNAMIQUE
#
# La repartition figee « une piste = une voie » jetait 42 % des notes-trames
# de la Fee Dragee : le celesta monte a SEPT notes simultanees et on n'en
# gardait que trois, pendant que hautbois, cors et bassons etaient ignores.
#
# Ici, chaque AY recoit un GROUPE de pistes et choisit, a chaque trame, les
# trois notes a jouer parmi tout ce qui sonne dans son groupe. Deux regles
# musicales priment sur les poids :
#
#   - la note la PLUS HAUTE passe toujours : c'est la melodie ;
#   - la note la PLUS BASSE passe toujours : c'est ce qui donne l'assise.
#
# Le reste se departage aux poids. Et une note deja en cours GARDE sa voie :
# la reassigner en plein son produirait un clic et casserait l'enveloppe.
#
# Le decoupage en deux groupes est conserve -- chaque AY ayant sa propre sortie
# audio, c'est un placement stereo, pas un detail de cablage.
# ===========================================================================

class Group:
    """Un AY : ses pistes, leurs instruments et leurs poids."""
    def __init__(self, spec):
        self.entries = []            # (pistes, instrument, poids)
        for part in spec.split(","):
            p = (part.strip().split(":") + ["pluck", "5"])[:3]
            trks = [int(x) for x in p[0].split("+") if x]
            self.entries.append((trks, p[1], int(p[2])))

    def instruments(self):
        out = []
        for _, ins, _ in self.entries:
            if ins not in out:
                out.append(ins)
        return out


def group_notes(m, g, conv, max_frames, skip_frames):
    """-> liste de (trame_debut, trame_fin, hauteur, poids, instrument)."""
    out = []
    for trks, ins, w in g.entries:
        for ti in trks:
            if ti >= len(m["tracks"]):
                continue
            for n in m["tracks"][ti]:
                f0 = int(conv(n.t)) - skip_frames
                f1 = int(conv(n.t + n.dur)) - skip_frames
                if f1 > 0 and f0 < max_frames:
                    out.append((max(0, f0), min(max_frames, max(f0 + 1, f1)),
                                n.pitch, w, ins))
    return out


def allocate(notes, total, transpose=0, n=3):
    """Choisit, trame par trame, les `n` notes que jouent CES voies.
    -> voix[n][trame] = (hauteur, instrument, debut) ou None.

    `n` vaut 3 (une puce) partout sauf pour le mixage des deux mains d'un
    piano sur les 6 voies a la fois -- cf. build_auto(merge=True). Le corps
    de la fonction ne connait pas la difference : "la plus haute et la plus
    basse d'abord, le reste au poids" marche pareil sur 3 ou 6 voies.

    `debut` est l'IDENTITE de la note, et il est indispensable : sans lui, deux
    croches de meme hauteur qui se suivent sont indiscernables, on n'emet
    qu'une seule attaque, et la seconde est MUETTE -- l'enveloppe ayant deja
    fini de decliner. Le celesta de la Fee Dragee ne fait pratiquement que ca.
    Comparer les hauteurs ne suffit donc pas : il faut comparer les notes."""
    voix = [[None] * total for _ in range(n)]
    for f in range(total):
        sounding = {}
        for f0, f1, p, w, ins in notes:
            if f0 <= f < f1:
                idx = p - 12 + transpose
                if 0 <= idx < 96 and w > sounding.get(idx, (-1, None, 0))[0]:
                    sounding[idx] = (w, ins, f0)
        if not sounding:
            continue
        pitches = sorted(sounding)
        # la plus haute et la plus basse d'abord, le reste au poids
        chosen = [pitches[-1]]
        if len(pitches) > 1:
            chosen.append(pitches[0])
        rest = sorted((p for p in pitches if p not in chosen),
                      key=lambda p: (-sounding[p][0], -p))
        chosen += rest[:n - len(chosen)]

        # continuite : une note deja en cours garde sa voie
        free = [v for v in range(n)]
        placed = {}
        for p in chosen:
            for v in list(free):
                prev = voix[v][f - 1] if f > 0 else None
                if prev and prev[0] == p and prev[2] == sounding[p][2]:
                    placed[p] = v; free.remove(v); break
        for p in chosen:
            if p not in placed and free:
                placed[p] = free.pop(0)
        for p, v in placed.items():
            voix[v][f] = (p, sounding[p][1], sounding[p][2])
    return voix


def build_events(m, voices, hz, max_frames, skip_frames=0):
    """Meme reduction que build(), mais en EVENEMENTS : on ne sort plus des
    registres, on sort des notes. Les enveloppes ne sont plus epelees trame par
    trame -- le lecteur les fabrique a partir de l'instrument de la voie."""
    conv = tick_to_frame(m, hz)

    lines = []
    total = 0
    for v in voices:
        notes = []
        for ti in v.tracks:
            if ti < len(m["tracks"]):
                for n in m["tracks"][ti]:
                    f0 = int(conv(n.t)) - skip_frames
                    f1 = int(conv(n.t + n.dur)) - skip_frames
                    if f1 > 0 and f0 < max_frames:
                        notes.append((max(0, f0), min(max_frames, max(f0 + 1, f1)),
                                      n.pitch))
        if notes:
            total = max(total, max(f1 for _, f1, _ in notes))
        lines.append(notes)
    total = min(total, max_frames)

    # Ce qui sonne sur chaque voie, trame par trame (meme regle de rang).
    seq = []
    for v, notes in zip(voices, lines):
        cur = [None] * total
        for f in range(total):
            sounding = sorted((p for f0, f1, p in notes if f0 <= f < f1),
                              reverse=True)
            if len(sounding) > v.rank:
                idx = sounding[v.rank] - 12 + v.transpose
                cur[f] = idx if 0 <= idx < 96 else None
        seq.append(cur)

    # Instruments : un par voie, dedoublonnes.
    used = []
    vinstr = []
    for v in voices:
        name = v.env if v.env in a2m.INSTRUMENTS else "pluck"
        if name not in used:
            used.append(name)
        vinstr.append(used.index(name))

    events = [("instr", i, vinstr[i]) for i in range(len(voices))]
    events.append(("loop",))

    last = [None] * len(voices)
    pending = 0
    for f in range(total):
        changes = []
        for vi in range(len(voices)):
            p = seq[vi][f]
            if p != last[vi]:
                if p is None:
                    changes.append(("off", vi))
                else:
                    changes.append(("on", vi, p))
                last[vi] = p
        if changes:
            if pending:
                events.append(("wait", pending)); pending = 0
            events.extend(changes)
        pending += 1
    if pending:
        events.append(("wait", pending))
    return events, used, total


def build_auto(m, g1, g2, hz, max_frames, skip_frames, tr1, tr2, merge=False):
    """Allocation dynamique sur les deux AY -> evenements du profil T.

    `merge` : au lieu de deux bassins INDEPENDANTS de 3 voix (g1 sur l'AY #1,
    g2 sur l'AY #2 -- un placement STEREO, chaque groupe plafonne a 3 notes
    meme si l'autre groupe est silencieux au meme instant), un seul bassin de
    6 voix ou n'importe quelle note de n'importe quel groupe peut en prendre
    une. Utile pour un piano a deux mains, dont la polyphonie totale depasse
    ce qu'une main plafonnee a 3 laisse passer -- au prix de la separation
    stereo main gauche/droite, qui disparait : une note peut atterrir sur
    n'importe laquelle des 6 voies selon ce qui est libre au moment ou elle
    commence. Le transpose de chaque groupe est applique AVANT la fusion, ici,
    puisqu'allocate() n'en accepte plus qu'un seul pour l'appel fusionne."""
    conv = tick_to_frame(m, hz)
    n1 = group_notes(m, g1, conv, max_frames, skip_frames)
    n2 = group_notes(m, g2, conv, max_frames, skip_frames)
    total = 0
    for lst in (n1, n2):
        if lst:
            total = max(total, max(f1 for _, f1, _, _, _ in lst))
    total = min(total, max_frames)

    if merge:
        n1 = [(f0, f1, p + tr1, w, ins) for f0, f1, p, w, ins in n1]
        n2 = [(f0, f1, p + tr2, w, ins) for f0, f1, p, w, ins in n2]
        seq = allocate(n1 + n2, total, 0, n=6)
    else:
        v1 = allocate(n1, total, tr1)
        v2 = allocate(n2, total, tr2)
        seq = v1 + v2

    instrs = []
    for g in (g1, g2):
        for name in g.instruments():
            if name not in instrs:
                instrs.append(name)

    events = [("loop",)]
    last = [None] * 6
    last_ins = [None] * 6
    pending = 0
    kept = dropped = 0
    for f in range(total):
        changes = []
        for vi in range(6):
            cur = seq[vi][f]
            prev = last[vi]
            if cur != prev:
                if cur is None:
                    changes.append(("off", vi))
                else:
                    ins = instrs.index(cur[1])
                    if last_ins[vi] != ins:
                        changes.append(("instr", vi, ins)); last_ins[vi] = ins
                    changes.append(("on", vi, cur[0]))
                last[vi] = cur
        if changes:
            if pending:
                events.append(("wait", pending)); pending = 0
            events.extend(changes)
        pending += 1
    if pending:
        events.append(("wait", pending))
    return events, instrs, total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("--title", default=""); ap.add_argument("--author", default="")
    ap.add_argument("--hz", type=int, default=50)
    ap.add_argument("--seconds", type=float, default=60.0)
    ap.add_argument("--start", type=float, default=0.0)
    ap.add_argument("--profile", default="R", choices=("R", "T"))
    ap.add_argument("--ay1", help="groupe de l'AY #1 : pistes:instrument:poids, ...")
    ap.add_argument("--ay2", help="groupe de l'AY #2")
    ap.add_argument("--transpose1", type=int, default=0)
    ap.add_argument("--transpose2", type=int, default=0)
    ap.add_argument("--merge", action="store_true",
                    help="fusionne ay1+ay2 en UN bassin de 6 voix (perd le placement stereo, "
                         "gagne en notes captees si une main sature pendant que l'autre est calme)")
    ap.add_argument("--voices", required=False,
                    help="six specifications 'pistes:rang:env:transpo:gain' separees par des virgules")
    args = ap.parse_args()

    m = midi.read(args.source)
    maxf, skipf = int(args.seconds * args.hz), int(args.start * args.hz)

    if args.ay1 and args.ay2:
        events, instrs, nframes = build_auto(
            m, Group(args.ay1), Group(args.ay2), args.hz, maxf, skipf,
            args.transpose1, args.transpose2, merge=args.merge)
        blob = a2m.encode_t(events, instrs, title=args.title, author=args.author,
                            hz=args.hz, looping=True, n_ay=2, n_frames=nframes)
        open(args.output, "wb").write(blob)
        secs = nframes / float(args.hz)
        n_notes = sum(1 for e in events if e[0] == "on")
        print("%-14s %6d o  %5d trames  %5.1f s  %4.0f o/s  %s" % (
            os.path.basename(args.output), len(blob), nframes, secs,
            (len(blob) - a2m.HEADER_SIZE) / max(secs, .001), args.title))
        print("   %d notes, %d instruments (allocation dynamique)" % (n_notes, len(instrs)))
        return blob

    voices = []
    for spec in args.voices.split(","):
        parts = (spec.split(":") + ["0", "pluck", "0", "0"])[:5]
        trks = [int(x) for x in parts[0].split("+") if x != ""]
        voices.append(Voice(trks, int(parts[1]), parts[2], int(parts[3]), int(parts[4])))

    if args.profile == "T":
        events, instrs, nframes = build_events(m, voices, args.hz, maxf, skipf)
        blob = a2m.encode_t(events, instrs, title=args.title, author=args.author,
                            hz=args.hz, looping=True, n_ay=2, n_frames=nframes)
        n_notes = sum(1 for e in events if e[0] == "on")
        print("   %d notes, %d instruments" % (n_notes, len(instrs)))
    else:
        frames = build(m, voices, args.hz, maxf, skipf)
        nframes = len(frames)
        blob = a2m.encode(frames, title=args.title, author=args.author,
                          hz=args.hz, looping=True, n_ay=2)
    open(args.output, "wb").write(blob)

    secs = nframes / float(args.hz)
    print("%-14s %6d o  %5d trames  %5.1f s  %4.0f o/s  %s" % (
        os.path.basename(args.output), len(blob), nframes, secs,
        (len(blob) - a2m.HEADER_SIZE) / max(secs, .001), args.title))
    return blob


if __name__ == "__main__":
    main()
