"""ym.py -- lecture des fichiers YM (dumps de registres YM2149, Atari ST).

Un YM n'est pas une partition : c'est l'ETAT des 14 registres du YM2149,
releve a chaque trame. C'est exactement ce que le profil R de A2M transporte,
et c'est pour ca que ce format-la existe -- il avale n'importe quelle source
sans rien supposer de la musique.

Trois choses le separent quand meme d'un module jouable sur Mockingboard :

  1. L'HORLOGE. Le YM2149 d'un Atari ST tourne a 2 000 000 Hz, l'AY d'une
     Mockingboard a 1 020 500. Rejoue tel quel, un YM sonne presque une octave
     trop grave. Les periodes de TON, de BRUIT et d'ENVELOPPE doivent etre
     reechelonnees ; les amplitudes et le mixer, non.

  2. LES BITS D'EFFET. En YM5/YM6, les bits hauts de r1, r3 et r6 ne sont pas
     de la musique : ils encodent digidrums, voix SID et sync-buzzer. Ecrits
     tels quels dans un AY, ils allongent des periodes au hasard. On les
     masque.

  3. r13 = 0xFF signifie « NE PAS ECRIRE ». Le registre 13 est un declencheur :
     y ecrire REARME l'enveloppe. Le format YM a donc besoin d'une valeur qui
     veut dire « laisse-la tranquille », et c'est 0xFF. La confondre avec une
     forme d'enveloppe reamorcerait l'enveloppe cinquante fois par seconde.

Les digidrums (echantillons joues par le registre de volume) ne sont PAS
reproductibles ici : ils demandent une ecriture d'amplitude a plusieurs
kilohertz, quand notre tick en fait 50. Ils sont signales, pas simules.
"""

import struct

APPLE_CLOCK = 1020500


def _decompress(path):
    """Un YM est presque toujours en archive LHA ; parfois non."""
    raw = open(path, "rb").read()
    if raw[:2] in (b"YM", b"MI"):          # deja en clair
        return raw
    try:
        import lhafile
    except ImportError:
        raise RuntimeError("fichier compresse LHA : pip install --user lhafile")
    a = lhafile.Lhafile(path)
    return a.read(a.infolist()[0].filename)


def _cstr(b, i):
    j = b.index(b"\0", i)
    return b[i:j].decode("latin-1", "replace"), j + 1


def read(path):
    d = _decompress(path)
    magic = d[:4]

    if magic in (b"YM5!", b"YM6!"):
        assert d[4:12] == b"LeOnArD!", "signature YM5/6 absente"
        (nframes, attrs) = struct.unpack(">II", d[12:20])
        ndigi = struct.unpack(">H", d[20:22])[0]
        clock = struct.unpack(">I", d[22:26])[0]
        hz    = struct.unpack(">H", d[26:28])[0]
        loop  = struct.unpack(">I", d[28:32])[0]
        extra = struct.unpack(">H", d[32:34])[0]
        off = 34 + extra
        for _ in range(ndigi):             # sauter les echantillons
            sz = struct.unpack(">I", d[off:off + 4])[0]
            off += 4 + sz
        name, off   = _cstr(d, off)
        author, off = _cstr(d, off)
        comment, off = _cstr(d, off)
        nreg = 16
        interleaved = bool(attrs & 1)

    elif magic in (b"YM3!", b"YM3b"):
        nframes = (len(d) - 4) // 14
        clock, hz, loop, ndigi = 2000000, 50, 0, 0
        name = author = comment = ""
        off, nreg, interleaved = 4, 14, True

    elif magic == b"YM2!":
        nframes = (len(d) - 4) // 14
        clock, hz, loop, ndigi = 2000000, 50, 0, 0
        name = author = comment = ""
        off, nreg, interleaved = 4, 14, True

    else:
        raise ValueError("format YM inconnu : %r" % magic)

    # --- lecture des registres -------------------------------------------
    body = d[off:]
    regs = [[0] * nframes for _ in range(nreg)]
    if interleaved:
        # entrelace PAR REGISTRE : toutes les trames de r0, puis de r1...
        for r in range(nreg):
            base = r * nframes
            row = body[base:base + nframes]
            for f in range(min(nframes, len(row))):
                regs[r][f] = row[f]
    else:
        for f in range(nframes):
            base = f * nreg
            for r in range(nreg):
                if base + r < len(body):
                    regs[r][f] = body[base + r]

    return {"frames": nframes, "clock": clock, "hz": hz or 50, "loop": loop,
            "digidrums": ndigi, "name": name, "author": author,
            "comment": comment, "regs": regs, "format": magic.decode("latin-1")}


def to_apple(ym, max_frames=None):
    """-> liste de trames {registre: valeur}, ne gardant que ce qui CHANGE.

    C'est ici que le YM devient jouable : masquage des bits d'effet,
    reechelonnement des periodes, et respect du 0xFF de r13."""
    n = ym["frames"] if max_frames is None else min(ym["frames"], max_frames)
    R = ym["regs"]
    ratio = APPLE_CLOCK / float(ym["clock"])

    out = []
    prev = {}
    for f in range(n):
        cur = {}

        # --- periodes de ton : 12 bits, reechelonnees -------------------
        for ch in range(3):
            fine = R[ch * 2][f]
            # bits 4-7 de r1 et r3 portent des donnees d'effet en YM5/6
            coarse = R[ch * 2 + 1][f] & 0x0F
            per = ((coarse << 8) | fine)
            per = int(round(per * ratio))
            per = max(1, min(4095, per)) if per else 0
            cur[ch * 2]     = per & 0xFF
            cur[ch * 2 + 1] = (per >> 8) & 0x0F

        # --- bruit : 5 bits, reechelonne aussi --------------------------
        np = R[6][f] & 0x1F
        cur[6] = max(1, min(31, int(round(np * ratio)))) if np else 0

        # --- mixer : 6 bits utiles ; les bits 6-7 sont la direction des
        # ports d'E/S du YM, qui n'ont rien a faire dans un mixer ---------
        cur[7] = R[7][f] & 0x3F

        # --- amplitudes : PAS de reechelonnement (ce n'est pas un temps) --
        for ch in range(3):
            cur[8 + ch] = R[8 + ch][f] & 0x1F

        # --- enveloppe : periode 16 bits, reechelonnee -------------------
        ep = (R[12][f] << 8) | R[11][f]
        ep = int(round(ep * ratio))
        ep = max(1, min(65535, ep)) if ep else 0
        cur[11] = ep & 0xFF
        cur[12] = (ep >> 8) & 0xFF

        # --- r13 : 0xFF = ne pas ecrire ---------------------------------
        shape = R[13][f]
        if shape != 0xFF:
            cur[13] = shape & 0x0F

        # ne garder que les changements ; r13 passe TOUJOURS quand il est la,
        # puisque le reecrire est precisement l'evenement.
        delta = {r: v for r, v in cur.items() if r == 13 or prev.get(r) != v}
        prev.update(cur)
        out.append(delta)
    return out
