/* sons.c -- catalogue de bruitages, en donnees pures.
 *
 * Ils ne sont PAS dans la bibliotheque : elle fournit le sequenceur, vous
 * fournissez les sons. Ce fichier est un catalogue d'IDEES autant qu'une
 * table -- chaque famille montre une technique differente.
 *
 * Un pas vaut { note, amplitude, argument, duree }. La duree est en ticks,
 * soit 20 ms a 50 Hz.
 *
 *   note   index (0 = do0), ou MB_FX_NOISE, MB_FX_KNOCK, MB_FX_END
 *   amp    0..15, ou AY_AMP_ENV pour suivre l'enveloppe materielle
 *   arg    bruit : periode 0..31 (grand = sourd)
 *          choc  : vitesse d'extinction (grand = plus long)
 *
 * Reperes de notes : do2=24 do3=36 do4=48 la4=57 do5=60 mi5=64 sol5=67
 *                    do6=72 mi6=76 sol6=79 do7=84
 */

#include "sons.h"

/* ===== INTERFACE ======================================================== */

/* Un clic. Une note aigue, deux ticks : plus court et on ne l'entend pas,
 * plus long et il traine derriere la touche. */
static const MbFxStep s_select[] = {
    { 69, 10, 0, 2 }, { MB_FX_END, 0, 0, 0 }
};

/* Validation : deux notes montantes, tres serrees. La montee dit « oui ». */
static const MbFxStep s_valide[] = {
    { 64, 11, 0, 2 }, { 71, 12, 0, 4 }, { MB_FX_END, 0, 0, 0 }
};

/* Refus : la meme figure a l'envers. Descendre dit « non » -- c'est une
 * convention si ancrée qu'elle n'a pas besoin d'etre apprise. */
static const MbFxStep s_erreur[] = {
    { 43, 12, 0, 4 }, { 36, 12, 0, 6 }, { MB_FX_END, 0, 0, 0 }
};

/* Page qui tourne : un souffle bref. Bruit clair, amplitude faible. */
static const MbFxStep s_page[] = {
    { MB_FX_NOISE, 6, 24, 2 }, { MB_FX_END, 0, 0, 0 }
};

/* ===== JEU ============================================================== */

static const MbFxStep s_ramasse[] = {
    { 64, 12, 0, 3 }, { 67, 12, 0, 5 }, { MB_FX_END, 0, 0, 0 }
};

/* Piece : une note, puis un trille. Alterner deux hauteurs a un tick chacune
 * donne ce scintillement metallique qu'aucune note tenue ne produit. */
static const MbFxStep s_piece[] = {
    { 76, 13, 0, 2 },
    { 83, 13, 0, 1 }, { 76, 13, 0, 1 }, { 83, 13, 0, 1 }, { 76, 13, 0, 1 },
    { 83, 12, 0, 6 }, { MB_FX_END, 0, 0, 0 }
};

/* Saut : un glissando. Des notes d'UN tick enchainees ne s'entendent pas
 * comme une melodie mais comme un mouvement continu. */
static const MbFxStep s_saut[] = {
    { 48, 13, 0, 1 }, { 53, 13, 0, 1 }, { 57, 13, 0, 1 },
    { 60, 13, 0, 1 }, { 64, 13, 0, 1 }, { 67, 12, 0, 3 },
    { MB_FX_END, 0, 0, 0 }
};

/* Bonus : arpege montant qui s'acheve sur un eclat tenu. */
static const MbFxStep s_bonus[] = {
    { 48, 11, 0, 2 }, { 52, 11, 0, 2 }, { 55, 11, 0, 2 }, { 60, 12, 0, 2 },
    { 64, 12, 0, 2 }, { 67, 13, 0, 2 }, { 72, 13, 0, 8 },
    { MB_FX_END, 0, 0, 0 }
};

/* ===== COMBAT =========================================================== */

/* Choc SEC. C'est l'enveloppe materielle qui fait le « sec » : a amplitude
 * constante on obtient un souffle plat qui s'arrete net -- ca sonne comme une
 * coupure, pas comme un impact. */
static const MbFxStep s_coup[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 1, 3 }, { MB_FX_END, 0, 0, 0 }
};

/* Epee : un choc CLAIR suivi d'une note aigue tres breve. Le bruit donne
 * l'impact, la note donne le metal. */
static const MbFxStep s_epee[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 0, 2 },
    { 88, 11, 0, 2 }, { 84, 9, 0, 2 },
    { MB_FX_END, 0, 0, 0 }
};

/* Laser : descente TRES rapide. Huit notes d'un tick sur deux octaves --
 * l'oreille n'entend pas huit notes, elle entend un tir. */
