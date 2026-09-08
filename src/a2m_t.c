/* a2m_t.c -- moteur du profil T : des notes et des instruments.
 *
 * Module SEPARE du profil R (voir a2m_r.c). Appelez a2m_play_t() plutot que
 * a2m_play() pour n'embarquer que celui-ci.
 */

#include "a2m_int.h"

/* Masques pre-calcules, indexes par la voie locale (0..2).
 *
 * cc65 n'a pas de decalage a distance variable : `1 << ch` avec ch inconnu a
 * la compilation devient une BOUCLE de decalages, et sur 16 bits elle coute
 * cher. Ces quatre tables la remplacent par une lecture indexee -- et ces
 * expressions tournent a chaque note et a chaque trame. */
/* Tout ce qui depend de la voie, PRE-CALCULE et indexe par la voie (0..5).
 *
 * MB_REG(ay, r) est une macro : `mb_regs[((ay) << 4) + (r)]`. Quand `r` est
 * lui-meme une expression -- `AY_TONE_A_LO + ch * 2` -- cc65 refait toute
 * l'arithmetique 16 bits A CHAQUE ECRITURE. Ces tables la remplacent par un
 * `ldy table,x` suivi d'un `sta mb_regs,y`. C'est la meme idee que pour les
 * decalages a distance variable : sur un 6502, une table bat un calcul. */
static const u8  k_ay[6]   = { 0, 0, 0, 1, 1, 1 };          /* puce           */
static const u8  k_treg[6] = { 0, 2, 4, 16, 18, 20 };       /* periode fine   */
static const u8  k_areg[6] = { 8, 9, 10, 24, 25, 26 };      /* amplitude      */
static const u8  k_nreg[6] = { 6, 6, 6, 22, 22, 22 };       /* bruit          */

static const u16 m_tone[6] = { 0x0003, 0x000C, 0x0030,
                               0x0003, 0x000C, 0x0030 };    /* bits r0+r1 ... */
static const u16 m_amp[6]  = { 0x0100, 0x0200, 0x0400,
                               0x0100, 0x0200, 0x0400 };    /* bits r8/r9/r10 */
static const u8  m_mixt[6] = { 0xFE, 0xFD, 0xFB, 0xFE, 0xFD, 0xFB };
static const u8  m_mixn[6] = { 0xF7, 0xEF, 0xDF, 0xF7, 0xEF, 0xDF };

static void t_init(const u8 *mod);
static void t_frame(void);
static void t_voices_off(void);


/* ===========================================================================
 * PROFIL T -- evenements + instruments
 *
 * Ce que le profil R transportait en clair -- 98 ecritures d'amplitude par
 * seconde, contre 9,7 debuts de notes -- n'etait rien d'autre que l'ENVELOPPE,
 * epelee trame par trame. Ici le lecteur la FABRIQUE : le flux ne porte plus
 * que les notes. Mesure sur la Fee Dragee : 6 625 -> 1 657 octets.
 *
 * Le prix est du temps CPU : six generateurs d'enveloppe a chaque tick. Le
 * gain est de la place : a 32 octets/seconde, les 14 Ko de tampon tiennent
 * sept minutes de musique au lieu d'une.
 * ======================================================================== */

#define EV_NOTE_ON  0x00
#define EV_NOTE_OFF 0x08
#define EV_INSTR    0x10
#define EV_WAIT     0x80
#define EV_LOOP     0xFE
#define EV_END      0xFF

#define PH_OFF      0
#define PH_ATTACK   1
#define PH_DECAY    2
#define PH_SUSTAIN  3
#define PH_RELEASE  4

/* Enveloppe ADSR a pas entiers. Les vitesses sont en SEIZIEMES de pas
 * d'amplitude par trame : 16 = un pas par trame, 8 = un pas toutes les deux
 * trames. L'amplitude est tenue en 4.4 (0..240), ce qui reduit chaque trame a
 * une addition saturee -- pas de multiplication, pas de table. */
/* Instruments en TABLEAUX PARALLELES, et non en tableau de structures.
 *
 * cc65 indexe `t[i]` par un simple decalage ; `s[i].champ` lui coute une
 * multiplication par la taille de la structure PLUS un calcul de pointeur 16
 * bits, a chaque acces. Sur six voies et cinq champs par trame, la difference
 * n'est pas cosmetique. */

/* drapeaux d'instrument */
#define IF_BRUIT 0x01

#define MAX_INSTR 8
u8 in_peak[MAX_INSTR], in_atk[MAX_INSTR], in_dec[MAX_INSTR];
u8 in_sus[MAX_INSTR],  in_rel[MAX_INSTR], in_flg[MAX_INSTR];
u8 in_ar1[MAX_INSTR],  in_ar2[MAX_INSTR];

