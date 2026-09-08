"""render.py -- une partition lisible -> des trames de registres AY.

Le format de partition est fait pour etre CORRIGE A LA MAIN. Une fausse note
se repare dans un fichier texte et un `make music`, sans toucher au code :

    title: FEE DRAGEE
    author: TCHAIKOVSKY
    tempo: 108            # noires par minute
    hz: 50
    loop: yes
    voice A: pluck        # pluck | bass | sustain | soft | major | ...
    voice C: bass

    A: e5/8 d#5/8 r/8 e5/8
    B: g4/4 .
    C: e3/4 b3/4

Une note : <nom><alteration><octave>/<denominateur>[.]  --  e5/8, d#5/16,
bb4/4, e5/4. (pointee). Silence : r/8. Le point d'orgue « . » seul prolonge.

L'AY n'a qu'UNE enveloppe materielle par puce, partagee par les trois voies :
elle ne peut donc pas donner a chaque voix son propre declin. On fabrique donc
les enveloppes EN LOGICIEL, en ecrivant l'amplitude de chaque voie a chaque
trame. C'est exactement ce qu'un flux de registres sait faire de mieux, et ca
ne coute rien au lecteur 6502 -- il recopie des octets, il ne calcule pas.
"""

import math, re

# Un commentaire commence par '#' EN DEBUT DE LIGNE, ou precede d'une espace.
# Sans cette nuance, 'd#5/16' (re dièse) serait tronque en 'd' : le diese et le
# commentaire partagent le meme caractere. C'est le genre de collision qui ne
# se voit qu'a l'execution, sur une partition qui module.
_COMMENT_RE = re.compile(r'(^|\s)#.*$')

VIA_CLOCK = 1020500.0

# --- notes ----------------------------------------------------------------
_STEP = {'c':0,'d':2,'e':4,'f':5,'g':7,'a':9,'b':11}
_NOTE_RE = re.compile(r'^([a-gA-G])([#b]?)(-?\d)$')


def note_index(name):
    """'e5' -> index absolu, 0 = do0, 57 = la4 (440 Hz)."""
    m = _NOTE_RE.match(name)
    if not m:
        raise ValueError("note illisible : %r" % name)
    step, alt, octv = m.group(1).lower(), m.group(2), int(m.group(3))
    n = _STEP[step] + (1 if alt == '#' else -1 if alt == 'b' else 0)
    return octv * 12 + n


def period(idx):
    """Periode AY pour l'horloge Mockingboard. 12 bits : 1..4095."""
    f = 440.0 * 2.0 ** ((idx - 57) / 12.0)
    return max(1, min(4095, int(round(VIA_CLOCK / (16.0 * f)))))


def cents_error(idx):
    """Ecart entre la note voulue et celle que l'AY sait produire."""
    f = 440.0 * 2.0 ** ((idx - 57) / 12.0)
    return 1200.0 * math.log2((VIA_CLOCK / (16.0 * period(idx))) / f)


# --- enveloppes logicielles ------------------------------------------------
# amp(t) pour t = 0..duree-1, en trames. Amplitude AY : 0..15, echelle
# LOGARITHMIQUE (chaque pas vaut ~1,5 dB), d'ou des decroissances lineaires en
# NUMERO de pas plutot qu'en amplitude.

def env_pluck(t, dur):
    """Celesta / pizzicato : attaque immediate, declin regulier. La note meurt
    avant la fin de sa duree si elle est longue -- c'est ce qui donne le grain
    « boite a musique » plutot qu'un orgue."""
    a = 15 - int(t * 1.05)
    return max(0, a)


