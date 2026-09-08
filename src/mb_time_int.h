/* mb_time_int.h -- interface INTERNE entre mb_time.c et mb_time_irq.c.
 *
 * Elle n'existe que pour une raison : permettre au lieur de NE PAS embarquer
 * le handler d'interruption et l'appel ProDOS quand l'application ne veut que
 * la scrutation. Sans cette separation, mb_time.c referencait mbt_isr et
 * pd_alloc_irq en toutes circonstances, et un programme en polling pur payait
 * 314 octets de code mort.
 *
 * Ce n'est pas un en-tete public : n'incluez pas ceci depuis une application.
 */
#ifndef A2MB_TIME_INT_H
#define A2MB_TIME_INT_H

#include "a2mb.h"

/* Arme T1 en free-run avec ce latch, IRQ desarmee. 0 si aucune carte. */
u8 __fastcall__ mbt_arm(u16 latch);

/* Declare le mode obtenu, et comment le rendre. `release` est appelee par
 * mbt_stop() ; c'est par ce pointeur que mb_time.c s'abstient de referencer
 * DEALLOC_INTERRUPT. */
void mbt_set_mode(u8 mode, void (*release)(void));

/* Base de la VIA #1, ou 0 si aucune carte. */
volatile u8 *mbt_via1(void);

#endif
