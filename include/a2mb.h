/* a2mb.h -- couche carte Mockingboard : detection, registres AY, silence.
 *
 * Materiel : 2x AY-3-8910 (ou 8913), chacun pilote par un 6522 (VIA).
 *   VIA #1 / AY #1 en $Cn00     VIA #2 / AY #2 en $Cn80     (n = slot 1..7)
 * Six voies, DEUX sorties audio (une par AY) : la carte est litteralement une
 * TurboSound. Voies 0-2 = AY #1, voies 3-5 = AY #2.
 *
 * Cette couche ne sait RIEN de la musique. Un projet qui ne veut que des
 * bruitages ne linke qu'elle (cf. a2m.h pour le lecteur).
 *
 * --------------------------------------------------------------------------
 * REGLE D'ACCES CONCURRENT -- a lire avant d'appeler quoi que ce soit d'ici
 * depuis le programme principal pendant qu'une musique tourne.
 *
 * Le coeur d'ecriture est du code AUTO-MODIFIANT : l'adresse de base du slot
 * est ecrite dans les instructions elles-memes par mb_io_setslot(). C'est ce
 * qui rend la poussee de registres rapide (absolu,X : 5 cycles) sans consommer
 * un octet de page zero -- il n'y en a pas : la cible apple2 de cc65 n'expose
 * que 26 octets de ZP, deja pris par le runtime C.
 *
 * Le prix : ces routines ne sont PAS reentrantes. Si le tick tourne sous IRQ
 * et pousse des registres, un appel concurrent depuis le programme principal
 * tombe au milieu. D'ou la regle :
 *
 *   TICK ARRETE   -> appeler librement (init, reglages, bruitages simples).
 *   TICK EN COURS -> tout passe par le tick. Un acces direct s'encadre de
 *                    mb_lock() / mb_unlock().
 *
 * C'est aussi pour ca que les effets de a2mb_fx.h sont ARMES et non joues :
 * sous IRQ, une fonction d'effet bloquante n'aurait aucun sens.
 * --------------------------------------------------------------------------
 */
#ifndef A2MB_H
#define A2MB_H

/* __fastcall__ n'existe que chez cc65 : sur hote (tests gcc) on l'efface. */
#ifndef __CC65__
#define __fastcall__
#endif

#ifndef A2MB_TYPES
#define A2MB_TYPES
typedef unsigned char  u8;
typedef unsigned int   u16;
#endif

/* --- Detection ---------------------------------------------------------- */

/* Y a-t-il une Mockingboard dans CE slot (1..7) ? Renvoie 1 si DEUX 6522
 * repondent, en $Cn00 et $Cn80 -- signature que ne presente pas une carte a
 * 6522 unique (Grappler, certaines cartes imprimante).
 *
 * Ne balaie rien : on ne touche qu'au slot demande. */
u8 __fastcall__ mb_probe(u8 slot);

/* Sonde UN SEUL des deux 6522 : which = 0 pour $Cn00, 1 pour $Cn80.
 *
 * mb_probe() ne repond que oui ou non ; celle-ci dit LEQUEL des deux manque,
 * ce qui separe deux pannes tres differentes : « pas de carte du tout » et
 * « la carte est la mais le second VIA ne repond pas » (cablage, ou une carte
 * a 6522 unique qu'on avait prise pour une Mockingboard). */
u8 __fastcall__ mb_probe_via(u8 slot, u8 which);