/* Crete et tenue deja multipliees par 16, calculees UNE FOIS au chargement.
 * En les decalant a chaque trame on payait quatre `asl` par voie et par
 * trame -- six voies, cinquante fois par seconde, pour un resultat qui ne
 * change jamais. */
u8 in_pk16[MAX_INSTR], in_su16[MAX_INSTR];
u8 n_instr;

u8 v_instr[6];      /* instrument de chaque voie          */
u8 v_phase[6];      /* PH_*                               */
u8 v_amp[6];        /* amplitude x16                      */
u8 v_out[6];        /* derniere amplitude poussee (0..15) */
u8 v_note[6];       /* hauteur de base, pour l'arpege     */
u8 v_arp[6];        /* pas courant de l'arpege : 0, 1, 2  */
u8 v_mix[6];        /* 1 = cette voie sort du BRUIT       */

/* Masques accumules pendant la trame, pousses UNE SEULE FOIS a la fin.
 *
 * Avant, chaque note-on poussait ses deux registres tout de suite. Or mb_push
 * balaie les quatorze bits du masque a chaque appel -- 22 cycles par bit
 * ignore -- si bien qu'une poussee de deux registres coute ~500 cycles et non
 * 170. Sur une trame dense (une dizaine de notes simultanees, ce qui arrive
 * dans une reduction d'orchestre) on payait donc dix fois ce balayage.
 * Mesure : pic du handler a 20 131 cycles sur les 20 408 d'une periode. */
u16 pend0, pend1;   /* deux scalaires, pas un tableau : cc65 indexe un
                     * tableau de mots par une arithmetique 16 bits a chaque
                     * acces, alors que ces deux-la sont connus a la compilation. */
static u8 wait_left;
static const u8 *loop_pos;

static void t_init(const u8 *mod)
{
    const u8 *p = mod + a2m_rd16(mod + H_DATAOFF);
    u8 i, k;

    n_instr = *p++;
    if (n_instr > MAX_INSTR)
        n_instr = MAX_INSTR;
    for (i = 0; i < n_instr; ++i) {
        in_peak[i] = *p++;
        in_atk[i]  = *p++;
        in_dec[i]  = *p++;
        in_sus[i]  = *p++;
        in_rel[i]  = *p++;
        in_flg[i]  = *p++;
        in_ar1[i]  = *p++;
        in_ar2[i]  = *p++;
        in_pk16[i] = (u8)(in_peak[i] << 4);
        in_su16[i] = (u8)(in_sus[i] << 4);
    }
    /* Le corps ne commence qu'APRES la table : c'est elle qui fixe le point
     * de depart du flux, pas l'en-tete. */
    a2m_p = p;
    /* Point de rebouclage, en offset ABSOLU. Garde-fou : s'il tombe avant le
     * debut du flux (module produit par un ancien encodeur, ou en-tete
     * abimee), on reboucle au debut plutot que d'aller lire l'en-tete comme
     * si c'etaient des evenements. */
    loop_pos = mod + a2m_rd16(mod + H_LOOPFR);
    if (loop_pos < p)
        loop_pos = p;
    wait_left = 0;
    for (k = 0; k < 6; ++k) {
        v_instr[k] = 0;
        v_phase[k] = PH_OFF;
        v_amp[k] = 0;
        v_out[k] = 0xFF;                 /* force la premiere ecriture */
        v_note[k] = 0; v_arp[k] = 0; v_mix[k] = 0;
    }
    pend0 = pend1 = 0;
}

/* Pose la hauteur d'une voie : periode de ton, ou periode de BRUIT. */
/* `ay` et `ch` sont fournis par l'appelant, qui les connait deja : les
 * recalculer ici les faisait deriver deux fois par note. */
static void t_pitch(u8 v, u8 note)
{
    u16 per;
    u8  r;

    if (v_mix[v]) {
        /* Instrument de bruit : la hauteur ecrite ne choisit pas une note mais
         * le GRAIN du bruit -- grave = sourd, aigu = claquant. La periode du
         * generateur tient sur 5 bits, et il est UNIQUE par puce : deux voies
         * de bruit sur le meme AY se partagent la derniere valeur ecrite. */
        mb_regs[k_nreg[v]] = (note < 13) ? 31
                           : (u8)((note > 43) ? 1 : 44 - note);
        if (k_ay[v]) pend1 |= (1 << AY_NOISE);
        else         pend0 |= (1 << AY_NOISE);
        return;
    }
    /* Acces DIRECT a la table plutot que mb_note_period() : on economise un
     * appel de fonction et un controle de bornes que l'appelant a deja fait
     * -- et cette routine tourne a chaque note et a chaque pas d'arpege. */
    if (note >= MB_NOTE_MAX)
        return;
    per = mb_note_table[note];
    r = k_treg[v];
    mb_regs[r]     = (u8)(per & 0xFF);
    mb_regs[r + 1] = (u8)((per >> 8) & 0x0F);
    if (k_ay[v]) pend1 |= m_tone[v];
    else         pend0 |= m_tone[v];
}

