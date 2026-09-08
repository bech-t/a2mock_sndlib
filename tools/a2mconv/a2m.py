"""a2m.py -- le format A2M, profil R (flux de registres).

Un module R decrit l'etat des 14 registres de l'AY, trame par trame, en ne
codant QUE ce qui change.

    trame := masque(2 octets, petit-boutien) [rle(1 octet)] valeurs(0..14)
             bit i (0..13) = « le registre i est ecrit, sa valeur suit »
             bit 14        = un octet de RLE suit le masque : nombre de trames
                             IDENTIQUES supplementaires (1..255)
             bit 15        = reserve (0)
    fin    := 0xFFFF

Les valeurs suivent dans l'ORDRE CROISSANT des numeros de registre -- le meme
ordre que celui dans lequel mb_push() les pousse, ce n'est pas un hasard : le
lecteur 6502 recopie les octets au fil du masque sans jamais revenir en
arriere.

POURQUOI UN MASQUE ET PAS UN DIFF. Le registre 13 (forme d'enveloppe) n'est pas
un etat mais un DECLENCHEUR : y ecrire REARME l'enveloppe, meme avec la meme
valeur. Un encodage « ecris ce qui a change » perdrait tous les rearmements et
transformerait une basse percussive en note tenue. Le masque distingue
nativement « r13 vaut toujours 8 » de « on reecrit 8 ».
"""

import struct

MAGIC = b"A2M\x03"
PROFILE_R = ord('R')
PROFILE_T = ord('T')

FLAG_LOOP = 0x01
FLAG_2AY  = 0x02

HEADER_SIZE = 48
VIA_CLOCK   = 1020500        # horloge du 6522 en slot, moyenne Apple II


def latch(hz):
    """Valeur T1 pour `hz` ticks/seconde. Le -2 : T1 en free-run compte N+2."""
    return VIA_CLOCK // hz - 2


def _field(text, n):
    """Champ texte de n octets : majuscules ASCII, complete par des espaces.

    Majuscules parce que l'Apple II+ n'a aucun glyphe minuscule -- un titre en
    minuscules y serait illisible, pas seulement laid."""
    b = (text or "").upper().encode("ascii", "replace")[:n]
    return b + b" " * (n - len(b))


# --- Encodage v2 ----------------------------------------------------------
#
# La v1 payait DEUX masques de 16 bits par trame, soit 4 octets fixes -- 54 %
# d'un module a six voies, avant la premiere valeur utile. Mesure sur la Fee
# Dragee (2500 trames) :
#
#     rien ne change ................................ 35,9 %
#     SEULES des amplitudes changent, de -2/-1/+1 ... 51,3 %
#     periodes ou mixer ............................. 12,7 %
#
# Autrement dit, une trame sur deux ne transporte que « telle voie baisse d'un
# cran » -- et le faisait en sept octets. D'ou trois changements :
#
#  1. UN OCTET DE CONTROLE dit ce qui suit. Une trame vide coute 1 octet, plus
#     l'octet de RLE ; elle en coutait 4.
#
#  2. UN BLOC DE DELTAS D'AMPLITUDE : six voies x 2 bits dans 2 octets.
#     00 inchange, 01 = -1, 10 = -2, 11 = +1. Les 51,3 % de trames « amplitudes
#     seules » passent de ~7 octets a 3, et ne transportent PLUS DE VALEURS du
#     tout -- c'est le lecteur qui applique le delta.
#
#  3. MASQUES REORDONNES PAR FREQUENCE, avec bit de continuation. L'octet de
#     poids faible porte les sept registres qui bougent (amplitudes, mixer,
#     octets fins de periode) ; le second n'est emis que pour les registres
#     rares (octets grossiers de periode, bruit, enveloppe). Mesure : r1, r3 et
#     r5 -- les octets grossiers -- ne changent QU'UNE FOIS dans tout le
#     morceau, la musique tenant dans une octave de periodes.

# Ordre des bits de masque : du plus frequent au plus rare.
MASK_ORDER = [8, 9, 10, 7, 0, 2, 4,   1, 3, 5, 6, 11, 12, 13]

CTRL_MASK1 = 0x01     # un masque AY#1 suit
CTRL_MASK2 = 0x02     # un masque AY#2 suit
CTRL_RLE   = 0x04     # un octet de repetition suit
CTRL_AMP   = 0x08     # un bloc de deltas d'amplitude (2 octets) suit
CTRL_END   = 0xFF     # fin de flux

# Codes de delta d'amplitude, sur 2 bits.
_DELTA_CODE = {0: 0, -1: 1, -2: 2, +1: 3}
_CODE_DELTA = {0: 0, 1: -1, 2: -2, 3: +1}


