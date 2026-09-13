"""pt3.py -- lecture des modules PT3 (ProTracker 3 / Vortex Tracker II, ZX
Spectrum) -> flux de registres profil R.

Un PT3 n'est PAS un dump de registres comme un YM : c'est une partition
compressee (notes, ornements, echantillons, effets) pensee pour etre
INTERPRETEE par un lecteur. Contrairement au YM (§ym.py), on ne peut pas se
contenter de rejouer des octets ; il faut reproduire ce que ferait un lecteur
PT3 reel, frame par frame, pour obtenir un flux de registres exploitable par
le profil R -- exactement l'"oracle" de spec.md §5.6/§5.7.

CE QUE CETTE VERSION FAIT :
  - lit le conteneur en entier : en-tete, liste de motifs, tables
    d'echantillons et d'ornements, flux d'evenements par voie ;
  - interprete les notes, le volume, l'ARPEGE D'ORNEMENT, l'ENVELOPPE
    D'ECHANTILLON (amplitude, glissement de ton, routage bruit/mixer -- le
    tableau `samples`, lu depuis la v1 mais pas encore exploite) et les
    EFFETS de note (glissando, portamento, vibrato on/off, offsets
    d'echantillon/ornement, changement de vitesse) ;
  - utilise les VRAIES tables de frequence et de volume publiees, par
    version de tracker (pt3_tables.py), pas une approximation par calcul ;
  - reechelonne l'horloge ZX -> Apple, exactement comme ym.py.

L'ORACLE STRICT DE §5.6 EXISTE ET A TOURNE (2026-09-10/11) : `PT3Play.cs`
(portage C# direct du lecteur original de Sergey Bulba) compile et s'execute
tel quel sous .NET 8/Linux -- voir spec.md §5.7 et tools/pt3oracle/README.md
pour l'installation et le harnais de comparaison. Compare trame par trame,
registre par registre, sur 4 fichiers reels, il a trouve et fait corriger
SIX bugs reels que la seule relecture n'avait pas vus :
  1. le mixer forcait une voie inactive a "desactivee" ; la reference la
     laisse a "activee" (l'amplitude a 0 suffit a la taire) -- ecart des
     la toute premiere trame ;
  2. `$B1` (Number_Of_Notes_To_Skip) ajoutait N lignes vides au lieu de
     N-1 -- Note_Skip_Counter est decompte a CHAQUE limite de ligne, la
     ligne suivante n'arrive qu'apres N limites, pas N+1 ;
  3. `$B1` est PERSISTANT PAR VOIE, pas ponctuel : la reference reecrit
     Note_Skip_Counter = Number_Of_Notes_To_Skip a la fin de CHAQUE ligne
     qui s'execute, que $B1 y soit reapparu ou non -- une valeur fixee une
     fois continue donc a s'appliquer a tous les evenements suivants
     jusqu'au prochain $B1 ;
  4. `$10-$1F` (enveloppe + echantillon) ne divisait pas par deux l'octet
     d'indice d'echantillon, contrairement a `$F0-$FF` qui partage le
     meme octet -- un boitier "boucle == longueur" sur l'echantillon
     mal indexe masquait le symptome au lieu de planter ;
  5. le bruit (r6) n'etait JAMAIS reechelonne ZX -> Apple (ym.py le fait
     deja pour le sien) -- transportait la periode source telle quelle ;
  6. le glissando ($01) portait le nom `"glissando/tone"` dans la table des
     effets mais le codage l'attendait sous `"glissando"` -- l'effet le
     plus courant du corpus etait donc COMPTE comme applique sans jamais
     l'etre, en silence, depuis le debut.
Apres ces six corrections, le flux de registres (reechelonne ZX->Apple)
colle a l'oracle EXACTEMENT, registre par registre, sur les 800 premieres
trames de chacun des 4 morceaux de la demo -- aucun ecart nulle part en
dehors de r11/r12 (periode d'enveloppe, voir juste en dessous : implementee,
mais dont la correction reste PLAUSIBLE, pas PROUVEE).

LA ROUTE ENVELOPPE (r11/r12), implementee mais NON VERIFIABLE :
  la formule de reference elle-meme est cassee -- l'oracle CONFIRME que
  `Env_Base_lo` vaut 0 tant qu'aucune commande d'enveloppe n'est survenue, et
  que `AY_Sys_GetWord(Module, Env_Base_lo)` relit alors les DEUX PREMIERS
  OCTETS DU FICHIER LUI-MEME ("Pr" de "ProTracker 3.") comme si c'etait une
  periode -- verifie sur oldlove.pt3, trame 0 : oracle r11/r12 = 0x7250,
  exactement "Pr" en petit-boutien. Bug confirme du portage (ou de sa
  source), pas une formule a reproduire : impossible de comparer notre sortie
  a un oracle dont la sortie de reference est elle-meme fausse sur ce point
  precis. `to_apple()` implemente donc la route enveloppe (base fixee par la
  commande d'enveloppe + AddToEnv accumule par voie depuis l'octet de
  donnees d'echantillon qui porte le bit route-enveloppe + glissement propre
  a l'effet $08) directement d'apres la lecture de PT3_ChangeRegisters/
  PT3_PatternInterpreter, register r11/r12 ecrit A CHAQUE TRAME (pas
  seulement au declenchement, puisqu'il peut desormais varier entre deux
  commandes) ; r13 (forme) reste ecrit UNIQUEMENT au declenchement, comme
  avant (l'ecrire rearmerait le generateur materiel a chaque trame). C'est
  la seule piece du convertisseur qui reste "plausible" plutot que "prouvee
  registre par registre" -- documente ici et dans spec.md §5.7 pour que ce
  ne soit pas oublie.

CE QUI RESTE NON FAIT, et POURQUOI :
  - TURBOSOUND (6 voix, deux sous-fichiers PT3 chaines) : non lu du tout.
    La detection cote reference (`PT3_FindSig` + `Array.Copy` dans
    `PT3_Init`) a l'air elle-meme fausse (offset RELATIF a la fenetre de
    recherche utilise comme position ABSOLUE), et aucun fichier TurboSound
    sous licence claire n'est disponible pour verifier -- voir spec.md
    §5.7. Un module a 2 sous-fichiers se lirait aujourd'hui comme un
    module 3 voix ordinaire (le second sous-fichier ignore).

Fichier tronque ou corrompu : `ValueError` claire (pas une IndexError sans
contexte) des la signature ou la taille minimale, et a la premiere lecture
hors bornes plus loin dans le flux -- voir `parse()`.

Reference du format : README_pt3.txt de Vince Weaver (deater.net) pour le
conteneur ; PT3Play.cs (github.com/benbaker76/PT3Play, MIT -- portage direct
du lecteur original de Sergey Bulba, l'auteur du format) pour tout le reste,
la doc en prose s'etant averee ambigue sur un point et fausse sur un autre
(voir le commentaire de _parse_channel_stream) -- et l'oracle construit a
partir de ce meme PT3Play.cs a son tour trouve six bugs que la prose
n'aurait jamais pu reveler.
"""

