/* sons.c -- VOS bruitages.
 *
 * La bibliotheque fournit le sequenceur, pas le catalogue : un jeu a son
 * identite sonore. Chaque pas vaut { note, amplitude, argument, duree } ;
 * la duree est en ticks, soit 20 ms a 50 Hz.
 *
 * Notes : 0 = do0, 48 = do4, 57 = la4, 60 = do5, 64 = mi5, 67 = sol5.
 */

#include "sons.h"

/* Deux notes montantes, breves : « ramasse ». */
const MbFxStep son_ramasse[] = {
    { 64, 12, 0, 3 },
    { 67, 12, 0, 5 },
    { MB_FX_END, 0, 0, 0 }
};

/* Un choc SEC. C'est l'enveloppe materielle qui fait le « sec » : a amplitude
 * constante on obtient un souffle plat qui s'arrete net, ce qui sonne comme
 * une coupure, pas comme un choc. `arg` regle la vitesse d'extinction. */
const MbFxStep son_choc[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 1, 3 },
    { MB_FX_END, 0, 0, 0 }
};
