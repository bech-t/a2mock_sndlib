/* hosttest.c -- tests de la logique PORTABLE, sur PC, sans emulateur.
 *
 * Ce que ca couvre : la table de notes, le calcul du latch, et la machine a
 * etats des effets. Ce que ca NE couvre PAS, et il faut le dire : tout le
 * pilotage materiel (mb_io.s, mb_irq.s, mb_prodos.s), qui est en asm 6502 et
 * ne s'execute pas ici. Un hosttest vert ne dit rien de la carte.
 *
 * Il reste utile : ces trois morceaux sont ceux ou une erreur se voit mal a
 * l'oreille -- une note fausse d'un demi-ton s'entend, une duree fausse d'un
 * tick ne s'entend pas, et elle desynchronise une musique en trois minutes.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "a2mb.h"
#include "a2mb_time.h"
#include "a2mb_fx.h"
#include "a2m.h"

static int fails = 0, checks = 0;

static void ck(int cond, const char *what)
{
    ++checks;
    if (!cond) { ++fails; printf("  ECHEC : %s\n", what); }
}

/* --- table de notes ------------------------------------------------------ */
static void test_notes(void)
{
    unsigned i;
    double f, cents, worst = 0.0;
    unsigned worst_i = 0;

    printf("table de notes\n");

    ck(mb_note_period(MB_NOTE_A4) == 145, "la4 -> periode 145");
    ck(mb_note_period(MB_NOTE_MAX) == 0,  "hors table -> 0");
    ck(mb_note_period(255) == 0,          "index 255 -> 0");

    /* Strictement decroissante : plus aigu = periode plus petite. Une table
     * mal engendree se trahit ici avant de se trahir a l'oreille. */
    for (i = 1; i < MB_NOTE_MAX; ++i)
        if (mb_note_table[i] >= mb_note_table[i - 1]) {
            printf("  ECHEC : periode non decroissante en %u\n", i);
            ++fails;
            break;
        }
    ++checks;

    /* Toutes les periodes tiennent sur les 12 bits de l'AY. */
    for (i = 0; i < MB_NOTE_MAX; ++i)
        if (mb_note_table[i] == 0 || mb_note_table[i] > 4095) {
            printf("  ECHEC : periode hors 1..4095 en %u\n", i);
            ++fails;
            break;
        }
    ++checks;

    /* Justesse reelle, octave par octave : on RECALCULE la frequence obtenue
     * et on la compare au temperament egal. C'est la seule facon de savoir si
     * la table a ete engendree pour la bonne horloge -- une table de ZX
     * passerait les tests ci-dessus sans broncher, et sonnerait une quinte
     * trop grave. */
    for (i = 0; i < MB_NOTE_MAX; ++i) {
        f     = 440.0 * pow(2.0, ((double)i - MB_NOTE_A4) / 12.0);
        cents = 1200.0 * log2((1020500.0 / (16.0 * mb_note_table[i])) / f);
        if (fabs(cents) > fabs(worst)) { worst = cents; worst_i = i; }
    }
    printf("  erreur max : %+.1f cents (index %u, octave %u)\n",
           worst, worst_i, worst_i / 12);
    ck(fabs(worst) < 40.0, "erreur de justesse < 40 cents sur 8 octaves");

    /* Les cinq premieres octaves doivent etre JUSTES (< 5 cents), sinon la
     * table n'est pas celle de l'horloge Mockingboard. */
    worst = 0.0;
    for (i = 0; i < 60; ++i) {
        f     = 440.0 * pow(2.0, ((double)i - MB_NOTE_A4) / 12.0);
        cents = 1200.0 * log2((1020500.0 / (16.0 * mb_note_table[i])) / f);
        if (fabs(cents) > fabs(worst)) worst = cents;
    }
    printf("  octaves 0-4 : %+.1f cents\n", worst);
    ck(fabs(worst) < 5.0, "octaves 0-4 justes a moins de 5 cents");
}