import math
import struct

import pt3_tables as _t

ZX_CLOCK    = 1773400.0
APPLE_CLOCK = 1020500.0

# --- notes ------------------------------------------------------------
# idx 0..95 = C-1..B-8 dans la numerotation PT3 (octet de note $50+idx).
# Table selectionnee par (freq_table, version) -- cf. PT3_GetNoteFreq,
# PT3Play.cs:254. Familles 0/2/3 ont une variante "vieille" (version<=3) et
# une variante "34_35" ; la famille 1 ("ST", historiquement Atari ST) est
# commune a toutes les versions.
_NOTE_TABLES = {
    (0, True): _t.PT3NOTETABLE_PT_33_34R, (0, False): _t.PT3NOTETABLE_PT_34_35,
    (1, True): _t.PT3NOTETABLE_ST,        (1, False): _t.PT3NOTETABLE_ST,
    (2, True): _t.PT3NOTETABLE_ASM_34R,   (2, False): _t.PT3NOTETABLE_ASM_34_35,
    (3, True): _t.PT3NOTETABLE_REAL_34R,  (3, False): _t.PT3NOTETABLE_REAL_34_35,
}


def _note_table(freq_table, version):
    return _NOTE_TABLES.get((freq_table % 4, version <= 3), _t.PT3NOTETABLE_ST)


def _volume_table(version):
    """PT3Play.cs:589 -- table a deux entrees (volume 0..15, amplitude
    0..15) -> amplitude AY reelle. Change de forme a la version 3.5."""
    return _t.PT3VOLUMETABLE_33_34 if version <= 4 else _t.PT3VOLUMETABLE_35


def _rescale(per_zx, bits=12):
    """Reechelonnement ZX -> Apple (§2.2), applique a une periode DEJA
    calculee -- pas de recalcul depuis une frequence theorique ici."""
    per = int(round(per_zx * (APPLE_CLOCK / ZX_CLOCK)))
    return max(1, min((1 << bits) - 1, per))


def period_for(mod, idx):
    """Periode Apple pour la note `idx`, avec la VRAIE table du module --
    ce que `to_apple()` utilise en interne. Expose pour les tests et le
    rapport de justesse."""
    return _rescale(_note_table(mod["freq_table"], mod["version"])[idx])