def env_sustain(t, dur):
    """Tenue, avec une courte chute finale pour detacher la note suivante."""
    if t >= dur - 1:
        return 0
    if t < 2:
        return 11 + t * 2
    return max(0, 14 - t // 12)


def env_soft(t, dur):
    """Attaque progressive : cordes, nappes."""
    if t >= dur - 1:
        return 0
    return max(0, min(13, 3 + t))


def env_bass(t, dur):
    """Basse pizzicato : court et sec, quelle que soit la duree ecrite."""
    a = 14 - int(t * 1.6)
    return max(0, a)


ENVELOPPES = {'pluck': env_pluck, 'sustain': env_sustain,
              'soft': env_soft, 'bass': env_bass}


# --- lecture de la partition ----------------------------------------------
class Score:
    def __init__(self):
        self.titre = ""; self.auteur = ""
        self.tempo = 120; self.hz = 50; self.boucle = True
        self.env = {'A': 'pluck', 'B': 'pluck', 'C': 'bass'}
        self.voix = {'A': [], 'B': [], 'C': []}   # (index|None, duree_frames)

    @staticmethod
    def load(path):
        s = Score()
        raw = {}
        for line in open(path, encoding='utf-8'):
            line = _COMMENT_RE.sub('', line).strip()
            if not line:
                continue
            if ':' not in line:
                raise ValueError("ligne sans ':' -> %r" % line)
            key, val = line.split(':', 1)
            key = key.strip().lower(); val = val.strip()
            if key == 'title':    s.titre = val
            elif key == 'author': s.auteur = val
            elif key == 'tempo':  s.tempo = int(val)
            elif key == 'hz':     s.hz = int(val)
            elif key == 'loop':   s.boucle = val.lower() in ('yes', 'oui', '1')
            elif key.startswith('voice '):
                s.env[key.split()[1].upper()] = val
            elif key.upper() in ('A', 'B', 'C'):
                raw.setdefault(key.upper(), []).append(val)
            else:
                raise ValueError("clef inconnue : %r" % key)

        # Duree d'une ronde, en trames : 4 noires au tempo donne.
        whole = 4.0 * 60.0 / s.tempo * s.hz
        for v, chunks in raw.items():
            # On accumule la POSITION en flottant et on arrondit chaque
            # frontiere, au lieu d'arrondir chaque duree separement.
            #
            # C'est tout sauf un detail. A 120 bpm et 50 Hz, une croche fait
            # 12,5 trames : arrondie a 12, elle perd une demi-trame A CHAQUE
            # FOIS. Une voix en croches derivait ainsi de 16 trames sur quatre
            # mesures pendant qu'une voix en noires restait juste -- les voix
            # se desynchronisaient et le rythme partait en morceaux.
            # En arrondissant les positions, l'erreur ne s'accumule plus :
            # elle reste bornee a une demi-trame.
            pos = 0.0
            for tok in " ".join(chunks).split():
                idx, dur = _token(tok, whole)
                start, end = int(round(pos)), int(round(pos + dur))
                s.voix[v].append((idx, max(1, end - start)))
                pos += dur
        return s


def _token(tok, whole):
    """-> (index de note ou None, duree en trames, NON arrondie).

    La duree reste flottante : c'est l'appelant qui arrondit les positions,
    pour que l'erreur ne s'accumule pas d'une note a l'autre."""
    name, _, dur = tok.partition('/')
    if not dur:
        raise ValueError("duree absente : %r" % tok)
    dotted = dur.endswith('.')
    if dotted:
        dur = dur[:-1]
    frames = whole / float(dur)
    if dotted:
        frames *= 1.5
    idx = None if name.lower() in ('r', '-') else note_index(name)
    return (idx, frames)


# --- rendu -----------------------------------------------------------------
def render(score):
    """-> (frames, notes_utilisees). frames = liste de dict {registre: valeur}."""
    # 1. dérouler chaque voix en amplitude + periode par trame
    total = max(sum(d for _, d in score.voix[v]) for v in 'ABC') or 1
    amp   = {v: [0] * total for v in 'ABC'}
    per   = {v: [0] * total for v in 'ABC'}
    used  = set()

    for vi, v in enumerate('ABC'):
        envf = ENVELOPPES.get(score.env.get(v, 'pluck'), env_pluck)
        t = 0
        for idx, dur in score.voix[v]:
            if idx is not None:
                used.add(idx)
                p = period(idx)
                for k in range(dur):
                    if t + k >= total:
                        break
                    amp[v][t + k] = envf(k, dur)
                    per[v][t + k] = p
            t += dur

    # 2. traduire en registres, en ne gardant que ce qui CHANGE
    frames = []
    prev = {}
    for t in range(total):
        cur = {}
        mixer = 0x3F                      # bits a 1 = COUPE
        for vi, v in enumerate('ABC'):
            p, a = per[v][t], amp[v][t]
            if a > 0 and p:
                cur[vi * 2]     = p & 0xFF
                cur[vi * 2 + 1] = (p >> 8) & 0x0F
                mixer &= ~(1 << vi)       # ouvre le TON de cette voie
            cur[8 + vi] = a
        cur[7] = mixer
        frames.append({r: val for r, val in cur.items() if prev.get(r) != val})
        prev.update(cur)

    return frames, used
