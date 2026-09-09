"""pt3_fixtures.py -- construit des fichiers .pt3 minimaux, a la main, a
partir du meme document de reference que tools/a2mconv/pt3.py
(README_pt3.txt de Vince Weaver). C'est le point (1) de spec.md §5.7 :
« fixtures maison » -- zero question de droit, puisqu'on les ecrit
nous-memes, mais AUTO-COHERENT avec le lecteur qu'elles testent, pas
verifie contre un vrai fichier PT3 ni un lecteur tiers.

Sert de bibliotheque a test/pt3_test.py : n'ecrit rien sur disque seul.
"""

import struct

HEADER_LEN = 0xC9


def _pad(s, n):
    b = s.encode("latin-1")[:n]
    return b + b"\x00" * (n - len(b))


# --- constructeurs d'octets d'evenement, un par opcode documente ----------
#
# ORDRE DANS UNE LIGNE (assume, pas confirme par un fichier reel -- cf.
# docstring de tools/a2mconv/pt3.py) : les commandes de prefixe (ornement,
# enveloppe, BRUIT, VOLUME, choix d'echantillon) d'abord, dans n'importe
# quel ordre entre elles ; puis le declencheur (note / $C0 / $D0) qui
# cloture la ligne ; puis au plus un effet. Les helpers ci-dessous doivent
# donc s'ecrire `volume(v) + note(idx)`, jamais l'inverse.
def note(idx):       return bytes([0x50 + idx])
def note_off():       return bytes([0xC0])
def end_row():         return bytes([0xD0])
def volume(v):          return bytes([0xC0 | v])          # v : 1..15
def ornament(o):         return bytes([0x40 | o])          # o : 0..15
def ornament_reset():     return bytes([0xB0])
def envelope(etype, period):
    return bytes([0xB0 | (etype + 1)]) + struct.pack(">H", period)
def skip(n_lines):
    return bytes([0xB1, n_lines])
def effect_glissando(delay, add):
    return bytes([0x01, delay]) + struct.pack("<h", add)
def effect_set_speed(new_speed):
    return bytes([0x09, new_speed])


def build(name, author, order, pattern_streams, samples=None, ornaments=None,
          speed=3, freq_table=1, lpos=0, version=ord('6')):
    """pattern_streams : {pat_idx: {'A': bytes, 'B': bytes, 'C': bytes}} --
    chaque flux de voie DEJA construit avec les helpers ci-dessus, terminant
    sa derniere ligne par `end_row()`/une note/etc (PAS de $00 final : il
    est ajoute ici, comme le ferait le vrai format).

    samples / ornaments : {idx: (loop, [valeurs])} -- valeurs = tuples de 4
    octets pour un echantillon, entiers signes pour un ornement.
    """
    order_bytes = bytes(p * 3 for p in order) + b"\xFF"
    pat_indices = sorted(pattern_streams)
    n_slots = (max(pat_indices) + 1) if pat_indices else 0

    pos = HEADER_LEN + len(order_bytes) + n_slots * 6
    body = bytearray()
    chan_addr = {}
    for idx in pat_indices:
        for c in "ABC":
            chan_addr[(idx, c)] = pos
            data = pattern_streams[idx][c] + b"\x00"
            body += data
            pos += len(data)

    sam_addr = {}
    samples = samples or {}
    for idx, (loop, vals) in samples.items():
        sam_addr[idx] = pos
        body.append(loop); body.append(len(vals)); pos += 2
        for v in vals:
            body += bytes(v); pos += 4

    orn_addr = {}
    ornaments = ornaments or {}
    for idx, (loop, vals) in ornaments.items():
        orn_addr[idx] = pos
        body.append(loop); body.append(len(vals)); pos += 2
        for v in vals:
            body.append(v & 0xFF); pos += 1

    head = bytearray(HEADER_LEN)
    head[0:13] = b"ProTracker 3."
    head[0x0D] = version
    head[0x0E:0x1E] = _pad(" compilation of ", 16)
    head[0x1E:0x3E] = _pad(name, 32)
    head[0x3E:0x42] = _pad(" by ", 4)
    head[0x42:0x62] = _pad(author, 32)
    head[0x63] = freq_table
    head[0x64] = speed
    head[0x65] = (max(pat_indices) + 2) if pat_indices else 1
    head[0x66] = lpos
    struct.pack_into("<H", head, 0x67, HEADER_LEN + len(order_bytes))
    for i in range(32):
        struct.pack_into("<H", head, 0x69 + i * 2, sam_addr.get(i, 0))
    for i in range(16):
        struct.pack_into("<H", head, 0xA9 + i * 2, orn_addr.get(i, 0))

    pat_table = bytearray(n_slots * 6)
    for idx in pat_indices:
        struct.pack_into("<H", pat_table, idx * 6 + 0, chan_addr[(idx, "A")])
        struct.pack_into("<H", pat_table, idx * 6 + 2, chan_addr[(idx, "B")])
        struct.pack_into("<H", pat_table, idx * 6 + 4, chan_addr[(idx, "C")])

    return bytes(head) + order_bytes + bytes(pat_table) + bytes(body)


