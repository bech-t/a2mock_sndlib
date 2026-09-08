/* main.c -- disquette de validation a2mock_sndlib.
 *
 * Ce n'est PAS la demo de la spec §9 : c'est le banc de validation des jalons
 * M0 et M2. Son travail est de dire OU ca casse, pas de faire joli.
 *
 * Rappel de ce qui n'a jamais tourne au moment ou cette disquette est faite :
 * tout l'asm 6502 (mb_io.s, mb_irq.s, mb_prodos.s). Si quelque chose se passe
 * mal, c'est en tres bonne place.
 */

#include <conio.h>
#include <stdlib.h>
#include "tests.h"
#include "a2mb_time.h"
#include "a2mb_fx.h"

/* Choix du slot -- A LA MAIN, et rien d'autre.
 *
 * Aucune detection automatique, aucun balayage, meme sur demande : tant que
 * vous n'avez pas designe un slot, RIEN n'est ecrit nulle part. C'est la
 * doctrine de la bibliotheque, et il serait etrange que son propre programme
 * de validation soit le premier a y deroger.
 *
 * Le slot designe est en revanche VERIFIE avant usage (mb_init -> mb_probe) :
 * refuser de deviner n'oblige pas a ecrire a l'aveugle dans le slot annonce.
 */
static u8 pick_slot(void)
{
    u8 k;

    for (;;) {
        clrscr();
        cprintf("a2mock_sndlib -- validation M0/M2\r\n");
        cprintf("=================================\r\n\n");

        cprintf("DANS QUEL SLOT EST LA CARTE ?\r\n\n");
        cprintf("  touches 1 a 7\r\n\n");

        cprintf("aucune recherche automatique : rien\r\n");
        cprintf("n'est ecrit dans un slot que vous\r\n");
        cprintf("n'avez pas designe.\r\n\n");
        cprintf("le slot 4 est la convention historique\r\n");
        cprintf("de la Mockingboard.\r\n\n");

        cprintf("choix : ");
        k = cgetc();
        if (k < '1' || k > '7')
            continue;
        cprintf("%c\r\n", k);
        return (u8)(k - '0');
    }
}

/* Repartir d'un etat connu a CHAQUE changement d'ecran.
 *
 * Deux raisons, et aucune n'est de la precaution :
 *
 *  1. mb_silence() ne coupe que les amplitudes et le mixer. La periode de
 *     bruit, les periodes de ton et les registres d'enveloppe restent tels
 *     quels dans la puce. L'ecran suivant rouvre une voie et ressort le bruit
 *     du precedent -- ou reste muet parce que la periode vaut zero.
 *
 *  2. Le tick appelle encore le hook de l'ecran qu'on vient de quitter. Un
 *     lecteur arrete mais toujours branche continue de pousser des registres
 *     par-dessus ce que fait le nouvel ecran.
 *
 * L'ordre compte : arreter le tick AVANT de toucher a la carte, sinon une IRQ
 * tombe au milieu de la reinitialisation -- et le coeur d'ecriture n'est pas
 * reentrant (cf. a2mb.h). */
static void contexte_propre(void)
{
    mbt_stop();
    mbt_hook(0);
    mb_fx_stop();
    mb_reset();
}

int main(void)
{
    u8 slot, k, ready;

    slot  = pick_slot();
    ready = mb_init(slot);

    if (!ready) {
        cprintf("\r\nmb_init a echoue sur le slot %u.\r\n", slot);
        cprintf("l'epreuve 1 reste utilisable : elle ne\r\n");
        cprintf("pilote rien, elle ne fait que sonder.\r\n");
        cprintf("\r\n-- une touche --");
        cgetc();
    }

    for (;;) {
        clrscr();
        cprintf("a2mock_sndlib -- validation M0/M2\r\n");
        cprintf("=================================\r\n\n");
        if (ready) cprintf("carte : slot %u\r\n\n", slot);
        else       cprintf("carte : AUCUNE (epreuve 1 seule)\r\n\n");

        cprintf("  1. sonder le slot %u (en detail)\r\n", slot);
        cprintf("  2. les six voies\r\n");
        cprintf("  3. la base de temps (IRQ)\r\n");
        cprintf("  4. les bruitages\r\n");
        cprintf("  5. MUSIQUE (modules A2M)\r\n");
        cprintf("  6. TIMBRES (test d'instruments)\r\n");
        cprintf("\r\n  S. changer de slot\r\n");
        cprintf("  Q. quitter\r\n");
        cprintf("\r\nchoix : ");

        k = cgetc();
        if (k >= '1' && k <= '6')
            contexte_propre();          /* avant : la carte est a nous seule */

        switch (k) {
        case '1': t_probe_slot(slot); break;
        case '2': if (ready) t_voices(); break;
        case '3': if (ready) t_tick();   break;
        case '4': if (ready) t_fx();     break;
        case '5': if (ready) t_music(); break;
        case '6': if (ready) t_instruments(); break;
        case 's': case 'S':
            contexte_propre();
            slot  = pick_slot();
            ready = mb_init(slot);
            break;
        case 'q': case 'Q':
            /* Rendre la machine dans l'etat ou on l'a trouvee : le tick arrete
             * et le vecteur d'interruption rendu. Un handler laisse en place
             * sur un timer qui bat encore plante la machine des que le code a
             * disparu -- et sous ProDOS, le code disparait au prochain
             * programme charge. */
            mbt_stop();
            if (ready) mb_shutdown();
            clrscr();
            cprintf("a2mock_sndlib.\r\n");
            return 0;
        default: break;
        }

        if (k >= '1' && k <= '6') {
            /* Apres : l'epreuve 1 a SONDE le slot, ce qui reprogramme le timer
             * T1 de la VIA #1 -- c'est ainsi qu'on reconnait un 6522. Sans
             * cette reinitialisation, l'ecran suivant demarre sur une base de
             * temps qui n'est plus en free-run, et il ne sort rien. */
            contexte_propre();
            if (ready)
                mb_init(slot);
        }
    }
}
