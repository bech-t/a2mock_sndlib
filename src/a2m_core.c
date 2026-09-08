/* a2m_core.c -- noyau du lecteur A2M : l'API publique et l'etat partage.
 *
 * Ce module ne connait NI le profil R, NI le profil T. Il appelle le moteur
 * par un pointeur, pose au demarrage. C'est ce detour qui permet a une
 * application qui ne joue que du profil T de ne pas embarquer le decodeur de
 * registres, et inversement.
 */

#include "a2m_int.h"

const u8 *a2m_p;
const u8 *a2m_data0;
const u8 *a2m_end;      /* un octet APRES le dernier valide -- cf. a2m_int.h */
u8  a2m_st;
u8  a2m_loop;
u8  a2m_nay;
u8  a2m_atten;
u16 a2m_no;
void (*a2m_engine)(void);

u16 a2m_rd16(const u8 *p) { return (u16)(p[0] | ((u16)p[1] << 8)); }

/* `len` est le nombre d'octets REELLEMENT en memoire a partir de `mod` -- pas
 * une taille annoncee par le fichier lui-meme, qu'on ne peut pas croire sur
 * parole. Un disque qui rend moins d'octets qu'attendu (secteur illisible,
 * copie interrompue) ne doit pas se voir confier un module que le lecteur
 * croira plus long qu'il ne l'est : c'est `len` qui tranche, pas l'en-tete. */
u8 __fastcall__ a2m_check(const u8 *mod, u16 len)
{
    if (len < 48)
        return 0;
    if (mod[0] != 'A' || mod[1] != '2' || mod[2] != 'M' || mod[3] != 0x03)
        return 0;
    if (mod[H_PROFILE] != PROFILE_R && mod[H_PROFILE] != PROFILE_T)
        return 0;
    /* Le corps doit commencer DANS ce qui a ete charge. Ca n'empeche pas une
     * troncature plus loin dans le corps -- c'est aux deux moteurs de s'en
     * proteger, trame par trame, car eux seuls connaissent la structure du
     * flux (cf. a2m_end dans a2m_r.c / a2m_t.c). */
    return (u8)(a2m_rd16(mod + H_DATAOFF) <= len);
}

const char *__fastcall__ a2m_title(const u8 *mod)  { return (const char *)(mod + H_TITLE); }
const char *__fastcall__ a2m_author(const u8 *mod) { return (const char *)(mod + H_AUTHOR); }
u8  __fastcall__ a2m_hz(const u8 *mod)             { return mod[H_HZ]; }
u16 __fastcall__ a2m_frames(const u8 *mod)         { return a2m_rd16(mod + 9); }
u8  __fastcall__ a2m_profile(const u8 *mod)        { return mod[H_PROFILE]; }
u16 __fastcall__ a2m_latch(const u8 *mod)          { return a2m_rd16(mod + H_LATCH); }

u8  a2m_state(void) { return a2m_st; }
u16 a2m_pos(void)   { return a2m_no; }

void __fastcall__ a2m_volume(u8 a) { a2m_atten = (u8)(a > 15 ? 15 : a); }

void a2m_rewind_chips(void)
{
    mb_reset_ay(0);
    if (a2m_nay == 2)
        mb_reset_ay(1);
}

/* Amorce commune aux deux moteurs. */
u8 a2m_begin(const u8 *mod, u16 len, u8 loop)
{
    if (!mb_slot || !a2m_check(mod, len)) {
        a2m_st = A2M_STOPPED;
        return 0;
    }
    a2m_end   = mod + len;
    a2m_data0 = mod + a2m_rd16(mod + H_DATAOFF);
    a2m_p     = a2m_data0;
    a2m_no    = 0;
    a2m_loop  = (u8)(loop || (mod[H_FLAGS] & FLAG_LOOP));
    a2m_nay   = mod[H_NAY] == 2 ? 2 : 1;

    /* Les puces du MODULE partent d'un etat connu -- et elles seules. Dans un
     * jeu, les bruitages vivent sur l'AY #2 pendant que la musique tient
     * l'AY #1 : lancer un morceau ne doit pas couper un effet en cours. */
    a2m_rewind_chips();
    return 1;
}

void a2m_stop(void)
{
    a2m_st = A2M_STOPPED;
    if (mb_slot)
        a2m_rewind_chips();      /* on ne rend que ce qu'on avait pris */
}

void __fastcall__ a2m_pause(u8 on)
{
    u8 ay;

    if (a2m_st == A2M_STOPPED)
        return;
    if (!on) {
        a2m_st = A2M_PLAYING;
        return;
    }
    a2m_st = A2M_PAUSED;
    if (!mb_slot)
        return;
    /* On coupe les amplitudes SANS toucher au reste : a la reprise, periodes
     * et mixer sont encore ceux de la trame en cours, donc la note reprend ou
     * elle en etait. Les DEUX puces si le module les prend, sinon la moitie
     * de l'orchestre continuerait de jouer pendant la pause. */
    for (ay = 0; ay < a2m_nay; ++ay) {
        MB_REG(ay, AY_AMP_A) = 0;
        MB_REG(ay, AY_AMP_B) = 0;
        MB_REG(ay, AY_AMP_C) = 0;
        mb_push(ay, (1 << AY_AMP_A) | (1 << AY_AMP_B) | (1 << AY_AMP_C));
    }
}

/* Aiguillage par POINTEUR, et non par test du profil : un `if` sur l'octet 4
 * nommerait les deux moteurs, et le lieur les embarquerait tous les deux. */
void a2m_frame(void)
{
    if (a2m_st == A2M_PLAYING && a2m_engine)
        a2m_engine();
}
