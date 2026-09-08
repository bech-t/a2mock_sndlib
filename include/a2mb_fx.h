/* a2mb_fx.h -- moteur de bruitages. LES SONS SONT A VOUS.
 *
 * Cette couche fournit un sequenceur, pas un catalogue. Un jeu a ses propres
 * sons ; les figer dans la bibliotheque reviendrait a imposer une identite
 * sonore a tous ses utilisateurs, et a faire payer neuf effets a celui qui
 * n'en veut qu'un. Un exemple complet de table de sons vit dans
 * demo/src/sons.c et examples/skeleton/src/sons.c.
 *
 * ---------------------------------------------------------------------------
 * UN EFFET EST UNE SUITE DE PAS
 *
 * Chaque pas dit quoi poser sur la voie et combien de ticks le laisser sonner.
 * mb_fx_tick() passe au suivant quand le compte est epuise. Aucune boucle
 * d'attente nulle part -- c'est ce qui rend l'ensemble utilisable depuis une
 * interruption.
 *
 *   static const MbFxStep coup[] = {
 *       { MB_FX_KNOCK, AY_AMP_ENV, 6, 3 },   bruit + enveloppe percussive
 *       { MB_FX_END,   0, 0, 0 }
 *   };
 *   mb_fx_play(coup);
 *
 * mb_fx_play() ARME l'effet et rend la main tout de suite ; c'est le tick qui
 * le fait avancer. Sous IRQ, une fonction bloquante n'aurait aucun sens --
 * appelee depuis le programme elle le gelerait, appelee depuis l'interruption
 * elle gelerait la machine.
 * ---------------------------------------------------------------------------
 *
 * Les effets vivent sur l'AY #2 (voies 3-5), la musique sur l'AY #1 (voies
 * 0-2). Un bruitage ne coupe donc jamais la musique : ce sont deux puces
 * distinctes, avec deux sorties audio distinctes.
 */
#ifndef A2MB_FX_H
#define A2MB_FX_H

#include "a2mb.h"

/* Voie d'effets : premiere voie de l'AY #2. */
#define MB_CH_FX   3
#define MB_FX_AY   1

/* Un pas d'effet. */
typedef struct {
    u8 note;    /* index de note (0 = do0), ou MB_FX_NOISE / _KNOCK / _END */
    u8 amp;     /* 0..15, ou AY_AMP_ENV pour suivre l'enveloppe materielle */
    u8 arg;     /* note : inutilise
                 * bruit : periode 0..31 (grand = sourd)
                 * choc  : vitesse d'extinction, octet de poids fort */
    u8 ticks;   /* duree du pas, en ticks (20 ms a 50 Hz) */
} MbFxStep;

#define MB_FX_END    0xFF   /* fin de l'effet                              */
#define MB_FX_NOISE  0xFE   /* bruit a amplitude constante                 */
#define MB_FX_KNOCK  0xFD   /* bruit + enveloppe percussive : un choc SEC  */

/* Arme un effet. Rend la main IMMEDIATEMENT.
 * Un effet deja en cours est remplace : le dernier arme gagne, plutot que
 * d'empiler une file dont personne n'a besoin dans un jeu.
 * `fx` doit rester valide tant que l'effet joue -- en pratique, une table
 * `static const`. Un pointeur nul arrete l'effet en cours. */
void __fastcall__ mb_fx_play(const MbFxStep *fx);

void mb_fx_stop(void);
u8   mb_fx_active(void);

/* Fait avancer l'effet d'UN pas. A brancher sur le tick :
 *     mbt_hook(mb_fx_tick);
 * ou, si une musique tourne aussi, depuis le hook du lecteur, qui appellera
 * les deux. No-op si aucun effet n'est arme. */
void mb_fx_tick(void);

#endif /* A2MB_FX_H */