def _pack_mask(f):
    """dict {registre: valeur} -> (octets de masque, octets de valeurs)."""
    if not f:
        return b"", b""
    bits = 0
    for i, r in enumerate(MASK_ORDER):
        if r in f:
            bits |= 1 << i
    lo = bits & 0x7F
    hi = (bits >> 7) & 0x7F
    mask = bytes([lo | 0x80, hi]) if hi else bytes([lo])
    vals = bytearray()
    for i, r in enumerate(MASK_ORDER):
        if bits & (1 << i):
            vals.append(f[r] & 0xFF)
    return mask, bytes(vals)


def encode(frames, *, title="", author="", hz=50, loop_frame=0,
           looping=True, n_ay=1):
    """frames : ce qui CHANGE a chaque trame.
    n_ay = 1 : liste de dict {registre: valeur}
    n_ay = 2 : liste de couples (dict_AY1, dict_AY2)
    """
    two = (n_ay == 2)
    norm = [(f if two else (f, {})) for f in frames]

    # Etat courant des amplitudes, pour reconnaitre les deltas encodables.
    amp = [[0, 0, 0], [0, 0, 0]]
    body = bytearray()
    i = 0
    n = len(norm)
    while i < n:
        d0, d1 = norm[i]

        # -- la trame ne touche-t-elle QUE des amplitudes, par petits pas ? --
        only_amp = True
        codes = [0] * 6
        for ay, d in ((0, d0), (1, d1)):
            for r, v in d.items():
                if r in (8, 9, 10):
                    delta = v - amp[ay][r - 8]
                    c = _DELTA_CODE.get(delta)
                    if c is None:
                        only_amp = False
                    else:
                        codes[ay * 3 + (r - 8)] = c
                else:
                    only_amp = False
        use_delta = only_amp and any(codes)

        ctrl = 0
        payload = bytearray()
        if use_delta:
            ctrl |= CTRL_AMP
            packed = 0
            for k, c in enumerate(codes):
                packed |= c << (k * 2)
            payload += struct.pack("<H", packed)
            m0 = m1 = b""
            v0 = v1 = b""
        else:
            m0, v0 = _pack_mask(d0)
            m1, v1 = _pack_mask(d1)
            if m0: ctrl |= CTRL_MASK1
            if m1: ctrl |= CTRL_MASK2

        # -- RLE : uniquement sur les trames rigoureusement vides ------------
        run = 0
        if ctrl == 0:
            j = i + 1
            while j < n and run < 255 and not norm[j][0] and not norm[j][1]:
                run += 1; j += 1
        if run:
            ctrl |= CTRL_RLE

        body.append(ctrl)
        body += payload
        body += m0; body += m1
        if run:
            body.append(run)
        body += v0; body += v1

        # -- suivre l'etat des amplitudes ------------------------------------
        if use_delta:
            for k, c in enumerate(codes):
                if c:
                    ay, ch = divmod(k, 3)
                    amp[ay][ch] = max(0, min(15, amp[ay][ch] + _CODE_DELTA[c]))
        else:
            for ay, d in ((0, d0), (1, d1)):
                for r, v in d.items():
                    if r in (8, 9, 10):
                        amp[ay][r - 8] = v
        i += 1 + run

    body.append(CTRL_END)
    flags = (FLAG_LOOP if looping else 0) | (FLAG_2AY if n_ay == 2 else 0)
    head = bytearray(HEADER_SIZE)
    head[0:4]  = MAGIC
    head[4]    = PROFILE_R
    head[5]    = flags
    head[6:8]  = struct.pack("<H", latch(hz))
    head[8]    = hz
    head[9:11] = struct.pack("<H", min(n, 0xFFFF))
    head[11:13] = struct.pack("<H", loop_frame)
    head[13:15] = struct.pack("<H", HEADER_SIZE)
    head[15]   = n_ay
    head[16:32] = _field(title, 16)
    head[32:48] = _field(author, 16)
    return bytes(head) + bytes(body)


