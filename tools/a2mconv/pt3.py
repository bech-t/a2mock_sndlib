"""pt3.py -- lecture des modules PT3 (ProTracker 3 / Vortex Tracker II, ZX
Spectrum) -> flux de registres profil R.

Un PT3 n'est PAS un dump de registres comme un YM : c'est une partition
compressee (notes, ornements, echantillons, effets) pensee pour etre
INTERPRETEE par un lecteur. Contrairement au YM (§ym.py), on ne peut pas se
contenter de rejouer des octets ; il faut reproduire ce que ferait un lecteur
PT3 reel, frame par frame, pour obtenir un flux de registres exploitable par
le profil R -- exactement l'"oracle" de spec.md §5.6/§5.7.

CE QUE CETTE VERSION FAIT (etape 1+2 de spec.md §5.7) :
  - lit le conteneur en entier : en-tete, liste de motifs, tables
    d'echantillons et d'ornements, flux d'evenements par voie ;
  - interprete les notes, le volume et l'ARPEGE D'ORNEMENT (l'ornement
    ajoute un decalage de pas de table a la hauteur, trame par trame --
    le meme principe que les instruments "major"/"minor" de a2m.py) ;
  - reechelonne l'horloge ZX -> Apple, exactement comme ym.py.

CE QUE CETTE VERSION NE FAIT PAS ENCORE (etape 3, deliberement differee) :
  - les EFFETS (glissando, portamento, vibrato, offset d'echantillon,
    changement de vitesse...) : les octets sont correctement consommes,
    pour ne pas desynchroniser le flux, mais leur action n'est PAS
    appliquee. Chaque ligne concernee est comptee et signalee dans
    `--rapport`, jamais silencieuse ;
  - le TABLEAU D'ECHANTILLONS (enveloppe d'amplitude/glissement de ton par
    instrument) : seule la hauteur de base et le volume passent. Un module
    dont l'identite sonore repose sur ses echantillons (percussions,
    attaques filees) sonnera plat ;
  - le BRUIT : par prudence, aucune voie n'est routee sur le bruit en v1 --
    le bit de mixer qui en decide est sous-specifie meme dans la doc source
    (cf. reference ci-dessous, "Bit 4 - ... ?"). Un module percussif perdra
    donc sa caisse claire.

Rien de tout ca n'est mesure sur un fichier PT3 reel : aucun corpus sous
licence claire n'a ete trouve (spec.md §5.7). Les seuls PT3 exerces ici sont
les fixtures maison de test/pt3_fixtures.py, ecrites (pas capturees) a partir
du meme document de reference -- donc AUTO-COHERENT, pas VERIFIE contre un
lecteur tiers. A traiter comme *plausible*, jusqu'a preuve du contraire.

Reference du format : README_pt3.txt de Vince Weaver (deater.net), qui
documente lui-meme des zones d'ombre ("?", "corner cases seen in the wild").
"""

import math
import struct

ZX_CLOCK    = 1773400.0
APPLE_CLOCK = 1020500.0

# --- notes ------------------------------------------------------------
# idx 0..95 = C-1..B-8 dans la numerotation PT3 (octet de note $50+idx).
# Ancrage : idx 45 = la-4 = 440 Hz -- convention de concert usuelle. Les
# vraies tables PT3 (par version) different legerement de ce calcul
# theorique par des arrondis d'epoque ; ecart non mesure ici (§5.7).

def _period_zx(idx):
    f = 440.0 * 2.0 ** ((idx - 45) / 12.0)
    return max(1, min(4095, int(round(ZX_CLOCK / (16.0 * f)))))


def _period_apple(idx):
    per = _period_zx(idx)
    per = int(round(per * (APPLE_CLOCK / ZX_CLOCK)))
    return max(1, min(4095, per))


def cents_error(idx):
    """Ecart, en cents, entre la note theorique et celle que la periode
    entiere obtenue sur Mockingboard reproduit reellement -- cf. render.py,
    meme calcul, horloge Apple directement (le rapport ZX->Apple s'annule)."""
    f = 440.0 * 2.0 ** ((idx - 45) / 12.0)
    per = _period_apple(idx)
    f_reelle = APPLE_CLOCK / (16.0 * per)
    return 1200.0 * math.log2(f_reelle / f)


