/* tests.h -- les epreuves de la disquette de validation.
 * Chacune suppose qu'une carte a deja ete prise (mb_init) par main.c. */
#ifndef A2MB_TESTS_H
#define A2MB_TESTS_H

#include "a2mb.h"

void __fastcall__ t_probe_slot(u8 slot);  /* sonde CE slot, en detail */
void t_voices(void);      /* les six voies, puis une enveloppe              */
void t_tick(void);        /* la base de temps bat-elle toute seule ?        */
void t_fx(void);          /* banc d'essai des bruitages non bloquants       */
void t_music(void);       /* lecture de modules A2M depuis la disquette     */

#endif