static void t_voices_off(void)
{
    u8 k;
    for (k = 0; k < 6; ++k) {
        v_phase[k] = PH_OFF;
        v_amp[k]   = 0;
        v_out[k]   = 0xFF;      /* force la reecriture de l'amplitude */
        v_note[k]  = 0;
        v_arp[k]   = 0;
        v_mix[k]   = 0;
    }
}

static void t_note_on(u8 v, u8 note)
{
    u8 ins = v_instr[v];

    v_mix[v]  = (u8)((in_flg[ins] & IF_BRUIT) != 0);
    v_note[v] = note;
    v_arp[v]  = 0;
    t_pitch(v, note);
    /* Attaque instantanee : on saute a la crete. Un `atk` de 0 voudrait dire
     * « ne monte jamais », ce qui n'a pas de sens pour une attaque. */
    if (in_atk[ins] == 0) {
        v_amp[v] = (u8)(in_peak[ins] << 4);
        v_phase[v] = PH_DECAY;
    } else {
        v_amp[v] = 0;
        v_phase[v] = PH_ATTACK;
    }
}

static void t_events(void)
{
    u8 e, v;

    /* Copie LOCALE du pointeur de flux. Un `*p++` sur une variable globale
     * oblige cc65 a recharger le pointeur, faire l'acces indirect, puis
     * reecrire le pointeur -- a chaque octet. Une copie locale lui permet de
     * le garder ; on ne remet la globale a jour qu'en sortant. */
    const u8 *p = a2m_p;

    for (;;) {
        e = *p++;

        /* Les valeurs >= 0x80 sont les commandes de flux, et l'attente est de
         * TRES loin la plus frequente : une par trame porteuse d'evenements.
         * Elle est donc testee la premiere -- l'ordre precedent lui faisait
         * subir trois comparaisons, et une note-on autant. */
        if (e >= EV_WAIT) {
            if (e < EV_LOOP) {                  /* 0x80..0xFD : attendre */
                wait_left = (u8)(e - (EV_WAIT - 1));
                a2m_p = p;
                return;
            }
            if (e == EV_LOOP)
                continue;
            /* EV_END */
            if (!a2m_loop) { a2m_p = p; a2m_stop(); return; }
            /* Rebouclage : on eteint les voix et on repart d'une puce propre,
             * sinon les notes en cours se superposent a la reprise. */
            t_voices_off();
            a2m_rewind_chips();
            p = loop_pos;
            a2m_no = 0;
            continue;
        }

        v = (u8)(e & 7);
        if (v > 5)
            continue;                    /* evenement inconnu : on l'ignore */
        if (e < EV_NOTE_OFF)             { t_note_on(v, *p); ++p; }
        else if (e < EV_INSTR)           v_phase[v] = PH_RELEASE;
        else                             { v_instr[v] = *p; ++p; }
    }
}

/* Les six enveloppes sont en ASSEMBLEUR (a2m_t_env.s).
 *
 * C'est le seul chemin vraiment chaud du lecteur : six voies, cinquante fois
 * par seconde, quoi que contienne le module. En C il pesait 2 263 octets et
 * portait le pic du handler a 15 400 cycles sur les 20 408 d'une periode.
 * Trois couts que cc65 dissimulait : le `switch` sur cinq etats, l'arithmetique
 * saturee en 4.4, et l'indexation des tableaux recalculee a chaque acces. */

/* Les six enveloppes d'une trame.
 *
 * Ce chemin a ete ecrit en assembleur puis REVENU en C : mesure faite,
 * l'assembleur ne rapportait que 11 % de cycles de plus, pour deux
 * implementations a tenir en phase -- et deux routines `pitch` qui pouvaient
 * diverger. Le C bien ecrit fait -50 % depuis le debut ; le doublon ne valait
 * pas 1 462 cycles. */
