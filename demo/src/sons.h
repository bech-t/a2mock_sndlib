/* sons.h -- les bruitages de CETTE demo.
 *
 * Ils ne sont pas dans la bibliotheque, et c'est voulu : un jeu a son identite
 * sonore, une demo une autre. La bibliotheque fournit le sequenceur
 * (a2mb_fx.h), l'application fournit les sons. Copiez ce fichier et changez
 * les notes.
 */
#ifndef DEMO_SONS_H
#define DEMO_SONS_H

#include "a2mb_fx.h"

#define SON_COUNT 19
extern const MbFxStep *const sons[SON_COUNT];
extern const char *const sons_noms[SON_COUNT];

#endif
