/* main.c -- squelette d'un programme qui utilise a2mock_sndlib.
 *
 * Il fait le minimum utile, et rien de plus :
 *   - prend la carte dans un slot DONNE (jamais devine) ;
 *   - charge un module A2M depuis la disquette ;
 *   - le joue sous IRQ, donc la musique avance pendant que le programme
 *     travaille ;
 *   - declenche un bruitage par-dessus, sur l'autre puce -- un bruitage DEFINI
 *     PAR L'APPLICATION, dans src/sons.c : la bibliotheque fournit le
 *     sequenceur, pas le catalogue ;
 *   - rend la machine propre en sortant.
 *
 * Copiez ce dossier, changez le nom du programme dans le Makefile, et vous
 * avez un projet qui boote.
 */

#include <conio.h>
#include <stdio.h>
#include "a2mb.h"        /* la carte   */
#include "a2mb_time.h"   /* le tick    */
#include "a2mb_fx.h"     /* les effets */
#include "a2m.h"         /* le lecteur */
#include "sons.h"        /* VOS sons -- pas ceux de la bibliotheque */

/* Slot de la carte. Le slot 4 est la convention historique de la
 * Mockingboard. On ne le DEVINE pas : mb_scan() existe mais il ecrit dans
 * chaque slot essaye, ce qu'une application n'a pas a faire dans le dos de
 * son utilisateur. Faites-le choisir, ou fixez-le comme ici. */
#define SLOT 4

/* Le tick doit faire avancer la musique ET les effets. C'est tout le
 * branchement necessaire : sous IRQ, les deux tournent pendant que la boucle
 * principale fait autre chose. */
static void tick(void)
{
    a2m_frame();
    mb_fx_tick();
}

/* Octets REELLEMENT lus : c'est elle, et pas A2M_BUFSZ, qui borne la lecture
 * du module -- cf. a2m.h. Un module tronque a moins de 48 octets ne serait
 * meme pas un en-tete complet, donc jamais valide. */
static u16 tune_len;

static u8 charger(const char *nom)
{
    FILE  *f;
    size_t n;

    f = fopen(nom, "rb");
    if (!f)
        return 0;
    n = fread(A2M_BUF, 1, A2M_BUFSZ, f);   /* $0800, 14 Ko */
    fclose(f);
    tune_len = (u16)n;
    return (u8)(n >= 48 && a2m_check(A2M_BUF, tune_len));
}

int main(void)
{
    u8 k;

    clrscr();
    cprintf("squelette a2mock_sndlib\r\n\n");

    /* 1. Prendre la carte. mb_init VERIFIE le slot avant d'y ecrire et rend
     *    0 s'il se trompe -- refuser de deviner n'oblige pas a ecrire a
     *    l'aveugle dans le slot annonce. */
    if (!mb_init(SLOT)) {
        cprintf("pas de Mockingboard au slot %u.\r\n", SLOT);
        cgetc();
        return 1;
    }

    /* 2. Charger le module. A2M_BUF vit en $0800-$3FFF, sous le programme
     *    charge en $4000 : 14 Ko, soit environ sept minutes en profil T. */
    if (!charger("THEME.A2M")) {
        cprintf("THEME.A2M introuvable ou invalide.\r\n");
        cgetc();
        return 1;
    }

    /* 3. Brancher le tick a la cadence que le MODULE demande. Un morceau a
     *    60 Hz ne se joue pas comme un morceau a 50 : on ne suppose pas. */
    mbt_hook(tick);
    if (!mbt_start(a2m_latch(A2M_BUF), MBT_IRQ)) {
        /* Refus franc plutot qu'un tick qui n'arriverait jamais. Le repli
         * polling marche, mais il n'avance que quand la boucle l'appelle. */
        cprintf("IRQ refusee, repli polling.\r\n");
        mbt_start(a2m_latch(A2M_BUF), MBT_POLL);
    }

    /* a2m_play_t() et non a2m_play() : ce projet ne diffuse que ses propres
     * modules, et il les connait -- profil T. a2m_play() lirait l'octet de
     * profil et appellerait le bon moteur, mais elle NOMME les deux, donc le
     * lieur embarquerait aussi le decodeur du profil R : 2 145 octets pour
     * rien. On paie ce qu'on nomme. */
    a2m_play_t(A2M_BUF, tune_len, 1);   /* 1 = en boucle */

    cprintf("en lecture.\r\n\n");
    cprintf("  ESPACE  un bruitage par-dessus\r\n");
    cprintf("  P       pause\r\n");
    cprintf("  Q       quitter\r\n");

    /* 4. La boucle du programme. Sous IRQ, la musique avance TOUTE SEULE
     *    pendant tout ce qui suit. */
    for (;;) {
        /* En repli polling, c'est cette ligne qui fait avancer le tick.
         * Sous IRQ elle ne fait rien. Meme code dans les deux cas. */
        if (mbt_mode() == MBT_POLL)
            mbt_poll();

        if (!kbhit())
            continue;
        k = cgetc();

        if (k == 'q' || k == 'Q')
            break;
        if (k == ' ') {
            /* Le bruitage vit sur l'AY #2, la musique sur l'AY #1 : il ne la
             * coupe pas. Vrai seulement si le module n'utilise qu'UN AY --
             * un module stereo prend les deux puces. */
            if (A2M_BUF[15] == 1)
                mb_fx_play(son_ramasse);
        }
        if (k == 'p' || k == 'P')
            a2m_pause((u8)(a2m_state() != A2M_PAUSED));
    }

    /* 5. Rendre la machine dans l'etat ou on l'a trouvee. IMPERATIF : un
     *    handler laisse en place sur un timer qui bat encore plante la
     *    machine des que le code a disparu -- et sous ProDOS, il disparait au
     *    prochain programme charge. */
    a2m_stop();
    mbt_stop();
    mb_shutdown();
    clrscr();
    return 0;
}
