"""midi.py -- lecteur de fichier MIDI standard, sans dependance.

On ne garde que ce qui sert a faire de la musique sur un AY : les notes, leur
instant, leur duree, leur velocite, et le tempo. Ni controleurs, ni pitch bend,
ni percussions -- l'AY n'a que trois voies par puce, il faudra deja choisir.
"""

import struct


def _vlq(b, i):
    v = 0
    while True:
        c = b[i]; i += 1
        v = (v << 7) | (c & 0x7F)
        if not c & 0x80:
            return v, i


class Note:
    __slots__ = ("t", "dur", "pitch", "vel", "track", "chan")
    def __init__(self, t, dur, pitch, vel, track, chan):
        self.t, self.dur, self.pitch, self.vel = t, dur, pitch, vel
        self.track, self.chan = track, chan
    def __repr__(self):
        return "Note(t=%d dur=%d p=%d)" % (self.t, self.dur, self.pitch)


def read(path):
    b = open(path, "rb").read()
    assert b[0:4] == b"MThd", "pas un MIDI"
    fmt, ntrk, div = struct.unpack(">HHH", b[8:14])
    assert div & 0x8000 == 0, "division SMPTE non geree"
    i = 14
    tracks, tempos = [], []                # (tick, microsecondes par noire)
    names = {}
    for tk in range(ntrk):
        assert b[i:i+4] == b"MTrk", "piste attendue"
        ln = struct.unpack(">I", b[i+4:i+8])[0]
        end = i + 8 + ln
        j = i + 8
        t = 0
        running = None
        opened = {}
        notes = []
        while j < end:
            dt, j = _vlq(b, j)
            t += dt
            st = b[j]
            if st & 0x80:
                running = st; j += 1
            else:
                st = running
            if st == 0xFF:
                mt = b[j]; j += 1
                ln2, j = _vlq(b, j)
                data = b[j:j+ln2]; j += ln2
                if mt == 0x51:
                    tempos.append((t, (data[0] << 16) | (data[1] << 8) | data[2]))
                elif mt in (0x03, 0x04):
                    names[tk] = data.decode("latin-1", "replace").strip()
            elif st in (0xF0, 0xF7):
                ln2, j = _vlq(b, j); j += ln2
            else:
                hi, chan = st & 0xF0, st & 0x0F
                if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                    d1, d2 = b[j], b[j+1]; j += 2
                    if hi == 0x90 and d2 > 0:
                        opened.setdefault((chan, d1), []).append((t, d2))
                    elif hi == 0x80 or (hi == 0x90 and d2 == 0):
                        st_list = opened.get((chan, d1))
                        if st_list:
                            t0, vel = st_list.pop(0)
                            notes.append(Note(t0, t - t0, d1, vel, tk, chan))
                else:
                    j += 1                 # program change / aftertouch canal
        notes.sort(key=lambda n: (n.t, -n.pitch))
        tracks.append(notes)
        i = end
    # Tri par TICK SEUL, stable : deux « Set Tempo » sur le meme tick ne sont
    # pas ambigus dans un vrai MIDI, l'ORDRE dans le fichier tranche (le
    # second efface le premier). Trier par (tick, microsecondes) -- comme
    # avant -- les aurait a la place classes par VALEUR, ce qui pouvait faire
    # gagner le mauvais des deux : le tempo par defaut ci-dessous, ajoute
    # inconditionnellement au tick 0, entrait alors en concurrence avec un
    # tempo REELLEMENT declare au meme tick, et le plus lent des deux gagnait
    # au hasard de la comparaison numerique -- jamais a l'oreille, mais un
    # Debussy marque a 144 pouvait ainsi se retrouver joue a 120 sans qu'aucun
    # message ne le signale. `sort()` de Python est stable : ceci ne rejoue
    # pas ce piege, l'ORDRE D'ARRIVEE des evenements reels est preserve.
    tempos.sort(key=lambda x: x[0])
    if not tempos or tempos[0][0] != 0:
        tempos.insert(0, (0, 500000))      # aucun tempo declare au debut -- repli 120 bpm
    return {"division": div, "tracks": tracks, "tempos": tempos, "names": names}