/* Balaye les slots et renvoie le premier qui repond, ou 0.
 *
 * OPT-IN, et il faut savoir ce qu'on accepte : la sonde ECRIT dans $Cn03,
 * $Cn04, $Cn05 et $Cn0B de CHAQUE slot balaye, y compris ceux qui portent une
 * carte inconnue.
 *
 * ---------------------------------------------------------------------------
 * LE SLOT 6 EST EXCLU DU BALAYAGE, ET CE N'EST PAS UNE PRECAUTION VAGUE.
 *
 * Sur un controleur Disk II -- qui vit en slot 6 par convention quasi
 * universelle -- ces quatre adresses valent :
 *
 *     $Cn03 = phase 1 ON        $Cn04 = phase 2 OFF
 *     $Cn05 = phase 2 ON        $Cn0B = selection du lecteur 2
 *
 * Sonder le slot 6 ENERGISE donc deux phases du moteur pas-a-pas et change de
 * lecteur : la tete bouge, sur la disquette dont la machine vient de demarrer.
 * Ce n'est pas destructeur, mais c'est un effet de bord bien reel, et une
 * bibliotheque n'a pas a l'infliger par defaut.
 *
 * Une Mockingboard en slot 6 se prend donc A LA MAIN : mb_probe(6) puis
 * mb_init(6). L'appelant qui fait ce choix sait ce qu'il fait.
 * ---------------------------------------------------------------------------
 *
 * Balayage decroissant (7, 5, 4, 3, 2, 1) : on s'arrete au premier trouve,
 * donc partir du haut limite le nombre de slots effectivement touches. */
u8 mb_scan(void);

/* Slot exclu du balayage automatique (cf. ci-dessus). */
#define MB_SCAN_SKIP_SLOT 6

/* Variantes identifiables APRES une detection reussie. Purement informatif
 * (la demo l'affiche) : la bibliotheque n'en depend jamais. */
#define MB_CARD_UNKNOWN      0
#define MB_CARD_MOCKINGBOARD 1
#define MB_CARD_PHASOR       2
u8 mb_ident(void);

/* --- Initialisation ----------------------------------------------------- */

/* Prend la carte du slot donne : ports en sortie, les deux AY reset, silence.
 * N'arme aucune base de temps (cf. a2mb_time.h). Renvoie 0 si le slot ne
 * repond pas -- auquel cas la lib reste inactive et tous les appels ci-dessous
 * sont des no-op silencieux. */
u8 __fastcall__ mb_init(u8 slot);

/* Coupe les six voies. RAPIDE mais PARTIEL : n'ecrit que les amplitudes et le
 * mixer. Periodes, bruit et enveloppes gardent leur valeur dans la puce.
 * Convient entre deux notes ; pas pour changer de contexte. */
void mb_silence(void);

/* REMISE A ZERO d'UNE puce : impulsion de RESET, les 14 registres remis a un
 * etat connu, image RAM resynchronisee.
 *
 * Pourquoi par puce et pas globalement : dans un jeu, la MUSIQUE tient l'AY #1
 * et les BRUITAGES l'AY #2, en meme temps. Un reset global lance depuis le
 * lecteur de musique couperait l'effet en cours -- et inversement. Chacun ne
 * reinitialise que ce qu'il possede.
 *
 * C'est aussi pour ca que le lecteur A2M ne fait PAS de reset global : il
 * remet a zero l'AY #1, et l'AY #2 seulement si le module s'en sert (module
 * stereo / TurboSound, qui prend alors les deux). */
void __fastcall__ mb_reset_ay(u8 ay);

/* Les deux puces. Reservee a l'APPLICATION, qui seule sait qu'elle change de
 * contexte -- changement d'ecran, fin de niveau, retour au menu. La
 * bibliotheque ne l'appelle jamais d'elle-meme : ce serait decider a la place
 * de l'appelant ce qui doit se taire.
 *
 * mb_silence() ne la remplace pas : elle ne coupe qu'amplitudes et mixer, en
 * laissant la periode de bruit, les periodes de ton et les enveloppes telles
 * quelles. Une voie rouverte ensuite ressort le bruit d'avant, ou reste muette
 * parce que sa periode vaut zero.
 *
 * ATTENTION, dans l'autre sens : mb_probe() et mb_init() REPROGRAMMENT le
 * timer T1 de la VIA #1 (c'est ainsi qu'ils reconnaissent un 6522). Si un tick
 * tourne, il meurt. Toujours mbt_stop() AVANT de sonder ou de reinitialiser. */
void mb_reset(void);

/* Silence + on rend la carte : plus aucun acces materiel ensuite. */
void mb_shutdown(void);

/* Slot actif (1..7), ou 0 si aucune carte. */
extern u8 mb_slot;

/* --- Registres ---------------------------------------------------------- */
/*
 * Les registres de l'AY sont en ECRITURE SEULE : impossible de relire pour
 * modifier un seul bit. Tout ce qui demande un read-modify-write (le mixer,
 * registre 7) se tient donc en RAM, dans mb_regs.
 */

