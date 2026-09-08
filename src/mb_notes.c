/* mb_notes.c -- index de note -> periode AY.
 *
 * Table ENGENDREE pour l'horloge Mockingboard (1 020 500 Hz), pas recopiee
 * d'ailleurs : une table calculee pour un ZX (1,7734 MHz) ou un CPC (1 MHz)
 * sonnerait faux ici. Formule :
 *
 *     periode = horloge / (16 * frequence)
 *
 * Index 0 = do0, index 57 = la4 = 440 Hz, index 95 = si7. Huit octaves.
 *
 * ---------------------------------------------------------------------------
 * JUSTESSE -- a lire avant d'ecrire une melodie dans l'aigu.
 *
 * La periode de l'AY tient sur 12 bits, et plus la note est aigue, plus la
 * periode est petite : le pas de quantification devient enorme en proportion.
 * Erreur maximale mesuree, par octave :
 *
 *     octave 0 :  +0,2 cents        octave 4 :  -4,8 cents
 *     octave 1 :  -0,7 cents        octave 5 : +11,5 cents
 *     octave 2 :  -1,4 cents        octave 6 : +17,9 cents
 *     octave 3 :  -2,3 cents        octave 7 : -35,9 cents  (fa#7)
 *
 * L'oreille commence a percevoir un ecart vers 5 cents. Les octaves 0 a 4 sont
 * donc justes, la 5 est limite, et les octaves 6-7 sont AUDIBLEMENT fausses --
 * 36 cents, c'est plus d'un tiers de demi-ton.
 *
 * Ce n'est pas un defaut de cette table : c'est le materiel. Aucun encodage,
 * aucun reglage ne le rattrape. La consequence pratique : une melodie qui vit
 * au-dessus de do6 sonnera fausse sur Mockingboard, quel qu'en soit l'auteur.
 * C'est aussi pourquoi le convertisseur (cf. spec §7) doit CHIFFRER l'erreur
 * de justesse d'un morceau plutot que de laisser la deviner.
 * ---------------------------------------------------------------------------
 */

#include "a2mb.h"

const u16 mb_note_table[MB_NOTE_MAX] = {
    3901, 3682, 3475, 3280, 3096, 2922, 2758, 2603, 2457, 2319, 2189, 2066,   /* octave 0 */
    1950, 1841, 1738, 1640, 1548, 1461, 1379, 1302, 1229, 1160, 1095, 1033,   /* octave 1 */
     975,  920,  869,  820,  774,  731,  690,  651,  614,  580,  547,  517,   /* octave 2 */
     488,  460,  434,  410,  387,  365,  345,  325,  307,  290,  274,  258,   /* octave 3 */
     244,  230,  217,  205,  193,  183,  172,  163,  154,  145,  137,  129,   /* octave 4 */
     122,  115,  109,  103,   97,   91,   86,   81,   77,   72,   68,   65,   /* octave 5 */
      61,   58,   54,   51,   48,   46,   43,   41,   38,   36,   34,   32,   /* octave 6 */
      30,   29,   27,   26,   24,   23,   22,   20,   19,   18,   17,   16,   /* octave 7 */};

u16 __fastcall__ mb_note_period(u8 note)
{
    /* Hors table -> 0, et l'appelant doit le traiter comme un silence. Rendre
     * une periode arbitraire ferait sonner une note fausse au lieu de rien,
     * ce qui est plus difficile a diagnostiquer que le silence. */
    return (note < MB_NOTE_MAX) ? mb_note_table[note] : 0;
}