def _cstr16(d, off):
    return struct.unpack_from("<H", d, off)[0]


# --- opcodes du flux d'evenements par voie -----------------------------
# cf. README_pt3.txt, section "Pattern data". Les plages sont EXCLUSIVES
# entre elles ; un octet appartient a une seule.

EFFECT_NAMES = {
    0x01: "glissando/tone",
    0x02: "portamento",
    0x03: "sample-offset",
    0x04: "ornament-offset",
    0x05: "vibrato",
    0x08: "envelope-glissando",
    0x09: "set-speed",
}


def _parse_channel_stream(d, start):
    """Decode le flux d'UNE voie -> liste d'"evenements de ligne".

    Une ligne PT3 s'ecrit en TROIS PHASES successives (README_pt3.txt,
    "Note when parsing if you reach a note, a $D0 or a $C0 then you are
    done parsing the note ... move on to parsing the effects") :

      1. zero ou plusieurs commandes de PREFIXE -- enveloppe ($10-$1F,
         $B0-$BF), bruit ($20-$3F), ornement ($40-$4F), choix d'echantillon
         ($D1-$EF, $F0-$FF) -- qui s'accumulent dans le MEME evenement ;
      2. exactement un DECLENCHEUR qui cloture la ligne : une note
         ($50-$AF), $C0 (coupure) ou $D0 (rien de plus) ;
      3. au plus un EFFET ($01-$0F, avec ses parametres), immediatement
         apres le declencheur -- jamais avant.

    `$B1` (sauter N lignes), une commande de prefixe comme les autres,
    etire l'evenement qui vient d'etre construit sur les N lignes
    suivantes, representees par des evenements vides (aucun changement).

    Retourne (evenements, n_effets_ignores)."""
    rows = []
    n_effects = 0
    p = start
    n = len(d)
    while p < n and d[p] != 0x00:
        ev = {}
        skip = 0

        # -- phase 1 : prefixe(s), jusqu'au declencheur de note --------------
        while True:
            b = d[p]
            if 0x10 <= b <= 0x1F:
                if b == 0x10:
                    ev["envelope"] = None          # desactivee
                    p += 1
                    ev["sample"] = d[p]; p += 1
                else:
                    etype = b & 0x0F
                    period = (d[p + 1] << 8) | d[p + 2]      # grand-boutien
                    ev["envelope"] = (etype, period)
                    ev["sample"] = d[p + 4]
                    p += 5
            elif 0x20 <= b <= 0x3F:
                ev["noise"] = b - 0x20
                p += 1
            elif 0x40 <= b <= 0x4F:
                ev["ornament"] = b & 0x0F
                p += 1
            elif b == 0xB0:
                ev["envelope"] = None
                ev["ornament_reset"] = True
                p += 1
            elif b == 0xB1:
                skip = d[p + 1]
                p += 2
            elif 0xB2 <= b <= 0xBF:
                etype = (b & 0x0F) - 1
                period = (d[p + 1] << 8) | d[p + 2]
                ev["envelope"] = (etype, period)
                p += 3
            elif 0xD1 <= b <= 0xEF:
                ev["sample"] = b - 0xD0
                p += 1
            elif 0xF0 <= b <= 0xFF:
                ev["ornament"] = b & 0x0F
                ev["envelope"] = None
                p += 1
                ev["sample"] = d[p] * 2; p += 1
            elif 0xC1 <= b <= 0xCF:
                ev["volume"] = b & 0x0F
                p += 1
            else:
                break            # note, $C0 ou $D0 : fin du prefixe

        # -- phase 2 : le declencheur, qui cloture la ligne -------------------
        b = d[p]
        if 0x50 <= b <= 0xAF:
            ev["note"] = b - 0x50
            p += 1
        elif b == 0xC0:
            ev["note_off"] = True
            p += 1
        elif b == 0xD0:
            p += 1
        else:
            raise ValueError(
                "declencheur de note attendu ($50-$AF, $C0 ou $D0), "
                "octet %#x a l'offset %d" % (b, p))

        # -- phase 3 : au plus un effet, immediatement apres -------------
        if p < n and 0x01 <= d[p] <= 0x0F:
            eb = d[p]
            # Longueurs de parametres : cf. README_pt3.txt "Effects". On
            # consomme sans appliquer (etape 3, differee -- voir docstring
            # du module). Codes non documentes (0x06, 0x07, 0x0A-0x0F) :
            # echec explicite plutot qu'une longueur devinee, qui
            # desynchroniserait tout le reste du flux en silence.
            try:
                param_len = {0x01: 3, 0x02: 5, 0x03: 1, 0x04: 1,
                             0x05: 2, 0x08: 3, 0x09: 1}[eb]
            except KeyError:
                raise ValueError(
                    "effet %#x non documente (README_pt3.txt ne couvre pas"
                    " ce code) a l'offset %d -- longueur de parametre"
                    " inconnue, abandon plutot que deviner" % (eb, p))
            ev.setdefault("effects", []).append(EFFECT_NAMES.get(eb, "effet-%#x" % eb))
            n_effects += 1
            p += 1 + param_len

        rows.append(ev)
        for _ in range(skip):
            rows.append({})

    return rows, n_effects