/* Image RAM des registres : 0..13 = AY #1, 16..29 = AY #2.
 * (Pas 14 puis 14 : le pas de 16 rend l'indexation par AY gratuite en asm.)
 * On y ecrit, puis on pousse avec mb_push. */
extern u8 mb_regs[32];

#define MB_REG(ay, r)  mb_regs[((ay) << 4) + (r)]

/* Pousse dans l'AY `ay` (0 ou 1) les registres designes par `mask` :
 * bit i (0..13) = « ecrire le registre i », valeur prise dans mb_regs.
 *
 * C'est LE chemin chaud -- il tourne 50 fois par seconde. Ecrit en asm, sans
 * page zero : 85 cycles par registre pousse, 22 par registre ignore. Une trame
 * pleine (14 registres) coute 1230 cycles, soit 6 % du CPU a 50 Hz.
 *
 * Pourquoi un masque et pas « ecris ce qui a change » : le registre 13 (forme
 * d'enveloppe) n'est pas un etat mais un DECLENCHEUR -- y ecrire REARME
 * l'enveloppe, meme avec la meme valeur. Un encodage differentiel perdrait
 * tous les rearmements et transformerait une basse percussive en note tenue.
 * Le masque distingue nativement « r13 vaut toujours 8 » de « on reecrit 8 ».
 * C'est le meme masque que celui du format A2M, dessine avec cette couche. */
void __fastcall__ mb_push(u8 ay, u16 mask);

#define MB_MASK_ALL  0x3FFF   /* les 14 registres */

/* Un seul registre, valeur immediate. Met aussi mb_regs a jour, pour que
 * l'image RAM ne mente jamais sur ce que la carte a recu. */
void __fastcall__ mb_reg(u8 ay, u8 reg, u8 val);

/* --- Notes -------------------------------------------------------------- */
/* Index 0 = do0, 57 = la4 (440 Hz), 95 = si7. Huit octaves.
 *
 * ATTENTION a l'aigu : la periode AY tient sur 12 bits, et la quantification
 * rend les octaves 6 et 7 audiblement fausses (jusqu'a -36 cents). Detail
 * chiffre octave par octave en tete de mb_notes.c. C'est le materiel, pas la
 * table. */
#define MB_NOTE_MAX  96
#define MB_NOTE_A4   57

extern const u16 mb_note_table[MB_NOTE_MAX];
u16 __fastcall__ mb_note_period(u8 note);   /* 0 si hors table */

/* --- Section critique --------------------------------------------------- */
/* Encadre un acces direct a la carte pendant qu'un tick IRQ tourne (cf. la
 * regle en tete de fichier). mb_lock est un SEI, mb_unlock un CLI : a garder
 * COURT, une IRQ manquee est un tick de musique perdu. */
void mb_lock(void);
void mb_unlock(void);

/* --- Numeros de registres AY -------------------------------------------- */
#define AY_TONE_A_LO  0
#define AY_TONE_A_HI  1
#define AY_TONE_B_LO  2
#define AY_TONE_B_HI  3
#define AY_TONE_C_LO  4
#define AY_TONE_C_HI  5
#define AY_NOISE      6     /* periode de bruit, 5 bits */
#define AY_MIXER      7     /* b0-2 tons, b3-5 bruits : 1 = COUPE */
#define AY_AMP_A      8     /* 0..15, ou AY_AMP_ENV */
#define AY_AMP_B      9
#define AY_AMP_C     10
#define AY_ENV_LO    11
#define AY_ENV_HI    12
#define AY_ENV_SHAPE 13     /* ecrire ici REARME l'enveloppe */

#define AY_AMP_ENV   0x10   /* « suivre l'enveloppe » (bit 4) */

/* Formes d'enveloppe utiles (registre 13) */
#define AY_ENV_DECAY   0x00   /* \___  percussif : attaque puis extinction */
#define AY_ENV_ATTACK  0x0C   /* /|/|  montee repetee */
#define AY_ENV_SWELL   0x0D   /* /---  montee puis tenue */
#define AY_ENV_TRI     0x0E   /* /\/\  triangle continu */

#endif /* A2MB_H */