def table_anchor(table):
    """(A, confiance_cents). Les 7 tables publiees N'UTILISENT PAS TOUTES
    le meme ancrage -- verifie empiriquement (2026-09-10) : sur la table
    "ST", l'indice 47 tombe a 439,8 Hz (la-4, a 0,7 cent pres), PAS
    l'indice 45 malgre le nommage "A-4" de README_pt3.txt pour cet indice.
    Les autres tables ("PT", "ASM", "REAL") ne s'alignent sur AUCUN indice
    a mieux que 30-40 cents -- ce ne sont probablement pas de simples
    transpositions d'un 12-TET propre, mais des tables tenant compte de
    quirks materiels/d'epoque (leurs noms le suggerent). On cherche donc,
    empiriquement, l'indice le mieux aligne sur la grille 12-TET (la-4 =
    440 Hz) et on s'en sert comme reference -- `confiance_cents` dit a
    quel point cette reference est solide (proche de 0 = table 12-TET
    precise ; grand = la notion meme d'"ecart de justesse" est fragile
    pour cette table, cf. spec.md §5.7)."""
    best_A, best_dev = 45, 1e9
    for idx in range(24, 72):
        f = ZX_CLOCK / (16.0 * table[idx])
        k = 12.0 * math.log2(f / 440.0)
        dev = abs(k - round(k))
        if dev < best_dev:
            best_dev, best_A = dev, idx - round(k)
    return best_A, best_dev * 100.0


def cents_error(idx, per_zx=None, anchor=45):
    """Ecart, en cents, entre la note theorique (12-TET, ancree sur
    `anchor` = la-4) et celle que la periode entiere obtenue sur
    Mockingboard reproduit reellement. `per_zx` : periode ZX reelle (table
    publiee) ; a defaut, calculee par la meme formule theorique -- utile
    aux tests qui n'ont pas de module PT3 sous la main, mais NE mesure
    alors que l'arrondi 12 bits, pas l'ecart propre a la table du tracker.
    `anchor` : voir `table_anchor()` -- 45 (l'ancrage "naif", cf. §5.7)
    n'est correct QUE pour les tests/fixtures qui n'utilisent pas de vraie
    table publiee."""
    f = 440.0 * 2.0 ** ((idx - anchor) / 12.0)
    if per_zx is None:
        per_zx = max(1, min(4095, int(round(ZX_CLOCK / (16.0 * f)))))
    per = _rescale(per_zx)
    f_reelle = APPLE_CLOCK / (16.0 * per)
    return 1200.0 * math.log2(f_reelle / f)


def _cstr16(d, off):
    return struct.unpack_from("<H", d, off)[0]


def _s16le(d, off):
    return struct.unpack_from("<h", d, off)[0]


# --- opcodes du flux d'evenements par voie -----------------------------
# cf. README_pt3.txt, section "Pattern data". Les plages sont EXCLUSIVES
# entre elles ; un octet appartient a une seule.

EFFECT_NAMES = {
    0x01: "glissando",
    0x02: "portamento",
    0x03: "sample-offset",
    0x04: "ornament-offset",
    0x05: "vibrato",
    0x08: "envelope-glissando",
    0x09: "set-speed",
}

# code -> extracteur(d, p) des parametres, lus a l'offset `p` (le premier
# octet APRES le declencheur de note, cf. _parse_channel_stream) -- jamais
# a cote de l'opcode lui-meme.
EFFECT_PARAMS = {
    0x01: lambda d, p: {"delay": d[p], "add": _s16le(d, p + 1)},
    0x02: lambda d, p: {"delay": d[p], "step": _s16le(d, p + 3)},
    0x03: lambda d, p: {"value": d[p]},
    0x04: lambda d, p: {"value": d[p]},
    0x05: lambda d, p: {"onoff": d[p], "offon": d[p + 1]},
    0x08: lambda d, p: {"delay": d[p], "add": _s16le(d, p + 1)},
    0x09: lambda d, p: {"speed": d[p]},
}

# Longueur des PARAMETRES de chaque effet (sans l'octet d'opcode, deja
# consomme pendant le balayage -- voir plus bas pourquoi).
EFFECT_PARAM_LEN = {0x01: 3, 0x02: 5, 0x03: 1, 0x04: 1, 0x05: 2, 0x08: 3, 0x09: 1}