def decode(blob):
    """Rejoue un module et rend l'etat COMPLET de chaque trame.

    Sert aux tests aller-retour : une erreur d'ordre de valeurs ou de bit de
    masque est inaudible et fatale au lecteur 6502."""
    assert blob[0:4] == MAGIC, "magie A2M absente"
    assert blob[4] == PROFILE_R, "profil non R"
    off = struct.unpack("<H", blob[13:15])[0]
    n_ay = blob[15]
    state = [[0] * 14, [0] * 14]
    out = []
    while True:
        ctrl = blob[off]; off += 1
        if ctrl == CTRL_END:
            break
        if ctrl & CTRL_AMP:
            packed = struct.unpack("<H", blob[off:off + 2])[0]; off += 2
            for k in range(6):
                c = (packed >> (k * 2)) & 3
                if c:
                    ay, ch = divmod(k, 3)
                    state[ay][8 + ch] = max(0, min(15,
                        state[ay][8 + ch] + _CODE_DELTA[c]))
        masks = []
        for flag in (CTRL_MASK1, CTRL_MASK2):
            if ctrl & flag:
                b0 = blob[off]; off += 1
                bits = b0 & 0x7F
                if b0 & 0x80:
                    bits |= (blob[off] & 0x7F) << 7; off += 1
                masks.append(bits)
            else:
                masks.append(0)
        run = 0
        if ctrl & CTRL_RLE:
            run = blob[off]; off += 1
        for ay, bits in enumerate(masks):
            for i, r in enumerate(MASK_ORDER):
                if bits & (1 << i):
                    state[ay][r] = blob[off]; off += 1
        snap = ([list(state[0]), list(state[1])] if n_ay == 2 else list(state[0]))
        out.append(snap)
        for _ in range(run):
            out.append(snap)
    return out


def header_info(blob):
    return {
        "profil":  chr(blob[4]),
        "boucle":  bool(blob[5] & FLAG_LOOP),
        "latch":   struct.unpack("<H", blob[6:8])[0],
        "hz":      blob[8],
        "trames":  struct.unpack("<H", blob[9:11])[0],
        "n_ay":    blob[15],
        "titre":   blob[16:32].decode("ascii").rstrip(),
        "auteur":  blob[32:48].decode("ascii").rstrip(),
    }


# ===========================================================================
# PROFIL T -- evenements + instruments
# ===========================================================================
#
# Mesure qui a decide de ce profil (Fee Dragee, 50 s, six voies) :
#
#     debuts de notes ..........   484   (9,7 / s)
#     ecritures d'amplitude ....  4902   (98 / s)
#
# Vingt fois plus d'amplitudes que de notes. Ce flux d'amplitudes n'est rien
# d'autre que l'ENVELOPPE, epelee trame par trame parce que le lecteur ne sait
# pas la fabriquer. Le profil T la lui apprend une fois pour toutes, et ne
# transmet plus que les notes.
#
# Ce n'est pas « mieux » que le profil R, c'est un autre marche. R convertit
# n'importe quoi exactement -- un YM Atari, un dump brut -- parce qu'il ne
# suppose rien. T ne marche que quand on CONNAIT les instruments. Depuis un
# MIDI on les connait : c'est nous qui les fabriquons.
#
# --- Corps ----------------------------------------------------------------
#     u8    nombre d'instruments
#     n x 8 instruments : crete, attaque, declin, tenue, chute, drapeaux,
#                         arpege1, arpege2
#     ...   flux d'evenements
#
# --- Instrument (8 octets) ------------------------------------------------
# Une enveloppe ADSR a pas entiers. Les vitesses sont en SEIZIEMES de pas
# d'amplitude par trame : 16 = un pas par trame, 8 = un pas toutes les deux
# trames, 26 = 1,6 pas par trame. Le lecteur tient l'amplitude en 4.4 (0..240)
# et ne fait qu'une addition saturee par voie et par trame.
#
#     crete    amplitude visee a l'attaque (0..15)
#     attaque  vitesse de montee ; 0 = instantane
#     declin   vitesse de descente vers `tenue`
#     tenue    plancher tant que la note dure (0 = la note s'eteint seule)
#     chute    vitesse de descente apres le NOTE OFF
#     drapeaux b0 = BRUIT au lieu du ton. La hauteur ecrite ne choisit plus
#                   une periode de ton mais la periode du generateur de bruit :
#                   grave = sourd, aigu = claquant. Attention, ce generateur
#                   est UNIQUE par puce -- deux voies de bruit sur le meme AY
#                   se partagent la derniere periode ecrite.
#     arpege1/2 decalages en DEMI-TONS. Non nuls, la voie alterne
#               note, note+arpege1, note+arpege2 a chaque trame : un accord sur
#               une seule voie. C'est l'astuce chiptune par excellence, et elle
#               ne coute qu'une reecriture de periode par trame.
#
# --- Evenements -----------------------------------------------------------
#     0x00+v   NOTE ON  voie v (0..5) ; octet suivant = hauteur (0..95)
#     0x08+v   NOTE OFF voie v
#     0x10+v   INSTRUMENT voie v ; octet suivant = numero
#     0x80..0xFD  ATTENDRE (n - 0x7F) trames, soit 1 a 126
#     0xFE     POINT DE BOUCLE
#     0xFF     FIN

