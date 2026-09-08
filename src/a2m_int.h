/* a2m_int.h -- etat partage entre le noyau du lecteur et ses deux moteurs.
 *
 * Cet en-tete n'existe que pour une raison : permettre au lieur de NE PAS
 * embarquer le decodeur du profil R quand l'application ne joue que du T, et
 * inversement. Les deux profils vivaient dans le meme module ; en utiliser un
 * les embarquait tous les deux, soit ~2 Ko de code mort.
 *
 * Ce n'est pas un en-tete public.
 */
#ifndef A2M_INT_H
#define A2M_INT_H

#include "a2m.h"

/* Positions dans l'en-tete (48 octets). */
#define H_PROFILE   4
#define H_FLAGS     5
#define H_LATCH     6
#define H_HZ        8
#define H_LOOPFR   11
#define H_DATAOFF  13
#define H_NAY      15
#define H_TITLE    16
#define H_AUTHOR   32

#define PROFILE_R  'R'
#define PROFILE_T  'T'
#define FLAG_LOOP  0x01

/* Etat commun aux deux moteurs. */
extern const u8 *a2m_p;        /* prochaine trame / prochain evenement */
extern const u8 *a2m_data0;    /* premiere trame : point de rebouclage */
extern u8  a2m_st;             /* A2M_STOPPED | _PLAYING | _PAUSED     */
extern u8  a2m_loop;
extern u8  a2m_nay;
extern u8  a2m_atten;
extern u16 a2m_no;             /* trame courante */

/* Ce que a2m_frame() appelle. Pose par a2m_play_r() ou a2m_play_t() ; c'est
 * par ce pointeur que le noyau s'abstient de nommer les deux moteurs. */
extern void (*a2m_engine)(void);

u16 a2m_rd16(const u8 *p);

/* Amorce commune : valide, remplit l'etat partage, remet les puces du module
 * a zero. Renvoie 0 si le module est refuse. */
u8 a2m_begin(const u8 *mod, u8 loop);

/* Remet a zero les puces du MODULE -- et elles seules. Appelee au demarrage
 * et A CHAQUE REBOUCLAGE : sans ca, les notes qui sonnaient continuent
 * par-dessus la reprise, et en profil R (flux differentiel) les registres que
 * la premiere trame ne reecrit pas restent figes sur l'etat de FIN. */
void a2m_rewind_chips(void);

#endif