/* --- base de temps ------------------------------------------------------- */
static void test_latch(void)
{
    printf("base de temps\n");
    /* 1020500/50 - 2 = 20408 ; 1020500/60 - 2 = 17006. Les deux details qui
     * comptent : l'horloge est 1 020 500 (pas 1 023 000) et T1 en free-run
     * compte N+2. */
    ck(mbt_latch(50) == 20408, "latch 50 Hz = 20408");
    ck(mbt_latch(60) == 17006, "latch 60 Hz = 17006");
    ck(mbt_latch(0)  == 20408, "hz absurde -> repli 50 Hz");
}

/* --- machine a etats des effets ------------------------------------------ */
/*
 * Les sons ne sont plus dans la bibliotheque : on definit les notres, ce qui
 * exerce du meme coup l'API telle qu'une application la voit.
 */

static const MbFxStep fx_court[] = {          /* un pas de 2 ticks */
    { 57, 10, 0, 2 }, { MB_FX_END, 0, 0, 0 }
};
static const MbFxStep fx_trois[] = {          /* trois pas */
    { 55, 13, 0, 5 }, { 52, 13, 0, 4 }, { 36, 13, 0, 10 }, { MB_FX_END, 0, 0, 0 }
};
static const MbFxStep fx_bruit[] = {
    { MB_FX_NOISE, 6, 24, 2 }, { MB_FX_END, 0, 0, 0 }
};
static const MbFxStep fx_choc[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 3, 6 }, { MB_FX_END, 0, 0, 0 }
};
static const MbFxStep fx_vide[] = {           /* effet vide : doit etre refuse */
    { MB_FX_END, 0, 0, 0 }
};

static const MbFxStep *const tous[] = { fx_court, fx_trois, fx_bruit, fx_choc };

static void test_fx(void)
{
    unsigned n;

    printf("effets\n");
    mb_slot = 4;                 /* fait croire a la lib qu'une carte est la */

    mb_fx_stop();
    ck(!mb_fx_active(), "au repos, aucun effet");

    /* Un pas de 2 ticks doit etre actif au tick 1 et fini au tick 2 --
     * exactement, pas « a peu pres ». Un tick de trop par pas et une musique
     * de trois minutes derive d'une seconde. */
    mb_fx_play(fx_court);
    ck(mb_fx_active(), "un effet arme devient actif");
    mb_fx_tick();
    ck(mb_fx_active(), "encore actif apres 1 tick");
    mb_fx_tick();
    ck(!mb_fx_active(), "fini apres 2 ticks");

    /* Chaque effet doit se TERMINER. Un pas sans MB_FX_END boucle sans fin --
     * exactement le bug qu'avait a2adv sur sa porte : un compteur non signe
     * qui passait de 1 a 255 au lieu de s'arreter. */
    for (n = 0; n < sizeof(tous) / sizeof(tous[0]); ++n) {
        unsigned t = 0;
        mb_fx_play(tous[n]);
        while (mb_fx_active() && t < 5000) { mb_fx_tick(); ++t; }
        if (t >= 5000) { printf("  ECHEC : effet %u ne finit pas\n", n); ++fails; }
        ++checks;
    }

    /* Le dernier arme gagne : pas de file d'attente. */
    mb_fx_play(fx_trois);
    mb_fx_play(fx_choc);
    ck(mb_fx_active(), "reamorcage : un effet est bien en cours");
    mb_fx_stop();
    ck(!mb_fx_active(), "mb_fx_stop coupe");

    /* Deux facons de ne rien demander, toutes deux sans effet de bord. */
    mb_fx_play(0);
    ck(!mb_fx_active(), "pointeur nul -> rien");
    mb_fx_play(fx_vide);
    ck(!mb_fx_active(), "effet vide -> rien");

    mb_slot = 0;
}

/* --- bornes du flux A2M --------------------------------------------------
 *
 * a2m_check() valide l'en-tete PAR RAPPORT A `len`, et a2m_play_t()/_r()
 * bornent chaque lecture du corps sur ce meme `len`. Un module tronque doit
 * s'arreter proprement (a2m_state() -> A2M_STOPPED) au lieu de continuer a
 * lire au-dela du tableau -- ce que gcc/ASan trahirait tot ou tard sur ces
 * tableaux C, mais qu'on verifie ici par le COMPORTEMENT (etat, position),
 * pas en esperant un crash. C'est la meme logique que sur la carte : la
 * memoire qui suit A2M_BUF n'est pas a nous, y compris cote hote.
 *
 * v_instr[] est une globale de a2m_t.c (pas static) : on la lit directement
 * pour verifier le repli sur l'instrument 0, exactement comme test_fx() lit
 * mb_slot plus haut. */