def _parse_channel_stream(d, start):
    """Decode le flux d'UNE voie -> liste d'"evenements de ligne".

    GRAMMAIRE CONFIRMEE CONTRE UNE SOURCE FAISANT AUTORITE, pas seulement
    contre README_pt3.txt : `PT3Play.cs` (github.com/benbaker76/PT3Play,
    MIT), qui est un portage direct du lecteur original de Sergey Bulba --
    l'auteur du format. Premiere version de ce fichier ecrite sur la seule
    foi de README_pt3.txt, dont la description en prose etait AMBIGUE sur
    un point precis, et faux sur un autre :

    1. Une ligne PT3 est un simple BALAYAGE d'octets, un par un : chaque
       commande de prefixe (enveloppe, bruit, ornement, volume, choix
       d'echantillon) consomme SES PROPRES octets immediatement. Un effet
       ($01-$09) ne consomme, PENDANT ce balayage, que son octet d'opcode
       -- ses parametres ne sont PAS a cote de lui. Le balayage continue
       (d'autres commandes de prefixe peuvent suivre) jusqu'a un
       DECLENCHEUR : une note ($50-$AF), $C0 ou $D0.
    2. Les PARAMETRES de chaque effet rencontre ne sont lus qu'APRES le
       declencheur, dans une seconde passe -- c'est le sens exact de la
       phrase "parameters to the effect appear in the bytestream *after*
       the note to play" de README_pt3.txt, qu'une premiere lecture rendait
       autrement (effet ET parametres tous deux apres la note).
    3. `$11-$1F` (enveloppe + echantillon) ne fait QUE 4 octets (opcode +
       periode 16 bits + index d'echantillon), pas 5 : README_pt3.txt
       mentionne un octet de "delai d'enveloppe" entre les deux que
       PT3Play.cs ne lit jamais. Confirme sur 7 fichiers PT3 reels
       (shiru.untergrund.net, CC-BY, spec.md §5.7) : la version a 5 octets
       desynchronisait TOUJOURS le flux en quelques centaines d'octets, la
       version a 4 ne desynchronise plus aucun des 8 fichiers testes.
    4. Les octets $00, $06, $07, $0A-$0F ne sont pas des erreurs : ni
       commande de prefixe, ni effet, ni declencheur reconnu par
       PT3_PatternInterpreter, ils sont des NO-OP d'un octet (`$00` reste a
       part : c'est la fin du flux de la voie, testee par l'appelant, pas
       ici).

    `$B1` (sauter N lignes) etire l'evenement qui vient d'etre construit
    sur les N lignes suivantes, representees par des evenements vides.

    Retourne (evenements, n_effets_total) -- tous les effets connus sont
    desormais interpretes par to_apple() (cf. docstring du module)."""
    rows = []
    n_effects = 0
    p = start
    n = len(d)
    # $B1 (Number_Of_Notes_To_Skip) est PERSISTANT PAR VOIE dans la
    # reference : PT3Play.cs ecrit Note_Skip_Counter = Number_Of_Notes_To_Skip
    # a la fin de CHAQUE ligne qui s'execute, que $B1 y soit reapparu ou non
    # -- la valeur reste donc collee a tous les evenements suivants jusqu'au
    # prochain $B1, pas seulement a celui qui vient de la fixer. Trouve par
    # l'oracle (spec.md §5.7) : sans ca, un $B1 rencontre une fois ne
    # s'appliquait qu'a l'evenement qui le portait, et tout le reste du flux
    # avancait une ligne a la fois -- correct pour les premieres lignes,
    # faux des que $B1 devait continuer a s'appliquer plus loin.
    skip = 1
    while p < n and d[p] != 0x00:
        ev = {}
        effects_seen = []

        # -- balayage : commandes de prefixe et opcodes d'effet, jusqu'au
        # declencheur qui cloture la ligne -----------------------------
        while True:
            b = d[p]
            if 0x10 <= b <= 0x1F:
                if b == 0x10:
                    ev["envelope"] = None          # desactivee
                    p += 1
                else:
                    etype = b & 0x0F
                    period = (d[p + 1] << 8) | d[p + 2]      # grand-boutien
                    ev["envelope"] = (etype, period)
                    p += 3
                # Comme $F0-$FF : PT3Play.cs divise cet octet par 2
                # (`Module[Address_In_Pattern] / 2`) -- oublie ici, ce qui
                # lisait le double du bon indice d'echantillon. Trouve par
                # l'oracle (spec.md §5.7) : $10-$1F pointait sur
                # l'echantillon 6 (46 pas) au lieu du 3 (3 pas) reellement
                # vise, avec un boitier "boucle == longueur" qui masquait
                # le symptome au lieu de planter.
                ev["sample"] = d[p] // 2
                p += 1
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
            elif 0xC1 <= b <= 0xCF:
                ev["volume"] = b & 0x0F
                p += 1
            elif 0xD1 <= b <= 0xEF:
                ev["sample"] = b - 0xD0
                p += 1
            elif 0xF0 <= b <= 0xFF:
                ev["ornament"] = b & 0x0F
                ev["envelope"] = None
                p += 1
                ev["sample"] = d[p] // 2           # l'octet stocke est deja x2
                p += 1
            elif b in EFFECT_PARAM_LEN:
                effects_seen.append(b)
                p += 1                              # opcode seul ; params plus bas
            elif b in (0x00, 0x06, 0x07) or 0x0A <= b <= 0x0F:
                p += 1                               # reserve/inconnu : no-op
            else:
                break            # note ($50-$AF), $C0 ou $D0 : fin de la ligne

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

        # -- parametres des effets rencontres, MAINTENANT (apres le
        # declencheur), un par un ; l'ordre entre plusieurs effets sur la
        # meme ligne n'affecte pas le nombre d'octets consommes. Valeurs
        # LUES (pas seulement sautees) depuis cette version -- cf.
        # EFFECT_PARAMS, AY_Sys_GetWord (les mots 16 bits d'effet sont
        # PETIT-boutiens, contrairement aux periodes d'enveloppe/ton qui
        # sont grand-boutiennes).
        for eb in effects_seen:
            ev.setdefault("effects", []).append(
                (EFFECT_NAMES.get(eb, "effet-%#x" % eb), EFFECT_PARAMS[eb](d, p)))
            n_effects += 1
            p += EFFECT_PARAM_LEN[eb]

        rows.append(ev)
        # $B1 : Number_Of_Notes_To_Skip=N devient Note_Skip_Counter=N APRES
        # cette ligne (PT3Play.cs:526,645-646) ; le compteur est decremente
        # a CHAQUE limite de ligne suivante et l'interpreteur ne rejoue
        # qu'au moment ou il atteint 0 -- soit N-1 lignes vides apres
        # celle-ci, pas N. Off-by-one trouve par l'oracle (spec.md §5.7) :
        # une premiere version en ajoutait N, decalant tout d'une ligne des
        # le premier $B1 rencontre dans un fichier reel.
        for _ in range(max(0, skip - 1)):
            rows.append({})

    return rows, n_effects


