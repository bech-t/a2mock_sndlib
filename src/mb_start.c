/* mb_start.c -- mbt_start(latch, mode), la commodite.
 *
 * Elle reference les DEUX chemins, donc l'appeler embarque le code IRQ meme si
 * on ne demande que la scrutation. C'est pour ca qu'elle vit dans son propre
 * module : une application qui ne veut que le polling appelle directement
 * mbt_start_poll() et ne paie rien.
 */

#include "a2mb_time.h"

u8 __fastcall__ mbt_start(u16 latch, u8 mode)
{
    if (mode == MBT_IRQ)
        return mbt_start_irq(latch);
    return mbt_start_poll(latch);
}