static void t_envelopes(void)
{
    u8  v, a, tgt, ins, ay;
    u8  amp, dec, rel, atk;
    u16 push0, push1;
    u8  mix0, mix1;

    push0 = pend0;
    push1 = pend1;
    pend0 = pend1 = 0;
    mix0 = mix1 = 0x3F;

    for (v = 0; v < 6; ++v) {
        ay = k_ay[v];

        /* Voie muette et qui le reste : rien a calculer, rien a pousser.
         * Le mixer part ferme (0x3F), donc la sauter ne rouvre rien. Dans une
         * reduction d'orchestre a six voix, plusieurs voies sont souvent au
         * repos -- c'est l'economie la plus rentable de cette boucle. */
        if (v_phase[v] == PH_OFF && v_out[v] == 0)
            continue;

        ins = v_instr[v];
        amp = v_amp[v];

        switch (v_phase[v]) {
        case PH_ATTACK:
            atk = in_atk[ins];
            tgt = in_pk16[ins];
            if (amp + atk >= tgt) { amp = tgt; v_phase[v] = PH_DECAY; }
            else                    amp = (u8)(amp + atk);
            break;
        case PH_DECAY:
            dec = in_dec[ins];
            tgt = in_su16[ins];
            if (dec == 0 || amp <= tgt + dec) {
                amp = tgt;
                v_phase[v] = tgt ? PH_SUSTAIN : PH_OFF;
            } else {
                amp = (u8)(amp - dec);
            }
            break;
        case PH_RELEASE:
            rel = in_rel[ins];
            if (rel == 0 || amp <= rel) { amp = 0; v_phase[v] = PH_OFF; }
            else                          amp = (u8)(amp - rel);
            break;
        default:
            break;
        }
        v_amp[v] = amp;

        if (v_phase[v] != PH_OFF && (in_ar1[ins] || in_ar2[ins])) {
            u8 pas = v_arp[v];
            pas = (u8)(pas >= 2 ? 0 : pas + 1);
            v_arp[v] = pas;
            t_pitch(v, (u8)(v_note[v] + (pas == 1 ? in_ar1[ins]
                                       : pas == 2 ? in_ar2[ins] : 0)));
            if (ay) push1 |= pend1; else push0 |= pend0;
            pend0 = pend1 = 0;
        }

        a = (u8)(amp >> 4);
        if (a) {
            tgt = v_mix[v] ? m_mixn[v] : m_mixt[v];
            if (ay) mix1 &= tgt; else mix0 &= tgt;
        }
        if (a != v_out[v]) {
            v_out[v] = a;
            if (a2m_atten && a > a2m_atten)  a = (u8)(a - a2m_atten);
            else if (a2m_atten)              a = 0;
            mb_regs[k_areg[v]] = a;
            if (ay) push1 |= m_amp[v]; else push0 |= m_amp[v];
        }
    }

    if (mb_regs[AY_MIXER] != mix0) {
        mb_regs[AY_MIXER] = mix0;
        push0 |= (1 << AY_MIXER);
    }
    if (push0)
        mb_push(0, push0);

    if (a2m_nay == 2) {
        if (mb_regs[16 + AY_MIXER] != mix1) {
            mb_regs[16 + AY_MIXER] = mix1;
            push1 |= (1 << AY_MIXER);
        }
        if (push1)
            mb_push(1, push1);
    }
}


#ifdef A2MB_DEBUG
/* Chronometrage des deux moities de la trame. L'horloge est celle du handler
 * (T1 de la VIA #2, free-run) : elle DECOMPTE, donc entree - sortie donne les
 * cycles. Sert a savoir OU part le temps au lieu de le supposer. */
unsigned __fastcall__ mbt_clock(void);
u16 a2m_t_ev, a2m_t_env;        /* pics respectifs, en cycles */
#endif

static void t_frame(void)
{
#ifdef A2MB_DEBUG
    u16 c0, c1, c2;
#endif
#ifdef A2MB_DEBUG
    c0 = mbt_clock();
#endif
    if (wait_left == 0) {
        t_events();
        if (a2m_st != A2M_PLAYING)
            return;
    }
    if (wait_left)
        --wait_left;
#ifdef A2MB_DEBUG
    c1 = mbt_clock();
#endif
    t_envelopes();
#ifdef A2MB_DEBUG
    c2 = mbt_clock();
    /* Lire le compteur en deux fois peut le prendre a cheval sur une
     * decrementation de l'octet haut : la valeur est alors fausse de 256, et
     * l'ecart devient negatif -- donc enorme en non signe. On ecarte tout ce
     * qui depasse une periode : aucune moitie de trame ne peut durer ca. */
    c0 = (u16)(c0 - c1);
    c1 = (u16)(c1 - c2);
    if (c0 < 20000 && c0 > a2m_t_ev)  a2m_t_ev  = c0;
    if (c1 < 20000 && c1 > a2m_t_env) a2m_t_env = c1;
#endif
    ++a2m_no;
}

/* Aiguillage : l'appelant ne sait pas quel profil il joue, et n'a pas a le
 * savoir. C'est l'octet 4 de l'en-tete qui decide. */

void __fastcall__ a2m_play_t(const u8 *mod, u8 loop)
{
    if (!a2m_begin(mod, loop))
        return;
    t_init(mod);
    a2m_engine = t_frame;
    a2m_st = A2M_PLAYING;
}