def read(path):
    return parse(open(path, "rb").read())


def parse(d):
    """Comme `read()`, mais depuis des octets deja en memoire -- ce qui
    permet aux tests (test/pt3_fixtures.py) de construire un module sans
    passer par le disque."""
    if d[:13] != b"ProTracker 3.":
        raise ValueError("signature PT3 absente (pas un fichier .pt3 ?)")

    version_byte = d[0x0D]
    version = version_byte - 0x30 if 0x30 <= version_byte <= 0x39 else 6
    name = d[0x1E:0x3E].rstrip(b"\x00 ").decode("latin-1", "replace")
    author = d[0x42:0x62].rstrip(b"\x00 ").decode("latin-1", "replace")
    freq_table = d[0x63]
    speed = d[0x64]
    lpos_ptr = d[0x66]
    pats_ptr = _cstr16(d, 0x67)

    sam_ptrs = [_cstr16(d, 0x69 + i * 2) for i in range(32)]
    orn_ptrs = [_cstr16(d, 0xA9 + i * 2) for i in range(16)]

    # -- liste de lecture : offsets *3, terminee par $FF --------------------
    order = []
    p = 0xC9
    while d[p] != 0xFF:
        order.append(d[p] // 3)
        p += 1
    loop_order = min(lpos_ptr, max(len(order) - 1, 0))

    # -- echantillons ---------------------------------------------------
    samples = {}
    for i, off in enumerate(sam_ptrs):
        if off == 0 or off >= len(d):
            continue
        loop, length = d[off], d[off + 1]
        vals = []
        for k in range(length):
            base = off + 2 + k * 4
            if base + 4 > len(d):
                break
            vals.append(tuple(d[base:base + 4]))
        samples[i] = {"loop": loop, "data": vals}

    # -- ornements --------------------------------------------------------
    ornaments = {0: {"loop": 0, "data": [0]}}       # ornement 0 = neutre
    for i, off in enumerate(orn_ptrs):
        if off == 0 or off >= len(d):
            continue
        loop, length = d[off], d[off + 1]
        vals = []
        for k in range(length):
            if off + 2 + k >= len(d):
                break
            v = d[off + 2 + k]
            vals.append(v - 256 if v >= 128 else v)     # signe
        ornaments[i] = {"loop": loop, "data": vals or [0]}

    # -- motifs : table de pointeurs (6 octets/motif : A,B,C x 16 bits) ---
    patterns = {}
    total_effects = 0
    for idx in set(order):
        base = pats_ptr + idx * 6
        chans = {}
        for ci, name_c in enumerate("ABC"):
            addr = _cstr16(d, base + ci * 2)
            rows, nfx = _parse_channel_stream(d, addr)
            chans[name_c] = rows
            total_effects += nfx
        n_rows = max(len(chans[c]) for c in "ABC") if any(chans.values()) else 0
        for c in "ABC":
            if len(chans[c]) < n_rows:
                chans[c] += [{}] * (n_rows - len(chans[c]))
        patterns[idx] = chans

    return {
        "version": version, "name": name, "author": author,
        "freq_table": freq_table, "speed": speed,
        "order": order, "loop_order": loop_order,
        "samples": samples, "ornaments": ornaments,
        "patterns": patterns, "n_effects_ignored": total_effects,
    }


def to_apple(mod, max_frames=None):
    """-> (trames, loop_frame).

    `trames` : liste de {registre: valeur}, comme ym.to_apple() -- un seul
    passage lineaire dans `order`, jamais reboucle ici (§ commentaire
    ci-dessous). `loop_frame` : index de trame ou commence le motif de
    bouclage (`loop_order`), a passer tel quel a `a2m.encode(loop_frame=)`.

    Simulation de lecture en ordre chanson : parcourt `order`, pour chaque
    motif chaque ligne, tient `speed` trames par ligne. Seuls note + volume
    + arpege d'ornement sont appliques (cf. docstring du module)."""
    speed = max(1, mod["speed"])
    ornaments = mod["ornaments"]

    ch = {c: {"note": None, "on": False, "vol": 0, "orn": 0, "orn_pos": 0,
              "orn_reset": False} for c in "ABC"}

    prev = {}
    out = []

    def emit_frame(env_trigger):
        cur = {}
        mixer_lo = 0x3F
        for i, c in enumerate("ABC"):
            st = ch[c]
            if st["on"] and st["note"] is not None:
                orn = ornaments.get(st["orn"], ornaments[0])
                data = orn["data"]
                pos = st["orn_pos"]
                if pos >= len(data):
                    loop = orn["loop"]
                    pos = loop + (pos - len(data)) % max(1, len(data) - loop)
                offset = data[pos] if data else 0
                note = max(0, min(95, st["note"] + offset))
                per = _period_apple(note)
                cur[i * 2] = per & 0xFF
                cur[i * 2 + 1] = (per >> 8) & 0x0F
                cur[8 + i] = st["vol"]
                mixer_lo &= ~(1 << i) & 0x3F      # ton actif -> bit a 0
                st["orn_pos"] += 1
            else:
                cur[8 + i] = 0
        cur[7] = mixer_lo
        if env_trigger is not None:
            shape, per = env_trigger      # stocke (etype, periode) au parsing
            # Reechelonnee comme les periodes de ton (§2.2) : sur 16 bits,
            # pas 12, donc pas de clamp a 4095 ici.
            per = max(1, min(0xFFFF, int(round(per * (APPLE_CLOCK / ZX_CLOCK)))))
            cur[11] = per & 0xFF
            cur[12] = (per >> 8) & 0xFF
            cur[13] = shape & 0x0F
        delta = {r: v for r, v in cur.items() if r == 13 or prev.get(r) != v}
        prev.update(cur)
        out.append(delta)

    def apply_row(c, ev):
        st = ch[c]
        if "note" in ev:
            st["note"] = ev["note"]; st["on"] = True; st["orn_pos"] = 0
        if ev.get("note_off"):
            st["on"] = False
        if "ornament" in ev:
            st["orn"] = ev["ornament"]; st["orn_pos"] = 0
        if ev.get("ornament_reset"):
            st["orn_pos"] = 0
        if "volume" in ev:
            st["vol"] = ev["volume"]

    # Un seul passage lineaire dans `order` : c'est au LECTEUR 6502 de
    # reboucler (comme il le fait deja pour YM/MIDI), pas a l'exporteur.
    # On note seulement a quelle trame commence le motif de bouclage, pour
    # le passer en `loop_frame` a a2m.encode().
    pat_seq = mod["order"] or [0]
    n_frames_done = 0
    loop_frame = 0
    truncated = False

    for i, pat_idx in enumerate(pat_seq):
        if i == mod["loop_order"]:
            loop_frame = n_frames_done
        pat = mod["patterns"].get(pat_idx, {"A": [], "B": [], "C": []})
        n_rows = max((len(pat[c]) for c in "ABC"), default=0)
        for row in range(n_rows):
            env_trigger = None
            for c in "ABC":
                rows_c = pat[c]
                ev = rows_c[row] if row < len(rows_c) else {}
                apply_row(c, ev)
                if ev.get("envelope") is not None:
                    env_trigger = ev["envelope"]
            for _ in range(speed):
                emit_frame(env_trigger)
                env_trigger = None
                n_frames_done += 1
                if max_frames is not None and n_frames_done >= max_frames:
                    truncated = True
                    break
            if truncated:
                break
        if truncated:
            break

    return out, min(loop_frame, len(out))

    return out