static const MbFxStep s_laser[] = {
    { 96, 13, 0, 1 }, { 91, 13, 0, 1 }, { 86, 12, 0, 1 }, { 81, 12, 0, 1 },
    { 76, 11, 0, 1 }, { 71, 10, 0, 1 }, { 66,  9, 0, 1 }, { 60,  8, 0, 2 },
    { MB_FX_END, 0, 0, 0 }
};

/* Explosion : un choc grave, puis du bruit qui retombe en trois paliers.
 * Une seule enveloppe materielle ne suffit pas -- c'est la SUITE de pas
 * d'amplitude decroissante qui fait la trainee. */
static const MbFxStep s_explosion[] = {
    { MB_FX_KNOCK,  AY_AMP_ENV, 12, 4 },
    { MB_FX_NOISE, 13, 28, 4 },
    { MB_FX_NOISE, 10, 30, 6 },
    { MB_FX_NOISE,  6, 31, 8 },
    { MB_FX_NOISE,  3, 31, 8 },
    { MB_FX_END, 0, 0, 0 }
};

/* ===== AMBIANCE ========================================================= */

/* Porte : un battant qui claque. Coup sourd et sec -- le grincement
 * descendant qu'on serait tente d'ecrire dure trop et sonne faux. */
static const MbFxStep s_porte[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 3, 6 }, { MB_FX_END, 0, 0, 0 }
};

/* Pas : deux bruits courts espaces. L'ECART fait le pas ; deux chocs colles
 * sonneraient comme un trebuchement. */
static const MbFxStep s_pas[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 2, 3 },
    { MB_FX_NOISE, 0, 0, 8 },                 /* silence : amplitude nulle */
    { MB_FX_KNOCK, AY_AMP_ENV, 2, 3 },
    { MB_FX_END, 0, 0, 0 }
};

/* Pluie : bruit clair, doux, tenu. Amplitude basse et periode courte. */
static const MbFxStep s_pluie[] = {
    { MB_FX_NOISE, 4, 8, 20 }, { MB_FX_NOISE, 5, 6, 20 },
    { MB_FX_NOISE, 4, 9, 20 }, { MB_FX_END, 0, 0, 0 }
};

/* Tonnerre : bruit tres grave, long, en paliers qui enflent puis retombent.
 * La duree fait tout -- un tonnerre court n'est qu'un choc. */
static const MbFxStep s_tonnerre[] = {
    { MB_FX_NOISE,  7, 31, 6 }, { MB_FX_NOISE, 11, 30, 8 },
    { MB_FX_NOISE, 14, 31, 12 }, { MB_FX_NOISE, 11, 29, 14 },
    { MB_FX_NOISE,  7, 31, 16 }, { MB_FX_NOISE,  4, 30, 18 },
    { MB_FX_END, 0, 0, 0 }
};

/* Moteur : bruit grave PULSE. L'alternance regulier/plus fort donne la
 * rotation ; un bruit constant ne serait qu'un souffle. */
static const MbFxStep s_moteur[] = {
    { MB_FX_NOISE, 11, 27, 3 }, { MB_FX_NOISE, 7, 29, 3 },
    { MB_FX_NOISE, 11, 27, 3 }, { MB_FX_NOISE, 7, 29, 3 },
    { MB_FX_NOISE, 11, 27, 3 }, { MB_FX_NOISE, 7, 29, 3 },
    { MB_FX_END, 0, 0, 0 }
};

/* Coeur : deux chocs graves rapproches, puis rien. Le SILENCE apres le
 * deuxieme est ce qui en fait un battement et non un roulement. */
static const MbFxStep s_coeur[] = {
    { MB_FX_KNOCK, AY_AMP_ENV, 6, 4 },
    { MB_FX_NOISE, 0, 0, 3 },
    { MB_FX_KNOCK, AY_AMP_ENV, 4, 4 },
    { MB_FX_END, 0, 0, 0 }
};

/* Magie : arpege montant et eclat aigu tenu. */
static const MbFxStep s_magie[] = {
    { 48, 11, 0, 2 }, { 55, 11, 0, 2 }, { 60, 11, 0, 2 },
    { 67, 12, 0, 2 }, { 84, 12, 0, 8 }, { MB_FX_END, 0, 0, 0 }
};

/* ===== TABLE ============================================================ */

const MbFxStep *const sons[SON_COUNT] = {
    s_select, s_valide, s_erreur, s_page,
    s_ramasse, s_piece, s_saut, s_bonus,
    s_coup, s_epee, s_laser, s_explosion,
    s_porte, s_pas, s_pluie, s_tonnerre,
    s_moteur, s_coeur, s_magie
};

const char *const sons_noms[SON_COUNT] = {
    "SELECT", "VALIDE", "ERREUR", "PAGE",
    "RAMASSE", "PIECE", "SAUT", "BONUS",
    "COUP", "EPEE", "LASER", "EXPLOSE",
    "PORTE", "PAS", "PLUIE", "TONNERRE",
    "MOTEUR", "COEUR", "MAGIE"
};