PROFILE_T   = ord('T')
EV_NOTE_ON  = 0x00
EV_NOTE_OFF = 0x08
EV_INSTR    = 0x10
EV_WAIT     = 0x80
EV_LOOP     = 0xFE
EV_END      = 0xFF

FLAG_BRUIT = 0x01

# (crete, attaque, declin, tenue, chute, drapeaux, arpege1, arpege2)
INSTRUMENTS = {
    # --- tons simples ----------------------------------------------------
    #  (les CLEFS sont en anglais : ce sont des mots du format de fichier,
    #   pas de la documentation)
    "pluck":   (15,  0, 17,  0, 17, 0, 0, 0),   # celesta, boite a musique
    "bass":    (14,  0, 26,  0, 26, 0, 0, 0),   # pizzicato, plus sec encore
    "sustain": (14, 48,  2,  8, 64, 0, 0, 0),   # tenue, fin nette
    "soft":    (13, 16,  0, 13, 64, 0, 0, 0),   # cordes, nappes

    "organ":   (15,  0,  0, 15, 40, 0, 0, 0),   # plateau plein, sans attaque ni declin
    "piano":   (15,  0,  5,  5, 24, 0, 0, 0),   # frappe nette puis long declin continu
    "brass":   (15, 64, 10, 12, 48, 0, 0, 0),   # attaque soufflee, plateau eclatant

    # --- accords sur UNE voie (arpege a 3 pas, une note par trame) --------
    "major":   (15,  0, 14,  0, 17, 0, 4, 7),
    "minor":   (15,  0, 14,  0, 17, 0, 3, 7),
    "seventh": (15,  0, 14,  0, 17, 0, 4, 10),
    "fifth":   (15,  0, 14,  0, 17, 0, 7, 12),
    "octave":   (15,  0, 16,  0, 17, 0, 12, 0),

    # --- percussions (bruit ; la hauteur ecrite regle le grain) -----------
    "drum":    (15,  0, 30,  0, 30, FLAG_BRUIT, 0, 0),   # sec : caisse, grosse
    "cymbal":  (12,  0,  5,  0, 20, FLAG_BRUIT, 0, 0),   # long : charleston
    "wind":  (10, 24,  0,  9, 32, FLAG_BRUIT, 0, 0),   # vent, ressac
}
INSTR_NAMES = list(INSTRUMENTS)


def encode_t(events, instruments, *, title="", author="", hz=50,
             looping=True, n_ay=2, n_frames=0):
    """events : liste de tuples
         ('wait', n) | ('on', voie, note) | ('off', voie) | ('instr', voie, num)
       instruments : liste de noms, dans l'ordre des numeros."""
    body = bytearray()
    body.append(len(instruments))
    for name in instruments:
        rec = INSTRUMENTS[name]
        body += bytes(rec + (0,) * (8 - len(rec)))

    loop_at = len(body)
    for ev in events:
        if ev[0] == "wait":
            n = ev[1]
            while n > 0:                     # une attente tient sur 1..126
                k = min(n, 126)
                body.append(EV_WAIT + k - 1)
                n -= k
        elif ev[0] == "on":
            body.append(EV_NOTE_ON + ev[1]); body.append(ev[2])
        elif ev[0] == "off":
            body.append(EV_NOTE_OFF + ev[1])
        elif ev[0] == "instr":
            body.append(EV_INSTR + ev[1]); body.append(ev[2])
        elif ev[0] == "loop":
            loop_at = len(body)
    body.append(EV_END)

    flags = (FLAG_LOOP if looping else 0) | (FLAG_2AY if n_ay == 2 else 0)
    head = bytearray(HEADER_SIZE)
    head[0:4]   = MAGIC
    head[4]     = PROFILE_T
    head[5]     = flags
    head[6:8]   = struct.pack("<H", latch(hz))
    head[8]     = hz
    head[9:11]  = struct.pack("<H", min(n_frames, 0xFFFF))
    # Offset ABSOLU dans le fichier, comme data_offset juste en dessous.
    # `loop_at` compte depuis le debut du CORPS ; y oublier l'en-tete faisait
    # reboucler le lecteur 48 octets trop tot -- en plein dans la chaine
    # d'auteur, dont il interpretait les caracteres ASCII comme des
    # evenements. D'ou des notes qui restaient et un morceau qui partait en
    # vrille des la premiere reprise.
    head[11:13] = struct.pack("<H", loop_at + HEADER_SIZE)
    head[13:15] = struct.pack("<H", HEADER_SIZE)
    head[15]    = n_ay
    head[16:32] = _field(title, 16)
    head[32:48] = _field(author, 16)
    return bytes(head) + bytes(body)