# --- fixtures concretes ----------------------------------------------------

def scale():
    """Une voie joue une gamme montante avec volume decroissant ; les deux
    autres restent muettes (aucune note). Teste : conteneur, une seule
    voie active, note+volume par ligne, fin de motif propre."""
    a = b"".join(volume(15 - i) + note(24 + i) for i in range(8))
    b = end_row() * 8
    c = end_row() * 8
    return build("GAMME", "TEST", order=[0],
                  pattern_streams={0: {"A": a, "B": b, "C": c}})


def arpeggio():
    """Voie A : une seule note tenue huit lignes, ornement 1 = arpege
    majeur (0, +4, +7 demi-tons). Teste : ornement, arpege trame par
    trame (pas seulement ligne par ligne)."""
    a = ornament(1) + volume(15) + note(36) + end_row() * 7
    b = end_row() * 8
    c = end_row() * 8
    orn = {1: (0, [0, 4, 7])}
    return build("ARPEGE", "TEST", order=[0],
                  pattern_streams={0: {"A": a, "B": b, "C": c}},
                  ornaments=orn)


def skip_lines():
    """Une note suivie d'un saut de 3 lignes : la note doit rester tenue
    (meme hauteur/volume) sur les lignes sautees. Teste $B1."""
    a = skip(3) + volume(10) + note(40) + end_row()
    b = end_row() * 5
    c = end_row() * 5
    return build("SAUT", "TEST", order=[0],
                  pattern_streams={0: {"A": a, "B": b, "C": c}})


def envelope_trigger():
    """Une note avec enveloppe declenchee explicitement. Teste que r11-r13
    ne sortent QUE sur la ligne du declenchement, pas a chaque trame."""
    a = envelope(8, 0x1234) + volume(15) + note(30) + end_row() * 3
    b = end_row() * 4
    c = end_row() * 4
    return build("ENVELOPPE", "TEST", order=[0],
                  pattern_streams={0: {"A": a, "B": b, "C": c}})


def effect_skipped():
    """Une note portant un effet (glissando) : l'effet doit etre consomme
    sans desynchroniser la ligne suivante. Teste la phase 3 du parseur."""
    a = (volume(15) + note(24) + effect_glissando(1, 100)
         + volume(15) + note(28))
    b = end_row() * 2
    c = end_row() * 2
    return build("EFFET", "TEST", order=[0],
                  pattern_streams={0: {"A": a, "B": b, "C": c}})


def two_patterns_loop():
    """Deux motifs joues dans l'ordre 0, 1, avec bouclage sur le motif 1
    (LPosPtr = 1). Teste `order`, la table de motifs a plusieurs entrees,
    et le calcul de `loop_frame`."""
    a0 = volume(15) + note(24) + end_row() * 3
    a1 = volume(15) + note(31) + end_row() * 3
    b0 = end_row() * 4
    b1 = end_row() * 4
    c0 = end_row() * 4
    c1 = end_row() * 4
    return build("DEUX MOTIFS", "TEST", order=[0, 1], lpos=1,
                  pattern_streams={0: {"A": a0, "B": b0, "C": c0},
                                    1: {"A": a1, "B": b1, "C": c1}})
