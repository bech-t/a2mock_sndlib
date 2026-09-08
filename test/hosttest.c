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
#include "a2mb.h"
#include "a2mb_time.h"
#include "a2mb_fx.h"

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

int main(void)
{
    printf("== a2mock_sndlib : tests hote ==\n\n");
    test_notes();
    test_latch();
    test_fx();
    printf("\n%d verifications, %d echec(s)\n", checks, fails);
    return fails ? 1 : 0;
}