extern u8 v_instr[6];

/* En-tete de 48 octets, commun aux deux profils (cf. docs/format-a2m.md).
 * `dataoff` vaut toujours 48 dans ces modules de test : le corps suit
 * immediatement l'en-tete. */
static void header(u8 *h, char profile, u8 nay)
{
    memset(h, ' ', 48);
    h[0] = 'A'; h[1] = '2'; h[2] = 'M'; h[3] = 0x03;
    h[4] = (u8)profile;
    h[5] = 0x00;                        /* drapeaux : pas de boucle propre */
    h[6] = 0xB8; h[7] = 0x4F;           /* latch, sans importance ici */
    h[8] = 50;
    h[9] = 0; h[10] = 0;                /* frames : inconnu */
    h[11] = 48; h[12] = 0;              /* point de rebouclage */
    h[13] = 48; h[14] = 0;              /* DATAOFF */
    h[15] = nay;
}

static void test_a2m_bounds(void)
{
    u8 m[80];

    printf("bornes du flux A2M\n");
    mb_slot = 4;

    /* --- a2m_check() : rejets a l'entree, avant de jouer quoi que ce soit */
    header(m, 'T', 1);
    ck(!a2m_check(m, 47), "len < 48 -> refuse (meme en-tete par ailleurs bonne)");
    ck(a2m_check(m, 48),  "len == 48, DATAOFF == 48 -> accepte (corps vide)");
    m[13] = 200; m[14] = 0;             /* DATAOFF au-dela de tout ce qu'on a */
    ck(!a2m_check(m, 60), "DATAOFF > len -> refuse");

    /* --- profil T, module complet et VALIDE : sert de temoin -- prouve que
     * le blindage ne coupe pas un fichier legitime trop tot. */
    header(m, 'T', 1);
    m[48] = 1;                                    /* n_instr        */
    m[49]=15; m[50]=0; m[51]=17; m[52]=0;          /* instrument 0,  */
    m[53]=17; m[54]=0; m[55]=0; m[56]=0;           /* type "pluck"   */
    m[57] = 0x10; m[58] = 0;            /* EV_INSTR voie0, instrument 0 */
    m[59] = 0x00; m[60] = 60;           /* EV_NOTE_ON voie0, note 60    */
    m[61] = 0x84;                       /* EV_WAIT : 5 trames           */
    m[62] = 0xFF;                       /* EV_END                      */
    /* len = 63 : le fichier COMPLET, rien de tronque. */
    a2m_play_t(m, 63, 0);
    ck(a2m_state() == A2M_PLAYING, "module T complet : demarre");
    { u8 i; for (i = 0; i < 5; ++i) a2m_frame(); }
    ck(a2m_state() == A2M_PLAYING && a2m_pos() == 5,
       "module T complet : 5 trames d'attente, toujours en cours");
    a2m_frame();                        /* 6e appel : lit EV_END, loop=0 */
    ck(a2m_state() == A2M_STOPPED && a2m_pos() == 5,
       "module T complet : s'arrete PILE sur EV_END, pas avant, pas apres");

    /* --- meme module, tronque juste APRES la note -- avant le WAIT. Sans
     * bornage, le lecteur continuerait de lire les octets 61, 62, 63... qui
     * n'existent pas dans ce fichier. */
    a2m_play_t(m, 61, 0);
    ck(a2m_state() == A2M_PLAYING, "module T tronque : demarre quand meme");
    a2m_frame();
    ck(a2m_state() == A2M_STOPPED && a2m_pos() == 0,
       "module T tronque apres la note : s'arrete AU premier tick, sans lire au-dela");

    /* --- tronque encore plus tot : juste apres l'opcode EV_INSTR, avant son
     * operande. Garde DIFFERENTE de la precedente (celle du sommet de
     * boucle) -- elle doit porter seule si on la retire.
     *
     * Piege a faux-positif evite ici : DEUX instruments declares (0 et 1),
     * et l'operande manquant est physiquement present en memoire, EMPOISONNE
     * a 1 -- un indice VALIDE pour ce module (< n_instr), que le repli sur 0
     * de la Q13 ne rejetterait donc pas. Seule la garde de longueur peut
     * empecher cette lecture-la. Avec un seul instrument declare, ce meme
     * test passerait par accident : le repli sur 0 masquerait l'absence de
     * la garde, sans la verifier vraiment. */
    header(m, 'T', 1);
    m[48] = 2;                                     /* n_instr = 2   */
    m[49]=15; m[50]=0; m[51]=17; m[52]=0; m[53]=17; m[54]=0; m[55]=0; m[56]=0;
    m[57]= 7; m[58]=0; m[59]=17; m[60]=0; m[61]=17; m[62]=0; m[63]=0; m[64]=0;
    m[65] = 0x10;                        /* EV_INSTR voie0 -- dernier octet valide */
    m[66] = 1;                           /* operande EMPOISONNE, hors de `len`     */
    a2m_play_t(m, 66, 0);                /* len = 66 : m[66] n'existe pas pour lui */
    a2m_frame();
    ck(a2m_state() == A2M_STOPPED && v_instr[0] == 0,
       "module T tronque APRES l'opcode EV_INSTR : s'arrete SANS lire l'operande empoisonne");

    /* --- indice d'instrument hors bornes : le module ne declare qu'UN
     * instrument (0), l'evenement en demande un autre. Sans repli, t_note_on
     * indexerait in_peak[]/in_atk[]/... hors de leurs 8 cases -- a CHAQUE
     * trame, pas seulement au chargement. */
    header(m, 'T', 1);
    m[48] = 1;
    m[49]=15; m[50]=0; m[51]=17; m[52]=0; m[53]=17; m[54]=0; m[55]=0; m[56]=0;
    m[57] = 0x10; m[58] = 5;             /* EV_INSTR voie0, instrument 5 : INEXISTANT */
    m[59] = 0x00; m[60] = 60;            /* EV_NOTE_ON voie0, note 60             */
    m[61] = 0xFF;                        /* EV_END                                */
    a2m_play_t(m, 62, 0);
    a2m_frame();
    ck(v_instr[0] == 0, "indice d'instrument hors bornes -> replie sur 0");

    /* --- profil R, module complet et VALIDE : meme role de temoin. */
    header(m, 'R', 1);
    m[48] = 0x01;                        /* ctrl : CTRL_MASK1 seulement */
    m[49] = 0x01;                        /* bits0 : un seul registre    */
    m[50] = 0x05;                        /* sa valeur                  */
    m[51] = 0xFF;                        /* CTRL_END                   */
    a2m_play_r(m, 52, 0);
    ck(a2m_state() == A2M_PLAYING, "module R complet : demarre");
    a2m_frame();
    ck(a2m_state() == A2M_PLAYING && a2m_pos() == 1,
       "module R complet : une trame jouee, toujours en cours");
    a2m_frame();
    ck(a2m_state() == A2M_STOPPED && a2m_pos() == 1,
       "module R complet : s'arrete PILE sur CTRL_END");

    /* --- meme module R, tronque juste apres le mot de masque, avant la
     * valeur de registre qu'il annonce. */
    a2m_play_r(m, 50, 0);
    a2m_frame();
    ck(a2m_state() == A2M_STOPPED,
       "module R tronque avant la valeur de registre annoncee : s'arrete");

    /* --- tronque encore plus tot : juste apres ctrl, avant le mot de masque
     * lui-meme. Garde differente de la precedente. */
    a2m_play_r(m, 49, 0);
    a2m_frame();
    ck(a2m_state() == A2M_STOPPED,
       "module R tronque avant le mot de masque : s'arrete");

    mb_slot = 0;
}

int main(void)
{
    printf("== a2mock_sndlib : tests hote ==\n\n");
    test_notes();
    test_latch();
    test_fx();
    test_a2m_bounds();
    printf("\n%d verifications, %d echec(s)\n", checks, fails);
    return fails ? 1 : 0;
}
