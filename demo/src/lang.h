/* lang.h -- francais/anglais, choisis une fois au demarrage.
 *
 * Ternaire, pas une table de messages indexee : deux langues fixees a la
 * compilation, jamais rechargees -- une table n'acheterait rien ici, et
 * eloignerait le texte anglais de son jumeau francais, qui doivent rester
 * lisibles cote a cote pour verifier que les %-specificateurs concordent.
 *
 * DISCIPLINE, a chaque paire T(fr, en) :
 *   1. memes %-specificateurs, dans le meme ordre, meme nombre ;
 *   2. si le texte remplit un champ a largeur fixe a l'ecran (cf. les mots
 *      d'etat "en lecture"/"en pause  "/"arrete    " de module_screen()),
 *      le rendu doit faire EXACTEMENT la meme largeur des deux cotes, sinon
 *      le panneau se decale.
 * `make i18ncheck` verifie mecaniquement le point 1 (pas le 2, ni la qualite
 * de traduction : une relecture humaine reste necessaire).
 */
#ifndef A2MB_LANG_H
#define A2MB_LANG_H

#include "a2mb.h"

#define LANG_FR 0
#define LANG_EN 1

extern u8 g_lang;

#define T(fr, en) (g_lang == LANG_EN ? (en) : (fr))

#endif
