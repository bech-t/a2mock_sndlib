/* a2m_any.c -- a2m_play(), la commodite.
 *
 * Elle lit le profil dans l'en-tete et appelle le moteur qui va bien -- donc
 * elle NOMME les deux, et le lieur les embarque tous les deux (~2 Ko).
 *
 * Une application qui ne joue qu'un seul profil appelle directement
 * a2m_play_t() ou a2m_play_r() et ne paie que celui-la. C'est le meme principe
 * que mbt_start_poll() / mbt_start_irq() : on paie ce qu'on nomme.
 */

#include "a2m_int.h"

void __fastcall__ a2m_play(const u8 *mod, u8 loop)
{
    if (mod[H_PROFILE] == PROFILE_T)
        a2m_play_t(mod, loop);
    else
        a2m_play_r(mod, loop);
}