def read(path):
    return parse(open(path, "rb").read())


def parse(d):
    """Comme `read()`, mais depuis des octets deja en memoire -- ce qui
    permet aux tests (test/pt3_fixtures.py) de construire un module sans
    passer par le disque.

    Ne valide que la signature et la taille minimale AVANT de commencer --
    pas le contenu au-dela (meme limite, assumee, que le lecteur 6502 lui
    -meme : cf. le README du depot). Un fichier tronque ou corrompu plus
    loin dans le flux leve une erreur CLAIRE (fichier, offset) plutot
    qu'une IndexError brute sans contexte -- une conversion ratee doit
    dire pourquoi, pas juste planter."""
    if d[:13] != b"ProTracker 3.":
        raise ValueError("signature PT3 absente (pas un fichier .pt3 ?)")
    if len(d) < 0xC9 + 1:            # en-tete + au moins le $FF de la liste
        raise ValueError(
            "fichier trop court pour un en-tete PT3 complet (%d o, "
            "%d o minimum)" % (len(d), 0xC9 + 1))
    try:
        return _parse_body(d)
    except (IndexError, struct.error) as e:
        raise ValueError(
            "PT3 tronque ou corrompu : %s a manque d'octets en le lisant "
            "(%s)" % (type(e).__name__, e)) from e


def _parse_body(d):
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
        "patterns": patterns,
        "n_effects": total_effects,
    }


def _new_channel_state():
    return {
        "note": 0, "on": False, "vol": 15,
        "orn": 0, "orn_pos": 0,
        "sample": 0, "sam_pos": 0,
        "env_on": False,
        "ton_acc": 0, "amp_slide": 0, "noise_slide": 0,
        # glissando ($01) / portamento ($02) -- PT3Play.cs:474-490
        "slide_count": 0, "slide_delay": 0, "slide_step": 0,
        "simple_gliss": True, "ton_delta": 0, "slide_to_note": 0,
        "cur_ton_slide": 0,
        # vibrato ($05) -- PT3Play.cs:501-510
        "onoff_cur": 0, "onoff_delay": 0, "offon_delay": 0,
        # route enveloppe ($08, AddToEnv) -- accumulateur par voie, meme
        # forme que noise_slide (cf. docstring du module : plausible, pas
        # verifie contre l'oracle).
        "env_slide": 0,
    }


def to_apple(mod, max_frames=None):
    """-> (trames, loop_frame).

    `trames` : liste de {registre: valeur}, comme ym.to_apple() -- un seul
    passage lineaire dans `order`, jamais reboucle ici (§ commentaire
    ci-dessous). `loop_frame` : index de trame ou commence le motif de
    bouclage (`loop_order`), a passer tel quel a `a2m.encode(loop_frame=)`.

    Port de PT3_ChangeRegisters/PT3_PatternInterpreter (PT3Play.cs) --
    voir la docstring du module pour ce qui est simplifie ou pas applique."""
    speed = [max(1, mod["speed"])]      # mutable : $09 (set-speed) le change
    ornaments = mod["ornaments"]
    samples = mod["samples"]
    note_table = _note_table(mod["freq_table"], mod["version"])
    volume_table = _volume_table(mod["version"])

    ch = {c: _new_channel_state() for c in "ABC"}
    chip_noise_base = [0]
    # Route enveloppe ($08, effet glissement d'enveloppe) -- etat au niveau
    # de la puce (un seul generateur d'enveloppe AY, partage par les 3
    # voies), meme structure que le glissando de ton par voie
    # (slide_delay/slide_count/cur_ton_slide) mais ici globale. Cf.
    # docstring du module : implemente sur la base de la lecture de
    # PT3Play.cs, mais PAS verifiable contre l'oracle (sa propre route
    # enveloppe est confirmee cassee) -- reste "plausible", pas "prouve".
    chip_env_base = [0]
    chip_env_delay = [0]
    chip_env_cur_delay = [0]
    chip_env_slide_add = [0]
    chip_env_cur_slide = [0]

    prev = {}
    out = []

    def _table_pos(table_entry, pos):
        """Position dans une table ornement/echantillon, avec bouclage."""
        data = table_entry["data"]
        if pos >= len(data):
            loop = table_entry["loop"]
            pos = loop + (pos - len(data)) % max(1, len(data) - loop)
        return data[pos] if data else None

    def emit_frame(env_trigger):
        cur = {}
        mixer = 0
        chip_add_noise = 0
        any_noise = False
        chip_add_env = 0

        # Glissement d'enveloppe ($08) : compteur AU NIVEAU DE LA PUCE (pas
        # par voie), meme mecanique que le glissando de ton (slide_count/
        # slide_delay) mais une seule instance pour les 3 voies -- un seul
        # generateur d'enveloppe materiel.
        if chip_env_cur_delay[0] > 0:
            chip_env_cur_delay[0] -= 1
            if chip_env_cur_delay[0] == 0:
                chip_env_cur_slide[0] += chip_env_slide_add[0]
                chip_env_cur_delay[0] = chip_env_delay[0]

        for i, c in enumerate("ABC"):
            st = ch[c]
            if not st["on"]:
                cur[8 + i] = 0
                # PAS de bits mixer forces a 1 ici : verifie contre l'oracle
                # (PT3Play.cs) qu'une voie inactive ne contribue RIEN au
                # mixer (TempMixer part de 0 et le bloc entier est saute
                # pour un canal Enabled=false) -- elle "semble" activee
                # dans le registre, et c'est l'amplitude a 0 qui la tait
                # reellement, pas le mixer. Le contraire (force a 1) faisait
                # diverger r7 des la toute premiere trame.
            else:
                sample = samples.get(st["sample"])
                entry = _table_pos(sample, st["sam_pos"]) if sample else None

                if entry is None:
                    # Pas d'echantillon exploitable pour cette voie : repli
                    # simple (ton + volume directs), comme la version qui
                    # n'exploitait pas encore les echantillons.
                    b0 = b1 = 0
                    ton_from_sample = 0
                    amplitude = st["vol"]
                    tone_bit, noise_bit = 0, 1
                else:
                    b0, b1, tlo, thi = entry
                    ton_from_sample = tlo | (thi << 8)
                    tone_bit = (b1 >> 4) & 1
                    noise_bit = (b1 >> 7) & 1

                ton = ton_from_sample + st["ton_acc"]
                if entry is not None and (b1 & 0x40):
                    st["ton_acc"] = ton

                orn = ornaments.get(st["orn"], ornaments[0])
                offset = _table_pos(orn, st["orn_pos"]) or 0
                j = max(0, min(95, st["note"] + offset))
                ton_zx = (ton + st["cur_ton_slide"] + note_table[j]) & 0xFFF

                if st["slide_count"] > 0:
                    st["slide_count"] -= 1
                    if st["slide_count"] == 0:
                        st["cur_ton_slide"] += st["slide_step"]
                        st["slide_count"] = st["slide_delay"]
                        if not st["simple_gliss"]:
                            step, delta, cur_s = (st["slide_step"], st["ton_delta"],
                                                   st["cur_ton_slide"])
                            if (step < 0 and cur_s <= delta) or (step >= 0 and cur_s >= delta):
                                st["note"] = st["slide_to_note"]
                                st["slide_count"] = 0
                                st["cur_ton_slide"] = 0

                if entry is not None:
                    if b0 & 0x80:
                        if b0 & 0x40:
                            st["amp_slide"] = min(15, st["amp_slide"] + 1)
                        else:
                            st["amp_slide"] = max(-15, st["amp_slide"] - 1)
                    amplitude = max(0, min(15, (b1 & 0xF) + st["amp_slide"]))
                    amplitude = volume_table[st["vol"]][amplitude]
                    if (b0 & 1) == 0 and st["env_on"]:
                        amplitude |= 16

                    if not (b1 & 0x80):           # route bruit, pas enveloppe
                        add_noise = ((b0 >> 1) + st["noise_slide"]) & 0xFF
                        if b1 & 0x20:
                            st["noise_slide"] = add_noise
                        chip_add_noise = add_noise
                        any_noise = True
                    else:                          # route enveloppe (AddToEnv)
                        # Nibble signe (bit 0x20 du sample-data = signe) +
                        # glissement par voie, replie en sbyte (-128..127)
                        # comme le ferait l'accumulateur C# cote reference.
                        # SOMME sur les voies (pas ecrasement, contrairement
                        # au bruit) -- cf. docstring du module.
                        raw = ((b0 >> 1) | 0xF0) if (b0 & 0x20) else ((b0 >> 1) & 0xF)
                        j = raw + st["env_slide"]
                        j = ((j + 0x80) & 0xFF) - 0x80
                        chip_add_env += j
                        if b1 & 0x20:
                            st["env_slide"] = j

                    st["sam_pos"] += 1
                    if st["sam_pos"] >= len(sample["data"]):
                        st["sam_pos"] = sample["loop"]
                st["orn_pos"] += 1

                per = _rescale(ton_zx)
                cur[i * 2] = per & 0xFF
                cur[i * 2 + 1] = (per >> 8) & 0x0F
                cur[8 + i] = amplitude & 0x1F
                mixer |= (tone_bit << i) | (noise_bit << (i + 3))

            # Vibrato ($05) : bascule Enabled independamment du reste --
            # PT3Play.cs:622-633, hors du bloc if(Enabled).
            if st["onoff_cur"] > 0:
                st["onoff_cur"] -= 1
                if st["onoff_cur"] == 0:
                    st["on"] = not st["on"]
                    st["onoff_cur"] = st["onoff_delay"] if st["on"] else st["offon_delay"]

        cur[7] = mixer & 0x3F
        if any_noise:
            # Periode de bruit : rescale ZX -> Apple oublie (ym.py le fait
            # deja pour le sien) -- transportait la valeur ZX brute telle
            # quelle. Trouve par l'oracle sur summer.pt3 (spec.md §5.7).
            noise_zx = (chip_noise_base[0] + chip_add_noise) & 0x1F
            cur[6] = max(1, min(31, round(noise_zx * (APPLE_CLOCK / ZX_CLOCK)))) if noise_zx else 0
        # r11/r12 (periode d'enveloppe) : ECRITE A CHAQUE TRAME desormais,
        # pas seulement au declenchement -- chip_env_base a deja ete mis a
        # jour par apply_row() au moment du declenchement (voir plus bas),
        # et chip_add_env/chip_env_cur_slide la font vivre entre deux
        # declenchements (route enveloppe des echantillons + effet $08).
        # Reechelonnee comme les periodes de ton (§2.2) : sur 16 bits, pas
        # 12, donc pas de clamp a 4095 ici.
        env_zx = chip_env_base[0] + chip_add_env + chip_env_cur_slide[0]
        per = _rescale(env_zx, bits=16)
        cur[11] = per & 0xFF
        cur[12] = (per >> 8) & 0xFF
        if env_trigger is not None:
            shape, _per = env_trigger     # stocke (etype, periode) au parsing
            # r13 (forme) : SEULEMENT a la trame de declenchement -- l'ecrire
            # rearme le generateur materiel (test_envelope le verifie).
            cur[13] = shape & 0x0F
        delta = {r: v for r, v in cur.items() if r == 13 or prev.get(r) != v}
        prev.update(cur)
        out.append(delta)

    def apply_row(c, ev):
        st = ch[c]
        prev_note, prev_sliding = st["note"], st["cur_ton_slide"]

        if "note" in ev:
            st["note"] = ev["note"]; st["on"] = True
            st["sam_pos"] = 0; st["orn_pos"] = 0
            st["amp_slide"] = 0; st["noise_slide"] = 0
            st["slide_count"] = 0; st["cur_ton_slide"] = 0; st["ton_acc"] = 0
            st["onoff_cur"] = 0
        if ev.get("note_off"):
            st["on"] = False
            st["sam_pos"] = 0; st["orn_pos"] = 0
            st["amp_slide"] = 0; st["noise_slide"] = 0
            st["slide_count"] = 0; st["cur_ton_slide"] = 0; st["ton_acc"] = 0
            st["onoff_cur"] = 0
        if "ornament" in ev:
            st["orn"] = ev["ornament"]; st["orn_pos"] = 0
        if ev.get("ornament_reset"):
            st["orn_pos"] = 0
        if "volume" in ev:
            st["vol"] = ev["volume"]
        if "sample" in ev:
            st["sample"] = ev["sample"]            # PAS de reset de sam_pos
        if "envelope" in ev:
            st["env_on"] = ev["envelope"] is not None
            if ev["envelope"] is not None:
                # Nouvelle commande d'enveloppe : la periode DEVIENT la
                # nouvelle base (chip_env_base, en unites ZX -- reechelonne
                # a l'emission comme le ton), et tout glissement en cours
                # (effet ou echantillon) repart de zero -- meme logique que
                # ton_acc/cur_ton_slide remis a zero sur une note. orn_pos
                # est aussi remis a zero pour CETTE voie : elle porte la
                # commande d'enveloppe, et son ornement doit repartir avec.
                chip_env_base[0] = ev["envelope"][1]
                chip_env_cur_slide[0] = 0
                chip_env_cur_delay[0] = 0
                st["orn_pos"] = 0

        speed_change = None
        for name, params in ev.get("effects", []):
            if name == "glissando":
                st["slide_delay"] = params["delay"]
                st["slide_count"] = params["delay"] or 1
                st["slide_step"] = params["add"]
                st["simple_gliss"] = True
                st["onoff_cur"] = 0
            elif name == "portamento":
                st["simple_gliss"] = False
                st["onoff_cur"] = 0
                st["slide_delay"] = params["delay"]
                st["slide_count"] = params["delay"] or 1
                st["slide_step"] = abs(params["step"])
                st["ton_delta"] = note_table[st["note"]] - note_table[prev_note]
                st["slide_to_note"] = st["note"]
                st["note"] = prev_note
                st["cur_ton_slide"] = prev_sliding
                if st["ton_delta"] - st["cur_ton_slide"] < 0:
                    st["slide_step"] = -st["slide_step"]
            elif name == "sample-offset":
                st["sam_pos"] = params["value"]
            elif name == "ornament-offset":
                st["orn_pos"] = params["value"]
            elif name == "vibrato":
                st["onoff_delay"] = params["onoff"]
                st["offon_delay"] = params["offon"]
                st["onoff_cur"] = st["onoff_delay"]
                st["slide_count"] = 0; st["cur_ton_slide"] = 0
            elif name == "set-speed":
                speed_change = params["speed"]
            elif name == "envelope-glissando":
                # Meme mecanique que le glissando de ton ($01), mais au
                # niveau de la puce (chip_env_*, pas st[...]) -- un seul
                # generateur d'enveloppe pour les 3 voies. "or 1" ne
                # s'applique qu'a l'amorce, comme pour le ton : si delay=0,
                # le compteur suivant peut se figer a 0 -- reproduit tel
                # quel (cf. glissando de ton plus haut), pas corrige.
                chip_env_delay[0] = params["delay"]
                chip_env_cur_delay[0] = params["delay"] or 1
                chip_env_slide_add[0] = params["add"]
        return speed_change

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
            new_speed = None
            if row == 0:
                chip_noise_base[0] = 0     # PT3_Play_Chip:656, au changement de motif
            for c in "ABC":
                rows_c = pat[c]
                ev = rows_c[row] if row < len(rows_c) else {}
                if ev.get("noise") is not None:
                    chip_noise_base[0] = ev["noise"]
                sc = apply_row(c, ev)
                if sc is not None:
                    new_speed = sc
                if ev.get("envelope") is not None:
                    env_trigger = ev["envelope"]
            for _ in range(speed[0]):
                emit_frame(env_trigger)
                env_trigger = None
                n_frames_done += 1
                if max_frames is not None and n_frames_done >= max_frames:
                    truncated = True
                    break
            if new_speed is not None:
                speed[0] = max(1, new_speed)
            if truncated:
                break
        if truncated:
            break

    return out, min(loop_frame, len(out))
